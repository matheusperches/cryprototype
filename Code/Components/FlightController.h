// Copyright 2017-2021 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once
#include <Components/FlightModifiers.h>


class CShipThrusterComponent;
class CVehicleComponent;

namespace Cry::DefaultComponents
{
	class CRigidBodyComponent;
}

class CFlightController final : public IEntityComponent
{
	static constexpr EEntityAspects kVehicleAspect = eEA_GameClientA;

public:
	CFlightController() = default;
	virtual ~CFlightController() = default;

	virtual void Initialize() override;

	virtual Cry::Entity::EventFlags GetEventMask() const override;

	virtual void ProcessEvent(const SEntityEvent& event) override;

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

	virtual bool NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags) override;

	virtual NetworkAspectType GetNetSerializeAspectMask() const override { return kVehicleAspect; }

	// Axis Vector initializer
	void InitializeAccelParamsVectors();
	// Jerk data initializer
	void InitializeJerkParams();
	// Reset the jerk values 
	void ResetJerkParams();

	// Physical Entity reference
	IPhysicalEntity* physEntity = nullptr;

	enum class EAccelState
	{
		Idle,
		Accelerating,
		Decelerating
	};

protected:
private:

	struct SerializeImpulseData
	{
		Vec3 position = ZERO;        // Position of the entity
		Quat orientation = ZERO;     // Orientation of the entity
		Vec3 linearImpulse = ZERO;   // Linear impulse to be applied
		Vec3 rollImpulse = ZERO;  // Angular impulse to be applied
		Vec3 pitchYawImpulse = ZERO;  // Angular impulse to be applied

		void SerializeWith(TSerialize ser)
		{
			ser.Value("position", position, 'wrld');
			ser.Value("orientation", orientation, 'ori3');
			ser.Value("linearImpulse", linearImpulse);
			ser.Value("angularImpulse", rollImpulse);
			ser.Value("angularImpulse", pitchYawImpulse);
		}
	};

	struct SerializeImpulse
	{
		Vec3 impulse = ZERO;

		void SerializeWith(TSerialize ser)
		{
			ser.Value("impulse", impulse);
		}
	};

	struct SerializeTransformData
	{
		Vec3 position;
		Quat orientation;

		void SerializeWith(TSerialize ser)
		{
			ser.Value("position", position, 'wrld');
			ser.Value("orientation", orientation, 'ori3');
		}
	};

	// Accel data structures. Jerk values for each are set in Initialize(), retrieved from AddMember listings above.
	struct JerkAccelerationData
	{
		float jerk;
		float jerkDecelRate;
		Vec3 currentJerkAccel;
		Vec3 targetJerkAccel;
		EAccelState state;

		JerkAccelerationData() : jerk(0.f), jerkDecelRate(0.f), currentJerkAccel(0.f), targetJerkAccel(0.f), state(EAccelState::Idle) {}
	};

	JerkAccelerationData m_linearAccelData = {};
	JerkAccelerationData m_rollAccelData = {};
	JerkAccelerationData m_pitchYawAccelData = {};

	enum class AxisType
	{
		Roll,
		PitchYaw,
		Linear
	};

	// struct to combine thruster axis and actuation
	struct AxisAccelParams {
		string axisName;
		float AccelAmount;
		Vec3 localDirection;
	};

	// Vector maps to organize our axis types, names and input data together 
	VectorMap<AxisType, DynArray<AxisAccelParams>> m_linearAxisParamsMap;
	VectorMap<AxisType, DynArray<AxisAccelParams>> m_rollAxisParamsMap;
	VectorMap<AxisType, DynArray<AxisAccelParams>> m_pitchYawAxisParamsMap;

	struct VelocityData
	{
		float currentVelocity;
		float previousVelocity;

		VelocityData() : currentVelocity(0.f), previousVelocity(0.f) {}
	};

	VelocityData m_shipVelocity = {};

	// Variables to track how much thrust we are generating and prevent exceeding that limit.

	/*
	struct AxisImpulseTracker
	{
		float forward = 0.f;
		float maxForward = 0.f;

		float backward = 0.f;
		float maxBackward = 0.f;

		float leftRight = 0.f;
		float maxLeftRight = 0.f;

		float upDown = 0.f;
		float maxUpDown = 0.f;

		float roll = 0.f;
		float maxRoll = 0.f;

		float pitch = 0.f;
		float maxPitch = 0.f;

		float yaw = 0.0f;
		float maxYaw = 0.f;
	};

	AxisImpulseTracker m_axisImpulseTracker;
	*/

	struct AxisImpulseTracker
	{
		Vec3 linearAxisMaxThrustPositive = ZERO;
		Vec3 linearAxisMaxThrustNegative = ZERO;
		Vec3 rollAxisMaxThrust = ZERO;
		Vec3 pitchYawMaxThrust = ZERO;

		Vec3 linearAxisCurrentThrust = ZERO;
		Vec3 rollAxisCurrentThrust = ZERO;
		Vec3 pitchYawCurrentThrust = ZERO;
	};
	
	AxisImpulseTracker m_axisImpulseTracker;

	// Getting the key states from the Vehicle
	FlightModifierBitFlag GetFlightModifierState();

	// Getting the Axis values from the Vehicle
	float AxisGetter(const string& axisName);

	// Utility functions
	float DegreesToRadian(float degrees);

	// Convert world coordinates to local coordinates
	Vec3 WorldToLocal(const Vec3& localDirection);

	// Clamping the input between -1 and 1, as well as implementing mouse sensitivity scale for the newtonian mode.
	float ClampInput(float inputValue, bool isMouse = false) const;

	/* This function is instrumental for the correct execution of UpdateAccelerationWithJerk()
	*  This function is called by each axis group independently (Pitch / Yaw; Roll; Linear)
	*  Obtains the target acceleration from the JerkAccelerationData struct and updates the struct with the current state.
	*  targetAccel is upated according to the player input (-1; 1). Any value different than zero means the player wants to move.
	*  Simply sets the acceleration state if the target Accel is zero.
	*/
	void UpdateAccelerationState(JerkAccelerationData& accelData, const Vec3& targetAccel);

	// Compares the current vs target accel, and calculates the jerk
	Vec3 UpdateAccelerationWithJerk(JerkAccelerationData& accelData, float frameTime);

	// Scales the acceleration asked, according to input magnitude, taking into account the inputs pressed
	Vec3 ScaleAccel(const VectorMap<AxisType, DynArray<AxisAccelParams>>& axisAccelParamsMap);

	/* Direct input mode: raw acceleration requests on an input scale
	*  Step 1. For each axis group, call ScaleAccel to create a scaled direction vector by input in local space
	*  Step 2. Jerk gets infused
	*  Step 3. Directly convert the result of step 2 into a force and apply it
	*/
	void DirectInput(float frameTime);

	void CoupledFM(float frameTime);

	// Compensates for the gravity pull
	void GravityAssist(float frameTime);

	/* Avoids drift by reducing the linear velocity in proportion to the alignment to your forward direction vector.
	*  Cannot be used in decoupled mode
	*/
	void ComstabAssist(float frameTime);

	// toggle between the flight modes on a key press
	void FlightModifierHandler(FlightModifierBitFlag bitFlag);

	// Impulse generator
	bool ServerRequestImpulse(SerializeImpulseData&& data, INetChannel*);

	bool ClientRequestImpulse(SerializeImpulseData&& data, INetChannel*);

	// Converts the accel target (after jerk) which contains both direction and magnitude, into thrust values.
	Vec3 AccelToImpulse(Vec3 desiredAccel, float frameTime, bool countTotal = true);
	float GetImpulse() const;
	void ResetImpulseCounter();

	// Applies an impulse, with optional parameters. roll and pitch / yaw (angular axes) will be combined.
	bool ApplyImpulse();

	// Calculate current vel / accel
	Vec3 GetVelocity();
	float GetAcceleration(float frameTime);

	// Default Components
	CVehicleComponent* m_pVehicleComponent = nullptr;

	// Custom Components
	CShipThrusterComponent* m_pShipThrusterComponent = nullptr;

	// tracking frametime
	float m_frameTime = 0.f;

	const float m_MAX_INPUT_VALUE = 1.f; // Maximum clamped input value
	const float m_MIN_INPUT_VALUE = -1.f; // Maximum clamped input value

	//Debug color
	const float m_debugColor[4] = { 1, 0, 0, 1 };

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

	// Tracking the impulses generated
	float m_totalImpulse = 0.f;
	float m_totalAngularImpulse = 0.f;
	Vec3 m_linearImpulse = ZERO;
	Vec3 m_angularImpulse = ZERO;

	// Watching the target accelerations (before jerk is applied) to track the ship state
	Vec3 targetLinearAccel = ZERO;
	Vec3 targetRollAccelDir = ZERO;
	Vec3 targetPitchYawAccel = ZERO;

	// Tracking ship position and orientation
	Vec3 m_shipPosition = ZERO;
	Quat m_shipOrientation = ZERO;
};