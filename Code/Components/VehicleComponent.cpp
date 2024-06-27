// Copyright 2017-2019 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "VehicleComponent.h"
#include <CryRenderer/IRenderAuxGeom.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CryCore/StaticInstanceList.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CryEntitySystem/IEntitySystem.h>

// Forward declaration
#include <DefaultComponents/Cameras/CameraComponent.h>
#include <DefaultComponents/Input/InputComponent.h>
#include <DefaultComponents/Physics/RigidBodyComponent.h>

// Registers the component to be used in the engine
static void RegisterVehicleComponent(Schematyc::IEnvRegistrar& registrar)
{
	Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
	{
		Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CVehicleComponent));
		{

		}
	}
}

CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterVehicleComponent)

void CVehicleComponent::Initialize()
{
	m_pCameraComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CCameraComponent>();
	m_pInputComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CInputComponent>();
	m_pRigidBodyComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CRigidBodyComponent>();
	m_pFlightController = m_pEntity->GetOrCreateComponent<CFlightController>();
	InitializeInput();
}

Cry::Entity::EventFlags CVehicleComponent::GetEventMask() const
{
	//Listening to the update event
	return EEntityEvent::Update | EEntityEvent::GameplayStarted;
}

void CVehicleComponent::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
	case EEntityEvent::GameplayStarted:
	{
		hasGameStarted = true;
		if (GetFpsUseShip() == 1)
		{
			m_pCameraComponent->Activate();
		}
	}
	break;
	case EEntityEvent::Update:
	{

	}
	break;
	case Cry::Entity::EEvent::Reset:
	{
		hasGameStarted = false;
	}
	break;
	}
}

void CVehicleComponent::InitializeInput()
{
	// Translation Controls
	m_pInputComponent->RegisterAction("ship", "accel_forward", [this](int activationMode, float value) { m_axisValues["accel_forward"] = value;});
	m_pInputComponent->BindAction("ship", "accel_forward", eAID_KeyboardMouse, eKI_W);

	m_pInputComponent->RegisterAction("ship", "accel_backward", [this](int activationMode, float value) {m_axisValues["accel_backward"] = value;});
	m_pInputComponent->BindAction("ship", "accel_backward", eAID_KeyboardMouse, eKI_S);

	m_pInputComponent->RegisterAction("ship", "accel_right", [this](int activationMode, float value) {m_axisValues["accel_right"] = value;});
	m_pInputComponent->BindAction("ship", "accel_right", eAID_KeyboardMouse, eKI_D);

	m_pInputComponent->RegisterAction("ship", "accel_left", [this](int activationMode, float value) {m_axisValues["accel_left"] = value;});
	m_pInputComponent->BindAction("ship", "accel_left", eAID_KeyboardMouse, eKI_A);

	m_pInputComponent->RegisterAction("ship", "accel_up", [this](int activationMode, float value) {m_axisValues["accel_up"] = value;});
	m_pInputComponent->BindAction("ship", "accel_up", eAID_KeyboardMouse, eKI_Space);

	m_pInputComponent->RegisterAction("ship", "accel_down", [this](int activationMode, float value) {m_axisValues["accel_down"] = value;});
	m_pInputComponent->BindAction("ship", "accel_down", eAID_KeyboardMouse, eKI_LCtrl);

	m_pInputComponent->RegisterAction("ship", "boost", [this](int activationMode, float value)
		{
			// Changing vehicle boost state based on Shift key

			if (activationMode == (int)eAAM_OnPress)
			{
				m_keyStates["boost"] = 1;
			}
			else if (activationMode == eAAM_OnRelease)
			{
				m_keyStates["boost"] = 0;
			}
		});
	m_pInputComponent->BindAction("ship", "boost", eAID_KeyboardMouse, eKI_LShift);

	// Rotation Controls

	m_pInputComponent->RegisterAction("ship", "yaw", [this](int activationMode, float value) {m_axisValues["pitch"] = value;});
	m_pInputComponent->BindAction("ship", "yaw", eAID_KeyboardMouse, eKI_MouseY);
	
	m_pInputComponent->RegisterAction("ship", "pitch", [this](int activationMode, float value) {m_axisValues["yaw"] = value; });
	m_pInputComponent->BindAction("ship", "pitch", eAID_KeyboardMouse, eKI_MouseX);

	m_pInputComponent->RegisterAction("ship", "roll_left", [this](int activationMode, float value) {m_axisValues["roll_left"] = value; });
	m_pInputComponent->BindAction("ship", "roll_left", eAID_KeyboardMouse, eKI_Q);

	m_pInputComponent->RegisterAction("ship", "roll_right", [this](int activationMode, float value) {m_axisValues["roll_right"] = value; });
	m_pInputComponent->BindAction("ship", "roll_right", eAID_KeyboardMouse, eKI_E);

	// Exiting the vehicle
	m_pInputComponent->RegisterAction("ship", "exit", [this](int activationMode, float value)
		{ 
			// Checking the cvar value, if we are the ship, then change it back to the human.
			if (GetFpsUseShip() == 1)
			{
				if (activationMode == (int)eAAM_OnPress)
				{
					gEnv->pConsole->GetCVar("fps_use_ship")->Set(0);
				}
			}
		});
	m_pInputComponent->BindAction("ship", "exit", eAID_KeyboardMouse, EKeyId::eKI_Y);
}

int CVehicleComponent::IsKeyPressed(const string& actionName)
{
	return m_keyStates[actionName];
}

float CVehicleComponent::GetAxisValue(const string& axisName)
{
	return m_axisValues[axisName];
}

bool CVehicleComponent::IsFpsUseShip()
{
	return gEnv->pConsole->GetCVar("fps_use_ship")->GetIVal() == 1 ? true : false;
}

int CVehicleComponent::GetFpsUseShip()
{
	return gEnv->pConsole->GetCVar("fps_use_ship")->GetIVal();
}
