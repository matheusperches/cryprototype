// Copyright 2017-2019 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "PlayerManager.h"
#include <CryRenderer/IRenderAuxGeom.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CryCore/StaticInstanceList.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CrySystem/ConsoleRegistration.h>

// Forward declaration
#include <DefaultComponents/Input/InputComponent.h>


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

void CPlayerManager::CharacterSwitcher()
{
	// Bug: FPS model gets stuck flying mid air if gamemode ends with Value = 1 / probably not resetting things correctly
	fps_use_ship = gEnv->pConsole->GetCVar("fps_use_ship");
	if (gEnv->pEntitySystem)
	{
		IEntity* shipEntity = gEnv->pEntitySystem->FindEntityByName("ship");
		IEntity* playerEntity = gEnv->pEntitySystem->FindEntityByName("human");

		if (gEnv->IsEditorGameMode())
		{
			if (playerEntity || shipEntity)
			{
				if (fps_use_ship->GetIVal() == 0)
				{
					if (!playerEntity->GetParent())
					{
						// Force unhide and activate
						playerEntity->Hide(false);
						playerEntity->GetComponent<Cry::DefaultComponents::CCameraComponent>()->Activate();
					}
					else
					{
						Matrix34 currentPosWithOffset = Matrix34::CreateTranslationMat(Vec3(shipEntity->GetPos().x - 1.0f, shipEntity->GetPos().y, shipEntity->GetPos().z));
						playerEntity->Hide(false);
						playerEntity->GetComponent<Cry::DefaultComponents::CCameraComponent>()->Activate();
						playerEntity->SetWorldTM(shipEntity->GetWorldTM() * currentPosWithOffset);
						playerEntity->DetachThis();
					}
				}
				else if (fps_use_ship->GetIVal() == 1)
				{
					if (!playerEntity->GetParent())
					{
						playerEntity->Hide(true);
						shipEntity->AttachChild(playerEntity);
						shipEntity->GetComponent<Cry::DefaultComponents::CCameraComponent>()->Activate();
					}
					else
					{
						playerEntity->Hide(true);
						shipEntity->GetComponent<Cry::DefaultComponents::CCameraComponent>()->Activate();
					}
				}
			}
			else
			{
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "One or more entities are invalid: %s, %s",
					shipEntity ? shipEntity->GetName() : "null",
					playerEntity ? playerEntity->GetName() : "null");
			}
		}
	}
}