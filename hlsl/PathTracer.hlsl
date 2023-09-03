#if DX10_0 == 1
#define __EXIT return out_ps
#else
#define __EXIT return
#endif

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

// CUDA textures containing scene data
//texture<float4, 1, cudaReadModeElementType> bvhNodesTexture;
//texture<float4, 1, cudaReadModeElementType> triWoopTexture;
//texture<float4, 1, cudaReadModeElementType> triNormalsTexture;
//texture<int, 1, cudaReadModeElementType> triIndicesTexture;
//texture<float4, 1, cudaReadModeElementType> HDRtexture;

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

struct Sphere {

	float rad;				// radius 
	float3 pos, emi, col;	// position, emission, color 
	Refl_t refl;			// reflection type (DIFFuse, SPECular, REFRactive)

	float intersect(const Ray r) { // returns distance, 0 if nohit 

		// ray/sphere intersection
		float3 op = pos - r.orig;
		float t, epsilon = 0.01f;
		float b = dot(op, r.dir);
		float disc = b * b - dot(op, op) + rad * rad; // discriminant of quadratic formula
		if (disc < 0) return 0; else disc = sqrt(disc);
		return (t = b - disc) > epsilon ? t : ((t = b + disc) > epsilon ? t : 0.0f);
	}
};

Sphere spheres[] = {
	// sun
	//{ 10000, { 50.0f, 40.8f, -1060 }, { 0.3, 0.3, 0.3 }, { 0.175f, 0.175f, 0.25f }, DIFF }, // sky   0.003, 0.003, 0.003	
	//{ 4.5, { 0.0f, 12.5, 0 }, { 6, 4, 1 }, { .6f, .6f, 0.6f }, DIFF },  /// lightsource	
	{ 10000.02, { 50.0f, -10001.35, 0 }, { 0.0, 0.0, 0 }, { 0.3f, 0.3f, 0.3f }, DIFF }, // ground  300/-301.0
	//{ 10000, { 50.0f, -10000.1, 0 }, { 0, 0, 0 }, { 0.3f, 0.3f, 0.3f }, DIFF }, // double shell to prevent light leaking
	//{ 110000, { 50.0f, -110048.5, 0 }, { 3.6, 2.0, 0.2 }, { 0.f, 0.f, 0.f }, DIFF },  // horizon brightener

	//{ 0.5, { 30.0f, 180.5, 42 }, { 0, 0, 0 }, { .6f, .6f, 0.6f }, DIFF },  // small sphere 1  
	//{ 0.8, { 2.0f, 0.f, 0 }, { 0.0, 0.0, 0.0 }, { 0.8f, 0.8f, 0.8f }, SPEC },  // small sphere 2
	//{ 0.8, { -3.0f, 0.f, 0 }, { 0.0, 0.0, 0.0 }, { 0.0f, 0.0f, 0.2f }, COAT },  // small sphere 2
	{ 2.5, { -6.0f, 0.5f, 0.0f }, { 0.0, 0.0, 0.0 }, { 0.9f, 0.9f, 0.9f }, SPEC },  // small sphere 2
	//{ 0.6, { -10.0f, -2.f, 1.0f }, { 0.0, 0.0, 0.0 }, { 0.8f, 0.8f, 0.8f }, DIFF },  // small sphere 2
	//{ 0.8, { -1.0f, -0.7f, 4.0f }, { 0.0, 0.0, 0.0 }, { 0.8f, 0.8f, 0.8f }, REFR },  // small sphere 2
	//{ 9.4, { 9.0f, 0.f, -9.0f }, { 0.0, 0.0, 0.0 }, { 0.8f, 0.8f, 0.f }, DIFF },  // small sphere 2
	//{ 22, { 105.0f, 22, 24 }, { 0, 0, 0 }, { 0.9f, 0.9f, 0.9f }, DIFF }, // small sphere 3
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

Buffer<float3> buf_curvePoints : register(t30);
Buffer<float3> buf_curveTangents : register(t31);

Buffer<float4> buf_gpuNodes : register(t0);
Buffer<float4> buf_gpuTriWoops : register(t1);
Buffer<float4> buf_gpuDebugTris : register(t2);
Buffer<int> buf_gpuTriIndices : register(t3);

#define WILDCARD_DEPTH_OUTLINE -2.f
#define INSIDE_DIST_FLAG 10000.f
#define OUTSIDE_PLANE -1.f

#if DX10_0 == 1
#include "CommonShader.hlsl"

// USE PIXEL SHADER //
// USE PS_FILL_OUTPUT //
Texture2D prev_fragment_vis : register(t20);
Texture2D<float> prev_fragment_zdepth : register(t21);

struct VS_OUTPUT
{
	float4 f4PosSS : SV_POSITION;
	float3 f3VecNormalWS : NORMAL;
	float3 f3PosWS : TEXCOORD0;
	float3 f3Custom : TEXCOORD1;
};
struct PS_FILL_OUTPUT
{
	float4 color : SV_TARGET0; // UNORM
	float depthcs : SV_TARGET1;

	//float ds_z : SV_Depth;
};
#else
#include "./kbuf/Sr_Kbuf.hlsl"

#if PATHTR_USE_KBUF == 0
RWTexture2D<uint> fragment_counter : register(u0);
RWByteAddressBuffer deep_k_buf : register(u1);
#endif
RWBuffer<uint> picking_buf : register(u2);
RWTexture2D<unorm float4> fragment_vis : register(u3);
RWTexture2D<float> fragment_zdepth : register(u4);
#endif

// modified intersection routine (uses regular instead of woopified triangles) for debugging purposes

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
	[loop]
	while ((uint)nodeAddr != EntrypointSentinel)
	{
		// Traverse internal nodes until all SIMD lanes have found a leaf.

		bool searchingLeaf = true; // required for warp efficiency
		[loop]
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

							// assume... CW 
							trinormal = cross(float3(v22.x, v22.y, v22.z), float3(v11.x, v11.y, v11.z));  // works
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

#if DX10_0 == 1
PS_FILL_OUTPUT ThickSlicePathTracer(VS_OUTPUT input)
#else
[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void ThickSlicePathTracer(uint3 DTid : SV_DispatchThreadID)
#endif
{
	float4 vis_out = 0;
	float depth_out = 0;

#if DX10_0 == 1
	PS_FILL_OUTPUT out_ps;
	//out_ps.ds_z = 0;
	int2 ss_xy = int2(input.f4PosSS.xy);

	out_ps.color = prev_fragment_vis[ss_xy];
	out_ps.depthcs = prev_fragment_zdepth[ss_xy];
	float fvPrev = prev_fragment_zdepth[ss_xy];
	if (fvPrev == WILDCARD_DEPTH_OUTLINE)
		__EXIT;

	out_ps.depthcs = OUTSIDE_PLANE;
#else
	int2 ss_xy = int2(DTid.xy);

#if PICKING == 1
	int x_pick_ss = g_cbCamState.iSrCamDummy__1 & 0xFFFF;
	int y_pick_ss = g_cbCamState.iSrCamDummy__1 >> 16;
	if (x_pick_ss != ss_xy.x || y_pick_ss != ss_xy.y)
		__EXIT;
#endif

	// do not compute 1st hit surface separately
	if (DTid.x >= g_cbCamState.rt_width || DTid.y >= g_cbCamState.rt_height || g_cbPobj.alpha < 0.001f)
		__EXIT;

	float fvPrev = fragment_zdepth[ss_xy];// asfloat(ConvertFloat4ToUInt(v_rgba));
	if (fvPrev == WILDCARD_DEPTH_OUTLINE)
		__EXIT;
	fragment_zdepth[ss_xy] = OUTSIDE_PLANE;

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
	float3 trinormal = float3(0, 0, 0);
	Refl_t refltype;
	float ray_tmin = 0.00001f;
	float ray_tmax = 1e20; // use thickness!!
	float ray_tmin2 = 0.00001f;
	float ray_tmax2 = 1e20; // use thickness!!

	// intersect all triangles in the scene stored in BVH
	int debugbingo = 0;
	float planeThickness = g_cbCamState.far_plane;// g_cbCurvedSlicer.thicknessPlane;// g_cbCamState.far_plane;

	if (disableSolidFill) {
		// planeThickness > 0
		pos_ip_ws = pos_ip_ws + ray_dir_unit_ws * planeThickness * 0.5f;
		planeThickness = 0;
		//return;
	}

	//pos_ip_ws = float3(0, 0, 0);
	//ray_dir_unit_ws = float3(0, 1, 0);
	float3 ray_orig_os = TransformPoint(pos_ip_ws, g_cbPobj.mat_ws2os);
	float3 r0 = TransformPoint(float3(0, 0, 0), g_cbPobj.mat_ws2os);
	float3 r1 = TransformPoint(ray_dir_unit_ws, g_cbPobj.mat_ws2os);
	//float3 ray_dir_unit_os = normalize(TransformVector(ray_dir_unit_ws, g_cbPobj.mat_ws2os));
	float3 ray_dir_unit_os = normalize(r1 - r0);
	//if (ray_orig_os.z == 0) ray_orig_os.z = 0.00001234f; // trick... for avoiding zero block skipping error
	//if (ray_orig_os.y == 0) ray_orig_os.y = 0.00001234f; // trick... for avoiding zero block skipping error
	//if (ray_orig_os.x == 0) ray_orig_os.x = 0.00001234f; // trick... for avoiding zero block skipping error
	//if (ray_dir_unit_os.z == 0) ray_dir_unit_os.z = 0.00001234f; // trick... for avoiding zero block skipping error
	//if (ray_dir_unit_os.y == 0) ray_dir_unit_os.y = 0.00001234f; // trick... for avoiding zero block skipping error
	//if (ray_dir_unit_os.x == 0) ray_dir_unit_os.x = 0.00001234f; // trick... for avoiding zero block skipping error

	bool isInsideOnPlane = false;
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
	//s	__EXIT;
	float pixelSpace = length(f3PosSampleWS_C_ - f3PosSampleWS_C_nbr) * 1.5; // 1.5 for heuristic correction of curve
	//pixelSpace = length(f3VecSampleUpWS);
#endif
	// safe inside test (up- and down-side)//
	{
#if CURVEDPLANE == 0
		float3 upPos0 = TransformVector(float3(ss_xy, 0), g_cbCamState.mat_ss2ws);
		float3 upPos1 = TransformVector(float3(ss_xy + float2(0, -1), 0), g_cbCamState.mat_ss2ws);
		float3 updir = normalize(TransformVector(upPos1 - upPos0, g_cbPobj.mat_ws2os));
#else
		float3 updir = normalize(f3VecSampleUpWS);// / fPlanePitch; // unit vector
#endif
		float4 test_rayorig = float4(ray_orig_os, ray_tmin);
		float4 test_raydir = float4(updir, ray_tmax);
		intersectBVHandTriangles(test_rayorig, test_raydir, buf_gpuNodes, buf_gpuTriWoops, buf_gpuTriIndices, hitTriIdx, hitDistance, debugbingo, trinormal, false);
		if (hitTriIdx >= 0) {
			isInsideOnPlane = dot(trinormal, test_raydir.xyz) > 0;
			float3 posHitOS = test_rayorig.xyz + hitDistance * test_raydir.xyz;
#if CURVEDPLANE == 0
			float3 posHitSS = TransformPoint(posHitOS, mat_os2ss);
			float hitDistSS = length(posHitSS.xy - float2(ss_xy));
#else 
			float3 posHitWS = TransformPoint(posHitOS, g_cbPobj.mat_os2ws);
			float hitDistWS = length(posHitWS - pos_ip_ws);
			float hitDistSS = hitDistWS / pixelSpace;
#endif
			minDistOnPlane = min(hitDistSS, minDistOnPlane);
		}

		test_raydir = float4(-updir, ray_tmax);
		intersectBVHandTriangles(test_rayorig, test_raydir, buf_gpuNodes, buf_gpuTriWoops, buf_gpuTriIndices, hitTriIdx, hitDistance, debugbingo, trinormal, false);

		if (hitTriIdx >= 0) {
			isInsideOnPlane = dot(trinormal, test_raydir.xyz) > 0 && isInsideOnPlane;

			float3 posHitOS = test_rayorig.xyz + hitDistance * test_raydir.xyz;
#if CURVEDPLANE == 0
			float3 posHitSS = TransformPoint(posHitOS, mat_os2ss);
			float hitDistSS = length(posHitSS.xy - float2(ss_xy));
#else 
			float3 posHitWS = TransformPoint(posHitOS, g_cbPobj.mat_os2ws);
			float hitDistWS = length(posHitWS - pos_ip_ws);
			float hitDistSS = hitDistWS / pixelSpace;
#endif
			minDistOnPlane = min(hitDistSS, minDistOnPlane);
		}
	}
	// safe inside test (right- and left-side)//
	{
#if CURVEDPLANE == 0
		float3 rightPos0 = TransformVector(float3(ss_xy, 0), g_cbCamState.mat_ss2ws);
		float3 rightPos1 = TransformVector(float3(ss_xy + float2(1, 0), 0), g_cbCamState.mat_ss2ws);
		float3 rightdir = normalize(TransformVector(rightPos1 - rightPos0, g_cbPobj.mat_ws2os));
#else
		float3 rightdir = f3VecSampleTangentWS;
#endif
		float4 test_rayorig = float4(ray_orig_os, ray_tmin);
		float4 test_raydir = float4(rightdir, ray_tmax);
		intersectBVHandTriangles(test_rayorig, test_raydir, buf_gpuNodes, buf_gpuTriWoops, buf_gpuTriIndices, hitTriIdx, hitDistance, debugbingo, trinormal, false);
		if (hitTriIdx >= 0) {
			isInsideOnPlane = dot(trinormal, test_raydir.xyz) > 0 && isInsideOnPlane;

			float3 posHitOS = test_rayorig.xyz + hitDistance * test_raydir.xyz;
#if CURVEDPLANE == 0
			float3 posHitSS = TransformPoint(posHitOS, mat_os2ss);
			float hitDistSS = length(posHitSS.xy - float2(ss_xy));
#else 
			float3 posHitWS = TransformPoint(posHitOS, g_cbPobj.mat_os2ws);
			float hitDistWS = length(posHitWS - pos_ip_ws);
			float hitDistSS = hitDistWS / pixelSpace;
#endif
			minDistOnPlane = min(hitDistSS, minDistOnPlane);
		}

		test_raydir = float4(-rightdir, ray_tmax);
		intersectBVHandTriangles(test_rayorig, test_raydir, buf_gpuNodes, buf_gpuTriWoops, buf_gpuTriIndices, hitTriIdx, hitDistance, debugbingo, trinormal, false);

		if (hitTriIdx >= 0) {
			isInsideOnPlane = dot(trinormal, test_raydir.xyz) > 0 && isInsideOnPlane;

			float3 posHitOS = test_rayorig.xyz + hitDistance * test_raydir.xyz;
#if CURVEDPLANE == 0
			float3 posHitSS = TransformPoint(posHitOS, mat_os2ss);
			float hitDistSS = length(posHitSS.xy - float2(ss_xy));
#else 
			float3 posHitWS = TransformPoint(posHitOS, g_cbPobj.mat_os2ws);
			float hitDistWS = length(posHitWS - pos_ip_ws);
			float hitDistSS = hitDistWS / pixelSpace;
#endif
			minDistOnPlane = min(hitDistSS, minDistOnPlane);
		}
	}

	if (planeThickness == 0) {
#if DX10_0
		out_ps.depthcs = minDistOnPlane + INSIDE_DIST_FLAG;
#else
		fragment_zdepth[ss_xy] = minDistOnPlane + INSIDE_DIST_FLAG;
#endif
		//EXIT;
	}

	float4 rayorig = float4(ray_orig_os, ray_tmin);
	float4 raydir = float4(ray_dir_unit_os, ray_tmax);
	int hitCount = 0;
#define HITBUFFERSIZE 5
	float hitDistsWS[HITBUFFERSIZE];
	bool faceDirs[HITBUFFERSIZE];
	[unroll]
	for (int i = 0; i < HITBUFFERSIZE; i++) {
		//DEBUGintersectBVHandTriangles(rayorig, raydir, buf_gpuNodes, buf_gpuDebugTris, buf_gpuTriIndices, hitTriIdx, hitDistance, debugbingo, trinormal, false);
		intersectBVHandTriangles(rayorig, raydir, buf_gpuNodes, buf_gpuTriWoops, buf_gpuTriIndices, hitTriIdx, hitDistance, debugbingo, trinormal, false);
		if (hitTriIdx < 0)
			break;

		//if (hitTriIdx >= 0) 
		{
			hitCount++;

			float3 posHitOS = rayorig.xyz + hitDistance * ray_dir_unit_os;
			float3 posHitWS = TransformPoint(posHitOS, g_cbPobj.mat_os2ws);
			float hitDistWS = length(posHitWS - pos_ip_ws);
			hitDistsWS[i] = hitDistWS;
			faceDirs[i] = dot(trinormal, ray_dir_unit_os) < 0; // 'true' refers to normal towards cam

			rayorig.xyz = posHitOS + 0.0001f * ray_dir_unit_os;
		}
	}
	if (hitCount == 0) {
		//fragment_vis[ss_xy] = float4(1, 0, 0, 1);
		// note ... when ray passes through a triangle edge or vertex, hit may not be detected
		__EXIT;
	}
	//if (hitDistsWS[0] > 0) {
	//	fragment_vis[ss_xy] = float4(1, 0, 0, 1);
	//	//return;
	//}
	//if (hitDistsWS[1] > 0) {
	//	fragment_vis[ss_xy] = float4(1, 1, 0, 1);
	//	//return;
	//}
	//if (hitDistsWS[2] > 0) {
	//	fragment_vis[ss_xy] = float4(0, 1, 0, 1);
	//	//return;
	//}
	//if (hitDistsWS[3] > 0) {
	//	fragment_vis[ss_xy] = float4(0, 0, 1, 1);
	//	//return;
	//}
	//if (hitDistsWS[4] > 0) {
	//	fragment_vis[ss_xy] = float4(1, 0, 1, 1);
	//	//return;
	//}
	//fragment_vis[ss_xy] = float4(1, 1, 1, 1);
	//return;

	//float3 posFirstWS, posLastWS;
	float zdepth0 = -1.f, zdepth1 = -1.f; // WS
	if (isInsideOnPlane) {
		if (planeThickness == 0) {
			float4 rayorig2 = float4(ray_orig_os, ray_tmin2);
			float4 raydir2 = float4(-ray_dir_unit_os, ray_tmax2);
			float3 trinormal2 = float3(0, 0, 0);
			float hitDistance2 = 1e20;
			int hitTriIdx2 = -1;
			intersectBVHandTriangles(rayorig2, raydir2, buf_gpuNodes, buf_gpuTriWoops, buf_gpuTriIndices, hitTriIdx2, hitDistance2, debugbingo, trinormal2, false);
			if (hitTriIdx2 < 0) {
				__EXIT;
			}

			zdepth0 = 0;
			zdepth1 = 0;
		}
		else { // planeThickness > 0
			//float3 posFirstWS, posLastWS;
			//posFirstWS = pos_ip_ws;
			zdepth0 = 0;
			[loop]
			for (i = 0; i < hitCount; i++) {
				if (!faceDirs[i]) {
					//posLastWS = pos_ip_ws + hitDistsWS[i] * ray_dir_unit_ws;
					zdepth1 = hitDistsWS[i];
					break;
				}
			}
			if (zdepth1 < 0) {
				__EXIT;
			}
		}
	}
	else { // outside
		//float3 posFirstWS, posLastWS;
		[loop]
		for (i = 0; i < hitCount; i++) {
			if (faceDirs[i] && zdepth0 < 0) {
				//posFirstWS = pos_ip_ws + hitDistsWS[i] * ray_dir_unit_ws;
				zdepth0 = hitDistsWS[i];
				//break;
			}
			else if (!faceDirs[i]) {
				//posLastWS = pos_ip_ws + hitDistsWS[i] * ray_dir_unit_ws;
				zdepth1 = hitDistsWS[i];
				//break;
			}
		}
		if (zdepth0 < 0 || zdepth1 < 0) {
			__EXIT;
		}

		if (zdepth0 > planeThickness) {
			__EXIT;
		}

		//posFirstWS = pos_ip_ws + zdepth0 * ray_dir_unit_ws;
		//posLastWS = pos_ip_ws + zdepth1 * ray_dir_unit_ws;
		zdepth1 = min(zdepth1, planeThickness);
	}

	//if (zThickness < 0)
	//{
	//	fragment_vis[ss_xy] = float4(1, 1, 1, 1);
	//	return;
	//}

#if PICKING == 1 // NO DEFINED DX10_0
	uint fc = 0;
	InterlockedAdd(fragment_counter[ss_xy], 1, fc);
	picking_buf[2 * fc + 0] = g_cbPobj.pobj_dummy_0;
	picking_buf[2 * fc + 1] = asuint(zdepth0);
	//float3 posPlane = pos_ip_ws + ray_dir_unit_ws * (planeThickness * 0.5f);// -fThicknessPosition);
	//picking_buf[5 * fc + 2] = asuint(posPlane.x);
	//picking_buf[5 * fc + 3] = asuint(posPlane.y);
	//picking_buf[5 * fc + 4] = asuint(posPlane.z);
	return;
#endif

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
			v_rgba = float4(0, 0, 0, 0.001);
		out_ps.color = MixOpt(v_rgba, v_rgba.a, out_ps.color, out_ps.color.a);
		out_ps.depthcs = minDistOnPlane + INSIDE_DIST_FLAG;
		if (g_cbCamState.far_plane > 0) out_ps.depthcs = g_cbCamState.far_plane * 0.5f;
	}

	__EXIT;
#else
	if (planeThickness == 0)
	//if (g_cbCamState.far_plane == 0)
	{
		//return;

		float4 v_rgba = float4(g_cbPobj.Kd, g_cbPobj.alpha);
		v_rgba.a = 1;
		if (planeThickness == 0.f)
			v_rgba.a = min(0.3, v_rgba.a);

		float zthickness = 0.1f;
		if (disableSolidFill) {
			v_rgba = float4(0, 0, 0, 0.01);
			zthickness = 0.f;
		}

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

		//if (!store_to_kbuf)
		fragment_vis[ss_xy] = v_rgba;

		fragment_counter[ss_xy] = 1;
		fragment_zdepth[ss_xy] = minDistOnPlane + INSIDE_DIST_FLAG;
	}
	else 
	{
		float zThickness = zdepth1 - zdepth0;

		float4 v_rgba = float4(g_cbPobj.Kd, g_cbPobj.alpha);
		if (v_rgba.a < 0.01)
			return;
		// always to k-buf not render-out buffer
		float4 v_rgba0 = v_rgba, v_rgba1 = v_rgba;

		// DOJO TO consider...
		// preserve thr original alpha (i.e., v_rgba.a) or not..????
		v_rgba0.a *= min((planeThickness - zdepth0 + zThickness) / planeThickness + 0.1f, 1.0f);
		v_rgba0.a *= v_rgba0.a;
		v_rgba0.rgb *= v_rgba0.a;
		//v_rgba0.a = v_rgba.a;
		float vz_thickness = GetVZThickness(zdepth0, g_cbPobj.vz_thickness);
		Fill_kBuffer(ss_xy, g_cbCamState.k_value, v_rgba0, zdepth0, vz_thickness);

		v_rgba1.a *= min((planeThickness - zdepth1 + zThickness) / planeThickness + 0.1f, 1.0f);
		v_rgba1.a *= v_rgba1.a;
		v_rgba1.rgb *= v_rgba1.a;
		//v_rgba1.a = v_rgba.a;
		vz_thickness = GetVZThickness(zdepth1, g_cbPobj.vz_thickness);
		Fill_kBuffer(ss_xy, g_cbCamState.k_value, v_rgba1, zdepth1, vz_thickness);
	}

	return;
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
//			float fv = prev_fragment_zdepth[(int2)neighbor_pos] - INSIDE_DIST_FLAG;
//#else
//			float fv = fragment_zdepth[(int2)neighbor_pos] - INSIDE_DIST_FLAG;
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

#if DX10_0 == 1
PS_FILL_OUTPUT Outline2D(VS_OUTPUT input)
#else
[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void Outline2D(uint3 DTid : SV_DispatchThreadID)
#endif
{
#if DX10_0 == 1
	int2 ss_xy = int2(input.f4PosSS.xy);
	PS_FILL_OUTPUT out_ps;
	//out_ps.ds_z = 0;
	out_ps.color = prev_fragment_vis[ss_xy];
	out_ps.depthcs = prev_fragment_zdepth[ss_xy];
	float fvcur = out_ps.depthcs;
	
	//if (out_ps.depthcs == WILDCARD_DEPTH_OUTLINE)
	//	return out_ps;
	//out_ps.color = (float4)1;
	//out_ps.depthcs = 1;
	//return out_ps;
#else
	if (DTid.x >= g_cbCamState.rt_width || DTid.y >= g_cbCamState.rt_height)
		return;
	int2 ss_xy = int2(DTid.xy);
	float fvcur = fragment_zdepth[ss_xy];
#endif

	if (fvcur == WILDCARD_DEPTH_OUTLINE || fvcur == OUTSIDE_PLANE)
		__EXIT;
	
	if (fvcur >= INSIDE_DIST_FLAG) 
		fvcur -= INSIDE_DIST_FLAG;

	const float lingThres = 1.3f;
	if (fvcur >= lingThres)
		__EXIT;

	float4 outline_color = float4(g_cbPobj.Kd, 1);
	outline_color.a = max(min((lingThres - fvcur) * 1.f, 1.f), 0); // AA option
	outline_color.rgb *= outline_color.a;

	//outline_color = float4(fvcur / 1000, fvcur / 1000, fvcur / 1000, 1);

//	if (outline_color.a == 0)
//		__EXIT;

	//if (fvcur < 10)
	//	outline_color = float4(1, 0, 0, 1);

	bool disableSolidFill = BitCheck(g_cbPobj.pobj_flag, 6);

#if DX10_0 == 1
	//out_ps.ds_z = input.f4PosSS.z;
	out_ps.color = outline_color;
	out_ps.depthcs = WILDCARD_DEPTH_OUTLINE;

	return out_ps;
#else
	fragment_vis[ss_xy] = outline_color;
	fragment_zdepth[ss_xy] = WILDCARD_DEPTH_OUTLINE;

	// to do 
	if (disableSolidFill)
		Fill_kBuffer(ss_xy, g_cbCamState.k_value, outline_color, 0.01f, g_cbCamState.far_plane);
#endif
}