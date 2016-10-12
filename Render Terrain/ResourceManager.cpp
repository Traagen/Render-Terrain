/*
ResourceManager.cpp

Author:			Chris Serson
Last Edited:	October 12, 2016

Description:	Class for creating and managing a Direct3D 12 Resource Maanager.
*/

#include "ResourceManager.h"
#include "lodepng.h"
#include <string>

ResourceManager::ResourceManager(Device* d, unsigned int numRTVs, unsigned int numDSVs, unsigned int numCBVSRVUAVs,
	unsigned int numSamplers) :	m_pDev(d), m_numRTVs(numRTVs), m_numDSVs(numDSVs), m_numCBVSRVUAVs(numCBVSRVUAVs),
	m_numSamplers(numSamplers) {
	m_pheapRTV = nullptr;
	m_pheapDSV = nullptr;
	m_pheapCBVSRVUAV = nullptr;
	m_pheapSampler = nullptr;
	m_pCmdAllocator = nullptr;
	m_pCmdList = nullptr;
	m_pFence = nullptr;

	m_pDev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, m_pCmdAllocator);
	m_pDev->CreateGraphicsCommandList(D3D12_COMMAND_LIST_TYPE_DIRECT, m_pCmdAllocator, m_pCmdList);
	m_pCmdList->Close();

	m_valFence = 0;
	m_pDev->CreateFence(m_valFence, D3D12_FENCE_FLAG_NONE, m_pFence);
	m_hdlFenceEvent = CreateEvent(NULL, false, false, NULL);
	if (!m_hdlFenceEvent) {
		throw GFX_Exception("ResourceManager::ResourceManager: Create Fence Event failed on init.");
	}

	// initialize the descriptor heaps.
	D3D12_DESCRIPTOR_HEAP_DESC descHeap = {};
	descHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	if (m_numRTVs) {
		descHeap.NumDescriptors = m_numRTVs;
		descHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		m_pDev->CreateDescriptorHeap(&descHeap, m_pheapRTV);
		m_pheapRTV->SetName(L"RTV Heap");
		m_indexFirstFreeSlotRTV = 0;
	}
	else {
		m_indexFirstFreeSlotRTV = -1;
	}
	if (m_numDSVs) {
		descHeap.NumDescriptors = m_numDSVs;
		descHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		m_pDev->CreateDescriptorHeap(&descHeap, m_pheapDSV);
		m_pheapDSV->SetName(L"DSV Heap");
		m_indexFirstFreeSlotDSV = 0;
	}
	else {
		m_indexFirstFreeSlotDSV = -1;
	}

	descHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	if (m_numCBVSRVUAVs) {
		descHeap.NumDescriptors = m_numCBVSRVUAVs;
		descHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		m_pDev->CreateDescriptorHeap(&descHeap, m_pheapCBVSRVUAV);
		m_pheapCBVSRVUAV->SetName(L"CBV/SRV/UAV Heap");
		m_indexFirstFreeSlotCBVSRVUAV = 0;
	}
	else {
		m_indexFirstFreeSlotCBVSRVUAV = -1;
	}
	if (m_numSamplers) {
		descHeap.NumDescriptors = m_numSamplers;
		descHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
		m_pDev->CreateDescriptorHeap(&descHeap, m_pheapSampler);
		m_pheapSampler->SetName(L"Sampler Heap");
		m_indexFirstFreeSlotSampler = 0;
	}
	else {
		m_indexFirstFreeSlotSampler = -1;
	}

	m_sizeRTVHeapDesc = m_pDev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_sizeDSVHeapDesc = m_pDev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	m_sizeCBVSRVUAVHeapDesc = m_pDev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_sizeSamplerHeapDesc = m_pDev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

	// Create an upload buffer.
	m_pDev->CreateCommittedResource(m_pUpload, &CD3DX12_RESOURCE_DESC::Buffer(DEFAULT_UPLOAD_BUFFER_SIZE), &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);
	m_iUpload = 0;
}

ResourceManager::~ResourceManager() {	
	if (m_pUpload) {
		WaitForGPU();
		
		m_pUpload->Release();
		m_pUpload = nullptr;
	}

	m_pDev = nullptr;

	CloseHandle(m_hdlFenceEvent);

	if (m_pFence) {
		m_pFence->Release();
		m_pFence = nullptr;
	}

	if (m_pCmdAllocator) {
		m_pCmdAllocator->Release();
		m_pCmdAllocator = nullptr;
	}

	if (m_pCmdList) {
		m_pCmdList->Release();
		m_pCmdList = nullptr;
	}

	while (!m_listFileData.empty()) {
		unsigned char* tmp = m_listFileData.back();

		if (tmp) delete[] tmp;

		m_listFileData.pop_back();
	}

	while (!m_listResources.empty()) {
		ID3D12Resource* tex = m_listResources.back();

		if (tex) tex->Release();

		m_listResources.pop_back();
	}

	if (m_pheapSampler) {
		m_pheapSampler->Release();
		m_pheapSampler = nullptr;
	}

	if (m_pheapCBVSRVUAV) {
		m_pheapCBVSRVUAV->Release();
		m_pheapCBVSRVUAV = nullptr;
	}

	if (m_pheapDSV) {
		m_pheapDSV->Release();
		m_pheapDSV = nullptr;
	}

	if (m_pheapRTV) {
		m_pheapRTV->Release();
		m_pheapRTV = nullptr;
	}
}

void ResourceManager::AddRTV(ID3D12Resource* tex, D3D12_RENDER_TARGET_VIEW_DESC* desc,
	D3D12_CPU_DESCRIPTOR_HANDLE& handleCPU) {
	if (m_indexFirstFreeSlotRTV >= m_numRTVs || m_indexFirstFreeSlotRTV < 0) {
		throw GFX_Exception("Error adding to RTV Heap. No space remaining.");
	}

	handleCPU = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_pheapRTV->GetCPUDescriptorHandleForHeapStart(),
		m_indexFirstFreeSlotRTV, m_sizeRTVHeapDesc);
	m_pDev->CreateRTV(tex, desc, handleCPU);
	++m_indexFirstFreeSlotRTV;
}

void ResourceManager::AddDSV(ID3D12Resource* tex, D3D12_DEPTH_STENCIL_VIEW_DESC* desc,
	D3D12_CPU_DESCRIPTOR_HANDLE& handleCPU) {
	if (m_indexFirstFreeSlotDSV >= m_numDSVs || m_indexFirstFreeSlotDSV < 0) {
		throw GFX_Exception("Error adding to DSV Heap. No space remaining.");
	}

	handleCPU = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_pheapDSV->GetCPUDescriptorHandleForHeapStart(),
		m_indexFirstFreeSlotDSV, m_sizeDSVHeapDesc);
	m_pDev->CreateDSV(tex, desc, handleCPU);
	++m_indexFirstFreeSlotDSV;
}

void ResourceManager::AddCBV(D3D12_CONSTANT_BUFFER_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE& handleCPU,
	D3D12_GPU_DESCRIPTOR_HANDLE& handleGPU) {
	if (m_indexFirstFreeSlotCBVSRVUAV >= m_numCBVSRVUAVs || m_indexFirstFreeSlotCBVSRVUAV < 0) {
		throw GFX_Exception("Error adding CBV to CBV/SRV/UAV Heap. No space remaining.");
	}

	handleCPU = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_pheapCBVSRVUAV->GetCPUDescriptorHandleForHeapStart(),
		m_indexFirstFreeSlotCBVSRVUAV, m_sizeCBVSRVUAVHeapDesc);
	handleGPU = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_pheapCBVSRVUAV->GetGPUDescriptorHandleForHeapStart(),
		m_indexFirstFreeSlotCBVSRVUAV, m_sizeCBVSRVUAVHeapDesc);
	m_pDev->CreateCBV(desc, handleCPU);
	++m_indexFirstFreeSlotCBVSRVUAV;
}

void ResourceManager::AddSRV(ID3D12Resource* tex, D3D12_SHADER_RESOURCE_VIEW_DESC* desc,
	D3D12_CPU_DESCRIPTOR_HANDLE& handleCPU, D3D12_GPU_DESCRIPTOR_HANDLE& handleGPU) {
	if (m_indexFirstFreeSlotCBVSRVUAV >= m_numCBVSRVUAVs || m_indexFirstFreeSlotCBVSRVUAV < 0) {
		throw GFX_Exception("Error adding SRV to CBV/SRV/UAV Heap. No space remaining.");
	}

	handleCPU = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_pheapCBVSRVUAV->GetCPUDescriptorHandleForHeapStart(),
		m_indexFirstFreeSlotCBVSRVUAV, m_sizeCBVSRVUAVHeapDesc);
	handleGPU = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_pheapCBVSRVUAV->GetGPUDescriptorHandleForHeapStart(),
		m_indexFirstFreeSlotCBVSRVUAV, m_sizeCBVSRVUAVHeapDesc);
	m_pDev->CreateSRV(tex, desc, handleCPU);
	++m_indexFirstFreeSlotCBVSRVUAV;
}

void ResourceManager::AddSampler(D3D12_SAMPLER_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE& handleCPU) {
	if (m_indexFirstFreeSlotSampler >= m_numSamplers || m_indexFirstFreeSlotSampler < 0) {
		throw GFX_Exception("Error adding Sampler Heap. No space remaining.");
	}

	handleCPU = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_pheapSampler->GetCPUDescriptorHandleForHeapStart(), m_indexFirstFreeSlotSampler, m_sizeSamplerHeapDesc);
	m_pDev->CreateSampler(desc, handleCPU);
	++m_indexFirstFreeSlotSampler;
}

// takes a pointer to the existing resource, adds it to the list of resources, and returns the index to that resource.
unsigned int ResourceManager::AddExistingResource(ID3D12Resource* tex) {
	m_listResources.push_back(tex);
	return m_listResources.size() - 1;
}

// Create a new empty buffer. A pointer to the buffer is stored in buffer and index in list of resources is returned.
unsigned int ResourceManager::NewBuffer(ID3D12Resource*& buffer, D3D12_RESOURCE_DESC* descBuffer,
	D3D12_HEAP_PROPERTIES* props, D3D12_HEAP_FLAGS flags, D3D12_RESOURCE_STATES state, D3D12_CLEAR_VALUE* clear) {
	m_pDev->CreateCommittedResource(buffer, descBuffer, props, flags, state, clear);

	m_listResources.push_back(buffer);
	return m_listResources.size() - 1;
}

// Create a new empty buffer at index i in resource list, over-writing existing pointer.
// A pointer to the buffer is stored in buffer and index in list of resources is returned.
unsigned int ResourceManager::NewBufferAt(unsigned int i, ID3D12Resource*& buffer, D3D12_RESOURCE_DESC* descBuffer,
	D3D12_HEAP_PROPERTIES* props, D3D12_HEAP_FLAGS flags, D3D12_RESOURCE_STATES state, D3D12_CLEAR_VALUE* clear) {
	if (i < 0 || i >= m_listResources.size()) {
		std::string msg = "ResourceManager::NewBufferAt failed due to index " + std::to_string(i) + " out of bounds.";
		throw GFX_Exception(msg.c_str());
	}
	m_pDev->CreateCommittedResource(buffer, descBuffer, props, flags, state, clear);

	m_listResources[i] = buffer;

	return i;
}

// Upload to the buffer stored at index i.
void ResourceManager::UploadToBuffer(unsigned int i, unsigned int numSubResources, D3D12_SUBRESOURCE_DATA* data, D3D12_RESOURCE_STATES initialState) {
	if (i < 0 || i >= m_listResources.size()) {
		std::string msg = "ResourceManager::UploadToBuffer failed due to index " + std::to_string(i) + " out of bounds.";
		throw GFX_Exception(msg.c_str());
	}

	if (FAILED(m_pCmdList->Reset(m_pCmdAllocator, NULL))) {
		throw GFX_Exception("ResourceManager::UploadToBuffer: CommandList Reset failed.");
	}

	m_pCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_listResources[i],
		initialState, D3D12_RESOURCE_STATE_COPY_DEST));

	auto size = GetRequiredIntermediateSize(m_listResources[i], 0, numSubResources);
	size = pow(2, ceilf(log(size) / log(2))); // round size up to the next power of 2 to ensure good alignment in upload buffer.

	if (size > DEFAULT_UPLOAD_BUFFER_SIZE) {
		// then we're going to have to create a new temporary buffer.
		ID3D12Resource* tmpUpload;
		m_pDev->CreateCommittedResource(tmpUpload, &CD3DX12_RESOURCE_DESC::Buffer(size), &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);

		UpdateSubresources(m_pCmdList, m_listResources[i], tmpUpload, 0, 0, numSubResources, data);

		// set resource barriers to inform GPU that data is ready for use.
		m_pCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_listResources[i],
			initialState, D3D12_RESOURCE_STATE_COPY_DEST));

		// close the command list.
		if (FAILED(m_pCmdList->Close())) {
			throw GFX_Exception("ResourceManager::UploadToBuffer: CommandList Close failed.");
		}

		// load the command list.
		ID3D12CommandList* lCmds[] = { m_pCmdList };
		m_pDev->ExecuteCommandLists(lCmds, __crt_countof(lCmds));

		// add fence signal.
		++m_valFence;
		m_pDev->SetFence(m_pFence, m_valFence);

		WaitForGPU();
		tmpUpload->Release();
	} else {
		if (size > DEFAULT_UPLOAD_BUFFER_SIZE - m_iUpload) {
			// then we need to wait for the GPU to finish with whatever it is currently uploading.
			// check to see if it is already done.
			if (m_pFence->GetCompletedValue() < m_valFence) {
				// then we're not done, so wait.
				WaitForGPU();
				
			}
			m_iUpload = 0;
		}

		UpdateSubresources(m_pCmdList, m_listResources[i], m_pUpload, m_iUpload, 0, numSubResources, data);
		m_iUpload += size;

		// set resource barriers to inform GPU that data is ready for use.
		m_pCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_listResources[i],
			D3D12_RESOURCE_STATE_COPY_DEST, initialState));

		// close the command list.
		if (FAILED(m_pCmdList->Close())) {
			throw GFX_Exception("ResourceManager::UploadToBuffer: CommandList Close failed.");
		}

		// load the command list.
		ID3D12CommandList* lCmds[] = { m_pCmdList };
		m_pDev->ExecuteCommandLists(lCmds, __crt_countof(lCmds));
	}
}

// Wait for GPU to signal it has completed with the previous iteration of this frame.
void ResourceManager::WaitForGPU() {
	// add fence signal.
	++m_valFence;
	m_pDev->SetFence(m_pFence, m_valFence);
	if (FAILED(m_pFence->SetEventOnCompletion(m_valFence, m_hdlFenceEvent))) {
		throw GFX_Exception("ResourceManager::WaitForGPU failed to SetEventOnCompletion.");
	}

	WaitForSingleObject(m_hdlFenceEvent, INFINITE);
}

// return a pointer to the resource at the provided index
ID3D12Resource* ResourceManager::GetResource(unsigned int index) {
	if (index < 0 || index >= m_listResources.size()) {
		std::string msg = "ResourceManager::GetResource failed due to index " + std::to_string(index) + " out of bounds.";
		throw GFX_Exception(msg.c_str());
	}

	return m_listResources[index];
}

// load a file and return the index of the data loaded in m_listFileData.
unsigned int ResourceManager::LoadFile(const char* fn, unsigned int& h, unsigned int& w) {
	unsigned char* data;
	// load the black and white heightmap png file. Data is RGBA unsigned char.
	unsigned error = lodepng_decode32_file(&data, &w, &h, fn);
	if (error) {
		std::string msg = "ResourceManager::LoadFile: Error loading file " + std::string(fn);
		throw GFX_Exception(msg.c_str());
	}

	m_listFileData.push_back(data);
	return m_listFileData.size() - 1;
}

// get the data saved at index i in m_listFileData.
unsigned char* ResourceManager::GetFileData(unsigned int i) {
	if (i < 0 || i >= m_listFileData.size()) {
		std::string msg = "ResourceManager::GetFileData: index out of bounds: " + std::to_string(i);
		throw GFX_Exception(msg.c_str());
	}

	return m_listFileData[i];
}

// tell the ResourceManager that you are done with the data saved at index i in m_listFileData.
// it will delete that data. Leaves a NULL pointer in the list so as not to mess with other indices.
void ResourceManager::UnloadFileData(unsigned int i) {
	if (i < 0 || i >= m_listFileData.size()) {
		std::string msg = "ResourceManager::UnloadFileData: index out of bounds: " + std::to_string(i);
		throw GFX_Exception(msg.c_str());
	}

	delete[] m_listFileData[i];
}