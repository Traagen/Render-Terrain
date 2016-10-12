Texture2D<float4> heightmap : register(t0);
SamplerState hmsampler : register(s0);

struct VS_OUTPUT {
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD;
};

float4 main(VS_OUTPUT input) : SV_TARGET {
	return heightmap.Sample(hmsampler, input.tex);
}