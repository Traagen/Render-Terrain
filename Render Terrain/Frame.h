/*
Frame.h

Author:			Chris Serson
Last Edited:	October 12, 2016

Description:	Class for handling all per frame interation for Render Terrain application.

Usage:			- Frame F(...);
				- Frame* F; F = new Frame(...);
				- Proper shutdown is handled by the destructor.
				- Create a Frame object for each frame, ie 3 for triple buffering.
*/
#pragma once

#include "ResourceManager.h"
#include "Light.h"

using namespace graphics;

struct PerFrameConstantBuffer {
	XMFLOAT4X4	viewproj;
	XMFLOAT4X4	shadowtexmatrices[4];
	XMFLOAT4	eye;
	XMFLOAT4	frustum[6];
	LightSource light;
	BOOL		useTextures;
};

struct ShadowMapShaderConstants {
	XMFLOAT4X4	shadowViewProj;
	XMFLOAT4	eye;
	XMFLOAT4	frustum[4];
};

class Frame {
public:
	Frame(unsigned int indexFrame, Device* dev, ResourceManager* rm, unsigned int h, unsigned int w, 
		unsigned int dimShadowAtlas = 4096);
	~Frame();

	ID3D12CommandAllocator* GetAllocator() { return m_pCmdAllocator; }

	// Confirm the previous GPU activity on this frame is completed and reset the command allocator for the next run.
	void Reset();
	// Resets a command list for use with this frame.
	void AttachCommandList(ID3D12GraphicsCommandList* cmdList);

	// Set the Frame Constant buffer.
	void SetFrameConstants(PerFrameConstantBuffer frameConstants);
	// Set the Shadow Constant buffer. i refers to which shadow map you're setting the constants for.
	void SetShadowConstants(ShadowMapShaderConstants shadowConstants, unsigned int i);

	// Call at the start of rendering the shadow passes to switch the shadow atlas to write mode.
	void BeginShadowPass(ID3D12GraphicsCommandList* cmdList);
	// Call at the end of rendering the shadow passes to switch the shadow atlas to read mode.
	void EndShadowPass(ID3D12GraphicsCommandList* cmdList);
	// Sets the back buffer for rendering and makes it the render target. Clears to clearColor.
	void BeginRenderPass(ID3D12GraphicsCommandList* cmdList, const float clearColor[4]);
	// Sets the back buffer to present.
	void EndRenderPass(ID3D12GraphicsCommandList* cmdList);
	// Attach the shadow pass resources to the provided command list for shadow pass i.
	// Requires the index into the root descriptor table to attach the shadow constant buffer to.
	void AttachShadowPassResources(unsigned int i, ID3D12GraphicsCommandList* cmdList, 
		unsigned int cbvDescTableIndex);
	// Attach the frame resources needed for the normal render pass.
	// Requires the indices into the root descriptor table to attach the shadow srv and shadow constant buffer to.
	void AttachFrameResources(ID3D12GraphicsCommandList* cmdList, unsigned int srvDescTableIndex, 
		unsigned int cbvDescTableIndex);
	
private:
	void InitShadowAtlas();
	void InitConstantBuffers();

	// Wait for GPU to signal it has completed with the previous iteration of this frame.
	void WaitForGPU();

	Device*						m_pDev;
	ResourceManager*			m_pResMgr;
	ID3D12CommandAllocator*		m_pCmdAllocator;
	ID3D12Resource*				m_pBackBuffer;
	ID3D12Resource*				m_pDepthStencilBuffer;
	ID3D12Resource*				m_pShadowAtlas;
	ID3D12Resource*				m_pFrameConstants;
	ID3D12Resource*				m_pShadowConstants[4];
	ID3D12Fence*				m_pFence;
	HANDLE						m_hdlFenceEvent;
	D3D12_CPU_DESCRIPTOR_HANDLE m_hdlBackBuffer;
	D3D12_CPU_DESCRIPTOR_HANDLE m_hdlDSV;
	D3D12_CPU_DESCRIPTOR_HANDLE m_hdlShadowAtlasDSV;
	D3D12_CPU_DESCRIPTOR_HANDLE m_hdlShadowAtlasSRV_CPU;
	D3D12_GPU_DESCRIPTOR_HANDLE m_hdlShadowAtlasSRV_GPU;
	D3D12_CPU_DESCRIPTOR_HANDLE m_hdlFrameConstantsCBV_CPU;
	D3D12_GPU_DESCRIPTOR_HANDLE m_hdlFrameConstantsCBV_GPU;
	D3D12_CPU_DESCRIPTOR_HANDLE m_hdlShadowConstantsCBV_CPU[4];
	D3D12_GPU_DESCRIPTOR_HANDLE m_hdlShadowConstantsCBV_GPU[4];
	D3D12_VIEWPORT				m_vpShadowAtlas[4];
	D3D12_RECT					m_srShadowAtlas[4];
	PerFrameConstantBuffer*		m_pFrameConstantsMapped;
	ShadowMapShaderConstants*	m_pShadowConstantsMapped[4];
	unsigned long long			m_valFence;						// Value to check fence against to confirm GPU is done.
	unsigned int				m_iFrame;						// Which frame number is this frame?
	unsigned int				m_wScreen;
	unsigned int				m_hScreen;
	unsigned int				m_wShadowAtlas;
	unsigned int				m_hShadowAtlas;
};

