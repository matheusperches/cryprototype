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

enum class AccelState
{
	Idle,
	Accelerating,
	Decelerating
};

struct AccelerationData
{
	float Jerk;
	float JerkDecelRate;
	Vec3 currentAccel;
	Vec3 targetAccel;
	AccelState state;
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
		desc.AddMember(&CFlightController::fwdAccel, 'fwdr', "forwarddir", "Forward Direction Acceleration", "Point force generator on the forward direction", ZERO);
		desc.AddMember(&CFlightController::bwdAccel, 'bwdr', "backwarddir", "Backward Direction Acceleration", "Point force generator on the backward direction", ZERO);
		desc.AddMember(&CFlightController::leftRightAccel, 'lrdr', "leftright", "Left/Right Acceleration", "Point force generator on the left direction", ZERO);
		desc.AddMember(&CFlightController::upDownAccel, 'uddr', "updown", "Up/Down Direction Acceleration", "Point force generator on the up / down direction", ZERO);
		desc.AddMember(&CFlightController::rollAccel, 'roll', "rollaccel", "Roll Acceleration", "Point force generator for rolling", ZERO);
		desc.AddMember(&CFlightController::pitchAccel, 'ptch', "pitch", "Pitch Acceleration", "Point force generator for pitching", ZERO);
		desc.AddMember(&CFlightController::yawAccel, 'yaw', "yaw", "Yaw Acceleration", "Point force generator for yawing", ZERO);
		desc.AddMember(&CFlightController::maxRoll, 'mxrl', "maxRoll", "Max Roll Rate", "Max roll rate in degrees / sec", ZERO);
		desc.AddMember(&CFlightController::maxPitch, 'mxpt', "maxPitch", "Max Pitch Rate", "Max pitch rate in degrees / sec", ZERO);
		desc.AddMember(&CFlightController::maxYaw, 'mxyw', "maxYaw", "Max Yaw Rate", "Max yaw rate in degrees / sec", ZERO);
		desc.AddMember(&CFlightController::mouseSenseFactor, 'msf', "mouseSensFact", "Mouse Sensitivity Factor", "Adjusts mouse sensitivity", ZERO);
		desc.AddMember(&CFlightController::linearJerkRate, 'lnjk', "linearjerk", "Linear Jerk", "Adjusts the force smoothing rate", ZERO);
		desc.AddMember(&CFlightController::linearJerkDecelRate, 'lndr', "linearjerkdecel", "Linear Jerk decel rate", "Adjusts the decel rate", ZERO);
		desc.AddMember(&CFlightController::RollJerkRate, 'rljk', "rolljerkrate", "Roll Jerk", "Adjusts the force smoothing rate", ZERO);
		desc.AddMember(&CFlightController::RollJerkDecelRate, 'rldr', "rolljerkdecel", "Roll Jerk decel rate", "Adjusts the decel rate", ZERO);
		desc.AddMember(&CFlightController::PitchYawJerkRate, 'pyjk', "pyjerkrate", "Pitch/Yaw Jerk", "Adjusts the force smoothing rate", ZERO);
		desc.AddMember(&CFlightController::PitchYawJerkDecelRate, 'pydr', "pyjerkdecel", "Pitch/Yaw Jerk decel rate", "Adjusts the decel rate", ZERO);
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
	struct AxisAccelParams {
		std::string axisName;
		float AccelAmount;
	};
	// Defining a list to receive the thrust directions dinamically
	std::vector<AxisAccelParams> LinearAxisAccelParamsList = {};
	std::vector<AxisAccelParams> RollAxisAccelParamsList = {};
	std::vector<AxisAccelParams> PitchYawAxisAccelParamsList = {};

	// Axis Vector initializer
	void InitializeAccelParamsVectors();

	// AccelerationData struct jerk initializer
	void InitializeJerkParams();

	// Receive the input map from VehicleComponent
	void GetVehicleInputManager();

	// Validator functions checking if the code can be run properly.
	bool Validator();

	// Getting the key states from the Vehicle
	bool IsKeyPressed(const std::string& actionName);
	// Getting the Axis values from the Vehicle
	float AxisGetter(const std::string& axisName);

	// Flight Computations

	// Adjusts the acceleration rate change over time 
	Vec3 UpdateAccelerationWithJerk(AccelerationData& accelData, float jerkRate, float jerkDecelRate, float deltaTime);

	// Scales the acceleration asked, according to input magnitude, taking into account the inputs pressed
	Vec3 ScaleLinearAccel();
	Vec3 ScaleRollAccel();
	Vec3 ScalePitchYawAccel();

	void UpdateAccelerationState(AccelerationData& accelData, const Vec3& targetAccel);

	// Convert rotation parameters to radians 
	float DegreesToRadian(float degrees);

	// Convert world coordinates to local coordinates
	Vec3 WorldToLocal(const Vec3& localDirection);

	float NormalizeInput(float inputValue, bool isMouse = false);

	// Calculates the thrust force in Vec3, containing direction.
	Vec3 AccelToImpulse(Vec3 desiredAccel);  

	// Apply the calculations
	void ProcessFlight();

	// Performance variables
	float fwdAccel = 0.f;
	float bwdAccel = 0.f;
	float leftRightAccel = 0.f;
	float upDownAccel = 0.f;
	float rollAccel = 0.f;
	float pitchAccel = 0.f;
	float yawAccel = 0.f;
	float maxRoll = 0.f;
	float maxPitch = 0.f;
	float maxYaw = 0.f;
	float mouseSenseFactor = 0.f;
	float linearJerkRate = 0.f;
	float linearJerkDecelRate = 0.f;
	float RollJerkRate = 0.f;
	float RollJerkDecelRate = 0.f;
	float PitchYawJerkRate = 0.f;
	float PitchYawJerkDecelRate = 0.f;

	// Acceleration data trackers
	Vec3 targetLinearAccel = Vec3(0.f, 0.f, 0.f);
	Vec3 currentAngularAccel = Vec3(0.f, 0.f, 0.f);
	Vec3 targetAngularAccel = Vec3(0.f, 0.f, 0.f);

	// Accel data structs. Jerk values set in Initialize() retrieved from AddMember

	AccelerationData linearAccelData;
	AccelerationData rollAccelData;
	AccelerationData pitchYawAccelData;
	

	//Debug color
	float color[4] = { 1, 0, 0, 1 };

	const float MAX_INPUT_VALUE = 1.f; // Maximum clamped input value
	const float MIN_INPUT_VALUE = -1.f; // Maximum clamped input value

	// Thruster Reference
	CShipThrusterComponent* m_pShipThrusterComponent = nullptr;

	// Physical Entity reference
	IPhysicalEntity* physEntity = nullptr;
};