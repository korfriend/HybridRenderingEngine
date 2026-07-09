#include "../CommonShader.hlsl"

// -----------------------------------------------------------------------------
// VXGI v1 - Stage 1 : Voxelize the DVR volume into the radiance/opacity grid.
// The grid box covers the volume box PLUS an empty margin shell (8 voxels per
// side): grid coord = volume ts * vox_fit_scale + vox_fit_offset. We sample the
// volume intensity at the mapped coord, run it through the same OTF LUT mapping
// DvrCS uses (voxels outside the volume stay empty), and store:
//     rgb = albedo (OTF color, un-associated)   a = opacity [0,1]
// (Radiance replaces rgb later in the InjectLight pass.)
// -----------------------------------------------------------------------------

Texture3D tex3D_volume : register(t0); // normalized intensity volume
Buffer<float4> buf_otf : register(t3); // unorm OTF LUT (rgb=color, a=opacity)

RWTexture3D<float4> vxgi_grid : register(u0); // R16G16B16A16_FLOAT radiance/opacity grid

[numthreads(8, 8, 8)]
void VXGI_VoxelizeVolume(uint3 id : SV_DispatchThreadID)
{
	uint R = g_cbVxgi.grid_res;
	if (R == 0)
		return;
	if (id.x >= R || id.y >= R || id.z >= R)
		return;

	// The grid box = volume box + an empty MARGIN shell (see vox_fit_scale/offset): convert this voxel's
	// grid coord to the VOLUME texture coord and leave voxels outside the volume empty, so boundary
	// diffusion / cone marches have room instead of clamping at the grid edge.
	const float3 uvc_grid = (float3(id) + 0.5f) / (float) R;
	const float3 tsc = (uvc_grid - g_cbVxgi.vox_fit_offset) / g_cbVxgi.vox_fit_scale; // voxel center in volume ts
	if (any(tsc < 0.0f) || any(tsc > 1.0f))
	{
		vxgi_grid[id] = (float4) 0; // margin shell: empty
		return;
	}

	// Supersample the volume over the voxel FOOTPRINT (box filter) to anti-alias. A single sample at the
	// voxel center undersamples a higher-res volume and bakes Moire/banding INTO the grid, which no amount of
	// linear grid filtering at sample time can remove. Averaging N^3 sub-samples per voxel fixes it (this
	// pass is scene-gated, so the cost is paid only on rebuild). Sub-samples are taken in VOLUME ts space.
	const int SS = 4;
	const float inv_ss = 1.0f / (float) SS;
	const float ts_cell = (1.0f / (float) R) / g_cbVxgi.vox_fit_scale; // one grid voxel, in volume ts units
	const float3 ts_min = tsc - 0.5f * ts_cell;                       // voxel min corner in volume ts
	float s = 0.0f;
	[unroll] for (int sz = 0; sz < SS; sz++)
	[unroll] for (int sy = 0; sy < SS; sy++)
	[unroll] for (int sx = 0; sx < SS; sx++)
	{
		float3 off = (float3(sx, sy, sz) + 0.5f) * inv_ss; // sub-cell center in [0,1)
		s += tex3D_volume.SampleLevel(g_samplerLinear_clamp, ts_min + off * ts_cell, 0).r;
	}
	s *= inv_ss * inv_ss * inv_ss; // box average -> anti-aliased normalized intensity

	// Intensity -> OTF index, mirroring DvrCS (sample_v * g_cbTmap.tmap_size_x).
	int otf_idx = (int) (s * g_cbTmap.tmap_size_x);
	otf_idx = clamp(otf_idx, 0, (int) g_cbTmap.tmap_size_x - 1);
	float4 otf = buf_otf[otf_idx]; // unorm rgba : rgb = albedo, a = opacity

	vxgi_grid[id] = float4(otf.rgb, otf.a);
}
