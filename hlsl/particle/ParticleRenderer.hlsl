#include "../Sr_Common.hlsl"

static const float3 BILLBOARD[] = {
    float3(-1, -1, 0),	// 0
    float3(1, -1, 0),	// 1
    float3(-1, 1, 0),	// 2
    float3(1, 1, 0),	// 4
};

ByteAddressBuffer vertexBuffer_POS : register(t53);
ByteAddressBuffer vertexBuffer_NOR : register(t54);
Buffer<float2> vertexBuffer_UVS : register(t55);
Buffer<float4> vertexBuffer_COL : register(t56);

StructuredBuffer<Particle> particleBuffer : register(t50);
StructuredBuffer<uint> culledIndirectionBuffer : register(t51);
StructuredBuffer<uint> culledIndirectionBuffer2 : register(t52);

VS_OUTPUT CommonVS_IDX(uint vid : SV_VertexID, uint instanceID : SV_InstanceID)
{
    VS_OUTPUT vout = (VS_OUTPUT)0;
    //vout.f4PosSS = mul(g_cbPobj.mat_os2ps, float4(input.f3PosOS, 1.f));
    //vout.f3PosWS = TransformPoint(input.f3PosOS, g_cbPobj.mat_os2ws);
    //vout.f3VecNormalWS = normalize(TransformVector(input.f3VecNormalOS, g_cbPobj.mat_os2ws));
    //vout.f3Custom = input.f3Custom;
    return vout;
}

//[earlydepthstencil]// ==> shader model 5
PS_FILL_OUTPUT ParticleRender(VS_OUTPUT input)
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