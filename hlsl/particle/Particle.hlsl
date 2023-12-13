//#include "../CommonShader.hlsl"
#include "../Sr_Common.hlsl"

struct HxCB_Particle_Blob
{
	float4 xyzr_spheres[4];
	int4 color_spheres;
};

cbuffer cbGlobalParams : register(b11)
{
	HxCB_Particle_Blob g_cbPclBlob;
}

RWTexture2D<unorm float4> fragment_rgba_singleLayer : register(u1);
RWTexture2D<float> fragment_zdepth_singleLayer : register(u2);
RWTexture2D<float> fragment_temp_zdepth_singleLayer : register(u3);

float SmoothMin2(float a, float b, float k) {
    float h = clamp(0.5 + 0.5 * (a - b) / k, 0.0, 1.0);
    return lerp(a, b, h) - k * h * (1.0 - h);
}

float GetDist(float3 p, float smthCoef)
{
    float4 xyzr = g_cbPclBlob.xyzr_spheres[0];
    float d = length(p - xyzr.xyz) - xyzr.w;

    for (int i = 1; i < 4; i++) {
        float4 xyzr = g_cbPclBlob.xyzr_spheres[i];
        float newDist = length(p - xyzr.xyz) - xyzr.w;
        d = SmoothMin2(newDist, d, smthCoef);
    }

    return d;
}

float3 GetNormal(float3 p, float smthCoef)
{
    float d = GetDist(p, smthCoef); // Distance
    float2 e = float2(.01, 0); // Epsilon

    float3 n = d - float3(
        GetDist(p - e.xyy, smthCoef),  // e.xyy is the same as vec3(.01,0,0). The x of e is .01. this is called a swizzle
        GetDist(p - e.yxy, smthCoef),
        GetDist(p - e.yyx, smthCoef));

    return normalize(n);
}

float4 GetColor(float3 p, float smthCoef)
{
    float4 xyzr = g_cbPclBlob.xyzr_spheres[0];
    float4 rgba = ConvertUIntToFloat4(g_cbPclBlob.color_spheres[0]);
    float d = length(p - xyzr.xyz) - xyzr.w;

    for (int i = 1; i < 4; i++) {
        xyzr = g_cbPclBlob.xyzr_spheres[i];
        float newDist = length(p - xyzr.xyz) - xyzr.w;

        float h = clamp(0.5 + 0.5 * (d - newDist) / smthCoef, 0.0, 1.0);
        rgba = lerp(rgba, ConvertUIntToFloat4(g_cbPclBlob.color_spheres[i]), h);

        d = SmoothMin2(newDist, d, smthCoef);
    }

    //rgba.a = 1;
    return rgba;
}

float2 ComputeAABBHits(const float3 posStart, const float3 posMin,
    const float3 posMax, const float3 vecDir)
{
    // intersect ray with a box
    // https://education.siggraph.org/static/HyperGraph/raytrace/rtinter3.htm
    float3 invR = float3(1.0f, 1.0f, 1.0f) / vecDir;
    float3 tbot = invR * (posMin - posStart);
    float3 ttop = invR * (posMax - posStart);

    // re-order intersections to find smallest and largest on each axis
    float3 tmin = min(ttop, tbot);
    float3 tmax = max(ttop, tbot);

    // find the largest tmin and the smallest tmax
    float largestTmin = max(max(tmin.x, tmin.y), max(tmin.x, tmin.z));
    float smallestTmax = min(min(tmax.x, tmax.y), min(tmax.x, tmax.z));

    float tnear = max(largestTmin, 0.f);
    float tfar = smallestTmax;
    return float2(tnear, tfar);
}

float3 PhongLighting(float3 color, float3 p, float3 v, float3 n)
{
    float3 Ka, Kd, Ks;
    float Ns = g_cbPobj.Ns;

    // note g_cbPobj's Ka, Kd, and Ks has already been multiplied by pb_shading_factor.xyz
    Ka = g_cbPobj.Ka;
    Kd = g_cbPobj.Kd;
    Ks = g_cbPobj.Ks;

    Ka *= g_cbEnv.ltint_ambient.rgb;
    Kd *= g_cbEnv.ltint_diffuse.rgb;
    Ks *= g_cbEnv.ltint_spec.rgb;

    float3 l = -g_cbEnv.dir_light_ws;
    if (g_cbEnv.env_flag & 0x1) // point light
        l = -normalize(p - g_cbEnv.pos_light_ws);

    {
        float3 light_dirinv = -g_cbEnv.dir_light_ws;
        if (g_cbEnv.env_flag & 0x1)
            light_dirinv = -normalize(p - g_cbEnv.pos_light_ws);
        color = color * PhongBlinn(-v, light_dirinv, n, g_cbPobj.pobj_flag & (0x1 << 5), Ka, Kd, Ks, Ns);
    }

    return color;
}

[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void RayMarchingDistanceMap( uint3 DTid : SV_DispatchThreadID )
{
    if (DTid.x >= g_cbCamState.rt_width || DTid.y >= g_cbCamState.rt_height)
        return;

    float3 pos_ip_ss = float3(DTid.xy, 0.0f);
    float3 pos_ip_ws = TransformPoint(pos_ip_ss, g_cbCamState.mat_ss2ws);
    float3 dir_ray_unit_ws = g_cbCamState.dir_view_ws;
    if (g_cbCamState.cam_flag & 0x1)
        dir_ray_unit_ws = pos_ip_ws - g_cbCamState.pos_cam_ws;
    dir_ray_unit_ws = normalize(dir_ray_unit_ws);

    const float3 cubePosMin = float3(-50, -50, -50), cubePosMax = float3(50, 50, 50);
    const float smthCoef = 10.0f;
    float2 t = ComputeAABBHits(pos_ip_ws, cubePosMin, cubePosMax, dir_ray_unit_ws);


    if (t.x >= t.y)
    {
        //fragment_rgba_singleLayer[DTid.xy] = float4(1, 1, 1, 1);
        //fragment_zdepth_singleLayer[DTid.xy] = FLT_MAX;
        return;
    }

    float rd = t.x;
    bool isHit = false;
#define MAX_LOOP 100
    [loop]
    for (int step = 0; step < MAX_LOOP; step++) {

        float3 ray_pos = pos_ip_ws + dir_ray_unit_ws * rd;
        float d = GetDist(ray_pos, smthCoef);
        if (d <= 0.0001) {
            isHit = true;
            break;
        }
        else if (rd >= t.y) {
            break;
        }
        rd += d;
    }

    float4 color = (float4)0.f;
    float s_depth = FLT_MAX;
    if (isHit) {
        s_depth = rd;
        float3 p_surf = pos_ip_ws + dir_ray_unit_ws * rd;
        color = GetColor(p_surf, smthCoef);
        float3 n = GetNormal(p_surf, smthCoef);

        // (float3 color, float3 p, float3 v, float3 n)
        color.rgb = PhongLighting(color.rgb, p_surf, dir_ray_unit_ws, n);
    }

    fragment_rgba_singleLayer[DTid.xy] = color;
    fragment_zdepth_singleLayer[DTid.xy] = s_depth;
}