// Copyright 2017-2019 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

namespace Cry::DefaultComponents
{
	class CCameraComponent;
	class CInputComponent;
	class CCharacterControllerComponent;
	class CAdvancedAnimationComponent;
}

enum class EPlayerState
{
	Walking,
	Sprinting
};


////////////////////////////////////////////////////////
// Represents a player participating in gameplay
////////////////////////////////////////////////////////
class CPlayerComponent final : public IEntityComponent
{

public:
	CPlayerComponent() = default;
	virtual ~CPlayerComponent() = default;

	virtual void Initialize() override;

	virtual Cry::Entity::EventFlags GetEventMask() const override;

	virtual void ProcessEvent(const SEntityEvent& event) override;

	// Reflect type to set a unique identifier for this component
	static void ReflectType(Schematyc::CTypeDesc<CPlayerComponent>& desc)
	{
		desc.SetGUID("{63F4C0C6-32AF-4ACB-8FB0-57D45DD14725}"_cry_guid);
		desc.SetEditorCategory("Pilot");
		desc.SetLabel("Pilot");
		desc.SetDescription("First Person character with movement and raycasting for interaction.");

		// First Person Parameters
		desc.AddMember(&CPlayerComponent::m_walkSpeed, 'pws', "playerwalkspeed", "Player Walk Speed", "Sets the player Walk speed", 4.f);
		desc.AddMember(&CPlayerComponent::m_runSpeed, 'prs', "playerrunspeed", "Player Run Speed", "Sets the player Run speed", 5.f);
		desc.AddMember(&CPlayerComponent::m_rotationSpeed, 'pros', "playerrotationspeed", "Player Rotation Speed", "Sets the player rotation speed", 1.f);
		desc.AddMember(&CPlayerComponent::m_jumpHeight, 'pjh', "playerjumpheight", "Player Jump Height", "Sets the player jump height", 5.f);
		desc.AddMember(&CPlayerComponent::m_cameraDefaultPos, 'cdp', "cameradefaultpos", "Camera Default Position", "Sets the camera default position", Vec3(0.f, 0.f, 1.75f));
		desc.AddMember(&CPlayerComponent::m_playerInteractionRange, 'pir', "playerinteractionrange", "Player Interaction Range", "Range or distance for the raycast", 10.0f);
		desc.AddMember(&CPlayerComponent::shouldStartOnVehicle, 'ssv', "shouldstartonvehicle", "Player Should Start On Vehicle", "Determines if the game starts directly on the ship or not.", false);
	}

	// Checking if we should listen to inputs
	bool isActiveEntity = false;

protected: 

private: 

	// Components
	Cry::DefaultComponents::CCameraComponent* m_pCameraComponent;
	Cry::DefaultComponents::CInputComponent* m_pInputComponent;
	Cry::DefaultComponents::CCharacterControllerComponent* m_pCharacterController;
	Cry::DefaultComponents::CAdvancedAnimationComponent* m_pAdvancedAnimationComponent;

	// Pilot variables
	Quat m_lookOrientation = ZERO;
	Vec3 m_cameraDefaultPos = Vec3(0.f, 0.f, 1.75f);
	Vec2 m_movementDelta = ZERO;
	Vec2 m_mouseDeltaRotation = ZERO;

	const float m_cameraPitchMax = 1.5f; 
	const float m_cameraPitchMin = -1.2f;
	float m_playerInteractionRange = 10.f;
	float m_rotationSpeed = 1.f;
	float m_walkSpeed = 4.f;
	float m_runSpeed = 10.f;
	float m_jumpHeight = 5.f;

	bool hasGameStarted = false;
	bool shouldStartOnVehicle = false;
	bool m_isInteractPressed = false;

	float m_frameTime = 0.f;

	EPlayerState m_currentPlayerState;

	// Functions
	void InitializeHumanInput();
	void PlayerMovement();
	void CameraMovement();
	void Interact(int activationMode, float value);

	// Get CVar value
	bool GetIsPiloting();

	// Raycasting for interactions
	IEntity* RayCast(Vec3 origin, Quat dir, IEntity& pSkipEntity) const;
};