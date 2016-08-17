/*
Camera.h

Author:			Chris Serson
Last Edited:	August 15, 2016

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
	// Move the camera along its 3 axis: mvStartLook (forward/backward), mvStartLeft (left/right), mvStartUp (up/down)
	void Translate(XMFLOAT3 move);
	// rotate the camera up and down, around mvStartLeft
	void Pitch(float theta);
	// rotate the camera left and right, around mvStartUp
	void Yaw(float theta);
	// rotate the camera clockwise and counter-clockwise, around mvStartLook
	void Roll(float theta);
	// calculate a bounding sphere center and radius for the current view matrix and the projection matrix defined by near/far.
	void GetBoundingSphereByNearFar(float near, float far, XMFLOAT4& center, float& radius);

private:
	void Update();
	
	XMFLOAT4X4	mmProjection;	// Projection matrix
	XMFLOAT4X4	mmView;			// View matrix
	XMFLOAT4	mvPos;			// Camera position
	XMFLOAT4	mvStartLook;	// Starting lookat vector
	XMFLOAT4	mvStartUp;		// Starting up vector
	XMFLOAT4	mvStartLeft;	// Starting left vector
	XMFLOAT4	mvCurLook;		// Current lookat vector
	XMFLOAT4	mvCurUp;		// Current up vector
	XMFLOAT4	mvCurLeft;		// Current left vector
	int			mWidth;
	int			mHeight;
	float		mYaw;
	float		mPitch;
	float		mRoll;
};

