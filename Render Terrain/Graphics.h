#pragma once

// Linkages
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include <d3d12.h>
#include <dxgi1_5.h>
#include <stdexcept>

namespace graphics {
	static const float SCREEN_DEPTH = 1000.0f;
	static const float SCREEN_NEAR = 0.1f;

	class GFX_Exception : public std::runtime_error {
	public:
		GFX_Exception(const char *msg) : std::runtime_error(msg) {}
	};
	
	class Graphics	{
	public:
		Graphics(int, int, HWND, bool);
		~Graphics();

		void Render();

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
