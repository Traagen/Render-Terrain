cbuffer PerFrameData : register(b0)
{
	float4x4 viewproj;
	float4x4 shadowviewproj;
	float4x4 shadowtransform;
	float4 eye;
	float4 frustum[6];
}

// Input control point
struct VS_OUTPUT
{
	float3 worldpos : POSITION0;
	float3 aabbmin : POSITION1;
	float3 aabbmax : POSITION2;
	float3 tex : TEXCOORD;
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
	uint skirt						: SKIRT;
};

#define NUM_CONTROL_POINTS 4

float CalcTessFactor(float3 p) {
	float d = distance(p, eye);

	float s = saturate((d - 128.0f) / (256.0f - 128.0f));
	return pow(2, (lerp(4, 0, s)));
}

// Patch Constant Function
HS_CONSTANT_DATA_OUTPUT CalcHSPatchConstants(
	InputPatch<VS_OUTPUT, NUM_CONTROL_POINTS> ip,
	uint PatchID : SV_PrimitiveID)
{
	HS_CONSTANT_DATA_OUTPUT output;
	output.skirt = ip[0].tex.z;

	// don't tessellate any part of the skirt that isn't directly touching the terrain.
	if (ip[0].tex.z == 0.0f) {
		output.EdgeTessFactor[0] = 1.0f;
		output.EdgeTessFactor[1] = 1.0f;
		output.EdgeTessFactor[2] = 1.0f;
		output.EdgeTessFactor[3] = 1.0f;
		output.InsideTessFactor[0] = 1.0f;
		output.InsideTessFactor[1] = 1.0f;

		return output;
	}
	else if (ip[0].tex.z < 5.0f) {
		float3 e3 = 0.5f * (ip[2].worldpos + ip[3].worldpos);

		output.EdgeTessFactor[0] = 1.0f;
		output.EdgeTessFactor[1] = 1.0f;
		output.EdgeTessFactor[2] = 1.0f;
		output.EdgeTessFactor[3] = CalcTessFactor(e3);
		output.InsideTessFactor[0] = 1.0f;
		output.InsideTessFactor[1] = 1.0f;

		return output;
	}

	// tessellate based on distance from the camera.
	// compute tess factor based on edges.
	// compute midpoint of edges.
	float3 e0 = 0.5f * (ip[0].worldpos + ip[2].worldpos);
	float3 e1 = 0.5f * (ip[0].worldpos + ip[1].worldpos);
	float3 e2 = 0.5f * (ip[1].worldpos + ip[3].worldpos);
	float3 e3 = 0.5f * (ip[2].worldpos + ip[3].worldpos);
	float3 c = 0.25f * (ip[0].worldpos + ip[1].worldpos + ip[2].worldpos + ip[3].worldpos);

	output.EdgeTessFactor[0] = CalcTessFactor(e0);
	output.EdgeTessFactor[1] = CalcTessFactor(e1);
	output.EdgeTessFactor[2] = CalcTessFactor(e2);
	output.EdgeTessFactor[3] = CalcTessFactor(e3);
	output.InsideTessFactor[0] = CalcTessFactor(c);
	output.InsideTessFactor[1] = output.InsideTessFactor[0];

	return output;
}

[domain("quad")]
[partitioning("fractional_even")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("CalcHSPatchConstants")]
HS_CONTROL_POINT_OUTPUT main( 
	InputPatch<VS_OUTPUT, NUM_CONTROL_POINTS> ip, 
	uint i : SV_OutputControlPointID,
	uint PatchID : SV_PrimitiveID )
{
	HS_CONTROL_POINT_OUTPUT output;

	// Insert code to compute Output here
	output.worldpos = ip[i].worldpos;
	output.tex = ip[i].tex.xy;

	return output;
}
