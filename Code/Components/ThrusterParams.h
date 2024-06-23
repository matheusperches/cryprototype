#pragma once
#include "StdAfx.h"
#include <CryMath/Cry_Math.h>

// Creating a struct to store thruster rotation and translation parameters
struct ThrusterParams
{
	Vec3 translation;
	Vec3 rotation;
	float force;

	ThrusterParams(const Vec3& transl, const Vec3& rot, const float& frc) :
		translation(transl), rotation(rot), force(frc) {}

	void LogTransform() const
	{
		CryLog("Translation: x:%f, y:%f, z:%f", translation.x, translation.y, translation.z);
		CryLog("Rotation: x:%f, y:%f, z:%f", rotation.x, rotation.y, rotation.z);
		CryLog("Force: %f", force);
	}
};