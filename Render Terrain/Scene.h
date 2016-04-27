#pragma once

#include "Terrain.h"

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

	Graphics*					mpGFX;
	/* The Scene will need it's own pointer to the command list so it can send commands to the gpu.
	   The command list must be given to it by the Graphics object on init. */
	ID3D12GraphicsCommandList*	mpCmdList;	
	Terrain						T;
	D3D12_VIEWPORT				mViewport;
	D3D12_RECT					mScissorRect;
};

