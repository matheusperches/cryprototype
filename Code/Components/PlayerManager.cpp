// Copyright 2017-2019 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "PlayerManager.h"
#include <CryRenderer/IRenderAuxGeom.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CryCore/StaticInstanceList.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CrySystem/ConsoleRegistration.h>

// Registers the component to be used in the engine
static void RegisterCPlayerManager(Schematyc::IEnvRegistrar& registrar)
{
	Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
	{
		Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CPlayerManager));
		{

		}
	}
}

CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterCPlayerManager)

void CPlayerManager::Initialize()
{
	// Nothing to see here... yet
}

Cry::Entity::EventFlags CPlayerManager::GetEventMask() const
{
	return EEntityEvent::Update | EEntityEvent::GameplayStarted;
}

void CPlayerManager::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
	case EEntityEvent::GameplayStarted:
	{
		//
	}
	break;
	case EEntityEvent::Update:
	{
		//
	}
	break;
	case Cry::Entity::EEvent::Reset:
	{
		//
	}
	break;
	}
}

void CPlayerManager::CharacterSwitcher(IEntity* requestingEntity, IEntity* targetEntity)
{
	if (gEnv->pEntitySystem && requestingEntity && targetEntity && gEnv->IsEditorGameMode()) // Checking if everything is valid and if we are in editor mode
	{
		if (requestingEntity->GetComponent<CPlayerComponent>()) // Checking if the requestor is the pilot
		{
			if (!requestingEntity->GetParent()) // If the requesting entity is the pilot, making sure it is in fact on foot (it gets set as a child of the ship)
			{
				targetEntity->AttachChild(requestingEntity);
				requestingEntity->Hide(true);

				targetEntity->GetComponent<Cry::DefaultComponents::CCameraComponent>()->Activate(); // Activate the target's camera to switch view points
				targetEntity->GetComponent<CVehicleComponent>()->isActiveEntity = true; // Toggle the variable to listen to the entity's inputs
				requestingEntity->GetComponent<CPlayerComponent>()->isActiveEntity = false;
				gEnv->pConsole->GetCVar("is_piloting")->Set(true);
			}
		}
		else if (requestingEntity->GetComponent<CVehicleComponent>()) // Checking if the requestor is the ship
		{
			Matrix34 currentPosWithOffset = Matrix34::CreateTranslationMat(Vec3(requestingEntity->GetPos().x - 1.0f, requestingEntity->GetPos().y, requestingEntity->GetPos().z));
			targetEntity->SetWorldTM(requestingEntity->GetWorldTM() * currentPosWithOffset);
			targetEntity->DetachThis(); // Detach the pilot from the ship
			targetEntity->Hide(false);

			targetEntity->GetComponent<Cry::DefaultComponents::CCameraComponent>()->Activate();
			targetEntity->GetComponent<CPlayerComponent>()->isActiveEntity = true;
			requestingEntity->GetComponent<CVehicleComponent>()->isActiveEntity = false;
			gEnv->pConsole->GetCVar("is_piloting")->Set(false);
		}
	}
}

void CPlayerManager::EnterLeaveByCvar()
{

}