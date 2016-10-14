/*
BoundingVolume.h

Author:			Chris Serson
Last Edited:	October 14, 2016

Description:	Classes and methods defining bounding volumes. Currently only BoundingSphere.

Usage:			- Proper shutdown is handled by the destructor.

Future Work:	- Add collision detection to Bounding Sphere.
				- Add Axis Aligned Bounding Box.
				- Add Object Oriented Bounding Box.
				- Add K-DOP.
*/
#pragma once
#include <DirectXMath.h>

using namespace DirectX;

class BoundingSphere;

// Find a bounding sphere by finding the circumcenter of 3 points and the distance from the points to the circumcenter
BoundingSphere FindBoundingSphere(XMFLOAT3 a, XMFLOAT3 b, XMFLOAT3 c);

class BoundingSphere {
public:
	BoundingSphere(float r = 0.0f, XMFLOAT3 c = XMFLOAT3(0.0f, 0.0f, 0.0f)) : m_valRadius(r), m_vCenter(c) {}
	~BoundingSphere() {}

	float GetRadius() { return m_valRadius; }
	void SetRadius(float r) { m_valRadius = r; }

	XMFLOAT3 GetCenter() { return m_vCenter; }
	void SetCenter(XMFLOAT3 c) { m_vCenter = c; }
	void SetCenter(float x, float y, float z) { m_vCenter = XMFLOAT3(x, y, z); }

private:
	float		m_valRadius;
	XMFLOAT3	m_vCenter;
};

