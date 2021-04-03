#include "../CommonShader.hlsl"
#include "../macros.hlsl"

#define DEBUG_OUT(C) {fragment_blendout[DTid.xy] = C; return;}

//RasterizerOrderedStructuredBuffer<FragmentArray> deep_kf_buf_rov : register(u7);
//RWStructuredBuffer<FragmentArray> deep_kf_buf : register(u8);

//Texture2D<uint> sr_fragment_counter : register(t0);
RWTexture2D<uint> fragment_counter : register(u0);
Buffer<float> g_bufHotSpotMask : register(t50);

RWByteAddressBuffer deep_k_buf : register(u1);
RWTexture2D<unorm float4> fragment_blendout : register(u2);
RWTexture2D<float> fragment_zdepth : register(u3);
#if DYNAMIC_K_MODE == 1
Buffer<uint> sr_offsettable_buf : register(t50); // gres_fb_ref_pidx
#endif

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

//groupshared int idx_array[k_value * 2]; // ??
//[numthreads(1, 1, 1)] // or consider 4x4 thread either
[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void OIT_RESOLVE_OLD(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
    if (DTid.x >= g_cbCamState.rt_width || DTid.y >= g_cbCamState.rt_height)
		return;
	
	uint frag_cnt = fragment_counter[DTid.xy];
	if (frag_cnt == 0)
	{
		// note that 
		// 1) PS (Filling) clears relevant pixel fragments using counter mask
		// 2) CS (DVR) clears the local fragments that have no primitive fragment (i.e., frag_cnt == 0)
		// ==> therefore, we do not clear the k-buffer here, reducing the overhead of memory-write.
		return;
	}
#ifdef DEBUG__
	else if (frag_cnt == 7777777)
	{
		fragment_blendout[DTid.xy] = float4(1, 0, 0, 1);
		return;
	}
	else if (frag_cnt == 8888888)
	{
		fragment_blendout[DTid.xy] = float4(0, 0, 1, 1);
		return;
	}
	else if (frag_cnt == 9999999)
	{
		fragment_blendout[DTid.xy] = float4(0, 1, 0, 1);
		return;
	}
#endif
	const uint k_value = g_cbCamState.k_value;
	frag_cnt = min(frag_cnt, k_value);

	// we will test our oit with the number of deep layers : 4, 8, 16 ... here, set max 32 ( larger/equal than k_value * 2 )
	uint bytes_per_frag = 4 * NUM_ELES_PER_FRAG;
#if DYNAMIC_K_MODE== 1
#define LOCAL_SIZE 64
	uint offsettable_idx = DTid.y * g_cbCamState.rt_width + DTid.x; // num of frags
	if (offsettable_idx == 0) return; // for test
	uint addr_base = sr_offsettable_buf[offsettable_idx] * bytes_per_frag;
#else
	// note that this local size is for the indexable temp registers of GPU threads 
	// recommended LOCAL_SIZE is dependent on GPU
	// if only small size of temp registers are used, then the performance is not reduced
	// in our experiments, static k is set to 8 or 16 (8 is used for all experiments)
#define LOCAL_SIZE 8 
	uint bytes_frags_per_pixel = k_value * bytes_per_frag; // to do : consider the dynamic scheme. (4 bytes unit)
	uint pixel_id = DTid.y * g_cbCamState.rt_width + DTid.x;
	uint addr_base = pixel_id * bytes_frags_per_pixel;
#endif
	
	float vz_thickness = GetHotspotThickness((int2)DTid);
	vz_thickness = max(vz_thickness, g_cbCamState.cam_vz_thickness);

	Fragment fs[LOCAL_SIZE];
	[loop]
	for (uint k = 0; k < frag_cnt; k++)
	{
		// note that k-buffer is cleared as zeros by mask-checking
		Fragment f;
		GET_FRAG(f, addr_base, k);
		fs[k] = f;
	}

	sort((int)frag_cnt, fs, Fragment);
	
	int valid_frag_cnt = frag_cnt;
	//int mer_cnt = 0;
#if FRAG_MERGING == 1
	// extended merging for consistent visibility transition
	[loop]
	for (k = 0; k < frag_cnt; k++)
	{
		Fragment f_k = fs[k];
		if (f_k.i_vis != 0)
		{
			[loop]
			for (uint l = k + 1; l < frag_cnt; l++)
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
			f_k.zthick = max(f_k.zthick, GetVZThickness(f_k.z, vz_thickness));
			fs[k] = f_k;
		}
	}
#endif

	//sort((int)frag_cnt, fs, Fragment);
	float merging_beta = asfloat(g_cbCamState.iSrCamDummy__0);

	float4 fmix_vis = (float4) 0;
	uint cnt_sorted_ztsurf = 0, i = 0;
#if FRAG_MERGING == 1
    // merge self-overlapping surfaces to thickness surfaces
    [loop]	
    for (i = 0; i < (uint)valid_frag_cnt; i++)
    {
		uint idx_ith = i;
        Fragment f_ith = fs[idx_ith];

        if (f_ith.i_vis != 0)
        {
            for (uint j = i + 1; j < (uint)valid_frag_cnt; j++)
            {
				int idx_next = j;
				Fragment f_next = fs[idx_next];
				if (f_next.i_vis != 0)
				//if (f_next.zthick > 0)
                {
					Fragment_OUT fs_out = MergeFrags(f_ith, f_next, merging_beta);
					f_ith = fs_out.f_prior;
					f_next = fs_out.f_posterior;
					fs[idx_next] = f_next;
                }
            }
			if (f_ith.i_vis != 0) // always.. (just for safe-check)
			//if (f_ith.zthick > 0) // always.. (just for safe-check)
            {
				int idx_sp = cnt_sorted_ztsurf;
				fs[idx_sp] = f_ith;
                cnt_sorted_ztsurf++;

                fmix_vis += ConvertUIntToFloat4(f_ith.i_vis) * (1.f - fmix_vis.a);
                //fmix_vis += ConvertUIntToFloat4(vis_array[idx_sp]) * (1.f - fmix_vis.a);
				if (fmix_vis.a > ERT_ALPHA)
					break;
            }
        } // if (rs_ith.zthick >= FLT_MIN__)
    }
#else
	// no thickness when zf_handling off
	cnt_sorted_ztsurf = valid_frag_cnt;
#endif
    
	// disable when SSAO is activated
	if (g_cbEnv.r_kernel_ao > 0)
	{
		cnt_sorted_ztsurf = valid_frag_cnt;
	}
	else
	{
		/*
		Fragment f_invalid = (Fragment)0;
		f_invalid.z = FLT_MAX;
		[loop]
		for (i = cnt_sorted_ztsurf; i < frag_cnt; i++)
		{
			int idx = i;
			fs[idx] = f_invalid; // when displaying layers for test, disable this.
		}
		/**/
	}

	bool store_to_kbuf = BitCheck(g_cbCamState.cam_flag, 3);
#if FRAG_MERGING == 1
	if (store_to_kbuf)
#endif
	{
		[loop]
		for (i = 0; i < cnt_sorted_ztsurf; i++)
		{
			int idx = i;
			Fragment f_ith = fs[idx];
#if FRAG_MERGING == 0
			float4 vis = ConvertUIntToFloat4(f_ith.i_vis);
			fmix_vis += vis * (1.f - fmix_vis.a);
			if (store_to_kbuf)
#endif

				SET_FRAG(addr_base, i, f_ith);
		}

#if FRAG_MERGING == 0
		if (store_to_kbuf)
#endif
			fragment_counter[DTid.xy] = cnt_sorted_ztsurf;
	}

	//fragment_blendout[DTid.xy] = ConvertUIntToFloat4(fs[idx_array[7]].i_vis);
	fragment_blendout[DTid.xy] = fmix_vis;
	fragment_zdepth[DTid.xy] = fs[0].z;
}

[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void OIT_RESOLVE(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	if (DTid.x >= g_cbCamState.rt_width || DTid.y >= g_cbCamState.rt_height)
		return;

	uint frag_cnt = fragment_counter[DTid.xy];
	if (frag_cnt == 0)
	{
		// note that 
		// 1) PS (Filling) clears relevant pixel fragments using counter mask
		// 2) CS (DVR) clears the local fragments that have no primitive fragment (i.e., frag_cnt == 0)
		// ==> therefore, we do not clear the k-buffer here, reducing the overhead of memory-write.
		return;
	}
#ifdef DEBUG__
	else if (frag_cnt == 7777777)
	{
		fragment_blendout[DTid.xy] = float4(1, 0, 0, 1);
		return;
	}
	else if (frag_cnt == 8888888)
	{
		fragment_blendout[DTid.xy] = float4(0, 0, 1, 1);
		return;
	}
	else if (frag_cnt == 9999999)
	{
		fragment_blendout[DTid.xy] = float4(0, 1, 0, 1);
		return;
	}
#endif
	const uint k_value = g_cbCamState.k_value;
	frag_cnt = min(frag_cnt, k_value);

	// we will test our oit with the number of deep layers : 4, 8, 16 ... here, set max 32 ( larger/equal than k_value * 2 )
	uint bytes_per_frag = 4 * NUM_ELES_PER_FRAG;
#if DYNAMIC_K_MODE== 1
#define LOCAL_SIZE 64
	uint offsettable_idx = DTid.y * g_cbCamState.rt_width + DTid.x; // num of frags
	if (offsettable_idx == 0) return; // for test
	uint addr_base = sr_offsettable_buf[offsettable_idx] * bytes_per_frag;
#else
	// note that this local size is for the indexable temp registers of GPU threads 
	// recommended LOCAL_SIZE is dependent on GPU
	// if only small size of temp registers are used, then the performance is not reduced
	// in our experiments, static k is set to 8 or 16 (8 is used for all experiments)
#define LOCAL_SIZE 8 
	uint bytes_frags_per_pixel = k_value * bytes_per_frag; // to do : consider the dynamic scheme. (4 bytes unit)
	uint pixel_id = DTid.y * g_cbCamState.rt_width + DTid.x;
	uint addr_base = pixel_id * bytes_frags_per_pixel;
#endif

	float vz_thickness = GetHotspotThickness((int2)DTid);
	vz_thickness = max(vz_thickness, g_cbCamState.cam_vz_thickness);

	Fragment fs[LOCAL_SIZE];
	[loop]
	for (uint k = 0; k < frag_cnt; k++)
	{
		// note that k-buffer is cleared as zeros by mask-checking
		Fragment f;
		GET_FRAG(f, addr_base, k);
		fs[k] = f;
	}

	sort((int)frag_cnt, fs, Fragment);

	int valid_frag_cnt = frag_cnt;
	//int mer_cnt = 0;
#if FRAG_MERGING == 1
	// extended merging for consistent visibility transition
	[loop]
	for (k = 0; k < frag_cnt; k++)
	{
		Fragment f_k = fs[k];
		if (f_k.i_vis != 0)
		{
			[loop]
			for (uint l = k + 1; l < frag_cnt; l++)
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
			f_k.zthick = max(f_k.zthick, GetVZThickness(f_k.z, vz_thickness));
			fs[k] = f_k;
		}
	}
#endif

	//sort((int)frag_cnt, fs, Fragment);
	float merging_beta = asfloat(g_cbCamState.iSrCamDummy__0);

	float4 fmix_vis = (float4) 0;
	uint cnt_sorted_ztsurf = 0, i = 0;
#if FRAG_MERGING == 1
	// merge self-overlapping surfaces to thickness surfaces
	[loop]
	for (i = 0; i < (uint)valid_frag_cnt; i++)
	{
		uint idx_ith = i;
		Fragment f_ith = fs[idx_ith];

		if (f_ith.i_vis != 0)
		{
			for (uint j = i + 1; j < (uint)valid_frag_cnt; j++)
			{
				int idx_next = j;
				Fragment f_next = fs[idx_next];
				if (f_next.i_vis != 0)
					//if (f_next.zthick > 0)
				{
					Fragment_OUT fs_out = MergeFrags(f_ith, f_next, merging_beta);
					f_ith = fs_out.f_prior;
					f_next = fs_out.f_posterior;
					fs[idx_next] = f_next;
				}
			}
			if (f_ith.i_vis != 0) // always.. (just for safe-check)
			//if (f_ith.zthick > 0) // always.. (just for safe-check)
			{
				int idx_sp = cnt_sorted_ztsurf;
				fs[idx_sp] = f_ith;
				cnt_sorted_ztsurf++;

				fmix_vis += ConvertUIntToFloat4(f_ith.i_vis) * (1.f - fmix_vis.a);
				//fmix_vis += ConvertUIntToFloat4(vis_array[idx_sp]) * (1.f - fmix_vis.a);
				if (fmix_vis.a > ERT_ALPHA)
					break;
			}
		} // if (rs_ith.zthick >= FLT_MIN__)
	}
#else
	// no thickness when zf_handling off
	cnt_sorted_ztsurf = valid_frag_cnt;
#endif

	// disable when SSAO is activated
	if (g_cbEnv.r_kernel_ao > 0)
	{
		cnt_sorted_ztsurf = valid_frag_cnt;
	}
	else
	{
		/*
		Fragment f_invalid = (Fragment)0;
		f_invalid.z = FLT_MAX;
		[loop]
		for (i = cnt_sorted_ztsurf; i < frag_cnt; i++)
		{
			int idx = i;
			fs[idx] = f_invalid; // when displaying layers for test, disable this.
		}
		/**/
	}

	bool store_to_kbuf = BitCheck(g_cbCamState.cam_flag, 3);
#if FRAG_MERGING == 1
	if (store_to_kbuf)
#endif
	{
		[loop]
		for (i = 0; i < cnt_sorted_ztsurf; i++)
		{
			int idx = i;
			Fragment f_ith = fs[idx];
#if FRAG_MERGING == 0
			float4 vis = ConvertUIntToFloat4(f_ith.i_vis);
			fmix_vis += vis * (1.f - fmix_vis.a);
			if (store_to_kbuf)
#endif

				SET_FRAG(addr_base, i, f_ith);
		}

#if FRAG_MERGING == 0
		if (store_to_kbuf)
#endif
			fragment_counter[DTid.xy] = cnt_sorted_ztsurf;
	}

	//fragment_blendout[DTid.xy] = ConvertUIntToFloat4(fs[idx_array[7]].i_vis);
	fragment_blendout[DTid.xy] = fmix_vis;
	fragment_zdepth[DTid.xy] = fs[0].z;
}