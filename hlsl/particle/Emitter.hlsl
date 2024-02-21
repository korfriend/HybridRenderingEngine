#include "../Sr_Common.hlsl"
#include "ParticleInterop.hlsli"

RWStructuredBuffer<Particle> particleBuffer : register(u0);
RWStructuredBuffer<uint> aliveBuffer_CURRENT : register(u1); // actually same to RWByteAddressBuffer (byte address vs. structure-size stride address)
RWStructuredBuffer<uint> aliveBuffer_NEW : register(u2); // will be used in the particle simulation
RWStructuredBuffer<uint> deadBuffer : register(u3);
RWByteAddressBuffer counterBuffer : register(u4);
RWByteAddressBuffer indirectBuffers : register(u5);
RWStructuredBuffer<float> distanceBuffer : register(u6);
RWByteAddressBuffer vertexBuffer_POS : register(u7);
RWByteAddressBuffer vertexBuffer_NOR : register(u8);
RWBuffer<unorm float2> vertexBuffer_UVS : register(u9);
RWBuffer<unorm float4> vertexBuffer_COL : register(u10);
RWStructuredBuffer<uint> culledIndirectionBuffer : register(u11);
RWStructuredBuffer<uint> culledIndirectionBuffer2 : register(u12);

[numthreads(1, 1, 1)]
void KickoffEmitterSystem(uint3 DTid : SV_DispatchThreadID)
{
	// Load dead particle count:
	uint deadCount = counterBuffer.Load(PARTICLECOUNTER_OFFSET_DEADCOUNT);

	// Load alive particle count:
	uint aliveCount_NEW = counterBuffer.Load(PARTICLECOUNTER_OFFSET_ALIVECOUNT_AFTERSIMULATION);

	// we can not emit more than there are free slots in the dead list:
	uint realEmitCount = min(deadCount, g_emitter.xEmitCount);

	// Fill dispatch argument buffer for emitting (ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ):
	indirectBuffers.Store3(ARGUMENTBUFFER_OFFSET_DISPATCHEMIT, uint3(ceil((float)realEmitCount / (float)THREADCOUNT_EMIT), 1, 1));

	// Fill dispatch argument buffer for simulation (ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ):
	indirectBuffers.Store3(ARGUMENTBUFFER_OFFSET_DISPATCHSIMULATION, uint3(ceil((float)(aliveCount_NEW + realEmitCount) / (float)THREADCOUNT_SIMULATION), 1, 1));

	// copy new alivelistcount to current alivelistcount:
	counterBuffer.Store(PARTICLECOUNTER_OFFSET_ALIVECOUNT, aliveCount_NEW);

	// reset new alivecount:
	counterBuffer.Store(PARTICLECOUNTER_OFFSET_ALIVECOUNT_AFTERSIMULATION, 0);

	// reset culled count:
	counterBuffer.Store(PARTICLECOUNTER_OFFSET_CULLEDCOUNT, 0);

	// write real emit count:
	counterBuffer.Store(PARTICLECOUNTER_OFFSET_REALEMITCOUNT, realEmitCount);

	// reset cell allocator:
	counterBuffer.Store(PARTICLECOUNTER_OFFSET_CELLALLOCATOR, 0);
}

[numthreads(THREADCOUNT_EMIT, 1, 1)]
void ParticleEmitter(uint3 DTid : SV_DispatchThreadID)
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

	float4 baseColor = float4(g_cbPobj.Kd, g_cbPobj.alpha);
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

[numthreads(1, 1, 1)]
void ParticleUpdateFinish(uint3 DTid : SV_DispatchThreadID)
{
	uint particleCount = counterBuffer.Load(PARTICLECOUNTER_OFFSET_CULLEDCOUNT);

	if (g_emitter.xEmitterOptions & EMITTER_OPTION_BIT_MESH_SHADER_ENABLED)
	{
		// Create DispatchMesh argument buffer:
		IndirectDispatchArgs args;
		args.ThreadGroupCountX = (particleCount + 31) / 32;
		args.ThreadGroupCountY = 1;
		args.ThreadGroupCountZ = 1;
#ifdef __PSSL__
		indirectBuffers.TypedStore<IndirectDispatchArgs>(ARGUMENTBUFFER_OFFSET_DRAWPARTICLES, args);
#else
		//indirectBuffers.Store<IndirectDispatchArgs>(ARGUMENTBUFFER_OFFSET_DRAWPARTICLES, args);
		indirectBuffers.Store3(ARGUMENTBUFFER_OFFSET_DRAWPARTICLES, 
			uint3(args.ThreadGroupCountX, args.ThreadGroupCountY, args.ThreadGroupCountZ));
#endif // __PSSL__
	}
	else
	{
		// Create draw argument buffer
		IndirectDrawArgsInstanced args;
		args.VertexCountPerInstance = 4;
		args.InstanceCount = particleCount;
		args.StartVertexLocation = 0;
		args.StartInstanceLocation = 0;
#ifdef __PSSL__
		indirectBuffers.TypedStore<IndirectDrawArgsInstanced>(ARGUMENTBUFFER_OFFSET_DRAWPARTICLES, args);
#else
		//indirectBuffers.Store<IndirectDrawArgsInstanced>(ARGUMENTBUFFER_OFFSET_DRAWPARTICLES, args);
		indirectBuffers.Store4(ARGUMENTBUFFER_OFFSET_DRAWPARTICLES, 
			uint4(args.VertexCountPerInstance, args.InstanceCount, args.StartVertexLocation, args.StartInstanceLocation));
#endif // __PSSL__
	}
}

[numthreads(THREADCOUNT_SIMULATION, 1, 1)]
void ParticleSimulation(uint3 DTid : SV_DispatchThreadID, uint Gid : SV_GroupIndex)
{
	uint aliveCount = counterBuffer.Load(PARTICLECOUNTER_OFFSET_ALIVECOUNT);

	if (DTid.x >= aliveCount)
		return;

	// simulation can be either fixed or variable timestep:
	const float dt = g_emitter.xEmitterFixedTimestep >= 0 ? g_emitter.xEmitterFixedTimestep : g_frame.delta_time;

	uint particleIndex = aliveBuffer_CURRENT[DTid.x];
	Particle particle = particleBuffer[particleIndex];
	uint v0 = particleIndex * 4;

	const float lifeLerp = 1 - particle.life / particle.maxLife;
	const float particleSize = lerp(particle.sizeBeginEnd.x, particle.sizeBeginEnd.y, lifeLerp);

	// integrate:
	particle.force += g_emitter.xParticleGravity;
	particle.velocity += particle.force * dt;
	particle.position += particle.velocity * dt;

	// reset force for next frame:
	particle.force = 0;

	// drag: 
	particle.velocity *= g_emitter.xParticleDrag;

	// Blocker shadow map check using previous frame:
	//	This is extended with blocker position calculation for splashing
	//[branch]
	//if ((xEmitterOptions & EMITTER_OPTION_BIT_USE_RAIN_BLOCKER) && g_frame.texture_shadowatlas_index >= 0 && any(g_frame.rain_blocker_mad_prev))
	//{
	//	Texture2D texture_shadowatlas = bindless_textures[g_frame.texture_shadowatlas_index];
	//	float3 shadow_pos = mul(g_frame.rain_blocker_matrix_prev, float4(particle.position, 1)).xyz;
	//	float3 shadow_uv = clipspace_to_uv(shadow_pos);
	//	if (is_saturated(shadow_uv))
	//	{
	//		shadow_uv.xy = mad(shadow_uv.xy, g_frame.rain_blocker_mad_prev.xy, g_frame.rain_blocker_mad_prev.zw);
	//		float shadow = texture_shadowatlas.SampleLevel(sampler_point_clamp, shadow_uv.xy, 0).r;
	//
	//		if (shadow > shadow_pos.z)
	//		{
	//			// Under blocker, make a splash by placing above blocker and modulating some params:
	//			float3 blockerPos = reconstruct_position(clipspace_to_uv(shadow_pos.xy), shadow, g_frame.rain_blocker_matrix_inverse_prev);
	//			particle.velocity = 0;
	//			float splashSize = GetWeather().rain_splash_scale;
	//			particle.position = blockerPos + float3(0, splashSize * 0.5, 0);
	//			particle.sizeBeginEnd = splashSize;
	//			particle.life = 0.15;
	//		}
	//	}
	//}

	if (particle.life > 0)
	{
		[branch]
		if (g_emitter.xEmitterOptions & EMITTER_OPTION_BIT_SPH_ENABLED)
		{
			// debug collisions:

			float elastic = 0.6;

#ifdef SPH_FLOOR_COLLISION
			// floor collision:
			if (particle.position.y - particleSize < 0)
			{
				particle.position.y = particleSize;
				particle.velocity.y *= -elastic;
			}
#endif // FLOOR_COLLISION

		}

		particle.life -= dt;

		// write back simulated particle:
		particleBuffer[particleIndex] = particle;

		// add to new alive list:
		uint newAliveIndex;
		counterBuffer.InterlockedAdd(PARTICLECOUNTER_OFFSET_ALIVECOUNT_AFTERSIMULATION, 1, newAliveIndex);
		aliveBuffer_NEW[newAliveIndex] = particleIndex;

		// Write out render buffers:
		//	These must be persistent, not culled (raytracing, surfels...)

		float4 baseColor = float4(g_cbPobj.Kd, g_cbPobj.alpha);
		float opacity = saturate(lerp(1, 0, lifeLerp) * baseColor.a);
		float4 particleColor = unpack_rgba(particle.color);
		particleColor.a *= opacity;

		float rotation = lifeLerp * particle.rotationalVelocity;
		float2x2 rot = float2x2(
			cos(rotation), -sin(rotation),
			sin(rotation), cos(rotation)
			);

		// Sprite sheet frame:
		const float spriteframe = g_emitter.xEmitterFrameRate == 0 ?
			lerp(g_emitter.xEmitterFrameStart, g_emitter.xEmitterFrameCount, lifeLerp) :
			((g_emitter.xEmitterFrameStart + particle.life * g_emitter.xEmitterFrameRate) % g_emitter.xEmitterFrameCount);
		const uint currentFrame = floor(spriteframe);
		const uint nextFrame = ceil(spriteframe);
		const float frameBlend = frac(spriteframe);
		uint2 offset = uint2(currentFrame % g_emitter.xEmitterFramesXY.x, currentFrame / g_emitter.xEmitterFramesXY.x);
		uint2 offset2 = uint2(nextFrame % g_emitter.xEmitterFramesXY.x, nextFrame / g_emitter.xEmitterFramesXY.x);

		for (uint vertexID = 0; vertexID < 4; ++vertexID)
		{
			// expand the point into a billboard in view space:
			float3 quadPos = BILLBOARD[vertexID];
			float2 uv = quadPos.xy * float2(0.5f, -0.5f) + 0.5f;
			float2 uv2 = uv;

			// sprite sheet UV transform:
			uv.xy += offset;
			uv.xy *= g_emitter.xEmitterTexMul;
			uv2.xy += offset2;
			uv2.xy *= g_emitter.xEmitterTexMul;

			// rotate the billboard:
			quadPos.xy = mul(quadPos.xy, rot);

			// scale the billboard:
			quadPos *= particleSize;

			// scale the billboard along view space motion vector:
			//float3 velocity = mul((float3x3)GetCamera().view, particle.velocity);
			float3 velocity = mul((float3x3)g_cbCamState.mat_ws2cs, particle.velocity); //ws2cs
			quadPos += dot(quadPos, velocity) * velocity * g_emitter.xParticleMotionBlurAmount;
			
			// rotate the billboard to face the camera:
			//quadPos = mul(quadPos, (float3x3)GetCamera().view); // reversed mul for inverse camera rotation!
			quadPos = mul(quadPos, (float3x3)g_cbCamState.mat_ws2cs); // reversed mul for inverse camera rotation!

			// write out vertex:
			//vertexBuffer_POS.Store<float3>((v0 + vertexID) * sizeof(float3), particle.position + quadPos);
			float3 p_vtx = particle.position + quadPos;
			uint3 p_asuint = asuint(p_vtx);// uint3(asuint(p_vtx.x), asuint(p_vtx.y), asuint(p_vtx.z));
			vertexBuffer_POS.Store3((v0 + vertexID) * 12, p_asuint);

			uint3 n_asuint = uint3(asuint(g_cbCamState.dir_view_ws.x), asuint(g_cbCamState.dir_view_ws.y), asuint(g_cbCamState.dir_view_ws.z));
			vertexBuffer_NOR.Store3((v0 + vertexID) * 12, n_asuint); //[v0 + vertexID] = float4(normalize(-g_cbCamState.dir_view_ws), 0);
			vertexBuffer_UVS[v0 + vertexID] = uv;// float4(uv, uv2);
			vertexBuffer_COL[v0 + vertexID] = particleColor;
		}

		// Frustum culling:
		//ShaderSphere sphere;
		//sphere.center = particle.position;
		//sphere.radius = particleSize;

		if (1)//GetCamera().frustum.intersects(sphere))
		{
			uint prevCount;
			counterBuffer.InterlockedAdd(PARTICLECOUNTER_OFFSET_CULLEDCOUNT, 1, prevCount);

			culledIndirectionBuffer[prevCount] = prevCount; // for sorting
			culledIndirectionBuffer2[prevCount] = particleIndex;

#ifdef SORTING
			// store squared distance to main camera:
			float3 eyeVector = particle.position - g_cbCamState.pos_cam_ws;
			float distSQ = dot(eyeVector, eyeVector);
			distanceBuffer[prevCount] = -distSQ; // this can be negated to modify sorting order here instead of rewriting sorting shaders...
#endif // SORTING
		}

	}
	else
	{
		// kill:
		uint deadIndex;
		counterBuffer.InterlockedAdd(PARTICLECOUNTER_OFFSET_DEADCOUNT, 1, deadIndex);
		deadBuffer[deadIndex] = particleIndex;

		float3 p_vtx = float3(1.f, 2.f, 3.f);
		uint3 p_asuint = uint3(asuint(p_vtx.x), asuint(p_vtx.y), asuint(p_vtx.z));

		vertexBuffer_POS.Store3((v0 + 0) * 12, (uint3)0);
		vertexBuffer_POS.Store3((v0 + 1) * 12, (uint3)0);
		vertexBuffer_POS.Store3((v0 + 2) * 12, (uint3)0);
		vertexBuffer_POS.Store3((v0 + 3) * 12, (uint3)0);
		//vertexBuffer_PNTC.Store3((v0 + 0) * 32, (uint3)0);
		//vertexBuffer_PNTC.Store3((v0 + 1) * 32, (uint3)0);
		//vertexBuffer_PNTC.Store3((v0 + 2) * 32, (uint3)0);
		//vertexBuffer_PNTC.Store3((v0 + 3) * 32, (uint3)0);
	}
}
