#include "Sr_Common.hlsl"

// Temporal anti-aliasing resolve. The engine (RenderScene) drives a static, jittered accumulation: each
// frame the scene is rendered with a different sub-pixel jitter and this pass folds the freshly composited
// color (RENDER_OUT_RGBA_0) into a persistent per-camera history buffer using an unbiased running mean, then
// writes the average back to RENDER_OUT_RGBA_0 for presentation. iSrCamDummy__0 carries the number of samples
// already present in the history (n); the current frame becomes sample n+1.
//
// The mean is taken in LINEAR HDR: RENDER_OUT_RGBA_0 is FP16 and the tonemap runs after this pass, never
// before it. Averaging radiance is the whole point -- mean(tonemap(L)) != tonemap(mean(L)), so folding the
// curve in per-sample would bias the converged image (the curve is concave, so high-contrast edges would
// converge darker than the truth). Game TAA deliberately buys that bias, via a Karis tone weight, to kill
// fireflies; this is a static convergence accumulator with no reprojection and no history rejection, where a
// firefly already decays as 1/N -- so there is nothing to buy.
RWTexture2D<float4> fragment_rgba_out : register(u1); // RENDER_OUT_RGBA_0 : current frame in, resolved out
RWTexture2D<float4> taa_history : register(u2);             // TAA_HISTORY_RGBA (R16G16B16A16_FLOAT) accumulator

[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void TaaResolve(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= g_cbCamState.rt_width || DTid.y >= g_cbCamState.rt_height)
        return;

    const uint accum = g_cbCamState.iSrCamDummy__0; // samples already accumulated in history (n)

    // Mandatory now that the source is FP16 rather than UNORM: a NaN folded into the history is permanent
    // (lerp(NaN, x, w) == NaN forever). The UNORM store used to absorb this for free; it no longer does.
    float4 cur = SanitizeRadiance(fragment_rgba_out[DTid.xy], g_cbCamState.tm_radiance_ceiling);

    float4 resolved;
    if (accum == 0)
        resolved = cur; // first sample seeds the history
    else
        resolved = lerp(taa_history[DTid.xy], cur, 1.0f / (float)(accum + 1)); // running mean of n+1 samples

    taa_history[DTid.xy] = resolved;
    fragment_rgba_out[DTid.xy] = resolved;
}
