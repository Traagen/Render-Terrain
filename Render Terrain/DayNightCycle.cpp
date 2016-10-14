/*
DayNightCycle.cpp

Author:			Chris Serson
Last Edited:	October 14, 2016

Description:	Class for managing the Day/Night Cycle for the scene.
*/
#include "DayNightCycle.h"

XMFLOAT4 ColorLerp(XMFLOAT4 color1, XMFLOAT4 color2, float interpolator) {
	//x + s(y - x)
	XMVECTOR c1 = XMLoadFloat4(&color1);
	XMVECTOR c2 = XMLoadFloat4(&color2);
	XMFLOAT4 newcolor;
	XMStoreFloat4(&newcolor, c1 + interpolator * (c2 - c1));

	return newcolor;
}

DayNightCycle::DayNightCycle(UINT period, UINT shadowSize) : m_dlSun(XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f), SUN_DIFFUSE_COLORS[0], SUN_SPECULAR_COLORS[0], XMFLOAT3(0.0f, 0.0f, 1.0f)),
								m_dlMoon(XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f), XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f), XMFLOAT4(0.6f, 0.6f, 0.6f, 1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f)),
								m_Period(period), m_sizeShadowMap(shadowSize) {
	m_tLast = system_clock::now();
}

DayNightCycle::~DayNightCycle() {
}

void DayNightCycle::Update(BoundingSphere& bsScene, Camera* cam) {
	time_point<system_clock> now = system_clock::now();

	if (!m_isPaused) {
		// get the amount of time in ms since the last time we updated.
		milliseconds elapsed = duration_cast<milliseconds>(now - m_tLast);

		// calculate how far to rotate.
		double angletorotate = elapsed.count() * m_Period * DEG_PER_MILLI;
		float angleinrads = XMConvertToRadians((float)angletorotate);

		// rotate the sun's direction vector.
		XMFLOAT3 tmp = m_dlSun.GetLight().direction;
		XMVECTOR dir = XMLoadFloat3(&tmp);
		XMVECTOR rot = XMQuaternionRotationRollPitchYaw(0.0f, -(float)angleinrads, 0.0f);
		dir = XMVector3Normalize(XMVector3Rotate(dir, rot));
		XMStoreFloat3(&tmp, dir);
		m_dlSun.SetLightDirection(tmp);

		// use the angletorotate to calculate what the current colour of the Sun should be.
		float newangle = fmod(m_angleSun + (float)angletorotate, 360.0f);
		int iColor1, iColor2; // color indices to get colors to interpolate between
		float iInterpolator; // amount to interpolate by
		if (newangle >= 330.0f) {
			iColor1 = 11;
			iColor2 = 0;
			iInterpolator = (newangle - 330.0f) / 30.0f;
		} else if (newangle >= 300.0f) {
			iColor1 = 10;
			iColor2 = 11;
			iInterpolator = (newangle - 300.0f) / 30.0f;
		} else if (newangle >= 270.0f) {
			iColor1 = 9;
			iColor2 = 10;
			iInterpolator = (newangle - 270.0f) / 30.0f;
		} else if (newangle >= 240.0f) {
			iColor1 = 8;
			iColor2 = 9;
			iInterpolator = (newangle - 240.0f) / 30.0f;
		} else if (newangle >= 210.0f) {
			iColor1 = 7;
			iColor2 = 8;
			iInterpolator = (newangle - 210.0f) / 30.0f;
		} else if (newangle >= 180.0f) {
			iColor1 = 6;
			iColor2 = 7;
			iInterpolator = (newangle - 180.0f) / 30.0f;
		} else if (newangle >= 150.0f) {
			iColor1 = 5;
			iColor2 = 6;
			iInterpolator = (newangle - 150.0f) / 30.0f;
		} else if (newangle >= 120.0f) {
			iColor1 = 4;
			iColor2 = 5;
			iInterpolator = (newangle - 120.0f) / 30.0f;
		} else if (newangle >= 90.0f) {
			iColor1 = 3;
			iColor2 = 4;
			iInterpolator = (newangle - 90.0f) / 30.0f;
		} else if (newangle >= 60.0f) {
			iColor1 = 2;
			iColor2 = 3;
			iInterpolator = (newangle - 60.0f) / 30.0f;
		} else if (newangle >= 30.0f) {
			iColor1 = 1;
			iColor2 = 2;
			iInterpolator = (newangle - 30.0f) / 30.0f;
		} else if (newangle >= 0.0f) {
			iColor1 = 0;
			iColor2 = 1;
			iInterpolator = newangle / 30.0f;
		}
		
		m_dlSun.SetDiffuseColor(ColorLerp(SUN_DIFFUSE_COLORS[iColor1], SUN_DIFFUSE_COLORS[iColor2], iInterpolator));
		m_dlSun.SetSpecularColor(ColorLerp(SUN_SPECULAR_COLORS[iColor1], SUN_SPECULAR_COLORS[iColor2], iInterpolator));

		m_angleSun = newangle;
	}

	// update the time for the next pass.
	m_tLast = now;

	CalculateShadowMatrices(bsScene, cam);
}

void DayNightCycle::CalculateShadowMatrices(BoundingSphere& bsScene, Camera* cam) {
	LightSource light = m_dlSun.GetLight();
	XMVECTOR lightdir = XMLoadFloat3(&light.direction);
	XMFLOAT3 vCenterScene = bsScene.GetCenter();
	XMVECTOR targetpos = XMLoadFloat3(&vCenterScene);
	
	float offset = (float)(m_sizeShadowMap + 8) / (float)m_sizeShadowMap; // add padding to projection for rounding and for pcf.
	float radiusScene = ceilf(bsScene.GetRadius()) * offset;

	XMVECTOR lightpos = targetpos - 2.0f * radiusScene * lightdir;
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	up = XMVector3Cross(up, lightdir);

	XMMATRIX V = XMMatrixLookAtLH(lightpos, targetpos, up); // light space view matrix transform bounding sphere to light space
	XMFLOAT4 spherecenterls;

	// create the cascades.
	for (int i = 0; i < 3; ++i) {
		Frustum fCascade = cam->CalculateFrustumByNearFar(CASCADE_PLANES[i], CASCADE_PLANES[i + 1]);
		float radius = ceilf(fCascade.bs.GetRadius());
		radius *= offset;
		XMVECTOR c = XMLoadFloat3(&fCascade.bs.GetCenter());
		XMStoreFloat4(&spherecenterls, XMVector3TransformCoord(c, V));
		XMVECTOR sc = XMLoadFloat3(&vCenterScene);
		XMFLOAT4 cbs;
		XMStoreFloat4(&cbs, XMVector3TransformCoord(sc, V));

		// orthographic frustum
		float l = spherecenterls.x - radius;
		float b = spherecenterls.y - radius;
		float n = spherecenterls.z - cbs.z - radiusScene;
		float r = spherecenterls.x + radius;
		float t = spherecenterls.y + radius;
		float f = spherecenterls.z + cbs.z + radiusScene;
		
		XMMATRIX P = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

		XMMATRIX S = V * P;

		// add rounding to update shadowmap by texel-sized increments.
		XMVECTOR shadowOrigin = XMVector3Transform(XMVectorZero(), S);
		shadowOrigin *= ((float)(m_sizeShadowMap + offset) / 4.0f);
		XMFLOAT2 so;
		XMStoreFloat2(&so, shadowOrigin);
		XMVECTOR roundedOrigin = XMLoadFloat2(&XMFLOAT2(round(so.x), round(so.y)));
		XMVECTOR rounding = roundedOrigin - shadowOrigin;
		rounding /= ((m_sizeShadowMap + offset) / 4.0f);
		XMStoreFloat2(&so, rounding);
		XMMATRIX roundMatrix = XMMatrixTranslation(so.x, so.y, 0.0f);
		S *= roundMatrix;

		// Calculate the frustum planes for this view projection matrix.
		CalculateShadowFrustum(i, S);
		
		XMStoreFloat4x4(&m_amShadowViewProjs[i], XMMatrixTranspose(S));

		// transform NDC space [-1, +1]^2 to texture space [0, 1]^2
		float x, y;
		if (i == 0) {
			x = 0.25f;
			y = 0.25f;
		} else if (i == 1) {
			x = 0.25f;
			y = 0.75f;
		} else if (i == 2) {
			x = 0.75f;
			y = 0.25f;
		} else {
			x = 0.75f;
			y = 0.75f;
		}
				
		XMMATRIX T(0.25f, 0.0f, 0.0f, 0.0f,
			0.0f, -0.25f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			x, y, 0.0f, 1.0f);

		S *= T;

		XMStoreFloat4x4(&m_amShadowViewProjTexs[i], XMMatrixTranspose(S));
	}

	XMVECTOR c = XMLoadFloat3(&vCenterScene);
	XMStoreFloat4(&spherecenterls, XMVector3TransformCoord(c, V));

	// orthographic frustum
	float l = spherecenterls.x - radiusScene;
	float b = spherecenterls.y - radiusScene;
	float n = spherecenterls.z - radiusScene;
	float r = spherecenterls.x + radiusScene;
	float t = spherecenterls.y + radiusScene;
	float f = spherecenterls.z + radiusScene;

	XMMATRIX P = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

	XMMATRIX S = V * P;

	// add rounding to update shadowmap by texel-sized increments.
	XMVECTOR shadowOrigin = XMVector3Transform(XMVectorZero(), S);
	shadowOrigin *= ((float)(m_sizeShadowMap + offset) / 4.0f);
	XMFLOAT2 so;
	XMStoreFloat2(&so, shadowOrigin);
	XMVECTOR roundedOrigin = XMLoadFloat2(&XMFLOAT2(round(so.x), round(so.y)));
	XMVECTOR rounding = roundedOrigin - shadowOrigin;
	rounding /= ((m_sizeShadowMap + offset) / 4.0f);
	XMStoreFloat2(&so, rounding);
	XMMATRIX roundMatrix = XMMatrixTranslation(so.x, so.y, 0.0f);
	S *= roundMatrix;

	// Calculate the frustum planes for this view projection matrix.
	CalculateShadowFrustum(3, S);

	XMStoreFloat4x4(&m_amShadowViewProjs[3], XMMatrixTranspose(S));

	// transform NDC space [-1, +1]^2 to texture space [0, 1]^2
	XMMATRIX T(0.25f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.25f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.75f, 0.75f, 0.0f, 1.0f);

	S *= T;

	XMStoreFloat4x4(&m_amShadowViewProjTexs[3], XMMatrixTranspose(S));
}

void DayNightCycle::CalculateShadowFrustum(int i, XMMATRIX VP) {
	XMFLOAT4X4 M;
	XMStoreFloat4x4(&M, VP);

	// left
	m_aShadowFrustums[i][0].x = M(0, 3) + M(0, 0);
	m_aShadowFrustums[i][0].y = M(1, 3) + M(1, 0);
	m_aShadowFrustums[i][0].z = M(2, 3) + M(2, 0);
	m_aShadowFrustums[i][0].w = M(3, 3) + M(3, 0);

	// right
	m_aShadowFrustums[i][1].x = M(0, 3) - M(0, 0);
	m_aShadowFrustums[i][1].y = M(1, 3) - M(1, 0);
	m_aShadowFrustums[i][1].z = M(2, 3) - M(2, 0);
	m_aShadowFrustums[i][1].w = M(3, 3) - M(3, 0);

	// bottom
	m_aShadowFrustums[i][2].x = M(0, 3) + M(0, 1);
	m_aShadowFrustums[i][2].y = M(1, 3) + M(1, 1);
	m_aShadowFrustums[i][2].z = M(2, 3) + M(2, 1);
	m_aShadowFrustums[i][2].w = M(3, 3) + M(3, 1);

	// top
	m_aShadowFrustums[i][3].x = M(0, 3) - M(0, 1);
	m_aShadowFrustums[i][3].y = M(1, 3) - M(1, 1);
	m_aShadowFrustums[i][3].z = M(2, 3) - M(2, 1);
	m_aShadowFrustums[i][3].w = M(3, 3) - M(3, 1);

	// normalize all planes
	for (auto j = 0; j < 4; ++j) {
		XMVECTOR v = XMPlaneNormalize(XMLoadFloat4(&m_aShadowFrustums[i][j]));
		XMStoreFloat4(&m_aShadowFrustums[i][j], v);
	}
}

void DayNightCycle::GetShadowFrustum(int i, XMFLOAT4 planes[6]) {
	for (int j = 0; j < 4; ++j) {
		planes[j] = m_aShadowFrustums[i][j];
	}
}