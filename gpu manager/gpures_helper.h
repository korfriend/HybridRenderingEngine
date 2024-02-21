#pragma once
//#include "GpuManager.h"
#include "../gpu_common_res.h"
#include <functional>

using namespace fncontainer;
//#include <map>
//#define __DX_DEBUG_QUERY


#ifdef DX11_3
#define __ID3D11Device ID3D11Device3
#define __ID3D11DeviceContext ID3D11DeviceContext3
#define __DLLNAME "vismtv_inbuilt_renderergpudx.dll"
#elif defined(DX11_0)
#define __ID3D11Device ID3D11Device
#define __ID3D11DeviceContext ID3D11DeviceContext
#ifdef _DEBUG
#define __DLLNAME "vismtv_inbuilt_renderergpudx.dll"
#else
#define __DLLNAME "vismtv_inbuilt_renderergpudx11_0.dll"
#endif
#elif defined(DX10_0)
#define __ID3D11Device ID3D11Device
#define __ID3D11DeviceContext ID3D11DeviceContext
#ifdef _DEBUG
#define __DLLNAME "vismtv_inbuilt_renderergpudx.dll"
#else
#define __DLLNAME "vismtv_inbuilt_renderergpudx10_0.dll"
#endif
#endif

#define TRANSPOSE(A) glm::transpose(A)

namespace grd_helper
{
	using namespace std;
	
	template<typename T>
	constexpr T AlignTo(T value, T alignment)
	{
		return ((value + alignment - T(1)) / alignment) * alignment;
	}
	template<typename T>
	constexpr bool IsAligned(T value, T alignment)
	{
		return value == AlignTo(value, alignment);
	}

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
			string a_str = to_string((int)a.res_type) + "_" + a.res_name;
			string b_str = to_string((int)b.res_type) + "_" + b.res_name;
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

	struct GPUBVH
	{
		// Mesh Object BVH intersection resources:
		int objId = 0;
		ID3D11Buffer* bvhNodeBuffer = NULL;
		ID3D11Buffer* bvhParentBuffer = NULL;
		ID3D11Buffer* bvhFlagBuffer = NULL;
		ID3D11Buffer* primitiveCounterBuffer = NULL;
		ID3D11Buffer* primitiveIDBuffer = NULL;
		ID3D11Buffer* primitiveBuffer = NULL;
		ID3D11Buffer* primitiveMortonBuffer = NULL;
		uint32_t primitiveCapacity = 0;
		//bool IsValid() const { return primitiveCounterBuffer.IsValid(); }

		void Update();
		void Build() const;

		void Clear();

		static void Initialize();
	};

	struct GpuDX11CommonParameters
	{
		bool is_initialized;
		D3D_FEATURE_LEVEL dx11_featureLevel;
		DXGI_ADAPTER_DESC dx11_adapter;

#define MAXSTAMPS 50
		ID3D11Query* dx11qr_disjoint;
		ID3D11Query* dx11qr_timestamps[MAXSTAMPS];
		map<string, vmint2> profile_map;

		__ID3D11Device* dx11Device;
		__ID3D11DeviceContext* dx11DeviceImmContext;
#ifdef __DX_DEBUG_QUERY
		ID3D11InfoQueue* debug_info_queue;
#endif

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
				((ID3D11Buffer*)it->second)->Release();
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
					VMERRORMESSAGE("ALREADY SET 1 ! : GpuDX11CommonParameters::safe_set_res");
			}
			for (auto it = dx11_cres.begin(); it != dx11_cres.end(); it++)
			{
				if (it->second == NULL)
					VMERRORMESSAGE("NULL RES DETECTED ! : GpuDX11CommonParameters::safe_set_res");
				if(it->second == res)
					VMERRORMESSAGE("ALREADY SET 2 ! : GpuDX11CommonParameters::safe_set_res");
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
			//	VMERRORMESSAGE("UNEXPECTED RESTYPE : ~GpuDX11CommonParameters");
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
					VMERRORMESSAGE("ALREADY SET 1 ! : GpuDX11CommonParameters::safe_set_cbuf ==> " + name);
			}
			for (auto it = dx11_cres.begin(); it != dx11_cres.end(); it++)
			{
				if (it->second == NULL)
					VMERRORMESSAGE("NULL RES DETECTED ! : GpuDX11CommonParameters::safe_set_cbuf ==> " + it->first);
				if (it->second == res)
					VMERRORMESSAGE("ALREADY SET 2 ! : GpuDX11CommonParameters::safe_set_cbuf ==> " + it->first);
			}
			
			dx11_cbuf[name] = res;
		}

		void* safe_get_res(const COMRES_INDICATOR& idc)
		{
			auto it = dx11_cres.find(idc.res_name);
			if (it == dx11_cres.end())
				VMERRORMESSAGE("NO RESOURCE ! : GpuDX11CommonParameters::safe_get_res ==> " + idc.res_name);
			return it->second;
		}

		ID3D11Buffer* get_cbuf(const string& name)
		{
			auto it = dx11_cbuf.find(name);
			if (it == dx11_cbuf.end())
				VMERRORMESSAGE("NO RESOURCE ! : GpuDX11CommonParameters::get_cbuf ==> " + name);
			return (ID3D11Buffer*)it->second;
		}

		ID3D11SamplerState* get_sampler(const string& name)
		{
			return (ID3D11SamplerState*)safe_get_res(COMRES_INDICATOR(GpuhelperResType::SAMPLER_STATE, name));
		}
		ID3D11RasterizerState2* get_rasterizer(const string& name)
		{
			return (ID3D11RasterizerState2*)safe_get_res(COMRES_INDICATOR(GpuhelperResType::RASTERIZER_STATE, name));
		}
		ID3D11BlendState* get_blender(const string& name)
		{
			return (ID3D11BlendState*)safe_get_res(COMRES_INDICATOR(GpuhelperResType::BLEND_STATE, name));
		}
		ID3D11DepthStencilState* get_depthstencil(const string& name)
		{
			return (ID3D11DepthStencilState*)safe_get_res(COMRES_INDICATOR(GpuhelperResType::DEPTHSTENCIL_STATE, name));
		}

		ID3D11VertexShader* get_vshader(const string& name)
		{
			return (ID3D11VertexShader*)safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, name));
		}
		ID3D11PixelShader* get_pshader(const string& name)
		{
			return (ID3D11PixelShader*)safe_get_res(COMRES_INDICATOR(GpuhelperResType::PIXEL_SHADER, name));
		}
		ID3D11GeometryShader* get_gshader(const string& name)
		{
			return (ID3D11GeometryShader*)safe_get_res(COMRES_INDICATOR(GpuhelperResType::GEOMETRY_SHADER, name));
		}
		ID3D11ComputeShader* get_cshader(const string& name)
		{
			return (ID3D11ComputeShader*)safe_get_res(COMRES_INDICATOR(GpuhelperResType::COMPUTE_SHADER, name));
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

		bool gpu_profile = false;
		void GpuProfile(const string& profile_name, const bool is_closed = false) {
			if (gpu_profile)
			{
				int stamp_idx = 0;
				auto it = profile_map.find(profile_name);
				if (it == profile_map.end()) {
					assert(is_closed == false);
					int gpu_profilecount = (int)profile_map.size() * 2;
					profile_map[profile_name] = vmint2(gpu_profilecount, -1);
					stamp_idx = gpu_profilecount;
				}
				else {
					//assert(it->second.y == -1 && is_closed == true);
					it->second.y = it->second.x + 1;
					stamp_idx = it->second.y;
				}

				dx11DeviceImmContext->End(dx11qr_timestamps[stamp_idx]);
				//gpu_profilecount++;
			}
		};

		int GpuQueryProfile(const string& profile_name, const bool is_closed = false) {
			int stamp_idx = 0;
			auto it = profile_map.find(profile_name);
			if (it == profile_map.end()) {
				assert(is_closed == false);
				int gpu_profilecount = (int)profile_map.size() * 2;
				profile_map[profile_name] = vmint2(gpu_profilecount, -1);
				stamp_idx = gpu_profilecount;
			}
			else {
				//assert(it->second.y == -1 && is_closed == true);
				it->second.y = it->second.x + 1;
				stamp_idx = it->second.y;
			}

			dx11DeviceImmContext->End(dx11qr_timestamps[stamp_idx]);
			//gpu_profilecount++;

			return stamp_idx;
		};

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
				//	VMERRORMESSAGE("UNEXPECTED RESTYPE : ~GpuDX11CommonParameters");
				//}
			}
			dx11_cres.clear();

			for (auto it = dx11_cbuf.begin(); it != dx11_cbuf.end(); it++)
			{
				((ID3D11Buffer*)it->second)->Release();
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

	struct Particle
	{
		vmfloat3 position;
		float mass;
		vmfloat3 force;
		float rotationalVelocity;
		vmfloat3 velocity;
		float maxLife;
		vmfloat2 sizeBeginEnd;
		float life;
		uint color;
	};

	struct ParticleCounters
	{
		uint aliveCount;
		uint deadCount;
		uint realEmitCount;
		uint aliveCount_afterSimulation;
		uint culledCount;
		uint cellAllocator;
	};

	struct IndirectDrawArgsInstanced
	{
		uint VertexCountPerInstance;
		uint InstanceCount;
		uint StartVertexLocation;
		uint StartInstanceLocation;
	};
	struct IndirectDrawArgsIndexedInstanced
	{
		uint IndexCountPerInstance;
		uint InstanceCount;
		uint StartIndexLocation;
		int BaseVertexLocation;
		uint StartInstanceLocation;
	};
	struct IndirectDispatchArgs
	{
		uint ThreadGroupCountX;
		uint ThreadGroupCountY;
		uint ThreadGroupCountZ;
	};

	int InitializePresettings(VmGpuManager* pCGpuManager, GpuDX11CommonParameters* gpu_params);
	void DeinitializePresettings();

	// volume/block structure
	bool UpdateOtfBlocks(GpuRes& gres, VmVObjectVolume* main_vobj, VmVObjectVolume* mask_vobj,
		VmObject* tobj, const int sculpt_value, LocalProgress* progress = NULL);
	bool UpdateMinMaxBlocks(GpuRes& gres_min, GpuRes& gres_max, const VmVObjectVolume* vobj, LocalProgress* progress = NULL);
	// bool UpdateAOMask(const VmVObjectVolume* vobj, LocalProgress* progress = NULL); // to do
	bool UpdateVolumeModel(GpuRes& gres, VmVObjectVolume* vobj, const bool use_nearest_max, bool heuristicResize = true, LocalProgress* progress = NULL);

	bool UpdateTMapBuffer(GpuRes& gres, VmObject* tobj, const bool isPreInt = false, LocalProgress* progress = NULL);

	// primitive structure
	bool UpdatePrimitiveModel(GpuRes& gres_vtx, GpuRes& gres_idx, map<string, GpuRes>& map_gres_texs, VmVObjectPrimitive* pobj, VmObject* imgObj = NULL, bool* hasTextureMap = NULL, LocalProgress* progress = NULL);

#define UPFB_SYSOUT 0x1
#define UPFB_RAWBYTE 0x2 // buffer only
#define UPFB_MIPMAP 0x4  // texture only
#define UPFB_HALF 0x8    // texture only
#define UPFB_HALF_W 0x10    // texture only
#define UPFB_HALF_H 0x20    // texture only
#define UPFB_NFPP_BUFFERSIZE 0x40 // buffer only //
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

	bool UpdateCustomBuffer(GpuRes& gres, VmObject* srcObj, const string& resName, const void* bufPtr, const int numElements, DXGI_FORMAT dxFormat, const int type_bytes, LocalProgress* progress = NULL);

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

	//struct RenderObjInfo
	//{
	//	VmVObjectPrimitive* pobj;
	//	bool is_wireframe;
	//	vmfloat4 fColor;
	//
	//	bool is_annotation_obj;
	//	bool has_texture_img;
	//	bool use_vertex_color;
	//	bool abs_diffuse;
	//	float vzthickness;
	//	int outline_thickness;
	//	float outline_depthThres;
	//	vmfloat3 outline_color;
	//
	//	int num_safe_loopexit;
	//
	//	RenderObjInfo()
	//	{
	//		pobj = NULL;
	//		is_wireframe = false;
	//		fColor = vmfloat4(1.f);
	//		is_annotation_obj = false;
	//		use_vertex_color = true;
	//		outline_thickness = 0;
	//		outline_depthThres = 10000.f;
	//		outline_color = vmfloat3(1.f, 1.f, 1.f);
	//		abs_diffuse = false;
	//		has_texture_img = false;
	//		vzthickness = 0;
	//		num_safe_loopexit = 100;
	//	}
	//};

	struct LightSource {
		bool is_on_camera = false; 
		bool is_pointlight = false; 
		vmfloat3 light_dir = vmfloat3(0); 
		vmfloat3 light_pos = vmfloat3(0); 

		vmfloat3 light_ambient_color = vmfloat3(1);
		vmfloat3 light_diffuse_color = vmfloat3(1);
		vmfloat3 light_specular_color = vmfloat3(1);
	};

	struct GlobalLighting {
		bool apply_ssao = false; 
		float ssao_r_kernel = 1.f; 
		int ssao_num_dirs = 4; 
		int ssao_num_steps = 4; 
		float ssao_tangent_bias = (float)(VM_PI / 6.0); 
		float ssao_intensity = 0.5f; 
		bool ssao_blur = true;
		int ssao_debug = 0;
	};

	struct LensEffect {
		bool apply_ssdof = false; 
		float dof_lens_r = 3.f;
		float dof_lens_F = 10.f; 
		int dof_ray_num_samples = 8; 
		float dof_focus_z = 20.f; 
	};

	struct CB_CameraState
	{
		vmmat44f mat_ws2ss;
		vmmat44f mat_ss2ws;
		vmmat44f mat_ws2cs;

		vmfloat3 pos_cam_ws;
		uint rt_width;

		vmfloat3 dir_view_ws;
		uint rt_height;

		float cam_vz_thickness;
		uint k_value; // used for max k for DK+B algorithm
		// 0- bit : 0 : (orthogonal), 1 : (perspective)
		// 1- bit : for RT to k-buffer : 0 (just RT), 1 : (after silhouette processing)
		// 2- bit : for dynamic K value // deprecated... (this will be treated as a separate shader
		// 3- bit : for storing the final fragments to the k buffer, which is used for sequentially coming renderer (e.g., DVR) : 0 (skipping), 1 (storing)
		// 4- bit : only for DFB without (S)FM. stores all fragments into the framebuffer (using offset table)
		// 5- bit : 0 : (normal rendering), 1 : picking mode
		// 6- bit : 0 : (stores the final RGBA and depth to RT), 1 : (does not store them) // will be deprecated
		// 7- bit : 0 : full raycaster, 1 : half raycaster for 4x faster volume rendering (dither)
		// 8- bit : 0 : only OIT fragments, 1 : foremostopaque or singlelayer effect ...
		// 9- bit : 0 : outlint mode (solid), 1 : outlint mode (gradient alpha)
		uint cam_flag;
		// used for 
		// 1) A-Buffer prefix computations /*deprecated*/ or 2) beta (asfloat) for merging operation
		// 2) Second Layer Blending : 1st 8 bit for Pattern Interval (pixels), 2nd 8 bit for Blending
		uint iSrCamDummy__0; 

		float near_plane;
		float far_plane;
		// used for 
		// 1) the level of MIPMAP generation (only for SSAO rendering), or 2) picking xy (16bit, 16bit)
		// 3) outline's colorRGB and thicknessPix
		uint iSrCamDummy__1; 
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

		vmmat44f	mat_ws2ls_smap;	// for shadow : Sample Depth Map (ws2ss)
		vmmat44f	mat_ls2ws_smap;	// for shadow : Depth Comparison (ss2ws)

		float r_kernel_ao;
		int num_dirs;
		int num_steps;
		float tangent_bias;

		float ao_intensity;
		uint num_safe_loopexit;
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
		vmfloat3 pos_clipplane;
		// 1st bit : 0 (No) 1 (Clip Box)
		// 2nd bit : 0 (No) 1 (Clip plane)
		uint clip_flag;

		vmfloat3 vec_clipplane;
		uint ci_dummy_1;

		ZERO_SET(CB_ClipInfo)
	};

	struct CB_PolygonObject
	{
		vmmat44f mat_os2ws;
		vmmat44f mat_ws2os;
		vmmat44f mat_os2ps;
		
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
		// 7th bit : 0 (slicer with solid filling) 1 (slicer does not fill)
		// 8th bit : 0 (normal color map) 1 (for windowing slice)
		// 10th bit : 0 (No XFlip) 1 (XFlip)
		// 11th bit : 0 (No XFlip) 1 (YFlip)
		// 20th bit : 0 (No Dashed Line) 1 (Dashed Line)
		// 21th bit : 0 (Transparent Dash) 1 (Dash As Color Inverted)
		// 23th bit : 0 (static alpha) 1 (dynamic alpha using mask t50) ... mode 1
		// 24th bit : 0 (static alpha) 1 (dynamic alpha using mask t50) ... mode 2
		// 31~32th bit : max components for dashed line : 0 ==> x, 1 ==> y, 2 ==> z
		uint pobj_flag;
		uint num_letters;	// for text object 
		float dash_interval;
		float depth_thres;	// for outline!

		float pix_thickness; // 1) for POINT and LINE TOPOLOGY, 2) slicer's cutting outline thickness
		float vz_thickness;
		uint pobj_dummy_0; // 1) actor_id used for picking, 2) outline color, 3) iso_value for difference map
		uint pobj_dummy_1;

		ZERO_SET(CB_PolygonObject)
	};

	struct CB_VolumeObject
	{
		// Volume Information and Clipping Information
		vmmat44f mat_ws2ts;	// for Sampling and Ray Traversing

		vmmat44f mat_alignedvbox_tr_ws2bs;

		float grad_max; // 
		float grad_scale; // 
		float kappa_i; // 
		float kappa_s; // 

		// note vec_grad_x,y,z need to be computed w.r.t. vol_size (volume size stored in GPU memory)
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

		// 0 bit : 0 (use the input normal) 1 (invert the input normal) ==> will be deprecated! (always faces to camera)
		// 1 bit : outline vr (1) or not (0)
		// 19 bit : ghost surface (1) or not (0)
		// 20 bit : hotspot visible (1) or not (0)
		// 24~31bit : Sculpt Mask Value (1 byte)
		uint vobj_flag;
		uint iso_value;
		uint v_dummy0; 
		uint v_dummy1;

		// light properties
		vmfloat4 pb_shading_factor;	// x : Ambient, y : Diffuse, z : Specular, w : Specular power

		vmfloat3 mask_vol_size; // volume size stored in GPU memory
		float mask_value_range;

		ZERO_SET(CB_VolumeObject)
	};

	// future.. change to material...
	struct CB_Material // normally for each object
	{
		// 1st bit : AO or Not , 2nd bit : Anisotropic BRDF or Not , 3rd bit : Apply Shading Factor or Not
		// NA ==> 4th bit : 0 : Normal Curvature Map (2D), 1 : Apply Concaveness
		// NA ==> 5th bit : Concaveness Direction or Not
		uint rf_flag;
		uint outline_mode; // deprecated
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

		float ao_intensity;
		uint rf_dummy_1;
		uint rf_dummy_2;
		uint rf_dummy_3;

		ZERO_SET(CB_Material)
	};

	struct CB_VolumeMaterial // normally for each volume
	{
		float clip_plane_intensity;
		float attribute_voxel_sharpness;
		float vrf_dummy_2;	
		float vrf_dummy_3; 

		float occ_sample_dist_scale; // for occlusion
		float sdm_sample_dist_scale; // for shadow
		// 0-bit : 0 - normal surface hit, 1 - jittering 
		uint flag;
		uint vrf_dummy_1;

		ZERO_SET(CB_VolumeMaterial)
	};

	struct CB_TMAP
	{
		vmfloat4 last_color;

		uint		first_nonzeroalpha_index; // For ESS
		uint		last_nonzeroalpha_index;
		uint		tmap_size_x;
		float		mapping_v_min; 

		float		mapping_v_max;
		// 1st bit set, then color clip
		uint		flag;	
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

	struct CB_CurvedSlicer
	{
		vmfloat3 posTopLeftCOS;
		int numCurvePoints;
		vmfloat3 posTopRightCOS;
		float planeHeight;
		vmfloat3 posBottomLeftCOS;
		float thicknessPlane; // use cam's far_plane
		vmfloat3 posBottomRightCOS;
		uint __dummy0;
		vmfloat3 planeUp; // WS, length is planePitch
		uint flag; // 1st bit : isRightSide
	};

	struct CB_TestBuffer
	{
		//uint testIntValues[16];
		//float testFloatValues[16];

		uint testA;
		uint testB;
		uint testC;
		uint testD;
	};

	struct CB_Particle_Blob
	{
		vmfloat4 xyzr_spheres[4];
		vmint4 color_spheres;
		float smoothCoeff;
		vmfloat3 minRoiCube;
		uint dummy1;
		vmfloat3 maxRoiCube;
	};

	struct CB_Undercut
	{
		vmmat44f	mat_ws2lss_udc_map;	
		vmmat44f	mat_ws2lcs_udc_map;

		vmfloat3	undercutDir;
		uint		icolor;
	};

	// CS ¿¡¼­ Particle Buffer Update (https://github.com/turanszkij/WickedEngine/blob/2f5631e46aed3e278377a678b9e49714bfd33968/WickedEngine/shaders/emittedparticle_emitCS.hlsl )
	// VS (rendering ¿ë)
	struct CB_Frame
	{
		uint		options;					// renderer bool options packed into bitmask (OPTION_BIT_ values)
		float		time;
		float		time_previous;
		float		delta_time;

		uint		frame_count;
		uint		temporalaa_samplerotation;

		uint		forcefieldarray_offset;		// indexing into entity array
		uint		forcefieldarray_count;		// indexing into entity array
	};

	struct CB_Emitter
	{
		vmmat44f	xEmitterTransform;
		vmmat44f	xEmitterBaseMeshUnormRemap;

		uint		xEmitCount;
		float		xEmitterRandomness;
		float		xParticleRandomColorFactor;
		float		xParticleSize;

		float		xParticleScaling;
		float		xParticleRotation;
		float		xParticleRandomFactor;
		float		xParticleNormalFactor;

		float		xParticleLifeSpan;
		float		xParticleLifeSpanRandomness;
		float		xParticleMass;
		float		xParticleMotionBlurAmount;

		uint		xEmitterMaxParticleCount;
		uint		xEmitterInstanceIndex;
		uint		xEmitterMeshGeometryOffset;
		uint		xEmitterMeshGeometryCount;

		vmuint2		xEmitterFramesXY;
		uint		xEmitterFrameCount;
		uint		xEmitterFrameStart;

		vmfloat2		xEmitterTexMul;
		float		xEmitterFrameRate;
		uint		xEmitterLayerMask;

		float		xSPH_h;					// smoothing radius
		float		xSPH_h_rcp;				// 1.0f / smoothing radius
		float		xSPH_h2;				// smoothing radius ^ 2
		float		xSPH_h3;				// smoothing radius ^ 3

		float		xSPH_poly6_constant;	// precomputed Poly6 kernel constant term
		float		xSPH_spiky_constant;	// precomputed Spiky kernel function constant term
		float		xSPH_visc_constant;	    // precomputed viscosity kernel function constant term
		float		xSPH_K;					// pressure constant

		float		xSPH_e;					// viscosity constant
		float		xSPH_p0;				// reference density
		uint		xEmitterOptions;
		float		xEmitterFixedTimestep;	// we can force a fixed timestep (>0) onto the simulation to avoid blowing up

		vmfloat3		xParticleGravity;
		float		xEmitterRestitution;

		vmfloat3		xParticleVelocity;
		float		xParticleDrag;
	};


	// Compute Constant Buffers //
	// global 
	void SetCb_Camera(CB_CameraState& cb_cam, const vmmat44f& matWS2SS, const vmmat44f& matSS2WS, const vmmat44f& matWS2CS, VmCObject* ccobj, const vmint2& fb_size, const int k_value, const float vz_thickness);
	void SetCb_Env(CB_EnvState& cb_env, VmCObject* ccobj, const LightSource& light_src, const GlobalLighting& global_lighting, const LensEffect& lens_effect);
	// each object
	void SetCb_TMap(CB_TMAP& cb_tmap, VmObject* tobj);
	//bool SetCbVrShadowMap(CB_VrShadowMap* pCBVrShadowMap, CB_VrCameraState* pCBVrCamStateForShadowMap, vmfloat3 f3PosOverviewBoxMinWS, vmfloat3 f3PosOverviewBoxMaxWS, map<string, void*>* pmapCustomParameter);
	void SetCb_ClipInfo(CB_ClipInfo& cb_clip, VmVObject* obj, VmActor* actor, const int camClipMode, const vmmat44f& matCamClipWS2BS, const vmfloat3& matCamClipPlanePos, const vmfloat3& matCamClipPlaneDir);
	void SetCb_VolumeObj(CB_VolumeObject& cb_volume, VmVObjectVolume* vobj, const vmmat44f& matWS2OS, GpuRes& gresVol, const int iso_value, const float volblk_valuerange, const float sample_precision, const bool is_xraymode, const int sculpt_index = -1);
	void SetCb_PolygonObj(CB_PolygonObject& cb_polygon, VmVObjectPrimitive* pobj, VmActor* actor, const vmmat44f& matWS2SS, const vmmat44f& matWS2PS, const bool is_annotation_obj, const bool use_vertex_color);
	void SetCb_RenderingEffect(CB_Material& cb_reffect, VmActor* actor);
	void SetCb_VolumeRenderingEffect(CB_VolumeMaterial& cb_vreffect, VmVObjectVolume* vobj, VmActor* actor);
	void SetCb_HotspotMask(CB_HotspotMask& cb_hsmask, VmFnContainer* _fncontainer, const vmmat44f& matWS2SS);
	void SetCb_CurvedSlicer(CB_CurvedSlicer& cb_curvedSlicer, VmFnContainer* _fncontainer, VmIObject* iobj, float& sample_dist);

	bool Compile_Hlsl(const string& str, const string& entry_point, const string& shader_model, D3D10_SHADER_MACRO* defines, void** sm);

	void __TestOutErrors();

	bool CollisionCheck(const vmmat44f& matWS2OS, const AaBbMinMax& aabb_os, const vmfloat3& ray_origin_ws, const vmfloat3& ray_dir_ws);
	bool GetEnginePath(std::string& enginePath);
}

#define GETVS(NAME) dx11CommonParams->get_vshader(#NAME)
#define GETPS(NAME) dx11CommonParams->get_pshader(#NAME)
#define GETGS(NAME) dx11CommonParams->get_gshader(#NAME)
#define GETCS(NAME) dx11CommonParams->get_cshader(#NAME)