#pragma once
//#include "GpuManager.h"
#include "../gpu_common_res.h"
#include <functional>

using namespace fncontainer;
//#include <map>
//#define __DX_DEBUG_QUERY

#ifdef USE_DX11_3
#define __ID3D11Device ID3D11Device3
#define __ID3D11DeviceContext ID3D11DeviceContext3
#else
#define __ID3D11Device ID3D11Device
#define __ID3D11DeviceContext ID3D11DeviceContext
#endif

#define TRANSPOSE(A) glm::transpose(A)

namespace grd_helper
{
	using namespace std;

	enum GpuhelperResType {
		VERTEX_SHADER = 0,
		PIXEL_SHADER,
		GEOMETRY_SHADER,
		COMPUTE_SHADER,
		//BUFFER, // constant buffer
		DEPTHSTENCIL_STATE,
		RASTERIZER_STATE,
		BLEND_STATE,
		SAMPLER_STATE,
		INPUT_LAYOUT,
		ETC,
	};

#define NUM_MATERIALS 6
	static string g_materials[NUM_MATERIALS] = { "MAP_KA", "MAP_KD", "MAP_KS", "MAP_NS", "MAP_BUMP", "MAP_D" };

	static bool is_test_out = false;

	struct COMRES_INDICATOR
	{
		GpuhelperResType res_type;
		string res_name;
		COMRES_INDICATOR(const COMRES_INDICATOR& gres)
		{
			res_type = gres.res_type;
			res_name = gres.res_name;
		}

		COMRES_INDICATOR(const GpuhelperResType& _res_type, const string& _res_name)
		{
			res_type = _res_type;
			res_name = _res_name;
		}
	};

	class value_cmp
	{
	public:
		bool operator()(const COMRES_INDICATOR& a, const COMRES_INDICATOR& b) const
		{
			string a_str = to_string(a.res_type) + "_" + a.res_name;
			string b_str = to_string(b.res_type) + "_" + b.res_name;
			return a_str < b_str;
			//if (a_str.compare(b_str) < 0)
			//	return true;
			//else if (a_str.compare(b_str) > 0)
			//	return false;
			//return false;
		}
	};

	//typedef map<COMRES_INDICATOR, ID3D11DeviceChild*, value_cmp> GCRMAP;
	typedef map<string, ID3D11DeviceChild*> GCRMAP;
	typedef map<string, ID3D11Resource*> CONSTBUFMAP;

	struct GpuDX11CommonParameters
	{
		bool is_initialized;
		D3D_FEATURE_LEVEL dx11_featureLevel;
		DXGI_ADAPTER_DESC dx11_adapter;

#define MAXSTAMPS 20
		ID3D11Query* dx11qr_disjoint;
		ID3D11Query* dx11qr_timestamps[MAXSTAMPS];

		__ID3D11Device* dx11Device;
		__ID3D11DeviceContext* dx11DeviceImmContext;
#ifdef __DX_DEBUG_QUERY
		ID3D11InfoQueue* debug_info_queue;
#endif
		std::function<void(GpuhelperResType res_type, ID3D11DeviceChild* res)> __check_and_release = [](GpuhelperResType res_type, ID3D11DeviceChild* res)
		{
			res->Release();
			//switch (res_type)
			//{
			//case VERTEX_SHADER:			((ID3D11VertexShader*)res)->Release(); break;
			//case PIXEL_SHADER:			((ID3D11PixelShader*)res)->Release(); break;
			//case GEOMETRY_SHADER:		((ID3D11GeometryShader*)res)->Release(); break;
			//case COMPUTE_SHADER:		((ID3D11ComputeShader*)res)->Release(); break;
			////case BUFFER:				((ID3D11Buffer*)res)->Release(); break;
			//case DEPTHSTENCIL_STATE:	((ID3D11DepthStencilState*)res)->Release(); break;
			//case RASTERIZER_STATE:		((ID3D11RasterizerState2*)res)->Release(); break;
			//case SAMPLER_STATE:			((ID3D11SamplerState*)res)->Release(); break;
			//case INPUT_LAYOUT:			((ID3D11InputLayout*)res)->Release(); break;
			//case BLEND_STATE:			((ID3D11BlendState*)res)->Release(); break;
			//case ETC:
			//default:
			//	GMERRORMESSAGE("UNEXPECTED RESTYPE : ~GpuDX11CommonParameters");
			//}
		};

		GCRMAP dx11_cres;
		CONSTBUFMAP dx11_cbuf;
		void safe_release_res(const COMRES_INDICATOR& idc)
		{
			auto it = dx11_cres.find(idc.res_name);
			if (it != dx11_cres.end())
			{
				__check_and_release(idc.res_type, it->second);
				dx11_cres.erase(it);
			}
		}
		void safe_release_cbuf(const string& name)
		{
			auto it = dx11_cbuf.find(name);
			if (it != dx11_cbuf.end())
			{
				it->second->Release();
				dx11_cbuf.erase(it);
			}
		}
		void safe_set_res(const COMRES_INDICATOR& idc, ID3D11DeviceChild* res, bool enable_override = false)
		{
			auto it = dx11_cres.find(idc.res_name);
			if (it != dx11_cres.end())
			{
				if (enable_override)
					safe_release_res(idc);
				else
					GMERRORMESSAGE("ALREADY SET 1 ! : GpuDX11CommonParameters::safe_set_res");
			}
			for (auto it = dx11_cres.begin(); it != dx11_cres.end(); it++)
			{
				if (it->second == NULL)
					GMERRORMESSAGE("NULL RES DETECTED ! : GpuDX11CommonParameters::safe_set_res");
				if(it->second == res)
					GMERRORMESSAGE("ALREADY SET 2 ! : GpuDX11CommonParameters::safe_set_res");
			}

			//switch (idc.res_type)
			//{
			//case VERTEX_SHADER:			dx11_cres[idc] = (ID3D11VertexShader*)res; break;
			//case PIXEL_SHADER:			dx11_cres[idc] = (ID3D11PixelShader*)res; break;
			//case GEOMETRY_SHADER:		dx11_cres[idc] = (ID3D11GeometryShader*)res; break;
			//case COMPUTE_SHADER:		dx11_cres[idc] = (ID3D11ComputeShader*)res; break;
			////case BUFFER:				dx11_cres[idc] = (ID3D11Buffer*)res; break;
			//case DEPTHSTENCIL_STATE:	dx11_cres[idc] = (ID3D11DepthStencilState*)res; break;
			//case RASTERIZER_STATE:		dx11_cres[idc] = (ID3D11RasterizerState2*)res; break;
			//case SAMPLER_STATE:			dx11_cres[idc] = (ID3D11SamplerState*)res; break;
			//case INPUT_LAYOUT:			dx11_cres[idc] = (ID3D11InputLayout*)res; break;
			//case BLEND_STATE:			dx11_cres[idc] = (ID3D11BlendState*)res; break;
			//case ETC:
			//default:
			//	GMERRORMESSAGE("UNEXPECTED RESTYPE : ~GpuDX11CommonParameters");
			//}

			dx11_cres[idc.res_name] = res;
		}

		void safe_set_cbuf(const string& name, ID3D11Resource* res, bool enable_override = false)
		{
			auto it = dx11_cbuf.find(name);
			if (it != dx11_cbuf.end())
			{
				if (enable_override)
					safe_release_cbuf(name);
				else
					GMERRORMESSAGE("ALREADY SET 1 ! : GpuDX11CommonParameters::safe_set_cbuf");
			}
			for (auto it = dx11_cres.begin(); it != dx11_cres.end(); it++)
			{
				if (it->second == NULL)
					GMERRORMESSAGE("NULL RES DETECTED ! : GpuDX11CommonParameters::safe_set_cbuf");
				if (it->second == res)
					GMERRORMESSAGE("ALREADY SET 2 ! : GpuDX11CommonParameters::safe_set_cbuf");
			}
			
			dx11_cbuf[name] = res;
		}

		void* safe_get_res(const COMRES_INDICATOR& idc)
		{
			auto it = dx11_cres.find(idc.res_name);
			if (it == dx11_cres.end())
				GMERRORMESSAGE("NO RESOURCE ! : GpuDX11CommonParameters::safe_get_res");
			return it->second;
		}

		ID3D11Buffer* get_cbuf(const string& name)
		{
			auto it = dx11_cbuf.find(name);
			if (it == dx11_cbuf.end())
				GMERRORMESSAGE("NO RESOURCE ! : GpuDX11CommonParameters::get_cbuf");
			return (ID3D11Buffer*)it->second;
		}

		ID3D11SamplerState* get_sampler(const string& name)
		{
			return (ID3D11SamplerState*)safe_get_res(COMRES_INDICATOR(SAMPLER_STATE, name));
		}
		ID3D11RasterizerState2* get_rasterizer(const string& name)
		{
			return (ID3D11RasterizerState2*)safe_get_res(COMRES_INDICATOR(RASTERIZER_STATE, name));
		}
		ID3D11BlendState* get_blender(const string& name)
		{
			return (ID3D11BlendState*)safe_get_res(COMRES_INDICATOR(BLEND_STATE, name));
		}
		ID3D11DepthStencilState* get_depthstencil(const string& name)
		{
			return (ID3D11DepthStencilState*)safe_get_res(COMRES_INDICATOR(DEPTHSTENCIL_STATE, name));
		}

		ID3D11VertexShader* get_vshader(const string& name)
		{
			return (ID3D11VertexShader*)safe_get_res(COMRES_INDICATOR(VERTEX_SHADER, name));
		}
		ID3D11PixelShader* get_pshader(const string& name)
		{
			return (ID3D11PixelShader*)safe_get_res(COMRES_INDICATOR(PIXEL_SHADER, name));
		}
		ID3D11GeometryShader* get_gshader(const string& name)
		{
			return (ID3D11GeometryShader*)safe_get_res(COMRES_INDICATOR(GEOMETRY_SHADER, name));
		}
		ID3D11ComputeShader* get_cshader(const string& name)
		{
			return (ID3D11ComputeShader*)safe_get_res(COMRES_INDICATOR(COMPUTE_SHADER, name));
		}

		GpuDX11CommonParameters()
		{
			is_initialized = false;
			dx11_featureLevel = D3D_FEATURE_LEVEL_9_1;
			memset(&dx11_adapter, NULL, sizeof(DXGI_ADAPTER_DESC));

			dx11Device = NULL;
			dx11DeviceImmContext = NULL;
#ifdef __DX_DEBUG_QUERY
			debug_info_queue = NULL;
#endif
			for (int i = 0; i < MAXSTAMPS; i++)
				dx11qr_timestamps[i] = NULL;
			dx11qr_disjoint = NULL;
		}

		void Delete()
		{
			for (auto it = dx11_cres.begin(); it != dx11_cres.end(); it++)
			{
				it->second->Release();
				//switch (it->first.res_type)
				//{
				//case VERTEX_SHADER:			((ID3D11VertexShader*)it->second)->Release(); break;
				//case PIXEL_SHADER:			((ID3D11PixelShader*)it->second)->Release(); break;
				//case GEOMETRY_SHADER:		((ID3D11GeometryShader*)it->second)->Release(); break;
				//case COMPUTE_SHADER:		((ID3D11ComputeShader*)it->second)->Release(); break;
				////case BUFFER:				((ID3D11Buffer*)it->second)->Release(); break;
				//case DEPTHSTENCIL_STATE:	((ID3D11DepthStencilState*)it->second)->Release(); break;
				//case RASTERIZER_STATE:		((ID3D11RasterizerState2*)it->second)->Release(); break;
				//case SAMPLER_STATE:			((ID3D11SamplerState*)it->second)->Release(); break;
				//case INPUT_LAYOUT:			((ID3D11InputLayout*)it->second)->Release(); break;
				//case BLEND_STATE:			((ID3D11BlendState*)it->second)->Release(); break;
				//case ETC:
				//default:
				//	GMERRORMESSAGE("UNEXPECTED RESTYPE : ~GpuDX11CommonParameters");
				//}
			}
			dx11_cres.clear();

			for (auto it = dx11_cbuf.begin(); it != dx11_cbuf.end(); it++)
			{
				it->second->Release();
			}
			dx11_cbuf.clear();

#ifdef __DX_DEBUG_QUERY
			if(debug_info_queue)
				debug_info_queue->Release();
#endif
			if (dx11qr_disjoint)
			{
				dx11qr_disjoint->Release();
				for (int i = 0; i < MAXSTAMPS; i++)
					dx11qr_timestamps[i]->Release();
				for (int i = 0; i < MAXSTAMPS; i++)
					dx11qr_timestamps[i] = NULL;
				dx11qr_disjoint = NULL;
			}
		}
	};

	int InitializePresettings(VmGpuManager* pCGpuManager, GpuDX11CommonParameters* gpu_params);
	void DeinitializePresettings();

	// volume/block structure
	bool UpdateOtfBlocks(GpuRes& gres, const VmVObjectVolume* vobj, const VmTObject	* tobj,
		const bool update_blks, const int sculpt_value, LocalProgress* progress = NULL);
	bool UpdateOtfBlocks(GpuRes& gres, const VmVObjectVolume* main_vobj, const VmVObjectVolume* mask_vobj,
		const map<int, VmTObject*>& mapTObjects, const int main_tmap_id, const double* mask_tmap_ids, const int num_mask_tmap_ids,
		const bool update_blks, const bool use_mask_otf, const int sculpt_value, LocalProgress* progress = NULL);
	bool UpdateMinMaxBlocks(GpuRes& gres_min, GpuRes& gres_max, const VmVObjectVolume* vobj, LocalProgress* progress = NULL);
	// bool UpdateAOMask(const VmVObjectVolume* vobj, LocalProgress* progress = NULL); // to do
	bool UpdateVolumeModel(GpuRes& gres, const VmVObjectVolume* vobj, const bool use_nearest_max, LocalProgress* progress = NULL);

	bool UpdateTMapBuffer(GpuRes& gres, const VmTObject* main_tobj,
		const map<int, VmTObject*>& tobj_map, const double* series_ids, const double* visible_mask, const int otf_series, const bool update_tf_content, LocalProgress* progress = NULL);

	// primitive structure
	bool UpdatePrimitiveModel(GpuRes& gres_vtx, GpuRes& gres_idx, map<string, GpuRes>& map_gres_texs, VmVObjectPrimitive* pobj, LocalProgress* progress = NULL);

#define UPFB_SYSOUT 0x1
#define UPFB_RAWBYTE 0x2 // buffer only
#define UPFB_MIPMAP 0x4  // texture only
#define UPFB_HALF 0x8    // texture only
#define UPFB_HALF_W 0x10    // texture only
#define UPFB_HALF_H 0x20    // texture only
#define UPFB_NFPP_BUFFERSIZE 0x40 // buffer only
#define UPFB_NFPP_TEXTURESTACK 0x80 // texture only
	// framebuffer structure
	bool UpdateFrameBuffer(GpuRes& gres, const VmIObject* iobj,
		const string& res_name,
		const GpuResType gres_type,
		const uint bind_flag,
		const uint dx_format,
		const int fb_flag,
		const int num_frags_perpixel = 1,
		const int structured_stride = 0);

	bool CheckOtfAndVolBlobkUpdate(VmVObjectVolume* vobj, VmTObject* tobj);

#define ZERO_SET(T) T(){memset(this, 0, sizeof(T));}

#define MAX_LAYERS 8
	struct Fragment
	{
		uint i_vis;
		float z;
		float zthick;
		float opacity_sum;
	};

	struct FragmentArray
	{
		Fragment frags[MAX_LAYERS];
	};

	struct RenderObjInfo
	{
		VmVObjectPrimitive* pobj;
		bool is_wireframe;
		vmfloat4 fColor;

		bool is_annotation_obj;
		bool has_texture_img;
		bool use_vertex_color;
		bool show_outline;
		bool abs_diffuse;
		float vzthickness;

		int num_safe_loopexit;

		RenderObjInfo()
		{
			pobj = NULL;
			is_wireframe = false;
			fColor = vmfloat4(1.f);
			is_annotation_obj = false;
			use_vertex_color = true;
			show_outline = false;
			abs_diffuse = false;
			has_texture_img = false;
			vzthickness = 0;
			num_safe_loopexit = 100;
		}
	};

	struct CB_CameraState
	{
		vmmat44f mat_ws2ss;
		vmmat44f mat_ss2ws;

		vmfloat3 pos_cam_ws;
		uint rt_width;

		vmfloat3 dir_view_ws;
		uint rt_height;

		float cam_vz_thickness;
		uint k_value; // used for max k for DK+B algorithm
		// 1st bit : 0 (orthogonal), 1 : (perspective)
		// 2nd bit : for RT to k-buffer : 0 (just RT), 1 : (after silhouette processing)
		// 3rd bit : for dynamic K value // deprecated... (this will be treated as a separate shader
		// 4th bit : for storing the final fragments to the k buffer, which is used for sequentially coming renderer (e.g., DVR) : 0 (skipping), 1 (storing)
		// 5th bit : only for DFB without (S)FM. stores all fragments into the framebuffer (using offset table)
		uint cam_flag;
		uint iSrCamDummy__0; // used for 1) A-Buffer prefix computations /*deprecated*/ or 2) beta (asfloat) for merging operation

		float near_plane;
		float far_plane;
		uint iSrCamDummy__1; // used for the level of MIPMAP generation
		uint iSrCamDummy__2; // scaling factor (asfloat) for the z-thickness value determined by the z-resolution

		ZERO_SET(CB_CameraState)
	};

	struct CB_EnvState
	{
		// Global Lighting Effect

		vmfloat3 pos_light_ws;
		// 1st bit : 0 (parallel), 1 : (spot)
		// 2nd bit : 0 (only polygons for SSAO), 1 : (volume G buffer for SSAO)
		// 10th bit : 0 (no SSAO output to render buffer), 1: (SSAO output to render buffer)
		// 11th~13th bit : 0~7th layer of SSAO
		uint env_flag;

		vmfloat3 dir_light_ws;
		uint num_lights;

		// associated colors
		vmfloat4 ltint_ambient;
		vmfloat4 ltint_diffuse;
		vmfloat4 ltint_spec;

		vmmat44f	mat_ws2ls_smap;	// for shadow : Sample Depth Map
		vmmat44f	mat_ws2wls_smap;	// for shadow : Depth Comparison And Storage

		float r_kernel_ao;
		int num_dirs;
		int num_steps;
		float tangent_bias;

		float ao_intensity;
		uint env_dummy_0;
		uint env_dummy_1;
		uint env_dummy_2;

		float dof_lens_r;
		float dof_lens_F;
		float dof_focus_z;
		int dof_lens_ray_num_samples;

		ZERO_SET(CB_EnvState)
	};

	struct CB_ClipInfo
	{
		vmmat44f mat_clipbox_ws2bs;  // To Clip Box Space (BS)
		vmfloat3 pos_clipbox_max_bs;
		// 1st bit : 0 (No) 1 (Clip Box)
		// 2nd bit : 0 (No) 1 (Clip plane)
		uint clip_flag;

		vmfloat3 pos_clipplane;
		uint ci_dummy_1;
		vmfloat3 vec_clipplane;
		uint ci_dummy_2;

		ZERO_SET(CB_ClipInfo)
	};

	struct CB_PolygonObject
	{
		vmmat44f mat_os2ws;
		vmmat44f mat_os2ps; // optional
		
		// if 1) color map model, or 2) vertex color model
		// use Ka as material description for shading.
		vmfloat3 Ka;	
		float Ns;
		vmfloat3 Kd;
		float alpha;
		vmfloat3 Ks;
		// 1st bit : g_texRgbaArray
		// 2nd bit : g_tex2D_KA
		// 3rd bit : g_tex2D_KD
		// 4th bit : g_tex2D_KS
		// 5th bit : g_tex2D_NS
		// 6th bit : g_tex2D_BUMP
		// 7th bit : g_tex2D_D
		uint tex_map_enum;

		// 1st bit : 0 (shading color to RT) 1 (normal to RT for the purpose of silhouette rendering)
		// 4th bit : 0 (Set texture0 to texture0) 1 (Set global_color to texture0)
		// 6th bit : 0 (Diffuse abs) 1 (Diffuse max)
		// 10th bit : 0 (No XFlip) 1 (XFlip)
		// 11th bit : 0 (No XFlip) 1 (YFlip)
		// 20th bit : 0 (No Dashed Line) 1 (Dashed Line)
		// 21th bit : 0 (Transparent Dash) 1 (Dash As Color Inverted)
		// 23th bit : 0 (static alpha) 1 (dynamic alpha using mask t50) ... mode 1
		// 24th bit : 0 (static alpha) 1 (dynamic alpha using mask t50) ... mode 2
		// 31~32th bit : max components for dashed line : 0 ==> x, 1 ==> y, 2 ==> z
		uint pobj_flag;
		uint num_letters;
		float dash_interval;
		float depth_forward_bias;	// deprecated!

		float pix_thickness; // only for POINT and LINE TOPOLOGY
		float vz_thickness;
		uint num_safe_loopexit;
		uint pobj_dummy_0;

		ZERO_SET(CB_PolygonObject)
	};

	struct CB_VolumeObject
	{
		// Volume Information and Clipping Information
		vmmat44f mat_ws2ts;	// for Sampling and Ray Traversing

		vmmat44f mat_alignedvbox_tr_ws2bs;

		vmfloat3 pos_alignedvbox_max_bs;
		// 1st bit : 0 (use the input normal) 1 (invert the input normal) ==> will be deprecated! (always faces to camera)
		// 24~31bit : Sculpt Mask Value (1 byte)
		uint	vobj_flag;

		vmfloat3 vec_grad_x; // ts
		float value_range;
		vmfloat3 vec_grad_y; // ts
		float sample_dist;
		vmfloat3 vec_grad_z; // ts
		float opacity_correction;
		vmfloat3 vol_size; // volume size stored in GPU memory
		float vz_thickness;

		vmfloat3 volblk_size_ts;
		float volblk_value_range;

		uint iso_value;
		float ao_intensity;
		uint vobj_dummy_1;
		uint vobj_dummy_2;

		// light properties
		vmfloat4 pb_shading_factor;	// x : Ambient, y : Diffuse, z : Specular, w : Specular power

		ZERO_SET(CB_VolumeObject)
	};

	struct CB_RenderingEffect // normally for each object
	{
		// 1st bit : AO or Not , 2nd bit : Anisotropic BRDF or Not , 3rd bit : Apply Shading Factor or Not
		// NA ==> 4th bit : 0 : Normal Curvature Map (2D), 1 : Apply Concaveness
		// NA ==> 5th bit : Concaveness Direction or Not
		uint rf_flag;
		uint outline_mode;
		float curvature_kernel_radius;
		uint rf_dummy_0;

		float brdf_diffuse_ratio;
		float brdf_reft_ratio;
		float brdf_expw_u;
		float brdf_expw_v;

		float shadowmap_occusion_w; // for shadow
		float shadowmap_depth_bias; // for shadow
		float occ_radius;
		uint occ_num_rays;

		ZERO_SET(CB_RenderingEffect)
	};

	struct CB_VolumeRenderingEffect // normally for each volume
	{
		float clip_plane_intensity;
		float attribute_voxel_sharpness;
		float mod_grad_mag_scale;	// for modulation 
		float mod_max_grad_size; // for modulation 

		float occ_sample_dist_scale; // for occlusion
		float sdm_sample_dist_scale; // for shadow
		uint vrf_dummy_0;
		uint vrf_dummy_1;

		ZERO_SET(CB_VolumeRenderingEffect)
	};

	struct CB_TMAP
	{
		vmfloat4 last_color;

		uint		first_nonzeroalpha_index; // For ESS
		uint		last_nonzeroalpha_index;
		uint		tmap_size;
		float		mapping_v_min;

		float		mapping_v_max;
		uint		tm_dummy_0;
		uint		tm_dummy_1;
		uint		tm_dummy_2;

		ZERO_SET(CB_TMAP)
	};

	struct MomentOIT
	{
		vmfloat4 wrapping_zone_parameters;

		float overestimation;
		float moment_bias;
		vmfloat2 warp_nf;
		
		ZERO_SET(MomentOIT)
	};

	struct HotspotMask
	{
		vmint2 pos_center;
		float radius;
		int smoothness;
		float thick;
		float kappa_t;
		float kappa_s;
		float bnd_thick;

		int flag;
		vmfloat3 pos_spotcenter;

		float in_depth_vis;
		int __dummy0;
		int __dummy1;
		int __dummy2;
	};
	struct CB_HotspotMask
	{
		HotspotMask mask_info_[2];
		//int pos_centerx_[4];
		//int pos_centery_[4];
		//int radius_[4];
		//int smoothness_[4];
		//float thick_[4];
		//float kappa_t_[4];
		//float kappa_s_[4];
		//float bnd_thick_[4];
	};

	// Compute Constant Buffers //
	// global 
	void SetCb_Camera(CB_CameraState& cb_cam, vmmat44f& matWS2PS, vmmat44f& matWS2SS, vmmat44f& matSS2WS, VmCObject* ccobj, const vmint2& fb_size, const int k_value, const float vz_thickness);
	void SetCb_Env(CB_EnvState& cb_env, VmCObject* ccobj, VmFnContainer* _fncontainer, vmfloat3 simple_light_intensities);
	// each object
	void SetCb_TMap(CB_TMAP& cb_tmap, VmTObject* tobj);
	//bool SetCbVrShadowMap(CB_VrShadowMap* pCBVrShadowMap, CB_VrCameraState* pCBVrCamStateForShadowMap, vmfloat3 f3PosOverviewBoxMinWS, vmfloat3 f3PosOverviewBoxMaxWS, map<string, void*>* pmapCustomParameter);
	void SetCb_ClipInfo(CB_ClipInfo& cb_clip, VmVObject* obj, VmLObject* lobj);
	void SetCb_VolumeObj(CB_VolumeObject& cb_volume, VmVObjectVolume* vobj, VmLObject* lobj, VmFnContainer* _fncontainer, const bool high_samplerate, const vmint3& vol_size, const int iso_value, const float volblk_valuerange, const int sculpt_index = -1);
	void SetCb_PolygonObj(CB_PolygonObject& cb_polygon, VmVObjectPrimitive* pobj, VmLObject* lobj,
		const vmmat44f& matOS2WS, const vmmat44f& matWS2SS, const vmmat44f& matWS2PS,
		const RenderObjInfo& rendering_obj_info, const double default_point_thickness, const double default_line_thickness, const double default_surfel_size);
	void SetCb_RenderingEffect(CB_RenderingEffect& cb_reffect, VmVObject* obj, VmLObject* lobj, const RenderObjInfo& rendering_obj_info);
	void SetCb_VolumeRenderingEffect(CB_VolumeRenderingEffect& cb_vreffect, VmVObjectVolume* vobj, VmLObject* lobj);
	
	void SetCb_HotspotMask(CB_HotspotMask& cb_hsmask, VmFnContainer* _fncontainer, const vmmat44f& matWS2SS);

	bool Compile_Hlsl(const string& str, const string& entry_point, const string& shader_model, D3D10_SHADER_MACRO* defines, void** sm);

	void __TestOutErrors();

	bool CollisionCheck(const vmmat44f& matWS2OS, const AaBbMinMax& aabb_os, const vmfloat3& ray_origin_ws, const vmfloat3& ray_dir_ws);
}

#define GETVS(NAME) dx11CommonParams->get_vshader(#NAME)
#define GETPS(NAME) dx11CommonParams->get_pshader(#NAME)
#define GETGS(NAME) dx11CommonParams->get_gshader(#NAME)
#define GETCS(NAME) dx11CommonParams->get_cshader(#NAME)