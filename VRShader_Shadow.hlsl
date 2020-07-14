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

struct VS_OUTPUT
{
	float4 f4PosSS : SV_POSITION;
};

struct PS_OUTPUT_SHADOW
{
	float f1stDepthCS : SV_TARGET0; // float
};


//=====================
// Texture Buffers
//=====================
Buffer<float4> g_f4bufOTF : register(t0);		// Mask OTFs
Buffer<int> g_ibufOTFIndex : register(t3);		// Mask OTFs

Texture3D g_tex3DVolume : register(t1);
Texture3D g_tex3DBlock : register(t2);
Texture3D g_tex3DVolumeMask : register(t8);

#ifdef __CS_MODEL
#define RETURN_DEFINED return;
#else
#define RETURN_DEFINED return stOut;
#endif

#ifdef __CS_MODEL
#define SET_OUTPUT(DEPTH_VALUE) \
g_rwt_ShadowMapOut[DTid.xy] = DEPTH_VALUE;
#else
#define SET_OUTPUT(DEPTH_VALUE) \
stOut.f1stDepthCS = DEPTH_VALUE;
#endif

//==============//
// Main VR Code //
#ifdef __CS_MODEL
RWTexture2D<float> g_rwt_ShadowMapOut : register(u1);

[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void CsDVR_ShadowMapGeneration(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
#else
PS_OUTPUT_SHADOW PsDVR_ShadowMapGeneration(VS_OUTPUT input)
#endif
{
#ifdef __CS_MODEL
	if (DTid.x >= g_cbCamState.uiScreenSizeX || DTid.y >= g_cbCamState.uiScreenSizeY)
		return;

	bool bReturnEarly = false;
	int2 i2PosSS;
	uint2 ui2PosRS = DTid.xy;// -uint2(g_cbCamState.uiScreenAaBbMinX, g_cbCamState.uiScreenAaBbMinY);
	if (ui2PosRS.x >= g_cbCamState.uiRenderingSizeX || ui2PosRS.y >= g_cbCamState.uiRenderingSizeY)
		return;
	
	float3 f3PosSS = vxTransformPoint(float3(ui2PosRS, 0), g_cbCamState.matRS2SS);
	i2PosSS = int2(f3PosSS.x, f3PosSS.y);

	const uint uiSampleAddr = i2PosSS.x + i2PosSS.y*g_cbCamState.uiGridOffsetX;
#else
	const uint2 ui2PosSS = int2(input.f4PosSS.xy);

	bool bReturnEarly = false;
	int2 i2PosSS;
	uint2 ui2PosRS = ui2PosSS.xy;// -uint2(g_cbCamState.uiScreenAaBbMinX, g_cbCamState.uiScreenAaBbMinY);
	if (ui2PosRS.x >= g_cbCamState.uiRenderingSizeX || ui2PosRS.y >= g_cbCamState.uiRenderingSizeY)
		return stOut;

	float3 f3PosSS = vxTransformPoint(float3(ui2PosRS, 0), g_cbCamState.matRS2SS);
	i2PosSS = int2(f3PosSS.x, f3PosSS.y);

	PS_OUTPUT_SHADOW stOut;
#endif	// #ifdef __CS_MODEL

	SET_OUTPUT(FLT_MAX);

	int i, j;

		
	// Image Plane's Position and Camera State //
	float3 f3PosIPSampleSS = float3(i2PosSS, 0.0f);
	float3 f3PosIPSampleWS = vxTransformPoint(f3PosIPSampleSS, g_cbCamState.matSS2WS);
	float3 f3VecSampleDirUnitWS = g_cbCamState.f3VecViewWS;
	if (g_cbCamState.iFlags & 0x1)
		f3VecSampleDirUnitWS = f3PosIPSampleWS - g_cbCamState.f3PosEyeWS;
	f3VecSampleDirUnitWS = normalize(f3VecSampleDirUnitWS);
	float3 f3VecSampleWS = f3VecSampleDirUnitWS * g_cbVolObj.fSampleDistWS;

	// Compute VObject Box Enter and Exit //
	float2 f2PrevNextT = vxVrComputeBoundary(f3PosIPSampleWS, f3VecSampleDirUnitWS, g_cbVolObj);
	if (f2PrevNextT.y <= f2PrevNextT.x)
		RETURN_DEFINED

	// Compute Sample Number //
	int iNumSamples = (int)((f2PrevNextT.y - f2PrevNextT.x) / g_cbVolObj.fSampleDistWS + 0.5f);

	// Surface Refinement //
	float3 f3PosBoundaryPrevWS = f3PosIPSampleWS + f3VecSampleDirUnitWS*f2PrevNextT.x;

	VxSurfaceRefinement vxSurfaceOut;
#ifdef OTF_MASK
	vxSurfaceOut = vxVrSlabSurfaceRefinementWithBlockSkipping(f3PosBoundaryPrevWS, f3VecSampleDirUnitWS, iNumSamples,
		g_cbVolObj, g_tex3DVolume, g_cbBlkObj, g_tex3DBlock, g_cbOtf1D, g_f4bufOTF, g_ibufOTFIndex, g_tex3DVolumeMask);
#else
	vxSurfaceOut = vxVrSlabSurfaceRefinementWithBlockSkipping(f3PosBoundaryPrevWS, f3VecSampleDirUnitWS, iNumSamples,
		g_cbVolObj, g_tex3DVolume, g_cbBlkObj, g_tex3DBlock, g_cbOtf1D);
#endif

	if (vxSurfaceOut.iHitSampleStep == -1)
		RETURN_DEFINED

	// Sample State Initialization //
	float3 f3VecSampleTS = vxTransformVector(f3VecSampleWS, g_cbVolObj.matWS2TS);
	float fShadowMapDepth = length(vxSurfaceOut.f3PosSurfaceWS - f3PosIPSampleWS) + g_cbVolObj.fSampleDistWS * g_cbShadowMap.fDepthBiasSampleCount;

	SET_OUTPUT(fShadowMapDepth)
	RETURN_DEFINED
}
