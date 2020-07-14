#include "Sr_Common.hlsl"

// this version considers the thickness is constant
void FillKDepth_v1(const in int2 tex2d_xy, const in int num_deep_layers, const in float4 v_rgba, const in float z_depth, const in float z_thickness)
{
    if (z_depth > 10000000 || v_rgba.a == 0)
        return;

    uint frag_cnt = fragment_counter[tex2d_xy.xy]; // asynch 가 되는 경우 queue heap 에 저장이 안 될 수 있는데, 이 정도는 허용 가능 동작
    if (frag_cnt == 77777)
        return;
    
    const float ferr_sq = 0.0001f * 0.0001f; // to do : dynamic variable z_thickness
    uint iv_rgba = ConvertFloat4ToUInt(v_rgba);
    int addr_base = (tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x) * num_deep_layers * 3;

    uint __dummy;
     
    // atomic routine (we are using spin-lock scheme)
    uint safe_unlock_count = 0;
    bool keep_loop = true;
    //break; ==> do not use this when using allow_uav_condition *** very important!
    [allow_uav_condition][loop]
    while (keep_loop)
    {
        if (++safe_unlock_count > 10000 && frag_cnt >= (uint) num_deep_layers)//g_cbPobj.num_safe_loopexit)
        {
            __InterlockedExchange(fragment_counter[tex2d_xy.xy], 77777, __dummy);
            //InterlockedCompareExchange(fragment_counter[tex2d_xy.xy], 1, 77777, __dummy);
            keep_loop = false;
            //break;
        }
        else
        {
            if (frag_cnt < (uint) num_deep_layers) // deprecated //
                InterlockedAdd(fragment_counter[tex2d_xy.xy], 1, frag_cnt);
            
            uint spin_lock = 0;
            // note that all of fragment_spinlock are initialized as zero
            //InterlockedExchange(fragment_spinlock[tex2d_xy.xy], 1, spin_lock);
            InterlockedCompareExchange(fragment_spinlock[tex2d_xy.xy], 0, 1, spin_lock);
            if (spin_lock == 0)
            {
                int addr_merge_slot = addr_base + 3 * (num_deep_layers - 1);
                float tail_z = asfloat(deep_k_buf[addr_merge_slot + 1]);
                if (tail_z == 0)
                    tail_z = FLT_MAX;
                uint tc = deep_k_buf[addr_merge_slot + 2];
                float tail_thickness = f16tof32(tc); //  & 0xFFFF
                
                if (z_depth > tail_z - tail_thickness)
                {
                    // update the merging slot
                    UpdateTailSlot(addr_base, num_deep_layers, v_rgba, v_rgba.a, z_depth, z_thickness);
                }
                else
                {
                    RaySegment2 rs_cur;
                    rs_cur.zdepth = z_depth;
                    rs_cur.fvis = v_rgba;
                    rs_cur.zthick = z_thickness;
                    rs_cur.alphaw = v_rgba.a;
                    int empty_slot = -1;
                    int core_max_zidx = -1;
                    float core_max_z = -1.f;
                    [allow_uav_condition][loop]
                    for (int i = 0; i < num_deep_layers - 1; i++)
                    {
                        // after atomic store operation
                        uint i_vis_ith = deep_k_buf[addr_base + 3 * i + 0];
                        if (i_vis_ith > 0)
                        {
                            RaySegment2 rs_ith;
                            rs_ith.fvis = ConvertUIntToFloat4(i_vis_ith);
                            rs_ith.zdepth = asfloat(deep_k_buf[addr_base + 3 * i + 1]);
                            uint tc_ith = deep_k_buf[addr_base + 3 * i + 2];
                            rs_ith.zthick = f16tof32(tc_ith); //  & 0xFFFF
                            rs_ith.alphaw = (tc_ith >> 16) / 255.f;
                            if (MergeFragRS_Avr(rs_cur, rs_ith, rs_cur) == 0)
                            {
                                __InterlockedExchange(deep_k_buf[addr_base + 3 * i + 0], ConvertFloat4ToUInt(rs_cur.fvis), __dummy);
                                __InterlockedExchange(deep_k_buf[addr_base + 3 * i + 1], asuint(rs_cur.zdepth), __dummy);
                                uint tc_cur = (min(uint(rs_cur.alphaw * 255.f), 65535) << 16) | f32tof16(rs_cur.zthick);
                                __InterlockedExchange(deep_k_buf[addr_base + 3 * i + 2], tc_cur, __dummy);
                                core_max_zidx = -2;
                                i = num_deep_layers;
                                // spinlock finish routine
                            }
                            else if (core_max_z < rs_ith.zdepth)
                            {
                                core_max_z = rs_ith.zdepth;
                                core_max_zidx = i;
                            }
                        }
                        else // if (i_vis > 0)
                            empty_slot = i;
                    }
                    if (core_max_zidx > -2)
                    {
                        if (empty_slot >= 0)  // core-filling case
                        {
                            __InterlockedExchange(deep_k_buf[addr_base + empty_slot * 3 + 0], iv_rgba, __dummy);
                            __InterlockedExchange(deep_k_buf[addr_base + empty_slot * 3 + 1], asuint(z_depth), __dummy);
                            uint tc_cur = (uint(v_rgba.a * 255) << 16) | f32tof16(z_thickness);
                            __InterlockedExchange(deep_k_buf[addr_base + empty_slot * 3 + 2], tc_cur, __dummy);
                        }
                        else // replace the prev max one and then update max z slot
                        {
                            int addr_prev_max = addr_base + core_max_zidx * 3;
                            
                            // load
                            float4 vis_prev_max = ConvertUIntToFloat4(deep_k_buf[addr_prev_max + 0]);
                            float zdepth_prev_max = asfloat(deep_k_buf[addr_prev_max + 1]);
                            uint tc_prev_max = deep_k_buf[addr_prev_max + 2];
                            float alphaw_prev_max = (tc_prev_max >> 16) / 255.f;
                            float zthick_prev_max = f16tof32(tc_prev_max);
                            
                            // replace
                            __InterlockedExchange(deep_k_buf[addr_prev_max + 0], iv_rgba, __dummy);
                            __InterlockedExchange(deep_k_buf[addr_prev_max + 1], asuint(z_depth), __dummy);
                            uint tc_cur = (uint(v_rgba.a * 255) << 16) | f32tof16(z_thickness);
                            __InterlockedExchange(deep_k_buf[addr_prev_max + 2], tc_cur, __dummy);
                            
                            // update the merging slot
                            UpdateTailSlot(addr_base, num_deep_layers, vis_prev_max, alphaw_prev_max, zdepth_prev_max, zthick_prev_max);
                        }
                    }
                }
                
                // always fragment_spinlock[tex2d_xy.xy] is 1, thus use InterlockedExchange instead of InterlockedCompareExchange
                InterlockedExchange(fragment_spinlock[tex2d_xy.xy], 0, spin_lock);
                //InterlockedCompareExchange(fragment_spinlock[tex2d_xy.xy], 1, 0, spin_lock);
                keep_loop = false;
                //break;
            } // critical section
        }
    } // while for spin-lock
}

// 그냥 국내 논문용? 너무 복잡??..
void FillKDepth_v2(const in int2 tex2d_xy, const in int num_deep_layers, const in float4 v_rgba, const in float z_depth, const in float z_thickness)
{
    if (z_depth > 10000000 || v_rgba.a == 0)
        return;
    
    const float ferr_sq = 0.0001f * 0.0001f; // to do : dynamic variable z_thickness
    uint iv_rgba = ConvertFloat4ToUInt(v_rgba);
    int addr_base = (tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x) * num_deep_layers * 3;
    
    uint __safe_unlock_count = 0;
    uint __dummy;
    uint frag_cnt;
    InterlockedAdd(fragment_counter[tex2d_xy.xy], 1, frag_cnt);
    if (frag_cnt == 77777)
        return;
    uint period = frag_cnt / (num_deep_layers + 1);
    do
    {
        if (++__safe_unlock_count > 100)
        {
            __InterlockedExchange(fragment_counter[tex2d_xy.xy], 77777, __dummy);
            return;
        }
        // safe check?!
    } while ((fragment_tmp_w_counter[tex2d_xy.xy] / (num_deep_layers + 1)) < period);
    int mod_cnt = frag_cnt % (num_deep_layers + 1);
    
    if (mod_cnt < num_deep_layers)
    {
        InterlockedExchange(deep_tmp_buf[addr_base + mod_cnt * 3 + 0], iv_rgba, __dummy);
        InterlockedExchange(deep_tmp_buf[addr_base + mod_cnt * 3 + 1], asuint(z_depth), __dummy);
        uint tc_cur = (uint(v_rgba.a * 255) << 16) | f32tof16(z_thickness);
        InterlockedExchange(deep_tmp_buf[addr_base + mod_cnt * 3 + 2], tc_cur, __dummy);
        InterlockedAdd(fragment_tmp_w_counter[tex2d_xy.xy], 1, __dummy);
    }
    __safe_unlock_count = 0;
    if (mod_cnt == num_deep_layers)
    {
        do
        {
            if (++__safe_unlock_count > 100000)
            {
                __InterlockedExchange(fragment_counter[tex2d_xy.xy], 99999, __dummy);
                return;
            }
        } while ((fragment_tmp_w_counter[tex2d_xy.xy] % ((uint) num_deep_layers + 1)) < (uint) num_deep_layers);
        
#define MAX_LAYERS_TMP 9
        uint vis_array[MAX_LAYERS_TMP];
        float zdepth_array[MAX_LAYERS_TMP];
        float zthick_array[MAX_LAYERS_TMP];
        float alphaw_array[MAX_LAYERS_TMP];
        [allow_uav_condition][loop]
        for (int i = 0; i < num_deep_layers; i++)
        {
            vis_array[i] = deep_tmp_buf[addr_base + i * 3 + 0];
            zdepth_array[i] = deep_tmp_buf[addr_base + i * 3 + 1];
            uint tc = deep_tmp_buf[addr_base + i * 3 + 2];
            zthick_array[i] = f16tof32(tc);
            alphaw_array[i] = (tc >> 16) / 255.f;
        }
        InterlockedAdd(fragment_tmp_w_counter[tex2d_xy.xy], 1, __dummy);
        vis_array[num_deep_layers] = iv_rgba;
        zdepth_array[num_deep_layers] = z_depth;
        zthick_array[num_deep_layers] = z_thickness;
        alphaw_array[num_deep_layers] = v_rgba.a;
        
        uint safe_unlock_count = 0;
        bool keep_loop = true;
        // atomic routine (we are using spin-lock scheme)
        //break; ==> do not use this when using allow_uav_condition *** very important!
        [allow_uav_condition][loop]
        while (keep_loop)
        {
            if (++safe_unlock_count > 100000)//g_cbPobj.num_safe_loopexit)
            {
                __InterlockedExchange(fragment_counter[tex2d_xy.xy], 55555, __dummy);
                keep_loop = false;
            }
            else
            {
                uint spin_lock = 0;
                // note that all of fragment_spinlock are initialized as zero
                //InterlockedExchange(fragment_spinlock[tex2d_xy.xy], 1, spin_lock);
                InterlockedCompareExchange(fragment_spinlock[tex2d_xy.xy], 0, 1, spin_lock);
                if (spin_lock == 0)
                {
                    int addr_merge_slot = addr_base + 3 * (num_deep_layers - 1);
                    float tail_z = asfloat(deep_k_buf[addr_merge_slot + 1]);
                    if (tail_z == 0)
                        tail_z = FLT_MAX;
                    uint tc = deep_k_buf[addr_merge_slot + 2];
                    float tail_thickness = f16tof32(tc); //  & 0xFFFF
                    
                    [allow_uav_condition][loop]
                    for (int ii = 0; ii <= num_deep_layers; ii++) // '<=' is for MAX_LAYERS_TMP
                    {
                        uint _i_rgba = vis_array[ii];
                        if (_i_rgba == 0)
                            continue;
                        float4 _v_rgba = ConvertUIntToFloat4(_i_rgba);
                        float _z_depth = zdepth_array[ii];
                        float _z_thickness = zthick_array[ii];
                        float _alphaw = alphaw_array[ii];
                        if (_z_depth > tail_z - tail_thickness)
                        {
                            // update the merging slot
                            UpdateTailSlot(addr_base, num_deep_layers, _v_rgba, _v_rgba.a, _z_depth, _z_thickness);
                            float front_z = min(_z_depth - _z_thickness, tail_z - tail_thickness);
                            tail_z = _z_depth;
                            tail_thickness = tail_z - front_z;
                        }
                        else
                        {
                            RaySegment2 rs_cur;
                            rs_cur.zdepth = _z_depth;
                            rs_cur.fvis = _v_rgba;
                            rs_cur.zthick = _z_thickness;
                            rs_cur.alphaw = _v_rgba.a;
                            int empty_slot = -1;
                            int core_max_zidx = -1;
                            float core_max_z = -1.f;
                            [allow_uav_condition][loop]
                            for (int i = 0; i < num_deep_layers - 1; i++)
                            {
                                // after atomic store operation
                                uint i_vis_ith = deep_k_buf[addr_base + 3 * i + 0];
                                if (i_vis_ith > 0)
                                {
                                    RaySegment2 rs_ith;
                                    rs_ith.fvis = ConvertUIntToFloat4(i_vis_ith);
                                    rs_ith.zdepth = asfloat(deep_k_buf[addr_base + 3 * i + 1]);
                                    uint tc_ith = deep_k_buf[addr_base + 3 * i + 2];
                                    rs_ith.zthick = f16tof32(tc_ith); //  & 0xFFFF
                                    rs_ith.alphaw = (tc_ith >> 16) / 255.f;
                                    if (MergeFragRS_Avr(rs_cur, rs_ith, rs_cur) == 0)
                                    {
                                        __InterlockedExchange(deep_k_buf[addr_base + 3 * i + 0], ConvertFloat4ToUInt(rs_cur.fvis), __dummy);
                                        __InterlockedExchange(deep_k_buf[addr_base + 3 * i + 1], asuint(rs_cur.zdepth), __dummy);
                                        uint tc_cur = (min(uint(rs_cur.alphaw * 255.f), 65535) << 16) | f32tof16(rs_cur.zthick);
                                        __InterlockedExchange(deep_k_buf[addr_base + 3 * i + 2], tc_cur, __dummy);
                                        core_max_zidx = -2;
                                        i = num_deep_layers;
                                    }
                                    else if (core_max_z < rs_ith.zdepth)
                                    {
                                        core_max_z = rs_ith.zdepth;
                                        core_max_zidx = i;
                                    }
                                }
                                else // if (i_vis > 0)
                                    empty_slot = i;
                            }
                            if (core_max_zidx > -2)
                            {
                                if (empty_slot >= 0)  // core-filling case
                                {
                                    __InterlockedExchange(deep_k_buf[addr_base + empty_slot * 3 + 0], ConvertFloat4ToUInt(_v_rgba), __dummy);
                                    __InterlockedExchange(deep_k_buf[addr_base + empty_slot * 3 + 1], asuint(_z_depth), __dummy);
                                    uint tc_cur = (uint(_v_rgba.a * 255) << 16) | f32tof16(_z_thickness);
                                    __InterlockedExchange(deep_k_buf[addr_base + empty_slot * 3 + 2], tc_cur, __dummy);
                                }
                                else // replace the prev max one and then update max z slot
                                {
                                    int addr_prev_max = addr_base + core_max_zidx * 3;
                            
                                    // load
                                    float4 vis_prev_max = ConvertUIntToFloat4(deep_k_buf[addr_prev_max + 0]);
                                    float zdepth_prev_max = asfloat(deep_k_buf[addr_prev_max + 1]);
                                    uint tc_prev_max = deep_k_buf[addr_prev_max + 2];
                                    float alphaw_prev_max = (tc_prev_max >> 16) / 255.f;
                                    float zthick_prev_max = f16tof32(tc_prev_max);
                            
                                    // replace
                                    __InterlockedExchange(deep_k_buf[addr_prev_max + 0], ConvertFloat4ToUInt(_v_rgba), __dummy);
                                    __InterlockedExchange(deep_k_buf[addr_prev_max + 1], asuint(_z_depth), __dummy);
                                    uint tc_cur = (uint(_v_rgba.a * 255) << 16) | f32tof16(_z_thickness);
                                    __InterlockedExchange(deep_k_buf[addr_prev_max + 2], tc_cur, __dummy);
                            
                                    // update the merging slot
                                    UpdateTailSlot(addr_base, num_deep_layers, vis_prev_max, alphaw_prev_max, zdepth_prev_max, zthick_prev_max);
                                    
                                    float front_z = min(zdepth_prev_max - zthick_prev_max, tail_z - tail_thickness);
                                    tail_z = zthick_prev_max;
                                    tail_thickness = tail_z - front_z;
                                }
                            }
                        }
                    }
                
                    // always fragment_spinlock[tex2d_xy.xy] is 1, thus use InterlockedExchange instead of InterlockedCompareExchange
                    InterlockedExchange(fragment_spinlock[tex2d_xy.xy], 0, spin_lock);
                    //InterlockedCompareExchange(fragment_spinlock[tex2d_xy.xy], 1, 0, spin_lock);
                    keep_loop = false;
                    
                    //__InterlockedExchange(fragment_counter[tex2d_xy.xy], 88888, __dummy);
                }
            }
        }
    }
}

void FillKDepth_NoInterlock(const in int2 tex2d_xy, const in int num_deep_layers, const in float4 v_rgba, const in float z_depth, const in float z_thickness)
{
    if (z_depth > 10000000 || v_rgba.a == 0)
        return;

    uint frag_cnt = fragment_counter[tex2d_xy.xy]; // asynch 가 되는 경우 queue heap 에 저장이 안 될 수 있는데, 이 정도는 허용 가능 동작
    
    const float ferr_sq = 0.0001f * 0.0001f; // to do : dynamic variable z_thickness
    uint iv_rgba = ConvertFloat4ToUInt(v_rgba);
    int addr_base = (tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x) * num_deep_layers * 3;

    uint __dummy;
    if (frag_cnt < (uint) num_deep_layers) // deprecated //
        InterlockedAdd(fragment_counter[tex2d_xy.xy], 1, frag_cnt);
            
    int addr_merge_slot = addr_base + 3 * (num_deep_layers - 1);
    float tail_z = asfloat(deep_k_buf[addr_merge_slot + 1]);
    if (tail_z == 0)
        tail_z = FLT_MAX;
    uint tc = deep_k_buf[addr_merge_slot + 2];
    float tail_thickness = f16tof32(tc); //  & 0xFFFF
                
    if (z_depth > tail_z - tail_thickness)
    {
        // update the merging slot
        UpdateTailSlot(addr_base, num_deep_layers, v_rgba, v_rgba.a, z_depth, z_thickness);
    }
    else
    {
        RaySegment2 rs_cur;
        rs_cur.zdepth = z_depth;
        rs_cur.fvis = v_rgba;
        rs_cur.zthick = z_thickness;
        rs_cur.alphaw = v_rgba.a;
        int empty_slot = -1;
        int core_max_zidx = -1;
        float core_max_z = -1.f;
        [allow_uav_condition][loop]
        for (int i = 0; i < num_deep_layers - 1; i++)
        {
            // after atomic store operation
            uint i_vis_ith = deep_k_buf[addr_base + 3 * i + 0];
            if (i_vis_ith > 0)
            {
                RaySegment2 rs_ith;
                rs_ith.fvis = ConvertUIntToFloat4(i_vis_ith);
                rs_ith.zdepth = asfloat(deep_k_buf[addr_base + 3 * i + 1]);
                uint tc_ith = deep_k_buf[addr_base + 3 * i + 2];
                rs_ith.zthick = f16tof32(tc_ith); //  & 0xFFFF
                rs_ith.alphaw = (tc_ith >> 16) / 255.f;
                if (MergeFragRS_Avr(rs_cur, rs_ith, rs_cur) == 0)
                {
                    __InterlockedExchange(deep_k_buf[addr_base + 3 * i + 0], ConvertFloat4ToUInt(rs_cur.fvis), __dummy);
                    __InterlockedExchange(deep_k_buf[addr_base + 3 * i + 1], asuint(rs_cur.zdepth), __dummy);
                    uint tc_cur = (min(uint(rs_cur.alphaw * 255.f), 65535) << 16) | f32tof16(rs_cur.zthick);
                    __InterlockedExchange(deep_k_buf[addr_base + 3 * i + 2], tc_cur, __dummy);
                    core_max_zidx = -2;
                    i = num_deep_layers;
                    // spinlock finish routine
                }
                else if (core_max_z < rs_ith.zdepth)
                {
                    core_max_z = rs_ith.zdepth;
                    core_max_zidx = i;
                }
            }
            else // if (i_vis > 0)
                empty_slot = i;
        }
        if (core_max_zidx > -2)
        {
            if (empty_slot >= 0)  // core-filling case
            {
                __InterlockedExchange(deep_k_buf[addr_base + empty_slot * 3 + 0], iv_rgba, __dummy);
                __InterlockedExchange(deep_k_buf[addr_base + empty_slot * 3 + 1], asuint(z_depth), __dummy);
                uint tc_cur = (uint(v_rgba.a * 255) << 16) | f32tof16(z_thickness);
                __InterlockedExchange(deep_k_buf[addr_base + empty_slot * 3 + 2], tc_cur, __dummy);
            }
            else // replace the prev max one and then update max z slot
            {
                int addr_prev_max = addr_base + core_max_zidx * 3;
                            
                // load
                float4 vis_prev_max = ConvertUIntToFloat4(deep_k_buf[addr_prev_max + 0]);
                float zdepth_prev_max = asfloat(deep_k_buf[addr_prev_max + 1]);
                uint tc_prev_max = deep_k_buf[addr_prev_max + 2];
                float alphaw_prev_max = (tc_prev_max >> 16) / 255.f;
                float zthick_prev_max = f16tof32(tc_prev_max);
                            
                // replace
                __InterlockedExchange(deep_k_buf[addr_prev_max + 0], iv_rgba, __dummy);
                __InterlockedExchange(deep_k_buf[addr_prev_max + 1], asuint(z_depth), __dummy);
                uint tc_cur = (uint(v_rgba.a * 255) << 16) | f32tof16(z_thickness);
                __InterlockedExchange(deep_k_buf[addr_prev_max + 2], tc_cur, __dummy);
                            
                // update the merging slot
                UpdateTailSlot(addr_base, num_deep_layers, vis_prev_max, alphaw_prev_max, zdepth_prev_max, zthick_prev_max);
            }
        }
    }
}

// SINGLE_LAYER 로 그려진 것을 읽고, outline 그리는 함수
[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void OIT_PRESET(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
    int2 tex2d_xy = int2(DTid.xy);
    
    //float4 v_rgba = sr_fragment_vis[tex2d_xy];
    float depthcs = sr_fragment_zdepth[tex2d_xy];
    
    float4 v_rgba = OutlineTest(tex2d_xy, depthcs);
    if (v_rgba.a == 0)
        return;
    
    // float vz_thickness = g_cbPobj.vz_thickness;
    FillKDepth_NoInterlock(tex2d_xy, g_cbCamState.num_deep_layers, v_rgba, depthcs, g_cbCamState.cam_vz_thickness);
}

void ComputeColor(inout float3 color_out, const in float3 Ka, const in float3 Kd, const in float3 Ks, float Ns, const in float3 pos_frag, const in float3 view_dir, const in float3 nrl, const in float nrl_len)
{
    if (nrl_len > 0)
    {
        float3 light_dirinv = -g_cbCamState.dir_light_ws;
        if (g_cbCamState.cam_flag & 0x2)
            light_dirinv = -normalize(pos_frag - g_cbCamState.pos_light_ws);
        color_out = PhongBlinn(g_cbRndEffect.pb_shading_factor, view_dir, light_dirinv, nrl / nrl_len, g_cbPobj.pobj_flag & (0x1 << 5), Ka, Kd, Ks, Ns);
    }
    else if ((g_cbPobj.pobj_flag & (0x1 << 3)) == 0)
    {
        color_out = Ka;
    }
}

#if __RENDERING_MODE == 2
void OIT_KDEPTH(VS_OUTPUT_TTT input)
#else
void OIT_KDEPTH(VS_OUTPUT input)
#endif
{
    POBJ_PRE_CONTEXT(return)
    
    float4 v_rgba = float4(g_cbPobj.Kd, g_cbPobj.alpha);
    float vz_thickness = g_cbPobj.vz_thickness;
    
    float3 nor = (float3)0;
    float nor_len = 0;
    
#if __RENDERING_MODE != 1 && __RENDERING_MODE != 2 && __RENDERING_MODE != 3
    nor = input.f3VecNormalWS;
    nor_len = length(nor);
#endif
    
#if __RENDERING_MODE == 1
    DashedLine(v_rgba, z_depth, input.f3Custom.x, g_cbPobj.dash_interval, g_cbPobj.pobj_flag & (0x1 << 19), g_cbPobj.pobj_flag & (0x1 << 20));
#elif __RENDERING_MODE == 2
    MultiTextMapping(v_rgba, z_depth, input.f3Custom0.xy, (int)(input.f3Custom0.z + 0.5f), input.f3Custom1, input.f3Custom2);
#elif __RENDERING_MODE == 3
    TextMapping(v_rgba, z_depth, input.f3Custom.xy, g_cbPobj.pobj_flag & (0x1 << 9), g_cbPobj.pobj_flag & (0x1 << 10));
#elif __RENDERING_MODE == 4
    TextureImgMap(v_rgba, z_depth, input.f3Custom);
    float3 Ka = v_rgba.rgb, Kd = v_rgba.rgb, Ks = g_cbPobj.Ks;
    ComputeColor(v_rgba.rgb, Ka, Kd, Ks, 0, input.f3PosWS, view_dir, nor, nor_len);
#else
    float3 Ka = g_cbPobj.Ka, Kd = g_cbPobj.Kd, Ks = g_cbPobj.Ks;
    float Ns = g_cbPobj.Ns;
    if ((g_cbPobj.pobj_flag & (0x1 << 3)) == 0)
        Ka = Kd = Ks = input.f3Custom;
    ComputeColor(v_rgba.rgb, Ka, Kd, Ks, Ns, input.f3PosWS, view_dir, nor, nor_len);
#endif
    
    // make it as an associated color.
    // as a color component is stored into 8 bit channel, the alpha-multiplied precision must be determined in this stage.
    // unless, noise dots appear.
    v_rgba.rgb *= v_rgba.a; 

    // add opacity culling with biased z depth
    // to do : SS-based LAO 에서는 z-culling 안 되도록.
    //if (v_rgba.a > 0.99f)
    //    out_ps.ds_z = input.f4PosSS.z + 0.00f; // to do : bias z
    // FillKDepth_NoInterlock, FillKDepth
    
    // to do with dynamic determination of vz_thickness
#if __RENDERING_MODE != 2
    //if (nor_len > 0)
    //{
    //    float3 u_nor = nor / nor_len;
    //    float rad = abs(dot(u_nor, -view_dir));
    //    float in_angle = min(acos(rad), F_PI / 3.f); // safe value
    //    float cos_v = cos(in_angle);
    //    vz_thickness /= cos_v;
    //}
#endif
    
#define __TEST__
#ifdef __TEST__
    // test //
    //z_depth -= vz_thickness;
    //vz_thickness = 0.00001;
#endif
    
    int2 tex2d_xy = int2(input.f4PosSS.xy);    
    FillKDepth_v1(tex2d_xy, g_cbCamState.num_deep_layers, v_rgba, z_depth, vz_thickness);
}

#if __RENDERING_MODE == 2
void OIT_A_BUFFER_CNF_FRAGS(VS_OUTPUT_TTT input)
#else
void OIT_A_BUFFER_CNF_FRAGS(VS_OUTPUT input)
#endif
{
    if (g_cbClipInfo.clip_flag & 0x1)
    {
        if (dot(g_cbClipInfo.vec_clipplane, input.f3PosWS - g_cbClipInfo.pos_clipplane) > 0)
            return;
    }
    if (g_cbClipInfo.clip_flag & 0x2)
    {
        if (!IsInsideClipBox(input.f3PosWS, g_cbClipInfo.pos_clipbox_max_bs, g_cbClipInfo.mat_clipbox_ws2bs))
            return;
    }
    float3 pos_ip_ss = float3(input.f4PosSS.xy, 0.0f);
    float3 pos_ip_ws = TransformPoint(pos_ip_ss, g_cbCamState.mat_ss2ws);
    float3 vec_pos_ip2frag = input.f3PosWS - pos_ip_ws;
    if (dot(vec_pos_ip2frag, g_cbCamState.dir_view_ws) < 0)\
        return;
    
    int2 tex2d_xy = int2(input.f4PosSS.xy);
    InterlockedAdd(fragment_counter[tex2d_xy.xy], 1);
}

#if __RENDERING_MODE == 2
void OIT_A_BUFFER_FILL(VS_OUTPUT_TTT input)
#else
void OIT_A_BUFFER_FILL(VS_OUTPUT input)
#endif
{
    POBJ_PRE_CONTEXT(return)
    
    float4 v_rgba = float4(g_cbPobj.Kd, g_cbPobj.alpha);
    float vz_thickness = g_cbPobj.vz_thickness;
    
    float3 nor = (float3) 0;
    float nor_len = 0;
    
#if __RENDERING_MODE != 1 && __RENDERING_MODE != 2 && __RENDERING_MODE != 3
    nor = input.f3VecNormalWS;
    nor_len = length(nor);
#endif
    
#if __RENDERING_MODE == 1
    DashedLine(v_rgba, z_depth, input.f3Custom.x, g_cbPobj.dash_interval, g_cbPobj.pobj_flag & (0x1 << 19), g_cbPobj.pobj_flag & (0x1 << 20));
#elif __RENDERING_MODE == 2
    MultiTextMapping(v_rgba, z_depth, input.f3Custom0.xy, (int)(input.f3Custom0.z + 0.5f), input.f3Custom1, input.f3Custom2);
#elif __RENDERING_MODE == 3
    TextMapping(v_rgba, z_depth, input.f3Custom.xy, g_cbPobj.pobj_flag & (0x1 << 9), g_cbPobj.pobj_flag & (0x1 << 10));
#elif __RENDERING_MODE == 4
    TextureImgMap(v_rgba, z_depth, input.f3Custom);
    float3 Ka = v_rgba.rgb, Kd = v_rgba.rgb, Ks = g_cbPobj.Ks;
    ComputeColor(v_rgba.rgb, Ka, Kd, Ks, 0, input.f3PosWS, view_dir, nor, nor_len);
#else
    float3 Ka = g_cbPobj.Ka, Kd = g_cbPobj.Kd, Ks = g_cbPobj.Ks;
    float Ns = g_cbPobj.Ns;
    if ((g_cbPobj.pobj_flag & (0x1 << 3)) == 0)
        Ka = Kd = Ks = input.f3Custom;
    ComputeColor(v_rgba.rgb, Ka, Kd, Ks, Ns, input.f3PosWS, view_dir, nor, nor_len);
#endif
    
    // make it as an associated color.
    // as a color component is stored into 8 bit channel, the alpha-multiplied precision must be determined in this stage.
    // unless, noise dots appear.
    v_rgba.rgb *= v_rgba.a;
    
    int2 tex2d_xy = int2(input.f4PosSS.xy);
    
    // Atomically allocate space in the deep buffer
    uint fc;
    InterlockedAdd(fragment_counter[tex2d_xy], 1, fc);
    
    //int addr_base = (tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x) * num_deep_layers * 3;
    //uint nPrefixSumPos = tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x;
    //uint nDeepBufferPos;
    //if (nPrefixSumPos == 0)
    //    nDeepBufferPos = fc;
    //else
    //    nDeepBufferPos = prefixSum[nPrefixSumPos - 1] + fc;
    //
    //// Store fragment data into the allocated space
    //deepBufferDepth[nDeepBufferPos] = input.pos.z;
    ////float4 color = float4(1, 0, 0, 1);
    //deepBufferColor[nDeepBufferPos] = clamp(input.color, 0, 1) * 255;
    //
    //
    //FillKDepth_v1(tex2d_xy, g_cbCamState.num_deep_layers, v_rgba, z_depth, vz_thickness);
}