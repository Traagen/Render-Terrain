#pragma once

#include "Graphics.h"
#include <vector>

using namespace graphics;

class Terrain {
public:
	Terrain(Graphics* GFX);
	~Terrain();

	void Draw(ID3D12GraphicsCommandList* cmdList);
private:
	// load the specified file containing the heightmap data.
	void LoadHeightMap(const char* filename);

	ID3D12PipelineState*		mpPSO;
	ID3D12RootSignature*		mpRootSig;
	ID3D12Resource*				mpHeightmap;
	ID3D12DescriptorHeap*		mpSRVHeap; // Shader Resource View Heap
	std::vector<unsigned char>	mvImage; //the raw pixels
	unsigned int				mWidth;
	unsigned int				mHeight;
};

