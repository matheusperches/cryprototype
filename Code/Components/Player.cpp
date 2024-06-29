// Copyright 2017-2020 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "Player.h"
#include "Bullet.h"
#include "SpawnPoint.h"
#include "GamePlugin.h"

#include <CryRenderer/IRenderAuxGeom.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CryCore/StaticInstanceList.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryNetwork/Rmi.h>

#include <Components/VehicleComponent.h>

// Forward declaration
#include <DefaultComponents/Cameras/CameraComponent.h>
#include <DefaultComponents/Input/InputComponent.h>
#include <DefaultComponents/Physics/CharacterControllerComponent.h>
#include <DefaultComponents/Geometry/AdvancedAnimationComponent.h>
#include <DefaultComponents/Audio/ListenerComponent.h>
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
	// The character controller is responsible for maintaining player physics
	m_pCharacterController = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CCharacterControllerComponent>();
	// Offset the default character controller up by one unit
	m_pCharacterController->SetTransformMatrix(Matrix34::Create(Vec3(1.f), IDENTITY, Vec3(0, 0, 1.f)));

	// Create the advanced animation component, responsible for updating Mannequin and animating the player
	m_pAdvancedAnimationComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CAdvancedAnimationComponent>();

	// Set the player geometry, this also triggers physics proxy creation
	m_pAdvancedAnimationComponent->SetMannequinAnimationDatabaseFile("Animations/Mannequin/ADB/FirstPerson.adb");
	m_pAdvancedAnimationComponent->SetCharacterFile("Objects/Characters/SampleCharacter/firstperson.cdf");

	m_pAdvancedAnimationComponent->SetControllerDefinitionFile("Animations/Mannequin/ADB/FirstPersonControllerDefinition.xml");
	m_pAdvancedAnimationComponent->SetDefaultScopeContextName("FirstPersonCharacter");
	// Queue the idle fragment to start playing immediately on next update
	m_pAdvancedAnimationComponent->SetDefaultFragmentName("Idle");

	// Disable movement coming from the animation (root joint offset), we control this entirely via physics
	m_pAdvancedAnimationComponent->SetAnimationDrivenMotion(true);

	// Load the character and Mannequin data from file
	m_pAdvancedAnimationComponent->LoadFromDisk();

	// Acquire fragment and tag identifiers to avoid doing so each update
	m_idleFragmentId = m_pAdvancedAnimationComponent->GetFragmentId("Idle");
	m_walkFragmentId = m_pAdvancedAnimationComponent->GetFragmentId("Walk");
	m_rotateTagId = m_pAdvancedAnimationComponent->GetTagId("Rotate");

	m_pEntity->GetNetEntity()->EnableDelegatableAspect(eEA_Physics, false);

	// Register the RemoteReviveOnClient function as a Remote Method Invocation (RMI) that can be executed by the server on clients
	SRmi<RMI_WRAP(&CPlayerComponent::RemoteReviveOnClient)>::Register(this, eRAT_NoAttach, false, eNRT_ReliableOrdered);
	// Register the Server Fire function as well 
	SRmi<RMI_WRAP(&CPlayerComponent::ServerRequestFire)>::Register(this, eRAT_NoAttach, false, eNRT_ReliableOrdered);
	SRmi<RMI_WRAP(&CPlayerComponent::ClientFire)>::Register(this, eRAT_NoAttach, false, eNRT_ReliableOrdered);
	SRmi<RMI_WRAP(&CPlayerComponent::TestValues)>::Register(this, eRAT_NoAttach, false, eNRT_ReliableOrdered);

}

void CPlayerComponent::InitializeLocalPlayer()
{
	m_pCameraComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CCameraComponent>();

	m_pAudioListenerComponent = m_pEntity->GetOrCreateComponent<Cry::Audio::DefaultComponents::CListenerComponent>();

	m_pInputComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CInputComponent>();

	InitializePilotInput();
}

Cry::Entity::EventFlags CPlayerComponent::GetEventMask() const
{
	return 
		Cry::Entity::EEvent::BecomeLocalPlayer |
		Cry::Entity::EEvent::Update |
		Cry::Entity::EEvent::Reset;
}

void CPlayerComponent::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
	case Cry::Entity::EEvent::BecomeLocalPlayer:
	{
		InitializeLocalPlayer();
		//m_pCameraComponent->Activate();
	}
	case Cry::Entity::EEvent::Update:
	{
		if (!m_isAlive) // Don't update the player if we haven't spawned yet
			return;

		const float frameTime = event.fParam[0];
		UpdatePlayerMovementRequest(frameTime);
		UpdateLookDirectionRequest(frameTime);
		UpdateAnimation(frameTime);

		if (IsLocalClient())
		{
			if (m_pCameraComponent)
			UpdateCamera(frameTime);
		}
	}
	break;
	case Cry::Entity::EEvent::Reset: // Reminder: Gets called both at startup and end
	{
		// Disable player when leaving game mode.
		m_isAlive = event.nParam[0] != 0;
	}
	break;
	}
}


void CPlayerComponent::InitializePilotInput()
{
	// Keyboard Controls
	m_pInputComponent->RegisterAction("pilot", "moveforward", [this](int activationMode, float value) {HandleInputFlagChange(EInputFlag::MoveForward, (EActionActivationMode)activationMode); });
	m_pInputComponent->BindAction("pilot", "moveforward", eAID_KeyboardMouse, eKI_W);

	m_pInputComponent->RegisterAction("pilot", "moveback", [this](int activationMode, float value) {HandleInputFlagChange(EInputFlag::MoveBack, (EActionActivationMode)activationMode); });
	m_pInputComponent->BindAction("pilot", "moveback", eAID_KeyboardMouse, eKI_S);

	m_pInputComponent->RegisterAction("pilot", "moveright", [this](int activationMode, float value) {HandleInputFlagChange(EInputFlag::MoveRight, (EActionActivationMode)activationMode); });
	m_pInputComponent->BindAction("pilot", "moveright", eAID_KeyboardMouse, eKI_D);

	m_pInputComponent->RegisterAction("pilot", "moveleft", [this](int activationMode, float value) {HandleInputFlagChange(EInputFlag::MoveLeft, (EActionActivationMode)activationMode); });
	m_pInputComponent->BindAction("pilot", "moveleft", eAID_KeyboardMouse, eKI_A);

	m_pInputComponent->RegisterAction("pilot", "sprint", [this](int activationMode, float value) {HandleInputFlagChange(EInputFlag::Sprint, (EActionActivationMode)activationMode); });
	m_pInputComponent->BindAction("pilot", "sprint", eAID_KeyboardMouse, eKI_LShift);

	m_pInputComponent->RegisterAction("pilot", "jump", [this](int activationMode, float value) {HandleInputFlagChange(EInputFlag::Jump, (EActionActivationMode)activationMode); });

	m_pInputComponent->BindAction("pilot", "jump", eAID_KeyboardMouse, eKI_Space);

	m_pInputComponent->RegisterAction("pilot", "interact", [this](int activationMode, float value) {
		m_interactActivationMode = activationMode;
		HandleInputFlagChange(EInputFlag::Interact, (EActionActivationMode)activationMode); 
		});
	m_pInputComponent->BindAction("pilot", "interact", eAID_KeyboardMouse, EKeyId::eKI_F);

	// Mouse Controls

	m_pInputComponent->RegisterAction("pilot", "updown", [this](int activationMode, float value) {
		m_mouseDeltaRotation.y = -value; 
		NetMarkAspectsDirty(InputAspect);
		});
	m_pInputComponent->BindAction("pilot", "updown", eAID_KeyboardMouse, eKI_MouseY);

	m_pInputComponent->RegisterAction("pilot", "leftright", [this](int activationMode, float value) {
		m_mouseDeltaRotation.x = -value;
		NetMarkAspectsDirty(InputAspect);
		});
	m_pInputComponent->BindAction("pilot", "leftright", eAID_KeyboardMouse, eKI_MouseX);

	// Register the shoot action
	m_pInputComponent->RegisterAction("pilot", "shoot", [this](int activationMode, float value)
		{
			// Only fire on press, not release
			if (activationMode == eAAM_OnPress)
			{
				SRmi<RMI_WRAP(&CPlayerComponent::ServerRequestFire)>::InvokeOnServer(this, NoParams{});
				SRmi<RMI_WRAP(&CPlayerComponent::TestValues)>::InvokeOnServer(this, TestParams{GetEntity()->GetName(), GetEntity()->GetNetEntity()->GetChannelId(), GetEntity()->GetId()});
			}
		});

	// Bind the shoot action to left mouse click
	m_pInputComponent->BindAction("pilot", "shoot", eAID_KeyboardMouse, EKeyId::eKI_Mouse1);

}

void CPlayerComponent::Interact(int activationMode)
{
	if (activationMode == (int)eAAM_OnPress)
	{
		// Creating an offset due to the camera position being set in code. Otherwise, the raycast would be stuck into the ground.
		Vec3 offsetWorldPos = Vec3(m_pCameraComponent->GetEntity()->GetWorldPos().x, m_pCameraComponent->GetEntity()->GetWorldPos().y, m_pCameraComponent->GetEntity()->GetWorldPos().z + m_cameraDefaultPos.z);
		IEntity* pHitEntity = RayCast(offsetWorldPos, m_lookOrientation, *m_pEntity);
		if (pHitEntity && pHitEntity->GetComponent<CVehicleComponent>())
		{
			CPlayerManager::GetInstance().CharacterSwitcher(m_pEntity, pHitEntity);
		}
	}
}

void CPlayerComponent::UpdatePlayerMovementRequest(float frameTime)
{
	// Don't handle input if we are in air
	if (!m_pCharacterController->IsOnGround() || GetIsPiloting())
		return;

	Vec3 velocity = ZERO;

	const float moveSpeed = 20.5f;

	if (m_inputFlags & EInputFlag::MoveLeft)
	{
		velocity.x -=  moveSpeed * frameTime;
	}
	if (m_inputFlags & EInputFlag::MoveRight)
	{
		velocity.x += moveSpeed * frameTime;
	}
	if (m_inputFlags & EInputFlag::MoveForward)
	{
		velocity.y += moveSpeed * frameTime;
	}
	if (m_inputFlags & EInputFlag::MoveBack)
	{
		velocity.y -= moveSpeed * frameTime;
	}
	if (m_inputFlags & EInputFlag::Jump)
	{
		m_pCharacterController->AddVelocity(Vec3(0.0f, 0.0f, m_jumpHeight));
	}
	if (m_inputFlags & EInputFlag::Sprint)
	{
		velocity *= m_runSpeed;
	}
	if (m_inputFlags & EInputFlag::Interact)
	{
		Interact(m_interactActivationMode);
	}

	m_pCharacterController->AddVelocity(GetEntity()->GetWorldRotation() * velocity);
}

void CPlayerComponent::UpdateCamera(float frameTime)
{
	// Start with updating look orientation from the latest input
	Ang3 ypr = CCamera::CreateAnglesYPR(Matrix33(m_lookOrientation));

	ypr.x += m_mouseDeltaRotation.x * m_rotationSpeed;

	const float rotationLimitsMinPitch = -0.84f;
	const float rotationLimitsMaxPitch = 1.5f;

	// TODO: Perform soft clamp here instead of hard wall, should reduce rot speed in this direction when close to limit.
	ypr.y = CLAMP(ypr.y + m_mouseDeltaRotation.y * m_rotationSpeed, rotationLimitsMinPitch, rotationLimitsMaxPitch);
	// Skip roll
	ypr.z = 0;

	m_lookOrientation = Quat(CCamera::CreateOrientationYPR(ypr));

	// Reset every frame
	m_mouseDeltaRotation = ZERO;

	// Ignore z-axis rotation, that's set by CPlayerAnimations
	ypr.x = 0;

	// Start with changing view rotation to the requested mouse look orientation
	Matrix34 localTransform = IDENTITY;
	localTransform.SetRotation33(CCamera::CreateOrientationYPR(ypr));
	const float viewOffsetForward = 0.01f;
	const float viewOffsetUp = 0.26f;

	if (ICharacterInstance* pCharacter = m_pAdvancedAnimationComponent->GetCharacter())
	{
		// Get the local space orientation of the camera joint
		const QuatT& cameraOrientation = pCharacter->GetISkeletonPose()->GetAbsJointByID(m_cameraJointId);
		// Apply the offset to the camera
		localTransform.SetTranslation(cameraOrientation.t + Vec3(0, viewOffsetForward, viewOffsetUp));
	}
	m_pCameraComponent->SetTransformMatrix(localTransform);
	m_pAudioListenerComponent->SetOffset(localTransform.GetTranslation());
}

void CPlayerComponent::UpdateLookDirectionRequest(float frameTime)
{
	const float rotationSpeed = 0.002f;
	const float rotationLimitsMinPitch = -0.84f;
	const float rotationLimitsMaxPitch = 1.5f;

	// Apply smoothing filter to the mouse input
	m_mouseDeltaRotation = m_mouseDeltaSmoothingFilter.Push(m_mouseDeltaRotation).Get();

	// Update angular velocity metrics
	m_horizontalAngularVelocity = (m_mouseDeltaRotation.x * rotationSpeed) / frameTime;
	m_averagedHorizontalAngularVelocity.Push(m_horizontalAngularVelocity);

	if (m_mouseDeltaRotation.IsEquivalent(ZERO, MOUSE_DELTA_TRESHOLD))
		return;
	// Start with updating look orientation from the latest input
	Ang3 ypr = CCamera::CreateAnglesYPR(Matrix33(m_lookOrientation));

	// Yaw
	ypr.x += m_mouseDeltaRotation.x * rotationSpeed;

	// Pitch
	// TODO: Perform soft clamp here instead of hard wall, should reduce rot speed in this direction when close to limit.
	ypr.y = CLAMP(ypr.y + m_mouseDeltaRotation.y * rotationSpeed, rotationLimitsMinPitch, rotationLimitsMaxPitch);
	// Roll (skip)
	ypr.z = 0;

	m_lookOrientation = Quat(CCamera::CreateOrientationYPR(ypr));

	// Reset the mouse delta accumulator every frame
	m_mouseDeltaRotation = ZERO;
}

void CPlayerComponent::UpdateAnimation(float frameTime)
{
	const float angularVelocityTurningThreshold = 0.174; // [rad/s]

	// Update tags and motion parameters used for turning
	const bool isTurning = std::abs(m_averagedHorizontalAngularVelocity.Get()) > angularVelocityTurningThreshold;
	m_pAdvancedAnimationComponent->SetTagWithId(m_rotateTagId, isTurning);
	if (isTurning)
	{
		// TODO: This is a very rough predictive estimation of eMotionParamID_TurnAngle that could easily be replaced with accurate reactive motion 
		// if we introduced IK look/aim setup to the character's model and decoupled entity's orientation from the look direction derived from mouse input.

		const float turnDuration = 1.0f; // Expect the turning motion to take approximately one second.
		m_pAdvancedAnimationComponent->SetMotionParameter(eMotionParamID_TurnAngle, m_horizontalAngularVelocity * turnDuration);
	}

	// Update active fragment
	const FragmentID& desiredFragmentId = m_pCharacterController->IsWalking() ? m_walkFragmentId : m_idleFragmentId;
	if (m_activeFragmentId != desiredFragmentId)
	{
		m_activeFragmentId = desiredFragmentId;
		m_pAdvancedAnimationComponent->QueueFragmentWithId(m_activeFragmentId);
	}

	// Update entity rotation as the player turns
	// We only want to affect Z-axis rotation, zero pitch and roll
	Ang3 ypr = CCamera::CreateAnglesYPR(Matrix33(m_lookOrientation));
	ypr.y = 0;
	ypr.z = 0;
	const Quat correctedOrientation = Quat(CCamera::CreateOrientationYPR(ypr));

	// Send updated transform to the entity, only orientation changes
	GetEntity()->SetPosRotScale(GetEntity()->GetWorldPos(), correctedOrientation, Vec3(1, 1, 1));
}

IEntity* CPlayerComponent::RayCast(Vec3 origin, Quat dir, IEntity& pSkipSelf) const
{
	float m_debugColor[4] = { 1, 0, 0, 1 };
	ray_hit hit;

	// Convert quaternion to direction vector
	Vec3 direction = dir.GetColumn1().GetNormalized(); // Assuming forward direction along y-axis
	Vec3 finalDir = direction * m_playerInteractionRange;

	int objTypes = ent_all;
	const unsigned int flags = rwi_stop_at_pierceable | rwi_colltype_any;

	// Initialize the skip entities array and assign the skip entity
	IPhysicalEntity* pSkipEntities[1] = { pSkipSelf.GetPhysics() };

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
}

bool CPlayerComponent::GetIsPiloting()
{
	return gEnv->pConsole->GetCVar("is_piloting")->GetIVal() == 1 ? true : false;
}

void CPlayerComponent::OnReadyForGameplayOnServer()
{
	CRY_ASSERT(gEnv->bServer, "This function should only be called on the server!");

	const Matrix34 newTransform = CSpawnPointComponent::GetFirstSpawnPointTransform();

	Revive(newTransform);

	// Invoke the RemoteReviveOnClient function on all remote clients, to ensure that Revive is called across the network
	SRmi<RMI_WRAP(&CPlayerComponent::RemoteReviveOnClient)>::InvokeOnOtherClients(this, RemoteReviveParams{ newTransform.GetTranslation(), Quat(newTransform) });

	// Go through all other players, and send the RemoteReviveOnClient on their instances to the new player that is ready for gameplay
	const int channelId = m_pEntity->GetNetEntity()->GetChannelId();
	CGamePlugin::GetInstance()->IterateOverPlayers([this, channelId](CPlayerComponent& player)
		{
			// Don't send the event for the player itself (handled in the RemoteReviveOnClient event above sent to all clients)
			if (player.GetEntityId() == GetEntityId())
				return;

			// Only send the Revive event to players that have already respawned on the server
			if (!player.m_isAlive)
				return;

			// Revive this player on the new player's machine, on the location the existing player was currently at
			const QuatT currentOrientation = QuatT(player.GetEntity()->GetWorldTM());
			SRmi<RMI_WRAP(&CPlayerComponent::RemoteReviveOnClient)>::InvokeOnClient(&player, RemoteReviveParams{ currentOrientation.t, currentOrientation.q }, channelId);
		});
}

bool CPlayerComponent::RemoteReviveOnClient(RemoteReviveParams&& params, INetChannel* pNetChannel)
{
	// Call the Revive function on this client
	Revive(Matrix34::Create(Vec3(1.f), params.rotation, params.position));
	
	return true;
}

void CPlayerComponent::Revive(const Matrix34& transform)
{
	m_isAlive = true;

	// Set the entity transformation, except if we are in the editor
	// In the editor case we always prefer to spawn where the viewport is
	if (!gEnv->IsEditor())
	{
		m_pEntity->SetWorldTM(transform);
	}

	// Apply the character to the entity and queue animations
	m_pAdvancedAnimationComponent->ResetCharacter();
	m_pCharacterController->Physicalize();

	// Reset input now that the player respawned
	m_inputFlags.Clear();
	NetMarkAspectsDirty(InputAspect);

	m_mouseDeltaRotation = ZERO;
	m_lookOrientation = IDENTITY;

	m_mouseDeltaSmoothingFilter.Reset();

	m_activeFragmentId = FRAGMENT_ID_INVALID;

	m_horizontalAngularVelocity = 0.0f;
	m_averagedHorizontalAngularVelocity.Reset();

	if (ICharacterInstance* pCharacter = m_pAdvancedAnimationComponent->GetCharacter())
	{
		// Cache the camera joint id so that we don't need to look it up every frame in UpdateView
		m_cameraJointId = pCharacter->GetIDefaultSkeleton().GetJointIDByName("head");
	}
}

void CPlayerComponent::HandleInputFlagChange(const CEnumFlags<EInputFlag> flags, const CEnumFlags<EActionActivationMode> activationMode, const EInputFlagType type)
{
	switch (type)
	{
	case EInputFlagType::Hold:
	{
		if (activationMode & eAAM_OnRelease)
		{
			m_inputFlags &= ~flags;
		}
		else
		{
			m_inputFlags |= flags;
		}
	}
	break;
	case EInputFlagType::Toggle:
	{
		if (activationMode & eAAM_OnRelease)
		{
			m_inputFlags ^= flags;
		}
	}
	break;
}

	if (IsLocalClient())
	{
		NetMarkAspectsDirty(InputAspect);
	}
}

bool CPlayerComponent::NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags)
{
	if (aspect == InputAspect)
	{
		ser.BeginGroup("PlayerInput");

		const CEnumFlags<EInputFlag> prevInputFlags = m_inputFlags;

		ser.Value("m_inputFlags", m_inputFlags.UnderlyingValue(), 'ui8');

		if (ser.IsReading())
		{
			const CEnumFlags<EInputFlag> changedKeys = prevInputFlags ^ m_inputFlags;

			const CEnumFlags<EInputFlag> pressedKeys = changedKeys & prevInputFlags;
			if (!pressedKeys.IsEmpty())
			{
				HandleInputFlagChange(pressedKeys, eAAM_OnPress);
			}

			const CEnumFlags<EInputFlag> releasedKeys = changedKeys & prevInputFlags;
			if (!releasedKeys.IsEmpty())
			{
				HandleInputFlagChange(pressedKeys, eAAM_OnRelease);
			}
		}

		// Serialize the player look orientation
		ser.Value("m_lookOrientation", m_lookOrientation, 'ori3');

		ser.EndGroup();
	}

	return true;
}

bool CPlayerComponent::ServerRequestFire(NoParams&& p, INetChannel*)
{
	CryLogAlways("Server side shooting");
	SRmi<RMI_WRAP(&CPlayerComponent::ClientFire)>::InvokeOnAllClients(this, NoParams{});
	return true;
}

bool CPlayerComponent::ClientFire(NoParams&& p, INetChannel*)
{
	CryLogAlways("Client side shooting");
	if (ICharacterInstance* pCharacter = m_pAdvancedAnimationComponent->GetCharacter())
	{
		IAttachment* pBarrelOutAttachment = pCharacter->GetIAttachmentManager()->GetInterfaceByName("barrel_out");

		if (pBarrelOutAttachment != nullptr)
		{
			QuatTS bulletOrigin = pBarrelOutAttachment->GetAttWorldAbsolute();

			SEntitySpawnParams spawnParams;
			spawnParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();

			spawnParams.vPosition = bulletOrigin.t;
			spawnParams.qRotation = bulletOrigin.q;

			const float bulletScale = 0.05f;
			spawnParams.vScale = Vec3(bulletScale);

			// Spawn the entity
			if (IEntity* pEntity = gEnv->pEntitySystem->SpawnEntity(spawnParams))
			{
				// See Bullet.cpp, bullet is propelled in  the rotation and position the entity was spawned with
				pEntity->CreateComponentClass<CBulletComponent>();
			}
		}
	}
	return true;
}

bool CPlayerComponent::TestValues(TestParams&& p, INetChannel*)
{
	CryLogAlways("entity name: %s, channel: %d, entityID: %d", p.name, p.channelID, int(p.entityID));

	return true;
}
