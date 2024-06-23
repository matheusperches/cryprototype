// Copyright 2017-2021 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "ShipThrusterComponent.h"
#include "ThrusterParams.h"

namespace Cry::DefaultComponents
{
	class CInputComponent;
	class CRigidBodyComponent;
	class CVehicleComponent;
	class CThrusterComponent;
}

enum class EFlightMode
{
	Coupled,
	Decoupled,
};

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
		desc.SetDescription("Executes flight calculations.");
		desc.AddMember(&CFlightController::fwdThrust, 'fwax', "forwardaxis", "Forward Axis thrust", "Point force generator on the forward axis", ZERO);
	}
	virtual void ProcessEvent(const SEntityEvent& event) override;
	virtual void Initialize() override;
	virtual Cry::Entity::EventFlags GetEventMask() const override;

protected:
private:
	// Default Components
	Cry::DefaultComponents::CInputComponent* m_pInputComponent;
	Cry::DefaultComponents::CRigidBodyComponent* m_pRigidBodyComponent;
	Cry::DefaultComponents::CThrusterComponent* m_pThrusterComponent;

	// Variables
	bool hasGameStarted = false;
	EFlightMode m_pFlightControllerState;

	// struct to combine thruster axis and actuation, simplifying code
	struct AxisThrustParams {
		std::string axisName;
		Vec3 direction;
		float thrustAmount;
	};

	// List to receive the thrust directions dinamically
	// Remmeber to add the rest of the variables
	std::vector<AxisThrustParams> axisThrusterParamsList = {
		{"accelforward", Vec3(0.f, 0.f, 1.f), fwdThrust},
		{"accelbackward", Vec3(0.f,0.f,-1.f), 0.f},
		{"accelright", Vec3(1.f, 0.f, 0.f), 0.f},
		{"accelleft", Vec3(-1.f, 0.f, 0.f), 0.f},
		{"accelup", Vec3(0.f, 1.f, 0.f), 0.f},
		{"acceldown", Vec3(0.f,-1.f,0.f), 0.f},
	};

	// Receive the input map from VehicleComponent
	void GetVehicleInputManager();

	// Validator functions checking if the code can be run properly.
	bool Validator();

	// Getting the key states and axis values from the Vehicle
	bool IsKeyPressed(const std::string& actionName);
	float AxisGetter(const std::string& axisName);

	// Calculate the thrust direction
	Vec3 CalculateThrustDirection();


	// Flight Behavior
	void CalculateThrust();

	// Performance variables
	float fwdThrust = 0.f;

	// Thruster Reference
	CShipThrusterComponent* m_pShipThrusterComponent = nullptr;
};