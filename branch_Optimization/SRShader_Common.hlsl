#include "ShaderCommonHeader.hlsl"

//=====================
// Constant Buffers
//=====================
cbuffer cbGlobalParams : register( b0 )	 // SLOT 0
{
	VxPolygonObject g_cbPolygonObj;
}

cbuffer cbGlobalParams : register( b1 )	 // SLOT 1
{
	VxCameraProjectionState g_cbCamState;
}

cbuffer cbGlobalParams : register( b2 )	 // SLOT 2
{
	VxVolumeObject g_cbVolObj;
}

cbuffer cbGlobalParams : register( b3 )	 // SLOT 3
{
	VxBlockObject g_cbBlkObj;
}

cbuffer cbGlobalParams : register( b4 )	 // SLOT 4
{
	VxBrickChunk g_cbBlkChunk;
}

cbuffer cbGlobalParams : register( b5 )	 // SLOT 5
{
	VxPolygonDeviation g_cbPolygonDeviation;
}

cbuffer cbGlobalParams : register( b7 )	 // SLOT 7
{
	VxOtf1D g_cbOtf1D;
}

cbuffer cbGlobalParams : register( b8 )	 // SLOT 8
{
	VxInterestingRegion g_cbInterestingRegion;
}

Buffer<float4> g_f4bufOTF : register(t0);		// Mask OTFs StructuredBuffer
//StructuredBuffer<float4> g_gstb_OTF : register(t15);		// Mask OTFs StructuredBuffer
Texture3D g_tex3DVolume : register(t1);
Texture3D g_tex3DBlock : register(t2);	// For Deviation
Texture2D g_tex2DTextCMM : register(t3);
Texture2D g_tex2DDepthDual : register(t4);
Texture2DArray g_tex2DImageStack : register(t10);

//struct VXAirwayControlPoints
//{
//	float3 f3PosPoint;
//	float3 f3VecTangent;
//	float fArea;
//};

//StructuredBuffer<VXAirwayControlPoints> g_stb_AirwayControls : register(t20);		// (x,y,z,area)
Buffer<float4> g_f4bufPointXYZandAreaW : register(t20);		// (x,y,z,area)

struct VS_INPUT_PN
{
	float3 f3PosOS : POSITION;
	float3 f3VecNormalOS : NORMAL;
};

struct VS_INPUT_PT
{
	float3 f3PosOS : POSITION;
	float3 f3Custom : TEXCOORD0;
};

struct VS_INPUT_PTTT
{
	float3 f3PosOS : POSITION;
	float3 f3Custom0 : TEXCOORD0;
	float3 f3Custom1 : TEXCOORD1;
	float3 f3Custom2 : TEXCOORD2;
};

struct VS_INPUT_PNT
{
	float3 f3PosOS : POSITION;
	float3 f3VecNormalOS : NORMAL;
	float3 f3Custom : TEXCOORD0;
};

struct VS_OUTPUT
{
	float4 f4PosSS : SV_POSITION;
	float3 f3VecNormalWS : NORMAL;
	float3 f3PosWS : TEXCOORD0;
	float3 f3Custom : TEXCOORD1;
};

struct VS_OUTPUT_TTT
{
	float4 f4PosSS : SV_POSITION;
	//float3 f3VecNormalWS : NORMAL;
	float3 f3PosWS : TEXCOORD0;
	float3 f3Custom0 : TEXCOORD1;
	float3 f3Custom1 : TEXCOORD2;
	float3 f3Custom2 : TEXCOORD3;
};

struct PS_OUTPUT
{
	float4 f4Color : SV_TARGET0; // UNORM
	float fDepthWS : SV_TARGET1;

	float fDepth : SV_Depth;
};

//#define CICERO_TEST

VS_OUTPUT CommonVS_OnlyPoint( float3 f3Pos : POSITION )
{
	VS_OUTPUT vxOut = (VS_OUTPUT) 0;
	vxOut.f4PosSS = mul( float4(f3Pos, 1.f), g_cbPolygonObj.matOS2PS );
	vxOut.f3PosWS = vxTransformPoint( f3Pos, g_cbPolygonObj.matOS2WS );
	vxOut.f3VecNormalWS = (float3)0;
	vxOut.f3Custom = g_cbPolygonObj.f4Color.rgb;

#ifdef VS_MINUSVIEWBOX
	vxOut.f4PosSS.z = (vxOut.f4PosSS.z + 1.f) * 0.5f;
#endif

	return vxOut;
}

VS_OUTPUT CommonVS_PN( VS_INPUT_PN input )
{
	VS_OUTPUT vxOut = (VS_OUTPUT) 0;
	vxOut.f4PosSS = mul( float4(input.f3PosOS, 1.f), g_cbPolygonObj.matOS2PS );
	vxOut.f3PosWS = vxTransformPoint( input.f3PosOS, g_cbPolygonObj.matOS2WS );
	vxOut.f3VecNormalWS = normalize(vxTransformVector( input.f3VecNormalOS, g_cbPolygonObj.matOS2WS ));
#ifdef CICERO_TEST
	vxOut.f3Custom = input.f3PosOS;
#else
	vxOut.f3Custom = g_cbPolygonObj.f4Color.rgb;
#endif

#ifdef VS_MINUSVIEWBOX
	vxOut.f4PosSS.z = (vxOut.f4PosSS.z + 1.f) * 0.5f;
#endif

	return vxOut;
}

VS_OUTPUT CommonVS_PT( VS_INPUT_PT input )
{
	VS_OUTPUT vxOut = (VS_OUTPUT) 0;
	vxOut.f4PosSS = mul( float4(input.f3PosOS, 1.f), g_cbPolygonObj.matOS2PS );
	vxOut.f3PosWS = vxTransformPoint( input.f3PosOS, g_cbPolygonObj.matOS2WS );
	vxOut.f3Custom = input.f3Custom;

#ifdef VS_MINUSVIEWBOX
	vxOut.f4PosSS.z = (vxOut.f4PosSS.z + 1.f) * 0.5f;
#endif

	return vxOut;
}

VS_OUTPUT CommonVS_PNT( VS_INPUT_PNT input )
{
	VS_OUTPUT vxOut = (VS_OUTPUT) 0;
	vxOut.f4PosSS = mul( float4(input.f3PosOS, 1.f), g_cbPolygonObj.matOS2PS );
	vxOut.f3PosWS = vxTransformPoint( input.f3PosOS, g_cbPolygonObj.matOS2WS );
	vxOut.f3VecNormalWS = normalize(vxTransformVector( input.f3VecNormalOS, g_cbPolygonObj.matOS2WS ));

	vxOut.f3Custom = input.f3Custom;
#ifdef VS_POINTDEVIATIRON
	//int iChannelID;	// 0 to 2
	// Discritization //
	float fDevValue = input.f3Custom.x;	//input.f3Custom[g_cbPolygonDeviation.iChannelID];
	float fValueNormalized = saturate((fDevValue - g_cbPolygonDeviation.fMinMappingValue)
		/(g_cbPolygonDeviation.fMaxMappingValue - g_cbPolygonDeviation.fMinMappingValue));
	int iValue = fValueNormalized * g_cbPolygonDeviation.iValueRange;
	float f4RGBA = vxLoadPointOtf1DFromBufferWithoutAC(iValue, g_f4bufOTF, 1.f);
	vxOut.f3Custom = vxLoadPointOtf1DFromBufferWithoutAC(iValue, g_f4bufOTF, 1.f).rgb;
#endif

#ifdef VS_MINUSVIEWBOX
	vxOut.f4PosSS.z = (vxOut.f4PosSS.z + 1.f) * 0.5f;
#endif

	return vxOut;
}

VS_OUTPUT_TTT CommonVS_PTTT(VS_INPUT_PTTT input)
{
	VS_OUTPUT_TTT vxOut = (VS_OUTPUT_TTT)0;
	vxOut.f4PosSS = mul(float4(input.f3PosOS, 1.f), g_cbPolygonObj.matOS2PS);
	vxOut.f3PosWS = vxTransformPoint(input.f3PosOS, g_cbPolygonObj.matOS2WS);

	vxOut.f3Custom0 = input.f3Custom0;

	vxOut.f3Custom1 = vxTransformVector(input.f3Custom1, g_cbPolygonObj.matOS2PS);
	vxOut.f3Custom2 = vxTransformVector(input.f3Custom2, g_cbPolygonObj.matOS2PS);
	vxOut.f3Custom1.y *= -1;
	vxOut.f3Custom2.y *= -1;

	return vxOut;
}

#define DUAL_DEPTH_TEST	\
	float fDepthPrev = g_tex2DDepthDual.Load( int3(input.f4PosSS.xy, 0) ).r;	\
	if(fDepthPrev < input.f4PosSS.z)	\
		vxOut.fDepth = input.f4PosSS.z;	\
	else	\
	{\
		vxOut.fDepth = 1;	\
		vxOut.f4Color = (float4)0;	\
		vxOut.fDepthWS = FLT_MAX;	\
	}\

#define CLIPPING_TEST	\
	if(g_cbPolygonObj.iFlag & 0x1)\
	{\
		if(!vxIsInsideClipBox(input.f3PosWS, g_cbPolygonObj.f3ClipBoxMaxBS, g_cbPolygonObj.matClipBoxWS2BS))\
		{\
			vxOut.fDepth = 1;\
			vxOut.f4Color = (float4)0;\
			vxOut.fDepthWS = FLT_MAX;\
			return vxOut;\
		}\
	}

PS_OUTPUT CommonPS_TEXCOORD1( VS_OUTPUT input )
{
	PS_OUTPUT vxOut = (PS_OUTPUT)0;

	CLIPPING_TEST;

	float3 f3PosIPSampleSS = float3(input.f4PosSS.xy, 0.0f);
	float3 f3PosIPSampleWS = vxTransformPoint(f3PosIPSampleSS, g_cbCamState.matSS2WS);

	float3 f3VecPix2PosWS = input.f3PosWS - f3PosIPSampleWS;
	vxOut.fDepthWS = length(f3VecPix2PosWS);
	if (dot(f3VecPix2PosWS, g_cbCamState.f3VecViewWS) < 0)
		vxOut.fDepthWS *= -1.f;
	vxOut.f4Color.rgb = input.f3Custom;
	vxOut.f4Color.a = g_cbPolygonObj.f4Color.a;

	DUAL_DEPTH_TEST;

	return vxOut;
}

PS_OUTPUT CommonPS_PhongShading(VS_OUTPUT input)
{
	PS_OUTPUT vxOut = (PS_OUTPUT)0;

	CLIPPING_TEST;

	float3 f3PosIPSampleSS = float3(input.f4PosSS.xy, 0.0f);
	float3 f3PosIPSampleWS = vxTransformPoint(f3PosIPSampleSS, g_cbCamState.matSS2WS);


	float3 f3VecPix2PosWS = input.f3PosWS - f3PosIPSampleWS;
	vxOut.fDepthWS = length(f3VecPix2PosWS);
	if (dot(f3VecPix2PosWS, g_cbCamState.f3VecViewWS) < 0)
		vxOut.fDepthWS *= -1.f;

	float4 f4ShadingFactor = g_cbPolygonObj.f4ShadingFactor;

	float3 f3VecLightWS = -g_cbCamState.f3VecLightWS;
	if (g_cbCamState.iFlag & 0x2)
		f3VecLightWS = -normalize(input.f3PosWS - g_cbCamState.f3PosLightWS);

	float fNormalLength = length(input.f3VecNormalWS);
	float fDiff = 0;
	if (fNormalLength > 0)
#ifdef PS_MAX_IILUMINATION
		fDiff = max(dot(-f3VecLightWS, input.f3VecNormalWS / fNormalLength) * g_cbPolygonObj.fShadingMultiplier, 0);
#else
		fDiff = abs(dot(-f3VecLightWS, input.f3VecNormalWS / fNormalLength));
#endif

#ifdef PS_COLOR_GLOBAL

#ifdef CICERO_TEST
	vxOut.f4Color.rgb = float3(input.f3Custom.x / 386.5, input.f3Custom.y / 1708.0, input.f3Custom.z / 744.4);
#else
	vxOut.f4Color.rgb = (f4ShadingFactor.x + f4ShadingFactor.y*fDiff + f4ShadingFactor.z*pow(fDiff, abs(f4ShadingFactor.w)))*g_cbPolygonObj.f4Color.rgb;
#endif
#else
	vxOut.f4Color.rgb = (f4ShadingFactor.x + f4ShadingFactor.y*fDiff + f4ShadingFactor.z*pow(fDiff, abs(f4ShadingFactor.w)))*input.f3Custom;
#endif

#ifdef MARKERS_ON_SURFACE
	if (g_cbInterestingRegion.iNumRegions > 0)
	{
		float3 f3MarkerColor = float3(1.f, 0, 0);

		float fLengths[3];
		fLengths[0] = length(input.f3PosWS - g_cbInterestingRegion.f3PosPtn0WS);
		fLengths[1] = length(input.f3PosWS - g_cbInterestingRegion.f3PosPtn1WS);
		fLengths[2] = length(input.f3PosWS - g_cbInterestingRegion.f3PosPtn2WS);
		float fRadius[3];
		fRadius[0] = g_cbInterestingRegion.fRadius0;
		fRadius[1] = g_cbInterestingRegion.fRadius1;
		fRadius[2] = g_cbInterestingRegion.fRadius2;
		for (int i = 0; i < g_cbInterestingRegion.iNumRegions; i++)
		{
			if (fLengths[i] <= fRadius[i])
			{
				float fRatio = max(fLengths[i] / fRadius[i], 0.2f);

				vxOut.f4Color.x = vxOut.f4Color.x * fRatio + f3MarkerColor.x * (1.f - fRatio);
				vxOut.f4Color.y = vxOut.f4Color.y * fRatio + f3MarkerColor.y * (1.f - fRatio);
				vxOut.f4Color.z = vxOut.f4Color.z * fRatio + f3MarkerColor.z * (1.f - fRatio);
			}
		}
	}
#endif

	vxOut.f4Color.a = g_cbPolygonObj.f4Color.a;

	DUAL_DEPTH_TEST;

	return vxOut;
}

PS_OUTPUT CommonPS_CMM_LINE( VS_OUTPUT input )
{
	PS_OUTPUT vxOut = (PS_OUTPUT)0;
	float3 f3PosIPSampleSS = float3(input.f4PosSS.xy, 0.0f);
	float3 f3PosIPSampleWS = vxTransformPoint(f3PosIPSampleSS, g_cbCamState.matSS2WS);

	vxOut.fDepthWS = length( input.f3PosWS - f3PosIPSampleWS );
	vxOut.f4Color = g_cbPolygonObj.f4Color;
	vxOut.fDepth = input.f4PosSS.z;	// Always single layer //
	
	// Parameter [fDashedDistance] Dashed Distance (WS)
	// Parameter [bIsDashed]
	if(g_cbPolygonObj.iFlag & (0x1 << 19))
	{
		float fDashedDistance = g_cbPolygonObj.fDashDistance;
		if((uint)(input.f3Custom.x / fDashedDistance) % 2 == 1)
		{
			if(g_cbPolygonObj.iFlag & (0x1 << 20))
			{
				vxOut.f4Color.rgb = (float3)1 - vxOut.f4Color.rgb;
			}
			else
			{
				vxOut.f4Color = (float4)0;
				vxOut.fDepthWS = FLT_MAX;
			}
		}
	}

	return vxOut;
}

PS_OUTPUT CommonPS_CMM_TEXT( VS_OUTPUT input )
{
	PS_OUTPUT vxOut = (PS_OUTPUT)0;
	//vxOut.fDepthWS = -vxTransformPoint( input.f3PosWS, g_cbCamState.matWS2CS ).z;
	float3 f3PosIPSampleSS = float3(input.f4PosSS.xy, 0.0f);
	float3 f3PosIPSampleWS = vxTransformPoint(f3PosIPSampleSS, g_cbCamState.matSS2WS);
	vxOut.fDepthWS = length( input.f3PosWS - f3PosIPSampleWS );
	vxOut.fDepth = input.f4PosSS.z;	// Always single layer //

	// g_tex2DTextCMM : TO DO
	// Sample Flip State 0, 1, 2, 3
	float2 f2PosSample = input.f3Custom.xy;
	if (g_cbPolygonObj.iFlag & (0x1 << 9))
		f2PosSample.x = 1.f - input.f3Custom.x;

	if (g_cbPolygonObj.iFlag & (0x1 << 10))
		f2PosSample.y = 1.f - input.f3Custom.y;


	vxOut.f4Color = g_tex2DTextCMM.SampleLevel(g_samplerLinear, f2PosSample, 0);

	if(vxOut.f4Color.a == 0)
	{
		vxOut.fDepthWS = FLT_MAX;
	}
	else 
	{
		//vxOut.f4Color = float4(g_cbPolygonObj.f4Color.rgb, 1);
		//vxOut.f4Color.rgb = float3(1, 0, 0)*vxOut.f4Color.a;
		vxOut.f4Color.rgb = g_cbPolygonObj.f4Color.rgb * vxOut.f4Color.a + (float3(1, 1, 1) - g_cbPolygonObj.f4Color.rgb) * (1.f - vxOut.f4Color.a);
		//vxOut.f4Color.a = 1;
		//if(vxOut.f4Color.a < 0.7f)
		//	vxOut.f4Color.rgb = float3(0, 0, 0);
		//else 
		//	vxOut.f4Color.rgb = float3(1, 1, 1);
		//vxOut.f4Color.a = 1.f;
	}

	return vxOut;
}

PS_OUTPUT CommonPS_CMM_MultiTEXTs(VS_OUTPUT_TTT input)
{
	PS_OUTPUT vxOut = (PS_OUTPUT)0;
	//vxOut.fDepthWS = -vxTransformPoint( input.f3PosWS, g_cbCamState.matWS2CS ).z;
	float3 f3PosIPSampleSS = float3(input.f4PosSS.xy, 0.0f);
	float3 f3PosIPSampleWS = vxTransformPoint(f3PosIPSampleSS, g_cbCamState.matSS2WS);
	vxOut.fDepthWS = length(input.f3PosWS - f3PosIPSampleWS);
	vxOut.fDepth = input.f4PosSS.z;	// Always single layer //

	float2 f2PosSample = input.f3Custom0.xy;
	float3 f3VecWidthPS = input.f3Custom1;
	float3 f3VecHeightPS = input.f3Custom2;

	// Sample Flip State 0, 1, 2, 3

	if (dot(f3VecWidthPS, float3(1, 0, 0)) < 0)
		f2PosSample.x = 1.f - f2PosSample.x;
	
	int iLetterID = (input.f3Custom0.z + 0.5f);
	float fNumLetters = (float)g_cbPolygonObj.iDummy__0;
	float fHeightU = 1.f / fNumLetters;
	float fOffsetU = fHeightU * (float)iLetterID;
	float fLetterU = f2PosSample.y / fNumLetters;


	float3 f3VecNormal = cross(f3VecHeightPS, f3VecWidthPS);
	f3VecHeightPS = cross(f3VecWidthPS, f3VecNormal);

	if (dot(f3VecHeightPS, float3(0, 1, 0)) <= 0)
		f2PosSample.y = fOffsetU + (fHeightU - fLetterU);
	else
		f2PosSample.y = fOffsetU + fLetterU;


	vxOut.f4Color = g_tex2DTextCMM.SampleLevel(g_samplerLinear, f2PosSample, 0);
	//vxOut.f4Color = float4(input.f3PosWS, 1);
	//return vxOut;

	if (vxOut.f4Color.a == 0)
	{
		vxOut.fDepthWS = FLT_MAX;
	}
	else
	{
		//vxOut.f4Color = float4(g_cbPolygonObj.f4Color.rgb, 1);
		//vxOut.f4Color.rgb = float3(1, 0, 0)*vxOut.f4Color.a;
		vxOut.f4Color.rgb = g_cbPolygonObj.f4Color.rgb * vxOut.f4Color.a + (float3(1, 1, 1) - g_cbPolygonObj.f4Color.rgb) * (1.f - vxOut.f4Color.a);
		//vxOut.f4Color.a = 1;
		//if(vxOut.f4Color.a < 0.7f)
		//	vxOut.f4Color.rgb = float3(0, 0, 0);
		//else 
		//	vxOut.f4Color.rgb = float3(1, 1, 1);
		//vxOut.f4Color.a = 1.f;
	}

	return vxOut;
}

PS_OUTPUT CommonPS_Superimpose( VS_OUTPUT input )
{
	PS_OUTPUT vxOut = (PS_OUTPUT)0;

	CLIPPING_TEST;

	float3 f3PosIPSampleSS = float3(input.f4PosSS.xy, 0.0f);
	float3 f3PosIPSampleWS = vxTransformPoint(f3PosIPSampleSS, g_cbCamState.matSS2WS);
	vxOut.fDepthWS = length( input.f3PosWS - f3PosIPSampleWS );

	float3 f3VecLightWS = -g_cbCamState.f3VecLightWS;
	if(g_cbCamState.iFlag & 0x2)
		f3VecLightWS = -normalize(input.f3PosWS - g_cbCamState.f3PosLightWS);

	float fNormalLength = length(input.f3VecNormalWS);
	float fDiff = 0;
	if(fNormalLength > 0)
		fDiff = abs(dot(-f3VecLightWS, input.f3VecNormalWS/fNormalLength));

	float3 f3PosTS = vxTransformPoint( input.f3PosWS, g_cbVolObj.matWS2TS );
	int iSampleValue = (int)(g_tex3DVolume.SampleLevel(g_samplerLinear, f3PosTS, 0).r * g_cbVolObj.fSampleValueRange + 0.5f);
	float4 f4RGBA = vxLoadPointOtf1DFromBuffer(max(iSampleValue, 1), g_f4bufOTF, 1.f);
	
	float4 f4ShadingFactor = g_cbPolygonObj.f4ShadingFactor;

	vxOut.f4Color.rgb = (f4ShadingFactor.x + f4ShadingFactor.y*fDiff + f4ShadingFactor.z*pow(fDiff, abs(f4ShadingFactor.w)))*f4RGBA.rgb;
	vxOut.f4Color.a = g_cbPolygonObj.f4Color.a;
	
	DUAL_DEPTH_TEST;

	return vxOut;
}

PS_OUTPUT CommonPS_Deviation( VS_OUTPUT input )
{
	PS_OUTPUT vxOut = (PS_OUTPUT)0;
	if(g_cbPolygonObj.iFlag & 0x1)
	{
		if(!vxIsInsideClipBox(input.f3PosWS, g_cbPolygonObj.f3ClipBoxMaxBS, g_cbPolygonObj.matClipBoxWS2BS))
		{
			vxOut.fDepth = 1.f;
			vxOut.f4Color = (float4)0;
			vxOut.fDepthWS = FLT_MAX;
			return vxOut;
		}
	}

	float3 f3PosIPSampleSS = float3(input.f4PosSS.xy, 0.0f);
	float3 f3PosIPSampleWS = vxTransformPoint(f3PosIPSampleSS, g_cbCamState.matSS2WS);
	vxOut.fDepthWS = length( input.f3PosWS - f3PosIPSampleWS );

	float3 f3VecSampleDirUnitWSs[2] = { normalize(input.f3VecNormalWS), -normalize(input.f3VecNormalWS) };
	float fDeviationMin = FLT_COMP_MAX;
	float fDistanceMin = FLT_COMP_MAX;
	int iMinSampleSteps = 100000000;

	for(int k = 0; k < 2; k++)
	{
		float3 f3VecSampleDirUnitWS = f3VecSampleDirUnitWSs[k];
		float2 f2PrevNextT = vxVrComputeBoundary(input.f3PosWS, f3VecSampleDirUnitWS, g_cbVolObj);
		if(f2PrevNextT.y <= f2PrevNextT.x)
			continue;
		
		int iNumSamples = min((int)((f2PrevNextT.y - f2PrevNextT.x) / g_cbVolObj.fSampleDistWS + 0.5f), iMinSampleSteps);
		//int iNumSamples = (int)((f2PrevNextT.y - f2PrevNextT.x) / g_cbVolObj.fSampleDistWS + 0.5f);

		float3 f3PosStartWS = input.f3PosWS + f3VecSampleDirUnitWS*f2PrevNextT.x;	// f2PrevNextT.x is almost 0
		float3 f3PosStartTS = vxTransformPoint(f3PosStartWS, g_cbVolObj.matWS2TS);

		float3 f3VecSampleDirWS = f3VecSampleDirUnitWS * g_cbVolObj.fSampleDistWS;
		float3 f3VecSampleDirTS = vxTransformVector( f3VecSampleDirWS, g_cbVolObj.matWS2TS);
		
		float3 f3PosRefTS = vxTransformPoint(input.f3PosWS, g_cbVolObj.matWS2TS);
		int iSampleValueRef = (int)(g_tex3DVolume.SampleLevel(g_samplerLinear, f3PosRefTS, 0).r * g_cbVolObj.fSampleValueRange + 0.5f);
		
		if (iSampleValueRef < g_cbPolygonDeviation.iIsoValueForVolume)
		{	// The polygonal surface is outside the volume surface
			if (iMinSampleSteps == 100000000)
			{
				fDeviationMin = -FLT_COMP_MAX;
				iMinSampleSteps == 99999999;
			}

			[loop]
			for (int i = 1; i < iNumSamples; i++)
			{
				float3 f3PosSampleWS = f3PosStartWS + f3VecSampleDirWS*(float)i;
				float3 f3PosSampleTS = vxTransformPoint(f3PosSampleWS, g_cbVolObj.matWS2TS);

				VxBlockSkip blkSkip = vxComputeBlockRayLengthBoundary(f3PosSampleTS, f3VecSampleDirTS, g_cbBlkObj, g_tex3DBlock);
				blkSkip.iNumStepSkip = min(max(1, blkSkip.iNumStepSkip), iNumSamples - i);

				if (blkSkip.iSampleBlockValue > 0)
				{
					for (int k = 0; k < blkSkip.iNumStepSkip; k++, i++)
					{
						//float3 f3PosSampleInBlockWS = f3PosStartWS + f3VecSampleDirWS*(float)i;
						//float3 f3PosSampleInBlockTS = vxTransformPoint(f3PosSampleInBlockWS, g_cbVolObj.matWS2TS);
						float3 f3PosSampleInBlockTS = f3PosStartTS + f3VecSampleDirTS*(float)i;
						int iSampleValue = (int)(g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleInBlockTS, 0).r * g_cbVolObj.fSampleValueRange + 0.5f);

						if (iSampleValue > g_cbPolygonDeviation.iIsoValueForVolume)
						{
							float fDistance = i * g_cbVolObj.fSampleDistWS + f2PrevNextT.x;// length(f3PosSampleInBlockWS - input.f3PosWS);
							//float fDistance = length(f3PosSampleInBlockWS - input.f3PosWS);
							if (fDistanceMin > fDistance)
							{
								fDistanceMin = fDistance;
								fDeviationMin = -fDistance;
								iMinSampleSteps = i;
							}
							i = iNumSamples;
							break;
						}
					}
				}
				else
				{
					i += blkSkip.iNumStepSkip;
				}
				// this is for outer loop's i++
				i -= 1;
			}
		}
		else
		{
			// The polygonal surface is inside the volume surface
			[loop]
			for (int i = 1; i < iNumSamples; i++)
			{
				float3 f3PosSampleWS = f3PosStartWS + f3VecSampleDirWS*(float)i;
				float3 f3PosSampleTS = vxTransformPoint(f3PosSampleWS, g_cbVolObj.matWS2TS);
				int iSampleValue = (int)(g_tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS, 0).r * g_cbVolObj.fSampleValueRange + 0.5f);
				if (iSampleValue < g_cbPolygonDeviation.iIsoValueForVolume)
				{
					float fDistance = i * g_cbVolObj.fSampleDistWS + f2PrevNextT.x;
					//float fDistance = length(f3PosSampleInBlockWS - input.f3PosWS);
					if (fDistanceMin > fDistance)
					{
						fDistanceMin = fDistance;
						fDeviationMin = fDistance;
						iMinSampleSteps = i;
					}
					i = iNumSamples;
					break;
				}
			}
		}
	}

	float3 f3Color = float3(1, 1, 1);// input.f3Custom;

	float fDeviationScaling = 1.f;
	
	//fDeviationMin = abs(fDeviationMin);
	//if (fDeviationMin > g_cbPolygonDeviation.fMinMappingValue * fDeviationScaling && fDeviationMin < g_cbPolygonDeviation.fMaxMappingValue * fDeviationScaling)
	//if (fDeviationMin > 0 && fDeviationMin < g_cbPolygonDeviation.fMaxMappingValue * fDeviationScaling)
	{
		float fValueNormalized = saturate((fDeviationMin - g_cbPolygonDeviation.fMinMappingValue * fDeviationScaling)
			/ (g_cbPolygonDeviation.fMaxMappingValue * fDeviationScaling - g_cbPolygonDeviation.fMinMappingValue * fDeviationScaling));
		int iValue = fValueNormalized * (g_cbPolygonDeviation.iValueRange - 1);
		f3Color = vxLoadPointOtf1DFromBufferWithoutAC(iValue, g_f4bufOTF, 1.f).rgb;

		//f3Color = float3(abs(fDeviationMin), 0, 0);
	}
	
	float3 f3VecLightWS = -g_cbCamState.f3VecLightWS;
	if(g_cbCamState.iFlag & 0x2)
		f3VecLightWS = -normalize(input.f3PosWS - g_cbCamState.f3PosLightWS);

	float fDiff;
	float fNormalLength = length(input.f3VecNormalWS);
	if(fNormalLength > 0)
		fDiff = abs(dot(-f3VecLightWS, input.f3VecNormalWS/fNormalLength));
	else 
		fDiff = 0;
	
	float4 f4ShadingFactor = g_cbPolygonObj.f4ShadingFactor;

	vxOut.f4Color.rgb = (f4ShadingFactor.x + f4ShadingFactor.y*fDiff + f4ShadingFactor.z*pow(fDiff, abs(f4ShadingFactor.w))) * f3Color;
	vxOut.f4Color.a = g_cbPolygonObj.f4Color.a;
	
	DUAL_DEPTH_TEST;

	return vxOut;
}

PS_OUTPUT CommonPS_ImageStackMapping(VS_OUTPUT input)
{
	PS_OUTPUT vxOut = (PS_OUTPUT)0;

	CLIPPING_TEST;

	float3 f3PosIPSampleSS = float3(input.f4PosSS.xy, 0.0f);
	float3 f3PosIPSampleWS = vxTransformPoint(f3PosIPSampleSS, g_cbCamState.matSS2WS);
	vxOut.fDepthWS = length(input.f3PosWS - f3PosIPSampleWS);

	float3 f3VecLightWS = -g_cbCamState.f3VecLightWS;
	if (g_cbCamState.iFlag & 0x2)
		f3VecLightWS = -normalize(input.f3PosWS - g_cbCamState.f3PosLightWS);

	float fNormalLength = length(input.f3VecNormalWS);
	float fDiff = 0;
	if (fNormalLength > 0)
		fDiff = abs(dot(-f3VecLightWS, input.f3VecNormalWS / fNormalLength));

	//////////// Bi-Linear
	int iSliceIndex = g_cbPolygonObj.iDummy__0;
	float2 f2PosSampleUV = input.f3Custom.xy;
	int2 i2PosSampleUV = int2(f2PosSampleUV);
	float2 f2VecRatio = f2PosSampleUV - i2PosSampleUV;

	float fSample0 = g_tex2DImageStack.Load(int4(i2PosSampleUV.xy, iSliceIndex, 0)).r;
	float fSample1 = g_tex2DImageStack.Load(int4(i2PosSampleUV.xy + int2(1, 0), iSliceIndex, 0)).r;
	float fSample2 = g_tex2DImageStack.Load(int4(i2PosSampleUV.xy + int2(0, 1), iSliceIndex, 0)).r;
	float fSample3 = g_tex2DImageStack.Load(int4(i2PosSampleUV.xy + int2(1, 1), iSliceIndex, 0)).r;

	float fSampleInter01 = fSample0*(1.f - f2VecRatio.x) + fSample1*f2VecRatio.x;
	float fSampleInter23 = fSample2*(1.f - f2VecRatio.x) + fSample3*f2VecRatio.x;
	float fSampleInter0123 = fSampleInter01*(1.f - f2VecRatio.y) + fSampleInter23*f2VecRatio.y;
	
	int iSampleValue = (int)(fSampleInter0123 * g_cbVolObj.fSampleValueRange + 0.5f);
	////////////
	float4 f4RGBA = vxLoadPointOtf1DFromBuffer(max(iSampleValue, 1), g_f4bufOTF, 1.f);	// alpha-multiplied color

	float4 f4ShadingFactor = g_cbPolygonObj.f4ShadingFactor;

	vxOut.f4Color.rgb = (f4ShadingFactor.x + f4ShadingFactor.y*fDiff + f4ShadingFactor.z*pow(fDiff, abs(f4ShadingFactor.w)))*f4RGBA.rgb;
	vxOut.f4Color.a = g_cbPolygonObj.f4Color.a;

	DUAL_DEPTH_TEST;

	return vxOut;
}

PS_OUTPUT CommonPS_AirwayColorMapping(VS_OUTPUT input)
{
	PS_OUTPUT vxOut = (PS_OUTPUT)0;

	CLIPPING_TEST;
	
	float3 f3PosIPSampleSS = float3(input.f4PosSS.xy, 0.0f);
	float3 f3PosIPSampleWS = vxTransformPoint(f3PosIPSampleSS, g_cbCamState.matSS2WS);

	float3 f3VecPix2PosWS = input.f3PosWS - f3PosIPSampleWS;
	vxOut.fDepthWS = length(f3VecPix2PosWS);
	if (dot(f3VecPix2PosWS, g_cbCamState.f3VecViewWS) < 0)
		vxOut.fDepthWS *= -1.f;

	float3 f3VecLightWS = -g_cbCamState.f3VecLightWS;
	if (g_cbCamState.iFlag & 0x2)
		f3VecLightWS = -normalize(input.f3PosWS - g_cbCamState.f3PosLightWS);

	float fNormalLength = length(input.f3VecNormalWS);
	float fDiff = 0;
	if (fNormalLength > 0)
		fDiff = abs(dot(-f3VecLightWS, input.f3VecNormalWS / fNormalLength));

	// 1. Compute the shortest length of the target index ... g_stb_AirwayControls<float4> xyz
	// 2. Compute the color map ... g_stb_AirwayControls<float4> w
	int iNumControlPoints = g_cbPolygonDeviation.iDummy__0;
	float fMinLength = FLT_MAX;
	float fArea = -1.f;
	for (int i = 0; i < iNumControlPoints - 1; i++)
	{
		float4 f4ContolA = g_f4bufPointXYZandAreaW[i];
		float4 f4ContolB = g_f4bufPointXYZandAreaW[i + 1];

		float3 f3PosPointA = f4ContolA.xyz;
		float3 f3PosPointB = f4ContolB.xyz;
		float3 f3VecAB = f3PosPointB - f3PosPointA;
		float3 f3VecCA = f3PosPointA - input.f3PosWS;	// Vector v2VecCA = d2PosLineA - d2PosC;
		float fT = -dot(f3VecCA, f3VecAB) / dot(f3VecAB, f3VecAB);

		if (fT >= 0 && fT <= 1.f)
		{
			float3 f3PosCross = f3PosPointA + f3VecAB * fT;
			float fLength = (input.f3PosWS - f3PosCross).Length;
			if (fLength < fMinLength)
			{
				fMinLength = fLength;
				fArea = f4ContolA.w * (1.f - fT) + f4ContolB.w * fT;
			}
		}
		else if (fT >= -0.5 && fT <= 1.5f)
		{
			float3 f3PosCross = f3PosPointA + f3VecAB * fT;
			float fLength = (input.f3PosWS - f3PosCross).Length;
			if (fLength < fMinLength)
			{
				fMinLength = fLength;
				fT = saturate(fT);
				fArea = f4ContolA.w * (1.f - fT) + f4ContolB.w * fT;
			}
		}
	}

	float fDeviationScaling = 1.f;
	float4 f4RGBA = (float4)1.f;
	if (fArea >= g_cbPolygonDeviation.fMinMappingValue * fDeviationScaling && fArea <= g_cbPolygonDeviation.fMaxMappingValue * fDeviationScaling)
	{
		float fValueNormalized = saturate((fArea - g_cbPolygonDeviation.fMinMappingValue * fDeviationScaling)
			/ (g_cbPolygonDeviation.fMaxMappingValue * fDeviationScaling - g_cbPolygonDeviation.fMinMappingValue * fDeviationScaling));
		int iValue = fValueNormalized * (g_cbPolygonDeviation.iValueRange - 1);
		f4RGBA = vxLoadPointOtf1DFromBufferWithoutAC(iValue, g_f4bufOTF, 1.f);
		f4RGBA.a = 1.f;
	}

	float4 f4ShadingFactor = g_cbPolygonObj.f4ShadingFactor;
	vxOut.f4Color.rgb = (f4ShadingFactor.x + f4ShadingFactor.y*fDiff + f4ShadingFactor.z*pow(fDiff, abs(f4ShadingFactor.w)))*f4RGBA.rgb;
	vxOut.f4Color.a = g_cbPolygonObj.f4Color.a;

	DUAL_DEPTH_TEST;

	return vxOut;
}

PS_OUTPUT PlanePS_Voxelization(VS_OUTPUT input)
{
	PS_OUTPUT vxOut = (PS_OUTPUT)0;

	vxOut.f4Color.rgb = input.f3Custom;
	vxOut.f4Color.a = g_cbPolygonObj.f4Color.a;

	vxOut.fDepth = input.f4PosSS.z;

	return vxOut;
}
// FEATURE LEVEL 9 //
struct PS_OUTPUT_DX9
{
	float4 f4Color : SV_TARGET0; // UNORM
	float4 f4DepthWS : SV_TARGET1;

	float fDepth : SV_Depth;
};

#define DUAL_DEPTH_TEST_DX9	\
	float fDepthPrev = g_tex2DDepthDual.Load( int3(input.f4PosSS.xy, 0) ).r;	\
	if(fDepthPrev < input.f4PosSS.z)	\
		vxOut.fDepth = input.f4PosSS.z;	\
	else	\
	{\
		vxOut.fDepth = 1;	\
		vxOut.f4Color = (float4)0;	\
		vxOut.f4DepthWS.r = FLT_MAX;	\
	}

#define CLIPPING_TEST_DX9 	{\
		if(g_cbPolygonObj.iFlag - (g_cbPolygonObj.iFlag/2)*2 > 0)\
		{\
			if(!vxIsInsideClipBox(input.f3PosWS, g_cbPolygonObj.f3ClipBoxMaxBS, g_cbPolygonObj.matClipBoxWS2BS))\
			{\
				vxOut.fDepth = 1;\
				vxOut.f4Color = (float4)0;\
				vxOut.f4DepthWS.r = FLT_MAX;\
				bIsClipIn = true;\
			}\
		}\
	}

PS_OUTPUT_DX9 CommonPS_TEXCOORD1_DX9( VS_OUTPUT input )
{
	PS_OUTPUT_DX9 vxOut = (PS_OUTPUT_DX9)0;
	
	bool bIsClipIn = false;
	CLIPPING_TEST_DX9;
	if(bIsClipIn)
	{
		float3 f3PosIPSampleSS = float3(input.f4PosSS.xy, 0.0f);
		float3 f3PosIPSampleWS = vxTransformPoint(f3PosIPSampleSS, g_cbCamState.matSS2WS);

		vxOut.f4DepthWS.r = length( input.f3PosWS - f3PosIPSampleWS );
		vxOut.f4Color.rgb = input.f3Custom;
		vxOut.f4Color.a = g_cbPolygonObj.f4Color.a;
	
		//DUAL_DEPTH_TEST_DX9;
	}
	return vxOut;
}

