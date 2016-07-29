#pragma once
#include "Light.h"

class DirectionalLight : public Light {
public:
	DirectionalLight() : Light() {}
	DirectionalLight(XMFLOAT4 amb, XMFLOAT4 dif, XMFLOAT4 spec, XMFLOAT3 dir) : Light(XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f), amb, dif, spec, XMFLOAT3(0.0f, 0.0f, 0.0f), 0.0f, dir, 0.0f) {}
	~DirectionalLight() {}
};

