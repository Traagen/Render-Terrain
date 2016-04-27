#pragma once

#include "Graphics.h"

using namespace graphics;

class Terrain {
public:
	Terrain(Graphics* GFX);
	~Terrain();

	void Draw(ID3D12GraphicsCommandList* cmdList);
private:
	ID3D12PipelineState*	mpPSO;
	ID3D12RootSignature*	mpRootSig;
};

