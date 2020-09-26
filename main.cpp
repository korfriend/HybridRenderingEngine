#include <string>
#include <iostream>
#include <chrono>
#include <thread>

#include "VisMtvApi.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

#define __cv3__ *(glm::fvec3*)
#define __cv4__ *(glm::fvec4*)
#define __cm4__ *(glm::fmat4x4*)

static int scene_id_cnt = 0;
static std::map<int, std::string> scene_name;
static float scene_stage_scale = 300;
static glm::fvec3 scene_stage_center = glm::fvec3();
auto show_window = [](const std::string& title, const int scene_id, const int cam_id, const bool write_img_file)
{
	vzm::RenderScene(scene_id, cam_id);
	unsigned char* ptr_rgba;
	float* ptr_zdepth;
	int w, h;
	if (vzm::GetRenderBufferPtrs(scene_id, &ptr_rgba, &ptr_zdepth, &w, &h, cam_id))
	{
		cv::Mat cvmat(h, w, CV_8UC4, ptr_rgba);
		//show the image
		cv::imshow(title, cvmat);

		if (write_img_file) cv::imwrite("testout" + std::to_string(scene_id) + ".png", cvmat);
	}

};

void CallBackFunc_Mouse(int event, int x, int y, int flags, void* userdata)
{
	using namespace cv;

	static int x_old = x;
	static int y_old = y;

	//if (x_old == x && y_old == y) return;
	//x_old = x;
	//y_old = y;

	vzm::CameraParameters cam_params;
	vzm::GetCameraParameters(0, cam_params, 0);

	static helpers::arcball aball_vr;
	if (event == EVENT_LBUTTONDOWN || event == EVENT_RBUTTONDOWN)
	{
		aball_vr.intializer((float*)&scene_stage_center, scene_stage_scale);

		helpers::cam_pose arc_cam_pose;
		glm::fvec3 pos = __cv3__ arc_cam_pose.pos = __cv3__ cam_params.pos;
		__cv3__ arc_cam_pose.up = __cv3__ cam_params.up;
		__cv3__ arc_cam_pose.view = __cv3__ cam_params.view;
		aball_vr.start((int*)&glm::ivec2(x, y), (float*)&glm::fvec2(cam_params.w / 2, cam_params.h / 2), arc_cam_pose);
	}
	else if (event == EVENT_MBUTTONDOWN)
	{
	}
	else if (event == EVENT_MOUSEWHEEL && (flags & EVENT_FLAG_CTRLKEY))
	{
		if (getMouseWheelDelta(flags) > 0)
			__cv3__ cam_params.pos += scene_stage_scale * 0.05f * (__cv3__ cam_params.view);
		else
			__cv3__ cam_params.pos -= scene_stage_scale * 0.05f * (__cv3__ cam_params.view);
		vzm::SetCameraParameters(0, cam_params, 0);
	}
	else if (event == EVENT_MOUSEMOVE)
	{
		if ((flags & EVENT_FLAG_LBUTTON) || (flags & EVENT_FLAG_RBUTTON))
		{
			helpers::cam_pose arc_cam_pose;
			if (flags & EVENT_FLAG_LBUTTON)
				aball_vr.pan_move((int*)&glm::ivec2(x, y), arc_cam_pose);
			else if (flags & EVENT_FLAG_RBUTTON)
				aball_vr.move((int*)&glm::ivec2(x, y), arc_cam_pose);

			__cv3__ cam_params.pos = __cv3__ arc_cam_pose.pos;
			__cv3__ cam_params.up = __cv3__ arc_cam_pose.up;
			__cv3__ cam_params.view = __cv3__ arc_cam_pose.view;

			for (auto& it : scene_name)
			{
				vzm::SetCameraParameters(it.first, cam_params);
			}
		}
	}
}

// test rendering example
#define __cv3__ *(glm::fvec3*)
#define __cv4__ *(glm::fvec4*)
#define __cm4__ *(glm::fmat4x4*)
#define __PR(A) A[0] << ", " << A[1] << ", " << A[2]

auto align_obj_to_world_center = [](int scene_id, const std::list<int>& obj_ids)
{
	glm::fvec3 pos_min, pos_max;
	vzm::GetSceneBoundingBox(obj_ids, scene_id, (float*)&pos_min, (float*)&pos_max);
	for (auto& obj_id : obj_ids)
	{
		vzm::ObjStates obj_state;
		vzm::GetSceneObjectState(scene_id, obj_id, obj_state);
		__cm4__ obj_state.os2ws = glm::translate(-(pos_min + pos_max) * 0.5f);
		vzm::ReplaceOrAddSceneObject(scene_id, obj_id, obj_state);
	}
};

#if (_MSVC_LANG < 201703L)
# this program uses C++ standard ver 17
#endif

int main_test()
{
	vzm::InitEngineLib();

	int loaded_obj_id = 0;

	vzm::LoadModelFile(".\\data\\Bunny70k.ply", loaded_obj_id, true);

	vzm::CameraParameters cam_params;
	__cv3__ cam_params.pos = glm::fvec3(0, 0, 300);
	__cv3__ cam_params.up = glm::fvec3(0, 1.f, 0);
	__cv3__ cam_params.view = glm::fvec3(0, 0, -1.f);
	cam_params.fov_y = 3.141592654f / 4.f;
	cam_params.aspect_ratio = 640.f / 480.f;
	cam_params.projection_mode = 2;
	cam_params.w = 1280;
	cam_params.h = 1024;

	vzm::SceneEnvParameters scn_env_params;
	scn_env_params.is_on_camera = true;
	scn_env_params.is_pointlight = true;
	__cv3__ scn_env_params.pos_light = __cv3__ cam_params.pos;
	__cv3__ scn_env_params.dir_light = __cv3__ cam_params.view;

	vzm::ObjStates obj_state;
	obj_state.emission = 0.3f;
	obj_state.diffusion = 0.5f;
	obj_state.specular = 0.2f;
	obj_state.sp_pow = 30.f;
	obj_state.point_thickness = 20.f; // control for visible point size
	__cv4__ obj_state.color = glm::fvec4(0.8f, 1.f, 1.f, 1.f); 
	obj_state.color[3] = 0.9f; // control for transparency
	__cm4__ obj_state.os2ws = glm::fmat4x4(); // identity

	scene_name[0] = "3d view";

	std::map<std::string, std::list<int>> prim_ids = { 
		{scene_name[0], {loaded_obj_id}} ,
		//{scene_name[1], {id1, id1, ...}} ,
	};

	for (int i = 0; i < (int)scene_name.size(); i++)
	{
		// (int)i is scene_id
		vzm::SetSceneEnvParameters(i, scn_env_params);
		vzm::SetCameraParameters(i, cam_params, 0);

		auto it = prim_ids.find(scene_name[i]);
		for (int& obj_id : it->second)
			vzm::ReplaceOrAddSceneObject(i, obj_id, obj_state);

		cv::namedWindow(scene_name[i], cv::WINDOW_AUTOSIZE);
		cv::setMouseCallback(scene_name[i], CallBackFunc_Mouse, NULL);// &scenes[i]);
		//show_window(scene_name[i], i, 0);
	}

	align_obj_to_world_center(0, { loaded_obj_id });

	int key = -1;
	while (key != 'q')
	{
		for (auto& it : scene_name)
		{
			show_window(it.second, it.first, 0, false);
		}
		key = cv::waitKey(1);
	}

	vzm::DeinitEngineLib();
	return 0;
}

int main()
{
	// OIT test 1
	vzm::InitEngineLib();

	std::list<int> loaded_obj_ids;

	vzm::CameraParameters cam_params;
	vzm::ObjStates obj_state;
#define __OBJ1
#ifdef __OBJ1
	vzm::LoadMultipleModelsFile(".\\data\\sportsCar.obj", loaded_obj_ids, true);
	scene_stage_scale = 5.f;
	__cv3__ cam_params.pos = glm::fvec3(0, 0, 5.f);
	__cv3__ cam_params.up = glm::fvec3(0, 1.f, 0);
	__cv3__ cam_params.view = glm::fvec3(0, 0, -1.f);
	// obj file includes material info, which is prior shading option for rendering; therefore, wildcard setting is required to change shading.
	glm::dvec4 shading_param(0.5, 1.5, 1.5, 100.0);
	vzm::DebugTestSet("_double4_ShadingFactorsForGlobalPrimitives", &shading_param, sizeof(glm::dvec4), 0, 0);

#elif defined(__OBJ2)
	vzm::LoadMultipleModelsFile(".\\data\\hairball_colored.ply", loaded_obj_ids, true);
	__cv3__ cam_params.pos = glm::fvec3(0, 0, 300);
	__cv3__ cam_params.up = glm::fvec3(0, 1.f, 0);
	__cv3__ cam_params.view = glm::fvec3(0, 0, -1.f);
	obj_state.emission = 0.3f;
	obj_state.diffusion = 0.5f;
	obj_state.specular = 0.2f;
	obj_state.sp_pow = 30.f;
#elif defined(__OBJ3)
	vzm::LoadMultipleModelsFile(".\\data\\densepoints_floor.ply", loaded_obj_id, true);
	__cv3__ cam_params.pos = glm::fvec3(0, 0, 300);
	__cv3__ cam_params.up = glm::fvec3(0, 1.f, 0);
	__cv3__ cam_params.view = glm::fvec3(0, 0, -1.f);
	obj_state.emission = 0.3f;
	obj_state.diffusion = 0.5f;
	obj_state.specular = 0.2f;
	obj_state.sp_pow = 30.f;
	obj_state.point_thickness = 20.f; // control for visible point size
#endif

	cam_params.fov_y = 3.141592654f / 4.f;
	cam_params.projection_mode = 2;
	cam_params.w = 1024;
	cam_params.h = 1024;
	cam_params.aspect_ratio = (float)cam_params.w / (float)cam_params.h;

	vzm::SceneEnvParameters scn_env_params;
	scn_env_params.is_on_camera = true;
	scn_env_params.is_pointlight = true;
	__cv3__ scn_env_params.pos_light = __cv3__ cam_params.pos;
	__cv3__ scn_env_params.dir_light = __cv3__ cam_params.view;

	__cv4__ obj_state.color = glm::fvec4(0.8f, 1.f, 1.f, 1.f);
	obj_state.color[3] = 0.9f; // control for transparency
	__cm4__ obj_state.os2ws = glm::fmat4x4(); // identity

	scene_name[0] = "OIT";

	std::map<std::string, std::list<int>> prim_ids = {
		{scene_name[0], loaded_obj_ids} ,
		//{scene_name[1], {id1, id1, ...}} ,
	};

	for (int i = 0; i < (int)scene_name.size(); i++)
	{
		// (int)i is scene_id
		vzm::SetSceneEnvParameters(i, scn_env_params);
		vzm::SetCameraParameters(i, cam_params, 0);

		auto it = prim_ids.find(scene_name[i]);
		for (int& obj_id : it->second)
			vzm::ReplaceOrAddSceneObject(i, obj_id, obj_state);

		cv::namedWindow(scene_name[i], cv::WINDOW_AUTOSIZE);
		cv::setMouseCallback(scene_name[i], CallBackFunc_Mouse, NULL);// &scenes[i]);
	}

	align_obj_to_world_center(0, loaded_obj_ids);

	int key = -1;
	while (key != 'q')
	{
		bool write_img_file = false;
		bool reload_hlsl_objs = false;
		switch (key)
		{
		case 'c':
		{
			// show cam states
			vzm::CameraParameters cam_params;
			vzm::GetCameraParameters(0, cam_params, 0);
			std::cout << "cam pos  : " << __PR(cam_params.pos) << std::endl;;
			std::cout << "up vec   : " << __PR(cam_params.up) << std::endl;;
			std::cout << "view vec : " << __PR(cam_params.view) << std::endl;;
			break;
		}
		case 'g':
		{
			// (de)activate GPU profiling
			static bool gpu_profile = false;
			gpu_profile = !gpu_profile;
			vzm::DebugTestSet("_bool_PrintOutRoutineObjs", &gpu_profile, sizeof(bool), 0, 0);
			vzm::DebugTestSet("_bool_GpuProfile", &gpu_profile, sizeof(bool), 0, 0);
			std::cout << "gpu profiling : " << (gpu_profile? "ON" : "OFF") << std::endl;
			break;
		}
		case '[':
		{
			vzm::ObjStates _obj_state;
			vzm::GetSceneObjectState(0, 0, _obj_state);
			if (_obj_state.color[3] > 0.1f) _obj_state.color[3] -= 0.1f;
			else _obj_state.color[3] -= 0.01f;
			_obj_state.color[3] = std::max(0.01f, _obj_state.color[3]);
			std::cout << "alpha : " << _obj_state.color[3] << std::endl;;
			vzm::ReplaceOrAddSceneObject(0, 0, _obj_state);
			break;
		}
		case ']':
		{
			vzm::ObjStates _obj_state;
			vzm::GetSceneObjectState(0, 0, _obj_state);
			if (_obj_state.color[3] >= 0.1f) _obj_state.color[3] += 0.1f;
			else _obj_state.color[3] += 0.01f;
			_obj_state.color[3] = std::min(1.f, _obj_state.color[3]);
			std::cout << "alpha : " << _obj_state.color[3] << std::endl;;
			vzm::ReplaceOrAddSceneObject(0, 0, _obj_state);
			break;
		}
		case 'w':
		{
			write_img_file = true;
			break;
		}
		case 'r':
		{
			reload_hlsl_objs = true;
			vzm::DebugTestSet("_bool_ReloadHLSLObjFiles", &reload_hlsl_objs, sizeof(bool), 0, 0);
			break;
		}
		}

		for (auto& it : scene_name)
		{
			show_window(it.second, it.first, 0, write_img_file);
		}
		if (reload_hlsl_objs)
		{
			reload_hlsl_objs = false;
			vzm::DebugTestSet("_bool_ReloadHLSLObjFiles", &reload_hlsl_objs, sizeof(bool), 0, 0);
		}
		key = cv::waitKey(1);
	}

	vzm::DeinitEngineLib();
	return 0;
}