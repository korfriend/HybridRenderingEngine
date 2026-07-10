#include "../CommonShader.hlsl"

// -----------------------------------------------------------------------------
// VXGI v3 - Stage 2 : Inject the light REACHING each voxel through the medium.
// Volumetric model (no surface normals): march a narrow cone from the voxel
// TOWARD the light through the material opacity (MAT has mips -> LOD-stepped,
// cheap full-range occlusion) and store
//     direct = albedo * light_tint * transmittance
// into the DIRECT grid. Voxels on the lit side are bright, deeply buried ones
// are dark — the VXGI_Propagate diffusion then spreads this field inward,
// frame by frame, producing progressive translucent scattering.
// -----------------------------------------------------------------------------

Texture3D grid_mat : register(t9);              // rgb = albedo, a = opacity (MIP CHAIN)
RWTexture3D<float4> grid_direct : register(u0); // rgb = arriving direct light, a = per-voxel obscurance

[numthreads(8, 8, 8)]
void VXGI_InjectLight(uint3 id : SV_DispatchThreadID)
{
	uint R = g_cbVxgi.grid_res;
	if (R == 0)
		return;
	if (id.x >= R || id.y >= R || id.z >= R)
		return;

	float4 mat = grid_mat.Load(int4(id, 0));
	if (mat.a <= 0.0f)
	{
		grid_direct[id] = (float4) 0; // empty voxel
		return;
	}

	// --- direction toward the light, in voxel space ---
	float3 uv = (float3(id) + 0.5f) / (float) R;
	float3 L;
	if (g_cbEnv.env_flag & 0x1)
	{
		float3 lp_vox = TransformPoint(g_cbEnv.pos_light_ws, g_cbVxgi.mat_ws2vox);
		L = normalize(lp_vox - uv);
	}
	else
	{
		L = -normalize(TransformVector(g_cbEnv.dir_light_ws, g_cbVxgi.mat_ws2vox));
	}

	// --- narrow occlusion cone toward the light through the material (LOD-stepped, full range) ---
	float voxel = 1.0f / (float) R;
	float tan_half = 0.1f; // narrow shaft
	float occ = 0.0f;
	float dist = 1.5f * voxel;
	[loop]
	for (int i = 0; i < 24; i++)
	{
		if (dist >= g_cbVxgi.max_trace_dist || occ >= 0.99f)
			break;
		float3 p = uv + L * dist;
		if (any(p < 0.0f) || any(p > 1.0f))
			break;
		float diam = max(voxel, 2.0f * tan_half * dist);
		float lod = clamp(log2(diam * (float) R), 0.0f, 5.0f);
		occ += (1.0f - occ) * grid_mat.SampleLevel(g_samplerLinear_clamp, p, lod).a;
		dist += diam * 0.5f; // geometric growth -> full range in ~20 steps
	}
	float T = 1.0f - occ; // transmittance: lit side ~1, deeply buried ~0

	// alpha = per-voxel OBSCURANCE (shared VXGI_Obscurance — this seeds the radiance grid's alpha at
	// bounce 0 via the DIRECT->grid copy, so per-sample AO is valid before the first diffusion step).
	float d1 = grid_mat.SampleLevel(g_samplerLinear_clamp, uv, 1).a;
	float d2 = grid_mat.SampleLevel(g_samplerLinear_clamp, uv, 2).a;
	float d3 = grid_mat.SampleLevel(g_samplerLinear_clamp, uv, 3).a;
	float obscurance = VXGI_Obscurance(d1, d2, d3);

	grid_direct[id] = float4(mat.rgb * g_cbEnv.ltint_diffuse.rgb * T, obscurance);
}
