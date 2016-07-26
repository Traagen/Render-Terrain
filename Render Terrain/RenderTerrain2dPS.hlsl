Texture2D<float> heightmap : register(t0);
SamplerState hmsampler : register(s0);

struct VS_OUTPUT {
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD;
};

float4 main(VS_OUTPUT input) : SV_TARGET {
	float height = heightmap.Sample(hmsampler, input.tex);
	return float4(height, height, height, 1);
}