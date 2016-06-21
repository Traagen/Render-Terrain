/*
Terrain.h

Author:			Chris Serson
Last Edited:	June 21, 2016

Description:	Class for loading a heightmap and rendering as a terrain.
				Currently renders only as 2D texture view.

Usage:			- Calling the constructor, either through Terrain T(...);
				or Terrain* T; T = new Terrain(...);, will load the
				heightmap texture and shaders needed to render the terrain.
				- Proper shutdown is handled by the destructor.
				- Requires a pointer to a Graphics object be passed in.
				- Is hard-coded for Direct3D 12.
				- Call Draw() and pass a Command List to load the set of
				commands necessary to render the terrain.

Future Work:	- Add support for 3D rendering.
				- Add a colour palette.
				- Add support for texturing.
				- Add tessellation support.

*/
#pragma once

#include "Graphics.h"
#include <vector>

using namespace graphics;

class Terrain {
public:
	Terrain(Graphics* GFX);
	~Terrain();

	void Draw(ID3D12GraphicsCommandList* cmdList);
private:
	// load the specified file containing the heightmap data.
	void LoadHeightMap(const char* filename);

	ID3D12PipelineState*		mpPSO;
	ID3D12RootSignature*		mpRootSig;
	ID3D12Resource*				mpHeightmap;
	ID3D12Resource*				mpUpload;	// upload buffer for the heightmap.
	ID3D12DescriptorHeap*		mpSRVHeap;	// Shader Resource View Heap
	std::vector<unsigned char>	mvImage;	// the raw pixels
	unsigned int				mWidth;
	unsigned int				mHeight;
};

