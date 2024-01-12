#include "../Sr_Common.hlsl"

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

struct Particle
{
	float3 position;
	float mass;
	float3 force;
	float rotationalVelocity;
	float3 velocity;
	float maxLife;
	float2 sizeBeginEnd;
	float life;
	uint color;
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
	uint		frame_count;
	float		time;
	float		time_previous;
	float		delta_time;
};

CBUFFER(EmittedParticleCB, 11)
{
	HxCB_Emitter g_emitter;
};

CBUFFER(EmittedParticleCB, 12)
{
	HxCB_FrameCB g_frame;
};


RWStructuredBuffer<Particle> particleBuffer : register(u0);
RWStructuredBuffer<uint> aliveBuffer_CURRENT : register(u1);
RWStructuredBuffer<uint> aliveBuffer_NEW : register(u2);
RWStructuredBuffer<uint> deadBuffer : register(u3);
RWByteAddressBuffer counterBuffer : register(u4);

#define THREADCOUNT_EMIT 1000

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

[numthreads(THREADCOUNT_EMIT, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	uint emitCount = counterBuffer.Load(PARTICLECOUNTER_OFFSET_REALEMITCOUNT);
	if (DTid.x >= emitCount)
		return;

	RNG rng;
	rng.init(uint2(g_emitter.xEmitterRandomness, DTid.x), g_frame.frame_count);

	const float4x4 worldMatrix = g_emitter.xEmitterTransform;
	float3 emitPos = 0;
	float3 nor = 0;
	float3 velocity = g_emitter.xParticleVelocity;
	
	//float3 Ka, Kd, Ks;
	//float Ns = g_cbPobj.Ns;
	//// note g_cbPobj's Ka, Kd, and Ks has already been multiplied by pb_shading_factor.xyz
	//Ka = g_cbPobj.Ka;
	//Kd = g_cbPobj.Kd;
	//Ks = g_cbPobj.Ks;

	float4 baseColor = g_cbPobj.Kd;

#ifdef EMITTER_VOLUME
	// Emit inside volume:
	emitPos = float3(rng.next_float() * 2 - 1, rng.next_float() * 2 - 1, rng.next_float() * 2 - 1);
#else
	// Just emit from center point:
	emitPos = 0;
#endif // EMITTER_VOLUME

	float3 pos = mul(worldMatrix, float4(emitPos, 1)).xyz;

	float particleStartingSize = g_emitter.xParticleSize + g_emitter.xParticleSize * (rng.next_float() - 0.5f) * g_emitter.xParticleRandomFactor;

	// create new particle:
	Particle particle;
	particle.position = pos;
	particle.force = 0;
	particle.mass = g_emitter.xParticleMass;
	particle.velocity = velocity + (nor + (float3(rng.next_float(), rng.next_float(), rng.next_float()) - 0.5f) * g_emitter.xParticleRandomFactor) * g_emitter.xParticleNormalFactor;
	particle.rotationalVelocity = g_emitter.xParticleRotation + (rng.next_float() - 0.5f) * g_emitter.xParticleRandomFactor;
	particle.maxLife = g_emitter.xParticleLifeSpan + g_emitter.xParticleLifeSpan * (rng.next_float() - 0.5f) * g_emitter.xParticleLifeSpanRandomness;
	particle.life = particle.maxLife;
	particle.sizeBeginEnd = float2(particleStartingSize, particleStartingSize * g_emitter.xParticleScaling);

	baseColor.r *= lerp(1, rng.next_float(), g_emitter.xParticleRandomColorFactor);
	baseColor.g *= lerp(1, rng.next_float(), g_emitter.xParticleRandomColorFactor);
	baseColor.b *= lerp(1, rng.next_float(), g_emitter.xParticleRandomColorFactor);
	particle.color = pack_rgba(baseColor);

	// new particle index retrieved from dead list (pop):
	uint deadCount;
	counterBuffer.InterlockedAdd(PARTICLECOUNTER_OFFSET_DEADCOUNT, -1, deadCount);
	uint newParticleIndex = deadBuffer[deadCount - 1];

	// write out the new particle:
	particleBuffer[newParticleIndex] = particle;

	// and add index to the alive list (push):
	uint aliveCount;
	counterBuffer.InterlockedAdd(PARTICLECOUNTER_OFFSET_ALIVECOUNT, 1, aliveCount);
	aliveBuffer_CURRENT[aliveCount] = newParticleIndex;
}