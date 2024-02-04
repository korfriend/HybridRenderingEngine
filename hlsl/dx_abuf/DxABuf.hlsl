#include "../Sr_Common.hlsl"

RWTexture2D<uint> fragment_counter : register(u2);
RWByteAddressBuffer deep_ubk_buf : register(u4);
RWBuffer<uint> picking_buf : register(u5);

Buffer<uint> sr_offsettable_buf : register(t50);

#define STORE1_RBB(V, ADDR) deep_ubk_buf.Store((ADDR) * 4, V)

[earlydepthstencil]
void OIT_A_BUFFER_CNF_FRAGS(__VS_OUT input)
{
	if (g_cbClipInfo.clip_flag & 0x1)
		clip(dot(g_cbClipInfo.vec_clipplane, input.f3PosWS - g_cbClipInfo.pos_clipplane) > 0 ? -1 : 1);
	if (g_cbClipInfo.clip_flag & 0x2)
		clip(!IsInsideClipBox(input.f3PosWS, g_cbClipInfo.mat_clipbox_ws2bs) ? -1 : 1);

	float3 pos_ip_ss = float3(input.f4PosSS.xy, 0.0f);
	float3 pos_ip_ws = TransformPoint(pos_ip_ss, g_cbCamState.mat_ss2ws);
	float3 vec_pos_ip2frag = input.f3PosWS - pos_ip_ws;
	clip(dot(vec_pos_ip2frag, g_cbCamState.dir_view_ws) < 0 ? -1 : 1);

	int2 tex2d_xy = int2(input.f4PosSS.xy);
	InterlockedAdd(fragment_counter[tex2d_xy.xy], 1);
}

[earlydepthstencil]
void OIT_A_BUFFER_FILL(__VS_OUT input)
{
	float4 v_rgba = (float4)0;
	float z_depth = FLT_MAX;
	int2 tex2d_xy = int2(input.f4PosSS.xy);

	if (tex2d_xy.x + tex2d_xy.y == 0) clip(-1);

	BasicShader(input, v_rgba, z_depth);

	// Atomically allocate space in the deep buffer
	uint fc = 0;
	InterlockedAdd(fragment_counter[tex2d_xy], 1, fc);

	uint offsettable_idx = tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x;
	uint addrBase = 0;
#if DX_11_STYLE == 1
	if (offsettable_idx == 0)
		addrBase = fc;
	else
		addrBase = sr_offsettable_buf[offsettable_idx - 1] + fc;
#else
	if (offsettable_idx == 0) clip(-1);
	else addrBase = sr_offsettable_buf[offsettable_idx] + fc;
#endif

	// Store fragment data into the allocated space
	//deep_ubk_buf[2 * addrBase + 0] = ConvertFloat4ToUInt(v_rgba);
	//deep_ubk_buf[2 * addrBase + 1] = asuint(z_depth);
	STORE1_RBB(ConvertFloat4ToUInt(v_rgba), 2 * addrBase + 0);
	STORE1_RBB(asuint(z_depth), 2 * addrBase + 1);
}

[earlydepthstencil]
void PICKING_A_BUFFER_FILL(__VS_OUT input)
{
	//POBJ_PRE_CONTEXT;
	if (g_cbPobj.alpha < 0.00001) clip(-1);
	int2 tex2d_xy = int2(input.f4PosSS.xy);
	int x_pick_ss = g_cbCamState.iSrCamDummy__1 & 0xFFFF;
	int y_pick_ss = g_cbCamState.iSrCamDummy__1 >> 16;
	if(x_pick_ss != tex2d_xy.x || y_pick_ss != tex2d_xy.y) clip(-1);

	//POBJ_PRE_CONTEXT;
	if (g_cbClipInfo.clip_flag & 0x1)
		clip(dot(g_cbClipInfo.vec_clipplane, input.f3PosWS - g_cbClipInfo.pos_clipplane) > 0 ? -1 : 1);
	if (g_cbClipInfo.clip_flag & 0x2)
		clip(!IsInsideClipBox(input.f3PosWS, g_cbClipInfo.mat_clipbox_ws2bs) ? -1 : 1);
	clip(input.f4PosSS.z);
	clip(input.f4PosSS.z / input.f4PosSS.w);
	float3 pos_ip_ss = float3(input.f4PosSS.xy, 0.0f);
	float3 pos_ip_ws = TransformPoint(pos_ip_ss, g_cbCamState.mat_ss2ws);
	float3 vec_pos_ip2frag = input.f3PosWS - pos_ip_ws;
	float z_depth = length(vec_pos_ip2frag) /*- g_cbPobj.depth_forward_bias*/;
	clip(dot(vec_pos_ip2frag, g_cbCamState.dir_view_ws) < 0 ? -1 : 1);
	float3 view_dir = g_cbCamState.dir_view_ws;
	if (g_cbCamState.cam_flag & 0x1) {
		view_dir = pos_ip_ws - g_cbCamState.pos_cam_ws;
		z_depth += length(view_dir);
	}
	else {
		z_depth += g_cbCamState.near_plane;
	}
	view_dir = normalize(view_dir);



	// Atomically allocate space in the deep buffer
	uint fc = 0;
	InterlockedAdd(fragment_counter[tex2d_xy], 1, fc);

	//picking_buf
	picking_buf[2 * fc + 0] = g_cbPobj.pobj_dummy_0;
	picking_buf[2 * fc + 1] = asuint(z_depth);
}