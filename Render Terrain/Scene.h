/*
Scene.h

Author:			Chris Serson
Last Edited:	October 12, 2016

Description:	Class for creating, managing, and rendering a scene.

Usage:			- Proper shutdown is handled by the destructor.
				- Requires a pointer to a Device object be passed in.
				- Is hard-coded for Direct3D 12.
				- Call Update() in the main loop to render the scene.
				- Press T to toggle between textured or coloured.
				- Press 1 for 2D view.
				- Press 2 for 3D view.
				
Future Work:	- Add support for multi-threaded rendering.
				- Add support for mip-maps.
				- Add sky box.
				- Add atmospheric scattering.
				- Add dynamic terrain mesh, ie geometry clipmapping.
				- Add support for loading multiple terrains.
				- Add support for other objects.
				- Add support to lock camera to terrain.
*/
#pragma once

#include "Frame.h"
#include "ResourceManager.h"
#include "Terrain.h"
#include "Camera.h"
#include "DayNightCycle.h"

using namespace graphics;

enum InputKeys { _0 = 0x30, _1, _2, _3, _4, _5, _6, _7, _8, _9, _A = 0x41, _B, _C, _D, _E, _F, _G, _H, _I, _J, _K, _L, _M, _N, _O, _P, _Q, _R, _S, _T, _U, _V, _W, _X, _Y, _Z };
#define MOVE_STEP 1.0f
#define ROT_ANGLE 0.75f

static const int FRAME_BUFFER_COUNT = 3; // triple buffering.

class Scene {
public:
	Scene(int height, int width, Device* DEV);
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
	void SetViewport(ID3D12GraphicsCommandList* cmdList);

	// Initialize the root signature and pipeline state object for rendering the terrain in 2D.
	void InitPipelineTerrain2D();
	// Initialize the root signature and pipeline state object for rendering the terrain in 3D.
	void InitPipelineTerrain3D();
	// Initialize the root signature and pipeline state object for rendering to the shadow map.
	void InitPipelineShadowMap();
	// Draw the terrain in both 3D and 2D
	void DrawTerrain(ID3D12GraphicsCommandList* cmdList);
	// Render the shadow map
	void DrawShadowMap(ID3D12GraphicsCommandList* cmdList);

	Device*								m_pDev;
	ResourceManager						m_ResMgr;
	Frame*								m_pFrames[::FRAME_BUFFER_COUNT];
	ID3D12GraphicsCommandList*			m_pCmdList;
	Terrain*							m_pT;
	Camera								m_Cam;
	DayNightCycle						m_DNC;
	D3D12_VIEWPORT						m_vpMain;
	D3D12_RECT							m_srMain;
	int									m_drawMode = 0;
	std::vector<ID3D12RootSignature*>	m_listRootSigs;
	std::vector<ID3D12PipelineState*>	m_listPSOs;
	int									m_iFrame = 0;
	bool								m_UseTextures = false;
};

