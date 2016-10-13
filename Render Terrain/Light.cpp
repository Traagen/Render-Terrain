/*
Light.h

Author:			Chris Serson
Last Edited:	October 12, 2016

Description:	Class representing lights.
*/
#include "Light.h"

Light::Light(XMFLOAT4 p, XMFLOAT4 amb, XMFLOAT4 dif, XMFLOAT4 spec, XMFLOAT3 att, float r, XMFLOAT3 dir, float sexp) {
	m_lsLight.pos = p;
	m_lsLight.intensityAmbient = amb;
	m_lsLight.intensityDiffuse = dif;
	m_lsLight.intensitySpecular = spec;
	m_lsLight.attenuation = att;
	m_lsLight.range = r;
	m_lsLight.spotlightConeExponent = sexp;

	XMVECTOR v = XMLoadFloat3(&dir);
	v = XMVector3Normalize(v);
	XMStoreFloat3(&m_lsLight.direction, v);
}