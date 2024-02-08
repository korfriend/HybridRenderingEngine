#include "Sr_Common.hlsl"

RWTexture2D<unorm float4> fragment_rgba_out : register(u1);
RWTexture2D<float> fragment_zdepth_out : register(u2);
RWTexture2D<unorm float4> fragment_rgba_secondLayer : register(u3);
RWTexture2D<float> fragment_zdepth_secondLayer : register(u4);

[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void Blend2ndLayer(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= g_cbCamState.rt_width || DTid.y >= g_cbCamState.rt_height)
        return;

    float4 secondLayerVis = fragment_rgba_secondLayer[DTid.xy];
    if (secondLayerVis.a == 0) 
        return;

    //fragment_rgba_out[DTid.xy] = float4(1, 0, 0, 1);
    //return;

    float4 rtVis = fragment_rgba_out[DTid.xy];
    float secondLayerDepth = fragment_zdepth_secondLayer[DTid.xy];
    if (rtVis.a == 0) {
        fragment_rgba_out[DTid.xy] = float4(secondLayerVis.rgb, 1.f);
        fragment_zdepth_out[DTid.xy] = secondLayerDepth;
        return;
    }

    const uint dotSize = g_cbCamState.iSrCamDummy__0 & 0xFF;
    bool showPattern = dotSize > 0;
    const int modSize = dotSize * 2;
    float rtDepth = fragment_zdepth_out[DTid.xy];
    bool isRtFront = rtDepth < secondLayerDepth;
    if (showPattern && isRtFront)
    {
        uint modY = DTid.y % modSize;
        if (modY < dotSize) {
            if (DTid.x % modSize < dotSize) secondLayerVis.rgb = (float3)0;
        }
        else {
            if (DTid.x % modSize >= dotSize) secondLayerVis.rgb = (float3)0;
        }
    }

    secondLayerVis.a = 1.f;
    //vis_out += vis_sample * (1.f - vis_out.a);
    float4 frontVis = rtVis, backVis = secondLayerVis;
    float finalDepth = rtDepth;
    if (!isRtFront) {
        fragment_rgba_out[DTid.xy] = secondLayerVis;
        fragment_zdepth_out[DTid.xy] = secondLayerDepth;
        return;
    }

    float blendingW = 1.f - ((g_cbCamState.iSrCamDummy__0 >> 8) & 0xFF) / 100.f;

    frontVis.a *= blendingW;
    if (secondLayerVis.r + secondLayerVis.g + secondLayerVis.b == 0 && showPattern)
        frontVis.a *= 0.5f;
    float4 finalVis = float4(frontVis.rgb * frontVis.a, frontVis.a) + float4(backVis.rgb * (1 - frontVis.a), (1 - frontVis.a));

    fragment_rgba_out[DTid.xy] = finalVis;
    fragment_zdepth_out[DTid.xy] = finalDepth;
    return;
}