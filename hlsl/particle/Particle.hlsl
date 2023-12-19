//#include "../CommonShader.hlsl"
#include "../Sr_Common.hlsl"

struct HxCB_Particle_Blob
{
	float4 xyzr_spheres[4];
	int4 color_spheres;
    float smoothCoeff;
    float3 minRoiCube;
    uint dummy1;
    float3 maxRoiCube;
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

float SmoothMinN(float values[4], int n, float k)
{
    float sum = 0.0f;
    for (int i = 0; i < n; i++)
    {
        sum += exp(-k * values[i]);
    }
    return -log(sum) / k;
}

float SmoothMax(float a, float b, float k)
{
    return log(exp(k * a) + exp(k * b)) / k;
}

float SmoothMin1(float a, float b, float k)
{
    return -SmoothMax(-a, -b, k);
}

float GetDist(float3 p, float smthCoef)
{
    float4 xyzr = g_cbPclBlob.xyzr_spheres[0];
    float d = length(p - xyzr.xyz) - xyzr.w;

    for (int i = 1; i < 4; i++) {
        xyzr = g_cbPclBlob.xyzr_spheres[i];
        float newDist = length(p - xyzr.xyz) - xyzr.w;
        d = SmoothMin2(newDist, d, smthCoef);
    }

    return d;
}

float3 GetNormal(float3 p, float smthCoef)
{
    float d = GetDist(p, smthCoef); // Distance
    float2 e = float2(.1, 0); // Epsilon

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
    rgba.rgb *= rgba.a;
    float d = length(p - xyzr.xyz) - xyzr.w;

    //float prevAlphaW = rgba.a;
    for (int i = 1; i < 4; i++) {
        xyzr = g_cbPclBlob.xyzr_spheres[i];
        float newDist = length(p - xyzr.xyz) - xyzr.w;
        float4 newColor = ConvertUIntToFloat4(g_cbPclBlob.color_spheres[i]);
        newColor.rgb *= newColor.a;

        float h = clamp(0.5 + 0.5 * (newDist - d) / smthCoef, 0.0, 1.0);

        //rgba = MixOpt(rgba, prevAlphaW * h, newColor, newColor.a * (1.0 - h));
        //prevAlphaW = prevAlphaW * h + newColor.a * (1.0 - h);
        //prevAlphaW += prevAlphaW * (1 - h);

        rgba = lerp(newColor, rgba, h);

        d = lerp(newDist, d, h);
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

    const float3 cubePosMin = g_cbPclBlob.minRoiCube, cubePosMax = g_cbPclBlob.maxRoiCube;
    const float smthCoef = max(g_cbPclBlob.smoothCoeff, 0.001);
    float2 t = ComputeAABBHits(pos_ip_ws, cubePosMin, cubePosMax, dir_ray_unit_ws);


    if (t.x >= t.y)
    {
        //fragment_rgba_singleLayer[DTid.xy] = float4(1, 1, 1, 1);
        //fragment_zdepth_singleLayer[DTid.xy] = FLT_MAX;
        return;
    }

    float rd = t.x;
    bool isHit = false;
    const float minDist = 0.2f;
    float3 surf_pos = pos_ip_ws;

//#define __NO_RAY_SURF_REFINEMENT
    int debug_ray = 0;
    float prev_d = 0, d = 0;
#define MAX_LOOP 100
#define SURF_REFINEMENT 5
    [loop]
    for (int step = 0; step < MAX_LOOP; step++) {

        float3 ray_pos = pos_ip_ws + dir_ray_unit_ws * rd;
        prev_d = d;
        d = GetDist(ray_pos, smthCoef);

        if (d < 0) {
#ifdef __NO_RAY_SURF_REFINEMENT
            surf_pos = ray_pos;
#else
            // note that if d < 0, then previous step size musy be minDist
            // now surface refinement!
            float3 ray_pos_bis_s = ray_pos - dir_ray_unit_ws * minDist;
            float3 ray_pos_bis_e = ray_pos;
            [loop]
            for (int j = 0; j < SURF_REFINEMENT; j++)
            {
                float3 ray_pos_bisection = (ray_pos_bis_s + ray_pos_bis_e) * 0.5f;
                float d_bisection = GetDist(ray_pos_bisection, smthCoef);
                if (d_bisection < 0)
                    ray_pos_bis_e = ray_pos_bisection;
                else
                    ray_pos_bis_s = ray_pos_bisection;
            }
            surf_pos = (ray_pos_bis_s + ray_pos_bis_e) * 0.5f;
#endif
            rd = length(surf_pos - pos_ip_ws);
            isHit = true;
            debug_ray = 2;
            break;
        }
        else if (rd >= t.y) {
            debug_ray = 1;
            break;
        }

#ifdef __NO_RAY_SURF_REFINEMENT
        rd += d;
#else
        rd += max(d, minDist);
#endif
    }

    float4 color = (float4)0.f;
    float s_depth = FLT_MAX;
    if (isHit) {
        s_depth = rd; 
        color = GetColor(surf_pos, smthCoef);
        float3 n = GetNormal(surf_pos, smthCoef);

        // (float3 color, float3 p, float3 v, float3 n)
        color.rgb = PhongLighting(color.rgb, surf_pos, dir_ray_unit_ws, n);
    }

    //if (debug_ray == 0) color = float4(0, 1, 0, 1);
    //else if (debug_ray == 1) color = float4(0, 0, 1, 1);
    //else color = float4(1, 0, 0, 1);
    if (debug_ray == 3) color = float4(0, 1, 1, 1);

    fragment_rgba_singleLayer[DTid.xy] = color;
    fragment_zdepth_singleLayer[DTid.xy] = s_depth;
}