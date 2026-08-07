// -----------------------------------------------------------------------------
// VXGI mip downsample with an OVERLAPPING kernel — the box GenerateMips replacement.
//
// WHY: D3D11 GenerateMips is a 2x2x2 BOX filter, and box kernels of neighbouring
// mip texels do not overlap by a single source texel. A thin (~2-3 voxel) surface
// band therefore lands in each mip texel with a sub-texel PHASE that varies along
// the surface, and the non-overlapping kernels turn that phase directly into a
// VALUE oscillation — contour "ripples" STORED in every generated level (clean at
// LOD 0, visible from LOD 1 up in the debug Load views, modes 1/2/7), then copied
// level to level because each mip boxes the already-rippled previous one. No
// read-side reconstruction can remove a pattern that is in the data; the fix is
// the downsample kernel itself.
//
// KERNEL: separable 6-tap per axis, (1,5,10,10,5,1)/32 — binomial (~Gaussian)
// weights whose neighbouring output texels SHARE 4 source texels per axis. The
// original 4-tap tent (1,3,3,1)/8 (2 shared texels) removed the box mips' gross
// LOD-to-LOD banding but left a residual per-texel checker at mip 1 (debug 7,
// LOD 1) which the obscurance density taps read at mips 2/3/4 and print into the
// AO alpha — the surviving fine band of the final image. The wider support
// attenuates exactly that residual phase, at the price of slightly softer mips.
// Weights are a plain per-channel weighted average — the same linear operator
// class as the box, so the premultiplied-by-coverage channel contract
// (radiance*cov, obsc*cov, vis*cov / MAT coverage) survives unchanged, and
// consumers keep dividing by the SAME-lod MAT coverage (downsampled with this
// same kernel — consistent).
//
// BINDING: one dispatch per level. t0 = a SINGLE-LEVEL SRV of the source mip
// (restricting the view keeps the SRV/UAV subresources disjoint — D3D11 forbids
// overlap), u0 = the destination mip slice. Deliberately self-contained: no CB
// (sizes come from GetDimensions), no sampler (explicit Loads with edge clamp),
// no CommonShader include — this pass has no VXGI semantics, only a filter.
// 6^3 = 216 Loads/texel; a full R=128 cascade is ~300k output texels (~65M
// loads) — BlurMat-class, still fine at the radiance grid's every-advance
// cadence (the mip-1 pass dominates and its loads are cache-coherent).
// -----------------------------------------------------------------------------

Texture3D<float4> src : register(t0);   // source mip, as a one-level view (Load lod 0)
RWTexture3D<float4> dst : register(u0); // destination mip slice

[numthreads(4, 4, 4)]
void VXGI_DownsampleGrid(uint3 id : SV_DispatchThreadID)
{
	uint3 od;
	dst.GetDimensions(od.x, od.y, od.z);
	if (any(id >= od))
		return;
	uint3 sd;
	src.GetDimensions(sd.x, sd.y, sd.z);
	const int3 smax = int3(sd) - 1;

	// Source footprint: texels 2*id-2 .. 2*id+3 per axis (the box pair widened by two on each
	// side), weights (1,5,10,10,5,1)/32 per axis -> outer product / 32768 total. Edge texels
	// clamp — the tail duplicates the border sample, slightly border-biased, and the VXGI grid
	// carries an empty margin shell there by construction. Odd mip sizes (non-pow2 grids
	// floor-halve, same as GenerateMips) keep the same 2x mapping the box uses.
	const float w1d[6] = { 1.0f, 5.0f, 10.0f, 10.0f, 5.0f, 1.0f };
	const int3 base = int3(id) * 2 - 2;

	float4 acc = (float4) 0;
	[loop]
	for (int z = 0; z < 6; z++)
	{
		[unroll]
		for (int y = 0; y < 6; y++)
		{
			[unroll]
			for (int x = 0; x < 6; x++)
			{
				int3 sp = clamp(base + int3(x, y, z), (int3) 0, smax);
				acc += (w1d[x] * w1d[y] * w1d[z]) * src.Load(int4(sp, 0));
			}
		}
	}
	dst[id] = acc * (1.0f / 32768.0f);
}
