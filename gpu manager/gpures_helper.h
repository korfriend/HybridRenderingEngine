#pragma once
//#include "GpuManager.h"
#include "../gpu_common_res.h"
#include "meshpainter/MeshPainter.h"
#include <functional>
#include <cstddef> // offsetof (the CB_VxgiLights layout static_asserts)

using namespace fncontainer;

#define TRANSPOSE(A) glm::transpose(A)

// ---- HDR color pipeline (DirectX 11.3 only) ----
// The renderers composite in LINEAR HDR and a single tonemap pass converts to display range at the very end
// (after the TAA resolve, before the D2D overlay and the present copy). Two things force these to be macros
// rather than per-call-site constants:
//
//  * __COLOR_RT_FORMAT is the format of EVERY color target the renderers composite into ("RENDER_OUT_RGBA_0"
//    and "_1"). It has to change at ALL creation sites at once, because GenerateGpuResource caches on
//    {src_id, res_name} ALONE and never compares the requested format (gpures_interface.cpp) -- so whichever
//    call site misses the cache first silently defines the format for the rest of the session. Converting a
//    subset would give you HDR or LDR depending on which renderer happened to run first.
//  * D2D cannot draw into an FP16 surface with its R8G8B8A8_UNORM pixel format, so the tonemap output has to
//    land in a separate LDR target. __PRESENT_RT_NAME is what D2D, the shared-present copy, the HWND swapchain
//    copy and the CPU readback all consume.
//
// FEATURE LEVEL. The TONEMAPPER is a DX11.3 feature (__TONEMAP_ENABLED). Below that, the rendered image is the
// un-tonemapped one -- identical to what the renderer produced before this pipeline existed. The two lower
// configurations reach that same result by different routes, and the difference is structural, not a choice:
//
//   * DX10.0 stays fully LDR. It has its own shader set (shader_compiled_objs_4_0/) whose DVR path is a pixel
//     shader writing SV_TARGET and never sees the RWTexture2D declarations at all. There simply is no tonemap
//     pass to run.
//   * DX11.0 SHARES the one cs_5_0 blob set with DX11.3. The compositing shaders declare RWTexture2D<float4>,
//     which only binds to a float-typed UAV, so DX11.0 cannot be handed LDR targets without a second,
//     unorm-declared copy of every affected shader. It therefore keeps the FP16 targets -- but the tonemap is
//     pinned to identity (clip + unit exposure + no encode), whose output is saturate(hdr): bit-for-bit the
//     un-tonemapped image. The operators and the exposure knob are DX11.3-only.
#if defined(DX10_0)
#define __HDR_PIPELINE      0
#define __TONEMAP_ENABLED   0
#define __COLOR_RT_FORMAT   DXGI_FORMAT_R8G8B8A8_UNORM
#define __PRESENT_RT_NAME   "RENDER_OUT_RGBA_0"
#elif defined(DX11_3)
#define __HDR_PIPELINE      1
#define __TONEMAP_ENABLED   1
#define __COLOR_RT_FORMAT   DXGI_FORMAT_R16G16B16A16_FLOAT
#define __PRESENT_RT_NAME   "RENDER_OUT_LDR_RGBA_0"
#else // DX11_0
#define __HDR_PIPELINE      1
#define __TONEMAP_ENABLED   0
#define __COLOR_RT_FORMAT   DXGI_FORMAT_R16G16B16A16_FLOAT
#define __PRESENT_RT_NAME   "RENDER_OUT_LDR_RGBA_0"
#endif

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
	
	static inline uint16_t PackUNorm16(float x)
	{
		x = std::clamp(x, 0.0f, 1.0f);
		return (uint16_t)std::lround(x * 65535.0f);
	}

	enum Attr : uint32_t {
		A_P = 1u << 0,
		A_N = 1u << 1,
		A_T0 = 1u << 2,
		A_C = 1u << 3,
		A_T1 = 1u << 4,
		A_T2 = 1u << 5,
		A_G = 1u << 6,
	};

	// mask
	constexpr uint32_t M_P = A_P;
	constexpr uint32_t M_PN = A_P | A_N;
	constexpr uint32_t M_PT = A_P | A_T0;
	constexpr uint32_t M_PC = A_P | A_C;
	constexpr uint32_t M_PNT = A_P | A_N | A_T0;
	constexpr uint32_t M_PNC = A_P | A_N | A_C;
	constexpr uint32_t M_PTC = A_P | A_T0 | A_C;
	constexpr uint32_t M_PNTC = A_P | A_N | A_T0 | A_C;
	constexpr uint32_t M_PG = A_P | A_G;
	constexpr uint32_t M_PNG = A_P | A_N | A_G;
	constexpr uint32_t M_PTG = A_P | A_T0 | A_G;
	constexpr uint32_t M_PCG = A_P | A_C | A_G;
	constexpr uint32_t M_PNTG = A_P | A_N | A_T0 | A_G;
	constexpr uint32_t M_PNCG = A_P | A_N | A_C | A_G;
	constexpr uint32_t M_PTCG = A_P | A_T0 | A_C | A_G;
	constexpr uint32_t M_PNTCG = A_P | A_N | A_T0 | A_C | A_G;
	// PTTT = P + (T0 + T1 + T2)
	constexpr uint32_t M_PTTT = A_P | A_T0 | A_T1 | A_T2;

	struct Variant {
		uint32_t mask;
		const char* name;                 // e.g., "PNTC"
		ID3D11VertexShader* vs;
		ID3D11InputLayout* il;
		// optional for more parameters can be added
	};

	const Variant* GetPSOVariant(uint32_t mask);

#define NUM_MATERIALS 6	// primitive-bound texture descriptions 

	static string g_materials[NUM_MATERIALS] = { "MAP_KA", "MAP_KD", "MAP_KS", "MAP_NS", "MAP_BUMP", "MAP_D"};

	const ID3D11ShaderResourceView* GetPushContantSRV();

	void PushConstants(const void* data, uint32_t size, uint32_t offset);

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
		}
	};

	//typedef map<COMRES_INDICATOR, ID3D11DeviceChild*, value_cmp> GCRMAP;
	typedef map<string, ID3D11DeviceChild*> GCRMAP;
	typedef map<string, ID3D11Resource*> CONSTBUFMAP;

	struct PSOManager
	{
		bool is_initialized;
		D3D_FEATURE_LEVEL dx11_featureLevel;
		DXGI_ADAPTER_DESC dx11_adapter;

#define MAXSTAMPS 50
		ID3D11Query* dx11qr_fenceQuery;
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

		// Multi-Light (plan §0.2): get_cbuf dereferences the END iterator on a miss -- unusable as a probe.
		// This is the RECOVERABLE lookup for the VXGI light-CB preflight (ML-D5): NULL on miss, no error
		// spam (the caller owns the W-L3 warning policy). Existing get_cbuf calls are untouched on purpose.
		ID3D11Buffer* try_get_cbuf(const string& name)
		{
			auto it = dx11_cbuf.find(name);
			if (it == dx11_cbuf.end())
				return NULL;
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

		PSOManager()
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
			dx11qr_fenceQuery = nullptr;
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
			if (dx11qr_fenceQuery)
			{
				dx11qr_fenceQuery->Release();
				dx11qr_fenceQuery = nullptr;
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
		uint32_t color;
	};

	struct ParticleCounters
	{
		uint32_t aliveCount;
		uint32_t deadCount;
		uint32_t realEmitCount;
		uint32_t aliveCount_afterSimulation;
		uint32_t culledCount;
		uint32_t cellAllocator;
	};

	static const uint32_t PARTICLECOUNTER_OFFSET_ALIVECOUNT = 0;
	static const uint32_t PARTICLECOUNTER_OFFSET_DEADCOUNT = PARTICLECOUNTER_OFFSET_ALIVECOUNT + 4;
	static const uint32_t PARTICLECOUNTER_OFFSET_REALEMITCOUNT = PARTICLECOUNTER_OFFSET_DEADCOUNT + 4;
	static const uint32_t PARTICLECOUNTER_OFFSET_ALIVECOUNT_AFTERSIMULATION = PARTICLECOUNTER_OFFSET_REALEMITCOUNT + 4;
	static const uint32_t PARTICLECOUNTER_OFFSET_CULLEDCOUNT = PARTICLECOUNTER_OFFSET_ALIVECOUNT_AFTERSIMULATION + 4;
	static const uint32_t PARTICLECOUNTER_OFFSET_CELLALLOCATOR = PARTICLECOUNTER_OFFSET_CULLEDCOUNT + 4;

	static const uint32_t EMITTER_OPTION_BIT_FRAME_BLENDING_ENABLED = 1 << 0;
	static const uint32_t EMITTER_OPTION_BIT_SPH_ENABLED = 1 << 1;
	static const uint32_t EMITTER_OPTION_BIT_MESH_SHADER_ENABLED = 1 << 2;
	static const uint32_t EMITTER_OPTION_BIT_COLLIDERS_DISABLED = 1 << 3;
	static const uint32_t EMITTER_OPTION_BIT_USE_RAIN_BLOCKER = 1 << 4;
	static const uint32_t EMITTER_OPTION_BIT_TAKE_COLOR_FROM_MESH = 1 << 5;

	struct IndirectDrawArgsInstanced
	{
		uint32_t VertexCountPerInstance;
		uint32_t InstanceCount;
		uint32_t StartVertexLocation;
		uint32_t StartInstanceLocation;
	};
	struct IndirectDrawArgsIndexedInstanced
	{
		uint32_t IndexCountPerInstance;
		uint32_t InstanceCount;
		uint32_t StartIndexLocation;
		int BaseVertexLocation;
		uint32_t StartInstanceLocation;
	};
	struct IndirectDispatchArgs
	{
		uint32_t ThreadGroupCountX;
		uint32_t ThreadGroupCountY;
		uint32_t ThreadGroupCountZ;
	};

	int Initialize(VmGpuManager* pCGpuManager, PSOManager* gpu_params);
	void Deinitialize();

	PSOManager* GetPSOManager();
	MeshPainter* GetMeshPainter();

	HRESULT PresetCompiledShader(__ID3D11Device* pdx11Device, HMODULE hModule, LPCWSTR pSrcResource, LPCSTR strShaderProfile, ID3D11DeviceChild** ppdx11Shader/*out*/
		, D3D11_INPUT_ELEMENT_DESC* pInputLayoutDesc, uint32_t num_elements, ID3D11InputLayout** ppdx11LayoutInputVS);

	void CheckReusability(GpuRes& gres, VmObject* resObj, bool& update_data, bool& reusable,
		const vmobjects::VmParamMap<std::string, std::any>& res_new_values);
	void Fence();

	// volume/block structure
	bool UpdateOtfBlocks(GpuRes& gres, VmVObjectVolume* main_vobj, VmVObjectVolume* mask_vobj,
		VmObject* tobj, const int sculpt_value, LocalProgress* progress = NULL);
	bool UpdateMinMaxBlocks(GpuRes& gres_min, GpuRes& gres_max, const VmVObjectVolume* vobj, LocalProgress* progress = NULL);
	// bool UpdateAOMask(const VmVObjectVolume* vobj, LocalProgress* progress = NULL); // to do
	bool UpdateVolumeModel(GpuRes& gres, VmVObjectVolume* vobj, const bool use_nearest_max, bool heuristicResize = false, LocalProgress* progress = NULL);

	void SetUserCapacity(const float maxVolumeSizeKB, const float maxVolumeExtent);

	bool UpdateTMapBuffer(GpuRes& gres, VmObject* tobj, const bool isPreInt = false, LocalProgress* progress = NULL);

	// primitive structure
	bool UpdatePrimitiveModel(map<string, GpuRes>& map_gres_vtxs, GpuRes& gres_idx, map<string, GpuRes>& map_gres_texs, VmVObjectPrimitive* pobj, VmObject* imgObj = NULL, bool* hasTextureMap = NULL, LocalProgress* progress = NULL);

#define UPFB_SYSOUT 0x1
#define UPFB_RAWBYTE 0x1 << 1 // buffer only
#define UPFB_MIPMAP 0x1 << 2  // texture only
#define UPFB_HALF 0x1 << 3    // texture only
#define UPFB_HALF_W 0x1 << 4    // texture only
#define UPFB_HALF_H 0x1 << 5    // texture only
#define UPFB_NFPP_BUFFERSIZE 0x1 << 6 // buffer only //
#define UPFB_NFPP_TEXTURESTACK 0x1 << 7 // texture only
#define UPFB_PICK_TEXTURE 0x1 << 8 // DX10 picking texture
	// framebuffer structure
	bool UpdateFrameBuffer(GpuRes& gres, const VmIObject* iobj,
		const string& res_name,
		const GpuResType gres_type,
		const uint32_t bind_flag,
		const uint32_t dx_format,
		const int fb_flag,
		const int num_frags_perpixel = 1,
		const int structured_stride = 0);

	bool UpdateCustomBuffer(GpuRes& gres, VmObject* srcObj, const string& resName, const void* bufPtr, const int numElements, DXGI_FORMAT dxFormat, const int type_bytes, LocalProgress* progress = NULL, uint64_t cpu_update_custom_time = 0);

	// Allocate (or reuse) a GPU-writable cubic Texture3D for VXGI (voxel radiance/opacity): USAGE_DEFAULT with
	// UNORDERED_ACCESS | SHADER_RESOURCE. Keyed on srcObj + res_name so it persists across frames. with_mips
	// adds a full auto-gen mip chain (+RENDER_TARGET for GenerateMips) so cone tracing can LOD-sample; the
	// UAV stays on mip 0 — write mip 0, then call GenerateMips on the SRV.
	// (rev.16) src_id = the SCENE id (VXGI is a scene-level field; a vobj can live in several scenes, so
	// a vobj key cross-contaminated them). The actor tag / co-ownership is gone -- single owner = scene.
	// out_recreated (verification round-1 Major 1): set true whenever the resource was (re)GENERATED this
	// call -- its CONTENTS ARE UNDEFINED, so any bake meta describing the old grid is now a lie. The
	// caller must drop _bool_VxgiFieldReady before any consume/gate logic can route this frame's reads
	// to the fresh, un-baked texture (rev.9: preflight failure must serve previous bake or DISABLED,
	// never a fresh grid). Returns false when generation failed (resource unusable).
	bool UpdateVoxelGrid(GpuRes& gres, const int src_id, const string& res_name, const uint32_t resolution, const uint32_t dx_format, const bool with_mips = false, bool* out_recreated = NULL);

	// Upload a CPU buffer as a DYNAMIC Texture3D (write-discard). The source is assumed to be tightly packed
	// (row pitch = width * bytes_per_texel, depth pitch = row pitch * height); destination Texture3D pitches
	// are handled internally.
	bool UpdateCustomTexture3D(GpuRes& gres, VmObject* srcObj, const string& resName,
		const void* bufPtr, const uint32_t width, const uint32_t height, const uint32_t depth,
		DXGI_FORMAT dxFormat, const int bytes_per_texel,
		LocalProgress* progress = NULL, uint64_t cpu_update_custom_time = 0);

	bool UpdatePaintTexture(VmActor* actor, const vmmat44f& matSS2WS, fncontainer::VmCamera* camObj, const vmfloat2& paint_pos2d_ss, const BrushParams& brushParams);

#define ZERO_SET(T) T(){memset(this, 0, sizeof(T));}

#define MAX_LAYERS 8
	struct Fragment
	{
		uint32_t i_vis;
		float z;
		float zthick;
		float opacity_sum;
	};

	struct FragmentArray
	{
		Fragment frags[MAX_LAYERS];
	};

	struct LightSource {
		// (rev.18) single LightType replaces the base is_on_camera/is_pointlight bool pair. Direct shading
		// renders SPOT as POINT (Q7 -- the cone is VXGI-only for now), so the spot angles are not carried here.
		// Default DIRECTIONAL reproduces the old (is_on_camera=false, is_pointlight=false) struct default.
		fncontainer::LightType type = fncontainer::LightType::DIRECTIONAL;
		vmfloat3 light_dir = vmfloat3(0);
		vmfloat3 light_pos = vmfloat3(0); 

		vmfloat3 light_ambient_color = vmfloat3(1);
		vmfloat3 light_diffuse_color = vmfloat3(1);
		vmfloat3 light_specular_color = vmfloat3(1);
	};

	// (v76) struct GlobalLighting REMOVED (user directive -- SSAO feature retired): it carried ONLY the
	// SSAO parameters. SetCb_Env lost the parameter; the CB fields it fed are now reserved pads (above).

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
		vmmat44f mat_ws2ps_revZ; // Reverse Z projection matrix

		vmfloat3 pos_cam_ws;
		uint32_t rt_width;

		vmfloat3 dir_view_ws;
		uint32_t rt_height;

		float cam_vz_thickness;
		uint32_t k_value;
		// 0-  bit : 0 : (orthogonal), 1 : (perspective)
		// 1-  bit : for RT to k-buffer : 0 (just RT), 1 : (after silhouette processing)
		// 2-  bit : for dynamic K value // deprecated... (this will be treated as a separate shader
		// 3-  bit : for storing the final fragments to the k buffer, which is used for sequentially coming renderer (e.g., DVR) : 0 (skipping), 1 (storing)
		// 4-  bit : only for DFB without (S)FM. stores all fragments into the framebuffer (using offset table)
		// 5-  bit : 0 : (normal rendering), 1 : picking mode
		// 6-  bit : 0 : (stores the final RGBA and depth to RT), 1 : (does not store them) // will be deprecated
		// 7-  bit : 0 : full raycaster, 1 : half raycaster for 4x faster volume rendering (dither)
		// 8-  bit : 0 : only OIT fragments, 1 : foremostopaque or singlelayer effect ...
		// 9-  bit : 0 : outlint mode (solid), 1 : outlint mode (gradient alpha)
		// 10- bit : 0 : 3D camera, 1 : Slicer
		// 11- bit : 0 : no external K-buffer, 1 : put an external RT to K-buffer (OIT_RESOLVE)
		// 12- bit : 0 : TAA off, 1 : TAA active (sub-pixel jitter this frame) — gates temporal-only effects (DVR ray-start dither)
		uint32_t cam_flag;
		// used for 
		// 1) A-Buffer prefix computations /*deprecated*/ or 2) beta (asfloat) for merging operation
		// 2) Second Layer Blending : 1st 8 bit for Pattern Interval (pixels), 2nd 8 bit for Blending
		uint32_t iSrCamDummy__0; 

		float near_plane;
		float far_plane;
		// used for 
		// 1) the level of MIPMAP generation (only for SSAO rendering), or 2) picking xy (16bit, 16bit)
		// 3) outline's colorRGB and thicknessPix
		uint32_t iSrCamDummy__1; 
		uint32_t iSrCamDummy__2; // scaling factor (asfloat) for the z-thickness value determined by the z-resolution

		vmfloat3 hoverPosWS;
		float hoverRadius;

		uint32_t hoverColor;
		float hoverBand;
		uint32_t iSrCamDummy__3;
		uint32_t iSrCamDummy__4;

		// ---- Tonemap post-pass (mirrors the tail of HxCB_CameraState) ----
		// Appended here rather than given their own CB because shader model 5.0 offers 14 cbuffer slots and
		// CommonShader.hlsl already uses b0..b13. Deliberately NOT reusing one of the iSrCamDummy fields:
		// iSrCamDummy__0 is already double-booked by Blend2ndLayer and TaaResolve, and a third meaning on it
		// would be a trap for the next reader.
		//
		// Only TaaResolve and the tonemap pass read these; every other shader ignores the tail. Filled by
		// TonemapParams::ApplyTo at each of those two dispatch sites.
		uint32_t tm_operator;
		uint32_t tm_encode;
		float tm_exposure;
		float tm_knee;

		float tm_white_point;
		float tm_radiance_ceiling;
		// TAA sub-pixel jitter in PIXELS (mirrors the tail of HxCB_CameraState). Only ray generators that do
		// not go through the camera matrices need this -- the curved slicer builds its sample position from
		// the thread id and buf_curvePoints, so folding the jitter into mat_ws2ss (as SetCb_Camera does for
		// every other path) never reaches it. Written by SetCb_Camera; zero means "no jitter".
		float taa_jitter_px_x;
		float taa_jitter_px_y;

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
		uint32_t env_flag;

		vmfloat3 dir_light_ws;
		uint32_t num_lights;

		// associated colors
		vmfloat4 ltint_ambient;
		vmfloat4 ltint_diffuse;
		vmfloat4 ltint_spec;

		vmmat44f	mat_ws2ls_smap;	// for shadow : Sample Depth Map (ws2ss)
		vmmat44f	mat_ls2ws_smap;	// for shadow : Depth Comparison (ss2ws)

		// (v76) SSAO RETIRED (user directive) -- the five parameter fields are kept as SAME-SIZE reserved
		// pads so the b7 layout (and every downstream field offset: num_safe_loopexit, dof_*) is unchanged
		// and no unrelated shader needs a relayout. ZERO_SET keeps them 0.
		float env_reserved_ssao_0;   // was r_kernel_ao
		int env_reserved_ssao_1;     // was num_dirs
		int env_reserved_ssao_2;     // was num_steps
		float env_reserved_ssao_3;   // was tangent_bias

		float env_reserved_ssao_4;   // was ao_intensity
		uint32_t num_safe_loopexit;
		uint32_t env_dummy_1;
		uint32_t env_dummy_2;

		float dof_lens_r;
		float dof_lens_F;
		float dof_focus_z;
		int dof_lens_ray_num_samples;

		ZERO_SET(CB_EnvState)
	};

	// Voxel Cone Tracing GI (VXGI). Volume-sourced grid: volumetric in-scatter + surface AO + surface
	// indirect, individually controllable (a 0 intensity disables that effect). Layout must match
	// HxCB_VXGI (register b13; b14 is invalid — D3D11 has 14 CB slots b0-b13) in hlsl/CommonShader.hlsl
	// byte-for-byte. Scalars are bit-packed (see SetCb_VXGI; HLSL decodes via the VXGI_* macros).
	struct CB_VXGI
	{
		vmmat44f mat_ws2vox;         // world -> voxel [0,1] space (= volume mat_ws2ts)

		uint32_t grid_res;           // cubic grid resolution (voxels per axis)
		uint32_t vxgi_flag;          // [0:7] flags (bit0=enabled, bit1=otf-mask, bit2=sculpt-mask, bit3=sculpt-bits, bit4=context/VR_MODE2, bit5=clip, bit6=preserve-AO light-only inject — set by VolumeRenderer, not SetCb_VXGI) | [8:23] num_cones | [24:27] debug mode | [28:31] debug mip
		uint32_t gi_ao_intensity;    // half(gi_intensity = volumetric in-scatter) | half(ao_intensity = surface AO) << 16
		uint32_t indirect_aperture;  // half(indirect_intensity = surface bounce) | half(cone_aperture) << 16

		float    max_trace_dist;     // cone max distance in [0,1] voxel space
		// Volume-fit mapping WITH MARGIN: the grid box is the volume box expanded by a margin shell (empty
		// voxels), so diffusion / cone marches near the volume boundary have room instead of clamping at the
		// edge. volume tex coord -> grid coord: vox = ts * vox_fit_scale + vox_fit_offset (uniform).
		float    vox_fit_scale;
		float    vox_fit_offset;
		uint32_t ao_pivot_slope;     // half(ao remap pivot, def 0.3) | half(ao remap slope, def 1.5) << 16

		// World metric of the grid (grid axes = volume axes, so the box is generally anisotropic in WS).
		// grid_axis_ws = world length of each full [0,1] grid axis (volume bbox edge incl. margin);
		// world length of a unit grid-space step along direction L = length(L * grid_axis_ws).
		vmfloat3 grid_axis_ws;
		float    voxel_ref_ws;       // one voxel's reference world thickness (mean axis / res): coverage alpha -> optical-depth scale

		vmmat44f mat_vox2ws;         // voxel [0,1] -> world (clip tests in Voxelize; inverse of mat_ws2vox)

		float    scatter_gain;       // diffusion in-scatter gain per iteration (HLSL VXGI_SCATTER_GAIN)
		float    surface_gi_gain;    // Part C surface cone indirect strength (HLSL VXGI_SURFACE_GI_GAIN; clamped [0, 0.95])
		float    surface_cone_ao_gain; // Part C surface cone AO blend strength (HLSL VXGI_SURFACE_CONE_AO_GAIN; 0 = density AO only)
		float    context_alpha_gain; // VR_MODE 2 coverage boost (HLSL VXGI_CONTEXT_ALPHA_GAIN; 1 = plain MODULATE parity, > 1 legal — shader saturates)
		// Direct-light SHADOW strength [0,1]. The voxel field is the ONLY place this system computes
		// light visibility; the DVR's local Phong has none. 0 = legacy (field DIRECT is added on top of
		// the unshadowed local direct = the double count). >0 = the DVR shadows its own full-res direct
		// with the field's visibility and adds INDIRECT only. Also gates the feature: 0 leaves every
		// consumer (incl. the curved slicer, whose CB is built by LoadVxgiConsumerCb) on legacy.
		float    direct_shadow_gain;
		float    _vxgi_pad0, _vxgi_pad1, _vxgi_pad2; // keep sizeof(CB_VXGI) a multiple of 16 (see the static_assert)

		ZERO_SET(CB_VXGI)
	};
	// D3D11 REJECTS a constant buffer whose ByteWidth is not a multiple of 16, and gpures_helper.cpp's
	// CREATE_AND_SET passes sizeof(STRUCT) straight through. Without this assert the failure appears only
	// at runtime, as 'error : basic dx11 resources!' followed by the whole renderer refusing to load --
	// a symptom that points nowhere near the struct that grew. Pad when you add a field.
	static_assert(sizeof(CB_VXGI) % 16 == 0, "CB_VXGI must stay 16-byte aligned (D3D11 constant-buffer rule)");

	// ---- Multi-Light (plan: secret_recipies/MULTI_LIGHT_PLAN.md, ML-D3) ----
	// VXGI light-set CB, bound at register b11 which is declared LOCALLY in hlsl/vxgi/InjectLight.hlsl
	// (CommonShader.hlsl declares nothing at b11 -- see its b-slot ledger comment). Cap = the view's
	// visible lights with the smallest actorIds; the rest are dropped with a W-L1 warning.
	//
	// Why 64 costs nothing to carry: InjectLight's light loop is [loop] over light_count, NOT over the
	// array size, so a scene with 2 lights marches 2 cones no matter how large this array is. The cap
	// only sets the CB footprint (16 + N*64 B), and the per-light cone march -- up to 64 3D-texture
	// samples EACH -- dominates the cost by orders of magnitude. Raising the cap is therefore free;
	// what is not free is actually lighting a scene with many lights.
	//
	// Stays a constant buffer on purpose: the loop index is wave-uniform (every thread reads the same
	// light per iteration), which is the case CBs are built for -- scalar/broadcast path, and no
	// contention with the L1/L2 traffic the cone march's grid sampling already saturates. A
	// StructuredBuffer would route the same uniform read through the vector memory path for no gain.
	// Revisit only past ~1000 lights, where 16 + N*64 breaks the 64 KB CB limit and forces an SRV.
	//
	// Layout must match InjectLight.hlsl's cbVxgiLights byte-for-byte (static_asserts below); the two
	// VXGI_MAX_LIGHTS defines must move together or the CB layout silently diverges.
#define VXGI_MAX_LIGHTS 64
	struct VxgiLight // 64 B = 4 HLSL float4 rows (rev.18: expanded from 48B/3 rows for the SPOT cone params)
	{
		vmfloat3 pos_ws;   uint32_t flags;      // bit0 = positional (point/spot), bit1 = spot (cone attenuation)
		vmfloat3 dir_ws;   float    intensity;  // spot: cone axis (= ray travel dir, directional dir convention); point: ignored. dir CPU-normalized (never zero)
		vmfloat3 color;    float    cos_inner;  // ML-D10 (default white); cos_inner = spot only: cos(inner half-angle) -- former pad0 slot
		float    cos_outer;                     // spot only: cos(outer half-angle)
		float    spot_rsv0, spot_rsv1, spot_rsv2; // reserved (range/exponent/etc. future spot params)

		ZERO_SET(VxgiLight)
	};
	struct CB_VxgiLights // 16 + 64*64 = 4112 B
	{
		uint32_t light_count;                  // lights actually in the CB (<= VXGI_MAX_LIGHTS); 0 = no light (dark field, V17)
		uint32_t vxgil_pad1, vxgil_pad2, vxgil_pad3;
		VxgiLight lights[VXGI_MAX_LIGHTS];

		ZERO_SET(CB_VxgiLights)
	};
	static_assert(sizeof(VxgiLight) == 64, "VxgiLight must stay 4 float4 rows (HLSL cbuffer packing)");
	static_assert(offsetof(CB_VxgiLights, lights) == 16, "CB_VxgiLights header must stay one float4 row");
	static_assert(sizeof(CB_VxgiLights) == 16 + 64 * 64, "CB_VxgiLights must match InjectLight.hlsl's cbVxgiLights (4112)");

	// Tonemap post-pass config. Plain CPU-side POD, NOT a GPU constant buffer: the values are copied into the
	// tail of CB_CameraState (see TonemapParams::ApplyTo). Shader model 5.0 has exactly 14 cbuffer slots
	// (b0..b13) and CommonShader.hlsl already claims every one of them, so there is no slot left to bind a
	// dedicated CB -- and both passes that need these values (TaaResolve, Tonemap) already bind and fill
	// CB_CameraState at their dispatch sites anyway.
	//
	// The defaults reproduce today's image exactly: TM_CLIP + exposure 1 + encode none == saturate(hdr).
	struct TonemapParams
	{
		uint32_t tm_operator = 0;         // 0 = clip (identity), 1 = knee/shoulder, 2 = Reinhard-ext, 3 = Hable, 4 = ACES
		uint32_t encode = 0;              // 0 = none (today), 1 = sRGB, 2 = gamma 2.2
		float    exposure = 1.f;          // linear scale applied before the curve (1 = off)
		float    knee = 1.f;              // operator 1: identity below this, shoulder above it (1 = pure clip)
		float    white_point = 4.f;       // operators 2/3: the value that maps to 1.0
		float    radiance_ceiling = 64.f; // NaN/Inf/firefly guard; also the sanitize bound shared with TaaResolve

		// Both TaaResolve and the tonemap pass map CB_CameraState with WRITE_DISCARD and repopulate only the
		// fields they read, so each of them has to stamp these in. They must agree on radiance_ceiling: one
		// sanitizes what enters the TAA history, the other what reaches the screen.
		void ApplyTo(CB_CameraState& cb) const
		{
			cb.tm_operator = tm_operator;
			cb.tm_encode = encode;
			cb.tm_exposure = exposure;
			cb.tm_knee = knee;
			cb.tm_white_point = white_point;
			cb.tm_radiance_ceiling = radiance_ceiling;
		}
	};

	struct CB_ClipInfo
	{
		vmmat44f mat_clipbox_ws2bs;  // To Clip Box Space (BS)
		vmfloat3 pos_clipplane;
		// 1st bit : 0 (No) 1 (Clip Box)
		// 2nd bit : 0 (No) 1 (Clip plane)
		uint32_t clip_flag;

		vmfloat3 vec_clipplane;
		uint32_t ci_dummy_1;

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
		// 17th bit : g_tex2D_PAINT
		// 18th bit : display camera brush
		// 19th bit : camera brush (0: geodesic, 1: 3D spatial)
		uint32_t tex_map_enum;

		// 1st bit : 0 (shading color to RT) 1 (normal to RT for the purpose of silhouette rendering)
		// 2nd bit : 0 (no face normal) 1 (use face normal) in GS (flat normal)
		// 3rd bit : 0 (no face color) 1 (use face color) in GS
		// 4th bit : 0 (no vertex color) 1 (use vertex color) via vs_output.f3Custom
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
		uint32_t pobj_flag;
		uint32_t num_letters;	// // for text object, or for slicer (rayprocessing.hlsl), vertexStride, 
		float dash_interval;	// for slicer (rayprocessing.hlsl), non-thickness solidfilling alpha
		float depth_thres;	// for outline!

		float pix_thickness; // 1) for POINT and LINE TOPOLOGY, 2) slicer's cutting outline thickness
		float vz_thickness;
		uint32_t pobj_dummy_0; // 1) actor_id used for picking, 2) outline color, 3) iso_value for difference map
		uint32_t pobj_dummy_1;

		vmfloat4 pb_shading_factor; // x : Ambient, y : Diffuse, z : Specular, w : specular

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
		// 2 bit : x-ray post-filter (1) or not (0)
		// 19 bit : ghost surface (1) or not (0)
		// 20 bit : hotspot visible (1) or not (0)
		// 24~31bit : Sculpt Mask Value (1 byte)
		uint32_t vobj_flag;
		uint32_t iso_value;
		uint32_t v_dummy0; 
		uint32_t v_dummy1;

		// light properties
		vmfloat4 pb_shading_factor;	// x : Ambient, y : Diffuse, z : Specular, w : Specular power

		vmfloat3 mask_vol_size; // volume size stored in GPU memory
		float mask_value_range;

		vmuint3 vol_original_size;
		uint32_t v_dummy2;

		CB_ClipInfo clip_info;

		ZERO_SET(CB_VolumeObject)
	};

	// future.. change to material...
	struct CB_Material // normally for each object
	{
		// 1st bit : AO or Not , 2nd bit : Anisotropic BRDF or Not , 3rd bit : Apply Shading Factor or Not
		// NA ==> 4th bit : 0 : Normal Curvature Map (2D), 1 : Apply Concaveness
		// NA ==> 5th bit : Concaveness Direction or Not
		uint32_t rf_flag;
		uint32_t outline_mode; // deprecated
		float curvature_kernel_radius;
		uint32_t rf_dummy_0;

		float brdf_diffuse_ratio;
		float brdf_reft_ratio;
		float brdf_expw_u;
		float brdf_expw_v;

		float shadowmap_occusion_w; // for shadow
		float shadowmap_depth_bias; // for shadow
		float occ_radius;
		uint32_t occ_num_rays;

		float ao_intensity;
		uint32_t rf_dummy_1;
		uint32_t rf_dummy_2;
		uint32_t rf_dummy_3;

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
		uint32_t flag; // (bit0 "jittering" retired; ray-start dither now keyed on TAA via CB_CameraState.cam_flag bit12)
		uint32_t vrf_dummy_1;

		ZERO_SET(CB_VolumeMaterial)
	};

	struct CB_TMAP
	{
		vmfloat4 last_color;

		uint32_t		first_nonzeroalpha_index; // For ESS
		uint32_t		last_nonzeroalpha_index;
		uint32_t		tmap_size_x;
		float		mapping_v_min; 

		float		mapping_v_max;
		// 1st bit set, then color clip
		uint32_t		flag;	
		uint32_t		tm_dummy_1;
		uint32_t		tm_dummy_2;

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
		float alphaCorrection; // F2: per-step DVR alpha correction (curved). Reuses the former __dummy0
		                       // slot (uint32_t -> float, same 4B offset). Must mirror HxCB_CurvedSlicer
		                       // in hlsl/CommonShader.hlsl. 1.0 = no correction. See CurvedSlicerVR.cpp.
		vmfloat3 planeUp; // WS, length is planePitch
		uint32_t flag; // 1st bit : isRightSide
	};
	// Phase 1 invariant (F16/G5): b10 is shared with out-of-scope consumers (SlicerSR.cpp and
	// RayProcessing.hlsl's curved thick-slice variants). alphaCorrection reuses __dummy0 at the same
	// offset, so size/layout must not change. Only an end-append (-> 96B) is permitted, and only in
	// Phase 3-4. If this fires, the HLSL mirror (HxCB_CurvedSlicer) and this struct have diverged.
	static_assert(sizeof(CB_CurvedSlicer) == 80, "CB_CurvedSlicer (b10) must stay 80B in Phase 1.");

	struct CB_TestBuffer
	{
		//uint32_t testIntValues[16];
		//float testFloatValues[16];

		uint32_t testA;
		uint32_t testB;
		uint32_t testC;
		uint32_t testD;
	};

	struct CB_Particle_Blob
	{
		vmfloat4 xyzr_spheres[4];
		vmint4 color_spheres;
		float smoothCoeff;
		vmfloat3 minRoiCube;
		uint32_t dummy1;
		vmfloat3 maxRoiCube;
	};

	struct CB_Undercut
	{
		vmmat44f	mat_ws2lss_udc_map;
		vmmat44f	mat_ws2lcs_udc_map;

		vmfloat3	undercutDir;
		uint32_t		icolor;
	};

	// Slicer x-ray image-level post-processing filter (SliceXrayFilter.hlsl, cbuffer b12)
	struct CB_SliceFilter
	{
		int32_t		filter_radius;		// convolution kernel radius (N = 2*radius+1); 0 = passthrough
		int32_t		use_filter;			// 0 = passthrough (composite only), !=0 = apply NxN convolution
		float		filter_pad[2];
	};

	// CS Particle Buffer Update (https://github.com/turanszkij/WickedEngine/blob/2f5631e46aed3e278377a678b9e49714bfd33968/WickedEngine/shaders/emittedparticle_emitCS.hlsl )
	// VS (for rendering)
	struct CB_Frame
	{
		uint32_t		options;					// renderer bool options packed into bitmask (OPTION_BIT_ values)
		float		time;
		float		time_previous;
		float		delta_time;

		uint32_t		frame_count;
		uint32_t		temporalaa_samplerotation;

		uint32_t		forcefieldarray_offset;		// indexing into entity array
		uint32_t		forcefieldarray_count;		// indexing into entity array
	};

	struct CB_Emitter
	{
		vmmat44f	xEmitterTransform;
		vmmat44f	xEmitterBaseMeshUnormRemap;

		uint32_t		xEmitCount;
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

		uint32_t		xEmitterMaxParticleCount;
		uint32_t		xEmitterInstanceIndex;
		uint32_t		xEmitterMeshGeometryOffset;
		uint32_t		xEmitterMeshGeometryCount;

		vmuint2		xEmitterFramesXY;
		uint32_t		xEmitterFrameCount;
		uint32_t		xEmitterFrameStart;

		vmfloat2		xEmitterTexMul;
		float		xEmitterFrameRate;
		uint32_t		xEmitterLayerMask;

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
		uint32_t		xEmitterOptions;
		float		xEmitterFixedTimestep;	// we can force a fixed timestep (>0) onto the simulation to avoid blowing up

		vmfloat3		xParticleGravity;
		float		xEmitterRestitution;

		vmfloat3		xParticleVelocity;
		float		xParticleDrag;
	};

	struct CB_SortConstants
	{
		vmint3 job_params;
		uint32_t counterReadOffset;
	};


	// Compute Constant Buffers //
	// global 
	void SetCb_Camera(CB_CameraState& cb_cam, const vmmat44f& matWS2SS, const vmmat44f& matSS2WS, const vmmat44f& matWS2CS, const vmmat44f& matWS2PS, fncontainer::VmCamera* ccobj, const vmint2& fb_size, const int k_value, const float vz_thickness, const vmfloat2& taa_jitter_px = vmfloat2(0.f, 0.f));
	void SetCb_Env(CB_EnvState& cb_env, fncontainer::VmCamera* ccobj, const LightSource& light_src, const LensEffect& lens_effect); // (v76) GlobalLighting param removed (SSAO retired)
	// Fill the VXGI constant buffer (register b13). mat_ws2vox_raw = world->voxel[0,1] (volume mat_ws2ts for v1).
	// gi/ao/indirect intensities gate the three effects individually (0 = off); they are half-packed into the
	// CB. debug_byte = (mode & 0xF) | (mip & 0xF) << 4, packed into vxgi_flag's top byte.
	// medium_flags: what the Voxelize shader reproduces of the DVR's own visibility (bit0=multi-OTF,
	// bit1=sculpt-mask, bit2=sculpt-bits, bit3=context/VR_MODE2, bit4=clip) — packed into vxgi_flag bits
	// [1:5]; Voxelize gates its per-sub-sample tests on them. The caller decides which of these to enable:
	// a gate NOT set means that material stays in the grid (it still occludes and scatters) AND its state
	// stops feeding the VXGI content stamp, so editing it does not re-voxelize.
	// context_alpha_gain applies to the context flag only (see VXGI_CONTEXT_ALPHA_GAIN).
	void SetCb_VXGI(CB_VXGI& cb, const vmmat44f& mat_ws2vox_raw, const uint32_t resolution, const float gi_intensity, const float ao_intensity, const bool enabled, const float indirect_intensity = 1.f, const uint32_t debug_byte = 0, const uint32_t medium_flags = 0, const float ao_pivot = 0.3f, const float ao_slope = 1.5f, const float scatter_gain = 0.75f, const float surface_gi_gain = 0.15f, const float surface_cone_ao_gain = 1.f, const float context_alpha_gain = 1.f, const float direct_shadow_gain = 0.f);

	// D9.1 bake CONTENT KEY — the SINGLE function computing it, so the producer (VolumeRenderer bake
	// publish) and the consumer (LoadVxgiConsumerCb) cannot drift. View-INDEPENDENT by construction:
	// volume content time + OTF content time + the actor's world->texture transform (FNV-1a). Clip and
	// resolved-light are deliberately excluded — they are per-view / builder-only, so a slicer consumer
	// could never reproduce them (would false-mismatch forever). See plan §D9.1.
	uint64_t VxgiBakeContentKey(VmObject* vobj, VmObject* tobj_otf, const vmmat44f& mat_ws2ts);

	// §3.1 shared slicer / non-owner CONSUMER helper. Loads the vobj-published bake CB into cb_out for a
	// consuming draw. Returns true ONLY when FieldReady is set AND the recomputed content key still matches
	// the published one; otherwise false with w1_reason = 1 (r1: no usable bake) or 2 (r2: stale bake). The
	// r3 case (resource lookup failure) is the caller's probe AFTER this returns true. On success cb_out
	// carries the bake's mapping / medium bits / scatter_gain, with THIS view's gi/ao intensity substituted
	// and the debug byte + transient preserve-AO bit cleared (D3 consume rules). Never runs any bake work.
	// (rev.16) state_anchor = scene state object (VXGI state); vobj = volume (for the content key only).
	bool LoadVxgiConsumerCb(CB_VXGI& cb_out, int& w1_reason, VmObject* state_anchor, VmObject* vobj, VmObject* tobj_otf,
		const vmmat44f& mat_ws2ts, const float gi_intensity, const float ao_intensity,
		// Direct-shadow gain of THE CONSUMING VIEW. The bake CB blob carries the BUILDER's gain, and the
		// verbatim copy handed it to consumers that never bind t10/t11 (curved slicer: null vis -> black
		// local direct AND undiminished field direct -- neither legacy nor split). Default 0 = feature off
		// unless the caller both opts in and binds the grids.
		const float direct_shadow_gain = 0.f);

	// D9.3 — session-monotonic VXGI identity token, the SINGLE issuer for the whole DLL. Object ids are
	// RECYCLED by the engine's ResourceManager, so every VXGI identity decision (builder/owner gen, warning
	// suppression keys) uses a gen issued once per object and never reused. Both the 3D builder
	// (VolumeRenderer) and the slicer consumers (planar + curved) MUST draw from this one counter — a second
	// counter would double-issue different identities for the same object. Zero = unissued.
	uint64_t VxgiIssueGen(VmObject* obj);

	// W1 ("consumer has no usable bake") warning suppression, shared by the planar and curved consumers so
	// the bookkeeping cannot fork: warn once per (vobj gen, reason bit r1/r2/r3), stored on the CONSUMER's
	// iobj, bounded at 64 entries with insertion-order eviction. A successful ready-consume clears the vobj's
	// entry (re-arm), so the next not-ready warns exactly once again. Returns true when the caller should
	// emit the warning (message text stays at the call site).
	bool VxgiW1ShouldWarn(VmObject* consumer_iobj, const uint64_t vobj_gen, const int reason);
	void VxgiW1Clear(VmObject* consumer_iobj, const uint64_t vobj_gen);
	// each object
	void SetCb_TMap(CB_TMAP& cb_tmap, VmObject* tobj);
	//bool SetCbVrShadowMap(CB_VrShadowMap* pCBVrShadowMap, CB_VrCameraState* pCBVrCamStateForShadowMap, vmfloat3 f3PosOverviewBoxMinWS, vmfloat3 f3PosOverviewBoxMaxWS, map<string, void*>* pmapCustomParameter);
	void SetCb_ClipInfo(CB_ClipInfo& cb_clip, VmVObject* obj, VmActor* actor, const int camClipMode, const std::set<int> camClipperFreeActor, const vmmat44f& matCamClipWS2BS, const vmfloat3& matCamClipPlanePos, const vmfloat3& matCamClipPlaneDir);
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

#define GETVS(NAME) psoManager->get_vshader(#NAME)
#define GETPS(NAME) psoManager->get_pshader(#NAME)
#define GETGS(NAME) psoManager->get_gshader(#NAME)
#define GETCS(NAME) psoManager->get_cshader(#NAME)


//__vmstatic bool UpdatePainterUvAtlas(
//	vmobjects::VmParamMap<std::string, std::any>& ioResObjs,
//	vmobjects::VmParamMap<std::string, std::any>& ioActors,
//	vmobjects::VmParamMap<std::string, std::any>& ioParams);