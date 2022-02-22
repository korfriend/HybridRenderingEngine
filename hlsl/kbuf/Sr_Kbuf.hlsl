#include "../Sr_Common.hlsl"
#include "../macros.hlsl"

#ifdef DEBUG__
#define DEBUG_RED(C) {if(C) {fragment_counter[tex2d_xy.xy] = 7777777; return; }}
#define DEBUG_BLUE(C) {if(C) {fragment_counter[tex2d_xy.xy] = 8888888; return; }}
#define DEBUG_GREEN(C) {if(C) {fragment_counter[tex2d_xy.xy] = 9999999; return; }}
#endif

#define GET_ALPHA_F(IVIS) ((float)(IVIS >> 24) / 255.f);

#if FRAG_MERGING == 1
bool OverlapTest(const in Fragment f_1, const in Fragment f_2)
{
	float diff_z1 = f_1.z - (f_2.z - f_2.zthick);
	float diff_z2 = (f_1.z - f_1.zthick) - f_2.z;
	return diff_z1 * diff_z2 < 0;

	//float z_front, z_back, zt_front, zt_back;
	//
	//if (f_1.z > f_2.z)
	//{
	//	z_front = f_2.z;
	//	z_back = f_1.z;
	//	zt_front = f_2.zthick;
	//	zt_back = f_1.zthick;
	//}
	//else
	//{
	//	z_front = f_1.z;
	//	z_back = f_2.z;
	//	zt_front = f_1.zthick;
	//	zt_back = f_2.zthick;
	//}
	//
	//return z_front > z_back - zt_back;
}
#endif

#if FRAG_MERGING == 1
int Fragment_OrderIndependentMerge(inout Fragment f_buf, const in Fragment f_in)
{
	float4 f_buf_fvis = ConvertUIntToFloat4(f_buf.i_vis);
	float4 f_in_fvis = ConvertUIntToFloat4(f_in.i_vis);

	float4 f_mix_vis = MixOpt(f_buf_fvis, f_buf.opacity_sum, f_in_fvis, f_in.opacity_sum);
	f_buf.i_vis = ConvertFloat4ToUInt(f_mix_vis);
	f_buf.opacity_sum = f_buf.opacity_sum + f_in.opacity_sum;
	float z_front = min(f_buf.z - f_buf.zthick, f_in.z - f_in.zthick);
	f_buf.z = max(f_buf.z, f_in.z);
	f_buf.zthick = f_buf.z - z_front;

	return 0;
}
#else
int Fragment_OrderIndependentMergeA(inout Fragment f_buf, inout float opacity_sum, const in Fragment f_in)
{
	float4 f_buf_fvis = ConvertUIntToFloat4(f_buf.i_vis);
	float4 f_in_fvis = ConvertUIntToFloat4(f_in.i_vis);

	// in this case (FRAG_MERGING == 0), only for tail fragment
	// f_in is the tail fragment
	f_buf.z = min(f_buf.z, f_in.z);
	float4 f_mix_vis = MixOpt(f_buf_fvis, opacity_sum, f_in_fvis, f_in_fvis.a);
	f_buf.i_vis = ConvertFloat4ToUInt(f_mix_vis);
	opacity_sum = opacity_sum + f_in_fvis.a;
	return 0;
}

int Fragment_OrderIndependentMergeB(inout Fragment f_buf, const in Fragment f_in, inout float opacity_sum)
{
	float4 f_buf_fvis = ConvertUIntToFloat4(f_buf.i_vis);
	float4 f_in_fvis = ConvertUIntToFloat4(f_in.i_vis);

	// in this case (FRAG_MERGING == 0), only for tail fragment
	// f_in is the tail fragment
	f_buf.z = min(f_buf.z, f_in.z);
	float4 f_mix_vis = MixOpt(f_buf_fvis, f_buf_fvis.a, f_in_fvis, opacity_sum);
	f_buf.i_vis = ConvertFloat4ToUInt(f_mix_vis);
	opacity_sum = opacity_sum + f_buf_fvis.a;
	return 0;
}
#endif

#if DYNAMIC_K_MODE == 1
Buffer<uint> sr_offsettable_buf : register(t50);// gres_fb_ref_pidx
#endif
#if USE_ROV == 1
#if PIXEL_SYNCH == 1
#define TEX2D_COUNTER RasterizerOrderedTexture2D
#else
#define TEX2D_COUNTER RWTexture2D
#endif
TEX2D_COUNTER<uint> fragment_counter : register(u2);
//RWTexture2D<uint> fragment_counter_test : register(u10); // for experiments
RWByteAddressBuffer deep_k_buf : register(u4);

void Fill_kBuffer(const in int2 tex2d_xy, const in uint k_value, const in float4 v_rgba, const in float z_depth, const in float z_thickness)
{
	if (z_depth > FLT_LARGE || v_rgba.a <= 0.01) EXIT;

	uint iv_rgba = ConvertFloat4ToUInt(v_rgba);

	uint bytes_per_frag = 4 * NUM_ELES_PER_FRAG;
	uint pixel_id = tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x;
#if DYNAMIC_K_MODE == 1
	if (pixel_id == 0) EXIT;
	uint pixel_offset = sr_offsettable_buf[pixel_id];
	uint addr_base = pixel_offset * bytes_per_frag;
#else
	uint bytes_frags_per_pixel = k_value * bytes_per_frag;
	uint addr_base = pixel_id * bytes_frags_per_pixel;
#endif

	Fragment f_in;
	f_in.i_vis = ConvertFloat4ToUInt(v_rgba);
	f_in.z = z_depth;
#if FRAG_MERGING == 1
	f_in.zthick = z_thickness;
	f_in.opacity_sum = v_rgba.a;
#endif

#if FRAG_MERGING == 0 && TAIL_HANDLING == 1
	// bound + offset
#if DYNAMIC_K_MODE == 1
	uint addr_tail = (g_cbCamState.rt_width * g_cbCamState.rt_height * k_value * NUM_ELES_PER_FRAG) * 4 + pixel_offset * 4;
#else
	uint addr_tail = (g_cbCamState.rt_width * g_cbCamState.rt_height * k_value * NUM_ELES_PER_FRAG) * 4 + pixel_id * 4;
#endif
#endif

	// critical section
	// pixel synchronization
	// note that the quality is the same as the DFB-Abuffer if k equals max-frags
	// but, the performance is reduced due to this pixel synchronization
	uint frag_cnt = fragment_counter[tex2d_xy.xy];
#ifdef DEBUG__
	if (frag_cnt == 7777777) clip(-1);
#endif

	// only available for static k buffer
	//if (frag_cnt == 0) // clear k_buffer using the counter as a mask
	//{
	//	for (uint k = 0; k < k_value; k++)
	//	{
	//		SET_ZEROFRAG(addr_base, k);
	//	}
	//}

	int store_index = -1;
	int core_max_idx = -1;
	bool count_frags = false;
	Fragment f_coremax = (Fragment)0;

	Fragment frag_tail; // unless TAIL_HANDLING, then use this as max core fragment
#if FRAG_MERGING == 0 && TAIL_HANDLING == 1
	float tail_opacity_sum = 0;
#endif
	if (frag_cnt == k_value)
	{
		GET_FRAG(frag_tail, addr_base, k_value - 1);
#if FRAG_MERGING == 0 && TAIL_HANDLING == 1
		tail_opacity_sum = asfloat(deep_k_buf.Load(addr_tail));
#endif
	}
	else
	{
		frag_tail.i_vis = 0;
		frag_tail.z = FLT_MAX;
#if FRAG_MERGING == 1
		frag_tail.zthick = 0;
		frag_tail.opacity_sum = 0;
#endif
	}

	const uint num_unsorted_cores = k_value - 1;
	
#if FRAG_MERGING == 0
	if (frag_cnt < num_unsorted_cores)
	{
		// filling the unsorted core fragments without checking max core
		store_index = frag_cnt;
		core_max_idx = -4;
		count_frags = true;
	}
	else
#endif
#if FRAG_MERGING == 1
	if (f_in.z > frag_tail.z - frag_tail.zthick)
#else
	if (f_in.z > frag_tail.z)
#endif
	{
		// frag_cnt == k_value case
		// this routine implies that frag_tail.i_vis > 0 and frag_tail.z is finite
		// update the merging slot
#if TAIL_HANDLING == 1
#if FRAG_MERGING == 1
		Fragment_OrderIndependentMerge(f_in, frag_tail);
#else
		Fragment_OrderIndependentMergeB(f_in, frag_tail, tail_opacity_sum);
#endif
		store_index = k_value - 1;
#else
#if FRAG_MERGING == 1
		if (frag_tail.z > f_in.z)
		{
			Fragment_OrderIndependentMerge(f_in, frag_tail);
			store_index = k_value - 1;
		}
#endif
#endif
	}
	else
	{
		[loop]
		for (uint i = 0; i < num_unsorted_cores; i++)
		{
			Fragment f_ith = (Fragment)0;
			if (i < frag_cnt)
				GET_FRAG(f_ith, addr_base, i);

			if (f_ith.i_vis == 0) // empty slot by being 1) merged out or 2) not yet filled
			{
				store_index = i;
				core_max_idx = -2;
#if FURTHER_OVERLAPS == 1 && FRAG_MERGING == 1
				count_frags = f_ith.opacity_sum == 0;
#else
				count_frags = true;
#endif
				break; // finish
			}
			else // if (f_ith.i_vis != 0)
			{
#if FRAG_MERGING == 1
				if (OverlapTest(f_in, f_ith))
				{
					Fragment_OrderIndependentMerge(f_in, f_ith);
					store_index = i;
					core_max_idx = -3;
					break; // finish
				}
				else
#endif
					if (f_coremax.z < f_ith.z)
					{
						f_coremax = f_ith;
						core_max_idx = i;
					}

				if (i == num_unsorted_cores - 1)
				{
					// core_max_idx >= 0
					// replacing or tail merging case 
					count_frags = frag_cnt < k_value;

					if (f_coremax.z > f_in.z) // replace case
					{
						// f_in to the core (and f_coremax to the tail when tail handling mode)
						store_index = core_max_idx;
					}
					else
					{
						// f_in to the tail and no to the core
						f_coremax = f_in;
					}

					if (count_frags) // implying frag_cnt < k_value
					{
						frag_tail = f_coremax;
#if FRAG_MERGING == 0 && TAIL_HANDLING == 1
						tail_opacity_sum = (float)(f_coremax.i_vis >> 24) / 255.f;
#endif
					}
					else
					{
#if TAIL_HANDLING == 1
#if FRAG_MERGING == 1
						Fragment_OrderIndependentMerge(frag_tail, f_coremax);
#else
						Fragment_OrderIndependentMergeA(frag_tail, tail_opacity_sum, f_coremax);
#endif
#else
#if FRAG_MERGING == 1
						if (OverlapTest(frag_tail, f_coremax))
							Fragment_OrderIndependentMerge(frag_tail, f_coremax);
						else 
#endif
						if (f_coremax.z < frag_tail.z)
								frag_tail = f_coremax;
						else 
							core_max_idx = -5;
#endif
					}
				}
			}
		}
	}
	// memory write section
#if FURTHER_OVERLAPS == 1 && FRAG_MERGING == 1
	if (core_max_idx == -3)
	{
		const int overlap_idx = store_index;

		Fragment f_zero_wildcard = (Fragment)0;
		f_zero_wildcard.opacity_sum = -1.f; // to avoid invalid count

		for (int i = overlap_idx + 1; i < (int)num_unsorted_cores; i++)
		{
			Fragment f_ith;
			GET_FRAG(f_ith, addr_base, i);
			if (f_ith.i_vis == 0)
				break;

			if (OverlapTest(f_ith, f_in))
			{
				Fragment_OrderIndependentMerge(f_in, f_ith);
				SET_FRAG(addr_base, i, f_zero_wildcard);
			}
		}
	}
#endif
	if (store_index >= 0)
	{
		SET_FRAG(addr_base, store_index, f_in);

#if FRAG_MERGING == 0 && TAIL_HANDLING == 1
		if(store_index == (int)k_value - 1)
			deep_k_buf.Store(addr_tail, asuint(tail_opacity_sum));
#endif
	}

	if (core_max_idx >= 0) // replace
	{
		SET_FRAG(addr_base, k_value - 1, frag_tail);
#if FRAG_MERGING == 0 && TAIL_HANDLING == 1
		deep_k_buf.Store(addr_tail, asuint(tail_opacity_sum));
#endif
	}

	if (count_frags)
	{
		fragment_counter[tex2d_xy] = frag_cnt + 1;
		// finish synch.. syntax
	}
}
#else
RWTexture2D<uint> fragment_counter : register(u2);
RWTexture2D<uint> fragment_spinlock : register(u3);
//RWBuffer<uint> deep_k_buf : register(u4);
RWByteAddressBuffer deep_k_buf : register(u4);

void Fill_kBuffer(const in int2 tex2d_xy, const in uint k_value, const in float4 v_rgba, const in float z_depth, const in float z_thickness)
{
	if (z_depth > FLT_LARGE || v_rgba.a <= 0.01)
		EXIT;

	uint __dummy;
	uint iv_rgba = ConvertFloat4ToUInt(v_rgba);

	uint bytes_per_frag = 4 * NUM_ELES_PER_FRAG;
	uint pixel_id = tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x;
#if DYNAMIC_K_MODE == 1
	if (pixel_id == 0) EXIT;
	uint pixel_offset = sr_offsettable_buf[pixel_id];
	uint addr_base = pixel_offset * bytes_per_frag;
#else
	uint bytes_frags_per_pixel = k_value * bytes_per_frag;
	uint addr_base = pixel_id * bytes_frags_per_pixel;
#endif

	Fragment f_in;
	f_in.z = z_depth;
	f_in.i_vis = ConvertFloat4ToUInt(v_rgba);
#if FRAG_MERGING == 1
	f_in.zthick = z_thickness;
	f_in.opacity_sum = v_rgba.a;
#endif

#if FRAG_MERGING == 0 && TAIL_HANDLING == 1
	// bound + offset
#if DYNAMIC_K_MODE == 1
	uint addr_tail = (g_cbCamState.rt_width * g_cbCamState.rt_height * k_value * NUM_ELES_PER_FRAG) * 4 + pixel_offset * 4;
#else
	uint addr_tail = (g_cbCamState.rt_width * g_cbCamState.rt_height * k_value * NUM_ELES_PER_FRAG) * 4 + pixel_id * 4;
#endif
#endif

#if STRICT_LOCKED == 1
#define __IES(_ADDR, I, V) deep_k_buf.InterlockedExchange(_ADDR + I * 4, V, __dummy)
#if FRAG_MERGING == 1
#define __SET_ZEROFRAG(ADDR, K) {__IES(ADDR + (K) * NUM_ELES_PER_FRAG * 4, 0, 0); \
__IES(ADDR + (K) * NUM_ELES_PER_FRAG * 4, 1, 0); \
__IES(ADDR + (K) * NUM_ELES_PER_FRAG * 4, 2, 0); \
__IES(ADDR + (K) * NUM_ELES_PER_FRAG * 4, 3, 0);}

#define __SET_FRAG(ADDR, K, F) {__IES(ADDR + (K) * NUM_ELES_PER_FRAG * 4, 0, F.i_vis); \
__IES(ADDR + (K) * NUM_ELES_PER_FRAG * 4, 1, asuint(F.z)); \
__IES(ADDR + (K) * NUM_ELES_PER_FRAG * 4, 2, asuint(F.zthick)); \
__IES(ADDR + (K) * NUM_ELES_PER_FRAG * 4, 3, asuint(F.opacity_sum));}
#else
#define __SET_ZEROFRAG(ADDR, K) {__IES(ADDR + (K) * NUM_ELES_PER_FRAG * 4, 0, 0); \
__IES(ADDR + (K) * NUM_ELES_PER_FRAG * 4, 1, 0);}

#define __SET_FRAG(ADDR, K, F) {__IES(ADDR + (K) * NUM_ELES_PER_FRAG * 4, 0, F.i_vis); \
__IES(ADDR + (K) * NUM_ELES_PER_FRAG * 4, 1, asuint(F.z)); }

#if TAIL_HANDLING == 1
	#define __SET_TAIL(OPA_SUM) __IES(addr_tail, 0, asuint(OPA_SUM))
#endif
#endif

//#define __ADD_COUNT(CNT) { InterlockedAdd(fragment_counter[tex2d_xy], 1, __dummy); }
#define __ADD_COUNT(CNT) { InterlockedExchange(fragment_counter[tex2d_xy], CNT + 1, __dummy); }
#else
	// allowing some synch errors but fast
	// in my test, with STRICT_LOCKED and g_cbPobj.num_safe_loopexit = 5000000, same as the result of ROV mode
#define __SET_FRAG SET_FRAG
#define __SET_ZEROFRAG SET_ZEROFRAG
#define __ADD_COUNT(CNT) { fragment_counter[tex2d_xy] = CNT + 1; }

#if TAIL_HANDLING == 1
#define __SET_TAIL(OPA_SUM) deep_k_buf.Store(addr_tail, asuint(OPA_SUM))
#endif
#endif
#if PIXEL_SYNCH == 1
	// atomic routine (we are using spin-lock scheme)
	uint safe_unlock_count = 0;
	bool keep_loop = true;
	//break; ==> do not use this when using allow_uav_condition
	[loop]
	while (keep_loop)
	{
		if (++safe_unlock_count > g_cbPobj.num_safe_loopexit)
		{
			InterlockedExchange(fragment_spinlock[tex2d_xy.xy], 0, __dummy);
			keep_loop = false;
			clip(-1);
		}
		else
		{
			uint spin_lock = 0;
			// note that all of fragment_spinlocks are initialized as zero
			// use InterlockedCompareExchange(..) instead of InterlockedExchange(fragment_spinlock[tex2d_xy.xy], 1, spin_lock)
			InterlockedCompareExchange(fragment_spinlock[tex2d_xy.xy], 0, 1, spin_lock);
			if (spin_lock == 0)
			{
#endif
				uint frag_cnt = fragment_counter[tex2d_xy.xy];

				int store_index = -1;
				int core_max_idx = -1;
				bool count_frags = false;
				Fragment f_coremax = (Fragment)0;

				Fragment frag_tail; // unless TAIL_HANDLING, then use this as max core fragment
#if FRAG_MERGING == 0 && TAIL_HANDLING == 1
				float tail_opacity_sum = 0;
#endif
				if (frag_cnt == k_value)
				{
					GET_FRAG(frag_tail, addr_base, k_value - 1);
#if FRAG_MERGING == 0 && TAIL_HANDLING == 1
					tail_opacity_sum = asfloat(deep_k_buf.Load(addr_tail));
#endif
				}
				else
				{
					frag_tail.i_vis = 0;
					frag_tail.z = FLT_MAX;
#if FRAG_MERGING == 1
					frag_tail.zthick = 0;
					frag_tail.opacity_sum = 0;
#endif
				}

				const uint num_unsorted_cores = k_value - 1;

#if FRAG_MERGING == 0
				if (frag_cnt < num_unsorted_cores)
				{
					// filling the unsorted core fragments without checking max core
					store_index = frag_cnt;
					core_max_idx = -4;
					count_frags = true;
				}
				else
#endif
#if FRAG_MERGING == 1
				if (f_in.z > frag_tail.z - frag_tail.zthick)
#else
				if (f_in.z > frag_tail.z)
#endif
				{
					// frag_cnt == k_value case
					// this routine implies that frag_tail.i_vis > 0 and frag_tail.z is finite
					// update the merging slot
#if TAIL_HANDLING == 1
#if FRAG_MERGING == 1
					Fragment_OrderIndependentMerge(f_in, frag_tail);
#else
					Fragment_OrderIndependentMergeB(f_in, frag_tail, tail_opacity_sum);
#endif
					store_index = k_value - 1;
#else
#if FRAG_MERGING == 1
					if (frag_tail.z > f_in.z)
					{
						Fragment_OrderIndependentMerge(f_in, frag_tail);
						store_index = k_value - 1;
					}
#endif
#endif
				}
				else
				{
					[loop]
					for (uint i = 0; i < num_unsorted_cores; i++)
					{
						Fragment f_ith = (Fragment)0;
						if (i < frag_cnt)
							GET_FRAG(f_ith, addr_base, i);

						if (f_ith.i_vis == 0) // empty slot by being 1) merged out or 2) not yet filled
						{
							store_index = i;
							core_max_idx = -2;
#if FURTHER_OVERLAPS == 1 && FRAG_MERGING == 1
							count_frags = f_ith.opacity_sum == 0;
#else
							count_frags = true;
#endif
							break; // finish
						}
						else // if (f_ith.i_vis != 0)
						{
#if FRAG_MERGING == 1
							if (OverlapTest(f_in, f_ith))
							{
								Fragment_OrderIndependentMerge(f_in, f_ith);
								store_index = i;
								core_max_idx = -3;
								break; // finish
							}
							else
#endif
								if (f_coremax.z < f_ith.z)
								{
									f_coremax = f_ith;
									core_max_idx = i;
								}

							if (i == num_unsorted_cores - 1)
							{
								// core_max_idx >= 0
								// replacing or tail merging case 
								count_frags = frag_cnt < k_value;

								if (f_coremax.z > f_in.z) // replace case
								{
									// f_in to the core (and f_coremax to the tail when tail handling mode)
									store_index = core_max_idx;
								}
								else
								{
									// f_in to the tail and no to the core
									f_coremax = f_in;
								}

								// f_coremax ==> frag_tail
								if (count_frags) // implying frag_cnt < k_value
								{
									frag_tail = f_coremax;
#if FRAG_MERGING == 0 && TAIL_HANDLING == 1
									tail_opacity_sum = (float)(f_coremax.i_vis >> 24) / 255.f;
#endif
								}
								else
								{
#if TAIL_HANDLING == 1
#if FRAG_MERGING == 1
									Fragment_OrderIndependentMerge(frag_tail, f_coremax);
#else
									Fragment_OrderIndependentMergeA(frag_tail, tail_opacity_sum, f_coremax);
#endif
#else
#if FRAG_MERGING == 1
									if (OverlapTest(frag_tail, f_coremax))
										Fragment_OrderIndependentMerge(frag_tail, f_coremax);
									else
#endif
										if (f_coremax.z < frag_tail.z)
											frag_tail = f_coremax;
										else
											core_max_idx = -5;
#endif
								}
							}
						}
					}
				}

				// memory write section
				if (store_index >= 0)
				{
					__SET_FRAG(addr_base, store_index, f_in);
#if FRAG_MERGING == 0 && TAIL_HANDLING == 1
					if (store_index == (int)k_value - 1)
						__SET_TAIL(tail_opacity_sum);
#endif
				}

				if (core_max_idx >= 0) // replace
				{
					__SET_FRAG(addr_base, k_value - 1, frag_tail);
#if FRAG_MERGING == 0 && TAIL_HANDLING == 1
					__SET_TAIL(tail_opacity_sum);
#endif
				}

				if (count_frags)
				{
					__ADD_COUNT(frag_cnt);
				}
				// finish synch.. syntax
#if PIXEL_SYNCH == 1
				// always fragment_spinlock[tex2d_xy.xy] is 1, thus use InterlockedExchange instead of InterlockedCompareExchange
				InterlockedExchange(fragment_spinlock[tex2d_xy.xy], 0, spin_lock);
				keep_loop = false;
				clip(-1);
			} // critical section
		}
	} // while for spin-lock
#endif
}
#endif

// load the result obtained by SINGLE_LAYER, and draw its outlines
[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void OIT_PRESET(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	if (DTid.x >= g_cbCamState.rt_width || DTid.y >= g_cbCamState.rt_height)
		return;
	int2 tex2d_xy = int2(DTid.xy);

	//float4 v_rgba = sr_fragment_vis[tex2d_xy];
	float depthcs = sr_fragment_zdepth[tex2d_xy];


	float4 v_rgba = (float4)0;
	if (BitCheck(g_cbCamState.cam_flag, 1)) {
		float4 outline_color = ConvertUIntToFloat4(g_cbPobj.pobj_dummy_0);
		v_rgba = OutlineTest(tex2d_xy, depthcs, g_cbPobj.depth_thres, outline_color.rgb, (int)g_cbPobj.pix_thickness);
	}
	else
		v_rgba = sr_fragment_vis[tex2d_xy.xy];
	//if(v_rgba.a == 0)
	//	v_rgba = float4(1, 0, 0, 1);

	if (v_rgba.a <= 0.01)
		return;

	float depth_res = GetVZThickness(depthcs, g_cbPobj.vz_thickness);
	float vz_thickness = depth_res;
	//v_rgba = float4(1, 1, 0, 0.1);
	//v_rgba.a = 0.1;
	Fill_kBuffer(tex2d_xy, g_cbCamState.k_value, v_rgba, depthcs, vz_thickness);
}

void OIT_KDEPTH(__VS_OUT input)
{
	POBJ_PRE_CONTEXT;

	if (g_cbPobj.alpha == 0 || z_depth < 0 || (input.f4PosSS.z / input.f4PosSS.w < 0)
		|| input.f4PosSS.x < 0 || input.f4PosSS.y < 0
		|| (uint)input.f4PosSS.x >= g_cbCamState.rt_width
		|| (uint)input.f4PosSS.y >= g_cbCamState.rt_height)
		clip(-1);

	float4 v_rgba = float4(g_cbPobj.Kd, g_cbPobj.alpha);
	float vz_thickness = GetVZThickness(z_depth, g_cbPobj.vz_thickness);

	float3 nor = (float3)0;
	float nor_len = 0;

#if __RENDERING_MODE != 1 && __RENDERING_MODE != 2 && __RENDERING_MODE != 3
	nor = input.f3VecNormalWS;
	nor_len = length(nor);
#endif

#if __RENDERING_MODE == 1
	if ((g_cbPobj.pobj_flag & (0x1 << 3)) == 0)
		v_rgba.rgb = input.f3Custom;
	int max_c = g_cbPobj.pobj_flag >> 30;
	float line_pos = max_c == 0 ? input.f3PosWS.x : max_c == 1 ? input.f3PosWS.y : input.f3PosWS.z;
	DashedLine(v_rgba, z_depth, line_pos, g_cbPobj.dash_interval, g_cbPobj.pobj_flag & (0x1 << 19), g_cbPobj.pobj_flag & (0x1 << 20));
	if (v_rgba.a <= 0.01) clip(-1);
#elif __RENDERING_MODE == 2
	MultiTextMapping(v_rgba, z_depth, input.f3Custom0.xy, (int)(input.f3Custom0.z + 0.5f), input.f3Custom1, input.f3Custom2);
	if (v_rgba.a <= 0.01) clip(-1);
#elif __RENDERING_MODE == 3
	TextMapping(v_rgba, z_depth, input.f3Custom.xy, g_cbPobj.pobj_flag & (0x1 << 9), g_cbPobj.pobj_flag & (0x1 << 10));
	if (v_rgba.a <= 0.01) clip(-1);
#elif __RENDERING_MODE == 4
	if (g_cbPobj.tex_map_enum == 1)
	{
		float4 clr_map;
		TextureImgMap(clr_map, input.f3Custom);
		if (clr_map.a == 0)
		{
			//v_rgba.rgb = float3(1, 0, 0);
			v_rgba = (float4) 0;
			z_depth = FLT_MAX;
			clip(-1);
		}
		else
		{
			//float3 mat_shading = float3(g_cbEnv.ltint_ambient.w, g_cbEnv.ltint_diffuse.w, g_cbEnv.ltint_spec.w);
			float3 Ka = clr_map.rgb * g_cbEnv.ltint_ambient.rgb;
			float3 Kd = clr_map.rgb * g_cbEnv.ltint_diffuse.rgb;
			float3 Ks = clr_map.rgb * g_cbEnv.ltint_spec.rgb;
			float Ns = g_cbPobj.Ns;
			ComputeColor(v_rgba.rgb, Ka, Kd, Ks, Ns, 1.0, input.f3PosWS, view_dir, nor, nor_len);
			v_rgba.a *= clr_map.a;
		}
		//v_rgba.rgb = float3(1, 0, 0);
	}
	else
	{
		//g_tex2D_Mat_KA.CalculateLevelOfDetail(g_samplerLinear, input.f3Custom.xy);

		float3 Ka = g_cbPobj.Ka, Kd = g_cbPobj.Kd, Ks = g_cbPobj.Ks;
		float Ns = g_cbPobj.Ns, d = 1.0, bump = 1.0;
		TextureMaterialMap(Ka, Kd, Ks, Ns, bump, d, input.f3Custom, g_cbPobj.tex_map_enum);
		if (Ns >= 0)
		{
			Ka *= g_cbEnv.ltint_ambient.rgb;
			Kd *= g_cbEnv.ltint_diffuse.rgb;
			Ks *= g_cbEnv.ltint_spec.rgb;
			ComputeColor(v_rgba.rgb, Ka, Kd, Ks, Ns, bump, input.f3PosWS, view_dir, nor, nor_len);
			//v_rgba.rgb = Ks;
		}
		else // illumination model 0
			v_rgba.rgb = Kd;
		//if(Ns == 77) v_rgba.rgb = float3(1, 0, 0);
		//v_rgba.rgb = (float3)d;
		//if(nor_len > 0)
		//v_rgba.rgb = Ka;// float3(1, 0, 0);
		//else
		if (d <= 0.01) clip(-1);
		v_rgba.a *= d;
	}
#else
	float3 Ka, Kd, Ks;
	float Ns = g_cbPobj.Ns;
	if ((g_cbPobj.pobj_flag & (0x1 << 3)) == 0)
	{
		Ka = input.f3Custom;
		Kd = input.f3Custom;
		Ks = input.f3Custom;
	}
	else
	{
		Ka = g_cbPobj.Ka, Kd = g_cbPobj.Kd, Ks = g_cbPobj.Ks;
	}
	//Ka = Kd = Ks = (float3)1;
	if (nor_len > 0)
	{
		Ka *= g_cbEnv.ltint_ambient.rgb;
		Kd *= g_cbEnv.ltint_diffuse.rgb;
		Ks *= g_cbEnv.ltint_spec.rgb;
		ComputeColor(v_rgba.rgb, Ka, Kd, Ks, Ns, 1.0, input.f3PosWS, view_dir, nor, nor_len);
	}
	else
		v_rgba.rgb = Kd;
	//v_rgba.rgb = float3(1, 1, 0);
#endif

	// make it as an associated color.
	// as a color component is stored into 8 bit channel, the alpha-multiplied precision must be determined in this stage.
	// unless, noise dots appear.
	v_rgba.rgb *= v_rgba.a;
	//v_rgba.rgb = float3(1, 0, 0);

	int2 tex2d_xy = int2(input.f4PosSS.xy);
	// dynamic opacity modulation
	bool is_dynamic_transparency = BitCheck(g_cbPobj.pobj_flag, 22);
	bool is_mask_transparency = BitCheck(g_cbPobj.pobj_flag, 23);
	if (is_dynamic_transparency || is_mask_transparency)
	{
		float mask_weight = 1, dynamic_alpha_weight = 1;
		int out_lined = GhostedEffect(mask_weight, dynamic_alpha_weight, input.f3PosWS, view_dir, nor, nor_len, is_dynamic_transparency);
		if (out_lined > 0)
			v_rgba = float4(1, 1, 0, 1);
		else
		{
			if (is_dynamic_transparency)
				v_rgba.rgba *= dynamic_alpha_weight;
			if (is_mask_transparency)
				v_rgba.rgba *= mask_weight;
			if (v_rgba.a <= 0.01) clip(-1);
		}
	}


	//v_rgba.rgba = float4(0.5, 0.5, 0.5, 1);// saturate((z_depth - 500) / 1000);

	// add opacity culling with biased z depth
	// to do : no z-culling when SS-based LAO
	//if (v_rgba.a > 0.99f)
	//    out_ps.ds_z = input.f4PosSS.z + 0.00f; // to do : bias z
	// FillFill_kBuffer

	// to do with dynamic determination of vz_thickness
#if __RENDERING_MODE != 2
	//if (nor_len > 0)
	//{
	//    float3 u_nor = nor / nor_len;
	//    float rad = abs(dot(u_nor, -view_dir));
	//    float in_angle = min(acos(rad), F_PI / 3.f); // safe value
	//    float cos_v = cos(in_angle);
	//    vz_thickness /= cos_v;
	//}
#endif
	
	Fill_kBuffer(tex2d_xy, g_cbCamState.k_value, v_rgba, z_depth, vz_thickness);
	//__F(tex2d_xy, g_cbCamState.k_value, v_rgba, z_depth, vz_thickness);
}
