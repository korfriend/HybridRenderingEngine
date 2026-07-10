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

	// TRUE-COVERAGE voxelization: apply the OTF PER SUB-SAMPLE and average the resulting opacities, over a
	// 2-voxel footprint. Averaging INTENSITY first and thresholding once (OTF(avg) instead of avg(OTF))
	// re-sharpens the opacity band to ~1 voxel no matter how well the intensity is anti-aliased — the OTF's
	// alpha step undoes the box filter — and that razor-thin band's trilinear field is the maze/stripe
	// interference that debug mode 1 exposes (and every consumer inherits: AO cones, light march, diffusion).
	// Per-sub-sample OTF makes alpha the true occupied FRACTION of the footprint: a surface crossing the
	// voxel yields a smooth ~2-voxel coverage ramp, phase-insensitive by construction. Scene-gated cost only.
	const int SS = 4;
	const float inv_ss = 1.0f / (float) SS;
	const float ts_cell = (1.0f / (float) R) / g_cbVxgi.vox_fit_scale; // one grid voxel, in volume ts units
	const float3 ts_min = tsc - 1.0f * ts_cell;                       // 2-voxel footprint centered on the voxel
	float a_sum = 0.0f;
	float3 rgb_sum = (float3) 0;
	[unroll] for (int sz = 0; sz < SS; sz++)
	[unroll] for (int sy = 0; sy < SS; sy++)
	[unroll] for (int sx = 0; sx < SS; sx++)
	{
		float3 off = (float3(sx, sy, sz) + 0.5f) * inv_ss; // sub-sample center in [0,1)
		float sv = tex3D_volume.SampleLevel(g_samplerLinear_clamp, ts_min + off * (2.0f * ts_cell), 0).r;
		int oi = clamp((int) (sv * g_cbTmap.tmap_size_x), 0, (int) g_cbTmap.tmap_size_x - 1);
		float4 o = buf_otf[oi]; // unorm rgba : rgb = albedo, a = opacity
		a_sum += o.a;
		rgb_sum += o.rgb * o.a; // opacity-weighted albedo (empty sub-samples must not darken the color)
	}
	float alpha = a_sum * (inv_ss * inv_ss * inv_ss); // occupied fraction of the footprint
	float3 albedo = rgb_sum / max(a_sum, 1e-4f);

	vxgi_grid[id] = float4(albedo, alpha);
}
