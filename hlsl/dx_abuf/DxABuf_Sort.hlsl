#include "../CommonShader.hlsl"

RWByteAddressBuffer deep_k_buf : register(u1);
RWTexture2D<unorm float4> fragment_blendout : register(u2);
RWTexture2D<float> fragment_zdepth : register(u3);
RWBuffer<uint> offsettable_buf : register(u4); // gres_fb_ref_pidx // only for the count pass
RWByteAddressBuffer deep_ubk_buf : register(u5); // only for the resolve pass

Texture2D<uint> sr_fragment_counter : register(t0);
RWTexture2D<uint> fragment_counter : register(u0);

Texture2D<unorm float4> fragment_rgba_singleLayer : register(t1);
Texture2D<float> fragment_zdepth_singleLayer : register(t2);

#define LOAD1_UBKB(ADDR) deep_ubk_buf.Load((ADDR) * 4)
#define STORE1_KB(V, ADDR) deep_k_buf.Store((ADDR) * 4, V)

///////////////////////////////////////////
// GPU-accelerated A-buffer algorithm
// First pass of the prefix sum creation algorithm.  Converts a 2D buffer to a 1D buffer,
// and sums every other value with the previous value.
//[numthreads(1, 1, 1)]
#if DX_11_STYLE==1
[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void CreatePrefixSum_Pass0_CS(uint3 nGid : SV_GroupID, uint3 nDTid : SV_DispatchThreadID, uint3 nGTid : SV_GroupThreadID)
{
	if (nDTid.x >= g_cbCamState.rt_width || nDTid.y >= g_cbCamState.rt_height)
	    return;
	uint nThreadNum = nDTid.y * g_cbCamState.rt_width + nDTid.x;
	if (nThreadNum % 2 == 0)
	{
		offsettable_buf[nThreadNum] = sr_fragment_counter[nDTid.xy];

		// Add the Fragment count to the next bin
		if ((nThreadNum + 1) < g_cbCamState.rt_width * g_cbCamState.rt_height)
		{
			int2 nextUV;
			nextUV.x = (nThreadNum + 1) % g_cbCamState.rt_width;
			nextUV.y = (nThreadNum + 1) / g_cbCamState.rt_width;
			offsettable_buf[nThreadNum + 1] = offsettable_buf[nThreadNum] + sr_fragment_counter[nextUV];
		}
	}
}
// Second and following passes.  Each pass distributes the sum of the first half of the group
// to the second half of the group.  There are n/groupsize groups in each pass.
// Each pass increases the group size until it is the size of the buffer.
// The resulting buffer holds the prefix sum of all preceding values in each position .
//[numthreads(1, 1, 1)]
[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void CreatePrefixSum_Pass1_CS(uint3 nGid : SV_GroupID, uint3 nDTid : SV_DispatchThreadID, uint3 nGTid : SV_GroupThreadID)
{
	if (nDTid.x >= g_cbCamState.rt_width || nDTid.y >= g_cbCamState.rt_height)
		return;
	int nThreadNum = nDTid.x + nDTid.y * g_cbCamState.rt_width;
	uint g_nPassSize = g_cbCamState.iSrCamDummy__0;
	uint nValue = offsettable_buf[nThreadNum * g_nPassSize + g_nPassSize / 2 - 1];
	for (uint i = nThreadNum * g_nPassSize + g_nPassSize / 2; i < nThreadNum * g_nPassSize + g_nPassSize && i < g_cbCamState.rt_width * g_cbCamState.rt_height; i++)
	{
		offsettable_buf[i] = offsettable_buf[i] + nValue;
	}
}
#else
groupshared int __offset = 0;
//[numthreads(1, 1, 1)] 
[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void CreateOffsetTable_CS(uint3 nGid : SV_GroupID, uint3 nDTid : SV_DispatchThreadID, uint3 nGTid : SV_GroupThreadID)
{
	if (nDTid.x >= g_cbCamState.rt_width || nDTid.y >= g_cbCamState.rt_height)
		return;
	uint nThreadId = nDTid.y * g_cbCamState.rt_width + nDTid.x;
	uint num_frags = sr_fragment_counter[nDTid.xy];
	if (num_frags == 0 
		|| nThreadId == 0 // test
		)
		return;

	uint offset = 0;
	InterlockedAdd(offsettable_buf[0], num_frags, offset);
	//InterlockedAdd(__offset, num_frags, offset);

	offsettable_buf[nThreadId] = offset;
}
#endif

#if DX_11_OIT==0
#include "../macros.hlsl"
#define MAX_ARRAY_SIZE 1024
struct FragmentVD
{
	uint i_vis;
	float z;
};

#if FRAG_MERGING == 0
#define FRAG FragmentVD
#else
#define FRAG Fragment

// these following two functions are the same as in Sr_KBuf.hlsl
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

bool OverlapTest(const in Fragment f_1, const in Fragment f_2)
{
	float diff_z1 = f_1.z - (f_2.z - f_2.zthick);
	float diff_z2 = (f_1.z - f_1.zthick) - f_2.z;
	return diff_z1 * diff_z2 < 0;
}

#endif

#endif

Buffer<uint> sr_offsettable_buf : register(t50); // gres_fb_ref_pidx
//[numthreads(1, 1, 1)]
[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void SortAndRenderCS(uint3 nGid : SV_GroupID, uint3 nDTid : SV_DispatchThreadID, uint3 nGTid : SV_GroupThreadID)
{
	if (nDTid.x >= g_cbCamState.rt_width || nDTid.y >= g_cbCamState.rt_height)
		return;
	uint nThreadNum = nDTid.y * g_cbCamState.rt_width + nDTid.x; // pixel_id
	if (nThreadNum == 0) // we used 0th pixel for temporal synch //
		return;

	//fragment_blendout[nDTid.xy] = fragment_rgba_singleLayer[nDTid.xy];
	//return;
	
	uint N = (uint)fragment_counter[nDTid.xy];
	bool additionalLayers = BitCheck(g_cbCamState.cam_flag, 8);
	if (N == 0 && !additionalLayers)
		return;

#if DX_11_STYLE==0
	uint offset = sr_offsettable_buf[nThreadNum];
	//if (offset == 12)
	//{
	//	fragment_blendout[nDTid.xy] = float4(1, 0, 0, 1);
	//	return;
	//}
	uint num_valid_fs = 0; // for safe check
	FragmentVD fragments[MAX_ARRAY_SIZE];
	[loop]
	for (uint i = 0; i < N; i++)
	{
		FragmentVD f;
		f.i_vis = LOAD1_UBKB(2 * (offset + i) + 0);
		if (f.i_vis >> 24 > 0)
		{
			f.z = asfloat(LOAD1_UBKB(2 * (offset + i) + 1));
			fragments[num_valid_fs++] = f;
		}
	};

	if (additionalLayers)
	{
		FragmentVD f;
		float4 colorRT = fragment_blendout[nDTid.xy];
		if (colorRT.a > 0) {
			f.i_vis = ConvertFloat4ToUInt(colorRT);
			f.z = fragment_zdepth[nDTid.xy];
			fragments[num_valid_fs++] = f;
		}
		colorRT = fragment_rgba_singleLayer[nDTid.xy];
		if (colorRT.a > 0) {
			f.i_vis = ConvertFloat4ToUInt(colorRT);
			f.z = fragment_zdepth_singleLayer[nDTid.xy];
			fragments[num_valid_fs++] = f;
		}
	}

	//if (num_valid_fs == 0) fragment_blendout[nDTid.xy] = float4(1, 1, 1, 1);
	//else if (num_valid_fs == 1) fragment_blendout[nDTid.xy] = float4(1, 0, 0, 1);
	//else if (num_valid_fs == 2) fragment_blendout[nDTid.xy] = float4(0, 1, 0, 1);
	//else if (num_valid_fs == 3) fragment_blendout[nDTid.xy] = float4(0, 0, 1, 1);
	//return;

	N = num_valid_fs;
	if (N == 0)
	{
		// this is the case that the incoming fragment is invalid, e.g., 
		fragment_counter[nDTid.xy] = 0;
		return;
	}

	//const int test_idx = 0;
	//if (N <= test_idx + 1) return;
	//fragment_blendout[nDTid.xy] = ConvertUIntToFloat4(fragments[test_idx].i_vis);
	//return;

	sort(N, fragments, FragmentVD);

	bool store_to_kbuf = BitCheck(g_cbCamState.cam_flag, 3);
	float4 vis_out = (float4) 0.0f;


#define NUM_K 8
	const uint k_value = g_cbCamState.k_value;
	uint bytes_per_frag = 4 * NUM_ELES_PER_FRAG;
	uint bytes_frags_per_pixel = k_value * bytes_per_frag; // to do : consider the dynamic scheme. (4 bytes unit)
	uint addr_base = nThreadNum * bytes_frags_per_pixel;

#if FRAG_MERGING == 1
#if TEST == 1
	store_to_kbuf = true;
#endif
	float merging_beta = asfloat(g_cbCamState.iSrCamDummy__0);
	//Fragment f_tail = (Fragment)0;
	int cnt_stored_fs = 0;
	Fragment f_1, f_2;
	f_1.i_vis = fragments[0].i_vis;
	f_1.z = fragments[0].z;
	f_1.zthick = GetVZThickness(f_1.z, g_cbCamState.cam_vz_thickness);
	//{
	//	if (f_1.zthick < 0)
	//		fragment_blendout[nDTid.xy] = float4(1, 0, 0, 1);
	//	else
	//		fragment_blendout[nDTid.xy] = float4(0, 0, 1, 1);
	//	return;
	//}
	f_1.opacity_sum = ConvertUIntToFloat4(f_1.i_vis).a;
	// use the SFM
	[loop]
	for (i = 0; i < N; i++)
	{
		f_2 = (Fragment)0;
		Fragment f_merge = (Fragment)0;
		uint inext = i + 1;
		if (inext < N)
		{
			f_2.i_vis = fragments[inext].i_vis;
			f_2.z = fragments[inext].z;
			f_2.zthick = GetVZThickness(f_2.z, g_cbCamState.cam_vz_thickness);
			f_2.opacity_sum = ConvertUIntToFloat4(f_2.i_vis).a;

			f_merge = MergeFrags_ver2(f_1, f_2, merging_beta);
		}

		//Fragment f_merge = (Fragment)0;
		//if (OverlapTest(f_1, f_2) && f_2.i_vis != 0)
		//{
		//	f_merge = f_1;
		//	Fragment_OrderIndependentMerge(f_merge, f_2);
		//}

		if (f_merge.i_vis == 0)
		{
			if (cnt_stored_fs < NUM_K - 1)
			{
				if(store_to_kbuf) SET_FRAG(addr_base, cnt_stored_fs, f_1);
				cnt_stored_fs++;

				float4 color = ConvertUIntToFloat4(f_1.i_vis);
				vis_out += color * (1 - vis_out.a);
				f_1 = f_2;
			}
			else
			{
				// tail //
				if (f_2.i_vis != 0)
				{
					float4 f_1_vis = ConvertUIntToFloat4(f_1.i_vis);
					float4 f_2_vis = ConvertUIntToFloat4(f_2.i_vis);
					f_1_vis += f_2_vis * (1.f - f_1_vis.a);
					f_1.i_vis = ConvertFloat4ToUInt(f_1_vis);
					f_1.zthick = f_2.z - f_1.z + f_1.zthick;
					f_1.z = f_2.z;
					f_1.opacity_sum += f_2.opacity_sum;
					//f_tail = f_1;
				}
			}
		}
		else
		{
			f_1 = f_merge;
		}
	}
	if (f_1.i_vis != 0)
	{
		if (store_to_kbuf) SET_FRAG(addr_base, cnt_stored_fs, f_1);
		cnt_stored_fs++;

		float4 vis = ConvertUIntToFloat4(f_1.i_vis);
		vis_out += vis * (1 - vis_out.a);
	}
	if (store_to_kbuf) fragment_counter[nDTid.xy] = cnt_stored_fs;

	if (!store_to_kbuf) {
		fragment_blendout[nDTid.xy] = vis_out;
		fragment_zdepth[nDTid.xy] = fragments[0].z;
	}
#if TEST == 1
	if (g_cbEnv.env_dummy_2 >= 1)
	{
		if (g_cbEnv.env_dummy_2 <= cnt_stored_fs)
		{
			Fragment f_test;
			GET_FRAG(f_test, addr_base, g_cbEnv.env_dummy_2 - 1);
			fragment_blendout[nDTid.xy] = ConvertUIntToFloat4(f_test.i_vis);
		}
		else
			fragment_blendout[nDTid.xy] = (float4)0;
	}
#endif

#else // FRAG_MERGING == 0
	//store_to_kbuf = true;
	bool store_all_onto_dymbuf = BitCheck(g_cbCamState.cam_flag, 4);
	//bool store_rgba_depth = !BitCheck(g_cbCamState.cam_flag, 6);
	uint storing_num_frags = store_all_onto_dymbuf? N : min(N, NUM_K);

	[loop]
	for (i = 0; i < storing_num_frags; i++)
	{
		uint ivis = fragments[i].i_vis;
		float4 color = ConvertUIntToFloat4(ivis);
		vis_out += color * (1 - vis_out.a);

		if (store_to_kbuf)
		{
			if (store_all_onto_dymbuf)
			{
				STORE1_KB(ivis, 2 * (offset + i) + 0);
				STORE1_KB(asuint(fragments[i].z), 2 * (offset + i) + 1);
			}
			else
			{
				STORE1_KB(ivis, addr_base/4 + 2 * i + 0);
				STORE1_KB(asuint(fragments[i].z), addr_base/4 + 2 * i + 1);
			}
		}
	}
	if (store_to_kbuf && !store_all_onto_dymbuf) fragment_counter[nDTid.xy] = storing_num_frags;
	
	if (!store_to_kbuf) {
		fragment_blendout[nDTid.xy] = vis_out;
		fragment_zdepth[nDTid.xy] = fragments[0].z;
	}
#if TEST == 1
	if (g_cbEnv.env_dummy_2 >= 1)
	{
		if (g_cbEnv.env_dummy_2 <= N)
			fragment_blendout[nDTid.xy] = ConvertUIntToFloat4(fragments[g_cbEnv.env_dummy_2 - 1].i_vis);
		else
			fragment_blendout[nDTid.xy] = (float4)0;
	}
#endif
#endif

	//Fragment f;
	//GET_FRAG(f, addr_base, 0);
	//fragment_blendout[nDTid.xy] = float4((float3)f.z / 100, 1);

#else // DX11 style (legacy... too slow)
	nThreadNum += 1; // to reuse nThreadNum - 1

#define blocksize 1
	uint N2 = 1 << (int)(ceil(log2(N)));

#define MAX_SORT 512
	int nIndex[MAX_SORT];
	float fDepth[MAX_SORT];
	for (uint i = 0; i < N; i++)
	{
		nIndex[i] = i;
		fDepth[i] = asfloat(LOAD1_UBKB(2 * (offsettable_buf[nThreadNum - 1] + i) + 1));
	}
	for (i = N; i < N2; i++)
	{
		nIndex[i] = i;
		fDepth[i] = FLT_MAX;
	}

	//if (N > 12)
	//    fragment_blendout[nDTid.xy] = float4(1, 0, 0, 1);//ConvertUIntToFloat4(deep_DxA_buf[2 * (offsettable_buf[nThreadNum - 1] + 1) + 0]);
	//return;

	uint idx = blocksize * nDTid.y + nDTid.x;

	// Bitonic sort
	for (uint k = 2; k <= N2; k = 2 * k)
	{
		for (uint j = k >> 1; j > 0; j = j >> 1)
		{
			for (uint i = 0; i < N2; i++)
			{
				//                GroupMemoryBarrierWithGroupSync();
								//i = idx;

				float di = fDepth[nIndex[i]];
				uint ixj = i ^ j;
				if ((ixj) > i)
				{
					float dixj = fDepth[nIndex[ixj]];
					if ((i & k) == 0 && di > dixj)
					{
						int temp = nIndex[i];
						nIndex[i] = nIndex[ixj];
						nIndex[ixj] = temp;
					}
					if ((i & k) != 0 && di < dixj)
					{
						int temp = nIndex[i];
						nIndex[i] = nIndex[ixj];
						nIndex[ixj] = temp;
					}
				}
			}
		}
	}

	// Output the final result to the frame buffer
	if (idx == 0)
	{

		/*
		   // Debug
		   uint color[8];
		   for(int i = 0; i < 8; i++)
		   {
			   color[i] = deep_DxA_buf[2 * (offsettable_buf[nThreadNum - 1] + i) + 0];
		   }

		   for(i = 0; i < 8; i++)
		   {
			   deep_DxA_buf[2 * (nThreadNum * 8 + i) + 1] = fDepth[i]; //fDepth[nIndex[i]];
			   deep_DxA_buf[2 * (nThreadNum * 8 + i) + 0] = color[nIndex[i]];
		   }
		/**/

		// Accumulate fragments into final result
		float4 result = (float4) 0.0f;
		//for (uint x = N - 1; x >= 0; x--)
		//{
		//    uint bufferValue = deep_DxA_buf[2 * (offsettable_buf[nThreadNum - 1] + nIndex[x]) + 0];
		//    float4 color = ConvertUIntToFloat4(bufferValue);
		//    //result = lerp(result, color, color.a);
		//    //result = result * (1 - color.a) + color * color.a;
		//    result = result * (1 - color.a) + color; // because color is an associated one.
		//}
		for (uint x = 0; x < N; x++)
		{
			//uint bufferValue = deep_DxA_buf[2 * (offsettable_buf[nThreadNum - 1] + nIndex[x]) + 0];
			uint bufferValue = LOAD1_UBKB(2 * (offsettable_buf[nThreadNum - 1] + nIndex[x]) + 0);
			float4 color = ConvertUIntToFloat4(bufferValue);
			result += color * (1 - result.a);
		}
		//fragment_blendout[nDTid.xy] = float4((float3)N / 8, 1);//
		//fragment_blendout[nDTid.xy] = float4((float3)N / 8, 1); //ConvertUIntToFloat4(LOAD1_UBKB(2 * (offsettable_buf[nThreadNum - 1] + nIndex[0]) + 0));
		fragment_blendout[nDTid.xy] = result;
		//fragment_zdepth[nDTid.xy] = asfloat(LOAD1_UBKB(2 * (offsettable_buf[nThreadNum - 1] + nIndex[x - 1]) + 1));
		fragment_zdepth[nDTid.xy] = N; // for checking # of layers
	}
#endif
}