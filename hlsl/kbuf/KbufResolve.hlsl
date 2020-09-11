#include "CommonShader.hlsl"


//RasterizerOrderedStructuredBuffer<FragmentArray> deep_kf_buf_rov : register(u7);
//RWStructuredBuffer<FragmentArray> deep_kf_buf : register(u8);

Texture2D<uint> sr_fragment_counter : register(t0);
RWTexture2D<uint> fragment_counter : register(u0);
Buffer<float> g_bufHotSpotMask : register(t50);

RWByteAddressBuffer deep_k_buf : register(u1);
RWTexture2D<unorm float4> fragment_blendout : register(u2);
RWTexture2D<float> fragment_zdepth : register(u3);

//#define GET_FRAG(F, B) {uint4 rb = B; F.i_vis = rb.x; F.z = asfloat(rb.y); F.zthick = asfloat(rb.z); F.opacity_sum = asfloat(rb.w);}
//#define SET_FRAG(B, F) {uint4 rb = uint4(F.i_vis, asuint(F.z), asuint(F.zthick), asuint(F.opacity_sum)); B = rb;}
//#define LOAD4_KBUF(V, F_ADDR, K) V = deep_k_buf.Load4((F_ADDR + K) * 16)
//#define STORE4_KBUF(V, F_ADDR, K) deep_k_buf.Store4((F_ADDR + K) * 16, V)
//#define GET_FRAG(F, F_ADDR, K) {uint4 rb; LOAD4_KBUF(rb, F_ADDR, K); F.i_vis = rb.x; F.z = asfloat(rb.y); F.zthick = asfloat(rb.z); F.opacity_sum = asfloat(rb.w);}
//#define SET_FRAG(F_ADDR, K, F) {uint4 rb = uint4(F.i_vis, asuint(F.z), asuint(F.zthick), asuint(F.opacity_sum)); STORE4_KBUF(rb, F_ADDR, K);}

//groupshared int idx_array[k_value * 2]; // ??
//[numthreads(1, 1, 1)] // or consider 4x4 thread either
[numthreads(GRIDSIZE, GRIDSIZE, 1)]
//[numthreads(1, 1, 1)]
void OIT_RESOLVE(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
    if (DTid.x >= g_cbCamState.rt_width || DTid.y >= g_cbCamState.rt_height)
        return;
	
	uint frag_cnt = min(fragment_counter[DTid.xy], MAX_LAYERS);
	uint addr_base = (DTid.y * g_cbCamState.rt_width + DTid.x) * MAX_LAYERS * 4;
	if (frag_cnt == 0)
	{
		Fragment f_zero = (Fragment)0;
		for (uint i = 0; i < MAX_LAYERS; i++)
			SET_FRAG(addr_base, i, f_zero);
		return;
	}

	//Fragment f_test;
	//GET_FRAG(f_test, addr_base, 7);
	//fragment_blendout[DTid.xy] = ConvertUIntToFloat4(f_test.i_vis);
	//fragment_blendout[DTid.xy] = float4((float3)frag_cnt / (MAX_LAYERS - 1), 1);
	//return;
    
	float v_thickness = GetHotspotThickness((int2)DTid);
	v_thickness = max(v_thickness, g_cbCamState.cam_vz_thickness);

    // we will test our oit with the number of deep layers : 4, 8, 16 ... here, set max 32 ( larger/equal than MAX_LAYERS * 2 )
    uint idx_array[MAX_LAYERS * 2]; // for the quick-sort algorithm
	Fragment fs[MAX_LAYERS];
    uint valid_frag_cnt = 0;
	[loop]
    for (uint k = 0; k < MAX_LAYERS; k++)
    {
		// note that k-buffer is cleared as zeros by mask-checking
		Fragment f;
		GET_FRAG(f, addr_base, k);
		if (f.i_vis > 0)
		{
			f.zthick = max(f.zthick, v_thickness);
			idx_array[valid_frag_cnt] = valid_frag_cnt;
			valid_frag_cnt++;
		}
		else f.z = FLT_MAX;
		fs[k] = f;
    }
	[loop]
	for (k = valid_frag_cnt; k < MAX_LAYERS * 2; k++)
		idx_array[k] = k;

	if (valid_frag_cnt == 0)
		return;
    
    //float4 f4_ = ConvertUIntToFloat4(vis_array[MAX_LAYERS - 1]);
    //float4 f4_ = ConvertUIntToFloat4(deepbuf_vis[addr_base_vis + MAX_LAYERS - 1 + 1]);
    //float4 f4__ = ConvertUIntToFloat4(vis_array[0]);
    //fragment_blendout[DTid.xy] = f4__;
    //return;
    
    uint frag_cnt2 = 1 << (uint) (ceil(log2(valid_frag_cnt))); // pow of 2 smaller than MAX_LAYERS * 2
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
                float di = idx_i < MAX_LAYERS ? fs[idx_i].z : FLT_MAX;
                uint ixj = i ^ j;
                if ((ixj) > i)
                {
					int idx_ixj = idx_array[ixj];
                    float dixj = idx_ixj < MAX_LAYERS ? fs[idx_ixj].z : FLT_MAX;
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
    //float4 f4_ = ConvertUIntToFloat4(vis_array[idx_array[0]]);
    //fragment_blendout[DTid.xy] = f4_;
    //return;
    // Output the final result to the frame buffer
    //if (idx > 0) return;

	float merging_beta = asfloat(g_cbCamState.iSrCamDummy__0);

	float4 fmix_vis = (float4) 0;
	uint cnt_sorted_ztsurf = 0, i = 0;
#if ZF_HANDLING == 1
    // merge self-overlapping surfaces to thickness surfaces
//#define USE_RS
    [loop]	
    for (i = 0; i < valid_frag_cnt; i++)
    {
        uint idx_ith = idx_array[i];
#ifdef USE_RS
#define RST RaySegment2
        RST rs_ith;
#endif
        Fragment f_ith = fs[idx_ith];

        if (f_ith.i_vis > 0)
        {
#ifdef USE_RS
            rs_ith.fvis = ConvertUIntToFloat4(f_ith.i_vis);
            rs_ith.zdepth = f_ith.z;
            rs_ith.zthick = f_ith.zthick;
            rs_ith.alphaw = f_ith.opacity_sum;
#endif
            for (uint j = i + 1; j < valid_frag_cnt + 1; j++)
            {
                int idx_next = idx_array[j];
#ifdef USE_RS
                RST rs_next;
#endif
				Fragment f_next = fs[idx_next];
                if (f_next.i_vis > 0)
                {
#ifdef USE_RS
                    rs_next.fvis = ConvertUIntToFloat4(f_next.i_vis);
                    rs_next.zdepth = f_next.z;
					rs_next.zthick = f_next.zthick;
					rs_next.alphaw = f_next.opacity_sum;
                    MergeRS_OUT2 rs_out = MergeRS2(rs_ith, rs_next, merging_beta);
                    rs_ith = rs_out.rs_prior;
					f_next.i_vis = ConvertFloat4ToUInt(rs_out.rs_posterior.fvis);
                    f_next.z = rs_out.rs_posterior.zdepth;
                    f_next.zthick = rs_out.rs_posterior.zthick;
                    f_next.opacity_sum = rs_out.rs_posterior.alphaw;
#else
					Fragment_OUT fs_out = MergeFrags(f_ith, f_next, merging_beta);
					f_ith = fs_out.f_prior;
					f_next = fs_out.f_posterior;
#endif
					fs[idx_next] = f_next;
                }
            }
#ifdef USE_RS
			if (rs_ith.fvis.a >= FLT_OPACITY_MIN__) // always.. (just for safe-check)
#else
			if ((f_ith.i_vis >> 24) > 0) // always.. (just for safe-check)
#endif
            {
                int idx_sp = idx_array[cnt_sorted_ztsurf];
#ifdef USE_RS
				Fragment f_sp;
				f_sp.i_vis = ConvertFloat4ToUInt(rs_ith.fvis);
                f_sp.z = rs_ith.zdepth;
                f_sp.zthick = rs_ith.zthick;
                f_sp.opacity_sum = rs_ith.alphaw;
				fs[idx_sp] = f_sp;
#else
				fs[idx_sp] = f_ith;
#endif
                cnt_sorted_ztsurf++;

#ifdef USE_RS
				fmix_vis += rs_ith.fvis * (1.f - fmix_vis.a);
#else
                fmix_vis += ConvertUIntToFloat4(f_ith.i_vis) * (1.f - fmix_vis.a);
#endif
                //fmix_vis += ConvertUIntToFloat4(vis_array[idx_sp]) * (1.f - fmix_vis.a);
                if (fmix_vis.a > ERT_ALPHA)
                    i = valid_frag_cnt;
            }
            // 여기에 deep buffer 값 넣기... ==> massive loop contents 는 피하는 것이 좋으므로 아래로 뺀다.
        } // if (rs_ith.zthick >= FLT_MIN__)
    }
#else
	// no thickness when zf_handling off
	cnt_sorted_ztsurf = valid_frag_cnt;
#endif

    //float4 f4_ = ConvertUIntToFloat4(vis_array[idx_array[0]]);
    //fragment_blendout[DTid.xy] = fmix_vis;
    //return;
    
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
		for (i = cnt_sorted_ztsurf; i < MAX_LAYERS; i++)
		{
			int idx = idx_array[i];
			fs[idx] = f_invalid; // when displaying layers for test, disable this.
		}
	}
    
    float4 blendout = (float4) 0;
	[loop]
	for (i = 0; i < MAX_LAYERS; i++)
	{
		uint idx = idx_array[i];
		Fragment f_ith = fs[idx];
		if (i < cnt_sorted_ztsurf)
		{
			float4 vis = ConvertUIntToFloat4(f_ith.i_vis);
			blendout += vis * (1.f - blendout.a);
		}
		SET_FRAG(addr_base, i, f_ith);
	}

	fragment_counter[DTid.xy] = cnt_sorted_ztsurf;
	//fragment_blendout[DTid.xy] = ConvertUIntToFloat4(fs[idx_array[7]].i_vis);
    fragment_blendout[DTid.xy] = blendout;
	fragment_zdepth[DTid.xy] = fs[idx_array[0]].z;
}