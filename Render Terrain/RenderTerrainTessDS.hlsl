cbuffer ConstantBuffer : register(b0)
{
	float4x4 viewproj;
	float4 eye;
	float4 frustum[6];
	float scale;
	int height;
	int width;
}

Texture2D<float4> heightmap : register(t0);
SamplerState hmsampler : register(s0);
SamplerState detailsampler : register(s1);

struct DS_OUTPUT
{
	float4 pos : SV_POSITION;
	float3 worldpos : POSITION;
//	float3 norm : NORMAL;
	float2 tex : TEXCOORD;
};

// Output control point
struct HS_CONTROL_POINT_OUTPUT
{
//	float4 pos : POSITION0;
	float3 worldpos : POSITION;
//	float3 norm : NORMAL;
	float2 tex : TEXCOORD;
};

// Output patch constant data.
struct HS_CONSTANT_DATA_OUTPUT
{
	float EdgeTessFactor[4]			: SV_TessFactor; // e.g. would be [4] for a quad domain
	float InsideTessFactor[2]		: SV_InsideTessFactor; // e.g. would be Inside[2] for a quad domain
};

#define NUM_CONTROL_POINTS 4

float3 estimateNormal(float2 texcoord) {
	float2 leftTex = texcoord + float2(-1.0f / (float)width, 0.0f);
	float2 rightTex = texcoord + float2(1.0f / (float)width, 0.0f);
	float2 bottomTex = texcoord + float2(0.0f, 1.0f / (float)height);
	float2 topTex = texcoord + float2(0.0f, -1.0f / (float)height);

	float leftZ = heightmap.SampleLevel(hmsampler, leftTex, 0).r * scale;
	float rightZ = heightmap.SampleLevel(hmsampler, rightTex, 0).r * scale;
	float bottomZ = heightmap.SampleLevel(hmsampler, bottomTex, 0).r * scale;
	float topZ = heightmap.SampleLevel(hmsampler, topTex, 0).r * scale;

	return normalize(float3(leftZ - rightZ, topZ - bottomZ, 1.0f));
}


[domain("quad")]
DS_OUTPUT main(
	HS_CONSTANT_DATA_OUTPUT input,
	float2 domain : SV_DomainLocation,
	const OutputPatch<HS_CONTROL_POINT_OUTPUT, NUM_CONTROL_POINTS> patch)
{
	DS_OUTPUT output;

	output.worldpos = lerp(lerp(patch[0].worldpos, patch[1].worldpos, domain.x), lerp(patch[2].worldpos, patch[3].worldpos, domain.x), domain.y);
	output.tex = lerp(lerp(patch[0].tex, patch[1].tex, domain.x), lerp(patch[2].tex, patch[3].tex, domain.x), domain.y);
	output.worldpos.z = heightmap.SampleLevel(hmsampler, output.tex, 0.0f).x * scale;
	
//	float3 norm = estimateNormal(output.tex);
//	output.worldpos += norm * (2.0f * heightmap.SampleLevel(detailsampler, output.tex * 256.0f, 0.0f).x - 1.0f) / 5.0f;
	
	//output.norm = posnorm.yzw;
	// perturb along face normal
	//output.norm = normalize(patch[0].norm * domain.x + patch[1].norm * domain.y + patch[2].norm * domain.z);
//	output.norm = lerp(lerp(patch[0].norm, patch[1].norm, domain.x), lerp(patch[2].norm, patch[3].norm, domain.x), domain.y);
/*	float4 mysample = heightmap.SampleLevel(hmsampler, output.tex / 16.0f, 0.0f);
	output.pos = float4(output.pos.xyz + output.norm * (2.0f * mysample.r - 1.0f), 1.0f);
	*/
	
	output.pos = float4(output.worldpos, 1.0f);
	output.pos = mul(output.pos, viewproj);
	return output;
}
