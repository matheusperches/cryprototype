// Copyright 2017-2019 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once
#include "StdAfx.h"
#include "FlightController.h"
#include <CryRenderer/IRenderAuxGeom.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CryCore/StaticInstanceList.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryPhysics/primitives.h>
#include <CryPhysics/physinterface.h>
#include <CryCore/Containers/CryArray.h>

// Forward declaration
#include <DefaultComponents/Input/InputComponent.h>
#include <DefaultComponents/Physics/RigidBodyComponent.h>
#include <Components/VehicleComponent.h>

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
	GetVehicleInputManager();
	m_pShipThrusterComponent = m_pEntity->GetOrCreateComponent<CShipThrusterComponent>();
	m_physEntity = m_pEntity->GetPhysicalEntity();
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
		hasGameStarted = true;
		InitializeAccelParamsVectors();
		InitializeJerkParams();
	}
	break;
	case EEntityEvent::Update:
	{
		if (hasGameStarted)
		{
			// Execute Flight controls
			// Check if self is valid
			if (m_pEntity)
			{
				ProcessFlight();
			}
		}
	}
	break;
	case Cry::Entity::EEvent::Reset:
	{
		//Reset everything at startup
		hasGameStarted = false;
		ResetJerkParams();
	}
	break;
	}
}

void CFlightController::GetVehicleInputManager()
{
	m_pInputComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CInputComponent>();
}

bool CFlightController::Validator()
{
	if (gEnv->pConsole->GetCVar("fps_use_ship")->GetIVal() == 1 && m_pEntity->GetChildCount() > 0 && m_pEntity->GetComponent<CVehicleComponent>())
	{
		return true;
	}
	else return false;
}

void CFlightController::InitializeAccelParamsVectors()
{
	float radYawAccel = DegreesToRadian(m_yawAccel);
	float radPitchAccel = DegreesToRadian(m_pitchAccel);
	float radRollAccel = DegreesToRadian(m_rollAccel);

	// Initializing the list with the values of our accels
	m_LinearAxisAccelParamsList = {
		{"accel_forward", m_fwdAccel},
		{"accel_backward", m_bwdAccel},
		{"accel_left", m_leftRightAccel},
		{"accel_right", m_leftRightAccel},
		{"accel_up", m_upDownAccel},
		{"accel_down", m_upDownAccel},
	};
	m_RollAxisAccelParamsList = {
		{"roll_left", radRollAccel},
		{"roll_right", radRollAccel},
	};
	m_PitchYawAxisAccelParamsList = {
		{"yaw", radYawAccel},
		{"pitch", radPitchAccel},
	};
}

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
}

bool CFlightController::IsKeyPressed(const string& actionName)
{
	return m_pEntity->GetComponent<CVehicleComponent>()->IsKeyPressed(actionName);
}

float CFlightController::AxisGetter(const string& axisName)
{
	return m_pEntity->GetComponent<CVehicleComponent>()->GetAxisValue(axisName);
}

Vec3 CFlightController::ImpulseWorldToLocal(const Vec3& localDirection)
{
	Quat worldRotation = m_pEntity->GetWorldRotation();

	Vec3 worldDirection = worldRotation * localDirection;

	return worldDirection;
}

float CFlightController::NormalizeInput(float inputValue, bool isMouse)
{
	// Scale the input value by the sensitivity factor
	if (isMouse)
		inputValue *= m_mouseSenseFactor;

	return CLAMP(inputValue, m_MIN_INPUT_VALUE, m_MAX_INPUT_VALUE);
}

Vec3 CFlightController::UpdateAccelerationWithJerk(JerkAccelerationData& accelData, float jerkRate, float jerkDecelRate, float deltaTime)
{
	// Calculate the difference between current and target acceleration
	Vec3 deltaAccel = accelData.targetJerkAccel - accelData.currentJerkAccel;
	Vec3 jerk;

	switch (accelData.state)
	{
		case AccelState::Idle:
		{
			jerk = Vec3(ZERO);
			break;
		}
		case AccelState::Accelerating:
		{
			// Root jerk: Calculates the scaling factor based on the square root of the magnitude of deltaAccel
			float scale = powf(fabs(deltaAccel.GetLength()), 0.5f);
			// Calculate jerk: normalized deltaAccel scaled by scale, jerkRate, and deltaTime
			jerk = deltaAccel.GetNormalized() * scale * jerkRate * deltaTime;
			break;
		}
		case AccelState::Decelerating:
		{
			jerk = deltaAccel * jerkDecelRate * deltaTime;
			break;
		}
	}
	// Calculate new acceleration by adding jerk to current acceleration
	Vec3 newAccel = accelData.currentJerkAccel + jerk;

	//gEnv->pAuxGeomRenderer->Draw2dLabel(50, 180, 2, color, false, "newAccel: x= %f, y= %f, z=%f", newAccel.x, newAccel.y, newAccel.z);


	// Clamp newAccel to targetAccel to prevent overshooting
	if ((deltaAccel.dot(jerk) < ZERO))
	{
		newAccel = accelData.targetJerkAccel;
		accelData.state = AccelState::Idle;
	}

	// Return the updated acceleration
	return newAccel;
}

Vec3 CFlightController::ScaleLinearAccel()
{
	// Initializing vectors for acceleration direction and desired acceleration
	Vec3 accelDirection(ZERO);   // Vector to accumulate the direction of applied accelerations
	Vec3 requestedAccel(ZERO);     // Vector to accumulate desired acceleration magnitudes

	// Iterating over the list of axis and their input values
	for (const auto& LinearAxisAccelParams : m_LinearAxisAccelParamsList)
	{
		float NormalizedinputValue = NormalizeInput(AxisGetter(LinearAxisAccelParams.axisName));   // Retrieve input value for the current axis and normalize to a range of -1 to 1

		// Calculate local thrust direction based on input value
		Vec3 localDirection;
		if (LinearAxisAccelParams.axisName == "accel_forward") {
			localDirection = Vec3(0.f, 1.f, 0.f); // Forward in local space
		}
		else if (LinearAxisAccelParams.axisName == "accel_backward") {
			localDirection = Vec3(0.f, -1.f, 0.f); // Backward in local space
		}
		else if (LinearAxisAccelParams.axisName == "accel_left") {
			localDirection = Vec3(-1.f, 0.f, 0.f); // Left in local space
		}
		else if (LinearAxisAccelParams.axisName == "accel_right") {
			localDirection = Vec3(1.f, 0.f, 0.f); // Right in local space
		}
		else if (LinearAxisAccelParams.axisName == "accel_up") {
			localDirection = Vec3(0.f, 0.f, 1.f); // Up in local space
		}
		else if (LinearAxisAccelParams.axisName == "accel_down") {
			localDirection = Vec3(0.f, 0.f, -1.f); // Down in local space
		}
		localDirection = ImpulseWorldToLocal(localDirection);

		// Calculate acceleration direction based on axis input, combining all axes
		accelDirection += localDirection * NormalizedinputValue;

		// Calculate desired acceleration based on thrust amount and input value, combining for multiple axes
		requestedAccel += localDirection * LinearAxisAccelParams.AccelAmount * NormalizedinputValue;
	}

	// Calculate the dot product of accelDirection and desiredAccel to preserve the direction
	float dotProduct = accelDirection.dot(requestedAccel);

	// Scale accelDirection by the dot product to get the final scaled acceleration direction
	Vec3 scaledAccelDirection = accelDirection * dotProduct;

	// Populate the accel data
	m_linearAccelData.targetJerkAccel = scaledAccelDirection;
	
	return scaledAccelDirection;   // Return the scaled acceleration direction vector
}

Vec3 CFlightController::ScaleRollAccel()
{
	// Initializing vectors for acceleration direction and desired acceleration
	Vec3 accelDirection(ZERO);   // Vector to accumulate the direction of applied accelerations
	Vec3 requestedAccel(ZERO);     // Vector to accumulate desired acceleration magnitudes

	// Iterating over the list of axis and their input values
	for (const auto& RollAxisAccelParams : m_RollAxisAccelParamsList)
	{
		float NormalizedinputValue = NormalizeInput(AxisGetter(RollAxisAccelParams.axisName));   // Retrieve input value for the current axis and normalize to a range of -1 to 1
		// Calculate local thrust direction based on input value
		Vec3 localDirection;
		if (RollAxisAccelParams.axisName == "roll_left") {
			localDirection = Vec3(0.f, -1.f, 0.f); // roll left in local space
		}
		else if (RollAxisAccelParams.axisName == "roll_right") {
			localDirection = Vec3(0.f, 1.f, 0.f); // roll right in local space
		}

		localDirection = ImpulseWorldToLocal(localDirection);

		// Calculate acceleration direction based on axis input, combining all axes
		accelDirection += localDirection * NormalizedinputValue;

		// Calculate desired acceleration based on thrust amount and input value, combining for multiple axes
		requestedAccel += localDirection * RollAxisAccelParams.AccelAmount * NormalizedinputValue;
	}

	// Calculate the dot product of accelDirection and desiredAccel to preserve the direction
	float dotProduct = accelDirection.dot(requestedAccel);

	// Scale accelDirection by the dot product to get the final scaled acceleration direction
	Vec3 scaledAccelDirection = accelDirection * dotProduct;

	// Log the scaled acceleration direction for debugging purposes
	//CryLog("scaledAccelDirection: x= %f, y = %f, z = %f", scaledAccelDirection.x, scaledAccelDirection.y, scaledAccelDirection.z);

	// Populate the accel data
	m_rollAccelData.targetJerkAccel = scaledAccelDirection;

	return scaledAccelDirection;   // Return the scaled acceleration direction vector
}

Vec3 CFlightController::ScalePitchYawAccel()
{
	// Initializing vectors for acceleration direction and desired acceleration
	Vec3 accelDirection(ZERO);   // Vector to accumulate the direction of applied accelerations
	Vec3 requestedAccel(ZERO);     // Vector to accumulate desired acceleration magnitudes
	// Calculate local thrust direction based on input value
	Vec3 localDirection;

	// Iterating over the list of axis and their input values
	for (const auto& PitchYawAxisAccelParamsList : m_PitchYawAxisAccelParamsList)
	{
		float NormalizedinputValue = NormalizeInput(AxisGetter(PitchYawAxisAccelParamsList.axisName), true);   // Retrieve input value for the current axis and normalize to a range of -1 to 1

		if (PitchYawAxisAccelParamsList.axisName == "yaw") {
			localDirection = Vec3(0.f, 0.f, -1.f); // yaw left or right
		}
		else if (PitchYawAxisAccelParamsList.axisName == "pitch") {
			localDirection = Vec3(-1.f, 0.f, 0.f); // pitch left or right in local space
		}

		// Adjust our direction to be local space, given the direction we want to go
		localDirection = ImpulseWorldToLocal(localDirection);

		// Calculate acceleration direction based on axis input, combining all axes
		accelDirection += localDirection * NormalizedinputValue;

		// Calculate desired acceleration based on thrust amount and input value, combining for multiple axes
		requestedAccel += localDirection * PitchYawAxisAccelParamsList.AccelAmount * NormalizedinputValue;
	}

	// Calculate the dot product of accelDirection and desiredAccel to preserve the direction
	float dotProduct = accelDirection.dot(requestedAccel);

	// Scale accelDirection by the dot product to get the final scaled acceleration direction
	Vec3 scaledAccelDirection = accelDirection * dotProduct;

	// Populate the accel data
	m_pitchYawAccelData.targetJerkAccel = scaledAccelDirection;

	return AccelToImpulse(scaledAccelDirection);   // Return the scaled acceleration direction vector
}

float CFlightController::DegreesToRadian(float degrees)
{
	return degrees * (gf_PI / 180.f);
}

Vec3 CFlightController::AccelToImpulse(Vec3 desiredAccel)
{
	if (Validator())
	{
		if (m_physEntity)
		{
			if (m_physEntity->GetStatus(&dynamics))
			{
				float mass = dynamics.mass;
				Vec3 impulse = desiredAccel * mass;
				//CryLog("impulse: x= %f, y = %f, z = %f", impulse.x, impulse.y, impulse.z);
				return impulse;
			}
		}
	}
	return Vec3(ZERO);
}

void CFlightController::UpdateAccelerationState(JerkAccelerationData& accelData, const Vec3& targetAccel)
{
	accelData.targetJerkAccel = targetAccel;
	if (targetAccel.IsZero())
	{
		accelData.state = AccelState::Decelerating;
	}
	else
	{
		accelData.state = AccelState::Accelerating;
	}
}

Vec3 CFlightController::GetVelocity()
{
	if (m_physEntity)
	{
		if (m_physEntity->GetStatus(&dynamics))
		{
			Vec3 velocity = dynamics.v; // In world space 

			Matrix34 worldToLocalMatrix = m_pEntity->GetWorldTM().GetInverted();

			// Transforming the velocity to local space

			return worldToLocalMatrix.TransformVector(velocity);
		}
	}
	return Vec3();
}

float CFlightController::GetAcceleration()
{
	m_shipVelocity.currentVelocity = GetVelocity().GetLength();
	float acceleration = (m_shipVelocity.currentVelocity - m_shipVelocity.previousVelocity) / gEnv->pTimer->GetFrameTime();
	m_shipVelocity.previousVelocity = m_shipVelocity.currentVelocity;

	return acceleration;
}

void CFlightController::ProcessFlight()
{
	Vec3 targetLinearAccel = ScaleLinearAccel();
	Vec3 targetRollAccelDir = ScaleRollAccel();
	Vec3 targetPitchYawAccel = ScalePitchYawAccel();

	UpdateAccelerationState(m_linearAccelData, targetLinearAccel);
	UpdateAccelerationState(m_rollAccelData, targetRollAccelDir);
	UpdateAccelerationState(m_pitchYawAccelData, targetPitchYawAccel);

	m_linearAccelData.currentJerkAccel = UpdateAccelerationWithJerk(m_linearAccelData, m_linearAccelData.jerk, m_linearAccelData.jerkDecelRate, gEnv->pTimer->GetFrameTime());
	m_pShipThrusterComponent->ApplyLinearImpulse(m_physEntity, AccelToImpulse(m_linearAccelData.currentJerkAccel));

	m_rollAccelData.currentJerkAccel = UpdateAccelerationWithJerk(m_rollAccelData, m_rollAccelData.jerk, m_rollAccelData.jerkDecelRate, gEnv->pTimer->GetFrameTime());
	m_pShipThrusterComponent->ApplyAngularImpulse(m_physEntity, AccelToImpulse(m_rollAccelData.currentJerkAccel));

	m_pitchYawAccelData.currentJerkAccel = UpdateAccelerationWithJerk(m_pitchYawAccelData, m_pitchYawAccelData.jerk, m_pitchYawAccelData.jerkDecelRate, gEnv->pTimer->GetFrameTime());
	m_pShipThrusterComponent->ApplyAngularImpulse(m_physEntity, AccelToImpulse(m_pitchYawAccelData.currentJerkAccel));

	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 60, 2, m_debugColor, false, "Velocity: %f", GetVelocity().GetLength());
	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 90, 2, m_debugColor, false, "acceleration: %f", GetAcceleration());

	/*
	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 80, 2, m_debugColor, false, "rollAccelData.currentAccel: x= %f, y = %f, z = %f", m_rollAccelData.currentJerkAccel.x, m_rollAccelData.currentJerkAccel.y, m_rollAccelData.currentJerkAccel.z);
	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 100, 2, m_debugColor, false, "linearAccelData.currentAccel: x= %f, y = %f, z = %f", m_linearAccelData.currentJerkAccel.x, m_linearAccelData.currentJerkAccel.y, m_linearAccelData.currentJerkAccel.z);
	*/
}