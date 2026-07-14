#include "Sr_Common.hlsl"

// Tonemap resolve: the one place the renderer leaves linear HDR and commits to display range.
//
// ORDER (vismtv_inbuilt_renderergpudx.cpp): renderers -> Blend2ndLayer -> TaaResolve -> [here] -> D2D -> present
//
// AFTER the TAA resolve, and that is load-bearing rather than incidental:
//   * TAA is a static jittered accumulator taking an unbiased running mean of N samples -- it is averaging
//     RADIANCE. Since mean(tonemap(L)) != tonemap(mean(L)), curving the per-frame samples first would bake the
//     operator into every sample and bias the converged image.
//   * Keeping the curve out of the history also means exposure/operator changes do not invalidate the
//     accumulation: the HDR mean is still correct, only its presentation changed. Curving before TAA would
//     throw away a converged frame on every slider tick.
//
// BEFORE the D2D overlay, which keeps UI text and annotation lines out of the curve -- and is forced anyway,
// since D2D cannot draw into an FP16 surface with an R8G8B8A8_UNORM pixel format.
//
// Defaults are an exact no-op: TM_CLIP + exposure 1 + TME_NONE == saturate(hdr), i.e. today's image.
//
// Scope note: widening the render targets does NOT make everything HDR. Volume samples that meet a mesh
// fragment go through the K-buffer intermix, which packs them via ConvertFloat4ToUInt -- min(x*255,255), so
// they are clamped to 1.0 and quantised to 8 bits regardless of RT format (macros.hlsl INTERMIX). Surface
// shading is likewise clamped inside the BRDF (Sr_Common.hlsl, saturate(Ka + diff*Kd + specular*Ks)). So today
// the operator only has real headroom to recover on DVR pixels with no mesh intermix; elsewhere it is
// remapping LDR. Lifting those two clamps is a separate, larger change.

Texture2D<float4> hdr_in : register(t0);          // RENDER_OUT_RGBA_0  (FP16, linear, premultiplied)
RWTexture2D<unorm float4> ldr_out : register(u0); // RENDER_OUT_LDR_RGBA_0 (RGBA8, display range)

// Shoulder that is EXACTLY the identity below the knee and compresses [knee, inf) into [knee, 1].
//
// The knee is the dial for how much of the existing image the operator is allowed to touch: at k == 1 it
// degenerates to a hard clip (today's behaviour) and nothing moves; lowering it trades near-white content for
// highlight headroom. That trade cannot be avoided -- any smooth highlight recovery has to make room
// somewhere -- so it is better to expose it as a knob than to hide it inside a fixed curve.
float KneeShoulder(const float x, const float k)
{
	if (x <= k)
		return x;
	if (k >= 1.f)
		return min(x, 1.f);
	// f(k) = k, f'(k) = 1, f(inf) -> 1 : a rational shoulder meeting the linear segment C1-continuously
	const float s = 1.f - k;
	return 1.f - s * s / (x - k + s);
}

float Tonemap_Hable(const float x)
{
	const float A = 0.15f, B = 0.50f, C = 0.10f, D = 0.20f, E = 0.02f, F = 0.30f;
	return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

float Tonemap_ACES(const float x)
{
	// Narkowicz fit
	const float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
	return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float3 EncodeSRGB(const float3 c)
{
	return (c <= 0.0031308f) ? (c * 12.92f) : (1.055f * pow(abs(c), 1.f / 2.4f) - 0.055f);
}

// Hue-preserving application: drive the curve with the MAX CHANNEL and rescale RGB by the ratio.
//
// Using the max channel rather than luminance guarantees the result lands in [0,1] without a second clip, so a
// saturated colour keeps both its hue and its channel ratios instead of drifting. That matters more here than
// in a game: the image carries OTF-authored segmentation colours and legend/annotation colours whose hue IS
// their meaning -- a nerve canal that the legend calls red must not render orange. (This is also why ACES is
// available but not the default: its fit skews saturated reds and oranges noticeably.)
float3 ApplyCurve(const float3 rgb)
{
	const float m = max(max(rgb.r, rgb.g), rgb.b);
	if (m <= 0.f)
		return rgb;

	float m_out;
	[branch] switch (g_cbCamState.tm_operator)
	{
		case TM_KNEE:
			m_out = KneeShoulder(m, g_cbCamState.tm_knee);
			break;
		case TM_REINHARD:
		{
			// extended Reinhard: white_point maps to 1.0
			const float w = max(g_cbCamState.tm_white_point, 1e-3f);
			m_out = m * (1.f + m / (w * w)) / (1.f + m);
			break;
		}
		case TM_HABLE:
		{
			const float w = max(g_cbCamState.tm_white_point, 1e-3f);
			m_out = Tonemap_Hable(m) / max(Tonemap_Hable(w), 1e-6f);
			break;
		}
		case TM_ACES:
			m_out = Tonemap_ACES(m);
			break;
		default: // TM_CLIP -- identity up to 1.0, i.e. exactly what the UNORM store used to do
			m_out = min(m, 1.f);
			break;
	}

	return rgb * (m_out / m);
}

[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void Tonemap(uint3 DTid : SV_DispatchThreadID)
{
	if (DTid.x >= g_cbCamState.rt_width || DTid.y >= g_cbCamState.rt_height)
		return;

	// TaaResolve sanitizes its own input, but it only runs when TAA is enabled. This pass always runs, so
	// repeating the guard here is what makes the "finite, non-negative, bounded" invariant hold on the
	// presented image with TAA both on and off.
	float4 c = SanitizeRadiance(hdr_in[DTid.xy], g_cbCamState.tm_radiance_ceiling);

	// IDENTITY FAST PATH -- and it must be a direct copy, not a degenerate case of the code below.
	//
	// The general path un-premultiplies, curves, then re-premultiplies. Run that with a clip curve and it is NOT
	// what the old UNORM store did: for c = (2,2,2,0.5) the legacy store gave saturate(rgb) = (1,1,1,0.5), while
	// undo/clip/redo gives (0.5,0.5,0.5,0.5) -- the additive VXGI in-scatter produces exactly this rgb > a state.
	// So the "un-tonemapped result" that DX11.0 and the VXGI debug views are promised, and the default look on
	// DX11.3, all have to come from the same copy the UNORM store used to perform implicitly.
	if (g_cbCamState.tm_operator == TM_CLIP && g_cbCamState.tm_encode == TME_NONE && g_cbCamState.tm_exposure == 1.f)
	{
		ldr_out[DTid.xy] = float4(saturate(c.rgb), c.a);
		return;
	}

	// The composite carries premultiplied colour (the OTF LUT emits premultiplied -- see DvrCS: "note the otf
	// result is the pre-multiplied color"), and a tone curve is defined on STRAIGHT colour. Curving the
	// premultiplied value would make the result depend on coverage: two samples of the same material, one at
	// alpha 0.2 and one at 0.9, would land at different points on the curve and come back different colours.
	// Thin structures (vessel walls, cortical shells) are exactly the low-alpha case, so they would shift
	// against the OTF the author committed to. Hence: undo, curve, redo. Alpha is coverage and is never curved.
	const float a = c.a;
	float3 rgb = (a > 1e-5f) ? (c.rgb / a) : c.rgb;

	rgb *= g_cbCamState.tm_exposure;
	rgb = ApplyCurve(rgb);

	[branch] switch (g_cbCamState.tm_encode)
	{
		case TME_SRGB:
			rgb = EncodeSRGB(saturate(rgb));
			break;
		case TME_GAMMA22:
			rgb = pow(saturate(rgb), 1.f / 2.2f);
			break;
		default: // TME_NONE. Every OTF, light intensity and colour map in the engine was authored against an
			break; // unencoded pipeline, so switching an encode on silently re-tunes every existing preset.
	}

	rgb *= a; // re-premultiply

	ldr_out[DTid.xy] = float4(saturate(rgb), a);
}
