#pragma once
#define _HAS_STD_BYTE 0

#include "vzm2/CommonInclude.h"
#include "vzm2/Backlog.h"
#include "VimCommon.h"
#include "gpures_helper.h"

#define PPL_USE

#include <process.h>
#include <ppl.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <windows.h>

using namespace Concurrency;	// for PPL

using namespace std;
using namespace vmmath;
using namespace vmobjects;
using namespace fncontainer;
using namespace vmgpuinterface;
using namespace grd_helper;

//using byte = uint8_t;

bool RenderSrSlicer(VmFnContainer* _fncontainer,
	VmGpuManager* gpu_manager,
	grd_helper::PSOManager* psoManager,
	LocalProgress* progress,
	double* run_time_ptr);

bool RenderVrCurvedSlicer(VmFnContainer* _fncontainer,
	VmGpuManager* gpu_manager,
	grd_helper::PSOManager* psoManager,
	LocalProgress* progress,
	double* run_time_ptr);

bool RenderPrimitives(VmFnContainer* _fncontainer,
	VmGpuManager* gpu_manager,
	grd_helper::PSOManager* psoManager,
	LocalProgress* progress,
	double* run_time_ptr);

bool RenderVrDLS(VmFnContainer* _fncontainer,
	VmGpuManager* gpu_manager,
	grd_helper::PSOManager* psoManager,
	LocalProgress* progress,
	double* run_time_ptr);

bool RenderVrDLS1(VmFnContainer* _fncontainer,
	VmGpuManager* gpu_manager,
	grd_helper::PSOManager* psoManager,
	LocalProgress* progress,
	double* run_time_ptr);

bool RenderVrDLS2(VmFnContainer* _fncontainer,
	VmGpuManager* gpu_manager,
	grd_helper::PSOManager* psoManager,
	LocalProgress* progress,
	double* run_time_ptr);

void ComputeSSAO(__ID3D11DeviceContext* dx11DeviceImmContext,
	grd_helper::PSOManager* psoManager, VmIObject* iobj,
	int num_grid_x, int num_grid_y,
	GpuRes& gres_fb_counter, GpuRes& gres_fb_deep_k_buffer, GpuRes& gres_fb_rgba, bool blur_SSAO,
	GpuRes& gres_fb_vr_depth, GpuRes& gres_fb_vr_ao, GpuRes& gres_fb_vr_ao_blf, bool involve_vr, bool apply_fragmerge);

void ComputeDOF(__ID3D11DeviceContext* dx11DeviceImmContext,
	grd_helper::PSOManager* psoManager, VmIObject* iobj,
	int num_grid_x, int num_grid_y,
	GpuRes& gres_fb_counter, GpuRes& gres_fb_deep_k_buffer, GpuRes& gres_fb_rgba,
	bool apply_SSAO, bool is_blurred_SSAO, bool apply_fragmerge,
	GpuRes& gres_fb_vr_depth, GpuRes& gres_fb_vr_ao, GpuRes& gres_fb_vr_ao_blf,
	CB_CameraState& cbCamState, ID3D11Buffer* cbuf_cam_state, int __BLOCKSIZE,
	bool involve_vr);

void GradientMagnitudeAnalysis(vmfloat2& grad_minmax, VmVObjectVolume* vobj);

//#define IS_SAFE_OBJ(OBJID) ((OBJID & 0xFFFF) >= 65536 - 4096)

enum MFR_MODE
{
	NONE = 0,
	STATIC_KB,
	DYNAMIC_FB,
	MOMENT,
	DYNAMIC_KB,	// deprecated
};

enum RENDER_GEOPASS {
	PASS_OPAQUESURFACES = 0, // no use MFR_MODE
	PASS_OIT = 1, // use MFR_MODE
	PASS_SILHOUETTE = 2, // no use MFR_MODE
	//PASS_SINGLELAYER = 3, // no use MFR_MODE
};
