// Copyright 2017-2019 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once
#include "StdAfx.h"
#include "FlightController.h"
#include "ShipThrusterComponent.h"
#include "ThrusterParams.h"

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
	//m_pInputComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CInputComponent>();
	m_pVehicleComponent = m_pEntity->GetOrCreateComponent<CVehicleComponent>();

	GetEntity()->GetNetEntity()->EnableDelegatableAspect(eEA_GameClientA, true);
	GetEntity()->GetNetEntity()->EnableDelegatableAspect(eEA_Physics, false);

	SRmi<RMI_WRAP(&CFlightController::ServerApplyImpulse)>::Register(this, eRAT_NoAttach, false, eNRT_ReliableOrdered);
	SRmi<RMI_WRAP(&CFlightController::ClientApplyImpulse)>::Register(this, eRAT_NoAttach, false, eNRT_ReliableOrdered);

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
		if (m_pVehicleComponent->GetIsPiloting())
		{
			const float m_frameTime = event.fParam[0];
			ProcessFlight(m_frameTime);
		}
		
	}
	break;
	case Cry::Entity::EEvent::Reset:
	{

	}
	break;
	}
}

void CFlightController::GetVehicleInputManager()
{
	//m_pInputComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CInputComponent>();
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

float CFlightController::DegreesToRadian(float degrees)
{
	return degrees * (gf_PI / 180.f);
}

int CFlightController::KeyState(const string& actionName)
{
	if (m_pVehicleComponent->m_pPlayerComponent)
		return m_pVehicleComponent->m_pPlayerComponent->GetComponent<CPlayerComponent>()->IsKeyPressed(actionName);
	else
		return 0;
}

float CFlightController::AxisGetter(const string& axisName)
{
	if (m_pVehicleComponent->m_pPlayerComponent)
		return m_pVehicleComponent->m_pPlayerComponent->GetComponent<CPlayerComponent>()->GetAxisValue(axisName);
	else
		return 0;
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
		if (accelDirection.GetLength() > 0.f) // Normalize accelDirection to get the direction vector
		{
			accelDirection.Normalize();
		}
		
		scaledAccelDirection = accelDirection * requestedAccel.GetLength(); // Scale the direction vector by the magnitude of requestedAccel
		
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

Vec3 CFlightController::UpdateAccelerationWithJerk(JerkAccelerationData& accelData, float deltaTime)
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
			float scale = powf(fabs(deltaAccel.GetLength()), 0.5f); // Root jerk: Calculates the scaling factor based on the square root of the magnitude of deltaAccel
			jerk = deltaAccel.GetNormalized() * scale * accelData.jerk * deltaTime; // Calculate jerk: normalized deltaAccel scaled by scale, jerkRate, and deltaTime
			break;
		}
		case AccelState::Decelerating:
		{
			jerk = deltaAccel * accelData.jerkDecelRate * deltaTime;
			break;
		}
	}
	Vec3 newAccel = accelData.currentJerkAccel + jerk; // Calculate new acceleration by adding jerk to current acceleration

	if ((deltaAccel.dot(jerk) < ZERO)) 	// Clamp newAccel to targetAccel to prevent overshooting
	{
		newAccel = accelData.targetJerkAccel;
		accelData.state = AccelState::Idle;
	}
	return newAccel; // Return the updated acceleration
}


/* This function is instrumental for the correct execution of UpdateAccelerationWithJerk()
*  This function is called by each axis group independently (Pitch / Yaw; Roll; Linear)
*  Obtains the target acceleration from the JerkAccelerationData struct and updates the struct with the current state.
*  targetAccel is upated according to the player input (-1; 1). Any value different than zero means the player wants to move.
*  Simply sets the acceleration state if the target Accel is zero. 
*/
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

Vec3 CFlightController::AccelToImpulse(Vec3 desiredAccel, float frameTime)
{
	if (physEntity)
	{
		// Retrieving the dynamics of our entity
		pe_status_dynamics dynamics;
		if (physEntity->GetStatus(&dynamics))
		{
			Vec3 impulse = desiredAccel * dynamics.mass * frameTime; // Calculates our final impulse based on the entity's mass.
			m_totalImpulse += impulse.GetLength();
			return impulse;
		}
	}
	return Vec3(ZERO);
}

void CFlightController::ResetImpulseCounter()
{
	m_totalImpulse = 0.f;
}


float CFlightController::GetImpulse() const
{
	return m_totalImpulse;
}

Vec3 CFlightController::GetVelocity()
{
	if (physEntity)
	{
		pe_status_dynamics dynamics; // Retrieving the dynamics of our entity
		if (physEntity->GetStatus(&dynamics))
		{
			Vec3 velocity = dynamics.v; // In world space 

			Matrix34 worldToLocalMatrix = m_pEntity->GetWorldTM().GetInverted();

			// Transforming the velocity to local space

			return worldToLocalMatrix.TransformVector(velocity);
		}
	}
	return Vec3();
}

float CFlightController::GetAcceleration(float frameTime)
{
	m_shipVelocity.currentVelocity = GetVelocity().GetLength();
	float acceleration = (m_shipVelocity.currentVelocity - m_shipVelocity.previousVelocity) / frameTime;
	m_shipVelocity.previousVelocity = m_shipVelocity.currentVelocity;

	return acceleration;
}

/* Puts the flight calculations in order, segmented by axis group, to produce motion.
*  Starts by resetting the impulse counter, a variable used to track how much total impulse we are generating. 
*  Step 1. Call ScaleAccel to create a desired acceleration, with a local space direction, scaled by the magnitude of the input.
*  Step 2. Infuse jerk into the result of step 1
*  Step 3. Convert the result of step 2 into a force and apply that force. 
*  Repeat step 1 to 3 for other axis.
*/
void CFlightController::ProcessFlight(float frameTime)
{
	ResetImpulseCounter();

	m_linearAccelData.targetJerkAccel = ScaleAccel(m_linearAxisParamsMap); // Scale and set the target acceleration for linear movement
	m_linearAccelData.currentJerkAccel = UpdateAccelerationWithJerk(m_linearAccelData, frameTime); 	// Infuse the acceleration value with the current jerk coeficient

	m_rollAccelData.targetJerkAccel = ScaleAccel(m_rollAxisParamsMap);
	m_rollAccelData.currentJerkAccel = UpdateAccelerationWithJerk(m_rollAccelData, frameTime);


	m_pitchYawAccelData.targetJerkAccel = ScaleAccel(m_pitchYawAxisParamsMap);
	m_pitchYawAccelData.currentJerkAccel = UpdateAccelerationWithJerk(m_pitchYawAccelData, frameTime);

	SRmi<RMI_WRAP(&CFlightController::ServerApplyImpulse)>::InvokeOnServer(this, SerializeImpulseData{ 
		Vec3(ZERO), 
		Quat(ZERO), 
		AccelToImpulse(m_linearAccelData.currentJerkAccel, frameTime), 
		AccelToImpulse(m_rollAccelData.currentJerkAccel, frameTime), 
		AccelToImpulse(m_pitchYawAccelData.currentJerkAccel, frameTime) 
		});

	// On-screen Debug 
	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 60, 2, m_debugColor, false, "Velocity: %.2f", GetVelocity().GetLength());
	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 90, 2, m_debugColor, false, "acceleration: %.2f", GetAcceleration(frameTime));
	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 120, 2, m_debugColor, false, "total impulse: %.3f", GetImpulse());
}

bool CFlightController::ServerApplyImpulse(SerializeImpulseData&& data, INetChannel*)
{
	IPhysicalEntity* pPhysicalEntity = GetEntity()->GetPhysics();

	if (pPhysicalEntity)
	{
		SRmi<RMI_WRAP(&CFlightController::ClientApplyImpulse)>::InvokeOnAllClients(this, SerializeImpulseData{ Vec3(ZERO), Quat(ZERO), data.linearImpulse, data.rollImpulse, data.pitchYawImpulse});
		// Update position and orientation
		m_shipPosition = GetEntity()->GetWorldPos();
		Matrix34 shipWorldTM = GetEntity()->GetWorldTM();
		m_shipOrientation = Quat(shipWorldTM);
		m_shipPosition = shipWorldTM.GetTranslation();
	}
	return true;
}

bool CFlightController::ClientApplyImpulse(SerializeImpulseData&& data, INetChannel*)
{
	IPhysicalEntity* pPhysicalEntity = GetEntity()->GetPhysics();
	if (pPhysicalEntity)
	{
		// Apply linear impulse
		pe_action_impulse actionImpulse;
		actionImpulse.impulse = data.linearImpulse;
		pPhysicalEntity->Action(&actionImpulse);

		// Apply angular impulse
		actionImpulse.impulse = Vec3(ZERO);
		actionImpulse.angImpulse = data.rollImpulse + data.pitchYawImpulse;
		pPhysicalEntity->Action(&actionImpulse);

		// Update our impulse tracking variables to send over to the server
		m_linearImpulse = data.linearImpulse;
		m_angularImpulse = data.rollImpulse + data.pitchYawImpulse;

		NetMarkAspectsDirty(kVehicleAspect);
		NetMarkAspectsDirty(kVehiclePhysics);
		
	}
	return true;
}

bool CFlightController::NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags)
{
	if (aspect & kVehicleAspect | kVehiclePhysics)
	{
		ser.BeginGroup("vehicleMovement");

		ser.Value("m_shipPosition", m_shipPosition, 'wrld');
		ser.Value("m_shipOrientation", m_shipOrientation, 'ori3');
		ser.Value("m_linearImpulse", m_linearImpulse, 'vimp');
		ser.Value("m_angularImpulse", m_angularImpulse, 'vimp');
		ser.EndGroup();
		return true;
	}
	return false;
}