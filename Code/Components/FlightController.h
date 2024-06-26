// Copyright 2017-2021 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once
#include "ShipThrusterComponent.h"
#include "ThrusterParams.h"


namespace Cry::DefaultComponents
{
	class CInputComponent;
	class CRigidBodyComponent;
	class CVehicleComponent;
}

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
		desc.AddMember(&CFlightController::m_fwdAccel, 'fwdr', "forwarddir", "Forward Direction Acceleration", "Point force generator on the forward direction", ZERO);
		desc.AddMember(&CFlightController::m_bwdAccel, 'bwdr', "backwarddir", "Backward Direction Acceleration", "Point force generator on the backward direction", ZERO);
		desc.AddMember(&CFlightController::m_leftRightAccel, 'lrdr', "leftright", "Left/Right Acceleration", "Point force generator on the left direction", ZERO);
		desc.AddMember(&CFlightController::m_upDownAccel, 'uddr', "updown", "Up/Down Direction Acceleration", "Point force generator on the up / down direction", ZERO);
		desc.AddMember(&CFlightController::m_rollAccel, 'roll', "rollaccel", "Roll Acceleration", "Point force generator for rolling", ZERO);
		desc.AddMember(&CFlightController::m_pitchAccel, 'ptch', "pitch", "Pitch Acceleration", "Point force generator for pitching", ZERO);
		desc.AddMember(&CFlightController::m_yawAccel, 'yaw', "yaw", "Yaw Acceleration", "Point force generator for yawing", ZERO);
		desc.AddMember(&CFlightController::m_maxRoll, 'mxrl', "maxRoll", "Max Roll Rate", "Max roll rate in degrees / sec", ZERO);
		desc.AddMember(&CFlightController::m_maxPitch, 'mxpt', "maxPitch", "Max Pitch Rate", "Max pitch rate in degrees / sec", ZERO);
		desc.AddMember(&CFlightController::m_maxYaw, 'mxyw', "maxYaw", "Max Yaw Rate", "Max yaw rate in degrees / sec", ZERO);
		desc.AddMember(&CFlightController::m_mouseSenseFactor, 'msf', "mouseSensFact", "Mouse Sensitivity Factor", "Adjusts mouse sensitivity", ZERO);
		desc.AddMember(&CFlightController::m_linearJerkRate, 'lnjk', "linearjerk", "Linear Jerk", "Adjusts the force smoothing rate", ZERO);
		desc.AddMember(&CFlightController::m_linearJerkDecelRate, 'lndr', "linearjerkdecel", "Linear Jerk decel rate", "Adjusts the decel rate", ZERO);
		desc.AddMember(&CFlightController::m_RollJerkRate, 'rljk', "rolljerkrate", "Roll Jerk", "Adjusts the force smoothing rate", ZERO);
		desc.AddMember(&CFlightController::m_RollJerkDecelRate, 'rldr', "rolljerkdecel", "Roll Jerk decel rate", "Adjusts the decel rate", ZERO);
		desc.AddMember(&CFlightController::m_PitchYawJerkRate, 'pyjk', "pyjerkrate", "Pitch/Yaw Jerk", "Adjusts the force smoothing rate", ZERO);
		desc.AddMember(&CFlightController::m_PitchYawJerkDecelRate, 'pydr', "pyjerkdecel", "Pitch/Yaw Jerk decel rate", "Adjusts the decel rate", ZERO);
	}
	virtual void ProcessEvent(const SEntityEvent& event) override;
	virtual void Initialize() override;
	virtual Cry::Entity::EventFlags GetEventMask() const override;


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

protected:
private:
	// Default Components
	Cry::DefaultComponents::CInputComponent* m_pInputComponent;
	Cry::DefaultComponents::CRigidBodyComponent* m_pRigidBodyComponent;

	// Custom Components
	CShipThrusterComponent* m_pShipThrusterComponent;

	const float m_MAX_INPUT_VALUE = 1.f; // Maximum clamped input value
	const float m_MIN_INPUT_VALUE = -1.f; // Maximum clamped input value
	
	// Game State
	bool hasGameStarted = false;
	EFlightMode m_pFlightMode;

	// Physical Entity reference
	IPhysicalEntity* m_physEntity = nullptr;

	//Debug color
	float m_debugColor[4] = { 1, 0, 0, 1 };

	// Accel data structures. Jerk values for each are set in Initialize(), retrieved from AddMember listings above.
	struct JerkAccelerationData
	{
		float jerk;
		float jerkDecelRate;
		Vec3 currentJerkAccel;
		Vec3 targetJerkAccel;
		AccelState state;

		JerkAccelerationData() : jerk(0), jerkDecelRate(0), currentJerkAccel(0), targetJerkAccel(0), state(AccelState::Idle) {}
	};

	JerkAccelerationData m_linearAccelData = {};
	JerkAccelerationData m_rollAccelData = {};
	JerkAccelerationData m_pitchYawAccelData = {};

	struct VelocityData
	{
		float currentVelocity;
		float previousVelocity;

		VelocityData() : currentVelocity(0), previousVelocity(0) {}
	};

	VelocityData m_shipVelocity = {};

	// Performance variables
	float m_fwdAccel = 0.f;
	float m_bwdAccel = 0.f;
	float m_leftRightAccel = 0.f;
	float m_upDownAccel = 0.f;
	float m_rollAccel = 0.f;
	float m_pitchAccel = 0.f;
	float m_yawAccel = 0.f;
	float m_maxRoll = 0.f;
	float m_maxPitch = 0.f;
	float m_maxYaw = 0.f;
	float m_mouseSenseFactor = 0.f;
	float m_linearJerkRate = 0.f;
	float m_linearJerkDecelRate = 0.f;
	float m_RollJerkRate = 0.f;
	float m_RollJerkDecelRate = 0.f;
	float m_PitchYawJerkRate = 0.f;
	float m_PitchYawJerkDecelRate = 0.f;

	// Retrieving the dynamics of our entity
	pe_status_dynamics dynamics;


	// struct to combine thruster axis and actuation
	struct AxisAccelParams {
		string axisName;
		float AccelAmount;
	};

	// List arrays to receive the thrust directions dinamically
	DynArray<AxisAccelParams> m_LinearAxisAccelParamsList = {};
	DynArray<AxisAccelParams> m_RollAxisAccelParamsList = {};
	DynArray<AxisAccelParams> m_PitchYawAxisAccelParamsList = {};

	// Axis Vector initializer
	void InitializeAccelParamsVectors();
	// Jerk data initializer
	void InitializeJerkParams();
	// Reset the jerk values 
	void ResetJerkParams();
	// Receive the input manager from VehicleComponent
	void GetVehicleInputManager();
	// Getting the key states from the Vehicle
	bool IsKeyPressed(const string& actionName);
	// Getting the Axis values from the Vehicle
	float AxisGetter(const string& axisName);

	// Validator function checking if the code can be run properly.
	bool Validator();

	// Compares the current vs target accel, and calculates the jerk
	Vec3 UpdateAccelerationWithJerk(JerkAccelerationData& accelData, float jerkRate, float jerkDecelRate, float deltaTime);

	// Scales the acceleration asked, according to input magnitude, taking into account the inputs pressed
	Vec3 ScaleLinearAccel();
	Vec3 ScaleRollAccel();
	Vec3 ScalePitchYawAccel();

	// Updates the enum class "AccelState" according to our ship state.
	void UpdateAccelerationState(JerkAccelerationData& accelData, const Vec3& targetAccel);

	// Utility functions
	float DegreesToRadian(float degrees);
	Vec3 GetVelocity();
	float GetAcceleration();

	// Convert world coordinates to local coordinates
	Vec3 ImpulseWorldToLocal(const Vec3& localDirection);
	float NormalizeInput(float inputValue, bool isMouse = false);

	// Converts the accel target (after jerk) which contains both direction and magnitude, into thrust values.
	Vec3 AccelToImpulse(Vec3 desiredAccel);  

	// Sequence all the function calls to produce our movement		
	void ProcessFlight();
};