// Copyright 2017-2019 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once
#include "StdAfx.h"
#include "FlightController.h"

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
	m_linearJerkData.jerk = m_linearJerkRate;
	m_linearJerkData.jerkDecelRate = m_linearJerkDecelRate;
	m_rollJerkData.jerk = m_RollJerkRate;
	m_rollJerkData.jerkDecelRate = m_RollJerkDecelRate;
	m_pitchYawJerkData.jerk = m_PitchYawJerkRate;
	m_pitchYawJerkData.jerkDecelRate = m_PitchYawJerkDecelRate;
}

void CFlightController::ResetJerkParams()
{
	m_linearJerkData.currentJerkAccel = Vec3(ZERO);
	m_rollJerkData.currentJerkAccel = Vec3(ZERO);
	m_pitchYawJerkData.currentJerkAccel = Vec3(ZERO);
	m_linearJerkData.targetJerkAccel = Vec3(ZERO);
	m_rollJerkData.targetJerkAccel = Vec3(ZERO);
	m_pitchYawJerkData.targetJerkAccel = Vec3(ZERO);
}

void CFlightController::InitializeMotionParamsVectors()
{
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
		{"roll_left", DEG2RAD(m_rollAccel), DEG2RAD(m_maxRoll), Vec3(0.f, -1.f, 0.f)},
		{"roll_right", DEG2RAD(m_rollAccel), DEG2RAD(m_maxRoll), Vec3(0.f, 1.f, 0.f)}
	};
	m_pitchYawParamsMap[AxisType::PitchYaw] = {
		{"yaw", DEG2RAD(m_yawAccel), DEG2RAD(m_maxYaw), Vec3(0.f, 0.f, -1.f)},
		{"pitch", DEG2RAD(m_pitchAccel), DEG2RAD(m_maxPitch), Vec3(-1.f, 0.f, 0.f)}
	};
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

float CFlightController::ClampInput(float inputValue, float maxAxisAccel, bool isMouse) const
{
	// Scale the input value by the sensitivity factor 
	// If we are mouse, clamp the input based on the max axis accel being used.
	if (isMouse)
	{
		inputValue *= m_mouseSenseFactor;
		inputValue = CLAMP(inputValue, -maxAxisAccel, maxAxisAccel) / maxAxisAccel;
		return inputValue;
	}
	else
		return CLAMP(inputValue, m_MIN_INPUT_VALUE, m_MAX_INPUT_VALUE);
}

///////////////////////////////////////////////////////////////////////////
// FLIGHT CALCULATIONS
///////////////////////////////////////////////////////////////////////////
ScaledMotion CFlightController::ScaleInput(const VectorMap<AxisType, DynArray<AxisMotionParams>>& axisParamsList)
{
	// Initializing vectors for acceleration direction and desired acceleration
	Vec3 localDirection(ZERO); // Calculate local thrust direction based on input value
	float clampedInput(ZERO); // Normalize input values

	// Vectors to accumulate the directions of the requested accelerations and velocities by the input magnitude.
	Vec3 requestedAccelDirection(ZERO), requestedVelDirection(ZERO);
	
	for (const auto& axisMotionParamsPair : axisParamsList)	// Iterating over the list of axis and their input values
	{
		AxisType axisType = axisMotionParamsPair.first;
		const DynArray<AxisMotionParams>& axisMotionParamsArray = axisMotionParamsPair.second;

		for (const auto& motionParams : axisMotionParamsArray)	// Iterate over the DynArray<AxisAccelParams> for the current Axis
		{
			// Need proper mouse detection in the future for joystick support
			switch (axisType)
			{
				case (AxisType::PitchYaw):
				{
					clampedInput = ClampInput(AxisGetter(motionParams.axisName), motionParams.AccelAmount, true); // Retrieve input value for the current axis and clamp to a range of -1 to 1
				}
				break;
				case (AxisType::Roll):
				{
					clampedInput = ClampInput(AxisGetter(motionParams.axisName));
				}
				break;
				default:
				{
					clampedInput = ClampInput(AxisGetter(motionParams.axisName));
				}
			}

			localDirection = WorldToLocal(motionParams.localDirection); // Convert to local space
			
			requestedAccelDirection += localDirection * motionParams.AccelAmount * clampedInput; // Accumulate axis direction, scaling each by its input magnitude in local space
			
			requestedVelDirection += localDirection * motionParams.velocityLimit * clampedInput; // Accumulate desired velocity based on thrust direction and input value
		}
	}
	return ScaledMotion(requestedAccelDirection, requestedVelDirection);   // Return the scaled acceleration direction vector in m/s
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

Vec3 CFlightController::UpdateAccelerationWithJerk(AxisType axisType, JerkAccelerationData& accelData, float frameTime) const
{
	// Calculate the difference between current and target acceleration
	Vec3 deltaAccel = accelData.targetJerkAccel - accelData.currentJerkAccel;
	Vec3 finalJerk;

	// Determine jerk rate based on acceleration state
	switch (accelData.state)
	{
	case EAccelState::Accelerating:
	{
		float tempJerk = accelData.jerk;
		if (m_isBoosting)
		{
			tempJerk *= (axisType == AxisType::Linear) ? m_linearBoost : m_angularBoost;
		}
		float scale = powf(fabs(deltaAccel.GetLength()), 0.15f);
		finalJerk = deltaAccel * scale * tempJerk * frameTime;
		break;
	}
	case EAccelState::Decelerating:
	{
		float tempJerkDecel = accelData.jerkDecelRate;
		if (m_isBoosting)
		{
			tempJerkDecel *= (axisType == AxisType::Linear) ? m_linearBoost : m_angularBoost;
		}
		finalJerk = deltaAccel * tempJerkDecel * frameTime;
		break;
	}
	}

	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 250, 2, m_debugColor, false, "jerk: %f", finalJerk.GetLength());

	// Update current acceleration
	Vec3 newAccel = accelData.currentJerkAccel + finalJerk; // Calculate new acceleration by adding jerk to current acceleration

	return newAccel; // Return the updated acceleration
}

ImpulseResult CFlightController::AccelToImpulse(const MotionData& motionData, float frameTime, bool mathOnly)
{
	pe_status_dynamics dynamics;
	if (!physEntity || !physEntity->GetStatus(&dynamics))
	{
		return ImpulseResult(Vec3(ZERO), Vec3(ZERO)); // return early if we cannot obtain the dynamics status
	}

	Vec3 linearImpulse = Vec3(ZERO);
	Vec3 angImpulse = Vec3(ZERO);

	// Create a copy of motionData to work with if mathOnly is true
	MotionData simulatedMotionData = motionData;

	// Infuse the acceleration value with the current jerk coefficient
	if (mathOnly)
	{
		simulatedMotionData.linearJerkData.targetJerkAccel = simulatedMotionData.linearAccel;
		simulatedMotionData.linearJerkData.currentJerkAccel = UpdateAccelerationWithJerk(AxisType::Linear, simulatedMotionData.linearJerkData, frameTime);

		simulatedMotionData.rollJerkData.targetJerkAccel = simulatedMotionData.rollAccel;
		simulatedMotionData.rollJerkData.currentJerkAccel = UpdateAccelerationWithJerk(AxisType::Roll, simulatedMotionData.rollJerkData, frameTime);

		simulatedMotionData.pitchYawJerkData.targetJerkAccel = simulatedMotionData.pitchYawAccel;
		simulatedMotionData.pitchYawJerkData.currentJerkAccel = UpdateAccelerationWithJerk(AxisType::PitchYaw, simulatedMotionData.pitchYawJerkData, frameTime);
	}
	else
	{
		// Use original motionData when mathOnly is false
		motionData.linearJerkData.targetJerkAccel = motionData.linearAccel;
		motionData.linearJerkData.currentJerkAccel = UpdateAccelerationWithJerk(AxisType::Linear, motionData.linearJerkData, frameTime);

		motionData.rollJerkData.targetJerkAccel = motionData.rollAccel;
		motionData.rollJerkData.currentJerkAccel = UpdateAccelerationWithJerk(AxisType::Roll, motionData.rollJerkData, frameTime);

		motionData.pitchYawJerkData.targetJerkAccel = motionData.pitchYawAccel;
		motionData.pitchYawJerkData.currentJerkAccel = UpdateAccelerationWithJerk(AxisType::PitchYaw, motionData.pitchYawJerkData, frameTime);
	}

	// Calculate impulses based on the jerk data
	if (mathOnly)
	{
		linearImpulse = simulatedMotionData.linearJerkData.currentJerkAccel * dynamics.mass * frameTime; // Use simulated data
		angImpulse = (simulatedMotionData.rollJerkData.currentJerkAccel + simulatedMotionData.pitchYawJerkData.currentJerkAccel) * dynamics.mass * frameTime;
	}
	else
	{
		linearImpulse = motionData.linearJerkData.currentJerkAccel * dynamics.mass * frameTime; // Use original data
		angImpulse = (motionData.rollJerkData.currentJerkAccel + motionData.pitchYawJerkData.currentJerkAccel) * dynamics.mass * frameTime;
	}

	ApplyImpulse(linearImpulse, angImpulse, mathOnly);

	return ImpulseResult(linearImpulse, angImpulse);
}

void CFlightController::ApplyImpulse(Vec3 linearImpulse, Vec3 angImpulse, bool mathOnly)
{

	IPhysicalEntity* pPhysicalEntity = GetEntity()->GetPhysics();
	if (pPhysicalEntity)
	{
		pe_action_impulse actionImpulse;

		actionImpulse.impulse = linearImpulse;
		actionImpulse.angImpulse = angImpulse;


		// Apply linear and angular impulse
		if (!mathOnly)
		{
			pPhysicalEntity->Action(&actionImpulse);
			pPhysicalEntity->Action(&actionImpulse);
		}
		// Update our impulse tracking variables to send over to the server
		m_linearImpulse = linearImpulse;
		m_angularImpulse = actionImpulse.angImpulse;


		Vec3 impulse = (actionImpulse.impulse + actionImpulse.angImpulse) / m_frameTime; // Revert the frametime scaling to have the proper values
		m_totalImpulse += impulse.GetLength();
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

	acceleration = deltaV / frameTime;

	return acceleration;
}

VelocityDiscrepancy CFlightController::CalculateDiscrepancy(Vec3 desiredVelocity)
{
	pe_status_dynamics dynamics;
	if (!m_pEntity || !m_pEntity->GetPhysicalEntity()->GetStatus(&dynamics))
	{
		return VelocityDiscrepancy(ZERO, ZERO);
	}

	Vec3 linearDiscrepancy = desiredVelocity - dynamics.v;
	Vec3 angularDiscrepancy = desiredVelocity - dynamics.w;

	// Compute the discrepancy for each axis
	return VelocityDiscrepancy(linearDiscrepancy, angularDiscrepancy);
}

float CFlightController::LogScale(float discrepancyMagnitude, float maxDiscrepancy, float base = 2.0f)
{
	if (discrepancyMagnitude == 0.0f)
		return 0.0f;

	float logDiscrepancy = std::log(discrepancyMagnitude + 1.0f) / std::log(base);
	float logMaxDiscrepancy = std::log(maxDiscrepancy + 1.0f) / std::log(base);

	return logDiscrepancy / logMaxDiscrepancy;
}

Vec3 CFlightController::CalculateCorrection(const VectorMap<AxisType, DynArray<AxisMotionParams>>& axisAccelParamsMap, Vec3 requestedVelocity, Vec3 velDiscrepancy)
{
	pe_status_dynamics dynamics;
	if (!m_pEntity || !m_pEntity->GetPhysicalEntity()->GetStatus(&dynamics))
	{
		return Vec3(ZERO);
	}
	Vec3 totalCorrectiveAccel = Vec3(ZERO);
	Vec3 predictedVelocity = Vec3(ZERO);
	float overshootFactor = 1.f;

	// Calculate the magnitude of the velocity discrepancy
	float discrepancyMagnitude = velDiscrepancy.GetLength();

	const float maxDiscrepancy = m_logMaxDiscrepancy;
	const float base = m_logBase;

	// Calculate the scaling factor using logarithmic scaling
	float scalingFactor = LogScale(discrepancyMagnitude, maxDiscrepancy, base);

	// Ensure the scaling factor does not exceed 1.0
	scalingFactor = std::min(scalingFactor, 1.0f);


	// Initializing motionData for simulated calculation
	MotionData motionData(Vec3(ZERO), Vec3(ZERO),
		Vec3(ZERO), m_linearJerkData, m_rollJerkData, m_pitchYawJerkData);

	// Handle Linear Correction
	for (const auto& axisAccelParamsPair : axisAccelParamsMap)
	{
		AxisType axisType = axisAccelParamsPair.first;

		const DynArray<AxisMotionParams>& axisParamsArray = axisAccelParamsPair.second;
		for (const auto& accelParams : axisParamsArray)
		{

			Vec3 localDirection = WorldToLocal(accelParams.localDirection).GetNormalized();
			float alignment = localDirection.Dot(velDiscrepancy.GetNormalized());

			Vec3 correction = accelParams.AccelAmount * localDirection * alignment * scalingFactor;
			totalCorrectiveAccel += correction;

			if (axisType == AxisType::Linear)
			{
				motionData.linearAccel = totalCorrectiveAccel;
				Vec3 simulatedAccel = AccelToImpulse(motionData, m_frameTime, true).GetLinearImpulse();
				// Predict future velocity based on current acceleration and jerk
				predictedVelocity = dynamics.v + simulatedAccel; // Use the current acceleration for prediction
			}
			else if (axisType == AxisType::Roll)
			{
				motionData.rollAccel = totalCorrectiveAccel;
				Vec3 simulatedAccel = AccelToImpulse(motionData, m_frameTime, true).GetAngularImpulse();
				// Predict future velocity based on current acceleration and jerk
				predictedVelocity = dynamics.w + simulatedAccel;
			}
			else if (axisType == AxisType::PitchYaw)
			{
				motionData.pitchYawAccel = totalCorrectiveAccel;
				Vec3 simulatedAccel = AccelToImpulse(motionData, m_frameTime, true).GetAngularImpulse();
				predictedVelocity = dynamics.w + simulatedAccel;
			}
		}
	}

	float targetVelocityLength = requestedVelocity.GetLength();

	// Calculate overshoot factor based on predicted velocity
	if (predictedVelocity.GetLength() > targetVelocityLength + 0.1f)
	{
		overshootFactor = targetVelocityLength / predictedVelocity.GetLength();

		// Scale total corrective acceleration by the overshoot factor
		totalCorrectiveAccel *= overshootFactor;
	}

	return totalCorrectiveAccel;
}

///////////////////////////////////////////////////////////////////////////
// FLIGHT MODES 
///////////////////////////////////////////////////////////////////////////

void CFlightController::DirectInput(float frameTime)
{
	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 30, 2, m_debugColor, false, "Newtonian");


	Vec3 linearAccelMagnitude = ScaleInput(m_linearParamsMap).GetAcceleration();
	Vec3 rollAccelMagnitude = ScaleInput(m_rollParamsMap).GetAcceleration();
	Vec3 pitchYawMagnitude = ScaleInput(m_pitchYawParamsMap).GetAcceleration();


	// Updates our current requested motion state to compute jerk accordingly (based on input, not ship motion!)
	UpdateAccelerationState(m_linearJerkData, linearAccelMagnitude);
	UpdateAccelerationState(m_rollJerkData, rollAccelMagnitude);
	UpdateAccelerationState(m_pitchYawJerkData, pitchYawMagnitude);

	MotionData motionData(linearAccelMagnitude, rollAccelMagnitude,
		pitchYawMagnitude, m_linearJerkData, m_rollJerkData, m_pitchYawJerkData);

	// Send movement data to the server if we are connected, apply locally if not
	if (!gEnv->bServer)
	{
		SRmi<RMI_WRAP(&CFlightController::RequestImpulseOnServer)>::InvokeOnServer(this, SerializeImpulseData{
		Vec3(ZERO),
		Quat(ZERO),
		linearAccelMagnitude,
		rollAccelMagnitude,
		pitchYawMagnitude });
	}
	else 
		AccelToImpulse(motionData, frameTime);
}

void CFlightController::CoupledFM(float frameTime)
{
	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 30, 2, m_debugColor, false, "Coupled");

	Vec3 linearVelMagnitude = ScaleInput(m_linearParamsMap).GetVelocity(); // Scale and set the target velocity for linear movement
	Vec3 rollVelMagnitude = ScaleInput(m_rollParamsMap).GetVelocity();
	Vec3 pitchYawVelMagnitude = ScaleInput(m_pitchYawParamsMap).GetVelocity();

	// Updates our current requested motion state to compute jerk accordingly (based on input, not ship motion!)
	UpdateAccelerationState(m_linearJerkData, linearVelMagnitude);

	//CryLog("rollVelMagnitude: x=%f, y=%f, z=%f", rollVelMagnitude.x, rollVelMagnitude.y, rollVelMagnitude.z);
	//CryLog("pitchYawVelMagnitude: x=%f, y=%f, z=%f", pitchYawVelMagnitude.x, pitchYawVelMagnitude.y, pitchYawVelMagnitude.z);

	Vec3 linearDiscrepancy = CalculateDiscrepancy(linearVelMagnitude).GetLinearDiscrepancy();
	Vec3 rollDiscrepancy = CalculateDiscrepancy(rollVelMagnitude).GetAngularDiscrepancy();
	Vec3 pitchYawDiscrepancy = CalculateDiscrepancy(pitchYawVelMagnitude).GetAngularDiscrepancy();

	Vec3 linearCorrection = CalculateCorrection(m_linearParamsMap, linearVelMagnitude, linearDiscrepancy);
	Vec3 rollCorrection = CalculateCorrection(m_rollParamsMap, rollVelMagnitude , rollDiscrepancy);
	Vec3 pitchYawCorrection = CalculateCorrection(m_pitchYawParamsMap, pitchYawVelMagnitude, pitchYawDiscrepancy);

	MotionData motionData(linearCorrection, rollCorrection,
		pitchYawCorrection, m_linearJerkData, m_rollJerkData, m_pitchYawJerkData);

	AccelToImpulse(motionData, frameTime);
}

///////////////////////////////////////////////////////////////////////////
// DEBUG
///////////////////////////////////////////////////////////////////////////

void CFlightController::DrawDirectionIndicator(float frameTime)
{
	pe_status_dynamics dynamics;
	if (!m_pEntity || !m_pEntity->GetPhysicalEntity()->GetStatus(&dynamics))
	{
		return; // Return early if we have a problem getting that data. 
	}
	Vec3 velocity = dynamics.v;

	// Transform the velocity vector to view | Other than direction, we need a 3D position to project onto the 2D screen.
	const CCamera& camera = gEnv->pSystem->GetViewCamera();
	Vec3 cameraPos = camera.GetPosition();
	Vec3 relativeVelocity = cameraPos + velocity.GetNormalized();

	// Project the 3D position (relative to the camera) to 2D screen coordinates
	Vec3 screenPos;
	gEnv->pRenderer->ProjectToScreen(relativeVelocity.x, relativeVelocity.y, relativeVelocity.z, &screenPos.x, &screenPos.y, &screenPos.z);

	// Converting to pixel coordinates
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

void CFlightController::DrawOnScreenDebugText(float frameTime)
{
	// Debug
	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 60, 2, m_debugColor, false, "Velocity: %.2f", GetVelocity().GetLength());
	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 90, 2, m_debugColor, false, "acceleration: %.2f", GetAcceleration(frameTime));
	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 120, 2, m_debugColor, false, "total impulse: %.3f", GetImpulse());

	DrawDirectionIndicator(frameTime);
}

///////////////////////////////////////////////////////////////////////////
// FLIGHT MODIFIERS
///////////////////////////////////////////////////////////////////////////

void CFlightController::AntiGravity(float frameTime)
{
	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 180, 2, m_debugColor, false, "Anti-Gravity: ON");

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

void CFlightController::BoostManager(bool isBoosting, float frameTime)
{
	if (isBoosting)
	{
		m_isBoosting = true;
		gEnv->pAuxGeomRenderer->Draw2dLabel(50, 150, 2, m_debugColor, false, "Boost: ON");
	}
	else
	{
		m_isBoosting = false;
		gEnv->pAuxGeomRenderer->Draw2dLabel(50, 150, 2, m_debugColor, false, "Boost: OFF");
	}
	// Adds a multiplier to the jerk values to enhance the ship's responsiveness, removes the multiplier when not using.
}

void CFlightController::FlightModifierHandler(FlightModifierBitFlag bitFlag, float frameTime)
{
	if (bitFlag.HasFlag(EFlightModifierFlag::Coupled))
	{
		CoupledFM(frameTime);
		bitFlag.SetFlag(EFlightModifierFlag::Gravity); // Enforcing gravity assist in coupled mode
	}
	else
	{
		DirectInput(frameTime);
	}
	if (bitFlag.HasFlag(EFlightModifierFlag::Boost))
	{
		BoostManager(true, frameTime);
	}
	else
	{
		BoostManager(false, frameTime);
	}
	if (bitFlag.HasFlag(EFlightModifierFlag::Gravity))
	{
		AntiGravity(frameTime);
	}
	else
		gEnv->pAuxGeomRenderer->Draw2dLabel(50, 180, 2, m_debugColor, false, "Anti-Gravity: OFF");

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
		MotionData motionData(data.linearImpulse, data.rollImpulse,
			data.pitchYawImpulse, m_linearJerkData, m_rollJerkData, m_pitchYawJerkData);
		AccelToImpulse(motionData, m_frameTime);
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
		ser.EndGroup();

		return true;
	}
	return false;
}