#pragma once
#include "StdAfx.h"
#include <CryMath/Cry_Math.h>

// Creating a struct to store thruster rotation and translation parameters
struct ThrusterParams
{
	Vec3 translation;
	Vec3 rotation;
	Vec3 thrustForce;

	ThrusterParams(const Vec3& transl, const Vec3& rot, const float& tfrc) :
		translation(transl), rotation(rot), thrustForce(tfrc) {}

	void LogTransform() const
	{
		CryLog("Translation: x:%f, y:%f, z:%f", translation.x, translation.y, translation.z);
		CryLog("Rotation: x:%f, y:%f, z:%f", rotation.x, rotation.y, rotation.z);
		CryLog("Force: %f", thrustForce);
	}
};