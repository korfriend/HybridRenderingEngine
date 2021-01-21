#pragma once
#include "VimCommon.h"
#include "gpures_helper.h"

#define PPL_USE

#include <process.h>
#include <ppl.h>
using namespace Concurrency;	// for PPL

using namespace std;
using namespace vmmath;
using namespace vmobjects;
using namespace fncontainer;
using namespace vmgpuinterface;

void RegisterVolumeRes(VmVObjectVolume* vol_obj, VmTObject* tobj, VmLObject* lobj, VmGpuManager* pCGpuManager, ID3D11DeviceContext* pdx11DeviceImmContext,
	map<int, VmObject*>& mapAssociatedObjects, map<int, GpuRes>& mapGpuRes, LocalProgress* progress);

bool RenderSrOIT(VmFnContainer* _fncontainer,
	VmGpuManager* gpu_manager,
	grd_helper::GpuDX11CommonParameters* dx11CommonParams,
	LocalProgress* progress,
	double* run_time_ptr);

bool RenderVrDLS(VmFnContainer* _fncontainer,
	VmGpuManager* gpu_manager,
	grd_helper::GpuDX11CommonParameters* dx11CommonParams,
	LocalProgress* progress,
	double* run_time_ptr);


#define IS_SAFE_OBJ(OBJID) ((OBJID & 0xFFFF) >= 65536 - 4096)

enum MFR_MODE
{
	STATIC_KB_FM = 0,
	DYNAMIC_FB,
	MOMENT,
	DYNAMIC_KB,
};
