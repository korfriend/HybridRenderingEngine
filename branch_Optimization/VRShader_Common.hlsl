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

//=====================
// Texture Buffers
//=====================
Buffer<float4> g_f4bufOTF : register(t0);		// Mask OTFs
Buffer<int> g_ibufOTFIndex : register(t3);		// Mask OTFs

Texture3D g_tex3DVolume : register(t1);
Texture3D g_tex3DBlock : register(t2);
Texture3D g_tex3DVolumeMask : register(t8);
Texture3D g_tex3DBrickChunks[3] : register(t10);	// Safe out

StructuredBuffer<CS_Output_ShadowMap> g_stb_ShadowMapInput : register(t6);
//==============//
// Main VR Code //

#if defined(VR_LAYER_OUT)
RWStructuredBuffer<CS_Output_MultiLayers> g_RWB_MultiLayerOut : register( u0 );
#else
RWStructuredBuffer<CS_Output_FB> g_RWB_RenderOut : register( u0 );
#endif

// OTF_MASK
// RM_SURFACEEFFECT | RM_SHADOW
// RM_MODULATION | RM_SHADOW | RM_CLIPSEMIOPAQUE
// MERGE_OUT

#ifdef RM_SHADOW
float SampleDepthOnShadowMap_BiLinear(uint2 ui2PosMS, float2 f2InterpolateRatio, uint uiDepthIndex)
{
	float fDepth0 = g_stb_ShadowMapInput[ui2PosMS.x + 0 + (ui2PosMS.y + 0)*g_cbCamState.uiGridOffsetX].fDepthHits[uiDepthIndex];
	float fDepth1 = g_stb_ShadowMapInput[ui2PosMS.x + 1 + (ui2PosMS.y + 0)*g_cbCamState.uiGridOffsetX].fDepthHits[uiDepthIndex];
	float fDepth2 = g_stb_ShadowMapInput[ui2PosMS.x + 0 + (ui2PosMS.y + 1)*g_cbCamState.uiGridOffsetX].fDepthHits[uiDepthIndex];
	float fDepth3 = g_stb_ShadowMapInput[ui2PosMS.x + 1 + (ui2PosMS.y + 1)*g_cbCamState.uiGridOffsetX].fDepthHits[uiDepthIndex];
	
	float fDepth01 = fDepth0*(1.f - f2InterpolateRatio.x) + fDepth1*f2InterpolateRatio.x;
	float fDepth23 = fDepth2*(1.f - f2InterpolateRatio.x) + fDepth3*f2InterpolateRatio.x;
	float fDepthFinal = fDepth01*(1.f - f2InterpolateRatio.y) + fDepth23*f2InterpolateRatio.y;

	return fDepthFinal;
}

float SampleOpacityOnShadowMap_BiLinear(uint2 ui2PosMS, float2 f2InterpolateRatio)
{
	float fOpacity0 = g_stb_ShadowMapInput[ui2PosMS.x + 0 + (ui2PosMS.y + 0)*g_cbCamState.uiGridOffsetX].fOpacity;
	float fOpacity1 = g_stb_ShadowMapInput[ui2PosMS.x + 1 + (ui2PosMS.y + 0)*g_cbCamState.uiGridOffsetX].fOpacity;
	float fOpacity2 = g_stb_ShadowMapInput[ui2PosMS.x + 0 + (ui2PosMS.y + 1)*g_cbCamState.uiGridOffsetX].fOpacity;
	float fOpacity3 = g_stb_ShadowMapInput[ui2PosMS.x + 1 + (ui2PosMS.y + 1)*g_cbCamState.uiGridOffsetX].fOpacity;
	
	float fOpacity01 = fOpacity0*(1.f - f2InterpolateRatio.x) + fOpacity1*f2InterpolateRatio.x;
	float fOpacity23 = fOpacity2*(1.f - f2InterpolateRatio.x) + fOpacity3*f2InterpolateRatio.x;
	float fOpacityFinal = fOpacity01*(1.f - f2InterpolateRatio.y) + fOpacity23*f2InterpolateRatio.y;

	return fOpacityFinal;
}

float SampleOpacityOnShadowMap_Bi_ClosePercentageFilter(float2 f2PosMS, float fDepthCurrentMS)
{
	uint2 ui2PosMS = uint2(f2PosMS.x, f2PosMS.y);
	float2 f2InterpolateRatio = f2PosMS - ui2PosMS;
	
	// Opaque!	
	float fDepth0 = g_stb_ShadowMapInput[ui2PosMS.x + 0 + (ui2PosMS.y + 0)*g_cbCamState.uiGridOffsetX].fDepthHits[0];
	float fDepth1 = g_stb_ShadowMapInput[ui2PosMS.x + 1 + (ui2PosMS.y + 0)*g_cbCamState.uiGridOffsetX].fDepthHits[0];
	float fDepth2 = g_stb_ShadowMapInput[ui2PosMS.x + 0 + (ui2PosMS.y + 1)*g_cbCamState.uiGridOffsetX].fDepthHits[0];
	float fDepth3 = g_stb_ShadowMapInput[ui2PosMS.x + 1 + (ui2PosMS.y + 1)*g_cbCamState.uiGridOffsetX].fDepthHits[0];
	
	float fOcclusionFactor0 = 1;
	if(fDepth0 >= fDepthCurrentMS)
		fOcclusionFactor0 = 0;
	float fOcclusionFactor1 = 1;
	if(fDepth1 >= fDepthCurrentMS)
		fOcclusionFactor1 = 0;
	float fOcclusionFactor2 = 1;
	if(fDepth2 >= fDepthCurrentMS)
		fOcclusionFactor2 = 0;
	float fOcclusionFactor3 = 1;
	if(fDepth3 >= fDepthCurrentMS)
		fOcclusionFactor3 = 0;
	
	float fOpacity01 = fOcclusionFactor0*(1.f - f2InterpolateRatio.x) + fOcclusionFactor1*f2InterpolateRatio.x;
	float fOpacity23 = fOcclusionFactor2*(1.f - f2InterpolateRatio.x) + fOcclusionFactor3*f2InterpolateRatio.x;
	float fOpacityFinal = fOpacity01*(1.f - f2InterpolateRatio.y) + fOpacity23*f2InterpolateRatio.y;

	return fOpacityFinal;//(fOcclusionFactor0 + fOcclusionFactor1 + fOcclusionFactor2 + fOcclusionFactor3)*0.25f;
}

float SampleOpacityOnShadowMap_ClosePercentageFilter(float2 f2PosMS, float fDepthCurrentMS)
{
	// Filter Kernel Size 3x3
	float fOpacity0 = SampleOpacityOnShadowMap_Bi_ClosePercentageFilter(f2PosMS + float2(-1, -1), fDepthCurrentMS);
	float fOpacity1 = SampleOpacityOnShadowMap_Bi_ClosePercentageFilter(f2PosMS + float2( 0, -1), fDepthCurrentMS);
	float fOpacity2 = SampleOpacityOnShadowMap_Bi_ClosePercentageFilter(f2PosMS + float2( 1, -1), fDepthCurrentMS);
	float fOpacity3 = SampleOpacityOnShadowMap_Bi_ClosePercentageFilter(f2PosMS + float2(-1,  0), fDepthCurrentMS);
	float fOpacity4 = SampleOpacityOnShadowMap_Bi_ClosePercentageFilter(f2PosMS + float2( 0,  0), fDepthCurrentMS);
	float fOpacity5 = SampleOpacityOnShadowMap_Bi_ClosePercentageFilter(f2PosMS + float2( 1,  0), fDepthCurrentMS);
	float fOpacity6 = SampleOpacityOnShadowMap_Bi_ClosePercentageFilter(f2PosMS + float2(-1,  1), fDepthCurrentMS);
	float fOpacity7 = SampleOpacityOnShadowMap_Bi_ClosePercentageFilter(f2PosMS + float2( 0,  1), fDepthCurrentMS);
	float fOpacity8 = SampleOpacityOnShadowMap_Bi_ClosePercentageFilter(f2PosMS + float2( 1,  1), fDepthCurrentMS);
	return (fOpacity0 + fOpacity1 + fOpacity2 + fOpacity3 + fOpacity4 + fOpacity5 + fOpacity6 + fOpacity7 + fOpacity8)/9;
}

// 1 ==> Occluded, 0 ==> Not Occluded
float SampleOpacityOnShadowMap_TriLinear(float2 f2PosMS, float fDepthCurrentMS)
{
	uint2 ui2PosMS = uint2(f2PosMS.x, f2PosMS.y);
	float2 f2InterpolateRatio = f2PosMS - ui2PosMS;

	// Only 0 is Valid!
	float fDepthPrev = SampleDepthOnShadowMap_BiLinear(ui2PosMS, f2InterpolateRatio, 0);
	if(fDepthPrev > fDepthCurrentMS)
		return 0;
	
	float fDepthNext = SampleDepthOnShadowMap_BiLinear(ui2PosMS, f2InterpolateRatio, 1);
	float fOpacity = SampleOpacityOnShadowMap_BiLinear(ui2PosMS, f2InterpolateRatio);
	if( fDepthNext > fDepthCurrentMS )
	{
		// Need Interpolation
		float fRatioNext = (fDepthCurrentMS - fDepthPrev)/(fDepthNext - fDepthPrev);
		fOpacity = fOpacity*fRatioNext;
	}

	return fOpacity;
}

float SampleOpacityOnShadowMap_DeepMap3x3Average(float2 f2PosMS, float fDepthCurrentMS)
{
	// Filter Kernel Size 3x3
	float fOpacity0 = SampleOpacityOnShadowMap_TriLinear(f2PosMS + float2(-1, -1), fDepthCurrentMS);
	float fOpacity1 = SampleOpacityOnShadowMap_TriLinear(f2PosMS + float2( 0, -1), fDepthCurrentMS);
	float fOpacity2 = SampleOpacityOnShadowMap_TriLinear(f2PosMS + float2( 1, -1), fDepthCurrentMS);
	float fOpacity3 = SampleOpacityOnShadowMap_TriLinear(f2PosMS + float2(-1,  0), fDepthCurrentMS);
	float fOpacity4 = SampleOpacityOnShadowMap_TriLinear(f2PosMS + float2( 0,  0), fDepthCurrentMS);
	float fOpacity5 = SampleOpacityOnShadowMap_TriLinear(f2PosMS + float2( 1,  0), fDepthCurrentMS);
	float fOpacity6 = SampleOpacityOnShadowMap_TriLinear(f2PosMS + float2(-1,  1), fDepthCurrentMS);
	float fOpacity7 = SampleOpacityOnShadowMap_TriLinear(f2PosMS + float2( 0,  1), fDepthCurrentMS);
	float fOpacity8 = SampleOpacityOnShadowMap_TriLinear(f2PosMS + float2( 1,  1), fDepthCurrentMS);
	return (fOpacity0 + fOpacity1 + fOpacity2 + fOpacity3 + fOpacity4 + fOpacity5 + fOpacity6 + fOpacity7 + fOpacity8)/9;
}

float SampleOpacityOnShadowMap_DepthLinear(uint2 ui2PosMS, float fDepthCurrentMS)
{
	uint uiAddr = ui2PosMS.x + ui2PosMS.y*g_cbCamState.uiGridOffsetX;
	float fDepthPrev = g_stb_ShadowMapInput[uiAddr].fDepthHits[0];
	if(fDepthPrev > fDepthCurrentMS)
		return 0;

	float fDepthNext = g_stb_ShadowMapInput[uiAddr].fDepthHits[1];
	float fOpacity = g_stb_ShadowMapInput[uiAddr].fOpacity;
	fOpacity = min(fOpacity, 1);
	//fOpacity = min(max(fOpacity, 0), 1);
	
	if( fDepthNext > fDepthCurrentMS )
	{
		// Need Interpolation
		float fRatioNext = (fDepthCurrentMS - fDepthPrev)/(fDepthNext - fDepthPrev);
		fOpacity = fOpacity*fRatioNext;
	}

	return fOpacity;// min(max(fOpacity, 0), 1);
}

float SampleOpacityOnShadowMap_PCF_TriLinear(float2 f2PosMS, float fDepthCurrentMS)
{
	uint2 ui2PosMS = uint2(f2PosMS.x, f2PosMS.y);
	float2 f2InterpolateRatio = f2PosMS - ui2PosMS;
		
	float fOcclusionFactor0 = SampleOpacityOnShadowMap_DepthLinear(uint2(ui2PosMS.x + 0, ui2PosMS.y + 0), fDepthCurrentMS);
	float fOcclusionFactor1 = SampleOpacityOnShadowMap_DepthLinear(uint2(ui2PosMS.x + 1, ui2PosMS.y + 0), fDepthCurrentMS);
	float fOcclusionFactor2 = SampleOpacityOnShadowMap_DepthLinear(uint2(ui2PosMS.x + 0, ui2PosMS.y + 1), fDepthCurrentMS);
	float fOcclusionFactor3 = SampleOpacityOnShadowMap_DepthLinear(uint2(ui2PosMS.x + 1, ui2PosMS.y + 1), fDepthCurrentMS);
	
	float fOpacity01 = fOcclusionFactor0*(1.f - f2InterpolateRatio.x) + fOcclusionFactor1*f2InterpolateRatio.x;
	float fOpacity23 = fOcclusionFactor2*(1.f - f2InterpolateRatio.x) + fOcclusionFactor3*f2InterpolateRatio.x;
	float fOpacityFinal = fOpacity01*(1.f - f2InterpolateRatio.y) + fOpacity23*f2InterpolateRatio.y;

	return fOpacityFinal;//(fOcclusionFactor0 + fOcclusionFactor1 + fOcclusionFactor2 + fOcclusionFactor3)*0.25f;
}

float SampleOpacityOnShadowMap_DeepMapClosePercentageFilter3x3Average(float2 f2PosMS, float fDepthCurrentMS)
{
	// Filter Kernel Size 3x3
	float fOpacity0 = SampleOpacityOnShadowMap_PCF_TriLinear(f2PosMS + float2(-1, -1), fDepthCurrentMS);
	float fOpacity1 = SampleOpacityOnShadowMap_PCF_TriLinear(f2PosMS + float2( 0, -1), fDepthCurrentMS);
	float fOpacity2 = SampleOpacityOnShadowMap_PCF_TriLinear(f2PosMS + float2( 1, -1), fDepthCurrentMS);
	float fOpacity3 = SampleOpacityOnShadowMap_PCF_TriLinear(f2PosMS + float2(-1,  0), fDepthCurrentMS);
	float fOpacity4 = SampleOpacityOnShadowMap_PCF_TriLinear(f2PosMS + float2( 0,  0), fDepthCurrentMS);
	float fOpacity5 = SampleOpacityOnShadowMap_PCF_TriLinear(f2PosMS + float2( 1,  0), fDepthCurrentMS);
	float fOpacity6 = SampleOpacityOnShadowMap_PCF_TriLinear(f2PosMS + float2(-1,  1), fDepthCurrentMS);
	float fOpacity7 = SampleOpacityOnShadowMap_PCF_TriLinear(f2PosMS + float2( 0,  1), fDepthCurrentMS);
	float fOpacity8 = SampleOpacityOnShadowMap_PCF_TriLinear(f2PosMS + float2( 1,  1), fDepthCurrentMS);
	return (fOpacity0 + fOpacity1 + fOpacity2 + fOpacity3 + fOpacity4 + fOpacity5 + fOpacity6 + fOpacity7 + fOpacity8)/9;
}
#endif

[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void VRCS_Common( uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex )
{
	if(DTid.x >= g_cbCamState.uiScreenSizeX || DTid.y >= g_cbCamState.uiScreenSizeY)
		return;
	
	VxRaySegment stCleanRS = (VxRaySegment)0;
	stCleanRS.fDepthBack = FLT_MAX;

	int i, j;
	uint uiSampleAddr = DTid.x + DTid.y*g_cbCamState.uiGridOffsetX;
	float fThicknessSurface = g_cbVolObj.fSampleDistWS;//g_cbVolObj.fThicknessVirtualzSlab;	#if defined(RM_SURFACEEFFECT)

	// Initial Check //
	float4 f4Out_Color = (float4)0;
	float fOut_Depth1stHit = FLT_MAX;
	float fOut_DepthExit = FLT_MAX;
	CS_Output_MultiLayers stOut2ndPhaseBuffer = g_RWB_PrevLayers[uiSampleAddr];
	VxRaySegment arrayPrevOutRSA[NUM_MERGE_LAYERS];
	int iNumPrevOutRSs = 0;
	// 여기에서 개수 카운트에 0 을 쓰므로 Ray-Segment에 의 alpha 에 FLT_OPACITY_MIN__ 체크... // 아니면 thickness 로 체크하도록 해야 함.!
	[unroll]
	for(i = 0; i < NUM_MERGE_LAYERS; i++)
	{
		float4 f4ColorIn = vxConvertColorToFloat4(stOut2ndPhaseBuffer.arrayIntVisibilityRSA[i]);
		if(f4ColorIn.a > 0 && f4Out_Color.a <= ERT_ALPHA)
		{
			VxRaySegment stRS;
			stRS.f4Visibility = f4ColorIn;
			stRS.fDepthBack = stOut2ndPhaseBuffer.arrayDepthBackRSA[i];
			stRS.fThickness = stOut2ndPhaseBuffer.arrayThicknessRSA[i];
			arrayPrevOutRSA[i] = stRS;
			iNumPrevOutRSs++;
			
			fOut_DepthExit = stRS.fDepthBack;
			f4Out_Color += stRS.f4Visibility * (1.f - f4Out_Color.a);
		}
		else
		{
			arrayPrevOutRSA[i] = stCleanRS;
		}
	}
	if (iNumPrevOutRSs > 0)	// == 0 이면 FLT_MAX 로 처리
		fOut_Depth1stHit = stOut2ndPhaseBuffer.arrayDepthBackRSA[0];// -stOut2ndPhaseBuffer.arrayThicknessRSA[0];
	
	VxRaySegment stPriorRS, stPosteriorRS;
	VxIntermixingKernelOut stIKOut;

#ifdef VR_LAYER_OUT
#define NUM_VR_LAYER NUM_MERGE_LAYERS
	VxRaySegment arrayCurrentRSA[NUM_MERGE_LAYERS];	// output
	g_RWB_MultiLayerOut[uiSampleAddr] = stOut2ndPhaseBuffer;
	float4 f4IntermixingVisibility = (float4)0;
#else
	// Only for DEFAULT (BRICK) // CLIPSEMIOPAQUE // MODULATION // SHADOW (BRICK) // DEFAULT_SCULPTMASK
	g_RWB_RenderOut[uiSampleAddr].iColor = vxConvertColorToInt(f4Out_Color);
	g_RWB_RenderOut[uiSampleAddr].fDepth1stHit = fOut_Depth1stHit;
	//g_RWB_RenderOut[uiSampleAddr].fDepthExit = fOut_DepthExit;	
#endif
	f4Out_Color = (float4)0;
	fOut_Depth1stHit = FLT_MAX;
	fOut_DepthExit = FLT_MAX;

	// Image Plane's Position and Camera State //
	float3 f3PosIPSampleSS = float3((float)DTid.x, (float)DTid.y, 0.0f);
	float3 f3PosIPSampleWS = vxTransformPoint(f3PosIPSampleSS, g_cbCamState.matSS2WS);
	float3 f3VecSampleDirUnitWS = g_cbCamState.f3VecViewWS;
	if(g_cbCamState.iFlags & 0x1)
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
	if(f2WithoutClipPrevNextT.y <= f2WithoutClipPrevNextT.x)
		return;
	
	int iNumSamples = (int)((f2WithoutClipPrevNextT.y - f2WithoutClipPrevNextT.x) / g_cbVolObj.fSampleDistWS + 0.5f);
	float3 f3PosBoundaryPrevWS = f3PosIPSampleWS + f3VecSampleDirUnitWS*f2WithoutClipPrevNextT.x;
#else
	// 1st Exit in the case that there is no ray-intersecting boundary in the volume box
	if(f2PrevNextT.y <= f2PrevNextT.x)
		return;
	
	int iNumSamples = (int)((f2PrevNextT.y - f2PrevNextT.x) / g_cbVolObj.fSampleDistWS + 0.5f);
	float3 f3PosBoundaryPrevWS = f3PosIPSampleWS + f3VecSampleDirUnitWS*f2PrevNextT.x;
#endif

#if (defined(RM_RAYMAX) || defined(RM_RAYMIN) || defined(RM_RAYSUM)) && defined(VR_LAYER_OUT)
	float fDepthSampleSatrt = length(f3PosBoundaryPrevWS - f3PosIPSampleWS);
	float3 f3VecSampleTS = vxTransformVector(f3VecSampleWS, g_cbVolObj.matWS2TS);
#if defined(RM_RAYMAX) || defined(RM_RAYMIN)
	int iLuckyStep = (int)((float)(vxRandom( f3PosBoundaryPrevWS.xy ) + 1) * (float)iNumSamples * 0.5f);
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

#else
	float fSampleDepth = fDepthSampleSatrt + g_cbVolObj.fSampleDistWS*(float)(iNumSamples);
	int iNumValidSamples = 0;
	float4 f4ColorOTF_Sum = (float4)0;
#endif
	[loop]
	for(i = 0; i < iNumSamples; i++)
	{
		float3 f3PosSampleWS = f3PosBoundaryPrevWS + f3VecSampleWS*(float)i;
		float3 f3PosSampleTS = vxTransformPoint(f3PosSampleWS, g_cbVolObj.matWS2TS);

#if defined(RM_RAYMAX) || defined(RM_RAYMIN)
		VxBlockSkip blkSkip = vxComputeBlockRayLengthBoundary(f3PosSampleTS, f3VecSampleTS, g_cbBlkObj, g_tex3DBlock);
		blkSkip.iNumStepSkip = min(max(1, blkSkip.iNumStepSkip), iNumSamples - i);
#ifdef RM_RAYMAX
		if(blkSkip.iSampleBlockValue > (int)fValueSamplePrev)
#elif RM_RAYMIN
		if(blkSkip.iSampleBlockValue < (int)fValueSamplePrev)
#endif
		{
			for(int k = 0; k < blkSkip.iNumStepSkip; k++, i++)
			{
				float3 f3PosSampleInBlockWS = f3PosBoundaryPrevWS + f3VecSampleWS*(float)i;
				float3 f3PosSampleInBlockTS = vxTransformPoint(f3PosSampleInBlockWS, g_cbVolObj.matWS2TS);
				float fSampleValue = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleInBlockTS, 0).r * g_cbVolObj.fSampleValueRange;

#ifdef OTF_MASK
				f3PosMaskSampleTS = f3PosSampleInBlockTS;
#endif

#ifdef RM_RAYMAX

				if(fSampleValue > fValueSamplePrev)
#else
				if(fSampleValue < fValueSamplePrev)
#endif
				{
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
#else
		// RAYSUM //
		float fSampleValue = g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS, 0).r * g_cbVolObj.fSampleValueRange;

#ifdef OTF_MASK
		float2 f2SampleMaskValue = vxSampleByCCF(f3PosSampleTS, g_cbVolObj.f3SizeVolumeCVS, g_tex3DVolumeMask);
		int iMaskValue = (int)(f2SampleMaskValue.x * 255 + 0.5f);
		int iMaskID = g_ibufOTFIndex[iMaskValue];
		float4 f4ColorOTF_Origin = vxLoadPointOtf1DFromBuffer((int)fSampleValue, g_f4bufOTF, 1.f);	// MUST BE 1.f instead of g_cbVolObj.fOpaqueCorrection
		float4 f4ColorOTF_Mask = vxLoadPointOtf1DSeriesFromBuffer((int)fSampleValue, iMaskID, g_f4bufOTF, 1.f);
		float4 f4ColorOTF = f4ColorOTF_Origin * (1.f - f2SampleMaskValue.y) + f4ColorOTF_Mask * f2SampleMaskValue.y;
#else
		float4 f4ColorOTF = vxLoadPointOtf1DFromBuffer((int)fSampleValue, g_f4bufOTF, 1.f);
#endif
		if(f4ColorOTF.a > 0)
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
#else	// RM_RAYMAX, RM_RAYMIN

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
	if(f4ColorOTF.a == 0)
		return;
	
	int iNumElementsOfCurRSA = 1;
	arrayCurrentRSA[0].f4Visibility = f4ColorOTF;
	arrayCurrentRSA[0].fDepthBack = fSampleDepth;
	arrayCurrentRSA[0].fThickness = max(fSampleDepth - fDepthSampleSatrt, FLT_MIN);
	VXMIXOUT;
	return;
#else
	
	// Surface Refinement //
	VxSurfaceRefinement vxSampleStartRefinement;

#if defined(BLOCKSHOW) && defined(VR_LAYER_OUT)
	vxSampleStartRefinement = vxVrBlockPointSurfaceWithBlockSkipping(f3PosBoundaryPrevWS, f3VecSampleDirUnitWS, iNumSamples,
		g_cbVolObj, g_cbBlkObj, g_tex3DBlock);
	
	if(vxSampleStartRefinement.iHitSampleStep == -1)
		return;

	// Block Vis 의 경우 1st slab 의 front 가 vxSampleStartRefinement.f3PosSurfaceWS 으로 나옴
	float f1stHitDepthBack = length(vxSampleStartRefinement.f3PosSurfaceWS - f3PosIPSampleWS);
	float4 f4ColorBox = float4(vxTransformPoint(vxSampleStartRefinement.f3PosSurfaceWS, g_cbVolObj.matWS2TS), 1);
	
	int iNumElementsOfCurRSA = 1;
	arrayCurrentRSA[0].f4Visibility = f4ColorBox;
	arrayCurrentRSA[0].fDepthBack = f1stHitDepthBack;
	arrayCurrentRSA[0].fThickness = fThicknessSurface;
	VXMIXOUT;
	return;
#else
	
#ifdef SAMPLE_FROM_BRICKCHUNK
	vxSampleStartRefinement = vxVrSlabSurfaceRefinementWithBlockSkipping(f3PosBoundaryPrevWS, f3VecSampleDirUnitWS, iNumSamples,
		g_cbVolObj, g_cbBlkChunk, g_tex3DBrickChunks, g_cbBlkObj, g_tex3DBlock, g_cbOtf1D);
#elif SCULPTMASK
	vxSampleStartRefinement = vxVrMaskSurfaceRefinementWithBlockSkipping(f3PosBoundaryPrevWS, f3VecSampleDirUnitWS, iNumSamples,
		g_cbVolObj, g_tex3DVolume, g_tex3DVolumeMask, g_cbBlkObj, g_tex3DBlock, g_cbOtf1D);
#else
#ifdef OTF_MASK
	vxSampleStartRefinement = vxVrSlabSurfaceRefinementWithBlockSkipping(f3PosBoundaryPrevWS, f3VecSampleDirUnitWS, iNumSamples,
		g_cbVolObj, g_tex3DVolume, g_cbBlkObj, g_tex3DBlock, g_cbOtf1D, g_f4bufOTF, g_ibufOTFIndex, g_tex3DVolumeMask);
#else
	vxSampleStartRefinement = vxVrSlabSurfaceRefinementWithBlockSkipping(f3PosBoundaryPrevWS, f3VecSampleDirUnitWS, iNumSamples,
		g_cbVolObj, g_tex3DVolume, g_cbBlkObj, g_tex3DBlock, g_cbOtf1D);
#endif
#endif
	
	// There is no hit surface in the volume
	if (vxSampleStartRefinement.iHitSampleStep == -1)
	{
		//arrayCurrentRSA[0].f4Visibility = float4(1, 0, 0, 1);
		//arrayCurrentRSA[0].fDepthBack = 0;
		//arrayCurrentRSA[0].fThickness = 0.1;
		//int iNumElementsOfCurRSA = 1;
		//VXMIXOUT;
		return;
	}

	float f1stHitDepthBack = length(vxSampleStartRefinement.f3PosSurfaceWS - f3PosIPSampleWS);
	float f1stHitDepthFront = f1stHitDepthBack - fThicknessSurface;
	
	// 3rd Exit in the case of the Surface is on the front of the ray's 1st hit
	// CONSIDER THAT THE VIRTUAL ZSLAB's ADDITIONAL OVERWRAPPING INTERVAL for OUR SLAB INTERMIXING.
	if(fOut_DepthExit <= f1stHitDepthFront && f4Out_Color.a > ERT_ALPHA)
		return;

#if defined(RM_SURFACEEFFECT) && defined(VR_LAYER_OUT)
	VxVolumeObject cbVolObj = g_cbVolObj;
	VxSurfaceEffect cbSurfEffect = g_cbSurfEffect;
#ifdef RM_SHADOW 
	// Shadow Effect
	float3 f3PosSampleLS = vxTransformPoint(vxSampleStartRefinement.f3PosSurfaceWS, g_cbShadowMap.matWS2LS);
	float fLightDistanceWLS = -vxTransformPoint(vxSampleStartRefinement.f3PosSurfaceWS, g_cbShadowMap.matWS2WLS).z;
	float fOpacityFiltered = SampleOpacityOnShadowMap_DeepMapClosePercentageFilter3x3Average(f3PosSampleLS.xy, fLightDistanceWLS);
	fOpacityFiltered = saturate(fOpacityFiltered);
	cbVolObj.f4ShadingFactor.y = (1.f - fOpacityFiltered * 0.8f) * g_cbVolObj.f4ShadingFactor.y;
	cbVolObj.f4ShadingFactor.z = (1.f - fOpacityFiltered) * g_cbVolObj.f4ShadingFactor.z;
#endif
	if(vxSampleStartRefinement.iHitSampleStep == 0)
	{
		cbVolObj.f4ShadingFactor.x = 1;
		cbVolObj.f4ShadingFactor.y = cbVolObj.f4ShadingFactor.z = 0;

		cbSurfEffect.iFlagsForIsoSurfaceRendering = 0;
	}

#ifdef SAMPLE_FROM_BRICKCHUNK
	float4 f4Color = vxComputeOpaqueSurfaceEffect(vxSampleStartRefinement.f3PosSurfaceWS, f3VecSampleWS, cbSurfEffect, g_cbCamState, cbVolObj, 
		g_cbBlkChunk, g_tex3DBrickChunks, g_cbBlkObj, g_tex3DBlock, g_cbOtf1D, g_f4bufOTF);
#else
#ifdef SURFACE_MODE_CURVATURE
#ifdef OTF_MASK
	float4 f4Color = vxComputeOpaqueSurfaceCurvature(vxSampleStartRefinement.f3PosSurfaceWS, f3VecSampleWS, cbSurfEffect, g_cbCamState, cbVolObj,
		g_cbOtf1D, g_tex3DVolume, g_f4bufOTF, g_ibufOTFIndex, g_tex3DVolumeMask);
#else
	float4 f4Color = vxComputeOpaqueSurfaceCurvature(vxSampleStartRefinement.f3PosSurfaceWS, f3VecSampleWS, cbSurfEffect, g_cbCamState, cbVolObj,
		g_cbOtf1D, g_tex3DVolume, g_f4bufOTF);
#endif
#else
#ifdef OTF_MASK
	float4 f4Color = vxComputeOpaqueSurfaceEffect(vxSampleStartRefinement.f3PosSurfaceWS, f3VecSampleWS, cbSurfEffect, g_cbCamState, cbVolObj, 
		g_cbOtf1D, g_tex3DVolume, g_f4bufOTF, g_ibufOTFIndex, g_tex3DVolumeMask);
#else
	float4 f4Color = vxComputeOpaqueSurfaceEffect(vxSampleStartRefinement.f3PosSurfaceWS, f3VecSampleWS, cbSurfEffect, g_cbCamState, cbVolObj,
		g_cbOtf1D, g_tex3DVolume, g_f4bufOTF);
#endif
#endif
#endif
	
#ifdef SAMPLER_VOLUME_CCF
	if (f4Color.a == 0)
		return;
	f4Color.a = 1.f;
#else
	f4Color.a = 1.f;
#endif
	int iNumElementsOfCurRSA = 1;

#ifdef MARKERS_ON_SURFACE
	if (g_cbInterestingRegion.iNumRegions > 0)
	{
		float3 f3MarkerColor = float3(1.f, 0, 0);

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
				float fRatio = max(fLengths[i] / fRadius[i], 0.2f);

				f4Color.x = f4Color.x * fRatio + f3MarkerColor.x * (1.f - fRatio);
				f4Color.y = f4Color.y * fRatio + f3MarkerColor.y * (1.f - fRatio);
				f4Color.z = f4Color.z * fRatio + f3MarkerColor.z * (1.f - fRatio);
			}
		}
	}
#endif

	arrayCurrentRSA[0].f4Visibility = f4Color;
	arrayCurrentRSA[0].fDepthBack = f1stHitDepthBack;
	arrayCurrentRSA[0].fThickness = fThicknessSurface;
	VXMIXOUT;
	return;

#else	// Another Renderers (Standard Ray-Casting)
	// Standard DVR //
	float3 f3PosSampleStartWS = vxSampleStartRefinement.f3PosSurfaceWS;// - f3VecSampleWS; // sculpting 의 경계 artifact 문제 때문.
	float3 f3PosSampleStartTS = vxTransformPoint(f3PosSampleStartWS, g_cbVolObj.matWS2TS);

	float fNextSampleDepth = f1stHitDepthBack;
#ifdef SAMPLE_FROM_BRICKCHUNK
	int iSampleValuePrev = (int)(vxSampleInBrickChunk(f3PosSampleStartTS, g_cbBlkObj, g_tex3DBlock, g_cbBlkChunk, g_tex3DBrickChunks) * g_cbVolObj.fSampleValueRange + 0.5f);
#else
	int iSampleValuePrev = (int)(g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleStartTS, 0).r * g_cbVolObj.fSampleValueRange + 0.5f);
#endif
	
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

#ifdef RM_MODULATION
	float fOpacityPrev = 0;
#endif
#ifdef SCULPTMASK
	int iSculptValue = (int)(g_cbVolObj.iFlags >> 24);
	if(iSculptValue == 0)
		iSculptValue = -1;
#endif
	
	float3 f3VecSampleTS = vxTransformVector(f3VecSampleWS, g_cbVolObj.matWS2TS);

#ifdef VR_LAYER_OUT
	int iIndexCurrentRSA = 0;
	for(i = 0; i < NUM_VR_LAYER; i++)
	{
		arrayCurrentRSA[i] = stCleanRS;
	}
	VxRaySegment stOnTheFlyRS = stCleanRS;
#else
	fOut_Depth1stHit = min(f1stHitDepthBack/* - fThicknessSurface*/, fOut_Depth1stHit);
	int iIndexOfPrevOutRSA = 0;
	VxRaySegment stRS_Cur, stRS_PrevOut;
#endif

	f4Out_Color = (float4)0;

	// 편의상 1st hit Light Direction 으로 통일 ㅋ_ㅋ //
	// Gradient Normal 은 density 가 큰쪽으로 가는 성질! //
	float3 f3VecLightWS = g_cbCamState.f3VecLightWS;
	if(g_cbCamState.iFlags & 0x2)
		f3VecLightWS = normalize(vxSampleStartRefinement.f3PosSurfaceWS - g_cbCamState.f3PosLightWS);

	// 1st Hit Clip Plane //
	int iStartStep = 0;
#if !defined(RM_CLIPSEMIOPAQUE) && !defined(RM_MODULATION)
	if(vxSampleStartRefinement.iHitSampleStep == 0)
	{
		iStartStep = 1;
		float3 f3PosSampleTS = vxTransformPoint(vxSampleStartRefinement.f3PosSurfaceWS, g_cbVolObj.matWS2TS);

#ifdef SAMPLE_FROM_BRICKCHUNK
		// This is for precision correction?! because of the difference between "f3VecSampleDirWS*(float)i" and computed block distance with single precision
		int3 i3BlockID = int3(f3PosSampleTS.x / g_cbBlkObj.f3SizeBlockTS.x, f3PosSampleTS.y / g_cbBlkObj.f3SizeBlockTS.y, f3PosSampleTS.z / g_cbBlkObj.f3SizeBlockTS.z);
		int iSampleBlockValue = (int)(g_tex3DBlock.Load(int4(i3BlockID, 0)).r * g_cbBlkObj.fSampleValueRange + 0.5f);	// New
		float3 f3PosSampleCTS = vxComputeBrickSamplePosInChunk(f3PosSampleTS, iSampleBlockValue - 1, g_cbBlkObj, g_cbBlkChunk);
		float fSampleValue = vxSampleInBrickChunkWithBlockValue(f3PosSampleCTS, iSampleBlockValue - 1, g_cbBlkChunk, g_tex3DBrickChunks);
		iSampleValuePrev = (int)(fSampleValue * g_cbVolObj.fSampleValueRange + 0.5f);
#else
		iSampleValuePrev = (int)(g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS, 0).r * g_cbVolObj.fSampleValueRange + 0.5f);
#endif
#ifdef SCULPTMASK
		//int iMaskValueNext = (int)(g_tex3DVolumeMask.SampleLevel(g_samplerLinear_Clamp, f3PosSampleInBlockTS, 0).r * g_cbVolObj.fSampleValueRange + 0.5f);
		float fMaskVisibility = vxSampleSculptMaskWeightByCCF(f3PosSampleTS, g_cbVolObj.f3SizeVolumeCVS, g_tex3DVolumeMask, iSculptValue);
		float4 f4ColorOTF = (float4)0;
		if(fMaskVisibility > 0.f)
		{
			f4ColorOTF = vxLoadPointOtf1DFromBuffer(iSampleValuePrev, g_f4bufOTF, g_cbVolObj.fOpaqueCorrection);
			f4ColorOTF *= fMaskVisibility;
		}
#else
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
		if(f4ColorOTF.a >= FLT_OPACITY_MIN__)
		{
#ifdef VR_LAYER_OUT
			stOnTheFlyRS.f4Visibility = f4ColorOTF;
			stOnTheFlyRS.fDepthBack = fNextSampleDepth;
			stOnTheFlyRS.fThickness = g_cbVolObj.fSampleDistWS;
			arrayCurrentRSA[iIndexCurrentRSA] = stOnTheFlyRS;	// iIndexCurrentRSA cannot be over than NUM_VR_LAYER - 1
			f4Out_Color = f4ColorOTF;
#else
			if(iIndexOfPrevOutRSA >= iNumPrevOutRSs)
			{
				f4Out_Color = f4ColorOTF;
			}
			else
			{
				// Intermixing
				stRS_Cur.f4Visibility = f4ColorOTF;
				stRS_Cur.fThickness = g_cbVolObj.fSampleDistWS;
				stRS_Cur.fDepthBack = fNextSampleDepth;
				while(iIndexOfPrevOutRSA < iNumPrevOutRSs && stRS_Cur.fThickness > 0)
				{
					stRS_PrevOut = arrayPrevOutRSA[iIndexOfPrevOutRSA];
					if(stRS_Cur.fDepthBack > stRS_PrevOut.fDepthBack)
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

					if(f4Out_Color.a > ERT_ALPHA)
					{
						iIndexOfPrevOutRSA = iNumPrevOutRSs; /*This implies ERT as well*/
						fOut_DepthExit = stIKOut.stIntegrationRS.fDepthBack;
						stRS_Cur.fThickness = 0;
						break;
					}
					else
					{
						if(stRS_Cur.fDepthBack > stRS_PrevOut.fDepthBack)
						{
							stRS_Cur = stIKOut.stRemainedRS;
							iIndexOfPrevOutRSA++;
						}
						else
						{
							arrayPrevOutRSA[iIndexOfPrevOutRSA] = stIKOut.stRemainedRS;
							stRS_Cur.fThickness = 0;
							if(stIKOut.stRemainedRS.fThickness == 0)
								iIndexOfPrevOutRSA++;
						}
					}
				} // End of This loop, There is no more surfaces before the slab

				if(stRS_Cur.fThickness > 0)	// This implies f4Out_Color.a <= ERT_ALPHA
				{
					// There is no Prev Surface Layers but still Slab has opacity
					f4Out_Color += stRS_Cur.f4Visibility * (1.f - f4Out_Color.a);
					fOut_DepthExit = stRS_Cur.fDepthBack;
				}
			}
#endif
		}
	}
#endif

	[loop]
	for(i = iStartStep; i < iNumSamples; i++)
	{
		float3 f3PosSampleWS = vxSampleStartRefinement.f3PosSurfaceWS + f3VecSampleWS*(float)i;
		float3 f3PosSampleTS = vxTransformPoint(f3PosSampleWS, g_cbVolObj.matWS2TS);
		
		VxBlockSkip blkSkip = vxComputeBlockRayLengthBoundary(f3PosSampleTS, f3VecSampleTS, g_cbBlkObj, g_tex3DBlock);
		blkSkip.iNumStepSkip = min(max(1, blkSkip.iNumStepSkip), iNumSamples - i);
		
		if(blkSkip.iSampleBlockValue > 0)
		{
			for(int k = 0; k < blkSkip.iNumStepSkip; k++, i++)
			{
				float3 f3PosSampleInBlockWS = vxSampleStartRefinement.f3PosSurfaceWS + f3VecSampleWS*(float)i;
				float3 f3PosSampleInBlockTS = vxTransformPoint(f3PosSampleInBlockWS, g_cbVolObj.matWS2TS);
#ifdef SAMPLE_FROM_BRICKCHUNK
				int iSampleValueNext = 0;
				float3 f3PosSampleInBlockCTS = vxComputeBrickSamplePosInChunk(f3PosSampleInBlockTS, blkSkip.iSampleBlockValue - 1, g_cbBlkObj, g_cbBlkChunk);
				// This is for precision correction?! because of the difference between "f3VecSampleDirWS*(float)i" and computed block distance with single precision
				if(k < blkSkip.iNumStepSkip - 1)
				{
					float fSampleValue = vxSampleInBrickChunkWithBlockValue(f3PosSampleInBlockCTS, blkSkip.iSampleBlockValue - 1, g_cbBlkChunk, g_tex3DBrickChunks);
					iSampleValueNext = (int)(fSampleValue * g_cbVolObj.fSampleValueRange + 0.5f);
				}
				else
				{
					int3 i3BlockID = int3(f3PosSampleInBlockTS.x / g_cbBlkObj.f3SizeBlockTS.x, f3PosSampleInBlockTS.y / g_cbBlkObj.f3SizeBlockTS.y, f3PosSampleInBlockTS.z / g_cbBlkObj.f3SizeBlockTS.z);
					blkSkip.iSampleBlockValue = (int)(g_tex3DBlock.Load(int4(i3BlockID, 0)).r * g_cbBlkObj.fSampleValueRange + 0.5f);	// New
					f3PosSampleInBlockCTS = vxComputeBrickSamplePosInChunk(f3PosSampleInBlockTS, blkSkip.iSampleBlockValue - 1, g_cbBlkObj, g_cbBlkChunk);
					float fSampleValue = vxSampleInBrickChunkWithBlockValue(f3PosSampleInBlockCTS, blkSkip.iSampleBlockValue - 1, g_cbBlkChunk, g_tex3DBrickChunks);
					iSampleValueNext = (int)(fSampleValue * g_cbVolObj.fSampleValueRange + 0.5f);
				}
#else
				int iSampleValueNext = (int)(g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleInBlockTS, 0).r * g_cbVolObj.fSampleValueRange + 0.5f);
#endif
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
#else
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
#endif
#ifdef RM_MODULATION
				float fAlphaOrigin = f4ColorOTF.a;
#endif

				if(f4ColorOTF.a >= FLT_OPACITY_MIN__)
				{
#ifdef RM_MODULATION
					//f4ColorOTF.a = SAFE_OPAQUEALPHA;
					//fAlphaOrigin = f4ColorOTF.a;
#endif
					
					fNextSampleDepth = f1stHitDepthBack + g_cbVolObj.fSampleDistWS*(float)i;
					fOut_DepthExit = fNextSampleDepth;

#ifdef SAMPLE_FROM_BRICKCHUNK
					float3 f3VecGrad = vxSampleVolumeGradientSamplerTex3DFromBrickChunks(f3PosSampleInBlockCTS, 
						g_cbVolObj.f3VecGradientSampleX, g_cbVolObj.f3VecGradientSampleY, g_cbVolObj.f3VecGradientSampleZ, 
						blkSkip.iSampleBlockValue - 1, g_cbBlkChunk.iNumBricksInChunkXY, g_tex3DBrickChunks);
#else
					float3 f3VecGrad = vxSampleVolumeGradientSamplerTex3D(f3PosSampleInBlockTS, 
						g_cbVolObj.f3VecGradientSampleX, g_cbVolObj.f3VecGradientSampleY, g_cbVolObj.f3VecGradientSampleZ, g_tex3DVolume);
#endif
					float fGradLength = length(f3VecGrad) + 0.0000001f;

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
					//float fOpacityFiltered = SampleOpacityOnShadowMap_ClosePercentageFilter(f3PosSampleLS.xy, fLightDistanceWLS);
					//float fOpacityFiltered = SampleOpacityOnShadowMap_DeepMap3x3Average(f3PosSampleLS.xy, fLightDistanceWLS);
					float fOpacityFiltered = SampleOpacityOnShadowMap_DeepMapClosePercentageFilter3x3Average(f3PosSampleLS.xy, fLightDistanceWLS); // Original
					//float fOpacityFiltered = SampleOpacityOnShadowMap_PCF_TriLinear(f3PosSampleLS.xy, fLightDistanceWLS);
					fOpacityFiltered = saturate(fOpacityFiltered);
					f4ColorOTF.rgb *= g_cbVolObj.f4ShadingFactor.x + (1.f - fOpacityFiltered * 0.8f) * g_cbVolObj.f4ShadingFactor.y * fLightDot 
						+ (1.f - fOpacityFiltered) * g_cbVolObj.f4ShadingFactor.z * fReflectDot;
#else
					f4ColorOTF.rgb *= g_cbVolObj.f4ShadingFactor.x + g_cbVolObj.f4ShadingFactor.y * fLightDot + g_cbVolObj.f4ShadingFactor.z * fReflectDot;
#endif

#ifdef RM_MODULATION
					float fModulator = 0.f;
					// Modulation
					float fDistEyeNorm = 0.0f;//max(distance(f3PosSamplePrevWS, f3PosSampleWS)/distance(f3PosSamplePrevWS, f3PosSampleNextWS), 1.f);
					// fExponent should be larger than 1.f
					float fExponent = max(pow(max(
						g_cbModulation.fRevealingFactor * 2.f * (1.f - fDistEyeNorm) * fLightDot * (1.f - fOpacityPrev), 0), g_cbModulation.fSharpnessFactor)
						, 1.f);
					fModulator = pow(min(fGradLength*g_cbModulation.fGradMagAmplifier/g_cbModulation.fMaxGradientSize, 1.f), fExponent);	
					f4ColorOTF.rgba *= fModulator;// *(fOpacityPrev + 0.01f);//
					if(f4ColorOTF.a >= FLT_OPACITY_MIN__){
#endif
					
#ifdef RM_CLIPSEMIOPAQUE
					if(!bIsClipInside)
					{
						f4ColorOTF.rgba *= g_cbVolObj.fClipPlaneIntensity;
					}
					else if(i < iStepClipPrev || i > iStepClipNext)
					{
						f4ColorOTF.rgba *= g_cbVolObj.fClipPlaneIntensity;
					}
					if(f4ColorOTF.a >= FLT_OPACITY_MIN__){
#endif

#ifdef VR_LAYER_OUT
					stOnTheFlyRS.f4Visibility += f4ColorOTF * (1.f - stOnTheFlyRS.f4Visibility.a);
					stOnTheFlyRS.fDepthBack = fNextSampleDepth;
					stOnTheFlyRS.fThickness += g_cbVolObj.fSampleDistWS;
					arrayCurrentRSA[iIndexCurrentRSA] = stOnTheFlyRS;	// iIndexCurrentRSA cannot be over than NUM_VR_LAYER - 1

					f4Out_Color += f4ColorOTF * (1.f - f4Out_Color.a);
#else
					if(iIndexOfPrevOutRSA >= iNumPrevOutRSs)
					{
						f4Out_Color += f4ColorOTF * (1.f - f4Out_Color.a);
					}
					else
					{
						// Intermixing
						stRS_Cur.f4Visibility = f4ColorOTF;
						stRS_Cur.fThickness = g_cbVolObj.fSampleDistWS;
						stRS_Cur.fDepthBack = fNextSampleDepth;
						while(iIndexOfPrevOutRSA < iNumPrevOutRSs && stRS_Cur.fThickness > 0)
						{
							stRS_PrevOut = arrayPrevOutRSA[iIndexOfPrevOutRSA];
							if(stRS_Cur.fDepthBack > stRS_PrevOut.fDepthBack)
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

							if(f4Out_Color.a > ERT_ALPHA)
							{
								iIndexOfPrevOutRSA = iNumPrevOutRSs; /*This implies ERT as well*/
								fOut_DepthExit = stIKOut.stIntegrationRS.fDepthBack;
								stRS_Cur.fThickness = 0;
								break;
							}
							else
							{
								if(stRS_Cur.fDepthBack > stRS_PrevOut.fDepthBack)
								{
									stRS_Cur = stIKOut.stRemainedRS;
									iIndexOfPrevOutRSA++;
								}
								else
								{
									arrayPrevOutRSA[iIndexOfPrevOutRSA] = stIKOut.stRemainedRS;
									stRS_Cur.fThickness = 0;
									if(stIKOut.stRemainedRS.fThickness == 0)
										iIndexOfPrevOutRSA++;
								}
							}
						} // End of This loop, There is no more surfaces before the slab

						if(stRS_Cur.fThickness > 0)	// This implies f4Out_Color.a <= ERT_ALPHA
						{
							// There is no Prev Surface Layers but still Slab has opacity
							f4Out_Color += stRS_Cur.f4Visibility * (1.f - f4Out_Color.a);
							fOut_DepthExit = stRS_Cur.fDepthBack;
						}
					}
#endif
					if(f4Out_Color.a > ERT_ALPHA)
					{
#ifndef VR_LAYER_OUT
						iIndexOfPrevOutRSA = iNumPrevOutRSs;	/*This implies ERT as well*/
#endif
						i = iNumSamples + 1;
						k = blkSkip.iNumStepSkip + 1;
						break;
					}
					iSampleValuePrev = iSampleValueNext;
#if defined(RM_MODULATION) || defined(RM_CLIPSEMIOPAQUE)
					}
#endif
				}//if(f4ColorOTF.a > 0)
#ifdef VR_LAYER_OUT
				else if(stOnTheFlyRS.f4Visibility.a > 0 && iIndexCurrentRSA < NUM_VR_LAYER - 1)
				{	//if(f4ColorOTF.a == 0)
					iIndexCurrentRSA++;
					stOnTheFlyRS = stCleanRS;
				}
#endif
#ifdef RM_MODULATION
				fOpacityPrev = f4ColorOTF.a;// fAlphaOrigin;
#endif
			}//for(int k = 0; k < blkSkip.iNumStepSkip; k++, i++)
		}
		else
		{
			i += blkSkip.iNumStepSkip;
			iSampleValuePrev = 0;

#ifdef RM_MODULATION
			fOpacityPrev = 0;
#endif
		}
		// this is for outer loop's i++
		i -= 1;
	}

#ifdef VR_LAYER_OUT
	int iNumElementsOfCurRSA = 0;//iIndexCurrentRSA + 1
	for(i = 0; i < NUM_VR_LAYER; i++)
	{
		if(arrayCurrentRSA[i].f4Visibility.a > 0)
			iNumElementsOfCurRSA++;
	}
	VXMIXOUT;
#else

	for(; iIndexOfPrevOutRSA < iNumPrevOutRSs; iIndexOfPrevOutRSA++)
	{
		stRS_PrevOut = arrayPrevOutRSA[iIndexOfPrevOutRSA];
		f4Out_Color += stRS_PrevOut.f4Visibility * (1.f - f4Out_Color.a);
		fOut_DepthExit = stRS_PrevOut.fDepthBack;
	}
	fOut_Depth1stHit = min(arrayPrevOutRSA[0].fDepthBack/* - arrayPrevOutRSA[0].fThickness*/, fOut_Depth1stHit);

	g_RWB_RenderOut[uiSampleAddr].iColor = vxConvertColorToInt(f4Out_Color);
	g_RWB_RenderOut[uiSampleAddr].fDepth1stHit = fOut_Depth1stHit;
	//g_RWB_RenderOut[uiSampleAddr].fDepthExit = fOut_DepthExit;
#endif
	return;

#endif	// ifdef RM_SURFACEEFFECT
#endif	// ifdef BLOCKSHOW
#endif	// if defined(RAYMAX) || defined(RAYMIN) || defined(RAYSUM)
	
}

Buffer<float4> g_f4bufOTF_Ex1 : register(t21);		// Mask OTFs
Buffer<float4> g_f4bufOTF_Ex2 : register(t22);		// Mask OTFs
Buffer<float4> g_f4bufOTF_Ex3 : register(t23);		// Mask OTFs
Texture3D g_tex3DVolume_Ex1 : register(t31);
Texture3D g_tex3DVolume_Ex2 : register(t32);
Texture3D g_tex3DVolume_Ex3 : register(t33);

cbuffer cbGlobalParams : register( b8 )
{
	VxVolumeObject g_cbVolObj_Ex1;
	VxVolumeObject g_cbVolObj_Ex2;
	VxVolumeObject g_cbVolObj_Ex3;
}
cbuffer cbGlobalParams : register( b9 )
{
	VxOtf1D g_cbOtf1D_Ex1;
	VxOtf1D g_cbOtf1D_Ex2;
	VxOtf1D g_cbOtf1D_Ex3;
}
cbuffer cbGlobalParams : register( b10 )
{
	VxModulation g_cbModulation_Ex1;
	VxModulation g_cbModulation_Ex2;
	VxModulation g_cbModulation_Ex3;
}

float4 vxComputeVisibility(float3 f3PosSampleWS, float3 f3VecLightWS, float3 f3VecSampleDirUnitWS, 
	VxVolumeObject cbVolObj, VxModulation cbModulation, float fOpacityPrev, Buffer<float4> f4bufOTF, Texture3D tex3DVolume, Texture3D tex3DVolumeMask, float fOpacityCorrection)
{
	float3 f3PosSampleTS = vxTransformPoint(f3PosSampleWS, cbVolObj.matWS2TS);
	int iSampleValue = (int)(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS, 0).r * cbVolObj.fSampleValueRange + 0.5f);

#ifdef OTF_MASK
	float2 f2SampleMaskValue = vxSampleByCCF(f3PosSampleTS, cbVolObj.f3SizeVolumeCVS, tex3DVolumeMask);
	int iMaskValue = (int)(f2SampleMaskValue.x * 255 + 0.5f);
	int iMaskID = g_ibufOTFIndex[iMaskValue];
	float4 f4ColorOTF_Origin = vxLoadPointOtf1DFromBuffer(iSampleValue, f4bufOTF, cbVolObj.fOpaqueCorrection);
	float4 f4ColorOTF_Mask = vxLoadPointOtf1DSeriesFromBuffer(iSampleValue, iMaskID, f4bufOTF, cbVolObj.fOpaqueCorrection);
	float4 f4ColorOTF = f4ColorOTF_Origin * (1.f - f2SampleMaskValue.y) + f4ColorOTF_Mask * f2SampleMaskValue.y;
#else
	float4 f4ColorOTF = vxLoadPointOtf1DFromBuffer(iSampleValue, f4bufOTF, fOpacityCorrection);
#endif
	if(f4ColorOTF.a > 0)
	{
		float3 f3VecGrad = vxSampleVolumeGradientSamplerTex3D(f3PosSampleTS, 
			cbVolObj.f3VecGradientSampleX, cbVolObj.f3VecGradientSampleY, cbVolObj.f3VecGradientSampleZ, tex3DVolume);

		float fGradLength = length(f3VecGrad) + 0.0000001f;

		float3 f3VecGradNormal = f3VecGrad / fGradLength;
		float fLightDot = max(dot(f3VecGradNormal, f3VecLightWS), 0);
				
		float3 f3VecReflect = 2.f*f3VecGradNormal*dot(f3VecGradNormal, f3VecSampleDirUnitWS) - f3VecSampleDirUnitWS;
		float3 f3VecReflectNormal = normalize(f3VecReflect);
		float fReflectDot = max(dot(f3VecReflectNormal, f3VecLightWS), 0);
		fReflectDot = pow(fReflectDot, cbVolObj.f4ShadingFactor.w);

		f4ColorOTF.rgb *= cbVolObj.f4ShadingFactor.x + cbVolObj.f4ShadingFactor.y * fLightDot + cbVolObj.f4ShadingFactor.z * fReflectDot;
		
		if(cbModulation.fdummy____0 == 1)
		{
			float fModulator = 0.f;
			// Modulation
			float fDistEyeNorm = 0.0f;//max(distance(f3PosSamplePrevWS, f3PosSampleWS)/distance(f3PosSamplePrevWS, f3PosSampleNextWS), 1.f);
			// fExponent should be larger than 1.f
			float fExponent = max(pow(max(
				cbModulation.fRevealingFactor*2*(1.f - fDistEyeNorm)*fLightDot*(1.f - fOpacityPrev), 0), cbModulation.fSharpnessFactor)
				, 1.f);
			fModulator = pow(min(fGradLength*cbModulation.fGradMagAmplifier/cbModulation.fMaxGradientSize, 1.f), fExponent);	
			f4ColorOTF.rgba *= fModulator;
		}
	}
	return f4ColorOTF;
}


[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void VRCS_Experiment( uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex )
{
	if(DTid.x >= g_cbCamState.uiScreenSizeX || DTid.y >= g_cbCamState.uiScreenSizeY)
		return;

	int i, j;
	uint uiSampleAddr = DTid.x + DTid.y*g_cbCamState.uiGridOffsetX;
	
	VxRaySegment stCleanRS = (VxRaySegment)0;
	stCleanRS.fDepthBack = FLT_MAX;

	// Initial Check //
	float4 f4Out_Color = (float4)0;
	float fOut_Depth1stHit = FLT_MAX;
	float fOut_DepthExit = FLT_MAX;
	CS_Output_MultiLayers stOut2ndPhaseBuffer = g_RWB_PrevLayers[uiSampleAddr];
	VxRaySegment arrayPrevOutRSA[NUM_MERGE_LAYERS];
	int iNumPrevOutRSs = 0;
	[unroll]
	for(i = 0; i < NUM_MERGE_LAYERS; i++)
	{
		float4 f4ColorIn = vxConvertColorToFloat4(stOut2ndPhaseBuffer.arrayIntVisibilityRSA[i]);
		if(f4ColorIn.a > 0 && f4Out_Color.a <= ERT_ALPHA)
		{
			VxRaySegment stRS;
			stRS.f4Visibility = f4ColorIn;
			stRS.fDepthBack = stOut2ndPhaseBuffer.arrayDepthBackRSA[i];
			stRS.fThickness = stOut2ndPhaseBuffer.arrayThicknessRSA[i];
			arrayPrevOutRSA[i] = stRS;
			iNumPrevOutRSs++;
			
			fOut_DepthExit = stRS.fDepthBack;
			f4Out_Color += stRS.f4Visibility * (1.f - f4Out_Color.a);
		}
		else
		{
			arrayPrevOutRSA[i] = stCleanRS;
		}
	}
	fOut_Depth1stHit = stOut2ndPhaseBuffer.arrayDepthBackRSA[0];// -stOut2ndPhaseBuffer.arrayThicknessRSA[0];
	
	VxRaySegment stPriorRS, stPosteriorRS;
	VxIntermixingKernelOut stIKOut;
	
#ifndef VR_LAYER_OUT
	g_RWB_RenderOut[uiSampleAddr].iColor = vxConvertColorToInt(f4Out_Color);
	g_RWB_RenderOut[uiSampleAddr].fDepth1stHit = fOut_Depth1stHit;
	//g_RWB_RenderOut[uiSampleAddr].fDepthExit = fOut_DepthExit;	
#endif
	
	f4Out_Color = (float4)0;
	fOut_Depth1stHit = FLT_MAX;
	fOut_DepthExit = FLT_MAX;

	// Image Plane's Position and Camera State //
	float3 f3PosIPSampleSS = float3((float)DTid.x, (float)DTid.y, 0.0f);
	float3 f3PosIPSampleWS = vxTransformPoint(f3PosIPSampleSS, g_cbCamState.matSS2WS);
	float3 f3VecSampleDirUnitWS = g_cbCamState.f3VecViewWS;
	if(g_cbCamState.iFlags & 0x1)
		f3VecSampleDirUnitWS = f3PosIPSampleWS - g_cbCamState.f3PosEyeWS;
	f3VecSampleDirUnitWS = normalize(f3VecSampleDirUnitWS);

	float fSampleDistMinWS = min(g_cbVolObj.fSampleDistWS, g_cbVolObj_Ex1.fSampleDistWS);
#ifdef THREE_SIMULTANEOUS_MIX
	fSampleDistMinWS = min(fSampleDistMinWS, g_cbVolObj_Ex2.fSampleDistWS);
#endif
	if(fSampleDistMinWS == 0)
		fSampleDistMinWS = g_cbVolObj.fSampleDistWS;

	float3 f3VecSampleWS = f3VecSampleDirUnitWS * fSampleDistMinWS;
	
	float fOpacityCorrection0 = g_cbVolObj.fOpaqueCorrection * fSampleDistMinWS / g_cbVolObj.fSampleDistWS;
	float fOpacityCorrection1 = g_cbVolObj_Ex1.fOpaqueCorrection * fSampleDistMinWS / g_cbVolObj_Ex1.fSampleDistWS;
#ifdef THREE_SIMULTANEOUS_MIX
	float fOpacityCorrection2 = g_cbVolObj_Ex2.fOpaqueCorrection * fSampleDistMinWS / g_cbVolObj_Ex2.fSampleDistWS;
#endif

	// Ray Intersection for Clipping Box //
	float2 f2PrevNextT0 = vxVrComputeBoundary(f3PosIPSampleWS, f3VecSampleDirUnitWS, g_cbVolObj);
	bool bIsValidVolume0 = true;
	if(f2PrevNextT0.x > f2PrevNextT0.y)
	{
		f2PrevNextT0.x = FLT_MAX;
		f2PrevNextT0.y = 0;
		bIsValidVolume0 = false;
	}

	float2 f2PrevNextT1 = vxVrComputeBoundary(f3PosIPSampleWS, f3VecSampleDirUnitWS, g_cbVolObj_Ex1);	// MULTICASE
	bool bIsValidVolume1 = true;
	if(f2PrevNextT1.x > f2PrevNextT1.y)
	{
		f2PrevNextT1.x = FLT_MAX;
		f2PrevNextT1.y = 0;
		bIsValidVolume1 = false;
	}
	
#ifdef THREE_SIMULTANEOUS_MIX
	float2 f2PrevNextT2 = vxVrComputeBoundary(f3PosIPSampleWS, f3VecSampleDirUnitWS, g_cbVolObj_Ex2);	// MULTICASE
	bool bIsValidVolume2 = true;
	if(f2PrevNextT2.x > f2PrevNextT2.y)
	{
		f2PrevNextT2.x = FLT_MAX;
		f2PrevNextT2.y = 0;
		bIsValidVolume2 = false;
	}
	if(!bIsValidVolume0 && !bIsValidVolume1 && !bIsValidVolume2)
		return;
	float2 f2PrevNextT = float2(min(min(f2PrevNextT0.x, f2PrevNextT1.x), f2PrevNextT2.x), max(max(f2PrevNextT0.y, f2PrevNextT1.y), f2PrevNextT2.y));
	if(f2PrevNextT.y <= f2PrevNextT.x)
		return;
#else
	if(!bIsValidVolume0 && !bIsValidVolume1)
		return;
	float2 f2PrevNextT = float2(min(f2PrevNextT0.x, f2PrevNextT1.x), max(f2PrevNextT0.y, f2PrevNextT1.y));
	if(f2PrevNextT.y <= f2PrevNextT.x)
		return;
#endif
	
	int iNumSamples = (int)((f2PrevNextT.y - f2PrevNextT.x) / fSampleDistMinWS + 0.5f);
	float3 f3PosBoundaryPrevWS = f3PosIPSampleWS + f3VecSampleDirUnitWS*f2PrevNextT.x;
	float3 f3VecSampleTS = vxTransformVector(f3VecSampleWS, g_cbVolObj.matWS2TS);
	float fDepthSampleStart = f2PrevNextT.x;
	
	float3 f3VecLightWS = g_cbCamState.f3VecLightWS;
	int iIndexOfPrevOutRSA = 0;
	VxRaySegment stRS_Cur, stRS_PrevOut;

	float fAlphaOTF0_Prev = 0;
	float fAlphaOTF1_Prev = 0;
#ifdef THREE_SIMULTANEOUS_MIX
	float fAlphaOTF2_Prev = 0;
#endif

	[loop]
	for(i = 0; i < iNumSamples; i++)
	{
		float3 f3PosSampleWS = f3PosBoundaryPrevWS + f3VecSampleWS * (float)i;

		float4 f4ColorOTF0 = vxComputeVisibility(f3PosSampleWS, f3VecLightWS, f3VecSampleDirUnitWS, g_cbVolObj, g_cbModulation, fAlphaOTF0_Prev, g_f4bufOTF, g_tex3DVolume, g_tex3DVolumeMask, fOpacityCorrection0);
		float4 f4ColorOTF1 = vxComputeVisibility(f3PosSampleWS, f3VecLightWS, f3VecSampleDirUnitWS, g_cbVolObj_Ex1, g_cbModulation_Ex1, fAlphaOTF1_Prev, g_f4bufOTF_Ex1, g_tex3DVolume_Ex1, g_tex3DVolumeMask, fOpacityCorrection1);
		float4 f4ColorOTF = vxFusionVisibility(f4ColorOTF0, f4ColorOTF1);
		fAlphaOTF0_Prev = f4ColorOTF0.a;
		fAlphaOTF1_Prev = f4ColorOTF1.a;
#ifdef THREE_SIMULTANEOUS_MIX
		float4 f4ColorOTF2 = vxComputeVisibility(f3PosSampleWS, f3VecLightWS, f3VecSampleDirUnitWS, g_cbVolObj_Ex2, g_cbModulation_Ex2, fAlphaOTF2_Prev, g_f4bufOTF_Ex2, g_tex3DVolume_Ex2, g_tex3DVolumeMask, fOpacityCorrection2);
		fAlphaOTF2_Prev = f4ColorOTF2.a;
		f4ColorOTF = vxFusionVisibility(f4ColorOTF, f4ColorOTF2);
#endif

		if(f4ColorOTF.a > 0)
		{
			fOut_DepthExit = fDepthSampleStart + fSampleDistMinWS*(float)(i + 1);

			if(iIndexOfPrevOutRSA >= iNumPrevOutRSs)
			{
				f4Out_Color += f4ColorOTF * (1.f - f4Out_Color.a);
			}
			else
			{
				// Intermixing
				stRS_Cur.f4Visibility = f4ColorOTF;
				stRS_Cur.fThickness = fSampleDistMinWS;
				stRS_Cur.fDepthBack = fOut_DepthExit;
				while(iIndexOfPrevOutRSA < iNumPrevOutRSs && stRS_Cur.fThickness > 0)
				{
					stRS_PrevOut = arrayPrevOutRSA[iIndexOfPrevOutRSA];
					if(stRS_Cur.fDepthBack > stRS_PrevOut.fDepthBack)
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

					if(f4Out_Color.a > ERT_ALPHA)
					{
						iIndexOfPrevOutRSA = iNumPrevOutRSs; /*This implies ERT as well*/
						fOut_DepthExit = stIKOut.stIntegrationRS.fDepthBack;
						stRS_Cur.fThickness = 0;
						break;
					}
					else
					{
						if(stRS_Cur.fDepthBack > stRS_PrevOut.fDepthBack)
						{
							stRS_Cur = stIKOut.stRemainedRS;
							iIndexOfPrevOutRSA++;
						}
						else
						{
							arrayPrevOutRSA[iIndexOfPrevOutRSA] = stIKOut.stRemainedRS;
							stRS_Cur.fThickness = 0;
							if(stIKOut.stRemainedRS.fThickness == 0)
								iIndexOfPrevOutRSA++;
						}
					}
				} // End of This loop, There is no more surfaces before the slab

				if(stRS_Cur.fThickness > 0)	// This implies f4Out_Color.a <= ERT_ALPHA
				{
					// There is no Prev Surface Layers but still Slab has opacity
					f4Out_Color += stRS_Cur.f4Visibility * (1.f - f4Out_Color.a);
					fOut_DepthExit = stRS_Cur.fDepthBack;
				}
			}
			if(f4Out_Color.a > ERT_ALPHA)
			{
				i = iNumSamples + 1;
				break;
			}
		}
	}

	for(; iIndexOfPrevOutRSA < iNumPrevOutRSs; iIndexOfPrevOutRSA++)
	{
		stRS_PrevOut = arrayPrevOutRSA[iIndexOfPrevOutRSA];
		f4Out_Color += stRS_PrevOut.f4Visibility * (1.f - f4Out_Color.a);
		fOut_DepthExit = stRS_PrevOut.fDepthBack;
	}
	fOut_Depth1stHit = min(arrayPrevOutRSA[0].fDepthBack/* - arrayPrevOutRSA[0].fThickness*/, fOut_Depth1stHit);

#ifndef VR_LAYER_OUT
	g_RWB_RenderOut[uiSampleAddr].iColor = vxConvertColorToInt(f4Out_Color);
	g_RWB_RenderOut[uiSampleAddr].fDepth1stHit = fOut_Depth1stHit;
	//g_RWB_RenderOut[uiSampleAddr].fDepthExit = fOut_DepthExit;
#endif
}