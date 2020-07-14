#include "CommonShader.hlsl"

RWBuffer<uint> deepbuf_vis : register(u1); 
RWBuffer<uint> deepbuf_zdepth : register(u2);
RWBuffer<uint> deepbuf_zthick : register(u3);
//RWTexture2D<uint> fragment_blendout : register(u4);
RWTexture2D<float4> fragment_blendout : register(u4);
RWTexture2D<float> fragment_zdepth : register(u5);

Texture2D<uint> sr_fragment_counter : register(t0); // RWTexture2D 나중에 Texture2D and t0 으로 수정

//groupshared int idx_array[num_deep_layers * 2]; // ??
//[numthreads(1, 1, 1)] // or 4x4 thread 도 생각해 보자...
[numthreads(GRIDSIZE, GRIDSIZE, 1)]
//[numthreads(1, 1, 1)]
void OIT_ARRAGNGE(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
    if (DTid.x >= g_cbCamState.rt_width || DTid.y >= g_cbCamState.rt_height)
        return;
    const uint num_deep_layers = g_cbCamState.num_deep_layers;
    uint addr_base = (DTid.y * g_cbCamState.rt_width + DTid.x) * num_deep_layers;
    uint addr_base_vis = (DTid.y * g_cbCamState.rt_width + DTid.x) * (num_deep_layers + 4);
    uint frag_cnt = sr_fragment_counter[DTid.xy];
    if (frag_cnt == 77777 || num_deep_layers < 1)
    {
        fragment_blendout[DTid.xy] = float4(1, 0, 0, 1);
        return;
    }
    
    if (frag_cnt == 0)
        return;
    
#define ALPHA_SUM_MODE 1
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
        uint i_vis = 0;
        if (ii < num_deep_layers - 1)
            i_vis = deepbuf_vis[addr_base_vis + ii];
        else
        {
#if ALPHA_SUM_MODE == 1
            //float4 fvis = ConvertUIntToFloat4(deepbuf_vis[addr_base_vis + num_deep_layers - 1 + 1]);
            i_vis = deepbuf_vis[addr_base_vis + num_deep_layers - 1 + 1]; //ConvertFloat4ToUInt(fvis);
            //float4 fvis = ConvertUIntToFloat4(deepbuf_vis[addr_base_vis + num_deep_layers - 1 + 1]);
            //i_vis = ConvertFloat4ToUInt(fvis);
#else
            float a_sum = asfloat(deepbuf_vis[addr_base_vis + num_deep_layers - 1 + 0]);
            float3 a_mult_rgb;
            a_mult_rgb.r = asfloat(deepbuf_vis[addr_base_vis + num_deep_layers - 1 + 1]) / a_sum;
            a_mult_rgb.g = asfloat(deepbuf_vis[addr_base_vis + num_deep_layers - 1 + 2]) / a_sum;
            a_mult_rgb.b = asfloat(deepbuf_vis[addr_base_vis + num_deep_layers - 1 + 3]) / a_sum;
            float mix_a = 1.f - asfloat(deepbuf_vis[addr_base_vis + num_deep_layers - 1 + 4]);
            float4 fvis = float4(a_mult_rgb * mix_a, mix_a);
            i_vis = ConvertFloat4ToUInt(fvis);
#endif
        }
        if (i_vis > 0)
        {
            idx_array[valid_frag_cnt] = valid_frag_cnt;
            vis_array[valid_frag_cnt] = i_vis;
            zdepth_array[valid_frag_cnt] = asfloat(deepbuf_zdepth[addr_base + ii]);
#if TC_MODE == 1
            uint tc = deepbuf_zthick[addr_base + ii];
            alphaw_array[valid_frag_cnt] = (tc >> 16) / 255.f;
            zthick_array[valid_frag_cnt] = 1;//f16tof32(tc & 0xFFFF);
#else
            zthick_array[valid_frag_cnt] = 1;//asfloat(deepbuf_zthick[addr_base + ii]);
#endif
            valid_frag_cnt++;
        }
    }
    
    //float4 f4_ = ConvertUIntToFloat4(vis_array[num_deep_layers - 1]);
    //float4 f4_ = ConvertUIntToFloat4(deepbuf_vis[addr_base_vis + num_deep_layers - 1 + 1]);
    //float4 f4_ = ConvertUIntToFloat4(vis_array[0]);
    //fragment_blendout[DTid.xy] = f4_;
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
    
    // merge self-overlapping surfaces to thickness surfaces
    float4 fmix_vis = (float4) 0;
    uint cnt_sorted_ztsurf = 0;
    [allow_uav_condition][loop]
    for (uint i = 0; i < valid_frag_cnt; i++)
    {
        uint idx_ith = idx_array[i];
        RST rs_ith;
        rs_ith.zthick = zthick_array[idx_ith];

        if (rs_ith.zthick >= FLT_MIN__)
        {
            rs_ith.zdepth = zdepth_array[idx_ith];
            rs_ith.fvis = ConvertUIntToFloat4(vis_array[idx_ith]);
#if TC_MODE == 1
            rs_ith.alphaw = alphaw_array[idx_ith];
#endif
        
            [allow_uav_condition][loop]
            for (uint j = i + 1; j < valid_frag_cnt + 1; j++)
            {
                int idx_next = idx_array[j];
                RST rs_next;
                rs_next.zthick = zthick_array[idx_next];
                if (rs_next.zthick >= FLT_MIN__)
                {
                    rs_next.zdepth = zdepth_array[idx_next];
                    rs_next.fvis = ConvertUIntToFloat4(vis_array[idx_next]);
#if TC_MODE == 1
                    rs_next.alphaw = alphaw_array[idx_next];
                    MergeRS_OUT2 rs_out = MergeRS2(rs_ith, rs_next);
#else
                    MergeRS_OUT rs_out = MergeRS(rs_ith, rs_next);
#endif
                    rs_ith = rs_out.rs_prior;
                    vis_array[idx_next] = ConvertFloat4ToUInt(rs_out.rs_posterior.fvis);
                    zdepth_array[idx_next] = rs_out.rs_posterior.zdepth;
                    zthick_array[idx_next] = rs_out.rs_posterior.zthick;
                }
            }
        
            if (rs_ith.zthick >= FLT_MIN__) // always.. (just for safe-check)
            {
                int idx_sp = idx_array[cnt_sorted_ztsurf];
                vis_array[idx_sp] = ConvertFloat4ToUInt(rs_ith.fvis);
                zdepth_array[idx_sp] = rs_ith.zdepth;
                zthick_array[idx_sp] = rs_ith.zthick;
                cnt_sorted_ztsurf++;
            
                fmix_vis += rs_ith.fvis * (1.f - fmix_vis.a);
                //fmix_vis += ConvertUIntToFloat4(vis_array[idx_sp]) * (1.f - fmix_vis.a);
                if (fmix_vis.a > ERT_ALPHA)
                    i = valid_frag_cnt;
            }
            // 여기에 deep buffer 값 넣기... ==> massive loop contents 는 피하는 것이 좋으므로 아래로 뺀다.
        } // if (rs_ith.zthick >= FLT_MIN__)
    }
    
    //float4 f4_ = ConvertUIntToFloat4(vis_array[idx_array[num_deep_layers - 1]]);
    //fragment_blendout[DTid.xy] = f4_;
    //return;
    
    [loop]
    for (i = cnt_sorted_ztsurf; i < num_deep_layers; i++)
    {
        int idx = idx_array[i];
        vis_array[idx] = 0;
        zdepth_array[idx] = FLT_MAX;
        zthick_array[idx] = 0;
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
        deepbuf_vis[addr_base_vis + i] = ivis;
        deepbuf_zdepth[addr_base + i] = asuint(zdepth_array[idx]);
        deepbuf_zthick[addr_base + i] = asuint(zthick_array[idx]);
    }
    //fragment_blendout[DTid.xy] = fmix_vis;
    fragment_blendout[DTid.xy] = blendout;
    fragment_zdepth[DTid.xy] = zdepth_array[idx_array[0]];
}