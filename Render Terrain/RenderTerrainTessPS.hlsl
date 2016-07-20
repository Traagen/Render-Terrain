cbuffer ConstantBuffer : register(b0)
{
	float4x4 viewproj;
	float4 eye;
	float4 frustum[6];
	float scale;
	int height;
	int width;
}

Texture2D<float4> heightmap : register(t0);
SamplerState hmsampler : register(s0);
SamplerState detailsampler : register(s2);

struct VS_OUTPUT
{
	float4 pos : SV_POSITION;
	float3 worldpos : POSITION;
  //  float3 norm : NORMAL;
    float2 tex : TEXCOORD;
};

/*
// Phone lighting model
struct Material
{
	float Ka, Kd, Ks, A;
};

//--------------------------------------------------------------------------------------
// Phong Lighting Reflection Model
//--------------------------------------------------------------------------------------
float4 calcPhongLighting(Material M, float4 LColor, float3 N, float3 L, float3 V, float3 R)
{
	float4 ambientLight = float4(0.32f, 0.82f, 0.41f, 1.0f);
	float4 Ia = M.Ka * ambientLight;
	float4 Id = M.Kd * saturate(dot(N, L));
	float4 Is = M.Ks * pow(saturate(dot(R, V)), M.A);

	//return saturate(Ia + (Id + Is) * LColor);
	return Ia + (Id + Is) * LColor;
}

float4 main(VS_OUTPUT input) : SV_Target
{
	// temporary light source.
	float4 light = normalize(float4(1.0f, 1.0f, -1.0f, 1.0f));
	float4 lightcolor = float4(0.32f, 0.82f, 0.41f, 1.0f);

	//calculate lighting vectors - renormalize vectors
	input.norm = normalize(input.norm);
	float3 V = normalize(eye - (float3) input.worldpos);
	//DONOT USE -light.dir since the reflection returns a ray from the surface
	float3 R = reflect(light, input.norm);

	Material mat;
	mat.Ka = 0.1f;
	mat.Kd = 0.8f;
	mat.Ks = 0.3f;
	mat.A = 0.1f;
	//calculate lighting
	return calcPhongLighting(mat, lightcolor, input.norm, -light, V, R);
}
*/

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
	float3 map = 2.0f * heightmap.Sample(detailsampler, texcoord).yzw - 1.0f;
	map.z *= 5.0f;
	float3x3 TBN = cotangent_frame(N, -V, texcoord);
	return normalize(mul(map, TBN));
}

float3 estimateNormal(float2 texcoord) {
	/*float2 leftTex = texcoord + float2(-1.0f / (float)width, 0.0f);
	float2 rightTex = texcoord + float2(1.0f / (float)width, 0.0f);
	float2 bottomTex = texcoord + float2(0.0f, 1.0f / (float)height);
	float2 topTex = texcoord + float2(0.0f, -1.0f / (float)height);

	float leftZ = heightmap.SampleLevel(hmsampler, leftTex, 0).r * scale;
	float rightZ = heightmap.SampleLevel(hmsampler, rightTex, 0).r * scale;
	float bottomZ = heightmap.SampleLevel(hmsampler, bottomTex, 0).r * scale;
	float topZ = heightmap.SampleLevel(hmsampler, topTex, 0).r * scale;

	return normalize(float3(leftZ - rightZ, topZ - bottomZ, 1.0f));*/

	float2 b = texcoord + float2(0.0f, -0.3f / (float)height);
	float2 c = texcoord + float2(0.3f / (float)width, -0.3f / (float)height);
	float2 d = texcoord + float2(0.3f / (float)width, 0.0f);
	float2 e = texcoord + float2(0.3f / (float)width, 0.3f / (float)height);
	float2 f = texcoord + float2(0.0f, 0.3f / (float)height);
	float2 g = texcoord + float2(-0.3f / (float)width, 0.3f / (float)height);
	float2 h = texcoord + float2(-0.3f / (float)width, 0.0f);
	float2 i = texcoord + float2(-0.3f / (float)width, -0.3f / (float)height);

	float zb = heightmap.SampleLevel(hmsampler, b, 0).r * scale;
	float zc = heightmap.SampleLevel(hmsampler, c, 0).r * scale;
	float zd = heightmap.SampleLevel(hmsampler, d, 0).r * scale;
	float ze = heightmap.SampleLevel(hmsampler, e, 0).r * scale;
	float zf = heightmap.SampleLevel(hmsampler, f, 0).r * scale;
	float zg = heightmap.SampleLevel(hmsampler, g, 0).r * scale;
	float zh = heightmap.SampleLevel(hmsampler, h, 0).r * scale;
	float zi = heightmap.SampleLevel(hmsampler, i, 0).r * scale;

	float x = zg + 2 * zh + zi - zc - 2 * zd - ze;
	float y = 2 * zb + zc + zi - ze - 2 * zf - zg;
	float z = 8.0f;

	return normalize(float3(x, y, z));
}

// basic diffuse/ambient lighting
float4 main(VS_OUTPUT input) : SV_TARGET
{
//    return heightmap.Sample(hmsampler, input.tex);
    float4 light = normalize(float4(1.0f, 1.0f, -2.0f, 1.0f));

	//float3 N = normalize(input.norm);
	//float3 viewvector = eye.xyz - input.worldpos;
	//N = perturb_normal(N, viewvector, input.tex);
	//float4 norm = float4(N, 1.0f);
	float3 norm = estimateNormal(input.tex);
//	float3 viewvector = eye.xyz - input.worldpos;
//	norm = perturb_normal(norm, viewvector, input.tex * 256.0f);

	float4 N = float4(norm, 1.0f);
	//float4 norm = float4(normalize(input.norm), 1.0f);
    float diffuse = saturate(dot(N, -light));
    float ambient = 0.1f;
    float3 color = float3(0.22f, 0.72f, 0.31f);
	//float3 color = float3(0.48f, 0.2f, 0.09f);
	
    /*float slope = 1.0f - input.norm.z;
	float scale = input.tex.w / 256.0f;


    if (input.tex.z == 0.0f) // sea level
    {
        color = float3(0.04f, 0.36f, 0.96f);
	}
	else if (input.tex.z < 20 * scale) {
		color = lerp(float3(1.0f, 0.92f, 0.8f), float3(0.32f, 0.82f, 0.41f), input.tex.z / (20 * scale));
	}
	else if (input.tex.z < 180 * scale) {
		if (slope <= 0.2f)
		{
			color = float3(0.32f, 0.82f, 0.41f); // deep green;
			//color = float3(0.0f, 1.0f, 0.0f);
		}
		else if (slope > 0.2f && slope < 0.6f)
		{
			color = float3(0.46f, 0.74f, 0.56f); // lemongrass;
			//color = float3(0.0f, 0.0f, 1.0f);
		}
		else if (slope >= 0.6f)
		{
			color = float3(0.38f, 0.41f, 0.42f); // basalt grey
			//  color = float3(1.0f, 0.0f, 0.0f);
		}
	}
	else if (input.tex.z < 200 * scale) {
		color = lerp(float3(0.46f, 0.74f, 0.56f), float3(0.38f, 0.41f, 0.42f), (input.tex.z - 180) / (20 * scale));
	}
	else if (input.tex.b < 220 * scale) {
		color = lerp(float3(0.38f, 0.41f, 0.42f), float3(1.0f, 1.0f, 1.0f), (input.tex.z - 200) / (20 * scale));
	}
	else {
		color = float3(1.0f, 1.0f, 1.0f);
	}
	*/
//	return float4(color, 1.0f);
    return float4(saturate((color * diffuse) + (color * ambient)), 1.0f);
}