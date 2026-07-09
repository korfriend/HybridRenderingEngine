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

static const float VXGI_SCATTER_K = 0.85f; // in-scatter gain per iteration (keep < 1 for stability)

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

	// Isotropic neighbour gather, OCCUPANCY-WEIGHTED: 6 axis taps ~2 voxels away at mip 1. Weighting each
	// tap by its opacity and normalizing by the occupied weight (instead of a fixed /6) keeps the energy
	// flowing through the medium — a plain average counts empty-air taps as zero radiance and kills the
	// diffusion within a few voxels of the surface (the "flash but no spreading" symptom).
	float rr = 2.0f / (float) R;
	const float3 TAPS[6] = {
		float3( rr, 0, 0), float3(-rr, 0, 0),
		float3(0,  rr, 0), float3(0, -rr, 0),
		float3(0, 0,  rr), float3(0, 0, -rr)
	};
	float3 gather = (float3) 0;
	float wsum = 0.0f;
	[unroll]
	for (int t = 0; t < 6; t++)
	{
		float4 s = grid_prev.SampleLevel(g_samplerLinear_clamp, uv + TAPS[t], 1);
		gather += s.rgb * s.a;
		wsum += s.a;
	}
	gather /= max(wsum, 0.25f); // occupied-weighted mean (floor keeps isolated voxels stable)

	float3 direct = grid_direct.Load(int4(id, 0)).rgb;
	float3 radiance = direct + mat.rgb * gather * VXGI_SCATTER_K;

	grid_next[id] = float4(radiance, mat.a);
}
