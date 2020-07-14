#include "Sr_Common.hlsl"


// to do 
// num_deep_layers ==> MAX_LAYERS 로 바꾸기.

#if USE_ROV == 1
//struct FragmentArray2
//{
//	Fragment frags[MAX_LAYERS];
//};

struct FragmentArray
{
	uint i_vis[MAX_LAYERS];
	float z[MAX_LAYERS];
	float zthick[MAX_LAYERS];
	float opacity_sum[MAX_LAYERS];
};
RasterizerOrderedStructuredBuffer<FragmentArray> deep_kf_buf_rov : register(u7);
RWStructuredBuffer<FragmentArray> deep_kf_buf : register(u8);

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
	f_buf.i_vis = ConvertFloat4ToUInt(MixOpt(ConvertUIntToFloat4(f_buf.i_vis), f_buf.opacity_sum, ConvertUIntToFloat4(f_in.i_vis), f_in.opacity_sum));
	f_buf.opacity_sum = f_buf.opacity_sum + f_in.opacity_sum;
	f_buf.z = max(f_buf.z, f_in.z);
	f_buf.zthick = f_buf.z - min(f_buf.z - f_buf.zthick, f_in.z - f_in.zthick);
	return 0;
}

void FillKDepth_v1(const in int2 tex2d_xy, const in int num_deep_layers, const in float4 v_rgba, const in float z_depth, const in float z_thickness)
{
	if (z_depth > 10000000 || v_rgba.a == 0)
		return;

	uint iv_rgba = ConvertFloat4ToUInt(v_rgba);
	uint xy_addr = tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x;

	Fragment f_in;
	f_in.z = z_depth;
	f_in.i_vis = ConvertFloat4ToUInt(v_rgba);
	f_in.zthick = z_thickness;
	f_in.opacity_sum = v_rgba.a;

	// pixel synchronization
	FragmentArray frag_dump = deep_kf_buf_rov[xy_addr];
	Fragment fragArray[MAX_LAYERS];
	for (int k = 0; k < MAX_LAYERS; k++)
	{
		Fragment f;
		f.i_vis = frag_dump.i_vis[k];
		f.z = frag_dump.z[k];
		f.zthick = frag_dump.zthick[k];
		f.opacity_sum = frag_dump.opacity_sum[k];
		fragArray[k] = f;
	}

	Fragment frag_tail = fragArray[MAX_LAYERS - 1];
#if TAIL_HANDLING == 1
	if (f_in.z > frag_tail.z - frag_tail.zthick)
	{
		// update the merging slot
		Fragment_OrderIndependentMerge(frag_tail, f_in);
		fragArray[MAX_LAYERS - 1] = frag_tail;
	}
	else
#endif
	{
		Fragment f_coremax = (Fragment)0;
		int core_max_idx = -1;
		[loop] // [allow_uav_condition]
#if TAIL_HANDLING == 1
		for (int i = 0; i < MAX_LAYERS - 1; i++)
#else
		for (int i = 0; i < MAX_LAYERS; i++)
#endif
		{
			// after atomic store operation
			Fragment f_buf = fragArray[i];
			if (f_buf.i_vis != 0)
			{
#if ZF_HANDLING == 1
				if (OverlapTest(f_buf, f_in))
				{
					Fragment_OrderIndependentMerge(f_buf, f_in);
					fragArray[i] = f_buf;
					core_max_idx = -1;
					break;
				}
				else
#endif
					if (f_coremax.z < f_buf.z)
					{
						f_coremax = f_buf;
						core_max_idx = i;
					}
			}
			else // if (i_vis > 0)
			{
				fragArray[i] = f_in;
				core_max_idx = -1;
				break;
			}
		}

		if (core_max_idx >= 0)
		{
			fragArray[core_max_idx] = f_in;
#if TAIL_HANDLING == 1
			Fragment_OrderIndependentMerge(frag_tail, f_coremax);
			fragArray[MAX_LAYERS - 1] = frag_tail;
#endif
		}
	}

	[unroll]
	for (k = 0; k < MAX_LAYERS; k++)
	{
		Fragment f = fragArray[k];
		frag_dump.i_vis[k] = f.i_vis;
		frag_dump.z[k] = f.z;
		frag_dump.zthick[k] = f.zthick;
		frag_dump.opacity_sum[k] = f.opacity_sum;
		fragArray[k] = f;
	}
	deep_kf_buf_rov[xy_addr] = frag_dump;
}

void FillKDepth_NoInterlock(const in int2 tex2d_xy, const in int num_deep_layers, const in float4 v_rgba, const in float z_depth, const in float z_thickness)
{
	if (z_depth > 10000000 || v_rgba.a == 0)
		return;

	const float ferr_sq = 0.0001f * 0.0001f; // to do : dynamic variable z_thickness
	uint iv_rgba = ConvertFloat4ToUInt(v_rgba);
	uint xy_addr = tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x;

	Fragment f_in;
	f_in.z = z_depth;
	f_in.i_vis = ConvertFloat4ToUInt(v_rgba);
	f_in.zthick = z_thickness;
	f_in.opacity_sum = v_rgba.a;

	// pixel synchronization
	FragmentArray frag_dump = deep_kf_buf[xy_addr];
	Fragment fragArray[MAX_LAYERS];
	for (int k = 0; k < MAX_LAYERS; k++)
	{
		Fragment f;
		f.i_vis = frag_dump.i_vis[k];
		f.z = frag_dump.z[k];
		f.zthick = frag_dump.zthick[k];
		f.opacity_sum = frag_dump.opacity_sum[k];
		fragArray[k] = f;
	}

	Fragment frag_tail = fragArray[MAX_LAYERS - 1];
#if TAIL_HANDLING == 1
	if (f_in.z > frag_tail.z - frag_tail.zthick)
	{
		// update the merging slot
		Fragment_OrderIndependentMerge(frag_tail, f_in);
		fragArray[MAX_LAYERS - 1] = frag_tail;
	}
	else
#endif
	{
		Fragment f_coremax = (Fragment)0;
		int core_max_idx = -1;
		[loop] // [allow_uav_condition]
#if TAIL_HANDLING == 1
		for (int i = 0; i < MAX_LAYERS - 1; i++)
#else
		for (int i = 0; i < MAX_LAYERS; i++)
#endif
		{
			// after atomic store operation
			Fragment f_buf = fragArray[i];
			if (f_buf.i_vis != 0)
			{
#if ZF_HANDLING == 1
				if (OverlapTest(f_buf, f_in))
				{
					Fragment_OrderIndependentMerge(f_buf, f_in);
					fragArray[i] = f_buf;
					core_max_idx = -1;
					break;
				}
				else
#endif
					if (f_coremax.z < f_buf.z)
					{
						f_coremax = f_buf;
						core_max_idx = i;
					}
			}
			else // if (i_vis > 0)
			{
				fragArray[i] = f_in;
				core_max_idx = -1;
				break;
			}
		}

		if (core_max_idx >= 0)
		{
			fragArray[core_max_idx] = f_in;
#if TAIL_HANDLING == 1
			Fragment_OrderIndependentMerge(frag_tail, f_coremax);
			fragArray[MAX_LAYERS - 1] = frag_tail;
#endif
		}
	}

	for (k = 0; k < MAX_LAYERS; k++)
	{
		Fragment f = fragArray[k];
		frag_dump.i_vis[k] = f.i_vis;
		frag_dump.z[k] = f.z;
		frag_dump.zthick[k] = f.zthick;
		frag_dump.opacity_sum[k] = f.opacity_sum;
		fragArray[k] = f;
	}
	deep_kf_buf[xy_addr] = frag_dump;
}
#else
RWTexture2D<uint> fragment_counter : register(u2);
RWTexture2D<uint> fragment_spinlock : register(u3);
RWBuffer<uint> deep_k_buf : register(u4);

void UpdateTailSlot(const in uint addr_base, const in uint num_deep_layers, const in float4 vis_in, const in float alphaw, const in float zdepth_in, const in float zthick_in)
{
	uint addr_merge_slot = addr_base + 3 * (num_deep_layers - 1);
	uint tc = deep_k_buf[addr_merge_slot + 2];
	float A_sum_prev = (tc >> 16) / 255.f;
	uint i_V_sum, i_depth, i_tc;
	uint __dummy;
	if (A_sum_prev == 0)
	{
		i_V_sum = ConvertFloat4ToUInt(vis_in);
		i_depth = asuint(zdepth_in);
		i_tc = f32tof16(zthick_in) | (uint(alphaw * 255) << 16);
	}
	else
	{
		float4 V_mix_prev = ConvertUIntToFloat4(deep_k_buf[addr_merge_slot + 0]);
		float3 C_sum_prev = V_mix_prev.rgb / V_mix_prev.a * A_sum_prev;


		float A_sum = A_sum_prev + alphaw;
		float3 C_sum = C_sum_prev + vis_in.rgb / vis_in.a * alphaw; // consider associated color
		float3 I_sum = C_sum / A_sum;
		float A_mix = 1 - (1 - V_mix_prev.a) * (1.f - vis_in.a);

		i_V_sum = ConvertFloat4ToUInt(float4(I_sum * A_mix, A_mix));
		float zdepth_prev_merge = asfloat(deep_k_buf[addr_merge_slot + 1]);
		float zthick_prev_merge = f16tof32(tc);// &0xFFFF);
		float new_back_z = zdepth_in, new_front_z = zthick_in;
		if (zthick_prev_merge > 0)
		{
			new_back_z = max(zdepth_in, zdepth_prev_merge);
			new_front_z = min(zdepth_in - zthick_in, zdepth_prev_merge - zthick_prev_merge);
		}
		i_depth = asuint(new_back_z);
		//i_thick = asuint(new_back_z - new_front_z);
		i_tc = f32tof16(new_back_z - new_front_z) | (min(65535, uint(A_sum * 255.f)) << 16);
	}

	__InterlockedExchange(deep_k_buf[addr_merge_slot + 0], i_V_sum, __dummy);
	__InterlockedExchange(deep_k_buf[addr_merge_slot + 1], i_depth, __dummy);
	__InterlockedExchange(deep_k_buf[addr_merge_slot + 2], i_tc, __dummy);
}

bool OverlapTest(const in RaySegment2 f_1, const in RaySegment2 f_2)
{
	float z_front, z_back, zt_front, zt_back;

	if (f_1.zdepth > f_2.zdepth)
	{
		z_front = f_2.zdepth;
		z_back = f_1.zdepth;
		zt_front = f_2.zthick;
		zt_back = f_1.zthick;
	}
	else
	{
		z_front = f_1.zdepth;
		z_back = f_2.zdepth;
		zt_front = f_1.zthick;
		zt_back = f_2.zthick;
	}

	return z_front > z_back - zt_back;
}

int Fragment_OrderIndependentMerge(inout RaySegment2 f_buf, const in RaySegment2 f_in)
{
	f_buf.fvis = MixOpt(f_buf.fvis, f_buf.alphaw, f_in.fvis, f_in.alphaw);
	f_buf.alphaw = f_buf.alphaw + f_in.alphaw;
	f_buf.zdepth = max(f_buf.zdepth, f_in.zdepth);
	f_buf.zthick = f_buf.zdepth - min(f_buf.zdepth - f_buf.zthick, f_in.zdepth - f_in.zthick);
	return 0;
}

void FillKDepth_v1(const in int2 tex2d_xy, const in int num_deep_layers, const in float4 v_rgba, const in float z_depth, const in float z_thickness)
{
	if (z_depth > 10000000 || v_rgba.a == 0)
		return;

	//uint frag_cnt = fragment_counter[tex2d_xy.xy]; // asynch 가 되는 경우 queue heap 에 저장이 안 될 수 있는데, 이 정도는 허용 가능 동작
	//if (frag_cnt == 77777)
	//	return;

	const float ferr_sq = 0.0001f * 0.0001f; // to do : dynamic variable z_thickness
	uint iv_rgba = ConvertFloat4ToUInt(v_rgba);
	int addr_base = (tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x) * num_deep_layers * 3;

	uint __dummy;

	// atomic routine (we are using spin-lock scheme)
	uint safe_unlock_count = 0;
	bool keep_loop = true;
	//break; ==> do not use this when using allow_uav_condition *** very important!
	[allow_uav_condition][loop]
	while (keep_loop)
	{
		if (++safe_unlock_count > 10000)// && frag_cnt >= (uint) num_deep_layers)//g_cbPobj.num_safe_loopexit)
		{
			//__InterlockedExchange(fragment_counter[tex2d_xy.xy], 77777, __dummy);
			//InterlockedCompareExchange(fragment_counter[tex2d_xy.xy], 1, 77777, __dummy);
			keep_loop = false;
			//break;
		}
		else
		{
			//if (frag_cnt < (uint) num_deep_layers) // deprecated //
			//	InterlockedAdd(fragment_counter[tex2d_xy.xy], 1, frag_cnt);

			uint spin_lock = 0;
			// note that all of fragment_spinlock are initialized as zero
			//InterlockedExchange(fragment_spinlock[tex2d_xy.xy], 1, spin_lock);
			InterlockedCompareExchange(fragment_spinlock[tex2d_xy.xy], 0, 1, spin_lock);
			if (spin_lock == 0)
			{
				uint frag_cnt = fragment_counter[tex2d_xy.xy];
				if (frag_cnt == 0) // clear mask
				{
					for (int i = 0; i < num_deep_layers; i++)
					{
						__InterlockedExchange(deep_k_buf[addr_base + i * 3 + 0], 0, __dummy);
						__InterlockedExchange(deep_k_buf[addr_base + i * 3 + 1], 0, __dummy);
						__InterlockedExchange(deep_k_buf[addr_base + i * 3 + 2], 0, __dummy);
					}
				}
#if TAIL_HANDLING == 1
				int addr_merge_slot = addr_base + 3 * (num_deep_layers - 1);
				float tail_z = asfloat(deep_k_buf[addr_merge_slot + 1]);
				uint tc_tail = deep_k_buf[addr_merge_slot + 2];
				float tail_thickness = f16tof32(tc_tail); //  & 0xFFFF
				if (tail_z == 0)
				{
					tail_z = FLT_MAX;
					tail_thickness = 0;
				}
				float tail_zf = tail_z - tail_thickness;
				if (z_depth > tail_zf)
				{
					// update the merging slot
					UpdateTailSlot(addr_base, num_deep_layers, v_rgba, v_rgba.a, z_depth, z_thickness);
				}
				else
#endif
				{
					RaySegment2 rs_cur;
					rs_cur.zdepth = z_depth;
					rs_cur.fvis = v_rgba;
					rs_cur.zthick = z_thickness;
					rs_cur.alphaw = v_rgba.a;
					int empty_slot = -1;
					int core_max_zidx = -1;
					float core_max_z = -1.f;
					//[allow_uav_condition][loop]
#if TAIL_HANDLING == 1
					for (int i = 0; i < num_deep_layers - 1; i++)
#else
					for (int i = 0; i < num_deep_layers; i++)
#endif
					{
						// after atomic store operation
						uint i_vis_ith = deep_k_buf[addr_base + 3 * i + 0];
						if (i_vis_ith > 0)
						{
							RaySegment2 rs_ith;
							rs_ith.zdepth = asfloat(deep_k_buf[addr_base + 3 * i + 1]);
#if ZF_HANDLING == 1
							uint tc_ith = deep_k_buf[addr_base + 3 * i + 2];
							rs_ith.zthick = f16tof32(tc_ith); //  & 0xFFFF
							rs_ith.alphaw = (tc_ith >> 16) / 255.f;
							rs_ith.fvis = ConvertUIntToFloat4(i_vis_ith);
							if (OverlapTest(rs_ith, rs_cur))
							{
								RaySegment2 f_merge;
								Fragment_OrderIndependentMerge(rs_cur, rs_ith);

								//rs_cur.fvis = float4(0, 1, 0, 1);
								__InterlockedExchange(deep_k_buf[addr_base + 3 * i + 0], ConvertFloat4ToUInt(rs_cur.fvis), __dummy);
								//__InterlockedExchange(deep_k_buf[addr_base + 3 * i + 0], ConvertFloat4ToUInt(float4(0,1,0,1)), __dummy);
								__InterlockedExchange(deep_k_buf[addr_base + 3 * i + 1], asuint(rs_cur.zdepth), __dummy);
								uint tc_cur = (min(uint(rs_cur.alphaw * 255.f), 65535) << 16) | f32tof16(rs_cur.zthick);
								__InterlockedExchange(deep_k_buf[addr_base + 3 * i + 2], tc_cur, __dummy);
								core_max_zidx = -2;
								i = num_deep_layers;
								// spinlock finish routine

								// test for zf counter
								//InterlockedAdd(fragment_counter[tex2d_xy.xy], 1, frag_cnt);
							}
							//else if (rs_ith.zdepth > tail_zf) // check if previous core fragments belong to the tail
							//{
							//	UpdateTailSlot(addr_base, num_deep_layers, rs_ith.fvis, rs_ith.alphaw, rs_ith.zdepth, rs_ith.zthick);
							//	__InterlockedExchange(deep_k_buf[addr_base + i * 3 + 0], 0, __dummy);
							//	__InterlockedExchange(deep_k_buf[addr_base + i * 3 + 1], 0, __dummy);
							//	__InterlockedExchange(deep_k_buf[addr_base + i * 3 + 2], 0, __dummy);
							//	empty_slot = i;
							//}
							else
#endif
								if (core_max_z < rs_ith.zdepth)
								{
									core_max_z = rs_ith.zdepth;
									core_max_zidx = i;
								}
						}
						else // if (i_vis > 0)
							empty_slot = i;
					}
					if (core_max_zidx > -2)
					{
						if (empty_slot >= 0)  // core-filling case
						{
							__InterlockedExchange(deep_k_buf[addr_base + empty_slot * 3 + 0], iv_rgba, __dummy);
							__InterlockedExchange(deep_k_buf[addr_base + empty_slot * 3 + 1], asuint(z_depth), __dummy);
							uint tc_cur = (uint(v_rgba.a * 255) << 16) | f32tof16(z_thickness);
							__InterlockedExchange(deep_k_buf[addr_base + empty_slot * 3 + 2], tc_cur, __dummy);
						}
						else
						{
							if (core_max_z > z_depth)
							{
								int addr_prev_max = addr_base + core_max_zidx * 3;
#if TAIL_HANDLING == 1
								// load
								float4 vis_prev_max = ConvertUIntToFloat4(deep_k_buf[addr_prev_max + 0]);
								float zdepth_prev_max = asfloat(deep_k_buf[addr_prev_max + 1]);
								uint tc_prev_max = deep_k_buf[addr_prev_max + 2];
								float alphaw_prev_max = (tc_prev_max >> 16) / 255.f;
								float zthick_prev_max = f16tof32(tc_prev_max);
#endif
								// replace
								__InterlockedExchange(deep_k_buf[addr_prev_max + 0], iv_rgba, __dummy);
								__InterlockedExchange(deep_k_buf[addr_prev_max + 1], asuint(z_depth), __dummy);
								uint tc_cur = (uint(v_rgba.a * 255) << 16) | f32tof16(z_thickness);
								__InterlockedExchange(deep_k_buf[addr_prev_max + 2], tc_cur, __dummy);
#if TAIL_HANDLING == 1
								// update the merging slot
								UpdateTailSlot(addr_base, num_deep_layers, vis_prev_max, alphaw_prev_max, zdepth_prev_max, zthick_prev_max);
#endif
							}
#if TAIL_HANDLING == 1
							else
							{
								// update the merging slot
								UpdateTailSlot(addr_base, num_deep_layers, v_rgba, v_rgba.a, z_depth, z_thickness);
							}
#endif
						}
					}
				}
				if (frag_cnt < num_deep_layers)
					fragment_counter[tex2d_xy.xy] = frag_cnt + 1;

				// always fragment_spinlock[tex2d_xy.xy] is 1, thus use InterlockedExchange instead of InterlockedCompareExchange
				InterlockedExchange(fragment_spinlock[tex2d_xy.xy], 0, spin_lock);
				//InterlockedCompareExchange(fragment_spinlock[tex2d_xy.xy], 1, 0, spin_lock);
				keep_loop = false;
				//break;
			} // critical section
		}
	} // while for spin-lock
}

void FillKDepth_NoInterlock(const in int2 tex2d_xy, const in int num_deep_layers, const in float4 v_rgba, const in float z_depth, const in float z_thickness)
{
	if (z_depth > 10000000 || v_rgba.a == 0)
		return;

	uint frag_cnt = fragment_counter[tex2d_xy.xy]; // asynch 가 되는 경우 queue heap 에 저장이 안 될 수 있는데, 이 정도는 허용 가능 동작

	const float ferr_sq = 0.0001f * 0.0001f; // to do : dynamic variable z_thickness
	uint iv_rgba = ConvertFloat4ToUInt(v_rgba);
	int addr_base = (tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x) * num_deep_layers * 3;

	uint __dummy;
	if (frag_cnt < (uint) num_deep_layers) // deprecated //
		InterlockedAdd(fragment_counter[tex2d_xy.xy], 1, frag_cnt);

	int addr_merge_slot = addr_base + 3 * (num_deep_layers - 1);
	float tail_z = asfloat(deep_k_buf[addr_merge_slot + 1]);
	if (tail_z == 0)
		tail_z = FLT_MAX;
	uint tc = deep_k_buf[addr_merge_slot + 2];
	float tail_thickness = f16tof32(tc); //  & 0xFFFF

#if TAIL_HANDLING == 1   
	if (z_depth > tail_z - tail_thickness)
	{
		// update the merging slot
		UpdateTailSlot(addr_base, num_deep_layers, v_rgba, v_rgba.a, z_depth, z_thickness);
	}
	else
#endif
	{
		RaySegment2 rs_cur;
		rs_cur.zdepth = z_depth;
		rs_cur.fvis = v_rgba;
		rs_cur.zthick = z_thickness;
		rs_cur.alphaw = v_rgba.a;
		int empty_slot = -1;
		int core_max_zidx = -1;
		float core_max_z = -1.f;
		[allow_uav_condition][loop]
#if TAIL_HANDLING == 1
			for (int i = 0; i < num_deep_layers - 1; i++)
#else
			for (int i = 0; i < num_deep_layers; i++)
#endif
			{
				// after atomic store operation
				uint i_vis_ith = deep_k_buf[addr_base + 3 * i + 0];
				if (i_vis_ith > 0)
				{
					RaySegment2 rs_ith;
					rs_ith.zdepth = asfloat(deep_k_buf[addr_base + 3 * i + 1]);
#if ZF_HANDLING == 1
					uint tc_ith = deep_k_buf[addr_base + 3 * i + 2];
					rs_ith.zthick = f16tof32(tc_ith); //  & 0xFFFF
					rs_ith.alphaw = (tc_ith >> 16) / 255.f;
					rs_ith.fvis = ConvertUIntToFloat4(i_vis_ith);
					if (MergeFragRS_Avr(rs_cur, rs_ith, rs_cur) == 0)
					{
						__InterlockedExchange(deep_k_buf[addr_base + 3 * i + 0], ConvertFloat4ToUInt(rs_cur.fvis), __dummy);
						__InterlockedExchange(deep_k_buf[addr_base + 3 * i + 1], asuint(rs_cur.zdepth), __dummy);
						uint tc_cur = (min(uint(rs_cur.alphaw * 255.f), 65535) << 16) | f32tof16(rs_cur.zthick);
						__InterlockedExchange(deep_k_buf[addr_base + 3 * i + 2], tc_cur, __dummy);
						core_max_zidx = -2;
						i = num_deep_layers;
						// spinlock finish routine
					}
					else
#endif
						if (core_max_z < rs_ith.zdepth)
						{
							core_max_z = rs_ith.zdepth;
							core_max_zidx = i;
						}
				}
				else // if (i_vis > 0)
					empty_slot = i;
			}
		if (core_max_zidx > -2)
		{
			if (empty_slot >= 0)  // core-filling case
			{
				__InterlockedExchange(deep_k_buf[addr_base + empty_slot * 3 + 0], iv_rgba, __dummy);
				__InterlockedExchange(deep_k_buf[addr_base + empty_slot * 3 + 1], asuint(z_depth), __dummy);
				uint tc_cur = (uint(v_rgba.a * 255) << 16) | f32tof16(z_thickness);
				__InterlockedExchange(deep_k_buf[addr_base + empty_slot * 3 + 2], tc_cur, __dummy);
			}
			else // replace the prev max one and then update max z slot
			{
				int addr_prev_max = addr_base + core_max_zidx * 3;
#if TAIL_HANDLING == 1       
				// load
				float4 vis_prev_max = ConvertUIntToFloat4(deep_k_buf[addr_prev_max + 0]);
				float zdepth_prev_max = asfloat(deep_k_buf[addr_prev_max + 1]);
				uint tc_prev_max = deep_k_buf[addr_prev_max + 2];
				float alphaw_prev_max = (tc_prev_max >> 16) / 255.f;
				float zthick_prev_max = f16tof32(tc_prev_max);
#endif       
				// replace
				__InterlockedExchange(deep_k_buf[addr_prev_max + 0], iv_rgba, __dummy);
				__InterlockedExchange(deep_k_buf[addr_prev_max + 1], asuint(z_depth), __dummy);
				uint tc_cur = (uint(v_rgba.a * 255) << 16) | f32tof16(z_thickness);
				__InterlockedExchange(deep_k_buf[addr_prev_max + 2], tc_cur, __dummy);
#if TAIL_HANDLING == 1        
				// update the merging slot
				UpdateTailSlot(addr_base, num_deep_layers, vis_prev_max, alphaw_prev_max, zdepth_prev_max, zthick_prev_max);
#endif
			}
		}
	}
}
#endif

// SINGLE_LAYER 로 그려진 것을 읽고, outline 그리는 함수
[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void OIT_PRESET(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
    int2 tex2d_xy = int2(DTid.xy);
    
    //float4 v_rgba = sr_fragment_vis[tex2d_xy];
    float depthcs = sr_fragment_zdepth[tex2d_xy];
    
    float4 v_rgba = OutlineTest(tex2d_xy, depthcs, g_cbPobj.depth_forward_bias);
    if (v_rgba.a == 0)
        return;
    
    float vz_thickness = g_cbPobj.vz_thickness;
    FillKDepth_NoInterlock(tex2d_xy, g_cbCamState.num_deep_layers, v_rgba, depthcs, vz_thickness);
}

#if __RENDERING_MODE == 2
void OIT_KDEPTH(VS_OUTPUT_TTT input)
#else
void OIT_KDEPTH(VS_OUTPUT input)
#endif
{
    POBJ_PRE_CONTEXT(return)
    
    if (g_cbPobj.alpha == 0)
        return;
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
			return;
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

    // add opacity culling with biased z depth
    // to do : SS-based LAO 에서는 z-culling 안 되도록.
    //if (v_rgba.a > 0.99f)
    //    out_ps.ds_z = input.f4PosSS.z + 0.00f; // to do : bias z
    // FillKDepth_NoInterlock, FillKDepth
    
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
    
    int2 tex2d_xy = int2(input.f4PosSS.xy);    

    FillKDepth_v1(tex2d_xy, g_cbCamState.num_deep_layers, v_rgba, z_depth, vz_thickness);
}
