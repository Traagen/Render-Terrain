/*
Scene.h

Author:			Chris Serson
Last Edited:	July 26, 2016

Description:	Class for creating, managing, and rendering a scene.

Usage:			- Calling the constructor, either through Scene S(...);
				or Scene* S; S = new Scene(...);, will initialize
				the scene.
				- Proper shutdown is handled by the destructor.
				- Requires a pointer to a Graphics object be passed in.
				- Is hard-coded for Direct3D 12.
				- Call Draw() in the main loop to render the scene.
				- Pressing 1 or 2 toggles between 2D and 3D rendering.
				
Future Work:	- Add support for multi-threaded rendering.
				
*/
#pragma once

#include "Terrain.h"
#include "Camera.h"
#include "DayNightCycle.h"

using namespace graphics;

enum InputKeys { _0 = 0x30, _1, _2, _3, _4, _5, _6, _7, _8, _9, _A = 0x41, _B, _C, _D, _E, _F, _G, _H, _I, _J, _K, _L, _M, _N, _O, _P, _Q, _R, _S, _T, _U, _V, _W, _X, _Y, _Z };
#define MOVE_STEP 1.0f
#define ROT_ANGLE 0.75f

struct PerFrameConstantBuffer {
	XMFLOAT4X4	viewproj;
	XMFLOAT4	eye;
	XMFLOAT4	frustum[6];
	LightSource light;
};

struct TerrainShaderConstants {
	float scale;
	float width;
	float depth;

	TerrainShaderConstants(float s, float w, float d) : scale(s), width(w), depth(d) {}
};

class Scene {
public:
	Scene(int height, int width, Graphics* GFX);
	~Scene();

	void Update();
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

	// Initialize a SRV/CBV heap. Currently hard-coded for 2 descriptors, the per-frame constant buffer and the terrain's heightmap texture.
	void InitSRVCBVHeap();
	// Initialize the per-frame constant buffer.
	void InitPerFrameConstantBuffer();
	
	// Initialize the root signature and pipeline state object for rendering the terrain in 2D.
	void InitPipelineTerrain2D();
	// Initialize the root signature and pipeline state object for rendering the terrain in 3D.
	void InitPipelineTerrain3D();
	// Initialize all of the resources needed by the terrain: heightmap texture, vertex buffer, index buffer.
	void InitTerrainResources();

	void CleanupTemporaryBuffers();
	
	void DrawTerrain(ID3D12GraphicsCommandList* cmdList);

	Graphics*							mpGFX;
	Terrain								T;
	Camera								C;
	DayNightCycle						DNC;
	D3D12_VIEWPORT						mViewport;
	D3D12_RECT							mScissorRect;
	int									mDrawMode;
	UINT								msizeofDescHeapIncrement;
	std::vector<ID3D12RootSignature*>	mlRootSigs;
	std::vector<ID3D12PipelineState*>	mlPSOs;
	std::vector<ID3D12DescriptorHeap*>	mlDescriptorHeaps;
	std::vector<ID3D12Resource*>		mlTemporaryUploadBuffers;
	ID3D12Resource*						mpPerFrameConstants;
	PerFrameConstantBuffer*				mpPerFrameConstantsMapped;
};

