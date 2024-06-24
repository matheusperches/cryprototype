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
	Newtonian,
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
		desc.AddMember(&CFlightController::rollAccel, 'roll', "rollaccel", "Roll acceleration", "Point force generator for rolling", ZERO);
		desc.AddMember(&CFlightController::pitchAccel, 'ptch', "pitch", "Pitch acceleration", "Point force generator for pitching", ZERO);
		desc.AddMember(&CFlightController::yawAccel, 'yaw', "yaw", "Yaw acceleration", "Point force generator for yawing", ZERO);
		desc.AddMember(&CFlightController::maxRoll, 'mxrl', "maxRoll", "Max Roll rate", "Max roll rate in degrees / sec", ZERO);
		desc.AddMember(&CFlightController::maxPitch, 'mxpt', "maxPitch", "Max Pitch rate", "Max pitch rate in degrees / sec", ZERO);
		desc.AddMember(&CFlightController::maxYaw, 'mxyw', "maxYaw", "Max Yaw rate", "Max yaw rate in degrees / sec", ZERO);
	}
	virtual void ProcessEvent(const SEntityEvent& event) override;
	virtual void Initialize() override;
	virtual Cry::Entity::EventFlags GetEventMask() const override;

protected:
private:
	// Default Components
	Cry::DefaultComponents::CInputComponent* m_pInputComponent;
	Cry::DefaultComponents::CRigidBodyComponent* m_pRigidBodyComponent;

	// Variables
	bool hasGameStarted = false;
	EFlightMode m_pFlightControllerState;

	// struct to combine thruster axis and actuation, simplifying code
	struct LinearAxisAccelParams {
		std::string axisName;
		float AccelAmount;
	};

	struct AngularAxisAccelParams {
		std::string axisName;
		float AccelAmount;
	};
	// Defining a list to receive the thrust directions dinamically
	std::vector<LinearAxisAccelParams> LinearAxisAccelParamsList = {};
	std::vector<AngularAxisAccelParams> RollAxisAccelParamsList = {};
	std::vector<AngularAxisAccelParams> PitchYawAxisAccelParamsList = {};

	// Axis Vector initializer
	void InitializeAccelParamsVectors();

	// Receive the input map from VehicleComponent
	void GetVehicleInputManager();

	// Validator functions checking if the code can be run properly.
	bool Validator();

	// Getting the key states from the Vehicle
	bool IsKeyPressed(const std::string& actionName);
	// Getting the Axis values from the Vehicle
	float AxisGetter(const std::string& axisName);

	// Flight Computations

	// Scales the acceleration asked, according to input magnitude, taking into account the inputs pressed
	Vec3 ScaleLinearAccel();
	Vec3 ScaleRollAccel();
	Vec3 ScalePitchYawAccel();

	// Convert rotation parameters to radians 
	float DegreesToRadian(float degrees);

	Vec3 WorldToLocal(const Vec3& localDirection);

	// Calculates the thrust force in Vec3, containing direction.
	Vec3 AccelToThrust(Vec3 desiredLinearAccel);  

	// Apply the calculations
	void ProcessFlight();

	// Performance variables
	float fwdAccel = 0.f;
	float bwdAccel = 0.f;
	float leftAccel = 0.f;
	float rightAccel = 0.f;
	float upAccel = 0.f;
	float downAccel = 0.f;
	float rollAccel = 0.f;
	float pitchAccel = 0.f;
	float yawAccel = 0.f;
	float maxRoll = 0.f;
	float maxPitch = 0.f;
	float maxYaw = 0.f;

	// Thruster Reference
	CShipThrusterComponent* m_pShipThrusterComponent = nullptr;

	// Physical Entity reference
	IPhysicalEntity* physEntity = nullptr;
};