#include "vismtv_inbuilt_renderergpudx.h"
//#include "VXDX11Helper.h"
#include "vzm2/Backlog.h"

#include <d2d1_1.h>
#include <dwrite.h>
#include "RendererHeader.h"

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

	void ReleaseD2DRes() {
		VMSAFE_RELEASE(pSolidBrush);
		VMSAFE_RELEASE(pRenderTarget);
		VMSAFE_RELEASE(pDxgiSurface);

		for (auto& kv : mapDevSharedSRVs) {
			VMSAFE_RELEASE(kv.second);
		}
		mapDevSharedSRVs.clear();
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
bool InitModule(fncontainer::VmFnContainer& _fncontainer)
{	
	InitializeCriticalSection(&cs);

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

	VmIObject* iobj = _fncontainer.fnParams.GetParam("_VmIObject*_RenderOut", (VmIObject*)NULL);
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
	g_psoManager.gpu_profile = _fncontainer.fnParams.GetParam("_bool_GpuProfile", false);
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

	if (is_first_renderer && !is_picking_routine && gpu_query_synch)
	{
		g_psoManager.dx11DeviceImmContext->Begin(g_psoManager.dx11qr_disjoint);
		//g_vmCommonParams.dx11DeviceImmContext->End(g_vmCommonParams.dx11qr_timestamps[0]);
	}

	if (strRendererSource == "VOLUME")
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

	auto RenderOut = [&iobj, &is_last_renderer, &planeThickness, &_fncontainer, &is_vr]() {

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
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
		grd_helper::UpdateFrameBuffer(gres_fb_depthcs, iobj, is_vr? "RENDER_OUT_DEPTH_1" : "RENDER_OUT_DEPTH_0", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, DXGI_FORMAT_R32_FLOAT, 0);
#else
		grd_helper::UpdateFrameBuffer(gres_fb_rgba, iobj, "RENDER_OUT_RGBA_0", RTYPE_TEXTURE2D,
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

	if (is_final_render_out && !is_picking_routine)
	{
		const uint32_t rtbind = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		GpuRes gres_fb_rgba;
		grd_helper::UpdateFrameBuffer(gres_fb_rgba, iobj, "RENDER_OUT_RGBA_0", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R8G8B8A8_UNORM, 0);

#ifndef DX10_0
		// here, compose the 2nd layer rendering target texture onto the final render out texture... 
		auto Blend2ndLayer = [&gres_fb_rgba, &iobj, &_fncontainer, &rtbind]() {

			vmint2 fb_size_cur;
			iobj->GetFrameBufferInfo(&fb_size_cur);

			GpuRes gres_fb_depthcs;
			grd_helper::UpdateFrameBuffer(gres_fb_depthcs, iobj, "RENDER_OUT_DEPTH_0", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R32_FLOAT, 0);

			GpuRes gres_fb_2ndlayer_rgba, gres_fb_2ndlayer_depthcs;
			grd_helper::UpdateFrameBuffer(gres_fb_2ndlayer_rgba, iobj, "RENDER_OUT_RGBA_1", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
			grd_helper::UpdateFrameBuffer(gres_fb_2ndlayer_depthcs, iobj, "RENDER_OUT_DEPTH_2", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R8G8B8A8_UNORM, 0);

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

		vector<TextItem>* textItems = (vector<TextItem>*)_fncontainer.fnParams.GetParam("_vector<TextItem>*_TextItems", (void*)NULL);
		if (textItems) {
			// IDWriteTextFormat ��ü ����.
			// https://learn.microsoft.com/en-us/windows/win32/Direct2D/how-to--draw-text
			D2D1_SIZE_F rSize = res2d->pRenderTarget->GetSize();
			IDWriteTextFormat* textFormat = g_d2dTextFormatMap["DEFAULT"];
			vector<TextItem>& _textItems = *textItems;
			for (TextItem& titem : _textItems) {
				std::wstring fontName_w;
				fontName_w.assign(titem.font.begin(), titem.font.end());
				std::wstring text_w;
				text_w.assign(titem.textStr.begin(), titem.textStr.end());

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
	DeleteCriticalSection(&cs);
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

	GpuRes gres_fb;
	gres_fb.vm_src_id = iobjId;
	gres_fb.res_name = "RENDER_OUT_RGBA_0";
	if (!g_pCGpuManager->UpdateGpuResource(gres_fb)) {
		vmlog::LogErr("No GPU Rendertarget is assigned! (" + std::to_string(iobjId) + ")");
		return false;
	}

	ID3D11Texture2D* rtTex2D = (ID3D11Texture2D*)gres_fb.alloc_res_ptrs[DTYPE_RES];

	ID3D11ShaderResourceView** ppsharedSRV = NULL;
	D2DRes* res2d = NULL;
	auto it = g_d2dResMap.find(iobjId);
	if (it == g_d2dResMap.end()) {
		vmlog::LogErr("No GPU Rendering is called! (" + std::to_string(iobjId) + ")");
		return false;
	}
	else {
		res2d = &it->second;

		auto it = res2d->mapDevSharedSRVs.find((void*)dx11devPtr);
		if (it != res2d->mapDevSharedSRVs.end()) {
			*sharedSRV = it->second;
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

	VmCObject* cam_obj = iobj->GetCameraObject();

	vmmat44 dmatWS2CS, dmatCS2PS, dmatPS2SS;
	vmmat44 dmatSS2PS, dmatPS2CS, dmatCS2WS;
	cam_obj->GetMatrixWStoSS(&dmatWS2CS, &dmatCS2PS, &dmatPS2SS);
	cam_obj->GetMatrixSStoWS(&dmatSS2PS, &dmatPS2CS, &dmatCS2WS);
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

	VmCObject* cam_obj = iobj->GetCameraObject();

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

	PrimitiveData* prim_data = pobj->GetPrimitiveData();
	if (prim_data == nullptr)
	{
		vzlog_error("Invalid PrimitiveData");
		return false;
	}

	bool only_foremost = ioParams.GetParam("_bool_OnlyForemost", false);
	std::vector<float> depth_fb;
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
		dx11DeviceImmContext->ClearDepthStencilView(dx11DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

		dx11DeviceImmContext->OMSetRenderTargets(1, &dx11RTV, dx11DSV);
		dx11DeviceImmContext->OMSetDepthStencilState(g_psoManager.get_depthstencil("LESSEQUAL"), 0);

		vmint2 fb_size_cur;
		iobj->GetFrameBufferInfo(&fb_size_cur);

		// Viewport
		D3D11_VIEWPORT dx11ViewPort;
		dx11ViewPort.Width = (float)fb_size_cur.x;
		dx11ViewPort.Height = (float)fb_size_cur.y;
		dx11ViewPort.MinDepth = 0;
		dx11ViewPort.MaxDepth = 1.0f;
		dx11ViewPort.TopLeftX = 0;
		dx11ViewPort.TopLeftY = 0;
		dx11DeviceImmContext->RSSetViewports(1, &dx11ViewPort);

		// Camera matrices
		vmmat44 dmatWS2CS, dmatCS2PS, dmatPS2SS;
		vmmat44 dmatSS2PS, dmatPS2CS, dmatCS2WS;
		cam_obj->GetMatrixWStoSS(&dmatWS2CS, &dmatCS2PS, &dmatPS2SS);
		cam_obj->GetMatrixSStoWS(&dmatSS2PS, &dmatPS2CS, &dmatCS2WS);
		vmmat44 dmatWS2PS = dmatWS2CS * dmatCS2PS;
		vmmat44f matWS2CS = dmatWS2CS;
		vmmat44f matWS2PS = dmatWS2PS;
		vmmat44f matWS2SS = dmatWS2PS * dmatPS2SS;
		vmmat44f matSS2WS = (dmatSS2PS * dmatPS2CS) * dmatCS2WS;

		// CB_CameraState
		CB_CameraState cbCamState;
		grd_helper::SetCb_Camera(cbCamState, matWS2SS, matSS2WS, matWS2CS, cam_obj, fb_size_cur, 8, 0);

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
		dx11DeviceImmContext->PSSetShader(g_psoManager.get_pshader("SR_SINGLE_LAYER_ps_4_0"), NULL, 0);
#else
		dx11DeviceImmContext->PSSetShader(g_psoManager.get_pshader("SR_SINGLE_LAYER_ps_5_0"), NULL, 0);
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

		if (prim_data->vidx_buffer != NULL && prim_data->ptype != PrimitiveTypePOINT) {
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

		D3D11_MAPPED_SUBRESOURCE mappedResDepth;
		dx11DeviceImmContext->Map((ID3D11Texture2D*)gres_fb_sys_depth.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, 0, &mappedResDepth);
		float* foremostDepthMap = (float*)mappedResDepth.pData;
		int foremostDepthRowPitch = mappedResDepth.RowPitch / sizeof(float);

		// TODO: use foremostDepthMap in vertex/polygon selection below
		for (int y = 0; y < fb_size_cur.y; ++y)
		{
			memcpy(&depth_fb[y * fb_size_cur.x], foremostDepthMap + foremostDepthRowPitch * y, sizeof(float) * fb_size_cur.x);
		}

		dx11DeviceImmContext->Unmap((ID3D11Texture2D*)gres_fb_sys_depth.alloc_res_ptrs[DTYPE_RES], 0);
	}


	vmmat44f matPivot = (meshActor->GetParam("_matrix44f_Pivot", vmmat44f(1)));
	vmmat44f matRS2WS = matPivot * meshActor->matOS2WS;

	vmmat44 dmatWS2CS, dmatCS2PS, dmatPS2SS;
	cam_obj->GetMatrixWStoSS(&dmatWS2CS, &dmatCS2PS, &dmatPS2SS);
	vmmat44 dmatWS2PS = dmatWS2CS * dmatCS2PS;
	vmmat44f matWS2CS = dmatWS2CS; // view
	vmmat44f matWS2PS = dmatWS2PS;
	vmmat44f matWS2SS = dmatWS2PS * dmatPS2SS;
	vmmat44f matRS2SS = matRS2WS * matWS2SS;

	vmmat44f matSS2WS;
	{
		vmmat44 dmatSS2PS, dmatPS2CS, dmatCS2WS;
		cam_obj->GetMatrixSStoWS(&dmatSS2PS, &dmatPS2CS, &dmatCS2WS);
		matSS2WS = (dmatSS2PS * dmatPS2CS) * dmatCS2WS;
	}

	vmfloat3* positions = prim_data->GetVerticeDefinition<vmfloat3>("POSITION");
	vmint2 i2SizeScreen;
	iobj->GetFrameBufferInfo(&i2SizeScreen);
	float depthThreshold = ioParams.GetParam("_float_DepthThreshold", 0.5f);
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
						vmfloat3 pos_ip_ss = vmfloat3(p_ss.x, p_ss.y, 0);
						vmfloat3 pos_ip_ws;
						vmmath::fTransformPoint(&pos_ip_ws, &pos_ip_ss, &matSS2WS);
						vmfloat3 diff = p_ws - pos_ip_ws;
						float vtx_depth = sqrtf(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
						float stored_depth = depth_fb[(int)p_ss.x + (int)p_ss.y * i2SizeScreen.x];
						if (stored_depth >= FLT_MAX || fabs(vtx_depth - stored_depth) > depthThreshold)
							continue;
					}
					ptr_listVertices->push_back(i);
				}
			}
		}
	}
	if (ptr_listPolygons)
	{
		ptr_listPolygons->clear();
		if (prim_data->vidx_buffer && prim_data->num_prims > 0 && !prim_data->is_stripe)
		{
			uint32_t idx_stride = prim_data->idx_stride;
			ptr_listPolygons->reserve(prim_data->num_prims);
			for (uint32_t i = 0; i < prim_data->num_prims; ++i)
			{
				uint32_t base = i * idx_stride;
				bool selected = false;
				for (uint32_t j = 0; j < idx_stride; ++j)
				{
					uint32_t vidx = prim_data->vidx_buffer[base + j];
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
								vmfloat3 pos_ip_ss = vmfloat3(p_ss.x, p_ss.y, 0);
								vmfloat3 pos_ip_ws;
								vmmath::fTransformPoint(&pos_ip_ws, &pos_ip_ss, &matSS2WS);
								vmfloat3 diff = p_ws - pos_ip_ws;
								float vtx_depth = sqrtf(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
								float stored_depth = depth_fb[(int)p_ss.x + (int)p_ss.y * i2SizeScreen.x];
								if (stored_depth >= FLT_MAX || fabs(vtx_depth - stored_depth) > depthThreshold)
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
					uint32_t vidx = (prim_data->vidx_buffer) ?
						prim_data->vidx_buffer[i + j] : (i + j);
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
								vmfloat3 pos_ip_ss = vmfloat3(p_ss.x, p_ss.y, 0);
								vmfloat3 pos_ip_ws;
								vmmath::fTransformPoint(&pos_ip_ws, &pos_ip_ss, &matSS2WS);
								vmfloat3 diff = p_ws - pos_ip_ws;
								float vtx_depth = sqrtf(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
								float stored_depth = depth_fb[(int)p_ss.x + (int)p_ss.y * i2SizeScreen.x];
								if (stored_depth >= FLT_MAX || fabs(vtx_depth - stored_depth) > depthThreshold)
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