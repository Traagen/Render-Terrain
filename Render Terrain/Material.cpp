/*
Material.cpp

Author:			Chris Serson
Last Edited:	October 12, 2016

Description:	Classes for creating and managing Direct3D 12 materials.
*/
#include "Material.h"

TerrainMaterial::TerrainMaterial(ResourceManager* rm, const char* fnNormals1, const char* fnNormals2, const char* fnNormals3, 
	const char* fnNormals4,	const char* fnDiff1, const char* fnDiff2, const char* fnDiff3, const char* fnDiff4, XMFLOAT4 colors[4]) : 
	m_pResMgr(rm) {
	unsigned int index[8], height, width;

	// assumes that all files are the same size.
	// forces this to be true as we are building a texture2darray.
	index[0] = m_pResMgr->LoadFile(fnNormals1, height, width);
	index[1] = m_pResMgr->LoadFile(fnNormals2, height, width);
	index[2] = m_pResMgr->LoadFile(fnNormals3, height, width);
	index[3] = m_pResMgr->LoadFile(fnNormals4, height, width);
	index[4] = m_pResMgr->LoadFile(fnDiff1, height, width);
	index[5] = m_pResMgr->LoadFile(fnDiff2, height, width);
	index[6] = m_pResMgr->LoadFile(fnDiff3, height, width);
	index[7] = m_pResMgr->LoadFile(fnDiff4, height, width);

	// Create the texture buffers.
	D3D12_RESOURCE_DESC	descTex = {};
	descTex.MipLevels = 1;
	descTex.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	descTex.Width = width;
	descTex.Height = height;
	descTex.Flags = D3D12_RESOURCE_FLAG_NONE;
	descTex.DepthOrArraySize = 8;
	descTex.SampleDesc.Count = 1;
	descTex.SampleDesc.Quality = 0;
	descTex.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	ID3D12Resource* textures;
	unsigned int iBuffer = m_pResMgr->NewBuffer(textures, &descTex, &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr);
	textures->SetName(L"Texture Array Buffer");

	D3D12_SUBRESOURCE_DATA dataTex[8];
	// prepare detail map data for upload.
	for (int i = 0; i < 8; ++i) {
		dataTex[i] = {};
		dataTex[i].pData = m_pResMgr->GetFileData(index[i]);
		dataTex[i].RowPitch = width * 4 * sizeof(unsigned char);
		dataTex[i].SlicePitch = height * width * 4 * sizeof(unsigned char);
	}

	m_pResMgr->UploadToBuffer(iBuffer, 8, dataTex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	// Create the SRV for the detail map texture and save to Terrain object.
	D3D12_SHADER_RESOURCE_VIEW_DESC	descSRV = {};
	descSRV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	descSRV.Format = descTex.Format;
	descSRV.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	descSRV.Texture2DArray.ArraySize = descTex.DepthOrArraySize;
	descSRV.Texture2DArray.MipLevels = descTex.MipLevels;

	m_pResMgr->AddSRV(textures, &descSRV, m_hdlTextureSRV_CPU, m_hdlTextureSRV_GPU);

	m_listColors[0] = colors[0];
	m_listColors[1] = colors[1];
	m_listColors[2] = colors[2];
	m_listColors[3] = colors[3];
}

TerrainMaterial::~TerrainMaterial() {
	m_pResMgr = nullptr;
}

// Attach our SRV to the provided root descriptor table slot.
void TerrainMaterial::Attach(ID3D12GraphicsCommandList* cmdList, unsigned int srvDescTableIndex) {
	cmdList->SetGraphicsRootDescriptorTable(srvDescTableIndex, m_hdlTextureSRV_GPU);
}