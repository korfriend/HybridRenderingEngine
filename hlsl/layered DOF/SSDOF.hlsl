#include "../CommonShader.hlsl"

#define DOWN_SAMPLE 4

Texture2D<uint> fragment_counter : register(t10);
ByteAddressBuffer deep_k_buf : register(t11);

RWTexture2D<unorm float4> rw_fragment_blendout : register(u10);

Buffer<uint> global_z_minmax_buffer : register(t15);
Texture2DArray<float2> z_minmax_textures : register(t16);
RWBuffer<uint> rw_global_z_minmax_buffer : register(u15);
RWTexture2DArray<float2> rw_z_minmax_textures : register(u16);

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

// assume camera space
// right handed, view dir is -z
// ray_v is a unit vector
// herein, z_coord and z are different: z_coord is a minus value, z is a distance (positive) value.
float3 ComputeCSPosPlaneZ(float z_coord, float3 pos_s, float3 ray_v)
{
	// http://www.gisdeveloper.co.kr/?p=792
	float t = (z_coord - pos_s.z) / ray_v.z;
	return pos_s + ray_v * t;
}

//#define GetFragHitRadius(Z, ZN, RN) (Z) / (ZN) * (RN)
float GetFragHitRadius(float z, float ratio_rn_zn)
{
	// ratio_rn_zn = rn / zn
	return z * ratio_rn_zn;
}

bool IsZeroPoint(float3 p)
{
	return p.x*p.x + p.y*p.y + p.z*p.z < FLT_MIN;
}

uint GetUintVisAt(int addr_base, int k)
{
#if ZT_MODEL == 1
	Fragment f;
	GET_FRAG(f, addr_base, k);
	uint i_vis = f.i_vis;
#else
	uint i_vis = LOAD1_KBUF_VIS(deep_k_buf, addr_base, k);
#endif
	return i_vis;
}

float3 ComputeCSPosPersFromKB(float3 p_cs, float z_bnd, int k, int bytes_frags_per_pixel)
{
	// g_cbCamState.mat_ss2ws is used as ss2cs
	float3 p_ss = TransformPoint(p_cs, g_cbCamState.mat_ws2ss);

	int2 xy_ss = int2(p_ss.x + 0.5, p_ss.y + 0.5);
	int frag_cnt_at_footprint = (int)fragment_counter[xy_ss];

	// assume a perspective projection
	float3 r_dir = normalize(p_cs);
	float rz = -r_dir.z;
	float ray_dist;
	if (frag_cnt_at_footprint <= k)
	{
		ray_dist = z_bnd / rz;
	}
	else
	{
		int addr_base_at_footprint = (xy_ss.y * g_cbCamState.rt_width + xy_ss.x) * bytes_frags_per_pixel;
#if ZT_MODEL == 1
		Fragment f;
		GET_FRAG(f, addr_base_at_footprint, k);
		float ray_dist_from_ip = f.z;
#else
		float ray_dist_from_ip = LOAD1_KBUF_Z(deep_k_buf, addr_base_at_footprint, k);
#endif
		ray_dist = ray_dist_from_ip + g_cbCamState.near_plane / rz;
	}
	return r_dir * (z_bnd / rz);
}

float3 IntersectionLinePoints(float3 p1, float3 p2, float3 p3, float3 p4)
{
	// float3 p_s, float3 p_e, float3 p_min, float3 p_max
	// note that this is the guaranteed version
	float3 p13, p43, p21;
	float d1343, d4321, d1321, d4343, d2121;
	float numer, denom;

	p13.x = p1.x - p3.x;
	p13.y = p1.y - p3.y;
	p13.z = p1.z - p3.z;
	p43.x = p4.x - p3.x;
	p43.y = p4.y - p3.y;
	p43.z = p4.z - p3.z;
	//if (ABS(p43.x) < EPS && ABS(p43.y) < EPS && ABS(p43.z) < EPS)
	//	return(FALSE);
	p21.x = p2.x - p1.x;
	p21.y = p2.y - p1.y;
	p21.z = p2.z - p1.z;
	//if (ABS(p21.x) < EPS && ABS(p21.y) < EPS && ABS(p21.z) < EPS)
	//	return(FALSE);

	d1343 = p13.x * p43.x + p13.y * p43.y + p13.z * p43.z;
	d4321 = p43.x * p21.x + p43.y * p21.y + p43.z * p21.z;
	d1321 = p13.x * p21.x + p13.y * p21.y + p13.z * p21.z;
	d4343 = p43.x * p43.x + p43.y * p43.y + p43.z * p43.z;
	d2121 = p21.x * p21.x + p21.y * p21.y + p21.z * p21.z;

	denom = d2121 * d4343 - d4321 * d4321;
	//if (ABS(denom) < EPS)
	//	return(FALSE);
	numer = d1343 * d4321 - d1321 * d4343;

	float mua = numer / denom;
	float mub = (d1343 + d4321 * mua) / d4343;

	float3 p_12 = p1 + mua * p21;
	float3 p_34 = p3 + mub * p43;

	return (p_12 + p_34) * 0.5;
}

bool PiecewiseIntersection(out float3 pos_f, out int2 pos_f_ss, float z_min, float z_max, int k, int bytes_frags_per_pixel)
{
	returntrue;
}

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
	//return;
	// at this moment, only static k buffer is supported.
	const uint k_value = g_cbCamState.k_value;
	uint bytes_per_frag = 4 * NUM_ELES_PER_FRAG;
	uint bytes_frags_per_pixel = k_value * bytes_per_frag; // to do : consider the dynamic scheme. (4 bytes unit)
	uint pixel_id = DTid.y * g_cbCamState.rt_width + DTid.x;
	uint addr_base_focalray = pixel_id * bytes_frags_per_pixel;

	//int frag_cnt = (int)fragment_counter[DTid.xy];
	//uint vr_hit_enc = frag_cnt >> 24;
	//frag_cnt = frag_cnt & 0xFFF;

	if (g_cbCamState.cam_flag & 0x1 == 0)
	{
		rw_fragment_blendout[DTid.xy] = float4(1, 0, 0, 1); // INVALID IN ORTHOGONAL PROJECTION
		return;
	}
	
	float3 pos_ip_ss = float3(DTid.xy, 0.0);
	// g_cbCamState.mat_ss2ws is used as ss2cs
	float3 pos_ip_cs = TransformPoint(pos_ip_ss, g_cbCamState.mat_ss2ws);
	float3 vec_eyeray_cs = normalize(pos_ip_cs);

	float3 pos_focus = ComputeCSPosPlaneZ(-g_cbEnv.dof_focus_z, (float3)0, vec_eyeray_cs);

	float3 ipO_cs = TransformPoint(float3(g_cbCamState.rt_width / 2.f, g_cbCamState.rt_height / 2.f, 0), g_cbCamState.mat_ss2ws);
	float3 ipx_cs = TransformPoint(float3(g_cbCamState.rt_width / 2.f + 1.f, g_cbCamState.rt_height / 2.f, 0), g_cbCamState.mat_ss2ws);
	float3 pix_cs = ipx_cs - ipO_cs;
	float pix_r = length(pix_cs) * 0.5; // SCALE for safe hit
	//float ratio_rn_zn = pix_r / g_cbCamState.near_plane;
	// g_cbCamState.near_plane is Focal Length?!
	float dof_lens_F = g_cbEnv.dof_sensor_z * g_cbEnv.dof_focus_z / (g_cbEnv.dof_sensor_z + g_cbEnv.dof_focus_z);
	float dof_ratio_sensor_to_ip = g_cbCamState.near_plane / g_cbEnv.dof_sensor_z;
	float pix_coc_r_const = dof_lens_F * dof_ratio_sensor_to_ip;

	// for fetch minmax z values from z_minmax_textures
	int half_w = g_cbCamState.rt_width / DOWN_SAMPLE;
	int half_h = g_cbCamState.rt_height / DOWN_SAMPLE;
	// entire layers culling...
	// in most practical cases, this is not efficient...
	// so, I do not perform this culling test (but do the layer's culling test per intersection ray)
#if EARLY_ENTIRE_LAYERS_CULLING == 1
	{
		float g_z_min = FLT_MAX, g_z_max = 0;
		for (int k = 0; k < (int)k_value; k++)
		{
			float z_min = asfloat(global_z_minmax_buffer[0 + 2 * k]);
			if (z_min == 0) break;
			float z_max = asfloat(global_z_minmax_buffer[1 + 2 * k]);
			g_z_min = min(g_z_min, z_min);
			g_z_max = max(g_z_max, z_max);
		}
		if (g_z_max == 0) return;

		float3 pos_lens_bb = float3(-1, 1, 0) * g_cbEnv.dof_lens_r;
		float lens_coc_ip_const_bb = length(pos_lens_bb) * pix_coc_r_const / (g_cbEnv.dof_focus_z - dof_lens_F);
		float coc_r_ip_zmin = lens_coc_ip_const_bb * abs(g_z_min - g_cbEnv.dof_focus_z) / g_z_min;
		float coc_r_ip_zmax = lens_coc_ip_const_bb * abs(g_z_max - g_cbEnv.dof_focus_z) / g_z_max;
		float3 lens_ray_v_bb = normalize(pos_focus - pos_lens_bb);
		float3 p_lensray_on_zplane = ComputeCSPosPlaneZ(coc_r_ip_zmin > coc_r_ip_zmax ? -g_z_min : -g_z_max, pos_lens_bb, lens_ray_v_bb);
		float3 p_lensray_on_zplane_ss = TransformPoint(p_lensray_on_zplane, g_cbCamState.mat_ws2ss);
		
		float max_coc_ip = max(coc_r_ip_zmin, coc_r_ip_zmax) / (pix_r); // diameter
		uint nbuf_idx = max(ceil(log2(max_coc_ip)) - 2, 0);
		int w_load_offset = ((nbuf_idx) % DOWN_SAMPLE) * half_w;
		int h_load_offset = ((nbuf_idx) / DOWN_SAMPLE) * half_h;
		float2 p_ss = p_lensray_on_zplane_ss.xy;
		uint2 p_xy = uint2(p_ss / DOWN_SAMPLE + (float2)0.5);
		// note that, in the k-buffer structure, 0-th layer represents 1st hit along eye ray.
		float2 z_minmax = z_minmax_textures[int3(p_xy + uint2(w_load_offset, h_load_offset), 0)]; 
		if (z_minmax.x == 0)
		{
			//rw_fragment_blendout[DTid.xy] = float4(1, 1, 0, 1);
			return;
		}
	}
#endif

#define ITERATION_RAY_HIT_TEST 3

	float4 acc_lens_vis = (float4)0;
	int num_hit_rays = 0;
	int _test_cnt_no_hit = 0;
	[loop]
	for (int iray = 0; iray < g_cbEnv.dof_lens_ray_num_samples; iray++)
	{
		// random pos_lens, lens_ray_v
		// g_cbEnv.dof_lens_r
		int hash_idx = DTid.x + g_cbCamState.rt_width * DTid.y + iray * g_cbCamState.rt_width * g_cbCamState.rt_height;
		// jittering
		float3 random_sample = float3(
			_random(hash_idx),
			_random(hash_idx + g_cbCamState.rt_width * g_cbCamState.rt_height * g_cbEnv.dof_lens_ray_num_samples), 0);//
			//_random(hash_idx + g_cbCamState.rt_width * g_cbCamState.rt_height * g_cbEnv.dof_lens_ray_num_samples * 2));

		//float3 random_sample = mul(AngleAxis3x3(F_PI / g_cbEnv.dof_lens_ray_num_samples * iray, float3(0, 0, -1)), float3(0.5, 0, 0));

		float3 pos_lens = (random_sample - float3(0.5, 0.5, 0)) * 2.0 * g_cbEnv.dof_lens_r;
		float3 lens_ray_v = normalize(pos_focus - pos_lens);

		float4 acc_ray_vis = (float4)0;
		float acc_alpha_wo_coc = 0;

		// is parallel with vec_eyeray_cs
		float3 vdiff = lens_ray_v - vec_eyeray_cs;
		float angle = atan2(vdiff.y, vdiff.x); // -pi~pi
		if(abs(angle) < FLT_MIN)
		{
			int2 p_xy = int2(pos_ip_ss.x + 0.5, pos_ip_ss.y + 0.5);
			int addr_base_at_ray = (p_xy.y * g_cbCamState.rt_width + p_xy.x) * bytes_frags_per_pixel;
			int frag_cnt_at_ray = (int)fragment_counter[p_xy];
			for (int k = 0; k < frag_cnt_at_ray; k++)
			{
				uint i_vis = GetUintVisAt(addr_base_at_ray, k);
				acc_ray_vis += ConvertUIntToFloat4(i_vis) * (1.0 - acc_ray_vis.a);
			}
			acc_lens_vis += acc_ray_vis;
			num_hit_rays++;
			//rw_fragment_blendout[DTid.xy] = float4(0, 1, 0, 1); return;
			
			continue; // next ray
		}

		float lens_coc_ip_const = length(pos_lens) * pix_coc_r_const / (g_cbEnv.dof_focus_z - dof_lens_F);

		// layer culling "Depth-of-Field Rendering with Multiview Synthesis, 2009, TOG"


		// note that, herein, eye ray and lens ray are not parallel!
		// , which implies that we consider only a hit per layer (k)
		// for each fragment layer, find a possible intersection point 
		// by iteratively sampling the surface (fragment layer) at interval endpoints to refine a piecewise-linear approximation
		[loop]
		for (int k = 0; k < (int)k_value; k++)
		{
			float z_min = asfloat(global_z_minmax_buffer[0 + 2 * k]);
			float z_max = asfloat(global_z_minmax_buffer[1 + 2 * k]);
			if (z_min == 0) break; // no more layers so not continue

			float3 p_lensray_s = ComputeCSPosPlaneZ(-z_min, pos_lens, lens_ray_v);
			float3 p_lensray_e = ComputeCSPosPlaneZ(-z_max, pos_lens, lens_ray_v);

			float3 p_lensray_ss_s = TransformPoint(p_lensray_s, g_cbCamState.mat_ws2ss);
			float3 p_lensray_ss_e = TransformPoint(p_lensray_e, g_cbCamState.mat_ws2ss);
			float2 v_lensray_ss_se = p_lensray_ss_e.xy - p_lensray_ss_s.xy;
#if RAY_LAYER_CULLING == 1
			float2 p_footprint_rect_min_ss = float2(min(p_lensray_ss_s.x, p_lensray_ss_e.x), min(p_lensray_ss_s.y, p_lensray_ss_e.y));
			float footprint_rect_size = max(abs(v_lensray_ss_se.x), abs(v_lensray_ss_se.y));
			uint nbuf_idx = max(ceil(log2(footprint_rect_size)) - 2, 0);
			int w_load_offset = ((nbuf_idx) % DOWN_SAMPLE) * half_w;
			int h_load_offset = ((nbuf_idx) / DOWN_SAMPLE) * half_h;
			uint2 p_fetch_xy = uint2(p_footprint_rect_min_ss / DOWN_SAMPLE + (float2)0.5);
			// note that, in the k-buffer structure, 0-th layer represents 1st hit along eye ray.
			float2 z_minmax = z_minmax_textures[int3(p_fetch_xy + uint2(w_load_offset, h_load_offset), k)];
			if (z_minmax.x == 0) 
			{
				//rw_fragment_blendout[DTid.xy] = float4(0, 1, 1, 1);
				//return;
				break; // no more layers along the ray footprint so not continue
			}
			z_min = z_minmax.x;
			z_max = z_minmax.y;
			p_lensray_s = ComputeCSPosPlaneZ(-z_min, pos_lens, lens_ray_v);
			p_lensray_e = ComputeCSPosPlaneZ(-z_max, pos_lens, lens_ray_v);
#endif

#define OPT_MIXMAXZ_SAMPLES 3
#if OPT_MIXMAXZ_SAMPLES > 1
			// update z_min/max, p_lensray_s/p_lensray_e with a more narrow depth interval
			uint2 prev_p_xy = (uint2)0;
			float nrw_z_min = FLT_MAX, nrw_z_max = 0;
			for (int o = 0; o < OPT_MIXMAXZ_SAMPLES; o++)
			{
				float2 p_ss = p_lensray_ss_s.xy + v_lensray_ss_se * (float)o / (float)(OPT_MIXMAXZ_SAMPLES - 1);
				uint2 p_xy = uint2(p_ss / 4.0 + (float2)0.5);
				uint2 pdiff = p_xy - prev_p_xy;
				if (dot(pdiff, pdiff) == 0) continue;
				float2 z_minmax = z_minmax_textures[int3(p_xy, k)];
				if (z_minmax.x > 0)
				{
					nrw_z_min = min(nrw_z_min, z_minmax.x);
					nrw_z_max = max(nrw_z_max, z_minmax.y);
				}
				prev_p_xy = p_xy;
			}
			if (nrw_z_max > 0)
			{
				z_min = max(z_min, nrw_z_min);
				z_max = min(z_max, nrw_z_max);
				p_lensray_s = ComputeCSPosPlaneZ(-z_min, pos_lens, lens_ray_v);
				p_lensray_e = ComputeCSPosPlaneZ(-z_max, pos_lens, lens_ray_v);
			}
#endif

			float3 p_eyeray_s = ComputeCSPosPersFromKB(p_lensray_s, z_max, k, bytes_frags_per_pixel);
			float3 p_eyeray_e = ComputeCSPosPersFromKB(p_lensray_e, z_min, k, bytes_frags_per_pixel);

			if (z_max - z_min < FLT_MIN)
			{
				float3 pos_m = (p_eyeray_s + p_eyeray_e) * 0.5;
				float3 p_ss = TransformPoint(pos_m, g_cbCamState.mat_ws2ss);
				int2 p_xy = int2(p_ss.x + 0.5, p_ss.y + 0.5);
				int frag_cnt = (int)fragment_counter[p_xy];
				if (frag_cnt > k)
				{
					int addr_base = (p_xy.y * g_cbCamState.rt_width + p_xy.x) * bytes_frags_per_pixel;
					uint i_vis = GetUintVisAt(addr_base, k);
					acc_ray_vis += ConvertUIntToFloat4(i_vis) * (1.0 - acc_ray_vis.a);
				}

				//rw_fragment_blendout[DTid.xy] = float4(1, 0, 1, 1); return;
				
				continue; // hit test finished at the (k)-th layer
			}
			//rw_fragment_blendout[DTid.xy] = float4(1, 0, 0, 1); return;

			// find a possible intersection point 
			// through a piecewise-linear approximation 
			// "Depth-of-Field Rendering with Multiview Synthesis, 2009, TOG"
			for (int i = 0; i < ITERATION_RAY_HIT_TEST; i++)
			{
				float3 p_i = IntersectionLinePoints(p_eyeray_s, p_eyeray_e, p_lensray_s, p_lensray_e);
				float3 p_is = ComputeCSPosPersFromKB(p_i, (uint)i%2 == 0? z_min : z_max, k, bytes_frags_per_pixel);
				if (p_i.z < p_is.z)
					p_eyeray_e = p_is;
				else
					p_eyeray_s = p_is;
			}
			// hit 에서 값을 제대로 찾는지 확인...
			float3 p_f = (p_eyeray_s + p_eyeray_e) * 0.5;
			float3 p_f_ss = TransformPoint(p_f, g_cbCamState.mat_ws2ss);
			int2 p_f_xy = int2(p_f_ss.x + 0.5, p_f_ss.y + 0.5);
			int frag_cnt_along_hit_eyeray = (int)fragment_counter[p_f_xy];
			if (k < frag_cnt_along_hit_eyeray)
			{
				//rw_fragment_blendout[DTid.xy] = float4(1, 0, 0, 1); return;
				int addr_base_f = (p_f_xy.y * g_cbCamState.rt_width + p_f_xy.x) * bytes_frags_per_pixel;
				uint i_vis = GetUintVisAt(addr_base_f, k);
				float4 f_vis = ConvertUIntToFloat4(i_vis);

				float hit_z = -p_f.z;
				float coc_r_ip = lens_coc_ip_const * abs(hit_z - g_cbEnv.dof_focus_z) / hit_z;
				float blur_w = 1.f;
				//if (coc_r_ip > pix_r)
				{
					// use DOB setting (degree of blurring, refer to "Real-Time Depth-of-Field Rendering Using Anisotropically Filtered Mipmap Interpolation")
					float sigma = coc_r_ip / 2.0;
					blur_w = exp(-0.5* (coc_r_ip) * (coc_r_ip) / (sigma*sigma));
				}
				acc_alpha_wo_coc += f_vis.a * (1.0 - acc_ray_vis.a);
				
				//f_vis *= blur_w;
				acc_ray_vis += f_vis * (1.0 - acc_ray_vis.a);
			}
			if (acc_alpha_wo_coc > ERT_ALPHA) break;
		} // for (int k = 0; k < (int)k_value; k++)
		// later, replace optical compositing
		// .. incorrect alpha coverage
		// cannot handle rays that hit no frag
		if (acc_alpha_wo_coc > 0)
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
	int half_w = g_cbCamState.rt_width / DOWN_SAMPLE;
	int half_h = g_cbCamState.rt_height / DOWN_SAMPLE;
	if (DTid.x >= (uint)half_w || DTid.y >= (uint)half_h)
		return;

	// at this moment, only static k buffer is supported.
	const int k_value = (int)g_cbCamState.k_value;
	uint bytes_per_frag = 4 * NUM_ELES_PER_FRAG;
	uint bytes_frags_per_pixel = k_value * bytes_per_frag; // to do : consider the dynamic scheme. (4 bytes unit)
	//uint pixel_id = DTid.y * g_cbCamState.rt_width + DTid.x;
	//uint addr_base = pixel_id * bytes_frags_per_pixel;

	uint2 xy_[DOWN_SAMPLE * DOWN_SAMPLE];
	uint addr_base_[DOWN_SAMPLE * DOWN_SAMPLE];
	int frag_cnt_[DOWN_SAMPLE * DOWN_SAMPLE];
	int sum_fs = 0;
	for (int _y = 0; _y < DOWN_SAMPLE; _y++)
		for (int _x = 0; _x < DOWN_SAMPLE; _x++)
		{
			uint2 xy = uint2(DTid.x * DOWN_SAMPLE + _x, DTid.y * DOWN_SAMPLE + _y);
			int idx = _x + _y * DOWN_SAMPLE;
			xy_[idx] = xy;
			addr_base_[idx] = (xy.y * g_cbCamState.rt_width + xy.x) * bytes_frags_per_pixel;

			int num_fs = fragment_counter[xy];
			frag_cnt_[idx] = num_fs;
			sum_fs += num_fs;
		}
	
	if (sum_fs == 0) return;

	//global min max
	[loop]
	for (int k = 0; k < k_value; k++)
	{
		float z_min = FLT_MAX, z_max = 0;
		for (int i = 0; i < DOWN_SAMPLE * DOWN_SAMPLE; i++)
		{
			int frag_cnt = frag_cnt_[i]; // current GPUs provide zero value when the address is out of the buffer/texture by default.
			if (k < frag_cnt) {

				// g_cbCamState.mat_ss2ws is used as ss2cs
				float3 pos_ip_cs = TransformPoint(float3(xy_[i], 0), g_cbCamState.mat_ss2ws);
				float3 ray_dir_cs = normalize(pos_ip_cs);
				uint addr_base = addr_base_[i];

#if ZT_MODEL == 1
				Fragment f;
				GET_FRAG(f, addr_base, k);

				float a = (f.i_vis >> 24) / 255.0;
				float zthick_merge = f.zthick - g_cbCamState.cam_vz_thickness;
				float check_merge = ((a - f.opacity_sum) * (a - f.opacity_sum) > (1.0 / 255)) && zthick_merge > 0.000001;

				float3 pos_f_b = pos_ip_cs + ray_dir_cs * f.z;
				float3 pos_f_f = pos_ip_cs + ray_dir_cs * (f.z - f.zthick);
				float frag_z = -pos_f_b.z;
				z_min = min(z_min, check_merge ? -pos_f_f.z : frag_z);
				z_max = max(z_max, frag_z);
#else
				float ray_dist_from_ip = LOAD1_KBUF_Z(deep_k_buf, addr_base, k);
				float3 pos_f = pos_ip_cs + ray_dir_cs * ray_dist_from_ip;
				float z = -pos_f.z;

				z_min = min(z_min, z);
				z_max = max(z_max, z);
#endif
				//if(z > g_cbCamState.far_plane)
				//{ rw_fragment_blendout[DTid.xy] = float4(1, 1, 0, 1); return; }
			}
		}

		if (z_max != 0)
		{
			//{ rw_fragment_blendout[DTid.xy] = float4(1, 0, 0, 1); return; }
			uint dummy;
			InterlockedMin(rw_global_z_minmax_buffer[0 + 2 * k], asuint(z_min), dummy);
			if (dummy == 0)  InterlockedExchange(rw_global_z_minmax_buffer[0 + 2 * k], asuint(z_min), dummy);
			InterlockedMax(rw_global_z_minmax_buffer[1 + 2 * k], asuint(z_max), dummy);

			rw_z_minmax_textures[int3(DTid.xy, k)] = float2(z_min, z_max);

			//if (asfloat(rw_global_z_minmax_buffer[0 + 2 * 1]) > 0) rw_fragment_blendout[DTid.xy] = float4(0, 1, 0, 1);
			//if ((rw_global_z_minmax_buffer[2]) == 1234) rw_fragment_blendout[DTid.xy] = float4(0, 1, 0, 1);
			//else rw_fragment_blendout[DTid.xy] = float4(1, 0, 0, 1);
		}
	}

	//if (asfloat(rw_global_z_minmax_buffer[0 + 2 * 1]) > 0) rw_fragment_blendout[DTid.xy] = float4(0, 1, 0, 1);
	//if ((rw_global_z_minmax_buffer[0]) == 1234) rw_fragment_blendout[DTid.xy] = float4(0, 1, 0, 1);
	//else rw_fragment_blendout[DTid.xy] = float4(1, 0, 0, 1);
}

[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void KB_TO_MINMAXDEPTH_NBUF(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	const uint nbuf_idx = (uint)g_cbCamState.iSrCamDummy__1;
	int half_w = g_cbCamState.rt_width / DOWN_SAMPLE;
	int half_h = g_cbCamState.rt_height / DOWN_SAMPLE;
	if (DTid.x >= (uint)half_w || DTid.y >= (uint)half_h || nbuf_idx < 1)
		return;

	// at this moment, only static k buffer is supported.
	const int k_value = (int)g_cbCamState.k_value;

	int w_load_offset = ((nbuf_idx - 1) % DOWN_SAMPLE) * half_w;
	int h_load_offset = ((nbuf_idx - 1) / DOWN_SAMPLE) * half_h;
	int w_store_offset = (nbuf_idx % DOWN_SAMPLE) * half_w;
	int h_store_offset = (nbuf_idx / DOWN_SAMPLE) * half_h;

	int2 xy_[4] =
	{
		  int2(DTid.x + 0, DTid.y + 0)
		, int2(DTid.x + 1, DTid.y + 0)
		, int2(DTid.x + 0, DTid.y + 1)
		, int2(DTid.x + 1, DTid.y + 1)
	};

	[loop]
	for (int k = 0; k < k_value; k++)
	{
		float z_min = FLT_MAX, z_max = 0;
		for (int i = 0; i < 4; i++)
		{
			//float2 z_minmax = rw_z_minmax_textures.SampleLevel(g_samplerPoint, float3(xy_[i], k), 0).rg
			int2 xy = xy_[i];
			if (xy.x >= half_w || xy.y >= half_h) continue;

			float2 z_minmax = rw_z_minmax_textures[int3(xy_[i] + int2(w_load_offset, h_load_offset), k)];
			if (z_minmax.y != 0)
			{
				z_min = min(z_minmax.x, z_min);
				z_max = max(z_minmax.y, z_max);
			}
		}

		if (z_max != 0)
			rw_z_minmax_textures[int3(DTid.xy + int2(w_store_offset, h_store_offset), k)] = float2(z_min, z_max);
	}
}