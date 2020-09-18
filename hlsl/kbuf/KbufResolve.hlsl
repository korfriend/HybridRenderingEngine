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

//groupshared int idx_array[k_value * 2]; // ??
//[numthreads(1, 1, 1)] // or consider 4x4 thread either
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
	uint bytes_per_frag = 4 * 4;
#if DYNAMIC_K_MODE== 1
#define LOCAL_SIZE 100
	uint offsettable_idx = DTid.y * g_cbCamState.rt_width + DTid.x; // num of frags
	if (offsettable_idx == 0) return;
	uint addr_base = sr_offsettable_buf[offsettable_idx] * bytes_per_frag;
#else
	// note that this local size is for the indexable temp registers of GPU threads 
	// recommended LOCAL_SIZE is dependent on GPU
	// if only small size of temp registers are used, then the performance is not reduced
	// in our experiments, static k scheme is set to 8 or 16 (8 is used for all experiments)
#define LOCAL_SIZE 8 
	uint bytes_frags_per_pixel = k_value * bytes_per_frag; // to do : consider the dynamic scheme. (4 bytes unit)
	uint pixel_id = DTid.y * g_cbCamState.rt_width + DTid.x;
	uint addr_base = pixel_id * bytes_frags_per_pixel;
#endif
	
	float v_thickness = GetHotspotThickness((int2)DTid);
	v_thickness = max(v_thickness, g_cbCamState.cam_vz_thickness);

//#define QUICK_SORT_AND_INDEX_DRIVEN 1
#ifdef QUICK_SORT_AND_INDEX_DRIVEN
	uint idx_array[LOCAL_SIZE * 2]; // for the quick-sort algorithm
	Fragment fs[LOCAL_SIZE];
	uint valid_frag_cnt = 0;
	for (uint k = 0; k < frag_cnt; k++)
	{
		// note that k-buffer is cleared as zeros by mask-checking
		Fragment f;
		GET_FRAG(f, addr_base, k);
		if (f.i_vis > 0)
		{
			f.zthick = max(f.zthick, v_thickness);
			idx_array[valid_frag_cnt] = valid_frag_cnt;
			fs[valid_frag_cnt] = f;
			valid_frag_cnt++;
		}
	}
	[loop]
	for (k = valid_frag_cnt; k < LOCAL_SIZE * 2; k++)
	{
		idx_array[k] = k;
		if (k < MAX_LAYERS)
		{
			Fragment f = (Fragment)0;
			f.opacity_sum = 0;
			f.z = FLT_MAX;
			fs[k] = f;
		}
	}

	if (valid_frag_cnt == 0)
		return;
    
    uint frag_cnt2 = 1 << (uint) (ceil(log2(valid_frag_cnt))); // pow of 2 smaller than LOCAL_SIZE * 2
//  uint idx = blocksize * GTid.y + GTid.x;
    // quick sort vs bitonic sort
	[loop] 
    for (k = 2; k <= frag_cnt2; k = 2 * k)
    {
        [allow_uav_condition][loop]
        for (uint j = k >> 1; j > 0; j = j >> 1)
        {
            [allow_uav_condition][loop]
            for (uint i = 0; i < frag_cnt2; i++)
            {
//              GroupMemoryBarrierWithGroupSync();
                //i = idx;
				int idx_i = idx_array[i];
                float di = idx_i < LOCAL_SIZE ? fs[idx_i].z : FLT_MAX;
                uint ixj = i ^ j;
                if ((ixj) > i)
                {
					int idx_ixj = idx_array[ixj];
                    float dixj = idx_ixj < LOCAL_SIZE ? fs[idx_ixj].z : FLT_MAX;
                    if ((i & k) == 0 && di > dixj)
                    {
                        uint temp = idx_array[i];
                        idx_array[i] = idx_array[ixj];
                        idx_array[ixj] = temp;
                    }
                    if ((i & k) != 0 && di < dixj)
                    {
                        uint temp = idx_array[i];
                        idx_array[i] = idx_array[ixj];
                        idx_array[ixj] = temp;
                    }
                }
            }
        }
    }
#else
	Fragment fs[LOCAL_SIZE];
	int valid_frag_cnt = 0;
	[loop]
	for (uint k = 0; k < frag_cnt; k++)
	{
		// note that k-buffer is cleared as zeros by mask-checking
		Fragment f;
		GET_FRAG(f, addr_base, k);
		if ((uint)f.i_vis > 0)
		{
			f.zthick = max(f.zthick, v_thickness);
			fs[valid_frag_cnt] = f;
			valid_frag_cnt++;
		}
	}

	sort(valid_frag_cnt, fs, Fragment);
#endif

	float merging_beta = asfloat(g_cbCamState.iSrCamDummy__0);

	float4 fmix_vis = (float4) 0;
	uint cnt_sorted_ztsurf = 0, i = 0;
#if ZF_HANDLING == 1
    // merge self-overlapping surfaces to thickness surfaces
    [loop]	
    for (i = 0; i < (uint)valid_frag_cnt; i++)
    {
#ifdef QUICK_SORT_AND_INDEX_DRIVEN
        uint idx_ith = idx_array[i];
#else
		uint idx_ith = i;
#endif
        Fragment f_ith = fs[idx_ith];

        if (f_ith.i_vis > 0)
        {
			
            for (uint j = i + 1; j < (uint)valid_frag_cnt; j++)
            {
#ifdef QUICK_SORT_AND_INDEX_DRIVEN
                int idx_next = idx_array[j];
#else
				int idx_next = j;
#endif
				Fragment f_next = fs[idx_next];
                if (f_next.i_vis > 0)
                {
					Fragment_OUT fs_out = MergeFrags(f_ith, f_next, merging_beta);
					f_ith = fs_out.f_prior;
					f_next = fs_out.f_posterior;
					fs[idx_next] = f_next;
                }
            }
			if ((f_ith.i_vis >> 24) > 0) // always.. (just for safe-check)
            {
#ifdef QUICK_SORT_AND_INDEX_DRIVEN
                int idx_sp = idx_array[cnt_sorted_ztsurf];
#else
				int idx_sp = cnt_sorted_ztsurf;
#endif
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
		Fragment f_invalid = (Fragment)0;
		f_invalid.z = FLT_MAX;
		[loop]
		for (i = cnt_sorted_ztsurf; i < frag_cnt; i++)
		{
#ifdef QUICK_SORT_AND_INDEX_DRIVEN
			int idx = idx_array[i];
#else
			int idx = i;
#endif
			fs[idx] = f_invalid; // when displaying layers for test, disable this.
		}
	}
    
    float4 blendout = (float4) 0;
	[loop]
	for (i = 0; i < cnt_sorted_ztsurf; i++)
	{
#ifdef QUICK_SORT_AND_INDEX_DRIVEN
		int idx = idx_array[i];
#else
		int idx = i;
#endif
		Fragment f_ith = fs[idx];
		float4 vis = ConvertUIntToFloat4(f_ith.i_vis);
		blendout += vis * (1.f - blendout.a);
#if SKIP_STORE_KBUF == 0
		SET_FRAG(addr_base, i, f_ith);
#endif
	}

	fragment_counter[DTid.xy] = cnt_sorted_ztsurf;
	//fragment_blendout[DTid.xy] = ConvertUIntToFloat4(fs[idx_array[7]].i_vis);
    fragment_blendout[DTid.xy] = blendout;
#ifdef QUICK_SORT_AND_INDEX_DRIVEN
	fragment_zdepth[DTid.xy] = fs[idx_array[0]].z;
#else
	fragment_zdepth[DTid.xy] = fs[0].z;
#endif
}
