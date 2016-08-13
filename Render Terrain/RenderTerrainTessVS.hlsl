struct VS_OUTPUT
{
	float3 worldpos : POSITION0;
	float3 aabbmin : POSITION1;
	float3 aabbmax : POSITION2;
    float3 tex : TEXCOORD;
};

struct VS_INPUT {
	float3 pos : POSITION0;
	float3 aabbmin : POSITION1;
	float3 aabbmax : POSITION2;
	float3 tex : TEXCOORD;
};

VS_OUTPUT main(VS_INPUT input) {
	VS_OUTPUT output;

	output.tex = input.tex;
	output.worldpos = input.pos;
	output.aabbmin = input.aabbmin;
	output.aabbmax = input.aabbmax;

	return output;
}