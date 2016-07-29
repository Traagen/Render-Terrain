#include "DayNightCycle.h"

DayNightCycle::DayNightCycle(UINT period) : mdlSun(XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f), XMFLOAT4(0.91f, 0.74f, 0.08f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f)),
								mdlMoon(XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f), XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f), XMFLOAT4(0.6f, 0.6f, 0.6f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f)),
								mPeriod(period) {
	mtLast = system_clock::now();
}


DayNightCycle::~DayNightCycle() {
}


void DayNightCycle::Update() {
	time_point<system_clock> now = system_clock::now();

	// get the amount of time in ms since the last time we updated.
	milliseconds elapsed = duration_cast<milliseconds>(now - mtLast);

	// calculate how far to rotate.
	double angletorotate = elapsed.count() * mPeriod * XMConvertToRadians(DEG_PER_MILLI);

	// rotate the sun's direction vector.
	XMFLOAT3 tmp = mdlSun.GetLight().direction;
	XMVECTOR dir = XMLoadFloat3(&tmp);
	XMVECTOR rot = XMQuaternionRotationRollPitchYaw(0.0f, (float)angletorotate, 0.0f);
	dir = XMVector3Normalize(XMVector3Rotate(dir, rot));
	XMStoreFloat3(&tmp, dir);
	mdlSun.SetLightDirection(tmp);

	// rotate the moon's direction vector.
	tmp = mdlMoon.GetLight().direction;
	dir = XMLoadFloat3(&tmp);
	rot = XMQuaternionRotationRollPitchYaw(0.0f, (float)angletorotate, 0.0f);
	dir = XMVector3Normalize(XMVector3Rotate(dir, rot));
	XMStoreFloat3(&tmp, dir);
	mdlMoon.SetLightDirection(tmp);

	// update the time for the next pass.
	mtLast = now;
}