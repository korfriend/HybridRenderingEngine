// Slicer x-ray image-level post-processing filter + mesh composite (single fused pass).
//
// One compute pass that runs AFTER the last DVR RayCasting pass when the post-filter
// is enabled (g_cbVobj.vobj_flag bit 2 set in the DVR pass). In that mode DvrCS writes
// only the volume-only x-ray color into a dedicated scratch target (gres_fb_xray_vol,
// bound at u2) and the clean volume (slice plane) depth into fragment_zdepth (u3),
// leaving the sorted mesh K-buffer intact.
//
// This pass:
//   1. Applies a GENERAL NxN linear convolution to the volume-only color. The kernel is
//      an arbitrary square mask supplied as a Buffer<float> SRV (row-major, length N*N,
//      N = 2*filter_radius+1). Weights are pre-normalized on the C++ side from a named
//      profile ("SHARPEN", "GAUSSIAN", ...). When use_filter == 0 the color passes through.
//   2. Composites (intermixes) the filtered volume color with the mesh fragments from the
//      K-buffer (deferred from DvrCS), producing the final on-screen color + depth.
//
// This file is slicer-only in practice (BitCheck(g_cbCamState.cam_flag, 10) expected true),
// but the non-slicer offset-table branch is preserved for parity with DvrCS:744-753.
// Compiled with FRAG_MERGING=1 to match the K-buffer layout used by the x-ray DVR
// variants (VR_RAYMAX/MIN/SUM_cs_5_0).

#include "../CommonShader.hlsl"
#include "../macros.hlsl"

#define VR_MAX_LAYERS 10 // matches DvrCS.hlsl

// --- SRV inputs ---
Texture2D<float4> xray_vol_in : register(t0);    // volume-only premultiplied color (= gres_fb_xray_vol)
Buffer<float> filter_mask : register(t1);        // NxN convolution weights (row-major), always bound
Buffer<uint> sr_offsettable_buf : register(t50); // gres_fb_ref_pidx (non-slicer branch only)

// --- UAVs (must match the DVR layout u0..u5) ---
//   u0 : fragment_counter, u1 : deep_dynK_buf, u2 : fragment_vis (final out),
//   u3 : fragment_zdepth (read clean vol depth + write final composited depth),
//   u5 : fragment_zthick (read the volume slab thickness DvrCS wrote in the bit-2 path)
RWTexture2D<uint> fragment_counter : register(u0);
RWByteAddressBuffer deep_dynK_buf : register(u1);
RWTexture2D<unorm float4> fragment_vis : register(u2);
RWTexture2D<float> fragment_zdepth : register(u3);
RWTexture2D<float> fragment_zthick : register(u5);

//==================================================================
// Fused : general NxN linear filter + mesh K-buffer composite
//==================================================================
[numthreads(GRIDSIZE_VR, GRIDSIZE_VR, 1)]
void XrayFilterComposite(uint3 DTid : SV_DispatchThreadID)
{
	if (DTid.x >= g_cbCamState.rt_width || DTid.y >= g_cbCamState.rt_height)
		return;

	int2 xy = int2(DTid.xy);
	uint pixel_id = DTid.y * g_cbCamState.rt_width + DTid.x;

	// K-buffer base address : exactly as DvrCS.hlsl (744-753)
	bool isSlicer = BitCheck(g_cbCamState.cam_flag, 10);
	uint addr_base = 0;
	if (isSlicer)
	{
		addr_base = pixel_id * 3 * 4 * 2; // 3: elements, 4: bytes, 2: k
	}
	else
	{
		if (pixel_id == 0) return;
		uint pixel_offset = sr_offsettable_buf[pixel_id];
		addr_base = pixel_offset * 4 * 2;
	}

	// --- general NxN linear filter (non-separable 2D convolution) ---
	int max_x = (int)g_cbCamState.rt_width - 1;
	int max_y = (int)g_cbCamState.rt_height - 1;
	int radius = g_cbSliceFilter.filter_radius;

	float4 vol;
	if (g_cbSliceFilter.use_filter != 0 && radius > 0)
	{
		float4 acc = (float4)0;
		uint w = 0;
		[loop]
		for (int dy = -radius; dy <= radius; dy++)
		{
			[loop]
			for (int dx = -radius; dx <= radius; dx++)
			{
				int2 p = int2(clamp(xy.x + dx, 0, max_x), clamp(xy.y + dy, 0, max_y));
				acc += xray_vol_in.Load(int3(p, 0)) * filter_mask[w++];
			}
		}
		vol = acc; // weights pre-normalized in C++ (sharpen/gaussian each sum to 1)
	}
	else
	{
		vol = xray_vol_in.Load(int3(xy, 0)); // passthrough
	}
	vol = saturate(vol);

	// --- composite mesh fragments (deferred from DvrCS) ---
	uint num_frags = fragment_counter[xy] & 0xFFF; // mask like DvrCS:717

	Fragment fs[VR_MAX_LAYERS];
	[loop]
	for (uint k = 0; k < num_frags; k++)
	{
		Fragment f;
		GET_FRAG(f, addr_base, k);
		fs[k] = f;
	}
	fs[num_frags] = (Fragment)0;
	fs[num_frags].z = FLT_MAX;

	float vol_depth = fragment_zdepth[xy];

	float4 vis_out = (float4) 0;
	uint idx_dlayer = 0;
	int i = 0;                  // referenced by INTERMIX (i = num_ray_samples)
	int num_ray_samples = 1;    // referenced by INTERMIX
	Fragment f_dly = fs[0];     // referenced by INTERMIX
	float vthick = fragment_zthick[xy]; // volume slab thickness (hits_t.y - hits_t.x) from DvrCS bit-2 path

	INTERMIX(vis_out, idx_dlayer, num_frags, vol, vol_depth, vthick, fs, 1.0);
	REMAINING_MIX(vis_out, idx_dlayer, num_frags, fs);
			
	fragment_vis[xy] = saturate(vis_out);
	fragment_zdepth[xy] = min(vol_depth, fs[0].z); // final composited depth for downstream passes
}
