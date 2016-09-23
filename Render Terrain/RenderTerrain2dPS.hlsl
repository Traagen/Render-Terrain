Texture2D<float> heightmap : register(t0);
SamplerState hmsampler : register(s0);

struct VS_OUTPUT {
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD;
};

float4 main(VS_OUTPUT input) : SV_TARGET {
	return float4(heightmap.Sample(hmsampler, input.tex), 0, 0, 1);
}