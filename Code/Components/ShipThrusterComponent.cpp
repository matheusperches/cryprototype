// Copyright 2017-2019 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "ShipThrusterComponent.h"
#include <CryRenderer/IRenderAuxGeom.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CryCore/StaticInstanceList.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CryEntitySystem/IEntitySystem.h>

// Forward declaration
#include <DefaultComponents/Physics/RigidBodyComponent.h>
#include <Components/FlightController.h>


// Registers the component to be used in the engine
static void RegisterShipThrusterComponent(Schematyc::IEnvRegistrar& registrar)
{
	Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
	{
		Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CShipThrusterComponent));
		{

		}
	}
}

CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterShipThrusterComponent)

void CShipThrusterComponent::Initialize()
{
	// Initialize stuff
}

Cry::Entity::EventFlags CShipThrusterComponent::GetEventMask() const
{
	//Listening to the update event
	return EEntityEvent::Update | EEntityEvent::GameplayStarted;
}

void CShipThrusterComponent::ProcessEvent(const SEntityEvent& event)
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

bool CShipThrusterComponent::Validator()
{
	if (gEnv->pConsole->GetCVar("fps_use_ship")->GetIVal() == 1 && m_pEntity->GetChildCount() > 0 && m_pEntity->GetComponent<CVehicleComponent>())
	{
		return true;
	}
	else return false;
}

void CShipThrusterComponent::ApplyLinearThrust(IPhysicalEntity* pPhysicalEntity, const Vec3& thrust)
{
	if (Validator())
	{
		if (pPhysicalEntity)
		{
			// Apply the force at the specified position and rotation
			impulseAction.impulse = thrust;
			pPhysicalEntity->Action(&impulseAction);
			//tParams.LogTransform();
		}
	}
}

void CShipThrusterComponent::ApplyAngularThrust(IPhysicalEntity* pPhysicalEntity, const Vec3& torque)
{
	if (Validator())
	{
		if (pPhysicalEntity)
		{
			pe_action_impulse torqueAction;
			torqueAction.angImpulse = torque;
			pPhysicalEntity->Action(&torqueAction);
		}
	}
}
