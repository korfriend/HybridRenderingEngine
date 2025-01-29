#include "../ShaderInterop_BVH.h"


Buffer<float> vertexBuffer : register(t0);
Buffer<uint> indexBuffer : register(t1);

RWStructuredBuffer<uint> primitiveIDBuffer : register(u0);
RWStructuredBuffer<BVHPrimitive> primitiveBuffer : register(u1);
RWStructuredBuffer<float> primitiveMortonBuffer : register(u2); // morton buffer is float because sorting is written for floats!

// This shader builds scene triangle data and performs BVH classification:
//	- This shader is run per object subset.
//	- Each thread processes a triangle
//	- Computes triangle bounding box, morton code and other properties and stores into global primitive buffer
StructuredBuffer<BVHPushConstants> pushBVH : register(t9);

[numthreads(BVH_BUILDER_GROUPSIZE, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint groupIndex : SV_GroupIndex)
{
	BVHPushConstants push = pushBVH[0];

	if (DTid.x >= push.primitiveCount)
		return;

	PrimitiveID prim;
	prim.primitiveIndex = DTid.x;
	prim.instanceIndex = 1u;
	prim.subsetIndex = 0u;

	uint startIndex = prim.primitiveIndex * 3;
	//uint i0 = bindless_buffers_uint[descriptor_index(geometry.ib)][startIndex + 0];
	//uint i1 = bindless_buffers_uint[descriptor_index(geometry.ib)][startIndex + 1];
	//uint i2 = bindless_buffers_uint[descriptor_index(geometry.ib)][startIndex + 2];
	uint i0 = indexBuffer[startIndex + 0];//.Load((startIndex + 0) * 4);
	uint i1 = indexBuffer[startIndex + 1];//.Load((startIndex + 1) * 4);
	uint i2 = indexBuffer[startIndex + 2];//.Load((startIndex + 2) * 4);
	
	//float3 P0 = bindless_buffers_float4[descriptor_index(geometry.vb_pos_wind)][i0].xyz;
	//float3 P1 = bindless_buffers_float4[descriptor_index(geometry.vb_pos_wind)][i1].xyz;
	//float3 P2 = bindless_buffers_float4[descriptor_index(geometry.vb_pos_wind)][i2].xyz;
	//float3 P0 = asfloat(vertexBuffer.Load3(i0 * 3 * 4));
	//float3 P1 = asfloat(vertexBuffer.Load3(i1 * 3 * 4));
	//float3 P2 = asfloat(vertexBuffer.Load3(i2 * 3 * 4));

	uint vtxIndex0 = i0 * push.vertexStride;
	uint vtxIndex1 = i1 * push.vertexStride;
	uint vtxIndex2 = i2 * push.vertexStride;
	float3 P0 = float3(vertexBuffer[vtxIndex0 + 0], vertexBuffer[vtxIndex0 + 1], vertexBuffer[vtxIndex0 + 2]);
	float3 P1 = float3(vertexBuffer[vtxIndex1 + 0], vertexBuffer[vtxIndex1 + 1], vertexBuffer[vtxIndex1 + 2]);
	float3 P2 = float3(vertexBuffer[vtxIndex2 + 0], vertexBuffer[vtxIndex2 + 1], vertexBuffer[vtxIndex2 + 2]);
	
	BVHPrimitive bvhprim = (BVHPrimitive)0;
	bvhprim.packed_prim = prim.pack2();
	bvhprim.flags = ~0u;

	//bvhprim.x0 = P0.x;
	//bvhprim.y0 = P0.y;
	//bvhprim.z0 = P0.z;
	//bvhprim.x1 = P1.x;
	//bvhprim.y1 = P1.y;
	//bvhprim.z1 = P1.z;
	//bvhprim.x2 = P2.x;
	//bvhprim.y2 = P2.y;
	//bvhprim.z2 = P2.z;


	bvhprim.p0 = P0;
	bvhprim.p1 = P1;
	bvhprim.p2 = P2;

	uint primitiveID = prim.primitiveIndex;

	//primitiveBuffer.Store<BVHPrimitive>(primitiveID * sizeof(BVHPrimitive), bvhprim);
	primitiveBuffer[primitiveID] = bvhprim;

	primitiveIDBuffer[primitiveID] = primitiveID; // will be sorted by morton so we need this!

	// Compute triangle morton code:
	float3 minAABB = min(P0, min(P1, P2));
	float3 maxAABB = max(P0, max(P1, P2));
	float3 centerAABB = (minAABB + maxAABB) * 0.5f;
	//const uint mortoncode = morton3D((centerAABB - push.aabb_min) * push.aabb_extents_rcp);
	const uint mortoncode = morton3D((centerAABB - float3(-1.59993887, -1.59993887, -8.50000000)) * float3(0.312505990, 0.312505990, 0.117647059));
	primitiveMortonBuffer[primitiveID] = (float)mortoncode; // convert to float before sorting

}
