#ifndef SHADERINTEROP_BVH_H
#define SHADERINTEROP_BVH_H

#ifndef __cplusplus // not invoking shader compiler, but included in engine source
struct PrimitiveID
{
	uint primitiveIndex;
	uint instanceIndex;
	uint subsetIndex;
	bool maybe_clustered;

	// These packing methods don't need meshlets, but they are packed into 64 bits:
	inline uint2 pack2()
	{
		// + 1 valid check
		// 32 bit primitiveIndex 
		// 24 bit instanceIndex
		// 8  bit subsetIndex
		return uint2(primitiveIndex + 1, ((instanceIndex + 1) & 0xFFFFFF) | ((subsetIndex & 0xFF) << 24u));
	}

	void unpack2(uint2 value)
	{
		primitiveIndex = value.x - 1; // remove valid check
		instanceIndex = value.y & 0xFFFFFF;
		subsetIndex = (value.y >> 24u) & 0xFF;
		maybe_clustered = false;
	}
};

// Expands a 10-bit integer into 30 bits
// by inserting 2 zeros after each bit.
inline uint expandBits(uint v)
{
	v = (v * 0x00010001u) & 0xFF0000FFu;
	v = (v * 0x00000101u) & 0x0F00F00Fu;
	v = (v * 0x00000011u) & 0xC30C30C3u;
	v = (v * 0x00000005u) & 0x49249249u;
	return v;
}

// Calculates a 30-bit Morton code for the
// given 3D point located within the unit cube [0,1].
inline uint morton3D(in float3 pos)
{
	pos.x = min(max(pos.x * 1024, 0), 1023);
	pos.y = min(max(pos.y * 1024, 0), 1023);
	pos.z = min(max(pos.z * 1024, 0), 1023);
	uint xx = expandBits((uint)pos.x);
	uint yy = expandBits((uint)pos.y);
	uint zz = expandBits((uint)pos.z);
	return xx * 4 + yy * 2 + zz;
}

#endif

#ifdef __cplusplus // not invoking shader compiler, but included in engine source

#include "../vzm2/Math.h"

using float3x3 = XMFLOAT3X3;
using float4x4 = XMFLOAT4X4;
using float2 = XMFLOAT2;
using float3 = XMFLOAT3;
using float4 = XMFLOAT4;
using uint = uint32_t;
using uint2 = XMUINT2;
using uint3 = XMUINT3;
using uint4 = XMUINT4;
using int2 = XMINT2;
using int3 = XMINT3;
using int4 = XMINT4;

#endif

struct BVHPushConstants
{
	uint instanceIndex;
	uint subsetIndex;
	uint primitiveCount;
	uint primitiveOffset;

	// GEOMETRYSPACE only (for a single geometry)
	float3 aabb_min;
	uint padding0;
	float3 aabb_extents_rcp;	// enclosing AABB 1.0f / abs(max - min)
	uint padding1;
};

static const uint BVH_BUILDER_GROUPSIZE = 64;

// lower 8-bits in the flags are for instance masking
static const uint BVH_PRIMITIVE_FLAG_DOUBLE_SIDED = 1 << 8;
static const uint BVH_PRIMITIVE_FLAG_TRANSPARENT = 1 << 9;

struct BVHPrimitive
{
	uint2 packed_prim;
	uint flags;
	float x0;

	float y0;
	float z0;
	float x1;
	float y1;

	float z1;
	float x2;
	float y2;
	float z2;

	float3 v0() { return float3(x0, y0, z0); }
	float3 v1() { return float3(x1, y1, z1); }
	float3 v2() { return float3(x2, y2, z2); }

#ifndef __cplusplus
	PrimitiveID primitiveID()
	{
		PrimitiveID prim;
		prim.unpack2(packed_prim);
		return prim;
	}
#endif // __cplusplus
};

struct BVHNode
{
	float3 min;
	uint LeftChildIndex;
	float3 max;
	uint RightChildIndex;
};

#endif // SHADERINTEROP_BVH_H
