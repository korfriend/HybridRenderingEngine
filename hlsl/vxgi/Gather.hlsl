#include "../CommonShader.hlsl"
#include "ConeTrace.hlsli"

// -----------------------------------------------------------------------------
// VXGI v1 - Stage 3 : Screen-space cone-tracing gather (post-pass).
// For each pixel that hit the volume surface (per the DVR first-hit depth), we
// reconstruct the world-space surface point (mirroring DvrCS), estimate the
// surface normal from the volume gradient, then cone-trace the radiance grid to
// gather indirect diffuse GI + ambient occlusion, and modulate the already
// composited color in RENDER_OUT_RGBA_0.
// -----------------------------------------------------------------------------

// ALPHA SEMANTICS (v4.5): the radiance grid's alpha is the per-voxel OBSCURANCE field PREMULTIPLIED
// by coverage (obscurance * mat.a), NOT opacity. Displaying/consuming the field requires dividing by
// the SAME-lod MAT coverage (t3); anything occupancy-dependent (march blocking, cone AO) reads the
// MAT grid directly, whose alpha is the true coverage fraction from Voxelize.
Texture3D vxgi_grid : register(t0);           // rgb = radiance, a = obscurance*coverage (PREMULTIPLIED)
Texture2D<float> firsthit_depth : register(t1); // DVR first-hit depth (FLT_MAX == no hit)
Texture3D tex3D_volume : register(t2);        // volume, for the surface-normal gradient
Texture3D grid_mat : register(t3);            // rgb = albedo, a = COVERAGE (occupancy source)

RWTexture2D<unorm float4> rgba_out : register(u1); // RENDER_OUT_RGBA_0 (in/out)

[numthreads(8, 8, 1)]
void VXGI_Gather(uint3 DTid : SV_DispatchThreadID)
{
	// Robustness : do nothing if VXGI is disabled or the grid is unallocated.
	if (g_cbVxgi.grid_res == 0 || !VXGI_IS_ENABLED)
		return;

	uint2 xy = DTid.xy;
	if (xy.x >= g_cbCamState.rt_width || xy.y >= g_cbCamState.rt_height)
		return;

	// debug request lives in vxgi_flag's top byte: [24:27] mode, [28:31] mip.
	int dbg = (int) VXGI_DEBUG_MODE;
	float dbg_mip = (float) VXGI_DEBUG_MIP;

	// --- modes 8/9 : direct VOXEL visualization — ray-march the grid itself (point-sampled => blocky
	// voxels), at the selected mip, independent of the DVR first-hit (covers empty background too).
	// 8 = radiance rgb (verifies inject/propagate + the mip chain), 9 = obscurance field (grid alpha).
	// Blocking/normalization use the MAT grid's COVERAGE — the radiance grid's alpha is obscurance.
	if (dbg >= 8)
	{
		// camera ray in voxel [0,1] space
		float3 pos_ip_ss = float3((float2) xy, 0.0f);
		float3 pos_ip_ws = TransformPoint(pos_ip_ss, g_cbCamState.mat_ss2ws);
		float3 dir_ws = (g_cbCamState.cam_flag & 0x1)
			? normalize(pos_ip_ws - g_cbCamState.pos_cam_ws)
			: normalize(g_cbCamState.dir_view_ws);
		float3 ov = TransformPoint(pos_ip_ws, g_cbVxgi.mat_ws2vox);
		float3 dv = normalize(TransformVector(dir_ws, g_cbVxgi.mat_ws2vox));

		// slab intersection with the unit grid box
		float3 inv_d = 1.0f / (abs(dv) > 1e-6f ? dv : (dv >= 0.0f ? 1e-6f : -1e-6f));
		float3 t0v = (0.0f - ov) * inv_d;
		float3 t1v = (1.0f - ov) * inv_d;
		float3 tminv = min(t0v, t1v), tmaxv = max(t0v, t1v);
		float t_in = max(max(tminv.x, tminv.y), max(tminv.z, 0.0f));
		float t_out = min(tmaxv.x, min(tmaxv.y, tmaxv.z));
		if (t_out <= t_in)
			return; // ray misses the grid: keep the underlying render

		float4 acc = (float4) 0;
		const int VOX_STEPS = 384;
		float stepv = (t_out - t_in) / (float) VOX_STEPS;
		[loop]
		for (int vs = 0; vs < VOX_STEPS; vs++)
		{
			if (acc.a >= 0.99f)
				break;
			float3 p = ov + dv * (t_in + ((float) vs + 0.5f) * stepv);
			float4 s = vxgi_grid.SampleLevel(g_samplerPoint_clamp, p, dbg_mip); // rgb=radiance, a=obsc*cov
			float cov = grid_mat.SampleLevel(g_samplerPoint_clamp, p, dbg_mip).a; // true occupancy
			// Mip compensation (display only): higher mips average occupied voxels with empty space, which
			// dilutes rgb, coverage AND the premultiplied alpha alike — dividing by the same-lod COVERAGE
			// recovers the coverage-weighted average (EXACT for the premultiplied obscurance at any mip;
			// with the old un-premultiplied storage this same division inflated thin-shell voxels at low
			// mips). Display alpha is boosted by 2^mip so every LOD reads at comparable brightness, and
			// radiance gets a sqrt tone boost so dim diffused light deep in the medium stays visible.
			// Radiance display range = (1-GAIN) * radiance — the SAME normalization the DVR consumption
			// applies. Without it the DIRECT-seeded shell already clips at 1.0 and the entire propagate
			// growth (up to direct/(1-GAIN)) disappears into the saturate: seed ~0.7 gray, converged -> 1.
			float3 col = (dbg == 8) ? sqrt(saturate((1.0f - VXGI_SCATTER_GAIN) * s.rgb / max(cov, 1e-3f)))
			                        : saturate(s.aaa / max(cov, 1e-3f)); // 9: obscurance per occupied voxel
			float a = saturate(cov * 0.25f * exp2(dbg_mip));
			acc.rgb += (1.0f - acc.a) * col * a;
			acc.a += (1.0f - acc.a) * a;
		}
		float4 bg = rgba_out[xy];
		rgba_out[xy] = float4(acc.rgb + (1.0f - acc.a) * bg.rgb, 1.0f);
		return;
	}

	float d = firsthit_depth[xy];
	if (d <= 0.0f || d >= FLT_LARGE) // DvrCS writes FLT_MAX when there is no surface hit
		return;

	// --- reconstruct the world-space surface point (mirror DvrCS) ---
	float3 pos_ip_ss = float3((float2) xy, 0.0f);
	float3 pos_ip_ws = TransformPoint(pos_ip_ss, g_cbCamState.mat_ss2ws);
	float3 dir = (g_cbCamState.cam_flag & 0x1)
		? normalize(pos_ip_ws - g_cbCamState.pos_cam_ws)
		: normalize(g_cbCamState.dir_view_ws);
	float3 pos_ws = pos_ip_ws + dir * d;

	// --- surface normal from the volume gradient (central diff in tex space) ---
	// TS axes are aligned with the grid's voxel-space axes, so the TS gradient is
	// already a valid voxel-space direction.
	float3 pos_ts = TransformPoint(pos_ws, g_cbVobj.mat_ws2ts);
	float3 grad = GradientVolume(pos_ts, g_cbVobj.vec_grad_x, g_cbVobj.vec_grad_y, g_cbVobj.vec_grad_z, tex3D_volume);

	float3 dir_vox = TransformVector(dir, g_cbVxgi.mat_ws2vox);
	float glen = length(grad);
	float3 N_vox = (glen > 1e-8f) ? (-grad / glen) : -normalize(dir_vox); // outward = -density gradient
	if (dot(N_vox, dir_vox) > 0.0f)
		N_vox = -N_vox; // keep the normal facing the camera

	// --- cone trace the grid in [0,1] voxel space ---
	float grid_res = (float) g_cbVxgi.grid_res;
	float3 p0 = TransformPoint(pos_ws, g_cbVxgi.mat_ws2vox);
	float3 origin = p0 + N_vox * (2.0f / max(grid_res, 1.0f)); // start a couple voxels off the surface

	float4 gi = VXGI_ConeTraceGI(vxgi_grid, g_samplerLinear_clamp, origin, N_vox,
		VXGI_CONE_APERTURE, g_cbVxgi.max_trace_dist, grid_res, VXGI_NUM_CONES);

	float3 indirectRgb = gi.rgb; // radiance accumulation is still meaningful (legacy debug view)
	float aoTerm = gi.a;         // NOTE: integrates the radiance grid's alpha = PREMULTIPLIED obscurance, not opacity
	if (dbg == 4)
	{
		// real opacity-based cone AO: trace the MAT grid, whose alpha is the true coverage — the v1..v3
		// semantics this debug view was built to show (the radiance grid's alpha became obscurance in v4).
		aoTerm = VXGI_ConeTraceGI(grid_mat, g_samplerLinear_clamp, origin, N_vox,
			VXGI_CONE_APERTURE, g_cbVxgi.max_trace_dist, grid_res, VXGI_NUM_CONES).a;
	}

	// --- DEBUG visualization: dbg (decoded above) selects a stage to dump straight to screen (0 = normal
	// render). Cycle it from the sample UI to isolate which stage is wrong before trusting the modulation. ---
	if (dbg > 0)
	{
		float4 gs = vxgi_grid.SampleLevel(g_samplerLinear_clamp, p0, 0);
		float gcov = grid_mat.SampleLevel(g_samplerLinear_clamp, p0, 0).a; // un-premultiply (alpha = obsc*cov)
		float3 o = (float3) 0;
		if      (dbg == 1) o = saturate(gs.aaa / max(gcov, 1e-3f)); // 1: OBSCURANCE field at the surface voxel (flat ~0.3, crevices -> 1, ridges -> 0)
		else if (dbg == 2) o = (1.0f - VXGI_SCATTER_GAIN) * gs.rgb; // 2: grid RADIANCE at the surface voxel (consumption-scaled: converged -> ~1)
		else if (dbg == 3) o = N_vox * 0.5f + 0.5f;     // 3: surface NORMAL in voxel space (should look like a smooth normal map)
		else if (dbg == 4) o = saturate(aoTerm).xxx;    // 4: cone AO over the MAT grid's coverage (white = fully occluded) [legacy]
		else if (dbg == 5) o = indirectRgb;             // 5: cone-traced INDIRECT radiance [legacy]
		else if (dbg == 6) o = saturate(p0);            // 6: voxel COORD p0 (should be a smooth 0..1 gradient; flat/black = mapping bug)
		else               o = frac(pos_ts);            // 7: volume TEX coord of the surface point
		rgba_out[xy] = float4(o, 1.0f);
		return;
	}

	// --- modulate the composited color (keep alpha) ---
	// AO darkens; indirect adds a bounded bounce of the surface's own color. saturate() prevents the
	// white/black blowout of the unbounded (1 - ao + indirect) form. The two surface terms are gated
	// INDIVIDUALLY: ao_intensity and indirect_intensity (0 disables each); the volumetric in-scatter is
	// gated separately by gi_intensity inside the DVR march.
	float4 c = rgba_out[xy];
	float ao = saturate(aoTerm * VXGI_AO_INTENSITY);
	float3 indirect = saturate(indirectRgb * VXGI_INDIRECT_INTENSITY);
	c.rgb = saturate(c.rgb * (1.0f - ao) + c.rgb * indirect);
	rgba_out[xy] = c;
}
