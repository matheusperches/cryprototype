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

void CFlightController::CalculateThrust()
{
	if (Validator())
	{
		if (AxisGetter("accelforward") > 0.0f)
		{
			IPhysicalEntity* physEntity = m_pEntity->GetPhysicalEntity();
			Vec3 worldPos = m_pEntity->GetWorldPos();
			ThrusterParams tParams = ThrusterParams(worldPos, Vec3(0.f, 0.f, 1.0f), fwdThrust);
			//m_pThrusterComponent->ApplySingleThrust(fwdThrust);
			m_pShipThrusterComponent->ApplyThrust(physEntity, tParams);

			/*
			//CryLog("fwd axis value: %f", AxisGetter("accelforward"));
			//CryLog("boosting: %d", IsKeyPressed("boost"));
			IPhysicalEntity* pPhysicalEntity = m_pEntity->GetPhysics();
			if (pPhysicalEntity)
			{
				Vec3 bottomCenter = m_pEntity->GetWorldPos();
				Vec3 forceDirection = Vec3(0.f, 1.f, 0.f);  // Example force direction (downwards)

				// Apply the force at the specified position
				pe_action_impulse impulseAction;
				impulseAction.impulse = forceDirection * fwdThrust;
				impulseAction.point = bottomCenter;
				pPhysicalEntity->Action(&impulseAction);
			}
			*/
		}
	}
}
