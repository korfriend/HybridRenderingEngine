#include "../Sr_Common.hlsl"
#include "ParticleInterop.hlsli"

ByteAddressBuffer vertexBuffer_POS : register(t53);
ByteAddressBuffer vertexBuffer_NOR : register(t54);
Buffer<float2> vertexBuffer_UVS : register(t55);
Buffer<float4> vertexBuffer_COL : register(t56);

StructuredBuffer<Particle> particleBuffer : register(t50);
StructuredBuffer<uint> culledIndirectionBuffer : register(t51);
StructuredBuffer<uint> culledIndirectionBuffer2 : register(t52);

VS_OUTPUT CommonVS_IDX(uint vid : SV_VertexID, uint instanceID : SV_InstanceID)
{
    uint particleIndex = culledIndirectionBuffer2[culledIndirectionBuffer[instanceID]];
    uint vertexID = particleIndex * 4 + vid;

    //float3 position = vertexBuffer_POS[vertexID].xyz;
    uint3 u3Position = vertexBuffer_POS.Load3(vertexID * 3 * 4);
    float3 position = asfloat(u3Position);
    //float3 normal = vertexBuffer_NOR[vertexID].xyz;
    uint3 u3Normal = vertexBuffer_NOR.Load3(vertexID * 3 * 4);
    float3 normal = asfloat(u3Normal);


    float2 uv = vertexBuffer_UVS[vertexID];
    float4 color = vertexBuffer_COL[vertexID];

    Particle particle = particleBuffer[particleIndex];

    // calculate render properties from life:
    float lifeLerp = 1 - particle.life / particle.maxLife;
    float size = lerp(particle.sizeBeginEnd.x, particle.sizeBeginEnd.y, lifeLerp);

    // Sprite sheet UV transform:
    const float spriteframe = g_emitter.xEmitterFrameRate == 0 ?
        lerp(g_emitter.xEmitterFrameStart, g_emitter.xEmitterFrameCount, lifeLerp) :
        ((g_emitter.xEmitterFrameStart + particle.life * g_emitter.xEmitterFrameRate) % g_emitter.xEmitterFrameCount);
    const float frameBlend = frac(spriteframe);

    //VertextoPixel Out;
    //Out.P = position;
    //Out.pos = float4(position, 1);
    //Out.clip = dot(Out.pos, GetCamera().clip_plane);
    //Out.pos = mul(GetCamera().view_projection, Out.pos);
    //Out.tex = min16float4(uvsets);
    //Out.size = min16float(size);
    //Out.color = pack_rgba(color);
    //Out.unrotated_uv = min16float2(BILLBOARD[vertexID % 4].xy * float2(1, -1) * 0.5f + 0.5f);
    //Out.frameBlend = min16float(frameBlend);
    //return Out;

    VS_OUTPUT vout = (VS_OUTPUT)0;
    vout.f4PosSS = mul(g_cbPobj.mat_os2ps, float4(position, 1.f));
    vout.f3PosWS = TransformPoint(position, g_cbPobj.mat_os2ws);
    vout.f3VecNormalWS = normalize(TransformVector(normal, g_cbPobj.mat_os2ws));
    vout.f3Custom = float3(uv, 1.f);

    return vout;
}

//[earlydepthstencil]// ==> shader model 5
PS_FILL_OUTPUT ParticleRender(VS_OUTPUT input)
{
    PS_FILL_OUTPUT out_ps;
    out_ps.ds_z = 1.f; 
    out_ps.color = (float4)0;
    out_ps.depthcs = FLT_MAX;

    float4 v_rgba = (float4)1;// float4(input.f3Color, 1);
    
    POBJ_PRE_CONTEXT;

    //if (g_cbPobj.alpha == 0 || z_depth < 0 || (input.f4PosSS.z / input.f4PosSS.w < 0)
    //    || input.f4PosSS.x < 0 || input.f4PosSS.y < 0
    //    || (uint)input.f4PosSS.x >= g_cbCamState.rt_width
    //    || (uint)input.f4PosSS.y >= g_cbCamState.rt_height)
    //    clip(-1);

    out_ps.ds_z = input.f4PosSS.z;
    out_ps.color = v_rgba;
    out_ps.depthcs = z_depth;

    return out_ps;
}