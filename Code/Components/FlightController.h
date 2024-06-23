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
		desc.AddMember(&CFlightController::fwdAccel, 'fwdr', "forwarddir", "Forward direction acceleration", "Point force generator on the forward direction", ZERO);
		desc.AddMember(&CFlightController::bwdAccel, 'bwdr', "backwarddir", "Backward direction acceleration", "Point force generator on the backward direction", ZERO);
		desc.AddMember(&CFlightController::leftAccel, 'lfdr', "leftdir", "Left direction acceleration", "Point force generator on the left direction", ZERO);
		desc.AddMember(&CFlightController::rightAccel, 'rtdr', "rightdir", "Right direction acceleration", "Point force generator on the right direction", ZERO);
		desc.AddMember(&CFlightController::upAccel, 'updr', "updir", "Upward direction acceleration", "Point force generator on the upward direction", ZERO);
		desc.AddMember(&CFlightController::downAccel, 'dndr', "downdir", "Downward direction acceleration", "Point force generator on the downards direction", ZERO);
		desc.AddMember(&CFlightController::rollAccel, 'rlac', "rollaccel", "Roll acceleration", "Point force generator for rolling", ZERO);
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
		float AccelAmount;
	};
	// Defining a list to receive the thrust directions dinamically
	std::vector<AxisThrustParams> axisThrusterParamsList = {};

	// Axis Vector initializer
	void InitializeAxisThrustParamsVector();

	// Receive the input map from VehicleComponent
	void GetVehicleInputManager();

	// Validator functions checking if the code can be run properly.
	bool Validator();

	// Getting the key states from the Vehicle
	bool IsKeyPressed(const std::string& actionName);
	// Getting the Axis values from the Vehicle
	float AxisGetter(const std::string& axisName);

	// Scales the acceleration asked, according to input magnitude, taking into account the inputs pressed
	Vec3 CalculateAccelDirectionAndScale();

	Vec3 CalculateThrust(Vec3 desiredAccel); // Calculates the thrust force in Vec3, containing direction. 

	// Execute the calculations
	void ProcessFlight();

	// Performance variables
	float fwdAccel = 0.f;
	float bwdAccel = 0.f;
	float leftAccel = 0.f;
	float rightAccel = 0.f;
	float upAccel = 0.f;
	float downAccel = 0.f;
	float rollAccel = 0.f;

	// Thruster Reference
	CShipThrusterComponent* m_pShipThrusterComponent = nullptr;

	// Physical Entity reference
	IPhysicalEntity* physEntity;
};