/*
ResourceManager.h

Author:			Chris Serson
Last Edited:	October 12, 2016

Description:	Class for creating and managing a Direct3D 12 Resource Maanager.

Usage:			- Proper shutdown is handled by the destructor.
				- Handles loading file data (LoadFile(), GetFileData())
				- Manages all resource heaps.
				- Manages all ID3D12Resources.

Future Work:	- Add and remove resources dynamically.
				- Add support for loading different file types. Currently only supports PNG.
				- Add support for reserved and placed resources.
*/
#pragma once

#include "Graphics.h"
#include <vector>

using namespace graphics;

static const unsigned long long DEFAULT_UPLOAD_BUFFER_SIZE = 100000000;

class ResourceManager {
public:
	ResourceManager(Device* d, unsigned int numRTVs, unsigned int numDSVs, unsigned int numCBVSRVUAVs, 
		unsigned int numSamplers);
	~ResourceManager();

	ID3D12DescriptorHeap* GetCBVSRVUAVHeap() { return m_pheapCBVSRVUAV; }

	void AddRTV(ID3D12Resource* tex, D3D12_RENDER_TARGET_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE& handleCPU);
	void AddDSV(ID3D12Resource* tex, D3D12_DEPTH_STENCIL_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE& handleCPU);
	void AddCBV(D3D12_CONSTANT_BUFFER_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE& handleCPU,
		D3D12_GPU_DESCRIPTOR_HANDLE& handleGPU);
	void AddSRV(ID3D12Resource* tex, D3D12_SHADER_RESOURCE_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE& handleCPU,
		D3D12_GPU_DESCRIPTOR_HANDLE& handleGPU);
	void AddSampler(D3D12_SAMPLER_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE& handleCPU);

	// takes a pointer to the existing resource, adds it to the list of resources, and returns the index to that resource.
	unsigned int AddExistingResource(ID3D12Resource* tex);
	// Create a new empty buffer. A pointer to the buffer is stored in buffer and index in list of resources is returned.
	unsigned int NewBuffer(ID3D12Resource*& buffer, D3D12_RESOURCE_DESC* descBuffer, D3D12_HEAP_PROPERTIES* props,
		D3D12_HEAP_FLAGS flags, D3D12_RESOURCE_STATES state, D3D12_CLEAR_VALUE* clear);
	// Create a new empty buffer at index i in resource list, over-writing existing pointer.
	// A pointer to the buffer is stored in buffer and index in list of resources is returned.
	unsigned int NewBufferAt(unsigned int i, ID3D12Resource*& buffer, D3D12_RESOURCE_DESC* descBuffer,
		D3D12_HEAP_PROPERTIES* props, D3D12_HEAP_FLAGS flags, D3D12_RESOURCE_STATES state, D3D12_CLEAR_VALUE* clear);
	// Upload to the buffer stored at index i.
	void UploadToBuffer(unsigned int i, unsigned int numSubResources, D3D12_SUBRESOURCE_DATA* data, D3D12_RESOURCE_STATES initialState);

	// return a pointer to the resource at the provided index
	ID3D12Resource* GetResource(unsigned int index);

	// load a file and return the index of the data loaded in m_listFileData.
	unsigned int LoadFile(const char* fn, unsigned int& h, unsigned int& w);
	// get the data saved at index i in m_listFileData.
	unsigned char* GetFileData(unsigned int i);
	// tell the ResourceManager that you are done with the data saved at index i in m_listFileData.
	// it will delete that data. Leaves a NULL pointer in the list so as not to mess with other indices.
	void UnloadFileData(unsigned int i);
	// Wait for GPU to signal it has completed with the previous iteration of this frame.
	void WaitForGPU();

private:
	Device*							m_pDev;
	ID3D12CommandAllocator*			m_pCmdAllocator;
	ID3D12GraphicsCommandList*		m_pCmdList;
	ID3D12Fence*					m_pFence;
	HANDLE							m_hdlFenceEvent;
	ID3D12DescriptorHeap*			m_pheapRTV;						// Render Target View Heap.
	ID3D12DescriptorHeap*			m_pheapDSV;						// Depth Stencil View Heap.
	ID3D12DescriptorHeap*			m_pheapCBVSRVUAV;				// Constant Buffer View, Shader Resource View, and Unordered Access View heap.
	ID3D12DescriptorHeap*			m_pheapSampler;					// Sampler heap.
	std::vector<ID3D12Resource*>	m_listResources;
	std::vector<unsigned char*>		m_listFileData;					// Any data loaded from files.
	ID3D12Resource*					m_pUpload;
	unsigned long long				m_iUpload;						// index into upload buffer where free space starts.
	unsigned long long				m_valFence;						// Value to check fence against to confirm GPU is done.
	unsigned int					m_numRTVs;
	unsigned int					m_numDSVs;
	unsigned int					m_numCBVSRVUAVs;
	unsigned int					m_numSamplers;
	unsigned int					m_indexFirstFreeSlotRTV;		// index into RTV heap of the first unused slot.
	unsigned int					m_indexFirstFreeSlotDSV;
	unsigned int					m_indexFirstFreeSlotCBVSRVUAV;
	unsigned int					m_indexFirstFreeSlotSampler;
	unsigned int					m_sizeRTVHeapDesc;				// Heap descriptor size for render target view heaps.
	unsigned int					m_sizeDSVHeapDesc;				// Heap descriptor size for depth stencil view heaps.
	unsigned int					m_sizeCBVSRVUAVHeapDesc;		// Heap descriptor size for CBV, SRV, and UAV heaps.
	unsigned int					m_sizeSamplerHeapDesc;			// Heap descriptor size for Sampler Heaps.
};
