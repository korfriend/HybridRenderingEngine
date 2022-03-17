#include "../Sr_Common.hlsl"
/*! \file
	This header provides the functionality to create the vectors of moments and 
	to blend surfaces together with an appropriately reconstructed 
	transmittance. It is needed for both additive passes of moment-based OIT.
*/

#define LOAD4_MMBUF(F_ADDR, K) moment_container_buf.Load4((F_ADDR + K) * 4)
#define LOAD2_MMBUF(F_ADDR, K) moment_container_buf.Load2((F_ADDR + K) * 4)
#define LOAD1_MMBUF(F_ADDR, K) moment_container_buf.Load((F_ADDR + K) * 4)
#if STRICT_LOCKED == 1
#define STORE4_MMBUF(V, F_ADDR, K) moment_container_buf.Store4((F_ADDR + K) * 4, V)
#define STORE2_MMBUF(V, F_ADDR, K) moment_container_buf.Store2((F_ADDR + K) * 4, V)
#define STORE1_MMBUF(V, F_ADDR, K) moment_container_buf.Store((F_ADDR + K) * 4, V)
#else
#define STORE4_MMBUF(V, F_ADDR, K) {moment_container_buf.InterlockedExchange((F_ADDR + K) * 4, V);\
	moment_container_buf.InterlockedExchange((F_ADDR + K) * 4 + 4, V);\
	moment_container_buf.InterlockedExchange((F_ADDR + K) * 4 + 8, V);\
	moment_container_buf.InterlockedExchange((F_ADDR + K) * 4 + 12, V)}

#define STORE2_MMBUF(V, F_ADDR, K) {moment_container_buf.Store2((F_ADDR + K) * 4, V);\
	moment_container_buf.Store2((F_ADDR + K) * 4 + 4, V);}

#define STORE1_MMBUF(V, F_ADDR, K) moment_container_buf.Store((F_ADDR + K) * 4, V)
#endif


#define LOAD22FF_MMB(V1, V2, F_ADDR, K) { uint4 mmi4 = LOAD4_MMBUF(F_ADDR, K); V1 = float2(asfloat(mmi4.x), asfloat(mmi4.y)); V2 = float2(asfloat(mmi4.z), asfloat(mmi4.w)); }
#define STORE22FF_MMB(V1, V2, F_ADDR, K) { float4 _ttmp4 = float4(V1, V2); uint4 mmi4 = uint4(asuint(_ttmp4.x), asuint(_ttmp4.y), asuint(_ttmp4.z), asuint(_ttmp4.w)); STORE4_MMBUF(mmi4, F_ADDR, K); }

#define LOAD4F_MMB(V, F_ADDR, K) { uint4 mmi4 = LOAD4_MMBUF(F_ADDR, K); V = float4(asfloat(mmi4.x), asfloat(mmi4.y), asfloat(mmi4.z), asfloat(mmi4.w)); }
#define STORE4F_MMB(V, F_ADDR, K) { float4 _ttmp4 = V; uint4 mmi4 = uint4(asuint(_ttmp4.x), asuint(_ttmp4.y), asuint(_ttmp4.z), asuint(_ttmp4.w)); STORE4_MMBUF(mmi4, F_ADDR, K); }
#define LOAD2F_MMB(V, F_ADDR, K) { uint2 mmi2 = LOAD2_MMBUF(F_ADDR, K); V = float2(asfloat(mmi2.x), asfloat(mmi2.y)); }
#define STORE2F_MMB(V, F_ADDR, K) { float2 _ttmp2 = V; uint2 mmi2 = uint2(asuint(_ttmp2.x), asuint(_ttmp2.y)); STORE2_MMBUF(mmi2, F_ADDR, K); }

#define LOAD1F_MMB(F_ADDR, K) asfloat(LOAD1_MMBUF(F_ADDR, K))
#define STORE1F_MMB(V, F_ADDR, K) moment_container_buf.Store((F_ADDR + K) * 4, asuint(V))
//#define STORE1F_MMB(V, F_ADDR, K) STORE1_MMBUF(asuint(V), F_ADDR, K)

#ifndef MOMENT_OIT_HLSLI
#define MOMENT_OIT_HLSLI

cbuffer cbGlobalParams : register(b8)
{
	struct {
		float4 wrapping_zone_parameters;
		float overestimation;
		float moment_bias;
		float2 warp_nf;
	} MomentOIT;
};

// see Section 3.3 Warping Depth in Mement-based OIT
float __warp_depth(float z_depth_cs)
{
//#define Z_MIN 0.01
//#define Z_MAX 10000.0
	const float Z_MIN = MomentOIT.warp_nf.x;
	const float Z_MAX = MomentOIT.warp_nf.y;
	return (log(max(min(z_depth_cs, Z_MAX), Z_MIN)) - log(Z_MIN)) / (log(Z_MAX) - log(Z_MIN)) * 2.f - 1.f;
}

#include "MomentMath.hlsli"

#if USE_ROV == 1
#define TEX2D_LOCK RasterizerOrderedTexture2D
#else
#define TEX2D_LOCK RWTexture2D
#endif

#if MOMENT_GENERATION
/*! Generation of moments in case that rasterizer ordered views are used. 
	This includes the case if moments are stored in 16 bits. */
TEX2D_LOCK<uint> fragment_counter : register(u2);
RWTexture2D<uint> fragment_spinlock : register(u3);
RWByteAddressBuffer moment_container_buf : register(u4);

#if !TRIGONOMETRIC

/*! This function handles the actual computation of the new vector of power 
	moments.*/
void generatePowerMoments(inout float b_0,
#if NUM_MOMENTS == 4
	inout float2 b_even, inout float2 b_odd,
#elif NUM_MOMENTS == 6
	inout float3 b_even, inout float3 b_odd,
#elif NUM_MOMENTS == 8
	inout float4 b_even, inout float4 b_odd,
#endif
	float depth, float transmittance)
{
	float absorbance = -log(transmittance);

	float depth_pow2 = depth * depth;
	float depth_pow4 = depth_pow2 * depth_pow2;

#if SINGLE_PRECISION
	b_0 += absorbance;
#if NUM_MOMENTS == 4
	b_even += float2(depth_pow2, depth_pow4) * absorbance;
	b_odd += float2(depth, depth_pow2 * depth) * absorbance;
#elif NUM_MOMENTS == 6
	b_even += float3(depth_pow2, depth_pow4, depth_pow4 * depth_pow2) * absorbance;
	b_odd += float3(depth, depth_pow2 * depth, depth_pow4 * depth) * absorbance;
#elif NUM_MOMENTS == 8
	float depth_pow6 = depth_pow4 * depth_pow2;
	b_even += float4(depth_pow2, depth_pow4, depth_pow6, depth_pow6 * depth_pow2) * absorbance;
	b_odd += float4(depth, depth_pow2 * depth, depth_pow4 * depth, depth_pow6 * depth) * absorbance;
#endif
#else // Quantized
	offsetMoments(b_even, b_odd, -1.0);
	b_even *= b_0;
	b_odd *= b_0;

	//  New Moments
#if NUM_MOMENTS == 4
	float2 b_even_new = float2(depth_pow2, depth_pow4);
	float2 b_odd_new = float2(depth, depth_pow2 * depth);
	float2 b_even_new_q, b_odd_new_q;
#elif NUM_MOMENTS == 6
	float3 b_even_new = float3(depth_pow2, depth_pow4, depth_pow4 * depth_pow2);
	float3 b_odd_new = float3(depth, depth_pow2 * depth, depth_pow4 * depth);
	float3 b_even_new_q, b_odd_new_q;
#elif NUM_MOMENTS == 8
	float depth_pow6 = depth_pow4 * depth_pow2;
	float4 b_even_new = float4(depth_pow2, depth_pow4, depth_pow6, depth_pow6 * depth_pow2);
	float4 b_odd_new = float4(depth, depth_pow2 * depth, depth_pow4 * depth, depth_pow6 * depth);
	float4 b_even_new_q, b_odd_new_q;
#endif
	quantizeMoments(b_even_new_q, b_odd_new_q, b_even_new, b_odd_new);

	// Combine Moments
	b_0 += absorbance;
	b_even += b_even_new_q * absorbance;
	b_odd += b_odd_new_q * absorbance;

	// Go back to interval [0, 1]
	b_even /= b_0;
	b_odd /= b_0;
	offsetMoments(b_even, b_odd, 1.0);
#endif
}

#else

/*! This function handles the actual computation of the new vector of 
	trigonometric moments.*/
void generateTrigonometricMoments(inout float b_0,
#if NUM_MOMENTS == 4
	inout float4 b_12,
#elif NUM_MOMENTS == 6
	inout float2 b_1, inout float2 b_2, inout float2 b_3,
#elif NUM_MOMENTS == 8
	inout float4 b_even, inout float4 b_odd,
#endif
	float depth, float transmittance, float4 wrapping_zone_parameters)
{
	float absorbance = -log(transmittance);

	float phase = mad(depth, wrapping_zone_parameters.y, wrapping_zone_parameters.y);
	float2 circle_point;
	sincos(phase, circle_point.y, circle_point.x);
	float2 circle_point_pow2 = Multiply(circle_point, circle_point);

#if NUM_MOMENTS == 4
	float4 b_12_new = float4(circle_point, circle_point_pow2) * absorbance;
#if SINGLE_PRECISION
	b_0 += absorbance;
	b_12 += b_12_new;
#else
	b_12 = mad(b_12, 2.0, -1.0) * b_0;

	b_0 += absorbance;
	b_12 += b_12_new;

	b_12 /= b_0;
	b_12 = mad(b_12, 0.5, 0.5);
#endif
#elif NUM_MOMENTS == 6
	float2 b_1_new = circle_point * absorbance;
	float2 b_2_new = circle_point_pow2 * absorbance;
	float2 b_3_new = Multiply(circle_point, circle_point_pow2) * absorbance;
#if SINGLE_PRECISION
	b_0 += absorbance;
	b_1 += b_1_new;
	b_2 += b_2_new;
	b_3 += b_3_new;
#else
	b_1 = mad(b_1, 2.0, -1.0) * b_0;
	b_2 = mad(b_2, 2.0, -1.0) * b_0;
	b_3 = mad(b_3, 2.0, -1.0) * b_0;

	b_0 += absorbance;
	b_1 += b_1_new;
	b_2 += b_2_new;
	b_3 += b_3_new;

	b_1 /= b_0;
	b_2 /= b_0;
	b_3 /= b_0;
	b_1 = mad(b_1, 0.5, 0.5);
	b_2 = mad(b_2, 0.5, 0.5);
	b_3 = mad(b_3, 0.5, 0.5);
#endif
#elif NUM_MOMENTS == 8
	float4 b_even_new = float4(circle_point_pow2, Multiply(circle_point_pow2, circle_point_pow2)) * absorbance;
	float4 b_odd_new = float4(circle_point, Multiply(circle_point, circle_point_pow2)) * absorbance;
#if SINGLE_PRECISION
	b_0 += absorbance;
	b_even += b_even_new;
	b_odd += b_odd_new;
#else
	b_even = mad(b_even, 2.0, -1.0) * b_0;
	b_odd = mad(b_odd, 2.0, -1.0) * b_0;

	b_0 += absorbance;
	b_even += b_even_new;
	b_odd += b_odd_new;

	b_even /= b_0;
	b_odd /= b_0;
	b_even = mad(b_even, 0.5, 0.5);
	b_odd = mad(b_odd, 0.5, 0.5);
#endif
#endif
}
#endif

/*! This function reads the stored moments from the rasterizer ordered view, 
	calls the appropriate moment-generating function and writes the new moments 
	back to the rasterizer ordered view.*/

void generateMoments(float depth, float transmittance, int2 sv_pos, float4 wrapping_zone_parameters)
{
	int addr_base = (sv_pos.y * g_cbCamState.rt_width + sv_pos.x) * (g_cbCamState.k_value * 4);

	//uint3 idx0 = uint3(sv_pos, 0);
	//uint3 idx1 = idx0;
	//idx1[2] = 1;
	uint dummy;
	
	// Return early if the surface is fully transparent
	clip(0.999f - transmittance);

#if NUM_MOMENTS == 4
	//float b_0 = b0[idx0];
	//float b_0 = asfloat(moment_container_buf[addr_base + 0]);
	float b_0 = LOAD1F_MMB(addr_base, 0);
	
	//float4 b_raw = b[idx0];
	float4 b_raw;
	//b_raw.x = asfloat(moment_container_buf[addr_base + 1]);
	//b_raw.y = asfloat(moment_container_buf[addr_base + 2]);
	//b_raw.z = asfloat(moment_container_buf[addr_base + 3]);
	//b_raw.w = asfloat(moment_container_buf[addr_base + 4]);
	LOAD4F_MMB(b_raw, addr_base, 1);

#if TRIGONOMETRIC
	generateTrigonometricMoments(b_0, b_raw, depth, transmittance, wrapping_zone_parameters);
	//b[idx0] = b_raw;
//	moment_container_buf[addr_base + 1] = asuint(b_raw.x);
//	moment_container_buf[addr_base + 2] = asuint(b_raw.y);
//	moment_container_buf[addr_base + 3] = asuint(b_raw.z);
//	moment_container_buf[addr_base + 4] = asuint(b_raw.w);
	STORE4F_MMB(b_raw, addr_base, 1);
#else
	float2 b_even = b_raw.yw;
	float2 b_odd = b_raw.xz;

	generatePowerMoments(b_0, b_even, b_odd, depth, transmittance);

	//b[idx0] = float4(b_odd.x, b_even.x, b_odd.y, b_even.y);
	b_raw = float4(b_odd.x, b_even.x, b_odd.y, b_even.y);
	//b0[idx0] = b_0;
	//moment_container_buf[addr_base + 0] = asuint(b_0);
	STORE1F_MMB(b_0, addr_base, 0);
	//moment_container_buf[addr_base + 1] = asuint(b_raw.x);
	//moment_container_buf[addr_base + 2] = asuint(b_raw.y);
	//moment_container_buf[addr_base + 3] = asuint(b_raw.z);
	//moment_container_buf[addr_base + 4] = asuint(b_raw.w);
	STORE4F_MMB(b_raw, addr_base, 1);
#endif
	//// END
#elif NUM_MOMENTS == 6
	//uint3 idx2 = idx0;
	//idx2[2] = 2;

	//float b_0 = b0[idx0];
	float b_0 = LOAD1F_MMB(addr_base, 0); // asfloat(moment_container_buf[addr_base + 0]);

	float2 b_raw[3];
	//b_raw[0] = b[idx0].xy;
	//b_raw[0].x = asfloat(moment_container_buf[addr_base + 1]);
	//b_raw[0].y = asfloat(moment_container_buf[addr_base + 2]);
	LOAD2F_MMB(b_raw[0], addr_base, 1);
	float4 tmp;
	//tmp.x = asfloat(moment_container_buf[addr_base + 3]);
	//tmp.y = asfloat(moment_container_buf[addr_base + 4]);
	//tmp.z = asfloat(moment_container_buf[addr_base + 5]);
	//tmp.w = asfloat(moment_container_buf[addr_base + 6]);
	LOAD44F_MMB(tmp, addr_base, 3);
	b_raw[1] = tmp.xy;
	b_raw[2] = tmp.zw;

#if TRIGONOMETRIC
	generateTrigonometricMoments(b_0, b_raw[0], b_raw[1], b_raw[2], depth, transmittance, wrapping_zone_parameters);
#else
	float3 b_even = float3(b_raw[0].y, b_raw[1].y, b_raw[2].y);
	float3 b_odd = float3(b_raw[0].x, b_raw[1].x, b_raw[2].x);

	generatePowerMoments(b_0, b_even, b_odd, depth, transmittance);

	b_raw[0] = float2(b_odd.x, b_even.x);
	b_raw[1] = float2(b_odd.y, b_even.y);
	b_raw[2] = float2(b_odd.z, b_even.z);
#endif

	//b0[idx0] = b_0;
	//__InterlockedExchange(moment_container_buf[addr_base + 0], asuint(b_0), dummy);
	STORE1F_MMB(b_0, addr_base, 0);
	//b[idx0] = b_raw[0];
	//__InterlockedExchange(moment_container_buf[addr_base + 1], asuint(b_raw[0].x), dummy);
	//__InterlockedExchange(moment_container_buf[addr_base + 2], asuint(b_raw[0].y), dummy);
	STORE1F_MMB(b_raw[0].x, addr_base, 1);
	STORE1F_MMB(b_raw[0].y, addr_base, 2);
//#if USE_R_RG_RBBA_FOR_MBOIT6
//	b_extra[idx0] = float4(b_raw[1], b_raw[2]);
//#else
//	b[idx1] = b_raw[1];
//	b[idx2] = b_raw[2];
//#endif
	//__InterlockedExchange(moment_container_buf[addr_base + 3], asuint(b_raw[1].x), dummy);
	//__InterlockedExchange(moment_container_buf[addr_base + 4], asuint(b_raw[1].y), dummy);
	//__InterlockedExchange(moment_container_buf[addr_base + 5], asuint(b_raw[2].x), dummy);
	//__InterlockedExchange(moment_container_buf[addr_base + 6], asuint(b_raw[2].y), dummy);
	STORE4F_MMB(float4(b_raw[1].xy, b_raw[2].xy), addr_base, 3);
	//END
#elif NUM_MOMENTS == 8
	//float b_0 = b0[idx0];
	float b_0 = LOAD1F_MMB(addr_base, 0); // asfloat(moment_container_buf[addr_base + 0]);
	//float4 b_even = b[idx0];
	float4 b_even;
	//b_even.x = asfloat(moment_container_buf[addr_base + 1]);
	//b_even.y = asfloat(moment_container_buf[addr_base + 2]);
	//b_even.z = asfloat(moment_container_buf[addr_base + 3]);
	//b_even.w = asfloat(moment_container_buf[addr_base + 4]);
	LOAD4F_MMB(b_even, addr_base, 1);
	//float4 b_odd = b[idx1];
	float4 b_odd;
	//b_odd.x = asfloat(moment_container_buf[addr_base + 5]);
	//b_odd.y = asfloat(moment_container_buf[addr_base + 6]);
	//b_odd.z = asfloat(moment_container_buf[addr_base + 7]);
	//b_odd.w = asfloat(moment_container_buf[addr_base + 8]);
	LOAD4F_MMB(b_odd, addr_base, 5);

#if TRIGONOMETRIC
	generateTrigonometricMoments(b_0, b_even, b_odd, depth, transmittance, wrapping_zone_parameters);
#else
	generatePowerMoments(b_0, b_even, b_odd, depth, transmittance);
#endif

	//b0[idx0] = b_0;
	//__InterlockedExchange(moment_container_buf[addr_base + 0], asuint(b_0), dummy);
	STORE1F_MMB(b_0, addr_base, 0);
	//b[idx0] = b_even;
	//__InterlockedExchange(moment_container_buf[addr_base + 1], asuint(b_even.x), dummy);
	//__InterlockedExchange(moment_container_buf[addr_base + 2], asuint(b_even.y), dummy);
	//__InterlockedExchange(moment_container_buf[addr_base + 3], asuint(b_even.z), dummy);
	//__InterlockedExchange(moment_container_buf[addr_base + 4], asuint(b_even.w), dummy);
	STORE4F_MMB(b_even, addr_base, 1);
	//b[idx1] = b_odd;
	//__InterlockedExchange(moment_container_buf[addr_base + 5], asuint(b_odd.x), dummy);
	//__InterlockedExchange(moment_container_buf[addr_base + 6], asuint(b_odd.y), dummy);
	//__InterlockedExchange(moment_container_buf[addr_base + 7], asuint(b_odd.z), dummy);
	//__InterlockedExchange(moment_container_buf[addr_base + 8], asuint(b_odd.w), dummy);
	STORE4F_MMB(b_odd, addr_base, 5);
	//END
#endif

}

void Moment_GeneratePass(__VS_OUT input)
{
	if (g_cbClipInfo.clip_flag & 0x1)
		clip(dot(g_cbClipInfo.vec_clipplane, input.f3PosWS - g_cbClipInfo.pos_clipplane) > 0 ? -1 : 1);
	if (g_cbClipInfo.clip_flag & 0x2)
		clip(!IsInsideClipBox(input.f3PosWS, g_cbClipInfo.pos_clipbox_max_bs, g_cbClipInfo.mat_clipbox_ws2bs) ? -1 : 1);

	float3 pos_ip_ss = float3(input.f4PosSS.xy, 0.0f);
	float3 pos_ip_ws = TransformPoint(pos_ip_ss, g_cbCamState.mat_ss2ws);
	float3 vec_pos_ip2frag = input.f3PosWS - pos_ip_ws;
	clip(dot(vec_pos_ip2frag, g_cbCamState.dir_view_ws) < 0 ? -1 : 1);

	float z_depth = length(vec_pos_ip2frag);
	clip((z_depth > FLT_LARGE || z_depth <= 0) ? -1 : 1);

#if __RENDERING_MODE == 2
	float4 v_rgba = float4(g_cbPobj.Kd, g_cbPobj.alpha);
	MultiTextMapping(v_rgba, z_depth, input.f3Custom0.xy, (int)(input.f3Custom0.z + 0.5f), input.f3Custom1, input.f3Custom2);
	float alpha = min(v_rgba.a, 0.999f);
#elif __RENDERING_MODE == 3
	float4 v_rgba = float4(g_cbPobj.Kd, g_cbPobj.alpha);
	TextMapping(v_rgba, z_depth, input.f3Custom.xy, g_cbPobj.pobj_flag & (0x1 << 9), g_cbPobj.pobj_flag & (0x1 << 10));
	float alpha = min(v_rgba.a, 0.999f);
#else
	float alpha = min(g_cbPobj.alpha, 0.999f); // to avoid opaque out
#endif
	clip(alpha == 0 ? -1 : 1);

	int2 tex2d_xy = int2(input.f4PosSS.xy);

	// atomic routine (we are using spin-lock scheme)
#if USE_ROV == 0
	uint __dummy;
	uint safe_unlock_count = 0;
	bool keep_loop = true;
	[loop]
	while (keep_loop)
	{
		if (++safe_unlock_count > g_cbEnv.num_safe_loopexit)
		{
			InterlockedExchange(fragment_spinlock[tex2d_xy.xy], 0, __dummy);
			keep_loop = false;
		}
		else
		{
			uint spin_lock = 0;
			// note that all of fragment_spinlock are initialized as zero
			// use InterlockedCompareExchange(..) instead of InterlockedExchange(fragment_spinlock[tex2d_xy.xy], 1, spin_lock)
			InterlockedCompareExchange(fragment_spinlock[tex2d_xy.xy], 0, 1, spin_lock);

			if (spin_lock == 0)
			{
#endif
				uint frag_cnt = fragment_counter[tex2d_xy.xy];
				if (frag_cnt == 0) // clear mask
				{
					int addr_base = (tex2d_xy.y * g_cbCamState.rt_width + tex2d_xy.x) * (g_cbCamState.k_value * 4);
					for (int j = 0; j < NUM_MOMENTS + 2; j++)
					{
						//moment_container_buf[addr_base + j] = 0;
						STORE1F_MMB(0, addr_base, j);
					}
				}
				// see Section 3.3 Warping Depth in Mement-based OIT
				float warp_z = __warp_depth(z_depth);

				generateMoments(warp_z, 1-alpha, tex2d_xy, MomentOIT.wrapping_zone_parameters);

				if(frag_cnt == 0)
					fragment_counter[tex2d_xy.xy] = frag_cnt + 1;
#if USE_ROV == 0
				//InterlockedAdd(fragment_counter[tex2d_xy.xy], 1, frag_cnt);
				// always fragment_spinlock[tex2d_xy.xy] is 1, thus use InterlockedExchange instead of InterlockedCompareExchange
				InterlockedExchange(fragment_spinlock[tex2d_xy.xy], 0, spin_lock);
				keep_loop = false;
				//break;
			} // critical section
		}
	} // while for spin-lock
#endif
}


#else //MOMENT_GENERATION is disabled

ByteAddressBuffer moment_container_buf : register(t20);

// unless PIXEL_SYNCH, use built-in blending pipeline (not stable implementation at this moment)
#if PIXEL_SYNCH
RWTexture2D<unorm float4> fragment_blendout : register(u2);
RWTexture2D<float> fragment_zdepth : register(u3);
#if USE_ROV == 0
RWTexture2D<uint> fragment_spinlock : register(u4); // ROV or not
#endif
TEX2D_LOCK<float4> vis_recon : register(u5);
#define PS_OUT void
#else
#define PS_OUT PS_FILL_OUTPUT
#endif

/*! This function is to be called from the shader that composites the 
	transparent fragments. It reads the moments and calls the appropriate 
	function to reconstruct the transmittance at the specified depth.*/

void resolveMoments(out float transmittance_at_depth, out float total_transmittance, float depth, int2 sv_pos)
{
	const float moment_bias = MomentOIT.moment_bias;
	const float overestimation = MomentOIT.overestimation;

	//int4 idx0 = int4(sv_pos, 0, 0);
	//int4 idx1 = idx0;
	//idx1[2] = 1;
	int addr_base = (sv_pos.y * g_cbCamState.rt_width + sv_pos.x) * (g_cbCamState.k_value * 4);

	transmittance_at_depth = 1;
	total_transmittance = 1;

	//float b_0 = zeroth_moment.Load(idx0).x;
	//float b_0 = asfloat(moment_container_buf[addr_base + 0]);
	float b_0 = LOAD1F_MMB(addr_base, 0); // 
	//clip(b_0 - 0.00100050033f);
	if (b_0 - 0.00100050033f < 0)
		clip(-1);

	total_transmittance = exp(-b_0);

#if NUM_MOMENTS == 4
#if TRIGONOMETRIC
	//float4 b_tmp = moments.Load(idx0);
	float4 b_tmp;
	//b_tmp.x = asfloat(moment_container_buf[addr_base + 1]);
	//b_tmp.y = asfloat(moment_container_buf[addr_base + 2]);
	//b_tmp.z = asfloat(moment_container_buf[addr_base + 3]);
	//b_tmp.w = asfloat(moment_container_buf[addr_base + 4]);
	LOAD4F_MMB(b_tmp, addr_base, 1);

	float2 trig_b[2];
	trig_b[0] = b_tmp.xy;
	trig_b[1] = b_tmp.zw;
#if SINGLE_PRECISION
	trig_b[0] /= b_0;
	trig_b[1] /= b_0;
#else
	trig_b[0] = mad(trig_b[0], 2.0, -1.0);
	trig_b[1] = mad(trig_b[1], 2.0, -1.0);
#endif
	transmittance_at_depth = computeTransmittanceAtDepthFrom2TrigonometricMoments(b_0, trig_b, depth, moment_bias, overestimation, MomentOIT.wrapping_zone_parameters);
#else
	//float4 b_1234 = moments.Load(idx0).xyzw;
	float4 b_1234;
	//b_1234.x = asfloat(moment_container_buf[addr_base + 1]);
	//b_1234.y = asfloat(moment_container_buf[addr_base + 2]);
	//b_1234.z = asfloat(moment_container_buf[addr_base + 3]);
	//b_1234.w = asfloat(moment_container_buf[addr_base + 4]);
	LOAD4F_MMB(b_1234, addr_base, 1);
#if SINGLE_PRECISION
	float2 b_even = b_1234.yw;
	float2 b_odd = b_1234.xz;

	b_even /= b_0;
	b_odd /= b_0;

	const float4 bias_vector = float4(0, 0.375, 0, 0.375);
#else
	float2 b_even_q = b_1234.yw;
	float2 b_odd_q = b_1234.xz;

	// Dequantize the moments
	float2 b_even;
	float2 b_odd;
	offsetAndDequantizeMoments(b_even, b_odd, b_even_q, b_odd_q);
	const float4 bias_vector = float4(0, 0.628, 0, 0.628);
#endif
	transmittance_at_depth = computeTransmittanceAtDepthFrom4PowerMoments(b_0, b_even, b_odd, depth, moment_bias, overestimation, bias_vector);
#endif
#elif NUM_MOMENTS == 6
	//int4 idx2 = idx0;
	//idx2[2] = 2;
#if TRIGONOMETRIC
	float2 trig_b[3];
	//trig_b[0] = moments.Load(idx0).xy;
	//trig_b[0].x = asfloat(moment_container_buf[addr_base + 1]);
	//trig_b[0].y = asfloat(moment_container_buf[addr_base + 2]);
	LOAD2F_MMB(trig_b[0], addr_base, 1);
#if USE_R_RG_RBBA_FOR_MBOIT6
	//float4 tmp = extra_moments.Load(idx0);
	float4 tmp;
	//tmp.x = asfloat(moment_container_buf[addr_base + 3]);
	//tmp.y = asfloat(moment_container_buf[addr_base + 4]);
	//tmp.z = asfloat(moment_container_buf[addr_base + 5]);
	//tmp.w = asfloat(moment_container_buf[addr_base + 6]);
	LOAD4F_MMB(tmp, addr_base, 3);
	trig_b[1] = tmp.xy;
	trig_b[2] = tmp.zw;
#else
	//trig_b[1] = moments.Load(idx1).xy;
	//trig_b[2] = moments.Load(idx2).xy;
	//trig_b[1].x = asfloat(moment_container_buf[addr_base + 3]);
	//trig_b[1].y = asfloat(moment_container_buf[addr_base + 4]);
	//trig_b[2].x = asfloat(moment_container_buf[addr_base + 5]);
	//trig_b[2].y = asfloat(moment_container_buf[addr_base + 6]);
	LOAD22FF_MMB(trig_b[1], trig_b[2], addr_base, 3);
#endif
#if SINGLE_PRECISION
	trig_b[0] /= b_0;
	trig_b[1] /= b_0;
	trig_b[2] /= b_0;
#else
	trig_b[0] = mad(trig_b[0], 2.0, -1.0);
	trig_b[1] = mad(trig_b[1], 2.0, -1.0);
	trig_b[2] = mad(trig_b[2], 2.0, -1.0);
#endif
	transmittance_at_depth = computeTransmittanceAtDepthFrom3TrigonometricMoments(b_0, trig_b, depth, moment_bias, overestimation, MomentOIT.wrapping_zone_parameters);
#else
	float2 b_12;// = moments.Load(idx0).xy;
	//b_12.x = asfloat(moment_container_buf[addr_base + 1]);
	//b_12.y = asfloat(moment_container_buf[addr_base + 2]);
	LOAD2F_MMB(b_12, addr_base, 1);
#if USE_R_RG_RBBA_FOR_MBOIT6
	float4 tmp;// = extra_moments.Load(idx0);
	//tmp.x = asfloat(moment_container_buf[addr_base + 3]);
	//tmp.y = asfloat(moment_container_buf[addr_base + 4]);
	//tmp.z = asfloat(moment_container_buf[addr_base + 5]);
	//tmp.w = asfloat(moment_container_buf[addr_base + 6]);
	LOAD4F_MMB(tmp, addr_base, 3);
	float2 b_34 = tmp.xy;
	float2 b_56 = tmp.zw;
#else
	float2 b_34;// = moments.Load(idx1).xy;
	float2 b_56;// = moments.Load(idx2).xy;
	//b_34.x = asfloat(moment_container_buf[addr_base + 3]);
	//b_34.y = asfloat(moment_container_buf[addr_base + 4]);
	//b_56.x = asfloat(moment_container_buf[addr_base + 5]);
	//b_56.y = asfloat(moment_container_buf[addr_base + 6]);
	LOAD22FF_MMB(b_34, b_56, addr_base, 3);
#endif
#if SINGLE_PRECISION
	float3 b_even = float3(b_12.y, b_34.y, b_56.y);
	float3 b_odd = float3(b_12.x, b_34.x, b_56.x);

	b_even /= b_0;
	b_odd /= b_0;

	const float bias_vector[6] = { 0, 0.48, 0, 0.451, 0, 0.45 };
#else
	float3 b_even_q = float3(b_12.y, b_34.y, b_56.y);
	float3 b_odd_q = float3(b_12.x, b_34.x, b_56.x);
	// Dequantize b_0 and the other moments
	float3 b_even;
	float3 b_odd;
	offsetAndDequantizeMoments(b_even, b_odd, b_even_q, b_odd_q);

	const float bias_vector[6] = { 0, 0.5566, 0, 0.489, 0, 0.47869382 };
#endif
	transmittance_at_depth = computeTransmittanceAtDepthFrom6PowerMoments(b_0, b_even, b_odd, depth, moment_bias, overestimation, bias_vector);
#endif
#elif NUM_MOMENTS == 8
#if TRIGONOMETRIC
	float4 b_tmp;// = moments.Load(idx0);
	//b_tmp.x = asfloat(moment_container_buf[addr_base + 1]);
	//b_tmp.y = asfloat(moment_container_buf[addr_base + 2]);
	//b_tmp.z = asfloat(moment_container_buf[addr_base + 3]);
	//b_tmp.w = asfloat(moment_container_buf[addr_base + 4]);
	LOAD4F_MMB(b_tmp, addr_base, 1);
	float4 b_tmp2;// = moments.Load(idx1);
	//b_tmp2.x = asfloat(moment_container_buf[addr_base + 5]);
	//b_tmp2.y = asfloat(moment_container_buf[addr_base + 6]);
	//b_tmp2.z = asfloat(moment_container_buf[addr_base + 7]);
	//b_tmp2.w = asfloat(moment_container_buf[addr_base + 8]);
	LOAD4F_MMB(b_tmp2, addr_base, 5);
#if SINGLE_PRECISION
	float2 trig_b[4] = {
		b_tmp2.xy / b_0,
		b_tmp.xy / b_0,
		b_tmp2.zw / b_0,
		b_tmp.zw / b_0
	};
#else
	float2 trig_b[4] = {
		mad(b_tmp2.xy, 2.0, -1.0),
		mad(b_tmp.xy, 2.0, -1.0),
		mad(b_tmp2.zw, 2.0, -1.0),
		mad(b_tmp.zw, 2.0, -1.0)
	};
#endif
	transmittance_at_depth = computeTransmittanceAtDepthFrom4TrigonometricMoments(b_0, trig_b, depth, moment_bias, overestimation, MomentOIT.wrapping_zone_parameters);
#else
#if SINGLE_PRECISION
	float4 b_even;// = moments.Load(idx0);
#if __TEST__ == 1
	b_even.x = asfloat(moment_container_buf[addr_base + 1]);
	b_even.y = asfloat(moment_container_buf[addr_base + 2]);
	b_even.z = asfloat(moment_container_buf[addr_base + 3]);
	b_even.w = asfloat(moment_container_buf[addr_base + 4]);
#else
	LOAD4F_MMB(b_even, addr_base, 1);
#endif
	float4 b_odd;// = moments.Load(idx1);
#if __TEST__ == 1
	b_odd.x = asfloat(moment_container_buf[addr_base + 5]);
	b_odd.y = asfloat(moment_container_buf[addr_base + 6]);
	b_odd.z = asfloat(moment_container_buf[addr_base + 7]);
	b_odd.w = asfloat(moment_container_buf[addr_base + 8]);
#else
	LOAD4F_MMB(b_odd, addr_base, 5);
#endif

	b_even /= b_0;
	b_odd /= b_0;
	const float bias_vector[8] = { 0, 0.75, 0, 0.67666666666666664, 0, 0.63, 0, 0.60030303030303034 };
#else
	float4 b_even_q;// = moments.Load(idx0);
	//b_even_q.x = asfloat(moment_container_buf[addr_base + 1]);
	//b_even_q.y = asfloat(moment_container_buf[addr_base + 2]);
	//b_even_q.z = asfloat(moment_container_buf[addr_base + 3]);
	//b_even_q.w = asfloat(moment_container_buf[addr_base + 4]);
	LOAD4F_MMB(b_even_q, addr_base, 1);
	float4 b_odd_q;// = moments.Load(idx1);
	//b_odd_q.x = asfloat(moment_container_buf[addr_base + 5]);
	//b_odd_q.y = asfloat(moment_container_buf[addr_base + 6]);
	//b_odd_q.z = asfloat(moment_container_buf[addr_base + 7]);
	//b_odd_q.w = asfloat(moment_container_buf[addr_base + 8]);
	LOAD4F_MMB(b_odd_q, addr_base, 5);

	// Dequantize the moments
	float4 b_even;
	float4 b_odd;
	offsetAndDequantizeMoments(b_even, b_odd, b_even_q, b_odd_q);
	const float bias_vector[8] = { 0, 0.42474916387959866, 0, 0.22407802675585284, 0, 0.15369230769230768, 0, 0.12900440529089119 };
#endif
	transmittance_at_depth = computeTransmittanceAtDepthFrom8PowerMoments(b_0, b_even, b_odd, depth, moment_bias, overestimation, bias_vector);
#endif
#endif

}

[earlydepthstencil]
PS_OUT Moment_ResolvePass(__VS_OUT input)
{
#if PIXEL_SYNCH
	POBJ_PRE_CONTEXT;
#else
	PS_FILL_OUTPUT out_ps;
	out_ps.ds_z = 1.f;
	out_ps.color = (float4)0;
	out_ps.depthcs = FLT_MAX;
	POBJ_PRE_CONTEXT;
#endif

	float4 v_rgba = float4(g_cbPobj.Kd, g_cbPobj.alpha);
	float3 nor = (float3) 0;
	float nor_len = 0;

#if __RENDERING_MODE != 1 && __RENDERING_MODE != 2 && __RENDERING_MODE != 3
	nor = input.f3VecNormalWS;
	nor_len = length(nor);
#endif

#if __RENDERING_MODE == 1
	DashedLine(v_rgba, z_depth, input.f3Custom.x, g_cbPobj.dash_interval, g_cbPobj.pobj_flag & (0x1 << 19), g_cbPobj.pobj_flag & (0x1 << 20));
#elif __RENDERING_MODE == 2
	MultiTextMapping(v_rgba, z_depth, input.f3Custom0.xy, (int)(input.f3Custom0.z + 0.5f), input.f3Custom1, input.f3Custom2);
#elif __RENDERING_MODE == 3
	TextMapping(v_rgba, z_depth, input.f3Custom.xy, g_cbPobj.pobj_flag & (0x1 << 9), g_cbPobj.pobj_flag & (0x1 << 10));
#elif __RENDERING_MODE == 4
	if (g_cbPobj.tex_map_enum == 1)
	{
		float4 clr_map;
		TextureImgMap(clr_map, input.f3Custom);
		if (clr_map.a == 0)
		{
			v_rgba = (float4) 0;
			z_depth = FLT_MAX;
		}
		else
		{
			//float3 mat_shading = float3(g_cbEnv.ltint_ambient.w, g_cbEnv.ltint_diffuse.w, g_cbEnv.ltint_spec.w);
			float3 Ka = clr_map.rgb * g_cbEnv.ltint_ambient.rgb;
			float3 Kd = clr_map.rgb * g_cbEnv.ltint_diffuse.rgb;
			float3 Ks = clr_map.rgb * g_cbEnv.ltint_spec.rgb;
			float Ns = g_cbPobj.Ns;
			ComputeColor(v_rgba.rgb, Ka, Kd, Ks, Ns, 1.0, input.f3PosWS, view_dir, nor, nor_len);
			v_rgba.a *= clr_map.a;
		}
		//v_rgba.rgb = float3(1, 0, 0);
	}
	else
	{
		float3 Ka = g_cbPobj.Ka, Kd = g_cbPobj.Kd, Ks = g_cbPobj.Ks;
		float Ns = g_cbPobj.Ns, d = 1.0, bump = 1.0;
		TextureMaterialMap(Ka, Kd, Ks, Ns, bump, d, input.f3Custom, g_cbPobj.tex_map_enum);
		if (Ns >= 0)
		{
			Ka *= g_cbEnv.ltint_ambient.rgb;
			Kd *= g_cbEnv.ltint_diffuse.rgb;
			Ks *= g_cbEnv.ltint_spec.rgb;
			ComputeColor(v_rgba.rgb, Ka, Kd, Ks, Ns, bump, input.f3PosWS, view_dir, nor, nor_len);
			//v_rgba.rgb = Ks;
		}
		else // illumination model 0
			v_rgba.rgb = Kd;
		v_rgba.a *= d;
	}
#else
	float3 Ka, Kd, Ks;
	float Ns = g_cbPobj.Ns;
	if ((g_cbPobj.pobj_flag & (0x1 << 3)) == 0)
	{
		Ka = input.f3Custom;
		Kd = input.f3Custom;
		Ks = input.f3Custom;
	}
	else
	{
		Ka = g_cbPobj.Ka, Kd = g_cbPobj.Kd, Ks = g_cbPobj.Ks;
	}
	if (nor_len > 0)
	{
		Ka *= g_cbEnv.ltint_ambient.rgb;
		Kd *= g_cbEnv.ltint_diffuse.rgb;
		Ks *= g_cbEnv.ltint_spec.rgb;
		ComputeColor(v_rgba.rgb, Ka, Kd, Ks, Ns, 1.0, input.f3PosWS, view_dir, nor, nor_len);
	}
	else
		v_rgba.rgb = Kd;
#endif

	int2 tex2d_xy = int2(input.f4PosSS.xy);
	float transmittance_at_depth, total_transmittance;
	// see Section 3.3 Warping Depth in Mement-based OIT
	float warp_z = __warp_depth(z_depth);

	resolveMoments(transmittance_at_depth, total_transmittance, warp_z, tex2d_xy); //z_depth

	if (transmittance_at_depth == 1)
		clip(-1);

	// This pass do not care opaque surface.
	// According to the authors of the paper "Moment-based OIT", 
	// they recommend an additional pass for handling the opaque object,
	// which means that this pass handles only transparecy surface, and 
	// blending them in the additive pass.
	// For our comparison experiments, we treat only objects that have transparency.
	// The entire pipeline that can handle opaque surfaces will be prepared after submitting our paper.
	if (transmittance_at_depth == 0)
		clip(-1);

	// make it as an alpha-multiplied color.
	// as a color component is stored into 8 bit channel, the alpha-multiplied precision must be determined in this stage.
	// unless, noise dots appear.
	v_rgba.rgb *= v_rgba.a;

#if PIXEL_SYNCH
#if USE_ROV == 0
	uint __dummy;
	uint safe_unlock_count = 0;
	bool keep_loop = true;
	[loop]
	while (keep_loop)
	{
		if (++safe_unlock_count > g_cbEnv.num_safe_loopexit)
		{
			InterlockedExchange(fragment_spinlock[tex2d_xy.xy], 0, __dummy);
			keep_loop = false;
		}
		else
		{
			uint spin_lock = 0;
			// note that all of fragment_spinlock are initialized as zero
			InterlockedCompareExchange(fragment_spinlock[tex2d_xy], 0, 1, spin_lock);
			if (spin_lock == 0)
			{
#endif
				// blending //
				float4 prev_vis = vis_recon[tex2d_xy];
				//if (lock_v > 0) // always true, but for avoid eliminating this routine during the compiling
				{
					float4 res_rgba;
					res_rgba.a = prev_vis.a + v_rgba.a * transmittance_at_depth;
					// note that the color is alpha-premultiplied color
					res_rgba.rgb = prev_vis.rgb + v_rgba.rgb * transmittance_at_depth;// / (1 - v_rgba.a);// exp(-v_rgba.a);
					vis_recon[tex2d_xy] = res_rgba;

					float renorm_term = (1 - total_transmittance) / res_rgba.a;
					float4 final_visout;
					final_visout.a = 1 - total_transmittance;
					final_visout.rgb = renorm_term * (prev_vis.rgb + v_rgba.rgb * transmittance_at_depth);
					fragment_blendout[tex2d_xy] = final_visout;

					// for z... use InterlockedMax ...
					// to do 
				}
#if USE_ROV == 0
				// always fragment_spinlock[tex2d_xy] is 1, thus use InterlockedExchange instead of InterlockedCompareExchange
				InterlockedExchange(fragment_spinlock[tex2d_xy], 0, spin_lock);
				keep_loop = false;
				//break;
			} // critical section
		}
	} // while for spin-lock
#endif
#else
	// make it as an associated color.
	// as a color component is stored into 8 bit channel, the alpha-multiplied precision must be determined in this stage.
	// unless, noise dots appear.
	v_rgba.rgb *= v_rgba.a * (transmittance_at_depth);
	v_rgba.a *= 0;// (transmittance_at_depth);
	//v_rgba.a = 1 - transmittance_at_depth;// *(1 - v_rgba.a); // 0 test

	//v_rgba = float4((float3)total_transmittance, 1);
	//v_rgba = float4(0, 1, 0, 1);

	out_ps.ds_z = input.f4PosSS.z;
	out_ps.color = v_rgba;
	out_ps.depthcs = z_depth;
	return out_ps;
#endif
}

#endif

#endif