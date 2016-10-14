/*
Terrain.h

Author:			Chris Serson
Last Edited:	October 14, 2016

Description:	Class for loading height map and displacement map data
				with which to render terrain. Also contains a reference
				to a TerrainMaterial object containing diffuse and
				normal maps to texture the terrain with.

Usage:			- Proper shutdown is handled by the destructor.
				- Is hard-coded for Direct3D 12.
				- Call AttachTerrainResources() to load the appropriate 
					buffer/resource views to render the terrain.
				- Call AttachMaterialResources() to load the material
					SRVs in order to texture the terrain.
				- Call Draw() and pass a Command List to load the set of
				commands necessary to render the terrain.

Future Work:	- Add a colour palette.
				- Add bounding sphere code.
				- Change to dynamic mesh, ie geometry clipmapping or similar.
*/
#pragma once

#include "Graphics.h"
#include "Material.h"
#include "BoundingVolume.h"
#include <vector>

using namespace graphics;

struct Vertex {
	XMFLOAT3 position;
	XMFLOAT3 aabbmin;
	XMFLOAT3 aabbmax;
	UINT skirt;
};

struct TerrainShaderConstants {
	float scale;
	float width;
	float depth;
	float base;

	TerrainShaderConstants(float s, float w, float d, float b) : scale(s), width(w), depth(d), base(b) {}
};

class Terrain {
public:
	Terrain(ResourceManager* rm, TerrainMaterial* mat, const char* fnHeightmap, const char* fnDisplacementMap);
	~Terrain();

	void Draw(ID3D12GraphicsCommandList* cmdList, bool Draw3D = true);
	// Attach the resources needed for rendering terrain.
	// Requires the indices into the root descriptor table to attach the heightmap and displacement map SRVs and constant buffer CBV to.
	void AttachTerrainResources(ID3D12GraphicsCommandList* cmdList, unsigned int srvDescTableIndexHeightMap,
		unsigned int srvDescTableIndexDisplacementMap, unsigned int cbvDescTableIndex);
	// Attach the material resources. Requires root descriptor table index.
	void AttachMaterialResources(ID3D12GraphicsCommandList* cmdList, unsigned int srvDescTableIndex);

	BoundingSphere GetBoundingSphere() { return m_BoundingSphere; }
	float GetHeightAtPoint(float x, float y);
	
private:
	// Generates an array of vertices and an array of indices.
	void CreateMesh3D();
	// Create the vertex buffer view
	void CreateVertexBuffer();
	// Create the index buffer view
	void CreateIndexBuffer();
	// Create the constant buffer for terrain shader constants
	void CreateConstantBuffer();
	// load the specified file containing the heightmap data.
	void LoadHeightMap(const char* fnHeightMap);
	// load the specified file containing a displacement map used for smaller geometry detail.
	void LoadDisplacementMap(const char* fnMap);
	// calculate the minimum and maximum z values for vertices between the provide bounds.
	XMFLOAT2 CalcZBounds(Vertex topLeft, Vertex bottomRight);
	// Clean up array data
	void DeleteVertexAndIndexArrays();

	float GetHeightMapValueAtPoint(float x, float y);
	float GetDisplacementMapValueAtPoint(float x, float y);
	XMFLOAT3 CalculateNormalAtPoint(float x, float y);

	TerrainMaterial*			m_pMat;
	ResourceManager*			m_pResMgr;
	D3D12_VERTEX_BUFFER_VIEW	m_viewVertexBuffer;
	D3D12_INDEX_BUFFER_VIEW		m_viewIndexBuffer;
	D3D12_CPU_DESCRIPTOR_HANDLE m_hdlHeightMapSRV_CPU;
	D3D12_GPU_DESCRIPTOR_HANDLE m_hdlHeightMapSRV_GPU;
	D3D12_CPU_DESCRIPTOR_HANDLE m_hdlDisplacementMapSRV_CPU;
	D3D12_GPU_DESCRIPTOR_HANDLE m_hdlDisplacementMapSRV_GPU;
	D3D12_CPU_DESCRIPTOR_HANDLE m_hdlConstantsCBV_CPU;
	D3D12_GPU_DESCRIPTOR_HANDLE m_hdlConstantsCBV_GPU;
	unsigned char*				m_dataHeightMap;
	unsigned char*				m_dataDisplacementMap;
	unsigned int				m_wHeightMap;
	unsigned int				m_hHeightMap;
	unsigned int				m_wDisplacementMap;
	unsigned int				m_hDisplacementMap;
	float						m_hBase;
	unsigned long				m_numVertices;
	unsigned long				m_numIndices;
	float						m_scaleHeightMap;
	Vertex*						m_dataVertices;		// buffer to contain vertex array prior to upload.
	UINT*						m_dataIndices;		// buffer to contain index array prior to upload.
	TerrainShaderConstants*		m_pConstants;
	BoundingSphere				m_BoundingSphere;
};

