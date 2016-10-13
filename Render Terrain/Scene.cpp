/*
Scene.cpp

Author:			Chris Serson
Last Edited:	October 12, 2016

Description:	Class for creating, managing, and rendering a scene.
*/
#include "Scene.h"
#include <stdlib.h>

Scene::Scene(int height, int width, Device* DEV) : 
	m_ResMgr(DEV, FRAME_BUFFER_COUNT, 6, 23, 0), m_Cam(height, width), m_DNC(6000, 4096) {
	m_pDev = DEV;
	m_pT = nullptr;

	for (int i = 0; i < FRAME_BUFFER_COUNT; ++i) {
		m_pFrames[i] = new Frame(i, m_pDev, &m_ResMgr, height, width, 4096);
	}

	XMFLOAT4 colors[] = { XMFLOAT4(0.35f, 0.5f, 0.18f, 1.0f), XMFLOAT4(0.89f, 0.89f, 0.89f, 1.0f),
		XMFLOAT4(0.31f, 0.25f, 0.2f, 1.0f), XMFLOAT4(0.39f, 0.37f, 0.38f, 1.0f) };
	m_pT = new Terrain(&m_ResMgr, new TerrainMaterial(&m_ResMgr, "grassnormals.png", "snownormals.png",
		"dirtnormals.png", "rocknormals.png", "grassdiffuse.png", "snowdiffuse.png", "dirtdiffuse.png",
		"rockdiffuse.png", colors), "heightmap6.png", "displacement.png");

	m_ResMgr.WaitForGPU();

	m_pDev->CreateGraphicsCommandList(D3D12_COMMAND_LIST_TYPE_DIRECT, m_pFrames[0]->GetAllocator(), m_pCmdList);
	CloseCommandLists();

	// create a viewport and scissor rectangle.
	m_vpMain.TopLeftX = 0;
	m_vpMain.TopLeftY = 0;
	m_vpMain.Width = (float)width;
	m_vpMain.Height = (float)height;
	m_vpMain.MinDepth = 0;
	m_vpMain.MaxDepth = 1;

	m_srMain.left = 0;
	m_srMain.top = 0;
	m_srMain.right = width;
	m_srMain.bottom = height;

	// Initialize Graphics Pipelines for 2D and 3D rendering.
	InitPipelineTerrain2D();
	InitPipelineTerrain3D();
	InitPipelineShadowMap();
}

Scene::~Scene() {
	while (!m_listRootSigs.empty()) {
		ID3D12RootSignature* sigRoot = m_listRootSigs.back();

		if (sigRoot) sigRoot->Release();

		m_listRootSigs.pop_back();
	}

	while (!m_listPSOs.empty()) {
		ID3D12PipelineState* pso = m_listPSOs.back();

		if (pso) pso->Release();

		m_listPSOs.pop_back();
	}

	if (m_pT) {
		delete m_pT;
	}

	for (int i = 0; i < FRAME_BUFFER_COUNT; ++i) {
		delete m_pFrames[i];
	}

	m_pDev = nullptr;
}

// Close all command lists. Currently there is only the one.
void Scene::CloseCommandLists() {
	// close the command list.
	if (FAILED(m_pCmdList->Close())) {
		throw GFX_Exception("Scene::CloseCommandLists failed.");
	}
}

// Initialize the root signature and pipeline state object for rendering the terrain in 2D.
void Scene::InitPipelineTerrain2D() {
	// set up the Root Signature.
	// create a descriptor table with 1 entry for the descriptor heap containing our SRV to the heightmap.
	CD3DX12_DESCRIPTOR_RANGE rangesRoot[3];
	CD3DX12_ROOT_PARAMETER paramsRoot[3];
	// height map
	rangesRoot[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	paramsRoot[0].InitAsDescriptorTable(1, &rangesRoot[0]);
	// displacement map
	rangesRoot[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
	paramsRoot[1].InitAsDescriptorTable(1, &rangesRoot[1]);
	// terrain constants
	rangesRoot[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	paramsRoot[2].InitAsDescriptorTable(1, &rangesRoot[2]);

	// create our texture sampler for the heightmap.
	CD3DX12_STATIC_SAMPLER_DESC	descSamplers[1];
	descSamplers[0].Init(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);

	// It isn't really necessary to deny the other shaders access, but it does technically allow the GPU to optimize more.
	CD3DX12_ROOT_SIGNATURE_DESC	descRoot;
	descRoot.Init(_countof(paramsRoot), paramsRoot, 1, descSamplers, D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS);
	ID3D12RootSignature* sigRoot;
	m_pDev->CreateRootSig(&descRoot, sigRoot);
	m_listRootSigs.push_back(sigRoot); // save a copy of the pointer to the root signature.

	D3D12_SHADER_BYTECODE bcPS = {};
	D3D12_SHADER_BYTECODE bcVS = {};
	CompileShader(L"RenderTerrain2dVS.hlsl", VERTEX_SHADER, bcVS);
	CompileShader(L"RenderTerrain2dPS.hlsl", PIXEL_SHADER, bcPS);

	DXGI_SAMPLE_DESC sampleDesc = {};
	sampleDesc.Count = 1; // turns multi-sampling off. Not supported feature for my card.
	
	// create the pipeline state object
	D3D12_GRAPHICS_PIPELINE_STATE_DESC descPSO = {};
	descPSO.pRootSignature = sigRoot;
	descPSO.VS = bcVS;
	descPSO.PS = bcPS;
	descPSO.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	descPSO.NumRenderTargets = 1;
	descPSO.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	descPSO.SampleDesc = sampleDesc;
	descPSO.SampleMask = UINT_MAX;
	descPSO.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	descPSO.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	descPSO.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	descPSO.DepthStencilState.DepthEnable = false;
	descPSO.DepthStencilState.StencilEnable = false;
	
	ID3D12PipelineState* pso;
	m_pDev->CreatePSO(&descPSO, pso);
	m_listPSOs.push_back(pso); // save a copy of the pointer to the PSO.
}

// Initialize the root signature and pipeline state object for rendering the terrain in 3D.
void Scene::InitPipelineTerrain3D() {
	// set up the Root Signature.
	// create a descriptor table.
	CD3DX12_ROOT_PARAMETER paramsRoot[6];
	CD3DX12_DESCRIPTOR_RANGE rangesRoot[6];
	
	// height map
	rangesRoot[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	paramsRoot[0].InitAsDescriptorTable(1, &rangesRoot[0]);
	// displacement map
	rangesRoot[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
	paramsRoot[1].InitAsDescriptorTable(1, &rangesRoot[1]);
	// terrain constants
	rangesRoot[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	paramsRoot[2].InitAsDescriptorTable(1, &rangesRoot[2]);
	// frame constants
	rangesRoot[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
	paramsRoot[3].InitAsDescriptorTable(1, &rangesRoot[3]);
	// shadow atlas
	rangesRoot[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
	paramsRoot[4].InitAsDescriptorTable(1, &rangesRoot[4]);
	// material
	rangesRoot[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);
	paramsRoot[5].InitAsDescriptorTable(1, &rangesRoot[5], D3D12_SHADER_VISIBILITY_PIXEL);

	// create our texture samplers for the heightmap.
	CD3DX12_STATIC_SAMPLER_DESC	descSamplers[4];
	descSamplers[0].Init(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
	descSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	descSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	descSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	descSamplers[1].Init(1, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
	descSamplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_DOMAIN;
	descSamplers[2].Init(2, D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT);
	descSamplers[2].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER; 
	descSamplers[2].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	descSamplers[2].MaxAnisotropy = 1;
	descSamplers[2].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	descSamplers[2].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	descSamplers[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	descSamplers[3].Init(3, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
	descSamplers[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// It isn't really necessary to deny the other shaders access, but it does technically allow the GPU to optimize more.
	CD3DX12_ROOT_SIGNATURE_DESC	descRoot;
	descRoot.Init(_countof(paramsRoot), paramsRoot, 4, descSamplers, D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	ID3D12RootSignature* sigRoot;
	m_pDev->CreateRootSig(&descRoot, sigRoot);
	m_listRootSigs.push_back(sigRoot);

	D3D12_SHADER_BYTECODE bcPS = {};
	D3D12_SHADER_BYTECODE bcVS = {};
	D3D12_SHADER_BYTECODE bcHS = {};
	D3D12_SHADER_BYTECODE bcDS = {};
	CompileShader(L"RenderTerrainTessVS.hlsl", VERTEX_SHADER, bcVS);
	CompileShader(L"RenderTerrainTessPS.hlsl", PIXEL_SHADER, bcPS);
	CompileShader(L"RenderTerrainTessHS.hlsl", HULL_SHADER, bcHS);
	CompileShader(L"RenderTerrainTessDS.hlsl", DOMAIN_SHADER, bcDS);

	DXGI_SAMPLE_DESC descSample = {};
	descSample.Count = 1; // turns multi-sampling off. Not supported feature for my card.
	
	// create the pipeline state object
	// create input layout.
	D3D12_INPUT_LAYOUT_DESC	descInputLayout = {};
	D3D12_INPUT_ELEMENT_DESC descElementLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "POSITION", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "POSITION", 2, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "SKIRT", 0, DXGI_FORMAT_R32_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
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
	descPSO.NumRenderTargets = 1;
	descPSO.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	descPSO.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	descPSO.SampleDesc = descSample;
	descPSO.SampleMask = UINT_MAX;
	descPSO.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	descPSO.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	descPSO.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	descPSO.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	descPSO.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

	ID3D12PipelineState* pso;
	m_pDev->CreatePSO(&descPSO, pso);
	m_listPSOs.push_back(pso);
}

// Initialize the root signature and pipeline state object for rendering to the shadow map.
void Scene::InitPipelineShadowMap() {
	// set up the Root Signature.
	// create a descriptor table with 2 entries for the descriptor heap containing our SRV to the heightmap and our CBV.
	CD3DX12_ROOT_PARAMETER paramsRoot[4];
	CD3DX12_DESCRIPTOR_RANGE rangesRoot[4];
	
	// heightmap
	rangesRoot[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	paramsRoot[0].InitAsDescriptorTable(1, &rangesRoot[0]);
	// displacement map
	rangesRoot[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
	paramsRoot[1].InitAsDescriptorTable(1, &rangesRoot[1]);
	// terrain constants
	rangesRoot[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	paramsRoot[2].InitAsDescriptorTable(1, &rangesRoot[2]);
	// shadow constants
	rangesRoot[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
	paramsRoot[3].InitAsDescriptorTable(1, &rangesRoot[3]);

	// create our texture samplers for the heightmap.
	CD3DX12_STATIC_SAMPLER_DESC	descSamplers[2];
	descSamplers[0].Init(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
	descSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_DOMAIN;
	descSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	descSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	descSamplers[1].Init(1, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
	descSamplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_DOMAIN;

	// It isn't really necessary to deny the other shaders access, but it does technically allow the GPU to optimize more.
	CD3DX12_ROOT_SIGNATURE_DESC	descRoot;
	descRoot.Init(_countof(paramsRoot), paramsRoot, 2, descSamplers, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);
	ID3D12RootSignature* sigRoot;
	m_pDev->CreateRootSig(&descRoot, sigRoot);
	m_listRootSigs.push_back(sigRoot);

	D3D12_SHADER_BYTECODE bcVS = {};
	D3D12_SHADER_BYTECODE bcHS = {};
	D3D12_SHADER_BYTECODE bcDS = {};

	CompileShader(L"RenderTerrainTessVS.hlsl", VERTEX_SHADER, bcVS);
	CompileShader(L"RenderShadowMapHS.hlsl", HULL_SHADER, bcHS);
	CompileShader(L"RenderShadowMapDS.hlsl", DOMAIN_SHADER, bcDS);

	DXGI_SAMPLE_DESC descSample = {};
	descSample.Count = 1; // turns multi-sampling off. Not supported feature for my card.
						  
	// create the pipeline state object
	// create input layout.
	D3D12_INPUT_LAYOUT_DESC	descInputLayout = {};
	D3D12_INPUT_ELEMENT_DESC descElementLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "POSITION", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "POSITION", 2, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "SKIRT", 0, DXGI_FORMAT_R32_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	descInputLayout.NumElements = sizeof(descElementLayout) / sizeof(D3D12_INPUT_ELEMENT_DESC);
	descInputLayout.pInputElementDescs = descElementLayout;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC descPSO = {};
	descPSO.pRootSignature = sigRoot;
	descPSO.InputLayout = descInputLayout;
	descPSO.VS = bcVS;
	descPSO.HS = bcHS;
	descPSO.DS = bcDS;
	descPSO.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
	descPSO.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	descPSO.NumRenderTargets = 0;
	descPSO.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	descPSO.SampleDesc = descSample;
	descPSO.SampleMask = UINT_MAX;
	descPSO.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	descPSO.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	descPSO.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	descPSO.RasterizerState.DepthBias = 10000;
	descPSO.RasterizerState.DepthBiasClamp = 0.0f;
	descPSO.RasterizerState.SlopeScaledDepthBias = 1.0f;
	descPSO.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	descPSO.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

	ID3D12PipelineState* pso;
	m_pDev->CreatePSO(&descPSO, pso);
	m_listPSOs.push_back(pso);
}

void Scene::SetViewport(ID3D12GraphicsCommandList* cmdList) {
	cmdList->RSSetViewports(1, &m_vpMain);
	cmdList->RSSetScissorRects(1, &m_srMain);
}

void Scene::DrawShadowMap(ID3D12GraphicsCommandList* cmdList) {
	m_pFrames[m_iFrame]->BeginShadowPass(cmdList);

	cmdList->SetPipelineState(m_listPSOs[2]);
	cmdList->SetGraphicsRootSignature(m_listRootSigs[2]);

	ID3D12DescriptorHeap* heaps[] = { m_ResMgr.GetCBVSRVUAVHeap() };
	cmdList->SetDescriptorHeaps(_countof(heaps), heaps);

	// Tell the terrain to attach its resources.
	m_pT->AttachTerrainResources(cmdList, 0, 1, 2);

	for (int i = 0; i < 4; ++i) {
		// fill in this cascade's shadow constants.
		ShadowMapShaderConstants constants;
		constants.shadowViewProj = m_DNC.GetShadowViewProjMatrix(i);
		constants.eye = m_Cam.GetEyePosition();
		m_DNC.GetShadowFrustum(i, constants.frustum);
		m_pFrames[m_iFrame]->SetShadowConstants(constants, i);
		m_pFrames[m_iFrame]->AttachShadowPassResources(i, cmdList, 3);

		// mDrawMode = 0/false for 2D rendering and 1/true for 3D rendering
		m_pT->Draw(cmdList, true);
	}

	m_pFrames[m_iFrame]->EndShadowPass(cmdList);
}

void Scene::DrawTerrain(ID3D12GraphicsCommandList* cmdList) {
	const float clearColor[] = { 0.2f, 0.6f, 1.0f, 1.0f };
	m_pFrames[m_iFrame]->BeginRenderPass(cmdList, clearColor);

	cmdList->SetPipelineState(m_listPSOs[m_drawMode]);
	cmdList->SetGraphicsRootSignature(m_listRootSigs[m_drawMode]);

	SetViewport(cmdList);

	ID3D12DescriptorHeap* heaps[] = { m_ResMgr.GetCBVSRVUAVHeap() };
	cmdList->SetDescriptorHeaps(_countof(heaps), heaps);

	// Tell the terrain to attach its resources.
	m_pT->AttachTerrainResources(cmdList, 0, 1, 2);

	if (m_drawMode) {
		// set the constant buffers.
		XMFLOAT4 frustum[6];
		m_Cam.GetViewFrustum(frustum);

		PerFrameConstantBuffer constants;
		constants.viewproj = m_Cam.GetViewProjectionMatrixTransposed();
		for (int i = 0; i < 4; ++i) {
			constants.shadowtexmatrices[i] = m_DNC.GetShadowViewProjTexMatrix(i);
		}
		constants.eye = m_Cam.GetEyePosition();
		constants.frustum[0] = frustum[0];
		constants.frustum[1] = frustum[1];
		constants.frustum[2] = frustum[2];
		constants.frustum[3] = frustum[3];
		constants.frustum[4] = frustum[4];
		constants.frustum[5] = frustum[5];
		constants.light = m_DNC.GetLight();
		constants.useTextures = m_UseTextures;
		m_pFrames[m_iFrame]->SetFrameConstants(constants);
		m_pFrames[m_iFrame]->AttachFrameResources(cmdList, 4, 3);

		m_pT->AttachMaterialResources(cmdList, 5);
	}
	
	// mDrawMode = 0/false for 2D rendering and 1/true for 3D rendering
	m_pT->Draw(cmdList, m_drawMode);

	m_pFrames[m_iFrame]->EndRenderPass(cmdList);
}

void Scene::Draw() {
	m_pFrames[m_iFrame]->Reset();
	m_pFrames[m_iFrame]->AttachCommandList(m_pCmdList);

	DrawShadowMap(m_pCmdList);
	
	DrawTerrain(m_pCmdList);

	CloseCommandLists();
	ID3D12CommandList* lCmds[] = { m_pCmdList };
	m_pDev->ExecuteCommandLists(lCmds, __crt_countof(lCmds));
	m_pDev->Present();
}

void Scene::Update() {
	float h = m_pT->GetHeightMapDepth() / 2.0f;
	float w = m_pT->GetHeightMapWidth() / 2.0f;
	m_DNC.Update(XMFLOAT3(w, h, 0.0f), sqrtf(w * w + h * h), &m_Cam);

	m_iFrame = m_pDev->GetCurrentBackBuffer();
	Draw();
}

// function allowing the main program to pass keyboard input to the scene.
void Scene::HandleKeyboardInput(UINT key) {
	switch (key) {
		case _W:
			if (m_drawMode > 0) m_Cam.Translate(XMFLOAT3(MOVE_STEP, 0.0f, 0.0f));
			break;
		case _S:
			if (m_drawMode > 0) m_Cam.Translate(XMFLOAT3(-MOVE_STEP, 0.0f, 0.0f));
			break;
		case _A:
			if (m_drawMode > 0) m_Cam.Translate(XMFLOAT3(0.0f, MOVE_STEP, 0.0f));
			break;
		case _D:
			if (m_drawMode > 0) m_Cam.Translate(XMFLOAT3(0.0f, -MOVE_STEP, 0.0f));
			break;
		case _Q:
			if (m_drawMode > 0) m_Cam.Translate(XMFLOAT3(0.0f, 0.0f, MOVE_STEP));
			break;
		case _Z:
			if (m_drawMode > 0) m_Cam.Translate(XMFLOAT3(0.0f, 0.0f, -MOVE_STEP));
			break;
		case _1: // draw in 2D. draw heightmap.
			m_drawMode = 0;
			break;
		case _2: // draw in 3D.
			m_drawMode = 1;
			break;
		case _T:
			m_UseTextures = !m_UseTextures;
			break;
		case VK_SPACE:
			m_DNC.TogglePause();
			break;
	}
}

// function allowing the main program to pass mouse input to the scene.
void Scene::HandleMouseInput(int x, int y) {
	if (m_drawMode > 0) {
		m_Cam.Pitch(ROT_ANGLE * y);
		m_Cam.Yaw(-ROT_ANGLE * x);
	}
}