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
	bool useTextures;
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
Texture2DArray<float4> detailmaps : register(t3);

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
//static const float4 colors[] = { { 0.22f, 0.52f, 0.11f, 1.0f }, { 0.8f, 0.8f, 0.8f, 1.0f }, { 0.35f, 0.2f, 0.0f, 1.0f },{ 0.42f, 0.42f, 0.52f, 1.0f } };
static const float4 colors[] = { { 0.35f, 0.5f, 0.18f, 1.0f },{ 0.89f, 0.89f, 0.89f, 1.0f },{ 0.31f, 0.25f, 0.2f, 1.0f },{ 0.39f, 0.37f, 0.38f, 1.0f } };

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

float3 SampleDetailTriplanar(float3 uvw, float3 N, float index) {
	float tighten = 0.4679f;
	float3 blending = saturate(abs(N) - tighten);
	// force weights to sum to 1.0
	float b = blending.x + blending.y + blending.z;
	blending /= float3(b, b, b);

	float3 x = detailmaps.Sample(displacementsampler, float3(uvw.yz, index)).xyz;
	float3 y = detailmaps.Sample(displacementsampler, float3(uvw.xz, index)).xyz;
	float3 z = detailmaps.Sample(displacementsampler, float3(uvw.xy, index)).xyz;

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

float3 sample_detailmap(float index, SamplerState sam, float3 uvw, float3 N) {
	float tighten = 0.4679f;
	float3 blending = saturate(abs(N) - tighten);
	// force weights to sum to 1.0
	float b = blending.x + blending.y + blending.z;
	blending /= float3(b, b, b);

	float3 x = detailmaps.Sample(sam, float3(uvw.yz, index)).xyz;
	float3 y = detailmaps.Sample(sam, float3(uvw.xz, index)).xyz;
	float3 z = detailmaps.Sample(sam, float3(uvw.xy, index)).xyz;

	return x * blending.x + y * blending.y + z * blending.z;
}

float3 get_normal_from_detailmap(float3 N, float3 V, float3 uvw, float index, SamplerState sam) {
	float3 map = sample_detailmap(index, sam, uvw, N);
	map = map - 0.5f;

	float3x3 TBN = cotangent_frame(N, -V, uvw);
	return normalize(mul(map, TBN));
}

float3 sample_detailmap_slopebased(float slope, float3 N, float3 V, float3 uvw, float indexXY, float indexZ, SamplerState sam) {
	if (slope < 0.25f) {
		return detailmaps.Sample(sam, float3(uvw.xy, indexZ)).xyz;
	}

	float tighten = 0.4679f;
	float3 blending = saturate(abs(N) - tighten);
	// force weights to sum to 1.0
	float b = blending.x + blending.y + blending.z;
	blending /= float3(b, b, b);

	float3 x = detailmaps.Sample(sam, float3(uvw.yz, indexXY)).xyz;
	float3 y = detailmaps.Sample(sam, float3(uvw.xz, indexXY)).xyz;
	float3 z = detailmaps.Sample(sam, float3(uvw.xy, indexXY)).xyz;

	if (slope < 0.5f) {
		float3 z2 = detailmaps.Sample(sam, float3(uvw.xy, indexZ)).xyz;

		float blend = (slope - 0.25f) * (1.0f / (0.5f - 0.25f));

		return lerp(z2, x * blending.x + y * blending.y + z * blending.z, blend);
	}

	return x * blending.x + y * blending.y + z * blending.z;
}

float3 get_normal_from_detailmap_slopebased(float slope, float3 N, float3 V, float3 uvw, float indexXY, float indexZ, SamplerState sam) {
	float3 map = sample_detailmap_slopebased(slope, N, V, uvw, indexXY, indexZ, sam);
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

float4 height_and_slope_based_texture(float height, float slope, float3 N, float3 V, float3 uvw) {
	float bounds = scale * 0.005f;
	float transition = scale * 0.6f;
	float greenBlendStart = transition - bounds;
	float snowBlendEnd = transition + 2 * bounds;
	float index1 = 1, index2 = 0, index3 = -1, index4;

	if (height > snowBlendEnd) {
		index1 = 2;
		index2 = 3;
	}
	else if (height > greenBlendStart) {
		index3 = 2;
		index4 = 3;
	}

	float3 C1 = sample_detailmap_slopebased(slope, N, V, uvw, index1, index2, displacementsampler);

	if (index3 != -1) {
		float blend = (height - greenBlendStart) * (1.0f / (snowBlendEnd - greenBlendStart));

		float3 C2 = sample_detailmap_slopebased(slope, N, V, uvw, index3, index4, displacementsampler);
		return float4(lerp(C1, C2, blend), 1);
	}

	return float4(C1, 1);
}

float4 height_and_slope_based_color(float height, float slope) {
	float4 colors[] = { { 0.22f, 0.52f, 0.11f, 1.0f },{ 0.35f, 0.2f, 0.0f, 1.0f },{ 0.42f, 0.42f, 0.52f, 1.0f },{ 0.8f, 0.8f, 0.8f, 1.0f } };
	float bounds = scale * 0.005f;
	float transition = scale * 0.6f;
	float greenBlendEnd = transition + bounds;
	float greenBlendStart = transition - bounds;
	float snowBlendEnd = greenBlendEnd + 2 * bounds;
	float index1 = 1, index2 = 0, index3 = -1, index4;

	if (height > snowBlendEnd) {
		index1 = 2;
		index2 = 3;
	} else if (height > greenBlendStart) {
		index3 = 2;
		index4 = 3;
	}

	float4 C1 = slope_based_color(slope, colors[index1], colors[index2]);

	if (index3 != -1) {
		float blend = (height - greenBlendStart) * (1.0f / (snowBlendEnd - greenBlendStart));

		float4 C2 = slope_based_color(slope, colors[index3], colors[index4]);
		return lerp(C1, C2, blend);
	}

	return C1;
}

float3 height_and_slope_based_normal(float height, float slope, float3 N, float3 V, float3 uvw) {
	float bounds = scale * 0.005f;
	float transition = scale * 0.6f;
	float greenBlendStart = transition - bounds;
	float snowBlendEnd = transition + 2 * bounds;
	float index1 = 1, index2 = 0, index3 = -1, index4;
	
	if (height > snowBlendEnd) {
		index1 = 2;
		index2 = 3;
	} else if (height > greenBlendStart) {
		index3 = 2;
		index4 = 3;
	}

	float3 N1 = get_normal_from_detailmap_slopebased(slope, N, V, uvw, index1, index2, displacementsampler);

	if (index3 != -1) {
		float blend = (height - greenBlendStart) * (1.0f / (snowBlendEnd - greenBlendStart));

		float3 N2 = get_normal_from_detailmap_slopebased(slope, N, V, uvw, index3, index4, displacementsampler);
		return lerp(N1, N2, blend);
	}

	return N1;
}

float3 GetTexByHeightPlanar(float height, float3 uvw, float index1, float index2) {
	float bounds = scale * 0.005f;
	float transition = scale * 0.6f;
	float blendStart = transition - 2 * bounds;
	float blendEnd = transition + 2 * bounds;
	float3 c;

	if (height < blendStart) {
		c = detailmaps.Sample(displacementsampler, float3(uvw.xy, index1));
	} else if (height < blendEnd) {
		float3 c1 = detailmaps.Sample(displacementsampler, float3(uvw.xy, index1));
		float3 c2 = detailmaps.Sample(displacementsampler, float3(uvw.xy, index2));

		float blend = (height - blendStart) * (1.0f / (blendEnd - blendStart));

		c = lerp(c1, c2, blend);
	} else {
		c = detailmaps.Sample(displacementsampler, float3(uvw.xy, index2));
	}

	return c;
}

float3 GetTexByHeightTriplanar(float height, float3 uvw, float3 N, float index1, float index2) {
	float bounds = scale * 0.005f;
	float transition = scale * 0.6f;
	float blendStart = transition - bounds;
	float blendEnd = transition + bounds;
	float3 c;

	if (height < blendStart) {
		c = SampleDetailTriplanar(uvw, N, index1);
	} else if (height < blendEnd) {
		float3 c1 = SampleDetailTriplanar(uvw, N, index1);
		float3 c2 = SampleDetailTriplanar(uvw, N, index2);
		float blend = (height - blendStart) * (1.0f / (blendEnd - blendStart));
		c = lerp(c1, c2, blend);
	} else {
		c = SampleDetailTriplanar(uvw, N, index2);
	}

	return c;
}

float4 GetColorByHeight(float height, float index1, float index2) {
	float bounds = scale * 0.005f;
	float transition = scale * 0.6f;
	float blendStart = transition - 2 * bounds;
	float blendEnd = transition + 2 * bounds;
	float4 c;

	if (height < blendStart) {
		c = colors[index1];
	} else if (height < blendEnd) {
		float4 c1 = colors[index1];
		float4 c2 = colors[index2];
		float blend = (height - blendStart) * (1.0f / (blendEnd - blendStart));
		c = lerp(c1, c2, blend);
	} else {
		c = colors[index2];
	}

	return c;
}


float3 GetTexBySlope(float slope, float height, float3 N, float3 uvw, float startingIndex) {
	float3 c;
	float blend;
	if (slope < 0.6f) {
		blend = slope / 0.6f;
		float3 c1 = GetTexByHeightPlanar(height, uvw, 0 + startingIndex, 1 + startingIndex);
		float3 c2 = GetTexByHeightTriplanar(height, uvw, N, 2 + startingIndex, 3 + startingIndex);
		c = lerp(c1, c2, blend);
	} else if (slope < 0.65f) {
		blend = (slope - 0.6f) * (1.0f / (0.65f - 0.6f));
		float3 c1 = GetTexByHeightTriplanar(height, uvw, N, 2 + startingIndex, 3 + startingIndex);
		float3 c2 = SampleDetailTriplanar(uvw, N, 3 + startingIndex);
		c = lerp(c1, c2, blend);
	} else {
		c = SampleDetailTriplanar(uvw, N, 3 + startingIndex);
	}

	return c;
}

float4 GetColorBySlope(float slope, float height) {
	float4 c;
	float blend;
	if (slope < 0.6f) {
		blend = slope / 0.6f;
		float4 c1 = GetColorByHeight(height, 0, 1);
		float4 c2 = GetColorByHeight(height, 2, 3);
		c = lerp(c1, c2, blend);
	} else if (slope < 0.65f) {
		blend = (slope - 0.6f) * (1.0f / (0.65f - 0.6f));
		float4 c1 = GetColorByHeight(height, 2, 3);
		float4 c2 = colors[3];
		c = lerp(c1, c2, blend);
	} else {
		c = colors[3];
	}

	return c;
}

float3 PerturbNormalByHeightSlope(float height, float slope, float3 N, float3 V, float3 uvw) {
	float3 c = GetTexBySlope(slope, height, N, uvw, 0) - 0.5f;

	float3x3 TBN = cotangent_frame(N, -V, uvw);
	return normalize(mul(c, TBN));
}

float4 dist_based_texturing(float height, float slope, float3 N, float3 V, float3 uvw) {
	float dist = length(V);

	if (dist > 75) return GetColorBySlope(slope, height);
	else if (dist > 25) {
		float blend = (dist - 25.0f) * (1.0f / (75.0f - 25.0f));
		float4 c1 = float4(GetTexBySlope(slope, height, N, uvw, 4), 1);
		float4 c2 = GetColorBySlope(slope, height);
		return lerp(c1, c2, blend);
	} else return float4(GetTexBySlope(slope, height, N, uvw, 4), 1);
}

float3 dist_based_normal(float height, float slope, float3 N, float3 V, float3 uvw) {
	float dist = length(V);

	float3 N1 = perturb_normal(N, V, uvw / 8, displacementmap, displacementsampler);
	//float3 N1 = perturb_normal_triplanar(N, V, uvw / 8, displacementmap, displacementsampler);

	if (dist > 150) return N;

	if (dist > 100) {
		float blend = (dist - 100.0f) / 50.0f;

		return lerp(N1, N, blend);
	}

	//float3 N2 = height_and_slope_based_normal(height, slope, N1, V, uvw);
	float3 N2 = PerturbNormalByHeightSlope(height, slope, N1, V, uvw);

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
	if (max(abs(shadowpos[0].x - 0.25), abs(shadowpos[0].y - 0.25)) < 0.247) {
		return calcShadowFactor(shadowpos[0]);
	}

	if (max(abs(shadowpos[1].x - 0.25), abs(shadowpos[1].y - 0.75)) < 0.247) {
		return calcShadowFactor(shadowpos[1]);
	}

	if (max(abs(shadowpos[2].x - 0.75), abs(shadowpos[2].y - 0.25)) < 0.247) {
		return calcShadowFactor(shadowpos[2]);
	}
	
	return calcShadowFactor(shadowpos[3]);
}

// basic diffuse/ambient lighting
float4 main(DS_OUTPUT input) : SV_TARGET
{
	float3 norm = estimateNormal(input.worldpos / width);
	float3 viewvector = eye.xyz - input.worldpos;
	
	norm = dist_based_normal(input.worldpos.z, acos(norm.z), norm, viewvector, input.worldpos / 4);
	float4 color;
	if (useTextures) color = dist_based_texturing(input.worldpos.z, acos(norm.z), norm, viewvector, input.worldpos / 4);
	else color = GetColorBySlope(acos(norm.z), input.worldpos.z);

	float shadowfactor = decideOnCascade(input.shadowpos);
	float4 diffuse = max(shadowfactor, light.amb) * light.dif * dot(-light.dir, norm);
	float3 V = reflect(light.dir, norm);
	float3 toEye = normalize(eye.xyz - input.worldpos);
	float4 specular = shadowfactor * 0.1f * light.spec * pow(max(dot(V, toEye), 0.0f), 2.0f);
	
	return (diffuse + specular) * color;
}