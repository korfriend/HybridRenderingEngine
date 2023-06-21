#include "../Sr_Common.hlsl"

// [earlydepthstencil] ==> shader model 5
PS_FILL_OUTPUT BasicShader4(__VS_OUT input)
{
	PS_FILL_OUTPUT out_ps;
	out_ps.ds_z = 1.f; // remove???
	out_ps.color = (float4)0;
	out_ps.depthcs = FLT_MAX;

	float4 v_rgba = (float4)0;
	float z_depth = FLT_MAX;

	BasicShader(input, v_rgba, z_depth);

	out_ps.ds_z = input.f4PosSS.z;
	out_ps.color = v_rgba;
	out_ps.depthcs = z_depth;

	return out_ps;
}

// https://stackoverflow.com/questions/39404502/direct11-write-data-to-buffer-in-pixel-shader-like-ssbo-in-open
// [earlydepthstencil] ==> shader model 5

[maxvertexcount(1)]
void GS_PickingPoint(triangle VS_OUTPUT input[3], inout PointStream<VS_OUTPUT> pointStream)
{
	float3 pos0 = input[0].f3PosWS;
	float3 pos1 = input[1].f3PosWS;
	float3 pos2 = input[2].f3PosWS;


	// test ...different type of structure... (pos, intersection info, id, ...)
	// test ... configurable from different models??
	pointStream.Append(input[0]);
	// get intersection //
}