#include "CommonShader.hlsl"

Buffer<float4> g_f4bufOTF : register(t0); // Mask OTFs StructuredBuffer
Texture3D g_tex3DVolume : register(t1);
Texture3D g_tex3DBlock : register(t2); // For Deviation

Texture2DArray g_texRgbaArray : register(t3); // for cmm text and ply textures

Texture2D g_tex2D_Mat_KA : register(t10);
Texture2D g_tex2D_Mat_KD : register(t11);
Texture2D g_tex2D_Mat_KS : register(t12);
Texture2D g_tex2D_Mat_NS : register(t13);
Texture2D g_tex2D_Mat_BUMP : register(t14);
Texture2D g_tex2D_Mat_D : register(t15);

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

    //float4 f4Color : SV_TARGET0; // UNORM
    //float fDepthCS : SV_TARGET1;
};

VS_OUTPUT CommonVS_P(float3 f3Pos : POSITION)
{
    VS_OUTPUT vout = (VS_OUTPUT) 0;
    vout.f4PosSS = mul(float4(f3Pos, 1.f), g_cbPobj.mat_os2ps);
    vout.f3PosWS = TransformPoint(f3Pos, g_cbPobj.mat_os2ws);
    vout.f3VecNormalWS = (float3) 0;
    vout.f3Custom = (float3) 0;//g_cbPobj.fcolor.rgb;
    //vout.f4PosSS.z -= g_cbPobj.depth_forward_bias;
    return vout;
}

VS_OUTPUT CommonVS_PN(VS_INPUT_PN input)
{
    VS_OUTPUT vout = (VS_OUTPUT) 0;
    vout.f4PosSS = mul(float4(input.f3PosOS, 1.f), g_cbPobj.mat_os2ps);
    vout.f3PosWS = TransformPoint(input.f3PosOS, g_cbPobj.mat_os2ws);
    vout.f3VecNormalWS = normalize(TransformVector(input.f3VecNormalOS, g_cbPobj.mat_os2ws));
    vout.f3Custom = (float3) 0; //g_cbPobj.fcolor.rgb;
    //vout.f4PosSS.z -= g_cbPobj.depth_forward_bias;
    return vout;
}

VS_OUTPUT CommonVS_PT(VS_INPUT_PT input)
{
    VS_OUTPUT vout = (VS_OUTPUT) 0;
    vout.f4PosSS = mul(float4(input.f3PosOS, 1.f), g_cbPobj.mat_os2ps);
    vout.f3PosWS = TransformPoint(input.f3PosOS, g_cbPobj.mat_os2ws);
    vout.f3VecNormalWS = (float3) 0;
   // if (g_cbPobj.pobj_flag & (0x1 << 3))
   //     vout.f3Custom = (float3) 0; //g_cbPobj.fcolor.rgb;
   // else
        vout.f3Custom = input.f3Custom;
    //vout.f4PosSS.z -= g_cbPobj.depth_forward_bias;
    return vout;
}

VS_OUTPUT CommonVS_PNT(VS_INPUT_PNT input)
{
    VS_OUTPUT vout = (VS_OUTPUT) 0;
    vout.f4PosSS = mul(float4(input.f3PosOS, 1.f), g_cbPobj.mat_os2ps);
    vout.f3PosWS = TransformPoint(input.f3PosOS, g_cbPobj.mat_os2ws);
    vout.f3VecNormalWS = normalize(TransformVector(input.f3VecNormalOS, g_cbPobj.mat_os2ws));
    //if (g_cbPobj.pobj_flag & (0x1 << 3))
    //    vout.f3Custom = g_cbPobj.fcolor.rgb;
    //else
        vout.f3Custom = input.f3Custom;
    //vout.f4PosSS.z -= g_cbPobj.depth_forward_bias;
    return vout;
}

VS_OUTPUT_TTT CommonVS_PTTT(VS_INPUT_PTTT input)
{
    VS_OUTPUT_TTT vout = (VS_OUTPUT_TTT) 0;
    vout.f4PosSS = mul(float4(input.f3PosOS, 1.f), g_cbPobj.mat_os2ps);
    vout.f3PosWS = TransformPoint(input.f3PosOS, g_cbPobj.mat_os2ws);
    vout.f3Custom0 = input.f3Custom0;
    vout.f3Custom1 = TransformPerspVector(input.f3Custom1, g_cbPobj.mat_os2ps);
    vout.f3Custom2 = TransformPerspVector(input.f3Custom2, g_cbPobj.mat_os2ps);
    vout.f3Custom1.y *= -1;
    vout.f3Custom2.y *= -1;
    return vout;
}

#define __InterlockedExchange(A, B, C) A = B
//#define __InterlockedExchange(A, B, C) InterlockedExchange(A, B, C)

#define POBJ_PRE_CONTEXT(EXIT_OUT) \
    if (g_cbClipInfo.clip_flag & 0x1)\
    {\
        if (dot(g_cbClipInfo.vec_clipplane, input.f3PosWS - g_cbClipInfo.pos_clipplane) > 0)\
            EXIT_OUT;\
    }\
    if (g_cbClipInfo.clip_flag & 0x2)\
    {\
        if (!IsInsideClipBox(input.f3PosWS, g_cbClipInfo.pos_clipbox_max_bs, g_cbClipInfo.mat_clipbox_ws2bs))\
            EXIT_OUT;\
    }\
    float3 pos_ip_ss = float3(input.f4PosSS.xy, 0.0f);\
    float3 pos_ip_ws = TransformPoint(pos_ip_ss, g_cbCamState.mat_ss2ws);\
    float3 vec_pos_ip2frag = input.f3PosWS - pos_ip_ws;\
    float z_depth = length(vec_pos_ip2frag) /*- g_cbPobj.depth_forward_bias*/;\
    if (dot(vec_pos_ip2frag, g_cbCamState.dir_view_ws) < 0)\
        EXIT_OUT;\
    float3 view_dir = g_cbCamState.dir_view_ws;\
    if (g_cbCamState.cam_flag & 0x1)\
        view_dir = pos_ip_ws - g_cbCamState.pos_cam_ws;\
    view_dir = normalize(view_dir);

// POBJ_PRE_CONTEXT(EXIT_OUT)
RWTexture2D<uint> fragment_counter : register(u2);
RWTexture2D<uint> fragment_spinlock : register(u3);
RWBuffer<uint> deep_k_buf : register(u4);
RWBuffer<uint> deep_prefix_LL_buf : register(u5);

void UpdateTailSlot(const in uint addr_base, const in uint num_deep_layers, const in float4 vis_in, const in float alphaw, const in float zdepth_in, const in float zthick_in)
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
    
    __InterlockedExchange(deep_k_buf[addr_merge_slot + 0], i_V_sum, __dummy);
    __InterlockedExchange(deep_k_buf[addr_merge_slot + 1], i_depth, __dummy);
    __InterlockedExchange(deep_k_buf[addr_merge_slot + 2], i_tc, __dummy);
}

//void UpdateTailSlot1(const in uint addr_base, const in uint num_deep_layers, const in float4 vis_in, const in float zdepth_in, const in float zthick_in)
//{
//    uint addr_merge_slot = addr_base + (num_deep_layers - 1);
//    uint i_depth_blend = 0;
//    uint i_thick_blend = 0;
//    
//    uint addr_merge_slot_vis = (addr_base / num_deep_layers) * (num_deep_layers + 4) + (num_deep_layers - 1);
//    uint i_vis_blend_r = 0;
//    uint i_vis_blend_g = 0;
//    uint i_vis_blend_b = 0;
//    uint i_vis_blend_a_sum = 0;
//    uint i_vis_blend_a_mult = 0;
//    uint i_vis_prev_merge_a_sum = deepbuf_vis[addr_merge_slot_vis + 0];
//    
//    if (i_vis_prev_merge_a_sum == 0)
//    {
//#if ALPHA_SUM_MODE == 1
//        i_vis_blend_a_sum = asuint(vis_in.a);
//        i_vis_blend_r = ConvertFloat4ToUInt(vis_in);
//        //i_vis_blend_a_mult = asuint(1.f - vis_in.a);
//        i_depth_blend = asuint(zdepth_in);
//        i_thick_blend = asuint(zthick_in);
//#else
//        i_vis_blend_a_sum = asuint(vis_in.a);
//        i_vis_blend_r = asuint(vis_in.r);
//        i_vis_blend_g = asuint(vis_in.g);
//        i_vis_blend_b = asuint(vis_in.b);
//        i_vis_blend_a_mult = asuint(1.f - vis_in.a);
//        i_depth_blend = asuint(zdepth_in);
//        i_thick_blend = asuint(zthick_in);
//#endif
//    }
//    else
//    {
//#if ALPHA_SUM_MODE == 1
//        float A_sum_prev = asfloat(i_vis_prev_merge_a_sum);
//        float4 C_sum_prev = ConvertUIntToFloat4(deepbuf_vis[addr_merge_slot_vis + 1]);
//        float3 I_sum_prev = C_sum_prev.rgb / C_sum_prev.a;
//
//        float A_sum = A_sum_prev + vis_in.a;
//        float3 C_sum = I_sum_prev * A_sum_prev + vis_in.rgb; // consider associated color
//        float3 I_sum = C_sum / A_sum;
//        float A_ = 1 - (1 - C_sum_prev.a) * (1.f - vis_in.a);
//        
//        i_vis_blend_a_sum = asuint(A_sum);
//        //i_vis_blend_r = ConvertFloat4ToUInt(float4(1, 0, 0, 1));
//        i_vis_blend_r = ConvertFloat4ToUInt(float4(I_sum * A_, A_));
//        //i_vis_blend_a_mult = asuint(T_mult);
//                
//        float zdepth_prev_merge = asfloat(deepbuf_zdepth[addr_merge_slot]);
//        float zthick_prev_merge = asfloat(deepbuf_zthick[addr_merge_slot]);
//        float new_back_z = zdepth_in, new_front_z = zthick_in;
//        if (zthick_prev_merge > 0)
//        {
//            new_back_z = max(zdepth_in, zdepth_prev_merge);
//            new_front_z = min(zdepth_in - zthick_in, zdepth_prev_merge - zthick_prev_merge);
//        }
//        i_depth_blend = asuint(new_back_z);
//        i_thick_blend = asuint(new_back_z - new_front_z);
//#else
//        uint i_vis_prev_merge_r = deepbuf_vis[addr_merge_slot_vis + 1];
//        uint i_vis_prev_merge_g = deepbuf_vis[addr_merge_slot_vis + 2];
//        uint i_vis_prev_merge_b = deepbuf_vis[addr_merge_slot_vis + 3];
//        uint i_vis_prev_merge_a_mult = deepbuf_vis[addr_merge_slot_vis + 4];
//
//        float4 vis_prev_merge = float4(asfloat(i_vis_prev_merge_r), asfloat(i_vis_prev_merge_g), asfloat(i_vis_prev_merge_b), asfloat(i_vis_prev_merge_a_sum));
//        float4 vis_blend = vis_prev_merge + vis_in; // consider associated color
//        i_vis_blend_a_sum = asuint(vis_blend.a);
//        i_vis_blend_r = asuint(vis_blend.r);
//        i_vis_blend_g = asuint(vis_blend.g);
//        i_vis_blend_b = asuint(vis_blend.b);
//        i_vis_blend_a_mult = asuint(asfloat(i_vis_prev_merge_a_mult) * (1.f - vis_in.a));
//                
//        float zdepth_prev_merge = asfloat(deepbuf_zdepth[addr_merge_slot]);
//        float zthick_prev_merge = asfloat(deepbuf_zthick[addr_merge_slot]);
//        float new_back_z = zdepth_in, new_front_z = zthick_in;
//        if (zthick_prev_merge > 0)
//        {
//            new_back_z = max(zdepth_in, zdepth_prev_merge);
//            new_front_z = min(zdepth_in - zthick_in, zdepth_prev_merge - zthick_prev_merge);
//        }
//        i_depth_blend = asuint(new_back_z);
//        i_thick_blend = asuint(new_back_z - new_front_z);
//#endif
//    }
//
//#if __INTERLOCK == 0
//    deepbuf_vis[addr_merge_slot_vis + 0] = i_vis_blend_a_sum;
//    deepbuf_vis[addr_merge_slot_vis + 1] = i_vis_blend_r;
//    deepbuf_vis[addr_merge_slot_vis + 2] = i_vis_blend_g;
//    deepbuf_vis[addr_merge_slot_vis + 3] = i_vis_blend_b;
//    deepbuf_vis[addr_merge_slot_vis + 4] = i_vis_blend_a_mult;
//    deepbuf_zdepth[addr_merge_slot] = i_depth_blend;
//    deepbuf_zthick[addr_merge_slot] = i_thick_blend;
//#else
//    uint __dummy;
//    __InterlockedExchange(deepbuf_vis[addr_merge_slot_vis + 0], i_vis_blend_a_sum, __dummy);
//    __InterlockedExchange(deepbuf_vis[addr_merge_slot_vis + 1], i_vis_blend_r, __dummy);
//    __InterlockedExchange(deepbuf_vis[addr_merge_slot_vis + 2], i_vis_blend_g, __dummy);
//    __InterlockedExchange(deepbuf_vis[addr_merge_slot_vis + 3], i_vis_blend_b, __dummy);
//    __InterlockedExchange(deepbuf_vis[addr_merge_slot_vis + 4], i_vis_blend_a_mult, __dummy);
//    
//    __InterlockedExchange(deepbuf_zdepth[addr_merge_slot], i_depth_blend, __dummy);
//    __InterlockedExchange(deepbuf_zthick[addr_merge_slot], i_thick_blend, __dummy);
//#endif
//}

Texture2D<float4> sr_fragment_vis : register(t10);
Texture2D<float> sr_fragment_zdepth : register(t11);

float4 OutlineTest(const in int2 tex2d_xy, const in float depth_c, const in float discont_depth_criterion)
{
    if (depth_c > 1000000)
        return (float4) 0;
    
	const int thick = 2;
    float depth_h0 = sr_fragment_zdepth[tex2d_xy.xy + int2(0, thick)].r;
    float depth_h1 = sr_fragment_zdepth[tex2d_xy.xy - int2(0, thick)].r;
    float depth_w0 = sr_fragment_zdepth[tex2d_xy.xy + int2(thick, 0)].r;
    float depth_w1 = sr_fragment_zdepth[tex2d_xy.xy - int2(thick, 0)].r;
    
	const float outline_thres = discont_depth_criterion;
    float depth_min = min(min(depth_h0, depth_h1), min(depth_w0, depth_w1));
    //if (abs(depth_c - depth_min) > outline_thres * 1)
    //    return (float4)0;
    //
    float diff_max = max(abs(depth_h0 - depth_h1), abs(depth_w0 - depth_w1));
    //if (diff_max < outline_thres * 1)
    //    return (float4) 0;

	if (abs(diff_max - depth_min) < outline_thres) return (float4) 0;
        
    return float4(1, 0.0, 0, 1);
}

float3 PhongBlinn(const in float3 cam_view, const in float3 light_dirinv, float3 normal, const in bool is_max_shading,
    const in float3 Ka, const in float3 Kd, const in float3 Ks, const in float Ns)
{
    if (dot(cam_view, normal) > 0)
        normal *= -1.f;
    float diff = dot(light_dirinv, normal);
    if (is_max_shading)
        diff = max(diff, 0);
    else
        diff = abs(diff);
    float3 H = normalize(cam_view - light_dirinv);
    //float specular_power = Ns > 0 ? Ns : shading_factors.w;
    float specular = pow(abs(dot(H, normal)), Ns);
	//float3 factors = shading_factors.xyz; // Ns > 0 ? float3(0.3, 0.3, 0.3) : shading_factors.xyz;
    //return saturate(shading_factors.x * Ka + shading_factors.y * diff * Kd + shading_factors.z * specular * Ks);
	return saturate(Ka + diff * Kd + specular * Ks);
	//return diff;
}

void ComputeColor(inout float3 color_out, const in float3 Ka, const in float3 Kd, const in float3 Ks, const in float Ns, const in float bump, const in float3 pos_frag, const in float3 view_dir, const in float3 nrl, const in float nrl_len)
{
	if (nrl_len > 0)
	{
		float3 light_dirinv = -g_cbEnv.dir_light_ws;
		if (g_cbEnv.light_flag & 0x1)
			light_dirinv = -normalize(pos_frag - g_cbEnv.pos_light_ws);
		color_out = PhongBlinn(view_dir, light_dirinv, nrl / nrl_len * bump, g_cbPobj.pobj_flag & (0x1 << 5), Ka, Kd, Ks, Ns);
	}
	else// if ((g_cbPobj.pobj_flag & (0x1 << 3)) == 0)
	{
		color_out = Ka;
	}
	//else
	//{
	//	color_out = float3(1, 0, 0);// Ka;
	//}
}
void DashedLine(inout float4 v_rgba, inout float depthcs, const in float line_pos, const in float dash_interval, const in bool is_dashed, const in bool traparent_interval)
{
	// Parameter [fDashedDistance] Dashed Distance (WS)
	// Parameter [bIsDashed]
    if (is_dashed)
    {
        if ((uint) (line_pos / dash_interval) % 2 == 1)
        {
            if (traparent_interval)
            {
                v_rgba.rgb = (float3) 1.f - v_rgba.rgb;
            }
            else
            {
                v_rgba = (float4) 0;
                depthcs = FLT_MAX;
            }
        }
    }
}

void TextMapping(inout float4 v_rgba, inout float depthcs, float2 pos_sample, const in bool is_xflip, const in bool is_yflip)
{
	// Sample Flip State 0, 1, 2, 3
    if (is_xflip)
        pos_sample.x = 1.f - pos_sample.x;

    if (is_yflip)
        pos_sample.y = 1.f - pos_sample.y;

    float intensity = g_texRgbaArray.SampleLevel(g_samplerLinear, float3(pos_sample, 0), 0).r * v_rgba.a; // a single channl into rgba equally
    
    if (intensity == 0)
    {
        v_rgba = (float4) 0;
        depthcs = FLT_MAX;
    }
    else
    {
        v_rgba.a = intensity;
        v_rgba.rgb = v_rgba.rgb * intensity + (float3(1, 1, 1) - v_rgba.rgb) * (1.f - intensity);
    }
}

void TextureImgMap(inout float4 v_rgba, const float3 pos_sample)
{
    // always rgba
    // to do // .. mipmap
    //pos_sample.x = 1. - pos_sample.x;
    //pos_sample.y = 1. - pos_sample.y;
    //pos_sample.z = 1;
    v_rgba = g_texRgbaArray.SampleLevel(g_samplerLinear_wrap, pos_sample, 0).bgra;
    //if (img_rgba.a == 0)
    //{
    //    v_rgba = (float4) 0;
    //    depthcs = FLT_MAX;
    //}
    //else
    //{
    //    v_rgba.rgb = img_rgba.rgb;
    //    v_rgba.a *= img_rgba.a;
    //}
}
    
void TextureMaterialMap(inout float3 Ka, inout float3 Kd, inout float3 Ks, inout float Ns, inout float bump, inout float d, const float3 pos_sample, const uint tex_map_enum)
{
    //Texture2D g_tex2D_Mat_KA : register(t10);
    //Texture2D g_tex2D_Mat_KD : register(t11);
    //Texture2D g_tex2D_Mat_KS : register(t12);
    //Texture2D g_tex2D_Mat_NS : register(t13);
    //Texture2D g_tex2D_Mat_BUMP : register(t14);
    //Texture2D g_tex2D_Mat_D : register(t15);
    if (tex_map_enum & (0x1 << 1))
        Ka = g_tex2D_Mat_KA.SampleLevel(g_samplerLinear_wrap, pos_sample.xy, 0).bgr;
	if (tex_map_enum & (0x1 << 2))
		Kd = g_tex2D_Mat_KD.SampleLevel(g_samplerLinear_wrap, pos_sample.xy, 0).bgr;
    if (tex_map_enum & (0x1 << 3))
        Ks = g_tex2D_Mat_KS.SampleLevel(g_samplerLinear_wrap, pos_sample.xy, 0).bgr;
    if (tex_map_enum & (0x1 << 4))
        Ns = g_tex2D_Mat_NS.SampleLevel(g_samplerLinear_wrap, pos_sample.xy, 0).r;
	if (tex_map_enum & (0x1 << 5))
		bump = g_tex2D_Mat_BUMP.SampleLevel(g_samplerLinear_wrap, pos_sample.xy, 0).r;
    if (tex_map_enum & (0x1 << 6))
        d = g_tex2D_Mat_D.SampleLevel(g_samplerLinear_wrap, pos_sample.xy, 0).r;
}

void MultiTextMapping(inout float4 v_rgba, inout float depthcs, float2 pos_sample, const in int letter_idx, const in float3 vec_width_ps, const in float3 vec_height_ps)
{
    //MultiTextMapping(v_rgba, z_depth, input.f3Custom0.xy, (int) (input.f3Custom0.z + 0.5f), input.f3Custom1, input.f3Custom2);
    // Sample Flip State 0, 1, 2, 3

    if (dot(vec_width_ps, float3(1, 0, 0)) < 0)
        pos_sample.x = 1.f - pos_sample.x;
	
    float fNumLetters = (float) g_cbPobj.num_letters;
    float fHeightU = 1.f / fNumLetters;
    float fOffsetU = fHeightU * (float) letter_idx;
    float fLetterU = pos_sample.y / fNumLetters;

    float3 vec_normal = cross(vec_height_ps, vec_width_ps);
    float3 __vec_height_ps = cross(vec_width_ps, vec_normal);

    if (dot(__vec_height_ps, float3(0, 1, 0)) < 0)
        pos_sample.y = fOffsetU + (fHeightU - fLetterU);
    else
        pos_sample.y = fOffsetU + fLetterU;

    float intensity = g_texRgbaArray.SampleLevel(g_samplerLinear, float3(pos_sample, 0), 0).r * v_rgba.a; // a single channl into rgba equally
    //g_tex2DText[float2(pos_sample.x * 207, pos_sample.y * 300)];
    //g_tex2DText.SampleLevel(g_samplerLinear, pos_sample, 0);
    
    //f4ColorOut = float4(pos_sample, 1, 1);
    //f4ColorOut = float4(g_cbPobj.fcolor.rgb, 1);
	//return vxOut;

    if (intensity == 0)
    {
        v_rgba = (float4) 0;
        depthcs = FLT_MAX;
    }
    else
    {
        v_rgba.a = intensity;
        v_rgba.rgb = v_rgba.rgb * intensity + (float3(1, 1, 1) - v_rgba.rgb) * (1.f - intensity);
    }
}

struct PS_FILL_OUTPUT
{
    float4 color : SV_TARGET0; // UNORM
    float depthcs : SV_TARGET1;

    float ds_z : SV_Depth;
};

[earlydepthstencil]
PS_FILL_OUTPUT SINGLE_LAYER(VS_OUTPUT input)
{
    PS_FILL_OUTPUT out_ps;
    out_ps.ds_z = 1.f;
    out_ps.color = (float4)0;
    out_ps.depthcs = FLT_MAX;
    POBJ_PRE_CONTEXT(return out_ps)
    
    float4 v_rgba = float4(g_cbPobj.Kd, g_cbPobj.alpha);
    float3 light_dirinv = -g_cbEnv.dir_light_ws;
    if (g_cbEnv.light_flag & 0x1)
        light_dirinv = -normalize(input.f3PosWS - g_cbEnv.pos_light_ws);
    float3 nor = input.f3VecNormalWS;
    float nor_len = length(nor);
    float shade = 1.f;

	float3 Ka, Kd, Ks;
	float Ns = g_cbPobj.Ns;
	if ((g_cbPobj.pobj_flag & (0x1 << 3)) == 0)
	{
		float3 mat_shading = g_cbPobj.Ka;
		Ka = input.f3Custom * mat_shading.x;
		Kd = input.f3Custom * mat_shading.y;
		Ks = input.f3Custom * mat_shading.z;
	}
	else
	{
		Ka = g_cbPobj.Ka, Kd = g_cbPobj.Kd, Ks = g_cbPobj.Ks;
	}

	ComputeColor(v_rgba.rgb, Ka, Kd, Ks, Ns, 1.0, input.f3PosWS, view_dir, nor, nor_len);
    
    // make it as an associated color.
    // as a color component is stored into 8 bit channel, the alpha-multiplied precision must be determined in this stage.
    // unless, noise dots appear.
    v_rgba.rgb *= v_rgba.a;
    
    out_ps.ds_z = input.f4PosSS.z;
    out_ps.color = v_rgba;
    out_ps.depthcs = z_depth;
    return out_ps;
}

// geometry shader
// d3d10
static const float PI = 3.1415926f;
static const float fRatio = 2.0f;
static float fThickness = 0.01f;

[maxvertexcount(42)]
void GS_ThickPoints(point VS_OUTPUT input[1], inout TriangleStream<VS_OUTPUT> triangleStream)
{
    float4 positionPointTransformed = input[0].f4PosSS;

    const int nCountTriangles = 10;
    VS_OUTPUT output = input[0];
    const float pixel_thicknessX = g_cbPobj.pix_thickness / (float) g_cbCamState.rt_width * positionPointTransformed.w;
    const float pixel_thicknessY = g_cbPobj.pix_thickness / (float) g_cbCamState.rt_height * positionPointTransformed.w;
    for (int nI = 0; nI < nCountTriangles; ++nI)
    {
        output.f4PosSS = positionPointTransformed;
        output.f4PosSS.x += cos(0 + (2 * PI / nCountTriangles * nI)) * pixel_thicknessX;
        output.f4PosSS.y += sin(0 + (2 * PI / nCountTriangles * nI)) * pixel_thicknessY;
        //output.f4PosSS.z = 0.0f;
        //output.f4PosSS.w = 0.0f;
        //output.f4PosSS += positionPointTransformed;
        //output.f4PosSS *= positionPointTransformed.w;
        triangleStream.Append(output);

        output.f4PosSS = positionPointTransformed;
        triangleStream.Append(output);

        output.f4PosSS.x += cos(0 + (2 * PI / nCountTriangles * (nI + 1))) * pixel_thicknessX;
        output.f4PosSS.y += sin(0 + (2 * PI / nCountTriangles * (nI + 1))) * pixel_thicknessY;
        //output.f4PosSS.z = 0.0f;
        //output.f4PosSS.w = 0.0f;
        //output.f4PosSS += positionPointTransformed;
        //output.f4PosSS *= positionPointTransformed.w;
        triangleStream.Append(output);

        triangleStream.RestartStrip();
    }
}

void addHalfCircle(inout TriangleStream<VS_OUTPUT> triangleStream, VS_OUTPUT input, int nCountTriangles, float4 linePointToConnect, float fPointWComponent, float fAngle, float pixel_thicknessX, float pixel_thicknessY)
{
    // projection position : linePointToConnect

    VS_OUTPUT output = input;
    for (int nI = 0; nI < nCountTriangles; ++nI)
    {
        output.f4PosSS = linePointToConnect;
        output.f4PosSS.x += cos(fAngle + (PI / nCountTriangles * nI)) * pixel_thicknessX;
        output.f4PosSS.y += sin(fAngle + (PI / nCountTriangles * nI)) * pixel_thicknessY;
        //output.f4PosSS.z = 0.0f;
        //output.f4PosSS.w = 0.0f;
        //output.f4PosSS += linePointToConnect;
        output.f4PosSS *= fPointWComponent;
        triangleStream.Append(output);

        output.f4PosSS = linePointToConnect * fPointWComponent;
        triangleStream.Append(output);

        output.f4PosSS = linePointToConnect;
        output.f4PosSS.x += cos(fAngle + (PI / nCountTriangles * (nI + 1))) * pixel_thicknessX;
        output.f4PosSS.y += sin(fAngle + (PI / nCountTriangles * (nI + 1))) * pixel_thicknessY;
        //output.f4PosSS.z = 0.0f;
        //output.f4PosSS.w = 0.0f;
        //output.f4PosSS += linePointToConnect;
        output.f4PosSS *= fPointWComponent;
        triangleStream.Append(output);

        triangleStream.RestartStrip();
    }
}

void __GS_ThickLines(VS_OUTPUT input[2], inout TriangleStream<VS_OUTPUT> triangleStream)
{
    float4 positionPoint0Transformed = input[0].f4PosSS;
    float4 positionPoint1Transformed = input[1].f4PosSS;

    float fPoint0w = positionPoint0Transformed.w;
    float fPoint1w = positionPoint1Transformed.w;

    //calculate out the W parameter, because of usage of perspective rendering
    positionPoint0Transformed.xyz = positionPoint0Transformed.xyz / positionPoint0Transformed.w;
    positionPoint0Transformed.w = 1.0f;
    positionPoint1Transformed.xyz = positionPoint1Transformed.xyz / positionPoint1Transformed.w;
    positionPoint1Transformed.w = 1.0f;

    //calculate the angle between the 2 points on the screen
    float3 positionDifference = positionPoint0Transformed.xyz - positionPoint1Transformed.xyz;
    float3 coordinateSystem = float3(1.0f, 0.0f, 0.0f);
    
    positionDifference.z = 0.0f;
    coordinateSystem.z = 0.0f;
    
    float fAngle = acos(dot(positionDifference.xy, coordinateSystem.xy) / (length(positionDifference.xy) * length(coordinateSystem.xy)));
    
    if (cross(positionDifference, coordinateSystem).z < 0.0f)
    {
        fAngle = 2.0f * PI - fAngle;
    }
    
    fAngle *= -1.0f;
    fAngle -= PI * 0.5f;
    
    const float pixel_thicknessX = g_cbPobj.pix_thickness / (float) g_cbCamState.rt_width;
    const float pixel_thicknessY = g_cbPobj.pix_thickness / (float) g_cbCamState.rt_height;
    //first half circle of the line
    int nCountTriangles = 6;
    addHalfCircle(triangleStream, input[0], nCountTriangles, positionPoint0Transformed, fPoint0w, fAngle, pixel_thicknessX, pixel_thicknessY);
    addHalfCircle(triangleStream, input[1], nCountTriangles, positionPoint1Transformed, fPoint1w, fAngle + PI, pixel_thicknessX, pixel_thicknessY);

    //connection between the two circles
    //triangle1
    VS_OUTPUT output0 = input[0];
    VS_OUTPUT output1 = input[1];
    output0.f4PosSS = positionPoint0Transformed;
    output0.f4PosSS.x += cos(fAngle) * pixel_thicknessX;
    output0.f4PosSS.y += sin(fAngle) * pixel_thicknessY;
    output0.f4PosSS *= fPoint0w; //undo calculate out the W parameter, because of usage of perspective rendering
    triangleStream.Append(output0);
    
    output0.f4PosSS = positionPoint0Transformed;
    output0.f4PosSS.x += cos(fAngle + (PI / nCountTriangles * (nCountTriangles))) * pixel_thicknessX;
    output0.f4PosSS.y += sin(fAngle + (PI / nCountTriangles * (nCountTriangles))) * pixel_thicknessY;
    output0.f4PosSS *= fPoint0w;
    triangleStream.Append(output0);
    
    output1.f4PosSS = positionPoint1Transformed;
    output1.f4PosSS.x += cos(fAngle + (PI / nCountTriangles * (nCountTriangles))) * pixel_thicknessX;
    output1.f4PosSS.y += sin(fAngle + (PI / nCountTriangles * (nCountTriangles))) * pixel_thicknessY;
    output1.f4PosSS *= fPoint1w;
    triangleStream.Append(output1);

    //triangle2
    output0.f4PosSS = positionPoint0Transformed;
    output0.f4PosSS.x += cos(fAngle) * pixel_thicknessX;
    output0.f4PosSS.y += sin(fAngle) * pixel_thicknessY;
    output0.f4PosSS *= fPoint0w;
    triangleStream.Append(output0);
    
    output1.f4PosSS = positionPoint1Transformed;
    output1.f4PosSS.x += cos(fAngle) * pixel_thicknessX;
    output1.f4PosSS.y += sin(fAngle) * pixel_thicknessY;
    output1.f4PosSS *= fPoint1w;
    triangleStream.Append(output1);
    
    output1.f4PosSS = positionPoint1Transformed;
    output1.f4PosSS.x += cos(fAngle + (PI / nCountTriangles * (nCountTriangles))) * pixel_thicknessX;
    output1.f4PosSS.y += sin(fAngle + (PI / nCountTriangles * (nCountTriangles))) * pixel_thicknessY;
    output1.f4PosSS *= fPoint1w;
    triangleStream.Append(output1);
}

[maxvertexcount(42)]
void GS_ThickLines(line VS_OUTPUT input[2], inout TriangleStream<VS_OUTPUT> triangleStream)
{
    __GS_ThickLines(input, triangleStream);
}