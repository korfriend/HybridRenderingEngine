#include "../CommonShader.hlsl"

#define DOWN_SAMPLE 4
#define MAX_NBUFFER_LEVEL 10

Texture2D<uint> fragment_counter : register(t10);
ByteAddressBuffer deep_k_buf : register(t11);

RWTexture2D<unorm float4> rw_fragment_blendout : register(u10);

Buffer<uint> global_z_minmax_buffer : register(t15);
Texture2DArray<float2> z_minmax_textures : register(t16);
Texture2DArray<unorm float> ao_textures : register(t17);

RWBuffer<uint> rw_global_z_minmax_buffer : register(u15);
RWTexture2DArray<float2> rw_z_minmax_textures : register(u16);

void DisplayRect(int x, int y, float4 color)
{
	for (int xx = 0; xx < 3; xx++)
		for (int yy = 0; yy < 3; yy++)
			rw_fragment_blendout[int2(x, y) + int2(xx, yy)] = color;
}

#if !defined(FRAG_MERGING) || FRAG_MERGING == 1
#define LOAD1_KBUF_VIS(KBUF, F_ADDR, K) KBUF.Load(F_ADDR + (K) * 4 * 4)
#define STORE1_KBUF_VIS(V, KBUF, F_ADDR, K) KBUF.Store(F_ADDR + (K) * 4 * 4, V)
#define LOAD1_KBUF_Z(KBUF, F_ADDR, K) asfloat(KBUF.Load(F_ADDR + ((K) * 4 + 1) * 4))
#define LOAD1_KBUF_ALPHA(KBUF, F_ADDR, K) (LOAD1_KBUF_VIS(KBUF, F_ADDR, K) >> 24)
#define LOAD1_KBUF_ALPHAF(KBUF, F_ADDR, K) (LOAD1_KBUF_ALPHA(KBUF, F_ADDR, K) / 255.f)
#endif

// assume camera space
// right handed, view dir is -z
// ray_v is a unit vector
// herein, z_coord and z are different: z_coord is a minus value, z is a distance (positive) value.
float3 ComputeCSPosPlaneZ(float z_coord, float3 pos_s, float3 ray_v)
{
	// http://www.gisdeveloper.co.kr/?p=792
	float t = (z_coord - pos_s.z) / ray_v.z;
	return pos_s + ray_v * t;
}

bool ComputeFootPrintBnd__(out float2 p_xy_inbnd_s, out float2 p_xy_inbnd_e, float2 p_xy_s, float2 p_xy_e)
{
	float2 v_xy_se = p_xy_e - p_xy_s;
	float2 pos_min = (float2)0;
	float2 pos_max = float2(g_cbCamState.rt_width - 1, g_cbCamState.rt_height - 1);

	// intersect ray with a box
	// http://www.siggraph.org/education/materials/HyperGraph/raytrace/rtinter3.htm
	float2 invR = float2(1.0f, 1.0f) / v_xy_se;
	float2 tbot = invR * (pos_min - p_xy_s);
	float2 ttop = invR * (pos_max - p_xy_s);

	// re-order intersections to find smallest and largest on each axis
	float2 tmin = min(ttop, tbot);
	float2 tmax = max(ttop, tbot);

	// find the largest tmin and the smallest tmax
	float largest_tmin = max(tmin.x, tmin.y);
	float smallest_tmax = min(tmax.x, tmax.y);

	float tnear = largest_tmin; // - is possible
	float tfar = smallest_tmax;

	p_xy_inbnd_s = p_xy_s;
	p_xy_inbnd_e = p_xy_e;
	if (tnear >= tfar) return false;
	if (tnear > 1 || tfar < 0) return false;
	if (tnear > 0) p_xy_inbnd_s = p_xy_s + v_xy_se * tnear;
	if (tfar < 1) p_xy_inbnd_e = p_xy_s + v_xy_se * tfar;

	return true;
}

bool ComputeFootPrintBnd(out float2 p_xy_inbnd_s, out float2 p_xy_inbnd_e, float2 p_xy_s, float2 p_xy_e)
{
	// https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-box-intersection#:~:text=Ray%2DBox%20Intersection,-Figure%201%3A%20equation&text=The%20equation%20of%20a%20line,as%20y%3Dmx%2Bb.&text=Such%20a%20box%20is%20called,axis%2Daligned%20bounding%20box).
	// g_cbCamState.rt_width, g_cbCamState.rt_height
	float2 bounds[2] = { float2(0, 0), float2(g_cbCamState.rt_width - 1, g_cbCamState.rt_height - 1) };

	float2 v_xy_se = p_xy_e - p_xy_s;
	float2 inv_v_xy_se = (float2)1 / v_xy_se;
	int sign[2] = { (int)(inv_v_xy_se.x < 0), (int)(inv_v_xy_se.y < 0) };

	float tmin, tmax, tymin, tymax;

	tmin = (bounds[sign[0]].x - p_xy_s.x) * inv_v_xy_se.x;
	tmax = (bounds[1 - sign[0]].x - p_xy_s.x) * inv_v_xy_se.x;
	tymin = (bounds[sign[1]].y - p_xy_s.y) * inv_v_xy_se.y;
	tymax = (bounds[1 - sign[1]].y - p_xy_s.y) * inv_v_xy_se.y;

	p_xy_inbnd_s = p_xy_s;
	p_xy_inbnd_e = p_xy_e;

	if ((tmin > tymax) || (tymin > tmax))
		return false;
	if (tymin > tmin)
		tmin = tymin;
	if (tymax < tmax)
		tmax = tymax;

	if (tmin > 1 || tmax < 0) return false;

	if (tmin > 0)
		p_xy_inbnd_s = p_xy_s + v_xy_se * tmin;
	if (tmax < 1) 
		p_xy_inbnd_e = p_xy_s + v_xy_se * tmax;
	return true;
}

// only for the case in lens model defined in CS
float Compute2DIntersectionCS(float2 p, float2 v, float2 vf, float clip_z_coord)
{
	// XY plane
	// vf = (a, b), v = (c, d)
	// P = vf * t1
	// P = p + v * t2
	float ad_bc = vf.x * v.y - v.x * vf.y;
	float t2;
	if (ad_bc == 0) t2 = (clip_z_coord - p.y) / v.y;
	else t2 = (p.x * vf.y - p.y * vf.x) / ad_bc;
	return t2 >= 0? t2 : FLT_MAX;
}

float Compute2dIntersectZ(float2 p, float2 v, float2 vf)
{
	// XY plane
	// vf = (a, b), v = (c, d)
	// P = vf * t1
	// P = p + v * t2
	float ad_bc = vf.x * v.y - v.x * vf.y;
	float t1;
	if (ad_bc * ad_bc < FLT_MIN) 0;
	else t1 = (p.x * v.y - p.y * v.x) / ad_bc;
	float2 p_i = vf * t1;
	return p_i.y;
}

float2 ComputeLensrayPosOnViewFrustomOld(out float3 p_lensray_s, out float3 p_lensray_e, float3 pos_lens, float3 unit_v_lens_ray, float3 ip_rect_ray_cs, float2 zminmax)
{
	// note that zvalues of zminmax are inbetween g_cbCamState.near_plane and g_cbCamState.far_plane
	float2 ray_near_2d, ray_far_2d;
	if (pos_lens.x * ip_rect_ray_cs.x < 0)
	{
		ray_near_2d = float2(-ip_rect_ray_cs.x, ip_rect_ray_cs.z);
		ray_far_2d = float2(ip_rect_ray_cs.x, ip_rect_ray_cs.z);
	}
	else
	{
		ray_near_2d = float2(ip_rect_ray_cs.x, ip_rect_ray_cs.z);
		ray_far_2d = float2(-ip_rect_ray_cs.x, ip_rect_ray_cs.z);
	}
	float inv_lenth_xz = 1.0 / length(unit_v_lens_ray.xz);
	float tx_near = Compute2DIntersectionCS(pos_lens.xz, unit_v_lens_ray.xz, ray_near_2d, -zminmax.x) * inv_lenth_xz;
	float tx_far  = Compute2DIntersectionCS(pos_lens.xz, unit_v_lens_ray.xz, ray_far_2d, -zminmax.y) * inv_lenth_xz;

	if (pos_lens.y * ip_rect_ray_cs.y < 0)
	{
		ray_near_2d = float2(-ip_rect_ray_cs.y, ip_rect_ray_cs.z);
		ray_far_2d = float2(ip_rect_ray_cs.y, ip_rect_ray_cs.z);
	}
	else
	{
		ray_near_2d = float2(ip_rect_ray_cs.y, ip_rect_ray_cs.z);
		ray_far_2d = float2(-ip_rect_ray_cs.y, ip_rect_ray_cs.z);
	}
	float inv_lenth_yz = 1.0 / length(unit_v_lens_ray.yz);
	float ty_near = Compute2DIntersectionCS(pos_lens.yz, unit_v_lens_ray.yz, ray_near_2d, -zminmax.x) * inv_lenth_yz;
	float ty_far = Compute2DIntersectionCS(pos_lens.yz, unit_v_lens_ray.yz, ray_far_2d, -zminmax.y) * inv_lenth_yz;

	//float t = (z_coord - pos_s.z) / ray_v.z;
	// pos_s + ray_v * t;
	float ray_z_inv = -1.0 / unit_v_lens_ray.z;
	float t_near = max(max(tx_near, ty_near), zminmax.x * ray_z_inv);
	float t_far = min(min(tx_far, ty_far), zminmax.y * ray_z_inv);

	p_lensray_s = pos_lens + unit_v_lens_ray * t_near;
	p_lensray_e = pos_lens + unit_v_lens_ray * t_far;
	//return float2(-p_lensray_s.z, -p_lensray_e.z); // test
	return float2(ty_near, ty_far); // test
}

float2 ComputeLensrayPosOnViewFrustomOld2(out float3 p_lensray_s, out float3 p_lensray_e, float3 pos_lens, float3 v_lens_ray, float3 ip_rect_ray_cs, float2 zminmax)
{
	// note that zvalues of zminmax are inbetween g_cbCamState.near_plane and g_cbCamState.far_plane
	float2 ray_near_2d, ray_far_2d;
	if (pos_lens.x * ip_rect_ray_cs.x < 0)
	{
		ray_near_2d = float2(-ip_rect_ray_cs.x, ip_rect_ray_cs.z);
		ray_far_2d = float2(ip_rect_ray_cs.x, ip_rect_ray_cs.z);
	}
	else
	{
		ray_near_2d = float2(ip_rect_ray_cs.x, ip_rect_ray_cs.z);
		ray_far_2d = float2(-ip_rect_ray_cs.x, ip_rect_ray_cs.z);
	}

	// note -z dir
	float zcoord_x_near = Compute2dIntersectZ(pos_lens.xz, v_lens_ray.xz, ray_near_2d);
	float zcoord_x_far = Compute2dIntersectZ(pos_lens.xz, v_lens_ray.xz, ray_far_2d);
	if (zcoord_x_near >= 0) zcoord_x_near = -zminmax.x;
	if (zcoord_x_far >= 0) zcoord_x_far = -zminmax.y;

	if (pos_lens.y * ip_rect_ray_cs.y < 0)
	{
		ray_near_2d = float2(-ip_rect_ray_cs.y, ip_rect_ray_cs.z);
		ray_far_2d = float2(ip_rect_ray_cs.y, ip_rect_ray_cs.z);
	}
	else
	{
		ray_near_2d = float2(ip_rect_ray_cs.y, ip_rect_ray_cs.z);
		ray_far_2d = float2(-ip_rect_ray_cs.y, ip_rect_ray_cs.z);
	}
	float zcoord_y_near = Compute2dIntersectZ(pos_lens.yz, v_lens_ray.yz, ray_near_2d);
	float zcoord_y_far = Compute2dIntersectZ(pos_lens.yz, v_lens_ray.yz, ray_far_2d);
	if (zcoord_y_near >= 0) zcoord_y_near = -zminmax.x;
	if (zcoord_y_far >= 0) zcoord_y_far = -zminmax.y;

	float zcoord_near = min(min(zcoord_x_near, zcoord_y_near), -zminmax.x);
	float zcoord_far = max(max(zcoord_x_far, zcoord_y_far), -zminmax.y);

	p_lensray_s = ComputeCSPosPlaneZ(zcoord_near, pos_lens, v_lens_ray);
	p_lensray_e = ComputeCSPosPlaneZ(zcoord_far, pos_lens, v_lens_ray);
	//return float2(-p_lensray_s.z, -p_lensray_e.z); // test
	//return float2(-zcoord_near, -zcoord_far); // test
	return TransformPoint(p_lensray_s, g_cbCamState.mat_ws2ss).xy; // test
}

float3 ComputeVfSideHit(float3 p_clipplane, float3 pos_lens, float3 v_lens_ray, float3 p_rect_xy_clip_max)
{
	float3 lrt_side[4] = {
		float3(pos_lens.x, v_lens_ray.x, -p_rect_xy_clip_max.x) ,
		float3(pos_lens.y, v_lens_ray.y, -p_rect_xy_clip_max.y) ,
		float3(pos_lens.y, v_lens_ray.y,  p_rect_xy_clip_max.y) ,
		float3(pos_lens.x, v_lens_ray.x,  p_rect_xy_clip_max.x) };

	float ov1 = p_rect_xy_clip_max.y * (p_clipplane.x) - p_rect_xy_clip_max.x * (p_clipplane.y);
	float ov2 = p_rect_xy_clip_max.y * (p_clipplane.x) + p_rect_xy_clip_max.x * (p_clipplane.y);
	int sign_idx = (int)(ov1 >= 0) + 2 * (int)(ov2 >= 0);
	float3 lrt = lrt_side[sign_idx];
	float zcoord =
		Compute2dIntersectZ(
			float2(lrt.x, pos_lens.z),
			float2(lrt.y, v_lens_ray.z),
			float2(lrt.z, p_rect_xy_clip_max.z));
	return ComputeCSPosPlaneZ(zcoord, pos_lens, v_lens_ray);
}

void ComputeLensrayPosOnViewFrustom(out float3 p_lensray_s, out float3 p_lensray_e, float3 pos_lens, float3 v_lens_ray_unit, float3 v_rect_max_unit, float2 zminmax)
{
	// note that zvalues of zminmax are inbetween g_cbCamState.near_plane and g_cbCamState.far_plane
	
	// 1. compute p_lensray_s
	float3 p_rect_xy_zmin = ComputeCSPosPlaneZ(-zminmax.x, (float3)0, v_rect_max_unit);
	float3 p_lensray_zmin = ComputeCSPosPlaneZ(-zminmax.x, pos_lens, v_lens_ray_unit);
	if (dot(-p_rect_xy_zmin.xy - p_lensray_zmin.xy, p_rect_xy_zmin.xy - p_lensray_zmin.xy) <= 0)
	{
		p_lensray_s = p_lensray_zmin;
	}
	else
	{
		p_lensray_s = ComputeVfSideHit(p_lensray_zmin, pos_lens, v_lens_ray_unit, p_rect_xy_zmin);
	}
	
	// 2. compute p_lensray_e
	float3 p_rect_xy_zmax = ComputeCSPosPlaneZ(-zminmax.y, (float3)0, v_rect_max_unit);
	float3 p_lensray_zmax = ComputeCSPosPlaneZ(-zminmax.y, pos_lens, v_lens_ray_unit);
	if (dot(-p_rect_xy_zmax.xy - p_lensray_zmax.xy, p_rect_xy_zmax.xy - p_lensray_zmax.xy) <= 0)
	{
		p_lensray_e = p_lensray_zmax;
	}
	else
	{
		p_lensray_e = ComputeVfSideHit(p_lensray_zmax, pos_lens, v_lens_ray_unit, p_rect_xy_zmax);
	}
	//return TransformPoint(p_lensray_s, g_cbCamState.mat_ws2ss).xy; // test
}

int ComputeFootprintMinMaxZ(out float2 z_minmax, float3 p_lensray_s, float3 p_lensray_e, int k, float2 init_zminmax)
{
	// assume that p_lensray_s and p_lensray_e are inside the view frustum!!
	// these points are computed via ComputeLensrayPosOnViewFrustom(..)
	z_minmax = init_zminmax;

	// note that 
	// 1. g_cbCamState.mat_ws2ss must consider the view frustum for float-point single precision.
	// 2. p_lensray_s and p_lensray_e are already clipped points against the view frustum
	float2 p_xy_s = TransformPoint(p_lensray_s, g_cbCamState.mat_ws2ss).xy;
	float2 p_xy_e = TransformPoint(p_lensray_e, g_cbCamState.mat_ws2ss).xy;
	//p_xy_s.x = min(max(0, p_xy_s.x), g_cbCamState.rt_width - 1);
	//p_xy_s.y = min(max(0, p_xy_s.y), g_cbCamState.rt_height - 1);
	//p_xy_e.x = min(max(0, p_xy_e.x), g_cbCamState.rt_width - 1);
	//p_xy_e.y = min(max(0, p_xy_e.y), g_cbCamState.rt_height - 1);

	float2 p_xy_min = float2(min(p_xy_s.x, p_xy_e.x), min(p_xy_s.y, p_xy_e.y));
	float2 p_xy_max = float2(max(p_xy_s.x, p_xy_e.x), max(p_xy_s.y, p_xy_e.y));

	//if (!ComputeFootPrintBnd(p_xy_s, p_xy_e, p_xy_s, p_xy_e))
	//	return 1;
	float2 v_xy_min_max = p_xy_max - p_xy_min;
	float footprint_rect_size = max(v_xy_min_max.x, v_xy_min_max.y);
	uint nbuf_idx = max(ceil(log2(footprint_rect_size)) - 2, 0);
	//if(nbuf_idx > MAX_NBUFFER_LEVEL) 
	//if(footprint_rect_size > 512)
	//	return 2; // true
	//return nbuf_idx;
	int w_load_offset = (nbuf_idx % DOWN_SAMPLE) * (g_cbCamState.rt_width / DOWN_SAMPLE);
	int h_load_offset = (nbuf_idx / DOWN_SAMPLE) * (g_cbCamState.rt_height / DOWN_SAMPLE);
	//uint2 p_xy = uint2((uint2)(p_xy_s / DOWN_SAMPLE) + (float2)0.5);
	uint2 p_xy = uint2((uint)p_xy_min.x / DOWN_SAMPLE + 0.5, (uint)p_xy_min.y / DOWN_SAMPLE + 0.5);
	// note that, in the k-buffer structure, 0-th layer represents 1st hit along eye ray.
	z_minmax = z_minmax_textures[int3(p_xy + uint2(w_load_offset, h_load_offset), k)];
	return nbuf_idx;
}

bool ComputeFootprintMinMaxZ_(out float2 z_minmax, float3 pos_lens, float3 v_lens_ray, float2 init_zminmax)
{
	z_minmax = init_zminmax;
	float3 p_lensray_s = ComputeCSPosPlaneZ(-init_zminmax.x, pos_lens, v_lens_ray);
	//if (p_lensray_s.z <= g_cbCamState.near_plane + FLT_MIN)
	//	p_lensray_s *= v_lens_ray * g_cbCamState.near_plane;
	float3 p_lensray_e = ComputeCSPosPlaneZ(-init_zminmax.y, pos_lens, v_lens_ray);
	float3 p_lensray_ss_s = TransformPoint(p_lensray_s, g_cbCamState.mat_ws2ss);
	float3 p_lensray_ss_e = TransformPoint(p_lensray_e, g_cbCamState.mat_ws2ss);
	float2 p_xy_s, p_xy_e; // lensray ss clipped in boundind rect
	if (!ComputeFootPrintBnd(p_xy_s, p_xy_e, p_lensray_ss_s.xy, p_lensray_ss_e.xy))
		return false;

	//if (p_xy_s.x < 0 || p_xy_s.x >= g_cbCamState.rt_width
	//	|| p_xy_s.y < 0 || p_xy_s.y >= g_cbCamState.rt_height) return false;

	float2 p_footprint_rect_min_ss = float2(min(p_xy_s.x, p_xy_e.x), min(p_xy_s.y, p_xy_e.y));
	float2 v_xy_se = p_xy_e.xy - p_xy_s.xy;
	float footprint_rect_size = max(abs(v_xy_se.x), abs(v_xy_se.y));
	uint nbuf_idx = max(ceil(log2(footprint_rect_size)) - 2, 0);
	int w_load_offset = ((nbuf_idx) % DOWN_SAMPLE) * (g_cbCamState.rt_width / DOWN_SAMPLE);
	int h_load_offset = ((nbuf_idx) / DOWN_SAMPLE) * (g_cbCamState.rt_height / DOWN_SAMPLE);
	uint2 p_xy = uint2(p_xy_s / DOWN_SAMPLE + (float2)0.5);
	// note that, in the k-buffer structure, 0-th layer represents 1st hit along eye ray.
	z_minmax = z_minmax_textures[int3(p_xy + uint2(w_load_offset, h_load_offset), 0)];
	return true;
}

int ComputeFootprintMinMaxZ__(out float2 z_minmax, float3 pos_lens, float3 v_lens_ray, float2 init_zminmax)
{
	z_minmax = init_zminmax;
	float3 p_lensray_s = ComputeCSPosPlaneZ(-init_zminmax.x, pos_lens, v_lens_ray);
	//if (p_lensray_s.z <= g_cbCamState.near_plane + FLT_MIN)
	//	p_lensray_s *= v_lens_ray * g_cbCamState.near_plane;
	float3 p_lensray_e = ComputeCSPosPlaneZ(-init_zminmax.y, pos_lens, v_lens_ray);
	float3 p_lensray_ss_s = TransformPoint(p_lensray_s, g_cbCamState.mat_ws2ss);
	float3 p_lensray_ss_e = TransformPoint(p_lensray_e, g_cbCamState.mat_ws2ss);
	float2 p_xy_s, p_xy_e; // lensray ss clipped in boundind rect
	if (!ComputeFootPrintBnd__(p_xy_s, p_xy_e, p_lensray_ss_s.xy, p_lensray_ss_e.xy))
		return 1;

	//if (p_xy_s.x < 0 || p_xy_s.x >= g_cbCamState.rt_width
	//	|| p_xy_e.y < 0 || p_xy_e.y >= g_cbCamState.rt_height) return 2;

	float2 p_footprint_rect_min_ss = float2(min(p_xy_s.x, p_xy_e.x), min(p_xy_s.y, p_xy_e.y));
	float2 v_xy_se = p_xy_e.xy - p_xy_s.xy;
	float footprint_rect_size = max(abs(v_xy_se.x), abs(v_xy_se.y));
	uint nbuf_idx = max(ceil(log2(footprint_rect_size)) - 2, 0);
	//if (nbuf_idx > 7) return 3;
	//return nbuf_idx;
	int w_load_offset = ((nbuf_idx) % DOWN_SAMPLE) * (g_cbCamState.rt_width / DOWN_SAMPLE);
	int h_load_offset = ((nbuf_idx) / DOWN_SAMPLE) * (g_cbCamState.rt_height / DOWN_SAMPLE);
	uint2 p_xy = uint2(p_xy_s / DOWN_SAMPLE + (float2)0.5);
	// note that, in the k-buffer structure, 0-th layer represents 1st hit along eye ray.
	z_minmax = z_minmax_textures[int3(p_xy + uint2(w_load_offset, h_load_offset), 0)];
	return 0;
}

//#define GetFragHitRadius(Z, ZN, RN) (Z) / (ZN) * (RN)
float GetFragHitRadius(float z, float ratio_rn_zn)
{
	// ratio_rn_zn = rn / zn
	return z * ratio_rn_zn;
}

bool IsZeroPoint(float3 p)
{
	return p.x*p.x + p.y*p.y + p.z*p.z < FLT_MIN;
}

float4 GetUintVisAt(int addr_base, int k, int2 xy)
{
#if ZT_MODEL == 1
	Fragment f;
	GET_FRAG(f, addr_base, k);
	uint i_vis = f.i_vis;
#else
	uint i_vis = LOAD1_KBUF_VIS(deep_k_buf, addr_base, k);
#endif

	float4 f_vis = ConvertUIntToFloat4(i_vis);
	//if(g_cbCamState.iSrCamDummy__1 >> 16)
	f_vis.rgb *= 1.f - ao_textures[int3(xy, k)];

	return f_vis;
}

float3 GetPosAt(int2 xy, int k, int bytes_frags_per_pixel)
{
	float3 pos_ip_cs = TransformPoint(float3(xy, 0), g_cbCamState.mat_ss2ws);
	float3 ray_dir_cs = normalize(pos_ip_cs);
	int addr_base = (xy.y * g_cbCamState.rt_width + xy.x) * bytes_frags_per_pixel;
#if ZT_MODEL == 1
	Fragment f;
	GET_FRAG(f, addr_base, k);
	float ray_dist_from_ip = f.z;
#else
	float ray_dist_from_ip = LOAD1_KBUF_Z(deep_k_buf, addr_base, k);
#endif
	return pos_ip_cs + ray_dir_cs * ray_dist_from_ip;
}

float3 ComputeCSPosPersFromKB(float3 p_along_cs_ray, float z_bnd, int k, int bytes_frags_per_pixel)
{
	// g_cbCamState.mat_ss2ws is used as ss2cs
	float3 p_ss = TransformPoint(p_along_cs_ray, g_cbCamState.mat_ws2ss);

	int2 xy_ss = int2(p_ss.x + 0.5, p_ss.y + 0.5);
	int frag_cnt_at_footprint = (int)fragment_counter[xy_ss];

	// assume a perspective projection
	float3 r_dir = normalize(p_along_cs_ray);
	float rz = -r_dir.z;
	float ray_dist;
	if (frag_cnt_at_footprint <= k)
	{
		ray_dist = z_bnd / rz;
	}
	else
	{
		int addr_base_at_footprint = (xy_ss.y * g_cbCamState.rt_width + xy_ss.x) * bytes_frags_per_pixel;
#if ZT_MODEL == 1
		Fragment f;
		GET_FRAG(f, addr_base_at_footprint, k);
		float ray_dist_from_ip = f.z;
#else
		float ray_dist_from_ip = LOAD1_KBUF_Z(deep_k_buf, addr_base_at_footprint, k);
#endif
		ray_dist = ray_dist_from_ip + g_cbCamState.near_plane / rz;
	}
	return r_dir * (z_bnd / rz);
}

bool IntersectionLinePoints2(out float3 p_i, float3 p1, float3 p2, float3 p3, float3 p4)
{
	// float3 p_s, float3 p_e, float3 p_min, float3 p_max
	// note that this is the guaranteed version
	float3 p13, p43, p21;
	float d1343, d4321, d1321, d4343, d2121;
	float numer, denom;

	p13.x = p1.x - p3.x;
	p13.y = p1.y - p3.y;
	p13.z = p1.z - p3.z;
	p43.x = p4.x - p3.x;
	p43.y = p4.y - p3.y;
	p43.z = p4.z - p3.z;
	//if (ABS(p43.x) < EPS && ABS(p43.y) < EPS && ABS(p43.z) < EPS)
	//	return(FALSE);
	p21.x = p2.x - p1.x;
	p21.y = p2.y - p1.y;
	p21.z = p2.z - p1.z;
	//if (ABS(p21.x) < EPS && ABS(p21.y) < EPS && ABS(p21.z) < EPS)
	//	return(FALSE);

	d1343 = p13.x * p43.x + p13.y * p43.y + p13.z * p43.z;
	d4321 = p43.x * p21.x + p43.y * p21.y + p43.z * p21.z;
	d1321 = p13.x * p21.x + p13.y * p21.y + p13.z * p21.z;
	d4343 = p43.x * p43.x + p43.y * p43.y + p43.z * p43.z;
	d2121 = p21.x * p21.x + p21.y * p21.y + p21.z * p21.z;

	denom = d2121 * d4343 - d4321 * d4321;
	//if (ABS(denom) < EPS)
	//	return(FALSE);
	numer = d1343 * d4321 - d1321 * d4343;

	float mua = numer / denom;
	float mub = (d1343 + d4321 * mua) / d4343;

	float3 p_12 = p1 + mua * p21;
	float3 p_34 = p3 + mub * p43;
	p_i = (p_12 + p_34) * 0.5;
	return mua >= 0 && mua <= 1;
}

float3 IntersectionLinePoints(float3 p1, float3 p2, float3 p3, float3 p4)
{
	// float3 p_s, float3 p_e, float3 p_min, float3 p_max
	// note that this is the guaranteed version
	float3 p13, p43, p21;
	float d1343, d4321, d1321, d4343, d2121;
	float numer, denom;

	p13.x = p1.x - p3.x;
	p13.y = p1.y - p3.y;
	p13.z = p1.z - p3.z;
	p43.x = p4.x - p3.x;
	p43.y = p4.y - p3.y;
	p43.z = p4.z - p3.z;
	//if (ABS(p43.x) < EPS && ABS(p43.y) < EPS && ABS(p43.z) < EPS)
	//	return(FALSE);
	p21.x = p2.x - p1.x;
	p21.y = p2.y - p1.y;
	p21.z = p2.z - p1.z;
	//if (ABS(p21.x) < EPS && ABS(p21.y) < EPS && ABS(p21.z) < EPS)
	//	return(FALSE);

	d1343 = p13.x * p43.x + p13.y * p43.y + p13.z * p43.z;
	d4321 = p43.x * p21.x + p43.y * p21.y + p43.z * p21.z;
	d1321 = p13.x * p21.x + p13.y * p21.y + p13.z * p21.z;
	d4343 = p43.x * p43.x + p43.y * p43.y + p43.z * p43.z;
	d2121 = p21.x * p21.x + p21.y * p21.y + p21.z * p21.z;

	denom = d2121 * d4343 - d4321 * d4321;
	//if (ABS(denom) < EPS)
	//	return(FALSE);
	numer = d1343 * d4321 - d1321 * d4343;

	float mua = numer / denom;
	float mub = (d1343 + d4321 * mua) / d4343;

	float3 p_12 = p1 + mua * p21;
	float3 p_34 = p3 + mub * p43;

	return (p_12 + p_34) * 0.5;
}


#define ITERATION_RAY_HIT_TEST 3
bool PiecewiseIntersection(out float3 p_f, out int2 p_f_xy,
	float3 p_lensray_s, float3 p_lensray_e,
	float3 p_eye_seg_ray_s, float3 p_eye_seg_ray_e,
	float z_min, float z_max, 
	int k, int bytes_frags_per_pixel)
{
	p_f = (float3)0;
	p_f_xy = (int2)0;
	// find a possible intersection point 
	// through a piecewise-linear approximation 
	// "Depth-of-Field Rendering with Multiview Synthesis, 2009, TOG"
	float3 p_i, p_is;
	[loop]
	for (int i = 0; i < ITERATION_RAY_HIT_TEST; i++)
	{
		if (!IntersectionLinePoints2(p_i, p_eye_seg_ray_s, p_eye_seg_ray_e, p_lensray_s, p_lensray_e)) return false;
		//p_i = IntersectionLinePoints(p_eye_seg_ray_s, p_eye_seg_ray_e, p_lensray_s, p_lensray_e);
		p_is = ComputeCSPosPersFromKB(p_i, (uint)i % 2 == 0 ? z_min : z_max, k, bytes_frags_per_pixel);
		if (p_i.z < p_is.z)
			p_eye_seg_ray_e = p_is;
		else
			p_eye_seg_ray_s = p_is;
	}
	p_f = (p_eye_seg_ray_s + p_eye_seg_ray_e) * 0.5;

	//if (dot(p_f - p_eye_seg_ray_s_ori, p_f - p_eye_seg_ray_e_ori) > 0) return false;

	float3 p_f_ss = TransformPoint(p_f, g_cbCamState.mat_ws2ss);
	p_f_xy = int2(p_f_ss.x + 0.5, p_f_ss.y + 0.5);
	int frag_cnt_along_hit_eyeray = (int)fragment_counter[p_f_xy];
	return k < frag_cnt_along_hit_eyeray;
}

float3x3 AngleAxis3x3(float angle, float3 axis)
{
	float c, s;
	sincos(angle, s, c);

	float t = 1 - c;
	float x = axis.x;
	float y = axis.y;
	float z = axis.z;

	return float3x3(
		t * x * x + c, t * x * y - s * z, t * x * z + s * y,
		t * x * y + s * z, t * y * y + c, t * y * z - s * x,
		t * x * z - s * y, t * y * z + s * x, t * z * z + c
		);
}

[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void KB_SSDOF_RT(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	if (DTid.x >= g_cbCamState.rt_width || DTid.y >= g_cbCamState.rt_height)
		return;
	{
		//int half_w = g_cbCamState.rt_width / DOWN_SAMPLE;
		//int half_h = g_cbCamState.rt_height / DOWN_SAMPLE;
		//
		////for (int nbuf_idx = 0; nbuf_idx < 10; nbuf_idx++)
		//int nbuf_idx = DTid.x / half_w + (DTid.y / half_h) * DOWN_SAMPLE;
		//{
		//	int w_load_offset = ((nbuf_idx) % DOWN_SAMPLE) * (g_cbCamState.rt_width / DOWN_SAMPLE);
		//	int h_load_offset = ((nbuf_idx) / DOWN_SAMPLE) * (g_cbCamState.rt_height / DOWN_SAMPLE);
		//	//uint2 p_xy = uint2(DTid.x % half_w, DTid.y % half_h);// +(float2)0.5);
		//	uint2 pp = uint2(DTid.x % half_w, DTid.y % half_h);
		//	uint2 p_xy = uint2((uint2)pp);// +(float2)0.5);
		//	float2 zmm = z_minmax_textures[int3(p_xy + uint2(w_load_offset, h_load_offset), 0)];
		//	//float2 zmm = z_minmax_textures[int3(DTid.xy, 0)];
		//	if (zmm.x > 0) rw_fragment_blendout[DTid.xy] = float4(1, 0, 0, 1);
		//	else rw_fragment_blendout[DTid.xy] = float4(1, 1, 0, 1);
		//}
		//return;
	}
	// at this moment, only static k buffer is supported.
	const uint k_value = g_cbCamState.k_value;
	uint bytes_per_frag = 4 * NUM_ELES_PER_FRAG;
	uint bytes_frags_per_pixel = k_value * bytes_per_frag; // to do : consider the dynamic scheme. (4 bytes unit)
	uint pixel_id = DTid.y * g_cbCamState.rt_width + DTid.x;
	uint addr_base_focalray = pixel_id * bytes_frags_per_pixel;

	//int frag_cnt = (int)fragment_counter[DTid.xy];
	//uint vr_hit_enc = frag_cnt >> 24;
	//frag_cnt = frag_cnt & 0xFFF;

	if (g_cbCamState.cam_flag & 0x1 == 0)
	{
		rw_fragment_blendout[DTid.xy] = float4(1, 0, 0, 1); // INVALID IN ORTHOGONAL PROJECTION
		return;
	}
	
	float3 pos_ip_ss = float3(DTid.xy, 0.0);
	// g_cbCamState.mat_ss2ws is used as ss2cs
	float3 pos_ip_cs = TransformPoint(pos_ip_ss, g_cbCamState.mat_ss2ws);
	float3 vec_eyeray_cs = normalize(pos_ip_cs);

	float3 pos_focus = ComputeCSPosPlaneZ(-g_cbEnv.dof_focus_z, (float3)0, vec_eyeray_cs);

	float3 ipO_cs = TransformPoint(float3(g_cbCamState.rt_width / 2.f, g_cbCamState.rt_height / 2.f, 0), g_cbCamState.mat_ss2ws);
	float3 ipx_cs = TransformPoint(float3(g_cbCamState.rt_width / 2.f + 1.f, g_cbCamState.rt_height / 2.f, 0), g_cbCamState.mat_ss2ws);
	float3 pix_cs = ipx_cs - ipO_cs;
	float pix_r = length(pix_cs) * 0.5; // SCALE for safe hit
	//float ratio_rn_zn = pix_r / g_cbCamState.near_plane;
	// g_cbCamState.near_plane is Focal Length?!
	float dof_lens_F = g_cbEnv.dof_sensor_z * g_cbEnv.dof_focus_z / (g_cbEnv.dof_sensor_z + g_cbEnv.dof_focus_z);
	float dof_ratio_sensor_to_ip = g_cbCamState.near_plane / g_cbEnv.dof_sensor_z;
	float pix_coc_r_const = dof_lens_F * dof_ratio_sensor_to_ip;

	// for fetch minmax z values from z_minmax_textures
	int half_w = g_cbCamState.rt_width / DOWN_SAMPLE;
	int half_h = g_cbCamState.rt_height / DOWN_SAMPLE;
	// entire layers culling...
	// in most practical cases, this is not efficient...
	// so, I do not perform this culling test (but do the layer's culling test per intersection ray)
#if EARLY_ENTIRE_LAYERS_CULLING == 1
	{
		float g_z_min = FLT_MAX, g_z_max = 0;
		for (int k = 0; k < (int)k_value; k++)
		{
			float z_min = asfloat(global_z_minmax_buffer[0 + 2 * k]);
			if (z_min == 0) break;
			float z_max = asfloat(global_z_minmax_buffer[1 + 2 * k]);
			g_z_min = min(g_z_min, z_min);
			g_z_max = max(g_z_max, z_max);
		}
		if (g_z_max == 0) return;
		//if (g_z_min <= g_cbCamState.near_plane)// + FLT_MIN)
		//{
		//	//g_z_min += 50;//
		//	rw_fragment_blendout[DTid.xy] = float4(1, 0, 1, 1);;
		//	return;
		//}

		float3 pos_lens_bb = float3(-1, 1, 0) * g_cbEnv.dof_lens_r;
		float3 v_lens_ray_bb = normalize(pos_focus - pos_lens_bb);

		float3 p_lensray_s_bb, p_lensray_e_bb;
		ComputeLensrayPosOnViewFrustom(p_lensray_s_bb, p_lensray_e_bb, pos_lens_bb, v_lens_ray_bb, float2(g_z_min, g_z_max));

		float2 z_minmax;
		// global layers culling based on "Depth-of-Field Rendering with Multiview Synthesis, 2009, TOG"
		bool is_valid_footprint = ComputeFootprintMinMaxZ(z_minmax, p_lensray_s_bb, p_lensray_e_bb, float2(g_z_min, g_z_max));
		if (z_minmax.x == 0 || !is_valid_footprint)
		{
			// note that, in the k-buffer structure, 0-th layer represents 1st hit along eye ray.
			rw_fragment_blendout[DTid.xy] = !is_valid_footprint? float4(1, 0, 0, 1) : float4(1, 1, 0, 1);
			return;
		}
	}
#endif

	float3 v_rect_max_unit = normalize(TransformPoint(float3(g_cbCamState.rt_width - 1, 0, 0), g_cbCamState.mat_ss2ws));
	float4 acc_lens_vis = (float4)0;
	int num_hit_rays = 0;
	int _test_cnt_no_hit = 0;
	[loop]
	for (int iray = 0; iray < g_cbEnv.dof_lens_ray_num_samples; iray++)
	{
		// random pos_lens, v_lens_ray
		// g_cbEnv.dof_lens_r
		int hash_idx = DTid.x + g_cbCamState.rt_width * DTid.y + iray * g_cbCamState.rt_width * g_cbCamState.rt_height;
		// jittering
		//float3 random_sample = float3(
		//	_random(hash_idx),
		//	_random(hash_idx + g_cbCamState.rt_width * g_cbCamState.rt_height * g_cbEnv.dof_lens_ray_num_samples), 0);//
			//_random(hash_idx + g_cbCamState.rt_width * g_cbCamState.rt_height * g_cbEnv.dof_lens_ray_num_samples * 2));
		//float3 pos_lens = (random_sample - float3(0.5, 0.5, 0)) * 2.0 * g_cbEnv.dof_lens_r;

		float3 random_sample = mul(AngleAxis3x3(F_PI / g_cbEnv.dof_lens_ray_num_samples * iray, float3(0, 0, -1)), float3(0.5, 0, 0));
		float3 pos_lens = (random_sample) * 2.0 * g_cbEnv.dof_lens_r;

		float3 v_lens_ray = normalize(pos_focus - pos_lens);

		float4 acc_ray_vis = (float4)0;

		// is parallel with vec_eyeray_cs
		float3 vdiff = v_lens_ray - vec_eyeray_cs;
		float angle = atan2(vdiff.y, vdiff.x); // -pi~pi
		if(abs(angle) < FLT_MIN)
		{
			int2 p_xy = int2(pos_ip_ss.x + 0.5, pos_ip_ss.y + 0.5);
			int addr_base_at_ray = (p_xy.y * g_cbCamState.rt_width + p_xy.x) * bytes_frags_per_pixel;
			int frag_cnt_at_ray = (int)fragment_counter[p_xy];
			for (int k = 0; k < frag_cnt_at_ray; k++)
			{
				float4 f_vis = GetUintVisAt(addr_base_at_ray, k, p_xy);
				acc_ray_vis += f_vis * (1.0 - acc_ray_vis.a);
			}
			acc_lens_vis += acc_ray_vis;
			num_hit_rays++;
			//rw_fragment_blendout[DTid.xy] = float4(0, 1, 0, 1); return;
			
			continue; // next ray
		}

		float lens_coc_ip_const = length(pos_lens) * pix_coc_r_const / (g_cbEnv.dof_focus_z - dof_lens_F);

		// note that, herein, eye ray and lens ray are not parallel!
		// , which implies that we consider only a hit per layer (k)
		// for each fragment layer, find a possible intersection point 
		// by iteratively sampling the surface (fragment layer) at interval endpoints to refine a piecewise-linear approximation
		[loop]
		//for (int k = 0; k < (int)k_value; k++)
			int k = 0;
		{
			float z_min = asfloat(global_z_minmax_buffer[0 + 2 * k]);
			float z_max = asfloat(global_z_minmax_buffer[1 + 2 * k]);
			if (z_min == 0) break; // no more layers so not continue

			float3 p_lensray_s, p_lensray_e;
			float2 pp = ComputeLensrayPosOnViewFrustom(p_lensray_s, p_lensray_e, pos_lens, v_lens_ray, v_rect_max_unit, float2(z_min, z_max));
			//rw_fragment_blendout[DTid.xy] = float4(float3(pp.x / g_cbCamState.rt_width, pp.y / g_cbCamState.rt_height, 0), 1); return;
#if RAY_LAYER_CULLING == 1
			// local layer culling based on "Depth-of-Field Rendering with Multiview Synthesis, 2009, TOG"
			float2 z_minmax;
			//bool is_valid_footprint = ComputeFootprintMinMaxZ(z_minmax, p_lensray_s, p_lensray_e, float2(z_min, z_max));
			//if (z_minmax.x == 0 || !is_valid_footprint)
			int is_valid_footprint = ComputeFootprintMinMaxZ(z_minmax, p_lensray_s, p_lensray_e, k, float2(z_min, z_max));
			if (z_minmax.x == 0)//  || is_valid_footprint == 1)
			{
				//if (is_valid_footprint >= 100)
				//{
				//	rw_fragment_blendout[DTid.xy] = float4(1, 0, 0, 1); return;
				//}
				//if (is_valid_footprint >= 8)
				//{
				//	rw_fragment_blendout[DTid.xy] = float4(0, 1, 0, 1); return;
				//}
				rw_fragment_blendout[DTid.xy] = float4((float3)is_valid_footprint / 8.0, 1); return;
				//rw_fragment_blendout[DTid.xy] = float4(float3(pp.x / g_cbCamState.rt_width, pp.y / g_cbCamState.rt_height, 0), 1); return;
				////rw_fragment_blendout[DTid.xy] = !is_valid_footprint ? float4(0, 0, 1, 1) : float4(0, 0, 0, 1);
				//if (is_valid_footprint == 1) rw_fragment_blendout[DTid.xy] = float4(1, 0, 0, 1);
				//else if (is_valid_footprint == 2) rw_fragment_blendout[DTid.xy] = float4(0, 1, 0, 1);
				//else if (is_valid_footprint == 3) rw_fragment_blendout[DTid.xy] = float4(1, 1, 0, 1);
				//else if (z_minmax.x == 0) rw_fragment_blendout[DTid.xy] = float4((float3)is_valid_footprint / 8.0, 1);
				//else rw_fragment_blendout[DTid.xy] = float4(0, 0, 1, 1);	
				//rw_fragment_blendout[DTid.xy] = is_valid_footprint == 1 ? float4(0, 0, 1, 1) : is_valid_footprint == 2 ? float4(0, 0, 0, 1);
				//return;
				break; // no more layers along the ray footprint so not continue
			}
			z_min = z_minmax.x;
			z_max = z_minmax.y;
			p_lensray_s = ComputeCSPosPlaneZ(-z_min, pos_lens, v_lens_ray);
			p_lensray_e = ComputeCSPosPlaneZ(-z_max, pos_lens, v_lens_ray);
#endif

#define MAX_ITERATIONS 5
#define MAX_PIXEL_TOLERENCE 5.0
			float3 p_eyeray_s = ComputeCSPosPersFromKB(p_lensray_s, z_max, k, bytes_frags_per_pixel);
			float3 p_eyeray_e = ComputeCSPosPersFromKB(p_lensray_e, z_min, k, bytes_frags_per_pixel);
			if (z_max - z_min < FLT_MIN)
			{
				//z_max += z_min + g_cbCamState.near_plane;
				float3 pos_m = (p_eyeray_s + p_eyeray_e) * 0.5;
				float3 p_ss = TransformPoint(pos_m, g_cbCamState.mat_ws2ss);
				int2 p_xy = int2(p_ss.x + 0.5, p_ss.y + 0.5);
				int frag_cnt = (int)fragment_counter[p_xy];
				if (frag_cnt > k)
				{
					int addr_base = (p_xy.y * g_cbCamState.rt_width + p_xy.x) * bytes_frags_per_pixel;
					float4 f_vis = GetUintVisAt(addr_base, k, p_xy);
					acc_ray_vis += f_vis * (1.0 - acc_ray_vis.a);
					if (acc_ray_vis.a > ERT_ALPHA) break;
				}
				//rw_fragment_blendout[DTid.xy] = float4(1, 0, 1, 1); return;
				continue; // hit test finished at the (k)-th layer
			}

			float3 p_f; int2 p_f_xy;
			float step_dist_ss = 0;
			float3 p_eyeray_s_ss = TransformPoint(p_eyeray_s, g_cbCamState.mat_ws2ss);
			float3 p_eyeray_e_ss = TransformPoint(p_eyeray_e, g_cbCamState.mat_ws2ss);
			float3 v_eye_ray_se_ss = p_eyeray_e_ss - p_eyeray_s_ss;
			float3 v_eye_ray_se = p_eyeray_e - p_eyeray_s;
			float interval_lensray_footprint = length(v_eye_ray_se_ss.xy) / MAX_ITERATIONS;
			float3 p_eyeray_s_iter = p_eyeray_s;
			for (int s = 1; s <= MAX_ITERATIONS; s++)
			{
				step_dist_ss += interval_lensray_footprint;
				if (step_dist_ss >= MAX_PIXEL_TOLERENCE || s == MAX_ITERATIONS)
				{
					float3 p_eyeray_e_iter = p_eyeray_s + v_eye_ray_se * (float)s / (float)MAX_ITERATIONS;
					if (PiecewiseIntersection(p_f, p_f_xy, p_lensray_s, p_lensray_e, p_eyeray_s_iter, p_eyeray_e_iter, z_min, z_max, k, bytes_frags_per_pixel))
					{
						int addr_base_f = (p_f_xy.y * g_cbCamState.rt_width + p_f_xy.x) * bytes_frags_per_pixel;
						float4 f_vis = GetUintVisAt(addr_base_f, k, p_f_xy);

						acc_ray_vis += f_vis * (1.0 - acc_ray_vis.a);
					}
					step_dist_ss = 0;
					p_eyeray_s_iter = p_eyeray_e_iter;
				}
			}

			if (acc_ray_vis.a > ERT_ALPHA) break;
		} // for (int k = 0; k < (int)k_value; k++)
		// later, replace optical compositing
		// .. incorrect alpha coverage
		// cannot handle rays that hit no frag
		if (acc_ray_vis.a > 0)
		{
			acc_lens_vis += acc_ray_vis;
			num_hit_rays++;
		}
		else _test_cnt_no_hit++;
	} // for (int iray = 0; iray < g_cbEnv.dof_lens_ray_num_samples; iray++)
	acc_lens_vis /= g_cbEnv.dof_lens_ray_num_samples;
	rw_fragment_blendout[DTid.xy] = acc_lens_vis;
	//rw_fragment_blendout[DTid.xy] = float4((float3)_test_cnt_no_hit / 8.0, 1);
	return;
}

[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void KB_SSDOF_RT_(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	if (DTid.x >= g_cbCamState.rt_width || DTid.y >= g_cbCamState.rt_height)
		return;
	//return;
	// at this moment, only static k buffer is supported.
	const uint k_value = g_cbCamState.k_value;
	uint bytes_per_frag = 4 * NUM_ELES_PER_FRAG;
	uint bytes_frags_per_pixel = k_value * bytes_per_frag; // to do : consider the dynamic scheme. (4 bytes unit)
	uint pixel_id = DTid.y * g_cbCamState.rt_width + DTid.x;
	uint addr_base_focalray = pixel_id * bytes_frags_per_pixel;

	//int frag_cnt = (int)fragment_counter[DTid.xy];
	//uint vr_hit_enc = frag_cnt >> 24;
	//frag_cnt = frag_cnt & 0xFFF;

	if (g_cbCamState.cam_flag & 0x1 == 0)
	{
		rw_fragment_blendout[DTid.xy] = float4(1, 0, 0, 1); // INVALID IN ORTHOGONAL PROJECTION
		return;
	}

	float3 pos_ip_ss = float3(DTid.xy, 0.0);
	// g_cbCamState.mat_ss2ws is used as ss2cs
	float3 pos_ip_cs = TransformPoint(pos_ip_ss, g_cbCamState.mat_ss2ws);
	float3 vec_eyeray_cs = normalize(pos_ip_cs);

	float3 pos_focus = ComputeCSPosPlaneZ(-g_cbEnv.dof_focus_z, (float3)0, vec_eyeray_cs);

	float3 ipO_cs = TransformPoint(float3(g_cbCamState.rt_width / 2.f, g_cbCamState.rt_height / 2.f, 0), g_cbCamState.mat_ss2ws);
	float3 ipx_cs = TransformPoint(float3(g_cbCamState.rt_width / 2.f + 1.f, g_cbCamState.rt_height / 2.f, 0), g_cbCamState.mat_ss2ws);
	float3 pix_cs = ipx_cs - ipO_cs;
	float pix_r = length(pix_cs) * 0.5; // SCALE for safe hit
	//float ratio_rn_zn = pix_r / g_cbCamState.near_plane;
	// g_cbCamState.near_plane is Focal Length?!
	float dof_lens_F = g_cbEnv.dof_sensor_z * g_cbEnv.dof_focus_z / (g_cbEnv.dof_sensor_z + g_cbEnv.dof_focus_z);
	float dof_ratio_sensor_to_ip = g_cbCamState.near_plane / g_cbEnv.dof_sensor_z;
	float pix_coc_r_const = dof_lens_F * dof_ratio_sensor_to_ip;

	// for fetch minmax z values from z_minmax_textures
	int half_w = g_cbCamState.rt_width / DOWN_SAMPLE;
	int half_h = g_cbCamState.rt_height / DOWN_SAMPLE;
	// entire layers culling...
	// in most practical cases, this is not efficient...
	// so, I do not perform this culling test (but do the layer's culling test per intersection ray)
#if EARLY_ENTIRE_LAYERS_CULLING == 1
	{
		float g_z_min = FLT_MAX, g_z_max = 0;
		for (int k = 0; k < (int)k_value; k++)
		{
			float z_min = asfloat(global_z_minmax_buffer[0 + 2 * k]);
			if (z_min == 0) break;
			float z_max = asfloat(global_z_minmax_buffer[1 + 2 * k]);
			g_z_min = min(g_z_min, z_min);
			g_z_max = max(g_z_max, z_max);
		}
		if (g_z_max == 0) return;

		float3 pos_lens_bb = float3(-1, 1, 0) * g_cbEnv.dof_lens_r;
		float lens_coc_ip_const_bb = length(pos_lens_bb) * pix_coc_r_const / (g_cbEnv.dof_focus_z - dof_lens_F);
		float coc_r_ip_zmin = lens_coc_ip_const_bb * abs(g_z_min - g_cbEnv.dof_focus_z) / g_z_min;
		float coc_r_ip_zmax = lens_coc_ip_const_bb * abs(g_z_max - g_cbEnv.dof_focus_z) / g_z_max;
		float3 v_lens_ray_bb = normalize(pos_focus - pos_lens_bb);
		float3 p_lensray_on_zplane = ComputeCSPosPlaneZ(coc_r_ip_zmin > coc_r_ip_zmax ? -g_z_min : -g_z_max, pos_lens_bb, v_lens_ray_bb);
		float3 p_lensray_on_zplane_ss = TransformPoint(p_lensray_on_zplane, g_cbCamState.mat_ws2ss);

		float max_coc_ip = max(coc_r_ip_zmin, coc_r_ip_zmax) / (pix_r); // diameter
		uint nbuf_idx = max(ceil(log2(max_coc_ip)) - 2, 0);
		int w_load_offset = ((nbuf_idx) % DOWN_SAMPLE) * half_w;
		int h_load_offset = ((nbuf_idx) / DOWN_SAMPLE) * half_h;
		float2 p_ss = p_lensray_on_zplane_ss.xy;
		uint2 p_xy = uint2(p_ss / DOWN_SAMPLE + (float2)0.5);
		// note that, in the k-buffer structure, 0-th layer represents 1st hit along eye ray.
		float2 z_minmax = z_minmax_textures[int3(p_xy + uint2(w_load_offset, h_load_offset), 0)];
		if (z_minmax.x == 0)
		{
			//rw_fragment_blendout[DTid.xy] = float4(1, 1, 0, 1);
			return;
		}
	}
#endif

#define ITERATION_RAY_HIT_TEST 3

	float4 acc_lens_vis = (float4)0;
	int num_hit_rays = 0;
	int _test_cnt_no_hit = 0;
	[loop]
	for (int iray = 0; iray < g_cbEnv.dof_lens_ray_num_samples; iray++)
	{
		// random pos_lens, v_lens_ray
		// g_cbEnv.dof_lens_r
		int hash_idx = DTid.x + g_cbCamState.rt_width * DTid.y + iray * g_cbCamState.rt_width * g_cbCamState.rt_height;
		// jittering
		//float3 random_sample = float3(
		//	_random(hash_idx),
		//	_random(hash_idx + g_cbCamState.rt_width * g_cbCamState.rt_height * g_cbEnv.dof_lens_ray_num_samples), 0);//
		//	//_random(hash_idx + g_cbCamState.rt_width * g_cbCamState.rt_height * g_cbEnv.dof_lens_ray_num_samples * 2));

		float3 random_sample = mul(AngleAxis3x3(F_PI / g_cbEnv.dof_lens_ray_num_samples * iray, float3(0, 0, -1)), float3(0.5, 0, 0));

		float3 pos_lens = (random_sample - float3(0.5, 0.5, 0)) * 2.0 * g_cbEnv.dof_lens_r;
		float3 v_lens_ray = normalize(pos_focus - pos_lens);

		float4 acc_ray_vis = (float4)0;
		float acc_alpha_wo_coc = 0;

		// is parallel with vec_eyeray_cs
		float3 vdiff = v_lens_ray - vec_eyeray_cs;
		float angle = atan2(vdiff.y, vdiff.x); // -pi~pi
		if (abs(angle) < FLT_MIN)
		{
			int2 p_xy = int2(pos_ip_ss.x + 0.5, pos_ip_ss.y + 0.5);
			int addr_base_at_ray = (p_xy.y * g_cbCamState.rt_width + p_xy.x) * bytes_frags_per_pixel;
			int frag_cnt_at_ray = (int)fragment_counter[p_xy];
			for (int k = 0; k < frag_cnt_at_ray; k++)
			{
				acc_ray_vis += GetUintVisAt(addr_base_at_ray, k, p_xy) * (1.0 - acc_ray_vis.a);
			}
			acc_lens_vis += acc_ray_vis;
			num_hit_rays++;
			//rw_fragment_blendout[DTid.xy] = float4(0, 1, 0, 1); return;

			continue; // next ray
		}

		float lens_coc_ip_const = length(pos_lens) * pix_coc_r_const / (g_cbEnv.dof_focus_z - dof_lens_F);

		// layer culling "Depth-of-Field Rendering with Multiview Synthesis, 2009, TOG"


		// note that, herein, eye ray and lens ray are not parallel!
		// , which implies that we consider only a hit per layer (k)
		// for each fragment layer, find a possible intersection point 
		// by iteratively sampling the surface (fragment layer) at interval endpoints to refine a piecewise-linear approximation
		[loop]
		for (int k = 0; k < (int)k_value; k++)
		{
			float z_min = asfloat(global_z_minmax_buffer[0 + 2 * k]);
			float z_max = asfloat(global_z_minmax_buffer[1 + 2 * k]);
			if (z_min == 0) break; // no more layers so not continue

			float3 p_lensray_s = ComputeCSPosPlaneZ(-z_min, pos_lens, v_lens_ray);
			float3 p_lensray_e = ComputeCSPosPlaneZ(-z_max, pos_lens, v_lens_ray);

			float3 p_lensray_ss_s = TransformPoint(p_lensray_s, g_cbCamState.mat_ws2ss);
			float3 p_lensray_ss_e = TransformPoint(p_lensray_e, g_cbCamState.mat_ws2ss);
			float2 v_lensray_ss_se = p_lensray_ss_e.xy - p_lensray_ss_s.xy;
#if RAY_LAYER_CULLING == 1
			float2 p_footprint_rect_min_ss = float2(min(p_lensray_ss_s.x, p_lensray_ss_e.x), min(p_lensray_ss_s.y, p_lensray_ss_e.y));
			float footprint_rect_size = max(abs(v_lensray_ss_se.x), abs(v_lensray_ss_se.y));
			uint nbuf_idx = max(ceil(log2(footprint_rect_size)) - 2, 0);
			int w_load_offset = ((nbuf_idx) % DOWN_SAMPLE) * half_w;
			int h_load_offset = ((nbuf_idx) / DOWN_SAMPLE) * half_h;
			uint2 p_fetch_xy = uint2(p_footprint_rect_min_ss / DOWN_SAMPLE + (float2)0.5);
			// note that, in the k-buffer structure, 0-th layer represents 1st hit along eye ray.
			float2 z_minmax = z_minmax_textures[int3(p_fetch_xy + uint2(w_load_offset, h_load_offset), k)];
			if (z_minmax.x == 0)
			{
				//rw_fragment_blendout[DTid.xy] = float4(0, 1, 1, 1);
				//return;
				break; // no more layers along the ray footprint so not continue
			}
			z_min = z_minmax.x;
			z_max = z_minmax.y;
			p_lensray_s = ComputeCSPosPlaneZ(-z_min, pos_lens, v_lens_ray);
			p_lensray_e = ComputeCSPosPlaneZ(-z_max, pos_lens, v_lens_ray);
#endif

#define OPT_MIXMAXZ_SAMPLES 3
#if OPT_MIXMAXZ_SAMPLES > 1
			// update z_min/max, p_lensray_s/p_lensray_e with a more narrow depth interval
			uint2 prev_p_xy = (uint2)0;
			float nrw_z_min = FLT_MAX, nrw_z_max = 0;
			for (int o = 0; o < OPT_MIXMAXZ_SAMPLES; o++)
			{
				float2 p_ss = p_lensray_ss_s.xy + v_lensray_ss_se * (float)o / (float)(OPT_MIXMAXZ_SAMPLES - 1);
				uint2 p_xy = uint2(p_ss / 4.0 + (float2)0.5);
				uint2 pdiff = p_xy - prev_p_xy;
				if (dot(pdiff, pdiff) == 0) continue;
				float2 z_minmax = z_minmax_textures[int3(p_xy, k)];
				if (z_minmax.x > 0)
				{
					nrw_z_min = min(nrw_z_min, z_minmax.x);
					nrw_z_max = max(nrw_z_max, z_minmax.y);
				}
				prev_p_xy = p_xy;
			}
			if (nrw_z_max > 0)
			{
				z_min = max(z_min, nrw_z_min);
				z_max = min(z_max, nrw_z_max);
				p_lensray_s = ComputeCSPosPlaneZ(-z_min, pos_lens, v_lens_ray);
				p_lensray_e = ComputeCSPosPlaneZ(-z_max, pos_lens, v_lens_ray);
			}
#endif

			float3 p_eyeray_s = ComputeCSPosPersFromKB(p_lensray_s, z_max, k, bytes_frags_per_pixel);
			float3 p_eyeray_e = ComputeCSPosPersFromKB(p_lensray_e, z_min, k, bytes_frags_per_pixel);

			if (z_max - z_min < FLT_MIN)
			{
				float3 pos_m = (p_eyeray_s + p_eyeray_e) * 0.5;
				float3 p_ss = TransformPoint(pos_m, g_cbCamState.mat_ws2ss);
				int2 p_xy = int2(p_ss.x + 0.5, p_ss.y + 0.5);
				int frag_cnt = (int)fragment_counter[p_xy];
				if (frag_cnt > k)
				{
					int addr_base = (p_xy.y * g_cbCamState.rt_width + p_xy.x) * bytes_frags_per_pixel;
					acc_ray_vis += GetUintVisAt(addr_base, k, p_xy) * (1.0 - acc_ray_vis.a);
				}

				//rw_fragment_blendout[DTid.xy] = float4(1, 0, 1, 1); return;

				continue; // hit test finished at the (k)-th layer
			}
			//rw_fragment_blendout[DTid.xy] = float4(1, 0, 0, 1); return;

			// find a possible intersection point 
			// through a piecewise-linear approximation 
			// "Depth-of-Field Rendering with Multiview Synthesis, 2009, TOG"
			for (int i = 0; i < ITERATION_RAY_HIT_TEST; i++)
			{
				float3 p_i = IntersectionLinePoints(p_eyeray_s, p_eyeray_e, p_lensray_s, p_lensray_e);
				float3 p_is = ComputeCSPosPersFromKB(p_i, (uint)i % 2 == 0 ? z_min : z_max, k, bytes_frags_per_pixel);
				if (p_i.z < p_is.z)
					p_eyeray_e = p_is;
				else
					p_eyeray_s = p_is;
			}
			// hit 에서 값을 제대로 찾는지 확인...
			float3 p_f = (p_eyeray_s + p_eyeray_e) * 0.5;
			float3 p_f_ss = TransformPoint(p_f, g_cbCamState.mat_ws2ss);
			int2 p_f_xy = int2(p_f_ss.x + 0.5, p_f_ss.y + 0.5);
			int frag_cnt_along_hit_eyeray = (int)fragment_counter[p_f_xy];
			if (k < frag_cnt_along_hit_eyeray)
			{
				//rw_fragment_blendout[DTid.xy] = float4(1, 0, 0, 1); return;
				int addr_base_f = (p_f_xy.y * g_cbCamState.rt_width + p_f_xy.x) * bytes_frags_per_pixel;
				float4 f_vis = GetUintVisAt(addr_base_f, k, p_f_xy);

				float hit_z = -p_f.z;
				float coc_r_ip = lens_coc_ip_const * abs(hit_z - g_cbEnv.dof_focus_z) / hit_z;
				float blur_w = 1.f;
				//if (coc_r_ip > pix_r)
				{
					// use DOB setting (degree of blurring, refer to "Real-Time Depth-of-Field Rendering Using Anisotropically Filtered Mipmap Interpolation")
					float sigma = coc_r_ip / 2.0;
					blur_w = exp(-0.5* (coc_r_ip) * (coc_r_ip) / (sigma*sigma));
				}
				acc_alpha_wo_coc += f_vis.a * (1.0 - acc_ray_vis.a);

				//f_vis *= blur_w;
				acc_ray_vis += f_vis * (1.0 - acc_ray_vis.a);
			}
			if (acc_alpha_wo_coc > ERT_ALPHA) break;
		} // for (int k = 0; k < (int)k_value; k++)
		// later, replace optical compositing
		// .. incorrect alpha coverage
		// cannot handle rays that hit no frag
		if (acc_alpha_wo_coc > 0)
		{
			acc_lens_vis += acc_ray_vis;
			num_hit_rays++;
		}
		else _test_cnt_no_hit++;
	} // for (int iray = 0; iray < g_cbEnv.dof_lens_ray_num_samples; iray++)
	acc_lens_vis /= num_hit_rays;// g_cbEnv.dof_lens_ray_num_samples;
	rw_fragment_blendout[DTid.xy] = acc_lens_vis;
	//rw_fragment_blendout[DTid.xy] = float4((float3)_test_cnt_no_hit / 8.0, 1);
	return;
}

[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void KB_TO_MINMAXDEPTHTEXTURE(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	int half_w = g_cbCamState.rt_width / DOWN_SAMPLE;
	int half_h = g_cbCamState.rt_height / DOWN_SAMPLE;
	if (DTid.x >= (uint)half_w || DTid.y >= (uint)half_h)
		return;

	// at this moment, only static k buffer is supported.
	const int k_value = (int)g_cbCamState.k_value;
	uint bytes_per_frag = 4 * NUM_ELES_PER_FRAG;
	uint bytes_frags_per_pixel = k_value * bytes_per_frag; // to do : consider the dynamic scheme. (4 bytes unit)
	//uint pixel_id = DTid.y * g_cbCamState.rt_width + DTid.x;
	//uint addr_base = pixel_id * bytes_frags_per_pixel;

	uint2 xy_[DOWN_SAMPLE * DOWN_SAMPLE];
	uint addr_base_[DOWN_SAMPLE * DOWN_SAMPLE];
	int frag_cnt_[DOWN_SAMPLE * DOWN_SAMPLE];
	int sum_fs = 0;
	for (int _y = 0; _y < DOWN_SAMPLE; _y++)
		for (int _x = 0; _x < DOWN_SAMPLE; _x++)
		{
			uint2 xy = uint2(DTid.x * DOWN_SAMPLE + _x, DTid.y * DOWN_SAMPLE + _y);
			int idx = _x + _y * DOWN_SAMPLE;
			xy_[idx] = xy;
			addr_base_[idx] = (xy.y * g_cbCamState.rt_width + xy.x) * bytes_frags_per_pixel;

			int num_fs = fragment_counter[xy];
			frag_cnt_[idx] = num_fs;
			sum_fs += num_fs;
		}
	
	if (sum_fs == 0) return;

	//global min max
	[loop]
	for (int k = 0; k < k_value; k++)
	{
		float z_min = FLT_MAX, z_max = 0;
		for (int i = 0; i < DOWN_SAMPLE * DOWN_SAMPLE; i++)
		{
			int frag_cnt = frag_cnt_[i]; // current GPUs provide zero value when the address is out of the buffer/texture by default.
			if (k < frag_cnt) {

				// g_cbCamState.mat_ss2ws is used as ss2cs
				float3 pos_ip_cs = TransformPoint(float3(xy_[i], 0), g_cbCamState.mat_ss2ws);
				float3 ray_dir_cs = normalize(pos_ip_cs);
				uint addr_base = addr_base_[i];

#if ZT_MODEL == 1
				Fragment f;
				GET_FRAG(f, addr_base, k);

				float a = (f.i_vis >> 24) / 255.0;
				float zthick_merge = f.zthick - g_cbCamState.cam_vz_thickness;
				float check_merge = ((a - f.opacity_sum) * (a - f.opacity_sum) > (1.0 / 255)) && zthick_merge > 0.000001;

				float3 pos_f_b = pos_ip_cs + ray_dir_cs * f.z;
				float3 pos_f_f = pos_ip_cs + ray_dir_cs * (f.z - f.zthick);
				float frag_z = -pos_f_b.z;
				z_min = min(z_min, check_merge ? -pos_f_f.z : frag_z);
				z_max = max(z_max, frag_z);
#else
				float ray_dist_from_ip = LOAD1_KBUF_Z(deep_k_buf, addr_base, k);
				float3 pos_f = pos_ip_cs + ray_dir_cs * ray_dist_from_ip;
				float z = -pos_f.z;

				z_min = min(z_min, z);
				z_max = max(z_max, z);
#endif
				//if(z > g_cbCamState.far_plane)
				//{ rw_fragment_blendout[DTid.xy] = float4(1, 1, 0, 1); return; }
			}
		}

		if (z_max != 0)
		{
			//{ rw_fragment_blendout[DTid.xy] = float4(1, 0, 0, 1); return; }
			uint dummy;
			InterlockedMin(rw_global_z_minmax_buffer[0 + 2 * k], asuint(z_min), dummy);
			if (dummy == 0)  InterlockedExchange(rw_global_z_minmax_buffer[0 + 2 * k], asuint(z_min), dummy);
			InterlockedMax(rw_global_z_minmax_buffer[1 + 2 * k], asuint(z_max), dummy);

			rw_z_minmax_textures[int3(DTid.xy, k)] = float2(z_min, z_max);

			//if (asfloat(rw_global_z_minmax_buffer[0 + 2 * 1]) > 0) rw_fragment_blendout[DTid.xy] = float4(0, 1, 0, 1);
			//if ((rw_global_z_minmax_buffer[2]) == 1234) rw_fragment_blendout[DTid.xy] = float4(0, 1, 0, 1);
			//else rw_fragment_blendout[DTid.xy] = float4(1, 0, 0, 1);
		}
	}

	//if (asfloat(rw_global_z_minmax_buffer[0 + 2 * 1]) > 0) rw_fragment_blendout[DTid.xy] = float4(0, 1, 0, 1);
	//if ((rw_global_z_minmax_buffer[0]) == 1234) rw_fragment_blendout[DTid.xy] = float4(0, 1, 0, 1);
	//else rw_fragment_blendout[DTid.xy] = float4(1, 0, 0, 1);
}

[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void KB_TO_MINMAXDEPTH_NBUF(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	const uint nbuf_idx = (uint)g_cbCamState.iSrCamDummy__1 & 0xFF;
	int half_w = g_cbCamState.rt_width / DOWN_SAMPLE;
	int half_h = g_cbCamState.rt_height / DOWN_SAMPLE;
	if (DTid.x >= (uint)half_w || DTid.y >= (uint)half_h || nbuf_idx < 1)
		return;

	// at this moment, only static k buffer is supported.
	const int k_value = (int)g_cbCamState.k_value;

	int w_load_offset = ((nbuf_idx - 1) % DOWN_SAMPLE) * half_w;
	int h_load_offset = ((nbuf_idx - 1) / DOWN_SAMPLE) * half_h;
	int w_store_offset = (nbuf_idx % DOWN_SAMPLE) * half_w;
	int h_store_offset = (nbuf_idx / DOWN_SAMPLE) * half_h;

	int2 xy_[4] =
	{
		  int2(DTid.x + 0, DTid.y + 0)
		, int2(DTid.x + 1, DTid.y + 0)
		, int2(DTid.x + 0, DTid.y + 1)
		, int2(DTid.x + 1, DTid.y + 1)
	};

	[loop]
	for (int k = 0; k < k_value; k++)
	{
		float z_min = FLT_MAX, z_max = 0;
		for (int i = 0; i < 4; i++)
		{
			//float2 z_minmax = rw_z_minmax_textures.SampleLevel(g_samplerPoint, float3(xy_[i], k), 0).rg
			int2 xy = xy_[i];
			if (xy.x >= half_w || xy.y >= half_h) continue;

			float2 z_minmax = rw_z_minmax_textures[int3(xy_[i] + int2(w_load_offset, h_load_offset), k)];
			if (z_minmax.y != 0)
			{
				z_min = min(z_minmax.x, z_min);
				z_max = max(z_minmax.y, z_max);
			}
		}

		if (z_max != 0)
			rw_z_minmax_textures[int3(DTid.xy + int2(w_store_offset, h_store_offset), k)] = float2(z_min, z_max);
	}
}