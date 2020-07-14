#pragma once
#include "VXGPUManager.h"
#include "resource.h"
#include <d3dx9math.h>	// For Math and Structure
#include <d3d11.h>
#include <d3dx11.h>

#include <atomic>
#include <mutex>

/*
_T("BUFFER_PRIMITIVE_VERTEX_LIST")
_T("BUFFER_PRIMITIVE_INDEX_LIST")

_T("TEXTURE3D_VOLUME_DEFAULT")
_T("TEXTURE3D_VOLUME_DOWNSAMPLE")
_T("TEXTURE3D_VOLUME_BLOCKMAP")
_T("TEXTURE3D_VOLUME_BRICKCHUNK")

_T("TEXTURE2DARRAY_IMAGESTACK")

_T("BUFFER_IOBJECT")
_T("BUFFER_IOBJECT_RESULTOUT")
_T("BUFFER_IOBJECT_SYSTEM")

_T("TEXTURE2D_IOBJECT")
_T("TEXTURE2D_IOBJECT_DEPTHSTENCIL")
_T("TEXTURE2D_IOBJECT_RENDEROUT")
_T("TEXTURE2D_IOBJECT_SYSTEM")
_T("TEXTURE2D_TOBJECT_2D")
_T("BUFFER_TOBJECT_OTFSERIES")
_T("TEXTURE2D_CMM_TEXT")

_T("DEVICE_POINTER")
_T("DEVICE_CONTEXT_POINTER")
_T("DEVICE_ADAPTER_DESC")
_T("FEATURE_LEVEL")
/**/

#define VXSAFE_RELEASE(p)			if(p) { (p)->Release(); (p)=NULL; }

// Constant Buffers //
#define __CB_VR_VOLOBJ 0
#define __CB_VR_CAMSTATE 1
#define __CB_VR_OTF1D 2
#define __CB_VR_BLOCKOBJ 3
#define __CB_VR_BRICKCHUNK 4
#define __CB_VR_SURFACEEFFECT 5
#define __CB_VR_MODULATION 6
#define __CB_VR_SHADOWMAP 7
#define __CB_SR_POLYGONOBJ 8
#define __CB_SR_CAMSTATE 9
#define __CB_SR_DEVIATION 10
#define __CB_VR_INTERESTINGREGION 11
#define __CB_SR_VIRTUALZSLABRS 12
#define __CB_VR_OTF1D_SUPERIMPOSEMPR 13
#define __CB_SR_PROXY 14
#define __CB_VR_OTF1D_Ex 15
#define __CB_VR_VOLOBJ_Ex 16
#define __CB_VR_MODULATION_Ex 17
#define __CB_COUNT 18

//#define ____MY_PERFORMANCE_TEST
#define __CS_MERGE_SROUT_AND_LAYEROUT_TO_LAYEROUT 0
#define __CS_MERGE_LAYEROUT_TO_RENDEROUT 1

// Initialized once //
#define __RState_SOLID_CW 0
#define __RState_SOLID_CCW 1
#define __RState_SOLID_NONE 2
#define __RState_WIRE_CW 3
#define __RState_WIRE_CCW 4
#define __RState_WIRE_NONE 5
#define __RState_ANTIALIASING_SOLID_CW 6
#define __RState_ANTIALIASING_SOLID_CCW 7
#define __RState_ANTIALIASING_SOLID_NONE 8
#define __RState_ANTIALIASING_WIRE_CW 9
#define __RState_ANTIALIASING_WIRE_CCW 10
#define __RState_ANTIALIASING_WIRE_NONE 11
#define __RState_COUNT 12

#define __SState_POINT_CLAMP 0
#define __SState_LINEAR_CLAMP 1
#define __SState_POINT_ZEROBORDER 2
#define __SState_LINEAR_ZEROBORDER 3
#define __SState_COUNT 4

#define __DSState_DEPTHENABLE_ALWAYS 0
#define __DSState_DEPTHENABLE_LESSEQUAL 1
#define __DSState_DEPTHENABLE_GREATER 2
#define __DSState_DEPTHENABLE_LESS 3
#define __DSState_COUNT 4

#define MAXLEVEL 2	// SHADOW
#define NUM_TEXRT_LAYERS 4
#define NUM_MERGE_LAYERS 4	// Max is 8

#pragma region SR (PLANE) Definitions
#define __FB_TX2D_RENDEROUT 0
#define __FB_RTV_RENDEROUT 1
#define __FB_SRV_RENDEROUT 2
#define __FB_TX2D_DEPTH 3
#define __FB_DSV_DEPTH 4

#define __FB_TX2D_DEPTH_ZTHICKCS 5
#define __FB_RTV_DEPTH_ZTHICKCS 6
#define __FB_SRV_DEPTH_ZTHICKCS 7
#define __FB_PLANE_COUNT___ 8

#define __FB_TX2D_DEPTH_DUAL 8
#define __FB_SRV_DEPTH_DUAL 9
#define __FB_RTV_DEPTH_DUAL 10
#define __FB_COUNT 11

// for unsupported version of 'Compute Shader'
#define __EXFB_TX2D_MERGER_RAYSEGMENT 0
#define __EXFB_SRV_MERGE_RAYSEGMENT 1
#define __EXFB_RTV_MERGE_RAYSEGMENT 2
#define __EXFB_COUNT 3
#pragma endregion

#define NUMSHADERS_VR 53
#define NUMSHADERS_SR_VS 7
#define NUMSHADERS_PLANE_SR_VS 5
#define NUMSHADERS_SR_PS 16
#define NUMSHADERS_MERGE 2
#define NUMINPUTLAYOUTS 5

#define __VS_P 0 
#define __VS_PN 1
#define __VS_PT 2
#define __VS_PNT 3
#define __VS_PTTT 4
#define __VS_PNT_DEV 5
#define __VS_PROXY 6

#define __PS_MAPPING_TEX1 0
#define __PS_PHONG_TEX1COLOR 1
#define __PS_PHONG_GLOBALCOLOR 2
#define __PS_PHONG_GLOBALCOLOR_MARKERONSURFACE 3
#define __PS_PHONG_GLOBALCOLOR_MAXSHADING 4
#define __PS_PHONG_GLOBALCOLOR_MAXSHADING_MARKERONSURFACE 5
#define __PS_VOLUME_DEVIATION 6
#define __PS_CMM_LINE 7
#define __PS_CMM_TEXT 8
#define __PS_CMM_MULTI_TEXTS 9
#define __PS_VOLUME_SUPERIMPOSE 10
#define __PS_IMAGESTACK_MAP 11
#define __PS_AIRWAY_ANALYSIS 12
#define __PS_MERGE_LAYERS_TO_RENDEROUT 13
#define __PS_MERGE_RSOUT_TO_LAYERS 14
#define __PS_MERGE_RSOUT_LOD2X_TO_LAYERS 15

enum EnumCullingMode{
	rxSR_CULL_NONE = 0,
	rxSR_CULL_DEFAULT,
	rxSR_CULL_FORCED_CW,
	rxSR_CULL_FORCED_CCW,
};

struct GpuDX11CommonParameters
{
	bool bIsInitialized;
	D3D_FEATURE_LEVEL eDx11FeatureLevel;
	DXGI_ADAPTER_DESC stDx11Adapter;

	ID3D11Device* pdx11Device;
	ID3D11DeviceContext* pdx11DeviceImmContext;

	// Shaders //
	ID3D11ComputeShader* pdx11CS_VR_Shaders[NUMSHADERS_VR];
	ID3D11ComputeShader* pdx11CS_MergeTextures[NUMSHADERS_MERGE];
	ID3D11VertexShader* pdx11VS_SR_Shaders[NUMSHADERS_SR_VS];	// P / PN / PT / PNT / PNT_DEV / PROXY
	ID3D11VertexShader* pdx11VS_PLANE_SR_Shaders[NUMSHADERS_PLANE_SR_VS];
	ID3D11PixelShader* pdx11PS_SR_Shaders[NUMSHADERS_SR_PS];
	ID3D11InputLayout* pdx11IinputLayouts[NUMINPUTLAYOUTS];	// P / PN / PT / PNT / PTTT

	// Buffers //
	ID3D11Buffer* pdx11BufImposeVertice = NULL;
	ID3D11Buffer* pdx11BufImposeIndice = NULL;
	ID3D11Buffer* pdx11BufProxyVertice = NULL;

	// STATE //
	ID3D11DepthStencilState* pdx11DSStates[__DSState_COUNT];
	ID3D11RasterizerState* pdx11RStates[__RState_COUNT];
	ID3D11SamplerState* pdx11SStates[__SState_COUNT];
	ID3D11Buffer* pdx11BufConstParameters[__CB_COUNT];

	GpuDX11CommonParameters()
	{
		bIsInitialized = false;
		eDx11FeatureLevel = D3D_FEATURE_LEVEL_9_1;
		memset(&stDx11Adapter, NULL, sizeof(DXGI_ADAPTER_DESC));

		pdx11Device = NULL;
		pdx11DeviceImmContext = NULL;
		
		memset(pdx11DSStates, NULL, sizeof(ID3D11DepthStencilState)*__DSState_COUNT);
		memset(pdx11RStates, NULL, sizeof(ID3D11RasterizerState)*__RState_COUNT);
		memset(pdx11SStates, NULL, sizeof(ID3D11SamplerState)*__SState_COUNT);

		memset(pdx11BufConstParameters, NULL, sizeof(ID3D11Buffer)*__CB_COUNT);

		memset(pdx11CS_VR_Shaders, NULL, sizeof(ID3D11ComputeShader)* NUMSHADERS_VR);
		memset(pdx11CS_MergeTextures, NULL, sizeof(ID3D11ComputeShader)* NUMSHADERS_MERGE);
		memset(pdx11VS_SR_Shaders, NULL, sizeof(ID3D11VertexShader)* NUMSHADERS_SR_VS);
		memset(pdx11VS_PLANE_SR_Shaders, NULL, sizeof(ID3D11VertexShader)* NUMSHADERS_PLANE_SR_VS);
		memset(pdx11PS_SR_Shaders, NULL, sizeof(ID3D11PixelShader)* NUMSHADERS_SR_PS);
		memset(pdx11IinputLayouts, NULL, sizeof(ID3D11InputLayout)* NUMINPUTLAYOUTS);

		pdx11BufImposeVertice = NULL;
		pdx11BufImposeIndice = NULL;
		pdx11BufProxyVertice = NULL;
	}
};

struct CB_VrVolumeObject
{
	// Volume Information and Clipping Information
	D3DXMATRIX matWS2TS;	// for Sampling and Ray Traversing
	D3DXMATRIX matAlginedWS;
	D3DXMATRIX matClipBoxTransform;  // To Clip Box Space (BS)

	// 1st bit - Clipping Plane (1) or Not (0)
	// 2nd bit - Clipping Box (1) or Not (0)
	// 24~31bit : Sculpt Mask Value (1 byte)
	int	iFlags;
	D3DXVECTOR3 f3PosClipPlaneWS;

	D3DXVECTOR3 f3PosVObjectMaxWS;
	float	fSampleValueRange;	// Value Range;

	D3DXVECTOR3 f3ClipBoxOrthoMaxWS;
	float	fClipPlaneIntensity;

	D3DXVECTOR4 f4ShadingFactor;

	D3DXVECTOR3 f3VecClipPlaneWS;
	float	fSampleDistWS;

	D3DXVECTOR3 f3VecGradientSampleX;
	float	fOpaqueCorrection;

	D3DXVECTOR3 f3VecGradientSampleY;
	int iDummy___;

	D3DXVECTOR3 f3VecGradientSampleZ;
	float fThicknessVirtualzSlab;

	D3DXVECTOR3 f3SizeVolumeCVS;
	float fVoxelSharpnessForAttributeVolume;
};

struct CB_VrBlockObject
{
	D3DXVECTOR3 f3SizeBlockTS;
	float fSampleValueRange;
};

struct CB_VrBrickChunk
{
	// TS in brick Chunk not in main Volume
	D3DXVECTOR3 f3SizeBrickFullTS;
	int iNumBricksInChunkXY;

	D3DXVECTOR3 f3SizeBrickExTS;
	int iNumBricksInChunkX;

	D3DXVECTOR3 f3ConvertRatioVTS2CTS;
	int iDummy___;
};

struct CB_VrOtf1D
{
	D3DXVECTOR4 f4OtfColorEnd;

	int		iOtf1stValue;		// For ESS
	int		iOtfLastValue;
	int		iOtfSize;
	int		iDummy___;
};

struct CB_VrCameraState
{
	D3DXMATRIX matSS2WS;	// for initialization vectors and positions

	D3DXVECTOR3 f3PosEyeWS;
	uint		uiGridOffsetX;

	D3DXVECTOR3 f3VecViewWS;	// Recommend Normalize Vector for Computing in Single Precision (Float Type)
	uint		uiScreenSizeX;

	D3DXVECTOR3 f3VecGlobalLightWS;
	uint		uiScreenSizeY;

	// 1st bit - 0 : Orthogonal or 1 : Perspective
	// 2nd bit - 0 : Parallel Light or 1 : Point Spot Light
	int	iFlags;
	D3DXVECTOR3 f3PosGlobalLightWS;
};

struct CB_VrSurfaceEffect
{
	// Ratio //
	// 1st bit : AO or Not , 2nd bit : Anisotropic BRDF or Not , 3rd bit : Apply Shading Factor or Not
	// 4th bit : 0 : Normal Curvature Map (2D), 1 : Apply Concaveness
	// 5th bit : Concaveness Direction or Not
	int iIsoSurfaceRenderingMode;
	int iOcclusionNumSamplesPerKernelRay;
	float fOcclusionSampleStepDistOfKernelRay;
	float fPhongBRDF_DiffuseRatio;

	float fPhongBRDF_ReflectanceRatio;
	float fPhongExpWeightU;
	float fPhongExpWeightV;
	int iSizeCurvatureKernel;

	float fThetaCurvatureTable;
	float fRadiusCurvatureTable;
	float fDummy0;
	float fDummy1;
};

struct CB_VrModulation
{
	float	fGradMagAmplifier;	//[0.1, 500]
	float	fRevealingFactor;	//[0, ]
	float	fSharpnessFactor;	//[0, 1]
	float	fDistEyeMaximum;

	float	fMaxGradientSize;
	float	fdummy____0;
	float	fdummy____1;
	float	fdummy____2;
};

struct CB_VrShadowMap
{   
	D3DXMATRIX	matWS2LS;	// for Sample Depth Map
	D3DXMATRIX	matWS2WLS;	// for Depth Comparison And Storage

	float fShadowOcclusionWeight;
	float fSampleDistWLS;
	float fOpaqueCorrectionSMap;
	float fDepthBiasSampleCount;
};

struct CB_SrPolygonObject
{
	D3DXMATRIX matOS2WS;
	D3DXMATRIX matOS2PS;
	D3DXVECTOR4 f4Color;
	D3DXVECTOR4 f4ShadingFactor;	// x : Ambient, y : Diffuse, z : Specular, w : specular

	D3DXMATRIX matClipBoxWS2BS;  // To Clip Box Space (BS)

	D3DXVECTOR3 f3ClipBoxMaxBS;
	// 1st bit : 0 (No) 1 (Clip Box)
	// 2nd bit : 0 (No) 1 (Clip plane)
	// 10th bit : 0 (No XFlip) 1 (XFlip)
	// 11th bit : 0 (No XFlip) 1 (YFlip)
	// 20th bit : 0 (No Dashed Line) 1 (Dashed Line)
	// 21th bit : 0 (Transparent Dash) 1 (Dash As Color Inverted)
	int iFlag;

	float fVThickness;
	float fDashDistance;
	float fShadingMultiplier;
	int iDummy__0;
};

struct CB_SrProjectionCameraState
{
	D3DXMATRIX matWS2PS;
	D3DXMATRIX matSS2WS;

	D3DXVECTOR3 f3VecLightWS;
	float fVThicknessGlobal;	// Merging Stage 에서 사용

	D3DXVECTOR3 f3PosLightWS;
	// 1st bit : 0 (orthogonal), 1 : (perspective)
	// 2nd bit : 0 (parallel), 1 : (spot)
	int iFlag;

	D3DXVECTOR3 f3PosEyeWS;
	int iDummy__0;

	D3DXVECTOR3 f3VecViewWS;
	int iDummy__1;
};

struct CB_SrDeviation
{
	float fMinMappingValue;
	float fMaxMappingValue;
	int iChannelID;	// 0 to 2
	int iValueRange;

	int iIsoValueForVolume;
	int iDummy__0;
	int iDummy__1;
	int iDummy__2;
};

struct CB_VrInterestingRegion
{
	D3DXVECTOR3 f3PosPtn0WS;
	float fRadius0;
	D3DXVECTOR3 f3PosPtn1WS;
	float fRadius1;
	D3DXVECTOR3 f3PosPtn2WS;
	float fRadius2;
	int iNumRegions; // Max is 3
	D3DXVECTOR3 f3dummy___;
};

struct CB_SrProxy
{
	D3DXMATRIX matOS2PS;
};

__vxstatic bool HDx11AllocAndUpdateBlockMap(float** ppfBlocksMap, ushort** ppusBlocksMap, const SVXVolumeBlock* psvxVolumeResBlock, DXGI_FORMAT eDgiFormat, SVXLocalProgress* psvxLocalProgress);

__vxstatic int HDx11InitializeGpuStatesAndBasicResources(CVXGPUManager* pCGpuManager, GpuDX11CommonParameters** ppParam /*out*/);
__vxstatic void HDx11ReleaseCommonResources();

__vxstatic HRESULT HDx11CompiledShaderSetting(ID3D11Device* pdx11Device, HMODULE hModule, LPCWSTR pSrcResource,
	LPCSTR strShaderProfile, ID3D11DeviceChild** ppdx11Shader/*out*/, 
	D3D11_INPUT_ELEMENT_DESC* pInputLayoutDesc = NULL, uint uiNumOfInputElements = 0, ID3D11InputLayout** ppdx11LayoutInputVS = NULL);
__vxstatic HRESULT HDx11ShaderSetting(ID3D11Device* pdx11Device, wstring* pstrShaderFilePath, HMODULE hModule, LPCWSTR pSrcResource, LPCSTR strShaderProfile, LPCSTR strShaderFuncName, ID3D11DeviceChild** ppdx11Shader/*out*/,
	D3D11_INPUT_ELEMENT_DESC* pInputLayoutDesc = NULL, uint uiNumOfInputElements = 0, ID3D11InputLayout** ppdx11LayoutInputVS = NULL);

__vxstatic bool HDx11ModifyBlockMap(CVXVObjectVolume* pCObjectVolume, SVXGPUResourceArchive* psvxGpuResourceBlockTexture, SVXGPUResourceArchive* psvxGpuResourceBlockSRView, bool bIsImmutable);

// Compute Constant Buffers //
__vxstatic bool HDx11ComputeConstantBufferVrBlockObj(CB_VrBlockObject* pCBVrBlockObj, CVXVObjectVolume* pCVolume, int iLevelBlock, float fScaleValue);
__vxstatic bool HDx11ComputeConstantBufferVrBrickChunk(CB_VrBrickChunk* pCBVrBrickChunk, CVXVObjectVolume* pCVolume, int iLevelBlock, SVXGPUResourceArchive* psvxResBrickChunk);
__vxstatic bool HDx11ComputeConstantBufferVrCamera(CB_VrCameraState* pCBVrCamera, int iGridSize, CVXCObject* pCCObject, vxint2 i2ScreenSizeFB, map<wstring, wstring>* pmapCustomParameter);
__vxstatic bool HDx11ComputeConstantBufferVrOtf1D(CB_VrOtf1D* pCBVrOtf1D, CVXTObject* pCTObject, int iOtfIndex, map<wstring, wstring>* pmapCustomParameter);
__vxstatic bool HDx11ComputeConstantBufferVrShadowMap(CB_VrShadowMap* pCBVrShadowMap, CB_VrCameraState* pCBVrCamStateForShadowMap, vxfloat3 f3PosOverviewBoxMinWS, vxfloat3 f3PosOverviewBoxMaxWS, map<wstring, wstring>* pmapCustomParameter);

// Compute Constant Buffers Using map<wstring, vector<double>>* pmapDValueVolume //
__vxstatic bool HDx11ComputeConstantBufferVrObject(CB_VrVolumeObject* pCBVrVolumeObj, bool bIsDownSample, CVXVObjectVolume* pCVObjectVolume, CVXCObject* pCCObject, vxint3 i3SizeVolumeTextureElements, map<wstring, vector<double>>* pmapDValueVolume);
__vxstatic bool HDx11ComputeConstantBufferVrSurfaceEffect(CB_VrSurfaceEffect* pCBVrSurfaceEffect, map<wstring, vector<double>>* pmapDValueVolume);
__vxstatic bool HDx11ComputeConstantBufferVrModulation(CB_VrModulation* pCBVrModulation, CVXVObjectVolume* pCVObjectVolume, vxfloat3 f3PosEyeWS, map<wstring, vector<double>>* pmapDValueVolume);
__vxstatic bool HDx11ComputeConstantBufferVrInterestingRegion(CB_VrInterestingRegion* pCBVrInterestingRegion, map<wstring, vector<double>>* pmapDValueVolume);

__vxstatic bool HDx11UploadDefaultVolume(SVXGPUResourceArchive* pstGpuResourceVolumeTexture, SVXGPUResourceArchive* pstGpuResourceVolumeSRV, CVXVObjectVolume* pCVolume, wstring strUsageSpecifier, SVXLocalProgress* pstProgress);
__vxstatic bool HDx11MixOut(CVXIObject* pCIObjectMerger, CVXIObject* pCIObjectSystemOut, int iLOD);

__vxstatic void* HDx11GetMutexGpuCriticalPath();
