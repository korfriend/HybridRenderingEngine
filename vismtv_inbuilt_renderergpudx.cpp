#include "vismtv_inbuilt_renderergpudx.h"
//#include "VXDX11Helper.h"
#include "RendererHeader.h"

#include <iostream>

//GPU Direct3D Renderer - (c)DongJoon Kim
#define MODULEDEFINEDSPECIFIER "GPU Direct3D Renderer - (c)DongJoon Kim"
//#define RELEASE_MODE 

double g_dProgress = 0;
double g_dRunTimeVRs = 0;
grd_helper::GpuDX11CommonParameters g_vmCommonParams;
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

	if (grd_helper::InitializePresettings(g_pCGpuManager, &g_vmCommonParams) == -1)
	{
		std::cout << "failure new initializer!" << std::endl;
		DeInitModule(fncontainer::VmFnContainer());
		return false;
	}

	return true;
}

bool DoModule(fncontainer::VmFnContainer& _fncontainer)
{
	if(g_pCGpuManager == NULL)
	{
		return false;
	}

	if (g_vmCommonParams.dx11Device == NULL || g_vmCommonParams.dx11DeviceImmContext == NULL)
	{
		if (grd_helper::InitializePresettings(g_pCGpuManager, &g_vmCommonParams) == -1)
		{
			DeInitModule(fncontainer::VmFnContainer());
			return false;
		}
	}

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
		(uint)(g_vmCommonParams.dx11_adapter.DedicatedVideoMemory / 1024);
	double dHalfCriterionKB = uiDedicatedGpuMemoryKB * dResourceRatioForVolume;
	dHalfCriterionKB = _fncontainer.GetParamValue("_double_GpuVolumeMaxSizeKB", 256.0 * 1024.0);
	// In CPU VR mode, Recommend to set dHalfCriterionKB = 16;
	vector<VmObject*> vtrInputVolumes;
	_fncontainer.GetVmObjectList(&vtrInputVolumes, VmObjKey(ObjectTypeVOLUME, true));
	for (int i = 0; i < (int)vtrInputVolumes.size(); i++)
		vtrInputVolumes.at(i)->RegisterCustomParameter("_double_ForcedHalfCriterionKB", dHalfCriterionKB);
#pragma endregion

	string strRendererSourceType = _fncontainer.GetParamValue("_string_RenderingSourceType", string("MESH"));
	//test_oit = true;
	bool is_shadow = _fncontainer.GetParamValue("_bool_IsShadow", false);

	if (strRendererSourceType.compare("VOLUME") == 0)
	{
		double dRuntime = 0;
		RenderVrDLS(&_fncontainer, g_pCGpuManager, &g_vmCommonParams, &g_LocalProgress, &dRuntime);
		g_dRunTimeVRs += dRuntime;
	}
	else if (strRendererSourceType.compare("MESH") == 0) // MESH
	{
		double dRuntime = 0;
		RenderSrOIT(&_fncontainer, g_pCGpuManager, &g_vmCommonParams, &g_LocalProgress, &dRuntime);
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

		*piGpuState = grd_helper::InitializePresettings(g_pCGpuManager, &g_vmCommonParams);

		int* piFeatureLevel = (int*)_fncontainer.ReadRmwBufferPtr("_out_int_FeatureLevelDX11", (int*)NULL);
		if(piFeatureLevel)
		{
			*piFeatureLevel = 
				g_vmCommonParams.dx11_featureLevel;
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

		grd_helper::InitializePresettings(g_pCGpuManager, &g_vmCommonParams);
		*ppCGpuManager = g_pCGpuManager;
	}

	//CVxCommander.VxNew().VxSetModuleCustomPointer("vismtv_inbuilt_renderergpudx", "Renderer Merger", "_outptr_function_RendererMerger", true);
	//CVxCommander.VxNew().VxSetModuleCustomPointer("vismtv_inbuilt_renderercpu", "Renderer Merger", "_inptr_function_RendererMerger", false);
	//void** ppvRendererMerger = (void**)_fncontainer.ReadRmwBufferPtr("_outptr_function_RendererMerger"));
	//if (ppvRendererMerger != NULL)
	//{
	//	*ppvRendererMerger = HDx11MixOut;
	//}

	if (g_vmCommonParams.dx11Device == NULL || g_vmCommonParams.dx11DeviceImmContext == NULL)
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

						grd_helper::UpdateVolumeModel(gres_main_vol, main_vol_obj, render_type == 5 /*__RM_SURFACECCF*/, &g_LocalProgress);

						g_LocalProgress.start = 0;
						g_LocalProgress.range = 100;
						g_LocalProgress.progress_ptr = &g_dProgress;

						grd_helper::UpdateVolumeModel(gres_mask_vol, mask_vol_obj, true, &g_LocalProgress);
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
				map<int, map<string, GpuRes>> mapGpuRes_Tex;
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
					RegisterVolumeRes(vol_obj, tobj, lobj, g_pCGpuManager, g_vmCommonParams.dx11DeviceImmContext, mapAssociatedObjects,
						mapGpuRes_VolumeAndTMap, &g_LocalProgress);

					g_LocalProgress.start = 0;
					g_LocalProgress.range = 100;
					g_LocalProgress.progress_ptr = &g_dProgress;
					GpuRes gres_vtx, gres_idx;
					map<string, GpuRes> map_gres_texs;
					grd_helper::UpdatePrimitiveModel(gres_vtx, gres_idx, map_gres_texs, prim_obj, &g_LocalProgress);
					if (gres_vtx.alloc_res_ptrs.size() > 0)
						mapGpuRes_Vtx.insert(pair<int, GpuRes>(prim_obj_id, gres_vtx));
					if (gres_idx.alloc_res_ptrs.size() > 0)
						mapGpuRes_Idx.insert(pair<int, GpuRes>(prim_obj_id, gres_idx));
					if (map_gres_texs.size() > 0)
						mapGpuRes_Tex.insert(pair<int, map<string, GpuRes>>(prim_obj_id, map_gres_texs));
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