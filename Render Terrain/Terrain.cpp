#include "Terrain.h"

Terrain::Terrain(Graphics* GFX) {
	ID3DBlob*							VS;
	ID3DBlob*							PS;
	ID3DBlob*							err;
	D3D12_SHADER_BYTECODE				PSBytecode = {};
	D3D12_SHADER_BYTECODE				VSBytecode = {};
	D3D12_INPUT_LAYOUT_DESC				inputLayoutDesc = {};
	D3D12_GRAPHICS_PIPELINE_STATE_DESC	psoDesc = {};
	DXGI_SAMPLE_DESC					sampleDesc = {};
	mpRootSig = 0;
	mpPSO = 0;
	
	sampleDesc.Count = 1; // turns multi-sampling off. Not supported feature for my card.

	// set up the Root Signature.
	CD3DX12_ROOT_SIGNATURE_DESC rootDesc;
	rootDesc.Init(0, NULL, 0, NULL, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	GFX->CreateRootSig(rootDesc, mpRootSig);

	// compile our shaders.
	// vertex shader.
	if (FAILED(D3DCompileFromFile(L"RenderTerrainVS.hlsl", NULL, NULL, "main", "vs_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &VS, &err))) {
		if (VS) VS->Release();
		if (err) {
			throw GFX_Exception((char *)err->GetBufferPointer());
		}
		else {
			throw GFX_Exception("Failed to compile terrain Vertex Shader. No error returned from compiler.");
		}
	}
	VSBytecode.BytecodeLength = VS->GetBufferSize();
	VSBytecode.pShaderBytecode = VS->GetBufferPointer();

	// pixel shader.
	if (FAILED(D3DCompileFromFile(L"RenderTerrainPS.hlsl", NULL, NULL, "main", "ps_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &PS, &err))) {
		if (PS) PS->Release();
		if (err) {
			throw GFX_Exception((char *)err->GetBufferPointer());
		}
		else {
			throw GFX_Exception("Failed to compile Pixel Shader. No error returned from compiler.");
		}
	}
	PSBytecode.BytecodeLength = PS->GetBufferSize();
	PSBytecode.pShaderBytecode = PS->GetBufferPointer();

	// create input layout.
	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
	inputLayoutDesc.NumElements = sizeof(inputLayout) / sizeof(D3D12_INPUT_ELEMENT_DESC);
	inputLayoutDesc.pInputElementDescs = inputLayout;

	// create the pipeline state object
	psoDesc.InputLayout = inputLayoutDesc;
	psoDesc.pRootSignature = mpRootSig;
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

	GFX->CreatePSO(psoDesc, mpPSO);
}

Terrain::~Terrain() {
	if (mpRootSig) {
		mpRootSig->Release();
		mpRootSig = NULL;
	}

	if (mpPSO) {
		mpPSO->Release();
		mpPSO = 0;
	}
}

void Terrain::Draw(ID3D12GraphicsCommandList* cmdList) {
	cmdList->SetPipelineState(mpPSO);
	cmdList->SetGraphicsRootSignature(mpRootSig);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // describe how to read the vertex buffer.
	cmdList->DrawInstanced(3, 1, 0, 0);
}
