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
	Physicalize(*m_pEntity); 
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
	// Initializing the list with the values of our accels
	// To rework Vec3 with something more dynamic 
	 LinearAxisAccelParamsList = {
		{"accel_forward", fwdAccel},
		{"accel_backward", bwdAccel},
		{"accel_left", leftAccel},
		{"accel_right", rightAccel},
		{"accel_up", upAccel},
		{"accel_down", downAccel},
	};
	 AngularAxisAccelParamsList = {
		{"roll_left", rollAccel},
		{"roll_right", rollAccel},
	 };
	 /*
	 {"yaw_left", Vec3(0.f, 0.f, 1.f), yawAccel},
	 { "yaw_right", Vec3(0.f, 0.f, -1.f), yawAccel},
	 */
}

bool CFlightController::IsKeyPressed(const std::string& actionName)
{
	return m_pEntity->GetComponent<CVehicleComponent>()->IsKeyPressed(actionName);
}

float CFlightController::AxisGetter(const std::string& axisName)
{
	return m_pEntity->GetComponent<CVehicleComponent>()->GetAxisValue(axisName);
}

Vec3 CFlightController::CalculateAccelDirection(const Vec3& localDirection)
{
	Quat worldRotation = GetEntity()->GetWorldRotation();

	Vec3 worldDirection = worldRotation * localDirection;

	return worldDirection;
}

Vec3 CFlightController::LinearCalcAccelDirAndScale()
{
	// Initializing vectors for acceleration direction and desired acceleration
	Vec3 accelDirection(0.f, 0.f, 0.f);   // Vector to accumulate the direction of applied accelerations
	Vec3 desiredAccel(0.f, 0.f, 0.f);     // Vector to accumulate desired acceleration magnitudes

	// Iterating over the list of axis and their input values
	for (const auto& LinearAxisAccelParams : LinearAxisAccelParamsList)
	{
		float inputValue = AxisGetter(LinearAxisAccelParams.axisName);   // Retrieve input value for the current axis
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
			localDirection = Vec3(0.f, 0.f, -1.f); // Up in local space
		}

		Vec3 worldDirection = CalculateAccelDirection(localDirection);

		// Calculate acceleration direction based on axis input, combining all axes
		accelDirection += worldDirection * inputValue;

		// Calculate desired acceleration based on thrust amount and input value, combining for multiple axes
		desiredAccel += worldDirection * LinearAxisAccelParams.AccelAmount * inputValue;
	}

	// Calculate the dot product of accelDirection and desiredAccel to preserve the direction
	float dotProduct = accelDirection.dot(desiredAccel);

	// Scale accelDirection by the dot product to get the final scaled acceleration direction
	Vec3 scaledAccelDirection = accelDirection * dotProduct;

	// Log the scaled acceleration direction for debugging purposes
	//CryLog("scaledAccelDirection: x= %f, y = %f, z = %f", scaledAccelDirection.x, scaledAccelDirection.y, scaledAccelDirection.z);

	return scaledAccelDirection;   // Return the scaled acceleration direction vector
}

Vec3 CFlightController::AngularCalcAccelDirAndScale()
{
	Vec3 angularDirection(0.f, 0.f, 0.f);
	Vec3 desiredAngularAccel(0.f, 0.f, 0.f);
	/*
	for (const auto& AngularAxisAccelParams : AngularAxisAccelParamsList)
	{
		float inputValue = AxisGetter(AngularAxisAccelParams.axisName);

	}

	float dotProduct = angularDirection.dot(desiredAngularAccel);
	Vec3 scaledAngularDirection = angularDirection * dotProduct;
	*/
	Vec3 scaledAngularDirection(0.f, 0.f, 0.f);
	return scaledAngularDirection;
}

Vec3 CFlightController::CalculateLinearThrust(Vec3 desiredLinearAccel)
{
	if (Validator())
	{
		if (physEntity)
		{
			pe_status_dynamics dynamics;
			if (physEntity->GetStatus(&dynamics))
			{
				float mass = dynamics.mass;
				Vec3 thrust = desiredLinearAccel * mass;
				//CryLog("Thrust: x=%f, y= %f, z=%f", thrust.x, thrust.y, thrust.z);
				return thrust;
			}
		}
	}
	return Vec3(0.f, 0.f, 0.f);
}

Vec3 CFlightController::CalculateAngularThrust(Vec3 desiredAngularAccel)
{
	if (Validator())
	{
		if (physEntity)
		{
			pe_status_dynamics dynamics;
			if (physEntity->GetStatus(&dynamics))
			{
				float mass = dynamics.mass;
				Vec3 torque = desiredAngularAccel * mass;
				//CryLog("torque: x=%f, y=%f, z=%f", torque.x, torque.y, torque.z);
				//CryLog("desiredAngularAccel: %f", desiredAngularAccel);
				return torque;
			}
		}
		else
		{
			// Log error if physEntity is null
			CryLog("Error: Physical entity is null.");
		}
	}
	else
	{
		// Log error if Validator fails
		CryLog("Error: Validator check failed.");
	}

	return Vec3(0.f, 0.f, 0.f);
}

void CFlightController::ProcessFlight()
{
	Vec3 desiredLinearAccelDir = LinearCalcAccelDirAndScale();
	Vec3 linearThrust = CalculateLinearThrust(desiredLinearAccelDir);

	Vec3 desiredAngularAccelDir = AngularCalcAccelDirAndScale();

	Vec3 angularThrust = CalculateAngularThrust(desiredAngularAccelDir);

	m_pShipThrusterComponent->ApplyThrust(physEntity, linearThrust);
	m_pShipThrusterComponent->ApplyTorque(physEntity, angularThrust);

	//CryLog("desiredAngularAccelDir: x= %f, y=%f, z=%f", desiredAngularAccelDir.x, desiredAngularAccelDir.y, desiredAngularAccelDir.z);
}

void CFlightController::Physicalize(IEntity& entity)
{
	SEntityPhysicalizeParams physicalizeParams;

	physicalizeParams.type = PE_RIGID;

	physicalizeParams.mass = 50.f;

	physicalizeParams.nSlot = -1;

	entity.Physicalize(physicalizeParams);

	if (IPhysicalEntity* pPhysicalEntity = entity.GetPhysicalEntity())
	{
		CryLog("Physicalized!");

		/*
		
		phys_geometry* pPhys = pPhysicalEntity->

		Vec3 inertiaTensor = statusPos.pGeom->CalcPhysicalProperties(pPhysGeom.Ibody);
		CryLog("inertiaTensor: x=%f, y= %f, z=%f", inertiaTensor.x, inertiaTensor.y, inertiaTensor.z);
		*/
	}
}
