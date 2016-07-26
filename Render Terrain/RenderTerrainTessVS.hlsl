struct VS_OUTPUT
{
	float3 worldpos : POSITION0;
	float2 boundsZ : POSITION1;
    float2 tex : TEXCOORD;
};

struct VS_INPUT {
	float3 pos : POSITION0;
	float2 boundsZ : POSITION1;
	float2 tex : TEXCOORD;
};

VS_OUTPUT main(VS_INPUT input) {
	VS_OUTPUT output;

	output.tex = input.tex;
	output.worldpos = input.pos;
	output.boundsZ = input.boundsZ;

	return output;
}