#include "../Sr_Common.hlsl"

RWTexture2D<uint> fragment_counter : register(u2);
RWByteAddressBuffer deep_DxA_buf : register(u4);

#define STORE1_RBB(V, ADDR) deep_DxA_buf.Store((ADDR) * 4, V)

void OIT_A_BUFFER_CNF_FRAGS(__VS_OUT input)
{
	if (g_cbClipInfo.clip_flag & 0x1)
		clip(dot(g_cbClipInfo.vec_clipplane, input.f3PosWS - g_cbClipInfo.pos_clipplane) > 0 ? -1 : 1);
	if (g_cbClipInfo.clip_flag & 0x2)
		clip(!IsInsideClipBox(input.f3PosWS, g_cbClipInfo.pos_clipbox_max_bs, g_cbClipInfo.mat_clipbox_ws2bs) ? -1 : 1);

	float3 pos_ip_ss = float3(input.f4PosSS.xy, 0.0f);
	float3 pos_ip_ws = TransformPoint(pos_ip_ss, g_cbCamState.mat_ss2ws);
	float3 vec_pos_ip2frag = input.f3PosWS - pos_ip_ws;
	clip(dot(vec_pos_ip2frag, g_cbCamState.dir_view_ws) < 0 ? -1 : 1);

	int2 tex2d_xy = int2(input.f4PosSS.xy);
	InterlockedAdd(fragment_counter[tex2d_xy.xy], 1);
}

Buffer<uint> sr_offsettable_buf : register(t50);

void OIT_A_BUFFER_FILL(__VS_OUT input)
{
	POBJ_PRE_CONTEXT;

	float4 v_rgba = float4(g_cbPobj.Kd, g_cbPobj.alpha);
	float3 nor = (float3) 0;
	float nor_len = 0;

#if __RENDERING_MODE != 1 && __RENDERING_MODE != 2 && __RENDERING_MODE != 3
	nor = input.f3VecNormalWS;
	nor_len = length(nor);
#endif

#if __RENDERING_MODE == 1
	DashedLine(v_rgba, z_depth, input.f3Custom.x, g_cbPobj.dash_interval, g_cbPobj.pobj_flag & (0x1 << 19), g_cbPobj.pobj_flag & (0x1 << 20));
#elif __RENDERING_MODE == 2
	MultiTextMapping(v_rgba, z_depth, input.f3Custom0.xy, (int)(input.f3Custom0.z + 0.5f), input.f3Custom1, input.f3Custom2);
#elif __RENDERING_MODE == 3
	TextMapping(v_rgba, z_depth, input.f3Custom.xy, g_cbPobj.pobj_flag & (0x1 << 9), g_cbPobj.pobj_flag & (0x1 << 10));
#elif __RENDERING_MODE == 4
	if (g_cbPobj.tex_map_enum == 1)
	{
		float4 clr_map;
		TextureImgMap(clr_map, input.f3Custom);
		if (clr_map.a == 0)
		{
			v_rgba = (float4) 0;
			z_depth = FLT_MAX;
			clip(-1);
		}
		else
		{
			//float3 mat_shading = float3(g_cbEnv.ltint_ambient.w, g_cbEnv.ltint_diffuse.w, g_cbEnv.ltint_spec.w);
			float3 Ka = clr_map.rgb * g_cbEnv.ltint_ambient.rgb;
			float3 Kd = clr_map.rgb * g_cbEnv.ltint_diffuse.rgb;
			float3 Ks = clr_map.rgb * g_cbEnv.ltint_spec.rgb;
			float Ns = g_cbPobj.Ns;
			ComputeColor(v_rgba.rgb, Ka, Kd, Ks, Ns, 1.0, input.f3PosWS, view_dir, nor, nor_len);
			v_rgba.a *= clr_map.a;
		}
		v_rgba.rgb = float3(1, 0, 0);
	}
	else
	{
		float3 Ka = g_cbPobj.Ka, Kd = g_cbPobj.Kd, Ks = g_cbPobj.Ks;
		float Ns = g_cbPobj.Ns, d = 1.0, bump = 1.0;
		TextureMaterialMap(Ka, Kd, Ks, Ns, bump, d, input.f3Custom, g_cbPobj.tex_map_enum);
		if (Ns >= 0)
		{
			Ka *= g_cbEnv.ltint_ambient.rgb;
			Kd *= g_cbEnv.ltint_diffuse.rgb;
			Ks *= g_cbEnv.ltint_spec.rgb;
			ComputeColor(v_rgba.rgb, Ka, Kd, Ks, Ns, bump, input.f3PosWS, view_dir, nor, nor_len);
			//v_rgba.rgb = Ks;
		}
		else // illumination model 0
			v_rgba.rgb = Kd;
		v_rgba.a *= d;
	}
#else
	float3 Ka, Kd, Ks;
	float Ns = g_cbPobj.Ns;
	if ((g_cbPobj.pobj_flag & (0x1 << 3)) == 0)
	{
		Ka = input.f3Custom;
		Kd = input.f3Custom;
		Ks = input.f3Custom;
	}
	else
	{
		Ka = g_cbPobj.Ka, Kd = g_cbPobj.Kd, Ks = g_cbPobj.Ks;
	}
	if (nor_len > 0)
	{
		Ka *= g_cbEnv.ltint_ambient.rgb;
		Kd *= g_cbEnv.ltint_diffuse.rgb;
		Ks *= g_cbEnv.ltint_spec.rgb;
		ComputeColor(v_rgba.rgb, Ka, Kd, Ks, Ns, 1.0, input.f3PosWS, view_dir, nor, nor_len);
	}
	else
		v_rgba.rgb = Kd;
#endif

	// make it as an associated color.
	// as a color component is stored into 8 bit channel, the alpha-multiplied precision must be determined in this stage.
	// unless, noise dots appear.
	v_rgba.rgb *= v_rgba.a;

	int2 tex2d_xy = int2(input.f4PosSS.xy);

	// Atomically allocate space in the deep buffer
	uint fc = 0;
	InterlockedAdd(fragment_counter[tex2d_xy], 1, fc);

	uint offsettable_idx = tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x;
	uint nDeepBufferPos = 0;
#if DX_11_STYLE == 1
	if (offsettable_idx == 0)
		nDeepBufferPos = fc;
	else
		nDeepBufferPos = sr_offsettable_buf[offsettable_idx - 1] + fc;
#else
	if (offsettable_idx == 0) clip(-1);
	else nDeepBufferPos = sr_offsettable_buf[offsettable_idx] + fc;
#endif

	// Store fragment data into the allocated space
	//deep_DxA_buf[2 * nDeepBufferPos + 0] = ConvertFloat4ToUInt(v_rgba);
	STORE1_RBB(ConvertFloat4ToUInt(v_rgba), 2 * nDeepBufferPos + 0);
	//deep_DxA_buf[2 * nDeepBufferPos + 1] = asuint(z_depth);
	STORE1_RBB(asuint(z_depth), 2 * nDeepBufferPos + 1);
}