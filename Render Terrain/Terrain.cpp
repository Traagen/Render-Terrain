#include "lodepng.h"
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
	D3D12_SHADER_RESOURCE_VIEW_DESC		srvDesc = {};
	D3D12_DESCRIPTOR_HEAP_DESC			srvHeapDesc = {};
	D3D12_RESOURCE_DESC					texDesc = {};
	D3D12_SUBRESOURCE_DATA				texData = {};
	CD3DX12_ROOT_SIGNATURE_DESC			rootDesc;
	CD3DX12_ROOT_PARAMETER				paramsRoot[1];
	CD3DX12_DESCRIPTOR_RANGE			rangeRoot;
	CD3DX12_STATIC_SAMPLER_DESC			descSamplers[1];

	mpRootSig = 0;
	mpPSO = 0;
	mpHeightmap = 0;
	mpSRVHeap = 0;
	
	sampleDesc.Count = 1; // turns multi-sampling off. Not supported feature for my card.

	// create the SRV heap
	srvHeapDesc.NumDescriptors = 1;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	GFX->CreateDescriptorHeap(srvHeapDesc, mpSRVHeap);
						  
	// set up the Root Signature.
	// create a descriptor table with 1 entry for the descriptor heap containing our SRV to the heightmap.
	rangeRoot.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
	paramsRoot[0].InitAsDescriptorTable(1, &rangeRoot);

	// create our sampler.
	descSamplers[0].Init(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
	rootDesc.Init(1, paramsRoot, 1, descSamplers, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
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

	LoadHeightMap("heightmap.png");
	// create the texture.
	texDesc.MipLevels = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.Width = mWidth;
	texDesc.Height = mHeight;
	texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	texDesc.DepthOrArraySize = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	// create shader resource view for the heightmap.
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = texDesc.MipLevels;

	texData.pData = mvImage.data();
	texData.RowPitch = mWidth * 4;
	texData.SlicePitch = mHeight * mWidth * 4;

	GFX->CreateSRV(texDesc, mpHeightmap, mpUpload, texData, srvDesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, mpSRVHeap);
}

Terrain::~Terrain() {
	// The order resources are released appears to matter. I haven't tested all possible orders, but at least releasing the heap
	// and resources after the pso and rootsig was causing my GPU to hang on shutdown. Using the current order resolved that issue.
	if (mpSRVHeap) {
		mpSRVHeap->Release();
		mpSRVHeap = 0;
	}

	if (mpUpload) {
		mpUpload->Release();
		mpUpload = 0;
	}

	if (mpHeightmap) {
		mpHeightmap->Release();
		mpHeightmap = 0;
	}

	if (mpPSO) {
		mpPSO->Release();
		mpPSO = 0;
	}
	
	if (mpRootSig) {
		mpRootSig->Release();
		mpRootSig = NULL;
	}
}

void Terrain::Draw(ID3D12GraphicsCommandList* cmdList) {
	cmdList->SetPipelineState(mpPSO);
	cmdList->SetGraphicsRootSignature(mpRootSig);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // describe how to read the vertex buffer.
	ID3D12DescriptorHeap* heaps[] = { mpSRVHeap };
	cmdList->SetDescriptorHeaps(1, heaps);
	cmdList->SetGraphicsRootDescriptorTable(0, mpSRVHeap->GetGPUDescriptorHandleForHeapStart());
	cmdList->DrawInstanced(3, 1, 0, 0);
}

// load the specified file containing the heightmap data.
void Terrain::LoadHeightMap(const char* filename) {
	//decode
	unsigned error = lodepng::decode(mvImage, mWidth, mHeight, filename);

	if (error) throw GFX_Exception("Error loading heightmap texture.");
}
