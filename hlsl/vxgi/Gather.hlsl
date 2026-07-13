#include "../CommonShader.hlsl"
#include "SurfaceVoxel.hlsli" // shared Part C classification/normal — debug modes 5/6 re-run the real logic

// -----------------------------------------------------------------------------
// VXGI debug view (v5). DEBUG-ONLY dispatch — the normal render path never runs
// this shader (C++ gates on VXGI_DEBUG != 0) and pays nothing for it.
//
// Every mode is a VOXEL RAY-MARCH visualization (point-sampled => blocky voxels,
// independent of the DVR first-hit, covers empty background too):
//   1 : radiance rgb, LOD-selectable   — verifies inject/propagate + the mip chain
//   2 : obscurance,   LOD-selectable   — radiance alpha / same-lod MAT coverage
//   3 : Part C surface indirect (grid_surf.rgb, composite-scaled)   [single mip]
//   4 : Part C surface cone occlusion (grid_surf.a, AO-gain scaled) [single mip]
//   5 : surface classification / normal-derivation path (re-runs the REAL
//       SurfaceGather logic via SurfaceVoxel.hlsli)                 [single mip]
//   6 : WS surface normal (verifies the coverage-gradient normal)   [single mip]
//   7 : RAW MAT coverage (grid_mat.a as-is), LOD-selectable — the SOURCE field
//       everything else derives from; a pattern visible here is baked in the
//       data, not introduced by any downstream pass
// The mip selector (vxgi_flag [28:31]) applies to modes 1/2/7 only — the SURF
// grid is single-mip by design.
//
// HISTORY: the v1..v4 screen-space half of this file (DVR first-hit probes,
// modes 1..7, and the legacy hemisphere cone AO/indirect, modes 4/5) is REMOVED
// (plan §3.6). Sampling the grid at arbitrary reconstructed surface points is
// inherently surface-vs-lattice phase-sensitive (the stripe artifacts that
// retired v1..v3), its normal came from a depth-reconstruction roundtrip, and
// the production GI terms are all per-sample inside the DVR march since v4 /
// lattice-aligned in SurfaceGather since v5. With those modes gone, the
// single-texture cone helpers (VXGI_TraceCone / VXGI_ConeTraceGI) lost their
// last caller and were deleted from ConeTrace.hlsli as well.
// -----------------------------------------------------------------------------

// ALPHA SEMANTICS (v4.5): the radiance grid's alpha is the per-voxel OBSCURANCE field PREMULTIPLIED
// by coverage (obscurance * mat.a), NOT opacity. Displaying the field requires dividing by the
// SAME-lod MAT coverage (t3), whose alpha is the true coverage fraction from Voxelize.
Texture3D vxgi_grid : register(t0); // rgb = radiance, a = obscurance*coverage (PREMULTIPLIED)
Texture3D tex3D_volume : register(t2); // ORIGINAL intensity volume — CT-gradient normal refinement (modes 5/6)
Texture3D grid_mat  : register(t3); // rgb = albedo, a = COVERAGE (occupancy source)
Texture3D grid_surf : register(t4); // Part C SURF grid (rgb = albedo*indirect, a = cone occ), single mip

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
	if (dbg < 1 || dbg > 7)
		return; // defensive: C++ already gates the dispatch on mode != 0

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

	// --- Amanatides & Woo DDA: visit every voxel along the ray EXACTLY ONCE, opacity from the true
	// segment length the ray spends inside it. The previous fixed-384-step point-sampled march
	// re-sampled each voxel a PHASE-DEPENDENT number of times (~0.5-voxel steps beating against the
	// voxel lattice): the per-pixel accumulation phase varies continuously across the screen and
	// printed smooth, contour-following WAVE fringes over EVERY view — the classic wood-grain
	// artifact. That moire was never in the data (it crossed voxel-block boundaries smoothly, and
	// survived every bake-side fix); a DDA has no sampling phase at all, so it cannot fringe.
	// Modes 1/2/7 honor the mip selector by walking the MIP lattice itself (opacity is per mip-TEXEL
	// crossed, so brightness self-normalizes across LODs — the old exp2 boost is obsolete); 3..6
	// read single-mip SURF/classification data, always on the full-res lattice.
	// Mode 7 = RAW MAT coverage (grid_mat.a as-is): the SOURCE field every other quantity derives
	// from (obscurance taps, surface normal gradient, cone visibility). If a pattern shows here it
	// is baked in the data, not introduced by any downstream pass — the first view to check.
	int mip = (dbg <= 2 || dbg == 7) ? (int) dbg_mip : 0;
	int Rm = max((int) g_cbVxgi.grid_res >> mip, 1);

	float t_cur = t_in + 1e-5f;
	float3 pev = (ov + dv * t_cur) * (float) Rm; // entry position, mip-texel units
	int3 vid = clamp(int3(floor(pev)), (int3) 0, (int3) (Rm - 1));
	float3 dvm = dv * (float) Rm; // texels advanced per unit t
	float3 inv_dvm = 1.0f / (abs(dvm) > 1e-6f ? dvm : (dvm >= 0.0f ? 1e-6f : -1e-6f));
	int3 istep = (int3) sign(inv_dvm);
	float3 nextb = float3(vid) + max(sign(inv_dvm), 0.0f); // next boundary per axis (vid or vid+1)
	float3 tMax = t_cur + (nextb - pev) * inv_dvm;         // t at the first boundary crossing per axis
	float3 tDelta = abs(inv_dvm);                          // t per one-texel step per axis

	float4 acc = (float4) 0;
	[loop]
	for (int i = 0; i < 1024; i++) // worst case 3*R (grid diagonal); breaks dominate
	{
		if (acc.a >= 0.99f || t_cur >= t_out)
			break;
		float t_exit = min(min(tMax.x, min(tMax.y, tMax.z)), t_out);
		float seg = max(t_exit - t_cur, 0.0f) * (float) Rm; // path length inside this voxel, texel units

		float3 col = (float3) 0;
		float dens = 0.0f; // display extinction per texel: a = 1 - exp(-dens * seg)
		if (dbg <= 2 || dbg == 7)
		{
			// --- 1/2/7 : radiance / obscurance / RAW coverage at the selected mip (Load == point sample).
			float4 s = vxgi_grid.Load(int4(vid, mip)); // rgb=radiance, a=obsc*cov
			float cov = grid_mat.Load(int4(vid, mip)).a; // true occupancy
			// Mip compensation (display only): higher mips average occupied voxels with empty space, which
			// dilutes rgb, coverage AND the premultiplied alpha alike — dividing by the same-lod COVERAGE
			// recovers the coverage-weighted average (EXACT for the premultiplied obscurance at any mip).
			// Radiance gets a sqrt tone boost so dim diffused light deep in the medium stays visible, and
			// its display range = (1-GAIN) * radiance — the SAME normalization the DVR consumption
			// applies. Without it the DIRECT-seeded shell already clips at 1.0 and the entire propagate
			// growth (up to direct/(1-GAIN)) disappears into the saturate: seed ~0.7 gray, converged -> 1.
			col = (dbg == 1) ? sqrt(saturate((1.0f - VXGI_SCATTER_GAIN) * s.rgb / max(cov, 1e-3f)))
			    : (dbg == 2) ? saturate(s.aaa / max(cov, 1e-3f)) // 2: obscurance per occupied voxel
			                 : saturate(cov).xxx;                // 7: RAW coverage — no un-premultiply, no remap
			// Near-OPAQUE display: translucent accumulation let deeper/backside voxels blend through
			// and fabricate contour patterns that track the coverage ripple (the same viewer artifact
			// that faked the surface-view "bands"). cov*4 saturates within 1-2 meaningfully-covered
			// voxels — the display lands on the front surface — while staying cov-proportional so
			// low-alpha OTF content (thin soft tissue) does not paint solid blocks out of near-air.
			dens = cov * 4.0f;
		}
		else
		{
			// --- 3..6 : Part C SURF-grid states (mip forced 0 above, so vid is the full-res voxel id).
			float mata = grid_mat.Load(int4(vid, 0)).a;
			if (dbg <= 4)
			{
				float4 sg = grid_surf.Load(int4(vid, 0));
				// SurfaceGather writes non-surface voxels as exact 0 — that IS the surface mask.
				bool has_surf = (sg.a > 0.0f) || any(sg.rgb > 0.0f);
				// 3: the surface indirect at the scale Propagate actually composites it
				//    (SURFACE_GI_GAIN * (1-SCATTER_GAIN)), sqrt tone boost like mode 1;
				// 4: cone occlusion, at the screen-blend scale (AO gain), white = blocked.
				col = (dbg == 3) ? sqrt(saturate(VXGI_SURFACE_GI_GAIN * (1.0f - VXGI_SCATTER_GAIN) * sg.rgb))
				                 : saturate(VXGI_SURFACE_CONE_AO_GAIN * sg.a).xxx;
				// OPAQUE shell (not mata-proportional!): coverage-weighted alpha let the low-coverage
				// ramp-tail voxels BLEND THE BACKSIDE SHELL THROUGH (opposite normal = complementary
				// color / different AO) — fabricating contour "wave bands" that track the coverage
				// ripple even when the per-voxel VALUES are perfectly smooth. That see-through mixing
				// is why these views were insensitive to every normal/AO computation fix. The first
				// surface voxel hit must win.
				dens = has_surf ? 4.0f : 0.0f;
			}
			else if (mata > 0.0f)
			{
				// 5/6: RE-RUN the SurfaceGather classification + normal derivation per voxel
				// (SurfaceVoxel.hlsli — the very code the real pass executes, so this view cannot
				// drift from it). Debug-only cost, and DDA visits each voxel once (not per step).
				VXGI_SurfaceTest st = VXGI_ClassifySurface(grid_mat, vid, Rm - 1);
				if (!st.is_surface)
				{
					// interior voxel: dim blue context in the state view, hidden in the normal view
					col = float3(0.08f, 0.12f, 0.45f);
					dens = (dbg == 5) ? mata * 0.1f : 0.0f;
				}
				else
				{
					SurfaceNormal sn = VXGI_CoverageGradientNormal(grid_mat, tex3D_volume, vid, st);
					if (dbg == 5)
					{
						// 5: normal-derivation path — CYAN = CT-gradient refined (expected on bone/skin),
						// green = coverage gradient (soft/OTF-only boundaries), yellow = empty-face
						// fallback, red = double-sided thin plate (2x cost), MAGENTA = invalid
						// (unreachable by construction -> a defect if ever seen).
						col = (sn.path == VXGI_SURF_PATH_CT)       ? float3(0.1f, 0.8f, 0.9f)
						    : (sn.path == VXGI_SURF_PATH_GRADIENT) ? float3(0.1f, 0.85f, 0.15f)
						    : (sn.path == VXGI_SURF_PATH_FACE)     ? float3(0.95f, 0.85f, 0.1f)
						    : (sn.path == VXGI_SURF_PATH_DOUBLE)   ? float3(0.95f, 0.15f, 0.1f)
						                                           : float3(1.0f, 0.0f, 1.0f);
					}
					else
					{
						col = sn.normal_ws * 0.5f + 0.5f; // 6: WS surface normal
					}
					dens = 4.0f; // OPAQUE shell — see the mode 3/4 note: front surface voxel must win
				}
			}
		}

		// Segment floor (all modes): a ray clipping a voxel CORNER gets a near-zero true segment ->
		// near-zero alpha -> speckled pinholes that are NOT in the data. The floor keeps every
		// crossed voxel legible; still phase-free (one evaluation per voxel).
		float seg_eff = max(seg, 0.75f);
		float a = 1.0f - exp(-dens * seg_eff);
		acc.rgb += (1.0f - acc.a) * col * a;
		acc.a += (1.0f - acc.a) * a;

		// advance to the neighbour across the nearest boundary
		t_cur = t_exit;
		if (tMax.x <= tMax.y && tMax.x <= tMax.z) { vid.x += istep.x; tMax.x += tDelta.x; }
		else if (tMax.y <= tMax.z)                { vid.y += istep.y; tMax.y += tDelta.y; }
		else                                      { vid.z += istep.z; tMax.z += tDelta.z; }
		if (any(vid < (int3) 0) || any(vid >= (int3) Rm))
			break;
	}
	float4 bg = rgba_out[xy];
	rgba_out[xy] = float4(acc.rgb + (1.0f - acc.a) * bg.rgb, 1.0f);
}
