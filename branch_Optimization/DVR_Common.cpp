#include "RendererHeader.h"
#define __FB_UBUF_RENDEROUT 0
#define __FB_UAV_RENDEROUT 1
#define __FB_SRV_RENDEROUT 2
#define __FB_UBUF_SYSTEM_RENDEROUT 3
#define __FB_UBUF_SHADOWMAP 4
#define __FB_UAV_SHADOWMAP 5
#define __FB_SRV_SHADOWMAP 6
#define __FB_COUNT_CS 7
enum SHADERINDEX{
	__CS_VOLUME_MIX_DEFAULT = 0,
	__CS_VOLUME_MIX_DEFAULT_HOMOGENEOUS,
	__CS_VOLUME_MIX_MASK_DEFAULT,
	__CS_VOLUME_MIX_CLIPSEMIOPAQUE,
	__CS_VOLUME_MIX_MASK_CLIPSEMIOPAQUE,
	__CS_VOLUME_MIX_MODULATION,
	__CS_VOLUME_MIX_MASK_MODULATION,
	__CS_VOLUME_MIX_SHADOW,
	__CS_VOLUME_MIX_MASK_SHADOW,
	__CS_VOLUME_MIX_SCULPTMASK,
	__CS_VOLUME_MIX_SIMULTANEOUS,
	__CS_VOLUME_MIX_SIMULTANEOUS_THREE,
	__CS_BLOCK_IND_SHOW,
	__CS_VOLUME_IND_DEFAULT,
	__CS_VOLUME_IND_DEFAULT_HOMOGENEOUS,
	__CS_VOLUME_IND_MASK_DEFAULT,
	__CS_VOLUME_IND_CLIPSEMIOPAQUE,
	__CS_VOLUME_IND_MASK_CLIPSEMIOPAQUE,
	__CS_VOLUME_IND_MODULATION,
	__CS_VOLUME_IND_MASK_MODULATION,
	__CS_VOLUME_IND_SHADOW,
	__CS_VOLUME_IND_MASK_SHADOW,
	__CS_VOLUME_IND_SCULPTMASK,
	__CS_VOLUME_IND_SURFACEEFFECT,
	__CS_VOLUME_IND_MASK_SURFACEEFFECT,
	__CS_VOLUME_IND_SURFACECURVATURE,
	__CS_VOLUME_IND_MASK_SURFACECURVATURE,
	__CS_VOLUME_IND_SURFACEEFFECT_MARKER, //
	__CS_VOLUME_IND_MASK_SURFACEEFFECT_MARKER,//
	__CS_VOLUME_IND_SURFACEEFFECT_SHADOW,
	__CS_VOLUME_IND_MASK_SURFACEEFFECT_SHADOW,
	__CS_VOLUME_IND_SURFACEEFFECT_SHADOW_MARKER,//
	__CS_VOLUME_IND_MASK_SURFACEEFFECT_SHADOW_MARKER,//
	__CS_VOLUME_IND_CCF_SURFACEEFFECT,
	__CS_VOLUME_IND_RAYMAX,
	__CS_VOLUME_IND_MASK_RAYMAX,
	__CS_VOLUME_IND_RAYMIN,
	__CS_VOLUME_IND_MASK_RAYMIN,
	__CS_VOLUME_IND_RAYSUM,
	__CS_VOLUME_IND_MASK_RAYSUM,
	__CS_BRICK_MIX_DEFAULT,
	__CS_BRICK_MIX_SHADOW,
	__CS_BRICK_IND_DEFAULT,
	__CS_BRICK_IND_SHADOW,
	__CS_BRICK_IND_SURFACEEFFECT,
	__CS_BRICK_IND_SURFACEEFFECT_SHADOW,
	__CS_BRICK_IND_CCF_SURFACEEFFECT,
	__CS_VOLUME_GEN_SHADOW,
	__CS_VOLUME_GEN_MASK_SHADOW,
	__CS_VOLUME_GEN_SURFACE_SHADOW,
	__CS_VOLUME_GEN_MASK_SURFACE_SHADOW,
	__CS_BRICK_GEN_SHADOW,
	__CS_BRICK_GEN_SURFACE_SHADOW,
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

map<int, vector<CVXVObjectVolume*>> g_mapStoryArchive_Volumes;
map<int, vector<CVXTObject*>> g_mapStoryArchive_TObjects;

bool RenderVrCommon(SVXModuleParameters* pstModuleParameter,
	CVXGPUManager* pCGpuManager,
	GpuDX11CommonParameters* pdx11CommonParams,
	ID3D11ComputeShader** ppdx11CS_VRs[NUMSHADERS_VR],
	ID3D11ComputeShader** ppdx11CS_Merge[NUMSHADERS_MERGE],
	bool bIsShadowStage, bool bSkipProcess,
	SVXLocalProgress* psvxLocalProgress,
	double* pdRunTime)
{
	LARGE_INTEGER lIntFreq;
	LARGE_INTEGER lIntCntStart, lIntCntEnd;

	// Get VXObjects //
	vector<CVXObject*> vtrInputVolumes;
	VXHStringGetVXObjectListFromObjectStringMap(&vtrInputVolumes, &pstModuleParameter->mapVXObjects, vxrObjectTypeVOLUME, true);
	vector<CVXObject*> vtrInputOTFs;
	VXHStringGetVXObjectListFromObjectStringMap(&vtrInputOTFs, &pstModuleParameter->mapVXObjects, vxrObjectTypeTRANSFERFUNCTION, true);
	vector<CVXObject*> vtrOutputIPs;
	VXHStringGetVXObjectListFromObjectStringMap(&vtrOutputIPs, &pstModuleParameter->mapVXObjects, vxrObjectTypeIMAGEPLANE, false);

	CVXLObject* pCLObjectForParamters = (CVXLObject*)pstModuleParameter->GetVXObject(vxrObjectTypeCUSTOMLIST, true, 0);
	CVXIObject* pCOutputIObject = (CVXIObject*)vtrOutputIPs[0];
	CVXIObject* pCMergeIObject = (CVXIObject*)vtrOutputIPs[1];

	if (pCMergeIObject == NULL || pCOutputIObject == NULL || pCLObjectForParamters == NULL)
	{
		VXERRORMESSAGE("Some of required VXObjects are missed!");
		return false;
	}

	// Check Parameters //
	bool bForcedUpdateOtf = false;
	pstModuleParameter->GetCustomParameter(&bForcedUpdateOtf, _T("_bool_ForceToUpdateOtf"));
	bool bIsSystemOut = true;
	pstModuleParameter->GetCustomParameter(&bIsSystemOut, _T("_bool_IsSystemOut"));
	bool bIsShowBlock = false;
	pstModuleParameter->GetCustomParameter(&bIsShowBlock, _T("_bool_IsShowBlock"));
	bool bIsSimultaenousMode = false;
	pstModuleParameter->GetCustomParameter(&bIsSimultaenousMode, _T("_bool_IsSimultaenousMode"));
	bool bIsShadowRenderer = false;
	pstModuleParameter->GetCustomParameter(&bIsShadowRenderer, _T("_bool_IsShadow"));
	bool bIsNoValidObjectInitValue = false;
	pstModuleParameter->GetCustomParameter(&bIsNoValidObjectInitValue, _T("_bool_IsNoValidObject"));
	bool bIsForcedDeferredMode = false;
	pstModuleParameter->GetCustomParameter(&bIsForcedDeferredMode, _T("_bool_IsForcedDeferredMode"));
	bool b_TEST_IsHomogeneous = false;
	pstModuleParameter->GetCustomParameter(&b_TEST_IsHomogeneous, _T("_bool_TestIsHomogeneous"));
	bool bValidateMaskVolume = false;
	pstModuleParameter->GetCustomParameter(&bValidateMaskVolume, _T("_bool_ValidateMaskVolume"));
	
#pragma region // SHADER SETTING
	// Shader Re-Compile Setting //
	bool bReloadHLSLObjFiles = false;
	pstModuleParameter->GetCustomParameter(&bReloadHLSLObjFiles, _T("_bool_ReloadHLSLObjFiles"));
	if (bReloadHLSLObjFiles)
	{
		wstring strNames_VR[NUMSHADERS_VR] = {
			_T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_MIX_DEFAULT_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_MIX_DEFAULT_HOMOGENEOUS_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_MIX_MASK_DEFAULT_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_MIX_CLIPSEMIOPAQUE_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_MIX_MASK_CLIPSEMIOPAQUE_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_MIX_MODULATION_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_MIX_MASK_MODULATION_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_MIX_SHADOW_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_MIX_MASK_SHADOW_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_MIX_DEFAULT_SCULPTMASK_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_MIX_SIMULTANEOUS_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_MIX_SIMULTANEOUS_THREE_cs_4_0")

			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_BLOCK_IND_SHOW_cs_4_0")

			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_IND_DEFAULT_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_IND_DEFAULT_HOMOGENEOUS_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_IND_MASK_DEFAULT_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_IND_CLIPSEMIOPAQUE_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_IND_MASK_CLIPSEMIOPAQUE_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_IND_MODULATION_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_IND_MASK_MODULATION_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_IND_SHADOW_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_IND_MASK_SHADOW_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_IND_DEFAULT_SCULPTMASK_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_IND_SURFACEEFFECT_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_IND_MASK_SURFACEEFFECT_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_IND_SURFACECURVATURE_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_IND_MASK_SURFACECURVATURE_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_IND_SURFACEEFFECT_MARKERSONSURFACE_cs_4_0")//
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_IND_MASK_SURFACEEFFECT_MARKERSONSURFACE_cs_4_0")//
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_IND_SURFACEEFFECT_SHADOW_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_IND_MASK_SURFACEEFFECT_SHADOW_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_IND_SURFACEEFFECT_SHADOW_MARKERSONSURFACE_cs_4_0")//
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_IND_MASK_SURFACEEFFECT_MARKERSONSURFACE_SHADOW_cs_4_0")//
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_IND_CCF_SURFACEEFFECT_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_IND_RAYMAX_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_IND_MASK_RAYMAX_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_IND_RAYMIN_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_IND_MASK_RAYMIN_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_IND_RAYSUM_cs_4_0")
			, _T("..\\.A.\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_IND_MASK_RAYSUM_cs_4_0")

			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_BRICK_MIX_DEFAULT_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_BRICK_MIX_SHADOW_cs_4_0")

			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_BRICK_IND_DEFAULT_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_BRICK_IND_SHADOW_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_BRICK_IND_SURFACEEFFECT_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_BRICK_IND_SURFACEEFFECT_SHADOW_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_BRICK_IND_CCF_SURFACEEFFECT_cs_4_0")

			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_GEN_SHADOW_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_GEN_MASK_SHADOW_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_GEN_SURFACE_SHADOW_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_VOLUME_GEN_MASK_SURFACE_SHADOW_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_BRICK_GEN_SHADOW_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\DVR_BRICK_GEN_SURFACE_SHADOW_cs_4_0")
		};
		wstring strNames_MG[NUMSHADERS_MERGE] = {
			_T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\MERGE_SROUT_AND_LAYEROUT_TO_LAYEROUT_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\MERGE_LAYEROUT_TO_RENDEROUT_cs_4_0")
		};

		for (int i = 0; i < NUMSHADERS_VR; i++)
		{
			wstring strName = strNames_VR[i];

			FILE* pFile;
			if (_wfopen_s(&pFile, strName.c_str(), _T("rb")) == 0)
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
					VXERRORMESSAGE("SHADER COMPILE FAILURE!")
				}
				else
				{
					VXSAFE_RELEASE(*ppdx11CS_VRs[i]);
					*ppdx11CS_VRs[i] = pdx11CShader;
				}
				VXSAFE_DELETEARRAY(pyRead);
			}
		}
		for (int i = 0; i < NUMSHADERS_MERGE; i++)
		{
			wstring strName = strNames_MG[i];

			FILE* pFile;
			if (_wfopen_s(&pFile, strName.c_str(), _T("rb")) == 0)
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
					VXERRORMESSAGE("SHADER COMPILE FAILURE!")
				}
				else
				{
					VXSAFE_RELEASE(*ppdx11CS_Merge[i]);
					*ppdx11CS_Merge[i] = pdx11CShader;
				}
				VXSAFE_DELETEARRAY(pyRead);
			}
		}
	}
#pragma endregion // SHADER SETTING

	ID3D11Device* pdx11Device = pdx11CommonParams->pdx11Device;
	ID3D11DeviceContext* pdx11DeviceImmContext = pdx11CommonParams->pdx11DeviceImmContext;
	// GPU Resource Check!! : Frame Buffer //
	SVXGPUResourceDESC gpuResourceFB_DESCs[__FB_COUNT_CS];
	SVXGPUResourceArchive gpuResourceFB_ARCHs[__FB_COUNT_CS];
	SVXGPUResourceDESC gpuResourceMERGE_DESCs[__UFB_MERGE_FB_COUNT];
	SVXGPUResourceArchive gpuResourceMERGE_ARCHs[__UFB_MERGE_FB_COUNT];
	SVXGPUResourceDESC gpuResourceLAYER_DESCs[__UFB_MERGE_FB_COUNT];
	SVXGPUResourceArchive gpuResourceLAYER_ARCHs[__UFB_MERGE_FB_COUNT];

#pragma region // IOBJECT OUT
	while (pCOutputIObject->GetFrameBuffer(vxrFrameBufferUsageRENDEROUT, 1) != NULL)
		pCOutputIObject->DeleteFrameBuffer(vxrFrameBufferUsageRENDEROUT, 1);
	if (!pCOutputIObject->ReplaceFrameBuffer(vxrDataTypeBYTE4CHANNEL, vxrFrameBufferUsageRENDEROUT, 0, _T("common render out frame buffer : defined in VS-GPUDX11VxRenderer module")))
		pCOutputIObject->InsertFrameBuffer(vxrDataTypeBYTE4CHANNEL, vxrFrameBufferUsageRENDEROUT, _T("common render out frame buffer : defined in VS-GPUDX11VxRenderer module"));

	while (pCOutputIObject->GetFrameBuffer(vxrFrameBufferUsageDEPTH, 1) != NULL)
		pCOutputIObject->DeleteFrameBuffer(vxrFrameBufferUsageDEPTH, 1);
	if (!pCOutputIObject->ReplaceFrameBuffer(vxrDataTypeFLOAT, vxrFrameBufferUsageDEPTH, 0, _T("1st hit screen depth frame buffer : defined in VS-GPUDX11VxRenderer module")))
		pCOutputIObject->InsertFrameBuffer(vxrDataTypeFLOAT, vxrFrameBufferUsageDEPTH, _T("1st hit screen depth frame buffer : defined in VS-GPUDX11VxRenderer module"));
#pragma endregion // IOBJECT OUT

#pragma region // IOBJECT MERGE
	bool bInitGen = false;		// GPU Resource 와의 이중 동기화를 피하기 위함.
	int iImageResCheckCount = __FB_COUNT_CS;
	// __FB_UBUF_SHADOWMAP, __FB_UAV_SHADOWMAP, __FB_SRV_SHADOWMAP
	if (!bIsShadowRenderer)
		iImageResCheckCount = __FB_COUNT_CS - 3;

	// pCMergeIObject Setting //
	while (pCMergeIObject->GetFrameBuffer(vxrFrameBufferUsageCUSTOM, 1) != NULL)
		pCMergeIObject->DeleteFrameBuffer(vxrFrameBufferUsageCUSTOM, 1);
	if (!pCMergeIObject->ReplaceFrameBuffer(vxrDataTypeSTRUCTURED, vxrFrameBufferUsageCUSTOM, 0, _T("Merge Buffer for Merge Layer : defined in VS-GPUDX11VxRenderer module"), 1))
		pCMergeIObject->InsertFrameBuffer(vxrDataTypeSTRUCTURED, vxrFrameBufferUsageCUSTOM, _T("Merge Buffer for Merge Layer : defined in VS-GPUDX11VxRenderer module"), 1);

	for (int i = 0; i < __FB_COUNT_CS; i++)
	{
		gpuResourceFB_DESCs[i].eDataType = vxrDataTypeSTRUCTURED;
		gpuResourceFB_DESCs[i].eGpuSdkType = vxgGpuSdkTypeDX11;
		gpuResourceFB_DESCs[i].strUsageSpecifier = _T("BUFFER_IOBJECT_RESULTOUT");
		gpuResourceFB_DESCs[i].iCustomMisc = __BLOCKSIZE;
		gpuResourceFB_DESCs[i].iSizeStride = sizeof(RWB_Output_RenderOut);
		gpuResourceFB_DESCs[i].iSourceObjectID = pCMergeIObject->GetObjectID();
	}
	gpuResourceFB_DESCs[__FB_UBUF_RENDEROUT].eResourceType = vxgResourceTypeDX11RESOURCE;
	gpuResourceFB_DESCs[__FB_UAV_RENDEROUT].eResourceType = vxgResourceTypeDX11UAV;
	gpuResourceFB_DESCs[__FB_SRV_RENDEROUT].eResourceType = vxgResourceTypeDX11SRV;
	gpuResourceFB_DESCs[__FB_UBUF_SYSTEM_RENDEROUT].eResourceType = vxgResourceTypeDX11RESOURCE;
	gpuResourceFB_DESCs[__FB_UBUF_SYSTEM_RENDEROUT].strUsageSpecifier = _T("BUFFER_IOBJECT_SYSTEM");
	// To Do  나중에  Merging Buffer 로 뺀다. //
	gpuResourceFB_DESCs[__FB_UBUF_SHADOWMAP].strUsageSpecifier = _T("BUFFER_IOBJECT_SHADOWMAP");
	gpuResourceFB_DESCs[__FB_UBUF_SHADOWMAP].eResourceType = vxgResourceTypeDX11RESOURCE;
	gpuResourceFB_DESCs[__FB_UBUF_SHADOWMAP].iSizeStride = sizeof(RWB_Output_ShadowMap);
	gpuResourceFB_DESCs[__FB_UAV_SHADOWMAP] = gpuResourceFB_DESCs[__FB_SRV_SHADOWMAP] = gpuResourceFB_DESCs[__FB_UBUF_SHADOWMAP];
	gpuResourceFB_DESCs[__FB_UAV_SHADOWMAP].eResourceType = vxgResourceTypeDX11UAV;
	gpuResourceFB_DESCs[__FB_SRV_SHADOWMAP].eResourceType = vxgResourceTypeDX11SRV;

	for (int i = 0; i < iImageResCheckCount; i++)
	{
		if (i == __FB_UBUF_SYSTEM_RENDEROUT && !bIsSystemOut)
			continue;
		if (!pCGpuManager->GetGpuResourceArchive(&gpuResourceFB_DESCs[i], &gpuResourceFB_ARCHs[i]))
		{
			pCGpuManager->GenerateGpuResource(&gpuResourceFB_DESCs[i], pCMergeIObject, &gpuResourceFB_ARCHs[i], NULL);
			bInitGen = true;
		}
	}

	bInitGen = false;	// This is important to handle the previous rendered framebuffer
	int iCountMerging = 0;
	pCMergeIObject->GetCustomParameter(_T("_int_CountMerging"), &iCountMerging);
	for (int i = 0; i < __UFB_MERGE_FB_COUNT; i++)
	{
		gpuResourceMERGE_DESCs[i].eDataType = vxrDataTypeSTRUCTURED;
		gpuResourceMERGE_DESCs[i].eGpuSdkType = vxgGpuSdkTypeDX11;
		gpuResourceMERGE_DESCs[i].strUsageSpecifier = _T("BUFFER_IOBJECT_RESULTOUT");
		gpuResourceMERGE_DESCs[i].iCustomMisc = __BLOCKSIZE | (2 << 16);	// '2' means two resource buffers
		gpuResourceMERGE_DESCs[i].iSizeStride = sizeof(RWB_Output_MultiLayers);
		gpuResourceMERGE_DESCs[i].iSourceObjectID = pCMergeIObject->GetObjectID();
	}
	gpuResourceMERGE_DESCs[__FB_UBUF_MERGEOUT].eResourceType = vxgResourceTypeDX11RESOURCE;
	gpuResourceMERGE_DESCs[__FB_UAV_MERGEOUT].eResourceType = vxgResourceTypeDX11UAV;
	gpuResourceMERGE_DESCs[__FB_SRV_MERGEOUT].eResourceType = vxgResourceTypeDX11SRV;

	for (int i = 0; i < __UFB_MERGE_FB_COUNT; i++)
	{
		if (!pCGpuManager->GetGpuResourceArchive(&gpuResourceMERGE_DESCs[i], &gpuResourceMERGE_ARCHs[i]))
		{
			pCGpuManager->GenerateGpuResource(&gpuResourceMERGE_DESCs[i], pCMergeIObject, &gpuResourceMERGE_ARCHs[i], NULL);
			bInitGen = true;
		}
	}

	vxint2 i2SizeScreenCurrent, i2SizeScreenOld = vxint2(0, 0);
	pCMergeIObject->GetFrameBufferInfo(&i2SizeScreenCurrent);
	if (bInitGen)
		pCMergeIObject->RegisterCustomParameter(_T("_int2_PreviousScreenSize"), i2SizeScreenCurrent);
	pCMergeIObject->GetCustomParameter(_T("_int2_PreviousScreenSize"), &i2SizeScreenOld);
	if (i2SizeScreenCurrent.x != i2SizeScreenOld.x || i2SizeScreenCurrent.y != i2SizeScreenOld.y)
	{
		pCGpuManager->ReleaseGpuResourcesRelatedObject(pCMergeIObject->GetObjectID());
		for (int i = 0; i < __UFB_MERGE_FB_COUNT; i++)
		{
			pCGpuManager->GenerateGpuResource(&gpuResourceMERGE_DESCs[i], pCMergeIObject, &gpuResourceMERGE_ARCHs[i], NULL);
		}

		for (int i = 0; i < iImageResCheckCount; i++)
		{
			if (i == __FB_UBUF_SYSTEM_RENDEROUT && !bIsSystemOut)
				continue;
			pCGpuManager->GenerateGpuResource(&gpuResourceFB_DESCs[i], pCMergeIObject, &gpuResourceFB_ARCHs[i], NULL);
		}
		pCMergeIObject->RegisterCustomParameter(_T("_int2_PreviousScreenSize"), i2SizeScreenCurrent);
	}
	pCOutputIObject->RegisterCustomParameter(_T("_int2_PreviousScreenSize"), i2SizeScreenCurrent);
#pragma endregion // IOBJECT MERGE

#pragma region // Presetting of VxObject
	int iLastRenderVolumeID = (vtrInputVolumes.at(vtrInputVolumes.size() - 1))->GetObjectID();
	pstModuleParameter->GetCustomParameter(&iLastRenderVolumeID, _T("_int_LastRenderVolumeID"));

	map<int, CVXVObjectVolume*> mapVolumes;
	for (int i = 0; i < (int)vtrInputVolumes.size(); i++)
	{
		CVXVObjectVolume* pCVolume = (CVXVObjectVolume*)vtrInputVolumes.at(i);
		mapVolumes.insert(pair<int, CVXVObjectVolume*>(pCVolume->GetObjectID(), pCVolume));
	}

	map<int, CVXTObject*> mapTObjects;
	for (int i = 0; i < (int)vtrInputOTFs.size(); i++)
	{
		CVXTObject* pCTObject = (CVXTObject*)vtrInputOTFs.at(i);
		mapTObjects.insert(pair<int, CVXTObject*>(pCTObject->GetObjectID(), pCTObject));
	}

	vector<int>* pvtrMainVolumeIDs = NULL;	// Rendering Volume List
	if (!pCLObjectForParamters->GetList(_T("_vlist_INT_MainVolumeIDs"), (void**)&pvtrMainVolumeIDs))
	{
		VXERRORMESSAGE("GPU DVR! - ERROR 00");
		return false;
	}

	vector<int> vtrOrderedMainVolumeIDs;
	bool bIsValidList = false;
	for (int i = 0; i < (int)pvtrMainVolumeIDs->size(); i++)
	{
		int iVolumeID = pvtrMainVolumeIDs->at(i);
		auto itrVolume = mapVolumes.find(iVolumeID);
		if (itrVolume == mapVolumes.end())
		{
			VXERRORMESSAGE("GPU DVR! - INVALID VOLUME ID");
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
		VXERRORMESSAGE("GPU DVR! - ERROR 01");
		return false;
	}
	vtrOrderedMainVolumeIDs.push_back(iLastRenderVolumeID);
#pragma endregion // Presetting of VxObject

	uint uiClearVlaues[4] = { 0 };
	bool bIsFirstRender = false;
	pstModuleParameter->GetCustomParameter(&bIsFirstRender, _T("_bool_IsFirstRender"));
	if (bIsFirstRender)
	{
		pdx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gpuResourceMERGE_ARCHs[__FB_UAV_MERGEOUT].vtrPtrs.at(0), uiClearVlaues);
		pdx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gpuResourceMERGE_ARCHs[__FB_UAV_MERGEOUT].vtrPtrs.at(1), uiClearVlaues);
	}
	if (pCOutputIObject)
		pdx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gpuResourceFB_ARCHs[__FB_UAV_RENDEROUT].vtrPtrs.at(0), uiClearVlaues);
	
#pragma region // Camera & Light & Shadow Setting
	CVXCObject* pCCObject = pCMergeIObject->GetCameraObject();
	vxfloat3 f3PosEyeWS, f3VecViewWS;
	pCCObject->GetCameraState(&f3PosEyeWS, &f3VecViewWS, NULL);
	float fFarPlaneLength;
	pCCObject->GetDefaultViewingState(NULL, NULL, &fFarPlaneLength, NULL);

	CB_VrCameraState cbVrCamState;
	HDx11ComputeConstantBufferVrCamera(&cbVrCamState, __BLOCKSIZE, pCCObject, i2SizeScreenCurrent, &pstModuleParameter->mapCustomParamters);

	// Box Range in the Parallel Light Beam Condition
	vxfloat3 f3PosMinBoxWS = vxfloat3(FLT_MAX, FLT_MAX, FLT_MAX);
	vxfloat3 f3PosMaxBoxWS = vxfloat3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	//if (!(cbVrCamState.iFlags & 0x2) && bIsShadowRenderer)
	if (bIsShadowRenderer)
	{
		for (int i = 0; i < (int)vtrOrderedMainVolumeIDs.size(); i++)
		{
			int iVolumeID = vtrOrderedMainVolumeIDs.at(i);

			auto itrVolume = mapVolumes.find(iVolumeID);
			if (itrVolume == mapVolumes.end())
			{
				VXERRORMESSAGE("NO Volume ID - Shadow");
				continue;
			}

			CVXVObjectVolume* pCMainVolume = itrVolume->second;
			SVXAaBbMinMax stAaBbOS;
			pCMainVolume->GetOrthoBoundingBox(&stAaBbOS);
			vxfloat3 f3PosOrthoBoxesVS[8];
			f3PosOrthoBoxesVS[0] = vxfloat3(stAaBbOS.f3PosMinBox.x, stAaBbOS.f3PosMinBox.y, stAaBbOS.f3PosMaxBox.z);
			f3PosOrthoBoxesVS[1] = vxfloat3(stAaBbOS.f3PosMaxBox.x, stAaBbOS.f3PosMinBox.y, stAaBbOS.f3PosMaxBox.z);
			f3PosOrthoBoxesVS[2] = vxfloat3(stAaBbOS.f3PosMinBox.x, stAaBbOS.f3PosMinBox.y, stAaBbOS.f3PosMinBox.z);
			f3PosOrthoBoxesVS[3] = vxfloat3(stAaBbOS.f3PosMaxBox.x, stAaBbOS.f3PosMinBox.y, stAaBbOS.f3PosMinBox.z);
			f3PosOrthoBoxesVS[4] = vxfloat3(stAaBbOS.f3PosMinBox.x, stAaBbOS.f3PosMaxBox.y, stAaBbOS.f3PosMaxBox.z);
			f3PosOrthoBoxesVS[5] = vxfloat3(stAaBbOS.f3PosMaxBox.x, stAaBbOS.f3PosMaxBox.y, stAaBbOS.f3PosMaxBox.z);
			f3PosOrthoBoxesVS[6] = vxfloat3(stAaBbOS.f3PosMinBox.x, stAaBbOS.f3PosMaxBox.y, stAaBbOS.f3PosMinBox.z);
			f3PosOrthoBoxesVS[7] = vxfloat3(stAaBbOS.f3PosMaxBox.x, stAaBbOS.f3PosMaxBox.y, stAaBbOS.f3PosMinBox.z);
			for (int i = 0; i < 8; i++) 
			{
				vxfloat3 f3PosOrthoBoxWS;
				VXHMTransformPoint(&f3PosOrthoBoxWS, &f3PosOrthoBoxesVS[i], pCMainVolume->GetMatrixOS2WS());
				f3PosMaxBoxWS.x = max(f3PosMaxBoxWS.x, f3PosOrthoBoxWS.x);
				f3PosMaxBoxWS.y = max(f3PosMaxBoxWS.y, f3PosOrthoBoxWS.y);
				f3PosMaxBoxWS.z = max(f3PosMaxBoxWS.z, f3PosOrthoBoxWS.z);
				f3PosMinBoxWS.x = min(f3PosMinBoxWS.x, f3PosOrthoBoxWS.x);
				f3PosMinBoxWS.y = min(f3PosMinBoxWS.y, f3PosOrthoBoxWS.y);
				f3PosMinBoxWS.z = min(f3PosMinBoxWS.z, f3PosOrthoBoxWS.z);
			}
		}
	}

	CB_VrShadowMap cbShadowMap;
	if (bIsShadowStage)
	{
		HDx11ComputeConstantBufferVrShadowMap(&cbShadowMap, &cbVrCamState, f3PosMinBoxWS, f3PosMaxBoxWS, &pstModuleParameter->mapCustomParamters);
	}
	else if (bIsShadowRenderer)
	{
		CB_VrCameraState cbVrCamStateShadowTemp = cbVrCamState;
		HDx11ComputeConstantBufferVrShadowMap(&cbShadowMap, &cbVrCamStateShadowTemp, f3PosMinBoxWS, f3PosMaxBoxWS, &pstModuleParameter->mapCustomParamters);
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
	int iTestIndex = 0, iTestChannel = 0;
	pstModuleParameter->GetCustomParameter(&iTestIndex, _T("_int_Index"));
	pstModuleParameter->GetCustomParameter(&iTestChannel, _T("_int_Channel"));
	pCBParamCamState->iFlags |= iTestIndex << 10;
	pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_CAMSTATE], 0);
	pdx11DeviceImmContext->CSSetConstantBuffers(1, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_CAMSTATE]);
	pdx11DeviceImmContext->CSSetConstantBuffers(11, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_CAMSTATE]);	// For Merge-Stage
#pragma endregion // Light & Shadow Setting

	// Grid Size Computation //
	uint uiNumGridX = (uint)ceil(i2SizeScreenCurrent.x / (float)__BLOCKSIZE);
	uint uiNumGridY = (uint)ceil(i2SizeScreenCurrent.y / (float)__BLOCKSIZE);

	// Initial Setting of Frame Buffers //
	map<wstring, vector<double>> mapDValueClear;

	QueryPerformanceCounter(&lIntCntStart);
	if (bIsSimultaenousMode)
		goto SIMULTANEOUSMODE;

	int iNumMainVolumes = (int)vtrOrderedMainVolumeIDs.size();
	for (int i = 0; i < iNumMainVolumes; i++)
	{
		int iVolumeID = vtrOrderedMainVolumeIDs.at(i);

#pragma region // Volume Custom Parameters
		CVXVObjectVolume* pCVObjectVolume = NULL;
		map<wstring, vector<double>>* pmapDValueVolume = NULL;
		pCLObjectForParamters->GetCustomDValue(iVolumeID, (void**)&pmapDValueVolume);
		if (pmapDValueVolume == NULL)
			pmapDValueVolume = &mapDValueClear;

		// Volumes //
		auto itrVolume = mapVolumes.find(iVolumeID);
		if (itrVolume == mapVolumes.end())
		{
			VXERRORMESSAGE("NO Volume ID");
			continue;
		}

		bool bIsVisible = true;
		VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_bool_IsVisible"), &bIsVisible);

		int iRendererType = 0;
		VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_int_RendererType"), &iRendererType);
		bool bIsXRayMode = false;
		if (iRendererType >= __RM_RAYMAX)
			bIsXRayMode = true;
		if (bIsShowBlock)
			iRendererType = __RM_SURFACEEFFECT;

		bool bForceToSkipBlockUpdateFromVolume = false;
		VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_bool_ForceToSkipBlockUpdate"), &bForceToSkipBlockUpdateFromVolume);

		int iLevelBlock = 1;
		VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_int_BlockLevel"), &iLevelBlock);
		iLevelBlock = max(min(1, iLevelBlock), 0);

		bool bIsBrickChunk = false;
		VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_bool_IsBrickChunk"), &bIsBrickChunk);

		int iTObjectID = 0;
		VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_int_MainTObjectID"), &iTObjectID);

		int iMaskVolumeID = 0;
		VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_int_MaskVolumeObjectID"), &iMaskVolumeID);
		
		pCVObjectVolume = itrVolume->second;
#pragma endregion // Volume Custom Parameters

#pragma region // Mask Volume Custom Parameters
		CVXVObjectVolume* pCVolumeMask = NULL;
		map<wstring, vector<double>>* pmapDValueMaskVolume = NULL;
		pCLObjectForParamters->GetCustomDValue(iMaskVolumeID, (void**)&pmapDValueMaskVolume);
		if (pmapDValueMaskVolume == NULL)
			pmapDValueMaskVolume = &mapDValueClear;

		int iSculptValue = 0;
		vector<double> vtrMaskOTFIDSeries;
		vector<double> vtrMaskOTFIDSeries_Visibilities;
		vector<double> vtrMaskOTFIDMap;
		if (iMaskVolumeID != 0 && (bValidateMaskVolume || iRendererType == __RM_SCULPTMASK))
		{
			auto itrMaskVolume = mapVolumes.find(iMaskVolumeID);
			if (itrMaskVolume == mapVolumes.end())
			{
				VXERRORMESSAGE("NO Mask Volume ID");
				continue;
			}

			pCVolumeMask = itrMaskVolume->second;
			if (pCVolumeMask == NULL)
				return false;

			pCLObjectForParamters->GetCustomDValue(iMaskVolumeID, (void**)&pmapDValueMaskVolume);
			if (pmapDValueMaskVolume == NULL)
				pmapDValueMaskVolume = &mapDValueClear;

			VXHGetCustomValueFromValueContinerMap(pmapDValueMaskVolume, _T("_int_SculptingIndex"), &iSculptValue);
			VXHGetCustomValueFromValueContinerMap(pmapDValueMaskVolume, _T("_vlist_DOUBLE_MaskOTFIDs"), &vtrMaskOTFIDSeries);
			VXHGetCustomValueFromValueContinerMap(pmapDValueMaskVolume, _T("_vlist_DOUBLE_MaskOTFIDs_VisibilitySeries"), &vtrMaskOTFIDSeries_Visibilities);
			VXHGetCustomValueFromValueContinerMap(pmapDValueMaskVolume, _T("_vlist_DOUBLE_MaskOTFIndexMap"), &vtrMaskOTFIDMap);

			if (vtrMaskOTFIDSeries.size() != vtrMaskOTFIDMap.size())
				VXERRORMESSAGE("ERROR!!!!! _vlist_DOUBLE_MaskOTFIDs and _vlist_DOUBLE_MaskOTFIndexMap!!");
		}
		int iNumMaskOTFs = (int)vtrMaskOTFIDSeries.size();
		bool bIsUsedMaskOtfInRenderer = false;
		if (iNumMaskOTFs > 0)
		{
			bIsUsedMaskOtfInRenderer = true;
			// Check if Mask Volume Renderer is used or not.
			if (iNumMaskOTFs == 1 && vtrMaskOTFIDSeries.at(0) == iTObjectID)
				bIsUsedMaskOtfInRenderer = false;
		}

#pragma endregion // Mask Volume Custom Parameters

#pragma region // Main TObject Custom Parameters
		CVXTObject* pCTObjectMain = NULL;
		map<wstring, vector<double>>* pmapDValueTObject = NULL;
		pCLObjectForParamters->GetCustomDValue(iTObjectID, (void**)&pmapDValueTObject);
		if (pmapDValueTObject == NULL)
			pmapDValueTObject = &mapDValueClear;

		auto itrTObject = mapTObjects.find(iTObjectID);
		if (itrTObject == mapTObjects.end())
		{
			VXERRORMESSAGE("NO TOBJECT ID");
			continue;
		}

		bool bIsTfChanged = false;
		VXHGetCustomValueFromValueContinerMap(pmapDValueTObject, _T("_bool_IsTfChanged"), &bIsTfChanged);
		if (bInitGen || bForcedUpdateOtf)
			bIsTfChanged = true;

		bool bForceToSkipBlockUpdateFromOTF = false;
		VXHGetCustomValueFromValueContinerMap(pmapDValueTObject, _T("_bool_ForceToSkipBlockUpdate"), &bForceToSkipBlockUpdateFromOTF);

		bool bUpdateBricks = false;
		VXHGetCustomValueFromValueContinerMap(pmapDValueTObject, _T("_bool_UpdateBricks"), &bUpdateBricks);

		bool bForceToSkipBlockUpdate = bForceToSkipBlockUpdateFromOTF || bForceToSkipBlockUpdateFromVolume;

		if (bIsUsedMaskOtfInRenderer)
		{
			for (int j = 0; j < iNumMaskOTFs; j++)
			{
				int iMaskOTFID = (int)vtrMaskOTFIDSeries.at(j);
				map<wstring, vector<double>>* pmapDValueMaskTObject = NULL;
				if (pCLObjectForParamters->GetCustomDValue(iMaskOTFID, (void**)&pmapDValueMaskTObject))
				{
					bool bIsMaskTfChanged = false;
					VXHGetCustomValueFromValueContinerMap(pmapDValueMaskTObject, _T("_bool_IsTfChanged"), &bIsMaskTfChanged);
					bIsTfChanged |= bIsMaskTfChanged;
				}
			}
		}

		pCTObjectMain = itrTObject->second;
#pragma endregion // Main TObject Custom Parameters

#pragma region // Volume Resource Setting 
		SVXVolumeDataArchive* pstVolArchive = pCVObjectVolume->GetVolumeArchive();
		SVXGPUResourceArchive stGpuResourceVolumeTexture, stGpuResourceVolumeSRV;
		SVXGPUResourceDESC stGpuVolumeResourceDesc;
		stGpuVolumeResourceDesc.eGpuSdkType = vxgGpuSdkTypeDX11;
		stGpuVolumeResourceDesc.eDataType = pstVolArchive->eVolumeDataType;
		stGpuVolumeResourceDesc.iSizeStride = VXHGetDataTypeSizeByte(stGpuVolumeResourceDesc.eDataType);
		stGpuVolumeResourceDesc.iSourceObjectID = pCVObjectVolume->GetObjectID();
		if (bIsBrickChunk)
		{
			bool bPoreModify = false;
			VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_bool_ModifyPoreSizeVolume"), &bPoreModify);
			if (bPoreModify)
				bUpdateBricks = true;

			stGpuVolumeResourceDesc.strUsageSpecifier = _T("TEXTURE3D_VOLUME_BRICKCHUNK");
			stGpuVolumeResourceDesc.iCustomMisc = iLevelBlock;
			if (bUpdateBricks && !bForceToSkipBlockUpdate)
			{
				stGpuVolumeResourceDesc.eResourceType = vxgResourceTypeDX11RESOURCE;
				pCGpuManager->ReleaseGpuResource(&stGpuVolumeResourceDesc);
				stGpuVolumeResourceDesc.eResourceType = vxgResourceTypeDX11SRV;
				pCGpuManager->ReleaseGpuResource(&stGpuVolumeResourceDesc);
			}
		}
		else
		{
			stGpuVolumeResourceDesc.strUsageSpecifier = _T("TEXTURE3D_VOLUME_DEFAULT");
			if (iRendererType == __RM_SURFACECCF)
			{
				stGpuVolumeResourceDesc.strUsageSpecifier = _T("TEXTURE3D_VOLUME_DEFAULT_MASK");
			}
		}

		stGpuVolumeResourceDesc.eResourceType = vxgResourceTypeDX11RESOURCE;
		if (!pCGpuManager->GetGpuResourceArchive(&stGpuVolumeResourceDesc, &stGpuResourceVolumeTexture))
			pCGpuManager->GenerateGpuResource(&stGpuVolumeResourceDesc, pCVObjectVolume, &stGpuResourceVolumeTexture, psvxLocalProgress);
		stGpuVolumeResourceDesc.eResourceType = vxgResourceTypeDX11SRV;
		if (!pCGpuManager->GetGpuResourceArchive(&stGpuVolumeResourceDesc, &stGpuResourceVolumeSRV))
			pCGpuManager->GenerateGpuResource(&stGpuVolumeResourceDesc, pCVObjectVolume, &stGpuResourceVolumeSRV, NULL);
#pragma endregion // Volume Resource Setting 

#pragma region // Mask Volume Resource Setting
		if (pCVolumeMask != NULL)
		{
			SVXVolumeDataArchive* pstVolMaskArchive = pCVolumeMask->GetVolumeArchive();

			SVXGPUResourceArchive stGpuResourceVolumeTextureForMask, stGpuResourceVolumeSRVForMask;
			SVXGPUResourceDESC stGpuVolumeResourceDescForMask;
			stGpuVolumeResourceDescForMask.eGpuSdkType = vxgGpuSdkTypeDX11;

			// Intel HD 에서는 BYTE 를 제대로 못 읽는다!! -_-;.... // 20140303 왜 못 읽음 -_-; 잘 되는 것으로 다시 확인
			// normalize to original... precision bug! 가 HD 4000 에서 있는 것을 확인 // 20160523
			stGpuVolumeResourceDescForMask.eDataType = pstVolMaskArchive->eVolumeDataType;	// vxrDataTypeUNSIGNEDSHORT;//
			stGpuVolumeResourceDescForMask.iSizeStride = VXHGetDataTypeSizeByte(stGpuVolumeResourceDescForMask.eDataType);
			stGpuVolumeResourceDescForMask.iSourceObjectID = pCVolumeMask->GetObjectID();
			stGpuVolumeResourceDescForMask.strUsageSpecifier = _T("TEXTURE3D_VOLUME_DEFAULT_MASK");
			stGpuVolumeResourceDescForMask.eResourceType = vxgResourceTypeDX11RESOURCE;
			if (!pCGpuManager->GetGpuResourceArchive(&stGpuVolumeResourceDescForMask, &stGpuResourceVolumeTextureForMask))
			{
				pCGpuManager->GenerateGpuResource(&stGpuVolumeResourceDescForMask, pCVolumeMask, &stGpuResourceVolumeTextureForMask, psvxLocalProgress);
			}
			stGpuVolumeResourceDescForMask.eResourceType = vxgResourceTypeDX11SRV;
			if (!pCGpuManager->GetGpuResourceArchive(&stGpuVolumeResourceDescForMask, &stGpuResourceVolumeSRVForMask))
			{
				pCGpuManager->GenerateGpuResource(&stGpuVolumeResourceDescForMask, pCVolumeMask, &stGpuResourceVolumeSRVForMask, NULL);
			}

			pdx11DeviceImmContext->CSSetShaderResources(8, 1, (ID3D11ShaderResourceView**)&stGpuResourceVolumeSRVForMask.vtrPtrs.at(0));
		}
#pragma endregion // Mask Volume Resource Setting

#pragma region // TObject Resource Setting 
		SVXTransferFunctionArchive* pstMainTfArchive = pCTObjectMain->GetTransferFunctionArchive();
		SVXGPUResourceDESC stGpuOtfResourceDesc;
		SVXGPUResourceArchive stGpuResourceOtfBuffer, stGpuResourceOtfSRV;
		SVXGPUResourceDESC stGpuOtfIdMapResourceDesc;
		SVXGPUResourceArchive stGpuResourceOtfIdMapBuffer, stGpuResourceOtfIdMapSRV;

		stGpuOtfResourceDesc.eGpuSdkType = vxgGpuSdkTypeDX11;
		stGpuOtfResourceDesc.eResourceType = vxgResourceTypeDX11RESOURCE;
		stGpuOtfResourceDesc.eDataType = pstMainTfArchive->eValueDataType;
		stGpuOtfResourceDesc.iSizeStride = VXHGetDataTypeSizeByte(stGpuOtfResourceDesc.eDataType);
		stGpuOtfResourceDesc.iSourceObjectID = pCTObjectMain->GetObjectID();
		stGpuOtfResourceDesc.iCustomMisc = 0;
		stGpuOtfResourceDesc.strUsageSpecifier = _T("BUFFER_TOBJECT_OTFSERIES");

		int iNumPrevMaskOTFs = 0;
		pCTObjectMain->GetCustomParameter(_T("_int_NumOTFSeries"), &iNumPrevMaskOTFs);

		bool bMaskUpdate = false;
		if (iNumPrevMaskOTFs != iNumMaskOTFs && bIsUsedMaskOtfInRenderer)
		{
			bMaskUpdate = true;
			pCGpuManager->ReleaseGpuResource(&stGpuOtfResourceDesc, false);
			stGpuOtfResourceDesc.eResourceType = vxgResourceTypeDX11SRV;
			pCGpuManager->ReleaseGpuResource(&stGpuOtfResourceDesc, false);
			stGpuOtfResourceDesc.eResourceType = vxgResourceTypeDX11RESOURCE;
			pCTObjectMain->RegisterCustomParameter(_T("_int_NumOTFSeries"), iNumMaskOTFs);
		}

		if (!pCGpuManager->GetGpuResourceArchive(&stGpuOtfResourceDesc, &stGpuResourceOtfBuffer))
		{
			pCGpuManager->GenerateGpuResource(&stGpuOtfResourceDesc, pCTObjectMain, &stGpuResourceOtfBuffer, NULL);
			bIsTfChanged = true;
			bForceToSkipBlockUpdate = false;
		}

		stGpuOtfResourceDesc.eResourceType = vxgResourceTypeDX11SRV;
		if (!pCGpuManager->GetGpuResourceArchive(&stGpuOtfResourceDesc, &stGpuResourceOtfSRV))
		{
			pCGpuManager->GenerateGpuResource(&stGpuOtfResourceDesc, pCTObjectMain, &stGpuResourceOtfSRV, NULL);
			bForceToSkipBlockUpdate = false;
		}

		if (bIsTfChanged)
		{
			D3D11_MAPPED_SUBRESOURCE d11MappedRes;
			pdx11DeviceImmContext->Map((ID3D11Resource*)stGpuResourceOtfBuffer.vtrPtrs.at(0), 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes);
			vxfloat4* pf4ColorTF = (vxfloat4*)d11MappedRes.pData;
			if (!bIsUsedMaskOtfInRenderer)
			{
				memcpy(pf4ColorTF, pstMainTfArchive->ppvArchiveTF[0], pstMainTfArchive->i3DimSize.x * sizeof(vxfloat4));
			}
			else
			{
				for (int j = 0; j < iNumMaskOTFs; j++)
				{
					int iTObjectMaskID = (int)vtrMaskOTFIDSeries.at(j);
					bool bVisibleMaskValue = false;
					if(vtrMaskOTFIDSeries_Visibilities.at(j) != 0)
						bVisibleMaskValue = true;

					if (bVisibleMaskValue)
					{
						auto itrTObjectMask = mapTObjects.find(iTObjectMaskID);
						CVXTObject* pCTObjectMask = itrTObjectMask->second;
						SVXTransferFunctionArchive* pstTfArchiveMask = pCTObjectMask->GetTransferFunctionArchive();
						memcpy(&pf4ColorTF[j * pstMainTfArchive->i3DimSize.x], pstTfArchiveMask->ppvArchiveTF[0], pstMainTfArchive->i3DimSize.x * sizeof(vxfloat4));
					}
					else
					{
						memset(&pf4ColorTF[j * pstMainTfArchive->i3DimSize.x], 0, pstMainTfArchive->i3DimSize.x * sizeof(vxfloat4));
					}
				}
			}
			pdx11DeviceImmContext->Unmap((ID3D11Resource*)stGpuResourceOtfBuffer.vtrPtrs.at(0), 0);
		}

		if (bIsUsedMaskOtfInRenderer)
		{
			stGpuOtfIdMapResourceDesc.eGpuSdkType = vxgGpuSdkTypeDX11;
			stGpuOtfIdMapResourceDesc.eDataType = vxrDataTypeINT;
			stGpuOtfIdMapResourceDesc.eResourceType = vxgResourceTypeDX11RESOURCE;
			stGpuOtfIdMapResourceDesc.iSizeStride = VXHGetDataTypeSizeByte(stGpuOtfIdMapResourceDesc.eDataType);
			stGpuOtfIdMapResourceDesc.iSourceObjectID = pCTObjectMain->GetObjectID();
			stGpuOtfIdMapResourceDesc.iCustomMisc = 1;
			stGpuOtfIdMapResourceDesc.strUsageSpecifier = _T("BUFFER_TOBJECT_OTFSERIES");

			if (bMaskUpdate)
			{
				pCGpuManager->ReleaseGpuResource(&stGpuOtfIdMapResourceDesc, false);
				stGpuOtfIdMapResourceDesc.eResourceType = vxgResourceTypeDX11SRV;
				pCGpuManager->ReleaseGpuResource(&stGpuOtfIdMapResourceDesc, false);
				stGpuOtfIdMapResourceDesc.eResourceType = vxgResourceTypeDX11RESOURCE;
			}

			if (!pCGpuManager->GetGpuResourceArchive(&stGpuOtfIdMapResourceDesc, &stGpuResourceOtfIdMapBuffer))
			{
				pCGpuManager->GenerateGpuResource(&stGpuOtfIdMapResourceDesc, pCTObjectMain, &stGpuResourceOtfIdMapBuffer, NULL);
			}

			stGpuOtfIdMapResourceDesc.eResourceType = vxgResourceTypeDX11SRV;
			if (!pCGpuManager->GetGpuResourceArchive(&stGpuOtfIdMapResourceDesc, &stGpuResourceOtfIdMapSRV))
			{
				pCGpuManager->GenerateGpuResource(&stGpuOtfIdMapResourceDesc, pCTObjectMain, &stGpuResourceOtfIdMapSRV, NULL);
			}

			D3D11_MAPPED_SUBRESOURCE d11MappedRes;
			pdx11DeviceImmContext->Map((ID3D11Resource*)stGpuResourceOtfIdMapBuffer.vtrPtrs.at(0), 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes);
			int* pi4OtfIndex = (int*)d11MappedRes.pData;
			memset(pi4OtfIndex, 0, sizeof(int)* 256);
			for (int j = 0; j < iNumMaskOTFs; j++)
			{
				int iIndexOtf = static_cast<int>(vtrMaskOTFIDMap.at(j));
				pi4OtfIndex[iIndexOtf] = j;
			}
			pdx11DeviceImmContext->Unmap((ID3D11Resource*)stGpuResourceOtfIdMapBuffer.vtrPtrs.at(0), 0);
		}
#pragma endregion // TObject Resource Setting 

#pragma region // Main Volume Block Mask Setting From OTFs
		SVXVolumeBlock* psvxVolBlock = pCVObjectVolume->GetVolumeBlock(iLevelBlock);
		if (psvxVolBlock)
		{
			if (psvxVolBlock->pvMinMaxBlocks == NULL)
			{
				vxint3 i3SizeVolume = pCVObjectVolume->GetVolumeArchive()->i3VolumeSize;
				vxint3 i3BlockLevelSize[2];
				uint uiMaxSize = max(max(i3SizeVolume.x, i3SizeVolume.y), i3SizeVolume.z);
				uint uiSizeBlockMax = max((uint)pow(2.0, floor((log((double)uiMaxSize / 16.0) / log(2.0)))), 8);
				i3BlockLevelSize[0].x = uiSizeBlockMax;
				i3BlockLevelSize[0].y = uiSizeBlockMax;
				i3BlockLevelSize[0].z = uiSizeBlockMax;
				i3BlockLevelSize[1].x = uiSizeBlockMax / 2;
				i3BlockLevelSize[1].y = uiSizeBlockMax / 2;
				i3BlockLevelSize[1].z = uiSizeBlockMax / 2;
				pCVObjectVolume->GenerateVolumeMinMaxBlocks(iLevelBlock, i3BlockLevelSize[iLevelBlock], psvxLocalProgress);
				psvxVolBlock = pCVObjectVolume->GetVolumeBlock(iLevelBlock);
			}
		}

		SVXGPUResourceDESC stGpuResBlockDesc;
		SVXGPUResourceArchive stGpuResourceBlockTexture, stGpuResourceBlockSRV;
		stGpuResBlockDesc.eGpuSdkType = vxgGpuSdkTypeDX11;
		stGpuResBlockDesc.eResourceType = vxgResourceTypeDX11RESOURCE;
		if (bIsXRayMode)
		{
			stGpuResBlockDesc.strUsageSpecifier = _T("TEXTURE3D_VOLUME_MINMAXBLOCK");
			stGpuResBlockDesc.eDataType = vxrDataTypeUNSIGNEDSHORT;
		}
		else
		{
			stGpuResBlockDesc.strUsageSpecifier = _T("TEXTURE3D_VOLUME_BLOCKMAP");
			if (psvxVolBlock->i3BlocksNumber.x * psvxVolBlock->i3BlocksNumber.y * psvxVolBlock->i3BlocksNumber.z < 65536)
				stGpuResBlockDesc.eDataType = vxrDataTypeUNSIGNEDSHORT;
			else
				stGpuResBlockDesc.eDataType = vxrDataTypeFLOAT;
		}
		stGpuResBlockDesc.iSizeStride = VXHGetDataTypeSizeByte(stGpuResBlockDesc.eDataType);
		stGpuResBlockDesc.iSourceObjectID = pCVObjectVolume->GetObjectID();
		stGpuResBlockDesc.iRelatedObjectID = pCTObjectMain->GetObjectID();
		stGpuResBlockDesc.iCustomMisc = iLevelBlock;
		if (!pCGpuManager->GetGpuResourceArchive(&stGpuResBlockDesc, &stGpuResourceBlockTexture))
		{
			bIsTfChanged = true;
			bUpdateBricks = true;
			bForceToSkipBlockUpdate = false;
		}
		stGpuResBlockDesc.eResourceType = vxgResourceTypeDX11SRV;
		if (!pCGpuManager->GetGpuResourceArchive(&stGpuResBlockDesc, &stGpuResourceBlockSRV))
		{
			bIsTfChanged = true;
			bUpdateBricks = true;
			bForceToSkipBlockUpdate = false;
		}

		if ((bIsTfChanged || bUpdateBricks) && !bForceToSkipBlockUpdate)
		{
			vxdouble2 d2ActiveValueRangeMinMax = vxdouble2(DBL_MAX, -DBL_MAX);

			if (!bIsUsedMaskOtfInRenderer)
			{
				SVXTransferFunctionArchive* pstTfArchive = pCTObjectMain->GetTransferFunctionArchive();
				d2ActiveValueRangeMinMax.x = pstTfArchive->i3ValidMinIndex.x;
				d2ActiveValueRangeMinMax.y = pstTfArchive->i3ValidMaxIndex.x;
			}
			else
			{
				for (int j = 0; j < iNumMaskOTFs; j++)
				{
					int iTObjectMaskID = (int)vtrMaskOTFIDSeries.at(j);
					auto itrTObjectMask = mapTObjects.find(iTObjectMaskID);
					if (itrTObjectMask == mapTObjects.end())
					{
						VXERRORMESSAGE("GPU DVR! - INVALID TOBJECT ID - MASK TF CHECK");
						return false;
					}

					CVXTObject* pCTObjectMask = itrTObjectMask->second;

					SVXTransferFunctionArchive* pstTfArchive = pCTObjectMask->GetTransferFunctionArchive();
					d2ActiveValueRangeMinMax.x = min(pstTfArchive->i3ValidMinIndex.x, d2ActiveValueRangeMinMax.x);
					d2ActiveValueRangeMinMax.y = max(pstTfArchive->i3ValidMaxIndex.x, d2ActiveValueRangeMinMax.y);
				}
			}

			pCVObjectVolume->UpdateTaggedActivatedBlocks(iLevelBlock, d2ActiveValueRangeMinMax, NULL);

			stGpuResBlockDesc.eResourceType = vxgResourceTypeDX11RESOURCE;
			if (!pCGpuManager->GetGpuResourceArchive(&stGpuResBlockDesc, &stGpuResourceBlockTexture))
				pCGpuManager->GenerateGpuResource(&stGpuResBlockDesc, pCVObjectVolume, &stGpuResourceBlockTexture, NULL);

			stGpuResBlockDesc.eResourceType = vxgResourceTypeDX11SRV;
			if (!pCGpuManager->GetGpuResourceArchive(&stGpuResBlockDesc, &stGpuResourceBlockSRV))
				pCGpuManager->GenerateGpuResource(&stGpuResBlockDesc, pCVObjectVolume, &stGpuResourceBlockSRV, NULL);

			if (!bIsXRayMode)
			{
				HDx11ModifyBlockMap(pCVObjectVolume, &stGpuResourceBlockTexture, &stGpuResourceBlockSRV, true);
				pCGpuManager->ReplaceOrAddGpuResource(&stGpuResourceBlockTexture);
				pCGpuManager->ReplaceOrAddGpuResource(&stGpuResourceBlockSRV);
			}
		}
#pragma endregion // Main Block Mask Setting From OTFs

		// Trivial Test //
		bool bIsNoValidObject = bIsNoValidObjectInitValue;
		if(!bIsVisible)
			bIsNoValidObject = true;
		//if ((pstMainTfArchive->i3ValidMinIndex.x > pstMainTfArchive->i3DimSize.x && !bIsSimultaenousMode)
		//	|| !bIsVisible)
		//	bIsNoValidObject = true;

#pragma region // Constant Buffers
		bool bIsDownSample = false;
		if (stGpuResourceVolumeTexture.f3SampleIntervalElements.x > 1.f ||
			stGpuResourceVolumeTexture.f3SampleIntervalElements.y > 1.f ||
			stGpuResourceVolumeTexture.f3SampleIntervalElements.z > 1.f)
			bIsDownSample = true;
		if (iRendererType == __RM_MODULATION && bIsDownSample)
			bIsDownSample = false;

		CB_VrVolumeObject cbVrVolumeObj;
		HDx11ComputeConstantBufferVrObject(&cbVrVolumeObj, bIsDownSample, pCVObjectVolume, pCCObject, stGpuResourceVolumeTexture.GetSizeLogicalElement(), pmapDValueVolume);
		if (bIsBrickChunk)
		{
			CB_VrBrickChunk cbVrBrickObj;
			HDx11ComputeConstantBufferVrBrickChunk(&cbVrBrickObj, pCVObjectVolume, iLevelBlock, &stGpuResourceVolumeTexture);
			D3D11_MAPPED_SUBRESOURCE mappedResVolBrickChunk;
			pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_BRICKCHUNK], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResVolBrickChunk);
			CB_VrBrickChunk* pCBParamVolumeBrickChunk = (CB_VrBrickChunk*)mappedResVolBrickChunk.pData;
			memcpy(pCBParamVolumeBrickChunk, &cbVrBrickObj, sizeof(CB_VrBrickChunk));
			pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_BRICKCHUNK], 0);
			pdx11DeviceImmContext->CSSetConstantBuffers(6, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_BRICKCHUNK]);

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
		pdx11DeviceImmContext->CSSetConstantBuffers(0, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_VOLOBJ]);

		CB_VrOtf1D cbVrOtf1D;
		HDx11ComputeConstantBufferVrOtf1D(&cbVrOtf1D, pCTObjectMain, stGpuOtfResourceDesc.iCustomMisc, &pstModuleParameter->mapCustomParamters);
		D3D11_MAPPED_SUBRESOURCE mappedResOtf1D;
		pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_OTF1D], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResOtf1D);
		CB_VrOtf1D* pCBParamOtf1D = (CB_VrOtf1D*)mappedResOtf1D.pData;
		memcpy(pCBParamOtf1D, &cbVrOtf1D, sizeof(CB_VrOtf1D));
		pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_OTF1D], 0);
		pdx11DeviceImmContext->CSSetConstantBuffers(2, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_OTF1D]);

		CB_VrBlockObject cbVrBlock;
		if (stGpuResBlockDesc.eDataType == vxrDataTypeFLOAT)
			HDx11ComputeConstantBufferVrBlockObj(&cbVrBlock, pCVObjectVolume, iLevelBlock, 1.f);
		else
			HDx11ComputeConstantBufferVrBlockObj(&cbVrBlock, pCVObjectVolume, iLevelBlock, 65535.f);
		D3D11_MAPPED_SUBRESOURCE mappedResBlock;
		pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_BLOCKOBJ], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResBlock);
		CB_VrBlockObject* pCBParamBlock = (CB_VrBlockObject*)mappedResBlock.pData;
		memcpy(pCBParamBlock, &cbVrBlock, sizeof(CB_VrBlockObject));
		pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_BLOCKOBJ], 0);
		pdx11DeviceImmContext->CSSetConstantBuffers(3, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_BLOCKOBJ]);

		bool bIsMarkerMode = false;
		switch (iRendererType)
		{
		case __RM_SURFACECURVATURE:
		case __RM_SURFACEEFFECT:
		case __RM_SURFACECCF:
		{
								CB_VrSurfaceEffect cbVrSurfaceEffect;
								HDx11ComputeConstantBufferVrSurfaceEffect(&cbVrSurfaceEffect, pmapDValueVolume);
								D3D11_MAPPED_SUBRESOURCE mappedResSurfaceEffect;
								pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_SURFACEEFFECT], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResSurfaceEffect);
								CB_VrSurfaceEffect* pCBParamSurfaceEffect = (CB_VrSurfaceEffect*)mappedResSurfaceEffect.pData;
								memcpy(pCBParamSurfaceEffect, &cbVrSurfaceEffect, sizeof(CB_VrSurfaceEffect));
								pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_SURFACEEFFECT], 0);
								pdx11DeviceImmContext->CSSetConstantBuffers(4, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_SURFACEEFFECT]);

								CB_VrInterestingRegion cbVrInterestingRegion;
								HDx11ComputeConstantBufferVrInterestingRegion(&cbVrInterestingRegion, pmapDValueVolume);
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
								HDx11ComputeConstantBufferVrModulation(&cbVrModulation, pCVObjectVolume, f3PosEyeWS, pmapDValueVolume);
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

#pragma region // Shader Setting
		pdx11DeviceImmContext->CSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&stGpuResourceOtfSRV.vtrPtrs.at(0));

		if (bIsUsedMaskOtfInRenderer)
		{
			pdx11DeviceImmContext->CSSetShaderResources(3, 1, (ID3D11ShaderResourceView**)&stGpuResourceOtfIdMapSRV.vtrPtrs.at(0));
		}

		if (iRendererType == __RM_RAYMAX)	// Min
			pdx11DeviceImmContext->CSSetShaderResources(2, 1, (ID3D11ShaderResourceView**)&stGpuResourceBlockSRV.vtrPtrs.at(1));
		else
			pdx11DeviceImmContext->CSSetShaderResources(2, 1, (ID3D11ShaderResourceView**)&stGpuResourceBlockSRV.vtrPtrs.at(0));

		if (bIsBrickChunk)
		{
			ID3D11ShaderResourceView* arrayViews[10] = { NULL };
			int iNumBrickChunks = (int)min(stGpuResourceVolumeSRV.vtrPtrs.size(), 10);
			for (int i = 0; i < iNumBrickChunks; i++)
			{
				arrayViews[i] = (ID3D11ShaderResourceView*)stGpuResourceVolumeSRV.vtrPtrs.at(i);
			}
			pdx11DeviceImmContext->CSSetShaderResources(10, iNumBrickChunks, arrayViews);
		}
		else
		{
			pdx11DeviceImmContext->CSSetShaderResources(1, 1, (ID3D11ShaderResourceView**)&stGpuResourceVolumeSRV.vtrPtrs.at(0));
		}

		pdx11DeviceImmContext->CSSetSamplers(0, 1, &pdx11CommonParams->pdx11SStates[__SState_LINEAR_ZEROBORDER]);
		pdx11DeviceImmContext->CSSetSamplers(1, 1, &pdx11CommonParams->pdx11SStates[__SState_POINT_ZEROBORDER]);
		pdx11DeviceImmContext->CSSetSamplers(2, 1, &pdx11CommonParams->pdx11SStates[__SState_LINEAR_CLAMP]);
		pdx11DeviceImmContext->CSSetSamplers(3, 1, &pdx11CommonParams->pdx11SStates[__SState_POINT_CLAMP]);

		if (bIsShadowRenderer && !bIsShadowStage)
			pdx11DeviceImmContext->CSSetShaderResources(6, 1, (ID3D11ShaderResourceView**)&gpuResourceFB_ARCHs[__FB_SRV_SHADOWMAP].vtrPtrs.at(0));

		if (iCountMerging % 2 == 0)
			pdx11DeviceImmContext->CSSetShaderResources(5, 1, (ID3D11ShaderResourceView**)&gpuResourceMERGE_ARCHs[__FB_SRV_MERGEOUT].vtrPtrs.at(1));
		else
			pdx11DeviceImmContext->CSSetShaderResources(5, 1, (ID3D11ShaderResourceView**)&gpuResourceMERGE_ARCHs[__FB_SRV_MERGEOUT].vtrPtrs.at(0));
#pragma endregion // Shader Setting

#pragma region // Renderer Setting
		UINT UAVInitialCounts = 0;
		bool bIsMixStage = false;
		if (bIsShadowStage)
		{
			// To do
			if (!bIsNoValidObject)
			{
				if (bIsBrickChunk)
				{
					if (iRendererType == __RM_SURFACEEFFECT || iRendererType == __RM_SURFACECCF)
						pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[__CS_BRICK_GEN_SURFACE_SHADOW], NULL, 0);
					else
						pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[__CS_BRICK_GEN_SHADOW], NULL, 0);
				}
				else
				{
					if (!bIsUsedMaskOtfInRenderer)
					{
						if (iRendererType == __RM_SURFACEEFFECT || iRendererType == __RM_SURFACECCF || iRendererType == __RM_SURFACECURVATURE)
							pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[__CS_VOLUME_GEN_SURFACE_SHADOW], NULL, 0);
						else
							pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[__CS_VOLUME_GEN_SHADOW], NULL, 0);
					}
					else
					{
						if (iRendererType == __RM_SURFACEEFFECT || iRendererType == __RM_SURFACECCF || iRendererType == __RM_SURFACECURVATURE)
							pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[__CS_VOLUME_GEN_MASK_SURFACE_SHADOW], NULL, 0);
						else
							pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[__CS_VOLUME_GEN_MASK_SHADOW], NULL, 0);
					}
				}
			}
		}
		else if (iRendererType == __RM_SURFACEEFFECT || iRendererType == __RM_SURFACECCF || iRendererType == __RM_SURFACECURVATURE || bIsXRayMode || bIsForcedDeferredMode)	// _IND_ //
		{
			if (bIsShowBlock)	// Makes iRendererType == __RM_SURFACEEFFECT || iRendererType == rmVR_SURFACECCF
			{
				pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_BLOCK_IND_SHOW], NULL, 0);
			}
			else
			{
				if (!bIsBrickChunk)
				{
					if (!bIsUsedMaskOtfInRenderer)
					{
						switch (iRendererType)
						{
						case __RM_DEFAULT:
							if (bIsShadowRenderer)
								pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_IND_SHADOW], NULL, 0);
							else
							{
								if (b_TEST_IsHomogeneous)
									pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_IND_DEFAULT_HOMOGENEOUS], NULL, 0);
								else
									pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_IND_DEFAULT], NULL, 0);
							}
							break;
						case __RM_SURFACEEFFECT:
						case __RM_SURFACECCF:
							if (bIsShadowRenderer && iRendererType != __RM_SURFACECCF)
							{
								if (!bIsMarkerMode)
									pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_IND_SURFACEEFFECT_SHADOW], NULL, 0);
								else
									pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_IND_SURFACEEFFECT_SHADOW_MARKER], NULL, 0);
							}
							else
							{
								if (iRendererType == __RM_SURFACECCF)
									pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_IND_CCF_SURFACEEFFECT], NULL, 0);
								else
								{
									if (!bIsMarkerMode)
										pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_IND_SURFACEEFFECT], NULL, 0);
									else
										pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_IND_SURFACEEFFECT_MARKER], NULL, 0);
								}
							}
							break;
						case __RM_SURFACECURVATURE:
							pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_IND_SURFACECURVATURE], NULL, 0);
							break;
						case __RM_CLIPOPAQUE:
							pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_IND_CLIPSEMIOPAQUE], NULL, 0);
							break;
						case __RM_MODULATION:
							pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_IND_MODULATION], NULL, 0);
							break;
						case __RM_SCULPTMASK:
							pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_IND_SCULPTMASK], NULL, 0);
							break;
						case __RM_RAYMAX:
							pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_IND_RAYMAX], NULL, 0);
							break;
						case __RM_RAYMIN:
							pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_IND_RAYMIN], NULL, 0);
							break;
						case __RM_RAYSUM:
							pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_IND_RAYSUM], NULL, 0);
							break;
						default:
							VXERRORMESSAGE("NOT SUPPORTED RENDERING MODE!");
							break;
						}
					}
					else // if (bIsUsedMaskOtfInRenderer)
					{
						switch (iRendererType)
						{
						case __RM_DEFAULT:
							if (bIsShadowRenderer)
								pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_IND_MASK_SHADOW], NULL, 0);
							else
								pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_IND_MASK_DEFAULT], NULL, 0);
							break;
						case __RM_SURFACEEFFECT:
							if (bIsShadowRenderer)
							{
								if (!bIsMarkerMode)
									pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_IND_MASK_SURFACEEFFECT_SHADOW], NULL, 0);
								else
									pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_IND_MASK_SURFACEEFFECT_SHADOW_MARKER], NULL, 0);
							}
							else
							{
								if (!bIsMarkerMode)
									pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_IND_MASK_SURFACEEFFECT], NULL, 0);
								else
									pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_IND_MASK_SURFACEEFFECT_MARKER], NULL, 0);
							}
							break;
						case __RM_SURFACECURVATURE:
							pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_IND_MASK_SURFACECURVATURE], NULL, 0);
							break;
						case __RM_CLIPOPAQUE:
							pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_IND_MASK_CLIPSEMIOPAQUE], NULL, 0);
							break;
						case __RM_MODULATION:
							pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_IND_MASK_MODULATION], NULL, 0);
							break;
						case __RM_RAYMAX:
							pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_IND_MASK_RAYMAX], NULL, 0);
							break;
						case __RM_RAYMIN:
							pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_IND_MASK_RAYMIN], NULL, 0);
							break;
						case __RM_RAYSUM:
							pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_IND_MASK_RAYSUM], NULL, 0);
							break;
						default:
							VXERRORMESSAGE("NOT SUPPORTED RENDERING MODE!");
							break;
						}
					}
				}
				else
				{
					if (bIsUsedMaskOtfInRenderer)
						VXERRORMESSAGE("NOT SUPPORTED RENDERING MODE!");
					switch (iRendererType)
					{
					case __RM_DEFAULT:
						if (bIsShadowRenderer)
							pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_BRICK_IND_SHADOW], NULL, 0);
						else
							pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_BRICK_IND_DEFAULT], NULL, 0);
						break;
					case __RM_SURFACEEFFECT:
					case __RM_SURFACECCF:
						if (bIsShadowRenderer)
							pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_BRICK_IND_SURFACEEFFECT_SHADOW], NULL, 0);
						else
						{
							if (iRendererType == __RM_SURFACECCF)
								pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_BRICK_IND_CCF_SURFACEEFFECT], NULL, 0);
							else
								pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_BRICK_IND_SURFACEEFFECT], NULL, 0);
						}
						break;
					default:
						VXERRORMESSAGE("NOT SUPPORTED RENDERING MODE!");
						break;
					}
				}
			}
		}
		else if (!bIsSimultaenousMode)	// _MIX_ //
		{
			bIsMixStage = true;

			if (!bIsBrickChunk)
			{
				if (!bIsUsedMaskOtfInRenderer)
				{
					switch (iRendererType)
					{
					case __RM_DEFAULT:
						if (bIsShadowRenderer)
							pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_MIX_SHADOW], NULL, 0);
						else
						{
							if (b_TEST_IsHomogeneous)
								pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_MIX_DEFAULT_HOMOGENEOUS], NULL, 0);
							else
								pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_MIX_DEFAULT], NULL, 0);
						}
						break;
					case __RM_CLIPOPAQUE:
						pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_MIX_CLIPSEMIOPAQUE], NULL, 0);
						break;
					case __RM_MODULATION:
						pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_MIX_MODULATION], NULL, 0);
						break;
					case __RM_SCULPTMASK:
						pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_MIX_SCULPTMASK], NULL, 0);
						break;
					default:
						VXERRORMESSAGE("NOT SUPPORTED RENDERING MODE!");
						break;
					}
				}
				else
				{
					switch (iRendererType)
					{
					case __RM_DEFAULT:
						if (bIsShadowRenderer)
							pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_MIX_MASK_SHADOW], NULL, 0);
						else
							pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_MIX_MASK_DEFAULT], NULL, 0);
						break;
					case __RM_CLIPOPAQUE:
						pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_MIX_MASK_CLIPSEMIOPAQUE], NULL, 0);
						break;
					case __RM_MODULATION:
						pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_VOLUME_MIX_MASK_MODULATION], NULL, 0);
						break;
					default:
						VXERRORMESSAGE("NOT SUPPORTED RENDERING MODE!");
						break;
					}
				}
			}
			else
			{
				if (bIsUsedMaskOtfInRenderer)
					VXERRORMESSAGE("NOT SUPPORTED RENDERING MODE!");

				switch (iRendererType)
				{
				case __RM_DEFAULT:
					if (bIsShadowRenderer)
						pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_BRICK_MIX_SHADOW], NULL, 0);
					else
						pdx11DeviceImmContext->CSSetShader(*ppdx11CS_VRs[(int)__CS_BRICK_MIX_DEFAULT], NULL, 0);
					break;
				default:
					break;
				}
			}
		}
#pragma endregion // Renderer Setting


#pragma region // Dispatch Shader
		ID3D11UnorderedAccessView* pUAVNULL[1] = { NULL };
		if (bIsShadowStage)
		{
			pdx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gpuResourceFB_ARCHs[__FB_UAV_SHADOWMAP].vtrPtrs.at(0), uiClearVlaues);
			pdx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, (ID3D11UnorderedAccessView**)&gpuResourceFB_ARCHs[__FB_UAV_SHADOWMAP].vtrPtrs.at(0), &UAVInitialCounts);
			if (!bSkipProcess)
				pdx11DeviceImmContext->Dispatch(uiNumGridX, uiNumGridY, 1);
		}
		else if (bIsNoValidObject && i == iNumMainVolumes - 1)	// Trivial Case //
		{
			pdx11DeviceImmContext->CSSetShader(*ppdx11CS_Merge[__CS_MERGE_LAYEROUT_TO_RENDEROUT], NULL, 0);
			pdx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, (ID3D11UnorderedAccessView**)&gpuResourceFB_ARCHs[__FB_UAV_RENDEROUT].vtrPtrs.at(0), &UAVInitialCounts);
			if (!bSkipProcess)
				pdx11DeviceImmContext->Dispatch(uiNumGridX, uiNumGridY, 1);
		}
		else
		{
			if (!bIsMixStage)
			{
				if (iCountMerging % 2 == 0)
					pdx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, (ID3D11UnorderedAccessView**)&gpuResourceMERGE_ARCHs[__FB_UAV_MERGEOUT].vtrPtrs.at(0), &UAVInitialCounts);
				else
					pdx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, (ID3D11UnorderedAccessView**)&gpuResourceMERGE_ARCHs[__FB_UAV_MERGEOUT].vtrPtrs.at(1), &UAVInitialCounts);
				if (!bSkipProcess)
					pdx11DeviceImmContext->Dispatch(uiNumGridX, uiNumGridY, 1);
				iCountMerging++;

				if (i == iNumMainVolumes - 1) // This is final stage for render-out
				{
					pdx11DeviceImmContext->CSSetShader(*ppdx11CS_Merge[__CS_MERGE_LAYEROUT_TO_RENDEROUT], NULL, 0);
					pdx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, pUAVNULL, &UAVInitialCounts);
					pdx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, (ID3D11UnorderedAccessView**)&gpuResourceFB_ARCHs[__FB_UAV_RENDEROUT].vtrPtrs.at(0), &UAVInitialCounts);
					if (iCountMerging % 2 == 0)
						pdx11DeviceImmContext->CSSetShaderResources(5, 1, (ID3D11ShaderResourceView**)&gpuResourceMERGE_ARCHs[__FB_SRV_MERGEOUT].vtrPtrs.at(1));
					else
						pdx11DeviceImmContext->CSSetShaderResources(5, 1, (ID3D11ShaderResourceView**)&gpuResourceMERGE_ARCHs[__FB_SRV_MERGEOUT].vtrPtrs.at(0));
					if (!bSkipProcess)
						pdx11DeviceImmContext->Dispatch(uiNumGridX, uiNumGridY, 1);
				}
				else
				{
					ID3D11ShaderResourceView* pdx11SRV_NULL = NULL;
					ID3D11UnorderedAccessView* pdx11UAV_NULL = NULL;
					pdx11DeviceImmContext->CSSetShaderResources(5, 1, &pdx11SRV_NULL);
					pdx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, &pdx11UAV_NULL, &UAVInitialCounts);
				}
			}
			else
			{
				// in this case, pCOutputIObject should be defined.
				pdx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, (ID3D11UnorderedAccessView**)&gpuResourceFB_ARCHs[__FB_UAV_RENDEROUT].vtrPtrs.at(0), &UAVInitialCounts);
				if (!bSkipProcess)
					pdx11DeviceImmContext->Dispatch(uiNumGridX, uiNumGridY, 1);
			}
		}
#pragma endregion // Dispatch Shader
	}

	goto POSTVR;
SIMULTANEOUSMODE:
	/*
	#pragma region // Simultaneous Rendering Mode
	if(bIsSimultaenousMode)
	{
	bIsMixStage = true;

	// 논문 TEST //
	int iMergeIObjectID = pCMergeIObject->GetObjectID();
	auto itrVolumeEXs = g_mapStoryArchive_Volumes.find(iMergeIObjectID);
	auto itrTObjectEXs = g_mapStoryArchive_TObjects.find(iMergeIObjectID);

	if(itrVolumeEXs->second.size() != itrTObjectEXs->second.size())
	::MessageBox(NULL, _T("ERROR 1"), NULL, MB_OK);

	int iNumEXs = (int)itrVolumeEXs->second.size();
	CB_VrVolumeObject arrayCbVrObjects[3];
	CB_VrOtf1D arrayCbOtfs[3];
	CB_VrModulation arrayCbModulations[3];

	for(int iIndexEx = 0; iIndexEx < min(iNumEXs, 3); iIndexEx++)
	{
	SVXGPUResourceDESC svxGpuOtfResourceDescEx;
	SVXGPUResourceArchive svxGpuResourceOtfTextureEx, svxGpuResourceOtfSRVEx;
	SVXGPUResourceArchive svxGpuResourceVolumeTextureEx, svxGpuResourceVolumeSRVEx;

	CVXVObjectVolume* pVolumeEx = (CVXVObjectVolume*)itrVolumeEXs->second.at(iIndexEx);
	CVXTObject* pTObjectEx = (CVXTObject*)itrTObjectEXs->second.at(iIndexEx);

	// OTF //
	SVXTransferFunctionArchive* psvxTfArchiveEx = pTObjectEx->GetTransferFunctionArchive();
	svxGpuOtfResourceDescEx.eGpuSdkType = vxgGpuSdkTypeDX11;
	svxGpuOtfResourceDescEx.eResourceType = vxgResourceTypeDX11RESOURCE;
	svxGpuOtfResourceDescEx.strUsageSpecifier = _T("");
	svxGpuOtfResourceDescEx.eDataType = psvxTfArchiveEx->eValueDataType;
	svxGpuOtfResourceDescEx.iSizeStride = VXHGetDataTypeSizeByte(svxGpuOtfResourceDescEx.eDataType);
	svxGpuOtfResourceDescEx.iSourceObjectID = pTObjectEx->GetObjectID();
	svxGpuOtfResourceDescEx.iCustomMisc = 0;	// No Pre-integration //!!!
	bool bIsForcedPreintegration = false;
	psvxModuleParameter->GetCustomParameter(&bIsForcedPreintegration, _T("_bool_IsForcedPreintegration"));
	if(!pCGpuManager->GetGpuResourceArchive(&svxGpuOtfResourceDescEx, &svxGpuResourceOtfTextureEx))
	{
	pCGpuManager->GenerateGpuResource(&svxGpuOtfResourceDescEx, pTObjectEx, &svxGpuResourceOtfTextureEx, NULL);
	bIsTfChanged = true;
	}
	svxGpuOtfResourceDescEx.eResourceType = vxgResourceTypeDX11SRV;
	if(!pCGpuManager->GetGpuResourceArchive(&svxGpuOtfResourceDescEx, &svxGpuResourceOtfSRVEx))
	{
	pCGpuManager->GenerateGpuResource(&svxGpuOtfResourceDescEx, pTObjectEx, &svxGpuResourceOtfSRVEx, NULL);
	}
	if(bIsTfChanged)
	{
	vxfloat4* pf4TF = (vxfloat4*)psvxTfArchiveEx->ppvArchiveTF[svxGpuOtfResourceDescEx.iCustomMisc];
	D3D11_MAPPED_SUBRESOURCE d11MappedRes;
	pdx11DeviceImmContext->Map((ID3D11Resource*)svxGpuResourceOtfTextureEx.vtrPtrs.at(0), 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes);
	byte* pyColorTF = (byte*)d11MappedRes.pData;
	int i = 0;
	int iLogitalWidthElement = svxGpuResourceOtfTextureEx.i3SizeLogicalResourceInBytes.x / svxGpuResourceOtfTextureEx.svxGpuResourceDesc.iSizeStride;
	for(i = 0; i < svxGpuResourceOtfTextureEx.i3SizeLogicalResourceInBytes.y - 1; i++)
	{
	memcpy(pyColorTF, &pf4TF[i*iLogitalWidthElement], svxGpuResourceOtfTextureEx.i3SizeLogicalResourceInBytes.x);

	pyColorTF += d11MappedRes.RowPitch;
	}
	memcpy(pyColorTF, &pf4TF[i*iLogitalWidthElement], psvxTfArchiveEx->i3DimSize.x * svxGpuResourceOtfTextureEx.svxGpuResourceDesc.iSizeStride - svxGpuResourceOtfTextureEx.i3SizeLogicalResourceInBytes.x * i);
	pdx11DeviceImmContext->Unmap((ID3D11Resource*)svxGpuResourceOtfTextureEx.vtrPtrs.at(0), 0);
	}
	pdx11DeviceImmContext->CSSetShaderResources(21 + iIndexEx, 1, (ID3D11ShaderResourceView**)&svxGpuResourceOtfSRVEx.vtrPtrs.at(0));

	// GPU Resource Check!! : Volume //
	SVXVolumeDataArchive* psvxVolArchiveEx = pVolumeEx->GetVolumeArchive();
	SVXGPUResourceDESC svxGpuVolumeResourceDescEx;
	svxGpuVolumeResourceDescEx.eGpuSdkType = vxgGpuSdkTypeDX11;
	svxGpuVolumeResourceDescEx.eDataType = psvxVolArchiveEx->eVolumeDataType;
	svxGpuVolumeResourceDescEx.iSizeStride = VXHGetDataTypeSizeByte(svxGpuVolumeResourceDescEx.eDataType);
	svxGpuVolumeResourceDescEx.iSourceObjectID = pVolumeEx->GetObjectID();
	svxGpuVolumeResourceDescEx.strUsageSpecifier = _T("TEXTURE3D_VOLUME_DEFAULT");
	svxGpuVolumeResourceDescEx.eResourceType = vxgResourceTypeDX11RESOURCE;
	if(!pCGpuManager->GetGpuResourceArchive(&svxGpuVolumeResourceDescEx, &svxGpuResourceVolumeTextureEx))
	{
	pCGpuManager->GenerateGpuResource(&svxGpuVolumeResourceDescEx, pVolumeEx, &svxGpuResourceVolumeTextureEx, psvxLocalProgress);
	}
	svxGpuVolumeResourceDescEx.eResourceType = vxgResourceTypeDX11SRV;
	if(!pCGpuManager->GetGpuResourceArchive(&svxGpuVolumeResourceDescEx, &svxGpuResourceVolumeSRVEx))
	{
	pCGpuManager->GenerateGpuResource(&svxGpuVolumeResourceDescEx, pVolumeEx, &svxGpuResourceVolumeSRVEx, NULL);
	}
	pdx11DeviceImmContext->CSSetShaderResources(31 + iIndexEx, 1, (ID3D11ShaderResourceView**)&svxGpuResourceVolumeSRVEx.vtrPtrs.at(0));

	// Constant Buffer //
	CB_VrVolumeObject cbVrVolumeObjEx;
	HDx11ComputeConstantBufferVrObject(&cbVrVolumeObjEx, bIsDownSample, pVolumeEx, pCCObject, svxGpuResourceVolumeTextureEx.GetSizeLogicalElement(), &psvxModuleParameter->mapCustomParamters);
	vxdouble4 d4ShadingFactor = vxdouble4(0.4, 0.6, 0.8, 70.0);
	pVolumeEx->GetCustomParameter(_T("_double4_ShadingFactors"), &d4ShadingFactor);
	cbVrVolumeObjEx.f4ShadingFactor = *(D3DXVECTOR4*)&vxfloat4(d4ShadingFactor);

	cbVrVolumeObjEx.iFlags = 0;
	int iSlabIntensity = 1;
	psvxModuleParameter->GetCustomParameter(&iSlabIntensity, _T("_int_SlabIntensity"));
	cbVrVolumeObjEx.fOpaqueCorrection *= (float)iSlabIntensity;
	cbVrVolumeObjEx.iFlags |= iSculptValue << 24;
	arrayCbVrObjects[iIndexEx] = cbVrVolumeObjEx;

	CB_VrOtf1D cbVrOtf1DEx;
	HDx11ComputeConstantBufferVrOtf1D(&cbVrOtf1DEx, pTObjectEx, 0, &psvxModuleParameter->mapCustomParamters);
	arrayCbOtfs[iIndexEx] = cbVrOtf1DEx;

	CB_VrModulation cbVrModulation;
	vxfloat3 f3PosEyeWS;
	pCMergeIObject->GetCameraObject()->GetCameraState(&f3PosEyeWS, NULL, NULL);
	HDx11ComputeConstantBufferVrModulation(&cbVrModulation, pVolumeEx, f3PosEyeWS, &psvxModuleParameter->mapCustomParamters);
	vxdouble3 d3ModulationFactor;
	pVolumeEx->GetCustomParameter(_T("_double3_ModulationFactors"), &d3ModulationFactor);
	cbVrModulation.fGradMagAmplifier = (float)d3ModulationFactor.x;
	cbVrModulation.fRevealingFactor = (float)d3ModulationFactor.y;
	cbVrModulation.fSharpnessFactor = (float)d3ModulationFactor.z;
	wstring strRenderModeEx;
	pVolumeEx->GetCustomParameter(_T("_string_RendererMode"), &strRenderModeEx);
	cbVrModulation.fdummy____0 = 0;
	if(strRenderModeEx.compare(_T("VR_MODULATION")) == 0)
	cbVrModulation.fdummy____0 = 1;

	arrayCbModulations[iIndexEx] = cbVrModulation;
	}

	pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_VOLOBJ_Ex], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResVolObj);
	CB_VrVolumeObject* pCBParamVolumeObjEx = (CB_VrVolumeObject*)mappedResVolObj.pData;
	memcpy(pCBParamVolumeObjEx, arrayCbVrObjects, sizeof(CB_VrVolumeObject) * 3);
	pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_VOLOBJ_Ex], 0);
	pdx11DeviceImmContext->CSSetConstantBuffers(8, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_VOLOBJ_Ex]);

	pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_OTF1D_Ex], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResOtf1D);
	CB_VrOtf1D* pCBParamOtf1DEx = (CB_VrOtf1D*)mappedResOtf1D.pData;
	memcpy(pCBParamOtf1DEx, arrayCbOtfs, sizeof(CB_VrOtf1D) * 3);
	pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_OTF1D_Ex], 0);
	pdx11DeviceImmContext->CSSetConstantBuffers(9, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_OTF1D_Ex]);

	D3D11_MAPPED_SUBRESOURCE mappedResModulation;
	pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_MODULATION_Ex], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResModulation);
	CB_VrModulation* pCBParamModulationEx = (CB_VrModulation*)mappedResModulation.pData;
	memcpy(pCBParamModulationEx, arrayCbModulations, sizeof(CB_VrModulation) * 3);
	pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_MODULATION_Ex], 0);
	pdx11DeviceImmContext->CSSetConstantBuffers(10, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_MODULATION_Ex]);

	if(iNumEXs == 1)
	pdx11DeviceImmContext->CSSetShader( *ppdx11CS_VRs[(int)__CS_VOLUME_MIX_SIMULTANEOUS], NULL, 0 );
	else
	pdx11DeviceImmContext->CSSetShader( *ppdx11CS_VRs[(int)__CS_VOLUME_MIX_SIMULTANEOUS_THREE], NULL, 0 );
	}
	#pragma endregion // Simultaneous Rendering Mode
	/**/
POSTVR:

	pdx11DeviceImmContext->ClearState();

	if (bSkipProcess || bIsShadowStage)
		return true;
	// IObject Marker //
	pCMergeIObject->RegisterCustomParameter(_T("_int_CountMerging"), iCountMerging);
	pCOutputIObject->SetDefineModuleSpecifier(_T("VS-GPUDX11VxRenderer module : Volume Renderer"));
	pCMergeIObject->SetDefineModuleSpecifier(_T("VS-GPUDX11VxRenderer module : Volume Renderer"));

	// Copy-back to system buffer //
	// Runtime Start From Constant Buffer Setting
	if (!bIsSystemOut)
	{
		QueryPerformanceFrequency(&lIntFreq);
		QueryPerformanceCounter(&lIntCntEnd);
		if (pdRunTime)
			*pdRunTime = (lIntCntEnd.QuadPart - lIntCntStart.QuadPart) / (double)(lIntFreq.QuadPart);
#ifdef ____MY_PERFORMANCE_TEST
		double dRunTime = (*pdRunTime * 1000);
		printf("Window (%dx%d), (ASynch) Deferred Intermixing : %f ms\n", i2SizeScreenCurrent.x, i2SizeScreenCurrent.y, (float)dRunTime);
#endif
		return true;
	}

	// Copy GPU to CPU //
	SVXFrameBuffer* psvxFrameBuffer = (SVXFrameBuffer*)pCOutputIObject->GetFrameBuffer(vxrFrameBufferUsageRENDEROUT, 0);
	vxbyte4* py4Buffer = (vxbyte4*)psvxFrameBuffer->pvBuffer;
	SVXFrameBuffer* psvxFrameBufferDepth1stHit = (SVXFrameBuffer*)pCOutputIObject->GetFrameBuffer(vxrFrameBufferUsageDEPTH, 0);
	float* pfBufferDepth1stHit = (float*)psvxFrameBufferDepth1stHit->pvBuffer;

	D3D11_MAPPED_SUBRESOURCE mappedResSys;
	pdx11DeviceImmContext->CopyResource((ID3D11Buffer*)gpuResourceFB_ARCHs[__FB_UBUF_SYSTEM_RENDEROUT].vtrPtrs.at(0),
		(ID3D11Buffer*)gpuResourceFB_ARCHs[__FB_UBUF_RENDEROUT].vtrPtrs.at(0));

	pdx11DeviceImmContext->Map((ID3D11Buffer*)gpuResourceFB_ARCHs[__FB_UBUF_SYSTEM_RENDEROUT].vtrPtrs.at(0), 0, D3D11_MAP_READ, NULL, &mappedResSys);

	// Time Check
	QueryPerformanceFrequency(&lIntFreq);
	QueryPerformanceCounter(&lIntCntEnd);
	if (pdRunTime)
		*pdRunTime = (lIntCntEnd.QuadPart - lIntCntStart.QuadPart) / (double)(lIntFreq.QuadPart);

#ifdef ____MY_PERFORMANCE_TEST
	double dRunTime = (*pdRunTime * 1000);
	printf("Window (%dx%d), Last-intermixing : %f ms\n", i2SizeScreenCurrent.x, i2SizeScreenCurrent.y, (float)dRunTime);
#endif

	RWB_Output_RenderOut* pOut = (RWB_Output_RenderOut*)mappedResSys.pData;

	QueryPerformanceCounter(&lIntCntStart);
	for (int i = 0; i < i2SizeScreenCurrent.y; i++)
	{
		for (int j = 0; j < i2SizeScreenCurrent.x; j++)
		{
			// BGRA
			int iAddrCpuMem = i*i2SizeScreenCurrent.x + j;
			int iAddrGpuBuffer = i*uiNumGridX*__BLOCKSIZE + j;
			memcpy(&py4Buffer[iAddrCpuMem], &pOut[iAddrGpuBuffer].iColor, sizeof(int));
			// 			py4Buffer[iAddrCpuMem].x = (byte)(min(pOut[iAddrGpuBuffer].f4Color.z*255.f, 255));
			// 			py4Buffer[iAddrCpuMem].y = (byte)(min(pOut[iAddrGpuBuffer].f4Color.y*255.f, 255));
			// 			py4Buffer[iAddrCpuMem].z = (byte)(min(pOut[iAddrGpuBuffer].f4Color.x*255.f, 255));
			// 			py4Buffer[iAddrCpuMem].w = (byte)(min(pOut[iAddrGpuBuffer].f4Color.w*255.f, 255));

			float fDepth1st = pOut[iAddrGpuBuffer].fDepth1stHit;
			pfBufferDepth1stHit[iAddrCpuMem] = fDepth1st;
			// 			if(fDepth1st == 0)
			// 				int gg = 0;
			// 			else
			// 				int gl = 0;
		}
	}

	if (bIsSimultaenousMode)
	{
		int iMergeIObjectID = pCMergeIObject->GetObjectID();
		auto itrStoryVolume = g_mapStoryArchive_Volumes.find(iMergeIObjectID);
		if (itrStoryVolume != g_mapStoryArchive_Volumes.end())
			g_mapStoryArchive_Volumes.erase(itrStoryVolume);

		auto itrStoryOTF = g_mapStoryArchive_TObjects.find(iMergeIObjectID);
		if (itrStoryOTF != g_mapStoryArchive_TObjects.end())
			g_mapStoryArchive_TObjects.erase(itrStoryOTF);
	};

	pdx11DeviceImmContext->Unmap((ID3D11Buffer*)gpuResourceFB_ARCHs[__FB_UBUF_SYSTEM_RENDEROUT].vtrPtrs.at(0), 0);

	return true;
}