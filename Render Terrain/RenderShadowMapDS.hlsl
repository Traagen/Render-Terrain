cbuffer TerrainData : register(b0) {
	float scale;
	float width;
	float depth;
	float base;
}

cbuffer ShadowConstants : register(b1) {
	float4x4 shadowmatrix;
}

Texture2D<float4> heightmap : register(t0);
Texture2D<float4> displacementmap : register(t1);
SamplerState hmsampler : register(s0);
SamplerState displacementsampler : register(s1);

struct DS_OUTPUT
{
	float4 pos : SV_POSITION;
};

// Output control point
struct HS_CONTROL_POINT_OUTPUT
{
	float3 worldpos : POSITION;
};

// Output patch constant data.
struct HS_CONSTANT_DATA_OUTPUT
{
	float EdgeTessFactor[4]			: SV_TessFactor; // e.g. would be [4] for a quad domain
	float InsideTessFactor[2]		: SV_InsideTessFactor; // e.g. would be Inside[2] for a quad domain
	uint skirt						: SKIRT;
};

float3 estimateNormal(float2 texcoord) {
	float2 b = texcoord + float2(0.0f, -0.3f / depth);
	float2 c = texcoord + float2(0.3f / width, -0.3f / depth);
	float2 d = texcoord + float2(0.3f / width, 0.0f);
	float2 e = texcoord + float2(0.3f / width, 0.3f / depth);
	float2 f = texcoord + float2(0.0f, 0.3f / depth);
	float2 g = texcoord + float2(-0.3f / width, 0.3f / depth);
	float2 h = texcoord + float2(-0.3f / width, 0.0f);
	float2 i = texcoord + float2(-0.3f / width, -0.3f / depth);

	float zb = heightmap.SampleLevel(hmsampler, b, 0).x * scale;
	float zc = heightmap.SampleLevel(hmsampler, c, 0).x * scale;
	float zd = heightmap.SampleLevel(hmsampler, d, 0).x * scale;
	float ze = heightmap.SampleLevel(hmsampler, e, 0).x * scale;
	float zf = heightmap.SampleLevel(hmsampler, f, 0).x * scale;
	float zg = heightmap.SampleLevel(hmsampler, g, 0).x * scale;
	float zh = heightmap.SampleLevel(hmsampler, h, 0).x * scale;
	float zi = heightmap.SampleLevel(hmsampler, i, 0).x * scale;

	float x = zg + 2 * zh + zi - zc - 2 * zd - ze;
	float y = 2 * zb + zc + zi - ze - 2 * zf - zg;
	float z = 8.0f;

	return normalize(float3(x, y, z));
}

#define NUM_CONTROL_POINTS 4

[domain("quad")]
DS_OUTPUT main(
	HS_CONSTANT_DATA_OUTPUT input,
	float2 domain : SV_DomainLocation,
	const OutputPatch<HS_CONTROL_POINT_OUTPUT, NUM_CONTROL_POINTS> patch)
{
	DS_OUTPUT output;
	float3 worldpos = lerp(lerp(patch[0].worldpos, patch[1].worldpos, domain.x), lerp(patch[2].worldpos, patch[3].worldpos, domain.x), domain.y);

	if (input.skirt < 5) {
		if (input.skirt > 0 && domain.y == 1) {
			worldpos.z = heightmap.SampleLevel(hmsampler, worldpos / width, 0.0f).x * scale;
		}
	} else {
		worldpos.z = heightmap.SampleLevel(hmsampler, worldpos / width, 0.0f).x * scale;
	}

	float3 norm = estimateNormal(worldpos / width);
	worldpos += norm * 0.5f * (2.0f * displacementmap.SampleLevel(displacementsampler, worldpos / 32, 0.0f).w - 1.0f);

	output.pos = float4(worldpos, 1.0f);
	output.pos = mul(output.pos, shadowmatrix);
	return output;
}
