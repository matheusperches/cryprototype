// Copyright 2017-2019 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "VehicleComponent.h"
#include "GamePlugin.h"

#include <CryRenderer/IRenderAuxGeom.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CryCore/StaticInstanceList.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryNetwork/Rmi.h>
#include <CryNetwork/ISerialize.h>

// Forward declaration
#include <DefaultComponents/Cameras/CameraComponent.h>
#include <DefaultComponents/Input/InputComponent.h>
#include <DefaultComponents/Physics/RigidBodyComponent.h>
#include <Components/FlightController.h>

// Registers the component to be used in the engine
static void RegisterVehicleComponent(Schematyc::IEnvRegistrar& registrar)
{
	Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
	{
		Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CVehicleComponent));
		{

		}
	}
}

CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterVehicleComponent)

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
	return EEntityEvent::GameplayStarted;
}

void CVehicleComponent::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
	case EEntityEvent::GameplayStarted:
	{
		m_hasGameStarted = true;

		m_pFlightController->ResetJerkParams();
		m_pFlightController->InitializeAccelParamsVectors();
		m_pFlightController->InitializeJerkParams();
		m_pFlightController->physEntity = m_pEntity->GetPhysicalEntity();
	}
	break;
	}
}

bool CVehicleComponent::GetIsPiloting()
{
	// Iterates over the child entities, trying to find the player component 
	// If one is found, return true.
	for (uint32 i = 0; i < GetEntity()->GetChildCount(); ++i)
	{
		IEntity* pChildEntity = GetEntity()->GetChild(i);
		if (pChildEntity && pChildEntity->GetComponent<CPlayerComponent>())
		{
			pilotID = pChildEntity->GetId();
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