#include "../CommonShader.hlsl"
#include "../macros.hlsl"

#define VR_ENC_BIT_SHIFT 30

Texture3D tex3D_volume : register(t0);
Texture3D tex3D_volblk : register(t1);
Texture3D tex3D_volmask : register(t2);
Buffer<float4> buf_otf : register(t3); // unorm 으로 변경하기
Buffer<float4> buf_preintotf : register(t13); // unorm 으로 변경하기
Buffer<float4> buf_windowing : register(t4); // not used here.
Buffer<int> buf_ids : register(t5); // Mask OTFs // not used here.
Texture2D<float> vr_fragment_1sthit_read : register(t6);
Buffer<uint> sculpt_bits : register(t7); 

#if DX10_0 == 1
Texture2D prev_fragment_vis : register(t20); 
Texture2D<float> prev_fragment_zdepth : register(t21);
Texture2D<uint> vrHitEnc : register(t22);
struct VS_OUTPUT
{
	float4 f4PosSS : SV_POSITION;
	float3 f3VecNormalWS : NORMAL;
	float3 f3PosWS : TEXCOORD0;
	float3 f3Custom : TEXCOORD1;
};
struct PS_FILL_OUTPUT
{
	float4 color : SV_TARGET0; // UNORM
	float depthcs : SV_TARGET1;

	//float ds_z : SV_Depth;
};
struct PS_FILL_OUTPUT_SURF
{
	uint enc : SV_TARGET0; // UNORM
	float depthcs : SV_TARGET1;
};
#else
RWTexture2D<uint> fragment_counter : register(u0);
RWByteAddressBuffer deep_dynK_buf : register(u1);
RWTexture2D<unorm float4> fragment_vis : register(u2);
RWTexture2D<float> fragment_zdepth : register(u3);
RWTexture2D<float> vr_fragment_1sthit_write : register(u4);	// use this as a z-thickness in Single layer mode!
RWTexture2D<float> fragment_zthick : register(u5);
#endif

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
	[branch]
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
		float alpha = min((float)count / (w * w / 2.f), 1.f);
		//alpha *= alpha;

		vout = float4(edge_color * alpha, alpha);
		depth_c = depth_min;
		//vout = float4(nor_c, 1);
		//vout = float4(depth_h0 / 40, depth_h0 / 40, depth_h0 / 40, 1);
	}

	return vout;
}

Buffer<uint> sr_offsettable_buf : register(t50);// gres_fb_ref_pidx

#define VR_MAX_LAYERS 10 // SKBTZ, +1 for DVR 

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
	int mask_vint = (int)(tex3D_volmask.SampleLevel(g_samplerPoint_clamp, pos_sample_ts, 0).r * g_cbVobj.mask_value_range + 0.5f);
	
	return (sample_v * g_cbTmap.tmap_size_x) >= min_valid_v;
#elif SCULPT_MASK == 1
	int mask_vint = (int)(tex3D_volmask.SampleLevel(g_samplerPoint_clamp, pos_sample_ts, 0).r * g_cbVobj.mask_value_range + 0.5f);
    
	int sculpt_value = (int)(g_cbVobj.vobj_flag >> 24);
	return (sample_v * g_cbTmap.tmap_size_x) >= min_valid_v && (mask_vint == 0 || mask_vint > sculpt_value);
#else 
#if SCULPT_BITS == 1
	int3 voxel_id = (int3)(pos_sample_ts * (g_cbVobj.vol_original_size - uint3(1, 1, 1)));
	int wwh = g_cbVobj.vol_original_size.x * g_cbVobj.vol_original_size.y;
	uint bit_id = voxel_id.x + voxel_id.y * g_cbVobj.vol_original_size.x + voxel_id.z * wwh;
	uint mod = bit_id % 32;
	bool visible = !(bool)(sculpt_bits[bit_id / 32] & (0x1u << mod));
	return (sample_v * g_cbTmap.tmap_size_x) >= min_valid_v && visible;
#else
	return (sample_v * g_cbTmap.tmap_size_x) >= min_valid_v;
#endif
#endif
}

bool Vis_Volume_And_Check(inout float4 vis_otf, inout float sample_v, const float3 pos_sample_ts)
{
	sample_v = tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_sample_ts, 0).r;
#if OTF_MASK==1
	int mask_vint = (int)(tex3D_volmask.SampleLevel(g_samplerPoint_clamp, pos_sample_ts, 0).r * g_cbVobj.mask_value_range + 0.5f);
	
	vis_otf = LoadOtfBufId(sample_v * g_cbTmap.tmap_size_x, buf_otf, g_cbVobj.opacity_correction, mask_vint);
	return vis_otf.a >= FLT_OPACITY_MIN__;//&& mask_vint > 0;//&& mask_vint == 3;
#elif SCULPT_MASK == 1
	int mask_vint = (int)(tex3D_volmask.SampleLevel(g_samplerPoint_clamp, pos_sample_ts, 0).r * g_cbVobj.mask_value_range + 0.5f);
	//int mask_vint = LoadMaxValueInt(pos_sample_ts, g_cbVobj.vol_size, g_cbVobj.mask_value_range, tex3D_volmask);
    int sculpt_value = (int) (g_cbVobj.vobj_flag >> 24);
    vis_otf = LoadOtfBuf(sample_v * g_cbTmap.tmap_size_x, buf_otf, g_cbVobj.opacity_correction);
    return ((uint)(vis_otf.a * 255.f) > 0) && (mask_vint == 0 || mask_vint > sculpt_value);
#else 

	vis_otf = LoadOtfBuf(sample_v * g_cbTmap.tmap_size_x, buf_otf, g_cbVobj.opacity_correction);
#if SCULPT_BITS == 1
	int3 voxel_id = (int3)(pos_sample_ts * (g_cbVobj.vol_original_size - uint3(1, 1, 1)));
	int wwh = g_cbVobj.vol_original_size.x * g_cbVobj.vol_original_size.y;
	uint bit_id = voxel_id.x + voxel_id.y * g_cbVobj.vol_original_size.x + voxel_id.z * wwh;
	uint mod = bit_id % 32;
	bool visible = !(bool)(sculpt_bits[bit_id / 32] & (0x1u << mod));
	return ((uint)(vis_otf.a * 255.f) > 0) && visible;
#else
    return vis_otf.a >= FLT_OPACITY_MIN__;
#endif

#endif
}

bool Vis_Volume_And_Check_Slab(inout float4 vis_otf, inout float sample_v, float sample_prev, const float3 pos_sample_ts)
{
	sample_v = tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_sample_ts, 0).r;

#if OTF_MASK==1
	int mask_vint = (int)(tex3D_volmask.SampleLevel(g_samplerPoint_clamp, pos_sample_ts, 0).r * g_cbVobj.mask_value_range + 0.5f);
	
	vis_otf = LoadSlabOtfBufId_PreInt(sample_v * g_cbTmap.tmap_size_x, sample_prev * g_cbTmap.tmap_size_x, buf_preintotf, g_cbVobj.opacity_correction, mask_vint);
	//vis_otf = LoadSlabOtfBuf_PreInt(sample_v * g_cbTmap.tmap_size_x, sample_prev * g_cbTmap.tmap_size_x, buf_preintotf, g_cbVobj.opacity_correction);
	//vis_otf = LoadOtfBufId(sample_v * g_cbTmap.tmap_size_x, buf_otf, g_cbVobj.opacity_correction, mask_vint);

	return vis_otf.a >= FLT_OPACITY_MIN__;//&& mask_vint > 0;//&& mask_vint == 3;

#elif SCULPT_MASK == 1
	int mask_vint = (int)(tex3D_volmask.SampleLevel(g_samplerPoint_clamp, pos_sample_ts, 0).r * g_cbVobj.mask_value_range + 0.5f);
	//int mask_vint = LoadMaxValueInt(pos_sample_ts, g_cbVobj.vol_size, 255, tex3D_volmask);
	int sculpt_value = (int)(g_cbVobj.vobj_flag >> 24);
	vis_otf = LoadSlabOtfBuf_PreInt(sample_v * g_cbTmap.tmap_size_x, sample_prev * g_cbTmap.tmap_size_x, buf_preintotf, g_cbVobj.opacity_correction);
	return ((uint)(vis_otf.a * 255.f) > 0) && (mask_vint == 0 || mask_vint > sculpt_value);
#else 

	vis_otf = LoadSlabOtfBuf_PreInt(sample_v * g_cbTmap.tmap_size_x, sample_prev * g_cbTmap.tmap_size_x, buf_preintotf, g_cbVobj.opacity_correction);
#if SCULPT_BITS == 1
	int3 voxel_id = (int3)(pos_sample_ts * (g_cbVobj.vol_original_size - uint3(1, 1, 1)));
	int wwh = g_cbVobj.vol_original_size.x * g_cbVobj.vol_original_size.y;
	uint bit_id = voxel_id.x + voxel_id.y * g_cbVobj.vol_original_size.x + voxel_id.z * wwh;
	uint mod = bit_id % 32;
	bool visible = !(bool)(sculpt_bits[bit_id / 32] & (0x1u << mod));
	return ((uint)(vis_otf.a * 255.f) > 0) && visible;
#else
	return vis_otf.a >= FLT_OPACITY_MIN__;
#endif

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
	[branch]
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
			
		[branch]
        if (blkSkip.blk_value > 0)
        {
	        [loop]
            for (int k = 0; k <= blkSkip.num_skip_steps; k++)
            {
                float3 pos_sample_blk_ts = pos_ray_start_ts + dir_sample_ts * (float) (i + k);
				[branch]
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

void Find1stSampleHit_Native(inout int step, const float3 pos_ray_start_ws, const float3 dir_sample_ws, const int num_ray_samples)
{
	step = -1;
	float sample_v = 0;

	float3 pos_ray_start_ts = TransformPoint(pos_ray_start_ws, g_cbVobj.mat_ws2ts);
	float3 dir_sample_ts = TransformVector(dir_sample_ws, g_cbVobj.mat_ws2ts);

	int min_valid_v = g_cbTmap.first_nonzeroalpha_index;
	[branch]
	if (Sample_Volume_And_Check(sample_v, pos_ray_start_ts, min_valid_v))
	{
		step = 0;
		return;
	}

	[loop]
	for (int i = 1; i < num_ray_samples; i++)
	{
		float3 pos_sample_ts = pos_ray_start_ts + dir_sample_ts * (float)i;

		if (Sample_Volume_And_Check(sample_v, pos_sample_ts, min_valid_v))
		{
			step = i;
			i = num_ray_samples;
			break;
		} // if(sample valid check)
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


#if VR_MODE != 3
#define GRAD_VOL2(Vc, Vp, P, VV, VU, VR, UVV, UVU, UVR) GradientVolume(P, g_cbVobj.vec_grad_x, g_cbVobj.vec_grad_y, g_cbVobj.vec_grad_z, tex3D_volume)
#define GRAD_VOL3(Vc, Vp, P, VV, VU, VR, UVV, UVU, UVR) GradientVolume2(Vc, P, g_cbVobj.vec_grad_x, g_cbVobj.vec_grad_y, g_cbVobj.vec_grad_z, tex3D_volume)
#define GRAD_VOL(Vc, Vp, P, VV, VU, VR, UVV, UVU, UVR) GradientVolume3(Vc, Vp, P, VV, VU, VR, UVV, UVU, UVR, tex3D_volume)
#else
#define GRAD_VOL(Vc, Vp, P, VV, VU, VR, UVV, UVU, UVR) GradientBinVolume(P, 2*g_cbVobj.vec_grad_x, 2*g_cbVobj.vec_grad_y, 2*g_cbVobj.vec_grad_z, tex3D_volume)
#endif

float3 GradientClippedVolume(const float sampleV, 
	const float3 pos_sample_ws, const float3 pos_sample_ts,
	const float3 vec_v, const float3 vec_u, const float3 vec_r, // TS from WS
	const float3 uvec_v, const float3 uvec_u, const float3 uvec_r,
	Texture3D tex3d_data)
{
	bool bv = IsInsideClipBound(pos_sample_ws - uvec_v * g_cbVobj.sample_dist * 2.f, g_cbClipInfo);
	bool bu = IsInsideClipBound(pos_sample_ws - uvec_u * g_cbVobj.sample_dist * 2.f, g_cbClipInfo);
	bool br = IsInsideClipBound(pos_sample_ws - uvec_r * g_cbVobj.sample_dist * 2.f, g_cbClipInfo);

	// note v, u, r are orthogonal for each other
	// vec_u and vec_r are defined in TS, and each length is sample distance
	// uvec_v, uvec_u, and uvec_r are unit vectors defined in WS
	//float dv = bv ? tex3d_data.SampleLevel(g_samplerLinear_clamp, pos_sample_ts - 2 * vec_v, 0).r - sampleV : 0;
	//float du = bu ? tex3d_data.SampleLevel(g_samplerLinear_clamp, pos_sample_ts - 2 * vec_u, 0).r - sampleV : 0;
	//float dr = br ? tex3d_data.SampleLevel(g_samplerLinear_clamp, pos_sample_ts - 2 * vec_r, 0).r - sampleV : 0;
	float dv = bv ? 1 : 0;
	float du = bu ? 1 : 0;
	float dr = br ? 1 : 0;

	float3 v_v = uvec_v * dv;
	float3 v_u = uvec_u * du;
	float3 v_r = uvec_r * dr;
	return v_v + v_u + v_r;
}

bool GetClipPlaneNormal(const float3 pos_sample_ws, out float3 clipNormal)
{
	float4x4 mat_target = float4x4(1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1);
	float3 p_cb = pos_sample_ws;
	float3 nrm = float3(1, 1, 1);
	float d = 100000.f;
	bool ret = false;
	if (g_cbClipInfo.clip_flag & 0x2)
	{
		mat_target = g_cbClipInfo.mat_clipbox_ws2bs;
		p_cb = TransformPoint(pos_sample_ws, mat_target);
		float3 pp = abs(p_cb);
		if (pp.x < pp.y)
		{
			nrm.x = 0;
			if (pp.y < pp.z)
			{
				nrm.y = 0;
				d = p_cb.z;
				if (d < 0){
					d = -d;
					nrm.z = -1;
				}
			}
			else
			{
				nrm.z = 0;
				d = p_cb.y;
				if (d < 0){
					d = -d;
					nrm.y = -1;
				}
			}
		}
		else {
			nrm.y = 0;
			if (pp.x < pp.z)
			{
				nrm.x = 0;
				d = p_cb.z;
				if (d < 0){
					d = -d;
					nrm.z = -1;
				}
			}
			else
			{
				nrm.z = 0;
				d = p_cb.x;
				if (d < 0){
					d = -d;
					nrm.x = -1;
				}
			}
		}
		float4x4 matbs2ws = inverse(mat_target);
		clipNormal = normalize(TransformVector(nrm, matbs2ws));
	}
	if (g_cbClipInfo.clip_flag & 0x1)
	{
		float3 normal_cb = normalize(TransformVector(g_cbClipInfo.vec_clipplane, mat_target));
		float3 p_plane_cb = TransformPoint(g_cbClipInfo.pos_clipplane, mat_target);
		float3 ph = p_cb - p_plane_cb;
		float dot_p = abs(dot(ph, normal_cb));
		if (d > dot_p)
		{
			clipNormal = normal_cb;
		}
	}
	return ret;
}
float3 GradientClippedVolume2(const float3 pos_sample_ws, const float3 pos_sample_ts, Texture3D tex3d_data)
{
	//g_cbVobj.vec_grad_x, g_cbVobj.vec_grad_y, g_cbVobj.vec_grad_z
	bool bx0 = IsInsideClipBound(pos_sample_ws - float3(1, 0, 0) * g_cbVobj.sample_dist * 2.f, g_cbClipInfo);
	bool bx1 = IsInsideClipBound(pos_sample_ws + float3(1, 0, 0) * g_cbVobj.sample_dist * 2.f, g_cbClipInfo);
	bool by0 = IsInsideClipBound(pos_sample_ws - float3(0, 1, 0) * g_cbVobj.sample_dist * 2.f, g_cbClipInfo);
	bool by1 = IsInsideClipBound(pos_sample_ws + float3(0, 1, 0) * g_cbVobj.sample_dist * 2.f, g_cbClipInfo);
	bool bz0 = IsInsideClipBound(pos_sample_ws - float3(0, 0, 1) * g_cbVobj.sample_dist * 2.f, g_cbClipInfo);
	bool bz1 = IsInsideClipBound(pos_sample_ws + float3(0, 0, 1) * g_cbVobj.sample_dist * 2.f, g_cbClipInfo);

	//!IsInsideClipBox(input.f3PosWS, g_cbClipInfo.mat_clipbox_ws2bs)

	float fGx = (bx1 ? tex3d_data.SampleLevel(g_samplerLinear_clamp, pos_sample_ts + g_cbVobj.vec_grad_x, 0).r : 0)
		- (bx0 ? tex3d_data.SampleLevel(g_samplerLinear_clamp, pos_sample_ts - g_cbVobj.vec_grad_x, 0).r : 0);
	float fGy = (by1 ? tex3d_data.SampleLevel(g_samplerLinear_clamp, pos_sample_ts + g_cbVobj.vec_grad_y, 0).r : 0)
		- (by0 ? tex3d_data.SampleLevel(g_samplerLinear_clamp, pos_sample_ts - g_cbVobj.vec_grad_y, 0).r : 0);
	float fGz = (bz1 ? tex3d_data.SampleLevel(g_samplerLinear_clamp, pos_sample_ts + g_cbVobj.vec_grad_z, 0).r : 0)
		- (bz0 ? tex3d_data.SampleLevel(g_samplerLinear_clamp, pos_sample_ts - g_cbVobj.vec_grad_z, 0).r : 0);

	//float fGx = (bx1 ? 1 : 0)
	//	- (bx0 ? 1 : 0);
	//float fGy = (by1 ? 1 : 0)
	//	- (by1 ? 1 : 0);
	//float fGz = (bz1 ? 1 : 0)
	//	- (bz0 ? 1 : 0);

	return float3(fGx, fGy, fGz);
}

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

#if FRAG_MERGING == 1
// these following two functions are the same as in Sr_KBuf.hlsl
int Fragment_OrderIndependentMerge(inout Fragment f_buf, const in Fragment f_in)
{
	float4 f_buf_fvis = ConvertUIntToFloat4(f_buf.i_vis);
	float4 f_in_fvis = ConvertUIntToFloat4(f_in.i_vis);

	float4 f_mix_vis = MixOpt(f_buf_fvis, f_buf.opacity_sum, f_in_fvis, f_in.opacity_sum);
	f_buf.i_vis = ConvertFloat4ToUInt(f_mix_vis);
	f_buf.opacity_sum = f_buf.opacity_sum + f_in.opacity_sum;
	float z_front = min(f_buf.z - f_buf.zthick, f_in.z - f_in.zthick);
	f_buf.z = max(f_buf.z, f_in.z);
	f_buf.zthick = f_buf.z - z_front;

	return 0;
}

bool OverlapTest(const in Fragment f_1, const in Fragment f_2)
{
	float diff_z1 = f_1.z - (f_2.z - f_2.zthick);
	float diff_z2 = (f_1.z - f_1.zthick) - f_2.z;
	return diff_z1 * diff_z2 < 0;
}
#endif

#if DX10_0 == 1
#define __EXIT_VR_RayCasting return output
PS_FILL_OUTPUT RayCasting(VS_OUTPUT input)
#else
#define __EXIT_VR_RayCasting return
[numthreads(GRIDSIZE_VR, GRIDSIZE_VR, 1)]
void RayCasting(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
#endif
{
#define __VRHIT_ON_CLIPPLANE 2
#define __VRHIT_OUTSIDE_CLIPPLANE 1
#define __VRHIT_OUTSIDE_VOLUME 0

	float4 vis_out = 0;
	float depth_out = FLT_MAX;

#if DX10_0 == 1
	PS_FILL_OUTPUT output;
	output.depthcs = FLT_MAX;
	output.color = (float4)0;
	//output.ds_z = input.f4PosSS.z;
	//return output;

	uint2 tex2d_xy = uint2(input.f4PosSS.xy);

	float4 prev_vis = prev_fragment_vis[tex2d_xy];
	float prev_depthcs = FLT_MAX;
	uint num_frags = 0;
	Fragment fs[2];
	if (prev_vis.a > 0) {
		num_frags = 1;
		prev_depthcs = prev_fragment_zdepth[tex2d_xy];
		if (asuint(prev_depthcs) == 0x12345678) // WILDCARD_DEPTH_OUTLINE
			prev_depthcs = 0.001;
		Fragment f;
		f.i_vis = ConvertFloat4ToUInt(prev_vis);
		f.z = prev_depthcs;
		fs[0] = f;
	}
	fs[num_frags] = (Fragment)0;
	fs[num_frags].z = FLT_MAX;

	output.color = prev_vis;
	output.depthcs = prev_depthcs;

	// in DX10_0 mode, use the register for vr_hit_enc
	uint vr_hit_enc = vrHitEnc[tex2d_xy];
	int i = 0;
	bool isSlicer = BitCheck(g_cbCamState.cam_flag, 10);

#else
    uint2 tex2d_xy = uint2(DTid.xy);
    if (DTid.x >= g_cbCamState.rt_width || DTid.y >= g_cbCamState.rt_height)
        return;

    // consider the deep-layers are sored in order
#if ONLY_SINGLE_LAYER == 1
#else
	Fragment fs[VR_MAX_LAYERS];
#endif

	float aos[AO_MAX_LAYERS] = {0, 0, 0, 0, 0, 0, 0, 0};
	float ao_vr = 0;
	if (g_cbEnv.r_kernel_ao > 0)
	{
		for (int k = 0; k < AO_MAX_LAYERS; k++) aos[k] = ao_textures[int3(DTid.xy, k)];
		ao_vr = ao_vr_texture[DTid.xy];
	}

#if ONLY_SINGLE_LAYER == 1
	uint vr_hit_enc = fragment_counter[DTid.xy] >> VR_ENC_BIT_SHIFT;
#else
	uint num_frags = fragment_counter[DTid.xy];

	if (num_frags == 0x12345679) {
		num_frags = 1;
	}

	uint vr_hit_enc = num_frags >> VR_ENC_BIT_SHIFT;
	num_frags = num_frags & 0xFFF;
#endif

	bool isDither = BitCheck(g_cbCamState.cam_flag, 7);
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

	uint pixel_id = tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x;
	//uint nThreadNum = nDTid.y * g_cbCamState.rt_width + nDTid.x; // pixel_id
//#if DYNAMIC_K_MODE == 1
//	if (pixel_id == 0) return;
//	uint pixel_offset = sr_offsettable_buf[pixel_id];
//	uint addr_base = pixel_offset * 4 * 2;
//#else
	// TEST

	bool isSlicer = BitCheck(g_cbCamState.cam_flag, 10);
	uint addr_base = 0;
	if (isSlicer) {
		addr_base = pixel_id * 3 * 4 * 2; // 3: elements, 4: bytes, 2: k
	}
	else {
		if (pixel_id == 0) return;
		uint pixel_offset = sr_offsettable_buf[pixel_id];
		addr_base = pixel_offset * 4 * 2;
	}
//#endif

#define MDVR_TEST

#if ONLY_SINGLE_LAYER == 1
	int i = 0;
	float dvr_layer_thickness = 0;
	//fragment_zthick[tex2d_xy] = 0;
#else

#ifdef MDVR_TEST
	float4 vis_prev = fragment_vis[tex2d_xy];
	float depth_prev = fragment_zdepth[tex2d_xy];
	float thick_prev = fragment_zthick[tex2d_xy];
	Fragment f_dvr = (Fragment)0;
	f_dvr.z = FLT_MAX;
	if (vis_prev.a > 0)
	{
		f_dvr.i_vis = ConvertFloat4ToUInt(vis_prev);
		f_dvr.z = depth_prev;
#if FRAG_MERGING == 1
		f_dvr.zthick = thick_prev;//g_cbVobj.sample_dist;
		f_dvr.opacity_sum = vis_prev.a;
#endif
	}
#endif
	int layer_count = 0;

	[loop]
    for (int i = 0; i < (int)num_frags; i++)
    {
        uint i_vis = 0;
		Fragment f;
		GET_FRAG(f, addr_base, i); // from K-buffer
        float4 vis_in = ConvertUIntToFloat4(f.i_vis);
		//if (g_cbEnv.r_kernel_ao > 0)
		//{
		//	vis_in.rgb *= 1.f - aos[layer_count];
		//	f.i_vis = ConvertFloat4ToUInt(vis_in);
		//}
#ifdef MDVR_TEST
		if (depth_prev < f.z && vis_prev.a > 0)
		{
			vis_out += vis_prev * (1.f - vis_out.a);
			fs[layer_count] = f_dvr;
			layer_count++;
			f_dvr.z = FLT_MAX;
		}
#endif
		fs[layer_count] = f;
		layer_count++;
		vis_out += vis_in * (1.f - vis_out.a);
    }
	num_frags = layer_count;

	//if (fragment_counter[DTid.xy] == 0x12345678) {
	//	fragment_vis[tex2d_xy] = float4(1, 0, 0, 1);
	//	return;
	//}
	//
	//if (fragment_counter[DTid.xy] == 0x87654321) {
	//	fragment_vis[tex2d_xy] = float4(0, 0, 1, 1);
	//	return;
	//}

#ifdef MDVR_TEST
	if (vis_prev.a > 0)
	{
		if (f_dvr.z < FLT_MAX)
		{
			vis_out += vis_prev * (1.f - vis_out.a);
			fs[num_frags++] = f_dvr;
		}
#if FRAG_MERGING == 1
		else
		{
			// other fragments exist
			// extended merging for consistent visibility transition
			vis_out = (float4)0;
			[loop]
			for (uint k = 0; k < num_frags; k++)
			{
				Fragment f_k = fs[k];
				if (f_k.i_vis != 0)
				{
					[loop]
					for (uint l = k + 1; l < num_frags; l++)
					{
						if (l != k)
						{
							Fragment f_l = fs[l];
							if (f_l.i_vis != 0)
							{
								if (OverlapTest(f_k, f_l))
								{
									Fragment_OrderIndependentMerge(f_k, f_l);
									f_l.i_vis = 0;
									f_l.z = FLT_MAX;
									fs[l] = f_l;
									//mer_cnt++;
								}
							}
						}
					}

					// optional setting for manual z-thickness
					//f_k.zthick = max(f_k.zthick, GetVZThickness(f_k.z, vz_thickness));
					fs[k] = f_k;
					vis_out += ConvertUIntToFloat4(f_k.i_vis) * (1.f - vis_out.a);
				}
			}
		}
#endif
	}
#endif
	//if (fs[0].i_vis != 0)
	//	fragment_vis[tex2d_xy] = float4(1, 1, 0, 1);

	//fragment_vis[tex2d_xy] = ConvertUIntToFloat4(fs[0].i_vis);// float4(1, 0, 0, 1);
	//return;

	//fragment_vis[DTid.xy] = float4(1, 0, 0, 1);
	//return;
	//if (num_frags == 0)
	//	fragment_vis[tex2d_xy] = float4(0, 0.3, 0.2, 1);
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
#endif // ONLY_SINGLE_LAYER == 1
    fragment_vis[tex2d_xy] = vis_out;
	
#if RAYMODE != 0
    fragment_zdepth[tex2d_xy] = depth_out = fs[0].z;
#endif 

#endif // DX10_0

    // Image Plane's Position and Camera State //
    float3 pos_ip_ss = float3(tex2d_xy, 0.0f);
    float3 pos_ip_ws = TransformPoint(pos_ip_ss, g_cbCamState.mat_ss2ws);
    float3 dir_sample_unit_ws = g_cbCamState.dir_view_ws;
    if (g_cbCamState.cam_flag & 0x1)
        dir_sample_unit_ws = pos_ip_ws - g_cbCamState.pos_cam_ws;
    dir_sample_unit_ws = normalize(dir_sample_unit_ws);
    float3 dir_sample_ws = dir_sample_unit_ws * g_cbVobj.sample_dist;


	// vv //
	float3 v_v = TransformVector(dir_sample_ws, g_cbVobj.mat_ws2ts); // v_v
	float3 uv_v = dir_sample_unit_ws; // uv_v
	float3 v_u = float3(tex2d_xy + float2(0, -1), 0.0f);
	float3 uv_u = normalize(TransformVector(v_u, g_cbCamState.mat_ss2ws));
	float3 uv_r = normalize(cross(uv_v, uv_u)); // uv_r
	uv_u = normalize(cross(uv_r, uv_v)); // uv_u , normalize?! for precision
	v_u = TransformVector(uv_u * g_cbVobj.sample_dist, g_cbVobj.mat_ws2ts); // v_u
	float3 v_r = TransformVector(uv_r * g_cbVobj.sample_dist, g_cbVobj.mat_ws2ts); // v_r

#if VR_MODE != 2
	v_r /= g_cbVobj.opacity_correction;
	v_u /= g_cbVobj.opacity_correction;
	v_v /= g_cbVobj.opacity_correction;
#endif
    
	const float merging_beta = 1.0;

	float3 vbos_hit_start_pos = pos_ip_ws;
#if RAYMODE == 0 // DVR
	//depth_out = fragment_zdepth[DTid.xy];
	// use the result from VR_SURFACE
	depth_out = vr_fragment_1sthit_read[tex2d_xy];

	if (BitCheck(g_cbVobj.vobj_flag, 1) && vr_hit_enc == __VRHIT_OUTSIDE_VOLUME) {
		int outlinePPack = g_cbCamState.iSrCamDummy__1;
		float3 outline_color = float3(((outlinePPack >> 16) & 0xFF) / 255.f, ((outlinePPack >> 8) & 0xFF) / 255.f, (outlinePPack & 0xFF) / 255.f);
		int pixThickness = (outlinePPack >> 24) & 0xFF;
		//v_rgba = OutlineTest(tex2d_xy, z_depth, 10000.f, outline_color, pixThickness);
	
		// note that the outline appears over background of the DVR front-surface
		float4 v_rgba = VrOutlineTest(tex2d_xy, depth_out, 10000.f, outline_color, pixThickness);
		uint idx_dlayer = 0;
		int num_ray_samples = VR_MAX_LAYERS;
		vis_out = (float4)0;
#if ONLY_SINGLE_LAYER == 1
		vis_out += v_rgba * (1.f - vis_out.a);
#else
#if FRAG_MERGING == 1
		Fragment f_dly = fs[0]; // if no frag, the z-depth is infinite
		INTERMIX(vis_out, idx_dlayer, num_frags, v_rgba, depth_out, g_cbVobj.sample_dist, fs, merging_beta);
#else
		INTERMIX_V1(vis_out, idx_dlayer, num_frags, v_rgba, depth_out, fs);
#endif
		REMAINING_MIX(vis_out, idx_dlayer, num_frags, fs);
#endif

#if DX10_0 == 1
		output.color = vis_out;
		output.depthcs = min(depth_out, fs[0].z);
#else
		fragment_vis[tex2d_xy] = vis_out;

#if ONLY_SINGLE_LAYER == 1
		fragment_zdepth[tex2d_xy] = depth_out;
		dvr_layer_thickness += g_cbVobj.sample_dist;
		fragment_zthick[tex2d_xy] = dvr_layer_thickness;
#else
		fragment_zdepth[tex2d_xy] = min(depth_out, fs[0].z);
#endif
#endif
		__EXIT_VR_RayCasting;
	}
	
	vbos_hit_start_pos = pos_ip_ws + dir_sample_unit_ws * depth_out;
#if DX10_0 == 1
	output.depthcs = fs[0].z;
#else
#if ONLY_SINGLE_LAYER == 1
	fragment_zdepth[tex2d_xy] = depth_out;
	fragment_zthick[tex2d_xy] = dvr_layer_thickness;
#else
	fragment_zdepth[tex2d_xy] = fs[0].z;
#endif
#endif
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
		__EXIT_VR_RayCasting;
	}
	
#endif
	// Ray Intersection for Clipping Box //
    float2 hits_t = ComputeVBoxHits(vbos_hit_start_pos, dir_sample_unit_ws, g_cbVobj.mat_alignedvbox_tr_ws2bs, g_cbClipInfo);
	// 1st Exit in the case that there is no ray-intersecting boundary in the volume box
	hits_t.y = min(g_cbCamState.far_plane, hits_t.y); // only available in orthogonal view (thickness slicer)
    int num_ray_samples = ceil((hits_t.y - hits_t.x) / g_cbVobj.sample_dist);
	if (num_ray_samples <= 0)
		__EXIT_VR_RayCasting;
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
	//float3 dir_sample_ts = v_v;// TransformVector(dir_sample_ws, g_cbVobj.mat_ws2ts);
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

	float sampleThickness = sample_dist;
#if ONLY_SINGLE_LAYER == 1
#else
#if FRAG_MERGING == 1
	Fragment f_dly = fs[0]; // if no frag, the z-depth is infinite
	sampleThickness = max(sample_dist, g_cbCamState.cam_vz_thickness);
#endif
#endif

	int start_idx = 0;
	float sample_v = tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_ray_start_ts, 0).r;

#if VR_MODE != 3
	// note that raycasters except vismask mode (or x-ray) use SLAB sample
	float sample_prev = tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_ray_start_ts - dir_sample_ts, 0).r;
#endif
	
#if VR_MODE == 1
	// opauqe vr
	float depth_sample = depth_hit;
	float4 vis_otf = (float4) 0; // note the otf result is the pre-multiplied color
#if OTF_MASK == 1
	if (Vis_Volume_And_Check(vis_otf, sample_v, pos_ray_start_ts)) // WHY NOT Vis_Volume_And_Check_Slab???????
#else
	if (Vis_Volume_And_Check_Slab(vis_otf, sample_v, sample_prev, pos_ray_start_ts))
#endif
	{
		
		float3 grad = GRAD_VOL(sample_v, sample_prev, pos_ray_start_ts, -v_v, v_u, v_r, -uv_v, uv_u, uv_r);
		float grad_len = length(grad);
		float3 nrl = grad / (grad_len + 0.00001f);

		float shade = 1.f;
		if (grad_len > 0 && vr_hit_enc != __VRHIT_ON_CLIPPLANE)
			shade = PhongBlinnVr(view_dir, g_cbVobj.pb_shading_factor, light_dirinv, nrl, true);

		//float4 vis_sample = float4(shade * vis_otf.rgb, vis_otf.a);
		float4 vis_sample = float4(shade * vis_otf.rgb, 1.f);
		vis_sample.rgb = saturate(vis_sample.rgb);

#if ONLY_SINGLE_LAYER == 1
		vis_out += vis_sample * (1.f - vis_out.a);
		dvr_layer_thickness += sample_dist;
#else
#if FRAG_MERGING == 1
		INTERMIX(vis_out, idx_dlayer, num_frags, vis_sample, depth_sample, sampleThickness, fs, merging_beta);
#else
		INTERMIX_V1(vis_out, idx_dlayer, num_frags, vis_sample, depth_sample, fs);
#endif
#endif	// ONLY_SINGLE_LAYER == 1
	}
#else
	if (vr_hit_enc == __VRHIT_ON_CLIPPLANE) // on the clip plane
	{
		float4 vis_otf = (float4) 0;
		start_idx++;

#if VR_MODE != 3
		//float3 grad = GRAD_VOL(sample_v, sample_prev, pos_ray_start_ts, v_v, v_u, v_r, uv_v, uv_u, uv_r);
		//float3 grad = GradientClippedVolume2(pos_ray_start_ws, pos_ray_start_ts, tex3D_volume);
#if VR_MODE != 2
		//float3 nrl;
		//GetClipPlaneNormal(pos_ray_start_ws, nrl);
#else
		float3 grad = GradientClippedVolume2(pos_ray_start_ws, pos_ray_start_ts, tex3D_volume);
#endif

		if (Vis_Volume_And_Check_Slab(vis_otf, sample_v, sample_prev, pos_ray_start_ts))
		//if (Vis_Volume_And_Check(vis_otf, sample_v, pos_ray_start_ts))
#else
		if (Vis_Volume_And_Check(vis_otf, pos_ray_start_ts))
#endif
		{
			float depth_sample = depth_hit;
#if VR_MODE != 3
			// note that depth_hit is the front boundary of slab
			depth_sample += sample_dist; // slab's back boundary
#endif

#if VR_MODE == 2
			float grad_len = length(grad) + 0.001f;
			float3 nrl = grad / grad_len;
#endif
			//float4 vis_sample = vis_otf;
			float shade = 1.f;
			//shade = saturate(PhongBlinnVr(view_dir, g_cbVobj.pb_shading_factor, light_dirinv, nrl, true));
			float4 vis_sample = float4(shade * vis_otf.rgb, vis_otf.a);
			//vis_sample.rgb = (nrl)/2;

#if VR_MODE == 2
			//float modulator = pow(min(grad_len * g_cbVobj.grad_scale / g_cbVobj.grad_max, 1.f), pow(g_cbVobj.kappa_i, g_cbVobj.kappa_s));
			//float modulator = min(grad_len * g_cbVobj.value_range * g_cbVobj.grad_scale / g_cbVobj.grad_max, 1.f);
			//vis_sample *= modulator; // https://github.com/korfriend/OsstemCoreAPIs/discussions/199#discussion-5114460
			MODULATE(0, grad_len);
#endif
			//vis_sample *= mask_weight;
#if ONLY_SINGLE_LAYER == 1
			vis_out += vis_sample * (1.f - vis_out.a);
#else
#if FRAG_MERGING == 1
			INTERMIX(vis_out, idx_dlayer, num_frags, vis_sample, depth_sample, sampleThickness, fs, merging_beta);
#else
			INTERMIX_V1(vis_out, idx_dlayer, num_frags, vis_sample, depth_sample, fs);
#endif
#endif	// ONLY_SINGLE_LAYER == 1
		}
#if VR_MODE != 3
		sample_prev = sample_v;
#endif
	}
	
	int sample_count = 0;
	//fragment_vis[tex2d_xy] = float4((pos_ray_start_ts + (float3)1) * 0.5f, 1.f);
	//if (num_ray_samples > start_idx + 30)
	//	fragment_vis[tex2d_xy] = float4(1, 0, 0, 1.f);
	//return;
	{
		float ert_ratio = asfloat(g_cbVobj.v_dummy0);
		num_ray_samples = (int)((float)num_ray_samples * ert_ratio);
	}
	float depth_sample = depth_hit;

	// check pos_ray_start_ws!!!!!!!!!!!!!!!! 
	//fragment_vis[tex2d_xy] = float4((pos_ray_start_ts * 3 + (float3)1) * 0.5f, 1.f);
	//return;
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
				if (sample_prev < 0) {
					sample_prev = tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_sample_blk_ts - dir_sample_ts, 0).r;
				}
				if (Vis_Volume_And_Check_Slab(vis_otf, sample_v, sample_prev, pos_sample_blk_ts))
				//if (Vis_Volume_And_Check(vis_otf, sample_v, pos_sample_blk_ts))
#else
				if (Vis_Volume_And_Check(vis_otf, pos_sample_blk_ts))
#endif
				{
					float3 grad = GRAD_VOL(sample_v, sample_prev, pos_sample_blk_ts, v_v, v_u, v_r, uv_v, uv_u, uv_r);
					float grad_len = length(grad);
					float3 nrl = grad / (grad_len + 0.00001f);

					float shade = 1.f;
					if (grad_len > 0) {
						shade = saturate(PhongBlinnVr(view_dir, g_cbVobj.pb_shading_factor, light_dirinv, nrl, true));
					}

					float4 vis_sample = float4(shade * vis_otf.rgb, vis_otf.a);
					depth_sample = depth_hit + (float)(i + j) * sample_dist;
#if VR_MODE == 2
					//g_cbVobj.kappa_i
					//float modulator = pow(min(grad_len * g_cbVobj.value_range * g_cbVobj.grad_scale / g_cbVobj.grad_max, 1.f), pow(g_cbVobj.kappa_i * max(__s, 0.1f), g_cbVobj.kappa_s));
					//float modulator = min(grad_len * g_cbVobj.value_range * g_cbVobj.grad_scale / g_cbVobj.grad_max, 1.f);
					//vis_sample *= modulator;

					float dist_sq = 1.f - (float)(i + j) / (float)num_ray_samples;
					dist_sq *= dist_sq;
					MODULATE(dist_sq, grad_len);
#endif
					//vis_sample *= mask_weight;
#if ONLY_SINGLE_LAYER == 1
					vis_out += vis_sample * (1.f - vis_out.a);
#else
#if FRAG_MERGING == 1
					INTERMIX(vis_out, idx_dlayer, num_frags, vis_sample, depth_sample, sampleThickness, fs, merging_beta);
#else
					INTERMIX_V1(vis_out, idx_dlayer, num_frags, vis_sample, depth_sample, fs);
#endif
#endif	// ONLY_SINGLE_LAYER == 1
					if (vis_out.a >= ERT_ALPHA)
					{
						i = num_ray_samples;
						j = num_ray_samples;
						vis_out.a = 1.f;
						break;
					}
				} // if(sample valid check)
#if VR_MODE != 3
				sample_prev = sample_v;
#endif
			} // for (int j = 0; j < blkSkip.num_skip_steps; j++, i++)
		}
		else
		{
			sample_count++;
#if VR_MODE != 3
			sample_prev = -1;
#endif
		}
		i += blkSkip.num_skip_steps;
		// this is for outer loop's i++
		//i -= 1;
	}

#endif
#if DX10_0 != 1
	vis_out.rgb *= (1.f - ao_vr);
#endif
#if ONLY_SINGLE_LAYER == 1
#else
	REMAINING_MIX(vis_out, idx_dlayer, num_frags, fs);
#endif


#else // RAYMODE != 0
	float3 pos_ray_start_ts = TransformPoint(pos_ray_start_ws, g_cbVobj.mat_ws2ts);
	float depth_begin = depth_out = length(pos_ray_start_ws - pos_ip_ws);
	float3 dir_sample_ts = v_v;// TransformVector(dir_sample_ws, g_cbVobj.mat_ws2ts);
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
#if SCULPT_MASK == 1
	int sculpt_value = (int)(g_cbVobj.vobj_flag >> 24);
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
#if SCULPT_MASK == 1
				int mask_vint = (int)(tex3D_volmask.SampleLevel(g_samplerPoint_clamp, pos_sample_in_blk_ts, 0).r * g_cbVobj.mask_value_range + 0.5f);
				float sample_v = 0;
				if (mask_vint == 0 || mask_vint > sculpt_value)
					sample_v = tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_sample_in_blk_ts, 0).r;
#else

#if SCULPT_BITS == 1
				int3 voxel_id = (int3)(pos_sample_ts * (g_cbVobj.vol_original_size - uint3(1, 1, 1)));
				int wwh = g_cbVobj.vol_original_size.x * g_cbVobj.vol_original_size.y;
				uint bit_id = voxel_id.x + voxel_id.y * g_cbVobj.vol_original_size.x + voxel_id.z * wwh;
				uint mod = bit_id % 32;
				bool visible = !(bool)(sculpt_bits[bit_id / 32] & (0x1u << mod));
				float sample_v = 0;
				if (visible)
					sample_v = tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_sample_in_blk_ts, 0).r;
#else
				float sample_v = tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_sample_in_blk_ts, 0).r;
#endif

#endif
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
		float sample_mask_v = tex3D_volmask.SampleLevel(g_samplerPoint_clamp, pos_sample_ts, 0).r * g_cbVolObj.mask_value_range;
		int mask_vint = (int)(sample_mask_v + 0.5f);
		float4 vis_otf = LoadOtfBufId(sample_v_norm * g_cbTmap.tmap_size_x, buf_otf, g_cbVobj.opacity_correction, mask_vint);
#elif SCULPT_MASK == 1
		int mask_vint = (int)(tex3D_volmask.SampleLevel(g_samplerPoint_clamp, pos_sample_ts, 0).r * g_cbVobj.mask_value_range + 0.5f);
		int sculpt_value = (int)(g_cbVobj.vobj_flag >> 24);
		float4 vis_otf = (float4)0;
		if (mask_vint == 0 || mask_vint > sculpt_value)
		{
			vis_otf = LoadOtfBuf(sample_v_norm * g_cbTmap.tmap_size_x, buf_otf, g_cbVobj.opacity_correction);
		}
#else	// OTF_MASK != 1


#if SCULPT_BITS == 1
		int3 voxel_id = (int3)(pos_sample_ts * (g_cbVobj.vol_original_size - uint3(1, 1, 1)));
		int wwh = g_cbVobj.vol_original_size.x * g_cbVobj.vol_original_size.y;
		uint bit_id = voxel_id.x + voxel_id.y * g_cbVobj.vol_original_size.x + voxel_id.z * wwh;
		uint mod = bit_id % 32;
		bool visible = !(bool)(sculpt_bits[bit_id / 32] & (0x1u << mod));
		float4 vis_otf = (float4)0;
		if (visible)
			vis_otf = LoadOtfBuf(sample_v_norm * g_cbTmap.tmap_size_x, buf_otf, g_cbVobj.opacity_correction);
#else
		float4 vis_otf = LoadOtfBuf(sample_v_norm * g_cbTmap.tmap_size_x, buf_otf, g_cbVobj.opacity_correction);
#endif

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
	float sample_mask_v = tex3D_volmask.SampleLevel(g_samplerPoint_clamp, pos_sample_ts, 0).r * g_cbVolObj.mask_value_range;
	int mask_vint = (int)(sample_mask_v + 0.5f);
	float4 vis_otf = LoadOtfBufId(sample_v_prev * g_cbTmap.tmap_size_x, buf_otf, g_cbVobj.opacity_correction, mask_vint);
#elif SCULPT_MASK == 1 && RAYMODE == 3
	vis_otf = LoadOtfBuf(sample_v_norm * g_cbTmap.tmap_size_x, buf_otf, g_cbVobj.opacity_correction);
#else
	float4 vis_otf = LoadOtfBuf(sample_v_prev * g_cbTmap.tmap_size_x, buf_otf, g_cbVobj.opacity_correction);
#endif
#endif
	uint idx_dlayer = 0;
#if ONLY_SINGLE_LAYER == 1
#else
#if FRAG_MERGING == 1
	float vthick = (hits_t.y - hits_t.x) * 1.f;
	if (vis_out.a > 0 && hits_t.x < fs[0].z && !isSlicer) 
	{
		num_frags = 1;
		vis_out.a *= 0.35;
		vis_out.rgb *= vis_out.a;
		fs[0].i_vis = ConvertFloat4ToUInt(vis_out);
		depth_sample = hits_t.x;
		vthick = 0.1;
	}	
	Fragment f_dly = fs[0]; // if no frag, the z-depth is infinite
	INTERMIX(vis_out, idx_dlayer, num_frags, vis_otf, depth_sample, vthick, fs, merging_beta);
#else
	INTERMIX_V1(vis_out, idx_dlayer, num_frags, vis_otf, depth_sample, fs);
#endif
	REMAINING_MIX(vis_out, idx_dlayer, num_frags, fs);
#endif // ONLY_SINGLE_LAYER == 1
#endif // // RAYMODE == 0


#ifdef DX10_0
	output.color = vis_out;
	output.depthcs = min(depth_out, fs[0].z);
	return output;
#else
	//if (count == 0) vis_otf = float4(1, 1, 0, 1);
	//vis_out = float4(ao_vr, ao_vr, ao_vr, 1);
	//vis_out = float4(TransformPoint(pos_ray_start_ws, g_cbVobj.mat_ws2ts), 1);

            vis_out = saturate(vis_out);

            fragment_vis[tex2d_xy] = vis_out;
#if ONLY_SINGLE_LAYER == 1
	// to do : compute thickness...
	fragment_zdepth[tex2d_xy] = depth_out;
	fragment_zthick[tex2d_xy] = max(depth_sample - depth_out, sample_dist);
#else
            fragment_zdepth[tex2d_xy] = min(depth_out, fs[0].z);
	//fragment_counter[DTid.xy] = num_frags + 1;
#endif

	//float tt = sample_count / 30.f;
	//fragment_vis[tex2d_xy] = float4((float3)tt, 1);// float4((pos_ray_start_ts + float3(1, 1, 1)) / 2, 1);
#endif
        }
/**/
/*
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
*/

#if DX10_0 == 1
#define __EXIT_VR_SURFACE return output
PS_FILL_OUTPUT_SURF VR_SURFACE(VS_OUTPUT input)
#else
#define __EXIT_VR_SURFACE return
[numthreads(GRIDSIZE_VR, GRIDSIZE_VR, 1)]

        void VR_SURFACE
        (
        uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
#endif
{
#if DX10_0 == 1
	PS_FILL_OUTPUT_SURF output;
	output.depthcs = FLT_MAX;
	output.enc = 0;
	int2 tex2d_xy = int2(input.f4PosSS.xy);
#else
            int2 tex2d_xy = int2(DTid.xy);
            vr_fragment_1sthit_write[DTid.xy] = FLT_MAX;

            if (DTid.x >= g_cbCamState.rt_width || DTid.y >= g_cbCamState.rt_height)
                return;

	// 2 : on the clip plane
	// 1 : outside the clip plane
	// 0 : outside the volume
            fragment_counter[DTid.xy] &= 0x3FFFFFFF;
#endif

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
            int num_ray_samples = (int) ((hits_t.y - hits_t.x) / g_cbVobj.sample_dist + 0.5f);
            if (num_ray_samples <= 0)
		__EXIT_VR_SURFACE;

            int hit_step = -1;
            float3 pos_start_ws = pos_ip_ws + dir_sample_unit_ws * hits_t.x;
            Find1stSampleHit(hit_step, pos_start_ws, dir_sample_ws, num_ray_samples);
            if (hit_step < 0)
		__EXIT_VR_SURFACE;
	
            float3 pos_hit_ws = pos_start_ws + dir_sample_ws * (float) hit_step;
            if (hit_step > 0)
            {
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
                float rand = _random(float2(tex2d_xy.x + g_cbCamState.rt_width * tex2d_xy.y, depth_hit));
                depth_hit -= rand * g_cbVobj.sample_dist;
		//float3 random_samples = float3(_random(DTid.x + g_cbCamState.rt_width * DTid.y), _random(DTid.x + g_cbCamState.rt_width * DTid.y + g_cbCamState.rt_width * g_cbCamState.rt_height), _random(DTid.xy));
            }

            uint dvr_hit_enc = length(pos_hit_ws - pos_start_ws) < g_cbVobj.sample_dist ? 2 : 1;
#if DX10_0 == 1
	output.depthcs = depth_hit;
	output.enc = dvr_hit_enc;
	return output;
#else
            vr_fragment_1sthit_write[DTid.xy] = depth_hit;
            uint fcnt = fragment_counter[DTid.xy];
            if (fcnt == 0x12345679)
            {
                fcnt = 1;
            }
	// 2 : on the clip plane
	// 1 : outside the clip plane
	// 0 : outside the volume
            fragment_counter[DTid.xy] = fcnt | (dvr_hit_enc << VR_ENC_BIT_SHIFT);
#endif
        }

#if DX10_0 == 1
#define __EXIT_PanoVR return output
//[earlydepthstencil]
PS_FILL_OUTPUT CurvedSlicer(VS_OUTPUT input)
#else
#define __EXIT_PanoVR return 
[numthreads(GRIDSIZE_VR, GRIDSIZE_VR, 1)]
void CurvedSlicer
(
uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
#endif
{
    float4 vis_out = 0;
    float depth_out = 0;

#if DX10_0 == 1
	PS_FILL_OUTPUT output;
	output.depthcs = FLT_MAX;
	output.color = (float4)0;

	uint2 cip_xy = uint2(input.f4PosSS.xy);

	float4 prev_vis = prev_fragment_vis[cip_xy];
	float prev_depthcs = FLT_MAX;
	uint num_frags = 0;
	Fragment fs[2];
	if (prev_vis.a > 0) {
		num_frags = 1;
		prev_depthcs = prev_fragment_zdepth[cip_xy];
		Fragment f; 
		f.i_vis = ConvertFloat4ToUInt(prev_vis);
		f.z = prev_depthcs;
#if FRAG_MERGING == 1
		f.zthick = g_cbVobj.sample_dist;
		f.opacity_sum = prev_vis.a;
#endif
		fs[0] = f;
	}
	fs[num_frags] = (Fragment)0;
	fs[num_frags].z = FLT_MAX;

	vis_out = prev_vis;

	output.color = prev_vis;
	output.depthcs = prev_depthcs;

	int i = 0;
#else
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
    if (num_frags == 0x12345679)
    {
        num_frags = 1;
    }
    num_frags = num_frags & 0xFFF;

	//if (num_frags == 0)
	//{
	//	fragment_vis[DTid.xy] = float4(1, 0, 0, 1);
	//	return;
	//}
	//else if (num_frags == 1)
	//{
	//	fragment_vis[DTid.xy] = float4(0, 0, 1, 1);
	//	return;
	//}
	//else if (num_frags == 2)
	//{
	//	fragment_vis[DTid.xy] = float4(0, 1, 0, 1);
	//	return;
	//}
	//else if (num_frags == 3)
	//{
	//	fragment_vis[DTid.xy] = float4(1, 1, 0, 1);
	//	return;
	//}
	//else if (num_frags == 4)
	//{
	//	fragment_vis[DTid.xy] = float4(0, 1, 1, 1);
	//	return;
	//}
	//else if (num_frags == 5)
	//{
	//	fragment_vis[DTid.xy] = float4(1, 0, 1, 1);
	//	return;
	//}
	//fragment_vis[DTid.xy] = float4(1, 1, 1, 1);
	//return;

    bool isDither = BitCheck(g_cbCamState.cam_flag, 8);
    if (isDither)
    {
        if (cip_xy.x % 2 != 0 || cip_xy.y % 2 != 0)
        {
            fragment_zdepth[cip_xy] = -777.0;
            return;
        }

		//fragment_vis[tex2d_xy] = float4(1, 0, 0, 1);
		//return;
    }

    Fragment fs[VR_MAX_LAYERS];

	[loop]
    for (int i = 0; i < (int) num_frags; i++)
    {
        uint i_vis = 0;
        Fragment f;
		GET_FRAG(f, addr_base, i); // from K-buffer
        float4 vis_in = ConvertUIntToFloat4(f.i_vis);
        if (g_cbEnv.r_kernel_ao > 0)
            f.i_vis = ConvertFloat4ToUInt(vis_in);
        if (vis_in.a > 0)
            vis_out += vis_in * (1.f - vis_out.a);

#if FRAG_MERGING == 1
		f.zthick = g_cbVobj.sample_dist;
#endif
        fs[i] = f;
    }

    fs[num_frags] = (Fragment) 0;
    fs[num_frags].z = FLT_MAX;
    fragment_vis[cip_xy] = vis_out;
#endif


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
    float fRatio0 = (float) ((i2SizeBuffer.x - 1) - cip_xy.x) / (float) (i2SizeBuffer.x - 1);
    float fRatio1 = (float) (cip_xy.x) / (float) (i2SizeBuffer.x - 1);

    float2 f2PosInterTopCOS, f2PosInterBottomCOS, f2PosSampleCOS;
    f2PosInterTopCOS.x = fRatio0 * f3PosTopLeftCOS.x + fRatio1 * f3PosTopRightCOS.x;
    f2PosInterTopCOS.y = fRatio0 * f3PosTopLeftCOS.y + fRatio1 * f3PosTopRightCOS.y;

    if (f2PosInterTopCOS.x < 0 || f2PosInterTopCOS.x >= (float) (iPlaneSizeX - 1))
		__EXIT_PanoVR;

    int iPosSampleCOS = (int) floor(f2PosInterTopCOS.x);
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
    float fRatio0Y = (float) ((i2SizeBuffer.y - 1) - cip_xy.y) / (float) (i2SizeBuffer.y - 1);
    float fRatio1Y = (float) (cip_xy.y) / (float) (i2SizeBuffer.y - 1);

    f2PosSampleCOS.x = fRatio0Y * f2PosInterTopCOS.x + fRatio1Y * f2PosInterBottomCOS.x;
    f2PosSampleCOS.y = fRatio0Y * f2PosInterTopCOS.y + fRatio1Y * f2PosInterBottomCOS.y;

    if (f2PosSampleCOS.y < 0 || f2PosSampleCOS.y > fPlaneSizeY)
		__EXIT_PanoVR;

    float sample_dist = g_cbVobj.sample_dist;
	// start position //
	//vmfloat3 f3PosSampleWS = f3PosSampleWS_C + f3VecSampleUpWS * (f2PosSampleCOS.y - fPlaneCenterY)
	//	+ f3VecSampleViewWS * fStepLength * (float)m;
    float3 pos_ray_start_ws = f3PosSampleWS_C + f3VecSampleUpWS * (f2PosSampleCOS.y - fPlaneCenterY);
    float3 dir_sample_ws = f3VecSampleViewWS * sample_dist;

	// vv //
    float3 uv_v = normalize(f3VecSampleViewWS); // uv_v
    float3 uv_u = normalize(f3VecSampleUpWS); // uv_u
    float3 uv_r = cross(uv_v, uv_u); // uv_r
    float3 v_v = TransformVector(dir_sample_ws, g_cbVobj.mat_ws2ts); // v_v
    float3 v_u = TransformVector(uv_u * g_cbVobj.sample_dist, g_cbVobj.mat_ws2ts); // v_u
    float3 v_r = TransformVector(uv_r * g_cbVobj.sample_dist, g_cbVobj.mat_ws2ts); // v_r

#if VR_MODE != 2
    v_r /= g_cbVobj.opacity_correction;
    v_u /= g_cbVobj.opacity_correction;
    v_v /= g_cbVobj.opacity_correction;
#endif

			
	float2 hits_t = ComputeVBoxHits(pos_ray_start_ws, f3VecSampleViewWS, g_cbVobj.mat_alignedvbox_tr_ws2bs, g_cbClipInfo);
	hits_t.y = min(hits_t.y, fPlaneThickness);
	int num_ray_samples = ceil((hits_t.y - hits_t.x) / sample_dist);
	if (num_ray_samples <= 0)
		__EXIT_VR_RayCasting;
			
	pos_ray_start_ws = pos_ray_start_ws + f3VecSampleViewWS * max(hits_t.x, 0);
			
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
		__EXIT_PanoVR;
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

	float4 vis_out_prev = vis_out;
	vis_out = (float4)0;
	float sample_v = tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_ray_hit_ts, 0).r;
	int start_idx = 0;

#if VR_MODE != 3
	// note that raycasters except vismask mode (or x-ray) use SLAB sample
	start_idx = 0;
	float sample_prev = tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_ray_hit_ts - v_v, 0).r;
#endif

#if VR_MODE != 2
	if (isOnPlane)//
	{
		float4 vis_otf = (float4) 0;
		start_idx++;

#if VR_MODE != 3
		float3 grad = GRAD_VOL(sample_v, sample_prev, pos_ray_hit_ts, v_v, v_u, v_r, uv_v, uv_u, uv_r);
		if (Vis_Volume_And_Check_Slab(vis_otf, sample_v, sample_prev, pos_ray_hit_ts))
		//if (Vis_Volume_And_Check(vis_otf, sample_v, pos_ray_hit_ts))
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
			float grad_len = length(grad);
			//float3 nrl = grad / (grad_len + 0.0001f);
			//float modulator = pow(min(grad_len * g_cbVobj.value_range * g_cbVobj.grad_scale / g_cbVobj.grad_max, 1.f), pow(kappa_i * max(__s, 0.1f), kappa_s));
			//float modulator = pow(min(grad_len * g_cbVobj.value_range * g_cbVobj.grad_scale / g_cbVobj.grad_max, 1.f), pow(kappa_i * max(__s, 0.1f), kappa_s));

			MODULATE(0, grad_len);
#endif
			//vis_sample *= mask_weight;
			
			//vis_out += vis_sample * (1.f - vis_out.a);

			float depth_sample = depthHit;
#if FRAG_MERGING == 1
			INTERMIX(vis_out, idx_dlayer, num_frags, vis_sample, depth_sample, sample_dist, fs, merging_beta);
#else
			INTERMIX_V1(vis_out, idx_dlayer, num_frags, vis_sample, depth_sample, fs);
#endif
			//fragment_vis[cip_xy] = float4(1, 0, 0, 1);
			//return;
		}
		//fragment_vis[cip_xy] = float4(1, 0, 0, 1);
		//return;
#if VR_MODE != 3
		sample_prev = sample_v;
#endif
	}
#endif
	//fragment_vis[cip_xy] = vis_out;
	//return;

	{
		float ert_ratio = asfloat(g_cbVobj.v_dummy0);
		num_new_ray_samples = (int)((float)num_new_ray_samples * ert_ratio);
	}

	[loop]
	for (i = start_idx; i < num_new_ray_samples; i++) 
	{
		float3 pos_sample_ts = pos_ray_hit_ts + dir_sample_ts * (float)i;

		LOAD_BLOCK_INFO(blkSkip, pos_sample_ts, dir_sample_ts, num_new_ray_samples, i);

		if (blkSkip.blk_value > 0)
		{
			[loop]
			//for (int j = 0; j <= blkSkip.num_skip_steps && i + j < num_new_ray_samples; j++)
			for (int j = 0; j <= blkSkip.num_skip_steps; j++)
			{
				//float3 pos_sample_blk_ws = pos_sample_ts + dir_sample_ws * (float) i;
				float3 pos_sample_blk_ts = pos_ray_hit_ts + dir_sample_ts * (float) (i + j);

				float4 vis_otf = (float4) 0;
#if VR_MODE != 3
				if (sample_prev < 0) {
					sample_prev = tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_sample_blk_ts - v_v, 0).r;
				}
				if (Vis_Volume_And_Check_Slab(vis_otf, sample_v, sample_prev, pos_sample_blk_ts))
				//if (Vis_Volume_And_Check(vis_otf, sample_v, pos_sample_blk_ts))
#else
				if (Vis_Volume_And_Check(vis_otf, pos_sample_blk_ts))
#endif
				{
					float3 grad = GRAD_VOL(sample_v, 0, pos_sample_blk_ts, v_v, v_u, v_r, uv_v, uv_u, uv_r);
					float grad_len = length(grad);
					float3 nrl = grad / (grad_len + 0.0001f);

					float shade = 1.f;
#if VR_MODE != 2
					if (grad_len > 0)
						shade = saturate(PhongBlinnVr(view_dir, g_cbVobj.pb_shading_factor, light_dirinv, nrl, true));
#endif
					
					float4 vis_sample = float4(shade * vis_otf.rgb, vis_otf.a);
					float depth_sample = depthHit + (float)(i + j) * sample_dist;
#if VR_MODE == 2
					float dist_sq = 1.f - (float)(i + j) / (float)num_new_ray_samples;
					dist_sq *= dist_sq;

					MODULATE(dist_sq, grad_len);
#endif

//					vis_out += vis_sample * (1.f - vis_out.a);

#if FRAG_MERGING == 1
					INTERMIX(vis_out, idx_dlayer, num_frags, vis_sample, depth_sample, sample_dist, fs, merging_beta);
#else
					INTERMIX_V1(vis_out, idx_dlayer, num_frags, vis_sample, depth_sample, fs);
#endif



					if (vis_out.a >= ERT_ALPHA)
					{
						i = 10000000;//num_ray_samples;
						j = 10000000;// num_ray_samples;
						vis_out.a = 1.f;
						break;
					}
				} // if(sample valid check)
#if VR_MODE != 3
				sample_prev = sample_v;
#endif
			} // for (int j = 0; j < blkSkip.num_skip_steps; j++, i++)
			//i += blkSkip.num_skip_steps;
		} // if (blkSkip.blk_value > 0)
		else
		{
			//i += blkSkip.num_skip_steps;// max(blkSkip.num_skip_steps - 1, 0);
#if VR_MODE != 3
			sample_prev = -1;
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
            float depth_sample = depth_begin + g_cbVobj.sample_dist * (float) (num_ray_samples);
            int num_valid_samples = 0;
            float4 vis_otf_sum = (float4) 0;
            float sampleSum = 0;
#endif
            float3 pos_ray_start_ts = TransformPoint(pos_ray_start_ws, g_cbVobj.mat_ws2ts);
            float3 dir_sample_ts = TransformVector(dir_sample_ws, g_cbVobj.mat_ws2ts);

            int count = 0;
	[loop]
            for (i = 0; i < num_ray_samples; i++)
            {
                float3 pos_sample_ts = pos_ray_start_ts + dir_sample_ts * (float) i;

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
		float sample_mask_v = tex3D_volmask.SampleLevel(g_samplerPoint_clamp, pos_sample_ts, 0).r * g_cbVolObj.mask_value_range;
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
	float sample_mask_v = tex3D_volmask.SampleLevel(g_samplerPoint_clamp, pos_sample_ts, 0).r * g_cbVolObj.mask_value_range;
	int mask_vint = (int)(sample_mask_v + 0.5f);
	float4 vis_otf = LoadOtfBufId(sample_v_prev * g_cbTmap.tmap_size_x, buf_otf, g_cbVobj.opacity_correction, mask_vint);
#else
            float4 vis_otf = LoadOtfBuf(sample_v_prev * g_cbTmap.tmap_size_x, buf_otf, g_cbVobj.opacity_correction);
#endif
#endif

            uint idx_dlayer = 0;
            Fragment f_dly = fs[0]; // if no frag, the z-depth is infinite
#if FRAG_MERGING == 1
	INTERMIX(vis_out, idx_dlayer, num_frags, vis_otf, depth_begin, fPlaneThickness, fs, merging_beta);
#else
	INTERMIX_V1(vis_out, idx_dlayer, num_frags, vis_otf, depth_begin, fs);
#endif
	REMAINING_MIX(vis_out, idx_dlayer, num_frags, fs);
#endif

#ifdef DX10_0
	output.color = vis_out;
	output.depthcs = depth_out;
	return output;
#else
                    fragment_vis[cip_xy] = vis_out;
                    fragment_zdepth[cip_xy] = depth_out;
#endif
                }
/**/