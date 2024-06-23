// Copyright 2017-2021 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once


namespace Cry::DefaultComponents
{
	class CCameraComponent;
	class CInputComponent;
	class CRigidBodyComponent;
}

////////////////////////////////////////////////////////
// Spawn point
////////////////////////////////////////////////////////
class CVehicleComponent final : public IEntityComponent
{
public:
	CVehicleComponent() = default;
	virtual ~CVehicleComponent() = default;

	// Reflect type to set a unique identifier for this component
	// and provide additional information to expose it in the sandbox
	static void ReflectType(Schematyc::CTypeDesc<CVehicleComponent>& desc)
	{
		desc.SetGUID("{DDE72471-EF55-4ABA-B4D0-ECD9D57F4C88}"_cry_guid);
		desc.SetEditorCategory("Flight");
		desc.SetLabel("VehicleComponent");
		desc.SetDescription("Turns the entity into a vehicle that can be entered. No Flight Logic.");
	}
	virtual void ProcessEvent(const SEntityEvent& event) override;
	virtual void Initialize() override;
	virtual Cry::Entity::EventFlags GetEventMask() const override;

	// Input states
	float GetMoveFwdAxisVal() const { return m_fwdAxisVal; };
	float GetMoveBwdAxisVal() const { return m_bwdAxisVal; };

	int IsKeyPressed(const std::string& actionName);
	float GetAxisValue(const std::string& axisName);

protected:
private:
	// Default Components
	Cry::DefaultComponents::CCameraComponent* m_pCameraComponent;
	Cry::DefaultComponents::CInputComponent* m_pInputComponent;
	Cry::DefaultComponents::CRigidBodyComponent* m_pRigidBodyComponent;

	// Flight Inputs
	void InitializeInput();
	bool fps_use_ship();


	// Variables
	bool hasGameStarted = false;
	float m_fwdAxisVal = 0.f;
	float m_bwdAxisVal = 0.f;

	//Input maps
	std::map<std::string, float> m_axisValues;
	std::map<std::string, int> m_keyStates;

	// Get CVar value
	int GetFpsUseShip();

	// Ref flight controller
	CFlightController* m_pFlightController;
};
