// Copyright 2017-2019 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once
#include "StdAfx.h"
#include "FlightController.h"

#include <CryRenderer/IRenderAuxGeom.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CryCore/StaticInstanceList.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryPhysics/physinterface.h>

// Forward declaration
#include <DefaultComponents/Input/InputComponent.h>
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
	m_pInputComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CInputComponent>();
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
		InitializeAccelParamsVectors();
		InitializeJerkParams();
	}
	break;
	case EEntityEvent::Update:
	{
		ProcessFlight();
	}
	break;
	case Cry::Entity::EEvent::Reset:
	{
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

	// Initializing the maps with the values of our accels

	m_linearAxisParamsMap[AxisType::Linear] = {
		{"accel_forward", m_fwdAccel},
		{"accel_backward", m_bwdAccel},
		{"accel_left", m_leftRightAccel},
		{"accel_right", m_leftRightAccel},
		{"accel_up", m_upDownAccel},
		{"accel_down", m_upDownAccel}
	};
	m_rollAxisParamsMap[AxisType::Roll] = {
		{"roll_left", radRollAccel},
		{"roll_right", radRollAccel}
	};
	m_pitchYawAxisParamsMap[AxisType::PitchYaw] = {
		{"yaw", radYawAccel},
		{"pitch", radPitchAccel}
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

float CFlightController::NormalizeInput(float inputValue, bool isMouse) const
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

Vec3 CFlightController::ScaleAccel(const VectorMap<AxisType, DynArray<AxisAccelParams>>& axisAccelParamsList)
{
	// Initializing vectors for acceleration direction and desired acceleration
	Vec3 accelDirection(ZERO);	// Vector to accumulate the direction of applied accelerations
	Vec3 requestedAccel(ZERO);  // Vector to accumulate desired acceleration magnitudes
	Vec3 localDirection(ZERO); 		// Calculate local thrust direction based on input value
	float normalizeInputValue = 0.f; // Normalize input values
	Vec3 scaledAccelDirection(ZERO); // Scaled vector, our final local direction + magnitude 
	
	for (const auto& axisAccelParamsPair : axisAccelParamsList)	// Iterating over the list of axis and their input values
	{
		AxisType axisType = axisAccelParamsPair.first;
		const DynArray<AxisAccelParams>& axisParamsArray = axisAccelParamsPair.second;

		for (const auto& accelParams : axisParamsArray)	// Iterate over the DynArray<AxisAccelParams> for the current AxisType
		{
			if (axisType == AxisType::Linear)
			{
				normalizeInputValue = NormalizeInput(AxisGetter(accelParams.axisName)); // Retrieve input value for the current axis and normalize to a range of -1 to 1
				if (accelParams.axisName == "accel_forward") 
				{
					localDirection = Vec3(0.f, 1.f, 0.f); // Forward in local space
				}
				else if (accelParams.axisName == "accel_backward") 
				{
					localDirection = Vec3(0.f, -1.f, 0.f); // Backward in local space
				}
				else if (accelParams.axisName == "accel_left") 
				{
					localDirection = Vec3(-1.f, 0.f, 0.f); // Left in local space
				}
				else if (accelParams.axisName == "accel_right") 
				{
					localDirection = Vec3(1.f, 0.f, 0.f); // Right in local space
				}
				else if (accelParams.axisName == "accel_up") 
				{
					localDirection = Vec3(0.f, 0.f, 1.f); // Up in local space
				}
				else if (accelParams.axisName == "accel_down") 
				{
					localDirection = Vec3(0.f, 0.f, -1.f); // Down in local space
				}
			}
			else if (axisType == AxisType::PitchYaw)
			{
				normalizeInputValue = NormalizeInput(AxisGetter(accelParams.axisName), true);
				if (accelParams.axisName == "yaw") 
				{
					localDirection = Vec3(0.f, 0.f, -1.f);
				}
				else if (accelParams.axisName == "pitch") 
				{
					localDirection = Vec3(-1.f, 0.f, 0.f);
				}
			}
			else if (axisType == AxisType::Roll)
			{
				normalizeInputValue = NormalizeInput(AxisGetter(accelParams.axisName));
				if (accelParams.axisName == "roll_left") 
				{
					localDirection = Vec3(0.f, -1.f, 0.f);
				}
				else if (accelParams.axisName == "roll_right") 
				{
					localDirection = Vec3(0.f, 1.f, 0.f);
				}
			}
			localDirection = ImpulseWorldToLocal(localDirection); // Convert to local space
			accelDirection += localDirection * normalizeInputValue; // Accumulate axis direction with magnitude in local space, scaling by the normalized input
			requestedAccel += localDirection * accelParams.AccelAmount * normalizeInputValue; // Accumulate desired acceleration based on thrust amount and input value, combining for multiple axes
		}

		// Normalize accelDirection to get the direction vector
		if (accelDirection.GetLength() > 0.f)
		{
			accelDirection.Normalize();
		}

		// Scale the direction vector by the magnitude of requestedAccel
		 scaledAccelDirection = accelDirection * requestedAccel.GetLength();
		//CryLog("scaledAccelDirection: x= %f, y= %f, z=%f ", scaledAccelDirection.x, scaledAccelDirection.y, scaledAccelDirection.z);

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

float CFlightController::DegreesToRadian(float degrees)
{
	return degrees * (gf_PI / 180.f);
}

Vec3 CFlightController::AccelToImpulse(Vec3 desiredAccel)
{
	//m_totalImpulse = 0.f;
	if (Validator())
	{
		if (m_physEntity)
		{
			if (m_physEntity->GetStatus(&dynamics))
			{
				Vec3 impulse = desiredAccel * dynamics.mass * gEnv->pTimer->GetFrameTime();
				CryLog("desiredAccel: x= %f, y = %f, z = %f", desiredAccel.x, desiredAccel.y, desiredAccel.z);
				CryLog("impulse: x= %f, y = %f, z = %f", impulse.x, impulse.y, impulse.z);
				m_totalImpulse += impulse.GetLength();
				return impulse;
			}
		}
	}
	return Vec3(ZERO);
}

void CFlightController::ResetImpulseCounter()
{
	m_totalImpulse = 0.f;
}


float CFlightController::GetImpulse()
{
	return m_totalImpulse;
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
	ResetImpulseCounter();

	// Scale and set the target jerk acceleration for linear movement
	m_linearAccelData.targetJerkAccel = ScaleAccel(m_linearAxisParamsMap);

	// Update the current jerk acceleration based on the target jerk acceleration, jerk rate, and frame time
	m_linearAccelData.currentJerkAccel = UpdateAccelerationWithJerk(m_linearAccelData, m_linearAccelData.jerk, m_linearAccelData.jerkDecelRate, gEnv->pTimer->GetFrameTime());

	// Convert the current jerk acceleration to an impulse and apply it to the ship for linear movement
	m_pShipThrusterComponent->ApplyLinearImpulse(m_physEntity, AccelToImpulse(m_linearAccelData.currentJerkAccel));


	m_rollAccelData.targetJerkAccel = ScaleAccel(m_rollAxisParamsMap);
	m_rollAccelData.currentJerkAccel = UpdateAccelerationWithJerk(m_rollAccelData, m_rollAccelData.jerk, m_rollAccelData.jerkDecelRate, gEnv->pTimer->GetFrameTime());
	m_pShipThrusterComponent->ApplyAngularImpulse(m_physEntity, AccelToImpulse(m_rollAccelData.currentJerkAccel));

	m_pitchYawAccelData.targetJerkAccel = ScaleAccel(m_pitchYawAxisParamsMap);
	m_pitchYawAccelData.currentJerkAccel = UpdateAccelerationWithJerk(m_pitchYawAccelData, m_pitchYawAccelData.jerk, m_pitchYawAccelData.jerkDecelRate, gEnv->pTimer->GetFrameTime());
	m_pShipThrusterComponent->ApplyAngularImpulse(m_physEntity, AccelToImpulse(m_pitchYawAccelData.currentJerkAccel));

	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 60, 2, m_debugColor, false, "Velocity: %f", GetVelocity().GetLength());
	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 90, 2, m_debugColor, false, "acceleration: %f", GetAcceleration());
	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 120, 2, m_debugColor, false, "total impulse: %f", GetImpulse());

	/*
	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 80, 2, m_debugColor, false, "rollAccelData.currentAccel: x= %f, y = %f, z = %f", m_rollAccelData.currentJerkAccel.x, m_rollAccelData.currentJerkAccel.y, m_rollAccelData.currentJerkAccel.z);
	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 100, 2, m_debugColor, false, "linearAccelData.currentAccel: x= %f, y = %f, z = %f", m_linearAccelData.currentJerkAccel.x, m_linearAccelData.currentJerkAccel.y, m_linearAccelData.currentJerkAccel.z);
	*/
}