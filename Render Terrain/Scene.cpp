/*
Scene.cpp

Author:			Chris Serson
Last Edited:	July 26, 2016

Description:	Class for creating, managing, and rendering a scene.
*/
#include "Scene.h"

Scene::Scene(int height, int width, Graphics* GFX) : T(), C(height, width), DNC(1440) {
	mpGFX = GFX;
	mDrawMode = 0;

	// create a viewport and scissor rectangle.
	mViewport.TopLeftX = 0;
	mViewport.TopLeftY = 0;
	mViewport.Width = (float)width;
	mViewport.Height = (float)height;
	mViewport.MinDepth = 0;
	mViewport.MaxDepth = 1;

	mScissorRect.left = 0;
	mScissorRect.top = 0;
	mScissorRect.right = width;
	mScissorRect.bottom = height;

	// Initialize Graphics Pipelines for 2D and 3D rendering.
	// Initialize for rendering terrain in 2D first so it is index 0.
	// Initialize for rendering terrain in 3D next so it is index 1.
	InitPipelineTerrain2D();
	InitPipelineTerrain3D();
	InitSRVCBVHeap();
	InitPerFrameConstantBuffer();
	InitTerrainResources();
	
	// after creating and initializing the heightmap for the terrain, we need to close the command list
	// and tell the Graphics object to execute the command list to actually finish the subresource init.
	CloseCommandLists();
	mpGFX->LoadAssets();

	CleanupTemporaryBuffers();
	T.DeleteVertexAndIndexArrays();
}

Scene::~Scene() {
	mpGFX->ClearAllFrames(); // ensure we are done rendering before releasing resources.
	mpGFX = nullptr;

	while (!mlRootSigs.empty()) {
		ID3D12RootSignature* sigRoot = mlRootSigs.back();

		if (sigRoot) sigRoot->Release();

		mlRootSigs.pop_back();
	}

	while (!mlPSOs.empty()) {
		ID3D12PipelineState* pso = mlPSOs.back();

		if (pso) pso->Release();

		mlPSOs.pop_back();
	}

	while (!mlDescriptorHeaps.empty()) {
		ID3D12DescriptorHeap* heap = mlDescriptorHeaps.back();

		if (heap) heap->Release();

		mlDescriptorHeaps.pop_back();
	}

	if (mpPerFrameConstants) {
		mpPerFrameConstants->Unmap(0, nullptr);
		mpPerFrameConstantsMapped = nullptr;
		mpPerFrameConstants->Release();
		mpPerFrameConstants = nullptr;
	}

	CleanupTemporaryBuffers();
}

// Close all command lists. Currently there is only the one.
void Scene::CloseCommandLists() {
	// close the command list.
	if (FAILED(mpGFX->GetCommandList()->Close())) {
		throw GFX_Exception("CommandList Close failed.");
	}
}

void Scene::CleanupTemporaryBuffers() {
	while (!mlTemporaryUploadBuffers.empty()) {
		ID3D12Resource* res = mlTemporaryUploadBuffers.back();

		if (res) res->Release();

		mlTemporaryUploadBuffers.pop_back();
	}
}

// Initialize a SRV/CBV heap. Currently hard-coded for 3 descriptors, the per-frame constant buffer, shader constants, and the terrain's heightmap texture.
void Scene::InitSRVCBVHeap() {
	D3D12_DESCRIPTOR_HEAP_DESC descCBVSRVHeap = {};

	// create the SRV heap that points at the heightmap and CBV.
	descCBVSRVHeap.NumDescriptors = 3;
	descCBVSRVHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descCBVSRVHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ID3D12DescriptorHeap* heap;
	mpGFX->CreateDescriptorHeap(&descCBVSRVHeap, heap);
	heap->SetName(L"CBV/SRV Heap");
	mlDescriptorHeaps.push_back(heap);
	msizeofDescHeapIncrement = mpGFX->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

// Initialize the per-frame constant buffer.
void Scene::InitPerFrameConstantBuffer() {
	// Create an upload buffer for the CBV
	UINT64 sizeofBuffer = sizeof(PerFrameConstantBuffer);
	mpGFX->CreateUploadBuffer(mpPerFrameConstants, &CD3DX12_RESOURCE_DESC::Buffer(sizeofBuffer));
	mpPerFrameConstants->SetName(L"CBV for general per frame values.");

	// Create the CBV itself
	D3D12_CONSTANT_BUFFER_VIEW_DESC	descCBV = {};
	descCBV.BufferLocation = mpPerFrameConstants->GetGPUVirtualAddress();
	descCBV.SizeInBytes = (sizeofBuffer + 255) & ~255; // CB size is required to be 256-byte aligned.

	CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(mlDescriptorHeaps[0]->GetCPUDescriptorHandleForHeapStart(), 0, msizeofDescHeapIncrement);

	mpGFX->CreateCBV(&descCBV, srvHandle);

	// initialize and map the constant buffers.
	// per the DirectX 12 sample code, we can leave this mapped until we close.
//	mpPerFrameConstantsMapped = new PerFrameConstantBuffer;
//	ZeroMemory(mpPerFrameConstantsMapped, sizeof(PerFrameConstantBuffer));

	CD3DX12_RANGE rangeRead(0, 0); // we won't be reading from this resource
	if (FAILED(mpPerFrameConstants->Map(0, &rangeRead, reinterpret_cast<void**>(&mpPerFrameConstantsMapped)))) {
		throw (GFX_Exception("Failed to map Per Frame Constant Buffer."));
	}
}

// Initialize the root signature and pipeline state object for rendering the terrain in 2D.
void Scene::InitPipelineTerrain2D() {
	// set up the Root Signature.
	// create a descriptor table with 1 entry for the descriptor heap containing our SRV to the heightmap.
	CD3DX12_DESCRIPTOR_RANGE rangeRoot;
	CD3DX12_ROOT_PARAMETER paramsRoot[1];
	rangeRoot.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	paramsRoot[0].InitAsDescriptorTable(1, &rangeRoot);

	// create our texture sampler for the heightmap.
	CD3DX12_STATIC_SAMPLER_DESC	descSamplers[1];
	descSamplers[0].Init(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);

	// It isn't really necessary to deny the other shaders access, but it does technically allow the GPU to optimize more.
	CD3DX12_ROOT_SIGNATURE_DESC	descRoot;
	descRoot.Init(1, paramsRoot, 1, descSamplers, D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS);
	ID3D12RootSignature* sigRoot;
	mpGFX->CreateRootSig(&descRoot, sigRoot);
	mlRootSigs.push_back(sigRoot); // save a copy of the pointer to the root signature.

	D3D12_SHADER_BYTECODE bcPS = {};
	D3D12_SHADER_BYTECODE bcVS = {};
	mpGFX->CompileShader(L"RenderTerrain2dVS.hlsl", bcVS, VERTEX_SHADER);
	mpGFX->CompileShader(L"RenderTerrain2dPS.hlsl", bcPS, PIXEL_SHADER);

	DXGI_SAMPLE_DESC sampleDesc = {};
	sampleDesc.Count = 1; // turns multi-sampling off. Not supported feature for my card.
						  // create the pipeline state object

	D3D12_GRAPHICS_PIPELINE_STATE_DESC descPSO = {};
	descPSO.pRootSignature = sigRoot;
	descPSO.VS = bcVS;
	descPSO.PS = bcPS;
	descPSO.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	descPSO.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	descPSO.SampleDesc = sampleDesc;
	descPSO.SampleMask = UINT_MAX;
	descPSO.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	descPSO.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	descPSO.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	descPSO.NumRenderTargets = 1;
	descPSO.DepthStencilState.DepthEnable = false;
	descPSO.DepthStencilState.StencilEnable = false;
	
	ID3D12PipelineState* pso;
	mpGFX->CreatePSO(&descPSO, pso);
	mlPSOs.push_back(pso); // save a copy of the pointer to the PSO.
}

// Initialize the root signature and pipeline state object for rendering the terrain in 3D.
void Scene::InitPipelineTerrain3D() {
	// set up the Root Signature.
	// create a descriptor table with 2 entries for the descriptor heap containing our SRV to the heightmap and our CBV.
	CD3DX12_ROOT_PARAMETER paramsRoot[3];
	CD3DX12_DESCRIPTOR_RANGE rangesRoot[3];


	// initialize a slot for the heightmap
	rangesRoot[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	paramsRoot[0].InitAsDescriptorTable(1, &rangesRoot[0]);

	// create a root parameter for our cbv
	rangesRoot[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	paramsRoot[1].InitAsDescriptorTable(1, &rangesRoot[1]);

	// set root constants descriptor
	rangesRoot[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
	paramsRoot[2].InitAsDescriptorTable(1, &rangesRoot[2]);
	
	// create our texture samplers for the heightmap.
	CD3DX12_STATIC_SAMPLER_DESC	descSamplers[3];
	descSamplers[0].Init(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
	descSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	descSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	descSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	descSamplers[1].Init(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
	descSamplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_DOMAIN;
	descSamplers[1].ShaderRegister = 1;
	descSamplers[2].Init(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
	descSamplers[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	descSamplers[2].ShaderRegister = 2;

	// It isn't really necessary to deny the other shaders access, but it does technically allow the GPU to optimize more.
	CD3DX12_ROOT_SIGNATURE_DESC	descRoot;
	descRoot.Init(_countof(paramsRoot), paramsRoot, 3, descSamplers, D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	ID3D12RootSignature* sigRoot;
	mpGFX->CreateRootSig(&descRoot, sigRoot);
	mlRootSigs.push_back(sigRoot);

	D3D12_SHADER_BYTECODE bcPS = {};
	D3D12_SHADER_BYTECODE bcVS = {};
	D3D12_SHADER_BYTECODE bcHS = {};
	D3D12_SHADER_BYTECODE bcDS = {};
	mpGFX->CompileShader(L"RenderTerrainTessVS.hlsl", bcVS, VERTEX_SHADER);
	mpGFX->CompileShader(L"RenderTerrainTessPS.hlsl", bcPS, PIXEL_SHADER);
	mpGFX->CompileShader(L"RenderTerrainTessHS.hlsl", bcHS, HULL_SHADER);
	mpGFX->CompileShader(L"RenderTerrainTessDS.hlsl", bcDS, DOMAIN_SHADER);

	DXGI_SAMPLE_DESC descSample = {};
	descSample.Count = 1; // turns multi-sampling off. Not supported feature for my card.
						  // create the pipeline state object
						  // create input layout.

	D3D12_INPUT_LAYOUT_DESC	descInputLayout = {};
	D3D12_INPUT_ELEMENT_DESC descElementLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "POSITION", 1, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	descInputLayout.NumElements = sizeof(descElementLayout) / sizeof(D3D12_INPUT_ELEMENT_DESC);
	descInputLayout.pInputElementDescs = descElementLayout;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC descPSO = {};
	descPSO.pRootSignature = sigRoot;
	descPSO.InputLayout = descInputLayout;
	descPSO.VS = bcVS;
	descPSO.PS = bcPS;
	descPSO.HS = bcHS;
	descPSO.DS = bcDS;
	descPSO.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
	descPSO.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	descPSO.SampleDesc = descSample;
	descPSO.SampleMask = UINT_MAX;
	descPSO.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	descPSO.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	descPSO.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	descPSO.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	descPSO.NumRenderTargets = 1;
	descPSO.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

	ID3D12PipelineState* pso;
	mpGFX->CreatePSO(&descPSO, pso);
	mlPSOs.push_back(pso);
}

// Initialize all of the resources needed by the terrain: heightmap texture, vertex buffer, index buffer.
// Also initializes shader specific constants that don't need to change.
void Scene::InitTerrainResources() {
	// our terrain consists of a heightmap texture, a vertex buffer, and an index buffer.
	// Create buffers for each.

	UINT width = T.GetHeightMapWidth();
	UINT depth = T.GetHeightMapDepth();
	size_t sizeofVertexBuffer = T.GetSizeOfVertexBuffer();
	size_t sizeofIndexBuffer = T.GetSizeOfIndexBuffer();
	size_t sizeofConstantBuffer = sizeof(TerrainShaderConstants);

	// Create the heightmap texture buffer.
	D3D12_RESOURCE_DESC	descTex = {};
	descTex.MipLevels = 1;
	descTex.Format = DXGI_FORMAT_R32_FLOAT;
	descTex.Width = width;
	descTex.Height = depth;
	descTex.Flags = D3D12_RESOURCE_FLAG_NONE;
	descTex.DepthOrArraySize = 1;
	descTex.SampleDesc.Count = 1;
	descTex.SampleDesc.Quality = 0;
	descTex.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	ID3D12Resource* heightmap;
	mpGFX->CreateDefaultBuffer(heightmap, &descTex);
	heightmap->SetName(L"Terrain Heightmap Texture Buffer");
	const auto sizeofHeightmap = GetRequiredIntermediateSize(heightmap, 0, 1);

	// prepare heightmap data for upload.
	D3D12_SUBRESOURCE_DATA dataTex = {};
	dataTex.pData = T.GetHeightMapTextureData();
	dataTex.RowPitch = width * sizeof(float);
	dataTex.SlicePitch = depth * width * sizeof(float);

	// Create the vertex buffer
	ID3D12Resource* vertexbuffer;
	mpGFX->CreateDefaultBuffer(vertexbuffer, &CD3DX12_RESOURCE_DESC::Buffer(sizeofVertexBuffer));
	vertexbuffer->SetName(L"Terrain Vertex Buffer");
	sizeofVertexBuffer = GetRequiredIntermediateSize(vertexbuffer, 0, 1);

	// prepare vertex data for upload.
	D3D12_SUBRESOURCE_DATA dataVB = {};
	dataVB.pData = T.GetVertexArray();
	dataVB.RowPitch = sizeofVertexBuffer;
	dataVB.SlicePitch = sizeofVertexBuffer;

	// Create the index buffer
	ID3D12Resource* indexbuffer;
	mpGFX->CreateDefaultBuffer(indexbuffer, &CD3DX12_RESOURCE_DESC::Buffer(sizeofIndexBuffer));
	vertexbuffer->SetName(L"Terrain Index Buffer");
	sizeofIndexBuffer = GetRequiredIntermediateSize(indexbuffer, 0, 1);

	// prepare index data for upload.
	D3D12_SUBRESOURCE_DATA dataIB = {};
	dataIB.pData = T.GetIndexArray();
	dataIB.RowPitch = sizeofIndexBuffer;
	dataIB.SlicePitch = sizeofIndexBuffer;

	// Create the constant buffer
	ID3D12Resource* constantbuffer;
	mpGFX->CreateDefaultBuffer(constantbuffer, &CD3DX12_RESOURCE_DESC::Buffer(sizeofConstantBuffer));
	constantbuffer->SetName(L"Terrain shader constant buffer");

	// prepare constant buffer data for upload.
	D3D12_SUBRESOURCE_DATA dataCB = {};
	dataCB.pData = &TerrainShaderConstants(T.GetScale(), (float)T.GetHeightMapWidth(), (float)T.GetHeightMapDepth());
	dataCB.RowPitch = GetRequiredIntermediateSize(constantbuffer, 0, 1);
	dataCB.SlicePitch = dataCB.RowPitch;

	// attempt to create 1 upload buffer to upload all three.
	// if that fails, we'll attempt to create separate upload buffers for each.
	ID3D12GraphicsCommandList *cmdList = mpGFX->GetCommandList();
	try {
		ID3D12Resource* upload;
		mpGFX->CreateUploadBuffer(upload, &CD3DX12_RESOURCE_DESC::Buffer(sizeofHeightmap + sizeofVertexBuffer + sizeofIndexBuffer + sizeofConstantBuffer));
		mlTemporaryUploadBuffers.push_back(upload);

		// upload heightmap data
		UpdateSubresources(cmdList, heightmap, upload, 0, 0, 1, &dataTex);

		// upload vertex buffer data
		UpdateSubresources(cmdList, vertexbuffer, upload, sizeofHeightmap, 0, 1, &dataVB);

		// upload index buffer data
		UpdateSubresources(cmdList, indexbuffer, upload, sizeofHeightmap + sizeofVertexBuffer, 0, 1, &dataIB);

		// upload the constant buffer data
		UpdateSubresources(cmdList, constantbuffer, upload, sizeofHeightmap + sizeofVertexBuffer + sizeofIndexBuffer, 0, 1, &dataCB);
	} catch (GFX_Exception e) {
		// create 4 separate upload buffers
		ID3D12Resource *uploadHeightmap, *uploadVB, *uploadIB, *uploadCB;
		mpGFX->CreateUploadBuffer(uploadHeightmap, &CD3DX12_RESOURCE_DESC::Buffer(sizeofHeightmap));
		mpGFX->CreateUploadBuffer(uploadVB, &CD3DX12_RESOURCE_DESC::Buffer(sizeofVertexBuffer));
		mpGFX->CreateUploadBuffer(uploadIB, &CD3DX12_RESOURCE_DESC::Buffer(sizeofIndexBuffer));
		mpGFX->CreateUploadBuffer(uploadCB, &CD3DX12_RESOURCE_DESC::Buffer(sizeofConstantBuffer));
		mlTemporaryUploadBuffers.push_back(uploadHeightmap);
		mlTemporaryUploadBuffers.push_back(uploadVB);
		mlTemporaryUploadBuffers.push_back(uploadIB);
		mlTemporaryUploadBuffers.push_back(uploadCB);

		// upload heightmap data
		UpdateSubresources(cmdList, heightmap, uploadHeightmap, 0, 0, 1, &dataTex);

		// upload vertex buffer data
		UpdateSubresources(cmdList, vertexbuffer, uploadVB, 0, 0, 1, &dataVB);

		// upload index buffer data
		UpdateSubresources(cmdList, indexbuffer, uploadIB, 0, 0, 1, &dataIB);

		// upload constant buffer data
		UpdateSubresources(cmdList, constantbuffer, uploadCB, 0, 0, 1, &dataCB);
	}

	// set resource barriers to inform GPU that data is ready for use.
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(heightmap, D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(vertexbuffer, D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(indexbuffer, D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_INDEX_BUFFER));
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(constantbuffer, D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

	// create and save vertex buffer view to Terrain object.
	D3D12_VERTEX_BUFFER_VIEW bvVertex;
	bvVertex.BufferLocation = vertexbuffer->GetGPUVirtualAddress();
	bvVertex.StrideInBytes = sizeof(Vertex);
	bvVertex.SizeInBytes = T.GetSizeOfVertexBuffer();
	T.SetVertexBufferView(bvVertex);
	T.SetVertexBufferResource(vertexbuffer);

	// create and save index buffer view to Terrain object.
	D3D12_INDEX_BUFFER_VIEW	bvIndex;
	bvIndex.BufferLocation = indexbuffer->GetGPUVirtualAddress();
	bvIndex.Format = DXGI_FORMAT_R32_UINT;
	bvIndex.SizeInBytes = T.GetSizeOfIndexBuffer();
	T.SetIndexBufferView(bvIndex);
	T.SetIndexBufferResource(indexbuffer);

	// Create the SRV for the heightmap texture and save to Terrain object.
	D3D12_SHADER_RESOURCE_VIEW_DESC	descSRV = {};
	descSRV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	descSRV.Format = descTex.Format;
	descSRV.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	descSRV.Texture2D.MipLevels = descTex.MipLevels;
	
	CD3DX12_CPU_DESCRIPTOR_HANDLE handleSRV(mlDescriptorHeaps[0]->GetCPUDescriptorHandleForHeapStart(), 1, msizeofDescHeapIncrement);
	mpGFX->CreateSRV(heightmap, &descSRV, handleSRV);
	T.SetHeightmapResource(heightmap);

	// Create a constant buffer view.
	D3D12_CONSTANT_BUFFER_VIEW_DESC descCBV = {};
	descCBV.BufferLocation = constantbuffer->GetGPUVirtualAddress();
	descCBV.SizeInBytes = (sizeof(TerrainShaderConstants) + 255) & ~255; // CB size is required to be 256-byte aligned.
	
	CD3DX12_CPU_DESCRIPTOR_HANDLE handleCBV(mlDescriptorHeaps[0]->GetCPUDescriptorHandleForHeapStart(), 2, msizeofDescHeapIncrement);
	mpGFX->CreateCBV(&descCBV, handleCBV);
}

void Scene::SetViewport() {
	ID3D12GraphicsCommandList* cmdList = mpGFX->GetCommandList();
	cmdList->RSSetViewports(1, &mViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);
}

void Scene::DrawTerrain(ID3D12GraphicsCommandList* cmdList) {
	cmdList->SetPipelineState(mlPSOs[mDrawMode]);
	cmdList->SetGraphicsRootSignature(mlRootSigs[mDrawMode]);

	ID3D12DescriptorHeap* heaps[] = { mlDescriptorHeaps[0] };
	cmdList->SetDescriptorHeaps(_countof(heaps), heaps);

	// set the srv buffer.
	CD3DX12_GPU_DESCRIPTOR_HANDLE handleSRV(mlDescriptorHeaps[0]->GetGPUDescriptorHandleForHeapStart(), 1, msizeofDescHeapIncrement);
	cmdList->SetGraphicsRootDescriptorTable(0, handleSRV);

	if (mDrawMode == 1) {
		// set the per frame constant buffer.
		XMFLOAT4 frustum[6];
		C.GetViewFrustum(frustum);
		
		PerFrameConstantBuffer constants;
		constants.viewproj = C.GetViewProjectionMatrixTransposed();
		constants.eye = C.GetEyePosition();
		constants.frustum[0] = frustum[0];
		constants.frustum[1] = frustum[1];
		constants.frustum[2] = frustum[2];
		constants.frustum[3] = frustum[3];
		constants.frustum[4] = frustum[4];
		constants.frustum[5] = frustum[5];
		constants.light = DNC.GetLight();
		memcpy(mpPerFrameConstantsMapped, &constants, sizeof(PerFrameConstantBuffer));
				
		cmdList->SetGraphicsRootDescriptorTable(1, mlDescriptorHeaps[0]->GetGPUDescriptorHandleForHeapStart());
	
		// set the terrain shader constants
		CD3DX12_GPU_DESCRIPTOR_HANDLE handleCBV(mlDescriptorHeaps[0]->GetGPUDescriptorHandleForHeapStart(), 2, msizeofDescHeapIncrement);
		cmdList->SetGraphicsRootDescriptorTable(2, handleCBV);
	}
	
	// mDrawMode = 0/false for 2D rendering and 1/true for 3D rendering
	T.Draw(cmdList, (bool)mDrawMode);
}

void Scene::Draw() {
	mpGFX->ResetPipeline();

	// set a clear color.
	const float clearColor[] = { 0.2f, 0.6f, 1.0f, 1.0f };
	ID3D12GraphicsCommandList* cmdList = mpGFX->GetCommandList();
	mpGFX->SetBackBufferRender(cmdList, clearColor);

	SetViewport();
	
	DrawTerrain(cmdList);

	mpGFX->SetBackBufferPresent(mpGFX->GetCommandList());
	CloseCommandLists();
	mpGFX->Render();
}

void Scene::Update() {
	DNC.Update();

	Draw();
}

// function allowing the main program to pass keyboard input to the scene.
void Scene::HandleKeyboardInput(UINT key) {
	switch (key) {
		case _W:
			if (mDrawMode > 0) C.Translate(XMFLOAT3(MOVE_STEP, 0.0f, 0.0f));
			break;
		case _S:
			if (mDrawMode > 0) C.Translate(XMFLOAT3(-MOVE_STEP, 0.0f, 0.0f));
			break;
		case _A:
			if (mDrawMode > 0) C.Translate(XMFLOAT3(0.0f, MOVE_STEP, 0.0f));
			break;
		case _D:
			if (mDrawMode > 0) C.Translate(XMFLOAT3(0.0f, -MOVE_STEP, 0.0f));
			break;
		case _Q:
			if (mDrawMode > 0) C.Translate(XMFLOAT3(0.0f, 0.0f, MOVE_STEP));
			break;
		case _Z:
			if (mDrawMode > 0) C.Translate(XMFLOAT3(0.0f, 0.0f, -MOVE_STEP));
			break;
		case _1: // draw in 2D.
			mDrawMode = 0;
			break;
		case _2: // draw in 3D.
			mDrawMode = 1;
			break;
	}
}

// function allowing the main program to pass mouse input to the scene.
void Scene::HandleMouseInput(int x, int y) {
	if (mDrawMode > 0) {
		C.Pitch(ROT_ANGLE * y);
		C.Yaw(-ROT_ANGLE * x);
	}
}