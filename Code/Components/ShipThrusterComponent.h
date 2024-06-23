// Copyright 2017-2021 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <CryPhysics/IPhysics.h>
#include <CryPhysics/physinterface.h>
#include "ThrusterParams.h"

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
		desc.SetEditorCategory("Flight");
		desc.SetLabel("Thruster");
		desc.SetComponentFlags({ IEntityComponent::EFlags::HideFromInspector });
		desc.SetDescription("Contains thruster logic only.");
	}
	virtual void ProcessEvent(const SEntityEvent& event) override;
	virtual void Initialize() override;
	virtual Cry::Entity::EventFlags GetEventMask() const override;

	// Get current thruster state
	EShipThrusterState GetThrusterState()
	{
		return m_currentThrusterState;
	};

	// Flight Behavior
	void ApplyThrust(IPhysicalEntity* pPhysicalEntity, ThrusterParams tParams);

protected:
private:
	// Default Components
	Cry::DefaultComponents::CRigidBodyComponent* m_pRigidBodyComponent;

	// Variables
	bool hasGameStarted = false;
	pe_action_impulse impulseAction;

	// Validator functions checking if the code can be run properly.
	bool Validator();

	// Thruster state
	EShipThrusterState m_currentThrusterState;
};
