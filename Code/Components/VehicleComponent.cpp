// Copyright 2017-2019 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "VehicleComponent.h"
#include "GamePlugin.h"

#include <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CryCore/StaticInstanceList.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryEntitySystem/IEntityComponent.h>
#include <CryRenderer/IRenderAuxGeom.h>

#include <CryPhysics/physinterface.h>
#include <CryNetwork/ISerialize.h>
#include <CryNetwork/Rmi.h>

// Forward declaration
#include <DefaultComponents/Cameras/CameraComponent.h>
#include <DefaultComponents/Input/InputComponent.h>
#include <DefaultComponents/Physics/RigidBodyComponent.h>
#include <Components/FlightController.h>

// Registers the component to be used in the engine

namespace
{
	static void RegisterVehicleComponent(Schematyc::IEnvRegistrar& registrar)
	{
		Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
		{
			Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CVehicleComponent));
			{

			}
		}
		CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterVehicleComponent)
	}
}


void CVehicleComponent::Initialize()
{
	m_pRigidBodyComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CRigidBodyComponent>();
	m_pFlightController = m_pEntity->GetOrCreateComponent<CFlightController>();

	// Load the cube geometry
	const char* geometryPath = "%engine%/engineassets/objects/primitive_cube.cgf";  // Example path to the cube mesh
	GetEntity()->LoadGeometry(0, geometryPath);

}

Cry::Entity::EventFlags CVehicleComponent::GetEventMask() const
{
	//Listening to the update event
	return EEntityEvent::GameplayStarted | EEntityEvent::Reset;
}

void CVehicleComponent::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
	case EEntityEvent::GameplayStarted:
	{
		m_hasGameStarted = true;
	}
	break;
	case EEntityEvent::Reset:
	{
		m_hasGameStarted = false;
	}
	}
}

bool CVehicleComponent::GetIsPiloting()
{

	for (uint32 i = 0; i < GetEntity()->GetChildCount(); ++i)
	{
		IEntity* pChildEntity = GetEntity()->GetChild(i);
		if (pChildEntity && pChildEntity->GetComponent<CPlayerComponent>())
		{
			m_pilotID = pChildEntity->GetId();
			m_pPlayerComponent = pChildEntity;
			return true;
		}
	}
	return false;
}

IEntity* CVehicleComponent::GetPlayerComponent() const
{
	return m_pPlayerComponent;
}