#include "ShaderCommonHeader.hlsl"

//=====================
// Constant Buffers
//=====================
cbuffer cbGlobalParams : register(b0) // SLOT 0
{
    VxPolygonObject g_cbPolygonObj;
}

cbuffer cbGlobalParams : register(b1) // SLOT 1
{
    VxCameraProjectionState g_cbCamState;
}

cbuffer cbGlobalParams : register(b2) // SLOT 2
{
    VxVolumeObject g_cbVolObj;
}

cbuffer cbGlobalParams : register(b3) // SLOT 3
{
    VxBlockObject g_cbBlkObj;
}

cbuffer cbGlobalParams : register(b7) // SLOT 7
{
    VxOtf1D g_cbOtf1D;
}

cbuffer cbGlobalParams : register(b10) // SLOT 10
{
    VxProxyParameter g_cbProxy;
}

struct FLoatEncodeInfo
{
    float z_depth_min;
    float z_depth_max;
    float z_thick_min;
    float z_thick_max;
};

cbuffer cbGlobalParams : register(b11) // SLOT 11
{
    FLoatEncodeInfo g_cbFloatEncode;
}

Buffer<float4> g_f4bufOTF : register(t0); // Mask OTFs StructuredBuffer
Texture3D g_tex3DVolume : register(t1);
Texture3D g_tex3DBlock : register(t2); // For Deviation
Texture2D g_tex2DTextCMM : register(t3);

struct VS_INPUT_PN
{
    float3 f3PosOS : POSITION;
    float3 f3VecNormalOS : NORMAL;
};

struct VS_INPUT_PT
{
    float3 f3PosOS : POSITION;
    float3 f3Custom : TEXCOORD0;
};

struct VS_INPUT_PTTT
{
    float3 f3PosOS : POSITION;
    float3 f3Custom0 : TEXCOORD0;
    float3 f3Custom1 : TEXCOORD1;
    float3 f3Custom2 : TEXCOORD2;
};

struct VS_INPUT_PNT
{
    float3 f3PosOS : POSITION;
    float3 f3VecNormalOS : NORMAL;
    float3 f3Custom : TEXCOORD0;
};

struct VS_OUTPUT
{
    float4 f4PosSS : SV_POSITION;
    float3 f3VecNormalWS : NORMAL;
    float3 f3PosWS : TEXCOORD0;
    float3 f3Custom : TEXCOORD1;
};

struct VS_OUTPUT_TTT
{
    float4 f4PosSS : SV_POSITION;
	//float3 f3VecNormalWS : NORMAL;
    float3 f3PosWS : TEXCOORD0;
    float3 f3Custom0 : TEXCOORD1;
    float3 f3Custom1 : TEXCOORD2;
    float3 f3Custom2 : TEXCOORD3;
};

struct PS_OUTPUT
{
    float fDepth : SV_Depth; // to avoid Clipping Early-Out-Ocllusion

    float4 f4Color : SV_TARGET0; // UNORM
    float fDepthCS : SV_TARGET1;
};

VS_OUTPUT CommonVS_P(float3 f3Pos : POSITION)
{
    VS_OUTPUT vout = (VS_OUTPUT) 0;
    vout.f4PosSS = mul(float4(f3Pos, 1.f), g_cbPolygonObj.matOS2PS);
    vout.f3PosWS = vxTransformPoint(f3Pos, g_cbPolygonObj.matOS2WS);
    vout.f3VecNormalWS = (float3) 0;
    vout.f3Custom = g_cbPolygonObj.f4Color.rgb;
    vout.f4PosSS.z -= g_cbPolygonObj.fDepthForwardBias;
    return vout;
}

VS_OUTPUT CommonVS_PN(VS_INPUT_PN input)
{
    VS_OUTPUT vout = (VS_OUTPUT) 0;
    vout.f4PosSS = mul(float4(input.f3PosOS, 1.f), g_cbPolygonObj.matOS2PS);
    vout.f3PosWS = vxTransformPoint(input.f3PosOS, g_cbPolygonObj.matOS2WS);
    vout.f3VecNormalWS = normalize(vxTransformVector(input.f3VecNormalOS, g_cbPolygonObj.matOS2WS));
    vout.f3Custom = g_cbPolygonObj.f4Color.rgb;
    vout.f4PosSS.z -= g_cbPolygonObj.fDepthForwardBias;
    return vout;
}

VS_OUTPUT CommonVS_PT(VS_INPUT_PT input)
{
    VS_OUTPUT vout = (VS_OUTPUT) 0;
    vout.f4PosSS = mul(float4(input.f3PosOS, 1.f), g_cbPolygonObj.matOS2PS);
    vout.f3PosWS = vxTransformPoint(input.f3PosOS, g_cbPolygonObj.matOS2WS);
    vout.f3VecNormalWS = (float3) 0;
    vout.f3Custom = input.f3Custom;
    vout.f4PosSS.z -= g_cbPolygonObj.fDepthForwardBias;
    return vout;
}

VS_OUTPUT CommonVS_PNT(VS_INPUT_PNT input)
{
    VS_OUTPUT vout = (VS_OUTPUT) 0;
    vout.f4PosSS = mul(float4(input.f3PosOS, 1.f), g_cbPolygonObj.matOS2PS);
    vout.f3PosWS = vxTransformPoint(input.f3PosOS, g_cbPolygonObj.matOS2WS);
    vout.f3VecNormalWS = normalize(vxTransformVector(input.f3VecNormalOS, g_cbPolygonObj.matOS2WS));
    vout.f3Custom = input.f3Custom;
    vout.f4PosSS.z -= g_cbPolygonObj.fDepthForwardBias;
    return vout;
}

VS_OUTPUT_TTT CommonVS_PTTT(VS_INPUT_PTTT input)
{
    VS_OUTPUT_TTT vout = (VS_OUTPUT_TTT) 0;
    vout.f4PosSS = mul(float4(input.f3PosOS, 1.f), g_cbPolygonObj.matOS2PS);
    vout.f3PosWS = vxTransformPoint(input.f3PosOS, g_cbPolygonObj.matOS2WS);
    vout.f3Custom0 = input.f3Custom0;
    vout.f3Custom1 = vxTransformVector(input.f3Custom1, g_cbPolygonObj.matOS2PS);
    vout.f3Custom2 = vxTransformVector(input.f3Custom2, g_cbPolygonObj.matOS2PS);
    vout.f3Custom1.y *= -1;
    vout.f3Custom2.y *= -1;
    return vout;
}


//uint __ConvertFloatToUint(float fv, float min_ec, float max_ec)
//{
//    fv = clamp(fv, min_ec, max_ec);
//    return fv - min_ec / (max_ec - min_ec) * (int) (0xFFFFFFFF);
//
//}
//
//float __ConvertUIntToFloat(int iv, float min_ec, float max_ec)
//{
//
//}
#define NUM_DEEP_LAYERS 8
// counter 있어야 하고... k-layers check!
// z-depth 비교 - 앞 vs '뒤'
// which one is max...
// read 한 것과 현재 비교.. InterlockedExchange 으로 교환...

// InterlockedMax 로 z-depth 비교하고... ??(original 과...)

RWTexture2D<uint> fragment_counter : register(u1);
RWTexture2D<uint> fragment_spinlock : register(u2);
RWTexture2D<uint> fragment_depth : register(u3);
//RWBuffer<uint> deepbuf_vis : register(u3); 
//RWBuffer<uint> deepbuf_zdepth : register(u4);
//RWBuffer<uint> deepbuf_zthick : register(u5);
// ========> RWBuffer<uint>, RWBuffer<float> * 8
// atomic operation is needed
// so structured is handicaped
// original OIT 랑 비교...;

float4 __BlendFloat4AndFloat4(float4 fColor1, float4 fColor2)
{
    return fColor1 + fColor2 - (fColor1 * fColor2.a + fColor2 * fColor1.a) / 2.f;
}

float4 __ConvertUIntToFloat4(uint iColor)
{
	// RGBA
    return float4(((iColor >> 16) & 0xFF) / 255.f, ((iColor >> 8) & 0xFF) / 255.f, (iColor & 0xFF) / 255.f, ((iColor >> 24) & 0xFF) / 255.f);
}

uint __ConvertFloat4ToUInt(float4 fColor)
{
	// RGBA
    uint iR = (uint) min(fColor.x * 255.f, 255.f);
    uint iG = (uint) min(fColor.y * 255.f, 255.f);
    uint iB = (uint) min(fColor.z * 255.f, 255.f);
    uint iA = (uint) min(fColor.w * 255.f, 255.f);
    return (iA << 24) | (iR << 16) | (iG << 8) | iB;
}

void __MergeTailSlot(float4 v_rgba, int addr_tail, float z_depth)
{
    // to do : blending comparison
    uint tail_vis = deepbuf_vis[addr_tail];
    float4 new_vis = __BlendFloat4AndFloat4(v_rgba, __ConvertUIntToFloat4(tail_vis));
    uint newi_vis = __ConvertFloat4ToUInt(new_vis);
    //InterlockedExchange(deepbuf_vis[addr_tail], newi_vis, newi_vis);
    deepbuf_vis[addr_tail] = newi_vis;
                        
    float z_prev = deepbuf_zdepth[addr_tail];
    float z_thick_prev = deepbuf_zthick[addr_tail];
    float z_new, z_thick_new;
    if (z_depth > z_prev)
    {
        z_new = z_depth;
        z_thick_new = z_thick_prev + (z_depth - z_prev);
    }
    else if (z_depth < z_prev - z_thick_prev)
    {
        z_new = z_prev;
        z_thick_new = z_thick_prev;
    }
    else // if (z_depth > z_prev);
    {
        z_new = z_prev;
        z_thick_new = (z_prev - z_depth);
    }
    deepbuf_zdepth[addr_tail] = z_new;
    deepbuf_zthick[addr_tail] = z_thick_new;
    //uint dummyi;
    //InterlockedExchange(deepbuf_zdepth[addr_tail], asuint(z_new), dummyi); // asuint
    //InterlockedExchange(deepbuf_zthick[addr_tail], asuint(z_thick_new), dummyi);
}

void __UpdateTailSlot(int addr_base, float4 vis_in, float zdepth_in, float zthick_in)
{
    int addr_merge_slot = addr_base + (NUM_DEEP_LAYERS - 1);
    float4 vis_prev_merge = __ConvertUIntToFloat4(deepbuf_vis[addr_merge_slot]); // w/ prev_maxz ===> thickness buffer model 나중에 적용
    uint vis_blend = __ConvertFloat4ToUInt(__BlendFloat4AndFloat4(vis_prev_merge, vis_in));
                
    float zdepth_prev_merge = deepbuf_zdepth[addr_merge_slot];
    float zthick_prev_merge = deepbuf_zthick[addr_merge_slot];
    float new_back_z = zdepth_in, new_front_z = zthick_in;
    if (zthick_prev_merge > 0)
    {
        new_back_z = max(zdepth_in, zdepth_prev_merge);
        new_front_z = min(zdepth_in - zthick_in, zdepth_prev_merge - zthick_prev_merge);
    }
    
    uint __dummy;
    InterlockedExchange(deepbuf_vis[addr_merge_slot], vis_blend, __dummy);
    InterlockedExchange(deepbuf_zdepth[addr_merge_slot], asuint(new_back_z), __dummy);
    InterlockedExchange(deepbuf_zthick[addr_merge_slot], asuint(new_back_z - new_front_z), __dummy);
}


struct PS_FILL_OUTPUT
{
    float ds_z : SV_Depth;
};
//PS_FILL_OUTPUT
void OIT_TEXMAP(VS_OUTPUT input)
{
    PS_FILL_OUTPUT out_ps;
    out_ps.ds_z = 1.f;
    if (g_cbPolygonObj.iFlag & 0x1)
    {
        if (dot(g_cbPolygonObj.f3VecClipPlaneWS, input.f3PosWS - g_cbPolygonObj.f3PosClipPlaneWS) > 0)
            return; //out_ps;
    }
    if (g_cbPolygonObj.iFlag & 0x2)
    {
        if (!vxIsInsideClipBox(input.f3PosWS, g_cbPolygonObj.f3ClipBoxMaxBS, g_cbPolygonObj.matClipBoxWS2BS))
            return; //out_ps;
    }
    
    float3 pos_ip_ss = float3(input.f4PosSS.xy, 0.0f);
    float3 pos_ip_ws = vxTransformPoint(pos_ip_ss, g_cbCamState.matSS2WS);

    float3 vec_pos_ip2frag = input.f3PosWS - pos_ip_ws;
    float z_depth = length(vec_pos_ip2frag);
    if (dot(vec_pos_ip2frag, g_cbCamState.f3VecViewWS) < 0)
        return; //out_ps;
    
    // atomic routine (we are using spin-lock scheme)
    bool keep_loop = true;
    [allow_uav_condition]
    while (keep_loop)
    {
        uint spin_lock = 0;
        // note that all of fragment_spinlock are initialized as zero
        InterlockedExchange(fragment_spinlock[pos_ip_ss.xy], 1, spin_lock);
        if (spin_lock == 0)
        {

        } // critical section
    } // while for spin-lock
}

/*
void OIT_TEXMAP_(VS_OUTPUT input)
{
    PS_FILL_OUTPUT out_ps;
    out_ps.ds_z = 1.f;
    if (g_cbPolygonObj.iFlag & 0x1)
    {
        if (dot(g_cbPolygonObj.f3VecClipPlaneWS, input.f3PosWS - g_cbPolygonObj.f3PosClipPlaneWS) > 0)
            return ;//out_ps;
    }
    if (g_cbPolygonObj.iFlag & 0x2)
    {
        if (!vxIsInsideClipBox(input.f3PosWS, g_cbPolygonObj.f3ClipBoxMaxBS, g_cbPolygonObj.matClipBoxWS2BS))
            return ;//out_ps;
    }
    
    float3 pos_ip_ss = float3(input.f4PosSS.xy, 0.0f);
    float3 pos_ip_ws = vxTransformPoint(pos_ip_ss, g_cbCamState.matSS2WS);

    float3 vec_pos_ip2frag = input.f3PosWS - pos_ip_ws;
    float z_depth = length(vec_pos_ip2frag);
    if (dot(vec_pos_ip2frag, g_cbCamState.f3VecViewWS) < 0)
        return ;//out_ps;
    
    float4 shading_factors = g_cbPolygonObj.f4ShadingFactor;
    float3 light_dirinv = -g_cbCamState.f3VecLightWS;
    if (g_cbCamState.iFlag & 0x2)
        light_dirinv = -normalize(input.f3PosWS - g_cbCamState.f3PosLightWS);
    float nor_len = length(input.f3VecNormalWS);
    float diff = 0;
    if (nor_len > 0)
        //diff = max(dot(light_dirinv, input.f3VecNormalWS / nor_len) * g_cbPolygonObj.fShadingMultiplier, 0);
        diff = abs(dot(light_dirinv, input.f3VecNormalWS / nor_len) * g_cbPolygonObj.fShadingMultiplier);
    
    float shade = shading_factors.x + shading_factors.y * diff + shading_factors.z * pow(diff, abs(shading_factors.w));
    float4 v_rgba;
    v_rgba.rgb = shade * input.f3Custom; // g_cbPolygonObj.f4Color.rgb; //
    v_rgba.a = g_cbPolygonObj.f4Color.a;
    uint iv_rgba = __ConvertFloat4ToUInt(v_rgba);
    //float4 v_rgba;
    //v_rgba.rgb = g_cbPolygonObj.f4Color.rgb;//input.f3Custom;
    //v_rgba.a = g_cbPolygonObj.f4Color.a;
    //v_rgba.rgb *= v_rgba.a; // make it as an associated color

    // add opacity culling with biased z depth
    // to do : SS-based LAO 에서는 z-culling 안 되도록.
    //if (v_rgba.a > 0.99f)
    //    out_ps.ds_z = input.f4PosSS.z + 0.00f; // to do : bias z
    
    uint x = pos_ip_ss.x;
    uint y = pos_ip_ss.y;
    int addr_base = (y * g_cbProxy.uiScreenSizeX + x) * NUM_DEEP_LAYERS;

    //// test //
    //uint frag_cnt;
    //InterlockedAdd(fragment_counter[pos_ip_ss.xy], 1, frag_cnt);
    //uint newi_vis;
    //int addr = addr_base; // + frag_cnt;
    //deepbuf_vis[addr] = __ConvertFloat4ToUInt(float4(0, 0, 1, 1));
    //InterlockedExchange(deepbuf_vis[addr], __ConvertFloat4ToUInt(float4(1, 1, 1, 1)), iv_rgba);
    //InterlockedExchange(deepbuf_zdepth[addr], asuint(0.12345f), iv_rgba);
    //InterlockedExchange(deepbuf_zthick[addr], asuint(1000.777f), iv_rgba);
    //return;
    ////deepbuf_zthick[17] = 1717;
    ////deepbuf_zdepth[17] = 1718.123f;
    //
    ////fragment_counter[pos_ip_ss.xy] = 30;
    //
    ////RWTexture2D<uint> fragment_counter : register(u1);
    ////RWTexture2D<uint> fragment_spinlock : register(u2);
    ////RWBuffer<uint> deepbuf_vis : register(u3);
    ////RWBuffer<float> deepbuf_zdepth : register(u4);
    ////RWBuffer<uint> deepbuf_zthick : register(u5);
    //deepbuf_vis[addr] = 100;
    //deepbuf_zdepth[addr] = 100.234f;
    //deepbuf_zthick[addr] = 30000;
    //if (pos_ip_ss.x < 1)
    //    deepbuf_zthick[addr] = 10000;
    //return;// out_ps;
    //int cnt_test = 0;
    //bool keep_loop = true;
    //[allow_uav_condition][loop]
    //while (keep_loop)
    //{
    //    //if (cnt_test++ > 1000)
    //    //    break;
    //    uint spin_lock;
    //    // note that all of fragment_spinlock are initialized as zero
    //    InterlockedExchange(fragment_spinlock[pos_ip_ss.xy], 1, spin_lock);
    //    //InterlockedExchange(deepbuf_vis[addr_base], 1, spin_lock);
    //    if (spin_lock == 0)
    //    {
    //        fragment_spinlock[pos_ip_ss.xy] = 0;
    //        //InterlockedExchange(deepbuf_vis[addr_base], 0, spin_lock);
    //        deepbuf_vis[addr_base] = v_rgba;
    //        keep_loop = false;
    //        //break;
    //    } // critical section
    //} // while for spin-lock
    //return;


    //////////////////////


    // spin-lock //
    // to do : merging coplanar ones
    // coplanar ones 를 고려하지 않은 버전도 준비..
    const float ferr_sq = 0.0001f * 0.0001f; // to do : dynamic variable

#define SIMPLE_VER
//#define CHECK_COPLANARITY
#ifdef SIMPLE_VER
    const float z_thickness = 0.01f; // to do : dynamic determination
    uint frag_cnt, __dummy;
    InterlockedAdd(fragment_counter[pos_ip_ss.xy], 1, frag_cnt);
    if (frag_cnt < NUM_DEEP_LAYERS - 1) // to do : handle this as a variable
    {
        int addr = addr_base + frag_cnt;
        // note that 'frag_cnt' is atomically guaranteed
        // if the following is needed 'interlocked', then when frag_cnt == NUM_DEEP_LAYERS - 1
        InterlockedExchange(deepbuf_vis[addr], iv_rgba, __dummy);
        InterlockedExchange(deepbuf_zdepth[addr], asuint(z_depth), __dummy);
        InterlockedExchange(deepbuf_zthick[addr], asuint(z_thickness), __dummy);
        return;
    }
     
    // atomic routine (we are using spin-lock scheme)
    bool keep_loop = true;
    [allow_uav_condition][loop]
    while (keep_loop)
    {
        uint spin_lock = 0;
        // note that all of fragment_spinlock are initialized as zero
        InterlockedExchange(fragment_spinlock[pos_ip_ss.xy], 1, spin_lock);
        if (spin_lock == 0)
        {
            // critical section / z_depth
            float core_max_z = z_depth;
            int core_max_zidx = -1;
            [allow_uav_condition]//[loop]
            for (int i = 0; i < NUM_DEEP_LAYERS - 1; i++)
            {
                int addr = addr_base + i;
                float _z = deepbuf_zdepth[addr]; // after atomic store operation
                float z_diff = z_depth - _z;
                if (z_diff * z_diff < ferr_sq) // check the coplanarity
                {
                    core_max_zidx = NUM_DEEP_LAYERS + i;
                    i = NUM_DEEP_LAYERS;
                    //break; ==> do not use this when using allow_uav_condition 
                }
                else if (core_max_z < _z)
                {
                    core_max_z = _z;
                    core_max_zidx = i;
                }
            }                
                
            //fragment_spinlock[pos_ip_ss.xy] = 0;
            InterlockedExchange(fragment_spinlock[pos_ip_ss.xy], 0, spin_lock);
            keep_loop = false;
            //break;
                
            if (core_max_zidx >= NUM_DEEP_LAYERS)
            {
                // coplanarity case, use same deepbuf_zdepth and deepbuf_zthick
                int zid = core_max_zidx - NUM_DEEP_LAYERS;
                int addr = addr_base + zid;
                float4 vis_prev = __ConvertUIntToFloat4(deepbuf_vis[addr]);
                uint new_vis = __ConvertFloat4ToUInt(__BlendFloat4AndFloat4(vis_prev, v_rgba));
                InterlockedExchange(deepbuf_vis[addr], new_vis, __dummy);
            }
            else if (core_max_zidx < 0)
            {
                // update the merging slot
                __UpdateTailSlot(addr_base, v_rgba, z_depth, z_thickness);
            }
            else
            {
                // replace the prev max one and then update max z slot
                int addr_prev_max = addr_base + core_max_zidx;
                
                // load
                float4 vis_prev_max = __ConvertUIntToFloat4(deepbuf_vis[addr_prev_max]);
                float zdepth_prev_max = deepbuf_zdepth[addr_prev_max];
                float zthick_prev_max = deepbuf_zthick[addr_prev_max];
                
                // replace
                InterlockedExchange(deepbuf_vis[addr_prev_max], iv_rgba, __dummy);
                InterlockedExchange(deepbuf_zdepth[addr_prev_max], asuint(z_depth), __dummy);
                InterlockedExchange(deepbuf_zthick[addr_prev_max], asuint(z_thickness), __dummy);
                
                // update the merging slot
                __UpdateTailSlot(addr_base, vis_prev_max, zdepth_prev_max, zthick_prev_max);
            }
        } // critical section
    } // while for spin-lock
    //return out_ps;
#elif defined(CHECK_COPLANARITY)
    float max_z = z_depth;
    int max_zidx = -1;
    float fdummy;
    bool keepWaiting = true;
    int gg = 0;
    [allow_uav_condition]//[loop]
    while (keepWaiting)
    {
        // note that all of fragment_spinlock are initialized as zero
        uint spin_lock = 0;
        InterlockedExchange(fragment_spinlock[pos_ip_ss.xy], 1, spin_lock); // ==> RWM HAZZARD
        if (spin_lock == 0)
        {
            uint frag_cnt;
            InterlockedAdd(fragment_counter[pos_ip_ss.xy], 1, frag_cnt);
            uint dummyi;
            InterlockedExchange(fragment_spinlock[pos_ip_ss.xy], (uint) 0, dummyi);
        
            uint newi_vis = __ConvertFloat4ToUInt(float4(1,0,0,1));
            InterlockedExchange(deepbuf_vis[addr_base], newi_vis, newi_vis);
        
            keepWaiting = false;
        }
        else
        {
            if (gg++ > 100)
            {//특정 경우에 exchange 가 안 되는?! 예외 영역 처리?! float 오차?!
                keepWaiting = false;
                uint newi_vis = __ConvertFloat4ToUInt(float4(0, 1, 0, 1));
                InterlockedExchange(deepbuf_vis[addr_base], newi_vis, newi_vis);
            }
            // critical section
            [allow_uav_condition][loop]
            for (int i = 0; i < NUM_DEEP_LAYERS; i++)
            {
                int addr = addr_base + i;
                float _z = deepbuf_zdepth[addr]; // enough delay?!
                if (_z > 0)
                {
                    float z_diff = z_depth - _z;
                    if (z_diff * z_diff < ferr_sq) // check the coplanarity
                    {
                        max_zidx = NUM_DEEP_LAYERS + i;
                        break;
                    }
                    else if (max_z < _z)
                    {
                        max_z = _z;
                        max_zidx = i;
                    }
                }
                //if (_z < 0) // not assigned 
                //if (_z > 10000000) // not assigned 
                //    break;
                //float z_diff = z_depth - _z;
                //if (z_diff * z_diff < ferr_sq) // check the coplanarity
                //{
                //    max_zidx = NUM_DEEP_LAYERS + i;
                //    break;
                //}
                //else if (max_z < _z)
                //{
                //    max_z = _z;
                //    max_zidx = i;
                //}
            }
            
            if (max_zidx >= NUM_DEEP_LAYERS) // core-merging case
            {
                int offset = max_zidx - NUM_DEEP_LAYERS;
                int addr = addr_base + i;
                float4 new_vis = __BlendFloat4AndFloat4(v_rgba, __ConvertUIntToFloat4(deepbuf_vis[addr]));
                uint newi_vis = __ConvertFloat4ToUInt(new_vis);
                InterlockedExchange(deepbuf_vis[addr], newi_vis, newi_vis);
            }
            else 
            {
                uint iv_rgba = __ConvertFloat4ToUInt(v_rgba);
                uint frag_cnt;
                InterlockedAdd(fragment_counter[pos_ip_ss.xy], 1, frag_cnt);
                if (frag_cnt < NUM_DEEP_LAYERS) // core-filling case, to do : handle this as a variable
                {
                    int addr = addr_base + frag_cnt;
                    // note that 'frag_cnt' is atomically guaranteed
                    // if the following is needed 'interlocked', then when frag_cnt == 7
                    //deepbuf_vis[addr] = iv_rgba;
                    //deepbuf_zdepth[addr] = z_depth;
                    //deepbuf_zthick[addr] = 0.001f; // to do : dynamic determination
                    InterlockedExchange(deepbuf_vis[addr], iv_rgba, iv_rgba);
                    InterlockedExchange(deepbuf_zdepth[addr], asuint(z_depth), iv_rgba); // asuint
                    InterlockedExchange(deepbuf_zthick[addr], asuint(0.001f), iv_rgba);
                }
                else // tail case
                {
                    int addr_tail = addr_base + NUM_DEEP_LAYERS - 1;
                    if (max_zidx < 0) // incoming one moves to tail
                    {
                        __MergeTailSlot(v_rgba, addr_tail, z_depth);
                    }
                    else // incoming one moves to core and update tail with the previous core one
                    {
                        int addr_core = addr_base + max_zidx;
                        
                        uint vis_prev = deepbuf_vis[addr_core];
                        float z_prev = deepbuf_zdepth[addr_core];
                        //float z_thick_prev = deepbuf_zthick[addr_core];
                        InterlockedExchange(deepbuf_vis[addr_core], iv_rgba, iv_rgba);
                        InterlockedExchange(deepbuf_zdepth[addr_core], asuint(z_depth), iv_rgba); // asuint
                        InterlockedExchange(deepbuf_zthick[addr_core], asuint(0.001f), iv_rgba);
            
                        __MergeTailSlot(__ConvertUIntToFloat4(vis_prev), addr_tail, z_prev);
                    } // if (max_zidx < 0)
                } // if (frag_cnt < NUM_DEEP_LAYERS)
            } // if (max_zidx >= NUM_DEEP_LAYERS)
            //GroupMemoryBarrier();
            uint dummyi;
            InterlockedExchange(fragment_spinlock[pos_ip_ss.xy], (uint) 0, dummyi);
            keepWaiting = false;
        } // critical section
    } // while for spin-lock
    
    //return out_ps;
#else    
    const float z_thickness = 0.01f; // to do : dynamic determination
    uint frag_cnt;
    InterlockedAdd(fragment_counter[pos_ip_ss.xy], 1, frag_cnt);
    if (frag_cnt < NUM_DEEP_LAYERS) // to do : handle this as a variable
    {
        uint __dummy;
        int addr = addr_base + frag_cnt;
        // note that 'frag_cnt' is atomically guaranteed
        // if the following is needed 'interlocked', then when frag_cnt == NUM_DEEP_LAYERS - 1
        //deepbuf_vis[addr] = iv_rgba;
        InterlockedExchange(deepbuf_vis[addr], iv_rgba, __dummy);
        //deepbuf_zdepth[addr] = z_depth;
        InterlockedExchange(deepbuf_zdepth[addr], asuint(z_depth), __dummy);
        //deepbuf_zthick[addr] = z_thickness; // to do : dynamic determination
        InterlockedExchange(deepbuf_zthick[addr], asuint(z_thickness), __dummy);
        //return;
    }
    else
    {
        // atomic routine (we are using spin-lock scheme)
        bool keep_loop = true;
        [allow_uav_condition][loop]
        while (keep_loop)
        {
            uint spin_lock;
            // note that all of fragment_spinlock are initialized as zero
            InterlockedExchange(fragment_spinlock[pos_ip_ss.xy], 1, spin_lock);
            if (spin_lock == 0)
            {
                // critical section / z_depth
                float merge_slot_z = 0;
                int merge_slot_idx = -1;
                float prev_max_z = 0;
                int prev_max_zidx = -1;
                [allow_uav_condition][loop]
                for (int i = 0; i < NUM_DEEP_LAYERS; i++)
                {
                    int addr = addr_base + i;
                    float _z = deepbuf_zdepth[addr]; // enough delay?!
                    float z_diff = z_depth - _z;
                    if (z_diff * z_diff < ferr_sq) // check the coplanarity
                    {
                        merge_slot_idx = NUM_DEEP_LAYERS + i;
                        i = NUM_DEEP_LAYERS;
                        //break; ==> do not use this when using allow_uav_condition 
                    }
                    else if (merge_slot_z < _z)
                    {
                        merge_slot_z = _z;
                        merge_slot_idx = i;
                    }
        
                    if (prev_max_z < _z && _z < merge_slot_z)
                    {
                        prev_max_z = _z;
                        prev_max_zidx = i;
                    }
                }                
                
                //fragment_spinlock[pos_ip_ss.xy] = 0;
                InterlockedExchange(fragment_spinlock[pos_ip_ss.xy], 0, spin_lock);
                keep_loop = false;
                //break;
                
                uint __dummy;
                if (merge_slot_idx >= NUM_DEEP_LAYERS)
                {
                    // coplanarity case, use same deepbuf_zdepth and deepbuf_zthick
                    int offset = merge_slot_idx - NUM_DEEP_LAYERS;
                    int addr = addr_base + offset;
                    float4 vis_prev = __ConvertUIntToFloat4(deepbuf_vis[addr]);
                    uint new_vis = __ConvertFloat4ToUInt(__BlendFloat4AndFloat4(vis_prev, v_rgba));
                    //deepbuf_vis[addr] = new_vis;
                    InterlockedExchange(deepbuf_vis[addr], new_vis, __dummy);
                }
                else if (z_depth < prev_max_z)
                {
                    // replace the prev max one and then update max z slot
                    int addr_merge_slot = addr_base + merge_slot_idx;
                    int addr_prev_max = addr_base + prev_max_zidx;
                    uint vi1 = deepbuf_vis[addr_merge_slot];
                    uint vi2 = deepbuf_vis[addr_prev_max];
                    float4 vis_prev_merge = __ConvertUIntToFloat4(vi1); // w/ prev_maxz ===> thickness buffer model 나중에 적용
                    float4 vis_prev_max = __ConvertUIntToFloat4(vi2);
                    float4 vis_blend = __BlendFloat4AndFloat4(vis_prev_merge, vis_prev_max);
                    uint new_vis = __ConvertFloat4ToUInt(vis_blend);
                
                    float zthick_prev_max = deepbuf_zthick[addr_prev_max];
                    float zthick_merge_slot = deepbuf_zthick[addr_merge_slot];
                    float zfront = min(merge_slot_z - zthick_merge_slot, prev_max_z - zthick_prev_max);
                
                    uint __dummy1, __dummy2, __dummy3, __dummy4, __dummy5;
                    //deepbuf_vis[addr_merge_slot] = new_vis;
                    InterlockedExchange(deepbuf_vis[addr_merge_slot], new_vis, __dummy1);
                    //deepbuf_zthick[addr_merge_slot] = merge_slot_z - zfront;
                    InterlockedExchange(deepbuf_zthick[addr_merge_slot], asuint(merge_slot_z - zfront), __dummy2);
                    //deepbuf_vis[addr_prev_max] = iv_rgba;
                    InterlockedExchange(deepbuf_vis[addr_prev_max], iv_rgba, __dummy3);
                    //deepbuf_zdepth[addr_prev_max] = z_depth;
                    InterlockedExchange(deepbuf_zdepth[addr_prev_max], asuint(z_depth), __dummy4);
                    //deepbuf_zthick[addr_prev_max] = z_thickness; // to do : dynamic determination
                    InterlockedExchange(deepbuf_zthick[addr_prev_max], asuint(z_thickness), __dummy5);
                }
                else //if (z_depth >= prev_max_z)
                {
                    // update the merging slot
                    int addr_merge_slot = addr_base + merge_slot_idx;
                    uint vi1 = deepbuf_vis[addr_merge_slot];
                    float4 vis_prev_merge = __ConvertUIntToFloat4(vi1); // w/ prev_maxz ===> thickness buffer model 나중에 적용
                    uint new_vis = vi1;//__ConvertFloat4ToUInt(__BlendFloat4AndFloat4(vis_prev_merge, v_rgba));
                
                    float zthick_merge_slot = deepbuf_zthick[addr_merge_slot];
                    float new_back_z = max(z_depth, merge_slot_z);
                    float new_front_z = min(z_depth - z_thickness, merge_slot_z - zthick_merge_slot);

                    //deepbuf_vis[addr_merge_slot] = new_vis;
                    InterlockedExchange(deepbuf_vis[addr_merge_slot], new_vis, __dummy);
                    //deepbuf_zdepth[addr_merge_slot] = new_back_z;
                    InterlockedExchange(deepbuf_zdepth[addr_merge_slot], asuint(new_back_z), __dummy);
                    //deepbuf_zthick[addr_merge_slot] = new_back_z - new_front_z;
                    InterlockedExchange(deepbuf_zthick[addr_merge_slot], asuint(new_back_z - new_front_z), __dummy);
                }
            } // critical section
        } // while for spin-lock
    } // if (frag_cnt < NUM_DEEP_LAYERS)
    //return out_ps;
#endif
    //fragment_layers[pos_ip_ss.y * g_cbProxy.uiGridOffsetX + pos_ip_ss.x];
    //max_depth_indicator[pos_ip_ss.y * g_cbProxy.uiGridOffsetX + pos_ip_ss.x].max_index = (int)z_depth;
}

void OIT_PHONG(VS_OUTPUT input)
{
    if (g_cbPolygonObj.iFlag & 0x1)
    {
        if (dot(g_cbPolygonObj.f3VecClipPlaneWS, input.f3PosWS - g_cbPolygonObj.f3PosClipPlaneWS) > 0)
            return;
    }
    if (g_cbPolygonObj.iFlag & 0x2)
    {
        if (!vxIsInsideClipBox(input.f3PosWS, g_cbPolygonObj.f3ClipBoxMaxBS, g_cbPolygonObj.matClipBoxWS2BS))
            return;
    }

    float3 pos_ip_ss = float3(input.f4PosSS.xy, 0.0f);
    float3 pos_ip_ws = vxTransformPoint(pos_ip_ss, g_cbCamState.matSS2WS);

    float3 vec_pos_ip2frag = input.f3PosWS - pos_ip_ws;
    float z_depth = length(vec_pos_ip2frag);
    if (dot(vec_pos_ip2frag, g_cbCamState.f3VecViewWS) < 0)
        z_depth *= -1.f;

    
    float4 shading_factors = g_cbPolygonObj.f4ShadingFactor;
    float3 light_dirinv = -g_cbCamState.f3VecLightWS;
    if (g_cbCamState.iFlag & 0x2)
        light_dirinv = -normalize(input.f3PosWS - g_cbCamState.f3PosLightWS);
    float nor_len = length(input.f3VecNormalWS);
    float diff = 0;
    if (nor_len > 0)
        diff = max(dot(light_dirinv, input.f3VecNormalWS / nor_len) * g_cbPolygonObj.fShadingMultiplier, 0);

    float4 v_rgba;
#ifdef PS_COLOR_GLOBAL
	v_rgba.rgb = (shading_factors.x + shading_factors.y*diff + shading_factors.z*pow(diff, abs(shading_factors.w)))*g_cbPolygonObj.f4Color.rgb;
	//v_rgba.rgb = float3(1, 0, 0);
#else
    v_rgba.rgb = (shading_factors.x + shading_factors.y * diff + shading_factors.z * pow(diff, abs(shading_factors.w))) * input.f3Custom;
#endif
	//v_rgb.rgb = float3(1, 0, 0);
    v_rgba.a = g_cbPolygonObj.f4Color.a;
}
/**/