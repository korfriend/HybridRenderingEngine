#pragma once
#include "../../EngineCores/GpuManager/GpuManager.h"
#include "resource.h"
#include <d3dx9math.h>	// For Math and Structure
#include <d3d11.h>
#include <d3dx11.h>

#include <atomic>
#include <mutex>

using namespace std;
using namespace vmmath;
using namespace vmobjects;
using namespace vmgpuinterface_v1;

#define GMERRORMESSAGE(a) ::MessageBoxA(NULL, a, NULL, MB_OK);

/*
("BUFFER_PRIMITIVE_VERTEX_LIST")
("BUFFER_PRIMITIVE_INDEX_LIST")

("TEXTURE3D_VOLUME_DEFAULT")
("TEXTURE3D_VOLUME_DOWNSAMPLE")
("TEXTURE3D_VOLUME_BLOCKMAP")
("TEXTURE3D_VOLUME_BRICKCHUNK")

("TEXTURE2DARRAY_IMAGESTACK")

("BUFFER_IOBJECT")
("BUFFER_IOBJECT_RESULTOUT")
("BUFFER_IOBJECT_SYSTEM")

("TEXTURE2D_IOBJECT")
("TEXTURE2D_IOBJECT_DEPTHSTENCIL")
("TEXTURE2D_IOBJECT_RENDEROUT")
("TEXTURE2D_IOBJECT_SYSTEM")
("TEXTURE2D_TOBJECT_2D")
("BUFFER_TOBJECT_OTFSERIES")
("TEXTURE2D_CMM_TEXT")

("DEVICE_POINTER")
("DEVICE_CONTEXT_POINTER")
("DEVICE_ADAPTER_DESC")
("FEATURE_LEVEL")
/**/

#define VMSAFE_RELEASE(p)			if(p) { (p)->Release(); (p)=NULL; }

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
#define __CB_SR_FLOATENCODE 18
#define __CB_COUNT 19

//#define ____MY_PERFORMANCE_TEST

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
//#ifdef _X86
//#define NUM_TEXRT_LAYERS 1
//#define NUM_MERGE_LAYERS 1	// Max is 8
//#else
#define NUM_TEXRT_LAYERS 4
#define NUM_MERGE_LAYERS 4
//#endif

#pragma region VR/SR (PLANE) Definitions
#define __FB_TX2D_RENDEROUT 0
#define __FB_RTV_RENDEROUT 1
#define __FB_SRV_RENDEROUT 2
#define __FB_TX2D_DEPTHCS 3
#define __FB_RTV_DEPTHCS 4
#define __FB_SRV_DEPTHCS 5
#define __FB_COUNT_PS 6
#define __FB_UAV_RENDEROUT 6
#define __FB_UAV_DEPTHCS 7
#define __FB_COUNT_CS 8

#define __DS_TX2D_DEPTH 0
#define __DS_DSV_DEPTH 1
#define __DS_COUNT 2

#define __SM_TX2D_DEPTH_SHADOWMAP 0
#define __SM_SRV_DEPTH_SHADOWMAP 1
#define __SM_RTV_DEPTH_SHADOWMAP 2
#define __SM_COUNT_PS 3
#define __SM_UAV_DEPTH_SHADOWMAP 3
#define __SM_COUNT_CS 4

// PS //
#define __EXFB_TX2D_RGBA_LAYERS 0
#define __EXFB_SRV_RGBA_LAYERS 1
#define __EXFB_RTV_RGBA_LAYERS 2
#define __EXFB_TX2D_DEPTHCS_LAYERS 3
#define __EXFB_SRV_DEPTHCS_LAYERS 4
#define __EXFB_RTV_DEPTHCS_LAYERS 5
#define __EXFB_COUNT 6

// CS //
#define __EXRWB_UBUF_LAYERS 0
#define __EXRWB_SRV_LAYERS 1
#define __EXRWB_UAV_LAYERS 2
#define __EXRWB_COUNT 3
#pragma endregion

#define NUMSHADERS_VR_PS 40
#define NUMSHADERS_VR_CS NUMSHADERS_VR_PS
#define NUMSHADERS_SR_VS 7
#define NUMSHADERS_PLANE_SR_VS 5
#define NUMSHADERS_BIASZ_SR_VS 5
#define NUMSHADERS_SR_PS 13
#define NUMSHADERS_MERGE_PS 3
#define NUMSHADERS_GS 2
#define NUMSHADERS_MERGE_CS NUMSHADERS_MERGE_PS
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

#define __PS_MERGE_LAYERS_TO_RENDEROUT 0
#define __PS_MERGE_RSOUT_TO_LAYERS 1
#define __PS_MERGE_ADV_RSOUT_TO_LAYERS 2

#define __CS_MERGE_LAYERS_TO_RENDEROUT 0
#define __CS_MERGE_RSOUT_TO_LAYERS 1
#define __CS_MERGE_ADV_RSOUT_TO_LAYERS 2

#define __BLOCKSIZE 8

#define NUMSHADERS_SR_OIT_VS 5
#define NUMSHADERS_SR_OIT_PS 1
#define NUMSHADERS_OIT_CS 1

enum EnumCullingMode{
	rxSR_CULL_NONE = 0,
	rxSR_CULL_DEFAULT,
	rxSR_CULL_FORCED_CW,
	rxSR_CULL_FORCED_CCW,
};

struct RWB_Output_Layers	// for CS //
{
	int iVisibilityLayers[NUM_MERGE_LAYERS]; // ARGB : high to low
	float fDepthLayers[NUM_MERGE_LAYERS];
};

struct GpuDX11CommonParameters
{
	bool bIsInitialized;
	D3D_FEATURE_LEVEL eDx11FeatureLevel;
	DXGI_ADAPTER_DESC stDx11Adapter;

	ID3D11Device* pdx11Device;
	ID3D11DeviceContext* pdx11DeviceImmContext;

	// Shaders //
	ID3D11PixelShader* pdx11PS_VR_Shaders[NUMSHADERS_VR_PS];
	ID3D11PixelShader* pdx11PS_MergeTextures[NUMSHADERS_MERGE_PS];
	ID3D11ComputeShader* pdx11CS_VR_Shaders[NUMSHADERS_VR_CS];
	ID3D11ComputeShader* pdx11CS_MergeTextures[NUMSHADERS_MERGE_CS];
	ID3D11VertexShader* pdx11VS_SR_Shaders[NUMSHADERS_SR_VS];	// P / PN / PT / PNT / PNT_DEV / PROXY
	ID3D11VertexShader* pdx11VS_PLANE_SR_Shaders[NUMSHADERS_PLANE_SR_VS];
	ID3D11VertexShader* pdx11VS_FBIAS_SR_Shaders[NUMSHADERS_BIASZ_SR_VS];
	ID3D11PixelShader* pdx11PS_SR_Shaders[NUMSHADERS_SR_PS];
	ID3D11GeometryShader* pdx11GS_Shaders[NUMSHADERS_GS];
	ID3D11InputLayout* pdx11IinputLayouts[NUMINPUTLAYOUTS];	// P / PN / PT / PNT / PTTT

	// New Shaders //
	ID3D11VertexShader* pdx11VS_SR_OIT[NUMSHADERS_SR_OIT_VS];
	ID3D11PixelShader* pdx11PS_SR_OIT[NUMSHADERS_SR_OIT_PS];
	ID3D11ComputeShader* pdx11CS_SR_OIT[NUMSHADERS_OIT_CS];

	// Buffers //
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

		memset(pdx11PS_VR_Shaders, NULL, sizeof(ID3D11PixelShader)* NUMSHADERS_VR_PS);
		memset(pdx11PS_MergeTextures, NULL, sizeof(ID3D11PixelShader)* NUMSHADERS_MERGE_PS);
		memset(pdx11CS_VR_Shaders, NULL, sizeof(ID3D11ComputeShader)* NUMSHADERS_VR_CS);
		memset(pdx11CS_MergeTextures, NULL, sizeof(ID3D11ComputeShader)* NUMSHADERS_MERGE_CS);
		memset(pdx11VS_SR_Shaders, NULL, sizeof(ID3D11VertexShader)* NUMSHADERS_SR_VS);
		memset(pdx11VS_PLANE_SR_Shaders, NULL, sizeof(ID3D11VertexShader)* NUMSHADERS_PLANE_SR_VS);
		memset(pdx11VS_FBIAS_SR_Shaders, NULL, sizeof(ID3D11VertexShader)* NUMSHADERS_BIASZ_SR_VS);
		memset(pdx11PS_SR_Shaders, NULL, sizeof(ID3D11PixelShader)* NUMSHADERS_SR_PS);
		memset(pdx11GS_Shaders, NULL, sizeof(ID3D11GeometryShader)* NUMSHADERS_GS);
		memset(pdx11IinputLayouts, NULL, sizeof(ID3D11InputLayout)* NUMINPUTLAYOUTS);

		memset(pdx11VS_SR_OIT, NULL, sizeof(ID3D11VertexShader)* NUMSHADERS_SR_OIT_VS);
		memset(pdx11PS_SR_OIT, NULL, sizeof(ID3D11PixelShader)* NUMSHADERS_SR_OIT_PS);
		memset(pdx11CS_SR_OIT, NULL, sizeof(ID3D11ComputeShader)* NUMSHADERS_OIT_CS);

		pdx11BufProxyVertice = NULL;
	}
};

struct CB_VrVolumeObject
{
	// Volume Information and Clipping Information
	D3DXMATRIX dxmatWS2TS;	// for Sampling and Ray Traversing
	D3DXMATRIX dxmatAlginedWS;
	D3DXMATRIX dxmatClipBoxTransform;  // To Clip Box Space (BS)

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
	D3DXMATRIX dxmatSS2WS;	// for initialization vectors and positions
	D3DXMATRIX dxmatRS2SS;

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

	uint		uiScreenAaBbMinX;
	uint		uiScreenAaBbMaxX;
	uint		uiScreenAaBbMinY;
	uint		uiScreenAaBbMaxY;

	uint		uiRenderingSizeX;
	uint		uiRenderingSizeY;
	uint		__dummy0;
	uint		__dummy1;
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
	float	fMaxGradientSize;
	float	fdummy____0;
	float	fdummy____1;
};

struct CB_VrShadowMap
{   
	D3DXMATRIX	dxmatWS2LS;	// for Sample Depth Map
	D3DXMATRIX	dxmatWS2WLS;	// for Depth Comparison And Storage

	float fShadowOcclusionWeight;
	float fSampleDistWLS;
	float fOpaqueCorrectionSMap;
	float fDepthBiasSampleCount;
};

struct CB_SrPolygonObject
{
	D3DXMATRIX dxmatOS2WS;
	D3DXMATRIX dxmatOS2PS;
	D3DXVECTOR4 f4Color;
	D3DXVECTOR4 f4ShadingFactor;	// x : Ambient, y : Diffuse, z : Specular, w : specular

	D3DXMATRIX dxmatClipBoxWS2BS;  // To Clip Box Space (BS)

	D3DXVECTOR3 f3ClipBoxMaxBS;
	// 1st bit : 0 (No) 1 (Clip Box)
	// 2nd bit : 0 (No) 1 (Clip plane)
	// 10th bit : 0 (No XFlip) 1 (XFlip)
	// 11th bit : 0 (No XFlip) 1 (YFlip)
	// 20th bit : 0 (No Dashed Line) 1 (Dashed Line)
	// 21th bit : 0 (Transparent Dash) 1 (Dash As Color Inverted)
	int iFlag;

	D3DXVECTOR3 f3PosClipPlaneWS;
	int iDummy__0;
	D3DXVECTOR3 f3VecClipPlaneWS;
	int iDummy__1;

	float fDashDistance;
	float fShadingMultiplier;
	float fDepthForwardBias;
	float fDummy__1;
};

struct CB_SrProjectionCameraState
{
	D3DXMATRIX dxmatWS2PS;
	D3DXMATRIX dxmatSS2WS;

	D3DXVECTOR3 f3VecLightWS;
	uint uiGridOffsetX;

	D3DXVECTOR3 f3PosLightWS;
	// 1st bit : 0 (orthogonal), 1 : (perspective)
	// 2nd bit : 0 (parallel), 1 : (spot)
	int iFlag;

	D3DXVECTOR3 f3PosEyeWS;
	uint uiScreenSizeX;

	D3DXVECTOR3 f3VecViewWS;
	uint uiScreenSizeY;
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
	D3DXMATRIX dxmatOS2PS;

	float fVZThickness;
	uint uiGridOffsetX;
	uint uiScreenSizeX;
	uint uiScreenSizeY;
};

// OIT new 
struct CB_FloatEncode
{
	float z_depth_min;
	float z_depth_max;
	float z_thick_min;
	float z_thick_max;
};

__vmstatic bool HDx11AllocAndUpdateBlockMap(uint** ppiBlocksMap, ushort** ppusBlocksMap, int related_tobj_id, VolumeBlocks* pstVolumeResBlock, DXGI_FORMAT eDgiFormat, LocalProgress* pLocalProgress);

__vmstatic int HDx11InitializeGpuStatesAndBasicResources(VmGpuManager_v1* pCGpuManager, GpuDX11CommonParameters** ppParam /*out*/, bool* pbIsAvailableCS);
__vmstatic void HDx11ReleaseCommonResources();

__vmstatic HRESULT HDx11CompiledShaderSetting(ID3D11Device* pdx11Device, HMODULE hModule, LPCWSTR pSrcResource,
	LPCSTR strShaderProfile, ID3D11DeviceChild** ppdx11Shader/*out*/, 
	D3D11_INPUT_ELEMENT_DESC* pInputLayoutDesc = NULL, uint uiNumOfInputElements = 0, ID3D11InputLayout** ppdx11LayoutInputVS = NULL);
__vmstatic HRESULT HDx11ShaderSetting(ID3D11Device* pdx11Device, string* pstrShaderFilePath, HMODULE hModule, LPCWSTR pSrcResource, LPCSTR strShaderProfile, LPCSTR strShaderFuncName, ID3D11DeviceChild** ppdx11Shader/*out*/,
	D3D11_INPUT_ELEMENT_DESC* pInputLayoutDesc = NULL, uint uiNumOfInputElements = 0, ID3D11InputLayout** ppdx11LayoutInputVS = NULL);

__vmstatic bool HDx11ModifyBlockMap(VmVObjectVolume* pCObjectVolume, GpuResource* pGpuResourceBlockTexture, GpuResource* pGpuResourceBlockSRView, bool bIsImmutable);

// Compute Constant Buffers //
__vmstatic bool HDx11ComputeConstantBufferVrBlockObj(CB_VrBlockObject* pCBVrBlockObj, VmVObjectVolume* pCVolume, int iLevelBlock, float fScaleValue);
__vmstatic bool HDx11ComputeConstantBufferVrBrickChunk(CB_VrBrickChunk* pCBVrBrickChunk, VmVObjectVolume* pCVolume, int iLevelBlock, GpuResource* psvxResBrickChunk);
__vmstatic bool HDx11ComputeConstantBufferVrCamera(CB_VrCameraState* pCBVrCamera, VmCObject* pCCObject, vmint2 i2ScreenSizeFB, map<string, void*>* pmapCustomParameter);
__vmstatic bool HDx11ComputeConstantBufferVrOtf1D(CB_VrOtf1D* pCBVrOtf1D, VmTObject* pCTObject, int iOtfIndex, map<string, void*>* pmapCustomParameter);
__vmstatic bool HDx11ComputeConstantBufferVrShadowMap(CB_VrShadowMap* pCBVrShadowMap, CB_VrCameraState* pCBVrCamStateForShadowMap, vmfloat3 f3PosOverviewBoxMinWS, vmfloat3 f3PosOverviewBoxMaxWS, map<string, void*>* pmapCustomParameter);

// Compute Constant Buffers Using VmLObject pCLObject* pCLObjectForParameters//
__vmstatic bool HDx11ComputeConstantBufferVrObject(CB_VrVolumeObject* pCBVrVolumeObj, bool bIsDownSample, VmVObjectVolume* pCVObjectVolume, VmCObject* pCCObject, vmint3 i3SizeVolumeTextureElements, VmLObject* pCLObjectForParameters, int obj_param_src_id);
__vmstatic bool HDx11ComputeConstantBufferVrSurfaceEffect(CB_VrSurfaceEffect* pCBVrSurfaceEffect, float fDistSample, VmLObject* pCLObjectForParameters, int obj_param_src_id);
__vmstatic bool HDx11ComputeConstantBufferVrModulation(CB_VrModulation* pCBVrModulation, VmVObjectVolume* pCVObjectVolume, vmfloat3 f3PosEyeWS, VmLObject* pCLObjectForParameters, int obj_param_src_id);
__vmstatic bool HDx11ComputeConstantBufferVrInterestingRegion(CB_VrInterestingRegion* pCBVrInterestingRegion, VmLObject* pCLObjectForParameters, int obj_param_src_id);

__vmstatic bool HDx11UploadDefaultVolume(GpuResource* pstGpuResourceVolumeTexture, GpuResource* pstGpuResourceVolumeSRV, VmVObjectVolume* pCVolume, string usage_specifier, LocalProgress* _progress);

__vmstatic bool HDx11UploadMesh(GpuResource* pstGpuResourceMeshBufferVtx,
	GpuResource* pstGpuResourceMeshBufferIdx,
	VmVObjectPrimitive* pMesh, LocalProgress* _progress);

__vmstatic void* HDx11GetMutexGpuCriticalPath();