/*
Copyright (c) 2019, Dongjoon Kim
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list of
conditions and the following disclaimer. Redistributions in binary form must reproduce
the above copyright notice, this list of conditions and the following disclaimer
in the documentation and/or other materials provided with the distribution. Commercial
purposed SW that uses this library must be approved by the main contributor of 
Dongjoon Kim (korfriend@gmail.com).

Neither the name of the Seoul National University nor the names of its contributors
may be used to endorse or promote products derived from this software without specific
prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
TO, PROCUREMENT OF SUBSTITUTE  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
DAMAGE.
*/

#pragma once
#define __dojostatic extern "C" __declspec(dllexport)
#define __dojosclass class __declspec(dllexport)

#define __FP (float*)&

#include <string>
#include <map>
#include <any>
#include <list>

#define VERSION "2.0" // released at 20.09.05

// Dongjoon's VisMotive interface.
namespace vzm
{
	struct CameraParameters
	{
		float pos[3], view[3], up[3]; // WS coordinates
		int projection_mode; // 0 : undefined, 1 : use ip_w, ip_h instead of fov_y, aspect_ratio, 2 : vice versa, 3: AR mode, 4: sectional mode
		union {
			struct { // projection_mode == 1 or 4
				float ip_w, ip_h; // defined in CS
			};
			struct { // projection_mode == 2
				// fov_y should be smaller than PI
				float fov_y, aspect_ratio; // ip_w / ip_h
			};
			struct { // projection_mode == 3
				// AR mode (normal camera intrinsics..)
				float fx, fy, sc, cx, cy;
			};
		};
		float np, fp; // the scale difference is recommended : ~100000 (in a single precision (float))
		int w, h; // resolution. note that the aspect ratio is recomputed w.r.t. w and h during the camera setting.
		bool is_rgba_write; // if false, use BGRA order
		CameraParameters() {
			projection_mode = 0; is_rgba_write = false; 
			np = 0.01f, fp = 1000.f;
		};
	};

	struct ObjStates
	{
		// common 
		float os2ws[16]; // 4x4 matrix col-major (same as in glm::fmat4x4)
		float emission, diffusion, specular, sp_pow; // Phong's material reflection model
		bool is_visible;
		float color[4]; // rgba [0,1]

		enum USAGE
		{
			VR_OTF,
			MPR_WINDOWING,
			VOLUME_MAP,
			COLOR_MAP
		};
		// usage : "VR_OTF", "MPR_WINDOWING", "VOLUME_MAP", "COLOR_MAP"
		std::map<USAGE, int> associated_obj_ids; // <usage, obj_id> 

		// 3D only
		bool show_outline;
		
		// primitive 3D only
		bool use_vertex_color; // use vertex color instead of color[0,1,2], if vertex buffer contains color information. note that color[3] is always used for the transparency
		float point_thickness; // (diameter in pixels) when the object is defined as a point cloud without surfels
		float surfel_size; // (diameter in world unit) when the object is defined as a point cloud with surfels
		bool represent_points_to_surfels; // available when the object is defined as a point cloud
		float line_thickness; // (pixels) available when the object is defined as line primitives and not available for wire frame lines
		bool is_wireframe; // available when the object is a polygonal mesh
		bool use_vertex_wirecolor; // use vertex color instead of wire_color[0,1,2], if vertex buffer contains color information. note that color[3] is always used for the transparency
		float wire_color[4]; // rgba [0,1].. only for wireframe object

		// volume 3D only
		float sample_rate; // NA in this version

		ObjStates()
		{
			memset(os2ws, 0, sizeof(float) * 16);
			os2ws[0] = os2ws[5] = os2ws[10] = os2ws[15] = 1.f;
			emission = 0.2f, diffusion = 0.4f, specular = 0.4f, sp_pow = 10.f;
			is_visible = true;
			is_wireframe = false;
			use_vertex_color = true;
			use_vertex_wirecolor = false;
			color[0] = color[1] = color[2] = color[3] = 1.f;
			memset(wire_color, 0, sizeof(float) * 4);
			wire_color[3] = 1.f;
			point_thickness = 0; // using 1 pixel 
			line_thickness = 0; // using 1 pixel
			surfel_size = 0; // using 0.002 size of object boundary
			show_outline = false;
			represent_points_to_surfels = true;

			sample_rate = 1.f;
		}
	};

	struct SSAO_Params
	{
		bool is_on_ssao;
		float kernel_r;
		float ao_power;
		float tangent_bias;
		int num_dirs;
		int num_steps;
		bool smooth_filter;
		SSAO_Params()
		{
			is_on_ssao = false;
			kernel_r = 0;
			ao_power = 1.f;
			tangent_bias = 3.141592654f / 6.f;
			num_dirs = 8;
			num_steps = 8;
			smooth_filter = true;
		}
	};

	struct SceneEnvParameters
	{
		float pos_light[3], dir_light[3];
		bool is_pointlight; // 'true' uses pos_light, 'false' uses dir_light
		bool is_on_camera; // 'true' sets cam params to light source. in this case, it is unnecessary to set pos_light and dir_light (ignore both)
		SSAO_Params effect_ssao;
		
		// to do ... add more in future...
		SceneEnvParameters()
		{
			is_on_camera = true;
			is_pointlight = false;
		}
	};

	__dojostatic bool InitEngineLib();
	__dojostatic bool DeinitEngineLib();

	// here, obj_id (without const) is [in/out].. in : when a registered object of obj_id exists, out : when there is no registered object of obj_id
	__dojostatic bool LoadModelFile(const std::string& filename, int& obj_id, const bool unify_redundancy = false);
	__dojostatic bool LoadMultipleModelsFile(const std::string& filename, std::list<int>& obj_ids, const bool unify_redundancy = false);
	// data_type "CHAR" "BYTE" "SHORT" "USHORT" "INT" "FLOAT"
	__dojostatic bool GenerateEmptyVolume(int& vol_id, const int ref_vol_id = 0, const std::string& data_type = "", const double min_v = 0, const double max_v = 0, const double fill_v = 0);
	__dojostatic bool GenerateEmptyPrimitive(int& prim_id);
	__dojostatic bool GenerateArrowObject(const float* pos_s, const float* pos_e, const float radius, int& obj_id);
	// optional : rgb_list (if NULL, this is not used)
	__dojostatic bool GenerateSpheresObject(const float* xyzr_list, const float* rgb_list, const int num_spheres, int& obj_id);
	__dojostatic bool GenerateCylindersObject(const float* xyz_01_list, const float* radius_list, const float* rgb_list, const int num_cylinders, int& obj_id);
	// when line_thickness = 0, the line thickness is not used, depending on the line renderer of the rendering API.
	__dojostatic bool GenerateLinesObject(const float* xyz_01_list, const float* rgb_01_list, const int num_lines, int& obj_id);
	__dojostatic bool GenerateTrianglesObject(const float* xyz_012_list, const float* rgb_012_list, const int num_tris, int& obj_id);
	// optional : nrl_list, rgb_list, tex_list, idx_prims (if NULL, this is not used)
	// stride_prim_idx : 1 ==> point cloud, 2 ==> line, 3 ==> triangle
	__dojostatic bool GeneratePrimitiveObject(const float* xyz_list, const float* nrl_list, const float* rgb_list, const float* tex_list, const int num_vtx, const unsigned int* idx_prims, const int num_prims, const int stride_prim_idx, int& obj_id);
	// optional : nrl_list, rgb_list (if NULL, this is not used)
	__dojostatic bool GeneratePointCloudObject(const float* xyz_list, const float* nrl_list, const float* rgb_list, const int num_points, int& obj_id);
	__dojostatic bool GenerateTextObject(const float* xyz_LT_view_up, const std::string& text, const float font_height, bool bold, bool italic, int& obj_id);
	__dojostatic bool GenerateMappingTable(const int table_size, const int num_alpha_ctrs, const float* ctr_alpha_idx_list, const int num_rgb_ctrs, const float* ctr_rgb_idx_list, int& tmap_id);
	__dojostatic bool GenerateCopiedObject(const int obj_src_id, int& obj_id);

	__dojostatic bool ReplaceOrAddSceneObject(const int scene_id, const int obj_id, const ObjStates& obj_states);
	__dojostatic bool GetSceneObjectState(const int scene_id, const int obj_id, ObjStates& obj_states);
	// when empty initializer_list, all objs in the scene are considered.
	__dojostatic bool GetSceneBoundingBox(const std::list<int>& io_obj_ids, const int scene_id, float* pos_aabb_min_ws, float* pos_aabb_max_ws);
	__dojostatic bool RemoveSceneObject(const int scene_id, const int obj_id);
	__dojostatic bool RemoveScene(const int scene_id);
	__dojostatic bool DeleteObject(const int obj_id); // the obj is deleted in memory
	__dojostatic bool SetSceneEnvParameters(const int scene_id, const SceneEnvParameters& env_params);
	__dojostatic bool GetSceneEnvParameters(const int scene_id, SceneEnvParameters& env_params);
	// cam id is corresponding to a specific renderer and ip states
	__dojostatic bool SetCameraParameters(const int scene_id, const CameraParameters& cam_params, const int cam_id = 0);
	__dojostatic bool GetCameraParameters(const int scene_id, CameraParameters& cam_params, const int cam_id = 0);
	__dojostatic bool GetCamProjMatrix(const int scene_id, const int cam_id, float* mat_ws2ss, float* mat_ss2ws = NULL, bool is_col_major = true);

	__dojostatic bool RenderScene(const int scene_id, const int cam_id = 0);
	__dojostatic bool GetRenderBufferPtrs(const int scene_id, unsigned char** ptr_rgba, float** ptr_zdepth, int* fbuf_w, int* fbuf_h, const int cam_id = 0, size_t* render_count = NULL);

	// etc
	__dojostatic bool GetPModelData(const int obj_id, float** pos_vtx, float** nrl_vtx, float** rgb_vtx, float** tex_vtx, int& num_vtx, unsigned int** idx_prims, int& num_prims, int& stride_prim_idx);
	__dojostatic bool GetVolumeInfo(const int obj_id, void*** vol_slices_2darray_pointer, int* size_xyz, float* pitch_xyz, int* stride_bytes);

	// picking
	__dojostatic bool ValidatePickTarget(const int obj_id);
	__dojostatic bool PickObject(int& pick_obj_id, float* pos_pick, const int x, const int y, const int scene_id, const int cam_id = 0);
	__dojostatic bool Pick1stHitSurfaceUsingDepthMap(float* pos_pick, const int x, const int y, const float valid_depth_range, const int scene_id, const int cam_id);

	// only for the contributor's (by DongJoon Kim) test info.
	__dojostatic void SetRenderTestParam(const std::string& _script, const std::any& value, const size_t size_bytes, const int scene_id, const int cam_id, const int obj_id = -1);
	__dojostatic bool GetRenderTestParam(const std::string& _script, void* pvalue, const size_t size_bytes,  const int scene_id, const int cam_id, const int obj_id = -1);
	__dojostatic void DisplayConsoleMessages(const bool is_display);

	__dojostatic bool ExecuteModule2(const std::string& module_dll_file, const std::string& dll_function, const std::list<int>& io_obj_ids, const std::map<std::string, std::any>& parameters);
}

namespace vzmproc
{
	__dojostatic bool SimplifyPModelByUGrid(const int obj_src_id, const float cell_width, int& obj_dst_id);
	__dojostatic bool ComputePCA(const int obj_id, float* egvals /*float3*/, float* egvecs /*three of float3*/);

	__dojostatic bool GenerateSamplePoints(const int obj_src_id, const float* pos_src, const float r, const float min_interval, int& obj_dst_id);
	// based on special-care ICP
	__dojostatic bool ComputeMatchingTransform(const int obj_from_id, const int obj_to_id, float* mat_tr /*float16*/);
}

namespace helpers
{
	//__dojostatic bool CopyObject(const int src_obj_id, const int dst_obj_id);
	__dojostatic bool ComputePCAc(const float* xyz_list, const int num_points, float* egvals /*float3*/, float* egvecs /*three of float3*/);

	// at least 3 point-pairs are requested
	// matching based on least squares
	// assume each point pair has the same index of the point list (one-to-one);
	__dojostatic bool ComputeRigidTransform(const float* xyz_from_list, const float* xyz_to_list, const int num_pts, float* mat_tr /*float16*/);

	// mat_ext : glm::fmat4x3 format, conventional camera system's extrinsic parameters (y down and z as view direction)
	// fx, fy : focusing parameters
	// sc : skew coefficient
	// cx, cy : principal point position
	// w, h : screen size
	// zn, zf : near and far plane
	// api_mode : 0 => opengl, 1 => direct3d
	// mat_ws2cs (view matrix), mat_cs2ps (projection matrix), mat_ps2ss : glm::fmat4x4 format, output
	__dojostatic bool ComputeCameraRendererMatrice(const float* mat_ext,
		const float fx, const float fy, const float sc, const float cx, const float cy,
		const int w, const int h, const float zn, const float zf, const int api_mode,
		float* mat_ws2cs, float* mat_cs2ps, float* mat_ps2ss);
	__dojostatic bool ComputeCameraRendererParameters(const float* pos_xyz_ws, const float* pos_xy_ss, const int num_mks,
		float* cam_pos, float* cam_view, float* cam_up, float* fx, float* fy, float* sc, float* cx, float* cy);

	__dojostatic bool ComputeArCameraCalibrateInfo(const float* mat_rbs2ts, const float* calrb_xyz_ts, const float* calrb_xy_ss, const int num_mks,
		float* mat_camcs2rbs, vzm::CameraParameters* cam_ar_mode_params);

	struct cam_pose
	{
		float pos[3], view[3], up[3]; // WS coordinates
	};

	__dojosclass arcball
	{
	public:
		arcball();
		~arcball();
		// stage_center .. fvec3
		bool intializer(const float* stage_center, const float stage_radius);
		// pos_xy .. ivec2
		bool start(const int* pos_xy, const float* screen_size, const cam_pose& cam_pose, const float np = 0.1f, const float fp = 100.f, const float sensitivity = 1.0);
		// pos_xy .. ivec2
		// mat_r_onmove .. fmat4x4
		bool move(const int* pos_xy, cam_pose& cam_pose);	// target is camera
		bool move(const int* pos_xy, float* mat_r_onmove);	// target is object
		bool pan_move(const int* pos_xy, cam_pose& cam_pose);
	};
}