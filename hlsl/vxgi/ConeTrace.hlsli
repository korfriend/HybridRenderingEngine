#ifndef VXGI_CONETRACE_HLSLI
#define VXGI_CONETRACE_HLSLI

// -----------------------------------------------------------------------------
// Voxel Cone Tracing (VXGI v1) shared helpers.
// All tracing happens in the grid's normalized [0,1] "voxel space", which is
// ALIGNED WITH THE VOLUME texture space (see SHARED CONTRACT). The grid stores
// rgb = injected radiance and a = opacity per voxel (R16G16B16A16_FLOAT).
// -----------------------------------------------------------------------------

// Orthonormal tangent basis around a unit normal n (n becomes the local +z).
void VXGI_BuildBasis(const in float3 n, out float3 t, out float3 b)
{
	float3 up = abs(n.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
	t = normalize(cross(up, n));
	b = cross(n, t);
}

// Tangent-space hemisphere cone set (local +z == surface normal).
// 1 axial cone + 5 side cones at 60 deg from the normal, 72 deg apart in azimuth.
// Cosine-ish weights summing to 1.0 for the full 6-cone set.
static const float3 VXGI_CONE_DIRS[6] =
{
	float3( 0.000000f,  0.000000f, 1.0f),
	float3( 0.000000f,  0.866025f, 0.5f),
	float3( 0.823639f,  0.267617f, 0.5f),
	float3( 0.509037f, -0.700629f, 0.5f),
	float3(-0.509037f, -0.700629f, 0.5f),
	float3(-0.823639f,  0.267617f, 0.5f)
};
static const float VXGI_CONE_WEIGHTS[6] = { 0.25f, 0.15f, 0.15f, 0.15f, 0.15f, 0.15f };

// Front-to-back march of a single cone through the grid in [0,1] voxel space.
// Returns float4( accumulated radiance.rgb, accumulated occlusion.a ).
// TRUE cone tracing: the sample LOD follows the cone diameter through the grid's
// mip chain (lod = log2(diameter_in_voxels)), so distant/wider footprints read
// pre-filtered coarser voxels — smooth, cheap, and correctly long-range.
#define VXGI_MAX_LOD 5.0f

float4 VXGI_TraceCone(Texture3D grid, SamplerState samp, const in float3 origin,
	const in float3 dir, const in float aperture, const in float max_dist, const in float grid_res)
{
	float voxel_size = 1.0f / max(grid_res, 1.0f);
	float3 acc_rgb = (float3) 0;
	float acc_occ = 0.0f;

	float tan_half = tan(0.5f * aperture);
	float dist = voxel_size; // origin is already pushed a couple voxels off the surface

	[loop]
	for (int i = 0; i < 64; i++)
	{
		if (dist >= max_dist || acc_occ >= 0.99f)
			break;

		float3 p = origin + dir * dist;
		if (any(p < 0.0f) || any(p > 1.0f))
			break;

		float diameter = max(voxel_size, 2.0f * tan_half * dist);
		float lod = clamp(log2(diameter * grid_res), 0.0f, VXGI_MAX_LOD);
		float4 s = grid.SampleLevel(samp, p, lod); // rgb = radiance, a = opacity (pre-filtered per mip)
		float a = s.a * (1.0f - acc_occ);          // front-to-back visibility weight
		acc_rgb += a * s.rgb;
		acc_occ += a;

		dist += diameter * 0.5f; // half-diameter steps: geometric growth, no gaps
	}

	return float4(acc_rgb, acc_occ);
}

// Trace the hemisphere around N_vox and return float4( avg indirect radiance.rgb, avg AO/occlusion.a ).
// num_cones (1..6) selects how many of the fixed cones to use; weights are renormalized accordingly.
float4 VXGI_ConeTraceGI(Texture3D grid, SamplerState samp, const in float3 origin_vox,
	const in float3 N_vox, const in float aperture, const in float max_dist,
	const in float grid_res, const in uint num_cones)
{
	float3 T, B;
	VXGI_BuildBasis(N_vox, T, B);

	uint nc = min(max(num_cones, 1u), 6u);

	float3 rad = (float3) 0;
	float occ = 0.0f;
	float wsum = 0.0f;

	[loop]
	for (uint c = 0; c < nc; c++)
	{
		float3 ld = VXGI_CONE_DIRS[c];
		float3 dir = normalize(ld.x * T + ld.y * B + ld.z * N_vox);
		float w = VXGI_CONE_WEIGHTS[c];

		float4 r = VXGI_TraceCone(grid, samp, origin_vox, dir, aperture, max_dist, grid_res);
		rad += w * r.rgb;
		occ += w * r.a;
		wsum += w;
	}

	if (wsum > 0.0f)
	{
		rad /= wsum;
		occ /= wsum;
	}
	return float4(rad, occ);
}

#endif // VXGI_CONETRACE_HLSLI
