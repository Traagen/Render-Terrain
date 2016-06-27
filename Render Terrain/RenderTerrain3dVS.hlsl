struct VS_OUTPUT
{
	float4 pos : SV_POSITION;
	float2 tex : TEXCOORD;
    float4 norm : NORMAL;
};

cbuffer ConstantBuffer : register(b0)
{
    float4x4 viewproj;
    int height;
    int width;
}

Texture2D<float4> heightmap : register(t0);

VS_OUTPUT main(float3 input : POSITION)
{
	VS_OUTPUT output;

    float4 mysample = heightmap.Load(int3(input));
    output.pos = float4(input.x, input.y, mysample.r*256, 1.0f);
    output.tex = float2(input.x / height, input.y / width);

    output.pos = mul(output.pos, viewproj);

    // calculate vertex normal from heightmap
    float zb = heightmap.Load(int3(input.xy + int2(0, -1), 0)).r* 256;
    float zc = heightmap.Load(int3(input.xy + int2(1, 0), 0)).r* 256;
    float zd = heightmap.Load(int3(input.xy + int2(0, 1), 0)).r* 256;
    float ze = heightmap.Load(int3(input.xy + int2(-1, 0), 0)).r* 256;

    float4 N = float4(ze - ze, zb - zd, 2.0f, 1.0f);
    N = normalize(N);
    output.norm = N;

 	return output;
}