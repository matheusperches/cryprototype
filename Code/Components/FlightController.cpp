// Copyright 2017-2019 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "FlightController.h"
#include <CryRenderer/IRenderAuxGeom.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CryCore/StaticInstanceList.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryPhysics/primitives.h>
#include <CryPhysics/IPhysics.h>
#include <CryPhysics/physinterface.h>

// Forward declaration
#include <DefaultComponents/Input/InputComponent.h>
#include <DefaultComponents/Physics/RigidBodyComponent.h>
#include <Components/VehicleComponent.h>
#include <Components/ShipThrusterComponent.h>

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
	physEntity = m_pEntity->GetPhysicalEntity();
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
	float radYawAccel = DegreesToRadian(yawAccel);
	float radPitchAccel = DegreesToRadian(pitchAccel);
	float radRollAccel = DegreesToRadian(rollAccel);

	// Initializing the list with the values of our accels
	LinearAxisAccelParamsList = {
		{"accel_forward", fwdAccel},
		{"accel_backward", bwdAccel},
		{"accel_left", leftAccel},
		{"accel_right", rightAccel},
		{"accel_up", upAccel},
		{"accel_down", downAccel},
	};
	RollAxisAccelParamsList = {
		{"roll_left", radRollAccel},
		{"roll_right", radRollAccel},
	};
	PitchYawAxisAccelParamsList = {
		{"yaw", radYawAccel},
		{"pitch", radPitchAccel},
	};
}

bool CFlightController::IsKeyPressed(const std::string& actionName)
{
	return m_pEntity->GetComponent<CVehicleComponent>()->IsKeyPressed(actionName);
}

float CFlightController::AxisGetter(const std::string& axisName)
{
	return m_pEntity->GetComponent<CVehicleComponent>()->GetAxisValue(axisName);
}

Vec3 CFlightController::WorldToLocal(const Vec3& localDirection)
{
	Quat worldRotation = GetEntity()->GetWorldRotation();

	Vec3 worldDirection = worldRotation * localDirection;

	return worldDirection;
}

float CFlightController::NormalizeInput(float inputValue, bool isMouse)
{
	// Scale the input value by the sensitivity factor
	if (isMouse)
		inputValue *= mouseSenseFactor;

	return CLAMP(inputValue, MIN_INPUT_VALUE, MAX_INPUT_VALUE);
}

Vec3 CFlightController::UpdateAccelerationWithJerk(const Vec3& currentAccel, const Vec3& targetAccel, float deltaTime)
{
	Vec3 deltaAccel = targetAccel - currentAccel;
	/*
	Vec3 jerk = deltaAccel * jerkRate * deltaTime;
	Vec3 newAccel = currentAccel + jerk;
	*/

	float scale = powf(fabs(deltaAccel.GetLength()), 0.5f);
	Vec3 jerk = deltaAccel.GetNormalized() * scale * jerkRate * deltaTime;

	Vec3 newAccel = currentAccel + jerk;

	// Optional: Clamp newAccel to targetAccel to prevent overshooting
	if ((deltaAccel.dot(jerk) < 0.f))
	{
		newAccel = targetAccel;
	}

	return newAccel;
}

Vec3 CFlightController::ScaleLinearAccel()
{
	// Initializing vectors for acceleration direction and desired acceleration
	Vec3 accelDirection(0.f, 0.f, 0.f);   // Vector to accumulate the direction of applied accelerations
	Vec3 requestedAccel(0.f, 0.f, 0.f);     // Vector to accumulate desired acceleration magnitudes

	// Iterating over the list of axis and their input values
	for (const auto& LinearAxisAccelParams : LinearAxisAccelParamsList)
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
		localDirection = WorldToLocal(localDirection);

		// Calculate acceleration direction based on axis input, combining all axes
		accelDirection += localDirection * NormalizedinputValue;

		// Calculate desired acceleration based on thrust amount and input value, combining for multiple axes
		requestedAccel += localDirection * LinearAxisAccelParams.AccelAmount * NormalizedinputValue;
	}

	// Calculate the dot product of accelDirection and desiredAccel to preserve the direction
	float dotProduct = accelDirection.dot(requestedAccel);

	// Scale accelDirection by the dot product to get the final scaled acceleration direction
	Vec3 scaledAccelDirection = accelDirection * dotProduct;
	
	return scaledAccelDirection;   // Return the scaled acceleration direction vector
}

Vec3 CFlightController::ScaleRollAccel()
{
	// Initializing vectors for acceleration direction and desired acceleration
	Vec3 accelDirection(0.f, 0.f, 0.f);   // Vector to accumulate the direction of applied accelerations
	Vec3 requestedAccel(0.f, 0.f, 0.f);     // Vector to accumulate desired acceleration magnitudes

	// Iterating over the list of axis and their input values
	for (const auto& RollAxisAccelParams : RollAxisAccelParamsList)
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

		localDirection = WorldToLocal(localDirection);

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

	return scaledAccelDirection;   // Return the scaled acceleration direction vector
}

Vec3 CFlightController::ScalePitchYawAccel()
{
	// Initializing vectors for acceleration direction and desired acceleration
	Vec3 accelDirection(0.f, 0.f, 0.f);   // Vector to accumulate the direction of applied accelerations
	Vec3 requestedAccel(0.f, 0.f, 0.f);     // Vector to accumulate desired acceleration magnitudes
	// Calculate local thrust direction based on input value
	Vec3 localDirection;

	// Iterating over the list of axis and their input values
	for (const auto& PitchYawAxisAccelParamsList : PitchYawAxisAccelParamsList)
	{
		float NormalizedinputValue = NormalizeInput(AxisGetter(PitchYawAxisAccelParamsList.axisName), true);   // Retrieve input value for the current axis and normalize to a range of -1 to 1

		if (PitchYawAxisAccelParamsList.axisName == "yaw") {
			localDirection = Vec3(0.f, 0.f, -1.f); // yaw left or right
		}
		else if (PitchYawAxisAccelParamsList.axisName == "pitch") {
			localDirection = Vec3(-1.f, 0.f, 0.f); // pitch left or right in local space
		}

		// Adjust our direction to be local space, given the direction we want to go
		localDirection = WorldToLocal(localDirection);

		// Calculate acceleration direction based on axis input, combining all axes
		accelDirection += localDirection * NormalizedinputValue;

		// Calculate desired acceleration based on thrust amount and input value, combining for multiple axes
		requestedAccel += localDirection * PitchYawAxisAccelParamsList.AccelAmount * NormalizedinputValue;
	}

	// Calculate the dot product of accelDirection and desiredAccel to preserve the direction
	float dotProduct = accelDirection.dot(requestedAccel);

	// Scale accelDirection by the dot product to get the final scaled acceleration direction
	Vec3 scaledAccelDirection = accelDirection * dotProduct;

	// Log the scaled acceleration direction for debugging purposes
	//CryLog("scaledAccelDirection: x= %f, y = %f, z = %f", scaledAccelDirection.x, scaledAccelDirection.y, scaledAccelDirection.z);

	return scaledAccelDirection;   // Return the scaled acceleration direction vector
}

float CFlightController::DegreesToRadian(float degrees)
{
	return degrees * (gf_PI / 180.f);
}

Vec3 CFlightController::AccelToImpulse(Vec3 desiredAccel)
{
	if (Validator())
	{
		if (physEntity)
		{
			pe_status_dynamics dynamics;
			if (physEntity->GetStatus(&dynamics))
			{
				float mass = dynamics.mass;
				Vec3 impulse = desiredAccel * mass;
				return impulse;
			}
		}
	}
	return Vec3(0.f, 0.f, 0.f);
}

void CFlightController::ProcessFlight()
{
	Vec3 requestedLinearAccelDir = ScaleLinearAccel();
	Vec3 linearThrust = AccelToImpulse(requestedLinearAccelDir);

	Vec3 requestedRollAccelDir = ScaleRollAccel();
	Vec3 rollThrust = AccelToImpulse(requestedRollAccelDir);

	Vec3 requestedPitchYawAccelDir = ScalePitchYawAccel();
	Vec3 pitchYawThrust = AccelToImpulse(requestedPitchYawAccelDir);

	currentLinearAccel = UpdateAccelerationWithJerk(currentLinearAccel, linearThrust, gEnv->pTimer->GetFrameTime());
	currentAngularAccel = UpdateAccelerationWithJerk(currentAngularAccel, pitchYawThrust, gEnv->pTimer->GetFrameTime());

	m_pShipThrusterComponent->ApplyLinearImpulse(physEntity, currentLinearAccel);
	m_pShipThrusterComponent->ApplyAngularImpulse(physEntity, rollThrust);
	m_pShipThrusterComponent->ApplyAngularImpulse(physEntity, pitchYawThrust);


	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 120, 2, color, false, "currentLinearAccel: x= %f, y = %f, z = %f", currentLinearAccel.x, currentLinearAccel.y, currentLinearAccel.z);
	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 140, 2, color, false, "currentAngularAccel: x= %f, y = %f, z = %f", currentAngularAccel.x, currentAngularAccel.y, currentAngularAccel.z);

	/*
	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 120, 2, color, false, "linearThrust: x= %f, y = %f, z = %f", linearThrust.x, linearThrust.y, linearThrust.z);
	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 140, 2, color, false, "pitchYawThrust: x= %f, y = %f, z = %f", pitchYawThrust.x, pitchYawThrust.y, pitchYawThrust.z);
	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 160, 2, color, false, "rollThrust: x= %f, y = %f, z = %f", rollThrust.x, rollThrust.y, rollThrust.z);
	*/
}