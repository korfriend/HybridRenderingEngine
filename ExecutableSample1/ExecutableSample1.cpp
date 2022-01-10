// ExecutableSample1.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "ExecutableSample1.h"

#include "../VisMtvApi.h"

#include <vector>
#include <windowsx.h>

// math using GLM
#include "../glm/gtc/matrix_transform.hpp"
#include "../glm/gtx/transform.hpp"
#include "../glm/gtc/constants.hpp"
#include "../glm/glm.hpp"
#include "../glm/gtc/type_ptr.hpp"
#include "../glm/gtx/vector_angle.hpp"

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
HWND hWnd;
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

auto transformPos = [](const glm::fvec3& pos, const glm::fmat4x4& m)
{
	glm::fvec4 pos4 = glm::fvec4(pos, 1);
	pos4 = m * pos4;
	pos4 /= pos4.w;
	return glm::fvec3(pos4);
};

auto transformVec = [](const glm::fvec3& vec, const glm::fmat4x4& m)
{
	glm::fvec4 vec4 = glm::fvec4(vec, 0);
	vec4 = m * vec4;
	return glm::fvec3(vec4);
};

std::string GetDataPath()
{
	using namespace std;
	char ownPth[2048];
	GetModuleFileNameA(NULL, ownPth, (sizeof(ownPth)));
	string exe_path = ownPth;
	string exe_path_;
	size_t pos = 0;
	std::string token;
	string delimiter = "\\";
	while ((pos = exe_path.find(delimiter)) != std::string::npos) {
		token = exe_path.substr(0, pos);
		if (token.find(".exe") != std::string::npos) break;
		exe_path += token + "\\";
		exe_path_ += token + "\\";
		exe_path.erase(0, pos + delimiter.length());
	}
	return exe_path_ + "..\\..\\data\\";
}


void EngineSetting()
{
	// loading model resources
	int loaded_vol2_id = 0;
	vzm::LoadModelFile(GetDataPath() + "result(dcm)\\FILE0.dcm", loaded_vol2_id, true);

	unsigned short** slices;
	int stride;
	int vol_size[3];
	float vox_pitch[3];
	vzm::GetVolumeInfo(loaded_vol2_id, (void***)&slices, vol_size, vox_pitch, &stride, NULL);
	glm::dmat4x4 dmat_vs2ws;
	vzm::GetObjectParam("_matrix_originalOS2WS", loaded_vol2_id, &dmat_vs2ws);
	glm::fvec3 dir_x = transformVec(glm::fvec3(1, 0, 0), dmat_vs2ws);
	glm::fvec3 dir_y = transformVec(glm::fvec3(0, 1, 0), dmat_vs2ws);
	glm::fvec3 dir_z = transformVec(glm::fvec3(0, 0, 1), dmat_vs2ws);
	bool is_rhs = glm::dot(glm::cross(dir_x, dir_y), dir_z) > 0;
	int loaded_vol_id = 0;
	vzm::GenerateVolumeFromData(loaded_vol_id, (const void**)slices, "USHORT", vol_size, vox_pitch,
		(const float*)glm::value_ptr(dir_x), (const float*)glm::value_ptr(dir_y), is_rhs, true);


	int loaded_mesh_id = 0;
	vzm::LoadModelFile(GetDataPath() + "stl\\PreparationScan_simple2.stl", loaded_mesh_id, true);

	int vr_tmap_id = 0;
	std::vector<glm::fvec2> alpha_ctrs;
	alpha_ctrs.push_back(glm::fvec2(0, 2050));
	alpha_ctrs.push_back(glm::fvec2(1.0, 5160));
	alpha_ctrs.push_back(glm::fvec2(1.0, 65536));
	alpha_ctrs.push_back(glm::fvec2(0, 65537));
	std::vector<glm::fvec4> rgb_ctrs;
	rgb_ctrs.push_back(glm::fvec4(1, 0, 0, 0));
	rgb_ctrs.push_back(glm::fvec4(1, 0.5, 0.5, 2300));
	rgb_ctrs.push_back(glm::fvec4(1, 1, 1, 3300));
	rgb_ctrs.push_back(glm::fvec4(1, 1, 1, 4000));
	rgb_ctrs.push_back(glm::fvec4(1, 1, 1, 65536));
	vzm::GenerateMappingTable(65537, alpha_ctrs.size(), (float*)&alpha_ctrs[0], rgb_ctrs.size(), (float*)&rgb_ctrs[0], vr_tmap_id);

	int mpr_tmap_id = 0;
	alpha_ctrs[0] = glm::fvec2(0, 550);
	alpha_ctrs[1] = glm::fvec2(1.0, 7000);
	rgb_ctrs.clear();
	rgb_ctrs.push_back(glm::fvec4(1, 1, 1, 0));
	rgb_ctrs.push_back(glm::fvec4(1, 1, 1, 65535));
	vzm::GenerateMappingTable(65537, alpha_ctrs.size(), (float*)&alpha_ctrs[0], rgb_ctrs.size(), (float*)&rgb_ctrs[0], mpr_tmap_id);

	vzm::ObjStates volume_state;
	volume_state.associated_obj_ids[vzm::ObjStates::VR_OTF] = vr_tmap_id;
	volume_state.associated_obj_ids[vzm::ObjStates::MPR_WINDOWING] = mpr_tmap_id;
	volume_state.is_visible = true; // see ObjStates

	// register objects in scene ID = 0
	vzm::ReplaceOrAddSceneObject(0, loaded_vol_id, volume_state);
	vzm::ObjStates obj_state;
	obj_state.color[3] = 0.7f; // control for transparency
	vzm::ReplaceOrAddSceneObject(0, loaded_mesh_id, obj_state);

	int iso_mesh_id = 0;
	vzm::GenerateIsoSurfaceObject(loaded_vol_id, 2500, 4, 0, 0, NULL, iso_mesh_id);

	*(glm::fvec4*)obj_state.color = glm::fvec4(0, 0.5, 1, 0.3);
	vzm::ReplaceOrAddSceneObject(0, iso_mesh_id, obj_state);
	//vzm::SetRenderTestParam("_bool_GpuProfile", true, sizeof(bool), -1, -1);
	//vzm::SetRenderTestParam("_double_UserSampleRate", 1.0, sizeof(double), -1, -1);
	//vzm::SetRenderTestParam("_bool_ApplySampleRateToGradient", false, sizeof(bool), -1, -1);
	//vzm::SetRenderTestParam("_int_RendererType", (int)3, sizeof(int), 0, 0, loaded_vol_id);
	std::map<std::string, std::any> test_parameters;
	test_parameters["test"] = (int)4747;
	//vzm::ExecuteModule2("test_module", "ExtractMeshViaMC",
	//	std::list<int>{ loaded_vol_id, iso_mesh_id }, test_parameters);

	RECT rc;
	GetClientRect(hWnd, &rc);
	int w = rc.right - rc.left;
	int h = rc.bottom - rc.top;

	vzm::CameraParameters vr_cam_params;
	*(glm::fvec3*)vr_cam_params.pos = glm::fvec3(0, -300, 0);
	*(glm::fvec3*)vr_cam_params.up = glm::fvec3(0, 0, 1);
	*(glm::fvec3*)vr_cam_params.view = glm::fvec3(0, 1, 0);
	vr_cam_params.np = 0.10f;
	vr_cam_params.fp = 1000.f;
	vr_cam_params.fov_y = 3.141592654f / 4.f;
	vr_cam_params.projection_mode = 2;
	vr_cam_params.w = w;
	vr_cam_params.h = h;
	vr_cam_params.aspect_ratio = (float)vr_cam_params.w / (float)vr_cam_params.h;

	// mpr cam (image slicer) setting
	vzm::CameraParameters mpr_cam_params;
	*(glm::fvec3*)mpr_cam_params.pos = glm::fvec3(0, 0, 0);
	*(glm::fvec3*)mpr_cam_params.up = glm::fvec3(0, 0, 1);
	*(glm::fvec3*)mpr_cam_params.view = glm::fvec3(0, 1, 0);
	mpr_cam_params.np = 0;
	mpr_cam_params.fp = 10.f;
	mpr_cam_params.ip_w = 300;
	mpr_cam_params.ip_h = vr_cam_params.ip_w * (float)h / (float)w;
	mpr_cam_params.projection_mode = 4;
	mpr_cam_params.w = w;
	mpr_cam_params.h = h;

	vzm::SceneEnvParameters scn_env_params;
	scn_env_params.is_on_camera = true;
	scn_env_params.is_pointlight = true;
	*(glm::fvec3*)scn_env_params.pos_light = *(glm::fvec3*)vr_cam_params.pos;
	*(glm::fvec3*)scn_env_params.dir_light = *(glm::fvec3*)vr_cam_params.view;

	vzm::SetSceneEnvParameters(0, scn_env_params);
	vzm::SetCameraParameters(0, vr_cam_params, 0);
	vzm::SetCameraParameters(0, mpr_cam_params, 1);

	vzm::ortho_box_transform boxTr;
	*(glm::fvec3*)boxTr.pos_minbox_ws = glm::dvec3(-20);
	*(glm::fvec3*)boxTr.pos_maxbox_ws = glm::dvec3(20);
	*(glm::fvec3*)boxTr.dir_y = glm::dvec3(0, 1, 0);
	*(glm::fvec3*)boxTr.dir_z = glm::dvec3(0, 0, 1);

	glm::fmat4x4 matClipWS2BS;
	boxTr.ComputeBoxTransformMatrix((float*)&matClipWS2BS);

	//glm::dmat4x4 dmatClipWS2BS = matClipWS2BS;
	//glm::dvec3 dposOrthoMaxWS = *(glm::fvec3*)boxTr.pos_maxbox_ws;

	// for mpr
	//vzm::SetRenderTestParam3("_double_PlaneThickness", (double)30.0, 0, 0, -1);
	// default: 100, ray_sum: 110, mip: 111, 
	//vzm::SetRenderTestParam3("_int_RendererType", (int)111, 0, 0, loaded_vol_id);

	// for vr, clipping
	vzm::SetRenderTestParam3("_int_ClippingMode", (int)2, 0, 0, loaded_vol_id);
	vzm::SetRenderTestParam3("_matrix44_MatrixClipWS2BS", glm::dmat4x4(matClipWS2BS), 0, 0, loaded_vol_id);
	vzm::SetRenderTestParam3("_double3_PosClipBoxMaxWS", glm::dvec3(*(glm::fvec3*)boxTr.pos_maxbox_ws), 0, 0, loaded_vol_id);
	//
	//vzm::SetRenderTestParam3("_int_ClippingMode", (int)2, 0, 0, loaded_mesh_id);
	//vzm::SetRenderTestParam3("_matrix44_MatrixClipWS2BS", glm::dmat4x4(matClipWS2BS), 0, 0, loaded_mesh_id);
	//vzm::SetRenderTestParam3("_double3_PosClipBoxMaxWS", glm::dvec3(*(glm::fvec3*)boxTr.pos_maxbox_ws), 0, 0, loaded_mesh_id);


	//vzm::SetRenderTestParam("_bool_UseSpinLock", false, sizeof(bool), -1, -1);
	//vzm::SetRenderTestParam("_double_VZThickness", 0.0, sizeof(double), -1, -1);
	//vzm::SetRenderTestParam("_double_MergingBeta", 0.5, sizeof(double), -1, -1);
	//vzm::SetRenderTestParam("_double_RobustRatio", 0.5, sizeof(double), -1, -1);
	//vzm::SetRenderTestParam("_double4_ShadingFactorsForGlobalPrimitives", glm::dvec4(0.4, 0.6, 0.2, 30.0), sizeof(glm::dvec4), -1, -1);

}

void UpdateRenderingResult()
{
	unsigned char* ptr_rgba;
	float* ptr_zdepth;
	int w, h;
	if (vzm::GetRenderBufferPtrs(0, &ptr_rgba, &ptr_zdepth, &w, &h, 0))
	{
		// https://stackoverflow.com/questions/26005744/how-to-display-pixels-on-screen-directly-from-a-raw-array-of-rgb-values-faster-t
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		// Temp HDC to copy picture
		HDC src = CreateCompatibleDC(hdc); // hdc - Device context for window, I've got earlier with GetDC(hWnd) or GetDC(NULL);
		// Creating temp bitmap
		HBITMAP map = CreateBitmap(w, h, 1, 8 * 4, (void*)ptr_rgba); // pointer to array
		SelectObject(src, map); // Inserting picture into our temp HDC
		// Copy image from temp HDC to window
		BitBlt(hdc, // Destination
			0,  // x and
			0,  // y - upper-left corner of place, where we'd like to copy
			w, // width of the region
			h, // height
			src, // source
			0,   // x and
			0,   // y of upper left corner  of part of the source, from where we'd like to copy
			SRCCOPY); // Defined DWORD to juct copy pixels. Watch more on msdn;

		DeleteObject(map);
		DeleteDC(src); // Deleting temp HDC
		EndPaint(hWnd, &ps);
	}
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// TODO: Place code here.
	// engine initialization to use the core APIs of framework
	// must be paired with DeinitEngineLib()
	vzm::InitEngineLib();

	// Initialize global strings
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_EXECUTABLESAMPLE1, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	EngineSetting();
	vzm::RenderScene(0, 0);
	UpdateRenderingResult();

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_EXECUTABLESAMPLE1));

	MSG msg;

	// Main message loop:
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	vzm::DeinitEngineLib();

	return (int)msg.wParam;
}

int main()
{
	return wWinMain(GetModuleHandle(NULL), NULL, GetCommandLine(), SW_SHOWNORMAL);
}


//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDC_EXECUTABLESAMPLE1));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_EXECUTABLESAMPLE1);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		return FALSE;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
#include <mutex>
std::mutex the_mutex;
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	vzm::CameraParameters cam_params;
	vzm::GetCameraParameters(0, cam_params, 0);

	static glm::fvec3 scene_stage_center = glm::fvec3();
	static float scene_stage_scale = 150.f;
	static helpers::arcball aball_vr;

	switch (message)
	{
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
	break;
	case WM_PAINT:
	{
		std::lock_guard<std::mutex> guard(the_mutex);
		//PAINTSTRUCT ps;
		//HDC hdc = BeginPaint(hWnd, &ps);
		//// TODO: Add any drawing code that uses hdc here...
		//EndPaint(hWnd, &ps);
		UpdateRenderingResult();
	}
	break;
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	{
		int x = GET_X_LPARAM(lParam);
		int y = GET_Y_LPARAM(lParam);
		aball_vr.intializer((float*)&scene_stage_center, scene_stage_scale);

		helpers::cam_pose arc_cam_pose;
		glm::fvec3 pos = *(glm::fvec3*)arc_cam_pose.pos = *(glm::fvec3*)cam_params.pos;
		*(glm::fvec3*)arc_cam_pose.up = *(glm::fvec3*)cam_params.up;
		*(glm::fvec3*)arc_cam_pose.view = *(glm::fvec3*)cam_params.view;
		glm::ivec2 pos_ss = glm::ivec2(x, y);
		glm::fvec2 screen_size = glm::fvec2(cam_params.w / 1, cam_params.h / 1);
		aball_vr.start((int*)&pos_ss, (float*)&screen_size, arc_cam_pose);
	}
	break;
	case WM_MOUSEMOVE:
	{
		int x = GET_X_LPARAM(lParam);
		int y = GET_Y_LPARAM(lParam);

		if ((wParam & MK_LBUTTON) || (wParam & MK_RBUTTON))
		{
			helpers::cam_pose arc_cam_pose;
			glm::ivec2 pos_ss = glm::ivec2(x, y);
			if (wParam & MK_LBUTTON)
				aball_vr.pan_move((int*)&pos_ss, arc_cam_pose);
			else if (wParam & MK_RBUTTON)
				aball_vr.move((int*)&pos_ss, arc_cam_pose);

			*(glm::fvec3*)cam_params.pos = *(glm::fvec3*)arc_cam_pose.pos;
			*(glm::fvec3*)cam_params.up = *(glm::fvec3*)arc_cam_pose.up;
			*(glm::fvec3*)cam_params.view = *(glm::fvec3*)arc_cam_pose.view;

			vzm::SetCameraParameters(0, cam_params, 0);

			vzm::RenderScene(0, 0);

			InvalidateRect(hWnd, NULL, FALSE);
			UpdateWindow(hWnd);
		}

	}
	break;
	case WM_MOUSEWHEEL:
	{
		int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
		if (zDelta > 0)
			*(glm::fvec3*)cam_params.pos += scene_stage_scale * 0.01f * (*(glm::fvec3*)cam_params.view);
		else
			*(glm::fvec3*)cam_params.pos -= scene_stage_scale * 0.01f * (*(glm::fvec3*)cam_params.view);
		vzm::SetCameraParameters(0, cam_params, 0);
		vzm::RenderScene(0, 0);

		InvalidateRect(hWnd, NULL, FALSE);
		UpdateWindow(hWnd);
	}
	break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
