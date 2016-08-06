/*
DayNightCycle.cpp

Author:			Chris Serson
Last Edited:	August 5, 2016

Description:	Class for managing the Day/Night Cycle for the scene.
*/
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
	XMVECTOR rot = XMQuaternionRotationRollPitchYaw(0.0f, -(float)angletorotate, 0.0f);
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

XMFLOAT4X4 DayNightCycle::GetShadowViewProjectionMatrixTransposed(XMFLOAT3 centerBoundingSphere, float radiusBoundingSphere) {
	LightSource light = mdlSun.GetLight();
	XMVECTOR lightdir = XMLoadFloat3(&light.direction);
	XMVECTOR lightpos = -2.0f * radiusBoundingSphere * lightdir;
	XMVECTOR targetpos = XMLoadFloat3(&centerBoundingSphere);
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	up = XMVector3Cross(lightdir, up);

	XMMATRIX V = XMMatrixLookAtLH(lightpos, targetpos, up); // light space view matrix
	
	// transform bounding sphere to light space
	XMFLOAT3 spherecenterls;
	XMStoreFloat3(&spherecenterls, XMVector3TransformCoord(targetpos, V));

	// orthographic frustum
	float l = spherecenterls.x - radiusBoundingSphere;
	float b = spherecenterls.y - radiusBoundingSphere;
	float n = spherecenterls.z - radiusBoundingSphere;
	float r = spherecenterls.x + radiusBoundingSphere;
	float t = spherecenterls.y + radiusBoundingSphere;
	float f = spherecenterls.z + radiusBoundingSphere;
	XMMATRIX P = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

	XMMATRIX S = XMMatrixTranspose(V * P);
	XMFLOAT4X4 final;
	XMStoreFloat4x4(&final, S);

	return final;
}

XMFLOAT4X4 DayNightCycle::GetShadowTransformMatrixTransposed(XMFLOAT3 centerBoundingSphere, float radiusBoundingSphere) {
	LightSource light = mdlSun.GetLight();
	XMVECTOR lightdir = XMLoadFloat3(&light.direction);
	XMVECTOR lightpos = -2.0f * radiusBoundingSphere * lightdir;
	XMVECTOR targetpos = XMLoadFloat3(&centerBoundingSphere);
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	up = XMVector3Cross(lightdir, up);

	XMMATRIX V = XMMatrixLookAtLH(lightpos, targetpos, up); // light space view matrix
															// transform bounding sphere to light space
	XMFLOAT3 spherecenterls;
	XMStoreFloat3(&spherecenterls, XMVector3TransformCoord(targetpos, V));

	// orthographic frustum
	float l = spherecenterls.x - radiusBoundingSphere;
	float b = spherecenterls.y - radiusBoundingSphere;
	float n = spherecenterls.z - radiusBoundingSphere;
	float r = spherecenterls.x + radiusBoundingSphere;
	float t = spherecenterls.y + radiusBoundingSphere;
	float f = spherecenterls.z + radiusBoundingSphere;
	XMMATRIX P = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

	// transform NDC space [-1, +1]^2 to texture space [0, 1]^2
	XMMATRIX T(0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	XMMATRIX S = XMMatrixTranspose(V * P * T);
	XMFLOAT4X4 final;
	XMStoreFloat4x4(&final, S);

	return final;
}