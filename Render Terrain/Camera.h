/*
Camera.h

Author:			Chris Serson
Last Edited:	June 26, 2016

Description:	Class for creating and controlling the camera

Usage:			- Calling the constructor, either through Camera C(...);
				or Camera* C; C = new Camera(...);, will initialize
				the scene.
				- Proper shutdown is handled by the destructor.
				- Is hard-coded for DirectXMath

Future Work:	- Add camera controls.
*/

#pragma once

#include <DirectXMath.h>

using namespace DirectX;

class Camera
{
public:
	Camera(int h, int w);
	~Camera();

	// combine the view and projection matrices and transpose the result
	XMFLOAT4X4 GetViewProjectionMatrixTransposed();

private:
	XMFLOAT4X4	mmProjection;	// Projection matrix
	XMFLOAT4X4	mmView;			// View matrix
	XMFLOAT4	mvPos;			// Camera position
	XMFLOAT4	mvLookAt;		// Vector describing direction the camera is facing
	XMFLOAT4	mvUp;			// Up vector for Camera
};

