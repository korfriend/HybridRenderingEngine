#pragma once
#include "VimCommon.h"
#include "gpures_helper.h"

#define PPL_USE

#include <process.h>
#include <ppl.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <cmath>
#include <windows.h>

using namespace Concurrency;	// for PPL

using namespace std;
using namespace vmmath;
using namespace vmobjects;
using namespace fncontainer;
using namespace vmgpuinterface;
using namespace grd_helper;

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

void ComputeSSAO(__ID3D11DeviceContext* dx11DeviceImmContext,
	grd_helper::GpuDX11CommonParameters* dx11CommonParams, VmIObject* iobj,
	int num_grid_x, int num_grid_y,
	GpuRes& gres_fb_counter, GpuRes& gres_fb_deep_k_buffer, GpuRes& gres_fb_rgba, bool blur_SSAO,
	GpuRes& gres_fb_vr_depth, GpuRes& gres_fb_vr_ao, GpuRes& gres_fb_vr_ao_blf, bool involve_vr, bool apply_fragmerge,
	map<string, vmint2>& profile_map, bool gpu_profile);

void ComputeDOF(__ID3D11DeviceContext* dx11DeviceImmContext,
	grd_helper::GpuDX11CommonParameters* dx11CommonParams, VmIObject* iobj,
	int num_grid_x, int num_grid_y,
	GpuRes& gres_fb_counter, GpuRes& gres_fb_deep_k_buffer, GpuRes& gres_fb_rgba,
	bool apply_SSAO, bool is_blurred_SSAO, bool apply_fragmerge,
	GpuRes& gres_fb_vr_depth, GpuRes& gres_fb_vr_ao, GpuRes& gres_fb_vr_ao_blf,
	CB_CameraState& cbCamState, ID3D11Buffer* cbuf_cam_state, int __BLOCKSIZE,
	bool involve_vr,
	map<string, vmint2>& profile_map, bool gpu_profile);

#define IS_SAFE_OBJ(OBJID) ((OBJID & 0xFFFF) >= 65536 - 4096)

enum MFR_MODE
{
	STATIC_KB = 0,
	DYNAMIC_FB,
	MOMENT,
	DYNAMIC_KB,	// deprecated
};

enum RENDER_GEOPASS {
	PASS_OPAQUESURFACES = 0,
	PASS_OIT = 1,
	PASS_SINGLELAYERS = 2,
};
