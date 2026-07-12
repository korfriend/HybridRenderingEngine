#include "../CommonShader.hlsl"

// -----------------------------------------------------------------------------
// VXGI - OPTIONAL obscurance blur. Runs ONCE per rebuild, right after
// InjectLight (dev knob _bool_VxgiAoBlur via vzm::SetRenderTestParam, def ON).
// NORMALIZED-convolution gaussian: aoBlur = G(A*C) / G(C), re-premultiplied by
// the center coverage — a true coverage-weighted smoothing (empty voxels
// cannot drag shell values down), consistent with consumers that
// un-premultiply by the (unblurred, mip-filtered) MAT coverage.
// Purpose: mop up residual band energy the cubic-B-spline density taps cannot
// see (e.g. CT slice periodicity already printed into MAT mip 0). The small
// fixed kernel is complementary to the cubic taps, NOT a replacement: the
// cubic kernel is scale-matched per mip (kills the 4/8/16-voxel reconstruction
// bands before the nonlinear remap), this pass cleans fine residue after it.
//
// SEPARABLE 3-pass ((1,2,1)/4 per axis == (1,2,1)^3/64 in 3D, for BOTH the
// numerator G(A*C) and the denominator G(C); the division runs once, in the
// final pass, after both are fully 3D-blurred). ~14 loads/voxel total vs 56
// for the single-pass 3^3 version, with axis-coherent cache access.
// NOT spread across frames on purpose: the alpha path has no temporal
// accumulator (Propagate passes it through verbatim, and every drag frame
// re-bakes DIRECT), so a partial blur state would display as axis-anisotropic
// AO flicker. All three passes run within one rebuild.
//   X: DIRECT(t10).a, MAT(t9).a          -> PING.rg  (x-blurred A*C, C)
//   Y: PING(t8).rg                       -> BLUR.rg  (xy-blurred)
//   Z: BLUR(t8).rg + DIRECT.rgb + MAT.a  -> PING     (rgb passthrough, final a)
// then the C++ copies PING back onto DIRECT.
// -----------------------------------------------------------------------------

Texture3D chain_src : register(t8);          // pass Y: PING.rg / pass Z: BLUR.rg (partial sums)
Texture3D grid_mat : register(t9);           // MAT: a = coverage
Texture3D grid_direct_src : register(t10);   // DIRECT: rgb = arriving light, a = obscurance*coverage

RWTexture3D<float4> out_rgba : register(u0); // pass X/Z target (PING)
RWTexture3D<float2> out_rg : register(u1);   // pass Y target (BLUR scratch, R16G16F)

#define BLUR_W0 0.25f // (1,2,1)/4 per axis
#define BLUR_W1 0.5f

bool blur_bounds(const in uint3 id, out int R_out)
{
	uint R = g_cbVxgi.grid_res;
	R_out = (int) R;
	return (R != 0) && (id.x < R) && (id.y < R) && (id.z < R);
}

[numthreads(8, 8, 8)]
void VXGI_BlurObscuranceX(uint3 id : SV_DispatchThreadID)
{
	int R;
	if (!blur_bounds(id, R))
		return;
	int3 p = int3(id);
	int3 p0 = int3(max(p.x - 1, 0), p.y, p.z);
	int3 p1 = int3(min(p.x + 1, R - 1), p.y, p.z);
	float2 acc = BLUR_W1 * float2(grid_direct_src.Load(int4(p, 0)).a, grid_mat.Load(int4(p, 0)).a)
	           + BLUR_W0 * float2(grid_direct_src.Load(int4(p0, 0)).a, grid_mat.Load(int4(p0, 0)).a)
	           + BLUR_W0 * float2(grid_direct_src.Load(int4(p1, 0)).a, grid_mat.Load(int4(p1, 0)).a);
	out_rgba[id] = float4(acc, 0.0f, 0.0f);
}

[numthreads(8, 8, 8)]
void VXGI_BlurObscuranceY(uint3 id : SV_DispatchThreadID)
{
	int R;
	if (!blur_bounds(id, R))
		return;
	int3 p = int3(id);
	int3 p0 = int3(p.x, max(p.y - 1, 0), p.z);
	int3 p1 = int3(p.x, min(p.y + 1, R - 1), p.z);
	float2 acc = BLUR_W1 * chain_src.Load(int4(p, 0)).rg
	           + BLUR_W0 * chain_src.Load(int4(p0, 0)).rg
	           + BLUR_W0 * chain_src.Load(int4(p1, 0)).rg;
	out_rg[id] = acc;
}

[numthreads(8, 8, 8)]
void VXGI_BlurObscuranceZ(uint3 id : SV_DispatchThreadID)
{
	int R;
	if (!blur_bounds(id, R))
		return;
	int3 p = int3(id);
	int3 p0 = int3(p.x, p.y, max(p.z - 1, 0));
	int3 p1 = int3(p.x, p.y, min(p.z + 1, R - 1));
	float2 acc = BLUR_W1 * chain_src.Load(int4(p, 0)).rg
	           + BLUR_W0 * chain_src.Load(int4(p0, 0)).rg
	           + BLUR_W0 * chain_src.Load(int4(p1, 0)).rg;
	// acc.x = G(A*C), acc.y = G(C): normalized convolution, re-premultiplied by the CENTER coverage
	// so the stored convention (alpha = obscurance * coverage) holds for every downstream consumer.
	float ao_blur = acc.x / max(acc.y, 1e-4f);
	float cov_center = grid_mat.Load(int4(p, 0)).a;
	float3 direct_rgb = grid_direct_src.Load(int4(p, 0)).rgb;
	out_rgba[id] = float4(direct_rgb, ao_blur * cov_center);
}
