#include "RendererHeader.h"

enum SHADERINDEX {
	__SHADER_DVR_BASIC_DEFAULT = 0,
	__SHADER_DVR_BASIC_DEFAULT_SHADOW,
	__SHADER_DVR_BASIC_DEFAULT_OTFMASK,
	__SHADER_DVR_BASIC_DEFAULT_OTFMASK_SHADOW,
	__SHADER_DVR_BASIC_CLIPSEMIOPAQUE,
	__SHADER_DVR_BASIC_SCULPTMASK,
	__SHADER_DVR_BASIC_MODULATION,
	__SHADER_DVR_BASIC_MODULATION_OTFMASK,
	__SHADER_DVR_LAYERS_DEFAULT, //
	__SHADER_DVR_LAYERS_DEFAULT_SHADOW,
	__SHADER_DVR_LAYERS_DEFAULT_OTFMASK,
	__SHADER_DVR_LAYERS_DEFAULT_OTFMASK_SHADOW,
	__SHADER_DVR_LAYERS_CLIPSEMIOPAQUE,
	__SHADER_DVR_LAYERS_SCULPTMASK,
	__SHADER_DVR_LAYERS_MODULATION,
	__SHADER_DVR_LAYERS_MODULATION_OTFMASK,
	__SHADER_DVR_LAYERS_ADV_DEFAULT, //
	__SHADER_DVR_LAYERS_ADV_DEFAULT_SHADOW,
	__SHADER_DVR_LAYERS_ADV_DEFAULT_OTFMASK,
	__SHADER_DVR_LAYERS_ADV_DEFAULT_OTFMASK_SHADOW,
	__SHADER_DVR_LAYERS_ADV_CLIPSEMIOPAQUE,
	__SHADER_DVR_LAYERS_ADV_SCULPTMASK,
	__SHADER_DVR_LAYERS_ADV_MODULATION,
	__SHADER_DVR_LAYERS_ADV_MODULATION_OTFMASK,
	__SHADER_DVR_SURFACE_DEFAULT, //
	__SHADER_DVR_SURFACE_DEFAULT_SHADOW,
	__SHADER_DVR_SURFACE_DEFAULT_OTFMASK,
	__SHADER_DVR_SURFACE_DEFAULT_OTFMASK_SHADOW,
	__SHADER_DVR_SURFACE_CURVATURE,
	__SHADER_DVR_SURFACE_CCF,
	__SHADER_DVR_SURFACE_MARKER,
	__SHADER_DVR_TEST, //
	__SHADER_XRAY_MAX, //
	__SHADER_XRAY_MAX_OTFMASK,
	__SHADER_XRAY_MIN,
	__SHADER_XRAY_MIN_OTFMASK,
	__SHADER_XRAY_SUM,
	__SHADER_XRAY_SUM_OTFMASK,
	__SHADER_DVR_BASIC_DEFAULT_SHADOWMAP,//
	__SHADER_DVR_BASIC_OTFMASK_SHADOWMAP,
};

#define __RM_DEFAULT 0
#define __RM_CLIPOPAQUE 1
#define __RM_SURFACEEFFECT 2
#define __RM_MODULATION 3
#define __RM_SCULPTMASK 4
#define __RM_SURFACECCF 5
#define __RM_SURFACECURVATURE 6
#define __RM_RAYMAX 10
#define __RM_RAYMIN 11
#define __RM_RAYSUM 12

//#define __CS_5_0_SUPPORT

bool RenderVrCommon(VmFnContainer* _fncontainer,
	VmGpuManager* pCGpuManager,
	GpuDX11CommonParameters* pdx11CommonParams,
	ID3D11PixelShader** ppdx11PS_VRs[NUMSHADERS_VR_PS],
	ID3D11PixelShader** ppdx11PS_Merges[NUMSHADERS_MERGE_PS],
	ID3D11ComputeShader** ppdx11CS_VRs[NUMSHADERS_VR_PS],
	ID3D11ComputeShader** ppdx11CS_Merges[NUMSHADERS_MERGE_CS],
	ID3D11VertexShader* pdx11VS_ProxyRect, ID3D11InputLayout* pdx11IL_ProxyRect,
	ID3D11Buffer* pdx11BufProxyRect,
	bool bIsShadowStage, LocalProgress* pstLocalProgress, double* pdRunTime)
{
	LARGE_INTEGER lIntFreq;
	LARGE_INTEGER lIntCntStart, lIntCntEnd;

	// Get VXObjects //
	vector<VmObject*> vtrInputVolumes;
	_fncontainer->GetVmObjectList(&vtrInputVolumes, VmObjKey(ObjectTypeVOLUME, true));
	vector<VmObject*> vtrInputOTFs;
	_fncontainer->GetVmObjectList(&vtrInputOTFs, VmObjKey(ObjectTypeTMAP, true));
	vector<VmObject*> vtrOutputIPs;
	_fncontainer->GetVmObjectList(&vtrOutputIPs, VmObjKey(ObjectTypeIMAGEPLANE, false));

	VmLObject* pCLObjectForParameters = (VmLObject*)_fncontainer->GetVmObject(VmObjKey(ObjectTypeBUFFER, true), 0);
	VmIObject* pCOutputIObject = (VmIObject*)vtrOutputIPs[0];
	VmIObject* pCMergeIObject = (VmIObject*)vtrOutputIPs[1];

	if (pCMergeIObject == NULL || pCOutputIObject == NULL || pCLObjectForParameters == NULL)
	{
		VMERRORMESSAGE("Some of required VXObjects are missed!");
		return false;
	}

	// Check Parameters //
	bool bForcedUpdateOtf = false;
	fncont_getparamvalue<bool>(_fncontainer, &bForcedUpdateOtf, ("_bool_ForceToUpdateOtf"));
	bool bIsShowBlock = false;
	fncont_getparamvalue<bool>(_fncontainer, &bIsShowBlock, ("_bool_IsShowBlock"));
	bool bIsShadowRenderer = false;
	fncont_getparamvalue<bool>(_fncontainer, &bIsShadowRenderer, ("_bool_IsShadow"));
	bool bIsNoValidObjectInitValue = false;
	fncont_getparamvalue<bool>(_fncontainer, &bIsNoValidObjectInitValue, ("_bool_IsNoValidObject"));
	bool bIsForcedDeferredMode = false;
	fncont_getparamvalue<bool>(_fncontainer, &bIsForcedDeferredMode, ("_bool_IsForcedDeferredMode"));
	bool bValidateMaskVolume = false;
	fncont_getparamvalue<bool>(_fncontainer, &bValidateMaskVolume, ("_bool_ValidateMaskVolume"));
	int iLevelVR = 1;
	fncont_getparamvalue<int>(_fncontainer, &iLevelVR, ("_int_LevelVR"));
	bool bIsFastQualityMode = false;
	fncont_getparamvalue<bool>(_fncontainer, &bIsFastQualityMode, ("_bool_IsFastQualityMode"));
	bool bBlockUpdateForSculptMask = false;
	fncont_getparamvalue<bool>(_fncontainer, &bBlockUpdateForSculptMask, ("_bool_BlockUpdateForSculptMask"));
	//iLevelVR = 2;

	// TEST
	int iTestValue = 0;
	fncont_getparamvalue<int>(_fncontainer, &iTestValue, ("_int_TestValue"));
	int iTestMode = false;
	fncont_getparamvalue<int>(_fncontainer, &iTestMode, ("_int_TestMode"));

	if (iLevelVR != 2) bIsFastQualityMode = false;

	// 1, 2, ..., NUM_TEXRT_LAYERS : 
	int iNumTexureLayers = NUM_TEXRT_LAYERS; // VR 에서는 NUM_TEXRT_LAYERS 을 기본으로 함!
	//_fncontainer->GetCustomParameter(&iNumTexureLayers, ("_int_NumTexureLayers"));
	//iNumTexureLayers = min(iNumTexureLayers, NUM_TEXRT_LAYERS);
	//iNumTexureLayers = max(iNumTexureLayers, 1);
	int iMergeLevel = min(iLevelVR, 1);

	bool bReusePrevRendering = false;
	fncont_getparamvalue<bool>(_fncontainer, &bReusePrevRendering, ("_bool_ReusePrevRendering"));

	bool bForcePreventReuse = false;
	fncont_getparamvalue<bool>(_fncontainer, &bForcePreventReuse, ("_bool_ForcePreventReuse"));
	if (bForcePreventReuse)
		bReusePrevRendering = false;

	if (bReusePrevRendering)
	{
		//bool bIsPrevRenderedInFastMode = false;
		if (!pCOutputIObject->GetCustomParameter("_bool_IsPrevRenderedInFastMode", data_type::dtype<bool>(), &bIsFastQualityMode)
			|| iLevelVR != 2)
			bReusePrevRendering = false;//VMERRORMESSAGE("IMPOSSIBLE!!");
	}

	//if (bIsFastQualityMode) bIsShadowRenderer = false;

#pragma region // SHADER SETTING
	// Shader Re-Compile Setting //
	bool bReloadHLSLObjFiles = false;
	fncont_getparamvalue<bool>(_fncontainer, &bReloadHLSLObjFiles, ("_bool_ReloadHLSLObjFiles"));
	if (bReloadHLSLObjFiles)
	{
#ifdef __CS_5_0_SUPPORT
		string strNames_VR_CS[NUMSHADERS_VR_CS] = {
			("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_BASIC_DEFAULT_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_BASIC_DEFAULT_SHADOW_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_BASIC_DEFAULT_OTFMASK_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_BASIC_DEFAULT_OTFMASK_SHADOW_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_BASIC_CLIPSEMIOPAQUE_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_BASIC_SCULPTMASK_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_BASIC_MODULATION_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_BASIC_MODULATION_OTFMASK_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_LAYERS_DEFAULT_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_LAYERS_DEFAULT_SHADOW_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_LAYERS_DEFAULT_OTFMASK_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_LAYERS_DEFAULT_OTFMASK_SHADOW_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_LAYERS_CLIPSEMIOPAQUE_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_LAYERS_SCULPTMASK_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_LAYERS_MODULATION_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_LAYERS_MODULATION_OTFMASK_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_LAYERS_ADV_DEFAULT_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_LAYERS_ADV_DEFAULT_SHADOW_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_LAYERS_ADV_DEFAULT_OTFMASK_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_LAYERS_ADV_DEFAULT_OTFMASK_SHADOW_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_LAYERS_ADV_CLIPSEMIOPAQUE_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_LAYERS_ADV_SCULPTMASK_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_LAYERS_ADV_MODULATION_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_LAYERS_ADV_MODULATION_OTFMASK_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_SURFACE_DEFAULT_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_SURFACE_DEFAULT_SHADOW_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_SURFACE_DEFAULT_OTFMASK_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_SURFACE_DEFAULT_OTFMASK_SHADOW_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_SURFACE_CURVATURE_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_SURFACE_CCF_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_SURFACE_MARKER_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_TEST_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_XRAY_MAX_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_XRAY_MAX_OTFMASK_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_XRAY_MIN_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_XRAY_MIN_OTFMASK_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_XRAY_SUM_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_XRAY_SUM_OTFMASK_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_BASIC_DEFAULT_SHADOWMAP_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_DVR_BASIC_OTFMASK_SHADOWMAP_cs_5_0")
		};

		for (int i = 0; i < NUMSHADERS_VR_CS; i++)
		{
			string strName = strNames_VR_CS[i];

			FILE* pFile;
			if (fopen_s(&pFile, strName.c_str(), ("rb")) == 0)
			{
				fseek(pFile, 0, SEEK_END);
				ullong ullFileSize = ftell(pFile);
				fseek(pFile, 0, SEEK_SET);
				byte* pyRead = new byte[ullFileSize];
				fread(pyRead, sizeof(byte), ullFileSize, pFile);
				fclose(pFile);

				ID3D11ComputeShader* pdx11CShader = NULL;
				if (pdx11CommonParams->pdx11Device->CreateComputeShader(pyRead, ullFileSize, NULL, &pdx11CShader) != S_OK)
				{
					VMERRORMESSAGE("SHADER COMPILE FAILURE!");
				}
				else
				{
					VMSAFE_RELEASE(*ppdx11CS_VRs[i]);
					*ppdx11CS_VRs[i] = pdx11CShader;
				}
				VMSAFE_DELETEARRAY(pyRead);
			}
		}

		string strNames_MG_CS[NUMSHADERS_MERGE_CS] = {
			("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_MERGE_LAYEROUT_TO_RENDEROUT_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_MERGE_SROUT_AND_LAYEROUT_TO_LAYEROUT_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_MERGE_ADV_SROUT_AND_LAYEROUT_TO_LAYEROUT_cs_5_0")
		};
		for (int i = 0; i < NUMSHADERS_MERGE_CS; i++)
		{
			string strName = strNames_MG_CS[i];

			FILE* pFile;
			if (fopen_s(&pFile, strName.c_str(), ("rb")) == 0)
			{
				fseek(pFile, 0, SEEK_END);
				ullong ullFileSize = ftell(pFile);
				fseek(pFile, 0, SEEK_SET);
				byte* pyRead = new byte[ullFileSize];
				fread(pyRead, sizeof(byte), ullFileSize, pFile);
				fclose(pFile);

				ID3D11ComputeShader* pdx11CShader = NULL;
				if (pdx11CommonParams->pdx11Device->CreateComputeShader(pyRead, ullFileSize, NULL, &pdx11CShader) != S_OK)
				{
					VMERRORMESSAGE("SHADER COMPILE FAILURE!");
				}
				else
				{
					VMSAFE_RELEASE(*ppdx11CS_Merges[i]);
					*ppdx11CS_Merges[i] = pdx11CShader;
				}
				VMSAFE_DELETEARRAY(pyRead);
			}
		}
#else
		string strNames_VR_PS[NUMSHADERS_VR_PS] = {
			("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_BASIC_DEFAULT_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_BASIC_DEFAULT_SHADOW_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_BASIC_DEFAULT_OTFMASK_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_BASIC_DEFAULT_OTFMASK_SHADOW_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_BASIC_CLIPSEMIOPAQUE_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_BASIC_SCULPTMASK_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_BASIC_MODULATION_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_BASIC_MODULATION_OTFMASK_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_LAYERS_DEFAULT_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_LAYERS_DEFAULT_SHADOW_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_LAYERS_DEFAULT_OTFMASK_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_LAYERS_DEFAULT_OTFMASK_SHADOW_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_LAYERS_CLIPSEMIOPAQUE_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_LAYERS_SCULPTMASK_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_LAYERS_MODULATION_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_LAYERS_MODULATION_OTFMASK_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_LAYERS_ADV_DEFAULT_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_LAYERS_ADV_DEFAULT_SHADOW_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_LAYERS_ADV_DEFAULT_OTFMASK_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_LAYERS_ADV_DEFAULT_OTFMASK_SHADOW_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_LAYERS_ADV_CLIPSEMIOPAQUE_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_LAYERS_ADV_SCULPTMASK_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_LAYERS_ADV_MODULATION_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_LAYERS_ADV_MODULATION_OTFMASK_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_SURFACE_DEFAULT_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_SURFACE_DEFAULT_SHADOW_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_SURFACE_DEFAULT_OTFMASK_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_SURFACE_DEFAULT_OTFMASK_SHADOW_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_SURFACE_CURVATURE_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_SURFACE_CCF_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_SURFACE_MARKER_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_TEST_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_XRAY_MAX_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_XRAY_MAX_OTFMASK_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_XRAY_MIN_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_XRAY_MIN_OTFMASK_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_XRAY_SUM_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_XRAY_SUM_OTFMASK_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_BASIC_DEFAULT_SHADOWMAP_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_DVR_BASIC_OTFMASK_SHADOWMAP_ps_4_0")
		};

		for (int i = 0; i < NUMSHADERS_VR_PS; i++)
		{
			string strName = strNames_VR_PS[i];

			FILE* pFile;
			if (fopen_s(&pFile, strName.c_str(), ("rb")) == 0)
			{
				fseek(pFile, 0, SEEK_END);
				ullong ullFileSize = ftell(pFile);
				fseek(pFile, 0, SEEK_SET);
				byte* pyRead = new byte[ullFileSize];
				fread(pyRead, sizeof(byte), ullFileSize, pFile);
				fclose(pFile);

				ID3D11PixelShader* pdx11PShader = NULL;
				if (pdx11CommonParams->pdx11Device->CreatePixelShader(pyRead, ullFileSize, NULL, &pdx11PShader) != S_OK)
				{
					VMERRORMESSAGE("SHADER COMPILE FAILURE!");
				}
				else
				{
					VMSAFE_RELEASE(*ppdx11PS_VRs[i]);
					*ppdx11PS_VRs[i] = pdx11PShader;
				}
				VMSAFE_DELETEARRAY(pyRead);
			}
		}

		string strNames_MG_PS[NUMSHADERS_MERGE_PS] = {
			("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_MERGE_LAYEROUT_TO_RENDEROUT_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_MERGE_SROUT_AND_LAYEROUT_TO_LAYEROUT_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_MERGE_ADV_SROUT_AND_LAYEROUT_TO_LAYEROUT_ps_4_0")
		};
		for (int i = 0; i < NUMSHADERS_MERGE_PS; i++)
		{
			string strName = strNames_MG_PS[i];

			FILE* pFile;
			if (fopen_s(&pFile, strName.c_str(), ("rb")) == 0)
			{
				fseek(pFile, 0, SEEK_END);
				ullong ullFileSize = ftell(pFile);
				fseek(pFile, 0, SEEK_SET);
				byte* pyRead = new byte[ullFileSize];
				fread(pyRead, sizeof(byte), ullFileSize, pFile);
				fclose(pFile);

				ID3D11PixelShader* pdx11PShader = NULL;
				if (pdx11CommonParams->pdx11Device->CreatePixelShader(pyRead, ullFileSize, NULL, &pdx11PShader) != S_OK)
				{
					VMERRORMESSAGE("SHADER COMPILE FAILURE!");
				}
				else
				{
					VMSAFE_RELEASE(*ppdx11PS_Merges[i]);
					*ppdx11PS_Merges[i] = pdx11PShader;
				}
				VMSAFE_DELETEARRAY(pyRead);
			}
		}
#endif
	}
#pragma endregion // SHADER SETTING

#pragma region // IOBJECT OUT
	while (pCOutputIObject->GetFrameBuffer(FrameBufferUsageRENDEROUT, 1) != NULL)
		pCOutputIObject->DeleteFrameBuffer(FrameBufferUsageRENDEROUT, 1);
	if (!pCOutputIObject->ReplaceFrameBuffer(FrameBufferUsageRENDEROUT, 0, data_type::dtype<vmbyte4>(), ("common render out frame buffer : defined in vismtv_inbuilt_renderergpudx module")))
		pCOutputIObject->InsertFrameBuffer(data_type::dtype<vmbyte4>(), FrameBufferUsageRENDEROUT, ("common render out frame buffer : defined in vismtv_inbuilt_renderergpudx module"));

	while (pCOutputIObject->GetFrameBuffer(FrameBufferUsageDEPTH, 1) != NULL)
		pCOutputIObject->DeleteFrameBuffer(FrameBufferUsageDEPTH, 1);
	if (!pCOutputIObject->ReplaceFrameBuffer(FrameBufferUsageDEPTH, 0, data_type::dtype<float>(), ("1st hit screen depth frame buffer : defined in vismtv_inbuilt_renderergpudx module")))
		pCOutputIObject->InsertFrameBuffer(data_type::dtype<float>(), FrameBufferUsageDEPTH, ("1st hit screen depth frame buffer : defined in vismtv_inbuilt_renderergpudx module"));

	if (!pCMergeIObject->ReplaceFrameBuffer(FrameBufferUsageVIRTUAL, 0, data_type::dtype<byte>(), ("Merge Buffer for Merge Layer : defined in vismtv_inbuilt_renderergpudx module")))
		pCMergeIObject->InsertFrameBuffer(data_type::dtype<byte>(), FrameBufferUsageVIRTUAL, ("Merge Buffer for Merge Layer : defined in vismtv_inbuilt_renderergpudx module"));

	if (iLevelVR == 2 || bIsFastQualityMode)
	{
		while (pCMergeIObject->GetFrameBuffer(FrameBufferUsageCUSTOM, 2) != NULL)
			pCMergeIObject->DeleteFrameBuffer(FrameBufferUsageCUSTOM, 2);
		if (!pCMergeIObject->ReplaceFrameBuffer(FrameBufferUsageCUSTOM, 0, data_type::dtype<vmbyte4>(), ("SCBuffer RGBA : defined in vismtv_inbuilt_renderergpudx module")))
			pCMergeIObject->InsertFrameBuffer(data_type::dtype<vmbyte4>(), FrameBufferUsageCUSTOM, ("SCBuffer RGBA : defined in vismtv_inbuilt_renderergpudx module"));
		if (!pCMergeIObject->ReplaceFrameBuffer(FrameBufferUsageCUSTOM, 1, data_type::dtype<float>(), ("SCBuffer Depth : defined in vismtv_inbuilt_renderergpudx module")))
			pCMergeIObject->InsertFrameBuffer(data_type::dtype<float>(), FrameBufferUsageCUSTOM, ("SCBuffer Depth : defined in vismtv_inbuilt_renderergpudx module"));
	}
	else
	{
		while (pCMergeIObject->GetFrameBuffer(FrameBufferUsageCUSTOM, 0) != NULL)
			pCMergeIObject->DeleteFrameBuffer(FrameBufferUsageCUSTOM, 0);
	}
#pragma endregion // IOBJECT OUT


	ID3D11Device* pdx11Device = pdx11CommonParams->pdx11Device;
	ID3D11DeviceContext* pdx11DeviceImmContext = pdx11CommonParams->pdx11DeviceImmContext;

#pragma region // IOBJECT GPU
	GpuResDescriptor gpuResourcesFB_DESCs[__FB_COUNT_CS];
	GpuResource gpuResourcesFB_ARCHs[__FB_COUNT_CS];
	GpuResDescriptor gpuResourceMERGE_CS_DESCs[__EXRWB_COUNT];
	GpuResource gpuResourceMERGE_CS_ARCHs[__EXRWB_COUNT];
	GpuResDescriptor gpuResourceMERGE_PS_DESCs[__EXFB_COUNT];
	GpuResource gpuResourceMERGE_PS_ARCHs[__EXFB_COUNT];
	GpuResDescriptor gpuResourcesSHADOWMAP_DESCs[__SM_COUNT_CS];
	GpuResource gpuResourcesSHADOWMAP_ARCHs[__SM_COUNT_CS];

#ifdef __CS_5_0_SUPPORT
	bool bIsAvailableCS = true;
	int iNumCommonFBs = __FB_COUNT_CS;
	int iNumShadowFBs = __SM_COUNT_CS;
#else 
	bool bIsAvailableCS = false;
	int iNumCommonFBs = __FB_COUNT_PS;
	int iNumShadowFBs = __SM_COUNT_PS;
#endif

	MakeFBDescriptions(bIsAvailableCS,
		gpuResourcesFB_DESCs,
		NULL,
		gpuResourceMERGE_PS_DESCs,
		gpuResourceMERGE_CS_DESCs,
		gpuResourcesSHADOWMAP_DESCs,
		pCMergeIObject->GetObjectID());

	bool bInitGen = false;
	for (int i = 0; i < iNumCommonFBs; i++)
	{
		if (!pCGpuManager->GetGpuResourceArchive(&gpuResourcesFB_DESCs[i], &gpuResourcesFB_ARCHs[i]))
		{
			pCGpuManager->GenerateGpuResource(&gpuResourcesFB_DESCs[i], pCMergeIObject, &gpuResourcesFB_ARCHs[i], NULL);
			bInitGen = true;
		}
	}

	for (int i = 0; i < iNumShadowFBs; i++)
	{
		if (!pCGpuManager->GetGpuResourceArchive(&gpuResourcesSHADOWMAP_DESCs[i], &gpuResourcesSHADOWMAP_ARCHs[i]))
		{
			pCGpuManager->GenerateGpuResource(&gpuResourcesSHADOWMAP_DESCs[i], pCMergeIObject, &gpuResourcesSHADOWMAP_ARCHs[i], NULL);
			bInitGen = true;
		}
	}

#ifdef __CS_5_0_SUPPORT
	for (int i = 0; i < __EXRWB_COUNT; i++)
	{
		if (!pCGpuManager->GetGpuResourceArchive(&gpuResourceMERGE_CS_DESCs[i], &gpuResourceMERGE_CS_ARCHs[i]))
		{
			pCGpuManager->GenerateGpuResource(&gpuResourceMERGE_CS_DESCs[i], pCMergeIObject, &gpuResourceMERGE_CS_ARCHs[i], NULL);
			bInitGen = true;
		}
	}
#else
	for (int i = 0; i < __EXFB_COUNT; i++)
	{
		if (!pCGpuManager->GetGpuResourceArchive(&gpuResourceMERGE_PS_DESCs[i], &gpuResourceMERGE_PS_ARCHs[i]))
		{
			pCGpuManager->GenerateGpuResource(&gpuResourceMERGE_PS_DESCs[i], pCMergeIObject, &gpuResourceMERGE_PS_ARCHs[i], NULL);
			bInitGen = true;
		}
	}
#endif

	vmint2 i2SizeScreenCurrent, i2SizeScreenOld = vmint2(0, 0);
	pCMergeIObject->GetFrameBufferInfo(&i2SizeScreenCurrent);
	if (bInitGen)
		pCMergeIObject->RegisterCustomParameter(("_int2_PreviousScreenSize"), i2SizeScreenCurrent);
	pCMergeIObject->GetCustomParameter(("_int2_PreviousScreenSize"), data_type::dtype<vmint2>(), &i2SizeScreenOld);
	if (i2SizeScreenCurrent.x != i2SizeScreenOld.x || i2SizeScreenCurrent.y != i2SizeScreenOld.y)
	{
		pCGpuManager->ReleaseGpuResourcesRelatedObject(pCMergeIObject->GetObjectID());	// System Out 포함 //

		for (int i = 0; i < iNumCommonFBs; i++)
			pCGpuManager->GenerateGpuResource(&gpuResourcesFB_DESCs[i], pCMergeIObject, &gpuResourcesFB_ARCHs[i], NULL);

		for (int i = 0; i < iNumShadowFBs; i++)
			pCGpuManager->GenerateGpuResource(&gpuResourcesSHADOWMAP_DESCs[i], pCMergeIObject, &gpuResourcesSHADOWMAP_ARCHs[i], NULL);

#ifdef __CS_5_0_SUPPORT
		for (int i = 0; i < __EXRWB_COUNT; i++)
			pCGpuManager->GenerateGpuResource(&gpuResourceMERGE_CS_DESCs[i], pCMergeIObject, &gpuResourceMERGE_CS_ARCHs[i], NULL);
#else
		for (int i = 0; i < __EXFB_COUNT; i++)
			pCGpuManager->GenerateGpuResource(&gpuResourceMERGE_PS_DESCs[i], pCMergeIObject, &gpuResourceMERGE_PS_ARCHs[i], NULL);
#endif

		pCMergeIObject->RegisterCustomParameter(("_int2_PreviousScreenSize"), i2SizeScreenCurrent);
	}
	pCMergeIObject->RegisterCustomParameter(("_int2_PreviousScreenSize"), i2SizeScreenCurrent);

	int iCountMerging = 0;
	pCMergeIObject->GetCustomParameter(("_int_CountMerging"), data_type::dtype<int>(), &iCountMerging);
#pragma endregion // IOBJECT GPU

#ifdef __CS_5_0_SUPPORT
	vector<void*>* pvtr_Layers_UAV = &gpuResourceMERGE_CS_ARCHs[__EXRWB_UAV_LAYERS].vtrPtrs;
	vector<void*>* pvtr_Layers_SRV = &gpuResourceMERGE_CS_ARCHs[__EXRWB_SRV_LAYERS].vtrPtrs;
	ID3D11UnorderedAccessView* pUAV_Merge_PingpongCS[2] = { (ID3D11UnorderedAccessView*)pvtr_Layers_UAV->at(0), (ID3D11UnorderedAccessView*)pvtr_Layers_UAV->at(1) };
	ID3D11ShaderResourceView* pSRV_Merge_PingpongCS[2] = { (ID3D11ShaderResourceView*)pvtr_Layers_SRV->at(0), (ID3D11ShaderResourceView*)pvtr_Layers_SRV->at(1) };
#else
	vector<void*>* pvtr_RGBA_RTV = &gpuResourceMERGE_PS_ARCHs[__EXFB_RTV_RGBA_LAYERS].vtrPtrs;
	vector<void*>* pvtr_RGBA_SRV = &gpuResourceMERGE_PS_ARCHs[__EXFB_SRV_RGBA_LAYERS].vtrPtrs;
	vector<void*>* pvtr_DepthCS_RTV = &gpuResourceMERGE_PS_ARCHs[__EXFB_RTV_DEPTHCS_LAYERS].vtrPtrs;
	vector<void*>* pvtr_DepthCS_SRV = &gpuResourceMERGE_PS_ARCHs[__EXFB_SRV_DEPTHCS_LAYERS].vtrPtrs;
	ID3D11RenderTargetView* pRTV_Merge_RGBA_PingpongPS[2] = { (ID3D11RenderTargetView*)pvtr_RGBA_RTV->at(0), (ID3D11RenderTargetView*)pvtr_RGBA_RTV->at(1) };
	ID3D11ShaderResourceView* pSRV_Merge_RGBA_PingpongPS[2] = { (ID3D11ShaderResourceView*)pvtr_RGBA_SRV->at(0), (ID3D11ShaderResourceView*)pvtr_RGBA_SRV->at(1) };
	ID3D11RenderTargetView* pRTV_Merge_DepthCS_PingpongPS[2] = { (ID3D11RenderTargetView*)pvtr_DepthCS_RTV->at(0), (ID3D11RenderTargetView*)pvtr_DepthCS_RTV->at(1) };
	ID3D11ShaderResourceView* pSRV_Merge_DepthCS_PingpongPS[2] = { (ID3D11ShaderResourceView*)pvtr_DepthCS_SRV->at(0), (ID3D11ShaderResourceView*)pvtr_DepthCS_SRV->at(1) };
#endif

#pragma region // Presetting of VxObject
	int iLastRenderVolumeID = (vtrInputVolumes.at(vtrInputVolumes.size() - 1))->GetObjectID();
	fncont_getparamvalue<int>(_fncontainer, &iLastRenderVolumeID, ("_int_LastRenderVolumeID"));

	map<int, VmVObjectVolume*> mapVolumes;
	for (int i = 0; i < (int)vtrInputVolumes.size(); i++)
	{
		VmVObjectVolume* pCVolume = (VmVObjectVolume*)vtrInputVolumes.at(i);
		mapVolumes.insert(pair<int, VmVObjectVolume*>(pCVolume->GetObjectID(), pCVolume));
	}

	map<int, VmTObject*> mapTObjects;
	for (int i = 0; i < (int)vtrInputOTFs.size(); i++)
	{
		VmTObject* pCTObject = (VmTObject*)vtrInputOTFs.at(i);
		mapTObjects.insert(pair<int, VmTObject*>(pCTObject->GetObjectID(), pCTObject));
	}

	int* pMainVolumeIDs = NULL;	// Rendering Volume List
	size_t bytes_pMainVolumeIDs;
	if (!pCLObjectForParameters->LoadBufferPtr("_vlist_INT_MainVolumeIDs", (void**)&pMainVolumeIDs, bytes_pMainVolumeIDs))
	{
		VMERRORMESSAGE("GPU DVR! - ERROR 00");
		return false;
	}
	int num_pMainVolumeIDs = voo::get_num_from_bytes<int>(bytes_pMainVolumeIDs);

	vector<int> vtrOrderedMainVolumeIDs;
	if (!bIsShadowStage)
	{
		bool bIsValidList = false;
		for (int i = 0; i < num_pMainVolumeIDs; i++)
		{
			int iVolumeID = pMainVolumeIDs[i];
			auto itrVolume = mapVolumes.find(iVolumeID);
			if (itrVolume == mapVolumes.end())
			{
				VMERRORMESSAGE("GPU DVR! - INVALID VOLUME ID");
				return false;
			}
			if (iVolumeID == iLastRenderVolumeID)
			{
				bIsValidList = true;
			}
			else
			{
				vtrOrderedMainVolumeIDs.push_back(iVolumeID);
			}
		}
		if (!bIsValidList)
		{
			VMERRORMESSAGE("GPU DVR! - ERROR 01");
			return false;
		}
	}
	vtrOrderedMainVolumeIDs.push_back(iLastRenderVolumeID);
#pragma endregion // Presetting of VxObject

	// Backup Previous Render Target //
	ID3D11RenderTargetView* pdxRTVOld = NULL;
	ID3D11DepthStencilView* pdxDSVOld = NULL;
	pdx11DeviceImmContext->OMGetRenderTargets(1, &pdxRTVOld, &pdxDSVOld);

	uint uiClearVlaues[4] = { 0 };
	float fClearColor[4] = { 0 };
	float fClearDepth[4] = { FLT_MAX };
	bool bIsFirstRender = false;
	fncont_getparamvalue<bool>(_fncontainer, &bIsFirstRender, ("_bool_IsFirstRenderer"));
	if (bIsFirstRender || iLevelVR == 2)
	{
		// Clear Merge Buffers //
#ifdef __CS_5_0_SUPPORT
		for (int i = 0; i < 2; i++)
			pdx11DeviceImmContext->ClearUnorderedAccessViewUint(pUAV_Merge_PingpongCS[i], uiClearVlaues);
#else
		for (int i = 0; i < 2; i++)
		{
			pdx11DeviceImmContext->ClearRenderTargetView(pRTV_Merge_RGBA_PingpongPS[i], fClearColor);
			pdx11DeviceImmContext->ClearRenderTargetView(pRTV_Merge_DepthCS_PingpongPS[i], fClearDepth);
		}
#endif

		iCountMerging = 0;
	}

	// Clear RTT //
	// Clear ShadowMap Depth //
#ifdef __CS_5_0_SUPPORT
	for (int i = 0; i < NUM_TEXRT_LAYERS; i++)
	{
		pdx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gpuResourcesFB_ARCHs[__FB_UAV_RENDEROUT].vtrPtrs.at(i), uiClearVlaues);
		pdx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gpuResourcesFB_ARCHs[__FB_UAV_DEPTHCS].vtrPtrs.at(i), uiClearVlaues);
	}
	if (bIsShadowStage)
		pdx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gpuResourcesSHADOWMAP_ARCHs[__SM_UAV_DEPTH_SHADOWMAP].vtrPtrs.at(0), uiClearVlaues);
	else if (bIsShadowRenderer)
		pdx11DeviceImmContext->CSSetShaderResources(4, 1, (ID3D11ShaderResourceView**)&gpuResourcesSHADOWMAP_ARCHs[__SM_SRV_DEPTH_SHADOWMAP].vtrPtrs.at(0));
#else
	for (int i = 0; i < NUM_TEXRT_LAYERS; i++)
	{
		pdx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_RENDEROUT].vtrPtrs.at(i), fClearColor);
		pdx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_DEPTHCS].vtrPtrs.at(i), fClearDepth);
	}
	if (bIsShadowStage)
		pdx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gpuResourcesSHADOWMAP_ARCHs[__SM_RTV_DEPTH_SHADOWMAP].vtrPtrs.at(0), fClearColor);
	else if (bIsShadowRenderer)
		pdx11DeviceImmContext->PSSetShaderResources(4, 1, (ID3D11ShaderResourceView**)&gpuResourcesSHADOWMAP_ARCHs[__SM_SRV_DEPTH_SHADOWMAP].vtrPtrs.at(0));
#endif

	// View List-Up //
	ID3D11ShaderResourceView* pdx11SRV_RenderOuts[NUM_TEXRT_LAYERS] = { NULL };
	ID3D11ShaderResourceView* pdx11SRV_DepthCSs[NUM_TEXRT_LAYERS] = { NULL };
#ifdef __CS_5_0_SUPPORT
	UINT UAVInitialCounts = 0;
	ID3D11UnorderedAccessView* pdx11UAV_RenderOuts[NUM_TEXRT_LAYERS] = { NULL };
	ID3D11UnorderedAccessView* pdx11UAV_DepthCSs[NUM_TEXRT_LAYERS] = { NULL };
	for (int i = 0; i < NUM_TEXRT_LAYERS; i++)
	{
		pdx11SRV_RenderOuts[i] = (ID3D11ShaderResourceView*)gpuResourcesFB_ARCHs[__FB_SRV_RENDEROUT].vtrPtrs.at(i);
		pdx11SRV_DepthCSs[i] = (ID3D11ShaderResourceView*)gpuResourcesFB_ARCHs[__FB_SRV_DEPTHCS].vtrPtrs.at(i);

		pdx11UAV_RenderOuts[i] = (ID3D11UnorderedAccessView*)gpuResourcesFB_ARCHs[__FB_UAV_RENDEROUT].vtrPtrs.at(i);
		pdx11UAV_DepthCSs[i] = (ID3D11UnorderedAccessView*)gpuResourcesFB_ARCHs[__FB_UAV_DEPTHCS].vtrPtrs.at(i);
	}
	ID3D11UnorderedAccessView* pdx11UAV_4NULLs[NUM_TEXRT_LAYERS] = { NULL };
	ID3D11UnorderedAccessView* pdx11UAV_NULL = NULL;
#else
	pdx11DeviceImmContext->OMSetDepthStencilState(pdx11CommonParams->pdx11DSStates[__DSState_DEPTHENABLE_ALWAYS], 0);

	// View Port Setting //
	D3D11_VIEWPORT dx11ViewPort;
	dx11ViewPort.Width = (float)i2SizeScreenCurrent.x;
	dx11ViewPort.Height = (float)i2SizeScreenCurrent.y;
	dx11ViewPort.MinDepth = 0;
	dx11ViewPort.MaxDepth = 1.0f;
	dx11ViewPort.TopLeftX = 0;
	dx11ViewPort.TopLeftY = 0;
	pdx11DeviceImmContext->RSSetViewports(1, &dx11ViewPort);

	ID3D11RenderTargetView* pdx11RTV_RenderOuts[NUM_TEXRT_LAYERS] = { NULL };
	ID3D11RenderTargetView* pdx11RTV_DepthCSs[NUM_TEXRT_LAYERS] = { NULL };
	for (int i = 0; i < NUM_TEXRT_LAYERS; i++)
	{
		pdx11SRV_RenderOuts[i] = (ID3D11ShaderResourceView*)gpuResourcesFB_ARCHs[__FB_SRV_RENDEROUT].vtrPtrs.at(i);
		pdx11SRV_DepthCSs[i] = (ID3D11ShaderResourceView*)gpuResourcesFB_ARCHs[__FB_SRV_DEPTHCS].vtrPtrs.at(i);

		pdx11RTV_RenderOuts[i] = (ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_RENDEROUT].vtrPtrs.at(i);
		pdx11RTV_DepthCSs[i] = (ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_DEPTHCS].vtrPtrs.at(i);
	}
	ID3D11RenderTargetView* pdx11RTV_4NULLs[NUM_TEXRT_LAYERS] = { NULL };
	ID3D11RenderTargetView* pdx11RTV_2NULLs[2] = { NULL };
#endif
	ID3D11ShaderResourceView* pdx11SRV_4NULLs[NUM_TEXRT_LAYERS] = { NULL };
	ID3D11ShaderResourceView* pdx11SRV_2NULLs[2] = { NULL };

#pragma region // Camera & Light & Shadow Setting
	uint uiNumGridX = (uint)ceil(i2SizeScreenCurrent.x / (float)__BLOCKSIZE);
	uint uiNumGridY = (uint)ceil(i2SizeScreenCurrent.y / (float)__BLOCKSIZE);

	VmCObject* pCCObject = pCMergeIObject->GetCameraObject();
	vmfloat3 f3PosEyeWS, f3VecViewWS;
	pCCObject->GetCameraExtStatef(&f3PosEyeWS, &f3VecViewWS, NULL);
	double dFarPlaneLength;
	pCCObject->GetCameraIntState(NULL, NULL, &dFarPlaneLength, NULL);

	CB_VrCameraState cbVrCamState;
	HDx11ComputeConstantBufferVrCamera(&cbVrCamState, pCCObject, i2SizeScreenCurrent, &_fncontainer->vmparams);

	// Box Range in the Parallel Light Beam Condition
	vmfloat3 pos_minWS = vmfloat3(FLT_MAX, FLT_MAX, FLT_MAX);
	vmfloat3 pos_maxWS = vmfloat3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	//if (!(cbVrCamState.iFlags & 0x2) && bIsShadowRenderer)
	vmint2 i2AaBbMin(INT_MAX, INT_MAX), i2AaBbMax(0, 0);
	vmmat44 dmatWS2CS, dmatCS2PS, dmatPS2SS;
	pCCObject->GetMatrixWStoSS(&dmatWS2CS, &dmatCS2PS, &dmatPS2SS);
	vmmat44f matWS2SS = dmatWS2CS * dmatCS2PS * dmatPS2SS;
	if (bIsShadowRenderer || bIsFastQualityMode) // iLevelVR == 2 || 
	{
		for (int i = 0; i < (int)vtrOrderedMainVolumeIDs.size(); i++)
		{
			int iVolumeID = vtrOrderedMainVolumeIDs.at(i);

			auto itrVolume = mapVolumes.find(iVolumeID);
			if (itrVolume == mapVolumes.end())
			{
				VMERRORMESSAGE("NO Volume ID - Shadow");
				continue;
			}

			VmVObjectVolume* pCMainVolume = itrVolume->second;
			AaBbMinMax stAaBbOS;
			pCMainVolume->GetOrthoBoundingBox(stAaBbOS);
			vmfloat3 f3PosOrthoBoxesVS[8];
			f3PosOrthoBoxesVS[0] = vmfloat3(stAaBbOS.pos_min.x, stAaBbOS.pos_min.y, stAaBbOS.pos_max.z);
			f3PosOrthoBoxesVS[1] = vmfloat3(stAaBbOS.pos_max.x, stAaBbOS.pos_min.y, stAaBbOS.pos_max.z);
			f3PosOrthoBoxesVS[2] = vmfloat3(stAaBbOS.pos_min.x, stAaBbOS.pos_min.y, stAaBbOS.pos_min.z);
			f3PosOrthoBoxesVS[3] = vmfloat3(stAaBbOS.pos_max.x, stAaBbOS.pos_min.y, stAaBbOS.pos_min.z);
			f3PosOrthoBoxesVS[4] = vmfloat3(stAaBbOS.pos_min.x, stAaBbOS.pos_max.y, stAaBbOS.pos_max.z);
			f3PosOrthoBoxesVS[5] = vmfloat3(stAaBbOS.pos_max.x, stAaBbOS.pos_max.y, stAaBbOS.pos_max.z);
			f3PosOrthoBoxesVS[6] = vmfloat3(stAaBbOS.pos_min.x, stAaBbOS.pos_max.y, stAaBbOS.pos_min.z);
			f3PosOrthoBoxesVS[7] = vmfloat3(stAaBbOS.pos_max.x, stAaBbOS.pos_max.y, stAaBbOS.pos_min.z);
			for (int i = 0; i < 8; i++)
			{
				vmfloat3 f3PosOrthoBoxWS;
				fTransformPoint(&f3PosOrthoBoxWS, &f3PosOrthoBoxesVS[i], &pCMainVolume->GetMatrixOS2WSf());
				pos_maxWS.x = max(pos_maxWS.x, f3PosOrthoBoxWS.x);
				pos_maxWS.y = max(pos_maxWS.y, f3PosOrthoBoxWS.y);
				pos_maxWS.z = max(pos_maxWS.z, f3PosOrthoBoxWS.z);
				pos_minWS.x = min(pos_minWS.x, f3PosOrthoBoxWS.x);
				pos_minWS.y = min(pos_minWS.y, f3PosOrthoBoxWS.y);
				pos_minWS.z = min(pos_minWS.z, f3PosOrthoBoxWS.z);

				vmfloat3 f3PosOrthoBoxSS;
				fTransformPoint(&f3PosOrthoBoxSS, &f3PosOrthoBoxWS, &matWS2SS);
				vmint2 i2PosSS = vmint2((int)f3PosOrthoBoxSS.x, (int)f3PosOrthoBoxSS.y);
				i2AaBbMin.x = min(i2AaBbMin.x, i2PosSS.x);
				i2AaBbMin.y = min(i2AaBbMin.y, i2PosSS.y);
				i2AaBbMax.x = max(i2AaBbMax.x, i2PosSS.x);
				i2AaBbMax.y = max(i2AaBbMax.y, i2PosSS.y);
			}
		}
	}

#pragma region // Compute WS2RS for Fast Quality Mode : 
	vmint2 i2RenderingSize;
	vmfloat2 f2ReductionRatio;
	if (bIsFastQualityMode) // iLevelVR == 2 || 
	{
		i2AaBbMin.x = min(max(i2AaBbMin.x, 0), i2SizeScreenCurrent.x - 1);
		i2AaBbMin.y = min(max(i2AaBbMin.y, 0), i2SizeScreenCurrent.y - 1);
		i2AaBbMax.x = max(min(i2AaBbMax.x, i2SizeScreenCurrent.x - 1), 0);
		i2AaBbMax.y = max(min(i2AaBbMax.y, i2SizeScreenCurrent.y - 1), 0);

		int iCrtSize = 300;
		fncont_getparamvalue<int>(_fncontainer, &iCrtSize, ("_int_LengthFastModeMax"));

		vmint2 i2RenderingBufferSize = vmint2(iCrtSize, iCrtSize);
		i2RenderingBufferSize = vmint2(min(i2SizeScreenCurrent.x, i2RenderingBufferSize.x), min(i2SizeScreenCurrent.y, i2RenderingBufferSize.y));

		vmint2 i2ValidRegion = i2AaBbMax - i2AaBbMin;

		float ratio = max(1.0f, max((float)i2ValidRegion.x / (i2RenderingBufferSize.x - 1), (float)i2ValidRegion.y / (i2RenderingBufferSize.y - 1)));
		f2ReductionRatio = vmfloat2(ratio, ratio);
		i2RenderingSize = vmint2((int)(i2ValidRegion.x / ratio), (int)(i2ValidRegion.y / ratio));

		vmmat44f matRS2SS, matRS2WS;
		matRS2SS[0][0] = f2ReductionRatio.x;
		matRS2SS[1][1] = f2ReductionRatio.y;
		matRS2SS[0][3] = (float)i2AaBbMin.x;
		matRS2SS[1][3] = (float)i2AaBbMin.y;
		vmmat44 dmatSS2PS, dmatPS2CS, dmatCS2WS;
		pCCObject->GetMatrixSStoWS(&dmatSS2PS, &dmatPS2CS, &dmatCS2WS);
		vmmat44f matSS2WS = dmatSS2PS * dmatPS2CS * dmatCS2WS;
		matRS2WS = matRS2SS * matSS2WS;


		cbVrCamState.dxmatSS2WS = *(D3DXMATRIX*)&matSS2WS;
		cbVrCamState.dxmatRS2SS = *(D3DXMATRIX*)&matRS2SS;
		/////D3DXMatrixTranspose(&cbVrCamState.matSS2WS, &cbVrCamState.matSS2WS);
		/////D3DXMatrixTranspose(&cbVrCamState.matRS2SS, &cbVrCamState.matRS2SS);

		if (iLevelVR == 1)
		{
			cbVrCamState.uiScreenAaBbMinX = i2AaBbMin.x;
			cbVrCamState.uiScreenAaBbMinY = i2AaBbMin.y;
			cbVrCamState.uiScreenAaBbMaxX = i2AaBbMax.x;
			cbVrCamState.uiScreenAaBbMaxY = i2AaBbMax.y;
		}
		else
		{
			uiNumGridX = (uint)ceil(i2RenderingSize.x / (float)__BLOCKSIZE);
			uiNumGridY = (uint)ceil(i2RenderingSize.y / (float)__BLOCKSIZE);


			//cbVrCamState.uiScreenAaBbMinX = i2AaBbMin.x;
			//cbVrCamState.uiScreenAaBbMinY = i2AaBbMin.y;
			//cbVrCamState.uiScreenAaBbMaxX = i2AaBbMax.x;
			//cbVrCamState.uiScreenAaBbMaxY = i2AaBbMax.y;
			cbVrCamState.uiScreenAaBbMinX = 0;
			cbVrCamState.uiScreenAaBbMinY = 0;
			cbVrCamState.uiScreenAaBbMaxX = (uint)i2SizeScreenCurrent.x;
			cbVrCamState.uiScreenAaBbMaxY = (uint)i2SizeScreenCurrent.y;
		}
		cbVrCamState.uiRenderingSizeX = (uint)i2RenderingSize.x;
		cbVrCamState.uiRenderingSizeY = (uint)i2RenderingSize.y;
	}
	else
	{
		D3DXMatrixIdentity(&cbVrCamState.dxmatRS2SS);

		cbVrCamState.uiScreenAaBbMinX = 0;
		cbVrCamState.uiScreenAaBbMinY = 0;
		cbVrCamState.uiScreenAaBbMaxX = (uint)i2SizeScreenCurrent.x;
		cbVrCamState.uiScreenAaBbMaxY = (uint)i2SizeScreenCurrent.y;

		cbVrCamState.uiRenderingSizeX = (uint)i2SizeScreenCurrent.x;
		cbVrCamState.uiRenderingSizeY = (uint)i2SizeScreenCurrent.y;
	}

#pragma endregion

	CB_VrShadowMap cbShadowMap;
	if (bIsShadowStage)
	{
		HDx11ComputeConstantBufferVrShadowMap(&cbShadowMap, &cbVrCamState, pos_minWS, pos_maxWS, &_fncontainer->vmparams);
	}
	else if (bIsShadowRenderer)
	{
		CB_VrCameraState cbVrCamStateShadowTemp = cbVrCamState;
		HDx11ComputeConstantBufferVrShadowMap(&cbShadowMap, &cbVrCamStateShadowTemp, pos_minWS, pos_maxWS, &_fncontainer->vmparams);
	}

	if (bIsShadowRenderer)
	{
		D3D11_MAPPED_SUBRESOURCE mappedResShadowMap;
		pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_SHADOWMAP], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResShadowMap);
		CB_VrShadowMap* pCBParamSurfaceEffect = (CB_VrShadowMap*)mappedResShadowMap.pData;
		memcpy(pCBParamSurfaceEffect, &cbShadowMap, sizeof(CB_VrShadowMap));
		pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_SHADOWMAP], 0);
#ifdef __CS_5_0_SUPPORT
		pdx11DeviceImmContext->CSSetConstantBuffers(7, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_SHADOWMAP]);
#else
		pdx11DeviceImmContext->PSSetConstantBuffers(7, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_SHADOWMAP]);
#endif
	}

	D3D11_MAPPED_SUBRESOURCE mappedResCamState;
	pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_CAMSTATE], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResCamState);
	CB_VrCameraState* pCBParamCamState = (CB_VrCameraState*)mappedResCamState.pData;
	memcpy(pCBParamCamState, &cbVrCamState, sizeof(CB_VrCameraState));
	int iTestIndex = 0;
	fncont_getparamvalue<int>(_fncontainer, &iTestIndex, ("_int_Index"));
	pCBParamCamState->iFlags |= iTestIndex << 10;
	pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_CAMSTATE], 0);
#ifdef __CS_5_0_SUPPORT
	pdx11DeviceImmContext->CSSetConstantBuffers(1, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_CAMSTATE]);
#else
	pdx11DeviceImmContext->PSSetConstantBuffers(1, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_CAMSTATE]);
#endif
#pragma endregion // Light & Shadow Setting

	// Initial Setting of Frame Buffers //
	QueryPerformanceCounter(&lIntCntStart);

#pragma region // Common Shader Setting : Proxy Setting, Sampler Setting
	ID3D11RenderTargetView* pdx11RTVs[2] = { (ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_RENDEROUT].vtrPtrs.at(0)
		, (ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_DEPTHCS].vtrPtrs.at(0) };

	ID3D11RenderTargetView* pdx11RTVs_NULL[2] = { NULL };
	pdx11DeviceImmContext->OMSetRenderTargets(2, pdx11RTVs_NULL, NULL);

	uint uiStrideSizeInput = sizeof(vmfloat3);
	uint uiOffset = 0;

	pdx11DeviceImmContext->IASetVertexBuffers(0, 1, (ID3D11Buffer**)&pdx11BufProxyRect, &uiStrideSizeInput, &uiOffset);
	pdx11DeviceImmContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pdx11DeviceImmContext->IASetInputLayout(pdx11IL_ProxyRect);
	pdx11DeviceImmContext->RSSetState(pdx11CommonParams->pdx11RStates[__RState_SOLID_NONE]);
	pdx11DeviceImmContext->VSSetShader(pdx11VS_ProxyRect, NULL, 0);

#ifdef __CS_5_0_SUPPORT
	pdx11DeviceImmContext->CSSetSamplers(0, 1, &pdx11CommonParams->pdx11SStates[__SState_LINEAR_ZEROBORDER]);
	pdx11DeviceImmContext->CSSetSamplers(1, 1, &pdx11CommonParams->pdx11SStates[__SState_POINT_ZEROBORDER]);
	pdx11DeviceImmContext->CSSetSamplers(2, 1, &pdx11CommonParams->pdx11SStates[__SState_LINEAR_CLAMP]);
	pdx11DeviceImmContext->CSSetSamplers(3, 1, &pdx11CommonParams->pdx11SStates[__SState_POINT_CLAMP]);
#else
	pdx11DeviceImmContext->PSSetSamplers(0, 1, &pdx11CommonParams->pdx11SStates[__SState_LINEAR_ZEROBORDER]);
	pdx11DeviceImmContext->PSSetSamplers(1, 1, &pdx11CommonParams->pdx11SStates[__SState_POINT_ZEROBORDER]);
	pdx11DeviceImmContext->PSSetSamplers(2, 1, &pdx11CommonParams->pdx11SStates[__SState_LINEAR_CLAMP]);
	pdx11DeviceImmContext->PSSetSamplers(3, 1, &pdx11CommonParams->pdx11SStates[__SState_POINT_CLAMP]);
#endif

	// Proxy Setting //
	auto itrVolume = mapVolumes.find(iLastRenderVolumeID);
	vmfloat3 f3PitchMain = itrVolume->second->GetVolumeData()->vox_pitch;
#ifdef __CS_5_0_SUPPORT
	D3D11_MAPPED_SUBRESOURCE mappedProxyState;
	pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_PROXY], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedProxyState);
	CB_SrProxy* pCBProxyParam = (CB_SrProxy*)mappedProxyState.pData;
	pCBProxyParam->fVZThickness = min(min(f3PitchMain.x, f3PitchMain.y), f3PitchMain.z);
	pCBProxyParam->uiGridOffsetX = cbVrCamState.uiGridOffsetX;
	pCBProxyParam->uiScreenSizeX = cbVrCamState.uiScreenSizeX;
	pCBProxyParam->uiScreenSizeY = cbVrCamState.uiScreenSizeY;
	pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_PROXY], 0);
	pdx11DeviceImmContext->CSSetConstantBuffers(10, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_PROXY]);
#else
	D3DXMATRIX dxmatWS2CS, dxmatWS2PS;
	D3DXMatrixLookAtRH(&dxmatWS2CS, &D3DXVECTOR3(0, 0, 1.f), &D3DXVECTOR3(0, 0, 0), &D3DXVECTOR3(0, 1.f, 0));
	D3DXMatrixOrthoRH(&dxmatWS2PS, 1.f, 1.f, 0.f, 2.f);
	D3DXMATRIX dxmatOS2PS = dxmatWS2CS * dxmatWS2PS;
	/////D3DXMatrixTranspose(&dxmatOS2PS, &dxmatOS2PS);

	D3D11_MAPPED_SUBRESOURCE mappedProxyState;
	pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_PROXY], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedProxyState);
	CB_SrProxy* pCBProxyParam = (CB_SrProxy*)mappedProxyState.pData;
	pCBProxyParam->fVZThickness = min(min(f3PitchMain.x, f3PitchMain.y), f3PitchMain.z);
	pCBProxyParam->uiGridOffsetX = cbVrCamState.uiGridOffsetX;
	pCBProxyParam->uiScreenSizeX = cbVrCamState.uiScreenSizeX;
	pCBProxyParam->uiScreenSizeY = cbVrCamState.uiScreenSizeY;
	pCBProxyParam->dxmatOS2PS = dxmatOS2PS;

	pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_PROXY], 0);
	pdx11DeviceImmContext->VSSetConstantBuffers(10, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_PROXY]);
	pdx11DeviceImmContext->PSSetConstantBuffers(10, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_PROXY]);
#endif
#pragma endregion

	int iLastRenderedVr = -1;
	pCOutputIObject->GetCustomParameter("_int_LastRenderedVrLevel", data_type::dtype<int>(), &iLastRenderedVr);
	if (iLevelVR == 2 && bReusePrevRendering && iLastRenderedVr == 2)
		goto MIXING_SR_VR;
	pCOutputIObject->RegisterCustomParameter("_int_LastRenderedVrLevel", iLevelVR);

	int iNumMainVolumes = (int)vtrOrderedMainVolumeIDs.size();
	int iCountSingleRendering = 0;
	int iCountMergingInVR = 0;
	//int iCountMergingInSR = iCountMerging;
	bool bIsCalledOnTheFlyMixing = false;

	for (int i = 0; i < iNumMainVolumes; i++)
	{
		int iVolumeID = vtrOrderedMainVolumeIDs.at(i);

#pragma region // Volume General Parameters
		VmVObjectVolume* pCVObjectVolume = NULL;

		// Volumes //
		auto itrVolume = mapVolumes.find(iVolumeID);
		if (itrVolume == mapVolumes.end())
		{
			VMERRORMESSAGE("NO Volume ID");
			continue;
		}
		pCVObjectVolume = itrVolume->second;

		bool bIsVisible = true;
		pCLObjectForParameters->GetDstObjValue(iVolumeID, ("_bool_IsVisible"), &bIsVisible);
		pCLObjectForParameters->GetDstObjValue(iVolumeID, ("_bool_TestVisible"), &bIsVisible);

		bool bIsNoValidObject = bIsNoValidObjectInitValue;
		if (!bIsVisible)
			bIsNoValidObject = true;
		if (bIsNoValidObject)
			continue;

		int iRendererType = 0;
		pCLObjectForParameters->GetDstObjValue(iVolumeID, ("_int_RendererType"), &iRendererType);
		bool bIsXRayMode = false;
		if (iRendererType >= __RM_RAYMAX)
			bIsXRayMode = true;
		if (bIsShowBlock)
			iRendererType = __RM_SURFACEEFFECT;

		// TEST //
		int iDensity0 = 0, iDensity1 = 0, iIsoValue = 0;
		double dKernelSize = 0, dThresholdGradSqLength = 0, dThresholdLaplacian = 0;
		if (iRendererType == __RM_SURFACEEFFECT && iTestValue > 0)
		{
			pCLObjectForParameters->GetDstObjValue(iVolumeID, ("_int_Density0"), &iDensity0);
			pCLObjectForParameters->GetDstObjValue(iVolumeID, ("_int_Density1"), &iDensity1);
			pCLObjectForParameters->GetDstObjValue(iVolumeID, ("_int_IsoValueBoundary"), &iIsoValue);
			pCLObjectForParameters->GetDstObjValue(iVolumeID, ("_double_KernelSize"), &dKernelSize);
			pCLObjectForParameters->GetDstObjValue(iVolumeID, ("_double_ThresholdGradSqLength"), &dThresholdGradSqLength);
			pCLObjectForParameters->GetDstObjValue(iVolumeID, ("_double_ThresholdLaplacian"), &dThresholdLaplacian);

			//iRendererType = 6;
			if (iTestValue == 1 || iTestValue == 3)
				bIsShowBlock = true;
			iRendererType = __RM_SURFACEEFFECT;
		}

		bool bForceToSkipBlockUpdateFromVolume = false;
		pCLObjectForParameters->GetDstObjValue(iVolumeID, ("_bool_ForceToSkipBlockUpdate"), &bForceToSkipBlockUpdateFromVolume);

		int iLevelBlock = 1;
		pCLObjectForParameters->GetDstObjValue(iVolumeID, ("_int_BlockLevel"), &iLevelBlock);
		iLevelBlock = max(min(1, iLevelBlock), 0);

		bool bIsBrickChunk = false; // 추후에 Pore 용?! 으로 사용 가능성...
		pCLObjectForParameters->GetDstObjValue(iVolumeID, ("_bool_IsBrickChunk"), &bIsBrickChunk);

		int iMainTObjectID = 0;
		pCLObjectForParameters->GetDstObjValue(iVolumeID, ("_int_MainTObjectID"), &iMainTObjectID);

		int iMaskVolumeID = 0;
		pCLObjectForParameters->GetDstObjValue(iVolumeID, ("_int_MaskVolumeObjectID"), &iMaskVolumeID);

		// TEST 
		int iWindowingTObjectID = 0;
		pCLObjectForParameters->GetDstObjValue(iVolumeID, ("_int_WindowingTObjectID"), &iWindowingTObjectID);

#pragma endregion // Volume General Parameters

#pragma region // Mask Volume Custom Parameters
		VmVObjectVolume* pCVolumeMask = NULL;
		//pCLObjectForParameters->GetCustomDValue(iMaskVolumeID, (void**)&pmapDValueMaskVolume);
		//if (pmapDValueMaskVolume == NULL)
		//	pmapDValueMaskVolume = &mapDValueClear;

		int iSculptValue = -1;
		vector<double> vtrMaskOTFIDSeries;
		vector<double> vtrMaskOTFIDSeries_Visibilities;
		vector<double> vtrMaskOTFIDMap;
		if (iMaskVolumeID != 0 && (bValidateMaskVolume || iRendererType == __RM_SCULPTMASK))
		{
			auto itrMaskVolume = mapVolumes.find(iMaskVolumeID);
			if (itrMaskVolume == mapVolumes.end())
			{
				VMERRORMESSAGE("NO Mask Volume ID");
				continue;
			}

			pCVolumeMask = itrMaskVolume->second;
			if (pCVolumeMask == NULL)
				return false;

			pCLObjectForParameters->GetDstObjValue(iMaskVolumeID, ("_int_SculptingIndex"), &iSculptValue);
			pCLObjectForParameters->GetDstObjValue(iMaskVolumeID, ("_vlist_DOUBLE_MaskOTFIDs"), &vtrMaskOTFIDSeries);
			pCLObjectForParameters->GetDstObjValue(iMaskVolumeID, ("_vlist_DOUBLE_MaskOTFIDs_VisibilitySeries"), &vtrMaskOTFIDSeries_Visibilities);
			pCLObjectForParameters->GetDstObjValue(iMaskVolumeID, ("_vlist_DOUBLE_MaskOTFIndexMap"), &vtrMaskOTFIDMap);

			if (vtrMaskOTFIDSeries.size() != vtrMaskOTFIDMap.size())
				VMERRORMESSAGE("ERROR!!!!! _vlist_DOUBLE_MaskOTFIDs and _vlist_DOUBLE_MaskOTFIndexMap!!");
		}
		int iNumMaskOTFs = (int)vtrMaskOTFIDSeries.size();
		bool bIsUsedMaskOtfInRenderer = false;
		if (iNumMaskOTFs > 0)
		{
			bIsUsedMaskOtfInRenderer = true;
			// Check if Mask Volume Renderer is used or not.
			if (iNumMaskOTFs == 1 && vtrMaskOTFIDSeries.at(0) == iMainTObjectID)
				bIsUsedMaskOtfInRenderer = false;
		}

#pragma endregion // Mask Volume Custom Parameters

#pragma region // Main TObject Custom Parameters
		VmTObject* pCTObjectMain = NULL;

		auto itrTObject = mapTObjects.find(iMainTObjectID);
		if (itrTObject == mapTObjects.end())
		{
			VMERRORMESSAGE("NO TOBJECT ID");
			continue;
		}
		pCTObjectMain = itrTObject->second;

		bool bIsTfChanged = false;
		pCLObjectForParameters->GetDstObjValue(iMainTObjectID, ("_bool_IsTfChanged"), &bIsTfChanged);
		if (bInitGen || bForcedUpdateOtf)
			bIsTfChanged = true;

		bool bForceToSkipBlockUpdateFromOTF = false;
		pCLObjectForParameters->GetDstObjValue(iMainTObjectID, ("_bool_ForceToSkipBlockUpdate"), &bForceToSkipBlockUpdateFromOTF);

		bool bUpdateBricks = false;
		pCLObjectForParameters->GetDstObjValue(iMainTObjectID, ("_bool_UpdateBricks"), &bUpdateBricks);

		bool bForceToSkipBlockUpdate = bForceToSkipBlockUpdateFromOTF || bForceToSkipBlockUpdateFromVolume;

		if (bIsUsedMaskOtfInRenderer)
		{
			for (int j = 0; j < iNumMaskOTFs; j++)
			{
				int iMaskOTFID = (int)vtrMaskOTFIDSeries.at(j);
				bool bIsMaskTfChanged = false;
				pCLObjectForParameters->GetDstObjValue(iMaskOTFID, "_bool_IsTfChanged", &bIsMaskTfChanged);
				bIsTfChanged |= bIsMaskTfChanged;
			}
		}
#pragma endregion // Main TObject Custom Parameters

#pragma region // Volume Resource Setting 
		VolumeData* pVolumeData = pCVObjectVolume->GetVolumeData();
		GpuResource stGpuResourceVolumeTexture, stGpuResourceVolumeSRV;
		GpuResDescriptor stGpuVolumeResourceDesc;
		stGpuVolumeResourceDesc.sdk_type = GpuSdkTypeDX11;
		stGpuVolumeResourceDesc.dtype = pVolumeData->store_dtype;
		stGpuVolumeResourceDesc.src_obj_id = pCVObjectVolume->GetObjectID();
		if (bIsBrickChunk)
		{
			bool bPoreModify = false;
			pCLObjectForParameters->GetDstObjValue(iVolumeID, ("_bool_ModifyPoreSizeVolume"), &bPoreModify);
			if (bPoreModify)
				bUpdateBricks = true;

			stGpuVolumeResourceDesc.usage_specifier = ("TEXTURE3D_VOLUME_BRICKCHUNK");
			stGpuVolumeResourceDesc.misc = iLevelBlock;
			if (bUpdateBricks && !bForceToSkipBlockUpdate)
			{
				stGpuVolumeResourceDesc.res_type = ResourceTypeDX11RESOURCE;
				pCGpuManager->ReleaseGpuResource(&stGpuVolumeResourceDesc);
				stGpuVolumeResourceDesc.res_type = ResourceTypeDX11SRV;
				pCGpuManager->ReleaseGpuResource(&stGpuVolumeResourceDesc);
			}
		}
		else
		{
			stGpuVolumeResourceDesc.usage_specifier = ("TEXTURE3D_VOLUME_DEFAULT");
			if (iRendererType == __RM_SURFACECCF)
			{
				stGpuVolumeResourceDesc.usage_specifier = ("TEXTURE3D_VOLUME_DEFAULT_MASK");
			}
		}

		stGpuVolumeResourceDesc.res_type = ResourceTypeDX11RESOURCE;
		if (!pCGpuManager->GetGpuResourceArchive(&stGpuVolumeResourceDesc, &stGpuResourceVolumeTexture))
			pCGpuManager->GenerateGpuResource(&stGpuVolumeResourceDesc, pCVObjectVolume, &stGpuResourceVolumeTexture, pstLocalProgress);
		stGpuVolumeResourceDesc.res_type = ResourceTypeDX11SRV;
		if (!pCGpuManager->GetGpuResourceArchive(&stGpuVolumeResourceDesc, &stGpuResourceVolumeSRV))
			pCGpuManager->GenerateGpuResource(&stGpuVolumeResourceDesc, pCVObjectVolume, &stGpuResourceVolumeSRV, NULL);
#pragma endregion // Volume Resource Setting 

#pragma region // Mask Volume Resource Setting
		if (pCVolumeMask != NULL)
		{
			VolumeData* pstVolMaskArchive = pCVolumeMask->GetVolumeData();

			GpuResource stGpuResourceVolumeTextureForMask, stGpuResourceVolumeSRVForMask;
			GpuResDescriptor stGpuVolumeResourceDescForMask;
			stGpuVolumeResourceDescForMask.sdk_type = GpuSdkTypeDX11;

			// Intel HD 에서는 BYTE 를 제대로 못 읽는다!! -_-;.... // 20140303 왜 못 읽음 -_-; 잘 되는 것으로 다시 확인
			// normalize to original... precision bug! 가 HD 4000 에서 있는 것을 확인 // 20160523
			stGpuVolumeResourceDescForMask.dtype = pstVolMaskArchive->store_dtype;	// vxrDataTypeUNSIGNEDSHORT;//
			stGpuVolumeResourceDescForMask.src_obj_id = pCVolumeMask->GetObjectID();
			stGpuVolumeResourceDescForMask.usage_specifier = ("TEXTURE3D_VOLUME_DEFAULT_MASK");
			stGpuVolumeResourceDescForMask.res_type = ResourceTypeDX11RESOURCE;
			if (!pCGpuManager->GetGpuResourceArchive(&stGpuVolumeResourceDescForMask, &stGpuResourceVolumeTextureForMask))
			{
				pCGpuManager->GenerateGpuResource(&stGpuVolumeResourceDescForMask, pCVolumeMask, &stGpuResourceVolumeTextureForMask, pstLocalProgress);
			}
			stGpuVolumeResourceDescForMask.res_type = ResourceTypeDX11SRV;
			if (!pCGpuManager->GetGpuResourceArchive(&stGpuVolumeResourceDescForMask, &stGpuResourceVolumeSRVForMask))
			{
				pCGpuManager->GenerateGpuResource(&stGpuVolumeResourceDescForMask, pCVolumeMask, &stGpuResourceVolumeSRVForMask, NULL);
			}

#ifdef __CS_5_0_SUPPORT
			pdx11DeviceImmContext->CSSetShaderResources(8, 1, (ID3D11ShaderResourceView**)&stGpuResourceVolumeSRVForMask.vtrPtrs.at(0));
#else
			pdx11DeviceImmContext->PSSetShaderResources(8, 1, (ID3D11ShaderResourceView**)&stGpuResourceVolumeSRVForMask.vtrPtrs.at(0));
#endif
		}
#pragma endregion // Mask Volume Resource Setting

#pragma region // TObject Resource Setting 
		TMapData* pstMainTfArchive = pCTObjectMain->GetTMapData();
		GpuResDescriptor stGpuOtfResourceDesc;
		GpuResource stGpuResourceOtfBuffer, stGpuResourceOtfSRV;
		GpuResDescriptor stGpuOtfIdMapResourceDesc;
		GpuResource stGpuResourceOtfIdMapBuffer, stGpuResourceOtfIdMapSRV;

		stGpuOtfResourceDesc.sdk_type = GpuSdkTypeDX11;
		stGpuOtfResourceDesc.res_type = ResourceTypeDX11RESOURCE;
		stGpuOtfResourceDesc.dtype = pstMainTfArchive->dtype;
		stGpuOtfResourceDesc.src_obj_id = iMainTObjectID;
		stGpuOtfResourceDesc.misc = 0;
		stGpuOtfResourceDesc.usage_specifier = ("BUFFER_TOBJECT_OTFSERIES");

		int iNumPrevMaskOTFs = 0;
		pCTObjectMain->GetCustomParameter("_int_NumOTFSeries", data_type::dtype<int>(), &iNumPrevMaskOTFs);

		bool bMaskUpdate = false;
		if (iNumPrevMaskOTFs != iNumMaskOTFs && bIsUsedMaskOtfInRenderer)
		{
			bMaskUpdate = true;
			pCGpuManager->ReleaseGpuResource(&stGpuOtfResourceDesc, false);
			stGpuOtfResourceDesc.res_type = ResourceTypeDX11SRV;
			pCGpuManager->ReleaseGpuResource(&stGpuOtfResourceDesc, false);
			stGpuOtfResourceDesc.res_type = ResourceTypeDX11RESOURCE;
			pCTObjectMain->RegisterCustomParameter("_int_NumOTFSeries", iNumMaskOTFs);
		}

		if (!pCGpuManager->GetGpuResourceArchive(&stGpuOtfResourceDesc, &stGpuResourceOtfBuffer))
		{
			pCGpuManager->GenerateGpuResource(&stGpuOtfResourceDesc, pCTObjectMain, &stGpuResourceOtfBuffer, NULL);
			bIsTfChanged = true;
			bForceToSkipBlockUpdate = false;
		}

		stGpuOtfResourceDesc.res_type = ResourceTypeDX11SRV;
		if (!pCGpuManager->GetGpuResourceArchive(&stGpuOtfResourceDesc, &stGpuResourceOtfSRV))
		{
			pCGpuManager->GenerateGpuResource(&stGpuOtfResourceDesc, pCTObjectMain, &stGpuResourceOtfSRV, NULL);
			bForceToSkipBlockUpdate = false;
		}

		if (bIsTfChanged)
		{
			D3D11_MAPPED_SUBRESOURCE d11MappedRes;
			pdx11DeviceImmContext->Map((ID3D11Resource*)stGpuResourceOtfBuffer.vtrPtrs.at(0), 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes);
			vmbyte4* py4ColorTF = (vmbyte4*)d11MappedRes.pData;
			if (!bIsUsedMaskOtfInRenderer)
			{
				memcpy(py4ColorTF, pstMainTfArchive->tmap_buffers[0], pstMainTfArchive->array_lengths.x * sizeof(vmbyte4));
			}
			else
			{
				for (int j = 0; j < iNumMaskOTFs; j++)
				{
					int iTObjectMaskID = (int)vtrMaskOTFIDSeries.at(j);
					bool bVisibleMaskValue = false;
					if (vtrMaskOTFIDSeries_Visibilities.at(j) != 0)
						bVisibleMaskValue = true;

					if (bVisibleMaskValue)
					{
						auto itrTObjectMask = mapTObjects.find(iTObjectMaskID);
						VmTObject* pCTObjectMask = itrTObjectMask->second;
						TMapData* pstTfArchiveMask = pCTObjectMask->GetTMapData();
						memcpy(&py4ColorTF[j * pstMainTfArchive->array_lengths.x], pstTfArchiveMask->tmap_buffers[0], pstMainTfArchive->array_lengths.x * sizeof(vmbyte4));
					}
					else
					{
						memset(&py4ColorTF[j * pstMainTfArchive->array_lengths.x], 0, pstMainTfArchive->array_lengths.x * sizeof(vmbyte4));
					}
				}
			}
			pdx11DeviceImmContext->Unmap((ID3D11Resource*)stGpuResourceOtfBuffer.vtrPtrs.at(0), 0);
		}

		// TEST 
		if (iTestValue == 1 && iWindowingTObjectID > 0)
		{
			auto itrTObject = mapTObjects.find(iWindowingTObjectID);
			if (itrTObject == mapTObjects.end())
			{
				VMERRORMESSAGE("NO TOBJECT ID");
				continue;
			}

			bool bIsWindowingChanged = false;
			pCLObjectForParameters->GetDstObjValue(iWindowingTObjectID, ("_bool_IsTfChanged"), &bIsWindowingChanged);
			bIsTfChanged |= bIsWindowingChanged;

			VmTObject* pCTObjectWindowing = itrTObject->second;

			TMapData* pstWindowingTfArchive = pCTObjectWindowing->GetTMapData();
			GpuResDescriptor stGpuWindowingResourceDesc;
			GpuResource stGpuResourceWindowingBuffer, stGpuResourceWindowingSRV;

			stGpuWindowingResourceDesc.sdk_type = GpuSdkTypeDX11;
			stGpuWindowingResourceDesc.res_type = ResourceTypeDX11RESOURCE;
			stGpuWindowingResourceDesc.dtype = pstWindowingTfArchive->dtype;
			stGpuWindowingResourceDesc.src_obj_id = iWindowingTObjectID;
			stGpuWindowingResourceDesc.misc = 0;
			stGpuWindowingResourceDesc.usage_specifier = ("BUFFER_TOBJECT_OTFSERIES");

			if (!pCGpuManager->GetGpuResourceArchive(&stGpuWindowingResourceDesc, &stGpuResourceWindowingBuffer))
			{
				pCGpuManager->GenerateGpuResource(&stGpuWindowingResourceDesc, pCTObjectWindowing, &stGpuResourceWindowingBuffer, NULL);
				bIsTfChanged = true;
				bForceToSkipBlockUpdate = false;
			}

			stGpuWindowingResourceDesc.res_type = ResourceTypeDX11SRV;
			if (!pCGpuManager->GetGpuResourceArchive(&stGpuWindowingResourceDesc, &stGpuResourceWindowingSRV))
			{
				pCGpuManager->GenerateGpuResource(&stGpuWindowingResourceDesc, pCTObjectWindowing, &stGpuResourceWindowingSRV, NULL);
				bForceToSkipBlockUpdate = false;
			}

			if (bIsTfChanged)
			{
				D3D11_MAPPED_SUBRESOURCE d11MappedRes;
				pdx11DeviceImmContext->Map((ID3D11Resource*)stGpuResourceWindowingBuffer.vtrPtrs.at(0), 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes);
				vmbyte4* py4ColorTF = (vmbyte4*)d11MappedRes.pData;
				memcpy(py4ColorTF, pstWindowingTfArchive->tmap_buffers[0], pstWindowingTfArchive->array_lengths.x * sizeof(vmbyte4));
				pdx11DeviceImmContext->Unmap((ID3D11Resource*)stGpuResourceWindowingBuffer.vtrPtrs.at(0), 0);
			}

			pdx11DeviceImmContext->CSSetShaderResources(9, 1, (ID3D11ShaderResourceView**)&stGpuResourceWindowingSRV.vtrPtrs.at(0));
		}

		if (bIsUsedMaskOtfInRenderer)
		{
			stGpuOtfIdMapResourceDesc.sdk_type = GpuSdkTypeDX11;
			stGpuOtfIdMapResourceDesc.dtype = data_type::dtype<int>();
			stGpuOtfIdMapResourceDesc.res_type = ResourceTypeDX11RESOURCE;
			stGpuOtfIdMapResourceDesc.src_obj_id = iMainTObjectID;
			stGpuOtfIdMapResourceDesc.misc = 1;
			stGpuOtfIdMapResourceDesc.usage_specifier = ("BUFFER_TOBJECT_OTFSERIES");

			if (bMaskUpdate)
			{
				pCGpuManager->ReleaseGpuResource(&stGpuOtfIdMapResourceDesc, false);
				stGpuOtfIdMapResourceDesc.res_type = ResourceTypeDX11SRV;
				pCGpuManager->ReleaseGpuResource(&stGpuOtfIdMapResourceDesc, false);
				stGpuOtfIdMapResourceDesc.res_type = ResourceTypeDX11RESOURCE;
			}

			if (!pCGpuManager->GetGpuResourceArchive(&stGpuOtfIdMapResourceDesc, &stGpuResourceOtfIdMapBuffer))
			{
				pCGpuManager->GenerateGpuResource(&stGpuOtfIdMapResourceDesc, pCTObjectMain, &stGpuResourceOtfIdMapBuffer, NULL);
			}

			stGpuOtfIdMapResourceDesc.res_type = ResourceTypeDX11SRV;
			if (!pCGpuManager->GetGpuResourceArchive(&stGpuOtfIdMapResourceDesc, &stGpuResourceOtfIdMapSRV))
			{
				pCGpuManager->GenerateGpuResource(&stGpuOtfIdMapResourceDesc, pCTObjectMain, &stGpuResourceOtfIdMapSRV, NULL);
			}

			D3D11_MAPPED_SUBRESOURCE d11MappedRes;
			pdx11DeviceImmContext->Map((ID3D11Resource*)stGpuResourceOtfIdMapBuffer.vtrPtrs.at(0), 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes);
			int* pi4OtfIndex = (int*)d11MappedRes.pData;
			memset(pi4OtfIndex, 0, sizeof(int) * 128);
			for (int j = 0; j < iNumMaskOTFs; j++)
			{
				int iIndexOtf = static_cast<int>(vtrMaskOTFIDMap.at(j));
				pi4OtfIndex[iIndexOtf] = j;
			}
			pdx11DeviceImmContext->Unmap((ID3D11Resource*)stGpuResourceOtfIdMapBuffer.vtrPtrs.at(0), 0);
		}
#pragma endregion // TObject Resource Setting 

#pragma region // Main Volume Block Mask Setting From OTFs
		VolumeBlocks* pstVolBlock = pCVObjectVolume->GetVolumeBlock(iLevelBlock);
		if (pstVolBlock == NULL)
		{
			pCVObjectVolume->UpdateVolumeMinMaxBlocks();
			pstVolBlock = pCVObjectVolume->GetVolumeBlock(iLevelBlock);
		}

		GpuResDescriptor stGpuResBlockDesc;
		GpuResource stGpuResourceBlockTexture, stGpuResourceBlockSRV;
		stGpuResBlockDesc.sdk_type = GpuSdkTypeDX11;
		stGpuResBlockDesc.res_type = ResourceTypeDX11RESOURCE;
		if (bIsXRayMode)
		{
			stGpuResBlockDesc.usage_specifier = ("TEXTURE3D_VOLUME_MINMAXBLOCK");
			stGpuResBlockDesc.dtype = data_type::dtype<ushort>();
		}
		else
		{
			stGpuResBlockDesc.usage_specifier = ("TEXTURE3D_VOLUME_BLOCKMAP");
			if (pstVolBlock->blk_vol_size.x * pstVolBlock->blk_vol_size.y * pstVolBlock->blk_vol_size.z < 65536)
				stGpuResBlockDesc.dtype = data_type::dtype<ushort>();
			else
				stGpuResBlockDesc.dtype = data_type::dtype<uint>();
		}
		stGpuResBlockDesc.src_obj_id = pCVObjectVolume->GetObjectID();
		//stGpuResBlockDesc.src_obj_id = iMainTObjectID;
		stGpuResBlockDesc.misc = ((iMainTObjectID % 65536) << 16) | iLevelBlock;
		if (!pCGpuManager->GetGpuResourceArchive(&stGpuResBlockDesc, &stGpuResourceBlockTexture))
		{
			bIsTfChanged = true;
			bUpdateBricks = true;
			bForceToSkipBlockUpdate = false;
		}
		stGpuResBlockDesc.res_type = ResourceTypeDX11SRV;
		if (!pCGpuManager->GetGpuResourceArchive(&stGpuResBlockDesc, &stGpuResourceBlockSRV))
		{
			bIsTfChanged = true;
			bUpdateBricks = true;
			bForceToSkipBlockUpdate = false;
		}

		byte* pyTaggedActivatedBlocks = NULL;
		bool bUpdateBlocks = (bIsTfChanged || bUpdateBricks || bBlockUpdateForSculptMask) && !bForceToSkipBlockUpdate;
		if (bUpdateBlocks)
		{
			vmdouble2 d2OtfValueRangeMinMax = vmdouble2(DBL_MAX, -DBL_MAX);
			TMapData* pstTfArchive = pCTObjectMain->GetTMapData();
			d2OtfValueRangeMinMax.x = pstTfArchive->valid_min_idx.x;
			d2OtfValueRangeMinMax.y = pstTfArchive->valid_max_idx.x;

			// iLevelBlock
			pCVObjectVolume->UpdateTagBlocks(iMainTObjectID, 0, d2OtfValueRangeMinMax, NULL);
			pCVObjectVolume->UpdateTagBlocks(iMainTObjectID, 1, d2OtfValueRangeMinMax, NULL);
			pyTaggedActivatedBlocks = pstVolBlock->GetTaggedActivatedBlocks(iMainTObjectID);

			if (bIsUsedMaskOtfInRenderer) // Only Cares for Main Block of iLevelBlock
			{
				VolumeBlocks* pstVolBlockOtfMask = pCVolumeMask->GetVolumeBlock(iLevelBlock);

				vmint3 blk_vol_size = pstVolBlockOtfMask->blk_vol_size;
				vmint3 blk_bnd_size = pstVolBlockOtfMask->blk_bnd_size;
				int iBlockNumSampleSizeX = blk_vol_size.x + blk_bnd_size.x * 2;
				int iBlockNumSampleSizeY = blk_vol_size.y + blk_bnd_size.y * 2;
				int iBlockNumSampleSizeZ = blk_vol_size.z + blk_bnd_size.z * 2;
				int iNumBlocks = iBlockNumSampleSizeX * iBlockNumSampleSizeY*iBlockNumSampleSizeZ;

				vmbyte2* py2MinMaxOtfMaskBlocks = (vmbyte2*)pstVolBlockOtfMask->mM_blks;

				for (int j = 1; j < iNumMaskOTFs; j++)
				{
					int iTObjectMaskID = (int)vtrMaskOTFIDSeries.at(j);
					auto itrTObjectMask = mapTObjects.find(iTObjectMaskID);
					if (itrTObjectMask == mapTObjects.end())
					{
						VMERRORMESSAGE("GPU DVR! - INVALID TOBJECT ID - MASK TF CHECK");
						return false;
					}

					VmTObject* pCTObjectMask = itrTObjectMask->second;

					TMapData* pstTfArchiveMask = pCTObjectMask->GetTMapData();
					ushort usMinOtf = (ushort)min(max(pstTfArchiveMask->valid_min_idx.x, 0), 65535);
					ushort usMaxOtf = (ushort)max(min(pstTfArchiveMask->valid_max_idx.x, 65535), 0);

					vmushort2* pus2MinMaxMainVolBlocks = (vmushort2*)pstVolBlock->mM_blks;

					for (int iBlkIndex = 0; iBlkIndex < iNumBlocks; iBlkIndex++)
					{
						//if (iSculptValue == 0 || iSculptValue > iSculptIndex)
						vmbyte2 y2MinMaxBlockValue = py2MinMaxOtfMaskBlocks[iBlkIndex];
						if (y2MinMaxBlockValue.y >= j)
						{
							vmushort2 us2MinMaxVolBlockValue = pus2MinMaxMainVolBlocks[iBlkIndex];
							if (us2MinMaxVolBlockValue.y < usMinOtf || us2MinMaxVolBlockValue.x > usMaxOtf)
								pyTaggedActivatedBlocks[iBlkIndex] = 0;
							else
								pyTaggedActivatedBlocks[iBlkIndex] = 1;
						}
					}
				}

			}

			stGpuResBlockDesc.res_type = ResourceTypeDX11RESOURCE;
			if (!pCGpuManager->GetGpuResourceArchive(&stGpuResBlockDesc, &stGpuResourceBlockTexture))
				pCGpuManager->GenerateGpuResource(&stGpuResBlockDesc, pCVObjectVolume, &stGpuResourceBlockTexture, NULL);

			stGpuResBlockDesc.res_type = ResourceTypeDX11SRV;
			if (!pCGpuManager->GetGpuResourceArchive(&stGpuResBlockDesc, &stGpuResourceBlockSRV))
				pCGpuManager->GenerateGpuResource(&stGpuResBlockDesc, pCVObjectVolume, &stGpuResourceBlockSRV, NULL);
		}

		if (bUpdateBlocks && iSculptValue >= 0 && pCVolumeMask)
		{
			VolumeBlocks* pstVolBlockSculptMask = pCVolumeMask->GetVolumeBlock(iLevelBlock);
			if (pstVolBlockSculptMask == NULL)
				VMERRORMESSAGE("IMPOSSIBLE SCULPT MASK BLOCK!!");

			vmbyte2* py2MinMaxSculptMaskBlocks = (vmbyte2*)pstVolBlockSculptMask->mM_blks;

			vmint3 blk_vol_size = pstVolBlockSculptMask->blk_vol_size;
			vmint3 blk_bnd_size = pstVolBlockSculptMask->blk_bnd_size;
			int iBlockNumSampleSizeX = blk_vol_size.x + blk_bnd_size.x * 2;
			int iBlockNumSampleSizeY = blk_vol_size.y + blk_bnd_size.y * 2;
			int iBlockNumSampleSizeZ = blk_vol_size.z + blk_bnd_size.z * 2;
			int iNumBlocks = iBlockNumSampleSizeX * iBlockNumSampleSizeY*iBlockNumSampleSizeZ;

			for (int iBlkIndex = 0; iBlkIndex < iNumBlocks; iBlkIndex++)
			{
				vmbyte2 y2MinMaxBlockValue = py2MinMaxSculptMaskBlocks[iBlkIndex];
				if (y2MinMaxBlockValue.x != 0 && y2MinMaxBlockValue.y <= iSculptValue)
					pyTaggedActivatedBlocks[iBlkIndex] = 0;
			}

			// pCVolumeMask->GetVolumeBlock
			bUpdateBlocks = true;
		}

		if (!bIsXRayMode && bUpdateBlocks)
		{
			HDx11ModifyBlockMap(pCVObjectVolume, &stGpuResourceBlockTexture, &stGpuResourceBlockSRV, true);
		}
#pragma endregion // Main Block Mask Setting From OTFs

#pragma region // Constant Buffers
		bool bIsDownSample = false;
		if (stGpuResourceVolumeTexture.sample_interval.x > 1.f ||
			stGpuResourceVolumeTexture.sample_interval.y > 1.f ||
			stGpuResourceVolumeTexture.sample_interval.z > 1.f)
			bIsDownSample = true;
		if (iRendererType == __RM_MODULATION && bIsDownSample)
			bIsDownSample = false;

		CB_VrVolumeObject cbVrVolumeObj;
		HDx11ComputeConstantBufferVrObject(&cbVrVolumeObj, bIsDownSample, pCVObjectVolume, pCCObject, stGpuResourceVolumeTexture.GetLogicalElements(), pCLObjectForParameters, iVolumeID);
		if (bIsBrickChunk)
		{
			CB_VrBrickChunk cbVrBrickObj;
			HDx11ComputeConstantBufferVrBrickChunk(&cbVrBrickObj, pCVObjectVolume, iLevelBlock, &stGpuResourceVolumeTexture);
			D3D11_MAPPED_SUBRESOURCE mappedResVolBrickChunk;
			pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_BRICKCHUNK], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResVolBrickChunk);
			CB_VrBrickChunk* pCBParamVolumeBrickChunk = (CB_VrBrickChunk*)mappedResVolBrickChunk.pData;
			memcpy(pCBParamVolumeBrickChunk, &cbVrBrickObj, sizeof(CB_VrBrickChunk));
			pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_BRICKCHUNK], 0);
#ifdef __CS_5_0_SUPPORT
			pdx11DeviceImmContext->CSSetConstantBuffers(6, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_BRICKCHUNK]);
#else
			pdx11DeviceImmContext->PSSetConstantBuffers(6, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_BRICKCHUNK]);
#endif

			// f3VecGradientSamples Correction //
			cbVrVolumeObj.f3VecGradientSampleX.x *= cbVrBrickObj.f3ConvertRatioVTS2CTS.x;
			cbVrVolumeObj.f3VecGradientSampleX.y *= cbVrBrickObj.f3ConvertRatioVTS2CTS.y;
			cbVrVolumeObj.f3VecGradientSampleX.z *= cbVrBrickObj.f3ConvertRatioVTS2CTS.z;
			cbVrVolumeObj.f3VecGradientSampleY.x *= cbVrBrickObj.f3ConvertRatioVTS2CTS.x;
			cbVrVolumeObj.f3VecGradientSampleY.y *= cbVrBrickObj.f3ConvertRatioVTS2CTS.y;
			cbVrVolumeObj.f3VecGradientSampleY.z *= cbVrBrickObj.f3ConvertRatioVTS2CTS.z;
			cbVrVolumeObj.f3VecGradientSampleZ.x *= cbVrBrickObj.f3ConvertRatioVTS2CTS.x;
			cbVrVolumeObj.f3VecGradientSampleZ.y *= cbVrBrickObj.f3ConvertRatioVTS2CTS.y;
			cbVrVolumeObj.f3VecGradientSampleZ.z *= cbVrBrickObj.f3ConvertRatioVTS2CTS.z;
		}
		cbVrVolumeObj.iFlags |= iSculptValue << 24;
		D3D11_MAPPED_SUBRESOURCE mappedResVolObj;
		pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_VOLOBJ], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResVolObj);
		CB_VrVolumeObject* pCBParamVolumeObj = (CB_VrVolumeObject*)mappedResVolObj.pData;
		memcpy(pCBParamVolumeObj, &cbVrVolumeObj, sizeof(CB_VrVolumeObject));
		pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_VOLOBJ], 0);
#ifdef __CS_5_0_SUPPORT
		pdx11DeviceImmContext->CSSetConstantBuffers(0, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_VOLOBJ]);
#else
		pdx11DeviceImmContext->PSSetConstantBuffers(0, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_VOLOBJ]);
#endif

		CB_VrOtf1D cbVrOtf1D;
		HDx11ComputeConstantBufferVrOtf1D(&cbVrOtf1D, pCTObjectMain, stGpuOtfResourceDesc.misc, &_fncontainer->vmparams);
		if (iTestValue == 2 || iTestValue == 3)
			cbVrOtf1D.iOtf1stValue = iIsoValue;	// TEST
		D3D11_MAPPED_SUBRESOURCE mappedResOtf1D;
		pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_OTF1D], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResOtf1D);
		CB_VrOtf1D* pCBParamOtf1D = (CB_VrOtf1D*)mappedResOtf1D.pData;
		memcpy(pCBParamOtf1D, &cbVrOtf1D, sizeof(CB_VrOtf1D));
		pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_OTF1D], 0);
#ifdef __CS_5_0_SUPPORT
		pdx11DeviceImmContext->CSSetConstantBuffers(2, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_OTF1D]);
#else
		pdx11DeviceImmContext->PSSetConstantBuffers(2, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_OTF1D]);
#endif

		CB_VrBlockObject cbVrBlock;
		if (stGpuResBlockDesc.dtype == data_type::dtype<float>())
			HDx11ComputeConstantBufferVrBlockObj(&cbVrBlock, pCVObjectVolume, iLevelBlock, 1.f);
		else
			HDx11ComputeConstantBufferVrBlockObj(&cbVrBlock, pCVObjectVolume, iLevelBlock, 65535.f);
		D3D11_MAPPED_SUBRESOURCE mappedResBlock;
		pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_BLOCKOBJ], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResBlock);
		CB_VrBlockObject* pCBParamBlock = (CB_VrBlockObject*)mappedResBlock.pData;
		memcpy(pCBParamBlock, &cbVrBlock, sizeof(CB_VrBlockObject));
		pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_BLOCKOBJ], 0);
#ifdef __CS_5_0_SUPPORT
		pdx11DeviceImmContext->CSSetConstantBuffers(3, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_BLOCKOBJ]);
#else
		pdx11DeviceImmContext->PSSetConstantBuffers(3, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_BLOCKOBJ]);
#endif

		bool bIsMarkerMode = false;
		switch (iRendererType)
		{
		case __RM_SURFACECURVATURE:
		case __RM_SURFACEEFFECT:
		case __RM_SURFACECCF:
		{
			CB_VrSurfaceEffect cbVrSurfaceEffect;
			HDx11ComputeConstantBufferVrSurfaceEffect(&cbVrSurfaceEffect, cbVrVolumeObj.fSampleDistWS, pCLObjectForParameters, iVolumeID);
			// TEST
			if (iTestValue == 1)
			{
				cbVrSurfaceEffect.fDummy0 = (float)iDensity0;
				cbVrSurfaceEffect.fDummy1 = (float)iDensity1;
				cbVrSurfaceEffect.iSizeCurvatureKernel = (int)dKernelSize;
				cbVrSurfaceEffect.fPhongExpWeightU = (float)dThresholdGradSqLength;
				cbVrSurfaceEffect.fPhongExpWeightV = (float)dThresholdLaplacian;
				cbVrSurfaceEffect.fPhongBRDF_DiffuseRatio = (float)iTestMode;
			}
			else if (iTestValue == 3)
			{
				cbVrSurfaceEffect.iSizeCurvatureKernel = (int)dKernelSize;
			}

			D3D11_MAPPED_SUBRESOURCE mappedResSurfaceEffect;
			pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_SURFACEEFFECT], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResSurfaceEffect);
			CB_VrSurfaceEffect* pCBParamSurfaceEffect = (CB_VrSurfaceEffect*)mappedResSurfaceEffect.pData;
			memcpy(pCBParamSurfaceEffect, &cbVrSurfaceEffect, sizeof(CB_VrSurfaceEffect));
			pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_SURFACEEFFECT], 0);
#ifdef __CS_5_0_SUPPORT
			pdx11DeviceImmContext->CSSetConstantBuffers(4, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_SURFACEEFFECT]);
#else
			pdx11DeviceImmContext->PSSetConstantBuffers(4, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_SURFACEEFFECT]);
#endif

			CB_VrInterestingRegion cbVrInterestingRegion;
			HDx11ComputeConstantBufferVrInterestingRegion(&cbVrInterestingRegion, pCLObjectForParameters, iVolumeID);
			if (cbVrInterestingRegion.iNumRegions > 0)
			{
				D3D11_MAPPED_SUBRESOURCE mappedResInterstingRegion;
				pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_INTERESTINGREGION], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResInterstingRegion);
				CB_VrInterestingRegion* pCBParamInterestingRegion = (CB_VrInterestingRegion*)mappedResInterstingRegion.pData;
				memcpy(pCBParamInterestingRegion, &cbVrInterestingRegion, sizeof(CB_VrInterestingRegion));
				pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_INTERESTINGREGION], 0);
#ifdef __CS_5_0_SUPPORT
				pdx11DeviceImmContext->CSSetConstantBuffers(8, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_INTERESTINGREGION]);
#else
				pdx11DeviceImmContext->PSSetConstantBuffers(8, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_INTERESTINGREGION]);
#endif

				bIsMarkerMode = true;
			}
		}
		break;
		case __RM_MODULATION:
		{
			CB_VrModulation cbVrModulation;
			HDx11ComputeConstantBufferVrModulation(&cbVrModulation, pCVObjectVolume, f3PosEyeWS, pCLObjectForParameters, iVolumeID);
			cbVrModulation.fdummy____0 = 1;

			D3D11_MAPPED_SUBRESOURCE mappedResModulation;
			pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_MODULATION], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResModulation);
			CB_VrModulation* pCBParamModulation = (CB_VrModulation*)mappedResModulation.pData;
			memcpy(pCBParamModulation, &cbVrModulation, sizeof(CB_VrSurfaceEffect));
			pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_MODULATION], 0);
#ifdef __CS_5_0_SUPPORT
			pdx11DeviceImmContext->CSSetConstantBuffers(5, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_MODULATION]);
#else
			pdx11DeviceImmContext->PSSetConstantBuffers(5, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_MODULATION]);
#endif
		}
		break;
		}

#pragma endregion // Constant Buffers

#pragma region // Resource Shader Setting
#ifdef __CS_5_0_SUPPORT
		pdx11DeviceImmContext->CSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&stGpuResourceOtfSRV.vtrPtrs.at(0));
		if (bIsUsedMaskOtfInRenderer)
			pdx11DeviceImmContext->CSSetShaderResources(3, 1, (ID3D11ShaderResourceView**)&stGpuResourceOtfIdMapSRV.vtrPtrs.at(0));

		if (iRendererType == __RM_RAYMAX)	// Min
			pdx11DeviceImmContext->CSSetShaderResources(2, 1, (ID3D11ShaderResourceView**)&stGpuResourceBlockSRV.vtrPtrs.at(1));
		else
			pdx11DeviceImmContext->CSSetShaderResources(2, 1, (ID3D11ShaderResourceView**)&stGpuResourceBlockSRV.vtrPtrs.at(0));

		if (bIsBrickChunk)
		{
			ID3D11ShaderResourceView* arrayViews[10] = { NULL };
			int iNumBrickChunks = (int)min(stGpuResourceVolumeSRV.vtrPtrs.size(), 10);
			for (int i = 0; i < iNumBrickChunks; i++)
				arrayViews[i] = (ID3D11ShaderResourceView*)stGpuResourceVolumeSRV.vtrPtrs.at(i);
			pdx11DeviceImmContext->CSSetShaderResources(10, iNumBrickChunks, arrayViews);
		}
		else
		{
			pdx11DeviceImmContext->CSSetShaderResources(1, 1, (ID3D11ShaderResourceView**)&stGpuResourceVolumeSRV.vtrPtrs.at(0));
		}
#else
		pdx11DeviceImmContext->PSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&stGpuResourceOtfSRV.vtrPtrs.at(0));
		if (bIsUsedMaskOtfInRenderer)
			pdx11DeviceImmContext->PSSetShaderResources(3, 1, (ID3D11ShaderResourceView**)&stGpuResourceOtfIdMapSRV.vtrPtrs.at(0));

		if (iRendererType == __RM_RAYMAX)	// Min
			pdx11DeviceImmContext->PSSetShaderResources(2, 1, (ID3D11ShaderResourceView**)&stGpuResourceBlockSRV.vtrPtrs.at(1));
		else
			pdx11DeviceImmContext->PSSetShaderResources(2, 1, (ID3D11ShaderResourceView**)&stGpuResourceBlockSRV.vtrPtrs.at(0));

		if (bIsBrickChunk)
		{
			ID3D11ShaderResourceView* arrayViews[10] = { NULL };
			int iNumBrickChunks = (int)min(stGpuResourceVolumeSRV.vtrPtrs.size(), 10);
			for (int i = 0; i < iNumBrickChunks; i++)
				arrayViews[i] = (ID3D11ShaderResourceView*)stGpuResourceVolumeSRV.vtrPtrs.at(i);
			pdx11DeviceImmContext->PSSetShaderResources(10, iNumBrickChunks, arrayViews);
		}
		else
		{
			pdx11DeviceImmContext->PSSetShaderResources(1, 1, (ID3D11ShaderResourceView**)&stGpuResourceVolumeSRV.vtrPtrs.at(0));
		}
#endif
#pragma endregion // Resource Shader Setting

#pragma region // Renderer
		if (bIsShadowStage)
		{
#ifdef __CS_5_0_SUPPORT
			if (bIsUsedMaskOtfInRenderer)
				pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[__SHADER_DVR_BASIC_OTFMASK_SHADOWMAP], NULL, 0);
			else
				pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[__SHADER_DVR_BASIC_DEFAULT_SHADOWMAP], NULL, 0);

			ID3D11UnorderedAccessView* pdx11UAV_ShadowMap = (ID3D11UnorderedAccessView*)gpuResourcesSHADOWMAP_ARCHs[__SM_UAV_DEPTH_SHADOWMAP].vtrPtrs.at(0);
			pdx11DeviceImmContext->CSSetUnorderedAccessViews(1, 1, &pdx11UAV_ShadowMap, &UAVInitialCounts);

			pdx11DeviceImmContext->Dispatch(uiNumGridX, uiNumGridY, 1);
			pdx11DeviceImmContext->Flush();
			pdx11DeviceImmContext->CSSetUnorderedAccessViews(1, 1, &pdx11UAV_NULL, NULL);
#else
			if (bIsUsedMaskOtfInRenderer)
				pdx11DeviceImmContext->PSSetShader(*ppdx11PS_VRs[__SHADER_DVR_BASIC_OTFMASK_SHADOWMAP], NULL, 0);
			else
				pdx11DeviceImmContext->PSSetShader(*ppdx11PS_VRs[__SHADER_DVR_BASIC_DEFAULT_SHADOWMAP], NULL, 0);

			pdx11RTVs[0] = (ID3D11RenderTargetView*)gpuResourcesSHADOWMAP_ARCHs[__SM_RTV_DEPTH_SHADOWMAP].vtrPtrs.at(0);
			pdx11RTVs[1] = NULL;
			pdx11DeviceImmContext->OMSetRenderTargets(1, pdx11RTVs, NULL);
			pdx11DeviceImmContext->Draw(4, 0);
			pdx11DeviceImmContext->Flush();
			pdx11DeviceImmContext->OMSetRenderTargets(1, pdx11RTVs_NULL, NULL);
#endif

			break; // Exit
		}
		else // Rendering Stage
		{
			bool bIsOnTheFlyMixing = false; // Layers
			if (iLevelVR != 2 &&
				iRendererType != __RM_SURFACEEFFECT &&
				iRendererType != __RM_SURFACECCF &&
				iRendererType != __RM_SURFACECURVATURE &&
				iRendererType != __RM_RAYMAX && iRendererType != __RM_RAYMIN && iRendererType != __RM_RAYSUM &&
				i == iNumMainVolumes - 1 && !bIsShowBlock
				)
				bIsOnTheFlyMixing = true;

			if (bIsOnTheFlyMixing)
			{
				int iShaderIndex = 0;
#pragma region RENDERING MODE
				switch (iRendererType)
				{
				case __RM_DEFAULT:
					if (bIsUsedMaskOtfInRenderer)
					{
						if (bIsShadowRenderer) iShaderIndex = __SHADER_DVR_LAYERS_DEFAULT_OTFMASK_SHADOW;
						else iShaderIndex = __SHADER_DVR_LAYERS_DEFAULT_OTFMASK;
					}
					else
					{
						if (bIsShadowRenderer) iShaderIndex = __SHADER_DVR_LAYERS_DEFAULT_SHADOW;
						else iShaderIndex = __SHADER_DVR_LAYERS_DEFAULT;
					}
					break;
				case __RM_CLIPOPAQUE:
					iShaderIndex = __SHADER_DVR_LAYERS_CLIPSEMIOPAQUE;
					break;
				case __RM_MODULATION:
					if (bIsUsedMaskOtfInRenderer) iShaderIndex = __SHADER_DVR_LAYERS_MODULATION_OTFMASK;
					else iShaderIndex = __SHADER_DVR_LAYERS_MODULATION;
					break;
				case __RM_SCULPTMASK:
					iShaderIndex = __SHADER_DVR_LAYERS_SCULPTMASK;
					break;
				default:
					VMERRORMESSAGE("NOT SUPPORTED RENDERING MODE!");
					break;
				}
#pragma endregion
				const int iQualityIndexOffset = 8;
				if (iLevelVR == 0)
					iShaderIndex += iQualityIndexOffset;

				int iModRTLayer = iCountSingleRendering % iNumTexureLayers;
				if (iModRTLayer != 0)
				{
#ifdef __CS_5_0_SUPPORT
					int iRSA_Index_Offset_Prev = RTTandLayersToLayersCS(pdx11DeviceImmContext, uiNumGridX, uiNumGridY,
						pdx11SRV_RenderOuts, pdx11SRV_DepthCSs, iCountMerging,
						pUAV_Merge_PingpongCS, pSRV_Merge_PingpongCS,
						pdx11CommonParams, ppdx11CS_Merges, pdx11SRV_4NULLs, pdx11SRV_2NULLs, NULL, iMergeLevel);

					// Clear //
					pdx11DeviceImmContext->ClearUnorderedAccessViewUint(pUAV_Merge_PingpongCS[iRSA_Index_Offset_Prev], uiClearVlaues);
#else
					int iRSA_Index_Offset_Prev = RTTandLayersToLayers(pdx11DeviceImmContext,
						pdx11SRV_RenderOuts, pdx11SRV_DepthCSs, iCountMerging,
						pRTV_Merge_RGBA_PingpongPS, pSRV_Merge_RGBA_PingpongPS,
						pRTV_Merge_DepthCS_PingpongPS, pSRV_Merge_DepthCS_PingpongPS,
						pdx11IL_ProxyRect, pdx11CommonParams, pdx11VS_ProxyRect,
						pdx11BufProxyRect, ppdx11PS_Merges, pdx11SRV_4NULLs, pdx11SRV_2NULLs, pdx11RTV_2NULLs, iMergeLevel);

					// Clear //
					pdx11DeviceImmContext->ClearRenderTargetView(pRTV_Merge_RGBA_PingpongPS[iRSA_Index_Offset_Prev], fClearColor);
					pdx11DeviceImmContext->ClearRenderTargetView(pRTV_Merge_DepthCS_PingpongPS[iRSA_Index_Offset_Prev], fClearDepth);

					pdx11DeviceImmContext->ClearRenderTargetView(pdx11RTV_RenderOuts[0], fClearColor);
					pdx11DeviceImmContext->ClearRenderTargetView(pdx11RTV_DepthCSs[0], fClearDepth);
#endif
					iCountMerging++;
					iCountMergingInVR++;
				}

				int iRSA_Index_Offset_Prev_LayerOut = 0;
				if (iCountMerging % 2 == 0)
					iRSA_Index_Offset_Prev_LayerOut = 1;

#ifdef __CS_5_0_SUPPORT
				ID3D11ShaderResourceView* pdx11SRV_Layers = pSRV_Merge_PingpongCS[iRSA_Index_Offset_Prev_LayerOut];
				pdx11DeviceImmContext->CSSetShaderResources(50, 1, &pdx11SRV_Layers);

				ID3D11UnorderedAccessView* pdx11UAV_Renderout = (ID3D11UnorderedAccessView*)gpuResourcesFB_ARCHs[__FB_UAV_RENDEROUT].vtrPtrs.at(0);
				ID3D11UnorderedAccessView* pdx11UAV_DepthCS = (ID3D11UnorderedAccessView*)gpuResourcesFB_ARCHs[__FB_UAV_DEPTHCS].vtrPtrs.at(0);

				pdx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, &pdx11UAV_Renderout, &UAVInitialCounts);
				pdx11DeviceImmContext->CSSetUnorderedAccessViews(2, 1, &pdx11UAV_DepthCS, &UAVInitialCounts);
				pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[iShaderIndex], NULL, 0);

				pdx11DeviceImmContext->Flush();
				pdx11DeviceImmContext->Dispatch(uiNumGridX, uiNumGridY, 1);
#else
				ID3D11ShaderResourceView* pdx11SRV_RGBA_RSA = pSRV_Merge_RGBA_PingpongPS[iRSA_Index_Offset_Prev_LayerOut];
				ID3D11ShaderResourceView* pdx11SRV_DepthCS_RSA = pSRV_Merge_DepthCS_PingpongPS[iRSA_Index_Offset_Prev_LayerOut];

				pdx11DeviceImmContext->PSSetShaderResources(50, 1, &pdx11SRV_RGBA_RSA);
				pdx11DeviceImmContext->PSSetShaderResources(51, 1, &pdx11SRV_DepthCS_RSA);

				pdx11RTVs[0] = (ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_RENDEROUT].vtrPtrs.at(0);
				pdx11RTVs[1] = (ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_DEPTHCS].vtrPtrs.at(0);
				pdx11DeviceImmContext->OMSetRenderTargets(2, pdx11RTVs, NULL);

				pdx11DeviceImmContext->PSSetShader(*ppdx11PS_VRs[iShaderIndex], NULL, 0);

				pdx11DeviceImmContext->Flush();
				pdx11DeviceImmContext->Draw(4, 0);
#endif
				bIsCalledOnTheFlyMixing = true;
			}
			else // Single Out
			{
				int iShaderIndex = 0;
#pragma region RENDERING MODE
				if (bIsShowBlock) iShaderIndex = __SHADER_DVR_TEST;
				else
				{
					switch (iRendererType)
					{
					case __RM_DEFAULT:
						if (bIsUsedMaskOtfInRenderer)
						{
							if (bIsShadowRenderer) iShaderIndex = __SHADER_DVR_BASIC_DEFAULT_OTFMASK_SHADOW;
							else iShaderIndex = __SHADER_DVR_BASIC_DEFAULT_OTFMASK;
						}
						else
						{
							if (bIsShadowRenderer) iShaderIndex = __SHADER_DVR_BASIC_DEFAULT_SHADOW;
							else iShaderIndex = __SHADER_DVR_BASIC_DEFAULT;
						}
						break;
					case __RM_CLIPOPAQUE:
						iShaderIndex = __SHADER_DVR_BASIC_CLIPSEMIOPAQUE;
						break;
					case __RM_MODULATION:
						if (bIsUsedMaskOtfInRenderer) iShaderIndex = __SHADER_DVR_BASIC_MODULATION_OTFMASK;
						else iShaderIndex = __SHADER_DVR_BASIC_MODULATION;
						break;
					case __RM_SCULPTMASK:
						iShaderIndex = __SHADER_DVR_BASIC_SCULPTMASK;
						break;
					case __RM_SURFACEEFFECT:
						if (bIsUsedMaskOtfInRenderer)
						{
							if (bIsShadowRenderer) iShaderIndex = __SHADER_DVR_SURFACE_DEFAULT_OTFMASK_SHADOW;
							else iShaderIndex = __SHADER_DVR_SURFACE_DEFAULT_OTFMASK;
						}
						else
						{
							if (bIsShadowRenderer) iShaderIndex = __SHADER_DVR_SURFACE_DEFAULT_SHADOW;
							else iShaderIndex = __SHADER_DVR_SURFACE_DEFAULT;
						}
						if (bIsMarkerMode)
							iShaderIndex = __SHADER_DVR_SURFACE_MARKER;
						break;
					case __RM_SURFACECCF:
						iShaderIndex = __SHADER_DVR_SURFACE_CCF;
						break;
					case __RM_SURFACECURVATURE:
						iShaderIndex = __SHADER_DVR_SURFACE_CURVATURE;
						break;
					case __RM_RAYMAX:
						if (bIsUsedMaskOtfInRenderer) iShaderIndex = __SHADER_XRAY_MAX_OTFMASK;
						else iShaderIndex = __SHADER_XRAY_MAX;
						break;
					case __RM_RAYMIN:
						if (bIsUsedMaskOtfInRenderer) iShaderIndex = __SHADER_XRAY_MIN_OTFMASK;
						else iShaderIndex = __SHADER_XRAY_MIN;
						break;
					case __RM_RAYSUM:
						if (bIsUsedMaskOtfInRenderer) iShaderIndex = __SHADER_XRAY_SUM_OTFMASK;
						else iShaderIndex = __SHADER_XRAY_SUM;
						break;
					default:
						VMERRORMESSAGE("NOT SUPPORTED RENDERING MODE!");
						break;
					}
				}
#pragma endregion

				iCountSingleRendering++;

				int iModRTLayer = iCountSingleRendering % iNumTexureLayers;
				int iIndexRTT = (iModRTLayer + 3) % iNumTexureLayers;

#ifdef __CS_5_0_SUPPORT
				ID3D11UnorderedAccessView* pdx11UAV_Renderout = (ID3D11UnorderedAccessView*)gpuResourcesFB_ARCHs[__FB_UAV_RENDEROUT].vtrPtrs.at(iIndexRTT);
				ID3D11UnorderedAccessView* pdx11UAV_DepthCS = (ID3D11UnorderedAccessView*)gpuResourcesFB_ARCHs[__FB_UAV_DEPTHCS].vtrPtrs.at(iIndexRTT);
				pdx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, &pdx11UAV_Renderout, &UAVInitialCounts);
				pdx11DeviceImmContext->CSSetUnorderedAccessViews(2, 1, &pdx11UAV_DepthCS, &UAVInitialCounts);

				pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[iShaderIndex], NULL, 0);
				if (iTestIndex > 0)
					pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[__SHADER_DVR_TEST], NULL, 0);
				pdx11DeviceImmContext->Dispatch(uiNumGridX, uiNumGridY, 1);
#else
				pdx11RTVs[0] = (ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_RENDEROUT].vtrPtrs.at(iIndexRTT);
				pdx11RTVs[1] = (ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_DEPTHCS].vtrPtrs.at(iIndexRTT);
				pdx11DeviceImmContext->OMSetRenderTargets(2, pdx11RTVs, NULL);

				pdx11DeviceImmContext->PSSetShader(*ppdx11PS_VRs[iShaderIndex], NULL, 0);
				pdx11DeviceImmContext->Draw(4, 0);
#endif
				if (iModRTLayer == 0)
				{
#ifdef __CS_5_0_SUPPORT
					int iRSA_Index_Offset_Prev = RTTandLayersToLayersCS(pdx11DeviceImmContext, uiNumGridX, uiNumGridY,
						pdx11SRV_RenderOuts, pdx11SRV_DepthCSs, iCountMerging,
						pUAV_Merge_PingpongCS, pSRV_Merge_PingpongCS,
						pdx11CommonParams, ppdx11CS_Merges, pdx11SRV_4NULLs, pdx11SRV_2NULLs, NULL, iMergeLevel);

					// Clear //
					pdx11DeviceImmContext->ClearUnorderedAccessViewUint(pUAV_Merge_PingpongCS[iRSA_Index_Offset_Prev], uiClearVlaues);

					for (int j = 0; j < iNumTexureLayers; j++)
					{
						pdx11DeviceImmContext->ClearUnorderedAccessViewUint(pdx11UAV_RenderOuts[j], uiClearVlaues);
						pdx11DeviceImmContext->ClearUnorderedAccessViewUint(pdx11UAV_DepthCSs[j], uiClearVlaues);
					}
#else
					int iRSA_Index_Offset_Prev = RTTandLayersToLayers(pdx11DeviceImmContext,
						pdx11SRV_RenderOuts, pdx11SRV_DepthCSs, iCountMerging,
						pRTV_Merge_RGBA_PingpongPS, pSRV_Merge_RGBA_PingpongPS,
						pRTV_Merge_DepthCS_PingpongPS, pSRV_Merge_DepthCS_PingpongPS,
						pdx11IL_ProxyRect, pdx11CommonParams, pdx11VS_ProxyRect,
						pdx11BufProxyRect, ppdx11PS_Merges, pdx11SRV_4NULLs, pdx11SRV_2NULLs, pdx11RTV_2NULLs, iMergeLevel);

					// Clear //
					pdx11DeviceImmContext->ClearRenderTargetView(pRTV_Merge_RGBA_PingpongPS[iRSA_Index_Offset_Prev], fClearColor);
					pdx11DeviceImmContext->ClearRenderTargetView(pRTV_Merge_DepthCS_PingpongPS[iRSA_Index_Offset_Prev], fClearDepth);

					for (int j = 0; j < iNumTexureLayers; j++)
					{
						pdx11DeviceImmContext->ClearRenderTargetView(pdx11RTV_RenderOuts[j], fClearColor);
						pdx11DeviceImmContext->ClearRenderTargetView(pdx11RTV_DepthCSs[j], fClearDepth);
					}
#endif

					iCountMerging++;
					iCountMergingInVR++;
				}

			} // if (bIsOnTheFlyMixing)
		}
#pragma endregion // Renderer
	}

	if (bIsShadowStage)	// supposed to call rendering again w/ !bIsShadowStage
	{
		pdx11DeviceImmContext->ClearState();

		ID3D11RenderTargetView* pdxRTVOlds[2];
		pdxRTVOlds[0] = pdxRTVOld;
		pdxRTVOlds[1] = NULL;
		pdx11DeviceImmContext->OMSetRenderTargets(2, pdxRTVOlds, pdxDSVOld);

		VMSAFE_RELEASE(pdxRTVOld);
		VMSAFE_RELEASE(pdxDSVOld);

		return true;
	}

	if (!bIsCalledOnTheFlyMixing)
	{
		int iModRTLayer = iCountSingleRendering % iNumTexureLayers;
		bool bCall_RTTandLayersToLayers = false;
		if (iModRTLayer != 0)
		{
			if (iLevelVR == 2)
				bCall_RTTandLayersToLayers = iCountSingleRendering > 1;
			else
				bCall_RTTandLayersToLayers = !(iCountSingleRendering == 1 && iCountMerging == 0);
		}
		if (bCall_RTTandLayersToLayers)
		{
#ifdef __CS_5_0_SUPPORT
			RTTandLayersToLayersCS(pdx11DeviceImmContext, uiNumGridX, uiNumGridY,
				pdx11SRV_RenderOuts, pdx11SRV_DepthCSs, iCountMerging,
				pUAV_Merge_PingpongCS, pSRV_Merge_PingpongCS,
				pdx11CommonParams, ppdx11CS_Merges, pdx11SRV_4NULLs, pdx11SRV_2NULLs, NULL, iMergeLevel);
#else
			RTTandLayersToLayers(pdx11DeviceImmContext,
				pdx11SRV_RenderOuts, pdx11SRV_DepthCSs, iCountMerging,
				pRTV_Merge_RGBA_PingpongPS, pSRV_Merge_RGBA_PingpongPS,
				pRTV_Merge_DepthCS_PingpongPS, pSRV_Merge_DepthCS_PingpongPS,
				pdx11IL_ProxyRect, pdx11CommonParams, pdx11VS_ProxyRect,
				pdx11BufProxyRect, ppdx11PS_Merges, pdx11SRV_4NULLs, pdx11SRV_2NULLs, pdx11RTV_2NULLs, iMergeLevel);
#endif	
			iCountMerging++;
			iCountMergingInVR++;
		}

		if (iCountMerging > 0)	// Layers to Render out
		{
			int iRSA_Index_Offset_Prev = 0;
			if (iCountMerging % 2 == 0)
			{
				iRSA_Index_Offset_Prev = 1;
			}

			pdx11DeviceImmContext->Flush();

#ifdef __CS_5_0_SUPPORT
			ID3D11ShaderResourceView* pdx11SRV_Layers = pSRV_Merge_PingpongCS[iRSA_Index_Offset_Prev];
			pdx11DeviceImmContext->CSSetShaderResources(50, 1, &pdx11SRV_Layers);

			ID3D11UnorderedAccessView* pdx11UAV_Renderout = (ID3D11UnorderedAccessView*)gpuResourcesFB_ARCHs[__FB_UAV_RENDEROUT].vtrPtrs.at(0);
			ID3D11UnorderedAccessView* pdx11UAV_DepthCS = (ID3D11UnorderedAccessView*)gpuResourcesFB_ARCHs[__FB_UAV_DEPTHCS].vtrPtrs.at(0);

			UINT UAVInitialCounts = 0;
			pdx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, &pdx11UAV_Renderout, &UAVInitialCounts);
			pdx11DeviceImmContext->CSSetUnorderedAccessViews(2, 1, &pdx11UAV_DepthCS, &UAVInitialCounts);
			pdx11DeviceImmContext->CSSetShader(*ppdx11CS_Merges[__CS_MERGE_LAYERS_TO_RENDEROUT], NULL, 0);

			pdx11DeviceImmContext->Dispatch(uiNumGridX, uiNumGridY, 1);
#else
			ID3D11ShaderResourceView* pdx11SRV_RGBA_RSA = pSRV_Merge_RGBA_PingpongPS[iRSA_Index_Offset_Prev];
			ID3D11ShaderResourceView* pdx11SRV_DepthCS_RSA = pSRV_Merge_DepthCS_PingpongPS[iRSA_Index_Offset_Prev];

			pdx11DeviceImmContext->PSSetShaderResources(50, 1, &pdx11SRV_RGBA_RSA);
			pdx11DeviceImmContext->PSSetShaderResources(51, 1, &pdx11SRV_DepthCS_RSA);

			pdx11RTVs[0] = (ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_RENDEROUT].vtrPtrs.at(0);
			pdx11RTVs[1] = (ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_DEPTHCS].vtrPtrs.at(0);
			pdx11DeviceImmContext->OMSetRenderTargets(2, pdx11RTVs, NULL);

			//__PS_MERGE_LAYERS_TO_RENDEROUT
			uint uiStrideSizeInput = sizeof(vmfloat3);
			uint uiOffset = 0;
			pdx11DeviceImmContext->IASetVertexBuffers(0, 1, (ID3D11Buffer**)&pdx11BufProxyRect, &uiStrideSizeInput, &uiOffset);
			pdx11DeviceImmContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			pdx11DeviceImmContext->IASetInputLayout(pdx11IL_ProxyRect);
			pdx11DeviceImmContext->RSSetState(pdx11CommonParams->pdx11RStates[__RState_SOLID_NONE]);
			pdx11DeviceImmContext->VSSetShader(pdx11VS_ProxyRect, NULL, 0);
			pdx11DeviceImmContext->PSSetShader(*ppdx11PS_Merges[__PS_MERGE_LAYERS_TO_RENDEROUT], NULL, 0);

			pdx11DeviceImmContext->Draw(4, 0);
#endif
		}
	}

	// iLevelVR, bIsCalledOnTheFlyMixing, iCountSingleRendering
	// iCountMerging, iCountMergingInVR
	//printf("iLevelVR : %d, bIsCalledOnTheFlyMixing : %d, iCountSingleRendering : %d, iCountMerging : %d, iCountMergingInVR : %d\n", 
	//	iLevelVR, (int)bIsCalledOnTheFlyMixing, iCountSingleRendering, iCountMerging, iCountMergingInVR);

MIXING_SR_VR:
	vmbyte4* py4ColorBufferVr = NULL;
	float* pfDepthBufferVr = NULL;
	FrameBuffer* pstFrameBufferFinal = (FrameBuffer*)pCOutputIObject->GetFrameBuffer(FrameBufferUsageRENDEROUT, 0);
	FrameBuffer* pstFrameBufferDepthFinal = (FrameBuffer*)pCOutputIObject->GetFrameBuffer(FrameBufferUsageDEPTH, 0);
	vmbyte4* py4ColorBufferFinal = (vmbyte4*)pstFrameBufferFinal->fbuffer;
	float* pfDepthBufferFinal = (float*)pstFrameBufferDepthFinal->fbuffer;

	if (iLevelVR == 2)
	{
		FrameBuffer* pstFrameBufferVr = (FrameBuffer*)pCMergeIObject->GetFrameBuffer(FrameBufferUsageCUSTOM, 0);
		FrameBuffer* pstFrameBufferDepthVr = (FrameBuffer*)pCMergeIObject->GetFrameBuffer(FrameBufferUsageCUSTOM, 1);
		py4ColorBufferVr = (vmbyte4*)pstFrameBufferVr->fbuffer;
		pfDepthBufferVr = (float*)pstFrameBufferDepthVr->fbuffer;
	}
	else
	{
		py4ColorBufferVr = py4ColorBufferFinal;
		pfDepthBufferVr = pfDepthBufferFinal;
	}

	if (!bReusePrevRendering || iLevelVR != 2 || iLastRenderedVr != 2)
	{
		pdx11DeviceImmContext->Flush();
#pragma region // Copy GPU to CPU
		GpuResDescriptor gpuResourceFB_RENDEROUT_SYSTEM_DESC;
		GpuResDescriptor gpuResourceFB_DEPTH_SYSTEM_DESC;
		GpuResource gpuResourceFB_RENDEROUT_SYSTEM_ARCH;
		GpuResource gpuResourceFB_DEPTH_SYSTEM_ARCH;
		gpuResourceFB_RENDEROUT_SYSTEM_DESC.res_type = ResourceTypeDX11RESOURCE;
		gpuResourceFB_RENDEROUT_SYSTEM_DESC.dtype = gpuResourcesFB_DESCs[__FB_TX2D_RENDEROUT].dtype;
		gpuResourceFB_RENDEROUT_SYSTEM_DESC.sdk_type = GpuSdkTypeDX11;
		gpuResourceFB_RENDEROUT_SYSTEM_DESC.usage_specifier = ("TEXTURE2D_IOBJECT_SYSTEM");
		gpuResourceFB_RENDEROUT_SYSTEM_DESC.misc = 0;
		gpuResourceFB_RENDEROUT_SYSTEM_DESC.src_obj_id = pCMergeIObject->GetObjectID();
		gpuResourceFB_DEPTH_SYSTEM_DESC.res_type = ResourceTypeDX11RESOURCE;
		gpuResourceFB_DEPTH_SYSTEM_DESC.dtype = data_type::dtype<float>();
		gpuResourceFB_DEPTH_SYSTEM_DESC.sdk_type = GpuSdkTypeDX11;
		gpuResourceFB_DEPTH_SYSTEM_DESC.usage_specifier = ("TEXTURE2D_IOBJECT_SYSTEM");
		gpuResourceFB_DEPTH_SYSTEM_DESC.misc = 0;
		gpuResourceFB_DEPTH_SYSTEM_DESC.src_obj_id = pCMergeIObject->GetObjectID();
		// When Resize, the Framebuffer is released, so we do not have to check the resize case.
		if (!pCGpuManager->GetGpuResourceArchive(&gpuResourceFB_RENDEROUT_SYSTEM_DESC, &gpuResourceFB_RENDEROUT_SYSTEM_ARCH))
			pCGpuManager->GenerateGpuResource(&gpuResourceFB_RENDEROUT_SYSTEM_DESC, pCMergeIObject, &gpuResourceFB_RENDEROUT_SYSTEM_ARCH, NULL);
		if (!pCGpuManager->GetGpuResourceArchive(&gpuResourceFB_DEPTH_SYSTEM_DESC, &gpuResourceFB_DEPTH_SYSTEM_ARCH))
			pCGpuManager->GenerateGpuResource(&gpuResourceFB_DEPTH_SYSTEM_DESC, pCMergeIObject, &gpuResourceFB_DEPTH_SYSTEM_ARCH, NULL);

		pdx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gpuResourceFB_RENDEROUT_SYSTEM_ARCH.vtrPtrs.at(0), (ID3D11Texture2D*)gpuResourcesFB_ARCHs[__FB_TX2D_RENDEROUT].vtrPtrs.at(0));
		pdx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gpuResourceFB_DEPTH_SYSTEM_ARCH.vtrPtrs.at(0), (ID3D11Texture2D*)gpuResourcesFB_ARCHs[__FB_TX2D_DEPTHCS].vtrPtrs.at(0));

		D3D11_MAPPED_SUBRESOURCE mappedResSysRGBA;
		pdx11DeviceImmContext->Map((ID3D11Texture2D*)gpuResourceFB_RENDEROUT_SYSTEM_ARCH.vtrPtrs.at(0), 0, D3D11_MAP_READ, NULL, &mappedResSysRGBA);
		D3D11_MAPPED_SUBRESOURCE mappedResSysDepth;
		pdx11DeviceImmContext->Map((ID3D11Texture2D*)gpuResourceFB_DEPTH_SYSTEM_ARCH.vtrPtrs.at(0), 0, D3D11_MAP_READ, NULL, &mappedResSysDepth);

		vmbyte4* py4RenderOuts = (vmbyte4*)mappedResSysRGBA.pData;
		float* pfDeferredContexts = (float*)mappedResSysDepth.pData;
		int iGpuBufferOffset = mappedResSysRGBA.RowPitch / 4;

		int count = i2SizeScreenCurrent.y;
		if (!bIsFastQualityMode || iLevelVR == 2)
		{
#ifdef PPL_USE
			parallel_for(int(0), count, [i2SizeScreenCurrent, py4RenderOuts, pfDeferredContexts, py4ColorBufferVr, pfDepthBufferVr, iGpuBufferOffset](int i)
#else
#pragma omp parallel for 
			for (int i = 0; i < i2SizeScreenCurrent.y; i++)
#endif
			{
				for (int j = 0; j < i2SizeScreenCurrent.x; j++)
				{
					vmbyte4 y4Renderout = py4RenderOuts[j + i * iGpuBufferOffset];
					// BGRA
					py4ColorBufferVr[i*i2SizeScreenCurrent.x + j] = vmbyte4(y4Renderout.z, y4Renderout.y, y4Renderout.x, y4Renderout.w);

					int iAddr = i * i2SizeScreenCurrent.x + j;
					if (y4Renderout.w > 0)
						pfDepthBufferVr[iAddr] = pfDeferredContexts[j + i * iGpuBufferOffset];
					else
						pfDepthBufferVr[iAddr] = FLT_MAX;
				}
#ifdef PPL_USE
			});
#else
		}
#endif
	}
		else
		{
			float fReducedStepY = 1.0f / f2ReductionRatio.y;
			float fReducedStepX = 1.0f / f2ReductionRatio.x;
#ifdef PPL_USE
			parallel_for(int(0), count, [i2SizeScreenCurrent, fReducedStepX, fReducedStepY, &cbVrCamState,
				py4RenderOuts, iGpuBufferOffset, pfDeferredContexts,
				py4ColorBufferVr, pfDepthBufferVr](int i)
#else
#pragma omp parallel for 
			for (int i = 0; i < i2SizeScreenCurrent.y; i++)
#endif
			{
				if (cbVrCamState.uiScreenAaBbMinY <= (uint)i
					&& cbVrCamState.uiScreenAaBbMaxY > (uint)i)
				{
					int iY = i;
					float fY = (i - cbVrCamState.uiScreenAaBbMinY) * fReducedStepY + cbVrCamState.uiScreenAaBbMinY;

					vmbyte4* py4Color_RS_Top_VR = py4RenderOuts + cbVrCamState.uiScreenAaBbMinX + (int)fY * iGpuBufferOffset;
					float* pfDepth_RS_Top_VR = pfDeferredContexts + cbVrCamState.uiScreenAaBbMinX + (int)fY * iGpuBufferOffset;

					float fWeightY = fY - floorf(fY);
					float fInvWeightY = 1.f - fWeightY;
					float fWeightX = 0;

					int iAddr = iY * i2SizeScreenCurrent.x + cbVrCamState.uiScreenAaBbMinX;

					vmbyte4* py4Color_RS_TopLeft = py4Color_RS_Top_VR;
					vmbyte4* py4Color_RS_TopRight = py4Color_RS_TopLeft + 1;
					vmbyte4* py4Color_RS_BottomLeft = py4Color_RS_TopLeft + iGpuBufferOffset;
					vmbyte4* py4Color_RS_BottomRight = py4Color_RS_BottomLeft + 1;

					float* pfDepth_RS_TopLeft = pfDepth_RS_Top_VR;
					float* pfDepth_RS_TopRight = pfDepth_RS_TopLeft + 1;
					float* pfDepth_RS_BottomLeft = pfDepth_RS_TopLeft + iGpuBufferOffset;
					float* pfDepth_RS_BottomRight = pfDepth_RS_BottomLeft + 1;

					for (uint iX = cbVrCamState.uiScreenAaBbMinX; iX < cbVrCamState.uiScreenAaBbMaxX; iX++, iAddr++, fWeightX += fReducedStepX)
						//for (int j = cbVrCamState.uiScreenAaBbMinX; j < cbVrCamState.uiScreenAaBbMaxX; j++)
					{
						if (fWeightX >= 1.0f)
						{
							py4Color_RS_TopRight++;
							py4Color_RS_TopLeft++;
							py4Color_RS_BottomRight++;
							py4Color_RS_BottomLeft++;

							pfDepth_RS_TopRight++;
							pfDepth_RS_TopLeft++;
							pfDepth_RS_BottomRight++;
							pfDepth_RS_BottomLeft++;

							fWeightX -= 1.f;
						}
						float fInvWeightX = 1.f - fWeightX;

						vmfloat4 f4ColorTopLeft = vmfloat4((float)py4Color_RS_TopLeft->x / 255.f, (float)py4Color_RS_TopLeft->y / 255.f, (float)py4Color_RS_TopLeft->z / 255.f, (float)py4Color_RS_TopLeft->w / 255.f);
						vmfloat4 f4ColorTopRight = vmfloat4((float)py4Color_RS_TopRight->x / 255.f, (float)py4Color_RS_TopRight->y / 255.f, (float)py4Color_RS_TopRight->z / 255.f, (float)py4Color_RS_TopRight->w / 255.f);
						vmfloat4 f4ColorBottomLeft = vmfloat4((float)py4Color_RS_BottomLeft->x / 255.f, (float)py4Color_RS_BottomLeft->y / 255.f, (float)py4Color_RS_BottomLeft->z / 255.f, (float)py4Color_RS_BottomLeft->w / 255.f);
						vmfloat4 f4ColorBottomRight = vmfloat4((float)py4Color_RS_BottomRight->x / 255.f, (float)py4Color_RS_BottomRight->y / 255.f, (float)py4Color_RS_BottomRight->z / 255.f, (float)py4Color_RS_BottomRight->w / 255.f);

						vmfloat4 f4ColorTop = f4ColorTopLeft * fInvWeightX + f4ColorTopRight * fWeightX;
						vmfloat4 f4ColorBottom = f4ColorBottomLeft * fInvWeightX + f4ColorBottomRight * fWeightX;
						vmfloat4 f4ColorOut = f4ColorTop * fInvWeightY + f4ColorBottom * fWeightY;
						py4ColorBufferVr[iAddr] = vmbyte4((byte)(f4ColorOut.x * 255.f), (byte)(f4ColorOut.y * 255.f), (byte)(f4ColorOut.z * 255.f), (byte)(f4ColorOut.w * 255.f));

						float fDepthTop = *pfDepth_RS_TopLeft * fInvWeightX + *pfDepth_RS_TopRight * fWeightX;
						float fDepthBottom = *pfDepth_RS_BottomLeft * fInvWeightX + *pfDepth_RS_BottomRight * fWeightX;
						float fDepthOut = fDepthTop * fInvWeightY + fDepthBottom * fWeightY;

						pfDepthBufferVr[iAddr] = fDepthOut;
					}

					for (uint j = 0; j < cbVrCamState.uiScreenAaBbMinX; j++)
					{
						vmbyte4 y4Renderout = py4RenderOuts[j + i * iGpuBufferOffset];
						// BGRA
						int iAddr = i * i2SizeScreenCurrent.x + j;
						py4ColorBufferVr[iAddr] = vmbyte4(y4Renderout.z, y4Renderout.y, y4Renderout.x, y4Renderout.w);

						if (y4Renderout.w > 0)
							pfDepthBufferVr[iAddr] = pfDeferredContexts[j + i * iGpuBufferOffset];
						else
							pfDepthBufferVr[iAddr] = FLT_MAX;
					}

					for (int j = cbVrCamState.uiScreenAaBbMaxX; j < i2SizeScreenCurrent.x; j++)
					{
						vmbyte4 y4Renderout = py4RenderOuts[j + i * iGpuBufferOffset];
						// BGRA
						int iAddr = i * i2SizeScreenCurrent.x + j;
						py4ColorBufferVr[iAddr] = vmbyte4(y4Renderout.z, y4Renderout.y, y4Renderout.x, y4Renderout.w);

						if (y4Renderout.w > 0)
							pfDepthBufferVr[iAddr] = pfDeferredContexts[j + i * iGpuBufferOffset];
						else
							pfDepthBufferVr[iAddr] = FLT_MAX;
					}
				}
				else
				{
					for (int j = 0; j < i2SizeScreenCurrent.x; j++)
					{
						vmbyte4 y4Renderout = py4RenderOuts[j + i * iGpuBufferOffset];
						// BGRA
						int iAddr = i * i2SizeScreenCurrent.x + j;
						py4ColorBufferVr[iAddr] = vmbyte4(y4Renderout.z, y4Renderout.y, y4Renderout.x, y4Renderout.w);

						if (y4Renderout.w > 0)
							pfDepthBufferVr[iAddr] = pfDeferredContexts[j + i * iGpuBufferOffset];
						else
							pfDepthBufferVr[iAddr] = FLT_MAX;
					}
				}
#ifdef PPL_USE
			});
#else
		}
#endif
}
		pdx11DeviceImmContext->Unmap((ID3D11Texture2D*)gpuResourceFB_RENDEROUT_SYSTEM_ARCH.vtrPtrs.at(0), 0);
		pdx11DeviceImmContext->Unmap((ID3D11Texture2D*)gpuResourceFB_DEPTH_SYSTEM_ARCH.vtrPtrs.at(0), 0);
#pragma endregion
	}

	if (iLevelVR == 2)
	{
		double dOcclusionOpaqueness = 1.0;
		fncont_getparamvalue<double>(_fncontainer, &dOcclusionOpaqueness, "_double_VrOcclusionOpaqueness");
		float fOcclusionOpaqueness = (float)dOcclusionOpaqueness;

#pragma region // Level2 Mixing //
		FrameBuffer* pstFrameBufferInSr = (FrameBuffer*)pCMergeIObject->GetFrameBuffer(FrameBufferUsageRENDEROUT, 0);
		FrameBuffer* pstFrameBufferDepthInSr = (FrameBuffer*)pCMergeIObject->GetFrameBuffer(FrameBufferUsageDEPTH, 0);
		if (pstFrameBufferInSr != NULL && pstFrameBufferDepthInSr != NULL)
		{
			vmbyte4* py4ColorBufferSr = (vmbyte4*)pstFrameBufferInSr->fbuffer;
			float* pfDepthBufferSr = (float*)pstFrameBufferDepthInSr->fbuffer;

			if (bIsFastQualityMode)
			{
				memcpy(py4ColorBufferFinal, py4ColorBufferSr, sizeof(vmbyte4) * i2SizeScreenCurrent.x * i2SizeScreenCurrent.y);
				memcpy(pfDepthBufferFinal, pfDepthBufferSr, sizeof(float) * i2SizeScreenCurrent.x * i2SizeScreenCurrent.y);

				float fReducedStepY = 1.0f / f2ReductionRatio.y;
				float fReducedStepX = 1.0f / f2ReductionRatio.x;

				int count = i2AaBbMax.y - i2AaBbMin.y - 1;
#ifdef PPL_USE
				parallel_for(int(0), count, [i2SizeScreenCurrent, fReducedStepX, fReducedStepY, i2AaBbMin, i2AaBbMax, fOcclusionOpaqueness,
					py4ColorBufferVr, py4ColorBufferSr, py4ColorBufferFinal,
					pfDepthBufferVr, pfDepthBufferSr, pfDepthBufferFinal](int i)
#else
#pragma omp parallel for 
				for (int i = 0; i <= count; i++)
#endif
				{
					int iY = i2AaBbMin.y + i;
					float fY = i * fReducedStepY;

					vmbyte4* py4Color_RS_Top_VR = py4ColorBufferVr + (int)fY * i2SizeScreenCurrent.x;
					float* pfDepth_RS_Top_VR = pfDepthBufferVr + (int)fY * i2SizeScreenCurrent.x;

					int iAddr = iY * i2SizeScreenCurrent.x + i2AaBbMin.x;

					float fWeightY = fY - floorf(fY);
					float fInvWeightY = 1.f - fWeightY;
					float fWeightX = 0;

					vmbyte4* py4Color_RS_TopLeft = py4Color_RS_Top_VR;
					vmbyte4* py4Color_RS_TopRight = py4Color_RS_TopLeft + 1;
					vmbyte4* py4Color_RS_BottomLeft = py4Color_RS_TopLeft + i2SizeScreenCurrent.x;
					vmbyte4* py4Color_RS_BottomRight = py4Color_RS_BottomLeft + 1;

					float* pfDepth_RS_TopLeft = pfDepth_RS_Top_VR;
					float* pfDepth_RS_TopRight = pfDepth_RS_TopLeft + 1;
					float* pfDepth_RS_BottomLeft = pfDepth_RS_TopLeft + i2SizeScreenCurrent.x;
					float* pfDepth_RS_BottomRight = pfDepth_RS_BottomLeft + 1;

					for (int iX = i2AaBbMin.x; iX < i2AaBbMax.x; iX++, iAddr++, fWeightX += fReducedStepX)
					{
						if (fWeightX >= 1.0f)
						{
							py4Color_RS_TopRight++;
							py4Color_RS_TopLeft++;
							py4Color_RS_BottomRight++;
							py4Color_RS_BottomLeft++;

							pfDepth_RS_TopRight++;
							pfDepth_RS_TopLeft++;
							pfDepth_RS_BottomRight++;
							pfDepth_RS_BottomLeft++;

							fWeightX -= 1.f;
						}
						float fInvWeightX = 1.f - fWeightX;

						vmfloat4 f4ColorTopLeft = vmfloat4((float)py4Color_RS_TopLeft->x / 255.f, (float)py4Color_RS_TopLeft->y / 255.f, (float)py4Color_RS_TopLeft->z / 255.f, (float)py4Color_RS_TopLeft->w / 255.f);
						vmfloat4 f4ColorTopRight = vmfloat4((float)py4Color_RS_TopRight->x / 255.f, (float)py4Color_RS_TopRight->y / 255.f, (float)py4Color_RS_TopRight->z / 255.f, (float)py4Color_RS_TopRight->w / 255.f);
						vmfloat4 f4ColorBottomLeft = vmfloat4((float)py4Color_RS_BottomLeft->x / 255.f, (float)py4Color_RS_BottomLeft->y / 255.f, (float)py4Color_RS_BottomLeft->z / 255.f, (float)py4Color_RS_BottomLeft->w / 255.f);
						vmfloat4 f4ColorBottomRight = vmfloat4((float)py4Color_RS_BottomRight->x / 255.f, (float)py4Color_RS_BottomRight->y / 255.f, (float)py4Color_RS_BottomRight->z / 255.f, (float)py4Color_RS_BottomRight->w / 255.f);

						vmfloat4 f4ColorTop = f4ColorTopLeft * fInvWeightX + f4ColorTopRight * fWeightX;
						vmfloat4 f4ColorBottom = f4ColorBottomLeft * fInvWeightX + f4ColorBottomRight * fWeightX;
						vmfloat4 f4ColorVR = f4ColorTop * fInvWeightY + f4ColorBottom * fWeightY;

						float fDepthTop = *pfDepth_RS_TopLeft * fInvWeightX + *pfDepth_RS_TopRight * fWeightX;
						float fDepthBottom = *pfDepth_RS_BottomLeft * fInvWeightX + *pfDepth_RS_BottomRight * fWeightX;
						float fDepthVR = fDepthTop * fInvWeightY + fDepthBottom * fWeightY;

						vmbyte4 y4ColorSR = py4ColorBufferSr[iAddr];
						float fDepthSR = pfDepthBufferSr[iAddr];
						float fDepth1stHit;

						vmfloat4 f4ColorForward, f4ColorBackward, f4ColorMix;
						f4ColorVR *= fOcclusionOpaqueness;
						if (fDepthSR < fDepthVR)
						{
							f4ColorForward = vmfloat4((float)y4ColorSR.x / 255.f, (float)y4ColorSR.y / 255.f, (float)y4ColorSR.z / 255.f, (float)y4ColorSR.w / 255.f);
							f4ColorBackward = f4ColorVR;
							fDepth1stHit = fDepthSR;

							f4ColorMix = f4ColorForward + f4ColorBackward * (1.f - f4ColorForward.w);
						}
						else
						{
							f4ColorForward = f4ColorVR;
							f4ColorBackward = vmfloat4((float)y4ColorSR.x / 255.f, (float)y4ColorSR.y / 255.f, (float)y4ColorSR.z / 255.f, (float)y4ColorSR.w / 255.f);
							fDepth1stHit = fDepthVR;

							//if (f4ColorBackward.w == 0)
							//	f4ColorMix = f4ColorForward + f4ColorBackward * (1.f - f4ColorForward.w);
							//else
							f4ColorMix = f4ColorForward + f4ColorBackward * (1.f - f4ColorForward.w);
						}

						py4ColorBufferFinal[iAddr] = vmbyte4((byte)min((f4ColorMix.x * 255.f), 255), (byte)min((f4ColorMix.y * 255.f), 255), (byte)min((f4ColorMix.z * 255.f), 255), (byte)min((f4ColorMix.w * 255.f), 255));
						pfDepthBufferFinal[iAddr] = fDepth1stHit;
					}

#ifdef PPL_USE
				});
#else
			}
#endif
		}
			else
			{
#ifdef PPL_USE
				int count = i2SizeScreenCurrent.y;
				parallel_for(int(0), count, [i2SizeScreenCurrent, fOcclusionOpaqueness, py4ColorBufferVr, py4ColorBufferSr, py4ColorBufferFinal, pfDepthBufferVr, pfDepthBufferSr, pfDepthBufferFinal](int i)
#else
#pragma omp parallel for 
				for (int i = 0; i < i2SizeScreenCurrent.y; i++)
#endif
				{
					for (int j = 0; j < i2SizeScreenCurrent.x; j++)
					{
						int iAddr = i * i2SizeScreenCurrent.x + j;

						vmbyte4 y4ColorVR = py4ColorBufferVr[iAddr];
						vmbyte4 y4ColorSR = py4ColorBufferSr[iAddr];

						float fDepthVR = pfDepthBufferVr[iAddr];
						float fDepthSR = pfDepthBufferSr[iAddr];
						float fDepth1stHit;

						vmfloat4 f4ColorForward, f4ColorBackward, f4ColorMix;
						if (fDepthSR < fDepthVR)
						{
							f4ColorForward = vmfloat4((float)y4ColorSR.x / 255.f, (float)y4ColorSR.y / 255.f, (float)y4ColorSR.z / 255.f, (float)y4ColorSR.w / 255.f);
							f4ColorBackward = vmfloat4((float)y4ColorVR.x / 255.f, (float)y4ColorVR.y / 255.f, (float)y4ColorVR.z / 255.f, (float)y4ColorVR.w / 255.f);
							f4ColorBackward *= fOcclusionOpaqueness;
							fDepth1stHit = fDepthSR;

							f4ColorMix = f4ColorForward + f4ColorBackward * (1.f - f4ColorForward.w);
						}
						else
						{
							f4ColorForward = vmfloat4((float)y4ColorVR.x / 255.f, (float)y4ColorVR.y / 255.f, (float)y4ColorVR.z / 255.f, (float)y4ColorVR.w / 255.f);
							f4ColorBackward = vmfloat4((float)y4ColorSR.x / 255.f, (float)y4ColorSR.y / 255.f, (float)y4ColorSR.z / 255.f, (float)y4ColorSR.w / 255.f);
							f4ColorForward *= fOcclusionOpaqueness;
							fDepth1stHit = fDepthVR;

							//if (f4ColorBackward.w == 0)
							//	f4ColorMix = f4ColorForward + f4ColorBackward * (1.f - f4ColorForward.w);
							//else
							f4ColorMix = f4ColorForward + f4ColorBackward * (1.f - f4ColorForward.w);
						}

						py4ColorBufferFinal[iAddr] = vmbyte4((byte)min((f4ColorMix.x * 255.f), 255), (byte)min((f4ColorMix.y * 255.f), 255), (byte)min((f4ColorMix.z * 255.f), 255), (byte)min((f4ColorMix.w * 255.f), 255));
						pfDepthBufferFinal[iAddr] = fDepth1stHit;
					}
#ifdef PPL_USE
				});
#else
			}
#endif
	}
		}
		else
		{	// Only VR //
			if (bIsFastQualityMode)
			{
				ZeroMemory(py4ColorBufferFinal, sizeof(vmbyte4) * i2SizeScreenCurrent.x * i2SizeScreenCurrent.y);
				ZeroMemory(pfDepthBufferFinal, sizeof(vmbyte4) * i2SizeScreenCurrent.x * i2SizeScreenCurrent.y);

				float fReducedStepY = 1.0f / f2ReductionRatio.y;
				float fReducedStepX = 1.0f / f2ReductionRatio.x;
				int count = i2AaBbMax.y - i2AaBbMin.y - 1;
#ifdef PPL_USE
				parallel_for(int(0), count, [i2SizeScreenCurrent, fReducedStepX, fReducedStepY, i2AaBbMin, i2AaBbMax,
					py4ColorBufferVr, py4ColorBufferFinal, fOcclusionOpaqueness,
					pfDepthBufferVr, pfDepthBufferFinal](int i)
#else
#pragma omp parallel for 
				for (int i = 0; i <= count; i++)
#endif
				{
					int iY = i2AaBbMin.y + i;
					float fY = i * fReducedStepY;

					vmbyte4* py4Color_RS_Top_VR = py4ColorBufferVr + (int)fY * i2SizeScreenCurrent.x;
					float* pfDepth_RS_Top_VR = pfDepthBufferVr + (int)fY * i2SizeScreenCurrent.x;

					int iAddr = iY * i2SizeScreenCurrent.x + i2AaBbMin.x;

					float fWeightY = fY - floorf(fY);
					float fInvWeightY = 1.f - fWeightY;
					float fWeightX = 0;

					vmbyte4* py4Color_RS_TopLeft = py4Color_RS_Top_VR;
					vmbyte4* py4Color_RS_TopRight = py4Color_RS_TopLeft + 1;
					vmbyte4* py4Color_RS_BottomLeft = py4Color_RS_TopLeft + i2SizeScreenCurrent.x;
					vmbyte4* py4Color_RS_BottomRight = py4Color_RS_BottomLeft + 1;

					float* pfDepth_RS_TopLeft = pfDepth_RS_Top_VR;
					float* pfDepth_RS_TopRight = pfDepth_RS_TopLeft + 1;
					float* pfDepth_RS_BottomLeft = pfDepth_RS_TopLeft + i2SizeScreenCurrent.x;
					float* pfDepth_RS_BottomRight = pfDepth_RS_BottomLeft + 1;

					for (int iX = i2AaBbMin.x; iX < i2AaBbMax.x; iX++, iAddr++, fWeightX += fReducedStepX)
					{
						if (fWeightX >= 1.0f)
						{
							py4Color_RS_TopRight++;
							py4Color_RS_TopLeft++;
							py4Color_RS_BottomRight++;
							py4Color_RS_BottomLeft++;

							pfDepth_RS_TopRight++;
							pfDepth_RS_TopLeft++;
							pfDepth_RS_BottomRight++;
							pfDepth_RS_BottomLeft++;

							fWeightX -= 1.f;
						}
						float fInvWeightX = 1.f - fWeightX;

						vmfloat4 f4ColorTopLeft = vmfloat4((float)py4Color_RS_TopLeft->x / 255.f, (float)py4Color_RS_TopLeft->y / 255.f, (float)py4Color_RS_TopLeft->z / 255.f, (float)py4Color_RS_TopLeft->w / 255.f);
						vmfloat4 f4ColorTopRight = vmfloat4((float)py4Color_RS_TopRight->x / 255.f, (float)py4Color_RS_TopRight->y / 255.f, (float)py4Color_RS_TopRight->z / 255.f, (float)py4Color_RS_TopRight->w / 255.f);
						vmfloat4 f4ColorBottomLeft = vmfloat4((float)py4Color_RS_BottomLeft->x / 255.f, (float)py4Color_RS_BottomLeft->y / 255.f, (float)py4Color_RS_BottomLeft->z / 255.f, (float)py4Color_RS_BottomLeft->w / 255.f);
						vmfloat4 f4ColorBottomRight = vmfloat4((float)py4Color_RS_BottomRight->x / 255.f, (float)py4Color_RS_BottomRight->y / 255.f, (float)py4Color_RS_BottomRight->z / 255.f, (float)py4Color_RS_BottomRight->w / 255.f);

						vmfloat4 f4ColorTop = f4ColorTopLeft * fInvWeightX + f4ColorTopRight * fWeightX;
						vmfloat4 f4ColorBottom = f4ColorBottomLeft * fInvWeightX + f4ColorBottomRight * fWeightX;
						vmfloat4 f4ColorVR = f4ColorTop * fInvWeightY + f4ColorBottom * fWeightY;

						float fDepthTop = *pfDepth_RS_TopLeft * fInvWeightX + *pfDepth_RS_TopRight * fWeightX;
						float fDepthBottom = *pfDepth_RS_BottomLeft * fInvWeightX + *pfDepth_RS_BottomRight * fWeightX;
						float fDepthVR = fDepthTop * fInvWeightY + fDepthBottom * fWeightY;

						f4ColorVR *= fOcclusionOpaqueness;
						py4ColorBufferFinal[iAddr] = vmbyte4((byte)min((f4ColorVR.x * 255.f), 255), (byte)min((f4ColorVR.y * 255.f), 255), (byte)min((f4ColorVR.z * 255.f), 255), (byte)min((f4ColorVR.w * 255.f), 255));
						pfDepthBufferFinal[iAddr] = fDepthVR;
					}

#ifdef PPL_USE
				});
#else
			}
#endif
		}
			else
			{
				if (fOcclusionOpaqueness > 0)
				{
#ifdef PPL_USE
					int count = i2SizeScreenCurrent.y;
					parallel_for(int(0), count, [i2SizeScreenCurrent, fOcclusionOpaqueness, py4ColorBufferVr, py4ColorBufferFinal, pfDepthBufferVr, pfDepthBufferFinal](int i)
#else
#pragma omp parallel for 
					for (int i = 0; i < i2SizeScreenCurrent.y; i++)
#endif
					{
						for (int j = 0; j < i2SizeScreenCurrent.x; j++)
						{
							int iAddr = i * i2SizeScreenCurrent.x + j;

							vmbyte4 y4ColorVR = py4ColorBufferVr[iAddr];
							vmfloat4 f4ColorVR = vmfloat4((float)y4ColorVR.x * fOcclusionOpaqueness, (float)y4ColorVR.y * fOcclusionOpaqueness, (float)y4ColorVR.z * fOcclusionOpaqueness, (float)y4ColorVR.w * fOcclusionOpaqueness);

							py4ColorBufferFinal[iAddr] = vmbyte4((byte)(f4ColorVR.x), (byte)(f4ColorVR.y), (byte)(f4ColorVR.z), (byte)(f4ColorVR.w));
						}
#ifdef PPL_USE
					});
#else
				}
#endif
			}
				else
				{
					memcpy(py4ColorBufferFinal, py4ColorBufferVr, sizeof(vmbyte4) * i2SizeScreenCurrent.x * i2SizeScreenCurrent.y);
				}
				memcpy(pfDepthBufferFinal, pfDepthBufferVr, sizeof(float) * i2SizeScreenCurrent.x * i2SizeScreenCurrent.y);
			}
		}
#pragma endregion // Level2 Mixing //
	}

	////////////
	if (!bReusePrevRendering)
		pCOutputIObject->RegisterCustomParameter("_bool_IsPrevRenderedInFastMode", bIsFastQualityMode);

	pdx11DeviceImmContext->ClearState();

	ID3D11RenderTargetView* pdxRTVOlds[2];
	pdxRTVOlds[0] = pdxRTVOld;
	pdxRTVOlds[1] = NULL;
	pdx11DeviceImmContext->OMSetRenderTargets(2, pdxRTVOlds, pdxDSVOld);

	VMSAFE_RELEASE(pdxRTVOld);
	VMSAFE_RELEASE(pdxDSVOld);

	QueryPerformanceFrequency(&lIntFreq);
	QueryPerformanceCounter(&lIntCntEnd);

	pCMergeIObject->RegisterCustomParameter(("_int_CountMerging"), 0);
	pCMergeIObject->SetDescriptor("vismtv_inbuilt_renderergpudx module : Volume Renderer");
	pCOutputIObject->SetDescriptor("vismtv_inbuilt_renderergpudx module : Volume Renderer");

	// Time Check
	QueryPerformanceFrequency(&lIntFreq);
	QueryPerformanceCounter(&lIntCntEnd);
	if (pdRunTime)
		*pdRunTime = (lIntCntEnd.QuadPart - lIntCntStart.QuadPart) / (double)(lIntFreq.QuadPart);

#ifdef ____MY_PERFORMANCE_TEST
	double dRunTime = (*pdRunTime * 1000);
	printf("Window (%dx%d), Last-intermixing : %f ms\n", i2SizeScreenCurrent.x, i2SizeScreenCurrent.y, (float)dRunTime);
#endif

	//printf("GPU MB : %d \n", pCGpuManager->GetUsedGpuMemorySizeKiloBytesInThisManager() / 1024);

	return true;
}