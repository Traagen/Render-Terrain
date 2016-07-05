/*
Scene.h

Author:			Chris Serson
Last Edited:	July 2, 2016

Description:	Class for creating, managing, and rendering a scene.

Usage:			- Calling the constructor, either through Scene S(...);
				or Scene* S; S = new Scene(...);, will initialize
				the scene.
				- Proper shutdown is handled by the destructor.
				- Requires a pointer to a Graphics object be passed in.
				- Is hard-coded for Direct3D 12.
				- Call Draw() in the main loop to render the scene.
				- Pressing T toggles between 2D and 3D rendering.
				
Future Work:	- Add support for multi-threaded rendering.
				
*/
#pragma once

#include "Terrain.h"
#include "Camera.h"

using namespace graphics;

enum InputKeys { _A = 0x41, _B, _C, _D, _E, _F, _G, _H, _I, _J, _K, _L, _M, _N, _O, _P, _Q, _R, _S, _T, _U, _V, _W, _X, _Y, _Z };
#define MOVE_STEP 5.0f
#define ROT_ANGLE 0.75f

class Scene {
public:
	Scene(int height, int width, Graphics* GFX);
	~Scene();

	void Draw();
	// function allowing the main program to pass keyboard input to the scene.
	void HandleKeyboardInput(UINT key);
	// function allowing the main program to pass mouse input to the scene.
	void HandleMouseInput(int x, int y);

private:
	// Close all command lists. Currently there is only the one.
	void CloseCommandLists();
	// Set the viewport and scissor rectangle for the scene.
	void SetViewport();

	Graphics*		mpGFX;
	Terrain			T;
	Camera			C;
	D3D12_VIEWPORT	mViewport;
	D3D12_RECT		mScissorRect;
	bool			mDraw2D = true;
};

