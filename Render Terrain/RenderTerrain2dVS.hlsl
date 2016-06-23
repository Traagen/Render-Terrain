struct VS_OUTPUT {
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD;
};

VS_OUTPUT main(uint input : SV_VERTEXID) {
	VS_OUTPUT output;

	output.pos = float4(float2((input << 1) & 2, input == 0) * float2(2.0f, -4.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
	output.tex = float2((output.pos.x + 1) / 2, (output.pos.y + 1) / 2);

	return output;
}