/*
Camera.h

Author:			Chris Serson
Last Edited:	October 12, 2016

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

struct Frustum {
	XMFLOAT3 nlb;
	XMFLOAT3 nlt;
	XMFLOAT3 nrb;
	XMFLOAT3 nrt;
	XMFLOAT3 flb;
	XMFLOAT3 flt;
	XMFLOAT3 frb;
	XMFLOAT3 frt;
	XMFLOAT3 center;
	float radius;
};

class Camera
{
public:
	Camera(int h, int w);
	~Camera();

	// combine the view and projection matrices and transpose the result
	XMFLOAT4X4 GetViewProjectionMatrixTransposed();
	// returns m_vPos;
	XMFLOAT4 GetEyePosition() { return m_vPos; }
	// Return the 6 planes forming the view frustum. Stored in the array planes.
	void GetViewFrustum(XMFLOAT4 planes[6]);
	// Move the camera along its 3 axis: m_vStartLook (forward/backward), m_vStartLeft (left/right), m_vStartUp (up/down)
	void Translate(XMFLOAT3 move);
	// rotate the camera up and down, around m_vStartLeft
	void Pitch(float theta);
	// rotate the camera left and right, around m_vStartUp
	void Yaw(float theta);
	// rotate the camera clockwise and counter-clockwise, around m_vStartLook
	void Roll(float theta);
	// calculate a bounding sphere center and radius for the current view matrix and the projection matrix defined by near/far.
	void GetBoundingSphereByNearFar(float near, float far, XMFLOAT4& center, float& radius);
	Frustum CalculateFrustumByNearFar(float near, float far);

private:
	void Update();
	
	XMFLOAT4X4	m_mProjection;	// Projection matrix
	XMFLOAT4X4	m_mView;			// View matrix
	XMFLOAT4	m_vPos;			// Camera position
	XMFLOAT4	m_vStartLook;	// Starting lookat vector
	XMFLOAT4	m_vStartUp;		// Starting up vector
	XMFLOAT4	m_vStartLeft;	// Starting left vector
	XMFLOAT4	m_vCurLook;		// Current lookat vector
	XMFLOAT4	m_vCurUp;		// Current up vector
	XMFLOAT4	m_vCurLeft;		// Current left vector
	int			m_wScreen;
	int			m_hScreen;
	float		m_fovHorizontal;
	float		m_fovVertical;
	float		m_angleYaw;
	float		m_anglePitch;
	float		m_angleRoll;
};

