/*
Camera.cpp

Author:			Chris Serson
Last Edited:	August 18, 2016

Description:	Class for creating and controlling the camera
*/

#include "Camera.h"

Camera::Camera(int h, int w) {
	mYaw = mPitch = mRoll = 0.0f;
	mWidth = w;
	mHeight = h;
	mVFOV = 60.0f;
	mHFOV = mVFOV * (float)w / (float)h;

	// build projection matrix
	XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(mVFOV), (float)w / (float)h, 0.1f, 3000.0f);
	XMStoreFloat4x4(&mmProjection, proj);

	// set starting camera state
	mvPos = XMFLOAT4(0.0f, 0.0f, 150.0f, 0.0f);
	XMVECTOR look = XMVector3Normalize(XMLoadFloat4(&XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f)));
	XMStoreFloat4(&mvStartLook, look);
	XMVECTOR left = XMVector3Cross(look, XMLoadFloat4(&XMFLOAT4(0.0f, 0.0f, 1.0f, 0.0f)));
	XMStoreFloat4(&mvStartLeft, left);
	XMVECTOR up = XMVector3Cross(left, look);
	XMStoreFloat4(&mvStartUp, up);

	Update();
}

Camera::~Camera() {
}

// combine the view and projection matrices and transpose the result
XMFLOAT4X4 Camera::GetViewProjectionMatrixTransposed() {
	XMMATRIX view = XMLoadFloat4x4(&mmView);
	XMMATRIX proj = XMLoadFloat4x4(&mmProjection);
	XMMATRIX viewproj = XMMatrixTranspose(view * proj);
	XMFLOAT4X4 final;
	XMStoreFloat4x4(&final, viewproj);
	return final;
}

// Return the 6 planes forming the view frustum. Stored in the array planes.
void Camera::GetViewFrustum(XMFLOAT4 planes[6]) {
	XMMATRIX view = XMLoadFloat4x4(&mmView);
	XMMATRIX proj = XMLoadFloat4x4(&mmProjection);
	XMFLOAT4X4 M;
	XMStoreFloat4x4(&M, view * proj);

	// left
	planes[0].x = M(0, 3) + M(0, 0);
	planes[0].y = M(1, 3) + M(1, 0);
	planes[0].z = M(2, 3) + M(2, 0);
	planes[0].w = M(3, 3) + M(3, 0);
	
	// right
	planes[1].x = M(0, 3) - M(0, 0);
	planes[1].y = M(1, 3) - M(1, 0);
	planes[1].z = M(2, 3) - M(2, 0);
	planes[1].w = M(3, 3) - M(3, 0);

	// bottom
	planes[2].x = M(0, 3) + M(0, 1);
	planes[2].y = M(1, 3) + M(1, 1);
	planes[2].z = M(2, 3) + M(2, 1);
	planes[2].w = M(3, 3) + M(3, 1);

	// top
	planes[3].x = M(0, 3) - M(0, 1);
	planes[3].y = M(1, 3) - M(1, 1);
	planes[3].z = M(2, 3) - M(2, 1);
	planes[3].w = M(3, 3) - M(3, 1);

	// near
	planes[4].x = M(0, 3) + M(0, 2);
	planes[4].y = M(1, 3) + M(1, 2);
	planes[4].z = M(2, 3) + M(2, 2);
	planes[4].w = M(3, 3) + M(3, 2);

	// far
	planes[5].x = M(0, 3) - M(0, 2);
	planes[5].y = M(1, 3) - M(1, 2);
	planes[5].z = M(2, 3) - M(2, 2);
	planes[5].w = M(3, 3) - M(3, 2);

	// normalize all planes
	for (auto i = 0; i < 6; ++i) {
		XMVECTOR v = XMPlaneNormalize(XMLoadFloat4(&planes[i]));
		XMStoreFloat4(&planes[i], v);
	}
}

void Camera::GetBoundingSphereByNearFar(float near, float far, XMFLOAT4& center, float& radius) {
	// get the current view matrix
	XMMATRIX view = XMLoadFloat4x4(&mmView);
	// get the original projection matrix
	//XMMATRIX proj = XMLoadFloat4x4(&mmProjection);
	// calculate the projection matrix based on the supplied near/far planes.
	XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), (float)mWidth / (float)mHeight, near, far);
	XMMATRIX viewproj = view * proj;
	XMMATRIX invViewProj = XMMatrixInverse(nullptr, viewproj); // the inverse view/projection matrix
	
	// 3 points on the unit cube representing the view frustum in view space.
	XMVECTOR nlb = XMLoadFloat4(&XMFLOAT4(-1.0f, -1.0f, -1.0f, 1.0f));
	XMVECTOR flb = XMLoadFloat4(&XMFLOAT4(-1.0f,  1.0f,  1.0f, 1.0f));
	XMVECTOR frt = XMLoadFloat4(&XMFLOAT4( 1.0f,  1.0f,  1.0f, 1.0f));

	// transform the frustum into world space.
	nlb = XMVector3Transform(nlb, invViewProj);
	flb = XMVector3Transform(flb, invViewProj);
	frt = XMVector3Transform(frt, invViewProj);
	
	XMFLOAT4 _nlb, _nrt, _flb, _frt;
	XMStoreFloat4(&_nlb, nlb);
	XMStoreFloat4(&_flb, flb);
	XMStoreFloat4(&_frt, frt);

	// find circumcenter of triangle formed by these three points on the frustum.
	XMVECTOR a = nlb / _nlb.w;
	XMVECTOR b = flb / _flb.w;
	XMVECTOR c = frt / _frt.w;
	XMVECTOR ac = c - a;
	XMVECTOR ab = b - a;
	XMVECTOR N = XMVector3Normalize(XMVector3Cross(ab, ac));
	XMVECTOR halfAB = a + ab * 0.5f;
	XMVECTOR halfAC = a + ac * 0.5f;
	XMVECTOR perpAB = XMVector3Normalize(XMVector3Cross(ab, N));
	XMVECTOR perpAC = XMVector3Normalize(XMVector3Cross(ac, N));
	// line,line intersection test. Line 1 origin: halfAB, direction: perpAB; Line 2 origin: halfAC, direction: perpAC
	N = XMVector3Cross(perpAB, perpAC);
	XMVECTOR SR = halfAB - halfAC;
	XMFLOAT4 _N, _SR, _E;
	XMStoreFloat4(&_N, N);
	XMStoreFloat4(&_SR, SR);
	XMStoreFloat4(&_E, perpAC);
	float absX = fabs(_N.x);
	float absY = fabs(_N.y);
	float absZ = fabs(_N.z);
	float t;
	if (absZ > absX && absZ > absY) {
		t = (_SR.x * _E.y - _SR.y * _E.x) / _N.z;
	} else if (absX > absY) {
		t = (_SR.y * _E.z - _SR.z * _E.y) / _N.x;
	} else {
		t = (_SR.z * _E.x - _SR.x * _E.z) / _N.y;
	}

	XMVECTOR Circumcenter = halfAB - t * perpAB;
	XMVECTOR r = XMVector3Length(frt - Circumcenter);

	XMStoreFloat(&radius, r);
	XMStoreFloat4(&center, Circumcenter);
}

void FindBoundingSphere(XMFLOAT3 a, XMFLOAT3 b, XMFLOAT3 c, XMFLOAT3& center, float& radius) {
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
	float absX = fabs(_N.x);
	float absY = fabs(_N.y);
	float absZ = fabs(_N.z);
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

	XMStoreFloat(&radius, r);
	XMStoreFloat3(&center, Circumcenter);
}

Frustum Camera::CalculateFrustumByNearFar(float near, float far) {
	Frustum f;

	float tanHalfHFOV = tanf(XMConvertToRadians(mHFOV / 2.0f));
	float tanHalfVFOV = tanf(XMConvertToRadians(mVFOV / 2.0f));

	float xNear = near * tanHalfHFOV;
	float xFar = far * tanHalfHFOV;
	float yNear = near * tanHalfVFOV;
	float yFar = far * tanHalfVFOV;

	f.nlb = XMFLOAT3(-xNear, -yNear, near);
	f.nrb = XMFLOAT3( xNear, -yNear, near);
	f.nlt = XMFLOAT3(-xNear,  yNear, near);
	f.nrt = XMFLOAT3( xNear,  yNear, near);

	f.flb = XMFLOAT3(-xFar, -yFar, far);
	f.frb = XMFLOAT3( xFar, -yFar, far);
	f.flt = XMFLOAT3(-xFar,  yFar, far);
	f.frt = XMFLOAT3( xFar,  yFar, far);

	// get the current view and projection matrices
	XMMATRIX view = XMLoadFloat4x4(&mmView);
	XMMATRIX viewproj = view;
	XMMATRIX invViewProj = XMMatrixInverse(nullptr, viewproj); // the inverse view/projection matrix

	XMVECTOR nlb = XMLoadFloat3(&f.nlb);
	XMVECTOR nrb = XMLoadFloat3(&f.nrb);
	XMVECTOR nlt = XMLoadFloat3(&f.nlt);
	XMVECTOR nrt = XMLoadFloat3(&f.nrt);
	XMVECTOR flb = XMLoadFloat3(&f.flb);
	XMVECTOR frb = XMLoadFloat3(&f.frb);
	XMVECTOR flt = XMLoadFloat3(&f.flt);
	XMVECTOR frt = XMLoadFloat3(&f.frt);

	nlb = XMVector3Transform(nlb, invViewProj);
	nrb = XMVector3Transform(nrb, invViewProj);
	nlt = XMVector3Transform(nlt, invViewProj);
	nrt = XMVector3Transform(nrt, invViewProj);
	flb = XMVector3Transform(flb, invViewProj);
	frb = XMVector3Transform(frb, invViewProj);
	flt = XMVector3Transform(flt, invViewProj);
	frt = XMVector3Transform(frt, invViewProj);

	XMFLOAT4 _nlb, _nrb, _nrt, _nlt, _flb, _frt, _frb, _flt;
	XMStoreFloat4(&_nlb, nlb);
	XMStoreFloat4(&_nrb, nrb);
	XMStoreFloat4(&_nlt, nlt);
	XMStoreFloat4(&_nrt, nrt);
	XMStoreFloat4(&_flb, flb);
	XMStoreFloat4(&_frb, frb);
	XMStoreFloat4(&_flt, flt);
	XMStoreFloat4(&_frt, frt);

	nlb = nlb / _nlb.w;
	nrb = nrb / _nrb.w;
	nlt = nlt / _nlt.w;
	nrt = nrt / _nrt.w;
	flb = flb / _flb.w;
	frb = frb / _frb.w;
	flt = flt / _flt.w;
	frt = frt / _frt.w;

	XMStoreFloat3(&f.nlb, nlb);
	XMStoreFloat3(&f.nrb, nrb);
	XMStoreFloat3(&f.nlt, nlt);
	XMStoreFloat3(&f.nrt, nrt);
	XMStoreFloat3(&f.flb, flb);
	XMStoreFloat3(&f.frb, frb);
	XMStoreFloat3(&f.flt, flt);
	XMStoreFloat3(&f.frt, frt);

	FindBoundingSphere(f.nlb, f.flb, f.frt, f.center, f.radius);

	return f;
}

// Move the camera along its 3 axis: mvStartLook (forward/backward), mvStartLeft (left/left), mvStartUp (up/down)
void Camera::Translate(XMFLOAT3 move) {
	XMVECTOR look = XMLoadFloat4(&mvCurLook);
	XMVECTOR left = XMLoadFloat4(&mvCurLeft);
	XMVECTOR up = XMLoadFloat4(&mvCurUp);
	XMVECTOR tmp = XMLoadFloat4(&mvPos);
	
	tmp += look * move.x + left * move.y + up * move.z;

	XMStoreFloat4(&mvPos, tmp);
	
	Update();
}

// rotate the camera up and down, around mvStartLeft
void Camera::Pitch(float theta) {
	mPitch += theta;
	mPitch = mPitch > 360 ? mPitch - 360 : mPitch < -360 ? mPitch + 360 : mPitch;

	Update();
}

// rotate the camera left and right, around mvStartUp
void Camera::Yaw(float theta) {
	mYaw += theta;
	mYaw = mYaw > 360 ? mYaw - 360 : mYaw < -360 ? mYaw + 360 : mYaw;

	Update();
}

// rotate the camera clockwise and counter-clockwise, around mvStartLook
void Camera::Roll(float theta) {
	mRoll += theta;
	mRoll = mRoll > 360 ? mRoll - 360 : mRoll < -360 ? mRoll + 360 : mRoll;

	Update();
}

void Camera::Update() {
	// rotate camera based on yaw, pitch, and roll.
	XMVECTOR look = XMLoadFloat4(&mvStartLook);
	XMVECTOR up = XMLoadFloat4(&mvStartUp);
	
	float pitch_rad = XMConvertToRadians(mPitch);
	float yaw_rad = XMConvertToRadians(mYaw);
	float roll_rad = XMConvertToRadians(mRoll);

	XMMATRIX rot, rotp, roty, rotr;
	XMVECTOR left = XMLoadFloat4(&mvStartLeft);

	rotp = XMMatrixRotationAxis(left, pitch_rad);
	roty = XMMatrixRotationAxis(up, yaw_rad);
	rotr = XMMatrixRotationAxis(look, roll_rad);
	rot = rotp * roty * rotr;
	look = XMVector3Normalize(XMVector3Transform(look, rot));
	left = XMVector3Normalize(XMVector3Transform(left, rot));
	up = XMVector3Cross(left, look);

	XMStoreFloat4(&mvCurLook, look);
	XMStoreFloat4(&mvCurUp, up);
	XMStoreFloat4(&mvCurLeft, left);

	// build view matrix
	XMVECTOR camera = XMLoadFloat4(&mvPos);
	XMVECTOR target = camera + look; // add camera position plus target direction to get target location for view matrix function
	XMMATRIX view = XMMatrixLookAtLH(camera, target, up);
	XMStoreFloat4x4(&mmView, view);
}