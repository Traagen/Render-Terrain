float4 main(float3 pos : POSITION) : SV_POSITION
{
	// just pass vertex position straight through
	return float4(pos, 1.0f);
}