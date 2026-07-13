#include "../CommonShader.hlsl"

// -----------------------------------------------------------------------------
// VXGI v3 - Stage 2 : Inject the light REACHING each voxel through the medium.
// Volumetric model (no surface normals): march a narrow cone from the voxel
// TOWARD the light through the material opacity (MAT has mips -> LOD-stepped,
// cheap full-range occlusion) and store
//     direct = albedo * light_tint * transmittance
// into the DIRECT grid. Voxels on the lit side are bright, deeply buried ones
// are dark — the VXGI_Propagate diffusion then spreads this field inward,
// frame by frame, producing progressive translucent scattering.
// -----------------------------------------------------------------------------

Texture3D grid_mat : register(t9);              // rgb = albedo, a = opacity (MIP CHAIN)
Texture3D prev_direct : register(t11);          // LIGHT-ONLY rebuild: previous DIRECT (baked alpha source)
RWTexture3D<float4> grid_direct : register(u0); // rgb = arriving direct light, a = per-voxel obscurance

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
		return;
	}

	// --- direction toward the light, in voxel space ---
	float3 uv = (float3(id) + 0.5f) / (float) R;
	float3 L;
	float d_light = 1e6f; // directional light: no positional bound on the march
	if (g_cbEnv.env_flag & 0x1)
	{
		// POINT light: occlusion must stop AT the light — a light inside the grid otherwise keeps
		// integrating material BEHIND itself, so structures past the light wrongly shadow this voxel.
		// NOTE no inverse-square attenuation here ON PURPOSE: the DVR's direct (Phong) shading applies
		// none, and adding it only to the GI field would skew the direct-vs-GI balance with distance —
		// introduce both together if physical falloff is ever wanted.
		float3 lp_vox = TransformPoint(g_cbEnv.pos_light_ws, g_cbVxgi.mat_ws2vox);
		float3 to_light = lp_vox - uv;
		d_light = length(to_light);
		L = to_light / max(d_light, 1e-6f);
	}
	else
	{
		L = -normalize(TransformVector(g_cbEnv.dir_light_ws, g_cbVxgi.mat_ws2vox));
	}

	// --- narrow occlusion cone toward the light: WORLD-length optical-depth integration ---
	// The stored alpha is a dimensionless coverage; treating it as opacity per sample made the
	// attenuation depend on the step schedule (near 0.5-voxel steps over-attenuated 2x, far
	// geometric steps diluted ~6x), on grid_res, and on the volume's WS anisotropy. Instead,
	// assign it a length: alpha = opacity over voxel_ref_ws of world thickness, i.e. extinction
	// sigma_t = -ln(1-a)/voxel_ref_ws, and integrate tau = sum(sigma_t * ds_ws) -> T = exp(-tau).
	// ds_ws converts the grid-space step through the (anisotropic) grid axis world lengths.
	float voxel = 1.0f / (float) R;
	float tan_half = 0.1f; // narrow shaft
	float ws_per_grid = length(L * g_cbVxgi.grid_axis_ws); // world length of a unit grid step along L
	float tau_per_grid = ws_per_grid / max(g_cbVxgi.voxel_ref_ws, 1e-6f); // grid step -> voxel-thickness units
	float max_dist = min(g_cbVxgi.max_trace_dist, d_light); // point light: never march past the light
	float tau = 0.0f;
	float dist = 1.5f * voxel;
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
		float d1 = VXGI_SampleAoDensity(grid_mat, uv, VXGI_AO_TAP_MIP1, (float)max(R >> VXGI_AO_TAP_MIP1, 1u));
		float d2 = VXGI_SampleAoDensity(grid_mat, uv, VXGI_AO_TAP_MIP2, (float)max(R >> VXGI_AO_TAP_MIP2, 1u));
		float d3 = VXGI_SampleAoDensity(grid_mat, uv, VXGI_AO_TAP_MIP3, (float)max(R >> VXGI_AO_TAP_MIP3, 1u));
		a_out = VXGI_Obscurance(d1, d2, d3) * mat.a;
	}

	static const float VXGI_DIRECT_SOURCE_SCALE = 1.0f;

	float3 direct = mat.rgb * g_cbEnv.ltint_diffuse.rgb * T;// * seed;
	grid_direct[id] = float4(direct * VXGI_DIRECT_SOURCE_SCALE, a_out);
}
