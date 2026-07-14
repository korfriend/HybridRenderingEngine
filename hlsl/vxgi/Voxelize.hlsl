#include "../CommonShader.hlsl"

// -----------------------------------------------------------------------------
// VXGI v1 - Stage 1 : Voxelize the DVR volume into the radiance/opacity grid.
// The grid box covers the volume box PLUS an empty margin shell (8 voxels per
// side): grid coord = volume ts * vox_fit_scale + vox_fit_offset. We sample the
// volume intensity at the mapped coord, run it through the same OTF LUT mapping
// DvrCS uses (voxels outside the volume stay empty), and store:
//     rgb = albedo (OTF color, un-associated)   a = opacity [0,1]
// (Radiance replaces rgb later in the InjectLight pass.)
//
// MEDIUM-VISIBILITY PARITY with DvrCS (per sub-sample, same bindings the DVR
// march uses): material the DVR hides must NOT occlude/scatter light here —
//   * clip box / plane  : IsInsideClipBound(pos_ws, g_cbClipInfo)   [b2]
//   * multi-OTF         : per-mask OTF row (mask id * tmap_size_x)  [t2]
//   * sculpt mask       : mask_vint==0 || mask_vint > sculpt_value  [t2]
//   * sculpt bits       : packed hidden-voxel bit test              [t7]
// gated by the VXGI_MEDIUM_* flag bits (mirroring the DVR's mode selection).
//
// VXGI_MEDIUM_CONTEXT (VR_MODE 2) is a FIFTH gate but NOT one of these, and is
// OPT-IN / DEFAULT OFF. The four above remove material — it is not there, so it
// must not occlude. Context modulation makes material see-through — it IS still
// there, and context-preserving DVR exists to keep it as context, so it should
// still block and scatter. Also, DvrCS applies MODULATE to the whole sample
// AFTER adding the in-scatter, so baking it here would apply it a second time
// and wash the GI out. Left in as an option (A/B, or a modulated-look GI).
//
// DO NOT "unify" further than visibility: DvrCS's opacity_correction and
// pre-integration MUST NOT be applied here. Those are ray-marching quantities
// tied to the DVR's sample step (opacity per sample distance / OTF aliasing
// between successive sample PAIRS). This grid's alpha is a COVERAGE fraction;
// its length semantics are assigned once by voxel_ref_ws (sigma_t conversion
// in InjectLight) — stacking the DVR's step correction on top would apply a
// length basis twice, and the 4^3 per-sub-sample OTF averaging below already
// plays pre-integration's anti-aliasing role for a field.
// -----------------------------------------------------------------------------

Texture3D tex3D_volume : register(t0);  // normalized intensity volume
Texture3D tex3D_volmask : register(t2); // mask volume (multi-OTF id / sculpt-mask value) — same slot as DvrCS
Buffer<float4> buf_otf : register(t3);  // unorm OTF LUT (rgb=color, a=opacity); multi-OTF: one row per mask id
// Packed sculpt bits, same slot AND ENCODING as DvrCS/VolumeRenderer.cpp (USE_SCULPT_BITS_TEX3D == 1):
// Texture3D<R32_UINT>, 32 voxels along X per texel, bit x&31 of texel (x>>5, y, z). Keep in sync.
Texture3D<uint> sculpt_bits_tex : register(t7);

RWTexture3D<float4> vxgi_grid : register(u0); // R16G16B16A16_FLOAT radiance/opacity grid

// Sub-sample jitter: 1 = stratified-jittered lattice (moire fix, default), 0 = regular +0.5 centers
// (A/B comparison only). WHY: the REGULAR 4^3 sub-lattice has a ~1.5-2.5 volume-texel pitch, which
// UNDERSAMPLES the OTF-stepped signal (the alpha step re-sharpens trilinear intensity to texel-period
// detail); a regular lattice beating against the volume texel lattice at a rational ratio prints a
// COHERENT multi-voxel moire into the coverage field itself — the contour-following "wave" bands seen
// in the obscurance (debug 2) and surface-normal (debug 6) views. Every consumer inherits a baked
// pattern; no read-side reconstruction filter can remove it. Jittering each sub-sample inside its
// stratum decorrelates the beat into fine noise that the ~2-voxel coverage ramp, the obscurance
// mips+blur and the cubic gradient taps all absorb.
// The jitter is seeded on the GLOBAL half-voxel stratum coordinate (not the local sub-sample index):
// neighbouring voxels' 2-voxel footprints OVERLAP at half-voxel pitch, so with global seeding the
// overlapping strata reuse the IDENTICAL jittered sample points — the whole field becomes a box
// average over ONE consistent jittered point set, so neighbour DIFFERENCES (the normal gradient!)
// carry no sampling noise. Deterministic (pure function of voxel id): stable across rebuilds, no
// temporal wobble, no stamp interaction.
#define VXGI_VOX_SS_JITTER 1

uint3 VXGI_Pcg3d(uint3 v) // Jarzynski & Olano PCG-3D
{
	v = v * 1664525u + 1013904223u;
	v.x += v.y * v.z; v.y += v.z * v.x; v.z += v.x * v.y;
	v ^= v >> 16u;
	v.x += v.y * v.z; v.y += v.z * v.x; v.z += v.x * v.y;
	return v;
}

[numthreads(8, 8, 8)]
void VXGI_VoxelizeVolume(uint3 id : SV_DispatchThreadID)
{
	uint R = g_cbVxgi.grid_res;
	if (R == 0)
		return;
	if (id.x >= R || id.y >= R || id.z >= R)
		return;

	// The grid box = volume box + an empty MARGIN shell (see vox_fit_scale/offset): convert this voxel's
	// grid coord to the VOLUME texture coord and leave voxels outside the volume empty, so boundary
	// diffusion / cone marches have room instead of clamping at the grid edge.
	const float3 uvc_grid = (float3(id) + 0.5f) / (float) R;
	const float3 tsc = (uvc_grid - g_cbVxgi.vox_fit_offset) / g_cbVxgi.vox_fit_scale; // voxel center in volume ts
	if (any(tsc < 0.0f) || any(tsc > 1.0f))
	{
		vxgi_grid[id] = (float4) 0; // margin shell: empty
		return;
	}

	// TRUE-COVERAGE voxelization: apply the OTF PER SUB-SAMPLE and average the resulting opacities, over a
	// 2-voxel footprint. Averaging INTENSITY first and thresholding once (OTF(avg) instead of avg(OTF))
	// re-sharpens the opacity band to ~1 voxel no matter how well the intensity is anti-aliased — the OTF's
	// alpha step undoes the box filter — and that razor-thin band's trilinear field is the maze/stripe
	// interference that debug mode 1 exposes (and every consumer inherits: AO cones, light march, diffusion).
	// Per-sub-sample OTF makes alpha the true occupied FRACTION of the footprint: a surface crossing the
	// voxel yields a smooth ~2-voxel coverage ramp, phase-insensitive by construction. Scene-gated cost only.
	// Visibility gates below run per sub-sample too, so a clip plane / sculpt edge crossing the footprint
	// yields the same smooth coverage ramp as a real material boundary.
	const int SS = 4;
	const float inv_ss = 1.0f / (float) SS;
	const float ts_cell = (1.0f / (float) R) / g_cbVxgi.vox_fit_scale; // one grid voxel, in volume ts units
	const float3 ts_min = tsc - 1.0f * ts_cell;                       // 2-voxel footprint centered on the voxel

	// Clip is now FLAGGED like the other medium gates (it used to read g_cbClipInfo.clip_flag directly).
	// The C++ sets VXGI_MEDIUM_CLIP only when the clip is BOTH active and wanted in the medium — turning
	// it off leaves the clipped material in the grid, so the clip becomes a pure viewing cutaway and its
	// state stops driving the content stamp (no re-voxelize per drag frame). The clip_flag test is kept
	// as a cheap belt-and-braces so a stale flag bit can never make IsInsideClipBound run on a zeroed CB.
	const bool clip_on = VXGI_MEDIUM_CLIP && (g_cbClipInfo.clip_flag != 0);
	const bool use_mask = VXGI_MEDIUM_OTF_MASK || VXGI_MEDIUM_SCULPT_MASK;
	const int sculpt_value = (int) (g_cbVobj.vobj_flag >> 24);

	float a_sum = 0.0f;
	float3 rgb_sum = (float3) 0;
	[loop]
	for (int s = 0; s < SS * SS * SS; s++)
	{
		int3 sc = int3(s & 3, (s >> 2) & 3, s >> 4); // stratum cell in the 4^3 footprint lattice
#if VXGI_VOX_SS_JITTER == 1
		// global half-voxel stratum coordinate (footprint spans [id-0.5, id+1.5] voxels = strata
		// 2*id-1 .. 2*id+2) -> overlapping footprints of neighbouring voxels hash to the SAME points
		uint3 gsi = asuint(int3(id) * 2 + sc - 1);
		float3 jit = float3(VXGI_Pcg3d(gsi)) * (1.0f / 4294967296.0f);
#else
		const float3 jit = 0.5f; // regular centers (moire-prone — A/B only)
#endif
		float3 off = (float3(sc) + jit) * inv_ss; // sub-sample position in [0,1)
		float3 ts = ts_min + off * (2.0f * ts_cell);

		// ---- visibility parity with DvrCS (hidden material contributes EMPTY, exactly like air) ----
		if (clip_on)
		{
			float3 pos_ws = TransformPoint(ts * g_cbVxgi.vox_fit_scale + g_cbVxgi.vox_fit_offset, g_cbVxgi.mat_vox2ws);
			if (!IsInsideClipBound(pos_ws, g_cbClipInfo))
				continue;
		}
		int mask_vint = 0;
		if (use_mask)
			mask_vint = (int) (tex3D_volmask.SampleLevel(g_samplerPoint_clamp, ts, 0).r * g_cbVobj.mask_value_range + 0.5f);
		if (VXGI_MEDIUM_SCULPT_MASK && !(mask_vint == 0 || mask_vint > sculpt_value))
			continue;
		if (VXGI_MEDIUM_SCULPT_BITS)
		{
			int3 voxel_id = (int3) (saturate(ts) * (g_cbVobj.vol_original_size - uint3(1, 1, 1)));
			uint word = sculpt_bits_tex.Load(int4(voxel_id.x >> 5, voxel_id.y, voxel_id.z, 0));
			if (word & (1u << ((uint) voxel_id.x & 31u)))
				continue;
		}

		float sv = tex3D_volume.SampleLevel(g_samplerLinear_clamp, ts, 0).r;
		int oi = clamp((int) (sv * g_cbTmap.tmap_size_x), 0, (int) g_cbTmap.tmap_size_x - 1);
		// bare OTF row lookup ONLY (multi-OTF: the mask id selects the row) — see the header note on why
		// opacity_correction / pre-integration must NOT be applied to this coverage field.
		int oi_off = VXGI_MEDIUM_OTF_MASK ? mask_vint * (int) g_cbTmap.tmap_size_x : 0;
		float4 o = buf_otf[oi + oi_off]; // unorm rgba : rgb = albedo, a = opacity

		// ---- CONTEXT-AWARE (VR_MODE 2) medium — OPT-IN, DEFAULT OFF (_bool_VxgiContextMedium).
		// See the header: this is deliberately NOT one of the parity gates. It makes the grid's medium
		// follow the modulated picture, which measurably washes the GI out (the modulator then lands
		// twice: once here, once when DvrCS scales the composited sample), and it deletes from the light
		// transport the very material context-preserving DVR exists to preserve. Kept for A/B and for a
		// deliberately modulated-look GI.
		//
		// What gets baked: MODULATE = A * B * C, of which only A is view-independent:
		//     A = min(|grad| * 2 * value_range * grad_scale / grad_max, 1)   <- baked here
		//     B = pow(clamp(depth_along_ray, .1, 1), kappa_i)                <- view: NOT bakeable
		//     C = pow(max(1 - |dot(view, N)|, .1), kappa_s)                  <- view: NOT bakeable
		// kappa_i/kappa_s default to 0 in the 3D DVR (VolumeRenderer.cpp), so pow(x,0)==1 and MODULATE
		// reduces exactly to A there. (Raising kappa makes this an over-occluding approximation; the
		// Curved slicer defaults kappa to 1 and is not covered — see VXGI_CONTEXT_DVR_PLAN.md.)
		//
		// The gradient comes from the ORIGINAL CT volume (t0) with the DVR's own voxel-step basis, via
		// CENTRAL DIFFERENCE. NOT the DVR's GRAD_VOL (= GradientVolume3): that one is built on the ray
		// basis and scaled by sample_dist, i.e. view- and samplerate-dependent, so it cannot be
		// reproduced in a view-independent grid. The calibration point therefore differs slightly from
		// the screen's; VXGI_CONTEXT_ALPHA_GAIN absorbs it.
		//
		// PER SUB-SAMPLE, like the OTF above, and for the same reason: modulating the AVERAGED alpha
		// would re-sharpen the coverage ramp to ~1 voxel and reintroduce the interference the
		// per-sub-sample OTF exists to prevent.
		if (VXGI_MEDIUM_CONTEXT)
		{
			float3 g = GradientVolume(ts, g_cbVobj.vec_grad_x, g_cbVobj.vec_grad_y, g_cbVobj.vec_grad_z, tex3D_volume);
			// grad_max == 0 is reachable (homogeneous / degenerate volume: GradientMagnitudeAnalysis).
			// The DVR would only produce a bad pixel; here a NaN would be BAKED into the grid and then
			// spread through mips / InjectLight / Propagate / SurfaceGather until the next rebuild.
			// Same epsilon as the screen-side guard in MODULATE — they must saturate at the same point.
			float m = min(length(g) * 2.f * g_cbVobj.value_range * g_cbVobj.grad_scale
				/ max(g_cbVobj.grad_max, 1e-6f), 1.f);
			// saturate: coverage in [0,1] is a contract (sigma_t conversion, cone visibility, surface
			// classification all assume it) and the gain is allowed to exceed 1.
			o.a = saturate(o.a * m * VXGI_CONTEXT_ALPHA_GAIN);
		}

		a_sum += o.a;
		rgb_sum += o.rgb * o.a; // opacity-weighted albedo (empty sub-samples must not darken the color)
	}
	float alpha = a_sum * (inv_ss * inv_ss * inv_ss); // occupied fraction of the footprint
	float3 albedo = rgb_sum / max(a_sum, 1e-4f);

	vxgi_grid[id] = float4(albedo, alpha);
}
