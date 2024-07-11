// Copyright 2017-2021 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once
#include <array>
#include <numeric>

#include <Components/FlightModifiers.h>

class CVehicleComponent;
class CShipThrusterComponent;

namespace Cry::DefaultComponents
{
	class CRigidBodyComponent;
}

class ScaledMotion
{
public:
	ScaledMotion(const Vec3& scaledAccel, const Vec3& scaledVel)
		: m_scaledAccel(scaledAccel), m_scaledVel(scaledVel) {}

	Vec3 GetAcceleration() const { return m_scaledAccel; }
	Vec3 GetVelocity() const { return m_scaledVel; }

private:
	Vec3 m_scaledAccel; // Scaled acceleration direction vector
	Vec3 m_scaledVel;   // Scaled velocity direction vector
};

class VelocityDiscrepancy
{
public:
	VelocityDiscrepancy(const Vec3& linearDiscrepancy, const Vec3& angularDiscrepancy)
		: m_linearDiscrepancy(linearDiscrepancy), m_angularDiscrepancy(angularDiscrepancy) {}

	Vec3 GetLinearDiscrepancy() const { return m_linearDiscrepancy; }
	Vec3 GetAngularDiscrepancy() const { return m_angularDiscrepancy; }

private:
	Vec3 m_linearDiscrepancy; // Scaled acceleration direction vector
	Vec3 m_angularDiscrepancy;   // Scaled velocity direction vector
};


class CFlightController final : public IEntityComponent
{
	static constexpr EEntityAspects kVehicleAspect = eEA_GameClientA;

public:
	CFlightController() = default;
	virtual ~CFlightController() = default;

	virtual void Initialize() override;

	virtual Cry::Entity::EventFlags GetEventMask() const override;

	virtual void ProcessEvent(const SEntityEvent& event) override;

	virtual bool NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags) override;

	virtual NetworkAspectType GetNetSerializeAspectMask() const override { return kVehicleAspect; }

	// Reflect type to set a unique identifier for this component
	// and provide additional information to expose it in the sandbox
	static void ReflectType(Schematyc::CTypeDesc<CFlightController>& desc)
	{
		desc.SetGUID("{52D14ABC-6525-4408-BBFD-D55B6710ECAB}"_cry_guid);
		desc.SetEditorCategory("Flight");
		desc.SetLabel("FlightController");
		desc.SetDescription("Executes flight calculations.");

		// Mouse sensitivity
		desc.AddMember(&CFlightController::m_mouseSenseFactor, 'msf', "mouseSensFact", "Mouse Sensitivity Factor", "Adjusts mouse sensitivity", ZERO);

		// Acceleration parameters
		desc.AddMember(&CFlightController::m_fwdAccel, 'fwdr', "forwarddir", "Forward Direction Acceleration", "Point force generator on the forward direction", ZERO);
		desc.AddMember(&CFlightController::m_bwdAccel, 'bwdr', "backwarddir", "Backward Direction Acceleration", "Point force generator on the backward direction", ZERO);
		desc.AddMember(&CFlightController::m_leftRightAccel, 'lrdr', "leftright", "Left/Right Acceleration", "Point force generator on the left direction", ZERO);
		desc.AddMember(&CFlightController::m_upDownAccel, 'uddr', "updown", "Up/Down Direction Acceleration", "Point force generator on the up / down direction", ZERO);
		desc.AddMember(&CFlightController::m_rollAccel, 'roll', "rollaccel", "Roll Acceleration", "Point force generator for rolling", ZERO);
		desc.AddMember(&CFlightController::m_pitchAccel, 'ptch', "pitch", "Pitch Acceleration", "Point force generator for pitching", ZERO);
		desc.AddMember(&CFlightController::m_yawAccel, 'yaw', "yaw", "Yaw Acceleration", "Point force generator for yawing", ZERO);
		
		// Rotational limits (degrees/sec)
		desc.AddMember(&CFlightController::m_maxRoll, 'mxrl', "maxRoll", "Max Roll Rate", "Max roll rate in degrees / sec", ZERO);
		desc.AddMember(&CFlightController::m_maxPitch, 'mxpt', "maxPitch", "Max Pitch Rate", "Max pitch rate in degrees / sec", ZERO);
		desc.AddMember(&CFlightController::m_maxYaw, 'mxyw', "maxYaw", "Max Yaw Rate", "Max yaw rate in degrees / sec", ZERO);

		// Translational limits (m/s)
		desc.AddMember(&CFlightController::m_maxFwdVel, 'mxfw', "maxFwd", "Max Forward Speed", "Max Forward Speed in m/s", ZERO);
		desc.AddMember(&CFlightController::m_maxBwdVel, 'mxbw', "maxBwd", "Max Backward Speed", "Max Backward Speed in m/s", ZERO);
		desc.AddMember(&CFlightController::m_maxLatVel, 'mxlt', "maxLT", "Max Left/Right Speed", "Max Left/Right Speed in m/s", ZERO);
		desc.AddMember(&CFlightController::m_maxUpDownVel, 'mxud', "maxUD", "Max Up/Down Speed", "Max Up/Down Speed in m/s", ZERO);

		// Jerk parameters
		desc.AddMember(&CFlightController::m_linearJerkRate, 'lnjk', "linearjerk", "Linear Jerk", "Adjusts the force smoothing rate", ZERO);
		desc.AddMember(&CFlightController::m_linearJerkDecelRate, 'lndr', "linearjerkdecel", "Linear Jerk decel rate", "Adjusts the decel rate", ZERO);
		desc.AddMember(&CFlightController::m_RollJerkRate, 'rljk', "rolljerkrate", "Roll Jerk", "Adjusts the force smoothing rate", ZERO);
		desc.AddMember(&CFlightController::m_RollJerkDecelRate, 'rldr', "rolljerkdecel", "Roll Jerk decel rate", "Adjusts the decel rate", ZERO);
		desc.AddMember(&CFlightController::m_PitchYawJerkRate, 'pyjk', "pyjerkrate", "Pitch/Yaw Jerk", "Adjusts the force smoothing rate", ZERO);
		desc.AddMember(&CFlightController::m_PitchYawJerkDecelRate, 'pydr', "pyjerkdecel", "Pitch/Yaw Jerk decel rate", "Adjusts the decel rate", ZERO);
	}

	// Axis Vector initializer
	void InitializeMotionParamsVectors();
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

	enum class AxisType
	{
		Roll,
		PitchYaw,
		Linear
	};

	// struct to combine thruster axis and actuation
	struct AxisMotionParams {
		string axisName;
		float AccelAmount;
		float velocityLimit;
		Vec3 localDirection;
	};

	// Vector maps to organize our axis types, names and input data together 
	VectorMap<AxisType, DynArray<AxisMotionParams>> m_linearParamsMap;
	VectorMap<AxisType, DynArray<AxisMotionParams>> m_rollParamsMap;
	VectorMap<AxisType, DynArray<AxisMotionParams>> m_pitchYawParamsMap;


	struct VelocityData
	{
		float currentVelocity;
		float previousVelocity;

		VelocityData() : currentVelocity(0.f), previousVelocity(0.f) {}
	};

	VelocityData m_shipVelocity = {};

	// Accel data structures. Jerk values for each are set in Initialize(), retrieved from the editor settings.
	struct JerkAccelerationData
	{
		float jerk;
		float jerkDecelRate;
		Vec3 currentJerkAccel;
		Vec3 targetJerkAccel;
		EAccelState state;

		JerkAccelerationData() : jerk(0.f), jerkDecelRate(0.f), currentJerkAccel(0.f), targetJerkAccel(0.f), state(EAccelState::Idle) {}
	};

	JerkAccelerationData m_linearJerkData = {};
	JerkAccelerationData m_rollJerkData = {};
	JerkAccelerationData m_pitchYawJerkData = {};

	struct MotionData {
		Vec3 linearAccel;
		Vec3 rollAccel;
		Vec3 pitchYawAccel;
		JerkAccelerationData& linearJerkData;
		JerkAccelerationData& rollJerkData;
		JerkAccelerationData& pitchYawJerkData;

		MotionData(Vec3 linear, Vec3 roll, Vec3 pitchYaw, JerkAccelerationData& linearJerk, JerkAccelerationData& rollJerk, JerkAccelerationData& pitchYawJerk)
			: linearAccel(linear), rollAccel(roll), pitchYawAccel(pitchYaw), linearJerkData(linearJerk), rollJerkData(rollJerk), pitchYawJerkData(pitchYawJerk) {}
	};



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
	ScaledMotion ScaleInput(const VectorMap<AxisType, DynArray<AxisMotionParams>>& axisAccelParamsMap);

	VelocityDiscrepancy CalculateDiscrepancy(Vec3 desiredLinearVelocity);

	Vec3 CalculateCorrection(const VectorMap<AxisType, DynArray<AxisMotionParams>>& axisAccelParamsMap, Vec3 linearDiscrepancy, float frameTime);

	Vec3 RotationalStability(Vec3 angularDiscrepancy, float frameTime);

	/* Direct input mode: raw acceleration requests on an input scale
	*  Step 1. For each axis group, call ScaleAccel to create a scaled direction vector by input in local space
	*  Step 2. Jerk gets infused
	*  Step 3. Directly convert the result of step 2 into a force and apply it
	*/
	void DirectInput(float frameTime);

	void CoupledFM(float frameTime);

	// Compensates for the gravity pull
	void AntiGravity(float frameTime);

	/* Avoids drift by reducing the linear velocity in proportion to the alignment to your forward direction vector.
	*  Cannot be used in decoupled mode
	*/
	void Comstab(float frameTime);

	// toggle between the flight modes on a key press
	void FlightModifierHandler(FlightModifierBitFlag bitFlag, float frameTime);

	// Converts the accel target (after jerk) which contains both direction and magnitude, into thrust values.
	Vec3 AccelToImpulse(const MotionData& motionData, float frameTime, bool mathOnly = false);
	float GetImpulse() const;
	void ResetImpulseCounter();

	// Applies an impulse, with optional parameters. roll and pitch / yaw (angular axes) will be combined.
	void ApplyImpulse(Vec3 linearImpulse, Vec3 angImpulse, bool countTotal = true);

	// Calculate current vel / accel
	Vec3 GetVelocity();
	float GetAcceleration(float frameTime);

	// TVI

	void DrawDirectionIndicator(float frameTime);

	// Debug
	void DrawOnScreenDebugText(float frameTime);

	// Networking
	bool RequestImpulseOnServer(SerializeImpulseData&& data, INetChannel*);

	bool UpdateMovement(SerializeImpulseData&& data, INetChannel*);

	// Default Components
	CVehicleComponent* m_pVehicleComponent = nullptr;

	// tracking frametime
	float m_frameTime = 0.f;

	const float m_MAX_INPUT_VALUE = 1.f; // Maximum clamped input value
	const float m_MIN_INPUT_VALUE = -1.f; // Maximum clamped input value

	//Debug color
	const float m_debugColor[4] = { 1, 0, 0, 1 };

	// Performance variables
	float m_mouseSenseFactor = 0.f;

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

	float m_maxFwdVel = 0.f;
	float m_maxBwdVel = 0.f;
	float m_maxLatVel = 0.f;
	float m_maxUpDownVel = 0.f;


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


	// Custom Components
	CShipThrusterComponent* m_pShipThrusterComponent = nullptr;
};

