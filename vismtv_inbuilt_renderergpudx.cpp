#include "vismtv_inbuilt_renderergpudx.h"
//#include "VXDX11Helper.h"
#include "vzm2/Backlog.h"

#include <d2d1_1.h>
#include <dwrite.h>
#include "RendererHeader.h"
#include "gpures_interface.h"

#include <iostream>

//GPU Direct3D Renderer - (c)DongJoon Kim
#define MODULEDEFINEDSPECIFIER "GPU Direct3D Renderer - (c)DongJoon Kim"
//#define RELEASE_MODE 

double g_dProgress = 0;
double g_dRunTimeVRs = 0;
grd_helper::PSOManager g_psoManager;
LocalProgress g_LocalProgress;

VmGpuManager* g_pCGpuManager = NULL;

ID2D1Factory1* g_pDirect2dFactory = NULL;
IDWriteFactory* g_pDWriteFactory = NULL;

struct D2DRes {
	IDXGISurface* pDxgiSurface = NULL;
	ID2D1RenderTarget* pRenderTarget = NULL;
	ID2D1SolidColorBrush* pSolidBrush = NULL;

	std::map<void*, ID3D11ShaderResourceView*> mapDevSharedSRVs;

	ID3D11Texture2D* pTex2DRT = NULL; // do not release this!

	// ---- Rotating shared PRESENT buffers (cross-device write-after-read hazard fix) ----
	// The finished frame in RENDER_OUT_RGBA_0 is copied into one of 3 rotating SHARED_PRESENT_RGBA_{0,1,2}
	// textures, and GetSharedShaderResView hands the app the SRV of the buffer just written. Because the
	// renderer cycles 0->1->2->0..., the buffer the app device is currently sampling is not overwritten for
	// 2 more frames -> the app's in-flight cross-device read cannot race the renderer's next write. These
	// app-device SRVs, and the present textures rotate every frame, so they MUST be cached separately from the
	// D2D render-target path (pTex2DRT / mapDevSharedSRVs), which stays pinned to RENDER_OUT_RGBA_0.
	// Key: (app-device ptr) within a per-present-buffer map. pPresentTex tracks the underlying present-texture
	// pointer per slot so a resize (which recreates it) invalidates that slot's cached SRVs.
	std::map<void*, ID3D11ShaderResourceView*> mapDevPresentSRVs[3];
	ID3D11Texture2D* pPresentTex[3] = { NULL, NULL, NULL }; // do not release these!
	int presentReadIdx = 0; // buffer index of the frame most recently finalized (set by the copy site)

	void ReleaseD2DRes() {
		VMSAFE_RELEASE(pSolidBrush);
		VMSAFE_RELEASE(pRenderTarget);
		VMSAFE_RELEASE(pDxgiSurface);

		for (auto& kv : mapDevSharedSRVs) {
			VMSAFE_RELEASE(kv.second);
		}
		mapDevSharedSRVs.clear();

		for (int i = 0; i < 3; i++) {
			for (auto& kv : mapDevPresentSRVs[i]) {
				VMSAFE_RELEASE(kv.second);
			}
			mapDevPresentSRVs[i].clear();
			pPresentTex[i] = NULL;
		}
	}
};
map<int, D2DRes> g_d2dResMap;
map<string, ID2D1StrokeStyle*> g_d2dStrokeStyleMap;
map<string, IDWriteTextFormat*> g_d2dTextFormatMap;

//std::atomic_int g_state = 0;
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

auto checkRefCount = [](IUnknown* obj, const char* name) {
	if (obj) {
		obj->AddRef();
		ULONG count = obj->Release();
		vzlog("%s refcount: %u", name, count);
	}
	};

static CRITICAL_SECTION cs;
static bool g_vimHandshakeArmed = false;
static bool g_moduleInitialized = false; // (idempotent DeInit) teardown-ownership marker: set once the first resource DeInit frees is acquired -> DeInit no-ops before that (pre-init), tears down owned stages after (partial/full)
bool __ArmVimCommonHandshake(const char* core_version, unsigned int core_sig)
{
	// (mutual handshake) core calls this with PRIMITIVE args before InitModule to prove a shared VimCommon
	// size; InitModule refuses un-armed, so an OLD core that never arms leaves this NEW DLL self-disabled.
	// LIFETIME (verification 17:29 Major 1): this is a DLL-LOAD-LIFETIME attestation, NOT per-Init. arm is called
	// once by ModuleArbiter::RegisterModule when it LOADS this DLL; the DLL then stays loaded across ClearModule
	// (DeInit, no FreeLibrary) / ExecuteModule (re-Init calls lpdllInit directly, no re-arm) cycles. That is
	// correct: the loaded DLL's VimCommon layout is immutable while loaded and the single core (one CommonApi/
	// ModuleArbiter per process) does not change, so g_vimHandshakeArmed==true always means "this loaded DLL
	// matched the core AT LOAD" and stays valid for the whole load. armed is intentionally NOT reset in DeInit.
	g_vimHandshakeArmed = (core_version != NULL && std::string(core_version) == __VERSION
		&& core_sig == fncontainer::VimCommonLayoutSig());
	return g_vimHandshakeArmed;
}

bool InitModule(fncontainer::VmFnContainer& _fncontainer)
{
	if (!g_vimHandshakeArmed) { vmlog::LogErr("GPU Renderer InitModule refused: VimCommon handshake not armed (stale/absent core)."); return false; }
	InitializeCriticalSection(&cs);
	g_moduleInitialized = true; // (idempotent DeInit guard) body is running past this point

	// Drop any hooks left from a prior Init/Deinit cycle, then register this
	// module's per-iobj caches (g_d2dResMap) for cleanup whenever
	// __ReleaseGpuResourcesBySrcID(iobjId) fires.
	ClearPerSrcIdReleaseHooks();
	RegisterPerSrcIdReleaseHook([](int iobjId) {
		auto it = g_d2dResMap.find(iobjId);
		if (it != g_d2dResMap.end()) {
			it->second.ReleaseD2DRes();
			g_d2dResMap.erase(it);
		}
	});

	if(g_pCGpuManager == NULL)
		g_pCGpuManager = new VmGpuManager(GpuSdkTypeDX11, __DLLNAME);

	if (grd_helper::Initialize(g_pCGpuManager, &g_psoManager) == -1)
	{
		vmlog::LogErr("failure new initializer!");
		DeInitModule(fncontainer::VmFnContainer());
		return false;
	}
	vmlog::LogInfo(string("Plugin: GPU DX11 Renderer using Core v(") + __VERSION + ")");
	
	// TEST //
	//return false;

	// Create a Direct2D factory.
	if (g_pDirect2dFactory == NULL) {

		checkRefCount(g_psoManager.dx11Device, "Before cleanup (dx11Device1)");

		if (D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_pDirect2dFactory) != S_OK)
			vmlog::LogErr("Failure D2D1CreateFactory!!");

		checkRefCount(g_psoManager.dx11Device, "Before cleanup (dx11Device2)");

		DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(g_pDWriteFactory), reinterpret_cast<IUnknown**>(&g_pDWriteFactory));

		checkRefCount(g_psoManager.dx11Device, "Before cleanup (dx11Device3)");

		ID2D1StrokeStyle* pStrokeStyle = NULL;
		HRESULT hr = g_pDirect2dFactory->CreateStrokeStyle(
			D2D1::StrokeStyleProperties(
				D2D1_CAP_STYLE_ROUND,
				D2D1_CAP_STYLE_ROUND,
				D2D1_CAP_STYLE_ROUND,
				D2D1_LINE_JOIN_ROUND,
				10.0f,
				D2D1_DASH_STYLE_SOLID,
				0.0f),
			NULL,
			0,
			&pStrokeStyle
		);
		g_d2dStrokeStyleMap["DEFAULT"] = pStrokeStyle;
		checkRefCount(g_psoManager.dx11Device, "Before cleanup (dx11Device4)");

		IDWriteTextFormat* pTextFormat = NULL;
		g_pDWriteFactory->CreateTextFormat(
			L"Comic Sans MS",            
			NULL,
			DWRITE_FONT_WEIGHT_NORMAL,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			50.f,
			L"",
			&pTextFormat
		);
		g_d2dTextFormatMap["DEFAULT"] = pTextFormat;
		checkRefCount(g_psoManager.dx11Device, "Before cleanup (dx11Device5)");

		if (hr != S_OK)
			vmlog::LogErr("D2D Device Setting Error!");
	}

	return true;
}

#ifdef THREAD_SAFE_CODE
static std::mutex queue_locker;
#endif 

// Van der Corput radical-inverse for the Halton low-discrepancy sequence (index >= 1); base 2 for X and
// base 3 for Y give well-distributed TAA sub-pixel jitter offsets.
static float TaaHalton(int index, int base)
{
	float f = 1.f, r = 0.f;
	while (index > 0) { f /= (float)base; r += f * (float)(index % base); index /= base; }
	return r;
}

bool DoModule(fncontainer::VmFnContainer& _fncontainer)
{
#ifdef THREAD_SAFE_CODE
	std::scoped_lock lock(queue_locker);
#endif 

#if defined(DX11_3)
	//vzlog("vismtv_inbuilt_renderergpudx DX11_3 start!");
#elif defined(DX11_0)
	//vzlog("vismtv_inbuilt_renderergpudx DX11_0 start!");
#else
	//vzlog("vismtv_inbuilt_renderergpudx DX10_0 start!");
#endif

	if(g_pCGpuManager == NULL)
	{
		return false;
	}

	if (g_psoManager.dx11Device == NULL || g_psoManager.dx11DeviceImmContext == NULL)
	{
		if (grd_helper::Initialize(g_pCGpuManager, &g_psoManager) == -1)
		{
			DeInitModule(fncontainer::VmFnContainer());
			return false;
		}
	}

#ifdef DX11_3
	if (g_psoManager.dx11_featureLevel < 0xb100) {
		_fncontainer.fnParams.SetParam("_bool_UseSpinLock", true);
	}
#else
	_fncontainer.fnParams.SetParam("_bool_UseSpinLock", true);
#endif

#ifdef DX10_0
	_fncontainer.fnParams.SetParam("_int_OitMode", (int)MFR_MODE::DYNAMIC_FB);
#endif

	g_LocalProgress.start = 0;
	g_LocalProgress.range = 100;
	g_LocalProgress.progress_ptr = &g_dProgress;

	fncontainer::VmCamera* _rcam = _fncontainer.fnParams.GetParam("_VmCamera*_RenderCamera", (fncontainer::VmCamera*)NULL);
	VmIObject* iobj = _rcam ? _rcam->iobj : NULL; // (increment 3) iobj derived from the render-from VmCamera
	if(iobj == NULL)
	{
		VMERRORMESSAGE("VisMotive Renderer needs at least one IObject as output!");
		return false;
	}
	if(_rcam == NULL) // (increment: lens absorption) cached lens == iobj->GetCameraObject()
	{
		VMERRORMESSAGE("VisMotive Renderer needs Camera Initializeation!");
		return false;
	}

	EnterCriticalSection(&cs);

#pragma region GPU/CPU Pre Setting
	//float sizeGpuResourceForVolume = _fncontainer.fnParams.GetParam("_float_SizeGpuResourceForVolume", 80.0f);
	//// 100 means 50%
	//float resourceRatioForVolume = sizeGpuResourceForVolume * 0.5f * 0.01f;
	//uint32_t uiDedicatedGpuMemoryKB = 
	//	(uint32_t)(g_vmCommonParams.dx11_adapter.DedicatedVideoMemory / 1024);
	//float halfCriterionKB = (float)uiDedicatedGpuMemoryKB * resourceRatioForVolume;
	//float halfCriterionKB = _fncontainer.fnParams.GetParam("_float_GpuVolumeMaxSizeKB", 256.f * 1024.f);
	// In CPU VR mode, Recommend to set dHalfCriterionKB = 16;
	//vector<VmObject*> vtrInputVolumes;
	//_fncontainer.GetVmObjectList(&vtrInputVolumes, VmObjKey(ObjectTypeVOLUME, true));
	//for (int i = 0; i < (int)vtrInputVolumes.size(); i++)
	//	vtrInputVolumes.at(i)->RegisterCustomParameter("_float_ForcedHalfCriterionKB", halfCriterionKB);
#pragma endregion

	float maxTextureResoulution = _fncontainer.fnParams.GetParam("_float_MaxTextureResoulution", 2048.f);
	float maxVolumeSizeKB = _fncontainer.fnParams.GetParam("_float_MaxVolumeSizeKB", 1024.f * 1024.f);
	grd_helper::SetUserCapacity(maxVolumeSizeKB, maxTextureResoulution);
	bool is_shadow = _fncontainer.fnParams.GetParam("_bool_IsShadow", false);
	bool curved_slicer = _fncontainer.fnParams.GetParam("_bool_IsNonlinear", false);
	string strRendererSource = _fncontainer.fnParams.GetParam("_string_RenderingSourceType", string("MESH"));
	bool is_sectional = strRendererSource == "SECTIONAL_MESH" || strRendererSource == "SECTIONAL_VOLUME";
	bool is_final_render_out = false;
	float planeThickness = _fncontainer.fnParams.GetParam("_float_PlaneThickness", -1.f);
	bool is_picking_routine = _fncontainer.fnParams.GetParam("_bool_IsPickingRoutine", false);
	bool is_first_renderer = _fncontainer.fnParams.GetParam("_bool_IsFirstRenderer", true);
	bool is_last_renderer = _fncontainer.fnParams.GetParam("_bool_IsFinalRenderer", true);

	// ---- On-demand CPU framebuffer store, WITHOUT rendering ----
	// "Bring this camera's CPU framebuffer up to date from the frame that is ALREADY on the GPU, and do nothing
	// else." The GPU->CPU copy-back is the RenderOut() lambda below, normally reachable only at the very end of a
	// render and gated there by _bool_SkipSysFBUpdate. That gate CANNOT be used to request a copy on demand:
	// skip_sys_fb_update is a real CameraParameters field, so setting it goes through SetCameraParams, bumps the
	// camera timeStamp and RESETS TAA accumulation -- destroying the very converged image a caller would be trying
	// to read back. Hence a plain fnParams string key: invisible to CameraParameters, and no VimCommon.h member
	// (which would bump __VERSION and force every plugin DLL to be rebuilt against the new handshake).
	//
	// Everything a render would do is skipped: no source dispatch, no TAA jitter/target bookkeeping, no 2nd-layer
	// blend, no TAA resolve, no tonemap, no D2D overlay, no shared-present rotation. The pixels handed to the CPU
	// are exactly the ones the last real render finalized into __PRESENT_RT_NAME.
	const bool force_store_fb = _fncontainer.fnParams.GetParam("_bool_ForceStoreRenderBuffer", false);

	// A force-store issues no render passes, so there is nothing to time -- and more importantly the
	// Begin(dx11qr_disjoint) below is drained by a block at the very END of DoModule, which this path returns
	// before reaching. Pinning the flag off keeps every GpuProfile() call on this path a no-op (GpuProfile()
	// early-outs on it) and leaves the disjoint query balanced.
	g_psoManager.gpu_profile = !force_store_fb && _fncontainer.fnParams.GetParam("_bool_GpuProfile", false);
	map<string, vmint2>& profile_map = g_psoManager.profile_map;
	if (g_psoManager.gpu_profile)
	{
		int gpu_profilecount = (int)profile_map.size();
		g_psoManager.dx11DeviceImmContext->Begin(g_psoManager.dx11qr_disjoint);
		//gpu_profilecount++;
	}

	if (!is_sectional && is_shadow) {
		// to do //
	}

	if (strRendererSource == "SECTIONAL_VOLUME" && !curved_slicer)
		strRendererSource = "VOLUME";
	
	bool is_vr = false;
	const bool gpu_query_synch = false;

	// ---- TAA accumulation state (renderer-owned algorithm; core only forwards config + scene stamp) ----
	// DoModule is invoked once per render source (MESH then VOLUME are separate calls sharing this iobj), so
	// the jitter and accumulation index live on the iobj: the FIRST source of the RENDER CALL computes them,
	// every source reads the same jitter (in grd_helper::SetCb_Camera), and the last source resolves + advances.
	//
	// WHY THE IOBJ AND NOT THE VmCamera (they are 1:1 — iobj->GetCameraObject() reaches the camera from here):
	//   * These values are produced AND consumed entirely inside this DLL; the core never reads them. VmCamera's
	//     temporal_render_count/global_render_count are the opposite — the core WRITES them and renderers only
	//     read. VmCamera is the core's forwarded-config surface, so a renderer-private accumulator does not
	//     belong in it.
	//   * VmCamera's layout is the cross-DLL ABI contract (3 byte-identical VimCommon.h mirrors + a sizeof
	//     fingerprint + a version bump). Paying that to carry state the core never touches is the wrong trade;
	//     SetObjParam is a string-keyed side table and costs no ABI.
	//   * _int_TaaAccum describes the RENDER TARGET's contents ("how many samples are already in the history"),
	//     not the camera's pose. If the RT is resized the history is gone and the count is meaningless — which
	//     is exactly why the framebuffer size is folded into the sig below. Storing it beside the buffer it
	//     describes keeps that invariant local.
	// NOTE is_first_renderer means "first DoModule within ONE vzm::RenderScene call" — NOT "first of a presented
	// frame". An app frame may issue several RenderScene calls for the same camera (e.g. an event callback that
	// re-renders it); each call is one full TAA step (sig check + jitter here, resolve + accum++ at the end).
	// That is correct as long as the scene is not mutated between the calls — any scene mutation moves the
	// scene stamp and legitimately resets the accumulation.
	// Reset when the scene content or the viewport changed. Picking is never jittered.
	//
	// Gated on the TAA config ALONE — do NOT engage it just because another temporal technique is running.
	// VXGI is temporal too, but its temporal state lives in a DIFFERENT ASSET, and that is the whole point:
	//   * TAA accumulates the IMAGE — a running mean of N sub-pixel-jittered samples, in the RT history.
	//   * VXGI accumulates the VOXEL GRID — the field refines by one propagate per rendered frame, and the
	//     grid IS the carrier of its cross-frame state.
	// So every VXGI frame is already a COMPLETE render of the current field: the newest frame is the best one.
	// Folding it into a TAA history would average the early UNCONVERGED field into the converged result —
	// smearing, not anti-aliasing. Two temporal families, two accumulators; they share only the RE-RENDER LOOP
	// (core's skip gate + CheckRenderConvergence keep the view drawing until EVERY technique reports done),
	// which is why VXGI already progresses without TAA. Pairing them is a quality choice, not a dependency.
	const bool taa_enabled = _fncontainer.fnParams.GetParam("_bool_TaaEnabled", false) && !is_picking_routine;
	int taa_max_samples = _fncontainer.fnParams.GetParam("_int_TaaMaxSamples", (int)1);
	if (taa_max_samples < 1) taa_max_samples = 1;
	if (force_store_fb)
	{
		// No render this call, so there is no jitter to choose -- and, critically, no _int_TaaTarget to write.
		// Core's CheckRenderConvergence reads that value back; the "TAA off" arm below writes 0 == "converged",
		// which would tell the app the view had finished accumulating when in fact nothing was rendered at all.
	}
	else if (taa_enabled)
	{
		if (is_first_renderer)
		{
			vmint2 fb_taa_now;
			iobj->GetFrameBufferInfo(&fb_taa_now);
			const uint64_t scene_stamp = (uint64_t)_fncontainer.fnParams.GetParam("_uint64_SceneStamp", (uint64_t)0);
			const uint64_t sig = scene_stamp ^ (((uint64_t)fb_taa_now.x << 20) ^ (uint64_t)fb_taa_now.y);
			int taa_accum = iobj->GetObjParam<int>("_int_TaaAccum", (int)0);
			if (sig != iobj->GetObjParam<uint64_t>("_uint64_TaaSig", (uint64_t)0))
			{
				taa_accum = 0;
				iobj->SetObjParam("_uint64_TaaSig", sig);
			}
			// Do NOT reset a converged accumulation: renders legitimately continue past TAA convergence
			// (e.g. while VXGI bounces are still propagating), and resetting here made the TAA counter
			// visibly cycle 0->32->0. Clamp instead — the resolve keeps blending new frames at 1/(N+1),
			// so post-convergence image changes (GI refinement) fade smoothly into the history.
			if (taa_accum > taa_max_samples) taa_accum = taa_max_samples;

			const float jx = TaaHalton(taa_accum + 1, 2) - 0.5f;
			const float jy = TaaHalton(taa_accum + 1, 3) - 0.5f;
			iobj->SetObjParam("_float2_TaaJitterPx", vmfloat2(jx, jy));
			iobj->SetObjParam("_int_TaaAccum", taa_accum);
			iobj->SetObjParam("_int_TaaTarget", taa_max_samples);
		}
	}
	else if (is_first_renderer && !is_picking_routine)
	{
		// TAA off: no jitter, and report "converged" via CheckRenderConvergence (target == 0). Picking leaves
		// the TAA iobj state untouched (the picking pass forces zero jitter on its own).
		iobj->SetObjParam("_float2_TaaJitterPx", vmfloat2(0.f, 0.f));
		iobj->SetObjParam("_int_TaaTarget", (int)0);
	}

	if (is_first_renderer && !is_picking_routine && gpu_query_synch)
	{
		g_psoManager.dx11DeviceImmContext->Begin(g_psoManager.dx11qr_disjoint);
		//g_vmCommonParams.dx11DeviceImmContext->End(g_vmCommonParams.dx11qr_timestamps[0]);
	}

	if (force_store_fb)
	{
		// THE render-skip. is_vr is normally set by the dispatch arms below; mirror it from the source type so
		// the DX10.0 build of RenderOut() -- the only configuration that reads it -- still selects the same
		// RENDER_OUT_* pair the last render wrote into. is_final_render_out stays false, so the whole
		// finalize-and-present block further down is skipped with it.
		is_vr = (strRendererSource == "VOLUME" || strRendererSource == "SECTIONAL_VOLUME");
	}
	else if (strRendererSource == "VOLUME")
	{
		double dRuntime = 0;
		uint32_t vrSlot = _fncontainer.fnParams.GetParam("DVR_CUSTOM_SLOT", 0u);
		switch (vrSlot)
		{
		case 1:
			//RenderVrDLS1(&_fncontainer, g_pCGpuManager, &g_vmCommonParams, &g_LocalProgress, &dRuntime);
			//break;
		case 2:
			//RenderVrDLS2(&_fncontainer, g_pCGpuManager, &g_vmCommonParams, &g_LocalProgress, &dRuntime);
			//break;
		default:
		case 0:
			RenderVrDLS(&_fncontainer, g_pCGpuManager, &g_psoManager, &g_LocalProgress, &dRuntime);
			break;
		}
		g_dRunTimeVRs += dRuntime;
		is_final_render_out = true;
		is_vr = true;
		//g_state = 1;
	}
	else if (strRendererSource == "MESH")
	{
		//g_state = 0;
		double dRuntime = 0;
		RenderPrimitives(&_fncontainer, g_pCGpuManager, &g_psoManager, &g_LocalProgress, &dRuntime);
		g_dRunTimeVRs += dRuntime;
		if (is_last_renderer) is_final_render_out = true;
	}
	else if (strRendererSource == "SECTIONAL_VOLUME")
	{
		assert(curved_slicer == true);
		double dRuntime = 0;
		RenderVrCurvedSlicer(&_fncontainer, g_pCGpuManager, &g_psoManager, &g_LocalProgress, &dRuntime);
		g_dRunTimeVRs += dRuntime;
		is_final_render_out = true;
		is_vr = true;
	}
	else if (strRendererSource == "SECTIONAL_MESH")
	{
		double dRuntime = 0;
		RenderSrSlicer(&_fncontainer, g_pCGpuManager, &g_psoManager, &g_LocalProgress, &dRuntime);
		g_dRunTimeVRs += dRuntime;
		if (is_last_renderer || planeThickness <= 0.f) is_final_render_out = true;
	}

	auto RenderOut = [&iobj, &is_last_renderer, &planeThickness, &_fncontainer, &is_vr, &_rcam]() {

		g_psoManager.GpuProfile("Copyback");

		VmGpuManager* gpu_manager = g_pCGpuManager;
		__ID3D11Device* dx11Device = g_psoManager.dx11Device;
		__ID3D11DeviceContext* dx11DeviceImmContext = g_psoManager.dx11DeviceImmContext;

		HWND hWnd = (HWND)_fncontainer.fnParams.GetParam("_hwnd_WindowHandle", (HWND)NULL);
		bool is_rgba = _fncontainer.fnParams.GetParam("_bool_IsRGBA", false);

		vmint2 fb_size_cur;
		iobj->GetFrameBufferInfo(&fb_size_cur);
		int count_call_render = iobj->GetObjParam("_int_NumCallRenders", 0);

		GpuRes gres_fb_rgba, gres_fb_depthcs;
		GpuRes gres_fb_sys_rgba, gres_fb_sys_depthcs;
#ifdef DX10_0
		grd_helper::UpdateFrameBuffer(gres_fb_rgba, iobj, is_vr? "RENDER_OUT_RGBA_1" : "RENDER_OUT_RGBA_0", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, __COLOR_RT_FORMAT, 0);
		grd_helper::UpdateFrameBuffer(gres_fb_depthcs, iobj, is_vr? "RENDER_OUT_DEPTH_1" : "RENDER_OUT_DEPTH_0", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, DXGI_FORMAT_R32_FLOAT, 0);
#else
		// The LDR target, not RENDER_OUT_RGBA_0: this lambda feeds the HWND swapchain copy and the CPU readback,
		// and both must see the tonemapped image. It is also a correctness requirement, not just a semantic one --
		// SYSTEM_OUT_RGBA is R8G8B8A8_UNORM and CopyResource demands matching formats, so copying from the FP16
		// scene target would simply fail.
		grd_helper::UpdateFrameBuffer(gres_fb_rgba, iobj, __PRESENT_RT_NAME, RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
		grd_helper::UpdateFrameBuffer(gres_fb_depthcs, iobj, "RENDER_OUT_DEPTH_0", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_FLOAT, 0);
#endif
		grd_helper::UpdateFrameBuffer(gres_fb_sys_rgba, iobj, "SYSTEM_OUT_RGBA", RTYPE_TEXTURE2D, NULL, DXGI_FORMAT_R8G8B8A8_UNORM, UPFB_SYSOUT);
		grd_helper::UpdateFrameBuffer(gres_fb_sys_depthcs, iobj, "SYSTEM_OUT_DEPTH", RTYPE_TEXTURE2D, NULL, DXGI_FORMAT_R32_FLOAT, UPFB_SYSOUT);

		if (hWnd && is_last_renderer)
		{
			// APPLY HWND MODE
			ID3D11Texture2D* pTex2dHwndRT = NULL;
			ID3D11RenderTargetView* pHwndRTV = NULL;
			gpu_manager->UpdateDXGI((void**)&pTex2dHwndRT, (void**)&pHwndRTV, hWnd, fb_size_cur.x, fb_size_cur.y);

			dx11DeviceImmContext->CopyResource(pTex2dHwndRT, (ID3D11Texture2D*)gres_fb_rgba.alloc_res_ptrs[DTYPE_RES]);
		}
		else {
			if (hWnd) gpu_manager->ReleaseDXGI(hWnd);

			int outIndex = planeThickness == 0.f && !is_last_renderer ? 1 : 0; // just for SlicerSR combined with CPU MPR
			FrameBuffer *fb_rout, *fb_dout;
			if (_fncontainer.fnParams.GetParam("STORE_RT_IOBJ", false))
			{
				if (!iobj->ReplaceFrameBuffer(FrameBufferUsageRENDEROUT, 1, data_type::dtype<vmbyte4>(), ("SR temp RGBA : defined in vismtv_inbuilt_renderercpu module")))
					iobj->InsertFrameBuffer(data_type::dtype<vmbyte4>(), FrameBufferUsageRENDEROUT, ("SR temp RGBA : defined in vismtv_inbuilt_renderercpu module"));
				if (!iobj->ReplaceFrameBuffer(FrameBufferUsageDEPTH, 1, data_type::dtype<float>(), ("SR temp depth : defined in vismtv_inbuilt_renderercpu module")))
					iobj->InsertFrameBuffer(data_type::dtype<float>(), FrameBufferUsageDEPTH, ("SR temp depth : defined in vismtv_inbuilt_renderercpu module"));

				fb_rout = (FrameBuffer*)iobj->GetFrameBuffer(FrameBufferUsageRENDEROUT, 1);
				fb_dout = (FrameBuffer*)iobj->GetFrameBuffer(FrameBufferUsageDEPTH, 1);
			}
			else
			{
				fb_rout = (FrameBuffer*)iobj->GetFrameBuffer(FrameBufferUsageRENDEROUT, outIndex);
				fb_dout = (FrameBuffer*)iobj->GetFrameBuffer(FrameBufferUsageDEPTH, 0);
			}

			if (count_call_render == 0)	// this means that there is no valid rendering pass
			{
				vmbyte4* rgba_buf = (vmbyte4*)fb_rout->fbuffer;
				float* depth_buf = (float*)fb_dout->fbuffer;

				memset(rgba_buf, 0, fb_size_cur.x * fb_size_cur.y * sizeof(vmbyte4));
				memset(depth_buf, 0x77, fb_size_cur.x * fb_size_cur.y * sizeof(float));
			}
			else
			{
#pragma region // Copy GPU to CPU
				dx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gres_fb_sys_rgba.alloc_res_ptrs[DTYPE_RES],
					(ID3D11Texture2D*)gres_fb_rgba.alloc_res_ptrs[DTYPE_RES]);
				dx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gres_fb_sys_depthcs.alloc_res_ptrs[DTYPE_RES],
					(ID3D11Texture2D*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_RES]);

				vmbyte4* rgba_sys_buf = (vmbyte4*)fb_rout->fbuffer;
				float* depth_sys_buf = (float*)fb_dout->fbuffer;
				
				D3D11_MAPPED_SUBRESOURCE mappedResSysRGBA;
				D3D11_MAPPED_SUBRESOURCE mappedResSysDepth;
				HRESULT hr = dx11DeviceImmContext->Map((ID3D11Texture2D*)gres_fb_sys_rgba.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, NULL, &mappedResSysRGBA);
				hr |= dx11DeviceImmContext->Map((ID3D11Texture2D*)gres_fb_sys_depthcs.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, NULL, &mappedResSysDepth);

				vmbyte4* rgba_gpu_buf = (vmbyte4*)mappedResSysRGBA.pData;
				float* depth_gpu_buf = (float*)mappedResSysDepth.pData;
				if (rgba_gpu_buf == NULL || depth_gpu_buf == NULL)
				{
#ifdef __DX_DEBUG_QUERY
					g_psoManager.debug_info_queue->PushEmptyStorageFilter();

					UINT64 message_count = g_psoManager.debug_info_queue->GetNumStoredMessages();

					for (UINT64 i = 0; i < message_count; i++) {
						SIZE_T message_size = 0;
						g_psoManager.debug_info_queue->GetMessage(i, nullptr, &message_size); //get the size of the message

						D3D11_MESSAGE* message = (D3D11_MESSAGE*)malloc(message_size); //allocate enough space
						g_psoManager.debug_info_queue->GetMessage(i, message, &message_size); //get the actual message

						//do whatever you want to do with it
						printf("Directx11: %.*s", message->DescriptionByteLength, message->pDescription);

						free(message);
					}
					g_psoManager.debug_info_queue->ClearStoredMessages();
#endif
					cout << ("VR ERROR -- OUT") << endl;
					cout << ("screen : " + to_string(fb_size_cur.x) + " x " + to_string(fb_size_cur.y)) << endl;
					//cout << ("v_thickness : " + to_string(v_thickness)) <<endl;
					//cout << ("k_value : " + to_string(k_value)) <<endl;
					//cout << ("grid width and height : " + to_string(num_grid_x) + " x " + to_string(num_grid_y))<<endl;
		}


				int buf_row_pitch = mappedResSysRGBA.RowPitch / 4;
				//is_rgba = is_rgba | !is_final_renderer;
#ifdef PPL_USE
				int count = fb_size_cur.y;
				parallel_for(int(0), count, [&](int i) // is_rgba, fb_size_cur, rgba_sys_buf, depth_sys_buf, rgba_gpu_buf, depth_gpu_buf, buf_row_pitch
#else
#pragma omp parallel for 
				for (int i = 0; i < fb_size_cur.y; i++)
#endif
				{
					for (int j = 0; j < fb_size_cur.x; j++)
					{
						vmbyte4 rgba = rgba_gpu_buf[j + i * buf_row_pitch];

						// BGRA
						if (is_rgba)
							rgba_sys_buf[i * fb_size_cur.x + j] = vmbyte4(rgba.x, rgba.y, rgba.z, rgba.w);
						else
							rgba_sys_buf[i * fb_size_cur.x + j] = vmbyte4(rgba.z, rgba.y, rgba.x, rgba.w);

						int iAddr = i * fb_size_cur.x + j;
						if (rgba.w > 0)
							depth_sys_buf[iAddr] = depth_gpu_buf[j + i * buf_row_pitch];
						else
							depth_sys_buf[iAddr] = FLT_MAX;
					}
#ifdef PPL_USE
				});
#else
			}
#endif
				dx11DeviceImmContext->Unmap((ID3D11Texture2D*)gres_fb_sys_rgba.alloc_res_ptrs[DTYPE_RES], 0);
				dx11DeviceImmContext->Unmap((ID3D11Texture2D*)gres_fb_sys_depthcs.alloc_res_ptrs[DTYPE_RES], 0);
			}
		}

		g_psoManager.GpuProfile("Copyback", true);
	};

#if __HDR_PIPELINE
	// ---- Tonemap: linear HDR -> display range ----
	// The renderers composite into an FP16 linear target; this is the single point where the frame commits to
	// display range.
	//
	// Why AFTER the TAA resolve: TAA takes an unbiased running mean of N jittered samples, i.e. it averages
	// RADIANCE. Since mean(tonemap(L)) != tonemap(mean(L)), curving each sample first would bake the operator
	// into the mean and bias the converged image. Keeping the curve out of the history also means an exposure
	// change costs nothing -- the HDR mean is still valid, only its presentation changed.
	//
	// Why BEFORE the D2D overlay: text and annotation lines must not go through the curve. That is also forced,
	// since D2D cannot draw into an FP16 surface with its R8G8B8A8_UNORM pixel format.
	//
	// Defaults are an exact no-op: TM_CLIP + exposure 1 + encode none == saturate(hdr) == today's image. Dev
	// channel until the look settles; promoting them means CameraParameters::script_params, not new struct
	// fields (ABI).
	grd_helper::TonemapParams tmParams;
	{
		// The VXGI debug views (_int_VxgiDebug 1..7 -> the screen-gather visualisation) are meant to be read as
		// data, not looked at as a picture: they show cone coverage, occlusion, raw radiance. A tone curve or an
		// exposure gain would rescale exactly the quantity being inspected and make the readout lie. Pinned to
		// identity, same as the DX11.0 path below.
		const bool vxgi_debug_view = _fncontainer.fnParams.GetParam("_int_VxgiDebug", (int)0) != 0;

#if __TONEMAP_ENABLED
		if (!vxgi_debug_view)
		{
		tmParams.tm_operator = (uint32_t)max(0, _rcam ? _rcam->GetParam("TONEMAP_OPERATOR", (int)0) : 0); // (1.70) tonemap from VmActor::_vmparams
		tmParams.encode = (uint32_t)max(0, _rcam ? _rcam->GetParam("TONEMAP_ENCODE", (int)0) : 0);
		tmParams.exposure = max(0.f, _rcam ? _rcam->GetParam("TONEMAP_EXPOSURE", 1.f) : 1.f);
		tmParams.knee = std::clamp(_rcam ? _rcam->GetParam("TONEMAP_KNEE", 1.f) : 1.f, 0.f, 1.f);
		tmParams.white_point = max(1e-3f, _rcam ? _rcam->GetParam("TONEMAP_WHITE_POINT", 4.f) : 4.f);
		}
#else
		// DX11.0: the tonemapper is a DX11.3 feature, so the knobs are ignored and the pass is pinned to its
		// identity (TonemapParams' defaults: clip + unit exposure + no encode). Its output is then saturate(hdr)
		// -- bit-for-bit the un-tonemapped image. The pass still runs because this configuration shares the
		// cs_5_0 blobs with DX11.3 and therefore still composites into FP16 targets; something has to bring the
		// frame down to the RGBA8 target that D2D and the present copy require.
		(void)vxgi_debug_view;
#endif
		// Not a look control in either configuration -- a NaN/Inf and firefly guard, and the bound TaaResolve
		// sanitizes against. A tighter value would silently cap legitimate HDR highlights before the operator
		// ever saw them.
		tmParams.radiance_ceiling = max(1.f, _fncontainer.fnParams.GetParam("_float_RadianceCeiling", 64.f));
	}

	// Reads RENDER_OUT_RGBA_0 (FP16) and writes the LDR target that D2D, the shared present, the HWND copy and
	// the CPU readback all consume. The input is bound as an SRV rather than a UAV: SRV loads are guaranteed for
	// every format, so this pass does not widen the engine's dependency on typed-UAV-load support.
	auto DispatchTonemap = [&tmParams, &iobj, &_fncontainer]() {
		const uint32_t tmbind = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		GpuRes gres_hdr, gres_ldr;
		grd_helper::UpdateFrameBuffer(gres_hdr, iobj, "RENDER_OUT_RGBA_0", RTYPE_TEXTURE2D, tmbind, __COLOR_RT_FORMAT, 0);
		grd_helper::UpdateFrameBuffer(gres_ldr, iobj, __PRESENT_RT_NAME, RTYPE_TEXTURE2D, tmbind, DXGI_FORMAT_R8G8B8A8_UNORM, 0);

		vmint2 fb_tm;
		iobj->GetFrameBufferInfo(&fb_tm);

		__ID3D11DeviceContext* ctx = g_psoManager.dx11DeviceImmContext;

		CB_CameraState cbTm;
		cbTm.rt_width = fb_tm.x;
		cbTm.rt_height = fb_tm.y;
		tmParams.ApplyTo(cbTm);

		ID3D11Buffer* cbuf_cam = g_psoManager.get_cbuf("CB_CameraState");
		D3D11_MAPPED_SUBRESOURCE mappedTm;
		ctx->Map(cbuf_cam, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedTm);
		memcpy(mappedTm.pData, &cbTm, sizeof(CB_CameraState));
		ctx->Unmap(cbuf_cam, 0);
		ctx->CSSetConstantBuffers(0, 1, &cbuf_cam);

		ID3D11ShaderResourceView* tmSRV = (ID3D11ShaderResourceView*)gres_hdr.alloc_res_ptrs[DTYPE_SRV];
		ID3D11UnorderedAccessView* tmUAV = (ID3D11UnorderedAccessView*)gres_ldr.alloc_res_ptrs[DTYPE_UAV];
		ctx->CSSetShaderResources(0, 1, &tmSRV);
		ctx->CSSetUnorderedAccessViews(0, 1, &tmUAV, NULL);
		ctx->CSSetShader(g_psoManager.get_cshader("CS_Tonemap_cs_5_0"), NULL, 0);

		int __BLOCKSIZE_TM = _fncontainer.fnParams.GetParam("_int_GpuThreadBlockSize", (int)4);
		ctx->Dispatch((uint32_t)ceil(fb_tm.x / (float)__BLOCKSIZE_TM), (uint32_t)ceil(fb_tm.y / (float)__BLOCKSIZE_TM), 1);

		ID3D11ShaderResourceView* srvNull = NULL;
		ID3D11UnorderedAccessView* uavNull = NULL;
		ctx->CSSetShaderResources(0, 1, &srvNull);
		ctx->CSSetUnorderedAccessViews(0, 1, &uavNull, NULL);
	};
#endif

	// ---- On-demand CPU framebuffer store (_bool_ForceStoreRenderBuffer -- see the flag's declaration above) ----
	// This is the earliest point at which the branch can live: RenderOut() is fully constructed here and every
	// value it captures (iobj, _fncontainer, is_last_renderer, planeThickness, is_vr, _rcam) is already final --
	// none of them is produced BY a render pass, so nothing about the lambda requires one to have run. Everything
	// it consumes that a render DOES produce -- the contents of __PRESENT_RT_NAME / RENDER_OUT_DEPTH_0 and
	// "_int_NumCallRenders" -- persists on the GPU resource / the iobj across DoModule calls, which is exactly why
	// a copy-back without a render is meaningful at all. DispatchTonemap() is in scope here too (see below for why
	// this path deliberately does not call it).
	//
	// Deliberately a standalone block ahead of the chain rather than an arm of it: the is_final_render_out block
	// and the STORE_RT_IOBJ hand-off below stay byte-for-byte unchanged, and STORE_RT_IOBJ in particular must keep
	// its own behaviour (it renders first, then stores into the index-1 buffers for the CPU-DVR module).
	if (force_store_fb)
	{
		bool stored = false;

		vmint2 fb_size_now;
		iobj->GetFrameBufferInfo(&fb_size_now);
		const vmint2 fb_size_gpu = iobj->GetObjParam("_int2_PreviousScreenSize", vmint2(0, 0));

		// NULL-CHECK -- and note carefully what it does and does NOT establish.
		//
		// The CPU framebuffer ALLOCATION lives inside the render path (SlicerSR.cpp's "IOBJECT CPU" region
		// and its twins in PrimitiveRenderer / VolumeRenderer / CurvedSlicerVR) and is NOT gated on
		// _bool_SkipSysFBUpdate. So a non-NULL RENDEROUT slot 0 separates these:
		//     never entered a render pass -> GetFrameBuffer(FrameBufferUsageRENDEROUT, 0) == NULL
		//     rendered, skip == false      -> allocated, contents current
		//     rendered, skip == true       -> allocated, contents STALE  <- the case this path exists to fix
		//
		// THIS COMMENT USED TO CLAIM THIS WAS A COMPLETE LIVENESS TEST -- "decided by the CPU framebuffer
		// being NULL and by nothing else". THAT WAS FALSE, and the false rationale mattered more than the
		// code: a PICKING pass also enters the allocation (PrimitiveRenderer.cpp's "IOBJECT CPU" region has
		// no enclosing conditional) while every RenderOut() call site is unreachable while picking (:821 is
		// inside `is_final_render_out && !is_picking_routine`). So a picked-but-never-rendered camera has a
		// non-NULL slot 0 holding INDETERMINATE HEAP -- vmbyte4 is glm::u8vec4 and GLM_FORCE_CTOR_INIT is
		// not defined anywhere in this workspace. Copying that out would hand a caller uninitialised memory
		// as a picture.
		//
		// The real liveness gate is therefore NOT here. CommonApi's StoreRenderBuffer requires a RECORDED
		// FINAL RENDER PASS before it ever dispatches this request, which picking cannot produce because it
		// does not go through RenderScene. What remains here is a null-deref guard for the pointers
		// RenderOut() is about to touch. Do not restore the old wording: it is exactly the reasoning that
		// would let someone delete CommonApi's gate as redundant.
		//
		// The two companion buffers below are NOT a second liveness test: RenderOut() dereferences whichever
		// RENDEROUT index it selects and DEPTH slot 0 unconditionally, so these guard the exact pointers it is
		// about to touch. On a never-rendered camera all three are NULL together, so they never widen the
		// failure set -- they only stop a null deref in the (slicer-only) index-1 selection.
		const int store_out_idx = (planeThickness == 0.f && !is_last_renderer) ? 1 : 0;
		const bool has_cpu_fb = iobj->GetFrameBuffer(FrameBufferUsageRENDEROUT, 0) != NULL
			&& iobj->GetFrameBuffer(FrameBufferUsageRENDEROUT, store_out_idx) != NULL
			&& iobj->GetFrameBuffer(FrameBufferUsageDEPTH, 0) != NULL;

		// NOTE "_int_NumCallRenders == 0" is deliberately NOT a failure here. It is a per-render pass counter
		// (each renderer zeroes a local, counts the actors it drew, and stores it), so 0 means the last render
		// legitimately drew nothing -- an empty scene, not a missing one. RenderOut() already has an arm for
		// exactly that and zero-fills, which is bit-for-bit what a real render of that scene produced. Treating
		// it as "not live" would refuse to store a frame the caller is entitled to.
		if (!has_cpu_fb)
		{
			// Never rendered: no CPU framebuffer exists yet, so there is nothing to store and nothing safe to
			// store into. The only fail-return of this path.
			vmlog::LogWarn("ForceStoreRenderBuffer: no rendered frame to store! ("
				+ std::to_string(iobj->GetObjectID()) + ")");
		}
		else if (fb_size_now.x != fb_size_gpu.x || fb_size_now.y != fb_size_gpu.y)
		{
			// NOT a liveness signal -- this camera HAS rendered; the buffers just no longer agree on size, and
			// the copy is not memory-safe until they do. The framebuffer was resized since that render:
			// VmIObject::ResizeFrameBuffer reallocated the CPU buffers immediately (contents undefined), but
			// grd_helper::UpdateFrameBuffer early-returns on a cache hit keyed by {src_id, res_name} WITHOUT
			// ever comparing size (gpures_helper.cpp: "if (g_pCGpuManager->UpdateGpuResource(gres)) return
			// true;"), so the GPU targets and the SYSTEM_OUT_* staging surfaces are all still at the OLD
			// dimensions. RenderOut()'s copy loop is bounded by the NEW GetFrameBufferInfo size, so it would
			// index the mapped staging surface out of bounds. Only a real render notices the change
			// (_int2_PreviousScreenSize -> ReleaseGpuResourcesBySrcID) and rebuilds them, and re-rendering is
			// exactly what this path must not do -- so refuse instead of reading past the mapping.
			vmlog::LogWarn("ForceStoreRenderBuffer: framebuffer resized since the last render! ("
				+ std::to_string(iobj->GetObjectID()) + ")");
		}
		else
		{
			// NO DispatchTonemap() here, on purpose. __PRESENT_RT_NAME already holds the FINALIZED frame: the
			// tonemap ran at the end of the last render AND the D2D overlay was drawn on top of it afterwards.
			// Re-running the tonemap would rewrite that target from the FP16 scene buffer and silently drop the
			// overlay from the copy-back, so the CPU image would no longer match what was presented. Idempotent
			// on the pixels, destructive on the overlay -- so: skip it. (The STORE_RT_IOBJ path below must call it
			// itself precisely because it bypasses the block that tonemaps.)
			//
			// Steer RenderOut() to its readback arm. With a window bound it takes the HWND arm instead, which
			// CopyResources into the swapchain and never touches the CPU buffers at all -- the opposite of what
			// was asked for. Clearing the key rather than forcing is_last_renderer=false is what keeps
			// ReleaseDXGI() off a live window and leaves the output index alone. _fncontainer is the core's
			// per-call copy (VmFnContainer vfn_sub = vfn), and this function already mutates it elsewhere, so the
			// write cannot leak into another call.
			_fncontainer.fnParams.SetParam("_hwnd_WindowHandle", (HWND)NULL);
			RenderOut();
			stored = true;
		}

		// Out-param so a caller can tell "nothing to store" apart from a module failure without reading the log.
		_fncontainer.fnParams.SetParam("_bool_StoredRenderBuffer", stored);

		g_dProgress = 100;
		LeaveCriticalSection(&cs);
		return stored;
	}

	if (is_final_render_out && !is_picking_routine)
	{
		const uint32_t rtbind = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		GpuRes gres_fb_rgba;
		grd_helper::UpdateFrameBuffer(gres_fb_rgba, iobj, "RENDER_OUT_RGBA_0", RTYPE_TEXTURE2D, rtbind, __COLOR_RT_FORMAT, 0);

#ifndef DX10_0
		// here, compose the 2nd layer rendering target texture onto the final render out texture... 
		auto Blend2ndLayer = [&gres_fb_rgba, &iobj, &_fncontainer, &rtbind]() {

			vmint2 fb_size_cur;
			iobj->GetFrameBufferInfo(&fb_size_cur);

			GpuRes gres_fb_depthcs;
			grd_helper::UpdateFrameBuffer(gres_fb_depthcs, iobj, "RENDER_OUT_DEPTH_0", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R32_FLOAT, 0);

			GpuRes gres_fb_2ndlayer_rgba, gres_fb_2ndlayer_depthcs;
			grd_helper::UpdateFrameBuffer(gres_fb_2ndlayer_rgba, iobj, "RENDER_OUT_RGBA_1", RTYPE_TEXTURE2D, rtbind, __COLOR_RT_FORMAT, 0);
			// R32_FLOAT, not RGBA8: this is a depth buffer. PrimitiveRenderer.cpp creates the same resource as
			// R32_FLOAT and wins the race (UpdateFrameBuffer caches on {src_id, res_name} and early-returns
			// without ever comparing the requested format), so the wrong constant here was inert -- but it would
			// have decided the format outright had the call order ever changed.
			grd_helper::UpdateFrameBuffer(gres_fb_2ndlayer_depthcs, iobj, "RENDER_OUT_DEPTH_2", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R32_FLOAT, 0);

			ID3D11UnorderedAccessView* dx11UAVs[4] = {
					(ID3D11UnorderedAccessView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_UAV]
					, (ID3D11UnorderedAccessView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_UAV]
					, (ID3D11UnorderedAccessView*)gres_fb_2ndlayer_rgba.alloc_res_ptrs[DTYPE_UAV]
					, (ID3D11UnorderedAccessView*)gres_fb_2ndlayer_depthcs.alloc_res_ptrs[DTYPE_UAV]
			};

			__ID3D11Device* dx11Device = g_psoManager.dx11Device;
			__ID3D11DeviceContext* dx11DeviceImmContext = g_psoManager.dx11DeviceImmContext;

			int dotSize = _fncontainer.fnParams.GetParam("_int_2ndLayerPatternInterval", (int)3);
			float blendingW = _fncontainer.fnParams.GetParam("_float_2ndLayerBlendingW", 0.2f);

			ID3D11Buffer* cbuf_cam_state = g_psoManager.get_cbuf("CB_CameraState");
			D3D11_MAPPED_SUBRESOURCE mappedResCamState;
			dx11DeviceImmContext->Map(cbuf_cam_state, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResCamState);
			CB_CameraState* cbCamStateData = (CB_CameraState*)mappedResCamState.pData;
			cbCamStateData->rt_width = fb_size_cur.x;
			cbCamStateData->rt_height = fb_size_cur.y;
			cbCamStateData->iSrCamDummy__0 = dotSize | ((int)(blendingW * 100) << 8);
			dx11DeviceImmContext->Unmap(cbuf_cam_state, 0);
			dx11DeviceImmContext->CSSetConstantBuffers(0, 1, &cbuf_cam_state);

			ID3D11ComputeShader* dx11CShader = g_psoManager.get_cshader("CS_Blend2ndLayer_cs_5_0");
			dx11DeviceImmContext->CSSetShader(dx11CShader, NULL, 0);
			dx11DeviceImmContext->CSSetUnorderedAccessViews(1, 4, dx11UAVs, (UINT*)(&dx11UAVs));

			int __BLOCKSIZE = _fncontainer.fnParams.GetParam("_int_GpuThreadBlockSize", (int)4);
			uint32_t num_grid_x = (uint32_t)ceil(fb_size_cur.x / (float)__BLOCKSIZE);
			uint32_t num_grid_y = (uint32_t)ceil(fb_size_cur.y / (float)__BLOCKSIZE);
			dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);

			// Set NULL States //
			ID3D11UnorderedAccessView* dx11UAVs_NULL[7] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };
			dx11DeviceImmContext->CSSetUnorderedAccessViews(1, 4, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
		};

		if (iobj->GetObjParam("NUM_SECOND_LAYER_OBJECTS", (int)0) > 0) {
			Blend2ndLayer();
		}

		// ---- TAA resolve ----
		// Fold the composited scene (RENDER_OUT_RGBA_0) into a persistent per-camera history buffer and write
		// the running average back for presentation. Runs once per RenderScene (final render source only). The
		// engine (RenderScene) owns the accumulation index: it writes _int_TaaAccum before this call and
		// advances it afterwards; _int_TaaTarget > 0 marks TAA active. D2D text draws afterwards so overlays
		// stay crisp on top of the resolved image.
		if (is_last_renderer && iobj->GetObjParam<int>("_int_TaaTarget", (int)0) > 0)
		{
			__ID3D11DeviceContext* dx11ImmCtx = g_psoManager.dx11DeviceImmContext;
			vmint2 fb_taa;
			iobj->GetFrameBufferInfo(&fb_taa);

			GpuRes gres_fb_history;
			grd_helper::UpdateFrameBuffer(gres_fb_history, iobj, "TAA_HISTORY_RGBA", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R16G16B16A16_FLOAT, 0);

			const int taa_accum = iobj->GetObjParam<int>("_int_TaaAccum", (int)0);

			ID3D11UnorderedAccessView* taaUAVs[2] = {
					(ID3D11UnorderedAccessView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_fb_history.alloc_res_ptrs[DTYPE_UAV]
			};

			ID3D11Buffer* cbuf_cam_state = g_psoManager.get_cbuf("CB_CameraState");
			D3D11_MAPPED_SUBRESOURCE mappedTaa;
			dx11ImmCtx->Map(cbuf_cam_state, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedTaa);
			CB_CameraState* cbTaa = (CB_CameraState*)mappedTaa.pData;
			cbTaa->rt_width = fb_taa.x;
			cbTaa->rt_height = fb_taa.y;
			cbTaa->iSrCamDummy__0 = (uint32_t)taa_accum; // samples already accumulated in history
#if __HDR_PIPELINE
			// TaaResolve sanitizes its input against tm_radiance_ceiling, and WRITE_DISCARD above means nothing
			// survives from the previous fill. The tonemap pass uses the SAME bound on the way to the screen --
			// if the two disagreed, the invariant they enforce would depend on whether TAA happened to be on.
			tmParams.ApplyTo(*cbTaa);
#endif
			dx11ImmCtx->Unmap(cbuf_cam_state, 0);
			dx11ImmCtx->CSSetConstantBuffers(0, 1, &cbuf_cam_state);

			ID3D11ComputeShader* csTaa = g_psoManager.get_cshader("CS_TaaResolve_cs_5_0");
			dx11ImmCtx->CSSetShader(csTaa, NULL, 0);
			dx11ImmCtx->CSSetUnorderedAccessViews(1, 2, taaUAVs, (UINT*)(&taaUAVs));

			int __BLOCKSIZE_TAA = _fncontainer.fnParams.GetParam("_int_GpuThreadBlockSize", (int)4);
			uint32_t taa_gx = (uint32_t)ceil(fb_taa.x / (float)__BLOCKSIZE_TAA);
			uint32_t taa_gy = (uint32_t)ceil(fb_taa.y / (float)__BLOCKSIZE_TAA);
			dx11ImmCtx->Dispatch(taa_gx, taa_gy, 1);

			ID3D11UnorderedAccessView* taaUAVs_NULL[2] = { NULL, NULL };
			dx11ImmCtx->CSSetUnorderedAccessViews(1, 2, taaUAVs_NULL, (UINT*)(&taaUAVs_NULL));

			// Advance accumulation for the next frame; core's CheckRenderConvergence reads this back.
			iobj->SetObjParam("_int_TaaAccum", (taa_accum + 1 < taa_max_samples) ? taa_accum + 1 : taa_max_samples);
		}

		DispatchTonemap();

		// From here on the presented image IS the LDR target: D2D binds it, the shared-present copy sources it,
		// and RenderOut() re-fetches it by name. RENDER_OUT_RGBA_0 (FP16) is scene data from this point, not
		// output -- nothing downstream may touch it.
		grd_helper::UpdateFrameBuffer(gres_fb_rgba, iobj, __PRESENT_RT_NAME, RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R8G8B8A8_UNORM, 0);

#endif

		ID3D11Texture2D* rtTex2D = (ID3D11Texture2D*)gres_fb_rgba.alloc_res_ptrs[DTYPE_RES];

		// check release or not
		D2DRes* res2d = NULL;
		bool reGenRes = false;
		auto it = g_d2dResMap.find(iobj->GetObjectID());
		if (it == g_d2dResMap.end()) {
			g_d2dResMap[iobj->GetObjectID()] = D2DRes();
			res2d = &g_d2dResMap[iobj->GetObjectID()];
			reGenRes = true;
		}
		else {
			res2d = &it->second;
			if (res2d->pTex2DRT != rtTex2D) {
				res2d->ReleaseD2DRes(); // clean also shared res views
				reGenRes = true;
			}
		}

		if (reGenRes) {
			// https://learn.microsoft.com/en-us/windows/win32/direct2d/direct2d-quickstart
			// https://learn.microsoft.com/en-us/windows/win32/direct2d/direct2d-and-direct3d-interoperation-overview
			IDXGISurface* pDxgiSurface = NULL;
			HRESULT hr = rtTex2D->QueryInterface(&pDxgiSurface);

			D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
					D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 0, 0);
			ID2D1RenderTarget* pRenderTarget = NULL;
			hr = g_pDirect2dFactory->CreateDxgiSurfaceRenderTarget(pDxgiSurface, &props, &pRenderTarget);

			//D2D1_BITMAP_PROPERTIES1 bitmapProperties =
			//	D2D1::BitmapProperties1(
			//		D2D1_BITMAP_OPTIONS_TARGET,
			//		D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
			//		0,
			//		0
			//	);
			//ID2D1Bitmap1* pRenderTarget = NULL;
			//hr = g_d2dDeviceContext->CreateBitmapFromDxgiSurface(pDxgiSurface, &bitmapProperties, &pRenderTarget);

			
			ID2D1SolidColorBrush* pSolidBrush = NULL;
			hr = pRenderTarget->CreateSolidColorBrush(
				D2D1::ColorF(D2D1::ColorF::White),
				&pSolidBrush
			);

			res2d->pTex2DRT = rtTex2D;
			res2d->pDxgiSurface = pDxgiSurface;
			res2d->pRenderTarget = pRenderTarget;
			res2d->pSolidBrush = pSolidBrush;
		}

		// drawing //
		// Now we can set the Direct2D render target.
		//g_d2dDeviceContext->SetTarget(res2d->pRenderTarget);

		res2d->pRenderTarget->BeginDraw();
		res2d->pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

		if (0)
		{
			res2d->pRenderTarget->DrawLine(
				D2D1::Point2F(100.f, 100.f),
				D2D1::Point2F(500.f, 500.f),
				res2d->pSolidBrush,
				1.5f, g_d2dStrokeStyleMap["DEFAULT"]
			);
		}

		// Draw circle at center (sculpt debug visualization)
#ifdef SCREEN_DEBUG
		if (!is_sectional)
		{
			D2D1_SIZE_F rSize = res2d->pRenderTarget->GetSize();
			float centerX = rSize.width / 2.0f;
			float centerY = rSize.height / 2.0f;
			float radius = 100.0f; // Circle radius in pixels (matches ProcModules.cpp:700)

			// Draw circle outline (bright for debugging)
			res2d->pSolidBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Yellow, 1.0f));
			res2d->pRenderTarget->DrawEllipse(
				D2D1::Ellipse(D2D1::Point2F(centerX, centerY), radius, radius),
				res2d->pSolidBrush,
				3.0f, // Thicker stroke for better visibility
				g_d2dStrokeStyleMap["DEFAULT"]
			);

			// Draw center crosshair for precise alignment check
			res2d->pSolidBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Red, 1.0f));
			res2d->pRenderTarget->DrawLine(
				D2D1::Point2F(centerX - 10.f, centerY),
				D2D1::Point2F(centerX + 10.f, centerY),
				res2d->pSolidBrush, 2.0f
			);
			res2d->pRenderTarget->DrawLine(
				D2D1::Point2F(centerX, centerY - 10.f),
				D2D1::Point2F(centerX, centerY + 10.f),
				res2d->pSolidBrush, 2.0f
			);
		}
#endif

		auto s2ws = [](const std::string& s)
		{
			int len;
			int slength = (int)s.length() + 1;
			len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
			wchar_t* buf = new wchar_t[len];
			MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
			std::wstring r(buf);
			delete[] buf;
			return r;
		};

		vector<TextItem>* textItems = (vector<TextItem>*)_fncontainer.fnParams.GetParam("_vector<TextItem>*_TextItems", (void*)NULL);
		if (textItems) {
			// https://learn.microsoft.com/en-us/windows/win32/Direct2D/how-to--draw-text
			D2D1_SIZE_F rSize = res2d->pRenderTarget->GetSize();
			IDWriteTextFormat* textFormat = g_d2dTextFormatMap["DEFAULT"];
			vector<TextItem>& _textItems = *textItems;
			for (TextItem& titem : _textItems) {
				std::wstring fontName_w = s2ws(titem.font);
				std::wstring text_w = s2ws(titem.textStr);

				IDWriteTextFormat* pDynamicTextFormat = nullptr;
				if (g_pDWriteFactory->CreateTextFormat(
					fontName_w.c_str(), nullptr,
					(DWRITE_FONT_WEIGHT)(titem.fontWeight * 100),
					titem.isItalic? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
					DWRITE_FONT_STRETCH_NORMAL, titem.fontSize, L"", &pDynamicTextFormat) != S_OK)
					continue;

				if (titem.alignment == "CENTER") {
					pDynamicTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
				}
				else if (titem.alignment == "RIGHT") {
					pDynamicTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
				}

				IDWriteTextLayout* pTextLayout = nullptr;
				HRESULT hr = g_pDWriteFactory->CreateTextLayout(
					text_w.c_str(),
					(UINT32)text_w.length(),
					pDynamicTextFormat,
					rSize.width,   
					rSize.height, 
					&pTextLayout
				);
				
				if (SUCCEEDED(hr)) {
					DWRITE_TEXT_METRICS textMetrics;
					hr = pTextLayout->GetMetrics(&textMetrics);
					if (SUCCEEDED(hr)) {
						float actualWidth = textMetrics.width;
						float actualHeight = textMetrics.height;
						titem.userData[1] = (int)actualWidth;
						titem.userData[2] = (int)actualHeight;
					}
					pTextLayout->Release();
				}
				
				UINT32 color_ = (titem.color & 0xFF) << 16 | ((titem.color >> 16) & 0xFF) | (titem.color & 0x00FF00);

				res2d->pSolidBrush->SetColor(D2D1::ColorF(color_, titem.alpha)); // D2D1::ColorF::Black
				const D2D1_RECT_F rectangle1 = D2D1::RectF((float)titem.posScreenX, (float)titem.posScreenY, rSize.width, rSize.height);

				res2d->pRenderTarget->DrawText(
					text_w.c_str(),
					text_w.length(),
					pDynamicTextFormat,
					&rectangle1,
					res2d->pSolidBrush
				);

				pDynamicTextFormat->Release();
			}
		}

		//= D2D1::RectF(0, 0, rSize.width, rSize.height);
		


		if (res2d->pRenderTarget->EndDraw() != S_OK)
			vmlog::LogErr("D2D Draw Error!");

#ifdef DX11_3
		// ---- Cross-device write-after-read fix: rotate the finalized frame through 3 shared PRESENT buffers ----
		// The app samples the renderer's output through a legacy GetSharedHandle SRV on its OWN D3D11 device (no
		// keyed mutex). The producer fence in GetSharedShaderResView already stops the app from reading a frame the
		// renderer has not finished writing, but it cannot cover the REVERSE hazard: frame N+1's renderer write
		// clobbering RENDER_OUT_RGBA_0 while the app device's GPU is still sampling frame N. Rather than ping-pong
		// the whole pipeline, we keep rendering into RENDER_OUT_RGBA_0 exactly as before and, now that the frame is
		// fully finalized (AFTER the D2D overlay EndDraw above, so overlay text/lines are included), COPY it into
		// one of 3 rotating SHARED_PRESENT_RGBA_{0,1,2} textures. The read side hands the app the SRV of the buffer
		// just written; because we cycle 0->1->2->0..., that buffer is not overwritten for 2 more frames, giving
		// the app's in-flight read enough slack to break the hazard without any cross-device sync primitive.
		// Guarded by is_last_renderer so it runs exactly once per frame for the final composited source only (same
		// scoping as the TAA resolve above). DX11.3-only: the DX11_0 / DX10_0 builds keep the original present path.
		if (is_last_renderer)
		{
			const int present_idx = iobj->GetObjParam<int>("_int_PresentBufIdx", (int)0);
			const std::string present_name = "SHARED_PRESENT_RGBA_" + std::to_string(present_idx);

			// RENDER_TARGET bind is only requested so gpures_interface auto-adds D3D11_RESOURCE_MISC_SHARED
			// (see gpures_interface.cpp: it flags any non-staging RENDER_TARGET texture as shareable); the buffer
			// itself is used purely as CopyResource-dest + shared SRV source. Same width/height/format as the
			// tonemapped LDR target it is copied from (gres_fb_rgba was repointed at it above -- CopyResource
			// requires matching formats, and the FP16 scene target would not qualify), so it is recreated by
			// UpdateFrameBuffer on a framebuffer resize just like it.
			GpuRes gres_present;
			grd_helper::UpdateFrameBuffer(gres_present, iobj, present_name, RTYPE_TEXTURE2D,
				D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, DXGI_FORMAT_R8G8B8A8_UNORM, 0);

			g_psoManager.dx11DeviceImmContext->CopyResource(
				(ID3D11Texture2D*)gres_present.alloc_res_ptrs[DTYPE_RES],
				(ID3D11Texture2D*)gres_fb_rgba.alloc_res_ptrs[DTYPE_RES]);

			// Tell the read side (GetSharedShaderResView, which only has iobjId) which buffer holds this frame.
			// res2d is keyed by iobjId in g_d2dResMap, so it is the read side's channel; also mirror onto the iobj
			// param so the rotation counter persists across calls. Advance so the NEXT frame targets a new buffer.
			res2d->presentReadIdx = present_idx;
			iobj->SetObjParam("_int_PresentBufReadIdx", present_idx);
			iobj->SetObjParam("_int_PresentBufIdx", (present_idx + 1) % 3);
		}
#endif

		// https://learn.microsoft.com/en-us/windows/win32/direct2d/path-geometries-overview

		/*
		if (pvtrLineList)
		{
			int num_lines = ((int)pvtrLineList->size() - prev_lines_count) / 4;
			if (num_lines) {
				vtrLineListObjColor.push_back(vmfloat3(actor->color));
				vtrLineListObjNum.push_back(num_lines);
				vtrLineListObjID.push_back(pobj->GetObjectID());
				prev_lines_count = (int)pvtrLineList->size();
			}
		}
		/**/

		bool isSkipSysFbBupdate = _fncontainer.fnParams.GetParam("_bool_SkipSysFBUpdate", false);
		if (!isSkipSysFbBupdate) RenderOut();
	}
	else if (_fncontainer.fnParams.GetParam("STORE_RT_IOBJ", false) && !is_vr)
	{
#if __HDR_PIPELINE
		// This path skips the final-render-out block entirely, so it would skip the tonemap with it -- and
		// RenderOut() now sources the LDR target, which would still be empty. Tonemap here too: the readback is
		// an RGBA8 CPU framebuffer, so it needs the display-range image, and CopyResource into SYSTEM_OUT_RGBA
		// (R8G8B8A8_UNORM) would fail outright against the FP16 scene target.
		DispatchTonemap();
#endif
		RenderOut();
		LeaveCriticalSection(&cs);
		return true;
	}


	if (is_last_renderer && !is_picking_routine && gpu_query_synch)
	{
		//g_vmCommonParams.dx11DeviceImmContext->End(g_vmCommonParams.dx11qr_timestamps[1]);
		g_psoManager.dx11DeviceImmContext->End(g_psoManager.dx11qr_disjoint);

		// Wait for data to be available
		static int testConsoleOut = 0;
		testConsoleOut++;
		int sleepCount = 0;

		while (g_psoManager.dx11DeviceImmContext->GetData(g_psoManager.dx11qr_disjoint, NULL, 0, 0) == S_FALSE)
		{
			Sleep(1);       // Wait a bit, but give other threads a chance to run
			//sleepCount++;
		}

		if (testConsoleOut % 100 == 0)
		{
			//vmlog::LogInfo("Sleep Count : " + std::to_string(sleepCount));


			// Check whether timestamps were disjoint during the last frame
			D3D10_QUERY_DATA_TIMESTAMP_DISJOINT tsDisjoint;
			g_psoManager.dx11DeviceImmContext->GetData(g_psoManager.dx11qr_disjoint, &tsDisjoint, sizeof(tsDisjoint), 0);
			if (!tsDisjoint.Disjoint)
			{
				/*
				auto DisplayDuration = [&tsDisjoint](UINT64 tsS, UINT64 tsE, const string& _test)
				{
					if (tsS == 0 || tsE == 0) return;
					cout << _test << " : " << float(tsE - tsS) / float(tsDisjoint.Frequency) * 1000.0f << " ms" << endl;
					cout << _test << "ts : " << tsS << endl;
					cout << _test << "te : " << tsE << endl;
				};
				UINT64 ts, te;

				while (g_vmCommonParams.dx11DeviceImmContext->GetData(g_vmCommonParams.dx11qr_timestamps[0], &ts, sizeof(UINT64), 0) == S_FALSE)
				{
					Sleep(1);       // Wait a bit, but give other threads a chance to run
					sleepCount++;
				}

				while (g_vmCommonParams.dx11DeviceImmContext->GetData(g_vmCommonParams.dx11qr_timestamps[1], &te, sizeof(UINT64), 0) == S_FALSE)
				{
					Sleep(1);       // Wait a bit, but give other threads a chance dx11qr_timestamps[0]to run
					sleepCount++;
				}

				vmlog::LogInfo("Sleep Count2 : " + std::to_string(sleepCount));



				//g_vmCommonParams.dx11DeviceImmContext->GetData(g_vmCommonParams.dx11qr_timestamps[0], &ts, sizeof(UINT64), 0);
				//g_vmCommonParams.dx11DeviceImmContext->GetData(g_vmCommonParams.dx11qr_timestamps[1], &te, sizeof(UINT64), 0);

				DisplayDuration(ts, te, "TIME : ");
				/**/

			}
			else {
				vmlog::LogErr("ERROR for QUERY-based SYNCH");
			}
		}
	}

	g_dProgress = 100;

	if (g_psoManager.gpu_profile)
	{
		g_psoManager.dx11DeviceImmContext->End(g_psoManager.dx11qr_disjoint);

		// Wait for data to be available
		while (g_psoManager.dx11DeviceImmContext->GetData(g_psoManager.dx11qr_disjoint, NULL, 0, 0) == S_FALSE)
		{
			Sleep(1);       // Wait a bit, but give other threads a chance to run
		}

		// Check whether timestamps were disjoint during the last frame
		D3D10_QUERY_DATA_TIMESTAMP_DISJOINT tsDisjoint;
		g_psoManager.dx11DeviceImmContext->GetData(g_psoManager.dx11qr_disjoint, &tsDisjoint, sizeof(tsDisjoint), 0);
		if (!tsDisjoint.Disjoint)
		{
			auto DisplayDuration = [&tsDisjoint](UINT64 tsS, UINT64 tsE, const string& _test)
			{
				if (tsS == 0 || tsE == 0) return;
				cout << _test << " : " << float(tsE - tsS) / float(tsDisjoint.Frequency) * 1000.0f << " ms" << endl;
			};

			for (auto& it : profile_map) {
				UINT64 ts, te;
				g_psoManager.dx11DeviceImmContext->GetData(g_psoManager.dx11qr_timestamps[it.second.x], &ts, sizeof(UINT64), 0);
				g_psoManager.dx11DeviceImmContext->GetData(g_psoManager.dx11qr_timestamps[it.second.y], &te, sizeof(UINT64), 0);

				DisplayDuration(ts, te, it.first);
			}

			//if (test_fps_profiling)
			//{
			//	auto it = profile_map.find("SR Render");
			//	UINT64 ts, te;
			//	dx11DeviceImmContext->GetData(psoManager->dx11qr_timestamps[it->second.x], &ts, sizeof(UINT64), 0);
			//	dx11DeviceImmContext->GetData(psoManager->dx11qr_timestamps[it->second.y], &te, sizeof(UINT64), 0);
			//	ofstream file_rendertime;
			//	file_rendertime.open(".\\data\\frames_profile_rendertime.txt", std::ios_base::app);
			//	file_rendertime << float(te - ts) / float(tsDisjoint.Frequency) * 1000.0f << endl;
			//	file_rendertime.close();
			//}
		}
	}

	LeaveCriticalSection(&cs);

	return true;
}

void DeInitModule(fncontainer::VmFnContainer& _fncontainer)
{
	// (verification 16:55 Major 2 / 17:29 Minor 1) The loader always calls DeInit on an Init that returned false.
	// The flag is set once teardown ownership begins (right after cs), so a PRE-init module (arm-refuse returns
	// before cs -> nothing acquired) is a safe no-op, while a partial/full init tears down the stages it owns.
	// Deleting an uninitialized CRITICAL_SECTION is UB, which the no-op prevents.
	if (!g_moduleInitialized) return;
	g_moduleInitialized = false;
	DeleteCriticalSection(&cs);

	// Drop the release hook before tearing down the maps it references.
	ClearPerSrcIdReleaseHooks();

	for (auto it = g_d2dResMap.begin(); it != g_d2dResMap.end(); it++) {
		it->second.ReleaseD2DRes();
	}
	g_d2dResMap.clear();
	for (auto it = g_d2dStrokeStyleMap.begin(); it != g_d2dStrokeStyleMap.end(); it++) {
		VMSAFE_RELEASE(it->second);
	}
	g_d2dStrokeStyleMap.clear();

	for (auto it = g_d2dTextFormatMap.begin(); it != g_d2dTextFormatMap.end(); it++) {
		VMSAFE_RELEASE(it->second);
	}
	g_d2dTextFormatMap.clear();

	VMSAFE_RELEASE(g_pDWriteFactory);
	VMSAFE_RELEASE(g_pDirect2dFactory);


	// ORDER!!
	grd_helper::Deinitialize();
	VMSAFE_DELETE(g_pCGpuManager);
}

int GetProgress(std::string& progressTag)
{
	return ((int)g_dProgress % 100);
}

void GetModuleSpecification(std::vector<std::string>& requirements)
{
	requirements.push_back("DX11");
	requirements.push_back("WINDOWS");
	requirements.push_back("GPUMANAGER");
}

void InteropCustomWork(fncontainer::VmFnContainer& _fncontainer)
{
	if (g_pCGpuManager == NULL) // for module initialization
		g_pCGpuManager = new VmGpuManager(GpuSdkTypeDX11, __DLLNAME);
	_fncontainer.fnParams.SetParam("_string_CoreVersion", string(__VERSION));
	_fncontainer.fnParams.SetParam("_VmGpuManager*_", g_pCGpuManager);
}

bool GetSharedShaderResView(const int iobjId, const void* dx11devPtr, void** sharedSRV)
{
	//if (g_state != 1) 
	//	assert(0);
	*sharedSRV = nullptr;

	if (g_pCGpuManager == NULL) {
		vmlog::LogErr("No GPU Manager is assigned!");
		return false;
	}

	// The tonemapped LDR target, never RENDER_OUT_RGBA_0. What the app receives here is a shared R8G8B8A8_UNORM
	// SRV, and under the HDR pipeline RENDER_OUT_RGBA_0 is R16G16B16A16_FLOAT -- handing that out would either
	// fail to create the view or feed the app raw un-tonemapped linear radiance. Under DX10 the macro collapses
	// back to RENDER_OUT_RGBA_0 and this is exactly the previous behaviour.
	GpuRes gres_fb;
	gres_fb.vm_src_id = iobjId;
	gres_fb.res_name = __PRESENT_RT_NAME;
	if (!g_pCGpuManager->UpdateGpuResource(gres_fb)) {
		vmlog::LogErr("No GPU Rendertarget is assigned! (" + std::to_string(iobjId) + ")");
		return false;
	}

	// Cross-device shared-surface barrier. The app samples RENDER_OUT_RGBA_0 through a shared SRV on its OWN
	// D3D11 device, while the renderer wrote it (raster + DVR compute + TAA resolve) on a separate device. The
	// legacy GetSharedHandle share carries no keyed mutex, so without a barrier the app can sample a frame the
	// renderer GPU has not finished writing -> visible flicker. TAA amplifies it: it re-renders the static view
	// every ImGui frame, so the read almost always races the write. Drain the renderer GPU up to this point (the
	// read boundary, hit on both the cached and freshly-created SRV paths below) so the surface handed back is
	// fully written. On an idle frame nothing is pending, so the event resolves almost immediately.
	if (g_psoManager.dx11qr_fenceQuery)
	{
		g_psoManager.dx11DeviceImmContext->End(g_psoManager.dx11qr_fenceQuery);
		g_psoManager.dx11DeviceImmContext->Flush(); // required: submit the event so GetData cannot spin forever
		while (g_psoManager.dx11DeviceImmContext->GetData(g_psoManager.dx11qr_fenceQuery, NULL, 0, 0) == S_FALSE)
			;
	}

	D2DRes* res2d = NULL;
	{
		auto it = g_d2dResMap.find(iobjId);
		if (it == g_d2dResMap.end()) {
			vmlog::LogErr("No GPU Rendering is called! (" + std::to_string(iobjId) + ")");
			return false;
		}
		res2d = &it->second;
	}

#ifdef DX11_3
	// ---- Rotating-present read side (see the copy site in DoModule) ----
	// Hand the app the SRV of the present buffer the renderer just finished writing (presentReadIdx), NOT
	// RENDER_OUT_RGBA_0. Because the renderer cycles 0->1->2->0..., this buffer will not be overwritten for 2
	// more frames, so the app device's in-flight sampling of it cannot race the renderer's next write -- the
	// cross-device write-after-read hazard the producer fence above cannot cover. The present SRVs live on the
	// APP device and the underlying present texture rotates every frame, so they are cached separately from the
	// D2D render-target path (mapDevSharedSRVs / pTex2DRT, which stays pinned to RENDER_OUT_RGBA_0). Cache key
	// is (app-device ptr) within the per-buffer map mapDevPresentSRVs[readIdx]; a slot is invalidated when its
	// underlying present texture pointer changes (e.g. a framebuffer resize recreated it).
	{
		const int readIdx = res2d->presentReadIdx; // 0..2, set by the copy site after D2D EndDraw

		GpuRes gres_present;
		gres_present.vm_src_id = iobjId;
		gres_present.res_name = "SHARED_PRESENT_RGBA_" + std::to_string(readIdx);
		if (!g_pCGpuManager->UpdateGpuResource(gres_present)) {
			vmlog::LogErr("No present buffer is assigned! (" + std::to_string(iobjId) + ")");
			return false;
		}
		ID3D11Texture2D* presentTex2D = (ID3D11Texture2D*)gres_present.alloc_res_ptrs[DTYPE_RES];

		// Invalidate this slot's cached app-device SRVs if the underlying present texture was recreated (resize).
		if (res2d->pPresentTex[readIdx] != presentTex2D) {
			for (auto& kv : res2d->mapDevPresentSRVs[readIdx]) {
				VMSAFE_RELEASE(kv.second);
			}
			res2d->mapDevPresentSRVs[readIdx].clear();
			res2d->pPresentTex[readIdx] = presentTex2D;
		}

		auto itSRV = res2d->mapDevPresentSRVs[readIdx].find((void*)dx11devPtr);
		if (itSRV != res2d->mapDevPresentSRVs[readIdx].end()) {
			*sharedSRV = itSRV->second;
			return true;
		}

		IDXGIResource* pDXGIResource = NULL;
		presentTex2D->QueryInterface(__uuidof(IDXGIResource), (LPVOID*)&pDXGIResource);
		HANDLE sharedHandle = NULL;
		pDXGIResource->GetSharedHandle(&sharedHandle);
		pDXGIResource->Release();

		if (sharedHandle == NULL) {
			vmlog::LogErr("Not Allowed for Shared Present Resource! (" + std::to_string(iobjId) + ")");
			return false;
		}

		ID3D11Device* pdx11AnotherDev = (ID3D11Device*)dx11devPtr;
		ID3D11Texture2D* presentTexShared = NULL;
		pdx11AnotherDev->OpenSharedResource(sharedHandle, __uuidof(ID3D11Texture2D), (LPVOID*)&presentTexShared);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		ZeroMemory(&srvDesc, sizeof(srvDesc));
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.MostDetailedMip = 0;

		ID3D11ShaderResourceView** ppPresentSRV = &res2d->mapDevPresentSRVs[readIdx][(void*)dx11devPtr];
		pdx11AnotherDev->CreateShaderResourceView(presentTexShared, &srvDesc, ppPresentSRV);
		presentTexShared->Release();

		*sharedSRV = *ppPresentSRV;
		return true;
	}
#else
	// ---- Legacy single-surface present path (DX11_0 / DX10_0 builds) ----
	// These configs do not triple-buffer; hand the app the shared SRV of RENDER_OUT_RGBA_0 directly, exactly as
	// before. Behavior is unchanged from prior to the triple-buffering change.
	ID3D11Texture2D* rtTex2D = (ID3D11Texture2D*)gres_fb.alloc_res_ptrs[DTYPE_RES];

	ID3D11ShaderResourceView** ppsharedSRV = NULL;
	{
		auto itSRV = res2d->mapDevSharedSRVs.find((void*)dx11devPtr);
		if (itSRV != res2d->mapDevSharedSRVs.end()) {
			*sharedSRV = itSRV->second;
			return true;
		}
		ppsharedSRV = &res2d->mapDevSharedSRVs[(void*)dx11devPtr];
	}

	// QI IDXGIResource interface to synchronized shared surface.
	IDXGIResource* pDXGIResource = NULL;
	rtTex2D->QueryInterface(__uuidof(IDXGIResource), (LPVOID*)&pDXGIResource);

	// obtain handle to IDXGIResource object.
	HANDLE sharedHandle;
	// this code snippet is only for dx11.0
	// for dx11.1 or higher, refer to
	// https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiresource-getsharedhandle
	pDXGIResource->GetSharedHandle(&sharedHandle);
	pDXGIResource->Release();

	ID3D11Device* pdx11AnotherDev = (ID3D11Device*)dx11devPtr;

	if (sharedHandle == NULL) {
		vmlog::LogErr("Not Allowed for Shared Resource! (" + std::to_string(iobjId) + ")");
		return false;
	}

	ID3D11Texture2D* rtTex2;
	pdx11AnotherDev->OpenSharedResource(sharedHandle, __uuidof(ID3D11Texture2D), (LPVOID*)&rtTex2);

	// Create texture view
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;

	pdx11AnotherDev->CreateShaderResourceView(rtTex2, &srvDesc, ppsharedSRV);
	rtTex2->Release();

	*sharedSRV = *ppsharedSRV;
	return true;
#endif
}

bool GetRendererDevice(
	vmobjects::VmParamMap<std::string, std::any>& ioResObjs,
	vmobjects::VmParamMap<std::string, std::any>& ioActors,
	vmobjects::VmParamMap<std::string, std::any>& ioParams)
{
	if (g_pCGpuManager == NULL) {
		vmlog::LogErr("No GPU Manager is assigned!");
		return false;
	}

	__ID3D11Device* device;
#ifdef DX11_3
	g_pCGpuManager->GetDeviceInformation((void*)(&device), "DEVICE_POINTER_3");
#else
	g_pCGpuManager->GetDeviceInformation((void*)(&device), "DEVICE_POINTER");
#endif
	ioParams.SetParam("DirectX11Device", (void*)device);
	return true;
}


constexpr size_t FNV1aHash(std::string_view str, size_t hash = 14695981039346656037ULL) {
	for (char c : str) {
		hash ^= static_cast<size_t>(c);
		hash *= 1099511628211ULL;
	}
	return hash;
}
constexpr static size_t HASH_BLEND_NORMAL = FNV1aHash("NORMAL");
constexpr static size_t HASH_BLEND_MULTIPLY = FNV1aHash("MULTIPLY");
constexpr static size_t HASH_BLEND_ADDITIVE = FNV1aHash("ADDITIVE");
constexpr static size_t HASH_BLEND_OVERLAY = FNV1aHash("OVERLAY");
constexpr static size_t HASH_BLEND_SOFT_LIGHT = FNV1aHash("SOFT_LIGHT");
constexpr static size_t HASH_BLEND_SCREEN = FNV1aHash("SCREEN");

bool BrushMeshActor(
	vmobjects::VmParamMap<std::string, std::any>& ioResObjs,
	vmobjects::VmParamMap<std::string, std::any>& ioActors,
	vmobjects::VmParamMap<std::string, std::any>& ioParams)
{
	VmActor* meshActor = ioActors.GetParam("TargetMeshActor", (VmActor*)NULL);
	VmIObject* iobj = ioActors.GetParam("SrcCamera", (VmIObject*)NULL);
	if (meshActor == nullptr || iobj == nullptr)
	{
		vzlog_error("Invalid TargetMeshActor(%d) or SrcCamera(%d)", meshActor ? 1 : 0, iobj ? 1 : 0);
		return false;
	}

	fncontainer::VmCamera* cam_obj = iobj->GetCameraObject();

	vmmat44 dmatWS2CS, dmatCS2PS, dmatPS2SS;
	vmmat44 dmatSS2PS, dmatPS2CS, dmatCS2WS;
	dmatWS2CS = cam_obj->mat_ws2cs; dmatCS2PS = cam_obj->mat_cs2ps; dmatPS2SS = cam_obj->mat_ps2ss;
	dmatSS2PS = cam_obj->mat_ss2ps; dmatPS2CS = cam_obj->mat_ps2cs; dmatCS2WS = cam_obj->mat_cs2ws;
	vmmat44 dmatWS2PS = dmatWS2CS * dmatCS2PS;
	vmmat44f matWS2CS = dmatWS2CS; // view
	vmmat44f matWS2PS = dmatWS2PS;
	vmmat44f matWS2SS = dmatWS2PS * dmatPS2SS;
	vmmat44f matSS2WS = (dmatSS2PS * dmatPS2CS) * dmatCS2WS;

	meshActor->SetParam("_string_PainterMode", ioParams.GetParam("_string_PainterMode", std::string("NONE")));
	vmfloat2 pos_xy = ioParams.GetParam("_float2_BrushPos", vmfloat2(-1.f, -1.f));

	BrushParams brushParams;
	*(vmfloat4*)brushParams.color = ioParams.GetParam("_float4_BrushColor", vmfloat4(1.f, 1.f, 1.f, 1.f));
	brushParams.size = ioParams.GetParam("_float_BrushSize", 1.f);
	brushParams.strength = std::max(std::min(ioParams.GetParam("_float_BrushStrength", 0.5f), 1.f), 0.f);
	brushParams.hardness = std::max(std::min(ioParams.GetParam("_float_BrushHardness", 0.5f), 1.f), 0.f);
	std::string blendmode_str = ioParams.GetParam("_string_BrushBlendMode", std::string("NORMAL"));
	size_t blendmode = FNV1aHash(blendmode_str);
	switch (blendmode)
	{
	case HASH_BLEND_NORMAL: brushParams.blendMode = PaintBlendMode::NORMAL; break;
	case HASH_BLEND_MULTIPLY: brushParams.blendMode = PaintBlendMode::MULTIPLY; break;
	case HASH_BLEND_ADDITIVE: brushParams.blendMode = PaintBlendMode::ADDITIVE; break;
	case HASH_BLEND_OVERLAY: brushParams.blendMode = PaintBlendMode::OVERLAY; break;
	case HASH_BLEND_SOFT_LIGHT: brushParams.blendMode = PaintBlendMode::SOFT_LIGHT; break;
	case HASH_BLEND_SCREEN: brushParams.blendMode = PaintBlendMode::SCREEN; break;
	default:
		vzlog_error("invalid paint brush mode! (%s)", blendmode_str.c_str());
		return false;
	}

	return grd_helper::UpdatePaintTexture(meshActor, matSS2WS, cam_obj, pos_xy, brushParams);
}

bool SelectPrimitives(
	vmobjects::VmParamMap<std::string, std::any>& ioResObjs,
	vmobjects::VmParamMap<std::string, std::any>& ioActors,
	vmobjects::VmParamMap<std::string, std::any>& ioParams)
{
	VmActor* meshActor = ioActors.GetParam("TargetMeshActor", (VmActor*)NULL);
	VmIObject* iobj = ioActors.GetParam("SrcCamera", (VmIObject*)NULL);
	if (meshActor == nullptr || iobj == nullptr)
	{
		vzlog_error("Invalid TargetMeshActor(%d) or SrcCamera(%d)", meshActor ? 1 : 0, iobj ? 1 : 0);
		return false;
	}

	fncontainer::VmCamera* cam_obj = iobj->GetCameraObject();

	// image mask corresponding to the rendering buffer
	// the mask size must be same to the rendering buffer size
	uint8_t* imageMask = ioParams.GetParam("_byte*_ImageMask", (uint8_t*)NULL);
	if (imageMask == nullptr)
	{
		vzlog_error("Invalid imageMask");
		return false;
	}

	std::vector<uint32_t>* ptr_listVertices = ioParams.GetParam("_vlist_ptr_uint_ListVertices", (std::vector<uint32_t>*)nullptr);
	std::vector<uint32_t>* ptr_listPolygons = ioParams.GetParam("_vlist_ptr_uint_ListPolygons", (std::vector<uint32_t>*)nullptr);
	if (ptr_listVertices == nullptr && ptr_listPolygons == nullptr)
	{
		vzlog_error("Non-output is required");
		return false;
	}

	VmVObjectPrimitive* pobj = (VmVObjectPrimitive*)meshActor->GetGeometryRes();
	if (pobj == nullptr)
	{
		vzlog_error("Invalid VmVObjectPrimitive");
		return false;
	}

	const PrimitiveData* prim_data = pobj->GetPrimitiveData();
	if (prim_data == nullptr)
	{
		vzlog_error("Invalid PrimitiveData");
		return false;
	}

	bool only_foremost = ioParams.GetParam("_bool_OnlyForemost", false);
	std::vector<float> depth_fb;
	std::vector<float> depth_fb_3x3;
	int w3 = 0, h3 = 0;
	if (only_foremost)
	{
		__ID3D11Device* dx11Device = g_psoManager.dx11Device;
		__ID3D11DeviceContext* dx11DeviceImmContext = g_psoManager.dx11DeviceImmContext;

		const uint32_t rtbind = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		GpuRes gres_fb_singlelayer_tempDepth, gres_fb_depthstencil;
		grd_helper::UpdateFrameBuffer(gres_fb_singlelayer_tempDepth, iobj, "RENDER_OUT_DEPTH_2", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R32_FLOAT, 0);
		grd_helper::UpdateFrameBuffer(gres_fb_depthstencil, iobj, "DEPTH_STENCIL", RTYPE_TEXTURE2D, D3D11_BIND_DEPTH_STENCIL, DXGI_FORMAT_D32_FLOAT, 0);

		ID3D11DepthStencilView* dx11DSV = (ID3D11DepthStencilView*)gres_fb_depthstencil.alloc_res_ptrs[DTYPE_DSV];
		ID3D11RenderTargetView* dx11RTV = (ID3D11RenderTargetView*)gres_fb_singlelayer_tempDepth.alloc_res_ptrs[DTYPE_RTV];
		float clr_float_fltmax_4[4] = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };
		dx11DeviceImmContext->ClearRenderTargetView(dx11RTV, clr_float_fltmax_4);
		dx11DeviceImmContext->ClearDepthStencilView(dx11DSV, D3D11_CLEAR_DEPTH, 0.0f, 0); // Reverse Z

		dx11DeviceImmContext->OMSetRenderTargets(1, &dx11RTV, dx11DSV);
		dx11DeviceImmContext->OMSetDepthStencilState(g_psoManager.get_depthstencil("LESSEQUAL"), 0);

		vmint2 fb_size_cur;
		iobj->GetFrameBufferInfo(&fb_size_cur);

		// Viewport
		D3D11_VIEWPORT dx11ViewPort;
		dx11ViewPort.Width = (float)fb_size_cur.x;
		dx11ViewPort.Height = (float)fb_size_cur.y;
		dx11ViewPort.MinDepth = 0;
		dx11ViewPort.MaxDepth = 1;
		dx11ViewPort.TopLeftX = 0;
		dx11ViewPort.TopLeftY = 0;
		dx11DeviceImmContext->RSSetViewports(1, &dx11ViewPort);

		// Camera matrices
		vmmat44 dmatWS2CS, dmatCS2PS, dmatPS2SS;
		vmmat44 dmatSS2PS, dmatPS2CS, dmatCS2WS;
		dmatWS2CS = cam_obj->mat_ws2cs; dmatCS2PS = cam_obj->mat_cs2ps; dmatPS2SS = cam_obj->mat_ps2ss;
		dmatSS2PS = cam_obj->mat_ss2ps; dmatPS2CS = cam_obj->mat_ps2cs; dmatCS2WS = cam_obj->mat_cs2ws;
		vmmat44 dmatWS2PS = dmatWS2CS * dmatCS2PS;
		vmmat44f matWS2CS = dmatWS2CS;
		vmmat44f matWS2PS = dmatWS2PS;
		vmmat44f matWS2SS = dmatWS2PS * dmatPS2SS;
		vmmat44f matSS2WS = (dmatSS2PS * dmatPS2CS) * dmatCS2WS;

		// CB_CameraState
		CB_CameraState cbCamState;
		grd_helper::SetCb_Camera(cbCamState, matWS2SS, matSS2WS, matWS2CS, matWS2PS, cam_obj, fb_size_cur, 8, 0);

		ID3D11Buffer* cbuf_cam_state = g_psoManager.get_cbuf("CB_CameraState");
		D3D11_MAPPED_SUBRESOURCE mappedResCamState;
		dx11DeviceImmContext->Map(cbuf_cam_state, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResCamState);
		memcpy(mappedResCamState.pData, &cbCamState, sizeof(CB_CameraState));
		dx11DeviceImmContext->Unmap(cbuf_cam_state, 0);

		dx11DeviceImmContext->VSSetConstantBuffers(0, 1, &cbuf_cam_state);
		dx11DeviceImmContext->PSSetConstantBuffers(0, 1, &cbuf_cam_state);

		// CB_PolygonObject
		CB_PolygonObject cbPolygonObj;
		grd_helper::SetCb_PolygonObj(cbPolygonObj, pobj, meshActor, matWS2SS, matWS2PS, false, false);

		ID3D11Buffer* cbuf_pobj = g_psoManager.get_cbuf("CB_PolygonObject");
		D3D11_MAPPED_SUBRESOURCE mappedResPobjData;
		dx11DeviceImmContext->Map(cbuf_pobj, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResPobjData);
		memcpy(mappedResPobjData.pData, &cbPolygonObj, sizeof(CB_PolygonObject));
		dx11DeviceImmContext->Unmap(cbuf_pobj, 0);

		dx11DeviceImmContext->VSSetConstantBuffers(1, 1, &cbuf_pobj);
		dx11DeviceImmContext->PSSetConstantBuffers(1, 1, &cbuf_pobj);

		int camClipMode = ioParams.GetParam("_int_ClippingMode", (int)0);
		vmfloat3 camClipPlanePos = ioParams.GetParam("_float3_PosClipPlaneWS", vmfloat3(0));
		vmfloat3 camClipPlaneDir = ioParams.GetParam("_float3_VecClipPlaneWS", vmfloat3(0));
		vmmat44f camClipMatWS2BS = ioParams.GetParam("_matrix44f_MatrixClipWS2BS", vmmat44f(1));
		std::set<int> camClipperFreeActors = ioParams.GetParam("_set_int_CamClipperFreeActors", std::set<int>());
		CB_ClipInfo cbClipInfo;
		grd_helper::SetCb_ClipInfo(cbClipInfo, pobj, meshActor, camClipMode, camClipperFreeActors, camClipMatWS2BS, camClipPlanePos, camClipPlaneDir);
		ID3D11Buffer* cbuf_clip = g_psoManager.get_cbuf("CB_ClipInfo");
		D3D11_MAPPED_SUBRESOURCE mappedResClipInfo;
		dx11DeviceImmContext->Map(cbuf_clip, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResClipInfo);
		memcpy(mappedResClipInfo.pData, &cbClipInfo, sizeof(CB_ClipInfo));
		dx11DeviceImmContext->Unmap(cbuf_clip, 0);
		dx11DeviceImmContext->PSSetConstantBuffers(2, 1, &cbuf_clip);

		// GPU primitive model (vertex/index buffers)
		GpuRes gres_idx;
		map<string, GpuRes> map_gres_texs, map_gres_vtxs;
		grd_helper::UpdatePrimitiveModel(map_gres_vtxs, gres_idx, map_gres_texs, pobj);

		// Vertex shader input layout
		uint32_t vs_mask = A_P;

		const Variant* pso = grd_helper::GetPSOVariant(vs_mask);
		dx11DeviceImmContext->IASetInputLayout(pso->il);
		dx11DeviceImmContext->VSSetShader(pso->vs, NULL, 0);
		dx11DeviceImmContext->GSSetShader(NULL, NULL, 0);

		// SINGLE_LAYER pixel shader (depth-only)
#ifdef DX10_0
		dx11DeviceImmContext->PSSetShader(g_psoManager.get_pshader("WRITE_DEPTH_ps_4_0"), NULL, 0);
#else
		dx11DeviceImmContext->PSSetShader(g_psoManager.get_pshader("WRITE_DEPTH_ps_5_0"), NULL, 0);
#endif
		dx11DeviceImmContext->RSSetState(g_psoManager.get_rasterizer("SOLID_NONE"));

		// Vertex buffers (7 slots, matching PrimitiveRenderer layout)
		ID3D11Buffer* dx11buffers[7] = {
			(ID3D11Buffer*)map_gres_vtxs["POSITION"].alloc_res_ptrs[DTYPE_RES],
			(ID3D11Buffer*)map_gres_vtxs["NORMAL"].alloc_res_ptrs[DTYPE_RES],
			(ID3D11Buffer*)map_gres_vtxs["TEXCOORD0"].alloc_res_ptrs[DTYPE_RES],
			(ID3D11Buffer*)map_gres_vtxs["COLOR"].alloc_res_ptrs[DTYPE_RES],
			(ID3D11Buffer*)map_gres_vtxs["TEXCOORD1"].alloc_res_ptrs[DTYPE_RES],
			(ID3D11Buffer*)map_gres_vtxs["TEXCOORD2"].alloc_res_ptrs[DTYPE_RES],
			(ID3D11Buffer*)map_gres_vtxs["GEODIST"].alloc_res_ptrs[DTYPE_RES],
		};
		UINT strides[7] = {
			sizeof(vmfloat3), sizeof(vmfloat3), sizeof(uint16_t) * 2,
			sizeof(uint32_t), sizeof(vmfloat3), sizeof(vmfloat3), sizeof(float),
		};
		UINT offsets[7] = { 0, 0, 0, 0, 0, 0, 0 };
		dx11DeviceImmContext->IASetVertexBuffers(0, 7, dx11buffers, strides, offsets);

		// Index buffer and topology
		D3D_PRIMITIVE_TOPOLOGY pobj_topology_type;
		if (prim_data->ptype == PrimitiveTypeTRIANGLE) {
			pobj_topology_type = prim_data->is_stripe ?
				D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP : D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		}
		else if (prim_data->ptype == PrimitiveTypeLINE) {
			pobj_topology_type = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
		}
		else {
			pobj_topology_type = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
		}
		dx11DeviceImmContext->IASetPrimitiveTopology(pobj_topology_type);

		if (prim_data->GetIndexBuffer() != NULL && prim_data->ptype != PrimitiveTypePOINT) {
			ID3D11Buffer* dx11IndiceTargetPrim = (ID3D11Buffer*)gres_idx.alloc_res_ptrs[DTYPE_RES];
			dx11DeviceImmContext->IASetIndexBuffer(dx11IndiceTargetPrim, DXGI_FORMAT_R32_UINT, 0);
		}

		// Draw
		if (prim_data->is_stripe || pobj_topology_type == D3D11_PRIMITIVE_TOPOLOGY_POINTLIST || prim_data->num_vidx == 0)
			dx11DeviceImmContext->Draw(prim_data->num_vtx, 0);
		else
			dx11DeviceImmContext->DrawIndexed(prim_data->num_vidx, 0, 0);

		// Unbind render targets
		dx11DeviceImmContext->OMSetRenderTargets(0, NULL, NULL);

		// Read back depth buffer to CPU
		GpuRes gres_fb_sys_depth;
		grd_helper::UpdateFrameBuffer(gres_fb_sys_depth, iobj, "SYSTEM_OUT_DEPTH", RTYPE_TEXTURE2D, NULL, DXGI_FORMAT_R32_FLOAT, UPFB_SYSOUT);
		dx11DeviceImmContext->CopyResource(
			(ID3D11Texture2D*)gres_fb_sys_depth.alloc_res_ptrs[DTYPE_RES],
			(ID3D11Texture2D*)gres_fb_singlelayer_tempDepth.alloc_res_ptrs[DTYPE_RES]);

		depth_fb.resize(fb_size_cur.x * fb_size_cur.y);
		w3 = fb_size_cur.x / 3;
		h3 = fb_size_cur.y / 3;
		depth_fb_3x3.resize(w3 * h3, FLT_MAX);

		D3D11_MAPPED_SUBRESOURCE mappedResDepth;
		dx11DeviceImmContext->Map((ID3D11Texture2D*)gres_fb_sys_depth.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, 0, &mappedResDepth);
		float* foremostDepthMap = (float*)mappedResDepth.pData;
		int foremostDepthRowPitch = mappedResDepth.RowPitch / sizeof(float);

		for (int y = 0; y < fb_size_cur.y; ++y)
		{
			memcpy(&depth_fb[y * fb_size_cur.x], foremostDepthMap + foremostDepthRowPitch * y, sizeof(float) * fb_size_cur.x);
		}

		dx11DeviceImmContext->Unmap((ID3D11Texture2D*)gres_fb_sys_depth.alloc_res_ptrs[DTYPE_RES], 0);

		for (int by = 0; by < h3; ++by)
		{
			for (int bx = 0; bx < w3; ++bx)
			{
				float maxDepth = -FLT_MAX;
				for (int dy = 0; dy < 3; ++dy)
				{
					for (int dx = 0; dx < 3; ++dx)
					{
						float d = depth_fb[(by * 3 + dy) * fb_size_cur.x + (bx * 3 + dx)];
						if (d < FLT_MAX && d > maxDepth)
							maxDepth = d;
					}
				}
				depth_fb_3x3[by * w3 + bx] = (maxDepth == -FLT_MAX) ? FLT_MAX : maxDepth;
			}
		}
	}

	vmmat44f matPivot = (meshActor->GetParam("_matrix44f_Pivot", vmmat44f(1)));
	vmmat44f matRS2WS = matPivot * meshActor->matOS2WS;

	vmmat44 dmatWS2CS, dmatCS2PS, dmatPS2SS;
	dmatWS2CS = cam_obj->mat_ws2cs; dmatCS2PS = cam_obj->mat_cs2ps; dmatPS2SS = cam_obj->mat_ps2ss;
	vmmat44 dmatWS2PS = dmatWS2CS * dmatCS2PS;
	vmmat44f matWS2CS = dmatWS2CS; // view
	vmmat44f matWS2PS = dmatWS2PS;
	vmmat44f matWS2SS = dmatWS2PS * dmatPS2SS;
	vmmat44f matRS2SS = matRS2WS * matWS2SS;

	vmmat44f matSS2WS;
	{
		vmmat44 dmatSS2PS, dmatPS2CS, dmatCS2WS;
		dmatSS2PS = cam_obj->mat_ss2ps; dmatPS2CS = cam_obj->mat_ps2cs; dmatCS2WS = cam_obj->mat_cs2ws;
		matSS2WS = (dmatSS2PS * dmatPS2CS) * dmatCS2WS;
	}

	vmfloat3* positions = prim_data->GetVerticeDefinition<vmfloat3>("POSITION");
	vmint2 i2SizeScreen;
	iobj->GetFrameBufferInfo(&i2SizeScreen);
	float depthThreshold = ioParams.GetParam("_float_DepthThreshold", 1.00f);
	if (ptr_listVertices)
	{
		ptr_listVertices->clear();
		ptr_listVertices->reserve(prim_data->num_vtx);
		for (uint32_t i = 0; i < prim_data->num_vtx; ++i)
		{
			vmfloat3 p = positions[i];
			vmfloat3 p_ss;
			vmmath::fTransformPoint(&p_ss, &p, &matRS2SS);
			if (p_ss.x >= 0 && p_ss.x < i2SizeScreen.x &&
				p_ss.y >= 0 && p_ss.y < i2SizeScreen.y) {
				if (imageMask[(int)p_ss.x + (int)p_ss.y * i2SizeScreen.x])
				{
					if (only_foremost && !depth_fb.empty())
					{
						vmfloat3 p_ws;
						vmmath::fTransformPoint(&p_ws, &p, &matRS2WS);

						vmfloat3 p_cs;
						vmmath::fTransformPoint(&p_cs, &p_ws, &matWS2CS);

						float vtx_depth = -p_cs.z;
						float stored_depth = depth_fb_3x3[(int)((p_ss.y + 0.5f) / 3) * w3 + (int)((p_ss.x + 0.5f) / 3)];
						if (vtx_depth > stored_depth + depthThreshold)
						{
							//if (p_cs.x * p_cs.x + p_cs.y * p_cs.y < 100)
							//{
							//	int gg = 0;
							//}
							continue;
						}
					}
					ptr_listVertices->push_back(i);
				}
			}
		}
	}
	if (ptr_listPolygons)
	{
		ptr_listPolygons->clear();
		if (prim_data->GetIndexBuffer() && prim_data->num_prims > 0 && !prim_data->is_stripe)
		{
			uint32_t idx_stride = prim_data->idx_stride;
			ptr_listPolygons->reserve(prim_data->num_prims);
			for (uint32_t i = 0; i < prim_data->num_prims; ++i)
			{
				uint32_t base = i * idx_stride;
				bool selected = false;
				for (uint32_t j = 0; j < idx_stride; ++j)
				{
					uint32_t vidx = prim_data->GetIndexBuffer()[base + j];
					vmfloat3 p = positions[vidx];
					vmfloat3 p_ss;
					vmmath::fTransformPoint(&p_ss, &p, &matRS2SS);
					if (p_ss.x >= 0 && p_ss.x < i2SizeScreen.x &&
						p_ss.y >= 0 && p_ss.y < i2SizeScreen.y) {
						if (imageMask[(int)p_ss.x + (int)p_ss.y * i2SizeScreen.x])
						{
							if (only_foremost && !depth_fb.empty())
							{
								vmfloat3 p_ws;
								vmmath::fTransformPoint(&p_ws, &p, &matRS2WS);

								vmfloat3 p_cs;
								vmmath::fTransformPoint(&p_cs, &p_ws, &matWS2CS);

								float vtx_depth = -p_cs.z;
								float stored_depth = depth_fb_3x3[(int)((p_ss.y + 0.5f) / 3) * w3 + (int)((p_ss.x + 0.5f) / 3)];
								if (vtx_depth > stored_depth + depthThreshold)
									continue;
							}
							selected = true;
							break;
						}
					}
				}
				if (selected)
				{
					ptr_listPolygons->push_back(i);
				}
			}
		}
		else if (prim_data->is_stripe && prim_data->num_prims > 0)
		{
			// triangle strip: polygon i uses vertices i, i+1, i+2
			ptr_listPolygons->reserve(prim_data->num_prims);
			for (uint32_t i = 0; i < prim_data->num_prims; ++i)
			{
				bool selected = false;
				for (uint32_t j = 0; j < 3; ++j)
				{
					uint32_t vidx = (prim_data->GetIndexBuffer()) ?
						prim_data->GetIndexBuffer()[i + j] : (i + j);
					vmfloat3 p = positions[vidx];
					vmfloat3 p_ss;
					vmmath::fTransformPoint(&p_ss, &p, &matRS2SS);
					if (p_ss.x >= 0 && p_ss.x < i2SizeScreen.x &&
						p_ss.y >= 0 && p_ss.y < i2SizeScreen.y) {
						if (imageMask[(int)p_ss.x + (int)p_ss.y * i2SizeScreen.x])
						{
							if (only_foremost && !depth_fb.empty())
							{
								vmfloat3 p_ws;
								vmmath::fTransformPoint(&p_ws, &p, &matRS2WS);

								vmfloat3 p_cs;
								vmmath::fTransformPoint(&p_cs, &p_ws, &matWS2CS);

								float vtx_depth = -p_cs.z;
								float stored_depth = depth_fb_3x3[(int)((p_ss.y + 0.5f) / 3) * w3 + (int)((p_ss.x + 0.5f) / 3)];
								if (vtx_depth > stored_depth + depthThreshold)
									continue;
							}
							selected = true;
							break;
						}
					}
				}
				if (selected)
				{
					ptr_listPolygons->push_back(i);
				}
			}
		}
	}
	return true;
}