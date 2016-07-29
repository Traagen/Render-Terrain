/*
Terrain.h

Author:			Chris Serson
Last Edited:	July 26, 2016

Description:	Class for loading a heightmap and rendering as a terrain.

Usage:			- Calling the constructor, either through Terrain T(...);
				or Terrain* T; T = new Terrain(...);, will load the
				heightmap texture and shaders needed to render the terrain.
				- Proper shutdown is handled by the destructor.
				- Is hard-coded for Direct3D 12.
				- Call Draw() and pass a Command List to load the set of
				commands necessary to render the terrain.

Future Work:	- Add a colour palette.
				- Add support for texturing.
				- Add shadowing.
*/
#pragma once

#include "Graphics.h"
#include <vector>

using namespace graphics;

struct Vertex {
	XMFLOAT3 position;
	XMFLOAT2 boundsZ;
	XMFLOAT2 tex;
};

class Terrain {
public:
	Terrain();
	~Terrain();

	void Draw(ID3D12GraphicsCommandList* cmdList, bool Draw3D = true);

	UINT GetSizeOfVertexBuffer() { return mVertexCount * sizeof(Vertex); }
	UINT GetSizeOfIndexBuffer() { return mIndexCount * sizeof(UINT); }
	UINT GetSizeOfHeightMap() { return mWidth * mDepth * sizeof(float); }
	UINT GetHeightMapWidth() { return mWidth; }
	UINT GetHeightMapDepth() { return mDepth; }
	float* GetHeightMapTextureData() { return maImage; }
	Vertex* GetVertexArray() { return maVertices; }
	UINT* GetIndexArray() { return maIndices; }
	float GetScale() { return mHeightScale; }
	
	void SetVertexBufferView(D3D12_VERTEX_BUFFER_VIEW vbv) { mVBV = vbv; }
	void SetIndexBufferView(D3D12_INDEX_BUFFER_VIEW ibv) { mIBV = ibv; }
	void SetHeightmapResource(ID3D12Resource* tex) { mpHeightmap = tex; }
	void SetVertexBufferResource(ID3D12Resource* vb) { mpVertexBuffer = vb; }
	void SetIndexBufferResource(ID3D12Resource* ib) { mpIndexBuffer = ib; }

	// Once the GPU has completed uploading buffers to GPU memory, we need to free system memory.
	void DeleteVertexAndIndexArrays();

private:
	// Generates an array of vertices and an array of indices.
	void CreateMesh3D();
	// load the specified file containing the heightmap data.
	void LoadHeightMap(const char* fnHeightMap);
	// calculate the minimum and maximum z values for vertices between the provide bounds.
	XMFLOAT2 CalcZBounds(Vertex topLeft, Vertex bottomRight);
	
	ID3D12Resource*				mpHeightmap;
	ID3D12Resource*				mpVertexBuffer;
	ID3D12Resource*				mpIndexBuffer;
	D3D12_VERTEX_BUFFER_VIEW	mVBV;
	D3D12_INDEX_BUFFER_VIEW		mIBV;
	float*						maImage;
	unsigned int				mWidth;
	unsigned int				mDepth;
	unsigned long				mVertexCount;
	unsigned long				mIndexCount;
	float						mHeightScale;
	Vertex*						maVertices;		// buffer to contain vertex array prior to upload.
	UINT*						maIndices;		// buffer to contain index array prior to upload.
};

