#include "../CommonShader.hlsl"

// -----------------------------------------------------------------------------
// VXGI v5 - Stage 1.5 : 3^3 Gaussian on the MAT grid, once per voxelize.
// WHY (the banding endgame): the thin surface band's BOX mips store a
// surface-vs-mip-lattice phase pattern IN THE TEXEL VALUES themselves — read-
// side reconstruction (cubic taps) only smooths BETWEEN texels and cannot
// remove it. The surface cones integrate that stored pattern, so their
// occlusion bands as a function of the receiver position — verified to survive
// a FIXED normal, cubic sampling at every lod, huge origin offsets and the
// tangent-plane slab clip, which exonerates every read-side stage. Blurring
// mip 0 BEFORE GenerateMips makes consecutive mip texels' effective kernels
// OVERLAP (box-of-Gaussian ~= a wider Gaussian), strongly attenuating the
// stored phase energy at every level — the single-source fix every consumer
// (cone visibility/AO, obscurance density taps, normal gradient, light march)
// inherits.
// Weights (1,2,1)^3/64. Alpha is averaged plainly; albedo is OPACITY-WEIGHTED
// (empty neighbours must not darken the color — same convention as Voxelize).
// Trade-off: the coverage ramp widens ~2 -> ~3 voxels (slightly softer optical
// depth in InjectLight, slightly wider surface band). Gated by
// _bool_VxgiMatBlur (folded into the MAT stamp).
// -----------------------------------------------------------------------------

Texture3D grid_mat_src : register(t9);        // freshly voxelized MAT (mip 0)
RWTexture3D<float4> grid_out : register(u0);  // PING (copied back to MAT mip 0 by the C++)

// Kernel knobs. RADIUS 1 = 3^3 (27 taps/voxel, ~57M loads at 128^3 — rebuild-only), 2 = 5^3
// (125 taps, ~260M — still rebuild-class), 3 = 7^3 (343 taps — heavy; if that is ever really
// needed, rewrite as separable X/Y/Z passes like BlurObscurance instead). SIGMA in voxels sets
// the Gaussian falloff inside the window (0.8 with R=1 ~= the (1,2,1)^3 binomial this replaced;
// keep SIGMA <= ~0.6*R or the window CLIPS the Gaussian tail and the kernel tends back toward a
// box — the very shape whose mip beating we are removing). Wider kernel = stronger band
// suppression but a wider coverage ramp: thin structures (septa, cortical shells) erode first —
// check the double-sided/thin-plate content before committing an increase.
#define VXGI_MAT_BLUR_RADIUS 1
#define VXGI_MAT_BLUR_SIGMA  0.8f

[numthreads(8, 8, 8)]
void VXGI_BlurMat(uint3 id : SV_DispatchThreadID)
{
	uint R = g_cbVxgi.grid_res;
	if (R == 0)
		return;
	if (id.x >= R || id.y >= R || id.z >= R)
		return;

	const int3 ip = int3(id);
	const int Rmax = (int) R - 1;
	const float inv2s2 = 1.0f / (2.0f * VXGI_MAT_BLUR_SIGMA * VXGI_MAT_BLUR_SIGMA);

	float wsum = 0.0f;
	float asum = 0.0f;
	float3 ca_sum = (float3) 0;
	[unroll]
	for (int z = -VXGI_MAT_BLUR_RADIUS; z <= VXGI_MAT_BLUR_RADIUS; z++)
	{
		[unroll]
		for (int y = -VXGI_MAT_BLUR_RADIUS; y <= VXGI_MAT_BLUR_RADIUS; y++)
		{
			[unroll]
			for (int x = -VXGI_MAT_BLUR_RADIUS; x <= VXGI_MAT_BLUR_RADIUS; x++)
			{
				float w = exp(-(float) (x * x + y * y + z * z) * inv2s2);
				float4 s = grid_mat_src.Load(int4(clamp(ip + int3(x, y, z), (int3) 0, (int3) Rmax), 0));
				asum += w * s.a;
				ca_sum += w * s.a * s.rgb;
				wsum += w;
			}
		}
	}

	float a = asum / wsum;
	float3 albedo = ca_sum / max(asum, 1e-4f);
	grid_out[id] = float4(albedo, a);
}
