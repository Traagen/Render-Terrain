/*
Terrain.h

Author:			Chris Serson
Last Edited:	June 23, 2016

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

struct ConstantBuffer {
	XMFLOAT4X4	viewproj;
	int			height;
	int			width;
};

class Terrain {
public:
	Terrain(Graphics* GFX);
	~Terrain();

	void Draw2D(ID3D12GraphicsCommandList* cmdList);
	void Draw3D(ID3D12GraphicsCommandList* cmdList);

	void SetViewProjectionMatrixTransposed(XMFLOAT4X4 vp) { mmViewProjTrans = vp; }

private:
	// prepare RootSig, PSO, Shaders, and Descriptor heaps for 2D render
	void PreparePipeline2D(Graphics *GFX);
	// prepare RootSig, PSO, Shaders, and Descriptor heaps for 3D render
	void PreparePipeline3D(Graphics *GFX);
	// load the specified file containing the heightmap data.
	void LoadHeightMap(const char* filename);
	// loads the heightmap texture into memory
	void PrepareHeightmap(Graphics *GFX);
	// create a constant buffer to contain shader values
	void CreateConstantBuffer(Graphics *GFX);
	
	ID3D12PipelineState*	mpPSO2D;
	ID3D12PipelineState*	mpPSO3D;
	ID3D12RootSignature*	mpRootSig2D;
	ID3D12RootSignature*	mpRootSig3D;
	ID3D12DescriptorHeap*	mpSRVHeap;			// Shader Resource View Heap
	ID3D12Resource*			mpHeightmap;
	ID3D12Resource*			mpUpload;			// upload buffer for the heightmap.
	ID3D12Resource*			mpCBV;	
	unsigned char*			maImage;
	unsigned int			mWidth;
	unsigned int			mHeight;
	XMFLOAT4X4				mmViewProjTrans;	// combined view/projection matrix. Assumed to already be transposed
	ConstantBuffer			mCBData;
	UINT8*					mpCBVDataBegin;		// memory mapped to CBV
	UINT					mSRVDescSize;
};

