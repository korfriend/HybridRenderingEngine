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
	std::cout << "Plugin: GPU DX11 Renderer using Core v(" << __VERSION << ")" << std::endl;

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

	VmIObject* iobj = _fncontainer.GetParamPtr<VmIObject>("_VmObject_RenderOut");
	if(iobj == NULL)
	{
		VMERRORMESSAGE("VisMotive Renderer needs at least one IObject as output!");
		return false;
	}
	if(iobj->GetCameraObject() == NULL)
	{
		VMERRORMESSAGE("VisMotive Renderer needs Camera Initializeation!");
		return false;
	}

#pragma region GPU/CPU Pre Setting
	//float sizeGpuResourceForVolume = _fncontainer.GetParam("_float_SizeGpuResourceForVolume", 80.0f);
	//// 100 means 50%
	//float resourceRatioForVolume = sizeGpuResourceForVolume * 0.5f * 0.01f;
	//uint uiDedicatedGpuMemoryKB = 
	//	(uint)(g_vmCommonParams.dx11_adapter.DedicatedVideoMemory / 1024);
	//float halfCriterionKB = (float)uiDedicatedGpuMemoryKB * resourceRatioForVolume;
	//float halfCriterionKB = _fncontainer.GetParam("_float_GpuVolumeMaxSizeKB", 256.f * 1024.f);
	// In CPU VR mode, Recommend to set dHalfCriterionKB = 16;
	//vector<VmObject*> vtrInputVolumes;
	//_fncontainer.GetVmObjectList(&vtrInputVolumes, VmObjKey(ObjectTypeVOLUME, true));
	//for (int i = 0; i < (int)vtrInputVolumes.size(); i++)
	//	vtrInputVolumes.at(i)->RegisterCustomParameter("_float_ForcedHalfCriterionKB", halfCriterionKB);
#pragma endregion

	string strRendererSource = _fncontainer.GetParam("_string_RenderingSourceType", string("MESH"));
	bool is_shadow = _fncontainer.GetParam("_bool_IsShadow", false);
	if (strRendererSource.compare("VOLUME") == 0)
	{
		double dRuntime = 0;
		RenderVrDLS(&_fncontainer, g_pCGpuManager, &g_vmCommonParams, &g_LocalProgress, &dRuntime);
		g_dRunTimeVRs += dRuntime;
	}
	else if (strRendererSource.compare("MESH") == 0) // MESH
	{
		double dRuntime = 0;
		RenderSrOIT(&_fncontainer, g_pCGpuManager, &g_vmCommonParams, &g_LocalProgress, &dRuntime);
		g_dRunTimeVRs += dRuntime;
	}
	else if (strRendererSource.compare("SECTIONAL_MESH") == 0)
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
	_fncontainer.SetParam("_string_CoreVersion", string(__VERSION));
}