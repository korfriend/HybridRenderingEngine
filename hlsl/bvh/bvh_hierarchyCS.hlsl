#include "../ShaderInterop_BVH.h"

// This shader will construct the BVH from sorted primitive morton codes.
//	Output is a list of continuous BVH tree nodes in memory: [parentIndex, leftChildNodeIndex, rightChildNodeIndex]. Additionally, we will reset the BVH Flag Buffer (used for AABB propagation step)
//	The output node is a leaf node if: leftChildNodeIndex == rightChildNodeIndex == 0
//	Else the output node is an intermediate node
//	Also, we know that intermediate nodes start at arrayIndex == 0 (starting with root node)
//	Also, we know that leaf nodes will start at arrayIndex == primitiveCount -1 (and they will correspond to a single primitive, which is indexable by primitiveIndexBuffer later)

// Using the Karras's 2012 parallel BVH construction algorithm outlined 
// in "Maximizing Parallelism in the Construction of BVHs, Octrees,
// and k-d Trees"

ByteAddressBuffer primitiveCounterBuffer : register(t0);
StructuredBuffer<uint> primitiveIDBuffer : register(t1);
StructuredBuffer<float> primitiveMortonBuffer : register(t2); // float because it was sorted

RWStructuredBuffer<BVHNode> bvhNodeBuffer : register(u0);
RWStructuredBuffer<uint> bvhParentBuffer : register(u1);
RWStructuredBuffer<uint> bvhFlagBuffer : register(u2);

//inline BVHNode LoadBVHNode(const uint idx)
//{
//	uint baseAddress = idx * 32;
//	uint4 raw0_4u = bvhNodeBuffer.Load4(baseAddress);
//	uint4 raw1_4u = bvhNodeBuffer.Load4(baseAddress + 16);
//
//	BVHNode node;
//	node.min = asfloat(raw0_4u.xyz);
//	node.LeftChildIndex = asfloat(raw0_4u.w);
//	node.max = asfloat(raw1_4u.xyz);
//	node.RightChildIndex = asfloat(raw1_4u.w);
//	return node;
//}
//
//inline void StoreBVHNode(const uint idx, const in BVHNode bvhNode)
//{
//	uint baseAddress = idx * 32;
//	
//	uint4 raw0_4u = uint4(asuint(bvhNode.min), bvhNode.LeftChildIndex);
//	uint4 raw1_4u = uint4(asuint(bvhNode.max), bvhNode.RightChildIndex);
//	
//    bvhNodeBuffer.Store4(baseAddress +  0, raw0_4u);
//    bvhNodeBuffer.Store4(baseAddress + 16, raw1_4u);
//}

int CountLeadingZeroes(uint num)
{
	return 31 - firstbithigh(num);
}

int GetLongestCommonPrefix(uint indexA, uint indexB, uint elementCount)
{
	if (indexA >= elementCount || indexB >= elementCount)
	{
		return -1;
	}
	else
	{
		uint mortonCodeA = (float)primitiveMortonBuffer[primitiveIDBuffer[indexA]];
		uint mortonCodeB = (float)primitiveMortonBuffer[primitiveIDBuffer[indexB]];
		if (mortonCodeA != mortonCodeB)
		{
			return CountLeadingZeroes(mortonCodeA ^ mortonCodeB);
		}
		else
		{
			// TODO: Technically this should be primitive ID
			return CountLeadingZeroes(indexA ^ indexB) + 31;
		}
	}
}

uint2 DetermineRange(uint idx, uint elementCount)
{
	int d = GetLongestCommonPrefix(idx, idx + 1, elementCount) - GetLongestCommonPrefix(idx, idx - 1, elementCount);
	d = clamp(d, -1, 1);
	int minPrefix = GetLongestCommonPrefix(idx, idx - d, elementCount);

	// TODO: Consider starting this at a higher number
	uint maxLength = 2;
	while (GetLongestCommonPrefix(idx, idx + maxLength * d, elementCount) > minPrefix)
	{
		maxLength *= 4;
	}

	int length = 0;
	for (uint t = maxLength / 2; t > 0; t /= 2)
	{
		if (GetLongestCommonPrefix(idx, idx + (length + t) * d, elementCount) > minPrefix)
		{
			length = length + t;
		}
	}

	int j = idx + length * d;
	return uint2(min((int)idx, j), max((int)idx, j));
}

int FindSplit(int first, uint last, uint elementCount)
{
	int commonPrefix = GetLongestCommonPrefix(first, last, elementCount);
	int split = first;
	int step = last - first;

	do
	{
		step = (step + 1) >> 1;
		int newSplit = split + step;

		if (newSplit < (int)last)
		{
			int splitPrefix = GetLongestCommonPrefix(first, newSplit, elementCount);
			if (splitPrefix > commonPrefix)
				split = newSplit;
		}
	} while (step > 1);

	return split;
}



[numthreads(BVH_BUILDER_GROUPSIZE, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	const uint idx = DTid.x;
	const uint primitiveCount = primitiveCounterBuffer.Load(0);

	if (idx < primitiveCount - 1)
	{
		uint2 range = DetermineRange(idx, primitiveCount);
		uint first = range.x;
		uint last = range.y;

		uint split = FindSplit(first, last, primitiveCount);

		uint internalNodeOffset = 0;
		uint leafNodeOffset = primitiveCount - 1;
		uint childAIndex;
		if (split == first)
			childAIndex = leafNodeOffset + split;
		else
			childAIndex = internalNodeOffset + split;

		uint childBIndex;
		if (split + 1 == last)
			childBIndex = leafNodeOffset + split + 1;
		else
			childBIndex = internalNodeOffset + split + 1;

		// write to parent:
		//BVHNode node = bvhNodeBuffer.Load<BVHNode>(idx * sizeof(BVHNode));
		BVHNode node = bvhNodeBuffer[idx];// LoadBVHNode(idx);

		node.LeftChildIndex = childAIndex;
		node.RightChildIndex = childBIndex;

		//bvhNodeBuffer.Store<BVHNode>(idx * sizeof(BVHNode), node);
		bvhNodeBuffer[idx] = node;//StoreBVHNode(idx, node);

		// write to children:
		bvhParentBuffer[childAIndex] = idx;
		bvhParentBuffer[childBIndex] = idx;

		// Reset bvh node flag (only internal nodes):
		bvhFlagBuffer[idx] = 0;
	}
}
