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
				m_frameTime = gEnv->pTimer->GetFrameTime();
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

		// This IF checks if the flag is set to true during game start or exit. If it is, and the cvar value is already set to 1, the 2nd if will be executed to setup the ship correctly regardless.
		// There are code safeties inside CharacterSwitcher() to not do anything if we are not in GameMode, so the entities look correct in the editor.
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
	}
	break;
	}
}

Cry::Entity::EventFlags CPlayerComponent::GetEventMask() const
{
	return Cry::Entity::EEvent::GameplayStarted | Cry::Entity::EEvent::Update | Cry::Entity::EEvent::Reset;
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
		if (activationMode == (int)eAAM_OnPress)
		{
			// Creating an offset due to the camera position being set in the editor. Otherwise, the raycast would be stuck into the ground.
			Vec3 offsetWorldPos = Vec3(m_pCameraComponent->GetEntity()->GetWorldPos().x, m_pCameraComponent->GetEntity()->GetWorldPos().y, m_pCameraComponent->GetEntity()->GetWorldPos().z + m_cameraDefaultPos.z);
			IEntity* pHitEntity = RayCastTest(offsetWorldPos, m_lookOrientation, *m_pEntity);
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
	Ang3 rotationAngle = CCamera::CreateAnglesYPR(Matrix33(m_lookOrientation)); // Captures the delta rotation from mouse input 
	rotationAngle.x += m_mouseDeltaRotation.x * m_rotationSpeed * m_frameTime; // Scale by frame time
	rotationAngle.y = CLAMP(rotationAngle.y + m_mouseDeltaRotation.y * m_rotationSpeed * m_frameTime, m_cameraPitchMin, m_cameraPitchMax); // Scale by frame time and clamp the rotation angles so it does not clip over
	rotationAngle.z = 0.f;
	m_lookOrientation = Quat(CCamera::CreateOrientationYPR(rotationAngle));

	// Creating angles for Yaw rotation
	Ang3 yawAngle = CCamera::CreateAnglesYPR(Matrix33(m_lookOrientation));
	yawAngle.y = 0.f;
	const Quat finalYaw = Quat(CCamera::CreateOrientationYPR(yawAngle));
	m_pEntity->SetRotation(finalYaw);

	// Creating angles for pitch rotation
	Ang3 pitchAngle = CCamera::CreateAnglesYPR(Matrix33(m_lookOrientation));
	pitchAngle.x = 0; // Zeroing out X to avoid any movement to get added into the mix. 
	Matrix34 finalCamMatrix;
	finalCamMatrix.SetTranslation(m_pCameraComponent->GetTransformMatrix().GetTranslation());
	Matrix33 camRotation = CCamera::CreateOrientationYPR(pitchAngle);
	finalCamMatrix.SetRotation33(camRotation);
	m_pCameraComponent->SetTransformMatrix(finalCamMatrix);
}

IEntity* CPlayerComponent::RayCast(Vec3 origin, Quat dir, IEntity& pSkipEntity)
{
	ray_hit hit;
	Vec3 finalDir = dir * Vec3(0.0f, m_playerInteractionRange, 0.0f);

	int objTypes = ent_all;
	const unsigned int flags = rwi_stop_at_pierceable | rwi_colltype_any;

	static IPhysicalEntity* pSkipEntities[10];
	pSkipEntities[0] = pSkipEntity.GetPhysics();

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
		return pEntity;
	}
	else
		return nullptr;
}

IEntity* CPlayerComponent::RayCastTest(Vec3 origin, Quat dir, IEntity& pSkipEntity) const
{
	ray_hit hit;

	// Convert quaternion to direction vector
	Vec3 direction = dir.GetColumn1().GetNormalized(); // Assuming forward direction along y-axis
	Vec3 finalDir = direction * m_playerInteractionRange;

	int objTypes = ent_all;
	const unsigned int flags = rwi_stop_at_pierceable | rwi_colltype_any;

	// Initialize the skip entities array and assign the skip entity
	IPhysicalEntity* pSkipEntities[1] = { pSkipEntity.GetPhysics() };

	// Perform the ray world intersection
	int hits = gEnv->pPhysicalWorld->RayWorldIntersection(origin, finalDir, objTypes, flags, &hit, 1, pSkipEntities, 1);


	if (hits == 0 || !hit.pCollider) // Stop executing right here if there are no hits
	{
		return nullptr;
	}

	// Debug visualization (ensure this is disabled in production builds)
	IPersistantDebug* pPD = gEnv->pGameFramework->GetIPersistantDebug();
	pPD->Begin("Raycast", false);
	pPD->AddSphere(hit.pt, 0.25f, ColorF(Vec3(1, 1, 0), 0.5f), 1.0f);


	IEntity* pEntity = gEnv->pEntitySystem->GetEntityFromPhysics(hit.pCollider);
	return pEntity;

	/*
	if (hit.pCollider)
	{
		pPD->Begin("Raycast", false);
		pPD->AddSphere(hit.pt, 0.25f, ColorF(Vec3(1, 1, 0), 0.5f), 1.0f);
		IPhysicalEntity* pHitEntity = hit.pCollider;
		IEntity* pEntity = gEnv->pEntitySystem->GetEntityFromPhysics(pHitEntity);
		if (pEntity)
		{
			return pEntity;
		}
		else 
			return nullptr; // Return the hit entity if found
	}
	return nullptr; // Return nullptr if no entity was hit
	*/
}