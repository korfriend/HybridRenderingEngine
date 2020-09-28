#include "../CommonShader.hlsl"

RWByteAddressBuffer deep_DxA_buf : register(u1);
RWTexture2D<float4> fragment_blendout : register(u2);
RWTexture2D<float> fragment_zdepth : register(u3);
RWBuffer<uint> offsettable_buf : register(u4); // gres_fb_ref_pidx

Texture2D<uint> sr_fragment_counter : register(t0);

#define LOAD1_RBB(ADDR) deep_DxA_buf.Load((ADDR) * 4)
#define STORE1_RBB(V, ADDR) deep_DxA_buf.Store((ADDR) * 4, V)

///////////////////////////////////////////
// GPU-accelerated A-buffer algorithm
// First pass of the prefix sum creation algorithm.  Converts a 2D buffer to a 1D buffer,
// and sums every other value with the previous value.
//[numthreads(1, 1, 1)]
#if DX_11_STYLE==1
[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void CreatePrefixSum_Pass0_CS(uint3 nGid : SV_GroupID, uint3 nDTid : SV_DispatchThreadID, uint3 nGTid : SV_GroupThreadID)
{
	//if (nDTid.x >= g_cbCamState.rt_width || nDTid.y >= g_cbCamState.rt_height)
	//    return;
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
#define MAX_ARRAY_SIZE_1n MAX_ARRAY_SIZE - 1
#define MAX_ARRAY_SIZE_2d MAX_ARRAY_SIZE >> 1
struct FragmentVD
{
	uint ivis;
	float z;
};

//static FragmentVD fragments[MAX_ARRAY_SIZE];
//static FragmentVD leftArray[MAX_ARRAY_SIZE_2d];
#endif

Buffer<uint> sr_offsettable_buf : register(t50); // gres_fb_ref_pidx
//[numthreads(1, 1, 1)]
[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void SortAndRenderCS(uint3 nGid : SV_GroupID, uint3 nDTid : SV_DispatchThreadID, uint3 nGTid : SV_GroupThreadID)
{
	uint nThreadNum = nDTid.y * g_cbCamState.rt_width + nDTid.x; // nDTid
	if (nThreadNum == 0) // we used 0th pixel for temporal synch //
		return;

	int N = (int)sr_fragment_counter[nDTid.xy];
	if (N == 0)
		return;

#if DX_11_STYLE==0
	uint offset = sr_offsettable_buf[nThreadNum];
	//if (offset == 12)
	//{
	//	fragment_blendout[nDTid.xy] = float4(1, 0, 0, 1);
	//	return;
	//}

	FragmentVD fragments[MAX_ARRAY_SIZE];
	for (int i = 0; i < N; i++)
	{
		FragmentVD f;
		f.ivis = LOAD1_RBB(2 * (offset + i) + 0);
		f.z = asfloat(LOAD1_RBB(2 * (offset + i) + 1));
		fragments[i] = f;
	}

	//const int test_idx = 0;
	//if (N <= test_idx + 1) return;
	//fragment_blendout[nDTid.xy] = ConvertUIntToFloat4(fragments[test_idx].ivis);
	//return;

	sort(N, fragments, FragmentVD);

	bool store_to_kbuf = BitCheck(g_cbCamState.cam_flag, 3);
	float4 result = (float4) 0.0f;
	for (i = 0; i < N; i++)
	{
		uint bufferValue = fragments[i].ivis;
		float4 color = ConvertUIntToFloat4(bufferValue);
		result += color * (1 - result.a);

		if (store_to_kbuf)
		{
			STORE1_RBB(bufferValue, 2 * offset + 0);
			STORE1_RBB(asuint(fragments[i].z), 2 * offset + 1);
		}
	}
	fragment_blendout[nDTid.xy] = result;
	fragment_zdepth[nDTid.xy] = fragments[0].z;

#else 
	nThreadNum += 1; // to reuse nThreadNum - 1

#define blocksize 1
	uint N2 = 1 << (int)(ceil(log2(N)));

#define MAX_SORT 400
	int nIndex[MAX_SORT];
	float fDepth[MAX_SORT];
	for (uint i = 0; i < N; i++)
	{
		nIndex[i] = i;
		fDepth[i] = asfloat(LOAD1_RBB(2 * (offsettable_buf[nThreadNum - 1] + i) + 1));
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
			uint bufferValue = LOAD1_RBB(2 * (offsettable_buf[nThreadNum - 1] + nIndex[x]) + 0);
			float4 color = ConvertUIntToFloat4(bufferValue);
			result += color * (1 - result.a);
		}
		//fragment_blendout[nDTid.xy] = float4((float3)N / 8, 1);//
		//fragment_blendout[nDTid.xy] = float4((float3)N / 8, 1); //ConvertUIntToFloat4(LOAD1_RBB(2 * (offsettable_buf[nThreadNum - 1] + nIndex[0]) + 0));
		fragment_blendout[nDTid.xy] = result;
		//fragment_zdepth[nDTid.xy] = asfloat(LOAD1_RBB(2 * (offsettable_buf[nThreadNum - 1] + nIndex[x - 1]) + 1));
		fragment_zdepth[nDTid.xy] = N; // for checking # of layers
	}
#endif
}