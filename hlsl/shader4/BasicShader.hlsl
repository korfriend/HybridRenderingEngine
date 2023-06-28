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

#if __RENDERING_MODE != 6
	BasicShader(input, v_rgba, z_depth);
#else
	int2 tex2d_xy = int2(input.f4PosSS.xy);
	z_depth = sr_fragment_zdepth[tex2d_xy];
	int outlinePPack = g_cbCamState.iSrCamDummy__1;
	float3 outline_color = float3(((outlinePPack >> 16) & 0xFF) / 255.f, ((outlinePPack >> 8) & 0xFF) / 255.f, (outlinePPack & 0xFF) / 255.f);
	int pixThickness = (outlinePPack >> 24) & 0xFF;
	v_rgba = OutlineTest(tex2d_xy, z_depth, 10000.f, outline_color, pixThickness);
	//v_rgba = float4(tex2d_xy / 1000.f, 0, 1);
	//v_rgba = float4(1, 0, 0, 1);
	//z_depth = 10.f;
	if (v_rgba.a <= 0.01) clip(-1);
#endif

	out_ps.ds_z = input.f4PosSS.z;
	out_ps.color = v_rgba;
	out_ps.depthcs = z_depth;

	return out_ps;
}

PS_FILL_OUTPUT OutlineShader(__VS_OUT input)
{

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