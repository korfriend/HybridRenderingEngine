#include "Sr_Common.hlsl"

// this version considers the thickness is constant
void FillKDepth_zBlendUnion(const in int2 tex2d_xy, const in int num_deep_layers, const in float4 v_rgba, const in float z_depth, const in float z_thickness)
{
    if (z_depth > 10000000 || v_rgba.a == 0)
        return;

    uint frag_cnt = fragment_counter[tex2d_xy.xy]; // asynch 가 되는 경우 queue heap 에 저장이 안 될 수 있는데, 이 정도는 허용 가능 동작
    
    const float ferr_sq = 0.0001f * 0.0001f; // to do : dynamic variable
    uint iv_rgba = ConvertFloat4ToUInt(v_rgba);
    int addr_base = (tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x) * num_deep_layers;
    int addr_base_vis = (tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x) * (num_deep_layers + 4);

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
                float tail_z = asfloat(deepbuf_zdepth[addr_base + num_deep_layers - 1]);
                float tail_thickness = asfloat(deepbuf_zthick[addr_base + num_deep_layers - 1]);
                
                if (z_depth > tail_z - tail_thickness)
                {
                    // update the merging slot
                    UpdateTailSlot(addr_base, num_deep_layers, v_rgba, z_depth, z_thickness);
                }
                else
                {
                    RaySegment rs_cur;
                    rs_cur.zdepth = z_depth;
                    rs_cur.zthick = z_thickness;
                    rs_cur.fvis = v_rgba;
                    int work_slot = -1;
                    int empty_slot = -1;
                    int core_max_zidx = -1;
                    float core_max_z = -1.f;
                    [allow_uav_condition][loop]
                    for (int i = 0; i < num_deep_layers - 1; i++)
                    {
                        uint i_vis;
                        // after atomic store operation
                        i_vis = deepbuf_vis[addr_base_vis + i];
                        if (i_vis > 0)
                        {
                            RaySegment rs_ith;
                            rs_ith.fvis = ConvertUIntToFloat4(i_vis);
                            rs_ith.zdepth = asfloat(deepbuf_zdepth[addr_base + i]);
                            rs_ith.zthick = asfloat(deepbuf_zthick[addr_base + i]);
                            if (MergeFragRS(rs_cur, rs_ith, rs_cur) == 0)
                            {
                                if (work_slot < 0)
                                    work_slot = i;
                                
                                // do not use 'else' here due to flatten side-effect in a higly parallelism mechanism
                                if (work_slot >= 0)
                                {
                                    __InterlockedExchange(deepbuf_vis[addr_base_vis + i], 0, __dummy);
                                    __InterlockedExchange(deepbuf_zdepth[addr_base + i], asuint(FLT_MAX), __dummy);
                                    __InterlockedExchange(deepbuf_zthick[addr_base + i], 0, __dummy);
                                }
                                
                            }
                            else
                            {
                                if (core_max_z < rs_ith.zdepth)
                                {
                                    core_max_z = rs_ith.zdepth;
                                    core_max_zidx = i;
                                }
                            }
                            if (rs_cur.fvis.a < FLT_OPACITY_MIN__)
                                i = num_deep_layers;

                        }
                        else
                            empty_slot = i;

                    }
                    if (work_slot >= 0)
                    {
                        __InterlockedExchange(deepbuf_vis[addr_base_vis + work_slot], ConvertFloat4ToUInt(rs_cur.fvis), __dummy);
                        __InterlockedExchange(deepbuf_zdepth[addr_base + work_slot], asuint(rs_cur.zdepth), __dummy);
                        __InterlockedExchange(deepbuf_zthick[addr_base + work_slot], asuint(rs_cur.zthick), __dummy);
                    }
                    else // if (work_slot < 0)
                    {
                        if (empty_slot >= 0)  // core-filling case
                        {
                            __InterlockedExchange(deepbuf_vis[addr_base_vis + empty_slot], iv_rgba, __dummy);
                            __InterlockedExchange(deepbuf_zdepth[addr_base + empty_slot], asuint(z_depth), __dummy);
                            __InterlockedExchange(deepbuf_zthick[addr_base + empty_slot], asuint(z_thickness), __dummy);
                        }
                        else // replace the prev max one and then update max z slot
                        {
                            int addr_prev_max = addr_base + core_max_zidx;
                
                            // load
                            float4 vis_prev_max = ConvertUIntToFloat4(deepbuf_vis[addr_base_vis + core_max_zidx]);
                            float zdepth_prev_max = asfloat(deepbuf_zdepth[addr_prev_max]);
                            float zthick_prev_max = asfloat(deepbuf_zthick[addr_prev_max]);
                
                            // replace
                            __InterlockedExchange(deepbuf_vis[addr_base_vis + core_max_zidx], iv_rgba, __dummy);
                            __InterlockedExchange(deepbuf_zdepth[addr_prev_max], asuint(z_depth), __dummy);
                            __InterlockedExchange(deepbuf_zthick[addr_prev_max], asuint(z_thickness), __dummy);
                
                            // update the merging slot
                            UpdateTailSlot(addr_base, num_deep_layers, vis_prev_max, zdepth_prev_max, zthick_prev_max);
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

void FillKDepth_Subdivide(const in int2 tex2d_xy, const in int num_deep_layers, const in float4 v_rgba, const in float z_depth, const in float z_thickness)
{
    if (z_depth > 10000000 || v_rgba.a == 0)
        return;

    uint frag_cnt = fragment_counter[tex2d_xy.xy]; // asynch 가 되는 경우 queue heap 에 저장이 안 될 수 있는데, 이 정도는 허용 가능 동작
    
    const float ferr_sq = 0.0001f * 0.0001f; // to do : dynamic variable
    uint iv_rgba = ConvertFloat4ToUInt(v_rgba);
    int addr_base = (tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x) * num_deep_layers;
    int addr_base_vis = (tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x) * (num_deep_layers + 4);

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
                float tail_z = asfloat(deepbuf_zdepth[addr_base + num_deep_layers - 1]);
                float tail_thickness = asfloat(deepbuf_zthick[addr_base + num_deep_layers - 1]);
                
                if (z_depth > tail_z - tail_thickness)
                {
                    // update the merging slot
                    UpdateTailSlot(addr_base, num_deep_layers, v_rgba, z_depth, z_thickness);
                }
                else
                {
                    RaySegment rs_cur;
                    rs_cur.zdepth = z_depth;
                    rs_cur.zthick = z_thickness;
                    rs_cur.fvis = v_rgba;
                    int work_slot = -1;
                    int empty_slot = -1;
                    int core_max_zidx = -1;
                    float core_max_z = -1.f;
                    [allow_uav_condition][loop]
                    for (int i = 0; i < num_deep_layers - 1; i++)
                    {
                        uint i_vis;
                        // after atomic store operation
                        i_vis = deepbuf_vis[addr_base_vis + i];
                        if (i_vis > 0)
                        {
                            RaySegment rs_ith;
                            rs_ith.fvis = ConvertUIntToFloat4(i_vis);
                            rs_ith.zdepth = asfloat(deepbuf_zdepth[addr_base + i]);
                            rs_ith.zthick = asfloat(deepbuf_zthick[addr_base + i]);
                            MergeRS_OUT rs_out;
                            if (rs_ith.zdepth > rs_cur.zdepth)
                            {
                                rs_out = MergeRS(rs_cur, rs_ith);
                                rs_cur = rs_out.rs_prior;
                                rs_ith = rs_out.rs_posterior;
                            }
                            else
                            {
                                rs_out = MergeRS(rs_ith, rs_cur);
                                rs_cur = rs_out.rs_posterior;
                                rs_ith = rs_out.rs_prior;
                            }
                            //if (MergeFragRS_NV(rs_ith, rs_cur) == 0)
                            {
                                //if (work_slot < 0)
                                //    work_slot = i;
                                
                                // do not use 'else' here due to flatten side-effect in a higly parallelism mechanism
                                //if (work_slot >= 0)
                                {
                                    __InterlockedExchange(deepbuf_vis[addr_base_vis + i], ConvertFloat4ToUInt(rs_ith.fvis), __dummy);
                                    __InterlockedExchange(deepbuf_zdepth[addr_base + i], asuint(rs_ith.zdepth), __dummy);
                                    __InterlockedExchange(deepbuf_zthick[addr_base + i], asuint(rs_ith.zthick), __dummy);
                                }
                                
                            }
                            //else
                            {
                                if (core_max_z < rs_ith.zdepth)
                                {
                                    core_max_z = rs_ith.zdepth;
                                    core_max_zidx = i;
                                }
                            }
                            if (rs_cur.fvis.a < FLT_OPACITY_MIN__)
                                i = num_deep_layers;

                        }
                        else
                            empty_slot = i;

                    }
                    //if (work_slot >= 0)
                    //{
                    //    __InterlockedExchange(deepbuf_vis[addr_base_vis + work_slot], ConvertFloat4ToUInt(rs_cur.fvis), __dummy);
                    //    __InterlockedExchange(deepbuf_zdepth[addr_base + work_slot], asuint(rs_cur.zdepth), __dummy);
                    //    __InterlockedExchange(deepbuf_zthick[addr_base + work_slot], asuint(rs_cur.zthick), __dummy);
                    //}
                    //else // if (work_slot < 0)
                    {
                        if (empty_slot >= 0)  // core-filling case
                        {
                            __InterlockedExchange(deepbuf_vis[addr_base_vis + empty_slot], ConvertFloat4ToUInt(rs_cur.fvis), __dummy);
                            __InterlockedExchange(deepbuf_zdepth[addr_base + empty_slot], asuint(rs_cur.zdepth), __dummy);
                            __InterlockedExchange(deepbuf_zthick[addr_base + empty_slot], asuint(rs_cur.zthick), __dummy);
                        }
                        else // replace the prev max one and then update max z slot
                        {
                            int addr_prev_max = addr_base + core_max_zidx;
                            
                            // load
                            float4 vis_prev_max = ConvertUIntToFloat4(deepbuf_vis[addr_base_vis + core_max_zidx]);
                            float zdepth_prev_max = asfloat(deepbuf_zdepth[addr_prev_max]);
                            float zthick_prev_max = asfloat(deepbuf_zthick[addr_prev_max]);
                            
                            // replace
                            __InterlockedExchange(deepbuf_vis[addr_base_vis + core_max_zidx], ConvertFloat4ToUInt(rs_cur.fvis), __dummy);
                            __InterlockedExchange(deepbuf_zdepth[addr_prev_max], asuint(rs_cur.zdepth), __dummy);
                            __InterlockedExchange(deepbuf_zthick[addr_prev_max], asuint(rs_cur.zthick), __dummy);
                            
                            // update the merging slot
                            UpdateTailSlot(addr_base, num_deep_layers, vis_prev_max, zdepth_prev_max, zthick_prev_max);
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

void FillKDepth_Original(const in int2 tex2d_xy, const in int num_deep_layers, const in float4 v_rgba, const in float z_depth, const in float z_thickness)
{
    if (z_depth > 10000000 || v_rgba.a == 0)
        return;

    uint frag_cnt = fragment_counter[tex2d_xy.xy]; // asynch 가 되는 경우 queue heap 에 저장이 안 될 수 있는데, 이 정도는 허용 가능 동작
    
    const float ferr_sq = 0.0001f;// * 0.0001f; // to do : dynamic variable
    uint iv_rgba = ConvertFloat4ToUInt(v_rgba);
    int addr_base = (tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x) * num_deep_layers;
    int addr_base_vis = (tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x) * (num_deep_layers + 4);

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
            InterlockedCompareExchange(fragment_counter[tex2d_xy.xy], num_deep_layers, 77777, __dummy);
            //InterlockedCompareExchange(fragment_counter[tex2d_xy.xy], 1, 77777, __dummy);
            keep_loop = false;
            //break;
        }
        else
        {
            InterlockedAdd(fragment_counter[tex2d_xy.xy], 1, frag_cnt);
            
            uint spin_lock = 0;
            // note that all of fragment_spinlock are initialized as zero
            //InterlockedExchange(fragment_spinlock[tex2d_xy.xy], 1, spin_lock);
            InterlockedCompareExchange(fragment_spinlock[tex2d_xy.xy], 0, 1, spin_lock);
            if (spin_lock == 0)
            {
                // critical section / z_depth
                float core_max_z = z_depth;
                int core_max_zidx = -1;
                [allow_uav_condition][loop]
                for (int i = 0; i < num_deep_layers - 1; i++)
                {
                    int addr = addr_base + i;
                    float _z = asfloat(deepbuf_zdepth[addr]); // after atomic store operation
                    if (_z > 10000000)
                    {
                        core_max_zidx = num_deep_layers * 3 + i;
                        i = num_deep_layers;
                    }
                    else
                    {
#if CHECK_COPLANAR_DEPTH == 1
                        float z_diff = z_depth - _z;
                        if (z_diff * z_diff < ferr_sq) // check the coplanarity
                        {
                            core_max_zidx = num_deep_layers + i;
                            i = num_deep_layers;
                        } else 
#endif 
                        if (core_max_z < _z)
                        {
                            core_max_z = _z;
                            core_max_zidx = i;
                        }
                    }
                }
            
                if (core_max_zidx >= num_deep_layers * 3)
                {
                    // core-filling case
                    // just put the incoming fragment
                    int zid = core_max_zidx - num_deep_layers * 3;
                    int addr = addr_base + zid;
                    __InterlockedExchange(deepbuf_vis[addr_base_vis + zid], iv_rgba, __dummy);
                    __InterlockedExchange(deepbuf_zdepth[addr], asuint(z_depth), __dummy);
                    __InterlockedExchange(deepbuf_zthick[addr], asuint(z_thickness), __dummy);
                }
#if CHECK_COPLANAR_DEPTH == 1
                else if (core_max_zidx >= num_deep_layers)
                {
                    // core-filling case
                    // coplanarity case, use same deepbuf_zdepth and deepbuf_zthick
                    int zid = core_max_zidx - num_deep_layers;
                    int addr = addr_base + zid;
                    float4 vis_prev = ConvertUIntToFloat4(deepbuf_vis[addr_base_vis + zid]);
                    float4 v_mix = BlendFloat4AndFloat4(vis_prev, v_rgba);
                    uint new_vis = ConvertFloat4ToUInt(v_mix);
                    __InterlockedExchange(deepbuf_vis[addr_base_vis + zid], new_vis, __dummy);
                }
#endif
                else if (core_max_zidx < 0)
                {
                    // update the merging slot
                    UpdateTailSlot(addr_base, num_deep_layers, v_rgba, z_depth, z_thickness);
                }
                else
                {
                    // replace the prev max one and then update max z slot
                    int addr_prev_max = addr_base + core_max_zidx;
                
                    // load
                    float4 vis_prev_max = ConvertUIntToFloat4(deepbuf_vis[addr_base_vis + core_max_zidx]);
                    float zdepth_prev_max = asfloat(deepbuf_zdepth[addr_prev_max]);
                    float zthick_prev_max = asfloat(deepbuf_zthick[addr_prev_max]);
                
                    // replace
                    __InterlockedExchange(deepbuf_vis[addr_base_vis + core_max_zidx], iv_rgba, __dummy);
                    __InterlockedExchange(deepbuf_zdepth[addr_prev_max], asuint(z_depth), __dummy);
                    __InterlockedExchange(deepbuf_zthick[addr_prev_max], asuint(z_thickness), __dummy);
                
                    // update the merging slot
                    UpdateTailSlot(addr_base, num_deep_layers, vis_prev_max, zdepth_prev_max, zthick_prev_max);
                } /**/

                // always fragment_spinlock[tex2d_xy.xy] is 1, thus use InterlockedExchange instead of InterlockedCompareExchange
                InterlockedExchange(fragment_spinlock[tex2d_xy.xy], 0, spin_lock);
                //InterlockedCompareExchange(fragment_spinlock[tex2d_xy.xy], 1, 0, spin_lock);
                keep_loop = false;
                //break;
            } // critical section
        }
    } // while for spin-lock
}

void FillKDepth_zBlendUnion_Aw(const in int2 tex2d_xy, const in int num_deep_layers, const in float4 v_rgba, const in float z_depth, const in float z_thickness)
{
    if (z_depth > 10000000 || v_rgba.a == 0)
        return;

    uint frag_cnt = fragment_counter[tex2d_xy.xy]; // asynch 가 되는 경우 queue heap 에 저장이 안 될 수 있는데, 이 정도는 허용 가능 동작
    
    const float ferr_sq = 0.0001f * 0.0001f; // to do : dynamic variable
    uint iv_rgba = ConvertFloat4ToUInt(v_rgba);
    int addr_base = (tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x) * num_deep_layers;
    int addr_base_vis = (tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x) * (num_deep_layers + 4);

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
                float tail_z = asfloat(deepbuf_zdepth[addr_base + num_deep_layers - 1]);
                //float tail_thickness = asfloat(deepbuf_zthick[addr_base + num_deep_layers - 1]);
                uint tc = deepbuf_zthick[addr_base + num_deep_layers - 1];
                float tail_thickness = f16tof32(tc & 0xFFFF);
                //uint alphaw = 
                
                if (z_depth > tail_z - tail_thickness)
                {
                    // update the merging slot
                    UpdateTailSlot(addr_base, num_deep_layers, v_rgba, z_depth, z_thickness);
                }
                else
                {
                    RaySegment2 rs_cur;
                    rs_cur.zdepth = z_depth;
                    rs_cur.fvis = v_rgba;
                    rs_cur.zthick = z_thickness;
                    rs_cur.alphaw = v_rgba.a;
                    int work_slot = -1;
                    int empty_slot = -1;
                    int core_max_zidx = -1;
                    float core_max_z = -1.f;
                    [allow_uav_condition][loop]
                    for (int i = 0; i < num_deep_layers - 1; i++)
                    {
                        uint i_vis;
                        // after atomic store operation
                        i_vis = deepbuf_vis[addr_base_vis + i];
                        if (i_vis > 0)
                        {
                            RaySegment2 rs_ith;
                            rs_ith.fvis = ConvertUIntToFloat4(i_vis);
                            rs_ith.zdepth = asfloat(deepbuf_zdepth[addr_base + i]);
                            //rs_ith.zthick = asfloat(deepbuf_zthick[addr_base + i]);
                            uint tc_ith = deepbuf_zthick[addr_base + i];
                            rs_ith.zthick = f16tof32(tc_ith & 0xFFFF);
                            rs_ith.alphaw = (tc_ith >> 16) / 255.f;
                            if (MergeFragRS2(rs_cur, rs_ith, rs_cur) == 0)
                            {
                                if (work_slot < 0)
                                    work_slot = i;
                                
                                // do not use 'else' here due to flatten side-effect in a higly parallelism mechanism
                                if (work_slot >= 0)
                                {
                                    __InterlockedExchange(deepbuf_vis[addr_base_vis + i], 0, __dummy);
                                    __InterlockedExchange(deepbuf_zdepth[addr_base + i], asuint(FLT_MAX), __dummy);
                                    __InterlockedExchange(deepbuf_zthick[addr_base + i], 0, __dummy);
                                }
                                
                            }
                            else
                            {
                                if (core_max_z < rs_ith.zdepth)
                                {
                                    core_max_z = rs_ith.zdepth;
                                    core_max_zidx = i;
                                }
                            }
                            if (rs_cur.fvis.a < FLT_OPACITY_MIN__)
                                i = num_deep_layers;

                        }
                        else
                            empty_slot = i;

                    }
                    if (work_slot >= 0)
                    {
                        __InterlockedExchange(deepbuf_vis[addr_base_vis + work_slot], ConvertFloat4ToUInt(rs_cur.fvis), __dummy);
                        __InterlockedExchange(deepbuf_zdepth[addr_base + work_slot], asuint(rs_cur.zdepth), __dummy);
                        //__InterlockedExchange(deepbuf_zthick[addr_base + work_slot], asuint(rs_cur.zthick), __dummy);
                        uint tc_cur = (uint(min(rs_cur.alphaw, 255) * 255) << 16) | f32tof16(rs_cur.zthick);
                        __InterlockedExchange(deepbuf_zthick[addr_base + work_slot], tc_cur, __dummy);
                    }
                    else // if (work_slot < 0)
                    {
                        if (empty_slot >= 0)  // core-filling case
                        {
                            __InterlockedExchange(deepbuf_vis[addr_base_vis + empty_slot], iv_rgba, __dummy);
                            __InterlockedExchange(deepbuf_zdepth[addr_base + empty_slot], asuint(z_depth), __dummy);
                            //__InterlockedExchange(deepbuf_zthick[addr_base + empty_slot], asuint(z_thickness), __dummy);
                            uint tc_cur = (uint(v_rgba.a * 255) << 16) | f32tof16(z_thickness);
                            __InterlockedExchange(deepbuf_zthick[addr_base + empty_slot], tc_cur, __dummy);
                        }
                        else // replace the prev max one and then update max z slot
                        {
                            int addr_prev_max = addr_base + core_max_zidx;
                
                            // load
                            float4 vis_prev_max = ConvertUIntToFloat4(deepbuf_vis[addr_base_vis + core_max_zidx]);
                            float zdepth_prev_max = asfloat(deepbuf_zdepth[addr_prev_max]);
                            float zthick_prev_max = asfloat(deepbuf_zthick[addr_prev_max]);
                
                            // replace
                            __InterlockedExchange(deepbuf_vis[addr_base_vis + core_max_zidx], iv_rgba, __dummy);
                            __InterlockedExchange(deepbuf_zdepth[addr_prev_max], asuint(z_depth), __dummy);
                            //__InterlockedExchange(deepbuf_zthick[addr_prev_max], asuint(z_thickness), __dummy);
                            uint tc_cur = (uint(v_rgba.a * 255) << 16) | f32tof16(z_thickness);
                            __InterlockedExchange(deepbuf_zthick[addr_prev_max], tc_cur, __dummy);
                
                            // update the merging slot
                            UpdateTailSlot(addr_base, num_deep_layers, vis_prev_max, zdepth_prev_max, zthick_prev_max);
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

void FillKDepth_Avr(const in int2 tex2d_xy, const in int num_deep_layers, const in float4 v_rgba, const in float z_depth, const in float z_thickness)
{
    if (z_depth > 10000000 || v_rgba.a == 0)
        return;

    uint frag_cnt = fragment_counter[tex2d_xy.xy]; // asynch 가 되는 경우 queue heap 에 저장이 안 될 수 있는데, 이 정도는 허용 가능 동작
    
    const float ferr_sq = 0.0001f * 0.0001f; // to do : dynamic variable z_thickness
    uint iv_rgba = ConvertFloat4ToUInt(v_rgba);
    int addr_base = (tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x) * num_deep_layers;
    int addr_base_vis = (tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x) * (num_deep_layers + 4);

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
                float tail_z = asfloat(deepbuf_zdepth[addr_base + num_deep_layers - 1]);
                //float tail_thickness = asfloat(deepbuf_zthick[addr_base + num_deep_layers - 1]);
                uint tc = deepbuf_zthick[addr_base + num_deep_layers - 1];
                float tail_thickness = f16tof32(tc & 0xFFFF);
                
                if (z_depth > tail_z - tail_thickness)
                {
                    // update the merging slot
                    UpdateTailSlot(addr_base, num_deep_layers, v_rgba, z_depth, z_thickness);
                }
                else
                {
                    RaySegment2 rs_cur;
                    rs_cur.zdepth = z_depth;
                    rs_cur.fvis = v_rgba;
                    rs_cur.zthick = z_thickness;
                    rs_cur.alphaw = v_rgba.a;
                    int work_slot = -1;
                    int empty_slot = -1;
                    int core_max_zidx = -1;
                    float core_max_z = -1.f;
                    [allow_uav_condition][loop]
                    for (int i = 0; i < num_deep_layers - 1; i++)
                    {
                        uint i_vis;
                        // after atomic store operation
                        i_vis = deepbuf_vis[addr_base_vis + i];
                        if (i_vis > 0)
                        {
                            RaySegment2 rs_ith;
                            rs_ith.fvis = ConvertUIntToFloat4(i_vis);
                            rs_ith.zdepth = asfloat(deepbuf_zdepth[addr_base + i]);
                            //rs_ith.zthick = asfloat(deepbuf_zthick[addr_base + i]);
                            uint tc_ith = deepbuf_zthick[addr_base + i];
                            rs_ith.zthick = 1;//f16tof32(tc_ith & 0xFFFF);
                            rs_ith.alphaw = (tc_ith >> 16) / 255.f;
                            if (MergeFragRS_Avr(rs_cur, rs_ith, rs_cur) == 0)
                            {
                                if (work_slot < 0)
                                    work_slot = i;
                                
                                // do not use 'else' here due to flatten side-effect in a higly parallelism mechanism
                                if (work_slot >= 0)
                                {
                                    __InterlockedExchange(deepbuf_vis[addr_base_vis + i], 0, __dummy);
                                    __InterlockedExchange(deepbuf_zdepth[addr_base + i], asuint(FLT_MAX), __dummy);
                                    __InterlockedExchange(deepbuf_zthick[addr_base + i], 0, __dummy);
                                }
                                
                            }
                            else
                            {
                                if (core_max_z < rs_ith.zdepth)
                                {
                                    core_max_z = rs_ith.zdepth;
                                    core_max_zidx = i;
                                }
                            }
                            if (rs_cur.fvis.a < FLT_OPACITY_MIN__)
                                i = num_deep_layers;

                        }
                        else
                            empty_slot = i;

                    }
                    if (work_slot >= 0)
                    {
                        __InterlockedExchange(deepbuf_vis[addr_base_vis + work_slot], ConvertFloat4ToUInt(rs_cur.fvis), __dummy);
                        __InterlockedExchange(deepbuf_zdepth[addr_base + work_slot], asuint(rs_cur.zdepth), __dummy);
                        //__InterlockedExchange(deepbuf_zthick[addr_base + work_slot], asuint(rs_cur.zthick), __dummy);
                        uint tc_cur = (uint(min(rs_cur.alphaw, 255) * 255) << 16) | f32tof16(rs_cur.zthick);
                        __InterlockedExchange(deepbuf_zthick[addr_base + work_slot], tc_cur, __dummy);
                    }
                    else // if (work_slot < 0)
                    {
                        if (empty_slot >= 0)  // core-filling case
                        {
                            __InterlockedExchange(deepbuf_vis[addr_base_vis + empty_slot], iv_rgba, __dummy);
                            __InterlockedExchange(deepbuf_zdepth[addr_base + empty_slot], asuint(z_depth), __dummy);
                            //__InterlockedExchange(deepbuf_zthick[addr_base + empty_slot], asuint(z_thickness), __dummy);
                            uint tc_cur = (uint(v_rgba.a * 255) << 16) | f32tof16(z_thickness);
                            __InterlockedExchange(deepbuf_zthick[addr_base + empty_slot], tc_cur, __dummy);
                        }
                        else // replace the prev max one and then update max z slot
                        {
                            int addr_prev_max = addr_base + core_max_zidx;
                
                            // load
                            float4 vis_prev_max = ConvertUIntToFloat4(deepbuf_vis[addr_base_vis + core_max_zidx]);
                            float zdepth_prev_max = asfloat(deepbuf_zdepth[addr_prev_max]);
                            float zthick_prev_max = asfloat(deepbuf_zthick[addr_prev_max]);
                
                            // replace
                            __InterlockedExchange(deepbuf_vis[addr_base_vis + core_max_zidx], iv_rgba, __dummy);
                            __InterlockedExchange(deepbuf_zdepth[addr_prev_max], asuint(z_depth), __dummy);
                            //__InterlockedExchange(deepbuf_zthick[addr_prev_max], asuint(z_thickness), __dummy);
                            uint tc_cur = (uint(v_rgba.a * 255) << 16) | f32tof16(z_thickness);
                            __InterlockedExchange(deepbuf_zthick[addr_prev_max], tc_cur, __dummy);
                
                            // update the merging slot
                            UpdateTailSlot(addr_base, num_deep_layers, vis_prev_max, zdepth_prev_max, zthick_prev_max);
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

void FillKDepth_NoInterlock(const in int2 tex2d_xy, const in int num_deep_layers, const in float4 v_rgba, const in float z_depth, const in int frag_cnt, const in float z_thickness)
{
    if (z_depth > 10000000 || v_rgba.a == 0)
        return;    
    
    const float ferr_sq = 0.0001f * 0.0001f; // to do : dynamic variable
    uint iv_rgba = ConvertFloat4ToUInt(v_rgba);
    int addr_base = (tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x) * num_deep_layers;
    int addr_base_vis = (tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x) * (num_deep_layers + 4);

    float core_max_z = z_depth;
    int core_max_zidx = -1;
    [loop]
    for (int i = 0; i < num_deep_layers - 1; i++)
    {
        int addr = addr_base + i;
        float _z = asfloat(deepbuf_zdepth[addr]); // after atomic store operation
        if (_z > 10000000)
        {
            core_max_zidx = num_deep_layers * 3 + i;
            i = num_deep_layers;
        }
        else
        {
#if CHECK_COPLANAR_DEPTH == 1
            float z_diff = z_depth - _z;
            if (z_diff * z_diff < ferr_sq) // check the coplanarity
            {
                core_max_zidx = num_deep_layers + i;
                i = num_deep_layers;
            } else
#endif
            if (core_max_z < _z)
            {
                core_max_z = _z;
                core_max_zidx = i;
            }
        }
    }
            
    if (core_max_zidx >= num_deep_layers * 3)
    {
        // core-filling case
        // just put the incoming fragment
        int zid = core_max_zidx - num_deep_layers * 3;
        int addr = addr_base + zid;
        deepbuf_vis[addr_base_vis + zid] = iv_rgba;
        deepbuf_zdepth[addr] = asuint(z_depth);
        deepbuf_zthick[addr] = asuint(z_thickness);
    }
#if CHECK_COPLANAR_DEPTH == 1
    else if (core_max_zidx >= num_deep_layers)
    {
        // core-filling case
        // coplanarity case, use same deepbuf_zdepth and deepbuf_zthick
        int zid = core_max_zidx - num_deep_layers;
        int addr = addr_base + zid;
        float4 vis_prev = ConvertUIntToFloat4(deepbuf_vis[addr_base_vis + zid]);
        float4 v_mix = BlendFloat4AndFloat4(vis_prev, v_rgba);
        uint new_vis = ConvertFloat4ToUInt(v_mix);
        deepbuf_vis[addr_base_vis + zid] = new_vis;
    }
#endif
    else if (core_max_zidx < 0)
    {
        // update the merging slot
        UpdateTailSlot(addr_base, num_deep_layers, v_rgba, z_depth, z_thickness);
    }
    else
    {
        // replace the prev max one and then update max z slot
        int addr_prev_max = addr_base + core_max_zidx;
                
        // load
        float4 vis_prev_max = ConvertUIntToFloat4(deepbuf_vis[addr_base_vis + core_max_zidx]);
        float zdepth_prev_max = asfloat(deepbuf_zdepth[addr_prev_max]);
        float zthick_prev_max = asfloat(deepbuf_zthick[addr_prev_max]);
                
        // replace
        deepbuf_vis[addr_base_vis + core_max_zidx] = iv_rgba;
        deepbuf_zdepth[addr_prev_max] = asuint(z_depth);
        deepbuf_zthick[addr_prev_max] = asuint(z_thickness);
                
        // update the merging slot
        UpdateTailSlot(addr_base, num_deep_layers, vis_prev_max, zdepth_prev_max, zthick_prev_max);
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
    
    uint frag_cnt;
    InterlockedAdd(fragment_counter[tex2d_xy.xy], 1, frag_cnt);
    if (frag_cnt > 999)
        return;
    
    // float vz_thickness = g_cbPobj.vz_thickness;
    FillKDepth_NoInterlock(tex2d_xy, g_cbCamState.num_deep_layers, v_rgba, depthcs, frag_cnt, g_cbCamState.cam_vz_thickness);
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
    FillKDepth_Avr(tex2d_xy, g_cbCamState.num_deep_layers, v_rgba, z_depth, vz_thickness);
}