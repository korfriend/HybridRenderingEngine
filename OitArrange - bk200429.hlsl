#include "CommonShader.hlsl"


//RasterizerOrderedStructuredBuffer<FragmentArray> deep_kf_buf_rov : register(u7);
//RWStructuredBuffer<FragmentArray> deep_kf_buf : register(u8);

Texture2D<uint> sr_fragment_counter : register(t0);
#if USE_ROV == 1
struct FragmentArray
{
	uint i_vis[MAX_LAYERS];
	float z[MAX_LAYERS];
	float zthick[MAX_LAYERS];
	float opacity_sum[MAX_LAYERS];
};
RWStructuredBuffer<FragmentArray> deep_kf_buf : register(u8);
#else
RWBuffer<uint> deep_k_buf : register(u1);
#endif
RWTexture2D<unorm float4> fragment_blendout : register(u2);
RWTexture2D<float> fragment_zdepth : register(u3);


//groupshared int idx_array[num_deep_layers * 2]; // ??
//[numthreads(1, 1, 1)] // or 4x4 thread 도 생각해 보자...
[numthreads(GRIDSIZE, GRIDSIZE, 1)]
//[numthreads(1, 1, 1)]
void OIT_ARRAGNGE(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
    if (DTid.x >= g_cbCamState.rt_width || DTid.y >= g_cbCamState.rt_height)
        return;
#if USE_ROV == 1
	uint xy_addr = DTid.y * g_cbCamState.rt_width + DTid.x;
	FragmentArray frag_dump = deep_kf_buf[xy_addr];
	if (frag_dump.opacity_sum[0] == 0)
		return;
#else
    //const uint num_deep_layers = g_cbCamState.num_deep_layers;
    uint addr_base = (DTid.y * g_cbCamState.rt_width + DTid.x) * MAX_LAYERS * 3;
    uint frag_cnt = sr_fragment_counter[DTid.xy];
    //if (frag_cnt > 100)
    //{
    //    fragment_blendout[DTid.xy] = float4(0, 1, 0, 1);
    //    return;
    //}
	//if (frag_cnt > 0)
	//{
	//	if (frag_cnt == 1)
	//		fragment_blendout[DTid.xy] = float4(255, 127, 0, 255) / 255.;
	//	else if (frag_cnt == 2)
	//		fragment_blendout[DTid.xy] = float4(152, 78, 163, 255) / 255.;
	//	else if (frag_cnt == 3)
	//		fragment_blendout[DTid.xy] = float4(78, 175, 74, 255) / 255.;
	//	else if (frag_cnt == 4)
	//		fragment_blendout[DTid.xy] = float4(54, 127, 184, 255) / 255.;
	//	else if (frag_cnt >= 5)
	//		fragment_blendout[DTid.xy] = float4(228, 26, 27, 255) / 255.;
	//	//else if (frag_cnt >= 6)
	//	//	fragment_blendout[DTid.xy] = float4(1, 0, 1, 1);
	//	return;
	//}

	if (frag_cnt == 0)
	{
		for (uint i = 0; i < MAX_LAYERS; i++)
		{
			deep_k_buf[addr_base + 3 * i + 0] = 0;
			deep_k_buf[addr_base + 3 * i + 1] = 0;
			deep_k_buf[addr_base + 3 * i + 2] = 0;
		}
		return;
	}
#endif
    
#define TC_MODE 1
    
    // we will test our oit with the number of deep layers : 4, 8, 16 ... here, set max 32 ( larger/equal than MAX_LAYERS * 2 )
//#define MAX_LAYERS 16
    uint vis_array[MAX_LAYERS * 2];
    float zdepth_array[MAX_LAYERS * 2];
    float zthick_array[MAX_LAYERS * 2];
#if TC_MODE == 1
    float alphaw_array[MAX_LAYERS * 2];
#endif
    uint idx_array[MAX_LAYERS * 2];
    uint valid_frag_cnt = 0;
    [allow_uav_condition][loop]
    for (uint ii = 0; ii < MAX_LAYERS; ii++)
    {
#if USE_ROV == 1
		uint i_vis = frag_dump.i_vis[ii];
		if (i_vis > 0)
		{
			idx_array[valid_frag_cnt] = valid_frag_cnt;
			vis_array[valid_frag_cnt] = i_vis;
			zdepth_array[valid_frag_cnt] = frag_dump.z[ii];
			alphaw_array[valid_frag_cnt] = frag_dump.opacity_sum[ii];
			zthick_array[valid_frag_cnt] = frag_dump.zthick[ii];
			valid_frag_cnt++;
		}
#else
        uint i_vis = ii < frag_cnt ? deep_k_buf[addr_base + 3 * ii + 0] : 0;
        if (i_vis > 0)
        {
            idx_array[valid_frag_cnt] = valid_frag_cnt;
            vis_array[valid_frag_cnt] = i_vis;
            zdepth_array[valid_frag_cnt] = asfloat(deep_k_buf[addr_base + 3 * ii + 1]);
#if TC_MODE == 1
            uint tc = deep_k_buf[addr_base + 3 * ii + 2];
            alphaw_array[valid_frag_cnt] = (tc >> 16) / 255.f;
            zthick_array[valid_frag_cnt] = max(f16tof32(tc), g_cbCamState.cam_vz_thickness); //f16tof32(tc);
#else
            zthick_array[valid_frag_cnt] = max(asfloat(deep_k_buf[addr_base + 3 * ii + 2]), g_cbCamState.cam_vz_thickness);
#endif
            valid_frag_cnt++;
        }
#endif
    }

	if (valid_frag_cnt == 0) return;
    
    //float4 f4_ = ConvertUIntToFloat4(vis_array[MAX_LAYERS - 1]);
    //float4 f4_ = ConvertUIntToFloat4(deepbuf_vis[addr_base_vis + MAX_LAYERS - 1 + 1]);
    //float4 f4__ = ConvertUIntToFloat4(vis_array[0]);
    //fragment_blendout[DTid.xy] = f4__;
    //return;
    
    uint frag_cnt2 = 1 << (uint) (ceil(log2(valid_frag_cnt))); // pow of 2 smaller than MAX_LAYERS * 2

    [allow_uav_condition][loop]
    for (ii = valid_frag_cnt; ii < MAX_LAYERS * 2; ii++)
    {
        idx_array[ii] = ii;
        vis_array[ii] = 0;
        zdepth_array[ii] = FLT_MAX;
        zthick_array[ii] = 0;
#if TC_MODE == 1
        alphaw_array[ii] = 0;
#endif
    }
    
//  uint idx = blocksize * GTid.y + GTid.x;
    // quick sort 로 대체하기...
    // Bitonic sort
    [allow_uav_condition][loop] 
    for (uint k = 2; k <= frag_cnt2; k = 2 * k)
    {
        [allow_uav_condition][loop]
        for (uint j = k >> 1; j > 0; j = j >> 1)
        {
            [allow_uav_condition][loop]
            for (uint i = 0; i < frag_cnt2; i++)
            {
//              GroupMemoryBarrierWithGroupSync();
                //i = idx;
                float di = zdepth_array[idx_array[i]];
                uint ixj = i ^ j;
                if ((ixj) > i)
                {
                    float dixj = zdepth_array[idx_array[ixj]];
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
#if TC_MODE == 1
#define RST RaySegment2
#else
#define RST RaySegment
#endif

	float merging_beta = asfloat(g_cbCamState.iSrCamDummy__0);

	float4 fmix_vis = (float4) 0;
	uint cnt_sorted_ztsurf = 0, i = 0;
#if ZF_HANDLING == 1
    // merge self-overlapping surfaces to thickness surfaces
    [allow_uav_condition][loop]
    for (i = 0; i < valid_frag_cnt; i++)
    {
        uint idx_ith = idx_array[i];
        RST rs_ith;
        uint ivis_ith = vis_array[idx_ith];

        if (ivis_ith > 0)
        {
            rs_ith.fvis = ConvertUIntToFloat4(ivis_ith);
            rs_ith.zdepth = zdepth_array[idx_ith];
            rs_ith.zthick = zthick_array[idx_ith];
#if TC_MODE == 1
            rs_ith.alphaw = alphaw_array[idx_ith];
#endif
        
            [allow_uav_condition][loop]
            for (uint j = i + 1; j < valid_frag_cnt + 1; j++)
            {
                int idx_next = idx_array[j];
                RST rs_next;
                uint ivis_next = vis_array[idx_next];
                if (ivis_next > 0)
                {
                    rs_next.fvis = ConvertUIntToFloat4(ivis_next);
                    rs_next.zdepth = zdepth_array[idx_next];
                    rs_next.zthick = zthick_array[idx_next];
#if TC_MODE == 1
                    rs_next.alphaw = alphaw_array[idx_next];
                    MergeRS_OUT2 rs_out = MergeRS2(rs_ith, rs_next, merging_beta);
#else
                    MergeRS_OUT rs_out = MergeRS(rs_ith, rs_next);
#endif
                    rs_ith = rs_out.rs_prior;
                    vis_array[idx_next] = ConvertFloat4ToUInt(rs_out.rs_posterior.fvis);
                    zdepth_array[idx_next] = rs_out.rs_posterior.zdepth;
                    zthick_array[idx_next] = rs_out.rs_posterior.zthick;
#if TC_MODE == 1
                    alphaw_array[idx_next] = rs_out.rs_posterior.alphaw;
#endif
                }
            }
        
            if (rs_ith.fvis.a >= FLT_OPACITY_MIN__) // always.. (just for safe-check)
            {
                int idx_sp = idx_array[cnt_sorted_ztsurf];
                vis_array[idx_sp] = ConvertFloat4ToUInt(rs_ith.fvis);
                zdepth_array[idx_sp] = rs_ith.zdepth;
                zthick_array[idx_sp] = rs_ith.zthick;
#if TC_MODE == 1
                alphaw_array[idx_sp] = rs_ith.alphaw;
#endif
                cnt_sorted_ztsurf++;
            
                fmix_vis += rs_ith.fvis * (1.f - fmix_vis.a);
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
    //fragment_blendout[DTid.xy] = f4_;
    //return;
    
    [loop]
    for (i = cnt_sorted_ztsurf; i < MAX_LAYERS; i++)
    {
        int idx = idx_array[i];
        vis_array[idx] = 0;
        zdepth_array[idx] = FLT_MAX;
        zthick_array[idx] = 0;
#if TC_MODE == 1
        alphaw_array[idx] = 0;
#endif
    }
    
    float4 blendout = (float4) 0;
#if USE_ROV == 1
    [unroll]
    for (i = 0; i < MAX_LAYERS; i++)
    {
        uint idx = idx_array[i];
        uint ivis = vis_array[idx];
        if (i < cnt_sorted_ztsurf)
        {
            float4 vis = ConvertUIntToFloat4(ivis);
            blendout += vis * (1.f - blendout.a);
        }
		frag_dump.i_vis[i] = ivis;
		frag_dump.z[i] = zdepth_array[idx];
		frag_dump.zthick[i] = zthick_array[idx];
		frag_dump.opacity_sum[i] = alphaw_array[idx];
    }
	deep_kf_buf[xy_addr] = frag_dump;
#else
    [allow_uav_condition][loop]
    for (i = 0; i < MAX_LAYERS; i++)
    {
		uint idx = idx_array[i];
        uint ivis = vis_array[idx];
        if (i < cnt_sorted_ztsurf)
        {
            float4 vis = ConvertUIntToFloat4(ivis);
            blendout += vis * (1.f - blendout.a);
        }
        deep_k_buf[addr_base + 3 * i + 0] = ivis;
        deep_k_buf[addr_base + 3 * i + 1] = asuint(zdepth_array[idx]);
#if TC_MODE == 1
        uint tc = (uint(min(alphaw_array[idx], 255) * 255) << 16) | f32tof16(zthick_array[idx]);
        deep_k_buf[addr_base + 3 * i + 2] = tc;
#else
        deep_k_buf[addr_base + 3 * i + 2] = asuint(zthick_array[idx]);
#endif
		// TR test
		deep_k_buf[addr_base + 3 * i + 2] = asuint(zthick_array[idx]);
    }
#endif

    //fragment_blendout[DTid.xy] = fmix_vis;
    fragment_blendout[DTid.xy] = blendout;
	fragment_zdepth[DTid.xy] = zdepth_array[idx_array[i - 1]] - zthick_array[idx_array[i - 1]];//zdepth_array[idx_array[0]];
}