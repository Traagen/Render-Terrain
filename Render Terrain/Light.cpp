#include "Light.h"

Light::Light(XMFLOAT4 p, XMFLOAT4 amb, XMFLOAT4 dif, XMFLOAT4 spec, XMFLOAT3 att, float r, XMFLOAT3 dir, float sexp) {
	mlsLightData.pos = p;
	mlsLightData.intensityAmbient = amb;
	mlsLightData.intensityDiffuse = dif;
	mlsLightData.intensitySpecular = spec;
	mlsLightData.attenuation = att;
	mlsLightData.range = r;
	mlsLightData.spotlightConeExponent = sexp;

	XMVECTOR v = XMLoadFloat3(&dir);
	v = XMVector3Normalize(v);
	XMStoreFloat3(&mlsLightData.direction, v);
}