
#include "../CommonShader.hlsl"
#include "../macros.hlsl"

struct HxCB_TestBuffer
{
	//uint testIntValues[16];
	//float testFloatValues[16];

	uint testA;
	uint testB;
	uint testC;
	uint testD;
};

cbuffer cbGlobalParams : register(b8)
{
	HxCB_TestBuffer g_cbTestBuffer;
}

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
RWTexture2D<float4> fragment_vis : register(u2);
RWTexture2D<float> fragment_zdepth : register(u3);

RWTexture2D<float> vr_fragment_1sthit_write : register(u4);

#define AO_MAX_LAYERS 8 

Texture2DArray<unorm float> ao_textures : register(t10);
Texture2D<unorm float> ao_vr_texture : register(t20);

Buffer<float3> buf_curvePoints : register(t30);
Buffer<float3> buf_curveTangents : register(t31);

#define VR_MAX_LAYERS 9 // SKBTZ, +1 for DVR 

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
bool Sample_Volume_And_Check(inout float sample_v, const float3 pos_sample_ts, const int min_valid_v)
{
	sample_v = tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_sample_ts, 0).r;
	//sample_v = (int) (fsample * g_cbVobj.value_range + 0.5f);
	return (sample_v * g_cbTmap.tmap_size_x) >= min_valid_v;
}

bool Vis_Volume_And_Check(inout float4 vis_otf, inout float sample_v, const float3 pos_sample_ts)
{
	sample_v = tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_sample_ts, 0).r;

	vis_otf = LoadOtfBuf(sample_v * g_cbTmap.tmap_size_x, buf_otf, g_cbVobj.opacity_correction);
	return vis_otf.a >= FLT_OPACITY_MIN__;
}

bool Vis_Volume_And_Check_Slab(inout float4 vis_otf, inout float sample_v, float sample_prev, const float3 pos_sample_ts)
{
	sample_v = tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_sample_ts, 0).r;
	vis_otf = LoadSlabOtfBuf_PreInt(sample_v * g_cbTmap.tmap_size_x, sample_prev * g_cbTmap.tmap_size_x, buf_preintotf, g_cbVobj.opacity_correction);
	return vis_otf.a >= FLT_OPACITY_MIN__;
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

[numthreads(GRIDSIZE_VR, GRIDSIZE_VR, 1)]
void RayCasting(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	int2 tex2d_xy = int2(DTid.xy);
	if (DTid.x >= g_cbCamState.rt_width || DTid.y >= g_cbCamState.rt_height)
		return;


	if (g_cbTestBuffer.testA == 777) {
		fragment_vis[tex2d_xy] = float4(0, 0, 1, 1);
		return;
	}

	const uint k_value = g_cbCamState.k_value;
	uint bytes_per_frag = 4 * NUM_ELES_PER_FRAG;
	uint pixel_id = tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x;

	uint bytes_frags_per_pixel = k_value * bytes_per_frag;
	uint addr_base = pixel_id * bytes_frags_per_pixel;

	uint num_frags = fragment_counter[DTid.xy];
#define __VRHIT_ON_CLIPPLANE 2
#define __VRHIT_OUTSIDE_CLIPPLANE 1
#define __VRHIT_OUTSIDE_VOLUME 0
	// 2 : on the clip plane
	// 1 : outside the clip plane
	// 0 : outside the volume
	uint vr_hit_enc = num_frags >> 24;
	num_frags = num_frags & 0xFFF;

	float4 vis_out = 0;
	float depth_out = FLT_MAX;
	// consider the deep-layers are sored in order
	Fragment fs[VR_MAX_LAYERS];

	[loop]
	for (int i = 0; i < (int)num_frags; i++)
	{
		uint i_vis = 0;
		Fragment f;
		GET_FRAG(f, addr_base, i); // from K-buffer
		float4 vis_in = ConvertUIntToFloat4(f.i_vis);
		if (g_cbEnv.r_kernel_ao > 0)
		{
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

	// Image Plane's Position and Camera State //
	float3 pos_ip_ss = float3(tex2d_xy, 0.0f);
	float3 pos_ip_ws = TransformPoint(pos_ip_ss, g_cbCamState.mat_ss2ws);
	float3 dir_sample_unit_ws = g_cbCamState.dir_view_ws;
	if (g_cbCamState.cam_flag & 0x1)
		dir_sample_unit_ws = pos_ip_ws - g_cbCamState.pos_cam_ws;
	dir_sample_unit_ws = normalize(dir_sample_unit_ws);
	float3 dir_sample_ws = dir_sample_unit_ws * g_cbVobj.sample_dist;

	const float merging_beta = 1.0;

	////////////////////////////////
	// Read 1st Hit Surface
	float3 vbos_hit_start_pos = pos_ip_ws;
	//depth_out = fragment_zdepth[DTid.xy];
	// use the result from VR_SURFACE
	depth_out = vr_fragment_1sthit_read[DTid.xy];

	vbos_hit_start_pos = pos_ip_ws + dir_sample_unit_ws * depth_out;
	fragment_zdepth[tex2d_xy] = fs[0].z;

	if (vr_hit_enc == __VRHIT_OUTSIDE_VOLUME)
	{
		return;
	}

	// Ray Intersection for Clipping Box //
	float2 hits_t = ComputeVBoxHits(vbos_hit_start_pos, dir_sample_unit_ws, g_cbVobj.mat_alignedvbox_tr_ws2bs, g_cbClipInfo);
	// 1st Exit in the case that there is no ray-intersecting boundary in the volume box
	hits_t.y = min(g_cbCamState.far_plane, hits_t.y); // only available in orthogonal view (thickness slicer)
	int num_ray_samples = ceil((hits_t.y - hits_t.x) / g_cbVobj.sample_dist);
	if (num_ray_samples <= 0)
		return;
	// note hits_t.x >= 0
	float3 pos_ray_start_ws = vbos_hit_start_pos + dir_sample_unit_ws * hits_t.x;
	////////////////////////////////
	
	// recompute the vis result  

	// DVR ray-casting core part
	vis_out = 0;
	depth_out = FLT_MAX;

	float depth_hit = depth_out = length(pos_ray_start_ws - pos_ip_ws);
	float sample_dist = g_cbVobj.sample_dist;

	float3 pos_ray_start_ts = TransformPoint(pos_ray_start_ws, g_cbVobj.mat_ws2ts);
	float3 dir_sample_ts = TransformVector(dir_sample_ws, g_cbVobj.mat_ws2ts);

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

	// note that raycasters except vismask mode (or x-ray) use SLAB sample
	start_idx = 1;
	//float3 grad_prev = GRAD_VOL(pos_ray_start_ts);
	float3 grad_prev = GradientVolume(pos_ray_start_ts, g_cbVobj.vec_grad_x, g_cbVobj.vec_grad_y, g_cbVobj.vec_grad_z, tex3D_volume);
	float sample_prev = sample_v;

	if (vr_hit_enc == __VRHIT_ON_CLIPPLANE) // on the clip plane
	{
		float4 vis_otf = (float4) 0;
		start_idx++;

		float3 pos_sample_ts = pos_ray_start_ts + dir_sample_ts;

		sample_v = tex3D_volume.SampleLevel(g_samplerLinear_clamp, pos_sample_ts, 0).r;
		float3 grad = GRAD_VOL(pos_sample_ts);
		float3 gradSlab = grad + grad_prev;
		grad_prev = grad;
		if (Vis_Volume_And_Check_Slab(vis_otf, sample_v, sample_prev, pos_sample_ts))
		{
			float depth_sample = depth_hit;
			// note that depth_hit is the front boundary of slab
			depth_sample += sample_dist; // slab's back boundary
			float4 vis_sample = vis_otf;

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

	bool isPrevValid = true;
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
					float shade = 1.f;
					if (grad_len > 0)
						shade = PhongBlinnVr(view_dir, g_cbVobj.pb_shading_factor, light_dirinv, nrl, true);

					float4 vis_sample = float4(shade * vis_otf.rgb, vis_otf.a);
					float depth_sample = depth_hit + (float)(i + j) * sample_dist;

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
				else {
					isPrevValid = false;
				}
				sample_prev = sample_v;
				} // for (int j = 0; j < blkSkip.num_skip_steps; j++, i++)
			}
		else
		{
			sample_count++;
			sample_prev = 0;
			isPrevValid = false;
			grad_prev = (float3)0;
		}
		i += blkSkip.num_skip_steps;
		// this is for outer loop's i++
		//i -= 1;
	}

	REMAINING_MIX(vis_out, idx_dlayer, num_frags, fs);


	fragment_vis[tex2d_xy] = vis_out;
	fragment_vis[tex2d_xy] = float4(0, 1, 1, 1);
	if(g_cbTestBuffer.testA == 0)
		fragment_vis[tex2d_xy] = float4(1, 1, 0, 1);
	fragment_zdepth[tex2d_xy] = depth_out;
}

[numthreads(GRIDSIZE_VR, GRIDSIZE_VR, 1)]
void RayCasting2(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	int2 tex2d_xy = int2(DTid.xy);
	if (DTid.x >= g_cbCamState.rt_width || DTid.y >= g_cbCamState.rt_height)
		return;

	const uint k_value = g_cbCamState.k_value;
	uint bytes_per_frag = 4 * NUM_ELES_PER_FRAG;
	uint pixel_id = tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x;

	uint bytes_frags_per_pixel = k_value * bytes_per_frag;
	uint addr_base = pixel_id * bytes_frags_per_pixel;

	uint num_frags = fragment_counter[DTid.xy];
#define __VRHIT_ON_CLIPPLANE 2
#define __VRHIT_OUTSIDE_CLIPPLANE 1
#define __VRHIT_OUTSIDE_VOLUME 0
	// 2 : on the clip plane
	// 1 : outside the clip plane
	// 0 : outside the volume
	uint vr_hit_enc = num_frags >> 24;
	num_frags = num_frags & 0xFFF;

	fragment_vis[tex2d_xy] = float4(0, 0, 1, 1);
	fragment_zdepth[tex2d_xy] = 0;
}
