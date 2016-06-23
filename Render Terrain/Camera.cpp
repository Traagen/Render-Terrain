/*
Camera.cpp

Author:			Chris Serson
Last Edited:	June 23, 2016

Description:	Class for creating and controlling the camera
*/

#include "Camera.h"

Camera::Camera(int h, int w) {
	// build projection matrix
	XMMATRIX viewproj = XMMatrixPerspectiveFovLH(45.0f * (XM_PI / 180.0f), (float)w / (float)h, 0.1f, 1000.0f);
	XMStoreFloat4x4(&mmProjection, viewproj);

	// set starting camera state
	mvPos = XMFLOAT4(0.0f, 2.0f, -4.0f, 0.0f);
	mvLookAt = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	mvUp = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);

	// build view matrix
	viewproj = XMMatrixLookAtLH(XMLoadFloat4(&mvPos), XMLoadFloat4(&mvLookAt), XMLoadFloat4(&mvUp));
	XMStoreFloat4x4(&mmView, viewproj);
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