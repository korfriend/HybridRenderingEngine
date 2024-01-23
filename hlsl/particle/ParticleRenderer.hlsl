#include "../Sr_Common.hlsl"

//[earlydepthstencil]// ==> shader model 5
PS_FILL_OUTPUT ParticleRender(VS_OUTPUT_PNTC input)
{
    PS_FILL_OUTPUT out_ps;
    out_ps.ds_z = 1.f; // remove???
    out_ps.color = (float4)0;
    out_ps.depthcs = FLT_MAX;

    float4 v_rgba = (float4)1;// float4(input.f3Color, 1);
    
    POBJ_PRE_CONTEXT;

    if (g_cbPobj.alpha == 0 || z_depth < 0 || (input.f4PosSS.z / input.f4PosSS.w < 0)
        || input.f4PosSS.x < 0 || input.f4PosSS.y < 0
        || (uint)input.f4PosSS.x >= g_cbCamState.rt_width
        || (uint)input.f4PosSS.y >= g_cbCamState.rt_height)
        clip(-1);

    out_ps.ds_z = input.f4PosSS.z;
    out_ps.color = v_rgba;
    out_ps.depthcs = z_depth;

    return out_ps;
}