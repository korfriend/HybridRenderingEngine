#include "../CommonShader.hlsl"

// -----------------------------------------------------------------------------
// VXGI v5 - Stage 3 : ONE volumetric light-diffusion iteration (progressive).
// Jacobi step of multiple scattering in a participating medium:
//     radiance' = direct
//               + SURFACE_GI_GAIN * (1 - VXGI_SCATTER_GAIN) * surf   (Part C source term)
//               + VXGI_SCATTER_GAIN * albedo * gather                (Part D in-medium diffusion)
// direct is the stable source (light arriving through the medium, from
// InjectLight); surf is the surface-voxel cone-traced indirect (SurfaceGather,
// rewritten wholesale at checkpoint bounces); the ray-step gather diffuses both
// a couple of voxels further into the medium every frame, so the scattered
// field visibly creeps inward while the engine's convergence loop keeps
// supplying frames (like TAA).
// Surface normals are NOT used here — this is isotropic in-medium scattering,
// which is the correct model for a translucent volume. The DIRECTIONAL,
// long-range surface-to-surface transport lives in SurfaceGather (Part C):
// isotropic occupancy-weighted taps contribute 0 across empty space, so this
// pass can never carry light between disjoint surfaces no matter how many
// directions it uses — the two passes are physically complementary, not
// redundant (see VXGI_IMPLEMENTATION_PLAN.md §0).
// -----------------------------------------------------------------------------

Texture3D grid_prev   : register(t8);  // previous radiance*coverage + obscurance*coverage (MIP CHAIN)
Texture3D grid_mat    : register(t9);  // albedo+opacity (static per content)
Texture3D grid_direct : register(t10); // stable direct-light source (from InjectLight) — SRV forever
Texture3D grid_surf   : register(t11); // surface cone indirect (rgb, albedo-modulated) + cone occlusion (a)
RWTexture3D<float4> grid_next : register(u0);

// VXGI_SCATTER_GAIN / VXGI_SURFACE_GI_GAIN / VXGI_SURFACE_CONE_AO_GAIN are defined in CommonShader.hlsl.

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

	// Isotropic in-medium scattering, ray-step mini-march per direction (Part D).
	// 14 directions (6 axes + 8 corner diagonals), the same proven set as the previous fixed-tap
	// version — axis-only taps miss thin diagonal structures (documented anisotropy bug: their
	// interior saw all six taps land in empty air, wsum -> 0, so light could not spread along them).
	// Per direction, march STEPS samples with the shared cone-growth schedule (diam = 2*tan_half*dist,
	// lod = log2(diam*R), dist += diam/2) — fixed compile-time step count, no early-out, so there is
	// no warp divergence. STEPS=3 covers the previous tap's reach (2 voxels @ mip 1) while adding a
	// precise mip-0 near sample; STEPS=2 tops out at 1.5 voxels (lod 0.58) and REGRESSES the
	// per-bounce diffusion distance — cost option only, gated on a creep-depth measurement.
	// COST OPTION: DIRS can be reduced to the 6 axes if broad testing confirms the difference stays
	// negligible across datasets (thin diagonal structures: vessels, trabecular bone).
	// Each step is independently occupancy-weighted (grid_mat.a) and averaged — no occlusion chain
	// along the march, consistent with the isotropic diffusion model. grid_prev.rgb is already
	// coverage-premultiplied, including every generated mip, so it is the weighted numerator; the
	// same-lod grid_mat.a is only accumulated into the denominator. Multiplying rgb by coverage again
	// would square the partial-coverage ramp. grid_prev.a carries baked OBSCURANCE, not occupancy.
	const float dg = 0.57735027f; // 1/sqrt(3): unit diagonal component
	const float3 DIRS_14[14] = {
		float3( 1, 0, 0), float3(-1, 0, 0),
		float3(0,  1, 0), float3(0, -1, 0),
		float3(0, 0,  1), float3(0, 0, -1),
		float3( dg,  dg,  dg), float3(-dg,  dg,  dg), float3( dg, -dg,  dg), float3( dg,  dg, -dg),
		float3(-dg, -dg,  dg), float3(-dg,  dg, -dg), float3( dg, -dg, -dg), float3(-dg, -dg, -dg)
	};
	static const int STEPS = 3;        // default 3 (2 only after the reach-regression check, plan §2.1)
	const float tan_half = 0.5f;       // wide diffusion aperture
	const float voxel = 1.0f / (float) R;
	// One propagation kernel retains the R=128 world-space reach. At R=256 this starts at two current
	// voxels and follows the same 1,1.5,2.25-reference-voxel schedule; using one current voxel halved
	// the physical diffusion length and preserved twice as much high-frequency bake structure.
	const float transport_voxel = voxel * VXGI_ResolutionScale();

	float3 gather = (float3) 0;
	float wsum = 0.0f;
	[unroll]
	for (int d = 0; d < 14; d++)
	{
		float3 dir = DIRS_14[d];
		float dist = transport_voxel; // step 1: one reference-grid voxel
		[unroll]
		for (int s = 0; s < STEPS; s++)
		{
			float diam = max(voxel, 2.0f * tan_half * dist);
			float lod = clamp(log2(diam * (float) R), 0.0f, 5.0f);
			float3 p = uv + dir * dist;
			float w = grid_mat.SampleLevel(g_samplerLinear_clamp, p, lod).a;
			gather += grid_prev.SampleLevel(g_samplerLinear_clamp, p, lod).rgb;
			wsum += w;
			dist += diam * 0.5f; // same direction, farther + higher lod (x1.5 geometric growth)
		}
	}
	gather /= max(wsum, 0.25f); // occupied-weighted mean (floor keeps isolated voxels stable)

	float4 direct4 = grid_direct.Load(int4(id, 0));
	float3 direct = direct4.rgb / max(mat.a, 1e-4f); // DIRECT rgb is coverage-premultiplied
	// Part C source term: the surface cone indirect, rewritten wholesale at checkpoint bounces.
	// surf coefficient = SURFACE_GI_GAIN * (1 - SCATTER_GAIN): the (1-GAIN) normalization cancels the
	// propagate resolvent's dense-interior amplification 1/(1-SCATTER_GAIN), so the surface<->surface
	// feedback loop gain stays raw_gain * albedo * cone_visibility — bounded by the CPU-side clamp of
	// SURFACE_GI_GAIN to [0, 0.95] (strict contraction even at albedo=1). Dropping this (1-GAIN)
	// factor breaks the convergence contract in plan §4.4. Non-surface voxels carry surf == 0.
	float4 surf = grid_surf.Load(int4(id, 0));
	// SOURCE-TERM form: r' = direct + surf_term + GAIN*albedo*gather — direct is composed, never damped.
	// History: `direct + 0.85*gather` blew up to direct/(1-0.85) ~ 6.7x in dense interiors; the damped
	// fix `(1-K)*direct + K*albedo*gather` bounded that but scaled DIRECT by (1-K), only restoring it
	// where the neighbour gather could: an isolated/thin voxel (gather -> 0) converged to 0.15*direct
	// (85% direct loss), and since the seed is grid=DIRECT, convergence visibly DIMMED toward that —
	// brightness wobble on every rebuild/crossfade. The blow-up was the 0.85 gain, not the additive
	// form: gather is a weighted neighbour average (gain <= albedo <= 1), so with GAIN=0.5 the series
	// is bounded by direct/(1-GAIN) = 2x, isolated voxels keep exactly 1.0*direct, and iteration from
	// the DIRECT seed is monotone brightening (light creeping inward), never a dimming transient.
	float3 radiance = direct + VXGI_SURFACE_GI_GAIN * (1.0f - VXGI_SCATTER_GAIN) * surf.rgb
	                         + VXGI_SCATTER_GAIN * (mat.rgb * gather);

	// Per-voxel OBSCURANCE alpha: the density obscurance from the DIRECT field (material-only, baked
	// once per rebuild by InjectLight — premultiplied by coverage, cubic-B-spline taps + optional blur)
	// SCREEN-BLENDED with the surface cone occlusion (grid_surf.a, a free by-product of the Part C
	// cones — lattice-aligned, so it has none of the retired v1..v3 screen-space cone AO's
	// surface-vs-lattice phase striping). Screen blend is monotone and bounded (no double-darkening
	// cliff), and with surf.a == 0 (non-surface voxel / Part C off) or gain 0 it reduces EXACTLY to
	// the previous density-only obscurance — full backward compatibility. The result is re-premultiplied
	// by coverage (mat.a), so the DVR's un-premultiply + sqrt consumption path needs no change.
	float obsc_density = direct4.a / max(mat.a, 1e-4f);
	float obsc = 1.0f - (1.0f - obsc_density) * (1.0f - saturate(VXGI_SURFACE_CONE_AO_GAIN * surf.a));
	// Keep both channels coverage-premultiplied. Generated mips are then physically consistent on
	// soft OTF ramps, and consumers recover the conditional occupied-voxel value with a same-lod
	// divide by grid_mat.a.
	grid_next[id] = float4(radiance * mat.a, obsc * mat.a);
}
