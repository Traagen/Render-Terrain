cbuffer ConstantBuffer : register(b0)
{
	float4x4 viewproj;
	float4 eye;
	int height;
	int width;
}
/*
Texture2D<float4> heightmap : register(t0);
*/
struct DS_OUTPUT
{
	float4 pos : SV_POSITION;
//	float4 worldpos : POSITION;
	float4 norm : NORMAL;
//	float4 tex : TEXCOORD;
};

// Output control point
struct HS_CONTROL_POINT_OUTPUT
{
	float4 pos : POSITION0;
//	float4 worldpos : POSITION1;
	float4 norm : NORMAL;
//	float4 tex : TEXCOORD;
};

// Output patch constant data.
struct HS_CONSTANT_DATA_OUTPUT
{
	float EdgeTessFactor[3]			: SV_TessFactor; // e.g. would be [4] for a quad domain
	float InsideTessFactor			: SV_InsideTessFactor; // e.g. would be Inside[2] for a quad domain
	// TODO: change/add other stuff
};

#define NUM_CONTROL_POINTS 3

[domain("tri")]
DS_OUTPUT main(
	HS_CONSTANT_DATA_OUTPUT input,
	float3 domain : SV_DomainLocation,
	const OutputPatch<HS_CONTROL_POINT_OUTPUT, NUM_CONTROL_POINTS> patch)
{
	DS_OUTPUT output;

	output.pos = float4(patch[0].pos.xyz*domain.x+patch[1].pos.xyz*domain.y+patch[2].pos.xyz*domain.z,1);
	output.pos = mul(output.pos, viewproj);
//	output.worldpos = float4(patch[0].worldpos.xyz*domain.x + patch[1].worldpos.xyz*domain.y + patch[2].worldpos.xyz*domain.z, 1);
	output.norm = float4(patch[0].norm.xyz*domain.x + patch[1].norm.xyz*domain.y + patch[2].norm.xyz*domain.z, 1);
//	output.tex = float4(patch[0].tex*domain.x + patch[1].tex*domain.y + patch[2].tex*domain.z);

	return output;
}
