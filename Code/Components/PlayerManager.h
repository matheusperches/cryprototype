// Copyright 2017-2021 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Cry::DefaultComponents
{
	class CCameraComponent;
	class CInputComponent;
	class CRigidBodyComponent;
}


////////////////////////////////////////////////////////
// Spawn point
////////////////////////////////////////////////////////
class CPlayerManager final : public IEntityComponent
{
public:
	CPlayerManager() {}
	~CPlayerManager() {}
	static CPlayerManager& GetInstance()
	{
		static CPlayerManager instance;
		return instance;
	}

	// Reflect type to set a unique identifier for this component
	// and provide additional information to expose it in the sandbox
	static void ReflectType(Schematyc::CTypeDesc<CPlayerManager>& desc)
	{
		desc.SetGUID("{C79F7332-FDD5-4E45-A7FA-B627166A37EC}"_cry_guid);
		desc.SetEditorCategory("PlayerManager");
		desc.SetLabel("PlayerManager");
		desc.SetDescription("Handles CVar updates and logic to enter / exit vehicles.");
	}
	virtual void ProcessEvent(const SEntityEvent& event) override;
	virtual void Initialize() override;
	virtual Cry::Entity::EventFlags GetEventMask() const override;


	// Functions 
	void CharacterSwitcher();

protected:
private:
	CPlayerManager(const CPlayerManager&) = delete;
	CPlayerManager& operator=(const CPlayerManager&) = delete;

	// Cvar reference to be initialized later
	ICVar* m_fpsUseShip;

};
