#include "VS-GPUDX11VxControl.h"
#include "VXDX11Helper.h"
#include "RendererHeader.h"

//"GPU DX11 Vx Unified Renderer By Dojo 2012.09.28"
#define MODULEDEFINEDSPECIFIER "GPU DX11 Vx Unified Renderer By Dojo 2014.01.27"
#define RELEASE_MODE 

double g_dProgress = 0;
double g_dRunTimeVRs = 0;
GpuDX11CommonParameters* g_pVxgCommonParams = NULL;
SVXLocalProgress g_svxLocalProgress;

CVXGPUManager* g_pCGpuManager = NULL;

bool CheckIsAvailableParameters(SVXModuleParameters* pstModuleParameter)
{
	/////////////////////////////////////////////////
	// Check whether the module inputs are correct //
// 	vector<CVXObject*> vtrInputVolumes;
// 	VXHStringGetVXObjectListFromObjectStringMap(&vtrInputVolumes, &psvxModuleParameter->mapVXObjects, vxrObjectTypeVOLUME, true);
// 	vector<CVXObject*> vtrInputOTFs;
// 	VXHStringGetVXObjectListFromObjectStringMap(&vtrInputOTFs, &psvxModuleParameter->mapVXObjects, vxrObjectTypeTRANSFERFUNCTION, true);
// 	vector<CVXObject*> vtrOutputIPs;
// 	VXHStringGetVXObjectListFromObjectStringMap(&vtrOutputIPs, &psvxModuleParameter->mapVXObjects, vxrObjectTypeIMAGEPLANE, false);
// 
// 	if(vtrInputVolumes.size() == 0 || vtrInputOTFs.size() == 0 || vtrOutputIPs.size() == 0)
// 		return false;
	
	g_dProgress = 0;

	return true;
}

bool InitModule(SVXModuleParameters* psvxModuleParameter)
{	
	if(g_pCGpuManager == NULL)
		g_pCGpuManager = new CVXGPUManager(vxgGpuSdkTypeDX11, _T("VS-GPUDX11VxRenderer.dll"));
	if (!HDx11InitializeGpuStatesAndBasicResources(g_pCGpuManager, &g_pVxgCommonParams))
	{
		DeInitModule(NULL);
		return false;
	}

	return true;
}

bool DoModule(SVXModuleParameters* pstModuleParameter)
{
	if(g_pCGpuManager == NULL)
	{
		return false;
	}

	if(g_pVxgCommonParams->pdx11Device == NULL || g_pVxgCommonParams->pdx11DeviceImmContext == NULL)
	{
		return false;
	}

	g_svxLocalProgress.dStartProgress = 0;
	g_svxLocalProgress.dRangeProgress = 100;
	g_svxLocalProgress.pdProgressOfCurWork = &g_dProgress;

	CVXIObject* pCIObject = (CVXIObject*)pstModuleParameter->GetVXObject(vxrObjectTypeIMAGEPLANE, false, 0);
	if(pCIObject == NULL)
	{
		VXERRORMESSAGE("VxRenderer needs at least one IObject as output!");
		return false;
	}
	if(pCIObject->GetCameraObject() == NULL)
	{
		VXERRORMESSAGE("VxRenderer needs Camera Initializeation!");
		return false;
	}

#pragma region GPU/CPU Pre Setting
	double dSizeGpuResourceForVolume = 80;
	pstModuleParameter->GetCustomParameter(&dSizeGpuResourceForVolume, _T("_double_SizeGpuResourceForVolume"));
	// 100 means 50%
	double dResourceRatioForVolume = dSizeGpuResourceForVolume * 0.5 * 0.01;
	uint uiDedicatedGpuMemoryKB = (uint)(g_pVxgCommonParams->stDx11Adapter.DedicatedVideoMemory / 1024);
	double dHalfCriterionKB = uiDedicatedGpuMemoryKB * dResourceRatioForVolume;
	bool bIsSupportedCS = true;
	pstModuleParameter->GetCustomParameter(&bIsSupportedCS, _T("_bool_IsSupportedCS"));
	if (!bIsSupportedCS)
		dHalfCriterionKB = 16 * 1024;
	else
		dHalfCriterionKB = 256 * 1024;	// FIXED
	vector<CVXObject*> vtrInputVolumes;
	VXHStringGetVXObjectListFromObjectStringMap(&vtrInputVolumes, &pstModuleParameter->mapVXObjects, vxrObjectTypeVOLUME, true);
	for (int i = 0; i < (int)vtrInputVolumes.size(); i++)
		vtrInputVolumes.at(i)->RegisterCustomParameter(_T("_double_ForcedHalfCriterionKB"), dHalfCriterionKB);
#pragma endregion

	wstring strRendererSourceType = _T("MESH");
	pstModuleParameter->GetCustomParameter(&strRendererSourceType, _T("_string_RenderingSourceType"));

	bool bIsShadow = false;
	pstModuleParameter->GetCustomParameter(&bIsShadow, _T("_bool_IsShadow"));
	bool bGenerateShadowMap = false;
	pstModuleParameter->GetCustomParameter(&bGenerateShadowMap, _T("_bool_GenerateShadowMap"));
	bool bIsShadowStage = false;
	if (bIsShadow)
	{
		// Shadow Check //
		// To Do
		// pCIObject and TObject
		bIsShadowStage = true;
	}

	if (strRendererSourceType.compare(_T("VOLUME")) == 0)
	{
		ID3D11ComputeShader** ppdx11CS_VRs[NUMSHADERS_VR];
		for (int i = 0; i < NUMSHADERS_VR; i++)
			ppdx11CS_VRs[i] = &g_pVxgCommonParams->pdx11CS_VR_Shaders[i];
		ID3D11ComputeShader** ppdx11CS_MERGEs[NUMSHADERS_MERGE];
		for (int i = 0; i < NUMSHADERS_MERGE; i++)
			ppdx11CS_MERGEs[i] = &g_pVxgCommonParams->pdx11CS_MergeTextures[i];

		double dRuntime = 0;
		if (bIsShadowStage)
			RenderVrCommon(pstModuleParameter, g_pCGpuManager, g_pVxgCommonParams, ppdx11CS_VRs, ppdx11CS_MERGEs, true, false, &g_svxLocalProgress, &dRuntime);
		RenderVrCommon(pstModuleParameter, g_pCGpuManager, g_pVxgCommonParams, ppdx11CS_VRs, ppdx11CS_MERGEs, false, false, &g_svxLocalProgress, &dRuntime);
		g_dRunTimeVRs += dRuntime;
	}
	else if (strRendererSourceType.compare(_T("MESH")) == 0)
	{
		ID3D11InputLayout* pdx11ILs[NUMINPUTLAYOUTS] = { NULL };
		for (int i = 0; i < NUMINPUTLAYOUTS; i++)
			pdx11ILs[i] = g_pVxgCommonParams->pdx11IinputLayouts[i];

		ID3D11VertexShader** ppdx11VSs[NUMSHADERS_SR_VS] = { NULL };
		for (int i = 0; i < NUMSHADERS_SR_VS; i++)
			ppdx11VSs[i] = &g_pVxgCommonParams->pdx11VS_SR_Shaders[i];

		ID3D11Buffer* pdx11Bufs[3] = {
			g_pVxgCommonParams->pdx11BufProxyVertice,
			g_pVxgCommonParams->pdx11BufImposeVertice,
			g_pVxgCommonParams->pdx11BufImposeIndice,
		};

		ID3D11PixelShader** ppdx11PSs[NUMSHADERS_SR_PS];
		for (int i = 0; i < NUMSHADERS_SR_PS; i++)
			ppdx11PSs[i] = &g_pVxgCommonParams->pdx11PS_SR_Shaders[i];
		ID3D11ComputeShader** ppdx11CS_MERGEs[NUMSHADERS_MERGE];
		for (int i = 0; i < NUMSHADERS_MERGE; i++)
			ppdx11CS_MERGEs[i] = &g_pVxgCommonParams->pdx11CS_MergeTextures[i];
		double dRuntime = 0;
		RenderSrCommon(pstModuleParameter, g_pCGpuManager, g_pVxgCommonParams, pdx11ILs, ppdx11VSs, ppdx11PSs, ppdx11CS_MERGEs, pdx11Bufs, &g_svxLocalProgress, &dRuntime);
		g_dRunTimeVRs += dRuntime;
	}
	else if (strRendererSourceType.compare(_T("SECTIONAL_MESH")) == 0)
	{	
		// Only For Mesh
		ID3D11InputLayout* pdx11ILs[NUMINPUTLAYOUTS] = { NULL };
		for (int i = 0; i < NUMINPUTLAYOUTS; i++)
			pdx11ILs[i] = g_pVxgCommonParams->pdx11IinputLayouts[i];

		ID3D11VertexShader** ppdx11CommonVSs[NUMSHADERS_SR_VS] = { NULL };
		for (int i = 0; i < NUMSHADERS_SR_VS; i++)
			ppdx11CommonVSs[i] = &g_pVxgCommonParams->pdx11VS_SR_Shaders[i];

		ID3D11VertexShader** ppdx11PlaneVSs[NUMSHADERS_PLANE_SR_VS] = { NULL };
		for (int i = 0; i < NUMSHADERS_PLANE_SR_VS; i++)
			ppdx11PlaneVSs[i] = &g_pVxgCommonParams->pdx11VS_PLANE_SR_Shaders[i];

		ID3D11Buffer* pdx11Bufs[3] = {
			g_pVxgCommonParams->pdx11BufProxyVertice,
			g_pVxgCommonParams->pdx11BufImposeVertice,
			g_pVxgCommonParams->pdx11BufImposeIndice,
		};

		ID3D11PixelShader** ppdx11PSs[NUMSHADERS_SR_PS];
		for (int i = 0; i < NUMSHADERS_SR_PS; i++)
			ppdx11PSs[i] = &g_pVxgCommonParams->pdx11PS_SR_Shaders[i];

		double dRuntime = 0;
		RenderSrOnPlane(pstModuleParameter, g_pCGpuManager, g_pVxgCommonParams, pdx11ILs, ppdx11CommonVSs, ppdx11PlaneVSs, ppdx11PSs, pdx11Bufs, &g_svxLocalProgress, &dRuntime);
		g_dRunTimeVRs += dRuntime;
	}

	g_dProgress = 100;
	
	return true;
}

void DeInitModule(map<wstring, wstring>* pmapCustomParamters)
{
	HDx11ReleaseCommonResources();

	VXSAFE_DELETE(g_pCGpuManager);
}

double GetProgress()
{
	return (double)((int)g_dProgress % 100);
}

void GetModuleSpecification(vector<wstring>* pvtrSpecification)
{
	pvtrSpecification->push_back(_T("DX11"));
	pvtrSpecification->push_back(_T("WINDOWS"));
	pvtrSpecification->push_back(_T("GPUMANAGER"));
}

void InteropCustomWork(SVXModuleParameters* pstModuleParameter)
{
	if(pstModuleParameter == NULL)
		return;

	int* piGpuState = (int*)pstModuleParameter->GetCustomObject(_T("_out_int_GpuState"));
	if (piGpuState)
	{
		if (g_pCGpuManager == NULL)
			g_pCGpuManager = new CVXGPUManager(vxgGpuSdkTypeDX11, _T("VS-GPUDX11VxRenderer.dll"));
		*piGpuState = HDx11InitializeGpuStatesAndBasicResources(g_pCGpuManager, &g_pVxgCommonParams);

		int* piFeatureLevel = (int*)pstModuleParameter->GetCustomObject(_T("_out_int_FeatureLevelDX11"));
		if(piFeatureLevel)
		{
			*piFeatureLevel = g_pVxgCommonParams->eDx11FeatureLevel;
			// 0x10DE : NVIDIA
			// 0x1002, 0x1022 : AMD ATI
		}
		return;
	}

	CVXGPUManager** ppCGpuManager = (CVXGPUManager**)pstModuleParameter->GetCustomObject(_T("_outptr_class_GpuManager"));
	if(ppCGpuManager != NULL)
	{
		if (g_pCGpuManager == NULL)
			g_pCGpuManager = new CVXGPUManager(vxgGpuSdkTypeDX11, _T("VS-GPUDX11VxRenderer.dll"));
		HDx11InitializeGpuStatesAndBasicResources(g_pCGpuManager, &g_pVxgCommonParams);
		*ppCGpuManager = g_pCGpuManager;
	}

	void** ppvRendererMerger = (void**)pstModuleParameter->GetCustomObject(_T("_outptr_function_RendererMerger"));
	if (ppvRendererMerger != NULL)
	{
		*ppvRendererMerger = HDx11MixOut;
	}

	if(g_pVxgCommonParams->pdx11Device == NULL || g_pVxgCommonParams->pdx11DeviceImmContext == NULL)
	{
		bool* pbIsGpuLoaded = (bool*)pstModuleParameter->GetCustomObject(_T("_out_bool_IsLoadedVolume0"));
		if(pbIsGpuLoaded != NULL)
			*pbIsGpuLoaded = true;
		return;
	}

	double* pdInitialProgress = (double*)pstModuleParameter->GetCustomObject(_T("_in_double_InitializeProgress"));
	if(pdInitialProgress)
		g_dProgress = *pdInitialProgress;

	bool* pbIsGpuLoaded = (bool*)pstModuleParameter->GetCustomObject(_T("_out_bool_IsLoadedVolumes"));
	if(pbIsGpuLoaded != NULL)
	{
		*pbIsGpuLoaded = true;
		wstring strRendererResourceType = _T("VOLUME");
		pstModuleParameter->GetCustomParameter(&strRendererResourceType, _T("_string_RenderingSourceType"));

		if (g_pCGpuManager != NULL)
		{
			map<int, wstring> mapResourceVolumeIDs;	// ID, IsMaskVolume
			CVXLObject* pCLObjectForParamters = (CVXLObject*)pstModuleParameter->GetVXObject(vxrObjectTypeCUSTOMLIST, true, 0);
			map<wstring, vector<double>> mapDValueClear;

			if (strRendererResourceType.compare(_T("VOLUME")) == 0)
			{
				vector<int>* pvtrMainVolumeIDs = NULL;	// Rendering Volume List
				if (!pCLObjectForParamters->GetList(_T("_vlist_INT_MainVolumeIDs"), (void**)&pvtrMainVolumeIDs))
					VXERRORMESSAGE("GPU DVR! - ERROR 00");

				bool bValidateMaskVolume = false;
				pstModuleParameter->GetCustomParameter(&bValidateMaskVolume, _T("_bool_ValidateMaskVolume"));

				for (int i = 0; i < (int)pvtrMainVolumeIDs->size(); i++)
				{
					int iVolumeID = pvtrMainVolumeIDs->at(i);
					if (iVolumeID == 0)
						continue;
					mapResourceVolumeIDs[iVolumeID] = _T("TEXTURE3D_VOLUME_DEFAULT");
					map<wstring, vector<double>>* pmapDValueVolume = NULL;
					pCLObjectForParamters->GetCustomDValue(iVolumeID, (void**)&pmapDValueVolume);
					if (pmapDValueVolume == NULL)
						pmapDValueVolume = &mapDValueClear;

					int iRendererType = 0;
					VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_int_RendererType"), &iRendererType);

					int iMaskVolumeID = 0;
					VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_int_MaskVolumeObjectID"), &iMaskVolumeID);

#define __RM_SCULPTMASK 4
					if (iMaskVolumeID != 0 &&
						(bValidateMaskVolume || iRendererType == __RM_SCULPTMASK))
						mapResourceVolumeIDs[iMaskVolumeID] = _T("TEXTURE3D_VOLUME_DEFAULT_MASK");
				}

				int iLoadedVolumeCount = 0;
				auto itrVolumeIDs = mapResourceVolumeIDs.begin();
				for (; itrVolumeIDs != mapResourceVolumeIDs.end(); itrVolumeIDs++)
				{
					int iVolumeID = itrVolumeIDs->first;
					vector<SVXGPUResourceArchive*> vtrGpuVolumes;
					if (g_pCGpuManager->GetGpuResourceArchivesByObjectID(iVolumeID, &vtrGpuVolumes) == 0)
						break;
					iLoadedVolumeCount++;
				}
				if (iLoadedVolumeCount < (int)mapResourceVolumeIDs.size())
					*pbIsGpuLoaded = false;
			}
			else if (strRendererResourceType.compare(_T("MESH")) == 0)
			{
				vector<CVXObject*> vtrInputPrimitives;
				int iNumMeshes = VXHStringGetVXObjectListFromObjectStringMap(&vtrInputPrimitives, &pstModuleParameter->mapVXObjects, vxrObjectTypePRIMITIVE, true);
				vector<CVXObject*> vtrInputVolumes;
				int iNumVolumes = VXHStringGetVXObjectListFromObjectStringMap(&vtrInputVolumes, &pstModuleParameter->mapVXObjects, vxrObjectTypeVOLUME, true);
				map<int, CVXObject*> mapAssociatedObjects;
				for (int i = 0; i < vtrInputVolumes.size(); i++)
					mapAssociatedObjects.insert(pair<int, CVXObject*>(vtrInputVolumes[i]->GetObjectID(), vtrInputVolumes[i]));

				uint uiNumPrimitiveObjects = (uint)vtrInputPrimitives.size();
				for (uint i = 0; i < uiNumPrimitiveObjects; i++)
				{
					CVXVObjectPrimitive* pCVMeshObj = (CVXVObjectPrimitive*)vtrInputPrimitives[i];
					if (pCVMeshObj->IsDefinedResource() == false)
						continue;

					bool bIsVisibleItem = pCVMeshObj->GetVisibility();
					if (!bIsVisibleItem)
						continue;

					int iVolumeID = 0;
					int iMeshObjID = pCVMeshObj->GetObjectID();
					map<wstring, vector<double>>* pmapDValueMesh = NULL;
					pCLObjectForParamters->GetCustomDValue(iMeshObjID, (void**)&pmapDValueMesh);
					if (pmapDValueMesh == NULL)
						pmapDValueMesh = &mapDValueClear;
					VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_int_AssociatedVolumeID"), &iVolumeID);

					map<wstring, vector<double>> mapDValueClear;
					auto itrVolume = mapAssociatedObjects.find(iVolumeID);
					if (iVolumeID != 0 && itrVolume != mapAssociatedObjects.end())
					{
						CVXVObjectVolume* pCVolume = (CVXVObjectVolume*)itrVolume->second;
						bool bIsImageStack = false;
						pCVolume->GetCustomParameter(_T("_bool_IsImageStack"), &bIsImageStack);
						if (bIsImageStack)
							mapResourceVolumeIDs[iVolumeID] = _T("TEXTURE2DARRAY_IMAGESTACK");
						else
							mapResourceVolumeIDs[iVolumeID] = _T("TEXTURE3D_VOLUME_DEFAULT");
					}
				}

				int iVolumeID_Superimpose = 0;
				pstModuleParameter->GetCustomParameter(&iVolumeID_Superimpose, _T("_int_SuperimposeVolumeID"));
				if (iVolumeID_Superimpose > 0)
					mapResourceVolumeIDs[iVolumeID_Superimpose] = _T("TEXTURE3D_VOLUME_DEFAULT");;

				int iLoadedVolumeCount = 0;
				auto itrVolumeIDs = mapResourceVolumeIDs.begin();
				for (; itrVolumeIDs != mapResourceVolumeIDs.end(); itrVolumeIDs++)
				{
					int iVolumeID = itrVolumeIDs->first;
					vector<SVXGPUResourceArchive*> vtrGpuVolumes;
					if (g_pCGpuManager->GetGpuResourceArchivesByObjectID(iVolumeID, &vtrGpuVolumes) == 0)
						break;
					iLoadedVolumeCount++;
				}
				if (iLoadedVolumeCount < (int)mapResourceVolumeIDs.size())
					*pbIsGpuLoaded = false;
			}	// Mesh or Volume

			bool bCallGpuUpload = false;
			bool* pbCallGpuUpload = (bool*)pstModuleParameter->GetCustomObject(_T("_in_bool_CallGpuUpload"));
			if (pbCallGpuUpload)
				bCallGpuUpload = *pbCallGpuUpload;
			if (!*pbIsGpuLoaded && bCallGpuUpload)
			{
				g_dProgress = 0;
				bool bIsGpuVolumeRendering = true;
				bool* pbIsGpuVolumeRendering = (bool*)pstModuleParameter->GetCustomObject(_T("_in_bool_IsGpuVolumeRendering"));
				if (pbIsGpuVolumeRendering)
					bIsGpuVolumeRendering = *pbIsGpuVolumeRendering;
				double dHalfCriterionKB = 256 * 1024;
				if (!bIsGpuVolumeRendering)
					dHalfCriterionKB = 16 * 1024;
				vector<CVXObject*> vtrInputVolumes;
				VXHStringGetVXObjectListFromObjectStringMap(&vtrInputVolumes, &pstModuleParameter->mapVXObjects, vxrObjectTypeVOLUME, true);
				for (int i = 0; i < (int)vtrInputVolumes.size(); i++)
				{
					CVXVObjectVolume* pCVolume = (CVXVObjectVolume*)vtrInputVolumes.at(i);
					pCVolume->RegisterCustomParameter(_T("_double_ForcedHalfCriterionKB"), dHalfCriterionKB);
					auto itrVolumeTarget = mapResourceVolumeIDs.find(pCVolume->GetObjectID());
					if (itrVolumeTarget != mapResourceVolumeIDs.end())
					{
						SVXGPUResourceArchive stGpuResourceVolumeTexture, stGpuResourceVolumeSRV;
						g_svxLocalProgress.dStartProgress = 0;
						g_svxLocalProgress.dRangeProgress = 100;
						g_svxLocalProgress.pdProgressOfCurWork = &g_dProgress;
						HDx11UploadDefaultVolume(&stGpuResourceVolumeTexture, &stGpuResourceVolumeSRV, pCVolume, itrVolumeTarget->second, &g_svxLocalProgress);
					}
				}
				g_dProgress = 100;
			}
		}
	}
	
	double* pdRunTimeVRs = (double*)VXHStringGetCustomObjectFromPointerStringMap(&pstModuleParameter->mapCustomObjects, _T("_out_double_RunTimeVRs"));
	if(pdRunTimeVRs)
	{
		*pdRunTimeVRs = g_dRunTimeVRs;
		g_dRunTimeVRs = 0;
	}
}