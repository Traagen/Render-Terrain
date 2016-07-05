cbuffer ConstantBuffer : register(b0)
{
	float4x4 viewproj;
	float4 eye;
	int height;
	int width;
}

Texture2D<float4> heightmap : register(t0);
SamplerState hmsampler : register(s0);

struct VS_OUTPUT
{
	float4 pos : SV_POSITION;
	float4 worldpos : POSITION;
    float4 norm : NORMAL;
    float4 tex : TEXCOORD;
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

// basic diffuse/ambient lighting
float4 main(VS_OUTPUT input) : SV_TARGET
{
//    return heightmap.Sample(hmsampler, input.tex);
    float4 light = normalize(float4(1.0f, 1.0f, -1.0f, 1.0f));
    float diffuse = saturate(dot(input.norm, -light));
    float ambient = 0.2f;
    float3 color = float3(0.32f, 0.82f, 0.41f);
	
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
  
    return float4(saturate((color * diffuse) + (color * ambient)), 1.0f);
}