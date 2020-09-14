#include "../CommonShader.hlsl"
#include "../macros.hlsl"

Texture3D tex3D_volume : register(t0);
Texture3D tex3D_volblk : register(t1);
Texture3D tex3D_volmask : register(t2);
Buffer<float4> buf_otf : register(t3); // unorm 으로 변경하기
Buffer<float4> buf_windowing : register(t4);
Buffer<int> buf_ids : register(t5); // Mask OTFs

RWTexture2D<uint> fragment_counter : register(u0);
RWByteAddressBuffer deep_k_buf : register(u1);
RWTexture2D<float4> fragment_vis : register(u2);
RWTexture2D<float> fragment_zdepth : register(u3);

Texture2D<unorm float4> ao_textures[MAX_LAYERS / 4] : register(t10);
Texture2D<unorm float> ao_vr_texture : register(t20);

// case 1 : dvr to deep layers (intra layers are mixed with deep layers)
// case 2 : mixnig dvr (deep layers and on-the-fly samples)

int Sample_Volume(const in float3 pos_sample_ts)
{
    return (int) (tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_sample_ts, 0).r * g_cbVobj.value_range + 0.5f);
}

bool Sample_Volume_And_Check(inout int sample_v, const in float3 pos_sample_ts, const in int min_valid_v)
{
    sample_v = (int) (tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_sample_ts, 0).r * g_cbVobj.value_range + 0.5f);
#if OTF_MASK==1
    int max_vint = LoadMaxValueInt(pos_sample_ts, g_cbVobj.vol_size, 255, tex3D_volmask);
    int mask_otf_id = buf_ids[max_vint];
    float4 vis_otf = LoadOtfBufId(sample_v, buf_otf, g_cbVobj.opacity_correction, mask_otf_id);
    return (uint)(vis_otf.a * 255.f) > 0;
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
    sample_v = (int) (tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_sample_ts, 0).r * g_cbVobj.value_range + 0.5f);
#if OTF_MASK==1
    int max_vint = LoadMaxValueInt(pos_sample_ts, g_cbVobj.vol_size, 255, tex3D_volmask);
    int mask_otf_id = buf_ids[max_vint];
    vis_otf = LoadOtfBufId(sample_v, buf_otf, g_cbVobj.opacity_correction, mask_otf_id);
    return (uint)(vis_otf.a * 255.f) > FLT_MIN;
#elif SCULPT_MASK == 1
	int max_vint = LoadMaxValueInt(pos_sample_ts, g_cbVobj.vol_size, 255, tex3D_volmask);
    int sculpt_value = (int) (g_cbVobj.vobj_flag >> 24);
    vis_otf = LoadOtfBuf(sample_v, buf_otf, g_cbVobj.opacity_correction);
    return ((uint)(vis_otf.a * 255.f) > 0) && (max_vint == 0 || max_vint > sculpt_value);
#else 
    vis_otf = LoadOtfBuf(sample_v, buf_otf, g_cbVobj.opacity_correction);
    return vis_otf.a >= FLT_OPACITY_MIN__;
#endif
}

#define VR_MAX_LAYERS MAX_LAYERS+1

// this stores deep info into deep buffers (c.f INTERMIX and INTERMIX_V1 store only visibility, assuming the depth_out is the hit depth)
void InsertLayerToSortedDeepLayers(inout float4 vis_out, inout float depth_out, 
	const in Fragment f_in, inout Fragment fs[VR_MAX_LAYERS],
    const in uint addr_base, const in int num_frags, const in int num_dlayers, float merging_beta)
{
    int inser_idx = 0;
    [loop]
    for (int i = 0; i <= num_frags; i++)
    {
        if (fs[i].z > f_in.z)
        {
            inser_idx = i;
            break;
        }
    }

    int offset = 0;
    [loop]
    for (i = num_frags - 1; i >= inser_idx; i--, offset++)
    {
		fs[num_frags - offset] = fs[i];
    }
	fs[inser_idx] = f_in;

    // merge self-overlapping surfaces to thickness surfaces
    float4 fmix_vis = (float4) 0;
    int num_frags_new = num_frags + 1;
    int cnt_sorted_ztsurf = 0;
    [loop]
    for (i = 0; i < num_frags_new; i++)
    {
		Fragment f_ith = fs[i];
        if (f_ith.i_vis > 0)
        {
            for (int j = i + 1; j < num_frags_new; j++)
            {
				Fragment f_next = fs[j];
                if (f_next.i_vis > 0)
                {
					Fragment_OUT fs_out = MergeFrags(f_ith, f_next, merging_beta);
					f_ith = fs_out.f_prior;
					fs[j] = fs_out.f_posterior;
                }
            }
        
            if ((f_ith.i_vis >> 24) > 0) // always.. (just for safe-check)
            {
				fs[cnt_sorted_ztsurf] = f_ith;
                cnt_sorted_ztsurf++;
                fmix_vis += ConvertUIntToFloat4(f_ith.i_vis) * (1.f - fmix_vis.a);
            }
            // 여기에 deep buffer 값 넣기... ==> massive loop contents 는 피하는 것이 좋으므로 아래로 뺀다.
        } // if (rs_ith.zthick >= FLT_MIN__)
    }

    [loop]
    for (i = cnt_sorted_ztsurf; i < num_dlayers + 1; i++)
    {
		fs[i] = (Fragment)0;
		fs[i].z = FLT_MAX;
    }
    
    float4 blendout = (float4) 0;
    [loop]
    for (i = 0; i < num_dlayers; i++)
    {
		Fragment f = fs[i];
        if (i == num_dlayers - 1) // last index
        {
			Fragment f_next = fs[num_dlayers]; // i + 1
            float4 fvis_next = ConvertUIntToFloat4(f_next.i_vis);
            float4 fvis_cur = ConvertUIntToFloat4(f.i_vis);

            float4 blend_last_slot = fvis_cur + fvis_next * (1.f - fvis_cur.a);
			f.i_vis = ConvertFloat4ToUInt(blend_last_slot);
        }

        if (i < cnt_sorted_ztsurf)
        {
            float4 vis = ConvertUIntToFloat4(f.i_vis);
            blendout += vis * (1.f - blendout.a);
        }
		SET_FRAG(addr_base, i, f);
    }
    vis_out = blendout;
    depth_out = fs[0].z;
}

void RaySum(inout float4 vis_out, inout float depth_out, const in float3 pos_ip_ws, const in float3 pos_ray_start_ws, const in float3 dir_sample_ws, const in int num_ray_samples,
	inout Fragment fs[VR_MAX_LAYERS],
    const in uint addr_base, const in int num_frags, const in int num_dlayers, float merging_beta)
{
    float3 dir_sample_ts = TransformVector(dir_sample_ws, g_cbVobj.mat_ws2ts);
    float3 pos_ray_start_ts = TransformPoint(pos_ray_start_ws, g_cbVobj.mat_ws2ts);
    
    int num_valid_samples = 0;
    int last_sample_idx = 0;
    int first_sample_idx = -1;
    float4 vis_layer = (float4) 0;
    float depth_layer = FLT_MAX;

	[loop]
    for (int i = 0; i < num_ray_samples; i++)
    {
        float3 pos_sample_ts = pos_ray_start_ts + dir_sample_ts * (float) i;
		
        int sample_value = Sample_Volume(pos_sample_ts);
#if OTF_MASK==1
        int max_vint = LoadMaxValueInt(pos_sample_ts, g_cbVobj.vol_size, 255, tex3D_volmask);
        int mask_otf_id = buf_ids[max_vint];
        float4 vis_otf = LoadOtfBufId(sample_value, buf_otf, 1.f, mask_otf_id);
#else	// ~OTF_MASK
        float4 vis_otf = LoadOtfBuf(sample_value, buf_otf, 1.f);
#endif
        if (vis_otf.a > 0)
		{
            vis_layer += vis_otf;
			num_valid_samples++;
            last_sample_idx = i;
            if (first_sample_idx < 0)
                first_sample_idx = i;
        }
	}

	if (num_valid_samples == 0)
		return;
    
    vis_out = 0;
    depth_out = FLT_MAX;
	Fragment f_in;
	float4 fvis_in = vis_layer / (float)num_valid_samples;
	f_in.i_vis = ConvertFloat4ToUInt(fvis_in);
	f_in.z = length(pos_ray_start_ws - pos_ip_ws) + g_cbVobj.sample_dist * (float)(last_sample_idx);
	f_in.zthick = g_cbVobj.sample_dist * (float) (last_sample_idx - first_sample_idx + 1);
	f_in.opacity_sum = fvis_in.a;
    InsertLayerToSortedDeepLayers(vis_out, depth_out, f_in, fs, addr_base, num_frags, num_dlayers, merging_beta);
}

void RayMinMax(inout float4 vis_out, inout float depth_out, float3 pos_ip_ws, float3 pos_ray_start_ws, float3 dir_sample_ws, int num_ray_samples, int mode,
	inout Fragment fs[VR_MAX_LAYERS],
    const in uint addr_base, const in int num_frags, const in int num_dlayers, float merging_beta)
{
    float depth_sample_start = length(pos_ray_start_ws - pos_ip_ws);
    float3 dir_sample_ts = TransformVector(dir_sample_ws, g_cbVobj.mat_ws2ts);
    float3 pos_ray_start_ts = TransformPoint(pos_ray_start_ws, g_cbVobj.mat_ws2ts);
    
    // mode == 0 : RM_RAYMAX
    // mode == 1 : RM_RAYMIN
    
    int num_valid_samples = 0;
    float4 vis_sum = (float4) 0;
    int lucky_step = (int) ((float) (Random(pos_ray_start_ws.xy) + 1) * (float) num_ray_samples * 0.5f);
    float3 pos_lucky_sample_ws = pos_ray_start_ws + dir_sample_ws * (float) lucky_step;
    float3 pos_lucky_sample_ts = TransformPoint(pos_lucky_sample_ws, g_cbVobj.mat_ws2ts);
    int value_sample_prev = Sample_Volume(pos_lucky_sample_ts);
    if (mode == 1)
        value_sample_prev = 65535;

#if OTF_MASK==1
	float3 pos_mask_sample_ts = pos_lucky_sample_ts;
#endif
    
    int value_multply = 1.f;
    if (mode == 1)
        value_multply = -1.f;

	[loop]
    for (int i = 0; i < num_ray_samples; i++)
    {
        float3 pos_sample_ts = pos_ray_start_ts + dir_sample_ts * (float) i;
        
        LOAD_BLOCK_INFO(blkSkip, pos_sample_ts, dir_sample_ts, num_ray_samples, i)
        
        int comp_value = value_sample_prev * value_multply;
        if (blkSkip.blk_value > (int) comp_value)
        {
	        [loop]
            for (int k = 0; k < blkSkip.num_skip_steps; k++, i++)
			{
				float3 pos_sample_blk_ws = pos_ray_start_ws + dir_sample_ws*(float)i;
				float3 pos_sample_blk_ts = TransformPoint(pos_sample_blk_ws, g_cbVobj.mat_ws2ts);
                int sample_value = Sample_Volume(pos_sample_blk_ts);
                if (sample_value > value_sample_prev)
				{
#if OTF_MASK==1
					pos_mask_sample_ts = pos_sample_blk_ts;
#endif
					value_sample_prev = sample_value;
                }
			}
		}
		else
		{
			i += blkSkip.num_skip_steps;
		}
		// this is for outer loop's i++
		i -= 1;
    }


    float4 vis_layer = (float4) 0;
#if OTF_MASK==1
	int max_vint = LoadMaxValueInt(pos_mask_sample_ts, g_cbVobj.vol_size, 255, tex3D_volmask);
    int mask_otf_id = buf_ids[max_vint];
    vis_layer = LoadOtfBufId(value_sample_prev, buf_otf, 1.f, mask_otf_id);
#else
    vis_layer = LoadOtfBuf(value_sample_prev, buf_otf, 1.f);
#endif

	Fragment f_in;
	f_in.i_vis = ConvertFloat4ToUInt(vis_layer);
	f_in.z = depth_sample_start + g_cbVobj.sample_dist * (float) (num_ray_samples);
	f_in.zthick = g_cbVobj.sample_dist * (float) (num_ray_samples);
	f_in.opacity_sum = vis_layer.a;
    InsertLayerToSortedDeepLayers(vis_out, depth_out, f_in, fs, addr_base, num_frags, num_dlayers, merging_beta);
}

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

void RayCasting(out float4 vis_out, out float depth_out, const in float3 pos_ip_ws, const in float3 pos_ray_start_ws, const in float3 dir_sample_ws, const in int num_ray_samples, 
	inout Fragment fs[VR_MAX_LAYERS], float ao_vr,
    const in uint addr_base, const in int num_frags, const in int hit_enc, const in int num_dlayers, float merging_beta)
{
    vis_out = 0;
    depth_out = FLT_MAX;

    float3 pos_ray_start_ts = TransformPoint(pos_ray_start_ws, g_cbVobj.mat_ws2ts);
    float3 dir_sample_ts = TransformVector(dir_sample_ws, g_cbVobj.mat_ws2ts);
    
    float depth_hit = depth_out = length(pos_ray_start_ws - pos_ip_ws);

    // note that the gradient normal direction faces to the inside
    float3 light_dirinv = -g_cbEnv.dir_light_ws;
    if (g_cbEnv.env_flag & 0x1)
        light_dirinv = -normalize(pos_ray_start_ts - g_cbEnv.pos_light_ws);
    
    int idx_dlayer = 0;
	{
		// care for clip plane ... 
	}
#if VR_MODE==1 // OPAQUE SURFACE
	int sample_v = (int)(tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_ray_start_ts, 0).r * g_cbVobj.value_range + 0.5f);
    float3 otf_rgb = buf_otf[sample_v].rgb;    
    float grad_len = 0;
    float3 nrl = GRAD_NRL_VOL(pos_ray_start_ts, dir_sample_ws, grad_len);
    float shade = 1.f;
    if (grad_len > 0)
        shade = PhongBlinnVr(normalize(dir_sample_ws), hit_enc == 2? float4(1, 0, 0, 0) : g_cbVobj.pb_shading_factor, light_dirinv, nrl, true); // g_cbVobj.pb_shading_factor
    float4 vis_sample = float4(shade * otf_rgb, 1);
	vis_sample.rgb *= (1.f - ao_vr);

	//vis_out.rgba = ConvertUIntToFloat4(vis_array[0]);
	//vis_out.rgb = (float3)zdepth_array[0];
	//vis_out.a = 1.0;
#if GHOST_EFFECT==1
	if(vis_array[0] > 0) vis_sample *= 0.7;
#endif
	Fragment f_in;
	f_in.i_vis = ConvertFloat4ToUInt(vis_sample);
	f_in.z = depth_hit;
	f_in.zthick = g_cbVobj.vz_thickness;
	f_in.opacity_sum = vis_sample.a;
    InsertLayerToSortedDeepLayers(vis_out, depth_out, f_in, fs, addr_base, num_frags, num_dlayers, merging_beta);
	//vis_out = vis_sample;// g_cbVobj.pb_shading_factor;// vis_sample;//float4(otf_rgb, 1);
    //INTERMIX_V1(vis_out, idx_dlayer, num_frags, vis_sample, depth_hit, vis_array, zdepth_array, alphaw_array);
    //INTERMIX(vis_out, idx_dlayer, num_frags, vis_sample, depth_hit, g_cbVobj.vz_thickness, vis_array, zdepth_array, zthick_array, alphaw_array, merging_beta);
	// END of VR_MODE==1 : OPAQUE SURFACE
#else
    float sample_dist = g_cbVobj.sample_dist;
	float3 view_dir = normalize(dir_sample_ws);
	//vis_out = float4(TransformPoint(pos_ray_start_ws, g_cbVobj.mat_ws2ts), 1);
	//return;

	int start_idx = 0, sample_v = 0;
	if (hit_enc == 2)
	{
		start_idx = 1;
		float4 vis_otf = (float4) 0;
		if (Vis_Volume_And_Check(vis_otf, sample_v, pos_ray_start_ts))
		{
			float4 vis_sample = vis_otf;
			float depth_sample = depth_hit;
			INTERMIX(vis_out, idx_dlayer, num_frags, vis_sample, depth_sample, sample_dist, fs, merging_beta);
		}
	}

    [loop]
    for (int i = start_idx; i < num_ray_samples; i++)
    {
        float3 pos_sample_ts = pos_ray_start_ts + dir_sample_ts * (float) i;

        LOAD_BLOCK_INFO(blkSkip, pos_sample_ts, dir_sample_ts, num_ray_samples, i)

        if (blkSkip.blk_value > 0)
        {
	        [loop]
            for (int j = 0; j < blkSkip.num_skip_steps; j++, i++)
            {
                //float3 pos_sample_blk_ws = pos_hit_ws + dir_sample_ws * (float) i;
                float3 pos_sample_blk_ts = pos_ray_start_ts + dir_sample_ts * (float) i;
                
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
                    float depth_sample = depth_hit + (float) i * sample_dist;
#if VR_MODE == 2
					const float grad_max = 100.f;
					const float grad_scale = 18000.f;
					float modulator = min(grad_len * grad_scale / grad_max, 1.f);
					vis_sample *= modulator;
#endif
#if GHOST_EFFECT == 1
                    vis_out += vis_sample * (1.f - vis_out.a);
#else
                    INTERMIX(vis_out, idx_dlayer, num_frags, vis_sample, depth_sample, sample_dist, fs, merging_beta);
                    //INTERMIX_V1(vis_out, idx_dlayer, num_frags, vis_sample, depth_sample, fs);
                    if (vis_out.a > ERT_ALPHA)
#endif
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
#if GHOST_EFFECT == 1
	//
	float vr_thick = sample_dist * 3; // g_cbVobj.vz_thickness
	[loop]
	for (i = 0; i <= num_frags; i++) // num_frags
	{
		float depth = zdepth_array[i];
		float thick = zthick_array[i];
		//if (zdepth_array[i] > depth_hit)
		//if (depth - thick < depth_hit - vr_thick)
		{
			float z_front = depth_hit - vr_thick;// max(depth_hit - vr_thick, zdepth_array[i] - zthick_array[i]);
			zthick_array[i] = max(zdepth_array[i] - z_front, 0.01f);
		}
	}

	Fragment f_in;
	f_in.i_vis = ConvertFloat4ToUInt(vis_out);
	f_in.z = depth_hit;
	f_in.zthick = vr_thick;
	f_in.opacity_sum = vis_out.a;
	InsertLayerToSortedDeepLayers(vis_out, depth_out, f_in, fs, addr_base, num_frags, num_dlayers, merging_beta);
#else
	vis_out.rgb *= (1.f - ao_vr);
    REMAINING_MIX(vis_out, idx_dlayer, num_frags, fs);
#endif

#endif    
}

[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void DVR(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
    int2 tex2d_xy = int2(DTid.xy);

    if (DTid.x >= g_cbCamState.rt_width || DTid.y >= g_cbCamState.rt_height)
        return;
	
	const uint k_value = MAX_LAYERS;// g_cbCamState.k_value; // 각 pixel 마다 max k 가 다르다.
    uint addr_base = (DTid.y * g_cbCamState.rt_width + DTid.x) * MAX_LAYERS * 4;
	uint frag_cnt = fragment_counter[DTid.xy];
	uint vr_hit_enc = frag_cnt >> 24;
	frag_cnt = min(frag_cnt & 0xFFF, MAX_LAYERS);

#if SCULPT_MASK==1
#if OTF_MASK==1
    INVALID CASE IN THIS VERSION
#endif
#endif
    
    int num_frags = 0;
    float4 vis_out = 0;
    float depth_out = FLT_MAX;
    // consider the deep-layers are sored in order
//#define MAX_LAYERS 16
	Fragment fs[VR_MAX_LAYERS];

	float aos[MAX_LAYERS] = {0, 0, 0, 0, 0, 0, 0, 0};
	float ao_vr = 0;
	if (g_cbEnv.r_kernel_ao > 0)
	{
		float4 aos_tex0 = ao_textures[0][DTid.xy];
		float4 aos_tex1 = ao_textures[1][DTid.xy];
		aos[0] = aos_tex0.x; aos[1] = aos_tex0.y; aos[2] = aos_tex0.z; aos[3] = aos_tex0.w;
		aos[4] = aos_tex1.x; aos[5] = aos_tex1.y; aos[6] = aos_tex1.z; aos[7] = aos_tex1.w;
		ao_vr = ao_vr_texture[DTid.xy];
	}

	[loop]
    for (uint i = 0; i < MAX_LAYERS; i++)
    {
        uint i_vis = 0;
		Fragment f;
		GET_FRAG(f, addr_base, i);
		if (i < frag_cnt && f.i_vis > 0)
        {
            float4 vis_in = ConvertUIntToFloat4(f.i_vis);
			if (g_cbEnv.r_kernel_ao > 0)
			{
				vis_in.rgb *= 1.f - aos[i];
				f.i_vis = ConvertFloat4ToUInt(vis_in);
			}
            if (vis_in.a > 0)
            {
                vis_out += vis_in * (1.f - vis_out.a);
                num_frags++;
            }
        } 
        else
        {
            break;
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
    
	float merging_beta = 1.0;// asfloat(g_cbCamState.iSrCamDummy__0);

	float3 vbos_hit_start_pos = pos_ip_ws;
#if RAYMODE == 0
	depth_out = fragment_zdepth[DTid.xy];
	fragment_zdepth[tex2d_xy] = fs[0].z;
	if (depth_out > FLT_LARGE) return;
	vbos_hit_start_pos = pos_ip_ws + dir_sample_unit_ws * depth_out;
#endif
	// Ray Intersection for Clipping Box //
    float2 hits_t = ComputeVBoxHits(vbos_hit_start_pos, dir_sample_unit_ws, g_cbVobj.pos_alignedvbox_max_bs, g_cbVobj.mat_alignedvbox_tr_ws2bs, g_cbClipInfo);
	// 1st Exit in the case that there is no ray-intersecting boundary in the volume box
    int num_ray_samples = (int) ((hits_t.y - hits_t.x) / g_cbVobj.sample_dist + 0.5f);
    if (num_ray_samples <= 0)
        return;
    float3 pos_ray_start_ws = vbos_hit_start_pos + dir_sample_unit_ws * hits_t.x;
    // recompute the vis result    
#if RAYMODE==1
    RayMinMax(vis_out, depth_out, pos_ip_ws, pos_ray_start_ws, dir_sample_ws, num_ray_samples, 0, fs, addr_base, num_frags, k_value, merging_beta);
#elif RAYMODE==2
    RayMinMax(vis_out, depth_out, pos_ip_ws, pos_ray_start_ws, dir_sample_ws, num_ray_samples, 1, fs, addr_base, num_frags, k_value, merging_beta);
#elif RAYMODE==3
    RaySum(vis_out, depth_out, pos_ip_ws, pos_ray_start_ws, dir_sample_ws, num_ray_samples, fs, addr_base, num_frags, k_value, merging_beta);
#else
    RayCasting(vis_out, depth_out, pos_ip_ws, pos_ray_start_ws, dir_sample_ws, num_ray_samples, fs, ao_vr, addr_base, num_frags, vr_hit_enc, k_value, merging_beta);
#endif
	//vis_out = float4(ao_vr, ao_vr, ao_vr, 1);
	//vis_out = float4(TransformPoint(pos_ray_start_ws, g_cbVobj.mat_ws2ts), 1);
    fragment_vis[tex2d_xy] = vis_out;
    fragment_zdepth[tex2d_xy] = depth_out;
	fragment_counter[DTid.xy] = frag_cnt + 1;
}

[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void VR_SURFACE(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	int2 tex2d_xy = int2(DTid.xy);

	fragment_zdepth[DTid.xy] = FLT_MAX;

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
	float2 hits_t = ComputeVBoxHits(pos_ip_ws, dir_sample_unit_ws, g_cbVobj.pos_alignedvbox_max_bs, g_cbVobj.mat_alignedvbox_tr_ws2bs, g_cbClipInfo);
	// 1st Exit in the case that there is no ray-intersecting boundary in the volume box
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

	fragment_zdepth[DTid.xy] = depth_hit;
	uint fcnt = fragment_counter[DTid.xy];
	uint dvr_hit_enc = hit_step == 0 ? 2 : 1;
	fragment_counter[DTid.xy] = fcnt | (dvr_hit_enc << 24);
}