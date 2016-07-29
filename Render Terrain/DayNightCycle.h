#pragma once

#include "DirectionalLight.h"
#include <chrono>

using namespace std::chrono;

static const double DEG_PER_MILLI = 360.0 / 86400000.0; // the number of degrees per millisecond, assuming you rotate 360 degrees in 24 hours.

class DayNightCycle
{
public:
	DayNightCycle(UINT period);
	~DayNightCycle();

	void Update();

	LightSource GetLight() { return mdlSun.GetLight(); }

private:
	UINT						mPeriod;	// the number of game milliseconds that each real time millisecond should count as.
	DirectionalLight			mdlSun;		// light source representing the sun. 
	DirectionalLight			mdlMoon;	// light source representing the moon.
	time_point<system_clock>	mtLast;		// the last point in time that we updated our cycle.
};

