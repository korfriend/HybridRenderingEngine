#include "../CommonShader.hlsl"
#include "../macros.hlsl"

Texture3D tex3D_volume : register(t0);
Texture3D tex3D_volblk : register(t1);
Texture3D tex3D_volmask : register(t2);
Buffer<float4> buf_otf : register(t3); // unorm 으로 변경하기
Buffer<float4> buf_windowing : register(t4); // not used here.
Buffer<int> buf_ids : register(t5); // Mask OTFs // not used here.
Texture2D<float> vr_fragment_1sthit_read : register(t6);

RWTexture2D<uint> fragment_counter : register(u0);
RWByteAddressBuffer deep_k_buf : register(u1);
RWTexture2D<float4> fragment_vis : register(u2);
RWTexture2D<float> fragment_zdepth : register(u3);

RWTexture2D<float> vr_fragment_1sthit_write : register(u4);

#define AO_MAX_LAYERS 8 

Texture2DArray<unorm float> ao_textures : register(t10);
Texture2D<unorm float> ao_vr_texture : register(t20);

// same as Sr_Common.hlsl
float4 VrOutlineTest(const in int2 tex2d_xy, inout float depth_c, const in float discont_depth_criterion, const in float3 edge_color, const in int thick)
{
	float2 min_rect = (float2)0;
	float2 max_rect = float2(g_cbCamState.rt_width - 1, g_cbCamState.rt_height - 1);

	float4 vout = (float4) 0;
	if (depth_c > 100000)
	{
		int count = 0;
		float depth_min = FLT_MAX;

		for (int y = -thick; y <= thick; y++) {
			for (int x = -thick; x <= thick; x++) {
				float2 sample_pos = tex2d_xy.xy + float2(x, y);
				float2 v1 = min_rect - sample_pos;
				float2 v2 = max_rect - sample_pos;
				float2 v12 = v1 * v2;
				if (v12.x >= 0 || v12.y >= 0)
					continue;
				float depth = vr_fragment_1sthit_read[int2(sample_pos)].r;
				if (depth < 100000) {
					count++;
					depth_min = min(depth_min, depth);
				}
			}
		}

		float w = 2 * thick + 1;
		float alpha = min((floaT)count / (w * w / 2.f), 1.f);
		//alpha *= alpha;

		vout = float4(edge_color * alpha, alpha);
		depth_c = depth_min;
		//vout = float4(nor_c, 1);
		//vout = float4(depth_h0 / 40, depth_h0 / 40, depth_h0 / 40, 1);
	}

	return vout;
}

#if DYNAMIC_K_MODE == 1
Buffer<uint> sr_offsettable_buf : register(t50);// gres_fb_ref_pidx
#define VR_MAX_LAYERS 400 
#else
#define VR_MAX_LAYERS 9 // SKBTZ, +1 for DVR 
#endif

#if MBT == 1
#include "MomentMath.hlsli"
ByteAddressBuffer moment_container_buf : register(t30); // later for shadow?!
#define LOAD4_MMBUF(F_ADDR, K) moment_container_buf.Load4((F_ADDR + K) * 4)
#define LOAD4F_MMB(V, F_ADDR, K) { uint4 mmi4 = LOAD4_MMBUF(F_ADDR, K); V = float4(asfloat(mmi4.x), asfloat(mmi4.y), asfloat(mmi4.z), asfloat(mmi4.w)); }
void resolveMoments(out float transmittance_at_depth, out float total_transmittance, float depth, int2 sv_pos)
{
	const float moment_bias = MomentOIT.moment_bias;
	const float overestimation = MomentOIT.overestimation;

	int addr_base = (sv_pos.y * g_cbCamState.rt_width + sv_pos.x) * (g_cbCamState.k_value * 4);

	transmittance_at_depth = 1;
	total_transmittance = 1;

	float b_0 = LOAD1F_MMB(addr_base, 0); // 
	if (b_0 - 0.00100050033f < 0)
		return;

	total_transmittance = exp(-b_0);

	float4 b_even;// = moments.Load(idx0);

	LOAD4F_MMB(b_even, addr_base, 1);
	float4 b_odd;// = moments.Load(idx1);
	LOAD4F_MMB(b_odd, addr_base, 5);

	b_even /= b_0;
	b_odd /= b_0;
	const float bias_vector[8] = { 0, 0.75, 0, 0.67666666666666664, 0, 0.63, 0, 0.60030303030303034 };

	transmittance_at_depth = computeTransmittanceAtDepthFrom8PowerMoments(b_0, b_even, b_odd, depth, moment_bias, overestimation, bias_vector);
}
#endif

// case 1 : dvr to deep layers (intra layers are mixed with deep layers)
// case 2 : mixnig dvr (deep layers and on-the-fly samples)

#if SCULPT_MASK==1
#if OTF_MASK==1
INVALID CASE IN THIS VERSION
#endif
#endif

//int Sample_Volume(const in float3 pos_sample_ts)
//{
//    return (int) (tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_sample_ts, 0).r * g_cbVobj.value_range + 0.5f);
//}

#if VR_MODE == 3
bool Sample_Volume_And_Check(inout int sample_v, const in float3 pos_sample_ts, const in int min_valid_v)
{
	float fsample = tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_sample_ts, 0).r;
	return sample_v > 0;
}
bool Vis_Volume_And_Check(inout float4 vis_otf, inout int sample_v, const in float3 pos_sample_ts)
{
	float3 pos_vs = float3(pos_sample_ts.x * g_cbVobj.vol_size.x - 0.5f, 
		pos_sample_ts.y * g_cbVobj.vol_size.y - 0.5f, 
		pos_sample_ts.z * g_cbVobj.vol_size.z - 0.5f);
	int3 idx_vs = int3(pos_vs);

	float samples[8];
	samples[0] = tex3D_volume.Load(int4(idx_vs, 0)).r;
	samples[1] = tex3D_volume.Load(int4(idx_vs + int3(1, 0, 0), 0)).r;
	samples[2] = tex3D_volume.Load(int4(idx_vs + int3(0, 1, 0), 0)).r;
	samples[3] = tex3D_volume.Load(int4(idx_vs + int3(1, 1, 0), 0)).r;
	samples[4] = tex3D_volume.Load(int4(idx_vs + int3(0, 0, 1), 0)).r;
	samples[5] = tex3D_volume.Load(int4(idx_vs + int3(1, 0, 1), 0)).r;
	samples[6] = tex3D_volume.Load(int4(idx_vs + int3(0, 1, 1), 0)).r;
	samples[7] = tex3D_volume.Load(int4(idx_vs + int3(1, 1, 1), 0)).r;

	float3 f3InterpolateRatio = pos_vs - idx_vs;

	float fInterpolateWeights[8];
	fInterpolateWeights[0] = (1.f - f3InterpolateRatio.z) * (1.f - f3InterpolateRatio.y) * (1.f - f3InterpolateRatio.x);
	fInterpolateWeights[1] = (1.f - f3InterpolateRatio.z) * (1.f - f3InterpolateRatio.y) * f3InterpolateRatio.x;
	fInterpolateWeights[2] = (1.f - f3InterpolateRatio.z) * f3InterpolateRatio.y * (1.f - f3InterpolateRatio.x);
	fInterpolateWeights[3] = (1.f - f3InterpolateRatio.z) * f3InterpolateRatio.y * f3InterpolateRatio.x;
	fInterpolateWeights[4] = f3InterpolateRatio.z * (1.f - f3InterpolateRatio.y) * (1.f - f3InterpolateRatio.x);
	fInterpolateWeights[5] = f3InterpolateRatio.z * (1.f - f3InterpolateRatio.y) * f3InterpolateRatio.x;
	fInterpolateWeights[6] = f3InterpolateRatio.z * f3InterpolateRatio.y * (1.f - f3InterpolateRatio.x);
	fInterpolateWeights[7] = f3InterpolateRatio.z * f3InterpolateRatio.y * f3InterpolateRatio.x;

	vis_otf = (float4)0;
	[unroll]
	for (int m = 0; m < 8; m++) {
		float4 vis = LoadOtfBuf(samples[m] * g_cbTmap.tmap_size_x, buf_otf, 1);
		vis_otf += vis * fInterpolateWeights[m];
	}
	return vis_otf.a >= FLT_OPACITY_MIN__;
}
#else
bool Sample_Volume_And_Check(inout int sample_v, const in float3 pos_sample_ts, const in int min_valid_v)
{
	float fsample = tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_sample_ts, 0).r;
    sample_v = (int) (fsample * g_cbVobj.value_range + 0.5f);
#if OTF_MASK==1
    //int max_vint = LoadMaxValueInt(pos_sample_ts, g_cbVobj.vol_size, 255, tex3D_volmask);
    //int mask_otf_id = buf_ids[max_vint];
    //float4 vis_otf = LoadOtfBufId(sample_v, buf_otf, g_cbVobj.opacity_correction, mask_otf_id);
    //return (uint)(vis_otf.a * 255.f) > 0;
	int mask_vint = (int)(tex3D_volmask.SampleLevel(g_samplerPoint, pos_sample_ts, 0).r * g_cbVobj.mask_value_range + 0.5f);
	//int mask_vint = LoadMaxValueInt(pos_sample_ts, g_cbVobj.vol_size, g_cbVobj.mask_value_range, tex3D_volmask);
	return sample_v >= min_valid_v && mask_vint > 0;
#elif SCULPT_MASK == 1
	int max_vint = LoadMaxValueInt(pos_sample_ts, g_cbVobj.vol_size, 255, tex3D_volmask);
    int sculpt_value = (int) (g_cbVobj.vobj_flag >> 24);
    return sample_v >= min_valid_v && (max_vint == 0 || max_vint > sculpt_value);
#else 
    return sample_v >= min_valid_v;
#endif
}

bool Vis_Volume_And_Check(inout float4 vis_otf, inout int sample_v, const in float3 pos_sample_ts)
{
	float fsample = tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_sample_ts, 0).r;
	sample_v = (int)(fsample * g_cbVobj.value_range + 0.5f);
#if OTF_MASK==1
    //int max_vint = LoadMaxValueInt(pos_sample_ts, g_cbVobj.vol_size, 255, tex3D_volmask);
    //int mask_otf_id = buf_ids[max_vint];
    //vis_otf = LoadOtfBufId(sample_v, buf_otf, g_cbVobj.opacity_correction, mask_otf_id);
    //return (uint)(vis_otf.a * 255.f) > FLT_MIN;
	int mask_vint = (int)(tex3D_volmask.SampleLevel(g_samplerPoint, pos_sample_ts, 0).r * g_cbVobj.mask_value_range + 0.5f);
	//int mask_vint = LoadMaxValueInt(pos_sample_ts, g_cbVobj.vol_size, g_cbVobj.mask_value_range, tex3D_volmask);
	vis_otf = LoadOtfBufId(fsample * g_cbTmap.tmap_size_x, buf_otf, g_cbVobj.opacity_correction, mask_vint);
	return vis_otf.a >= FLT_OPACITY_MIN__ && mask_vint > 0;//&& mask_vint == 3;
#elif SCULPT_MASK == 1
	int max_vint = LoadMaxValueInt(pos_sample_ts, g_cbVobj.vol_size, 255, tex3D_volmask);
    int sculpt_value = (int) (g_cbVobj.vobj_flag >> 24);
    vis_otf = LoadOtfBuf(sample_v, buf_otf, g_cbVobj.opacity_correction);
    return ((uint)(vis_otf.a * 255.f) > 0) && (max_vint == 0 || max_vint > sculpt_value);
#else 
	vis_otf = LoadOtfBuf(fsample * g_cbTmap.tmap_size_x, buf_otf, 1);// g_cbVobj.opacity_correction);
    return vis_otf.a >= FLT_OPACITY_MIN__;
#endif
}
#endif

#define SURFACEREFINEMENTNUM 5
void Compute_1stHit(inout int step, inout int sample_v, const in float3 pos_ray_start_ws, const in float3 dir_sample_ws, const in int num_ray_samples)
{
    step = -1;
    sample_v = 0;

    float3 pos_ray_start_ts = TransformPoint(pos_ray_start_ws, g_cbVobj.mat_ws2ts);
    float3 dir_sample_ts = TransformVector(dir_sample_ws, g_cbVobj.mat_ws2ts);
    
    int min_valid_v = g_cbTmap.first_nonzeroalpha_index;
    if (Sample_Volume_And_Check(sample_v, pos_ray_start_ts, min_valid_v))
    {
        step = 0;
        return;
    }

	[loop]
    for (int i = 1; i < num_ray_samples; i++)
    {
        float3 pos_sample_ts = pos_ray_start_ts + dir_sample_ts * (float) i;

        LOAD_BLOCK_INFO(blkSkip, pos_sample_ts, dir_sample_ts, num_ray_samples, i)

        if (blkSkip.blk_value > 0)
        {
	        [loop]
            for (int k = 0; k < blkSkip.num_skip_steps; k++, i++)
            {
                float3 pos_sample_blk_ts = pos_ray_start_ts + dir_sample_ts * (float) i;
                if (Sample_Volume_And_Check(sample_v, pos_sample_blk_ts, min_valid_v))
                {
                    step = i;
                    i = num_ray_samples;
                    k = num_ray_samples;
                    break;
                } // if(sample valid check)
            } // for(int k = 0; k < blkSkip.iNumStepSkip; k++, i++)
        }
        else
        {
            i += blkSkip.num_skip_steps;
        }
		// this is for outer loop's i++
        i -= 1;
    }
}

void SurfaceRefinement(inout float3 pos_refined_ws, inout int sample_v, const in float3 pos_sample_ws, const in float3 dir_sample_ws, const in int num_refinement)
{
    float3 pos_sample_ts = TransformPoint(pos_sample_ws, g_cbVobj.mat_ws2ts);
    float3 dir_sample_ts = TransformVector(dir_sample_ws, g_cbVobj.mat_ws2ts);
    float t0 = 0, t1 = 1;
    
	// Interval bisection
    float3 pos_bis_s_ts = pos_sample_ts - dir_sample_ts;
    float3 pos_bis_e_ts = pos_sample_ts;

    int min_valid_v = g_cbTmap.first_nonzeroalpha_index;
    
	[loop]
    for (int j = 0; j < num_refinement; j++)
    {
        float3 pos_bis_ts = (pos_bis_s_ts + pos_bis_e_ts) * 0.5f;
        float t = (t0 + t1) * 0.5f;
        int _sample_v = 0;
        if (Sample_Volume_And_Check(_sample_v, pos_bis_ts, min_valid_v))
        {
            sample_v = _sample_v;
            pos_bis_e_ts = pos_bis_ts;
            t1 = t;
        }
        else
        {
            pos_bis_s_ts = pos_bis_ts;
            t0 = t;
        }
    }

    pos_refined_ws = pos_sample_ws + dir_sample_ws * (t1 - 1.f);
}

#define GRAD_VOL(P) GradientVolume(P, g_cbVobj.vec_grad_x, g_cbVobj.vec_grad_y, g_cbVobj.vec_grad_z, tex3D_volume)
#define GRAD_NRL_VOL(P, V, L) GradientNormalVolume(L, P, V, g_cbVobj.vec_grad_x, g_cbVobj.vec_grad_y, g_cbVobj.vec_grad_z, tex3D_volume)

float PhongBlinnVr(const float3 cam_view, const in float4 shading_factors, const in float3 light_dirinv, in float3 normal, const in bool is_max_shading)
{
	if (dot(cam_view, normal) > 0)
		normal *= -1.f;
    float diff = dot(light_dirinv, normal);
    if (is_max_shading)
        diff = max(diff, 0);
    else
        diff = abs(diff);
    float reft = pow(diff, shading_factors.w);
    return shading_factors.x + shading_factors.y * diff + shading_factors.z * reft;
}

#define __InsertLayerToSortedDeepLayers(vis_out, depth_out,	f_in, fs, addr_base, num_frags, k_value, merging_beta) {			 \
	int inser_idx = 0;																											 \
	[loop]																														 \
	for (int i = 0; i <= num_frags; i++)																						 \
	{																															 \
		if (fs[i].z > f_in.z)																									 \
		{																														 \
			inser_idx = i;																										 \
			break;																												 \
		}																														 \
	}																															 \
																																 \
	int offset = 0;																												 \
	[loop]																														 \
	for (i = num_frags - 1; i >= inser_idx; i--, offset++)																		 \
	{																															 \
		fs[num_frags - offset] = fs[i];																							 \
	}																															 \
	fs[inser_idx] = f_in;																										 \
	/*merge self-overlapping surfaces to thickness surfaces*/																	 \
	float4 blendout = (float4) 0;																								 \
	int num_frags_new = num_frags + 1;																							 \
	int cnt_sorted_ztsurf = 0;																									 \
	[loop]																														 \
	for (i = 0; i < num_frags_new; i++)																							 \
	{																															 \
		Fragment f_ith = fs[i];																									 \
		if (f_ith.i_vis > 0)																									 \
		{																														 \
			for (int j = i + 1; j < num_frags_new; j++)																			 \
			{																													 \
				Fragment f_next = fs[j];																						 \
				if (f_next.i_vis > 0)																							 \
				{																												 \
					Fragment_OUT fs_out = MergeFrags(f_ith, f_next, merging_beta);												 \
					f_ith = fs_out.f_prior;																						 \
					fs[j] = fs_out.f_posterior;																					 \
				}																												 \
			}																													 \
			if ((f_ith.i_vis >> 24) > 0) /*always.. (just for safe-check)*/														 \
			{																													 \
				fs[cnt_sorted_ztsurf] = f_ith;																					 \
				cnt_sorted_ztsurf++;																							 \
				blendout += ConvertUIntToFloat4(f_ith.i_vis) * (1.f - blendout.a);												 \
			}																													 \
		} /*if (rs_ith.zthick >= FLT_MIN__)*/																					 \
	}																															 \
	vis_out = blendout;																											 \
	depth_out = fs[0].z;																										 \
}

[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void RayCasting(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
    int2 tex2d_xy = int2(DTid.xy);
    if (DTid.x >= g_cbCamState.rt_width || DTid.y >= g_cbCamState.rt_height)
        return;

	const uint k_value = g_cbCamState.k_value;
	uint bytes_per_frag = 4 * NUM_ELES_PER_FRAG;
	uint pixel_id = tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x;
#if DYNAMIC_K_MODE == 1
	if (pixel_id == 0) return;
	uint pixel_offset = sr_offsettable_buf[pixel_id];
	uint addr_base = pixel_offset * bytes_per_frag;
#else
	uint bytes_frags_per_pixel = k_value * bytes_per_frag;
	uint addr_base = pixel_id * bytes_frags_per_pixel;
#endif

	uint num_frags = fragment_counter[DTid.xy];
	uint vr_hit_enc = num_frags >> 24;
	num_frags = num_frags & 0xFFF;

    float4 vis_out = 0;
    float depth_out = FLT_MAX;
    // consider the deep-layers are sored in order
	Fragment fs[VR_MAX_LAYERS];

	float aos[AO_MAX_LAYERS] = {0, 0, 0, 0, 0, 0, 0, 0};
	float ao_vr = 0;
	if (g_cbEnv.r_kernel_ao > 0)
	{
		for (int k = 0; k < AO_MAX_LAYERS; k++) aos[k] = ao_textures[int3(DTid.xy, k)];
		ao_vr = ao_vr_texture[DTid.xy];
	}

	[loop]
    for (uint i = 0; i < num_frags; i++)
    {
        uint i_vis = 0;
		Fragment f;
		GET_FRAG(f, addr_base, i); // from K-buffer
        float4 vis_in = ConvertUIntToFloat4(f.i_vis);
		if (g_cbEnv.r_kernel_ao > 0)
		{
			vis_in.rgb *= 1.f - aos[i];
			f.i_vis = ConvertFloat4ToUInt(vis_in);
		}
        if (vis_in.a > 0)
        {
            vis_out += vis_in * (1.f - vis_out.a);
        }
		fs[i] = f;
    }

	fs[num_frags] = (Fragment)0;
	fs[num_frags].z = FLT_MAX;
    fragment_vis[tex2d_xy] = vis_out;
#if RAYMODE != 0
    fragment_zdepth[tex2d_xy] = depth_out = fs[0].z;
#endif

	//Texture2D<unorm float4> ao_textures[MAX_LAYERS / 4] : register(t10);
	//Texture2D<unorm float> ao_vr_texture : register(t20);
	//fragment_vis[tex2d_xy] = float4((float3)(1.f - ao_textures[0][tex2d_xy].x), 1);
	//fragment_vis[tex2d_xy] = float4((float3)(1.f - ao_vr_texture[tex2d_xy].x), 1);
	//return;

    // Image Plane's Position and Camera State //
    float3 pos_ip_ss = float3(tex2d_xy, 0.0f);
    float3 pos_ip_ws = TransformPoint(pos_ip_ss, g_cbCamState.mat_ss2ws);
    float3 dir_sample_unit_ws = g_cbCamState.dir_view_ws;
    if (g_cbCamState.cam_flag & 0x1)
        dir_sample_unit_ws = pos_ip_ws - g_cbCamState.pos_cam_ws;
    dir_sample_unit_ws = normalize(dir_sample_unit_ws);
    float3 dir_sample_ws = dir_sample_unit_ws * g_cbVobj.sample_dist;
    
	const float merging_beta = 1.0;

	float3 vbos_hit_start_pos = pos_ip_ws;
#if RAYMODE == 0 // DVR
	//depth_out = fragment_zdepth[DTid.xy];
	depth_out = vr_fragment_1sthit_read[DTid.xy];
	

	float4 outline_test = ConvertUIntToFloat4(g_cbVobj.outline_color);
	if (outline_test.w > 0 && vr_hit_enc == 0) {
		// note that the outline appears over background of the DVR front-surface
		float4 outline_color = VrOutlineTest(tex2d_xy, depth_out, 100000.f, outline_test.rgb, (int)(outline_test.w*255.f));
		uint idx_dlayer = 0;
		int num_ray_samples = VR_MAX_LAYERS;
		vis_out = (float4)0;
#if FRAG_MERGING == 1
		Fragment f_dly = fs[0]; // if no frag, the z-depth is infinite
		INTERMIX(vis_out, idx_dlayer, num_frags, outline_color, depth_out, g_cbVobj.sample_dist, fs, merging_beta);
#else
		INTERMIX_V1(vis_out, idx_dlayer, num_frags, outline_color, depth_out, fs);
#endif
		REMAINING_MIX(vis_out, idx_dlayer, num_frags, fs);
		fragment_vis[tex2d_xy] = vis_out;
		fragment_zdepth[tex2d_xy] = min(depth_out, fs[0].z);
		return;
	}
	
	vbos_hit_start_pos = pos_ip_ws + dir_sample_unit_ws * depth_out;
	fragment_zdepth[tex2d_xy] = fs[0].z;

	//bool is_dynamic_transparency = false;// BitCheck(g_cbPobj.pobj_flag, 19);
	//bool is_mask_transparency = true;// BitCheck(g_cbPobj.pobj_flag, 20);
	//float mask_weight = 1.f;
	//if (1)//(is_mask_transparency)// || is_dynamic_transparency)
	//{
	//	float dynamic_alpha_weight = 1;
	//	int out_lined = GhostedEffect(mask_weight, dynamic_alpha_weight, vbos_hit_start_pos, dir_sample_unit_ws, (float3)1, 1, false);
	//	if (is_mask_transparency)
	//		vis_out.rgba *= mask_weight;
	//	//if (out_lined > 0)
	//	//	v_rgba = float4(1, 1, 0, 1);
	//	//else
	//	//{
	//	//if (is_mask_transparency && mask_weight < 0.001f)
	//	//	return;
	//	//		v_rgba.rgba *= mask_weight;
	//	//	if (v_rgba.a <= 0.01) return;
	//	fragment_vis[tex2d_xy] = vis_out;
	//}

	if (vr_hit_enc == 0)//depth_out > FLT_LARGE)
	{
		//fragment_vis[tex2d_xy] = float4(1, 0, 0, 1);
		return;
	}
#endif
	// Ray Intersection for Clipping Box //
    float2 hits_t = ComputeVBoxHits(vbos_hit_start_pos, dir_sample_unit_ws, g_cbVobj.mat_alignedvbox_tr_ws2bs, g_cbClipInfo);
	// 1st Exit in the case that there is no ray-intersecting boundary in the volume box
	hits_t.y = min(g_cbCamState.far_plane, hits_t.y); // only available in orthogonal view (thickness slicer)
    int num_ray_samples = (int) ((hits_t.y - hits_t.x) / g_cbVobj.sample_dist + 0.5f);
    if (num_ray_samples <= 0)
        return;
    float3 pos_ray_start_ws = vbos_hit_start_pos + dir_sample_unit_ws * hits_t.x;
    // recompute the vis result  

	// DVR ray-casting core part
	{
		vis_out = 0;
		depth_out = FLT_MAX;

		float3 pos_ray_start_ts = TransformPoint(pos_ray_start_ws, g_cbVobj.mat_ws2ts);
		float3 dir_sample_ts = TransformVector(dir_sample_ws, g_cbVobj.mat_ws2ts);

		float depth_hit = depth_out = length(pos_ray_start_ws - pos_ip_ws);

		// note that the gradient normal direction faces to the inside
		float3 light_dirinv = -g_cbEnv.dir_light_ws;
		if (g_cbEnv.env_flag & 0x1)
			light_dirinv = -normalize(pos_ray_start_ws - g_cbEnv.pos_light_ws);

		uint idx_dlayer = 0;
		{
			// care for clip plane ... 
		}

		float sample_dist = g_cbVobj.sample_dist;
		float3 view_dir = normalize(dir_sample_ws);
		//vis_out = float4(TransformPoint(pos_ray_start_ws, g_cbVobj.mat_ws2ts), 1);
		//return;

#if FRAG_MERGING == 1
		Fragment f_dly = fs[0]; // if no frag, the z-depth is infinite
#endif

		int start_idx = 0, sample_v = 0;
		if (vr_hit_enc == 2)
		{
			start_idx = 1;
			float4 vis_otf = (float4) 0;
			if (Vis_Volume_And_Check(vis_otf, sample_v, pos_ray_start_ts))
			{
				float4 vis_sample = vis_otf;
				float depth_sample = depth_hit;
#if VR_MODE == 2
				float grad_len;
				GRAD_NRL_VOL(pos_ray_start_ts, dir_sample_ws, grad_len);
				//float modulator = pow(min(grad_len * g_cbVobj.grad_scale / g_cbVobj.grad_max, 1.f), pow(g_cbVobj.kappa_i, g_cbVobj.kappa_s));
				float modulator = min(grad_len * g_cbVobj.value_range * g_cbVobj.grad_scale / g_cbVobj.grad_max, 1.f);
				vis_sample *= modulator;
#endif
				//vis_sample *= mask_weight;
#if FRAG_MERGING == 1
				INTERMIX(vis_out, idx_dlayer, num_frags, vis_sample, depth_sample, sample_dist, fs, merging_beta);
#else
				INTERMIX_V1(vis_out, idx_dlayer, num_frags, vis_sample, depth_sample, fs);
#endif
			}
		}

		[loop]
		for (int i = start_idx; i < num_ray_samples; i++)
		{
			float3 pos_sample_ts = pos_ray_start_ts + dir_sample_ts * (float)i;

			LOAD_BLOCK_INFO(blkSkip, pos_sample_ts, dir_sample_ts, num_ray_samples, i);

			if (blkSkip.blk_value > 0)
			{
				[loop]
				for (int j = 0; j < blkSkip.num_skip_steps; j++, i++)
				{
					//float3 pos_sample_blk_ws = pos_hit_ws + dir_sample_ws * (float) i;
					float3 pos_sample_blk_ts = pos_ray_start_ts + dir_sample_ts * (float)i;

					float4 vis_otf = (float4) 0;
					if (Vis_Volume_And_Check(vis_otf, sample_v, pos_sample_blk_ts))
					{
						float grad_len;
						float3 nrl = GRAD_NRL_VOL(pos_sample_blk_ts, dir_sample_ws, grad_len);
						float shade = 1.f;
						if (grad_len > 0)
							shade = PhongBlinnVr(view_dir, g_cbVobj.pb_shading_factor, light_dirinv, nrl, true);
						//vis_otf *= 0.01;
						float4 vis_sample = float4(shade * vis_otf.rgb, vis_otf.a);
						float depth_sample = depth_hit + (float)i * sample_dist;
#if VR_MODE == 2
						float __s = abs(dot(view_dir, nrl));
						//g_cbVobj.kappa_i
						//float modulator = pow(min(grad_len * g_cbVobj.value_range * g_cbVobj.grad_scale / g_cbVobj.grad_max, 1.f), pow(g_cbVobj.kappa_i * max(__s, 0.1f), g_cbVobj.kappa_s));
						float modulator = min(grad_len * g_cbVobj.value_range * g_cbVobj.grad_scale / g_cbVobj.grad_max, 1.f);
						vis_sample *= modulator;
#endif
						//vis_sample *= mask_weight;
#if FRAG_MERGING == 1
						INTERMIX(vis_out, idx_dlayer, num_frags, vis_sample, depth_sample, sample_dist, fs, merging_beta);
#else
						INTERMIX_V1(vis_out, idx_dlayer, num_frags, vis_sample, depth_sample, fs);
#endif
						if (vis_out.a > ERT_ALPHA)
						{
							i = num_ray_samples;
							j = num_ray_samples;
							break;
						}
					} // if(sample valid check)
				} // for (int j = 0; j < blkSkip.num_skip_steps; j++, i++)
			}
			else
			{
				i += blkSkip.num_skip_steps;
			}
			// this is for outer loop's i++
			i -= 1;
		}

		vis_out.rgb *= (1.f - ao_vr);
		REMAINING_MIX(vis_out, idx_dlayer, num_frags, fs);
	}

	//vis_out = float4(ao_vr, ao_vr, ao_vr, 1);
	//vis_out = float4(TransformPoint(pos_ray_start_ws, g_cbVobj.mat_ws2ts), 1);
    fragment_vis[tex2d_xy] = vis_out;
    fragment_zdepth[tex2d_xy] = depth_out;
	//fragment_counter[DTid.xy] = num_frags + 1;
}

[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void VR_SURFACE(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	int2 tex2d_xy = int2(DTid.xy);

	vr_fragment_1sthit_write[DTid.xy] = FLT_MAX;

	if (DTid.x >= g_cbCamState.rt_width || DTid.y >= g_cbCamState.rt_height)
		return;

	// Image Plane's Position and Camera State //
	float3 pos_ip_ss = float3(tex2d_xy, 0.0f);
	float3 pos_ip_ws = TransformPoint(pos_ip_ss, g_cbCamState.mat_ss2ws);
	float3 dir_sample_unit_ws = g_cbCamState.dir_view_ws;
	if (g_cbCamState.cam_flag & 0x1)
		dir_sample_unit_ws = pos_ip_ws - g_cbCamState.pos_cam_ws;
	dir_sample_unit_ws = normalize(dir_sample_unit_ws);
	float3 dir_sample_ws = dir_sample_unit_ws * g_cbVobj.sample_dist;

	// Ray Intersection for Clipping Box //
	float2 hits_t = ComputeVBoxHits(pos_ip_ws, dir_sample_unit_ws, g_cbVobj.mat_alignedvbox_tr_ws2bs, g_cbClipInfo);
	// 1st Exit in the case that there is no ray-intersecting boundary in the volume box
	hits_t.y = min(g_cbCamState.far_plane, hits_t.y); // only available in orthogonal view (thickness slicer)
	int num_ray_samples = (int)((hits_t.y - hits_t.x) / g_cbVobj.sample_dist + 0.5f);
	if (num_ray_samples <= 0)
		return;

	int sample_v = 0;
	int hit_step = -1;
	float3 pos_start_ws = pos_ip_ws + dir_sample_unit_ws * hits_t.x;
	Compute_1stHit(hit_step, sample_v, pos_start_ws, dir_sample_ws, num_ray_samples);
	if (hit_step < 0)
		return;

	float3 pos_hit_ws;
	// note that sample_v is first set by Compute_1stHit
	SurfaceRefinement(pos_hit_ws, sample_v, pos_start_ws + dir_sample_ws * (float)hit_step, dir_sample_ws, SURFACEREFINEMENTNUM);
	float depth_hit = length(pos_hit_ws - pos_ip_ws);

	vr_fragment_1sthit_write[DTid.xy] = depth_hit;
	uint fcnt = fragment_counter[DTid.xy];
	uint dvr_hit_enc = hit_step == 0 ? 2 : 1;
	fragment_counter[DTid.xy] = fcnt | (dvr_hit_enc << 24);
}