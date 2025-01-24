#include "../ShaderInterop_BVH.h"

// This shader builds scene triangle data and performs BVH classification:
//	- This shader is run per object subset.
//	- Each thread processes a triangle
//	- Computes triangle bounding box, morton code and other properties and stores into global primitive buffer
cbuffer cbGlobalParams : register(b0)
{
	BVHPushConstants g_cbBVH;
}

ByteAddressBuffer positionBuffer : register(t0);
ByteAddressBuffer indexBuffer : register(t1);

RWStructuredBuffer<uint> primitiveIDBuffer : register(u0);
RWStructuredBuffer<BVHPrimitive> primitiveBuffer : register(u1);
RWStructuredBuffer<float> primitiveMortonBuffer : register(u2); // morton buffer is float because sorting is written for floats!

[numthreads(BVH_BUILDER_GROUPSIZE, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint groupIndex : SV_GroupIndex)
{
	if (DTid.x >= g_cbBVH.primitiveCount)
		return;

	PrimitiveID prim;
	prim.primitiveIndex = DTid.x;
	prim.instanceIndex = g_cbBVH.instanceIndex;
	prim.subsetIndex = g_cbBVH.subsetIndex;

	uint startIndex = prim.primitiveIndex * 3;
	//uint i0 = bindless_buffers_uint[descriptor_index(geometry.ib)][startIndex + 0];
	//uint i1 = bindless_buffers_uint[descriptor_index(geometry.ib)][startIndex + 1];
	//uint i2 = bindless_buffers_uint[descriptor_index(geometry.ib)][startIndex + 2];
	uint i0 = indexBuffer.Load((startIndex + 0) * 4);
	uint i1 = indexBuffer.Load((startIndex + 1) * 4);
	uint i2 = indexBuffer.Load((startIndex + 2) * 4);
	
	//float3 P0 = bindless_buffers_float4[descriptor_index(geometry.vb_pos_wind)][i0].xyz;
	//float3 P1 = bindless_buffers_float4[descriptor_index(geometry.vb_pos_wind)][i1].xyz;
	//float3 P2 = bindless_buffers_float4[descriptor_index(geometry.vb_pos_wind)][i2].xyz;
	float3 P0 = asfloat(positionBuffer.Load3(i0 * 3 * 4));
	float3 P1 = asfloat(positionBuffer.Load3(i1 * 3 * 4));
	float3 P2 = asfloat(positionBuffer.Load3(i2 * 3 * 4));

	
	BVHPrimitive bvhprim;
	bvhprim.packed_prim = prim.pack2();
	bvhprim.flags = 0;

	bvhprim.x0 = P0.x;
	bvhprim.y0 = P0.y;
	bvhprim.z0 = P0.z;
	bvhprim.x1 = P1.x;
	bvhprim.y1 = P1.y;
	bvhprim.z1 = P1.z;
	bvhprim.x2 = P2.x;
	bvhprim.y2 = P2.y;
	bvhprim.z2 = P2.z;

	uint primitiveID = g_cbBVH.primitiveOffset + prim.primitiveIndex;

	//primitiveBuffer.Store<BVHPrimitive>(primitiveID * sizeof(BVHPrimitive), bvhprim);
	primitiveBuffer[primitiveID] = bvhprim;

	primitiveIDBuffer[primitiveID] = primitiveID; // will be sorted by morton so we need this!

	// Compute triangle morton code:
	float3 minAABB = min(P0, min(P1, P2));
	float3 maxAABB = max(P0, max(P1, P2));
	float3 centerAABB = (minAABB + maxAABB) * 0.5f;
	const uint mortoncode = morton3D((centerAABB - g_cbBVH.aabb_min) * g_cbBVH.aabb_extents_rcp);
	primitiveMortonBuffer[primitiveID] = (float)mortoncode; // convert to float before sorting

}
