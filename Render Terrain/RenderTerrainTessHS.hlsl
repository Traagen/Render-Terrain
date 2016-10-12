cbuffer PerFrameData : register(b1)
{
	float4x4 viewproj;
	float4x4 shadowtexmatrices[4];
	float4 eye;
	float4 frustum[6];
}

// Input control point
struct VS_OUTPUT
{
	float3 worldpos : POSITION0;
	float3 aabbmin : POSITION1;
	float3 aabbmax : POSITION2;
	uint skirt : SKIRT;
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
	uint skirt : SKIRT;
};

#define NUM_CONTROL_POINTS 4

// code for frustum culling and dynamic LOD from Introduction to 3D Game Programming with DirectX 11.

// returns true if the box is completely behind the plane.
bool aabbBehindPlaneTest(float3 center, float3 extents, float4 plane) {
	float3 n = abs(plane.xyz); // |n.x|, |n.y|, |n.z|

	float e = dot(extents, n); // always positive

	float s = dot(float4(center, 1.0f), plane); // signed distance from center point to plane

	// if the center point of the box is a distance of e or more behind the plane
	// (in which case s is negative since it is behind the plane), then the box
	// is completely in the negative half space of the plane.
	return (s + e) < 0.0f;
}

// returns true if the box is completely outside the frustum.
bool aabbOutsideFrustumTest(float3 center, float3 extents, float4 frustumPlanes[6]) {
	[unroll]
	for (int i = 0; i < 6; ++i) {
		// if the box is completely behind any of the frustum planes, then it is outside the frustum.
		if (aabbBehindPlaneTest(center, extents, frustumPlanes[i])) {
			return true;
		}
	}

	return false;
}

float CalcTessFactor(float3 p) {
	float d = distance(p, eye.xyz);

	float s = saturate((d - 16.0f) / (256.0f - 16.0f));
	return pow(2, (lerp(6, 0, s)));
}

// Patch Constant Function
HS_CONSTANT_DATA_OUTPUT CalcHSPatchConstants(
	InputPatch<VS_OUTPUT, NUM_CONTROL_POINTS> ip,
	uint PatchID : SV_PrimitiveID)
{
	HS_CONSTANT_DATA_OUTPUT output;
	output.skirt = ip[0].skirt;

	// build axis-aligned bounding box. 
	float3 vMin = ip[0].aabbmin;
	float3 vMax = ip[0].aabbmax;
	
	// center/extents representation.
	float3 boxCenter = 0.5f * (vMin + vMax);
	float3 boxExtents = 0.5f * (vMax - vMin);

	if (aabbOutsideFrustumTest(boxCenter, boxExtents, frustum)) {
		output.EdgeTessFactor[0] = 0.0f;
		output.EdgeTessFactor[1] = 0.0f;
		output.EdgeTessFactor[2] = 0.0f;
		output.EdgeTessFactor[3] = 0.0f;
		output.InsideTessFactor[0] = 0.0f;
		output.InsideTessFactor[1] = 0.0f;

		return output;
	} else {
		// don't tessellate any part of the skirt that isn't directly touching the terrain.
		if (output.skirt == 0) {
			output.EdgeTessFactor[0] = 1.0f;
			output.EdgeTessFactor[1] = 1.0f;
			output.EdgeTessFactor[2] = 1.0f;
			output.EdgeTessFactor[3] = 1.0f;
			output.InsideTessFactor[0] = 1.0f;
			output.InsideTessFactor[1] = 1.0f;
			
			return output;
		} else if (output.skirt < 5) {
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

	return output;
}
