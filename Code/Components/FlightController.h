// Copyright 2017-2021 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

class CShipThrusterComponent;
class CVehicleComponent;


namespace Cry::DefaultComponents
{
	class CInputComponent;
	class CRigidBodyComponent;
}

class CFlightController final : public IEntityComponent
{
	static constexpr EEntityAspects kVehiclePhysics = eEA_GameClientA;

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

	virtual NetworkAspectType GetNetSerializeAspectMask() const override { return kVehiclePhysics; }

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
			ser.Value("linearImpulse", linearImpulse, 'vimp');
			ser.Value("angularImpulse", rollImpulse, 'vimp');
			ser.Value("angularImpulse", pitchYawImpulse, 'vimp');
		}
	};
	struct SerializeImpulse
	{
		Vec3 impulse = ZERO;

		void SerializeWith(TSerialize ser)
		{
			ser.Value("impulse", impulse, 'vimp');
		}
	};

	struct NoParams
	{
		void SerializeWith(TSerialize ser)
		{

		}
	};


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
	enum class AxisType
	{
		Roll,
		PitchYaw,
		Linear
	};

	// Physical Entity reference
	IPhysicalEntity* physEntity = nullptr;

	// Axis Vector initializer
	void InitializeAccelParamsVectors();
	// Jerk data initializer
	void InitializeJerkParams();
	// Reset the jerk values 
	void ResetJerkParams();

	// Put the flight calculations in order, segmented by axis group, to produce motion.
	void ProcessFlight(float frameTime);

	// Impulse generator
	bool ServerApplyImpulse(SerializeImpulseData&& data, INetChannel*);

	Vec3 GetVelocity();
	float GetAcceleration(float frameTime);

	bool ClientApplyImpulse(SerializeImpulseData&& data, INetChannel*);

protected:
private:
	// Default Components
	CVehicleComponent* m_pVehicleComponent = nullptr;

	// Custom Components
	CShipThrusterComponent* m_pShipThrusterComponent = nullptr;

	const float m_MAX_INPUT_VALUE = 1.f; // Maximum clamped input value
	const float m_MIN_INPUT_VALUE = -1.f; // Maximum clamped input value
	
	// Ship State
	EFlightMode m_pFlightMode = EFlightMode::Newtonian;

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


	// struct to combine thruster axis and actuation
	struct AxisAccelParams {
		string axisName;
		float AccelAmount;
	};

	// Vector maps to organize our axis types, names and input data together 
	VectorMap<AxisType, DynArray<AxisAccelParams>> m_linearAxisParamsMap;
	VectorMap<AxisType, DynArray<AxisAccelParams>> m_rollAxisParamsMap;
	VectorMap<AxisType, DynArray<AxisAccelParams>> m_pitchYawAxisParamsMap;

	
	// Receive the input manager from VehicleComponent
	void GetVehicleInputManager();
	// Getting the key states from the Vehicle
	int KeyState(const string& actionName);
	// Getting the Axis values from the Vehicle
	float AxisGetter(const string& axisName);

	// Compares the current vs target accel, and calculates the jerk
	Vec3 UpdateAccelerationWithJerk(JerkAccelerationData& accelData, float deltaTime);

	// Scales the acceleration asked, according to input magnitude, taking into account the inputs pressed
	Vec3 ScaleAccel(const VectorMap<AxisType, DynArray<AxisAccelParams>>& axisAccelParamsMap);

	// Updates the enum class "AccelState" according to our ship state.
	void UpdateAccelerationState(JerkAccelerationData& accelData, const Vec3& targetAccel);

	// Utility functions
	float DegreesToRadian(float degrees);

	// Convert world coordinates to local coordinates
	Vec3 ImpulseWorldToLocal(const Vec3& localDirection);
	float NormalizeInput(float inputValue, bool isMouse = false) const;

	// Converts the accel target (after jerk) which contains both direction and magnitude, into thrust values.
	Vec3 AccelToImpulse(Vec3 desiredAccel, float frameTime);  
	float GetImpulse() const;
	void ResetImpulseCounter();
};