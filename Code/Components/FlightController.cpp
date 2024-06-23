// Copyright 2017-2019 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "FlightController.h"
#include <CryRenderer/IRenderAuxGeom.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CryCore/StaticInstanceList.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CryEntitySystem/IEntitySystem.h>

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
	}
	break;
	case EEntityEvent::Update:
	{
		if (hasGameStarted)
		{
			// Execute Flight controls
			// Check if self is valid
			if (m_pEntity)
			CalculateThrust();
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

bool CFlightController::IsKeyPressed(const std::string& actionName)
{
	return m_pEntity->GetComponent<CVehicleComponent>()->IsKeyPressed(actionName);
}

float CFlightController::AxisGetter(const std::string& axisName)
{
	return m_pEntity->GetComponent<CVehicleComponent>()->GetAxisValue(axisName);
}


Vec3 CFlightController::CalculateThrustDirection()
{
	// Initializing a Vec3
	Vec3 thrustDirection(0.f, 0.f, 0.f);

	//flag to check if any input was detected
	bool hasInput = false;

	// Iterating over the axis and input value 
	for (const auto& AxisThrustParams : axisThrusterParamsList)
	{
		float inputValue = AxisGetter(AxisThrustParams.axisName);

		if (inputValue != 0.f)
		{
			hasInput = true;
			// thrust direction calculated based on axis input, combined for multiple axis
			thrustDirection += AxisThrustParams.direction * inputValue;
		}
	}
	if (hasInput)
	thrustDirection.normalize(); // normalizing the vector only if there was input.

	return thrustDirection;
}

void CFlightController::CalculateThrust()
{
	if (Validator())
	{
		IPhysicalEntity* physEntity = m_pEntity->GetPhysicalEntity();
		Vec3 worldPos = m_pEntity->GetWorldPos(); //placeholder
		Vec3 thrustDirection = CalculateThrustDirection();
		if (!thrustDirection.IsZero())
		{
			ThrusterParams tParams = ThrusterParams(worldPos, thrustDirection, fwdThrust);
			m_pShipThrusterComponent->ApplyThrust(physEntity, tParams);
		}
	}
}
