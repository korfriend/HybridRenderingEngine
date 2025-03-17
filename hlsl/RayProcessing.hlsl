#if DX10_0 == 1
#define BVH_LEGACY
#define __EXIT return out_ps
#else
#define __EXIT return
#endif


#ifdef BVH_LEGACY

#define STACK_SIZE  64  // Size of the traversal stack in local memory.
#define M_PI 3.1415926535897932384626422832795028841971f
#define TWO_PI 6.2831853071795864769252867665590057683943f
#define DYNAMIC_FETCH_THRESHOLD 20          // If fewer than this active, fetch new rays
#define samps 1
#define F32_MIN          (1.175494351e-38f)
#define F32_MAX          (3.402823466e+38f)

#define EntrypointSentinel 0x76543210 
#define MaxBlockHeight 6

typedef int Refl_t;
#define DIFF 0
#define METAL 1
#define SPEC 2
#define REFR 3
#define COAT 4
//enum Refl_t { DIFF = 0, METAL, SPEC, REFR, COAT };  // material types

typedef float3 Vec3f;
typedef float4 Vec4f;

Vec3f absmax3f(const Vec3f v1, const Vec3f v2) {
	return Vec3f(v1.x * v1.x > v2.x * v2.x ? v1.x : v2.x, v1.y * v1.y > v2.y * v2.y ? v1.y : v2.y, v1.z * v1.z > v2.z * v2.z ? v1.z : v2.z);
}

struct Ray {
	float3 orig;	// ray origin
	float3 dir;		// ray direction	
	//Ray(float3 o_, float3 d_) : orig(o_), dir(d_) {}
	//Ray(float3 o_, float3 d_) { orig = o_; dir = d_; }
};

//  RAY BOX INTERSECTION ROUTINES

// Experimentally determined best mix of float/int/video minmax instructions for Kepler.

// float c0min = spanBeginKepler2(c0lox, c0hix, c0loy, c0hiy, c0loz, c0hiz, tmin); // Tesla does max4(min, min, min, tmin)
// float c0max = spanEndKepler2(c0lox, c0hix, c0loy, c0hiy, c0loz, c0hiz, hitT); // Tesla does min4(max, max, max, tmax)

// Perform min/max operations in hardware
// Using Kepler's video instructions, see http://docs.nvidia.com/cuda/parallel-thread-execution/#axzz3jbhbcTZf																			//  : "=r"(v) overwrites v and puts it in a register
// see https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html

int   min_min(int a, int b, int c) { return min(min(a, b), c); }
int   min_max(int a, int b, int c) { return max(min(a, b), c); }
int   max_min(int a, int b, int c) { return min(max(a, b), c); }
int   max_max(int a, int b, int c) { return max(max(a, b), c); }
float fmin_fmin(float a, float b, float c) { return min(min(a, b), c); }
float fmin_fmax(float a, float b, float c) { return max(min(a, b), c); }
float fmax_fmin(float a, float b, float c) { return min(max(a, b), c); }
float fmax_fmax(float a, float b, float c) { return max(max(a, b), c); }

#define fminf min
#define fmaxf max
#define min3f min
#define max3f max

float v3maxf(float3 v3) { return fmax_fmax(v3.x, v3.y, v3.z); }
float v3minf(float3 v3) { return fmin_fmin(v3.x, v3.y, v3.z); }

float spanBeginKepler(float a0, float a1, float b0, float b1, float c0, float c1, float d) { return fmax_fmax(fminf(a0, a1), fminf(b0, b1), fmin_fmax(c0, c1, d)); }
float spanEndKepler(float a0, float a1, float b0, float b1, float c0, float c1, float d) { return fmin_fmin(fmaxf(a0, a1), fmaxf(b0, b1), fmax_fmin(c0, c1, d)); }

// standard ray box intersection routines (for debugging purposes only)
// based on Intersect::RayBox() in original Aila/Laine code
float spanBeginKepler2(float lo_x, float hi_x, float lo_y, float hi_y, float lo_z, float hi_z, float d) {

	Vec3f t0 = Vec3f(lo_x, lo_y, lo_z);
	Vec3f t1 = Vec3f(hi_x, hi_y, hi_z);

	Vec3f realmin = min3f(t0, t1);

	float raybox_tmin = v3maxf(realmin); // maxmin

	//return Vec2f(tmin, tmax);
	return raybox_tmin;
}

float spanEndKepler2(float lo_x, float hi_x, float lo_y, float hi_y, float lo_z, float hi_z, float d) {

	Vec3f t0 = Vec3f(lo_x, lo_y, lo_z);
	Vec3f t1 = Vec3f(hi_x, hi_y, hi_z);

	Vec3f realmax = max3f(t0, t1);

	float raybox_tmax = v3minf(realmax);// .min(); /// minmax

	//return Vec2f(tmin, tmax);
	return raybox_tmax;
}

void swap2(inout int a, inout int b) { int temp = a; a = b; b = temp; }

// standard ray triangle intersection routines (for debugging purposes only)
// based on Intersect::RayTriangle() in original Aila/Laine code
#define __EPSILON 0.00001f // works better
Vec3f intersectRayTriangle(const Vec3f v0, const Vec3f v1, const Vec3f v2, const Vec4f rayorig, const Vec4f raydir) {
	const Vec3f miss = Vec3f(F32_MAX, F32_MAX, F32_MAX);

	Vec3f v0v1 = v1 - v0;
	Vec3f v0v2 = v2 - v0;
	Vec3f pvec = cross(raydir.xyz, v0v2);
	float det = dot(v0v1, pvec);
#ifdef CULLING 
	// if the determinant is negative the triangle is backfacing
	// if the determinant is close to 0, the ray misses the triangle
	if (det < __EPSILON) 
		return miss;
#else 
	// ray and triangle are parallel if det is close to 0
	if (abs(det) < __EPSILON)
		return miss;
#endif 
	float invDet = 1 / det;

	Vec3f tvec = rayorig.xyz - v0;
	float u = dot(tvec, pvec) * invDet;
	if (u < 0 || u > 1) return miss;

	Vec3f qvec = cross(tvec, v0v1);
	float v = dot(raydir.xyz, qvec) * invDet;
	if (v < 0 || u + v > 1) return miss;

	float t = dot(v0v2, qvec) * invDet;

	return Vec3f(u, v, t); //true;
}

Buffer<float4> buf_gpuNodes : register(t0);
Buffer<float4> buf_gpuTriWoops : register(t1);
Buffer<float4> buf_gpuDebugTris : register(t2);
Buffer<int> buf_gpuTriIndices : register(t3);

#else // BVH_LEGACY

#include "Globals.hlsli"
#include "ShaderInterop_BVH.h"

ByteAddressBuffer primitiveCounterBuffer : register(t0);
StructuredBuffer<uint> primitiveIDBuffer : register(t1);
StructuredBuffer<float> primitiveMortonBuffer : register(t2); // float because it was sorted
StructuredBuffer<BVHNode> bvhNodeBuffer : register(t3);
StructuredBuffer<BVHPrimitive> primitiveBuffer : register(t4);
StructuredBuffer<uint> bvhParentBuffer : register(t5);
StructuredBuffer<uint> bvhFlagBuffer : register(t6);

// TRY THIS!! 
Buffer<float> vertexBuffer : register(t10);
Buffer<uint> indexBuffer : register(t11);

//StructuredBuffer<BVHPushConstants> pushBVH : register(t100);

#endif // BVH_LEGACY

Buffer<float3> buf_curvePoints : register(t30);
Buffer<float3> buf_curveTangents : register(t31);

// magic values
#define WILDCARD_DEPTH_OUTLINE 0x12345678
#define WILDCARD_DEPTH_OUTLINE_DIRTY 0x12345679
#define OUTSIDE_PLANE 0x87654321

struct PS_FILL_OUTPUT_NO_DS
{
	float4 color : SV_TARGET0;
#if PICKING != 1
	float depthcs : SV_TARGET1;
#endif
	//float ds_z : SV_Depth;
};

Texture2D<float> prev_fragment_zdepth : register(t21);

#if DX10_0 == 1
//#include "Sr_Common.hlsl"
#include "./kbuf/Sr_Kbuf.hlsl"
//#include "CommonShader.hlsl"
//#define __VS_OUT VS_OUTPUT

// USE PIXEL SHADER //
// USE PS_FILL_OUTPUT //
Texture2D prev_fragment_vis : register(t20);

#else
#include "./kbuf/Sr_Kbuf.hlsl"

#if PATHTR_USE_KBUF == 0
//RWTexture2D<uint> fragment_counter : register(u2);
//RWByteAddressBuffer deep_k_buf : register(u4);
#endif
RWBuffer<uint> picking_buf : register(u2);
RWTexture2D<unorm float4> fragment_vis : register(u3);
RWTexture2D<float> fragment_zdepth : register(u4);
#endif

// modified intersection routine (uses regular instead of woopified triangles) for debugging purposes

#ifdef BVH_LEGACY

int DEBUGintersectBVHandTriangles(const in float4 rayorig, const in float4 raydir,
	const in Buffer<float4> gpuNodes, const in Buffer<float4> gpuDebugTris, const in Buffer<int> gpuTriIndices, // const in Buffer<float4> gpuTriWoops,
	inout int hitTriIdx, inout float hitdistance, inout int debugbingo, inout float3 trinormal, bool needClosestHit) 
{
	int traversalStack[STACK_SIZE];

	float   origx, origy, origz;    // Ray origin.
	float   dirx, diry, dirz;       // Ray direction.
	float   tmin;                   // t-value from which the ray starts. Usually 0.
	float   idirx, idiry, idirz;    // 1 / dir
	float   oodx, oody, oodz;       // orig / dir

	int		idxStak;
	int		leafAddr;
	int		nodeAddr;
	int     hitIndex;
	float	hitT;

	origx = rayorig.x;
	origy = rayorig.y;
	origz = rayorig.z;
	dirx = raydir.x;
	diry = raydir.y;
	dirz = raydir.z;
	tmin = rayorig.w;

	// ooeps is very small number, used instead of raydir xyz component when that component is near zero
	float ooeps = 0.00001f;// exp2(-80.0f); // Avoid div by zero, returns 1/2^80, an extremely small number
	float ooeps_x = raydir.x >= 0 ? ooeps : -ooeps;
	float ooeps_y = raydir.y >= 0 ? ooeps : -ooeps;
	float ooeps_z = raydir.z >= 0 ? ooeps : -ooeps;
	idirx = 1.0f / (abs(raydir.x) > ooeps ? raydir.x : ooeps_x); // inverse ray direction
	idiry = 1.0f / (abs(raydir.y) > ooeps ? raydir.y : ooeps_y); // inverse ray direction
	idirz = 1.0f / (abs(raydir.z) > ooeps ? raydir.z : ooeps_z); // inverse ray direction
	oodx = origx * idirx;  // ray origin / ray direction
	oody = origy * idiry;  // ray origin / ray direction
	oodz = origz * idirz;  // ray origin / ray direction

	traversalStack[0] = EntrypointSentinel; // Bottom-most entry. 0x76543210 is 1985229328 in decimal
	// stackPtr = (char*)&traversalStack[0];
	idxStak = 0; // point stackPtr (idxStak) to bottom of traversal stack = EntryPointSentinel
	leafAddr = 0;   // No postponed leaf.
	nodeAddr = 0;   // Start from the root.
	hitIndex = -1;  // No triangle intersected so far.
	hitT = raydir.w;

	[loop]
	while ((uint)nodeAddr != EntrypointSentinel) // EntrypointSentinel = 0x76543210 
	{
		// Traverse internal nodes until all SIMD lanes have found a leaf.

		bool searchingLeaf = true; // flag required to increase efficiency of threads in warp
		[loop]
		while (nodeAddr >= 0 && (uint)nodeAddr != EntrypointSentinel)
		{
			int nodeIdx = nodeAddr >> 4; // float4* ptr = (float4*)((char*)gpuNodes + nodeAddr);
			float4 n0xy = gpuNodes[nodeIdx];// ptr[0]; // childnode 0, xy-bounds (c0.lo.x, c0.hi.x, c0.lo.y, c0.hi.y)		
			float4 n1xy = gpuNodes[nodeIdx + 1];//ptr[1]; // childnode 1. xy-bounds (c1.lo.x, c1.hi.x, c1.lo.y, c1.hi.y)		
			float4 nz = gpuNodes[nodeIdx + 2];//ptr[2]; // childnodes 0 and 1, z-bounds(c0.lo.z, c0.hi.z, c1.lo.z, c1.hi.z)			

			// ptr[3] contains indices to 2 childnodes in case of innernode, see below
			// (childindex = size of array during building, see CudaBVH.cpp)

			// compute ray intersections with BVH node bounding box

			float c0lox = n0xy.x * idirx - oodx; // n0xy.x = c0.lo.x, child 0 minbound x
			float c0hix = n0xy.y * idirx - oodx; // n0xy.y = c0.hi.x, child 0 maxbound x
			float c0loy = n0xy.z * idiry - oody; // n0xy.z = c0.lo.y, child 0 minbound y
			float c0hiy = n0xy.w * idiry - oody; // n0xy.w = c0.hi.y, child 0 maxbound y
			float c0loz = nz.x * idirz - oodz; // nz.x   = c0.lo.z, child 0 minbound z
			float c0hiz = nz.y * idirz - oodz; // nz.y   = c0.hi.z, child 0 maxbound z
			float c1loz = nz.z * idirz - oodz; // nz.z   = c1.lo.z, child 1 minbound z
			float c1hiz = nz.w * idirz - oodz; // nz.w   = c1.hi.z, child 1 maxbound z
			float c0min = spanBeginKepler(c0lox, c0hix, c0loy, c0hiy, c0loz, c0hiz, tmin); // Tesla does max4(min, min, min, tmin)
			float c0max = spanEndKepler(c0lox, c0hix, c0loy, c0hiy, c0loz, c0hiz, hitT); // Tesla does min4(max, max, max, tmax)
			float c1lox = n1xy.x * idirx - oodx; // n1xy.x = c1.lo.x, child 1 minbound x
			float c1hix = n1xy.y * idirx - oodx; // n1xy.y = c1.hi.x, child 1 maxbound x
			float c1loy = n1xy.z * idiry - oody; // n1xy.z = c1.lo.y, child 1 minbound y
			float c1hiy = n1xy.w * idiry - oody; // n1xy.w = c1.hi.y, child 1 maxbound y
			float c1min = spanBeginKepler(c1lox, c1hix, c1loy, c1hiy, c1loz, c1hiz, tmin);
			float c1max = spanEndKepler(c1lox, c1hix, c1loy, c1hiy, c1loz, c1hiz, hitT);

			float ray_tmax = 1e20;
			bool traverseChild0 = (c0min <= c0max) && (c0min >= tmin) && (c0min <= ray_tmax);
			bool traverseChild1 = (c1min <= c1max) && (c1min >= tmin) && (c1min <= ray_tmax);

			if (!traverseChild0 && !traverseChild1)
			{
				nodeAddr = traversalStack[idxStak];// *(int*)stackPtr; // fetch next node by popping stack
				idxStak -= 1; // stackPtr -= 4; // popping decrements stack by 4 bytes (because stackPtr is a pointer to char) 
			}
			// Otherwise => fetch child pointers.
			else  // one or both children intersected
			{
				float4 nodef4 = gpuNodes[nodeIdx + 3];
				int2 cnodes = int2(asint(nodef4.x), asint(nodef4.y));// = *(int2*) & ptr[3];

				// set nodeAddr equal to intersected childnode (first childnode when both children are intersected)
				nodeAddr = (traverseChild0) ? cnodes.x : cnodes.y;

				// Both children were intersected => push the farther one on the stack.

				if (traverseChild0 && traverseChild1) // store closest child in nodeAddr, swap if necessary
				{
					if (c1min < c0min)
						swap2(nodeAddr, cnodes.y);
					idxStak += 1;// stackPtr += 4;  // pushing increments stack by 4 bytes (stackPtr is a pointer to char)
					traversalStack[idxStak] = cnodes.y;// *(int*)stackPtr = cnodes.y; // push furthest node on the stack
				}
			}

			// First leaf => postpone and continue traversal.
			// leafnodes have a negative index to distinguish them from inner nodes
			// if nodeAddr less than 0 -> nodeAddr is a leaf
			if (nodeAddr < 0 && leafAddr >= 0)  // if leafAddr >= 0 -> no leaf found yet (first leaf)
			{
				searchingLeaf = false; // required for warp efficiency
				leafAddr = nodeAddr;

				nodeAddr = traversalStack[idxStak];// nodeAddr = *(int*)stackPtr;  // pops next node from stack
				idxStak -= 1;// stackPtr -= 4;  // decrement by 4 bytes (stackPtr is a pointer to char)
			}

			// All SIMD lanes have found a leaf => process them.
			// NOTE: inline PTX implementation of "if(!__any(leafAddr >= 0)) break;".
			// tried everything with CUDA 4.2 but always got several redundant instructions.

			// if (!searchingLeaf){ break;  }  

			// if (!__any(searchingLeaf)) break; // "__any" keyword: if none of the threads is searching a leaf, in other words
			// if all threads in the warp found a leafnode, then break from while loop and go to triangle intersection

			// if(!__any(leafAddr >= 0))   /// als leafAddr in PTX code >= 0, dan is het geen echt leafNode   
			//    break;

			//unsigned int mask; // mask replaces searchingLeaf in PTX code
			if (leafAddr < 0) {
				break;
			}
			//
			//asm("{\n"
			//"   .reg .pred p;               \n"
			//	"setp.ge.s32        p, %1, 0;   \n"
			//	"vote.ballot.b32    %0,p;       \n"
			//	"}"
			//	: "=r"(mask)
			//	: "r"(leafAddr));
			//
			//if (!mask)
			//	break;
		}
		///////////////////////////////////////
		/// LEAF NODE / TRIANGLE INTERSECTION
		///////////////////////////////////////

		[loop]
		while (leafAddr < 0)  // if leafAddr is negative, it points to an actual leafnode (when positive or 0 it's an innernode
		{
			// leafAddr is stored as negative number, see cidx[i] = ~triWoopData.getSize(); in CudaBVH.cpp
			[loop]
			for (int triAddr = ~leafAddr;; triAddr += 3)
			{    // no defined upper limit for loop, continues until leaf terminator code 0x80000000 is encountered

				// Read first 16 bytes of the triangle.
				// fetch first triangle vertex
				float3 v0 = gpuDebugTris[triAddr + 0].xyz;

				// End marker 0x80000000 (= negative zero) => all triangles in leaf processed. --> terminate 				
				if (asuint(v0.x) == 0x80000000) break;

				float3 v1 = gpuDebugTris[triAddr + 1].xyz;
				float3 v2 = gpuDebugTris[triAddr + 2].xyz;

				//const float3 v0 = float3(v0f.x, v0f.y, v0f.z);
				//const float3 v1 = float3(v1f.x, v1f.y, v1f.z);
				//const float3 v2 = float3(v2f.x, v2f.y, v2f.z);

				float4 rayorigfloat4 = float4(rayorig.x, rayorig.y, rayorig.z, rayorig.w);
				float4 raydirfloat4 = float4(raydir.x, raydir.y, raydir.z, raydir.w);

				float3 bary = intersectRayTriangle(v0, v1, v2, rayorigfloat4, raydirfloat4);

				float t = bary.z; // hit distance along ray

				if (t > tmin && t < hitT)   // if there is a miss, t will be larger than hitT (ray.tmax)
				{
					hitIndex = triAddr;
					hitT = t;  /// keeps track of closest hitpoint

					trinormal = cross(v0 - v1, v0 - v2);

					if (!needClosestHit) {  // shadow rays only require "any" hit with scene geometry, not the closest one
						nodeAddr = EntrypointSentinel;
						break;
					}
				}

			} // triangle

		// Another leaf was postponed => process it as well.

			leafAddr = nodeAddr;

			if (nodeAddr < 0)
			{
				nodeAddr = traversalStack[idxStak]; // nodeAddr = *(int*)stackPtr;  // pop stack
				idxStak -= 1;// stackPtr -= 4;               // decrement with 4 bytes to get the next int (stackPtr is char*)
			}
		} // end leaf/triangle intersection loop
	} // end of node traversal loop

	// Remap intersected triangle index, and store the result.

	if (hitIndex != -1) {
		// remapping tri indices delayed until this point for performance reasons
		// (slow global memory lookup in de gpuTriIndices array) because multiple triangles per node can potentially be hit
		hitIndex = gpuTriIndices[hitIndex];
	}

	hitTriIdx = hitIndex;
	hitdistance = hitT;

	return 0;
}

int intersectBVHandTriangles(const float4 rayorig, const float4 raydir,
	const in Buffer<float4> gpuNodes, const in Buffer<float4> gpuTriWoops, const in Buffer<int> gpuTriIndices,
	inout int hitTriIdx, inout float hitdistance, inout int debugbingo, inout float3 trinormal, bool anyHit)
{
	// assign a CUDA thread to every pixel by using the threadIndex
	// global threadId, see richiesams blogspot
	//int thread_index = (blockIdx.x + blockIdx.y * gridDim.x) * (blockDim.x * blockDim.y) + (threadIdx.y * blockDim.x) + threadIdx.x;

	///////////////////////////////////////////
	//// FERMI / KEPLER KERNEL
	///////////////////////////////////////////

	// BVH layout Compact2 for Kepler, Ccompact for Fermi (nodeOffsetSizeDiv is different)
	// void CudaBVH::createCompact(const BVH& bvh, int nodeOffsetSizeDiv)
	// createCompact(bvh,16); for Compact2
	// createCompact(bvh,1); for Compact

	int traversalStack[STACK_SIZE];

	// Live state during traversal, stored in registers.

	int		rayidx;		// not used, can be removed
	float   origx, origy, origz;    // Ray origin.
	float   dirx, diry, dirz;       // Ray direction.
	float   tmin;                   // t-value from which the ray starts. Usually 0.
	float   idirx, idiry, idirz;    // 1 / ray direction
	float   oodx, oody, oodz;       // ray origin / ray direction

	int idxStak; //char* stackPtr;               // Current position in traversal stack.
	int     leafAddr;               // If negative, then first postponed leaf, non-negative if no leaf (innernode).
	int     nodeAddr;
	int     hitIndex;               // Triangle index of the closest intersection, -1 if none.
	float   hitT;                   // t-value of the closest intersection.
	// Kepler kernel only
	//int     leafAddr2;              // Second postponed leaf, non-negative if none.  
	//int     nodeAddr = EntrypointSentinel; // Non-negative: current internal node, negative: second postponed leaf.

	//int threadId1; // ipv rayidx

	// Initialize (stores local variables in registers)
	{
		// Pick ray index.

		//threadId1 = threadIdx.x + blockDim.x * (threadIdx.y + blockDim.y * (blockIdx.x + gridDim.x * blockIdx.y));


		// Fetch ray.

		// required when tracing ray batches
		// float4 o = rays[rayidx * 2 + 0];  
		// float4 d = rays[rayidx * 2 + 1];
		//__shared__ volatile int nextRayArray[MaxBlockHeight]; // Current ray index in global buffer.

		origx = rayorig.x;
		origy = rayorig.y;
		origz = rayorig.z;

		dirx = raydir.x;
		diry = raydir.y;
		dirz = raydir.z;
		tmin = rayorig.w;

		// ooeps is very small number, used instead of raydir xyz component when that component is near zero
		float ooeps = FLT_MIN; // exp2(-80.0f); // Avoid div by zero, returns 1/2^80, an extremely small number
		float ooeps_x = raydir.x >= 0 ? ooeps : -ooeps;
		float ooeps_y = raydir.y >= 0 ? ooeps : -ooeps;
		float ooeps_z = raydir.z >= 0 ? ooeps : -ooeps;
		idirx = 1.0f / (abs(raydir.x) > ooeps ? raydir.x : ooeps_x); // inverse ray direction
		idiry = 1.0f / (abs(raydir.y) > ooeps ? raydir.y : ooeps_y); // inverse ray direction
		idirz = 1.0f / (abs(raydir.z) > ooeps ? raydir.z : ooeps_z); // inverse ray direction
		oodx = origx * idirx;  // ray origin / ray direction
		oody = origy * idiry;  // ray origin / ray direction
		oodz = origz * idirz;  // ray origin / ray direction

		// Setup traversal + initialisation

		traversalStack[0] = EntrypointSentinel; // Bottom-most entry. 0x76543210 (1985229328 in decimal)
		idxStak = 0;//stackPtr = (char*)&traversalStack[0]; // point stackPtr to bottom of traversal stack = EntryPointSentinel
		leafAddr = 0;   // No postponed leaf.
		nodeAddr = 0;   // Start from the root.
		hitIndex = -1;  // No triangle intersected so far.
		hitT = raydir.w; // tmax  
	}

	// Traversal loop.
	[allow_uav_condition]
	while ((uint)nodeAddr != EntrypointSentinel)
	{
		// Traverse internal nodes until all SIMD lanes have found a leaf.

		bool searchingLeaf = true; // required for warp efficiency
		[allow_uav_condition]
		while (nodeAddr >= 0 && (uint)nodeAddr != EntrypointSentinel)
		{
			// Fetch AABBs of the two child nodes.

			// nodeAddr is an offset in number of bytes (char) in gpuNodes array

			// float4* ptr = (float4*)((char*)gpuNodes + nodeAddr);
			int nodeIdx = nodeAddr >> 4;
			float4 n0xy = gpuNodes[nodeIdx];// ptr[0]; // childnode 0, xy-bounds (c0.lo.x, c0.hi.x, c0.lo.y, c0.hi.y)		
			float4 n1xy = gpuNodes[nodeIdx + 1];//ptr[1]; // childnode 1, xy-bounds (c1.lo.x, c1.hi.x, c1.lo.y, c1.hi.y)		
			float4 nz = gpuNodes[nodeIdx + 2];//ptr[2]; // childnode 0 and 1, z-bounds (c0.lo.z, c0.hi.z, c1.lo.z, c1.hi.z)		
			// ptr[3] contains indices to 2 childnodes in case of innernode, see below
			// (childindex = size of array during building, see CudaBVH.cpp)

			// compute ray intersections with BVH node bounding box

			/// RAY BOX INTERSECTION
			// Intersect the ray against the child nodes.

			float c0lox = n0xy.x * idirx - oodx; // n0xy.x = c0.lo.x, child 0 minbound x
			float c0hix = n0xy.y * idirx - oodx; // n0xy.y = c0.hi.x, child 0 maxbound x
			float c0loy = n0xy.z * idiry - oody; // n0xy.z = c0.lo.y, child 0 minbound y
			float c0hiy = n0xy.w * idiry - oody; // n0xy.w = c0.hi.y, child 0 maxbound y
			float c0loz = nz.x * idirz - oodz; // nz.x   = c0.lo.z, child 0 minbound z
			float c0hiz = nz.y * idirz - oodz; // nz.y   = c0.hi.z, child 0 maxbound z
			float c1loz = nz.z * idirz - oodz; // nz.z   = c1.lo.z, child 1 minbound z
			float c1hiz = nz.w * idirz - oodz; // nz.w   = c1.hi.z, child 1 maxbound z
			float c0min = spanBeginKepler(c0lox, c0hix, c0loy, c0hiy, c0loz, c0hiz, tmin); // Tesla does max4(min, min, min, tmin)
			float c0max = spanEndKepler(c0lox, c0hix, c0loy, c0hiy, c0loz, c0hiz, hitT); // Tesla does min4(max, max, max, tmax)
			float c1lox = n1xy.x * idirx - oodx; // n1xy.x = c1.lo.x, child 1 minbound x
			float c1hix = n1xy.y * idirx - oodx; // n1xy.y = c1.hi.x, child 1 maxbound x
			float c1loy = n1xy.z * idiry - oody; // n1xy.z = c1.lo.y, child 1 minbound y
			float c1hiy = n1xy.w * idiry - oody; // n1xy.w = c1.hi.y, child 1 maxbound y
			float c1min = spanBeginKepler(c1lox, c1hix, c1loy, c1hiy, c1loz, c1hiz, tmin);
			float c1max = spanEndKepler(c1lox, c1hix, c1loy, c1hiy, c1loz, c1hiz, hitT);

			// ray box intersection boundary tests:

			float ray_tmax = 1e20;
			bool traverseChild0 = (c0min <= c0max) && (c0min >= tmin) && (c0min <= ray_tmax);
			bool traverseChild1 = (c1min <= c1max) && (c1min >= tmin) && (c1min <= ray_tmax);

			// Neither child was intersected => pop stack.

			if (!traverseChild0 && !traverseChild1)
			{
				nodeAddr = traversalStack[idxStak];// *(int*)stackPtr; // fetch next node by popping the stack 
				idxStak -= 1; // stackPtr -= 4; // popping decrements stackPtr by 4 bytes (because stackPtr is a pointer to char)   
			}

			// Otherwise, one or both children intersected => fetch child pointers.

			else
			{
				//int2 cnodes = *(int2*) & ptr[3];
				float4 nodef4 = gpuNodes[nodeIdx + 3];
				int2 cnodes = int2(asint(nodef4.x), asint(nodef4.y));// = *(int2*) & ptr[3];
			
				// set nodeAddr equal to intersected childnode index (or first childnode when both children are intersected)
				nodeAddr = (traverseChild0) ? cnodes.x : cnodes.y;

				// Both children were intersected => push the farther one on the stack.

				if (traverseChild0 && traverseChild1) // store closest child in nodeAddr, swap if necessary
				{
					if (c1min < c0min)
						swap2(nodeAddr, cnodes.y);
					idxStak += 1; //stackPtr += 4;  // pushing increments stack by 4 bytes (stackPtr is a pointer to char)
					traversalStack[idxStak] = cnodes.y; //*(int*)stackPtr = cnodes.y; // push furthest node on the stack
				}
			}

			// First leaf => postpone and continue traversal.
			// leafnodes have a negative index to distinguish them from inner nodes
			// if nodeAddr less than 0 -> nodeAddr is a leaf
			if (nodeAddr < 0 && leafAddr >= 0)
			{
				searchingLeaf = false; // required for warp efficiency
				leafAddr = nodeAddr;
				nodeAddr = traversalStack[idxStak];//*(int*)stackPtr;  // pops next node from stack
				idxStak -= 1;// stackPtr -= 4;  // decrements stackptr by 4 bytes (because stackPtr is a pointer to char)
			}

			// All SIMD lanes have found a leaf => process them.

			// to increase efficiency, check if all the threads in a warp have found a leaf before proceeding to the
			// ray/triangle intersection routine
			// this bit of code requires PTX (CUDA assembly) code to work properly

			// if (!__any(searchingLeaf)) -> "__any" keyword: if none of the threads is searching a leaf, in other words
			// if all threads in the warp found a leafnode, then break from while loop and go to triangle intersection

			//if(!__any(leafAddr >= 0))     
			//    break;

			// if (!__any(searchingLeaf))
			//	break;    /// break from while loop and go to code below, processing leaf nodes

			// NOTE: inline PTX implementation of "if(!__any(leafAddr >= 0)) break;".
			// tried everything with CUDA 4.2 but always got several redundant instructions.

			//unsigned int mask; // mask replaces searchingLeaf in PTX code
			if (leafAddr < 0) {
				break;
			}

			//asm("{\n"
			//"   .reg .pred p;               \n"
			//	"setp.ge.s32        p, %1, 0;   \n"
			//	"vote.ballot.b32    %0,p;       \n"
			//	"}"
			//	: "=r"(mask)
			//	: "r"(leafAddr));

			//if (!mask)
			//	break;
		}


		///////////////////////////////////////////
		/// TRIANGLE INTERSECTION
		//////////////////////////////////////

		// Process postponed leaf nodes.
		[loop]
		while (leafAddr < 0)  /// if leafAddr is negative, it points to an actual leafnode (when positive or 0 it's an innernode)
		{
			// Intersect the ray against each triangle using Sven Woop's algorithm.
			// Woop ray triangle intersection: Woop triangles are unit triangles. Each ray
			// must be transformed to "unit triangle space", before testing for intersection
			[loop]
			for (int triAddr = ~leafAddr;; triAddr += 3)  // triAddr is index in triWoop array (and bitwise complement of leafAddr)
			{ // no defined upper limit for loop, continues until leaf terminator code 0x80000000 is encountered

				// Read first 16 bytes of the triangle.
				// fetch first precomputed triangle edge
				float4 v00 = gpuTriWoops[triAddr];// tex1Dfetch(triWoopTexture, triAddr);

				// End marker 0x80000000 (negative zero) => all triangles in leaf processed --> terminate
				if (asuint(v00.x) == 0x80000000)
					break;

				// Compute and check intersection t-value (hit distance along ray).
				float Oz = v00.w - origx * v00.x - origy * v00.y - origz * v00.z;   // Origin z
				float invDz = 1.0f / (dirx * v00.x + diry * v00.y + dirz * v00.z);  // inverse Direction z
				float t = Oz * invDz;

				if (t > tmin && t < hitT)
				{
					// Compute and check barycentric u.

					// fetch second precomputed triangle edge
					float4 v11 = gpuTriWoops[triAddr + 1];// tex1Dfetch(triWoopTexture, triAddr + 1);
					float Ox = v11.w + origx * v11.x + origy * v11.y + origz * v11.z;  // Origin.x
					float Dx = dirx * v11.x + diry * v11.y + dirz * v11.z;  // Direction.x
					float u = Ox + t * Dx; /// parametric equation of a ray (intersection point)

					if (u >= -0.0001f && u <= 1.0001f)
					//if (u >= 0.0001f && u <= 0.9999f)
					{
						// Compute and check barycentric v.

						// fetch third precomputed triangle edge
						float4 v22 = gpuTriWoops[triAddr + 2];// tex1Dfetch(triWoopTexture, triAddr + 2);
						float Oy = v22.w + origx * v22.x + origy * v22.y + origz * v22.z;
						float Dy = dirx * v22.x + diry * v22.y + dirz * v22.z;
						float v = Oy + t * Dy;

						if (v >= -0.0001f && u + v <= 1.0001f)
						//if (v >= 0.0001f && u + v <= 0.9999f)
						{
							// We've got a hit!
							// Record intersection.

							hitT = t;
							hitIndex = triAddr; // store triangle index for shading

							// Closest intersection not required => terminate.
							if (anyHit)  // only true for shadow rays
							{
								nodeAddr = EntrypointSentinel;
								break;
							}

							// compute normal vector by taking the cross product of two edge vectors
							// because of Woop transformation, only one set of vectors works

							// assume... CCW 
							//trinormal = cross(float3(v22.x, v22.y, v22.z), float3(v11.x, v11.y, v11.z));  // works
							trinormal = cross(float3(v11.x, v11.y, v11.z), float3(v22.x, v22.y, v22.z));  // works
							//trinormal = float3(100, 100, 100);
						}
					}
				}
			} // end triangle intersection

			// Another leaf was postponed => process it as well.

			leafAddr = nodeAddr;
			if (nodeAddr < 0)    // nodeAddr is an actual leaf when < 0
			{
				nodeAddr = traversalStack[idxStak];//*(int*)stackPtr;  // pop stack
				idxStak -= 1;// stackPtr -= 4;               // decrement with 4 bytes to get the next int (stackPtr is char*)
			}
		} // end leaf/triangle intersection loop
	} // end traversal loop (AABB and triangle intersection)

	// Remap intersected triangle index, and store the result.

	if (hitIndex != -1) {
		hitIndex = gpuTriIndices[hitIndex];// tex1Dfetch(triIndicesTexture, hitIndex);
		// remapping tri indices delayed until this point for performance reasons
		// (slow texture memory lookup in de triIndicesTexture) because multiple triangles per node can potentially be hit
	}

	hitTriIdx = hitIndex;
	hitdistance = hitT;

	return 0;
}

#else // BVH_LEGACY

struct RayDesc
{
	float3 Origin;
	float TMin;
	float3 Direction;
	float TMax;
};

struct RayHit
{
	float2 bary;
	float distance;
	PrimitiveID primitiveID;
	bool is_backface;
};

inline RayHit CreateRayHit()
{
	RayHit hit;
	hit.bary = 0;
	hit.distance = FLT_MAX;
	hit.is_backface = false;
	//hit.primitiveID.init();
	return hit;
}

/*
struct Surface
{
	// Fill these yourself:
	float3 P;
	float3 N;

	float3 data0;
	float3 data1;
	float3 data2;

	float2 bary;

	inline void init()
	{
		P = 0;
		N = 0;
	}

	bool preload_internal(PrimitiveID prim)
	{
		uint startIndex = prim.primitiveIndex * 3;
		if (startIndex >= primitiveCounterBuffer.Load(0))
			return false;
		uint i0 = indexBuffer[startIndex + 0];
		uint i1 = indexBuffer[startIndex + 1];
		uint i2 = indexBuffer[startIndex + 2];

		uint vertexStride = g_cbPobj.num_letters;

		uint vtxIndex0 = i0 * vertexStride; //pushBVH[0].vertexStride;
		uint vtxIndex1 = i1 * vertexStride; //pushBVH[0].vertexStride;
		uint vtxIndex2 = i2 * vertexStride; //pushBVH[0].vertexStride;
		data0 = float3(vertexBuffer[vtxIndex0 + 0], vertexBuffer[vtxIndex0 + 1], vertexBuffer[vtxIndex0 + 2]);
		data1 = float3(vertexBuffer[vtxIndex1 + 0], vertexBuffer[vtxIndex1 + 1], vertexBuffer[vtxIndex1 + 2]);
		data2 = float3(vertexBuffer[vtxIndex2 + 0], vertexBuffer[vtxIndex2 + 1], vertexBuffer[vtxIndex2 + 2]);
		return true;
	}

	bool load(in PrimitiveID prim, in float2 barycentrics)
	{
		if (!preload_internal(prim))
			return false;

		bary = barycentrics;

		P = attribute_at_bary(data0, data1, data2, bary);
		N = cross(data1 - data0, data2 - data0);
		N = normalize(N);

		return true;
	}

};
/**/

#ifndef RAYTRACE_STACKSIZE
#define RAYTRACE_STACKSIZE 64
#endif // RAYTRACE_STACKSIZE

static const float eps = 1e-6;

inline void IntersectTriangle(
	in RayDesc ray,
	inout RayHit bestHit,
	in BVHPrimitive prim
)
{
	float3 v0v1 = prim.v1() - prim.v0();
	float3 v0v2 = prim.v2() - prim.v0();
	float3 pvec = cross(ray.Direction, v0v2);
	float det = dot(v0v1, pvec);

	// ray and triangle are parallel if det is close to 0
	if (abs(det) < eps)
		return;
	float invDet = rcp(det);

	float3 tvec = ray.Origin - prim.v0();
	float u = dot(tvec, pvec) * invDet;
	if (u < 0 || u > 1)
		return;

	float3 qvec = cross(tvec, v0v1);
	float v = dot(ray.Direction, qvec) * invDet;
	if (v < 0 || u + v > 1)
		return;

	float t = dot(v0v2, qvec) * invDet;

	if (t >= ray.TMin && t <= bestHit.distance)
	{
		RayHit hit;
		hit.distance = t;
		hit.primitiveID = prim.primitiveID();
		hit.bary = float2(u, v);
		hit.is_backface = det < 0;

		bestHit = hit;
	}
}

inline bool IntersectNode(
	in RayDesc ray,
	in BVHNode box,
	in float3 rcpDirection,
	in float primitive_best_distance
)
{
	// check if the ray Origin is inside the node
	//bool originInside = (ray.Origin.x >= box.min.x && ray.Origin.x <= box.max.x &&
	//	ray.Origin.y >= box.min.y && ray.Origin.y <= box.max.y &&
	//	ray.Origin.z >= box.min.z && ray.Origin.z <= box.max.z);
	//float3 check = (ray.Origin - box.max) * (ray.Origin - box.min);
	//bool originInside = check.x <= 0 && check.y <= 0 && check.z <= 0;
	//if (originInside) {
	//	return true; // always TRUE intersection when the ray Origin is inside the node
	//}

	const float t0 = (box.min.x - ray.Origin.x) * rcpDirection.x;
	const float t1 = (box.max.x - ray.Origin.x) * rcpDirection.x;
	const float t2 = (box.min.y - ray.Origin.y) * rcpDirection.y;
	const float t3 = (box.max.y - ray.Origin.y) * rcpDirection.y;
	const float t4 = (box.min.z - ray.Origin.z) * rcpDirection.z;
	const float t5 = (box.max.z - ray.Origin.z) * rcpDirection.z;
	const float tmin = max(max(min(t0, t1), min(t2, t3)), min(t4, t5)); // close intersection point's distance on ray
	const float tmax = min(min(max(t0, t1), max(t2, t3)), max(t4, t5)); // far intersection point's distance on ray

	return (tmax < -eps || tmin > tmax + eps || tmin > primitive_best_distance) ? false : true;
	//if (tmax < 0 || tmin > tmax || tmin > primitive_best_distance) // this also checks if a better primitive was already hit or not and skips if yes
	//{
	//	return false;
	//}
	//else
	//{
	//	return true;
	//}
}

// Returns the closest hit primitive if any (useful for generic trace). If nothing was hit, then rayHit.distance will be equal to FLT_MAX
inline RayHit TraceRay_Closest(RayDesc ray, uint groupIndex = 0)
{
	if (ray.Origin.x == 0) ray.Origin.x = 0.0001234f; // trick... for avoiding zero block skipping error
	if (ray.Origin.y == 0) ray.Origin.y = 0.0001234f; // trick... for avoiding zero block skipping error
	if (ray.Origin.z == 0) ray.Origin.z = 0.0001234f; // trick... for avoiding zero block skipping error
	if (ray.Direction.x == 0) ray.Direction.x = 0.0001234f; // trick... for avoiding zero block skipping error
	if (ray.Direction.y == 0) ray.Direction.y = 0.0001234f; // trick... for avoiding zero block skipping error
	if (ray.Direction.z == 0) ray.Direction.z = 0.0001234f; // trick... for avoiding zero block skipping error

	const float3 rcpDirection = rcp(ray.Direction);

	RayHit bestHit = CreateRayHit();

#ifndef RAYTRACE_STACK_SHARED
	// Emulated stack for tree traversal:
	uint stack[RAYTRACE_STACKSIZE][1];
#endif // RAYTRACE_STACK_SHARED
	uint stackpos = 0;

	const uint primitiveCount = primitiveCounterBuffer.Load(0);
	const uint leafNodeOffset = primitiveCount - 1;

	// push root node
	stack[stackpos++][groupIndex] = 0;

	uint count = 0;

	[loop]
	while (stackpos > 0 && count < 5000u) {
		// pop untraversed node
		const uint nodeIndex = stack[--stackpos][groupIndex];

		//BVHNode node = bvhNodeBuffer.Load<BVHNode>(nodeIndex * sizeof(BVHNode));
		BVHNode node = bvhNodeBuffer[nodeIndex];

		if (IntersectNode(ray, node, rcpDirection, bestHit.distance))
		{
			if (nodeIndex >= leafNodeOffset)
			{
				// Leaf node
				const uint primitiveID = node.LeftChildIndex;
				//const BVHPrimitive prim = primitiveBuffer.Load<BVHPrimitive>(primitiveID * sizeof(BVHPrimitive));
				const BVHPrimitive prim = primitiveBuffer[primitiveID];
				//if (prim.flags & mask)
				{
					IntersectTriangle(ray, bestHit, prim);
				}
			}
			else
			{
				// Internal node
				if (stackpos < RAYTRACE_STACKSIZE - 1)
				{
					// push left child
					stack[stackpos++][groupIndex] = node.LeftChildIndex;
					// push right child
					stack[stackpos++][groupIndex] = node.RightChildIndex;
				}
				else
				{
					// stack overflow, terminate
					break;
				}
			}

		}
		count++;

	} //while (stackpos > 0);


	return bestHit;
}

#endif // BVH_LEGACY

#if DX10_0 == 1
PS_FILL_OUTPUT_NO_DS ThickSlicePathTracer(VS_OUTPUT input)
#else
[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void ThickSlicePathTracer(uint3 DTid : SV_DispatchThreadID, uint groupIndex_ : SV_GroupIndex)
#endif
{
	float4 vis_out = 0;
	float depth_out = 0;

#if DX10_0 == 1
	PS_FILL_OUTPUT_NO_DS out_ps;
	//out_ps.ds_z = 0;
#if PICKING == 1
	int2 ss_xy = int2(g_cbCamState.iSrCamDummy__1 & 0xFFFF, g_cbCamState.iSrCamDummy__1 >> 16);
	int2 prev_load_index = int2(0, 0);
#else
	int2 ss_xy = int2(input.f4PosSS.xy);
	int2 prev_load_index = ss_xy;
#endif

	out_ps.color = prev_fragment_vis[prev_load_index];
#if PICKING != 1
	out_ps.depthcs = prev_fragment_zdepth[prev_load_index];
	uint wildcard_v = asuint(out_ps.depthcs);
	if (wildcard_v == WILDCARD_DEPTH_OUTLINE)
		__EXIT;
	out_ps.depthcs = asfloat(OUTSIDE_PLANE);
#endif

#else

#if PICKING == 1
	int2 ss_xy = int2(g_cbCamState.iSrCamDummy__1 & 0xFFFF, g_cbCamState.iSrCamDummy__1 >> 16);
	if (DTid.x != 0 || DTid.y != 0)
		__EXIT;
#else
	int2 ss_xy = int2(DTid.xy);
#endif

	// do not compute 1st hit surface separately
	if (DTid.x >= g_cbCamState.rt_width || DTid.y >= g_cbCamState.rt_height || g_cbPobj.alpha < 0.001f)
		__EXIT;

	float fvPrev = fragment_zdepth[ss_xy];// asfloat(ConvertFloat4ToUInt(v_rgba));
	if (asuint(fvPrev) == WILDCARD_DEPTH_OUTLINE)
		__EXIT;

	if (fragment_counter[ss_xy] == WILDCARD_DEPTH_OUTLINE_DIRTY)
	{
		fragment_counter[ss_xy] = 1;
		fragment_zdepth[ss_xy] = asfloat(WILDCARD_DEPTH_OUTLINE);
		__EXIT;
	}
	
	fragment_zdepth[ss_xy] = asfloat(OUTSIDE_PLANE);
	//__EXIT;

	const uint k_value = g_cbCamState.k_value;
	uint bytes_per_frag = 4 * NUM_ELES_PER_FRAG;
	uint pixel_id = ss_xy.y * g_cbCamState.rt_width + ss_xy.x;
	uint bytes_frags_per_pixel = k_value * bytes_per_frag;
	uint addr_base = pixel_id * bytes_frags_per_pixel;
#endif

	bool disableSolidFill = BitCheck(g_cbPobj.pobj_flag, 6);

	// Image Plane's Position and Camera State //
#if CURVEDPLANE == 0
	float3 pos_ip_ss = float3(ss_xy, 0.0f);
	float3 pos_ip_ws = TransformPoint(pos_ip_ss, g_cbCamState.mat_ss2ws);
	float3 ray_dir_unit_ws = g_cbCamState.dir_view_ws;
	if (g_cbCamState.cam_flag & 0x1)
		ray_dir_unit_ws = pos_ip_ws - g_cbCamState.pos_cam_ws;
	ray_dir_unit_ws = normalize(ray_dir_unit_ws);

#else
	int2 i2SizeBuffer = int2(g_cbCamState.rt_width, g_cbCamState.rt_height);
	int iPlaneSizeX = g_cbCurvedSlicer.numCurvePoints;
	float3 f3VecSampleUpWS = g_cbCurvedSlicer.planeUp;
	//float fPlanePitch = length(f3VecSampleUpWS);
	bool bIsRightSide = BitCheck(g_cbCurvedSlicer.flag, 0);
	float3 f3PosTopLeftCOS = g_cbCurvedSlicer.posTopLeftCOS;
	float3 f3PosTopRightCOS = g_cbCurvedSlicer.posTopRightCOS;
	float3 f3PosBottomLeftCOS = g_cbCurvedSlicer.posBottomLeftCOS;
	float3 f3PosBottomRightCOS = g_cbCurvedSlicer.posBottomRightCOS;
	float fPlaneThickness = g_cbCurvedSlicer.thicknessPlane;
	float fPlaneSizeY = g_cbCurvedSlicer.planeHeight;
	float fPlaneCenterY = fPlaneSizeY * 0.5f;
	const float fThicknessPosition = 0;
	const float merging_beta = 1.0;

	int2 cip_xy = ss_xy;
	// i ==> cip_xy.x
	float fRatio0 = (float)((i2SizeBuffer.x - 1) - cip_xy.x) / (float)(i2SizeBuffer.x - 1);
	float fRatio1 = (float)(cip_xy.x) / (float)(i2SizeBuffer.x - 1);

	float2 f2PosInterTopCOS, f2PosInterBottomCOS, f2PosSampleCOS;
	f2PosInterTopCOS.x = fRatio0 * f3PosTopLeftCOS.x + fRatio1 * f3PosTopRightCOS.x;
	f2PosInterTopCOS.y = fRatio0 * f3PosTopLeftCOS.y + fRatio1 * f3PosTopRightCOS.y;

	if (f2PosInterTopCOS.x < 0 || f2PosInterTopCOS.x >= (float)(iPlaneSizeX - 1))
		__EXIT;

	int iPosSampleCOS = (int)floor(f2PosInterTopCOS.x);
	float fInterpolateRatio = f2PosInterTopCOS.x - iPosSampleCOS;

	int iMinMaxAddrX = min(max(iPosSampleCOS, 0), iPlaneSizeX - 1);
	int iMinMaxAddrNextX = min(max(iPosSampleCOS + 1, 0), iPlaneSizeX - 1);

	float3 f3PosSampleWS_C0 = buf_curvePoints[iMinMaxAddrX];
	float3 f3PosSampleWS_C1 = buf_curvePoints[iMinMaxAddrNextX];
	float3 f3PosSampleWS_C_ = f3PosSampleWS_C0 * (1.f - fInterpolateRatio) + f3PosSampleWS_C1 * fInterpolateRatio;

	float3 f3VecSampleTangentWS_0 = buf_curveTangents[iMinMaxAddrX];
	float3 f3VecSampleTangentWS_1 = buf_curveTangents[iMinMaxAddrNextX];
	float3 f3VecSampleTangentWS = normalize(f3VecSampleTangentWS_0 * (1.f - fInterpolateRatio) + f3VecSampleTangentWS_1 * fInterpolateRatio);
	float3 f3VecSampleViewWS = normalize(cross(f3VecSampleUpWS, f3VecSampleTangentWS));

	if (bIsRightSide)
		f3VecSampleViewWS *= -1.f;
	float3 f3PosSampleWS_C = f3PosSampleWS_C_ + f3VecSampleViewWS * (fThicknessPosition - fPlaneThickness * 0.5f);
	//f3VecSampleUpWS *= fPlanePitch; // already multiplied

	f2PosInterBottomCOS.x = fRatio0 * f3PosBottomLeftCOS.x + fRatio1 * f3PosBottomRightCOS.x;
	f2PosInterBottomCOS.y = fRatio0 * f3PosBottomLeftCOS.y + fRatio1 * f3PosBottomRightCOS.y;

	// j ==> cip_xy.y
	float fRatio0Y = (float)((i2SizeBuffer.y - 1) - cip_xy.y) / (float)(i2SizeBuffer.y - 1);
	float fRatio1Y = (float)(cip_xy.y) / (float)(i2SizeBuffer.y - 1);

	f2PosSampleCOS.x = fRatio0Y * f2PosInterTopCOS.x + fRatio1Y * f2PosInterBottomCOS.x;
	f2PosSampleCOS.y = fRatio0Y * f2PosInterTopCOS.y + fRatio1Y * f2PosInterBottomCOS.y;

	if (f2PosSampleCOS.y < 0 || f2PosSampleCOS.y > fPlaneSizeY)
		__EXIT;

	// start position //
	//vmfloat3 f3PosSampleWS = f3PosSampleWS_C + f3VecSampleUpWS * (f2PosSampleCOS.y - fPlaneCenterY)
	//	+ f3VecSampleViewWS * fStepLength * (float)m;
	float3 pos_ip_ws = f3PosSampleWS_C + f3VecSampleUpWS * (f2PosSampleCOS.y - fPlaneCenterY);
	float3 ray_dir_unit_ws = f3VecSampleViewWS; // already normalized
#endif
	////////////////////////////
	int hitTriIdx = -1;
	//int bestTriIdx = -1;
	//int geomtype = -1;
	float hitDistance = 1e20;
	//float scene_t = 1e20;
	//float3 objcol = float3(0, 0, 0);
	//float3 emit = float3(0, 0, 0);
	//float3 hitpoint; // intersection point
	//float3 n; // normal
	//float3 nl; // oriented normal
	//float3 nextdir; // ray direction of next path segment
#ifdef BVH_LEGACY
	float3 trinormal = float3(0, 0, 0);
#endif
	float ray_tmin = 0.0001f; // MAGIC VALUE
	float ray_tmax = 1e20; // use thickness!!

	// intersect all triangles in the scene stored in BVH
	int debugbingo = 0;
	float planeThickness = g_cbCamState.far_plane;// g_cbCurvedSlicer.thicknessPlane;// g_cbCamState.far_plane; // WS

#if DX10_0 == 1
#else
	if (disableSolidFill && planeThickness > 0) 
#endif
	{
		// planeThickness > 0
		pos_ip_ws = pos_ip_ws + ray_dir_unit_ws * planeThickness * 0.5f;
		planeThickness = 0;
		//return;
	}

	if (planeThickness == 0)
	{
		// this is for rubust signed distance computation
		ray_tmin = 0;
	}

	// clipping //
	if (g_cbClipInfo.clip_flag & 0x1) {
		if (dot(g_cbClipInfo.vec_clipplane, pos_ip_ws - g_cbClipInfo.pos_clipplane) > 0) __EXIT;
	}
	if (g_cbClipInfo.clip_flag & 0x2) {
		if (!IsInsideClipBox(pos_ip_ws, g_cbClipInfo.mat_clipbox_ws2bs)) __EXIT;
	}

	//pos_ip_ws = float3(0, 0, 0);
	//ray_dir_unit_ws = float3(0, 1, 0);
	float3 ray_orig_ws = pos_ip_ws;
	float3 ray_orig_os = TransformPoint(ray_orig_ws, g_cbPobj.mat_ws2os);

	float3 end_pos_ws = ray_orig_ws + ray_dir_unit_ws * planeThickness;
	float3 end_pos_os = TransformPoint(end_pos_ws, g_cbPobj.mat_ws2os);
	float planeThickness_os = length(ray_orig_os - end_pos_os);

	float3 r0 = TransformPoint(float3(0, 0, 0), g_cbPobj.mat_ws2os);
	float3 r1 = TransformPoint(ray_dir_unit_ws, g_cbPobj.mat_ws2os);
	//float3 ray_dir_unit_os = normalize(TransformVector(ray_dir_unit_ws, g_cbPobj.mat_ws2os));
	float3 ray_dir_unit_os = normalize(r1 - r0);
	if (ray_orig_os.z == 0) ray_orig_os.z = 0.0001234f; // trick... for avoiding zero block skipping error
	if (ray_orig_os.y == 0) ray_orig_os.y = 0.0001234f; // trick... for avoiding zero block skipping error
	if (ray_orig_os.x == 0) ray_orig_os.x = 0.0001234f; // trick... for avoiding zero block skipping error
	if (ray_dir_unit_os.z == 0) ray_dir_unit_os.z = 0.0001234f; // trick... for avoiding zero block skipping error
	if (ray_dir_unit_os.y == 0) ray_dir_unit_os.y = 0.0001234f; // trick... for avoiding zero block skipping error
	if (ray_dir_unit_os.x == 0) ray_dir_unit_os.x = 0.0001234f; // trick... for avoiding zero block skipping error
	
	bool isInsideOnPlane = false;
	int checkCountInsideHorizon = 0;
	int checkCountInsideVertical = 0;

	float minDistOnPlane = FLT_MAX;
#if CURVEDPLANE == 0
	float4x4 mat_os2ss = mul(g_cbCamState.mat_ws2ss, g_cbPobj.mat_os2ws);
#else
	//f3PosSampleWS_C_

	int nextsampleIdx = cip_xy.x + 1;
	float fRatio0_nbr = (float)((i2SizeBuffer.x - 1) - nextsampleIdx) / (float)(i2SizeBuffer.x - 1);
	float fRatio1_nbr = (float)(nextsampleIdx) / (float)(i2SizeBuffer.x - 1);
	float posInterTopCOS_nbr = fRatio0_nbr * f3PosTopLeftCOS.x + fRatio1_nbr * f3PosTopRightCOS.x;
	int iPosSampleCOS_nbr = (int)floor(posInterTopCOS_nbr);
	int iMinMaxAddrX_nbr = min(max(iPosSampleCOS_nbr, 0), iPlaneSizeX - 1);
	int iMinMaxAddrNextX_nbr = min(max(iPosSampleCOS_nbr + 1, 0), iPlaneSizeX - 1);

	float3 f3PosSampleWS_C0_nbr = buf_curvePoints[iMinMaxAddrX_nbr];
	float3 f3PosSampleWS_C1_nbr = buf_curvePoints[iMinMaxAddrNextX_nbr];
	float fInterpolateRatio_nbr = posInterTopCOS_nbr - iPosSampleCOS_nbr;
	float3 f3PosSampleWS_C_nbr = f3PosSampleWS_C0_nbr * (1.f - fInterpolateRatio_nbr) + f3PosSampleWS_C1_nbr * fInterpolateRatio_nbr;

	//sif (iPosSampleCOS_nbr < 0 || iPosSampleCOS_nbr >= iPlaneSizeX)
	//	__EXIT;
	float pixelSpace = length(f3PosSampleWS_C_ - f3PosSampleWS_C_nbr) * 1.5; // 1.5 for heuristic correction of curve
	//pixelSpace = length(f3VecSampleUpWS);
#endif
	// safe inside test (up- and down-side)//
	{
#if CURVEDPLANE == 0
		float3 upPos0 = TransformPoint(float3(ss_xy, 0), g_cbCamState.mat_ss2ws);
		float3 upPos1 = TransformPoint(float3(ss_xy + float2(0, -1), 0), g_cbCamState.mat_ss2ws);
		float3 updir = normalize(TransformVector(upPos1 - upPos0, g_cbPobj.mat_ws2os));
#else
		//f3VecSampleUpWS
		float3 updir = normalize(TransformVector(f3VecSampleUpWS, g_cbPobj.mat_ws2os));// / fPlanePitch; // unit vector
#endif
		float4 test_rayorig = float4(ray_orig_os, ray_tmin);
		float4 test_raydir = float4(updir, ray_tmax);

#ifdef BVH_LEGACY
		intersectBVHandTriangles(test_rayorig, test_raydir, buf_gpuNodes, buf_gpuTriWoops, buf_gpuTriIndices, hitTriIdx, hitDistance, debugbingo, trinormal, false);
#else
		RayDesc ray;
		ray.Origin = ray_orig_os;
		ray.Direction = updir;
		ray.TMin = ray_tmin;
		ray.TMax = ray_tmax;

		RayHit hit = TraceRay_Closest(ray);
		hitTriIdx = hit.distance >= FLT_MAX - 1 ? -1 : hit.primitiveID.primitiveIndex;
		hitDistance = hit.distance;
#endif

		if (hitTriIdx >= 0) {
#ifdef BVH_LEGACY
			bool localInside = dot(trinormal, test_raydir.xyz) > 0;
#else
			bool localInside = hit.is_backface;
#endif
#if PICKING == 1 
			if (localInside) checkCountInsideVertical++;
#else
			isInsideOnPlane = localInside;
#endif
			
			float3 posHitOS = test_rayorig.xyz + hitDistance * test_raydir.xyz;
#if CURVEDPLANE == 0
			float3 posHitSS = TransformPoint(posHitOS, mat_os2ss);
			float hitDistSS = length(posHitSS.xy - float2(ss_xy));
#else 
			float3 posHitWS = TransformPoint(posHitOS, g_cbPobj.mat_os2ws);
			float hitDistWS = length(posHitWS - pos_ip_ws);
			float hitDistSS = hitDistWS / pixelSpace;
#endif
			if (minDistOnPlane * minDistOnPlane > hitDistSS * hitDistSS) {
				minDistOnPlane = localInside ? -hitDistSS : hitDistSS;
			}
		}

		test_raydir = float4(-updir, ray_tmax);
#ifdef BVH_LEGACY
		intersectBVHandTriangles(test_rayorig, test_raydir, buf_gpuNodes, buf_gpuTriWoops, buf_gpuTriIndices, hitTriIdx, hitDistance, debugbingo, trinormal, false);
#else
		ray.Origin = ray_orig_os;
		ray.Direction = -updir;
		ray.TMin = ray_tmin;
		ray.TMax = ray_tmax;

		hit = TraceRay_Closest(ray);
		hitTriIdx = hit.distance >= FLT_MAX - 1 ? -1 : hit.primitiveID.primitiveIndex;
		hitDistance = hit.distance;
#endif

		if (hitTriIdx >= 0) {
#ifdef BVH_LEGACY
			bool localInside = dot(trinormal, test_raydir.xyz) > 0;
#else
			bool localInside = hit.is_backface;
#endif
#if PICKING == 1 
			if (localInside) checkCountInsideVertical++;
#else
			isInsideOnPlane = localInside && isInsideOnPlane;
			//isInsideOnPlane = localInside || isInsideOnPlane;
#endif

			float3 posHitOS = test_rayorig.xyz + hitDistance * test_raydir.xyz;
#if CURVEDPLANE == 0
			float3 posHitSS = TransformPoint(posHitOS, mat_os2ss);
			float hitDistSS = length(posHitSS.xy - float2(ss_xy));
#else 
			float3 posHitWS = TransformPoint(posHitOS, g_cbPobj.mat_os2ws);
			float hitDistWS = length(posHitWS - pos_ip_ws);
			float hitDistSS = hitDistWS / pixelSpace;
#endif
			if (minDistOnPlane * minDistOnPlane > hitDistSS * hitDistSS) {
				minDistOnPlane = localInside ? -hitDistSS : hitDistSS;
			}
		}
	}
	// safe inside test (right- and left-side)//
	{
#if CURVEDPLANE == 0
		float3 rightPos0 = TransformPoint(float3(ss_xy, 0), g_cbCamState.mat_ss2ws);
		float3 rightPos1 = TransformPoint(float3(ss_xy + float2(1, 0), 0), g_cbCamState.mat_ss2ws);
		float3 rightdir = normalize(TransformVector(rightPos1 - rightPos0, g_cbPobj.mat_ws2os));
#else
		float3 rightdir = normalize(TransformVector(f3VecSampleTangentWS, g_cbPobj.mat_ws2os));
#endif
		float4 test_rayorig = float4(ray_orig_os, ray_tmin);
		float4 test_raydir = float4(rightdir, ray_tmax);
#ifdef BVH_LEGACY
		intersectBVHandTriangles(test_rayorig, test_raydir, buf_gpuNodes, buf_gpuTriWoops, buf_gpuTriIndices, hitTriIdx, hitDistance, debugbingo, trinormal, false);
#else
		RayDesc ray;
		ray.Origin = ray_orig_os;
		ray.Direction = rightdir;
		ray.TMin = ray_tmin;
		ray.TMax = ray_tmax;

		RayHit hit = TraceRay_Closest(ray);
		hitTriIdx = hit.distance >= FLT_MAX - 1 ? -1 : hit.primitiveID.primitiveIndex;
		hitDistance = hit.distance;
#endif
		if (hitTriIdx >= 0) {
#ifdef BVH_LEGACY
			bool localInside = dot(trinormal, test_raydir.xyz) > 0;
#else
			bool localInside = hit.is_backface;
#endif
#if PICKING == 1 
			if (localInside) checkCountInsideHorizon++;
#else
			isInsideOnPlane = localInside && isInsideOnPlane;
#endif

			float3 posHitOS = test_rayorig.xyz + hitDistance * test_raydir.xyz;
#if CURVEDPLANE == 0
			float3 posHitSS = TransformPoint(posHitOS, mat_os2ss);
			float hitDistSS = length(posHitSS.xy - float2(ss_xy));
#else 
			float3 posHitWS = TransformPoint(posHitOS, g_cbPobj.mat_os2ws);
			float hitDistWS = length(posHitWS - pos_ip_ws);
			float hitDistSS = hitDistWS / pixelSpace;
#endif
			if (minDistOnPlane * minDistOnPlane > hitDistSS * hitDistSS) {
				minDistOnPlane = localInside ? -hitDistSS : hitDistSS;
			}

		}

		test_raydir = float4(-rightdir, ray_tmax);
#ifdef BVH_LEGACY
		intersectBVHandTriangles(test_rayorig, test_raydir, buf_gpuNodes, buf_gpuTriWoops, buf_gpuTriIndices, hitTriIdx, hitDistance, debugbingo, trinormal, false);
#else
		ray.Origin = ray_orig_os;
		ray.Direction = -rightdir;
		ray.TMin = ray_tmin;
		ray.TMax = ray_tmax;

		hit = TraceRay_Closest(ray);
		hitTriIdx = hit.distance >= FLT_MAX - 1 ? -1 : hit.primitiveID.primitiveIndex;
		hitDistance = hit.distance;
#endif

		if (hitTriIdx >= 0) {
#ifdef BVH_LEGACY
			bool localInside = dot(trinormal, test_raydir.xyz) > 0;
#else
			bool localInside = hit.is_backface;
#endif
#if PICKING == 1 
			if (localInside) checkCountInsideHorizon++;
#else
			isInsideOnPlane = localInside && isInsideOnPlane;
			//isInsideOnPlane = localInside || isInsideOnPlane;
#endif

			float3 posHitOS = test_rayorig.xyz + hitDistance * test_raydir.xyz;
#if CURVEDPLANE == 0
			float3 posHitSS = TransformPoint(posHitOS, mat_os2ss);
			float hitDistSS = length(posHitSS.xy - float2(ss_xy));
#else 
			float3 posHitWS = TransformPoint(posHitOS, g_cbPobj.mat_os2ws);
			float hitDistWS = length(posHitWS - pos_ip_ws);
			float hitDistSS = hitDistWS / pixelSpace;
#endif
			if (minDistOnPlane * minDistOnPlane > hitDistSS * hitDistSS) {
				minDistOnPlane = localInside ? -hitDistSS : hitDistSS;
			}

		}
	}

#if PICKING == 1 
	if (planeThickness == 0) {
		if (minDistOnPlane * minDistOnPlane < 4.5 * 4.5 || checkCountInsideHorizon == 2 || checkCountInsideVertical == 2)
		{
			uint iii = (checkCountInsideHorizon == 2 ? 10 : 1) + (checkCountInsideVertical == 2 ? 1000 : 100);
			uint fc = 0;
#if DX10_0 == 1
			out_ps.color.r = asfloat(g_cbPobj.pobj_dummy_0);
			out_ps.color.g = 0.f;
			out_ps.color.b = 7.f;
			out_ps.color.a = 77.f;
#else
			//InterlockedAdd(fragment_counter[ss_xy], 1, fc);
			picking_buf[2 * fc + 0] = g_cbPobj.pobj_dummy_0;
			picking_buf[2 * fc + 1] = asuint(0.f);
#endif
		}
		__EXIT;
	}
#else
	if (planeThickness == 0) {
#if DX10_0
#if PICKING != 1 
		out_ps.depthcs = minDistOnPlane;
#endif
#else
		fragment_zdepth[ss_xy] = minDistOnPlane;
#endif
		//__EXIT; // DO NOT finish here (for solid filling)
	}
#endif

	// forward check
	bool hit_on_forward_ray = false;
	float forward_hit_depth = FLT_MAX;
	bool is_front_forward_face = false;
	{
#ifdef BVH_LEGACY
		float4 test_rayorig = float4(ray_orig_os, ray_tmin);
		float4 test_raydir = float4(ray_dir_unit_os, ray_tmax);
		intersectBVHandTriangles(test_rayorig, test_raydir, buf_gpuNodes, buf_gpuTriWoops, buf_gpuTriIndices, hitTriIdx, forward_hit_depth, debugbingo, trinormal, false);
		is_front_forward_face = dot(trinormal, test_raydir.xyz) < 0;
#else
		RayDesc ray;
		ray.Origin = ray_orig_os;
		ray.Direction = ray_dir_unit_os;
		ray.TMin = ray_tmin;
		ray.TMax = ray_tmax;

		RayHit hit = TraceRay_Closest(ray);
		hitTriIdx = hit.distance >= FLT_MAX - 1 ? -1 : hit.primitiveID.primitiveIndex;
		forward_hit_depth = hit.distance;

		is_front_forward_face = !hit.is_backface;
#endif
		hit_on_forward_ray = hitTriIdx >= 0;
	}

	if (!hit_on_forward_ray) {
		//fragment_vis[ss_xy] = float4(1, 0, 0, 1);
		// note ... when ray passes through a triangle edge or vertex, hit may not be detected
		__EXIT;
	}

	if (!isInsideOnPlane && forward_hit_depth > planeThickness_os)
	{
		//fragment_vis[ss_xy] = float4(1, 1, 0, 1);
		__EXIT;
	}

	// backward check
	bool hit_on_backward_ray = false;
	float backward_hit_depth = FLT_MAX;
	float last_layer_depth = -1.f; // 
	bool is_front_backward_face = false;
	{
		float3 ray_backward_origin_os = ray_orig_os + ray_dir_unit_os * planeThickness_os;

#ifdef BVH_LEGACY
		float4 test_rayorig = float4(ray_backward_origin_os, ray_tmin);
		float4 test_raydir = float4(-ray_dir_unit_os, ray_tmax);
		intersectBVHandTriangles(test_rayorig, test_raydir, buf_gpuNodes, buf_gpuTriWoops, buf_gpuTriIndices, hitTriIdx, backward_hit_depth, debugbingo, trinormal, false);
		is_front_backward_face = dot(trinormal, test_raydir.xyz) < 0;
#else
		RayDesc ray;
		ray.Origin = ray_backward_origin_os;
		ray.Direction = -ray_dir_unit_os;
		ray.TMin = ray_tmin;
		ray.TMax = ray_tmax;

		RayHit hit = TraceRay_Closest(ray);
		hitTriIdx = hit.distance >= FLT_MAX - 1 ? -1 : hit.primitiveID.primitiveIndex;
		backward_hit_depth = hit.distance;

		is_front_backward_face = !hit.is_backface;
#endif
		hit_on_backward_ray = hitTriIdx >= 0;
		if (hit_on_backward_ray) last_layer_depth = planeThickness_os - backward_hit_depth;
	}

	float thickness_through_os = 0.f;
	if (planeThickness > 0)
	{
		float hitDistance = FLT_MAX;
		float3 forward_hit_pos_os = ray_orig_os + ray_dir_unit_os * forward_hit_depth;
#ifdef BVH_LEGACY
		float4 test_rayorig = float4(forward_hit_pos_os, ray_tmin);
		float4 test_raydir = float4(ray_dir_unit_os, ray_tmax);
#else
		RayDesc ray;
		ray.Origin = forward_hit_pos_os;
		ray.Direction = ray_dir_unit_os;
		ray.TMin = ray_tmin;
		ray.TMax = ray_tmax;
#endif

#define HITBUFFERSIZE 5
		bool is_backface_prev = !is_front_forward_face;
		uint hitCount = 0; // just for debugging
		//float hitDistsWS[HITBUFFERSIZE];
		if (last_layer_depth > 0)
		{
			if (is_backface_prev)
			{
				//float3 forward_hit_pos_ws = TransformPoint(forward_hit_pos_os, g_cbPobj.mat_os2ws); 
				thickness_through_os = forward_hit_depth;
			}

			float ray_march_dist = forward_hit_depth;

			[allow_uav_condition]
			for (uint i = 0; i < HITBUFFERSIZE; i++)
			{
#ifdef BVH_LEGACY
				intersectBVHandTriangles(test_rayorig, test_raydir, buf_gpuNodes, buf_gpuTriWoops, buf_gpuTriIndices, hitTriIdx, hitDistance, debugbingo, trinormal, false);
				test_rayorig.xyz += ray_dir_unit_os * hitDistance;
#else
				//const float3 mapTex[] = {
				//	float3(0,0,0),
				//	float3(0,0,1),
				//	float3(0,1,1),
				//	float3(0,1,0),
				//	float3(1,1,0),
				//	float3(1,0,0),
				//};
				//const uint mapTexLen = 5;
				//const uint maxHeat = 10;
				//
				//
				//uint hits = TraceRay_DebugBVH(ray);
				//if (hits > 0)
				//{
				//	float l = saturate((float)hits / maxHeat) * mapTexLen;
				//	float3 a = mapTex[floor(l)];
				//	float3 b = mapTex[ceil(l)];
				//	float4 heatmap = float4(lerp(a, b, l - floor(l)), 0.8f);
				//	fragment_vis[ss_xy] = float4(heatmap.xyz, 1);
				//}
				//else if (ray_orig_os.x > 0)
				//{
				//	fragment_vis[ss_xy] = float4(0, 1, 0, 1);
				//}
				////else if (ray_orig_os.y < 0)
				////{
				////	fragment_vis[ss_xy] = float4(0, 0, 1, 1);
				////}
				//else
				//{
				//	fragment_vis[ss_xy] = float4(0, 0, 0, 1);
				//}
				//return;

				RayHit hit = TraceRay_Closest(ray);
				hitTriIdx = hit.distance >= FLT_MAX - 1 ? -1 : hit.primitiveID.primitiveIndex;
				hitDistance = hit.distance;

				ray.Origin += ray.Direction * hit.distance;
				ray.TMin = ray_tmin; // small offset!
				ray.TMax = ray_tmax;

#endif
				if (hitTriIdx < 0)
				{
					break;
				}
				ray_march_dist += hitDistance;

#ifdef BVH_LEGACY
				bool localInside = dot(trinormal, test_raydir.xyz) > 0;
#else
				bool localInside = hit.is_backface;
#endif
				if (localInside)
				{
					if (ray_march_dist < planeThickness_os)
					{
						thickness_through_os += hitDistance;
					}
					else
					{
						thickness_through_os += hitDistance - (ray_march_dist - planeThickness_os);
					}
					hitCount++;
				}
				if (ray_march_dist >= planeThickness_os)
				{
					break;
				}
				is_backface_prev = localInside;
			}
			//fragment_vis[ss_xy] = float4((float3)(hitDistsWS[2] - 30) / 30.f, 1);
			//fragment_vis[ss_xy] = float4((float3)(hitCount) / HITBUFFERSIZE, 1);
			//return;

			//fragment_vis[ss_xy] = float4(thickness_through_os / (planeThickness_os), 0, 0, 1);
			//return;
		}
		else if (!is_front_backward_face)
		{
			thickness_through_os = planeThickness_os;
		}
	}

	//float3 posFirstWS, posLastWS;
	float zdepth0 = -1.f, zdepth1 = -1.f; // WS
	if (isInsideOnPlane) {
		if (planeThickness == 0) {

			if (!hit_on_backward_ray) {
				__EXIT;
			}

			zdepth0 = 0;
			zdepth1 = 0;
			thickness_through_os = 0;
		}
		else { // planeThickness > 0
			//float3 posFirstWS, posLastWS;
			//posFirstWS = pos_ip_ws;
			zdepth0 = 0;

			if (!is_front_forward_face)
			{
				float backward_dist = 0;
#ifdef BVH_LEGACY
				float4 test_rayorig = float4(ray_orig_os, ray_tmin);
				float4 test_raydir = float4(-ray_dir_unit_os, ray_tmax);
				intersectBVHandTriangles(test_rayorig, test_raydir, buf_gpuNodes, buf_gpuTriWoops, buf_gpuTriIndices, hitTriIdx, backward_dist, debugbingo, trinormal, false);
				bool localInside = dot(trinormal, test_raydir.xyz) > 0;
#else
				RayDesc ray;
				ray.Origin = ray_orig_os;
				ray.Direction = -ray_dir_unit_os;
				ray.TMin = ray_tmin;
				ray.TMax = ray_tmax;

				RayHit hit = TraceRay_Closest(ray);
				hitTriIdx = hit.distance >= FLT_MAX - 1 ? -1 : hit.primitiveID.primitiveIndex;
				backward_dist = hit.distance;
				bool localInside = hit.is_backface;
#endif
				if (hitTriIdx >= 0 && localInside)
				{
					float3 vray0_os = backward_dist * ray_dir_unit_os;
					zdepth0 = -length(TransformVector(vray0_os, g_cbPobj.mat_os2ws));
					//fragment_vis[ss_xy] = float4(1, 1, 0, 1);
					//__EXIT;
				}
			}
			//fragment_vis[ss_xy] = float4(1, 1, 0, 1);
			//__EXIT;

			//if (thickness_through_os < 0) {
			//	//fragment_vis[ss_xy] = float4(1, 0, 0, 1);
			//	__EXIT;
			//}

			if (last_layer_depth > 0) {
				float3 vray1_os = last_layer_depth * ray_dir_unit_os;
				zdepth1 = length(TransformVector(vray1_os, g_cbPobj.mat_os2ws));
			}
			else
			{
				zdepth1 = planeThickness;
			}
		}
	}
	else {
		if (planeThickness == 0) {
			__EXIT;
		}
		// outside
		float3 vray0_os = min(forward_hit_depth, planeThickness_os) * ray_dir_unit_os;
		zdepth0 = length(TransformVector(vray0_os, g_cbPobj.mat_os2ws));
		if (zdepth0 > planeThickness) {
			__EXIT;
		}

		if (last_layer_depth > 0) {
			float3 vray1_os = last_layer_depth * ray_dir_unit_os;
			zdepth1 = length(TransformVector(vray1_os, g_cbPobj.mat_os2ws));
		}
		else
		{
			zdepth1 = planeThickness;
		}
	}

	//if (zdepth0 > 0 && thickness_through_os > 0 && hit_on_backward_ray)
	//{
	//	fragment_vis[ss_xy] = float4((float3)saturate(thickness_through_os / (planeThickness_os)), 1);
	//	return;
	//}

#if PICKING == 1 // NO DEFINED DX10_0
	uint fc = 0;
#if DX10_0 == 1
	out_ps.color.r = asfloat(g_cbPobj.pobj_dummy_0);
	out_ps.color.g = zdepth0;
	out_ps.color.b = 7.f;
	out_ps.color.a = 77.f;
	//out_ps.depthcs = zdepth0;
#else
	//InterlockedAdd(fragment_counter[ss_xy], 1, fc);
	picking_buf[2 * fc + 0] = g_cbPobj.pobj_dummy_0;
	picking_buf[2 * fc + 1] = asuint(zdepth0);
#endif
	//float3 posPlane = pos_ip_ws + ray_dir_unit_ws * (planeThickness * 0.5f);// -fThicknessPosition);
	//picking_buf[5 * fc + 2] = asuint(posPlane.x);
	//picking_buf[5 * fc + 3] = asuint(posPlane.y);
	//picking_buf[5 * fc + 4] = asuint(posPlane.z);
	__EXIT;
#endif


#if PICKING != 1 

#if DX10_0 == 1
	float4 v_rgba = float4(g_cbPobj.Kd, g_cbPobj.alpha);
	v_rgba.a = 1;

	if (planeThickness > 0.f) 
	{
		if (out_ps.depthcs > zdepth0) {
			out_ps.color = v_rgba;
			out_ps.depthcs = zdepth0;
		}
	}
	else if (planeThickness == 0.f) 
	{
		v_rgba.a = min(0.3, v_rgba.a);
		if (disableSolidFill)
			v_rgba = float4(0, 0, 0, 0.01);

		v_rgba.rgb *= v_rgba.a;

		out_ps.color = MixOpt(v_rgba, v_rgba.a, out_ps.color, out_ps.color.a);
		out_ps.depthcs = minDistOnPlane;
		//if (g_cbCamState.far_plane > 0) out_ps.depthcs = g_cbCamState.far_plane * 0.5f;
	}

	__EXIT;
#else
	if (planeThickness == 0)
	{
		float4 v_rgba = float4(g_cbPobj.Kd, g_cbPobj.alpha);
		v_rgba.a = 1;
		if (planeThickness == 0.f)
			v_rgba.a = min(0.3, v_rgba.a);

		float zthickness = 0.1f;
		if (disableSolidFill) {
			v_rgba = float4(0, 0, 0, 0.01);
			zthickness = 0.f;
		}

		v_rgba.rgb *= v_rgba.a;

		Fragment frag;
		frag.i_vis = ConvertFloat4ToUInt(v_rgba); // current
		frag.zthick = zthickness;
		frag.z = zdepth0;
		frag.opacity_sum = v_rgba.a;

		Fragment fragMerge = (Fragment)frag;

		uint numFrag = fragment_counter[ss_xy];
		if (numFrag > 0) 
		{
			Fragment fragPrev = (Fragment)0;
			GET_FRAG(fragPrev, addr_base, 0); // previous frag stored in K-buffer

			float4 v_rgbaPrev = ConvertUIntToFloat4(fragPrev.i_vis);
			if (v_rgbaPrev.a > 0.01f)
				v_rgba = MixOpt(v_rgba, v_rgba.a, v_rgbaPrev, fragPrev.opacity_sum);
			//v_rgba = float4(v_rgba.rgb, 1);//
			fragMerge = frag;
			//fragMerge.zthick = 0;
			//v_rgba = float4(1, 0, 0, 1);
			fragMerge.i_vis = ConvertFloat4ToUInt(v_rgba);
			fragMerge.opacity_sum += fragPrev.opacity_sum;
		}

		//bool store_to_kbuf = BitCheck(g_cbCamState.cam_flag, 3) && planeThickness > 0;
		SET_FRAG(addr_base, 0, fragMerge);
		//Fill_kBuffer(ss_xy, 2, v_rgba0, zdepth1, vz_thickness);

		//if (!store_to_kbuf)
		fragment_vis[ss_xy] = v_rgba;

		fragment_counter[ss_xy] = 1;
		//fragment_zdepth[ss_xy] = minDistOnPlane;
	}
	else 
	{
		//float zThickness = last_layer_depth - zdepth0;

		float4 v_rgba = float4(g_cbPobj.Kd, g_cbPobj.alpha);
		if (v_rgba.a < 0.01)
			return;
		// always to k-buf not render-out buffer
		float4 v_rgba0 = v_rgba;// , v_rgba1 = v_rgba;

		// DOJO TO consider...
		// preserve the original alpha (i.e., v_rgba.a) or not..????
		//v_rgba0.a *= min(thickness_through_os / (last_layer_depth - zdepth0) + 0.1f, 1.0f);
		v_rgba0.a *= saturate(thickness_through_os / (planeThickness_os) + 0.1f);
		if (v_rgba0.a < 0.01)
			return;
		//v_rgba0.a *= v_rgba0.a; // heuristic 
		v_rgba0.rgb *= v_rgba0.a;
		//v_rgba0.a = v_rgba.a;

		if (zdepth1 >= planeThickness) // to avoid unexpected frustom culling!
		{
			zdepth1 = planeThickness - 0.001f;
		}

		float vz_thickness = zdepth1 - zdepth0;// GetVZThickness(zdepth0, g_cbPobj.vz_thickness);
		//if (vz_thickness >= planeThickness)
		//{
		//	fragment_vis[ss_xy] = float4(1, 0, 0, 1);
		//	return;
		//}
		//if (zdepth1 >= planeThickness)
		//	fragment_vis[ss_xy] = float4((float3)thickness_through_os / (planeThickness_os), 1);
		//else
		//	fragment_vis[ss_xy] = float4(1, 1, 0, 1);
		//return;
		//k_value
		//if (v_rgba0.a < 1 / 255.f || vz_thickness == 0)
		//	__EXIT;
		//if (v_rgba0.a >= 1.f) v_rgba0.a = 0.999f;
		Fill_kBuffer(ss_xy, 2, v_rgba0, zdepth1, vz_thickness);
		//
		/*
		v_rgba1.a *= min(thickness_through_os / (planeThickness_os) + 0.1f, 1.0f);
		v_rgba1.a *= v_rgba1.a;
		v_rgba1.rgb *= v_rgba1.a;
		//v_rgba1.a = v_rgba.a;
		vz_thickness = GetVZThickness(last_layer_depth, g_cbPobj.vz_thickness);
		
		//k_value
		Fill_kBuffer(ss_xy, 2, v_rgba1, last_layer_depth, vz_thickness);
		/**/
	}

	return;
#endif
#endif
}

//float4 SlicerOutlineTest(const in int2 tex2d_xy, const in float3 edge_color, const in int thick)
//{
//	float2 min_rect = (float2)0;
//	float2 max_rect = float2(g_cbCamState.rt_width - 1, g_cbCamState.rt_height - 1);
//
//	float4 vout = (float4) 0;
//	//float cloestDist = FLT_MAX;
//	int cnt = 0;
//	for (int y = -thick; y <= thick; y++) {
//		for (int x = -thick; x <= thick; x++) {
//			float2 neighbor_pos = tex2d_xy.xy + float2(x, y);
//			float2 v1 = min_rect - neighbor_pos;
//			float2 v2 = max_rect - neighbor_pos;
//			float2 v12 = v1 * v2;
//			if (v12.x >= 0 || v12.y >= 0)
//				continue;
//#if DX10_0 == 1
//			float fv = prev_fragment_zdepth[(int2)neighbor_pos];
//#else
//			float fv = fragment_zdepth[(int2)neighbor_pos];
//#endif
//			if (fv < 1.5f && fv >= 0.f) {
//				//cloestDist = min(cloestDist, length(neighbor_pos - tex2d_xy.xy));
//				cnt++;
//				break;
//			}
//		}
//	}
//
//	if (cnt == 0)
//		return (float4)0;
//	//float w = 2 * thick + 1;
//	//alpha *= alpha;
//
//	//float alpha = min(cnt / 3.f, 1.f);// min((thick + 1 - cloestDist) / thick, 1.0); // min((floaT)count / (w * w / 2.f), 1.f);
//	//vout = float4(edge_color * alpha, alpha);
//
//	vout = float4(edge_color, 1);
//	return vout;
//}

//#define WILDCARD_DEPTH_OUTLINE 777888
///#define OUTSIDE_PLANE 777788
inline bool WildcardPassCheck(uint wildcard_v, float sd)
{
#if DX10_0 == 1
	if (wildcard_v == WILDCARD_DEPTH_OUTLINE || wildcard_v == OUTSIDE_PLANE)
		return false;
#else
	if (wildcard_v == WILDCARD_DEPTH_OUTLINE || asuint(sd) == OUTSIDE_PLANE)
		return false;
#endif
	return true;
}

inline float LineAlpha(float v)
{
	float distAbs = abs(v);
	if (distAbs >= g_cbPobj.pix_thickness)
		return 0;

	return max(min(g_cbPobj.pix_thickness - distAbs, 1.f), 0); // AA option
}

float TestAlpha(float v) {
	uint wildcard_v = asuint(v);
	if (wildcard_v == WILDCARD_DEPTH_OUTLINE || wildcard_v == OUTSIDE_PLANE)
		return 0;

	const float lineThres = g_cbPobj.pix_thickness;
	float distAbs = abs(v);
	if (distAbs >= lineThres)
		return 0;

	return max(min(lineThres - distAbs, 1.f), 0); // AA option
}

float getAlpha(float sd, float sd_neighbor[8], float thickness) {
	// 현재 픽셀이 contour 근처인지 확인
	if (abs(sd) > thickness)
		return 0.0;

	// 부호가 바뀌는 edge 찾기
	int signChanges = 0;
	for (int i = 0; i < 8; i++) {
		if (sign(sd) != sign(sd_neighbor[i])) {
			signChanges++;
		}
	}

	// contour가 지나가는 정도를 계산
	float base = 1.0 - abs(sd) / thickness;
	float edgeFactor = float(signChanges) / 8.0;

	return base * lerp(0.5, 1.0, edgeFactor);
}

float getAlpha2(float sd, float thickness) {
	float smoothWidth = 1.0; // 부드러운 정도를 조절
	return smoothstep(thickness, thickness - smoothWidth, abs(sd));
}

float getAlpha1(float sd, float thickness) {
	//float thickness = 1.0; // 원하는 두께 조절 (1.0이면 기본, 값을 키우면 전환 범위가 넓어짐)
	float aa = fwidth(sd);
	return 1.0 - smoothstep(0.0, thickness * aa, abs(sd));
	
}

float getAlpha3(float sd, float sd_neighbors[8], float thickness) {
	// 좌우, 상하 방향의 중앙 차분으로 gradient 근사
	float dx = 0.5 * (sd_neighbors[0] - sd_neighbors[2]);
	float dy = 0.5 * (sd_neighbors[1] - sd_neighbors[3]);
	float aa = length(float2(dx, dy));  // 변화율 (anti-aliasing 너비)
	float alpha = 1.0 - smoothstep(0.0, aa * thickness, abs(sd));
	return saturate(alpha);
}


#if PICKING != 1
#if DX10_0 == 1
PS_FILL_OUTPUT_NO_DS Outline2D(VS_OUTPUT input) 
#else
[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void Outline2D(uint3 DTid : SV_DispatchThreadID)
#endif
{
#if DX10_0 == 1
	int2 ss_xy = int2(input.f4PosSS.xy);
	PS_FILL_OUTPUT_NO_DS out_ps;
	out_ps.color = prev_fragment_vis[ss_xy];
	out_ps.depthcs = prev_fragment_zdepth[ss_xy];
	float sd = out_ps.depthcs;
#else
	if (DTid.x >= g_cbCamState.rt_width || DTid.y >= g_cbCamState.rt_height)
		return;
	int2 ss_xy = int2(DTid.xy);
	float sd = prev_fragment_zdepth[ss_xy];
#endif
	uint wildcard_v = asuint(sd);
	if (wildcard_v == WILDCARD_DEPTH_OUTLINE || wildcard_v == OUTSIDE_PLANE)
		__EXIT;

	//float sd_neighbors[8] = {
	//	prev_fragment_zdepth[ss_xy + int2(1, 0)],
	//	prev_fragment_zdepth[ss_xy + int2(0, 1)],
	//	prev_fragment_zdepth[ss_xy + int2(-1, 0)],
	//	prev_fragment_zdepth[ss_xy + int2(0, -1)],
	//	prev_fragment_zdepth[ss_xy + int2(-1, -1)],
	//	prev_fragment_zdepth[ss_xy + int2(1, 1)],
	//	prev_fragment_zdepth[ss_xy + int2(1, -1)],
	//	prev_fragment_zdepth[ss_xy + int2(-1, 1)],
	//};


	// half16 사용...
//#define NEW_TEST
#ifdef NEW_TEST
	
	//float a_acc = LineAlpha(sd);
	//uint count = 1;
	//
	[loop]
	for (uint i = 0; i < 8; i++)
	{
		wildcard_v = asuint(sd_neighbors[i]);
		if (wildcard_v == WILDCARD_DEPTH_OUTLINE || wildcard_v == OUTSIDE_PLANE)
			__EXIT;
	}
	//
	//float a = a_acc / (float)count;
	//if (a < 0.5) {
	//	__EXIT;
	//}
	//a = 1;

	//float a_cc = getAlpha1(sd, g_cbPobj.pix_thickness);
	//uint count = 0;
	//[loop]
	//for (uint i = 0; i < 8; i++)
	//{
	//	float a_test = sd_neighbors[i];// 
	//	if (WildcardPassCheck(a_test))
	//	{
	//		a_cc += a_test;
	//		a += getAlpha1(sd_neighbors[i], g_cbPobj.pix_thickness);
	//		count++;
	//	}
	//}
	//a /= count;
	float a = getAlpha(sd, sd_neighbors, g_cbPobj.pix_thickness);
	if (a < 0.1)
	{
		__EXIT;
	}

	if (sd * sd_neighbors[0] < 0 || sd * sd_neighbors[1] < 0 || sd * sd_neighbors[2] < 0 || sd * sd_neighbors[3] < 0)
		a = 1.f;

#else

	float a = TestAlpha(sd);
	if (a <= 0.01) {
		__EXIT;
	}
	
	//float a1 = TestAlpha(sd_neighbors[0]);
	//float a2 = TestAlpha(sd_neighbors[1]);
	//float a3 = TestAlpha(sd_neighbors[2]);
	//float a4 = TestAlpha(sd_neighbors[3]);
	//
	//if (a1 <= a && a2 <= a && a3 <= a && a4 <= a && 1.f > a) {
	//
	//	if (sd * sd_neighbors[0] < 0 || sd * sd_neighbors[1] < 0 || sd * sd_neighbors[2] < 0 || sd * sd_neighbors[3] < 0)
	//		a = 1.f;
	//}
#endif

	float4 outline_color = float4(g_cbPobj.Kd, 1);
	outline_color.a = a;// *a; // heuristic aliasing 
	outline_color.rgb *= outline_color.a;
	//outline_color.rgb *= outline_color.a;
	//outline_color.rgb *= outline_color.a;
	//outline_color.rgb *= outline_color.a; 

	//outline_color = float4(fvcur / 1000, fvcur / 1000, fvcur / 1000, 1);

//	if (outline_color.a == 0)
//		__EXIT;

	//if (fvcur < 10)
	//	outline_color = float4(1, 0, 0, 1);

	bool disableSolidFill = BitCheck(g_cbPobj.pobj_flag, 6);

#if DX10_0 == 1
	//out_ps.ds_z = 0;
	//out_ps.ds_z = input.f4PosSS.z;
	out_ps.color = outline_color;
	out_ps.depthcs = asfloat(WILDCARD_DEPTH_OUTLINE);

	return out_ps;
#else
	fragment_vis[ss_xy] = outline_color;
	//fragment_zdepth[ss_xy] = asfloat(WILDCARD_DEPTH_OUTLINE);

	if (disableSolidFill) {
		//if (outline_color.a < 0)
		//outline_color = float4(1 * outline_color.a, 0, 0, outline_color.a);
		Fill_kBuffer(ss_xy, g_cbCamState.k_value, outline_color, 0.0001, max(g_cbCamState.far_plane, 0.1));
	}

	//if (a > 0.01)
	{
		fragment_counter[ss_xy] = WILDCARD_DEPTH_OUTLINE_DIRTY;
	}
#endif
}

cbuffer cbGlobalParams : register(b8)
{
	HxCB_Undercut g_cbUndercut;
}

Texture2D undercutMap : register(t40);

void ApplyUndercutColor(inout float4 v_rgba, in float3 f3PosWS, in uint2 ss_xy)
{
	const float3 coverDirWS = g_cbUndercut.undercutDir;// float3(g_cbPobj.dash_interval, g_cbPobj.depth_thres, g_cbPobj.pix_thickness);

	int hitTriIdx = -1;
	float hitDistance = 1e20;
	float ray_tmin = 0.01;// .01f;// 0.00001f;
	float ray_tmax = 1e20; // use thickness!!

	// intersect all triangles in the scene stored in BVH
	int debugbingo = 0;

	float3 ray_orig_os = TransformPoint(f3PosWS, g_cbPobj.mat_ws2os);
	float3 ray_dir_os = TransformVector(-coverDirWS, g_cbPobj.mat_ws2os);
	float4 rayorig = float4(ray_orig_os + ray_dir_os * 0.00f, ray_tmin);
	float4 raydir = float4(ray_dir_os, ray_tmax);
	int hitCount = 0;
#ifdef BVH_LEGACY
	float3 trinormal = float3(0, 0, 0);
	intersectBVHandTriangles(rayorig, raydir, buf_gpuNodes, buf_gpuTriWoops, buf_gpuTriIndices, hitTriIdx, hitDistance, debugbingo, trinormal, false);
#else
	RayDesc ray;
	ray.Origin = ray_orig_os;
	ray.Direction = ray_dir_os;
	ray.TMin = ray_tmin;
	ray.TMax = ray_tmax;

	RayHit hit = TraceRay_Closest(ray);
	hitTriIdx = hit.distance >= FLT_MAX - 1 ? -1 : hit.primitiveID.primitiveIndex;
	hitDistance = hit.distance;
#endif

	if (hitTriIdx >= 0) {
		v_rgba.rgb *= ConvertUIntToFloat4(g_cbUndercut.icolor).rgb;
	}
}

void ApplyUndercutColor2(inout float4 v_rgba, in float3 f3PosWS)
{
	float3 posMap = TransformPoint(f3PosWS, g_cbUndercut.mat_ws2lss_udc_map);

	if (posMap.z <= 0) {
		return;
	}

	float3 posLCS = TransformPoint(f3PosWS, g_cbUndercut.mat_ws2lcs_udc_map);
	float mapDepth = abs(-posLCS.z);

	float2 wh = float2(g_cbCamState.rt_width, g_cbCamState.rt_height);
	//float2 uv = posMap.xy / wh;
	float2 uv = float2(posMap.x / wh.x, posMap.y / wh.y);

	//float storedMapDepth = undercutMap.SampleLevel(g_samplerLinear_wrap, uv, 0).r;

	float sd0 = min(undercutMap[int2(posMap.xy)].r, undercutMap[int2(posMap.xy) + int2(1, 0)].r);
	float sd1 = min(undercutMap[int2(posMap.xy) + int2(1, 1)].r, undercutMap[int2(posMap.xy) + int2(0, 1)].r);
	float storedMapDepth = min(sd0, sd1);

	if (mapDepth > storedMapDepth - 0.001) {
		v_rgba.rgb *= ConvertUIntToFloat4(g_cbUndercut.icolor).rgb;
	}
}

/*
void ApplyUndercutColorOld(inout float4 v_rgba, in float3 f3PosWS)
{
	float3 posMap = TransformPoint(f3PosWS, g_cbUndercut.mat_ws2ss_udc_map);

	if (posMap.z <= 0) {
		v_rgba.rgb = float3(0, 0, 1);
		return;
	}

	posMap.z = 0;
	float3 pos_map_ws = TransformPoint(posMap, g_cbUndercut.mat_ss2ws_udc_map);
	float3 vec_pos_map2frag = f3PosWS - pos_map_ws;
	float mapDepth = length(vec_pos_map2frag);

	float2 wh = float2(g_cbCamState.rt_width, g_cbCamState.rt_height);
	//float2 uv = posMap.xy / wh;
	float2 uv = float2(posMap.x / wh.x, posMap.y / wh.y);

	float storedMapDepth = undercutMap.SampleLevel(g_samplerLinear_wrap, uv, 0).r;// +0.01f;

	//v_rgba.rgb = float3(uv, 0);
	if (mapDepth > storedMapDepth) {
		v_rgba.rgb *= ConvertUIntToFloat4(g_cbUndercut.icolor).rgb;
	}
}

/**/
PS_FILL_OUTPUT_NO_DS UndercutShader(__VS_OUT input)
{
	PS_FILL_OUTPUT_NO_DS out_ps;
	//out_ps.ds_z = 1.f; // remove???
	out_ps.color = (float4)0;
	out_ps.depthcs = FLT_MAX;

	float4 v_rgba = (float4)0;
	float z_depth = FLT_MAX;

	BasicShader(input, v_rgba, z_depth);

	if ((g_cbUndercut.icolor >> 24) == 1)
		ApplyUndercutColor(v_rgba, input.f3PosWS, uint2(input.f4PosSS.xy));
	else
		ApplyUndercutColor2(v_rgba, input.f3PosWS);
	//v_rgba = float4(1, 1, 0, 1);

	//out_ps.ds_z = input.f4PosSS.z;
	out_ps.color = v_rgba;
	out_ps.depthcs = z_depth;

	return out_ps;
}

Buffer<uint> sr_offsettable_buf : register(t50);
#define STORE1_RBB(V, ADDR) deep_dynK_buf.Store((ADDR) * 4, V)


[earlydepthstencil]
void OIT_A_BUFFER_FILL_UNDERCUT(__VS_OUT input)
{
	float4 v_rgba = (float4)0;
	float z_depth = FLT_MAX;

	BasicShader(input, v_rgba, z_depth);

	if ((g_cbUndercut.icolor >> 24) == 1)
		ApplyUndercutColor(v_rgba, input.f3PosWS, uint2(input.f4PosSS.xy));
	else
		ApplyUndercutColor2(v_rgba, input.f3PosWS);

	// Atomically allocate space in the deep buffer
	int2 tex2d_xy = int2(input.f4PosSS.xy);
	uint fc = 0;
	InterlockedAdd(fragment_counter[tex2d_xy], 1, fc);

	uint offsettable_idx = tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x;
	uint nDeepBufferPos = 0;
#if DX_11_STYLE == 1
	if (offsettable_idx == 0)
		nDeepBufferPos = fc;
	else
		nDeepBufferPos = sr_offsettable_buf[offsettable_idx - 1] + fc;
#else
	if (offsettable_idx == 0) clip(-1);
	else nDeepBufferPos = sr_offsettable_buf[offsettable_idx] + fc;
#endif

	// Store fragment data into the allocated space
	//deep_ubk_buf[2 * nDeepBufferPos + 0] = ConvertFloat4ToUInt(v_rgba);
	//deep_ubk_buf[2 * nDeepBufferPos + 1] = asuint(z_depth);
	STORE1_RBB(ConvertFloat4ToUInt(v_rgba), 2 * nDeepBufferPos + 0);
	STORE1_RBB(asuint(z_depth), 2 * nDeepBufferPos + 1);
}
#endif