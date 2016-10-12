/*
Terrain.cpp

Author:			Chris Serson
Last Edited:	October 12, 2016

Description:	Class for loading a heightmap and rendering as a terrain.
*/
#include "lodepng.h"
#include "Terrain.h"

Terrain::Terrain(ResourceManager* rm, TerrainMaterial* mat, const char* fnHeightmap, const char* fnDisplacementMap) : 
	m_pMat(mat), m_pResMgr(rm) {
	maImage = nullptr;
	maDispImage = nullptr;
	maVertices = nullptr;
	maIndices = nullptr;
	m_pConstants = nullptr;

	LoadHeightMap(fnHeightmap);
	LoadDisplacementMap(fnDisplacementMap);

	CreateMesh3D();
}

Terrain::~Terrain() {
	// The order resources are released appears to matter. I haven't tested all possible orders, but at least releasing the heap
	// and resources after the pso and rootsig was causing my GPU to hang on shutdown. Using the current order resolved that issue.
	maImage = nullptr;
	maDispImage = nullptr;

	DeleteVertexAndIndexArrays();

	m_pResMgr = nullptr;
	delete m_pMat;
}

void Terrain::Draw(ID3D12GraphicsCommandList* cmdList, bool Draw3D) {
	if (Draw3D) {
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST); // describe how to read the vertex buffer.
		cmdList->IASetVertexBuffers(0, 1, &mVBV);
		cmdList->IASetIndexBuffer(&mIBV);

		cmdList->DrawIndexedInstanced(mIndexCount, 1, 0, 0, 0);
	} else {
		// draw in 2D
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // describe how to read the vertex buffer.
		cmdList->DrawInstanced(3, 1, 0, 0);
	}
}

// Once the GPU has completed uploading buffers to GPU memory, we need to free system memory.
void Terrain::DeleteVertexAndIndexArrays() {
	if (maVertices) {
		delete[] maVertices;
		maVertices = nullptr;
	}

	if (maIndices) {
		delete[] maIndices;
		maIndices = nullptr;
	}

	if (m_pConstants) {
		delete m_pConstants;
		m_pConstants = nullptr;
	}
}

// generate vertex and index buffers for 3D mesh of terrain
void Terrain::CreateMesh3D() {
	// Create a vertex buffer
	mHeightScale = (float)mWidth / 4.0f;
	int tessFactor = 8;
	int scalePatchX = mWidth / tessFactor;
	int scalePatchY = mDepth / tessFactor;
	int numVertsInTerrain = scalePatchX * scalePatchY;
	// number of vertices needed for terrain + base vertices for skirt.
	mVertexCount = numVertsInTerrain + scalePatchX * 4;

	// create a vertex array 1/4 the size of the height map in each dimension,
	// to be stretched over the height map
	int arrSize = (int)(mVertexCount);
	maVertices = new Vertex[arrSize];
	for (int y = 0; y < scalePatchY; ++y) {
		for (int x = 0; x < scalePatchX; ++x) {
			maVertices[y * scalePatchX + x].position = XMFLOAT3((float)x * tessFactor, (float)y * tessFactor, ((float)maImage[((y * mWidth + x) * 4) * tessFactor] / 255.0f) * mHeightScale);
			maVertices[y * scalePatchX + x].skirt = 5;
		}
	}

	mBaseHeight = (int)(CalcZBounds(maVertices[0], maVertices[numVertsInTerrain - 1]).x - 10);

	// create base vertices for side 1 of skirt. y = 0.
	int iVertex = numVertsInTerrain;
	for (int x = 0; x < scalePatchX; ++x) {
		maVertices[iVertex].position = XMFLOAT3(x * tessFactor, 0.0f, mBaseHeight);
		maVertices[iVertex++].skirt = 1;
	}

	// create base vertices for side 2 of skirt. y = mDepth - tessFactor.
	for (int x = 0; x < scalePatchX; ++x) {
		maVertices[iVertex].position = XMFLOAT3(x * tessFactor, mDepth - tessFactor, mBaseHeight);
		maVertices[iVertex++].skirt = 2;
	}

	// create base vertices for side 3 of skirt. x = 0.
	for (int y = 0; y < scalePatchY; ++y) {
		maVertices[iVertex].position = XMFLOAT3(0.0f, y * tessFactor, mBaseHeight);
		maVertices[iVertex++].skirt = 3;
	}

	// create base vertices for side 4 of skirt. x = mWidth - tessFactor.
	for (int y = 0; y < scalePatchY; ++y) {
		maVertices[iVertex].position = XMFLOAT3(mWidth - tessFactor, y * tessFactor, mBaseHeight);
		maVertices[iVertex++].skirt = 4;
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
	maIndices = new UINT[arrSize];
	int i = 0;
	for (int y = 0; y < scalePatchY - 1; ++y) {
		for (int x = 0; x < scalePatchX - 1; ++x) {
			UINT vert0 = x + y * scalePatchX;
			UINT vert1 = x + 1 + y * scalePatchX;
			UINT vert2 = x + (y + 1) * scalePatchX;
			UINT vert3 = x + 1 + (y + 1) * scalePatchX;
			maIndices[i++] = vert0;
			maIndices[i++] = vert1;
			maIndices[i++] = vert2;
			maIndices[i++] = vert3;
			
			// now that we have the indices for our patch, we need to calculate the bounding box.
			// z bounds is a bit harder as we need to find the max and min y values in the heightmap for the patch range.
			// store it in the first vertex
			// subtract one from coords of min and add one to coords of max to take into account
			// the offsets caused by displacement, which should always be between -1 and 1.
			XMFLOAT2 bz = CalcZBounds(maVertices[vert0], maVertices[vert3]);
			maVertices[vert0].aabbmin = XMFLOAT3(maVertices[vert0].position.x - 0.5f, maVertices[vert0].position.y - 0.5f, bz.x - 0.5f);
			maVertices[vert0].aabbmax = XMFLOAT3(maVertices[vert3].position.x + 0.5f, maVertices[vert3].position.y + 0.5f, bz.y + 0.5f);
		}
	}

	// so as not to interfere with the terrain wrt bounds for frustum culling, we need the 0th control point of each skirt patch to be a base vertex as defined above.
	// add indices for side 1 of skirt. y = 0.
	iVertex = numVertsInTerrain;
	for (int x = 0; x < scalePatchX - 1; ++x) {
		maIndices[i++] = iVertex;		// control point 0
		maIndices[i++] = iVertex + 1;	// control point 1
		maIndices[i++] = x;				// control point 2
		maIndices[i++] = x + 1;			// control point 3
		XMFLOAT2 bz = CalcZBounds(maVertices[x], maVertices[x + 1]);
		maVertices[iVertex].aabbmin = XMFLOAT3(x * tessFactor, 0.0f, mBaseHeight);
		maVertices[iVertex++].aabbmax = XMFLOAT3((x + 1) * tessFactor, 0.0f, bz.y);
	}
	// add indices for side 2 of skirt. y = mDepth - tessFactor.
	++iVertex;
	for (int x = 0; x < scalePatchX - 1; ++x) {
		maIndices[i++] = iVertex + 1;
		maIndices[i++] = iVertex;
		int offset = scalePatchX * (scalePatchY - 1);
		maIndices[i++] = x + offset + 1;
		maIndices[i++] = x + offset;
		XMFLOAT2 bz = CalcZBounds(maVertices[x + offset], maVertices[x + offset + 1]);
		maVertices[++iVertex].aabbmin = XMFLOAT3(x * tessFactor, mDepth - tessFactor, mBaseHeight);
		maVertices[iVertex].aabbmax = XMFLOAT3((x + 1) * tessFactor, mDepth - tessFactor, bz.y);
	}
	// add indices for side 3 of skirt. x = 0.
	++iVertex;
	for (int y = 0; y < scalePatchY - 1; ++y) {
		maIndices[i++] = iVertex + 1;
		maIndices[i++] = iVertex;
		maIndices[i++] = (y + 1) * scalePatchX;
		maIndices[i++] = y * scalePatchX;
		XMFLOAT2 bz = CalcZBounds(maVertices[y * scalePatchX], maVertices[(y + 1) * scalePatchX]);
		maVertices[++iVertex].aabbmin = XMFLOAT3(0.0f, y * tessFactor, mBaseHeight);
		maVertices[iVertex].aabbmax = XMFLOAT3(0.0f, (y + 1) * tessFactor, bz.y);
	}
	// add indices for side 4 of skirt. x = mWidth - tessFactor.
	++iVertex;
	for (int y = 0; y < scalePatchY - 1; ++y) {
		maIndices[i++] = iVertex;
		maIndices[i++] = iVertex + 1;
		maIndices[i++] = y * scalePatchX + scalePatchX - 1;
		maIndices[i++] = (y + 1) * scalePatchX + scalePatchX - 1;
		XMFLOAT2 bz = CalcZBounds(maVertices[y * scalePatchX + scalePatchX - 1], maVertices[(y + 1) * scalePatchX + scalePatchX - 1]);
		maVertices[iVertex].aabbmin = XMFLOAT3(mWidth - tessFactor, y * tessFactor, mBaseHeight);
		maVertices[iVertex++].aabbmax = XMFLOAT3(mWidth - tessFactor, (y + 1) * tessFactor, bz.y);
	}
	// add indices for bottom plane.
	maIndices[i++] = numVertsInTerrain + scalePatchX - 1;
	maIndices[i++] = numVertsInTerrain;
	maIndices[i++] = numVertsInTerrain + scalePatchX + scalePatchX - 1;
	maIndices[i++] = numVertsInTerrain + scalePatchX;
	maVertices[numVertsInTerrain + scalePatchX - 1].aabbmin = XMFLOAT3(0.0f, 0.0f, mBaseHeight);
	maVertices[numVertsInTerrain + scalePatchX - 1].aabbmax = XMFLOAT3(mWidth, mDepth, mBaseHeight);
	maVertices[numVertsInTerrain + scalePatchX - 1].skirt = 0;
	
	mIndexCount = arrSize;

	CreateVertexBuffer();
	CreateIndexBuffer();
	CreateConstantBuffer();
}

// Create the vertex buffer view
void Terrain::CreateVertexBuffer() {
	// Create the vertex buffer
	ID3D12Resource* buffer;
	auto iBuffer = m_pResMgr->NewBuffer(buffer, &CD3DX12_RESOURCE_DESC::Buffer(mVertexCount * sizeof(Vertex)),
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, nullptr);
	buffer->SetName(L"Terrain Vertex Buffer");
	auto sizeofVertexBuffer = GetRequiredIntermediateSize(buffer, 0, 1);

	// prepare vertex data for upload.
	D3D12_SUBRESOURCE_DATA dataVB = {};
	dataVB.pData = maVertices;
	dataVB.RowPitch = sizeofVertexBuffer;
	dataVB.SlicePitch = sizeofVertexBuffer;

	m_pResMgr->UploadToBuffer(iBuffer, 1, &dataVB, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	// create and save vertex buffer view to Terrain object.
	mVBV = {};
	mVBV.BufferLocation = buffer->GetGPUVirtualAddress();
	mVBV.StrideInBytes = sizeof(Vertex);
	mVBV.SizeInBytes = sizeofVertexBuffer;
}

// Create the index buffer view
void Terrain::CreateIndexBuffer() {
	// Create the index buffer
	ID3D12Resource* buffer;
	auto iBuffer = m_pResMgr->NewBuffer(buffer, &CD3DX12_RESOURCE_DESC::Buffer(mIndexCount * sizeof(UINT)),
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
		D3D12_RESOURCE_STATE_INDEX_BUFFER, nullptr);
	buffer->SetName(L"Terrain Index Buffer");
	auto sizeofIndexBuffer = GetRequiredIntermediateSize(buffer, 0, 1);

	// prepare index data for upload.
	D3D12_SUBRESOURCE_DATA dataIB = {};
	dataIB.pData = maIndices;
	dataIB.RowPitch = sizeofIndexBuffer;
	dataIB.SlicePitch = sizeofIndexBuffer;

	m_pResMgr->UploadToBuffer(iBuffer, 1, &dataIB, D3D12_RESOURCE_STATE_INDEX_BUFFER);

	// create and save index buffer view to Terrain object.
	mIBV = {};
	mIBV.BufferLocation = buffer->GetGPUVirtualAddress();
	mIBV.Format = DXGI_FORMAT_R32_UINT;
	mIBV.SizeInBytes = sizeofIndexBuffer;
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
	m_pConstants = new TerrainShaderConstants(mHeightScale, (float)mWidth, (float)mDepth, (float)mBaseHeight);
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
	int topRightX = topRight.position.x >= mWidth ? (int)topRight.position.x : (int)topRight.position.x + 1;
	int topRightY = topRight.position.y >= mWidth ? (int)topRight.position.y : (int)topRight.position.y + 1;

	for (int y = bottomLeftY; y <= topRightY; ++y) {
		for (int x = bottomLeftX; x <= topRightX; ++x) {
			float z = ((float)maImage[(x + y * mWidth) * 4] / 255.0f) * mHeightScale;

			if (z > max) max = z;
			if (z < min) min = z;
		}
	}

	return XMFLOAT2(min, max);
}

// load the specified file containing the heightmap data.
void Terrain::LoadHeightMap(const char* fnHeightMap) {
	unsigned int index;
	index = m_pResMgr->LoadFile(fnHeightMap, mDepth, mWidth);
	maImage = m_pResMgr->GetFileData(index);

	// Create the texture buffers.
	D3D12_RESOURCE_DESC	descTex = {};
	descTex.MipLevels = 1;
	descTex.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	descTex.Width = mWidth;
	descTex.Height = mDepth;
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
	dataTex.pData = maImage;
	dataTex.RowPitch = mWidth * 4 * sizeof(unsigned char);
	dataTex.SlicePitch = mDepth * mWidth * 4 * sizeof(unsigned char);	
	
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
	index = m_pResMgr->LoadFile(fnMap, mDispDepth, mDispWidth);
	maDispImage = m_pResMgr->GetFileData(index);

	// Create the texture buffers.
	D3D12_RESOURCE_DESC	descTex = {};
	descTex.MipLevels = 1;
	descTex.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	descTex.Width = mDispWidth;
	descTex.Height = mDispDepth;
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
	dataTex.pData = maDispImage;
	dataTex.RowPitch = mDispWidth * 4 * sizeof(unsigned char);
	dataTex.SlicePitch = mDispDepth * mDispWidth * 4 * sizeof(unsigned char);

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