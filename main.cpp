#include <string>
#include <iostream>
#include <chrono>
#include <thread>
#include <fstream>
#include <sstream>
#include <mutex>

#include "Windows.h"

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
void show_window(const std::string& title, const int scene_id, const int cam_id, const bool write_img_file, const std::string& preset_file)
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

		if (write_img_file)
		{
			size_t lastindex = preset_file.find_last_of(".");
			std::string rawname = preset_file.substr(0, lastindex);
			cv::imwrite(rawname + "_render.png", cvmat);
		}
	}

}

std::mutex the_mutex;
void CallBackFunc_Mouse(int event, int x, int y, int flags, void* userdata)
{
	std::lock_guard<std::mutex> guard(the_mutex);
	using namespace cv;

	static int x_old = x;
	static int y_old = y;

	//if (x_old == x && y_old == y) return;
	//x_old = x;
	//y_old = y;

	if (flags & EVENT_FLAG_ALTKEY && event == EVENT_LBUTTONDOWN)
	{
		int oit_mode = 0;
		vzm::GetRenderTestParam("_int_OitMode", &oit_mode, sizeof(int), -1, -1);

		vzm::SetRenderTestParam("_double_TrInvterval", 0.002, sizeof(double), -1, -1);
		vzm::SetRenderTestParam("_double_TrStartOffset", 2.0, sizeof(double), -1, -1);
		vzm::SetRenderTestParam("_int2_PixelPos", glm::ivec2(x, y), sizeof(int) * 2, -1, -1);
		vzm::SetRenderTestParam("_bool_PixelTransmittance", true, sizeof(bool), -1, -1);
		vzm::SetRenderTestParam("_int_OitMode", (int)1, sizeof(int), -1, -1);
		vzm::RenderScene(0, 0);
		vzm::SetRenderTestParam("_int_OitMode", (int)0, sizeof(int), -1, -1);
		vzm::RenderScene(0, 0);
		vzm::SetRenderTestParam("_int_OitMode", (int)2, sizeof(int), -1, -1);
		vzm::RenderScene(0, 0);
		vzm::SetRenderTestParam("_int_OitMode", (int)4, sizeof(int), -1, -1);
		vzm::SetRenderTestParam("_double_RobustRatio", 0.2, sizeof(double), -1, -1);
		vzm::RenderScene(0, 0);

		vzm::SetRenderTestParam("_bool_PixelTransmittance", false, sizeof(bool), -1, -1);
		vzm::SetRenderTestParam("_int_OitMode", oit_mode, sizeof(int), -1, -1);
		return;
	}

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
			__cv3__ cam_params.pos += scene_stage_scale * 0.01f * (__cv3__ cam_params.view);
		else
			__cv3__ cam_params.pos -= scene_stage_scale * 0.01f * (__cv3__ cam_params.view);
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
#define __PR(A, INTERVAL) A[0] << INTERVAL << A[1] << INTERVAL << A[2]

void align_obj_to_world_center(int scene_id, const std::list<int>& obj_ids)
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
}

void load_preset(const std::string& preset_file, const std::list<int>& obj_ids)
{
	using namespace std;
	
	ifstream filein(preset_file); //File to read from
	if (!filein.is_open()) return;

	vzm::CameraParameters cam_params;
	vzm::GetCameraParameters(0, cam_params, 0);
	int cam_loaded = 0;
	string line;
	while (getline(filein, line))
	{
		istringstream iss(line);

		string param_name;

		iss >> param_name;

#define SET_CAM(A) A[0] >> A[1] >> A[2]

		if (param_name == "cam_pos")
		{
			cam_loaded++;
			iss >> SET_CAM(cam_params.pos);
		}
		else if (param_name == "cam_up")
		{
			cam_loaded++;
			iss >> SET_CAM(cam_params.up);
		}
		else if (param_name == "cam_view")
		{
			cam_loaded++;
			iss >> SET_CAM(cam_params.view);
		}
		else if (param_name == "z_thickenss_sp")
		{
			double v;
			iss >> v;
			vzm::SetRenderTestParam("_double_CopVZThickness", v, sizeof(double), -1, -1);
		}
		else if (param_name == "z_thickenss_rp")
		{
			double v;
			iss >> v;
			vzm::SetRenderTestParam("_double_VZThickness", v, sizeof(double), -1, -1);
		}
		else if (param_name == "beta")
		{
			double v;
			iss >> v;
			vzm::SetRenderTestParam("_double_MergingBeta", v, sizeof(double), -1, -1);
		}
		else if (param_name == "robustness_ratio")
		{
			double v;
			iss >> v;
			vzm::SetRenderTestParam("_double_RobustRatio", v, sizeof(double), -1, -1);
		}
		else if (param_name == "forced_shading")
		{
			glm::dvec4 v;
			iss >> v.x >> v.y >> v.z >> v.w;
			vzm::SetRenderTestParam("_double4_ShadingFactorsForGlobalPrimitives", v, sizeof(glm::dvec4), -1, -1);
		}
		else if (param_name == "alpha" && obj_ids.size() > 0)
		{
			double v;
			iss >> v;
			vzm::ObjStates _obj_state;
			if (vzm::GetSceneObjectState(0, *obj_ids.begin(), _obj_state))
			{
				_obj_state.color[3] = (float)v;
				for (const int& obj_id : obj_ids)
					vzm::ReplaceOrAddSceneObject(0, obj_id, _obj_state);
			}
		}
		else if (param_name == "surfel_size" && obj_ids.size() > 0)
		{
			double v;
			iss >> v;
			vzm::ObjStates _obj_state;
			if (vzm::GetSceneObjectState(0, *obj_ids.begin(), _obj_state))
			{
				_obj_state.surfel_size = (float)v;
				for (const int& obj_id : obj_ids)
					vzm::ReplaceOrAddSceneObject(0, obj_id, _obj_state);
			}
		}
	}
	if (cam_loaded == 3)
		vzm::SetCameraParameters(0, cam_params, 0);

	filein.close();
}

void store_preset(const std::string& preset_file, const std::list<int>& obj_ids)
{
	using namespace std;
	ofstream fileout(preset_file);
	if (!fileout.is_open()) return;

	fileout.clear();

	vzm::CameraParameters cam_params;
	vzm::GetCameraParameters(0, cam_params, 0);
	fileout << "cam_pos " << __PR(cam_params.pos, " ") << endl;
	fileout << "cam_up " << __PR(cam_params.up, " ") << endl;
	fileout << "cam_view " << __PR(cam_params.view, " ") << endl;

	double z_thickenss_sp, z_thickenss_rp, beta, Rh;
	glm::dvec4 shading_param;

	if (vzm::GetRenderTestParam("_double_CopVZThickness", &z_thickenss_sp, sizeof(double), 0, 0))
		fileout << "z_thickenss_sp " << z_thickenss_sp << endl;
	if (vzm::GetRenderTestParam("_double_VZThickness", &z_thickenss_rp, sizeof(double), 0, 0))
		fileout << "z_thickenss_rp " << z_thickenss_rp << endl;
	if (vzm::GetRenderTestParam("_double_MergingBeta", &beta, sizeof(double), 0, 0))
		fileout << "beta " << beta << endl;
	if (vzm::GetRenderTestParam("_double_RobustRatio", &Rh, sizeof(double), 0, 0))
		fileout << "robustness_ratio " << Rh << endl;
	if (vzm::GetRenderTestParam("_double4_ShadingFactorsForGlobalPrimitives", &shading_param, sizeof(glm::dvec4), 0, 0))
		fileout << "forced_shading " << __PR(((double*)&shading_param), " ") << " " << shading_param.w << endl;

	vzm::ObjStates _obj_state;
	if (vzm::GetSceneObjectState(0, *obj_ids.begin(), _obj_state))
	{
		fileout << "alpha " << _obj_state.color[3] << endl;
		fileout << "surfel_size " << _obj_state.surfel_size << endl;
	}

	fileout.close();
}

double high_Rh = 0.8, low_Rh = 0.2, diff_amp = 10.0;
void compute_difference(const std::string& out_file_prefix)
{
	auto make_cvmat = [](int oit_mode, double rh = 0.5) -> cv::Mat
	{
		int ot_mode_original = 0;
		vzm::GetRenderTestParam("_int_OitMode", &ot_mode_original, sizeof(int), -1, -1);
		double rh_original;
		vzm::GetRenderTestParam("_double_RobustRatio", &rh_original, sizeof(double), -1, -1);

		vzm::SetRenderTestParam("_int_OitMode", oit_mode, sizeof(int), -1, -1);
		vzm::SetRenderTestParam("_double_RobustRatio", rh, sizeof(double), -1, -1);
		vzm::RenderScene(0, 0);
		unsigned char* ptr_rgba;
		float* ptr_zdepth;
		int w, h;
		vzm::GetRenderBufferPtrs(0, &ptr_rgba, &ptr_zdepth, &w, &h, 0);
		cv::Mat cvmat(h, w, CV_8UC4);
		memcpy(cvmat.data, ptr_rgba, sizeof(unsigned char) * 4 * w * h);

		vzm::SetRenderTestParam("_int_OitMode", ot_mode_original, sizeof(int), -1, -1);
		vzm::SetRenderTestParam("_double_RobustRatio", rh_original, sizeof(double), -1, -1);
		return cvmat;
	};

	bool profiling_original = false;
	vzm::GetRenderTestParam("_bool_GpuProfile", &profiling_original, sizeof(bool), -1, -1);
	vzm::SetRenderTestParam("_bool_GpuProfile", true, sizeof(bool), -1, -1);
	cv::Mat cvmat_DFB = make_cvmat(1);
	cv::Mat cvmat_MBT = make_cvmat(2);
	cv::Mat cvmat_DKBT_highRh = make_cvmat(4, high_Rh);
	cv::Mat cvmat_DKBT_lowRh = make_cvmat(4, low_Rh);
	cv::Mat cvmat_DKBTZ_lowRh = make_cvmat(3, low_Rh);
	cv::Mat cvmat_SKBTZ = make_cvmat(0);
	vzm::SetRenderTestParam("_bool_GpuProfile", profiling_original, sizeof(bool), -1, -1);

	cv::Mat cvmat_DFB_rgb;
	cv::cvtColor(cvmat_DFB, cvmat_DFB_rgb, cv::COLOR_BGRA2BGR);

	auto GenColorMap = [&cvmat_DFB_rgb](cv::Mat mat_rgba, float _amp) -> cv::Mat
	{
		cv::Mat mat_rgb;
		cv::cvtColor(mat_rgba, mat_rgb, cv::COLOR_BGRA2BGR);

		cv::Mat mat_diff;
		cv::absdiff(cvmat_DFB_rgb, mat_rgb, mat_diff);

		auto enhance_diff = [](cv::Mat& img, float factor)
		{
			int ch = img.channels();
			for (int i = 0; i < img.rows * img.cols * ch; i++)
			{
				img.data[i] = (uchar)std::min(img.data[i] * factor, 255.f);
			}
		};
		enhance_diff(mat_diff, _amp);

		cv::Mat mat_heatmap; // 3 channels
		cv::applyColorMap(mat_diff, mat_heatmap, cv::COLORMAP_JET);

		for (int i = 0; i < mat_heatmap.rows * mat_heatmap.cols; i++)
		{
			byte a = mat_rgba.data[4 * i + 3];
			if (a == 0)
			{
				mat_heatmap.data[3 * i + 0] = 255;
				mat_heatmap.data[3 * i + 1] = 255;
				mat_heatmap.data[3 * i + 2] = 255;
			}
		}

		return mat_heatmap;
	};

	const float _amp = diff_amp;
	cv::Mat cvmat_MBT_DIFFMAP = GenColorMap(cvmat_MBT, _amp);
	cv::Mat cvmat_DKBT_highRh_DIFFMAP = GenColorMap(cvmat_DKBT_highRh, _amp);
	cv::Mat cvmat_DKBT_lowRh_DIFFMAP = GenColorMap(cvmat_DKBT_lowRh, _amp);
	cv::Mat cvmat_DKBTZ_lowRh_DIFFMAP = GenColorMap(cvmat_DKBTZ_lowRh, _amp);
	cv::Mat cvmat_SKBTZ_DIFFMAP = GenColorMap(cvmat_SKBTZ, _amp);

	cv::imshow("cvmat_MBT_DIFFMAP", cvmat_MBT_DIFFMAP);
	cv::imshow("cvmat_DKBT_HighRh_DIFFMAP", cvmat_DKBT_highRh_DIFFMAP);
	cv::imshow("cvmat_DKBT_lowRh_DIFFMAP", cvmat_DKBT_lowRh_DIFFMAP);
	cv::imshow("cvmat_DKBTZ_lowRh_DIFFMAP", cvmat_DKBTZ_lowRh_DIFFMAP);
	cv::imshow("cvmat_SKBTZ_DIFFMAP", cvmat_SKBTZ_DIFFMAP);

	cv::imwrite(out_file_prefix + "MBT_DIFFMAP.png", cvmat_MBT_DIFFMAP);
	cv::imwrite(out_file_prefix + "DKBT_HighRh_DIFFMAP.png", cvmat_DKBT_highRh_DIFFMAP);
	cv::imwrite(out_file_prefix + "DKBT_lowRh_DIFFMAP.png", cvmat_DKBT_lowRh_DIFFMAP);
	cv::imwrite(out_file_prefix + "DKBTZ_lowRh_DIFFMAP.png", cvmat_DKBTZ_lowRh_DIFFMAP);
	cv::imwrite(out_file_prefix + "SKBTZ_DIFFMAP.png", cvmat_SKBTZ_DIFFMAP);
}

std::string GetSolutionPath()
{
	using namespace std;
	char ownPth[2048];
	GetModuleFileNameA(NULL, ownPth, (sizeof(ownPth)));
	string exe_path = ownPth;
	size_t pos = 0;
	std::string token;
	string delimiter = "\\";
	string sol_path = "";
	while ((pos = exe_path.find(delimiter)) != std::string::npos) {
		token = exe_path.substr(0, pos);
		if (token.find(".exe") != std::string::npos) break;
		exe_path += token + "\\";
		exe_path.erase(0, pos + delimiter.length());
	}
	return sol_path + "..\\..\\VmProjects\\hybrid_rendering_engine\\";
}

#if (_MSVC_LANG < 201703L)
# this program uses C++ standard ver 17
#endif

void key_actions(const int key, const std::string& preset_file, const std::list<int>& loaded_obj_ids, bool& write_img_file)
{
	switch (key)
	{
	case 'g':
	{
		// (de)activate GPU profiling
		static bool gpu_profile = false;
		gpu_profile = !gpu_profile;
		vzm::SetRenderTestParam("_bool_GpuProfile", gpu_profile, sizeof(bool), -1, -1);
		std::cout << "gpu profiling : " << (gpu_profile ? "ON" : "OFF") << std::endl;
		break;
	}
	case 'w':
	{
		write_img_file = true;
		std::cout << "write result image into a file" << std::endl;
		break;
	}
	case 'r':
	{
		vzm::SetRenderTestParam("_bool_ReloadHLSLObjFiles", true, sizeof(bool), -1, -1);
		std::cout << "reload hlsl objs" << std::endl;
		break;
	}
	case 's':
	{
		if (preset_file == "") break;
		store_preset(preset_file, loaded_obj_ids);
		std::cout << "store preset" << std::endl;
		break;
	}
	case 'l':
	{
		if (preset_file == "") break;
		load_preset(preset_file, loaded_obj_ids);
		std::cout << "load preset" << std::endl;
		break;
	}
	case '`':
	case '0':
	{
		vzm::SetRenderTestParam("_int_OitMode", (int)0, sizeof(int), -1, -1);
		std::cout << "oit mode : SK+BTZ" << std::endl;
		break;
	}
	case '1':
	{
		vzm::SetRenderTestParam("_int_OitMode", (int)1, sizeof(int), -1, -1);
		std::cout << "oit mode : DFB" << std::endl;
		break;
	}
	case '2':
	{
		vzm::SetRenderTestParam("_int_OitMode", (int)2, sizeof(int), -1, -1);
		std::cout << "oit mode : MBT" << std::endl;
		break;
	}
	case '3':
	{
		vzm::SetRenderTestParam("_int_OitMode", (int)3, sizeof(int), -1, -1);
		vzm::SetRenderTestParam("_double_RobustRatio", low_Rh, sizeof(double), -1, -1);
		std::cout << "oit mode : DK+BTZ with Rh (" << low_Rh << ")" << std::endl;
		break;
	}
	case '4':
	{
		vzm::SetRenderTestParam("_int_OitMode", (int)3, sizeof(int), -1, -1);
		vzm::SetRenderTestParam("_double_RobustRatio", high_Rh, sizeof(double), -1, -1);
		std::cout << "oit mode : DK+BTZ with Rh (" << high_Rh << ")" << std::endl;
		break;
	}
	case '5':
	{
		vzm::SetRenderTestParam("_int_OitMode", (int)4, sizeof(int), -1, -1);
		vzm::SetRenderTestParam("_double_RobustRatio", low_Rh, sizeof(double), -1, -1);
		std::cout << "oit mode : DK+BT with Rh (" << low_Rh << ")" << std::endl;
		break;
	}
	case '6':
	{
		vzm::SetRenderTestParam("_int_OitMode", (int)4, sizeof(int), -1, -1);
		vzm::SetRenderTestParam("_double_RobustRatio", high_Rh, sizeof(double), -1, -1);
		std::cout << "oit mode : DK+BT with Rh (" << high_Rh << ")" << std::endl;
		break;
	}
	case 'c':
	{
		if (preset_file == "") break;
		size_t lastindex = preset_file.find_last_of(".");
		std::string rawname = preset_file.substr(0, lastindex);
		compute_difference(rawname + "_");
		std::cout << "compute difference" << std::endl;
		break;
	}
	default:
		break;
	}
}

int Fig_Absorbance()
{
	std::list<int> loaded_obj_ids;

	vzm::CameraParameters cam_params;
	vzm::ObjStates obj_state;
	high_Rh = 0.75, low_Rh = 0.2, diff_amp = 5.0;
	scene_stage_scale = 50.f;

	std::string preset_file = GetSolutionPath() + ".\\data\\preset_floor_absorbance.txt";
	vzm::LoadMultipleModelsFile(GetSolutionPath() + ".\\data\\densepoints_floor.ply", loaded_obj_ids, true);
	__cv3__ cam_params.pos = glm::fvec3(0, 0, scene_stage_scale);
	__cv3__ cam_params.up = glm::fvec3(0, 1.f, 0);
	__cv3__ cam_params.view = glm::fvec3(0, 0, -1.f);
	cam_params.np = 0.1f;
	cam_params.fp = 500.f;
	obj_state.represent_points_to_surfels = true;
	obj_state.surfel_size = 0.1f; // control for visible point size

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

	vzm::SetRenderTestParam("_bool_UseSpinLock", true, sizeof(bool), -1, -1);
	vzm::SetRenderTestParam("_double_CopVZThickness", 0.002, sizeof(double), -1, -1);
	vzm::SetRenderTestParam("_double_VZThickness", 0.0, sizeof(double), -1, -1);
	vzm::SetRenderTestParam("_double_MergingBeta", 0.5, sizeof(double), -1, -1);
	vzm::SetRenderTestParam("_double_RobustRatio", 0.5, sizeof(double), -1, -1);
	vzm::SetRenderTestParam("_double4_ShadingFactorsForGlobalPrimitives", glm::dvec4(0.4, 0.6, 0.2, 30.0), sizeof(glm::dvec4), -1, -1);
	// after presetting of SetRenderTestParams
	load_preset(preset_file, loaded_obj_ids);
	std::cout << "oit mode : SK+BTZ" << std::endl;

	int oit_mode = 0;
	int key = -1;
	while (key != 'q')
	{
		bool write_img_file = false;
		key_actions(key, preset_file, loaded_obj_ids, write_img_file);

		for (auto& it : scene_name)
		{
			show_window(it.second, it.first, 0, write_img_file, preset_file);
		}

		vzm::SetRenderTestParam("_bool_ReloadHLSLObjFiles", false, sizeof(bool), -1, -1);
		key = cv::waitKey(1);
	}

	return 0;
}

int Fig_OitIntersection()
{
	int loaded_obj_id = 0;

	std::string preset_file = GetSolutionPath() + ".\\data\\preset_oitsection_bunny.txt";
	std::string model_file = GetSolutionPath() + ".\\data\\Bunny70k.ply";
	vzm::LoadModelFile(model_file, loaded_obj_id, true);

	vzm::CameraParameters cam_params;
	__cv3__ cam_params.pos = glm::fvec3(0, 0, 300);
	__cv3__ cam_params.up = glm::fvec3(0, 1.f, 0);
	__cv3__ cam_params.view = glm::fvec3(0, 0, -1.f);
	cam_params.fov_y = 3.141592654f / 4.f;
	cam_params.aspect_ratio = 1.f;
	cam_params.projection_mode = 2;
	cam_params.w = 1024;
	cam_params.h = 1024;
	cam_params.np = 1.f;
	cam_params.fp = 1000.f;

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
	obj_state.surfel_size = 5.f; // control for visible point size
	__cv4__ obj_state.color = glm::fvec4(0.8f, 1.f, 1.f, 1.f); 
	obj_state.color[3] = 0.2f; // control for transparency
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
		cv::setMouseCallback(scene_name[i], CallBackFunc_Mouse, NULL);
	}

	align_obj_to_world_center(0, { loaded_obj_id });

	int plane_ids[3] = { 0, 0, 0 };
	int lines_ids[3] = { 0, 0, 0 };
	glm::fvec3* pos_vtx = new glm::fvec3[4];
	glm::fvec3* nrl_vtx = new glm::fvec3[4];
	uint* idx_prims = new uint[6];
	idx_prims[0] = 0;
	idx_prims[1] = 1;
	idx_prims[2] = 2;
	idx_prims[3] = 1;
	idx_prims[4] = 3;
	idx_prims[5] = 2;
	uint* idx_lines = new uint[8];
	idx_lines[0] = 0;
	idx_lines[1] = 1;
	idx_lines[2] = 1;
	idx_lines[3] = 3;
	idx_lines[4] = 3;
	idx_lines[5] = 2;
	idx_lines[6] = 2;
	idx_lines[7] = 0;

	pos_vtx[0] = glm::fvec3(0, -75, -75);
	pos_vtx[1] = glm::fvec3(0, +75, -75);
	pos_vtx[2] = glm::fvec3(0, -75, +75);
	pos_vtx[3] = glm::fvec3(0, +75, +75);
	nrl_vtx[0] = nrl_vtx[1] = nrl_vtx[2] = nrl_vtx[3] = glm::fvec3(1, 0, 0);
	vzm::GeneratePrimitiveObject((float*)pos_vtx, (float*)nrl_vtx, NULL, NULL, 4, idx_prims, 2, 3, plane_ids[0]);
	vzm::GeneratePrimitiveObject((float*)pos_vtx, NULL, NULL, NULL, 4, idx_lines, 4, 2, lines_ids[0]);
	pos_vtx[0] = glm::fvec3(-75, 0, -75);
	pos_vtx[1] = glm::fvec3(+75, 0, -75);
	pos_vtx[2] = glm::fvec3(-75, 0, +75);
	pos_vtx[3] = glm::fvec3(+75, 0, +75);
	nrl_vtx[0] = nrl_vtx[1] = nrl_vtx[2] = nrl_vtx[3] = glm::fvec3(0, 1, 0);
	vzm::GeneratePrimitiveObject((float*)pos_vtx, (float*)nrl_vtx, NULL, NULL, 4, idx_prims, 2, 3, plane_ids[1]);
	vzm::GeneratePrimitiveObject((float*)pos_vtx, NULL, NULL, NULL, 4, idx_lines, 4, 2, lines_ids[1]);
	pos_vtx[0] = glm::fvec3(-75, -75, 0);
	pos_vtx[1] = glm::fvec3(+75, -75, 0);
	pos_vtx[2] = glm::fvec3(-75, +75, 0);
	pos_vtx[3] = glm::fvec3(+75, +75, 0);
	nrl_vtx[0] = nrl_vtx[1] = nrl_vtx[2] = nrl_vtx[3] = glm::fvec3(0, 0, 1);
	vzm::GeneratePrimitiveObject((float*)pos_vtx, (float*)nrl_vtx, NULL, NULL, 4, idx_prims, 2, 3, plane_ids[2]);
	vzm::GeneratePrimitiveObject((float*)pos_vtx, NULL, NULL, NULL, 4, idx_lines, 4, 2, lines_ids[2]);

	obj_state.line_thickness = 5;

	__cv4__ obj_state.color = glm::fvec4(1, 0, 0, 0.6);
	vzm::ReplaceOrAddSceneObject(0, plane_ids[0], obj_state);
	__cv4__ obj_state.color = glm::fvec4(1, 0, 0, 1);
	vzm::ReplaceOrAddSceneObject(0, lines_ids[0], obj_state);
	__cv4__ obj_state.color = glm::fvec4(0, 1, 0, 0.6);
	vzm::ReplaceOrAddSceneObject(0, plane_ids[1], obj_state);
	__cv4__ obj_state.color = glm::fvec4(0, 1, 0, 1);
	vzm::ReplaceOrAddSceneObject(0, lines_ids[1], obj_state);
	__cv4__ obj_state.color = glm::fvec4(0, 0, 1, 0.6);
	vzm::ReplaceOrAddSceneObject(0, plane_ids[2], obj_state);
	__cv4__ obj_state.color = glm::fvec4(0, 0, 1, 1);
	vzm::ReplaceOrAddSceneObject(0, lines_ids[2], obj_state);

	delete[] pos_vtx;
	delete[] nrl_vtx;
	delete[] idx_prims;
	delete[] idx_lines;

	vzm::SetRenderTestParam("_bool_UseSpinLock", true, sizeof(bool), -1, -1);
	vzm::SetRenderTestParam("_double_CopVZThickness", 0.002, sizeof(double), -1, -1);
	vzm::SetRenderTestParam("_double_VZThickness", 0.0, sizeof(double), -1, -1);
	vzm::SetRenderTestParam("_double_MergingBeta", 0.5, sizeof(double), -1, -1);
	vzm::SetRenderTestParam("_double_RobustRatio", 0.5, sizeof(double), -1, -1);
	vzm::SetRenderTestParam("_double4_ShadingFactorsForGlobalPrimitives", glm::dvec4(0.4, 0.6, 0.2, 30.0), sizeof(glm::dvec4), -1, -1);
	// after presetting of SetRenderTestParams
	load_preset(preset_file, {loaded_obj_id});
	std::cout << "oit mode : SK+BTZ" << std::endl;

	int oit_mode = 0;
	int key = -1;
	while (key != 'q')
	{
		bool write_img_file = false;

		key_actions(key, preset_file, { loaded_obj_id }, write_img_file);

		if (key == 'e')
		{
			static int eg1_obj = 0, eg2_obj = 0, eg3_obj = 0;
			//vzm::DeleteObject(eg1_obj);
			//vzm::DeleteObject(eg2_obj);
			//vzm::DeleteObject(eg3_obj);
			vzm::LoadModelFile(GetSolutionPath() + ".\\data\\EG1.obj", eg1_obj, true);
			vzm::LoadModelFile(GetSolutionPath() + ".\\data\\EG2.obj", eg2_obj, true);
			vzm::LoadModelFile(GetSolutionPath() + ".\\data\\EG3.obj", eg3_obj, true);
			obj_state.color[3] = 0.7f;
			vzm::ReplaceOrAddSceneObject(0, eg1_obj, obj_state);
			vzm::ReplaceOrAddSceneObject(0, eg2_obj, obj_state);
			vzm::ReplaceOrAddSceneObject(0, eg3_obj, obj_state);
		}

		for (auto& it : scene_name)
		{
			show_window(it.second, it.first, 0, write_img_file, preset_file);
		}

		vzm::SetRenderTestParam("_bool_ReloadHLSLObjFiles", false, sizeof(bool), -1, -1);
		key = cv::waitKey(1);
	}

	return 0;
}

int Fig_OitPerformance()
{
	// OIT test 1
	std::list<int> loaded_obj_ids;
	
	vzm::CameraParameters cam_params;
	vzm::ObjStates obj_state;
	int w = 1024, h = 1024;
	if(w > 1024 && h > 1024)
		vzm::SetRenderTestParam("_int_BufExScale", (int)4, sizeof(int), -1, -1); // set this when the resolution 2048x2048 (NVIDIA GTX 1080)
#define __OBJ3
#ifdef __OBJ1
	high_Rh = 0.75, low_Rh = 0.2, diff_amp = 10.0;
	scene_stage_scale = 5.f;

	std::string preset_file = GetSolutionPath() + ".\\data\\preset_oit1_sportscar.txt";
	vzm::LoadMultipleModelsFile(GetSolutionPath() + ".\\data\\sportsCar.obj", loaded_obj_ids, true);
	__cv3__ cam_params.pos = glm::fvec3(0, 0, scene_stage_scale);
	__cv3__ cam_params.up = glm::fvec3(0, 1.f, 0);
	__cv3__ cam_params.view = glm::fvec3(0, 0, -1.f);
	cam_params.np = 0.01f;
	cam_params.fp = 20.f;
	// obj file includes material info, which is prior shading option for rendering; therefore, wildcard setting is required to change shading.

#elif defined(__OBJ2)
	high_Rh = 0.75, low_Rh = 0.2, diff_amp = 10.0;
	scene_stage_scale = 15.f;

	std::string preset_file = GetSolutionPath() + ".\\data\\preset_oit1_hairball.txt";
	vzm::LoadMultipleModelsFile(GetSolutionPath() + ".\\data\\hairball_colored.ply", loaded_obj_ids, true);
	__cv3__ cam_params.pos = glm::fvec3(0, 0, scene_stage_scale);
	__cv3__ cam_params.up = glm::fvec3(0, 1.f, 0);
	__cv3__ cam_params.view = glm::fvec3(0, 0, -1.f);
	cam_params.np = 0.01f;
	cam_params.fp = 100.f;
#elif defined(__OBJ3)
	high_Rh = 0.75, low_Rh = 0.2, diff_amp = 7.0;
	scene_stage_scale = 50.f;

	std::string preset_file = GetSolutionPath() + ".\\data\\preset_oit1_floor.txt";
	vzm::LoadMultipleModelsFile(GetSolutionPath() + ".\\data\\densepoints_floor.ply", loaded_obj_ids, true);
	__cv3__ cam_params.pos = glm::fvec3(0, 0, scene_stage_scale);
	__cv3__ cam_params.up = glm::fvec3(0, 1.f, 0);
	__cv3__ cam_params.view = glm::fvec3(0, 0, -1.f);
	cam_params.np = 0.1f;
	cam_params.fp = 500.f;
	obj_state.surfel_size = 0.02f; // control for visible point size
#endif

	cam_params.fov_y = 3.141592654f / 4.f;
	cam_params.projection_mode = 2;
	cam_params.w = w;
	cam_params.h = h;
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

	vzm::SetRenderTestParam("_bool_UseSpinLock", true, sizeof(bool), -1, -1);
	vzm::SetRenderTestParam("_double_CopVZThickness", 0.002, sizeof(double), -1, -1);
	vzm::SetRenderTestParam("_double_VZThickness", 0.0, sizeof(double), -1, -1);
	vzm::SetRenderTestParam("_double_MergingBeta", 0.5, sizeof(double), -1, -1);
	vzm::SetRenderTestParam("_double_RobustRatio", 0.5, sizeof(double), -1, -1);
	vzm::SetRenderTestParam("_double4_ShadingFactorsForGlobalPrimitives", glm::dvec4(0.4, 0.6, 0.2, 30.0), sizeof(glm::dvec4), -1, -1);
	// after presetting of SetRenderTestParams
	load_preset(preset_file, loaded_obj_ids);
	std::cout << "oit mode : SK+BTZ" << std::endl;

	int oit_mode = 0;
	int key = -1;
	while (key != 'q')
	{
		bool write_img_file = false;
		key_actions(key, preset_file, loaded_obj_ids, write_img_file);

		for (auto& it : scene_name)
		{
			show_window(it.second, it.first, 0, write_img_file, preset_file);
		}

		vzm::SetRenderTestParam("_bool_ReloadHLSLObjFiles", false, sizeof(bool), -1, -1);
		key = cv::waitKey(1);
	}

	return 0;
}

int Fig_LocalDepthBlending()
{
	// OIT test 1
	std::list<int> loaded_obj_ids;

	vzm::CameraParameters cam_params;
	vzm::ObjStates obj_state;
	high_Rh = 0.75, low_Rh = 0.2, diff_amp = 5.0;
	scene_stage_scale = 50.f;

	std::string preset_file = GetSolutionPath() + ".\\data\\preset_shallowdepth_floor.txt";
	vzm::LoadMultipleModelsFile(GetSolutionPath() + ".\\data\\densepoints_floor.ply", loaded_obj_ids, true);
	__cv3__ cam_params.pos = glm::fvec3(0, 0, scene_stage_scale);
	__cv3__ cam_params.up = glm::fvec3(0, 1.f, 0);
	__cv3__ cam_params.view = glm::fvec3(0, 0, -1.f);
	cam_params.np = 0.1f;
	cam_params.fp = 500.f;
	obj_state.surfel_size = 0.02f; // control for visible point size

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
	obj_state.color[3] = 1.0f; // control for transparency
	__cm4__ obj_state.os2ws = glm::fmat4x4(); // identity

	scene_name[0] = "Sallow Depth Blending";

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

	vzm::SetRenderTestParam("_bool_UseSpinLock", true, sizeof(bool), -1, -1);
	vzm::SetRenderTestParam("_double_CopVZThickness", 0.002, sizeof(double), -1, -1);
	vzm::SetRenderTestParam("_double_VZThickness", 0.0, sizeof(double), -1, -1);
	vzm::SetRenderTestParam("_double_MergingBeta", 0.5, sizeof(double), -1, -1);
	vzm::SetRenderTestParam("_double_RobustRatio", 0.5, sizeof(double), -1, -1);
	vzm::SetRenderTestParam("_double4_ShadingFactorsForGlobalPrimitives", glm::dvec4(0.4, 0.6, 0.2, 30.0), sizeof(glm::dvec4), -1, -1);
	// after presetting of SetRenderTestParams
	load_preset(preset_file, loaded_obj_ids);
	std::cout << "oit mode : SK+BTZ" << std::endl;

	int oit_mode = 0;
	int key = -1;
	while (key != 'q')
	{
		bool write_img_file = false;
		key_actions(key, preset_file, loaded_obj_ids, write_img_file);

		for (auto& it : scene_name)
		{
			show_window(it.second, it.first, 0, write_img_file, preset_file);
		}

		vzm::SetRenderTestParam("_bool_ReloadHLSLObjFiles", false, sizeof(bool), -1, -1);
		key = cv::waitKey(1);
	}

	return 0;
}

int Fig_HybridVR()
{
	int loaded_vol_id = 0;
	std::string preset_file = GetSolutionPath() + ".\\data\\preset_hybrid_backpack.txt";
	vzm::LoadModelFile(GetSolutionPath() + ".\\data\\backpack16_512x512x373_0.0039064x0.0039064x0.005_ushort.den", loaded_vol_id, true);
	int loaded_mesh_id = 0;
	vzm::LoadModelFile(GetSolutionPath() + ".\\data\\hairball_colored.ply", loaded_mesh_id, true);
	int loaded_surfel_id= 0;
	vzm::LoadModelFile(GetSolutionPath() + ".\\data\\densepoints_floor.ply", loaded_surfel_id, true);

	int vr_tmap_id = 0;
	std::vector<glm::fvec2> alpha_ctrs;
	alpha_ctrs.push_back(glm::fvec2(0, 50));
	alpha_ctrs.push_back(glm::fvec2(1.0, 60));
	alpha_ctrs.push_back(glm::fvec2(1.0, 65536));
	alpha_ctrs.push_back(glm::fvec2(0, 65537));
	std::vector<glm::fvec4> rgb_ctrs;
	rgb_ctrs.push_back(glm::fvec4(1, 0.31, 0.78, 0));
	rgb_ctrs.push_back(glm::fvec4(1, 0.31, 0.78, 30));
	rgb_ctrs.push_back(glm::fvec4(0.51, 1, 0.49, 130));
	rgb_ctrs.push_back(glm::fvec4(1, 0.31, 0.78, 200));
	rgb_ctrs.push_back(glm::fvec4(1, 1, 1, 65536));
	vzm::GenerateMappingTable(65537, alpha_ctrs.size(), (float*)&alpha_ctrs[0], rgb_ctrs.size(), (float*)&rgb_ctrs[0], vr_tmap_id);

	vzm::ObjStates volume_state;
	volume_state.associated_obj_ids[vzm::ObjStates::VR_OTF] = vr_tmap_id;
	double vol_scale = 1.0;// 0.02f * 0.2f;
	volume_state.is_visible = false;
	__cm4__ volume_state.os2ws = glm::translate(glm::fvec3(-4.37, 8.71, 0)) * glm::scale(glm::fvec3(vol_scale));

	vzm::ReplaceOrAddSceneObject(0, loaded_vol_id, volume_state);
	vzm::ObjStates obj_state;
	obj_state.color[3] = 0.1f; // control for transparency
	obj_state.surfel_size = 0.12f;
	vzm::ReplaceOrAddSceneObject(0, loaded_surfel_id, obj_state);
	align_obj_to_world_center(0, { loaded_surfel_id });

	obj_state.color[3] = 0.1f;
	__cm4__ obj_state.os2ws = glm::translate(glm::fvec3(-4.37, 8.71, 0)) * glm::scale(glm::fvec3(0.2));
	vzm::ReplaceOrAddSceneObject(0, loaded_mesh_id, obj_state);

	vzm::SetRenderTestParam("_double_UserSampleRate", 1.0 / vol_scale, sizeof(double), -1, -1);
	vzm::SetRenderTestParam("_bool_ApplySampleRateToGradient", false, sizeof(bool), -1, -1);
	vzm::SetRenderTestParam("_int_RendererType", (int)3, sizeof(int), 0, 0, loaded_vol_id);

	vzm::CameraParameters cam_params;
	high_Rh = 0.75, low_Rh = 0.2, diff_amp = 5.0;
	scene_stage_scale = 50.f;

	__cv3__ cam_params.pos = glm::fvec3(-3.7116, 2.22519, 0.150089);
	__cv3__ cam_params.up = glm::fvec3(-0.0210909, 0.0635336, 0.997757);
	__cv3__ cam_params.view = glm::fvec3(-0.0798922, 0.99468, -0.0650266);
	cam_params.np = 0.10f;
	cam_params.fp = 1000.f;

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

	scene_name[0] = "Hybrid Objects Scene";

	std::map<std::string, std::list<int>> prim_ids = {
		{scene_name[0], {loaded_vol_id, loaded_mesh_id, loaded_surfel_id}} ,
	};

	for (int i = 0; i < (int)scene_name.size(); i++)
	{
		// (int)i is scene_id
		vzm::SetSceneEnvParameters(i, scn_env_params);
		vzm::SetCameraParameters(i, cam_params, 0);

		//auto it = prim_ids.find(scene_name[i]);

		cv::namedWindow(scene_name[i], cv::WINDOW_AUTOSIZE);
		cv::setMouseCallback(scene_name[i], CallBackFunc_Mouse, NULL);// &scenes[i]);
	}

	vzm::SetRenderTestParam("_bool_UseSpinLock", true, sizeof(bool), -1, -1);
	vzm::SetRenderTestParam("_double_CopVZThickness", 0.00002, sizeof(double), -1, -1);
	vzm::SetRenderTestParam("_double_VZThickness", 0.0, sizeof(double), -1, -1);
	vzm::SetRenderTestParam("_double_MergingBeta", 0.5, sizeof(double), -1, -1);
	vzm::SetRenderTestParam("_double_RobustRatio", 0.5, sizeof(double), -1, -1);
	vzm::SetRenderTestParam("_double4_ShadingFactorsForGlobalPrimitives", glm::dvec4(0.4, 0.6, 0.2, 30.0), sizeof(glm::dvec4), -1, -1);
	// after presetting of SetRenderTestParams
	load_preset(preset_file, { });
	std::cout << "oit mode : SK+BTZ" << std::endl;

	int oit_mode = 0;
	int key = -1;
	while (key != 'q')
	{
		bool write_img_file = false;
		key_actions(key, preset_file, { }, write_img_file);

		for (auto& it : scene_name)
		{
			show_window(it.second, it.first, 0, write_img_file, preset_file);
		}

		vzm::SetRenderTestParam("_bool_ReloadHLSLObjFiles", false, sizeof(bool), -1, -1);
		key = cv::waitKey(1);
	}

	return 0;
}

int Fig_GhostedIllustration()
{
	std::list<int> loaded_obj_ids;

	vzm::CameraParameters cam_params;
	vzm::ObjStates obj_state;
	int w = 1024, h = 1024;
	if (w > 1024 && h > 1024)
		vzm::SetRenderTestParam("_int_BufExScale", (int)4, sizeof(int), -1, -1); // set this when the resolution 2048x2048 (NVIDIA GTX 1080)

	high_Rh = 0.75, low_Rh = 0.2, diff_amp = 10.0;
	scene_stage_scale = 1.f;

	vzm::LoadMultipleModelsFile(GetSolutionPath() + ".\\data\\crab.obj", loaded_obj_ids, true);
	__cv3__ cam_params.pos = glm::fvec3(0, 0, scene_stage_scale);
	__cv3__ cam_params.up = glm::fvec3(0, 1.f, 0);
	__cv3__ cam_params.view = glm::fvec3(0, 0, -1.f);
	cam_params.np = 0.01f;
	cam_params.fp = 10.f;
	// obj file includes material info, which is prior shading option for rendering; therefore, wildcard setting is required to change shading.


	cam_params.fov_y = 3.141592654f / 4.f;
	cam_params.projection_mode = 2;
	cam_params.w = w;
	cam_params.h = h;
	cam_params.aspect_ratio = (float)cam_params.w / (float)cam_params.h;

	vzm::SceneEnvParameters scn_env_params;
	scn_env_params.is_on_camera = true;
	scn_env_params.is_pointlight = true;
	__cv3__ scn_env_params.pos_light = __cv3__ cam_params.pos;
	__cv3__ scn_env_params.dir_light = __cv3__ cam_params.view;

	__cv4__ obj_state.color = glm::fvec4(0.2f, 1.f, 1.f, 1.f);
	obj_state.color[3] = 1.0f; // control for transparency
	__cm4__ obj_state.os2ws = glm::fmat4x4(); // identity

	scene_name[0] = "Ghosted Illustration ZT";
	scene_name[1] = "Ghosted Illustration ALPHA";

	std::map<std::string, std::list<int>> prim_ids = {
		{scene_name[0], loaded_obj_ids} ,
		{scene_name[1], loaded_obj_ids} ,
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
	align_obj_to_world_center(1, loaded_obj_ids);

	vzm::SetRenderTestParam("_bool_UseSpinLock", true, sizeof(bool), -1, -1);
	vzm::SetRenderTestParam("_double_CopVZThickness", 0.0002, sizeof(double), -1, -1);
	vzm::SetRenderTestParam("_double_VZThickness", 0.0, sizeof(double), -1, -1);
	vzm::SetRenderTestParam("_double_MergingBeta", 1.0, sizeof(double), -1, -1);
	vzm::SetRenderTestParam("_double_RobustRatio", 0.5, sizeof(double), -1, -1);
	vzm::SetRenderTestParam("_double4_ShadingFactorsForGlobalPrimitives", glm::dvec4(0.0, 1.5, 2.0, 100.0), sizeof(glm::dvec4), -1, -1);
	// after presetting of SetRenderTestParams
	//load_preset(preset_file, loaded_obj_ids);
	std::cout << "oit mode : SK+BTZ" << std::endl;

	int control_value = 250;
	cv::createTrackbar("thickness", scene_name[0], &control_value, 1000, NULL);
	int control_alpha = 30;
	cv::createTrackbar("alpha", scene_name[1], &control_alpha, 100, NULL);

	auto transform = [](const glm::fvec3& pos, const glm::fmat4x4& m)
	{
		glm::fvec4 pos4 = glm::fvec4(pos, 1);
		pos4 = m * pos4;
		pos4 /= pos4.w;
		return glm::fvec3(pos4);
	};

	int oit_mode = 0;
	int key = -1;
	while (key != 'q')
	{
		bool write_img_file = false;
		key_actions(key, "", loaded_obj_ids, write_img_file);

		vzm::GetCameraParameters(0, cam_params, 0);
		glm::fmat4x4 r = glm::rotate(glm::pi<float>() * 0.001f, glm::fvec3(__cv3__ cam_params.up));
		__cv3__ cam_params.pos = transform(__cv3__ cam_params.pos, r);
		__cv3__ cam_params.view = -(__cv3__ cam_params.pos);
		vzm::SetCameraParameters(0, cam_params, 0);
		vzm::SetCameraParameters(1, cam_params, 0);

		auto ito = prim_ids.find(scene_name[1]);
		for (int& obj_id : ito->second)
		{
			vzm::GetSceneObjectState(1, obj_id, obj_state);
			obj_state.color[3] = control_alpha / 100.0f;
			vzm::ReplaceOrAddSceneObject(1, obj_id, obj_state);
		}

		vzm::SetRenderTestParam("_double_VZThickness", control_value * 2.0 / 1000.0, sizeof(double), -1, -1);
		show_window(scene_name[0], 0, 0, write_img_file, GetSolutionPath() + ".\\data\\GI_crab_thickness");
		vzm::SetRenderTestParam("_double_VZThickness", 0.0002, sizeof(double), -1, -1);
		show_window(scene_name[1], 1, 0, write_img_file, GetSolutionPath() + ".\\data\\GI_crab_alpha");

		vzm::SetRenderTestParam("_bool_ReloadHLSLObjFiles", false, sizeof(bool), -1, -1);
		key = cv::waitKey(1);
	}

	return 0;
}

int Test()
{
	// OIT test 1
	std::list<int> loaded_obj_ids;

	vzm::CameraParameters cam_params;
	vzm::ObjStates obj_state;
	int w = 1024, h = 1024;
	if (w > 1024 && h > 1024)
		vzm::SetRenderTestParam("_int_BufExScale", (int)4, sizeof(int), -1, -1); // set this when the resolution 2048x2048 (NVIDIA GTX 1080)

	high_Rh = 0.75, low_Rh = 0.2, diff_amp = 10.0;
	scene_stage_scale = 100.f;

	std::string preset_file = GetSolutionPath() + ".\\data\\preset_test_ellipsoidal_spheres.txt";
	int obj_loaded_id = 0;
	vzm::LoadModelFile(GetSolutionPath() + ".\\data\\ellipsoidal_spheres.stl", obj_loaded_id, true);
	loaded_obj_ids.push_back(obj_loaded_id);
	obj_loaded_id = 0;
	vzm::LoadModelFile(GetSolutionPath() + ".\\data\\ellipsoidal_spheres.stl", obj_loaded_id, true);
	loaded_obj_ids.push_back(obj_loaded_id);
	obj_loaded_id = 0;
	vzm::LoadModelFile(GetSolutionPath() + ".\\data\\ellipsoidal_spheres.stl", obj_loaded_id, true);
	loaded_obj_ids.push_back(obj_loaded_id);
	__cv3__ cam_params.pos = glm::fvec3(0, 0, scene_stage_scale);
	__cv3__ cam_params.up = glm::fvec3(0, 1.f, 0);
	__cv3__ cam_params.view = glm::fvec3(0, 0, -1.f);
	cam_params.np = 0.10f;
	cam_params.fp = 1000.f;
	// obj file includes material info, which is prior shading option for rendering; therefore, wildcard setting is required to change shading.


	cam_params.fov_y = 3.141592654f / 4.f;
	cam_params.projection_mode = 2;
	cam_params.w = w;
	cam_params.h = h;
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

	vzm::SetRenderTestParam("_bool_UseSpinLock", true, sizeof(bool), -1, -1);
	vzm::SetRenderTestParam("_double_CopVZThickness", 0.002, sizeof(double), -1, -1);
	vzm::SetRenderTestParam("_double_VZThickness", 0.0, sizeof(double), -1, -1);
	vzm::SetRenderTestParam("_double_MergingBeta", 0.5, sizeof(double), -1, -1);
	vzm::SetRenderTestParam("_double_RobustRatio", 0.5, sizeof(double), -1, -1);
	vzm::SetRenderTestParam("_double4_ShadingFactorsForGlobalPrimitives", glm::dvec4(0.4, 0.6, 0.2, 30.0), sizeof(glm::dvec4), -1, -1);
	// after presetting of SetRenderTestParams
	load_preset(preset_file, loaded_obj_ids);
	std::cout << "oit mode : SK+BTZ" << std::endl;

	int oit_mode = 0;
	int key = -1;
	while (key != 'q')
	{
		bool write_img_file = false;
		key_actions(key, preset_file, loaded_obj_ids, write_img_file);

		for (auto& it : scene_name)
		{
			show_window(it.second, it.first, 0, write_img_file, preset_file);
		}

		vzm::SetRenderTestParam("_bool_ReloadHLSLObjFiles", false, sizeof(bool), -1, -1);
		key = cv::waitKey(1);
	}

	return 0;
}

int main()
{
	vzm::InitEngineLib();

	//Fig_Absorbance();
	//Fig_OitIntersection();
	//Test();
	//Fig_OitPerformance();
	Fig_LocalDepthBlending();
	//Fig_GhostedIllustration();
	//Fig_HybridVR();
	
	vzm::DeinitEngineLib();
	return 0;
}
