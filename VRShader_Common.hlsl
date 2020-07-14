#include "ShaderCommonHeader.hlsl"

cbuffer cbGlobalParams : register( b0 )	 // SLOT 0
{
	VxVolumeObject g_cbVolObj;
}

cbuffer cbGlobalParams : register( b1 )	 // SLOT 1
{
	VxCameraState g_cbCamState;
}

cbuffer cbGlobalParams : register( b2 )	 // SLOT 2
{
	VxOtf1D g_cbOtf1D;
}

cbuffer cbGlobalParams : register( b3 )	 // SLOT 3
{
	VxBlockObject g_cbBlkObj;
}

cbuffer cbGlobalParams : register( b4 )	 // SLOT 4
{
	VxSurfaceEffect g_cbSurfEffect;
}

cbuffer cbGlobalParams : register( b5 )	 // SLOT 5
{
	VxModulation g_cbModulation;
}

cbuffer cbGlobalParams : register( b6 )	 // SLOT 6
{
	VxBrickChunk g_cbBlkChunk;
}

cbuffer cbGlobalParams : register( b7 )	 // SLOT 7
{
	VxShadowMap g_cbShadowMap;
}

cbuffer cbGlobalParams : register( b8 )	 // SLOT 8
{
	VxInterestingRegion g_cbInterestingRegion;
}

struct VS_OUTPUT
{
	float4 f4PosSS : SV_POSITION;
};

struct PS_OUTPUT
{
	float4 f4Color : SV_TARGET0; // UNORM
	float fDepthCS : SV_TARGET1;
};

struct PS_OUTPUT_SHADOW
{
	float f1stDepthCS : SV_TARGET0; // float
};

//=====================
// Texture Buffers
//=====================
Buffer<float4> g_f4bufOTF : register(t0);		// UNORM Mask OTFs
Buffer<float4> g_f4bufWindowing : register(t9);		// UNORM // OTFs
Buffer<int> g_ibufOTFIndex : register(t3);		// Mask OTFs

Texture3D g_tex3DVolume : register(t1);
Texture3D g_tex3DBlock : register(t2);
Texture3D g_tex3DVolumeMask : register(t8);

Texture2D<float> g_tex2DShadowMap : register(t4); // r (float)
#ifdef __CS_MODEL
RWTexture2D<float4> g_rwt_RenderOut : register(u0);	// UNORM
RWTexture2D<float> g_rwt_DepthCS : register(u2);
#endif

// Brick Tex3D : register(t10)~~;

// input layer : INPUT_LAYERS (ZSLAB_BLENDING or not)
// rendering mode : 
// OTF_MASK
// RM_SURFACEEFFECT | RM_SHADOW
// RM_MODULATION | RM_SHADOW | RM_CLIPSEMIOPAQUE
// MERGE_OUT

#ifdef __CS_MODEL
#define RETURN_DEFINED return;
#else
#define RETURN_DEFINED return stOut;
#endif


#ifdef __CS_MODEL
#define SET_OUTPUT(COLOR_VALUE, DEPTH_VALUE) \
g_rwt_RenderOut[DTid.xy] = COLOR_VALUE;\
g_rwt_DepthCS[DTid.xy] = DEPTH_VALUE;
#else
#define SET_OUTPUT(COLOR_VALUE, DEPTH_VALUE) \
stOut.f4Color = COLOR_VALUE;\
stOut.fDepthCS = DEPTH_VALUE;
#endif


//==============//
// Main VR Code //

#ifdef RM_SHADOW
float SampleOpacityOnShadowMap_PCF(int2 i2PosMS, float fDepthCurrentMS)
{
	float fDepthShadowMap = g_tex2DShadowMap.Load(int3(i2PosMS.xy, 0)).r;
	if (fDepthShadowMap > fDepthCurrentMS)
		return 0;
	return 1.f;
}

float SampleOpacityOnShadowMap_3x3(int2 i2PosMS, float fDepthCurrentMS)
{
	// Filter Kernel Size 3x3
	float fOpacity0 = SampleOpacityOnShadowMap_PCF(i2PosMS + float2(-1, -1), fDepthCurrentMS);
	float fOpacity1 = SampleOpacityOnShadowMap_PCF(i2PosMS + float2(0, -1), fDepthCurrentMS);
	float fOpacity2 = SampleOpacityOnShadowMap_PCF(i2PosMS + float2(1, -1), fDepthCurrentMS);
	float fOpacity3 = SampleOpacityOnShadowMap_PCF(i2PosMS + float2(-1, 0), fDepthCurrentMS);
	float fOpacity4 = SampleOpacityOnShadowMap_PCF(i2PosMS + float2(0, 0), fDepthCurrentMS);
	float fOpacity5 = SampleOpacityOnShadowMap_PCF(i2PosMS + float2(1, 0), fDepthCurrentMS);
	float fOpacity6 = SampleOpacityOnShadowMap_PCF(i2PosMS + float2(-1, 1), fDepthCurrentMS);
	float fOpacity7 = SampleOpacityOnShadowMap_PCF(i2PosMS + float2(0, 1), fDepthCurrentMS);
	float fOpacity8 = SampleOpacityOnShadowMap_PCF(i2PosMS + float2(1, 1), fDepthCurrentMS);
	return (fOpacity0 + fOpacity1 + fOpacity2 + fOpacity3 + fOpacity4 + fOpacity5 + fOpacity6 + fOpacity7 + fOpacity8) / 9;
}
#endif

#ifdef __CS_MODEL
[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void CsDVR_Common(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
#else
PS_OUTPUT PsDVR_Common(VS_OUTPUT input)
#endif
{
#ifdef __CS_MODEL
	if (DTid.x >= g_cbCamState.uiScreenSizeX || DTid.y >= g_cbCamState.uiScreenSizeY)
		return;
	
	// TEST //
	//float fOpacityFiltered = SampleOpacityOnShadowMap_3x3(DTid.xy, 1000);
	//if (fOpacityFiltered > 0)
	//{
	//	SET_OUTPUT(float4(1, 0, 0, 1), 1.0)
	//}
	//else
	//{
	//	//SET_OUTPUT(float4(0, 0, 1, 1), 1.0)
	//}
	//
	//RETURN_DEFINED

	SET_OUTPUT((float4)0, FLT_MAX)
	bool bReturnEarly = false;
	int2 i2PosSS;
	if (g_cbCamState.uiScreenAaBbMinX <= DTid.x
		&& g_cbCamState.uiScreenAaBbMaxX > DTid.x 
		&& g_cbCamState.uiScreenAaBbMinY <= DTid.y 
		&& g_cbCamState.uiScreenAaBbMaxY > DTid.y
		)
	{
		uint2 ui2PosRS = DTid.xy - uint2(g_cbCamState.uiScreenAaBbMinX, g_cbCamState.uiScreenAaBbMinY);
		if (ui2PosRS.x >= g_cbCamState.uiRenderingSizeX || ui2PosRS.y >= g_cbCamState.uiRenderingSizeY)
		{
			return;
		}

		float3 f3PosSS = vxTransformPoint(float3(ui2PosRS, 0), g_cbCamState.matRS2SS);
		i2PosSS = int2(f3PosSS.x, f3PosSS.y);
	}
	else
	{
		bReturnEarly = true;
		i2PosSS = int2(DTid.xy);
	}

	const uint uiSampleAddr = i2PosSS.x + i2PosSS.y*g_cbCamState.uiGridOffsetX;
#else
	PS_OUTPUT stOut;
	SET_OUTPUT((float4)0, FLT_MAX)

	const uint2 ui2PosSS = int2(input.f4PosSS.xy);

	bool bReturnEarly = false;
	int2 i2PosSS;
	if (g_cbCamState.uiScreenAaBbMinX <= ui2PosSS.x
		&& g_cbCamState.uiScreenAaBbMaxX > ui2PosSS.x
		&& g_cbCamState.uiScreenAaBbMinY <= ui2PosSS.y
		&& g_cbCamState.uiScreenAaBbMaxY > ui2PosSS.y
		)
	{
		uint2 ui2PosRS = ui2PosSS.xy - uint2(g_cbCamState.uiScreenAaBbMinX, g_cbCamState.uiScreenAaBbMinY);
		if (ui2PosRS.x >= g_cbCamState.uiRenderingSizeX || ui2PosRS.y >= g_cbCamState.uiRenderingSizeY)
			return stOut;
		float3 f3PosSS = vxTransformPoint(float3(ui2PosRS, 0), g_cbCamState.matRS2SS);
		i2PosSS = int2(f3PosSS.x, f3PosSS.y);
	}
	else
	{
		bReturnEarly = true;
		i2PosSS = int2(ui2PosSS.xy);
	}
#endif	// #ifdef __CS_MODEL


    int i, j;
    //const float fThicknessSurface = g_cbVolObj.fSampleDistWS; //g_cbVolObj.fThicknessVirtualzSlab;
    const float fThicknessSurface = g_cbVolObj.fThicknessVirtualzSlab;

#ifdef INPUT_LAYERS

	float4 f4ColorInputLayers = 0;
	float fDepthLayers1st = FLT_MAX;
	float fDepthLayersEnd = FLT_MAX;


#ifdef __CS_MODEL
	CS_LayerOut stb_PrevLayers = g_stb_PrevLayers[uiSampleAddr];

#ifdef ZSLAB_BLENDING
	VxRaySegment stCleanRS = (VxRaySegment)0;
	stCleanRS.fDepthBack = FLT_MAX;
	VxRaySegment stPrevOutRSAs[NUM_MERGE_LAYERS];
	int iNumPrevOutRSs = 0;
	stPrevOutRSAs[0] = stPrevOutRSAs[1] = stPrevOutRSAs[2] = stPrevOutRSAs[3] = stCleanRS;

	[unroll]
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

		f4ColorInputLayers += stRS.f4Visibility * (1.f - f4ColorInputLayers.a);
	}

	if (iNumPrevOutRSs > 0)
	{
		fDepthLayers1st = stPrevOutRSAs[0].fDepthBack;
		fDepthLayersEnd = stPrevOutRSAs[iNumPrevOutRSs - 1].fDepthBack;
	}
#else
	float4 f4PrevOutLayersColorRGBAs[NUM_MERGE_LAYERS];
	float fPrevOutLayersDepthCSs[NUM_MERGE_LAYERS] = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };
	int iNumPrevOuts = 0;

	[unroll]
	for (i = 0; i < NUM_MERGE_LAYERS; i++)
	{
		int iIntColor = stb_PrevLayers.iVisibilityLayers[i];
		if (iIntColor == 0)
			break;

		float4 f4Color = vxConvertColorToFloat4(iIntColor);
		f4PrevOutLayersColorRGBAs[i] = f4Color;
		fPrevOutLayersDepthCSs[i] = stb_PrevLayers.fDepthLayers[i];
		iNumPrevOuts++;

		f4ColorInputLayers += f4Color * (1.f - f4ColorInputLayers.a);
	}

	if (iNumPrevOuts > 0)
	{
		fDepthLayers1st = fPrevOutLayersDepthCSs[0];
		fDepthLayersEnd = fPrevOutLayersDepthCSs[iNumPrevOuts - 1];
	}
#endif
	
#else	// ~__CS_MODEL
	int4 i4PsRSA_RGBA = g_tex2DPrevRSA_RGBA.Load(int3(i2PosSS, 0)).rgba;
	float4 f4PsRSA_DepthCS = g_tex2DPrevRSA_DepthCS.Load(int3(i2PosSS, 0)).rgba;

#ifdef ZSLAB_BLENDING
	VxRaySegment stCleanRS = (VxRaySegment)0;
	stCleanRS.fDepthBack = FLT_MAX;
	VxRaySegment stPrevOutRSAs[NUM_MERGE_LAYERS];
	int iNumPrevOutRSs = 0;
	stPrevOutRSAs[0] = stPrevOutRSAs[1] = stPrevOutRSAs[2] = stPrevOutRSAs[3] = stCleanRS;

	if (i4PsRSA_RGBA.r != 0)
	{
		VxRaySegment stRS;
		stRS.f4Visibility = vxConvertColorToFloat4(i4PsRSA_RGBA.r);
		stRS.fDepthBack = f4PsRSA_DepthCS.r;
		stRS.fThickness = fThicknessSurface;

		f4ColorInputLayers = stRS.f4Visibility;
		fDepthLayersEnd = stRS.fDepthBack;
		fDepthLayers1st = stRS.fDepthBack;

		stPrevOutRSAs[0] = stRS;
		iNumPrevOutRSs++;
	}
	if (i4PsRSA_RGBA.g != 0)
	{
		VxRaySegment stRS;
		stRS.f4Visibility = vxConvertColorToFloat4(i4PsRSA_RGBA.g);
		stRS.fDepthBack = f4PsRSA_DepthCS.g;
		stRS.fThickness = fThicknessSurface;

		f4ColorInputLayers += stRS.f4Visibility * (1.f - f4ColorInputLayers.a);
		fDepthLayersEnd = stRS.fDepthBack;

		stPrevOutRSAs[1] = stRS;
		iNumPrevOutRSs++;
	}
	if (i4PsRSA_RGBA.b != 0)
	{
		VxRaySegment stRS;
		stRS.f4Visibility = vxConvertColorToFloat4(i4PsRSA_RGBA.b);
		stRS.fDepthBack = f4PsRSA_DepthCS.b;
		stRS.fThickness = fThicknessSurface;

		f4ColorInputLayers += stRS.f4Visibility * (1.f - f4ColorInputLayers.a);
		fDepthLayersEnd = stRS.fDepthBack;

		stPrevOutRSAs[2] = stRS;
		iNumPrevOutRSs++;
	}
	if (i4PsRSA_RGBA.a != 0)
	{
		VxRaySegment stRS;
		stRS.f4Visibility = vxConvertColorToFloat4(i4PsRSA_RGBA.a);
		stRS.fDepthBack = f4PsRSA_DepthCS.a;
		stRS.fThickness = fThicknessSurface;

		f4ColorInputLayers += stRS.f4Visibility * (1.f - f4ColorInputLayers.a);
		fDepthLayersEnd = stRS.fDepthBack;

		stPrevOutRSAs[3] = stRS;
		iNumPrevOutRSs++;
	}
#else
	float4 f4PrevOutLayersColorRGBAs[NUM_MERGE_LAYERS];
	float fPrevOutLayersDepthCSs[NUM_MERGE_LAYERS] = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };

	int iNumPrevOuts = 0;
	if (i4PsRSA_RGBA.r != 0)
	{
		f4PrevOutLayersColorRGBAs[0] = vxConvertColorToFloat4(i4PsRSA_RGBA.r);
		fPrevOutLayersDepthCSs[0] = f4PsRSA_DepthCS.r;

		f4ColorInputLayers = f4PrevOutLayersColorRGBAs[0];
		fDepthLayersEnd = f4PsRSA_DepthCS.r;
		fDepthLayers1st = fDepthLayersEnd;

		iNumPrevOuts++;
	}
	if (i4PsRSA_RGBA.g != 0)
	{
		f4PrevOutLayersColorRGBAs[1] = vxConvertColorToFloat4(i4PsRSA_RGBA.g);
		fPrevOutLayersDepthCSs[1] = f4PsRSA_DepthCS.g;

		f4ColorInputLayers += f4PrevOutLayersColorRGBAs[1] * (1.f - f4ColorInputLayers.a);
		fDepthLayersEnd = f4PsRSA_DepthCS.g;

		iNumPrevOuts++;
	}
	if (i4PsRSA_RGBA.b != 0)
	{
		f4PrevOutLayersColorRGBAs[2] = vxConvertColorToFloat4(i4PsRSA_RGBA.b);
		fPrevOutLayersDepthCSs[2] = f4PsRSA_DepthCS.b;

		f4ColorInputLayers += f4PrevOutLayersColorRGBAs[2] * (1.f - f4ColorInputLayers.a);
		fDepthLayersEnd = f4PsRSA_DepthCS.b;

		iNumPrevOuts++;
	}
	if (i4PsRSA_RGBA.a != 0)
	{
		f4PrevOutLayersColorRGBAs[3] = vxConvertColorToFloat4(i4PsRSA_RGBA.a);
		fPrevOutLayersDepthCSs[3] = f4PsRSA_DepthCS.a;

		f4ColorInputLayers += f4PrevOutLayersColorRGBAs[3] * (1.f - f4ColorInputLayers.a);
		fDepthLayersEnd = f4PsRSA_DepthCS.a;

		iNumPrevOuts++;
	}
#endif

#endif	// #ifdef __CS_MODEL
	SET_OUTPUT(f4ColorInputLayers, fDepthLayers1st)
	if (bReturnEarly) RETURN_DEFINED
#else
	if (bReturnEarly)
	{
		RETURN_DEFINED
	}
#endif

	// Image Plane's Position and Camera State //
	float3 f3PosIPSampleSS = float3(i2PosSS, 0.0f);
	float3 f3PosIPSampleWS = vxTransformPoint(f3PosIPSampleSS, g_cbCamState.matSS2WS);
	float3 f3VecSampleDirUnitWS = g_cbCamState.f3VecViewWS;
	if (g_cbCamState.iFlags & 0x1)
		f3VecSampleDirUnitWS = f3PosIPSampleWS - g_cbCamState.f3PosEyeWS;
	f3VecSampleDirUnitWS = normalize(f3VecSampleDirUnitWS);
	float3 f3VecSampleWS = f3VecSampleDirUnitWS * g_cbVolObj.fSampleDistWS;

	// Ray Intersection for Clipping Box //
	float2 f2PrevNextT = vxVrComputeBoundary(f3PosIPSampleWS, f3VecSampleDirUnitWS, g_cbVolObj);

	// Compute Ray Traversing Sample Number in the Volume Box //
#ifdef RM_CLIPSEMIOPAQUE
	VxVolumeObject cbVolObjWithoutClip = g_cbVolObj;
	cbVolObjWithoutClip.iFlags &= 0xFFFC;
	float2 f2WithoutClipPrevNextT = vxVrComputeBoundary(f3PosIPSampleWS, f3VecSampleDirUnitWS, cbVolObjWithoutClip);
	if (f2WithoutClipPrevNextT.y <= f2WithoutClipPrevNextT.x)
		RETURN_DEFINED

	int iNumSamples = (int)((f2WithoutClipPrevNextT.y - f2WithoutClipPrevNextT.x) / g_cbVolObj.fSampleDistWS + 0.5f);
	float3 f3PosBoundaryPrevWS = f3PosIPSampleWS + f3VecSampleDirUnitWS*f2WithoutClipPrevNextT.x;
#else	// ~RM_CLIPSEMIOPAQUE
	// 1st Exit in the case that there is no ray-intersecting boundary in the volume box
	if (f2PrevNextT.y <= f2PrevNextT.x)
		RETURN_DEFINED

	int iNumSamples = (int)((f2PrevNextT.y - f2PrevNextT.x) / g_cbVolObj.fSampleDistWS + 0.5f);
	float3 f3PosBoundaryPrevWS = f3PosIPSampleWS + f3VecSampleDirUnitWS*f2PrevNextT.x;
#endif

#if (defined(RM_RAYMAX) || defined(RM_RAYMIN) || defined(RM_RAYSUM))
	///////////////////////////////
	// X-Ray - casting Mode	WITHOUT INPUT_LAYERS!!
	///////////////////////////////
#ifdef INPUT_LAYERS
	NOT AVAILABLE!!! // NOT AVAILABLE!!!
#endif

	float fDepthSampleSatrt = length(f3PosBoundaryPrevWS - f3PosIPSampleWS);
	float3 f3VecSampleTS = vxTransformVector(f3VecSampleWS, g_cbVolObj.matWS2TS);
#if defined(RM_RAYMAX) || defined(RM_RAYMIN)
	int iLuckyStep = (int)((float)(vxRandom(f3PosBoundaryPrevWS.xy) + 1) * (float)iNumSamples * 0.5f);
	float fSampleDepth = fDepthSampleSatrt + g_cbVolObj.fSampleDistWS*(float)(iLuckyStep);
	float3 f3PosLuckySampleWS = f3PosBoundaryPrevWS + f3VecSampleWS * (float)iLuckyStep;
	float3 f3PosLuckySampleTS = vxTransformPoint(f3PosLuckySampleWS, g_cbVolObj.matWS2TS);
	float fValueSamplePrev = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosLuckySampleTS, 0).r * g_cbVolObj.fSampleValueRange;
#ifdef RM_RAYMIN
	fValueSamplePrev = 65535;
#endif

#if defined(OTF_MASK) || defined(RM_RAYMAX) || defined(RM_RAYMIN)
	float3 f3PosMaskSampleTS = f3PosLuckySampleTS;
#endif
		
#else	// ~(defined(RM_RAYMAX) || defined(RM_RAYMIN))
	float fSampleDepth = fDepthSampleSatrt + g_cbVolObj.fSampleDistWS*(float)(iNumSamples);
	int iNumValidSamples = 0;
	float4 f4ColorOTF_Sum = (float4)0;
#endif
	[loop]
	for (i = 0; i < iNumSamples; i++)
	{
		float3 f3PosSampleWS = f3PosBoundaryPrevWS + f3VecSampleWS*(float)i;
		float3 f3PosSampleTS = vxTransformPoint(f3PosSampleWS, g_cbVolObj.matWS2TS);

#if defined(RM_RAYMAX) || defined(RM_RAYMIN)
		VxBlockSkip blkSkip = vxComputeBlockRayLengthBoundary(f3PosSampleTS, f3VecSampleTS, g_cbBlkObj, g_tex3DBlock);
		blkSkip.iNumStepSkip = min(max(1, blkSkip.iNumStepSkip), iNumSamples - i);
#if defined(RM_RAYMAX)
		if (blkSkip.iSampleBlockValue >(int)fValueSamplePrev)
#elif defined(RM_RAYMIN)
		if (blkSkip.iSampleBlockValue < (int)fValueSamplePrev)
#endif
		{
			for (int k = 0; k < blkSkip.iNumStepSkip; k++, i++)
			{
				float3 f3PosSampleInBlockWS = f3PosBoundaryPrevWS + f3VecSampleWS*(float)i;
				float3 f3PosSampleInBlockTS = vxTransformPoint(f3PosSampleInBlockWS, g_cbVolObj.matWS2TS);
				float fSampleValue = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleInBlockTS, 0).r * g_cbVolObj.fSampleValueRange;
#ifdef RM_RAYMAX

				if (fSampleValue > fValueSamplePrev)
#else	// ~RM_RAYMAX
				if (fSampleValue < fValueSamplePrev)
#endif
				{
#ifdef OTF_MASK
					f3PosMaskSampleTS = f3PosSampleInBlockTS;
#endif
					fValueSamplePrev = fSampleValue;
					fSampleDepth = fDepthSampleSatrt + g_cbVolObj.fSampleDistWS*(float)i;
				}
			}
		}
		else
		{
			i += blkSkip.iNumStepSkip;
		}
		// this is for outer loop's i++
		i -= 1;
#else	// ~(defined(RM_RAYMAX) || defined(RM_RAYMIN)) , which means RAYSUM 
		float fSampleValue = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS, 0).r * g_cbVolObj.fSampleValueRange;
#ifdef OTF_MASK
		float2 f2SampleMaskValue = vxSampleByCCF(f3PosSampleTS, g_cbVolObj.f3SizeVolumeCVS, g_tex3DVolumeMask);
		int iMaskValue = (int)(f2SampleMaskValue.x * 255 + 0.5f);
		int iMaskID = g_ibufOTFIndex[iMaskValue];
		float4 f4ColorOTF_Origin = vxLoadPointOtf1DFromBuffer((int)fSampleValue, g_f4bufOTF, 1.f);	// MUST BE 1.f instead of g_cbVolObj.fOpaqueCorrection
		float4 f4ColorOTF_Mask = vxLoadPointOtf1DSeriesFromBuffer((int)fSampleValue, iMaskID, g_f4bufOTF, 1.f);
		float4 f4ColorOTF = f4ColorOTF_Origin * (1.f - f2SampleMaskValue.y) + f4ColorOTF_Mask * f2SampleMaskValue.y;
#else	// ~OTF_MASK
		float4 f4ColorOTF = vxLoadPointOtf1DFromBuffer((int)fSampleValue, g_f4bufOTF, 1.f);
#endif
		if (f4ColorOTF.a > 0)
		{
			f4ColorOTF_Sum += f4ColorOTF;
			iNumValidSamples++;
		}
#endif
	}

#ifdef RM_RAYSUM
	if (iNumValidSamples == 0)
		iNumValidSamples = 1;
	float4 f4ColorOTF = f4ColorOTF_Sum / (float)iNumValidSamples;
#else // ~RM_RAYSUM

#ifdef OTF_MASK
	float2 f2SampleMaskValue = vxSampleByCCF(f3PosMaskSampleTS, g_cbVolObj.f3SizeVolumeCVS, g_tex3DVolumeMask);
	int iMaskValue = (int)(f2SampleMaskValue.x * 255 + 0.5f);
	int iMaskID = g_ibufOTFIndex[iMaskValue];
	float4 f4ColorOTF_Origin = vxLoadPointOtf1DFromBuffer((int)fValueSamplePrev, g_f4bufOTF, 1.f);
	float4 f4ColorOTF_Mask = vxLoadPointOtf1DSeriesFromBuffer((int)fValueSamplePrev, iMaskID, g_f4bufOTF, 1.f);
	float4 f4ColorOTF = f4ColorOTF_Origin * (1.f - f2SampleMaskValue.y) + f4ColorOTF_Mask * f2SampleMaskValue.y;
#else
	float4 f4ColorOTF = vxLoadPointOtf1DFromBuffer((int)fValueSamplePrev, g_f4bufOTF, 1.f);
#endif

#ifdef RM_RAYMIN
#endif
#endif

	if (f4ColorOTF.a == 0)
		RETURN_DEFINED

	SET_OUTPUT(f4ColorOTF, fSampleDepth)
	RETURN_DEFINED

#else
	///////////////////////////////
	// VR Ray-casting Mode 
	///////////////////////////////
	
	// Surface Refinement //
	VxSurfaceRefinement vxSampleStartRefinement;

//#define BLOCKSHOW
//#define __DOJO_SURF_CINE_EFFECT

#if defined(BLOCKSHOW)
#ifdef INPUT_LAYERS
	NOT AVAILABLE!!! // NOT AVAILABLE!!!
#endif

#if defined(__DOJO_SURF_TEST)
	VxSurfaceRefinement_Test vxSampleStartRefinement_test = vxVrSurfaceRefinementWithBlockSkipping_TEST_1(f3PosBoundaryPrevWS, f3VecSampleDirUnitWS, iNumSamples,
		(int)g_cbSurfEffect.fDummy0, (int)g_cbSurfEffect.fDummy1, g_cbSurfEffect.iSizeCurvatureKernel, g_cbSurfEffect.fPhongExpWeightU, g_cbSurfEffect.fPhongExpWeightV,
		g_cbVolObj, g_tex3DVolume, g_cbBlkObj, g_tex3DBlock, g_cbOtf1D, g_f4bufOTF);
	
	if (vxSampleStartRefinement_test.iHitStatus == -1)
		RETURN_DEFINED
	
	float f1stHitDepthBack = length(vxSampleStartRefinement_test.f3PosSurfaceWS - f3PosIPSampleWS);
	float f1stHitDepthFront = f1stHitDepthBack - fThicknessSurface;
	
	VxVolumeObject cbVolObj = g_cbVolObj;
	VxSurfaceEffect cbSurfEffect = g_cbSurfEffect;
	if (vxSampleStartRefinement_test.iHitStatus == 0)
	{
		cbVolObj.f4ShadingFactor.x = 1;
		cbVolObj.f4ShadingFactor.y = cbVolObj.f4ShadingFactor.z = 0;
		cbSurfEffect.iFlagsForIsoSurfaceRendering = 0;
	}
	

	//
	//float4 f4Color = float4(vxSampleStartRefinement_test.f3Color, 1);
	//float4 f4Color = vxComputeOpaqueSurfaceCurvature_TEST(vxSampleStartRefinement_test.f3PosSurfaceWS, f3VecSampleWS, cbSurfEffect, g_cbCamState, cbVolObj,
	//	g_cbOtf1D, g_tex3DVolume, g_f4bufOTF);
	
	//cbVolObj.f3VecGradientSampleX *= 2;
	//cbVolObj.f3VecGradientSampleY *= 2;
	//cbVolObj.f3VecGradientSampleZ *= 2;

	float4 f4Color = vxComputeOpaqueSurfaceEffect_Test(vxSampleStartRefinement_test.f3PosSurfaceWS, 
		vxSampleStartRefinement_test.f3VecNormalWS, f3VecSampleWS, cbSurfEffect, (int)g_cbSurfEffect.fDummy1, g_cbCamState, cbVolObj,
		g_cbOtf1D, g_tex3DVolume, g_f4bufOTF);
	
	if (g_cbSurfEffect.fPhongBRDF_DiffuseRatio == 2.f || g_cbSurfEffect.fPhongBRDF_DiffuseRatio == 3.f)
	{
		float3 f3PosSampleTS = vxTransformPoint(vxSampleStartRefinement_test.f3PosSurfaceWS, cbVolObj.matWS2TS);

		const float __fSampleFactor = 1.f;// (float)g_cbSurfEffect.iSizeCurvatureKernel;
		const float3 __f3VecGradientSampleX = cbVolObj.f3VecGradientSampleX * __fSampleFactor;
		const float3 __f3VecGradientSampleY = cbVolObj.f3VecGradientSampleY * __fSampleFactor;
		const float3 __f3VecGradientSampleZ = cbVolObj.f3VecGradientSampleZ * __fSampleFactor;

		float fSampleValue = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS, 0).r;
		float fSampleValueXR = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + __f3VecGradientSampleX, 0).r;
		float fSampleValueXL = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - __f3VecGradientSampleX, 0).r;
		float fSampleValueYR = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + __f3VecGradientSampleY, 0).r;
		float fSampleValueYL = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - __f3VecGradientSampleY, 0).r;
		float fSampleValueZR = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + __f3VecGradientSampleZ, 0).r;
		float fSampleValueZL = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - __f3VecGradientSampleZ, 0).r;
		float fSampleValueXRYR = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + __f3VecGradientSampleX + __f3VecGradientSampleY, 0).r;
		float fSampleValueXRYL = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + __f3VecGradientSampleX - __f3VecGradientSampleY, 0).r;
		float fSampleValueXLYR = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - __f3VecGradientSampleX + __f3VecGradientSampleY, 0).r;
		float fSampleValueXLYL = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - __f3VecGradientSampleX - __f3VecGradientSampleY, 0).r;
		float fSampleValueYRZR = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + __f3VecGradientSampleY + __f3VecGradientSampleZ, 0).r;
		float fSampleValueYRZL = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + __f3VecGradientSampleY - __f3VecGradientSampleZ, 0).r;
		float fSampleValueYLZR = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - __f3VecGradientSampleY + __f3VecGradientSampleZ, 0).r;
		float fSampleValueYLZL = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - __f3VecGradientSampleY - __f3VecGradientSampleZ, 0).r;
		float fSampleValueXRZR = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + __f3VecGradientSampleX + __f3VecGradientSampleZ, 0).r;
		float fSampleValueXRZL = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + __f3VecGradientSampleX - __f3VecGradientSampleZ, 0).r;
		float fSampleValueXLZR = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - __f3VecGradientSampleX + __f3VecGradientSampleZ, 0).r;
		float fSampleValueXLZL = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - __f3VecGradientSampleX - __f3VecGradientSampleZ, 0).r;

#define __OPACITY(A) A >= __fMaxIsoValue? 1.0 : 0
#define INDEXSAMPLE2(a) (int)(a * cbVolObj.fSampleValueRange + 0.5f)

		//float fOpacity = __OPACITY(fSampleValue);
		//float fOpacityXR = __OPACITY(fSampleValueXR);
		//float fOpacityXL = __OPACITY(fSampleValueXL);
		//float fOpacityYR = __OPACITY(fSampleValueYR);
		//float fOpacityYL = __OPACITY(fSampleValueYL);
		//float fOpacityZR = __OPACITY(fSampleValueZR);
		//float fOpacityZL = __OPACITY(fSampleValueZL);
		//float fOpacityXRYR = __OPACITY(fSampleValueXRYR);
		//float fOpacityXRYL = __OPACITY(fSampleValueXRYL);
		//float fOpacityXLYR = __OPACITY(fSampleValueXLYR);
		//float fOpacityXLYL = __OPACITY(fSampleValueXLYL);
		//float fOpacityYRZR = __OPACITY(fSampleValueYRZR);
		//float fOpacityYRZL = __OPACITY(fSampleValueYRZL);
		//float fOpacityYLZR = __OPACITY(fSampleValueYLZR);
		//float fOpacityYLZL = __OPACITY(fSampleValueYLZL);
		//float fOpacityXRZR = __OPACITY(fSampleValueXRZR);
		//float fOpacityXRZL = __OPACITY(fSampleValueXRZL);
		//float fOpacityXLZR = __OPACITY(fSampleValueXLZR);
		//float fOpacityXLZL = __OPACITY(fSampleValueXLZL);

		//float fOpacity = g_f4bufOTF[INDEXSAMPLE2(fSampleValue)].w;
		//float fOpacityXR = g_f4bufOTF[INDEXSAMPLE2(fSampleValueXR)].w;
		//float fOpacityXL = g_f4bufOTF[INDEXSAMPLE2(fSampleValueXL)].w;
		//float fOpacityYR = g_f4bufOTF[INDEXSAMPLE2(fSampleValueYR)].w;
		//float fOpacityYL = g_f4bufOTF[INDEXSAMPLE2(fSampleValueYL)].w;
		//float fOpacityZR = g_f4bufOTF[INDEXSAMPLE2(fSampleValueZR)].w;
		//float fOpacityZL = g_f4bufOTF[INDEXSAMPLE2(fSampleValueZL)].w;
		//float fOpacityXRYR = g_f4bufOTF[INDEXSAMPLE2(fSampleValueXRYR)].w;
		//float fOpacityXRYL = g_f4bufOTF[INDEXSAMPLE2(fSampleValueXRYL)].w;
		//float fOpacityXLYR = g_f4bufOTF[INDEXSAMPLE2(fSampleValueXLYR)].w;
		//float fOpacityXLYL = g_f4bufOTF[INDEXSAMPLE2(fSampleValueXLYL)].w;
		//float fOpacityYRZR = g_f4bufOTF[INDEXSAMPLE2(fSampleValueYRZR)].w;
		//float fOpacityYRZL = g_f4bufOTF[INDEXSAMPLE2(fSampleValueYRZL)].w;
		//float fOpacityYLZR = g_f4bufOTF[INDEXSAMPLE2(fSampleValueYLZR)].w;
		//float fOpacityYLZL = g_f4bufOTF[INDEXSAMPLE2(fSampleValueYLZL)].w;
		//float fOpacityXRZR = g_f4bufOTF[INDEXSAMPLE2(fSampleValueXRZR)].w;
		//float fOpacityXRZL = g_f4bufOTF[INDEXSAMPLE2(fSampleValueXRZL)].w;
		//float fOpacityXLZR = g_f4bufOTF[INDEXSAMPLE2(fSampleValueXLZR)].w;
		//float fOpacityXLZL = g_f4bufOTF[INDEXSAMPLE2(fSampleValueXLZL)].w;

		float4x4 H = 0;

		//float3 f3VecGrad_Curv = float3(fOpacityXR - fOpacityXL, fOpacityYR - fOpacityYL, fOpacityZR - fOpacityZL) / 2.0f;
		//H._11 = fOpacityXR - 2 * fOpacity + fOpacityXL;							//f_xx
		//H._12 = (fOpacityXRYR - fOpacityXLYR - fOpacityXRYL + fOpacityXLYL) / 4;	//f_xy
		//H._13 = (fOpacityXRZR - fOpacityXLZR - fOpacityXRZL + fOpacityXLZL) / 4;	//f_xz
		//H._21 = H._12;
		//H._22 = fOpacityYR - 2 * fOpacity + fOpacityYL;						//f_yy
		//H._23 = (fOpacityYRZR - fOpacityYLZR - fOpacityYRZL + fOpacityYLZL) / 4;	//f_yz
		//H._31 = H._13;
		//H._32 = H._23;
		//H._33 = fOpacityZR - 2 * fOpacity + fOpacityZL;							//f_zz
		//H._44 = 1;

		float3 f3VecGrad_Curv = float3(fSampleValueXR - fSampleValueXL, fSampleValueYR - fSampleValueYL, fSampleValueZR - fSampleValueZL) / 2.0f;
		H._11 = fSampleValueXR - 2 * fSampleValue + fSampleValueXL;								//f_xx
		H._12 = (fSampleValueXRYR - fSampleValueXLYR - fSampleValueXRYL + fSampleValueXLYL) / 4.f;	//f_xy
		H._13 = (fSampleValueXRZR - fSampleValueXLZR - fSampleValueXRZL + fSampleValueXLZL) / 4.f;	//f_xz
		H._21 = H._12;
		H._22 = fSampleValueYR - 2 * fSampleValue + fSampleValueYL;								//f_yy
		H._23 = (fSampleValueYRZR - fSampleValueYLZR - fSampleValueYRZL + fSampleValueYLZL) / 4.f;	//f_yz
		H._31 = H._13;
		H._32 = H._23;
		H._33 = fSampleValueZR - 2 * fSampleValue + fSampleValueZL;								//f_zz
		H._44 = 1;
		//
		float fGradCurvLength = length(f3VecGrad_Curv) + 0.000001f;
		float3 n = -f3VecGrad_Curv / fGradCurvLength;
		float4x4 nnT = 0, P = 0;
		nnT._11 = n.x*n.x;
		nnT._12 = n.x*n.y;
		nnT._13 = n.x*n.z;
		nnT._21 = n.y*n.x;
		nnT._22 = n.y*n.y;
		nnT._23 = n.y*n.z;
		nnT._31 = n.z*n.x;
		nnT._32 = n.z*n.y;
		nnT._33 = n.z*n.z;
		nnT._44 = 0;
		P._11 = 1.0f;
		P._22 = 1.0f;
		P._33 = 1.0f;
		P._44 = 1.0f;
		P = P - nnT;
		//
		float4x4 FF = mul((mul(-P, H) / fGradCurvLength), nnT);
		float fFlowCurv = sqrt(FF._11 * FF._11 + FF._12 * FF._12 + FF._13 * FF._13
			+ FF._21 * FF._21 + FF._22 * FF._22 + FF._23 * FF._23
			+ FF._31 * FF._31 + FF._32 * FF._32 + FF._33 * FF._33);
		//

		float4x4 G = mul(mul(-P, H), P) / fGradCurvLength;
		float T = G._11 + G._22 + G._33; // trace of G
		float F = sqrt(G._11 * G._11 + G._12 * G._12 + G._13 * G._13
			+ G._21 * G._21 + G._22 * G._22 + G._23 * G._23
			+ G._31 * G._31 + G._32 * G._32 + G._33 * G._33);

		float k1 = (T + sqrt(2 * F*F - T*T)) / 2;
		float k2 = (T - sqrt(2 * F*F - T*T)) / 2;

		float r = sqrt(k1*k1 + k2*k2);
		const float PI = 3.1415926535f;
		float theta = PI / 4 - atan2(k2, k1);

		float3 f3CurvatureColor = float3(max(-r*cos(theta), 0.0f), max(r*sin(theta), 0.0f), max(r*cos(theta), 0.0f));
		//r = min(r, 1.f);

		//vxOut.f4CurvatureInfo = float4(k1, k2, r, fFlowCurv);

		float fr = min(r, 1.f);
		//float fr = min(r, fFlowCurv);

		f4Color.x = f3CurvatureColor.x * r + f4Color.x * (1.f - r);
		f4Color.y = f3CurvatureColor.y * r + f4Color.y * (1.f - r);
		f4Color.z = f3CurvatureColor.z * r + f4Color.z * (1.f - r);
	}

	if (g_cbSurfEffect.fPhongBRDF_DiffuseRatio == 1.f || g_cbSurfEffect.fPhongBRDF_DiffuseRatio == 3.f)
	{
		if(vxSampleStartRefinement_test.iOutType == 1)
			f4Color.rgb = float3(0, 0, 1);
		else if(vxSampleStartRefinement_test.iOutType == 2)
			f4Color.rgb = float3(0, 1, 0);
		else if(vxSampleStartRefinement_test.iOutType == 3)
			f4Color.rgb = float3(1, 0, 0);
		else if (vxSampleStartRefinement_test.iOutType == 4)
			f4Color.rgb = float3(1, 1, 0); // yellow
		else if (vxSampleStartRefinement_test.iOutType == 5)
			f4Color.rgb = float3(0, 1, 1); // sky
	}

#ifdef SAMPLER_VOLUME_CCF
	if (f4Color.a == 0)
		RETURN_DEFINED
#endif
	f4Color.a = 1.f;

	SET_OUTPUT(f4Color, f1stHitDepthBack)
	RETURN_DEFINED

#elif defined(__DOJO_SURF_CURVATURE)

	//VxSurfaceRefinement_Test vxSampleStartRefinement_test = vxVrSurfaceRefinementWithBlockSkipping_TEST_1(f3PosBoundaryPrevWS, f3VecSampleDirUnitWS, iNumSamples,
	//	(int)g_cbSurfEffect.fDummy0, (int)g_cbSurfEffect.fDummy1, g_cbSurfEffect.iSizeCurvatureKernel, g_cbSurfEffect.fPhongExpWeightU, g_cbSurfEffect.fPhongExpWeightV,
	//	g_cbVolObj, g_tex3DVolume, g_cbBlkObj, g_tex3DBlock, g_cbOtf1D, g_f4bufOTF);

	vxSampleStartRefinement = vxVrMaskSurfaceRefinementWithBlockSkipping(f3PosBoundaryPrevWS, f3VecSampleDirUnitWS, iNumSamples,
		g_cbVolObj, g_tex3DVolume, g_tex3DVolumeMask, g_cbBlkObj, g_tex3DBlock, g_cbOtf1D);

	if (vxSampleStartRefinement.iHitSampleStep == -1)
		RETURN_DEFINED

	VxVolumeObject cbVolObj = g_cbVolObj;
	float3 f3PosSampleTS = vxTransformPoint(vxSampleStartRefinement.f3PosSurfaceWS, cbVolObj.matWS2TS);

	const float __fSampleFactor = (float)g_cbSurfEffect.iSizeCurvatureKernel;
	const float3 __f3VecGradientSampleX = cbVolObj.f3VecGradientSampleX * __fSampleFactor;
	const float3 __f3VecGradientSampleY = cbVolObj.f3VecGradientSampleY * __fSampleFactor;
	const float3 __f3VecGradientSampleZ = cbVolObj.f3VecGradientSampleZ * __fSampleFactor;

	float fSampleValue = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS, 0).r;
	float fSampleValueXXR = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + 2 * __f3VecGradientSampleX, 0).r;
	float fSampleValueXXL = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - 2 * __f3VecGradientSampleX, 0).r;
	float fSampleValueYYR = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + 2 * __f3VecGradientSampleY, 0).r;
	float fSampleValueYYL = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - 2 * __f3VecGradientSampleY, 0).r;
	float fSampleValueZZR = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + 2 * __f3VecGradientSampleZ, 0).r;
	float fSampleValueZZL = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - 2 * __f3VecGradientSampleZ, 0).r;
	float fSampleValueXR = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + __f3VecGradientSampleX, 0).r;
	float fSampleValueXL = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - __f3VecGradientSampleX, 0).r;
	float fSampleValueYR = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + __f3VecGradientSampleY, 0).r;
	float fSampleValueYL = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - __f3VecGradientSampleY, 0).r;
	float fSampleValueZR = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + __f3VecGradientSampleZ, 0).r;
	float fSampleValueZL = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - __f3VecGradientSampleZ, 0).r;
	float fSampleValueXRYR = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + __f3VecGradientSampleX + __f3VecGradientSampleY, 0).r;
	float fSampleValueXRYL = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + __f3VecGradientSampleX - __f3VecGradientSampleY, 0).r;
	float fSampleValueXLYR = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - __f3VecGradientSampleX + __f3VecGradientSampleY, 0).r;
	float fSampleValueXLYL = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - __f3VecGradientSampleX - __f3VecGradientSampleY, 0).r;
	float fSampleValueYRZR = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + __f3VecGradientSampleY + __f3VecGradientSampleZ, 0).r;
	float fSampleValueYRZL = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + __f3VecGradientSampleY - __f3VecGradientSampleZ, 0).r;
	float fSampleValueYLZR = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - __f3VecGradientSampleY + __f3VecGradientSampleZ, 0).r;
	float fSampleValueYLZL = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - __f3VecGradientSampleY - __f3VecGradientSampleZ, 0).r;
	float fSampleValueXRZR = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + __f3VecGradientSampleX + __f3VecGradientSampleZ, 0).r;
	float fSampleValueXRZL = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + __f3VecGradientSampleX - __f3VecGradientSampleZ, 0).r;
	float fSampleValueXLZR = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - __f3VecGradientSampleX + __f3VecGradientSampleZ, 0).r;
	float fSampleValueXLZL = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - __f3VecGradientSampleX - __f3VecGradientSampleZ, 0).r;

#define __OPACITY(A) A >= __fMaxIsoValue? 1.0 : 0
#define INDEXSAMPLE2(a) (int)(a * cbVolObj.fSampleValueRange + 0.5f)

	//float fOpacity = __OPACITY(fSampleValue);
	//float fOpacityXR = __OPACITY(fSampleValueXR);
	//float fOpacityXL = __OPACITY(fSampleValueXL);
	//float fOpacityYR = __OPACITY(fSampleValueYR);
	//float fOpacityYL = __OPACITY(fSampleValueYL);
	//float fOpacityZR = __OPACITY(fSampleValueZR);
	//float fOpacityZL = __OPACITY(fSampleValueZL);
	//float fOpacityXRYR = __OPACITY(fSampleValueXRYR);
	//float fOpacityXRYL = __OPACITY(fSampleValueXRYL);
	//float fOpacityXLYR = __OPACITY(fSampleValueXLYR);
	//float fOpacityXLYL = __OPACITY(fSampleValueXLYL);
	//float fOpacityYRZR = __OPACITY(fSampleValueYRZR);
	//float fOpacityYRZL = __OPACITY(fSampleValueYRZL);
	//float fOpacityYLZR = __OPACITY(fSampleValueYLZR);
	//float fOpacityYLZL = __OPACITY(fSampleValueYLZL);
	//float fOpacityXRZR = __OPACITY(fSampleValueXRZR);
	//float fOpacityXRZL = __OPACITY(fSampleValueXRZL);
	//float fOpacityXLZR = __OPACITY(fSampleValueXLZR);
	//float fOpacityXLZL = __OPACITY(fSampleValueXLZL);

	//float fOpacity = g_f4bufOTF[INDEXSAMPLE2(fSampleValue)].w;
	//float fOpacityXR = g_f4bufOTF[INDEXSAMPLE2(fSampleValueXR)].w;
	//float fOpacityXL = g_f4bufOTF[INDEXSAMPLE2(fSampleValueXL)].w;
	//float fOpacityYR = g_f4bufOTF[INDEXSAMPLE2(fSampleValueYR)].w;
	//float fOpacityYL = g_f4bufOTF[INDEXSAMPLE2(fSampleValueYL)].w;
	//float fOpacityZR = g_f4bufOTF[INDEXSAMPLE2(fSampleValueZR)].w;
	//float fOpacityZL = g_f4bufOTF[INDEXSAMPLE2(fSampleValueZL)].w;
	//float fOpacityXRYR = g_f4bufOTF[INDEXSAMPLE2(fSampleValueXRYR)].w;
	//float fOpacityXRYL = g_f4bufOTF[INDEXSAMPLE2(fSampleValueXRYL)].w;
	//float fOpacityXLYR = g_f4bufOTF[INDEXSAMPLE2(fSampleValueXLYR)].w;
	//float fOpacityXLYL = g_f4bufOTF[INDEXSAMPLE2(fSampleValueXLYL)].w;
	//float fOpacityYRZR = g_f4bufOTF[INDEXSAMPLE2(fSampleValueYRZR)].w;
	//float fOpacityYRZL = g_f4bufOTF[INDEXSAMPLE2(fSampleValueYRZL)].w;
	//float fOpacityYLZR = g_f4bufOTF[INDEXSAMPLE2(fSampleValueYLZR)].w;
	//float fOpacityYLZL = g_f4bufOTF[INDEXSAMPLE2(fSampleValueYLZL)].w;
	//float fOpacityXRZR = g_f4bufOTF[INDEXSAMPLE2(fSampleValueXRZR)].w;
	//float fOpacityXRZL = g_f4bufOTF[INDEXSAMPLE2(fSampleValueXRZL)].w;
	//float fOpacityXLZR = g_f4bufOTF[INDEXSAMPLE2(fSampleValueXLZR)].w;
	//float fOpacityXLZL = g_f4bufOTF[INDEXSAMPLE2(fSampleValueXLZL)].w;

	float4x4 H = 0;

	//float3 f3VecGrad_Curv = float3(fOpacityXR - fOpacityXL, fOpacityYR - fOpacityYL, fOpacityZR - fOpacityZL) / 2.0f;
	//H._11 = fOpacityXR - 2 * fOpacity + fOpacityXL;							//f_xx
	//H._12 = (fOpacityXRYR - fOpacityXLYR - fOpacityXRYL + fOpacityXLYL) / 4;	//f_xy
	//H._13 = (fOpacityXRZR - fOpacityXLZR - fOpacityXRZL + fOpacityXLZL) / 4;	//f_xz
	//H._21 = H._12;
	//H._22 = fOpacityYR - 2 * fOpacity + fOpacityYL;						//f_yy
	//H._23 = (fOpacityYRZR - fOpacityYLZR - fOpacityYRZL + fOpacityYLZL) / 4;	//f_yz
	//H._31 = H._13;
	//H._32 = H._23;
	//H._33 = fOpacityZR - 2 * fOpacity + fOpacityZL;							//f_zz
	//H._44 = 1;

	float3 f3VecGrad_Curv = float3(fSampleValueXR - fSampleValueXL, fSampleValueYR - fSampleValueYL, fSampleValueZR - fSampleValueZL);
	float fGradCurvLength = length(f3VecGrad_Curv);
	float4 f4Color = vxComputeOpaqueSurfaceEffect(vxSampleStartRefinement.f3PosSurfaceWS, f3VecSampleWS, g_cbSurfEffect, g_cbCamState, cbVolObj,
		g_cbOtf1D, g_tex3DVolume, g_f4bufOTF);
	if(fGradCurvLength > 0.00001f)
	{
		H._11 = fSampleValueXXR - 2.f * fSampleValue + fSampleValueXXL;							//f_xx
		H._12 = (fSampleValueXRYR - fSampleValueXLYR - fSampleValueXRYL + fSampleValueXLYL);// / 4.f;	//f_xy
		H._13 = (fSampleValueXRZR - fSampleValueXLZR - fSampleValueXRZL + fSampleValueXLZL);// / 4.f;	//f_xz
		H._21 = H._12;
		H._22 = fSampleValueYYR - 2.f * fSampleValue + fSampleValueYYL;							//f_yy
		H._23 = (fSampleValueYRZR - fSampleValueYLZR - fSampleValueYRZL + fSampleValueYLZL);// / 4.f;	//f_yz
		H._31 = H._13;
		H._32 = H._23;
		H._33 = fSampleValueZZR - 2.f * fSampleValue + fSampleValueZZL;							//f_zz
		H._44 = 1;
		//
		float3 n = -f3VecGrad_Curv / fGradCurvLength;
		float4x4 nnT = 0, P = 0;
		nnT._11 = n.x*n.x;
		nnT._12 = n.x*n.y;
		nnT._13 = n.x*n.z;
		nnT._21 = n.y*n.x;
		nnT._22 = n.y*n.y;
		nnT._23 = n.y*n.z;
		nnT._31 = n.z*n.x;
		nnT._32 = n.z*n.y;
		nnT._33 = n.z*n.z;
		nnT._44 = 0;
		P._11 = 1.0f;
		P._22 = 1.0f;
		P._33 = 1.0f;
		P._44 = 1.0f;
		P = P - nnT;
		//
		float4x4 FF = mul((mul(-P, H) / fGradCurvLength), nnT);
		//float4x4 FF = mul(-P, H) / fGradCurvLength;
		float fFlowCurv = //sqrt(FF._13 * FF._13 + FF._23 * FF._23);
			sqrt(FF._11 * FF._11 + FF._12 * FF._12 + FF._13 * FF._13
			+ FF._21 * FF._21 + FF._22 * FF._22 + FF._23 * FF._23
			+ FF._31 * FF._31 + FF._32 * FF._32 + FF._33 * FF._33);
		//

		float4x4 G = mul(mul(-P, H), P) / fGradCurvLength;
		float T = G._11 + G._22 + G._33; // trace of G
		float F = sqrt(G._11 * G._11 + G._12 * G._12 + G._13 * G._13
			+ G._21 * G._21 + G._22 * G._22 + G._23 * G._23
			+ G._31 * G._31 + G._32 * G._32 + G._33 * G._33);

		float k1 = (T + sqrt(2 * F*F - T*T)) / 2;
		float k2 = (T - sqrt(2 * F*F - T*T)) / 2;

		float r = sqrt(k1*k1 + k2*k2);
		const float PI = 3.1415926535f;

		float theta = PI / 4.f + atan2(k2, k1);

		float3 f3CurvatureColor = float3(max(-r*sin(theta), 0.0f), max(r*cos(theta), 0.0f), max(r*sin(theta), 0.0f));
		//float3 f3CurvatureColor = float3(1.f, 0, 0.0f);
		//r = min(r, 1.f);

		//vxOut.f4CurvatureInfo = float4(k1, k2, r, fFlowCurv);

		//float fr = min(r, 1.f);
		//float fr = min(max(fFlowCurv, r), 1.f);
		float fr = min(fFlowCurv, 1.f);
		//if(fFlowCurv > r)
		//	f3CurvatureColor = float3(0, 1.f, 0.0f);
		//if(fr > 0.3f)
		//	fr = 1;
		//else
		//	fr = 0;
		f4Color.x = f3CurvatureColor.x * fr + f4Color.x * (1.f - fr);
		f4Color.y = f3CurvatureColor.y * fr + f4Color.y * (1.f - fr);
		f4Color.z = f3CurvatureColor.z * fr + f4Color.z * (1.f - fr);
		f4Color.a = 1.f;
	}

	float f1stHitDepthBack = length(vxSampleStartRefinement.f3PosSurfaceWS - f3PosIPSampleWS);
	SET_OUTPUT(f4Color, f1stHitDepthBack)
	RETURN_DEFINED
    
#elif defined(__DOJO_SURF_CINE_EFFECT)

	vxSampleStartRefinement = vxVrBlockPointSurfaceWithBlockSkipping(f3PosBoundaryPrevWS, f3VecSampleDirUnitWS, iNumSamples,
		g_cbVolObj, g_cbBlkObj, g_tex3DBlock);
    
	//SET_OUTPUT(float4(1, 0, 0, 1), FLT_MAX)
	//RETURN_DEFINED
	
	if(vxSampleStartRefinement.iHitSampleStep == -1)
		RETURN_DEFINED

	// Block Vis 의 경우 1st slab 의 front 가 vxSampleStartRefinement.f3PosSurfaceWS 으로 나옴
	float f1stHitDepthBack = length(vxSampleStartRefinement.f3PosSurfaceWS - f3PosIPSampleWS);
	float4 f4ColorBox = float4(vxTransformPoint(vxSampleStartRefinement.f3PosSurfaceWS, g_cbVolObj.matWS2TS), 1);
	
	SET_OUTPUT(f4ColorBox, f1stHitDepthBack)
	RETURN_DEFINED

#else // ~__DOJO_SURF_TEST
	vxSampleStartRefinement = vxVrBlockPointSurfaceWithBlockSkipping(f3PosBoundaryPrevWS, f3VecSampleDirUnitWS, iNumSamples,
		g_cbVolObj, g_cbBlkObj, g_tex3DBlock);
	
	if(vxSampleStartRefinement.iHitSampleStep == -1)
		RETURN_DEFINED

	// Block Vis 의 경우 1st slab 의 front 가 vxSampleStartRefinement.f3PosSurfaceWS 으로 나옴
	float f1stHitDepthBack = length(vxSampleStartRefinement.f3PosSurfaceWS - f3PosIPSampleWS);
	float4 f4ColorBox = float4(vxTransformPoint(vxSampleStartRefinement.f3PosSurfaceWS, g_cbVolObj.matWS2TS), 1);
	
	SET_OUTPUT(f4ColorBox, f1stHitDepthBack)
	RETURN_DEFINED
#endif
#else // ~defined(BLOCKSHOW)

#ifdef SCULPTMASK
#ifdef OTF_MASK
	NOT AVAILABLE!!! // NOT AVAILABLE!!!
#endif 
	vxSampleStartRefinement = vxVrMaskSurfaceRefinementWithBlockSkipping(f3PosBoundaryPrevWS, f3VecSampleDirUnitWS, iNumSamples,
		g_cbVolObj, g_tex3DVolume, g_tex3DVolumeMask, g_cbBlkObj, g_tex3DBlock, g_cbOtf1D);
#else // ~SCULPTMASK
#ifdef OTF_MASK
	vxSampleStartRefinement = vxVrSlabSurfaceRefinementWithBlockSkipping(f3PosBoundaryPrevWS, f3VecSampleDirUnitWS, iNumSamples,
		g_cbVolObj, g_tex3DVolume, g_cbBlkObj, g_tex3DBlock, g_cbOtf1D, g_f4bufOTF, g_ibufOTFIndex, g_tex3DVolumeMask);
#else // ~OTF_MASK
	vxSampleStartRefinement = vxVrSlabSurfaceRefinementWithBlockSkipping(f3PosBoundaryPrevWS, f3VecSampleDirUnitWS, iNumSamples,
		g_cbVolObj, g_tex3DVolume, g_cbBlkObj, g_tex3DBlock, g_cbOtf1D);
#endif
#endif

	// There is no hit surface in the volume
	if (vxSampleStartRefinement.iHitSampleStep == -1)
		RETURN_DEFINED

	float f1stHitDepthBack = length(vxSampleStartRefinement.f3PosSurfaceWS - f3PosIPSampleWS);
	float f1stHitDepthFront = f1stHitDepthBack - fThicknessSurface;

    
    //// TEST //
	//SET_OUTPUT(float4(1, 0, 0, 1), f1stHitDepthBack)
	//RETURN_DEFINED
    ////////////

#ifdef INPUT_LAYERS
	// 3rd Exit in the case of the Surface is on the front of the ray's 1st hit
	// CONSIDER THAT THE VIRTUAL ZSLAB's ADDITIONAL OVERWRAPPING INTERVAL for OUR SLAB INTERMIXING.
	if (fDepthLayersEnd <= f1stHitDepthFront && f4ColorInputLayers.a > ERT_ALPHA)
		RETURN_DEFINED

	int iIndexOfPrevOutRSA = 0;
#ifdef ZSLAB_BLENDING
	VxRaySegment stRS_Cur, stRS_PrevOut;
	VxRaySegment stPriorRS, stPosteriorRS;
	VxIntermixingKernelOut stIKOut;
#endif
#endif // #ifdef INPUT_LAYERS

#if defined(RM_SURFACEEFFECT)
#ifdef INPUT_LAYERS
	NOT AVAILABLE!!! // NOT AVAILABLE!!!
#endif 

	VxVolumeObject cbVolObj = g_cbVolObj;
	VxSurfaceEffect cbSurfEffect = g_cbSurfEffect;
	
#ifdef RM_SHADOW 
	// Shadow Effect
	float3 f3PosSampleLS = vxTransformPoint(vxSampleStartRefinement.f3PosSurfaceWS, g_cbShadowMap.matWS2LS);
	float fLightDistanceWLS = -vxTransformPoint(vxSampleStartRefinement.f3PosSurfaceWS, g_cbShadowMap.matWS2WLS).z;
	float fOpacityFiltered = SampleOpacityOnShadowMap_3x3(f3PosSampleLS.xy, fLightDistanceWLS);
	fOpacityFiltered = saturate(fOpacityFiltered);
	cbVolObj.f4ShadingFactor.y = (1.f - fOpacityFiltered * 0.5f) * g_cbVolObj.f4ShadingFactor.y;
	cbVolObj.f4ShadingFactor.z = (1.f - fOpacityFiltered * 0.8f) * g_cbVolObj.f4ShadingFactor.z;
#endif
	if (vxSampleStartRefinement.iHitSampleStep == 0)
	{
		cbVolObj.f4ShadingFactor.x = 1;
		cbVolObj.f4ShadingFactor.y = cbVolObj.f4ShadingFactor.z = 0;
		cbSurfEffect.iFlagsForIsoSurfaceRendering = 0;
	}

	// Ratio //
	// 1st bit : AO or Not , 2nd bit : Anisotropic BRDF or Not , 3rd bit : Apply Shading Factor or Not
	// 4th bit : 0 : Normal Curvature Map (2D), 1 : Apply Concaveness
	// 5th bit : Concaveness Direction or Not
	//int iFlagsForIsoSurfaceRendering;	
	//
	//int iOcclusionNumSamplesPerKernelRay;
	//float fOcclusionSampleStepDistOfKernelRay;
	//
	//float fPhongBRDF_DiffuseRatio;
	//float fPhongBRDF_ReflectanceRatio;
	//float fPhongExpWeightU;
	//float fPhongExpWeightV;
	VxCameraState cbCamState = g_cbCamState;
#ifdef MARKERS_ON_SURFACE
	cbSurfEffect.iFlagsForIsoSurfaceRendering = 5;
	cbSurfEffect.iOcclusionNumSamplesPerKernelRay = 15;
	cbSurfEffect.fOcclusionSampleStepDistOfKernelRay = 0.5;
	cbVolObj.f4ShadingFactor.x = 0.3;
	cbVolObj.f4ShadingFactor.y = 0.5;
	cbVolObj.f4ShadingFactor.z = 0.8;
	cbVolObj.f4ShadingFactor.w = 70;
	cbCamState.f3VecLightWS = normalize(float3(-1, 1, -1.0));
#endif

#ifdef SURFACE_MODE_CURVATURE
#ifdef OTF_MASK
	float4 f4Color = vxComputeOpaqueSurfaceCurvature(vxSampleStartRefinement.f3PosSurfaceWS, f3VecSampleWS, cbSurfEffect, g_cbCamState, cbVolObj,
		g_cbOtf1D, g_tex3DVolume, g_f4bufOTF, g_ibufOTFIndex, g_tex3DVolumeMask);
#else	
	float4 f4Color = vxComputeOpaqueSurfaceCurvature(vxSampleStartRefinement.f3PosSurfaceWS, f3VecSampleWS, cbSurfEffect, g_cbCamState, cbVolObj,
		g_cbOtf1D, g_tex3DVolume, g_f4bufOTF);
#endif
#else // ~SURFACE_MODE_CURVATURE
#ifdef OTF_MASK
	float4 f4Color = vxComputeOpaqueSurfaceEffect(vxSampleStartRefinement.f3PosSurfaceWS, f3VecSampleWS, cbSurfEffect, g_cbCamState, cbVolObj,
		g_cbOtf1D, g_tex3DVolume, g_f4bufOTF, g_ibufOTFIndex, g_tex3DVolumeMask);
#else
	float4 f4Color = vxComputeOpaqueSurfaceEffect(vxSampleStartRefinement.f3PosSurfaceWS, f3VecSampleWS, cbSurfEffect, cbCamState, cbVolObj,
		g_cbOtf1D, g_tex3DVolume, g_f4bufOTF);
#endif
#endif

#ifdef SAMPLER_VOLUME_CCF
	if (f4Color.a == 0)
		RETURN_DEFINED
#endif
	f4Color.a = 1.f;

#ifdef MARKERS_ON_SURFACE
	if (g_cbInterestingRegion.iNumRegions > 0)
	{
		float3 f3MarkerColor = float3(1.f, 1.f, 0);

		/////////////////
		float3 f3PosSampleTS = vxTransformPoint(vxSampleStartRefinement.f3PosSurfaceWS, cbVolObj.matWS2TS);
		float3 f3VecGrad = vxSampleVolumeGradientSamplerTex3D(f3PosSampleTS, 
			cbVolObj.f3VecGradientSampleX, cbVolObj.f3VecGradientSampleY, cbVolObj.f3VecGradientSampleZ, g_tex3DVolume);

		float3 f3VecLightWS = -g_cbCamState.f3VecLightWS;
		if(g_cbCamState.iFlags & 0x2)
			f3VecLightWS = -normalize(vxSampleStartRefinement.f3PosSurfaceWS - g_cbCamState.f3PosLightWS);


		float3 f3VecNormal = -normalize(f3VecGrad);
		float3 f3VecViewUnit = -normalize(f3VecSampleWS);
		
		float3 f3VecReflect = 2.f*f3VecNormal*dot(f3VecNormal, f3VecViewUnit) - f3VecViewUnit;
		float3 f3VecReflectNormal = normalize(f3VecReflect);

		float fLightDot = max(dot(f3VecNormal, f3VecLightWS), 0);
		float fDiffuse = fLightDot;
		float fDot2 = dot(f3VecReflectNormal, f3VecLightWS);
		float fReflectDot = max(fDot2, 0);
		float fReflect = pow(fReflectDot, cbVolObj.f4ShadingFactor.w);

		f3MarkerColor = f3MarkerColor.rgb * f4Color;//(cbVolObj.f4ShadingFactor.x + cbVolObj.f4ShadingFactor.y * fDiffuse + cbVolObj.f4ShadingFactor.z * fReflect);
		//////////////////////


		float fLengths[3];
		fLengths[0] = length(vxSampleStartRefinement.f3PosSurfaceWS - g_cbInterestingRegion.f3PosPtn0WS);
		fLengths[1] = length(vxSampleStartRefinement.f3PosSurfaceWS - g_cbInterestingRegion.f3PosPtn1WS);
		fLengths[2] = length(vxSampleStartRefinement.f3PosSurfaceWS - g_cbInterestingRegion.f3PosPtn2WS);
		float fRadius[3];
		fRadius[0] = g_cbInterestingRegion.fRadius0;
		fRadius[1] = g_cbInterestingRegion.fRadius1;
		fRadius[2] = g_cbInterestingRegion.fRadius2;
		for (int i = 0; i < g_cbInterestingRegion.iNumRegions; i++)
		{
			if (fLengths[i] <= fRadius[i])
			{
				float fRatio = 0.01f;//max(fLengths[i] / fRadius[i], 0.2f);

				f4Color.x = f4Color.x * fRatio + f3MarkerColor.x * (1.f - fRatio);
				f4Color.y = f4Color.y * fRatio + f3MarkerColor.y * (1.f - fRatio);
				f4Color.z = f4Color.z * fRatio + f3MarkerColor.z * (1.f - fRatio);
			}
		}
	}
#endif

	SET_OUTPUT(f4Color, f1stHitDepthBack)
	RETURN_DEFINED

#else //~defined(RM_SURFACEEFFECT) , which means Standard ray-casting
	float3 f3PosSampleStartWS = vxSampleStartRefinement.f3PosSurfaceWS;// - f3VecSampleWS; // sculpting 의 경계 artifact 문제 때문.
	float3 f3PosSampleStartTS = vxTransformPoint(f3PosSampleStartWS, g_cbVolObj.matWS2TS);

	float fNextSampleDepth = f1stHitDepthBack;
	int iSampleValuePrev = (int)(g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleStartTS, 0).r * g_cbVolObj.fSampleValueRange + 0.5f);
	
	float4 f4Out_Color = (float4)0;
#ifdef INPUT_LAYERS
	float fOut_Depth1stHit = min(f1stHitDepthBack, fDepthLayers1st);
#else
	float fOut_Depth1stHit = f1stHitDepthBack;
#endif
	float fOut_DepthExit = FLT_MAX;

#ifdef RM_CLIPSEMIOPAQUE
	// Clip Semi-Opaque //
	bool bIsClipInside = true;
	if(f2PrevNextT.y <= f2PrevNextT.x)
		bIsClipInside = false;
	float3 f3PosClipBoundaryPrevWS = f3PosIPSampleWS + f3VecSampleDirUnitWS*f2PrevNextT.x;
	float3 f3PosClipBoundaryNextWS = f3PosIPSampleWS + f3VecSampleDirUnitWS*f2PrevNextT.y;
	float3 f3VecClipPrev2SurfWS = vxSampleStartRefinement.f3PosSurfaceWS - f3PosClipBoundaryPrevWS;
	float3 f3VecClipNext2SurfWS = vxSampleStartRefinement.f3PosSurfaceWS - f3PosClipBoundaryNextWS;
	int iStepClipPrev = (int)(length(f3VecClipPrev2SurfWS) / g_cbVolObj.fSampleDistWS + 0.5f);
	if(dot(f3VecClipPrev2SurfWS, f3VecSampleWS) > 0)
		iStepClipPrev *= -1;
	int iStepClipNext = (int)(length(f3VecClipNext2SurfWS) / g_cbVolObj.fSampleDistWS + 0.5f);
	if(dot(f3VecClipNext2SurfWS, f3VecSampleWS) > 0)
		iStepClipNext *= -1;
	float3 f3PosBoundaryNextWS = f3PosIPSampleWS + f3VecSampleDirUnitWS*f2WithoutClipPrevNextT.y;
#else
	float3 f3PosBoundaryNextWS = f3PosIPSampleWS + f3VecSampleDirUnitWS*f2PrevNextT.y;
#endif
	iNumSamples = (int)(length(vxSampleStartRefinement.f3PosSurfaceWS - f3PosBoundaryNextWS) / g_cbVolObj.fSampleDistWS + 0.5f) + 1;
	
#ifdef SCULPTMASK
	int iSculptValue = (int)(g_cbVolObj.iFlags >> 24);
	if (iSculptValue == 0)
		iSculptValue = -1;
#endif

	float3 f3VecSampleTS = vxTransformVector(f3VecSampleWS, g_cbVolObj.matWS2TS);

	// Gradient Normal 은 density 가 큰쪽으로 가는 성질! //
	float3 f3VecLightWS = g_cbCamState.f3VecLightWS;
	if (g_cbCamState.iFlags & 0x2)
		f3VecLightWS = normalize(vxSampleStartRefinement.f3PosSurfaceWS - g_cbCamState.f3PosLightWS);
        
	// 1st Hit Clip Plane //
	int iStartStep = 0;
#if !defined(RM_CLIPSEMIOPAQUE) && !defined(RM_MODULATION)
	if (vxSampleStartRefinement.iHitSampleStep == 0)
	{
		iStartStep = 1;
		float3 f3PosSampleTS = vxTransformPoint(vxSampleStartRefinement.f3PosSurfaceWS, g_cbVolObj.matWS2TS);
		iSampleValuePrev = (int)(g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS, 0).r * g_cbVolObj.fSampleValueRange + 0.5f);
#ifdef SCULPTMASK
		float fMaskVisibility = vxSampleSculptMaskWeightByCCF(f3PosSampleTS, g_cbVolObj.f3SizeVolumeCVS, g_tex3DVolumeMask, iSculptValue);
		float4 f4ColorOTF = (float4)0;
		if (fMaskVisibility > 0.f)
		{
			f4ColorOTF = vxLoadPointOtf1DFromBuffer(iSampleValuePrev, g_f4bufOTF, g_cbVolObj.fOpaqueCorrection);
			f4ColorOTF *= fMaskVisibility;
		}
#else // ~SCULPTMASK
#ifdef OTF_MASK
		float2 f2SampleMaskValue = vxSampleByCCF(f3PosSampleTS, g_cbVolObj.f3SizeVolumeCVS, g_tex3DVolumeMask);
		int iMaskValue = (int)(f2SampleMaskValue.x * 255 + 0.5f);
		int iMaskID = g_ibufOTFIndex[iMaskValue];
		float4 f4ColorOTF_Origin = vxLoadPointOtf1DFromBuffer(iSampleValuePrev, g_f4bufOTF, g_cbVolObj.fOpaqueCorrection);
		float4 f4ColorOTF_Mask = vxLoadPointOtf1DSeriesFromBuffer(iSampleValuePrev, iMaskID, g_f4bufOTF, g_cbVolObj.fOpaqueCorrection);
		float4 f4ColorOTF = f4ColorOTF_Origin * (1.f - f2SampleMaskValue.y) + f4ColorOTF_Mask * f2SampleMaskValue.y;
#else
		float4 f4ColorOTF = vxLoadPointOtf1DFromBuffer(iSampleValuePrev, g_f4bufOTF, g_cbVolObj.fOpaqueCorrection);
#endif
#endif
		f4ColorOTF.a *= g_cbVolObj.fClipPlaneIntensity;

		if (f4ColorOTF.a >= FLT_OPACITY_MIN__)
		{
#if defined(ZSLAB_BLENDING) && defined(INPUT_LAYERS)
			if (iIndexOfPrevOutRSA >= iNumPrevOutRSs)
			{
				f4Out_Color = f4ColorOTF;
			}
			else
			{
				// Intermixing
				stRS_Cur.f4Visibility = f4ColorOTF;
				stRS_Cur.fThickness = g_cbVolObj.fSampleDistWS;
				stRS_Cur.fDepthBack = fNextSampleDepth;
				while (iIndexOfPrevOutRSA < iNumPrevOutRSs && stRS_Cur.fThickness > 0)
				{
					stRS_PrevOut = stPrevOutRSAs[iIndexOfPrevOutRSA];
					if (stRS_Cur.fDepthBack > stRS_PrevOut.fDepthBack)
					{
						stPriorRS = stRS_PrevOut;
						stPosteriorRS = stRS_Cur;
					}
					else
					{
						stPriorRS = stRS_Cur;
						stPosteriorRS = stRS_PrevOut;
					}
					stIKOut = vxIntermixingKernel(stPriorRS, stPosteriorRS, f4Out_Color);

					f4Out_Color = stIKOut.f4IntermixingVisibility;

					if (f4Out_Color.a > ERT_ALPHA)
					{
						iIndexOfPrevOutRSA = iNumPrevOutRSs; /*This implies ERT as well*/
						fOut_DepthExit = stIKOut.stIntegrationRS.fDepthBack;
						stRS_Cur.fThickness = 0;
						break;
					}
					else
					{
						if (stRS_Cur.fDepthBack > stRS_PrevOut.fDepthBack)
						{
							stRS_Cur = stIKOut.stRemainedRS;
							iIndexOfPrevOutRSA++;
						}
						else
						{
							stPrevOutRSAs[iIndexOfPrevOutRSA] = stIKOut.stRemainedRS;
							stRS_Cur.fThickness = 0;
							if (stIKOut.stRemainedRS.fThickness == 0)
								iIndexOfPrevOutRSA++;
						}
					}
				} // End of This loop, There is no more surfaces before the slab

				if (stRS_Cur.fThickness > 0)	// This implies f4Out_Color.a <= ERT_ALPHA
				{
					// There is no Prev Surface Layers but still Slab has opacity
					f4Out_Color += stRS_Cur.f4Visibility * (1.f - f4Out_Color.a);
					fOut_DepthExit = stRS_Cur.fDepthBack;
				}
			}
#else // ~ ((ZSLAB_BLENDING) && defined(INPUT_LAYERS))
			f4Out_Color += f4ColorOTF * (1.f - f4Out_Color.a);
			fOut_DepthExit = fNextSampleDepth;
#endif
		}
	}
#endif // #if !defined(RM_CLIPSEMIOPAQUE) && !defined(RM_MODULATION)

    
    
//#ifdef INPUT_LAYERS
//#ifdef ZSLAB_BLENDING
//	for (; iIndexOfPrevOutRSA < iNumPrevOutRSs; iIndexOfPrevOutRSA++)
//	{
//		stRS_PrevOut = stPrevOutRSAs[iIndexOfPrevOutRSA];
//		f4Out_Color += stRS_PrevOut.f4Visibility * (1.f - f4Out_Color.a);
//		fOut_DepthExit = stRS_PrevOut.fDepthBack;
//	}
//    //f4Out_Color = float4(1, 0, 0, 1);
//	SET_OUTPUT(f4Out_Color, fOut_Depth1stHit)
//	RETURN_DEFINED
//#endif // #ifdef INPUT_LAYERS
//#endif
    
    
	[loop]
	for (i = iStartStep; i < iNumSamples; i++)
	{
		float3 f3PosSampleWS = vxSampleStartRefinement.f3PosSurfaceWS + f3VecSampleWS*(float)i;
		float3 f3PosSampleTS = vxTransformPoint(f3PosSampleWS, g_cbVolObj.matWS2TS);

		VxBlockSkip blkSkip = vxComputeBlockRayLengthBoundary(f3PosSampleTS, f3VecSampleTS, g_cbBlkObj, g_tex3DBlock);
		blkSkip.iNumStepSkip = min(max(1, blkSkip.iNumStepSkip), iNumSamples - i);

		if (blkSkip.iSampleBlockValue > 0)
		{
			for (int k = 0; k < blkSkip.iNumStepSkip; k++, i++)
			{
				float3 f3PosSampleInBlockWS = vxSampleStartRefinement.f3PosSurfaceWS + f3VecSampleWS*(float)i;
				float3 f3PosSampleInBlockTS = vxTransformPoint(f3PosSampleInBlockWS, g_cbVolObj.matWS2TS);
				int iSampleValueNext = (int)(g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleInBlockTS, 0).r * g_cbVolObj.fSampleValueRange + 0.5f);
#ifdef SCULPTMASK
				float4 f4ColorOTF = (float4)0;

				//int iMaskValueNext = (int)(g_tex3DVolumeMask.SampleLevel(g_samplerLinear_Clamp, f3PosSampleInBlockTS, 0).r * g_cbVolObj.fSampleValueRange);
				//int iMaskValueNext = (int)(vxSampleByCCF(f3PosSampleInBlockTS, g_cbVolObj.f3SizeVolumeCVS, g_tex3DVolumeMask).x * 255 + 0.5f);	//255
				float fMaskVisibility = vxSampleSculptMaskWeightByCCF(f3PosSampleInBlockTS, g_cbVolObj.f3SizeVolumeCVS, g_tex3DVolumeMask, iSculptValue);
				if (fMaskVisibility > 0.f)
				{
					f4ColorOTF = vxLoadPointOtf1DFromBuffer(iSampleValueNext, g_f4bufOTF, g_cbVolObj.fOpaqueCorrection);
					f4ColorOTF *= fMaskVisibility;
				}

				//if(iMaskValueNext == 0 || iMaskValueNext > iSculptValue)
				//{
				//	f4ColorOTF = vxLoadSlabOtf1DFromTex2D(iSampleValuePrev, iSampleValueNext, g_f4bufOTF, g_cbOtf1D.iOtfSize, g_cbVolObj.fOpaqueCorrection, g_cbOtf1D.f4OtfColorEnd);
				//}
#else // ~SCULPTMASK
#ifdef OTF_MASK
				float2 f2SampleMaskValue = vxSampleByCCF(f3PosSampleInBlockTS, g_cbVolObj.f3SizeVolumeCVS, g_tex3DVolumeMask);
				int iMaskValue = (int)(f2SampleMaskValue.x * 255 + 0.5f);
				int iMaskID = g_ibufOTFIndex[iMaskValue];
				float4 f4ColorOTF_Origin = vxLoadPointOtf1DFromBuffer(iSampleValueNext, g_f4bufOTF, g_cbVolObj.fOpaqueCorrection);
				float4 f4ColorOTF_Mask = vxLoadPointOtf1DSeriesFromBuffer(iSampleValueNext, iMaskID, g_f4bufOTF, g_cbVolObj.fOpaqueCorrection);
				float4 f4ColorOTF = f4ColorOTF_Origin * (1.f - f2SampleMaskValue.y) + f4ColorOTF_Mask * f2SampleMaskValue.y;
#else
				float4 f4ColorOTF = vxLoadPointOtf1DFromBuffer(iSampleValueNext, g_f4bufOTF, g_cbVolObj.fOpaqueCorrection);
#endif
#endif // #ifdef SCULPTMASK

				if (f4ColorOTF.a >= FLT_OPACITY_MIN__)
				{
					fNextSampleDepth = f1stHitDepthBack + g_cbVolObj.fSampleDistWS*(float)i;
					fOut_DepthExit = fNextSampleDepth;

					float3 f3VecGrad = vxSampleVolumeGradientSamplerTex3D(f3PosSampleInBlockTS,
						g_cbVolObj.f3VecGradientSampleX, g_cbVolObj.f3VecGradientSampleY, g_cbVolObj.f3VecGradientSampleZ, g_tex3DVolume);
					float fGradLength = length(f3VecGrad) + 0.0001f;
					float3 f3VecGradNormal = f3VecGrad / fGradLength;
					float fLightDot = max(dot(f3VecGradNormal, f3VecLightWS), 0);
					float3 f3VecReflect = 2.f*f3VecGradNormal*dot(f3VecGradNormal, f3VecSampleDirUnitWS) - f3VecSampleDirUnitWS;
					float3 f3VecReflectNormal = normalize(f3VecReflect);
					float fReflectDot = max(dot(f3VecReflectNormal, f3VecLightWS), 0);
					fReflectDot = pow(fReflectDot, g_cbVolObj.f4ShadingFactor.w);
#ifdef RM_SHADOW
					// Shadow Effect
					float3 f3PosSampleLS = vxTransformPoint(f3PosSampleInBlockWS, g_cbShadowMap.matWS2LS);
					float fLightDistanceWLS = -vxTransformPoint(f3PosSampleInBlockWS, g_cbShadowMap.matWS2WLS).z;
					float fOpacityFiltered = SampleOpacityOnShadowMap_3x3(f3PosSampleLS.xy, fLightDistanceWLS); // Original
					f4ColorOTF.rgb *= g_cbVolObj.f4ShadingFactor.x + (1.f - fOpacityFiltered * 0.8f) * g_cbVolObj.f4ShadingFactor.y * fLightDot
						+ (1.f - fOpacityFiltered) * g_cbVolObj.f4ShadingFactor.z * fReflectDot;
#else // ~RM_SHADOW
					f4ColorOTF.rgb *= g_cbVolObj.f4ShadingFactor.x + g_cbVolObj.f4ShadingFactor.y * fLightDot + g_cbVolObj.f4ShadingFactor.z * fReflectDot;
#endif

#ifdef RM_MODULATION
					float fModulator = min(fGradLength * g_cbModulation.fGradMagAmplifier / g_cbModulation.fMaxGradientSize, 1.f);
					f4ColorOTF.rgba *= fModulator;// *(fOpacityPrev + 0.01f);//
#endif

#ifdef RM_CLIPSEMIOPAQUE
					if (!bIsClipInside)
					{
						f4ColorOTF.rgba *= g_cbVolObj.fClipPlaneIntensity;
					}
					else if (i < iStepClipPrev || i > iStepClipNext)
					{
						f4ColorOTF.rgba *= g_cbVolObj.fClipPlaneIntensity;
					}
#endif
                    
                    //#define INPUT_LAYERS
                    //#define ZSLAB_BLENDING
#ifdef INPUT_LAYERS
#ifdef ZSLAB_BLENDING
					if (iIndexOfPrevOutRSA >= iNumPrevOutRSs)
					{
						f4Out_Color += f4ColorOTF * (1.f - f4Out_Color.a);
					}
					else
					{
						// Intermixing
						stRS_Cur.f4Visibility = f4ColorOTF;
						stRS_Cur.fThickness = g_cbVolObj.fSampleDistWS;
						stRS_Cur.fDepthBack = fNextSampleDepth;
						while (iIndexOfPrevOutRSA < iNumPrevOutRSs && stRS_Cur.fThickness > 0)
						{
							stRS_PrevOut = stPrevOutRSAs[iIndexOfPrevOutRSA];
							if (stRS_Cur.fDepthBack > stRS_PrevOut.fDepthBack)
							{
								stPriorRS = stRS_PrevOut;
								stPosteriorRS = stRS_Cur;
							}
							else
							{
								stPriorRS = stRS_Cur;
								stPosteriorRS = stRS_PrevOut;
							}
							stIKOut = vxIntermixingKernel(stPriorRS, stPosteriorRS, f4Out_Color);

							f4Out_Color = stIKOut.f4IntermixingVisibility;

							if (f4Out_Color.a > ERT_ALPHA)
							{
								iIndexOfPrevOutRSA = iNumPrevOutRSs; /*This implies ERT as well*/
								fOut_DepthExit = stIKOut.stIntegrationRS.fDepthBack;
								stRS_Cur.fThickness = 0;
								break;
							}
							else
							{
								if (stRS_Cur.fDepthBack > stRS_PrevOut.fDepthBack)
								{
									stRS_Cur = stIKOut.stRemainedRS;
									iIndexOfPrevOutRSA++;
								}
								else
								{
									stPrevOutRSAs[iIndexOfPrevOutRSA] = stIKOut.stRemainedRS;
									stRS_Cur.fThickness = 0;
									if (stIKOut.stRemainedRS.fThickness == 0)
										iIndexOfPrevOutRSA++;
								}
							}
						} // End of This loop, There is no more surfaces before the slab

						if (stRS_Cur.fThickness > 0)	// This implies f4Out_Color.a <= ERT_ALPHA
						{
							// There is no Prev Surface Layers but still Slab has opacity
							f4Out_Color += stRS_Cur.f4Visibility * (1.f - f4Out_Color.a);
							fOut_DepthExit = stRS_Cur.fDepthBack;
						}
					}
#else // ~ZSLAB_BLENDING
					if (iIndexOfPrevOutRSA >= iNumPrevOuts)
					{
						f4Out_Color += f4ColorOTF * (1.f - f4Out_Color.a);
					}
					else
					{	// f4PrevOutLayersColorRGBAs, fPrevOutLayersDepthCSs
						// f4ColorOTF, fNextSampleDepth
						// fDepthLayers1st, fDepthLayersEnd
						//[unroll]
						for (int k = iIndexOfPrevOutRSA; k < iNumPrevOuts; k++)
						{
							float fDepthLayer = fPrevOutLayersDepthCSs[k]; // iIndexOfPrevOutRSA == k

							float fDiff = fNextSampleDepth - fDepthLayer;
							float4 fColorMix = f4ColorOTF;
							if (fDiff >= 0)
							{
								float4 f4ColorLayer = f4PrevOutLayersColorRGBAs[k];  // iIndexOfPrevOutRSA == k
								if (fDiff < g_cbVolObj.fSampleDistWS)
									fColorMix = vxFusionVisibility(f4ColorOTF, f4ColorLayer);
								else
									fColorMix = f4ColorLayer;

								f4Out_Color += fColorMix * (1.f - f4Out_Color.a);

								iIndexOfPrevOutRSA++;

								if (f4Out_Color.a > ERT_ALPHA)
								{
									iIndexOfPrevOutRSA = iNumPrevOuts; /*This implies ERT as well*/
									fOut_DepthExit = fNextSampleDepth;
									if (fDiff >= 0)
										fOut_DepthExit = fDepthLayer;

									k = iNumPrevOuts;
								}
							}
							else
							{
								f4Out_Color += fColorMix * (1.f - f4Out_Color.a);

								k = iNumPrevOuts;
							}
						}

						//while (iIndexOfPrevOutRSA < iNumPrevOuts)
						//{
						//	float fDepthLayer = fPrevOutLayersDepthCSs[iIndexOfPrevOutRSA];
						//
						//	float fDiff = fNextSampleDepth - fDepthLayer;
						//	float4 fColorMix = f4ColorOTF;
						//	if (fDiff >= 0)
						//	{
						//		float4 f4ColorLayer = f4PrevOutLayersColorRGBAs[iIndexOfPrevOutRSA];
						//		if (fDiff < g_cbVolObj.fSampleDistWS)
						//			fColorMix = vxFusionVisibility(f4ColorOTF, f4ColorLayer);
						//		else
						//			fColorMix = f4ColorLayer;
						//
						//		f4Out_Color += fColorMix * (1.f - f4Out_Color.a);
						//		iIndexOfPrevOutRSA++;
						//	}
						//	else
						//	{
						//		f4Out_Color += fColorMix * (1.f - f4Out_Color.a);
						//		break;
						//	}
						//
						//	if (f4Out_Color.a > ERT_ALPHA)
						//	{
						//		iIndexOfPrevOutRSA = iNumPrevOuts; /*This implies ERT as well*/
						//		fOut_DepthExit = fNextSampleDepth;
						//		if (fDiff >= 0)
						//			fOut_DepthExit = fDepthLayer;
						//		break;
						//	}
						//}
					}
#endif
#else // ~INPUT_LAYERS
					f4Out_Color += f4ColorOTF * (1.f - f4Out_Color.a);
#endif // #ifdef INPUT_LAYERS

					if (f4Out_Color.a > ERT_ALPHA)
					{
						i = iNumSamples + 1;
						k = blkSkip.iNumStepSkip + 1;
					//	break;
					}
					iSampleValuePrev = iSampleValueNext;
				}//if(f4ColorOTF.a > 0)
			}//for(int k = 0; k < blkSkip.iNumStepSkip; k++, i++)
		}
		else
		{
			i += blkSkip.iNumStepSkip;
			iSampleValuePrev = 0;
		}
		// this is for outer loop's i++
		i -= 1;
	}

#ifdef INPUT_LAYERS
#ifdef ZSLAB_BLENDING
	for (; iIndexOfPrevOutRSA < iNumPrevOutRSs; iIndexOfPrevOutRSA++)
	{
		stRS_PrevOut = stPrevOutRSAs[iIndexOfPrevOutRSA];
		f4Out_Color += stRS_PrevOut.f4Visibility * (1.f - f4Out_Color.a);
		fOut_DepthExit = stRS_PrevOut.fDepthBack;
	}
#else // ~ZSLAB_BLENDING
	for (; iIndexOfPrevOutRSA < iNumPrevOuts; iIndexOfPrevOutRSA++)
	{
		float4 f4ColorLayer = f4PrevOutLayersColorRGBAs[iIndexOfPrevOutRSA];
		float fDepthLayer = fPrevOutLayersDepthCSs[iIndexOfPrevOutRSA];

		f4Out_Color += f4ColorLayer * (1.f - f4Out_Color.a);
		fOut_DepthExit = fDepthLayer;
	}
#endif
#endif // #ifdef INPUT_LAYERS
    //f4Out_Color = float4(1, 0, 0, 1);
	SET_OUTPUT(f4Out_Color, fOut_Depth1stHit)
	RETURN_DEFINED

#endif // surface rendering or not
#endif // block show or not
#endif // x ray-casting or VR ray-casting
}


