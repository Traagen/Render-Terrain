#include "Graphics.h"

using namespace graphics;

static const UINT FACTORY_DEBUG = DXGI_CREATE_FACTORY_DEBUG; // set to 0 if not debugging, DXGI_CREATE_FACTORY_DEBUG if debugging.
static const DXGI_FORMAT DESIRED_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
static const int FRAME_BUFFER_COUNT = 2;

Graphics::Graphics(int height, int width, HWND win, bool fullscreen) {
	mDev = 0;
	mCmdQ = 0;
	mCmdAllocator = 0;
	mCmdList = 0;
	mSwapChain = 0;
	mRTVHeap = 0;
	mBBRenderTarget[0] = 0;
	mBBRenderTarget[1] = 0;
	mPipelineState = 0;
	mFence = 0;
	mFenceEvent = 0;

	unsigned int					rtvDescSize;
	D3D_FEATURE_LEVEL				fLvl;
	D3D12_COMMAND_QUEUE_DESC		cmdQDesc;
	D3D12_DESCRIPTOR_HEAP_DESC		rtvHeapDesc;
	D3D12_CPU_DESCRIPTOR_HANDLE		rtvHandle;
	IDXGIFactory4*					factory;
	IDXGISwapChain*					swapChain;
	DXGI_SWAP_CHAIN_DESC			swapChainDesc;

	// attempt to create the device.
	fLvl = D3D_FEATURE_LEVEL_11_0; // my current card doesn't support higher than this.
	if (FAILED(D3D12CreateDevice(NULL, fLvl, IID_PPV_ARGS(&mDev)))) {
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

	// Create a DirectX graphics interface factory.
	if (FAILED(CreateDXGIFactory2(FACTORY_DEBUG, IID_PPV_ARGS(&factory))))
	{
		throw GFX_Exception("CreateDXGIFactory2 failed on init.");
	}

	// attempt to create the swap chain.
	ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
	swapChainDesc.BufferCount = FRAME_BUFFER_COUNT; // double buffering.
	swapChainDesc.BufferDesc.Height = height;
	swapChainDesc.BufferDesc.Width = width;
	swapChainDesc.BufferDesc.Format = DESIRED_FORMAT;
	swapChainDesc.OutputWindow = win;
	swapChainDesc.Windowed = !fullscreen;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1; // turns multi-sampling off. Not supported feature for my card.

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

	// create the render target views for the back buffers.
	ZeroMemory(&rtvHeapDesc, sizeof(rtvHeapDesc));
	rtvHeapDesc.NumDescriptors = FRAME_BUFFER_COUNT;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	if (FAILED(mDev->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&mRTVHeap)))) {
		throw GFX_Exception("CreateDescriptorHeap failed on init.");
	}

	// Get a handle to the starting memory location in the render target view heap to identify where the render target views will be located for the 2 back buffers.
	rtvHandle = mRTVHeap->GetCPUDescriptorHandleForHeapStart();
	// Get the size of the memory location for the render target view descriptors.
	rtvDescSize = mDev->GetDescriptorHandleIncrementSize(rtvHeapDesc.Type);
	for (int i = 0; i < FRAME_BUFFER_COUNT; ++i) {
		// Get a pointer to the first back buffer from the swap chain.
		if (FAILED(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mBBRenderTarget[i])))) {
			throw GFX_Exception("Swap Chain GetBuffer failed on init.");
		}
		mDev->CreateRenderTargetView(mBBRenderTarget[i], NULL, rtvHandle);

		// move to the next back buffer and repeat.
		rtvHandle.ptr += rtvDescSize;
	}
	// get the index for the current back buffer.
	mBufferIndex = mSwapChain->GetCurrentBackBufferIndex();

	// attempt to create a command allocator.
	if (FAILED(mDev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCmdAllocator)))) {
		throw GFX_Exception("CreateCommandAllocator failed on init.");
	}

	// create a command list.
	if (FAILED(mDev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCmdAllocator, NULL, IID_PPV_ARGS(&mCmdList)))) {
		throw GFX_Exception("CreateCommandList failed on init.");
	}
	// the command list needs to be closed when initialized as it is created in a recording state.
	if (FAILED(mCmdList->Close())) {
		throw GFX_Exception("Command List Close failed on init.");
	}

	// Create a fence to deal with GPU synchronization.
	if (FAILED(mDev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)))) {
		throw GFX_Exception("CreateFence failed on init.");
	}
	mFenceEvent = CreateEventEx(NULL, false, false, EVENT_ALL_ACCESS);
	if (!mFenceEvent) {
		throw GFX_Exception("Create Fence Event failed on init.");
	}
	mFenceValue = 1;
}

Graphics::~Graphics() {
	// ensure swap chain in windows mode.
	if (mSwapChain) {
		mSwapChain->SetFullscreenState(false, NULL);
		mSwapChain->Release();
		mSwapChain = NULL;
	}

	CloseHandle(mFenceEvent);

	if (mFence) {
		mFence->Release();
		mFence = NULL;
	}
	if (mPipelineState) {
		mPipelineState->Release();
		mPipelineState = NULL;
	}
	if (mCmdList) {
		mCmdList->Release();
		mCmdList = NULL;
	}
	if (mCmdAllocator) {
		mCmdAllocator->Release();
		mCmdAllocator = NULL;
	}

	for (int i = 0; i < FRAME_BUFFER_COUNT; ++i) {
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

void Graphics::Render() {
	D3D12_RESOURCE_BARRIER			barrier;
	D3D12_CPU_DESCRIPTOR_HANDLE		rtvHandle;
	ID3D12CommandList*				lCmds[1];
	unsigned long long				fenceToWaitFor;
	unsigned int					rtvDescSize;
	float							color[4];

	// reset command allocator and command list memory.
	if (FAILED(mCmdAllocator->Reset())) {
		throw GFX_Exception("CommandAllocator Reset failed on Render.");
	}
	if (FAILED(mCmdList->Reset(mCmdAllocator, mPipelineState))) {
		throw GFX_Exception("CommandList Reset failed on Render.");
	}

	// record commands into command list.
	// set resource barrier.
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = mBBRenderTarget[mBufferIndex];
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	mCmdList->ResourceBarrier(1, &barrier);

	// get the handle to the back buffer and set as render target.
	rtvHandle = mRTVHeap->GetCPUDescriptorHandleForHeapStart();
	rtvDescSize = mDev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	if (mBufferIndex == 1) rtvHandle.ptr += rtvDescSize;
	mCmdList->OMSetRenderTargets(1, &rtvHandle, false, NULL);

	// set a clear color.
	color[0] = 0.5f;
	color[1] = 0.0f;
	color[2] = 0.5f;
	color[3] = 1.0f;
	mCmdList->ClearRenderTargetView(rtvHandle, color, 0, NULL);

	// switch back buffer to present state.
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	mCmdList->ResourceBarrier(1, &barrier);

	// close the command list.
	if (FAILED(mCmdList->Close())) {
		throw GFX_Exception("CommandList Close failed on Render.");
	}

	// load the command list.
	lCmds[0] = mCmdList;

	// execute
	mCmdQ->ExecuteCommandLists(1, lCmds);

	// swap the back buffers.
	if (FAILED(mSwapChain->Present(0, 0))) {
		throw GFX_Exception("SwapChain Present failed on Render.");
	}

	// for simplicity, wait for GPU to complete rendering.
	fenceToWaitFor = mFenceValue;
	if (FAILED(mCmdQ->Signal(mFence, fenceToWaitFor))) {
		throw GFX_Exception("CommandQueue Signal Fence failed on Render.");
	}
	mFenceValue++;

	if (mFence->GetCompletedValue() < fenceToWaitFor) {
		if (FAILED(mFence->SetEventOnCompletion(fenceToWaitFor, mFenceEvent))) {
			throw GFX_Exception("Fence SetEventOnCompletion failed on Render.");
		}
		WaitForSingleObject(mFenceEvent, INFINITE);
	}

	// swap buffer index.
	mBufferIndex = (mBufferIndex ? 0 : 1);
}