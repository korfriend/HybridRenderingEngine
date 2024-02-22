//#define PUSHCONSTANT(name, type) ConstantBuffer<type> name : register(b999)

struct SortConstants
{
	int3 job_params;
	uint counterReadOffset;
};

cbuffer cbGlobalParams : register(b13)
{
	SortConstants g_cbSortContants;
}


ByteAddressBuffer counterBuffer : register(t0);
StructuredBuffer<float> comparisonBuffer : register(t1);

RWStructuredBuffer<uint> indexBuffer : register(u0);
RWByteAddressBuffer indirectBuffers : register(u1);

[numthreads(1, 1, 1)]
void SortKickoff(uint3 DTid : SV_DispatchThreadID)
{
	// retrieve GPU itemcount:
	uint itemCount = counterBuffer.Load(g_cbSortContants.counterReadOffset);

	if (itemCount > 0)
	{
		// calculate threadcount:
		uint threadCount = ((itemCount - 1) >> 9) + 1;

		// and prepare to dispatch the sort for the alive simulated particles:
		indirectBuffers.Store3(0, uint3(threadCount, 1, 1));
	}
	else
	{
		// dispatch nothing:
		indirectBuffers.Store3(0, uint3(0, 0, 0));
	}
}


//
// Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#if SORT_INNER == 0

#define SORT_SIZE 512

#if( SORT_SIZE>4096 )
// won't work for arrays>4096
#error due to LDS size SORT_SIZE must be 4096 or smaller
#else
#define ITEMS_PER_GROUP	( SORT_SIZE )
#endif

#define HALF_SIZE		(SORT_SIZE/2)
#define ITERATIONS		(HALF_SIZE > 1024 ? HALF_SIZE/1024 : 1)
#define NUM_THREADS		(HALF_SIZE/ITERATIONS)
#define INVERSION		(16*2 + 8*3)
#else

#define SORT_SIZE 512

#if( SORT_SIZE>2048 )
#error
#endif

#define HALF_SIZE		(SORT_SIZE/2)
#define ITERATIONS		(HALF_SIZE > 1024 ? HALF_SIZE/1024 : 1)

#define NUM_THREADS		(SORT_SIZE/2)
#define INVERSION		(16*2 + 8*3)

#endif
//--------------------------------------------------------------------------------------
// Bitonic Sort Compute Shader
//--------------------------------------------------------------------------------------
groupshared float2	g_LDS[SORT_SIZE];

[numthreads(NUM_THREADS, 1, 1)]
void Sort(uint3 Gid	: SV_GroupID,
	uint3 DTid : SV_DispatchThreadID,
	uint3 GTid : SV_GroupThreadID,
	uint	GI : SV_GroupIndex)
{
	uint NumElements = counterBuffer.Load(g_cbSortContants.counterReadOffset);

	uint GlobalBaseIndex = (Gid.x * SORT_SIZE) + GTid.x;
	uint LocalBaseIndex = GI;

	uint numElementsInThreadGroup = min(SORT_SIZE, NumElements - (Gid.x * SORT_SIZE));

	// Load shared data
	uint i;
	[unroll] for (i = 0; i < 2 * ITERATIONS; ++i)
	{
		if (GI + i * NUM_THREADS < numElementsInThreadGroup)
		{
			uint particleIndex = indexBuffer[GlobalBaseIndex + i * NUM_THREADS];
			float dist = comparisonBuffer[particleIndex];
			g_LDS[LocalBaseIndex + i * NUM_THREADS] = float2(dist, (float)particleIndex);
		}
	}
	GroupMemoryBarrierWithGroupSync();

	// Bitonic sort
	for (uint nMergeSize = 2; nMergeSize <= SORT_SIZE; nMergeSize = nMergeSize * 2)
	{
		for (uint nMergeSubSize = nMergeSize >> 1; nMergeSubSize > 0; nMergeSubSize = nMergeSubSize >> 1)
		{
			[unroll] for (i = 0; i < ITERATIONS; ++i)
			{
				int tmp_index = GI + NUM_THREADS * i;
				int index_low = tmp_index & (nMergeSubSize - 1);
				int index_high = 2 * (tmp_index - index_low);
				int index = index_high + index_low;

				uint nSwapElem = nMergeSubSize == nMergeSize >> 1 ? index_high + (2 * nMergeSubSize - 1) - index_low : index_high + nMergeSubSize + index_low;
				if (nSwapElem < numElementsInThreadGroup)
				{
					float2 a = g_LDS[index];
					float2 b = g_LDS[nSwapElem];

					if (a.x > b.x)
					{
						g_LDS[index] = b;
						g_LDS[nSwapElem] = a;
					}
				}
				GroupMemoryBarrierWithGroupSync();
			}
		}
	}

	// Store shared data
	[unroll] for (i = 0; i < 2 * ITERATIONS; ++i)
	{
		if (GI + i * NUM_THREADS < numElementsInThreadGroup)
		{
			indexBuffer[GlobalBaseIndex + i * NUM_THREADS] = (uint)g_LDS[LocalBaseIndex + i * NUM_THREADS].y;
		}
	}
}


[numthreads(256, 1, 1)]
void SortStep(uint3 Gid	: SV_GroupID,
	uint3 GTid : SV_GroupThreadID)
{
	uint NumElements = counterBuffer.Load(g_cbSortContants.counterReadOffset);

	uint4 tgp;

	tgp.x = Gid.x * 256;
	tgp.y = 0;
	tgp.z = NumElements;
	tgp.w = min(512, max(0, NumElements - Gid.x * 512));

	uint localID = tgp.x + GTid.x; // calculate threadID within this sortable-array

	uint index_low = localID & (g_cbSortContants.job_params.x - 1);
	uint index_high = 2 * (localID - index_low);

	uint index = tgp.y + index_high + index_low;
	uint nSwapElem = tgp.y + index_high + g_cbSortContants.job_params.y + g_cbSortContants.job_params.z * index_low;

	if (nSwapElem < tgp.y + tgp.z)
	{
		uint index_a = indexBuffer[index];
		uint index_b = indexBuffer[nSwapElem];
		float a = comparisonBuffer[index_a];
		float b = comparisonBuffer[index_b];

		if (a > b)
		{
			indexBuffer[index] = index_b;
			indexBuffer[nSwapElem] = index_a;
		}
	}
}

[numthreads(NUM_THREADS, 1, 1)]
void SortInner(uint3 Gid	: SV_GroupID,
	uint3 DTid : SV_DispatchThreadID,
	uint3 GTid : SV_GroupThreadID,
	uint	GI : SV_GroupIndex)
{
	uint NumElements = counterBuffer.Load(g_cbSortContants.counterReadOffset);

	uint4 tgp;

	tgp.x = Gid.x * 256;
	tgp.y = 0;
	tgp.z = NumElements;
	tgp.w = min(512, max(0, NumElements - Gid.x * 512));

	uint GlobalBaseIndex = tgp.y + tgp.x * 2 + GTid.x;
	uint LocalBaseIndex = GI;
	uint i;

	// Load shared data
	[unroll] for (i = 0; i < 2; ++i)
	{
		if (GI + i * NUM_THREADS < tgp.w)
		{
			uint particleIndex = indexBuffer[GlobalBaseIndex + i * NUM_THREADS];
			float dist = comparisonBuffer[particleIndex];
			g_LDS[LocalBaseIndex + i * NUM_THREADS] = float2(dist, (float)particleIndex);
		}
	}
	GroupMemoryBarrierWithGroupSync();

	// sort threadgroup shared memory
	for (int nMergeSubSize = SORT_SIZE >> 1; nMergeSubSize > 0; nMergeSubSize = nMergeSubSize >> 1)
	{
		int tmp_index = GI;
		int index_low = tmp_index & (nMergeSubSize - 1);
		int index_high = 2 * (tmp_index - index_low);
		int index = index_high + index_low;

		uint nSwapElem = index_high + nMergeSubSize + index_low;

		if (nSwapElem < tgp.w)
		{
			float2 a = g_LDS[index];
			float2 b = g_LDS[nSwapElem];

			if (a.x > b.x)
			{
				g_LDS[index] = b;
				g_LDS[nSwapElem] = a;
			}
		}
		GroupMemoryBarrierWithGroupSync();
	}

	// Store shared data
	[unroll] for (i = 0; i < 2; ++i)
	{
		if (GI + i * NUM_THREADS < tgp.w)
		{
			indexBuffer[GlobalBaseIndex + i * NUM_THREADS] = (uint)g_LDS[LocalBaseIndex + i * NUM_THREADS].y;
		}
	}
}