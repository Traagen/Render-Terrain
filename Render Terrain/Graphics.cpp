#include "Graphics.h"

namespace graphics {
	Graphics::Graphics(int height, int width, HWND win, bool fullscreen) {
		mDev = 0;
		mCmdQ = 0;
		mCmdList = 0;
		mSwapChain = 0;
		mRTVHeap = 0;
		mPipelineState = 0;
		mFenceEvent = 0;
//		mVertexBuffer = 0;
//		mIndexBuffer = 0;
		mRootSig = 0;
		for (int i = 0; i < FRAME_BUFFER_COUNT; ++i) {
			mCmdAllocator[i] = 0;
			mBBRenderTarget[i] = 0;
			mFence[i] = 0;
		}

		D3D12_COMMAND_QUEUE_DESC			cmdQDesc;
		D3D12_DESCRIPTOR_HEAP_DESC			rtvHeapDesc;
		IDXGIFactory4*						factory;
		IDXGIAdapter1*						adapter;
		IDXGISwapChain*						swapChain;
		DXGI_SWAP_CHAIN_DESC				swapChainDesc;
		ID3DBlob*							sig;
		ID3DBlob*							err;
		ID3DBlob*							VS;
		ID3DBlob*							PS;
		D3D12_SHADER_BYTECODE				PSBytecode = {};
		D3D12_SHADER_BYTECODE				VSBytecode = {};
		D3D12_INPUT_LAYOUT_DESC				inputLayoutDesc = {};
		D3D12_GRAPHICS_PIPELINE_STATE_DESC	psoDesc = {};
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
		if (FAILED(D3D12CreateDevice(adapter, FEATURE_LEVEL, IID_PPV_ARGS(&mDev)))) {
			throw GFX_Exception("D3D12CreateDevice failed on init.");
		}

		// attempt to create the command queue.
		ZeroMemory(&cmdQDesc, sizeof(cmdQDesc));
		cmdQDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		cmdQDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		cmdQDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		cmdQDesc.NodeMask = 0;

		if (FAILED(mDev->CreateCommandQueue(&cmdQDesc, IID_PPV_ARGS(&mCmdQ)))) {
			throw GFX_Exception("CreateCommandQueue failed on init.");
		}

		// attempt to create the swap chain.
		DXGI_SAMPLE_DESC sampleDesc;
		ZeroMemory(&sampleDesc, sizeof(sampleDesc));
		sampleDesc.Count = 1; // turns multi-sampling off. Not supported feature for my card.

		ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
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
		if (FAILED(factory->CreateSwapChain(mCmdQ, &swapChainDesc, &swapChain))) {
			throw GFX_Exception("CreateSwapChain failed on init.");
		}
		// upgrade swap chain to swapchain3 and store in mSwapChain.
		mSwapChain = static_cast<IDXGISwapChain3*>(swapChain);

		// clean up.
		swapChain = NULL;
		if (factory) {
			factory->Release();
			factory = NULL;
		}

		// get the index for the current back buffer.
		mBufferIndex = mSwapChain->GetCurrentBackBufferIndex();

		// create the render target views for the back buffers.
		ZeroMemory(&rtvHeapDesc, sizeof(rtvHeapDesc));
		rtvHeapDesc.NumDescriptors = FRAME_BUFFER_COUNT;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		if (FAILED(mDev->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&mRTVHeap)))) {
			throw GFX_Exception("CreateDescriptorHeap failed on init.");
		}

		// record the size of the RTV descriptor heap.
		mRTVDescSize = mDev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		// get a handle to the first descriptor in the descriptor heap. a handle is basically a pointer,
		// but we cannot literally use it like a c++ pointer.
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mRTVHeap->GetCPUDescriptorHandleForHeapStart());

		for (int i = 0; i < FRAME_BUFFER_COUNT; ++i) {
			// Get a pointer to the first back buffer from the swap chain.
			if (FAILED(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mBBRenderTarget[i])))) {
				throw GFX_Exception("Swap Chain GetBuffer failed on init.");
			}
			mDev->CreateRenderTargetView(mBBRenderTarget[i], NULL, rtvHandle);

			// move to the next back buffer and repeat.
			rtvHandle.Offset(1, mRTVDescSize);
		}

		// Create CommandAllocator and Fence for each Frame Buffer.
		for (int i = 0; i < FRAME_BUFFER_COUNT; ++i) {
			// attempt to create a command allocator.
			if (FAILED(mDev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCmdAllocator[i])))) {
				throw GFX_Exception("CreateCommandAllocator failed on init.");
			}

			// Create a fence to deal with GPU synchronization.
			if (FAILED(mDev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence[i])))) {
				throw GFX_Exception("CreateFence failed on init.");
			}

			mFenceValue[i] = 0;
		}

		// create a command list.
		if (FAILED(mDev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCmdAllocator[mBufferIndex], NULL, IID_PPV_ARGS(&mCmdList)))) {
			throw GFX_Exception("CreateCommandList failed on init.");
		}

		mFenceEvent = CreateEvent(NULL, false, false, NULL);
		if (!mFenceEvent) {
			throw GFX_Exception("Create Fence Event failed on init.");
		}

		// set up the Root Signature.
		CD3DX12_ROOT_SIGNATURE_DESC rootDesc;
		rootDesc.Init(0, NULL, 0, NULL, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
				
		if (FAILED(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err))) {
			throw GFX_Exception((char *)err->GetBufferPointer());
		}
		if (FAILED(mDev->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&mRootSig)))) {
			throw GFX_Exception("Failed to create Root Signature on init.");
		}
		sig->Release();

		// compile our shaders.
		// vertex shader.
		if (FAILED(D3DCompileFromFile(L"RenderTerrainVS.hlsl", NULL, NULL, "main", "vs_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &VS, &err))) {
			if (VS) VS->Release();
			if (err) {
				throw GFX_Exception((char *)err->GetBufferPointer());
			} else {
				throw GFX_Exception("Failed to compile Vertex Shader. No error returned from compiler.");
			}
		}
		VSBytecode.BytecodeLength = VS->GetBufferSize();
		VSBytecode.pShaderBytecode = VS->GetBufferPointer();

		// pixel shader.
		if (FAILED(D3DCompileFromFile(L"RenderTerrainPS.hlsl", NULL, NULL, "main", "ps_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &PS, &err))) {
			if (PS) PS->Release();
			if (err) {
				throw GFX_Exception((char *)err->GetBufferPointer());
			} else {
				throw GFX_Exception("Failed to compile Pixel Shader. No error returned from compiler.");
			}
		}
		PSBytecode.BytecodeLength = PS->GetBufferSize();
		PSBytecode.pShaderBytecode = PS->GetBufferPointer();

		// create input layout.
		D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
//			{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
		};
		inputLayoutDesc.NumElements = sizeof(inputLayout) / sizeof(D3D12_INPUT_ELEMENT_DESC);
		inputLayoutDesc.pInputElementDescs = inputLayout;
	
		// create the pipeline state object
		ZeroMemory(&psoDesc, sizeof(psoDesc));
		psoDesc.InputLayout = inputLayoutDesc;
		psoDesc.pRootSignature = mRootSig;
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

		if (FAILED(mDev->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPipelineState)))) {
			throw GFX_Exception("Failed to CreateGraphicsPipeline on init");
		}
/*
		// create the vertex buffer and fill it.
		// create 4 vertices for 2 triangles.
		Vertex vertices[] = {
			{ -0.5f,  0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f },
			{  0.5f, -0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f },
			{ -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
			{  0.5f,  0.5f, 0.5f, 0.0f, 1.0f, 1.0f, 1.0f }
		};

		int vbSize = sizeof(vertices);

		// create a default heap to allocate memory for the vertex buffer on the GPU.
		if (FAILED(mDev->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, 
												 &CD3DX12_RESOURCE_DESC::Buffer(vbSize), D3D12_RESOURCE_STATE_COPY_DEST, NULL, IID_PPV_ARGS(&mVertexBuffer)))) {
			throw GFX_Exception("CreateCommittedResource failed on creating default vertex buffer on init.");
		}
		mVertexBuffer->SetName(L"Vertex Buffer Resource Heap"); // give the resource a name so it is easily spotted in the graphics debugger.

		// create an upload heap that we can add the vertex data to to upload to the GPU.
		ID3D12Resource* vbUploadHeap;
		if (FAILED(mDev->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, 
												 &CD3DX12_RESOURCE_DESC::Buffer(vbSize), D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&vbUploadHeap)))) {
			throw GFX_Exception("CreateCommittedResource failed on creating upload vertex buffer on init.");
		}
		vbUploadHeap->SetName(L"Vertex Buffer Upload Resource Heap");

		// store the vertex buffer in the upload heap.
		D3D12_SUBRESOURCE_DATA vertexData = {};
		vertexData.pData = reinterpret_cast<BYTE*>(vertices);
		vertexData.RowPitch = vbSize;
		vertexData.SlicePitch = vbSize;

		// upload the data to the GPU and copy from the upload heap to the default heap.
		UpdateSubresources(mCmdList, mVertexBuffer, vbUploadHeap, 0, 0, 1, &vertexData);
		mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mVertexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

		// create an index buffer to define the 2 triangles of our quad.
		DWORD indices[] = {
			0, 1, 2, // first triangle
			0, 3, 1  // second triangle
		};

		int ibSize = sizeof(indices);

		// create default heap to hold index buffer
		if (FAILED(mDev->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
												 &CD3DX12_RESOURCE_DESC::Buffer(ibSize), D3D12_RESOURCE_STATE_COPY_DEST, NULL, IID_PPV_ARGS(&mIndexBuffer)))) {
			throw GFX_Exception("CreateCommittedResource failed on creating default index buffer on init.");
		}
		mIndexBuffer->SetName(L"Index Buffer Resource Heap");

		// create upload heap for index buffer.
		ID3D12Resource* ibUploadHeap;
		if (FAILED(mDev->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(ibSize), D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&ibUploadHeap)))) {
			throw GFX_Exception("CreateCommittedResource failed on creating upload index buffer on init.");
		}
		ibUploadHeap->SetName(L"Index Buffer Upload Resource Heap");

		// store index buffer in upload heap
		D3D12_SUBRESOURCE_DATA indexData = {};
		indexData.pData = reinterpret_cast<BYTE *>(indices);
		indexData.RowPitch = ibSize;
		indexData.SlicePitch = ibSize;

		// upload the data to the GPU and copy from the upload heap to the default heap.
		UpdateSubresources(mCmdList, mIndexBuffer, ibUploadHeap, 0, 0, 1, &indexData);
		mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mIndexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
	*/			
		mCmdList->Close();
		ID3D12CommandList* cmdLists[] = { mCmdList };
		mCmdQ->ExecuteCommandLists(1, cmdLists);

		++mFenceValue[mBufferIndex];
		if (FAILED(mCmdQ->Signal(mFence[mBufferIndex], mFenceValue[mBufferIndex]))) {
			throw GFX_Exception("CommandQueue Signal add failed on init.");
		}
/*
		// create a vertex buffer view.
		mVBView.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
		mVBView.StrideInBytes = sizeof(Vertex);
		mVBView.SizeInBytes = vbSize;

		// create an index buffer view.
		mIBView.BufferLocation = mIndexBuffer->GetGPUVirtualAddress();
		mIBView.Format = DXGI_FORMAT_R32_UINT;
		mIBView.SizeInBytes = ibSize;
*/
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
	}

	Graphics::~Graphics() {
		// ensure swap chain in windows mode.
		if (mSwapChain) {
			mSwapChain->SetFullscreenState(false, NULL);
			mSwapChain->Release();
			mSwapChain = NULL;
		}

		CloseHandle(mFenceEvent);

		if (mPipelineState) {
			mPipelineState->Release();
			mPipelineState = NULL;
		}

		if (mRootSig) {
			mRootSig->Release();
			mRootSig = NULL;
		}
/*
		if (mVertexBuffer) {
			mVertexBuffer->Release();
			mVertexBuffer = NULL;
		}

		if (mIndexBuffer) {
			mIndexBuffer->Release();
			mIndexBuffer = NULL;
		}
*/
		if (mCmdList) {
			mCmdList->Release();
			mCmdList = NULL;
		}

		for (int i = 0; i < FRAME_BUFFER_COUNT; ++i) {
			if (mFence[i]) {
				mFence[i]->Release();
				mFence[i] = NULL;
			}

			if (mCmdAllocator[i]) {
				mCmdAllocator[i]->Release();
				mCmdAllocator[i] = NULL;
			}

			if (mBBRenderTarget[i]) {
				mBBRenderTarget[i]->Release();
				mBBRenderTarget[i] = NULL;
			}
		}

		if (mRTVHeap) {
			mRTVHeap->Release();
			mRTVHeap = NULL;
		}
		if (mCmdQ) {
			mCmdQ->Release();
			mCmdQ = NULL;
		}
		if (mDev) {
			mDev->Release();
			mDev = NULL;
		}
	}

	void Graphics::UpdatePipeline() {
		WaitOnBackBuffer();

		// reset command allocator and command list memory.
		if (FAILED(mCmdAllocator[mBufferIndex]->Reset())) {
			throw GFX_Exception("CommandAllocator Reset failed on UpdatePipeline.");
		}
		if (FAILED(mCmdList->Reset(mCmdAllocator[mBufferIndex], mPipelineState))) {
			throw GFX_Exception("CommandList Reset failed on UpdatePipeline.");
		}

		// set back buffer to render target.
		mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBBRenderTarget[mBufferIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

		// get the handle to the back buffer and set as render target.
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mRTVHeap->GetCPUDescriptorHandleForHeapStart(), mBufferIndex, mRTVDescSize);
		mCmdList->OMSetRenderTargets(1, &rtvHandle, false, NULL);

		// set a clear color.
		const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
		mCmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, NULL);
		
		// draw
		mCmdList->SetGraphicsRootSignature(mRootSig);
		mCmdList->RSSetViewports(1, &mViewport);
		mCmdList->RSSetScissorRects(1, &mScissorRect); 
		mCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // describe how to read the vertex buffer.
//		mCmdList->IASetVertexBuffers(0, 1, &mVBView); // set the vertex buffer using the view.
//		mCmdList->IASetIndexBuffer(&mIBView); // set the index buffer using the view.
//		mCmdList->DrawIndexedInstanced(6, 1, 0, 0, 0); // draw 6 vertices for 2 triangles.
		mCmdList->DrawInstanced(3, 1, 0, 0);

		// switch back buffer to present state.
		mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBBRenderTarget[mBufferIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

		// close the command list.
		if (FAILED(mCmdList->Close())) {
			throw GFX_Exception("CommandList Close failed on UpdatePipeline.");
		}
	}

	void Graphics::WaitOnBackBuffer() {
		// set the buffer index to point to the current back buffer.
		mBufferIndex = mSwapChain->GetCurrentBackBufferIndex();

		// if the current value returned by the fence is less than the current fence value for this buffer, then we know the GPU is not done with the buffer, so wait.
		if (mFence[mBufferIndex]->GetCompletedValue() < mFenceValue[mBufferIndex]) {
			if (FAILED(mFence[mBufferIndex]->SetEventOnCompletion(mFenceValue[mBufferIndex], mFenceEvent))) {
				throw GFX_Exception("Failed to SetEventOnCompletion for fence in WaitOnBackBuffer.");
			}

			WaitForSingleObject(mFenceEvent, INFINITE);
		}

		++mFenceValue[mBufferIndex];
	}

	void Graphics::Render() {
		UpdatePipeline();

		// load the command list.
		ID3D12CommandList* lCmds[] = { mCmdList };

		// execute
		mCmdQ->ExecuteCommandLists(__crt_countof(lCmds), lCmds);

		// Add Signal command to set fence to the fence value that indicates the GPU is done with that buffer. mFenceValue[i] contains the frame count for that buffer.
		if (FAILED(mCmdQ->Signal(mFence[mBufferIndex], mFenceValue[mBufferIndex]))) {
			throw GFX_Exception("CommandQueue Signal Fence failed on Render.");
		}

		// swap the back buffers.
		if (FAILED(mSwapChain->Present(0, 0))) {
			throw GFX_Exception("SwapChain Present failed on Render.");
		}
	}
}