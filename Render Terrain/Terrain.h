/*
Terrain.h

Author:			Chris Serson
Last Edited:	July 1, 2016

Description:	Class for loading a heightmap and rendering as a terrain.

Usage:			- Calling the constructor, either through Terrain T(...);
				or Terrain* T; T = new Terrain(...);, will load the
				heightmap texture and shaders needed to render the terrain.
				- Proper shutdown is handled by the destructor.
				- Requires a pointer to a Graphics object be passed in.
				- Is hard-coded for Direct3D 12.
				- Call Draw() and pass a Command List to load the set of
				commands necessary to render the terrain.

Future Work:	- Add a colour palette.
				- Add support for texturing.
				- Add tessellation support.
				- Add shadowing.
*/
#pragma once

#include "Graphics.h"
#include <vector>

using namespace graphics;

struct ConstantBuffer {
	XMFLOAT4X4	viewproj;
	XMFLOAT4	eye;
	int			height;
	int			width;
};

struct Vertex {
	XMFLOAT3 position;
};

class Terrain {
public:
	Terrain(Graphics* GFX);
	~Terrain();

	void Draw2D(ID3D12GraphicsCommandList* cmdList);
	void Draw3D(ID3D12GraphicsCommandList* cmdList, XMFLOAT4X4 vp, XMFLOAT4 eye);

	// Once the GPU has completed uploading buffers to GPU memory, we need to free system memory.
	void ClearUnusedUploadBuffersAfterInit();

private:
	// prepare RootSig, PSO, Shaders, and Descriptor heaps for 2D render
	void PreparePipeline2D(Graphics *GFX);
	// prepare RootSig, PSO, Shaders, and Descriptor heaps for 3D render
	void PreparePipeline3D(Graphics *GFX);
	// generate vertex and index buffers for 3D mesh of terrain
	void CreateMesh3D(Graphics *GFX);
	// load the specified file containing the heightmap data.
	void LoadHeightMap(const char* filename);
	// loads the heightmap texture into memory
	void PrepareHeightmap(Graphics *GFX);
	// create a constant buffer to contain shader values
	void CreateConstantBuffer(Graphics *GFX);
	
	ID3D12PipelineState*		mpPSO2D;
	ID3D12PipelineState*		mpPSO3D;
	ID3D12RootSignature*		mpRootSig2D;
	ID3D12RootSignature*		mpRootSig3D;
	ID3D12DescriptorHeap*		mpSRVHeap;			// Shader Resource View Heap
	ID3D12Resource*				mpHeightmap;
	ID3D12Resource*				mpUploadHeightmap;			// upload buffer for the heightmap.
	ID3D12Resource*				mpCBV;	
	ID3D12Resource*				mpVertexBuffer;
	ID3D12Resource*				mpUploadVB;
	ID3D12Resource*				mpIndexBuffer;
	ID3D12Resource*				mpUploadIB;
	D3D12_VERTEX_BUFFER_VIEW	mVBV;
	D3D12_INDEX_BUFFER_VIEW		mIBV;
	unsigned char*				maImage;
	unsigned int				mWidth;
	unsigned int				mHeight;
	unsigned int				mIndexCount;
	ConstantBuffer				mCBData;
	UINT8*						mpCBVDataBegin;		// memory mapped to CBV
	UINT						mSRVDescSize;
};

