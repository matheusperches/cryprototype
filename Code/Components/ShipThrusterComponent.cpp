// Copyright 2017-2019 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once
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

void CShipThrusterComponent::ApplyLinearImpulse(IPhysicalEntity* pPhysicalEntity, const Vec3& linearImpulse)
{
	if (m_pEntity->GetComponent<CVehicleComponent>()->GetIsPiloting())
	{
		if (pPhysicalEntity)
		{
			// Apply the force at the specified position and rotation
			impulseAction.impulse = linearImpulse;
			pPhysicalEntity->Action(&impulseAction);
		}
	}
}

void CShipThrusterComponent::ApplyAngularImpulse(IPhysicalEntity* pPhysicalEntity, const Vec3& angularImpulse)
{
	if (m_pEntity->GetComponent<CVehicleComponent>()->GetIsPiloting())
	{
		if (pPhysicalEntity)
		{
			pe_action_impulse torqueAction;
			torqueAction.angImpulse = angularImpulse;
			pPhysicalEntity->Action(&torqueAction);
		}
	}
}
