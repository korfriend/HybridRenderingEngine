#define FLT_MAX 3.402823466e+38F
#define FLT_COMP_MAX 3.402823466e+30F
#define FLT_MIN 0.0000001F
#define M_PI 3.14159265358979323846
#define GRIDSIZE 8
#define ERT_ALPHA 0.99f

#define __CS_MODEL

#define NUM_CPU_TEXRT_LAYERS 2
#define NUM_TEXRT_LAYERS 4 // SAME AS C++'s NUM TEX2D RTs
#define NUM_MERGE_LAYERS NUM_TEXRT_LAYERS
#define SURFACEREFINEMENTNUM 5
//#define HOMOGENEOUS
#define FORCE2POINTSAMPLEOTF

#define FLT_MIN__ 0.000001f			// precision problem for zero-division 
#define FLT_OPACITY_MIN__ 1.f/255.f		// trival storage problem 
#define SAFE_OPAQUEALPHA 	0.999f		// ERT_ALPHA

//== External Definitions ==
// (default : trilinear) SAMPLER_VOLUME_CCF
// (default : full volume) SAMPLE_FROM_BRICKCHUNK

// Sampler //
SamplerState g_samplerLinear : register( s0 );
SamplerState g_samplerPoint : register( s1 );
SamplerState g_samplerLinear_Clamp : register( s2 );
SamplerState g_samplerPoint_Clamp : register( s3 );

Texture2D g_tex2DPrevColorRenderOuts[NUM_TEXRT_LAYERS] : register(t30);	// rgba (unorm)
Texture2D g_tex2DPrevDepthRenderOuts[NUM_TEXRT_LAYERS] : register(t40);	// r (float)

#ifdef __CS_MODEL
struct CS_LayerOut
{
	int iVisibilityLayers[NUM_MERGE_LAYERS]; // ARGB : high to low
	float fDepthLayers[NUM_MERGE_LAYERS];
};

StructuredBuffer<CS_LayerOut> g_stb_PrevLayers: register(t50);	// For Mix //
#else
Texture2D<int4> g_tex2DPrevRSA_RGBA : register(t50);	// rgba (int4)
Texture2D<float4> g_tex2DPrevRSA_DepthCS : register(t51);	// rgba (float4)
#endif

// Helpers //
float4 vxConvertColorToFloat4(int iColor)
{
	// RGBA
	return float4(((iColor>>16) & 0xFF)/255.f, ((iColor>>8) & 0xFF)/255.f, (iColor & 0xFF)/255.f, ((iColor>>24) & 0xFF)/255.f);
}

int vxConvertColorToInt(float4 f4Color)
{
	// RGBA
	int iR = (int)min(f4Color.x * 255.f, 255.f);
	int iG = (int)min(f4Color.y * 255.f, 255.f);
	int iB = (int)min(f4Color.z * 255.f, 255.f);
	int iA = (int)min(f4Color.w * 255.f, 255.f);
	return (iA<<24) | (iR<<16) | (iG<<8) | iB;
}

float3 vxTransformPoint(float3 f3PosSrc, float4x4 matConvert)
{
	float4 f4PosSrc = mul( float4(f3PosSrc, 1.f), matConvert );
	return f4PosSrc.xyz/f4PosSrc.w;
}

float3 vxTransformVector(float3 f3VecSrc, float4x4 matConvert)
{						
	float4 f4PosOrigin = mul( float4(0, 0, 0, 1), matConvert);
	float3 f3PosOrigin = f4PosOrigin.xyz/f4PosOrigin.w;
	float4 f4PosDistance = mul( float4(f3VecSrc, 1.f), matConvert);
	float3 f3PosDistance = f4PosDistance.xyz/f4PosDistance.w;
	return f3PosDistance - f3PosOrigin;
}

float vxRandom(float2 f2Seed)
{
	float2 f2Temp = float2(23.1406926327792690, 2.6651441426902251);
	return frac(cos(123456789.0%1e-7 + 256.0 * dot(f2Seed, f2Temp)));
}

bool vxIsInsideOrthoBox(float3 f3PosTarget, float3 f3PosOrthoBoxMin, float3 f3PosOrthoBoxMax, float fErrorBound)
{
	float3 f3VecMax2TargetBS = f3PosTarget - f3PosOrthoBoxMax;
	float3 f3VecMin2TargetBS = f3PosTarget - f3PosOrthoBoxMin;
	float3 f3CompareElement = f3VecMin2TargetBS * f3VecMax2TargetBS;
	if(f3CompareElement.x > 0 || f3CompareElement.y > 0 || f3CompareElement.z > 0)
		return false;
	return true;
}

bool vxIsInsideClipBox(float3 f3PosTargetWS, float3 f3PosBoxMaxBS, float4x4 matWS2BS)
{
	float3 f3PosTargetBS = vxTransformPoint( f3PosTargetWS, matWS2BS );
	float3 f3VecMax2TargetBS = f3PosTargetBS - f3PosBoxMaxBS;
	float3 f3CompareElement = f3PosTargetBS * f3VecMax2TargetBS;
	if(f3CompareElement.x > 0 || f3CompareElement.y > 0 || f3CompareElement.z > 0)
		return false;
	return true;
}

float2 vxComputePrevNextAlignedBoxBoundary(float3 f3PosStart, float3 f3PosBoxMin, float3 f3PosBoxMax, float3 f3VecDirection)
{
	// intersect ray with a box
	// http://www.siggraph.org/education/materials/HyperGraph/raytrace/rtinter3.htm
	float3 invR = float3(1.0f, 1.0f, 1.0f) / f3VecDirection;
    float3 tbot = invR * (f3PosBoxMin - f3PosStart);
    float3 ttop = invR * (f3PosBoxMax - f3PosStart);

    // re-order intersections to find smallest and largest on each axis
    float3 tmin = min(ttop, tbot);
    float3 tmax = max(ttop, tbot);

    // find the largest tmin and the smallest tmax
    float largest_tmin = max(max(tmin.x, tmin.y), max(tmin.x, tmin.z));
    float smallest_tmax = min(min(tmax.x, tmax.y), min(tmax.x, tmax.z));

	float tnear = max(largest_tmin, 0.f);
	float tfar = smallest_tmax;
	return float2(tnear, tfar);
}

float2 vxComputePrevNextBoxTransformBoundary(float3 f3PosStart, float3 f3PosBoxMax, float3 f3VecDirection, float4x4 matTrWS)
{
	//float3 f3PosStartBS = mul( float4(f3PosStart, 1.f), matTrWS ).xyz;
	//float3 f3PosBoxMaxBS = mul( float4(f3PosBoxMax, 1.f), matTrWS ).xyz;
	float3 f3PosStartBS = vxTransformPoint(f3PosStart, matTrWS );
	float3 f3PosBoxMaxBS = vxTransformPoint(f3PosBoxMax, matTrWS );
	float3 f3VecDirectionBS = normalize(vxTransformVector(f3VecDirection, matTrWS));
	return vxComputePrevNextAlignedBoxBoundary(f3PosStartBS, float3(0, 0, 0), f3PosBoxMaxBS, f3VecDirectionBS);
}

float2 vxComputePrevNextOnClipPlane(float fPrevT, float fNextT, float3 f3PosOnPlane, float3 f3VecPlane, float3 f3PosStart, float3 f3VecDirection)
{
	float2 f2PrevNextT = float2(fPrevT, fNextT);

	// H : f3VecPlaneSVS, V : f3VecSampleSVS, A : f3PosIPSampleSVS, Q : f3PosPlaneSVS //
	// 0. Is ray direction parallel with plane's vector?
	float fDotHV = dot(f3VecPlane, f3VecDirection);
	if(fDotHV != 0)
	{
		// 1. Compute T for Position on Plane
		float fT = dot(f3VecPlane, f3PosOnPlane - f3PosStart)/fDotHV;
		// 2. Check if on Cube
		if(fT > fPrevT && fT < fNextT)
		{
			// 3. Check if front or next position
			if(fDotHV < 0)
				f2PrevNextT.x = fT;
			else
				f2PrevNextT.y = fT;
		}
		else if(fT > fPrevT && fT > fNextT)
		{
			if(fDotHV < 0)
				f2PrevNextT.y = -FLT_MAX; // return;
			else
				; // conserve fPrevT and fNextT
		}
		else if(fT < fPrevT && fT < fNextT)
		{
			if(fDotHV < 0)
				;
			else
				f2PrevNextT.y = -FLT_MAX; // return;
		}
	}
	else
	{
		// Check Upperside of plane
		if(dot(f3VecPlane, f3PosOnPlane - f3PosStart) <= 0)
			f2PrevNextT.y = -FLT_MAX; // return;
	}
	
	return f2PrevNextT;
}

#define OTFROWPITCH 65537

float4 vxLoadPointOtf1DFromBuffer(int iSampleValue, Buffer<float4> f4bufOTF, float fOpaqueCorrection)
{
	float4 f4OtfColor = f4bufOTF[iSampleValue];
	f4OtfColor.a *= fOpaqueCorrection;
	f4OtfColor.rgb *= f4OtfColor.a; // associate color
	return f4OtfColor;
}

float4 vxLoadPointOtf1DSeriesFromBuffer(int iSampleValue, int iIdOTF, Buffer<float4> f4bufOTF, float fOpaqueCorrection)
{
	float4 f4OtfColor = f4bufOTF[iSampleValue + iIdOTF * OTFROWPITCH];
	f4OtfColor.a *= fOpaqueCorrection;
	f4OtfColor.rgb *= f4OtfColor.a; // associate color
	return f4OtfColor;
}

float4 vxLoadPointOtf1DFromBufferWithoutAC(int iSampleValue, Buffer<float4> f4bufOTF, float fOpaqueCorrection)
{
	float4 f4OtfColor = f4bufOTF[iSampleValue];
	f4OtfColor.a *= fOpaqueCorrection;
	return f4OtfColor;
}

float4 vxLoadPointOtf1DSeriesFromBufferWithoutAC(int iSampleValue, int iIdOTF, Buffer<float4> f4bufOTF, float fOpaqueCorrection)
{
	float4 f4OtfColor = f4bufOTF[iSampleValue + iIdOTF * OTFROWPITCH];
	f4OtfColor.a *= fOpaqueCorrection;
	return f4OtfColor;
}

float4 vxLoadSlabOtf1DFromBuffer(int iSampleValuePrev, int iSampleValueNext, Buffer<float4> f4bufOTF, int iOtfDimSize, float fOpaqueCorrection, float4 f4OtfEndOpacity)
{
	float4 f4ColorOTF = (float4)0;

	if(iSampleValueNext == iSampleValuePrev)
		iSampleValueNext++;

	int iDiffSampleValue = iSampleValueNext - iSampleValuePrev;
	float fDiffSampleValueInverse = 1.f/(float)iDiffSampleValue;
	
	//float4 f4OtfColorPrev = f4OtfEndOpacity;
	//float4 f4OtfColorNext = f4OtfEndOpacity;
	//if(iSampleValuePrev < iOtfDimSize)
	//{
	//	uint uiTFAddrPrev = (uint)iSampleValuePrev;
	//	uint2 ui2TFSampleAddrPrev;
	//	ui2TFSampleAddrPrev.x = uiTFAddrPrev%TEX2DOTFROWPITCH;
	//	ui2TFSampleAddrPrev.y = uiTFAddrPrev/TEX2DOTFROWPITCH;
	//	f4OtfColorPrev = tex2DOTF.Load(int3(ui2TFSampleAddrPrev, 0));
	//}
	//if(iSampleValueNext < iOtfDimSize)
	//{
	//	uint uiTFAddrNext = (uint)iSampleValueNext;
	//	uint2 ui2TFSampleAddrNext;
	//	ui2TFSampleAddrNext.x = uiTFAddrNext%TEX2DOTFROWPITCH;
	//	ui2TFSampleAddrNext.y = uiTFAddrNext/TEX2DOTFROWPITCH;
	//	f4OtfColorNext = tex2DOTF.Load(int3(ui2TFSampleAddrNext, 0));
	//}
	float4 f4OtfColorPrev = f4bufOTF[iSampleValuePrev];
	float4 f4OtfColorNext = f4bufOTF[iSampleValueNext];
	
	f4ColorOTF.rgb = (f4OtfColorNext.rgb - f4OtfColorPrev.rgb)*fDiffSampleValueInverse;
	f4ColorOTF.a = 1.f - exp(-(f4OtfColorNext.a - f4OtfColorPrev.a)*fDiffSampleValueInverse*fOpaqueCorrection);
	f4ColorOTF.rgb *= f4ColorOTF.a; // associate color
	return f4ColorOTF;
}

float3 vxSampleOtfGradientSamplerTex3D(float3 f3PosSample, float3 f3VecGradientSampleX, float3 f3VecGradientSampleY, float3 f3VecGradientSampleZ, 
	Buffer<float4> f4bufOTF, float fSampleValueRange, float fOpacityCorrection, Texture3D tex3DVolume)
{
	int iSampleGxR = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSample + f3VecGradientSampleX, 0).r * fSampleValueRange);
	int iSampleGxL = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSample - f3VecGradientSampleX, 0).r * fSampleValueRange);
	int iSampleGyR = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSample + f3VecGradientSampleY, 0).r * fSampleValueRange);
	int iSampleGyL = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSample - f3VecGradientSampleY, 0).r * fSampleValueRange);
	int iSampleGzR = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSample + f3VecGradientSampleZ, 0).r * fSampleValueRange);
	int iSampleGzL = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSample - f3VecGradientSampleZ, 0).r * fSampleValueRange);
	float fAlphaGxR = vxLoadPointOtf1DFromBufferWithoutAC(iSampleGxR, f4bufOTF, fOpacityCorrection).a;
	float fAlphaGxL = vxLoadPointOtf1DFromBufferWithoutAC(iSampleGxL, f4bufOTF, fOpacityCorrection).a;
	float fAlphaGyR = vxLoadPointOtf1DFromBufferWithoutAC(iSampleGyR, f4bufOTF, fOpacityCorrection).a;
	float fAlphaGyL = vxLoadPointOtf1DFromBufferWithoutAC(iSampleGyL, f4bufOTF, fOpacityCorrection).a;
	float fAlphaGzR = vxLoadPointOtf1DFromBufferWithoutAC(iSampleGzR, f4bufOTF, fOpacityCorrection).a;
	float fAlphaGzL = vxLoadPointOtf1DFromBufferWithoutAC(iSampleGzL, f4bufOTF, fOpacityCorrection).a;
	float fGx = fAlphaGxR - fAlphaGxL;
	float fGy = fAlphaGyR - fAlphaGyL;
	float fGz = fAlphaGzR - fAlphaGzL;
	return float3(fGx, fGy, fGz);
}

float vxTrilinearInterpolation(
	float fDensity0, float fDensity1, float fDensity2, float fDensity3,
	float fDensity4, float fDensity5, float fDensity6, float fDensity7,
	float3 f3VecRatio)
{
	float fDenInter01 = fDensity0*(1.f - f3VecRatio.x) + fDensity1*f3VecRatio.x;
	float fDenInter23 = fDensity2*(1.f - f3VecRatio.x) + fDensity3*f3VecRatio.x;
	float fDenInter0123 = fDenInter01*(1.f - f3VecRatio.y) + fDenInter23*f3VecRatio.y;
	float fDenInter45 = fDensity4*(1.f - f3VecRatio.x) + fDensity5*f3VecRatio.x;
	float fDenInter67 = fDensity6*(1.f - f3VecRatio.x) + fDensity7*f3VecRatio.x;
	float fDenInter4567 = fDenInter45*(1.f - f3VecRatio.y) + fDenInter67*f3VecRatio.y;
	float fDenTrilinear = fDenInter0123*(1.f - f3VecRatio.z) + fDenInter4567*f3VecRatio.z;
	return fDenTrilinear;
}

float2 vxSampleByCCF(float3 f3PosSampleTS, float3 f3SizeVolumeCVS, Texture3D tex3DVolume)
{
	//float fDensityLinear = tex3DVolume.SampleLevel( g_samplerLinear_Clamp, f3PosSampleTS, 0).r;
	//if(fDensityLinear == 0)
	//	return 0;
	float3 f3PosSampleCVS = float3(f3PosSampleTS.x * f3SizeVolumeCVS.x - 0.5f, f3PosSampleTS.y * f3SizeVolumeCVS.y - 0.5f, f3PosSampleTS.z * f3SizeVolumeCVS.z - 0.5f);
	int3 i3PosSampleCVS = int3(f3PosSampleCVS);

	float fSamples[8];
	fSamples[0] = tex3DVolume.Load( int4(i3PosSampleCVS, 0) ).r;
	fSamples[1] = tex3DVolume.Load( int4(i3PosSampleCVS + int3(1, 0, 0), 0) ).r;
	fSamples[2] = tex3DVolume.Load( int4(i3PosSampleCVS + int3(0, 1, 0), 0) ).r;
	fSamples[3] = tex3DVolume.Load( int4(i3PosSampleCVS + int3(1, 1, 0), 0) ).r;
	fSamples[4] = tex3DVolume.Load( int4(i3PosSampleCVS + int3(0, 0, 1), 0) ).r;
	fSamples[5] = tex3DVolume.Load( int4(i3PosSampleCVS + int3(1, 0, 1), 0) ).r;
	fSamples[6] = tex3DVolume.Load( int4(i3PosSampleCVS + int3(0, 1, 1), 0) ).r;
	fSamples[7] = tex3DVolume.Load( int4(i3PosSampleCVS + int3(1, 1, 1), 0) ).r;
	
	float fDensityPoint = 0;
	for(int i = 0; i < 8; i++)
	{
		float fSampleValue = fSamples[i];
		if (fSampleValue > 0)
		{
			fDensityPoint = fSampleValue;//max(fSampleValue, fDensityPoint);
			fSamples[i] = 1.f;
		}
	}
	
	float2 f2OutValues = float2(fDensityPoint, 0);
	float3 f3VecRatio = f3PosSampleCVS - i3PosSampleCVS;
	f2OutValues.y = vxTrilinearInterpolation(fSamples[0], fSamples[1], fSamples[2], fSamples[3], fSamples[4], fSamples[5], fSamples[6], fSamples[7], f3VecRatio);
	return f2OutValues;
}

float vxSampleSculptMaskWeightByCCF(float3 f3PosSampleTS, float3 f3SizeVolumeCVS, Texture3D tex3DVolume, int iSculptIndex)
{
	float3 f3PosSampleCVS = float3(f3PosSampleTS.x * f3SizeVolumeCVS.x - 0.5f, f3PosSampleTS.y * f3SizeVolumeCVS.y - 0.5f, f3PosSampleTS.z * f3SizeVolumeCVS.z - 0.5f);
	int3 i3PosSampleCVS = int3(f3PosSampleCVS);

	float fSamples[8];

	f3SizeVolumeCVS -= float3(1, 1, 1);
	
	float3 f3PosSampleTS0 = float3(i3PosSampleCVS.x / f3SizeVolumeCVS.x, i3PosSampleCVS.y / f3SizeVolumeCVS.y, i3PosSampleCVS.z / f3SizeVolumeCVS.z);
	float3 f3PosSampleTS1 = float3((float)(i3PosSampleCVS.x + int3(1, 0, 0)) / f3SizeVolumeCVS.x, (float)(i3PosSampleCVS.y + int3(1, 0, 0)) / f3SizeVolumeCVS.y, (float)(i3PosSampleCVS.z + int3(1, 0, 0)) / f3SizeVolumeCVS.z);
	float3 f3PosSampleTS2 = float3((float)(i3PosSampleCVS.x + int3(0, 1, 0)) / f3SizeVolumeCVS.x, (float)(i3PosSampleCVS.y + int3(0, 1, 0)) / f3SizeVolumeCVS.y, (float)(i3PosSampleCVS.z + int3(0, 1, 0)) / f3SizeVolumeCVS.z);
	float3 f3PosSampleTS3 = float3((float)(i3PosSampleCVS.x + int3(1, 1, 0)) / f3SizeVolumeCVS.x, (float)(i3PosSampleCVS.y + int3(1, 1, 0)) / f3SizeVolumeCVS.y, (float)(i3PosSampleCVS.z + int3(1, 1, 0)) / f3SizeVolumeCVS.z);
	float3 f3PosSampleTS4 = float3((float)(i3PosSampleCVS.x + int3(0, 0, 1)) / f3SizeVolumeCVS.x, (float)(i3PosSampleCVS.y + int3(0, 0, 1)) / f3SizeVolumeCVS.y, (float)(i3PosSampleCVS.z + int3(0, 0, 1)) / f3SizeVolumeCVS.z);
	float3 f3PosSampleTS5 = float3((float)(i3PosSampleCVS.x + int3(1, 0, 1)) / f3SizeVolumeCVS.x, (float)(i3PosSampleCVS.y + int3(1, 0, 1)) / f3SizeVolumeCVS.y, (float)(i3PosSampleCVS.z + int3(1, 0, 1)) / f3SizeVolumeCVS.z);
	float3 f3PosSampleTS6 = float3((float)(i3PosSampleCVS.x + int3(0, 1, 1)) / f3SizeVolumeCVS.x, (float)(i3PosSampleCVS.y + int3(0, 1, 1)) / f3SizeVolumeCVS.y, (float)(i3PosSampleCVS.z + int3(0, 1, 1)) / f3SizeVolumeCVS.z);
	float3 f3PosSampleTS7 = float3((float)(i3PosSampleCVS.x + int3(1, 1, 1)) / f3SizeVolumeCVS.x, (float)(i3PosSampleCVS.y + int3(1, 1, 1)) / f3SizeVolumeCVS.y, (float)(i3PosSampleCVS.z + int3(1, 1, 1)) / f3SizeVolumeCVS.z);
	// Clamp //
	fSamples[0] = tex3DVolume.SampleLevel(g_samplerPoint_Clamp, f3PosSampleTS0, 0).r;
	fSamples[1] = tex3DVolume.SampleLevel(g_samplerPoint_Clamp, f3PosSampleTS1, 0).r;
	fSamples[2] = tex3DVolume.SampleLevel(g_samplerPoint_Clamp, f3PosSampleTS2, 0).r;
	fSamples[3] = tex3DVolume.SampleLevel(g_samplerPoint_Clamp, f3PosSampleTS3, 0).r;
	fSamples[4] = tex3DVolume.SampleLevel(g_samplerPoint_Clamp, f3PosSampleTS4, 0).r;
	fSamples[5] = tex3DVolume.SampleLevel(g_samplerPoint_Clamp, f3PosSampleTS5, 0).r;
	fSamples[6] = tex3DVolume.SampleLevel(g_samplerPoint_Clamp, f3PosSampleTS6, 0).r;
	fSamples[7] = tex3DVolume.SampleLevel(g_samplerPoint_Clamp, f3PosSampleTS7, 0).r;

//	int3 i3PosMaxVolumeCVS = (int3)f3SizeVolumeCVS - int3(1, 1, 1);
//	int3 i3PosSample, i3VecMax2Sample, i3MultDot;
//
//	i3PosSample = i3PosSampleCVS;
//	i3VecMax2Sample = i3PosSample - i3PosMaxVolumeCVS;
//	i3MultDot = i3VecMax2Sample * i3PosSample;
//	if (i3MultDot.x > 0 || i3MultDot.y > 0 || i3MultDot.z > 0)
//		fSamples[0] = 1.f/255.f;
//	else
//		fSamples[0] = tex3DVolume.Load(int4(i3PosSample, 0)).r;
//	i3PosSample = i3PosSampleCVS + int3(1, 0, 0);
//	i3VecMax2Sample = i3PosSample - i3PosMaxVolumeCVS;
//	i3MultDot = i3VecMax2Sample * i3PosSample;
//	if (i3MultDot.x > 0 || i3MultDot.y > 0 || i3MultDot.z > 0)
//		fSamples[1] = 1.f/255.f;
//	else
//		fSamples[1] = tex3DVolume.Load(int4(i3PosSample, 0)).r;
//	i3PosSample = i3PosSampleCVS + int3(0, 1, 0);
//	i3VecMax2Sample = i3PosSample - i3PosMaxVolumeCVS;
//	i3MultDot = i3VecMax2Sample * i3PosSample;
//	if (i3MultDot.x > 0 || i3MultDot.y > 0 || i3MultDot.z > 0)
//		fSamples[2] = 1.f / 255.f;
//	else
//		fSamples[2] = tex3DVolume.Load(int4(i3PosSample, 0)).r;
//	i3PosSample = i3PosSampleCVS + int3(1, 1, 0);
//	i3VecMax2Sample = i3PosSample - i3PosMaxVolumeCVS;
//	i3MultDot = i3VecMax2Sample * i3PosSample;
//	if (i3MultDot.x > 0 || i3MultDot.y > 0 || i3MultDot.z > 0)
//		fSamples[3] = 1.f / 255.f;
//	else
//		fSamples[3] = tex3DVolume.Load(int4(i3PosSample, 0)).r;
//	i3PosSample = i3PosSampleCVS + int3(0, 0, 1);
//	i3VecMax2Sample = i3PosSample - i3PosMaxVolumeCVS;
//	i3MultDot = i3VecMax2Sample * i3PosSample;
//	if (i3MultDot.x > 0 || i3MultDot.y > 0 || i3MultDot.z > 0)
//		fSamples[4] = 1.f / 255.f;
//	else
//		fSamples[4] = tex3DVolume.Load(int4(i3PosSample, 0)).r;
//	i3PosSample = i3PosSampleCVS + int3(1, 0, 1);
//	i3VecMax2Sample = i3PosSample - i3PosMaxVolumeCVS;
//	i3MultDot = i3VecMax2Sample * i3PosSample;
//	if (i3MultDot.x > 0 || i3MultDot.y > 0 || i3MultDot.z > 0)
//		fSamples[5] = 1.f / 255.f;
//	else
//		fSamples[5] = tex3DVolume.Load(int4(i3PosSample, 0)).r;
//	i3PosSample = i3PosSampleCVS + int3(0, 1, 1);
//	i3VecMax2Sample = i3PosSample - i3PosMaxVolumeCVS;
//	i3MultDot = i3VecMax2Sample * i3PosSample;
//	if (i3MultDot.x > 0 || i3MultDot.y > 0 || i3MultDot.z > 0)
//		fSamples[6] = 1.f / 255.f;
//	else
//		fSamples[6] = tex3DVolume.Load(int4(i3PosSample, 0)).r;
//	i3PosSample = i3PosSampleCVS + int3(1, 1, 1);
//	i3VecMax2Sample = i3PosSample - i3PosMaxVolumeCVS;
//	i3MultDot = i3VecMax2Sample * i3PosSample;
//	if (i3MultDot.x > 0 || i3MultDot.y > 0 || i3MultDot.z > 0)
//		fSamples[7] = 1.f / 255.f;
//	else
//		fSamples[7] = tex3DVolume.Load(int4(i3PosSample, 0)).r;

	//fSamples[0] = tex3DVolume.Load(int4(i3PosSampleCVS, 0)).r;
	//fSamples[1] = tex3DVolume.Load(int4(i3PosSampleCVS + int3(1, 0, 0), 0)).r;
	//fSamples[2] = tex3DVolume.Load(int4(i3PosSampleCVS + int3(0, 1, 0), 0)).r;
	//fSamples[3] = tex3DVolume.Load(int4(i3PosSampleCVS + int3(1, 1, 0), 0)).r;
	//fSamples[4] = tex3DVolume.Load(int4(i3PosSampleCVS + int3(0, 0, 1), 0)).r;
	//fSamples[5] = tex3DVolume.Load(int4(i3PosSampleCVS + int3(1, 0, 1), 0)).r;
	//fSamples[6] = tex3DVolume.Load(int4(i3PosSampleCVS + int3(0, 1, 1), 0)).r;
	//fSamples[7] = tex3DVolume.Load(int4(i3PosSampleCVS + int3(1, 1, 1), 0)).r;

	for (int i = 0; i < 8; i++)
	{
		//if ((int)(fSamples[i]) == 0 || (int)(fSamples[i]*255) > iSculptIndex)	// <== BUG 임 -_-;
		int iSculptValue = (int)(fSamples[i] * 255 + 0.5f);
		if (iSculptValue == 0 || iSculptValue > iSculptIndex)
			fSamples[i] = 1.f;
		else
			fSamples[i] = 0.f;
	}

	float3 f3VecRatio = f3PosSampleCVS - i3PosSampleCVS;
	return vxTrilinearInterpolation(fSamples[0], fSamples[1], fSamples[2], fSamples[3], fSamples[4], fSamples[5], fSamples[6], fSamples[7], f3VecRatio);
}

float3 vxSampleVolumeGradientSamplerTex3D(float3 f3PosSampleTS, float3 f3VecGradientSampleX, float3 f3VecGradientSampleY, float3 f3VecGradientSampleZ, Texture3D tex3DVolume)
{
	float fGx = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + f3VecGradientSampleX, 0).r
		- tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - f3VecGradientSampleX, 0).r;
	float fGy = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + f3VecGradientSampleY, 0).r
		- tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - f3VecGradientSampleY, 0).r;
	float fGz = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + f3VecGradientSampleZ, 0).r
		- tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - f3VecGradientSampleZ, 0).r;
	return float3(fGx, fGy, fGz);
}

float3 vxSampleVolumeGradientSamplerTex3DByCCF(float3 f3PosSampleTS, float3 f3SizeVolumeCVS, float3 f3VecGradientSampleX, float3 f3VecGradientSampleY, float3 f3VecGradientSampleZ, Texture3D tex3DVolume)
{
	float fGX0 = vxSampleByCCF(f3PosSampleTS + f3VecGradientSampleX, f3SizeVolumeCVS, tex3DVolume).y;
	float fGX1 = vxSampleByCCF(f3PosSampleTS - f3VecGradientSampleX, f3SizeVolumeCVS, tex3DVolume).y;
	
	float fGY0 = vxSampleByCCF(f3PosSampleTS + f3VecGradientSampleY, f3SizeVolumeCVS, tex3DVolume).y;
	float fGY1 = vxSampleByCCF(f3PosSampleTS - f3VecGradientSampleY, f3SizeVolumeCVS, tex3DVolume).y;
	
	float fGZ0 = vxSampleByCCF(f3PosSampleTS + f3VecGradientSampleZ, f3SizeVolumeCVS, tex3DVolume).y;
	float fGZ1 = vxSampleByCCF(f3PosSampleTS - f3VecGradientSampleZ, f3SizeVolumeCVS, tex3DVolume).y;

	float fGx = fGX0 - fGX1;
	float fGy = fGY0 - fGY1;
	float fGz = fGZ0 - fGZ1;

	return float3(fGx, fGy, fGz);
	//return float3(1, 0, 0);
}

float3 vxSampleVolumeGradientSamplerTex3DFromBrickChunks(float3 f3PosSampleCTS
	, float3 f3VecGradientSampleX, float3 f3VecGradientSampleY, float3 f3VecGradientSampleZ
	, int iSampleBlockValue, int iNumBricksInChunkXY, Texture3D tex3DChunks[3])
{
#ifdef ONLYSINGLECHUNK
	float3 f3VecGrad = vxSampleVolumeGradientSamplerTex3D(f3PosSampleCTS, 
		f3VecGradientSampleX, f3VecGradientSampleY, f3VecGradientSampleZ, tex3DChunks[0]);
#else
	float3 f3VecGrad = (float3)0;
	switch((uint)(iSampleBlockValue) / (uint)iNumBricksInChunkXY)
	{
	case 0 : f3VecGrad = vxSampleVolumeGradientSamplerTex3D(f3PosSampleCTS, f3VecGradientSampleX, f3VecGradientSampleY, f3VecGradientSampleZ, tex3DChunks[0]); break;
	case 1 : f3VecGrad = vxSampleVolumeGradientSamplerTex3D(f3PosSampleCTS, f3VecGradientSampleX, f3VecGradientSampleY, f3VecGradientSampleZ, tex3DChunks[1]); break;
	case 2 : f3VecGrad = vxSampleVolumeGradientSamplerTex3D(f3PosSampleCTS, f3VecGradientSampleX, f3VecGradientSampleY, f3VecGradientSampleZ, tex3DChunks[2]); break;
	//case 3 : f3VecGrad = vxSampleVolumeGradientSamplerTex3D(f3PosSampleCTS, f3VecGradientSampleX, f3VecGradientSampleY, f3VecGradientSampleZ, tex3DChunks[3]); break;
	//case 4 : f3VecGrad = vxSampleVolumeGradientSamplerTex3D(f3PosSampleCTS, f3VecGradientSampleX, f3VecGradientSampleY, f3VecGradientSampleZ, tex3DChunks[4]); break;
	//case 5 : f3VecGrad = vxSampleVolumeGradientSamplerTex3D(f3PosSampleCTS, f3VecGradientSampleX, f3VecGradientSampleY, f3VecGradientSampleZ, tex3DChunks[3]); break;
	//case 6 : f3VecGrad = vxSampleVolumeGradientSamplerTex3D(f3PosSampleCTS, f3VecGradientSampleX, f3VecGradientSampleY, f3VecGradientSampleZ, tex3DChunks[6]); break;
	//case 7 : f3VecGrad = vxSampleVolumeGradientSamplerTex3D(f3PosSampleCTS, f3VecGradientSampleX, f3VecGradientSampleY, f3VecGradientSampleZ, tex3DChunks[7]); break;
	//case 8 : f3VecGrad = vxSampleVolumeGradientSamplerTex3D(f3PosSampleCTS, f3VecGradientSampleX, f3VecGradientSampleY, f3VecGradientSampleZ, tex3DChunks[8]); break;
	//case 9 : f3VecGrad = vxSampleVolumeGradientSamplerTex3D(f3PosSampleCTS, f3VecGradientSampleX, f3VecGradientSampleY, f3VecGradientSampleZ, tex3DChunks[9]); break;
	}
#endif
	return f3VecGrad;
}

float3 vxSampleVolumeGradientSamplerTex3DFromBrickChunksByCCF(float3 f3PosSampleCTS, float3 f3SizeVolumeCVS
	, float3 f3VecGradientSampleX, float3 f3VecGradientSampleY, float3 f3VecGradientSampleZ
	, int iSampleBlockValue, int iNumBricksInChunkXY, Texture3D tex3DChunks[3])
{
#ifdef ONLYSINGLECHUNK
	float3 f3VecGrad = vxSampleVolumeGradientSamplerTex3DByCCF(f3PosSampleCTS, f3SizeVolumeCVS, 
		f3VecGradientSampleX, f3VecGradientSampleY, f3VecGradientSampleZ, tex3DChunks[0]);
#else
	float3 f3VecGrad = (float3)0;
	switch((uint)(iSampleBlockValue) / (uint)iNumBricksInChunkXY)
	{
	case 0 : f3VecGrad = vxSampleVolumeGradientSamplerTex3DByCCF(f3PosSampleCTS, f3SizeVolumeCVS, f3VecGradientSampleX, f3VecGradientSampleY, f3VecGradientSampleZ, tex3DChunks[0]); break;
	case 1 : f3VecGrad = vxSampleVolumeGradientSamplerTex3DByCCF(f3PosSampleCTS, f3SizeVolumeCVS, f3VecGradientSampleX, f3VecGradientSampleY, f3VecGradientSampleZ, tex3DChunks[1]); break;
	case 2 : f3VecGrad = vxSampleVolumeGradientSamplerTex3DByCCF(f3PosSampleCTS, f3SizeVolumeCVS, f3VecGradientSampleX, f3VecGradientSampleY, f3VecGradientSampleZ, tex3DChunks[2]); break;
	//case 3 : f3VecGrad = vxSampleVolumeGradientSamplerTex3DByCCF(f3PosSampleCTS, f3SizeVolumeCVS, f3VecGradientSampleX, f3VecGradientSampleY, f3VecGradientSampleZ, tex3DChunks[3]); break;
	//case 4 : f3VecGrad = vxSampleVolumeGradientSamplerTex3DByCCF(f3PosSampleCTS, f3SizeVolumeCVS, f3VecGradientSampleX, f3VecGradientSampleY, f3VecGradientSampleZ, tex3DChunks[4]); break;
	//case 5 : f3VecGrad = vxSampleVolumeGradientSamplerTex3DByCCF(f3PosSampleCTS, f3SizeVolumeCVS, f3VecGradientSampleX, f3VecGradientSampleY, f3VecGradientSampleZ, tex3DChunks[3]); break;
	//case 6 : f3VecGrad = vxSampleVolumeGradientSamplerTex3DByCCF(f3PosSampleCTS, f3SizeVolumeCVS, f3VecGradientSampleX, f3VecGradientSampleY, f3VecGradientSampleZ, tex3DChunks[6]); break;
	//case 7 : f3VecGrad = vxSampleVolumeGradientSamplerTex3DByCCF(f3PosSampleCTS, f3SizeVolumeCVS, f3VecGradientSampleX, f3VecGradientSampleY, f3VecGradientSampleZ, tex3DChunks[7]); break;
	//case 8 : f3VecGrad = vxSampleVolumeGradientSamplerTex3DByCCF(f3PosSampleCTS, f3SizeVolumeCVS, f3VecGradientSampleX, f3VecGradientSampleY, f3VecGradientSampleZ, tex3DChunks[8]); break;
	//case 9 : f3VecGrad = vxSampleVolumeGradientSamplerTex3DByCCF(f3PosSampleCTS, f3SizeVolumeCVS, f3VecGradientSampleX, f3VecGradientSampleY, f3VecGradientSampleZ, tex3DChunks[9]); break;
	}
#endif
	return f3VecGrad;
}

float3 vxSampleVolumeGradientSamplerTex3DViewOriented(float3 f3PosSample, float3 f3VecGradientSampleX, float3 f3VecGradientSampleY, float3 f3VecGradientSampleZ
	, float f3VecView, Texture3D tex3DVolume)
{
	float3 f3VecGrad = vxSampleVolumeGradientSamplerTex3D(f3PosSample, f3VecGradientSampleX, f3VecGradientSampleY, f3VecGradientSampleZ, tex3DVolume);
	float fDot = dot(f3VecGrad, f3VecView);
	if(fDot < 0)
		return -f3VecGrad;
	return f3VecGrad;
}

float vxSampleVolumeTrilinearTex2DArray(float3 f3PosSample, Texture2DArray tex2DArray)
{
	float3 f3PosSampleVS = f3PosSample;
	int3 i3PosSampleVS = int3(f3PosSampleVS); // floor
	float3 f3VecRatio = f3PosSampleVS - i3PosSampleVS;
	
	float fDensity0 = tex2DArray.Load( int4(i3PosSampleVS + int3(0, 0, 0), 0) ).r;
	float fDensity1 = tex2DArray.Load( int4(i3PosSampleVS + int3(1, 0, 0), 0) ).r;
	float fDensity2 = tex2DArray.Load( int4(i3PosSampleVS + int3(0, 1, 0), 0) ).r;
	float fDensity3 = tex2DArray.Load( int4(i3PosSampleVS + int3(1, 1, 0), 0) ).r;
	float fDensity4 = tex2DArray.Load( int4(i3PosSampleVS + int3(0, 0, 1), 0) ).r;
	float fDensity5 = tex2DArray.Load( int4(i3PosSampleVS + int3(1, 0, 1), 0) ).r;
	float fDensity6 = tex2DArray.Load( int4(i3PosSampleVS + int3(0, 1, 1), 0) ).r;
	float fDensity7 = tex2DArray.Load( int4(i3PosSampleVS + int3(1, 1, 1), 0) ).r;

	return vxTrilinearInterpolation(fDensity0, fDensity1, fDensity2, fDensity3, fDensity4, fDensity5, fDensity6, fDensity7, f3VecRatio);
}

float3 vxSampleVolumeGradientTrilinearTex2DArray(float3 f3PosSample, Texture2DArray tex2DArray)
{
	float3 f3PosSampleVS = f3PosSample;
	int3 i3PosSampleVS = int3(f3PosSampleVS); // floor
	float3 f3VecRatio = f3PosSampleVS - i3PosSampleVS;
	
	float fDensity0 = tex2DArray.Load( int4(i3PosSampleVS + int3(0, 0, 0), 0) ).r;
	float fDensity1 = tex2DArray.Load( int4(i3PosSampleVS + int3(1, 0, 0), 0) ).r;
	float fDensity2 = tex2DArray.Load( int4(i3PosSampleVS + int3(0, 1, 0), 0) ).r;
	float fDensity3 = tex2DArray.Load( int4(i3PosSampleVS + int3(1, 1, 0), 0) ).r;
	float fDensity4 = tex2DArray.Load( int4(i3PosSampleVS + int3(0, 0, 1), 0) ).r;
	float fDensity5 = tex2DArray.Load( int4(i3PosSampleVS + int3(1, 0, 1), 0) ).r;
	float fDensity6 = tex2DArray.Load( int4(i3PosSampleVS + int3(0, 1, 1), 0) ).r;
	float fDensity7 = tex2DArray.Load( int4(i3PosSampleVS + int3(1, 1, 1), 0) ).r;
	
	float fDensity00 = tex2DArray.Load( int4(i3PosSampleVS + int3(-1, 0, 0), 0) ).r;
	float fDensity01 = tex2DArray.Load( int4(i3PosSampleVS + int3(-1, 1, 0), 0) ).r;
	float fDensity02 = tex2DArray.Load( int4(i3PosSampleVS + int3(-1, 0, 1), 0) ).r;
	float fDensity03 = tex2DArray.Load( int4(i3PosSampleVS + int3(-1, 1, 1), 0) ).r;

	float fDensity10 = tex2DArray.Load( int4(i3PosSampleVS + int3(2, 0, 0), 0) ).r;
	float fDensity11 = tex2DArray.Load( int4(i3PosSampleVS + int3(2, 1, 0), 0) ).r;
	float fDensity12 = tex2DArray.Load( int4(i3PosSampleVS + int3(2, 0, 1), 0) ).r;
	float fDensity13 = tex2DArray.Load( int4(i3PosSampleVS + int3(2, 1, 1), 0) ).r;

	float fDensity20 = tex2DArray.Load( int4(i3PosSampleVS + int3(0, -1, 0), 0) ).r;
	float fDensity21 = tex2DArray.Load( int4(i3PosSampleVS + int3(1, -1, 0), 0) ).r;
	float fDensity22 = tex2DArray.Load( int4(i3PosSampleVS + int3(0, -1, 1), 0) ).r;
	float fDensity23 = tex2DArray.Load( int4(i3PosSampleVS + int3(1, -1, 1), 0) ).r;

	float fDensity30 = tex2DArray.Load( int4(i3PosSampleVS + int3(0, 2, 0), 0) ).r;
	float fDensity31 = tex2DArray.Load( int4(i3PosSampleVS + int3(1, 2, 0), 0) ).r;
	float fDensity32 = tex2DArray.Load( int4(i3PosSampleVS + int3(0, 2, 1), 0) ).r;
	float fDensity33 = tex2DArray.Load( int4(i3PosSampleVS + int3(1, 2, 1), 0) ).r;

	float fDensity40 = tex2DArray.Load( int4(i3PosSampleVS + int3(0, 0, -1), 0) ).r;
	float fDensity41 = tex2DArray.Load( int4(i3PosSampleVS + int3(1, 0, -1), 0) ).r;
	float fDensity42 = tex2DArray.Load( int4(i3PosSampleVS + int3(0, 1, -1), 0) ).r;
	float fDensity43 = tex2DArray.Load( int4(i3PosSampleVS + int3(1, 1, -1), 0) ).r;
	
	float fDensity50 = tex2DArray.Load( int4(i3PosSampleVS + int3(0, 0, 2), 0) ).r;
	float fDensity51 = tex2DArray.Load( int4(i3PosSampleVS + int3(1, 0, 2), 0) ).r;
	float fDensity52 = tex2DArray.Load( int4(i3PosSampleVS + int3(0, 1, 2), 0) ).r;
	float fDensity53 = tex2DArray.Load( int4(i3PosSampleVS + int3(1, 1, 2), 0) ).r;

	float gx =
		vxTrilinearInterpolation(fDensity1, fDensity10, fDensity3, fDensity11, fDensity5, fDensity12, fDensity7, fDensity13, f3VecRatio) -
		vxTrilinearInterpolation(fDensity00, fDensity0, fDensity01, fDensity2, fDensity02, fDensity4, fDensity03, fDensity6, f3VecRatio);
	float gy =
		vxTrilinearInterpolation(fDensity2, fDensity3, fDensity30, fDensity31, fDensity6, fDensity7, fDensity32, fDensity33, f3VecRatio) -
		vxTrilinearInterpolation(fDensity20, fDensity21, fDensity0, fDensity1, fDensity22, fDensity23, fDensity4, fDensity5, f3VecRatio);
	float gz =
		vxTrilinearInterpolation(fDensity4, fDensity5, fDensity6, fDensity7, fDensity50, fDensity51, fDensity52, fDensity53, f3VecRatio) -
		vxTrilinearInterpolation(fDensity40, fDensity41, fDensity42, fDensity43, fDensity0, fDensity1, fDensity2, fDensity3, f3VecRatio);

	return float3(gx, gy, gz);
}

float vxComputeSpecularStaticBRDF(float3 f3VecView, float3 f3VecLight, float3 f3VecNormal, float3 f3VecReflect, float fWeightU, float fWeightV, float fReflectanceRatio)
{
	float3 f3VecHalf = normalize(f3VecView + f3VecLight);
	float fHK = max(dot(f3VecHalf, f3VecReflect), 0);
	float fFresnelFraction = fReflectanceRatio + (1 - fReflectanceRatio)*pow((1 - fHK), 5);
	float fNK1 = dot(f3VecNormal, f3VecView);
	float fNK2 = max(dot(f3VecNormal, f3VecLight), 0);
	float fMaxNK12 = max(fNK1, fNK2);
	float fNH = max(dot(f3VecHalf, f3VecNormal), 0);

	float3 f3VecOrthoU = float3(1, 0, 0), f3VecOrthoV;
	float fLength = 0;
	while((fLength = length(f3VecOrthoV = cross(f3VecNormal, f3VecOrthoU))) < 0.000001f)
	{
		f3VecOrthoU.z += 1;
	}
	f3VecOrthoV /= fLength;
	f3VecOrthoU = normalize(cross(f3VecOrthoV, f3VecNormal));

	float fSpecularPower = fWeightU*pow(dot(f3VecHalf, f3VecOrthoU), 2) + fWeightV*pow(dot(f3VecHalf, f3VecOrthoV), 2) / (1 - pow(fNH, 2));

	return sqrt((fWeightU + 1)*(fWeightV + 1))/(8*M_PI)*pow(fNH, fSpecularPower)*fFresnelFraction;///max((fHK*fMaxNK12), 0.5);
}

float vxComputeDiffuseStaticBRDF(float3 f3VecView, float3 f3VecLight, float3 f3VecNormal, float fDiffuseRatio, float fReflectanceRatio)
{
	float fNK1 = dot(f3VecNormal, f3VecView);
	float fNK2 = max(dot(f3VecNormal, f3VecLight), 0);

	return 28*fDiffuseRatio*(1 - fReflectanceRatio)/(23*M_PI)*(1 - pow(1 - fNK1/2, 5))*(1 - pow(1 - fNK2/2, 5));
}

//=====================
// Constant Buffers
//=====================
struct VxVolumeObject
{
	// Volume Information and Clipping Information
	float4x4 matWS2TS;	// for Sampling and Ray Traversing
	float4x4 matAlginedWS;
	float4x4 matClipBoxTransform;  // To Clip Box Space (BS)
	
	// 1st bit - Clipping Plane (1) or Not (0)
	// 2nd bit - Clipping Box (1) or Not (0)
	// 3rd bit - Brick Show (1) or Not (0)
	int	iFlags;
	float3 f3PosClipPlaneWS;

	float3 f3PosVObjectMaxWS;
	float	fSampleValueRange;	// Value Range;

	float3 f3ClipBoxOrthoMaxWS;
	float	fClipPlaneIntensity;

	float4 f4ShadingFactor;

	float3 f3VecClipPlaneWS;
	float	fSampleDistWS;

	float3 f3VecGradientSampleX;
	float	fOpaqueCorrection;

	float3 f3VecGradientSampleY;
	int iDummy0___;

	float3 f3VecGradientSampleZ;
	float fThicknessVirtualzSlab;	// This is used in Opaque Surface's Virtual zSlab's Size

	float3 f3SizeVolumeCVS;
	float fVoxelSharpnessForAttributeVolume;
};

struct VxBlockObject
{
	float3 f3SizeBlockTS;
	float fSampleValueRange;
};

struct VxBrickChunk
{
	// TS in brick Chunk not in main Volume
	float3 f3SizeBrickFullTS;
	int iNumBricksInChunkXY;

	float3 f3SizeBrickExTS;
	int iNumBricksInChunkX;

	float3 f3ConvertRatioVTS2CTS;
	int iDummy___;
};

struct VxCameraState
{
	float4x4 matSS2WS;	// for initialization vectors and positions
	float4x4 matRS2SS;	// for initialization vectors and positions

	float3 f3PosEyeWS;
	uint		uiGridOffsetX;

	float3 f3VecViewWS;	// Recommend Normalize Vector for Computing in Single Precision (Float Type)
	uint		uiScreenSizeX;

	float3 f3VecLightWS;
	uint		uiScreenSizeY;

	// 1st bit - 0 : Orthogonal or 1 : Perspective
	// 2nd bit - 0 : Parallel Light or 1 : Point Spot Light
	// 3rd and 4th bit - 00 : No Previous Out (clearing), 01 : Tex2D, 10 : other RWB,  11 : this RWB(without initialization)
	// 5th bit - 0 : This Pass is Previous Rendering, 1 : This Pass is Deferred Out Rendering
	// 6th bit - 1 : Just Copy-back end return process :: this is for TVCG experiment
	int	iFlags;
	float3 f3PosLightWS;

	uint		uiScreenAaBbMinX;
	uint		uiScreenAaBbMaxX;
	uint		uiScreenAaBbMinY;
	uint		uiScreenAaBbMaxY;

	uint		uiRenderingSizeX;
	uint		uiRenderingSizeY;
	uint		__dummy0;
	uint		__dummy1;
};

struct VxCameraProjectionState
{
	float4x4 matWS2PS;
	float4x4 matSS2WS;
	
	float3 f3VecLightWS;
	uint uiGridOffsetX;

	float3 f3PosLightWS;
	// 1st bit : 0 (orthogonal), 1 : (perspective)
	// 2nd bit : 0 (parallel), 1 : (spot)
	int iFlag;

	float3 f3PosEyeWS;
	uint uiScreenSizeX;

	float3 f3VecViewWS;
	uint uiScreenSizeY;
    
    uint k_value;
    uint iSrCamDummy__0;
    uint iSrCamDummy__1;
    uint iSrCamDummy__2;
};

struct VxPolygonObject
{
	float4x4 matOS2WS;
	float4x4 matOS2PS;
	float4 f4Color;
	float4 f4ShadingFactor;	// x : Ambient, y : Diffuse, z : Specular, w : specular
	
	float4x4 matClipBoxWS2BS;  // To Clip Box Space (BS)

	float3 f3ClipBoxMaxBS;
	// 1st bit : 0 (No) 1 (Clip Box)
	// 2nd bit : 0 (No) 1 (Clip plane)
	// 10th bit : 0 (No XFlip) 1 (XFlip)
	// 11th bit : 0 (No XFlip) 1 (YFlip)
	// 20th bit : 0 (No Dashed Line) 1 (Dashed Line)
	// 21th bit : 0 (Transparent Dash) 1 (Dash As Color Inverted)
	int iFlag;

	float3 f3PosClipPlaneWS;
    int num_letters;

	float3 f3VecClipPlaneWS;
    int outline_mode;
	
	float fDashDistance;
	float fShadingMultiplier;
	float fDepthForwardBias;
    float pix_thickness;
    
    float vzthickness;
    int num_safe_loopexit;
    int pobj_dummy_1;
    int pobj_dummy_2;
};

struct VxPolygonDeviation
{
	float fMinMappingValue;
	float fMaxMappingValue;
	int iChannelID;	// 0 to 2
	int iValueRange;

	int iIsoValueForVolume;
	int iSrDevDummy__0;
	int iSrDevDummy__1;
	int iSrDevDummy__2;
};

struct VxOtf1D
{
	float4 f4OtfColorEnd;

	int		iOtf1stValue;		// For ESS
	int		iOtfLastValue;
	int		iOtfSize;
	int		iDummy___;
};

struct VxProxyParameter
{
	float4x4 matOS2PS;

	float fVZThickness;
	uint uiGridOffsetX;
	uint uiScreenSizeX;
	uint uiScreenSizeY;
};

struct VxSurfaceEffect
{
	// Ratio //
	// 1st bit : AO or Not , 2nd bit : Anisotropic BRDF or Not , 3rd bit : Apply Shading Factor or Not
	// 4th bit : 0 : Normal Curvature Map (2D), 1 : Apply Concaveness
	// 5th bit : Concaveness Direction or Not
	int iFlagsForIsoSurfaceRendering;	

	int iOcclusionNumSamplesPerKernelRay;
	float fOcclusionSampleStepDistOfKernelRay;

	float fPhongBRDF_DiffuseRatio;
	float fPhongBRDF_ReflectanceRatio;
	float fPhongExpWeightU;
	float fPhongExpWeightV;

	// Curvature
	int iSizeCurvatureKernel;
	float fThetaCurvatureTable;
	float fRadiusCurvatureTable;
	
	float fDummy0;
	float fDummy1;
};

struct VxModulation
{
	float fGradMagAmplifier;	//[0.1, 500]
	float fMaxGradientSize;	//[0, ]
	float fdummy____0;
	float fdummy____1;
};

struct VxShadowMap
{
	float4x4 matWS2LS;	// for Sample Depth Map
	float4x4 matWS2WLS;	// for Depth Comparison And Storage

	float fShadowOcclusionWeight;	// NA
	float fSampleDistWLS;	// NA
	float fOpaqueCorrectionSMap;	// 
	float fDepthBiasSampleCount;	// 
};

struct VxInterestingRegion
{
	float3 f3PosPtn0WS;
	float fRadius0;
	float3 f3PosPtn1WS;
	float fRadius1;
	float3 f3PosPtn2WS;
	float fRadius2;
	int iNumRegions; // Max is 3
    float3 f3VrRoidummy___;
};

float2 vxVrComputeBoundary(float3 f3PosStartWS, float3 f3VecDirWS, VxVolumeObject volObj)
{
	// Compute VObject Box Enter and Exit //
	float2 f2PrevNextT = vxComputePrevNextBoxTransformBoundary(f3PosStartWS, volObj.f3PosVObjectMaxWS, f3VecDirWS, volObj.matAlginedWS);
	
	if(f2PrevNextT.y <= f2PrevNextT.x)
	{
		return f2PrevNextT;
	}
	
	// Custom Clip Plane //
	if( volObj.iFlags & 0x1 )
	{
		float2 f2ClipPlanePrevNextT = vxComputePrevNextOnClipPlane(f2PrevNextT.x, f2PrevNextT.y, volObj.f3PosClipPlaneWS, volObj.f3VecClipPlaneWS, f3PosStartWS, f3VecDirWS);
		
		f2PrevNextT.x = max(f2PrevNextT.x, f2ClipPlanePrevNextT.x);
		f2PrevNextT.y = min(f2PrevNextT.y, f2ClipPlanePrevNextT.y);

		if(f2PrevNextT.y <= f2PrevNextT.x)
		{
			return f2PrevNextT;
		}
	}

	if( volObj.iFlags & 0x2 )
	{
		float2 f2ClipBoxPrevNextT = vxComputePrevNextBoxTransformBoundary(f3PosStartWS, volObj.f3ClipBoxOrthoMaxWS, f3VecDirWS, volObj.matClipBoxTransform);
		
		f2PrevNextT.x = max(f2PrevNextT.x, f2ClipBoxPrevNextT.x);
		f2PrevNextT.y = min(f2PrevNextT.y, f2ClipBoxPrevNextT.y);
		
		if(f2PrevNextT.y <= f2PrevNextT.x)
		{
			return f2PrevNextT;
		}
	}
	
	return f2PrevNextT;
}

//iIndexBrick -= 1
float3 vxComputeBrickSamplePosInChunk(float3 f3PosSampleVTS, int iIndexBrick, VxBlockObject blkObj, VxBrickChunk blkChunk)
{
	// VTS : VxBlockObject, CTS : VxBrickChunk

	// VTS
	int3 i3BlockId = int3(f3PosSampleVTS.x / blkObj.f3SizeBlockTS.x, f3PosSampleVTS.y / blkObj.f3SizeBlockTS.y, f3PosSampleVTS.z / blkObj.f3SizeBlockTS.z);
	float3 f3PosSampleInBrickVTS;
	f3PosSampleInBrickVTS.x = f3PosSampleVTS.x - i3BlockId.x * blkObj.f3SizeBlockTS.x;
	f3PosSampleInBrickVTS.y = f3PosSampleVTS.y - i3BlockId.y * blkObj.f3SizeBlockTS.y;
	f3PosSampleInBrickVTS.z = f3PosSampleVTS.z - i3BlockId.z * blkObj.f3SizeBlockTS.z;

	// CTS
	float3 f3PosSampleInBrickCTS = float3(f3PosSampleInBrickVTS.x * blkChunk.f3ConvertRatioVTS2CTS.x
		, f3PosSampleInBrickVTS.y * blkChunk.f3ConvertRatioVTS2CTS.y
		, f3PosSampleInBrickVTS.z * blkChunk.f3ConvertRatioVTS2CTS.z);

	int3 i3BrickIdInChunk;
	i3BrickIdInChunk.z = (uint)iIndexBrick / (uint)blkChunk.iNumBricksInChunkXY;
	i3BrickIdInChunk.y = (uint)(iIndexBrick - i3BrickIdInChunk.z * blkChunk.iNumBricksInChunkXY) / (uint)blkChunk.iNumBricksInChunkX;
	i3BrickIdInChunk.x = iIndexBrick - i3BrickIdInChunk.y * blkChunk.iNumBricksInChunkX - i3BrickIdInChunk.z * blkChunk.iNumBricksInChunkXY;

	float3 f3PosSampleInChunkCTS;
	f3PosSampleInChunkCTS.x = f3PosSampleInBrickCTS.x + blkChunk.f3SizeBrickExTS.x + i3BrickIdInChunk.x * blkChunk.f3SizeBrickFullTS.x;
	f3PosSampleInChunkCTS.y = f3PosSampleInBrickCTS.y + blkChunk.f3SizeBrickExTS.y + i3BrickIdInChunk.y * blkChunk.f3SizeBrickFullTS.y;
	f3PosSampleInChunkCTS.z = f3PosSampleInBrickCTS.z + blkChunk.f3SizeBrickExTS.z;// + i3BrickIdInChunk.z * blkChunk.f3SizeBrickFullTS.z;

	return f3PosSampleInChunkCTS;
}

struct VxSurfaceRefinement
{
	float3 f3PosSurfaceWS;	// Last Valid Sample : Back Sample Position
	int iHitSampleStep;
};

struct VxSurfaceRefinement_Test
{
	float3 f3PosSurfaceWS;	// Last Valid Sample : Back Sample Position
	int iHitStatus;
	float3 f3VecNormalWS;	
	int iOutType;
};

struct VxBlockSkip 
{
	int iSampleBlockValue;
	int iNumStepSkip;
};

VxBlockSkip vxComputeBlockRayLengthBoundary(float3 f3PosStartTS, float3 f3VecSampleTS, VxBlockObject blkObj, Texture3D tex3DBlock)
{
	VxBlockSkip vxOut = (VxBlockSkip)0;
	int3 i3BlockID = int3(f3PosStartTS.x / blkObj.f3SizeBlockTS.x, f3PosStartTS.y / blkObj.f3SizeBlockTS.y, f3PosStartTS.z / blkObj.f3SizeBlockTS.z);
	vxOut.iSampleBlockValue = (int)(tex3DBlock.Load(int4(i3BlockID, 0)).r * blkObj.fSampleValueRange + 0.5f);

	float3 f3PosTargetBlockMinTS = float3(i3BlockID.x * blkObj.f3SizeBlockTS.x, i3BlockID.y * blkObj.f3SizeBlockTS.y, i3BlockID.z * blkObj.f3SizeBlockTS.z);
	float3 f3PosTargetBlockMaxTS = f3PosTargetBlockMinTS + blkObj.f3SizeBlockTS;
	float2 f2T = vxComputePrevNextAlignedBoxBoundary(f3PosStartTS, f3PosTargetBlockMinTS, f3PosTargetBlockMaxTS, f3VecSampleTS);
	float fDistanceSkipTS = f2T.y - f2T.x;
	vxOut.iNumStepSkip = ceil(fDistanceSkipTS);
	return vxOut;
};

float vxSampleInBrickChunkWithBlockValue(float3 f3PosSampleCTS, int iSampleBlockValue, VxBrickChunk blkChunk, Texture3D tex3DChunks[3])
{
	float fSampleValue = 0;
#ifdef ONLYSINGLECHUNK
	fSampleValue = tex3DChunks[0].SampleLevel(g_samplerLinear_Clamp, f3PosSampleCTS, 0).r;
#else
	switch((uint)iSampleBlockValue / (uint)blkChunk.iNumBricksInChunkXY)
	{
	case 0: fSampleValue = tex3DChunks[0].SampleLevel(g_samplerLinear_Clamp, f3PosSampleCTS, 0).r; break;
	case 1: fSampleValue = tex3DChunks[1].SampleLevel(g_samplerLinear_Clamp, f3PosSampleCTS, 0).r; break;
	case 2: fSampleValue = tex3DChunks[2].SampleLevel(g_samplerLinear_Clamp, f3PosSampleCTS, 0).r; break;
	//case 3 : fSampleValue = tex3DChunks[3].SampleLevel(g_samplerLinear_Clamp, f3PosSampleCTS, 0).r; break;
	//case 4 : fSampleValue = tex3DChunks[4].SampleLevel(g_samplerLinear_Clamp, f3PosSampleCTS, 0).r; break;
	//case 5 : fSampleValue = tex3DChunks[3].SampleLevel(g_samplerLinear_Clamp, f3PosSampleCTS, 0).r; break;
	//case 6 : fSampleValue = tex3DChunks[6].SampleLevel(g_samplerLinear_Clamp, f3PosSampleCTS, 0).r; break;
	//case 7 : fSampleValue = tex3DChunks[7].SampleLevel(g_samplerLinear_Clamp, f3PosSampleCTS, 0).r; break;
	//case 8 : fSampleValue = tex3DChunks[8].SampleLevel(g_samplerLinear_Clamp, f3PosSampleCTS, 0).r; break;
	//case 9 : fSampleValue = tex3DChunks[9].SampleLevel(g_samplerLinear_Clamp, f3PosSampleCTS, 0).r; break;
	}
#endif
	return fSampleValue;
}

float vxSampleInBrickChunk(float3 f3PosSampleTS, VxBlockObject blkObj, Texture3D tex3DBlock, VxBrickChunk blkChunk, Texture3D tex3DChunks[3])
{
	int3 i3BlockID = int3(f3PosSampleTS.x / blkObj.f3SizeBlockTS.x, f3PosSampleTS.y / blkObj.f3SizeBlockTS.y, f3PosSampleTS.z / blkObj.f3SizeBlockTS.z);
	int iSampleBlockValue = (int)(tex3DBlock.Load(int4(i3BlockID, 0)).r * blkObj.fSampleValueRange + 0.5f);
	float fSampleValue = 0;
	if(iSampleBlockValue > 0)
	{
		float3 f3PosSampleCTS = vxComputeBrickSamplePosInChunk(f3PosSampleTS, iSampleBlockValue - 1, blkObj, blkChunk);
		fSampleValue = vxSampleInBrickChunkWithBlockValue(f3PosSampleCTS, iSampleBlockValue - 1, blkChunk, tex3DChunks);
	}
	return fSampleValue;
}

float2 vxSampleInBrickChunkByCCFWithBlockValue(float3 f3PosSampleCTS, int iSampleBlockValue, VxVolumeObject volObj, VxBrickChunk blkChunk, Texture3D tex3DChunks[3])
{
	float2 f2SampleValues = (float2)0;
#ifdef ONLYSINGLECHUNK
	f2SampleValues = vxSampleByCCF(f3PosSampleCTS, volObj.f3SizeVolumeCVS, tex3DChunks[0]);
#else
	switch((uint)iSampleBlockValue / (uint)blkChunk.iNumBricksInChunkXY)
	{
	case 0 : f2SampleValues = vxSampleByCCF(f3PosSampleCTS, volObj.f3SizeVolumeCVS, tex3DChunks[0]); break;
	case 1 : f2SampleValues = vxSampleByCCF(f3PosSampleCTS, volObj.f3SizeVolumeCVS, tex3DChunks[1]); break;
	case 2 : f2SampleValues = vxSampleByCCF(f3PosSampleCTS, volObj.f3SizeVolumeCVS, tex3DChunks[2]); break;
	//case 3 : f2SampleValues = vxSampleByCCF(f3PosSampleCTS, volObj.f3SizeVolumeCVS, tex3DChunks[3]); break;
	//case 4 : f2SampleValues = vxSampleByCCF(f3PosSampleCTS, volObj.f3SizeVolumeCVS, tex3DChunks[4]); break;
	//case 5 : f2SampleValues = vxSampleByCCF(f3PosSampleCTS, volObj.f3SizeVolumeCVS, tex3DChunks[3]); break;
	//case 6 : f2SampleValues = vxSampleByCCF(f3PosSampleCTS, volObj.f3SizeVolumeCVS, tex3DChunks[6]); break;
	//case 7 : f2SampleValues = vxSampleByCCF(f3PosSampleCTS, volObj.f3SizeVolumeCVS, tex3DChunks[7]); break;
	//case 8 : f2SampleValues = vxSampleByCCF(f3PosSampleCTS, volObj.f3SizeVolumeCVS, tex3DChunks[8]); break;
	//case 9 : f2SampleValues = vxSampleByCCF(f3PosSampleCTS, volObj.f3SizeVolumeCVS, tex3DChunks[9]); break;
	}
#endif
	return f2SampleValues;
}

float2 vxSampleInBrickChunkByCCF(float3 f3PosSampleTS, VxVolumeObject volObj, VxBlockObject blkObj, Texture3D tex3DBlock, VxBrickChunk blkChunk, Texture3D tex3DChunks[3])
{
	int3 i3BlockID = int3(f3PosSampleTS.x / blkObj.f3SizeBlockTS.x, f3PosSampleTS.y / blkObj.f3SizeBlockTS.y, f3PosSampleTS.z / blkObj.f3SizeBlockTS.z);
	int iSampleBlockValue = (int)(tex3DBlock.Load(int4(i3BlockID, 0)).r * blkObj.fSampleValueRange + 0.5f);
	float2 f2SampleValues = (float2)0;
	if(iSampleBlockValue > 0)
	{
		float3 f3PosSampleCTS = vxComputeBrickSamplePosInChunk(f3PosSampleTS, iSampleBlockValue - 1, blkObj, blkChunk);
		f2SampleValues = vxSampleInBrickChunkByCCFWithBlockValue(f3PosSampleCTS, iSampleBlockValue - 1, volObj, blkChunk, tex3DChunks);
	}
	return f2SampleValues;
}

/*
bool VxIntersectedTri2(float3 f3VerticeOnTriangle[3], float3 vLine[2]) 
{ 
	float3 v1=vPoly[1]-vPoly[0]; 
	float3 v2=vPoly[2]-vPoly[0]; 
	float3 vNormal; //법선 구함 
	CROSS(v1,v2,vNormal); //외적 vNormal = v1 x v2 
	vNormal.Normalize(); //정규화 
	const float d= -Dot(vNormal,vPoly[0]); 
	const float i0=Dot(vNormal,vLine[0]), 
	i1=Dot(vNormal,vLine[1]); //내적 

	//평면을 통과하는지 
	const float t=-(i1+d)/(i0-i1); 
	if (t<0.f || t>1.f) 
	return false; 

	//평면과 교차점 
	float3 vIntersection;

	vIntersection.x=vLine[0].x*t+vLine[1].x*(1.f-t); 
	vIntersection.y=vLine[0].y*t+vLine[1].y*(1.f-t); 
	vIntersection.z=vLine[0].z*t+vLine[1].z*(1.f-t); 

	//점이 삼각형 안에 있는지 판단 
	const float tempa = Dot(v1,v1); //내적 
	const float tempb =Dot(v1,v2); //내적 
	const float tempc = Dot(v2,v2); //내적 
	const float ac_bb=(tempa*tempc)-(tempb*tempb); 
	float3 vp=vIntersection-vPoly[0]; 
	const float tempd = Dot(vp,v1); //내적 
	const float tempe = Dot(vp,v2); //내적 
	const float x = (tempd*tempc)-(tempe*tempb); 
	const float y = (tempe*tempa)-(tempd*tempb); 
	const float z = x+y-ac_bb; 
	return (( in(z)& ~(in(x)|in(y)) ) & 0x80000000); 
} /**/



#define NUM_SAMPLES 5
#ifdef SAMPLE_FROM_BRICKCHUNK
float vxComputeOcclusion(float3 f3VecNormal, float3 f3PosSample, int iNumRayStep, float fSampleDist, float4x4 matXS2TS, float fSampleValueRange, int iOtf1stValue,
	VxBlockObject blkObj, Texture3D tex3DBlock, VxBrickChunk blkChunk, Texture3D tex3DChunks[3])
#else
float vxComputeOcclusion(float3 f3VecNormal, float3 f3PosSample, int iNumRayStep, float fSampleDist, float4x4 matXS2TS, float fSampleValueRange, int iOtf1stValue,
	Texture3D tex3DVolume)
#endif
{
	float3 f3VecDirSamples[NUM_SAMPLES];
	float3 f3VecNormalUnit = normalize(f3VecNormal);
	float3 f3VecUp = float3(0, 0, 1);
	float3 f3VecTmp = cross(f3VecNormalUnit, f3VecUp);
	if (length(f3VecTmp) < 0.000001f)
		f3VecUp = float3(0, 1, 0);
	float3 f3VecRight = cross(f3VecNormalUnit, f3VecUp);
	f3VecUp = cross(f3VecRight, f3VecNormalUnit);

	f3VecDirSamples[0] = f3VecNormalUnit;
	f3VecDirSamples[1] = normalize(f3VecNormalUnit + f3VecRight);
	f3VecDirSamples[2] = normalize(f3VecNormalUnit + f3VecUp);
	f3VecDirSamples[3] = normalize(f3VecNormalUnit - f3VecRight);
	f3VecDirSamples[4] = normalize(f3VecNormalUnit - f3VecUp);

	float fOcclusion = 0;//NUM_SAMPLES;

	[unroll]
	for(int i = 0; i < NUM_SAMPLES; i++)
	{
		float3 f3VecDirSample = f3VecDirSamples[i]*fSampleDist;
		for(int j = 0; j < iNumRayStep; j++)
		{
			float3 f3PosOccSample = f3PosSample + f3VecDirSample*(j + 1);
			float3 f3PosOccSampleTS = mul( float4(f3PosOccSample, 1.f), matXS2TS ).xyz;
#ifdef SAMPLE_FROM_BRICKCHUNK
			int iSampleValue = (int)(vxSampleInBrickChunk(f3PosOccSampleTS, blkObj, tex3DBlock, blkChunk, tex3DChunks) * fSampleValueRange + 0.5f);
#else
			int iSampleValue = (int)(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosOccSampleTS, 0).r * fSampleValueRange + 0.5f);
#endif
			if(iSampleValue >= iOtf1stValue)
			{
				fOcclusion += 1.f;
				//break;
			}
		}
	}

	return fOcclusion;
}

VxSurfaceRefinement vxVrMaskSurfaceRefinementWithBlockSkipping(float3 f3PosStartWS, float3 f3VecDirUnitWS, int iNumSamples,
	VxVolumeObject volObj, Texture3D tex3DVolume, Texture3D tex3DVolumeMask, VxBlockObject blkObj, Texture3D tex3DBlock, VxOtf1D otf1D)
{
	VxSurfaceRefinement vxOut = (VxSurfaceRefinement)0;
	vxOut.f3PosSurfaceWS = f3PosStartWS;
	vxOut.iHitSampleStep = -1;

	float4 f4ColorOTF = (float4)0;
	float3 f3PosStartTS = vxTransformPoint(f3PosStartWS, volObj.matWS2TS);
	float3 f3VecSampleDirWS = f3VecDirUnitWS * volObj.fSampleDistWS;
	float3 f3VecSampleDirTS = vxTransformVector(f3VecSampleDirWS, volObj.matWS2TS);
	int iSculptValue = (int)(volObj.iFlags >> 24);

	int iSampleValue = 0;

	iSampleValue = (int)(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosStartTS, 0).r * volObj.fSampleValueRange + 0.5f);

	if (iSampleValue >= otf1D.iOtf1stValue)
	{
		vxOut.iHitSampleStep = 0;
		return vxOut;
	}

	[loop]
	for (int i = 1; i < iNumSamples; i++)
	{
		float3 f3PosSampleWS = f3PosStartWS + f3VecSampleDirWS*(float)i;
		float3 f3PosSampleTS = vxTransformPoint(f3PosSampleWS, volObj.matWS2TS);

		VxBlockSkip blkSkip = vxComputeBlockRayLengthBoundary(f3PosSampleTS, f3VecSampleDirTS, blkObj, tex3DBlock);
		blkSkip.iNumStepSkip = min(max(1, blkSkip.iNumStepSkip), iNumSamples - i);

		if (blkSkip.iSampleBlockValue > 0)
		{
			for (int k = 0; k < blkSkip.iNumStepSkip; k++, i++)
			{
				float3 f3PosSampleInBlockWS = f3PosStartWS + f3VecSampleDirWS*(float)i;
				float3 f3PosSampleInBlockTS = vxTransformPoint(f3PosSampleInBlockWS, volObj.matWS2TS);

				float fMaskVisibility = vxSampleSculptMaskWeightByCCF(f3PosSampleInBlockTS, volObj.f3SizeVolumeCVS, tex3DVolumeMask, iSculptValue);
				iSampleValue = (int)(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleInBlockTS, 0).r * volObj.fSampleValueRange + 0.5f);

				if (fMaskVisibility > 0 && iSampleValue >= otf1D.iOtf1stValue)
				{
					// Surface refinement //
					// Interval bisection
					float3 f3PosBisectionStartWS = f3PosSampleInBlockWS - f3VecSampleDirWS;
					float3 f3PosBisectionEndWS = f3PosSampleInBlockWS;

					for (int j = 0; j < SURFACEREFINEMENTNUM; j++)
					{
						float3 f3PosBisectionWS = (f3PosBisectionStartWS + f3PosBisectionEndWS)*0.5f;
						float3 f3PosBisectionTS = vxTransformPoint(f3PosBisectionWS, volObj.matWS2TS);

						fMaskVisibility = vxSampleSculptMaskWeightByCCF(f3PosSampleInBlockTS, volObj.f3SizeVolumeCVS, tex3DVolumeMask, iSculptValue);
						iSampleValue = (int)(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosBisectionTS, 0).r * volObj.fSampleValueRange + 0.5f);

						if (fMaskVisibility > 0 && iSampleValue >= otf1D.iOtf1stValue)
						{
							f3PosBisectionEndWS = f3PosBisectionWS;
						}
						else
						{
							f3PosBisectionStartWS = f3PosBisectionWS;
						}
					}
					vxOut.f3PosSurfaceWS = f3PosBisectionEndWS;
					vxOut.iHitSampleStep = i;
					i = iNumSamples + 1;
					k = blkSkip.iNumStepSkip + 1;
					break;
				}	// if(iSampleValueNext >= otf1D.iOtf1stValue)
			} // for(int k = 0; k < blkSkip.iNumStepSkip; k++, i++)
		}
		else
		{
			i += blkSkip.iNumStepSkip;
		}
		// this is for outer loop's i++
		i -= 1;
	}

	return vxOut;
}

VxSurfaceRefinement_Test vxVrSurfaceRefinementWithBlockSkipping_TEST(float3 f3PosStartWS, float3 f3VecDirUnitWS, int iNumSamples, 
	int iDensityErrTolerence0, int iDensityErrTolerence1, int iSizeCurvatureKernel, float fThresholdGradSqLength, float fThresholdLaplacian,
	VxVolumeObject volObj, Texture3D tex3DVolume, VxBlockObject blkObj, Texture3D tex3DBlock, VxOtf1D otf1D, Buffer<float4> f4bufWindowing)
{
	const int __iIsoValue = otf1D.iOtf1stValue;
	const int __iMinIsoValue = iDensityErrTolerence0;
	const int __iMaxIsoValue = iDensityErrTolerence1;
	const float __fSampleFactor = (float)iSizeCurvatureKernel;
	const float __fThresGradientLength = fThresholdGradSqLength;
	const float __fCarvingDepth = fThresholdLaplacian * 5;

	VxSurfaceRefinement_Test vxOut = (VxSurfaceRefinement_Test)0;
	vxOut.f3PosSurfaceWS = f3PosStartWS;
	vxOut.iHitStatus = -1;

	float4 f4ColorOTF = (float4)0;
	float3 f3PosStartTS = vxTransformPoint(f3PosStartWS, volObj.matWS2TS);
	float3 f3VecSampleDirWS = f3VecDirUnitWS * volObj.fSampleDistWS;
	float3 f3VecSampleDirTS = vxTransformVector(f3VecSampleDirWS, volObj.matWS2TS);
	int iSculptValue = (int)(volObj.iFlags >> 24);
	float fSampleValue = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosStartTS, 0).r;
	int iSampleValue = (int)(fSampleValue * volObj.fSampleValueRange + 0.5f);

	if (iSampleValue >= __iIsoValue)
	{
		vxOut.iHitStatus = 0;
		return vxOut;
	}
	const float __fMinIsoValue = ((float)__iMinIsoValue - 0.5f) / volObj.fSampleValueRange;
	const float __fMaxIsoValue = ((float)__iMaxIsoValue - 0.5f) / volObj.fSampleValueRange;
	const float3 __f3VecGradientSampleX = volObj.f3VecGradientSampleX * __fSampleFactor;
	const float3 __f3VecGradientSampleY = volObj.f3VecGradientSampleY * __fSampleFactor;
	const float3 __f3VecGradientSampleZ = volObj.f3VecGradientSampleZ * __fSampleFactor;

#define INDEXSAMPLE(a) (int)(a * volObj.fSampleValueRange + 0.5f)

	float i = 1.f;
	float fNumSamples = (float)iNumSamples;

	float3 f3PosSampleWS = f3PosStartWS + f3VecSampleDirWS;
	float3 f3PosSampleTS = vxTransformPoint(f3PosSampleWS, volObj.matWS2TS);
	fSampleValue = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS, 0).r;

	[loop]
	while (i < fNumSamples)
	{
		VxBlockSkip blkSkip = vxComputeBlockRayLengthBoundary(f3PosSampleTS, f3VecSampleDirTS, blkObj, tex3DBlock);
		float fNumStepSkip = min(max(1, blkSkip.iNumStepSkip), fNumSamples - i);

		if (blkSkip.iSampleBlockValue > 0)
		{
			float k = 0;
			while (k < fNumStepSkip)
			{
				if (fSampleValue >= __fMinIsoValue) // no need for 1st hit surface refinement //
				{
					float3 f3PosSampleOuterWS = f3PosSampleWS;

					const float __fSampleNumStep = 0.2f;
					int iTracingCount = 1;
					while (i < fNumSamples && fSampleValue >= __fMinIsoValue)
					{
						//vxOut.f3PosSurfaceWS = f3PosSampleWS;
						//vxOut.iOutType = 4;
						//vxOut.iHitStatus = 1;
						//float3 f3PosSurfaceTS = vxTransformPoint(vxOut.f3PosSurfaceWS, volObj.matWS2TS);
						//float3 f3VecGradientWS = vxSampleVolumeGradientSamplerTex3D(f3PosSurfaceTS,
						//	volObj.f3VecGradientSampleX, volObj.f3VecGradientSampleY, volObj.f3VecGradientSampleZ, tex3DVolume);
						//vxOut.f3VecNormalWS = -normalize(f3VecGradientWS);
						//i = fNumSamples + 1.f; // in order to exit while (i < fNumSamples)
						//k = fNumStepSkip + 1.f; // in order to exit while (k < fNumStepSkip)
						//break;

						// the surface searching process //
						float fSampleValueXR = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + __f3VecGradientSampleX, 0).r;
						float fSampleValueXL = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - __f3VecGradientSampleX, 0).r;
						float fSampleValueYR = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + __f3VecGradientSampleY, 0).r;
						float fSampleValueYL = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - __f3VecGradientSampleY, 0).r;
						float fSampleValueZR = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + __f3VecGradientSampleZ, 0).r;
						float fSampleValueZL = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - __f3VecGradientSampleZ, 0).r;
						float fOpacityXR = f4bufWindowing[INDEXSAMPLE(fSampleValueXR)].w;
						float fOpacityXL = f4bufWindowing[INDEXSAMPLE(fSampleValueXL)].w;
						float fOpacityYR = f4bufWindowing[INDEXSAMPLE(fSampleValueYR)].w;
						float fOpacityYL = f4bufWindowing[INDEXSAMPLE(fSampleValueYL)].w;
						float fOpacityZR = f4bufWindowing[INDEXSAMPLE(fSampleValueZR)].w;
						float fOpacityZL = f4bufWindowing[INDEXSAMPLE(fSampleValueZL)].w;

						float3 f3VecGradient = float3(fOpacityXR - fOpacityXL, fOpacityYR - fOpacityYL, fOpacityZR - fOpacityZL) / 2.0f;
						//float3 f3VecGradient = float3(fSampleValueXR - fSampleValueXL, fSampleValueYR - fSampleValueYL, fSampleValueZR - fSampleValueZL) / 2.0f;
						float fGradientLength = length(f3VecGradient);

						if (fSampleValue >= __fMaxIsoValue) // exit with best fit surface 
						{
							// Surface refinement //
							// Interval bisection
							float3 f3PosBisectionStartWS = f3PosSampleWS - f3VecSampleDirWS * __fSampleNumStep;
							if (iTracingCount == 1)
								f3PosBisectionStartWS = f3PosSampleWS - f3VecSampleDirWS;
							float3 f3PosBisectionEndWS = f3PosSampleWS;
#ifdef __SURFREFINE__
							for (int m = 0; m < SURFACEREFINEMENTNUM; m++)
							{
								float3 f3PosBisectionWS = (f3PosBisectionStartWS + f3PosBisectionEndWS)*0.5f;
								float3 f3PosBisectionTS = vxTransformPoint(f3PosBisectionWS, volObj.matWS2TS);
								float fSampleValueBisection = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosBisectionTS, 0).r;
								if (fSampleValueBisection >= __fMaxIsoValue)
									f3PosBisectionEndWS = f3PosBisectionWS;
								else
									f3PosBisectionStartWS = f3PosBisectionWS;
							}
#endif
							vxOut.f3PosSurfaceWS = f3PosBisectionEndWS;
							vxOut.iOutType = 4;
							//if (iTracingCount == 1)
							//	vxOut.iOutType = 1;
							vxOut.iHitStatus = 1;
							float3 f3PosSurfaceTS = vxTransformPoint(vxOut.f3PosSurfaceWS, volObj.matWS2TS);
							float3 f3VecGradientWS = vxSampleVolumeGradientSamplerTex3D(f3PosSurfaceTS,
								volObj.f3VecGradientSampleX, volObj.f3VecGradientSampleY, volObj.f3VecGradientSampleZ, tex3DVolume);
							vxOut.f3VecNormalWS = -normalize(f3VecGradientWS);

							i = fNumSamples + 1.f; // in order to exit while (i < fNumSamples)
							k = fNumStepSkip + 1.f; // in order to exit while (k < fNumStepSkip)
							break;	// in order to exit while (i < fNumSamples && fSampleValue >= __fMinIsoValue)
						}
						else if (fGradientLength < __fThresGradientLength)	// exit with depth correction surface for false surface case 
						{
							// depth outer surface refinement correspoing to __fMinIsoValue //
							float3 f3PosBisectionStartWS = f3PosSampleOuterWS - f3VecSampleDirWS;
							float3 f3PosBisectionEndWS = f3PosSampleOuterWS;
#ifdef __SURFREFINE__
							for (int m = 0; m < SURFACEREFINEMENTNUM; m++)
							{
								float3 f3PosBisectionWS = (f3PosBisectionStartWS + f3PosBisectionEndWS)*0.5f;
								float3 f3PosBisectionTS = vxTransformPoint(f3PosBisectionWS, volObj.matWS2TS);
								float fSampleValueBisection = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosBisectionTS, 0).r;
								if (fSampleValueBisection >= __fMinIsoValue)
									f3PosBisectionEndWS = f3PosBisectionWS;
								else
									f3PosBisectionStartWS = f3PosBisectionWS;
							}
#endif
							float3 f3PosRefinedOuterSurfWS = f3PosBisectionEndWS;
							float3 f3PosRefinedOuterSurfTS = vxTransformPoint(f3PosRefinedOuterSurfWS, volObj.matWS2TS);

							float3 f3VecGradientWS = vxSampleVolumeGradientSamplerTex3D(f3PosRefinedOuterSurfTS,
								volObj.f3VecGradientSampleX, volObj.f3VecGradientSampleY, volObj.f3VecGradientSampleZ, tex3DVolume);
							f3VecGradientWS = normalize(f3VecGradientWS);

							// depth correction //
							float fAngle = acos(dot(f3VecGradientWS, f3VecDirUnitWS));
							float fCosAngle = cos(fAngle);
							if (fCosAngle < 0.001f) // false case!
							{
								vxOut.iOutType = 1;
								vxOut.f3PosSurfaceWS = f3PosRefinedOuterSurfWS;
							}
							else
							{
								vxOut.iOutType = 2;
								float fCarvingDepth = __fCarvingDepth / fCosAngle;
								vxOut.f3PosSurfaceWS = f3PosRefinedOuterSurfWS + f3VecDirUnitWS * fCarvingDepth;
							}

							// use the outer surface normal as the correction surface normal
							vxOut.f3VecNormalWS = -f3VecGradientWS;
							vxOut.iHitStatus = 1;

							i = fNumSamples + 1.f; // in order to exit while (i < fNumSamples)
							k = fNumStepSkip + 1.f; // in order to exit while (k < fNumStepSkip)
							break;	// in order to exit while (i < fNumSamples && fSampleValue >= __fMinIsoValue)
						}

						f3PosSampleWS += f3VecSampleDirWS * __fSampleNumStep;
						i += __fSampleNumStep;

						f3PosSampleTS = vxTransformPoint(f3PosSampleWS, volObj.matWS2TS);
						fSampleValue = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS, 0).r;

						iTracingCount++;
					} // while (i < fNumSamples)
				}
				else // if (fSampleValueForOuterSurf >= __fMinIsoValue)
				{
					f3PosSampleWS += f3VecSampleDirWS;
					k += 1.f;
					i += 1.f;

					f3PosSampleTS = vxTransformPoint(f3PosSampleWS, volObj.matWS2TS);
					fSampleValue = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS, 0).r;
				}
			} // while (k < fNumStepSkip)
		} // if (blkSkip.iSampleBlockValue > 0)
		else
		{
			f3PosSampleWS += f3VecSampleDirWS * fNumStepSkip;
			i += fNumStepSkip;

			f3PosSampleTS = vxTransformPoint(f3PosSampleWS, volObj.matWS2TS);
			fSampleValue = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS, 0).r;
		}
	}

	return vxOut;
}

VxSurfaceRefinement_Test vxVrSurfaceRefinementWithBlockSkipping_TEST_1(float3 f3PosStartWS, float3 f3VecDirUnitWS, int iNumSamples,
	int iDensityErrTolerence0, int iDensityErrTolerence1, int iSizeCurvatureKernel, float fThresholdGradSqLength, float fThresholdLaplacian,
	VxVolumeObject volObj, Texture3D tex3DVolume, VxBlockObject blkObj, Texture3D tex3DBlock, VxOtf1D otf1D, Buffer<float4> f4bufWindowing)
{
	const int __iIsoValue = otf1D.iOtf1stValue;
	const int __iMinIsoValue = iDensityErrTolerence0;
	const int __iMaxIsoValue = iDensityErrTolerence1;
	const float __fSampleFactor = (float)iSizeCurvatureKernel;
	const float __fThresGradientLength = fThresholdGradSqLength;
	const float __fCarvingDepth = fThresholdLaplacian;

	VxSurfaceRefinement_Test vxOut = (VxSurfaceRefinement_Test)0;
	vxOut.f3PosSurfaceWS = f3PosStartWS;
	vxOut.iHitStatus = -1;

	float4 f4ColorOTF = (float4)0;
	float3 f3PosStartTS = vxTransformPoint(f3PosStartWS, volObj.matWS2TS);
	float3 f3VecSampleDirWS = f3VecDirUnitWS * volObj.fSampleDistWS;
	float3 f3VecSampleDirTS = vxTransformVector(f3VecSampleDirWS, volObj.matWS2TS);
	int iSculptValue = (int)(volObj.iFlags >> 24);
	float fSampleValue = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosStartTS, 0).r;
	int iSampleValue = (int)(fSampleValue * volObj.fSampleValueRange + 0.5f);

	if (iSampleValue >= __iIsoValue)
	{
		vxOut.iHitStatus = 0;
		return vxOut;
	}
	const float __fMinIsoValue = ((float)__iMinIsoValue - 0.5f) / volObj.fSampleValueRange;
	const float __fMaxIsoValue = ((float)__iMaxIsoValue - 0.5f) / volObj.fSampleValueRange;
	float3 __f3VecGradientSampleX = volObj.f3VecGradientSampleX * __fSampleFactor;
	float3 __f3VecGradientSampleY = volObj.f3VecGradientSampleY * __fSampleFactor;
	float3 __f3VecGradientSampleZ = volObj.f3VecGradientSampleZ * __fSampleFactor;

#define INDEXSAMPLE(a) (int)(a * volObj.fSampleValueRange + 0.5f)

	float i = 1.f;
	float fNumSamples = (float)iNumSamples;

	float3 f3PosSampleWS = f3PosStartWS + f3VecSampleDirWS;
	float3 f3PosSampleTS = vxTransformPoint(f3PosSampleWS, volObj.matWS2TS);
	fSampleValue = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS, 0).r;

	[loop]
	while (i < fNumSamples)
	{
		VxBlockSkip blkSkip = vxComputeBlockRayLengthBoundary(f3PosSampleTS, f3VecSampleDirTS, blkObj, tex3DBlock);
		float fNumStepSkip = min(max(1, blkSkip.iNumStepSkip), fNumSamples - i);

		if (blkSkip.iSampleBlockValue > 0)
		{
			float k = 0;
			while (k < fNumStepSkip)
			{
				if (fSampleValue >= __fMinIsoValue)
				{
					// surface rho0 along v //
					float3 f3PosInnerSurfWS, f3PosOuterSurfWS, f3PosErrorSurfWS;
					// Surface refinement with Interval bisection //
					float3 f3PosBisectionOuterStartWS = f3PosSampleWS - f3VecSampleDirWS;
					float3 f3PosBisectionOuterEndWS = f3PosSampleWS;
#ifdef __SURFREFINE__
					for (int m = 0; m < SURFACEREFINEMENTNUM; m++)
					{
						float3 f3PosBisectionWS = (f3PosBisectionOuterStartWS + f3PosBisectionOuterEndWS) * 0.5f;
						float3 f3PosBisectionTS = vxTransformPoint(f3PosBisectionWS, volObj.matWS2TS);
						float fSampleValueBisection = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosBisectionTS, 0).r;
						if (fSampleValueBisection >= __fMinIsoValue)
						{
							fSampleValue = fSampleValueBisection;
							f3PosBisectionOuterEndWS = f3PosBisectionWS;
						}
						else
							f3PosBisectionOuterStartWS = f3PosBisectionWS;
					}
#endif
					f3PosOuterSurfWS = f3PosBisectionOuterEndWS;
					i -= length(f3PosOuterSurfWS - f3PosSampleWS);
					f3PosSampleWS = f3PosOuterSurfWS;

					const float __fSampleNumStep = 1.0f;
					int iTracingCount = 1;

					bool bErrorSurface = false;
					bool bIsCrossNewEmpty = false;
#ifdef __BLENDING_BASED_METHOD
					bool bPassOpaquenessOnBlending = false;
					const float __fOpaqueAlphaThres = __fCarvingDepth;// 0.95f;
					float fLastBeforeOpaqueAlpha = 0;
					float3 f3PosLastBeforeOpaqueWS;
#endif
					// finding rho1 along v //
					while (i < fNumSamples && !(bIsCrossNewEmpty = fSampleValue < __fMinIsoValue))
					{
						// the surface searching process //
						float fSampleValueXR = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + __f3VecGradientSampleX, 0).r;
						float fSampleValueXL = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - __f3VecGradientSampleX, 0).r;
						float fSampleValueYR = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + __f3VecGradientSampleY, 0).r;
						float fSampleValueYL = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - __f3VecGradientSampleY, 0).r;
						float fSampleValueZR = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + __f3VecGradientSampleZ, 0).r;
						float fSampleValueZL = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - __f3VecGradientSampleZ, 0).r;
						float fOpacityXR = f4bufWindowing[INDEXSAMPLE(fSampleValueXR)].w;
						float fOpacityXL = f4bufWindowing[INDEXSAMPLE(fSampleValueXL)].w;
						float fOpacityYR = f4bufWindowing[INDEXSAMPLE(fSampleValueYR)].w;
						float fOpacityYL = f4bufWindowing[INDEXSAMPLE(fSampleValueYL)].w;
						float fOpacityZR = f4bufWindowing[INDEXSAMPLE(fSampleValueZR)].w;
						float fOpacityZL = f4bufWindowing[INDEXSAMPLE(fSampleValueZL)].w;

						float3 f3VecGradient = float3(fOpacityXR - fOpacityXL, fOpacityYR - fOpacityYL, fOpacityZR - fOpacityZL) / 2.0f;
						//float3 f3VecGradient = float3(fSampleValueXR - fSampleValueXL, fSampleValueYR - fSampleValueYL, fSampleValueZR - fSampleValueZL) / 2.0f;
						float fGradientLength = length(f3VecGradient);

						if (fSampleValue >= __fMaxIsoValue) // break with best fit surface 
						{
							// Surface refinement with Interval bisection //
							float3 f3PosBisectionInnerStartWS = f3PosSampleWS - f3VecSampleDirWS * __fSampleNumStep;
							if (iTracingCount == 1)
								f3PosBisectionInnerStartWS = f3PosSampleWS - f3VecSampleDirWS;
							float3 f3PosBisectionInnerEndWS = f3PosSampleWS;
#ifdef __SURFREFINE__
							for (int m = 0; m < SURFACEREFINEMENTNUM; m++)
							{
								float3 f3PosBisectionWS = (f3PosBisectionInnerStartWS + f3PosBisectionInnerEndWS) * 0.5f;
								float3 f3PosBisectionTS = vxTransformPoint(f3PosBisectionWS, volObj.matWS2TS);
								float fSampleValueBisection = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosBisectionTS, 0).r;
								if (fSampleValueBisection >= __fMaxIsoValue)
								{
									f3PosBisectionInnerEndWS = f3PosBisectionWS;
									fSampleValue = fSampleValueBisection;
								}
								else
									f3PosBisectionInnerStartWS = f3PosBisectionWS;
							}
#endif
							f3PosInnerSurfWS = f3PosBisectionInnerEndWS;
							i -= length(f3PosInnerSurfWS - f3PosSampleWS);
							f3PosSampleWS = f3PosInnerSurfWS;
							break; // while (i < fNumSamples && !bIsErrorSurface)
						}
						else if (fGradientLength < __fThresGradientLength && !bErrorSurface) // break with depth correction surface for false surface case 
						{
							bErrorSurface = true;
							f3PosErrorSurfWS = f3PosSampleWS;
							//break; // while (i < fNumSamples && !bIsErrorSurface)
						}

#ifdef __BLENDING_BASED_METHOD
						if (!bPassOpaquenessOnBlending)
						{
							float fAlphaCorrection = iTracingCount == 1 ? 1.f : __fSampleNumStep;
							float fOpacity = f4bufWindowing[INDEXSAMPLE(fSampleValue)].w * fAlphaCorrection;
							float fAccumulatedAlpha = fLastBeforeOpaqueAlpha + fOpacity * (1.f - fLastBeforeOpaqueAlpha);
							if (fAccumulatedAlpha >= __fOpaqueAlphaThres)
							{
								bPassOpaquenessOnBlending = true;
#ifdef __ALPHA_SURFACE
								// refinement
								float3 f3PosBisectionAlphaStartWS = f3PosSampleWS - f3VecSampleDirWS * __fSampleNumStep;
								if (iTracingCount == 1)
									f3PosBisectionAlphaStartWS = f3PosSampleWS - f3VecSampleDirWS;
								float3 f3PosBisectionAlphaEndWS = f3PosSampleWS;
								//float fAccumulatedAlphaBisection = fLastBeforeOpaqueAlpha ;

								for (int m = 0; m < SURFACEREFINEMENTNUM; m++)
								{
									float3 f3PosBisectionWS = (f3PosBisectionAlphaStartWS + f3PosBisectionAlphaEndWS) * 0.5f;
									float3 f3PosBisectionTS = vxTransformPoint(f3PosBisectionWS, volObj.matWS2TS);
									float fSampleValueBisection = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosBisectionTS, 0).r;
									float fOpacityBisection = f4bufWindowing[INDEXSAMPLE(fSampleValueBisection)].w;

									float fLengthSample = length(f3PosBisectionWS - f3PosBisectionAlphaStartWS);
									float fAlphaCorrectionBisection = fOpacityBisection * fLengthSample / volObj.fSampleDistWS * fAlphaCorrection;
									float fAccumulatedAlphaBisection = fLastBeforeOpaqueAlpha + fAlphaCorrectionBisection * (1.f - fLastBeforeOpaqueAlpha);

									if (fAccumulatedAlphaBisection >= __fOpaqueAlphaThres)
									{
										f3PosBisectionAlphaEndWS = f3PosBisectionWS;
									}
									else
									{
										f3PosBisectionAlphaStartWS = f3PosBisectionWS;
										fLastBeforeOpaqueAlpha = fAccumulatedAlphaBisection;
									}
								}

								f3PosLastBeforeOpaqueWS = f3PosBisectionAlphaEndWS;
#endif
							}
							else
							{
								fLastBeforeOpaqueAlpha = fAccumulatedAlpha;
								f3PosLastBeforeOpaqueWS = f3PosSampleWS;
							}
						}
#endif

						// check gradient magnitude depth //

						f3PosSampleWS += f3VecSampleDirWS * __fSampleNumStep;
						i += __fSampleNumStep;

						f3PosSampleTS = vxTransformPoint(f3PosSampleWS, volObj.matWS2TS);
						fSampleValue = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS, 0).r;

						iTracingCount++;
					} // while (i < fNumSamples)

					// bErrorSurface bIsCrossNewEmpty f3PosInnerSurfWS f3PosOuterSurfWS
					if (!bIsCrossNewEmpty || bErrorSurface) // determine the desired surface and exit the main loop
					{
						if (bIsCrossNewEmpty) // f3PosOuterSurfWS //
						{
#ifdef __BLENDING_BASED_METHOD
							//float3 f3PosSurfaceTS = vxTransformPoint(f3PosOuterSurfWS, volObj.matWS2TS);
							float3 f3PosSurfaceTS = vxTransformPoint(f3PosLastBeforeOpaqueWS, volObj.matWS2TS);
							float3 f3VecGradientWS = vxSampleVolumeGradientSamplerTex3D(f3PosSurfaceTS,
								volObj.f3VecGradientSampleX, volObj.f3VecGradientSampleY, volObj.f3VecGradientSampleZ, tex3DVolume);

							vxOut.f3PosSurfaceWS = f3PosLastBeforeOpaqueWS; // refine with fLastBeforeOpaqueAlpha??
							vxOut.iOutType = 3;
							vxOut.f3VecNormalWS = -normalize(f3VecGradientWS);
#else
							float3 f3PosSurfaceTS = vxTransformPoint(f3PosOuterSurfWS, volObj.matWS2TS);
							float3 f3VecGradientWS = vxSampleVolumeGradientSamplerTex3D(f3PosSurfaceTS,
								volObj.f3VecGradientSampleX, volObj.f3VecGradientSampleY, volObj.f3VecGradientSampleZ, tex3DVolume);

							float fAngle = acos(dot(f3VecGradientWS, f3VecDirUnitWS));
							float fTolerenceLenth = __fCarvingDepth / (cos(fAngle) + 0.00001f);

							vxOut.f3PosSurfaceWS = f3PosOuterSurfWS + fTolerenceLenth * f3VecDirUnitWS;
							vxOut.iOutType = 3;
							vxOut.f3VecNormalWS = -normalize(f3VecGradientWS);
#endif
						}
						else // both f3PosInnerSurfWS and f3PosOuterSurfWS are available
						{
							if (!bErrorSurface) // f3PosInnerSurfWS
							{
#ifdef __BLENDING_BASED_METHOD
								vxOut.f3PosSurfaceWS = f3PosInnerSurfWS;
#else
								vxOut.f3PosSurfaceWS = f3PosInnerSurfWS;
#endif
								vxOut.iOutType = 7;

								float3 f3PosSurfaceTS = vxTransformPoint(vxOut.f3PosSurfaceWS, volObj.matWS2TS);
								float3 f3VecGradientWS = vxSampleVolumeGradientSamplerTex3D(f3PosSurfaceTS,
									volObj.f3VecGradientSampleX, volObj.f3VecGradientSampleY, volObj.f3VecGradientSampleZ, tex3DVolume);
								vxOut.f3VecNormalWS = -normalize(f3VecGradientWS);
							}
							else
							{
#ifdef __BLENDING_BASED_METHOD
								//float3 f3PosSurfaceTS = vxTransformPoint(f3PosOuterSurfWS, volObj.matWS2TS);
								float3 f3PosSurfaceTS = vxTransformPoint(f3PosLastBeforeOpaqueWS, volObj.matWS2TS);
								float3 f3VecGradientWS = vxSampleVolumeGradientSamplerTex3D(f3PosSurfaceTS,
									volObj.f3VecGradientSampleX, volObj.f3VecGradientSampleY, volObj.f3VecGradientSampleZ, tex3DVolume);

								vxOut.f3PosSurfaceWS = f3PosLastBeforeOpaqueWS; // refine with fLastBeforeOpaqueAlpha??
								vxOut.iOutType = 2;
								vxOut.f3VecNormalWS = -normalize(f3VecGradientWS);
#else
								float3 f3PosSurfaceTS = vxTransformPoint(f3PosOuterSurfWS, volObj.matWS2TS);
								float3 f3VecGradientWS = vxSampleVolumeGradientSamplerTex3D(f3PosSurfaceTS,
									volObj.f3VecGradientSampleX, volObj.f3VecGradientSampleY, volObj.f3VecGradientSampleZ, tex3DVolume);

								float fDisInBtw = length(f3PosOuterSurfWS - f3PosInnerSurfWS);

								float fAngle = acos(dot(f3VecGradientWS, f3VecDirUnitWS));
								float fTolerenceLenth = __fCarvingDepth / (cos(fAngle) + 0.00001f);

								if (fDisInBtw < fTolerenceLenth)
								{
									vxOut.f3PosSurfaceWS = f3PosInnerSurfWS;
									vxOut.iOutType = 1;

									f3PosSurfaceTS = vxTransformPoint(f3PosInnerSurfWS, volObj.matWS2TS);
									f3VecGradientWS = vxSampleVolumeGradientSamplerTex3D(f3PosSurfaceTS,
										volObj.f3VecGradientSampleX, volObj.f3VecGradientSampleY, volObj.f3VecGradientSampleZ, tex3DVolume);
									vxOut.f3VecNormalWS = -normalize(f3VecGradientWS);
								}
								else
								{
									vxOut.f3PosSurfaceWS = f3PosOuterSurfWS + fTolerenceLenth * f3VecDirUnitWS;// f3PosErrorSurfWS;
									vxOut.iOutType = 2;

									vxOut.f3VecNormalWS = -normalize(f3VecGradientWS);
								}
#endif
							} // else if (bErrorSurface)
						} // else if (!bIsCrossNewEmpty)

						vxOut.iHitStatus = 1;

						i = fNumSamples + 1.f; // in order to exit while (i < fNumSamples)
						k = fNumStepSkip + 1.f; // in order to exit while (k < fNumStepSkip)
						break;	// in order to exit while (i < fNumSamples && fSampleValue >= __fMinIsoValue)
					} // if (!bIsCrossNewEmpty || bErrorSurface)
				}
				else // if (fSampleValue >= __fMinIsoValue)
				{
					f3PosSampleWS += f3VecSampleDirWS;
					k += 1.f;
					i += 1.f;

					f3PosSampleTS = vxTransformPoint(f3PosSampleWS, volObj.matWS2TS);
					fSampleValue = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS, 0).r;
				}
			} // while (k < fNumStepSkip)
		} // if (blkSkip.iSampleBlockValue > 0)
		else
		{
			f3PosSampleWS += f3VecSampleDirWS * fNumStepSkip;
			i += fNumStepSkip;

			f3PosSampleTS = vxTransformPoint(f3PosSampleWS, volObj.matWS2TS);
			fSampleValue = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS, 0).r;
		}
	} // while (i < fNumSamples)

	return vxOut;
}

float4 vxComputeOpaqueSurfaceEffect_Test(float3 f3PosSurfaceWS, float3 f3VecNormalWS, float3 f3VecSampleWS, 
	VxSurfaceEffect surfEffect, int iMaxDensityValue, VxCameraState camState,
	VxVolumeObject volObj, VxOtf1D otf1D, Texture3D tex3DVolume, Buffer<float4> f4bufOTF)
{
	float3 f3PosSampleWS = f3PosSurfaceWS;
	float3 f3PosSampleTS = vxTransformPoint(f3PosSampleWS, volObj.matWS2TS);

	const float __fMinIsoValue = ((float)otf1D.iOtf1stValue - 0.5f) / volObj.fSampleValueRange;
	const float __fMaxIsoValue = ((float)iMaxDensityValue - 0.5f) / volObj.fSampleValueRange;

	float fSampleValue = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS, 0).r;
	int iSampleValue = max((int)(fSampleValue * volObj.fSampleValueRange + 0.5f), otf1D.iOtf1stValue);
	//if (fSampleValue >= __fMaxIsoValue)
	//	iSampleValue += 1000;

	float4 f4ColorOTF = vxLoadPointOtf1DFromBufferWithoutAC(iSampleValue, f4bufOTF, volObj.fOpaqueCorrection);

	float3 f3VecViewUnit = -normalize(f3VecSampleWS);
	float3 f3VecLightWS = -camState.f3VecLightWS;
	if (camState.iFlags & 0x2)
		f3VecLightWS = -normalize(f3PosSurfaceWS - camState.f3PosLightWS);

	//float3 f3VecGrad = vxSampleVolumeGradientSamplerTex3D(f3PosSampleTS,
	//	volObj.f3VecGradientSampleX, volObj.f3VecGradientSampleY, volObj.f3VecGradientSampleZ, tex3DVolume);

	//float3 f3VecGradNormal = -normalize(f3VecGrad);
	float3 f3VecGradNormal = f3VecNormalWS;

	float3 f3VecReflect = 2.f*f3VecGradNormal*dot(f3VecGradNormal, f3VecViewUnit) - f3VecViewUnit;
	float3 f3VecReflectNormal = normalize(f3VecReflect);

	float fDiffuse = 0;
	float fReflect = 0;
	float fOcclusionRatio = 1.f;

	if (surfEffect.iFlagsForIsoSurfaceRendering & 0x2)
	{
		// Anisotropic BRDF
		fDiffuse = vxComputeDiffuseStaticBRDF(f3VecViewUnit, f3VecLightWS, f3VecGradNormal
			, surfEffect.fPhongBRDF_DiffuseRatio, surfEffect.fPhongBRDF_ReflectanceRatio);
		fReflect = vxComputeSpecularStaticBRDF(f3VecViewUnit, f3VecLightWS, f3VecGradNormal, f3VecReflectNormal
			, surfEffect.fPhongExpWeightU, surfEffect.fPhongExpWeightV, surfEffect.fPhongBRDF_ReflectanceRatio);
	}
	else
	{
		// float fDot1 = dot(f3VecGradNormal, f3VecLightWS);
		float fLightDot = max(dot(f3VecGradNormal, f3VecLightWS), 0);
		fDiffuse = fLightDot;

		float fDot2 = dot(f3VecReflectNormal, f3VecLightWS);
		float fReflectDot = max(fDot2, 0);
		fReflect = pow(fReflectDot, volObj.f4ShadingFactor.w);
	}

	if (surfEffect.iFlagsForIsoSurfaceRendering & 0x1)
	{
		float fOcclusion = vxComputeOcclusion(f3VecGradNormal, f3PosSampleWS, surfEffect.iOcclusionNumSamplesPerKernelRay
			, surfEffect.fOcclusionSampleStepDistOfKernelRay, volObj.matWS2TS, volObj.fSampleValueRange, iMaxDensityValue, tex3DVolume);
		fOcclusionRatio = (NUM_SAMPLES * surfEffect.iOcclusionNumSamplesPerKernelRay - fOcclusion) / (float)(NUM_SAMPLES * surfEffect.iOcclusionNumSamplesPerKernelRay);
	}

	float3 f3OutColor = f4ColorOTF.rgb * (volObj.f4ShadingFactor.x + volObj.f4ShadingFactor.y * fDiffuse + volObj.f4ShadingFactor.z * fReflect) * fOcclusionRatio;

	return float4(f3OutColor, f4ColorOTF.a);
}

#ifdef SAMPLE_FROM_BRICKCHUNK
VxSurfaceRefinement vxVrSlabSurfaceRefinementWithBlockSkipping(float3 f3PosStartWS, float3 f3VecDirUnitWS, int iNumSamples,
	VxVolumeObject volObj, VxBrickChunk blkChunk, Texture3D tex3DChunks[3], VxBlockObject blkObj, Texture3D tex3DBlock, VxOtf1D otf1D)
#else
#ifdef OTF_MASK
VxSurfaceRefinement vxVrSlabSurfaceRefinementWithBlockSkipping(float3 f3PosStartWS, float3 f3VecDirUnitWS, int iNumSamples, 
	VxVolumeObject volObj, Texture3D tex3DVolume, VxBlockObject blkObj, Texture3D tex3DBlock, VxOtf1D otf1D, Buffer<float4> f4bufOTF, Buffer<int> f4bufOTFIndex, Texture3D tex3DVolumeMask)
#else
VxSurfaceRefinement vxVrSlabSurfaceRefinementWithBlockSkipping(float3 f3PosStartWS, float3 f3VecDirUnitWS, int iNumSamples,
	VxVolumeObject volObj, Texture3D tex3DVolume, VxBlockObject blkObj, Texture3D tex3DBlock, VxOtf1D otf1D)
#endif
#endif
{
	VxSurfaceRefinement vxOut = (VxSurfaceRefinement)0;
	vxOut.f3PosSurfaceWS = f3PosStartWS;
	vxOut.iHitSampleStep = -1;

	float4 f4ColorOTF = (float4)0;
	float3 f3PosStartTS = vxTransformPoint(f3PosStartWS, volObj.matWS2TS);
	float3 f3VecSampleDirWS = f3VecDirUnitWS * volObj.fSampleDistWS;
	float3 f3VecSampleDirTS = vxTransformVector(f3VecSampleDirWS, volObj.matWS2TS);

	float fSampleValue = 0;
	float fMinValidValue = (float)otf1D.iOtf1stValue / volObj.fSampleValueRange;
#ifdef SAMPLER_VOLUME_CCF
	fMinValidValue /= 8.f;
#endif

	fSampleValue = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosStartTS, 0).r;

	if (fSampleValue >= fMinValidValue)
	{
		vxOut.iHitSampleStep = 0;
		return vxOut;
	}

	[loop]
	for(int i = 1; i < iNumSamples; i++)
	{
		float3 f3PosSampleWS = f3PosStartWS + f3VecSampleDirWS*(float)i;
		float3 f3PosSampleTS = vxTransformPoint(f3PosSampleWS, volObj.matWS2TS);
		
		VxBlockSkip blkSkip = vxComputeBlockRayLengthBoundary(f3PosSampleTS, f3VecSampleDirTS, blkObj, tex3DBlock);
		blkSkip.iNumStepSkip = min(max(1, blkSkip.iNumStepSkip), iNumSamples - i);

		if(blkSkip.iSampleBlockValue > 0)
		{
			for(int k = 0; k < blkSkip.iNumStepSkip; k++, i++)
			{
				float3 f3PosSampleInBlockWS = f3PosStartWS + f3VecSampleDirWS*(float)i;
				float3 f3PosSampleInBlockTS = vxTransformPoint(f3PosSampleInBlockWS, volObj.matWS2TS);

#ifdef SAMPLER_VOLUME_CCF
				bool bIsPass = false;
				fSampleValue = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleInBlockTS, 0).r;
				if (fSampleValue >= fMinValidValue)
				{
					float2 f2SampleValue = vxSampleByCCF(f3PosSampleInBlockTS, volObj.f3SizeVolumeCVS, tex3DVolume);
					if (f2SampleValue.y >= 0.5f)
						bIsPass = true;
				}
				if (bIsPass)
#else
				fSampleValue = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleInBlockTS, 0).r;
#ifdef OTF_MASK
				float2 f2SampleMaskValue = vxSampleByCCF(f3PosSampleInBlockTS, volObj.f3SizeVolumeCVS, tex3DVolumeMask);
				int iMaskValue = (int)(f2SampleMaskValue.x * 255 + 0.5f);
				int iMaskID = f4bufOTFIndex[iMaskValue];
				int iSampleValue = (int)(fSampleValue * volObj.fSampleValueRange + 0.5f);
				float4 f4ColorOTF_Origin = vxLoadPointOtf1DFromBufferWithoutAC(iSampleValue, f4bufOTF, volObj.fOpaqueCorrection);
				float4 f4ColorOTF_Mask = vxLoadPointOtf1DSeriesFromBufferWithoutAC(iSampleValue, iMaskID, f4bufOTF, volObj.fOpaqueCorrection);
				float4 f4ColorOTF = f4ColorOTF_Origin * (1.f - f2SampleMaskValue.y) + f4ColorOTF_Mask * f2SampleMaskValue.y;
				if (f4ColorOTF.a > FLT_MIN)
#else
				if (fSampleValue >= fMinValidValue)
#endif
#endif
				{
					// Surface refinement //
					// Interval bisection
					float3 f3PosBisectionStartWS = f3PosSampleInBlockWS - f3VecSampleDirWS;
					float3 f3PosBisectionEndWS = f3PosSampleInBlockWS;

					for(int j = 0; j < SURFACEREFINEMENTNUM; j++)
					{
						float3 f3PosBisectionWS = (f3PosBisectionStartWS + f3PosBisectionEndWS)*0.5f;
						float3 f3PosBisectionTS = vxTransformPoint(f3PosBisectionWS, volObj.matWS2TS);
#ifdef SAMPLER_VOLUME_CCF
						bool bIsPass_Refine = false;
						fSampleValue = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosBisectionTS, 0).r;
						if (fSampleValue >= fMinValidValue)
						{
							float2 f2SampleValue = vxSampleByCCF(f3PosBisectionTS, volObj.f3SizeVolumeCVS, tex3DVolume);
							if (f2SampleValue.y >= 0.5f)
								bIsPass_Refine = true;
						}
						if (bIsPass_Refine)
#else
						fSampleValue = tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosBisectionTS, 0).r;

#ifdef OTF_MASK
						f2SampleMaskValue = vxSampleByCCF(f3PosBisectionTS, volObj.f3SizeVolumeCVS, tex3DVolumeMask);
						int iMaskValue = (int)(f2SampleMaskValue.x * 255 + 0.5f);
						int iMaskID = f4bufOTFIndex[iMaskValue];
						iSampleValue = (int)(fSampleValue * volObj.fSampleValueRange + 0.5f);
						f4ColorOTF_Origin = vxLoadPointOtf1DFromBufferWithoutAC(iSampleValue, f4bufOTF, volObj.fOpaqueCorrection);
						f4ColorOTF_Mask = vxLoadPointOtf1DSeriesFromBufferWithoutAC(iSampleValue, iMaskID, f4bufOTF, volObj.fOpaqueCorrection);
						f4ColorOTF = f4ColorOTF_Origin * (1.f - f2SampleMaskValue.y) + f4ColorOTF_Mask * f2SampleMaskValue.y;
						if (f4ColorOTF.a > FLT_MIN)
#else
						if (fSampleValue >= fMinValidValue)
#endif
#endif
						{
							f3PosBisectionEndWS = f3PosBisectionWS;
						}
						else
						{
							f3PosBisectionStartWS = f3PosBisectionWS;
						}
					}
					vxOut.f3PosSurfaceWS = f3PosBisectionEndWS;
					vxOut.iHitSampleStep = i;
					i = iNumSamples + 1;
					k = blkSkip.iNumStepSkip + 1;
					break;
				}	// if(iSampleValueNext >= otf1D.iOtf1stValue)
			} // for(int k = 0; k < blkSkip.iNumStepSkip; k++, i++)
		}
		else
		{
			i += blkSkip.iNumStepSkip;
		}
		// this is for outer loop's i++
		i -= 1;
	}
		
	return vxOut;
}

VxSurfaceRefinement vxVrBlockPointSurfaceWithBlockSkipping(float3 f3PosStartWS, float3 f3VecDirUnitWS, int iNumSamples, 
	VxVolumeObject volObj, VxBlockObject blkObj, Texture3D tex3DBlock)
{
	VxSurfaceRefinement vxOut;

	vxOut.f3PosSurfaceWS = f3PosStartWS;
	vxOut.iHitSampleStep = -1;
	
	float3 f3VecSampleDirWS = f3VecDirUnitWS * volObj.fSampleDistWS;
	float3 f3VecSampleDirTS = vxTransformVector( f3VecSampleDirWS, volObj.matWS2TS);

	[loop]
	for(int i = 0; i < iNumSamples; i++)
	{
		float3 f3PosSampleWS = f3PosStartWS + f3VecSampleDirWS*(float)i;
		float3 f3PosSampleTS = vxTransformPoint(f3PosSampleWS, volObj.matWS2TS);
		
		VxBlockSkip blkSkip = vxComputeBlockRayLengthBoundary(f3PosSampleTS, f3VecSampleDirTS, blkObj, tex3DBlock);
		blkSkip.iNumStepSkip = max(1, blkSkip.iNumStepSkip);

		if(blkSkip.iSampleBlockValue > 0)
		{
			vxOut.f3PosSurfaceWS = f3PosSampleWS;
			vxOut.iHitSampleStep = i;
			i = iNumSamples + 1;
			break;
		}
		else
		{
			i += blkSkip.iNumStepSkip;
		}
		// this is for outer loop's i++
		i -= 1;
	}
		
	return vxOut;
}

#ifdef SAMPLE_FROM_BRICKCHUNK
float4 vxComputeOpaqueSurfaceEffect(float3 f3PosSurfaceWS, float3 f3VecSampleWS, VxSurfaceEffect surfEffect, VxCameraState camState, 
	VxVolumeObject volObj, VxBrickChunk blkChunk, Texture3D tex3DChunks[3], VxBlockObject blkObj, Texture3D tex3DBlock, VxOtf1D otf1D, Buffer<float4> f4bufOTF)
#else
#ifdef OTF_MASK
float4 vxComputeOpaqueSurfaceEffect(float3 f3PosSurfaceWS, float3 f3VecSampleWS, VxSurfaceEffect surfEffect, VxCameraState camState,
	VxVolumeObject volObj, VxOtf1D otf1D, Texture3D tex3DVolume, Buffer<float4> f4bufOTF, Buffer<int> f4bufOTFIndex, Texture3D tex3DVolumeMask)
#else
float4 vxComputeOpaqueSurfaceEffect(float3 f3PosSurfaceWS, float3 f3VecSampleWS, VxSurfaceEffect surfEffect, VxCameraState camState, 
	VxVolumeObject volObj, VxOtf1D otf1D, Texture3D tex3DVolume, Buffer<float4> f4bufOTF)
#endif
#endif
{
	float3 f3PosSampleWS = f3PosSurfaceWS;
	float3 f3PosSampleTS = vxTransformPoint(f3PosSampleWS, volObj.matWS2TS);
	
#ifdef SAMPLE_FROM_BRICKCHUNK
	int3 i3SampleBlockID = int3(f3PosSampleTS.x / blkObj.f3SizeBlockTS.x, f3PosSampleTS.y / blkObj.f3SizeBlockTS.y, f3PosSampleTS.z / blkObj.f3SizeBlockTS.z);
	int iSampleBlockValue = (int)(tex3DBlock.Load(int4(i3SampleBlockID, 0)).r * blkObj.fSampleValueRange + 0.5f);
	float3 f3PosSampleCTS = vxComputeBrickSamplePosInChunk(f3PosSampleTS, iSampleBlockValue - 1, blkObj, blkChunk);
#ifdef SAMPLER_VOLUME_CCF
	int iSampleValue = (int)(vxSampleInBrickChunkByCCFWithBlockValue(f3PosSampleCTS, iSampleBlockValue - 1, volObj, blkChunk, tex3DChunks).x * volObj.fSampleValueRange + 0.5);
#else
	int iSampleValue = max((int)(vxSampleInBrickChunkWithBlockValue(f3PosSampleCTS, iSampleBlockValue - 1, blkChunk, tex3DChunks) * volObj.fSampleValueRange + 0.5f), otf1D.iOtf1stValue);
#endif
#else
#ifdef SAMPLER_VOLUME_CCF
	int iSampleValue = (int)(vxSampleByCCF(f3PosSampleTS, volObj.f3SizeVolumeCVS, tex3DVolume).x * volObj.fSampleValueRange + 0.5f);
#else
	int iSampleValue = max((int)(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS, 0).r * volObj.fSampleValueRange + 0.5f), otf1D.iOtf1stValue);
#endif
#endif

#ifdef OTF_MASK
	float2 f2SampleMaskValue = vxSampleByCCF(f3PosSampleTS, volObj.f3SizeVolumeCVS, tex3DVolumeMask);
	int iMaskValue = (int)(f2SampleMaskValue.x * 255 + 0.5f);
	int iMaskID = f4bufOTFIndex[iMaskValue];
	//float4 f4ColorOTF_Origin = vxLoadPointOtf1DFromBufferWithoutAC(iSampleValue, f4bufOTF, volObj.fOpaqueCorrection);
	float4 f4ColorOTF_Mask = vxLoadPointOtf1DSeriesFromBufferWithoutAC(iSampleValue, iMaskID, f4bufOTF, volObj.fOpaqueCorrection);
	float4 f4ColorOTF = f4ColorOTF_Mask;// f4ColorOTF_Origin * (1.f - f2SampleMaskValue.y) + f4ColorOTF_Mask * f2SampleMaskValue.y;
#else
	float4 f4ColorOTF = vxLoadPointOtf1DFromBufferWithoutAC(iSampleValue, f4bufOTF, volObj.fOpaqueCorrection);
#endif


#ifdef SAMPLE_FROM_BRICKCHUNK
#ifdef SAMPLER_VOLUME_CCF
	float3 f3VecGrad = vxSampleVolumeGradientSamplerTex3DFromBrickChunksByCCF(f3PosSampleCTS, volObj.f3SizeVolumeCVS, 
		volObj.f3VecGradientSampleX, volObj.f3VecGradientSampleY, volObj.f3VecGradientSampleZ, 
		iSampleBlockValue - 1, blkChunk.iNumBricksInChunkXY, tex3DChunks);
#else
	float3 f3VecGrad = vxSampleVolumeGradientSamplerTex3DFromBrickChunks(f3PosSampleCTS, 
		volObj.f3VecGradientSampleX, volObj.f3VecGradientSampleY, volObj.f3VecGradientSampleZ, 
		iSampleBlockValue - 1, blkChunk.iNumBricksInChunkXY, tex3DChunks);
#endif
#else
#ifdef SAMPLER_VOLUME_CCF
	float3 f3VecGrad = vxSampleVolumeGradientSamplerTex3DByCCF(f3PosSampleTS, volObj.f3SizeVolumeCVS, 
		volObj.f3VecGradientSampleX, volObj.f3VecGradientSampleY, volObj.f3VecGradientSampleZ, tex3DVolume);
#else
	float3 f3VecGrad = vxSampleVolumeGradientSamplerTex3D(f3PosSampleTS, 
		volObj.f3VecGradientSampleX, volObj.f3VecGradientSampleY, volObj.f3VecGradientSampleZ, tex3DVolume);
#endif
#endif
	
	float fGradLength = length(f3VecGrad);
	float3 f3VecViewUnit = -normalize(f3VecSampleWS);
	
	float3 f3VecLightWS = -camState.f3VecLightWS;
	if(camState.iFlags & 0x2)
		f3VecLightWS = -normalize(f3PosSurfaceWS - camState.f3PosLightWS);


	float3 f3VecGradNormal = -f3VecGrad / fGradLength;
	float3 f3VecReflect = 2.f*f3VecGradNormal*dot(f3VecGradNormal, f3VecViewUnit) - f3VecViewUnit;
	float3 f3VecReflectNormal = normalize(f3VecReflect);
	
	float fDiffuse = 0;
	float fReflect = 0;
	float fOcclusionRatio = 1.f;

	if( surfEffect.iFlagsForIsoSurfaceRendering & 0x2 )
	{
		// Anisotropic BRDF
		fDiffuse = vxComputeDiffuseStaticBRDF(f3VecViewUnit, f3VecLightWS, f3VecGradNormal
			, surfEffect.fPhongBRDF_DiffuseRatio, surfEffect.fPhongBRDF_ReflectanceRatio);
		fReflect = vxComputeSpecularStaticBRDF(f3VecViewUnit, f3VecLightWS, f3VecGradNormal, f3VecReflectNormal
			, surfEffect.fPhongExpWeightU, surfEffect.fPhongExpWeightV, surfEffect.fPhongBRDF_ReflectanceRatio);
	}
	else 
	{
		//float fDot1 = dot(f3VecGradNormal, f3VecLightWS);
		float fLightDot = max(dot(f3VecGradNormal, f3VecLightWS), 0);
		fDiffuse = fLightDot;
		
		float fDot2 = dot(f3VecReflectNormal, f3VecLightWS);
		float fReflectDot = max(fDot2, 0);
		fReflect = pow(fReflectDot, volObj.f4ShadingFactor.w);
	}

	if((surfEffect.iFlagsForIsoSurfaceRendering & 0x1) && fGradLength > 0)
	{
#ifdef SAMPLE_FROM_BRICKCHUNK
		float fOcclusion = vxComputeOcclusion(f3VecGradNormal, f3PosSampleWS, surfEffect.iOcclusionNumSamplesPerKernelRay
			, surfEffect.fOcclusionSampleStepDistOfKernelRay, volObj.matWS2TS, volObj.fSampleValueRange, otf1D.iOtf1stValue, 
			blkObj, tex3DBlock, blkChunk, tex3DChunks);
#else
		float fOcclusion = vxComputeOcclusion(f3VecGradNormal, f3PosSampleWS, surfEffect.iOcclusionNumSamplesPerKernelRay
			, surfEffect.fOcclusionSampleStepDistOfKernelRay, volObj.matWS2TS, volObj.fSampleValueRange, otf1D.iOtf1stValue, tex3DVolume);
#endif
		fOcclusionRatio = (NUM_SAMPLES * surfEffect.iOcclusionNumSamplesPerKernelRay - fOcclusion) / (float)(NUM_SAMPLES * surfEffect.iOcclusionNumSamplesPerKernelRay);
	}
	
	float3 f3OutColor = f4ColorOTF.rgb * (volObj.f4ShadingFactor.x + volObj.f4ShadingFactor.y * fDiffuse + volObj.f4ShadingFactor.z * fReflect) * fOcclusionRatio;

	return float4(f3OutColor, f4ColorOTF.a);
	//return f4ColorOTF;
}


float4 vxComputeOpaqueSurfaceCurvature_TEST(float3 f3PosSurfaceWS, float3 f3VecSampleWS, VxSurfaceEffect surfEffect, VxCameraState camState,
	VxVolumeObject volObj, VxOtf1D otf1D, Texture3D tex3DVolume, Buffer<float4> f4bufOTF)
{
	float3 f3PosSampleWS = f3PosSurfaceWS;
	float3 f3PosSampleTS = vxTransformPoint(f3PosSampleWS, volObj.matWS2TS);

	int iSampleFactor = surfEffect.iSizeCurvatureKernel;

	int iSampleValue = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS, 0).r * volObj.fSampleValueRange);

	float4 f4OtfColor = vxLoadPointOtf1DFromBufferWithoutAC(iSampleValue, f4bufOTF, volObj.fOpaqueCorrection);
		
	//if (volObj.f4ShadingFactor.x == 1.f)
	//	return f4OtfColor;

	int iSampleValueXR = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + volObj.f3VecGradientSampleX * iSampleFactor, 0).r * volObj.fSampleValueRange);
	int iSampleValueXL = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - volObj.f3VecGradientSampleX * iSampleFactor, 0).r * volObj.fSampleValueRange);
	int iSampleValueYR = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + volObj.f3VecGradientSampleY * iSampleFactor, 0).r * volObj.fSampleValueRange);
	int iSampleValueYL = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - volObj.f3VecGradientSampleY * iSampleFactor, 0).r * volObj.fSampleValueRange);
	int iSampleValueZR = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + volObj.f3VecGradientSampleZ * iSampleFactor, 0).r * volObj.fSampleValueRange);
	int iSampleValueZL = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - volObj.f3VecGradientSampleZ * iSampleFactor, 0).r * volObj.fSampleValueRange);

	int iSampleValueXRYR = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + volObj.f3VecGradientSampleX * iSampleFactor + volObj.f3VecGradientSampleY * iSampleFactor, 0).r * volObj.fSampleValueRange);
	int iSampleValueXRYL = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + volObj.f3VecGradientSampleX * iSampleFactor - volObj.f3VecGradientSampleY * iSampleFactor, 0).r * volObj.fSampleValueRange);
	int iSampleValueXLYR = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - volObj.f3VecGradientSampleX * iSampleFactor + volObj.f3VecGradientSampleY * iSampleFactor, 0).r * volObj.fSampleValueRange);
	int iSampleValueXLYL = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - volObj.f3VecGradientSampleX * iSampleFactor - volObj.f3VecGradientSampleY * iSampleFactor, 0).r * volObj.fSampleValueRange);

	int iSampleValueYRZR = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + volObj.f3VecGradientSampleY * iSampleFactor + volObj.f3VecGradientSampleZ * iSampleFactor, 0).r * volObj.fSampleValueRange);
	int iSampleValueYRZL = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + volObj.f3VecGradientSampleY * iSampleFactor - volObj.f3VecGradientSampleZ * iSampleFactor, 0).r * volObj.fSampleValueRange);
	int iSampleValueYLZR = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - volObj.f3VecGradientSampleY * iSampleFactor + volObj.f3VecGradientSampleZ * iSampleFactor, 0).r * volObj.fSampleValueRange);
	int iSampleValueYLZL = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - volObj.f3VecGradientSampleY * iSampleFactor - volObj.f3VecGradientSampleZ * iSampleFactor, 0).r * volObj.fSampleValueRange);

	int iSampleValueXRZR = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + volObj.f3VecGradientSampleX * iSampleFactor + volObj.f3VecGradientSampleZ * iSampleFactor, 0).r * volObj.fSampleValueRange);
	int iSampleValueXRZL = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + volObj.f3VecGradientSampleX * iSampleFactor - volObj.f3VecGradientSampleZ * iSampleFactor, 0).r * volObj.fSampleValueRange);
	int iSampleValueXLZR = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - volObj.f3VecGradientSampleX * iSampleFactor + volObj.f3VecGradientSampleZ * iSampleFactor, 0).r * volObj.fSampleValueRange);
	int iSampleValueXLZL = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - volObj.f3VecGradientSampleX * iSampleFactor - volObj.f3VecGradientSampleZ * iSampleFactor, 0).r * volObj.fSampleValueRange);

	float3 f3VecGrad = float3(iSampleValueXR - iSampleValueXL, iSampleValueYR - iSampleValueYL, iSampleValueZR - iSampleValueZL)/ 2.0f;

	float4x4 H = 0;
	H._11 = float(iSampleValueXR - 2 * iSampleValue + iSampleValueXL);								//f_xx
	H._12 = float(iSampleValueXRYR - iSampleValueXLYR - iSampleValueXRYL + iSampleValueXLYL) / 4;	//f_xy
	H._13 = float(iSampleValueXRZR - iSampleValueXLZR - iSampleValueXRZL + iSampleValueXLZL) / 4;	//f_xz
	H._21 = H._12;
	H._22 = float(iSampleValueYR - 2 * iSampleValue + iSampleValueYL);								//f_yy
	H._23 = float(iSampleValueYRZR - iSampleValueYLZR - iSampleValueYRZL + iSampleValueYLZL) / 4;	//f_yz
	H._31 = H._13;
	H._32 = H._23;
	H._33 = float(iSampleValueZR - 2 * iSampleValue + iSampleValueZL);								//f_zz
	H._44 = 1;

	float fGradLength = length(f3VecGrad) + 0.0001f;
	float3 n = -f3VecGrad / fGradLength;
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
	
	float4 f4OutColor;

#ifndef FLOWCURVATURE
	float4x4 G = mul(mul(-P, H), P) / fGradLength;
	float T = G._11 + G._22 + G._33; // trace of G
	float F = sqrt(G._11 * G._11 + G._12 * G._12 + G._13 * G._13
		+ G._21 * G._21 + G._22 * G._22 + G._23 * G._23
		+ G._31 * G._31 + G._32 * G._32 + G._33 * G._33);

	//float3x3 GGT = G * transpose(G);

	//float F = sqrt(GGT._11 + GGT._22 + GGT._33);

/*	float F = 0.0f;	// Frobenius norm of G

	for (int j = 0; j < 3; j++)
		for (int i = 0; i < 3; i++)
			F += G[j][i] * G[j][i];

	F = sqrt(F);*/

	float k1 = (T + sqrt(2 * F*F - T*T)) / 2;
	float k2 = (T - sqrt(2 * F*F - T*T)) / 2;

	float r = sqrt(k1*k1 + k2*k2);
	const float PI = 3.1415926535f;
	float theta = PI / 4 - atan2(k2, k1);
	
	float3 f3ColorCurvature = float3(max(-r*cos(theta), 0.0f), max(r*sin(theta), 0.0f), max(r*cos(theta), 0.0f));
	r = min(r, 1.f);
			
	f4OutColor.x = f3ColorCurvature.x * (r)+f4OtfColor.x * (1.f - r);
	f4OutColor.y = f3ColorCurvature.y * (r)+f4OtfColor.y * (1.f - r);
	f4OutColor.z = f3ColorCurvature.z * (r)+f4OtfColor.z * (1.f - r);
#else
	float4x4 FF = mul((mul(-P, H) / fGradLength), nnT);
	float FLOWCURV = sqrt(FF._11 * FF._11 + FF._12 * FF._12 + FF._13 * FF._13
		+ FF._21 * FF._21 + FF._22 * FF._22 + FF._23 * FF._23
		+ FF._31 * FF._31 + FF._32 * FF._32 + FF._33 * FF._33);
		
	float3 f3ColorCurvature = float3(1, 1, 1);//vxLoadPointOtf1DFromBufferWithoutAC(FLOWCURV * volObj.fSampleValueRange *0.5 , f4bufOTF, volObj.fOpaqueCorrection).rgb;
	if(FLOWCURV > 0.3)
		f3ColorCurvature = float3(0, 0, 1);//
	f4OutColor.rgb = f3ColorCurvature;//f4OtfColor.rgb = float3(1, 1, 1);
#endif
	f4OutColor.w = SAFE_OPAQUEALPHA;

	float3 f3VecLightWS = camState.f3VecLightWS;
	float fLightDot = max(dot(-n, f3VecLightWS) , 0);

	f4OutColor.x *= 0.5f + 0.5f * fLightDot;
	f4OutColor.y *= 0.5f + 0.5f * fLightDot;
	f4OutColor.z *= 0.5f + 0.5f * fLightDot;

	return f4OutColor;
}

#ifdef OTF_MASK
float4 vxComputeOpaqueSurfaceCurvature(float3 f3PosSurfaceWS, float3 f3VecSampleWS, VxSurfaceEffect surfEffect, VxCameraState camState,
	VxVolumeObject volObj, VxOtf1D otf1D, Texture3D tex3DVolume, Buffer<float4> f4bufOTF, Buffer<int> f4bufOTFIndex, Texture3D tex3DVolumeMask)
#else
float4 vxComputeOpaqueSurfaceCurvature(float3 f3PosSurfaceWS, float3 f3VecSampleWS, VxSurfaceEffect surfEffect, VxCameraState camState,
	VxVolumeObject volObj, VxOtf1D otf1D, Texture3D tex3DVolume, Buffer<float4> f4bufOTF)
#endif
{

	float3 f3PosSampleWS = f3PosSurfaceWS;
	float3 f3PosSampleTS = vxTransformPoint(f3PosSampleWS, volObj.matWS2TS);

	int iSampleFactor = surfEffect.iSizeCurvatureKernel;

	int iSampleValue = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS, 0).r * volObj.fSampleValueRange);

#ifdef OTF_MASK
	float2 f2SampleMaskValue = vxSampleByCCF(f3PosSampleTS, volObj.f3SizeVolumeCVS, tex3DVolumeMask);
	int iMaskValue = (int)(f2SampleMaskValue.x * 255 + 0.5f);
	int iMaskID = f4bufOTFIndex[iMaskValue];
	//float4 f4ColorOTF_Origin = vxLoadPointOtf1DFromBufferWithoutAC(iSampleValue, f4bufOTF, volObj.fOpaqueCorrection);
	float4 f4ColorOTF_Mask = vxLoadPointOtf1DSeriesFromBufferWithoutAC(iSampleValue, iMaskID, f4bufOTF, volObj.fOpaqueCorrection);
	float4 f4OtfColor = f4ColorOTF_Mask;//f4ColorOTF_Origin * (1.f - f2SampleMaskValue.y) + f4ColorOTF_Mask * f2SampleMaskValue.y;
#else
	float4 f4OtfColor = vxLoadPointOtf1DFromBufferWithoutAC(iSampleValue, f4bufOTF, volObj.fOpaqueCorrection);
#endif
		
	if (volObj.f4ShadingFactor.x == 1.f)
		return f4OtfColor;

	int iSampleValueXR = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + volObj.f3VecGradientSampleX * iSampleFactor, 0).r * volObj.fSampleValueRange);
	int iSampleValueXL = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - volObj.f3VecGradientSampleX * iSampleFactor, 0).r * volObj.fSampleValueRange);
	int iSampleValueYR = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + volObj.f3VecGradientSampleY * iSampleFactor, 0).r * volObj.fSampleValueRange);
	int iSampleValueYL = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - volObj.f3VecGradientSampleY * iSampleFactor, 0).r * volObj.fSampleValueRange);
	int iSampleValueZR = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + volObj.f3VecGradientSampleZ * iSampleFactor, 0).r * volObj.fSampleValueRange);
	int iSampleValueZL = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - volObj.f3VecGradientSampleZ * iSampleFactor, 0).r * volObj.fSampleValueRange);

	int iSampleValueXRYR = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + volObj.f3VecGradientSampleX * iSampleFactor + volObj.f3VecGradientSampleY * iSampleFactor, 0).r * volObj.fSampleValueRange);
	int iSampleValueXRYL = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + volObj.f3VecGradientSampleX * iSampleFactor - volObj.f3VecGradientSampleY * iSampleFactor, 0).r * volObj.fSampleValueRange);
	int iSampleValueXLYR = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - volObj.f3VecGradientSampleX * iSampleFactor + volObj.f3VecGradientSampleY * iSampleFactor, 0).r * volObj.fSampleValueRange);
	int iSampleValueXLYL = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - volObj.f3VecGradientSampleX * iSampleFactor - volObj.f3VecGradientSampleY * iSampleFactor, 0).r * volObj.fSampleValueRange);

	int iSampleValueYRZR = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + volObj.f3VecGradientSampleY * iSampleFactor + volObj.f3VecGradientSampleZ * iSampleFactor, 0).r * volObj.fSampleValueRange);
	int iSampleValueYRZL = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + volObj.f3VecGradientSampleY * iSampleFactor - volObj.f3VecGradientSampleZ * iSampleFactor, 0).r * volObj.fSampleValueRange);
	int iSampleValueYLZR = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - volObj.f3VecGradientSampleY * iSampleFactor + volObj.f3VecGradientSampleZ * iSampleFactor, 0).r * volObj.fSampleValueRange);
	int iSampleValueYLZL = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - volObj.f3VecGradientSampleY * iSampleFactor - volObj.f3VecGradientSampleZ * iSampleFactor, 0).r * volObj.fSampleValueRange);

	int iSampleValueXRZR = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + volObj.f3VecGradientSampleX * iSampleFactor + volObj.f3VecGradientSampleZ * iSampleFactor, 0).r * volObj.fSampleValueRange);
	int iSampleValueXRZL = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS + volObj.f3VecGradientSampleX * iSampleFactor - volObj.f3VecGradientSampleZ * iSampleFactor, 0).r * volObj.fSampleValueRange);
	int iSampleValueXLZR = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - volObj.f3VecGradientSampleX * iSampleFactor + volObj.f3VecGradientSampleZ * iSampleFactor, 0).r * volObj.fSampleValueRange);
	int iSampleValueXLZL = int(tex3DVolume.SampleLevel(g_samplerLinear_Clamp, f3PosSampleTS - volObj.f3VecGradientSampleX * iSampleFactor - volObj.f3VecGradientSampleZ * iSampleFactor, 0).r * volObj.fSampleValueRange);

	float3 f3VecGrad = float3(iSampleValueXR - iSampleValueXL, iSampleValueYR - iSampleValueYL, iSampleValueZR - iSampleValueZL)/ 2.0f;

	float4x4 H = 0;
	H._11 = float(iSampleValueXR - 2 * iSampleValue + iSampleValueXL);								//f_xx
	H._12 = float(iSampleValueXRYR - iSampleValueXLYR - iSampleValueXRYL + iSampleValueXLYL) / 4;	//f_xy
	H._13 = float(iSampleValueXRZR - iSampleValueXLZR - iSampleValueXRZL + iSampleValueXLZL) / 4;	//f_xz
	H._21 = H._12;
	H._22 = float(iSampleValueYR - 2 * iSampleValue + iSampleValueYL);								//f_yy
	H._23 = float(iSampleValueYRZR - iSampleValueYLZR - iSampleValueYRZL + iSampleValueYLZL) / 4;	//f_yz
	H._31 = H._13;
	H._32 = H._23;
	H._33 = float(iSampleValueZR - 2 * iSampleValue + iSampleValueZL);								//f_zz
	H._44 = 1;

	float fGradLength = length(f3VecGrad) + 0.0001f;
	float3 n = -f3VecGrad / fGradLength;
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

	float4x4 G = mul(mul(-P, H), P) / fGradLength;
	float T = G._11 + G._22 + G._33; // trace of G
	float F = sqrt(G._11 * G._11 + G._12 * G._12 + G._13 * G._13
		+ G._21 * G._21 + G._22 * G._22 + G._23 * G._23
		+ G._31 * G._31 + G._32 * G._32 + G._33 * G._33);

	//float3x3 GGT = G * transpose(G);

	//float F = sqrt(GGT._11 + GGT._22 + GGT._33);

/*	float F = 0.0f;	// Frobenius norm of G

	for (int j = 0; j < 3; j++)
		for (int i = 0; i < 3; i++)
			F += G[j][i] * G[j][i];

	F = sqrt(F);*/

	float k1 = (T + sqrt(2 * F*F - T*T)) / 2;
	float k2 = (T - sqrt(2 * F*F - T*T)) / 2;

	float r = sqrt(k1*k1 + k2*k2);
	const float PI = 3.1415926535f;
	float theta = PI / 4 - atan2(k2, k1);


	int iModeCurvatureDescription = 0; // 0 : Normal Curvature Map (2D), 1 : Apply Concaveness
	if (surfEffect.iFlagsForIsoSurfaceRendering & 0x10)
		iModeCurvatureDescription = 1;
	bool bIsConcavenessDirection = false;
	if (surfEffect.iFlagsForIsoSurfaceRendering & 0x20)
		bIsConcavenessDirection = true;
	//surfEffect.iSizeCurvatureKernel
	//surfEffect.fThetaCurvatureTable
	//surfEffect.fRadiusCurvatureTable
	// ==> float3 Out //
	//
	float4 f4OutColor;

	switch (iModeCurvatureDescription)
	{
	case 0:	// normal curvature map
	{
			float3 f3ColorCurvature = float3(max(-r*cos(theta), 0.0f), max(r*sin(theta), 0.0f), max(r*cos(theta), 0.0f));
			r = min(r, 1.f);
			
			f4OutColor.x = f3ColorCurvature.x * (r)+f4OtfColor.x * (1.f - r);
			f4OutColor.y = f3ColorCurvature.y * (r)+f4OtfColor.y * (1.f - r);
			f4OutColor.z = f3ColorCurvature.z * (r)+f4OtfColor.z * (1.f - r);
			f4OutColor.w = SAFE_OPAQUEALPHA;

			break;
	}
	case 1: // concaveness
	{
			float fConcaveness = 0.0f;

			r = max(0.0f, r - surfEffect.fRadiusCurvatureTable);
			if (bIsConcavenessDirection)
				theta = max(0.0f, theta - surfEffect.fThetaCurvatureTable) / max(PI - surfEffect.fThetaCurvatureTable, 0.0001f);
			else
				theta = max(0.0f, surfEffect.fThetaCurvatureTable - theta) / max(surfEffect.fThetaCurvatureTable, 0.0001f);

			fConcaveness = min(pow(theta, 2) * pow(r, 2), 1);

			if (fConcaveness > 0)
			{
				float3 f3ColorError = float3(1.f, 0, 0);
				f4OutColor.x = f3ColorError.x * (fConcaveness)+f4OtfColor.x * (1.f - fConcaveness);
				f4OutColor.y = f3ColorError.y * (fConcaveness)+f4OtfColor.y * (1.f - fConcaveness);
				f4OutColor.z = f3ColorError.z * (fConcaveness)+f4OtfColor.z * (1.f - fConcaveness);
				f4OutColor.w = SAFE_OPAQUEALPHA;
			}
			else
			{
				f4OutColor.w = SAFE_OPAQUEALPHA;
			}

			break;
	}
	}

	float3 f3VecLightWS = camState.f3VecLightWS;
	float fLightDot = max(dot(-n, f3VecLightWS) , 0);

	f4OutColor.x *= 0.5f + 0.5f * fLightDot;
	f4OutColor.y *= 0.5f + 0.5f * fLightDot;
	f4OutColor.z *= 0.5f + 0.5f * fLightDot;


/*

	float3 f3VecViewUnit = -normalize(f3VecSampleWS);
	float3 f3VecLightWS = -camState.f3VecLightWS;
	if (camState.iFlags & 0x2)
		f3VecLightWS = -normalize(f3PosSurfaceWS - camState.f3PosLightWS);

	float3 f3VecGradNormal = -f3VecGrad / fGradLength;
	float3 f3VecReflect = 2.f*f3VecGradNormal*dot(f3VecGradNormal, f3VecViewUnit) - f3VecViewUnit;
	float3 f3VecReflectNormal = normalize(f3VecReflect);

	float fDiffuse = 0;
	float fReflect = 0;
	float fOcclusionRatio = 1.f;

	float fLightDot = max(dot(f3VecGradNormal, f3VecLightWS), 0);
	fDiffuse = fLightDot;

	float fDot2 = dot(f3VecReflectNormal, f3VecLightWS);
	float fReflectDot = max(fDot2, 0);
	fReflect = pow(fReflectDot, volObj.f4ShadingFactor.w);

	float3 f3OutColor = float3(1, 0, 0);// f4ColorOTF.rgb * (volObj.f4ShadingFactor.x + volObj.f4ShadingFactor.y * fDiffuse + volObj.f4ShadingFactor.z * fReflect) * fOcclusionRatio;
*/

	return f4OutColor;
}

float4 vxFusionVisibility(float4 fCO1, float4 fCO2)
{
    float4 f4CO;
    //f4CO.a = 1.f - (1.f - fCO1.a) * (1.f - fCO2.a);
    //f4CO.a = fCO1.a + fCO2.a - fCO1.a * fCO2.a;

    f4CO.rgb = fCO2.rgb + fCO1.rgb - (fCO1.rgb * fCO2.a + fCO2.rgb * fCO1.a) * 0.5f;
	f4CO = fCO2 + fCO1 - (fCO1 * fCO2.a + fCO2 * fCO1.a) * 0.5f;

	//float4 f4CO = (float4)0;
	//float fAlphaSum = fCO1.a + fCO2.a;
	//if(fAlphaSum > 0.0001f)
	//{
	//	f4CO.a = fCO1.a + fCO2.a - fCO1.a * fCO2.a;
	//	f4CO.rgb = (fCO1.rgb + fCO2.rgb) / fAlphaSum * f4CO.a;
	//}
	return f4CO;
};

// Structure 안에 Array 는 삼가자. as input?!
struct VxRaySegment
{
	float4 f4Visibility;
	float fDepthBack;
	float fThickness;
};

struct VxIntermixingKernelOut
{
	VxRaySegment stIntegrationRS;	// Includes current intermixed depth
	VxRaySegment stRemainedRS;
	float4 f4IntermixingVisibility;
};

VxIntermixingKernelOut vxIntermixingKernel(VxRaySegment stPriorRS, VxRaySegment stPosteriorRS, float4 f4IntermixingVisibility)
{
	// Overall algorithm computation cost 
	// : 3 branches, 2 visibility interpolations, 2 visibility integrations, and 1 fusion of overlapping ray-segments

	// Safe-Check to avoid zero-division
	stPriorRS.f4Visibility.a = min(stPriorRS.f4Visibility.a, SAFE_OPAQUEALPHA);
	stPosteriorRS.f4Visibility.a = min(stPosteriorRS.f4Visibility.a, SAFE_OPAQUEALPHA);

	// stPriorRS and stPosteriorRS mean stPriorRS.fDepthBack >= stPriorRS.fDepthBack
	VxRaySegment stIntegrationRS;
	VxRaySegment stRemainedRS;

	float fDepthFront_PosteriorRS = stPosteriorRS.fDepthBack - stPosteriorRS.fThickness;
	if(stPriorRS.fDepthBack <= fDepthFront_PosteriorRS)
	{
		// Case 1 : No Overlapping
		stIntegrationRS = stPriorRS;
	}
	else
	{
		float fDepthFront_PriorRS = stPriorRS.fDepthBack - stPriorRS.fThickness;
		if(fDepthFront_PriorRS <= fDepthFront_PosteriorRS)
		{
			// Case 2 : Overlapping each boundary
            stIntegrationRS.fThickness = fDepthFront_PosteriorRS - fDepthFront_PriorRS; 
			stIntegrationRS.fDepthBack = fDepthFront_PosteriorRS;
#ifdef HOMOGENEOUS
			float fA = 1.f - pow(1.f - stPriorRS.f4Visibility.a, stIntegrationRS.fThickness / stPriorRS.fThickness);
			stIntegrationRS.f4Visibility.rgb = stPriorRS.f4Visibility.rgb * fA / stPriorRS.f4Visibility.a;
			stIntegrationRS.f4Visibility.a = fA;
#else
			stIntegrationRS.f4Visibility = stPriorRS.f4Visibility * (stIntegrationRS.fThickness / stPriorRS.fThickness);
#endif
			stPriorRS.fThickness -= stIntegrationRS.fThickness;
			stPriorRS.f4Visibility = (stPriorRS.f4Visibility - stIntegrationRS.f4Visibility) / (1.f - stIntegrationRS.f4Visibility.a);
		}
		else 
		{
			// Case 3 : Over-Wrapped stPriorRS by stPosteriorRS
            stIntegrationRS.fThickness = fDepthFront_PriorRS - fDepthFront_PosteriorRS;
			stIntegrationRS.fDepthBack = fDepthFront_PriorRS;
#ifdef HOMOGENEOUS
			float fA = 1.f - pow(1.f - stPosteriorRS.f4Visibility.a, stIntegrationRS.fThickness / stPosteriorRS.fThickness);
			stIntegrationRS.f4Visibility.rgb = stPosteriorRS.f4Visibility.rgb * fA / stPosteriorRS.f4Visibility.a;
			stIntegrationRS.f4Visibility.a = fA;
#else
			stIntegrationRS.f4Visibility = stPosteriorRS.f4Visibility * (stIntegrationRS.fThickness / stPosteriorRS.fThickness);	
#endif

			stPosteriorRS.fThickness -= stIntegrationRS.fThickness;
			stPosteriorRS.f4Visibility = (stPosteriorRS.f4Visibility - stIntegrationRS.f4Visibility) / (1.f - stIntegrationRS.f4Visibility.a);
		}

		// Case 4 : Fusion Subdivided RS

		if(f4IntermixingVisibility.a <= ERT_ALPHA)
		{
			stIntegrationRS.fThickness += stPriorRS.fThickness;
			stIntegrationRS.fDepthBack = stPriorRS.fDepthBack;
			float4 f4Visibility_FrontSubdividedRS = stPosteriorRS.f4Visibility * (stPriorRS.fThickness / stPosteriorRS.fThickness);	// REDESIGN
			float4 f4Visibility_FusionSubdividedRS = vxFusionVisibility(f4Visibility_FrontSubdividedRS, stPriorRS.f4Visibility);
			stIntegrationRS.f4Visibility += f4Visibility_FusionSubdividedRS * (1.f - stIntegrationRS.f4Visibility.a);

			stPosteriorRS.fThickness -= stPriorRS.fThickness;
			stPosteriorRS.f4Visibility = (stPosteriorRS.f4Visibility - f4Visibility_FrontSubdividedRS) / (1.f - f4Visibility_FrontSubdividedRS.a);
		}

        //stIntegrationRS.f4Visibility = float4(1, 0, 0, 1);
    }
	
	f4IntermixingVisibility += stIntegrationRS.f4Visibility * (1.f - f4IntermixingVisibility.a);
	stRemainedRS = stPosteriorRS;
	// Safe-Check For Byte Precision //
	if(stIntegrationRS.f4Visibility.a < FLT_OPACITY_MIN__) // DO NOT COUNT OPERATION OF INTERMIXING KERNEL !
	{
		stIntegrationRS.f4Visibility = (float4)0;
		stIntegrationRS.fThickness = 0;
	}
	if(stRemainedRS.f4Visibility.a < FLT_OPACITY_MIN__) // DO NOT COUNT OPERATION OF INTERMIXING KERNEL !
	{
		stRemainedRS.f4Visibility = (float4)0;
		stRemainedRS.fThickness = 0;
	}

	VxIntermixingKernelOut stOut;
	stOut.stIntegrationRS = stIntegrationRS;
	stOut.stRemainedRS = stRemainedRS;
	stOut.f4IntermixingVisibility = f4IntermixingVisibility;
	return stOut;
}

#define VXMIXMAIN	\
	int iCountStoredBuffer = 0;	\
	int iIndexOfPrevOutRSA = 0;	\
	f4IntermixingVisibility = (float4)0;	\
	VxRaySegment stRS_Cur, stRS_PrevOut;\
	VxRaySegment stOutRSAs[NUM_MERGE_LAYERS];\
	[loop]\
	for(i = 0; i < iNumElementsOfCurRSA; i++)	\
	{	\
		stRS_Cur = stCurrentRSAs[i];\
		while(iIndexOfPrevOutRSA < iNumPrevOutRSs && stRS_Cur.fThickness > 0)	\
		{	/*stRS_Cur */\
			stRS_PrevOut = stPrevOutRSAs[iIndexOfPrevOutRSA];\
			if(stRS_Cur.fDepthBack > stRS_PrevOut.fDepthBack)\
			{\
				stPriorRS = stRS_PrevOut;\
				stPosteriorRS = stRS_Cur;\
			}\
			else\
			{\
				stPriorRS = stRS_Cur;\
				stPosteriorRS = stRS_PrevOut;\
			}\
			\
			stIKOut = vxIntermixingKernel(stPriorRS, stPosteriorRS, f4IntermixingVisibility);\
			\
			if(stIKOut.stIntegrationRS.fThickness > 0)\
			{\
				if(iCountStoredBuffer < NUM_MERGE_LAYERS)\
				{\
					stOutRSAs[iCountStoredBuffer] = stIKOut.stIntegrationRS;\
					iCountStoredBuffer++;	\
				}\
				else\
				{\
					VxRaySegment stPrevRS = stOutRSAs[NUM_MERGE_LAYERS - 1];\
					stPrevRS.f4Visibility += stIKOut.stIntegrationRS.f4Visibility * (1.f - stPrevRS.f4Visibility.a);\
					stPrevRS.fThickness += stIKOut.stIntegrationRS.fThickness;	\
					stPrevRS.fDepthBack = stIKOut.stIntegrationRS.fDepthBack;	\
					stOutRSAs[NUM_MERGE_LAYERS - 1] = stPrevRS;\
				}\
			}\
			f4IntermixingVisibility = stIKOut.f4IntermixingVisibility;	\
			if(f4IntermixingVisibility.a > ERT_ALPHA)\
			{\
				iIndexOfPrevOutRSA = iNumPrevOutRSs; /*This implies ERT as well*/\
				i = iNumElementsOfCurRSA;\
				break;\
			}\
			else\
			{\
				if(stRS_Cur.fDepthBack > stRS_PrevOut.fDepthBack)\
				{\
					stRS_Cur = stIKOut.stRemainedRS;\
					iIndexOfPrevOutRSA++;\
				}\
				else\
				{\
					stPrevOutRSAs[iIndexOfPrevOutRSA] = stIKOut.stRemainedRS;\
					stRS_Cur.fThickness = 0;\
					if(stIKOut.stRemainedRS.fThickness == 0)\
						iIndexOfPrevOutRSA++;\
				}\
			}\
		} \
		\
		if(stRS_Cur.fThickness > 0 && iIndexOfPrevOutRSA >= iNumPrevOutRSs) \
		{	/*stPrevOutRSAs 종료*/\
			f4IntermixingVisibility += stRS_Cur.f4Visibility * (1.f - f4IntermixingVisibility.a);	\
			if(iCountStoredBuffer < NUM_MERGE_LAYERS)\
			{\
				stOutRSAs[iCountStoredBuffer] = stRS_Cur;\
				iCountStoredBuffer++;	\
			}\
			else\
			{\
				VxRaySegment stPrevRS = stOutRSAs[NUM_MERGE_LAYERS - 1];\
				stPrevRS.f4Visibility += stRS_Cur.f4Visibility * (1.f - stPrevRS.f4Visibility.a);\
				stPrevRS.fThickness += stRS_Cur.fThickness;	\
				stPrevRS.fDepthBack = stRS_Cur.fDepthBack;	\
				stOutRSAs[NUM_MERGE_LAYERS - 1] = stPrevRS;\
			}\
		}\
	}\
	for(i = iIndexOfPrevOutRSA; i < iNumPrevOutRSs; i++)/*stPrevOutRSAs 만 남아있는 경우*/\
	{\
		stRS_PrevOut = stPrevOutRSAs[i];	\
		f4IntermixingVisibility += stRS_PrevOut.f4Visibility * (1.f - f4IntermixingVisibility.a);	\
		if(iCountStoredBuffer < NUM_MERGE_LAYERS)\
		{\
			stOutRSAs[iCountStoredBuffer] = stRS_PrevOut;\
			iCountStoredBuffer++;	\
		}\
		else\
		{\
			VxRaySegment stPrevRS = stOutRSAs[NUM_MERGE_LAYERS - 1];\
			stPrevRS.f4Visibility += stRS_PrevOut.f4Visibility * (1.f - stPrevRS.f4Visibility.a);\
			stPrevRS.fThickness += stRS_PrevOut.fThickness;	\
			stPrevRS.fDepthBack = stRS_PrevOut.fDepthBack;	\
			stOutRSAs[NUM_MERGE_LAYERS - 1] = stPrevRS;\
		}\
	}\

#define VXMIXOUT \
	VXMIXMAIN\
	\
	[unroll]\
	for(i = 0; i < NUM_MERGE_LAYERS; i++)\
	{\
		if(i < iCountStoredBuffer)\
		{\
			VxRaySegment stRS = stOutRSAs[i];\
			stOut2ndPhaseBuffer.arrayIntVisibilityRSA[i] = vxConvertColorToInt(stRS.f4Visibility);\
			stOut2ndPhaseBuffer.arrayDepthBackRSA[i] = stRS.fDepthBack;\
			stOut2ndPhaseBuffer.arrayThicknessRSA[i] =  stRS.fThickness;\
		}\
		else\
		{\
			stOut2ndPhaseBuffer.arrayIntVisibilityRSA[i] = 0;\
			stOut2ndPhaseBuffer.arrayDepthBackRSA[i] = FLT_MAX;\
			stOut2ndPhaseBuffer.arrayThicknessRSA[i] = 0;\
		}\
	}\
	g_RWB_MultiLayerOut[uiSampleAddr] = stOut2ndPhaseBuffer;\
