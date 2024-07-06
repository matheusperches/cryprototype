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
#include <CryPhysics/physinterface.h>
#include <CryNetwork/Rmi.h>
#include <CryNetwork/ISerialize.h>

// Forward declaration
#include <DefaultComponents/Input/InputComponent.h>
#include <Components/VehicleComponent.h>
#include <Components/Player.h>


// Registers the component to be used in the engine
static void RegisterFlightController(Schematyc::IEnvRegistrar& registrar)
{
	Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
	{
		Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CFlightController));
		{

		}
	}
}

CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterFlightController)

void CFlightController::Initialize()
{
	// Initialize stuff
	m_pShipThrusterComponent = m_pEntity->GetOrCreateComponent<CShipThrusterComponent>();
	m_pVehicleComponent = m_pEntity->GetOrCreateComponent<CVehicleComponent>();

	GetEntity()->GetNetEntity()->EnableDelegatableAspect(eEA_GameClientA, false);
	GetEntity()->GetNetEntity()->EnableDelegatableAspect(eEA_Physics, false);

	SRmi<RMI_WRAP(&CFlightController::ServerRequestImpulse)>::Register(this, eRAT_Urgent, false, eNRT_ReliableOrdered);
	SRmi<RMI_WRAP(&CFlightController::ClientRequestImpulse)>::Register(this, eRAT_Urgent, false, eNRT_ReliableOrdered);

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

	}
	break;
	case EEntityEvent::Update:
	{
		m_frameTime = event.fParam[0];
		if (m_pVehicleComponent->GetIsPiloting())
		{
			ResetImpulseCounter();
			FlightModifierHandler(GetFlightModifierState());
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

void CFlightController::InitializeAccelParamsVectors()
{
	float radYawAccel = DegreesToRadian(m_yawAccel);
	float radPitchAccel = DegreesToRadian(m_pitchAccel);
	float radRollAccel = DegreesToRadian(m_rollAccel);

	// Initializing the maps with the names of our axis and their respective accelerations

	m_linearAxisParamsMap[AxisType::Linear] = {
		{"accel_forward", m_fwdAccel,  Vec3(0.f, 1.f, 0.f)},
		{"accel_backward", m_bwdAccel, Vec3(0.f, -1.f, 0.f)},
		{"accel_left", m_leftRightAccel, Vec3(-1.f, 0.f, 0.f)},
		{"accel_right", m_leftRightAccel, Vec3(1.f, 0.f, 0.f)},
		{"accel_up", m_upDownAccel, Vec3(0.f, 0.f, 1.f)},
		{"accel_down", m_upDownAccel, Vec3(0.f, 0.f, -1.f)}
	};
	m_rollAxisParamsMap[AxisType::Roll] = {
		{"roll_left", radRollAccel, Vec3(0.f, -1.f, 0.f)},
		{"roll_right", radRollAccel, Vec3(0.f, 1.f, 0.f)}
	};
	m_pitchYawAxisParamsMap[AxisType::PitchYaw] = {
		{"yaw", radYawAccel, Vec3(0.f, 0.f, -1.f)},
		{"pitch", radPitchAccel, Vec3(-1.f, 0.f, 0.f)}
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
Vec3 CFlightController::ScaleAccel(const VectorMap<AxisType, DynArray<AxisAccelParams>>& axisAccelParamsList)
{
	// Initializing vectors for acceleration direction and desired acceleration
	Vec3 m_accelDirection(ZERO);	 // Vector to accumulate the direction of applied accelerations
	Vec3 m_requestedAccel(ZERO);  // Vector to accumulate desired acceleration magnitudes
	Vec3 m_localDirection(ZERO);  // Calculate local thrust direction based on input value
	Vec3 scaledAccelDirection(ZERO); // Scaled vector, our final local direction + magnitude 
	float normalizeInputValue(ZERO); // Normalize input values
	
	for (const auto& axisAccelParamsPair : axisAccelParamsList)	// Iterating over the list of axis and their input values
	{
		AxisType axisType = axisAccelParamsPair.first;
		const DynArray<AxisAccelParams>& axisParamsArray = axisAccelParamsPair.second;

		for (const auto& accelParams : axisParamsArray)	// Iterate over the DynArray<AxisAccelParams> for the current AxisType
		{
			if (axisType == AxisType::Linear)
			{
				normalizeInputValue = ClampInput(AxisGetter(accelParams.axisName)); // Retrieve input value for the current axis and normalize to a range of -1 to 1
				m_localDirection = accelParams.localDirection;
			}
			else if (axisType == AxisType::Roll)
			{
				normalizeInputValue = ClampInput(AxisGetter(accelParams.axisName));
				m_localDirection = accelParams.localDirection;
			}
			else if (axisType == AxisType::PitchYaw)
			{
				normalizeInputValue = ClampInput(AxisGetter(accelParams.axisName), true);
				m_localDirection = accelParams.localDirection;
			}
			m_localDirection = WorldToLocal(m_localDirection); // Convert to local space
			m_accelDirection += m_localDirection * normalizeInputValue; // Accumulate axis direction with magnitude in local space, scaling by the normalized input
			m_requestedAccel += m_localDirection * accelParams.AccelAmount * normalizeInputValue; // Accumulate desired acceleration based on thrust amount and input value, combining for multiple axes
		}
		if (m_accelDirection.GetLength() > 1.f) // Normalize accelDirection to mitigate excessive accelerations when combining inputs
		{
			m_accelDirection.Normalize();
		}

		scaledAccelDirection = m_accelDirection * m_requestedAccel.GetLength(); // Scale the direction vector by the magnitude of requestedAccel
		
		if (axisType == AxisType::Linear)
		{
			UpdateAccelerationState(m_linearAccelData, scaledAccelDirection);
		}
		else if (axisType == AxisType::Roll)
		{
			UpdateAccelerationState(m_rollAccelData, scaledAccelDirection);
		}
		else if (axisType == AxisType::PitchYaw)
		{
			UpdateAccelerationState(m_pitchYawAccelData, scaledAccelDirection);
		}
	}
	return scaledAccelDirection;   // Return the scaled acceleration direction vector
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


void CFlightController::ImpulseTracker(Vec3 desiredAccel, const VectorMap<AxisType, DynArray<AxisAccelParams>>& axisAccelParamsList)
{
	for (const auto& axisAccelParamsPair : axisAccelParamsList)	// Iterating over the list of axis and their input values
	{
		AxisType axisType = axisAccelParamsPair.first;
		const DynArray<AxisAccelParams>& axisParamsArray = axisAccelParamsPair.second;

		for (const auto& accelParams : axisParamsArray)	// Iterate over the DynArray<AxisAccelParams> for the current AxisType
		{
			if (axisType == AxisType::Linear)
			{
				// Do stuff
			}
			else if (axisType == AxisType::Roll)
			{

			}
			else if (axisType == AxisType::PitchYaw)
			{

			}
		}
	}
}

Vec3 CFlightController::AccelToImpulse(Vec3 desiredAccel, const VectorMap<AxisType, DynArray<AxisAccelParams>>& axisAccelParamsList, float frameTime)
{
	if (physEntity)
	{
		// Retrieving the dynamics of our entity
		pe_status_dynamics dynamics;
		if (physEntity->GetStatus(&dynamics))
		{
			Vec3 impulse = Vec3(ZERO);
			impulse = desiredAccel * dynamics.mass * frameTime; // Calculates our final impulse based on the entity's mass
			m_totalImpulse += impulse.GetLength();
			return impulse;
		}
	}
	return Vec3(ZERO);
}

bool CFlightController::ApplyImpulse(Vec3 linearImpulse, Vec3 rollImpulse, Vec3 pitchYawImpulse)
{
	IPhysicalEntity* pPhysicalEntity = GetEntity()->GetPhysics();
	if (pPhysicalEntity)
	{
		// Apply linear impulse
		pe_action_impulse actionImpulse;
		actionImpulse.impulse = linearImpulse;
		pPhysicalEntity->Action(&actionImpulse);

		// Apply angular impulse
		actionImpulse.impulse = Vec3(ZERO);
		actionImpulse.angImpulse = rollImpulse + pitchYawImpulse;
		pPhysicalEntity->Action(&actionImpulse);

		// Update our impulse tracking variables to send over to the server
		m_linearImpulse = linearImpulse;
		m_angularImpulse = rollImpulse + pitchYawImpulse;
	}
	return true;
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

///////////////////////////////////////////////////////////////////////////
// FLIGHT MODES 
///////////////////////////////////////////////////////////////////////////

void CFlightController::DirectInput(float frameTime)
{
	m_linearAccelData.targetJerkAccel = ScaleAccel(m_linearAxisParamsMap); // Scale and set the target acceleration for linear movement
	m_linearAccelData.currentJerkAccel = UpdateAccelerationWithJerk(m_linearAccelData, frameTime); 	// Infuse the acceleration value with the current jerk coeficient

	m_rollAccelData.targetJerkAccel = ScaleAccel(m_rollAxisParamsMap);
	m_rollAccelData.currentJerkAccel = UpdateAccelerationWithJerk(m_rollAccelData, frameTime);

	m_pitchYawAccelData.targetJerkAccel = ScaleAccel(m_pitchYawAxisParamsMap);
	m_pitchYawAccelData.currentJerkAccel = UpdateAccelerationWithJerk(m_pitchYawAccelData, frameTime);
	
	// Send movement data to the server if we are connected, apply locally if not
	if (!gEnv->bServer)
	{
		SRmi<RMI_WRAP(&CFlightController::ServerRequestImpulse)>::InvokeOnServer(this, SerializeImpulseData{
		Vec3(ZERO),
		Quat(ZERO),
		AccelToImpulse(m_linearAccelData.currentJerkAccel, m_linearAxisParamsMap, frameTime),
		AccelToImpulse(m_rollAccelData.currentJerkAccel, m_rollAxisParamsMap, frameTime),
		AccelToImpulse(m_pitchYawAccelData.currentJerkAccel, m_pitchYawAxisParamsMap, frameTime)
			});
	}
	else 
		ApplyImpulse(AccelToImpulse(m_linearAccelData.currentJerkAccel, m_linearAxisParamsMap, frameTime), AccelToImpulse(m_rollAccelData.currentJerkAccel, m_rollAxisParamsMap, frameTime), AccelToImpulse(m_pitchYawAccelData.currentJerkAccel, m_pitchYawAxisParamsMap, frameTime));
	
	// Debug
	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 60, 2, m_debugColor, false, "Velocity: %.2f", GetVelocity().GetLength());
	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 90, 2, m_debugColor, false, "acceleration: %.2f", GetAcceleration(frameTime));
	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 120, 2, m_debugColor, false, "total impulse: %.3f", GetImpulse());
}

void CFlightController::CoupledFM(float frameTime)
{
	// Code Stuff
}

///////////////////////////////////////////////////////////////////////////
// FLIGHT MODIFIERS
///////////////////////////////////////////////////////////////////////////

void CFlightController::GravityAssist(float frameTime)
{
	if (m_pEntity)
	{
		pe_status_dynamics dynamics;
		if (m_pEntity->GetPhysicalEntity()->GetStatus(&dynamics))
		{
			Vec3 gravity = gEnv->pPhysicalWorld->GetPhysVars()->gravity;
			Vec3 antiGravityForce = -gravity * dynamics.mass;
			pe_action_impulse impulseAction;
			impulseAction.impulse = antiGravityForce * m_frameTime;
			m_pEntity->GetPhysicalEntity()->Action(&impulseAction);
		}
	}

}

void CFlightController::ComstabAssist(float frameTime)
{
	// Code stuff
}

void CFlightController::FlightModifierHandler(FlightModifierBitFlag bitFlag)
{
	if (bitFlag.HasFlag(EFlightModifierFlag::Coupled))
	{
		CoupledFM(m_frameTime);
		gEnv->pAuxGeomRenderer->Draw2dLabel(50, 30, 2, m_debugColor, false, "Coupled");
	}
	else
	{
		DirectInput(m_frameTime);
		gEnv->pAuxGeomRenderer->Draw2dLabel(50, 30, 2, m_debugColor, false, "Newtonian");
	}
	if (bitFlag.HasFlag(EFlightModifierFlag::Boost))
	{
		gEnv->pAuxGeomRenderer->Draw2dLabel(50, 150, 2, m_debugColor, false, "Boost: ON");
	}
	else 
		gEnv->pAuxGeomRenderer->Draw2dLabel(50, 150, 2, m_debugColor, false, "Boost: OFF");
	if (bitFlag.HasFlag(EFlightModifierFlag::Gravity))
	{
		GravityAssist(m_frameTime);
		gEnv->pAuxGeomRenderer->Draw2dLabel(50, 180, 2, m_debugColor, false, "Gravity assist: ON");
	}
	else
		gEnv->pAuxGeomRenderer->Draw2dLabel(50, 180, 2, m_debugColor, false, "Gravity assist: OFF");
	if (bitFlag.HasFlag(EFlightModifierFlag::Comstab))
	{
		gEnv->pAuxGeomRenderer->Draw2dLabel(50, 210, 2, m_debugColor, false, "Comstab: ON");
	}
	else
		gEnv->pAuxGeomRenderer->Draw2dLabel(50, 210, 2, m_debugColor, false, "Comstab: OFF");
}

///////////////////////////////////////////////////////////////////////////
// NETWORKING
///////////////////////////////////////////////////////////////////////////
bool CFlightController::ServerRequestImpulse(SerializeImpulseData&& data, INetChannel*)
{
	IPhysicalEntity* pPhysicalEntity = GetEntity()->GetPhysics();

	if (pPhysicalEntity)
	{
		SRmi<RMI_WRAP(&CFlightController::ClientRequestImpulse)>::InvokeOnAllClients(this, std::move(data));

		// Update position and orientation serialization variables
		m_shipPosition = GetEntity()->GetWorldPos();
		Matrix34 shipWorldTM = GetEntity()->GetWorldTM();
		m_shipOrientation = Quat(shipWorldTM);
		m_shipPosition = shipWorldTM.GetTranslation();
	}
	return true;
}

bool CFlightController::ClientRequestImpulse(SerializeImpulseData&& data, INetChannel*)
{
	IPhysicalEntity* pPhysicalEntity = GetEntity()->GetPhysics();
	if (pPhysicalEntity)
	{
		ApplyImpulse(data.linearImpulse, data.rollImpulse, data.pitchYawImpulse);
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