// Copyright 2017-2019 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once
#include "StdAfx.h"
#include "FlightController.h"
#include "ShipThrusterComponent.h"

#include <CryRenderer/IRenderAuxGeom.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CryCore/StaticInstanceList.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CryEntitySystem/IEntitySystem.h>

#include <CryEntitySystem/IEntityComponent.h>
#include <CryPhysics/physinterface.h>
#include <CryNetwork/ISerialize.h>
#include <CryNetwork/Rmi.h>

// Forward declaration
#include <DefaultComponents/Input/InputComponent.h>
#include <Components/VehicleComponent.h>
#include <Components/Player.h>


// Registers the component to be used in the engine
namespace
{
	static void RegisterFlightController(Schematyc::IEnvRegistrar& registrar)
	{
		Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
		{
			Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CFlightController));
		}
	}
	CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterFlightController)
}


void CFlightController::Initialize()
{
	// Initialize stuff
	m_pShipThrusterComponent = m_pEntity->GetOrCreateComponent<CShipThrusterComponent>();
	m_pVehicleComponent = m_pEntity->GetOrCreateComponent<CVehicleComponent>();

	GetEntity()->GetNetEntity()->EnableDelegatableAspect(eEA_GameClientA, false);
	GetEntity()->GetNetEntity()->EnableDelegatableAspect(eEA_Physics, false);

	SRmi<RMI_WRAP(&CFlightController::RequestImpulseOnServer)>::Register(this, eRAT_Urgent, false, eNRT_ReliableOrdered);
	SRmi<RMI_WRAP(&CFlightController::UpdateMovement)>::Register(this, eRAT_Urgent, false, eNRT_ReliableOrdered);

	GetEntity()->EnablePhysics(true);
	GetEntity()->PhysicsNetSerializeEnable(true);
	GetEntity()->GetNetEntity()->BindToNetwork();
}

Cry::Entity::EventFlags CFlightController::GetEventMask() const
{
	//Listening to the update event
	return EEntityEvent::Update | EEntityEvent::GameplayStarted;
}

void CFlightController::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
	case EEntityEvent::GameplayStarted:
	{
		ResetJerkParams();
		InitializeJerkParams();
		physEntity = m_pEntity->GetPhysicalEntity();
		InitializeMotionParamsVectors();
	}
	break;
	case EEntityEvent::Update:
	{
		m_frameTime = event.fParam[0];
		if (m_pVehicleComponent->GetIsPiloting())
		{
			ResetImpulseCounter();
			FlightModifierHandler(GetFlightModifierState(), m_frameTime);
		}
	}
	break;
	case Cry::Entity::EEvent::Reset:
	{

	}
	break;
	}
}

///////////////////////////////////////////////////////////////////////////
// PARAMETERS AND INPUT INITIALIZATION
///////////////////////////////////////////////////////////////////////////
void CFlightController::InitializeJerkParams()
{
	m_linearAccelData.jerk = m_linearJerkRate;
	m_linearAccelData.jerkDecelRate = m_linearJerkDecelRate;
	m_rollAccelData.jerk = m_RollJerkRate;
	m_rollAccelData.jerkDecelRate = m_RollJerkDecelRate;
	m_pitchYawAccelData.jerk = m_PitchYawJerkRate;
	m_pitchYawAccelData.jerkDecelRate = m_PitchYawJerkDecelRate;
}

void CFlightController::ResetJerkParams()
{
	m_linearAccelData.currentJerkAccel = Vec3(ZERO);
	m_rollAccelData.currentJerkAccel = Vec3(ZERO);
	m_pitchYawAccelData.currentJerkAccel = Vec3(ZERO);
	m_linearAccelData.targetJerkAccel = Vec3(ZERO);
	m_rollAccelData.targetJerkAccel = Vec3(ZERO);
	m_pitchYawAccelData.targetJerkAccel = Vec3(ZERO);
}

void CFlightController::InitializeMotionParamsVectors()
{
	float radYawAccel = DegreesToRadian(m_yawAccel);
	float radPitchAccel = DegreesToRadian(m_pitchAccel);
	float radRollAccel = DegreesToRadian(m_rollAccel);

	// Initializing the maps of the motion profile
	m_linearParamsMap[AxisType::Linear] = {
		{"accel_forward", m_fwdAccel, m_maxFwdVel,  Vec3(0.f, 1.f, 0.f)},
		{"accel_backward", m_bwdAccel, m_maxBwdVel, Vec3(0.f, -1.f, 0.f)},
		{"accel_left", m_leftRightAccel, m_maxLatVel, Vec3(-1.f, 0.f, 0.f)},
		{"accel_right", m_leftRightAccel, m_maxLatVel, Vec3(1.f, 0.f, 0.f)},
		{"accel_up", m_upDownAccel, m_maxUpDownVel, Vec3(0.f, 0.f, 1.f)},
		{"accel_down", m_upDownAccel, m_maxUpDownVel, Vec3(0.f, 0.f, -1.f)}
	};
	m_rollParamsMap[AxisType::Roll] = {
		{"roll_left", radRollAccel, m_maxRoll , Vec3(0.f, -1.f, 0.f)},
		{"roll_right", radRollAccel, m_maxRoll, Vec3(0.f, 1.f, 0.f)}
	};
	m_pitchYawParamsMap[AxisType::PitchYaw] = {
		{"yaw", radYawAccel, m_maxYaw, Vec3(0.f, 0.f, -1.f)},
		{"pitch", radPitchAccel, m_maxPitch, Vec3(-1.f, 0.f, 0.f)}
	};
}

float CFlightController::DegreesToRadian(float degrees)
{
	return degrees * (gf_PI / 180.f);
}

FlightModifierBitFlag CFlightController::GetFlightModifierState()
{
	return m_pVehicleComponent->m_pPlayerComponent->GetComponent<CPlayerComponent>()->GetFlightModifierState();
}

float CFlightController::AxisGetter(const string& axisName)
{
	if (m_pVehicleComponent->m_pPlayerComponent)
		return m_pVehicleComponent->m_pPlayerComponent->GetComponent<CPlayerComponent>()->GetAxisValue(axisName);
	else
		return 0;
}

Vec3 CFlightController::WorldToLocal(const Vec3& localDirection)
{
	Quat worldRotation = m_pEntity->GetWorldRotation();
	Vec3 worldDirection = worldRotation * localDirection;

	return worldDirection;
}

float CFlightController::ClampInput(float inputValue, bool isMouse) const
{
	// Scale the input value by the sensitivity factor
	if (isMouse)
		inputValue *= m_mouseSenseFactor;

	return CLAMP(inputValue, m_MIN_INPUT_VALUE, m_MAX_INPUT_VALUE);
}

///////////////////////////////////////////////////////////////////////////
// FLIGHT CALCULATIONS
///////////////////////////////////////////////////////////////////////////
ScaledMotion CFlightController::ScaleInput(const VectorMap<AxisType, DynArray<AxisMotionParams>>& axisParamsList, JerkAccelerationData& accelData)
{
	// Initializing vectors for acceleration direction and desired acceleration
	Vec3 accelDirection(ZERO); // Vector to accumulate the direction of applied accelerations
	Vec3 requestedAccel(ZERO); // Vector to accumulate desired acceleration magnitudes
	Vec3 localDirection(ZERO); // Calculate local thrust direction based on input value
	Vec3 scaledInputDirection(ZERO); // Scaled vector, our final local direction + magnitude 
	Vec3 scaledVelocityDirection(ZERO);
	float normalizedInputValue(ZERO); // Normalize input values
	float maxSpeedMagnitude = 0.f; // Maximum speed magnitude
	
	for (const auto& axisMotionParamsPair : axisParamsList)	// Iterating over the list of axis and their input values
	{
		const DynArray<AxisMotionParams>& axisMotionParamsArray = axisMotionParamsPair.second;

		for (const auto& motionParams : axisMotionParamsArray)	// Iterate over the DynArray<AxisAccelParams> for the current AxisType
		{
			normalizedInputValue = ClampInput(AxisGetter(motionParams.axisName)); // Retrieve input value for the current axis and normalize to a range of -1 to 1
			localDirection = motionParams.localDirection;
			localDirection = WorldToLocal(localDirection); // Convert to local space
			accelDirection += localDirection * normalizedInputValue; // Accumulate axis direction with magnitude in local space, scaling by the normalized input
			requestedAccel += localDirection * motionParams.AccelAmount * normalizedInputValue; // Accumulate desired acceleration based on thrust amount and input value, combining for multiple axes
			scaledVelocityDirection += localDirection * motionParams.velocityLimit * normalizedInputValue; // Accumulate desired velocity based on thrust direction and input value
		
			// Calculate the speed for the current axis and update the maximum speed magnitude
			float axisSpeed = motionParams.velocityLimit * std::fabs(normalizedInputValue); // Absolute value since we are scaling velocity, not acceleration
			maxSpeedMagnitude = std::max(maxSpeedMagnitude, axisSpeed);
		}
	}

	if (accelDirection.GetLength() > 1.f) // Normalize accelDirection to mitigate excessive accelerations when combining inputs
	{
		accelDirection.Normalize();
	}
	scaledInputDirection = accelDirection * requestedAccel.GetLength(); // Scale the direction vector by the magnitude of requestedAccel
	UpdateAccelerationState(accelData, scaledInputDirection); // Updates our current *requested* motion state to compute jerk accordingly (based on input, not actual ship motion!)

	// Normalize the velocity vector if necessary
	if (scaledVelocityDirection.GetLength() > maxSpeedMagnitude)
	{
		scaledVelocityDirection.Normalize();
		scaledVelocityDirection *= maxSpeedMagnitude;
	}

	// Scale the normalized velocity vector by the maximum speed magnitude
	scaledVelocityDirection *= maxSpeedMagnitude;

	return ScaledMotion(scaledInputDirection, scaledVelocityDirection);   // Return the scaled acceleration direction vector in m/s
}

void CFlightController::UpdateAccelerationState(JerkAccelerationData& accelData, const Vec3& targetAccel)
{
	accelData.targetJerkAccel = targetAccel;
	if (targetAccel.IsZero())
	{
		accelData.state = EAccelState::Decelerating;
	}
	else
	{
		accelData.state = EAccelState::Accelerating;
	}
}

Vec3 CFlightController::UpdateAccelerationWithJerk(JerkAccelerationData& accelData, float frameTime)
{
	// Calculate the difference between current and target acceleration
	Vec3 deltaAccel = accelData.targetJerkAccel - accelData.currentJerkAccel;
	Vec3 jerk;

	switch (accelData.state)
	{
		case EAccelState::Idle:
		{
			jerk = Vec3(ZERO);
			break;
		}
		case EAccelState::Accelerating:
		{
			float scale = powf(fabs(deltaAccel.GetLength()), 0.15f); // Root jerk: Calculates the scaling factor based on the square root of the magnitude of deltaAccel
			jerk = deltaAccel * scale * accelData.jerk * frameTime; // Calculate jerk: normalized deltaAccel scaled by scale, jerkRate, and deltaTime
			break;
		}
		case EAccelState::Decelerating:
		{
			jerk = deltaAccel * accelData.jerkDecelRate * frameTime;
			break;
		}
	}
	Vec3 newAccel = accelData.currentJerkAccel + jerk; // Calculate new acceleration by adding jerk to current acceleration

	return newAccel; // Return the updated acceleration
}

Vec3 CFlightController::AccelToImpulse(Vec3 linearAccel, Vec3 rollAccel, Vec3 pitchYawAccel, float frameTime, bool mathOnly)
{
	if (physEntity)
	{

		// Retrieving the dynamics of our entity
		pe_status_dynamics dynamics;
		if (physEntity->GetStatus(&dynamics))
		{
			Vec3 linearImpulse = Vec3(ZERO);
			Vec3 angImpulse = Vec3(ZERO);
			linearImpulse = linearAccel * dynamics.mass * frameTime; // Calculates our final impulse independent from the entity's mass
			angImpulse = (rollAccel + pitchYawAccel) * dynamics.mass * frameTime;
			if (!mathOnly)
				ApplyImpulse(linearImpulse, angImpulse);
			return linearImpulse + angImpulse;
		}
	}
	return Vec3(ZERO);
}

void CFlightController::ApplyImpulse(Vec3 linearImpulse, Vec3 angImpulse, bool countTotal)
{

	IPhysicalEntity* pPhysicalEntity = GetEntity()->GetPhysics();
	if (pPhysicalEntity)
	{

		// Apply linear impulse
		pe_action_impulse actionImpulse;
		actionImpulse.impulse = linearImpulse;
		pPhysicalEntity->Action(&actionImpulse);

		// Apply angular impulse
		actionImpulse.angImpulse = angImpulse;
		pPhysicalEntity->Action(&actionImpulse);

		// Update our impulse tracking variables to send over to the server
		m_linearImpulse = linearImpulse;
		m_angularImpulse = actionImpulse.angImpulse;

		if (countTotal)
		{
			Vec3 impulse = (actionImpulse.impulse + actionImpulse.angImpulse) / m_frameTime; // Revert the frametime scaling to have the proper values
			m_totalImpulse += impulse.GetLength();
		}
	}
}

float CFlightController::GetImpulse() const
{
	return m_totalImpulse;
}

void CFlightController::ResetImpulseCounter()
{
	m_totalImpulse = 0.f;
}

Vec3 CFlightController::GetVelocity()
{
	pe_status_dynamics dynamics;
	if (!physEntity || !physEntity->GetStatus(&dynamics))
	{
		CryLog("Error: physEntity is null or failed to get dynamics status");
		return Vec3(ZERO);
	}
	Vec3 velocity = dynamics.v; // In world space 
	Matrix34 worldToLocalMatrix = m_pEntity->GetWorldTM().GetInverted();

	return worldToLocalMatrix.TransformVector(velocity);
}

float CFlightController::GetAcceleration(float frameTime)
{
	float acceleration = 0.f;
	m_shipVelocity.currentVelocity = GetVelocity().GetLength();
	float deltaV = m_shipVelocity.currentVelocity - m_shipVelocity.previousVelocity;
	m_shipVelocity.previousVelocity = m_shipVelocity.currentVelocity;

	if (fabs(frameTime) > FLT_EPSILON)
	{
		acceleration = deltaV / frameTime;
		if (acceleration < 0)
		{
			acceleration = -acceleration;
		}
	}

	return acceleration;
}

void CFlightController::DrawDirectionIndicator(float frameTime)
{
	if (m_pEntity)
	{
		pe_status_dynamics dynamics;

		if (m_pEntity->GetPhysicalEntity()->GetStatus(&dynamics))
		{
			// Get the current velocity vector
			Vec3 velocity = dynamics.v;

			// Transform the velocity vector to view
			// Other than direction, we need a 3D position to project onto the 2D screen.
			const CCamera& camera = gEnv->pSystem->GetViewCamera();
			Vec3 cameraPos = camera.GetPosition();
			Vec3 relativeVelocity = cameraPos + velocity.GetNormalized(); // Scale it for visibility

			// Project the 3D position (relative to the camera) to 2D screen coordinates
			Vec3 screenPos;
			gEnv->pRenderer->ProjectToScreen(relativeVelocity.x, relativeVelocity.y, relativeVelocity.z, &screenPos.x, &screenPos.y, &screenPos.z);

			// The screenPos now contains the normalized screen coordinates (0 to 100)
			// Convert these to actual pixel coordinates
			const int screenWidth = gEnv->pRenderer->GetWidth();
			const int screenHeight = gEnv->pRenderer->GetHeight();
			float screenX = screenPos.x / 100.0f * screenWidth;
			float screenY = screenPos.y / 100.0f * screenHeight;

			// Draw the 2D label
			if (screenPos.z < 1.f)
				gEnv->pAuxGeomRenderer->Draw2dLabel(screenX, screenY, 3.0f, m_debugColor, true, "+");
			else
				gEnv->pAuxGeomRenderer->Draw2dLabel(screenX, screenY, 3.0f, m_debugColor, true, "x");
		}
	}
}

void CFlightController::DrawOnScreenDebugText(float frameTime)
{
	// Debug
	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 60, 2, m_debugColor, false, "Velocity: %.2f", GetVelocity().GetLength());
	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 90, 2, m_debugColor, false, "acceleration: %.2f", GetAcceleration(frameTime));
	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 120, 2, m_debugColor, false, "total impulse: %.3f", GetImpulse());

	DrawDirectionIndicator(frameTime);
}


///////////////////////////////////////////////////////////////////////////
// FLIGHT MODES 
///////////////////////////////////////////////////////////////////////////

void CFlightController::DirectInput(float frameTime)
{
	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 30, 2, m_debugColor, false, "Newtonian");

	m_linearAccelData.targetJerkAccel = ScaleInput(m_linearParamsMap, m_linearAccelData).GetAcceleration(); // Scale and set the target acceleration for linear movement
	m_linearAccelData.currentJerkAccel = UpdateAccelerationWithJerk(m_linearAccelData, frameTime); 	// Infuse the acceleration value with the current jerk coeficient

	m_rollAccelData.targetJerkAccel = ScaleInput(m_rollParamsMap, m_rollAccelData).GetAcceleration();
	m_rollAccelData.currentJerkAccel = UpdateAccelerationWithJerk(m_rollAccelData, frameTime);

	m_pitchYawAccelData.targetJerkAccel = ScaleInput(m_pitchYawParamsMap, m_pitchYawAccelData).GetAcceleration();
	m_pitchYawAccelData.currentJerkAccel = UpdateAccelerationWithJerk(m_pitchYawAccelData, frameTime);

	
	// Send movement data to the server if we are connected, apply locally if not
	if (!gEnv->bServer)
	{
		SRmi<RMI_WRAP(&CFlightController::RequestImpulseOnServer)>::InvokeOnServer(this, SerializeImpulseData{
		Vec3(ZERO),
		Quat(ZERO),
		m_linearAccelData.currentJerkAccel,
		m_rollAccelData.currentJerkAccel,
		m_pitchYawAccelData.currentJerkAccel });
	}
	else 
		AccelToImpulse(m_linearAccelData.currentJerkAccel, m_rollAccelData.currentJerkAccel, m_pitchYawAccelData.currentJerkAccel, frameTime, false);
}

void CFlightController::CoupledFM(float frameTime)
{
	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 30, 2, m_debugColor, false, "Coupled");


	m_linearAccelData.targetJerkAccel = ScaleInput(m_linearParamsMap, m_linearAccelData).GetVelocity(); // Scale and set the target velocity for linear movement
	m_linearAccelData.currentJerkAccel = UpdateAccelerationWithJerk(m_linearAccelData, frameTime); 	// Infuse the acceleration value with the current jerk coeficient

	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 250, 2, m_debugColor, false, "m_linearAccelData.targetJerkAccel: x=%f, y=%f, z=%f",
		m_linearAccelData.targetJerkAccel.x,
		m_linearAccelData.targetJerkAccel.y,
		m_linearAccelData.targetJerkAccel.z);

}

///////////////////////////////////////////////////////////////////////////
// FLIGHT MODIFIERS
///////////////////////////////////////////////////////////////////////////

void CFlightController::GravityAssist(float frameTime)
{
	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 180, 2, m_debugColor, false, "Gravity assist: ON");

	if (m_pEntity)
	{
		pe_status_dynamics dynamics;

		if (m_pEntity->GetPhysicalEntity()->GetStatus(&dynamics))
		{
			// Get gravity vector
			Vec3 gravity = gEnv->pPhysicalWorld->GetPhysVars()->gravity;
			Vec3 antiGravityForce = -gravity * dynamics.mass;
			float totalAlignment = 0.0f;
			Vec3 totalScaledAntiGravityForce = Vec3(ZERO);
			pe_action_impulse impulseAction;

			// First pass: Calculate total alignment
			for (const auto& axisAccelParamsPair : m_linearParamsMap) // Iterating over each axis within the linear set.
			{
				const DynArray<AxisMotionParams>& axisParamsArray = axisAccelParamsPair.second;
				
				for (const auto& accelParams : axisParamsArray)	// Iterate over the DynArray<AxisAccelParams> for the current AxisType
				{
					Vec3 localDirection = WorldToLocal(accelParams.localDirection);

					localDirection.Normalize(); // Normalize to have a range of -1 to 1, which indicates their alignment (1 = perfect / -1 = anti) 
					
					float alignment = localDirection.Dot(gravity.GetNormalized()); // Get the alignment between localDirection and gravity, using this to scale the thrust amount
					if (alignment > ZERO)
					{
						totalAlignment += alignment;
					}
				}
			}

			// Second pass: Apply impulses proportionally
			for (const auto& axisAccelParamsPair : m_linearParamsMap)
			{
				const DynArray<AxisMotionParams>& axisParamsArray = axisAccelParamsPair.second;

				for (const auto& accelParams : axisParamsArray)
				{
					Vec3 localDirection = WorldToLocal(accelParams.localDirection);
					localDirection.Normalize();

					float alignment = localDirection.Dot(gravity.GetNormalized());
					if (alignment > ZERO)
					{
						float proportionalAlignment = alignment / totalAlignment;
						Vec3 scaledAntiGravityForce = antiGravityForce * proportionalAlignment;
						totalScaledAntiGravityForce += scaledAntiGravityForce;
						impulseAction.impulse = scaledAntiGravityForce * frameTime;
						m_pEntity->GetPhysicalEntity()->Action(&impulseAction);
					}
				}
			}
			m_totalImpulse += totalScaledAntiGravityForce.GetLength();
		}
	}
}

void CFlightController::ComstabAssist(float frameTime)
{
	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 210, 2, m_debugColor, false, "Comstab: ON");

}

void CFlightController::FlightModifierHandler(FlightModifierBitFlag bitFlag, float frameTime)
{
	if (bitFlag.HasFlag(EFlightModifierFlag::Coupled))
	{
		CoupledFM(frameTime);
	}
	else
	{
		DirectInput(frameTime);
	}
	if (bitFlag.HasFlag(EFlightModifierFlag::Boost))
	{
		gEnv->pAuxGeomRenderer->Draw2dLabel(50, 150, 2, m_debugColor, false, "Boost: ON");
	}
	else 
		gEnv->pAuxGeomRenderer->Draw2dLabel(50, 150, 2, m_debugColor, false, "Boost: OFF");
	if (bitFlag.HasFlag(EFlightModifierFlag::Gravity))
	{
		GravityAssist(frameTime);
	}
	else
		gEnv->pAuxGeomRenderer->Draw2dLabel(50, 180, 2, m_debugColor, false, "Gravity assist: OFF");
	if (bitFlag.HasFlag(EFlightModifierFlag::Comstab))
	{
		ComstabAssist(frameTime);
	}
	else
		gEnv->pAuxGeomRenderer->Draw2dLabel(50, 210, 2, m_debugColor, false, "Comstab: OFF");


	// Debug stuff
	DrawOnScreenDebugText(frameTime);
}

///////////////////////////////////////////////////////////////////////////
// NETWORKING
///////////////////////////////////////////////////////////////////////////
bool CFlightController::RequestImpulseOnServer(SerializeImpulseData&& data, INetChannel*)
{
	IPhysicalEntity* pPhysicalEntity = GetEntity()->GetPhysics();

	if (pPhysicalEntity)
	{
		AccelToImpulse(data.linearImpulse,data.rollImpulse, data.pitchYawImpulse, m_frameTime, false);
		SRmi<RMI_WRAP(&CFlightController::UpdateMovement)>::InvokeOnAllClients(this, std::move(data));

		// Update position and orientation serialization variables
		m_shipPosition = GetEntity()->GetWorldPos();
		Matrix34 shipWorldTM = GetEntity()->GetWorldTM();
		m_shipOrientation = Quat(shipWorldTM);
		m_shipPosition = shipWorldTM.GetTranslation();
	}
	return true;
}

bool CFlightController::UpdateMovement(SerializeImpulseData&& data, INetChannel*)
{
	IPhysicalEntity* pPhysicalEntity = GetEntity()->GetPhysics();
	if (pPhysicalEntity)
	{
		NetMarkAspectsDirty(kVehicleAspect);
	}
	return true;
}

bool CFlightController::NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags)
{
	if (aspect & kVehicleAspect)
	{
		ser.BeginGroup("vehicleMovement");

		ser.Value("m_shipPosition", m_shipPosition, 'wrld');
		ser.Value("m_shipOrientation", m_shipOrientation, 'ori3');
		ser.Value("m_linearImpulse", m_linearImpulse);
		ser.Value("m_angularImpulse", m_angularImpulse);
		ser.EndGroup();

		return true;
	}
	return false;
}