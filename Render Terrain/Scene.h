/*
Scene.h

Author:			Chris Serson
Last Edited:	June 26, 2016

Description:	Class for creating, managing, and rendering a scene.

Usage:			- Calling the constructor, either through Scene S(...);
				or Scene* S; S = new Scene(...);, will initialize
				the scene.
				- Proper shutdown is handled by the destructor.
				- Requires a pointer to a Graphics object be passed in.
				- Is hard-coded for Direct3D 12.
				- Call Draw() in the main loop to render the scene.
				- Currently can only draw terrain in 2D or 3D depending on hard-coded choice.
				
Future Work:	- Add support for multi-threaded rendering.
				- Add camera move and mouse-look support.
				- Add ability to swap between 2D and 3D.
*/
#pragma once

#include "Terrain.h"
#include "Camera.h"

using namespace graphics;

class Scene {
public:
	Scene(int height, int width, Graphics* GFX);
	~Scene();

	void Draw();

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
};

