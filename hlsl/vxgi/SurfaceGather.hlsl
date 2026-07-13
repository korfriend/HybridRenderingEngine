#include "../CommonShader.hlsl"
#include "ConeTrace.hlsli"
#include "SurfaceVoxel.hlsli"

// -----------------------------------------------------------------------------
// VXGI v5 - Part C : SURFACE-voxel cone-traced indirect + cone AO.
// True voxel cone tracing, evaluated NOT in screen space (retired v1 Gather:
// DVR-first-hit only, unstable gradient normals, per-frame reconstruction
// flicker) and NOT at every voxel (interior voxels have no normal, cost blows
// up), but at the occupied/empty BOUNDARY voxels, in voxel space — view
// independent, so it converges once per content rebuild like the rest of the
// grid pipeline.
//
// Runs at CHECKPOINT bounces only (bounce 0 at rebuild, then {T/2, T-1} in the
// convergence loop, per _int_VxgiSurfaceCheckpoints): each checkpoint re-traces
// ALL 6 cones per surface voxel against the CURRENT radiance field and rewrites
// grid_surf WHOLESALE (non-surface voxels get 0) — a full REPLACE, so stale
// values are impossible by construction and no clear dispatch is needed while
// the pass runs at least once (first-build clear is the C++'s job, for the
// checkpoints==0 case). The early checkpoint sees a barely-diffused field; the
// later ones REFINE the surface term against the converged one — that is the
// whole reason checkpoints exist instead of trace-once-and-freeze (plan §4.2).
//
// Cone AO: the occlusion accumulated by these same cones is a free by-product
// (grid_surf.a). The v1..v3 screen-space cone AO was retired for its
// surface-vs-lattice phase striping; evaluated at VOXEL CENTERS the lattice
// phase problem is structurally gone — the same argument that moved v4's AO to
// density obscurance now brings the directional cone term back as a complement
// (open vs blocked far-field visibility that the isotropic density obscurance
// cannot see). Propagate screen-blends it with the density obscurance (§3.5).
//
// Classification + normal derivation live in SurfaceVoxel.hlsli — SHARED with
// the voxel debug views (Gather.hlsl modes 12/13), so the debug visualization
// always shows exactly what this pass computes.
// -----------------------------------------------------------------------------

Texture3D tex3D_volume : register(t0); // ORIGINAL intensity volume (~4x grid res): CT-gradient normal refinement
Texture3D grid_prev : register(t8); // radiance grid (MIP CHAIN): cone gather source (bounce 0: = DIRECT seed)
Texture3D grid_mat  : register(t9); // rgb = albedo, a = TRUE coverage (MIP CHAIN): surface test + normal + cone visibility
RWTexture3D<float4> grid_surf : register(u0); // OUT rgb = albedo * hemisphere indirect, a = cone occlusion

// Self-intersection margins (production values): origin pushed 2 voxels off the iso anchor, cones
// start another 2 voxels out along their own direction — together with the tangent-plane slab clip
// (VXGI_TraceCone_2Tex) this clears the voxel's own ~2-voxel coverage ramp for the 60-deg side cones
// too. Raising these to 5/4 was the banding-hunt diagnostic; the bands turned out to be the debug
// viewer's coverage-proportional transparency blending, so contact-AO reach wins the trade again.
#define VXGI_SURF_TRACE_OFFSET_VOX 2.0f
#define VXGI_SURF_CONE_START_VOX   2.0f

// DIAGNOSTIC isolation switches (0 in production) — each removes ONE phase-dependent input wholesale:
//  * FIXED_NORMAL: every surface voxel traces the SAME WS hemisphere. AO bands still present with
//    this on => the normal pipeline (gradient, gate, fallbacks) is fully exonerated and the bands
//    enter through the traced field / anchor geometry. Bands gone => normal path is the carrier.
//  * NO_ISO_ANCHOR: disable the iso-surface origin refinement (iso_d = 0). The anchor's ramp-slope
//    estimate uses the RAW gradient, so its own phase noise can wobble the origin height while the
//    slab still assumes a constant clearance — this switch isolates that channel.
#define VXGI_SURF_DIAG_FIXED_NORMAL 0
#define VXGI_SURF_DIAG_NO_ISO_ANCHOR 0

[numthreads(8, 8, 8)]
void VXGI_SurfaceGather(uint3 id : SV_DispatchThreadID)
{
	uint R = g_cbVxgi.grid_res;
	if (R == 0)
		return;
	if (id.x >= R || id.y >= R || id.z >= R)
		return;

	float4 mat = grid_mat.Load(int4(id, 0));
	[branch]
	if (mat.a <= 0.0f)
	{
		grid_surf[id] = (float4) 0; // wholesale write: stale values self-erase, no separate clear
		return;
	}

	// ---- surface classification, INLINE (plan §3.1 — no separate classify pass in v1: the test costs
	// only the 6 face-neighbour fetches, and it runs on checkpoint frames only (2-3 per rebuild), so a
	// reusable mask texture buys nearly nothing. Split into a SurfaceClassify pass + R8 mask if
	// profiling ever shows this inline cost — the criteria are material-only, so the mask would stamp
	// with the MAT stamp.)
	VXGI_SurfaceTest st = VXGI_ClassifySurface(grid_mat, int3(id), (int) R - 1);
	[branch]
	if (!st.is_surface)
	{
		grid_surf[id] = (float4) 0; // interior voxel: Part D's in-medium diffusion owns it
		return;
	}

	SurfaceNormal sn = VXGI_CoverageGradientNormal(grid_mat, tex3D_volume, int3(id), st);
	[branch]
	if (!sn.valid)
	{
		grid_surf[id] = (float4) 0;
		return;
	}
#if VXGI_SURF_DIAG_FIXED_NORMAL == 1
	// DIAGNOSTIC: identical hemisphere everywhere — removes the normal as a variable entirely.
	sn.normal_ws = float3(0, 0, 1);
	sn.double_sided = false;
#endif

	// ---- hemisphere cone gather (2-texture march, plan §3.5): visibility/occlusion from grid_mat.a
	// (true coverage), radiance from the CURRENT radiance field (bounce 0: the DIRECT seed; later
	// checkpoints: direct + everything diffused so far — the refinement this pass exists for).
	float3 uv = (float3(id) + 0.5f) / (float) R;
	float3 N_vox_axial = normalize(TransformVector(sn.normal_ws, g_cbVxgi.mat_ws2vox));

	// PHASE-CONTINUOUS trace anchor: a fixed "center + 2 voxels" push gives every band voxel a
	// DIFFERENT clearance above the true surface — the voxel center sits at a lattice-quantized,
	// sub-voxel-phase-varying depth inside the ~2-voxel coverage ramp, so the cones' near-field
	// occlusion oscillates with the surface phase and prints contour-following wave bands into
	// surf.a. That is trace GEOMETRY: no sampling filter (cubic taps, jittered voxelize, exact DDA)
	// can remove it — which is exactly how it survived all of them. Anchor the origin on the
	// cov=0.5 ISOSURFACE instead: its distance along +N is ~(cov_center - 0.5)/slope with the ramp
	// slope from the classification's own central difference (|g|/2 per voxel, floored against
	// blow-up). Every band voxel then starts its cones at the SAME height above the SAME local
	// surface — the lattice phase cancels out of the trace geometry entirely.
	float slope = max(0.5f * length(st.g_vox), 0.15f); // coverage drop per voxel along N
	float iso_d = sn.double_sided ? 0.0f               // thin plate: center IS the natural anchor
	            : clamp((mat.a - 0.5f) / slope, -1.5f, 1.5f); // voxels from center to the iso, along +N
#if VXGI_SURF_DIAG_NO_ISO_ANCHOR == 1
	iso_d = 0.0f; // DIAGNOSTIC: plain voxel-center origins — isolates the anchor's slope-noise channel
#endif

	// start_dist = 2 voxels: together with the origin push this clears the local ramp for the 60-deg
	// side cones too (their height gain is only 0.5/voxel — with a 1-voxel start their first samples
	// grazed the ramp: systematic false cone occlusion; see the note in VXGI_TraceCone_2Tex).
	// clearance above the local tangent plane (the iso anchor) = the origin push; the slab clip
	// inside the trace measures every sample's height against that plane (staircase guard).
	const float cone_start = VXGI_SURF_CONE_START_VOX / (float) R;
	const float cone_clearance = VXGI_SURF_TRACE_OFFSET_VOX / (float) R;
	float4 gi = VXGI_ConeTraceGI_2Tex(grid_mat, grid_prev, g_samplerLinear_clamp,
		uv + N_vox_axial * ((VXGI_SURF_TRACE_OFFSET_VOX + iso_d) / (float) R), sn.normal_ws,
		VXGI_CONE_APERTURE, g_cbVxgi.max_trace_dist, (float) R, VXGI_NUM_CONES, cone_start, cone_clearance);
	[branch]
	if (sn.double_sided)
	{
		// symmetric thin plate: trace the opposite hemisphere too and average — both sides really
		// receive light (2x cost on these voxels only).
		float4 gi_back = VXGI_ConeTraceGI_2Tex(grid_mat, grid_prev, g_samplerLinear_clamp,
			uv - N_vox_axial * (VXGI_SURF_TRACE_OFFSET_VOX / (float) R), -sn.normal_ws,
			VXGI_CONE_APERTURE, g_cbVxgi.max_trace_dist, (float) R, VXGI_NUM_CONES, cone_start, cone_clearance);
		gi = 0.5f * (gi + gi_back);
	}

	// Albedo modulation is REQUIRED (not a choice): every term the grid carries is "light this voxel
	// RE-EMITS", and InjectLight bakes albedo*light*T the same way. Without it (a) a red surface would
	// re-emit white indirect light, and (b) the surface<->surface feedback series (plan §4.4) loses its
	// albedo contraction margin. The GAINS are NOT baked here — Propagate multiplies them at composite
	// time, so a tuning change takes effect on the next bounce instead of waiting for the next checkpoint.
	grid_surf[id] = float4(mat.rgb * gi.rgb, gi.a);
}
