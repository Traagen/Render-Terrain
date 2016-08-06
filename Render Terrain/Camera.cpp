/*
Camera.cpp

Author:			Chris Serson
Last Edited:	July 18, 2016

Description:	Class for creating and controlling the camera
*/

#include "Camera.h"

Camera::Camera(int h, int w) {
	mYaw = mPitch = mRoll = 0.0f;

	// build projection matrix
	XMMATRIX viewproj = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), (float)w / (float)h, 0.1f, 3000.0f);
	XMStoreFloat4x4(&mmProjection, viewproj);

	// set starting camera state
	mvPos = XMFLOAT4(0.0f, 0.0f, 150.0f, 0.0f);
	XMVECTOR look = XMVector3Normalize(XMLoadFloat4(&XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f)));
	XMStoreFloat4(&mvLookAt, look);
	XMVECTOR left = XMVector3Cross(look, XMLoadFloat4(&XMFLOAT4(0.0f, 0.0f, 1.0f, 0.0f)));
	XMStoreFloat4(&mvLeft, left);
	XMVECTOR up = XMVector3Cross(left, look);
	XMStoreFloat4(&mvUp, up);
}

Camera::~Camera() {
}

// combine the view and projection matrices and transpose the result
XMFLOAT4X4 Camera::GetViewProjectionMatrixTransposed() {
	// rotate camera based on yaw, pitch, and roll.
	XMVECTOR look = XMLoadFloat4(&mvLookAt);
	XMVECTOR up = XMLoadFloat4(&mvUp);
	XMVECTOR left = XMLoadFloat4(&mvLeft);
	if (mPitch != 0 || mYaw != 0 || mRoll != 0) {
		float pitch_rad = XMConvertToRadians(mPitch);
		float yaw_rad = XMConvertToRadians(mYaw);
		float roll_rad = XMConvertToRadians(mRoll);

		XMMATRIX rot, rotp, roty, rotr;
		rotp = XMMatrixRotationAxis(left, pitch_rad);
		roty = XMMatrixRotationAxis(up, yaw_rad);
		rotr = XMMatrixRotationAxis(look, roll_rad);
		rot = rotp * roty * rotr;
		look = XMVector3Normalize(XMVector3Transform(look, rot));
		left = XMVector3Normalize(XMVector3Transform(left, rot));
		up = XMVector3Cross(left, look);
	}
	
	// build view matrix
	XMVECTOR camera = XMLoadFloat4(&mvPos);
	XMVECTOR target = camera + look; // add camera position plus target direction to get target location for view matrix function
	XMMATRIX view = XMMatrixLookAtLH(camera, target, up);
	
	XMMATRIX proj = XMLoadFloat4x4(&mmProjection);
	XMMATRIX viewproj = XMMatrixTranspose(view * proj);
	XMFLOAT4X4 final;
	XMStoreFloat4x4(&final, viewproj);
	return final;
}

// Return the 6 planes forming the view frustum. Stored in the array planes.
void Camera::GetViewFrustum(XMFLOAT4 planes[6]) {
	// rotate camera based on yaw, pitch, and roll.
	XMVECTOR look = XMLoadFloat4(&mvLookAt);
	XMVECTOR up = XMLoadFloat4(&mvUp);
	XMVECTOR left = XMLoadFloat4(&mvLeft);
	if (mPitch != 0 || mYaw != 0 || mRoll != 0) {
		float pitch_rad = XMConvertToRadians(mPitch);
		float yaw_rad = XMConvertToRadians(mYaw);
		float roll_rad = XMConvertToRadians(mRoll);

		XMMATRIX rot, rotp, roty, rotr;
		rotp = XMMatrixRotationAxis(left, pitch_rad);
		roty = XMMatrixRotationAxis(up, yaw_rad);
		rotr = XMMatrixRotationAxis(look, roll_rad);
		rot = rotp * roty * rotr;
		look = XMVector3Normalize(XMVector3Transform(look, rot));
		left = XMVector3Normalize(XMVector3Transform(left, rot));
		up = XMVector3Cross(left, look);
	}
	// build view/projection matrix
	XMVECTOR camera = XMLoadFloat4(&mvPos);
	XMVECTOR target = camera + look; // add camera position plus target direction to get target location for view matrix function
	XMMATRIX view = XMMatrixLookAtLH(camera, target, up);
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
	planes[4].x = M(0, 2);
	planes[4].y = M(1, 2);
	planes[4].z = M(2, 2);
	planes[4].w = M(3, 2);

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

// Move the camera along its 3 axis: mvLookAt (forward/backward), mvLeft (left/left), mvUp (up/down)
XMFLOAT4 Camera::Translate(XMFLOAT3 move) {
	// rotate camera based on yaw, pitch, and roll.
	XMVECTOR look = XMLoadFloat4(&mvLookAt);
	XMVECTOR up = XMLoadFloat4(&mvUp);
	XMVECTOR left = XMLoadFloat4(&mvLeft);
	if (mPitch != 0 || mYaw != 0 || mRoll != 0) {
		float pitch_rad = XMConvertToRadians(mPitch);
		float yaw_rad = XMConvertToRadians(mYaw);
		float roll_rad = XMConvertToRadians(mRoll);

		XMMATRIX rot, rotp, roty, rotr;
		rotp = XMMatrixRotationAxis(left, pitch_rad);
		roty = XMMatrixRotationAxis(up, yaw_rad);
		rotr = XMMatrixRotationAxis(look, roll_rad);
		rot = rotp * roty * rotr;
		look = XMVector3Normalize(XMVector3Transform(look, rot));
		left = XMVector3Normalize(XMVector3Transform(left, rot));
		up = XMVector3Cross(left, look);
	}

	XMVECTOR tmp = XMLoadFloat4(&mvPos);
	
	tmp += look * move.x + left * move.y + up * move.z;

	XMStoreFloat4(&mvPos, tmp);
	return mvPos;
}

// rotate the camera up and down, around mvLeft
void Camera::Pitch(float theta) {
	mPitch += theta;
	mPitch = mPitch > 360 ? mPitch - 360 : mPitch < -360 ? mPitch + 360 : mPitch;
}

// rotate the camera left and right, around mvUp
void Camera::Yaw(float theta) {
	mYaw += theta;
	mYaw = mYaw > 360 ? mYaw - 360 : mYaw < -360 ? mYaw + 360 : mYaw;
}

// rotate the camera clockwise and counter-clockwise, around mvLookAt
void Camera::Roll(float theta) {
	mRoll += theta;
	mRoll = mRoll > 360 ? mRoll - 360 : mRoll < -360 ? mRoll + 360 : mRoll;
}