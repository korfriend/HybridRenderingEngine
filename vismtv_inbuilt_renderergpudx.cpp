#include "vismtv_inbuilt_renderergpudx.h"
//#include "VXDX11Helper.h"
#include "gpures_helper_old.h"
#include "RendererHeader_legacy.h"
#include "RendererHeader.h"

#include <iostream>

//GPU Direct3D Renderer - (c)DongJoon Kim
#define MODULEDEFINEDSPECIFIER "GPU Direct3D Renderer - (c)DongJoon Kim"
//#define RELEASE_MODE 

double g_dProgress = 0;
double g_dRunTimeVRs = 0;

#define ENABLE_LEGACY
#define ENABLE_NEWOIT
#ifdef ENABLE_LEGACY
grd_helper_legacy::GpuDX11CommonParameters g_vmCommonParams_legacy;
#endif
#ifdef ENABLE_NEWOIT
grd_helper::GpuDX11CommonParameters g_vmCommonParams;
#endif
LocalProgress g_LocalProgress;

VmGpuManager* g_pCGpuManager = NULL;

bool CheckModuleParameters(fncontainer::VmFnContainer& _fncontainer)
{
	/////////////////////////////////////////////////
	// Check whether the module inputs are correct //
// 	vector<VmObject*> vtrInputVolumes;
// 	_fncontainer.GetVmObjectList(&vtrInputVolumes, VmObjKey(ObjectTypeVOLUME, true));
// 	vector<VmObject*> vtrInputOTFs;
// 	_fncontainer.GetVmObjectList(&vtrInputOTFs, VmObjKey(ObjectTypeTMAP, true));
// 	vector<VmObject*> vtrOutputIPs;
// 	_fncontainer.GetVmObjectList(&vtrOutputIPs, VmObjKey(ObjectTypeIMAGEPLANE, false));
// 
// 	if(vtrInputVolumes.size() == 0 || vtrInputOTFs.size() == 0 || vtrOutputIPs.size() == 0)
// 		return false;
	
	g_dProgress = 0;

	return true;
}

bool InitModule(fncontainer::VmFnContainer& _fncontainer)
{	
	if(g_pCGpuManager == NULL)
		g_pCGpuManager = new VmGpuManager(GpuSdkTypeDX11, "vismtv_inbuilt_renderergpudx.dll");

#ifdef ENABLE_LEGACY
	if (grd_helper_legacy::InitializePresettings(g_pCGpuManager, g_vmCommonParams_legacy) == -1)
	{
		std::cout << "failure legacy initializer!" << std::endl;
		DeInitModule(fncontainer::VmFnContainer());
		return false;
	}
#endif
#ifdef ENABLE_NEWOIT
	if (grd_helper::InitializePresettings(g_pCGpuManager, g_vmCommonParams) == -1)
	{
		std::cout << "failure new initializer!" << std::endl;
		DeInitModule(fncontainer::VmFnContainer());
		return false;
	}
#endif

	return true;
}

bool DoModule(fncontainer::VmFnContainer& _fncontainer)
{
	if(g_pCGpuManager == NULL)
	{
		return false;
	}

#ifdef ENABLE_LEGACY
	if (g_vmCommonParams_legacy.pdx11Device == NULL || g_vmCommonParams_legacy.pdx11DeviceImmContext == NULL)
	{
		if (grd_helper_legacy::InitializePresettings(g_pCGpuManager, g_vmCommonParams_legacy) == -1)
		{
			DeInitModule(fncontainer::VmFnContainer());
			return false;
		}
	}
#endif
#ifdef ENABLE_NEWOIT
	if (g_vmCommonParams.dx11Device == NULL || g_vmCommonParams.dx11DeviceImmContext == NULL)
	{
		if (grd_helper::InitializePresettings(g_pCGpuManager, g_vmCommonParams) == -1)
		{
			DeInitModule(fncontainer::VmFnContainer());
			return false;
		}
	}
#endif

	g_LocalProgress.start = 0;
	g_LocalProgress.range = 100;
	g_LocalProgress.progress_ptr = &g_dProgress;

	VmIObject* pCIObject = (VmIObject*)_fncontainer.GetVmObject(VmObjKey(ObjectTypeIMAGEPLANE, false), 0);
	if(pCIObject == NULL)
	{
		VMERRORMESSAGE("VisMotive Renderer needs at least one IObject as output!");
		return false;
	}
	if(pCIObject->GetCameraObject() == NULL)
	{
		VMERRORMESSAGE("VisMotive Renderer needs Camera Initializeation!");
		return false;
	}

#pragma region GPU/CPU Pre Setting
	double dSizeGpuResourceForVolume = _fncontainer.GetParamValue("_double_SizeGpuResourceForVolume", 80.0);
	// 100 means 50%
	double dResourceRatioForVolume = dSizeGpuResourceForVolume * 0.5 * 0.01;
	uint uiDedicatedGpuMemoryKB = 
#if defined(ENABLE_LEGACY)
		(uint)(g_vmCommonParams_legacy.stDx11Adapter.DedicatedVideoMemory / 1024);
#elif defined(ENABLE_NEWOIT)
		(uint)(g_vmCommonParams.stDx11Adapter.DedicatedVideoMemory / 1024);
#endif
	double dHalfCriterionKB = uiDedicatedGpuMemoryKB * dResourceRatioForVolume;
	dHalfCriterionKB = _fncontainer.GetParamValue("_double_GpuVolumeMaxSizeKB", 256.0 * 1024.0);
	// In CPU VR mode, Recommend to set dHalfCriterionKB = 16;
	vector<VmObject*> vtrInputVolumes;
	_fncontainer.GetVmObjectList(&vtrInputVolumes, VmObjKey(ObjectTypeVOLUME, true));
	for (int i = 0; i < (int)vtrInputVolumes.size(); i++)
		vtrInputVolumes.at(i)->RegisterCustomParameter("_double_ForcedHalfCriterionKB", dHalfCriterionKB);
#pragma endregion

	string strRendererSourceType = _fncontainer.GetParamValue("_string_RenderingSourceType", string("MESH"));
	bool test_oit = _fncontainer.GetParamValue("_bool_TestOit", true);
	//test_oit = true;
	bool is_shadow = _fncontainer.GetParamValue("_bool_IsShadow", false);

#if defined(ENABLE_LEGACY)
	bool generate_shadowmap = _fncontainer.GetParamValue("_bool_GenerateShadowMap", false);

	// ID3D11PixelShader
	ID3D11PixelShader** ppdx11PSs[NUMSHADERS_SR_PS];
	for (int i = 0; i < NUMSHADERS_SR_PS; i++)
		ppdx11PSs[i] = &g_vmCommonParams_legacy.pdx11PS_SR_Shaders[i];

	// ID3D11GeometryShader
	ID3D11GeometryShader** ppdx11GSs[NUMSHADERS_GS];
	for (int i = 0; i < NUMSHADERS_GS; i++)
		ppdx11GSs[i] = &g_vmCommonParams_legacy.pdx11GS_Shaders[i];

	// ID3D11ComputeShader
	ID3D11ComputeShader** ppdx11CS_VRs[NUMSHADERS_VR_CS];
	for (int i = 0; i < NUMSHADERS_VR_CS; i++)
		ppdx11CS_VRs[i] = &g_vmCommonParams_legacy.pdx11CS_VR_Shaders[i];
	ID3D11ComputeShader** ppdx11CS_MERGEs[NUMSHADERS_MERGE_CS];
	for (int i = 0; i < NUMSHADERS_MERGE_CS; i++)
		ppdx11CS_MERGEs[i] = &g_vmCommonParams_legacy.pdx11CS_MergeTextures[i];

	// ID3D11VertexShader
	ID3D11VertexShader** ppdx11VSs[NUMSHADERS_SR_VS];
	for (int i = 0; i < NUMSHADERS_SR_VS; i++)
		ppdx11VSs[i] = &g_vmCommonParams_legacy.pdx11VS_SR_Shaders[i];
	ID3D11VertexShader** ppdx11PlaneVSs[NUMSHADERS_PLANE_SR_VS];
	for (int i = 0; i < NUMSHADERS_PLANE_SR_VS; i++)
		ppdx11PlaneVSs[i] = &g_vmCommonParams_legacy.pdx11VS_PLANE_SR_Shaders[i];
	ID3D11VertexShader** ppdx11FBiasVSs[NUMSHADERS_BIASZ_SR_VS];
	for (int i = 0; i < NUMSHADERS_BIASZ_SR_VS; i++)
		ppdx11FBiasVSs[i] = &g_vmCommonParams_legacy.pdx11VS_FBIAS_SR_Shaders[i];

	// ID3D11InputLayout
	ID3D11InputLayout* pdx11ILs[NUMINPUTLAYOUTS];
	for (int i = 0; i < NUMINPUTLAYOUTS; i++)
		pdx11ILs[i] = g_vmCommonParams_legacy.pdx11IinputLayouts[i];
#endif

	if (strRendererSourceType.compare("VOLUME") == 0)
	{
		double dRuntime = 0;
		if (!test_oit)
		{
#if defined(ENABLE_LEGACY)
			if (is_shadow)
				RenderVrCommonCS(&_fncontainer, g_pCGpuManager, &g_vmCommonParams_legacy,
					ppdx11CS_VRs, ppdx11CS_MERGEs, *ppdx11VSs[__VS_PROXY], pdx11ILs[0],
					g_vmCommonParams_legacy.pdx11BufProxyVertice, true, &g_LocalProgress, &dRuntime);
			RenderVrCommonCS(&_fncontainer, g_pCGpuManager, &g_vmCommonParams_legacy,
				ppdx11CS_VRs, ppdx11CS_MERGEs, *ppdx11VSs[__VS_PROXY], pdx11ILs[0],
				g_vmCommonParams_legacy.pdx11BufProxyVertice, false, &g_LocalProgress, &dRuntime);
#endif
		}
		else
		{
#if defined(ENABLE_NEWOIT)
			RenderVrDLS(&_fncontainer, g_pCGpuManager, &g_vmCommonParams, &g_LocalProgress, &dRuntime);
#endif
		}

		g_dRunTimeVRs += dRuntime;
	}
	else if (strRendererSourceType.compare("MESH") == 0) // MESH
	{
		double dRuntime = 0;
		if (!test_oit)
		{
#if defined(ENABLE_LEGACY)
			RenderSrCommonCS(&_fncontainer, g_pCGpuManager, &g_vmCommonParams_legacy,
				pdx11ILs, ppdx11VSs, ppdx11FBiasVSs, ppdx11PSs, ppdx11GSs, ppdx11CS_MERGEs, &g_vmCommonParams_legacy.pdx11CS_Outline,
				g_vmCommonParams_legacy.pdx11BufProxyVertice, &g_LocalProgress, &dRuntime);
#endif
		}
		else
		{
#if defined(ENABLE_NEWOIT)
			RenderSrOIT(&_fncontainer, g_pCGpuManager, &g_vmCommonParams, &g_LocalProgress, &dRuntime);
#endif
		}
		g_dRunTimeVRs += dRuntime;
	}
	else if (strRendererSourceType.compare("SECTIONAL_MESH") == 0)
	{	
		double dRuntime = 0;
#if defined(ENABLE_LEGACY)
		RenderSrOnPlane(&_fncontainer, g_pCGpuManager, &g_vmCommonParams_legacy,
			pdx11ILs, ppdx11VSs, ppdx11PlaneVSs, ppdx11PSs, ppdx11CS_MERGEs, &g_LocalProgress, &dRuntime);
#endif
		g_dRunTimeVRs += dRuntime;
	}

	g_dProgress = 100;
	
	return true;
}

void DeInitModule(fncontainer::VmFnContainer& _fncontainer)
{
	// ORDER!!
	grd_helper_legacy::DeinitializePresettings();
	grd_helper::DeinitializePresettings();
	VMSAFE_DELETE(g_pCGpuManager);
}

double GetProgress()
{
	return (double)((int)g_dProgress % 100);
}

void GetModuleSpecification(std::vector<std::string>& requirements)
{
	requirements.push_back("DX11");
	requirements.push_back("WINDOWS");
	requirements.push_back("GPUMANAGER");
}

void InteropCustomWork(fncontainer::VmFnContainer& _fncontainer)
{
	if(_fncontainer.is_empty())
		return;

	int* piGpuState = (int*)_fncontainer.ReadRmwBufferPtr("_out_int_GpuState", (int*)NULL);
	if (piGpuState)
	{
		if (g_pCGpuManager == NULL)
			g_pCGpuManager = new VmGpuManager(GpuSdkTypeDX11, "vismtv_inbuilt_renderergpudx.dll");

#if defined(ENABLE_LEGACY)
		*piGpuState = grd_helper_legacy::InitializePresettings(g_pCGpuManager, g_vmCommonParams_legacy);
#endif
#if defined(ENABLE_NEWOIT)
		*piGpuState = grd_helper::InitializePresettings(g_pCGpuManager, g_vmCommonParams);
#endif

		int* piFeatureLevel = (int*)_fncontainer.ReadRmwBufferPtr("_out_int_FeatureLevelDX11", (int*)NULL);
		if(piFeatureLevel)
		{
			*piFeatureLevel = 
#if defined(ENABLE_LEGACY)
				g_vmCommonParams_legacy.eDx11FeatureLevel;
#elif defined(ENABLE_NEWOIT)
				g_vmCommonParams.dx11_featureLevel;
#endif
			// 0x10DE : NVIDIA
			// 0x1002, 0x1022 : AMD ATI
		}
		return;
	}

	VmGpuManager** ppCGpuManager = (VmGpuManager**)_fncontainer.ReadRmwBufferPtr("_outptr_class_GpuManager", (VmGpuManager**)NULL);
	if(ppCGpuManager != NULL)
	{
		if (g_pCGpuManager == NULL)
			g_pCGpuManager = new VmGpuManager(GpuSdkTypeDX11, "vismtv_inbuilt_renderergpudx.dll");
#if defined(ENABLE_LEGACY)
		grd_helper_legacy::InitializePresettings(g_pCGpuManager, g_vmCommonParams_legacy);
#endif
#if defined(ENABLE_NEWOIT)
		grd_helper::InitializePresettings(g_pCGpuManager, g_vmCommonParams);
#endif
		*ppCGpuManager = g_pCGpuManager;
	}

	//CVxCommander.VxNew().VxSetModuleCustomPointer("vismtv_inbuilt_renderergpudx", "Renderer Merger", "_outptr_function_RendererMerger", true);
	//CVxCommander.VxNew().VxSetModuleCustomPointer("vismtv_inbuilt_renderercpu", "Renderer Merger", "_inptr_function_RendererMerger", false);
	//void** ppvRendererMerger = (void**)_fncontainer.ReadRmwBufferPtr("_outptr_function_RendererMerger"));
	//if (ppvRendererMerger != NULL)
	//{
	//	*ppvRendererMerger = HDx11MixOut;
	//}

#if defined(ENABLE_LEGACY)
	if (g_vmCommonParams_legacy.pdx11Device == NULL || g_vmCommonParams_legacy.pdx11DeviceImmContext == NULL)
#elif defined(ENABLE_NEWOIT)
	if (g_vmCommonParams.dx11Device == NULL || g_vmCommonParams.dx11DeviceImmContext == NULL)
#endif
	{
		bool* pbIsGpuLoaded = (bool*)_fncontainer.ReadRmwBufferPtr("_out_bool_IsLoadedVolume0", (int*)NULL);
		if(pbIsGpuLoaded != NULL)
			*pbIsGpuLoaded = true;
		return;
	}

	double* pdInitialProgress = (double*)_fncontainer.ReadRmwBufferPtr("_in_double_InitializeProgress", (double*)NULL);
	if(pdInitialProgress)
		g_dProgress = *pdInitialProgress;

	bool* pbIsGpuLoaded = (bool*)_fncontainer.ReadRmwBufferPtr("_out_bool_IsLoadedVolumes", (bool*)NULL);
	if(pbIsGpuLoaded != NULL)
	{
		bool bCallGpuUpload = false;
		bool* pbCallGpuUpload = (bool*)_fncontainer.ReadRmwBufferPtr("_in_bool_CallGpuUpload", (bool*)NULL);
		if (pbCallGpuUpload)
			bCallGpuUpload = *pbCallGpuUpload;

		*pbIsGpuLoaded = true;
		string strRendererResourceType = _fncontainer.GetParamValue("_string_RenderingSourceType", string("VOLUME"));

		if (g_pCGpuManager != NULL && bCallGpuUpload)
		{
			VmLObject* lobj = (VmLObject*)_fncontainer.GetVmObject(VmObjKey(ObjectTypeBUFFER, true), 0);
			if (strRendererResourceType.compare("VOLUME") == 0)
			{
				g_dProgress = 0;
				vector<VmObject*> vtrInputVolumes;
				_fncontainer.GetVmObjectList(&vtrInputVolumes, VmObjKey(ObjectTypeVOLUME, true));
				VmLObject* lobj = (VmLObject*)_fncontainer.GetVmObject(VmObjKey(ObjectTypeBUFFER, true), 0);
				map<int, VmVObjectVolume*> mapVolumes;
				for (int i = 0; i < (int)vtrInputVolumes.size(); i++)
				{
					VmVObjectVolume* vol_obj = (VmVObjectVolume*)vtrInputVolumes[i];
					mapVolumes.insert(pair<int, VmVObjectVolume*>(vol_obj->GetObjectID(), vol_obj));
				}
				int* pMainVolumeIDs = NULL;	// Rendering Volume List
				size_t bytes_pMainVolumeIDs;
				if (lobj->LoadBufferPtr("_vlist_INT_MainVolumeIDs", (void**)&pMainVolumeIDs, bytes_pMainVolumeIDs))
				{
					int last_render_vol_id = _fncontainer.GetParamValue("_int_LastRenderVolumeID", (vtrInputVolumes.at(vtrInputVolumes.size() - 1))->GetObjectID());
					int num_pMainVolumeIDs = voo::get_num_from_bytes<int>(bytes_pMainVolumeIDs);

					vector<int> vtrOrderedMainVolumeIDs;
					bool bIsValidList = false;
					for (int i = 0; i < num_pMainVolumeIDs; i++)
					{
						int iVolumeID = pMainVolumeIDs[i];
						auto itrVolume = mapVolumes.find(iVolumeID);
						if (itrVolume == mapVolumes.end())
						{
							VMERRORMESSAGE("GPU DVR! - INVALID VOLUME ID");
							return;
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
						return;
					}
					vtrOrderedMainVolumeIDs.push_back(last_render_vol_id);

					for (int i = 0; i < (int)vtrOrderedMainVolumeIDs.size(); i++)
					{
						int vol_obj_id = vtrOrderedMainVolumeIDs[i];
						int render_type = 0;
						lobj->GetDstObjValue(vol_obj_id, ("_int_RendererType"), &render_type);
						int mask_vol_id = 0;
						lobj->GetDstObjValue(vol_obj_id, ("_int_MaskVolumeObjectID"), &mask_vol_id);
						VmVObjectVolume* main_vol_obj = mapVolumes[vol_obj_id];
						VmVObjectVolume* mask_vol_obj = mapVolumes[mask_vol_id];
						GpuRes gres_main_vol, gres_mask_vol;

						g_LocalProgress.start = 0;
						g_LocalProgress.range = 100;
						g_LocalProgress.progress_ptr = &g_dProgress;

						grd_helper_legacy::UpdateVolumeModel(gres_main_vol, main_vol_obj, render_type == 5 /*__RM_SURFACECCF*/, &g_LocalProgress);

						g_LocalProgress.start = 0;
						g_LocalProgress.range = 100;
						g_LocalProgress.progress_ptr = &g_dProgress;

						grd_helper_legacy::UpdateVolumeModel(gres_mask_vol, mask_vol_obj, true, &g_LocalProgress);
					}
				}
				g_dProgress = 100;
			}
			else if (strRendererResourceType.compare("MESH") == 0)
			{
				vector<VmObject*> vtrInputVolumes;
				_fncontainer.GetVmObjectList(&vtrInputVolumes, VmObjKey(ObjectTypeVOLUME, true));
				vector<VmObject*> vtrInputTObjects;
				_fncontainer.GetVmObjectList(&vtrInputTObjects, VmObjKey(ObjectTypeTMAP, true));

				map<int, VmObject*> mapAssociatedObjects;
				for (int i = 0; i < (int)vtrInputVolumes.size(); i++)
					mapAssociatedObjects.insert(pair<int, VmObject*>(vtrInputVolumes[i]->GetObjectID(), vtrInputVolumes[i]));
				for (int i = 0; i < (int)vtrInputTObjects.size(); i++)
					mapAssociatedObjects.insert(pair<int, VmObject*>(vtrInputTObjects[i]->GetObjectID(), vtrInputTObjects[i]));

				auto find_asscociated_obj = [&mapAssociatedObjects](int obj_id) -> VmObject*
				{
					auto vol_obj = mapAssociatedObjects.find(obj_id);
					if (vol_obj == mapAssociatedObjects.end()) return NULL;
					return vol_obj->second;
				};

				map<int, GpuRes> mapGpuRes_Vtx;
				map<int, GpuRes> mapGpuRes_Idx;
				map<int, GpuRes> mapGpuRes_Tex;
				map<int, GpuRes> mapGpuRes_VolumeAndTMap;
				vector<VmObject*> vtrInputPrimitives;
				int num_prim_objs = _fncontainer.GetVmObjectList(&vtrInputPrimitives, VmObjKey(ObjectTypePRIMITIVE, true));
				for (int i = 0; i < num_prim_objs; i++)
				{
					VmVObjectPrimitive* prim_obj = (VmVObjectPrimitive*)vtrInputPrimitives[i];
					if (prim_obj->IsDefined() == false)
						continue;

					int prim_obj_id = prim_obj->GetObjectID();

					int iVolumeID = 0, iTObjectID = 0;
					lobj->GetDstObjValue(prim_obj_id, ("_int_AssociatedVolumeID"), &iVolumeID);
					lobj->GetDstObjValue(prim_obj_id, ("_int_AssociatedTObjectID"), &iTObjectID);

					VmVObjectVolume* vol_obj = (VmVObjectVolume*)find_asscociated_obj(iVolumeID);
					VmTObject* tobj = (VmTObject*)find_asscociated_obj(iTObjectID);

					g_LocalProgress.start = 0;
					g_LocalProgress.range = 100;
					g_LocalProgress.progress_ptr = &g_dProgress;
					RegisterVolumeRes(vol_obj, tobj, lobj, g_pCGpuManager, g_vmCommonParams_legacy.pdx11DeviceImmContext, mapAssociatedObjects,
						mapGpuRes_VolumeAndTMap, &g_LocalProgress);

					g_LocalProgress.start = 0;
					g_LocalProgress.range = 100;
					g_LocalProgress.progress_ptr = &g_dProgress;
					GpuRes gres_vtx, gres_idx, gres_tex;
					grd_helper_legacy::UpdatePrimitiveModel(gres_vtx, gres_idx, gres_tex, prim_obj, &g_LocalProgress);
					if (gres_vtx.alloc_res_ptrs.size() > 0)
						mapGpuRes_Vtx.insert(pair<int, GpuRes>(prim_obj_id, gres_vtx));
					if (gres_idx.alloc_res_ptrs.size() > 0)
						mapGpuRes_Idx.insert(pair<int, GpuRes>(prim_obj_id, gres_idx));
					if (gres_tex.alloc_res_ptrs.size() > 0)
						mapGpuRes_Tex.insert(pair<int, GpuRes>(prim_obj_id, gres_tex));
				}
			}
		}
	}
	
	double* pdRunTimeVRs = (double*)_fncontainer.ReadRmwBufferPtr("_out_double_RunTimeVRs", (double*)NULL);
	if(pdRunTimeVRs)
	{
		*pdRunTimeVRs = g_dRunTimeVRs;
		g_dRunTimeVRs = 0;
	}
}