inline float3 attribute_at_bary(in float3 a0, in float3 a1, in float3 a2, in float2 bary)
{
	return mad(a0, 1 - bary.x - bary.y, mad(a1, bary.x, a2 * bary.y));
}

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