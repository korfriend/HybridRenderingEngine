#include "CommonShader.hlsl"

Buffer<float4> g_f4bufOTF : register(t0); // Mask OTFs StructuredBuffer
Texture3D g_tex3DVolume : register(t1);
Texture3D tex3D_volblk : register(t2); // For Deviation

Texture2DArray g_texRgbaArray : register(t3); // for cmm text and ply textures

Texture2D g_tex2D_Mat_KA : register(t10);
Texture2D g_tex2D_Mat_KD : register(t11);
Texture2D g_tex2D_Mat_KS : register(t12);
Texture2D g_tex2D_Mat_NS : register(t13);
Texture2D g_tex2D_Mat_BUMP : register(t14);
Texture2D g_tex2D_Mat_D : register(t15);
// Paint texture slot (t60)
Texture2D g_tex2D_Paint : register(t60);

// Paint blend modes
#define PAINT_BLEND_NORMAL     0
#define PAINT_BLEND_MULTIPLY   1
#define PAINT_BLEND_ADDITIVE   2
#define PAINT_BLEND_OVERLAY    3
#define PAINT_BLEND_SOFT_LIGHT 4
#define PAINT_BLEND_SCREEN     5

Texture2D g_tex2D_shadowMap : register(t20);

Buffer<unorm float4> g_faceColors : register(t30); 

struct VS_INPUTS
{
    float3 f3PosOS : POSITION;
#if VSIN_N == 1
	float3 f3VecNormalOS : NORMAL;
#endif
#if VSIN_T == 1
	float2 f2UV : TEXCOORD0;
#endif
#if VSIN_C == 1
	float4 f4Color : COLOR0;
#endif
#if VSIN_G == 1
	float fDist : TEXCOORD1;
#endif
	uint vertexID : SV_VertexID; // vertex index
};

struct VS_INPUT_PTTT
{
    float3 f3PosOS : POSITION;
    float3 f3Custom0 : TEXCOORD0;
    float3 f3Custom1 : TEXCOORD1;
	float3 f3Custom2 : TEXCOORD2;
	uint vertexID : SV_VertexID; // vertex index
};

struct VS_OUTPUT
{
    float4 f4PosSS : SV_POSITION;
    float3 f3VecNormalWS : NORMAL;
	float3 f3PosWS : TEXCOORD0;
	float2 f2UV : TEXCOORD1;
	float4 f4Color : COLOR;
	float fDist : TEXCOORD2;
};

struct VS_OUTPUT_TTT
{
    float4 f4PosSS : SV_POSITION;
    float3 f3PosWS : TEXCOORD0;
    float3 f3Custom0 : TEXCOORD1;
    float3 f3Custom1 : TEXCOORD2;
    float3 f3Custom2 : TEXCOORD3;
};

struct PS_OUTPUT
{
    float fDepth : SV_Depth; // to avoid Clipping Early-Out-Ocllusion
};

VS_OUTPUT CommonVS(VS_INPUTS input)
{
	VS_OUTPUT vout = (VS_OUTPUT) 0;
	vout.f3PosWS = TransformPoint(input.f3PosOS, g_cbPobj.mat_os2ws);
	vout.f4PosSS = mul(g_cbCamState.mat_ws2ps_revZ, float4(vout.f3PosWS, 1.f)); // Reverse Z
    
#if VSIN_N == 1
	vout.f3VecNormalWS = normalize(TransformVector(input.f3VecNormalOS, g_cbPobj.mat_os2ws));
#endif
#if VSIN_T == 1
	vout.f2UV = input.f2UV;
#endif
#if VSIN_C == 1
	vout.f4Color = input.f4Color;
#endif
#if VSIN_G == 1
	vout.fDist = input.fDist;
#endif
    
	return vout;
}

// Quad VS: uses mat_os2ps directly (for PROXY_QUAD / full-screen passes)
VS_OUTPUT QuadVS(VS_INPUTS input)
{
	VS_OUTPUT vout = (VS_OUTPUT) 0;
	vout.f4PosSS = mul(float4(input.f3PosOS, 1.f), g_cbPobj.mat_os2ps);
	vout.f3PosWS = input.f3PosOS;
	return vout;
}

VS_OUTPUT_TTT CommonVS_PTTT(VS_INPUT_PTTT input)
{
    VS_OUTPUT_TTT vout = (VS_OUTPUT_TTT) 0;
    vout.f3PosWS = TransformPoint(input.f3PosOS, g_cbPobj.mat_os2ws);
    vout.f4PosSS = mul(g_cbCamState.mat_ws2ps_revZ, float4(vout.f3PosWS, 1.f)); // Reverse Z
    vout.f3Custom0 = input.f3Custom0;
    vout.f3Custom1 = TransformPerspVector(input.f3Custom1, g_cbPobj.mat_os2ps);
    vout.f3Custom2 = TransformPerspVector(input.f3Custom2, g_cbPobj.mat_os2ps);
    vout.f3Custom1.y *= -1;
    vout.f3Custom2.y *= -1;
    return vout;
}

#if __RENDERING_MODE == 2
#define __VS_OUT VS_OUTPUT_TTT
#else
#define __VS_OUT VS_OUTPUT
#endif

#define __InterlockedExchange(A, B, C) A = B
//#define __InterlockedExchange(A, B, C) InterlockedExchange(A, B, C)

#define POBJ_PRE_CONTEXT \
    if (g_cbPobj.alpha == 0) clip(-1);\
    if (g_cbClipInfo.clip_flag & 0x1)\
        clip(dot(g_cbClipInfo.vec_clipplane, input.f3PosWS - g_cbClipInfo.pos_clipplane) > 0 ? -1 : 1);\
    if (g_cbClipInfo.clip_flag & 0x2)\
        clip(!IsInsideClipBox(input.f3PosWS, g_cbClipInfo.mat_clipbox_ws2bs) ? -1 : 1);\
	clip(input.f4PosSS.z);\
	clip(input.f4PosSS.z/input.f4PosSS.w);\
    float3 pos_ip_ss = float3(input.f4PosSS.xy, 0.0f);\
    float3 pos_ip_ws = TransformPoint(pos_ip_ss, g_cbCamState.mat_ss2ws);\
    float3 vec_pos_ip2frag = input.f3PosWS - pos_ip_ws;\
    float z_depth = length(vec_pos_ip2frag) /*- g_cbPobj.depth_forward_bias*/;\
    clip(dot(vec_pos_ip2frag, g_cbCamState.dir_view_ws) < 0 ? -1 : 1);\
    float3 view_dir = g_cbCamState.dir_view_ws;\
    if (g_cbCamState.cam_flag & 0x1)\
        view_dir = pos_ip_ws - g_cbCamState.pos_cam_ws;\
    view_dir = normalize(view_dir);

// POBJ_PRE_CONTEXT(EXIT_OUT)

Texture2D<unorm float4> sr_fragment_vis : register(t10);
Texture2D<float> sr_fragment_zdepth : register(t11);
#if DX10_0 == 1
Texture2D<float> sr_fragment_zdepth_prev : register(t12);
#endif

float4 OutlineTest2(const in int2 tex2d_xy, inout float depth_c, const in float discont_depth_criterion)
{
    const float3 edge_color = float3(1, 0, 0);
    float4 vout = (float4) 0;
    if (depth_c < 1000000)
    {
        const int thick = 5;// 15;
        float depth_h0 = sr_fragment_zdepth[tex2d_xy.xy + int2(0, thick)].r;
        float depth_h1 = sr_fragment_zdepth[tex2d_xy.xy - int2(0, thick)].r;
        float depth_w0 = sr_fragment_zdepth[tex2d_xy.xy + int2(thick, 0)].r;
        float depth_w1 = sr_fragment_zdepth[tex2d_xy.xy - int2(thick, 0)].r;

        const float outline_thres = discont_depth_criterion;
        float depth_min = min(min(depth_h0, depth_h1), min(depth_w0, depth_w1));
        float depth_max = max(max(depth_h0, depth_h1), max(depth_w0, depth_w1));
        //if (abs(depth_c - depth_min) > outline_thres * 1)
        //    return (float4)0;
        //float diff_max = max(abs(depth_h0 - depth_h1), abs(depth_w0 - depth_w1));
        //if (diff_max < outline_thres * 1)
        //    return (float4) 0;
#if SILHOUETTE_EDGE == 1
        const int thick2 = 1;
        float3 nor_c = normalize(sr_fragment_vis[tex2d_xy.xy + int2(0, 0)].xyz * 2.f - (float3)1.f);
        float3 nor_h0 = normalize(sr_fragment_vis[tex2d_xy.xy + int2(0, thick2)].xyz * 2.f - (float3)1.f);
        float3 nor_h1 = normalize(sr_fragment_vis[tex2d_xy.xy - int2(0, thick2)].xyz * 2.f - (float3)1.f);
        float3 nor_w0 = normalize(sr_fragment_vis[tex2d_xy.xy + int2(thick2, 0)].xyz * 2.f - (float3)1.f);
        float3 nor_w1 = normalize(sr_fragment_vis[tex2d_xy.xy - int2(thick2, 0)].xyz * 2.f - (float3)1.f);
        //float dot_h = dot(nor_h0, nor_h1);
        //float dot_v = dot(nor_w0, nor_w1);
        //float dotv = dot_h + dot_v;
        float dot_h0 = dot(nor_c, nor_h0);
        float dot_v0 = dot(nor_c, nor_w0);
        float dot_h1 = dot(nor_c, nor_h1);
        float dot_v1 = dot(nor_c, nor_w1);
        float dotv = dot_h0 + dot_v0 + dot_h1 + dot_v1;
        depth_c = depth_min;
#endif

        if (abs(depth_max - depth_min) < outline_thres)
        {
            vout = (float4) 0;
#if SILHOUETTE_EDGE == 1
            if (dotv < 3.90)
                vout = float4(edge_color, 1);
#endif
        }
        else vout = float4(edge_color, 1);
        //vout = float4(nor_c, 1);
        //vout = float4(depth_h0 / 40, depth_h0 / 40, depth_h0 / 40, 1);
    }

    return vout;
}

float4 OutlineTest_old(const in int2 tex2d_xy, const in float depth_c, const in float discont_depth_criterion, const in float3 edge_color, const in int thick)
{
	float4 vout = (float4) 0;
	if (depth_c < 1000000)
	{
		float depth_h0 = sr_fragment_zdepth[tex2d_xy.xy + int2(0, thick)].r;
		float depth_h1 = sr_fragment_zdepth[tex2d_xy.xy - int2(0, thick)].r;
		float depth_w0 = sr_fragment_zdepth[tex2d_xy.xy + int2(thick, 0)].r;
		float depth_w1 = sr_fragment_zdepth[tex2d_xy.xy - int2(thick, 0)].r;

		const float outline_thres = discont_depth_criterion;
		float depth_min = min(min(depth_h0, depth_h1), min(depth_w0, depth_w1));
		float depth_max = max(max(depth_h0, depth_h1), max(depth_w0, depth_w1));
		//if (abs(depth_c - depth_min) > outline_thres * 1)
		//    return (float4)0;
		//float diff_max = max(abs(depth_h0 - depth_h1), abs(depth_w0 - depth_w1));
		//if (diff_max < outline_thres * 1)
		//    return (float4) 0;
#if SILHOUETTE_EDGE == 1
		const int thick2 = 1;
		float3 nor_c = normalize(sr_fragment_vis[tex2d_xy.xy + int2(0, 0)].xyz * 2.f - (float3)1.f);
		float3 nor_h0 = normalize(sr_fragment_vis[tex2d_xy.xy + int2(0, thick2)].xyz * 2.f - (float3)1.f);
		float3 nor_h1 = normalize(sr_fragment_vis[tex2d_xy.xy - int2(0, thick2)].xyz * 2.f - (float3)1.f);
		float3 nor_w0 = normalize(sr_fragment_vis[tex2d_xy.xy + int2(thick2, 0)].xyz * 2.f - (float3)1.f);
		float3 nor_w1 = normalize(sr_fragment_vis[tex2d_xy.xy - int2(thick2, 0)].xyz * 2.f - (float3)1.f);
		//float dot_h = dot(nor_h0, nor_h1);
		//float dot_v = dot(nor_w0, nor_w1);
		//float dotv = dot_h + dot_v;
		float dot_h0 = dot(nor_c, nor_h0);
		float dot_v0 = dot(nor_c, nor_w0);
		float dot_h1 = dot(nor_c, nor_h1);
		float dot_v1 = dot(nor_c, nor_w1);
		float dotv = dot_h0 + dot_v0 + dot_h1 + dot_v1;
#endif

		if (abs(depth_max - depth_min) < outline_thres)
		{
			vout = (float4) 0;
#if SILHOUETTE_EDGE == 1
			if(dotv < 3.90)
				vout = float4(edge_color, 1);
#endif
		}
		else vout = float4(edge_color, 1);
		//vout = float4(nor_c, 1);
		//vout = float4(depth_h0 / 40, depth_h0 / 40, depth_h0 / 40, 1);
	}
        
    return vout;
}

float4 OutlineTest(const in int2 tex2d_xy, inout float depth_c, const in float discont_depth_criterion, const in float3 edge_color, const in int thick, const in bool fadeMode)
{
    float2 min_rect = (float2)0;
    float2 max_rect = float2(g_cbCamState.rt_width - 1, g_cbCamState.rt_height - 1);
    
    float4 vout = (float4) 0;
    if (depth_c > 100000)
    {
        int count = 0;
        float depth_min = FLT_MAX;

        for (int y = -thick; y <= thick; y++) {
            for (int x = -thick; x <= thick; x++) {
                float2 sample_pos = tex2d_xy.xy + float2(x, y);
                float2 v1 = min_rect - sample_pos;
                float2 v2 = max_rect - sample_pos;
                float2 v12 = v1 * v2;
                if (v12.x >= 0 || v12.y >= 0)
                    continue;
                float depth = sr_fragment_zdepth[sample_pos].r;
                if (depth < 100000) {
                    count++;
                    depth_min = min(depth_min, depth);
#if DX10_0 == 1
                    y = thick + 1;
                    break;
#endif
                }
            }
        }

        if (count > 0) {
#if DX10_0 == 1
            vout = float4(edge_color, 1);
#else
            if (fadeMode) {
                float w = 2 * thick + 1;
                float alpha = min((float)count / (w * w / 2.f), 1.f);
                //alpha *= alpha;

                vout = float4(edge_color * alpha, alpha);
            }
            else vout = float4(edge_color, 1);
#endif
        }

        depth_c = depth_min;
        //vout = float4(nor_c, 1);
        //vout = float4(depth_h0 / 40, depth_h0 / 40, depth_h0 / 40, 1);
    }

    return vout;
}

float3 PhongBlinn(const in float3 cam_view, const in float3 light_dirinv, float3 normal, const in bool is_max_shading,
    const in float3 Ka, const in float3 Kd, const in float3 Ks, const in float Ns)
{
    if (dot(cam_view, normal) < 0)
        normal *= -1.f;
    float diff = dot(light_dirinv, normal);
    if (is_max_shading)
        diff = max(diff, 0);
    else
        diff = abs(diff);
    float3 H = normalize(cam_view + light_dirinv);
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
		if (g_cbEnv.env_flag & 0x1)
			light_dirinv = -normalize(pos_frag - g_cbEnv.pos_light_ws);
		color_out = PhongBlinn(-view_dir, light_dirinv, nrl / nrl_len * bump, g_cbPobj.pobj_flag & (0x1 << 5), Ka, Kd, Ks, Ns);
	}
	else// if ((g_cbPobj.pobj_flag & (0x1 << 3)) == 0)
	{
		color_out = Ka;
	}
}

void DashedLine(inout float4 v_rgba, inout float depthcs, const in float line_pos, const in float dash_interval, const in bool is_dashed, const in bool traparent_interval)
{
	// Parameter [fDashedDistance] Dashed Distance (WS)
	// Parameter [bIsDashed]
    if (is_dashed)
    {
		if ((uint) (abs(line_pos) / dash_interval) % 2 == 1)
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

    float intensity = g_texRgbaArray.Sample(g_samplerLinear, float3(pos_sample, 0)).r * v_rgba.a; // a single channl into rgba equally
    
    if (intensity == 0)
    {
        v_rgba = (float4) 0;
        depthcs = FLT_MAX;
    }
    else
    {
        v_rgba.rgb = v_rgba.rgb * intensity + (float3(1, 1, 1) - v_rgba.rgb) * (1.f - intensity);
        v_rgba.a = intensity;
    }
}

void TextureImgMap(inout float4 v_rgba, const float2 uv)
{
    // always rgba
    // to do // .. mipmap
    //pos_sample.x = 1. - pos_sample.x;
    //pos_sample.y = 1. - pos_sample.y;
    //pos_sample.z = 0;
    v_rgba = g_texRgbaArray.Sample(g_samplerLinear_wrap, float3(uv, 0)).bgra;
}
        
void TextureMaterialMap(inout float3 Ka, inout float3 Kd, inout float3 Ks, inout float Ns, inout float bump, inout float d, const float2 uv, const uint tex_map_enum)
{
    //Texture2D g_tex2D_Mat_KA : register(t10);
    //Texture2D g_tex2D_Mat_KD : register(t11);
    //Texture2D g_tex2D_Mat_KS : register(t12);
    //Texture2D g_tex2D_Mat_NS : register(t13);
    //Texture2D g_tex2D_Mat_BUMP : register(t14);
    //Texture2D g_tex2D_Mat_D : register(t15);
	float alpha_wildcard = -1;
	if (tex_map_enum & (0x1 << 1))
	{
        float4 load_tex = g_tex2D_Mat_KA.Sample(g_samplerLinear_wrap, uv).bgra;
		Ka = load_tex.rgb;
		alpha_wildcard = max(alpha_wildcard, load_tex.a);
	}
	if (tex_map_enum & (0x1 << 2))
	{
        float4 load_tex = g_tex2D_Mat_KD.Sample(g_samplerLinear_wrap, uv).bgra;
		Kd = load_tex.rgb;
		alpha_wildcard = max(alpha_wildcard, load_tex.a);
	}
	if (tex_map_enum & (0x1 << 3))
	{
        Ks = g_tex2D_Mat_KS.Sample(g_samplerLinear_wrap, uv).bgr;
    }
	if (tex_map_enum & (0x1 << 4))
        Ns = g_tex2D_Mat_NS.Sample(g_samplerLinear_wrap, uv).r;
	if (tex_map_enum & (0x1 << 5))
        bump = g_tex2D_Mat_BUMP.Sample(g_samplerLinear_wrap, uv).r;
    if (tex_map_enum & (0x1 << 6))
        d = g_tex2D_Mat_D.Sample(g_samplerLinear_wrap, uv).r;

	if (alpha_wildcard >= 0) d *= alpha_wildcard;
}

float3 PaintBlendOverlay(float3 base, float3 blend)
{
    float3 result;
    result.r = base.r < 0.5 ? 2.0 * base.r * blend.r : 1.0 - 2.0 * (1.0 - base.r) * (1.0 - blend.r);
    result.g = base.g < 0.5 ? 2.0 * base.g * blend.g : 1.0 - 2.0 * (1.0 - base.g) * (1.0 - blend.g);
    result.b = base.b < 0.5 ? 2.0 * base.b * blend.b : 1.0 - 2.0 * (1.0 - base.b) * (1.0 - blend.b);
    return result;
}

// Apply paint texture to diffuse color
void ApplyPaintTexture(inout float3 color, const float2 uv, const uint tex_map_enum, const int blendMode)
{
	float4 paintColor = g_tex2D_Paint.Sample(g_samplerLinear_clamp, uv);

	if (paintColor.a > 0.001)
	{
		float3 blendedColor;

		switch (blendMode)
		{
			case PAINT_BLEND_NORMAL:
				blendedColor = lerp(color, paintColor.rgb, paintColor.a);
				break;
			case PAINT_BLEND_MULTIPLY:
				blendedColor = lerp(color, color * paintColor.rgb, paintColor.a);
				break;
			case PAINT_BLEND_ADDITIVE:
				blendedColor = lerp(color, saturate(color + paintColor.rgb), paintColor.a);
				break;
			case PAINT_BLEND_OVERLAY:
				blendedColor = lerp(color, PaintBlendOverlay(color, paintColor.rgb), paintColor.a);
				break;
			case PAINT_BLEND_SCREEN:
				blendedColor = lerp(color, 1.0 - (1.0 - color) * (1.0 - paintColor.rgb), paintColor.a);
				break;
			default:
				blendedColor = lerp(color, paintColor.rgb, paintColor.a);
				break;
		}

		color = blendedColor;
	}
}

void MultiTextMapping(inout float4 v_rgba, inout float depthcs, float2 uv, const in int letter_idx, const in float3 vec_width_ps, const in float3 vec_height_ps)
{
    //MultiTextMapping(v_rgba, z_depth, input.f3Custom0.xy, (int) (input.f3Custom0.z + 0.5f), input.f3Custom1, input.f3Custom2);
    // Sample Flip State 0, 1, 2, 3

    if (dot(vec_width_ps, float3(1, 0, 0)) < 0)
        uv.x = 1.f - uv.x;
	
    float fNumLetters = (float) g_cbPobj.num_letters;
    float fHeightU = 1.f / fNumLetters;
    float fOffsetU = fHeightU * (float) letter_idx;
    float fLetterU = uv.y / fNumLetters;

    float3 vec_normal = cross(vec_height_ps, vec_width_ps);
    float3 __vec_height_ps = cross(vec_width_ps, vec_normal);

    if (dot(__vec_height_ps, float3(0, 1, 0)) < 0)
        uv.y = fOffsetU + (fHeightU - fLetterU);
    else
        uv.y = fOffsetU + fLetterU;

    float intensity = g_texRgbaArray.Sample(g_samplerLinear, float3(uv, 0)).r * v_rgba.a; // a single channl into rgba equally
    //g_tex2DText[float2(uv.x * 207, uv.y * 300)];
    //g_tex2DText.Sample(g_samplerLinear, uv, 0);
    
    //f4ColorOut = float4(uv, 1, 1);
    //f4ColorOut = float4(g_cbPobj.fcolor.rgb, 1);
	//return vxOut;

    if (intensity == 0)
    {
        v_rgba = (float4) 0;
        depthcs = FLT_MAX;
    }
    else
    {
        v_rgba.rgb = v_rgba.rgb * intensity + (float3(1, 1, 1) - v_rgba.rgb) * (1.f - intensity);
        v_rgba.a = intensity;
    }
}

struct PS_FILL_OUTPUT
{
    float4 color : SV_TARGET0; // UNORM
    float depthcs : SV_TARGET1;

    float ds_z : SV_Depth;
};

struct PS_FILL_DEPTHCS
{
    float depthcs : SV_TARGET0;

    float ds_z : SV_Depth;
};

float3 BisectionalRefine(const float3 pos_sample_ts, const float3 dir_sample_ts, const int num_refinement,
    const float dst_isovalue, const bool largerCheck)
{
    float t0 = 0, t1 = 1;

    // Interval bisection
    float3 pos_bis_s_ts = pos_sample_ts - dir_sample_ts;
    float3 pos_bis_e_ts = pos_sample_ts;

    int min_valid_v = g_cbTmap.first_nonzeroalpha_index;
    float _singed = largerCheck ? 1.f : -1.f;

    [loop]
    for (int j = 0; j < num_refinement; j++)
    {
        float3 pos_bis_ts = (pos_bis_s_ts + pos_bis_e_ts) * 0.5f;
        float t = (t0 + t1) * 0.5f;
        float sample_v = g_tex3DVolume.SampleLevel(g_samplerLinear_clamp, pos_bis_ts, 0).r;
        if (sample_v * _singed > dst_isovalue * _singed)
        {
            pos_bis_e_ts = pos_bis_ts;
            t1 = t;
        }
        else
        {
            pos_bis_s_ts = pos_bis_ts;
            t0 = t;
        }
    }

	return pos_bis_e_ts; //	float2(t0, t1);
}

float3 ComputeDeviation(float3 pos, float3 nrl, out bool colored, out float s)
{
    float sampleDist = g_cbVobj.sample_dist;
    float devMin = FLT_COMP_MAX;
    float distMin = FLT_COMP_MAX;

    float dst_isovalue = asfloat(g_cbPobj.pobj_dummy_0);

    // note pos and nrl are defined in WS
    float4x4 mat_ws2ts = g_cbVobj.mat_ws2ts;
	float3 posTS = TransformPoint(pos, mat_ws2ts);    
    
	bool is_clipped = false;
	if (g_cbVobj.clip_info.clip_flag & 0x1)
	{
		is_clipped = dot(g_cbVobj.clip_info.vec_clipplane, pos - g_cbVobj.clip_info.pos_clipplane) > 0;
	}
	if (g_cbVobj.clip_info.clip_flag & 0x2)
	{
		if (!IsInsideClipBox(pos, g_cbVobj.clip_info.mat_clipbox_ws2bs))
		{
			is_clipped = true;
		}
	}
    
	if (posTS.x <= 0 || posTS.x >= 1
        || posTS.y <= 0 || posTS.y >= 1
        || posTS.z <= 0 || posTS.z >= 1)
		is_clipped = true;
        
	if (!is_clipped)
	{
		nrl = normalize(nrl);
		float3 dirSampleTS = TransformVector(nrl * sampleDist, mat_ws2ts);
		float sample_dist_ts = length(dirSampleTS);
    
		float3 sampleDirs[2] = { dirSampleTS, -dirSampleTS };
		float3 sampleDirs_unit_ws[2] = { nrl, -nrl };
        
        float refVolValue = g_tex3DVolume.SampleLevel(g_samplerLinear, posTS, 0).r;
        
        [loop]
		for (int k = 0; k < 2; k++)
		{
			float3 sampleDirUnitWS = sampleDirs_unit_ws[k];
            float2 hits_t = ComputeVBoxHits(pos, sampleDirUnitWS, g_cbVobj.mat_alignedvbox_tr_ws2bs, g_cbVobj.clip_info);
            hits_t.y = min(g_cbCamState.far_plane, hits_t.y); // only available in orthogonal view (thickness slicer)
        
            hits_t.x = max(hits_t.x, 0.0);
            
			if (hits_t.y <= hits_t.x)
			{
				continue;
			}

			int numSamples = (int) ((hits_t.y - hits_t.x) / sampleDist);
            
			float3 posStartWS = pos + sampleDirUnitWS * hits_t.x; // hits_t.x is almost 0
			float3 posStartTS = TransformPoint(posStartWS, mat_ws2ts);
			float3 sampleDir = sampleDirs[k];

			if (refVolValue < dst_isovalue)
			{                 
                [loop]
				for (int i = 1; i < numSamples; i++)
				{
					float3 posSampleTS = posStartTS + sampleDir * (float) i;

                    LOAD_BLOCK_INFO(blkSkip, posSampleTS, sampleDir, numSamples, i)

					if (blkSkip.blk_value > 0)
					{
                        [loop]
						for (int j = 0; j <= blkSkip.num_skip_steps; j++)
						{
							float3 pos_sample_blk_ts = posStartTS + sampleDir * (float) (i + j);
							float sampleValue = g_tex3DVolume.SampleLevel(g_samplerLinear_clamp, pos_sample_blk_ts, 0).r;

							if (sampleValue > dst_isovalue)
							{
                                // Interval bisection
								float3 pos_refined_ts = BisectionalRefine(pos_sample_blk_ts, sampleDir, 10, dst_isovalue, true);
								float dist_ts = length(posTS - pos_refined_ts);
								float dist = dist_ts / sample_dist_ts * sampleDist;
								if (distMin > dist)
								{
									distMin = dist;
                                    // old version : 
                                    // devMin = -dist;
                                    devMin = k == 0 ? dist : -dist;
                                }
								i = numSamples;
								j = numSamples + blkSkip.num_skip_steps;
								break;
							}
						}
					}
					i += blkSkip.num_skip_steps;
				}
			}
			else
			{
            // The polygonal surface is inside the volume surface
                [loop]
				for (int i = 1; i < numSamples; i++)
				{
					float3 posSampleTS = posStartTS + sampleDir * (float) i;

					float sampleValue = g_tex3DVolume.SampleLevel(g_samplerLinear_clamp, posSampleTS, 0).r;
					if (sampleValue < dst_isovalue)
					{
						float3 pos_refined_ts = BisectionalRefine(posSampleTS, sampleDir, 10, dst_isovalue, false);
						float dist_ts = length(posTS - pos_refined_ts);
						float dist = dist_ts / sample_dist_ts * sampleDist;
						if (distMin > dist)
						{
                            distMin = dist;
                            // old version : 
                            // devMin = dist;
                            devMin = k == 0 ? dist : -dist;
                        }
						i = numSamples;
						break;
					}
				}
			}
		}
	}

	float3 f3Color = float3(1, 1, 1);
	colored = false;
    s = -1.f;

    float minMapping = g_cbTmap.mapping_v_min;
    float maxMapping = g_cbTmap.mapping_v_max;

    //devMin = abs(devMin);
    //if (devMin > minMapping && devMin < maxMapping)
    //if (devMin > 0 && devMin < maxMapping)
	if (!is_clipped)
    {
        float mapValue = ((devMin - minMapping) / (maxMapping - minMapping));
        s = mapValue;

        if (BitCheck(g_cbTmap.flag, 0)) 
        {
            float4 f4ColorMap = g_f4bufOTF[(int) (saturate(mapValue) * (g_cbTmap.tmap_size_x - 1))];
            colored = true;
            f3Color = f4ColorMap.rgb * f4ColorMap.a;

        }
        else
        {
            if (mapValue >= 0 && mapValue <= 1.f) {
                float4 f4ColorMap = g_f4bufOTF[(int) ((mapValue) * (g_cbTmap.tmap_size_x - 1))];
                colored = true;
                f3Color = f4ColorMap.rgb * f4ColorMap.a;
            }
        }
    }

	return f3Color;
}

void BasicShader(__VS_OUT input, out float4 v_rgba_out, out float z_depth_out)
{
    POBJ_PRE_CONTEXT;

    if (g_cbPobj.alpha == 0 || z_depth < 0 || (input.f4PosSS.z / input.f4PosSS.w < 0)
        || input.f4PosSS.x < 0 || input.f4PosSS.y < 0
        || (uint)input.f4PosSS.x >= g_cbCamState.rt_width
        || (uint)input.f4PosSS.y >= g_cbCamState.rt_height)
        clip(-1);

#if DX10_0
    float4 v_rgba = float4(g_cbPobj.Kd, 1.f); // note zero alpha actors are filtered out in the preprocessing of the rendering
#else
	float4 v_rgba = float4(g_cbPobj.Kd, g_cbPobj.alpha);
#endif
    float3 nor = (float3) 0;
    float nor_len = 0;

#if __RENDERING_MODE != 1 && __RENDERING_MODE != 2 && __RENDERING_MODE != 3
    nor = input.f3VecNormalWS;
    nor_len = length(nor);
#endif
    
	float3 Ka_pobj = g_cbPobj.Ka;
	float3 Kd_pobj = g_cbPobj.Kd;
	float3 Ks_pobj = g_cbPobj.Ks;

#if __RENDERING_MODE != 2
	if (BitCheck(g_cbPobj.pobj_flag, 3))
	{
		float3 color_in = input.f4Color.rgb * input.f4Color.a;
		Ka_pobj = color_in * g_cbPobj.pb_shading_factor.x;
		Kd_pobj = color_in * g_cbPobj.pb_shading_factor.y;
		Ks_pobj = color_in * g_cbPobj.pb_shading_factor.z;
	}
#endif
    
#if __PAINTER_UV == 1
    if (g_cbPobj.tex_map_enum & (0x1 << 17))
    {
	    float4 paintColor = g_tex2D_Paint.Sample(g_samplerLinear_clamp, input.f2UV);

	    if (paintColor.a > 0.001)
	    {
		    Ka_pobj = lerp(Ka_pobj, paintColor.rgb * g_cbPobj.pb_shading_factor.x, paintColor.a);
		    Kd_pobj = lerp(Kd_pobj, paintColor.rgb * g_cbPobj.pb_shading_factor.y, paintColor.a);
		    Ks_pobj = lerp(Ks_pobj, paintColor.rgb * g_cbPobj.pb_shading_factor.z, paintColor.a);
        }
    }
#endif
    
#if __RENDERING_MODE != 2
	if (g_cbPobj.tex_map_enum & (0x1 << 18))
	{
		float d = input.fDist;
		if (g_cbPobj.tex_map_enum & (0x1 << 19))
            d = length(g_cbCamState.hoverPosWS - input.f3PosWS);
		float r = g_cbCamState.hoverRadius;
        
        // 중심선에서의 signed distance (월드 단위)
		float sd = d - r;

        // 픽셀 한 칸에서 sd가 얼마나 바뀌는지(월드 단위 / pixel)
		float px = max(fwidth(d), 1e-6); // aa

        // 링 “반 두께”를 픽셀 기준으로 지정 (예: 2px -> 전체 4px)
		float halfWidthPx = 2.0; // 1.5~2.0 정도로 조절 , g_cbCamState.hoverBand

        // 월드 단위 반 두께 = (픽셀 반 두께) * (월드/픽셀)
		float w = halfWidthPx * px;

        // 링 마스크: sd=0에서 1, |sd|가 w보다 커지면 0 (부드럽게)
		float ring = 1.0 - smoothstep(w, w + px, abs(sd));
		//float ring = 1.0 - smoothstep(halfWidthPx * aa,
        //                      halfWidthPx * aa + aa,
        //                      abs(sd));

        // ring 값으로 색/알파 섞기
		float4 ink = ConvertUIntToFloat4(g_cbCamState.hoverColor); // 원하는 링 색
		ink.rgb *= ink.a;
        
		Ka_pobj = lerp(Ka_pobj, ink.rgb * g_cbPobj.pb_shading_factor.x, ring);
		Kd_pobj = lerp(Kd_pobj, ink.rgb * g_cbPobj.pb_shading_factor.y, ring);

	}
#endif
    
#if __RENDERING_MODE == 1
    DashedLine(v_rgba, z_depth, input.f2UV.x, g_cbPobj.dash_interval, g_cbPobj.pobj_flag & (0x1 << 19), g_cbPobj.pobj_flag & (0x1 << 20));
    if (v_rgba.a <= 0.01) clip(-1);
#elif __RENDERING_MODE == 2
    MultiTextMapping(v_rgba, z_depth, input.f3Custom0.xy, (int)(input.f3Custom0.z + 0.5f), input.f3Custom1, input.f3Custom2);
    if (v_rgba.a <= 0.01) clip(-1);
#elif __RENDERING_MODE == 3
    TextMapping(v_rgba, z_depth, input.f2UV.xy, g_cbPobj.pobj_flag & (0x1 << 9), g_cbPobj.pobj_flag & (0x1 << 10));
    if (v_rgba.a <= 0.01) clip(-1);
    //v_rgba.a = 1;
#elif __RENDERING_MODE == 4
    if (g_cbPobj.tex_map_enum == 1)
    {
        float4 clr_map;
        TextureImgMap(clr_map, input.f2UV);

        //clr_map.a g_cbPobj.alpha

        if (clr_map.a == 0)
        {
            //v_rgba = (float4) 0;
            //z_depth = FLT_MAX;
            clip(-1);
        }
        else
        {
            //float3 mat_shading = float3(g_cbEnv.ltint_ambient.w, g_cbEnv.ltint_diffuse.w, g_cbEnv.ltint_spec.w);
            float3 Ka = clr_map.rgb * g_cbEnv.ltint_ambient.rgb * Ka_pobj;
            float3 Kd = clr_map.rgb * g_cbEnv.ltint_diffuse.rgb * Kd_pobj;
            float3 Ks = clr_map.rgb * g_cbEnv.ltint_spec.rgb * Ks_pobj;
            float Ns = g_cbPobj.Ns;
            ComputeColor(v_rgba.rgb, Ka, Kd, Ks, Ns, 1.0, input.f3PosWS, view_dir, nor, nor_len);
        }
        v_rgba.a *= clr_map.a;
    }
    else
    {
        float3 Ka = Ka_pobj, Kd = Kd_pobj, Ks = Ks_pobj;
        float Ns = g_cbPobj.Ns, d = 1.0, bump = 1.0;
        TextureMaterialMap(Ka, Kd, Ks, Ns, bump, d, input.f2UV, g_cbPobj.tex_map_enum);
    
        if (any(Ka != Ka_pobj))
        {
            Ka *= g_cbPobj.pb_shading_factor.x;
        }
        if (any(Kd != Kd_pobj))
        {
            Kd *= g_cbPobj.pb_shading_factor.y;
        }
        if (any(Ks != Ks_pobj))
        {
            Ks *= g_cbPobj.pb_shading_factor.z;
        }
    
        if (Ns >= 0)
        {
            Ka *= g_cbEnv.ltint_ambient.rgb;
            Kd *= g_cbEnv.ltint_diffuse.rgb;
            Ks *= g_cbEnv.ltint_spec.rgb;
            ComputeColor(v_rgba.rgb, Ka, Kd, Ks, Ns, bump, input.f3PosWS, view_dir, nor, nor_len);
        }
        else // illumination model 0
            v_rgba.rgb = Kd;
        if (d <= 0.01) clip(-1);
        v_rgba.a *= d;
    }
    
#elif __RENDERING_MODE == 5    
    bool is_clipped = false;
    if (g_cbVobj.clip_info.clip_flag & 0x1)
    {
        is_clipped = dot(g_cbVobj.clip_info.vec_clipplane, input.f3PosWS - g_cbVobj.clip_info.pos_clipplane) > 0;
    }
    if (g_cbVobj.clip_info.clip_flag & 0x2)
    {
        if (!IsInsideClipBox(input.f3PosWS, g_cbVobj.clip_info.mat_clipbox_ws2bs))
        {
            is_clipped = true;
        }
    }
    
    float3 posTS = TransformPoint(input.f3PosWS, g_cbVobj.mat_ws2ts);
    if (posTS.x <= 0 || posTS.x >= 1
        || posTS.y <= 0 || posTS.y >= 1
        || posTS.z <= 0 || posTS.z >= 1)
        is_clipped = true;
    
    bool colored = false;
    float3 colorcoded = float3(1, 1, 1);
    if (!is_clipped)
    {
        float sample_v = g_tex3DVolume.SampleLevel(g_samplerLinear, posTS, 0).r;
        float4 colorMap = g_f4bufOTF[(int)(sample_v * (g_cbTmap.tmap_size_x - 1))];
        //colorMap = float4(0, 1, 0, 1);
        //colorMap.rgb = float3(0, 1, 0);
        
        if (BitCheck(g_cbPobj.pobj_flag, 7)) 
        {
            colored = true;
            colorcoded = colorMap.rgb * colorMap.a;
        }
        else 
        {
            if (colorMap.a > 0) 
            {
                colored = true;
                colorcoded = colorMap.rgb * colorMap.a;
            }
        }
    }
    
    if (nor_len > 0)
    {
        float3 Ka, Kd, Ks;
        if (colored)
        {
            Ka = colorcoded * g_cbVobj.pb_shading_factor.x * g_cbEnv.ltint_ambient.rgb;
            Kd = colorcoded * g_cbVobj.pb_shading_factor.y * g_cbEnv.ltint_diffuse.rgb;
            Ks = colorcoded * g_cbVobj.pb_shading_factor.z * g_cbEnv.ltint_spec.rgb;
        }
        else
        {
            Ka = Ka_pobj * g_cbEnv.ltint_ambient.rgb;
            Kd = Kd_pobj * g_cbEnv.ltint_diffuse.rgb;
            Ks = Ks_pobj * g_cbEnv.ltint_spec.rgb;
        }
        float Ns = g_cbPobj.Ns;

        ComputeColor(v_rgba.rgb, Ka, Kd, Ks, Ns, 1.0, input.f3PosWS, view_dir, nor, nor_len);
    }
    
#elif __RENDERING_MODE == 6
    bool colored = false;
    float3 colorcoded = float3(1, 1, 1);
    float s = -1.f;
    if (nor_len > 0)
        colorcoded = ComputeDeviation(input.f3PosWS, nor, colored, s); 
             
    if (nor_len > 0)
    {
        float3 Ka, Kd, Ks;
        if (colored)
        {
            float w = 3;//clamp(fwidth(s), 1e-6, 0.01);

            // inside 마스크: s가 [0,1] 안이면 1, 밖이면 0 (경계에서만 부드럽게)
            //float in0 = smoothstep(0.0 - w, 0.0 + w, s);       // 0 아래 ->0, 0 위 ->1
            //float in1 = 1.0 - smoothstep(1.0 - w, 1.0 + w, s); // 1 아래 ->1, 1 위 ->0
            //float inside = in0 * in1;                           // 둘 다 만족하면 inside≈1
    
            //Ka = lerp(Ka_pobj, colorcoded * g_cbVobj.pb_shading_factor.x, inside) * g_cbEnv.ltint_ambient.rgb;
            //Kd = lerp(Kd_pobj, colorcoded * g_cbVobj.pb_shading_factor.y, inside) * g_cbEnv.ltint_diffuse.rgb;
            //Ks = lerp(Ks_pobj, colorcoded * g_cbVobj.pb_shading_factor.z, inside) * g_cbEnv.ltint_spec.rgb;
    
            Ka = colorcoded * g_cbVobj.pb_shading_factor.x * g_cbEnv.ltint_ambient.rgb;
            Kd = colorcoded * g_cbVobj.pb_shading_factor.y * g_cbEnv.ltint_diffuse.rgb;
            Ks = colorcoded * g_cbVobj.pb_shading_factor.z * g_cbEnv.ltint_spec.rgb;

        }
        else
        {
            Ka = Ka_pobj * g_cbEnv.ltint_ambient.rgb;
            Kd = Kd_pobj * g_cbEnv.ltint_diffuse.rgb;
            Ks = Ks_pobj * g_cbEnv.ltint_spec.rgb;
        }
        float Ns = g_cbPobj.Ns;
        ComputeColor(v_rgba.rgb, Ka, Kd, Ks, Ns, 1.0, input.f3PosWS, view_dir, nor, nor_len);
    }

#else // __RENDERING_MODE == 0
    float3 Ka, Kd, Ks;
    float Ns = g_cbPobj.Ns;

    //if (BitCheck(g_cbPobj.pobj_flag, 3))
    //{
	//	float3 color_vtx = input.f4Color.rgb * input.f4Color.a;
    //    Ka = color_vtx * Ka_pobj;
    //    Kd = color_vtx * Kd_pobj;
	//	Ks = color_vtx * Ks_pobj;
	//}
    //else
    {
        // note g_cbPobj's Ka, Kd, and Ks has already been multiplied by pb_shading_factor.xyz
        Ka = Ka_pobj;
        Kd = Kd_pobj;
		Ks = Ks_pobj;
	}

    if (nor_len > 0)
    {
        Ka *= g_cbEnv.ltint_ambient.rgb;
        Kd *= g_cbEnv.ltint_diffuse.rgb;
		Ks *= g_cbEnv.ltint_spec.rgb;
        ComputeColor(v_rgba.rgb, Ka, Kd, Ks, Ns, 1.0, input.f3PosWS, view_dir, nor, nor_len);
    }
    else {
        v_rgba.rgb = Kd;
    }

#endif
        
#if DX10_0 == 1
    v_rgba.a = 1.f;
#else
    // make it as an associated color.
    // as a color component is stored into 8 bit channel, the alpha-multiplied precision must be determined in this stage.
    // unless, noise dots appear.
	v_rgba.rgb *= v_rgba.a;

#ifdef __GHOST_EFFECT__
    // dynamic opacity modulation
    bool is_dynamic_transparency = BitCheck(g_cbPobj.pobj_flag, 22);
    bool is_mask_transparency = BitCheck(g_cbPobj.pobj_flag, 23);
    if (is_dynamic_transparency || is_mask_transparency)
    {
        float mask_weight = 1, dynamic_alpha_weight = 1;
        int out_lined = GhostedEffect(mask_weight, dynamic_alpha_weight, input.f3PosWS, view_dir, nor, nor_len, is_dynamic_transparency);
        if (out_lined > 0)
            v_rgba = float4(1, 1, 0, 1);
        else
        {
            if (is_dynamic_transparency)
                v_rgba.rgba *= dynamic_alpha_weight;
            if (is_mask_transparency)
                v_rgba.rgba *= mask_weight;
            if (v_rgba.a <= 0.01) clip(-1);
        }
        //v_rgba = float4(1, 1, 0, 1);
    }
#endif

#endif
    v_rgba_out = v_rgba;
    z_depth_out = z_depth;
}

[earlydepthstencil]
PS_FILL_DEPTHCS SINGLE_LAYER(VS_OUTPUT input)
{
    PS_FILL_DEPTHCS out_ps;
    out_ps.ds_z = 0.f;
    out_ps.depthcs = FLT_MAX;

    POBJ_PRE_CONTEXT;

    out_ps.ds_z = input.f4PosSS.z;
    //out_ps.color = v_rgba;
    out_ps.depthcs = z_depth;
    return out_ps;
}


[earlydepthstencil]
PS_FILL_DEPTHCS WRITE_DEPTHZ(VS_OUTPUT input)
{
    PS_FILL_DEPTHCS out_ps;
    out_ps.ds_z = 0.f;
    out_ps.depthcs = FLT_MAX;

    POBJ_PRE_CONTEXT;

    out_ps.ds_z = input.f4PosSS.z;

    float3 pos_cs = TransformPoint(input.f3PosWS, g_cbCamState.mat_ws2cs);

    out_ps.depthcs = -pos_cs.z;
    return out_ps;
}


// geometry shader
// d3d10
static const float PI = 3.1415926f;
static const float fRatio = 2.0f;
static float fThickness = 0.01f;

//struct VS_OUTPUT
//{
//	float4 f4PosSS : SV_POSITION;
//	float3 f3VecNormalWS : NORMAL;
//	float3 f3PosWS : TEXCOORD0;
//	float3 f3Custom : TEXCOORD1;
//};

[maxvertexcount(42)]
void GS_TriNormal(triangle VS_OUTPUT input[3], inout TriangleStream<VS_OUTPUT> triangleStream, uint primID : SV_PrimitiveID)
{
    float3 pos0 = input[0].f3PosWS;
    float3 pos1 = input[1].f3PosWS;
    float3 pos2 = input[2].f3PosWS;

    float3 normal = normalize(cross(pos1 - pos0, pos2 - pos0));
    float4 faceColor = (float4)1;
    if (BitCheck(g_cbPobj.pobj_flag, 2))
    {
        faceColor = g_faceColors[primID].rgba;
    }


    VS_OUTPUT output[3] = { input[0] , input[1] , input[2] };
    for (uint i = 0; i < 3; i++) {
        VS_OUTPUT output = input[i];

        if (BitCheck(g_cbPobj.pobj_flag, 1))
        {
            output.f3VecNormalWS = normal;
        }
        if (BitCheck(g_cbPobj.pobj_flag, 2))
		{
			output.f4Color = faceColor;
        }

        triangleStream.Append(output);
    }
    triangleStream.RestartStrip();
}


[maxvertexcount(42)]
void GS_Surfels(point VS_OUTPUT input[1], inout TriangleStream<VS_OUTPUT> triangleStream)
{
	// later using hull shader...
	const int nCountTriangles = 10;
	VS_OUTPUT output = input[0];

	float4 pos_center_ss = output.f4PosSS;
	float3 pos_center_ws = output.f3PosWS;
	float3 nrl_center_ws = output.f3VecNormalWS;
	if (length(nrl_center_ws) == 0) // no normals
		nrl_center_ws = normalize(TransformPerspVector(float3(0, 0, 1), g_cbCamState.mat_ss2ws));
	float3 up_center_ws = float3(1, 0, 0);
	float3 right_center_ws = cross(nrl_center_ws, up_center_ws);
	if (length(right_center_ws) < 0.0001f)
	{
		up_center_ws = float3(0, 1, 0);
		right_center_ws = cross(nrl_center_ws, up_center_ws);
	}
	right_center_ws = normalize(right_center_ws);
	up_center_ws = normalize(cross(right_center_ws, nrl_center_ws));

	float4x4 mat_t_ori = m_translate(IDENTITY_MATRIX, -pos_center_ws);
	float4x4 mat_t_cen = m_translate(IDENTITY_MATRIX, pos_center_ws);

	float angle = 2.f * F_PI / nCountTriangles;
	float4 qt = float4(sin(angle * 0.5f) * nrl_center_ws, cos(angle * 0.5f));
	float4x4 mat_r = quaternion_to_matrix(qt);
	float4x4 mat = mul(mat_t_cen, mul(mat_r, mat_t_ori));

	const float radii = g_cbPobj.pix_thickness;
	float3 pos_upr_ws = pos_center_ws + up_center_ws * radii;

	for (int nI = 0; nI < nCountTriangles; ++nI)
	{
		output.f3PosWS = pos_center_ws;
		output.f4PosSS = pos_center_ss;
		triangleStream.Append(output);
		
		output.f3PosWS = pos_upr_ws;
		output.f4PosSS = mul(g_cbCamState.mat_ws2ps_revZ, float4(output.f3PosWS, 1.f)); // Reverse Z
		triangleStream.Append(output);

		output.f3PosWS = pos_upr_ws = TransformPoint(pos_upr_ws, mat);
		output.f4PosSS = mul(g_cbCamState.mat_ws2ps_revZ, float4(output.f3PosWS, 1.f)); // Reverse Z
		triangleStream.Append(output);
		
		triangleStream.RestartStrip();
	}
}

[maxvertexcount(42)]
void GS_ThickPoints_PixelSurfelSize(point VS_OUTPUT input[1], inout TriangleStream<VS_OUTPUT> triangleStream)
{
	float3 vec_up_ws = normalize(TransformPerspVector(float3(0, -1, 0), g_cbCamState.mat_ss2ws));
	float3 vec_r_ws = normalize(TransformPerspVector(float3(1, 0, 0), g_cbCamState.mat_ss2ws));

	float3 pos_w_ws = input[0].f3PosWS + vec_r_ws * g_cbPobj.pix_thickness;
	float3 pos_h_ws = input[0].f3PosWS + vec_up_ws * g_cbPobj.pix_thickness;

	float3 pos_o_ss = TransformPoint(input[0].f3PosWS, g_cbCamState.mat_ws2ss);
	float3 pos_w_ss = TransformPoint(pos_w_ws, g_cbCamState.mat_ws2ss);
	float3 pos_h_ss = TransformPoint(pos_h_ws, g_cbCamState.mat_ws2ss);

	float4 positionPointTransformed = input[0].f4PosSS;
	// note that input[0].f4PosSS (SV_POSITION) is for the rasterizer coordinate, whose range is [0, 1] different from the symentics of PS.
	const float pixel_thicknessXss = abs(pos_w_ss.x - pos_o_ss.x);
	const float pixel_thicknessYss = abs(pos_h_ss.y - pos_o_ss.y);
	if (pixel_thicknessXss < 0.01 || pixel_thicknessYss < 0.01)
		return;

	const float pixel_thicknessX = pixel_thicknessXss / (float)g_cbCamState.rt_width * positionPointTransformed.w;
	const float pixel_thicknessY = pixel_thicknessYss / (float)g_cbCamState.rt_height * positionPointTransformed.w;

    const int nCountTriangles = 10;
    VS_OUTPUT output = input[0];

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

[maxvertexcount(42)]
void GS_ThickPoints_Pixels(point VS_OUTPUT input[1], inout TriangleStream<VS_OUTPUT> triangleStream)
{
	float4 positionPointTransformed = input[0].f4PosSS;

	const int nCountTriangles = 10;
	VS_OUTPUT output = input[0];
	const float pixel_thicknessX = g_cbPobj.pix_thickness / (float)g_cbCamState.rt_width * positionPointTransformed.w;
	const float pixel_thicknessY = g_cbPobj.pix_thickness / (float)g_cbCamState.rt_height * positionPointTransformed.w;
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
    
    float2 d = positionPoint0Transformed.xy - positionPoint1Transformed.xy;
    float  ls = dot(d, d);
    if (ls < 1e-10) return;           // skip zero-length

    float  cosA = clamp(d.x * rsqrt(ls), -1.0, 1.0);
    float  fAngle = acos(cosA);
    if (d.y > 0) fAngle = 2.0f * PI - fAngle;
    fAngle = -(fAngle + PI * 0.5f);
        
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