#include "../Sr_Common.hlsl"
#include "ParticleInterop.hlsli"

ByteAddressBuffer vertexBuffer_POS : register(t53);
ByteAddressBuffer vertexBuffer_NOR : register(t54);
Buffer<unorm float2> vertexBuffer_UVS : register(t55);
Buffer<unorm float4> vertexBuffer_COL : register(t56);

StructuredBuffer<Particle> particleBuffer : register(t50);
StructuredBuffer<uint> culledIndirectionBuffer : register(t51);
StructuredBuffer<uint> culledIndirectionBuffer2 : register(t52);

struct VertextoPixel
{
    float4 pos : SV_POSITION;
    float clip : SV_ClipDistance0;
    min16float2 tex : TEXCOORD0;
    float3 P : WORLDPOSITION;
    min16float2 unrotated_uv : UNROTATED_UV;
    nointerpolation min16float frameBlend : FRAMEBLEND;
    nointerpolation min16float size : PARTICLESIZE;
    nointerpolation uint color : PARTICLECOLOR;
};

VertextoPixel CommonVS_IDX(uint vid : SV_VertexID, uint instanceID : SV_InstanceID)
{
    // culledIndirectionBuffer implies sorted order instances (optional)
    uint particleIndex = culledIndirectionBuffer2[culledIndirectionBuffer[instanceID]];
    uint vertexID = particleIndex * 4 + vid;

    //float3 position = vertexBuffer_POS[vertexID].xyz;
    uint3 u3Position = vertexBuffer_POS.Load3(vertexID * 3 * 4);
    float3 position = asfloat(u3Position);
    //float3 normal = vertexBuffer_NOR[vertexID].xyz;
    uint3 u3Normal = vertexBuffer_NOR.Load3(vertexID * 3 * 4);
    float3 normal = asfloat(u3Normal);

    float2 uv = vertexBuffer_UVS[vertexID];
	float4 color = vertexBuffer_COL[vertexID];

    Particle particle = particleBuffer[particleIndex];

    // calculate render properties from life:
    float lifeLerp = 1 - particle.life / particle.maxLife;
    float size = lerp(particle.sizeBeginEnd.x, particle.sizeBeginEnd.y, lifeLerp);

    // Sprite sheet UV transform:
    const float spriteframe = g_emitter.xEmitterFrameRate == 0 ?
        lerp(g_emitter.xEmitterFrameStart, g_emitter.xEmitterFrameCount, lifeLerp) :
        ((g_emitter.xEmitterFrameStart + particle.life * g_emitter.xEmitterFrameRate) % g_emitter.xEmitterFrameCount);
    const float frameBlend = frac(spriteframe);


	// note that the particle has been calculated in world space!
    VertextoPixel Out;
	Out.P = position;// TransformPoint(position, g_cbPobj.mat_os2ws);
    // Out.pos = float4(position, 1);
    Out.clip = 1;// dot(Out.pos, GetCamera().clip_plane);
    Out.pos = mul(g_cbPobj.mat_os2ps, float4(position, 1.f)); //mul(GetCamera().view_projection, Out.pos);
    Out.tex = min16float2(uv);
    Out.size = min16float(size);
    Out.color = pack_rgba(color);
    Out.unrotated_uv = min16float2(BILLBOARD[vertexID % 4].xy * float2(1, -1) * 0.5f + 0.5f);
    Out.frameBlend = min16float(frameBlend);
    return Out;

    //VS_OUTPUT vout = (VS_OUTPUT)0;
    //vout.f4PosSS = mul(g_cbPobj.mat_os2ps, float4(position, 1.f));
    //vout.f3PosWS = TransformPoint(position, g_cbPobj.mat_os2ws);
    //vout.f3VecNormalWS = normalize(TransformVector(normal, g_cbPobj.mat_os2ws));
    //vout.f3Custom = float3(uv, 1.f);
    //return vout;
}

[earlydepthstencil]
PS_FILL_OUTPUT ParticleRender(VertextoPixel input)
{
	PS_FILL_OUTPUT out_ps;
	out_ps.ds_z = 1.f;
	out_ps.color = (float4)0;
	out_ps.depthcs = FLT_MAX;
    
    if (g_cbClipInfo.clip_flag & 0x1)
        clip(dot(g_cbClipInfo.vec_clipplane, input.P - g_cbClipInfo.pos_clipplane) > 0 ? -1 : 1);
    if (g_cbClipInfo.clip_flag & 0x2)
        clip(!IsInsideClipBox(input.P, g_cbClipInfo.mat_clipbox_ws2bs) ? -1 : 1);
	clip(input.pos.z);
	clip(input.pos.z/input.pos.w);

	uint2 pixel = input.pos.xy;
    float3 pos_ip_ss = float3(pixel, 0.0f);
    float3 pos_ip_ws = TransformPoint(pos_ip_ss, g_cbCamState.mat_ss2ws);
    float3 vec_pos_ip2frag = input.P - pos_ip_ws;
    float z_depth = length(vec_pos_ip2frag) /*- g_cbPobj.depth_forward_bias*/;\
    clip(dot(vec_pos_ip2frag, g_cbCamState.dir_view_ws) < 0 ? -1 : 1);\
    float3 view_dir = g_cbCamState.dir_view_ws;\
    if (g_cbCamState.cam_flag & 0x1)\
        view_dir = pos_ip_ws - g_cbCamState.pos_cam_ws;\
    view_dir = normalize(view_dir);

	// Blocker shadow map check:
	//[branch]
	//if ((xEmitterOptions & EMITTER_OPTION_BIT_USE_RAIN_BLOCKER) && rain_blocker_check(input.P))
	//{
	//	return 0;
	//}

	//ShaderMaterial material = EmitterGetMaterial();

	float4 color = 1;
	color.a = g_cbPobj.alpha;

	//[branch]
	//if (material.textures[SLOT].IsValid())
	{
		//color = material.textures[SLOT].Sample(sampler_linear_clamp, input.tex.xyxy);

		//[branch]
		// 이전 frame 에 대한 정보 zwzw 에서 sample 한 것과 blending...
		//if (g_emitter.xEmitterOptions & EMITTER_OPTION_BIT_FRAME_BLENDING_ENABLED)
		//{
		//	float4 color2 = material.textures[SLOT].Sample(sampler_linear_clamp, input.tex.zwzw);
		//	color = lerp(color, color2, input.frameBlend);
		//}
	}

	float4 inputColor;
	inputColor.r = ((input.color >> 0) & 0xFF) / 255.0f;
	inputColor.g = ((input.color >> 8) & 0xFF) / 255.0f;
	inputColor.b = ((input.color >> 16) & 0xFF) / 255.0f;
	inputColor.a = ((input.color >> 24) & 0xFF) / 255.0f;

	float opacity = color.a * inputColor.a;

	//[branch]
	//if (GetCamera().texture_lineardepth_index >= 0)
	//{
	//	float2 ScreenCoord = pixel * GetCamera().internal_resolution_rcp;
	//	float4 depthScene = texture_lineardepth.GatherRed(sampler_linear_clamp, ScreenCoord) * GetCamera().z_far;
	//	float depthFragment = input.pos.w;
	//	opacity *= saturate(1.0 / input.size * (max(max(depthScene.x, depthScene.y), max(depthScene.z, depthScene.w)) - depthFragment));
	//}

	opacity = saturate(opacity);

	//color.rgb *= inputColor.rgb * (1 + material.GetEmissive());
	const float emissive = 1.f;
	color.rgb *= inputColor.rgb * (1 + emissive);
	color.a = opacity;

#ifdef EMITTEDPARTICLE_DISTORTION
	// just make normal maps blendable:
	color.rgb = color.rgb - 0.5f;
#endif // EMITTEDPARTICLE_DISTORTION

#ifdef EMITTEDPARTICLE_LIGHTING

	[branch]
	if (color.a > 0)
	{
		float3 N;
		N.x = -cos(PI * input.unrotated_uv.x);
		N.y = cos(PI * input.unrotated_uv.y);
		N.z = -sin(PI * length(input.unrotated_uv));
		N = mul((float3x3)GetCamera().inverse_view, N);
		N = normalize(N);

		float3 V = GetCamera().position - input.P;
		float dist = length(V);
		V /= dist;

		Lighting lighting;
		lighting.create(0, 0, GetAmbient(N), 0);

		Surface surface;
		surface.init();
		surface.create(material, color, surfacemap_simple);
		surface.P = input.P;
		surface.N = N;
		surface.V = V;
		surface.pixel = pixel;
		surface.sss = material.subsurfaceScattering;
		surface.sss_inv = material.subsurfaceScattering_inv;
		surface.update();

		TiledLighting(surface, lighting, GetFlatTileIndex(pixel));

		color.rgb *= lighting.direct.diffuse + lighting.indirect.diffuse;

		//color.rgb = float3(unrotated_uv, 0);
		//color.rgb = float3(input.tex, 0);

		ApplyFog(dist, V, color);

		color = max(0, color);
	}

#endif // EMITTEDPARTICLE_LIGHTING

	//return color;

	out_ps.ds_z = input.pos.z;
	out_ps.color = color;
	out_ps.depthcs = z_depth;

	return out_ps;
}