// -----------------------------------------------------------------------------
// VXGI - surface-band-aware 3^3 Gaussian on the SURF grid (Part C cone results).
//
// WHY: SurfaceGather traces its hemisphere cones from each surface VOXEL CENTER.
// Along an oblique surface, neighbouring centers sit at different depths inside
// the coverage ramp (surface-vs-lattice phase), so the cone occlusion surf.a
// oscillates with lattice period along the surface — visible verbatim in debug
// view 4, and screen-blended into the obscurance alpha by Propagate, it is the
// band that caps how LOW the DVR's AO consume lod can go (owner-verified:
// cone-AO gain 0 -> low lods clean; gain 1 -> mip0's band prints). Averaging a
// surface voxel with its surface NEIGHBOURS mixes shallow- and deep-buried
// centers — the same lattice-aligned source-side medicine as BlurMat/BlurDirect,
// applied to the last untreated cone-origin field. rgb (surface indirect) rides
// along: same origin, same phase, additive-and-weak so it never showed, but
// smoothing it is consistent and free here.
//
// SURFACE-AWARE WEIGHTS: the SURF grid is EXACTLY 0 on non-surface voxels
// (SurfaceGather rewrites wholesale), and a genuine surface voxel may
// legitimately store 0 (fully open + unlit) — so value-based masking is wrong
// twice. Weight each tap by the REAL classifier (mat.a > 0 &&
// VXGI_ClassifySurface(...).is_surface — the same predicate the gather and the
// debug views use), so empty/interior neighbours contribute NOTHING and the
// average never dilutes toward 0 at the band edge. Non-surface centers pass
// their (zero) value through untouched.
//
// CADENCE: once per SurfaceGather (checkpoint bounces + throttled drag rewrites)
// — a checkpoint-class cost on shell voxels only (~27 classifier probes each).
// BINDING: t8 = freshly gathered SURF, t9 = MAT (classifier + weights),
// u0 = PING scratch; the C++ copies PING back over SURF.
// -----------------------------------------------------------------------------
#include "../CommonShader.hlsl"
#include "SurfaceVoxel.hlsli"

Texture3D grid_surf_src : register(t8); // rgb = surface indirect, a = cone occlusion
Texture3D grid_mat : register(t9);      // classifier input + coverage
RWTexture3D<float4> surf_out : register(u0); // PING (copied back by the C++)

// RADIUS 2 (5^3): radius 1 attenuated 4-6 voxel band periods by only ~55-30% — measured as "barely
// visible in debug 4, no final-image effect". Radius 2 / sigma 1.2 lifts that to ~87%/58% while
// staying inside BlurMat's sigma <= 0.6*RADIUS window rule (a clipped tail tends back toward a box).
// ~125 classifier-gated taps per SURFACE voxel, checkpoint cadence only — BlurMat-class cost.
#define VXGI_SURF_BLUR_RADIUS 2
#define VXGI_SURF_BLUR_SIGMA  1.2f

[numthreads(8, 8, 8)]
void VXGI_BlurSurf(uint3 id : SV_DispatchThreadID)
{
	uint R = g_cbVxgi.grid_res;
	if (R == 0)
		return;
	if (id.x >= R || id.y >= R || id.z >= R)
		return;

	const int3 ip = int3(id);
	const int Rm1 = (int) R - 1;
	const float4 center = grid_surf_src.Load(int4(ip, 0));

	float mata = grid_mat.Load(int4(ip, 0)).a;
	bool center_is_surface = (mata > 0.0f) && VXGI_ClassifySurface(grid_mat, ip, Rm1).is_surface;
	if (!center_is_surface)
	{
		surf_out[id] = center; // non-surface: pass the (zero) value through untouched
		return;
	}

	const float inv2s2 = 1.0f / (2.0f * VXGI_SURF_BLUR_SIGMA * VXGI_SURF_BLUR_SIGMA);
	float wsum = 0.0f;
	float4 acc = (float4) 0;
	[loop]
	for (int z = -VXGI_SURF_BLUR_RADIUS; z <= VXGI_SURF_BLUR_RADIUS; z++)
	{
		[loop]
		for (int y = -VXGI_SURF_BLUR_RADIUS; y <= VXGI_SURF_BLUR_RADIUS; y++)
		{
			[loop]
			for (int x = -VXGI_SURF_BLUR_RADIUS; x <= VXGI_SURF_BLUR_RADIUS; x++)
			{
				int3 sp = clamp(ip + int3(x, y, z), (int3) 0, (int3) Rm1);
				float mat_n = grid_mat.Load(int4(sp, 0)).a;
				if (mat_n <= 0.0f)
					continue;
				if (!VXGI_ClassifySurface(grid_mat, sp, Rm1).is_surface)
					continue;
				float w = exp(-(float) (x * x + y * y + z * z) * inv2s2);
				acc += w * grid_surf_src.Load(int4(sp, 0));
				wsum += w;
			}
		}
	}
	// wsum >= the center's own weight (the center classified as surface above), so this never
	// divides by zero and an isolated surface voxel keeps its value exactly.
	surf_out[id] = acc / wsum;
}
