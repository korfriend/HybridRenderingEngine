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
auto show_window = [](const std::string& title, const int scene_id, const int cam_id)
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
		aball_vr.intializer((float*)&glm::fvec3(), 500);

		helpers::cam_pose arc_cam_pose;
		glm::fvec3 pos = __cv3__ arc_cam_pose.pos = __cv3__ cam_params.pos;
		__cv3__ arc_cam_pose.up = __cv3__ cam_params.up;
		__cv3__ arc_cam_pose.view = __cv3__ cam_params.view;
		aball_vr.start((int*)&glm::ivec2(x, y), (float*)&glm::fvec2(cam_params.w / 2, cam_params.h / 2), arc_cam_pose);
	}
	else if (event == EVENT_MBUTTONDOWN)
	{
	}
	else if (event == EVENT_MOUSEHWHEEL)
	{
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

int main()
{
#if (_MSVC_LANG < 201703L)
	# this program uses C++ standard ver 17
#endif

	vzm::InitEngineLib();

	int loaded_obj_id = 0;

	vzm::LoadModelFile(".\\data\\Bunny70k.ply", loaded_obj_id, true);

	
	// test rendering example
#define __cv3__ *(glm::fvec3*)
#define __cv4__ *(glm::fvec4*)
#define __cm4__ *(glm::fmat4x4*)

	auto align_obj_to_world_center = [](int scene_id, int obj_id)
	{
		glm::fvec3 pos_min, pos_max;
		vzm::GetSceneBoundingBox({ obj_id }, scene_id, (float*)&pos_min, (float*)&pos_max);
		vzm::ObjStates obj_state;
		vzm::GetSceneObjectState(scene_id, obj_id, obj_state);
		__cm4__ obj_state.os2ws = glm::translate(-(pos_min + pos_max) * 0.5f);
		vzm::ReplaceOrAddSceneObject(scene_id, obj_id, obj_state);
	};

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

	align_obj_to_world_center(0, loaded_obj_id);

	int key = -1;
	while (key != 'q')
	{
		for (auto& it : scene_name)
		{
			show_window(it.second, it.first, 0);
		}
		key = cv::waitKey(1);
	}

	vzm::DeinitEngineLib();
	return 0;
}