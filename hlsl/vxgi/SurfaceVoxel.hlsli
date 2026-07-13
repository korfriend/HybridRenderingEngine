#ifndef VXGI_SURFACEVOXEL_HLSLI
#define VXGI_SURFACEVOXEL_HLSLI

// -----------------------------------------------------------------------------
// VXGI v5 Part C — SHARED surface-voxel classification + normal derivation.
// Used by BOTH the real pass (SurfaceGather.hlsl) and the voxel debug views
// (Gather.hlsl modes 12/13), so the debug visualization can never drift from
// what the actual pass computes. Requires CommonShader.hlsl #included first
// (g_cbVxgi, TransformVector).
// -----------------------------------------------------------------------------

// Surface classification thresholds (coverage is Voxelize's true-coverage field: the boundary is a
// ~2-voxel anti-aliased ramp, not a 1-voxel shell — both tests below key off that ramp).
#define VXGI_SURF_EMPTY_EPS 0.001f // a neighbour below this counts as EMPTY (occupied/empty boundary)
#define VXGI_SURF_GRAD_TH   0.15f  // |central-difference gradient| above this marks the ramp band
#define VXGI_SURF_EPS2      1e-8f  // degenerate-direction threshold (gradient/face-sum fallback chain)

// Normal-gradient reconstruction: 1 = C2 cubic B-spline taps (VXGI_SampleDensityCubic), 0 = raw
// texel central difference (A/B comparison only). WHY CUBIC: the coverage field carries a low-frequency
// interference pattern (CT slice periodicity beating against the grid resample — the same band energy
// the AO path fought; see the VXGI AO history in CommonShader.hlsl), and raw/trilinear taps are C0 —
// their slope JUMPS at every texel boundary, so a central difference of them prints the pattern
// straight into the normal DIRECTION (wave banding across flat anatomy in the debug normal view,
// wobbling hemisphere orientations in SurfaceGather). The B-spline kernel is C2: same fix, same
// justification as VXGI_AO_TAP_CUBIC. Cost: 6 taps x 8 fetches = 48 fetches per SURFACE voxel,
// checkpoint frames only (debug modes 5/6 pay it per march sample — debug-only).
#define VXGI_SURF_GRAD_CUBIC 1

// Gradient tap SCALE — the decisive knob against the CT ring ripple (Gibbs bands parallel to strong
// edges, period ~2-4 voxels, verified in the raw-coverage debug view). A +/-1-voxel central difference
// fits INSIDE one ring period, so the derivative direction is captured by the ring, not the surface —
// wave bands of rotated/inverted normals along the ring isolines that no reconstruction filter or
// occupancy gate can fully repair (the +/-1 face sum inherits the same ripple). Widen the support past
// the ring wavelength instead: taps at MIP 1 (cubic support there = ~8 full-res voxels) with a
// +/-2-voxel baseline average the ring out; the surface's low-frequency orientation — all a 60-deg
// diffuse cone needs — survives. Trade-off: sub-2-voxel normal detail is smoothed; thin plates lose
// their gradient entirely and drop to the face/double-sided fallback (by design).
// NOTE the WS metric correction (g/axis^2, the inverse-transpose transform) legitimately AMPLIFIES the
// short-world-axis gradient component by 1/L — correct math, but it makes slice-axis ripple worse on
// anisotropic volumes, which is why the ripple must be removed HERE, at the tap level.
#define VXGI_SURF_GRAD_MIP  0.0f // gradient tap mip: cubic support at mip 2 = ~16 full-res voxels
#define VXGI_SURF_GRAD_STEP 1.0f // central-difference half-baseline, in full-res voxels

// CT-GRADIENT normal refinement (plan §3.3 FUTURE OPTION, promoted): the coverage field of a tilted
// flat surface is a 1-voxel STAIRCASE at 128^3 — its gradient wobbles by tens of degrees in bands
// along the staircase phase isolines no matter how wide/smooth the taps (verified: the AO bands track
// the normal bands, and every read-side fix left them unchanged). The ORIGINAL CT volume is ~4x the
// grid resolution and its intensity edge at bone/skin is sharp and staircase-free — its gradient at
// the surface voxel position gives the smooth normal the coverage field cannot. HYBRID: the coverage
// gradient (occupancy-arbitrated) stays as the coarse orientation prior; the CT gradient REPLACES the
// direction only where it is strong (a real intensity edge) and agrees with the prior (< ~72 deg) —
// soft/OTF-defined boundaries with weak noisy CT gradients keep the coverage normal (the plan's
// documented risk case).
// RETIRED (keep 0): measured WORSE than the coverage gradient — the Gibbs rings live in the CT DATA
// itself, so the 4x-resolution CT gradient resolves the ring oscillation (with its sign flips) MORE
// sharply, not less; the coverage field is at least sub-sample-averaged. Kept for reference only.
#define VXGI_SURF_CT_NORMAL    0
#define VXGI_SURF_CT_GRAD_MIN2 1e-4f // |CT gradient|^2 gate: ~0.01 normalized-intensity over the baseline

// Which branch of the normal-derivation fallback chain fired (also the debug "surface state" color code).
#define VXGI_SURF_PATH_GRADIENT 0u // coverage-gradient normal (the healthy main path)
#define VXGI_SURF_PATH_FACE     1u // empty-face weighted-sum fallback (gradient cancelled)
#define VXGI_SURF_PATH_DOUBLE   2u // symmetric thin plate: double-sided +/- hemispheres
#define VXGI_SURF_PATH_INVALID  3u // no direction derivable — unreachable by construction (defect if seen)
#define VXGI_SURF_PATH_CT       4u // CT-gradient-refined normal (strong intensity edge, prior-consistent)

// Surface test result. Carries the 6 face-neighbour coverages and the central-difference gradient so
// the normal derivation reuses them with ZERO extra fetches.
struct VXGI_SurfaceTest
{
	bool   is_surface;
	float3 g_vox; // central-difference coverage gradient (2-voxel baseline), voxel space
	float  a_px, a_mx, a_py, a_my, a_pz, a_mz;
};

// Occupied/empty boundary test around one voxel (assumes the CALLER already checked mat.a > 0).
// Criterion: any empty face neighbour OR sitting on the coverage ramp (strong gradient).
VXGI_SurfaceTest VXGI_ClassifySurface(Texture3D grid_mat, const in int3 id, const in int Rmax)
{
	VXGI_SurfaceTest st;
	st.a_px = grid_mat.Load(int4(clamp(id + int3(1, 0, 0), (int3) 0, (int3) Rmax), 0)).a;
	st.a_mx = grid_mat.Load(int4(clamp(id - int3(1, 0, 0), (int3) 0, (int3) Rmax), 0)).a;
	st.a_py = grid_mat.Load(int4(clamp(id + int3(0, 1, 0), (int3) 0, (int3) Rmax), 0)).a;
	st.a_my = grid_mat.Load(int4(clamp(id - int3(0, 1, 0), (int3) 0, (int3) Rmax), 0)).a;
	st.a_pz = grid_mat.Load(int4(clamp(id + int3(0, 0, 1), (int3) 0, (int3) Rmax), 0)).a;
	st.a_mz = grid_mat.Load(int4(clamp(id - int3(0, 0, 1), (int3) 0, (int3) Rmax), 0)).a;

	float a_min = min(min(min(st.a_px, st.a_mx), min(st.a_py, st.a_my)), min(st.a_pz, st.a_mz));
	st.g_vox = float3(st.a_px - st.a_mx, st.a_py - st.a_my, st.a_pz - st.a_mz);
	st.is_surface = (a_min < VXGI_SURF_EMPTY_EPS)
		|| (dot(st.g_vox, st.g_vox) > VXGI_SURF_GRAD_TH * VXGI_SURF_GRAD_TH);
	return st;
}

// Result of the surface-normal estimation (plan §3.3 contract — the double-sided path cannot be
// expressed by a bare float3 return, so it is pinned in the struct to prevent implementation drift).
struct SurfaceNormal
{
	float3 normal_ws;    // primary normal, WORLD space (per-cone WS->vox conversion happens in the trace)
	bool   double_sided; // symmetric thin plate (both opposite faces empty): caller traces +/- hemispheres
	bool   valid;        // false: no direction could be derived (defensive; classification makes this unreachable)
	uint   path;         // VXGI_SURF_PATH_* — which fallback branch fired (debug state view / diagnostics)
};

// Normal from the SAME field that defines the surface — grid_mat.a (post-OTF true coverage):
//  * consistent: exactly perpendicular to the isosurface the classification found;
//  * noise-immune: the surface is defined by the OTF, not CT intensity — where the OTF puts an alpha
//    step on a soft CT ramp the raw CT gradient is noise-dominated, while the coverage field is already
//    OTF-stepped AND anti-aliased by Voxelize's 4x4x4 sub-sample averaging (a built-in binarize+smooth);
//  * lighting-invariant: material-only field, so light moves never bend the normals (grid_direct's rgb
//    gradient would be dominated by lighting, not geometry).
// The gradient taps are cubic-B-spline reconstructed (VXGI_SURF_GRAD_CUBIC — wave-banding fix; see the
// knob comment above); the FALLBACK chain still reuses the classification's raw 6 face-neighbour values
// (the empty-face tests are occupancy semantics — smoothing them would blur the very emptiness they test).
SurfaceNormal VXGI_CoverageGradientNormal(Texture3D grid_mat, Texture3D tex_vol, const in int3 id, const in VXGI_SurfaceTest st)
{
	// Fully initialized at declaration, as a DEFENSIVE default only — every return path below already
	// assigns all four fields, and this is NOT what silenced the X4000 the struct appeared to be causing
	// (that was g_vox's component-wise init; see the note there). Dead stores — the optimizer drops them.
	// The defaults are the INVALID state deliberately, not a plausible-looking normal: if a future edit
	// ever adds a return path that forgets `path`, the debug state view paints it magenta instead of
	// silently reporting a healthy GRADIENT (a zero-default would have made exactly that mistake invisible).
	SurfaceNormal sn;
	sn.normal_ws = float3(0, 0, 1);
	sn.double_sided = false;
	sn.valid = true;
	sn.path = VXGI_SURF_PATH_INVALID;

	float Rf = (float) g_cbVxgi.grid_res;
	float3 uvc = (float3(id) + 0.5f) / Rf;

#if VXGI_SURF_GRAD_CUBIC == 1
	// C2-reconstructed WIDE central difference — mip and baseline per the VXGI_SURF_GRAD_MIP/STEP
	// knobs above (ring-ripple suppression: the support must exceed the ring wavelength).
	float Rg = max(Rf / exp2(VXGI_SURF_GRAD_MIP), 1.0f); // texel count at the gradient mip
	float e = VXGI_SURF_GRAD_STEP / Rf;                  // half-baseline in uv units
	// Built with a single float3 constructor, NOT declare-then-write-.x/.y/.z: fxc does not treat the three
	// component writes as proving the vector whole, so the declared-empty form left g_vox "potentially
	// uninitialized" (X4000). The taint then propagated through out_vox into sn.normal_ws, and fxc reported
	// it at the two enclosing branch merge points (the closing braces of the gradient / grad_sane blocks) —
	// which is why the warning did NOT come from the struct it appeared to point at. Same math, no warning.
	float3 g_vox = float3(
		VXGI_SampleDensityCubic(grid_mat, uvc + float3(e, 0, 0), VXGI_SURF_GRAD_MIP, Rg)
	  - VXGI_SampleDensityCubic(grid_mat, uvc - float3(e, 0, 0), VXGI_SURF_GRAD_MIP, Rg),
		VXGI_SampleDensityCubic(grid_mat, uvc + float3(0, e, 0), VXGI_SURF_GRAD_MIP, Rg)
	  - VXGI_SampleDensityCubic(grid_mat, uvc - float3(0, e, 0), VXGI_SURF_GRAD_MIP, Rg),
		VXGI_SampleDensityCubic(grid_mat, uvc + float3(0, 0, e), VXGI_SURF_GRAD_MIP, Rg)
	  - VXGI_SampleDensityCubic(grid_mat, uvc - float3(0, 0, e), VXGI_SURF_GRAD_MIP, Rg));
#else
	float3 g_vox = st.g_vox; // raw texel central difference (band-prone — A/B only)
#endif

	// EMPTY-FACE occupancy prior (always computed — pure ALU on the already-fetched neighbours):
	// the weighted sum of the six face directions toward emptiness. Being an OCCUPANCY sum, not a
	// derivative, it can never invert — which makes it the arbiter for the gradient below, and the
	// fallback when the gradient is degenerate.
	float3 nf_vox = float3(1, 0, 0) * (1.0f - st.a_px) + float3(-1, 0, 0) * (1.0f - st.a_mx)
	              + float3(0, 1, 0) * (1.0f - st.a_py) + float3(0, -1, 0) * (1.0f - st.a_my)
	              + float3(0, 0, 1) * (1.0f - st.a_pz) + float3(0, 0, -1) * (1.0f - st.a_mz);
	// STRONG-evidence threshold (|nf| >= 0.5: at least half a face's worth of exposed emptiness).
	// A near-epsilon threshold made mid-ramp voxels' nf — a ~0.01-length NOISE vector — a "valid"
	// arbiter: normalize() of noise is a random direction, the agreement test failed randomly, and
	// the gate demoted smooth-gradient voxels to quasi-random face normals ALONG THE RAMP PHASE
	// ISOLINES — manufacturing the very normal bands it was meant to prevent, immune to any amount
	// of gradient smoothing (the flip happened after it).
	bool face_valid = dot(nf_vox, nf_vox) > 0.25f;

	// SINGLE EXIT (one `return sn` at the end), gated by this flag rather than an early return per path.
	// fxc restructures a multi-return function into a single exit itself, and on the fall-through edges of
	// these nested [branch] blocks its synthesized return temp reads as un-set — that, not the struct or the
	// gradient vector, was the real source of the X4000 pair reported at the two closing braces. Writing the
	// single exit explicitly removes the synthesized temp and the warning with it. Codegen is unchanged
	// (fxc was already producing this shape); the flag folds away.
	bool resolved = false;

	[branch]
	if (dot(g_vox, g_vox) > VXGI_SURF_EPS2)
	{
		// SANITY GATE against the occupancy prior: CT ring artifacts (Gibbs over/undershoot bands
		// running PARALLEL to strong edges) put false extrema inside the coverage ramp, and on those
		// ring isolines the local derivative rotates arbitrarily — up to a full INVERSION (verified:
		// the AO wave bands coincide with inverted-color loci in the normal debug view; the hemisphere
		// then fires INTO the material and occ saturates). A derivative cannot be trusted to orient
		// itself there — the occupancy prior arbitrates: if the gradient's outward direction opposes
		// or near-orthogonally ignores the face evidence (<~75 deg agreement), take the face path.
		bool grad_sane = !face_valid
			|| dot(normalize(-g_vox), normalize(nf_vox)) > 0.25f;
		[branch]
		if (grad_sane)
		{
			float3 out_vox = -g_vox; // outward = -coverage gradient (voxel/ts frame)
			sn.path = VXGI_SURF_PATH_GRADIENT;
#if VXGI_SURF_CT_NORMAL == 1
			// CT refinement (see the knob comment): the grid's staircase-banded gradient only sets the
			// coarse orientation; where the ORIGINAL volume has a strong, prior-consistent intensity
			// edge, its 4x-resolution gradient replaces the direction — smooth across flat bone/skin.
			// Frames align: grid vox = volume ts * uniform fit, so ts-space directions ARE voxel-space
			// directions (uniform scalars die in normalize); the WS metric transform below applies
			// identically. Sampled at a 2-texel baseline (volume texels — still ~4x finer than the grid).
			float3 ts = (uvc - g_cbVxgi.vox_fit_offset) / g_cbVxgi.vox_fit_scale;
			[branch]
			if (all(ts >= 0.0f) && all(ts <= 1.0f))
			{
				float3 g_ct = GradientVolume(ts, g_cbVobj.vec_grad_x * 2.0f,
					g_cbVobj.vec_grad_y * 2.0f, g_cbVobj.vec_grad_z * 2.0f, tex_vol);
				if (dot(g_ct, g_ct) > VXGI_SURF_CT_GRAD_MIN2
					&& dot(normalize(-g_ct), normalize(out_vox)) > 0.3f)
				{
					out_vox = -g_ct; // outward = -intensity gradient (denser inside)
					sn.path = VXGI_SURF_PATH_CT;
				}
			}
#endif
			// COORDINATE CONVENTION (plan §3.3, team-review pinned): the normal, the hemisphere basis and
			// the 6 cone directions are all built in WS; each cone direction converts to voxel space for
			// the march. A voxel-space gradient normalized as-is mixes coordinate systems on anisotropic
			// grids (slice thickness 2-3x the pixel pitch skews the normal past the ~10-deg tolerance of
			// the 60-deg diffuse cones). Gradients map by the inverse-transpose: with orthogonal grid axes
			// A = R*S (S = diag(grid_axis_ws)), A^-T = A*S^-2 — divide componentwise by axis^2, then push
			// through mat_vox2ws (this includes the rotation the bare axis-divide convention would drop).
			float3 axis2 = g_cbVxgi.grid_axis_ws * g_cbVxgi.grid_axis_ws;
			sn.normal_ws = normalize(TransformVector(out_vox / axis2, g_cbVxgi.mat_vox2ws));
			resolved = true;
		}
		// ring-corrupted gradient: fall through to the face path below (face_valid is true here)
	}

	// DEGENERATE (thin plate: central difference exactly 0 on both sides — a wider gradient cannot
	// fix that) or RING-CORRUPTED gradient — use the empty-face weighted sum:
	// one exposed side -> that face's direction; asymmetric exposure -> the composite direction.
	// NOTE dir_f is a voxel-space direction — convert to WS before returning (returning a vox axis as
	// a WS normal re-introduces the coordinate mixing on anisotropic grids).
	[branch]
	if (!resolved && face_valid)
	{
		sn.normal_ws = normalize(TransformVector(nf_vox, g_cbVxgi.mat_vox2ws));
		sn.path = VXGI_SURF_PATH_FACE;
		resolved = true;
	}

	// STILL cancelled — symmetric two-sided plate: an opposite face PAIR is empty on both sides.
	// DOUBLE-SIDED: report the pair's + axis; the caller cone-traces BOTH hemispheres at 0.5 weight
	// (2x cost for these voxels only). Physically right — a thin plate really is lit from both sides,
	// and thin cortical shells / septa are exactly the geometry that needs GI most, so no skipping.
	float best = 1e9f;
	float3 axis_vox = (float3) 0;
	if (st.a_px < VXGI_SURF_EMPTY_EPS && st.a_mx < VXGI_SURF_EMPTY_EPS) { best = st.a_px + st.a_mx; axis_vox = float3(1, 0, 0); }
	if (st.a_py < VXGI_SURF_EMPTY_EPS && st.a_my < VXGI_SURF_EMPTY_EPS && st.a_py + st.a_my < best) { best = st.a_py + st.a_my; axis_vox = float3(0, 1, 0); }
	if (st.a_pz < VXGI_SURF_EMPTY_EPS && st.a_mz < VXGI_SURF_EMPTY_EPS && st.a_pz + st.a_mz < best) { best = st.a_pz + st.a_mz; axis_vox = float3(0, 0, 1); }
	[branch]
	if (!resolved && best < 1e8f)
	{
		sn.normal_ws = normalize(TransformVector(axis_vox, g_cbVxgi.mat_vox2ws));
		sn.double_sided = true;
		sn.path = VXGI_SURF_PATH_DOUBLE;
		resolved = true;
	}

	// Unreachable by construction (the classification passed via an empty face or a strong gradient),
	// kept as a defensive guard: report invalid so the caller writes 0 (debug mode 12 paints it magenta).
	// The declaration defaults already ARE the invalid state, so this only has to clear `valid`.
	if (!resolved)
		sn.valid = false;

	return sn;
}

#endif // VXGI_SURFACEVOXEL_HLSLI
