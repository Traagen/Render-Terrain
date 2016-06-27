Texture2D<float4> heightmap : register(t0);
SamplerState hmsampler : register(s0);

struct VS_OUTPUT
{
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD;
    float4 norm : NORMAL;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
 //   return heightmap.Sample(hmsampler, input.tex);
    float4 light = normalize(float4(1.0f, 1.0f, -1.0f, 1.0f));
    float diffuse = saturate(dot(input.norm, -light));
    float ambient = 0.1f;
    float3 color = float3(0.2f, 0.7f, 0.2f);
    
    return float4(saturate((color * diffuse) + (color * ambient)), 1.0f);
}