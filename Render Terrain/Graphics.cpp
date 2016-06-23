/*
Graphics.cpp

Author:			Chris Serson
Last Edited:	June 23, 2016

Description:	Class for creating and managing a Direct3D 12 instance.
*/
#include "Graphics.h"

namespace graphics {
	Graphics::Graphics(int height, int width, HWND win, bool fullscreen) {
		mpDev = nullptr;
		mpCmdQ = nullptr;
		mpCmdList = nullptr;
		mpSwapChain = nullptr;
		mpRTVHeap = nullptr;
		mFenceEvent = nullptr;
		for (int i = 0; i < FRAME_BUFFER_COUNT; ++i) {
			maCmdAllocators[i] = nullptr;
			maBackBuffers[i] = nullptr;
			maFences[i] = nullptr;
		}

		D3D12_COMMAND_QUEUE_DESC			cmdQDesc = {};
		D3D12_DESCRIPTOR_HEAP_DESC			rtvHeapDesc = {};
		IDXGIFactory4*						factory;
		IDXGIAdapter1*						adapter;
		IDXGISwapChain*						swapChain;
		DXGI_SWAP_CHAIN_DESC				swapChainDesc = {};
		int									adapterIndex;
		bool								adapterFound;

		// Create a DirectX graphics interface factory.
		if (FAILED(CreateDXGIFactory2(FACTORY_DEBUG, IID_PPV_ARGS(&factory))))
		{
			throw GFX_Exception("CreateDXGIFactory2 failed on init.");
		}

		// Search for a DirectX 12 compatible Hardware device (ie graphics card). Minimum feature level = 11.0
		adapterIndex = 0;
		adapterFound = false;

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
			throw GFX_Exception("No DirectX 12 compatible graphics card found on init.");
		}

		// attempt to create the device.
		if (FAILED(D3D12CreateDevice(adapter, FEATURE_LEVEL, IID_PPV_ARGS(&mpDev)))) {
			throw GFX_Exception("D3D12CreateDevice failed on init.");
		}

		// attempt to create the command queue.
		cmdQDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		cmdQDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		cmdQDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		cmdQDesc.NodeMask = 0;

		if (FAILED(mpDev->CreateCommandQueue(&cmdQDesc, IID_PPV_ARGS(&mpCmdQ)))) {
			throw GFX_Exception("CreateCommandQueue failed on init.");
		}

		// attempt to create the swap chain.
		DXGI_SAMPLE_DESC sampleDesc = {};
		sampleDesc.Count = 1; // turns multi-sampling off. Not supported feature for my card.

		swapChainDesc.BufferCount = FRAME_BUFFER_COUNT; // double buffering.
		swapChainDesc.BufferDesc.Height = height;
		swapChainDesc.BufferDesc.Width = width;
		swapChainDesc.BufferDesc.Format = DESIRED_FORMAT;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // this says the pipeline will render to this swap chain
		swapChainDesc.OutputWindow = win;
		swapChainDesc.Windowed = !fullscreen;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.SampleDesc = sampleDesc; 

		// create temporary swapchain.
		if (FAILED(factory->CreateSwapChain(mpCmdQ, &swapChainDesc, &swapChain))) {
			throw GFX_Exception("CreateSwapChain failed on init.");
		}
		// upgrade swapchain to swapchain3 and store in mpSwapChain.
		mpSwapChain = static_cast<IDXGISwapChain3*>(swapChain);

		// clean up.
		swapChain = NULL;
		if (factory) {
			factory->Release();
			factory = NULL;
		}

		// get the index for the current back buffer.
		mBufferIndex = mpSwapChain->GetCurrentBackBufferIndex();

		// create the render target views for the back buffers.
		rtvHeapDesc.NumDescriptors = FRAME_BUFFER_COUNT;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		if (FAILED(mpDev->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&mpRTVHeap)))) {
			throw GFX_Exception("CreateDescriptorHeap failed on init.");
		}

		// record the size of the RTV descriptor heap.
		mRTVDescSize = mpDev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		// get a handle to the first descriptor in the descriptor heap. a handle is basically a pointer,
		// but we cannot literally use it like a c++ pointer.
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mpRTVHeap->GetCPUDescriptorHandleForHeapStart());

		for (int i = 0; i < FRAME_BUFFER_COUNT; ++i) {
			// Get a pointer to the first back buffer from the swap chain.
			if (FAILED(mpSwapChain->GetBuffer(i, IID_PPV_ARGS(&maBackBuffers[i])))) {
				throw GFX_Exception("Swap Chain GetBuffer failed on init.");
			}
			mpDev->CreateRenderTargetView(maBackBuffers[i], NULL, rtvHandle);

			// move to the next back buffer and repeat.
			rtvHandle.Offset(1, mRTVDescSize);
		}

		// Create CommandAllocator and Fence for each Frame Buffer.
		for (int i = 0; i < FRAME_BUFFER_COUNT; ++i) {
			// attempt to create a command allocator.
			if (FAILED(mpDev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&maCmdAllocators[i])))) {
				throw GFX_Exception("CreateCommandAllocator failed on init.");
			}

			// Create a fence to deal with GPU synchronization.
			if (FAILED(mpDev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&maFences[i])))) {
				throw GFX_Exception("CreateFence failed on init.");
			}

			maFenceValues[i] = 0;
		}

		// create a command list.
		if (FAILED(mpDev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, maCmdAllocators[mBufferIndex], NULL, IID_PPV_ARGS(&mpCmdList)))) {
			throw GFX_Exception("CreateCommandList failed on init.");
		}

		mFenceEvent = CreateEvent(NULL, false, false, NULL);
		if (!mFenceEvent) {
			throw GFX_Exception("Create Fence Event failed on init.");
		}
	}

	Graphics::~Graphics() {
		// ensure swap chain in windows mode.
		if (mpSwapChain) {
			mpSwapChain->SetFullscreenState(false, NULL);
			mpSwapChain->Release();
			mpSwapChain = nullptr;
		}

		CloseHandle(mFenceEvent);

		if (mpCmdList) {
			mpCmdList->Release();
			mpCmdList = nullptr;
		}

		for (int i = 0; i < FRAME_BUFFER_COUNT; ++i) {
			if (maFences[i]) {
				maFences[i]->Release();
				maFences[i] = nullptr;
			}

			if (maCmdAllocators[i]) {
				maCmdAllocators[i]->Release();
				maCmdAllocators[i] = nullptr;
			}

			if (maBackBuffers[i]) {
				maBackBuffers[i]->Release();
				maBackBuffers[i] = nullptr;
			}
		}
		if (mpRTVHeap) {
			mpRTVHeap->Release();
			mpRTVHeap = nullptr;
		}
		if (mpCmdQ) {
			mpCmdQ->Release();
			mpCmdQ = nullptr;
		}
		if (mpDev) {
			mpDev->Release();
			mpDev = nullptr;
		}
	}
	
	UINT Graphics::GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE ht) {
		return mpDev->GetDescriptorHandleIncrementSize(ht);
	}

	// Create and pass back a pointer to a new root signature matching the provided description.
	void Graphics::CreateRootSig(CD3DX12_ROOT_SIGNATURE_DESC rootDesc, ID3D12RootSignature*& rootSig) {
		ID3DBlob*	err;
		ID3DBlob*	sig;

		if (FAILED(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err))) {
			throw GFX_Exception((char *)err->GetBufferPointer());
		}
		if (FAILED(mpDev->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&rootSig)))) {
			throw GFX_Exception("Failed to create Root Signature.");
		}
		sig->Release();
	}

	// Create and pass back a pointer to a new Pipeline State Object matching the provided description.
	void Graphics::CreatePSO(D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc, ID3D12PipelineState*& PSO) {
		if (FAILED(mpDev->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&PSO)))) {
			throw GFX_Exception("Failed to CreateGraphicsPipeline.");
		}
	}

	// Create and return a pointer to a Descriptor Heap.
	void Graphics::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_DESC heapDesc, ID3D12DescriptorHeap*& heap) {
		if FAILED(mpDev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap))) {
			throw GFX_Exception("Failed to create descriptor heap.");
		}
	}

	// Create and upload to the gpu a shader resource and create a view for it.
	void Graphics::CreateSRV(D3D12_RESOURCE_DESC texDesc, ID3D12Resource*& tex, ID3D12Resource*& upload, D3D12_SUBRESOURCE_DATA texData,
						     D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc, D3D12_RESOURCE_STATES resourceType, ID3D12DescriptorHeap* heap, UINT64 offset) {
		// Create the resource heap on the gpu.
		if (FAILED(mpDev->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &texDesc, 
												  D3D12_RESOURCE_STATE_COPY_DEST, NULL, IID_PPV_ARGS(&tex)))) {
			throw GFX_Exception("Failed to create default heap on CreateSRV.");
		}

		// Create an upload heap.
		const auto buffSize = GetRequiredIntermediateSize(tex, 0, 1);
		if (FAILED(mpDev->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, 
												  &CD3DX12_RESOURCE_DESC::Buffer(buffSize), D3D12_RESOURCE_STATE_GENERIC_READ, 
												  NULL, IID_PPV_ARGS(&upload)))) {
			throw GFX_Exception("Failed to create upload heap on CreateSRV.");
		}

		// copy the data to the upload heap.
		const unsigned int subresourceCount = texDesc.DepthOrArraySize * texDesc.MipLevels;
		UpdateSubresources(mpCmdList, tex, upload, offset, 0, subresourceCount, &texData);
		mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(tex, D3D12_RESOURCE_STATE_COPY_DEST, resourceType));
	
		mpDev->CreateShaderResourceView(tex, &srvDesc, heap->GetCPUDescriptorHandleForHeapStart());
	}

	// Create a constant buffer view
	void Graphics::CreateCBV(D3D12_CONSTANT_BUFFER_VIEW_DESC desc, D3D12_CPU_DESCRIPTOR_HANDLE handle) {
		mpDev->CreateConstantBufferView(&desc, handle);
	}

	void Graphics::CreateBuffer(ID3D12Resource*& buffer, UINT size) {
		if (FAILED(mpDev->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
												  &CD3DX12_RESOURCE_DESC::Buffer(size),
												  D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer)))) {
			throw GFX_Exception("Failed to create buffer.");
		}
	}

	// set the back buffer as the render target for the provided command list.
	void Graphics::SetBackBufferRender(ID3D12GraphicsCommandList* cmdList, const float clearColor[4]) {
		// set back buffer to render target.
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(maBackBuffers[mBufferIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

		// get the handle to the back buffer and set as render target.
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mpRTVHeap->GetCPUDescriptorHandleForHeapStart(), mBufferIndex, mRTVDescSize);
		cmdList->OMSetRenderTargets(1, &rtvHandle, false, NULL);

		cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, NULL);
	}

	// set the back buffer as presenting for the provided command list.
	void Graphics::SetBackBufferPresent(ID3D12GraphicsCommandList* cmdList) {
		// switch back buffer to present state.
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(maBackBuffers[mBufferIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
	}

	void Graphics::ResetPipeline() {
		NextFrame();

		// reset command allocator and command list memory.
		if (FAILED(maCmdAllocators[mBufferIndex]->Reset())) {
			throw GFX_Exception("CommandAllocator Reset failed on UpdatePipeline.");
		}
		if (FAILED(mpCmdList->Reset(maCmdAllocators[mBufferIndex], NULL))) {
			throw GFX_Exception("CommandList Reset failed on UpdatePipeline.");
		}
	}

	void Graphics::LoadAssets() {
		// load the command list.
		ID3D12CommandList* lCmds[] = { mpCmdList };

		// execute
		mpCmdQ->ExecuteCommandLists(__crt_countof(lCmds), lCmds);

		// Add Signal command to set fence to the fence value that indicates the GPU is done with that buffer. maFenceValues[i] contains the frame count for that buffer.
		if (FAILED(mpCmdQ->Signal(maFences[mBufferIndex], maFenceValues[mBufferIndex]))) {
			throw GFX_Exception("CommandQueue Signal Fence failed on Render.");
		}

		// if the current value returned by the fence is less than the current fence value for this buffer, then we know the GPU is not done with the buffer, so wait.
		if (FAILED(maFences[mBufferIndex]->SetEventOnCompletion(maFenceValues[mBufferIndex], mFenceEvent))) {
			throw GFX_Exception("Failed to SetEventOnCompletion for fence in WaitForGPU.");
		}

		WaitForSingleObject(mFenceEvent, INFINITE);
		
		++maFenceValues[mBufferIndex];
	}

	void Graphics::NextFrame() {
		// Add Signal command to set fence to the fence value that indicates the GPU is done with that buffer. maFenceValues[i] contains the frame count for that buffer.
		if (FAILED(mpCmdQ->Signal(maFences[mBufferIndex], maFenceValues[mBufferIndex]))) {
			throw GFX_Exception("CommandQueue Signal Fence failed on Render.");
		}

		// set the buffer index to point to the current back buffer.
		mBufferIndex = mpSwapChain->GetCurrentBackBufferIndex();

		// if the current value returned by the fence is less than the current fence value for this buffer, then we know the GPU is not done with the buffer, so wait.
		if (maFences[mBufferIndex]->GetCompletedValue() < maFenceValues[mBufferIndex]) {
			if (FAILED(maFences[mBufferIndex]->SetEventOnCompletion(maFenceValues[mBufferIndex], mFenceEvent))) {
				throw GFX_Exception("Failed to SetEventOnCompletion for fence in NextFrame.");
			}

			WaitForSingleObject(mFenceEvent, INFINITE);
		}

		++maFenceValues[mBufferIndex];
	}

	void Graphics::Render() {
		// load the command list.
		ID3D12CommandList* lCmds[] = { mpCmdList };
		
		// execute
		mpCmdQ->ExecuteCommandLists(__crt_countof(lCmds), lCmds);

		// swap the back buffers.
		if (FAILED(mpSwapChain->Present(0, 0))) {
			throw GFX_Exception("SwapChain Present failed on Render.");
		}
	}

	// Compile the specified shader.
	void Graphics::CompileShader(LPCWSTR filename, D3D12_SHADER_BYTECODE& shaderBytecode, ShaderType st) {
		ID3DBlob*	shader;
		ID3DBlob*	err;
		LPCSTR		version;

		switch (st) {
			case PIXEL_SHADER:
				version = "ps_5_0";
				break;
			case VERTEX_SHADER:
				version = "vs_5_0";
				break;
		}

		if (FAILED(D3DCompileFromFile(filename, NULL, NULL, "main", version, D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &shader, &err))) {
			if (shader) shader->Release();
			if (err) {
				throw GFX_Exception((char *)err->GetBufferPointer());
			}
			else {
				throw GFX_Exception("Failed to compile Pixel Shader. No error returned from compiler.");
			}
		}
		shaderBytecode.BytecodeLength = shader->GetBufferSize();
		shaderBytecode.pShaderBytecode = shader->GetBufferPointer();
	}
}