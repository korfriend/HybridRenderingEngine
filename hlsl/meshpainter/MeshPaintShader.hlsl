// MeshPaintShader.hlsl
// Shaders for mesh painting system
// - Brush stroke rendering (to paint texture)
// - Paint blending (base texture + paint texture)

#include "../macros.hlsl"

// ============================================================================
// Samplers
// ============================================================================
SamplerState g_samplerLinear : register(s0); // ZeroBoader
SamplerState g_samplerPoint : register(s1); // ZeroBoader
SamplerState g_samplerLinear_clamp : register(s2);
SamplerState g_samplerPoint_clamp : register(s3);
SamplerState g_samplerLinear_wrap : register(s4);
SamplerState g_samplerPoint_wrap : register(s5);

// ============================================================================
// Constant Buffers
// ============================================================================

// Brush parameters for painting strokes
cbuffer CB_Brush : register(b1)
{
    float4 brushColor;       // RGBA brush color
    
	float3 brushCenter; // world space center
	float brushStrength; // Opacity/strength [0-1]
    
    float3 brushRadius;      // world space radius (x, y)
    float brushHardness;     // Edge falloff [0-1]
    
	float4x4 matOS2WS;
    
	int blendMode; // PaintBlendMode enum
	float padding0;
	float padding1;
	float padding2;
};

// Paint layer parameters for compositing
cbuffer CB_PaintLayer : register(b2)
{
    int paintBlendMode;      // Blend mode for compositing
    float paintOpacity;      // Overall layer opacity
    float2 paintPadding;
};

// ============================================================================
// Textures
// ============================================================================
Texture2D<float4> tex2d_base : register(t0);      // Base/diffuse texture
Texture2D<float4> tex2d_paint : register(t1);     // Paint layer texture
Texture2D<float4> tex2d_previous : register(t2);  // Previous paint state (for ping-pong)

StructuredBuffer<float2> g_uvBuffer : register(t61);
// ============================================================================
// Blend Mode Functions
// ============================================================================

// Blend modes matching PaintBlendMode enum
#define BLEND_NORMAL     0
#define BLEND_MULTIPLY   1
#define BLEND_ADDITIVE   2
#define BLEND_OVERLAY    3
#define BLEND_SOFT_LIGHT 4
#define BLEND_SCREEN     5

float3 BlendNormal(float3 base, float3 blend, float opacity)
{
    return lerp(base, blend, opacity);
}

float3 BlendMultiply(float3 base, float3 blend, float opacity)
{
    float3 result = base * blend;
    return lerp(base, result, opacity);
}

float3 BlendAdditive(float3 base, float3 blend, float opacity)
{
    float3 result = saturate(base + blend);
    return lerp(base, result, opacity);
}

float OverlayChannel(float base, float blend)
{
    if (base < 0.5)
        return 2.0 * base * blend;
    else
        return 1.0 - 2.0 * (1.0 - base) * (1.0 - blend);
}

float3 BlendOverlay(float3 base, float3 blend, float opacity)
{
    float3 result;
    result.r = OverlayChannel(base.r, blend.r);
    result.g = OverlayChannel(base.g, blend.g);
    result.b = OverlayChannel(base.b, blend.b);
    return lerp(base, result, opacity);
}

float SoftLightChannel(float base, float blend)
{
    if (blend < 0.5)
        return base - (1.0 - 2.0 * blend) * base * (1.0 - base);
    else
        return base + (2.0 * blend - 1.0) * (sqrt(base) - base);
}

float3 BlendSoftLight(float3 base, float3 blend, float opacity)
{
    float3 result;
    result.r = SoftLightChannel(base.r, blend.r);
    result.g = SoftLightChannel(base.g, blend.g);
    result.b = SoftLightChannel(base.b, blend.b);
    return lerp(base, result, opacity);
}

float3 BlendScreen(float3 base, float3 blend, float opacity)
{
    float3 result = 1.0 - (1.0 - base) * (1.0 - blend);
    return lerp(base, result, opacity);
}

float3 ApplyBlendMode(float3 base, float3 blend, float opacity, int mode)
{
    switch (mode)
    {
        case BLEND_NORMAL:
            return BlendNormal(base, blend, opacity);
        case BLEND_MULTIPLY:
            return BlendMultiply(base, blend, opacity);
        case BLEND_ADDITIVE:
            return BlendAdditive(base, blend, opacity);
        case BLEND_OVERLAY:
            return BlendOverlay(base, blend, opacity);
        case BLEND_SOFT_LIGHT:
            return BlendSoftLight(base, blend, opacity);
        case BLEND_SCREEN:
            return BlendScreen(base, blend, opacity);
        default:
            return BlendNormal(base, blend, opacity);
    }
}

// ============================================================================
// Vertex Shader - Fullscreen Quad
// ============================================================================

struct VS_OUTPUT
{
    float4 position : SV_Position;
	float2 texcoord : TEXCOORD0;
	float3 worldPos : TEXCOORD1;
};

VS_OUTPUT VS_Brush(float3 position : POSITION, uint vertexID : SV_VertexID)
{
	VS_OUTPUT output;
    
	float2 uv = g_uvBuffer[vertexID];
      // Use UV as screen position (maps to render target)
	output.position = float4(uv * 2.0 - 1.0, 0.0, 1.0);
	output.position.y = -output.position.y; // Flip Y for DX coordinate system

	// Texcoord for sampling previous paint (same UV coordinate system)
	output.texcoord = uv;

      // Pass world position to pixel shader
	float4 pos_ws = mul(matOS2WS, float4(position, 1));
	output.worldPos = pos_ws.xyz / pos_ws.w;
	return output;
}

// Alternative: Generate fullscreen quad from vertex ID (no vertex buffer needed)
VS_OUTPUT VS_FullscreenQuad(uint vertexID : SV_VertexID)
{
    VS_OUTPUT output;

    // Generate fullscreen triangle strip from vertex ID
    // 0: (-1, 1), 1: (1, 1), 2: (-1, -1), 3: (1, -1)
    float2 uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    output.position.y = -output.position.y; // Flip Y for DX coordinate system
    output.texcoord = uv;

    return output;
}

// ============================================================================
// Pixel Shader - Brush Stroke
// ============================================================================

float4 PS_BrushStroke(VS_OUTPUT input) : SV_Target
{
	//return float4(input.texcoord, 0, 1);
    float3 posWS = input.worldPos;

    // Calculate distance from brush center
	float3 delta = (posWS - brushCenter) / 5.f;//brushRadius;
    float dist = length(delta);

    // Outside brush radius
    if (dist > 1.0)
        discard;
    
    // Calculate brush falloff based on hardness
    // hardness = 1.0: hard edge, hardness = 0.0: soft edge
    float falloffStart = brushHardness;
    float falloff = 1.0 - smoothstep(falloffStart, 1.0, dist);

    // Apply strength
    float alpha = brushColor.a * brushStrength * falloff;

    // Get previous paint value
	float4 prevPaint = tex2d_previous.Sample(g_samplerPoint, input.texcoord);

    // Blend brush stroke with previous paint
	//float3 blendedColor = ApplyBlendMode(prevPaint.rgb, brushColor.rgb, alpha, blendMode);
	float3 blendedColor = ApplyBlendMode(prevPaint.rgb, float3(0, 1, 0), alpha, blendMode);
    float blendedAlpha = saturate(prevPaint.a + alpha * (1.0 - prevPaint.a));
    
	return float4(blendedColor, blendedAlpha);
}

// Simple brush stroke (overwrites with alpha blend)
//float4 PS_BrushStrokeSimple(VS_OUTPUT input) : SV_Target
//{
//    float2 uv = input.texcoord;
//
//    float2 delta = (uv - brushCenter) / brushRadius;
//    float dist = length(delta);
//
//    if (dist > 1.0)
//        discard;
//
//    float falloffStart = brushHardness;
//    float falloff = 1.0 - smoothstep(falloffStart, 1.0, dist);
//    float alpha = brushColor.a * brushStrength * falloff;
//
//    return float4(brushColor.rgb, alpha);
//}

// ============================================================================
// Pixel Shader - Paint Layer Compositing
// ============================================================================

// Composite paint layer onto base texture
float4 PS_CompositePaint(VS_OUTPUT input) : SV_Target
{
    float2 uv = input.texcoord;

    float4 baseColor = tex2d_base.Sample(g_samplerLinear, uv);
    float4 paintColor = tex2d_paint.Sample(g_samplerLinear, uv);

    // Apply paint layer with blend mode
    float effectiveOpacity = paintColor.a * paintOpacity;
    float3 blendedColor = ApplyBlendMode(baseColor.rgb, paintColor.rgb, effectiveOpacity, paintBlendMode);

    return float4(blendedColor, baseColor.a);
}

// ============================================================================
// Pixel Shader - Copy/Passthrough
// ============================================================================

float4 PS_Copy(VS_OUTPUT input) : SV_Target
{
    return tex2d_base.Sample(g_samplerLinear, input.texcoord);
}

// ============================================================================
// Pixel Shader - Clear to transparent
// ============================================================================

float4 PS_ClearTransparent(VS_OUTPUT input) : SV_Target
{
    return float4(0, 0, 0, 0);
}

// ============================================================================
// Dilation Shader (for UV seam bleeding)
// ============================================================================

float4 PS_Dilate(VS_OUTPUT input) : SV_Target
{
    float2 uv = input.texcoord;
    float4 center = tex2d_base.Sample(g_samplerPoint, uv);

    // If this pixel has paint, keep it
    if (center.a > 0.001)
        return center;

    // Get texture dimensions
    float2 texSize;
    tex2d_base.GetDimensions(texSize.x, texSize.y);
    float2 texelSize = 1.0 / texSize;

    // Sample neighbors
    float4 sum = float4(0, 0, 0, 0);
    float count = 0;

    [unroll]
    for (int y = -1; y <= 1; y++)
    {
        [unroll]
        for (int x = -1; x <= 1; x++)
        {
            if (x == 0 && y == 0) continue;

            float2 offset = float2(x, y) * texelSize;
            float4 neighbor = tex2d_base.Sample(g_samplerPoint, uv + offset);

            if (neighbor.a > 0.001)
            {
                sum += neighbor;
                count += 1.0;
            }
        }
    }

    if (count > 0)
    {
        return sum / count;
    }

    return center;
}
