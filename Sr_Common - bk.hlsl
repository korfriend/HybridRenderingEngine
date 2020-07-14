#include "CommonShader.hlsl"

Buffer<float4> g_f4bufOTF : register(t0); // Mask OTFs StructuredBuffer
Texture3D g_tex3DVolume : register(t1);
Texture3D g_tex3DBlock : register(t2); // For Deviation
Texture2D g_tex2DText : register(t3);

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
    vout.f4PosSS = mul(float4(f3Pos, 1.f), g_cbPobj.mat_os2ps);
    vout.f3PosWS = TransformPoint(f3Pos, g_cbPobj.mat_os2ws);
    vout.f3VecNormalWS = (float3) 0;
    vout.f3Custom = g_cbPobj.fcolor.rgb;
    vout.f4PosSS.z -= g_cbPobj.depth_forward_bias;
    return vout;
}

VS_OUTPUT CommonVS_PN(VS_INPUT_PN input)
{
    VS_OUTPUT vout = (VS_OUTPUT) 0;
    vout.f4PosSS = mul(float4(input.f3PosOS, 1.f), g_cbPobj.mat_os2ps);
    vout.f3PosWS = TransformPoint(input.f3PosOS, g_cbPobj.mat_os2ws);
    vout.f3VecNormalWS = normalize(TransformVector(input.f3VecNormalOS, g_cbPobj.mat_os2ws));
    vout.f3Custom = g_cbPobj.fcolor.rgb;
    vout.f4PosSS.z -= g_cbPobj.depth_forward_bias;
    return vout;
}

VS_OUTPUT CommonVS_PT(VS_INPUT_PT input)
{
    VS_OUTPUT vout = (VS_OUTPUT) 0;
    vout.f4PosSS = mul(float4(input.f3PosOS, 1.f), g_cbPobj.mat_os2ps);
    vout.f3PosWS = TransformPoint(input.f3PosOS, g_cbPobj.mat_os2ws);
    vout.f3VecNormalWS = (float3) 0;
    if (g_cbPobj.pobj_flag & (0x1 << 3))
        vout.f3Custom = g_cbPobj.fcolor.rgb;
    else
        vout.f3Custom = input.f3Custom;
    vout.f4PosSS.z -= g_cbPobj.depth_forward_bias;
    return vout;
}

VS_OUTPUT CommonVS_PNT(VS_INPUT_PNT input)
{
    VS_OUTPUT vout = (VS_OUTPUT) 0;
    vout.f4PosSS = mul(float4(input.f3PosOS, 1.f), g_cbPobj.mat_os2ps);
    vout.f3PosWS = TransformPoint(input.f3PosOS, g_cbPobj.mat_os2ws);
    vout.f3VecNormalWS = normalize(TransformVector(input.f3VecNormalOS, g_cbPobj.mat_os2ws));
    if (g_cbPobj.pobj_flag & (0x1 << 3))
        vout.f3Custom = g_cbPobj.fcolor.rgb;
    else
        vout.f3Custom = input.f3Custom;
    vout.f4PosSS.z -= g_cbPobj.depth_forward_bias;
    return vout;
}

VS_OUTPUT_TTT CommonVS_PTTT(VS_INPUT_PTTT input)
{
    VS_OUTPUT_TTT vout = (VS_OUTPUT_TTT) 0;
    vout.f4PosSS = mul(float4(input.f3PosOS, 1.f), g_cbPobj.mat_os2ps);
    vout.f3PosWS = TransformPoint(input.f3PosOS, g_cbPobj.mat_os2ws);
    vout.f3Custom0 = input.f3Custom0;
    vout.f3Custom1 = TransformVector(input.f3Custom1, g_cbPobj.mat_os2ps);
    vout.f3Custom2 = TransformVector(input.f3Custom2, g_cbPobj.mat_os2ps);
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
    float z_depth = length(vec_pos_ip2frag) - g_cbPobj.depth_forward_bias;\
    if (dot(vec_pos_ip2frag, g_cbCamState.dir_view_ws) < 0)\
        EXIT_OUT;\
    float3 view_dir = g_cbCamState.dir_view_ws;\
    if (g_cbCamState.cam_flag & 0x1)\
        view_dir = pos_ip_ws - g_cbCamState.pos_cam_ws;\
    view_dir = normalize(view_dir);

// POBJ_PRE_CONTEXT(EXIT_OUT)
RWTexture2D<uint> fragment_counter : register(u2);
RWTexture2D<uint> fragment_spinlock : register(u3);
RWBuffer<uint> deepbuf_vis : register(u4);
RWBuffer<uint> deepbuf_zdepth : register(u5);
RWBuffer<uint> deepbuf_zthick : register(u6);

void UpdateTailSlot(const in uint addr_base, const in uint num_deep_layers, const in float4 vis_in, const in float zdepth_in, const in float zthick_in)
{
    uint addr_merge_slot = addr_base + (num_deep_layers - 1);
    uint i_depth_blend = 0;
    uint i_thick_blend = 0;
    
#if __AVR_MERGE == 1
    uint addr_merge_slot_vis = (addr_base / num_deep_layers) * (num_deep_layers + 4) + (num_deep_layers - 1);
    uint i_vis_blend_r = 0;
    uint i_vis_blend_g = 0;
    uint i_vis_blend_b = 0;
    uint i_vis_blend_a_sum = 0;
    uint i_vis_blend_a_mult = 0;
    uint i_vis_prev_merge_a_sum = deepbuf_vis[addr_merge_slot_vis + 0];
    if (i_vis_prev_merge_a_sum == 0)
#else
    uint i_vis_blend = 0;
    uint i_vis_prev_merge = deepbuf_vis[addr_merge_slot];
    if (i_vis_prev_merge == 0)
#endif
    {
        
#if __AVR_MERGE == 1
        i_vis_blend_a_sum = asuint(vis_in.a);
        i_vis_blend_r = asuint(vis_in.r);
        i_vis_blend_g = asuint(vis_in.g);
        i_vis_blend_b = asuint(vis_in.b);
        i_vis_blend_a_mult = asuint(1.f - vis_in.a);
#else
        i_vis_blend = ConvertFloat4ToUInt(vis_in);
#endif
        i_depth_blend = asuint(zdepth_in);
        i_thick_blend = asuint(zthick_in);
    }
    else
    {
#if __AVR_MERGE == 1
        uint i_vis_prev_merge_r = deepbuf_vis[addr_merge_slot_vis + 1];
        uint i_vis_prev_merge_g = deepbuf_vis[addr_merge_slot_vis + 2];
        uint i_vis_prev_merge_b = deepbuf_vis[addr_merge_slot_vis + 3];
        uint i_vis_prev_merge_a_mult = deepbuf_vis[addr_merge_slot_vis + 4];

        float4 vis_prev_merge = float4(asfloat(i_vis_prev_merge_r), asfloat(i_vis_prev_merge_g), asfloat(i_vis_prev_merge_b), asfloat(i_vis_prev_merge_a_sum));
        float4 vis_blend = vis_prev_merge + vis_in; // consider associated color
        i_vis_blend_a_sum = asuint(vis_blend.a);
        i_vis_blend_r = asuint(vis_blend.r);
        i_vis_blend_g = asuint(vis_blend.g);
        i_vis_blend_b = asuint(vis_blend.b);
        i_vis_blend_a_mult = asuint(asfloat(i_vis_prev_merge_a_mult) * (1.f - vis_in.a));
#else
        float4 vis_prev_merge = ConvertUIntToFloat4(i_vis_prev_merge); 
        float4 vis_blend = BlendFloat4AndFloat4(vis_prev_merge, vis_in);
        i_vis_blend = ConvertFloat4ToUInt(vis_blend);
#endif
                
        float zdepth_prev_merge = asfloat(deepbuf_zdepth[addr_merge_slot]);
        float zthick_prev_merge = asfloat(deepbuf_zthick[addr_merge_slot]);
        float new_back_z = zdepth_in, new_front_z = zthick_in;
        if (zthick_prev_merge > 0)
        {
            new_back_z = max(zdepth_in, zdepth_prev_merge);
            new_front_z = min(zdepth_in - zthick_in, zdepth_prev_merge - zthick_prev_merge);
        }
        i_depth_blend = asuint(new_back_z);
        i_thick_blend = asuint(new_back_z - new_front_z);
    }

#if __INTERLOCK == 0
#if __AVR_MERGE == 1
    deepbuf_vis[addr_merge_slot_vis + 0] = i_vis_blend_a_sum;
    deepbuf_vis[addr_merge_slot_vis + 1] = i_vis_blend_r;
    deepbuf_vis[addr_merge_slot_vis + 2] = i_vis_blend_g;
    deepbuf_vis[addr_merge_slot_vis + 3] = i_vis_blend_b;
    deepbuf_vis[addr_merge_slot_vis + 4] = i_vis_blend_a_mult;
#else
    deepbuf_vis[addr_merge_slot] = i_vis_blend;
#endif
    deepbuf_zdepth[addr_merge_slot] = i_depth_blend;
    deepbuf_zthick[addr_merge_slot] = i_thick_blend;
#else
    uint __dummy;
#if __AVR_MERGE == 1
    __InterlockedExchange(deepbuf_vis[addr_merge_slot_vis + 0], i_vis_blend_a_sum, __dummy);
    __InterlockedExchange(deepbuf_vis[addr_merge_slot_vis + 1], i_vis_blend_r, __dummy);
    __InterlockedExchange(deepbuf_vis[addr_merge_slot_vis + 2], i_vis_blend_g, __dummy);
    __InterlockedExchange(deepbuf_vis[addr_merge_slot_vis + 3], i_vis_blend_b, __dummy);
    __InterlockedExchange(deepbuf_vis[addr_merge_slot_vis + 4], i_vis_blend_a_mult, __dummy);
#else
    __InterlockedExchange(deepbuf_vis[addr_merge_slot], i_vis_blend, __dummy);
#endif
    __InterlockedExchange(deepbuf_zdepth[addr_merge_slot], i_depth_blend, __dummy);
    __InterlockedExchange(deepbuf_zthick[addr_merge_slot], i_thick_blend, __dummy);
#endif
}

Texture2D<float4> sr_fragment_vis : register(t10);
Texture2D<float> sr_fragment_zdepth : register(t11);

float4 OutlineTest(const in int2 tex2d_xy, const in float depth_c)
{
    if (depth_c > 1000000)
        return (float4) 0;
    
    float depth_h0 = sr_fragment_zdepth[tex2d_xy.xy + int2(0, 1)].r;
    float depth_h1 = sr_fragment_zdepth[tex2d_xy.xy - int2(0, 1)].r;
    float depth_w0 = sr_fragment_zdepth[tex2d_xy.xy + int2(1, 0)].r;
    float depth_w1 = sr_fragment_zdepth[tex2d_xy.xy - int2(1, 0)].r;
    
    const float outline_thres = g_cbCamState.cam_vz_thickness; // test //
    float depth_min = min(min(depth_h0, depth_h1), min(depth_w0, depth_w1));
    if (depth_c - depth_min > outline_thres * 9)
        return (float4)0;
    
    float diff_max = max(abs(depth_h0 - depth_h1), abs(depth_w0 - depth_w1));
    if (diff_max < outline_thres * 10)
        return (float4) 0;
        
    return float4(0, 0, 0, 1);
}

float PhongBlinn(const in float4 shading_factors, const in float3 cam_view, const in float3 light_dirinv, float3 normal, const in bool is_max_shading)
{
    if (dot(cam_view, normal) > 0)
        normal *= -1.f;
    float diff = dot(light_dirinv, normal);
    if (is_max_shading)
        diff = max(diff, 0);
    else
        diff = abs(diff);
    float reft = pow(diff, shading_factors.w);
    return shading_factors.x + shading_factors.y * diff + shading_factors.z * reft;
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

    float intensity = g_tex2DText.SampleLevel(g_samplerLinear, pos_sample, 0).r * v_rgba.a; // a single channl into rgba equally
    
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

void MultiTextMapping(inout float4 v_rgba, inout float depthcs, float2 pos_sample, const in int letter_idx, const in float3 vec_width_ps, float3 vec_height_ps)
{
    // Sample Flip State 0, 1, 2, 3

    if (dot(vec_width_ps, float3(1, 0, 0)) < 0)
        pos_sample.x = 1.f - pos_sample.x;
	
    float fNumLetters = (float) g_cbPobj.num_letters;
    float fHeightU = 1.f / fNumLetters;
    float fOffsetU = fHeightU * (float) letter_idx;
    float fLetterU = pos_sample.y / fNumLetters;

    float3 vec_normal = cross(vec_height_ps, vec_width_ps);
    vec_height_ps = cross(vec_width_ps, vec_normal);

    if (dot(vec_height_ps, float3(0, 1, 0)) <= 0)
        pos_sample.y = fOffsetU + (fHeightU - fLetterU);
    else
        pos_sample.y = fOffsetU + fLetterU;

    float intensity = g_tex2DText.SampleLevel(g_samplerLinear, pos_sample, 0).r * v_rgba.a; // a single channl into rgba equally
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
    
    float4 v_rgba;
    float3 light_dirinv = -g_cbCamState.dir_light_ws;
    if (g_cbCamState.cam_flag & 0x2)
        light_dirinv = -normalize(input.f3PosWS - g_cbCamState.pos_light_ws);
    float3 nor = input.f3VecNormalWS;
    float nor_len = length(nor);
    float shade = 1.f;
    if (nor_len > 0)
        shade = PhongBlinn(g_cbRndEffect.pb_shading_factor, view_dir, light_dirinv, nor / nor_len, g_cbPobj.pobj_flag & (0x1 << 5));
    v_rgba.rgb = input.f3Custom * shade;
    v_rgba.a = g_cbPobj.fcolor.a;
    
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