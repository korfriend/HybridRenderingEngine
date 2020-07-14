#include "CommonShader.hlsl"

RWBuffer<uint> deep_k_buf : register(u1); 
RWTexture2D<float4> fragment_blendout : register(u2);
RWTexture2D<float> fragment_zdepth : register(u3);
RWBuffer<uint> deep_tmp_buf : register(u4);

Texture2D<uint> sr_fragment_counter : register(t0); 
Texture2D<uint> sr_fragment_tmp_w_counter : register(t1); 

#define __InterlockedExchange(A, B, C) A = B
//#define __InterlockedExchange(A, B, C) InterlockedExchange(A, B, C)
void __UpdateTailSlot(const in uint addr_base, const in uint num_deep_layers, const in float4 vis_in, const in float alphaw, const in float zdepth_in, const in float zthick_in)
{
    uint addr_merge_slot = addr_base + 3 * (num_deep_layers - 1);
    uint tc = deep_k_buf[addr_merge_slot + 2];
    float A_sum_prev = (tc >> 16) / 255.f;
    uint i_V_sum, i_depth, i_tc;
    uint __dummy;
    if (A_sum_prev == 0)
    {
        i_V_sum = ConvertFloat4ToUInt(vis_in);
        i_depth = asuint(zdepth_in);
        i_tc = f32tof16(zthick_in) | (uint(alphaw * 255) << 16);
    }
    else
    {
        float4 V_mix_prev = ConvertUIntToFloat4(deep_k_buf[addr_merge_slot + 0]);
        float3 C_sum_prev = V_mix_prev.rgb / V_mix_prev.a * A_sum_prev;
        

        float A_sum = A_sum_prev + alphaw;
        float3 C_sum = C_sum_prev + vis_in.rgb / vis_in.a * alphaw; // consider associated color
        float3 I_sum = C_sum / A_sum;
        float A_mix = 1 - (1 - V_mix_prev.a) * (1.f - vis_in.a);
        
        i_V_sum = ConvertFloat4ToUInt(float4(I_sum * A_mix, A_mix));
        float zdepth_prev_merge = asfloat(deep_k_buf[addr_merge_slot + 1]);
        float zthick_prev_merge = f16tof32(tc);
        float new_back_z = zdepth_in, new_front_z = zthick_in;
        if (zthick_prev_merge > 0)
        {
            new_back_z = max(zdepth_in, zdepth_prev_merge);
            new_front_z = min(zdepth_in - zthick_in, zdepth_prev_merge - zthick_prev_merge);
        }
        i_depth = asuint(new_back_z);
        //i_thick = asuint(new_back_z - new_front_z);
        i_tc = f32tof16(new_back_z - new_front_z) | (min(65535, uint(A_sum * 255.f)) << 16);
    }
    
	// inout 으로 하고 밖으로?! InterlockedExchange 으로 test 해 보기
    __InterlockedExchange(deep_k_buf[addr_merge_slot + 0], i_V_sum, __dummy);
    __InterlockedExchange(deep_k_buf[addr_merge_slot + 1], i_depth, __dummy);
    __InterlockedExchange(deep_k_buf[addr_merge_slot + 2], i_tc, __dummy);
}

//void MergeTmpBufferToKBuffer(const in uint addr_base, const in uint2 tex2d_xy, const in int num_deep_layers)
//{
//    uint w_cnt = sr_fragment_tmp_w_counter[tex2d_xy.xy];
//    if (w_cnt % (num_deep_layers + 1) == 0)
//        return;
//    uint __dummy;
//    
//    #define MAX_LAYERS_TMP 9
//    uint vis_array[MAX_LAYERS_TMP];
//    float zdepth_array[MAX_LAYERS_TMP];
//    float zthick_array[MAX_LAYERS_TMP];
//    float alphaw_array[MAX_LAYERS_TMP];
//    [allow_uav_condition][loop]
//    for (uint i = 0; i < w_cnt; i++)
//    {
//        vis_array[i] = deep_tmp_buf[addr_base + i * 3 + 0];
//        zdepth_array[i] = deep_tmp_buf[addr_base + i * 3 + 1];
//        uint tc = deep_tmp_buf[addr_base + i * 3 + 2];
//        zthick_array[i] = f16tof32(tc);
//        alphaw_array[i] = (tc >> 16) / 255.f;
//    }
//                
//    int addr_merge_slot = addr_base + 3 * (num_deep_layers - 1);
//    float tail_z = asfloat(deep_k_buf[addr_merge_slot + 1]);
//    if (tail_z == 0)
//        tail_z = FLT_MAX;
//    uint tc = deep_k_buf[addr_merge_slot + 2];
//    float tail_thickness = f16tof32(tc); //  & 0xFFFF
//                    
//    [allow_uav_condition][loop]
//    for (uint ii = 0; ii < w_cnt; ii++)
//    {
//        uint _i_rgba = vis_array[ii];
//        if (_i_rgba == 0)
//            continue;
//        float4 _v_rgba = ConvertUIntToFloat4(_i_rgba);
//        float _z_depth = zdepth_array[ii];
//        float _z_thickness = zthick_array[ii];
//        float _alphaw = alphaw_array[ii];
//        if (_z_depth > tail_z - tail_thickness)
//        {
//            // update the merging slot
//            __UpdateTailSlot(addr_base, num_deep_layers, _v_rgba, _v_rgba.a, _z_depth, _z_thickness);
//            float front_z = min(_z_depth - _z_thickness, tail_z - tail_thickness);
//            tail_z = _z_depth;
//            tail_thickness = tail_z - front_z;
//        }
//        else
//        {
//            RaySegment2 rs_cur;
//            rs_cur.zdepth = _z_depth;
//            rs_cur.fvis = _v_rgba;
//            rs_cur.zthick = _z_thickness;
//            rs_cur.alphaw = _v_rgba.a;
//            int empty_slot = -1;
//            int core_max_zidx = -1;
//            float core_max_z = -1.f;
//            [allow_uav_condition][loop]
//            for (int i = 0; i < num_deep_layers - 1; i++) 
//            {
//                // after atomic store operation
//                uint i_vis_ith = deep_k_buf[addr_base + 3 * i + 0];
//                if (i_vis_ith > 0)
//                {
//                    RaySegment2 rs_ith;
//                    rs_ith.fvis = ConvertUIntToFloat4(i_vis_ith);
//                    rs_ith.zdepth = asfloat(deep_k_buf[addr_base + 3 * i + 1]);
//                    uint tc_ith = deep_k_buf[addr_base + 3 * i + 2];
//                    rs_ith.zthick = f16tof32(tc_ith); //  & 0xFFFF
//                    rs_ith.alphaw = (tc_ith >> 16) / 255.f;
//                    if (MergeFragRS_Avr(rs_cur, rs_ith, rs_cur) == 0)
//                    {
//                        __InterlockedExchange(deep_k_buf[addr_base + 3 * i + 0], ConvertFloat4ToUInt(rs_cur.fvis), __dummy);
//                        __InterlockedExchange(deep_k_buf[addr_base + 3 * i + 1], asuint(rs_cur.zdepth), __dummy);
//                        uint tc_cur = (min(uint(rs_cur.alphaw * 255.f), 65535) << 16) | f32tof16(rs_cur.zthick);
//                        __InterlockedExchange(deep_k_buf[addr_base + 3 * i + 2], tc_cur, __dummy);
//                        core_max_zidx = -2;
//                        i = num_deep_layers;
//                    }
//                    else if (core_max_z < rs_ith.zdepth)
//                    {
//                        core_max_z = rs_ith.zdepth;
//                        core_max_zidx = i;
//                    }
//                }
//                else // if (i_vis > 0)
//                    empty_slot = i;
//            }
//            if (core_max_zidx > -2)
//            {
//                if (empty_slot >= 0)  // core-filling case
//                {
//                    __InterlockedExchange(deep_k_buf[addr_base + empty_slot * 3 + 0], ConvertFloat4ToUInt(_v_rgba), __dummy);
//                    __InterlockedExchange(deep_k_buf[addr_base + empty_slot * 3 + 1], asuint(_z_depth), __dummy);
//                    uint tc_cur = (uint(_v_rgba.a * 255) << 16) | f32tof16(_z_thickness);
//                    __InterlockedExchange(deep_k_buf[addr_base + empty_slot * 3 + 2], tc_cur, __dummy);
//                }
//                else // replace the prev max one and then update max z slot
//                {
//                    int addr_prev_max = addr_base + core_max_zidx * 3;
//                            
//                    // load
//                    float4 vis_prev_max = ConvertUIntToFloat4(deep_k_buf[addr_prev_max + 0]);
//                    float zdepth_prev_max = asfloat(deep_k_buf[addr_prev_max + 1]);
//                    uint tc_prev_max = deep_k_buf[addr_prev_max + 2];
//                    float alphaw_prev_max = (tc_prev_max >> 16) / 255.f;
//                    float zthick_prev_max = f16tof32(tc_prev_max);
//                            
//                    // replace
//                    __InterlockedExchange(deep_k_buf[addr_prev_max + 0], ConvertFloat4ToUInt(_v_rgba), __dummy);
//                    __InterlockedExchange(deep_k_buf[addr_prev_max + 1], asuint(_z_depth), __dummy);
//                    uint tc_cur = (uint(_v_rgba.a * 255) << 16) | f32tof16(_z_thickness);
//                    __InterlockedExchange(deep_k_buf[addr_prev_max + 2], tc_cur, __dummy);
//                            
//                    // update the merging slot
//                    __UpdateTailSlot(addr_base, num_deep_layers, vis_prev_max, alphaw_prev_max, zdepth_prev_max, zthick_prev_max);
//                                    
//                    float front_z = min(zdepth_prev_max - zthick_prev_max, tail_z - tail_thickness);
//                    tail_z = zthick_prev_max;
//                    tail_thickness = tail_z - front_z;
//                }
//            }
//        }
//    }
//}

//groupshared int idx_array[num_deep_layers * 2]; // ??
//[numthreads(1, 1, 1)] // or 4x4 thread 도 생각해 보자...
[numthreads(GRIDSIZE, GRIDSIZE, 1)]
//[numthreads(1, 1, 1)]
void OIT_ARRAGNGE(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
    if (DTid.x >= g_cbCamState.rt_width || DTid.y >= g_cbCamState.rt_height)
        return;
    const uint num_deep_layers = g_cbCamState.num_deep_layers;
    uint addr_base = (DTid.y * g_cbCamState.rt_width + DTid.x) * num_deep_layers * 3;
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
        return;
    
    //MergeTmpBufferToKBuffer(addr_base, DTid.xy, num_deep_layers);
    //return;
#define TC_MODE 1
    
    // we will test our oit with the number of deep layers : 4, 8, 16 ... here, set max 32 ( larger/equal than num_deep_layers * 2 )
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
    for (uint ii = 0; ii < num_deep_layers; ii++)
    {
        uint i_vis = deep_k_buf[addr_base + 3 * ii + 0];
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
            zthick_array[valid_frag_cnt] = max(asfloat(deepbuf_zthick[addr_base + 3 * ii + 2]), g_cbCamState.cam_vz_thickness); 
#endif
            valid_frag_cnt++;
        }
    }

	if (valid_frag_cnt == 0) return;
    
    //float4 f4_ = ConvertUIntToFloat4(vis_array[num_deep_layers - 1]);
    //float4 f4_ = ConvertUIntToFloat4(deepbuf_vis[addr_base_vis + num_deep_layers - 1 + 1]);
    //float4 f4__ = ConvertUIntToFloat4(vis_array[0]);
    //fragment_blendout[DTid.xy] = f4__;
    //return;
    
    uint frag_cnt2 = 1 << (uint) (ceil(log2(valid_frag_cnt))); // pow of 2 smaller than num_deep_layers * 2

    [allow_uav_condition][loop]
    for (ii = valid_frag_cnt; ii < num_deep_layers * 2; ii++)
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
        
            if (rs_ith.fvis.a >= FLT_MIN__) // always.. (just for safe-check)
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
    for (i = cnt_sorted_ztsurf; i < num_deep_layers; i++)
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
    [allow_uav_condition][loop]
    for (i = 0; i < num_deep_layers; i++)
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
    }
    //fragment_blendout[DTid.xy] = fmix_vis;
    fragment_blendout[DTid.xy] = blendout;
    fragment_zdepth[DTid.xy] = zdepth_array[idx_array[0]];
}


///////////////////////////////////////////
// GPU-accelerated A-buffer algorithm
// First pass of the prefix sum creation algorithm.  Converts a 2D buffer to a 1D buffer,
// and sums every other value with the previous value.
[numthreads(1, 1, 1)]
void CreatePrefixSum_Pass0_CS(uint3 nGid : SV_GroupID, uint3 nDTid : SV_DispatchThreadID, uint3 nGTid : SV_GroupThreadID)
{
    //if (nDTid.x >= g_cbCamState.rt_width || nDTid.y >= g_cbCamState.rt_height)
    //    return;
    uint nThreadNum = nGid.y * g_cbCamState.rt_width + nGid.x;
    if (nThreadNum % 2 == 0)
    {
        deep_tmp_buf[nThreadNum] = sr_fragment_counter[nGid.xy];
        
        // Add the Fragment count to the next bin
        if ((nThreadNum + 1) < g_cbCamState.rt_width * g_cbCamState.rt_height)
        {
            int2 nextUV;
            nextUV.x = (nThreadNum + 1) % g_cbCamState.rt_width;
            nextUV.y = (nThreadNum + 1) / g_cbCamState.rt_width;
            deep_tmp_buf[nThreadNum + 1] = deep_tmp_buf[nThreadNum] + sr_fragment_counter[nextUV];
        }
    }
}
// Second and following passes.  Each pass distributes the sum of the first half of the group
// to the second half of the group.  There are n/groupsize groups in each pass.
// Each pass increases the group size until it is the size of the buffer.
// The resulting buffer holds the prefix sum of all preceding values in each position .
[numthreads(1, 1, 1)]
void CreatePrefixSum_Pass1_CS(uint3 nGid : SV_GroupID, uint3 nDTid : SV_DispatchThreadID, uint3 nGTid : SV_GroupThreadID)
{
    int nThreadNum = nGid.x + nGid.y * g_cbCamState.rt_width;
    uint g_nPassSize = g_cbCamState.iSrCamDummy__0;
    uint nValue = deep_tmp_buf[nThreadNum * g_nPassSize + g_nPassSize / 2 - 1];
    for (uint i = nThreadNum * g_nPassSize + g_nPassSize / 2; i < nThreadNum * g_nPassSize + g_nPassSize && i < g_cbCamState.rt_width * g_cbCamState.rt_height; i++)
    {
        deep_tmp_buf[i] = deep_tmp_buf[i] + nValue;
    }
}

#define blocksize 1
[numthreads(1, 1, 1)]
void SortAndRenderCS(uint3 nGid : SV_GroupID, uint3 nDTid : SV_DispatchThreadID, uint3 nGTid : SV_GroupThreadID)
{
    uint nThreadNum = nGid.y * g_cbCamState.rt_width + nGid.x; // nDTid
    
//    uint r0, r1, r2;
//    float rd0, rd1, rd2, rd3, rd4, rd5, rd6, rd7;
#define MAX_SORT 300
    int nIndex[MAX_SORT];
    uint N = sr_fragment_counter[nDTid.xy];

	//if (N > 20)
	//{
	//	fragment_blendout[nGid.xy] = float4(1, 0, 0, 1);
	//	return;
	//}
    
    uint N2 = 1 << (int) (ceil(log2(N)));

    float fDepth[MAX_SORT];
    for (uint i = 0; i < N; i++)
    {
        nIndex[i] = i;
        fDepth[i] = asfloat(deep_k_buf[2 * (deep_tmp_buf[nThreadNum - 1] + i) + 1]);
    }
    for (i = N; i < N2; i++)
    {
        nIndex[i] = i;
        fDepth[i] = FLT_MAX;
    }
    
    //if (N > 12)
    //    fragment_blendout[nGid.xy] = float4(1, 0, 0, 1);//ConvertUIntToFloat4(deep_k_buf[2 * (deep_tmp_buf[nThreadNum - 1] + 1) + 0]);
    //return;
    
    uint idx = blocksize * nGTid.y + nGTid.x;

    // Bitonic sort
    for (uint k = 2; k <= N2; k = 2 * k)
    {
        for (uint j = k >> 1; j > 0; j = j >> 1)
        {
            for (uint i = 0; i < N2; i++)
            {
//                GroupMemoryBarrierWithGroupSync();
                //i = idx;

                float di = fDepth[nIndex[i]];
                uint ixj = i ^ j;
                if ((ixj) > i)
                {
                    float dixj = fDepth[nIndex[ixj]];
                    if ((i & k) == 0 && di > dixj)
                    {
                        int temp = nIndex[i];
                        nIndex[i] = nIndex[ixj];
                        nIndex[ixj] = temp;
                    }
                    if ((i & k) != 0 && di < dixj)
                    {
                        int temp = nIndex[i];
                        nIndex[i] = nIndex[ixj];
                        nIndex[ixj] = temp;
                    }
                }
            }
        }
    }

    // Output the final result to the frame buffer
    if (idx == 0)
    {

     /*   
        // Debug
        uint color[8];
        for(int i = 0; i < 8; i++)
        {
            color[i] = deep_k_buf[2 * (deep_tmp_buf[nThreadNum - 1] + i) + 0];
        }

        for(i = 0; i < 8; i++)
        {
            deep_k_buf[2 * (nThreadNum * 8 + i) + 1] = fDepth[i]; //fDepth[nIndex[i]];
            deep_k_buf[2 * (nThreadNum * 8 + i) + 0] = color[nIndex[i]];
        }
     /**/     
   
        // Accumulate fragments into final result
        float4 result = (float4) 0.0f;
        //for (uint x = N - 1; x >= 0; x--)
        //{
        //    uint bufferValue = deep_k_buf[2 * (deep_tmp_buf[nThreadNum - 1] + nIndex[x]) + 0];
        //    float4 color = ConvertUIntToFloat4(bufferValue);
        //    //result = lerp(result, color, color.a);
        //    //result = result * (1 - color.a) + color * color.a;
        //    result = result * (1 - color.a) + color; // because color is an associated one.
        //}
        for (uint x = 0; x < N; x++)
        {
            uint bufferValue = deep_k_buf[2 * (deep_tmp_buf[nThreadNum - 1] + nIndex[x]) + 0];
            float4 color = ConvertUIntToFloat4(bufferValue);
            result += color * (1 - result.a);
        }
        fragment_blendout[nGid.xy] = result; 
		fragment_zdepth[nGid.xy] = asfloat(deep_k_buf[2 * (deep_tmp_buf[nThreadNum - 1] + nIndex[x]) + 1]);
    }
}