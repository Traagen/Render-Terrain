/*
DayNightCycle.h

Author:			Chris Serson
Last Edited:	August 7, 2016

Description:	Class for managing the Day/Night Cycle for the scene.

Usage:			- Calling the constructor, either through DayNightCycle D(...);
				or DayNightCycle* D; D = new DayNightCycle(...);, will initialize
				the object.
				- Proper shutdown is handled by the destructor.
				- Call Update() to move time forward. Moves forward by real time passed * mPeriod.
				- Currently only works for Sun, aligned with y axis.
				- Time currently starts at midnight
				- Diffuse and Specular light intensities for the Sun now interpolated based on angle/position of Sun.

Future Work:	- Add support for both Sun and Moon at arbitrary locations.
				- Add support for animated sky box depicting time of day.
*/
#pragma once

#include "DirectionalLight.h"
#include <chrono>

using namespace std::chrono;

static const double DEG_PER_MILLI = 360.0 / 86400000.0; // the number of degrees per millisecond, assuming you rotate 360 degrees in 24 hours.
static const XMFLOAT4 SUN_DIFFUSE_COLORS[] = {
	{ 0.0f, 0.0f, 0.0f, 1.0f },
	{ 0.0f, 0.0f, 0.0f, 1.0f },
	{ 0.0f, 0.0f, 0.0f, 1.0f },
	{ 0.9f, 0.2f, 0.2f, 1.0f },
	{ 0.98f, 0.86f, 0.2f, 1.0f },
	{ 0.8f, 0.8f, 0.6f, 1.0f },
	{ 0.8f, 0.8f, 0.8f, 1.0f },
	{ 0.8f, 0.8f, 0.6f, 1.0f },
	{ 0.98f, 0.86f, 0.2f, 1.0f},
	{ 0.9f, 0.2f, 0.2f, 1.0f },
	{ 0.0f, 0.0f, 0.0f, 1.0f },
	{ 0.0f, 0.0f, 0.0f, 1.0f }
};

static const XMFLOAT4 SUN_SPECULAR_COLORS[] = {
	{ 0.0f, 0.0f, 0.0f, 1.0f },
	{ 0.0f, 0.0f, 0.0f, 1.0f },
	{ 0.0f, 0.0f, 0.0f, 1.0f },
	{ 0.5f, 0.5f, 0.5f, 1.0f },
	{ 0.8f, 0.8f, 0.8f, 1.0f },
	{ 1.0f, 1.0f, 1.0f, 1.0f },
	{ 1.0f, 1.0f, 1.0f, 1.0f },
	{ 1.0f, 1.0f, 1.0f, 1.0f },
	{ 0.8f, 0.8f, 0.8f, 1.0f },
	{ 0.5f, 0.5f, 0.5f, 1.0f },
	{ 0.0f, 0.0f, 0.0f, 1.0f },
	{ 0.0f, 0.0f, 0.0f, 1.0f }
};

class DayNightCycle {
public:
	DayNightCycle(UINT period);
	~DayNightCycle();

	void Update();
	void TogglePause() { isPaused = !isPaused; }

	LightSource GetLight() { return mdlSun.GetLight(); }
	XMFLOAT4X4 GetShadowViewProjectionMatrixTransposed(XMFLOAT3 centerBoundingSphere, float radiusBoundingSphere);
	XMFLOAT4X4 GetShadowTransformMatrixTransposed(XMFLOAT3 centerBoundingSphere, float radiusBoundingSphere);

private:
	UINT						mPeriod;	// the number of game milliseconds that each real time millisecond should count as.
	DirectionalLight			mdlSun;		// light source representing the sun. 
	DirectionalLight			mdlMoon;	// light source representing the moon.
	time_point<system_clock>	mtLast;		// the last point in time that we updated our cycle.
	float						mCurrentSunAngle = 0.0f;
	bool						isPaused = false;
};

