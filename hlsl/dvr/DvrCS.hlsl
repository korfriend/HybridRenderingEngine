#include "../CommonShader.hlsl"
#include "../macros.hlsl"

Texture3D tex3D_volume : register(t0);
Texture3D tex3D_volblk : register(t1);
Texture3D tex3D_volmask : register(t2);
Buffer<float4> buf_otf : register(t3); // unorm 으로 변경하기
Buffer<float4> buf_preintotf : register(t13); // unorm 으로 변경하기
Buffer<float4> buf_windowing : register(t4); // not used here.
Buffer<int> buf_ids : register(t5); // Mask OTFs // not used here.
Texture2D<float> vr_fragment_1sthit_read : register(t6);

RWTexture2D<uint> fragment_counter : register(u0);
RWByteAddressBuffer deep_k_buf : register(u1);
RWTexture2D<unorm float4> fragment_vis : register(u2);
RWTexture2D<float> fragment_zdepth : register(u3);

RWTexture2D<float> vr_fragment_1sthit_write : register(u4);

#define AO_MAX_LAYERS 8 

Texture2DArray<unorm float> ao_textures : register(t10);
Texture2D<unorm float> ao_vr_texture : register(t20);

Buffer<float3> buf_curvePoints : register(t30);
Buffer<float3> buf_curveTangents : register(t31);

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

// min_valid_v is g_cbTmap.first_nonzeroalpha_index
#if VR_MODE == 3
bool Sample_Volume_And_Check(inout float sample_v, const float3 pos_sample_ts, const int min_valid_v)
{
	sample_v = tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_sample_ts, 0).r;
	return sample_v > 0;
}
bool Vis_Volume_And_Check(inout float4 vis_otf, const float3 pos_sample_ts)
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
		float4 vis = LoadOtfBuf(samples[m] * g_cbTmap.tmap_size_x, buf_otf, g_cbVobj.opacity_correction);
		vis_otf += vis * fInterpolateWeights[m];
	}
	return vis_otf.a >= FLT_OPACITY_MIN__;
}
#else
bool Sample_Volume_And_Check(inout float sample_v, const float3 pos_sample_ts, const int min_valid_v)
{
	sample_v = tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_sample_ts, 0).r;
    //sample_v = (int) (fsample * g_cbVobj.value_range + 0.5f);
#if OTF_MASK==1
	int mask_vint = (int)(tex3D_volmask.SampleLevel(g_samplerPoint, pos_sample_ts, 0).r * g_cbVobj.mask_value_range + 0.5f);
	//int mask_vint = LoadMaxValueInt(pos_sample_ts, g_cbVobj.vol_size, g_cbVobj.mask_value_range, tex3D_volmask);
	return (sample_v * g_cbTmap.tmap_size_x) >= min_valid_v;
#elif SCULPT_MASK == 1
	int mask_vint = (int)(tex3D_volmask.SampleLevel(g_samplerPoint, pos_sample_ts, 0).r * g_cbVobj.mask_value_range + 0.5f);
	//int mask_vint = LoadMaxValueInt(pos_sample_ts, g_cbVobj.vol_size, g_cbVobj.mask_value_range, tex3D_volmask);
    int sculpt_value = (int) (g_cbVobj.vobj_flag >> 24);
    return (sample_v * g_cbTmap.tmap_size_x) >= min_valid_v && (mask_vint == 0 || mask_vint > sculpt_value);
#else 
    return (sample_v * g_cbTmap.tmap_size_x) >= min_valid_v;
#endif
}

bool Vis_Volume_And_Check(inout float4 vis_otf, inout float sample_v, const float3 pos_sample_ts)
{
	sample_v = tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_sample_ts, 0).r;
#if OTF_MASK==1
	int mask_vint = (int)(tex3D_volmask.SampleLevel(g_samplerPoint, pos_sample_ts, 0).r * g_cbVobj.mask_value_range + 0.5f);
	//int mask_vint = LoadMaxValueInt(pos_sample_ts, g_cbVobj.vol_size, g_cbVobj.mask_value_range, tex3D_volmask);
	vis_otf = LoadOtfBufId(sample_v * g_cbTmap.tmap_size_x, buf_otf, g_cbVobj.opacity_correction, mask_vint);
	return vis_otf.a >= FLT_OPACITY_MIN__;//&& mask_vint > 0;//&& mask_vint == 3;
#elif SCULPT_MASK == 1
	int mask_vint = (int)(tex3D_volmask.SampleLevel(g_samplerPoint, pos_sample_ts, 0).r * g_cbVobj.mask_value_range + 0.5f);
	//int mask_vint = LoadMaxValueInt(pos_sample_ts, g_cbVobj.vol_size, g_cbVobj.mask_value_range, tex3D_volmask);
    int sculpt_value = (int) (g_cbVobj.vobj_flag >> 24);
    vis_otf = LoadOtfBuf(sample_v * g_cbTmap.tmap_size_x, buf_otf, g_cbVobj.opacity_correction);
    return ((uint)(vis_otf.a * 255.f) > 0) && (mask_vint == 0 || mask_vint > sculpt_value);
#else 
	vis_otf = LoadOtfBuf(sample_v * g_cbTmap.tmap_size_x, buf_otf, g_cbVobj.opacity_correction);
    return vis_otf.a >= FLT_OPACITY_MIN__;
#endif
}

bool Vis_Volume_And_Check_Slab(inout float4 vis_otf, inout float sample_v, float sample_prev, const float3 pos_sample_ts)
{
	sample_v = tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_sample_ts, 0).r;

#if OTF_MASK==1
	int mask_vint = (int)(tex3D_volmask.SampleLevel(g_samplerPoint, pos_sample_ts, 0).r * g_cbVobj.mask_value_range + 0.5f);
	//int mask_vint = LoadMaxValueInt(pos_sample_ts, g_cbVobj.vol_size, g_cbVobj.mask_value_range, tex3D_volmask);

	//float3 pos_vs = float3(pos_sample_ts.x * g_cbVobj.mask_vol_size.x - 0.5f,
	//	pos_sample_ts.y * g_cbVobj.mask_vol_size.y - 0.5f,
	//	pos_sample_ts.z * g_cbVobj.mask_vol_size.z - 0.5f);
	//int3 idx_vs = int3(pos_vs);
	//float mask_vints[8];
	//const int offset = 2;
	//mask_vints[0] = tex3D_volmask.Load(int4(idx_vs, 0)).r * g_cbVobj.mask_value_range + 0.5f;
	//mask_vints[1] = tex3D_volmask.Load(int4(idx_vs + int3(offset, 0, 0), 0)).r * g_cbVobj.mask_value_range + 0.5f;
	//mask_vints[2] = tex3D_volmask.Load(int4(idx_vs + int3(0, offset, 0), 0)).r * g_cbVobj.mask_value_range + 0.5f;
	//mask_vints[3] = tex3D_volmask.Load(int4(idx_vs + int3(offset, offset, 0), 0)).r * g_cbVobj.mask_value_range + 0.5f;
	//mask_vints[4] = tex3D_volmask.Load(int4(idx_vs + int3(0, 0, offset), 0)).r * g_cbVobj.mask_value_range + 0.5f;
	//mask_vints[5] = tex3D_volmask.Load(int4(idx_vs + int3(offset, 0, offset), 0)).r * g_cbVobj.mask_value_range + 0.5f;
	//mask_vints[6] = tex3D_volmask.Load(int4(idx_vs + int3(0, offset, offset), 0)).r * g_cbVobj.mask_value_range + 0.5f;
	//mask_vints[7] = tex3D_volmask.Load(int4(idx_vs + int3(offset, offset, offset), 0)).r * g_cbVobj.mask_value_range + 0.5f;
	//
	//// 41 or 0 번 
	//for (int i = 0; i < 8; i++) {
	//	if ((int)mask_vints[i] == 41) {
	//		if ((int)mask_vints[0] == 41 || (int)mask_vints[0] == 0) 
	//		{
	//			vis_otf = (float4)0;
	//			return false;
	//		}
	//	}
	//}
	//


	vis_otf = LoadSlabOtfBufId_PreInt(sample_v * g_cbTmap.tmap_size_x, sample_prev * g_cbTmap.tmap_size_x, buf_preintotf, g_cbVobj.opacity_correction, mask_vint);
	//vis_otf = LoadOtfBufId(sample_v * g_cbTmap.tmap_size_x, buf_otf, g_cbVobj.opacity_correction, mask_vint);

	//if (g_cbTmap.tmap_size_x * fsample < g_cbTmap.first_nonzeroalpha_index
	//	|| g_cbTmap.tmap_size_x * fsample_prev < g_cbTmap.first_nonzeroalpha_index) return false;

	//vis_otf = LoadSlabOtfBufId_PreInt(fsample * g_cbTmap.tmap_size_x, fsample_prev * g_cbTmap.tmap_size_x, buf_preintotf, g_cbVobj.opacity_correction, mask_vint);
	//if (mask_vint == 0) sample_v = 0;
	//vis_otf = LoadSlabOtfBuf_PreInt(fsample * g_cbTmap.tmap_size_x, fsample_prev * g_cbTmap.tmap_size_x, buf_preintotf, g_cbVobj.opacity_correction);// g_cbVobj.opacity_correction);

	return vis_otf.a >= FLT_OPACITY_MIN__;//&& mask_vint > 0;//&& mask_vint == 3;

#elif SCULPT_MASK == 1
	int mask_vint = (int)(tex3D_volmask.SampleLevel(g_samplerPoint, pos_sample_ts, 0).r * g_cbVobj.mask_value_range + 0.5f);
	//int mask_vint = LoadMaxValueInt(pos_sample_ts, g_cbVobj.vol_size, 255, tex3D_volmask);
	int sculpt_value = (int)(g_cbVobj.vobj_flag >> 24);
	vis_otf = LoadSlabOtfBuf_PreInt(sample_v * g_cbTmap.tmap_size_x, sample_prev * g_cbTmap.tmap_size_x, buf_preintotf, g_cbVobj.opacity_correction);
	return ((uint)(vis_otf.a * 255.f) > 0) && (mask_vint == 0 || mask_vint > sculpt_value);
#else 
	vis_otf = LoadSlabOtfBuf_PreInt(sample_v * g_cbTmap.tmap_size_x, sample_prev * g_cbTmap.tmap_size_x, buf_preintotf, g_cbVobj.opacity_correction);
	return vis_otf.a >= FLT_OPACITY_MIN__;
#endif
}
#endif

#define ITERATION_REFINESURFACE 5
void Find1stSampleHit(inout int step, const float3 pos_ray_start_ws, const float3 dir_sample_ws, const int num_ray_samples)
{
    step = -1;
    float sample_v = 0;

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
            for (int k = 0; k <= blkSkip.num_skip_steps; k++)
            {
                float3 pos_sample_blk_ts = pos_ray_start_ts + dir_sample_ts * (float) (i + k);
                if (Sample_Volume_And_Check(sample_v, pos_sample_blk_ts, min_valid_v))
                {
					step = i + k;
					i = num_ray_samples;
                    k = num_ray_samples;
                    break;
                } // if(sample valid check)
            } // for(int k = 0; k < blkSkip.iNumStepSkip; k++, i++)
        }
        //else
        //{
        //    i += blkSkip.num_skip_steps;
        //}
		i += blkSkip.num_skip_steps;
		// this is for outer loop's i++
        //i -= 1;
    }
}

// 
void FindNearestInsideSurface(inout float3 pos_refined_ws, const float3 pos_sample_ws, const float3 dir_sample_ws, const int num_refinement)
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
        float _sample_v = 0;
        if (Sample_Volume_And_Check(_sample_v, pos_bis_ts, min_valid_v))
        {
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
#if VR_MODE != 3
	if (dot(cam_view, normal) >= 0)
		normal *= -1.f;
#endif
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

[numthreads(GRIDSIZE_VR, GRIDSIZE_VR, 1)]
void RayCasting(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
    uint2 tex2d_xy = uint2(DTid.xy);
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
#define __VRHIT_ON_CLIPPLANE 2
#define __VRHIT_OUTSIDE_CLIPPLANE 1
#define __VRHIT_OUTSIDE_VOLUME 0
	// 2 : on the clip plane
	// 1 : outside the clip plane
	// 0 : outside the volume
	uint vr_hit_enc = num_frags >> 24;
	num_frags = num_frags & 0xFFF;

	bool isDither = BitCheck(g_cbCamState.cam_flag, 8);
	if (isDither) {
#if RAYMODE == 0
		if (vr_hit_enc != __VRHIT_OUTSIDE_VOLUME)
#endif
		{
			if (tex2d_xy.x % 2 != 0 || tex2d_xy.y % 2 != 0) {
				fragment_zdepth[tex2d_xy] = -777.0;
				return;
			}
		}

		//fragment_vis[tex2d_xy] = float4(1, 0, 0, 1);
		//return;
	}

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
    for (int i = 0; i < (int)num_frags; i++)
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

	//fragment_vis[tex2d_xy] = float4(1, 1, 0, 1);
	//if(num_frags > 0)
	//	//fragment_vis[tex2d_xy] = ConvertUIntToFloat4(fs[0].i_vis);
	//	fragment_vis[tex2d_xy] = float4(1, 0, 0, 1);
	//return;

	//return;
	//if (num_frags == 0)
	//	fragment_vis[tex2d_xy] = float4(0, 0, 0, 1);
	//if (num_frags == 1)
	//	fragment_vis[tex2d_xy] = float4(1, 0, 0, 1);
	//if (num_frags == 2)
	//	fragment_vis[tex2d_xy] = float4(0, 1, 0, 1);
	//if (num_frags == 3)
	//	fragment_vis[tex2d_xy] = float4(0, 0, 1, 1);
	//if (num_frags == 4)
	//	fragment_vis[tex2d_xy] = float4(1, 1, 0, 1);
	//if (num_frags == 5)
	//	fragment_vis[tex2d_xy] = float4(0, 1, 1, 1);
	//if (num_frags == 6)
	//	fragment_vis[tex2d_xy] = float4(1, 0, 1, 1);
	//if (num_frags == 7)
	//	fragment_vis[tex2d_xy] = float4(1, 1, 1, 1);
	//return;

	fs[num_frags] = (Fragment)0;
	fs[num_frags].z = FLT_MAX;
    fragment_vis[tex2d_xy] = vis_out;
#if RAYMODE != 0
    fragment_zdepth[tex2d_xy] = depth_out = fs[0].z;
#endif

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
	// use the result from VR_SURFACE
	depth_out = vr_fragment_1sthit_read[DTid.xy];
	
	float4 outline_test = ConvertUIntToFloat4(g_cbVobj.outline_color);
	if (outline_test.w > 0 && vr_hit_enc == __VRHIT_OUTSIDE_VOLUME) {
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

	if (vr_hit_enc == __VRHIT_OUTSIDE_VOLUME)//depth_out > FLT_LARGE)
	{
		// fragment_vis[tex2d_xy] = float4(1, 0, 0, 1);
		return;
	}
	
#endif
	// Ray Intersection for Clipping Box //
    float2 hits_t = ComputeVBoxHits(vbos_hit_start_pos, dir_sample_unit_ws, g_cbVobj.mat_alignedvbox_tr_ws2bs, g_cbClipInfo);
	// 1st Exit in the case that there is no ray-intersecting boundary in the volume box
	hits_t.y = min(g_cbCamState.far_plane, hits_t.y); // only available in orthogonal view (thickness slicer)
    int num_ray_samples = ceil((hits_t.y - hits_t.x) / g_cbVobj.sample_dist);
	if (num_ray_samples <= 0)
		return;
	// note hits_t.x >= 0
    float3 pos_ray_start_ws = vbos_hit_start_pos + dir_sample_unit_ws * hits_t.x;
    // recompute the vis result  

	// DVR ray-casting core part
#if RAYMODE == 0 // DVR

	vis_out = 0;
	depth_out = FLT_MAX;

	float depth_hit = depth_out = length(pos_ray_start_ws - pos_ip_ws);
	float sample_dist = g_cbVobj.sample_dist;

	float3 pos_ray_start_ts = TransformPoint(pos_ray_start_ws, g_cbVobj.mat_ws2ts);
	float3 dir_sample_ts = TransformVector(dir_sample_ws, g_cbVobj.mat_ws2ts);

	// check pos_ray_start_ws!!!!!!!!!!!!!!!! 
	//fragment_vis[tex2d_xy] = float4((pos_ray_start_ts * 3 + (float3)1) * 0.5f, 1.f);
	//return;

	// check num_ray_samples/!!!
	// test //
	//float3 pos_sample_ts = pos_ray_start_ts;
	//float4 __vis_otf = (float4) 0;
	//int vv;
	//Vis_Volume_And_Check(__vis_otf, vv, pos_sample_ts);
	//float ttt = num_ray_samples / 200.f;
	//fragment_vis[tex2d_xy] = float4((float3)ttt, 1);// float4((pos_ray_start_ts + float3(1, 1, 1)) / 2, 1);
	//return;
	//////////

	// note that the gradient normal direction faces to the inside
	float3 light_dirinv = -g_cbEnv.dir_light_ws;
	if (g_cbEnv.env_flag & 0x1)
		light_dirinv = -normalize(pos_ray_start_ws - g_cbEnv.pos_light_ws);

	uint idx_dlayer = 0;
	{
		// care for clip plane ... 
	}

	float3 view_dir = normalize(dir_sample_ws);
	//vis_out = float4(TransformPoint(pos_ray_start_ws, g_cbVobj.mat_ws2ts), 1);
	//return;

#if FRAG_MERGING == 1
	Fragment f_dly = fs[0]; // if no frag, the z-depth is infinite
#endif

	int start_idx = 0;
	float sample_v = tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_ray_start_ts, 0).r;

#if VR_MODE != 3
	// note that raycasters except vismask mode (or x-ray) use SLAB sample
	start_idx = 1;
	//float3 grad_prev = GRAD_VOL(pos_ray_start_ts);
	float3 grad_prev = GradientVolume2(pos_ray_start_ts, g_cbVobj.vec_grad_x, g_cbVobj.vec_grad_y, g_cbVobj.vec_grad_z, tex3D_volume);
	float sample_prev = sample_v;
#endif
	
	if (vr_hit_enc == __VRHIT_ON_CLIPPLANE) // on the clip plane
	{
		//fragment_vis[tex2d_xy] = float4(1, 0, 0, 1);
		//return;
		float4 vis_otf = (float4) 0;
		start_idx++;

#if VR_MODE != 3
		float3 pos_sample_ts = pos_ray_start_ts + dir_sample_ts;

		sample_v = tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_sample_ts, 0).r;
		float3 grad = GRAD_VOL(pos_sample_ts);
		float3 gradSlab = grad + grad_prev;
		grad_prev = grad;
		if (Vis_Volume_And_Check_Slab(vis_otf, sample_v, sample_prev, pos_sample_ts))
#else
		if (Vis_Volume_And_Check(vis_otf, pos_ray_start_ts))
#endif
		{
			float depth_sample = depth_hit;
#if VR_MODE != 3
			// note that depth_hit is the front boundary of slab
			depth_sample += sample_dist; // slab's back boundary
#endif

			float4 vis_sample = vis_otf;
#if VR_MODE == 2
			float grad_len = length(gradSlab) * 0.5f;
			//float modulator = pow(min(grad_len * g_cbVobj.grad_scale / g_cbVobj.grad_max, 1.f), pow(g_cbVobj.kappa_i, g_cbVobj.kappa_s));
			//float modulator = min(grad_len * g_cbVobj.value_range * g_cbVobj.grad_scale / g_cbVobj.grad_max, 1.f);
			//vis_sample *= modulator; // https://github.com/korfriend/OsstemCoreAPIs/discussions/199#discussion-5114460
#endif
			//vis_sample *= mask_weight;
#if FRAG_MERGING == 1
			INTERMIX(vis_out, idx_dlayer, num_frags, vis_sample, depth_sample, sample_dist, fs, merging_beta);
#else
			INTERMIX_V1(vis_out, idx_dlayer, num_frags, vis_sample, depth_sample, fs);
#endif
		}
	}
	/**/
	int sample_count = 0;

#if VR_MODE != 3
	bool isPrevValid = true;
#endif

	[loop]
	for (i = start_idx; i < num_ray_samples; i++)
	{
		float3 pos_sample_ts = pos_ray_start_ts + dir_sample_ts * (float)i;

		LOAD_BLOCK_INFO(blkSkip, pos_sample_ts, dir_sample_ts, num_ray_samples, i);

		if (blkSkip.blk_value > 0)
		{
			[loop]
			for (int j = 0; j <= blkSkip.num_skip_steps; j++)
			{
				//float3 pos_sample_blk_ws = pos_hit_ws + dir_sample_ws * (float) i;
				float3 pos_sample_blk_ts = pos_ray_start_ts + dir_sample_ts * (float)(i + j);

				float4 vis_otf = (float4) 0;
#if VR_MODE != 3
				//if (!isPrevValid) {
				//	//sample_prev = sample_v;
				//	float3 pos_prev_sample_blk_ts = pos_sample_blk_ts - dir_sample_ts;
				//	//grad_prev = GRAD_VOL(pos_prev_sample_blk_ts);
				//	sample_prev = tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_prev_sample_blk_ts, 0).r;
				//}
				if (Vis_Volume_And_Check_Slab(vis_otf, sample_v, sample_prev, pos_sample_blk_ts))
				//if (Vis_Volume_And_Check(vis_otf, sample_v, pos_sample_blk_ts))
				{
					float3 grad = GRAD_VOL(pos_sample_blk_ts);
					if (!isPrevValid) 
					{
						float3 pos_prev_sample_blk_ts = pos_sample_blk_ts - dir_sample_ts;
						grad_prev = GRAD_VOL(pos_prev_sample_blk_ts);
						//grad_prev = grad;// GRAD_VOL(pos_prev_sample_blk_ts);
					}
					isPrevValid = true;
					
					float3 gradSlab = grad + grad_prev;
					grad_prev = grad;
					
					float grad_len = length(gradSlab);
					float3 nrl = gradSlab / (grad_len + 0.00001f);
#else
				if (Vis_Volume_And_Check(vis_otf, pos_sample_blk_ts))
				{
					float grad_len;
					float3 nrl = GRAD_NRL_VOL(pos_sample_blk_ts, dir_sample_ws, grad_len);
#endif
					float shade = 1.f;
					if (grad_len > 0)
						shade = PhongBlinnVr(view_dir, g_cbVobj.pb_shading_factor, light_dirinv, nrl, true);

					float4 vis_sample = float4(shade * vis_otf.rgb, vis_otf.a);
					float depth_sample = depth_hit + (float)(i + j) * sample_dist;
#if VR_MODE == 2
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
					if (vis_out.a >= ERT_ALPHA)
					{
						i = num_ray_samples;
						j = num_ray_samples;
						vis_out.a = 1.f;
						break;
					}
				} // if(sample valid check)
#if VR_MODE != 3
				else {
					isPrevValid = false;
				}
				sample_prev = sample_v;
#endif
			} // for (int j = 0; j < blkSkip.num_skip_steps; j++, i++)
		}
		else
		{
			sample_count++;
#if VR_MODE != 3
			sample_prev = 0;
			isPrevValid = false;
			grad_prev = (float3)0;
#endif
		}
		i += blkSkip.num_skip_steps;
		// this is for outer loop's i++
		//i -= 1;
	}

	vis_out.rgb *= (1.f - ao_vr);
	REMAINING_MIX(vis_out, idx_dlayer, num_frags, fs);
#else // RAYMODE != 0
	float3 pos_ray_start_ts = TransformPoint(pos_ray_start_ws, g_cbVobj.mat_ws2ts);
	float depth_begin = depth_out = length(pos_ray_start_ws - pos_ip_ws);
	float3 dir_sample_ts = TransformVector(dir_sample_ws, g_cbVobj.mat_ws2ts);
#if RAYMODE==1 || RAYMODE==2
	int luckyStep = (int)((float)(Random(pos_ray_start_ws.xy) + 1) * (float)num_ray_samples * 0.5f);
	float depth_sample = depth_begin + g_cbVobj.sample_dist * (float)(luckyStep);
	float3 pos_lucky_sample_ws = pos_ray_start_ws + dir_sample_ws * (float)luckyStep;
	float3 pos_lucky_sample_ts = TransformPoint(pos_lucky_sample_ws, g_cbVobj.mat_ws2ts);
#if RAYMODE==2
	float sample_v_prev = 1.f;// g_cbVobj.value_range;
#else
	float sample_v_prev = 0;/// tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_lucky_sample_ts, 0).r* g_cbVobj.value_range;
#endif

#if OTF_MASK == 1
	float3 pos_mask_sample_ts = pos_lucky_sample_ts;
#endif

#else // ~(RAYMODE==1 || RAYMODE==2) ... RAYMODE==3
	float depth_sample = depth_begin + g_cbVobj.sample_dist * (float)(num_ray_samples);
	int num_valid_samples = 0;
	float4 vis_otf_sum = (float4)0;
#endif

	int count = 0;
	[loop]
	for (i = 0; i < num_ray_samples; i++)
	{
		float3 pos_sample_ts = pos_ray_start_ts + dir_sample_ts * (float)i;

#if RAYMODE == 1 || RAYMODE == 2
		LOAD_BLOCK_INFO(blkSkip, pos_sample_ts, dir_sample_ts, num_ray_samples, i);
#if RAYMODE == 1
		if (blkSkip.blk_value > sample_v_prev)
#elif RAYMODE == 2
		if (blkSkip.blk_value < sample_v_prev)
#endif
		{
			count++;
			for (int k = 0; k <= blkSkip.num_skip_steps; k++)
			{
				float3 pos_sample_in_blk_ts = pos_ray_start_ts + dir_sample_ts * (float)(i + k);
				float sample_v = tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_sample_in_blk_ts, 0).r;// *g_cbVobj.value_range;
#if RAYMODE == 1
				if (sample_v > sample_v_prev)
#else	// ~RM_RAYMAX
				if (sample_v < sample_v_prev)
#endif
				{
#if OTF_MASK == 1
					pos_mask_sample_ts = pos_sample_in_blk_ts;
#endif
					sample_v_prev = sample_v;
					depth_sample = depth_begin + g_cbVobj.sample_dist * (float)i;
				}
			}
		}
		i += blkSkip.num_skip_steps;
		// this is for outer loop's i++
		//i -= 1;
#else	// ~(RAYMODE == 1 || RAYMODE == 2) , which means RAYSUM 
		float sample_v_norm = tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_sample_ts, 0).r;
#if OTF_MASK == 1
		float sample_mask_v = tex3D_volmask.SampleLevel(g_samplerPoint, pos_sample_ts, 0).r * g_cbVolObj.mask_value_range;
		int mask_vint = (int)(sample_mask_v + 0.5f);
		float4 vis_otf = LoadOtfBufId(sample_v_norm * g_cbTmap.tmap_size_x, buf_otf, g_cbVobj.opacity_correction, mask_vint);
#else	// OTF_MASK != 1
		float4 vis_otf = LoadOtfBuf(sample_v_norm * g_cbTmap.tmap_size_x, buf_otf, 1);// g_cbVobj.opacity_correction);
#endif
		// otf sum is necessary for multi-otf case (tooth segmentation-out case)
		//if (vis_otf.a > 0) // results in discontinuous visibility caused by aliasing problem
		{
			vis_otf_sum += vis_otf;
			num_valid_samples++;
		}
#endif
	}

#if RAYMODE == 3
	if (num_valid_samples == 0)
		num_valid_samples = 1;
	float4 vis_otf = vis_otf_sum / (float)num_valid_samples;
#else // RAYMODE != 3

#if OTF_MASK == 1
	float sample_mask_v = tex3D_volmask.SampleLevel(g_samplerPoint, pos_sample_ts, 0).r * g_cbVolObj.mask_value_range;
	int mask_vint = (int)(sample_mask_v + 0.5f);
	float4 vis_otf = LoadOtfBufId(sample_v_prev * g_cbTmap.tmap_size_x, buf_otf, g_cbVobj.opacity_correction, mask_vint);
#else
	float4 vis_otf = LoadOtfBuf(sample_v_prev* g_cbTmap.tmap_size_x, buf_otf, g_cbVobj.opacity_correction);// g_cbVobj.opacity_correction);
#endif
#endif

	uint idx_dlayer = 0;
#if FRAG_MERGING == 1
	Fragment f_dly = fs[0]; // if no frag, the z-depth is infinite
	INTERMIX(vis_out, idx_dlayer, num_frags, vis_otf, depth_begin, hits_t.y - hits_t.x, fs, merging_beta);
#else
	INTERMIX_V1(vis_out, idx_dlayer, num_frags, vis_otf, depth_begin, fs);
#endif
	REMAINING_MIX(vis_out, idx_dlayer, num_frags, fs);
#endif

	//if (count == 0) vis_otf = float4(1, 1, 0, 1);
	//vis_out = float4(ao_vr, ao_vr, ao_vr, 1);
	//vis_out = float4(TransformPoint(pos_ray_start_ws, g_cbVobj.mat_ws2ts), 1);
    fragment_vis[tex2d_xy] = vis_out;
    fragment_zdepth[tex2d_xy] = depth_out;
	//fragment_counter[DTid.xy] = num_frags + 1;


	//float tt = sample_count / 30.f;
	//fragment_vis[tex2d_xy] = float4((float3)tt, 1);// float4((pos_ray_start_ts + float3(1, 1, 1)) / 2, 1);
}

[numthreads(GRIDSIZE_VR, GRIDSIZE_VR, 1)]
void FillDither(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	if (DTid.x >= g_cbCamState.rt_width || DTid.y >= g_cbCamState.rt_height)
		return;

	int2 tex2d_xy = int2(DTid.xy);

	//fragment_vis[tex2d_xy] = float4(1, 0, 0, 1);
	float depth = fragment_zdepth[tex2d_xy];
	if ((DTid.x % 2 == 0 && DTid.y % 2 == 0) || depth != -777.0)
		return;

	//fragment_vis[tex2d_xy] = float4(1, 0, 0, 1);
	//return;

	//fragment_zdepth[tex2d_xy] = depth_out;
	//fragment_vis[tex2d_xy] = float4(1, 0, 0, 1);

	float4 rgbaRendered[4];
	int count = 0;
	if (DTid.x % 2 != 0 && DTid.y % 2 == 0) {
		rgbaRendered[count++] = fragment_vis[tex2d_xy - int2(1, 0)];
		rgbaRendered[count++] = fragment_vis[tex2d_xy + int2(1, 0)];
	}
	else if (DTid.x % 2 == 0 && DTid.y % 2 != 0) {
		rgbaRendered[count++] = fragment_vis[tex2d_xy - int2(0, 1)];
		rgbaRendered[count++] = fragment_vis[tex2d_xy + int2(0, 1)];
	}
	else if (DTid.x % 2 != 0 && DTid.y % 2 != 0) {
		rgbaRendered[count++] = fragment_vis[tex2d_xy - int2(1, 1)];
		rgbaRendered[count++] = fragment_vis[tex2d_xy + int2(1, 1)];
		rgbaRendered[count++] = fragment_vis[tex2d_xy + int2(-1, 1)];
		rgbaRendered[count++] = fragment_vis[tex2d_xy + int2(1, -1)];
	}
	else {
		//fragment_vis[tex2d_xy] = float4(1, 0, 0, 1);
		return;
	}

	float4 v_rgba = (float4)0;
	for (int i = 0; i < count; i++) {
		v_rgba += rgbaRendered[i];
	}
	v_rgba /= count;
	fragment_vis[tex2d_xy] = v_rgba;
}

[numthreads(GRIDSIZE_VR, GRIDSIZE_VR, 1)]
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

	int hit_step = -1;
	float3 pos_start_ws = pos_ip_ws + dir_sample_unit_ws * hits_t.x;
	Find1stSampleHit(hit_step, pos_start_ws, dir_sample_ws, num_ray_samples);
	if (hit_step < 0)
		return;
	
	float3 pos_hit_ws = pos_start_ws + dir_sample_ws * (float)hit_step;
	if (hit_step > 0) {
		FindNearestInsideSurface(pos_hit_ws, pos_hit_ws, dir_sample_ws, ITERATION_REFINESURFACE);
#if VR_MODE != 3
		pos_hit_ws -= dir_sample_ws;
#endif
		if (dot(pos_hit_ws - pos_start_ws, dir_sample_ws) <= 0)
			pos_hit_ws = pos_start_ws;
	}

	float depth_hit = length(pos_hit_ws - pos_ip_ws);

	if (BitCheck(g_cbVolMaterial.flag, 0))
	{
		// additional feature : https://koreascience.kr/article/JAKO201324947256830.pdf
		float rand = _random(float2(DTid.x + g_cbCamState.rt_width * DTid.y, depth_hit));
		depth_hit -= rand * g_cbVobj.sample_dist;
		//float3 random_samples = float3(_random(DTid.x + g_cbCamState.rt_width * DTid.y), _random(DTid.x + g_cbCamState.rt_width * DTid.y + g_cbCamState.rt_width * g_cbCamState.rt_height), _random(DTid.xy));
	}

	vr_fragment_1sthit_write[DTid.xy] = depth_hit;
	uint fcnt = fragment_counter[DTid.xy];
	uint dvr_hit_enc = length(pos_hit_ws - pos_start_ws) < g_cbVobj.sample_dist ? 2 : 1;
	// 2 : on the clip plane
	// 1 : outside the clip plane
	// 0 : outside the volume
	fragment_counter[DTid.xy] = fcnt | (dvr_hit_enc << 24);
}

[numthreads(GRIDSIZE_VR, GRIDSIZE_VR, 1)]
void CurvedSlicer(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	uint2 cip_xy = uint2(DTid.xy);

	// do not compute 1st hit surface separately
	if (DTid.x >= g_cbCamState.rt_width || DTid.y >= g_cbCamState.rt_height)
		return;

	const uint k_value = g_cbCamState.k_value;
	uint bytes_per_frag = 4 * NUM_ELES_PER_FRAG;
	uint pixel_id = cip_xy.y * g_cbCamState.rt_width + cip_xy.x;
	uint bytes_frags_per_pixel = k_value * bytes_per_frag;
	uint addr_base = pixel_id * bytes_frags_per_pixel;

	uint num_frags = fragment_counter[DTid.xy];
	num_frags = num_frags & 0xFFF;


	bool isDither = BitCheck(g_cbCamState.cam_flag, 8);
	if (isDither) {
		if (cip_xy.x % 2 != 0 || cip_xy.y % 2 != 0) {
			fragment_zdepth[cip_xy] = -777.0;
			return;
		}

		//fragment_vis[tex2d_xy] = float4(1, 0, 0, 1);
		//return;
	}

	float4 vis_out = 0;
	float depth_out = 0;

	// Image Plane's Position and Camera State //
	// cip refers to curved-ip

	// g_cbCurvedSlicer::
	//float3 posTopLeftCOS;
	//int numCurvePoints;
	//float3 posTopRightCOS;
	//float planeHeight;
	//float3 posBottomLeftCOS;
	//float thicknessPlane;
	//float3 posBottomRightCOS;
	//uint __dummy0; 
	//float3 planeUp; // WS, length is planePitch
	//uint flag; // 1st bit : isRightSide

	//return;
	Fragment fs[VR_MAX_LAYERS];

	[loop]
	for (int i = 0; i < (int)num_frags; i++)
	{
		uint i_vis = 0;
		Fragment f;
		GET_FRAG(f, addr_base, i); // from K-buffer
		float4 vis_in = ConvertUIntToFloat4(f.i_vis);
		if (g_cbEnv.r_kernel_ao > 0)
			f.i_vis = ConvertFloat4ToUInt(vis_in);
		if (vis_in.a > 0)
			vis_out += vis_in * (1.f - vis_out.a);
		fs[i] = f;
	}

	fs[num_frags] = (Fragment)0;
	fs[num_frags].z = FLT_MAX;
	fragment_vis[cip_xy] = vis_out;

	int2 i2SizeBuffer = int2(g_cbCamState.rt_width, g_cbCamState.rt_height);
	int iPlaneSizeX = g_cbCurvedSlicer.numCurvePoints;
	float3 f3VecSampleUpWS = g_cbCurvedSlicer.planeUp;
	bool bIsRightSide = BitCheck(g_cbCurvedSlicer.flag, 0);
	float3 f3PosTopLeftCOS = g_cbCurvedSlicer.posTopLeftCOS;
	float3 f3PosTopRightCOS = g_cbCurvedSlicer.posTopRightCOS;
	float3 f3PosBottomLeftCOS = g_cbCurvedSlicer.posBottomLeftCOS;
	float3 f3PosBottomRightCOS = g_cbCurvedSlicer.posBottomRightCOS;
	float fPlaneThickness = g_cbCurvedSlicer.thicknessPlane;
	float fPlaneSizeY = g_cbCurvedSlicer.planeHeight;
	float fPlaneCenterY = fPlaneSizeY * 0.5f;
	const float fThicknessPosition = 0;
	const float merging_beta = 1.0;

	// i ==> cip_xy.x
	float fRatio0 = (float)((i2SizeBuffer.x - 1) - cip_xy.x) / (float)(i2SizeBuffer.x - 1);
	float fRatio1 = (float)(cip_xy.x) / (float)(i2SizeBuffer.x - 1);

	float2 f2PosInterTopCOS, f2PosInterBottomCOS, f2PosSampleCOS;
	f2PosInterTopCOS.x = fRatio0 * f3PosTopLeftCOS.x + fRatio1 * f3PosTopRightCOS.x;
	f2PosInterTopCOS.y = fRatio0 * f3PosTopLeftCOS.y + fRatio1 * f3PosTopRightCOS.y;

	if (f2PosInterTopCOS.x < 0 || f2PosInterTopCOS.x >= (float)(iPlaneSizeX - 1))
		return;

	int iPosSampleCOS = (int)floor(f2PosInterTopCOS.x);
	float fInterpolateRatio = f2PosInterTopCOS.x - iPosSampleCOS;

	int iMinMaxAddrX = min(max(iPosSampleCOS, 0), iPlaneSizeX - 1);
	int iMinMaxAddrNextX = min(max(iPosSampleCOS + 1, 0), iPlaneSizeX - 1);

	float3 f3PosSampleWS_C0 = buf_curvePoints[iMinMaxAddrX];
	float3 f3PosSampleWS_C1 = buf_curvePoints[iMinMaxAddrNextX];
	float3 f3PosSampleWS_C_ = f3PosSampleWS_C0 * (1.f - fInterpolateRatio) + f3PosSampleWS_C1 * fInterpolateRatio;

	float3 f3VecSampleTangentWS_0 = buf_curveTangents[iMinMaxAddrX];
	float3 f3VecSampleTangentWS_1 = buf_curveTangents[iMinMaxAddrNextX];
	float3 f3VecSampleTangentWS = normalize(f3VecSampleTangentWS_0 * (1.f - fInterpolateRatio) + f3VecSampleTangentWS_1 * fInterpolateRatio);
	float3 f3VecSampleViewWS = normalize(cross(f3VecSampleUpWS, f3VecSampleTangentWS));

	if (bIsRightSide)
		f3VecSampleViewWS *= -1.f;
	
	float3 f3PosSampleWS_C = f3PosSampleWS_C_ + f3VecSampleViewWS * (fThicknessPosition - fPlaneThickness * 0.5f);
	//f3VecSampleUpWS *= fPlanePitch; // already multiplied

	f2PosInterBottomCOS.x = fRatio0 * f3PosBottomLeftCOS.x + fRatio1 * f3PosBottomRightCOS.x;
	f2PosInterBottomCOS.y = fRatio0 * f3PosBottomLeftCOS.y + fRatio1 * f3PosBottomRightCOS.y;

	// j ==> cip_xy.y
	float fRatio0Y = (float)((i2SizeBuffer.y - 1) - cip_xy.y) / (float)(i2SizeBuffer.y - 1);
	float fRatio1Y = (float)(cip_xy.y) / (float)(i2SizeBuffer.y - 1);

	f2PosSampleCOS.x = fRatio0Y * f2PosInterTopCOS.x + fRatio1Y * f2PosInterBottomCOS.x;
	f2PosSampleCOS.y = fRatio0Y * f2PosInterTopCOS.y + fRatio1Y * f2PosInterBottomCOS.y;

	if (f2PosSampleCOS.y < 0 || f2PosSampleCOS.y > fPlaneSizeY)
		return;

	float sample_dist = g_cbVobj.sample_dist;
	// start position //
	//vmfloat3 f3PosSampleWS = f3PosSampleWS_C + f3VecSampleUpWS * (f2PosSampleCOS.y - fPlaneCenterY)
	//	+ f3VecSampleViewWS * fStepLength * (float)m;
	float3 pos_ray_start_ws = f3PosSampleWS_C + f3VecSampleUpWS * (f2PosSampleCOS.y - fPlaneCenterY);
	float3 dir_sample_ws = f3VecSampleViewWS * sample_dist;

	int num_ray_samples = ceil(fPlaneThickness / sample_dist);
	// DVR ray-casting core part
#if RAYMODE == 0 // DVR
	// note that the gradient normal direction faces to the inside
	float3 view_dir = normalize(dir_sample_ws);
	float3 light_dirinv = -g_cbEnv.dir_light_ws;
	if (g_cbEnv.env_flag & 0x1) {
		light_dirinv = -normalize(pos_ray_start_ws - g_cbEnv.pos_light_ws);
	}
	else if (g_cbEnv.env_flag & 0x4) {
		light_dirinv = -view_dir;
	}

	uint idx_dlayer = 0;
	{
		// care for clip plane ... 
	}

	//vis_out = float4(TransformPoint(pos_ray_start_ws, g_cbVobj.mat_ws2ts), 1);
	//return;

#if FRAG_MERGING == 1
	Fragment f_dly = fs[0]; // if no frag, the z-depth is infinite
#endif

	int hit_step = -1;
	Find1stSampleHit(hit_step, pos_ray_start_ws, dir_sample_ws, num_ray_samples);
	if (hit_step < 0) {
		//fragment_vis[cip_xy] = float4(1, 1, 0, 1);
		return;
	}

	float3 pos_hit_ws = pos_ray_start_ws + dir_sample_ws * (float)hit_step;
	if (hit_step > 0) 
	{
		//FindNearestInsideSurface(pos_hit_ws, pos_hit_ws, dir_sample_ws, ITERATION_REFINESURFACE);
#if VR_MODE != 3
		pos_hit_ws -= dir_sample_ws;
#endif
		if (dot(pos_hit_ws - pos_ray_start_ws, dir_sample_ws) <= 0)
			pos_hit_ws = pos_ray_start_ws;
	}

	float depthHit = length(pos_hit_ws - pos_ray_start_ws);
	bool isOnPlane = depthHit < sample_dist - 0.001; // 0.001 for sample distance precision
	//float remainThickness = fPlaneThickness - depthHit + sample_dist;
	int num_new_ray_samples = int((fPlaneThickness - depthHit) / sample_dist + 0.5);
	//int num_new_ray_samples = int((fPlaneThickness - depthHit) / sample_dist);
	//if(dot(dir_sample_ws, float3(0, 1, 0)) < 0)
	//	fragment_vis[cip_xy] = float4(1, 0, 0, 1);
	//else
	//	fragment_vis[cip_xy] = float4(1, 1, 0, 1);
	//
	//fragment_vis[cip_xy] = float4((float3) ((float)remainThickness / fPlaneThickness), 1);
	//if(num_new_ray_samples > 10)
	//	fragment_vis[cip_xy] = float4(1, 0, 0, 1);
	//return;

	//fragment_vis[cip_xy] = float4((float3)((float)num_new_ray_samples / num_ray_samples), 1);
	//return;

	// note that sample_v is first set by Find1stSampleHit
	float3 pos_ray_hit_ts = TransformPoint(pos_hit_ws, g_cbVobj.mat_ws2ts);
	float3 dir_sample_ts = TransformVector(dir_sample_ws, g_cbVobj.mat_ws2ts);

	// test //
	//float3 pos_sample_ts = pos_ray_start_ts + dir_sample_ts * (float)(0 + 1);
	//float4 __vis_otf = (float4) 0;
	//Vis_Volume_And_Check(__vis_otf, sample_v, pos_sample_ts);
	//fragment_vis[cip_xy] = float4((pos_ray_start_ts + float3(1, 1, 1)) / 2, 1);
	//return;
	//////////

	float sample_v = tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_ray_hit_ts, 0).r;
	int start_idx = 0;

#if VR_MODE != 3
	// note that raycasters except vismask mode (or x-ray) use SLAB sample
	start_idx = 1;
	//float3 grad_prev = GRAD_VOL(pos_ray_hit_ts);
	float3 grad_prev = GradientVolume2(pos_ray_hit_ts, g_cbVobj.vec_grad_x, g_cbVobj.vec_grad_y, g_cbVobj.vec_grad_z, tex3D_volume);
	float sample_prev = sample_v;
#endif

#if VR_MODE != 2
	if (isOnPlane)//
	{
		float4 vis_otf = (float4) 0;
		start_idx++;

#if VR_MODE != 3
		float3 pos_sample_ts = pos_ray_hit_ts + dir_sample_ts;

		//sample_v = tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_sample_ts, 0).r;
		float3 grad = GRAD_VOL(pos_sample_ts);
		float3 gradSlab = grad + grad_prev;
		grad_prev = grad;
		if (Vis_Volume_And_Check_Slab(vis_otf, sample_v, sample_prev, pos_sample_ts))
		//if (Vis_Volume_And_Check(vis_otf, sample_v, pos_sample_ts))
#else
		if (Vis_Volume_And_Check(vis_otf, pos_ray_hit_ts))
#endif
		{
#if VR_MODE != 3
			// note that depth_hit is the front boundary of slab
			//depth_sample += sample_dist; // slab's back boundary
#endif

			float4 vis_sample = vis_otf;
#if VR_MODE == 2
			float grad_len = length(gradSlab);
			float3 nrl = gradSlab / (grad_len + 0.0001f);
			grad_len *= 0.5f;

			float depth_sample = depthHit + sample_dist;
			float __s = grad_len > 0.001f ? abs(dot(view_dir, nrl)) : 0;
			float kappa_t = 5.0;// g_cbVobj.kappa_i;
			float kappa_s = 0.5;// g_cbVobj.kappa_s;
			//float modulator = pow(min(grad_len * g_cbVobj.value_range * g_cbVobj.grad_scale / g_cbVobj.grad_max, 1.f), pow(kappa_i * max(__s, 0.1f), kappa_s));
			//float modulator = pow(min(grad_len * g_cbVobj.value_range * g_cbVobj.grad_scale / g_cbVobj.grad_max, 1.f), pow(kappa_i * max(__s, 0.1f), kappa_s));
			float modulator = min(grad_len * 2.f * g_cbVobj.value_range * g_cbVobj.grad_scale / g_cbVobj.grad_max, 1.f);
			float dist_plane_sq = fPlaneThickness * 0.5f - depth_sample;
			dist_plane_sq /= fPlaneThickness * 0.5f;
			dist_plane_sq *= dist_plane_sq;
			modulator *= pow(max(1.f - dist_plane_sq, 0.1), kappa_t) * pow(max(1.f - __s, 0.1), kappa_s);
			vis_sample *= modulator;
#endif
			//vis_sample *= mask_weight;
			vis_out += vis_sample * (1.f - vis_out.a);
			//fragment_vis[cip_xy] = float4(1, 0, 0, 1);
			//return;
		}
		//fragment_vis[cip_xy] = float4(1, 0, 0, 1);
		//return;
	}
#endif
	/**/
	//fragment_vis[cip_xy] = vis_out;
	//return;

	[loop]
	for (i = start_idx; i < num_new_ray_samples; i++) 
	{
		float3 pos_sample_ts = pos_ray_hit_ts + dir_sample_ts * (float)i;

		LOAD_BLOCK_INFO(blkSkip, pos_sample_ts, dir_sample_ts, num_new_ray_samples, i);

		//if (blkSkip.num_skip_steps == 0) {
		//	vis_out = float4(1, 0, 0, 1);
		//	break;
		//}
		//if (blkSkip.num_skip_steps < 0) {
		//	vis_out = float4(0, 1, 0, 1);
		//	break;
		//}
		//if (blkSkip.num_skip_steps > 100) {
		//	vis_out = float4(1, 1, 0, 1);
		//	break;
		//}
		//break;
		//i += blkSkip.num_skip_steps;
		//continue;

		if (blkSkip.blk_value > 0)
		{
#if VR_MODE != 3
			bool isPrevValid = true;
#endif
			[loop]
			//for (int j = 0; j <= blkSkip.num_skip_steps && i + j < num_new_ray_samples; j++)
			for (int j = 0; j <= blkSkip.num_skip_steps; j++)
			{
				//float3 pos_sample_blk_ws = pos_sample_ts + dir_sample_ws * (float) i;
				float3 pos_sample_blk_ts = pos_ray_hit_ts + dir_sample_ts * (float) (i + j);

				float4 vis_otf = (float4) 0;
#if VR_MODE != 3
				if (Vis_Volume_And_Check_Slab(vis_otf, sample_v, sample_prev, pos_sample_blk_ts))
				//if (Vis_Volume_And_Check(vis_otf, sample_v, pos_sample_blk_ts))
				{
					float3 grad = GRAD_VOL(pos_sample_blk_ts);
					if (!isPrevValid) 
					{
						//float3 pos_prev_sample_blk_ts = pos_sample_blk_ts - dir_sample_ts;
						grad_prev = grad;// GRAD_VOL(pos_prev_sample_blk_ts);
					}
					isPrevValid = true;

					float3 gradSlab = grad + grad_prev;
					grad_prev = grad;

					float grad_len = length(gradSlab);
					float3 nrl = gradSlab / (grad_len + 0.0001f);
					grad_len *= 0.5f;
#else
				if (Vis_Volume_And_Check(vis_otf, pos_sample_blk_ts))
				{
					float grad_len;
					float3 nrl = GRAD_NRL_VOL(pos_sample_blk_ts, dir_sample_ws, grad_len);
#endif
					float shade = 1.f;
#if VR_MODE != 2
					if (grad_len > 0)
						shade = PhongBlinnVr(view_dir, g_cbVobj.pb_shading_factor, light_dirinv, nrl, true);
#endif
					
					float4 vis_sample = float4(shade * vis_otf.rgb, vis_otf.a);
#if VR_MODE == 2
					float depth_sample = depthHit + (float)(i + j) * sample_dist;
					float __s = grad_len > 0.001f ? abs(dot(view_dir, nrl)) : 0;
					float kappa_t = 5.0;// g_cbVobj.kappa_i;
					float kappa_s = 0.5;// g_cbVobj.kappa_s;
					//float modulator = pow(min(grad_len * g_cbVobj.value_range * g_cbVobj.grad_scale / g_cbVobj.grad_max, 1.f), pow(kappa_i * max(__s, 0.1f), kappa_s));
					//float modulator = pow(min(grad_len * g_cbVobj.value_range * g_cbVobj.grad_scale / g_cbVobj.grad_max, 1.f), pow(kappa_i * max(__s, 0.1f), kappa_s));
					//if (grad_len < 0.01f) grad_len = 0;
					float modulator = max(min(grad_len * 2.f * g_cbVobj.value_range * g_cbVobj.grad_scale / g_cbVobj.grad_max, 1.f), 0.01);
					float dist_plane_sq = fPlaneThickness * 0.5f - depth_sample;
					dist_plane_sq /= fPlaneThickness * 0.5f;
					dist_plane_sq *= dist_plane_sq;
					modulator *= pow(max(1.f - dist_plane_sq, 0.1), kappa_t) * pow(max(1.f - __s, 0.1), kappa_s);
					vis_sample *= modulator;
#endif

					vis_out += vis_sample * (1.f - vis_out.a);

					if (vis_out.a >= ERT_ALPHA)
					{
						i = 10000000;//num_ray_samples;
						j = 10000000;// num_ray_samples;
						vis_out.a = 1.f;
						break;
					}
				} // if(sample valid check)
#if VR_MODE != 3
				else {
					isPrevValid = false;
				}
				sample_prev = sample_v;
#endif
			} // for (int j = 0; j < blkSkip.num_skip_steps; j++, i++)
			//i += blkSkip.num_skip_steps;
		} // if (blkSkip.blk_value > 0)
		else
		{
			//i += blkSkip.num_skip_steps;// max(blkSkip.num_skip_steps - 1, 0);
#if VR_MODE != 3
			sample_prev = 0;
			grad_prev = (float3)0;
#endif
		}
		i += blkSkip.num_skip_steps;
		// this is for outer loop's i++
		//i -= 1;
	}

	REMAINING_MIX(vis_out, idx_dlayer, num_frags, fs);

#else // RAYMODE != 0
	float depth_begin = 0;


#if RAYMODE==1 || RAYMODE==2
	int luckyStep = (int)((float)(Random(pos_ray_start_ws.xy) + 1) * (float)num_ray_samples * 0.5f);
	float depth_sample = depth_begin + g_cbVobj.sample_dist * (float)(luckyStep);
	float3 pos_lucky_sample_ws = pos_ray_start_ws + dir_sample_ws * (float)luckyStep;
	float3 pos_lucky_sample_ts = TransformPoint(pos_lucky_sample_ws, g_cbVobj.mat_ws2ts);
#if RAYMODE==2
	float sample_v_prev = 1.f;
#else
	float sample_v_prev = 0;/// tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_lucky_sample_ts, 0).r* g_cbVobj.value_range;
#endif

#if OTF_MASK == 1
	float3 pos_mask_sample_ts = pos_lucky_sample_ts;
#endif

#else // ~(RAYMODE==1 || RAYMODE==2)
	float depth_sample = depth_begin + g_cbVobj.sample_dist * (float)(num_ray_samples);
	int num_valid_samples = 0;
	float4 vis_otf_sum = (float4)0;
	float sampleSum = 0;
#endif
	float3 pos_ray_start_ts = TransformPoint(pos_ray_start_ws, g_cbVobj.mat_ws2ts);
	float3 dir_sample_ts = TransformVector(dir_sample_ws, g_cbVobj.mat_ws2ts);

	int count = 0;
	[loop]
	for (i = 0; i < num_ray_samples; i++)
	{
		float3 pos_sample_ts = pos_ray_start_ts + dir_sample_ts * (float)i;

#if RAYMODE == 1 || RAYMODE == 2
		LOAD_BLOCK_INFO(blkSkip, pos_sample_ts, dir_sample_ts, num_ray_samples, i);
#if RAYMODE == 1
		if (blkSkip.blk_value > sample_v_prev)
#elif RAYMODE == 2
		if (blkSkip.blk_value < sample_v_prev)
#endif
		{
			count++;
			for (int k = 0; k <= blkSkip.num_skip_steps; k++)
			{
				float3 pos_sample_in_blk_ts = pos_ray_start_ts + dir_sample_ts * (float)(i + k);
				float sample_v = tex3D_volume.SampleLevel(g_samplerLinear, pos_sample_in_blk_ts, 0).r;// *g_cbVobj.value_range;
#if RAYMODE == 1
				if (sample_v > sample_v_prev)
#else	// ~RM_RAYMAX
				if (sample_v < sample_v_prev)
#endif
				{
#if OTF_MASK == 1
					pos_mask_sample_ts = pos_sample_in_blk_ts;
#endif
					sample_v_prev = sample_v;
					depth_sample = depth_begin + g_cbVobj.sample_dist * (float)i;
				}
			}
		}
		i += blkSkip.num_skip_steps;
#else	// ~(RAYMODE == 1 || RAYMODE == 2) , which means RAYSUM 
		// use g_samplerLinear instead of g_samplerLinear_clamp
		float sample_v_norm = tex3D_volume.SampleLevel(g_samplerLinear, pos_sample_ts, 0).r;
#if OTF_MASK == 1
		float sample_mask_v = tex3D_volmask.SampleLevel(g_samplerPoint, pos_sample_ts, 0).r * g_cbVolObj.mask_value_range;
		int mask_vint = (int)(sample_mask_v + 0.5f);
		float4 vis_otf = LoadOtfBufId(sample_v_norm * g_cbTmap.tmap_size_x, buf_otf, g_cbVobj.opacity_correction, mask_vint);
#else	// OTF_MASK != 1
		float4 vis_otf = LoadOtfBuf(sample_v_norm * g_cbTmap.tmap_size_x, buf_otf, g_cbVobj.opacity_correction);
#endif
		// https://github.com/korfriend/OsstemCoreAPIs/discussions/185#discussion-4843169
		//if (vis_otf.a > 0.0001)
		//if (sample_v_norm > 0.001)
		{
			sampleSum += sample_v_norm;
			vis_otf_sum += vis_otf;
			num_valid_samples++;
		}
#endif
	}

#if RAYMODE == 3
	if (num_valid_samples == 0)
		num_valid_samples = 1;

	float4 vis_otf = LoadOtfBuf(sampleSum / num_valid_samples * g_cbTmap.tmap_size_x, buf_otf, 1); //float4(pos_sample_ts, 1);
	//if (vis_otf.a > 0.199) vis_otf = float4(1, 0, 0, 1);
	//float4 vis_otf = vis_otf_sum / (float)num_valid_samples;
#else // RAYMODE != 3

#if OTF_MASK == 1
	float sample_mask_v = tex3D_volmask.SampleLevel(g_samplerPoint, pos_sample_ts, 0).r * g_cbVolObj.mask_value_range;
	int mask_vint = (int)(sample_mask_v + 0.5f);
	float4 vis_otf = LoadOtfBufId(sample_v_prev * g_cbTmap.tmap_size_x, buf_otf, g_cbVobj.opacity_correction, mask_vint);
#else
	float4 vis_otf = LoadOtfBuf(sample_v_prev * g_cbTmap.tmap_size_x, buf_otf, g_cbVobj.opacity_correction);
#endif
#endif

	uint idx_dlayer = 0;
	Fragment f_dly = fs[0]; // if no frag, the z-depth is infinite
	INTERMIX(vis_out, idx_dlayer, num_frags, vis_otf, depth_begin, fPlaneThickness, fs, merging_beta);
	REMAINING_MIX(vis_out, idx_dlayer, num_frags, fs);
#endif

	fragment_vis[cip_xy] = vis_out;
	fragment_zdepth[cip_xy] = depth_out;
}
