/*
Terrain.cpp

Author:			Chris Serson
Last Edited:	October 14, 2016

Description:	Class for loading a heightmap and rendering as a terrain.
*/
#include "lodepng.h"
#include "Terrain.h"
#include "Common.h"

Terrain::Terrain(ResourceManager* rm, TerrainMaterial* mat, const char* fnHeightmap, const char* fnDisplacementMap) : 
	m_pMat(mat), m_pResMgr(rm) {
	m_dataHeightMap = nullptr;
	m_dataDisplacementMap = nullptr;
	m_dataVertices = nullptr;
	m_dataIndices = nullptr;
	m_pConstants = nullptr;

	LoadHeightMap(fnHeightmap);
	LoadDisplacementMap(fnDisplacementMap);

	CreateMesh3D();
}

Terrain::~Terrain() {
	// The order resources are released appears to matter. I haven't tested all possible orders, but at least releasing the heap
	// and resources after the pso and rootsig was causing my GPU to hang on shutdown. Using the current order resolved that issue.
	m_dataHeightMap = nullptr;
	m_dataDisplacementMap = nullptr;

	DeleteVertexAndIndexArrays();

	m_pResMgr = nullptr;
	delete m_pMat;
}

void Terrain::Draw(ID3D12GraphicsCommandList* cmdList, bool Draw3D) {
	if (Draw3D) {
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST); // describe how to read the vertex buffer.
		cmdList->IASetVertexBuffers(0, 1, &m_viewVertexBuffer);
		cmdList->IASetIndexBuffer(&m_viewIndexBuffer);

		cmdList->DrawIndexedInstanced(m_numIndices, 1, 0, 0, 0);
	} else {
		// draw in 2D
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // describe how to read the vertex buffer.
		cmdList->DrawInstanced(3, 1, 0, 0);
	}
}

// Clean up array data.
void Terrain::DeleteVertexAndIndexArrays() {
	if (m_dataVertices) {
		delete[] m_dataVertices;
		m_dataVertices = nullptr;
	}

	if (m_dataIndices) {
		delete[] m_dataIndices;
		m_dataIndices = nullptr;
	}

	if (m_pConstants) {
		delete m_pConstants;
		m_pConstants = nullptr;
	}
}

// generate vertex and index buffers for 3D mesh of terrain
void Terrain::CreateMesh3D() {
	// Create a vertex buffer
	m_scaleHeightMap = (float)m_wHeightMap / 16.0f;
	int tessFactor = 8;
	int scalePatchX = m_wHeightMap / tessFactor;
	int scalePatchY = m_hHeightMap / tessFactor;
	int numVertsInTerrain = scalePatchX * scalePatchY;
	// number of vertices needed for terrain + base vertices for skirt.
	m_numVertices = numVertsInTerrain + scalePatchX * 4;

	// create a vertex array 1/4 the size of the height map in each dimension,
	// to be stretched over the height map
	int arrSize = (int)(m_numVertices);
	m_dataVertices = new Vertex[arrSize];
	for (int y = 0; y < scalePatchY; ++y) {
		for (int x = 0; x < scalePatchX; ++x) {
			m_dataVertices[y * scalePatchX + x].position = XMFLOAT3((float)x * tessFactor, (float)y * tessFactor, ((float)m_dataHeightMap[((y * m_wHeightMap + x) * 4) * tessFactor] / 255.0f) * m_scaleHeightMap);
			m_dataVertices[y * scalePatchX + x].skirt = 5;
		}
	}

	XMFLOAT2 zBounds = CalcZBounds(m_dataVertices[0], m_dataVertices[numVertsInTerrain - 1]);
	m_hBase = zBounds.x - 10;

	// create base vertices for side 1 of skirt. y = 0.
	int iVertex = numVertsInTerrain;
	for (int x = 0; x < scalePatchX; ++x) {
		m_dataVertices[iVertex].position = XMFLOAT3((float)(x * tessFactor), 0.0f, m_hBase);
		m_dataVertices[iVertex++].skirt = 1;
	}

	// create base vertices for side 2 of skirt. y = m_hHeightMap - tessFactor.
	for (int x = 0; x < scalePatchX; ++x) {
		m_dataVertices[iVertex].position = XMFLOAT3((float)(x * tessFactor), (float)(m_hHeightMap - tessFactor), m_hBase);
		m_dataVertices[iVertex++].skirt = 2;
	}

	// create base vertices for side 3 of skirt. x = 0.
	for (int y = 0; y < scalePatchY; ++y) {
		m_dataVertices[iVertex].position = XMFLOAT3(0.0f, (float)(y * tessFactor), m_hBase);
		m_dataVertices[iVertex++].skirt = 3;
	}

	// create base vertices for side 4 of skirt. x = m_wHeightMap - tessFactor.
	for (int y = 0; y < scalePatchY; ++y) {
		m_dataVertices[iVertex].position = XMFLOAT3((float)(m_wHeightMap - tessFactor), (float)(y * tessFactor), m_hBase);
		m_dataVertices[iVertex++].skirt = 4;
	}

	// create an index buffer
	// our grid is scalePatchX * scalePatchY in size.
	// the vertices are oriented like so:
	//  0,  1,  2,  3,  4,
	//  5,  6,  7,  8,  9,
	// 10, 11, 12, 13, 14
	// need to add indices for 4 edge skirts (2 x-aligned, 2 y-aligned, 4 indices per patch).
	// + 4 indices for patch representing bottom plane.
	arrSize = (scalePatchX - 1) * (scalePatchY - 1) * 4 + 2 * 4 * (scalePatchX - 1) + 2 * 4 * (scalePatchY - 1) + 4;
	m_dataIndices = new UINT[arrSize];
	int i = 0;
	for (int y = 0; y < scalePatchY - 1; ++y) {
		for (int x = 0; x < scalePatchX - 1; ++x) {
			UINT vert0 = x + y * scalePatchX;
			UINT vert1 = x + 1 + y * scalePatchX;
			UINT vert2 = x + (y + 1) * scalePatchX;
			UINT vert3 = x + 1 + (y + 1) * scalePatchX;
			m_dataIndices[i++] = vert0;
			m_dataIndices[i++] = vert1;
			m_dataIndices[i++] = vert2;
			m_dataIndices[i++] = vert3;
			
			// now that we have the indices for our patch, we need to calculate the bounding box.
			// z bounds is a bit harder as we need to find the max and min y values in the heightmap for the patch range.
			// store it in the first vertex
			// subtract one from coords of min and add one to coords of max to take into account
			// the offsets caused by displacement, which should always be between -1 and 1.
			XMFLOAT2 bz = CalcZBounds(m_dataVertices[vert0], m_dataVertices[vert3]);
			m_dataVertices[vert0].aabbmin = XMFLOAT3(m_dataVertices[vert0].position.x - 0.5f, m_dataVertices[vert0].position.y - 0.5f, bz.x - 0.5f);
			m_dataVertices[vert0].aabbmax = XMFLOAT3(m_dataVertices[vert3].position.x + 0.5f, m_dataVertices[vert3].position.y + 0.5f, bz.y + 0.5f);
		}
	}

	// so as not to interfere with the terrain wrt bounds for frustum culling, we need the 0th control point of each skirt patch to be a base vertex as defined above.
	// add indices for side 1 of skirt. y = 0.
	iVertex = numVertsInTerrain;
	for (int x = 0; x < scalePatchX - 1; ++x) {
		m_dataIndices[i++] = iVertex;		// control point 0
		m_dataIndices[i++] = iVertex + 1;	// control point 1
		m_dataIndices[i++] = x;				// control point 2
		m_dataIndices[i++] = x + 1;			// control point 3
		XMFLOAT2 bz = CalcZBounds(m_dataVertices[x], m_dataVertices[x + 1]);
		m_dataVertices[iVertex].aabbmin = XMFLOAT3((float)(x * tessFactor), 0.0f, m_hBase);
		m_dataVertices[iVertex++].aabbmax = XMFLOAT3((float)((x + 1) * tessFactor), 0.0f, bz.y);
	}
	// add indices for side 2 of skirt. y = m_hHeightMap - tessFactor.
	++iVertex;
	for (int x = 0; x < scalePatchX - 1; ++x) {
		m_dataIndices[i++] = iVertex + 1;
		m_dataIndices[i++] = iVertex;
		int offset = scalePatchX * (scalePatchY - 1);
		m_dataIndices[i++] = x + offset + 1;
		m_dataIndices[i++] = x + offset;
		XMFLOAT2 bz = CalcZBounds(m_dataVertices[x + offset], m_dataVertices[x + offset + 1]);
		m_dataVertices[++iVertex].aabbmin = XMFLOAT3((float)(x * tessFactor), (float)(m_hHeightMap - tessFactor), m_hBase);
		m_dataVertices[iVertex].aabbmax = XMFLOAT3((float)((x + 1) * tessFactor), (float)(m_hHeightMap - tessFactor), bz.y);
	}
	// add indices for side 3 of skirt. x = 0.
	++iVertex;
	for (int y = 0; y < scalePatchY - 1; ++y) {
		m_dataIndices[i++] = iVertex + 1;
		m_dataIndices[i++] = iVertex;
		m_dataIndices[i++] = (y + 1) * scalePatchX;
		m_dataIndices[i++] = y * scalePatchX;
		XMFLOAT2 bz = CalcZBounds(m_dataVertices[y * scalePatchX], m_dataVertices[(y + 1) * scalePatchX]);
		m_dataVertices[++iVertex].aabbmin = XMFLOAT3(0.0f, (float)(y * tessFactor), m_hBase);
		m_dataVertices[iVertex].aabbmax = XMFLOAT3(0.0f, (float)((y + 1) * tessFactor), bz.y);
	}
	// add indices for side 4 of skirt. x = m_wHeightMap - tessFactor.
	++iVertex;
	for (int y = 0; y < scalePatchY - 1; ++y) {
		m_dataIndices[i++] = iVertex;
		m_dataIndices[i++] = iVertex + 1;
		m_dataIndices[i++] = y * scalePatchX + scalePatchX - 1;
		m_dataIndices[i++] = (y + 1) * scalePatchX + scalePatchX - 1;
		XMFLOAT2 bz = CalcZBounds(m_dataVertices[y * scalePatchX + scalePatchX - 1], m_dataVertices[(y + 1) * scalePatchX + scalePatchX - 1]);
		m_dataVertices[iVertex].aabbmin = XMFLOAT3((float)(m_wHeightMap - tessFactor), (float)(y * tessFactor), m_hBase);
		m_dataVertices[iVertex++].aabbmax = XMFLOAT3((float)(m_wHeightMap - tessFactor), (float)((y + 1) * tessFactor), bz.y);
	}
	// add indices for bottom plane.
	m_dataIndices[i++] = numVertsInTerrain + scalePatchX - 1;
	m_dataIndices[i++] = numVertsInTerrain;
	m_dataIndices[i++] = numVertsInTerrain + scalePatchX + scalePatchX - 1;
	m_dataIndices[i++] = numVertsInTerrain + scalePatchX;
	m_dataVertices[numVertsInTerrain + scalePatchX - 1].aabbmin = XMFLOAT3(0.0f, 0.0f, m_hBase);
	m_dataVertices[numVertsInTerrain + scalePatchX - 1].aabbmax = XMFLOAT3((float)m_wHeightMap, (float)m_hHeightMap, m_hBase);
	m_dataVertices[numVertsInTerrain + scalePatchX - 1].skirt = 0;
	
	m_numIndices = arrSize;

	CreateVertexBuffer();
	CreateIndexBuffer();
	CreateConstantBuffer();

	// Create a bounding sphere for the height map.
	float w = (float)m_wHeightMap / 2.0f;
	float h = (float)m_hHeightMap / 2.0f;
	m_BoundingSphere.SetCenter(w, h, (zBounds.y + zBounds.x) / 2.0f);
	m_BoundingSphere.SetRadius(sqrtf(w * w + h * h));
}

// Create the vertex buffer view
void Terrain::CreateVertexBuffer() {
	// Create the vertex buffer
	ID3D12Resource* buffer;
	auto iBuffer = m_pResMgr->NewBuffer(buffer, &CD3DX12_RESOURCE_DESC::Buffer(m_numVertices * sizeof(Vertex)),
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, nullptr);
	buffer->SetName(L"Terrain Vertex Buffer");
	auto sizeofVertexBuffer = GetRequiredIntermediateSize(buffer, 0, 1);

	// prepare vertex data for upload.
	D3D12_SUBRESOURCE_DATA dataVB = {};
	dataVB.pData = m_dataVertices;
	dataVB.RowPitch = sizeofVertexBuffer;
	dataVB.SlicePitch = sizeofVertexBuffer;

	m_pResMgr->UploadToBuffer(iBuffer, 1, &dataVB, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	// create and save vertex buffer view to Terrain object.
	m_viewVertexBuffer = {};
	m_viewVertexBuffer.BufferLocation = buffer->GetGPUVirtualAddress();
	m_viewVertexBuffer.StrideInBytes = sizeof(Vertex);
	m_viewVertexBuffer.SizeInBytes = (UINT)sizeofVertexBuffer;
}

// Create the index buffer view
void Terrain::CreateIndexBuffer() {
	// Create the index buffer
	ID3D12Resource* buffer;
	auto iBuffer = m_pResMgr->NewBuffer(buffer, &CD3DX12_RESOURCE_DESC::Buffer(m_numIndices * sizeof(UINT)),
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
		D3D12_RESOURCE_STATE_INDEX_BUFFER, nullptr);
	buffer->SetName(L"Terrain Index Buffer");
	auto sizeofIndexBuffer = GetRequiredIntermediateSize(buffer, 0, 1);

	// prepare index data for upload.
	D3D12_SUBRESOURCE_DATA dataIB = {};
	dataIB.pData = m_dataIndices;
	dataIB.RowPitch = sizeofIndexBuffer;
	dataIB.SlicePitch = sizeofIndexBuffer;

	m_pResMgr->UploadToBuffer(iBuffer, 1, &dataIB, D3D12_RESOURCE_STATE_INDEX_BUFFER);

	// create and save index buffer view to Terrain object.
	m_viewIndexBuffer = {};
	m_viewIndexBuffer.BufferLocation = buffer->GetGPUVirtualAddress();
	m_viewIndexBuffer.Format = DXGI_FORMAT_R32_UINT;
	m_viewIndexBuffer.SizeInBytes = (UINT)sizeofIndexBuffer;
}

// Create the constant buffer for terrain shader constants
void Terrain::CreateConstantBuffer() {
	// Create the constant buffer
	ID3D12Resource* buffer;
	auto iBuffer = m_pResMgr->NewBuffer(buffer, &CD3DX12_RESOURCE_DESC::Buffer(sizeof(TerrainShaderConstants)),
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, nullptr);
	buffer->SetName(L"Terrain Shader Constants Buffer");
	auto sizeofBuffer = GetRequiredIntermediateSize(buffer, 0, 1);

	// prepare constant buffer data for upload.
	m_pConstants = new TerrainShaderConstants(m_scaleHeightMap, (float)m_wHeightMap, (float)m_hHeightMap, m_hBase);
	D3D12_SUBRESOURCE_DATA dataCB = {};
	dataCB.pData = m_pConstants;
	dataCB.RowPitch = sizeofBuffer;
	dataCB.SlicePitch = sizeofBuffer;

	m_pResMgr->UploadToBuffer(iBuffer, 1, &dataCB, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	// Create a constant buffer view.
	D3D12_CONSTANT_BUFFER_VIEW_DESC descCBV = {};
	descCBV.BufferLocation = buffer->GetGPUVirtualAddress();
	descCBV.SizeInBytes = (sizeof(TerrainShaderConstants) + 255) & ~255; // CB size is required to be 256-byte aligned.

	m_pResMgr->AddCBV(&descCBV, m_hdlConstantsCBV_CPU, m_hdlConstantsCBV_GPU);
}

// calculate the minimum and maximum z values for vertices between the provided bounds.
XMFLOAT2 Terrain::CalcZBounds(Vertex bottomLeft, Vertex topRight) {
	float max = -100000;
	float min = 100000;
	int bottomLeftX = bottomLeft.position.x == 0 ? (int)bottomLeft.position.x: (int)bottomLeft.position.x - 1;
	int bottomLeftY = bottomLeft.position.y == 0 ? (int)bottomLeft.position.y : (int)bottomLeft.position.y - 1;
	int topRightX = topRight.position.x >= m_wHeightMap ? (int)topRight.position.x : (int)topRight.position.x + 1;
	int topRightY = topRight.position.y >= m_wHeightMap ? (int)topRight.position.y : (int)topRight.position.y + 1;

	for (int y = bottomLeftY; y <= topRightY; ++y) {
		for (int x = bottomLeftX; x <= topRightX; ++x) {
			float z = ((float)m_dataHeightMap[(x + y * m_wHeightMap) * 4] / 255.0f) * m_scaleHeightMap;

			if (z > max) max = z;
			if (z < min) min = z;
		}
	}

	return XMFLOAT2(min, max);
}

// load the specified file containing the heightmap data.
void Terrain::LoadHeightMap(const char* fnHeightMap) {
	unsigned int index;
	index = m_pResMgr->LoadFile(fnHeightMap, m_hHeightMap, m_wHeightMap);
	m_dataHeightMap = m_pResMgr->GetFileData(index);

	// Create the texture buffers.
	D3D12_RESOURCE_DESC	descTex = {};
	descTex.MipLevels = 1;
	descTex.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	descTex.Width = m_wHeightMap;
	descTex.Height = m_hHeightMap;
	descTex.Flags = D3D12_RESOURCE_FLAG_NONE;
	descTex.DepthOrArraySize = 1;
	descTex.SampleDesc.Count = 1;
	descTex.SampleDesc.Quality = 0;
	descTex.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	
	ID3D12Resource* hm;
	unsigned int iBuffer = m_pResMgr->NewBuffer(hm, &descTex, &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr);
	hm->SetName(L"Height Map");

	// prepare height map data for upload.
	D3D12_SUBRESOURCE_DATA dataTex = {};
	dataTex.pData = m_dataHeightMap;
	dataTex.RowPitch = m_wHeightMap * 4 * sizeof(unsigned char);
	dataTex.SlicePitch = m_hHeightMap * m_wHeightMap * 4 * sizeof(unsigned char);	
	
	m_pResMgr->UploadToBuffer(iBuffer, 1, &dataTex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	// Create the SRV for the detail map texture and save to Terrain object.
	D3D12_SHADER_RESOURCE_VIEW_DESC	descSRV = {};
	descSRV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	descSRV.Format = descTex.Format;
	descSRV.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	descSRV.Texture2D.MipLevels = 1;

	m_pResMgr->AddSRV(hm, &descSRV, m_hdlHeightMapSRV_CPU, m_hdlHeightMapSRV_GPU);
}

void Terrain::LoadDisplacementMap(const char* fnMap) {
	unsigned int index;
	index = m_pResMgr->LoadFile(fnMap, m_hDisplacementMap, m_wDisplacementMap);
	m_dataDisplacementMap = m_pResMgr->GetFileData(index);

	// Create the texture buffers.
	D3D12_RESOURCE_DESC	descTex = {};
	descTex.MipLevels = 1;
	descTex.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	descTex.Width = m_wDisplacementMap;
	descTex.Height = m_hDisplacementMap;
	descTex.Flags = D3D12_RESOURCE_FLAG_NONE;
	descTex.DepthOrArraySize = 1;
	descTex.SampleDesc.Count = 1;
	descTex.SampleDesc.Quality = 0;
	descTex.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	ID3D12Resource* dm;
	unsigned int iBuffer = m_pResMgr->NewBuffer(dm, &descTex, &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr);
	dm->SetName(L"Displacement Map");

	D3D12_SUBRESOURCE_DATA dataTex = {};
	// prepare height map data for upload.
	dataTex.pData = m_dataDisplacementMap;
	dataTex.RowPitch = m_wDisplacementMap * 4 * sizeof(unsigned char);
	dataTex.SlicePitch = m_hDisplacementMap * m_wDisplacementMap * 4 * sizeof(unsigned char);

	m_pResMgr->UploadToBuffer(iBuffer, 1, &dataTex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	// Create the SRV for the detail map texture and save to Terrain object.
	D3D12_SHADER_RESOURCE_VIEW_DESC	descSRV = {};
	descSRV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	descSRV.Format = descTex.Format;
	descSRV.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	descSRV.Texture2D.MipLevels = 1;

	m_pResMgr->AddSRV(dm, &descSRV, m_hdlDisplacementMapSRV_CPU, m_hdlDisplacementMapSRV_GPU);
}

// Attach the resources needed for rendering terrain.
// Requires the indices into the root descriptor table to attach the heightmap and displacement map SRVs and constant buffer CBV to.
void Terrain::AttachTerrainResources(ID3D12GraphicsCommandList* cmdList, unsigned int srvDescTableIndexHeightMap,
	unsigned int srvDescTableIndexDisplacementMap, unsigned int cbvDescTableIndex) {
	cmdList->SetGraphicsRootDescriptorTable(srvDescTableIndexHeightMap, m_hdlHeightMapSRV_GPU);
	cmdList->SetGraphicsRootDescriptorTable(srvDescTableIndexDisplacementMap, m_hdlDisplacementMapSRV_GPU);
	cmdList->SetGraphicsRootDescriptorTable(cbvDescTableIndex, m_hdlConstantsCBV_GPU);
}

// Attach the material resources. Requires root descriptor table index.
void Terrain::AttachMaterialResources(ID3D12GraphicsCommandList* cmdList, unsigned int srvDescTableIndex) {
	m_pMat->Attach(cmdList, srvDescTableIndex);
}

float Terrain::GetHeightMapValueAtPoint(float x, float y) {
	// use bilinear interpolation to calculate the height of the base terrain at point (x, y).
	float x1 = floorf(x);
	x1 = x1 < 0.0f ? 0.0f : x1 > m_wHeightMap ? m_wHeightMap : x1;
	float x2 = ceilf(x);
	x2 = x2 < 0.0f ? 0.0f : x2 > m_wHeightMap ? m_wHeightMap : x2;
	float dx = x - x1;
	float y1 = floorf(y);
	y1 = y1 < 0.0f ? 0.0f : y1 > m_hHeightMap ? m_hHeightMap : y1;
	float y2 = ceilf(y);
	y2 = y2 < 0.0f ? 0.0f : y2 > m_hHeightMap ? m_hHeightMap : y2;
	float dy = y - y1;
	
	float a = (float)m_dataHeightMap[(int)(y1 * m_wHeightMap + x1) * 4] / 255.0f;
	float b = (float)m_dataHeightMap[(int)(y1 * m_wHeightMap + x2) * 4] / 255.0f;
	float c = (float)m_dataHeightMap[(int)(y2 * m_wHeightMap + x1) * 4] / 255.0f;
	float d = (float)m_dataHeightMap[(int)(y2 * m_wHeightMap + x2) * 4] / 255.0f;

	return bilerp(a, b, c, d, dx, dy);
}

float Terrain::GetDisplacementMapValueAtPoint(float x, float y) {
	float _x = x / m_wDisplacementMap / 32;
	float _y = y / m_hDisplacementMap / 32;
	// use bilinear interpolation to calculate the height of the base terrain at point (x, y).
	float x1 = floorf(_x);
	float x2 = ceilf(_x);
	float dx = _x - x1;
	float y1 = floorf(_y);
	float y2 = ceilf(_y);
	float dy = _y - y1;

	float a = (float)m_dataDisplacementMap[(int)(y1 * m_wDisplacementMap + x1) * 4 + 3] / 255.0f;
	float b = (float)m_dataDisplacementMap[(int)(y1 * m_wDisplacementMap + x2) * 4 + 3] / 255.0f;
	float c = (float)m_dataDisplacementMap[(int)(y2 * m_wDisplacementMap + x1) * 4 + 3] / 255.0f;
	float d = (float)m_dataDisplacementMap[(int)(y2 * m_wDisplacementMap + x2) * 4 + 3] / 255.0f;

	return bilerp(a, b, c, d, dx, dy);
}

XMFLOAT3 Terrain::CalculateNormalAtPoint(float x, float y) {
	XMFLOAT2 b(x, y - 0.3f / m_hHeightMap);
	XMFLOAT2 c(x + 0.3f / m_wHeightMap, y - 0.3f / m_hHeightMap);
	XMFLOAT2 d(x + 0.3f / m_wHeightMap, y);
	XMFLOAT2 e(x + 0.3f / m_wHeightMap, y + 0.3f / m_hHeightMap);
	XMFLOAT2 f(x, y +  0.3f / m_hHeightMap);
	XMFLOAT2 g(x - 0.3f / m_wHeightMap, y + 0.3f / m_hHeightMap);
	XMFLOAT2 h(x - 0.3f / m_wHeightMap, y);
	XMFLOAT2 i(x - 0.3f / m_wHeightMap, y - 0.3f / m_hHeightMap);

	float zb = GetHeightMapValueAtPoint(b.x, b.y) * m_scaleHeightMap;
	float zc = GetHeightMapValueAtPoint(c.x, c.y) * m_scaleHeightMap;
	float zd = GetHeightMapValueAtPoint(d.x, d.y) * m_scaleHeightMap;
	float ze = GetHeightMapValueAtPoint(e.x, e.y) * m_scaleHeightMap;
	float zf = GetHeightMapValueAtPoint(f.x, f.y) * m_scaleHeightMap;
	float zg = GetHeightMapValueAtPoint(g.x, g.y) * m_scaleHeightMap;
	float zh = GetHeightMapValueAtPoint(h.x, h.y) * m_scaleHeightMap;
	float zi = GetHeightMapValueAtPoint(i.x, i.y) * m_scaleHeightMap;

	float u = zg + 2 * zh + zi - zc - 2 * zd - ze;
	float v = 2 * zb + zc + zi - ze - 2 * zf - zg;
	float w = 8.0f;

	XMFLOAT3 norm(u, v, w);
	XMVECTOR normalized = XMVector3Normalize(XMLoadFloat3(&norm));
	XMStoreFloat3(&norm, normalized);

	return norm;
}

float Terrain::GetHeightAtPoint(float x, float y) {
	float z = GetHeightMapValueAtPoint(x, y) * m_scaleHeightMap;
	float d = 2.0f * GetDisplacementMapValueAtPoint(x, y) - 1.0f;
	XMFLOAT3 norm = CalculateNormalAtPoint(x, y);
	XMFLOAT3 pos(x, y, z);
	XMVECTOR normal = XMLoadFloat3(&norm);
	XMVECTOR position = XMLoadFloat3(&pos);
	XMVECTOR posFinal = position + normal * 0.5f * d;
	XMFLOAT3 fp;
	XMStoreFloat3(&fp, posFinal);

	return fp.z;
}