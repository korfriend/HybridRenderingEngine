#include "vismtv_inbuilt_renderergpudx.h"
//#include "VXDX11Helper.h"

#include <d2d1_1.h>
#include <dwrite.h>
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

static CRITICAL_SECTION cs;
bool InitModule(fncontainer::VmFnContainer& _fncontainer)
{	
	InitializeCriticalSection(&cs);

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

		// IDWriteFactory는 DWriteCreateFactory 함수 호출 생성.
		DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(g_pDWriteFactory), reinterpret_cast<IUnknown**>(&g_pDWriteFactory));
		

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

		IDWriteTextFormat* pTextFormat = NULL;
		g_pDWriteFactory->CreateTextFormat(
			L"Comic Sans MS",                  // 폰트 패밀리 이름의 문자열
			NULL,                        // 폰트 컬렉션 객체, NULL=시스템 폰트 컬렉션
			DWRITE_FONT_WEIGHT_NORMAL,   // 폰트 굵기. LIGHT, NORMAL, BOLD 등.
			DWRITE_FONT_STYLE_NORMAL,    // 폰트 스타일. NORMAL, OBLIQUE, ITALIC.
			DWRITE_FONT_STRETCH_NORMAL,  // 폰트 간격. CONDENSED, NORMAL, MEDIUM, EXTEXDED 등.
			50.f,                          // 폰트 크기.
			L"",                         // 로케일을 문자열로 명시.  영어-미국=L"en-us", 한국어-한국=L"ko-kr"
			&pTextFormat
		);
		g_d2dTextFormatMap["DEFAULT"] = pTextFormat;

		if (hr != S_OK)
			vmlog::LogErr("D2D Device Setting Error!");
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

#ifdef DX11_3
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

	EnterCriticalSection(&cs);

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
	bool is_picking_routine = _fncontainer.fnParams.GetParam("_bool_IsPickingRoutine", false);
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
	
	bool is_vr = false;
	if (strRendererSource == "VOLUME")
	{
		double dRuntime = 0;
		RenderVrDLS(&_fncontainer, g_pCGpuManager, &g_vmCommonParams, &g_LocalProgress, &dRuntime);
		g_dRunTimeVRs += dRuntime;
		is_system_out = true;
		is_vr = true;
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
		is_system_out = true;
		is_vr = true;
	}
	else if (strRendererSource == "SECTIONAL_MESH")
	{
		double dRuntime = 0;
		RenderSrSlicer(&_fncontainer, g_pCGpuManager, &g_vmCommonParams, &g_LocalProgress, &dRuntime);
		g_dRunTimeVRs += dRuntime;
		if (is_final_renderer || planeThickness <= 0.f) is_system_out = true;
	}

	auto RenderOut = [&iobj, &is_final_renderer, &planeThickness, &_fncontainer, &is_vr]() {

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

		if (hWnd && is_final_renderer)
		{
			// APPLY HWND MODE
			ID3D11Texture2D* pTex2dHwndRT = NULL;
			ID3D11RenderTargetView* pHwndRTV = NULL;
			gpu_manager->UpdateDXGI((void**)&pTex2dHwndRT, (void**)&pHwndRTV, hWnd, fb_size_cur.x, fb_size_cur.y);

			dx11DeviceImmContext->CopyResource(pTex2dHwndRT, (ID3D11Texture2D*)gres_fb_rgba.alloc_res_ptrs[DTYPE_RES]);
		}
		else {
			if (hWnd) gpu_manager->ReleaseDXGI(hWnd);
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

	if (is_system_out && !is_picking_routine)
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

		vector<TextItem>* textItems = (vector<TextItem>*)_fncontainer.fnParams.GetParam("_vector<TextItem>*_TextItems", (void*)NULL);
		if (textItems) {
			// IDWriteTextFormat 객체 생성.
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
				
				res2d->pSolidBrush->SetColor(D2D1::ColorF(titem.iColor, titem.alpha)); // D2D1::ColorF::Black
				const D2D1_RECT_F rectangle1 = D2D1::RectF(titem.posScreenX, titem.posScreenY, rSize.width, rSize.height);

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
	grd_helper::DeinitializePresettings();
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