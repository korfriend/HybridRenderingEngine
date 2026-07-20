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
#include <algorithm> // std::sort -- CollectViewLights (Multi-Light rev.14)
#include <vector>

using namespace Concurrency;	// for PPL

// ---- Multi-Light rev.14 (plan: secret_recipies/MULTI_LIGHT_PLAN.md, ML-D2) ----
// Lights are ACTORS: they ride VmFnContainer::sceneActors by pointer, already filtered by this
// view's hidden_actors + scene-level visible. There is no light payload channel any more (the old
// "_VmLight_LightSource" value copy and "_vector<VmSceneLight>*_SceneLights" are retired); core
// forwards only "_int_DominantLightId".
//
// DOMINANT = the single direct-shading light AND the only light allowed to honour CAMERA_ATTACHED
// (ML-D9). 0 / absent / not-found -> NULL, which every renderer already treats as "no light"
// (the legacy default path). An OLD core sets neither channel, so NULL is also the graceful
// degrade for a new renderer against an old core [user directive: accepted].
// The -1 default distinguishes "core did not send the channel at all" (an OLD core: every light would
// be silently dropped, which on a medical viewer is the quiet version of a crash) from the perfectly
// legal "new core, this view has no dominant" (0). A version mismatch cannot corrupt anything here --
// the light travels as a pointer in an id-keyed map and both sides re-check GetActorType() -- but it
// must not fail SILENTLY, so the old-core case logs once per process (api-stability review, rev.14).
inline fncontainer::VmLight* GetDominantLight(fncontainer::VmFnContainer* fnc)
{
	const int dominant_id = fnc->fnParams.GetParam("_int_DominantLightId", (int)-1);
	if (dominant_id < 0)
	{
		static bool warned_old_core = false;
		if (!warned_old_core)
		{
			warned_old_core = true;
			vzlog_error("[Multi-Light] core did not provide \"_int_DominantLightId\" - this renderer needs API v76+ (VmLight-as-actor). ALL LIGHTING IS DISABLED; rebuild/redeploy CommonApi + GpuManager + the renderer DLLs as one set.");
		}
		return NULL;
	}
	if (dominant_id == 0) return NULL; // new core, no dominant in this view (all hidden / none) -- legal, silent
	fncontainer::VmActor* actor = fnc->sceneActors.GetParam(dominant_id, (fncontainer::VmActor*)NULL);
	if (actor == NULL || actor->GetActorType() != "LIGHT") return NULL; // defensive: id/type must agree
	return (fncontainer::VmLight*)actor;
}

// The view's light set for VXGI (ML-D6 rev.14): every LIGHT actor in sceneActors -- i.e. THIS view's
// hidden/visible filter already applied (Q3's scene-global membership is retired; a light hidden in
// this view is out of this view's GI) -- sorted by actorId ASCENDING. The sort is the renderer's job:
// sceneActors is a VmMap (unordered_map), so its iteration order is not deterministic and the
// min-id cap / snapshot comparison would otherwise be nondeterministic (V13).
inline void CollectViewLights(fncontainer::VmFnContainer* fnc, std::vector<fncontainer::VmLight*>& lights_out)
{
	lights_out.clear();
	for (auto& actorPair : fnc->sceneActors)
	{
		fncontainer::VmActor* actor = std::get<1>(actorPair);
		if (actor == NULL || actor->GetActorType() != "LIGHT") continue;
		lights_out.push_back((fncontainer::VmLight*)actor);
	}
	std::sort(lights_out.begin(), lights_out.end(),
		[](const fncontainer::VmLight* a, const fncontainer::VmLight* b) { return a->actorId < b->actorId; });
}

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

// (v76) ComputeSSAO REMOVED -- SSAO feature retired (user directive). ComputeDOF keeps its signature;
// its apply_SSAO / is_blurred_SSAO arguments are now hard false at the only call site, so the AO GpuRes
// reference args are never dereferenced.
void ComputeDOF(__ID3D11DeviceContext* dx11DeviceImmContext,
	grd_helper::PSOManager* psoManager, VmIObject* iobj,
	int num_grid_x, int num_grid_y,
	GpuRes& gres_fb_counter, GpuRes& gres_fb_deep_k_buffer, GpuRes& gres_fb_rgba,
	bool apply_SSAO, bool is_blurred_SSAO, bool apply_fragmerge,
	GpuRes& gres_fb_vr_depth, GpuRes& gres_fb_vr_ao, GpuRes& gres_fb_vr_ao_blf,
	CB_CameraState& cbCamState, ID3D11Buffer* cbuf_cam_state, int __BLOCKSIZE,
	bool involve_vr);

void GradientMagnitudeAnalysis(vmfloat2& grad_minmax, VmVObjectVolume* vobj);

// X-ray slicer image-level post-filter modes (mirror vzm::CameraParameters::XRayPostFilter). Shared by
// the volume DVR (VolumeRenderer) and curved DVR (CurvedSlicerVR) x-ray post-filter paths (F10 dedup).
#define __XRPF_NONE 0
#define __XRPF_MEAN 1
#define __XRPF_GAUSSIAN 2
#define __XRPF_SHARPEN 3
#define __XRPF_SHARPEN_GAUSSIAN 4
#define __XRPF_LAPLACIAN 5
#define __XRPF_EDGE 6

// Fills weights[121] (row-major N*N, N=2*radius+1) with the X-ray post-filter convolution kernel for
// `mode`. Pure math; the volume and curved DVR paths carried a verbatim copy of this (F10). The caller
// supplies precomputed N/kcount/center and use_filter (0 = passthrough / mask stays zero), and owns the
// change-detection cache and the GPU mask upload. Brightness-preserving modes sum to 1; EDGE sums to 0.
void BuildXrayPostFilterKernel(int mode, float strength, int radius,
	int N, int kcount, int center, int use_filter, float weights[121]);

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
