/*
struct VS_INPUT {
	float3 pos : POSITION;
	float4 color : COLOR;
};
*/
struct VS_OUTPUT {
	float4 pos : SV_POSITION;
	float4 color : COLOR;
};
/*
VS_OUTPUT main(VS_INPUT input) {
	VS_OUTPUT output;
	output.pos = float4(input.pos, 1.0f);
	output.color = input.color;
	return output;
}
*/

VS_OUTPUT main(uint input : SV_VERTEXID) {
	VS_OUTPUT output;

	output.pos = float4(float2((input << 1) & 2, input == 0) * float2(2.0f, -4.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
	output.color = float4((input == 0) * 1.0f, (input == 1) * 1.0f, (input == 2) * 1.0f, 1.0f);

	return output;
}