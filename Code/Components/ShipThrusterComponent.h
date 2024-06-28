// Copyright 2017-2021 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once
#include <CryPhysics/physinterface.h>


namespace Cry::DefaultComponents
{
	class CRigidBodyComponent;
}

enum class EShipThrusterState
{
	Active,
	Inactive,
	Overclock,
};

class CShipThrusterComponent final : public IEntityComponent
{
public:
	CShipThrusterComponent() = default;
	virtual ~CShipThrusterComponent() = default;

	// Reflect type to set a unique identifier for this component
	// and provide additional information to expose it in the sandbox
	static void ReflectType(Schematyc::CTypeDesc<CShipThrusterComponent>& desc)
	{
		desc.SetGUID("{F0FD2A0A-DD3F-45D5-BBE7-4BCBBBF296BD}"_cry_guid);
		desc.SetComponentFlags({ IEntityComponent::EFlags::HideFromInspector });
	}
	virtual void ProcessEvent(const SEntityEvent& event) override;
	virtual void Initialize() override;
	virtual Cry::Entity::EventFlags GetEventMask() const override;

	// Get CVar value
	bool GetIsPiloting();

	// Get current thruster state
	EShipThrusterState GetThrusterState()
	{
		return m_currentThrusterState;
	};

	// Flight Behavior
	void ApplyLinearImpulse(IPhysicalEntity* pPhysicalEntity, const Vec3& linearImpulse);
	void ApplyAngularImpulse(IPhysicalEntity* pPhysicalEntity, const Vec3& linearImpulse);

	// Adjusts the acceleration rate change over time 
	Vec3 UpdateAccelerationWithJerk(const Vec3& currentAccel, const Vec3& targetAccel, float deltaTime);

protected:
private:
	// Default Components
	Cry::DefaultComponents::CRigidBodyComponent* m_pRigidBodyComponent;


	// Variables
	bool hasGameStarted = false;
	pe_action_impulse impulseAction;
	float jerkRate = 0.f;

	// Validator functions checking if the code can be run properly.
	bool Validator();

	// Thruster state
	EShipThrusterState m_currentThrusterState;

	//Debug color
	float m_debugColor[4] = { 1, 0, 0, 1 };
};
