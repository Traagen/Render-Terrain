/*
Terrain.h

Author:			Chris Serson
Last Edited:	July 6, 2016

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
	XMFLOAT4	frustum[6];
	float		scale;
	int			height;
	int			width;
};

struct Vertex {
	XMFLOAT3 position;
	XMFLOAT2 boundsZ;
	XMFLOAT2 tex;
};

class Terrain {
public:
	Terrain(Graphics* GFX);
	~Terrain();

	void Draw2D(ID3D12GraphicsCommandList* cmdList);
	void Draw3D(ID3D12GraphicsCommandList* cmdList, XMFLOAT4X4 vp, XMFLOAT4 eye);
	void DrawTess(ID3D12GraphicsCommandList* cmdList, XMFLOAT4X4 vp, XMFLOAT4 eye, XMFLOAT4 frustum[6]);

	// Once the GPU has completed uploading buffers to GPU memory, we need to free system memory.
	void ClearUnusedUploadBuffersAfterInit();

private:
	// prepare RootSig, PSO, Shaders, and Descriptor heaps for 2D render
	void PreparePipeline2D(Graphics *GFX);
	// prepare RootSig, PSO, Shaders, and Descriptor heaps for 3D render
	void PreparePipeline3D(Graphics *GFX);
	// prepare RootSig, PSO, and Shaders for render with tessellation. Uses same heaps as 3D.
	void PreparePipelineTess(Graphics *GFX);
	// generate vertex and index buffers for 3D mesh of terrain
	void CreateMesh3D(Graphics *GFX);
	// load the specified file containing the heightmap data.
	void LoadHeightMap(const char* fnHeightMap, const char* fnNormalMap);
	// loads the heightmap texture into memory
	void PrepareHeightmap(Graphics *GFX);
	void PrepareDetailMap(Graphics *GFX);
	void LoadDetailMap(const char* filename);
	// create a constant buffer to contain shader values
	void CreateConstantBuffer(Graphics *GFX);
	// calculate the minimum and maximum z values for vertices between the provide bounds.
	XMFLOAT2 CalcZBounds(Vertex topLeft, Vertex bottomRight);
	
	ID3D12PipelineState*		mpPSO2D;
	ID3D12PipelineState*		mpPSO3D;
	ID3D12PipelineState*		mpPSOTes;
	ID3D12RootSignature*		mpRootSig2D;
	ID3D12RootSignature*		mpRootSig3D;
	ID3D12RootSignature*		mpRootSigTes;
	ID3D12DescriptorHeap*		mpSRVHeap;			// Shader Resource View Heap
	ID3D12Resource*				mpHeightmap;
	ID3D12Resource*				mpUploadHeightmap;	// upload buffer for the heightmap.
	ID3D12Resource*				mpCBV;	
	ID3D12Resource*				mpVertexBuffer;
	ID3D12Resource*				mpUploadVB;
	ID3D12Resource*				mpIndexBuffer;
	ID3D12Resource*				mpUploadIB;
	D3D12_VERTEX_BUFFER_VIEW	mVBV;
	D3D12_INDEX_BUFFER_VIEW		mIBV;
	float*						maImage;
	float*						maDetail;
	unsigned int				mWidth;
	unsigned int				mHeight;
	unsigned int				mDetailWidth;
	unsigned int				mDetailHeight;
	unsigned int				mIndexCount;
	float						mHeightScale;
	ConstantBuffer				mCBData;
	UINT8*						mpCBVDataBegin;		// memory mapped to CBV
	UINT						mSRVDescSize;
	Vertex*						maVertices;			// buffer to contain vertex array prior to upload.
	UINT*						maIndices;			// buffer to contain index array prior to upload.
};

