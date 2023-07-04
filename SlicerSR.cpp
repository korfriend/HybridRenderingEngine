#include "RendererHeader.h"


#define STACK_SIZE  64  // Size of the traversal stack in local memory.
#define M_PI 3.1415926535897932384626422832795028841971f
#define TWO_PI 6.2831853071795864769252867665590057683943f
#define DYNAMIC_FETCH_THRESHOLD 20          // If fewer than this active, fetch new rays
#define samps 1
#define F32_MIN          (1.175494351e-38f)
#define F32_MAX          (3.402823466e+38f)

#define HDRwidth 3200
#define HDRheight 1600
#define HDR
#define EntrypointSentinel 0x76543210
#define MaxBlockHeight 6

enum Refl_t { DIFF, METAL, SPEC, REFR, COAT };  // material types

typedef vmfloat4 Vec4f;
typedef vmfloat3 Vec3f;
typedef vmfloat4 float4;
typedef vmfloat3 float3;
// CUDA textures containing scene data
Vec4f* bvhNodesTexture;
Vec4f* triWoopTexture;
Vec4f* triNormalsTexture;
int* triIndicesTexture;

inline Vec3f absmax3f(const Vec3f& v1, const Vec3f& v2) {
	return Vec3f(v1.x * v1.x > v2.x * v2.x ? v1.x : v2.x, v1.y * v1.y > v2.y * v2.y ? v1.y : v2.y, v1.z * v1.z > v2.z * v2.z ? v1.z : v2.z);
}

struct Ray {
	float3 orig;	// ray origin
	float3 dir;		// ray direction	
	Ray(float3 o_, float3 d_) : orig(o_), dir(d_) {}
};

struct Sphere {

	float rad;				// radius 
	float3 pos, emi, col;	// position, emission, color 
	Refl_t refl;			// reflection type (DIFFuse, SPECular, REFRactive)

	float intersect(const Ray& r) const { // returns distance, 0 if nohit 

		// ray/sphere intersection
		float3 op = pos - r.orig;
		float t, epsilon = 0.01f;
		float b = dot(op, r.dir);
		float disc = b * b - dot(op, op) + rad * rad; // discriminant of quadratic formula
		if (disc < 0) return 0; else disc = sqrtf(disc);
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

#define __int_as_float *(float*)&
#define __float_as_int *(int*)&
int   min_min(int a, int b, int c) { return min(min(a, b), c); }
int   min_max(int a, int b, int c) { return max(min(a, b), c); }
int   max_min(int a, int b, int c) { return min(max(a, b), c); }
int   max_max(int a, int b, int c) { return max(max(a, b), c); }
//float fmin_fmin(float a, float b, float c) { int v = min_min(__float_as_int(a), __float_as_int(b), __float_as_int(c)); return __int_as_float(v); }
//float fmin_fmax(float a, float b, float c) { int v = min_max(__float_as_int(a), __float_as_int(b), __float_as_int(c)); return __int_as_float(v); }
//float fmax_fmin(float a, float b, float c) { int v = max_min(__float_as_int(a), __float_as_int(b), __float_as_int(c)); return __int_as_float(v); }
//float fmax_fmax(float a, float b, float c) { int v = max_max(__float_as_int(a), __float_as_int(b), __float_as_int(c)); return __int_as_float(v); }
float fmin_fmin(float a, float b, float c) { return min(min(a, b), c); }
float fmin_fmax(float a, float b, float c) { return max(min(a, b), c); }
float fmax_fmin(float a, float b, float c) { return min(max(a, b), c); }
float fmax_fmax(float a, float b, float c) { return max(max(a, b), c); }

float spanBeginKepler(float a0, float a1, float b0, float b1, float c0, float c1, float d) { return fmax_fmax(fminf(a0, a1), fminf(b0, b1), fmin_fmax(c0, c1, d)); }
float spanEndKepler(float a0, float a1, float b0, float b1, float c0, float c1, float d) { return fmin_fmin(fmaxf(a0, a1), fmaxf(b0, b1), fmax_fmin(c0, c1, d)); }

//float3 min3f(float3 v0, float3 v1) { return float3(min(v0.x, v1.x), min(v0.y, v1.y), min(v0.z, v1.z)); }
//float3 max3f(float3 v0, float3 v1) { return float3(max(v0.x, v1.x), max(v0.y, v1.y), max(v0.z, v1.z)); }
Vec3f min3f(const Vec3f& v1, const Vec3f& v2) { return Vec3f(v1.x < v2.x ? v1.x : v2.x, v1.y < v2.y ? v1.y : v2.y, v1.z < v2.z ? v1.z : v2.z); }
Vec3f max3f(const Vec3f& v1, const Vec3f& v2) { return Vec3f(v1.x > v2.x ? v1.x : v2.x, v1.y > v2.y ? v1.y : v2.y, v1.z > v2.z ? v1.z : v2.z); }

float mmmax(float3 v) { return fmax_fmax(v.x, v.y, v.z); }
float mmmin(float3 v) { return fmin_fmin(v.x, v.y, v.z); }
// standard ray box intersection routines (for debugging purposes only)
// based on Intersect::RayBox() in original Aila/Laine code
float spanBeginKepler2(float lo_x, float hi_x, float lo_y, float hi_y, float lo_z, float hi_z, float d) {

	Vec3f t0 = Vec3f(lo_x, lo_y, lo_z);
	Vec3f t1 = Vec3f(hi_x, hi_y, hi_z);
	
	Vec3f realmin = min3f(t0, t1);

	float raybox_tmin = mmmax(realmin);// .max(); // maxmin

	//return Vec2f(tmin, tmax);
	return raybox_tmin;
}

float spanEndKepler2(float lo_x, float hi_x, float lo_y, float hi_y, float lo_z, float hi_z, float d) {

	Vec3f t0 = Vec3f(lo_x, lo_y, lo_z);
	Vec3f t1 = Vec3f(hi_x, hi_y, hi_z);

	Vec3f realmax = max3f(t0, t1);

	float raybox_tmax = mmmin(realmax);// .min(); /// minmax

	//return Vec2f(tmin, tmax);
	return raybox_tmax;
}

typedef vmfloat2 float2;

const float __ooeps = exp2f(-80.0f); // Avoid div by zero, returns 1/2^80, an extremely small number
float2 ComputeAaBbHits(const float3& pos_start, const float3& pos_min, const float3& pos_max, const float3& vec_dir)
{
	// intersect ray with a box
	// http://www.siggraph.org/education/materials/HyperGraph/raytrace/rtinter3.htm
	float3 invR;// = float3(1.0f, 1.0f, 1.0f) / vec_dir.x;

	invR.x = 1.0f / (fabsf(vec_dir.x) > __ooeps ? vec_dir.x : copysignf(__ooeps, vec_dir.x)); // inverse ray direction
	invR.y = 1.0f / (fabsf(vec_dir.y) > __ooeps ? vec_dir.y : copysignf(__ooeps, vec_dir.y)); // inverse ray direction
	invR.z = 1.0f / (fabsf(vec_dir.z) > __ooeps ? vec_dir.z : copysignf(__ooeps, vec_dir.z)); // inverse ray direction


	float3 tbot = invR * (pos_min - pos_start);
	float3 ttop = invR * (pos_max - pos_start);

	// re-order intersections to find smallest and largest on each axis
	float3 tmin = min3f(ttop, tbot);
	float3 tmax = max3f(ttop, tbot);

	// find the largest tmin and the smallest tmax
	float largest_tmin = max(max(tmin.x, tmin.y), max(tmin.x, tmin.z));
	float smallest_tmax = min(min(tmax.x, tmax.y), min(tmax.x, tmax.z));

	float tnear = max(largest_tmin, 0.f);
	float tfar = smallest_tmax;
	return float2(tnear, tfar);
}

void swap2(int& a, int& b) { int temp = a; a = b; b = temp; }

// standard ray triangle intersection routines (for debugging purposes only)
// based on Intersect::RayTriangle() in original Aila/Laine code
Vec3f intersectRayTriangle(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2, const Vec4f& rayorig, const Vec4f& raydir) {

	const Vec3f rayorig3f = Vec3f(rayorig.x, rayorig.y, rayorig.z);
	const Vec3f raydir3f = Vec3f(raydir.x, raydir.y, raydir.z);

	const float EPSILON = 0.00001f; // works better
	const Vec3f miss(F32_MAX, F32_MAX, F32_MAX);

	float raytmin = rayorig.w;
	float raytmax = raydir.w;

	Vec3f edge1 = v1 - v0;
	Vec3f edge2 = v2 - v0;

	Vec3f tvec = rayorig3f - v0;
	Vec3f pvec = cross(raydir3f, edge2);
	float det = dot(edge1, pvec);

	float invdet = 1.0f / det;

	float u = dot(tvec, pvec) * invdet;

	Vec3f qvec = cross(tvec, edge1);

	float v = dot(raydir3f, qvec) * invdet;

	if (det > EPSILON)
	{
		//if (u < 0.0f || u > 1.0f) return miss; // 1.0 want = det * 1/det  
		//if (v < 0.0f || (u + v) > 1.0f) return miss;
		// if u and v are within these bounds, continue and go to float t = dot(...	           
	}

	else if (det < -EPSILON)
	{
		//if (u > 0.0f || u < 1.0f) return miss;
		//if (v > 0.0f || (u + v) < 1.0f) return miss;
		// else continue
	}

	else // if det is not larger (more positive) than EPSILON or not smaller (more negative) than -EPSILON, there is a "miss"
		return miss;

	float t = dot(edge2, qvec) * invdet;

	if (t > raytmin && t < raytmax)
		return Vec3f(u, v, t);

	// otherwise (t < raytmin or t > raytmax) miss
	return miss;
}

// modified intersection routine (uses regular instead of woopified triangles) for debugging purposes

void DEBUGintersectBVHandTriangles(const float4 rayorig, const float4 raydir,
	const float4* gpuNodes, const float4* gpuTriWoops, const float4* gpuDebugTris, const int* gpuTriIndices,
	int& hitTriIdx, float& hitdistance, int& debugbingo, Vec3f& trinormal, bool needClosestHit) { // int leafcount, int tricount, 

	int traversalStack[STACK_SIZE];

	float   origx, origy, origz;    // Ray origin.
	float   dirx, diry, dirz;       // Ray direction.
	float   tmin;                   // t-value from which the ray starts. Usually 0.
	float   idirx, idiry, idirz;    // 1 / dir
	float   oodx, oody, oodz;       // orig / dir

	char* stackPtr;
	int		leafAddr;
	int		nodeAddr;
	int     hitIndex;
	float	hitT;
	int threadId1;

	//threadId1 = threadIdx.x + blockDim.x * (threadIdx.y + blockDim.y * (blockIdx.x + gridDim.x * blockIdx.y));

	origx = rayorig.x;
	origy = rayorig.y;
	origz = rayorig.z;
	dirx = raydir.x;
	diry = raydir.y;
	dirz = raydir.z;
	tmin = rayorig.w;

	// ooeps is very small number, used instead of raydir xyz component when that component is near zero
	float ooeps = exp2f(-80.0f); // Avoid div by zero, returns 1/2^80, an extremely small number
	//float idirx__ = 1.0f / (fabsf(raydir.x) > ooeps ? raydir.x : copysignf(ooeps, raydir.x)); // inverse ray direction
	//float idiry__ = 1.0f / (fabsf(raydir.y) > ooeps ? raydir.y : copysignf(ooeps, raydir.y)); // inverse ray direction
	//float idirz__ = 1.0f / (fabsf(raydir.z) > ooeps ? raydir.z : copysignf(ooeps, raydir.z)); // inverse ray direction
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
	stackPtr = (char*)&traversalStack[0]; // point stackPtr to bottom of traversal stack = EntryPointSentinel
	leafAddr = 0;   // No postponed leaf.
	nodeAddr = 0;   // Start from the root.
	hitIndex = -1;  // No triangle intersected so far.
	hitT = raydir.w;

	while (nodeAddr != EntrypointSentinel) // EntrypointSentinel = 0x76543210 
	{
		// Traverse internal nodes until all SIMD lanes have found a leaf.

		bool searchingLeaf = true; // flag required to increase efficiency of threads in warp
		while (nodeAddr >= 0 && nodeAddr != EntrypointSentinel)
		{
			float4* ptr = (float4*)((char*)gpuNodes + nodeAddr);
			float4 __n0xy = ptr[0]; // childnode 0, xy-bounds (c0.lo.x, c0.hi.x, c0.lo.y, c0.hi.y)		
			float4 __n1xy = ptr[1]; // childnode 1. xy-bounds (c1.lo.x, c1.hi.x, c1.lo.y, c1.hi.y)		
			float4 __nz = ptr[2]; // childnodes 0 and 1, z-bounds(c0.lo.z, c0.hi.z, c1.lo.z, c1.hi.z)	

			int nodeIdx = nodeAddr / (int)16;
			float4 n0xy = gpuNodes[nodeIdx];// ptr[0]; // childnode 0, xy-bounds (c0.lo.x, c0.hi.x, c0.lo.y, c0.hi.y)		
			float4 n1xy = gpuNodes[nodeIdx + 1];//ptr[1]; // childnode 1. xy-bounds (c1.lo.x, c1.hi.x, c1.lo.y, c1.hi.y)		
			float4 nz = gpuNodes[nodeIdx + 2];//ptr[2]; // childnodes 0 and 1, z-bounds(c0.lo.z, c0.hi.z, c1.lo.z, c1.hi.z)		

			float3 __aabb0_min = float3(n0xy.x, n0xy.z, nz.x);
			float3 __aabb0_max = float3(n0xy.y, n0xy.w, nz.y);
			float3 __aabb1_min = float3(n1xy.x, n0xy.z, nz.z);
			float3 __aabb1_max = float3(n1xy.y, n0xy.w, nz.w);

			float3 __pos_org = float3(rayorig.x, rayorig.y, rayorig.z);
			float3 __ray_dir = float3(raydir.x, raydir.y, raydir.z);
			float2 tt0 = ComputeAaBbHits(__pos_org, __aabb0_min, __aabb0_max, __ray_dir);
			float2 tt1 = ComputeAaBbHits(__pos_org, __aabb1_min, __aabb1_max, __ray_dir);

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
				nodeAddr = *(int*)stackPtr; // fetch next node by popping stack
				stackPtr -= 4; // popping decrements stack by 4 bytes (because stackPtr is a pointer to char) 
			}

			// Otherwise => fetch child pointers.

			else  // one or both children intersected
			{
				//vmint2 cnodes = *(vmint2*)&ptr[3];

				float4 nodef4 = gpuNodes[nodeIdx + 3];
				vmint2 cnodes = vmint2(*(int*)&(nodef4.x), *(int*)&(nodef4.y));// = *(int2*) & ptr[3];


				// set nodeAddr equal to intersected childnode (first childnode when both children are intersected)
				nodeAddr = (traverseChild0) ? cnodes.x : cnodes.y;

				// Both children were intersected => push the farther one on the stack.

				if (traverseChild0 && traverseChild1) // store closest child in nodeAddr, swap if necessary
				{
					if (c1min < c0min)
						swap2(nodeAddr, cnodes.y);
					stackPtr += 4;  // pushing increments stack by 4 bytes (stackPtr is a pointer to char)
					*(int*)stackPtr = cnodes.y; // push furthest node on the stack
				}
			}

			// First leaf => postpone and continue traversal.
			// leafnodes have a negative index to distinguish them from inner nodes
			// if nodeAddr less than 0 -> nodeAddr is a leaf
			if (nodeAddr < 0 && leafAddr >= 0)  // if leafAddr >= 0 -> no leaf found yet (first leaf)
			{
				searchingLeaf = false; // required for warp efficiency
				leafAddr = nodeAddr;

				nodeAddr = *(int*)stackPtr;  // pops next node from stack
				stackPtr -= 4;  // decrement by 4 bytes (stackPtr is a pointer to char)
			}

			// All SIMD lanes have found a leaf => process them.
			// NOTE: inline PTX implementation of "if(!__any(leafAddr >= 0)) break;".
			// tried everything with CUDA 4.2 but always got several redundant instructions.

			// if (!searchingLeaf){ break;  }  

			// if (!__any(searchingLeaf)) break; // "__any" keyword: if none of the threads is searching a leaf, in other words
			// if all threads in the warp found a leafnode, then break from while loop and go to triangle intersection

			// if(!__any(leafAddr >= 0))   /// als leafAddr in PTX code >= 0, dan is het geen echt leafNode   
			//    break;

			unsigned int mask; // mask replaces searchingLeaf in PTX code
			bool p = leafAddr >= 0;
			if (!p) {
				break;
			}
			//asm("{\n"
			//	"   .reg .pred p;               \n"
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

		while (leafAddr < 0)  // if leafAddr is negative, it points to an actual leafnode (when positive or 0 it's an innernode
		{
			// leafAddr is stored as negative number, see cidx[i] = ~triWoopData.getSize(); in CudaBVH.cpp

			for (int triAddr = ~leafAddr;; triAddr += 3)
			{    // no defined upper limit for loop, continues until leaf terminator code 0x80000000 is encountered

				// Read first 16 bytes of the triangle.
				// fetch first triangle vertex
				float4 v0f = gpuDebugTris[triAddr + 0];

				// End marker 0x80000000 (= negative zero) => all triangles in leaf processed. --> terminate 				
				if (__float_as_int(v0f.x) == 0x80000000) break;

				float4 v1f = gpuDebugTris[triAddr + 1];
				float4 v2f = gpuDebugTris[triAddr + 2];

				const Vec3f v0 = Vec3f(v0f.x, v0f.y, v0f.z);
				const Vec3f v1 = Vec3f(v1f.x, v1f.y, v1f.z);
				const Vec3f v2 = Vec3f(v2f.x, v2f.y, v2f.z);

				// convert float4 to Vec4f

				Vec4f rayorigvec4f = Vec4f(rayorig.x, rayorig.y, rayorig.z, rayorig.w);
				Vec4f raydirvec4f = Vec4f(raydir.x, raydir.y, raydir.z, raydir.w);

				Vec3f bary = intersectRayTriangle(v0, v1, v2, rayorigvec4f, raydirvec4f);

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
				nodeAddr = *(int*)stackPtr;  // pop stack
				stackPtr -= 4;               // decrement with 4 bytes to get the next int (stackPtr is char*)
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
}


void intersectBVHandTriangles(const float4 rayorig, const float4 raydir,
	const float4* gpuNodes, const float4* gpuTriWoops, const float4* gpuDebugTris, const int* gpuTriIndices,
	int& hitTriIdx, float& hitdistance, int& debugbingo, Vec3f& trinormal, int leafcount, int tricount, bool anyHit)
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

	char* stackPtr;               // Current position in traversal stack.
	int     leafAddr;               // If negative, then first postponed leaf, non-negative if no leaf (innernode).
	int     nodeAddr;
	int     hitIndex;               // Triangle index of the closest intersection, -1 if none.
	float   hitT;                   // t-value of the closest intersection.
	// Kepler kernel only
	//int     leafAddr2;              // Second postponed leaf, non-negative if none.  
	//int     nodeAddr = EntrypointSentinel; // Non-negative: current internal node, negative: second postponed leaf.

	int threadId1; // ipv rayidx

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
		float ooeps = exp2f(-80.0f); // Avoid div by zero, returns 1/2^80, an extremely small number
		idirx = 1.0f / (fabsf(raydir.x) > ooeps ? raydir.x : copysignf(ooeps, raydir.x)); // inverse ray direction
		idiry = 1.0f / (fabsf(raydir.y) > ooeps ? raydir.y : copysignf(ooeps, raydir.y)); // inverse ray direction
		idirz = 1.0f / (fabsf(raydir.z) > ooeps ? raydir.z : copysignf(ooeps, raydir.z)); // inverse ray direction
		oodx = origx * idirx;  // ray origin / ray direction
		oody = origy * idiry;  // ray origin / ray direction
		oodz = origz * idirz;  // ray origin / ray direction

		// Setup traversal + initialisation

		traversalStack[0] = EntrypointSentinel; // Bottom-most entry. 0x76543210 (1985229328 in decimal)
		stackPtr = (char*)&traversalStack[0]; // point stackPtr to bottom of traversal stack = EntryPointSentinel
		leafAddr = 0;   // No postponed leaf.
		nodeAddr = 0;   // Start from the root.
		hitIndex = -1;  // No triangle intersected so far.
		hitT = raydir.w; // tmax  
	}

	// Traversal loop.

	while (nodeAddr != EntrypointSentinel)
	{
		// Traverse internal nodes until all SIMD lanes have found a leaf.

		bool searchingLeaf = true; // required for warp efficiency
		while (nodeAddr >= 0 && nodeAddr != EntrypointSentinel)
		{
			// Fetch AABBs of the two child nodes.

			// nodeAddr is an offset in number of bytes (char) in gpuNodes array

			float4* ptr = (float4*)((char*)gpuNodes + nodeAddr);
			float4 n0xy = ptr[0]; // childnode 0, xy-bounds (c0.lo.x, c0.hi.x, c0.lo.y, c0.hi.y)		
			float4 n1xy = ptr[1]; // childnode 1, xy-bounds (c1.lo.x, c1.hi.x, c1.lo.y, c1.hi.y)		
			float4 nz = ptr[2]; // childnode 0 and 1, z-bounds (c0.lo.z, c0.hi.z, c1.lo.z, c1.hi.z)		
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
			bool traverseChild0 = (c0min <= c0max); // && (c0min >= tmin) && (c0min <= ray_tmax);
			bool traverseChild1 = (c1min <= c1max); // && (c1min >= tmin) && (c1min <= ray_tmax);

			// Neither child was intersected => pop stack.

			if (!traverseChild0 && !traverseChild1)
			{
				nodeAddr = *(int*)stackPtr; // fetch next node by popping the stack 
				stackPtr -= 4; // popping decrements stackPtr by 4 bytes (because stackPtr is a pointer to char)   
			}

			// Otherwise, one or both children intersected => fetch child pointers.

			else
			{
				vmint2 cnodes = *(vmint2*)&ptr[3];
				// set nodeAddr equal to intersected childnode index (or first childnode when both children are intersected)
				nodeAddr = (traverseChild0) ? cnodes.x : cnodes.y;

				// Both children were intersected => push the farther one on the stack.

				if (traverseChild0 && traverseChild1) // store closest child in nodeAddr, swap if necessary
				{
					if (c1min < c0min)
						swap2(nodeAddr, cnodes.y);
					stackPtr += 4;  // pushing increments stack by 4 bytes (stackPtr is a pointer to char)
					*(int*)stackPtr = cnodes.y; // push furthest node on the stack
				}
			}

			// First leaf => postpone and continue traversal.
			// leafnodes have a negative index to distinguish them from inner nodes
			// if nodeAddr less than 0 -> nodeAddr is a leaf
			if (nodeAddr < 0 && leafAddr >= 0)
			{
				searchingLeaf = false; // required for warp efficiency
				leafAddr = nodeAddr;
				nodeAddr = *(int*)stackPtr;  // pops next node from stack
				stackPtr -= 4;  // decrements stackptr by 4 bytes (because stackPtr is a pointer to char)
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

			unsigned int mask; // replaces searchingLeaf
			bool p = leafAddr >= 0;
			if (!p) {
				break;
			}
			//asm("{\n"
			//	"   .reg .pred p;               \n"
			//	"setp.ge.s32        p, %1, 0;   \n"
			//	"vote.ballot.b32    %0,p;       \n"
			//	"}"
			//	: "=r"(mask)
			//	: "r"(leafAddr));
			//
			//if (!mask)
			//	break;
		}


		///////////////////////////////////////////
		/// TRIANGLE INTERSECTION
		//////////////////////////////////////

		// Process postponed leaf nodes.

		while (leafAddr < 0)  /// if leafAddr is negative, it points to an actual leafnode (when positive or 0 it's an innernode)
		{
			// Intersect the ray against each triangle using Sven Woop's algorithm.
			// Woop ray triangle intersection: Woop triangles are unit triangles. Each ray
			// must be transformed to "unit triangle space", before testing for intersection

			for (int triAddr = ~leafAddr;; triAddr += 3)  // triAddr is index in triWoop array (and bitwise complement of leafAddr)
			{ // no defined upper limit for loop, continues until leaf terminator code 0x80000000 is encountered

				// Read first 16 bytes of the triangle.
				// fetch first precomputed triangle edge
				float4 v00 = triWoopTexture[triAddr];// tex1Dfetch(triWoopTexture, triAddr);

				// End marker 0x80000000 (negative zero) => all triangles in leaf processed --> terminate
				if (__float_as_int(v00.x) == 0x80000000)
					break;

				// Compute and check intersection t-value (hit distance along ray).
				float Oz = v00.w - origx * v00.x - origy * v00.y - origz * v00.z;   // Origin z
				float invDz = 1.0f / (dirx * v00.x + diry * v00.y + dirz * v00.z);  // inverse Direction z
				float t = Oz * invDz;

				if (t > tmin && t < hitT)
				{
					// Compute and check barycentric u.

					// fetch second precomputed triangle edge
					float4 v11 = triWoopTexture[triAddr + 1];// tex1Dfetch(triWoopTexture, triAddr + 1);
					float Ox = v11.w + origx * v11.x + origy * v11.y + origz * v11.z;  // Origin.x
					float Dx = dirx * v11.x + diry * v11.y + dirz * v11.z;  // Direction.x
					float u = Ox + t * Dx; /// parametric equation of a ray (intersection point)

					if (u >= 0.0f && u <= 1.0f)
					{
						// Compute and check barycentric v.

						// fetch third precomputed triangle edge
						float4 v22 = triWoopTexture[triAddr + 2];// tex1Dfetch(triWoopTexture, triAddr + 2);
						float Oy = v22.w + origx * v22.x + origy * v22.y + origz * v22.z;
						float Dy = dirx * v22.x + diry * v22.y + dirz * v22.z;
						float v = Oy + t * Dy;

						if (v >= 0.0f && u + v <= 1.0f)
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

							//trinormal = cross(Vec3f(v22.x, v22.y, v22.z), Vec3f(v11.x, v11.y, v11.z));  // works
							trinormal = cross(Vec3f(v11.x, v11.y, v11.z), Vec3f(v22.x, v22.y, v22.z));
						}
					}
				}
			} // end triangle intersection

			// Another leaf was postponed => process it as well.

			leafAddr = nodeAddr;
			if (nodeAddr < 0)    // nodeAddr is an actual leaf when < 0
			{
				nodeAddr = *(int*)stackPtr;  // pop stack
				stackPtr -= 4;               // decrement with 4 bytes to get the next int (stackPtr is char*)
			}
		} // end leaf/triangle intersection loop
	} // end traversal loop (AABB and triangle intersection)

	// Remap intersected triangle index, and store the result.

	if (hitIndex != -1) {
		hitIndex = triIndicesTexture[hitIndex];// tex1Dfetch(triIndicesTexture, hitIndex);
		// remapping tri indices delayed until this point for performance reasons
		// (slow texture memory lookup in de triIndicesTexture) because multiple triangles per node can potentially be hit
	}

	hitTriIdx = hitIndex;
	hitdistance = hitT;
}





bool RenderSrSlicer(VmFnContainer* _fncontainer,
	VmGpuManager* gpu_manager,
	grd_helper::GpuDX11CommonParameters* dx11CommonParams,
	LocalProgress* progress,
	double* run_time_ptr)
{
	// https://atyuwen.github.io/posts/antialiased-line/


	using namespace std::chrono;
	//((std::mutex*)HDx11GetMutexGpuCriticalPath())->lock();

#pragma region // Parameter Setting //
	VmIObject* iobj = _fncontainer->fnParams.GetParam("_VmIObject*_RenderOut", (VmIObject*)NULL);

	int k_value_old = iobj->GetObjParam("_int_NumK", (int)8);
	int k_value = _fncontainer->fnParams.GetParam("_int_NumK", k_value_old);
	iobj->SetObjParam("_int_NumK", k_value);

	int num_moments_old = iobj->GetObjParam("_int_NumQueueLayers", (int)8);
	int num_moments = _fncontainer->fnParams.GetParam("_int_NumQueueLayers", num_moments_old);
	int num_safe_loopexit = _fncontainer->fnParams.GetParam("_int_SpinLockSafeLoops", (int)10000000);
	bool is_final_renderer = _fncontainer->fnParams.GetParam("_bool_IsFinalRenderer", true);
	//double v_discont_depth = _fncontainer->fnParams.GetParam("_float_DiscontDepth", -1.0);
	float merging_beta = _fncontainer->fnParams.GetParam("_float_MergingBeta", 0.5f);
	bool blur_SSAO = _fncontainer->fnParams.GetParam("_bool_BlurSSAO", true);

	bool is_picking_routine = _fncontainer->fnParams.GetParam("_bool_IsPickingRoutine", false);
	vmint2 picking_pos_ss = _fncontainer->fnParams.GetParam("_int2_PickingPosSS", vmint2(-1, -1));

	int buf_ex_scale = _fncontainer->fnParams.GetParam("_int_BufExScale", (int)8); // scaling the capacity of the k-buffer for _bool_PixelTransmittance
	bool use_blending_option_MomentOIT = _fncontainer->fnParams.GetParam("_bool_UseBlendingOptionMomentOIT", false);
	bool check_pixel_transmittance = _fncontainer->fnParams.GetParam("_bool_PixelTransmittance", false);
	//vr_level = 2;
	vmfloat4 default_phong_lighting_coeff = vmfloat4(0.2, 1.0, 0.5, 5); // Emission, Diffusion, Specular, Specular Power

	float default_point_thickness = _fncontainer->fnParams.GetParam("_float_PointThickness", 3.0f);
	float default_surfel_size = _fncontainer->fnParams.GetParam("_float_SurfelSize", 0.0f);
	float default_line_thickness = _fncontainer->fnParams.GetParam("_float_LineThickness", 2.0f);
	vmfloat3 default_color_cmmobj = _fncontainer->fnParams.GetParam("_float3_CmmGlobalColor", vmfloat3(-1, -1, -1));
	bool use_spinlock_pixsynch = _fncontainer->fnParams.GetParam("_bool_UseSpinLock", false);
	bool is_ghost_mode = _fncontainer->fnParams.GetParam("_bool_GhostEffect", false);
	bool is_rgba = _fncontainer->fnParams.GetParam("_bool_IsRGBA", false); // false means bgra

	float planeThickness = _fncontainer->fnParams.GetParam("_float_PlaneThickness", 0.f);

	bool is_system_out = false;
	// note : planeThickness == 0 calls CPU renderer which uses system-out buffer
	if (is_final_renderer || planeThickness <= 0.f) is_system_out = true;
	//is_system_out = true;

	bool only_surface_test = _fncontainer->fnParams.GetParam("_bool_OnlySurfaceTest", false);
	bool test_consoleout = _fncontainer->fnParams.GetParam("_bool_TestConsoleOut", false);
	bool test_fps_profiling = _fncontainer->fnParams.GetParam("_bool_TestFpsProfile", false);
	auto test_out = [&test_consoleout](const string& _message)
	{
		if (test_consoleout)
			cout << _message << endl;
	};

	float sample_rate = _fncontainer->fnParams.GetParam("_float_UserSampleRate", 0.0f);
	if (sample_rate <= 0) sample_rate = 1.0f;
	bool apply_samplerate2gradient = _fncontainer->fnParams.GetParam("_bool_ApplySampleRateToGradient", false);
	bool reload_hlsl_objs = _fncontainer->fnParams.GetParam("_bool_ReloadHLSLObjFiles", false);

	int __BLOCKSIZE = _fncontainer->fnParams.GetParam("_int_GpuThreadBlockSize", (int)4);
	float v_thickness = _fncontainer->fnParams.GetParam("_float_VZThickness", 0.0f);
	float gi_v_thickness = _fncontainer->fnParams.GetParam("_float_GIVZThickness", v_thickness);
	float scale_z_res = _fncontainer->fnParams.GetParam("_float_zResScale", 1.0f);

	int i_test_shader = (int)_fncontainer->fnParams.GetParam("_int_ShaderTest", (int)0);

	VmLight* light = _fncontainer->fnParams.GetParam("_VmLight*_LightSource", (VmLight*)NULL);
	VmLens* lens = _fncontainer->fnParams.GetParam("_VmLens*_CamLens", (VmLens*)NULL);
	LightSource light_src;
	GlobalLighting global_lighting;
	LensEffect lens_effect;
	if (light) {
		light_src.is_on_camera = light->is_on_camera;
		light_src.is_pointlight = light->is_pointlight;
		light_src.light_pos = light->pos;
		light_src.light_dir = light->dir;
		light_src.light_ambient_color = vmfloat3(1.f);
		light_src.light_diffuse_color = vmfloat3(1.f);
		light_src.light_specular_color = vmfloat3(1.f);

		global_lighting.apply_ssao = light->effect_ssao.is_on_ssao;
		global_lighting.ssao_r_kernel = light->effect_ssao.kernel_r;
		global_lighting.ssao_num_steps = light->effect_ssao.num_steps;
		global_lighting.ssao_num_dirs = light->effect_ssao.num_dirs;
		global_lighting.ssao_tangent_bias = light->effect_ssao.tangent_bias;
		global_lighting.ssao_blur = light->effect_ssao.smooth_filter;
		global_lighting.ssao_intensity = light->effect_ssao.ao_power;
		global_lighting.ssao_debug = _fncontainer->fnParams.GetParam("_int_SSAOOutput", (int)0);
	}
	if (lens) {
		lens_effect.apply_ssdof = lens->apply_ssdof;
		lens_effect.dof_focus_z = lens->dof_focus_z;
		lens_effect.dof_lens_F = lens->dof_lens_F;
		lens_effect.dof_lens_r = lens->dof_lens_r;
		lens_effect.dof_ray_num_samples = lens->dof_ray_num_samples;
	}
#pragma endregion

#pragma region // Shader Setting
	// Shader Re-Compile Setting //
	if (reload_hlsl_objs)
	{
		char ownPth[2048];
		GetModuleFileNameA(NULL, ownPth, (sizeof(ownPth)));
		string exe_path = ownPth;
		size_t pos = 0;
		std::string token;
		string delimiter = "\\";
		string hlslobj_path = "";
		while ((pos = exe_path.find(delimiter)) != std::string::npos) {
			token = exe_path.substr(0, pos);
			if (token.find(".exe") != std::string::npos) break;
			hlslobj_path += token + "\\";
			exe_path.erase(0, pos + delimiter.length());
		}
		//hlslobj_path += "..\\..\\VmModuleProjects\\plugin_gpudx11_renderer\\shader_compiled_objs\\";
		hlslobj_path += "..\\..\\VmProjects\\hybrid_rendering_engine\\shader_compiled_objs\\";
		//cout << hlslobj_path << endl;

		string prefix_path = hlslobj_path;
		cout << "RECOMPILE HLSL _ OIT renderer!!" << endl;

		dx11CommonParams->dx11DeviceImmContext->VSSetShader(NULL, NULL, 0);
		dx11CommonParams->dx11DeviceImmContext->GSSetShader(NULL, NULL, 0);
		dx11CommonParams->dx11DeviceImmContext->PSSetShader(NULL, NULL, 0);
		dx11CommonParams->dx11DeviceImmContext->CSSetShader(NULL, NULL, 0);

#define VS_NUM 5
#define GS_NUM 1
#define CS_NUM 5
#define SET_VS(NAME, __S) dx11CommonParams->safe_set_res(grd_helper::COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, NAME), __S, true)
#define SET_CS(NAME, __S) dx11CommonParams->safe_set_res(grd_helper::COMRES_INDICATOR(GpuhelperResType::COMPUTE_SHADER, NAME), __S, true)
#define SET_GS(NAME, __S) dx11CommonParams->safe_set_res(grd_helper::COMRES_INDICATOR(GpuhelperResType::GEOMETRY_SHADER, NAME), __S, true)

		string strNames_VS[VS_NUM] = {
			   "SR_OIT_P_vs_5_0"
			  ,"SR_OIT_PN_vs_5_0"
			  ,"SR_OIT_PT_vs_5_0"
			  ,"SR_OIT_PNT_vs_5_0"
			  ,"SR_OIT_PTTT_vs_5_0"
		};

		for (int i = 0; i < VS_NUM; i++)
		{
			string strName = strNames_VS[i];

			FILE* pFile;
			if (fopen_s(&pFile, (prefix_path + strName).c_str(), "rb") == 0)
			{
				fseek(pFile, 0, SEEK_END);
				ullong ullFileSize = ftell(pFile);
				fseek(pFile, 0, SEEK_SET);
				byte* pyRead = new byte[ullFileSize];
				fread(pyRead, sizeof(byte), ullFileSize, pFile);
				fclose(pFile);

				ID3D11VertexShader* dx11VShader = NULL;
				if (dx11CommonParams->dx11Device->CreateVertexShader(pyRead, ullFileSize, NULL, &dx11VShader) != S_OK)
				{
					VMERRORMESSAGE("SHADER COMPILE FAILURE!");
				}
				else
				{
					SET_VS(strName, dx11VShader);
				}
				VMSAFE_DELETEARRAY(pyRead);
			}
		}
		/**/

		string strNames_GS[GS_NUM] = {
			   "GS_CutLines_gs_5_0"
		};

		for (int i = 0; i < GS_NUM; i++)
		{
			string strName = strNames_GS[i];

			FILE* pFile;
			if (fopen_s(&pFile, (prefix_path + strName).c_str(), "rb") == 0)
			{
				fseek(pFile, 0, SEEK_END);
				ullong ullFileSize = ftell(pFile);
				fseek(pFile, 0, SEEK_SET);
				byte* pyRead = new byte[ullFileSize];
				fread(pyRead, sizeof(byte), ullFileSize, pFile);
				fclose(pFile);

				ID3D11GeometryShader* dx11GShader = NULL;
				if (dx11CommonParams->dx11Device->CreateGeometryShader(pyRead, ullFileSize, NULL, &dx11GShader) != S_OK)
				{
					VMERRORMESSAGE("SHADER COMPILE FAILURE!");
				}
				else
				{
					SET_GS(strName, dx11GShader);
				}
				VMSAFE_DELETEARRAY(pyRead);
			}
		}

		string strNames_CS[CS_NUM] = {
			   "ThickSlicePathTracer_cs_5_0"
			  ,"CurvedThickSlicePathTracer_cs_5_0"
			  ,"SliceOutline_cs_5_0"
			  ,"PickingThickSlice_cs_5_0"
			  ,"PickingCurvedThickSlice_cs_5_0"
		};

		for (int i = 0; i < CS_NUM; i++)
		{
			string strName = strNames_CS[i];

			FILE* pFile;
			if (fopen_s(&pFile, (prefix_path + strName).c_str(), "rb") == 0)
			{
				fseek(pFile, 0, SEEK_END);
				ullong ullFileSize = ftell(pFile);
				fseek(pFile, 0, SEEK_SET);
				byte* pyRead = new byte[ullFileSize];
				fread(pyRead, sizeof(byte), ullFileSize, pFile);
				fclose(pFile);

				ID3D11ComputeShader* dx11CShader = NULL;
				if (dx11CommonParams->dx11Device->CreateComputeShader(pyRead, ullFileSize, NULL, &dx11CShader) != S_OK)
				{
					VMERRORMESSAGE("SHADER COMPILE FAILURE!");
				}
				else
				{
					SET_CS(strName, dx11CShader);
				}
				VMSAFE_DELETEARRAY(pyRead);
			}
		}
		/**/
		dx11CommonParams->dx11DeviceImmContext->Flush();
	}

	ID3D11InputLayout* dx11LI_P = (ID3D11InputLayout*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "P"));
	ID3D11InputLayout* dx11LI_PN = (ID3D11InputLayout*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "PN"));
	ID3D11InputLayout* dx11LI_PT = (ID3D11InputLayout*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "PT"));
	ID3D11InputLayout* dx11LI_PNT = (ID3D11InputLayout*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "PNT"));
	ID3D11InputLayout* dx11LI_PTTT = (ID3D11InputLayout*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "PTTT"));

	ID3D11VertexShader* dx11VShader_P = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_P_vs_5_0"));
	ID3D11VertexShader* dx11VShader_PN = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PN_vs_5_0"));
	ID3D11VertexShader* dx11VShader_PT = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PT_vs_5_0"));
	ID3D11VertexShader* dx11VShader_PNT = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PNT_vs_5_0"));
	ID3D11VertexShader* dx11VShader_PTTT = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PTTT_vs_5_0"));

	ID3D11Buffer* cbuf_cam_state = dx11CommonParams->get_cbuf("CB_CameraState");
	ID3D11Buffer* cbuf_env_state = dx11CommonParams->get_cbuf("CB_EnvState");
	ID3D11Buffer* cbuf_clip = dx11CommonParams->get_cbuf("CB_ClipInfo");
	ID3D11Buffer* cbuf_pobj = dx11CommonParams->get_cbuf("CB_PolygonObject");
	ID3D11Buffer* cbuf_vobj = dx11CommonParams->get_cbuf("CB_VolumeObject");
	ID3D11Buffer* cbuf_reffect = dx11CommonParams->get_cbuf("CB_Material");
	ID3D11Buffer* cbuf_tmap = dx11CommonParams->get_cbuf("CB_TMAP");
	ID3D11Buffer* cbuf_hsmask = dx11CommonParams->get_cbuf("CB_HotspotMask");
	ID3D11Buffer* cbuf_curvedslicer = dx11CommonParams->get_cbuf("CB_CurvedSlicer");
#pragma endregion 

#pragma region // IOBJECT CPU
	//while (iobj->GetFrameBuffer(FrameBufferUsageRENDEROUT, 2) != NULL)
	//	iobj->DeleteFrameBuffer(FrameBufferUsageRENDEROUT, 2);
	if (!iobj->ReplaceFrameBuffer(FrameBufferUsageRENDEROUT, 0, data_type::dtype<vmbyte4>(), ("common render out frame buffer : defined in vismtv_inbuilt_renderergpudx module")))
		iobj->InsertFrameBuffer(data_type::dtype<vmbyte4>(), FrameBufferUsageRENDEROUT, ("common render out frame buffer : defined in vismtv_inbuilt_renderergpudx module"));
	if (iobj->GetFrameBuffer(FrameBufferUsageRENDEROUT, 1) == NULL)
		iobj->InsertFrameBuffer(data_type::dtype<vmbyte4>(), FrameBufferUsageRENDEROUT, ("temp render out frame buffer Backup : defined in vismtv_inbuilt_renderergpudx module"));


	//while (iobj->GetFrameBuffer(FrameBufferUsageDEPTH, 1) != NULL)
	//	iobj->DeleteFrameBuffer(FrameBufferUsageDEPTH, 1);
	if (!iobj->ReplaceFrameBuffer(FrameBufferUsageDEPTH, 0, data_type::dtype<float>(), ("1st hit screen depth frame buffer : defined in vismtv_inbuilt_renderergpudx module")))
		iobj->InsertFrameBuffer(data_type::dtype<float>(), FrameBufferUsageDEPTH, ("1st hit screen depth frame buffer : defined in vismtv_inbuilt_renderergpudx module"));
#pragma endregion 

	__ID3D11Device* dx11Device = dx11CommonParams->dx11Device;
	__ID3D11DeviceContext* dx11DeviceImmContext = dx11CommonParams->dx11DeviceImmContext;

#pragma region // IOBJECT GPU
	vmint2 fb_size_cur;
	iobj->GetFrameBufferInfo(&fb_size_cur);
	vmint2 fb_size_old = iobj->GetObjParam("_int2_PreviousScreenSize", vmint2(0, 0));
	if (fb_size_cur.x != fb_size_old.x || fb_size_cur.y != fb_size_old.y
		|| k_value != k_value_old)
	{
		gpu_manager->ReleaseGpuResourcesBySrcID(iobj->GetObjectID());	// System Out 포함 //
		iobj->SetObjParam("_int2_PreviousScreenSize", fb_size_cur);
		iobj->SetObjParam("_int_PreviousBufferEx", (int)1);
	}
	ullong lastest_render_time = iobj->GetObjParam("_ullong_LatestSrTime", (ullong)0);

	GpuRes gres_fb_rgba, gres_fb_depthcs, gres_fb_depthstencil, gres_fb_counter;
	GpuRes gres_fb_k_buffer;
	GpuRes gres_fb_sys_rgba, gres_fb_sys_depthcs;

	// Ghost effect mode
	//GpuRes gres_fb_mask_hotspot;
	grd_helper::UpdateFrameBuffer(gres_fb_rgba, iobj, "RENDER_OUT_RGBA_0", RTYPE_TEXTURE2D,
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
	grd_helper::UpdateFrameBuffer(gres_fb_depthcs, iobj, "RENDER_OUT_DEPTH_0", RTYPE_TEXTURE2D,
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_FLOAT, 0);
	grd_helper::UpdateFrameBuffer(gres_fb_depthstencil, iobj, "DEPTH_STENCIL", RTYPE_TEXTURE2D,
		D3D11_BIND_DEPTH_STENCIL, DXGI_FORMAT_D32_FLOAT, false);
	grd_helper::UpdateFrameBuffer(gres_fb_counter, iobj, "RW_COUNTER", RTYPE_TEXTURE2D,
		D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_UINT, 0);

	const int num_frags_perpixel = k_value * 4;
	grd_helper::UpdateFrameBuffer(gres_fb_k_buffer, iobj, "BUFFER_RW_K_BUF", RTYPE_BUFFER,
		D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_TYPELESS, UPFB_RAWBYTE, num_frags_perpixel);

	const int max_picking_layers = 2048;
	GpuRes gres_picking_buffer, gres_picking_system_buffer;
	if (is_picking_routine) {
		// those picking layers contain depth (4bytes) and id (4bytes) information
		grd_helper::UpdateFrameBuffer(gres_picking_buffer, iobj, "BUFFER_RW_PICKING_BUF", RTYPE_BUFFER,
			D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_UINT, UPFB_NFPP_BUFFERSIZE, max_picking_layers * 2);
		grd_helper::UpdateFrameBuffer(gres_picking_system_buffer, iobj, "SYSTEM_OUT_RW_PICKING_BUF", RTYPE_BUFFER,
			NULL, DXGI_FORMAT_R32_UINT, UPFB_SYSOUT | UPFB_NFPP_BUFFERSIZE, max_picking_layers * 2);

		uint clr_unit4[4] = { 0, 0, 0, 0 };
		dx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gres_picking_buffer.alloc_res_ptrs[DTYPE_UAV], clr_unit4);
	}

	grd_helper::UpdateFrameBuffer(gres_fb_sys_rgba, iobj, "SYSTEM_OUT_RGBA", RTYPE_TEXTURE2D, NULL, DXGI_FORMAT_R8G8B8A8_UNORM, UPFB_SYSOUT);
	grd_helper::UpdateFrameBuffer(gres_fb_sys_depthcs, iobj, "SYSTEM_OUT_DEPTH", RTYPE_TEXTURE2D, NULL, DXGI_FORMAT_R32_FLOAT, UPFB_SYSOUT);

#pragma endregion 

	uint num_grid_x = __BLOCKSIZE == 1 ? fb_size_cur.x : (uint)ceil(fb_size_cur.x / (float)__BLOCKSIZE);
	uint num_grid_y = __BLOCKSIZE == 1 ? fb_size_cur.y : (uint)ceil(fb_size_cur.y / (float)__BLOCKSIZE);

	bool curved_slicer = _fncontainer->fnParams.GetParam("_bool_IsNonlinear", false);
	if (curved_slicer) {
		vector<vmfloat3>& vtrCurveInterpolations = *_fncontainer->fnParams.GetParamPtr<vector<vmfloat3>>("_vlist_FLOAT3_CurveInterpolations");
		vector<vmfloat3>& vtrCurveUpVectors = *_fncontainer->fnParams.GetParamPtr<vector<vmfloat3>>("_vlist_FLOAT3_CurveUpVectors");
		vector<vmfloat3>& vtrCurveTangentVectors = *_fncontainer->fnParams.GetParamPtr<vector<vmfloat3>>("_vlist_FLOAT3_CurveTangentVectors");
		GpuRes gres_cpPoints, gres_cpTangents;
		int num_curvedPoints = (int)vtrCurveInterpolations.size();
		if (num_curvedPoints < 1)
			return false;
		grd_helper::UpdateCustomBuffer(gres_cpPoints, iobj, "CurvedPlanePoints", &vtrCurveInterpolations[0], num_curvedPoints, DXGI_FORMAT_R32G32B32_FLOAT, 12);
		grd_helper::UpdateCustomBuffer(gres_cpTangents, iobj, "CurvedPlaneTangents", &vtrCurveTangentVectors[0], num_curvedPoints, DXGI_FORMAT_R32G32B32_FLOAT, 12);
		dx11DeviceImmContext->CSSetShaderResources(30, 1, (ID3D11ShaderResourceView**)&gres_cpPoints.alloc_res_ptrs[DTYPE_SRV]);
		dx11DeviceImmContext->CSSetShaderResources(31, 1, (ID3D11ShaderResourceView**)&gres_cpTangents.alloc_res_ptrs[DTYPE_SRV]);
	
		float sample_dist = 1.f;
		CB_CurvedSlicer cbCurvedSlicer;
		grd_helper::SetCb_CurvedSlicer(cbCurvedSlicer, _fncontainer, iobj, sample_dist);
		D3D11_MAPPED_SUBRESOURCE mappedResCurvedSlicerState;
		dx11DeviceImmContext->Map(cbuf_curvedslicer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResCurvedSlicerState);
		CB_CurvedSlicer* cbCurvedSlicerData = (CB_CurvedSlicer*)mappedResCurvedSlicerState.pData;
		memcpy(cbCurvedSlicerData, &cbCurvedSlicer, sizeof(CB_CurvedSlicer));
		dx11DeviceImmContext->Unmap(cbuf_curvedslicer, 0);
		dx11DeviceImmContext->CSSetConstantBuffers(10, 1, &cbuf_curvedslicer);
	}

#pragma region // Camera & Light Setting
	VmCObject* cam_obj = iobj->GetCameraObject();

	vmmat44 dmatWS2CS, dmatCS2PS, dmatPS2SS;
	vmmat44 dmatSS2PS, dmatPS2CS, dmatCS2WS;
	cam_obj->GetMatrixWStoSS(&dmatWS2CS, &dmatCS2PS, &dmatPS2SS);
	cam_obj->GetMatrixSStoWS(&dmatSS2PS, &dmatPS2CS, &dmatCS2WS);
	vmmat44 dmatWS2PS = dmatWS2CS * dmatCS2PS;
	vmmat44f matWS2PS = dmatWS2PS;
	vmmat44f matWS2SS = dmatWS2PS * dmatPS2SS;
	vmmat44f matSS2WS = (dmatSS2PS * dmatPS2CS) * dmatCS2WS;

	vmfloat3 picking_ray_origin, picking_ray_dir;
	if (is_picking_routine) {
		if (curved_slicer) {
			float curved_plane_w, curved_plane_h;
			int num_curve_width_pts;
			
			vector<vmfloat3>& curve_pos_pts = *_fncontainer->fnParams.GetParamPtr<vector<vmfloat3>>("_vlist_FLOAT3_CurveInterpolations");
			vector<vmfloat3>& curve_up_pts = *_fncontainer->fnParams.GetParamPtr<vector<vmfloat3>>("_vlist_FLOAT3_CurveUpVectors");
			vector<vmfloat3>& curve_tan_pts = *_fncontainer->fnParams.GetParamPtr<vector<vmfloat3>>("_vlist_FLOAT3_CurveTangentVectors");

			curved_plane_w = _fncontainer->fnParams.GetParam("_float_ExPlaneWidth", 1.f);
			curved_plane_h = _fncontainer->fnParams.GetParam("_float_ExPlaneHeight", 1.f);
			num_curve_width_pts = (int)curve_pos_pts.size();

			vmfloat3 cam_pos_cws, cam_dir_cws, cam_up_cws;
			cam_obj->GetCameraExtStatef(&cam_pos_cws, &cam_dir_cws, &cam_up_cws);
			vmdouble2 ipSize;
			cam_obj->GetCameraIntState(&ipSize, NULL, NULL, NULL);
			vmfloat2 fIpSize = ipSize;

			vmfloat3 cam_right_cws = normalize(cross(cam_dir_cws, cam_up_cws));
			// if view is +z and up is +y, then x dir is left, which means that curve index increases along right to left
			vmfloat3 pos_tl_cws = cam_pos_cws - cam_right_cws * fIpSize.x * 0.5f + cam_up_cws * fIpSize.y * 0.5f;
			vmfloat3 pos_tr_cws = cam_pos_cws + cam_right_cws * fIpSize.x * 0.5f + cam_up_cws * fIpSize.y * 0.5f;
			vmfloat3 pos_bl_cws = cam_pos_cws - cam_right_cws * fIpSize.x * 0.5f - cam_up_cws * fIpSize.y * 0.5f;
			vmfloat3 pos_br_cws = cam_pos_cws + cam_right_cws * fIpSize.x * 0.5f - cam_up_cws * fIpSize.y * 0.5f;
			float curve_plane_pitch = curved_plane_w / (float)num_curve_width_pts;
			float num_curve_height_pts = curved_plane_h / curve_plane_pitch;
			float num_curve_midheight_pts = num_curve_height_pts * 0.5f;
			
			vmmat44f mat_scale;// = scale(vmfloat3(curve_plane_pitch));
			fMatrixScaling(&mat_scale, &vmfloat3(curve_plane_pitch, curve_plane_pitch, curve_plane_pitch));
			vmmat44f mat_translate;// = translate(vmfloat3(-curved_plane_w * 0.5f, -curved_plane_h * 0.5f, -curve_plane_pitch * 0.5f));
			fMatrixTranslation(&mat_translate, &vmfloat3(-curved_plane_w * 0.5f, -curved_plane_h * 0.5f, -curve_plane_pitch * 0.5f));
			vmmat44f mat_cos2cws = mat_scale * mat_translate; //mat_translate * mat_scale;
			vmmat44f mat_cws2cos = inverse(mat_cos2cws);

			vmfloat3 pos_tl_cos;//transformPos(pos_tl_cws, mat_cws2cos);
			fTransformPoint(&pos_tl_cos, &pos_tl_cws, &mat_cws2cos);
			vmfloat3 pos_tr_cos;//transformPos(pos_tr_cws, mat_cws2cos);
			fTransformPoint(&pos_tr_cos, &pos_tr_cws, &mat_cws2cos);
			vmfloat3 pos_bl_cos;//transformPos(pos_bl_cws, mat_cws2cos);
			fTransformPoint(&pos_bl_cos, &pos_bl_cws, &mat_cws2cos);
			vmfloat3 pos_br_cos;//transformPos(pos_br_cws, mat_cws2cos);
			fTransformPoint(&pos_br_cos, &pos_br_cws, &mat_cws2cos);

			vmfloat2 pos_inter_top_cos, pos_inter_bottom_cos, pos_sample_cos;
			float fRatio0 = (float)(((fb_size_cur.x - 1) - picking_pos_ss.x) / (float)(fb_size_cur.x - 1));
			float fRatio1 = (float)(picking_pos_ss.x) / (float)(fb_size_cur.x - 1);
			pos_inter_top_cos.x = fRatio0 * pos_tl_cos.x + fRatio1 * pos_tr_cos.x;
			pos_inter_top_cos.y = fRatio0 * pos_tl_cos.y + fRatio1 * pos_tr_cos.y;
			if (pos_inter_top_cos.x >= 0 && pos_inter_top_cos.x < (float)(num_curve_width_pts - 1)) {
				int x_sample_cos = (int)floor(pos_inter_top_cos.x);
				float x_ratio = pos_inter_top_cos.x - (float)x_sample_cos;
				int addr_minmix_x = min(max(x_sample_cos, 0), num_curve_width_pts - 1);
				int addr_minmax_nextx = min(max(x_sample_cos + 1, 0), num_curve_width_pts - 1);
				vmfloat3 pos_sample_ws_c0 = curve_pos_pts[addr_minmix_x];
				vmfloat3 pos_sample_ws_c1 = curve_pos_pts[addr_minmax_nextx];
				vmfloat3 pos_sample_ws_c = pos_sample_ws_c0 * (1.f - x_ratio) + pos_sample_ws_c1 * x_ratio;

				vmfloat3 up_sample_ws_c0 = curve_up_pts[addr_minmix_x];
				vmfloat3 up_sample_ws_c1 = curve_up_pts[addr_minmax_nextx];
				vmfloat3 up_sample_ws_c = up_sample_ws_c0 * (1.f - x_ratio) + up_sample_ws_c1 * x_ratio;

				vmfloat3 tan_sample_ws_c0 = curve_tan_pts[addr_minmix_x];
				vmfloat3 tan_sample_ws_c1 = curve_tan_pts[addr_minmax_nextx];
				vmfloat3 tan_sample_ws_c = tan_sample_ws_c0 * (1.f - x_ratio) + tan_sample_ws_c1 * x_ratio;

				//picking_ray_dir;// = normalize(cross(up_sample_ws_c, tan_sample_ws_c));
				fCrossDotVector(&picking_ray_dir, &up_sample_ws_c, &tan_sample_ws_c);
				fNormalizeVector(&picking_ray_dir, &picking_ray_dir);

				up_sample_ws_c = normalize(up_sample_ws_c);
				up_sample_ws_c *= curve_plane_pitch;
				pos_inter_bottom_cos.x = fRatio0 * pos_bl_cos.x + fRatio1 * pos_br_cos.x;
				pos_inter_bottom_cos.y = fRatio0 * pos_bl_cos.y + fRatio1 * pos_br_cos.y;

				//================== y
				float y_ratio0 = (float)((fb_size_cur.y - 1) - picking_pos_ss.y) / (float)(fb_size_cur.y - 1);
				float y_ratio1 = (float)(picking_pos_ss.y) / (float)(fb_size_cur.y - 1);
				pos_sample_cos.x = y_ratio0 * pos_inter_top_cos.x + y_ratio1 * pos_inter_bottom_cos.x;
				pos_sample_cos.y = y_ratio0 * pos_inter_top_cos.y + y_ratio1 * pos_inter_bottom_cos.y;
				if (pos_sample_cos.y < 0 || pos_sample_cos.y > num_curve_height_pts)
					return false;
				picking_ray_origin = pos_sample_ws_c + up_sample_ws_c * (pos_sample_cos.y - num_curve_midheight_pts);
				bool bIsRightSide = _fncontainer->fnParams.GetParam("_bool_IsRightSide", false);
				if (bIsRightSide)
					picking_ray_dir *= -1.f;
				float fPlaneThickness = _fncontainer->fnParams.GetParam("_float_PlaneThickness", 0.f);
				picking_ray_origin -= picking_ray_dir * fPlaneThickness * 0.5f;
			}
			else 
				return true;

			//float fExCurveThicknessPositionRange = _fncontainer->fnParams.GetParam("_float_CurveThicknessPositionRange", 1.f);
			//float fThicknessRatio = _fncontainer->fnParams.GetParam("_float_ThicknessRatio", 0.f);
			//float fThicknessPosition = fThicknessRatio * fExCurveThicknessPositionRange * 0.5;
			//float fPlaneThickness = _fncontainer->fnParams.GetParam("_float_PlaneThickness", 0.f);
		}
		else {
			cam_obj->GetCameraExtStatef(&picking_ray_origin, &picking_ray_dir, NULL);
			vmfloat3 pos_picking_ws, pos_picking_ss(picking_pos_ss.x, picking_pos_ss.y, 0);
			vmmath::fTransformPoint(&pos_picking_ws, &pos_picking_ss, &matSS2WS);

			if (cam_obj->IsPerspective()) {
				picking_ray_dir = pos_picking_ws - picking_ray_origin;
				vmmath::fNormalizeVector(&picking_ray_dir, &picking_ray_dir);
			}
			else {
				picking_ray_origin = pos_picking_ws;
			}
		}
	}

	CB_EnvState cbEnvState;
	grd_helper::SetCb_Env(cbEnvState, cam_obj, light_src, global_lighting, lens_effect);
	cbEnvState.num_safe_loopexit = num_safe_loopexit;
	cbEnvState.env_dummy_2 = i_test_shader;
	D3D11_MAPPED_SUBRESOURCE mappedResEnvState;
	dx11DeviceImmContext->Map(cbuf_env_state, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResEnvState);
	CB_EnvState* cbEnvStateData = (CB_EnvState*)mappedResEnvState.pData;
	memcpy(cbEnvStateData, &cbEnvState, sizeof(CB_EnvState));
	dx11DeviceImmContext->Unmap(cbuf_env_state, 0);
	dx11DeviceImmContext->PSSetConstantBuffers(7, 1, &cbuf_env_state);
	dx11DeviceImmContext->CSSetConstantBuffers(7, 1, &cbuf_env_state);

	if (is_ghost_mode)
	{
		// do 'dynamic'
		D3D11_MAPPED_SUBRESOURCE mappedResHSMask;
		dx11DeviceImmContext->Map(cbuf_hsmask, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResHSMask);
		CB_HotspotMask* cbHSMaskData = (CB_HotspotMask*)mappedResHSMask.pData;
		grd_helper::SetCb_HotspotMask(*cbHSMaskData, _fncontainer, matWS2SS);
		dx11DeviceImmContext->Unmap(cbuf_hsmask, 0);
		dx11DeviceImmContext->PSSetConstantBuffers(9, 1, &cbuf_hsmask);
		dx11DeviceImmContext->CSSetConstantBuffers(9, 1, &cbuf_hsmask);
	}
	
	//if (is_ghost_mode)
	//{
	//	dx11DeviceImmContext->PSSetShaderResources(50, 1, (ID3D11ShaderResourceView**)&gres_fb_mask_hotspot.alloc_res_ptrs[DTYPE_SRV]);
	//	dx11DeviceImmContext->CSSetShaderResources(50, 1, (ID3D11ShaderResourceView**)&gres_fb_mask_hotspot.alloc_res_ptrs[DTYPE_SRV]);
	//}

	D3D11_RECT rects[1];
	rects[0].left = 0;
	rects[0].right = fb_size_cur.x;
	rects[0].top = 0;
	rects[0].bottom = fb_size_cur.y;
	dx11DeviceImmContext->RSSetScissorRects(1, rects);

	// View Port Setting //
	D3D11_VIEWPORT dx11ViewPort;
	dx11ViewPort.Width = (float)fb_size_cur.x;
	dx11ViewPort.Height = (float)fb_size_cur.y;
	dx11ViewPort.MinDepth = 0;
	dx11ViewPort.MaxDepth = 1.0f;
	dx11ViewPort.TopLeftX = 0;
	dx11ViewPort.TopLeftY = 0;
	dx11DeviceImmContext->RSSetViewports(1, &dx11ViewPort);
#pragma endregion 

#pragma region // Presetting of VmObject
	auto __compute_computespace_screen = [](int& w, int& h, const vmmat44f& matOS2SS, const AaBbMinMax& stAaBbOS)
	{
		vmint2 aaBbMinSS(INT_MAX, INT_MAX), aaBbMaxSS(0, 0);
		vmfloat3 f3PosOrthoBoxesOS[8];
		f3PosOrthoBoxesOS[0] = vmfloat3(stAaBbOS.pos_min.x, stAaBbOS.pos_min.y, stAaBbOS.pos_max.z);
		f3PosOrthoBoxesOS[1] = vmfloat3(stAaBbOS.pos_max.x, stAaBbOS.pos_min.y, stAaBbOS.pos_max.z);
		f3PosOrthoBoxesOS[2] = vmfloat3(stAaBbOS.pos_min.x, stAaBbOS.pos_min.y, stAaBbOS.pos_min.z);
		f3PosOrthoBoxesOS[3] = vmfloat3(stAaBbOS.pos_max.x, stAaBbOS.pos_min.y, stAaBbOS.pos_min.z);
		f3PosOrthoBoxesOS[4] = vmfloat3(stAaBbOS.pos_min.x, stAaBbOS.pos_max.y, stAaBbOS.pos_max.z);
		f3PosOrthoBoxesOS[5] = vmfloat3(stAaBbOS.pos_max.x, stAaBbOS.pos_max.y, stAaBbOS.pos_max.z);
		f3PosOrthoBoxesOS[6] = vmfloat3(stAaBbOS.pos_min.x, stAaBbOS.pos_max.y, stAaBbOS.pos_min.z);
		f3PosOrthoBoxesOS[7] = vmfloat3(stAaBbOS.pos_max.x, stAaBbOS.pos_max.y, stAaBbOS.pos_min.z);
		for (int i = 0; i < 8; i++)
		{
			vmfloat3 f3PosOrthoBoxSS;
			fTransformPoint(&f3PosOrthoBoxSS, &f3PosOrthoBoxesOS[i], &matOS2SS);
			vmint2 pos_xy = vmint2((int)f3PosOrthoBoxSS.x, (int)f3PosOrthoBoxSS.y);
			aaBbMinSS.x = min(aaBbMinSS.x, pos_xy.x);
			aaBbMinSS.y = min(aaBbMinSS.y, pos_xy.y);
			aaBbMaxSS.x = max(aaBbMaxSS.x, pos_xy.x);
			aaBbMaxSS.y = max(aaBbMaxSS.y, pos_xy.y);
		}
		w = aaBbMaxSS.x - aaBbMinSS.x;
		h = aaBbMaxSS.y - aaBbMinSS.y;
	};

	vector<VmActor> temperal_actors;
	vector<VmActor*> slicer_actors;
	int _w_max = 0;
	int _h_max = 0;
	// For Each Primitive //
	for (auto& actorPair : _fncontainer->sceneActors)
	{
		VmActor* actor = get<1>(actorPair);
		VmVObject* geo_obj = actor->GetGeometryRes();
		if (geo_obj == NULL ||
			geo_obj->GetObjectType() != ObjectTypePRIMITIVE ||
			!geo_obj->IsDefined() ||
			!actor->visible || actor->color.a == 0)
			continue;

		VmVObjectPrimitive* pobj = (VmVObjectPrimitive*)geo_obj;
		PrimitiveData* prim_data = pobj->GetPrimitiveData();
		if (prim_data->GetVerticeDefinition("POSITION") == NULL || 
			pobj->GetBVHTree() == NULL ||
			prim_data->ptype != PrimitiveTypeTRIANGLE)
			continue;

		//if (is_picking_routine) {
		//	if (prim_data->ptype == vmenums::PrimitiveTypeLINE //|| 
		//		//grd_helper::CollisionCheck(actor->matWS2OS, prim_data->aabb_os, picking_ray_origin, picking_ray_dir) ||
		//		//curved_slicer
		//		)
		//		slicer_actors.push_back(actor);
		//	//std::cout << "###### obb ray intersection : " << actor->actorId << std::endl;
		//	// NOTE THAT is_picking_routine allows only general_oit_routine_objs!!
		//	continue;
		//}

		//vmmat44f matOS2SS = actor->matOS2WS * matWS2SS;
		//int w, h;
		//__compute_computespace_screen(w, h, matOS2SS, prim_data->aabb_os);
		slicer_actors.push_back(actor);
	}

	bool gpu_profile = false;
	if (fb_size_cur.x > 200 && fb_size_cur.y > 200)
	{
		gpu_profile = _fncontainer->fnParams.GetParam("_bool_GpuProfile", false);
		if (gpu_profile)
		{
			cout << "  ** # of slicer actors    : " << slicer_actors.size() << endl;
		}
	}
#pragma endregion 

	map<string, vmint2>& profile_map = dx11CommonParams->profile_map;
	if (gpu_profile)
	{
		int gpu_profilecount = (int)profile_map.size();
		dx11DeviceImmContext->Begin(dx11CommonParams->dx11qr_disjoint);
		//gpu_profilecount++;
	}

	auto ___GpuProfile = [&gpu_profile, &dx11DeviceImmContext, &profile_map, &dx11CommonParams](const string& profile_name, const bool is_closed = false) {
		if (gpu_profile)
		{
			int stamp_idx = 0;
			auto it = profile_map.find(profile_name);
			if (it == profile_map.end()) {
				assert(is_closed == false);
				int gpu_profilecount = (int)profile_map.size() * 2;
				profile_map[profile_name] = vmint2(gpu_profilecount, -1);
				stamp_idx = gpu_profilecount;
			}
			else {
				assert(it->second.y == -1 && is_closed == true);
				it->second.y = it->second.x + 1;
				stamp_idx = it->second.y;
			}

			dx11DeviceImmContext->End(dx11CommonParams->dx11qr_timestamps[stamp_idx]);
			//gpu_profilecount++;
		}
	};

#pragma region // FrameBuffer Setting
	// Backup Previous Render Target //
	ID3D11RenderTargetView* pdxRTVOld = NULL;
	ID3D11DepthStencilView* pdxDSVOld = NULL;
	dx11DeviceImmContext->OMGetRenderTargets(1, &pdxRTVOld, &pdxDSVOld);

	___GpuProfile("Clear for Slicer Render - SR");

	// Clear Depth Stencil //
	ID3D11DepthStencilView* dx11DSV = (ID3D11DepthStencilView*)gres_fb_depthstencil.alloc_res_ptrs[DTYPE_DSV];
	dx11DeviceImmContext->ClearDepthStencilView(dx11DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

	float flt_max_ = FLT_MAX;
	uint flt_max_u = *(uint*)&flt_max_;
	uint clr_unit4[4] = { 0, 0, 0, 0 };
	uint clr_max_ufloat_4[4] = { flt_max_u, flt_max_u, flt_max_u, flt_max_u };
	float clr_float_zero_4[4] = { 0, 0, 0, 0 };
	float clr_float_fltmax_4[4] = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };
	//float clr_float_minus_4[4] = { -1.f, -1.f, -1.f, -1.f };
	dx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gres_fb_counter.alloc_res_ptrs[DTYPE_UAV], clr_unit4);

	dx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_RTV], clr_float_zero_4);
	dx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_RTV], clr_float_fltmax_4);

	___GpuProfile("Clear for Slicer Render - SR", true);
#pragma endregion 

#pragma region // HLSL Sampler Setting
	ID3D11SamplerState* sampler_PZ = dx11CommonParams->get_sampler("POINT_ZEROBORDER");
	ID3D11SamplerState* sampler_LZ = dx11CommonParams->get_sampler("LINEAR_ZEROBORDER");
	ID3D11SamplerState* sampler_PC = dx11CommonParams->get_sampler("POINT_CLAMP");
	ID3D11SamplerState* sampler_LC = dx11CommonParams->get_sampler("LINEAR_CLAMP");
	ID3D11SamplerState* sampler_PW = dx11CommonParams->get_sampler("POINT_WRAP");
	ID3D11SamplerState* sampler_LW = dx11CommonParams->get_sampler("LINEAR_WRAP");

	dx11DeviceImmContext->VSSetSamplers(0, 1, &sampler_LZ);
	dx11DeviceImmContext->VSSetSamplers(1, 1, &sampler_PZ);
	dx11DeviceImmContext->VSSetSamplers(2, 1, &sampler_LC);
	dx11DeviceImmContext->VSSetSamplers(3, 1, &sampler_PC);
	dx11DeviceImmContext->VSSetSamplers(4, 1, &sampler_LW);
	dx11DeviceImmContext->VSSetSamplers(5, 1, &sampler_PW);

	dx11DeviceImmContext->PSSetSamplers(0, 1, &sampler_LZ);
	dx11DeviceImmContext->PSSetSamplers(1, 1, &sampler_PZ);
	dx11DeviceImmContext->PSSetSamplers(2, 1, &sampler_LC);
	dx11DeviceImmContext->PSSetSamplers(3, 1, &sampler_PC);
	dx11DeviceImmContext->PSSetSamplers(4, 1, &sampler_LW);
	dx11DeviceImmContext->PSSetSamplers(5, 1, &sampler_PW);

	dx11DeviceImmContext->CSSetSamplers(0, 1, &sampler_LZ);
	dx11DeviceImmContext->CSSetSamplers(1, 1, &sampler_PZ);
	dx11DeviceImmContext->CSSetSamplers(2, 1, &sampler_LC);
	dx11DeviceImmContext->CSSetSamplers(3, 1, &sampler_PC);
	dx11DeviceImmContext->CSSetSamplers(4, 1, &sampler_LW);
	dx11DeviceImmContext->CSSetSamplers(5, 1, &sampler_PW);
#pragma endregion 

	ID3D11DepthStencilView* dx11DSVNULL = NULL;
	ID3D11RenderTargetView* dx11RTVsNULL[2] = { NULL, NULL };
	ID3D11UnorderedAccessView* dx11UAVs_NULL[7] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };
	ID3D11ShaderResourceView* dx11SRVs_NULL[7] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };
	// For Each Primitive //
	int count_call_render = 0;

#define NUM_UAVs 5
	auto PathTracer = [&dx11CommonParams, &dx11DeviceImmContext, &_fncontainer, &dx11LI_P, &dx11LI_PN, &dx11LI_PT, &dx11LI_PNT, &dx11LI_PTTT, &dx11VShader_P,
		&dx11VShader_PN, &dx11VShader_PT, &dx11VShader_PNT, &dx11VShader_PTTT, &dx11DSV,
		&gres_fb_counter, &gres_picking_buffer, &gres_fb_k_buffer, &gres_fb_rgba, &gres_fb_depthcs,
		&cbuf_cam_state, &cbuf_env_state, &cbuf_clip, &cbuf_pobj, &cbuf_vobj, &cbuf_reffect, &cbuf_tmap, &cbuf_hsmask,
		&num_grid_x, &num_grid_y, &matWS2PS, &matWS2SS, &matSS2WS,
		&light_src, &default_phong_lighting_coeff, &default_point_thickness, &default_surfel_size, &default_line_thickness, &default_color_cmmobj, &use_spinlock_pixsynch, &use_blending_option_MomentOIT,
		&count_call_render, &progress, &cam_obj, &planeThickness,
		&clr_float_zero_4, &clr_float_fltmax_4, &dx11DSVNULL, &dx11RTVsNULL, &dx11UAVs_NULL, &dx11SRVs_NULL
	](vector<VmActor*>& actor_list, const bool curved_slicer, const bool is_ghost_mode, const bool is_picking_routine)
	{

		// main geometry rendering process
		for (VmActor* actor : actor_list)
		{
			VmVObjectPrimitive* pobj = (VmVObjectPrimitive*)actor->GetGeometryRes();
			assert(pobj->GetBVHTree() != NULL);
			PrimitiveData* prim_data = pobj->GetPrimitiveData();
			// note that the actor is visible (already checked)
#pragma region Actor Parameters
			bool has_texture_img = actor->GetParam("_bool_HasTextureMap", false); 

			vmfloat4 material_phongCoeffs = actor->GetParam("_float4_PhongCoeffs", default_phong_lighting_coeff);
			bool use_vertex_color = actor->GetParam("_bool_UseVertexColor", false) && prim_data->GetVerticeDefinition("TEXCOORD0") != NULL;
#pragma endregion

#pragma region GPU resource updates
			//VmObject* tobj_otf = (VmObject*)actor->GetAssociateRes("OTF");
			//VmObject* tobj_maptable = (VmObject*)actor->GetAssociateRes("MAPTABLE");
			//VmVObjectVolume* vobj = (VmVObjectVolume*)actor->GetAssociateRes("VOLUME");

			CB_Material cbRenderEffect;
			grd_helper::SetCb_RenderingEffect(cbRenderEffect, actor);
			D3D11_MAPPED_SUBRESOURCE mappedResRenderEffect;
			dx11DeviceImmContext->Map(cbuf_reffect, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResRenderEffect);
			CB_Material* cbRenderEffectData = (CB_Material*)mappedResRenderEffect.pData;
			memcpy(cbRenderEffectData, &cbRenderEffect, sizeof(CB_Material));
			dx11DeviceImmContext->Unmap(cbuf_reffect, 0);

			GpuRes gres_vtx, gres_idx;
			map<string, GpuRes> map_gres_texs;
			grd_helper::UpdatePrimitiveModel(gres_vtx, gres_idx, map_gres_texs, pobj);
			int tex_map_enum = 0;
			//bool is_annotation_obj = pobj->GetObjParam("_bool_IsAnnotationObj", false) && prim_data->texture_res_info.size() > 0;
			//if (is_annotation_obj)
			//{
			//	auto it_tex = map_gres_texs.find("MAP_COLOR4");
			//	if (it_tex != map_gres_texs.end())
			//	{
			//		tex_map_enum = 1;
			//		GpuRes& gres_tex = it_tex->second;
			//		dx11DeviceImmContext->CSSetShaderResources(10, 1, (ID3D11ShaderResourceView**)&gres_tex.alloc_res_ptrs[DTYPE_SRV]);
			//	}
			//}
			//else 
			if (has_texture_img)
			{
				auto it_tex = map_gres_texs.find("MAP_COLOR4");
				if (it_tex != map_gres_texs.end())
				{
					tex_map_enum |= 0x1;
					GpuRes& gres_tex = it_tex->second;
					dx11DeviceImmContext->CSSetShaderResources(10, 1, (ID3D11ShaderResourceView**)&gres_tex.alloc_res_ptrs[DTYPE_SRV]);
				}

				for (int i = 0; i < NUM_MATERIALS; i++)
				{
					it_tex = map_gres_texs.find(g_materials[i]);
					if (it_tex != map_gres_texs.end())
					{
						tex_map_enum |= (0x1 << (i + 1));
						GpuRes& gres_tex = it_tex->second;
						dx11DeviceImmContext->CSSetShaderResources(11 + i, 1, (ID3D11ShaderResourceView**)&gres_tex.alloc_res_ptrs[DTYPE_SRV]);
					}
				}
			}

			CB_PolygonObject cbPolygonObj;
			cbPolygonObj.tex_map_enum = tex_map_enum;
			cbPolygonObj.pobj_dummy_0 = actor->actorId;// pobj->GetObjectID(); // used for picking
			grd_helper::SetCb_PolygonObj(cbPolygonObj, pobj, actor, matWS2SS, matWS2PS, false, use_vertex_color);
			cbPolygonObj.Ka *= material_phongCoeffs.x;
			cbPolygonObj.Kd *= material_phongCoeffs.y;
			cbPolygonObj.Ks *= material_phongCoeffs.z;
			cbPolygonObj.Ns *= material_phongCoeffs.w;
			if (default_color_cmmobj.x >= 0 && default_color_cmmobj.y >= 0 && default_color_cmmobj.z >= 0)
				cbPolygonObj.Ka = cbPolygonObj.Kd = cbPolygonObj.Ks = default_color_cmmobj;
			if (is_ghost_mode)
			{
				bool is_ghost_surface = actor->GetParam("_bool_IsGhostSurface", false);
				bool is_only_hotspot_visible = actor->GetParam("_bool_IsOnlyHotSpotVisible", false);
				if (is_ghost_surface) cbPolygonObj.pobj_flag |= 0x1 << 22;
				if (is_only_hotspot_visible) cbPolygonObj.pobj_flag |= 0x1 << 23;
				//cout << "TEST : " << is_ghost_surface << ", " << is_only_hotspot_visible << endl;
			}
			D3D11_MAPPED_SUBRESOURCE mappedResPobjData;
			dx11DeviceImmContext->Map(cbuf_pobj, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResPobjData);
			CB_PolygonObject* cbPolygonObjData = (CB_PolygonObject*)mappedResPobjData.pData;
			memcpy(cbPolygonObjData, &cbPolygonObj, sizeof(CB_PolygonObject));
			dx11DeviceImmContext->Unmap(cbuf_pobj, 0);

			CB_ClipInfo cbClipInfo;
			grd_helper::SetCb_ClipInfo(cbClipInfo, pobj, actor);
			D3D11_MAPPED_SUBRESOURCE mappedResClipInfo;
			dx11DeviceImmContext->Map(cbuf_clip, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResClipInfo);
			CB_ClipInfo* cbClipInfoData = (CB_ClipInfo*)mappedResClipInfo.pData;
			memcpy(cbClipInfoData, &cbClipInfo, sizeof(CB_ClipInfo));
			dx11DeviceImmContext->Unmap(cbuf_clip, 0);

			//dx11DeviceImmContext->CSSetConstantBuffers(0, 1, &cbuf_cam_state);
			dx11DeviceImmContext->CSSetConstantBuffers(1, 1, &cbuf_pobj);
			dx11DeviceImmContext->CSSetConstantBuffers(2, 1, &cbuf_clip);
			dx11DeviceImmContext->CSSetConstantBuffers(3, 1, &cbuf_reffect);
#pragma endregion

			ID3D11UnorderedAccessView* dx11UAVs[NUM_UAVs] = {
					(ID3D11UnorderedAccessView*)gres_fb_counter.alloc_res_ptrs[DTYPE_UAV], 
					(ID3D11UnorderedAccessView*)gres_fb_k_buffer.alloc_res_ptrs[DTYPE_UAV], 
					is_picking_routine ? (ID3D11UnorderedAccessView*)gres_picking_buffer.alloc_res_ptrs[DTYPE_UAV] : NULL,
					(ID3D11UnorderedAccessView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_UAV],
					(ID3D11UnorderedAccessView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_UAV]
			};

#define GETDEPTHSTENTIL(NAME) dx11CommonParams->get_depthstencil(#NAME)

			// to do ray-tracer
			//const bool fillInside = true;
			//if (fillInside)
			{
				vmint4* nodePtr;
				int nodeSize;
				vmint4* triWoopPtr;
				int triWoopSize;
				vmint4* triDebugPtr;
				int triDebugSize;
				int* cpuTriIndicesPtr;
				int triIndicesSize;
				pobj->GetBVHTreeBuffers(&nodePtr, &nodeSize, &triWoopPtr, &triWoopSize,
					&triDebugPtr, &triDebugSize, &cpuTriIndicesPtr, &triIndicesSize);

				GpuRes bvhNode, bvhTriWoop, bvhTriDebug, bvhIndice;
				grd_helper::UpdateCustomBuffer(bvhNode, pobj, "BvhNode", nodePtr, nodeSize, DXGI_FORMAT_R32G32B32A32_FLOAT, sizeof(vmint4));
				grd_helper::UpdateCustomBuffer(bvhTriWoop, pobj, "BvhTriWoop", triWoopPtr, triWoopSize, DXGI_FORMAT_R32G32B32A32_FLOAT, sizeof(vmint4));
				grd_helper::UpdateCustomBuffer(bvhTriDebug, pobj, "BvhTriDebug", triDebugPtr, triDebugSize, DXGI_FORMAT_R32G32B32A32_FLOAT, sizeof(vmint4));
				grd_helper::UpdateCustomBuffer(bvhIndice, pobj, "BvhIndice", cpuTriIndicesPtr, triIndicesSize, DXGI_FORMAT_R32_SINT, sizeof(int));

				dx11DeviceImmContext->CSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&bvhNode.alloc_res_ptrs[DTYPE_SRV]);
				dx11DeviceImmContext->CSSetShaderResources(1, 1, (ID3D11ShaderResourceView**)&bvhTriWoop.alloc_res_ptrs[DTYPE_SRV]);
				dx11DeviceImmContext->CSSetShaderResources(2, 1, (ID3D11ShaderResourceView**)&bvhTriDebug.alloc_res_ptrs[DTYPE_SRV]);
				dx11DeviceImmContext->CSSetShaderResources(3, 1, (ID3D11ShaderResourceView**)&bvhIndice.alloc_res_ptrs[DTYPE_SRV]);

				// test //
				if(0)
				{
					// mat_ss2ws
					auto TransformPoint = [](float3& p, vmmat44f& m) {
						float3 tp;
						fTransformPoint(&tp, &p, &m);
						return tp;
					};
					auto TransformVector = [](float3& p, vmmat44f& m) {
						float3 tp;
						fTransformVector(&tp, &p, &m);
						return tp;
					};

					bool bbb = cam_obj->IsPerspective();
					vmfloat3 pos_cam_ws, dir_view_ws;
					cam_obj->GetCameraExtStatef(&pos_cam_ws, &dir_view_ws, NULL);

					vmint2 ss_xy = vmint2(300, 300);

					// Image Plane's Position and Camera State //
					float3 pos_ip_ss = float3(ss_xy, 0.0f);
					float3 pos_ip_ws = TransformPoint(pos_ip_ss, matSS2WS);
					float3 ray_dir_unit_ws = dir_view_ws;
					if (cam_obj->IsPerspective())
						ray_dir_unit_ws = pos_ip_ws - pos_cam_ws;
					ray_dir_unit_ws = normalize(ray_dir_unit_ws);

					////////////////////////////
					int hitTriIdx = -1;
					int bestTriIdx = -1;
					int geomtype = -1;
					float hitDistance = 1e20;
					float scene_t = 1e20;
					float3 objcol = float3(0, 0, 0);
					float3 emit = float3(0, 0, 0);
					float3 hitpoint; // intersection point
					float3 n; // normal
					float3 nl; // oriented normal
					float3 nextdir; // ray direction of next path segment
					float3 trinormal = float3(0, 0, 0);
					Refl_t refltype;
					float ray_tmin = 0.00001f;
					float ray_tmax = 1e20; // use thickness!!

					// intersect all triangles in the scene stored in BVH
					int debugbingo = 0;
						
					float3 ray_orig_os = float3(0, 0, 0);// TransformPoint(pos_ip_ws, actor->matWS2OS);
					float3 ray_dir_unit_os = normalize(TransformVector(ray_dir_unit_ws, actor->matWS2OS) + float3(0.3, 0, 0));

					float4 rayorig = float4(ray_orig_os, ray_tmin);
					float4 raydir = float4(ray_dir_unit_os, ray_tmax);

					float3 __mmmax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
					float3 __mmmin(FLT_MAX, FLT_MAX, FLT_MAX);
					float4* ddd = (float4*)triDebugPtr;
					for (int ii = 0; ii < triDebugSize; ii++) {
						__mmmin = min3f(__mmmin, float3(ddd[ii].x, ddd[ii].y, ddd[ii].z));
						__mmmax = max3f(__mmmax, float3(ddd[ii].x, ddd[ii].y, ddd[ii].z));
					}

					DEBUGintersectBVHandTriangles(rayorig, raydir, (float4*)nodePtr, (float4*)triWoopPtr, (float4*)triDebugPtr, cpuTriIndicesPtr, bestTriIdx, hitDistance, debugbingo, trinormal, false);
					//int num_frags = intersectBVHandTriangles(rayorig, raydir, buf_gpuNodes, buf_gpuTriWoops, buf_gpuTriIndices, bestTriIdx, hitDistance, debugbingo, trinormal, false);

					int gg = 0;
				}

				dx11DeviceImmContext->CSSetUnorderedAccessViews(0, NUM_UAVs, dx11UAVs, NULL);

				if (curved_slicer)
					dx11DeviceImmContext->CSSetShader(
						is_picking_routine ? GETCS(PickingCurvedThickSlice_cs_5_0) :
						GETCS(CurvedThickSlicePathTracer_cs_5_0), NULL, 0);
				else
					dx11DeviceImmContext->CSSetShader(
						is_picking_routine ? GETCS(PickingThickSlice_cs_5_0) :
						GETCS(ThickSlicePathTracer_cs_5_0), NULL, 0);

				//dx11DeviceImmContext->Flush();
				dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);
				//dx11DeviceImmContext->Flush();

				if (planeThickness == 0.f) {
					dx11DeviceImmContext->CSSetShader(GETCS(SliceOutline_cs_5_0), NULL, 0);
					//dx11DeviceImmContext->Flush();
					dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);
					// SliceOutline_cs_5_0
				}
				
				dx11DeviceImmContext->CSSetUnorderedAccessViews(0, NUM_UAVs, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
			}

			if (is_picking_routine) 
				continue;

			// Lines Cuttins
#pragma region // Setting Rasterization Stages
			ID3D11InputLayout* dx11InputLayer_Target = NULL;
			ID3D11VertexShader* dx11VS_Target = NULL;
			ID3D11GeometryShader* dx11GS_Target = NULL;
			//ID3D11PixelShader* dx11PS_Target = NULL;
			ID3D11RasterizerState2* dx11RState_TargetObj = NULL;
			uint offset = 0;
			D3D_PRIMITIVE_TOPOLOGY pobj_topology_type;

			if (prim_data->GetVerticeDefinition("NORMAL"))
			{
				if (prim_data->GetVerticeDefinition("TEXCOORD0"))
				{
					// PNT (here, T is used as color)
					dx11InputLayer_Target = dx11LI_PNT;
					dx11VS_Target = dx11VShader_PNT;
				}
				else
				{
					// PN
					dx11InputLayer_Target = dx11LI_PN;
					dx11VS_Target = dx11VShader_PN;
				}
			}
			else if (prim_data->GetVerticeDefinition("TEXCOORD0"))
			{
				if (prim_data->GetVerticeDefinition("TEXCOORD2"))
				{
					dx11InputLayer_Target = dx11LI_PTTT;
					dx11VS_Target = dx11VShader_PTTT;
				}
				else
				{
					dx11InputLayer_Target = dx11LI_PT;
					dx11VS_Target = dx11VShader_PT;
				}
			}
			else
			{
				// P
				dx11InputLayer_Target = dx11LI_P;
				dx11VS_Target = dx11VShader_P;
			}

			assert(prim_data->ptype == PrimitiveTypeTRIANGLE);
			if (prim_data->is_stripe)
				pobj_topology_type = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
			else
				pobj_topology_type = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

#define GETRASTER(NAME) dx11CommonParams->get_rasterizer(#NAME)

			dx11RState_TargetObj = GETRASTER(SOLID_NONE);

			// cut lines to dx11GS_Target
			// dx11DeviceImmContext->GSSetConstantBuffers
			
			ID3D11Buffer* dx11BufferTargetPrim = (ID3D11Buffer*)gres_vtx.alloc_res_ptrs[DTYPE_RES];
			ID3D11Buffer* dx11IndiceTargetPrim = NULL;
			uint stride_inputlayer = sizeof(vmfloat3) * (uint)prim_data->GetNumVertexDefinitions();
			dx11DeviceImmContext->IASetVertexBuffers(0, 1, (ID3D11Buffer**)&dx11BufferTargetPrim, &stride_inputlayer, &offset);
			if (prim_data->vidx_buffer != NULL && prim_data->ptype != EvmPrimitiveType::PrimitiveTypePOINT)
			{
				dx11IndiceTargetPrim = (ID3D11Buffer*)gres_idx.alloc_res_ptrs[DTYPE_RES];
				dx11DeviceImmContext->IASetIndexBuffer(dx11IndiceTargetPrim, DXGI_FORMAT_R32_UINT, 0);
			}

			dx11DeviceImmContext->IASetInputLayout(dx11InputLayer_Target);
			dx11DeviceImmContext->VSSetShader(dx11VS_Target, NULL, 0);
			dx11DeviceImmContext->GSSetShader(dx11GS_Target, NULL, 0);
			dx11DeviceImmContext->RSSetState(dx11RState_TargetObj);
			dx11DeviceImmContext->IASetPrimitiveTopology(pobj_topology_type);
#pragma endregion 

#pragma region // GEO RENDERING PASS

			// https://stackoverflow.com/questions/12606556/how-do-you-use-geometry-shader-with-output-stream
			//dx11DeviceImmContext->GSSetShaderResources(0, NUM_UAVs, dx11UAVs, NULL);

			dx11DeviceImmContext->OMSetDepthStencilState(GETDEPTHSTENTIL(ALWAYS), 0);

			//dx11DeviceImmContext->Flush();
			if (prim_data->is_stripe || pobj_topology_type == D3D11_PRIMITIVE_TOPOLOGY_POINTLIST)
				dx11DeviceImmContext->Draw(prim_data->num_vtx, 0);
			else
				dx11DeviceImmContext->DrawIndexed(prim_data->num_vidx, 0, 0);
			count_call_render++;

			dx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(2, dx11RTVsNULL, dx11DSVNULL, 2, NUM_UAVs, dx11UAVs_NULL, 0);
#pragma endregion // GEO RENDERING PASS
		}
	};

	auto RenderOut = [&iobj, &___GpuProfile, &fb_size_cur, &dx11DeviceImmContext, &is_final_renderer, &planeThickness,
		&gres_fb_rgba, &gres_fb_depthcs, &gres_fb_sys_rgba, &gres_fb_sys_depthcs, &gpu_manager, &is_rgba](const int count_call_render, const HWND hWnd) {

		// APPLY HWND MODE
		if (hWnd && is_final_renderer)
		{
			ID3D11Texture2D* pTex2dHwndRT = NULL;
			ID3D11RenderTargetView* pHwndRTV = NULL;
			gpu_manager->UpdateDXGI((void**)&pTex2dHwndRT, (void**)&pHwndRTV, hWnd, fb_size_cur.x, fb_size_cur.y);

			dx11DeviceImmContext->CopyResource(pTex2dHwndRT, (ID3D11Texture2D*)gres_fb_rgba.alloc_res_ptrs[DTYPE_RES]);
			return;
		}

		// note CPU MPR renderer uses FrameBufferUsageRENDEROUT with index 1
		FrameBuffer* fb_rout = (FrameBuffer*)iobj->GetFrameBuffer(FrameBufferUsageRENDEROUT, planeThickness == 0.f && !is_final_renderer ? 1 : 0);
		FrameBuffer* fb_dout = (FrameBuffer*)iobj->GetFrameBuffer(FrameBufferUsageDEPTH, 0);

		if (count_call_render == 0)	// this means that there is no valid rendering pass
		{
			vmbyte4* rgba_buf = (vmbyte4*)fb_rout->fbuffer;
			float* depth_buf = (float*)fb_dout->fbuffer;

			memset(rgba_buf, 0, fb_size_cur.x * fb_size_cur.y * sizeof(vmbyte4));
			memset(depth_buf, 0x77, fb_size_cur.x * fb_size_cur.y * sizeof(float));
		}
		else
		{
#pragma region // Copy GPU to CPU
			dx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gres_fb_sys_rgba.alloc_res_ptrs[DTYPE_RES],
				(ID3D11Texture2D*)gres_fb_rgba.alloc_res_ptrs[DTYPE_RES]);
			dx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gres_fb_sys_depthcs.alloc_res_ptrs[DTYPE_RES],
				(ID3D11Texture2D*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_RES]);

			vmbyte4* rgba_sys_buf = (vmbyte4*)fb_rout->fbuffer;
			float* depth_sys_buf = (float*)fb_dout->fbuffer;

			D3D11_MAPPED_SUBRESOURCE mappedResSysRGBA;
			D3D11_MAPPED_SUBRESOURCE mappedResSysDepth;
			HRESULT hr = dx11DeviceImmContext->Map((ID3D11Texture2D*)gres_fb_sys_rgba.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, NULL, &mappedResSysRGBA);
			hr |= dx11DeviceImmContext->Map((ID3D11Texture2D*)gres_fb_sys_depthcs.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, NULL, &mappedResSysDepth);

			vmbyte4* rgba_gpu_buf = (vmbyte4*)mappedResSysRGBA.pData;
			float* depth_gpu_buf = (float*)mappedResSysDepth.pData;
			int buf_row_pitch = mappedResSysRGBA.RowPitch / 4;
#ifdef PPL_USE
			int count = fb_size_cur.y;
			parallel_for(int(0), count, [&](int i) // is_rgba, fb_size_cur, rgba_sys_buf, depth_sys_buf, rgba_gpu_buf, depth_gpu_buf, buf_row_pitch
#else
#pragma omp parallel for 
			for (int i = 0; i < fb_size_cur.y; i++)
#endif
			{
				for (int j = 0; j < fb_size_cur.x; j++)
				{
					vmbyte4 rgba = rgba_gpu_buf[j + i * buf_row_pitch];
					// __PS_MERGE_LAYERS_TO_RENDEROUT 에서 INT -> FLOAT4 로 되어 배열된 color 요소가 들어 옴. //

					// BGRA
					if (is_rgba)
						rgba_sys_buf[i * fb_size_cur.x + j] = vmbyte4(rgba.x, rgba.y, rgba.z, rgba.w);
					else
						rgba_sys_buf[i * fb_size_cur.x + j] = vmbyte4(rgba.z, rgba.y, rgba.x, rgba.w);

					int iAddr = i * fb_size_cur.x + j;
					if (rgba.w > 0)
						depth_sys_buf[iAddr] = depth_gpu_buf[j + i * buf_row_pitch];
					else
						depth_sys_buf[iAddr] = FLT_MAX;
				}
#ifdef PPL_USE
			});
#else
		}
#endif
			dx11DeviceImmContext->Unmap((ID3D11Texture2D*)gres_fb_sys_rgba.alloc_res_ptrs[DTYPE_RES], 0);
			dx11DeviceImmContext->Unmap((ID3D11Texture2D*)gres_fb_sys_depthcs.alloc_res_ptrs[DTYPE_RES], 0);
	}
};

	dx11DeviceImmContext->VSSetConstantBuffers(0, 1, &cbuf_cam_state);
	dx11DeviceImmContext->GSSetConstantBuffers(0, 1, &cbuf_cam_state);
	dx11DeviceImmContext->PSSetConstantBuffers(0, 1, &cbuf_cam_state);
	dx11DeviceImmContext->CSSetConstantBuffers(0, 1, &cbuf_cam_state);
	CB_CameraState cbCamState;
	grd_helper::SetCb_Camera(cbCamState, matWS2SS, matSS2WS, cam_obj, fb_size_cur, k_value, gi_v_thickness);
	cbCamState.far_plane = planeThickness;
	if (!is_system_out) {
		// which means the k-buffer will be used for the following renderer
		// stores the fragments into the k-buffer and do not store the rendering result into RT
		cbCamState.cam_flag |= (0x1 << 3);
	}
	auto SetCamConstBuf = [&dx11DeviceImmContext, &cbuf_cam_state](const CB_CameraState& cbCamState) {
		D3D11_MAPPED_SUBRESOURCE mappedResCamState;
		dx11DeviceImmContext->Map(cbuf_cam_state, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResCamState);
		CB_CameraState* cbCamStateData = (CB_CameraState*)mappedResCamState.pData;
		memcpy(cbCamStateData, &cbCamState, sizeof(CB_CameraState));
		dx11DeviceImmContext->Unmap(cbuf_cam_state, 0);
	};

	HWND hWnd = (HWND)_fncontainer->fnParams.GetParam("_hwnd_WindowHandle", (HWND)NULL);

	// RENDER BEGIN
	___GpuProfile("Render Slicer");

	if (is_picking_routine) {
		cbCamState.iSrCamDummy__1 = (picking_pos_ss.x & 0xFFFF | picking_pos_ss.y << 16);
		cbCamState.cam_flag |= (0x1 << 5);
		
		SetCamConstBuf(cbCamState);

		___GpuProfile("Picking");

		PathTracer(slicer_actors, curved_slicer, is_ghost_mode, true); // is_picking_routine = true

#pragma region copyback to sysmem
		dx11DeviceImmContext->CopyResource((ID3D11Buffer*)gres_picking_system_buffer.alloc_res_ptrs[DTYPE_RES],
			(ID3D11Buffer*)gres_picking_buffer.alloc_res_ptrs[DTYPE_RES]);

		map<int, float> picking_layers_id_depth;
		map<float, int> picking_layers_depth_id;
		D3D11_MAPPED_SUBRESOURCE mappedResSysPicking;
		HRESULT hr = dx11DeviceImmContext->Map((ID3D11Buffer*)gres_picking_system_buffer.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, NULL, &mappedResSysPicking);
		uint* picking_buf = (uint*)mappedResSysPicking.pData;
		int num_layers = 0;
		// note each layer has 5 integer-stored data 
		// id, depth, planePos.xyz
		for (int i = 0; i < max_picking_layers; i += 2) 
		{ 
			uint obj_id = picking_buf[i];
			if (obj_id == 0) break;

			float pick_depth = *(float*)&picking_buf[i + 1];

			auto it = picking_layers_id_depth.find(obj_id);
			if (it == picking_layers_id_depth.end()) {
				picking_layers_id_depth.insert(pair<int, float>(obj_id, pick_depth));
			}
			else {
				if (pick_depth < it->second) it->second = pick_depth;
			}
			num_layers++;
		}
		dx11DeviceImmContext->Unmap((ID3D11Buffer*)gres_picking_system_buffer.alloc_res_ptrs[DTYPE_RES], 0);
#pragma endregion copyback to sysmem
		___GpuProfile("Picking", true);

		//if (gpu_profile) {
		//	cout << "### NUM PICKING LAYERS : " << num_layers << endl;
		//	cout << "### NUM PICKING UNIQUE ID LAYERS : " << picking_layers_id_depth.size() << endl;
		//}
		for (auto& it : picking_layers_id_depth) {
			picking_layers_depth_id[it.second] = it.first;
		}

		vector<vmfloat3> picking_pos_out;
		vector<int> picking_id_out;
		for (auto& it : picking_layers_depth_id) {
			vmfloat3 pos_pick = picking_ray_origin + picking_ray_dir * it.first;
			picking_pos_out.push_back(pos_pick);
			picking_id_out.push_back(it.second);
		}

		_fncontainer->fnParams.SetParam("_vlist_float3_PickPos", picking_pos_out);
		_fncontainer->fnParams.SetParam("_vlist_int_PickId", picking_id_out);
		// END of Render Process for picking
	}
	else {
		cbCamState.iSrCamDummy__0 = *(uint*)&merging_beta;
		cbCamState.iSrCamDummy__2 = *(uint*)&scale_z_res;
		SetCamConstBuf(cbCamState);

		___GpuProfile("PathTracer");
		// buffer filling
		PathTracer(slicer_actors, curved_slicer, is_ghost_mode, false); // is_picking_routine = false
		___GpuProfile("PathTracer", true);

		// Set NULL States //
		//dx11DeviceImmContext->CSSetUnorderedAccessViews(1, NUM_UAVs_GEO, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
		dx11DeviceImmContext->CSSetShaderResources(0, 2, dx11SRVs_NULL);

		if (is_system_out) {
			___GpuProfile("Copyback");
			RenderOut(count_call_render, hWnd);
			___GpuProfile("Copyback", true);
		}
	}

	iobj->SetObjParam("_int_NumCallRenders", count_call_render);
	___GpuProfile("Render Slicer", true);

	if (gpu_profile)
	{
		dx11DeviceImmContext->End(dx11CommonParams->dx11qr_disjoint);

		// Wait for data to be available
		while (dx11DeviceImmContext->GetData(dx11CommonParams->dx11qr_disjoint, NULL, 0, 0) == S_FALSE)
		{
			Sleep(1);       // Wait a bit, but give other threads a chance to run
		}

		// Check whether timestamps were disjoint during the last frame
		D3D10_QUERY_DATA_TIMESTAMP_DISJOINT tsDisjoint;
		dx11DeviceImmContext->GetData(dx11CommonParams->dx11qr_disjoint, &tsDisjoint, sizeof(tsDisjoint), 0);
		if (!tsDisjoint.Disjoint)
		{
			auto DisplayDuration = [&tsDisjoint](UINT64 tsS, UINT64 tsE, const string& _test)
			{
				if (tsS == 0 || tsE == 0) return;
				cout << _test << " : " << float(tsE - tsS) / float(tsDisjoint.Frequency) * 1000.0f << " ms" << endl;
			};

			for (auto& it : profile_map) {
				UINT64 ts, te;
				dx11DeviceImmContext->GetData(dx11CommonParams->dx11qr_timestamps[it.second.x], &ts, sizeof(UINT64), 0);
				dx11DeviceImmContext->GetData(dx11CommonParams->dx11qr_timestamps[it.second.y], &te, sizeof(UINT64), 0);

				DisplayDuration(ts, te, it.first);
			}

			if (test_fps_profiling)
			{
				auto it = profile_map.find("SR Render");
				UINT64 ts, te;
				dx11DeviceImmContext->GetData(dx11CommonParams->dx11qr_timestamps[it->second.x], &ts, sizeof(UINT64), 0);
				dx11DeviceImmContext->GetData(dx11CommonParams->dx11qr_timestamps[it->second.y], &te, sizeof(UINT64), 0);
				ofstream file_rendertime;
				file_rendertime.open(".\\data\\frames_profile_rendertime.txt", std::ios_base::app);
				file_rendertime << float(te - ts) / float(tsDisjoint.Frequency) * 1000.0f << endl;
				file_rendertime.close();
			}
		}
	}

	dx11DeviceImmContext->ClearState();

	dx11DeviceImmContext->OMSetRenderTargets(1, &pdxRTVOld, pdxDSVOld);
	VMSAFE_RELEASE(pdxRTVOld);
	VMSAFE_RELEASE(pdxDSVOld);

	iobj->SetDescriptor("vismtv_inbuilt_renderergpudx module");

	iobj->SetObjParam("_ullong_LatestSrTime", vmhelpers::GetCurrentTimePack());
	//((std::mutex*)HDx11GetMutexGpuCriticalPath())->unlock();

//#define __COUNT_DEBUG
#ifdef __COUNT_DEBUG
	GpuRes gres_fb_counter_sys;
	{
		grd_helper::UpdateFrameBuffer(gres_fb_counter_sys, iobj, "SYSTEM_COUNTER", RTYPE_TEXTURE2D, NULL, DXGI_FORMAT_R32_UINT, UPFB_SYSOUT);

		dx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gres_fb_counter_sys.alloc_res_ptrs[DTYPE_RES],
			(ID3D11Texture2D*)gres_fb_counter.alloc_res_ptrs[DTYPE_RES]);

		D3D11_MAPPED_SUBRESOURCE mappedResSysTest;
		HRESULT hr = dx11DeviceImmContext->Map((ID3D11Texture2D*)gres_fb_counter_sys.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, NULL, &mappedResSysTest);
		int buf_row_pitch = mappedResSysTest.RowPitch / 4;
		uint* __count = (uint*)mappedResSysTest.pData;
		for (int i = 0; i < fb_size_cur.y; i++)
		{
			for (int j = 0; j < fb_size_cur.x; j++)
			{
				if (__count[j + i * buf_row_pitch] > 0)
					int gg = 0;
			}
		};
		dx11DeviceImmContext->Unmap((ID3D11Texture2D*)gres_fb_counter_sys.alloc_res_ptrs[DTYPE_RES], 0);
	}
#endif
	return true;
}