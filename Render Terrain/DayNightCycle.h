/*
DayNightCycle.h

Author:			Chris Serson
Last Edited:	October 14, 2016

Description:	Class for managing the Day/Night Cycle for the scene.

Usage:			- Calling the constructor, either through DayNightCycle D(...);
				or DayNightCycle* D; D = new DayNightCycle(...);, will initialize
				the object.
				- Proper shutdown is handled by the destructor.
				- Call Update() to move time forward. Moves forward by real time passed * m_Period.
				- Currently only works for Sun, aligned with y axis.
				- Time currently starts at midnight
				- Diffuse and Specular light intensities for the Sun now interpolated based on angle/position of Sun.

Future Work:	- Add support for both Sun and Moon at arbitrary locations.
				- Add support for animated sky box depicting time of day.
*/
#pragma once

#include "DirectionalLight.h"
#include "Camera.h"
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
static const float CASCADE_PLANES[] = { 0.1f, 64.0f, 128.0f, 256.0f, 1200.0f };

class DayNightCycle {
public:
	DayNightCycle(UINT period, UINT shadowSize);
	~DayNightCycle();

	void Update(BoundingSphere& bsScene, Camera* cam);
	void TogglePause() { m_isPaused = !m_isPaused; }

	LightSource GetLight() { return m_dlSun.GetLight(); }
	XMFLOAT4X4 GetShadowViewProjMatrix(int i) { return m_amShadowViewProjs[i]; }
	XMFLOAT4X4 GetShadowViewProjTexMatrix(int i) { return m_amShadowViewProjTexs[i]; }
	void GetShadowFrustum(int i, XMFLOAT4 planes[6]);

private:
	void CalculateShadowMatrices(BoundingSphere& bsScene, Camera* cam);
	void CalculateShadowFrustum(int i, XMMATRIX VP);
		
	UINT						m_Period;	// the number of game milliseconds that each real time millisecond should count as.
	DirectionalLight			m_dlSun;		// light source representing the sun. 
	DirectionalLight			m_dlMoon;	// light source representing the moon.
	time_point<system_clock>	m_tLast;		// the last point in time that we updated our cycle.
	float						m_angleSun = 0.0f;
	bool						m_isPaused = false;
	UINT						m_sizeShadowMap;
	XMFLOAT4X4					m_amShadowViewProjs[4];
	XMFLOAT4X4					m_amShadowViewProjTexs[4];
	XMFLOAT4					m_aShadowFrustums[4][4];
};

