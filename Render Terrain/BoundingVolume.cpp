/*
BoundingVolume.cpp

Author:			Chris Serson
Last Edited:	October 14, 2016

Description:	Classes and methods defining bounding volumes.
*/

#include "BoundingVolume.h"

BoundingSphere FindBoundingSphere(XMFLOAT3 a, XMFLOAT3 b, XMFLOAT3 c) {
	XMVECTOR _a = XMLoadFloat3(&a);
	XMVECTOR _b = XMLoadFloat3(&b);
	XMVECTOR _c = XMLoadFloat3(&c);
	XMVECTOR ac = _c - _a;
	XMVECTOR ab = _b - _a;
	XMVECTOR N = XMVector3Normalize(XMVector3Cross(ab, ac));
	XMVECTOR halfAB = _a + ab * 0.5f;
	XMVECTOR halfAC = _a + ac * 0.5f;
	XMVECTOR perpAB = XMVector3Normalize(XMVector3Cross(ab, N));
	XMVECTOR perpAC = XMVector3Normalize(XMVector3Cross(ac, N));
	// line,line intersection test. Line 1 origin: halfAB, direction: perpAB; Line 2 origin: halfAC, direction: perpAC
	N = XMVector3Cross(perpAB, perpAC);
	XMVECTOR SR = halfAB - halfAC;
	XMFLOAT4 _N, _SR, _E;
	XMStoreFloat4(&_N, N);
	XMStoreFloat4(&_SR, SR);
	XMStoreFloat4(&_E, perpAC);
	float absX = fabsf(_N.x);
	float absY = fabsf(_N.y);
	float absZ = fabsf(_N.z);
	float t;
	if (absZ > absX && absZ > absY) {
		t = (_SR.x * _E.y - _SR.y * _E.x) / _N.z;
	}
	else if (absX > absY) {
		t = (_SR.y * _E.z - _SR.z * _E.y) / _N.x;
	}
	else {
		t = (_SR.z * _E.x - _SR.x * _E.z) / _N.y;
	}

	XMVECTOR Circumcenter = halfAB - t * perpAB;
	XMVECTOR r = XMVector3Length(_c - Circumcenter);

	float radius;
	XMFLOAT3 center;
	XMStoreFloat(&radius, r);
	XMStoreFloat3(&center, Circumcenter);
	return BoundingSphere(radius, center);
}