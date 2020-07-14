#include "ShaderCommonHeader.hlsl"

cbuffer cbGlobalParams : register( b0 )	 // SLOT 0 for DVR 나중에 Superimpose 용 따로 하나...
{
	VxVolumeObject g_cbVolObj;
}

cbuffer cbGlobalParams : register( b1 )
{
	VxCameraProjectionState g_cbCamProjState;
}

cbuffer cbGlobalParams : register( b10 )
{
	VxProxyParameter g_cbProxy;
}

cbuffer cbGlobalParams : register( b11 )
{
	VxCameraState g_cbCamState;
}

struct PS_OUTPUT_MERGE
{
	float4 f4PsRaySegment0 : SV_TARGET0;
	float4 f4PsRaySegment1 : SV_TARGET1;
	float4 f4PsRaySegment2 : SV_TARGET2;
	float4 f4PsRaySegment3 : SV_TARGET3;
#if (NUM_MERGE_LAYERS == 8)
	float4 f4PsRaySegment4 : SV_TARGET4;
	float4 f4PsRaySegment5 : SV_TARGET5;
	float4 f4PsRaySegment6 : SV_TARGET6;
	float4 f4PsRaySegment7 : SV_TARGET7;
#endif
};

struct VS_OUTPUT
{
	float4 f4PosSS : SV_POSITION;
};

struct PS_OUTPUT
{
	float4 f4Color : SV_TARGET0; // UNORM
	float fDepthWS : SV_TARGET1;
};

Texture2D g_tex2DPrevRgbaLayersRSA[NUM_MERGE_LAYERS] : register(t90);	// rgba (float)

// NUM_TEXRT_LAYERS is same as SR's main loop period to merge rendered textures //
// RTT
Texture2D g_tex2DPrevRgbaLayers[NUM_TEXRT_LAYERS] : register(t30);	// rgba (unorm)
Texture2D g_tex2DPrevDepthLayers[NUM_TEXRT_LAYERS] : register(t60);	
Texture2D g_tex2DPrevThicknessLayers[NUM_TEXRT_LAYERS / 2] : register(t80);

// PS //
VS_OUTPUT ProxyVS_Point( float3 f3Pos : POSITION )
{
	VS_OUTPUT vxOut = (VS_OUTPUT) 0;
	vxOut.f4PosSS = mul( float4(f3Pos, 1.f), g_cbProxy.matOS2PS );
	return vxOut;
}

#if defined(SROUT_AND_LAYEROUT_TO_LAYEROUT)
RWStructuredBuffer<CS_Output_MultiLayers> g_RWB_MultiLayerOut : register( u0 );
#elif defined(LAYEROUT_TO_RENDEROUT)
RWStructuredBuffer<CS_Output_FB> g_RWB_RenderOut : register( u0 );
#define EARLY_RETURN_RENDEROUT \
		int iNumPrevOutRSs = 0;	\
		[loop]	\
		for(i = 0; i < NUM_MERGE_LAYERS; i++)	\
		{	\
			float4 f4ColorSeg = vxConvertColorToFloat4(stOut2ndPhaseBuffer.arrayIntVisibilityRSA[i]);	\
			if(f4ColorSeg.a > 0)	\
			{	\
				iNumPrevOutRSs++;	\
				f4Out_Color += f4ColorSeg * (1.f - f4Out_Color.a);	\
				fOut_DepthExit = stOut2ndPhaseBuffer.arrayDepthBackRSA[i];	\
				if(f4Out_Color.a > ERT_ALPHA)	\
					break;	\
			}	\
		}	\
		if(iNumPrevOutRSs > 0)	\
			fOut_Depth1stHit = stOut2ndPhaseBuffer.arrayDepthBackRSA[0];/* - stOut2ndPhaseBuffer.arrayThicknessRSA[0];	\*/\
		\
		g_RWB_RenderOut[uiSampleAddr].iColor = vxConvertColorToInt(f4Out_Color);	\
		g_RWB_RenderOut[uiSampleAddr].fDepth1stHit = fOut_Depth1stHit;	\
		//g_RWB_RenderOut[uiSampleAddr].fDepthExit = fOut_DepthExit;
#endif


#define CONVERT_FROM_PsRAYSEGMENT_TO_COLOR(f4Color, f4PsRaySegment) {\
	uint iR = (uint)f4PsRaySegment.r;\
	uint iG = (uint)f4PsRaySegment.g;\
	f4Color = float4((float)(iR / 1000) / 255.f, (float)(iR % 1000) / 255.f, (float)(iG / 1000) / 255.f, (float)(iG % 1000) / 255.f);\
}

#define CONVERT_FROM_RaySegment_TO_PsRAYSEGMENT(f4PsRaySegment, f4Color, fDepthBack, fThickness) {\
	if(fThickness == 0){\
		f4PsRaySegment = float4(0, 0, FLT_MAX, 0);\
	}\
	else {\
		f4PsRaySegment.x = (int)(f4Color.r * 255) * 1000 + (int)(f4Color.g * 255);\
		f4PsRaySegment.y = (int)(f4Color.b * 255) * 1000 + (int)(f4Color.a * 255);\
		f4PsRaySegment.z = fDepthBack;\
		f4PsRaySegment.w = fThickness;\
	}\
}

//
// SROUT_AND_LAYEROUT_TO_LAYEROUT : SROUT + LAYEROUT -> LAYEROUT (CS and PS)
// LAYEROUT_TO_RENDEROUT : LAYEROUT -> RENDEROUT (CS and PS)
#if defined(PROXY_PS)
#ifdef LAYEROUT_TO_RENDEROUT
PS_OUTPUT MergeLayers( VS_OUTPUT input )
#else
PS_OUTPUT_MERGE MergeLayers( VS_OUTPUT input )
#endif
#else
[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void MergeLayers( uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex )
#endif
{
#if defined(PROXY_PS) && !defined(LAYEROUT_TO_RENDEROUT)
	PS_OUTPUT_MERGE stOut = (PS_OUTPUT_MERGE)0;
#elif !defined(PROXY_PS)
	if(DTid.x >= g_cbCamState.uiScreenSizeX || DTid.y >= g_cbCamState.uiScreenSizeY)
		return;
	uint uiSampleAddr = DTid.x + DTid.y*g_cbCamState.uiGridOffsetX;
#endif
	
	int i, j;
	VxRaySegment stCleanRS = (VxRaySegment)0;
	stCleanRS.fDepthBack = FLT_MAX;

#ifdef LAYEROUT_TO_RENDEROUT
	float4 f4Out_Color = (float4)0;
	float fOut_Depth1stHit = FLT_MAX;
	float fOut_DepthExit = FLT_MAX;
	// Only for DEFAULT (BRICK) // CLIPSEMIOPAQUE // MODULATION // SHADOW (BRICK) // DEFAULT_SCULPTMASK
#ifdef PROXY_PS
	int iNumPrevOutRSs = 0;
	[unroll]
	for(i = 0; i < NUM_MERGE_LAYERS; i++)
	{
		float4 f4PsRaySegment = g_tex2DPrevRgbaLayersRSA[i].Load( int3(input.f4PosSS.xy, 0) ).rgba;
		
		if(f4PsRaySegment.w > 0)	// Thickness Check //
		{
			float4 f4ColorSeg;
			CONVERT_FROM_PsRAYSEGMENT_TO_COLOR(f4ColorSeg, f4PsRaySegment);
			fOut_DepthExit = f4PsRaySegment.z;

			iNumPrevOutRSs++;
			f4Out_Color += f4ColorSeg * (1.f - f4Out_Color.a);
			if(f4Out_Color.a > ERT_ALPHA)
				break;
		}
	}
	if(iNumPrevOutRSs > 0)
	{
		float4 f4PsRaySegment = g_tex2DPrevRgbaLayersRSA[0].Load(int3(input.f4PosSS.xy, 0)).rgba;
		fOut_Depth1stHit = f4PsRaySegment.z;// -f4PsRaySegment.w;
	}
	
	//if(iNumPrevOutRSs > 0)
	//	f4Out_Color = float4(1, 0, 0, 1);
	//else if(iNumPrevOutRSs == 1)
	//	f4Out_Color = float4(1, 0, 0, 1);
	//else if(iNumPrevOutRSs == 2)
	//	f4Out_Color = float4(0, 1, 0, 1);
	//else if(iNumPrevOutRSs == 3)
	//	f4Out_Color = float4(0, 0, 1, 1);
	
	PS_OUTPUT stOut;
	stOut.f4Color = f4Out_Color;
	stOut.fDepthWS = fOut_Depth1stHit;
	return stOut;
#else
	CS_Output_MultiLayers stOut2ndPhaseBuffer = g_RWB_PrevLayers[uiSampleAddr];
	EARLY_RETURN_RENDEROUT;
#endif

#if defined(TEST) && !defined(PROXY_PS)
	int iTestIndex = (int)((g_cbCamState.iFlags >> 10) & 0xF);
	if(iTestIndex > 0)
	{
		if(iTestIndex < 5)
		{
			switch(iTestIndex)
			{
			case 1: g_RWB_RenderOut[uiSampleAddr].iColor = stOut2ndPhaseBuffer.arrayIntVisibilityRSA[0];	break;
			case 2: g_RWB_RenderOut[uiSampleAddr].iColor = stOut2ndPhaseBuffer.arrayIntVisibilityRSA[1];	break;
			case 3: g_RWB_RenderOut[uiSampleAddr].iColor = stOut2ndPhaseBuffer.arrayIntVisibilityRSA[2];	break;
			case 4: g_RWB_RenderOut[uiSampleAddr].iColor = stOut2ndPhaseBuffer.arrayIntVisibilityRSA[3];	break;
			default: /*g_RWB_RenderOut[uiSampleAddr].iColor = vxConvertColorToInt(f4COIntegrated);*/break;
			}
		}
		else if(iTestIndex < 10)
		{
			switch(iNumPrevOutRSs)
			{
			case 1: g_RWB_RenderOut[uiSampleAddr].iColor = vxConvertColorToInt(float4(1,0,0,1));	break;
			case 2: g_RWB_RenderOut[uiSampleAddr].iColor = vxConvertColorToInt(float4(0,1,0,1));	break;
			case 3: g_RWB_RenderOut[uiSampleAddr].iColor = vxConvertColorToInt(float4(0,0,1,1));	break;
			case 4: g_RWB_RenderOut[uiSampleAddr].iColor = vxConvertColorToInt(float4(1,1,1,1));	break;
			case 0: g_RWB_RenderOut[uiSampleAddr].iColor = vxConvertColorToInt(float4(0.5,0.5,0.5,1));	break;
			default: /*g_RWB_RenderOut[uiSampleAddr].iColor = vxConvertColorToInt(f4COIntegrated);*/break;
			}
		}
		else
		{
			bool bFlag = false;
			for(i = 0; i < NUM_MERGE_LAYERS; i++)
			{
				float4 f4Color = vxConvertColorToFloat4(stOut2ndPhaseBuffer.arrayIntVisibilityRSA[i]);
				if(f4Color.a == 0 && i >= 1)
				{
					bFlag = true;
				}
				if(bFlag && f4Color.a > 0)
				{
					switch(i)
					{
					case 1: g_RWB_RenderOut[uiSampleAddr].iColor = vxConvertColorToInt(float4(1,0,0,1));	break;
					case 2: g_RWB_RenderOut[uiSampleAddr].iColor = vxConvertColorToInt(float4(0,1,0,1));	break;
					case 3: g_RWB_RenderOut[uiSampleAddr].iColor = vxConvertColorToInt(float4(0,0,1,1));	break;
					case 4: g_RWB_RenderOut[uiSampleAddr].iColor = vxConvertColorToInt(float4(1,1,1,1));	break;
					case 0: g_RWB_RenderOut[uiSampleAddr].iColor = vxConvertColorToInt(float4(0.5,0.5,0.5,1));	break;
					default: /*g_RWB_RenderOut[uiSampleAddr].iColor = vxConvertColorToInt(f4COIntegrated);*/break;
					}
					break;
				}
			}
		}
	}
	return;
#endif
#else
	
	VxRaySegment arrayCurrentRSA[NUM_TEXRT_LAYERS];	// of which each has same thickness //
	int iCountValidInputFromTextures = 0;
#if defined(SROUT_AND_LAYEROUT_TO_LAYEROUT)
	////////////////// SR_RESULT
	float fThicknessSurface = g_cbCamProjState.fVThicknessGlobal;
	[unroll]
	for(i = 0; i < NUM_TEXRT_LAYERS; i++)
	{
#ifdef PROXY_PS
		int2 i2PosSS = int2(input.f4PosSS.xy);
		float4 f4ColorIn = (float4)0;
		float fDepthWS = FLT_MAX;
#ifdef PROXY_PS_LOD_2X	// Only for CPU VR
		if (i < 1)
		{
			int iModX = (uint)i2PosSS.x % 2;
			int iModY = (uint)i2PosSS.y % 2;
			if (iModX + iModY == 0)
			{
				f4ColorIn = g_tex2DPrevRgbaLayers[i].Load(int3(i2PosSS.xy, 0)).rgba;
				fDepthWS = g_tex2DPrevDepthLayers[i].Load(int3(i2PosSS.xy, 0)).r;
				fThicknessSurface = g_tex2DPrevThicknessLayers[i].Load(int3(i2PosSS.xy, 0)).r;
			}
			else
			{
				if (iModX + iModY == 2)
				{
					float4 f4Color0 = g_tex2DPrevRgbaLayers[i].Load(int3(i2PosSS.xy - int2(1, 1), 0)).rgba;
					float4 f4Color1 = g_tex2DPrevRgbaLayers[i].Load(int3(i2PosSS.xy + int2(1, 1), 0)).rgba;
					f4ColorIn = (f4Color0 + f4Color1) * 0.5f;
					float fDepthWS0 = g_tex2DPrevDepthLayers[i].Load(int3(i2PosSS.xy - int2(1, 1), 0)).r;
					float fDepthWS1 = g_tex2DPrevDepthLayers[i].Load(int3(i2PosSS.xy + int2(1, 1), 0)).r;
					fDepthWS = (fDepthWS0 + fDepthWS1) * 0.5f;
					float fThicknessSurface0 = g_tex2DPrevThicknessLayers[i].Load(int3(i2PosSS.xy - int2(1, 1), 0)).r;
					float fThicknessSurface1 = g_tex2DPrevThicknessLayers[i].Load(int3(i2PosSS.xy + int2(1, 1), 0)).r;
					fThicknessSurface = (fThicknessSurface0 + fThicknessSurface1) * 0.5;
				}
				else if (iModX == 1)
				{
					float4 f4Color0 = g_tex2DPrevRgbaLayers[i].Load(int3(i2PosSS.xy - int2(1, 0), 0)).rgba;
					float4 f4Color1 = g_tex2DPrevRgbaLayers[i].Load(int3(i2PosSS.xy + int2(1, 0), 0)).rgba;
					f4ColorIn = (f4Color0 + f4Color1) * 0.5f;
					float fDepthWS0 = g_tex2DPrevDepthLayers[i].Load(int3(i2PosSS.xy - int2(1, 0), 0)).r;
					float fDepthWS1 = g_tex2DPrevDepthLayers[i].Load(int3(i2PosSS.xy + int2(1, 0), 0)).r;
					fDepthWS = (fDepthWS0 + fDepthWS1) * 0.5f;
					float fThicknessSurface0 = g_tex2DPrevThicknessLayers[i].Load(int3(i2PosSS.xy - int2(1, 0), 0)).r;
					float fThicknessSurface1 = g_tex2DPrevThicknessLayers[i].Load(int3(i2PosSS.xy + int2(1, 0), 0)).r;
					fThicknessSurface = (fThicknessSurface0 + fThicknessSurface1) * 0.5;
				}
				else if (iModY == 1)
				{
					float4 f4Color0 = g_tex2DPrevRgbaLayers[i].Load(int3(i2PosSS.xy - int2(0, 1), 0)).rgba;
					float4 f4Color1 = g_tex2DPrevRgbaLayers[i].Load(int3(i2PosSS.xy + int2(0, 1), 0)).rgba;
					f4ColorIn = (f4Color0 + f4Color1) * 0.5f;
					float fDepthWS0 = g_tex2DPrevDepthLayers[i].Load(int3(i2PosSS.xy - int2(0, 1), 0)).r;
					float fDepthWS1 = g_tex2DPrevDepthLayers[i].Load(int3(i2PosSS.xy + int2(0, 1), 0)).r;
					fDepthWS = (fDepthWS0 + fDepthWS1) * 0.5f;
					float fThicknessSurface0 = g_tex2DPrevThicknessLayers[i].Load(int3(i2PosSS.xy - int2(0, 1), 0)).r;
					float fThicknessSurface1 = g_tex2DPrevThicknessLayers[i].Load(int3(i2PosSS.xy + int2(0, 1), 0)).r;
					fThicknessSurface = (fThicknessSurface0 + fThicknessSurface1) * 0.5;
				}
			}

			if (f4ColorIn.a > 0)
			{
				f4ColorIn.rgb /= f4ColorIn.a;
			}
		} // if (i < 1)
#else
		f4ColorIn = g_tex2DPrevRgbaLayers[i].Load(int3(i2PosSS.xy, 0)).rgba;
		fDepthWS = g_tex2DPrevDepthLayers[i].Load(int3(i2PosSS.xy, 0)).r;
		
		if (((g_cbCamProjState.iFlag >> 16) & 0x1) && f4ColorIn.a > 0 && i < NUM_CPU_TEXRT_LAYERS)
		{
			fThicknessSurface = g_tex2DPrevThicknessLayers[i].Load(int3(i2PosSS.xy, 0)).r;
			f4ColorIn.rgb /= f4ColorIn.a;
		}
#endif
#else
		// !PROXY_PS means CS
		float4 f4ColorIn = g_tex2DPrevRgbaLayers[i].Load( int3(DTid.xy, 0) ).rgba;
		float fDepthWS = g_tex2DPrevDepthLayers[i].Load( int3(DTid.xy, 0) ).r;
#endif
		
		VxRaySegment stInputRS;
		if(f4ColorIn.a > 0)
		{
			iCountValidInputFromTextures++;
			// Association Color //
			stInputRS.f4Visibility = float4(f4ColorIn.rgb * f4ColorIn.a, min(f4ColorIn.a, SAFE_OPAQUEALPHA));
			//if(fDepthWS == 0)	// Anti Aliasing ?!
			//{
			//	fDepthWS = max(g_tex2DPrevDepthLayers[i].Load( int3(input.f4PosSS.xy + float2(1, 0), 0) ).r, fDepthWS);
			//	fDepthWS = max(g_tex2DPrevDepthLayers[i].Load( int3(input.f4PosSS.xy + float2(0, 1), 0) ).r, fDepthWS);
			//	fDepthWS = max(g_tex2DPrevDepthLayers[i].Load( int3(input.f4PosSS.xy + float2(1, 1), 0) ).r, fDepthWS);
			//	fDepthWS = max(g_tex2DPrevDepthLayers[i].Load( int3(input.f4PosSS.xy + float2(-1, 0), 0) ).r, fDepthWS);
			//	fDepthWS = max(g_tex2DPrevDepthLayers[i].Load( int3(input.f4PosSS.xy + float2(0, -1), 0) ).r, fDepthWS);
			//	fDepthWS = max(g_tex2DPrevDepthLayers[i].Load( int3(input.f4PosSS.xy + float2(-1, -1), 0) ).r, fDepthWS);
			//}

			stInputRS.fDepthBack = fDepthWS;
			stInputRS.fThickness = fThicknessSurface;
		}
		else
		{
			stInputRS = stCleanRS;
		}
		arrayCurrentRSA[i] = stInputRS;
	}
#endif

	float4 f4Out_Color = (float4)0;
	float fOut_Depth1stHit = FLT_MAX;
	float fOut_DepthExit = FLT_MAX;

#ifdef PROXY_PS
	if(iCountValidInputFromTextures == 0)
	{
		// Also, imply 2nd phase buffer initialization //
		stOut.f4PsRaySegment0 = g_tex2DPrevRgbaLayersRSA[0].Load( int3(input.f4PosSS.xy, 0) ).rgba;
		if(stOut.f4PsRaySegment0.w == 0)
			stOut.f4PsRaySegment0 = float4(0, 0, FLT_MAX, 0);

		stOut.f4PsRaySegment1 = g_tex2DPrevRgbaLayersRSA[1].Load( int3(input.f4PosSS.xy, 0) ).rgba;
		if(stOut.f4PsRaySegment1.w == 0)
			stOut.f4PsRaySegment1 = float4(0, 0, FLT_MAX, 0);
		
		stOut.f4PsRaySegment2 = g_tex2DPrevRgbaLayersRSA[2].Load( int3(input.f4PosSS.xy, 0) ).rgba;
		if(stOut.f4PsRaySegment2.w == 0)
			stOut.f4PsRaySegment2 = float4(0, 0, FLT_MAX, 0);
		
		stOut.f4PsRaySegment3 = g_tex2DPrevRgbaLayersRSA[3].Load( int3(input.f4PosSS.xy, 0) ).rgba;
		if(stOut.f4PsRaySegment3.w == 0)
			stOut.f4PsRaySegment3 = float4(0, 0, FLT_MAX, 0);

#if (NUM_MERGE_LAYERS == 8)
		stOut.f4PsRaySegment4 = g_tex2DPrevRgbaLayersRSA[4].Load( int3(input.f4PosSS.xy, 0) ).rgba;
		if(stOut.f4PsRaySegment4.w == 0)
			stOut.f4PsRaySegment4 = float4(0, 0, FLT_MAX, 0);
	
		stOut.f4PsRaySegment5 = g_tex2DPrevRgbaLayersRSA[5].Load( int3(input.f4PosSS.xy, 0) ).rgba;
		if(stOut.f4PsRaySegment5.w == 0)
			stOut.f4PsRaySegment5 = float4(0, 0, FLT_MAX, 0);
		
		stOut.f4PsRaySegment6 = g_tex2DPrevRgbaLayersRSA[6].Load( int3(input.f4PosSS.xy, 0) ).rgba;
		if(stOut.f4PsRaySegment6.w == 0)
			stOut.f4PsRaySegment6 = float4(0, 0, FLT_MAX, 0);
		
		stOut.f4PsRaySegment7 = g_tex2DPrevRgbaLayersRSA[7].Load( int3(input.f4PosSS.xy, 0) ).rgba;
		if(stOut.f4PsRaySegment7.w == 0)
			stOut.f4PsRaySegment7 = float4(0, 0, FLT_MAX, 0);
#endif

		return stOut;
	}
#else
	CS_Output_MultiLayers stOut2ndPhaseBuffer = g_RWB_PrevLayers[uiSampleAddr];
	if(iCountValidInputFromTextures == 0)
	{
		// Also, imply 2nd phase buffer initialization //
		for(int i = 0; i < NUM_MERGE_LAYERS; i++)
		{
			if(stOut2ndPhaseBuffer.arrayThicknessRSA[i] == 0)
			{
				stOut2ndPhaseBuffer.arrayIntVisibilityRSA[i] = 0;
				stOut2ndPhaseBuffer.arrayDepthBackRSA[i] = FLT_MAX;
				stOut2ndPhaseBuffer.arrayThicknessRSA[i] = 0;
			}
		}
		g_RWB_MultiLayerOut[uiSampleAddr] = stOut2ndPhaseBuffer;
		return;
	}
#endif

	// Sorting into vxInputSlabs simply using bubble sort algorithm //
	[loop]
	for(i = 0; i < NUM_TEXRT_LAYERS; i++)
	{
		for(j = NUM_TEXRT_LAYERS - 1; j >= i + 1; j--)
		{
			if (arrayCurrentRSA[j].fDepthBack < arrayCurrentRSA[j - 1].fDepthBack) 
			{
				VxRaySegment stTmpRS = arrayCurrentRSA[j];
				arrayCurrentRSA[j] = arrayCurrentRSA[j - 1];
				arrayCurrentRSA[j - 1] = stTmpRS;
			}
		}
	}
	
	// merge sorted thickness-surfaces
	float4 f4IntermixingVisibility = (float4)0;
	VxRaySegment stPriorRS, stPosteriorRS;
	VxIntermixingKernelOut stIKOut;
	int iNumElementsOfCurRSA = 0;
	for(i = 0; i < iCountValidInputFromTextures; i++)
	{
		VxRaySegment stRS_Cur = arrayCurrentRSA[i];
		VxRaySegment stRS_Next;

		if(i + 1 < NUM_TEXRT_LAYERS)
		{
			stRS_Next = arrayCurrentRSA[i + 1];
		}
		else
		{
			stRS_Next = stCleanRS;
		}

		if(stRS_Cur.fDepthBack > stRS_Next.fDepthBack)
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

		if(stIKOut.stIntegrationRS.fThickness > 0)
		{
			f4IntermixingVisibility = stIKOut.f4IntermixingVisibility;
			arrayCurrentRSA[iNumElementsOfCurRSA] = stIKOut.stIntegrationRS;
			iNumElementsOfCurRSA++;

			if(f4IntermixingVisibility.a > ERT_ALPHA)
				break;
			
			if(stIKOut.stRemainedRS.fThickness > 0)
			{
				if(i + 1 < NUM_TEXRT_LAYERS)
					arrayCurrentRSA[i + 1] = stIKOut.stRemainedRS;
			}
			else
			{
				i += 1;
			}
		}
	}

	for(i = iNumElementsOfCurRSA; i < NUM_TEXRT_LAYERS; i++)
	{
		arrayCurrentRSA[i] = stCleanRS;
	}
	
	// intermix two different sorted thickness-surfaces
	VxRaySegment arrayPrevOutRSA[NUM_MERGE_LAYERS];
	int iNumPrevOutRSs = 0;
#ifdef PROXY_PS
	[unroll]
	for(i = 0; i < NUM_MERGE_LAYERS; i++)
	{
		float4 f4PsRaySegment = g_tex2DPrevRgbaLayersRSA[i].Load( int3(input.f4PosSS.xy, 0) ).rgba;
		if(f4PsRaySegment.w > 0)	// Thickness Check //
		{
			float4 f4ColorIn;
			CONVERT_FROM_PsRAYSEGMENT_TO_COLOR(f4ColorIn, f4PsRaySegment);
			float2 f2DepthThickness = f4PsRaySegment.zw;

			VxRaySegment stRS;
			stRS.f4Visibility = f4ColorIn;
			stRS.fDepthBack = f2DepthThickness.x;
			stRS.fThickness = f2DepthThickness.y;

			arrayPrevOutRSA[i] = stRS;
			iNumPrevOutRSs++;
		}
		else
		{
			arrayPrevOutRSA[i] = stCleanRS;
		}
	}

	VXMIXMAIN;

	for(i = iCountStoredBuffer; i < NUM_MERGE_LAYERS; i++)
	{
		arrayOutRSA[i] = stCleanRS;
	}
	
	VxRaySegment stRS = arrayOutRSA[0];
	CONVERT_FROM_RaySegment_TO_PsRAYSEGMENT(stOut.f4PsRaySegment0, stRS.f4Visibility, stRS.fDepthBack, stRS.fThickness);
	stRS = arrayOutRSA[1];
	CONVERT_FROM_RaySegment_TO_PsRAYSEGMENT(stOut.f4PsRaySegment1, stRS.f4Visibility, stRS.fDepthBack, stRS.fThickness);
	stRS = arrayOutRSA[2];
	CONVERT_FROM_RaySegment_TO_PsRAYSEGMENT(stOut.f4PsRaySegment2, stRS.f4Visibility, stRS.fDepthBack, stRS.fThickness);
	stRS = arrayOutRSA[3];
	CONVERT_FROM_RaySegment_TO_PsRAYSEGMENT(stOut.f4PsRaySegment3, stRS.f4Visibility, stRS.fDepthBack, stRS.fThickness);

#if (NUM_MERGE_LAYERS == 8)
	stRS = arrayOutRSA[4];
	CONVERT_FROM_RaySegment_TO_PsRAYSEGMENT(stOut.f4PsRaySegment4, stRS.f4Visibility, stRS.fDepthBack, stRS.fThickness);
	stRS = arrayOutRSA[5];
	CONVERT_FROM_RaySegment_TO_PsRAYSEGMENT(stOut.f4PsRaySegment5, stRS.f4Visibility, stRS.fDepthBack, stRS.fThickness);
	stRS = arrayOutRSA[6];
	CONVERT_FROM_RaySegment_TO_PsRAYSEGMENT(stOut.f4PsRaySegment6, stRS.f4Visibility, stRS.fDepthBack, stRS.fThickness);
	stRS = arrayOutRSA[7];
	CONVERT_FROM_RaySegment_TO_PsRAYSEGMENT(stOut.f4PsRaySegment7, stRS.f4Visibility, stRS.fDepthBack, stRS.fThickness);
#endif
	return stOut;

#else
	[unroll]
	for(i = 0; i < NUM_MERGE_LAYERS; i++)
	{
		float4 f4ColorIn = vxConvertColorToFloat4(stOut2ndPhaseBuffer.arrayIntVisibilityRSA[i]);
		if(f4ColorIn.a > 0)
		{
			VxRaySegment stRS;
			stRS.f4Visibility = f4ColorIn;
			stRS.fDepthBack = stOut2ndPhaseBuffer.arrayDepthBackRSA[i];
			stRS.fThickness = stOut2ndPhaseBuffer.arrayThicknessRSA[i];

			arrayPrevOutRSA[i] = stRS;
			iNumPrevOutRSs++;
		}
		else
		{
			arrayPrevOutRSA[i] = stCleanRS;
		}
	}
	VXMIXOUT;
	return;
#endif
#endif	// LAYEROUT_TO_RENDEROUT
}