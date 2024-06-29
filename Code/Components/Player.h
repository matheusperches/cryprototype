// Copyright 2017-2019 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <array>
#include <numeric>

#include <ICryMannequin.h>
#include <CryMath/Cry_Camera.h>


namespace Cry::DefaultComponents
{
	class CCameraComponent;
	class CInputComponent;
	class CCharacterControllerComponent;
	class CAdvancedAnimationComponent;
}

namespace Cry::Audio::DefaultComponents
{
	class CListenerComponent;
}


////////////////////////////////////////////////////////
// Represents a player participating in gameplay
////////////////////////////////////////////////////////
class CPlayerComponent final : public IEntityComponent
{
	enum class EPlayerState
	{
		Walking,
		Sprinting
	};

	enum class EInputFlagType
	{
		Hold = 0,
		Toggle
	};

	enum class EInputFlag : uint8
	{
		MoveLeft = 1 << 0,
		MoveRight = 1 << 1,
		MoveForward = 1 << 2,
		MoveBack = 1 << 3,
		Jump = 1 << 4,
		Sprint = 1 << 5,
		Interact = 1 << 6
	};

	static constexpr EEntityAspects InputAspect = eEA_GameClientD;

	template<typename T, size_t SAMPLES_COUNT>
	class MovingAverage
	{
		static_assert(SAMPLES_COUNT > 0, "SAMPLES_COUNT shall be larger than zero!");

	public:

		MovingAverage()
			: m_values()
			, m_cursor(SAMPLES_COUNT)
			, m_accumulator()
		{
		}

		MovingAverage& Push(const T& value)
		{
			if (m_cursor == SAMPLES_COUNT)
			{
				m_values.fill(value);
				m_cursor = 0;
				m_accumulator = std::accumulate(m_values.begin(), m_values.end(), T(0));
			}
			else
			{
				m_accumulator -= m_values[m_cursor];
				m_values[m_cursor] = value;
				m_accumulator += m_values[m_cursor];
				m_cursor = (m_cursor + 1) % SAMPLES_COUNT;
			}

			return *this;
		}

		T Get() const
		{
			return m_accumulator / T(SAMPLES_COUNT);
		}

		void Reset()
		{
			m_cursor = SAMPLES_COUNT;
		}

	private:

		std::array<T, SAMPLES_COUNT> m_values;
		size_t m_cursor;

		T m_accumulator;
	};

	struct NoParams
	{
		void SerializeWith(TSerialize ser)
		{

		}
	};

	struct TestParams
	{
		string name; 
		int channelID; 
		EntityId entityID;
		void SerializeWith(TSerialize ser)
		{
			ser.Value("name", name, 'stab');
			ser.Value("channelID", channelID);
			ser.Value("entityID", entityID);
		}
	};

public:
	CPlayerComponent() = default;
	virtual ~CPlayerComponent() = default;

	// IEntityComponent
	virtual void Initialize() override;

	virtual Cry::Entity::EventFlags GetEventMask() const override;

	virtual void ProcessEvent(const SEntityEvent& event) override;

	virtual bool NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags) override;

	virtual NetworkAspectType GetNetSerializeAspectMask() const override { return InputAspect; }

	bool ServerRequestFire(NoParams&& p, INetChannel*);
	bool ClientFire(NoParams&& p, INetChannel*);
	bool TestValues(TestParams&& p, INetChannel*);

	// Reflect type to set a unique identifier for this component
	static void ReflectType(Schematyc::CTypeDesc<CPlayerComponent>& desc)
	{
		desc.SetGUID("{63F4C0C6-32AF-4ACB-8FB0-57D45DD14725}"_cry_guid);
		/*
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
		desc.AddMember(&CPlayerComponent::m_shouldStartOnVehicle, 'stv', "shouldstartonvehicle", "Player Should Start On Vehicle", "Determines if the game starts directly on the ship or not.", false);
		*/
	}

	void OnReadyForGameplayOnServer();
	bool IsLocalClient() const { return (m_pEntity->GetFlags() & ENTITY_FLAG_LOCAL_PLAYER) != 0; }

	// Checking if we should listen to inputs
	bool isActiveEntity = false;

protected: 

	// Functions
	void InitializePilotInput();
	void UpdatePlayerMovementRequest(float frameTime);
	void UpdateLookDirectionRequest(float frameTime);
	void UpdateAnimation(float frameTime);
	void UpdateCamera(float frameTime);
	void Interact(int activationMode);
	void HandleInputFlagChange(CEnumFlags<EInputFlag> flags, CEnumFlags<EActionActivationMode> activationMode, EInputFlagType type = EInputFlagType::Hold);

	// Respawn
	void Revive(const Matrix34& transform);
	// Called when this entity becomes the local player, to create client specific setup such as the Camera
	void InitializeLocalPlayer();

protected:
	// Parameters to be passed to the RemoteReviveOnClient function
	struct RemoteReviveParams
	{
		// Called once on the server to serialize data to the other clients
		// Then called once on the other side to deserialize
		void SerializeWith(TSerialize ser)
		{
			// Serialize the position with the 'wrld' compression policy
			ser.Value("pos", position, 'wrld');
			// Serialize the rotation with the 'ori0' compression policy
			ser.Value("rot", rotation, 'ori0');
		}

		Vec3 position;
		Quat rotation;
	};
	// Remote method intended to be called on all remote clients when a player spawns on the server
	bool RemoteReviveOnClient(RemoteReviveParams&& params, INetChannel* pNetChannel);

protected: 

	bool m_isAlive = false;
	int m_interactActivationMode;

	// Components
	Cry::DefaultComponents::CCameraComponent* m_pCameraComponent = nullptr;
	Cry::DefaultComponents::CInputComponent* m_pInputComponent = nullptr;
	Cry::DefaultComponents::CCharacterControllerComponent* m_pCharacterController = nullptr;
	Cry::DefaultComponents::CAdvancedAnimationComponent* m_pAdvancedAnimationComponent = nullptr;
	Cry::Audio::DefaultComponents::CListenerComponent* m_pAudioListenerComponent = nullptr;

	FragmentID m_activeFragmentId;
	FragmentID m_idleFragmentId;
	FragmentID m_walkFragmentId;
	TagID m_rotateTagId;

	// Pilot variables
	Quat m_lookOrientation = ZERO;
	Vec3 m_cameraDefaultPos = Vec3(0.f, 0.f, 1.75f);
	Vec2 m_movementDelta = ZERO;
	Vec2 m_mouseDeltaRotation = ZERO;

	const float m_cameraPitchMax = 1.5f; 
	const float m_cameraPitchMin = -1.2f;
	float m_playerInteractionRange = 10.f;
	float m_walkSpeed = 4.f;
	float m_runSpeed = 7.f;
	float m_jumpHeight = 2.f;

	bool hasGameStarted = false;
	bool m_shouldStartOnVehicle = false;
	bool m_isInteractPressed = false;

	int m_cameraJointId = -1;

	CEnumFlags<EInputFlag> m_inputFlags;
	MovingAverage<Vec2, 10> m_mouseDeltaSmoothingFilter;

	EPlayerState m_currentPlayerState;

	float m_horizontalAngularVelocity;
	MovingAverage<float, 10> m_averagedHorizontalAngularVelocity;

	const float m_rotationSpeed = 0.002f;

	// Get CVar value
	bool GetIsPiloting();

	// Raycasting for interactions
	IEntity* RayCast(Vec3 origin, Quat dir, IEntity& pSkipEntity) const;
};