cbuffer PerFrameData : register(b0)
{
	float4x4 viewproj;
	float4 eye;
	float4 frustum[6];
}

cbuffer TerrainData : register(b1)
{
	float scale;
	float width;
	float depth;
}

Texture2D<float> heightmap : register(t0);
SamplerState hmsampler : register(s0);
SamplerState detailsampler : register(s1);

struct DS_OUTPUT
{
	float4 pos : SV_POSITION;
	float3 worldpos : POSITION;
	float2 tex : TEXCOORD;
};

// Output control point
struct HS_CONTROL_POINT_OUTPUT
{
	float3 worldpos : POSITION;
	float2 tex : TEXCOORD;
};

// Output patch constant data.
struct HS_CONSTANT_DATA_OUTPUT
{
	float EdgeTessFactor[4]			: SV_TessFactor; // e.g. would be [4] for a quad domain
	float InsideTessFactor[2]		: SV_InsideTessFactor; // e.g. would be Inside[2] for a quad domain
};

#define NUM_CONTROL_POINTS 4

[domain("quad")]
DS_OUTPUT main(
	HS_CONSTANT_DATA_OUTPUT input,
	float2 domain : SV_DomainLocation,
	const OutputPatch<HS_CONTROL_POINT_OUTPUT, NUM_CONTROL_POINTS> patch)
{
	DS_OUTPUT output;

	output.worldpos = lerp(lerp(patch[0].worldpos, patch[1].worldpos, domain.x), lerp(patch[2].worldpos, patch[3].worldpos, domain.x), domain.y);
	output.tex = lerp(lerp(patch[0].tex, patch[1].tex, domain.x), lerp(patch[2].tex, patch[3].tex, domain.x), domain.y);
	output.worldpos.z = heightmap.SampleLevel(hmsampler, output.tex, 0.0f) * scale;
	
//	float3 norm = estimateNormal(output.tex);
//	output.worldpos += norm * (2.0f * heightmap.SampleLevel(detailsampler, output.tex * 256.0f, 0.0f) - 1.0f) / 5.0f;
	
	//output.norm = posnorm.yzw;
	// perturb along face normal
	//output.norm = normalize(patch[0].norm * domain.x + patch[1].norm * domain.y + patch[2].norm * domain.z);
//	output.norm = lerp(lerp(patch[0].norm, patch[1].norm, domain.x), lerp(patch[2].norm, patch[3].norm, domain.x), domain.y);
/*	float mysample = heightmap.SampleLevel(hmsampler, output.tex / 16.0f, 0.0f);
	output.pos = float4(output.pos.xyz + output.norm * (2.0f * mysample - 1.0f), 1.0f);
	*/
	
	output.pos = float4(output.worldpos, 1.0f);
	output.pos = mul(output.pos, viewproj);
	return output;
}
