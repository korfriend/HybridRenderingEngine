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

ID2D1Factory* g_pDirect2dFactory = NULL;
struct D2DRes {
	IDXGISurface* pDxgiSurface = NULL;
	ID2D1RenderTarget* pRenderTarget = NULL; 
	ID2D1SolidColorBrush* pSolidBrush = NULL;

	ID3D11Texture2D* pTex2DRT = NULL; // do not release this!

	void ReleaseD2DRes() {
		VMSAFE_RELEASE(pSolidBrush);
		VMSAFE_RELEASE(pRenderTarget);
		VMSAFE_RELEASE(pDxgiSurface);
	}
};
map<int, D2DRes> g_d2dResMap;
map<string, ID2D1StrokeStyle*> g_d2dStrokeStyleMap;


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
		g_pCGpuManager = new VmGpuManager(GpuSdkTypeDX11, __DLLNAME);

	if (grd_helper::InitializePresettings(g_pCGpuManager, &g_vmCommonParams) == -1)
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
		if (D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_pDirect2dFactory) != S_OK)
			vmlog::LogErr("Failure D2D1CreateFactory!!");

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

#ifdef USE_DX11_3
	if (g_vmCommonParams.dx11_featureLevel < 0xb100) {
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

#pragma region GPU/CPU Pre Setting
	//float sizeGpuResourceForVolume = _fncontainer.fnParams.GetParam("_float_SizeGpuResourceForVolume", 80.0f);
	//// 100 means 50%
	//float resourceRatioForVolume = sizeGpuResourceForVolume * 0.5f * 0.01f;
	//uint uiDedicatedGpuMemoryKB = 
	//	(uint)(g_vmCommonParams.dx11_adapter.DedicatedVideoMemory / 1024);
	//float halfCriterionKB = (float)uiDedicatedGpuMemoryKB * resourceRatioForVolume;
	//float halfCriterionKB = _fncontainer.fnParams.GetParam("_float_GpuVolumeMaxSizeKB", 256.f * 1024.f);
	// In CPU VR mode, Recommend to set dHalfCriterionKB = 16;
	//vector<VmObject*> vtrInputVolumes;
	//_fncontainer.GetVmObjectList(&vtrInputVolumes, VmObjKey(ObjectTypeVOLUME, true));
	//for (int i = 0; i < (int)vtrInputVolumes.size(); i++)
	//	vtrInputVolumes.at(i)->RegisterCustomParameter("_float_ForcedHalfCriterionKB", halfCriterionKB);
#pragma endregion

	bool is_shadow = _fncontainer.fnParams.GetParam("_bool_IsShadow", false);
	bool curved_slicer = _fncontainer.fnParams.GetParam("_bool_IsNonlinear", false);
	string strRendererSource = _fncontainer.fnParams.GetParam("_string_RenderingSourceType", string("MESH"));
	bool is_sectional = strRendererSource == "SECTIONAL_MESH" || strRendererSource == "SECTIONAL_VOLUME";
	bool is_system_out = false;
	float planeThickness = _fncontainer.fnParams.GetParam("_float_PlaneThickness", -1.f);
	bool is_final_renderer = _fncontainer.fnParams.GetParam("_bool_IsFinalRenderer", true);
	g_vmCommonParams.gpu_profile = _fncontainer.fnParams.GetParam("_bool_GpuProfile", false);
	map<string, vmint2>& profile_map = g_vmCommonParams.profile_map;
	if (g_vmCommonParams.gpu_profile)
	{
		int gpu_profilecount = (int)profile_map.size();
		g_vmCommonParams.dx11DeviceImmContext->Begin(g_vmCommonParams.dx11qr_disjoint);
		//gpu_profilecount++;
	}

	if (!is_sectional && is_shadow) {
		// to do //
	}

	if (strRendererSource == "SECTIONAL_VOLUME" && !curved_slicer)
		strRendererSource = "VOLUME";
	
	if (strRendererSource == "VOLUME")
	{
		double dRuntime = 0;
		RenderVrDLS(&_fncontainer, g_pCGpuManager, &g_vmCommonParams, &g_LocalProgress, &dRuntime);
		g_dRunTimeVRs += dRuntime;
		is_system_out = true;
	}
	else if (strRendererSource == "MESH")
	{
		double dRuntime = 0;
		RenderSrOIT(&_fncontainer, g_pCGpuManager, &g_vmCommonParams, &g_LocalProgress, &dRuntime);
		g_dRunTimeVRs += dRuntime;
		if (is_final_renderer) is_system_out = true;
	}
	else if (strRendererSource == "SECTIONAL_VOLUME")
	{
		assert(curved_slicer == true);
		double dRuntime = 0;
		RenderVrCurvedSlicer(&_fncontainer, g_pCGpuManager, &g_vmCommonParams, &g_LocalProgress, &dRuntime);
		g_dRunTimeVRs += dRuntime;
		bool is_system_out = true;
	}
	else if (strRendererSource == "SECTIONAL_MESH")
	{
		double dRuntime = 0;
		RenderSrSlicer(&_fncontainer, g_pCGpuManager, &g_vmCommonParams, &g_LocalProgress, &dRuntime);
		g_dRunTimeVRs += dRuntime;
		if (is_final_renderer || planeThickness <= 0.f) is_system_out = true;
	}

	auto RenderOut = [&iobj, &is_final_renderer, &planeThickness, &_fncontainer]() {

		g_vmCommonParams.GpuProfile("Copyback");

		VmGpuManager* gpu_manager = g_pCGpuManager;
		__ID3D11Device* dx11Device = g_vmCommonParams.dx11Device;
		__ID3D11DeviceContext* dx11DeviceImmContext = g_vmCommonParams.dx11DeviceImmContext;

		HWND hWnd = (HWND)_fncontainer.fnParams.GetParam("_hwnd_WindowHandle", (HWND)NULL);
		bool is_rgba = _fncontainer.fnParams.GetParam("_bool_IsRGBA", false);

		vmint2 fb_size_cur;
		iobj->GetFrameBufferInfo(&fb_size_cur);
		int count_call_render = iobj->GetObjParam("_int_NumCallRenders", 0);

		GpuRes gres_fb_rgba, gres_fb_depthcs;
		GpuRes gres_fb_sys_rgba, gres_fb_sys_depthcs;
		grd_helper::UpdateFrameBuffer(gres_fb_rgba, iobj, "RENDER_OUT_RGBA_0", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
		grd_helper::UpdateFrameBuffer(gres_fb_depthcs, iobj, "RENDER_OUT_DEPTH_0", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_FLOAT, 0);
		grd_helper::UpdateFrameBuffer(gres_fb_sys_rgba, iobj, "SYSTEM_OUT_RGBA", RTYPE_TEXTURE2D, NULL, DXGI_FORMAT_R8G8B8A8_UNORM, UPFB_SYSOUT);
		grd_helper::UpdateFrameBuffer(gres_fb_sys_depthcs, iobj, "SYSTEM_OUT_DEPTH", RTYPE_TEXTURE2D, NULL, DXGI_FORMAT_R32_FLOAT, UPFB_SYSOUT);

		if (hWnd && is_final_renderer)
		{
			// APPLY HWND MODE
			ID3D11Texture2D* pTex2dHwndRT = NULL;
			ID3D11RenderTargetView* pHwndRTV = NULL;
			gpu_manager->UpdateDXGI((void**)&pTex2dHwndRT, (void**)&pHwndRTV, hWnd, fb_size_cur.x, fb_size_cur.y);

			dx11DeviceImmContext->CopyResource(pTex2dHwndRT, (ID3D11Texture2D*)gres_fb_rgba.alloc_res_ptrs[DTYPE_RES]);
		}
		else {
			int outIndex = planeThickness == 0.f && !is_final_renderer ? 1 : 0; // just for SlicerSR combined with CPU MPR
			FrameBuffer* fb_rout = (FrameBuffer*)iobj->GetFrameBuffer(FrameBufferUsageRENDEROUT, outIndex);
			FrameBuffer* fb_dout = (FrameBuffer*)iobj->GetFrameBuffer(FrameBufferUsageDEPTH, 0);

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
					g_vmCommonParams.debug_info_queue->PushEmptyStorageFilter();

					UINT64 message_count = g_vmCommonParams.debug_info_queue->GetNumStoredMessages();

					for (UINT64 i = 0; i < message_count; i++) {
						SIZE_T message_size = 0;
						g_vmCommonParams.debug_info_queue->GetMessage(i, nullptr, &message_size); //get the size of the message

						D3D11_MESSAGE* message = (D3D11_MESSAGE*)malloc(message_size); //allocate enough space
						g_vmCommonParams.debug_info_queue->GetMessage(i, message, &message_size); //get the actual message

						//do whatever you want to do with it
						printf("Directx11: %.*s", message->DescriptionByteLength, message->pDescription);

						free(message);
					}
					g_vmCommonParams.debug_info_queue->ClearStoredMessages();
#endif
					cout << ("VR ERROR -- OUT") << endl;
					cout << ("screen : " + to_string(fb_size_cur.x) + " x " + to_string(fb_size_cur.y)) << endl;
					//cout << ("v_thickness : " + to_string(v_thickness)) <<endl;
					//cout << ("k_value : " + to_string(k_value)) <<endl;
					//cout << ("grid width and height : " + to_string(num_grid_x) + " x " + to_string(num_grid_y))<<endl;
		}


				int buf_row_pitch = mappedResSysRGBA.RowPitch / 4;
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
						// __PS_MERGE_LAYERS_TO_RENDEROUT 에서 INT -> FLOAT4 로 되어 배열된 color 요소가 들어 옴. //

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

		g_vmCommonParams.GpuProfile("Copyback", true);
	};

	if(0)
	{
		GpuRes gres_fb_rgba;
		grd_helper::UpdateFrameBuffer(gres_fb_rgba, iobj, "RENDER_OUT_RGBA_0", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
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
				res2d->ReleaseD2DRes();
				reGenRes = true;
			}
		}

		if (reGenRes) {
			// https://learn.microsoft.com/en-us/windows/win32/direct2d/direct2d-quickstart
			// https://learn.microsoft.com/en-us/windows/win32/direct2d/direct2d-and-direct3d-interoperation-overview
			IDXGISurface* pDxgiSurface = NULL;
			HRESULT hr = rtTex2D->QueryInterface(&pDxgiSurface);

			D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
					D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED), 0, 0);

			ID2D1RenderTarget* pRenderTarget = NULL;
			hr = g_pDirect2dFactory->CreateDxgiSurfaceRenderTarget(pDxgiSurface, &props, &pRenderTarget);

			ID2D1SolidColorBrush* pSolidBrush;
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
		res2d->pRenderTarget->BeginDraw();
		res2d->pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
		//res2d->pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

		res2d->pRenderTarget->DrawLine(
			D2D1::Point2F(100.f, 100.f),
			D2D1::Point2F(500.f, 500.f),
			res2d->pSolidBrush,
			0.5f, g_d2dStrokeStyleMap["DEFAULT"]
		);
		HRESULT hr = res2d->pRenderTarget->EndDraw();
	}

	if (is_system_out) {
		RenderOut();
	}

	g_dProgress = 100;

	if (g_vmCommonParams.gpu_profile)
	{
		g_vmCommonParams.dx11DeviceImmContext->End(g_vmCommonParams.dx11qr_disjoint);

		// Wait for data to be available
		while (g_vmCommonParams.dx11DeviceImmContext->GetData(g_vmCommonParams.dx11qr_disjoint, NULL, 0, 0) == S_FALSE)
		{
			Sleep(1);       // Wait a bit, but give other threads a chance to run
		}

		// Check whether timestamps were disjoint during the last frame
		D3D10_QUERY_DATA_TIMESTAMP_DISJOINT tsDisjoint;
		g_vmCommonParams.dx11DeviceImmContext->GetData(g_vmCommonParams.dx11qr_disjoint, &tsDisjoint, sizeof(tsDisjoint), 0);
		if (!tsDisjoint.Disjoint)
		{
			auto DisplayDuration = [&tsDisjoint](UINT64 tsS, UINT64 tsE, const string& _test)
			{
				if (tsS == 0 || tsE == 0) return;
				cout << _test << " : " << float(tsE - tsS) / float(tsDisjoint.Frequency) * 1000.0f << " ms" << endl;
			};

			for (auto& it : profile_map) {
				UINT64 ts, te;
				g_vmCommonParams.dx11DeviceImmContext->GetData(g_vmCommonParams.dx11qr_timestamps[it.second.x], &ts, sizeof(UINT64), 0);
				g_vmCommonParams.dx11DeviceImmContext->GetData(g_vmCommonParams.dx11qr_timestamps[it.second.y], &te, sizeof(UINT64), 0);

				DisplayDuration(ts, te, it.first);
			}

			//if (test_fps_profiling)
			//{
			//	auto it = profile_map.find("SR Render");
			//	UINT64 ts, te;
			//	dx11DeviceImmContext->GetData(dx11CommonParams->dx11qr_timestamps[it->second.x], &ts, sizeof(UINT64), 0);
			//	dx11DeviceImmContext->GetData(dx11CommonParams->dx11qr_timestamps[it->second.y], &te, sizeof(UINT64), 0);
			//	ofstream file_rendertime;
			//	file_rendertime.open(".\\data\\frames_profile_rendertime.txt", std::ios_base::app);
			//	file_rendertime << float(te - ts) / float(tsDisjoint.Frequency) * 1000.0f << endl;
			//	file_rendertime.close();
			//}
		}
	}


	return true;
}

void DeInitModule(fncontainer::VmFnContainer& _fncontainer)
{
	for (auto it = g_d2dResMap.begin(); it != g_d2dResMap.end(); it++) {
		it->second.ReleaseD2DRes();
	}
	g_d2dResMap.clear();
	for (auto it = g_d2dStrokeStyleMap.begin(); it != g_d2dStrokeStyleMap.end(); it++) {
		VMSAFE_RELEASE(it->second);
	}
	g_d2dStrokeStyleMap.clear();
	VMSAFE_RELEASE(g_pDirect2dFactory);


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
	if (g_pCGpuManager == NULL) // for module initialization
		g_pCGpuManager = new VmGpuManager(GpuSdkTypeDX11, __DLLNAME);
	_fncontainer.fnParams.SetParam("_string_CoreVersion", string(__VERSION));
	_fncontainer.fnParams.SetParam("_VmGpuManager*_", g_pCGpuManager);
}