struct LightData {
	float4 pos;
	float4 amb;
	float4 dif;
	float4 spec;
	float3 att;
	float rng;
	float3 dir;
	float sexp;
};

cbuffer PerFrameData : register(b0)
{
	float4x4 viewproj;
	float4x4 shadowtexmatrices[4];
	float4 eye;
	float4 frustum[6];
	LightData light;
}

cbuffer TerrainData : register(b1)
{
	float scale;
	float width;
	float depth;
	float base;
}

Texture2D<float> heightmap : register(t0);
Texture2D<float> shadowmap : register(t1);
Texture2D<float4> displacementmap : register(t2);

SamplerState hmsampler : register(s0);
SamplerComparisonState shadowsampler : register(s2);
SamplerState displacementsampler : register(s3);

struct DS_OUTPUT
{
	float4 pos : SV_POSITION;
	float4 shadowpos[4] : TEXCOORD0;
	float3 worldpos : POSITION;
    float2 tex : TEXCOORD4;
};

// shadow map constants
static const float SMAP_SIZE = 4096.0f;
static const float SMAP_DX = 1.0f / SMAP_SIZE;

// code for putting together cotangent frame and perturbing normal from normal map.
// code originally presented by Christian Schuler
// http://www.thetenthplanet.de/archives/1180
// converted from his glsl to hlsl
float3x3 cotangent_frame(float3 N, float3 p, float2 uv) {
	// get edge vectors of the pixel triangle
	float3 dp1 = ddx(p);
	float3 dp2 = ddy(p);
	float2 duv1 = ddx(uv);
	float2 duv2 = ddy(uv);

	// solve the linear system
	float3 dp2perp = cross(dp2, N);
	float3 dp1perp = cross(N, dp1);
	float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
	float3 B = dp2perp * duv1.y + dp1perp * duv2.y;

	// construct a scale-invariant frame
	float invmax = rsqrt(max(dot(T, T), dot(B, B)));
	return float3x3(T * invmax, B * invmax, N);
}

float3 perturb_normal(float3 N, float3 V, float2 texcoord) {
	// assume N, the interpolated vertex normal and
	// V, the view vector (vertex to eye)
	float3 map = 2.0f * displacementmap.Sample(displacementsampler, texcoord).xyz - 1.0f;
	float3x3 TBN = cotangent_frame(N, -V, texcoord);
	return normalize(mul(map, TBN));
}

float3 estimateNormal(float2 texcoord) {
	float2 b = texcoord + float2(0.0f, -0.3f / depth);
	float2 c = texcoord + float2(0.3f / width, -0.3f / depth);
	float2 d = texcoord + float2(0.3f / width, 0.0f);
	float2 e = texcoord + float2(0.3f / width, 0.3f / depth);
	float2 f = texcoord + float2(0.0f, 0.3f / depth);
	float2 g = texcoord + float2(-0.3f / width, 0.3f / depth);
	float2 h = texcoord + float2(-0.3f / width, 0.0f);
	float2 i = texcoord + float2(-0.3f / width, -0.3f / depth);

	float zb = heightmap.SampleLevel(hmsampler, b, 0) * scale;
	float zc = heightmap.SampleLevel(hmsampler, c, 0) * scale;
	float zd = heightmap.SampleLevel(hmsampler, d, 0) * scale;
	float ze = heightmap.SampleLevel(hmsampler, e, 0) * scale;
	float zf = heightmap.SampleLevel(hmsampler, f, 0) * scale;
	float zg = heightmap.SampleLevel(hmsampler, g, 0) * scale;
	float zh = heightmap.SampleLevel(hmsampler, h, 0) * scale;
	float zi = heightmap.SampleLevel(hmsampler, i, 0) * scale;

	float x = zg + 2 * zh + zi - zc - 2 * zd - ze;
	float y = 2 * zb + zc + zi - ze - 2 * zf - zg;
	float z = 8.0f;

	return normalize(float3(x, y, z));
}

float calcShadowFactor(float4 shadowPosH) {
	// No need to divide shadowPosH.xyz by shadowPosH.w because we only have a directional light.
	
	// Depth in NDC space.
	float depth = shadowPosH.z;

	// Texel size.
	const float dx = SMAP_DX;

	float percentLit = 0.0f;
	
	const float2 offsets[9] = {
		float2(-dx,  -dx), float2(0.0f,  -dx), float2(dx,  -dx),
		float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
		float2(-dx,   dx), float2(0.0f,   dx), float2(dx,   dx)
	};
//	return shadowmap.SampleCmpLevelZero(shadowsampler, shadowPosH.xy, depth);
	// 3x3 box filter pattern. Each sample does a 4-tap PCF.
	[unroll]
	for (int i = 0; i < 9; ++i) {
		percentLit += shadowmap.SampleCmpLevelZero(shadowsampler, shadowPosH.xy + offsets[i], depth);
	}

	// average the samples.
	return percentLit / 9.0f;
}

float decideOnCascade(float4 shadowpos[4]) {
	// if shadowpos[0].xy is in the range [0, 0.5], then this point is in the first cascade
	if (max(abs(shadowpos[0].x - 0.25), abs(shadowpos[0].y - 0.25)) < 0.24) {
		return calcShadowFactor(shadowpos[0]);
	}

	if (max(abs(shadowpos[1].x - 0.25), abs(shadowpos[1].y - 0.75)) < 0.24) {
		return calcShadowFactor(shadowpos[1]);
	}

	if (max(abs(shadowpos[2].x - 0.75), abs(shadowpos[2].y - 0.25)) < 0.24) {
		return calcShadowFactor(shadowpos[2]);
	}
	
	return calcShadowFactor(shadowpos[3]);
}

// basic diffuse/ambient lighting
float4 main(DS_OUTPUT input) : SV_TARGET
{
//    return heightmap.Sample(hmsampler, input.tex);
 //   float4 light = normalize(float4(1.0f, 1.0f, -2.0f, 1.0f));

	//float3 N = normalize(input.norm);
	//float3 viewvector = eye.xyz - input.worldpos;
	//N = perturb_normal(N, viewvector, input.tex);
	//float4 norm = float4(N, 1.0f);
	float3 norm = estimateNormal(input.tex);
	float3 viewvector = eye.xyz - input.worldpos;
	norm = perturb_normal(norm, viewvector, input.tex * 64.0f);

	float4 color = float4(0.22f, 0.72f, 0.31f, 1.0f);

	float shadowfactor = decideOnCascade(input.shadowpos);
	float4 diffuse = max(shadowfactor, light.amb) * light.dif * dot(-light.dir, norm);
	float3 V = reflect(light.dir, norm);
	float3 toEye = normalize(eye.xyz - input.worldpos);
	float4 specular = shadowfactor * 0.1f * light.spec * pow(max(dot(V, toEye), 0.0f), 2.0f);
	
	return (diffuse + specular) * color;
}