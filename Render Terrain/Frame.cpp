/*
Frame.cpp

Author:			Chris Serson
Last Edited:	October 12, 2016

Description:	Class for handling all per frame interation for Render Terrain application.
*/
#include "Frame.h"
#include <string>

Frame::Frame(unsigned int indexFrame, Device* dev, ResourceManager* rm, unsigned int h, unsigned int w, 
	unsigned int dimShadowAtlas) : m_pDev(dev), m_pResMgr(rm), m_iFrame(indexFrame), m_hScreen(h), 
	m_wScreen(w), m_wShadowAtlas(dimShadowAtlas), m_hShadowAtlas(dimShadowAtlas) {
	m_pCmdAllocator = nullptr;
	m_pBackBuffer = nullptr;
	m_pDepthStencilBuffer = nullptr;
	m_pFence = nullptr;
	m_hdlFenceEvent = nullptr;
	m_pFrameConstants = nullptr;
	m_pFrameConstantsMapped = nullptr;
	for (int i = 0; i < 4; ++i) {
		m_pShadowConstants[i] = nullptr;
		m_pShadowConstantsMapped[i] = nullptr;
	}

	m_pDev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, m_pCmdAllocator);

	m_pDev->GetBackBuffer(m_iFrame, m_pBackBuffer);
	m_pBackBuffer->SetName((L"Back Buffer " + std::to_wstring(m_iFrame)).c_str());
	m_pResMgr->AddExistingResource(m_pBackBuffer);
	m_pResMgr->AddRTV(m_pBackBuffer, NULL, m_hdlBackBuffer);

	// create optimized clear value and default heap for DSB.
	D3D12_CLEAR_VALUE dsOptimizedClearValue = {};
	dsOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	dsOptimizedClearValue.DepthStencil.Depth = 1.0f;
	dsOptimizedClearValue.DepthStencil.Stencil = 0;

	m_pResMgr->NewBuffer(m_pDepthStencilBuffer,
		&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, m_wScreen, m_hScreen, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, D3D12_RESOURCE_STATE_DEPTH_WRITE, &dsOptimizedClearValue);
	m_pDepthStencilBuffer->SetName((L"Depth/Stencil Buffer " + std::to_wstring(m_iFrame)).c_str());

	// create the depth/stencil view
	D3D12_DEPTH_STENCIL_VIEW_DESC descDSV = {};
	descDSV.Format = dsOptimizedClearValue.Format;
	descDSV.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	descDSV.Flags = D3D12_DSV_FLAG_NONE;
	m_pResMgr->AddDSV(m_pDepthStencilBuffer, &descDSV, m_hdlDSV);

	m_valFence = 0;
	m_pDev->CreateFence(m_valFence, D3D12_FENCE_FLAG_NONE, m_pFence);
	m_hdlFenceEvent = CreateEvent(NULL, false, false, NULL);
	if (!m_hdlFenceEvent) {
		throw GFX_Exception("Frame::Frame: Create Fence Event failed on init.");
	}

	InitShadowAtlas();
	InitConstantBuffers();
}

Frame::~Frame() {
	CloseHandle(m_hdlFenceEvent);

	if (m_pFence) {
		m_pFence->Release();
		m_pFence = nullptr;
	}

	if (m_pCmdAllocator) {
		m_pCmdAllocator->Release();
		m_pCmdAllocator = nullptr;
	}

	m_pDev = nullptr;
	m_pResMgr = nullptr;
	m_pDepthStencilBuffer = nullptr;
	m_pBackBuffer = nullptr;
	
	if (m_pFrameConstants) {
		m_pFrameConstants->Unmap(0, nullptr);
		m_pFrameConstantsMapped = nullptr;
		m_pFrameConstants = nullptr;
	}

	for (int i = 0; i < 4; ++i) {
		if (m_pShadowConstants[i]) {
			m_pShadowConstants[i]->Unmap(0, nullptr);
			m_pShadowConstantsMapped[i] = nullptr;
			m_pShadowConstants[i] = nullptr;
		}
	}
}

void Frame::InitShadowAtlas() {
	// shadow atlas is broken into 4 equal maps. Create a viewport and scissor rectangle for each.
	// to do so, divide atlas in 2 along both x and y axes.
	int w = m_wShadowAtlas / 2;
	int h = m_hShadowAtlas / 2;
	for (int i = 0; i < 2; ++i) {
		for (int j = 0; j < 2; ++j) {
			int index = i * 2 + j;
			m_vpShadowAtlas[index].TopLeftX = (float)(i * w);
			m_vpShadowAtlas[index].TopLeftY = (float)(j * h);
			m_vpShadowAtlas[index].Width = (float)w;
			m_vpShadowAtlas[index].Height = (float)h;
			m_vpShadowAtlas[index].MinDepth = 0;
			m_vpShadowAtlas[index].MaxDepth = 1;

			m_srShadowAtlas[index].left = i * w;
			m_srShadowAtlas[index].top = j * h;
			m_srShadowAtlas[index].right = m_srShadowAtlas[index].left + w;
			m_srShadowAtlas[index].bottom = m_srShadowAtlas[index].top + h;
		}
	}

	// Create the shadow map texture buffer.
	D3D12_RESOURCE_DESC	descTex = {};
	descTex.Alignment = 0;
	descTex.MipLevels = 1;
	descTex.Format = DXGI_FORMAT_R24G8_TYPELESS;
	descTex.Width = m_wShadowAtlas;
	descTex.Height = m_hShadowAtlas;
	descTex.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	descTex.DepthOrArraySize = 1;
	descTex.SampleDesc.Count = 1;
	descTex.SampleDesc.Quality = 0;
	descTex.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	descTex.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	D3D12_CLEAR_VALUE clearValue;	// Performance tip: Tell the runtime at resource creation the desired clear value. (per microsoft examples)
	clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	clearValue.DepthStencil.Depth = 1.0f;
	clearValue.DepthStencil.Stencil = 0;

	D3D12_DEPTH_STENCIL_VIEW_DESC descDSV = {};
	descDSV.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	descDSV.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	descDSV.Flags = D3D12_DSV_FLAG_NONE;

	// Create the SRV for the heightmap texture and save to Terrain object.
	D3D12_SHADER_RESOURCE_VIEW_DESC	descSRV = {};
	descSRV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	descSRV.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	descSRV.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	descSRV.Texture2D.MipLevels = descTex.MipLevels;
	descSRV.Texture2D.MostDetailedMip = 0;
	descSRV.Texture2D.ResourceMinLODClamp = 0.0f;
	descSRV.Texture2D.PlaneSlice = 0;

	m_pResMgr->NewBuffer(m_pShadowAtlas, &descTex, &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearValue);
	m_pShadowAtlas->SetName((L"Shadow Atlas Texture " + std::to_wstring(m_iFrame)).c_str());
	m_pResMgr->AddDSV(m_pShadowAtlas, &descDSV, m_hdlShadowAtlasDSV);
	m_pResMgr->AddSRV(m_pShadowAtlas, &descSRV, m_hdlShadowAtlasSRV_CPU, m_hdlShadowAtlasSRV_GPU);
}

void Frame::InitConstantBuffers() {
	// initialize frame constant buffer
	// Create an upload buffer for the CBV
	UINT64 sizeofBuffer = sizeof(PerFrameConstantBuffer);
	m_pResMgr->NewBuffer(m_pFrameConstants, &CD3DX12_RESOURCE_DESC::Buffer(sizeofBuffer), 
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);
	m_pFrameConstants->SetName((L"Frame Constant Buffer " + std::to_wstring(m_iFrame)).c_str());

	// Create the CBV itself
	D3D12_CONSTANT_BUFFER_VIEW_DESC	descCBV = {};
	descCBV.BufferLocation = m_pFrameConstants->GetGPUVirtualAddress();
	descCBV.SizeInBytes = (sizeofBuffer + 255) & ~255; // CB size is required to be 256-byte aligned.
	m_pResMgr->AddCBV(&descCBV, m_hdlFrameConstantsCBV_CPU, m_hdlFrameConstantsCBV_GPU);
	
	// initialize and map the constant buffers.
	// per the DirectX 12 sample code, we can leave this mapped until we close.
	CD3DX12_RANGE rangeRead(0, 0); // we won't be reading from this resource
	if (FAILED(m_pFrameConstants->Map(0, &rangeRead, reinterpret_cast<void**>(&m_pFrameConstantsMapped)))) {
		throw (GFX_Exception("Frame::InitConstantBuffers failed on Frame Constant Buffer."));
	}

	// initialize constant buffer for each shadow map in atlas.
	for (int i = 0; i < 4; ++i) {
		// Create an upload buffer for the CBV
		sizeofBuffer = sizeof(ShadowMapShaderConstants);
		m_pResMgr->NewBuffer(m_pShadowConstants[i], &CD3DX12_RESOURCE_DESC::Buffer(sizeofBuffer),
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);
		m_pShadowConstants[i]->SetName((L"Shadow Constant Buffer " + std::to_wstring(m_iFrame) + L", " + std::to_wstring(i)).c_str());

		// Create the CBV itself
		descCBV.BufferLocation = m_pShadowConstants[i]->GetGPUVirtualAddress();
		descCBV.SizeInBytes = (sizeofBuffer + 255) & ~255; // CB size is required to be 256-byte aligned.
		m_pResMgr->AddCBV(&descCBV, m_hdlShadowConstantsCBV_CPU[i], m_hdlShadowConstantsCBV_GPU[i]);

		// initialize and map the constant buffers.
		if (FAILED(m_pShadowConstants[i]->Map(0, &rangeRead, reinterpret_cast<void**>(&m_pShadowConstantsMapped[i])))) {
			throw (GFX_Exception(("Frame::InitConstantBuffers failed on Shadow Constant Buffer " + std::to_string(i)).c_str()));
		}
	}
}

// Wait for GPU to signal it has completed with the previous iteration of this frame.
void Frame::WaitForGPU() {
	// add fence signal.
	m_pDev->SetFence(m_pFence, m_valFence);
		
	// if the current value returned by the fence is less than the current fence value for this buffer, then we know the GPU is not done with the buffer, so wait.
	if (FAILED(m_pFence->SetEventOnCompletion(m_valFence, m_hdlFenceEvent))) {
		throw GFX_Exception("Frame::WaitForGPU failed to SetEventOnCompletion.");
	}

	WaitForSingleObject(m_hdlFenceEvent, INFINITE);
		
	++m_valFence;
}

// Confirm the previous GPU activity on this frame is completed and reset the command allocator for the next run.
void Frame::Reset() {
	WaitForGPU();

	// reset command allocator.
	// This appears to only be necessary when you want to associate a different command list with the command allocator.
	// You can simply reset the same command list and keep going without resetting the command allocator.
//	if (FAILED(m_pCmdAllocator->Reset())) {
//		throw GFX_Exception("Frame::Reset: m_pCmdAllocator Reset failed.");
//	}
}

// Resets a command list for use with this frame.
void Frame::AttachCommandList(ID3D12GraphicsCommandList* cmdList) {
	if (FAILED(cmdList->Reset(m_pCmdAllocator, NULL))) {
		throw GFX_Exception("Frame::AttachCommandList: CommandList Reset failed.");
	}
}

// Set the Frame Constant buffer.
void Frame::SetFrameConstants(PerFrameConstantBuffer frameConstants) {
	memcpy(m_pFrameConstantsMapped, &frameConstants, sizeof(PerFrameConstantBuffer));
}

// Set the Shadow Constant buffer. i refers to which shadow map you're setting the constants for.
void Frame::SetShadowConstants(ShadowMapShaderConstants shadowConstants, unsigned int i) {
	memcpy(m_pShadowConstantsMapped[i], &shadowConstants, sizeof(ShadowMapShaderConstants));
}

// Call at the start of rendering the shadow passes to switch the shadow atlas to write mode.
void Frame::BeginShadowPass(ID3D12GraphicsCommandList* cmdList) {
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_pShadowAtlas,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	cmdList->ClearDepthStencilView(m_hdlShadowAtlasDSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	cmdList->OMSetRenderTargets(0, nullptr, false, &m_hdlShadowAtlasDSV);
}

// Call at the end of rendering the shadow passes to switch the shadow atlas to read mode.
void Frame::EndShadowPass(ID3D12GraphicsCommandList* cmdList) {
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_pShadowAtlas,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
}

// Sets the back buffer for rendering and makes it the render target. Clears to clearColor.
void Frame::BeginRenderPass(ID3D12GraphicsCommandList* cmdList, const float clearColor[4]) {
	// set back buffer to render target.
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_pBackBuffer, 
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	cmdList->ClearRenderTargetView(m_hdlBackBuffer, clearColor, 0, NULL);
	cmdList->ClearDepthStencilView(m_hdlDSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	// get the handle to the back buffer and set as render target.
	cmdList->OMSetRenderTargets(1, &m_hdlBackBuffer, false, &m_hdlDSV);
}

// Sets the back buffer to present.
void Frame::EndRenderPass(ID3D12GraphicsCommandList* cmdList) {
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_pBackBuffer,
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
}

// Attach the shadow pass resources to the provided command list for shadow pass i.
// Requires the index into the root descriptor table to attach the shadow constant buffer to.
void Frame::AttachShadowPassResources(unsigned int i, ID3D12GraphicsCommandList* cmdList, 
	unsigned int cbvDescTableIndex) {
	cmdList->RSSetViewports(1, &m_vpShadowAtlas[i]);
	cmdList->RSSetScissorRects(1, &m_srShadowAtlas[i]);

	cmdList->SetGraphicsRootDescriptorTable(cbvDescTableIndex, m_hdlShadowConstantsCBV_GPU[i]);
}

// Attach the frame resources needed for the normal render pass.
// Requires the indices into the root descriptor table to attach the shadow srv and shadow constant buffer to.
void Frame::AttachFrameResources(ID3D12GraphicsCommandList* cmdList, unsigned int srvDescTableIndex, 
	unsigned int cbvDescTableIndex) {
	cmdList->SetGraphicsRootDescriptorTable(srvDescTableIndex, m_hdlShadowAtlasSRV_GPU);
	cmdList->SetGraphicsRootDescriptorTable(cbvDescTableIndex, m_hdlFrameConstantsCBV_GPU);
}