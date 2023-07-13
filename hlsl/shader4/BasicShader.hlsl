#include "../Sr_Common.hlsl"

[earlydepthstencil]// ==> shader model 5
PS_FILL_OUTPUT BasicShader4(__VS_OUT input)
{
	PS_FILL_OUTPUT out_ps;
	out_ps.ds_z = 1.f; // remove???
	out_ps.color = (float4)0;
	out_ps.depthcs = FLT_MAX;

	float4 v_rgba = (float4)0;
	float z_depth = FLT_MAX;

#if __RENDERING_MODE != 6
	BasicShader(input, v_rgba, z_depth);
#if DX10_0
    if (v_rgba.a <= 0.01) clip(-1);
    v_rgba.a = 1.f;
#endif
#else
	int2 tex2d_xy = int2(input.f4PosSS.xy);
	z_depth = sr_fragment_zdepth[tex2d_xy];
	int outlinePPack = g_cbCamState.iSrCamDummy__1;
	float3 outline_color = float3(((outlinePPack >> 16) & 0xFF) / 255.f, ((outlinePPack >> 8) & 0xFF) / 255.f, (outlinePPack & 0xFF) / 255.f);
	int pixThickness = (outlinePPack >> 24) & 0xFF;
	v_rgba = OutlineTest(tex2d_xy, z_depth, 10000.f, outline_color, pixThickness);
	//v_rgba = float4(tex2d_xy / 1000.f, 0, 1);
	//v_rgba = float4(1, 0, 0, 1);
	//z_depth = 10.f;
	if (v_rgba.a <= 0.01) clip(-1);
#endif

	out_ps.ds_z = input.f4PosSS.z;
	out_ps.color = v_rgba;
	out_ps.depthcs = z_depth;

	return out_ps;
}

PS_FILL_OUTPUT OutlineShader(__VS_OUT input)
{

}

// https://stackoverflow.com/questions/39404502/direct11-write-data-to-buffer-in-pixel-shader-like-ssbo-in-open
// [earlydepthstencil] ==> shader model 5

// https://www.scratchapixel.com/lessons/3d-basic-rendering/ray-tracing-rendering-a-triangle/ray-triangle-intersection-geometric-solution.html
bool rayTriangleIntersect(
    const float3 orig, const float3 dir,
    const float3 v0, const float3 v1, const float3 v2,
    out float t)
{
    // compute the plane's normal
    float3 v0v1 = v1 - v0;
    float3 v0v2 = v2 - v0;
    // no need to normalize
    float3 N = cross(v0v1, v0v2); // N
    float area2 = length(N);

    // Step 1: finding P

    // check if the ray and plane are parallel.
    float NdotRayDirection = dot(N, dir);
    if (abs(NdotRayDirection) < 0.0001f) // almost 0
        return false; // they are parallel, so they don't intersect! 

    // compute d parameter using equation 2
    float d = -dot(N, v0);

    // compute t (equation 3)
    t = -(dot(N, orig) + d) / NdotRayDirection;

    // check if the triangle is behind the ray
    if (t < 0) return false; // the triangle is behind

    // compute the intersection point using equation 1
    float3 P = orig + t * dir;

    // Step 2: inside-outside test
    float3 C; // vector perpendicular to triangle's plane

    // edge 0
    float3 edge0 = v1 - v0;
    float3 vp0 = P - v0;
    C = cross(edge0, vp0);
    if (dot(N, C) < 0) return false; // P is on the right side

    // edge 1
    float3 edge1 = v2 - v1;
    float3 vp1 = P - v1;
    C = cross(edge1, vp1);
    if (dot(N, C) < 0)  return false; // P is on the right side

    // edge 2
    float3 edge2 = v0 - v2;
    float3 vp2 = P - v2;
    C = cross(edge2, vp2);
    if (dot(N, C) < 0) return false; // P is on the right side;

    return true; // this ray hits the triangle
}


struct SO_OUTPUT
{
	float3 infoIntersection: TEXCOORD0;
};

[maxvertexcount(42)]
void GS_PickingPoint(uint pid : SV_PrimitiveId, triangle VS_OUTPUT input[3], inout PointStream<SO_OUTPUT> pointStream)
{
	float3 pos0 = input[0].f3PosWS;
	float3 pos1 = input[1].f3PosWS;
	float3 pos2 = input[2].f3PosWS;

	float3 ray_dir = g_cbCamState.dir_view_ws;
	float3 pos_ray_origin = g_cbCamState.pos_cam_ws;
	
    //SO_OUTPUT so_out_test;
    //so_out_test.infoIntersection.x = 11.f;
    //so_out_test.infoIntersection.y = asfloat(pid);
    //so_out_test.infoIntersection.z = asfloat(g_cbPobj.pobj_dummy_0);
    //pointStream.Append(so_out_test);
    //pointStream.RestartStrip();
    //return;
    
    // compute parameters
    float t;
	bool isValidIntersect = rayTriangleIntersect(pos_ray_origin, ray_dir, pos0, pos1, pos2, t);

    if (isValidIntersect) {
        float3 posIntersect = pos_ray_origin + ray_dir * t;

        if (g_cbClipInfo.clip_flag & 0x1)
            isValidIntersect = dot(g_cbClipInfo.vec_clipplane, posIntersect - g_cbClipInfo.pos_clipplane) <= 0;

        if (isValidIntersect) {
            if (g_cbClipInfo.clip_flag & 0x2)
                isValidIntersect = IsInsideClipBox(posIntersect, g_cbClipInfo.mat_clipbox_ws2bs);

            if (isValidIntersect) {
                SO_OUTPUT so_out;
                so_out.infoIntersection.x = t;
                so_out.infoIntersection.y = asfloat(pid);
                so_out.infoIntersection.z = asfloat(g_cbPobj.pobj_dummy_0);
                pointStream.Append(so_out);
            }
        }
    }
}

int GetSegmentPlaneIntersection(float3 planeN, float planeD, float3 P1, float3 P2, out float3 output[2]) {

    int count = 0;

    float d1 = dot(planeN, P1) + planeD;
    float d2 = dot(planeN, P2) + planeD;

    bool bP1OnPlane = (abs(d1) < FLT_MIN__);
    bool bP2OnPlane = (abs(d2) < FLT_MIN__);

    if (bP1OnPlane) {
        output[count++] = P1;
    }
    if (bP2OnPlane) {
        output[count++] = P2;
    }
    if (bP1OnPlane && bP2OnPlane)
        return count;

    if (d1 * d2 > FLT_MIN__)  // points on the same side of plane
        return count;

    float t = d1 / (d1 - d2); // 'time' of intersection point on the segment

    output[count++] = P1 + t * (P2 - P1);
    return count;
}

[maxvertexcount(42)]
void GS_MeshCutLines(uint pid : SV_PrimitiveId, triangle VS_OUTPUT input[3], inout PointStream<SO_OUTPUT> pointStream)
{
    float3 P1 = input[0].f3PosWS;
    float3 P2 = input[1].f3PosWS;
    float3 P3 = input[2].f3PosWS;

    float3 planeN = g_cbCamState.dir_view_ws;
    float3 planeP = g_cbCamState.pos_cam_ws;
    float plandD = -dot(planeN, planeP);

    // 4 cases :
    // no intersection point ==> no SO_OUTPUT
    // 1 intersection point ==> no SO_OUTPUT
    // 2 interseciton points ==> two SO_OUTPUTs
    // 3 intersection points ==> four SO_OUTPUTs (composed of P1, P2 an P3)

    float3 intersectionPts1[2];
    int count1 = GetSegmentPlaneIntersection(planeN, plandD, P1, P2, intersectionPts1);
    float3 intersectionPts2[2];
    int count2 = GetSegmentPlaneIntersection(planeN, plandD, P2, P3, intersectionPts2);

    SO_OUTPUT so_out;
    if (count1 + count2 == 2) {
        for (int i = 0; i < count1; i++) {
            so_out.infoIntersection = intersectionPts1[i];
            pointStream.Append(so_out);
        }
        for (i = 0; i < count2; i++) {
            so_out.infoIntersection = intersectionPts2[i];
            pointStream.Append(so_out);
        }
    }
    else if (count1 + count2 == 4) {
        so_out.infoIntersection = P1;
        pointStream.Append(so_out);
        so_out.infoIntersection = P2;
        pointStream.Append(so_out);
        so_out.infoIntersection = P2;
        pointStream.Append(so_out);
        so_out.infoIntersection = P3;
        pointStream.Append(so_out);
    }
    else {
        float3 intersectionPts3[2];
        int count3 = GetSegmentPlaneIntersection(planeN, plandD, P3, P1, intersectionPts3);
        if (count1 + count2 + count3 == 0) return;

        // note only two points are to be appended!
        if (count1) {
            so_out.infoIntersection = intersectionPts1[0];
            pointStream.Append(so_out);
        }
        if (count2) {
            so_out.infoIntersection = intersectionPts2[0];
            pointStream.Append(so_out);
        }
        if (count3) {
            so_out.infoIntersection = intersectionPts3[0];
            pointStream.Append(so_out);
        }
    }
}