/*
Graphics.cpp

Author:			Chris Serson
Last Edited:	October 12, 2016

Description:	Class for creating and managing a Direct3D 12 instance.
*/
#include "Graphics.h"
#include <string>

namespace graphics {
	/* Definitions for Non-class-specific Functions. */
	// Compile the specified shader.
	void CompileShader(LPCWSTR fn, ShaderType st, D3D12_SHADER_BYTECODE& bcShader) {
		LPCSTR version;
		switch (st) {
		case PIXEL_SHADER:
			version = "ps_5_0";
			break;
		case VERTEX_SHADER:
			version = "vs_5_0";
			break;
		case GEOMETRY_SHADER:
			version = "gs_5_0";
			break;
		case HULL_SHADER:
			version = "hs_5_0";
			break;
		case DOMAIN_SHADER:
			version = "ds_5_0";
			break;
		default:
			version = ""; // will break on attempting to compile as not valid.
		}

		ID3DBlob* shader;
		ID3DBlob* err;
		if (FAILED(D3DCompileFromFile(fn, NULL, NULL, "main", version, D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &shader, &err))) {
			if (shader) shader->Release();
			if (err) {
				std::string msg((char *)err->GetBufferPointer());
				msg = "graphics::CompileShader: " + msg;
				throw GFX_Exception(msg.c_str());
			}
			else {
				std::string msg(version);
				msg = "graphics::CompileShader: Failed to compile shader version " + msg + ". No error returned from compiler";
				throw GFX_Exception(msg.c_str());
			}
		}
		bcShader.BytecodeLength = shader->GetBufferSize();
		bcShader.pShaderBytecode = shader->GetBufferPointer();
	}

	/* Definitions for Device Class */
	Device::Device(HWND win, unsigned int h, unsigned int w, bool fullscreen, unsigned int numFrames) : 
		m_hScreen(h), m_wScreen(w), m_isWindowed(!fullscreen), m_numFrames(numFrames) {
		m_pDev = nullptr;
		m_pCmdQ = nullptr;
		m_pSwapChain = nullptr;
		
		// Create a DirectX graphics interface factory.
		IDXGIFactory4* factory;
		if (FAILED(CreateDXGIFactory2(FACTORY_DEBUG, IID_PPV_ARGS(&factory))))
		{
			throw GFX_Exception("In Device::Device: CreateDXGIFactory2 failed.");
		}

		// Search for a DirectX 12 compatible Hardware device (ie graphics card). Minimum feature level = 11.0
		int				adapterIndex = 0;
		bool			adapterFound = false;
		IDXGIAdapter1*	adapter;
		while (factory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND) {
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
				// we don't want a software device (emulator).
				++adapterIndex;
				continue;
			}

			if (SUCCEEDED(D3D12CreateDevice(adapter, FEATURE_LEVEL, _uuidof(ID3D12Device), NULL))) {
				adapterFound = true;
				break;
			}

			++adapterIndex;
		}
		if (!adapterFound) {
			throw GFX_Exception("In Device::Device: No DirectX 12 compatible graphics card found on init.");
		}

		// attempt to create the device.
		if (FAILED(D3D12CreateDevice(adapter, FEATURE_LEVEL, IID_PPV_ARGS(&m_pDev)))) {
			throw GFX_Exception("In Device::Device: D3D12CreateDevice failed on init.");
		}

		// attempt to create the command queue.
		D3D12_COMMAND_QUEUE_DESC descCmdQ = {};
		descCmdQ.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		descCmdQ.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		descCmdQ.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		descCmdQ.NodeMask = 0;

		if (FAILED(m_pDev->CreateCommandQueue(&descCmdQ, IID_PPV_ARGS(&m_pCmdQ)))) {
			throw GFX_Exception("In Device::Device: CreateCommandQueue failed on init.");
		}

		// attempt to create the swap chain.
		DXGI_SAMPLE_DESC descSample = {};
		descSample.Count = 1; // turns multi-sampling off. Not supported feature for my card.
		DXGI_SWAP_CHAIN_DESC descSwapChain = {};
		descSwapChain.BufferCount = m_numFrames;
		descSwapChain.BufferDesc.Height = m_hScreen;
		descSwapChain.BufferDesc.Width = m_wScreen;
		descSwapChain.BufferDesc.Format = DESIRED_FORMAT;
		descSwapChain.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // this says the pipeline will render to this swap chain
		descSwapChain.OutputWindow = win;
		descSwapChain.Windowed = m_isWindowed;
		descSwapChain.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		descSwapChain.SampleDesc = descSample;

		// create temporary swapchain.
		IDXGISwapChain* swapChain;
		if (FAILED(factory->CreateSwapChain(m_pCmdQ, &descSwapChain, &swapChain))) {
			throw GFX_Exception("In Device::Device: CreateSwapChain failed on init.");
		}
		// upgrade swapchain to swapchain3 and store in mpSwapChain.
		m_pSwapChain = static_cast<IDXGISwapChain3*>(swapChain);

		// clean up.
		swapChain = NULL;
		if (factory) {
			factory->Release();
			factory = NULL;
		}
	}

	Device::~Device() {
		if (m_pSwapChain) {
			m_pSwapChain->SetFullscreenState(false, NULL); // ensure swap chain in windowed mode before releasing.
			m_pSwapChain->Release();
			m_pSwapChain = nullptr;
		}

		if (m_pCmdQ) {
			m_pCmdQ->Release();
			m_pCmdQ = nullptr;
		}
		if (m_pDev) {
			m_pDev->Release();
			m_pDev = nullptr;
		}
	}

	// Return the heap descriptor size for the specified heap type.
	unsigned int Device::GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE ht) {
		return m_pDev->GetDescriptorHandleIncrementSize(ht);
	}

	// Create and return a pointer to a Command Allocator
	void Device::CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE clt, ID3D12CommandAllocator*& allocator) {
		// attempt to create a command allocator.
		if (FAILED(m_pDev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)))) {
			throw GFX_Exception("Device::CreateCommandAllocator failed.");
		}
	}

	// Return a pointer to the specified back buffer
	void Device::GetBackBuffer(unsigned int i, ID3D12Resource*& buffer) {
		if (i >= m_numFrames || i < 0) {
			throw GFX_Exception("Invalid buffer index provided to Device::GetBackBuffer.");
		}

		if (FAILED(m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&buffer)))) {
			throw GFX_Exception("Swap Chain GetBuffer failed in Device::GetBackBuffer.");
		}
	}

	// Return the index of the initial back buffer
	unsigned int Device::GetCurrentBackBuffer() {
		return m_pSwapChain->GetCurrentBackBufferIndex();
	}

	// Create and return a pointer to a new root signature matching the provided description.
	void Device::CreateRootSig(CD3DX12_ROOT_SIGNATURE_DESC* desc, ID3D12RootSignature*& root) {
		ID3DBlob* err;
		ID3DBlob* sig;

		if (FAILED(D3D12SerializeRootSignature(desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err))) {
			std::string msg((char *)err->GetBufferPointer());
			msg = "In Device::CreateRootSig: " + msg;
			throw GFX_Exception(msg.c_str());
		}
		if (FAILED(m_pDev->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&root)))) {
			throw GFX_Exception("Device::CreateRootSig failed.");
		}
		sig->Release();
	}

	// Create and return a pointer to a new Pipeline State Object matching the provided description.
	void Device::CreatePSO(D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc, ID3D12PipelineState*& pso) {
		if (FAILED(m_pDev->CreateGraphicsPipelineState(desc, IID_PPV_ARGS(&pso)))) {
			throw GFX_Exception("Device::CreateGraphicsPipeline failed.");
		}
	}

	// Create and return a pointer to a Descriptor Heap.
	void Device::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_DESC* desc, ID3D12DescriptorHeap*& heap) {
		if FAILED(m_pDev->CreateDescriptorHeap(desc, IID_PPV_ARGS(&heap))) {
			throw GFX_Exception("Device::CreateDescriptorHeap failed.");
		}
	}

	// Create a Shader Resource view for the supplied resource.
	void Device::CreateSRV(ID3D12Resource*& tex, D3D12_SHADER_RESOURCE_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE handle) {
		m_pDev->CreateShaderResourceView(tex, desc, handle);
	}

	// Create a constant buffer view
	void Device::CreateCBV(D3D12_CONSTANT_BUFFER_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE handle) {
		m_pDev->CreateConstantBufferView(desc, handle);
	}

	// Create a depth/stencil buffer view
	void Device::CreateDSV(ID3D12Resource*& tex, D3D12_DEPTH_STENCIL_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE handle) {
		m_pDev->CreateDepthStencilView(tex, desc, handle);
	}

	// Create a render target view
	void Device::CreateRTV(ID3D12Resource*& tex, D3D12_RENDER_TARGET_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE handle) {
		m_pDev->CreateRenderTargetView(tex, desc, handle);
	}

	// Create a sampler
	void Device::CreateSampler(D3D12_SAMPLER_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE handle) {
		m_pDev->CreateSampler(desc, handle);
	}

	// Create a fence
	void Device::CreateFence(unsigned long long valInit, D3D12_FENCE_FLAGS flags, ID3D12Fence*& fence) {
		if (FAILED(m_pDev->CreateFence(valInit, flags, IID_PPV_ARGS(&fence)))) {
			throw GFX_Exception("Device::CreateFence failed on init.");
		}
	}

	// Create a Command List
	void Device::CreateGraphicsCommandList(D3D12_COMMAND_LIST_TYPE type, ID3D12CommandAllocator* alloc, ID3D12GraphicsCommandList*& list, unsigned int mask, 
		ID3D12PipelineState* psoInit) {
		// create a command list.
		if (FAILED(m_pDev->CreateCommandList(mask, type, alloc, psoInit, IID_PPV_ARGS(&list)))) {
			throw GFX_Exception("Device::CreateCommandList failed.");
		}
	}

	// Create a commited resource. 
	void Device::CreateCommittedResource(ID3D12Resource*& heap, D3D12_RESOURCE_DESC* desc,
		D3D12_HEAP_PROPERTIES* props, D3D12_HEAP_FLAGS flags, D3D12_RESOURCE_STATES state, D3D12_CLEAR_VALUE* clear) {
		/*if (FAILED(m_pDev->CreateCommittedResource(props, flags, desc, state, clear, IID_PPV_ARGS(&heap)))) {
			throw GFX_Exception("Device::CreateCommittedResource failed.");
		}*/
		HRESULT hr = m_pDev->CreateCommittedResource(props, flags, desc, state, clear, IID_PPV_ARGS(&heap));
		if (FAILED(hr)) {
			hr = m_pDev->GetDeviceRemovedReason();
			throw GFX_Exception("Device::CreateCommittedResource failed.");
		}
	}
		
	// Signal Command Queue with provided fence value.
	void Device::SetFence(ID3D12Fence* fence, unsigned long long val) {
		// Add Signal command to set fence to the fence value that indicates the GPU is done with that buffer. 
		if (FAILED(m_pCmdQ->Signal(fence, val))) {
			throw GFX_Exception("Device::SetFence failed.");
		}
	}

	// Run the submitted array of commands
	void Device::ExecuteCommandLists(ID3D12CommandList* lCmds[], unsigned int numCommands) {
		// execute
		m_pCmdQ->ExecuteCommandLists(numCommands, lCmds);
	}

	// Present the latest back buffer on the swap chain.
	void Device::Present() {
		// swap the back buffers.
		if (FAILED(m_pSwapChain->Present(0, 0))) {
			throw GFX_Exception("Device::Present SwapChain failed to present.");
		}
	}
}