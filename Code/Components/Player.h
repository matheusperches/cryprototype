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
		desc.SetLabel("MainPlayerComponent");
		desc.SetDescription("PLayer Component. There should be only one entity containing this component.");

		// First Person Parameters
		desc.AddMember(&CPlayerComponent::m_walkSpeed, 'pws', "playerwalkspeed", "Player Walk Speed", "Sets the player Walk speed", ZERO);
		desc.AddMember(&CPlayerComponent::m_runSpeed, 'prs', "playerrunspeed", "Player Run Speed", "Sets the player Run speed", ZERO);
		desc.AddMember(&CPlayerComponent::m_rotationSpeed, 'pros', "playerrotationspeed", "Player Rotation Speed", "Sets the player rotation speed", ZERO);
		desc.AddMember(&CPlayerComponent::m_jumpHeight, 'pjh', "playerjumpheight", "Player Jump Height", "Sets the player jump height", ZERO);
		desc.AddMember(&CPlayerComponent::m_cameraDefaultPos, 'cdp', "cameradefaultpos", "Camera Default Position", "Sets the camera default position", ZERO);
		desc.AddMember(&CPlayerComponent::m_rotationLimitsMaxPitch, 'cpm', "camerapitchmax", "Camera Pitch Max", "Maximum rotation value for camera pitch", -0.85f);
		desc.AddMember(&CPlayerComponent::m_rotationLimitsMinPitch, 'cpmi', "camerapitchmin", "Camera Pitch Min", "Minimum rotation value for camera pitch", 1.5f);
		desc.AddMember(&CPlayerComponent::m_InteractionDistance, 'pir', "playerinteractionrange", "Player Interaction Range", "Range or distance for the raycast", 10.0f);
		desc.AddMember(&CPlayerComponent::shouldStartOnVehicle, 'ssv', "shouldstartonvehicle", "Player Should Start On Vehicle", "Determines if the game starts directly on the ship or not.", false);
	}

protected: 

private: 

	Cry::DefaultComponents::CCameraComponent* m_pCameraComponent;
	Cry::DefaultComponents::CInputComponent* m_pInputComponent;
	Cry::DefaultComponents::CCharacterControllerComponent* m_pCharacterController;
	Cry::DefaultComponents::CAdvancedAnimationComponent* m_pAdvancedAnimationComponent;

	// Inputs 
	void InitializeHumanInput();
	bool m_isInteractPressed = false; // Flag to track the input state

	// Human Actions
	void PlayerMovement();
	void CameraMovement();
	void Interact(int activationMode, float value);

	// Raycasting for interactions
	IEntity* RayCast(Vec3 origin, Quat dir, IEntity& pSkipEntity);

	// Player Control 

	Quat m_lookOrientation;
	
	Vec3 m_cameraDefaultPos;

	Vec2 m_movementDelta;
	Vec2 m_mouseDeltaRotation;

	float m_rotationSpeed;
	float m_walkSpeed;
	float m_runSpeed;
	float m_jumpHeight;

	float m_rotationLimitsMaxPitch;
	float m_rotationLimitsMinPitch;
	float m_InteractionDistance;

	const char* pClassName = "";

	bool hasGameStarted = false;
	bool shouldStartOnVehicle = false;

	EPlayerState m_currentPlayerState;

};