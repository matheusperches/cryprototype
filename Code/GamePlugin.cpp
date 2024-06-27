// Copyright 2016-2019 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "GamePlugin.h"

#include <CrySchematyc/Env/IEnvRegistry.h>
#include <CrySchematyc/Env/EnvPackage.h>
#include <CrySchematyc/Utils/SharedString.h>

#include <IGameObjectSystem.h>
#include <IGameObject.h>
#include <CrySystem/ConsoleRegistration.h>

#include <DefaultComponents/Cameras/CameraComponent.h>
#include <Components/PlayerManager.h>

// Included only once per DLL module.
#include <CryCore/Platform/platform_impl.inl>

CGamePlugin::~CGamePlugin()
{

	gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);

	if (gEnv->pSchematyc)
	{
		gEnv->pSchematyc->GetEnvRegistry().DeregisterPackage(CGamePlugin::GetCID());
	}
}

bool CGamePlugin::Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
{
	// Register for engine system events, in our case we need ESYSTEM_EVENT_GAME_POST_INIT to load the map
	gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "CGamePlugin");
	int value = 0;
	//Create SpawnAndUseShip cvar
	m_fpsUseShip = REGISTER_INT("fps_use_ship", value , VF_CHEAT, "Switch between ship and player through commands");
	m_fpsUseShip->Set(value);
	return true;
}

void CGamePlugin::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
		// Called when the game framework has initialized and we are ready for game logic to start
		case ESYSTEM_EVENT_GAME_POST_INIT:
		{
			// Don't need to load the map in editor
			if (!gEnv->IsEditor())
			{
				// Load the example map in client server mode
				gEnv->pConsole->ExecuteString("map example s", false, true);
			}
			// Uses a specifc component to handle player switching
			m_fpsUseShip->AddOnChange([this]() {CPlayerManager::GetInstance().CharacterSwitcher(); });
		}
		break;

		case ESYSTEM_EVENT_REGISTER_SCHEMATYC_ENV:
		{
			// Register all components that belong to this plug-in
			auto staticAutoRegisterLambda = [](Schematyc::IEnvRegistrar& registrar)
			{
				// Call all static callback registered with the CRY_STATIC_AUTO_REGISTER_WITH_PARAM
				Detail::CStaticAutoRegistrar<Schematyc::IEnvRegistrar&>::InvokeStaticCallbacks(registrar);
			};

			if (gEnv->pSchematyc)
			{
				gEnv->pSchematyc->GetEnvRegistry().RegisterPackage(
					stl::make_unique<Schematyc::CEnvPackage>(
						CGamePlugin::GetCID(),
						"EntityComponents",
						"Crytek GmbH",
						"Components",
						staticAutoRegisterLambda
						)
				);
			}
		}
		break;
		
		case ESYSTEM_EVENT_LEVEL_UNLOAD:
		{
		}
		break;
	}
}

CRYREGISTER_SINGLETON_CLASS(CGamePlugin)