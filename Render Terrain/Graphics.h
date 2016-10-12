/*
Graphics.h

Author:			Chris Serson
Last Edited:	October 12, 2016

Description:	Class for creating and managing a Direct3D 12 instance.

Usage:			- Calling the constructor, either through Device DEV(...);
				or Device* DEV; DEV = new Device(...);, will find a
				Direct3D 12 compatible hardware device and initialize
				Direct3D on it.
				- Proper shutdown is handled by the destructor.
				- All requests to the graphics device must go through the
				Device object.
				- The calling object can request a Command List from the Graphics
				object (CreateGraphicsCommandList()).
				- The calling object must tell the Graphics object when to
				reset the pipeline for a new frame (ResetPipeline()), 
				when to swap the buffers (SetBackBufferRender(), SetBackBufferPresent(), 
				and when to actually execute the command list (Render()).

Future Work:	- Add support for compute shaders.
				- Add support for bundles.
				- Add support for reserved and placed resources.
*/
#pragma once

// Linkages
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include "D3DX12.h"
#include <dxgi1_5.h>
#include <DirectXMath.h>
#include <D3DCompiler.h>
#include <stdexcept>

namespace graphics {
	using namespace DirectX;

	static const float SCREEN_DEPTH = 1000.0f;
	static const float SCREEN_NEAR = 0.1f;
	static const UINT FACTORY_DEBUG = DXGI_CREATE_FACTORY_DEBUG; // set to 0 if not debugging, DXGI_CREATE_FACTORY_DEBUG if debugging.
	static const DXGI_FORMAT DESIRED_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
	static const D3D_FEATURE_LEVEL	FEATURE_LEVEL = D3D_FEATURE_LEVEL_11_0; // minimum feature level necessary for DirectX 12 compatibility.
																			// this is all my current card supports.
	enum ShaderType { PIXEL_SHADER, VERTEX_SHADER, GEOMETRY_SHADER, HULL_SHADER, DOMAIN_SHADER };

	class GFX_Exception : public std::runtime_error {
	public:
		GFX_Exception(const char *msg) : std::runtime_error(msg) {}
	};
	
	// Compile the specified shader.
	void CompileShader(LPCWSTR fn, ShaderType st, D3D12_SHADER_BYTECODE& bcShader);

	class Device {
	public:
		Device(HWND win, unsigned int h, unsigned int w, bool fullscreen = false, unsigned int numFrames = 3);
		~Device();

		// Return the heap descriptor size for the specified heap type.
		unsigned int GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE ht);

		// Create and return a pointer to a Command Allocator
		void CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE clt, ID3D12CommandAllocator*& allocator);
		// Return a pointer to the specified back buffer
		void GetBackBuffer(unsigned int i, ID3D12Resource*& buffer);
		// Return the index of the initial back buffer
		unsigned int GetCurrentBackBuffer();
		// Signal Command Queue with provided fence value.
		void SetFence(ID3D12Fence* fence, unsigned long long val);

		// Create and return a pointer to a new root signature matching the provided description.
		void CreateRootSig(CD3DX12_ROOT_SIGNATURE_DESC* desc, ID3D12RootSignature*& root);
		// Create and return a pointer to a new Pipeline State Object matching the provided description.
		void CreatePSO(D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc, ID3D12PipelineState*& pso);
		
		// Create and return a pointer to a Descriptor Heap.
		void CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_DESC* desc, ID3D12DescriptorHeap*& heap);
		// Create a Shader Resource view for the supplied resource.
		void CreateSRV(ID3D12Resource*& tex, D3D12_SHADER_RESOURCE_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE handle);
		// Create a constant buffer view
		void CreateCBV(D3D12_CONSTANT_BUFFER_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE handle);
		// Create a depth/stencil buffer view
		void CreateDSV(ID3D12Resource*& tex, D3D12_DEPTH_STENCIL_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE handle);
		// Create a render target view
		void CreateRTV(ID3D12Resource*& tex, D3D12_RENDER_TARGET_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE handle);
		// Create a sampler
		void CreateSampler(D3D12_SAMPLER_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE handle);
		// Create a fence
		void CreateFence(unsigned long long valInit, D3D12_FENCE_FLAGS flags, ID3D12Fence*& fence);
		// Create a Command List
		void CreateGraphicsCommandList(D3D12_COMMAND_LIST_TYPE type, ID3D12CommandAllocator* alloc, ID3D12GraphicsCommandList*& list,
			unsigned int mask = 0, ID3D12PipelineState* psoInit = nullptr);

		// Create a commited resource. 
		void CreateCommittedResource(ID3D12Resource*& heap, D3D12_RESOURCE_DESC* desc, D3D12_HEAP_PROPERTIES* props, D3D12_HEAP_FLAGS flags,
			D3D12_RESOURCE_STATES state, D3D12_CLEAR_VALUE* clear);

		// Run the submitted array of commands
		void ExecuteCommandLists(ID3D12CommandList* lCmds[], unsigned int numCommands);
		// Present the latest back buffer on the swap chain.
		void Present();
		
	private:
		ID3D12Device*				m_pDev;
		ID3D12CommandQueue*			m_pCmdQ;
		IDXGISwapChain3*			m_pSwapChain;
		unsigned int				m_wScreen;
		unsigned int				m_hScreen;
		unsigned int				m_numFrames;
		bool						m_isWindowed;
	};
};
