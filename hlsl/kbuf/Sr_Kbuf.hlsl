#include "../Sr_Common.hlsl"

#define DEBUG__

bool OverlapTest(const in Fragment f_1, const in Fragment f_2)
{
	float z_front, z_back, zt_front, zt_back;

	if (f_1.z > f_2.z)
	{
		z_front = f_2.z;
		z_back = f_1.z;
		zt_front = f_2.zthick;
		zt_back = f_1.zthick;
	}
	else
	{
		z_front = f_1.z;
		z_back = f_2.z;
		zt_front = f_1.zthick;
		zt_back = f_2.zthick;
	}

	return z_front > z_back - zt_back;
}
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

#if USE_ROV == 1
#if PIXEL_SYNCH == 1
#define TEX2D_COUNTER RasterizerOrderedTexture2D
#else
#define TEX2D_COUNTER RWTexture2D
#endif
TEX2D_COUNTER<uint> fragment_counter : register(u2);
//RWTexture2D<uint> fragment_counter_test : register(u10); // for experiments
RWByteAddressBuffer deep_k_buf : register(u4);

void Fill_kBuffer__TOO_MUCH(const in int2 tex2d_xy, const in uint k_value, const in float4 v_rgba, const in float z_depth, const in float z_thickness)
{
	if (z_depth > FLT_LARGE || v_rgba.a == 0)
		clip(-1);

	uint iv_rgba = ConvertFloat4ToUInt(v_rgba);

	uint bytes_frags_per_pixel = k_value * 4 * 4; // to do : consider the dynamic scheme. (4 bytes unit)
	uint pixel_id = tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x;
	uint addr_base = pixel_id * bytes_frags_per_pixel;
	uint addr_base_max_z = (g_cbCamState.rt_height * g_cbCamState.rt_width) * bytes_frags_per_pixel + pixel_id * 4;

	Fragment f_in;
	f_in.z = z_depth;
	f_in.i_vis = ConvertFloat4ToUInt(v_rgba);
	f_in.zthick = z_thickness;
	f_in.opacity_sum = v_rgba.a;

#ifdef DEBUG__
#define MARK_RED(C)\
	if(C)\
	{\
		fragment_counter[tex2d_xy] = 7777777;\
		return;\
	}
#endif

	// critical section
	// pixel synchronization
	uint frag_cnt = fragment_counter[tex2d_xy]; // ROV metaphor
#ifdef DEBUG__
	if (frag_cnt == 7777777) clip(-1);
#endif
	if (frag_cnt == 0) // clear k_buffer using the counter as a mask
	{
		for (uint k = 0; k < k_value; k++)
		{
			SET_ZEROFRAG(addr_base, k);
		}
	}

	int store_index = -1;
	float new_core_max_z = -1.f;
	int new_core_max_idx = -1;
	Fragment f_coremax = (Fragment)0;
	int core_max_idx = -1;
	bool count_frags = false;
	float second_new_core_max_z = -1.f;
	int second_new_core_max_idx = -1;

	// use frag id instead of float-type depth to avoid asfloat denormalization issue!
	float prev_core_max_z = -1.f; 
	int prev_core_max_idx = -1; 
	if (frag_cnt > 0)
	{
		//prev_core_max_z = asfloat(deep_k_buf.Load(addr_base_max_z));
		prev_core_max_idx = (int)deep_k_buf.Load(addr_base_max_z) - 1;
		Fragment f_prev_core;
		GET_FRAG(f_prev_core, addr_base, prev_core_max_idx);
		prev_core_max_z = f_prev_core.z;
		//MARK_RED(prev_core_max_idx == -1);
	}

#if TAIL_HANDLING == 1
	const uint num_cores = k_value - 1;
#if NO_OVERLAP == 1
	uint further_overlap_idx = -1;
#endif
	Fragment frag_tail;
	if (frag_cnt == k_value)
	{
		GET_FRAG(frag_tail, addr_base, k_value - 1);
	}
	
	if (f_in.z - f_in.zthick > prev_core_max_z 
		&& frag_cnt >= num_cores)
	{
		// update the merging slot
		if (frag_cnt == k_value)
		{
			Fragment_OrderIndependentMerge(f_in, frag_tail);
		}
		else
			count_frags = true;
		store_index = k_value - 1;
	}
	else
#else
	const uint num_cores = k_value;
#endif
	{
		[loop]
		for (uint i = 0; i < num_cores; i++)
		{
			Fragment f_ith;
			GET_FRAG(f_ith, addr_base, i);
			if (f_ith.i_vis == 0) // empty slot
			{
				store_index = i;
				if (prev_core_max_z < f_in.z)
				{
					new_core_max_z = f_in.z;
					new_core_max_idx = i;
				}
#if NO_OVERLAP == 1
				count_frags = f_ith.opacity_sum == 0;
#else
				count_frags = true;
#endif
				core_max_idx = -2;
				break; // finish 
			}
			else //if (f_ith.i_vis != 0) 
			{
#if ZF_HANDLING == 1
				if (OverlapTest(f_ith, f_in))
				{
					// no need for updating deep_k_buf.Store(addr_base_max_z,..)
					Fragment_OrderIndependentMerge(f_in, f_ith);
					store_index = i;
					core_max_idx = -3;
#if NO_OVERLAP == 1
					for (uint j = i + 1; j < num_cores; j++)
					{
						GET_FRAG(f_ith, addr_base, j);
						if (f_ith.i_vis == 0)
							break;

						if (OverlapTest(f_ith, f_in))
						{
							Fragment_OrderIndependentMerge(f_in, f_ith);
							further_overlap_idx = j;
						}
					}
#endif
					break; // finish
				}
				else
#endif
				{
					//if (prev_core_max_z == f_ith.z) // note that this does not work! due to f_ith.z/prev_core_max_z recovered by asfloat
					if (prev_core_max_idx == (int)i)
					{
						f_coremax = f_ith;
						core_max_idx = (int)i;
					}
					else if(second_new_core_max_z < f_ith.z)
					{
						second_new_core_max_z = f_ith.z;
						second_new_core_max_idx = i;
					}

					if (i == num_cores - 1)
					{
						// 'replace' routine 
						store_index = core_max_idx;
						if (f_in.z > second_new_core_max_z)
						{
							new_core_max_z = f_in.z;
							new_core_max_idx = core_max_idx;
						}
						else
						{
							new_core_max_z = second_new_core_max_z;
							new_core_max_idx = second_new_core_max_idx;
						}
	
						count_frags = frag_cnt < k_value;
					}
				}
			}
		}
	}

	// memory write section
	if (store_index >= 0)
	{
		SET_FRAG(addr_base, store_index, f_in);

		if (core_max_idx >= 0) // replace
		{
			if (frag_cnt == k_value)
			{
				Fragment_OrderIndependentMerge(f_coremax, frag_tail);
			}
			SET_FRAG(addr_base, k_value - 1, f_coremax);
		}
#if NO_OVERLAP == 1
		if (further_overlap_idx >= 0)
		{
			Fragment f_zero_wildcard = (Fragment)0;
			f_zero_wildcard.opacity_sum = -1.f; // to avoid invalid count
			SET_FRAG(addr_base, further_overlap_idx, f_zero_wildcard);

			//if (further_overlap_idx == new_core_max_idx)
			//{
			//	for (uint i = 0; i < num_cores; i++)
			//	{
			//		Fragment f_ith;
			//		GET_FRAG(f_ith, addr_base, i);
			//}
		}
#endif
	}
	if (new_core_max_z >= 0)
	{
		deep_k_buf.Store(addr_base_max_z, (uint)(new_core_max_idx + 1));
	}
	if (count_frags)
	{
		fragment_counter[tex2d_xy] = frag_cnt + 1;
	}
}

void Fill_kBuffer(const in int2 tex2d_xy, const in uint k_value, const in float4 v_rgba, const in float z_depth, const in float z_thickness)
{
	if (z_depth > FLT_LARGE || v_rgba.a == 0)
		clip(-1);

	uint iv_rgba = ConvertFloat4ToUInt(v_rgba);

	uint bytes_frags_per_pixel = k_value * 4 * 4; // to do : consider the dynamic scheme. (4 bytes unit)
	uint pixel_id = tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x;
	uint addr_base = pixel_id * bytes_frags_per_pixel;

	Fragment f_in;
	f_in.z = z_depth;
	f_in.i_vis = ConvertFloat4ToUInt(v_rgba);
	f_in.zthick = z_thickness;
	f_in.opacity_sum = v_rgba.a;

	// critical section
	// pixel synchronization
	uint frag_cnt = fragment_counter[tex2d_xy.xy];
#ifdef DEBUG__
	if (frag_cnt == 7777777) clip(-1);
#endif

	if (frag_cnt == 0) // clear k_buffer using the counter as a mask
	{
		for (uint k = 0; k < k_value; k++)
		{
			SET_ZEROFRAG(addr_base, k);
		}
	}

	int store_index = -1;
	int core_max_idx = -1;
	bool count_frags = false;
	Fragment f_coremax = (Fragment)0;

	Fragment frag_tail; // unless TAIL_HANDLING, then use this as max core fragment
	if (frag_cnt == k_value)
	{
		GET_FRAG(frag_tail, addr_base, k_value - 1);
	}
	else
	{
		frag_tail.i_vis = 0;
		frag_tail.opacity_sum = 0;
		frag_tail.zthick = 0;
		frag_tail.z = FLT_MAX;
	}

	const uint num_unsorted_cores = k_value - 1;
	if (f_in.z > frag_tail.z - frag_tail.zthick)
	{
		// this routine implies that frag_tail.i_vis > 0 and frag_tail.z is finite
		// update the merging slot
#if TAIL_HANDLING == 0
		if (OverlapTest(f_in, frag_tail))
#endif
#if ZF_HANDLING == 1
		Fragment_OrderIndependentMerge(f_in, frag_tail);
#endif
		store_index = k_value - 1;
	}
	else
	{
		[loop]
		for (uint i = 0; i < num_unsorted_cores; i++)
		{
			Fragment f_ith;
			GET_FRAG(f_ith, addr_base, i);
			if (f_ith.i_vis == 0) // empty slot
			{
				store_index = i;
				core_max_idx = -2;
#if NO_OVERLAP == 1 && ZF_HANDLING == 1
				count_frags = f_ith.opacity_sum == 0;
#else
				count_frags = true;
#endif
				break; // finish
			}
			else //if (f_ith.i_vis != 0)
			{
#if ZF_HANDLING == 1
				if (OverlapTest(f_ith, f_in))
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
#if TAIL_HANDLING == 1
					else
					{
						// f_in to the tail and no to the core
						f_coremax = f_in;
					}

					if (count_frags) // implying frag_cnt < k_value
						frag_tail = f_coremax;
					else
						Fragment_OrderIndependentMerge(frag_tail, f_coremax);
#endif
				}
			}
		}
	}
	// memory write section
#if NO_OVERLAP == 1 && ZF_HANDLING == 1
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
	}

#if TAIL_HANDLING == 1
	if (core_max_idx >= 0) // replace
	{
		SET_FRAG(addr_base, k_value - 1, frag_tail);
	}
#endif

	if (count_frags)
	{
		fragment_counter[tex2d_xy] = frag_cnt + 1;
		// finish synch.. syntax
	}
}

void Fill_kBuffer__MAX_Z_CULLING(const in int2 tex2d_xy, const in uint k_value, const in float4 v_rgba, const in float z_depth, const in float z_thickness)
{
	if (z_depth > FLT_LARGE || v_rgba.a == 0)
		clip(-1);

	uint iv_rgba = ConvertFloat4ToUInt(v_rgba);

	uint bytes_frags_per_pixel = k_value * 4 * 4; // to do : consider the dynamic scheme. (4 bytes unit)
	uint pixel_id = tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x;
	uint addr_base = pixel_id * bytes_frags_per_pixel;

#if FRAG_Z_CULLING==1
	uint addr_base_max_z = (g_cbCamState.rt_height * g_cbCamState.rt_width) * bytes_frags_per_pixel + pixel_id * 4;
	float prev_core_max_z = FLT_MAX;
#endif

	Fragment f_in;
	f_in.z = z_depth;
	f_in.i_vis = ConvertFloat4ToUInt(v_rgba);
	f_in.zthick = z_thickness;
	f_in.opacity_sum = v_rgba.a;

	// critical section
	// pixel synchronization
	uint frag_cnt = fragment_counter[tex2d_xy.xy];
#ifdef DEBUG__
	if (frag_cnt == 7777777) clip(-1);
#endif

#if TAIL_HANDLING==1
	const uint num_cores = k_value - 1;
#else
	const uint num_cores = k_value;
#endif

	if (frag_cnt == 0) // clear k_buffer using the counter as a mask
	{
		for (uint k = 0; k < k_value; k++)
		{
			SET_ZEROFRAG(addr_base, k);
		}
#if FRAG_Z_CULLING==1
		deep_k_buf.Store(addr_base_max_z, asuint(FLT_MAX));
#endif
	}
#if FRAG_Z_CULLING==1
	else if(frag_cnt >= num_cores)
	{
		prev_core_max_z = asfloat(deep_k_buf.Load(addr_base_max_z)); 
		//MARK_RED(prev_core_max_z > 10000.f);
	}
#endif

	int store_index = -1;
	int core_max_idx = -1;
	bool count_frags = false;
	Fragment f_coremax = (Fragment)0;

#if TAIL_HANDLING == 1
	Fragment frag_tail;
	if (frag_cnt == k_value)
	{
		GET_FRAG(frag_tail, addr_base, k_value - 1);
	}
	else
	{
		frag_tail.i_vis = 0;
		frag_tail.opacity_sum = 0;
		frag_tail.zthick = 0;
		frag_tail.z = FLT_MAX;
	}

#if FRAG_Z_CULLING==1
	if (f_in.z >= prev_core_max_z)
	{
		// update the merging slot
		if (frag_cnt == k_value)
			Fragment_OrderIndependentMerge(f_in, frag_tail);
		store_index = k_value - 1;
		//MARK_RED(1);
	}
#else
	if (f_in.z > frag_tail.z - frag_tail.zthick)
	{
		// this routine implies that frag_tail.i_vis > 0 and frag_tail.z is finite
		// update the merging slot
		Fragment_OrderIndependentMerge(f_in, frag_tail);
		store_index = k_value - 1;
	}
#endif
	else
#else
#if FRAG_Z_CULLING==1
	clip(f_in.z < prev_core_max_z);
#endif
#endif
	{
		[loop]
		for (uint i = 0; i < num_cores; i++)
		{
			Fragment f_ith;
			GET_FRAG(f_ith, addr_base, i);
			if (f_ith.i_vis == 0) // empty slot
			{
				store_index = i;
				core_max_idx = -2;
#if NO_OVERLAP == 1 && ZF_HANDLING == 1
				count_frags = f_ith.opacity_sum == 0;
#else
				count_frags = true;
#endif
				break; // finish
			}
			else //if (f_ith.i_vis != 0)
			{
#if ZF_HANDLING == 1
				if (OverlapTest(f_ith, f_in))
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

				if (i == num_cores - 1)
				{
					// core_max_idx >= 0
					// replacing or tail merging case 
					count_frags = frag_cnt < k_value;

					if (f_coremax.z > f_in.z) // replace case
					{
						// f_in to the core (and f_coremax to the tail when tail handling mode)
						store_index = core_max_idx;
					}
#if TAIL_HANDLING == 1
#if FRAG_Z_CULLING==0
					else
					{
						// f_in to the tail and no to the core
						f_coremax = f_in;
					}
#endif

					if (count_frags) // implying frag_cnt < k_value
						frag_tail = f_coremax;
					else
						Fragment_OrderIndependentMerge(frag_tail, f_coremax);
#endif
				}
			}
		}
	}
	// memory write section
#if NO_OVERLAP == 1 && ZF_HANDLING == 1
	if (core_max_idx == -3)
	{
		const int overlap_idx = store_index;

		Fragment f_zero_wildcard = (Fragment)0;
		f_zero_wildcard.opacity_sum = -1.f; // to avoid invalid count

		for (int i = overlap_idx + 1; i < (int)num_cores; i++)
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
	}

#if FRAG_Z_CULLING==1
	if (core_max_idx >= 0)
	{
		if (frag_cnt + 1 == num_cores && count_frags)
		{
			deep_k_buf.Store(addr_base_max_z, asuint(f_coremax.z));
		}
		else if(frag_cnt + 1 > num_cores)
		{
			deep_k_buf.Store(addr_base_max_z, asuint(max(f_coremax.z, f_in.z)));
		}
	}
#endif
#if TAIL_HANDLING == 1
	if (core_max_idx >= 0) // replace
	{
		SET_FRAG(addr_base, k_value - 1, frag_tail);
	}
#endif

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
	if (z_depth > FLT_LARGE || v_rgba.a == 0)
	{
		clip(-1);
	}
	uint __dummy;
	uint iv_rgba = ConvertFloat4ToUInt(v_rgba);
	uint addr_base = (tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x) * k_value * 4;

	Fragment f_in;
	f_in.z = z_depth;
	f_in.i_vis = ConvertFloat4ToUInt(v_rgba);
	f_in.zthick = z_thickness;
	f_in.opacity_sum = v_rgba.a;

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
			keep_loop = false;
		}
		else
		{
			uint spin_lock = 0;
			// note that all of fragment_spinlock are initialized as zero
			// use InterlockedCompareExchange(..) instead of InterlockedExchange(fragment_spinlock[tex2d_xy.xy], 1, spin_lock)
			InterlockedCompareExchange(fragment_spinlock[tex2d_xy.xy], 0, 1, spin_lock);

			// for a strict behavior, use InterlockedExchange instead of SET_(ZERO)FRAG
			if (spin_lock == 0)
			{
#endif
				uint frag_cnt = fragment_counter[tex2d_xy.xy];
				if (frag_cnt == 0) // clear mask
				{
					for (uint i = 0; i < k_value; i++)
					{
						SET_ZEROFRAG(addr_base, i); // InterlockedExchange
					}
				}
#if TAIL_HANDLING == 1
				Fragment frag_tail;

				if (frag_cnt == k_value)
				{
					GET_FRAG(frag_tail, addr_base, k_value - 1);
				}
				else
				{
					frag_tail.i_vis = 0;
					frag_tail.opacity_sum = 0;
					frag_tail.zthick = 0;
					frag_tail.z = FLT_MAX;
				}

				if (f_in.z > frag_tail.z - frag_tail.zthick)
				{
					// update the merging slot
					Fragment_OrderIndependentMerge(frag_tail, f_in);
					SET_FRAG(addr_base, k_value - 1, frag_tail);
				}
				else
#endif
				{
					Fragment f_coremax = (Fragment)0;
					int core_max_idx = -1;
					bool count_frags = false;
#if NO_OVERLAP == 1 && ZF_HANDLING == 1
					int merge_idx = -1;
#endif
					[loop]
#if TAIL_HANDLING == 1
					for (uint i = 0; i < k_value - 1; i++)
#else
					for (uint i = 0; i < k_value; i++)
#endif
					{
						Fragment f_ith;
						GET_FRAG(f_ith, addr_base, i);
						if (f_ith.i_vis == 0) // empty slot
						{
							SET_FRAG(addr_base, i, f_in);
							core_max_idx = -3;
							if (f_ith.opacity_sum == 0)
								count_frags = true;
							i = k_value;
						}
						else //if (f_ith.i_vis != 0)
						{
#if ZF_HANDLING == 1
							if (OverlapTest(f_ith, f_in))
							{
#if NO_OVERLAP == 1
								Fragment_OrderIndependentMerge(f_in, f_ith);
								merge_idx = i;
#else
								Fragment_OrderIndependentMerge(f_ith, f_in);
								SET_FRAG(addr_base, i, f_ith);
#endif
								core_max_idx = -2;
								i = k_value;
								//break;
							}
							else
#endif
								if (f_coremax.z < f_ith.z)
								{
									f_coremax = f_ith;
									core_max_idx = i;
								}
						}
					}
#if NO_OVERLAP == 1 && ZF_HANDLING == 1
					if (merge_idx >= 0) // core_max_idx == -2
					{
						Fragment f_zero_wildcard = (Fragment)0;
						f_zero_wildcard.opacity_sum = -1.f; // to avoid invalid count
						[loop]
#if TAIL_HANDLING == 1
						for (i = merge_idx + 1; i < k_value - 1; i++)
#else
						for (i = merge_idx + 1; i < k_value; i++)
#endif
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
						SET_FRAG(addr_base, merge_idx, f_in);
					}
#endif

					if (core_max_idx >= 0)
					{
						if (f_coremax.z > f_in.z) // replace
						{
							SET_FRAG(addr_base, core_max_idx, f_in);
#if TAIL_HANDLING == 1
							if (frag_cnt < k_value)
							{
								frag_tail = f_coremax;
								count_frags = true; // first insertion to tail
							}
							else
							{
								Fragment_OrderIndependentMerge(frag_tail, f_coremax);
							}
							SET_FRAG(addr_base, k_value - 1, frag_tail);
#endif
						}
#if TAIL_HANDLING == 1
						else // merge tail
						{
							if (frag_cnt < k_value)
							{
								frag_tail = f_in;
								count_frags = true; // first insertion to tail
							}
							else
							{
								Fragment_OrderIndependentMerge(frag_tail, f_in);
							}
							SET_FRAG(addr_base, k_value - 1, frag_tail);
						}
#endif
					}
					if (count_frags)
						fragment_counter[tex2d_xy.xy] = frag_cnt + 1;
				}

#if PIXEL_SYNCH == 1
				// always fragment_spinlock[tex2d_xy.xy] is 1, thus use InterlockedExchange instead of InterlockedCompareExchange
				InterlockedExchange(fragment_spinlock[tex2d_xy.xy], 0, spin_lock);
				keep_loop = false;
				//break;
			} // critical section
		}
	} // while for spin-lock
#endif
}
#endif

// SINGLE_LAYER 로 그려진 것을 읽고, outline 그리는 함수
[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void OIT_PRESET(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	int2 tex2d_xy = int2(DTid.xy);

	//float4 v_rgba = sr_fragment_vis[tex2d_xy];
	float depthcs = sr_fragment_zdepth[tex2d_xy];

	float4 v_rgba = (float4)0;
	if (BitCheck(g_cbCamState.cam_flag, 1))
		v_rgba = OutlineTest(tex2d_xy, depthcs, g_cbPobj.depth_forward_bias);
	else
		v_rgba = sr_fragment_vis[tex2d_xy.xy];

	if (BitCheck(g_cbPobj.pobj_flag, 22))
	{
		int out_lined = 0;
		float weight = 0;
		float sum_w = 0;
		for (int i = 0; i < 2; i++)
		{
			float w = GetHotspotMaskWeightIdx(out_lined, tex2d_xy, i, true);
			weight += w * w;
			sum_w += w;
			//if (weight > 0) count++;
		}
		if (sum_w > 0) weight /= sum_w;
		v_rgba *= weight;
	}

	if (v_rgba.a == 0)
		clip(-1);

	float vz_thickness = g_cbPobj.vz_thickness;
	Fill_kBuffer(tex2d_xy, g_cbCamState.k_value, v_rgba, depthcs, vz_thickness);
}

#if __RENDERING_MODE == 2
void OIT_KDEPTH(VS_OUTPUT_TTT input)
#else
void OIT_KDEPTH(VS_OUTPUT input)
#endif
{
	POBJ_PRE_CONTEXT;

	if (g_cbPobj.alpha == 0 || z_depth < 0 || (input.f4PosSS.z / input.f4PosSS.w < 0)
		|| input.f4PosSS.x < 0 || input.f4PosSS.y < 0
		|| (uint)input.f4PosSS.x >= g_cbCamState.rt_width
		|| (uint)input.f4PosSS.y >= g_cbCamState.rt_height)
		clip(-1);

	float4 v_rgba = float4(g_cbPobj.Kd, g_cbPobj.alpha);
	float vz_thickness = g_cbPobj.vz_thickness;

	float3 nor = (float3)0;
	float nor_len = 0;

#if __RENDERING_MODE != 1 && __RENDERING_MODE != 2 && __RENDERING_MODE != 3
	nor = input.f3VecNormalWS;
	nor_len = length(nor);
#endif

#if __RENDERING_MODE == 1
	DashedLine(v_rgba, z_depth, input.f3Custom.x, g_cbPobj.dash_interval, g_cbPobj.pobj_flag & (0x1 << 19), g_cbPobj.pobj_flag & (0x1 << 20));
#elif __RENDERING_MODE == 2
	MultiTextMapping(v_rgba, z_depth, input.f3Custom0.xy, (int)(input.f3Custom0.z + 0.5f), input.f3Custom1, input.f3Custom2);
#elif __RENDERING_MODE == 3
	TextMapping(v_rgba, z_depth, input.f3Custom.xy, g_cbPobj.pobj_flag & (0x1 << 9), g_cbPobj.pobj_flag & (0x1 << 10));
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

	int2 tex2d_xy = int2(input.f4PosSS.xy);
	// dynamic opacity modulation
	if (BitCheck(g_cbPobj.pobj_flag, 22) && nor_len > 0)//&& g_cbPobj.dash_interval != 77.77f)
	{
		//int addr_base = (tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x) * 4;
		//float mask_v = g_bufHotSpotMask[addr_base + 3];
		//if (mask_v >= 0)
		//{
		//	float kappa_t = g_bufHotSpotMask[addr_base + 1];
		//	float kappa_s = g_bufHotSpotMask[addr_base + 2];
		//	//float omega_d = g_bufHotSpotMask[addr_base + 0];
		//	float s = abs(dot(nor / nor_len, view_dir)); // [0, 1]
		//	float a_w = (1.f / kappa_t) * pow(saturate(1.f - s), kappa_s);
		//	v_rgba *= a_w;
		//}
		//else
		//	v_rgba = float4(0, 0, 0, 1);

		// mask value compute
		int out_lined = 0;
		float weight_[2] = { GetHotspotMaskWeightIdx(out_lined, tex2d_xy, 0, false) , GetHotspotMaskWeightIdx(out_lined, tex2d_xy, 1, false) };
		if (out_lined > 0)
		{
			v_rgba = float4(1, 1, 0, 1);
		}
		else
		{
			float kappa_t = 0.f, kappa_s = 0;
			float w = 0;
			for (int i = 0; i < 2; i++)
			{
				if (weight_[i] > 0)
				{
					w += weight_[i];
					kappa_t += g_cbHSMask.mask_info_[i].kappa_t * weight_[i] * weight_[i];
					kappa_s += g_cbHSMask.mask_info_[i].kappa_s * weight_[i] * weight_[i];
				}
			}
			//v_rgba.rgba = float4((float3)0.0, 1);
			if (w > 0)
			{
				kappa_t /= w;
				kappa_s /= w;

				float s = saturate(abs(dot(nor / nor_len, view_dir))); // [0, 1]
				//float a_w = saturate(1.f - kappa_t) * pow(min(max(saturate(1.f - s), 0.01f), 1.f), kappa_s);
				float a_w = (1.f - kappa_t) * pow(1.f - s, kappa_s);
				v_rgba *= saturate(a_w);
				//v_rgba.rgba = float4((float3)max(w, 0.0), 1);
			}
		}/**/
		{ // for Figure
			//float s = saturate(abs(dot(nor / nor_len, view_dir))); // [0, 1]
			//float a_w = (1.f - g_cbHSMask.mask_info_[0].kappa_t) * pow(1.f - s, g_cbHSMask.mask_info_[0].kappa_s);
			//v_rgba *= saturate(a_w);
		}
	}
	//v_rgba.rgba = float4(0.5, 0.5, 0.5, 1);// saturate((z_depth - 500) / 1000);

	// add opacity culling with biased z depth
	// to do : SS-based LAO 에서는 z-culling 안 되도록.
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

#define __TEST__
#ifdef __TEST__
	// test //
	//z_depth -= vz_thickness;
	//vz_thickness = 0.00001;
#endif

	Fill_kBuffer(tex2d_xy, g_cbCamState.k_value, v_rgba, z_depth, vz_thickness);
	//__F(tex2d_xy, g_cbCamState.k_value, v_rgba, z_depth, vz_thickness);
}
