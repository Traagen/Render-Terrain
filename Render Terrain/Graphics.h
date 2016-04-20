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
	static const int FRAME_BUFFER_COUNT = 3; // triple buffering.
	static const D3D_FEATURE_LEVEL	FEATURE_LEVEL = D3D_FEATURE_LEVEL_11_0; // minimum feature level necessary for DirectX 12 compatibility.

	struct Vertex {
		Vertex(float x, float y, float z, float r, float g, float b, float a) : pos(x, y, z), color(r, g, b, a) {}
		XMFLOAT3 pos;
		XMFLOAT4 color;
	};
	
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
		void UpdatePipeline();	// Create the list of commands to run each frame. Currently just sets the clear color.
		void WaitOnBackBuffer(); // Waits for and confirms that the GPU is done running any commands on the current back buffer.

		ID3D12Device*				mDev;
		ID3D12CommandQueue*			mCmdQ;
		ID3D12CommandAllocator*		mCmdAllocator[FRAME_BUFFER_COUNT];
		ID3D12GraphicsCommandList*	mCmdList;
		IDXGISwapChain3*			mSwapChain;
		ID3D12DescriptorHeap*		mRTVHeap; // Render Target View Heap
		ID3D12Resource*				mBBRenderTarget[FRAME_BUFFER_COUNT];
//		ID3D12Resource*				mVertexBuffer;
//		ID3D12Resource*				mIndexBuffer;
		ID3D12PipelineState*		mPipelineState;
		ID3D12RootSignature*		mRootSig;
		ID3D12Fence*				mFence[FRAME_BUFFER_COUNT];
		HANDLE						mFenceEvent;
		D3D12_VIEWPORT				mViewport;
		D3D12_RECT					mScissorRect;
//		D3D12_VERTEX_BUFFER_VIEW	mVBView;
//		D3D12_INDEX_BUFFER_VIEW		mIBView;
		unsigned long long			mFenceValue[FRAME_BUFFER_COUNT];
		unsigned int				mBufferIndex;
		unsigned int				mRTVDescSize; // Descriptor sizes may vary from device to device, so keep the size around so we can increment an offset when necessary.
	};
};
