#pragma once

// Linkages
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include <d3d12.h>
#include <dxgi1_5.h>

namespace graphics {

	static const bool VSYNC_ENABLED = true;
	static const float SCREEN_DEPTH = 1000.0f;
	static const float SCREEN_NEAR = 0.1f;

	class Graphics
	{
	public:
		Graphics();
		~Graphics();

		bool Init(int, int, HWND, bool);
		bool Render();

	private:
		ID3D12Device*				mDev;
		ID3D12CommandQueue*			mCmdQ;
		ID3D12CommandAllocator*		mCmdAllocator;
		ID3D12GraphicsCommandList*	mCmdList;
		IDXGISwapChain3*			mSwapChain;
		ID3D12DescriptorHeap*		mRTVHeap; // Render Target View Heap
		ID3D12Resource*				mBBRenderTarget[2];
		ID3D12PipelineState*		mPipelineState;
		ID3D12Fence*				mFence;
		HANDLE						mFenceEvent;
		unsigned long long			mFenceValue;
		unsigned int				mBufferIndex;
	};
};
