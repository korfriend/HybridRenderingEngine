#include "RendererHeader_legacy.h"

enum SHADERINDEX{
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

#define __CS_5_0_SUPPORT

using namespace grd_helper_legacy;

bool RenderVrCommonCS(VmFnContainer* _fncontainer,
	VmGpuManager* pCGpuManager,
	GpuDX11CommonParametersOld* pdx11CommonParams,
	ID3D11ComputeShader** ppdx11CS_VRs[NUMSHADERS_VR_CS],
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

	VmLObject* lobj = (VmLObject*)_fncontainer->GetVmObject(VmObjKey(ObjectTypeBUFFER, true), 0);
	VmIObject* pCOutputIObject = (VmIObject*)vtrOutputIPs[0];
	VmIObject* pCMergeIObject = (VmIObject*)vtrOutputIPs[1];

	if (pCMergeIObject == NULL || pCOutputIObject == NULL || lobj == NULL)
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
	double user_sample_rate = _fncontainer->GetParamValue("_double_UserSampleRate", 0.0);
	//iLevelVR = 2;

	double v_thickness = _fncontainer->GetParamValue("_double_VZThickness", -1.0);

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

	bool is_rgba = false; // false means bgra
	fncont_getparamvalue<bool>(_fncontainer, &is_rgba, ("_bool_IsRGBA"));
	//if (bIsFastQualityMode) bIsShadowRenderer = false;

#pragma region // SHADER SETTING
	// Shader Re-Compile Setting //
	bool bReloadHLSLObjFiles = false;
	fncont_getparamvalue<bool>(_fncontainer, &bReloadHLSLObjFiles, ("_bool_ReloadHLSLObjFiles"));
	if (bReloadHLSLObjFiles)
	{
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
	}
#pragma endregion // SHADER SETTING

#pragma region // IOBJECT OUT
	while (pCOutputIObject->GetFrameBuffer(FrameBufferUsageRENDEROUT, 1) != NULL)
		pCOutputIObject->DeleteFrameBuffer(FrameBufferUsageRENDEROUT, 1);
	if (!pCOutputIObject->ReplaceFrameBuffer(FrameBufferUsageRENDEROUT, 0, data_type::dtype<vmbyte4>(),  ("common render out frame buffer : defined in vismtv_inbuilt_renderergpudx module")))
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
	vmint2 i2SizeScreenCurrent, i2SizeScreenOld = vmint2(0, 0);
	pCMergeIObject->GetFrameBufferInfo(&i2SizeScreenCurrent);
	pCMergeIObject->GetCustomParameter("_int2_PreviousScreenSize", data_type::dtype<vmint2>(), &i2SizeScreenOld);
	if (i2SizeScreenCurrent.x != i2SizeScreenOld.x || i2SizeScreenCurrent.y != i2SizeScreenOld.y)
	{
		pCGpuManager->ReleaseGpuResourcesBySrcID(pCOutputIObject->GetObjectID());	// System Out 포함 //
		pCGpuManager->ReleaseGpuResourcesBySrcID(pCMergeIObject->GetObjectID());	// System Out 포함 //
		pCMergeIObject->RegisterCustomParameter(("_int2_PreviousScreenSize"), i2SizeScreenCurrent);
	}

	GpuRes gres_fbs[__GRES_FB_V1_COUNT], gres_fb_shadowmap;
	auto set_fbs = [](GpuRes gres_fbs[__GRES_FB_V1_COUNT], const VmIObject* iobj)
	{
		for (int i = 0; i < NUM_TEXRT_LAYERS; i++)
		{
			grd_helper_legacy::UpdateFrameBuffer(gres_fbs[__GRES_FB_V1_ROUT_RGBA_0 + i], iobj, "RENDER_OUT_RGBA_" + std::to_string(i),
				RTYPE_TEXTURE2D, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, false);
			grd_helper_legacy::UpdateFrameBuffer(gres_fbs[__GRES_FB_V1_ROUT_DEPTH_0 + i], iobj, "RENDER_OUT_DEPTH_" + std::to_string(i),
				RTYPE_TEXTURE2D, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_FLOAT, false);
		}
		grd_helper_legacy::UpdateFrameBuffer(gres_fbs[__GRES_FB_V1_DOUT_DS], iobj, "DEPTH_STENCIL",
			RTYPE_TEXTURE2D, D3D11_BIND_DEPTH_STENCIL, DXGI_FORMAT_D32_FLOAT, false);
		grd_helper_legacy::UpdateFrameBuffer(gres_fbs[__GRES_FB_V1_DEEP_0], iobj, "DEEP_LAYER_0",
			RTYPE_BUFFER, D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE, DXGI_FORMAT_UNKNOWN, false, 1, sizeof(RWB_Output_Layers));
		grd_helper_legacy::UpdateFrameBuffer(gres_fbs[__GRES_FB_V1_DEEP_1], iobj, "DEEP_LAYER_1",
			RTYPE_BUFFER, D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE, DXGI_FORMAT_UNKNOWN, false, 1, sizeof(RWB_Output_Layers));
		grd_helper_legacy::UpdateFrameBuffer(gres_fbs[__GRES_FB_V1_SYS_ROUT], iobj, "SYSTEM_OUT_RGBA",
			RTYPE_TEXTURE2D, NULL, DXGI_FORMAT_R8G8B8A8_UNORM, true);
		grd_helper_legacy::UpdateFrameBuffer(gres_fbs[__GRES_FB_V1_SYS_DOUT], iobj, "SYSTEM_OUT_DEPTH",
			RTYPE_TEXTURE2D, NULL, DXGI_FORMAT_R32_FLOAT, true);
	};

	set_fbs(gres_fbs, pCMergeIObject);
	grd_helper_legacy::UpdateFrameBuffer(gres_fb_shadowmap, pCMergeIObject, "SHADOW_MAP",
		RTYPE_TEXTURE2D, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_FLOAT, false);

	ID3D11UnorderedAccessView* pUAV_Merge_PingpongCS[2] = {
		(ID3D11UnorderedAccessView*)gres_fbs[__GRES_FB_V1_DEEP_0].alloc_res_ptrs[DTYPE_UAV],
		(ID3D11UnorderedAccessView*)gres_fbs[__GRES_FB_V1_DEEP_1].alloc_res_ptrs[DTYPE_UAV] };
	ID3D11ShaderResourceView* pSRV_Merge_PingpongCS[2] = {
		(ID3D11ShaderResourceView*)gres_fbs[__GRES_FB_V1_DEEP_0].alloc_res_ptrs[DTYPE_SRV],
		(ID3D11ShaderResourceView*)gres_fbs[__GRES_FB_V1_DEEP_1].alloc_res_ptrs[DTYPE_SRV] };

	int iCountMerging = 0;
	pCMergeIObject->GetCustomParameter(("_int_CountMerging"), data_type::dtype<int>(), &iCountMerging);
#pragma endregion // IOBJECT GPU

#pragma region // Presetting of VxObject
	int last_render_vol_id = (vtrInputVolumes.at(vtrInputVolumes.size() - 1))->GetObjectID();
	fncont_getparamvalue<int>(_fncontainer, &last_render_vol_id, "_int_LastRenderVolumeID");

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
	if (!lobj->LoadBufferPtr("_vlist_INT_MainVolumeIDs", (void**)&pMainVolumeIDs, bytes_pMainVolumeIDs))
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
			if (iVolumeID == last_render_vol_id)
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
	vtrOrderedMainVolumeIDs.push_back(last_render_vol_id);
#pragma endregion // Presetting of VxObject

	// Backup Previous Render Target //
	ID3D11RenderTargetView* pdxRTVOld = NULL;
	ID3D11DepthStencilView* pdxDSVOld = NULL;
	pdx11DeviceImmContext->OMGetRenderTargets(1, &pdxRTVOld, &pdxDSVOld);

	// View List-Up //
	ID3D11ShaderResourceView* pdx11SRV_RenderOuts[NUM_TEXRT_LAYERS] = { NULL, };
	ID3D11ShaderResourceView* pdx11SRV_DepthCSs[NUM_TEXRT_LAYERS] = { NULL, };
	UINT UAVInitialCounts = 0;
	ID3D11UnorderedAccessView* pdx11UAV_RenderOuts[NUM_TEXRT_LAYERS] = { NULL, };
	ID3D11UnorderedAccessView* pdx11UAV_DepthCSs[NUM_TEXRT_LAYERS] = { NULL, };
	for (int i = 0; i < NUM_TEXRT_LAYERS; i++)
	{
		pdx11SRV_RenderOuts[i] = (ID3D11ShaderResourceView*)gres_fbs[__GRES_FB_V1_ROUT_RGBA_0 + i].alloc_res_ptrs[DTYPE_SRV];
		pdx11SRV_DepthCSs[i] = (ID3D11ShaderResourceView*)gres_fbs[__GRES_FB_V1_ROUT_DEPTH_0 + i].alloc_res_ptrs[DTYPE_SRV];

		pdx11UAV_RenderOuts[i] = (ID3D11UnorderedAccessView*)gres_fbs[__GRES_FB_V1_ROUT_RGBA_0 + i].alloc_res_ptrs[DTYPE_UAV];
		pdx11UAV_DepthCSs[i] = (ID3D11UnorderedAccessView*)gres_fbs[__GRES_FB_V1_ROUT_DEPTH_0 + i].alloc_res_ptrs[DTYPE_UAV];
	}
	ID3D11UnorderedAccessView* pdx11UAV_4NULLs[NUM_TEXRT_LAYERS] = { NULL };
	ID3D11UnorderedAccessView* pdx11UAV_NULL = NULL;

	ID3D11ShaderResourceView* pdx11SRV_4NULLs[NUM_TEXRT_LAYERS] = { NULL };
	ID3D11ShaderResourceView* pdx11SRV_2NULLs[2] = { NULL };
	uint uiClearVlaues[4] = { 0, 0, 0, 0 };
	float fClearColor[4] = { 0, 0, 0, 0 };
	float fClearDepth[4] = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };
	bool bIsFirstRender = false;
	fncont_getparamvalue<bool>(_fncontainer, &bIsFirstRender, ("_bool_IsFirstRenderer"));
	if (bIsFirstRender || iLevelVR == 2)
	{
		// Clear Merge Buffers //
		for (int i = 0; i < 2; i++)
			pdx11DeviceImmContext->ClearUnorderedAccessViewUint(pUAV_Merge_PingpongCS[i], uiClearVlaues);
		iCountMerging = 0;
	}

	// Clear RTT //
	// Clear ShadowMap Depth //
	for (int i = 0; i < NUM_TEXRT_LAYERS; i++)
	{
		pdx11DeviceImmContext->ClearUnorderedAccessViewUint(pdx11UAV_RenderOuts[i], uiClearVlaues);
		pdx11DeviceImmContext->ClearUnorderedAccessViewUint(pdx11UAV_DepthCSs[i], uiClearVlaues);
	}
	if (bIsShadowStage)
		pdx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gres_fb_shadowmap.alloc_res_ptrs[DTYPE_UAV], uiClearVlaues);
	else if (bIsShadowRenderer)
		pdx11DeviceImmContext->CSSetShaderResources(4, 1, (ID3D11ShaderResourceView**)&gres_fb_shadowmap.alloc_res_ptrs[DTYPE_SRV]);

#pragma region // Camera & Light & Shadow Setting
	uint uiNumGridX = (uint)ceil(i2SizeScreenCurrent.x / (float)__BLOCKSIZE);
	uint uiNumGridY = (uint)ceil(i2SizeScreenCurrent.y / (float)__BLOCKSIZE);

	VmCObject* pCCObject = pCMergeIObject->GetCameraObject();
	vmfloat3 f3PosEyeWS, f3VecViewWS;
	pCCObject->GetCameraExtStatef(&f3PosEyeWS, &f3VecViewWS, NULL);
	double dFarPlaneLength;
	pCCObject->GetCameraIntState(NULL, NULL, &dFarPlaneLength, NULL);

	CB_VrCameraState cbVrCamState;
	grd_helper_legacy::SetCbVrCamera(&cbVrCamState, pCCObject, i2SizeScreenCurrent, &_fncontainer->vmparams);

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


		cbVrCamState.dxmatSS2WS = *(XMMATRIX*)&matSS2WS;
		cbVrCamState.dxmatRS2SS = *(XMMATRIX*)&matRS2SS;
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
		cbVrCamState.dxmatRS2SS = XMMatrixIdentity();

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
		grd_helper_legacy::SetCbVrShadowMap(&cbShadowMap, &cbVrCamState, pos_minWS, pos_maxWS, &_fncontainer->vmparams);
	}
	else if (bIsShadowRenderer)
	{
		CB_VrCameraState cbVrCamStateShadowTemp = cbVrCamState;
		grd_helper_legacy::SetCbVrShadowMap(&cbShadowMap, &cbVrCamStateShadowTemp, pos_minWS, pos_maxWS, &_fncontainer->vmparams);
	}

	if (bIsShadowRenderer)
	{
		D3D11_MAPPED_SUBRESOURCE mappedResShadowMap;
		pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_SHADOWMAP], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResShadowMap);
		CB_VrShadowMap* pCBParamSurfaceEffect = (CB_VrShadowMap*)mappedResShadowMap.pData;
		memcpy(pCBParamSurfaceEffect, &cbShadowMap, sizeof(CB_VrShadowMap));
		pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_SHADOWMAP], 0);
		pdx11DeviceImmContext->CSSetConstantBuffers(7, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_SHADOWMAP]);
	}

	D3D11_MAPPED_SUBRESOURCE mappedResCamState;
	pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_CAMSTATE], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResCamState);
	CB_VrCameraState* pCBParamCamState = (CB_VrCameraState*)mappedResCamState.pData;
	memcpy(pCBParamCamState, &cbVrCamState, sizeof(CB_VrCameraState));
	int iTestIndex = 0;
	fncont_getparamvalue<int>(_fncontainer, &iTestIndex, ("_int_Index"));
	pCBParamCamState->iFlags |= iTestIndex << 10;
	pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_CAMSTATE], 0);
	pdx11DeviceImmContext->CSSetConstantBuffers(1, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_CAMSTATE]);
#pragma endregion // Light & Shadow Setting

	// Initial Setting of Frame Buffers //
	QueryPerformanceCounter(&lIntCntStart);

#pragma region // Common Shader Setting : Proxy Setting, Sampler Setting
	ID3D11RenderTargetView* pdx11RTVs[2] = {
		(ID3D11RenderTargetView*)gres_fbs[__GRES_FB_V1_ROUT_RGBA_0].alloc_res_ptrs[DTYPE_RTV],
		(ID3D11RenderTargetView*)gres_fbs[__GRES_FB_V1_ROUT_DEPTH_0].alloc_res_ptrs[DTYPE_RTV] };

	ID3D11RenderTargetView* pdx11RTVs_NULL[2] = { NULL };
	pdx11DeviceImmContext->OMSetRenderTargets(2, pdx11RTVs_NULL, NULL);

	uint uiStrideSizeInput = sizeof(vmfloat3);
	uint uiOffset = 0;

	pdx11DeviceImmContext->IASetVertexBuffers(0, 1, (ID3D11Buffer**)&pdx11BufProxyRect, &uiStrideSizeInput, &uiOffset);
	pdx11DeviceImmContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pdx11DeviceImmContext->IASetInputLayout(pdx11IL_ProxyRect);
	pdx11DeviceImmContext->RSSetState(pdx11CommonParams->pdx11RStates[__RState_SOLID_NONE]);
	pdx11DeviceImmContext->VSSetShader(pdx11VS_ProxyRect, NULL, 0);

	pdx11DeviceImmContext->CSSetSamplers(0, 1, &pdx11CommonParams->pdx11SStates[__SState_LINEAR_ZEROBORDER]);
	pdx11DeviceImmContext->CSSetSamplers(1, 1, &pdx11CommonParams->pdx11SStates[__SState_POINT_ZEROBORDER]);
	pdx11DeviceImmContext->CSSetSamplers(2, 1, &pdx11CommonParams->pdx11SStates[__SState_LINEAR_CLAMP]);
	pdx11DeviceImmContext->CSSetSamplers(3, 1, &pdx11CommonParams->pdx11SStates[__SState_POINT_CLAMP]);

	// Proxy Setting //
	auto itrVolume = mapVolumes.find(last_render_vol_id);
	vmfloat3 f3PitchMain = itrVolume->second->GetVolumeData()->vox_pitch;

	D3D11_MAPPED_SUBRESOURCE mappedProxyState;
	pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_PROXY], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedProxyState);
	CB_SrProxy* pCBProxyParam = (CB_SrProxy*)mappedProxyState.pData;
	pCBProxyParam->fVZThickness = min(min(f3PitchMain.x, f3PitchMain.y), f3PitchMain.z);
	pCBProxyParam->uiGridOffsetX = cbVrCamState.uiGridOffsetX;
	pCBProxyParam->uiScreenSizeX = cbVrCamState.uiScreenSizeX;
	pCBProxyParam->uiScreenSizeY = cbVrCamState.uiScreenSizeY;
	pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_PROXY], 0);
	pdx11DeviceImmContext->CSSetConstantBuffers(10, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_PROXY]);
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
		int vol_obj_id = vtrOrderedMainVolumeIDs[i];

#pragma region // Volume General Parameters
		VmVObjectVolume* main_vol_obj = NULL;

		// Volumes //
		auto itrVolume = mapVolumes.find(vol_obj_id);
		if (itrVolume == mapVolumes.end())
		{
			VMERRORMESSAGE("NO Volume ID");
			continue;
		}
		main_vol_obj = itrVolume->second;

		bool is_visible = true;
		lobj->GetDstObjValue(vol_obj_id, "_bool_IsVisible", &is_visible); // VxFramework
		lobj->GetDstObjValue(vol_obj_id, "_bool_TestVisible", &is_visible);
		lobj->GetDstObjValue(vol_obj_id, "_bool_visibility", &is_visible); // VisMotive

		bool is_invalid = bIsNoValidObjectInitValue;
		if (!is_visible)
			is_invalid = true;
		if (is_invalid)
			continue;

		int render_type = 0;
		lobj->GetDstObjValue(vol_obj_id, ("_int_RendererType"), &render_type);
		bool is_xray_mode = false;
		if (render_type >= __RM_RAYMAX)
			is_xray_mode = true;
		if (bIsShowBlock)
			render_type = __RM_SURFACEEFFECT;

		// TEST //
		int data_value_0 = 0, data_value_1 = 0, iso_value = 0;
		double dKernelSize = 0, dThresholdGradSqLength = 0, dThresholdLaplacian = 0;
		if (render_type == __RM_SURFACEEFFECT && iTestValue > 0)
		{
			lobj->GetDstObjValue(vol_obj_id, ("_int_Density0"), &data_value_0);
			lobj->GetDstObjValue(vol_obj_id, ("_int_Density1"), &data_value_1);
			lobj->GetDstObjValue(vol_obj_id, ("_int_IsoValueBoundary"), &iso_value);
			lobj->GetDstObjValue(vol_obj_id, ("_double_KernelSize"), &dKernelSize);
			lobj->GetDstObjValue(vol_obj_id, ("_double_ThresholdGradSqLength"), &dThresholdGradSqLength);
			lobj->GetDstObjValue(vol_obj_id, ("_double_ThresholdLaplacian"), &dThresholdLaplacian);

			//iRendererType = 6;
			if (iTestValue == 1 || iTestValue == 3)
				bIsShowBlock = true;
			render_type = __RM_SURFACEEFFECT;
		}

		bool bForceToSkipBlockUpdateFromVolume = false;
		lobj->GetDstObjValue(vol_obj_id, ("_bool_ForceToSkipBlockUpdate"), &bForceToSkipBlockUpdateFromVolume);

		int blk_level = 1;
		lobj->GetDstObjValue(vol_obj_id, ("_int_BlockLevel"), &blk_level);
		blk_level = max(min(1, blk_level), 0);

		int main_tobj_id = 0;
		lobj->GetDstObjValue(vol_obj_id, ("_int_MainTObjectID"), &main_tobj_id);

		int mask_vol_id = 0;
		lobj->GetDstObjValue(vol_obj_id, ("_int_MaskVolumeObjectID"), &mask_vol_id);

		// TEST 
		int windowing_tobj_id = 0;
		lobj->GetDstObjValue(vol_obj_id, ("_int_WindowingTObjectID"), &windowing_tobj_id);

#pragma endregion // Volume General Parameters

#pragma region // Mask Volume Custom Parameters
		VmVObjectVolume* mask_vol_obj = NULL;
		//pCLObjectForParameters->GetCustomDValue(iMaskVolumeID, (void**)&pmapDValueMaskVolume);
		//if (pmapDValueMaskVolume == NULL)
		//	pmapDValueMaskVolume = &mapDValueClear;

		int iSculptValue = -1;
		double* pMaskOTFIDSeries = NULL;
		double* pMaskOTFIDSeries_Visibilities = NULL;
		double* pMaskOTFIDMap = NULL;
		size_t bytes_otf_map = 0;
		if (mask_vol_id != 0 && (bValidateMaskVolume || render_type == __RM_SCULPTMASK))
		{
			auto itrMaskVolume = mapVolumes.find(mask_vol_id);
			if (itrMaskVolume == mapVolumes.end())
			{
				VMERRORMESSAGE("NO Mask Volume ID");
				continue;
			}

			mask_vol_obj = itrMaskVolume->second;
			if (mask_vol_obj == NULL)
				return false;

			// 새로운 구조로 refactorying
			lobj->GetDstObjValue(mask_vol_id, ("_int_SculptingIndex"), &iSculptValue);
			lobj->GetDstObjValueBufferPtr(mask_vol_id, ("_vlist_DOUBLE_MaskOTFIDs"), (void**)&pMaskOTFIDSeries, bytes_otf_map);
			lobj->GetDstObjValueBufferPtr(mask_vol_id, ("_vlist_DOUBLE_MaskOTFIDs_VisibilitySeries"), (void**)&pMaskOTFIDSeries_Visibilities, bytes_otf_map);
			lobj->GetDstObjValueBufferPtr(mask_vol_id, ("_vlist_DOUBLE_MaskOTFIndexMap"), (void**)&pMaskOTFIDMap, bytes_otf_map);
		}
		int iNumMaskOTFs = (int)voo::get_num_from_bytes<double>(bytes_otf_map);
		bool bIsUsedMaskOtfInRenderer = false;
		if (iNumMaskOTFs > 0)
		{
			bIsUsedMaskOtfInRenderer = true;
			// Check if Mask Volume Renderer is used or not.
			if (iNumMaskOTFs == 1 && (int)pMaskOTFIDSeries[0] == main_tobj_id)
				bIsUsedMaskOtfInRenderer = false;
		}

#pragma endregion // Mask Volume Custom Parameters

#pragma region // Main TObject Custom Parameters
		VmTObject* main_tobj = NULL;

		auto itrTObject = mapTObjects.find(main_tobj_id);
		if (itrTObject == mapTObjects.end())
		{
			VMERRORMESSAGE("NO TOBJECT ID");
			continue;
		}
		main_tobj = itrTObject->second;

		bool bIsTfChanged = false;
		lobj->GetDstObjValue(main_tobj_id, ("_bool_IsTfChanged"), &bIsTfChanged);
		if (bForcedUpdateOtf)
			bIsTfChanged = true;

		bool bForceToSkipBlockUpdateFromOTF = false;
		lobj->GetDstObjValue(main_tobj_id, ("_bool_ForceToSkipBlockUpdate"), &bForceToSkipBlockUpdateFromOTF);

		bool bUpdateBricks = false;
		lobj->GetDstObjValue(main_tobj_id, ("_bool_UpdateBricks"), &bUpdateBricks);

		bool bForceToSkipBlockUpdate = bForceToSkipBlockUpdateFromOTF || bForceToSkipBlockUpdateFromVolume;

		if (bIsUsedMaskOtfInRenderer)
		{
			for (int j = 0; j < iNumMaskOTFs; j++)
			{
				int iMaskOTFID = (int)pMaskOTFIDSeries[j];
				bool bIsMaskTfChanged = false;
				lobj->GetDstObjValue(iMaskOTFID, "_bool_IsTfChanged", &bIsMaskTfChanged);
				bIsTfChanged |= bIsMaskTfChanged;
			}
		}
#pragma endregion // Main TObject Custom Parameters

#pragma region // Volume Resource Setting 
		GpuRes gres_main_vol;
		grd_helper_legacy::UpdateVolumeModel(gres_main_vol, main_vol_obj, render_type == __RM_SURFACECCF);
#pragma endregion // Volume Resource Setting 

#pragma region // Mask Volume Resource Setting
		if (mask_vol_obj != NULL)
		{
			GpuRes gres_mask_vol;
			grd_helper_legacy::UpdateVolumeModel(gres_mask_vol, mask_vol_obj, true);

			// Intel HD 에서는 BYTE 를 제대로 못 읽는다!! -_-;.... // 20140303 왜 못 읽음 -_-; 잘 되는 것으로 다시 확인
			// normalize to original... precision bug! 가 HD 4000 에서 있는 것을 확인 // 20160523
			pdx11DeviceImmContext->CSSetShaderResources(8, 1, (ID3D11ShaderResourceView**)&gres_mask_vol.alloc_res_ptrs[DTYPE_SRV]);
		}
#pragma endregion // Mask Volume Resource Setting

#pragma region // TObject Resource Setting 
		GpuRes gres_otf;
		int iNumPrevMaskOTFs = 0;
		main_tobj->GetCustomParameter("_int_NumOTFSeries", data_type::dtype<int>(), &iNumPrevMaskOTFs);
		bool bMaskUpdate = false;
		if (iNumPrevMaskOTFs != iNumMaskOTFs && bIsUsedMaskOtfInRenderer)
		{
			bMaskUpdate = true;
			gres_otf.vm_src_id = main_tobj->GetObjectID();
			gres_otf.res_name = "OTF_BUFFER";
			pCGpuManager->ReleaseGpuResource(gres_otf);
			main_tobj->RegisterCustomParameter("_int_NumOTFSeries", iNumMaskOTFs);
		}
		grd_helper_legacy::UpdateTMapBuffer(gres_otf, main_tobj, mapTObjects, 
			pMaskOTFIDSeries, pMaskOTFIDSeries_Visibilities, iNumMaskOTFs, bIsTfChanged);

		// TEST 
		if (iTestValue == 1 && windowing_tobj_id > 0)
		{
			auto itrTObject = mapTObjects.find(windowing_tobj_id);
			if (itrTObject == mapTObjects.end())
			{
				VMERRORMESSAGE("NO TOBJECT ID");
				continue;
			}

			bool bIsWindowingChanged = false;
			lobj->GetDstObjValue(windowing_tobj_id, ("_bool_IsTfChanged"), &bIsWindowingChanged);
			bIsTfChanged |= bIsWindowingChanged;

			VmTObject* windowing_tobj = itrTObject->second;

			GpuRes gres_windowing;
			grd_helper_legacy::UpdateTMapBuffer(gres_windowing, windowing_tobj, mapTObjects, NULL, NULL, 1, bIsWindowingChanged);
			pdx11DeviceImmContext->CSSetShaderResources(9, 1, (ID3D11ShaderResourceView**)&gres_windowing.alloc_res_ptrs[DTYPE_SRV]);
		}

		GpuRes gres_otf_series_ids;
		if (bIsUsedMaskOtfInRenderer)
		{
			gres_otf_series_ids.vm_src_id = main_tobj->GetObjectID();
			gres_otf_series_ids.res_name = string("OTF_SERIES_IDS");
			gres_otf_series_ids.rtype = RTYPE_BUFFER;
			gres_otf_series_ids.options["USAGE"] = D3D11_USAGE_DYNAMIC;
			gres_otf_series_ids.options["CPU_ACCESS_FLAG"] = D3D11_CPU_ACCESS_WRITE;
			gres_otf_series_ids.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE;
			gres_otf_series_ids.options["FORMAT"] = DXGI_FORMAT_R32_SINT;
			gres_otf_series_ids.res_dvalues["NUM_ELEMENTS"] = 128;
			gres_otf_series_ids.res_dvalues["STRIDE_BYTES"] = sizeof(int);

			if (bMaskUpdate)
				pCGpuManager->ReleaseGpuResource(gres_otf_series_ids);

			if (!pCGpuManager->UpdateGpuResource(gres_otf_series_ids))
				pCGpuManager->GenerateGpuResource(gres_otf_series_ids);

			D3D11_MAPPED_SUBRESOURCE d11MappedRes;
			pdx11DeviceImmContext->Map((ID3D11Resource*)gres_otf_series_ids.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes);
			int* pi4OtfIndex = (int*)d11MappedRes.pData;
			memset(pi4OtfIndex, 0, sizeof(int) * 128);
			for (int j = 0; j < iNumMaskOTFs; j++)
			{
				int iIndexOtf = static_cast<int>(pMaskOTFIDMap[j]);
				pi4OtfIndex[iIndexOtf] = j;
			}
			pdx11DeviceImmContext->Unmap((ID3D11Resource*)gres_otf_series_ids.alloc_res_ptrs[DTYPE_RES], 0);
		}
#pragma endregion // TObject Resource Setting 

#pragma region // Main Volume Block Mask Setting From OTFs
		VolumeBlocks* vol_blk = main_vol_obj->GetVolumeBlock(blk_level);
		if (vol_blk == NULL)
		{
			main_vol_obj->UpdateVolumeMinMaxBlocks();
			vol_blk = main_vol_obj->GetVolumeBlock(blk_level);
		}

		GpuRes gres_volblk_otf, gres_volblk_min, gres_volblk_max;
		if (is_xray_mode)
			grd_helper_legacy::UpdateMinMaxBlocks(gres_volblk_min, gres_volblk_max, main_vol_obj);
		
		// this is always used even when MIP mode
		grd_helper_legacy::UpdateOtfBlocks(gres_volblk_otf, main_vol_obj, mask_vol_obj, mapTObjects, main_tobj->GetObjectID(), pMaskOTFIDMap, iNumMaskOTFs,
				(bIsTfChanged | bUpdateBricks | bBlockUpdateForSculptMask) && !bForceToSkipBlockUpdate, bIsUsedMaskOtfInRenderer, iSculptValue);
#pragma endregion // Main Block Mask Setting From OTFs

#pragma region // Constant Buffers
		bool high_samplerate = gres_main_vol.res_dvalues["SAMPLE_OFFSET_X"] > 1.f ||
			gres_main_vol.res_dvalues["SAMPLE_OFFSET_Y"] > 1.f ||
			gres_main_vol.res_dvalues["SAMPLE_OFFSET_Z"] > 1.f;
		
		if (render_type == __RM_MODULATION && high_samplerate)
			high_samplerate = false;

		CB_VrVolumeObject cbVrVolumeObj;
		vmint3 vol_sampled_size = vmint3(gres_main_vol.res_dvalues["WIDTH"], gres_main_vol.res_dvalues["HEIGHT"], gres_main_vol.res_dvalues["DEPTH"]);
		grd_helper_legacy::SetCbVrObject(&cbVrVolumeObj, high_samplerate, main_vol_obj, pCCObject, vol_sampled_size, lobj, vol_obj_id, (float)v_thickness);
		if (user_sample_rate > 0)
			cbVrVolumeObj.fSampleDistWS /= (float)user_sample_rate;
		
		cbVrVolumeObj.iFlags |= iSculptValue << 24;
		D3D11_MAPPED_SUBRESOURCE mappedResVolObj;
		pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_VOLOBJ], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResVolObj);
		CB_VrVolumeObject* pCBParamVolumeObj = (CB_VrVolumeObject*)mappedResVolObj.pData;
		memcpy(pCBParamVolumeObj, &cbVrVolumeObj, sizeof(CB_VrVolumeObject));
		pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_VOLOBJ], 0);
		pdx11DeviceImmContext->CSSetConstantBuffers(0, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_VOLOBJ]);

		CB_VrOtf1D cbVrOtf1D;
		grd_helper_legacy::SetCbVrOtf1D(&cbVrOtf1D, main_tobj, 1, &_fncontainer->vmparams);
		if (iTestValue == 2 || iTestValue == 3)
			cbVrOtf1D.iOtf1stValue = iso_value;	// TEST
		D3D11_MAPPED_SUBRESOURCE mappedResOtf1D;
		pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_OTF1D], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResOtf1D);
		CB_VrOtf1D* pCBParamOtf1D = (CB_VrOtf1D*)mappedResOtf1D.pData;
		memcpy(pCBParamOtf1D, &cbVrOtf1D, sizeof(CB_VrOtf1D));
		pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_OTF1D], 0);
		pdx11DeviceImmContext->CSSetConstantBuffers(2, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_OTF1D]);

		CB_VrBlockObject cbVrBlock;
		grd_helper_legacy::SetCbVrBlockObj(&cbVrBlock, main_vol_obj, blk_level, gres_volblk_otf.options["FORMAT"] == DXGI_FORMAT_R16_UNORM ? 65535.f : 1.f);
		D3D11_MAPPED_SUBRESOURCE mappedResBlock;
		pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_BLOCKOBJ], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResBlock);
		CB_VrBlockObject* pCBParamBlock = (CB_VrBlockObject*)mappedResBlock.pData;
		memcpy(pCBParamBlock, &cbVrBlock, sizeof(CB_VrBlockObject));
		pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_BLOCKOBJ], 0);
		pdx11DeviceImmContext->CSSetConstantBuffers(3, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_BLOCKOBJ]);

		bool bIsMarkerMode = false;
		switch (render_type)
		{
		case __RM_SURFACECURVATURE:
		case __RM_SURFACEEFFECT:
		case __RM_SURFACECCF:
		{
			CB_VrSurfaceEffect cbVrSurfaceEffect;
			grd_helper_legacy::SetCbVrSurfaceEffect(&cbVrSurfaceEffect, cbVrVolumeObj.fSampleDistWS, lobj, vol_obj_id);
			// TEST
			if (iTestValue == 1)
			{
				cbVrSurfaceEffect.fDummy0 = (float)data_value_0;
				cbVrSurfaceEffect.fDummy1 = (float)data_value_1;
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
			pdx11DeviceImmContext->CSSetConstantBuffers(4, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_SURFACEEFFECT]);

			CB_VrInterestingRegion cbVrInterestingRegion;
			grd_helper_legacy::SetCbVrInterestingRegion(&cbVrInterestingRegion, lobj, vol_obj_id);
			if (cbVrInterestingRegion.iNumRegions > 0)
			{
				D3D11_MAPPED_SUBRESOURCE mappedResInterstingRegion;
				pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_INTERESTINGREGION], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResInterstingRegion);
				CB_VrInterestingRegion* pCBParamInterestingRegion = (CB_VrInterestingRegion*)mappedResInterstingRegion.pData;
				memcpy(pCBParamInterestingRegion, &cbVrInterestingRegion, sizeof(CB_VrInterestingRegion));
				pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_INTERESTINGREGION], 0);
				pdx11DeviceImmContext->CSSetConstantBuffers(8, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_INTERESTINGREGION]);
				bIsMarkerMode = true;
			}
		}
		break;
		case __RM_MODULATION:
		{
			CB_VrModulation cbVrModulation;
			grd_helper_legacy::SetCbVrModulation(&cbVrModulation, main_vol_obj, f3PosEyeWS, lobj, vol_obj_id);
			cbVrModulation.fdummy____0 = 1;

			D3D11_MAPPED_SUBRESOURCE mappedResModulation;
			pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_MODULATION], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResModulation);
			CB_VrModulation* pCBParamModulation = (CB_VrModulation*)mappedResModulation.pData;
			memcpy(pCBParamModulation, &cbVrModulation, sizeof(CB_VrSurfaceEffect));
			pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_MODULATION], 0);
			pdx11DeviceImmContext->CSSetConstantBuffers(5, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_MODULATION]);
		}
		break;
		}
#pragma endregion // Constant Buffers

#pragma region // Resource Shader Setting
		pdx11DeviceImmContext->CSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&gres_otf.alloc_res_ptrs[DTYPE_SRV]);
		if (bIsUsedMaskOtfInRenderer)
			pdx11DeviceImmContext->CSSetShaderResources(3, 1, (ID3D11ShaderResourceView**)&gres_otf_series_ids.alloc_res_ptrs[DTYPE_SRV]);

		if (is_xray_mode)
		{
			if (render_type == __RM_RAYMAX)	// Min
				pdx11DeviceImmContext->CSSetShaderResources(2, 1, (ID3D11ShaderResourceView**)&gres_volblk_max.alloc_res_ptrs[DTYPE_SRV]);
			else if(render_type == __RM_RAYMIN)
				pdx11DeviceImmContext->CSSetShaderResources(2, 1, (ID3D11ShaderResourceView**)&gres_volblk_min.alloc_res_ptrs[DTYPE_SRV]);
		}
		else
			pdx11DeviceImmContext->CSSetShaderResources(2, 1, (ID3D11ShaderResourceView**)&gres_volblk_otf.alloc_res_ptrs[DTYPE_SRV]);

		pdx11DeviceImmContext->CSSetShaderResources(1, 1, (ID3D11ShaderResourceView**)&gres_main_vol.alloc_res_ptrs[DTYPE_SRV]);
#pragma endregion // Resource Shader Setting

#pragma region // Renderer
		if (bIsShadowStage)
		{
			if (bIsUsedMaskOtfInRenderer)
				pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[__SHADER_DVR_BASIC_OTFMASK_SHADOWMAP], NULL, 0);
			else
				pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[__SHADER_DVR_BASIC_DEFAULT_SHADOWMAP], NULL, 0);

			ID3D11UnorderedAccessView* pdx11UAV_ShadowMap = (ID3D11UnorderedAccessView*)gres_fb_shadowmap.alloc_res_ptrs[DTYPE_UAV];
			pdx11DeviceImmContext->CSSetUnorderedAccessViews(1, 1, &pdx11UAV_ShadowMap, &UAVInitialCounts);

			pdx11DeviceImmContext->Dispatch(uiNumGridX, uiNumGridY, 1);
			pdx11DeviceImmContext->Flush();
			pdx11DeviceImmContext->CSSetUnorderedAccessViews(1, 1, &pdx11UAV_NULL, NULL);

			break; // Exit
		}
		else // Rendering Stage
		{
			bool bIsOnTheFlyMixing = false; // Layers
			if (iLevelVR != 2 &&
				render_type != __RM_SURFACEEFFECT &&
				render_type != __RM_SURFACECCF &&
				render_type != __RM_SURFACECURVATURE &&
				render_type != __RM_RAYMAX && render_type != __RM_RAYMIN && render_type != __RM_RAYSUM &&
				i == iNumMainVolumes - 1 && !bIsShowBlock
				)
				bIsOnTheFlyMixing = true;

			if (bIsOnTheFlyMixing)
			{
				int iShaderIndex = 0;
#pragma region RENDERING MODE
				switch (render_type)
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
					int iRSA_Index_Offset_Prev = RTTandLayersToLayersCS(pdx11DeviceImmContext, uiNumGridX, uiNumGridY,
						pdx11SRV_RenderOuts, pdx11SRV_DepthCSs, iCountMerging,
						pUAV_Merge_PingpongCS, pSRV_Merge_PingpongCS,
						pdx11CommonParams, ppdx11CS_Merges, pdx11SRV_4NULLs, pdx11SRV_2NULLs, NULL, iMergeLevel);

					// Clear //
					pdx11DeviceImmContext->ClearUnorderedAccessViewUint(pUAV_Merge_PingpongCS[iRSA_Index_Offset_Prev], uiClearVlaues);

					iCountMerging++;
					iCountMergingInVR++;
				}

				int iRSA_Index_Offset_Prev_LayerOut = 0;
				if (iCountMerging % 2 == 0)
					iRSA_Index_Offset_Prev_LayerOut = 1;

				ID3D11ShaderResourceView* pdx11SRV_Layers = pSRV_Merge_PingpongCS[iRSA_Index_Offset_Prev_LayerOut];
				pdx11DeviceImmContext->CSSetShaderResources(50, 1, &pdx11SRV_Layers);

				ID3D11UnorderedAccessView* pdx11UAV_Renderout = (ID3D11UnorderedAccessView*)gres_fbs[__GRES_FB_V1_ROUT_RGBA_0].alloc_res_ptrs[DTYPE_UAV];
				ID3D11UnorderedAccessView* pdx11UAV_DepthCS = (ID3D11UnorderedAccessView*)gres_fbs[__GRES_FB_V1_ROUT_DEPTH_0].alloc_res_ptrs[DTYPE_UAV];

				pdx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, &pdx11UAV_Renderout, &UAVInitialCounts);
				pdx11DeviceImmContext->CSSetUnorderedAccessViews(2, 1, &pdx11UAV_DepthCS, &UAVInitialCounts);
				pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[iShaderIndex], NULL, 0);

				pdx11DeviceImmContext->Flush();
				pdx11DeviceImmContext->Dispatch(uiNumGridX, uiNumGridY, 1);

				bIsCalledOnTheFlyMixing = true;
			}
			else // Single Out
			{
				int iShaderIndex = 0;
#pragma region RENDERING MODE
				if (bIsShowBlock) iShaderIndex = __SHADER_DVR_TEST;
				else
				{
					switch (render_type)
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

				ID3D11UnorderedAccessView* pdx11UAV_Renderout = (ID3D11UnorderedAccessView*)gres_fbs[__GRES_FB_V1_ROUT_RGBA_0 + iIndexRTT].alloc_res_ptrs[DTYPE_UAV];
				ID3D11UnorderedAccessView* pdx11UAV_DepthCS = (ID3D11UnorderedAccessView*)gres_fbs[__GRES_FB_V1_ROUT_DEPTH_0 + iIndexRTT].alloc_res_ptrs[DTYPE_UAV];
				pdx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, &pdx11UAV_Renderout, &UAVInitialCounts);
				pdx11DeviceImmContext->CSSetUnorderedAccessViews(2, 1, &pdx11UAV_DepthCS, &UAVInitialCounts);

				pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[iShaderIndex], NULL, 0);
				if (iTestIndex > 0)
					pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[__SHADER_DVR_TEST], NULL, 0);
				pdx11DeviceImmContext->Dispatch(uiNumGridX, uiNumGridY, 1);

				if (iModRTLayer == 0)
				{
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
			RTTandLayersToLayersCS(pdx11DeviceImmContext, uiNumGridX, uiNumGridY,
				pdx11SRV_RenderOuts, pdx11SRV_DepthCSs, iCountMerging,
				pUAV_Merge_PingpongCS, pSRV_Merge_PingpongCS,
				pdx11CommonParams, ppdx11CS_Merges, pdx11SRV_4NULLs, pdx11SRV_2NULLs, NULL, iMergeLevel);
	
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

			ID3D11ShaderResourceView* pdx11SRV_Layers = pSRV_Merge_PingpongCS[iRSA_Index_Offset_Prev];
			pdx11DeviceImmContext->CSSetShaderResources(50, 1, &pdx11SRV_Layers);

			ID3D11UnorderedAccessView* pdx11UAV_Renderout = (ID3D11UnorderedAccessView*)gres_fbs[__GRES_FB_V1_ROUT_RGBA_0].alloc_res_ptrs[DTYPE_UAV];
			ID3D11UnorderedAccessView* pdx11UAV_DepthCS = (ID3D11UnorderedAccessView*)gres_fbs[__GRES_FB_V1_ROUT_DEPTH_0].alloc_res_ptrs[DTYPE_UAV];

			UINT UAVInitialCounts = 0;
			pdx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, &pdx11UAV_Renderout, &UAVInitialCounts);
			pdx11DeviceImmContext->CSSetUnorderedAccessViews(2, 1, &pdx11UAV_DepthCS, &UAVInitialCounts);
			pdx11DeviceImmContext->CSSetShader(*ppdx11CS_Merges[__CS_MERGE_LAYERS_TO_RENDEROUT], NULL, 0);

			pdx11DeviceImmContext->Dispatch(uiNumGridX, uiNumGridY, 1);
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

	// consider vxConvertColorToInt... ARGB ... ==> bgra

	if (!bReusePrevRendering || iLevelVR != 2 || iLastRenderedVr != 2)
	{
#pragma region // Copy GPU to CPU
		pdx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gres_fbs[__GRES_FB_V1_SYS_ROUT].alloc_res_ptrs[DTYPE_RES],
			(ID3D11Texture2D*)gres_fbs[__GRES_FB_V1_ROUT_RGBA_0].alloc_res_ptrs[DTYPE_RES]);
		pdx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gres_fbs[__GRES_FB_V1_SYS_DOUT].alloc_res_ptrs[DTYPE_RES],
			(ID3D11Texture2D*)gres_fbs[__GRES_FB_V1_ROUT_DEPTH_0].alloc_res_ptrs[DTYPE_RES]);

		D3D11_MAPPED_SUBRESOURCE mappedResSysRGBA;
		D3D11_MAPPED_SUBRESOURCE mappedResSysDepth;
		pdx11DeviceImmContext->Map((ID3D11Texture2D*)gres_fbs[__GRES_FB_V1_SYS_ROUT].alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, NULL, &mappedResSysRGBA);
		pdx11DeviceImmContext->Map((ID3D11Texture2D*)gres_fbs[__GRES_FB_V1_SYS_DOUT].alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, NULL, &mappedResSysDepth);

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
					//py4ColorBufferVr[i*i2SizeScreenCurrent.x + j] = vmbyte4(255, 0, 0, 255);

					int iAddr = i*i2SizeScreenCurrent.x + j;
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
					&& cbVrCamState.uiScreenAaBbMaxY >(uint)i)
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
						int iAddr = i*i2SizeScreenCurrent.x + j;
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
						int iAddr = i*i2SizeScreenCurrent.x + j;
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
						int iAddr = i*i2SizeScreenCurrent.x + j;
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
		pdx11DeviceImmContext->Unmap((ID3D11Texture2D*)gres_fbs[__GRES_FB_V1_SYS_ROUT].alloc_res_ptrs[DTYPE_RES], 0);
		pdx11DeviceImmContext->Unmap((ID3D11Texture2D*)gres_fbs[__GRES_FB_V1_SYS_DOUT].alloc_res_ptrs[DTYPE_RES], 0);
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
						int iAddr = i*i2SizeScreenCurrent.x + j;

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
							int iAddr = i*i2SizeScreenCurrent.x + j;

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