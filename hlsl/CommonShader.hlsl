#include "macros.hlsl"
#include "Matrix.hlsl"

#define FLT_MAX 3.402823466e+38F
#define FLT_COMP_MAX 3.402823466e+30F
#define FLT_MIN 0.0000001F
#define D_PI 3.14159265358979323846
#define F_PI 3.1415927
#define GRIDSIZE 8
#define ERT_ALPHA 0.99f

#define FLT_MIN__ 0.000001f			// precision problem for zero-division 
#define FLT_OPACITY_MIN__ 1.f/255.f		// trival storage problem 
#define SAFE_OPAQUEALPHA 	0.999f		// ERT_ALPHA
#define FLT_LARGE 1000000.f

// Sampler //
SamplerState g_samplerLinear : register(s0); // ZeroBoader
SamplerState g_samplerPoint : register(s1); // ZeroBoader
SamplerState g_samplerLinear_clamp : register(s2);
SamplerState g_samplerPoint_clamp : register(s3);
SamplerState g_samplerLinear_wrap : register(s4);
SamplerState g_samplerPoint_wrap: register(s5);


struct HxCB_CameraState // Hlsl dX Contant Buffer
{
	float4x4 mat_ws2ss;
	float4x4 mat_ss2ws;

	float3 pos_cam_ws;
	uint rt_width;

	float3 dir_view_ws;
	uint rt_height;

	float cam_vz_thickness;
	uint k_value;
	// 1st bit : 0 (orthogonal), 1 : (perspective)
	// 2nd bit : for RT to k-buffer : 0 (just RT), 1 : (after silhouette processing)
	// 3rd bit : for DK+B // deprecated... (this will be treated as a separate shader
	// 4th bit : for storing the final fragments to the k buffer, which is used for sequentially coming renderer (e.g., DVR) : 0 (skipping), 1 (storing)
	// 5th bit : only for DFB without (S)FM. stores all fragments into the framebuffer (using offset table)
	// 6th bit : 0 : (normal rendering), 1 : picking mode
	// 7th bit : 0 : (stores the final RGBA and depth to RT), 1 : (does not store them) // will be deprecated
	uint cam_flag;
	// used for 1) A-Buffer prefix computations /*deprecated*/ or 2) beta (asfloat) for merging operation
	uint iSrCamDummy__0; 

	float near_plane;
	float far_plane;
	// used for 1) the level of MIPMAP generation (only for SSAO rendering), or 2) picking xy (16bit, 16bit)
	uint iSrCamDummy__1; // used for the N-buffer index or SSAO setting for DOF
	uint iSrCamDummy__2; // scale_z_res
};

struct HxCB_EnvState
{
	float3 pos_light_ws;
	// 1st bit : 0 (parallel), 1 : (spot)
	// 2nd bit : 0 (only polygons for SSAO), 1 : (volume G buffer for SSAO)
	// 10th~13th bit : 0 (no SSAO output to render buffer), 1~8: (0~7th layer of SSAO to render buffer), 9: vr layer of SSAO to render buffer
	uint env_flag;

	float3 dir_light_ws;
	uint num_lights;

	float4 ltint_ambient;
	float4 ltint_diffuse;
	float4 ltint_spec;

	float4x4 shadowmap_mat_ws2ls; // for shadow : Sample Depth Map
	float4x4 shadowmap_mat_ws2wls; // for shadow : Depth Comparison And Storage

	float r_kernel_ao;
	int num_dirs;
	int num_steps;
	float tangent_bias;

	float ao_intensity;
	uint num_safe_loopexit;
	uint env_dummy_1;
	uint env_dummy_2;

	float dof_lens_r;
	float dof_sensor_z; // sensor distance (lens' image plane, not view-frustum's image plane of rendering pipeline)
	float dof_focus_z;
	int dof_lens_ray_num_samples;
};

struct HxCB_ClipInfo
{
    float4x4 mat_clipbox_ws2bs; // To Clip Box Space (BS)
	float3 pos_clipplane;
	// 1st bit : 0 (No) 1 (Clip Box)
	// 2nd bit : 0 (No) 1 (Clip plane)
	uint clip_flag;

	float3 vec_clipplane;
	uint ci_dummy_1;
};

struct HxCB_PolygonObject
{
    float4x4 mat_os2ws;
    float4x4 mat_os2ps; // optional
    
    float3 Ka;
    float Ns;
    float3 Kd;
    float alpha;
    float3 Ks;
	// 1st bit : g_texRgbaArray
	// 2nd bit : g_tex2D_KA
	// 3rd bit : g_tex2D_KD
	// 4th bit : g_tex2D_KS
	// 5th bit : g_tex2D_NS
	// 6th bit : g_tex2D_BUMP
	// 7th bit : g_tex2D_D
    uint tex_map_enum;

	// 1st bit : 0 (shading color to RT) 1 (normal to RT for the purpose of silhouette rendering)
	// 4th bit : 0 (Set texture0 to texture0) 1 (Set global_color to texture0)
	// 6th bit : 0 (Diffuse abs) 1 (Diffuse max)
	// 10th bit : 0 (No XFlip) 1 (XFlip)
	// 11th bit : 0 (No XFlip) 1 (YFlip)
	// 20th bit : 0 (No Dashed Line) 1 (Dashed Line)
	// 21th bit : 0 (Transparent Dash) 1 (Dash As Color Inverted)
	// 23th bit : 0 (static alpha) 1 (dynamic alpha using mask t50) ... mode 1
	// 24th bit : 0 (static alpha) 1 (dynamic alpha using mask t50) ... mode 2
	// 31~32th bit : max components for dashed line : 0 ==> x, 1 ==> y, 2 ==> z
    uint pobj_flag;
    uint num_letters;
    float dash_interval;
    float depth_thres; // for outline!

    float pix_thickness; // 1) for POINT and LINE TOPOLOGY, 2) outline thickness (in pixel)
    float vz_thickness;
	uint pobj_dummy_0; // 1) actor_id used for picking, 2) outline color
	uint pobj_dummy_1; 
};

struct HxCB_VolumeObject
{
	// Volume Information and Clipping Information
    float4x4 mat_ws2ts; // for Sampling and Ray Traversing

    float4x4 mat_alignedvbox_tr_ws2bs;

	float grad_max; // 
	float grad_scale; // 
	float kappa_i; // 
	float kappa_s; // 

    float3 vec_grad_x;
    float value_range;
    float3 vec_grad_y;
    float sample_dist;
    float3 vec_grad_z;
    float opacity_correction;
    float3 vol_size; // volume size stored in GPU memory
    float vz_thickness;

    float3 volblk_size_ts;
    float volblk_value_range;

	// 1st bit : 0 (use the input normal) 1 (invert the input normal) ==> will be deprecated! (always faces to camera)
	// 24~31bit : Sculpt Mask Value (1 byte)
	uint vobj_flag;
	uint iso_value;
	float mask_value_range;
	uint outline_color; // 

	float4 pb_shading_factor; // x : Ambient, y : Diffuse, z : Specular, w : specular
};

struct HxCB_RenderingEffect // normally for each object
{
	// 1st bit : AO or Not , 2nd bit : Anisotropic BRDF or Not , 3rd bit : Apply Shading Factor or Not
	// NA ==> 4th bit : 0 : Normal Curvature Map (2D), 1 : Apply Concaveness
	// NA ==> 5th bit : Concaveness Direction or Not
    uint rf_flag;
    uint outline_mode;
    float curvature_kernel_radius;
    uint rf_dummy_0;

    float brdf_diffuse_ratio;
    float brdf_reft_ratio;
    float brdf_expw_u;
    float brdf_expw_v;

    float shadowmap_occusion_w; // for shadow
    float shadowmap_depth_bias; // for shadow
    float occ_radius;
    uint occ_num_rays;

	float ao_intensity;
	uint rf_dummy_1;
	uint rf_dummy_2;
	uint rf_dummy_3;
};

struct HxCB_VolumeRenderingEffect // normally for each volume
{
    float clip_plane_intensity;
    float attribute_voxel_sharpness;
    float vrf_dummy_2;
    float vrf_dummy_3;

    float occ_sample_dist_scale; // for occlusion
    float sdm_sample_dist_scale; // for shadow
    uint vrf_dummy_0;
    uint vrf_dummy_1;
};

struct HxCB_TMAP
{
    float4 last_color;

    uint first_nonzeroalpha_index; // For ESS
    uint last_nonzeroalpha_index;
    uint tmap_size_x;
    float mapping_v_min;

    float mapping_v_max;
    uint tm_dummy_0;
    uint tm_dummy_1;
    uint tm_dummy_2;
};

//struct HxCB_HotspotMask
//{
//	int pos_centerx_[4];
//	int pos_centery_[4];
//	int radius_[4];
//	int smoothness_[4];
//	float thick_[4];
//	float kappa_t_[4];
//	float kappa_s_[4];
//	float bnd_thick_[4];
//};

struct HotspotMask
{
	int2 pos_center;
	float radius;
	int smoothness;
	float thick;
	float kappa_t;
	float kappa_s;
	float bnd_thick;

	int flag;
	float3 pos_spotcenter;

	float in_depth_vis;
	int __dummy0;
	int __dummy1;
	int __dummy2;
};

struct HxCB_HotspotMask
{
	HotspotMask mask_info_[2];
};

struct HxCB_CurvedSlicer
{
	float3 posTopLeftCOS;
	int numCurvePoints;
	float3 posTopRightCOS;
	float planeHeight;
	float3 posBottomLeftCOS;
	float thicknessPlane;
	float3 posBottomRightCOS;
	int numRaySteps; 
	float3 planeUp; // WS, length is planePitch
	uint flag; // 1st bit : isRightSide
};

//=====================
// Constant Buffers
//=====================
cbuffer cbGlobalParams : register(b0)
{
    HxCB_CameraState g_cbCamState;
}

cbuffer cbGlobalParams : register(b1)
{
    HxCB_PolygonObject g_cbPobj;
}

cbuffer cbGlobalParams : register(b2)
{
    HxCB_ClipInfo g_cbClipInfo;
}

cbuffer cbGlobalParams : register(b3)
{
    HxCB_RenderingEffect g_cbRndEffect;
}

cbuffer cbGlobalParams : register(b4)
{
    HxCB_VolumeObject g_cbVobj;
}

cbuffer cbGlobalParams : register(b5)
{
    HxCB_TMAP g_cbTmap;
}

cbuffer cbGlobalParams : register(b6)
{
	HxCB_VolumeRenderingEffect g_cbVrEffect;
}

cbuffer cbGlobalParams : register(b7)
{
	HxCB_EnvState g_cbEnv;
}

//=====================
// Helpers
//=====================
float3 TransformPoint(in float3 pos_src, in float4x4 matT)
{
	//float4 pos_src_h = mul(float4(pos_src, 1.f), matT);
	float4 pos_src_h = mul(matT, float4(pos_src, 1.f));
    return pos_src_h.xyz / pos_src_h.w;
}

float3 TransformVector(in float3 vec_src, in float4x4 matT)
{
	//return mul(vec_src, (float3x3) matT);
	return mul((float3x3) matT, vec_src);
}

float3 TransformPerspVector(const in float3 vec_src, const in float4x4 matT)
{
	//float4 f4PosOrigin = mul(float4(0, 0, 0, 1), matT);
	float4 f4PosOrigin = mul(matT, float4(0, 0, 0, 1));
    float3 f3PosOrigin = f4PosOrigin.xyz / f4PosOrigin.w;
	//float4 f4PosDistance = mul(float4(vec_src, 1.f), matT);
	float4 f4PosDistance = mul(matT, float4(vec_src, 1.f));
    float3 f3PosDistance = f4PosDistance.xyz / f4PosDistance.w;
    return f3PosDistance - f3PosOrigin;
}

float3 TransformVectorWoPerspective(const in float3 vec_src, const in float4x4 matT)
{
	//return mul(vec_src, (float3x3)matT);
	return mul((float3x3)matT, vec_src);
}

float4 BlendFloat4AndFloat4(const in float4 fColor1, const in float4 fColor2)
{
    return fColor1 + fColor2 - (fColor1 * fColor2.a + fColor2 * fColor1.a) / 2.f;
    //float4 color_merge;
    //float _a_sum = fColor1.a + fColor2.a;
    //color_merge.a = fColor1.a + fColor2.a - fColor1.a * fColor2.a;
    //color_merge.rgb = (fColor1.rgb + fColor2.rgb) / _a_sum * color_merge.a;
    //return color_merge;
}

float4 ConvertUIntToFloat4(const uint iColor)
{
	// RGBA
    return float4(((iColor >> 16) & 0xFF) / 255.f, ((iColor >> 8) & 0xFF) / 255.f, (iColor & 0xFF) / 255.f, ((iColor >> 24) & 0xFF) / 255.f);
}

uint ConvertFloat4ToUInt(const in float4 fColor)
{
	// RGBA
    uint iR = (uint) (min(fColor.x * 255.f, 255.f) + 0.5f);
    uint iG = (uint) (min(fColor.y * 255.f, 255.f) + 0.5f);
    uint iB = (uint) (min(fColor.z * 255.f, 255.f) + 0.5f);
    uint iA = (uint) (min(fColor.w * 255.f, 255.f) + 0.5f);
    return (iA << 24) | (iR << 16) | (iG << 8) | iB;
}

bool IsInsideClipBox(const in float3 pos_target, const in float4x4 mat_2_bs)
{
    float3 pos_target_bs = TransformPoint(pos_target, mat_2_bs);
    //float3 pos_maxbs2targetbs = pos_target_bs - pos_max_bs;
    float3 _dot = (pos_target_bs + float3(0.5, 0.5, 0.5)) * (pos_target_bs - float3(0.5, 0.5, 0.5));
	return _dot.x <= 0 && _dot.y <= 0 && _dot.z <= 0;
    //if (_dot.x > 0 || _dot.y > 0 || _dot.z > 0)
    //    return false;
    //return true;
}

float2 ComputeAaBbHits(const in float3 pos_start, in float3 pos_min, const in float3 pos_max, const in float3 vec_dir)
{
	// intersect ray with a box
	// http://www.siggraph.org/education/materials/HyperGraph/raytrace/rtinter3.htm
    float3 invR = float3(1.0f, 1.0f, 1.0f) / vec_dir;
    float3 tbot = invR * (pos_min - pos_start);
    float3 ttop = invR * (pos_max - pos_start);

    // re-order intersections to find smallest and largest on each axis
    float3 tmin = min(ttop, tbot);
    float3 tmax = max(ttop, tbot);

    // find the largest tmin and the smallest tmax
    float largest_tmin = max(max(tmin.x, tmin.y), max(tmin.x, tmin.z));
    float smallest_tmax = min(min(tmax.x, tmax.y), min(tmax.x, tmax.z));

    float tnear = max(largest_tmin, 0.f);
    float tfar = smallest_tmax;
    return float2(tnear, tfar);
}

float2 ComputeClipBoxHits(const in float3 pos_start, const in float3 vec_dir, const in float4x4 mat_vbox_2bs)
{
    float3 pos_src_bs = TransformPoint(pos_start, mat_vbox_2bs);
    //float3 pos_max_bs = TransformPoint(pos_vbox_max, mat_vbox_2bs);
	float3 vec_dir_bs = TransformVector(vec_dir, mat_vbox_2bs);// normalize(TransformVector(vec_dir, mat_vbox_2bs));
    float2 hit_t = ComputeAaBbHits(pos_src_bs, float3(-0.5, -0.5, -0.5), float3(0.5, 0.5, 0.5), vec_dir_bs);
    return hit_t;
}

float2 ComputePlaneHits(const in float prev_t, const in float next_t, const in float3 pos_onplane, const in float3 vec_plane, const in float3 pos_start, const in float3 vec_dir)
{
    float2 hits_t = float2(prev_t, next_t);

	// H : vec_planeSVS, V : f3VecSampleSVS, A : f3PosIPSampleSVS, Q : f3PosPlaneSVS //
	// 0. Is ray direction parallel with plane's vector?
    float dot_HV = dot(vec_plane, vec_dir);
    if (dot_HV != 0)
    {
		// 1. Compute T for Position on Plane
        float fT = dot(vec_plane, pos_onplane - pos_start) / dot_HV;
		// 2. Check if on Cube
        if (fT > prev_t && fT < next_t)
        {
			// 3. Check if front or next position
            if (dot_HV < 0)
                hits_t.x = fT;
            else
                hits_t.y = fT;
        }
        else if (fT > prev_t && fT > next_t)
        {
            if (dot_HV < 0)
                hits_t.y = -FLT_MAX; // return;
            else
				; // conserve prev_t and next_t
        }
        else if (fT < prev_t && fT < next_t)
        {
            if (dot_HV < 0)
				;
            else
                hits_t.y = -FLT_MAX; // return;
        }
    }
    else
    {
		// Check Upperside of plane
        if (dot(vec_plane, pos_onplane - pos_start) <= 0)
            hits_t.y = -FLT_MAX; // return;
    }
	
    return hits_t;
}

float2 ComputeVBoxHits(const in float3 pos_start, const in float3 vec_dir, const in float4x4 mat_vbox_2bs, const in HxCB_ClipInfo clip_info)
{
	// Compute VObject Box Enter and Exit //
    float2 hits_t = ComputeClipBoxHits(pos_start, vec_dir, mat_vbox_2bs);
	
	if (hits_t.y > hits_t.x)
	{
		// Custom Clip Plane //
		if (clip_info.clip_flag & 0x1)
		{
			float2 hits_clipplane_t = ComputePlaneHits(hits_t.x, hits_t.y, clip_info.pos_clipplane, clip_info.vec_clipplane, pos_start, vec_dir);

			hits_t.x = max(hits_t.x, hits_clipplane_t.x);
			hits_t.y = min(hits_t.y, hits_clipplane_t.y);

			//if (hits_t.y <= hits_t.x)
			//{
			//	return hits_t;
			//}
		}

		if (clip_info.clip_flag & 0x2)
		{
			float2 hits_clipbox_t = ComputeClipBoxHits(pos_start, vec_dir, clip_info.mat_clipbox_ws2bs);

			hits_t.x = max(hits_t.x, hits_clipbox_t.x);
			hits_t.y = min(hits_t.y, hits_clipbox_t.y);

			//if (hits_t.y <= hits_t.x)
			//{
			//	return hits_t;
			//}
		}
	}
	
    return hits_t;
}

float Random(const in float2 seed2)
{
    float2 temp = float2(23.1406926327792690, 2.6651441426902251);
    return frac(cos(123456789.0 % 1e-7 + 256.0 * dot(seed2, temp)));
}

// A single iteration of Bob Jenkins' One-At-A-Time hashing algorithm.
uint hash(uint x) {
	x += (x << 10u);
	x ^= (x >> 6u);
	x += (x << 3u);
	x ^= (x >> 11u);
	x += (x << 15u);
	return x;
}

// Compound versions of the hashing algorithm I whipped together.
uint hash(uint2 v) { return hash(v.x ^ hash(v.y)); }
uint hash(uint3 v) { return hash(v.x ^ hash(v.y) ^ hash(v.z)); }
uint hash(uint4 v) { return hash(v.x ^ hash(v.y) ^ hash(v.z) ^ hash(v.w)); }

// Construct a float with half-open range [0:1] using low 23 bits.
// All zeroes yields 0.0, all ones yields the next smallest representable value below 1.0.
float floatConstruct(uint m) {
	const uint ieeeMantissa = 0x007FFFFFu; // binary32 mantissa bitmask
	const uint ieeeOne = 0x3F800000u; // 1.0 in IEEE binary32

	m &= ieeeMantissa;                     // Keep only mantissa bits (fractional part)
	m |= ieeeOne;                          // Add fractional part to 1.0

	float  f = asfloat(m);       // Range [1:2]
	return f - 1.0;              // Range [0:1]
}

// Pseudo-random value in half-open range [0:1].
float _random(float x) { return floatConstruct(hash(asuint(x))); }
float _random(float2 v) { return floatConstruct(hash(asuint(v))); }
float _random(float3 v) { return floatConstruct(hash(asuint(v))); }
float _random(float4 v) { return floatConstruct(hash(asuint(v))); }

float4 LoadOtfBuf(const in int sample_value, const in Buffer<float4> buf_otf, const in float opacity_correction)
{
    float4 vis_otf = buf_otf[sample_value];
    vis_otf.a *= opacity_correction;
    vis_otf.rgb *= vis_otf.a; // associate color
    return vis_otf;
}

float4 LoadSlabOtfBuf_Avg(const in int sample_v, const in int sample_prev, const in Buffer<float4> buf_otf, const in float opacity_correction)
{
	float4 vis_otf = (buf_otf[sample_prev] + buf_otf[sample_v]) * 0.5f;
	vis_otf.a *= opacity_correction;
	vis_otf.rgb *= vis_otf.a; // associate color
	return vis_otf;
}

float4 LoadSlabOtfBuf_PreInt(const in int sample_v, const in int sample_prev, const in Buffer<float4> buf_preintotf, const in float opacity_correction)
{
	float4 vis_otf = (float4)0;

	int diff = sample_v - sample_prev;
	if (diff == 0) diff = 1;

	float divDiff = 1.f / (float)diff;

	float4 f4OtfColorPrev = buf_preintotf[sample_prev];
	float4 f4OtfColorNext = buf_preintotf[sample_v];

	vis_otf.rgb = (f4OtfColorNext.rgb - f4OtfColorPrev.rgb) * divDiff;
	//vis_otf.a = 1.f - exp(-(f4OtfColorNext.a - f4OtfColorPrev.a) * divDiff * opacity_correction)
	vis_otf.a = (f4OtfColorNext.a - f4OtfColorPrev.a) * divDiff * opacity_correction;
	vis_otf.rgb *= vis_otf.a; // associate color
	return vis_otf;
}

float4 LoadOtfBufId(const in int sample_value, const in Buffer<float4> buf_otf, const in float opacity_correction, const in int id)
{
    float4 vis_otf = buf_otf[sample_value + id * g_cbTmap.tmap_size_x];
    vis_otf.a *= opacity_correction;
    vis_otf.rgb *= vis_otf.a; // associate color
    return vis_otf;
}

int LoadMaxValueInt(float3 pos_ts, float3 size_tex3d, const int scale, const in Texture3D tex3d_data)
{
    float3 pos_vs = float3(pos_ts.x * size_tex3d.x - 0.5f, pos_ts.y * size_tex3d.y - 0.5f, pos_ts.z * size_tex3d.z - 0.5f);
    int3 idx_vs = int3(pos_vs);

    float samples[8];
    samples[0] = tex3d_data.Load(int4(idx_vs, 0)).r;
    samples[1] = tex3d_data.Load(int4(idx_vs + int3(1, 0, 0), 0)).r;
    samples[2] = tex3d_data.Load(int4(idx_vs + int3(0, 1, 0), 0)).r;
    samples[3] = tex3d_data.Load(int4(idx_vs + int3(1, 1, 0), 0)).r;
    samples[4] = tex3d_data.Load(int4(idx_vs + int3(0, 0, 1), 0)).r;
    samples[5] = tex3d_data.Load(int4(idx_vs + int3(1, 0, 1), 0)).r;
    samples[6] = tex3d_data.Load(int4(idx_vs + int3(0, 1, 1), 0)).r;
    samples[7] = tex3d_data.Load(int4(idx_vs + int3(1, 1, 1), 0)).r;
	
    float max_v = 0;
    for (int i = 0; i < 8; i++)
    {
        max_v = max(samples[i], max_v);
    }
    return (int) (max_v * scale + 0.5f);
}

struct BlockSkip
{
    int blk_value;
    int num_skip_steps;
};
BlockSkip ComputeBlockSkip(const in float3 pos_start_ts, const in float3 vec_sample_ts, const in float3 size_volblk_ts, const in float volblk_value_range, const in Texture3D tex3d_blk_data)
{
    BlockSkip blk_v = (BlockSkip) 0;
    //int3 blk_id = int3(pos_start_ts.x / size_volblk_ts.x, pos_start_ts.y / size_volblk_ts.y, pos_start_ts.z / size_volblk_ts.z);
    int3 blk_id = pos_start_ts / size_volblk_ts;
    blk_v.blk_value = (int) (tex3d_blk_data.Load(int4(blk_id, 0)).r * volblk_value_range + 0.5f);
    
    float3 pos_min_ts = float3(blk_id.x * size_volblk_ts.x, blk_id.y * size_volblk_ts.y, blk_id.z * size_volblk_ts.z);
    float3 pos_max_ts = pos_min_ts + size_volblk_ts;
    float2 hits_t = ComputeAaBbHits(pos_start_ts, pos_min_ts, pos_max_ts, vec_sample_ts);
    float dist_skip_ts = hits_t.y - hits_t.x;
    blk_v.num_skip_steps = ceil(dist_skip_ts);
    return blk_v;
};

float3 GradientVolume(const in float3 pos_sample_ts, const in float3 vec_x, const in float3 vec_y, const in float3 vec_z, const in Texture3D tex3d_data)
{
    float fGx = tex3d_data.SampleLevel(g_samplerLinear_clamp, pos_sample_ts + vec_x, 0).r
		- tex3d_data.SampleLevel(g_samplerLinear_clamp, pos_sample_ts - vec_x, 0).r;
    float fGy = tex3d_data.SampleLevel(g_samplerLinear_clamp, pos_sample_ts + vec_y, 0).r
		- tex3d_data.SampleLevel(g_samplerLinear_clamp, pos_sample_ts - vec_y, 0).r;
    float fGz = tex3d_data.SampleLevel(g_samplerLinear_clamp, pos_sample_ts + vec_z, 0).r
		- tex3d_data.SampleLevel(g_samplerLinear_clamp, pos_sample_ts - vec_z, 0).r;
    return float3(fGx, fGy, fGz);
}

float3 GradientVolume2(const in float3 pos_sample_ts, const in float3 vec_x, const in float3 vec_y, const in float3 vec_z, const in Texture3D tex3d_data)
{
	float fGx = tex3d_data.SampleLevel(g_samplerLinear_clamp, pos_sample_ts + vec_x, 0).r
		- tex3d_data.SampleLevel(g_samplerLinear_clamp, pos_sample_ts - vec_x, 0).r;
	float fGy = tex3d_data.SampleLevel(g_samplerLinear_clamp, pos_sample_ts + vec_y, 0).r
		- tex3d_data.SampleLevel(g_samplerLinear_clamp, pos_sample_ts - vec_y, 0).r;
	float fGz = tex3d_data.SampleLevel(g_samplerLinear_clamp, pos_sample_ts + vec_z, 0).r
		- tex3d_data.SampleLevel(g_samplerLinear_clamp, pos_sample_ts - vec_z, 0).r;
	return float3(fGx, fGy, fGz);
}

float3 GradientNormalVolume(inout float leng, const in float3 pos_sample_ts, const in float3 pos_view_ws, const in float3 vec_x, const in float3 vec_y, const in float3 vec_z, const in Texture3D tex3d_data)
{
    float3 grad = GradientVolume(pos_sample_ts, vec_x, vec_y, vec_z, tex3d_data);
    leng = length(grad);
	float3 nrl = (float3) 0;
	if (leng > 0)
	{
		if (dot(grad, pos_view_ws) > 0)
			grad *= -1.f;
		nrl = grad / leng;
	}
    return nrl;
}

//==================================
// z-thickness blending core
//==================================
struct RaySegment
{
    float4 fvis;
    float zdepth;
    float zthick;
};

struct MergeRS_OUT
{
    RaySegment rs_prior; // Includes current intermixed depth
    RaySegment rs_posterior;
};

// rs_prior and rs_posterior mean rs_prior.zdepth <= rs_posterior.zdepth
MergeRS_OUT MergeRS(RaySegment rs_prior, RaySegment rs_posterior)
{
	// Overall algorithm computation cost 
	// : 3 branches, 2 visibility interpolations, 2 visibility integrations, and 1 fusion of overlapping ray-segments

	// Safe-Check to avoid zero-division
    rs_prior.fvis.a = min(rs_prior.fvis.a, SAFE_OPAQUEALPHA);
    rs_posterior.fvis.a = min(rs_posterior.fvis.a, SAFE_OPAQUEALPHA);

	// rs_prior and rs_posterior mean rs_prior.zdepth >= rs_prior.zdepth
    MergeRS_OUT rs_out;

    float zfront_posterior_f = rs_posterior.zdepth - rs_posterior.zthick;
    if (rs_prior.zdepth <= zfront_posterior_f)
    {
		// Case 1 : No Overlapping
        rs_out.rs_prior = rs_prior;
    }
    else
    {
        float zfront_prior_f = rs_prior.zdepth - rs_prior.zthick;
        if (zfront_prior_f <= zfront_posterior_f)
        {
			// Case 2 : Intersecting each other
            //rs_out.rs_prior.zthick = max(zfront_posterior_f - zfront_prior_f, 0); // to avoid computational precision error (0 or small minus)
            rs_out.rs_prior.zthick = zfront_posterior_f - zfront_prior_f;

            rs_out.rs_prior.zdepth = zfront_posterior_f;

            rs_out.rs_prior.fvis = rs_prior.fvis * (rs_out.rs_prior.zthick / rs_prior.zthick);
            
            rs_prior.zthick -= rs_out.rs_prior.zthick;
            rs_prior.fvis = (rs_prior.fvis - rs_out.rs_prior.fvis) / (1.f - rs_out.rs_prior.fvis.a);
        }
        else
        {
			// Case 3 : rs_prior belongs to rs_posterior
            //rs_out.rs_prior.zthick = max(zfront_prior_f - zfront_posterior_f, 0); // to avoid computational precision error (0 or small minus)
            rs_out.rs_prior.zthick = zfront_prior_f - zfront_posterior_f;
            rs_out.rs_prior.zdepth = zfront_prior_f;

            rs_out.rs_prior.fvis = rs_posterior.fvis * (rs_out.rs_prior.zthick / rs_posterior.zthick);

            rs_posterior.zthick -= rs_out.rs_prior.zthick;
            rs_posterior.fvis = (rs_posterior.fvis - rs_out.rs_prior.fvis) / (1.f - rs_out.rs_prior.fvis.a);
        }
        
        // merge the fusion sub_rs (rs_prior) to rs_out.rs_prior
        rs_out.rs_prior.zthick += rs_prior.zthick;
        rs_out.rs_prior.zdepth = rs_prior.zdepth;
        float4 fvis_front_sub_rs = rs_posterior.fvis * (rs_prior.zthick / rs_posterior.zthick); // REDESIGN
        float4 fvis_fusion_sub_rs = BlendFloat4AndFloat4(fvis_front_sub_rs, rs_prior.fvis);
        rs_out.rs_prior.fvis += fvis_fusion_sub_rs * (1.f - rs_out.rs_prior.fvis.a);

        rs_posterior.zthick -= rs_prior.zthick;
        rs_posterior.fvis = (rs_posterior.fvis - fvis_front_sub_rs) / (1.f - fvis_front_sub_rs.a);
    }
    rs_out.rs_posterior = rs_posterior;
	// Safe-Check For Byte Precision //
    if (rs_out.rs_prior.fvis.a < FLT_OPACITY_MIN__) // DO NOT COUNT OPERATION OF INTERMIXING KERNEL !
    {
        rs_out.rs_prior.fvis = (float4) 0;
        rs_out.rs_prior.zthick = 0;
    }
    if (rs_out.rs_posterior.fvis.a < FLT_OPACITY_MIN__) // DO NOT COUNT OPERATION OF INTERMIXING KERNEL !
    {
        rs_out.rs_posterior.fvis = (float4) 0;
        rs_out.rs_posterior.zthick = 0;
    }
    return rs_out;
}

int MergeFragRS(inout RaySegment rs_merge, RaySegment rs_1, RaySegment rs_2)
{
    RaySegment rs_prior, rs_posterior;
    if (rs_1.zdepth > rs_2.zdepth)
    {
        rs_prior = rs_2;
        rs_posterior = rs_1;
    }
    else
    {
        rs_prior = rs_1;
        rs_posterior = rs_2;
    }
    
	// Overall algorithm computation cost 
	// : 3 branches, 2 visibility interpolations, 2 visibility integrations, and 1 fusion of overlapping ray-segments

	// Safe-Check to avoid zero-division
    rs_prior.fvis.a = min(rs_prior.fvis.a, SAFE_OPAQUEALPHA);
    rs_posterior.fvis.a = min(rs_posterior.fvis.a, SAFE_OPAQUEALPHA);

	// rs_prior and rs_posterior mean rs_prior.zdepth >= rs_prior.zdepth

    float zfront_posterior_f = rs_posterior.zdepth - rs_posterior.zthick;
    int ret = 0;
    if (rs_prior.zdepth <= zfront_posterior_f)
    {
		// Case 1 : No Overlapping
        //rs_merge = rs_2;
        ret = 1;
    }
    else
    {
        MergeRS_OUT rs_out;
        float zfront_prior_f = rs_prior.zdepth - rs_prior.zthick;
        if (zfront_prior_f <= zfront_posterior_f)
        {
			// Case 2 : Intersecting each other
            //rs_out.rs_prior.zthick = max(zfront_posterior_f - zfront_prior_f, 0); // to avoid computational precision error (0 or small minus)
            rs_out.rs_prior.zthick = zfront_posterior_f - zfront_prior_f;

            rs_out.rs_prior.zdepth = zfront_posterior_f;

#define SIMPLE_MERGE 1
#ifdef SIMPLE_MERGE
            rs_out.rs_prior.fvis = rs_prior.fvis * (rs_out.rs_prior.zthick / rs_prior.zthick);
#else
            float _a = 1.f - pow(1.f - rs_prior.fvis.a, rs_out.rs_prior.zthick / rs_prior.zthick);
            rs_out.rs_prior.fvis = float4(rs_prior.fvis.rgb * _a / rs_prior.fvis.a, _a);
#endif
            
            rs_prior.zthick -= rs_out.rs_prior.zthick;
            rs_prior.fvis = (rs_prior.fvis - rs_out.rs_prior.fvis) / (1.f - rs_out.rs_prior.fvis.a);
        }
        else
        {
			// Case 3 : rs_prior belongs to rs_posterior
            //rs_out.rs_prior.zthick = max(zfront_prior_f - zfront_posterior_f, 0); // to avoid computational precision error (0 or small minus)
            rs_out.rs_prior.zthick = zfront_prior_f - zfront_posterior_f;
            rs_out.rs_prior.zdepth = zfront_prior_f;
            
#ifdef SIMPLE_MERGE
            rs_out.rs_prior.fvis = rs_posterior.fvis * (rs_out.rs_prior.zthick / rs_posterior.zthick);
#else
            float _a = 1.f - pow(1.f - rs_posterior.fvis.a, rs_out.rs_prior.zthick / rs_posterior.zthick);
            rs_out.rs_prior.fvis = float4(rs_posterior.fvis.rgb * _a / rs_posterior.fvis.a, _a);
#endif

            rs_posterior.zthick -= rs_out.rs_prior.zthick;
            rs_posterior.fvis = (rs_posterior.fvis - rs_out.rs_prior.fvis) / (1.f - rs_out.rs_prior.fvis.a);
        }
        
        // merge the fusion sub_rs (rs_prior) to rs_out.rs_prior
        rs_out.rs_prior.zthick += rs_prior.zthick;
        rs_out.rs_prior.zdepth = rs_prior.zdepth;
        float4 fvis_front_sub_rs = rs_posterior.fvis * (rs_prior.zthick / rs_posterior.zthick); // REDESIGN
        float4 fvis_fusion_sub_rs = BlendFloat4AndFloat4(fvis_front_sub_rs, rs_prior.fvis);
        rs_out.rs_prior.fvis += fvis_fusion_sub_rs * (1.f - rs_out.rs_prior.fvis.a);
        
        rs_posterior.zthick -= rs_prior.zthick;
        rs_posterior.fvis = (rs_posterior.fvis - fvis_front_sub_rs) / (1.f - fvis_front_sub_rs.a);
        rs_out.rs_posterior = rs_posterior;
        
        rs_merge.zdepth = rs_out.rs_posterior.zdepth;
        rs_merge.zthick = rs_out.rs_posterior.zthick + rs_out.rs_prior.zthick;
        rs_merge.fvis = rs_out.rs_prior.fvis + rs_out.rs_posterior.fvis * (1.f - rs_out.rs_prior.fvis.a);
    }
    if (rs_merge.fvis.a < FLT_OPACITY_MIN__) // DO NOT COUNT OPERATION OF INTERMIXING KERNEL !
    {
        rs_merge.fvis = (float4) 0;
        rs_merge.zthick = 0;
    }
    return ret;
}

int MergeFragRS_NV(inout RaySegment rs_1, inout RaySegment rs_2)
{
    RaySegment rs_prior, rs_posterior;
    if (rs_1.zdepth > rs_2.zdepth)
    {
        rs_prior = rs_2;
        rs_posterior = rs_1;
    }
    else
    {
        rs_prior = rs_1;
        rs_posterior = rs_2;
    }
    
	// Overall algorithm computation cost 
	// : 3 branches, 2 visibility interpolations, 2 visibility integrations, and 1 fusion of overlapping ray-segments

	// Safe-Check to avoid zero-division
    rs_prior.fvis.a = min(rs_prior.fvis.a, SAFE_OPAQUEALPHA);
    rs_posterior.fvis.a = min(rs_posterior.fvis.a, SAFE_OPAQUEALPHA);

	// rs_prior and rs_posterior mean rs_prior.zdepth >= rs_prior.zdepth

    float zfront_posterior_f = rs_posterior.zdepth - rs_posterior.zthick;
    int ret = 0;
    MergeRS_OUT rs_out;
    if (rs_prior.zdepth <= zfront_posterior_f)
    {
		// Case 1 : No Overlapping
        //rs_merge = rs_2;
        ret = 1;
    }
    else
    {
        float zfront_prior_f = rs_prior.zdepth - rs_prior.zthick;
        if (zfront_prior_f <= zfront_posterior_f)
        {
			// Case 2 : Intersecting each other
            //rs_out.rs_prior.zthick = max(zfront_posterior_f - zfront_prior_f, 0); // to avoid computational precision error (0 or small minus)
            rs_out.rs_prior.zthick = zfront_posterior_f - zfront_prior_f;

            rs_out.rs_prior.zdepth = zfront_posterior_f;

#define SIMPLE_MERGE 1
#ifdef SIMPLE_MERGE
            rs_out.rs_prior.fvis = rs_prior.fvis * (rs_out.rs_prior.zthick / rs_prior.zthick);
#else
            float _a = 1.f - pow(1.f - rs_prior.fvis.a, rs_out.rs_prior.zthick / rs_prior.zthick);
            rs_out.rs_prior.fvis = float4(rs_prior.fvis.rgb * _a / rs_prior.fvis.a, _a);
#endif
            
            rs_prior.zthick -= rs_out.rs_prior.zthick;
            rs_prior.fvis = (rs_prior.fvis - rs_out.rs_prior.fvis) / (1.f - rs_out.rs_prior.fvis.a);
        }
        else
        {
			// Case 3 : rs_prior belongs to rs_posterior
            //rs_out.rs_prior.zthick = max(zfront_prior_f - zfront_posterior_f, 0); // to avoid computational precision error (0 or small minus)
            rs_out.rs_prior.zthick = zfront_prior_f - zfront_posterior_f;
            rs_out.rs_prior.zdepth = zfront_prior_f;
            
#ifdef SIMPLE_MERGE
            rs_out.rs_prior.fvis = rs_posterior.fvis * (rs_out.rs_prior.zthick / rs_posterior.zthick);
#else
            float _a = 1.f - pow(1.f - rs_posterior.fvis.a, rs_out.rs_prior.zthick / rs_posterior.zthick);
            rs_out.rs_prior.fvis = float4(rs_posterior.fvis.rgb * _a / rs_posterior.fvis.a, _a);
#endif

            rs_posterior.zthick -= rs_out.rs_prior.zthick;
            rs_posterior.fvis = (rs_posterior.fvis - rs_out.rs_prior.fvis) / (1.f - rs_out.rs_prior.fvis.a);
        }
        
        // merge the fusion sub_rs (rs_prior) to rs_out.rs_prior
        rs_out.rs_prior.zthick += rs_prior.zthick;
        rs_out.rs_prior.zdepth = rs_prior.zdepth;
        float4 fvis_front_sub_rs = rs_posterior.fvis * (rs_prior.zthick / rs_posterior.zthick); // REDESIGN
        float4 fvis_fusion_sub_rs = BlendFloat4AndFloat4(fvis_front_sub_rs, rs_prior.fvis);
        rs_out.rs_prior.fvis += fvis_fusion_sub_rs * (1.f - rs_out.rs_prior.fvis.a);
        
        rs_posterior.zthick -= rs_prior.zthick;
        rs_posterior.fvis = (rs_posterior.fvis - fvis_front_sub_rs) / (1.f - fvis_front_sub_rs.a);
    }
    rs_out.rs_posterior = rs_posterior;
	// Safe-Check For Byte Precision //
    if (rs_out.rs_prior.fvis.a < FLT_OPACITY_MIN__) // DO NOT COUNT OPERATION OF INTERMIXING KERNEL !
    {
        rs_out.rs_prior.fvis = (float4) 0;
        rs_out.rs_prior.zthick = 0;
    }
    if (rs_out.rs_posterior.fvis.a < FLT_OPACITY_MIN__) // DO NOT COUNT OPERATION OF INTERMIXING KERNEL !
    {
        rs_out.rs_posterior.fvis = (float4) 0;
        rs_out.rs_posterior.zthick = 0;
    }
    if (rs_1.zdepth > rs_2.zdepth)
    {
        rs_2 = rs_out.rs_prior;
        rs_1 = rs_out.rs_posterior;
    }
    else
    {
        rs_1 = rs_out.rs_prior;
        rs_2 = rs_out.rs_posterior;
    }
    return ret;
}

struct RaySegment2
{
    float4 fvis;
    float zdepth;
    float zthick;
    float alphaw;
};

struct MergeRS_OUT2
{
    RaySegment2 rs_prior; // Includes current intermixed depth
    RaySegment2 rs_posterior;
};

float4 MixOpt(const in float4 vis1, const in float alphaw1, const in float4 vis2, const in float alphaw2)
{
	float4 vout = (float4)0;
	if (alphaw1 + alphaw2 > 0)
	{
		float3 C_mix1 = vis1.rgb / vis1.a * alphaw1;
		float3 C_mix2 = vis2.rgb / vis2.a * alphaw2;
		float3 I_mix = (C_mix1 + C_mix2) / (alphaw1 + alphaw2);
		float T_mix1 = 1 - vis1.a;
		float T_mix2 = 1 - vis2.a;
		float A_mix = 1 - T_mix1 * T_mix2;
		vout = float4(I_mix * A_mix, A_mix);
	}
	return vout;
    //return vis1 + vis2 - (vis1 * vis2.a + vis2 * vis1.a) / 2.f;
    //float4 color_merge;
    //float _a_sum = fColor1.a + fColor2.a;
    //color_merge.a = fColor1.a + fColor2.a - fColor1.a * fColor2.a;
    //color_merge.rgb = (fColor1.rgb + fColor2.rgb) / _a_sum * color_merge.a;
    //return color_merge;
}

MergeRS_OUT2 MergeRS2(RaySegment2 rs_prior, RaySegment2 rs_posterior, const in float beta)
{
	// Overall algorithm computation cost 
	// : 3 branches, 2 visibility interpolations, 2 visibility integrations, and 1 fusion of overlapping ray-segments

	// Safe-Check to avoid zero-division
    rs_prior.fvis.a = min(rs_prior.fvis.a, SAFE_OPAQUEALPHA);
    rs_posterior.fvis.a = min(rs_posterior.fvis.a, SAFE_OPAQUEALPHA);

	// rs_prior and rs_posterior mean rs_prior.zdepth >= rs_prior.zdepth
    MergeRS_OUT2 rs_out;

    float zfront_posterior_f = rs_posterior.zdepth - rs_posterior.zthick;
    if (rs_prior.zdepth <= zfront_posterior_f)
    {
		// Case 1 : No Overlapping
        rs_out.rs_prior = rs_prior;
    }
    else
    {
        float zfront_prior_f = rs_prior.zdepth - rs_prior.zthick;
        if (zfront_prior_f <= zfront_posterior_f)
        {
			// Case 2 : Intersecting each other
            //rs_out.rs_prior.zthick = max(zfront_posterior_f - zfront_prior_f, 0); // to avoid computational precision error (0 or small minus)
            rs_out.rs_prior.zthick = zfront_posterior_f - zfront_prior_f;
            rs_out.rs_prior.zdepth = zfront_posterior_f;
			// if simple linear mode
			{
				//rs_out.rs_prior.fvis = rs_prior.fvis * (rs_out.rs_prior.zthick / rs_prior.zthick); 
			}
			// else
			{
				float zd_ratio = rs_out.rs_prior.zthick / rs_prior.zthick;
				float Ad = rs_prior.fvis.a;
				float3 Id = rs_prior.fvis.rgb / Ad;

				// strict mode
				float Az = Ad * zd_ratio * beta + (1 - beta) * (1 - pow(abs(1 - Ad), zd_ratio));
				// approx. mode
				//float term1 = zd_ratio * (1 - zd_ratio) / 2.f * Ad * Ad;
				//float term2 = term1 * (2 - zd_ratio) / 3.f * Ad;
				//float term3 = term2 * (3 - zd_ratio) / 4.f * Ad;
				//float term4 = term3 * (4 - zd_ratio) / 5.f * Ad;
				//float Az = Ad * zd_ratio + (1 - beta) * (term1 + term2 + term3 + term4 + term4 * (5 - zd_ratio) / 6.f * Ad);

				float3 Cz = Id * Az;
				rs_out.rs_prior.fvis = float4(Cz, Az);
			}
            
            float old_alpha = rs_prior.fvis.a;
            rs_prior.zthick -= rs_out.rs_prior.zthick;
            rs_prior.fvis = (rs_prior.fvis - rs_out.rs_prior.fvis) / (1.f - rs_out.rs_prior.fvis.a);

            rs_out.rs_prior.alphaw = rs_prior.alphaw * rs_out.rs_prior.fvis.a / old_alpha;
            rs_prior.alphaw = rs_prior.alphaw * rs_prior.fvis.a / old_alpha;
        }
        else
        {
			// Case 3 : rs_prior belongs to rs_posterior
            //rs_out.rs_prior.zthick = max(zfront_prior_f - zfront_posterior_f, 0); // to avoid computational precision error (0 or small minus)
            rs_out.rs_prior.zthick = zfront_prior_f - zfront_posterior_f;
            rs_out.rs_prior.zdepth = zfront_prior_f;

			// if simple linear mode
			{
				//rs_out.rs_prior.fvis = rs_posterior.fvis * (rs_out.rs_prior.zthick / rs_posterior.zthick); 
			}
			// else
			{
				float zd_ratio = rs_out.rs_prior.zthick / rs_posterior.zthick;
				float Ad = rs_posterior.fvis.a;
				float3 Id = rs_posterior.fvis.rgb / Ad;

				// strict mode
				float Az = Ad * zd_ratio * beta + (1 - beta) * (1 - pow(abs(1 - Ad), zd_ratio));
				// approx. mode
				//float term1 = zd_ratio * (1 - zd_ratio) / 2.f * Ad * Ad;
				//float term2 = term1 * (2 - zd_ratio) / 3.f * Ad;
				//float term3 = term2 * (3 - zd_ratio) / 4.f * Ad;
				//float term4 = term3 * (4 - zd_ratio) / 5.f * Ad;
				//float Az = Ad * zd_ratio + (1 - beta) * (term1 + term2 + term3 + term4 + term4 * (5 - zd_ratio) / 6.f * Ad);

				float3 Cz = Id * Az;
				rs_out.rs_prior.fvis = float4(Cz, Az);
			}
            
            float old_alpha = rs_posterior.fvis.a;
            rs_posterior.zthick -= rs_out.rs_prior.zthick;
            rs_posterior.fvis = (rs_posterior.fvis - rs_out.rs_prior.fvis) / (1.f - rs_out.rs_prior.fvis.a);

            rs_out.rs_prior.alphaw = rs_posterior.alphaw * rs_out.rs_prior.fvis.a / old_alpha;
            rs_posterior.alphaw = rs_posterior.alphaw * rs_posterior.fvis.a / old_alpha;
        }
        
        // merge the fusion sub_rs (rs_prior) to rs_out.rs_prior
        rs_out.rs_prior.zthick += rs_prior.zthick;
        rs_out.rs_prior.zdepth = rs_prior.zdepth;
        float4 fvis_front_sub_rs = rs_posterior.fvis * (rs_prior.zthick / rs_posterior.zthick); // REDESIGN
        float alphaw_front_sub_rs = rs_posterior.alphaw * fvis_front_sub_rs.a / rs_posterior.fvis.a;
        //float4 fvis_fusion_sub_rs = BlendFloat4AndFloat4(fvis_front_sub_rs, rs_prior.fvis);
        float4 fvis_fusion_sub_rs = MixOpt(fvis_front_sub_rs, alphaw_front_sub_rs, rs_prior.fvis, rs_prior.alphaw);
        rs_out.rs_prior.fvis += fvis_fusion_sub_rs * (1.f - rs_out.rs_prior.fvis.a);
        rs_out.rs_prior.alphaw += alphaw_front_sub_rs + rs_prior.alphaw;

        rs_posterior.zthick -= rs_prior.zthick;
        float old_alpha = rs_posterior.fvis.a;
        rs_posterior.fvis = (rs_posterior.fvis - fvis_front_sub_rs) / (1.f - fvis_front_sub_rs.a);
        rs_posterior.alphaw *= rs_posterior.fvis.a / old_alpha;


		//rs_out.rs_prior.fvis = float4(1, 1, 0, 1);
    }
    rs_out.rs_posterior = rs_posterior;
	// Safe-Check For Byte Precision //
    if (rs_out.rs_prior.fvis.a < FLT_OPACITY_MIN__) // DO NOT COUNT OPERATION OF INTERMIXING KERNEL !
    {
        rs_out.rs_prior.fvis = (float4) 0;
        rs_out.rs_prior.zthick = 0;
    }
    if (rs_out.rs_posterior.fvis.a < FLT_OPACITY_MIN__) // DO NOT COUNT OPERATION OF INTERMIXING KERNEL !
    {
        rs_out.rs_posterior.fvis = (float4) 0;
        rs_out.rs_posterior.zthick = 0;
    }
    return rs_out;
}

int MergeFragRS2(inout RaySegment2 rs_merge, RaySegment2 rs_1, RaySegment2 rs_2)
{
    RaySegment2 rs_prior, rs_posterior;
    if (rs_1.zdepth > rs_2.zdepth)
    {
        rs_prior = rs_2;
        rs_posterior = rs_1;
    }
    else
    {
        rs_prior = rs_1;
        rs_posterior = rs_2;
    }
    
	// Overall algorithm computation cost 
	// : 3 branches, 2 visibility interpolations, 2 visibility integrations, and 1 fusion of overlapping ray-segments

	// Safe-Check to avoid zero-division
    rs_prior.fvis.a = min(rs_prior.fvis.a, SAFE_OPAQUEALPHA);
    rs_posterior.fvis.a = min(rs_posterior.fvis.a, SAFE_OPAQUEALPHA);

    float zfront_posterior_f = rs_posterior.zdepth - rs_posterior.zthick;
    int ret = 0;
    if (rs_prior.zdepth <= zfront_posterior_f)
    {
		// Case 1 : No Overlapping
        //rs_merge = rs_2;
        ret = 1;
    }
    else
    {
        MergeRS_OUT2 rs_out;
        float zfront_prior_f = rs_prior.zdepth - rs_prior.zthick;
        if (zfront_prior_f <= zfront_posterior_f)
        {
			// Case 2 : Intersecting each other
            //rs_out.rs_prior.zthick = max(zfront_posterior_f - zfront_prior_f, 0); // to avoid computational precision error (0 or small minus)
            rs_out.rs_prior.zthick = zfront_posterior_f - zfront_prior_f;
            rs_out.rs_prior.zdepth = zfront_posterior_f;

//#define SIMPLE_MERGE 1
#ifdef SIMPLE_MERGE
            rs_out.rs_prior.fvis = rs_prior.fvis * (rs_out.rs_prior.zthick / rs_prior.zthick);
#else
            float _a = 1.f - pow(1.f - rs_prior.fvis.a, rs_out.rs_prior.zthick / rs_prior.zthick);
            rs_out.rs_prior.fvis = float4(rs_prior.fvis.rgb * _a / rs_prior.fvis.a, _a);
#endif      
            float old_alpha = rs_prior.fvis.a;
            rs_prior.zthick -= rs_out.rs_prior.zthick;
            rs_prior.fvis = (rs_prior.fvis - rs_out.rs_prior.fvis) / (1.f - rs_out.rs_prior.fvis.a);
            
            rs_out.rs_prior.alphaw = rs_prior.alphaw * rs_out.rs_prior.fvis.a / old_alpha;
            rs_prior.alphaw = rs_prior.alphaw * rs_prior.fvis.a / old_alpha;
        }
        else
        {
			// Case 3 : rs_prior belongs to rs_posterior
            //rs_out.rs_prior.zthick = max(zfront_prior_f - zfront_posterior_f, 0); // to avoid computational precision error (0 or small minus)
            rs_out.rs_prior.zthick = zfront_prior_f - zfront_posterior_f;
            rs_out.rs_prior.zdepth = zfront_prior_f;
            
#ifdef SIMPLE_MERGE
            rs_out.rs_prior.fvis = rs_posterior.fvis * (rs_out.rs_prior.zthick / rs_posterior.zthick);
#else
            float _a = 1.f - pow(1.f - rs_posterior.fvis.a, rs_out.rs_prior.zthick / rs_posterior.zthick);
            rs_out.rs_prior.fvis = float4(rs_posterior.fvis.rgb * _a / rs_posterior.fvis.a, _a);
#endif
            
            float old_alpha = rs_posterior.fvis.a;
            rs_posterior.zthick -= rs_out.rs_prior.zthick;
            rs_posterior.fvis = (rs_posterior.fvis - rs_out.rs_prior.fvis) / (1.f - rs_out.rs_prior.fvis.a);
            
            rs_out.rs_prior.alphaw = rs_posterior.alphaw * rs_out.rs_prior.fvis.a / old_alpha;
            rs_posterior.alphaw = rs_posterior.alphaw * rs_posterior.fvis.a / old_alpha;
        }
        
        // merge the fusion sub_rs (rs_prior) to rs_out.rs_prior
        rs_out.rs_prior.zthick += rs_prior.zthick;
        rs_out.rs_prior.zdepth = rs_prior.zdepth;
        float4 fvis_front_sub_rs = rs_posterior.fvis * (rs_prior.zthick / rs_posterior.zthick); // REDESIGN
        float alphaw_front_sub_rs = rs_posterior.alphaw * fvis_front_sub_rs.a / rs_posterior.fvis.a;
        float4 fvis_fusion_sub_rs = MixOpt(fvis_front_sub_rs, alphaw_front_sub_rs, rs_prior.fvis, rs_prior.alphaw);
        rs_out.rs_prior.fvis += fvis_fusion_sub_rs * (1.f - rs_out.rs_prior.fvis.a);
        rs_out.rs_prior.alphaw += alphaw_front_sub_rs + rs_prior.alphaw;
        
        rs_posterior.zthick -= rs_prior.zthick;
        rs_posterior.fvis = (rs_posterior.fvis - fvis_front_sub_rs) / (1.f - fvis_front_sub_rs.a);
        rs_out.rs_posterior = rs_posterior;
        
        rs_merge.zdepth = rs_out.rs_posterior.zdepth;
        rs_merge.zthick = rs_out.rs_posterior.zthick + rs_out.rs_prior.zthick;
        rs_merge.fvis = rs_out.rs_prior.fvis + rs_out.rs_posterior.fvis * (1.f - rs_out.rs_prior.fvis.a);
        rs_merge.alphaw = rs_out.rs_prior.alphaw + rs_out.rs_posterior.alphaw;
    }
    if (rs_merge.fvis.a < FLT_OPACITY_MIN__) // DO NOT COUNT OPERATION OF INTERMIXING KERNEL !
    {
        rs_merge.fvis = (float4) 0;
        rs_merge.zthick = 0;
        rs_merge.alphaw = 0;
    }
    return ret;
}

int MergeFragRS_Avr(inout RaySegment2 rs_merge, RaySegment2 rs_1, RaySegment2 rs_2)
{
    RaySegment2 rs_prior, rs_posterior;
    if (rs_1.zdepth >= rs_2.zdepth)
    {
        rs_prior = rs_2;
        rs_posterior = rs_1;
    }
    else
    {
        rs_prior = rs_1;
        rs_posterior = rs_2;
    }

    float zfront_posterior_f = rs_posterior.zdepth - rs_posterior.zthick;
    int ret = 0;
    if (rs_prior.zdepth < zfront_posterior_f)
    {
		// Case 1 : No Overlapping
        //rs_merge = rs_2;
        ret = 1;
    }
    else
    {
        rs_merge.fvis = MixOpt(rs_prior.fvis, rs_prior.alphaw, rs_posterior.fvis, rs_posterior.alphaw);
        rs_merge.zdepth = rs_posterior.zdepth;
        rs_merge.zthick = rs_posterior.zdepth - (rs_prior.zdepth - rs_prior.zthick);
        rs_merge.alphaw = rs_prior.alphaw + rs_posterior.alphaw;
    }
    return ret;
}

struct Fragment
{
	uint i_vis;
	float z; // note that the distance along the view-ray, not z value in CS
#if !defined(FRAG_MERGING) || FRAG_MERGING == 1
	float zthick;
	float opacity_sum;
#endif
};

#if !defined(FRAG_MERGING) || FRAG_MERGING == 1
struct Fragment_OUT
{
	Fragment f_prior; // Includes current intermixed depth
	Fragment f_posterior;
};

Fragment_OUT MergeFrags(Fragment f_prior, Fragment f_posterior, const in float beta)
{
	// Overall algorithm computation cost 
	// : 3 branches, 2 visibility interpolations, 2 visibility integrations, and 1 fusion of overlapping ray-segments

	// f_prior and f_posterior mean f_prior.z >= f_prior.z
	Fragment_OUT fs_out;

	float zfront_posterior_f = f_posterior.z - f_posterior.zthick;
	if (f_prior.z <= zfront_posterior_f)
	{
		// Case 1 : No Overlapping
		fs_out.f_prior = f_prior;
	}
	else
	{
		float4 f_m_prior_vis;
		float4 f_prior_vis = ConvertUIntToFloat4(f_prior.i_vis);
		float4 f_posterior_vis = ConvertUIntToFloat4(f_posterior.i_vis);
		f_prior_vis.a = min(f_prior_vis.a, SAFE_OPAQUEALPHA);
		f_posterior_vis.a = min(f_posterior_vis.a, SAFE_OPAQUEALPHA);

		float zfront_prior_f = f_prior.z - f_prior.zthick;
		if (zfront_prior_f <= zfront_posterior_f)
		{
			// Case 2 : Intersecting each other
			//fs_out.f_prior.zthick = max(zfront_posterior_f - zfront_prior_f, 0); // to avoid computational precision error (0 or small minus)
			fs_out.f_prior.zthick = zfront_posterior_f - zfront_prior_f;
			fs_out.f_prior.z = zfront_posterior_f;
#if LINEAR_MODE == 1
			{
				f_m_prior_vis = f_prior_vis * (fs_out.f_prior.zthick / f_prior.zthick);
			}
#else
			{
				float zd_ratio = fs_out.f_prior.zthick / f_prior.zthick;
				float Ad = f_prior_vis.a;
				float3 Id = f_prior_vis.rgb / Ad;

				// strict mode
				float Az = Ad * zd_ratio * beta + (1 - beta) * (1 - pow(abs(1 - Ad), zd_ratio));
				// approx. mode
				//float term1 = zd_ratio * (1 - zd_ratio) / 2.f * Ad * Ad;
				//float term2 = term1 * (2 - zd_ratio) / 3.f * Ad;
				//float term3 = term2 * (3 - zd_ratio) / 4.f * Ad;
				//float term4 = term3 * (4 - zd_ratio) / 5.f * Ad;
				//float Az = Ad * zd_ratio + (1 - beta) * (term1 + term2 + term3 + term4 + term4 * (5 - zd_ratio) / 6.f * Ad);

				float3 Cz = Id * Az;
				f_m_prior_vis = float4(Cz, Az);
			}
#endif

			float old_alpha = f_prior_vis.a;
			f_prior.zthick -= fs_out.f_prior.zthick;
			f_prior_vis = (f_prior_vis - f_m_prior_vis) / (1.f - f_m_prior_vis.a);

			fs_out.f_prior.opacity_sum = f_prior.opacity_sum * f_m_prior_vis.a / old_alpha;
			//f_prior.opacity_sum = f_prior.opacity_sum * f_prior_vis.a / old_alpha;
			f_prior.opacity_sum = f_prior.opacity_sum - fs_out.f_prior.opacity_sum;
		}
		else
		{
			// Case 3 : f_prior belongs to f_posterior
			//fs_out.f_prior.zthick = max(zfront_prior_f - zfront_posterior_f, 0); // to avoid computational precision error (0 or small minus)
			fs_out.f_prior.zthick = zfront_prior_f - zfront_posterior_f;
			fs_out.f_prior.z = zfront_prior_f;

#if LINEAR_MODE == 1
			{
				f_m_prior_vis = f_posterior_vis * (fs_out.f_prior.zthick / f_posterior.zthick); 
			}
#else
			{
				float zd_ratio = fs_out.f_prior.zthick / f_posterior.zthick;
				float Ad = f_posterior_vis.a;
				float3 Id = f_posterior_vis.rgb / Ad;

				// strict mode
				float Az = Ad * zd_ratio * beta + (1 - beta) * (1 - pow(abs(1 - Ad), zd_ratio));
				// approx. mode
				//float term1 = zd_ratio * (1 - zd_ratio) / 2.f * Ad * Ad;
				//float term2 = term1 * (2 - zd_ratio) / 3.f * Ad;
				//float term3 = term2 * (3 - zd_ratio) / 4.f * Ad;
				//float term4 = term3 * (4 - zd_ratio) / 5.f * Ad;
				//float Az = Ad * zd_ratio + (1 - beta) * (term1 + term2 + term3 + term4 + term4 * (5 - zd_ratio) / 6.f * Ad);

				float3 Cz = Id * Az;
				f_m_prior_vis = float4(Cz, Az);
			}
#endif

			float old_alpha = f_posterior_vis.a;
			f_posterior.zthick -= fs_out.f_prior.zthick;
			f_posterior_vis = (f_posterior_vis - f_m_prior_vis) / (1.f - f_m_prior_vis.a);

			fs_out.f_prior.opacity_sum = f_posterior.opacity_sum * f_m_prior_vis.a / old_alpha;
			//f_posterior.opacity_sum = f_posterior.opacity_sum * f_posterior_vis.a / old_alpha;
			f_posterior.opacity_sum = f_posterior.opacity_sum - fs_out.f_prior.opacity_sum;
		}

		// merge the fusion sub_rs (f_prior) to fs_out.f_prior
		fs_out.f_prior.zthick += f_prior.zthick;
		fs_out.f_prior.z = f_prior.z;
		float4 f_mid_vis = f_posterior_vis * (f_prior.zthick / f_posterior.zthick); // REDESIGN
		float f_mid_alphaw = f_posterior.opacity_sum * f_mid_vis.a / f_posterior_vis.a;
		//float4 f_mid_mix_vis = BlendFloat4AndFloat4(f_mid_vis, f_prior_vis);
		float4 f_mid_mix_vis = MixOpt(f_mid_vis, f_mid_alphaw, f_prior_vis, f_prior.opacity_sum);
		f_m_prior_vis += f_mid_mix_vis * (1.f - f_m_prior_vis.a);
		fs_out.f_prior.opacity_sum += f_mid_alphaw + f_prior.opacity_sum;

		f_posterior.zthick -= f_prior.zthick;
		float old_alpha = f_posterior_vis.a;
		f_posterior_vis = (f_posterior_vis - f_mid_vis) / (1.f - f_mid_vis.a);
		//f_posterior.opacity_sum *= f_posterior_vis.a / old_alpha;
		f_posterior.opacity_sum -= f_mid_alphaw;

		// convert to 8b channels
		//f_prior.i_vis = ConvertFloat4ToUInt(f_prior_vis);
		f_posterior.i_vis = ConvertFloat4ToUInt(f_posterior_vis);
		fs_out.f_prior.i_vis = ConvertFloat4ToUInt(f_m_prior_vis);

#define ALPHA_CHECK(IV) ((IV >> 24) > 0)
		if (!ALPHA_CHECK(f_posterior.i_vis))
		{
			f_posterior.i_vis = 0;
			f_posterior.zthick = 0;
		}
		if (!ALPHA_CHECK(fs_out.f_prior.i_vis))
		{
			fs_out.f_prior.i_vis = 0;
			fs_out.f_prior.zthick = 0;
		}
	}
	fs_out.f_posterior = f_posterior;
	return fs_out;
}

Fragment MergeFrags_ver2_test(Fragment f_prior, Fragment f_posterior, const in float beta)
{
	// Overall algorithm computation cost 
	// : 3 branches, 2 visibility interpolations, 2 visibility integrations, and 1 fusion of overlapping ray-segments

	// f_prior and f_posterior mean f_prior.z >= f_prior.z
	Fragment fs_m;
	Fragment_OUT fs_out;

	float zfront_posterior_f = f_posterior.z - f_posterior.zthick;
	if (f_prior.z <= zfront_posterior_f)
	{
		// Case 1 : No Overlapping
		return (Fragment)0;
	}

	float4 f_m_prior_vis;
	float4 f_prior_vis = ConvertUIntToFloat4(f_prior.i_vis);
	float4 f_posterior_vis = ConvertUIntToFloat4(f_posterior.i_vis);
	f_prior_vis.a = min(f_prior_vis.a, SAFE_OPAQUEALPHA);
	f_posterior_vis.a = min(f_posterior_vis.a, SAFE_OPAQUEALPHA);

	float zfront_prior_f = f_prior.z - f_prior.zthick;
	if (zfront_prior_f <= zfront_posterior_f)
	{
		// Case 2 : Intersecting each other
		//fs_out.f_prior.zthick = max(zfront_posterior_f - zfront_prior_f, 0); // to avoid computational precision error (0 or small minus)
		fs_out.f_prior.zthick = zfront_posterior_f - zfront_prior_f;
		fs_out.f_prior.z = zfront_posterior_f;
#if LINEAR_MODE == 1
		{
			f_m_prior_vis = f_prior_vis * (fs_out.f_prior.zthick / f_prior.zthick);
		}
#else
		{
			float zd_ratio = fs_out.f_prior.zthick / f_prior.zthick;
			float Ad = f_prior_vis.a;
			float3 Id = f_prior_vis.rgb / Ad;

			// strict mode
			float Az = Ad * zd_ratio * beta + (1 - beta) * (1 - pow(abs(1 - Ad), zd_ratio));
			// approx. mode
			//float term1 = zd_ratio * (1 - zd_ratio) / 2.f * Ad * Ad;
			//float term2 = term1 * (2 - zd_ratio) / 3.f * Ad;
			//float term3 = term2 * (3 - zd_ratio) / 4.f * Ad;
			//float term4 = term3 * (4 - zd_ratio) / 5.f * Ad;
			//float Az = Ad * zd_ratio + (1 - beta) * (term1 + term2 + term3 + term4 + term4 * (5 - zd_ratio) / 6.f * Ad);

			float3 Cz = Id * Az;
			f_m_prior_vis = float4(Cz, Az);
		}
#endif

		float old_alpha = f_prior_vis.a;
		f_prior.zthick -= fs_out.f_prior.zthick;
		f_prior_vis = (f_prior_vis - f_m_prior_vis) / (1.f - f_m_prior_vis.a);

		fs_out.f_prior.opacity_sum = f_prior.opacity_sum * f_m_prior_vis.a / old_alpha;
		//f_prior.opacity_sum = f_prior.opacity_sum * f_prior_vis.a / old_alpha;
		f_prior.opacity_sum = f_prior.opacity_sum - fs_out.f_prior.opacity_sum;
	}
	else
	{
		// Case 3 : f_prior belongs to f_posterior
		//fs_out.f_prior.zthick = max(zfront_prior_f - zfront_posterior_f, 0); // to avoid computational precision error (0 or small minus)
		fs_out.f_prior.zthick = zfront_prior_f - zfront_posterior_f;
		fs_out.f_prior.z = zfront_prior_f;

#if LINEAR_MODE == 1
		{
			f_m_prior_vis = f_posterior_vis * (fs_out.f_prior.zthick / f_posterior.zthick);
		}
#else
		{
			float zd_ratio = fs_out.f_prior.zthick / f_posterior.zthick;
			float Ad = f_posterior_vis.a;
			float3 Id = f_posterior_vis.rgb / Ad;

			// strict mode
			float Az = Ad * zd_ratio * beta + (1 - beta) * (1 - pow(abs(1 - Ad), zd_ratio));
			// approx. mode
			//float term1 = zd_ratio * (1 - zd_ratio) / 2.f * Ad * Ad;
			//float term2 = term1 * (2 - zd_ratio) / 3.f * Ad;
			//float term3 = term2 * (3 - zd_ratio) / 4.f * Ad;
			//float term4 = term3 * (4 - zd_ratio) / 5.f * Ad;
			//float Az = Ad * zd_ratio + (1 - beta) * (term1 + term2 + term3 + term4 + term4 * (5 - zd_ratio) / 6.f * Ad);

			float3 Cz = Id * Az;
			f_m_prior_vis = float4(Cz, Az);
		}
#endif

		float old_alpha = f_posterior_vis.a;
		f_posterior.zthick -= fs_out.f_prior.zthick;
		f_posterior_vis = (f_posterior_vis - f_m_prior_vis) / (1.f - f_m_prior_vis.a);

		fs_out.f_prior.opacity_sum = f_posterior.opacity_sum * f_m_prior_vis.a / old_alpha;
		//f_posterior.opacity_sum = f_posterior.opacity_sum * f_posterior_vis.a / old_alpha;
		f_posterior.opacity_sum = f_posterior.opacity_sum - fs_out.f_prior.opacity_sum;
	}

	// merge the fusion sub_rs (f_prior) to fs_out.f_prior
	fs_out.f_prior.zthick += f_prior.zthick;
	fs_out.f_prior.z = f_prior.z;
	float4 f_mid_vis = f_posterior_vis * (f_prior.zthick / f_posterior.zthick); // REDESIGN
	float f_mid_alphaw = f_posterior.opacity_sum * f_mid_vis.a / f_posterior_vis.a;
	//float4 f_mid_mix_vis = BlendFloat4AndFloat4(f_mid_vis, f_prior_vis);
	float4 f_mid_mix_vis = MixOpt(f_mid_vis, f_mid_alphaw, f_prior_vis, f_prior.opacity_sum);
	f_m_prior_vis += f_mid_mix_vis * (1.f - f_m_prior_vis.a);
	fs_out.f_prior.opacity_sum += f_mid_alphaw + f_prior.opacity_sum;

	f_posterior.zthick -= f_prior.zthick;
	float old_alpha = f_posterior_vis.a;
	f_posterior_vis = (f_posterior_vis - f_mid_vis) / (1.f - f_mid_vis.a);
	//f_posterior.opacity_sum *= f_posterior_vis.a / old_alpha;
	f_posterior.opacity_sum -= f_mid_alphaw;

	// convert to 8b channels
	//f_prior.i_vis = ConvertFloat4ToUInt(f_prior_vis);
	f_posterior.i_vis = ConvertFloat4ToUInt(f_posterior_vis);
	fs_out.f_prior.i_vis = ConvertFloat4ToUInt(f_m_prior_vis);
	fs_out.f_posterior = f_posterior;

	fs_m.i_vis = ConvertFloat4ToUInt(f_m_prior_vis + f_posterior_vis * (1.f - f_m_prior_vis.a));
	fs_m.zthick = fs_out.f_prior.zthick + fs_out.f_posterior.zthick;
	fs_m.z = fs_out.f_posterior.z;
	fs_m.opacity_sum = fs_out.f_prior.opacity_sum + fs_out.f_posterior.opacity_sum;

#define ALPHA_CHECK(IV) ((IV >> 24) > 0)
	if (!ALPHA_CHECK(fs_m.i_vis))
	{
		fs_m.i_vis = 0;
		fs_m.zthick = 0;
	}
	return fs_m;
}

Fragment OFM(in Fragment f_1, const in Fragment f_2) // ordered ver.
{
	float4 f_1_vis = ConvertUIntToFloat4(f_1.i_vis);
	float4 f_2_vis = ConvertUIntToFloat4(f_2.i_vis);

	Fragment f_mix;
	float4 f_mix_vis = MixOpt(f_1_vis, f_1.opacity_sum, f_2_vis, f_2.opacity_sum);
	f_mix.i_vis = ConvertFloat4ToUInt(f_mix_vis);
	f_mix.opacity_sum = f_1.opacity_sum + f_2.opacity_sum;
	float z_front = min(f_1.z - f_1.zthick, f_2.z - f_2.zthick);
	f_mix.z = f_2.z;
	f_mix.zthick = f_mix.z - z_front;

	return f_mix;
}

Fragment MergeFrags_ver2(Fragment f_prior, Fragment f_posterior, const in float beta)
{
	// Overall algorithm computation cost 
	// : 3 branches, 2 visibility interpolations, 2 visibility integrations, and 1 fusion of overlapping ray-segments

	// f_prior and f_posterior mean f_prior.z >= f_prior.z
	Fragment f_out = (Fragment)0;

	float zfront_posterior_f = f_posterior.z - f_posterior.zthick;
	if (f_prior.z > zfront_posterior_f) // overlapping test
	{
		Fragment f_m_prior;
		float4 f_m_prior_vis;
		float4 f_prior_vis = ConvertUIntToFloat4(f_prior.i_vis);
		float4 f_posterior_vis = ConvertUIntToFloat4(f_posterior.i_vis);
		f_prior_vis.a = min(f_prior_vis.a, SAFE_OPAQUEALPHA);
		f_posterior_vis.a = min(f_posterior_vis.a, SAFE_OPAQUEALPHA);

		float zfront_prior_f = f_prior.z - f_prior.zthick;
		if (zfront_prior_f < zfront_posterior_f)
		{
			// Case 2 : Intersecting each other
			f_m_prior.zthick = zfront_posterior_f - zfront_prior_f;
			f_m_prior.z = zfront_posterior_f;
#if LINEAR_MODE == 1
			{
				f_m_prior_vis = f_prior_vis * (f_m_prior.zthick / f_prior.zthick);
			}
#else
			{
				float zd_ratio = f_m_prior.zthick / f_prior.zthick;
				float Ad = f_prior_vis.a;
				float3 Id = f_prior_vis.rgb / Ad;

				// strict mode
				float Az = Ad * zd_ratio * beta + (1 - beta) * (1 - pow(abs(1 - Ad), zd_ratio));
				// approx. mode
				//float term1 = zd_ratio * (1 - zd_ratio) / 2.f * Ad * Ad;
				//float term2 = term1 * (2 - zd_ratio) / 3.f * Ad;
				//float term3 = term2 * (3 - zd_ratio) / 4.f * Ad;
				//float term4 = term3 * (4 - zd_ratio) / 5.f * Ad;
				//float Az = Ad * zd_ratio + (1 - beta) * (term1 + term2 + term3 + term4 + term4 * (5 - zd_ratio) / 6.f * Ad);

				float3 Cz = Id * Az;
				f_m_prior_vis = float4(Cz, Az);
			}
#endif
			float old_alpha = f_prior_vis.a;
			f_prior.zthick -= f_m_prior.zthick;
			f_prior_vis = (f_prior_vis - f_m_prior_vis) / (1.f - f_m_prior_vis.a);

			f_m_prior.opacity_sum = f_prior.opacity_sum * f_m_prior_vis.a / old_alpha;
			f_prior.opacity_sum = f_prior.opacity_sum * f_prior_vis.a / old_alpha;
			//f_prior.opacity_sum = f_prior.opacity_sum - f_m_prior.opacity_sum;
		}
		else
		{
			// Case 3 : f_prior belongs to f_posterior
			f_m_prior.zthick = zfront_prior_f - zfront_posterior_f;
			f_m_prior.z = zfront_prior_f;

#if LINEAR_MODE == 1
			{
				f_m_prior_vis = f_posterior_vis * (f_m_prior.zthick / f_posterior.zthick);
			}
#else
			{
				float zd_ratio = f_m_prior.zthick / f_posterior.zthick;
				float Ad = f_posterior_vis.a;
				float3 Id = f_posterior_vis.rgb / Ad;

				// strict mode
				float Az = Ad * zd_ratio * beta + (1 - beta) * (1 - pow(abs(1 - Ad), zd_ratio));
				// approx. mode
				//float term1 = zd_ratio * (1 - zd_ratio) / 2.f * Ad * Ad;
				//float term2 = term1 * (2 - zd_ratio) / 3.f * Ad;
				//float term3 = term2 * (3 - zd_ratio) / 4.f * Ad;
				//float term4 = term3 * (4 - zd_ratio) / 5.f * Ad;
				//float Az = Ad * zd_ratio + (1 - beta) * (term1 + term2 + term3 + term4 + term4 * (5 - zd_ratio) / 6.f * Ad);

				float3 Cz = Id * Az;
				f_m_prior_vis = float4(Cz, Az);
			}
#endif

			float old_alpha = f_posterior_vis.a;
			f_posterior.zthick -= f_m_prior.zthick;
			f_posterior_vis = (f_posterior_vis - f_m_prior_vis) / (1.f - f_m_prior_vis.a);

			f_m_prior.opacity_sum = f_posterior.opacity_sum * f_m_prior_vis.a / old_alpha;
			f_posterior.opacity_sum = f_posterior.opacity_sum * f_posterior_vis.a / old_alpha;
			//f_posterior.opacity_sum = f_posterior.opacity_sum - f_m_prior.opacity_sum;
		}

		// merge the fusion (remained) f_prior to f_m_prior
		f_m_prior.zthick += f_prior.zthick;
		f_m_prior.z = f_prior.z;
		float4 f_mid_vis = f_posterior_vis * (f_prior.zthick / f_posterior.zthick); // REDESIGN
		float f_mid_alphaw = f_posterior.opacity_sum * f_mid_vis.a / f_posterior_vis.a;
		//float4 f_mid_mix_vis = BlendFloat4AndFloat4(f_mid_vis, f_prior_vis);
		float4 f_mid_mix_vis = MixOpt(f_mid_vis, f_mid_alphaw, f_prior_vis, f_prior.opacity_sum);
		f_m_prior_vis += f_mid_mix_vis * (1.f - f_m_prior_vis.a); // OV operator
		f_m_prior.opacity_sum += f_mid_alphaw + f_prior.opacity_sum;

		f_posterior.zthick -= f_prior.zthick;
		float old_alpha = f_posterior_vis.a;
		f_posterior_vis = (f_posterior_vis - f_mid_vis) / (1.f - f_mid_vis.a);
		f_posterior.opacity_sum *= f_posterior_vis.a / old_alpha;
		//f_posterior.opacity_sum -= f_mid_alphaw;

		// merge f_m_prior, f_posterior
		float4 vis_out = f_m_prior_vis + f_posterior_vis * (1.f - f_m_prior_vis.a);
		f_out.i_vis = ConvertFloat4ToUInt(vis_out);
		//f_out.i_vis = ConvertFloat4ToUInt(float4(1, 0, 0, 1));
		f_out.zthick = f_m_prior.zthick + f_posterior.zthick;
		f_out.z = f_posterior.z;
		f_out.opacity_sum = f_m_prior.opacity_sum + f_posterior.opacity_sum;

#define ALPHA_CHECK(IV) ((IV >> 24) > 0)
		if (!ALPHA_CHECK(f_out.i_vis))
		{
			f_out.i_vis = 0;
			f_out.zthick = 0;
		}
	}
	return f_out;
}

Fragment MergeFrags_ShiftVer(Fragment f_prior, Fragment f_posterior, const in float beta)
{
	// Overall algorithm computation cost 
	// : 3 branches, 2 visibility interpolations, 2 visibility integrations, and 1 fusion of overlapping ray-segments

	// f_prior and f_posterior mean f_prior.z >= f_prior.z
	Fragment f_out = (Fragment)0;

	float delta_z = f_posterior.z - f_prior.z;
	// the following code suffers from single-precision floating-point problem
	// instead, linear shift is used for resolving the precision issue
	//float zfront_posterior_f = f_posterior.z - f_posterior.zthick;
	//if (f_prior.z > zfront_posterior_f)
	if ((delta_z - f_posterior.zthick) * (delta_z + f_prior.zthick) < 0) // overlapping test
	{
		Fragment f_m_prior, f_m_posterior;
		float4 f_m_prior_vis;
		float4 f_prior_vis = ConvertUIntToFloat4(f_prior.i_vis);
		float4 f_posterior_vis = ConvertUIntToFloat4(f_posterior.i_vis);
		f_prior_vis.a = min(f_prior_vis.a, SAFE_OPAQUEALPHA);
		f_posterior_vis.a = min(f_posterior_vis.a, SAFE_OPAQUEALPHA);

		float zfront_prior_f = f_prior.z - f_prior.zthick;
		float zfront_posterior_f = f_posterior.z - f_posterior.zthick;
		// the following code suffers from single-precision floating-point problem
		// instead, linear shift is used for resolving the precision issue
		//if (zfront_prior_f < zfront_posterior_f)
		if (f_posterior.zthick - f_prior.zthick < delta_z)
		{
			// Case 2 : Intersecting each other
			//f_m_prior.zthick = zfront_posterior_f - zfront_prior_f;
			f_m_prior.zthick = delta_z + (f_prior.zthick - f_posterior.zthick);
			f_m_prior.z = zfront_posterior_f;
#if LINEAR_MODE == 1
			{
				f_m_prior_vis = f_prior_vis * (f_m_prior.zthick / f_prior.zthick);
			}
#else
			{
				float zd_ratio = f_m_prior.zthick / f_prior.zthick;
				float Ad = f_prior_vis.a;
				float3 Id = f_prior_vis.rgb / Ad;

				// strict mode
				float Az = Ad * zd_ratio * beta + (1 - beta) * (1 - pow(abs(1 - Ad), zd_ratio));
				// approx. mode
				//float term1 = zd_ratio * (1 - zd_ratio) / 2.f * Ad * Ad;
				//float term2 = term1 * (2 - zd_ratio) / 3.f * Ad;
				//float term3 = term2 * (3 - zd_ratio) / 4.f * Ad;
				//float term4 = term3 * (4 - zd_ratio) / 5.f * Ad;
				//float Az = Ad * zd_ratio + (1 - beta) * (term1 + term2 + term3 + term4 + term4 * (5 - zd_ratio) / 6.f * Ad);

				float3 Cz = Id * Az;
				f_m_prior_vis = float4(Cz, Az);
			}
#endif
			float old_alpha = f_prior_vis.a;
			f_prior.zthick = (f_prior.zthick - f_m_prior.zthick);
			f_prior_vis = (f_prior_vis - f_m_prior_vis) / (1.f - f_m_prior_vis.a);

			f_m_prior.opacity_sum = f_prior.opacity_sum * f_m_prior_vis.a / old_alpha;
			f_prior.opacity_sum = f_prior.opacity_sum * f_prior_vis.a / old_alpha;
			//f_prior.opacity_sum = f_prior.opacity_sum - f_m_prior.opacity_sum;
		}
		else
		{
			// Case 3 : f_prior belongs to f_posterior
			//f_m_prior.zthick = zfront_prior_f - zfront_posterior_f;
			f_m_prior.zthick = (f_posterior.zthick - f_prior.zthick) - delta_z;
			f_m_prior.z = zfront_prior_f;

#if LINEAR_MODE == 1
			{
				f_m_prior_vis = f_posterior_vis * (f_m_prior.zthick / f_posterior.zthick);
			}
#else
			{
				float zd_ratio = f_m_prior.zthick / f_posterior.zthick;
				float Ad = f_posterior_vis.a;
				float3 Id = f_posterior_vis.rgb / Ad;

				// strict mode
				float Az = Ad * zd_ratio * beta + (1 - beta) * (1 - pow(abs(1 - Ad), zd_ratio));
				// approx. mode
				//float term1 = zd_ratio * (1 - zd_ratio) / 2.f * Ad * Ad;
				//float term2 = term1 * (2 - zd_ratio) / 3.f * Ad;
				//float term3 = term2 * (3 - zd_ratio) / 4.f * Ad;
				//float term4 = term3 * (4 - zd_ratio) / 5.f * Ad;
				//float Az = Ad * zd_ratio + (1 - beta) * (term1 + term2 + term3 + term4 + term4 * (5 - zd_ratio) / 6.f * Ad);

				float3 Cz = Id * Az;
				f_m_prior_vis = float4(Cz, Az);
			}
#endif

			float old_alpha = f_posterior_vis.a;
			f_posterior.zthick = f_posterior.zthick - f_m_prior.zthick;
			f_posterior_vis = (f_posterior_vis - f_m_prior_vis) / (1.f - f_m_prior_vis.a);

			f_m_prior.opacity_sum = f_posterior.opacity_sum * f_m_prior_vis.a / old_alpha;
			f_posterior.opacity_sum = f_posterior.opacity_sum * f_posterior_vis.a / old_alpha;
			//f_posterior.opacity_sum = f_posterior.opacity_sum - f_m_prior.opacity_sum;
		}

		// merge the fusion (remained) f_prior to f_m_prior
		f_m_prior.zthick += f_prior.zthick;
		f_m_prior.z = f_prior.z;
		float4 f_mid_vis = f_posterior_vis * (f_prior.zthick / f_posterior.zthick); // REDESIGN
		float f_mid_alphaw = f_posterior.opacity_sum * f_mid_vis.a / f_posterior_vis.a;
		//float4 f_mid_mix_vis = BlendFloat4AndFloat4(f_mid_vis, f_prior_vis);
		float4 f_mid_mix_vis = MixOpt(f_mid_vis, f_mid_alphaw, f_prior_vis, f_prior.opacity_sum);
		f_m_prior_vis += f_mid_mix_vis * (1.f - f_m_prior_vis.a); // OV operator
		f_m_prior.opacity_sum += f_mid_alphaw + f_prior.opacity_sum;

		f_posterior.zthick -= f_prior.zthick;
		float old_alpha = f_posterior_vis.a;
		f_posterior_vis = (f_posterior_vis - f_mid_vis) / (1.f - f_mid_vis.a);
		f_posterior.opacity_sum *= f_posterior_vis.a / old_alpha;
		//f_posterior.opacity_sum -= f_mid_alphaw;

		// merge f_m_prior, f_posterior
		float4 vis_out = f_m_prior_vis + f_posterior_vis * (1.f - f_m_prior_vis.a);
		f_out.i_vis = ConvertFloat4ToUInt(vis_out);
		//f_out.i_vis = ConvertFloat4ToUInt(float4(1, 0, 0, 1));
		f_out.zthick = f_m_prior.zthick + f_posterior.zthick;
		f_out.z = f_posterior.z;
		f_out.opacity_sum = f_m_prior.opacity_sum + f_posterior.opacity_sum;

#define ALPHA_CHECK(IV) ((IV >> 24) > 0)
		if (!ALPHA_CHECK(f_out.i_vis))
		{
			f_out.i_vis = 0;
			f_out.zthick = 0;
		}
	}
	return f_out;
}
#endif

cbuffer cbGlobalParams : register(b9)
{
	HxCB_HotspotMask g_cbHSMask;
}

cbuffer cbGlobalParams : register(b10)
{
	HxCB_CurvedSlicer g_cbCurvedSlicer;
}

float GetHotspotMaskWeightIdx(inout int out_lined, in int2 pos_xy, in int i, in bool check_silhouete)
{
	//out_lined = 0;
	float weight = 0;
	//float count = 0;
	float smoothness = (g_cbHSMask.mask_info_[i].smoothness & 0xFFFF);
	bool silhouette = (g_cbHSMask.mask_info_[i].smoothness >> 16) > 0;
	if (smoothness > 0 && (silhouette || !check_silhouete))
	{
		const float MAX_X = 100.f;
		const float MAX_ATAN = 1.56079666011;// acos(MAX_X);
		float coeff_atan = 1. / smoothness;
		int2 vdiff = pos_xy - g_cbHSMask.mask_info_[i].pos_center;
		float leng = length(vdiff);
		if (abs(leng - g_cbHSMask.mask_info_[i].radius) < g_cbHSMask.mask_info_[i].bnd_thick)
		{
			out_lined++;
		}
		else
		{
			if(leng < g_cbHSMask.mask_info_[i].radius)
				out_lined--;
		}
		//count++;
		float r = g_cbHSMask.mask_info_[i].radius - leng;
		if (r > 0)
		{
			float arc_x = max(r, 0) / g_cbHSMask.mask_info_[i].radius * 2.f * MAX_X - MAX_X; // [-MAX_X (outside), MAX_X (inside)]
			//if(arc_x > 0)
			weight = saturate((atan(arc_x * coeff_atan) + MAX_ATAN) / (2. * MAX_ATAN)); // [0, 1]
		}
	}
	//if (count > 0)
	//	weight /= count;
	return weight;
}

float Get3DHotspotMaskWeightIdx(inout int out_lined, in float3 pos_frag, in float3 view_eye_probe, in int i, in bool check_silhouete)
{
	// use 'radius' and 'bnd_thick' by world space units

	//out_lined = 0;
	float weight = 0;
	//float count = 0;
	float smoothness = (g_cbHSMask.mask_info_[i].smoothness & 0xFFFF);
	bool silhouette = (g_cbHSMask.mask_info_[i].smoothness >> 16) > 0;
	if (smoothness > 0 && (silhouette || !check_silhouete))
	{
		// g_cbCamState.pos_cam_ws
		float3 pos_spotcenter = g_cbHSMask.mask_info_[i].pos_spotcenter;
		//const float MAX_X = 100.f;
		//const float MAX_ATAN = 1.40079666011;// acos(MAX_X);
		//float coeff_atan = 1. / smoothness;
		float3 vdiff = pos_frag - pos_spotcenter;
		float vdot = dot(view_eye_probe, vdiff);
		float3 v_pos = pos_spotcenter + view_eye_probe * vdot;
		float leng = length(v_pos - pos_frag);
		//float leng = length(vdiff);
		//if (abs(leng - g_cbHSMask.mask_info_[i].radius) < g_cbHSMask.mask_info_[i].bnd_thick)
		//{
		//	out_lined++;
		//}
		//else
		//{
		//	if (leng < g_cbHSMask.mask_info_[i].radius)
		//		out_lined--;
		//}
		//count++;
		float mask_max_r = g_cbHSMask.mask_info_[i].radius;
		float mask_r = min(mask_max_r, leng);
		float g_r = mask_r / mask_max_r * 3.f;
		weight = exp(-g_r * g_r / (2.f * 0.8 * 0.8));
	}
	//if (count > 0)
	//	weight /= count;
	return weight;
}

int GhostedEffect(out float mask_weight, out float dynamic_alpha_weight, 
	const in float3 pos_frag_ws, const in float3 view_dir, const in float3 nor, const in float nor_len,
	const in bool is_dynamic_transparency)
{
	// mask value compute
	int out_lined = 0;
	float3 pos_spotcenter = g_cbHSMask.mask_info_[0].pos_spotcenter;
	float3 view_eye_probe = normalize(pos_spotcenter - g_cbCamState.pos_cam_ws);
	dynamic_alpha_weight = 1.f;
	mask_weight = Get3DHotspotMaskWeightIdx(out_lined, pos_frag_ws, view_eye_probe, 0, false);
	if (out_lined <= 0)
	{
		float dynamic_kappa_w = mask_weight;
		if (BitCheck(g_cbHSMask.mask_info_[0].flag, 0))
		{
			float3 vec_pos_probe2frag = pos_frag_ws - pos_spotcenter;
			float dot_vec = dot(view_eye_probe, vec_pos_probe2frag);

			if (dot_vec > 0)
			{
				const float depth_transparency = g_cbHSMask.mask_info_[0].in_depth_vis; // ws unit
				// view_dir
				float depth_w = saturate(-(dot_vec - depth_transparency) / depth_transparency);
				//dynamic_kappa_w *= depth_w;// depth_w;
				mask_weight *= depth_w;// depth_w;
			}
		}

		float kappa_t = g_cbHSMask.mask_info_[0].kappa_t * dynamic_kappa_w;
		float kappa_s = g_cbHSMask.mask_info_[0].kappa_s * dynamic_kappa_w;

		//v_rgba.rgba = float4((float3)0.0, 1);
		if (dynamic_kappa_w > 0 && is_dynamic_transparency)
		{
			float s = 1.f;
			if (nor_len > 0) 
				s = saturate(abs(dot(nor / nor_len, view_dir))); // [0, 1]
			dynamic_alpha_weight = saturate((1.f - kappa_t) * pow(1.f - s, kappa_s));
		}
	}
	return out_lined;
}

float GetHotspotThickness(in int2 pos_xy)
{
	int out_lined = 0;
	float v_thickness = 0;
	float w_sum = 0;
	for (int i = 0; i < 2; i++)
	{
		float w = GetHotspotMaskWeightIdx(out_lined, pos_xy, i, false);
		if (w > 0)
		{
			v_thickness += g_cbHSMask.mask_info_[i].thick * w * w;
			w_sum += w;
		}
	}
	if (w_sum == 0) w_sum = 1.f;
	return max(v_thickness / w_sum, 0.0001f);
}

float GetVZThickness(const float z, const float vz_thick)
{
	float vz_thickness = vz_thick;
	if (vz_thick <= 0)
	{
#define nbits 24
		// view-source:https://www.sjbaker.org/steve/omniv/love_your_z_buffer.html
		// compute z-precision
		const float zNear = g_cbCamState.near_plane;
		const float zFar = g_cbCamState.far_plane;
		float b = zFar * zNear / (zNear - zFar);
		float res = (b / ((b / z) - 1.0f / (1 << nbits))) - z;
		if (res < 0.0001)
			vz_thickness = -res;
		else
			vz_thickness = -floor(res * 100000.0f) / 100000.0f;

		vz_thickness *= 2.0; // refer to Section 4.1 "z-thickness value determination"

		const float scale_factor = asfloat(g_cbCamState.iSrCamDummy__2);
		vz_thickness *= scale_factor;
	}
	return vz_thickness;
}

//f16tof32
//#define U16TO32F(TC) (g_cbCamState.scene_thickness_range * ((float)((TC) & 0xFFFF) / 65535.f))
//f32tof16
//#define F32TO16U(F) ((uint)(min((F) / g_cbCamState.scene_thickness_range, 1.f) * 65535.f))

void ComputeSSS_PerspMask(out float3 w_a, out float3 h_b, const float3 p_c, const float3 v,
	const float r, const float3 p_i, const float3 p, const float3 _w)
{
	// Dandelin Spheres and the Equation
	float3 v_pc_p = p - p_c;
	float len_pc_p = length(v_pc_p);
	float theta_1 = asin(r / len_pc_p);
	float len_pc_pcv = dot(v, v_pc_p);
	float3 p_cv = p_c + len_pc_pcv * v;
	float3 w = p - p_cv;
	float len_w = length(w);
	if (len_w > 0.0001f)
	{
		w /= length(w);

		float theta_2 = acos(len_pc_pcv / len_pc_p) - theta_1;
		float l = len_pc_pcv - r;
		float a_1 = l * tan(theta_2);
		float a = (l * tan(2.f * theta_1 + theta_2) - a_1) / 2.f;

		float3 p_e = p_c + l * v + (a_1 + a) * w;
		float3 p_f = p - r * v;
		float c = length(p_e - p_f);
		float b = sqrt(abs(a * a - c * c));

		float3 h = cross(v, w);
		h /= length(h);
		float l_c = length(p_i - p_c) * cos(theta_1 + theta_2);
		float s = l_c / l;

		w_a = w * a * s;
		h_b = h * b * s;
	}
	else
	{
		float l = len_pc_p - r;
		float r_p = l * tan(theta_1);

		float3 h = cross(v, _w);
		float s = length(p_i - p_c) / l;

		//w_a = float3(1, 0, 0);// _w * r_p * s;
		//h_b = float3(1, 0, 0);// h * r_p * s;
		w_a = _w * r_p * s;
		h_b = h * r_p * s;
	}
}

void ComputeSSS_PerspMask2(out float r_i, const float3 p_c, const float r, const float3 p_i, const float3 p)
{
	float cam2p_dist = length(p - p_c);
	float cam2i_dist = length(p_i - p_c);
	r_i = r * cam2i_dist / cam2p_dist;
}
