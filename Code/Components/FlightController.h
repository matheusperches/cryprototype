// Copyright 2017-2021 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Cry::DefaultComponents
{
	class CCameraComponent;
	class CInputComponent;
	class CCharacterControllerComponent;
	class CAdvancedAnimationComponent;
	class CRigidBodyComponent;
}

enum class EFlightControllerState
{
	Boosting,
	Coupled,
	Decoupled,

};
////////////////////////////////////////////////////////
// Spawn point
////////////////////////////////////////////////////////
class CFlightController final : public IEntityComponent
{
public:
	CFlightController() = default;
	virtual ~CFlightController() = default;

	// Reflect type to set a unique identifier for this component
	// and provide additional information to expose it in the sandbox
	static void ReflectType(Schematyc::CTypeDesc<CFlightController>& desc)
	{
		desc.SetGUID("{52D14ABC-6525-4408-BBFD-D55B6710ECAB}"_cry_guid);
		desc.SetEditorCategory("Flight");
		desc.SetLabel("FlightController");
		desc.SetDescription("Contains flight logic only.");
		desc.AddMember(&CFlightController::fwdThrust, 'fwax', "forwardaxis", "Forward Axis thrust", "Point force generator on the forward axis", ZERO);
	}
	virtual void ProcessEvent(const SEntityEvent& event) override;
	virtual void Initialize() override;
	virtual Cry::Entity::EventFlags GetEventMask() const override;


	float m_force = 0.0f;

protected:
private:
	// Default Components
	Cry::DefaultComponents::CCameraComponent* m_pCameraComponent;
	Cry::DefaultComponents::CInputComponent* m_pInputComponent;
	Cry::DefaultComponents::CRigidBodyComponent* m_pRigidBodyComponent;

	// Variables
	bool hasGameStarted = false;

	// Receive the input map from VehicleComponent
	void GetVehicleInputManager();

	// Validator functions checking if the code can be run properly.
	bool Validator();
	bool IsKeyPressed(const std::string& actionName);
	float AxisGetter(const std::string& axisName);


	// Flight Behavior
	void CreateThrust();

	// Performance variables
	float fwdThrust = 0.f;
};
