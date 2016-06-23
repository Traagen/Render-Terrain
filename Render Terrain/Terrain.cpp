/*
Terrain.cpp

Author:			Chris Serson
Last Edited:	June 23, 2016

Description:	Class for loading a heightmap and rendering as a terrain.
				Currently renders only as 2D texture view.
*/
#include "lodepng.h"
#include "Terrain.h"

Terrain::Terrain(Graphics* GFX) {
	mpPSO2D = nullptr;
	mpPSO3D = nullptr;
	mpRootSig2D = nullptr;
	mpRootSig3D = nullptr;
	mpSRVHeap = nullptr;
	mpHeightmap = nullptr;
	mpUpload = nullptr;
	maImage = nullptr;

	PreparePipeline2D(GFX);
	PreparePipeline3D(GFX);
	PrepareHeightmap(GFX); // the heightmap is used across 2D and 3D.
}

Terrain::~Terrain() {
	// The order resources are released appears to matter. I haven't tested all possible orders, but at least releasing the heap
	// and resources after the pso and rootsig was causing my GPU to hang on shutdown. Using the current order resolved that issue.
	if (mpSRVHeap) {
		mpSRVHeap->Release();
		mpSRVHeap = nullptr;
	}

	if (mpUpload) {
		mpUpload->Release();
		mpUpload = nullptr;
	}

	if (mpHeightmap) {
		mpHeightmap->Release();
		mpHeightmap = nullptr;
	}

	if (mpPSO2D) {
		mpPSO2D->Release();
		mpPSO2D = nullptr;
	}

	if (mpPSO3D) {
		mpPSO3D->Release();
		mpPSO3D = nullptr;
	}
	
	if (mpRootSig2D) {
		mpRootSig2D->Release();
		mpRootSig2D = nullptr;
	}

	if (mpRootSig3D) {
		mpRootSig3D->Release();
		mpRootSig3D = nullptr;
	}

	delete[] maImage;
}

void Terrain::Draw2D(ID3D12GraphicsCommandList* cmdList) {
	cmdList->SetPipelineState(mpPSO2D);
	cmdList->SetGraphicsRootSignature(mpRootSig2D);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // describe how to read the vertex buffer.
	ID3D12DescriptorHeap* heaps[] = { mpSRVHeap };
	cmdList->SetDescriptorHeaps(1, heaps);
	cmdList->SetGraphicsRootDescriptorTable(0, mpSRVHeap->GetGPUDescriptorHandleForHeapStart());
	cmdList->DrawInstanced(3, 1, 0, 0);
}

void Terrain::Draw3D(ID3D12GraphicsCommandList* cmdList) {
	cmdList->SetPipelineState(mpPSO3D);
	cmdList->SetGraphicsRootSignature(mpRootSig3D);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // describe how to read the vertex buffer.

	mCBData.viewproj = mmViewProjTrans;
	mCBData.height = mHeight;
	mCBData.width = mWidth;
	memcpy(mpCBVDataBegin, &mCBData, sizeof(mCBData));
	
	ID3D12DescriptorHeap* heaps[] = { mpSRVHeap };
	cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
	cmdList->SetGraphicsRootDescriptorTable(0, mpSRVHeap->GetGPUDescriptorHandleForHeapStart());
	CD3DX12_GPU_DESCRIPTOR_HANDLE cbvHandle(mpSRVHeap->GetGPUDescriptorHandleForHeapStart(), 1, mSRVDescSize);
	cmdList->SetGraphicsRootDescriptorTable(1, cbvHandle);

	cmdList->DrawInstanced(3, 1, 0, 0);
}

// prepare RootSig, PSO, Shaders, and Descriptor heaps for 2D render
void Terrain::PreparePipeline2D(Graphics *GFX) {
	CD3DX12_ROOT_SIGNATURE_DESC			rootDesc;
	CD3DX12_ROOT_PARAMETER				paramsRoot[1];
	CD3DX12_DESCRIPTOR_RANGE			rangeRoot;
	CD3DX12_STATIC_SAMPLER_DESC			descSamplers[1];
	D3D12_GRAPHICS_PIPELINE_STATE_DESC	psoDesc = {};
	DXGI_SAMPLE_DESC					sampleDesc = {};
	D3D12_SHADER_BYTECODE				PSBytecode = {};
	D3D12_SHADER_BYTECODE				VSBytecode = {};
	D3D12_DESCRIPTOR_HEAP_DESC			srvHeapDesc = {};

	// create the SRV heap that points at the heightmap.
	srvHeapDesc.NumDescriptors = 1;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	GFX->CreateDescriptorHeap(srvHeapDesc, mpSRVHeap);
	mpSRVHeap->SetName(L"SRV Heap");

	// set up the Root Signature.
	// create a descriptor table with 1 entry for the descriptor heap containing our SRV to the heightmap.
	rangeRoot.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	paramsRoot[0].InitAsDescriptorTable(1, &rangeRoot);

	// create our texture sampler for the heightmap.
	descSamplers[0].Init(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
	
	// It isn't really necessary to deny the other shaders access, but it does technically allow the GPU to optimize more.
	rootDesc.Init(1, paramsRoot, 1, descSamplers, D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS | 
												  D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
												  D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
												  D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS);
	GFX->CreateRootSig(rootDesc, mpRootSig2D);

	GFX->CompileShader(L"RenderTerrain2dVS.hlsl", VSBytecode, VERTEX_SHADER);
	GFX->CompileShader(L"RenderTerrain2dPS.hlsl", PSBytecode, PIXEL_SHADER);

	sampleDesc.Count = 1; // turns multi-sampling off. Not supported feature for my card.
						  // create the pipeline state object

	psoDesc.pRootSignature = mpRootSig2D;
	psoDesc.VS = VSBytecode;
	psoDesc.PS = PSBytecode;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc = sampleDesc;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.NumRenderTargets = 1;
	psoDesc.DepthStencilState.DepthEnable = false;
	psoDesc.DepthStencilState.StencilEnable = false;

	GFX->CreatePSO(psoDesc, mpPSO2D);
}

// prepare RootSig, PSO, Shaders, and Descriptor heaps for 3D render
void Terrain::PreparePipeline3D(Graphics *GFX) {
	CD3DX12_ROOT_SIGNATURE_DESC			rootDesc;
	CD3DX12_ROOT_PARAMETER				paramsRoot[2];
	CD3DX12_DESCRIPTOR_RANGE			ranges[2];
	CD3DX12_STATIC_SAMPLER_DESC			descSamplers[1];
	D3D12_GRAPHICS_PIPELINE_STATE_DESC	psoDesc = {};
	DXGI_SAMPLE_DESC					sampleDesc = {};
	D3D12_SHADER_BYTECODE				PSBytecode = {};
	D3D12_SHADER_BYTECODE				VSBytecode = {};
	D3D12_DESCRIPTOR_HEAP_DESC			srvHeapDesc = {};

	// create the SRV heap that points at the heightmap and CBV.
	srvHeapDesc.NumDescriptors = 2;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	GFX->CreateDescriptorHeap(srvHeapDesc, mpSRVHeap);
	mpSRVHeap->SetName(L"CBV/SRV Heap");

	mSRVDescSize = GFX->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// set up the Root Signature.
	// create a descriptor table with 2 entries for the descriptor heap containing our SRV to the heightmap and our CBV.
	ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	paramsRoot[0].InitAsDescriptorTable(1, &ranges[0]);

	// create a root parameter for our cbv
	ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	paramsRoot[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_VERTEX);

	// create our texture sampler for the heightmap.
	descSamplers[0].Init(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
	
	// It isn't really necessary to deny the other shaders access, but it does technically allow the GPU to optimize more.
	rootDesc.Init(_countof(paramsRoot), paramsRoot, 1, descSamplers, D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
																	 D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
																	 D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);
	GFX->CreateRootSig(rootDesc, mpRootSig3D);

	CreateConstantBuffer(GFX);

	GFX->CompileShader(L"RenderTerrain3dVS.hlsl", VSBytecode, VERTEX_SHADER);
	GFX->CompileShader(L"RenderTerrain3dPS.hlsl", PSBytecode, PIXEL_SHADER);

	sampleDesc.Count = 1; // turns multi-sampling off. Not supported feature for my card.
						  // create the pipeline state object

	psoDesc.pRootSignature = mpRootSig3D;
	psoDesc.VS = VSBytecode;
	psoDesc.PS = PSBytecode;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc = sampleDesc;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.NumRenderTargets = 1;
	psoDesc.DepthStencilState.DepthEnable = false;
	psoDesc.DepthStencilState.StencilEnable = false;

	GFX->CreatePSO(psoDesc, mpPSO3D);
}

// create a constant buffer to contain shader values
void Terrain::CreateConstantBuffer(Graphics *GFX) {
	D3D12_DESCRIPTOR_HEAP_DESC		heapDesc = {};
	D3D12_CONSTANT_BUFFER_VIEW_DESC	cbvDesc = {};
	CD3DX12_RANGE					readRange(0, 0); // we won't be reading from this resource

	// Create an upload buffer for the CBV
	GFX->CreateBuffer(mpCBV, sizeof(ConstantBuffer));
	mpCBV->SetName(L"CBV");

	// Create the CBV itself
	cbvDesc.BufferLocation = mpCBV->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = (sizeof(ConstantBuffer) + 255) & ~255; // CB size is required to be 256-byte aligned.

	CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(mpSRVHeap->GetCPUDescriptorHandleForHeapStart(), 1, mSRVDescSize);

	GFX->CreateCBV(cbvDesc, srvHandle);

	// initialize and map the constant buffers.
	// per the DirectX 12 sample code, we can leave this mapped until we close.
	ZeroMemory(&mCBData, sizeof(mCBData));

	if (FAILED(mpCBV->Map(0, &readRange, reinterpret_cast<void**>(&mpCBVDataBegin)))) {
		throw (GFX_Exception("Failed to map CBV in Terrain."));
	}
}

// loads the heightmap texture into memory
void Terrain::PrepareHeightmap(Graphics *GFX) {
	D3D12_RESOURCE_DESC					texDesc = {};
	D3D12_SUBRESOURCE_DATA				texData = {};
	D3D12_SHADER_RESOURCE_VIEW_DESC		srvDesc = {};

	LoadHeightMap("heightmap2.png");

	// create the texture.
	texDesc.MipLevels = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.Width = mWidth;
	texDesc.Height = mHeight;
	texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	texDesc.DepthOrArraySize = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	// create shader resource view for the heightmap.
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = texDesc.MipLevels;

	texData.pData = maImage;
	texData.RowPitch = mWidth * 4;
	texData.SlicePitch = mHeight * mWidth * 4;
	
	GFX->CreateSRV(texDesc, mpHeightmap, mpUpload, texData, srvDesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, mpSRVHeap, 0);
	mpHeightmap->SetName(L"Heightmap Resource Heap");
	mpUpload->SetName(L"Heightmap Upload Resource Heap");
}

// load the specified file containing the heightmap data.
void Terrain::LoadHeightMap(const char* filename) {
	//decode
	unsigned error = lodepng_decode32_file(&maImage, &mWidth, &mHeight, filename);
	if (error) throw GFX_Exception("Error loading heightmap texture.");
}