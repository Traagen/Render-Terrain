/*
Camera.h

Author:			Chris Serson
Last Edited:	July 1, 2016

Description:	Class for creating and controlling the camera

Usage:			- Calling the constructor, either through Camera C(...);
				or Camera* C; C = new Camera(...);, will initialize
				the scene.
				- Proper shutdown is handled by the destructor.
				- Is hard-coded for DirectXMath
				- Translate() to move camera
				- Roll(), Pitch(), and Yaw() to rotate camera

Future Work:	- I'm not 100% certain everything is correct when Roll is used. Will need to test further.				
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
	// returns mvPos;
	XMFLOAT4 GetEyePosition() { return mvPos; }
	// Return the 6 planes forming the view frustum. Stored in the array planes.
	void GetViewFrustum(XMFLOAT4 planes[6]);
	// Move the camera along its 3 axis: mvLookAt (forward/backward), mvLeft (left/right), mvUp (up/down)
	XMFLOAT4 Translate(XMFLOAT3 move);
	// rotate the camera up and down, around mvLeft
	void Pitch(float theta);
	// rotate the camera left and right, around mvUp
	void Yaw(float theta);
	// rotate the camera clockwise and counter-clockwise, around mvLookAt
	void Roll(float theta);
private:
	XMFLOAT4X4	mmProjection;	// Projection matrix
	XMFLOAT4	mvPos;			// Camera position
	XMFLOAT4	mvLookAt;		// Vector describing direction camera is facing
	XMFLOAT4	mvUp;			// Up vector for Camera
	XMFLOAT4	mvLeft;			// Vector pointing left, perpendicular to Up and LookAt
	float		mYaw;
	float		mPitch;
	float		mRoll;
};

