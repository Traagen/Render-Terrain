/*
Terrain.cpp

Author:			Chris Serson
Last Edited:	June 26, 2016

Description:	Class for loading a heightmap and rendering as a terrain.
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
	mpUploadHeightmap = nullptr;
	mpVertexBuffer = nullptr;
	mpUploadVB = nullptr;
	mpIndexBuffer = nullptr;
	mpUploadIB = nullptr;
	mpCBV = nullptr;
	maImage = nullptr;

	PreparePipeline2D(GFX);
	PreparePipeline3D(GFX);
//	PrepareHeightmap(GFX); // the heightmap is used across 2D and 3D.
}

Terrain::~Terrain() {
	// The order resources are released appears to matter. I haven't tested all possible orders, but at least releasing the heap
	// and resources after the pso and rootsig was causing my GPU to hang on shutdown. Using the current order resolved that issue.
	if (mpSRVHeap) {
		mpSRVHeap->Release();
		mpSRVHeap = nullptr;
	}

	if (mpUploadHeightmap) {
		mpUploadHeightmap->Release();
		mpUploadHeightmap = nullptr;
	}

	if (mpHeightmap) {
		mpHeightmap->Release();
		mpHeightmap = nullptr;
	}

	if (mpUploadIB) {
		mpUploadIB->Release();
		mpUploadIB = nullptr;
	}

	if (mpIndexBuffer) {
		mpIndexBuffer->Release();
		mpIndexBuffer = nullptr;
	}

	if (mpUploadVB) {
		mpUploadVB->Release();
		mpUploadVB = nullptr;
	}

	if (mpVertexBuffer) {
		mpVertexBuffer->Release();
		mpVertexBuffer = nullptr;
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

	if (mpCBV) {
		mpCBV->Unmap(0, nullptr);
		mpCBVDataBegin = nullptr;
		mpCBV->Release();
		mpCBV = nullptr;
	}

	if (maImage) delete[] maImage;
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


	mCBData.viewproj = mmViewProjTrans;
	mCBData.height = mHeight;
	mCBData.width = mWidth;
	memcpy(mpCBVDataBegin, &mCBData, sizeof(mCBData));
	
	ID3D12DescriptorHeap* heaps[] = { mpSRVHeap };
	cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
	cmdList->SetGraphicsRootDescriptorTable(0, mpSRVHeap->GetGPUDescriptorHandleForHeapStart());
	CD3DX12_GPU_DESCRIPTOR_HANDLE cbvHandle(mpSRVHeap->GetGPUDescriptorHandleForHeapStart(), 1, mSRVDescSize);
	cmdList->SetGraphicsRootDescriptorTable(1, cbvHandle);

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP); // describe how to read the vertex buffer.
	cmdList->IASetVertexBuffers(0, 1, &mVBV);
	cmdList->IASetIndexBuffer(&mIBV);
	
	cmdList->DrawIndexedInstanced(mIndexCount, 1, 0, 0, 0);
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
	D3D12_INPUT_LAYOUT_DESC				inputLayoutDesc = {};

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
																	 D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
																	 D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	GFX->CreateRootSig(rootDesc, mpRootSig3D);

	CreateConstantBuffer(GFX);

	GFX->CompileShader(L"RenderTerrain3dVS.hlsl", VSBytecode, VERTEX_SHADER);
	GFX->CompileShader(L"RenderTerrain3dPS.hlsl", PSBytecode, PIXEL_SHADER);

	sampleDesc.Count = 1; // turns multi-sampling off. Not supported feature for my card.
						  // create the pipeline state object

	// create input layout.
	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
	
	inputLayoutDesc.NumElements = sizeof(inputLayout) / sizeof(D3D12_INPUT_ELEMENT_DESC);
	inputLayoutDesc.pInputElementDescs = inputLayout;

	psoDesc.pRootSignature = mpRootSig3D;
	psoDesc.InputLayout = inputLayoutDesc;
	psoDesc.VS = VSBytecode;
	psoDesc.PS = PSBytecode;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc = sampleDesc;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.NumRenderTargets = 1;
	psoDesc.DepthStencilState.DepthEnable = false;
	psoDesc.DepthStencilState.StencilEnable = false;

	GFX->CreatePSO(psoDesc, mpPSO3D);

	PrepareHeightmap(GFX);
	CreateMesh3D(GFX);
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

// generate vertex and index buffers for 3D mesh of terrain
void Terrain::CreateMesh3D(Graphics *GFX) {
	D3D12_SUBRESOURCE_DATA		vertexData = {};
	D3D12_SUBRESOURCE_DATA		indexData = {};
	ID3D12GraphicsCommandList*	cmdList = GFX->GetCommandList();

	int height = mHeight;
	int width = mWidth;
	// Create a vertex buffer
	int arrSize = height * width;

	Vertex *vertices = new Vertex[arrSize];
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			vertices[y * width + x].position = XMFLOAT3(x, y, 0.0f);
		}
	}
	
	int buffSize = sizeof(Vertex) * arrSize;

	GFX->CreateCommittedBuffer(mpVertexBuffer, mpUploadVB, buffSize);
	mpVertexBuffer->SetName(L"Vertex buffer heap");
	mpUploadVB->SetName(L"Vertex buffer upload heap");

	vertexData.pData = vertices;
	vertexData.RowPitch = buffSize;
	vertexData.SlicePitch = buffSize;
	
	UpdateSubresources(cmdList, mpVertexBuffer, mpUploadVB, 0, 0, 1, &vertexData);
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mpVertexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

	// create the vertex buffer view
	mVBV.BufferLocation = mpVertexBuffer->GetGPUVirtualAddress();
	mVBV.StrideInBytes = sizeof(Vertex);
	mVBV.SizeInBytes = buffSize;
	
	// create an index buffer
	// our grid is width * height in size.
	// the vertices are oriented like so:
	//  0,  1,  2,  3,  4,
	//  5,  6,  7,  8,  9,
	// 10, 11, 12, 13, 14
	// we want our index buffer to define triangle strips that are wound correctly.
	// 5, 0, 6, 1, 7, 2, 8, 3, 9, 4 shoud wind clockwise for every triangle in the strip.
	int stripSize = width * 2;
	int numStrips = height - 1;
	arrSize = stripSize * numStrips + (numStrips - 1) * 4; // degenerate triangles

	UINT* indices = new UINT[arrSize];
	int i = 0;
	for (int s = 0; s < numStrips; ++s) {
		int m = 0;
		for (int n = 0; n < width; ++n) {
			m = n + s * width;
			indices[i++] = m + width;
			indices[i++] = m;
		}
		if (s < numStrips - 1) {
			indices[i++] = m;
			indices[i++] = m - width + 1;
			indices[i++] = m - width + 1;
			indices[i++] = m - width + 1;
		}
	}

	buffSize = sizeof(UINT) * arrSize;
	
	GFX->CreateCommittedBuffer(mpIndexBuffer, mpUploadIB, buffSize);
	mpIndexBuffer->SetName(L"Index buffer heap");
	mpUploadIB->SetName(L"Index buffer upload heap");

	indexData.pData = indices;
	indexData.RowPitch = buffSize;
	indexData.SlicePitch = buffSize;

	UpdateSubresources(cmdList, mpIndexBuffer, mpUploadIB, 0, 0, 1, &indexData);
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mpIndexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER));

	mIBV.BufferLocation = mpIndexBuffer->GetGPUVirtualAddress();
	mIBV.Format = DXGI_FORMAT_R32_UINT;
	mIBV.SizeInBytes = buffSize;

	mIndexCount = arrSize;
}

// loads the heightmap texture into memory
void Terrain::PrepareHeightmap(Graphics *GFX) {
	D3D12_RESOURCE_DESC					texDesc = {};
	D3D12_SUBRESOURCE_DATA				texData = {};
	D3D12_SHADER_RESOURCE_VIEW_DESC		srvDesc = {};

	LoadHeightMap("heightmap7.png");

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
	
	GFX->CreateSRV(texDesc, mpHeightmap, mpUploadHeightmap, texData, srvDesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, mpSRVHeap, 0);
	mpHeightmap->SetName(L"Heightmap Resource Heap");
	mpUploadHeightmap->SetName(L"Heightmap Upload Resource Heap");
}

// load the specified file containing the heightmap data.
void Terrain::LoadHeightMap(const char* filename) {
	//decode
	unsigned error = lodepng_decode32_file(&maImage, &mWidth, &mHeight, filename);
	if (error) throw GFX_Exception("Error loading heightmap texture.");
}