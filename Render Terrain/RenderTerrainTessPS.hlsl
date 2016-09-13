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
Texture2D<float4> detailmap1 : register(t3);
Texture2D<float4> detailmap2 : register(t4);
Texture2D<float4> detailmap3 : register(t5);
Texture2D<float4> detailmap4 : register(t6);

SamplerState hmsampler : register(s0);
SamplerComparisonState shadowsampler : register(s2);
SamplerState displacementsampler : register(s3);

struct DS_OUTPUT
{
	float4 pos : SV_POSITION;
	float4 shadowpos[4] : TEXCOORD0;
	float3 worldpos : POSITION;
};

// shadow map constants
static const float SMAP_SIZE = 4096.0f;
static const float SMAP_DX = 1.0f / SMAP_SIZE;

float3 triplanar_sample(Texture2D tex, SamplerState sam, float3 texcoord, float3 N) {
	float tighten = 0.4679f;
	float3 blending = saturate(abs(N) - tighten);
	// force weights to sum to 1.0
	float b = blending.x + blending.y + blending.z;
	blending /= float3(b, b, b);

	float3 x = tex.Sample(sam, texcoord.yz).xyz;
	float3 y = tex.Sample(sam, texcoord.xz).xyz;
	float3 z = tex.Sample(sam, texcoord.xy).xyz;

	return x * blending.x + y * blending.y + z * blending.z;
}

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

float3 perturb_normal(float3 N, float3 V, float2 texcoord, Texture2D tex, SamplerState sam) {
	// assume N, the interpolated vertex normal and
	// V, the view vector (vertex to eye)
	float3 map = 2.0f * tex.Sample(sam, texcoord.xy).xyz - 1.0f;
	map.z *= 2.0f; // scale normal as displacement height is scaled by 0.5
	float3x3 TBN = cotangent_frame(N, -V, texcoord.xy);
	return normalize(mul(map, TBN));
}

float3 perturb_normal_triplanar(float3 N, float3 V, float3 texcoord, Texture2D tex, SamplerState sam) {
	float3 map = triplanar_sample(tex, sam, texcoord, N);
	map = map - 0.5f;

	float3x3 TBN = cotangent_frame(N, -V, texcoord);
	return normalize(mul(map, TBN));
}

float3 triplanar_slopebased_sample(float slope, float3 N, float3 V, float3 uvw, Texture2D texXY, Texture2D texZ, SamplerState sam) {
	float3 blending = abs(N);
	// force weights to sum to 1.0
	float b = blending.x + blending.y + blending.z;
	blending /= float3(b, b, b);

		float3 x = texXY.Sample(sam, uvw.yz).xyz;
		float3 y = texXY.Sample(sam, uvw.xz).xyz;
		float3 z = texXY.Sample(sam, uvw.xy).xyz;

		if (slope < 0.25f) {
			return texZ.Sample(sam, uvw.xy).xyz;
		}

		if (slope < 0.5f) {
			float3 z2 = texZ.Sample(sam, uvw.xy).xyz;
		
			float blend = (slope - 0.25f) * (1.0f / (0.5f - 0.25f));

			return lerp(z2, x * blending.x + y * blending.y + z * blending.z, blend);
		} 
		
		return x * blending.x + y * blending.y + z * blending.z;
}

float3 perturb_normal_slopebased_triplanar(float slope, float3 N, float3 V, float3 uvw, Texture2D texXY, Texture2D texZ, SamplerState sam) {
	float3 map = triplanar_slopebased_sample(slope, N, V, uvw, texXY, texZ, sam);
	map = 2.0f * map - 1.0f;

	float3x3 TBN = cotangent_frame(N, -V, uvw);
	return normalize(mul(map, TBN));
}

float4 slope_based_color(float slope, float4 colorSteep, float4 colorFlat) {
	if (slope < 0.25f) {
		return colorFlat;
	}

	if (slope < 0.5f) {
		float blend = (slope - 0.25f) * (1.0f / (0.5f - 0.25f));

		return lerp(colorFlat, colorSteep, blend);
	} else {
		return colorSteep;
	}
}

float4 height_and_slope_based_color(float height, float slope) {
	float4 grass = float4(0.22f, 0.52f, 0.11f, 1.0f);
	float4 dirt = float4(0.35f, 0.20f, 0.0f, 1.0f);
	float4 rock = float4(0.42f, 0.42f, 0.52f, 1.0f);
	float4 snow = float4(0.8f, 0.8f, 0.8f, 1.0f);

	float bounds = scale * 0.02f;
	float transition = scale * 0.6f;
	float greenBlendEnd = transition + bounds;
	float greenBlendStart = transition - bounds;
	float snowBlendEnd = greenBlendEnd + 2 * bounds;

	if (height < greenBlendStart) {
		// get grass/dirt values
		return slope_based_color(slope, dirt, grass);
	}

	if (height < greenBlendEnd) {
		// get both grass/dirt values and rock values and blend
		float4 c1 = slope_based_color(slope, dirt, grass);
		float4 c2 = rock;

		float blend = (height - greenBlendStart) * (1.0f / (greenBlendEnd - greenBlendStart));
		return lerp(c1, c2, blend);
	}

	if (height < snowBlendEnd) {
		// get rock values and rock/snow values and blend
		float4 c1 = rock;
		float4 c2 = slope_based_color(slope, rock, snow);
		
		float blend = (height - greenBlendEnd) * (1.0f / (snowBlendEnd - greenBlendEnd));
		return lerp(c1, c2, blend);
	}

	// get rock/snow values
	return slope_based_color(slope, rock, snow);
}

float3 height_and_slope_based_normal(float height, float slope, float3 N, float3 V, float3 uvw) {
	float bounds = scale * 0.02f;
	float transition = scale * 0.6f;
	float greenBlendEnd = transition + bounds;
	float greenBlendStart = transition - bounds;
	float snowBlendEnd = greenBlendEnd + 2 * bounds;

	float3 N1 = perturb_normal_slopebased_triplanar(slope, N, V, uvw, detailmap2, detailmap1, displacementsampler);
	
	if (height < greenBlendStart) {
		// get grass/dirt values
		return N1;
	}

	float3 N2 = perturb_normal_triplanar(N, V, uvw, detailmap3, displacementsampler);

	if (height < greenBlendEnd) {
		// get both grass/dirt values and rock values and blend
		float blend = (height - greenBlendStart) * (1.0f / (greenBlendEnd - greenBlendStart));
		return lerp(N1, N2, blend);
	}

	float3 N3 = perturb_normal_slopebased_triplanar(slope, N, V, uvw, detailmap3, detailmap4, displacementsampler);

	if (height < snowBlendEnd) {
		// get rock values and rock/snow values and blend
		float blend = (height - greenBlendEnd) * (1.0f / (snowBlendEnd - greenBlendEnd));
		return lerp(N2, N3, blend);
	}

	// get rock/snow values
	return N3;
}

float3 dist_based_normal(float height, float slope, float3 N, float3 V, float3 uvw) {
	float dist = length(V);

	float3 N1 = perturb_normal(N, V, uvw / 32, displacementmap, displacementsampler);
	//float3 N1 = perturb_normal_triplanar(N, V, uvw / 32, displacementmap, displacementsampler);

	if (dist > 150) return N;

	if (dist > 100) {
		float blend = (dist - 100.0f) / 50.0f;

		return lerp(N1, N, blend);
	}

	float3 N2 = height_and_slope_based_normal(height, slope, N1, V, uvw);

	if (dist > 50) return N1;
	
	if (dist > 25) {
		float blend = (dist - 25.0f) / 25.0f;

		return lerp(N2, N1, blend);
	}

	return N2;
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
	float3 norm = estimateNormal(input.worldpos / width);
	float3 viewvector = eye.xyz - input.worldpos;
	float s = acos(norm.z);

	norm = dist_based_normal(input.worldpos.z, s, norm, viewvector, input.worldpos);
	float4 color = height_and_slope_based_color(input.worldpos.z, s);
	
	float shadowfactor = decideOnCascade(input.shadowpos);
	float4 diffuse = max(shadowfactor, light.amb) * light.dif * dot(-light.dir, norm);
	float3 V = reflect(light.dir, norm);
	float3 toEye = normalize(eye.xyz - input.worldpos);
	float4 specular = shadowfactor * 0.1f * light.spec * pow(max(dot(V, toEye), 0.0f), 2.0f);
	
	return (diffuse + specular) * color;
}