#include "Sr_Common.hlsl"

// core_max_zidx 
// case -1 : just put in max-array[frag_cnt - 1]
// case -2 : merge it to max-array[num_deep_layers - 1]
// else : replace
void ExploreMaxArray(inout int core_max_zidx, const in float cur_depth, 
    const in int addr_base, const in int num_deep_layers, const in int frag_cnt)
{
    core_max_zidx = -1;
    if (frag_cnt < num_deep_layers)
        return;
    
    float tail_z = asfloat(deepbuf_zdepth[addr_base + num_deep_layers - 1]);
    float tail_thickness = asfloat(deepbuf_zthick[addr_base + num_deep_layers - 1]);
    
    if (cur_depth > tail_z - tail_thickness)
    {
        core_max_zidx = -2;
        return;
    }
    
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
}

// this version considers the thickness is constant
void FillKDepth(const in int2 tex2d_xy, const in int num_deep_layers, const in float4 v_rgba, const in float z_depth, const in float z_thickness)
{
    if (z_depth > 10000000 || v_rgba.a == 0)
        return;

    uint frag_cnt = fragment_counter[tex2d_xy.xy]; // asynch 가 되는 경우 queue heap 에 저장이 안 될 수 있는데, 이 정도는 허용 가능 동작
    
    const float ferr_sq = 0.0001f * 0.0001f; // to do : dynamic variable
    uint iv_rgba = ConvertFloat4ToUInt(v_rgba);
    int addr_base = (tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x) * num_deep_layers;
#if __AVR_MERGE == 1
    int addr_base_vis = (tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x) * (num_deep_layers + 4);
#endif

    uint __dummy;
     
    // atomic routine (we are using spin-lock scheme)
    uint safe_unlock_count = 0;
    bool keep_loop = true;
    //break; ==> do not use this when using allow_uav_condition *** very important!
    [allow_uav_condition][loop]
    while (keep_loop)
    {
        if (++safe_unlock_count > 10000 && frag_cnt >= num_deep_layers)//g_cbPobj.num_safe_loopexit)
        {
            InterlockedExchange(fragment_counter[tex2d_xy.xy], 77777, __dummy);
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
#if __AVR_MERGE == 1
                    __InterlockedExchange(deepbuf_vis[addr_base_vis + zid], iv_rgba, __dummy);
#else
                    __InterlockedExchange(deepbuf_vis[addr], iv_rgba, __dummy);
#endif
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
#if __AVR_MERGE == 1
                    float4 vis_prev = ConvertUIntToFloat4(deepbuf_vis[addr_base_vis + zid]);
#else
                    float4 vis_prev = ConvertUIntToFloat4(deepbuf_vis[addr]);
#endif
                    float4 v_mix = BlendFloat4AndFloat4(vis_prev, v_rgba);
                    uint new_vis = ConvertFloat4ToUInt(v_mix);
#if __AVR_MERGE == 1
                    __InterlockedExchange(deepbuf_vis[addr_base_vis + zid], new_vis, __dummy);
#else
                    __InterlockedExchange(deepbuf_vis[addr], new_vis, __dummy);
#endif
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
#if __AVR_MERGE == 1
                    float4 vis_prev_max = ConvertUIntToFloat4(deepbuf_vis[addr_base_vis + core_max_zidx]);
#else
                    float4 vis_prev_max = ConvertUIntToFloat4(deepbuf_vis[addr_prev_max]);
#endif
                    float zdepth_prev_max = asfloat(deepbuf_zdepth[addr_prev_max]);
                    float zthick_prev_max = asfloat(deepbuf_zthick[addr_prev_max]);
                
                    // replace
#if __AVR_MERGE == 1
                    __InterlockedExchange(deepbuf_vis[addr_base_vis + core_max_zidx], iv_rgba, __dummy);
#else
                    __InterlockedExchange(deepbuf_vis[addr_prev_max], iv_rgba, __dummy);
#endif
                    __InterlockedExchange(deepbuf_zdepth[addr_prev_max], asuint(z_depth), __dummy);
                    __InterlockedExchange(deepbuf_zthick[addr_prev_max], asuint(z_thickness), __dummy);
                
                    // update the merging slot
                    UpdateTailSlot(addr_base, num_deep_layers, vis_prev_max, zdepth_prev_max, zthick_prev_max);
                }/**/

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
#if __AVR_MERGE == 1
    int addr_base_vis = (tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x) * (num_deep_layers + 4);
#endif

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
#if __AVR_MERGE == 1
        deepbuf_vis[addr_base_vis + zid] = iv_rgba;
#else
        deepbuf_vis[addr] = iv_rgba;
#endif
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
#if __AVR_MERGE == 1
        float4 vis_prev = ConvertUIntToFloat4(deepbuf_vis[addr_base_vis + zid]);
#else
        float4 vis_prev = ConvertUIntToFloat4(deepbuf_vis[addr]);
#endif
        float4 v_mix = BlendFloat4AndFloat4(vis_prev, v_rgba);
        uint new_vis = ConvertFloat4ToUInt(v_mix);
#if __AVR_MERGE == 1
        deepbuf_vis[addr_base_vis + zid] = new_vis;
#else
        deepbuf_vis[addr] = new_vis;
#endif
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
#if __AVR_MERGE == 1
        float4 vis_prev_max = ConvertUIntToFloat4(deepbuf_vis[addr_base_vis + core_max_zidx]);
#else
        float4 vis_prev_max = ConvertUIntToFloat4(deepbuf_vis[addr_prev_max]);
#endif
        float zdepth_prev_max = asfloat(deepbuf_zdepth[addr_prev_max]);
        float zthick_prev_max = asfloat(deepbuf_zthick[addr_prev_max]);
                
        // replace
#if __AVR_MERGE == 1
        deepbuf_vis[addr_base_vis + core_max_zidx] = iv_rgba;
#else
        deepbuf_vis[addr_prev_max] = iv_rgba;
#endif
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

#if __RENDERING_MODE == 2
void OIT_KDEPTH(VS_OUTPUT_TTT input)
#else
void OIT_KDEPTH(VS_OUTPUT input)
#endif
{
    POBJ_PRE_CONTEXT(return)
    
    float4 v_rgba;
    float vz_thickness = g_cbPobj.vz_thickness;
    
#if __RENDERING_MODE != 2
    float3 nor = input.f3VecNormalWS;
    float nor_len = length(nor);
#endif
    
#if __RENDERING_MODE == 1
    v_rgba = g_cbPobj.fcolor;
    DashedLine(v_rgba, z_depth, input.f3Custom.x, g_cbPobj.dash_interval, g_cbPobj.pobj_flag & (0x1 << 19), g_cbPobj.pobj_flag & (0x1 << 20));
#elif __RENDERING_MODE == 2
    v_rgba = g_cbPobj.fcolor;
    MultiTextMapping(v_rgba, z_depth, input.f3Custom0.xy, (int)(input.f3Custom0.z + 0.5f), input.f3Custom1, input.f3Custom2);
#elif __RENDERING_MODE == 3
    v_rgba = g_cbPobj.fcolor;
    TextMapping(v_rgba, z_depth, input.f3Custom.xy, g_cbPobj.pobj_flag & (0x1 << 9), g_cbPobj.pobj_flag & (0x1 << 10));
#elif __RENDERING_MODE == 4
#else
    float3 vtx_color = input.f3Custom;
    float vtx_alpha = g_cbPobj.fcolor.a;
 #if __CTMODELER_MODE == 1
    float4 _m_color = CtModeler_RMap(input.f3Custom);
    vtx_color = _m_color.rgb;
    vtx_alpha = _m_color.a;
#endif
    
    float3 light_dirinv = -g_cbCamState.dir_light_ws;
    if (g_cbCamState.cam_flag & 0x2)
        light_dirinv = -normalize(input.f3PosWS - g_cbCamState.pos_light_ws);
    float shade = 1.f;
    if (nor_len > 0)
        shade = PhongBlinn(g_cbRndEffect.pb_shading_factor, view_dir, light_dirinv, nor / nor_len, g_cbPobj.pobj_flag & (0x1 << 5));
    v_rgba.rgb = shade * vtx_color;
    v_rgba.a = vtx_alpha;
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
    FillKDepth(tex2d_xy, g_cbCamState.num_deep_layers, v_rgba, z_depth, vz_thickness);
}