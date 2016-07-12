/*cbuffer ConstantBuffer : register(b0)
{
	float4x4 viewproj;
	float4 eye;
	int height;
	int width;
}

Texture2D<float4> heightmap : register(t0);
*/
// Input control point
struct VS_OUTPUT
{
	float4 pos : POSITION0;
//	float4 worldpos : POSITION1;
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

// Patch Constant Function
HS_CONSTANT_DATA_OUTPUT CalcHSPatchConstants(
	InputPatch<VS_OUTPUT, NUM_CONTROL_POINTS> ip,
	uint PatchID : SV_PrimitiveID)
{
	HS_CONSTANT_DATA_OUTPUT output;

	// Insert code to compute output here
	output.EdgeTessFactor[0] = 4;
	output.EdgeTessFactor[1] = 4;
	output.EdgeTessFactor[2] = 4;
	output.InsideTessFactor = 4; // e.g. could calculate dynamic tessellation factors instead

	return output;
}

[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("CalcHSPatchConstants")]
HS_CONTROL_POINT_OUTPUT main( 
	InputPatch<VS_OUTPUT, NUM_CONTROL_POINTS> ip, 
	uint i : SV_OutputControlPointID,
	uint PatchID : SV_PrimitiveID )
{
	HS_CONTROL_POINT_OUTPUT output;

	// Insert code to compute Output here
	output.pos = ip[i].pos;
	output.norm = ip[i].norm;
//	output.worldpos = ip[i].worldpos;
//	output.tex = ip[i].tex;

	return output;
}
