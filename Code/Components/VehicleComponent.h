// Copyright 2017-2021 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once
class CFlightController;

namespace Cry::DefaultComponents
{
	class CCameraComponent;
	class CInputComponent;
	class CRigidBodyComponent;
}

namespace CustomComponents
{
	class CFlightController;
}

////////////////////////////////////////////////////////
// Spawn point
////////////////////////////////////////////////////////
class CVehicleComponent final : public IEntityComponent
{
	static constexpr EEntityAspects vehicle_aspect = eEA_GameClientA;
public:


	CVehicleComponent() = default;
	virtual ~CVehicleComponent() = default;

	virtual void Initialize() override;

	virtual Cry::Entity::EventFlags GetEventMask() const override;

	virtual void ProcessEvent(const SEntityEvent& event) override;

	virtual bool NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags) override;

	virtual NetworkAspectType GetNetSerializeAspectMask() const override { return vehicle_aspect; }

	// Reflect type to set a unique identifier for this component
	// and provide additional information to expose it in the sandbox
	static void ReflectType(Schematyc::CTypeDesc<CVehicleComponent>& desc)
	{
		desc.SetGUID("{DDE72471-EF55-4ABA-B4D0-ECD9D57F4C88}"_cry_guid);
		desc.SetEditorCategory("Flight");
		desc.SetLabel("VehicleComponent");
		desc.SetDescription("Turns the entity into a vehicle that can be entered. No Flight Logic.");
	}

	// Get if we have a pilot onboard
	bool GetIsPiloting();
	IEntity* GetPlayerComponent();
	IEntity* m_pPlayerComponent = nullptr;

protected:
private:
	// Default Components
	Cry::DefaultComponents::CCameraComponent* m_pCameraComponent;
	Cry::DefaultComponents::CInputComponent* m_pInputComponent;
	Cry::DefaultComponents::CRigidBodyComponent* m_pRigidBodyComponent;

	Vec3 m_position = ZERO;
	Quat m_rotation = ZERO;
	Vec3 m_velocity = ZERO;

	// Ref flight controller
	CFlightController* m_pFlightController;

	// Variables
	bool m_hasGameStarted = false;

	//Input maps
	VectorMap<string, float> m_axisValues;
	VectorMap<string, int> m_keyStates;

	//Ship's orientation 
	Quat m_shipLookOrientation = ZERO;

	// Get the pilot entity ID
	EntityId pilotID = NULL; 
};
