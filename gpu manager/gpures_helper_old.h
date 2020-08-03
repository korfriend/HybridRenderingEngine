#pragma once
#include "GpuManager.h"
#include "../gpu_common_res.h"

#include <atomic>
#include <mutex>

#define NUMSHADERS_VR_CS 40
#define NUMSHADERS_SR_VS 7
#define NUMSHADERS_PLANE_SR_VS 5
#define NUMSHADERS_BIASZ_SR_VS 5
#define NUMSHADERS_SR_PS 13
#define NUMSHADERS_MERGE_CS 3
#define NUMSHADERS_GS 3
#define NUMINPUTLAYOUTS 5

#define __BLOCKSIZE 8

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

#define NUMSHADERS_SR_OIT_VS 5
#define NUMSHADERS_SR_OIT_PS 6
#define NUMSHADERS_OIT_CS 4

#define __PS_OIT_KEPTH_PHONGBLINN 0
#define __PS_OIT_KEPTH_DASHEDLINE 1
#define __PS_OIT_KEPTH_MULTITEXTMAPPING 2
#define __PS_OIT_KEPTH_TEXTMAPPING 3
#define __PS_OIT_SINGLELAYER 4
#define __PS_OIT_KEPTH_PHONGBLINN_SEQ 5

#define __CS_OIT_MERGE 0
#define __CS_OIT_SINGLELAYER_TO_KDEPTH 1
#define __CS_OIT_MERGE_SEQ 2
#define __CS_OIT_SINGLELAYER_TO_KDEPTH_SEQ 3


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
//#define __CB_SR_FLOATENCODE 18
#define __CB_COUNT 18

#define __GRES_FB_V1_COUNT 13
#define __GRES_FB_V1_ROUT_RGBA_0 0
#define __GRES_FB_V1_ROUT_RGBA_1 1
#define __GRES_FB_V1_ROUT_RGBA_2 2
#define __GRES_FB_V1_ROUT_RGBA_3 3
#define __GRES_FB_V1_ROUT_DEPTH_0 4
#define __GRES_FB_V1_ROUT_DEPTH_1 5
#define __GRES_FB_V1_ROUT_DEPTH_2 6
#define __GRES_FB_V1_ROUT_DEPTH_3 7
#define __GRES_FB_V1_DOUT_DS 8
#define __GRES_FB_V1_DEEP_0 9
#define __GRES_FB_V1_DEEP_1 10
#define __GRES_FB_V1_SYS_ROUT 11
#define __GRES_FB_V1_SYS_DOUT 12

#define MAXLEVEL 2	// SHADOW
#define NUM_TEXRT_LAYERS 4
#define NUM_MERGE_LAYERS 4

namespace grd_helper_legacy
{
	enum EnumCullingMode {
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
		ID3D11ComputeShader* pdx11CS_VR_Shaders[NUMSHADERS_VR_CS];
		ID3D11ComputeShader* pdx11CS_MergeTextures[NUMSHADERS_MERGE_CS];
		ID3D11ComputeShader* pdx11CS_Outline;
		ID3D11VertexShader* pdx11VS_SR_Shaders[NUMSHADERS_SR_VS];	// P / PN / PT / PNT / PNT_DEV / PROXY
		ID3D11VertexShader* pdx11VS_PLANE_SR_Shaders[NUMSHADERS_PLANE_SR_VS];
		ID3D11VertexShader* pdx11VS_FBIAS_SR_Shaders[NUMSHADERS_BIASZ_SR_VS];
		ID3D11PixelShader* pdx11PS_SR_Shaders[NUMSHADERS_SR_PS];
		ID3D11GeometryShader* pdx11GS_Shaders[NUMSHADERS_GS];
		ID3D11InputLayout* pdx11IinputLayouts[NUMINPUTLAYOUTS];	// P / PN / PT / PNT / PTTT

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

			memset(pdx11CS_VR_Shaders, NULL, sizeof(ID3D11ComputeShader)* NUMSHADERS_VR_CS);
			memset(pdx11CS_MergeTextures, NULL, sizeof(ID3D11ComputeShader)* NUMSHADERS_MERGE_CS);
			memset(pdx11VS_SR_Shaders, NULL, sizeof(ID3D11VertexShader)* NUMSHADERS_SR_VS);
			memset(pdx11VS_PLANE_SR_Shaders, NULL, sizeof(ID3D11VertexShader)* NUMSHADERS_PLANE_SR_VS);
			memset(pdx11VS_FBIAS_SR_Shaders, NULL, sizeof(ID3D11VertexShader)* NUMSHADERS_BIASZ_SR_VS);
			memset(pdx11PS_SR_Shaders, NULL, sizeof(ID3D11PixelShader)* NUMSHADERS_SR_PS);
			memset(pdx11GS_Shaders, NULL, sizeof(ID3D11GeometryShader)* NUMSHADERS_GS);
			memset(pdx11IinputLayouts, NULL, sizeof(ID3D11InputLayout)* NUMINPUTLAYOUTS);

			pdx11CS_Outline = NULL;

			pdx11BufProxyVertice = NULL;
		}
	};
	//#include "VXDX11Helper.h" // temp;

	struct RWB_Output_Layers	// for CS //
	{
		int iVisibilityLayers[NUM_MERGE_LAYERS]; // ARGB : high to low
		float fDepthLayers[NUM_MERGE_LAYERS];
	};

	struct CB_VrVolumeObject
	{
		// Volume Information and Clipping Information
		XMMATRIX dxmatWS2TS;	// for Sampling and Ray Traversing
		XMMATRIX dxmatAlginedWS;
		XMMATRIX dxmatClipBoxTransform;  // To Clip Box Space (BS)

		// 1st bit - Clipping Plane (1) or Not (0)
		// 2nd bit - Clipping Box (1) or Not (0)
		// 24~31bit : Sculpt Mask Value (1 byte)
		int	iFlags;
		XMFLOAT3 f3PosClipPlaneWS;

		XMFLOAT3 f3PosVObjectMaxWS;
		float	fSampleValueRange;	// Value Range;

		XMFLOAT3 f3ClipBoxOrthoMaxWS;
		float	fClipPlaneIntensity;

		XMFLOAT4 f4ShadingFactor;

		XMFLOAT3 f3VecClipPlaneWS;
		float	fSampleDistWS;

		XMFLOAT3 f3VecGradientSampleX;
		float	fOpaqueCorrection;

		XMFLOAT3 f3VecGradientSampleY;
		int iDummy___;

		XMFLOAT3 f3VecGradientSampleZ;
		float fThicknessVirtualzSlab;

		XMFLOAT3 f3SizeVolumeCVS;
		float fVoxelSharpnessForAttributeVolume;
	};

	struct CB_VrBlockObject
	{
		XMFLOAT3 f3SizeBlockTS;
		float fSampleValueRange;
	};

	struct CB_VrBrickChunk
	{
		// TS in brick Chunk not in main Volume
		XMFLOAT3 f3SizeBrickFullTS;
		int iNumBricksInChunkXY;

		XMFLOAT3 f3SizeBrickExTS;
		int iNumBricksInChunkX;

		XMFLOAT3 f3ConvertRatioVTS2CTS;
		int iDummy___;
	};

	struct CB_VrOtf1D
	{
		XMFLOAT4 f4OtfColorEnd;

		int		iOtf1stValue;		// For ESS
		int		iOtfLastValue;
		int		iOtfSize;
		int		iDummy___;
	};

	struct CB_VrCameraState
	{
		XMMATRIX dxmatSS2WS;	// for initialization vectors and positions
		XMMATRIX dxmatRS2SS;

		XMFLOAT3 f3PosEyeWS;
		uint		uiGridOffsetX;

		XMFLOAT3 f3VecViewWS;	// Recommend Normalize Vector for Computing in Single Precision (Float Type)
		uint		uiScreenSizeX;

		XMFLOAT3 f3VecGlobalLightWS;
		uint		uiScreenSizeY;

		// 1st bit - 0 : Orthogonal or 1 : Perspective
		// 2nd bit - 0 : Parallel Light or 1 : Point Spot Light
		int	iFlags;
		XMFLOAT3 f3PosGlobalLightWS;

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
		XMMATRIX	dxmatWS2LS;	// for Sample Depth Map
		XMMATRIX	dxmatWS2WLS;	// for Depth Comparison And Storage

		float fShadowOcclusionWeight;
		float fSampleDistWLS;
		float fOpaqueCorrectionSMap;
		float fDepthBiasSampleCount;
	};

	struct CB_SrPolygonObject
	{
		XMMATRIX dxmatOS2WS;
		XMMATRIX dxmatOS2PS;
		XMFLOAT4 f4Color;
		XMFLOAT4 f4ShadingFactor;	// x : Ambient, y : Diffuse, z : Specular, w : specular

		XMMATRIX dxmatClipBoxWS2BS;  // To Clip Box Space (BS)

		XMFLOAT3 f3ClipBoxMaxBS;
		// 1st bit : 0 (No) 1 (Clip Box)
		// 2nd bit : 0 (No) 1 (Clip plane)
		// 3th bit : 0 (Set texture0 to texture0) 1 (Set global_color to texture0)
		// 5th bit : 0 (Diffuse abs) 1 (Diffuse max)
		// 10th bit : 0 (No XFlip) 1 (XFlip)
		// 11th bit : 0 (No XFlip) 1 (YFlip)
		// 20th bit : 0 (No Dashed Line) 1 (Dashed Line)
		// 21th bit : 0 (Transparent Dash) 1 (Dash As Color Inverted)
		int iFlag;

		XMFLOAT3 f3PosClipPlaneWS;
		int num_letters;
		XMFLOAT3 f3VecClipPlaneWS;
		int outline_mode;

		float fDashDistance;
		float fShadingMultiplier;
		float fDepthForwardBias;
		float pix_thickness;

		float vzthickness;
		int num_safe_loopexit;
		int pobj_dummy_1;
		int pobj_dummy_2;

		CB_SrPolygonObject()
		{
			outline_mode = 0;
		}
	};

	struct CB_SrProjectionCameraState
	{
		XMMATRIX dxmatWS2PS;
		XMMATRIX dxmatSS2WS;

		XMFLOAT3 f3VecLightWS;
		uint uiGridOffsetX;

		XMFLOAT3 f3PosLightWS;
		// 1st bit : 0 (orthogonal), 1 : (perspective)
		// 2nd bit : 0 (parallel), 1 : (spot)
		int iFlag;

		XMFLOAT3 f3PosEyeWS;
		uint uiScreenSizeX;

		XMFLOAT3 f3VecViewWS;
		uint uiScreenSizeY;

		uint num_deep_layers;
		uint iSrCamDummy__0;
		uint iSrCamDummy__1;
		uint iSrCamDummy__2;
	};

	struct CB_SrDeviation
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

	struct CB_VrInterestingRegion
	{
		XMFLOAT3 f3PosPtn0WS;
		float fRadius0;
		XMFLOAT3 f3PosPtn1WS;
		float fRadius1;
		XMFLOAT3 f3PosPtn2WS;
		float fRadius2;
		int iNumRegions; // Max is 3
		XMFLOAT3 f3VrRoidummy___;
	};

	struct CB_SrProxy
	{
		XMMATRIX dxmatOS2PS;

		float fVZThickness;
		uint uiGridOffsetX;
		uint uiScreenSizeX;
		uint uiScreenSizeY;
	};

	int InitializePresettings(VmGpuManager* pCGpuManager, GpuDX11CommonParameters& gpu_params);
	void DeinitializePresettings();

	// volume/block structure
	bool UpdateOtfBlocks(GpuRes& gres, const VmVObjectVolume* vobj, const VmTObject	* tobj,
		const bool update_blks, const int sculpt_value, LocalProgress* progress = NULL);
	bool UpdateOtfBlocks(GpuRes& gres, const VmVObjectVolume* main_vobj, const VmVObjectVolume* mask_vobj,
		const map<int, VmTObject*>& mapTObjects, const int main_tmap_id, const double* mask_tmap_ids, const int num_mask_tmap_ids,
		const bool update_blks, const bool use_mask_otf, const int sculpt_value, LocalProgress* progress = NULL);
	bool UpdateMinMaxBlocks(GpuRes& gres_min, GpuRes& gres_max, const VmVObjectVolume* vobj, LocalProgress* progress = NULL);
	// bool UpdateAOMask(const VmVObjectVolume* vobj, LocalProgress* progress = NULL); // to do
	bool UpdateVolumeModel(GpuRes& gres, const VmVObjectVolume* vobj, const bool use_nearest_max, LocalProgress* progress = NULL);

	bool UpdateTMapBuffer(GpuRes& gres, const VmTObject* main_tobj,
		const map<int, VmTObject*>& tobj_map, const double* series_ids, const double* visible_mask, const int otf_series, const bool update_tf_content, LocalProgress* progress = NULL);

	// primitive structure
	bool UpdatePrimitiveModel(GpuRes& gres_vtx, GpuRes& gres_idx, GpuRes& gres_tex, VmVObjectPrimitive* pobj, LocalProgress* progress = NULL);

	// framebuffer structure
	bool UpdateFrameBuffer(GpuRes& gres, const VmIObject* iobj,
		const string& res_name,
		const GpuResType gres_type,
		const uint bind_flag,
		const uint dx_format,
		const bool is_system_out,
		const int num_deeplayers = 1,
		const int structured_stride = 0);


	// Compute Constant Buffers //
	bool SetCbVrBlockObj(CB_VrBlockObject* pCBVrBlockObj, VmVObjectVolume* pCVolume, int iLevelBlock, float fScaleValue);
	bool SetCbVrCamera(CB_VrCameraState* pCBVrCamera, VmCObject* pCCObject, vmint2 i2ScreenSizeFB, map<string, void*>* pmapCustomParameter);
	bool SetCbVrOtf1D(CB_VrOtf1D* pCBVrOtf1D, VmTObject* pCTObject, int iOtfIndex, map<string, void*>* pmapCustomParameter);
	bool SetCbVrShadowMap(CB_VrShadowMap* pCBVrShadowMap, CB_VrCameraState* pCBVrCamStateForShadowMap, vmfloat3 f3PosOverviewBoxMinWS, vmfloat3 f3PosOverviewBoxMaxWS, map<string, void*>* pmapCustomParameter);

	// Compute Constant Buffers Using VmLObject pCLObject* pCLObjectForParameters//
	bool SetCbVrObject(CB_VrVolumeObject* pCBVrVolumeObj, bool high_samplerate, VmVObjectVolume* pCVObjectVolume, VmCObject* pCCObject, vmint3 i3SizeVolumeTextureElements, VmLObject* pCLObjectForParameters, int obj_param_src_id, float vz_thickness);
	bool SetCbVrSurfaceEffect(CB_VrSurfaceEffect* pCBVrSurfaceEffect, float fDistSample, VmLObject* pCLObjectForParameters, int obj_param_src_id);
	bool SetCbVrModulation(CB_VrModulation* pCBVrModulation, VmVObjectVolume* pCVObjectVolume, vmfloat3 f3PosEyeWS, VmLObject* pCLObjectForParameters, int obj_param_src_id);
	bool SetCbVrInterestingRegion(CB_VrInterestingRegion* pCBVrInterestingRegion, VmLObject* pCLObjectForParameters, int obj_param_src_id);
}