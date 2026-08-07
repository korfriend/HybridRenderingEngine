#ifndef VXGI_CONETRACE_HLSLI
#define VXGI_CONETRACE_HLSLI

// -----------------------------------------------------------------------------
// Voxel Cone Tracing shared helpers (VXGI v5: 2-TEXTURE march only).
// All marching happens in the grid's normalized [0,1] "voxel space", which is
// ALIGNED WITH THE VOLUME texture space (see SHARED CONTRACT).
// HISTORY: the single-texture pair (VXGI_TraceCone / VXGI_ConeTraceGI, v1) was
// DELETED with the debug-view redesign (plan §3.6) — its last callers were the
// retired screen-space Gather modes 4/5. It also carried a latent defect the
// 2-texture split fixes: it used the radiance grid's alpha as the visibility
// weight, but that alpha has been obscurance*coverage (NOT opacity) since v4.
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

// TRUE cone tracing: the sample LOD follows the cone diameter through the grid's
// mip chain (lod = log2(diameter_in_voxels)), so distant/wider footprints read
// pre-filtered coarser voxels — smooth, cheap, and correctly long-range.
#define VXGI_MAX_LOD_BASE 5.0f

// Cone-AO coverage reconstruction: 1 = cubic B-spline for the near-field lods (band-free, default),
// 0 = raw trilinear (A/B comparison only). WHY: the box-filtered mips of the fixed-world surface
// band carry surface-vs-mip-lattice CONTOUR BANDING (strong from mip 1 up — verified in the raw
// coverage debug view), and the cone integrates that coverage into acc_occ, printing wave bands into
// the cone-AO channel (verified: bands vanish with SURFACE_CONE_AO_GAIN=0). This is the SAME
// pathology the obscurance density taps fixed with VXGI_SampleDensityCubic — same medicine here,
// applied where the banding lives (lod < 2; higher lods are coarse/soft and the cone is far from
// the surface by then). The rgb gather channel needs NO fix: its cov*(rad/cov) cancellation is
// band-immune by construction, and its trilinear divisor must stay trilinear to keep it exact.
// Cost: +7 fetches on the first ~5 steps of each cone (~+30% per checkpoint dispatch).
#define VXGI_CONE_AO_CUBIC 1
// Cubic applies to lods BELOW this (production value 2.0: the near-field steps that decide contact
// occlusion). 5.0 (= every lod) was the banding-hunt diagnostic; the visible bands turned out to be
// the debug viewer's transparency blending, and the mid/far-cone mip banding contribution was not
// distinguishable in the clean opaque view — revisit only if a smooth-view regression points here.
#define VXGI_CONE_AO_CUBIC_MAX_LOD_BASE 2.0f

// Tangent-plane slab window (voxels above the local surface plane): occlusion fades in between LO
// and HI of the sample footprint's LOWEST point. This is the self-intersection guard for the whole
// LOCAL SURFACE (a voxelized flat-but-tilted plane is a 1-voxel staircase, and its coverage AA ramp
// extends the "own surface" material up to ~2 reference voxels above the ideal plane — measured: with HI at
// 1.5 the side cones' footprints still clipped the staircase ramp and AO bands returned on flat
// facial bone at production origin offsets 2/2). HI=2.5 covers step+ramp; geometry that matters
// (crevice walls, undercuts) rises well past it. Prefer widening THIS window over pushing the cone
// origins farther out — the slab excludes only the surface-parallel shell, while a bigger origin
// offset forfeits contact occlusion from ALL nearby geometry in every direction.
#define VXGI_CONE_SLAB_FADE_LO 1.0f
#define VXGI_CONE_SLAB_FADE_HI 2.5f
// Distance-proportional slab widening: the tangent plane is only as good as the derived normal, and
// the residual normal wobble (a few degrees along the staircase phase isolines) tilts it — the REAL
// surface then rises above the assumed plane by dist*sin(tilt), so far cone samples re-detect the
// flat surface as an occluder EXACTLY where the normal bands: AO banding correlated with the normal
// view, plus faint AO on open flat bone where there should be none. Widening the slab by ~0.09
// voxel per voxel of distance (~5 deg of tolerated plane tilt) absorbs that error class: material
// lying nearly PARALLEL to the plane stays "own surface" at any distance, while genuine occluders
// (crevice walls, undercuts — rising ~1:1) lose only a negligible sliver.
#define VXGI_CONE_SLAB_TILT_MARGIN 0.05f

// -----------------------------------------------------------------------------
// 2-TEXTURE cone march (VXGI v5, Part C — SurfaceGather). Front-to-back march
// with the shared cone-growth schedule; the two roles the retired single-grid
// trace conflated are split onto the correct sources:
//   * visibility / occlusion  <- grid_mat.a — the TRUE coverage from Voxelize.
//     (The radiance grid's alpha is obscurance*coverage since v4, NOT opacity —
//     using it as a visibility weight was a latent defect; the retired debug
//     Gather mode 4 traces the MAT grid separately for exactly this reason.)
//   * gathered radiance       <- radiance grid .rgb, PREMULTIPLIED by the
//     same-lod coverage. InjectLight and Propagate apply coverage at mip 0, so
//     GenerateMips stores average(coverage*radiance) at every level. Dividing by
//     the trilinear same-lod coverage recovers the conditional radiance; the
//     subsequent visibility product applies coverage exactly once. This is exact
//     for both binary occupancy and the fixed-world soft-OTF coverage ramp.
// NOTE this function requires CommonShader.hlsl to be #included first (it reads
// nothing itself, but its GI wrapper below uses g_cbVxgi + TransformVector).
// -----------------------------------------------------------------------------

// cos_n = dot(cone dir, surface normal) in voxel space; clearance_g = origin height above the local
// tangent plane along N, in grid [0,1] units. Together they give each sample's analytic height above
// the surface plane for the STAIRCASE slab clip below — zero extra fetches.
float4 VXGI_TraceCone_2Tex(Texture3D grid_mat, Texture3D radiance, SamplerState samp,
	const in float3 origin, const in float3 dir, const in float aperture,
	const in float max_dist, const in float grid_res, const in float start_dist,
	const in float cos_n, const in float clearance_g)
{
	float voxel_size = 1.0f / max(grid_res, 1.0f);
	float3 acc_rgb = (float3) 0;
	float acc_occ = 0.0f;

	float tan_half = tan(0.5f * aperture);
	float resolution_scale = max(grid_res / VXGI_REFERENCE_GRID_RES, 1.0f);
	float lod_bias = log2(resolution_scale);
	float max_lod = min(log2(max(grid_res, 1.0f)), VXGI_MAX_LOD_BASE + lod_bias);
	float cubic_max_lod = VXGI_CONE_AO_CUBIC_MAX_LOD_BASE + lod_bias;
	// SELF-INTERSECTION: the surface-normal origin push alone is not enough — the true-coverage ramp
	// is ~2 reference voxels thick and a 60-deg side cone gains only cos(60) = 0.5 voxel of height per voxel
	// marched, so with a 1-voxel start its first samples still graze the voxel's OWN ramp (worse for
	// inner-band voxels, whose pushed origin barely clears the ramp): systematic false occlusion in
	// the cone-AO channel. The caller sets start_dist to push the first sample past its own ramp
	// ALONG THE CONE DIRECTION as well (SurfaceGather: 2 voxels).
	float dist = max(voxel_size, start_dist);

	[loop]
	for (int i = 0; i < 64; i++)
	{
		if (dist >= max_dist || acc_occ >= 0.99f)
			break;

		float3 p = origin + dir * dist;
		if (any(p < 0.0f) || any(p > 1.0f))
			break;

		float diameter = max(voxel_size, 2.0f * tan_half * dist);
		float lod = clamp(log2(diameter * grid_res), 0.0f, max_lod);
		float cov_tri = grid_mat.SampleLevel(samp, p, lod).a;                 // TRUE coverage (box mips)
		// occlusion/visibility coverage: cubic-reconstructed near the surface (see VXGI_CONE_AO_CUBIC)
		float cov = cov_tri;
		float kernel_r_vox = 0.5f * diameter * grid_res; // sampling footprint radius, voxels (trilinear)
#if VXGI_CONE_AO_CUBIC == 1
		[branch]
		if (lod < cubic_max_lod)
		{
			float lodq = floor(lod + 0.5f); // cubic needs a single mip's texel metric
			// lodq is integral here, so stay on the single-lattice helper (also keeps fxc from
			// inlining a fractional-mip branch into this dynamic cone loop).
			cov = VXGI_SampleDensityCubic(grid_mat, p, lodq, max(floor(grid_res / exp2(lodq)), 1.0f));
			// the B-spline kernel spans 4 texels of its mip REGARDLESS of the cone diameter — at
			// near-field lods it is WIDER than the cone footprint and is what actually reaches down
			// into the surface ramp; the slab height test below must use the real reach.
			kernel_r_vox = max(kernel_r_vox, 2.0f * exp2(lodq));
		}
#endif
		// RGB and cov_tri use the same trilinear footprint: pm/cov recovers conditional radiance.
		// Visibility may use the smoother cubic reconstruction above, but storage coverage is applied
		// exactly once because mip RGB itself is average(coverage*radiance).
		float3 rad = radiance.SampleLevel(samp, p, lod).rgb / max(cov_tri, 1e-3f);

		// STAIRCASE self-intersection guard (tangent-plane slab clip): a voxelized FLAT surface is a
		// 1-voxel staircase, and the near-field cone re-detects the neighbouring steps as occluders —
		// false AO bands along the staircase isolines (self-intersection extended from one voxel to
		// the whole local surface; survives any origin push, because the steps are real neighbours).
		// Everything within ~a voxel of the local tangent plane IS that surface (steps + coverage
		// ramp), so its contribution fades out by the sample footprint's LOWEST height above the
		// plane. Real geometry (a crevice's opposite wall, an undercut roof) rises well past the
		// slab and keeps occluding. The same weight applies to the rgb gather — material inside the
		// slab is the receiver's own surface, whose radiance would be self-illumination double-count.
		float h_low = (clearance_g + dist * cos_n) * grid_res - kernel_r_vox; // sampling reach bottom, in voxels above the plane
		float tilt_m = VXGI_CONE_SLAB_TILT_MARGIN * dist * grid_res;         // plane-tilt tolerance grows with distance
		float w_slab = smoothstep(VXGI_CONE_SLAB_FADE_LO * resolution_scale + tilt_m,
			VXGI_CONE_SLAB_FADE_HI * resolution_scale + tilt_m, h_low);

		float a = w_slab * cov * (1.0f - acc_occ);                            // front-to-back visibility
		acc_rgb += a * rad;
		acc_occ += a;

		dist += diameter * 0.5f; // half-diameter steps: geometric growth, no gaps
	}

	return float4(acc_rgb, acc_occ);
}

// Hemisphere gather around a WORLD-SPACE normal (VXGI v5, Part C). The tangent basis and the
// 6 cone directions are built in WS — building them from a voxel-space normal on an anisotropic
// grid mixes coordinate systems (plan §3.3) — and each cone direction is then converted to voxel
// space for the march exactly like InjectLight converts L:
//     dir_vox = normalize(TransformVector(dir_ws, g_cbVxgi.mat_ws2vox))
// The step/LOD schedule stays voxel-space (the wide 60-deg aperture makes the residual
// anisotropic distortion a second-order effect). Returns float4(avg indirect rgb, avg cone occlusion).
// clearance_g = origin height above the local tangent plane (grid units) — see the slab clip in
// VXGI_TraceCone_2Tex; the per-cone plane inclination cos_n is derived here in voxel space.
float4 VXGI_ConeTraceGI_2Tex(Texture3D grid_mat, Texture3D radiance, SamplerState samp,
	const in float3 origin_vox, const in float3 N_ws, const in float aperture,
	const in float max_dist, const in float grid_res, const in uint num_cones,
	const in float start_dist, const in float clearance_g)
{
	float3 T, B;
	VXGI_BuildBasis(N_ws, T, B);

	uint nc = min(max(num_cones, 1u), 6u);
	// Plane normals are covectors, not displacement directions. With voxel->world linear map A,
	// n_vox = A^T*n_ws. Using mat_ws2vox (= A^-1) here skewed the tangent-plane climb rate on
	// anisotropic DICOM grids even though cone directions themselves correctly use A^-1 below.
	float3 N_vox_a = normalize(mul(transpose((float3x3) g_cbVxgi.mat_vox2ws), N_ws));

	float3 rad = (float3) 0;
	float occ = 0.0f;
	float wsum = 0.0f;

	[loop]
	for (uint c = 0; c < nc; c++)
	{
		float3 ld = VXGI_CONE_DIRS[c];
		float3 dir_ws = normalize(ld.x * T + ld.y * B + ld.z * N_ws);
		float3 dir_vox = normalize(TransformVector(dir_ws, g_cbVxgi.mat_ws2vox));
		float w = VXGI_CONE_WEIGHTS[c];
		float cos_n = max(dot(dir_vox, N_vox_a), 0.05f); // cone's climb rate above the plane

		float4 r = VXGI_TraceCone_2Tex(grid_mat, radiance, samp, origin_vox, dir_vox, aperture, max_dist, grid_res, start_dist, cos_n, clearance_g);
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
