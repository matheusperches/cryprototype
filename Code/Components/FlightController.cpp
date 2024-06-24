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
		{"accel_forward", Vec3(0.f, -1.f, 0.f), fwdAccel},
		{"accel_backward", Vec3(0.f, 1.f, 0.f), bwdAccel},
		{"accel_left", Vec3(1.f, 0.f, 0.f), leftAccel},
		{"accel_right", Vec3(-1.f, 0.f, 0.f), rightAccel},
		{"accel_up", Vec3(0.f, 0.f, 1.f), upAccel},
	};
	 AngularAxisAccelParamsList = {
		{"roll_left", Vec3(0.f, 0.f, 0.f), rollAccel},
		{"roll_right", Vec3(0.f, 0.f, 0.f), rollAccel},
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

Vec3 CFlightController::LinearCalcAccelDirAndScale()
{
	// Initializing vectors for acceleration direction and desired acceleration
	Vec3 accelDirection(0.f, 0.f, 0.f);   // Vector to accumulate the direction of applied accelerations
	Vec3 desiredAccel(0.f, 0.f, 0.f);     // Vector to accumulate desired acceleration magnitudes

	// Iterating over the list of axis and their input values
	for (const auto& LinearAxisAccelParams : LinearAxisAccelParamsList)
	{
		float inputValue = AxisGetter(LinearAxisAccelParams.axisName);   // Retrieve input value for the current axis

		// Calculate acceleration direction based on axis input, combining for multiple axes
		accelDirection += LinearAxisAccelParams.direction * inputValue;

		// Calculate desired acceleration based on thrust amount and input value, combining for multiple axes
		desiredAccel += LinearAxisAccelParams.direction * LinearAxisAccelParams.AccelAmount * inputValue;

		// Log the accumulated desired acceleration for debugging purposes
		//CryLog("desiredAccel:(%f, %f, %f)", desiredAccel.x, desiredAccel.y, desiredAccel.z);
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

	for (const auto& AngularAxisAccelParams : AngularAxisAccelParamsList)
	{
		float inputValue = AxisGetter(AngularAxisAccelParams.axisName);

		angularDirection += AngularAxisAccelParams.direction * inputValue;
		desiredAngularAccel += AngularAxisAccelParams.direction * AngularAxisAccelParams.AccelAmount * inputValue;
	}
	float dotProduct = angularDirection.dot(desiredAngularAccel);
	Vec3 scaledAngularDirection = angularDirection * dotProduct;

	//CryLog("scaledAngularDirection: x= %f, y = %f, z = %f", scaledAngularDirection.x, scaledAngularDirection.y, scaledAngularDirection.z);

	return scaledAngularDirection;
}

Vec3 CFlightController::CalculateThrust(Vec3 desiredLinearAccel)
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
				return thrust;
			}
		}
	}
	return Vec3(0.f, 0.f, 0.f);
}

Vec3 CFlightController::CalculateTorque(Vec3 desiredAngularAccel)
{
	physEntity = m_pEntity->GetPhysicalEntity();
	if (Validator())
	{
		if (physEntity)
		{
			pe_status_pos statusPos;
			if (physEntity->GetStatus(&statusPos))
			{	
				phys_geometry* pg = new phys_geometry();

				CryLog("physicalGeom: x=%f, y=%f, z=%f", pg->Ibody.x, pg->Ibody.y, pg->Ibody.z);
				
				//CryLog("pGeom: x=%f, y=%f, z=%f", physicalGeom.Ibody.x, physicalGeom.Ibody.y, physicalGeom.Ibody.z);
				Vec3 inertiaTensor = pg->Ibody;
				// Calculate the torque as the product of the inertia tensor and desired angular acceleration
				Vec3 torque(
					inertiaTensor.x * desiredAngularAccel.x,
					inertiaTensor.y * desiredAngularAccel.y,
					inertiaTensor.z * desiredAngularAccel.z
				);

				CryLog("Torque: %f, %f, %f", torque.x, torque.y, torque.z);
				return torque;

			}
			else
			{
				// Log error if GetParams fails
				CryLog("Error: Failed to get part parameters for the physical entity.");
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
	Vec3 thrust = CalculateThrust(desiredLinearAccelDir);

	Vec3 desiredAngularAccelDir = AngularCalcAccelDirAndScale();

	Vec3 torque = CalculateTorque(desiredAngularAccelDir);
	//m_pShipThrusterComponent->ApplyTorque(torque);

	Vec3 worldPos = m_pEntity->GetWorldPos();
	m_pShipThrusterComponent->ApplyThrust(physEntity, thrust);

}