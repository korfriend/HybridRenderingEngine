// Random number generator based on: https://github.com/diharaw/helios/blob/master/src/engine/shader/random.glsl
struct RNG
{
	uint2 s; // state

	// xoroshiro64* random number generator.
	// http://prng.di.unimi.it/xoroshiro64star.c
	uint rotl(uint x, uint k)
	{
		return (x << k) | (x >> (32 - k));
	}
	// Xoroshiro64* RNG
	uint next()
	{
		uint result = s.x * 0x9e3779bb;

		s.y ^= s.x;
		s.x = rotl(s.x, 26) ^ s.y ^ (s.y << 9);
		s.y = rotl(s.y, 13);

		return result;
	}
	// Thomas Wang 32-bit hash.
	// http://www.reedbeta.com/blog/quick-and-easy-gpu-random-numbers-in-d3d11/
	uint hash(uint seed)
	{
		seed = (seed ^ 61) ^ (seed >> 16);
		seed *= 9;
		seed = seed ^ (seed >> 4);
		seed *= 0x27d4eb2d;
		seed = seed ^ (seed >> 15);
		return seed;
	}

	void init(uint2 id, uint frameIndex)
	{
		uint s0 = (id.x << 16) | id.y;
		uint s1 = frameIndex;
		s.x = hash(s0);
		s.y = hash(s1);
		next();
	}
	float next_float()
	{
		uint u = 0x3f800000 | (next() >> 9);
		return asfloat(u) - 1.0;
	}
	uint next_uint(uint nmax)
	{
		float f = next_float();
		return uint(floor(f * nmax));
	}
	float2 next_float2()
	{
		return float2(next_float(), next_float());
	}
	float3 next_float3()
	{
		return float3(next_float(), next_float(), next_float());
	}
};

#define PASTE1(a, b) a##b
#define PASTE(a, b) PASTE1(a, b)
#define CBUFFER(name, slot) cbuffer name : register(PASTE(b, slot))

struct HxCB_Emitter {
	float4x4	xEmitterTransform;
	float4x4	xEmitterBaseMeshUnormRemap;

	uint		xEmitCount;
	float		xEmitterRandomness;
	float		xParticleRandomColorFactor;
	float		xParticleSize;

	float		xParticleScaling;
	float		xParticleRotation;
	float		xParticleRandomFactor;
	float		xParticleNormalFactor;

	float		xParticleLifeSpan;
	float		xParticleLifeSpanRandomness;
	float		xParticleMass;
	float		xParticleMotionBlurAmount;

	uint		xEmitterMaxParticleCount;
	uint		xEmitterInstanceIndex;
	uint		xEmitterMeshGeometryOffset;
	uint		xEmitterMeshGeometryCount;

	uint2		xEmitterFramesXY;
	uint		xEmitterFrameCount;
	uint		xEmitterFrameStart;

	float2		xEmitterTexMul;
	float		xEmitterFrameRate;
	uint		xEmitterLayerMask;

	float		xSPH_h;					// smoothing radius
	float		xSPH_h_rcp;				// 1.0f / smoothing radius
	float		xSPH_h2;				// smoothing radius ^ 2
	float		xSPH_h3;				// smoothing radius ^ 3

	float		xSPH_poly6_constant;	// precomputed Poly6 kernel constant term
	float		xSPH_spiky_constant;	// precomputed Spiky kernel function constant term
	float		xSPH_visc_constant;	    // precomputed viscosity kernel function constant term
	float		xSPH_K;					// pressure constant

	float		xSPH_e;					// viscosity constant
	float		xSPH_p0;				// reference density
	uint		xEmitterOptions;
	float		xEmitterFixedTimestep;	// we can force a fixed timestep (>0) onto the simulation to avoid blowing up

	float3		xParticleGravity;
	float		xEmitterRestitution;

	float3		xParticleVelocity;
	float		xParticleDrag;
};

struct HxCB_FrameCB
{
	uint		options;					// renderer bool options packed into bitmask (OPTION_BIT_ values)
	float		time;
	float		time_previous;
	float		delta_time;

	uint		frame_count;
	uint		temporalaa_samplerotation;

	uint		forcefieldarray_offset;		// indexing into entity array
	uint		forcefieldarray_count;		// indexing into entity array
};

struct ParticleCounters
{
	uint aliveCount;
	uint deadCount;
	uint realEmitCount;
	uint aliveCount_afterSimulation;
	uint culledCount;
	uint cellAllocator;
};

static const float3 BILLBOARD[] = {
	float3(-1, -1, 0),	// 0
	float3(1, -1, 0),	// 1
	float3(-1, 1, 0),	// 2
	float3(1, 1, 0),	// 4
};

struct IndirectDispatchArgs
{
	uint ThreadGroupCountX;
	uint ThreadGroupCountY;
	uint ThreadGroupCountZ;
};

struct IndirectDrawArgsInstanced
{
	uint VertexCountPerInstance;
	uint InstanceCount;
	uint StartVertexLocation;
	uint StartInstanceLocation;
};

CBUFFER(EmittedParticleCB, 11)
{
	HxCB_Emitter g_emitter;
};

CBUFFER(EmittedParticleCB, 12)
{
	HxCB_FrameCB g_frame;
};

#define THREADCOUNT_EMIT 256

// GPU Alignment Trick
//template<typename T>
//constexpr T AlignTo(T value, T alignment)
//{
//	return ((value + alignment - T(1)) / alignment) * alignment;
//}
//template<typename T>
//constexpr bool IsAligned(T value, T alignment)
//{
//	return value == AlignTo(value, alignment);
//}

static const uint IndirectDrawArgsAlignment = 4u;
static const uint IndirectDispatchArgsAlignment = 4u;

// https://support.functionbay.com/ko/technical-tip/single/293
// SPH will be sorted into a hashed grid structure and response lookup will be accelerated
static const uint SPH_PARTITION_BUCKET_COUNT = 128 * 128 * 64;
static const uint THREADCOUNT_SIMULATION = 64;

static const uint ARGUMENTBUFFER_OFFSET_DISPATCHEMIT = 0;
static const uint ARGUMENTBUFFER_OFFSET_DISPATCHSIMULATION = 12; // AlignTo(ARGUMENTBUFFER_OFFSET_DISPATCHEMIT + (3 * 4), IndirectDispatchArgsAlignment);
static const uint ARGUMENTBUFFER_OFFSET_DRAWPARTICLES = 24; // AlignTo(ARGUMENTBUFFER_OFFSET_DISPATCHSIMULATION + (3 * 4), IndirectDrawArgsAlignment);

static const uint PARTICLECOUNTER_OFFSET_ALIVECOUNT = 0;
static const uint PARTICLECOUNTER_OFFSET_DEADCOUNT = PARTICLECOUNTER_OFFSET_ALIVECOUNT + 4;
static const uint PARTICLECOUNTER_OFFSET_REALEMITCOUNT = PARTICLECOUNTER_OFFSET_DEADCOUNT + 4;
static const uint PARTICLECOUNTER_OFFSET_ALIVECOUNT_AFTERSIMULATION = PARTICLECOUNTER_OFFSET_REALEMITCOUNT + 4;
static const uint PARTICLECOUNTER_OFFSET_CULLEDCOUNT = PARTICLECOUNTER_OFFSET_ALIVECOUNT_AFTERSIMULATION + 4;
static const uint PARTICLECOUNTER_OFFSET_CELLALLOCATOR = PARTICLECOUNTER_OFFSET_CULLEDCOUNT + 4;

static const uint EMITTER_OPTION_BIT_FRAME_BLENDING_ENABLED = 1 << 0;
static const uint EMITTER_OPTION_BIT_SPH_ENABLED = 1 << 1;
static const uint EMITTER_OPTION_BIT_MESH_SHADER_ENABLED = 1 << 2;
static const uint EMITTER_OPTION_BIT_COLLIDERS_DISABLED = 1 << 3;
static const uint EMITTER_OPTION_BIT_USE_RAIN_BLOCKER = 1 << 4;
static const uint EMITTER_OPTION_BIT_TAKE_COLOR_FROM_MESH = 1 << 5;

inline uint pack_rgba(in float4 value)
{
	uint retVal = 0;
	retVal |= (uint)(value.x * 255.0) << 0u;
	retVal |= (uint)(value.y * 255.0) << 8u;
	retVal |= (uint)(value.z * 255.0) << 16u;
	retVal |= (uint)(value.w * 255.0) << 24u;
	return retVal;
}

inline float4 unpack_rgba(in uint value)
{
	float4 retVal;
	retVal.x = (float)((value >> 0u) & 0xFF) / 255.0;
	retVal.y = (float)((value >> 8u) & 0xFF) / 255.0;
	retVal.z = (float)((value >> 16u) & 0xFF) / 255.0;
	retVal.w = (float)((value >> 24u) & 0xFF) / 255.0;
	return retVal;
}