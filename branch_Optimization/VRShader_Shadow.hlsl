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

cbuffer cbGlobalParams : register( b6 )	 // SLOT 6
{
	VxBrickChunk g_cbBlkVolume;
}

cbuffer cbGlobalParams : register( b7 )	 // SLOT 7
{
	VxShadowMap g_cbShadowMap;
}

//=====================
// Texture Buffers
//=====================
Buffer<float4> g_f4bufOTF : register(t0);		// Mask OTFs
Buffer<int> g_ibufOTFIndex : register(t3);		// Mask OTFs

Texture3D g_tex3DVolume : register(t1);
Texture3D g_tex3DBlock : register(t2);
Texture3D g_tex3DVolumeMask : register(t8);

StructuredBuffer<CS_Output_ShadowMap> g_RWB_ShadowMapInput : register( t6 );

Texture3D g_tex3DBrickChunks[3] : register(t10);	// Safe out

//==============//
// Main VR Code //
//groupshared float4 shared_data[***]; Group Shared Memory...
RWStructuredBuffer<CS_Output_ShadowMap> g_RWB_ShadowMapOutput : register( u0 );

[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void VRCS_GenShadowMap( uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex )
{
	// Initial Check //
	CS_Output_ShadowMap vxOut = (CS_Output_ShadowMap)0;
	int i = 0;
	vxOut.fDepthHits[0] = FLT_MAX;
	vxOut.fDepthHits[1] = FLT_MAX;

	uint uiSampleAddr = DTid.x + DTid.y*g_cbCamState.uiGridOffsetX;
//#define __DEBUG_GEN
#ifdef __DEBUG_GEN
	g_RWB_Output[uiSampleAddr] = (CS_Output_FB)0;
	g_RWB_Output[uiSampleAddr].f4Color = float4(1, 1, 0, 1);
#else
	g_RWB_ShadowMapOutput[uiSampleAddr] = vxOut;
#endif

	if(DTid.x >= g_cbCamState.uiScreenSizeX || DTid.y >= g_cbCamState.uiScreenSizeY)
		return;
	
	// Image Plane's Position and Camera State //
	float3 f3PosIPSampleSS = float3((float)DTid.x, (float)DTid.y, 0);
	float3 f3PosIPSampleWS = vxTransformPoint(f3PosIPSampleSS, g_cbCamState.matSS2WS);

	float3 f3VecSampleDirUnitWS = normalize(g_cbCamState.f3VecViewWS);
	if(g_cbCamState.iFlags & 0x1)
		f3VecSampleDirUnitWS = normalize(f3PosIPSampleWS - g_cbCamState.f3PosEyeWS);

	// Compute VObject Box Enter and Exit //
	float2 f2PrevNextT = vxVrComputeBoundary(f3PosIPSampleWS, f3VecSampleDirUnitWS, g_cbVolObj);
	if(f2PrevNextT.y <= f2PrevNextT.x)
		return;
	
	// Compute Sample Number //
	int iNumSamples = (int)((f2PrevNextT.y - f2PrevNextT.x) / g_cbVolObj.fSampleDistWS + 0.5f);
	
	// Surface Refinement //
	float3 f3PosBoundaryPrevWS = f3PosIPSampleWS + f3VecSampleDirUnitWS*f2PrevNextT.x;

	VxSurfaceRefinement vxSurfaceOut;
#ifdef SAMPLE_FROM_BRICKCHUNK
	vxSurfaceOut = vxVrSlabSurfaceRefinementWithBlockSkipping(f3PosBoundaryPrevWS, f3VecSampleDirUnitWS, iNumSamples,
		g_cbVolObj, g_cbBlkVolume, g_tex3DBrickChunks, g_cbBlkObj, g_tex3DBlock, g_cbOtf1D);
#else

#ifdef OTF_MASK
	vxSurfaceOut = vxVrSlabSurfaceRefinementWithBlockSkipping(f3PosBoundaryPrevWS, f3VecSampleDirUnitWS, iNumSamples,
		g_cbVolObj, g_tex3DVolume, g_cbBlkObj, g_tex3DBlock, g_cbOtf1D, g_f4bufOTF, g_ibufOTFIndex, g_tex3DVolumeMask);
#else
	vxSurfaceOut = vxVrSlabSurfaceRefinementWithBlockSkipping(f3PosBoundaryPrevWS, f3VecSampleDirUnitWS, iNumSamples,
		g_cbVolObj, g_tex3DVolume, g_cbBlkObj, g_tex3DBlock, g_cbOtf1D);
#endif
#endif

	if(vxSurfaceOut.iHitSampleStep == -1)
		return;
		
	// Sample State Initialization //
	float3 f3VecSampleWS = f3VecSampleDirUnitWS * g_cbVolObj.fSampleDistWS;
	float3 f3VecSampleTS = vxTransformVector( f3VecSampleWS, g_cbVolObj.matWS2TS);
	vxOut.fDepthHits[0] = length(vxSurfaceOut.f3PosSurfaceWS - f3PosIPSampleWS) + g_cbVolObj.fSampleDistWS * g_cbShadowMap.fDepthBiasSampleCount;
	
#ifdef RM_SURFACEEFFECT
	vxOut.fDepthHits[1] = vxOut.fDepthHits[0] + g_cbVolObj.fSampleDistWS;
	vxOut.fOpacity = 1.f;
	g_RWB_ShadowMapOutput[uiSampleAddr] = vxOut;
	return;
#endif

	float3 f3PosBoundaryNextWS = f3PosIPSampleWS + f3VecSampleDirUnitWS * f2PrevNextT.y;
	iNumSamples = (int)(length(vxSurfaceOut.f3PosSurfaceWS - f3PosBoundaryNextWS) / g_cbVolObj.fSampleDistWS + 0.5f);

	// 1st conservation
	float3 f3PosSamplePrevTS = vxTransformPoint(vxSurfaceOut.f3PosSurfaceWS, g_cbVolObj.matWS2TS);
#ifdef SAMPLE_FROM_BRICKCHUNK
	int iSampleValuePrev = (int)(vxSampleInBrickChunk(f3PosSamplePrevTS, g_cbBlkObj, g_tex3DBlock, g_cbBlkVolume, g_tex3DBrickChunks) * g_cbVolObj.fSampleValueRange + 0.5f);
#else
	int iSampleValuePrev = (int)(g_tex3DVolume.SampleLevel(g_samplerLinear, f3PosSamplePrevTS, 0).r * g_cbVolObj.fSampleValueRange + 0.5f);
#endif

	float3 f3PosSampleLastWS = vxSurfaceOut.f3PosSurfaceWS;
	float fIntegratedAlpha = 0;
	[loop]
	for(i = 0; i < iNumSamples; i++)
	{
		float3 f3PosSampleWS = vxSurfaceOut.f3PosSurfaceWS + f3VecSampleWS*(float)(i);
		float3 f3PosSampleTS = vxTransformPoint(f3PosSampleWS, g_cbVolObj.matWS2TS);
		
		VxBlockSkip blkSkip = vxComputeBlockRayLengthBoundary(f3PosSampleTS, f3VecSampleTS, g_cbBlkObj, g_tex3DBlock);
		if(i + blkSkip.iNumStepSkip > iNumSamples)
			blkSkip.iNumStepSkip -= i + blkSkip.iNumStepSkip - iNumSamples;
		blkSkip.iNumStepSkip = max(1, blkSkip.iNumStepSkip);
		
		if(blkSkip.iSampleBlockValue > 0)
		{
			for(int k = 0; k < blkSkip.iNumStepSkip; k++, i++)
			{
				float3 f3PosSampleInBlockWS = vxSurfaceOut.f3PosSurfaceWS + f3VecSampleWS*(float)i;
				float3 f3PosSampleInBlockTS = vxTransformPoint(f3PosSampleInBlockWS, g_cbVolObj.matWS2TS);
#ifdef SAMPLE_FROM_BRICKCHUNK
				int iSampleValueNext = 0;
				float3 f3PosSampleInBlockCTS = vxComputeBrickSamplePosInChunk(f3PosSampleInBlockTS, blkSkip.iSampleBlockValue - 1, g_cbBlkObj, g_cbBlkVolume);
				// This is for precision correction?! because of the difference between "f3VecSampleDirWS*(float)i" and computed block distance with single precision
				if(k < blkSkip.iNumStepSkip - 1)
				{
					float fSampleValue = vxSampleInBrickChunkWithBlockValue(f3PosSampleInBlockCTS, blkSkip.iSampleBlockValue - 1, g_cbBlkVolume, g_tex3DBrickChunks);
					iSampleValueNext = (int)(fSampleValue * g_cbVolObj.fSampleValueRange + 0.5f);
				}
				else
				{
					int3 i3BlockID = int3(f3PosSampleInBlockTS.x / g_cbBlkObj.f3SizeBlockTS.x, f3PosSampleInBlockTS.y / g_cbBlkObj.f3SizeBlockTS.y, f3PosSampleInBlockTS.z / g_cbBlkObj.f3SizeBlockTS.z);
					blkSkip.iSampleBlockValue = (int)(g_tex3DBlock.Load(int4(i3BlockID, 0)).r * g_cbBlkObj.fSampleValueRange + 0.5f);	// New
					f3PosSampleInBlockCTS = vxComputeBrickSamplePosInChunk(f3PosSampleInBlockTS, blkSkip.iSampleBlockValue - 1, g_cbBlkObj, g_cbBlkVolume);
					float fSampleValue = vxSampleInBrickChunkWithBlockValue(f3PosSampleInBlockCTS, blkSkip.iSampleBlockValue - 1, g_cbBlkVolume, g_tex3DBrickChunks);
					iSampleValueNext = (int)(fSampleValue * g_cbVolObj.fSampleValueRange + 0.5f);
				}
#else
				int iSampleValueNext = (int)(g_tex3DVolume.SampleLevel(g_samplerLinear, f3PosSampleInBlockTS, 0).r * g_cbVolObj.fSampleValueRange + 0.5f);
#endif

#ifdef OTF_MASK
				float2 f2SampleMaskValue = vxSampleByCCF(f3PosSampleInBlockTS, g_cbVolObj.f3SizeVolumeCVS, g_tex3DVolumeMask);
				int iMaskValue = (int)(f2SampleMaskValue.x * 255 + 0.5f);
				int iMaskID = g_ibufOTFIndex[iMaskValue];
				float4 f4ColorOTF_Origin = vxLoadPointOtf1DFromBuffer(iSampleValueNext, g_f4bufOTF, g_cbVolObj.fOpaqueCorrection);
				float4 f4ColorOTF_Mask = vxLoadPointOtf1DSeriesFromBuffer(iSampleValueNext, iMaskID, g_f4bufOTF, g_cbVolObj.fOpaqueCorrection);
				float4 f4ColorOTF = f4ColorOTF_Origin * (1.f - f2SampleMaskValue.y) + f4ColorOTF_Mask * f2SampleMaskValue.y;
				float fOtfAlpha = f4ColorOTF.a;
#else
				float fOtfAlpha = vxLoadPointOtf1DFromBuffer(iSampleValueNext, g_f4bufOTF, g_cbShadowMap.fOpaqueCorrectionSMap).a;
#endif
				
				if(fOtfAlpha > 0)
				{
					float fAlphaOld = fIntegratedAlpha;
					fIntegratedAlpha = fAlphaOld + fOtfAlpha*(1.f - fAlphaOld);
					f3PosSampleLastWS = f3PosSampleInBlockWS;

					// To Do
					if(fIntegratedAlpha > ERT_ALPHA)
					{
						i = iNumSamples + 1;
						break;
					}
				}
				iSampleValuePrev = iSampleValueNext;
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
	
	vxOut.fDepthHits[1] = length(f3PosSampleLastWS - f3PosIPSampleWS) + g_cbVolObj.fSampleDistWS * g_cbShadowMap.fDepthBiasSampleCount;
	vxOut.fOpacity = fIntegratedAlpha;

#ifdef __DEBUG_GEN
	g_RWB_Output[uiSampleAddr].f4Color = float4((float3)vxOut.fOpacities[g_cbVolObj.iDummy0___], 1);
#else
	g_RWB_ShadowMapOutput[uiSampleAddr] = vxOut;
#endif
	return;
}