#include "../CommonShader.hlsl"

// -----------------------------------------------------------------------------
// VXGI v3 - Stage 3 : ONE volumetric light-diffusion iteration (progressive).
// Jacobi step of multiple scattering in a participating medium:
//     radiance' = direct  +  albedo * scatter_k * avg( neighbouring radiance )
// direct is the stable source (light arriving through the medium, from
// InjectLight); the neighbour average diffuses it a couple of voxels further
// into the medium every frame, so the scattered-light field visibly creeps
// inward while the engine's convergence loop keeps supplying frames (like TAA).
// Surface normals are NOT used — this is isotropic in-medium scattering, which
// is the correct model for a translucent volume (a hemisphere surface-gather
// sees mostly empty air around a volume and propagates nothing).
// -----------------------------------------------------------------------------

Texture3D grid_prev   : register(t8);  // previous radiance+opacity (MIP CHAIN)
Texture3D grid_mat    : register(t9);  // albedo+opacity (static per content)
Texture3D grid_direct : register(t10); // stable direct-light source (from InjectLight)
RWTexture3D<float4> grid_next : register(u0);

// VXGI_SCATTER_K (in-scatter gain per iteration, < 1 for stability) is defined in CommonShader.hlsl —
// shared so display-side consumers can normalize by the converged series gain 1/(1-K).

[numthreads(8, 8, 8)]
void VXGI_Propagate(uint3 id : SV_DispatchThreadID)
{
	uint R = g_cbVxgi.grid_res;
	if (R == 0)
		return;
	if (id.x >= R || id.y >= R || id.z >= R)
		return;

	float4 mat = grid_mat.Load(int4(id, 0));
	if (mat.a <= 0.0f)
	{
		grid_next[id] = (float4) 0;
		return;
	}

	float3 uv = (float3(id) + 0.5f) / (float) R;

	// Isotropic neighbour gather, OCCUPANCY-WEIGHTED, over 14 directions (6 axes + 8 diagonals) ~2 voxels
	// away at mip 1. The diagonals are ESSENTIAL: with axis-only taps, a thin DIAGONAL structure's interior
	// sees all six taps land in empty air (wsum -> 0, gather crushed by the floor) so light cannot spread
	// along it, while axis-aligned plates diffuse fine — exactly the observed anisotropy. Weighting each
	// tap by the MATERIAL opacity and normalizing by the occupied weight keeps the energy flowing through
	// the medium. NOTE weights come from grid_mat (t9), NOT grid_prev.a — the radiance grid's alpha carries
	// the baked per-voxel OBSCURANCE (see below).
	float rr = 2.0f / (float) R;
	const float dg = 0.57735027f; // 1/sqrt(3): unit diagonal component
	const float3 TAPS[14] = {
		float3( 1, 0, 0), float3(-1, 0, 0),
		float3(0,  1, 0), float3(0, -1, 0),
		float3(0, 0,  1), float3(0, 0, -1),
		float3( dg,  dg,  dg), float3(-dg,  dg,  dg), float3( dg, -dg,  dg), float3( dg,  dg, -dg),
		float3(-dg, -dg,  dg), float3(-dg,  dg, -dg), float3( dg, -dg, -dg), float3(-dg, -dg, -dg)
	};
	float3 gather = (float3) 0;
	float wsum = 0.0f;
	[unroll]
	for (int t = 0; t < 14; t++)
	{
		float3 tp = uv + TAPS[t] * rr;
		float w = grid_mat.SampleLevel(g_samplerLinear_clamp, tp, 1).a;
		gather += grid_prev.SampleLevel(g_samplerLinear_clamp, tp, 1).rgb * w;
		wsum += w;
	}
	gather /= max(wsum, 0.25f); // occupied-weighted mean (floor keeps isolated voxels stable)

	float3 direct = grid_direct.Load(int4(id, 0)).rgb;
	// DAMPED-Jacobi form: r' = (1-K)*direct + K*albedo*gather. The undamped `direct + K*gather` form
	// converges to direct/(1-K) (~6.7x blowout at K=0.85) in dense regions, and compensating that at
	// consumption with a flat (1-K) crushed the thin/near-surface regions (which have no gain) by the same
	// 6.7x — killing the translucent glow. The damped form's fixed point is direct-scale EVERYWHERE:
	// thin regions keep full direct-level scatter, dense interiors converge without amplification, and the
	// progressive iterations read as light spreading spatially rather than the image brightening.
	float3 radiance = (1.0f - VXGI_SCATTER_K) * direct + VXGI_SCATTER_K * (mat.rgb * gather);

	// Per-voxel OBSCURANCE, baked into the radiance grid's alpha (a FIELD, like everything else in this
	// volumetric pipeline — the DVR march then reads it per SAMPLE with one fetch). Evaluated at the voxel
	// CENTER from the material mips (local mean density at growing radii) and interpolated as a smooth
	// field, it is lattice-aligned and therefore free of the surface-vs-grid phase interference that made
	// every surface-point sampling scheme (the screen-space gather) stripe. Remap: see VXGI_Obscurance in
	// CommonShader.hlsl (flat surface ~0.3 base AO, crevices toward 1, exposed ridges toward 0).
	float d1 = grid_mat.SampleLevel(g_samplerLinear_clamp, uv, 1).a;
	float d2 = grid_mat.SampleLevel(g_samplerLinear_clamp, uv, 2).a;
	float d3 = grid_mat.SampleLevel(g_samplerLinear_clamp, uv, 3).a;
	float obscurance = VXGI_Obscurance(d1, d2, d3); // shared with InjectLight (identical bake)

	grid_next[id] = float4(radiance, obscurance);
}
