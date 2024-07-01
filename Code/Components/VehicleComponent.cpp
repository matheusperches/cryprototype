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

	// Set entity to be visible
	GetEntity()->SetFlags(GetEntity()->GetFlags() | ENTITY_FLAG_CALC_PHYSICS);

	// Load the cube geometry
	const char* geometryPath = "%engine%/engineassets/objects/primitive_cube.cgf";  // Example path to the cube mesh
	GetEntity()->LoadGeometry(0, geometryPath);

	// Optionally, set a material
	const char* materialPath = "%ENGINE%/EngineAssets/Materials/material_default.mtl";
	GetEntity()->SetMaterial(gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(materialPath));
	/*
	m_pCameraComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CCameraComponent>();
	m_pCameraComponent->EnableAutomaticActivation(false);
	m_pInputComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CInputComponent>();
	InitializeInput();
	*/
}

Cry::Entity::EventFlags CVehicleComponent::GetEventMask() const
{
	//Listening to the update event
	return EEntityEvent::GameplayStarted | EEntityEvent::Update | EEntityEvent::Reset;
}

void CVehicleComponent::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
	case EEntityEvent::GameplayStarted:
	{
		m_hasGameStarted = true;
		if (gEnv->bServer)
		{
			SEntitySpawnParams spawnParams;
			spawnParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();
			spawnParams.vPosition = GetEntity()->GetWorldPos();
			spawnParams.qRotation = GetEntity()->GetWorldRotation();
			gEnv->pEntitySystem->SpawnEntity(spawnParams);
			SEntityPhysicalizeParams physParams;
			physParams.type = PE_RIGID;
			m_pEntity->Physicalize(physParams);
		}

		m_pFlightController->ResetJerkParams();
		m_pFlightController->InitializeAccelParamsVectors();
		m_pFlightController->InitializeJerkParams();
		m_pFlightController->physEntity = m_pEntity->GetPhysicalEntity();
	}
	break;
	case EEntityEvent::Update:
	{
		if (GetIsPiloting())
		{
			//const float m_frameTime = event.fParam[0];
			//m_pFlightController->ProcessFlight(m_frameTime);
			//m_velocity = m_pFlightController->GetVelocity();
			//NetMarkAspectsDirty(kVehicleAspect);
		}
	}
	break;
	case Cry::Entity::EEvent::Reset:
	{
		m_hasGameStarted = false;
	}
	break;
	}
}

bool CVehicleComponent::GetIsPiloting()
{
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

IEntity* CVehicleComponent::GetPlayerComponent()
{
	return m_pPlayerComponent;
}