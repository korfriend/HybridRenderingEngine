#include "ShaderCommonHeader.hlsl"

cbuffer cbGlobalParams : register( b10 )
{
	VxProxyParameter g_cbProxy;
}

// Compute Shader (HLSL) 에서 output 으로 RTT 가능 버전이?!
// 이걸 지원 안 하면, MIX 는 무조건 CPU 로!
// VR 이 GPU 로 되는지... 확인...
// MIX CPU 코드 따로... (CPU Renderer)
struct PS_OUTPUT_MERGE
{
	int4 i4PsRSA_RGBA : SV_TARGET0;
	float4 f4PsRSA_DpethCS : SV_TARGET1;
};

struct VS_OUTPUT
{
	float4 f4PosSS : SV_POSITION;
};

struct PS_OUTPUT
{
	float4 f4Color : SV_TARGET0; // UNORM
	float fDepthCS : SV_TARGET1;
};

// PS //
VS_OUTPUT ProxyVS_Point( float3 f3Pos : POSITION )
{
	VS_OUTPUT vxOut = (VS_OUTPUT) 0;
	vxOut.f4PosSS = mul( float4(f3Pos, 1.f), g_cbProxy.matOS2PS );
	return vxOut;
}

// SROUT_AND_LAYEROUT_TO_LAYEROUT : SROUT + LAYEROUT -> LAYEROUT (PS)
// LAYEROUT_TO_RENDEROUT : LAYEROUT -> RENDEROUT (PS)

RWTexture2D<float4> g_rwt_RenderOut : register(u0);	// UNORM
RWTexture2D<float> g_rwt_DepthCS : register(u2);

RWStructuredBuffer<CS_LayerOut> g_rwb_LayerOut : register(u1);

[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void CsOutline(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
    if (DTid.x >= g_cbProxy.uiScreenSizeX || DTid.y >= g_cbProxy.uiScreenSizeY)
        return;
    
    float depth_c = g_tex2DPrevDepthRenderOuts[0][DTid.xy].r;
    if (depth_c > 1000000)
        return;
    
    float depth_h0 = g_tex2DPrevDepthRenderOuts[0][DTid.xy + int2(0, 1)].r;
    float depth_h1 = g_tex2DPrevDepthRenderOuts[0][DTid.xy - int2(0, 1)].r;
    float depth_w0 = g_tex2DPrevDepthRenderOuts[0][DTid.xy + int2(1, 0)].r;
    float depth_w1 = g_tex2DPrevDepthRenderOuts[0][DTid.xy - int2(1, 0)].r;
    
    const float outline_thres = g_cbProxy.fVZThickness; // test //
    float depth_min = min(min(depth_h0, depth_h1), min(depth_w0, depth_w1));
    if (depth_c - depth_min > outline_thres * 9)
        return;
    
    float diff_max = max(abs(depth_h0 - depth_h1), abs(depth_w0 - depth_w1));
    if (diff_max > outline_thres * 10)
        g_rwt_RenderOut[DTid.xy] = float4(0, 0, 0, 1);
}


[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void CsLAYEROUT_TO_RENDEROUT(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	if (DTid.x >= g_cbProxy.uiScreenSizeX || DTid.y >= g_cbProxy.uiScreenSizeY)
		return;
	const uint uiSampleAddr = DTid.x + DTid.y * g_cbProxy.uiGridOffsetX;

	float4 f4Out_Color = (float4)0;
	float fOut_Depth1stHit = FLT_MAX;

	int iNumPrevOuts = 0;
	CS_LayerOut stb_PrevLayers = g_stb_PrevLayers[uiSampleAddr];

	[unroll]
	for (int i = 0; i < NUM_MERGE_LAYERS; i++)
	{
		int iIntColor = stb_PrevLayers.iVisibilityLayers[i];
		if (iIntColor == 0)
			break;

		float4 f4Color = vxConvertColorToFloat4(iIntColor);
		iNumPrevOuts++;

		f4Out_Color += f4Color * (1.f - f4Out_Color.a);
	}

	if (iNumPrevOuts > 0)
		fOut_Depth1stHit = stb_PrevLayers.fDepthLayers[0];


	g_rwt_RenderOut[DTid.xy] = f4Out_Color;
	g_rwt_DepthCS[DTid.xy] = fOut_Depth1stHit;
	return;
}

//#define ZSLAB_BLENDING

#ifdef __CS_MODEL
[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void CsSROUT_AND_LAYEROUT_TO_LAYEROUT(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
#else
PS_OUTPUT_MERGE PsSROUT_AND_LAYEROUT_TO_LAYEROUT(VS_OUTPUT input)
#endif
{
#ifdef __CS_MODEL
	if (DTid.x >= g_cbProxy.uiScreenSizeX || DTid.y >= g_cbProxy.uiScreenSizeY)
		return;
	const uint uiSampleAddr = DTid.x + DTid.y * g_cbProxy.uiGridOffsetX;
	const int2 i2PosSS = int2(DTid.xy);
	CS_LayerOut stLayerOut = (CS_LayerOut)0;
#else
	const int2 i2PosSS = int2(input.f4PosSS.xy);
	PS_OUTPUT_MERGE stOut = (PS_OUTPUT_MERGE)0;
#endif

#ifdef ZSLAB_BLENDING
	VxRaySegment stCleanRS = (VxRaySegment)0;
	stCleanRS.fDepthBack = FLT_MAX;

	VxRaySegment stCurrentRSAs[NUM_TEXRT_LAYERS];	// of which each has same thickness //

	const float fThicknessSurface = g_cbProxy.fVZThickness; //0.001f
#else
	float4 f4LayersColorRGBAs[NUM_TEXRT_LAYERS];
	float fLayersDepthCSs[NUM_TEXRT_LAYERS];
#endif

	int i, j, iCountValidInputFromTextures = 0;

	[unroll]
	for (i = 0; i < NUM_TEXRT_LAYERS; i++)
	{
		float fDepthCS = FLT_MAX;
		float4 f4ColorIn = g_tex2DPrevColorRenderOuts[i].Load(int3(i2PosSS.xy, 0)).rgba;

#ifdef ZSLAB_BLENDING
		VxRaySegment stInputRS = stCleanRS;
		if (f4ColorIn.a > FLT_OPACITY_MIN__)
		{
			fDepthCS = g_tex2DPrevDepthRenderOuts[i].Load(int3(i2PosSS.xy, 0)).r;
			iCountValidInputFromTextures++;
			// Association Color //
			stInputRS.f4Visibility = float4(f4ColorIn.rgb * f4ColorIn.a, min(f4ColorIn.a, SAFE_OPAQUEALPHA));
			stInputRS.fDepthBack = fDepthCS;
			stInputRS.fThickness = fThicknessSurface;
		}
		stCurrentRSAs[i] = stInputRS;
#else
		if (f4ColorIn.a > FLT_OPACITY_MIN__)
		{
			f4ColorIn.rgb *= f4ColorIn.a;
			fDepthCS = g_tex2DPrevDepthRenderOuts[i].Load(int3(i2PosSS.xy, 0)).r;
			iCountValidInputFromTextures++;
		}
		f4LayersColorRGBAs[i] = f4ColorIn;
		fLayersDepthCSs[i] = fDepthCS;
#endif
	}

#ifdef __CS_MODEL

	CS_LayerOut stb_PrevLayers = g_stb_PrevLayers[uiSampleAddr];

#else
	int4 i4PsRSA_RGBA = g_tex2DPrevRSA_RGBA.Load(int3(i2PosSS, 0)).rgba;
	float4 f4PsRSA_DepthCS = g_tex2DPrevRSA_DepthCS.Load(int3(i2PosSS, 0)).rgba;
#endif

	if (iCountValidInputFromTextures == 0)
	{
		// Also, imply 2nd phase buffer initialization //
#ifdef __CS_MODEL
		g_rwb_LayerOut[uiSampleAddr] = stb_PrevLayers;
		return;
#else
		stOut.i4PsRSA_RGBA = i4PsRSA_RGBA;
		stOut.f4PsRSA_DpethCS = f4PsRSA_DepthCS;
		return stOut;
#endif
	}

	// Sorting into vxInputSlabs simply using bubble sort algorithm //
	//[loop]
	[unroll]
	for (i = 0; i < NUM_TEXRT_LAYERS; i++)
	{
		[unroll]
		for (j = NUM_TEXRT_LAYERS - 1; j >= i + 1; j--)
		{
#ifdef ZSLAB_BLENDING
			if (stCurrentRSAs[j].fDepthBack < stCurrentRSAs[j - 1].fDepthBack)
			{
				VxRaySegment stTmpRS = stCurrentRSAs[j];
				stCurrentRSAs[j] = stCurrentRSAs[j - 1];
				stCurrentRSAs[j - 1] = stTmpRS;
			}
#else
			if (fLayersDepthCSs[j] < fLayersDepthCSs[j - 1])
			{
				float fDepthCSTmp = fLayersDepthCSs[j];
				fLayersDepthCSs[j] = fLayersDepthCSs[j - 1];
				fLayersDepthCSs[j - 1] = fDepthCSTmp;

				float4 f4ColorTmp = f4LayersColorRGBAs[j];
				f4LayersColorRGBAs[j] = f4LayersColorRGBAs[j - 1];
				f4LayersColorRGBAs[j - 1] = f4ColorTmp;
			}
#endif
		}
	}

	//stOut.i4PsRSA_RGBA.rgba = int4(vxConvertColorToInt(f4LayersColorRGBAs[0]), vxConvertColorToInt(f4LayersColorRGBAs[1])
	//	, vxConvertColorToInt(f4LayersColorRGBAs[2]), vxConvertColorToInt(f4LayersColorRGBAs[3]));
	//stOut.f4PsRSA_DpethCS.rgba = float4(fLayersDepthCSs[0], fLayersDepthCSs[1], fLayersDepthCSs[2], fLayersDepthCSs[3]);
	//return stOut;
    
    //g_rwb_LayerOut[uiSampleAddr] = stb_PrevLayers;
    //return;

#ifdef ZSLAB_BLENDING
	// merge self-ovedrlapping surfaces to thickness surfaces
	float4 f4IntermixingVisibility = (float4)0;
	VxRaySegment stPriorRS, stPosteriorRS;
	VxIntermixingKernelOut stIKOut;
	int iNumElementsOfCurRSA = 0;
	for (i = 0; i < iCountValidInputFromTextures; i++)
	{
		VxRaySegment stRS_Cur = stCurrentRSAs[i];
		VxRaySegment stRS_Next;

		if (i + 1 < NUM_TEXRT_LAYERS)
		{
			stRS_Next = stCurrentRSAs[i + 1];
		}
		else
		{
			stRS_Next = stCleanRS;
		}

		if (stRS_Cur.fDepthBack > stRS_Next.fDepthBack)
		{
			stPriorRS = stRS_Next;
			stPosteriorRS = stRS_Cur;
		}
		else
		{
			stPriorRS = stRS_Cur;
			stPosteriorRS = stRS_Next;
		}

		stIKOut = vxIntermixingKernel(stPriorRS, stPosteriorRS, f4IntermixingVisibility);

		if (stIKOut.stIntegrationRS.fThickness > 0)
		{
			f4IntermixingVisibility = stIKOut.f4IntermixingVisibility;
			stCurrentRSAs[iNumElementsOfCurRSA] = stIKOut.stIntegrationRS;
			iNumElementsOfCurRSA++;

			if (f4IntermixingVisibility.a > ERT_ALPHA)
				break;

			if (stIKOut.stRemainedRS.fThickness > 0)
			{
				if (i + 1 < NUM_TEXRT_LAYERS)
					stCurrentRSAs[i + 1] = stIKOut.stRemainedRS;
			}
			else
			{
				i += 1;
			}
		}
	}

	for (i = iNumElementsOfCurRSA; i < NUM_TEXRT_LAYERS; i++)
	{
		stCurrentRSAs[i] = stCleanRS;
	}

	// intermix two different sorted thickness-surfaces
	VxRaySegment stPrevOutRSAs[NUM_MERGE_LAYERS];
	int iNumPrevOutRSs = 0;
	stPrevOutRSAs[0] = stPrevOutRSAs[1] = stPrevOutRSAs[2] = stPrevOutRSAs[3] = stCleanRS;

#ifdef __CS_MODEL
	for (i = 0; i < NUM_MERGE_LAYERS; i++)
	{
		int iIntColor = stb_PrevLayers.iVisibilityLayers[i];
		if (iIntColor == 0)
			break;

		VxRaySegment stRS;
		stRS.f4Visibility = vxConvertColorToFloat4(iIntColor);
		stRS.fDepthBack = stb_PrevLayers.fDepthLayers[i];
		stRS.fThickness = fThicknessSurface;
		iNumPrevOutRSs++;
		stPrevOutRSAs[i] = stRS;
	}
#else
	if (i4PsRSA_RGBA.r != 0)
	{
		VxRaySegment stRS;
		stRS.f4Visibility = vxConvertColorToFloat4(i4PsRSA_RGBA.r);
		stRS.fDepthBack = f4PsRSA_DepthCS.r;
		stRS.fThickness = fThicknessSurface;

		stPrevOutRSAs[0] = stRS;
		iNumPrevOutRSs++;
	}
	if (i4PsRSA_RGBA.g != 0)
	{
		VxRaySegment stRS;
		stRS.f4Visibility = vxConvertColorToFloat4(i4PsRSA_RGBA.g);
		stRS.fDepthBack = f4PsRSA_DepthCS.g;
		stRS.fThickness = fThicknessSurface;

		stPrevOutRSAs[1] = stRS;
		iNumPrevOutRSs++;
	}
	if (i4PsRSA_RGBA.b != 0)
	{
		VxRaySegment stRS;
		stRS.f4Visibility = vxConvertColorToFloat4(i4PsRSA_RGBA.b);
		stRS.fDepthBack = f4PsRSA_DepthCS.b;
		stRS.fThickness = fThicknessSurface;

		stPrevOutRSAs[2] = stRS;
		iNumPrevOutRSs++;
	}
	if (i4PsRSA_RGBA.a != 0)
	{
		VxRaySegment stRS;
		stRS.f4Visibility = vxConvertColorToFloat4(i4PsRSA_RGBA.a);
		stRS.fDepthBack = f4PsRSA_DepthCS.a;
		stRS.fThickness = fThicknessSurface;

		stPrevOutRSAs[3] = stRS;
		iNumPrevOutRSs++;
	}
#endif

	VXMIXMAIN;

	for (i = iCountStoredBuffer; i < NUM_MERGE_LAYERS; i++)
	{
		stOutRSAs[i] = stCleanRS;
	}

#ifdef __CS_MODEL
	[unroll]
	for (i = 0; i < NUM_MERGE_LAYERS; i++)
	{
		stLayerOut.iVisibilityLayers[i] = vxConvertColorToInt(stOutRSAs[i].f4Visibility);
		stLayerOut.fDepthLayers[i] = stOutRSAs[i].fDepthBack;
	}
#else
	stOut.i4PsRSA_RGBA.rgba = int4(vxConvertColorToInt(stOutRSAs[0].f4Visibility), vxConvertColorToInt(stOutRSAs[1].f4Visibility)
		, vxConvertColorToInt(stOutRSAs[2].f4Visibility), vxConvertColorToInt(stOutRSAs[3].f4Visibility));
	stOut.f4PsRSA_DpethCS.rgba = float4(stOutRSAs[0].fDepthBack, stOutRSAs[1].fDepthBack, stOutRSAs[2].fDepthBack, stOutRSAs[3].fDepthBack);
#endif

#else
	float4 f4PrevOutLayersColorRGBAs[NUM_TEXRT_LAYERS] = { (float4)0, (float4)0, (float4)0, (float4)0 };
	float fPrevOutLayersDepthCSs[NUM_TEXRT_LAYERS] = {FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX};

	int iNumPrevOuts = 0;

#ifdef __CS_MODEL
	for (i = 0; i < NUM_MERGE_LAYERS; i++)
	{
		int iIntColor = stb_PrevLayers.iVisibilityLayers[i];
		if (iIntColor == 0)
			break;

		float4 f4Color = vxConvertColorToFloat4(iIntColor);
		f4PrevOutLayersColorRGBAs[i] = f4Color;
		fPrevOutLayersDepthCSs[i] = stb_PrevLayers.fDepthLayers[i];
		iNumPrevOuts++;
	}
#else
	if (i4PsRSA_RGBA.r != 0)
	{
		f4PrevOutLayersColorRGBAs[0] = vxConvertColorToFloat4(i4PsRSA_RGBA.r);
		fPrevOutLayersDepthCSs[0] = f4PsRSA_DepthCS.r;

		iNumPrevOuts++;
	}
	if (i4PsRSA_RGBA.g != 0)
	{
		f4PrevOutLayersColorRGBAs[1] = vxConvertColorToFloat4(i4PsRSA_RGBA.g);
		fPrevOutLayersDepthCSs[1] = f4PsRSA_DepthCS.g;

		iNumPrevOuts++;
	}
	if (i4PsRSA_RGBA.b != 0)
	{
		f4PrevOutLayersColorRGBAs[2] = vxConvertColorToFloat4(i4PsRSA_RGBA.b);
		fPrevOutLayersDepthCSs[2] = f4PsRSA_DepthCS.b;

		iNumPrevOuts++;
	}
	if (i4PsRSA_RGBA.a != 0)
	{
		f4PrevOutLayersColorRGBAs[3] = vxConvertColorToFloat4(i4PsRSA_RGBA.a);
		fPrevOutLayersDepthCSs[3] = f4PsRSA_DepthCS.a;

		iNumPrevOuts++;
	}
#endif

	float4 f4MixOutLayersColorRGBAs[NUM_TEXRT_LAYERS] = { (float4)0, (float4)0, (float4)0, (float4)0 };
	float fMixOutLayersDepthCSs[NUM_TEXRT_LAYERS] = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };
	int iNumMixOuts = i = j = 0;

	//while (i < iCountValidInputFromTextures && j < iNumPrevOuts && iNumMixOuts < NUM_TEXRT_LAYERS)
	[unroll]
	for (; i < iCountValidInputFromTextures && j < iNumPrevOuts && iNumMixOuts < NUM_TEXRT_LAYERS;)
	{
		if (fLayersDepthCSs[i] < fPrevOutLayersDepthCSs[j])
		{
			f4MixOutLayersColorRGBAs[iNumMixOuts] = f4LayersColorRGBAs[i];
			fMixOutLayersDepthCSs[iNumMixOuts] = fLayersDepthCSs[i];
			iNumMixOuts++;
			i++;
		}
		else
		{
			f4MixOutLayersColorRGBAs[iNumMixOuts] = f4PrevOutLayersColorRGBAs[j];
			fMixOutLayersDepthCSs[iNumMixOuts] = fPrevOutLayersDepthCSs[j];
			iNumMixOuts++;
			j++;
		}
	}

	//stOut.i4PsRSA_RGBA.rgba = int4(vxConvertColorToInt(f4MixOutLayersColorRGBAs[0]), vxConvertColorToInt(f4MixOutLayersColorRGBAs[1])
	//	, vxConvertColorToInt(f4PrevOutLayersColorRGBAs[2]), vxConvertColorToInt(f4PrevOutLayersColorRGBAs[3]));
	//stOut.f4PsRSA_DpethCS.rgba = float4(fMixOutLayersDepthCSs[0], fPrevOutLayersDepthCSs[1], fMixOutLayersDepthCSs[2], fPrevOutLayersDepthCSs[3]);
	//return stOut;

	//while (i < iCountValidInputFromTextures && iNumMixOuts < NUM_TEXRT_LAYERS)
	[unroll]
	for (; i < iCountValidInputFromTextures && iNumMixOuts < NUM_TEXRT_LAYERS;)
	{
		f4MixOutLayersColorRGBAs[iNumMixOuts] = f4LayersColorRGBAs[i];
		fMixOutLayersDepthCSs[iNumMixOuts] = fLayersDepthCSs[i];
		iNumMixOuts++;
		i++;
	}
	//
	////while (j < iNumPrevOuts && iNumMixOuts < NUM_TEXRT_LAYERS)
	[unroll]
	for (; j < iNumPrevOuts && iNumMixOuts < NUM_TEXRT_LAYERS;)
	{
		f4MixOutLayersColorRGBAs[iNumMixOuts] = f4PrevOutLayersColorRGBAs[j];
		fMixOutLayersDepthCSs[iNumMixOuts] = fPrevOutLayersDepthCSs[j];
		iNumMixOuts++;
		j++;
	}

#ifdef __CS_MODEL
	[unroll]
	for (i = 0; i < NUM_MERGE_LAYERS; i++)
	{
		stLayerOut.iVisibilityLayers[i] = vxConvertColorToInt(f4MixOutLayersColorRGBAs[i]);
		stLayerOut.fDepthLayers[i] = fMixOutLayersDepthCSs[i];
	}
#else
	stOut.i4PsRSA_RGBA.rgba = int4(vxConvertColorToInt(f4MixOutLayersColorRGBAs[0]), vxConvertColorToInt(f4MixOutLayersColorRGBAs[1])
		, vxConvertColorToInt(f4MixOutLayersColorRGBAs[2]), vxConvertColorToInt(f4MixOutLayersColorRGBAs[3]));
	stOut.f4PsRSA_DpethCS.rgba = float4(fMixOutLayersDepthCSs[0], fMixOutLayersDepthCSs[1], fMixOutLayersDepthCSs[2], fMixOutLayersDepthCSs[3]);
#endif

#endif	// #ifdef ZSLAB_BLENDING
	
#ifdef __CS_MODEL
	g_rwb_LayerOut[uiSampleAddr] = stLayerOut;
	return;
#else
	return stOut;
#endif

}



/*
PS_OUTPUT_MERGE PsSROUT_AND_LAYEROUT_TO_LAYEROUT_____(VS_OUTPUT input)
{
	const int2 i2PosSS = int2(input.f4PosSS.xy);
	PS_OUTPUT_MERGE stOut = (PS_OUTPUT_MERGE)0;

	float4 f4LayersColorRGBAs[NUM_TEXRT_LAYERS];
	float fLayersDepthCSs[NUM_TEXRT_LAYERS];

	int i, j, iCountValidInputFromTextures = 0;
	const float fThicknessSurface = g_cbProxy.fVZThickness; //0.001f

	[unroll]
	for (i = 0; i < NUM_TEXRT_LAYERS; i++)
	{
		float4 f4ColorIn = (float4)0;
		float fDepthCS = FLT_MAX;

		f4ColorIn = g_tex2DPrevColorRenderOuts[i].Load(int3(i2PosSS.xy, 0)).rgba;

		if (f4ColorIn.a > FLT_OPACITY_MIN__)
		{
			f4ColorIn.rgb *= f4ColorIn.a;
			fDepthCS = g_tex2DPrevDepthRenderOuts[i].Load(int3(i2PosSS.xy, 0)).r;
			iCountValidInputFromTextures++;
		}
		f4LayersColorRGBAs[i] = f4ColorIn;
		fLayersDepthCSs[i] = fDepthCS;
	}

	int4 i4PsRSA_RGBA = g_tex2DPrevRSA_RGBA.Load(int3(i2PosSS, 0)).rgba;
	float4 f4PsRSA_DepthCS = g_tex2DPrevRSA_DepthCS.Load(int3(i2PosSS, 0)).rgba;
	
	if (iCountValidInputFromTextures == 0)
	{
		// Also, imply 2nd phase buffer initialization //
		stOut.i4PsRSA_RGBA = i4PsRSA_RGBA;
		stOut.f4PsRSA_DpethCS = f4PsRSA_DepthCS;
		return stOut;
	}

	// Sorting into vxInputSlabs simply using bubble sort algorithm //
	//[loop]
	[unroll]
	for (i = 0; i < NUM_TEXRT_LAYERS; i++)
	{
		[unroll]
		for (j = NUM_TEXRT_LAYERS - 1; j >= i + 1; j--)
		{
			if (fLayersDepthCSs[j] < fLayersDepthCSs[j - 1])
			{
				float fDepthCSTmp = fLayersDepthCSs[j];
				fLayersDepthCSs[j] = fLayersDepthCSs[j - 1];
				fLayersDepthCSs[j - 1] = fDepthCSTmp;
	
				float4 f4ColorTmp = f4LayersColorRGBAs[j];
				f4LayersColorRGBAs[j] = f4LayersColorRGBAs[j - 1];
				f4LayersColorRGBAs[j - 1] = f4ColorTmp;
			}
		}
	}
	//stOut.i4PsRSA_RGBA.rgba = int4(vxConvertColorToInt(f4LayersColorRGBAs[0]), vxConvertColorToInt(f4LayersColorRGBAs[1])
	//	, vxConvertColorToInt(f4LayersColorRGBAs[2]), vxConvertColorToInt(f4LayersColorRGBAs[3]));
	//stOut.f4PsRSA_DpethCS.rgba = float4(fLayersDepthCSs[0], fLayersDepthCSs[1], fLayersDepthCSs[2], fLayersDepthCSs[3]);
	//return stOut;


	float4 f4PrevOutLayersColorRGBAs[NUM_TEXRT_LAYERS] = { (float4)0, (float4)0, (float4)0, (float4)0 };
	float fPrevOutLayersDepthCSs[NUM_TEXRT_LAYERS] = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };

	int iNumPrevOuts = 0;
	if (i4PsRSA_RGBA.r != 0)
	{
		f4PrevOutLayersColorRGBAs[0] = vxConvertColorToFloat4(i4PsRSA_RGBA.r);
		fPrevOutLayersDepthCSs[0] = f4PsRSA_DepthCS.r;
	
		iNumPrevOuts++;
	}
	if (i4PsRSA_RGBA.g != 0)
	{
		f4PrevOutLayersColorRGBAs[1] = vxConvertColorToFloat4(i4PsRSA_RGBA.g);
		fPrevOutLayersDepthCSs[1] = f4PsRSA_DepthCS.g;
	
		iNumPrevOuts++;
	}
	if (i4PsRSA_RGBA.b != 0)
	{
		f4PrevOutLayersColorRGBAs[2] = vxConvertColorToFloat4(i4PsRSA_RGBA.b);
		fPrevOutLayersDepthCSs[2] = f4PsRSA_DepthCS.b;
	
		iNumPrevOuts++;
	}
	if (i4PsRSA_RGBA.a != 0)
	{
		f4PrevOutLayersColorRGBAs[3] = vxConvertColorToFloat4(i4PsRSA_RGBA.a);
		fPrevOutLayersDepthCSs[3] = f4PsRSA_DepthCS.a;
	
		iNumPrevOuts++;
	}

	// RUNNING MIX 
	// f4PrevOutLayersColorRGBAs, fPrevOutLayersDepthCSs, iNumPrevOuts
	// f4LayersColorRGBAs, fLayersDepthCSs, iCountValidInputFromTextures
	// ==> f4CurOutLayersColorRGBAs, fCurOutLayersDepthCSs, iNumCurOuts


	//stOut.i4PsRSA_RGBA.rgba = int4(vxConvertColorToInt(f4PrevOutLayersColorRGBAs[0]), vxConvertColorToInt(f4LayersColorRGBAs[1])
	//	, vxConvertColorToInt(f4PrevOutLayersColorRGBAs[2]), vxConvertColorToInt(f4LayersColorRGBAs[3]));
	//stOut.f4PsRSA_DpethCS.rgba = float4(fPrevOutLayersDepthCSs[0], fLayersDepthCSs[1], fPrevOutLayersDepthCSs[2], fLayersDepthCSs[3]);
	//return stOut;


	float4 f4MixOutLayersColorRGBAs[NUM_TEXRT_LAYERS] = { (float4)0, (float4)0, (float4)0, (float4)0 };
	float fMixOutLayersDepthCSs[NUM_TEXRT_LAYERS] = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };
	int iNumMixOuts = i = j = 0;
	while (i < iCountValidInputFromTextures && j < iNumPrevOuts && iNumMixOuts < NUM_TEXRT_LAYERS)
	{
		if (fLayersDepthCSs[i] < fPrevOutLayersDepthCSs[j])
		{
			f4MixOutLayersColorRGBAs[iNumMixOuts] = f4LayersColorRGBAs[i];
			fMixOutLayersDepthCSs[iNumMixOuts] = fLayersDepthCSs[i];
			iNumMixOuts++;
			i++;
		}
		else
		{
			f4MixOutLayersColorRGBAs[iNumMixOuts] = f4PrevOutLayersColorRGBAs[j];
			fMixOutLayersDepthCSs[iNumMixOuts] = fPrevOutLayersDepthCSs[j];
			iNumMixOuts++;
			j++;
		}
	}

	while (i < iCountValidInputFromTextures && iNumMixOuts < NUM_TEXRT_LAYERS)
	{
		f4MixOutLayersColorRGBAs[iNumMixOuts] = f4LayersColorRGBAs[i];
		fMixOutLayersDepthCSs[iNumMixOuts] = fLayersDepthCSs[i];
		iNumMixOuts++;
		i++;
	}

	while (j < iNumPrevOuts && iNumMixOuts < NUM_TEXRT_LAYERS)
	{
		f4MixOutLayersColorRGBAs[iNumMixOuts] = f4PrevOutLayersColorRGBAs[j];
		fMixOutLayersDepthCSs[iNumMixOuts] = fPrevOutLayersDepthCSs[j];
		iNumMixOuts++;
		j++;
	}

	//stOut.i4PsRSA_RGBA.rgba = int4(vxConvertColorToInt(f4PrevOutLayersColorRGBAs[0]), vxConvertColorToInt(f4PrevOutLayersColorRGBAs[1])
	//	, vxConvertColorToInt(f4PrevOutLayersColorRGBAs[2]), vxConvertColorToInt(f4PrevOutLayersColorRGBAs[3]));
	stOut.i4PsRSA_RGBA.rgba = int4(vxConvertColorToInt(f4MixOutLayersColorRGBAs[0]), vxConvertColorToInt(f4MixOutLayersColorRGBAs[1])
		, vxConvertColorToInt(f4MixOutLayersColorRGBAs[2]), vxConvertColorToInt(f4MixOutLayersColorRGBAs[3]));
	//stOut.i4PsRSA_RGBA.rgba = int4(vxConvertColorToInt(f4LayersColorRGBAs[0]), vxConvertColorToInt(f4LayersColorRGBAs[1])
	//	, vxConvertColorToInt(f4LayersColorRGBAs[2]), vxConvertColorToInt(f4LayersColorRGBAs[3]));
	stOut.f4PsRSA_DpethCS.rgba = float4(fMixOutLayersDepthCSs[0], fMixOutLayersDepthCSs[1], fMixOutLayersDepthCSs[2], fMixOutLayersDepthCSs[3]);
	return stOut;

	// Reuse f4LayersColorRGBAs, fLayersDepthCSs, fAlpha
	f4LayersColorRGBAs[NUM_TEXRT_LAYERS - 1] = (float4)0;
	fLayersDepthCSs[NUM_TEXRT_LAYERS - 1] = FLT_MAX;
	int iNumMixMergeOuts = 0;
	float fAlpha = 0;
	[unroll]
	for (i = 0; i < iNumMixOuts; i++)
	{
		float fDepthMixCur = fMixOutLayersDepthCSs[i];
		float4 f4ColorMixCur = f4MixOutLayersColorRGBAs[i];

			[unroll]
			for (int j = i + 1; j < iNumMixOuts; j++)
			{
				float fDepthMixNext = fMixOutLayersDepthCSs[j];
				float4 f4ColorMixNext = f4MixOutLayersColorRGBAs[j];

				if (fDepthMixNext - fDepthMixCur > fThicknessSurface)
					break;

				f4ColorMixCur += f4ColorMixNext * (1.f - f4ColorMixCur.a);
				fDepthMixCur = (fDepthMixCur + fDepthMixNext) * 0.5f;
				i++;
			}
		if (iNumMixMergeOuts < NUM_TEXRT_LAYERS - 1)
		{
			f4LayersColorRGBAs[iNumMixMergeOuts] = f4ColorMixCur;
			fLayersDepthCSs[iNumMixMergeOuts] = fDepthMixCur;
			iNumMixMergeOuts++;
		}
		else // iNumMixMergeOuts = NUM_TEXRT_LAYERS - 1
		{
			float4 f4ColorMixPrev = f4LayersColorRGBAs[NUM_TEXRT_LAYERS - 1];
				float fDepthMixPrev = fLayersDepthCSs[NUM_TEXRT_LAYERS - 1];
			f4LayersColorRGBAs[NUM_TEXRT_LAYERS - 1] = f4ColorMixPrev + f4ColorMixCur * (1.f - f4ColorMixPrev.a);
			//f4LayersColorRGBAs[NUM_TEXRT_LAYERS - 1] = vxFusionVisibility(f4ColorMixPrev, f4ColorMixCur);
			fLayersDepthCSs[NUM_TEXRT_LAYERS - 1] = min(fDepthMixPrev, fDepthMixCur);
			iNumMixMergeOuts = NUM_TEXRT_LAYERS;
		}

		fAlpha += f4ColorMixCur.a * (1.f - fAlpha);
		if (fAlpha > ERT_ALPHA)
			break;
	}

	[unroll]
	for (i = iNumMixMergeOuts; i < NUM_MERGE_LAYERS; i++)
	{
		f4LayersColorRGBAs[i] = (float4)0;
		fLayersDepthCSs[i] = FLT_MAX;
	}

	stOut.i4PsRSA_RGBA.rgba = int4(vxConvertColorToInt(f4LayersColorRGBAs[0]), vxConvertColorToInt(f4LayersColorRGBAs[1])
		, vxConvertColorToInt(f4LayersColorRGBAs[2]), vxConvertColorToInt(f4LayersColorRGBAs[3]));
	stOut.f4PsRSA_DpethCS.rgba = float4(fLayersDepthCSs[0], fLayersDepthCSs[1], fLayersDepthCSs[2], fLayersDepthCSs[3]);
	return stOut;
}

*/