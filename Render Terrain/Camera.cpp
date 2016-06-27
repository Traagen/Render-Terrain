/*
Camera.cpp

Author:			Chris Serson
Last Edited:	June 26, 2016

Description:	Class for creating and controlling the camera
*/

#include "Camera.h"

Camera::Camera(int h, int w) {
	// build projection matrix
	XMMATRIX viewproj = XMMatrixPerspectiveFovLH(XMConvertToRadians(45.0f), (float)w / (float)h, 0.1f, 24000.0f);
	XMStoreFloat4x4(&mmProjection, viewproj);

	// set starting camera state
	//mvPos = XMFLOAT4(1024.0f, 1024.0f, 1800.0f, 0.0f); // 1024
	mvPos = XMFLOAT4(512.0f, 512.0f, 900.0f, 0.0f); // 512
	//mvPos = XMFLOAT4(2048.0f, 2048.0f, 3600.0f, 0.0f); // 2048
	//mvPos = XMFLOAT4(4096.0f, 4096.0f, 7200.0f, 0.0f); // 4096
	//mvLookAt = XMFLOAT4(512.0f, 512.0f, 0.0f, 0.0f); // 1024
	mvLookAt = XMFLOAT4(256.0f, 256.0f, 0.0f, 0.0f); // 512
	//mvLookAt = XMFLOAT4(1024.0f, 1024.0f, 0.0f, 0.0f); // 2048
	//mvLookAt = XMFLOAT4(2048.0f, 2048.0f, 0.0f, 0.0f); // 4096
	mvUp = XMFLOAT4(0.0f, 0.0f, 1.0f, 0.0f);

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