
[![linkedin](https://img.shields.io/badge/linkedin-0A66C2?style=for-the-badge&logo=linkedin&logoColor=white)](https://www.linkedin.com/in/matheus-perches/)

#### [Return to my Portfolio](https://github.com/matheusperches/matheusperches.github.io) 

## [CryEngine Flight Controller study ]([https://github.com/matheusperches/6DoF-TimeAttack](https://github.com/matheusperches/cryprototype))

#### A small CryEngine project with the goal to get experience using the engine's tools and APIs while developing a flight system to be later used in a bigger project.

## Build instructions
Make sure you have Visual Studio 2022 installed with the C++ game development add ons. 

1. Click on Code -> Download ZIP.
2. Extract the file in a folder.
3. Click on game.cryproject 
4. Choose "build solution"
5. Navigate to solutions -> Win64 -> Game.sln and open it in VS2022. 
6. Build the project with CTRL + Shift + B.
7. Inside Visual Studio on the side bar "Solution Explorer", right click "Game" and select "Set as startup project"
8. Press F5 to launch the project
9. Play.
10. To launch the editor and customize parameters -> Switch startup project to "Editor" -> F5 -> File - Open - Level -"Example"

## Controls 
WASD - Linear movement

Q/E - Roll

Mouse X/Y - Pitch & Yaw

V - Netwonian toggle 

G - Gravity Assist 

Shift - Boost

## Features
 - First person character with a raycast system for entering the vehicle
 - A fully customizable flyable entity with six degrees of freedom control
 - Fully physically simulated, simplified by applying all math onto a symmetric cube. Can be hidden and replaced with a ship mesh. 
 - Tunable accelerations profiles for linear and angular motion, including jerk profiles
 - Acceleration control mode (Netwonian): The player's inputs are interpreted as a directional acceleration request.
 - Velocity control mode (Coupled): The player's inputs are interpreted as a directional velocity request.
 - Gravity assist: Can be toggled ON or OFF, and it is enforced in Coupled mode. Compensates for gravity by generating a proprotional counter force against gravity pull. 
 - Boost: a simple acceleration multiplier, applied at the generator of impulse (last stage of calculations)

## Code snippets (full code available inside Code -> Components -> FlightController.cpp

### Axis maps and flight parameters, organizing their information to be used by the flight system 
```c++
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
```
```c++
// Accel data structures. Jerk values for each are set in Initialize(), retrieved from the editor settings.
struct JerkAccelerationData
{
	float jerk;
	float jerkDecelRate;
	Vec3 currentJerkAccel;
	Vec3 targetJerkAccel;
	EAccelState state;

	JerkAccelerationData() : jerk(0.f), jerkDecelRate(0.f), currentJerkAccel(0.f), targetJerkAccel(0.f), state(EAccelState::Decelerating) {}
};

JerkAccelerationData m_linearJerkData = {};
JerkAccelerationData m_rollJerkData = {};
JerkAccelerationData m_pitchYawJerkData = {};

struct MotionData {
	Vec3 linearAccel;
	Vec3 rollAccel;
	Vec3 pitchYawAccel;
	JerkAccelerationData* linearJerkData;
	JerkAccelerationData* rollJerkData;
	JerkAccelerationData* pitchYawJerkData;

	// Default constructor
	MotionData()
		: linearAccel(Vec3(ZERO)), rollAccel(Vec3(ZERO)), pitchYawAccel(Vec3(ZERO)),
		linearJerkData(nullptr), rollJerkData(nullptr), pitchYawJerkData(nullptr) {}

	// Parameterized constructor
	MotionData(Vec3 linear, Vec3 roll, Vec3 pitchYaw, JerkAccelerationData* linearJerk, JerkAccelerationData* rollJerk, JerkAccelerationData* pitchYawJerk)
		: linearAccel(linear), rollAccel(roll), pitchYawAccel(pitchYaw),
		linearJerkData(linearJerk), rollJerkData(rollJerk), pitchYawJerkData(pitchYawJerk) {}
};
```

```
### Initialization of the input mapping and axis data (member variables received from the editor)
```c++
void CFlightController::InitializeMotionParamsVectors()
{
	// Initializing the maps of the motion profile
	m_linearParamsMap[AxisType::Linear] = {
		{"accel_forward", m_fwdAccel, m_maxFwdVel,  Vec3(0.f, 1.f, 0.f)},
		{"accel_backward", m_bwdAccel, m_maxBwdVel, Vec3(0.f, -1.f, 0.f)},
		{"accel_left", m_leftRightAccel, m_maxLatVel, Vec3(-1.f, 0.f, 0.f)},
		{"accel_right", m_leftRightAccel, m_maxLatVel, Vec3(1.f, 0.f, 0.f)},
		{"accel_up", m_upDownAccel, m_maxUpDownVel, Vec3(0.f, 0.f, 1.f)},
		{"accel_down", m_upDownAccel, m_maxUpDownVel, Vec3(0.f, 0.f, -1.f)}
	};
	m_rollParamsMap[AxisType::Roll] = {
		{"roll_left", DEG2RAD(m_rollAccel), DEG2RAD(m_maxRoll), Vec3(0.f, -1.f, 0.f)},
		{"roll_right", DEG2RAD(m_rollAccel), DEG2RAD(m_maxRoll), Vec3(0.f, 1.f, 0.f)}
	};
	m_pitchYawParamsMap[AxisType::PitchYaw] = {
		{"yaw", DEG2RAD(m_yawAccel), DEG2RAD(m_maxYaw), Vec3(0.f, 0.f, -1.f)},
		{"pitch", DEG2RAD(m_pitchAccel), DEG2RAD(m_maxPitch), Vec3(-1.f, 0.f, 0.f)}
	};
}
```
### Bitwise operations for handling the flight modifiers such as mode switching and boosting. 
```c++
// Define the Flight Modifier Flags enum
enum class EFlightModifierFlag : uint8_t
{
    Coupled = 1 << 1,
    Boost = 1 << 2,
    Gravity = 1 << 3,
    Comstab = 1 << 4
};

// Utility class for handling the bitwise flags
class FlightModifierBitFlag
{
public:
    void SetFlag(EFlightModifierFlag flag)
    {
        m_modifierValue |= (int)flag;
    }

    void UnsetFlag(EFlightModifierFlag flag)
    {
        m_modifierValue &= ~(int)flag;
    }

    void ToggleFlag(EFlightModifierFlag flag)
    {
        m_modifierValue ^= (int)flag;
    }

    bool HasFlag(EFlightModifierFlag flag) const
    {
        return (m_modifierValue & (int)flag) == (int)flag;
    }
    bool HasAnyFlag(EFlightModifierFlag multiFlag) const
    {
        return (m_modifierValue & (int)multiFlag) != 0;
    }

private:
    uint8_t m_modifierValue = 0;
};
```

### Receiving the input and scaling it for both acceleration and velocity control 
```c++
ScaledMotion CFlightController::ScaleInput(const VectorMap<AxisType, DynArray<AxisMotionParams>>& axisParamsList)
{
	// Initializing vectors for acceleration direction and desired acceleration
	Vec3 localDirection(ZERO); // Calculate local thrust direction based on input value

	// Vectors to accumulate the directions of the requested accelerations and velocities by the input magnitude.
	Vec3 requestedAccelDirection(ZERO), requestedVelDirection(ZERO);
	
	for (const auto& axisMotionParamsPair : axisParamsList)	// Iterating over the list of axis and their input values
	{
		AxisType axisType = axisMotionParamsPair.first;
		const DynArray<AxisMotionParams>& axisMotionParamsArray = axisMotionParamsPair.second;

		for (const auto& motionParams : axisMotionParamsArray)	// Iterate over the DynArray<AxisAccelParams> for the current Axis
		{
			float clampedInput(ZERO); // Clamping or normalizing input values
			if (axisType == AxisType::PitchYaw)
			{
				// Applying a mouse sentitivity scaling for pitch and yaw, if it is enabled
				clampedInput = ClampInput(AxisGetter(motionParams.axisName), motionParams.AccelAmount, true); // Retrieve input value for the current axis and clamp to a range of -1 to 1
			}
			else
				clampedInput = ClampInput(AxisGetter(motionParams.axisName), motionParams.AccelAmount);

			localDirection = WorldToLocal(motionParams.localDirection); // Convert to local space
			
			requestedAccelDirection += localDirection * motionParams.AccelAmount * clampedInput; // Accumulate axis direction, scaling each by its input magnitude in local space
			
			requestedVelDirection += localDirection * motionParams.velocityLimit * clampedInput; // Accumulate desired velocity based on thrust direction and input value
		}
	}
	return ScaledMotion(requestedAccelDirection, requestedVelDirection);   // Return the scaled acceleration direction vector in m/s
}
```

### Jerk calculations: Eases in / out the acceleration rate, smoothing out ship movement. 
```c++ 
Vec3 CFlightController::UpdateAccelerationWithJerk(AxisType axisType, JerkAccelerationData& accelData, float frameTime) const
{
	// Calculate the difference between current and target acceleration
	Vec3 deltaAccel = accelData.targetJerkAccel - accelData.currentJerkAccel;
	Vec3 finalJerk;

	// Determine jerk rate based on acceleration state
	switch (accelData.state)
	{
	case EAccelState::Accelerating:
	{
		float scale = powf(fabs(deltaAccel.GetLength()), 0.3f);
		finalJerk = deltaAccel * scale * accelData.jerk * frameTime;
		break;
	}
	case EAccelState::Decelerating:
	{
		finalJerk = deltaAccel * accelData.jerkDecelRate * frameTime;
		break;
	}
	}

	// Update current acceleration
	Vec3 newAccel = accelData.currentJerkAccel + finalJerk; // Calculate new acceleration by adding jerk to current acceleration

	return newAccel; // Return the updated acceleration
}
```
### Flight modes 
```c++ 
///////////////////////////////////////////////////////////////////////////
// FLIGHT MODES 
///////////////////////////////////////////////////////////////////////////

void CFlightController::DirectInput(float frameTime)
{
	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 30, 2, m_debugColor, false, "(V) Newtonian");


	Vec3 linearAccelMagnitude = ScaleInput(m_linearParamsMap).GetAcceleration();
	Vec3 rollAccelMagnitude = ScaleInput(m_rollParamsMap).GetAcceleration();
	Vec3 pitchYawMagnitude = ScaleInput(m_pitchYawParamsMap).GetAcceleration();


	// Updates our current requested motion state to compute jerk accordingly (based on input, not ship motion!)
	UpdateAccelerationState(m_linearJerkData, linearAccelMagnitude);
	UpdateAccelerationState(m_rollJerkData, rollAccelMagnitude);
	UpdateAccelerationState(m_pitchYawJerkData, pitchYawMagnitude);

	MotionData motionData(linearAccelMagnitude, rollAccelMagnitude,
		pitchYawMagnitude, &m_linearJerkData, &m_rollJerkData, &m_pitchYawJerkData);

	// Send movement data to the server if we are connected, apply locally if not
	if (!gEnv->bServer)
	{
		SRmi<RMI_WRAP(&CFlightController::RequestImpulseOnServer)>::InvokeOnServer(this, SerializeImpulseData{
		Vec3(ZERO),
		Quat(ZERO),
		linearAccelMagnitude,
		rollAccelMagnitude,
		pitchYawMagnitude });
	}
	else 
		AccelToImpulse(motionData, frameTime);
}

void CFlightController::CoupledFM(float frameTime)
{
	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 30, 2, m_debugColor, false, "(V) Coupled");

	Vec3 linearVelMagnitude = ScaleInput(m_linearParamsMap).GetVelocity(); // Scale and set the target velocity for linear movement
	Vec3 rollVelMagnitude = ScaleInput(m_rollParamsMap).GetVelocity();
	Vec3 pitchYawVelMagnitude = ScaleInput(m_pitchYawParamsMap).GetVelocity();

	// Updates our current requested motion state to compute jerk accordingly (based on input, not ship motion!)
	UpdateAccelerationState(m_linearJerkData, linearVelMagnitude);

	// Calculates the velocity discrepancy between the current velocity and the requested
	Vec3 linearDiscrepancy = CalculateDiscrepancy(linearVelMagnitude).GetLinearDiscrepancy();
	Vec3 rollDiscrepancy = CalculateDiscrepancy(rollVelMagnitude).GetAngularDiscrepancy();
	Vec3 pitchYawDiscrepancy = CalculateDiscrepancy(pitchYawVelMagnitude).GetAngularDiscrepancy();

	// Calculates a correction, accounting for overshoot
	Vec3 linearCorrection = CalculateCorrection(m_linearParamsMap, linearVelMagnitude, linearDiscrepancy);
	Vec3 rollCorrection = CalculateCorrection(m_rollParamsMap, rollVelMagnitude , rollDiscrepancy);
	Vec3 pitchYawCorrection = CalculateCorrection(m_pitchYawParamsMap, pitchYawVelMagnitude, pitchYawDiscrepancy);

	// Setting up the motion parameters 
	MotionData motionData(linearCorrection, rollCorrection,
		pitchYawCorrection, &m_linearJerkData, &m_rollJerkData, &m_pitchYawJerkData);

	// Send movement data to the server if we are connected, apply locally if not
	if (!gEnv->bServer)
	{
		SRmi<RMI_WRAP(&CFlightController::RequestImpulseOnServer)>::InvokeOnServer(this, SerializeImpulseData{
		Vec3(ZERO),
		Quat(ZERO),
		linearCorrection,
		rollCorrection,
		pitchYawCorrection });
	}
	else
		AccelToImpulse(motionData, frameTime);
}
```

### For coupled mode, calculating the speed correctio while accounting for potential overshoot
## Implementing a logarithmic scaling for speed, so we are not requesting 100% acceleration throughout the speed range, for both linear and angular velocities.

```c++
float CFlightController::LogScale(float discrepancyMagnitude, float maxDiscrepancy, float base)
{
	if (discrepancyMagnitude == 0.0f)
		return 0.0f;

	float logDiscrepancy = std::log(discrepancyMagnitude + 1.0f) / std::log(base);
	float logMaxDiscrepancy = std::log(maxDiscrepancy + 1.0f) / std::log(base);

	return logDiscrepancy / logMaxDiscrepancy;
}
```
```c++
VelocityDiscrepancy CFlightController::CalculateDiscrepancy(Vec3 desiredVelocity)
{
	pe_status_dynamics dynamics = GetDynamics();

	Vec3 linearDiscrepancy = desiredVelocity - dynamics.v;
	Vec3 angularDiscrepancy = desiredVelocity - dynamics.w;

	// Compute the discrepancy for each axis
	return VelocityDiscrepancy(linearDiscrepancy, angularDiscrepancy);
}
```

```c++
Vec3 CFlightController::CalculateCorrection(const VectorMap<AxisType, DynArray<AxisMotionParams>>& axisAccelParamsMap, Vec3 requestedVelocity, Vec3 velDiscrepancy)
{
	pe_status_dynamics dynamics = GetDynamics();

	Vec3 totalCorrectiveAccel = Vec3(ZERO);
	Vec3 predictedVelocity = Vec3(ZERO);
	float overshootFactor = 1.f;

	// Calculate the magnitude of the velocity discrepancy
	float discrepancyMagnitude = velDiscrepancy.GetLength();

	const float maxDiscrepancy = m_linearLogMaxDiscrepancy;
	const float base = m_linearLogBase;

	// Calculate the velocity scaling factor using logarithmic scaling
	float scalingFactor = LogScale(discrepancyMagnitude, maxDiscrepancy, base);

	// Ensure the scaling factor does not exceed 1.0
	scalingFactor = std::min(scalingFactor, 1.0f);

	// Initializing motionData for simulated calculation
	MotionData motionData(
		Vec3(ZERO),
		Vec3(ZERO),
		Vec3(ZERO),
		&m_linearJerkData,
		&m_rollJerkData,
		&m_pitchYawJerkData
	);

	// Handle Linear Correction
	for (const auto& axisAccelParamsPair : axisAccelParamsMap)
	{
		AxisType axisType = axisAccelParamsPair.first;

		const DynArray<AxisMotionParams>& axisParamsArray = axisAccelParamsPair.second;
		for (const auto& accelParams : axisParamsArray)
		{
			Vec3 localDirection = WorldToLocal(accelParams.localDirection).GetNormalized();
			float alignment = localDirection.Dot(velDiscrepancy.GetNormalized());

			Vec3 correction = accelParams.AccelAmount * localDirection * alignment * scalingFactor;
			totalCorrectiveAccel += correction;

			// Predicting velocity to account for overshoot and calculating logarithmic velocity scaling for smoother motion
			if (axisType == AxisType::Linear)
			{
				motionData.linearAccel = totalCorrectiveAccel;
				Vec3 simulatedAccel = AccelToImpulse(motionData, m_frameTime, true).GetLinearImpulse();
				// Predict future velocity based on current acceleration and jerk
				predictedVelocity = dynamics.v + simulatedAccel; // Use the current acceleration for prediction
			}
			else if (axisType == AxisType::Roll)
			{
				motionData.rollAccel = totalCorrectiveAccel;
				Vec3 simulatedAccel = AccelToImpulse(motionData, m_frameTime, true).GetAngularImpulse();
				// Predict future velocity based on current acceleration and jerk
				predictedVelocity = dynamics.w + simulatedAccel;
			}
			else if (axisType == AxisType::PitchYaw)
			{
				motionData.pitchYawAccel = totalCorrectiveAccel;
				Vec3 simulatedAccel = AccelToImpulse(motionData, m_frameTime, true).GetAngularImpulse();
				// Predict future velocity based on current acceleration and jerk
				predictedVelocity = dynamics.w + simulatedAccel;
			}
		}
	}

	float targetVelocityLength = requestedVelocity.GetLength();

	// Calculate overshoot factor based on predicted velocity
	if (predictedVelocity.GetLength() > targetVelocityLength + 0.1f)
	{
		overshootFactor = targetVelocityLength / predictedVelocity.GetLength();

		// Scale total corrective acceleration by the overshoot factor
		totalCorrectiveAccel *= overshootFactor;
	}

	return totalCorrectiveAccel;
}
```
### Anti-Gravity logic 
```
void CFlightController::AntiGravity(float frameTime)
{
	gEnv->pAuxGeomRenderer->Draw2dLabel(50, 180, 2, m_debugColor, false, "(G) Anti-Gravity: ON");

	pe_status_dynamics dynamics = GetDynamics();

	// Get gravity vector
	Vec3 gravity = gEnv->pPhysicalWorld->GetPhysVars()->gravity;
	Vec3 antiGravityForce = -gravity * dynamics.mass;
	float totalAlignment = 0.0f;
	Vec3 totalScaledAntiGravityForce = Vec3(ZERO);
	pe_action_impulse impulseAction;

	// First pass: Calculate total alignment
	for (const auto& axisAccelParamsPair : m_linearParamsMap) // Iterating over each axis within the linear set.
	{
		const DynArray<AxisMotionParams>& axisParamsArray = axisAccelParamsPair.second;

		for (const auto& accelParams : axisParamsArray)	// Iterate over the DynArray<AxisAccelParams> for the current AxisType
		{
			Vec3 localDirection = WorldToLocal(accelParams.localDirection);

			localDirection.Normalize(); // Normalize to have a range of -1 to 1, which indicates their alignment (1 = perfect / -1 = anti) 

			float alignment = localDirection.Dot(gravity.GetNormalized()); // Get the alignment between localDirection and gravity, using this to scale the thrust amount
			if (alignment > ZERO)
			{
				totalAlignment += alignment;
			}
		}
	}

	// Second pass: Apply impulses proportionally
	for (const auto& axisAccelParamsPair : m_linearParamsMap)
	{
		const DynArray<AxisMotionParams>& axisParamsArray = axisAccelParamsPair.second;

		for (const auto& accelParams : axisParamsArray)
		{
			Vec3 localDirection = WorldToLocal(accelParams.localDirection);
			localDirection.Normalize();

			float alignment = localDirection.Dot(gravity.GetNormalized());
			if (alignment > ZERO)
			{
				float proportionalAlignment = alignment / totalAlignment;
				Vec3 scaledAntiGravityForce = antiGravityForce * proportionalAlignment;
				totalScaledAntiGravityForce += scaledAntiGravityForce;
				impulseAction.impulse = scaledAntiGravityForce * frameTime;
				m_pEntity->GetPhysicalEntity()->Action(&impulseAction);
			}
		}
	}
	m_totalImpulse += totalScaledAntiGravityForce.GetLength();

}
```
### Converting the received accelerations, from either modes (Newtonian or Coupled) into an impulse, then applying it. 
```c++
ImpulseResult CFlightController::AccelToImpulse(const MotionData& motionData, float frameTime, bool mathOnly)
{
	pe_status_dynamics dynamics = GetDynamics();

	Vec3 linearImpulse = Vec3(ZERO);
	Vec3 angImpulse = Vec3(ZERO);

	// Pointer to the motion data to use, either original or simulated
	const MotionData* pMotionData = &motionData;
	MotionData simulatedMotionData;

	if (mathOnly)
	{
		// Create a copy of motionData to work with if mathOnly is true
		simulatedMotionData = motionData;
		// pMotionData will point to the copy if we are simulating an impulse, not affecting the proper data.
		pMotionData = &simulatedMotionData;
	}

	// Infuse the acceleration value with the current jerk coefficient
	pMotionData->linearJerkData->targetJerkAccel = pMotionData->linearAccel;
	pMotionData->linearJerkData->currentJerkAccel = UpdateAccelerationWithJerk(AxisType::Linear, *pMotionData->linearJerkData, frameTime);

	pMotionData->rollJerkData->targetJerkAccel = pMotionData->rollAccel;
	pMotionData->rollJerkData->currentJerkAccel = UpdateAccelerationWithJerk(AxisType::Roll, *pMotionData->rollJerkData, frameTime);

	pMotionData->pitchYawJerkData->targetJerkAccel = pMotionData->pitchYawAccel;
	pMotionData->pitchYawJerkData->currentJerkAccel = UpdateAccelerationWithJerk(AxisType::PitchYaw, *pMotionData->pitchYawJerkData, frameTime);

	// Calculate impulses based on the jerk data
	linearImpulse = pMotionData->linearJerkData->currentJerkAccel * dynamics.mass * frameTime;
	angImpulse = (pMotionData->rollJerkData->currentJerkAccel + pMotionData->pitchYawJerkData->currentJerkAccel) * dynamics.mass * frameTime;

	ApplyImpulse(linearImpulse, angImpulse, mathOnly);

	return ImpulseResult(linearImpulse, angImpulse);
}


void CFlightController::ApplyImpulse(Vec3 linearImpulse, Vec3 angImpulse, bool mathOnly)
{

	IPhysicalEntity* pPhysicalEntity = GetEntity()->GetPhysics();
	if (pPhysicalEntity)
	{
		pe_action_impulse actionImpulse;

		if (m_isBoosting)
		{
			linearImpulse *= m_linearBoost;
			angImpulse *= m_angularBoost;
		}
		actionImpulse.impulse = linearImpulse;
		actionImpulse.angImpulse = angImpulse;


		// Apply linear and angular impulse
		if (!mathOnly)
		{
			pPhysicalEntity->Action(&actionImpulse);
			pPhysicalEntity->Action(&actionImpulse);
			Vec3 impulse = (actionImpulse.impulse + actionImpulse.angImpulse) / m_frameTime; // Revert the frametime scaling to have the proper values
			m_totalImpulse += impulse.GetLength();
		}

		// Update our impulse tracking variables to send over to the server
		m_linearImpulse = linearImpulse;
		m_angularImpulse = actionImpulse.angImpulse;
	}
}
```

# Things I'm proud of 
- This flight controller is a much enhanced version of my original design in UE4, for the following reasons:
- The data is nicely organized into maps and containers.
- The mode switching is cleanly done by bitwise operations.
- Inputs are combined into a motion vector instead of being individually computed.
- Jerk implementation
- Speed limit implementation that does not rely on an enforced external dampening.
- Two modes of flying and many parameters for tuning.
- THe code is much more maintanable and modular.
