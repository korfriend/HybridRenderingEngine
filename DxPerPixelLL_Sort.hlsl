#include "CommonShader.hlsl"

RWByteAddressBuffer deep_LL_buf : register(u1);
RWTexture2D<float4> fragment_blendout : register(u2);
RWTexture2D<float> fragment_zdepth : register(u3);
RWBuffer<uint> prefix_LL_buf : register(u4);

Texture2D<uint> sr_fragment_counter : register(t0);

#define LOAD1_RBB(ADDR) deep_LL_buf.Load((ADDR) * 4)

///////////////////////////////////////////
// GPU-accelerated A-buffer algorithm
// First pass of the prefix sum creation algorithm.  Converts a 2D buffer to a 1D buffer,
// and sums every other value with the previous value.
[numthreads(1, 1, 1)]
void CreatePrefixSum_Pass0_CS(uint3 nGid : SV_GroupID, uint3 nDTid : SV_DispatchThreadID, uint3 nGTid : SV_GroupThreadID)
{
	//if (nDTid.x >= g_cbCamState.rt_width || nDTid.y >= g_cbCamState.rt_height)
	//    return;
	uint nThreadNum = nGid.y * g_cbCamState.rt_width + nGid.x;
	if (nThreadNum % 2 == 0)
	{
		prefix_LL_buf[nThreadNum] = sr_fragment_counter[nGid.xy];

		// Add the Fragment count to the next bin
		if ((nThreadNum + 1) < g_cbCamState.rt_width * g_cbCamState.rt_height)
		{
			int2 nextUV;
			nextUV.x = (nThreadNum + 1) % g_cbCamState.rt_width;
			nextUV.y = (nThreadNum + 1) / g_cbCamState.rt_width;
			prefix_LL_buf[nThreadNum + 1] = prefix_LL_buf[nThreadNum] + sr_fragment_counter[nextUV];
		}
	}
}
// Second and following passes.  Each pass distributes the sum of the first half of the group
// to the second half of the group.  There are n/groupsize groups in each pass.
// Each pass increases the group size until it is the size of the buffer.
// The resulting buffer holds the prefix sum of all preceding values in each position .
[numthreads(1, 1, 1)]
void CreatePrefixSum_Pass1_CS(uint3 nGid : SV_GroupID, uint3 nDTid : SV_DispatchThreadID, uint3 nGTid : SV_GroupThreadID)
{
	int nThreadNum = nGid.x + nGid.y * g_cbCamState.rt_width;
	uint g_nPassSize = g_cbCamState.iSrCamDummy__0;
	uint nValue = prefix_LL_buf[nThreadNum * g_nPassSize + g_nPassSize / 2 - 1];
	for (uint i = nThreadNum * g_nPassSize + g_nPassSize / 2; i < nThreadNum * g_nPassSize + g_nPassSize && i < g_cbCamState.rt_width * g_cbCamState.rt_height; i++)
	{
		prefix_LL_buf[i] = prefix_LL_buf[i] + nValue;
	}
}

#define blocksize 1
[numthreads(1, 1, 1)]
void SortAndRenderCS(uint3 nGid : SV_GroupID, uint3 nDTid : SV_DispatchThreadID, uint3 nGTid : SV_GroupThreadID)
{
	uint nThreadNum = nGid.y * g_cbCamState.rt_width + nGid.x; // nDTid

//    uint r0, r1, r2;
//    float rd0, rd1, rd2, rd3, rd4, rd5, rd6, rd7;
#define MAX_SORT 400
	int nIndex[MAX_SORT];
	uint N = sr_fragment_counter[nDTid.xy];

	//if (N > 20)
	//{
	//	fragment_blendout[nGid.xy] = float4(1, 0, 0, 1);
	//	return;
	//}

	uint N2 = 1 << (int)(ceil(log2(N)));

	float fDepth[MAX_SORT];
	for (uint i = 0; i < N; i++)
	{
		nIndex[i] = i;
		//fDepth[i] = asfloat(deep_LL_buf[2 * (prefix_LL_buf[nThreadNum - 1] + i) + 1]);
		fDepth[i] = asfloat(LOAD1_RBB(2 * (prefix_LL_buf[nThreadNum - 1] + i) + 1));
	}
	for (i = N; i < N2; i++)
	{
		nIndex[i] = i;
		fDepth[i] = FLT_MAX;
	}

	//if (N > 12)
	//    fragment_blendout[nGid.xy] = float4(1, 0, 0, 1);//ConvertUIntToFloat4(deep_LL_buf[2 * (prefix_LL_buf[nThreadNum - 1] + 1) + 0]);
	//return;

	uint idx = blocksize * nGTid.y + nGTid.x;

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
			   color[i] = deep_LL_buf[2 * (prefix_LL_buf[nThreadNum - 1] + i) + 0];
		   }

		   for(i = 0; i < 8; i++)
		   {
			   deep_LL_buf[2 * (nThreadNum * 8 + i) + 1] = fDepth[i]; //fDepth[nIndex[i]];
			   deep_LL_buf[2 * (nThreadNum * 8 + i) + 0] = color[nIndex[i]];
		   }
		/**/

		// Accumulate fragments into final result
		float4 result = (float4) 0.0f;
		//for (uint x = N - 1; x >= 0; x--)
		//{
		//    uint bufferValue = deep_LL_buf[2 * (prefix_LL_buf[nThreadNum - 1] + nIndex[x]) + 0];
		//    float4 color = ConvertUIntToFloat4(bufferValue);
		//    //result = lerp(result, color, color.a);
		//    //result = result * (1 - color.a) + color * color.a;
		//    result = result * (1 - color.a) + color; // because color is an associated one.
		//}
		for (uint x = 0; x < N; x++)
		{
			//uint bufferValue = deep_LL_buf[2 * (prefix_LL_buf[nThreadNum - 1] + nIndex[x]) + 0];
			uint bufferValue = LOAD1_RBB(2 * (prefix_LL_buf[nThreadNum - 1] + nIndex[x]) + 0);
			float4 color = ConvertUIntToFloat4(bufferValue);
			result += color * (1 - result.a);
		}
		//fragment_blendout[nGid.xy] = float4((float3)N / 8, 1);//
		//fragment_blendout[nGid.xy] = ConvertUIntToFloat4(LOAD1_RBB(2 * (prefix_LL_buf[nThreadNum - 1] + nIndex[0]) + 0));
		fragment_blendout[nGid.xy] = result;
		//fragment_zdepth[nGid.xy] = asfloat(LOAD1_RBB(2 * (prefix_LL_buf[nThreadNum - 1] + nIndex[x - 1]) + 1));
		fragment_zdepth[nGid.xy] = N; // for checking # of layers
	}
}