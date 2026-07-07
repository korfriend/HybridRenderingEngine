#include "Sr_Common.hlsl"

// Temporal anti-aliasing resolve. The engine (RenderScene) drives a static, jittered accumulation: each
// frame the scene is rendered with a different sub-pixel jitter and this pass folds the freshly composited
// color (RENDER_OUT_RGBA_0) into a persistent per-camera history buffer using an unbiased running mean, then
// writes the average back to RENDER_OUT_RGBA_0 for presentation. iSrCamDummy__0 carries the number of samples
// already present in the history (n); the current frame becomes sample n+1.
RWTexture2D<unorm float4> fragment_rgba_out : register(u1); // RENDER_OUT_RGBA_0 : current frame in, resolved out
RWTexture2D<float4> taa_history : register(u2);             // TAA_HISTORY_RGBA (R16G16B16A16_FLOAT) accumulator

[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void TaaResolve(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= g_cbCamState.rt_width || DTid.y >= g_cbCamState.rt_height)
        return;

    const uint accum = g_cbCamState.iSrCamDummy__0; // samples already accumulated in history (n)
    float4 cur = fragment_rgba_out[DTid.xy];

    float4 resolved;
    if (accum == 0)
        resolved = cur; // first sample seeds the history
    else
        resolved = lerp(taa_history[DTid.xy], cur, 1.0f / (float)(accum + 1)); // running mean of n+1 samples

    taa_history[DTid.xy] = resolved;
    fragment_rgba_out[DTid.xy] = resolved;
}
