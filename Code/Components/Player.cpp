// Copyright 2017-2020 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "Player.h"
#include "GamePlugin.h"

#include <CryRenderer/IRenderAuxGeom.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CryCore/StaticInstanceList.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CryEntitySystem/IEntitySystem.h>

#include <Components/VehicleComponent.h>

// Forward declaration
#include <DefaultComponents/Cameras/CameraComponent.h>
#include <DefaultComponents/Input/InputComponent.h>
#include <DefaultComponents/Physics/CharacterControllerComponent.h>
#include <DefaultComponents/Geometry/AdvancedAnimationComponent.h>
#include <Components/PlayerManager.h>

#define MOUSE_DELTA_TRESHOLD 0.0001f

namespace
{
	static void RegisterPlayerComponent(Schematyc::IEnvRegistrar& registrar)
	{
		Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
		{
			Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CPlayerComponent));
		}
	}

	CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterPlayerComponent);
}

void CPlayerComponent::Initialize()
{
	m_pCameraComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CCameraComponent>();
	m_pInputComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CInputComponent>();
	m_pCharacterController = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CCharacterControllerComponent>();
	m_pAdvancedAnimationComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CAdvancedAnimationComponent>();
	InitializeHumanInput();
}

void CPlayerComponent::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
	case Cry::Entity::EEvent::GameplayStarted:
	{
		hasGameStarted = true;
		// ------------------------- //
		// Can be uncommented to start with the human. Needs to check if other entities are setting camera to active during GameplayStarted first.
		// ------------------------- //
		if (gEnv->pConsole->GetCVar("fps_use_ship")->GetIVal() == 0)
		{
			m_pCameraComponent->Activate();
		}
	}
	break;
	case Cry::Entity::EEvent::Update:
	{
		float color[4] = { 1, 0, 0, 1 };
		//gEnv->pAuxGeomRenderer->Draw2dLabel(50, 100, 2, color, false, "fps_use_ship: %d", gEnv->pConsole->GetCVar("fps_use_ship")->GetIVal());
		//gEnv->pAuxGeomRenderer->Draw2dLabel(50, 125, 2, color, false, "Is Player.cpp Camera component active: %d", m_pCameraComponent->IsActive());
		if (hasGameStarted)
		{
			// ------------------------- //
			// Executing human actions if our camera is active.
			// ------------------------- //

			if (gEnv->pConsole->GetCVar("fps_use_ship")->GetIVal() == 0)
			{
				PlayerMovement();
				CameraMovement();
			}
			/*
			if (gEnv)
			{
				int fps_use_ship = gEnv->pConsole->GetCVar("fps_use_ship")->GetIVal();
				CryLog("cvar value: %d", fps_use_ship);
			}
			*/
		}
	}
	break;
	case Cry::Entity::EEvent::Reset: // Reminder: Gets called both at startup and end
	{
		//Reset everything at startup 
		m_movementDelta = ZERO;
		m_mouseDeltaRotation = ZERO;
		m_lookOrientation = IDENTITY;
		hasGameStarted = false;
		Matrix34 camDefaultMatrix;
		camDefaultMatrix.SetTranslation(m_cameraDefaultPos);
		camDefaultMatrix.SetRotation33(Matrix33(m_pEntity->GetWorldRotation()));
		m_pCameraComponent->SetTransformMatrix(camDefaultMatrix);

		if (shouldStartOnVehicle == true)
		{
			if (gEnv->pConsole->GetCVar("fps_use_ship")->GetIVal() != 1)
			gEnv->pConsole->GetCVar("fps_use_ship")->Set(1);
			else
			{
				//Forcing a call in case the value was already set, because we are reliant on the Cvar OnChange event trigger to move between actors.
				CPlayerManager::GetInstance().CharacterSwitcher();
			}
		}
		else if (shouldStartOnVehicle == false)
		{
			gEnv->pConsole->GetCVar("fps_use_ship")->Set(0);
		}
		else if (gEnv->pConsole->GetCVar("fps_use_ship")->GetIVal() == 1)
		{
		}
	}
	break;
	case Cry::Entity::EEvent::EditorPropertyChanged:
	{
		/*
		if (shouldStartOnVehicle == false)
		{
			gEnv->pConsole->GetCVar("fps_use_ship")->Set(0);
			CryLog("Value changed to 0");
		}
		else
		{
			gEnv->pConsole->GetCVar("fps_use_ship")->Set(1);
			CryLog("Value changed to 1");
		}
		*/
	}
	break;
	}
}

Cry::Entity::EventFlags CPlayerComponent::GetEventMask() const
{
	return Cry::Entity::EEvent::GameplayStarted | Cry::Entity::EEvent::Update | Cry::Entity::EEvent::Reset | Cry::Entity::EEvent::EditorPropertyChanged;
}

void CPlayerComponent::InitializeHumanInput()
{
	// Keyboard Controls
	m_pInputComponent->RegisterAction("human", "moveforward", [this](int activationMode, float value) {m_movementDelta.y = value;  });
	m_pInputComponent->BindAction("human", "moveforward", eAID_KeyboardMouse, eKI_W);

	m_pInputComponent->RegisterAction("human", "moveback", [this](int activationMode, float value) {m_movementDelta.y = -value;  });
	m_pInputComponent->BindAction("human", "moveback", eAID_KeyboardMouse, eKI_S);

	m_pInputComponent->RegisterAction("human", "moveright", [this](int activationMode, float value) {m_movementDelta.x = value;  });
	m_pInputComponent->BindAction("human", "moveright", eAID_KeyboardMouse, eKI_D);

	m_pInputComponent->RegisterAction("human", "moveleft", [this](int activationMode, float value) {m_movementDelta.x = -value;  });
	m_pInputComponent->BindAction("human", "moveleft", eAID_KeyboardMouse, eKI_A);

	m_pInputComponent->RegisterAction("human", "sprint", [this](int activationMode, float value)
		{
			// Changing player movement based on Shift key state

			if (activationMode == (int)eAAM_OnPress)
			{
				m_currentPlayerState = EPlayerState::Sprinting;
			}
			else if (activationMode == eAAM_OnRelease)
			{
				m_currentPlayerState = EPlayerState::Walking;
			}
		});
	m_pInputComponent->BindAction("human", "sprint", eAID_KeyboardMouse, eKI_LShift);

	m_pInputComponent->RegisterAction("human", "jump", [this](int activationMode, float value)
		{

			// Checking if the human camera is active, and then if we are grounded; jumping if so.
			if (gEnv->pConsole->GetCVar("fps_use_ship")->GetIVal() == 0)
			{
				if (m_pCharacterController->IsOnGround())
				{
					m_pCharacterController->AddVelocity(Vec3(0.0f, 0.0f, m_jumpHeight));
				}
				else if (activationMode == eAAM_OnRelease)
				{
					m_currentPlayerState = EPlayerState::Walking;
				}
			}

		});
	m_pInputComponent->BindAction("human", "jump", eAID_KeyboardMouse, eKI_Space);


	m_pInputComponent->RegisterAction("human", "interact", [this](int activationMode, float value)
		{
			Interact(activationMode, value);
		});
	m_pInputComponent->BindAction("human", "interact", eAID_KeyboardMouse, EKeyId::eKI_F);

	// Mouse Controls

	m_pInputComponent->RegisterAction("human", "updown", [this](int activationMode, float value) {m_mouseDeltaRotation.y = -value; });
	m_pInputComponent->BindAction("human", "updown", eAID_KeyboardMouse, eKI_MouseY);

	m_pInputComponent->RegisterAction("human", "leftright", [this](int activationMode, float value) {m_mouseDeltaRotation.x = -value; });
	m_pInputComponent->BindAction("human", "leftright", eAID_KeyboardMouse, eKI_MouseX);

}

void CPlayerComponent::Interact(int activationMode, float value)
{
	if (gEnv->pConsole->GetCVar("fps_use_ship")->GetIVal() == 0)
	{
		if (activationMode == (int)eAAM_OnHold)
		{
			// Creating an offset due to the camera position being set in the editor. Otherwise, the raycast would be stuck into the ground.
			Vec3 offsetWorldPos = Vec3(m_pCameraComponent->GetEntity()->GetWorldPos().x, m_pCameraComponent->GetEntity()->GetWorldPos().y, m_pCameraComponent->GetEntity()->GetWorldPos().z + m_cameraDefaultPos.z);
			IEntity* pHitEntity = RayCast(offsetWorldPos, m_lookOrientation, *m_pEntity);
			if (pHitEntity && pHitEntity->GetComponent<CVehicleComponent>())
			{
				gEnv->pConsole->GetCVar("fps_use_ship")->Set(1);
			}
		}
	}
}

void CPlayerComponent::PlayerMovement()
{
	// Player Movement
	Vec3 velocity = Vec3(m_movementDelta.x, m_movementDelta.y, 0.0f);
	velocity.Normalize();
	//If player state is sprinting, return appropriate (float) speed 
	float playerMoveSpeed = m_currentPlayerState == EPlayerState::Sprinting ? m_runSpeed : m_walkSpeed;
	m_pCharacterController->SetVelocity(m_pEntity->GetWorldRotation() * velocity * playerMoveSpeed);
}

void CPlayerComponent::CameraMovement()
{
	// Creates angles for look orientation
	Ang3 rotationAngle = CCamera::CreateAnglesYPR(Matrix33(m_lookOrientation));
	// Captures the delta rotation from mouse input 
	rotationAngle.x += m_mouseDeltaRotation.x * m_rotationSpeed;
	// Clamps the rotation angles so it does not clip over
	rotationAngle.y = CLAMP(rotationAngle.y + m_mouseDeltaRotation.y * m_rotationSpeed, m_rotationLimitsMinPitch, m_rotationLimitsMaxPitch);
	// Reset to zero to avoid double adding up mouse movement into the rotation
	rotationAngle.z = 0;
	m_lookOrientation = Quat(CCamera::CreateOrientationYPR(rotationAngle));

	// Creates angles for the camera

	Ang3 yawAngle = CCamera::CreateAnglesYPR(Matrix33(m_lookOrientation));
	// Reset to zero to avoid double adding up mouse movement into the rotation
	yawAngle.y = 0;
	// 
	const Quat finalYaw = Quat(CCamera::CreateOrientationYPR(yawAngle));
	m_pEntity->SetRotation(finalYaw);

	Ang3 pitchAngle = CCamera::CreateAnglesYPR(Matrix33(m_lookOrientation));
	// Zeroing out Y to avoid any movement to get added into the mix. 
	pitchAngle.x = 0;
	Matrix34 finalCamMatrix;
	finalCamMatrix.SetTranslation(m_pCameraComponent->GetTransformMatrix().GetTranslation());
	Matrix33 camRotation = CCamera::CreateOrientationYPR(pitchAngle);
	finalCamMatrix.SetRotation33(camRotation);
	m_pCameraComponent->SetTransformMatrix(finalCamMatrix);

}

IEntity* CPlayerComponent::RayCast(Vec3 origin, Quat dir, IEntity& pSkipEntity)
{
	ray_hit hit;
	Vec3 finalDir = dir * Vec3(0.0f, m_InteractionDistance, 0.0f);
	int objTypes = ent_all;
	const unsigned int flags = rwi_stop_at_pierceable | rwi_colltype_any;
	static IPhysicalEntity* pSkipEntities[10];
	pSkipEntities[0] = pSkipEntity.GetPhysics();

	//CryLog("CameraComponent: x=%f, y=%f, z=%f", m_pCameraComponent->GetEntity()->GetWorldPos().x, m_pCameraComponent->GetEntity()->GetWorldPos().y, m_pCameraComponent->GetEntity()->GetWorldPos().z);

	gEnv->pPhysicalWorld->RayWorldIntersection(origin, finalDir, objTypes, flags, &hit, 1, pSkipEntities, 2);

	IPersistantDebug* pPD = gEnv->pGameFramework->GetIPersistantDebug();

	if (hit.pCollider)
	{
		pPD->Begin("Raycast", false);
		pPD->AddSphere(hit.pt, 0.25f, ColorF(Vec3(1, 1, 0), 0.5f), 1.0f);
	}
	IPhysicalEntity* pHitEntity = hit.pCollider;
	IEntity* pEntity = gEnv->pEntitySystem->GetEntityFromPhysics(pHitEntity);
	if (pEntity)
	{
		IEntityClass* pClass = pEntity->GetClass();
		pClassName = pClass->GetName();
		return pEntity;
	}
	else
		return nullptr;
}
