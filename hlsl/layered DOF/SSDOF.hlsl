#include "../CommonShader.hlsl"

#define LOCAL_SIZE 8

Texture2D<uint> fragment_counter : register(t10);
ByteAddressBuffer deep_k_buf : register(t11);

RWTexture2D<unorm float4> rw_fragment_blendout : register(u10);

Texture2D<float2> z_minmax_texture : register(t15);
Buffer<uint> global_z_minmax_buffer : register(t16);
RWTexture2D<float2> rw_z_minmax_texture : register(u15);
RWBuffer<uint> rw_global_z_minmax_buffer : register(u16);

void DisplayRect(int x, int y, float4 color)
{
	for (int xx = 0; xx < 3; xx++)
		for (int yy = 0; yy < 3; yy++)
			rw_fragment_blendout[int2(x, y) + int2(xx, yy)] = color;
}

#if !defined(FRAG_MERGING) || FRAG_MERGING == 1
#define LOAD1_KBUF_VIS(KBUF, F_ADDR, K) KBUF.Load(F_ADDR + (K) * 4 * 4)
#define STORE1_KBUF_VIS(V, KBUF, F_ADDR, K) KBUF.Store(F_ADDR + (K) * 4 * 4, V)
#define LOAD1_KBUF_Z(KBUF, F_ADDR, K) asfloat(KBUF.Load(F_ADDR + ((K) * 4 + 1) * 4))
#define LOAD1_KBUF_ALPHA(KBUF, F_ADDR, K) (LOAD1_KBUF_VIS(KBUF, F_ADDR, K) >> 24)
#define LOAD1_KBUF_ALPHAF(KBUF, F_ADDR, K) (LOAD1_KBUF_ALPHA(KBUF, F_ADDR, K) / 255.f)
#endif

#define XOR_HASH(x, y) (((30*x)&y) + 10*x*y)
#define PACK_1TOFF(x) (((int)(x * 255.f)) & 0xFF)
#define PACK_1TOFF_CH8b(x, c) (PACK_1TOFF(x) << (c*8))

// assume camera space
// right handed, view dir is -z
// ray_v is a unit vector
// herein, z_coord and z are different: z_coord is a minus value, z is a distance (positive) value.
void GetP_on_ZPlaneCS(out float3 p_hit, float z_coord, float3 pos_s, float3 ray_v)
{
	// http://www.gisdeveloper.co.kr/?p=792
	float t = (z_coord - pos_s.z) / ray_v.z;
	p_hit = pos_s + ray_v * t;
}

//#define GetFragHitRadius(Z, ZN, RN) (Z) / (ZN) * (RN)
float GetFragHitRadius(float z, float ratio_rn_zn)
{
	// ratio_rn_zn = rn / zn
	return z * ratio_rn_zn;
}

float3 ComputePos_SSZ2CS(float2 xy_ss, float ray_dist_from_ip)
{
	// note that column major
	// g_cbCamState.mat_ss2ws is used as ss2cs
	float3 pos_ip_cs = TransformPoint(float3(xy_ss, 0), g_cbCamState.mat_ss2ws);
	float3 ray_dir_cs = float3(0, 0, -1);
	if (g_cbCamState.cam_flag & 0x1)
		ray_dir_cs = pos_ip_cs;// -float3(0, 0, 0);
	ray_dir_cs = normalize(ray_dir_cs);
	return pos_ip_cs + ray_dir_cs * ray_dist_from_ip;
}

#if ZT_MODEL == 1
float RayHitFragZ(float3 pos_s, float3 ray_v, Fragment f, float2 xy_ss, float ratio_rn_zn)
{
	// to do
	// merge check //
	// 3 interval checks, refer to HorizonOcclusion //
	float3 pos_f = ComputePos_SSZ2CS(xy_ss, f.z);
	float frag_radius = GetFragHitRadius(-pos_f.z, ratio_rn_zn);
	float3 p_hit;
	GetP_on_ZPlaneCS(p_hit, pos_f.z, pos_s, ray_v);
	float diff = p_hit - pos_f;
	float r_sq = dot(diff, diff);
	return r_sq < frag_radius*frag_radius ? -pos_f.z : 0;
}
#else
float RayHitFragZ(float3 pos_s, float3 ray_v, float ray_dist_from_ip, float2 xy_ss, float ratio_rn_zn)
{
	float3 pos_f = ComputePos_SSZ2CS(xy_ss, ray_dist_from_ip);
	float frag_radius = GetFragHitRadius(-pos_f.z, ratio_rn_zn);
	float3 p_hit;
	GetP_on_ZPlaneCS(p_hit, pos_f.z, pos_s, ray_v);
	float3 diff = p_hit - pos_f;
	float r_sq = dot(diff, diff);
	return r_sq < frag_radius*frag_radius ? -pos_f.z : 0;
}
#endif

float3x3 AngleAxis3x3(float angle, float3 axis)
{
	float c, s;
	sincos(angle, s, c);

	float t = 1 - c;
	float x = axis.x;
	float y = axis.y;
	float z = axis.z;

	return float3x3(
		t * x * x + c, t * x * y - s * z, t * x * z + s * y,
		t * x * y + s * z, t * y * y + c, t * y * z - s * x,
		t * x * z - s * y, t * y * z + s * x, t * z * z + c
		);
}

[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void KB_SSDOF_RT(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	if (DTid.x >= g_cbCamState.rt_width || DTid.y >= g_cbCamState.rt_height)
		return;
	
	// at this moment, only static k buffer is supported.
	const uint k_value = g_cbCamState.k_value;
	uint bytes_per_frag = 4 * NUM_ELES_PER_FRAG;
	uint bytes_frags_per_pixel = k_value * bytes_per_frag; // to do : consider the dynamic scheme. (4 bytes unit)
	uint pixel_id = DTid.y * g_cbCamState.rt_width + DTid.x;
	uint addr_base_focalray = pixel_id * bytes_frags_per_pixel;

	int frag_cnt = (int)fragment_counter[DTid.xy];
	uint vr_hit_enc = frag_cnt >> 24;
	frag_cnt = frag_cnt & 0xFFF;

	//if (frag_cnt == 0 && vr_hit_enc == 0)
	//{
	//	rw_fragment_blendout[DTid.xy] = (float4)0;
	//	return;
	//}
	
	float3 pos_ip_ss = float3(DTid.xy, 0.0f);
	// g_cbCamState.mat_ss2ws is used as ss2cs
	float3 pos_ip_cs = TransformPoint(pos_ip_ss, g_cbCamState.mat_ss2ws);
	float3 ray_dir_cs = float3(0, 0, -1);
	if (g_cbCamState.cam_flag & 0x1)
		ray_dir_cs = pos_ip_cs;
	ray_dir_cs = normalize(ray_dir_cs);

	float3 pos_focus;
	GetP_on_ZPlaneCS(pos_focus, -g_cbEnv.dof_focus_z, (float3)0, ray_dir_cs);

	float3 ipO_cs = TransformPoint(float3(g_cbCamState.rt_width / 2.f, g_cbCamState.rt_height / 2.f, 0), g_cbCamState.mat_ss2ws);
	float3 ipx_cs = TransformPoint(float3(g_cbCamState.rt_width / 2.f + 1.f, g_cbCamState.rt_height / 2.f, 0), g_cbCamState.mat_ss2ws);
	float3 pix_cs = ipx_cs - ipO_cs;
	float pix_r = length(pix_cs) * 0.5 * 1.2; // SCALE for safe hit
	float ratio_rn_zn = pix_r / g_cbCamState.near_plane;
	// g_cbCamState.near_plane is Focal Length?!
	float dof_lens_F = g_cbCamState.near_plane; // to do
	float lens_const = g_cbEnv.dof_lens_r * dof_lens_F / (g_cbEnv.dof_focus_z - dof_lens_F);

#define ITERATION_RAY_HIT_TEST 3
#define COMPUTE_LENSRAY_STEP(HALF) {\
	GetP_on_ZPlaneCS(p_min, -z_min, pos_lens, lens_ray_v);\
	GetP_on_ZPlaneCS(p_max, -z_max, pos_lens, lens_ray_v);\
	/* update z_min, z_max */\
	/* g_cbCamState.mat_ws2ss is used as cs2ss */\
	p_min_ss = TransformPoint(p_min, g_cbCamState.mat_ws2ss);\
	p_max_ss = TransformPoint(p_max, g_cbCamState.mat_ws2ss);\
	diff_ss = (p_max_ss - p_min_ss).xy;\
	dist_ss = length(diff_ss);\
	v_ss = diff_ss / (dist_ss + 0.00001);\
	count_steps = max(dist_ss + 0.5, 1);\
	p_addr = uint2(p_min_ss.xy) / HALF; }

#define UPDATE_LENSRAY_ADDR(RAY_STEP, HALF) {\
	p_min_ss.xy += v_ss;\
	uint2 p_addr_new = uint2(p_min_ss.xy) / HALF;\
	/*if (p_addr.x == p_addr_new.x && p_addr.y == p_addr_new.y)*/\
	{\
		/*RAY_STEP++;*/\
		/*p_min_ss.xy += v_ss;*/\
		/*p_addr_new = uint2(p_min_ss.xy) / HALF;*/\
	}\
	p_addr = p_addr_new; }

	float z_min = asfloat(global_z_minmax_buffer[0]);
	float z_max = asfloat(global_z_minmax_buffer[1]);
	//if(z_min >= g_cbCamState.near_plane && z_max <= g_cbCamState.far_plane)
	//	rw_fragment_blendout[DTid.xy] = float4(1, 1, 0, 1);
	//return;
	float4 acc_lens_vis = (float4)0;
	int num_hit_rays = 0;
	int _test_cnt_no_hit = 0;
	[loop]
	for (int iray = 0; iray < g_cbEnv.dof_lens_ray_num_samples; iray++)
	{
		// random pos_lens, lens_ray_v
		// g_cbEnv.dof_lens_r
		int hash_idx = DTid.x + g_cbCamState.rt_width * DTid.y + iray * g_cbCamState.rt_width * g_cbCamState.rt_height;
		float3 random_sample = float3(
			_random(hash_idx),
			_random(hash_idx + g_cbCamState.rt_width * g_cbCamState.rt_height * ITERATION_RAY_HIT_TEST), 0);//
			//_random(hash_idx + g_cbCamState.rt_width * g_cbCamState.rt_height * ITERATION_RAY_HIT_TEST * 2));

		//float3 random_sample = mul(AngleAxis3x3(F_PI / g_cbEnv.dof_lens_ray_num_samples * iray, float3(0, 0, -1)), float3(0.5, 0, 0));

		float3 pos_lens = (random_sample - float3(0.5, 0.5, 0)) * 2.0 * g_cbEnv.dof_lens_r;
		float3 lens_ray_v = normalize(pos_focus - pos_lens);

		float3 p_min, p_max, p_min_ss, p_max_ss;
		float2 diff_ss, v_ss;
		float dist_ss; int count_steps; uint2 p_addr;
		bool is_candidate_ray = false;
		for (int i = 0; i < ITERATION_RAY_HIT_TEST; i++)
		{
			COMPUTE_LENSRAY_STEP(1); // half option // p_min_ss, p_max_ss, p_addr, count_steps

			for (int j = 0; j < count_steps; j++)
			{
				float2 z_minmax = z_minmax_texture[p_addr];
				//if (z_minmax.x == 0)
				//{
				//	i = ITERATION_RAY_HIT_TEST;
				//	count_steps = -1;
				//	break;
				//}
				//z_min = min(z_min, z_minmax.x);
				//z_max = min(z_max, z_minmax.y);
				if (z_minmax.x > 0)
				{
					z_min = min(z_min, z_minmax.x);
					z_max = min(z_max, z_minmax.y);
					is_candidate_ray = true;
				}

				UPDATE_LENSRAY_ADDR(j, 1); // half option // p_addr
			}
		}

		if (!is_candidate_ray) continue;

		// Compute hit-fragments with updated z_min and z_max
		COMPUTE_LENSRAY_STEP(1); // p_min_ss, p_max_ss, p_addr, count_steps
		//rw_fragment_blendout[DTid.xy] = float4((float3)count_steps / 100.f, 1); return;
		//rw_fragment_blendout[DTid.xy] = float4((float3)z_min / 300.f, 1);
		//rw_fragment_blendout[DTid.xy] = float4((float3)count_steps / 100.f, 1);
		//return;
		// "count_steps" means # of samples along the ray-traveral footprint
		float4 acc_ray_vis = (float4)0;
#define MAX_LAYERS 8
		float3 prev_step_pos_frags[MAX_LAYERS] = {(float3)0, (float3)0, (float3)0, (float3)0, (float3)0, (float3)0, (float3)0, (float3)0}; // assume that k_value <= MAX_LAYERS
		uint prev_step_ivis[MAX_LAYERS] = { 0, 0, 0, 0, 0, 0, 0, 0 };
#if ZT_MODEL == 1
		float prev_step_zthick_frags[MAX_LAYERS] = { 0, 0, 0, 0, 0, 0, 0, 0 };
#endif
		for (int j = 0; j <= count_steps; j++)
		{
			int frag_cnt_at_footprint_pix = min((int)fragment_counter[p_addr], 1); // test
			//int frag_cnt_at_footprint_pix = (int)fragment_counter[p_addr];
			uint addr_base_at_footprint_pix = (p_addr.y * g_cbCamState.rt_width + p_addr.x) * bytes_frags_per_pixel;
			//rw_fragment_blendout[DTid.xy] = float4((float3)frag_cnt_at_footprint_pix / 8, 1); return;
			for (int k = 0; k < frag_cnt_at_footprint_pix; k++)
			{
#if ZT_MODEL == 1
				Fragment f;
				GET_FRAG(f, addr_base_at_footprint_pix, k);
				float ray_dist_from_ip = f.z;
				uint i_vis = f.i_vis;
#else
				float ray_dist_from_ip = LOAD1_KBUF_Z(deep_k_buf, addr_base_at_footprint_pix, k);
				uint i_vis = LOAD1_KBUF_VIS(deep_k_buf, addr_base_at_footprint_pix, k);
#endif
				float3 pos_f = ComputePos_SSZ2CS(p_addr, ray_dist_from_ip);
				// hit test //
				//if (!is_hit)
				{
					// compute a pos at center z along the lens ray 
					uint prev_ivis = prev_step_ivis[k];
					float hit_z = 0;
					if (prev_ivis == 0 || dist_ss == 0)
					{
						prev_ivis = i_vis;
						float frag_radius = GetFragHitRadius(-pos_f.z, ratio_rn_zn);
						float t = (pos_f.z - pos_lens.z) / lens_ray_v.z;
						float3 pos_lensray_at_z = pos_lens + lens_ray_v * t;
						if (dot(pos_f - pos_lensray_at_z, pos_f - pos_lensray_at_z) <= frag_radius * frag_radius) hit_z = -pos_f.z;
					}
					else
					{
						float3 prev_pos_f = prev_step_pos_frags[k];
						float z_mid = (pos_f.z + prev_pos_f.z) * 0.5;
						float t = (z_mid - pos_lens.z) / lens_ray_v.z;
						float3 pos_lensray_at_midz = pos_lens + lens_ray_v * t;
						//float frag_radius = GetFragHitRadius(-pos_f.z, ratio_rn_zn);
						if (dot(pos_f - pos_lensray_at_midz, prev_pos_f - pos_lensray_at_midz) <= 0) hit_z = -pos_lensray_at_midz.z;
					}
					if (hit_z > 0)
					{
						// note that lens ray hits only one fragment amongst the fragments that are located along the viewing ray.
						// "The intervals are still narrowed down quickly because lens rays are almost perpendicular to the image plane."
						float coc_r = abs(hit_z - g_cbEnv.dof_focus_z) / hit_z * lens_const;
						float coc_r_ip = pix_r * hit_z / g_cbCamState.near_plane * coc_r;
						float blur_w = 1.f;
						if (coc_r_ip > pix_r)
						{
							// use DOB setting (degree of blurring, refer to "Real-Time Depth-of-Field Rendering Using Anisotropically Filtered Mipmap Interpolation")
							float sigma = coc_r_ip / 2.0;
							blur_w = exp(-0.5* (coc_r_ip) * (coc_r_ip) / (sigma*sigma));
						}
						float4 f_vis_prev = ConvertUIntToFloat4(prev_ivis);
						float4 f_vis = ConvertUIntToFloat4(i_vis);
						f_vis = (f_vis + f_vis_prev) * 0.5;// *blur_w;
						acc_ray_vis += f_vis * (1.f - acc_ray_vis.a);
					}
					//float3 prev_pos_f = prev_ivis == 0 ? pos_f : prev_step_pos_frags[k];
					//float z_mid = (pos_f.z + prev_pos_f.z) * 0.5;
					//float t = (z_mid - pos_lens.z) / lens_ray_v.z;
					//float3 pos_lensray_at_midz = pos_lens + lens_ray_v * t;
					//float frag_radius = GetFragHitRadius(-z_mid, ratio_rn_zn);
					//if (dot(pos_f - pos_lensray_at_midz, prev_pos_f - pos_lensray_at_midz) <= frag_radius * 1)
					//{
					//	//rw_fragment_blendout[DTid.xy] = ConvertUIntToFloat4(i_vis); return;
					//	//rw_fragment_blendout[DTid.xy] = float4(1, 0, 0, 1); return;
					//	// note that lens ray hits only one fragment amongst the fragments that are located along the viewing ray.
					//	// "The intervals are still narrowed down quickly because lens rays are almost perpendicular to the image plane."
					//	is_hit = true;
					//	float hit_z = -pos_lensray_at_midz.z;
					//	float coc_r = abs(hit_z - g_cbEnv.dof_focus_z) / hit_z * lens_const;
					//	float coc_r_ip = pix_r * hit_z / g_cbCamState.near_plane * coc_r;
					//	float blur_w = 1.f;
					//	if (coc_r_ip > pix_r)
					//	{
					//		// use DOB setting (degree of blurring, refer to "Real-Time Depth-of-Field Rendering Using Anisotropically Filtered Mipmap Interpolation")
					//		float sigma = coc_r_ip / 2.0;
					//		blur_w = exp(-0.5* (coc_r_ip) * (coc_r_ip) / (sigma*sigma));
					//	}
					//	float4 f_vis_prev = ConvertUIntToFloat4(prev_ivis);
					//	float4 f_vis = ConvertUIntToFloat4(i_vis);
					//	f_vis = (f_vis + f_vis_prev) * 0.5;// *blur_w;
					//	acc_ray_vis += f_vis * (1.f - acc_ray_vis.a);
					//}
#if ZT_MODEL == 1
					// consider z-thickness
#else
#endif
				}

				prev_step_pos_frags[k] = pos_f;
				prev_step_ivis[k] = i_vis;
			} // for (int k = 0; k < frag_cnt_at_footprint_pix; k++)
			for (k = frag_cnt_at_footprint_pix; k < MAX_LAYERS; k++)
			{
				prev_step_pos_frags[k] = (float3)0;
				prev_step_ivis[k] = 0;
			}

			if (acc_ray_vis.a > ERT_ALPHA) break;
			UPDATE_LENSRAY_ADDR(j, 1);
		} // for (int j = 0; j <= count_steps; j++)
		/*
		for (int j = 0; j < count_steps; j++)
		{
			// float GetFragHitRadius(float z, float ratio_rn_zn)
			// bool IsRayHitFrag(float3 pos_s, float3 ray_v, Fragment f, float2 xy_ss, float ratio_rn_zn)
			int frag_cnt_at_footprint_pix = (int)fragment_counter[p_addr];
			uint addr_base_at_footprint_pix = (p_addr.y * g_cbCamState.rt_width + p_addr.x) * bytes_frags_per_pixel;
			for (int k = 0; k < frag_cnt_at_footprint_pix; k++)
			{
#if ZT_MODEL == 1
				Fragment f;
				GET_FRAG(f, addr_base_at_footprint_pix, k);
				uint i_vis = f.i_vis;
				float frag_z = RayHitFragZ(pos_lens, lens_ray_v, f, p_addr.xy, ratio_rn_zn);
#else
				float ray_dist_from_ip = LOAD1_KBUF_Z(deep_k_buf, addr_base_at_footprint_pix, k);
				uint i_vis = LOAD1_KBUF_VIS(deep_k_buf, addr_base_at_footprint_pix, k);
				float frag_z = RayHitFragZ(pos_lens, lens_ray_v, ray_dist_from_ip, p_addr.xy, ratio_rn_zn);
#endif
				if (frag_z > 0)
				{
					// note that lens ray hits only one fragment amongst the fragments that are located along the viewing ray.
					// g_cbCamState.near_plane is Focal Length?!
					float lens_const_test = g_cbEnv.dof_lens_r * g_cbCamState.near_plane / (g_cbEnv.dof_focus_z - g_cbCamState.near_plane);
					///
					float coc_r = abs(frag_z - g_cbEnv.dof_focus_z) / frag_z * lens_const_test;
					float coc_r_ip = pix_r * frag_z / g_cbCamState.near_plane * coc_r;
					float blur_w = 1.f;
					if (coc_r_ip > pix_r)
					{
						// use DOB setting (degree of blurring, refer to "Real-Time Depth-of-Field Rendering Using Anisotropically Filtered Mipmap Interpolation")
						float sigma = coc_r_ip / 2.0;
						blur_w = exp(-0.5* (coc_r_ip) * (coc_r_ip) / (sigma*sigma));
					}
					float4 f_vis = ConvertUIntToFloat4(i_vis) *blur_w;
					acc_ray_vis += f_vis * (1.f - acc_ray_vis.a);
					break;
				}
			}
			if (acc_ray_vis.a > ERT_ALPHA) break;
			UPDATE_LENSRAY_ADDR(j, 1);
		}
		/**/
		// later, replace optical compositing
		// .. incorrect alpha coverage
		// cannot handle rays that hit no frag
		if (acc_ray_vis.a > 0)
		{
			acc_lens_vis += acc_ray_vis;
			num_hit_rays++;
		}
		else _test_cnt_no_hit++;
	} // for (int iray = 0; iray < g_cbEnv.dof_lens_ray_num_samples; iray++)
	acc_lens_vis /= num_hit_rays;// g_cbEnv.dof_lens_ray_num_samples;
	rw_fragment_blendout[DTid.xy] = acc_lens_vis;
	//rw_fragment_blendout[DTid.xy] = float4((float3)_test_cnt_no_hit / 8.0, 1);
	return;
}

[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void KB_TO_MINMAXDEPTHTEXTURE(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	//if (DTid.x >= g_cbCamState.rt_width / 2 || DTid.y >= g_cbCamState.rt_height / 2)
	if (DTid.x >= g_cbCamState.rt_width || DTid.y >= g_cbCamState.rt_height)
		return;

	// at this moment, only static k buffer is supported.
	const uint k_value = g_cbCamState.k_value;
	uint bytes_per_frag = 4 * NUM_ELES_PER_FRAG;
	uint bytes_frags_per_pixel = k_value * bytes_per_frag; // to do : consider the dynamic scheme. (4 bytes unit)
	//uint pixel_id = DTid.y * g_cbCamState.rt_width + DTid.x;
	//uint addr_base = pixel_id * bytes_frags_per_pixel;

	uint2 xy_[4] = 
	{
		//uint2(DTid.x * 2 + 0, DTid.y * 2 + 0)
		uint2(DTid.x, DTid.y)
		, uint2(DTid.x * 2 + 1, DTid.y * 2 + 0)
		, uint2(DTid.x * 2 + 0, DTid.y * 2 + 1)
		, uint2(DTid.x * 2 + 1, DTid.y * 2 + 1)
	};

	uint addr_base_[4] =
	{
		(xy_[0].y * g_cbCamState.rt_width + xy_[0].x) * bytes_frags_per_pixel
		, (xy_[1].y * g_cbCamState.rt_width + xy_[1].x) * bytes_frags_per_pixel
		, (xy_[2].y * g_cbCamState.rt_width + xy_[2].x) * bytes_frags_per_pixel
		, (xy_[3].y * g_cbCamState.rt_width + xy_[3].x) * bytes_frags_per_pixel
	};

	int frag_cnt_[4] =
	{
		(int)fragment_counter[xy_[0]]
		, (int)fragment_counter[xy_[1]]
		, (int)fragment_counter[xy_[2]]
		, (int)fragment_counter[xy_[3]]
	};

	//global min max
	[loop]
	float z_min = FLT_MAX, z_max = 0;
	for (int i = 0; i < 1; i++)
	{
		int frag_cnt = frag_cnt_[i];
		if (frag_cnt == 0) continue;

		// g_cbCamState.mat_ss2ws is used as ss2cs
		float3 pos_ip_cs = TransformPoint(float3(xy_[i], 0), g_cbCamState.mat_ss2ws);
		float3 ray_dir_cs = float3(0, 0, -1);
		if (g_cbCamState.cam_flag & 0x1)
			ray_dir_cs = pos_ip_cs;
		ray_dir_cs = normalize(ray_dir_cs);
		uint addr_base = addr_base_[i];

		for (int j = 0; j < frag_cnt; j++)
		{
#if ZT_MODEL == 1
			Fragment f;
			GET_FRAG(f, addr_base, j);

			float a = (f.i_vis >> 24) / 255.0;
			float zthick_merge = f.zthick - g_cbCamState.cam_vz_thickness;
			float check_merge = ((a - f.opacity_sum) * (a - f.opacity_sum) > (1.0 / 255)) && zthick_merge > 0.000001;

			float3 pos_f_b = pos_ip_cs + ray_dir_cs * f.z;
			float3 pos_f_f = pos_ip_cs + ray_dir_cs * (f.z - f.zthick);
			float frag_z = -pos_f_b.z;
			z_min = min(z_min, check_merge ? -pos_f_f.z : frag_z);
			z_max = max(z_max, frag_z);
#else
			float ray_dist_from_ip = LOAD1_KBUF_Z(deep_k_buf, addr_base, j);
			float3 pos_f = pos_ip_cs + ray_dir_cs * ray_dist_from_ip;
			float z = -pos_f.z;

			z_min = min(z_min, z);
			z_max = max(z_max, z);
#endif
		}
	}

	uint dummy;
	InterlockedMin(rw_global_z_minmax_buffer[0], asuint(z_min), dummy);
	if (dummy == 0) InterlockedExchange(rw_global_z_minmax_buffer[0], asuint(z_min), dummy);
	InterlockedMax(rw_global_z_minmax_buffer[1], asuint(z_max), dummy);

	rw_z_minmax_texture[DTid.xy] = float2(z_max == 0 ? 0 : z_min, z_max);
}