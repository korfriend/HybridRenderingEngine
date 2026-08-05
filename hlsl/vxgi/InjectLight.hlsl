#include "../CommonShader.hlsl"

// -----------------------------------------------------------------------------
// VXGI v3 - Stage 2 : Inject the light REACHING each voxel through the medium.
// Volumetric model (no surface normals): march a narrow cone from the voxel
// TOWARD EACH light of the CB set (Multi-Light: uniform loop, every light equal)
// through the material opacity (MAT has mips -> LOD-stepped, cheap full-range
// occlusion) and store
//     direct = albedo * SUM_i(color_i * intensity_i * transmittance_i)
// into the DIRECT grid. Voxels on the lit side are bright, deeply buried ones
// are dark — the VXGI_Propagate diffusion then spreads this field inward,
// frame by frame, producing progressive translucent scattering (so the indirect
// side consumes the same light set automatically — no dominant anywhere here).
// -----------------------------------------------------------------------------

Texture3D grid_mat : register(t9);              // rgb = albedo, a = opacity (MIP CHAIN)
Texture3D prev_direct : register(t11);          // LIGHT-ONLY rebuild: previous DIRECT (baked alpha source)
RWTexture3D<float4> grid_direct : register(u0); // rgb = direct light * coverage, a = obscurance * coverage
// PER-VOXEL LIGHT VISIBILITY (r = scalar visibility * coverage, replicated to rgb). This is why the grid
// does not have to be high resolution: visibility is a LOW-FREQUENCY quantity, so the field carries it
// while the DVR keeps its own full-resolution gradient shading and MULTIPLIES its directional shade by
// this instead of ADDING this grid's direct on top -- adding both is the direct double-count.
// Coverage-premultiplied like every other channel so the generated mips stay physically consistent.
RWTexture3D<float4> grid_vis : register(u1);

// ---- Multi-Light (plan ML-D3/ML-D4) ----
// The VXGI light set, LOCAL to this shader at b11 (CommonShader.hlsl declares nothing at b11; the only
// other b11 user is BlobParticle.hlsl, a separate unit that never includes this file's chain). Layout
// must match gpures_helper.h's CB_VxgiLights byte-for-byte (static_asserts on the C++ side).
// g_cbEnv's single light (env_flag bit0 / pos_light_ws / dir_light_ws / ltint_diffuse) is NO LONGER read
// here -- the direct (Phong) shading path keeps using it untouched.
// MUST equal gpures_helper.h's VXGI_MAX_LIGHTS: the two defines size the same b11 buffer from opposite
// sides, and a mismatch misaligns every light past the first. The C++ static_assert only guards the C++
// side, so editing one without the other is caught at RUNTIME (garbage lights), not at build time.
#define VXGI_MAX_LIGHTS 64
struct VxgiLight // 64 B = 4 float4 rows -- MUST match gpures_helper.h VxgiLight byte-for-byte (static_asserts C++ side)
{
    float3 pos_ws;   uint  flags;      // bit0 = positional (point/spot), bit1 = spot (cone attenuation)
    float3 dir_ws;   float intensity;  // spot: cone axis (ray travel dir); dir CPU-normalized (zero -> (0,0,-1))
    float3 color;    float cos_inner;  // spot only: cos(inner half-angle) -- former pad0 slot
    float cos_outer;                   // spot only: cos(outer half-angle)
    float spot_rsv0, spot_rsv1, spot_rsv2; // reserved (future spot params)
};
cbuffer cbVxgiLights : register(b11)
{
    uint light_count;                  // <= VXGI_MAX_LIGHTS; 0 = no light -> DIRECT rgb stays 0 (V17)
    uint vxgil_pad1, vxgil_pad2, vxgil_pad3;
    VxgiLight vxgi_lights[VXGI_MAX_LIGHTS];
};

[numthreads(8, 8, 8)]
void VXGI_InjectLight(uint3 id : SV_DispatchThreadID)
{
	uint R = g_cbVxgi.grid_res;
	if (R == 0)
		return;
	if (id.x >= R || id.y >= R || id.z >= R)
		return;

	float4 mat = grid_mat.Load(int4(id, 0));
	if (mat.a <= 0.0f)
	{
		grid_direct[id] = (float4) 0; // empty voxel
		grid_vis[id] = (float4) 0;    // MUST clear too: a UAV is not cleared between bakes, and a stale
		                              // visibility texel would shadow material that moved into this cell.
		return;
	}

	// --- Multi-Light (ML-D4): UNIFORM loop over the CB light set -- every light equal, no dominant
	// special-casing, ADDITIVE composition (more lights = brighter is the physical expectation;
	// gi_intensity is the exposure knob). The cone-march body below is the SINGLE copy, per light.
	// Bounds defense (rev.6 Major 2): explicit zero-init + a double clamp of the loop bound, so a
	// corrupt light_count can never index past the CB array. light_count == 0 -> loop runs 0 times ->
	// DIRECT rgb = 0: DETERMINISTIC darkness (V17; the old path leaked the default-constructed light).
	float3 uv = (float3(id) + 0.5f) / (float) R;
	float voxel = 1.0f / (float) R;
	float tan_half = 0.1f; // narrow shaft
	float3 light_sum = (float3) 0;
	float3 light_sum_unshadowed = (float3) 0; // same sum with T == 1 -> their ratio is the visibility
	uint n_lights = min(light_count, (uint) VXGI_MAX_LIGHTS);
	[loop]
	for (uint li = 0; li < n_lights; li++)
	{
		// --- direction toward this light, in voxel space ---
		float3 L;
		float d_light = 1e6f; // directional light: no positional bound on the march
		if (vxgi_lights[li].flags & 0x1)
		{
			// POINT light: occlusion must stop AT the light — a light inside the grid otherwise keeps
			// integrating material BEHIND itself, so structures past the light wrongly shadow this voxel.
			// NOTE no inverse-square attenuation here ON PURPOSE: the DVR's direct (Phong) shading applies
			// none, and adding it only to the GI field would skew the direct-vs-GI balance with distance —
			// introduce both together if physical falloff is ever wanted.
			float3 lp_vox = TransformPoint(vxgi_lights[li].pos_ws, g_cbVxgi.mat_ws2vox);
			float3 to_light = lp_vox - uv;
			d_light = length(to_light);
			L = to_light / max(d_light, 1e-6f);
		}
		else
		{
			L = -normalize(TransformVector(vxgi_lights[li].dir_ws, g_cbVxgi.mat_ws2vox));
		}

		// --- narrow occlusion cone toward the light: WORLD-length optical-depth integration ---
		// The stored alpha is a dimensionless coverage; treating it as opacity per sample made the
		// attenuation depend on the step schedule (near 0.5-voxel steps over-attenuated 2x, far
		// geometric steps diluted ~6x), on grid_res, and on the volume's WS anisotropy. Instead,
		// assign it a length: alpha = opacity over voxel_ref_ws of world thickness, i.e. extinction
		// sigma_t = -ln(1-a)/voxel_ref_ws, and integrate tau = sum(sigma_t * ds_ws) -> T = exp(-tau).
		// ds_ws converts the grid-space step through the (anisotropic) grid axis world lengths.
		float ws_per_grid = length(L * g_cbVxgi.grid_axis_ws); // world length of a unit grid step along L
		float tau_per_grid = ws_per_grid / max(g_cbVxgi.voxel_ref_ws, 1e-6f); // grid step -> voxel-thickness units
		float max_dist = min(g_cbVxgi.max_trace_dist, d_light); // point light: never march past the light
		float tau = 0.0f;
		// Start in REFERENCE voxels, not current voxels. Voxelize now builds a FIXED-WORLD ~2-reference-
		// voxel coverage ramp, so a 1.5-current-voxel start at R=256 begins INSIDE the voxel's own ramp --
		// every march integrates self-coverage as tau and the whole DIRECT field (and everything diffused
		// from it) darkens as R rises. The one transport start the resolution-invariance pass missed.
		float dist = 1.5f * voxel * VXGI_ResolutionScale();
		[loop]
		for (int i = 0; i < 64; i++)
		{
			if (dist >= max_dist || tau >= 6.0f) // exp(-6) < 0.25% -> opaque, stop
				break;
			float3 p = uv + L * dist;
			if (any(p < 0.0f) || any(p > 1.0f))
				break;
			float diam = max(voxel, 2.0f * tan_half * dist);
			float lod = clamp(log2(diam * (float) R), 0.0f, 5.0f);
			float a = grid_mat.SampleLevel(g_samplerLinear_clamp, p, lod).a;
			float step = diam * 0.5f;
			tau += -log(1.0f - min(a, 0.995f)) * (step * tau_per_grid);
			// growth is x1.1 per step once diam unclamps (dist > 5 voxels): full [0,1]
			// range needs ~41 steps at R=128, ~49 at R=256 — 64 covers both. Dense
			// voxels exit early on tau saturation, so the worst case is a clear shaft
			// toward the light, which samples high LODs (cheap).
			dist += step;
		}
		float T = exp(-tau); // transmittance: lit side ~1, deeply buried ~0

		// (rev.18) SPOT cone factor (bit1). Computed in WORLD space via mat_vox2ws so the cone stays a true
		// right-circular cone regardless of grid anisotropy. Non-spot lights keep spot = 1.0 (x*1.0 is exact
		// in IEEE754), so their DIRECT term is bit-identical to the pre-spot path (V29 parity).
		float spot = 1.0f;
		if (vxgi_lights[li].flags & 0x2u) // spot
		{
			float3 voxel_ws = TransformPoint(uv, g_cbVxgi.mat_vox2ws); // CommonShader.hlsl:157
			float3 axis     = normalize(vxgi_lights[li].dir_ws);       // ray travel direction (CPU guarantees unit/finite)
			// (verification Major 3) APEX GUARD: at the cone apex (voxel == light pos) the direction is
			// undefined; normalize(0) would be NaN. Use length-squared and treat the apex as on-axis (cos_a=1,
			// inside the cone) rather than dividing by zero.
			float3 delta = voxel_ws - vxgi_lights[li].pos_ws;
			float  d2    = dot(delta, delta);
			float  cos_a = (d2 > 1e-12f) ? dot(delta * rsqrt(d2), axis) : 1.0f;
			// inner<=outer => cos_inner>=cos_outer (CPU clamp, R16). Normal cone (cos_inner>cos_outer) uses
			// smoothstep. HARD EDGE (inner==outer => cos_inner==cos_outer): smoothstep(e,e,x) is 0/0 -> NaN,
			// so use an explicit step (finite, deterministic) instead (V26 no-NaN + hard-edge contract).
			float ci = vxgi_lights[li].cos_inner, co = vxgi_lights[li].cos_outer;
			spot = (ci > co) ? smoothstep(co, ci, cos_a) : (cos_a >= co ? 1.0f : 0.0f);
		}

		// ML-D10: per-light color * intensity (defaults white/1.0 = the old ltint_diffuse constant, so a
		// single default light reproduces the legacy DIRECT bit patterns up to FP re-association -- V1).
		float3 light_i = vxgi_lights[li].color * vxgi_lights[li].intensity * spot;
		light_sum += light_i * T;
		// FREE: the cone march that produced T has already run, this is one add.
		light_sum_unshadowed += light_i;
	}

	// alpha = per-voxel OBSCURANCE (shared VXGI_Obscurance — this seeds the radiance grid's alpha at
	// bounce 0 via the DIRECT->grid copy, so per-sample AO is valid before the first diffusion step).
	// Stored PREMULTIPLIED by the voxel's coverage (mat.a): mip filtering of a raw obscurance mixes it
	// with empty-voxel zeros in proportion to the LOCAL occupancy (a shell-following dilution = banding
	// in every mip-sampled consumer). Premultiplied, any filtered read divided by the same-lod MAT
	// coverage is an exact coverage-weighted average at every mip / trilinear position.
	float a_out;
	[branch]
	if (VXGI_PRESERVE_AO)
	{
		// SPLIT-STAMP light-only rebuild: the material did not change, so the baked alpha (cubic
		// obscurance) in the previous DIRECT is still exact — re-emit it instead of re-running the
		// 3x8 cubic taps. The C++ copied DIRECT -> PING and bound it here as t11 (a UAV cannot read
		// its own previous texels).
		a_out = prev_direct.Load(int4(id, 0)).a;
	}
	else
	{
		// Density taps: mips + cubic/trilinear are EXPERIMENT KNOBS — see the VXGI_AO_TAP_* defines in
		// CommonShader.hlsl (footprint vs contact-punch trade-off; cubic reconstruction suppresses the
		// mip-texel contour bands that trilinear taps print). The flat-surface baseline (half-space
		// density ~0.5) is SCALE-INVARIANT across mip choices, so the VXGI_Obscurance remap holds.
		// Fixed-world AO footprints: one extra mip whenever R doubles above the R=128 reference.
		// The old integer shifts silently halved every density radius at R=256.
		float lod_bias = VXGI_ResolutionLodBias();
		float lod1 = min((float) VXGI_AO_TAP_MIP1 + lod_bias, log2((float) R));
		float lod2 = min((float) VXGI_AO_TAP_MIP2 + lod_bias, log2((float) R));
		float lod3 = min((float) VXGI_AO_TAP_MIP3 + lod_bias, log2((float) R));
		float d1 = VXGI_SampleAoDensity(grid_mat, uv, lod1, (float) R);
		float d2 = VXGI_SampleAoDensity(grid_mat, uv, lod2, (float) R);
		float d3 = VXGI_SampleAoDensity(grid_mat, uv, lod3, (float) R);
		a_out = VXGI_Obscurance(d1, d2, d3) * mat.a;
	}

	static const float VXGI_DIRECT_SOURCE_SCALE = 1.0f;

	// direct = albedo * SUM_i(color_i * intensity_i * T_i) -- additive, no normalization (ML-D4).
	// RGB is coverage-premultiplied just like alpha. The radiance grid copies this seed and keeps the
	// same contract through Propagate, so GenerateMips computes average(coverage*radiance) instead of
	// treating every partial-coverage ramp voxel as a fully occupied emitter.
	float3 direct = mat.rgb * light_sum;
	grid_direct[id] = float4(direct * (VXGI_DIRECT_SOURCE_SCALE * mat.a), a_out);

	// SCALAR visibility = luminance(shadowed) / luminance(unshadowed). Deliberately NOT per-channel: a
	// pure-red light drives the G/B denominators to 0, the 'unlit -> fully lit' fallback then reports
	// G/B visibility 1, and the local Phong (which never sees light colour) keeps full G/B direct -- a
	// coloured shadow the light cannot cast. This is an AGGREGATE multi-light approximation, stated as
	// such: an exact per-light decomposition needs each light's N.L weight, which no voxel field has.
	// (Paper experiments should use a single dominant light, where the aggregate is exact.)
	// Where no light reaches the voxel at all the denominator is 0 too -- report 1 (fully lit) so a
	// light-less scene can never darken the DVR below its legacy look.
	const float3 LUM_W = float3(0.2126f, 0.7152f, 0.0722f); // Rec.709
	float lum_un = dot(light_sum_unshadowed, LUM_W);
	float vis = (lum_un > 1e-6f) ? saturate(dot(light_sum, LUM_W) / lum_un) : 1.0f;
	grid_vis[id] = float4(vis.xxx * mat.a, mat.a);
}
