#include "../CommonShader.hlsl"
#include "ConeTrace.hlsli"

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
// -----------------------------------------------------------------------------

Texture3D grid_prev : register(t8); // radiance grid (MIP CHAIN): cone gather source (bounce 0: = DIRECT seed)
Texture3D grid_mat  : register(t9); // rgb = albedo, a = TRUE coverage (MIP CHAIN): surface test + normal + cone visibility
RWTexture3D<float4> grid_surf : register(u0); // OUT rgb = albedo * hemisphere indirect, a = cone occlusion

// Surface classification thresholds (coverage is Voxelize's true-coverage field: the boundary is a
// ~2-voxel anti-aliased ramp, not a 1-voxel shell — both tests below key off that ramp).
#define VXGI_SURF_EMPTY_EPS 0.001f // a neighbour below this counts as EMPTY (occupied/empty boundary)
#define VXGI_SURF_GRAD_TH   0.15f  // |central-difference gradient| above this marks the ramp band
#define VXGI_SURF_EPS2      1e-8f  // degenerate-direction threshold (gradient/face-sum fallback chain)

float CoverageAt(const in int3 p, const in int Rmax)
{
	return grid_mat.Load(int4(clamp(p, (int3) 0, (int3) Rmax), 0)).a;
}

// Result of the surface-normal estimation (plan §3.3 contract — the double-sided path cannot be
// expressed by a bare float3 return, so it is pinned in the struct to prevent implementation drift).
struct SurfaceNormal
{
	float3 normal_ws;    // primary normal, WORLD space (per-cone WS->vox conversion happens in the trace)
	bool   double_sided; // symmetric thin plate (both opposite faces empty): caller traces +/- hemispheres
	bool   valid;        // false: no direction could be derived (defensive; classification makes this unreachable)
};

// Normal from the SAME field that defines the surface — grid_mat.a (post-OTF true coverage):
//  * consistent: exactly perpendicular to the isosurface the classification found;
//  * noise-immune: the surface is defined by the OTF, not CT intensity — where the OTF puts an alpha
//    step on a soft CT ramp the raw CT gradient is noise-dominated, while the coverage field is already
//    OTF-stepped AND anti-aliased by Voxelize's 4x4x4 sub-sample averaging (a built-in binarize+smooth);
//  * lighting-invariant: material-only field, so light moves never bend the normals (grid_direct's rgb
//    gradient would be dominated by lighting, not geometry).
// Reuses the 6 face-neighbour fetches the surface test already made — zero extra fetches.
SurfaceNormal CoverageGradientNormal(const in float3 g_vox,
	const in float a_px, const in float a_mx, const in float a_py,
	const in float a_my, const in float a_pz, const in float a_mz)
{
	SurfaceNormal sn;
	sn.double_sided = false;
	sn.valid = true;

	[branch]
	if (dot(g_vox, g_vox) > VXGI_SURF_EPS2)
	{
		// COORDINATE CONVENTION (plan §3.3, team-review pinned): the normal, the hemisphere basis and
		// the 6 cone directions are all built in WS; each cone direction converts to voxel space for
		// the march. A voxel-space gradient normalized as-is mixes coordinate systems on anisotropic
		// grids (slice thickness 2-3x the pixel pitch skews the normal past the ~10-deg tolerance of
		// the 60-deg diffuse cones). Gradients map by the inverse-transpose: with orthogonal grid axes
		// A = R*S (S = diag(grid_axis_ws)), A^-T = A*S^-2 — divide componentwise by axis^2, then push
		// through mat_vox2ws (this includes the rotation the bare axis-divide convention would drop).
		float3 axis2 = g_cbVxgi.grid_axis_ws * g_cbVxgi.grid_axis_ws;
		sn.normal_ws = -normalize(TransformVector(g_vox / axis2, g_cbVxgi.mat_vox2ws)); // outward = -coverage gradient
		return sn;
	}

	// DEGENERATE GRADIENT — a symmetric thin plate (1-voxel slab) has a central difference of exactly
	// 0 on both sides (a(x+1) = a(x-1) = 0); a wider gradient cannot fix that (team-review corrected).
	// Fall back to the EMPTY-FACE weighted sum of the six face directions already fetched:
	// one exposed side -> that face's direction; asymmetric exposure -> the composite direction.
	// NOTE dir_f is a voxel-space axis — convert to WS before returning (returning a vox axis as a WS
	// normal re-introduces the coordinate mixing on anisotropic grids).
	float3 nf_vox = float3(1, 0, 0) * (1.0f - a_px) + float3(-1, 0, 0) * (1.0f - a_mx)
	              + float3(0, 1, 0) * (1.0f - a_py) + float3(0, -1, 0) * (1.0f - a_my)
	              + float3(0, 0, 1) * (1.0f - a_pz) + float3(0, 0, -1) * (1.0f - a_mz);
	[branch]
	if (dot(nf_vox, nf_vox) > VXGI_SURF_EPS2)
	{
		sn.normal_ws = normalize(TransformVector(nf_vox, g_cbVxgi.mat_vox2ws));
		return sn;
	}

	// STILL cancelled — symmetric two-sided plate: an opposite face PAIR is empty on both sides.
	// DOUBLE-SIDED: report the pair's + axis; the caller cone-traces BOTH hemispheres at 0.5 weight
	// (2x cost for these voxels only). Physically right — a thin plate really is lit from both sides,
	// and thin cortical shells / septa are exactly the geometry that needs GI most, so no skipping.
	float best = 1e9f;
	float3 axis_vox = (float3) 0;
	if (a_px < VXGI_SURF_EMPTY_EPS && a_mx < VXGI_SURF_EMPTY_EPS) { best = a_px + a_mx; axis_vox = float3(1, 0, 0); }
	if (a_py < VXGI_SURF_EMPTY_EPS && a_my < VXGI_SURF_EMPTY_EPS && a_py + a_my < best) { best = a_py + a_my; axis_vox = float3(0, 1, 0); }
	if (a_pz < VXGI_SURF_EMPTY_EPS && a_mz < VXGI_SURF_EMPTY_EPS && a_pz + a_mz < best) { best = a_pz + a_mz; axis_vox = float3(0, 0, 1); }
	[branch]
	if (best < 1e8f)
	{
		sn.normal_ws = normalize(TransformVector(axis_vox, g_cbVxgi.mat_vox2ws));
		sn.double_sided = true;
		return sn;
	}

	// Unreachable by construction (the classification passed via an empty face or a strong gradient),
	// kept as a defensive guard: report invalid so the caller writes 0.
	sn.normal_ws = float3(0, 0, 1);
	sn.valid = false;
	return sn;
}

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
	// only the 6 face-neighbour fetches below, and it runs on checkpoint frames only (2-3 per rebuild),
	// so a reusable mask texture buys nearly nothing. Split into a SurfaceClassify pass + R8 mask if
	// profiling ever shows this inline cost — the criteria are material-only, so the mask would stamp
	// with the MAT stamp.) Criterion: occupied AND (any empty face neighbour OR on the coverage ramp).
	const int Rmax = (int) R - 1;
	const int3 ip = int3(id);
	float a_px = CoverageAt(ip + int3(1, 0, 0), Rmax);
	float a_mx = CoverageAt(ip - int3(1, 0, 0), Rmax);
	float a_py = CoverageAt(ip + int3(0, 1, 0), Rmax);
	float a_my = CoverageAt(ip - int3(0, 1, 0), Rmax);
	float a_pz = CoverageAt(ip + int3(0, 0, 1), Rmax);
	float a_mz = CoverageAt(ip - int3(0, 0, 1), Rmax);

	float a_min = min(min(min(a_px, a_mx), min(a_py, a_my)), min(a_pz, a_mz));
	float3 g_vox = float3(a_px - a_mx, a_py - a_my, a_pz - a_mz); // central difference (2-voxel baseline)
	bool is_surface = (a_min < VXGI_SURF_EMPTY_EPS) || (dot(g_vox, g_vox) > VXGI_SURF_GRAD_TH * VXGI_SURF_GRAD_TH);
	[branch]
	if (!is_surface)
	{
		grid_surf[id] = (float4) 0; // interior voxel: Part D's in-medium diffusion owns it
		return;
	}

	SurfaceNormal sn = CoverageGradientNormal(g_vox, a_px, a_mx, a_py, a_my, a_pz, a_mz);
	[branch]
	if (!sn.valid)
	{
		grid_surf[id] = (float4) 0;
		return;
	}

	// ---- hemisphere cone gather (2-texture march, plan §3.5): visibility/occlusion from grid_mat.a
	// (true coverage), radiance from the CURRENT radiance field (bounce 0: the DIRECT seed; later
	// checkpoints: direct + everything diffused so far — the refinement this pass exists for).
	// Origin: pushed 2 voxels off the surface along the WS normal CONVERTED to voxel space (the same
	// direction the cones will march), to dodge self-occlusion by the source voxel's own ramp.
	float3 uv = (float3(id) + 0.5f) / (float) R;
	float3 N_vox_axial = normalize(TransformVector(sn.normal_ws, g_cbVxgi.mat_ws2vox));

	float4 gi = VXGI_ConeTraceGI_2Tex(grid_mat, grid_prev, g_samplerLinear_clamp,
		uv + N_vox_axial * (2.0f / (float) R), sn.normal_ws,
		VXGI_CONE_APERTURE, g_cbVxgi.max_trace_dist, (float) R, VXGI_NUM_CONES);
	[branch]
	if (sn.double_sided)
	{
		// symmetric thin plate: trace the opposite hemisphere too and average — both sides really
		// receive light (2x cost on these voxels only).
		float4 gi_back = VXGI_ConeTraceGI_2Tex(grid_mat, grid_prev, g_samplerLinear_clamp,
			uv - N_vox_axial * (2.0f / (float) R), -sn.normal_ws,
			VXGI_CONE_APERTURE, g_cbVxgi.max_trace_dist, (float) R, VXGI_NUM_CONES);
		gi = 0.5f * (gi + gi_back);
	}

	// Albedo modulation is REQUIRED (not a choice): every term the grid carries is "light this voxel
	// RE-EMITS", and InjectLight bakes albedo*light*T the same way. Without it (a) a red surface would
	// re-emit white indirect light, and (b) the surface<->surface feedback series (plan §4.4) loses its
	// albedo contraction margin. The GAINS are NOT baked here — Propagate multiplies them at composite
	// time, so a tuning change takes effect on the next bounce instead of waiting for the next checkpoint.
	grid_surf[id] = float4(mat.rgb * gi.rgb, gi.a);
}
