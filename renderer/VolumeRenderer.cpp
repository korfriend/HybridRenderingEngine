#include "RendererHeader.h"
#include <time.h>

// Toggle SCULPT_PACKEDBITS upload path. Priority: TILED > TEX3D > default.
//   USE_SCULPT_BITS_TEX3D_TILED == 1 : upload as Texture3D<R32_UINT> with 4x4x2 = 32-voxel 3D tiles
//                                       packed into one texel. Best 3D cache locality.
//   USE_SCULPT_BITS_TEX3D       == 1 : upload as Texture3D<R32_UINT> with 32 voxels along X per texel.
//                                       Good Y/Z cache locality, X-only packing.
//   both == 0 (default)             : keep existing Buffer<uint> path (regression-safe baseline).
// Both C++ and HLSL toggles MUST be flipped together; otherwise the SRV type at t7 will mismatch
// the shader's declared type and reads will be undefined.
#define USE_SCULPT_BITS_TEX3D 1
#define USE_SCULPT_BITS_TEX3D_TILED 0

#define __RM_DEFAULT 0
#define __RM_MODULATION 1
#define __RM_MULTIOTF_MODULATION 2
#define __RM_CLIPOPAQUE 20
#define __RM_OPAQUE 21
#define __RM_OPAQUE_MULTIOTF 26
#define __RM_SCULPTMASK 22
#define __RM_SCULPTMASK_MODULATION 25
#define __RM_SAMPLETEST 99
#define __RM_MULTIOTF 23
#define __RM_VISVOLMASK 24
//#define __RM_TEST 6
#define __RM_RAYMAX 10
#define __RM_RAYMIN 11
#define __RM_RAYSUM 12
#define __RM_RAYMAX_SCULPTMASK 27
#define __RM_RAYMIN_SCULPTMASK 28
#define __RM_RAYSUM_SCULPTMASK 29
// X-ray slicer post-filter modes (__XRPF_*) and the shared kernel builder now live in
// renderer/RendererHeader.h (F10 dedup with the curved path in CurvedSlicerVR.cpp).
#define __SRV_PTR ID3D11ShaderResourceView*

using namespace grd_helper;

bool RenderVrDLS(VmFnContainer* _fncontainer,
	VmGpuManager* gpu_manager,
	grd_helper::PSOManager* psoManager,
	LocalProgress* progress,
	double* run_time_ptr)
{
#ifdef __DX_DEBUG_QUERY
	if (psoManager->debug_info_queue == NULL)
		psoManager->dx11Device->QueryInterface(__uuidof(ID3D11InfoQueue), (void **)&psoManager->debug_info_queue);
	psoManager->debug_info_queue->PushEmptyStorageFilter();
#endif


	//clock_t start = clock();
	
#pragma region // Parameter Setting //
	VmIObject* iobj = _fncontainer->fnParams.GetParam("_VmIObject*_RenderOut", (VmIObject*)NULL);
	bool isSlicer = _fncontainer->fnParams.GetParam("_bool_IsSlicer", false);
	int k_value_old = iobj->GetObjParam("_int_NumK", isSlicer? (int)K_NUM_SLICER : (int)K_NUM_3D);
	int k_value = _fncontainer->fnParams.GetParam("_int_NumK", k_value_old);
	iobj->SetObjParam("_int_NumK", k_value);

	// --- VXGI v2 (Voxel Cone Tracing GI: volumetric in-scatter) ------------------------------------
	// Volume-sourced radiance/opacity grid consumed by the DVR RayCasting march itself: the grid is built
	// (voxelize + inject, scene-gated) BEFORE the last volume's RayCasting dispatch and bound as SRV t8, so
	// the ray-march adds per-sample in-scattered radiance (gated in-shader by g_cbVxgi.vxgi_flag). The old
	// v1 screen-space gather post-pass is disabled below. All GPU work is guarded by vxgi_on (zero overhead
	// when off). CB_VXGI is bound at b13.
	bool vxgi_on = _fncontainer->fnParams.GetParam("_bool_VxgiEnabled", false);
	int vxgi_resolution = _fncontainer->fnParams.GetParam("_int_VxgiResolution", (int)128);
	float vxgi_gi_intensity = _fncontainer->fnParams.GetParam("_float_VxgiGiIntensity", 1.f);       // volumetric in-scatter (0 = off)
	float vxgi_ao_intensity = _fncontainer->fnParams.GetParam("_float_VxgiAoIntensity", 1.f);       // surface AO (0 = off)
	// (indirect_intensity retired — the v1..v4 screen-space surface-indirect term is gone; removed from
	// EnableVoxelGI too. SetCb_VXGI's indirect arg defaults to a harmless 1.f into the now-unused CB half.)
	int vxgi_debug = _fncontainer->fnParams.GetParam("_int_VxgiDebug", (int)0); // 0=render, 1..7=debug viz (screen gather)
	// Slow-motion diffusion toggle (SetRenderTestParam "_bool_VxgiSlowMotion"): 1 bounce per 8 rendered
	// frames so the progressive inward spread is observable; full speed when off. The frame index is the
	// GLOBAL monotonic render count forwarded by core (== vzmpf::GetRenderCount) — NOT count_call_render /
	// "_int_NumCallRenders", which is a per-frame pass counter reset at frame start (using it froze the
	// modulo at a constant, so slow-motion never advanced a bounce).
	bool vxgi_slowmo = _fncontainer->fnParams.GetParam("_bool_VxgiSlowMotion", false);
	uint64_t temporal_render_count = (uint64_t)_fncontainer->fnParams.GetParam("_uint64_TemporalRenderCount", (uint64_t)0);
	GpuRes gres_vxgi;
	bool vxgi_ready = false;
	ID3D11ShaderResourceView* vxgi_mat_srv_dbg = NULL; // MAT grid (coverage) for the debug gather pass
	ID3D11ShaderResourceView* vxgi_surf_srv_dbg = NULL; // SURF grid (Part C) for the debug gather pass ONLY (voxel modes 3..6)
	// Default the convergence demand to "done" each frame; the build block below overwrites it when it actually
	// runs. Prevents a stale bounce target from keeping the app's re-render loop spinning after VXGI is turned
	// off or the volume switches to an x-ray mode (where the block is skipped).
	iobj->SetObjParam("_int_VxgiBounceTarget", (int)0);

	vmfloat4 default_phong_lighting_coeff = vmfloat4(0.2, 1.0, 0.5, 5); // Emission, Diffusion, Specular, Specular Power
	bool force_to_update_otf = _fncontainer->fnParams.GetParam("_bool_ForceToUpdateOtf", false);
	bool show_block_test = _fncontainer->fnParams.GetParam("_bool_IsShowBlock", false);
	float v_thickness = (float)_fncontainer->fnParams.GetParam("_float_VZThickness", 0.f);
	bool check_pixel_transmittance = _fncontainer->fnParams.GetParam("_bool_PixelTransmittance", false);

	int camClipMode = _fncontainer->fnParams.GetParam("_int_ClippingMode", (int)0);
	vmfloat3 camClipPlanePos = _fncontainer->fnParams.GetParam("_float3_PosClipPlaneWS", vmfloat3(0));
	vmfloat3 camClipPlaneDir = _fncontainer->fnParams.GetParam("_float3_VecClipPlaneWS", vmfloat3(0));
	vmmat44f camClipMatWS2BS = _fncontainer->fnParams.GetParam("_matrix44f_MatrixClipWS2BS", vmmat44f(1));
	std::set<int> camClipperFreeActors = _fncontainer->fnParams.GetParam("_set_int_CamClipperFreeActors", std::set<int>());

	// X-ray slicer post-filter request (CameraParameters::EnableXRayPostFilter). Mode != NONE means "requested";
	// whether it actually runs is decided below (apply_postprocessing_filter), gated on an x-ray ray-cast mode.
	bool try_postprocessing_filter = _fncontainer->fnParams.GetParam("_int_XRayPostFilterMode", (int)__XRPF_NONE) != __XRPF_NONE;
	// Default false: the x-ray post-filter is non-DX10 only. In a DX10 build the decision below is
	// compiled out, so this must stay false there (otherwise bit 2 could be tagged with no fused pass).
	bool apply_postprocessing_filter = false;

	float merging_beta = (float)_fncontainer->fnParams.GetParam("_float_MergingBeta", 0.5f);
	bool is_rgba = _fncontainer->fnParams.GetParam("_bool_IsRGBA", false); // false means bgra
	bool is_ghost_mode = _fncontainer->fnParams.GetParam("_bool_GhostEffect", false);
	bool blur_SSAO = _fncontainer->fnParams.GetParam("_bool_BlurSSAO", true);
	bool without_sr = _fncontainer->fnParams.GetParam("_bool_IsFirstRenderer", false);
	bool test_consoleout = _fncontainer->fnParams.GetParam("_bool_TestConsoleOut", false);
	auto test_out = [&test_consoleout](const string& _message)
	{
		if (test_consoleout)
			cout << _message << endl;
	};

	bool apply_fragmerge = _fncontainer->fnParams.GetParam("_bool_ApplyFragMerge", true);
	MFR_MODE mode_OIT = (MFR_MODE)_fncontainer->fnParams.GetParam("_int_OitMode", (int)MFR_MODE::DYNAMIC_FB); // 1
	mode_OIT = (MFR_MODE)min((int)mode_OIT, (int)MFR_MODE::MOMENT);
#ifdef DX10_0
	//mode_OIT = MFR_MODE::NONE;
#endif
	//if (mode_OIT == MFR_MODE::STATIC_KB_FM) apply_fragmerge = true;

	int ray_cast_type_global = _fncontainer->fnParams.GetParam("_int_VolumeRayCastType", (int)0);

	int buf_ex_scale = _fncontainer->fnParams.GetParam("_int_BufExScale", (int)8); // 32 layers
	int num_moments_old = iobj->GetObjParam("_int_NumQueueLayers", (int)8);
	int num_moments = _fncontainer->fnParams.GetParam("_int_NumQueueLayers", num_moments_old);

	bool fastRender2x = _fncontainer->fnParams.GetParam("_bool_FastRender2X", false);
	//fastRender2x = true;

	int outline_thickness = _fncontainer->fnParams.GetParam("_int_SilhouetteThickness", (int)0);
	float outline_depthThres = _fncontainer->fnParams.GetParam("_float_SilhouetteDepthThres", 10000.f);
	vmfloat3 outline_color = _fncontainer->fnParams.GetParam("_float3_SilhouetteColor", vmfloat3(1));
	bool outline_fadeEffect = _fncontainer->fnParams.GetParam("_bool_SilhouetteFadeEffect", true);

	// TEST
	int test_value = _fncontainer->fnParams.GetParam("_int_TestValue", (int)0);
	int test_mode = _fncontainer->fnParams.GetParam("_int_TestMode", (int)0);

	bool reload_hlsl_objs = _fncontainer->fnParams.GetParam("_bool_ReloadHLSLObjFiles", false);

	float samplePrecisionLevel = _fncontainer->fnParams.GetParam("_float_SamplePrecisionLevel", 1.0f);
	float planeThickness = _fncontainer->fnParams.GetParam("_float_PlaneThickness", -1.f);

	VmLight* light = _fncontainer->fnParams.GetParamPtr<VmLight>("_VmLight_LightSource");
	VmLens* lens = _fncontainer->fnParams.GetParam("_VmLens*_CamLens", (VmLens*)NULL);
	LightSource light_src;
	GlobalLighting global_lighting;
	LensEffect lens_effect;
	if (light) {
		light_src.is_on_camera = light->is_on_camera;
		light_src.is_pointlight = light->is_pointlight;
		light_src.light_pos = light->pos;
		light_src.light_dir = light->dir;
		light_src.light_ambient_color = vmfloat3(1.f);
		light_src.light_diffuse_color = vmfloat3(1.f);
		light_src.light_specular_color = vmfloat3(1.f);

		global_lighting.apply_ssao = light->effect_ssao.is_on_ssao;
		global_lighting.ssao_r_kernel = light->effect_ssao.kernel_r;
		global_lighting.ssao_num_steps = light->effect_ssao.num_steps;
		global_lighting.ssao_num_dirs = light->effect_ssao.num_dirs;
		global_lighting.ssao_tangent_bias = light->effect_ssao.tangent_bias;
		global_lighting.ssao_blur = light->effect_ssao.smooth_filter;
		global_lighting.ssao_intensity = light->effect_ssao.ao_power;
		global_lighting.ssao_debug = _fncontainer->fnParams.GetParam("_int_SSAOOutput", (int)0);
	}
	if (lens) {
		lens_effect.apply_ssdof = lens->apply_ssdof;
		lens_effect.dof_focus_z = lens->dof_focus_z;
		lens_effect.dof_lens_F = lens->dof_lens_F;
		lens_effect.dof_lens_r = lens->dof_lens_r;
		lens_effect.dof_ray_num_samples = lens->dof_ray_num_samples;
	}
#pragma endregion


#pragma region // SHADER SETTING
	// HLSL hot-reload GENERATION — bumped every time the block below actually re-creates the compute shaders
	// from the .cso files on disk (i.e. after a ShaderCompile.bat run, with _bool_ReloadHLSLObjFiles armed).
	// VXGI caches its bake ACROSS frames and only re-runs Voxelize / BlurMat / InjectLight when its content
	// stamp moves. A pure shader swap touches no content, so without this the newly loaded bake shaders would
	// NEVER execute — the radiance grid would keep serving the field baked by the OLD code and the edit would
	// look like a no-op (the long-standing "nudge the AO pivot slider to make a Voxelize edit show up" dance).
	// Folding this counter into the VXGI stamps (see the stamp block below) makes a reload read as a content
	// change: full re-voxelize + re-inject + a hard restart of the diffusion bounces.
	// LEVEL-triggered on purpose, not edge: the flag is per (scene, camera) and the samples arm it for several
	// cameras independently, so a shared rising-edge latch here would oscillate between their DoModule calls.
	// The samples' idiom is one-shot (arm -> render -> disarm), so this bumps once or twice per press; the
	// stamp compare is an inequality, so an extra bump costs at most one redundant re-bake on a dev button.
	static uint64_t vxgi_hlsl_reload_gen = 0;

	// Shader Re-Compile Setting //
	if (reload_hlsl_objs)
	{
		vxgi_hlsl_reload_gen++;
		char ownPth[2048];
		GetModuleFileNameA(NULL, ownPth, (sizeof(ownPth)));
		string exe_path = ownPth;
		size_t pos = 0;
		std::string token;
		string delimiter = "\\";
		string hlslobj_path = "";
		while ((pos = exe_path.find(delimiter)) != std::string::npos) {
			token = exe_path.substr(0, pos);
			if (token.find(".exe") != std::string::npos) break;
			hlslobj_path += token + "\\";
			exe_path.erase(0, pos + delimiter.length());
		}

		hlslobj_path += "..\\..\\VmProjects\\RendererGPU\\";
		//cout << hlslobj_path << endl;
		string enginePath;
		if (grd_helper::GetEnginePath(enginePath)) {
			hlslobj_path = enginePath;
		}
		//string hlslobj_path_4_0 = hlslobj_path + "shader_compiled_objs_4_0\\";

#ifdef DX10_0
		hlslobj_path += "shader_compiled_objs_4_0\\";
#else
		hlslobj_path += "shader_compiled_objs\\";
#endif
		string prefix_path = hlslobj_path;
		vmlog::LogInfo("RELOAD HLSL _ VR renderer");

#ifdef DX10_0
#define PS_NUM 14
#define SET_PS(NAME) psoManager->safe_set_res(grd_helper::COMRES_INDICATOR(GpuhelperResType::PIXEL_SHADER, NAME), dx11PShader, true)

		string strNames_PS[PS_NUM] = {
			   "VR_RAYMAX_ps_4_0"
			  ,"VR_RAYMIN_ps_4_0"
			  ,"VR_RAYSUM_ps_4_0"
			  ,"VR_DEFAULT_ps_4_0"
			  ,"VR_OPAQUE_ps_4_0"
			  ,"VR_CONTEXT_ps_4_0"
			  ,"VR_MULTIOTF_ps_4_0"
			  ,"VR_MULTIOTF_CONTEXT_ps_4_0"
			  ,"VR_MASKVIS_ps_4_0"
			  ,"VR_SCULPTMASK_ps_4_0"
			  ,"VR_SCULPTMASK_CONTEXT_ps_4_0"
			  ,"VR_DEFAULT_SCULPTBITS_ps_4_0"
			  ,"VR_CONTEXT_SCULPTBITS_ps_4_0"
			  ,"VR_SURFACE_ps_4_0"
		};

		for (int i = 0; i < PS_NUM; i++)
		{
			string strName = strNames_PS[i];

			FILE* pFile;
			if (fopen_s(&pFile, (prefix_path + strName).c_str(), "rb") == 0)
			{
				fseek(pFile, 0, SEEK_END);
				uint64_t ullFileSize = ftell(pFile);
				fseek(pFile, 0, SEEK_SET);
				byte* pyRead = new byte[ullFileSize];
				fread(pyRead, sizeof(byte), ullFileSize, pFile);
				fclose(pFile);

				ID3D11PixelShader* dx11PShader = NULL;
				if (psoManager->dx11Device->CreatePixelShader(pyRead, ullFileSize, NULL, &dx11PShader) != S_OK)
				{
					VMERRORMESSAGE("SHADER COMPILE FAILURE!");
				}
				else
				{
					SET_PS(strName);
				}
				VMSAFE_DELETEARRAY(pyRead);
			}
		}
#else
#define CS_NUM 50
#define SET_CS(NAME) psoManager->safe_set_res(grd_helper::COMRES_INDICATOR(GpuhelperResType::COMPUTE_SHADER, NAME), dx11CShader, true)

		string strNames_CS[CS_NUM] = {
			   "VR_RAYMAX_cs_5_0"
			  ,"VR_RAYMIN_cs_5_0"
			  ,"VR_RAYSUM_cs_5_0"
			  ,"VR_RAYMAX_SCULPTMASK_cs_5_0"
			  ,"VR_RAYMIN_SCULPTMASK_cs_5_0"
			  ,"VR_RAYSUM_SCULPTMASK_cs_5_0"
			  ,"VR_DEFAULT_cs_5_0"
			  ,"VR_OPAQUE_cs_5_0"
			  ,"VR_CONTEXT_cs_5_0"
			  ,"VR_MULTIOTF_cs_5_0"
			  ,"VR_MASKVIS_cs_5_0"
			  ,"VR_DEFAULT_FM_cs_5_0"
			  ,"VR_OPAQUE_FM_cs_5_0"
			  ,"VR_OPAQUE_MULTIOTF_FM_cs_5_0"
			  ,"VR_CINEMATIC_FM_cs_5_0"
			  ,"VR_CONTEXT_FM_cs_5_0"
			  ,"VR_MULTIOTF_FM_cs_5_0"
			  ,"VR_MULTIOTF_CONTEXT_FM_cs_5_0"
			  ,"VR_MASKVIS_FM_cs_5_0"
			  ,"VR_SCULPTMASK_FM_cs_5_0"
			  ,"VR_SCULPTMASK_CONTEXT_FM_cs_5_0"
			  ,"VR_DEFAULT_SCULPTBITS_FM_cs_5_0"
			  ,"VR_CONTEXT_SCULPTBITS_FM_cs_5_0"
			  ,"VR_DEFAULT_DKBZ_cs_5_0"
			  ,"VR_OPAQUE_DKBZ_cs_5_0"
			  ,"VR_CONTEXT_DKBZ_cs_5_0"
			  ,"VR_DEFAULT_DFB_cs_5_0"
			  ,"VR_OPAQUE_DFB_cs_5_0"
			  ,"VR_CONTEXT_DFB_cs_5_0"
			  ,"VR_SURFACE_cs_5_0"
			  ,"FillDither_cs_5_0"
			  ,"SampleTest_cs_5_0"
			  ,"VR_SINGLE_DEFAULT_FM_cs_5_0"
			  ,"VR_SINGLE_OPAQUE_FM_cs_5_0"
			  ,"VR_SINGLE_OPAQUE_MULTIOTF_FM_cs_5_0"
			  ,"VR_SINGLE_CONTEXT_FM_cs_5_0"
			  ,"VR_SINGLE_MULTIOTF_FM_cs_5_0"
			  ,"VR_SINGLE_MULTIOTF_CONTEXT_FM_cs_5_0"
			  ,"VR_SINGLE_MASKVIS_FM_cs_5_0"
			  ,"VR_SINGLE_SCULPTMASK_FM_cs_5_0"
			  ,"VR_SINGLE_SCULPTMASK_CONTEXT_FM_cs_5_0"
			  ,"VR_SINGLE_DEFAULT_SCULPTBITS_FM_cs_5_0"
			  ,"VR_SINGLE_CONTEXT_SCULPTBITS_FM_cs_5_0"
			  ,"XrayFilterComposite_cs_5_0"
			,"VXGI_VoxelizeVolume_cs_5_0"
			,"VXGI_InjectLight_cs_5_0"
			,"VXGI_Gather_cs_5_0"
			,"VXGI_Propagate_cs_5_0"
			,"VXGI_SurfaceGather_cs_5_0"
			,"VXGI_BlurMat_cs_5_0"
		};

		for (int i = 0; i < CS_NUM; i++)
		{
			string strName = strNames_CS[i];

			FILE* pFile;
			if (fopen_s(&pFile, (prefix_path + strName).c_str(), "rb") == 0)
			{
				fseek(pFile, 0, SEEK_END);
				uint64_t ullFileSize = ftell(pFile);
				fseek(pFile, 0, SEEK_SET);
				byte* pyRead = new byte[ullFileSize];
				fread(pyRead, sizeof(byte), ullFileSize, pFile);
				fclose(pFile);

				ID3D11ComputeShader* dx11CShader = NULL;
				if (psoManager->dx11Device->CreateComputeShader(pyRead, ullFileSize, NULL, &dx11CShader) != S_OK)
				{
					VMERRORMESSAGE("SHADER COMPILE FAILURE!");
				}
				else
				{
					SET_CS(strName);
				}
				VMSAFE_DELETEARRAY(pyRead);
			}
		}
#endif
	}
#pragma endregion 

#pragma region // IOBJECT OUT
	//while (iobj->GetFrameBuffer(FrameBufferUsageRENDEROUT, 2) != NULL)
	//	iobj->DeleteFrameBuffer(FrameBufferUsageRENDEROUT, 2);
	if (!iobj->ReplaceFrameBuffer(FrameBufferUsageRENDEROUT, 0, data_type::dtype<vmbyte4>(), ("common render out frame buffer : defined in vismtv_inbuilt_renderergpudx module")))
		iobj->InsertFrameBuffer(data_type::dtype<vmbyte4>(), FrameBufferUsageRENDEROUT, ("common render out frame buffer : defined in vismtv_inbuilt_renderergpudx module"));

	//while (iobj->GetFrameBuffer(FrameBufferUsageDEPTH, 1) != NULL)
	//	iobj->DeleteFrameBuffer(FrameBufferUsageDEPTH, 1);
	if (!iobj->ReplaceFrameBuffer(FrameBufferUsageDEPTH, 0, data_type::dtype<float>(), ("1st hit screen depth frame buffer : defined in vismtv_inbuilt_renderergpudx module")))
		iobj->InsertFrameBuffer(data_type::dtype<float>(), FrameBufferUsageDEPTH, ("1st hit screen depth frame buffer : defined in vismtv_inbuilt_renderergpudx module"));
#pragma endregion 

	__ID3D11Device* pdx11Device = psoManager->dx11Device;
	__ID3D11DeviceContext* dx11DeviceImmContext = psoManager->dx11DeviceImmContext;

#pragma region // IOBJECT GPU
	//int buffer_ex = 1, buffer_ex_old = 0; // optimal for K is 1
	int buffer_ex = (check_pixel_transmittance && mode_OIT == MFR_MODE::DYNAMIC_FB) ? buf_ex_scale : 1, buffer_ex_old = 0; // optimal for K is 1
	// 'cause we do not support the dynamic version of k+ buffer
	// it always uses static number of k!!
	// note that DFB uses a simple fragment model (vis and depth) and the stored simple fragments are extended into the z-thickness model fragments in the resolve pass

	vmint2 fb_size_cur;
	iobj->GetFrameBufferInfo(&fb_size_cur);
	vmint2 fb_size_old = iobj->GetObjParam("_int2_PreviousScreenSize", vmint2(0, 0));
	buffer_ex_old = iobj->GetObjParam("_int_PreviousBufferEx", buffer_ex_old);
	if (fb_size_cur.x != fb_size_old.x || fb_size_cur.y != fb_size_old.y || k_value != k_value_old
		|| k_value != k_value_old || num_moments != num_moments_old
		|| buffer_ex != buffer_ex_old)
	{
		gpu_manager->ReleaseGpuResourcesBySrcID(iobj->GetObjectID());	// System Out //
		iobj->SetObjParam("_int2_PreviousScreenSize", fb_size_cur);
		iobj->SetObjParam("_int_PreviousBufferEx", buffer_ex);
	}


//#define __COUNT_DEBUG
#ifdef __COUNT_DEBUG
	GpuRes gres_fb_counter_sys;
	{
		grd_helper::UpdateFrameBuffer(gres_fb_counter_sys, iobj, "SYSTEM_COUNTER", RTYPE_TEXTURE2D, NULL, DXGI_FORMAT_R32_UINT, UPFB_SYSOUT);

		dx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gres_fb_counter_sys.alloc_res_ptrs[DTYPE_RES],
			(ID3D11Texture2D*)gres_fb_counter.alloc_res_ptrs[DTYPE_RES]);

		D3D11_MAPPED_SUBRESOURCE mappedResSysTest;
		HRESULT hr = dx11DeviceImmContext->Map((ID3D11Texture2D*)gres_fb_counter_sys.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, NULL, &mappedResSysTest);
		int buf_row_pitch = mappedResSysTest.RowPitch / 4;
		uint32_t* __count = (uint32_t*)mappedResSysTest.pData;
		for (int i = 0; i < fb_size_cur.y; i++)
		{
			for (int j = 0; j < fb_size_cur.x; j++)
			{
				if(__count[j + i * buf_row_pitch] > 0)
					int gg = 0;
			}
		};
		dx11DeviceImmContext->Unmap((ID3D11Texture2D*)gres_fb_counter_sys.alloc_res_ptrs[DTYPE_RES], 0);
	}
#endif

	GpuRes gres_fb_sys_rgba, gres_fb_sys_depthcs;
	grd_helper::UpdateFrameBuffer(gres_fb_sys_rgba, iobj, "SYSTEM_OUT_RGBA", RTYPE_TEXTURE2D, NULL, DXGI_FORMAT_R8G8B8A8_UNORM, UPFB_SYSOUT);
	grd_helper::UpdateFrameBuffer(gres_fb_sys_depthcs, iobj, "SYSTEM_OUT_DEPTH", RTYPE_TEXTURE2D, NULL, DXGI_FORMAT_R32_FLOAT, UPFB_SYSOUT);

#ifdef DX10_0
	const uint32_t rtbind = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	
	GpuRes gres_fb_rgba, gres_fb_depthcs;
	grd_helper::UpdateFrameBuffer(gres_fb_rgba, iobj, "RENDER_OUT_RGBA_1", RTYPE_TEXTURE2D, rtbind, __COLOR_RT_FORMAT, 0);
	grd_helper::UpdateFrameBuffer(gres_fb_depthcs, iobj, "RENDER_OUT_DEPTH_1", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R32_FLOAT, 0);
	
	GpuRes gres_fb_vrdepthcs, gres_fb_vrenc;
	grd_helper::UpdateFrameBuffer(gres_fb_vrdepthcs, iobj, "RENDER_OUT_DEPTH_2", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R32_FLOAT, 0);
	grd_helper::UpdateFrameBuffer(gres_fb_vrenc, iobj, "RENDER_OUT_VRENC", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R8_UINT, 0);

	GpuRes gres_fb_rgba_prev, gres_fb_depthcs_prev;
	grd_helper::UpdateFrameBuffer(gres_fb_rgba_prev, iobj, "RENDER_OUT_RGBA_0", RTYPE_TEXTURE2D, rtbind, __COLOR_RT_FORMAT, 0);
	grd_helper::UpdateFrameBuffer(gres_fb_depthcs_prev, iobj, "RENDER_OUT_DEPTH_0", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R32_FLOAT, 0);
#else
	const uint32_t rtbind = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

	GpuRes gres_fb_rgba, gres_fb_depthcs, gres_fb_vrdepthcs;
	grd_helper::UpdateFrameBuffer(gres_fb_rgba, iobj, "RENDER_OUT_RGBA_0", RTYPE_TEXTURE2D, rtbind, __COLOR_RT_FORMAT, 0);
	grd_helper::UpdateFrameBuffer(gres_fb_depthcs, iobj, "RENDER_OUT_DEPTH_0", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R32_FLOAT, 0);
	grd_helper::UpdateFrameBuffer(gres_fb_vrdepthcs, iobj, "RENDER_OUT_DEPTH_1", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R32_FLOAT, 0);

	// Slicer x-ray post-processing filter scratch + kernel resources. Declared here so they are
	// visible to the DVR UAV binding / fused dispatch. gres_fb_xray_vol is NOT a new allocation: in the
	// Presetting region (only when apply_postprocessing_filter is decided true) it is pointed at the
	// existing "RENDER_OUT_RGBA_1" pool buffer, which is idle during the volume pass. The mask buffer is
	// filled/uploaded at the fused-dispatch site (its weights depend on per-frame profile params).
	//  gres_fb_xray_vol : volume-only x-ray color redirect target (DvrCS bit2 writes here at u2);
	//                     also the convolution input for the fused XrayFilterComposite pass. RGBA8 RT|SRV|UAV.
	//  gres_xray_filter_mask : NxN convolution weights (Buffer<float> SRV), filled from a named profile.
	GpuRes gres_fb_xray_vol;
	GpuRes gres_xray_filter_mask;

	GpuRes gres_fb_k_buffer, gres_fb_counter;
	GpuRes gres_fb_ao_texs[2], gres_fb_ao_blf_texs[2];
	//GpuRes gres_fb_mip_a_halftexs[2], gres_fb_mip_z_halftexs[2]; // deprecated
	GpuRes gres_fb_ao_vr_tex, gres_fb_ao_vr_blf_tex;
	GpuRes gres_fb_ref_pidx;

	const int num_frags_perpixel = k_value * 3 * buffer_ex;
	grd_helper::UpdateFrameBuffer(gres_fb_k_buffer, iobj, "BUFFER_RW_K_BUF", RTYPE_BUFFER,
		D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_TYPELESS, UPFB_RAWBYTE, num_frags_perpixel);

	grd_helper::UpdateFrameBuffer(gres_fb_counter, iobj, "RW_COUNTER", RTYPE_TEXTURE2D,
		D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_UINT, 0);
	// AO
	if (0)
	{
		grd_helper::UpdateFrameBuffer(gres_fb_ao_texs[0], iobj, "RW_TEXS_AO_0", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, UPFB_MIPMAP);
		grd_helper::UpdateFrameBuffer(gres_fb_ao_texs[1], iobj, "RW_TEXS_AO_1", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, UPFB_MIPMAP);
		grd_helper::UpdateFrameBuffer(gres_fb_ao_blf_texs[0], iobj, "RW_TEXS_AO_BLF_0", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, UPFB_MIPMAP);
		grd_helper::UpdateFrameBuffer(gres_fb_ao_blf_texs[1], iobj, "RW_TEXS_AO_BLF_1", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, UPFB_MIPMAP);
		//grd_helper::UpdateFrameBuffer(gres_fb_mip_a_halftexs[0], iobj, "RW_TEXS_OPACITY_0", RTYPE_TEXTURE2D,
		//	D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, UPFB_MIPMAP | UPFB_HALF);
		//grd_helper::UpdateFrameBuffer(gres_fb_mip_a_halftexs[1], iobj, "RW_TEXS_OPACITY_1", RTYPE_TEXTURE2D,
		//	D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, UPFB_MIPMAP | UPFB_HALF);
		//grd_helper::UpdateFrameBuffer(gres_fb_mip_z_halftexs[0], iobj, "RW_TEXS_Z_0", RTYPE_TEXTURE2D,
		//	D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32G32B32A32_FLOAT, UPFB_MIPMAP | UPFB_HALF);
		//grd_helper::UpdateFrameBuffer(gres_fb_mip_z_halftexs[1], iobj, "RW_TEXS_Z_1", RTYPE_TEXTURE2D,
		//	D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32G32B32A32_FLOAT, UPFB_MIPMAP | UPFB_HALF);

		grd_helper::UpdateFrameBuffer(gres_fb_ao_vr_tex, iobj, "RW_TEX_AO_VR", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8_UNORM, UPFB_MIPMAP);
		grd_helper::UpdateFrameBuffer(gres_fb_ao_vr_blf_tex, iobj, "RW_TEX_AO_VR_BLF", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8_UNORM, UPFB_MIPMAP);
	}

	if (mode_OIT == MFR_MODE::DYNAMIC_FB || mode_OIT == MFR_MODE::DYNAMIC_KB)
		grd_helper::UpdateFrameBuffer(gres_fb_ref_pidx, iobj, "BUFFER_RW_REF_PIDX_BUF", RTYPE_BUFFER, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_UINT, 0);
#endif
#pragma endregion 

#pragma region // Presetting of VmObject
	vector<VmActor*> dvr_volumes;
	vmfloat3 pos_aabb_min_ws(FLT_MAX), pos_aabb_max_ws(-FLT_MAX);
	float min_pitch = FLT_MAX;
	// For Each Primitive //
	for (auto& actorPair : _fncontainer->sceneActors)
	{
		VmActor* actor = get<1>(actorPair);
		VmVObject* geo_obj = actor->GetGeometryRes();
		if (geo_obj == NULL ||
			geo_obj->GetObjectType() != ObjectTypeVOLUME ||
			!geo_obj->IsDefined() ||
			!actor->visible || actor->color.a == 0)
			continue;

		dvr_volumes.push_back(actor);
		VmVObjectVolume* volobj = (VmVObjectVolume*)geo_obj;
		VolumeData* vol_data = volobj->GetVolumeData();

		min_pitch = (float)std::min(std::min(
			std::min(vol_data->vox_pitch.x, vol_data->vox_pitch.y),
			vol_data->vox_pitch.z), 
			(double)min_pitch);
	}

	GpuRes gres_fb_thickcs;
	if (dvr_volumes.size() > 1)
	{
		grd_helper::UpdateFrameBuffer(gres_fb_thickcs, iobj, "RENDER_OUT_THICK_0", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R32_FLOAT, 0);
	}

	// Decide the x-ray image-level post-filter HERE (presetting), not mid-render. This flag means only
	// "is the feature ON?" and is independent of the volume-actor count. It depends on:
	//   - the post-filter was requested (_int_XRayPostFilterMode != NONE), AND
	//   - the global ray-cast mode is x-ray (MIP/MinIP/Raysum, incl. sculpt-mask variants).
	// Multi-volume is supported: earlier (non-last) volumes accumulate into the RT (gres_fb_rgba) as usual;
	// only the LAST DVR volume is redirected to the volume-only scratch and then filtered+composited. That
	// "where" is gated separately by is_last_dvr at the render sites below — this flag is just the "whether".
	// DX10 is intentionally unsupported for this pass: its non-DX10-only resources (gres_fb_xray_vol,
	// the K-buffer) don't exist there, so the whole decision is compiled out and apply_postprocessing_filter
	// stays false in a DX10 build.
#ifndef DX10_0
	{
		const bool global_is_xray =
			ray_cast_type_global == __RM_RAYMAX || ray_cast_type_global == __RM_RAYMIN || ray_cast_type_global == __RM_RAYSUM ||
			ray_cast_type_global == __RM_RAYMAX_SCULPTMASK || ray_cast_type_global == __RM_RAYMIN_SCULPTMASK || ray_cast_type_global == __RM_RAYSUM_SCULPTMASK;
		apply_postprocessing_filter = try_postprocessing_filter && global_is_xray;
		if (apply_postprocessing_filter)
			// No dedicated allocation: reuse the existing "RENDER_OUT_RGBA_1" scratch (RGBA8, RT|SRV|UAV;
			// PrimitiveRenderer's single-layer buffer). The non-DX10 VolumeRenderer does not otherwise use
			// it (its own RT is "RENDER_OUT_RGBA_0"), the surface DoModule that uses it has already completed,
			// and the volume renderer is the final renderer — so it is idle for the duration of this pass.
			// The volume slab thickness (u5) reuses gres_fb_vrdepthcs ("RENDER_OUT_DEPTH_1") the same way:
			// it is always allocated and VR_SURFACE (its only writer) is skipped in x-ray mode, so it is idle.
			grd_helper::UpdateFrameBuffer(gres_fb_xray_vol, iobj, "RENDER_OUT_RGBA_1", RTYPE_TEXTURE2D, rtbind, __COLOR_RT_FORMAT, 0);
	}
#endif // !DX10_0
#pragma endregion

#ifdef DX10_0
	if (dvr_volumes.size() > 1) {
		vmlog::LogWarn("WARNNING!! multiple volume-actors are not allowed!");
		vmlog::LogWarn("WARNNING!! force to use the final one as a main volume actor!");
		VmActor* main_actor = dvr_volumes[dvr_volumes.size() - 1];
		dvr_volumes.clear();
		dvr_volumes.push_back(main_actor);
	}
#endif

	int count_call_render = iobj->GetObjParam("_int_NumCallRenders", (int)0);

	// Backup Previous Render Target //
	ID3D11RenderTargetView* pdxRTVOld = NULL;
	ID3D11DepthStencilView* pdxDSVOld = NULL;
	dx11DeviceImmContext->OMGetRenderTargets(1, &pdxRTVOld, &pdxDSVOld);

	float flt_max_ = FLT_MAX;
	uint32_t flt_max_u = *(uint32_t*)&flt_max_;
	uint32_t clr_unit4[4] = { 0, 0, 0, 0 };
	uint32_t clr_max_ufloat_4[4] = { flt_max_u, flt_max_u, flt_max_u, flt_max_u };
	float clr_float_zero_4[4] = { 0, 0, 0, 0 };
	float clr_float_fltmax_4[4] = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };
	float clr_float_minus_4[4] = { -1.f, -1.f, -1.f, -1.f };
	if (without_sr)
	{
#ifdef DX10_0
		//dx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_RTV], clr_float_zero_4);
		//dx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_RTV], clr_float_fltmax_4);
		dx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gres_fb_rgba_prev.alloc_res_ptrs[DTYPE_RTV], clr_float_zero_4);
		dx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gres_fb_depthcs_prev.alloc_res_ptrs[DTYPE_RTV], clr_float_fltmax_4);
		//dx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gres_fb_vrdepthcs.alloc_res_ptrs[DTYPE_RTV], clr_float_fltmax_4);
		//dx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gres_fb_vrenc.alloc_res_ptrs[DTYPE_RTV], clr_float_zero_4);
#else
		dx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gres_fb_counter.alloc_res_ptrs[DTYPE_UAV], clr_unit4);
		//dx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gres_fb_k_buffer.alloc_res_ptrs[DTYPE_UAV], clr_unit4);
		dx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_UAV], clr_unit4);
		dx11DeviceImmContext->ClearUnorderedAccessViewFloat((ID3D11UnorderedAccessView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_UAV], clr_float_fltmax_4);
		if (dvr_volumes.size() > 1)
			dx11DeviceImmContext->ClearUnorderedAccessViewFloat((ID3D11UnorderedAccessView*)gres_fb_thickcs.alloc_res_ptrs[DTYPE_UAV], clr_float_zero_4);
		// note that gres_fb_vrdepthcs is supposed to be initialized in VR_SURFACE
#endif
		count_call_render = 0;
	}
	else
	{
#ifndef DX10_0
		dx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_RTV], clr_float_zero_4);
		dx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_RTV], clr_float_fltmax_4);
#endif
	}

	ID3D11Buffer* cbuf_cam_state = psoManager->get_cbuf("CB_CameraState");
	ID3D11Buffer* cbuf_env_state = psoManager->get_cbuf("CB_EnvState");
	ID3D11Buffer* cbuf_clip = psoManager->get_cbuf("CB_ClipInfo");
	ID3D11Buffer* cbuf_pobj = psoManager->get_cbuf("CB_PolygonObject");
	ID3D11Buffer* cbuf_vobj = psoManager->get_cbuf("CB_VolumeObject");
	ID3D11Buffer* cbuf_reffect = psoManager->get_cbuf("CB_Material");
	ID3D11Buffer* cbuf_vreffect = psoManager->get_cbuf("CB_VolumeMaterial");
	ID3D11Buffer* cbuf_tmap = psoManager->get_cbuf("CB_TMAP");
	ID3D11Buffer* cbuf_hsmask = psoManager->get_cbuf("CB_HotspotMask");
	ID3D11Buffer* cbuf_testBuffer = psoManager->get_cbuf("CB_TestBuffer");

#pragma region // HLSL Sampler Setting
	ID3D11SamplerState* sampler_PZ = psoManager->get_sampler("POINT_ZEROBORDER");
	ID3D11SamplerState* sampler_LZ = psoManager->get_sampler("LINEAR_ZEROBORDER");
	ID3D11SamplerState* sampler_PC = psoManager->get_sampler("POINT_CLAMP");
	ID3D11SamplerState* sampler_LC = psoManager->get_sampler("LINEAR_CLAMP");

#ifdef DX10_0
#define SET_SAMPLERS dx11DeviceImmContext->PSSetSamplers
#define SET_CBUFFERS dx11DeviceImmContext->PSSetConstantBuffers
#define SET_SHADER_RES dx11DeviceImmContext->PSSetShaderResources
#define SET_SHADER dx11DeviceImmContext->PSSetShader

	ID3D11RenderTargetView* dx11RTVs_NULL[2] = { NULL, NULL };

	GpuRes gres_quad;
	gres_quad.vm_src_id = 1;
	gres_quad.res_name = string("PROXY_QUAD");

	if (gpu_manager->UpdateGpuResource(gres_quad))
	{
		D3D11_MAPPED_SUBRESOURCE mappedResPobjData;
		dx11DeviceImmContext->Map(cbuf_pobj, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResPobjData);
		CB_PolygonObject* cbPolygonObjData = (CB_PolygonObject*)mappedResPobjData.pData;

		vmmat44f matQaudWS2CS;
		vmmath::fMatrixWS2CS(&matQaudWS2CS, &vmfloat3(0, 0, 1), &vmfloat3(0, 1, 0), &vmfloat3(0, 0, -1));
		vmmat44f matQaudCS2PS;
		vmmath::fMatrixOrthogonalCS2PS(&matQaudCS2PS, 2.f, 2.f, 0, 2.f);
		vmmat44f matQaudWS2PS = matQaudWS2CS * matQaudCS2PS;
		cbPolygonObjData->mat_os2ps = TRANSPOSE(matQaudWS2PS);
		dx11DeviceImmContext->Unmap(cbuf_pobj, 0);
		dx11DeviceImmContext->VSSetConstantBuffers(1, 1, &cbuf_pobj);


		ID3D11InputLayout* dx11LI_P = (ID3D11InputLayout*)psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "P"));
		ID3D11VertexShader* dx11VShader_Quad = (ID3D11VertexShader*)psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_QUAD_P_vs_4_0"));

		ID3D11Buffer* dx11BufferTargetPrim = (ID3D11Buffer*)gres_quad.alloc_res_ptrs[DTYPE_RES];
		//ID3D11Buffer* dx11IndiceTargetPrim = NULL;
		uint32_t stride_inputlayer = sizeof(vmfloat3);
		uint32_t offset = 0;
		dx11DeviceImmContext->IASetVertexBuffers(0, 1, (ID3D11Buffer**)&dx11BufferTargetPrim, &stride_inputlayer, &offset);
		dx11DeviceImmContext->IASetInputLayout(dx11LI_P);
		dx11DeviceImmContext->VSSetShader(dx11VShader_Quad, NULL, 0);
		dx11DeviceImmContext->GSSetShader(NULL, NULL, 0);
		dx11DeviceImmContext->PSSetShader(NULL, NULL, 0);
		dx11DeviceImmContext->RSSetState(psoManager->get_rasterizer("SOLID_NONE"));
		dx11DeviceImmContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		dx11DeviceImmContext->OMSetDepthStencilState(psoManager->get_depthstencil("ALWAYS"), 0);
	}
	else assert(0);

	D3D11_RECT rects[1];
	rects[0].left = 0;
	rects[0].right = fb_size_cur.x;
	rects[0].top = 0;
	rects[0].bottom = fb_size_cur.y;
	dx11DeviceImmContext->RSSetScissorRects(1, rects);

	// View Port Setting //
	D3D11_VIEWPORT dx11ViewPort;
	dx11ViewPort.Width = (float)fb_size_cur.x;
	dx11ViewPort.Height = (float)fb_size_cur.y;
	dx11ViewPort.MinDepth = 0;
	dx11ViewPort.MaxDepth = 1;
	dx11ViewPort.TopLeftX = 0;
	dx11ViewPort.TopLeftY = 0;
	dx11DeviceImmContext->RSSetViewports(1, &dx11ViewPort);

#else
#define SET_SAMPLERS dx11DeviceImmContext->CSSetSamplers
#define SET_CBUFFERS dx11DeviceImmContext->CSSetConstantBuffers
#define SET_SHADER_RES dx11DeviceImmContext->CSSetShaderResources
#define SET_SHADER dx11DeviceImmContext->CSSetShader
#endif

	SET_SAMPLERS(0, 1, &sampler_LZ);
	SET_SAMPLERS(1, 1, &sampler_PZ);
	SET_SAMPLERS(2, 1, &sampler_LC);
	SET_SAMPLERS(3, 1, &sampler_PC);
#pragma endregion

	ID3D11UnorderedAccessView* dx11UAVs_NULL[10] = { NULL };
	ID3D11ShaderResourceView* dx11SRVs_NULL[10] = { NULL };

#pragma region // Camera & Environment 
// 	const int __BLOCKSIZE = 8;
// 	uint32_t num_grid_x = (uint32_t)ceil(fb_size_cur.x / (float)__BLOCKSIZE);
// 	uint32_t num_grid_y = (uint32_t)ceil(fb_size_cur.y / (float)__BLOCKSIZE);
	const int __BLOCKSIZE = _fncontainer->fnParams.GetParam("_int_GpuThreadBlockSize", (int)8);
	uint32_t num_grid_x = __BLOCKSIZE == 1 ? fb_size_cur.x : (uint32_t)ceil(fb_size_cur.x / (float)__BLOCKSIZE);
	uint32_t num_grid_y = __BLOCKSIZE == 1 ? fb_size_cur.y : (uint32_t)ceil(fb_size_cur.y / (float)__BLOCKSIZE);

	VmCObject* cam_obj = iobj->GetCameraObject();
	vmmat44 dmatWS2CS, dmatCS2PS, dmatPS2SS;
	vmmat44 dmatSS2PS, dmatPS2CS, dmatCS2WS;
	cam_obj->GetMatrixWStoSS(&dmatWS2CS, &dmatCS2PS, &dmatPS2SS);
	cam_obj->GetMatrixSStoWS(&dmatSS2PS, &dmatPS2CS, &dmatCS2WS);
	vmmat44 dmatWS2PS = dmatWS2CS * dmatCS2PS;
	vmmat44f matWS2CS = dmatWS2CS;
	vmmat44f matWS2PS = dmatWS2PS;
	vmmat44f matWS2SS = dmatWS2PS * dmatPS2SS;
	vmmat44f matSS2WS = (dmatSS2PS * dmatPS2CS) * dmatCS2WS;

	// TAA sub-pixel jitter (renderer-owned; computed once per frame on the iobj, zero when TAA is off). Same
	// offset for every render source this frame.
	vmfloat2 taa_jitter = iobj->GetObjParam<vmfloat2>("_float2_TaaJitterPx", vmfloat2(0.f, 0.f));
	CB_CameraState cbCamState;
	grd_helper::SetCb_Camera(cbCamState, matWS2SS, matSS2WS, matWS2CS, matWS2PS, cam_obj, fb_size_cur, k_value, v_thickness <= 0? min_pitch : (float)v_thickness, taa_jitter);
	cbCamState.iSrCamDummy__0 = *(uint32_t*)&merging_beta;
	if (fastRender2x) cbCamState.cam_flag |= 0x1 << 8; // 9th bit set
	int oulineiRGB = (int)(outline_color.r * 255.f) | (int)(outline_color.g * 255.f) << 8 | (int)(outline_color.b * 255.f) << 16;
	outline_thickness = min(32, outline_thickness);
	cbCamState.iSrCamDummy__1 = oulineiRGB | outline_thickness << 24;
	//cbCamState.iSrCamDummy__2 = *(uint32_t*)&scale_z_res;
	cbCamState.cam_flag |= ((int)outline_fadeEffect << 9); //
	if (isSlicer) cbCamState.cam_flag |= 0x1 << 10;
	
	D3D11_MAPPED_SUBRESOURCE mappedResCamState;
	dx11DeviceImmContext->Map(cbuf_cam_state, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResCamState);
	CB_CameraState* cbCamStateData = (CB_CameraState*)mappedResCamState.pData;
	memcpy(cbCamStateData, &cbCamState, sizeof(CB_CameraState));
	dx11DeviceImmContext->Unmap(cbuf_cam_state, 0);
	SET_CBUFFERS(0, 1, &cbuf_cam_state);

	CB_EnvState cbEnvState;
	grd_helper::SetCb_Env(cbEnvState, cam_obj, light_src, global_lighting, lens_effect);
	cbEnvState.env_flag |= 0x2;
	D3D11_MAPPED_SUBRESOURCE mappedResEnvState;
	dx11DeviceImmContext->Map(cbuf_env_state, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResEnvState);
	CB_EnvState* cbEnvStateData = (CB_EnvState*)mappedResEnvState.pData;
	memcpy(cbEnvStateData, &cbEnvState, sizeof(CB_EnvState));
	dx11DeviceImmContext->Unmap(cbuf_env_state, 0);
	SET_CBUFFERS(7, 1, &cbuf_env_state);

	if (is_ghost_mode)
	{
		// do 'dynamic'
		D3D11_MAPPED_SUBRESOURCE mappedResHSMask;
		dx11DeviceImmContext->Map(cbuf_hsmask, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResHSMask);
		CB_HotspotMask* cbHSMaskData = (CB_HotspotMask*)mappedResHSMask.pData;
		grd_helper::SetCb_HotspotMask(*cbHSMaskData, _fncontainer, matWS2SS);
		dx11DeviceImmContext->Unmap(cbuf_hsmask, 0);
		SET_CBUFFERS(9, 1, &cbuf_hsmask);
	}

	//	
	bool testMode = _fncontainer->fnParams.GetParam("_bool_UseTestBuffer", false);
	if (testMode)
	{
		// test to do 
		float myTestV0 = _fncontainer->fnParams.GetParam("myTestValue", 0.f);

		CB_TestBuffer cbTestBuffer = {};
		// test to do 
		//cbTestBuffer.testFloatValues[0] = myTestV0;
		//cbTestBuffer.testFloatValues[1] = 678.9f;
		cbTestBuffer.testA = 777;
		cbTestBuffer.testB = 888;

		D3D11_MAPPED_SUBRESOURCE mappedTestBuffer;
		dx11DeviceImmContext->Map(cbuf_testBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedTestBuffer);
		CB_TestBuffer* cbTestBufferData = (CB_TestBuffer*)mappedTestBuffer.pData;
		memcpy(cbTestBufferData, &cbTestBuffer, sizeof(CB_TestBuffer));
		dx11DeviceImmContext->Unmap(cbuf_testBuffer, 0);
		SET_CBUFFERS(8, 1, &cbuf_testBuffer);
	}
#pragma endregion // Light & Shadow Setting

	psoManager->GpuProfile("VR Begin");

	// Initial Setting of Frame Buffers //
	bool is_performed_ssao = false;

	int vr_render_count = 0;
	for (VmActor* actor : dvr_volumes)
	{
		bool is_last_dvr = vr_render_count == dvr_volumes.size() - 1;
		VmVObjectVolume* vobj = (VmVObjectVolume*)actor->GetGeometryRes();
		VolumeData* vol_data = vobj->GetVolumeData();

		// note that the actor is visible (already checked)
#pragma region Actor Parameters

		// will change integer to string type 
		// and its param name 'RaySamplerMode'
		// also its corresponding cpu renderer
		bool is_xray_mode = false;
		bool is_sculpt_mode = false;
		bool is_modulation_mode = false;
		vmfloat2 grad_minmax(FLT_MAX, -FLT_MAX);
		int ray_cast_type = is_last_dvr ? ray_cast_type_global : __RM_OPAQUE;
		switch (ray_cast_type) {
		case __RM_RAYMAX_SCULPTMASK:
		case __RM_RAYMIN_SCULPTMASK:
		case __RM_RAYSUM_SCULPTMASK: 
			ray_cast_type -= 17; 
			is_sculpt_mode = true;
		case __RM_RAYMAX:
		case __RM_RAYMIN:
		case __RM_RAYSUM:
			is_xray_mode = true;
			break;
		case __RM_MODULATION:
		case __RM_MULTIOTF_MODULATION:
		case __RM_SCULPTMASK_MODULATION:
			GradientMagnitudeAnalysis(grad_minmax, vobj);
			is_modulation_mode = true;
			if (ray_cast_type == __RM_SCULPTMASK_MODULATION)
				is_sculpt_mode = true;
			break;
		case __RM_SCULPTMASK: is_sculpt_mode = true; break;
		case __RM_DEFAULT:  
		case __RM_MULTIOTF: 
		case __RM_VISVOLMASK:
		case __RM_OPAQUE: 
		case __RM_OPAQUE_MULTIOTF: 
		default: break;
		}

		bool skip_volblk_update = actor->GetParam("_bool_ForceToSkipBlockUpdate", false);
		int blk_level = actor->GetParam("_int_BlockLevel", (int)1);
		blk_level = max(min(1, blk_level), 0);

		bool use_mask_volume = actor->GetParam("_bool_ValidateMaskVolume", false);
		int sculpt_index = actor->GetParam("_int_SculptingIndex", (int)-1);
		if (!is_sculpt_mode)
			sculpt_index = -1;

		bool showOutline = actor->GetParam("_bool_ShowOutline", false);
		vmfloat4 material_phongCoeffs = actor->GetParam("_float4_PhongCoeffs", default_phong_lighting_coeff);
		//int outline_thickness = actor->GetParam("_int_SilhouetteThickness", (int)0);
		//float outline_depthThres = 10000.f;
		//vmfloat3 outline_color = vmfloat3(1.f);
		//if (outline_thickness > 0) {
		//	outline_depthThres = actor->GetParam("_float_SilhouetteDepthThres", outline_depthThres);
		//	outline_color = actor->GetParam("_float3_SilhouetteColor", outline_color);
		//}

		// 0 : only full sampling (no downscale) for both main volume and mask volume
		// 1 : downscaling for 3d view transparency dvr and mask volume
		// 2 : downscaling for 3d view transparency dvr 
		// 3 : downscaling for 3d view dvr and mask volume
		// 4 : downscaling for 3d view dvr 
		// 5 : downscaling for mask volume
		int downScaleOption = actor->GetParam("_int_DownScaleOption", (int)1);
#pragma endregion

#pragma region GPU resource updates
		VmObject* tobj_otf = (VmObject*)actor->GetAssociateRes("OTF"); // essential!
		if (is_xray_mode) {
			VmObject* tobj_windowing = (VmObject*)actor->GetAssociateRes("WINDOWING");
			if (tobj_windowing) 
				tobj_otf = tobj_windowing;
		}
		if (tobj_otf == NULL)
		{
			VMERRORMESSAGE("NOT ASSIGNED OTF");
			continue;
		}

		MapTable* tmap_data = tobj_otf->GetObjParamPtr<MapTable>("_TableMap_OTF");

		VmVObjectVolume* mask_vol_obj = (VmVObjectVolume*)actor->GetAssociateRes("MASKVOLUME");

		GpuRes gres_mask_vol;
		if (mask_vol_obj != NULL)
		{
			if (ray_cast_type == __RM_VISVOLMASK) {

				vobj = mask_vol_obj;
				mask_vol_obj = NULL;
				vol_data = vobj->GetVolumeData();
			}
			else {
				//clock_t __start = clock();

				// down-scaling true when downScaleOption is 1, 3, or 5
				grd_helper::UpdateVolumeModel(gres_mask_vol, mask_vol_obj, true, 
					false // force to set FALSE (for quality issue)
					//downScaleOption == 1 || downScaleOption == 3 || downScaleOption == 5
				);
				SET_SHADER_RES(2, 1, (__SRV_PTR*)&gres_mask_vol.alloc_res_ptrs[DTYPE_SRV]);
			}
		}
		else if (ray_cast_type == __RM_MULTIOTF || ray_cast_type == __RM_VISVOLMASK) {
			ray_cast_type = __RM_DEFAULT;
		}

		VmObject* sculptBitPackedObj = (VmVObjectVolume*)actor->GetAssociateRes("SCULPT_PACKEDBITS");
		GpuRes gres_sculpt_bits;
		if (sculptBitPackedObj)
		{
			vector<uint32_t>* pvtrSculptBitPacked = sculptBitPackedObj->GetObjParamPtr<vector<uint32_t>>("_vlist_UINT_SculptPackedBits");
			if (pvtrSculptBitPacked)
			{
				gres_sculpt_bits.vm_src_id = sculptBitPackedObj->GetObjectID();

				auto checkBitPackedTex3D = [&gpu_manager](GpuRes& gres_sculpt_bits, VmObject* sculptBitPackedObj,
					const uint32_t width, const uint32_t height, const uint32_t depth)
					{
						bool needUpdate = true;
						if (gpu_manager->UpdateGpuResource(gres_sculpt_bits)) {

							uint32_t prevW = gres_sculpt_bits.res_values.GetParam("WIDTH", (uint32_t)0);
							uint32_t prevH = gres_sculpt_bits.res_values.GetParam("HEIGHT", (uint32_t)0);
							uint32_t prevD = gres_sculpt_bits.res_values.GetParam("DEPTH", (uint32_t)0);
							uint32_t prevFmt = gres_sculpt_bits.options["FORMAT"];

							needUpdate = (prevW != width) || (prevH != height) || (prevD != depth) || (prevFmt != (uint32_t)DXGI_FORMAT_R32_UINT);

							uint64_t _tp_cpu = sculptBitPackedObj->GetContentUpdateTime();
							uint64_t _tp_gpu = gres_sculpt_bits.res_values.GetParam("LAST_UPDATE_TIME", (uint64_t)0);

							needUpdate = _tp_cpu > _tp_gpu || needUpdate;
						}

						return needUpdate;
					};


#if USE_SCULPT_BITS_TEX3D_TILED == 1
				// 3D-tiled sculpt bits: each R32_UINT texel holds a 4x4x2 = 32-voxel block.
				// sub-index within the texel: sub = (x&3) | ((y&3)<<2) | ((z&1)<<4).
				// Source is the canonical 1D bit stream (bit_id = x + y*W + z*W*H, LSB-first within uint32).
				VolumeData* vol_data_for_bits = vobj->GetVolumeData();
				const int W = (int)vol_data_for_bits->vol_size.x;
				const int H = (int)vol_data_for_bits->vol_size.y;
				const int D = (int)vol_data_for_bits->vol_size.z;
				const uint32_t tex_w = (uint32_t)((W + 3) / 4);
				const uint32_t tex_h = (uint32_t)((H + 3) / 4);
				const uint32_t tex_d = (uint32_t)((D + 1) / 2);

				gres_sculpt_bits.res_name = "SculptPackedBits_Tex3D_Tiled";
				if (checkBitPackedTex3D(gres_sculpt_bits, sculptBitPackedObj, tex_w, tex_h, tex_d))
				{
					const size_t total_texels = (size_t)tex_w * (size_t)tex_h * (size_t)tex_d;

					static thread_local std::vector<uint32_t> tiled_buf;
					tiled_buf.assign(total_texels, 0u);

					const uint32_t* src_bits = pvtrSculptBitPacked->data();
					const size_t src_word_count = pvtrSculptBitPacked->size();
					for (int z = 0; z < D; ++z) {
						const int tz = z >> 1;
						const int sz = z & 1;
						for (int y = 0; y < H; ++y) {
							const int ty = y >> 2;
							const int sy = y & 3;
							const uint64_t row_bit_base = (uint64_t)y * (uint64_t)W + (uint64_t)z * (uint64_t)W * (uint64_t)H;
							for (int x = 0; x < W; ++x) {
								const uint64_t bit_id = (uint64_t)x + row_bit_base;
								const size_t word_idx = (size_t)(bit_id >> 5);
								if (word_idx >= src_word_count) continue;
								const uint32_t bit = (src_bits[word_idx] >> (bit_id & 31u)) & 1u;
								if (bit) {
									const int tx = x >> 2;
									const int sx = x & 3;
									const int sub = sx | (sy << 2) | (sz << 4);
									tiled_buf[(size_t)tx + (size_t)ty * tex_w + (size_t)tz * tex_w * tex_h] |= (1u << sub);
								}
							}
						}
					}

					grd_helper::UpdateCustomTexture3D(gres_sculpt_bits, sculptBitPackedObj, "SculptPackedBits_Tex3D_Tiled",
						tiled_buf.data(), tex_w, tex_h, tex_d, DXGI_FORMAT_R32_UINT, 4);
				}

#elif USE_SCULPT_BITS_TEX3D == 1
				// Upload packed sculpt bits as a Texture3D<R32_UINT>: 32 voxels along X are packed into one texel,
				// so cache locality follows 3D texture coherency (Y/Z neighbors hit the 3D texture cache).
				//
				// Source layout is a 1D bit stream (bit_id = x + y*W + z*W*H), so when W is not a multiple of 32
				// the source row boundaries do not align to 32-bit word boundaries. We handle both the aligned
				// fast path (zero-copy when sizes match, or simple word copy) and the unaligned path
				// (per-row word shuffle).
				VolumeData* vol_data_for_bits = vobj->GetVolumeData();
				const int W = (int)vol_data_for_bits->vol_size.x;
				const int H = (int)vol_data_for_bits->vol_size.y;
				const int D = (int)vol_data_for_bits->vol_size.z;
				const uint32_t tex_w = (uint32_t)((W + 31) / 32);
				const uint32_t tex_h = (uint32_t)H;
				const uint32_t tex_d = (uint32_t)D;
				gres_sculpt_bits.res_name = "SculptPackedBits_Tex3D";
				if (checkBitPackedTex3D(gres_sculpt_bits, sculptBitPackedObj, tex_w, tex_h, tex_d))
				{
					vzlog("------------> SculptPackedBits_Tex3D");

					const size_t expected_words = (size_t)tex_w * (size_t)tex_h * (size_t)tex_d;
					const uint32_t* src_bits = pvtrSculptBitPacked->data();
					const size_t src_word_count = pvtrSculptBitPacked->size();
					const bool w_aligned = ((W & 31) == 0);
					const bool zero_copy_ok = w_aligned && (src_word_count >= expected_words);

					if (zero_copy_ok) {
						// W is a multiple of 32 AND source has at least the expected words: layouts match bit-for-bit.
						grd_helper::UpdateCustomTexture3D(gres_sculpt_bits, sculptBitPackedObj, "SculptPackedBits_Tex3D",
							src_bits, tex_w, tex_h, tex_d, DXGI_FORMAT_R32_UINT, 4);
					}
					else {
						// Need to repack the 1D bit stream into a row-padded 3D texture layout.
						static thread_local std::vector<uint32_t> xpack_buf;
						xpack_buf.assign(expected_words, 0u);
						const uint32_t last_word_bits = (W & 31) ? (uint32_t)(W & 31) : 32u;
						const uint32_t last_word_mask = (last_word_bits == 32u) ? 0xFFFFFFFFu : ((1u << last_word_bits) - 1u);

						for (int z = 0; z < D; ++z) {
							for (int y = 0; y < H; ++y) {
								const uint64_t row_bit_start = (uint64_t)y * (uint64_t)W + (uint64_t)z * (uint64_t)W * (uint64_t)H;
								const size_t   src_word_start = (size_t)(row_bit_start >> 5);
								const uint32_t src_bit_offset = (uint32_t)(row_bit_start & 31u);
								uint32_t* dst_row = xpack_buf.data() + (size_t)y * tex_w + (size_t)z * tex_w * tex_h;

								if (src_bit_offset == 0) {
									// Source row is word-aligned: simple word copy + mask the tail.
									for (uint32_t w = 0; w < tex_w; ++w) {
										const size_t src_idx = src_word_start + w;
										uint32_t word = (src_idx < src_word_count) ? src_bits[src_idx] : 0u;
										if (w == tex_w - 1u) word &= last_word_mask;
										dst_row[w] = word;
									}
								}
								else {
									// Source row is word-unaligned: shift+combine adjacent words.
									const uint32_t shift = src_bit_offset;
									const uint32_t inv_shift = 32u - shift;
									for (uint32_t w = 0; w < tex_w; ++w) {
										const size_t src_idx = src_word_start + w;
										const uint32_t lo = (src_idx < src_word_count) ? (src_bits[src_idx] >> shift) : 0u;
										const uint32_t hi = (src_idx + 1u < src_word_count) ? (src_bits[src_idx + 1u] << inv_shift) : 0u;
										uint32_t word = lo | hi;
										if (w == tex_w - 1u) word &= last_word_mask;
										dst_row[w] = word;
									}
								}
							}
						}

						grd_helper::UpdateCustomTexture3D(gres_sculpt_bits, sculptBitPackedObj, "SculptPackedBits_Tex3D",
							xpack_buf.data(), tex_w, tex_h, tex_d, DXGI_FORMAT_R32_UINT, 4);
					}
				}
#else
				grd_helper::UpdateCustomBuffer(gres_sculpt_bits, sculptBitPackedObj, "SculptPackedBits", pvtrSculptBitPacked->data(), pvtrSculptBitPacked->size(), DXGI_FORMAT_R32_UINT, 4);
#endif
				SET_SHADER_RES(7, 1, (__SRV_PTR*)&gres_sculpt_bits.alloc_res_ptrs[DTYPE_SRV]);
			}
		}

		// down-scaling true when downScaleOption is 
		// 1 (&&is_modulation_mode)
		// 2 (&&is_modulation_mode) 
		// 3 or 4
		GpuRes gres_vol;
		grd_helper::UpdateVolumeModel(gres_vol, vobj, ray_cast_type == __RM_VISVOLMASK, 
			// force to set FALSE (for quality issue)
			false 
			//&& //planeThickness < 0 && 
			//(
			//	((downScaleOption == 1 || downScaleOption == 2) && is_modulation_mode)
			//	|| downScaleOption == 3 || downScaleOption == 4
			//	)
			, progress); // ray_cast_type == __RM_MAXMASK
		//grd_helper::UpdateVolumeModel(gres_vol, vobj, ray_cast_type == __RM_VISVOLMASK, true, progress); // ray_cast_type == __RM_MAXMASK

		SET_SHADER_RES(0, 1, (__SRV_PTR*)&gres_vol.alloc_res_ptrs[DTYPE_SRV]);

		// test code for kuei
		ID3D11ShaderResourceView* test_srv = (ID3D11ShaderResourceView*)actor->GetParam("TEST_SRV_VOLUME", (void*)nullptr);
		if (test_srv)
		{
			vmlog::LogInfo("test_srv comes in");
			SET_SHADER_RES(0, 1, &test_srv);
		}

		GpuRes gres_tmap_otf, gres_tmap_preintotf;
		grd_helper::UpdateTMapBuffer(gres_tmap_otf, tobj_otf, false);
		grd_helper::UpdateTMapBuffer(gres_tmap_preintotf, tobj_otf, true);
		SET_SHADER_RES(3, 1, (__SRV_PTR*)&gres_tmap_otf.alloc_res_ptrs[DTYPE_SRV]);
		SET_SHADER_RES(13, 1, (__SRV_PTR*)&gres_tmap_preintotf.alloc_res_ptrs[DTYPE_SRV]);

		if (vobj->GetVolumeBlock(blk_level) == NULL)
		{
			vobj->UpdateVolumeMinMaxBlocks();
		}

		GpuRes gres_volblk_otf, gres_volblk_min, gres_volblk_max;
		GpuRes& gres_volblk = gres_volblk_otf;
		__SRV_PTR volblk_srv = NULL;
		if (is_xray_mode) {
			grd_helper::UpdateMinMaxBlocks(gres_volblk_min, gres_volblk_max, vobj);
			if (ray_cast_type == __RM_RAYMAX) {	// Min 
				volblk_srv = (__SRV_PTR)gres_volblk_max.alloc_res_ptrs[DTYPE_SRV];
				gres_volblk = gres_volblk_max;
			}
			else if (ray_cast_type == __RM_RAYMIN) {
				volblk_srv = (__SRV_PTR)gres_volblk_min.alloc_res_ptrs[DTYPE_SRV];
				gres_volblk = gres_volblk_min;
			}
		}
		else {
			grd_helper::UpdateOtfBlocks(gres_volblk_otf, vobj, mask_vol_obj, tobj_otf, sculpt_index); // this tagged mask volume is always used even when MIP mode
			volblk_srv = (__SRV_PTR)gres_volblk_otf.alloc_res_ptrs[DTYPE_SRV];
		}
		SET_SHADER_RES(1, 1, (__SRV_PTR*)&volblk_srv);

		CB_VolumeObject cbVolumeObj;
		//vmint3 vol_sampled_size = vmint3(gres_vol.res_values.GetParam("WIDTH", (uint32_t)0),
		//	gres_vol.res_values.GetParam("HEIGHT", (uint32_t)0),
		//	gres_vol.res_values.GetParam("DEPTH", (uint32_t)0));
		//if ( && samplePrecisionLevel > 0)
		//high_samplerate ? 2.f : 1.f
		grd_helper::SetCb_VolumeObj(cbVolumeObj, vobj, actor->matWS2OS, gres_vol, tmap_data->valid_min_idx.x, gres_volblk.options["FORMAT"] == DXGI_FORMAT_R16_UNORM ? 65535.f : 255.f, 
			is_modulation_mode ? -samplePrecisionLevel : samplePrecisionLevel, is_xray_mode, sculpt_index);
		if (is_modulation_mode && ((uint32_t)vol_data->vol_size.x * (uint32_t)vol_data->vol_size.y * (uint32_t)vol_data->vol_size.z > 1000000)) {
			//cbVolumeObj.opacity_correction *= 2.f;
			//cbVolumeObj.sample_dist *= 2.f;
			//cbVolumeObj.vec_grad_x *= 2.f;
			//cbVolumeObj.vec_grad_y *= 2.f;
			//cbVolumeObj.vec_grad_z *= 2.f;
		}

		// TEST
		//{
		//float early_ray_termination = _fncontainer->fnParams.GetParam("_float_EarlyRayTermination", 1.0f);
		//cbVolumeObj.v_dummy0 = *(uint32_t*)&early_ray_termination;
		//}


		cbVolumeObj.pb_shading_factor = material_phongCoeffs;
		cbVolumeObj.vobj_flag |= (int)showOutline << 1;
		if (is_ghost_mode) {
			bool is_ghost_surface = actor->GetParam("_bool_IsGhostSurface", false);
			bool is_only_hotspot_visible = actor->GetParam("_bool_IsOnlyHotSpotVisible", false);
			cbVolumeObj.vobj_flag |= (int)is_ghost_surface << 19;
			cbVolumeObj.vobj_flag |= (int)is_only_hotspot_visible << 20;
			//cout << "TEST : " << is_ghost_surface << ", " << is_only_hotspot_visible << endl;
		}
		else if (is_modulation_mode) {
			cbVolumeObj.grad_max = grad_minmax.y;
			//cbVolumeObj.grad_scale = actor->GetParam("_float_ModulationGradScale", 1.f);
			//cbVolumeObj.kappa_i = actor->GetParam("_float_ModulationKappai", 1.f);
			//cbVolumeObj.kappa_s = actor->GetParam("_float_ModulationKappas", 1.f);
			cbVolumeObj.grad_scale = actor->GetParam("_float_ModulationGradScale", 0.5f);
			cbVolumeObj.kappa_i = actor->GetParam("_float_ModulationKappai", 0.f);
			cbVolumeObj.kappa_s = actor->GetParam("_float_ModulationKappas", 0.f);
		}
		
		// Tag ONLY the last DVR volume so DvrCS writes its volume-only x-ray color (bit 2) and skips the
		// in-DVR mesh composite, leaving it to the fused pass. Earlier volumes accumulate into the RT
		// normally, so is_last_dvr is required here (apply_postprocessing_filter alone is count-independent).
		if (is_last_dvr && apply_postprocessing_filter)
		{
			cbVolumeObj.vobj_flag |= (int)1 << 2;
		}

		if (mask_vol_obj) {
			cbVolumeObj.mask_vol_size = vmfloat3(gres_mask_vol.res_values.GetParam("WIDTH", (uint32_t)1),
				gres_mask_vol.res_values.GetParam("HEIGHT", (uint32_t)1),
				gres_mask_vol.res_values.GetParam("DEPTH", (uint32_t)1));
			VolumeData* mask_vol_data = mask_vol_obj->GetVolumeData();
			if (mask_vol_data->store_dtype.type_bytes == data_type::dtype<uint8_t>().type_bytes) // char
				cbVolumeObj.mask_value_range = 255.f;
			else if (mask_vol_data->store_dtype.type_bytes == data_type::dtype<uint16_t>().type_bytes) // short
				cbVolumeObj.mask_value_range = 65535.f;
			else VMERRORMESSAGE("UNSUPPORTED FORMAT : MASK VOLUME");
		}
		D3D11_MAPPED_SUBRESOURCE mappedResVolObj;
		dx11DeviceImmContext->Map(cbuf_vobj, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResVolObj);
		CB_VolumeObject* cbVolumeObjData = (CB_VolumeObject*)mappedResVolObj.pData;
		memcpy(cbVolumeObjData, &cbVolumeObj, sizeof(CB_VolumeObject));
		dx11DeviceImmContext->Unmap(cbuf_vobj, 0);
		SET_CBUFFERS(4, 1, &cbuf_vobj);

		CB_ClipInfo cbClipInfo;
		grd_helper::SetCb_ClipInfo(cbClipInfo, vobj, actor, camClipMode, camClipperFreeActors, camClipMatWS2BS, camClipPlanePos, camClipPlaneDir);
		D3D11_MAPPED_SUBRESOURCE mappedResClipInfo;
		dx11DeviceImmContext->Map(cbuf_clip, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResClipInfo);
		CB_ClipInfo* cbClipInfoData = (CB_ClipInfo*)mappedResClipInfo.pData;
		memcpy(cbClipInfoData, &cbClipInfo, sizeof(CB_ClipInfo));
		dx11DeviceImmContext->Unmap(cbuf_clip, 0);
		SET_CBUFFERS(2, 1, &cbuf_clip);

		CB_TMAP cbTmap;
		grd_helper::SetCb_TMap(cbTmap, tobj_otf);
		D3D11_MAPPED_SUBRESOURCE mappedResOtf;
		dx11DeviceImmContext->Map(cbuf_tmap, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResOtf);
		CB_TMAP* cbTmapData = (CB_TMAP*)mappedResOtf.pData;
		memcpy(cbTmapData, &cbTmap, sizeof(CB_TMAP));
		dx11DeviceImmContext->Unmap(cbuf_tmap, 0);
		SET_CBUFFERS(5, 1, &cbuf_tmap);

		CB_Material cbRndEffect;
		grd_helper::SetCb_RenderingEffect(cbRndEffect, actor);
		D3D11_MAPPED_SUBRESOURCE mappedResRdnEffect;
		dx11DeviceImmContext->Map(cbuf_reffect, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResRdnEffect);
		CB_Material* cbRndEffectData = (CB_Material*)mappedResRdnEffect.pData;
		memcpy(cbRndEffectData, &cbRndEffect, sizeof(CB_Material));
		dx11DeviceImmContext->Unmap(cbuf_reffect, 0);
		SET_CBUFFERS(3, 1, &cbuf_reffect);

		CB_VolumeMaterial cbVrMaterial;
		grd_helper::SetCb_VolumeRenderingEffect(cbVrMaterial, vobj, actor);
		D3D11_MAPPED_SUBRESOURCE mappedVrEffect;
		dx11DeviceImmContext->Map(cbuf_vreffect, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedVrEffect);
		CB_VolumeMaterial* cbVrEffectData = (CB_VolumeMaterial*)mappedVrEffect.pData;
		memcpy(cbVrEffectData, &cbVrMaterial, sizeof(CB_VolumeMaterial));
		dx11DeviceImmContext->Unmap(cbuf_vreffect, 0);
		SET_CBUFFERS(6, 1, &cbuf_vreffect);
#pragma endregion

#ifdef DX10_0
#else
		// ----- VXGI v5: build material + radiance grids, propagate bounces, bind for the DVR in-scatter -----
		// Runs on the last DVR volume (non-x-ray) BEFORE its RayCasting dispatch (RayCasting reads the radiance
		// grid at SRV t8 with cone-trace LOD, gated by g_cbVxgi.vxgi_flag). Five resources:
		//   VXGI_GRID_MAT    (single mip) : albedo+opacity from Voxelize — static until the CONTENT changes.
		//   VXGI_VOXEL_GRID  (MIP CHAIN)  : radiance+opacity — what shading cone-traces; mips via GenerateMips.
		//   VXGI_GRID_PING   (single mip) : propagation scratch (read prev radiance -> write next, then copy back).
		//   VXGI_GRID_DIRECT (single mip) : stable diffusion source from InjectLight — SRV forever after inject
		//                                   (no UAV/SRV rebinding, no inject-copy texture; plan §3.4).
		//   VXGI_GRID_SURF   (single mip) : Part C — surface cone indirect (rgb) + cone AO (a), rewritten
		//                                   WHOLESALE by SurfaceGather at checkpoint bounces, composited as a
		//                                   source term by every Propagate (only mip-0 Loads -> no mip chain).
		// Rebuild is gated on a CONTENT stamp (volume + OTF content times + resolved light state) — NOT the scene
		// stamp, so camera moves no longer re-voxelize (the grid is view-independent). While the content is
		// static, ONE VXGI_Propagate iteration runs per frame (up to a target bounce count): the radiance field
		// visibly refines frame over frame, driven by the same convergence re-render loop TAA uses.
		const bool vxgi_active = vxgi_on && is_last_dvr && !is_xray_mode;
		if (vxgi_active)
		{
			const uint32_t vxgi_R = (uint32_t)(vxgi_resolution > 0 ? vxgi_resolution : 128);
			GpuRes gres_vxgi_mat, gres_vxgi_ping, gres_vxgi_direct, gres_vxgi_surf;
			grd_helper::UpdateVoxelGrid(gres_vxgi_mat, iobj, "VXGI_GRID_MAT", vxgi_R, DXGI_FORMAT_R16G16B16A16_FLOAT, true); // mips: inject's light march LODs through it
			grd_helper::UpdateVoxelGrid(gres_vxgi, iobj, "VXGI_VOXEL_GRID", vxgi_R, DXGI_FORMAT_R16G16B16A16_FLOAT, true); // mip chain
			grd_helper::UpdateVoxelGrid(gres_vxgi_ping, iobj, "VXGI_GRID_PING", vxgi_R, DXGI_FORMAT_R16G16B16A16_FLOAT);
			grd_helper::UpdateVoxelGrid(gres_vxgi_direct, iobj, "VXGI_GRID_DIRECT", vxgi_R, DXGI_FORMAT_R16G16B16A16_FLOAT); // stable diffusion source
			grd_helper::UpdateVoxelGrid(gres_vxgi_surf, iobj, "VXGI_GRID_SURF", vxgi_R, DXGI_FORMAT_R16G16B16A16_FLOAT); // Part C surface cone term
			vxgi_mat_srv_dbg = (ID3D11ShaderResourceView*)gres_vxgi_mat.alloc_res_ptrs[DTYPE_SRV];
			vxgi_surf_srv_dbg = (ID3D11ShaderResourceView*)gres_vxgi_surf.alloc_res_ptrs[DTYPE_SRV];
			vxgi_ready = true;

			// CB_VXGI (b13): grid aligned with the volume, so world->voxel[0,1] == the volume's world->texture
			// matrix. SetCb_VolumeObj stored mat_ws2ts TRANSPOSED; recover the raw matrix (SetCb_VXGI re-transposes).
			ID3D11Buffer* cbuf_vxgi = psoManager->get_cbuf("CB_VXGI");
			CB_VXGI cbVxgi;
			// debug byte for vxgi_flag[24:31]: mode (4b, from VXGI_DEBUG low byte) | mip (4b, from its high bits)
			const uint32_t vxgi_debug_byte = ((uint32_t)vxgi_debug & 0xFu) | ((((uint32_t)vxgi_debug >> 8) & 0xFu) << 4);
			// Medium-visibility gates: what of the DVR's own visibility the grid reproduces. The Voxelize
			// shader reads the SAME bindings the DVR march uses (t2 mask, t7 sculpt bits, b2 clip — all bound
			// above) and tests them PER SUB-SAMPLE, gated by these flag bits.
			//
			// CLIP and SCULPT are OPTIONAL, and DEFAULT OFF — a clip box or a sculpt is treated as a VIEWING
			// CUTAWAY, not as a physical cut. The DVR still cuts the picture, but the grid keeps the whole
			// volume, so the lighting stays that of the INTACT anatomy: you look inside a body that is still
			// lit as a body. The cut interior is then lit as if the removed material were still there (a real
			// consequence — it reads darker than a physically-cut face would).
			//
			// Turning a gate ON gives the physical reading instead: the material is removed from the grid, so
			// it no longer shadows what it no longer covers, and the exposed cut face becomes a genuine
			// surface (Voxelize's per-sub-sample test leaves it a ~2-voxel coverage ramp, so the surface
			// classifier picks it up and it gets cone AO + surface GI).
			//
			// The default is OFF because ON is expensive in a way that is easy to under-estimate: the gate's
			// state is exactly what puts clip/sculpt into the VXGI CONTENT STAMP (below). With a gate ON,
			// every DRAG FRAME changes that state, which changes the MAT stamp, which re-runs Voxelize +
			// BlurMat + GenerateMips + InjectLight — a rebuild-class spike per frame — while the crossfade
			// path pins the bounce counter at 1, so the GI never converges for as long as you are dragging.
			// With the gate OFF none of it is stamped: a clip drag costs the VXGI pipeline nothing and the
			// diffusion keeps converging right through it.
			//
			// The multi-OTF gate is NOT optional: it selects WHICH OTF row defines the material, i.e. it is
			// part of the material definition, not a removal.
			const bool vxgi_clip_medium = _fncontainer->fnParams.GetParam("_bool_VxgiClipMedium", false);
			const bool vxgi_sculpt_medium = _fncontainer->fnParams.GetParam("_bool_VxgiSculptMedium", false);
			uint32_t vxgi_medium_flags = 0;
			const bool vxgi_mask_bound = mask_vol_obj != NULL; // t2 bound above whenever present
			if (vxgi_mask_bound && (ray_cast_type == __RM_MULTIOTF || ray_cast_type == __RM_MULTIOTF_MODULATION || ray_cast_type == __RM_OPAQUE_MULTIOTF))
				vxgi_medium_flags |= 0x1; // per-mask OTF row
			if (vxgi_sculpt_medium && is_sculpt_mode && sculptBitPackedObj == NULL && vxgi_mask_bound)
				vxgi_medium_flags |= 0x2; // sculpt mask (mask value vs sculpt_value)
			if (vxgi_sculpt_medium && is_sculpt_mode && sculptBitPackedObj != NULL)
				vxgi_medium_flags |= 0x4; // packed sculpt bits (t7)
			// Set only when the clip is BOTH active and wanted, so the flag alone answers "is clip baked?" —
			// the shader needs no second test and the stamp below keys off the same bit.
			if (vxgi_clip_medium && cbClipInfo.clip_flag != 0)
				vxgi_medium_flags |= 0x10; // clip box / plane (b2)
			// CONTEXT-AWARE (VR_MODE 2) medium — OPT-IN, DEFAULT OFF. Bakes MODULATE's gradient-length term
			// into the coverage so the grid's medium matches the modulated picture.
			//
			// OFF is the DEFAULT because it is the more correct model, not merely the better-looking one:
			//
			//  * Modulation is NOT the same kind of thing as clip / sculpt / OTF-mask. Those REMOVE material
			//    — it is not there, so it must not occlude or scatter (that is the parity rule the other
			//    three flags enforce). Context modulation makes material SEE-THROUGH: the skin is still
			//    there, you are just looking past it at the bone. Context-preserving DVR exists precisely to
			//    keep that material as context — so it should still block and scatter light. Baking the
			//    modulation into the medium deletes from the light transport the very thing the mode is
			//    named after.
			//
			//  * The modulator would be applied TWICE. On screen the in-scatter is added to vis_sample and
			//    then MODULATE scales the whole sample (DvrCS.hlsl), so the GI term already carries the
			//    modulator exactly once. Baking it into the grid dims the radiance field as well, and the
			//    two compound — which is the washed-out look this flag produces in practice. With the flag
			//    OFF the grid transports light through the REAL material and the modulator lands exactly
			//    once, at display, on emission and in-scatter alike.
			//
			// Kept as an option for A/B and for anyone who does want the GI to follow the modulated look.
			// Expect to raise the in-scatter intensity in VR_MODE 2 either way: the display is globally more
			// transparent, so the GI reads fainter and the SOURCE is what compensates (a plain exposure
			// adjustment — unlike the compounding above, it is not a structural error).
			const bool vxgi_context_medium = _fncontainer->fnParams.GetParam("_bool_VxgiContextMedium", false);
			const bool vxgi_context_on = is_modulation_mode && vxgi_context_medium;
			if (vxgi_context_on)
				vxgi_medium_flags |= 0x8; // all three modulation ray-cast types (is_modulation_mode), not just the plain one
			// Coverage boost for the above (dev channel). The gradient shell the modulation leaves is thin
			// against a grid voxel (grid ~1/4 the volume res), so the baked coverage can collapse to ~0.1-0.2
			// and take the GI/AO with it. 1.0 = plain MODULATE parity. > 1 is legal (Voxelize saturates).
			const float vxgi_context_gain = _fncontainer->fnParams.GetParam("_float_VxgiContextAlphaGain", 1.f);
			// AO remap tuning knobs (dev channel: vzm::SetRenderTestParam -> fnParams, no public API).
			// The cubic B-spline density taps spread thin-shell density more than the old trilinear ones,
			// so pivot/slope may need per-dataset retuning; folded into the content stamp below so a
			// change re-bakes the field immediately.
			const float vxgi_ao_pivot = _fncontainer->fnParams.GetParam("_float_VxgiAoPivot", 0.3f);
			const float vxgi_ao_slope = _fncontainer->fnParams.GetParam("_float_VxgiAoSlope", 1.5f);
			// MAT blur (gaussian on the voxelized coverage/albedo, BEFORE mip-gen): the single-source
			// fix for the stored mip-lattice phase banding the surface cones integrate (see BlurMat.hlsl).
			// It SUBSUMED the retired post-bake obscurance blur (_bool_VxgiAoBlur / BlurObscuranceX/Y/Z):
			// smoothing the source before the cubic density taps removes the same residual band energy
			// that pass cleaned after them, and A/B showed no remaining visible contribution.
			const bool vxgi_mat_blur = _fncontainer->fnParams.GetParam("_bool_VxgiMatBlur", true);
			// Diffusion gain: deeper light creep at higher values; consumption/debug scale by (1-gain)
			// so brightness self-normalizes. Folded into the content stamp (light side) so a change
			// re-runs the diffusion to the new fixed point. COUPLED with _int_VxgiBounceTarget
			// (convergence error ~ gain^n). The bounce budget is DERIVED from this by formula below
			// (not a separate knob); keep this default in sync with EnableVoxelGI's (VisMtvApi.h).
			const float vxgi_scatter_gain = _fncontainer->fnParams.GetParam("_float_VxgiScatterGain", 0.75f);
			// Part C (surface cone indirect + cone AO, VXGI v5) knobs — dev channel (SetRenderTestParam),
			// to be promoted to EnableVoxelGI trailing defaults once tuning stabilizes (plan §4.5).
			//   _int_VxgiSurfaceCheckpoints (N, def 2): SurfaceGather refinement count; the checkpoint
			//     bounce set is derived from N and the bounce target T: {0}, {0,T/2}, {0,T/2,T-1}.
			//     N=0 disables Part C entirely (D-only — useful for A/B comparison).
			//   _float_VxgiSurfaceGiGain (def 0.15): surface indirect strength (CPU clamp [0,0.95], §4.4).
			//     NOTE the 0.95 clamp is the STABILITY ceiling, not a display default: the cone gather
			//     returns O(direct)-scale radiance and the grazing side cones self-collect the curved
			//     shell nearly everywhere (the v1 AO history's ~0.3-0.5 flat-surface base, same geometry),
			//     so raw gains beyond ~0.2 saturate the additive DVR in-scatter (measured; plan §4.5).
			//   _float_VxgiSurfaceConeAoGain (def 1.0): cone AO screen-blend strength (0 = density AO only).
			const int vxgi_surface_checkpoints = max(0, min(3, _fncontainer->fnParams.GetParam("_int_VxgiSurfaceCheckpoints", (int)2)));
			float vxgi_surf_gi_gain = _fncontainer->fnParams.GetParam("_float_VxgiSurfaceGiGain", 0.15f);
			float vxgi_surf_ao_gain = _fncontainer->fnParams.GetParam("_float_VxgiSurfaceConeAoGain", 1.0f);
			if (vxgi_surface_checkpoints == 0)
			{
				// Part C OFF: force both gains to 0 so whatever grid_surf holds (stale or the first-build
				// clear's zeros) cancels in the Propagate composite (gain * surf) — no clear dispatch needed.
				vxgi_surf_gi_gain = 0.f;
				vxgi_surf_ao_gain = 0.f;
			}
			grd_helper::SetCb_VXGI(cbVxgi, TRANSPOSE(cbVolumeObj.mat_ws2ts), vxgi_R, vxgi_gi_intensity, vxgi_ao_intensity, true, 1.f /* indirect retired */, vxgi_debug_byte, vxgi_medium_flags, vxgi_ao_pivot, vxgi_ao_slope, vxgi_scatter_gain, vxgi_surf_gi_gain, vxgi_surf_ao_gain, vxgi_context_gain);
			// NOTE the CB is mapped BELOW, after the stamp split decides the light-only preserve-AO flag
			// (vxgi_flag bit6) — it must be part of the uploaded flags for the InjectLight dispatch.

			// CONTENT stamp: volume voxels + OTF (transfer function) + the RESOLVED light state (post headlight
			// resolution, so a camera-locked light correctly retriggers, while pure camera moves do not).
			auto f2u64 = [](float f) { return (uint64_t)(*(uint32_t*)&f); };
			uint64_t vxgi_light_hash = ((uint64_t)cbEnvState.env_flag << 32);
			const float* lposf = (const float*)&cbEnvState.pos_light_ws;
			const float* ldirf = (const float*)&cbEnvState.dir_light_ws;
			for (int lh = 0; lh < 3; lh++)
				vxgi_light_hash ^= (f2u64(lposf[lh]) << (lh * 8 + 4)) ^ (f2u64(ldirf[lh]) << (lh * 8));
			// _int_VxgiRestart: app-driven counter (debug UI) folded into the stamp so a button press restarts
			// the diffusion from bounce 0 without touching the actual content.
			// An HLSL hot-reload rides the SAME channel (high bits, so it cannot collide with the app's 32-bit
			// counter): a reloaded Gather / Propagate / SurfaceGather changes the diffusion's FIXED POINT, and
			// re-converging from the old field would just creep toward it — the restart hard-seeds the grid back
			// to DIRECT so the new shader's field is what you actually watch converge. Persisted through
			// _uint64_VxgiRestartApplied, so it fires exactly ONCE per reload.
			const uint64_t vxgi_restart = (uint64_t)_fncontainer->fnParams.GetParam("_int_VxgiRestart", (int)0)
				^ (vxgi_hlsl_reload_gen << 32);
			// MEDIUM state: whatever the grid actually BAKES is part of its content and must re-voxelize when
			// it changes. The converse is what makes the gates worth having: a state the grid does NOT bake
			// must NOT be stamped, or it would trigger rebuilds that change nothing.
			//
			// So every term below is keyed on the medium flag that put it into the bake, not on the DVR mode
			// that made it visible. Concretely: with _bool_VxgiClipMedium off, dragging the clip box changes
			// cbClipInfo every frame but touches no stamp — no Voxelize, no InjectLight, and the bounce
			// counter is free to keep converging instead of being pinned at 1. That is the whole point.
			//
			// vxgi_medium_flags itself is stamped (<< 56), so toggling any gate re-bakes exactly once.
			uint64_t vxgi_medium_stamp = (uint64_t)vxgi_medium_flags << 56;
			const bool vxgi_stamp_otf_mask = (vxgi_medium_flags & 0x1) != 0;
			const bool vxgi_stamp_sculpt_mask = (vxgi_medium_flags & 0x2) != 0;
			const bool vxgi_stamp_sculpt_bits = (vxgi_medium_flags & 0x4) != 0;
			const bool vxgi_stamp_clip = (vxgi_medium_flags & 0x10) != 0;
			// The mask volume feeds TWO gates (multi-OTF row selection and mask-value sculpt), so it is
			// content whenever EITHER reaches the bake — not whenever the object merely exists.
			if (mask_vol_obj && (vxgi_stamp_otf_mask || vxgi_stamp_sculpt_mask))
				vxgi_medium_stamp ^= mask_vol_obj->GetContentUpdateTime() << 2;
			if (sculptBitPackedObj && vxgi_stamp_sculpt_bits)
				vxgi_medium_stamp ^= sculptBitPackedObj->GetContentUpdateTime() << 3;
			// sculpt_index is the mask-value threshold: only the two sculpt gates read it.
			if (vxgi_stamp_sculpt_mask || vxgi_stamp_sculpt_bits)
				vxgi_medium_stamp ^= (uint64_t)(uint32_t)sculpt_index << 16;
			vxgi_medium_stamp ^= (f2u64(vxgi_ao_pivot) << 20) ^ (f2u64(vxgi_ao_slope) << 28); // AO remap knobs re-bake
			vxgi_medium_stamp ^= vxgi_mat_blur ? (1ull << 58) : 0ull; // MAT blur re-voxel-bakes too
			// CONTEXT (VR_MODE 2, opt-in): when the context medium is ON its inputs are baked into the
			// coverage, so they are CONTENT — without this, dragging the grad-scale or gain slider would
			// change the picture but not the grid. Keyed on vxgi_context_on, NOT on is_modulation_mode:
			// with the flag off these values never reach the bake, so stamping them would trigger
			// meaningless rebuilds on every modulation-knob tweak. (Toggling the flag itself re-bakes
			// already — vxgi_medium_flags is stamped above.) kappa_i/kappa_s are deliberately absent even
			// when on: they are the view-dependent factors, which are never baked.
			if (vxgi_context_on)
				vxgi_medium_stamp ^= f2u64(grad_minmax.y) ^ (f2u64(cbVolumeObj.grad_scale) << 8)
					^ (f2u64(vxgi_context_gain) << 12);
			if (vxgi_stamp_clip)
			{	// FNV-1a over the clip CB (flag/plane/box matrix). Keyed on the medium flag, which is only set
				// when the clip is active AND baked — so an inactive clip, or an active one with the gate off,
				// contributes nothing. Either way toggling the state changes the stamp exactly once (the flag
				// bits are stamped above), and with the gate off a clip DRAG costs zero VXGI work.
				uint64_t clip_hash = 0xcbf29ce484222325ull;
				const uint32_t* clip_w = (const uint32_t*)&cbClipInfo;
				for (size_t cw = 0; cw < sizeof(CB_ClipInfo) / 4; cw++)
					clip_hash = (clip_hash ^ clip_w[cw]) * 0x100000001b3ull;
				vxgi_medium_stamp ^= clip_hash;
			}
			{	// actor/volume TRANSFORM: mat_ws2vox drives the light-direction transform (InjectLight),
				// the clip test (mat_vox2ws) and the world metric (grid_axis_ws) — the volume rotating or
				// moving under a fixed light / clip box changes the field while volume/OTF/light state is
				// untouched, so the matrix itself must stamp the content.
				uint64_t xform_hash = 0xcbf29ce484222325ull;
				const uint32_t* mw = (const uint32_t*)&cbVxgi.mat_ws2vox;
				for (size_t cw = 0; cw < sizeof(cbVxgi.mat_ws2vox) / 4; cw++)
					xform_hash = (xform_hash ^ mw[cw]) * 0x100000001b3ull;
				vxgi_medium_stamp ^= xform_hash;
			}
			// ---- SPLIT STAMPS: material/AO state vs light state. The baked obscurance (cubic taps +
			// blur) depends ONLY on the MAT side, so a light-only change (dragging the light) must not
			// re-run Voxelize or the blur — it re-runs InjectLight alone, in alpha-preserving mode.
			// An HLSL hot-reload folds in HERE, on the MAT side, not just into the light-side content stamp:
			// Voxelize and BlurMat are MAT-side bakes, so only a mat_stamp move re-runs them (a content-only
			// move takes the LIGHT-ONLY inject path and leaves the MAT grid — the old shader's output — intact).
			// Landing it here makes both vxgi_mat_changed AND vxgi_rebuild true (content_stamp XORs mat_stamp in
			// below), which is the full chain: Voxelize -> BlurMat -> mips -> InjectLight -> grid advance.
			// (The reload bits in vxgi_restart shift out of range in content_stamp's `<< 40` — irrelevant, the
			//  signal reaches content_stamp through mat_stamp; vxgi_restart carries it for the re-seed compare.)
			const uint64_t vxgi_mat_stamp = vobj->GetContentUpdateTime()
				^ (tobj_otf->GetContentUpdateTime() << 1) ^ ((uint64_t)vxgi_R << 48) ^ vxgi_medium_stamp
				^ (vxgi_hlsl_reload_gen << 24);
			const uint64_t vxgi_content_stamp = vxgi_mat_stamp ^ vxgi_light_hash ^ (vxgi_restart << 40)
				^ (f2u64(vxgi_scatter_gain) << 6) // gain change re-converges the diffusion (light-side)
				// Part C knobs fold in too (stale-prevention, plan §4.5): once converged, propagate skips —
				// a CB-only change would never reach the field. Stamping them restarts the convergence loop.
				^ (f2u64(vxgi_surf_gi_gain) << 10) ^ (f2u64(vxgi_surf_ao_gain) << 14)
				^ ((uint64_t)(uint32_t)vxgi_surface_checkpoints << 44);

			// Diffusion iteration budget — DERIVED from the scatter gain, not an independent knob.
			// The diffusion converges with error ~ gain^n, so the bounces needed for a fixed residual
			// are fully determined by the gain: target = ceil(ln(residual)/ln(gain)). The residual is a
			// fixed internal constant (1e-2 = ~1%: 0.5 -> 7, 0.75 -> 16, 0.9 -> 44). This is why
			// bounce_target is NOT a public API arg (it must track the gain by formula; exposing it
			// independently invited under-converged images when only the gain was raised). Uses the
			// SAME [0.05,0.95] clamp the shader applies to the gain so the count matches the field.
			// DEBUG override: _int_VxgiBounceTarget (SetRenderTestParam) > 0 forces a manual count for
			// convergence experiments; 0 (the default) means "use the derived value".
			const float VXGI_CONV_RESIDUAL = 1e-2f; // fixed convergence tolerance (~1%; the "2" of 10^-2)
			const float vxgi_gain_clamped = max(0.05f, min(0.95f, vxgi_scatter_gain));
			const int vxgi_bounce_derived = max(1, min(64, (int)ceil(log(VXGI_CONV_RESIDUAL) / log(vxgi_gain_clamped))));
			const int vxgi_bounce_test = _fncontainer->fnParams.GetParam("_int_VxgiBounceTarget", (int)0);
			const int VXGI_BOUNCE_TARGET = (vxgi_bounce_test > 0) ? max(1, min(64, vxgi_bounce_test)) : vxgi_bounce_derived;
			int vxgi_bounce = iobj->GetObjParam<int>("_int_VxgiBounce", (int)0);
			const uint32_t vxgi_groups = (uint32_t)ceil(vxgi_R / 8.f);
			ID3D11ShaderResourceView* vxgi_grid_srv = (ID3D11ShaderResourceView*)gres_vxgi.alloc_res_ptrs[DTYPE_SRV];
			ID3D11ShaderResourceView* vxgi_mat_srv = (ID3D11ShaderResourceView*)gres_vxgi_mat.alloc_res_ptrs[DTYPE_SRV];
			ID3D11ShaderResourceView* vxgi_direct_srv = (ID3D11ShaderResourceView*)gres_vxgi_direct.alloc_res_ptrs[DTYPE_SRV];
			ID3D11ShaderResourceView* vxgi_surf_srv = (ID3D11ShaderResourceView*)gres_vxgi_surf.alloc_res_ptrs[DTYPE_SRV];

			// ONE damped diffusion iteration: prev radiance (t8, mips) + material (t9) + DIRECT source (t10)
			// + Part C surface term (t11, mip-0 Loads) -> PING (u0); copy back to grid mip 0; refresh mips.
			// Shared by BOTH branches below — the static branch for progressive refinement, and the rebuild
			// branch so continuous edits advance the visible field every frame instead of freezing it (see
			// the P0 note there). t11 is InjectLight's slot for the prev-DIRECT alpha, but the two dispatches
			// never overlap and both rebind/unbind locally — no conflict.
			auto vxgi_propagate_once = [&]()
			{
				ID3D11UnorderedAccessView* vxgi_uav_ping = (ID3D11UnorderedAccessView*)gres_vxgi_ping.alloc_res_ptrs[DTYPE_UAV];
				SET_SHADER_RES(8, 1, &vxgi_grid_srv);
				SET_SHADER_RES(9, 1, &vxgi_mat_srv);
				SET_SHADER_RES(10, 1, &vxgi_direct_srv);
				SET_SHADER_RES(11, 1, &vxgi_surf_srv);
				dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, &vxgi_uav_ping, (UINT*)(&vxgi_uav_ping));
				SET_SHADER(GETCS(VXGI_Propagate_cs_5_0), NULL, 0);
				dx11DeviceImmContext->Dispatch(vxgi_groups, vxgi_groups, vxgi_groups);
				dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
				SET_SHADER_RES(8, 1, dx11SRVs_NULL);
				SET_SHADER_RES(9, 1, dx11SRVs_NULL);
				SET_SHADER_RES(10, 1, dx11SRVs_NULL);
				SET_SHADER_RES(11, 1, dx11SRVs_NULL);

				dx11DeviceImmContext->CopySubresourceRegion(
					(ID3D11Resource*)gres_vxgi.alloc_res_ptrs[DTYPE_RES], 0, 0, 0, 0,
					(ID3D11Resource*)gres_vxgi_ping.alloc_res_ptrs[DTYPE_RES], 0, NULL);
				dx11DeviceImmContext->GenerateMips(vxgi_grid_srv);
			};

			// Part C CHECKPOINT (plan §4.2): re-trace all 6 cones per SURFACE voxel against the CURRENT
			// radiance field (t8 — mips must be fresh: after the seed copy's GenerateMips or any
			// vxgi_propagate_once) and rewrite VXGI_GRID_SURF wholesale (non-surface voxels = 0, so
			// stale values are structurally impossible). The early checkpoint sees a barely-diffused
			// field; later checkpoints REFINE against the (half-)converged one — the reason checkpoints
			// exist instead of trace-once-and-freeze. Cost spikes on checkpoint frames only
			// (surface-band voxels x 6 cones x 64 steps x 2 fetches — a rebuild-frame-class spike).
			auto vxgi_surface_gather = [&]()
			{
				ID3D11UnorderedAccessView* vxgi_uav_surf = (ID3D11UnorderedAccessView*)gres_vxgi_surf.alloc_res_ptrs[DTYPE_UAV];
				// (The shader's t0 volume slot serves the RETIRED CT-normal path — VXGI_SURF_CT_NORMAL 0
				// — so no explicit volume bind here; re-add a gres_vol SRV bind at t0 if that path is
				// ever revived.)
				SET_SHADER_RES(8, 1, &vxgi_grid_srv);
				SET_SHADER_RES(9, 1, &vxgi_mat_srv);
				dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, &vxgi_uav_surf, (UINT*)(&vxgi_uav_surf));
				SET_SHADER(GETCS(VXGI_SurfaceGather_cs_5_0), NULL, 0);
				dx11DeviceImmContext->Dispatch(vxgi_groups, vxgi_groups, vxgi_groups);
				dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
				SET_SHADER_RES(8, 1, dx11SRVs_NULL);
				SET_SHADER_RES(9, 1, dx11SRVs_NULL);
			};

			const uint64_t vxgi_prev_stamp = iobj->GetObjParam<uint64_t>("_uint64_VxgiStamp", (uint64_t)~0ull);
			const uint64_t vxgi_prev_mat_stamp = iobj->GetObjParam<uint64_t>("_uint64_VxgiMatStamp", (uint64_t)~0ull);
			const bool vxgi_mat_changed = (vxgi_prev_mat_stamp != vxgi_mat_stamp);
			const bool vxgi_rebuild = (vxgi_prev_stamp != vxgi_content_stamp);
			// SUSTAINED-EDIT detection, for the surface-gather throttle in the rebuild branch below.
			// grid_surf is stale the moment ANY rebuild lands (a MAT change moves the surface band itself;
			// a light change moves the radiance the cones integrated). Propagate composites it (t11) on every
			// bounce, so if the gather is skipped on the edit frame the OLD content's surface GI/AO rides the
			// field all the way to the next refinement checkpoint at T/2 — 7 bounces at the default gain. The
			// old gate made that the COMMON case AND a nondeterministic one: it keyed on the GLOBAL render
			// counter, so a one-shot edit hit `% 8 == 0` only ~1 frame in 8 and the same edit behaved
			// differently run to run.
			// Gathering on the edit frame does NOT zero the stale term — the gather runs AFTER that frame's
			// propagate (see the crossfade note below), so the old surf is still consumed by exactly ONE
			// propagation and is then replaced. That single step is the intended crossfade residue; what this
			// removes is the 7-bounce tail behind it and the pop when the T/2 checkpoint finally lands.
			// Throttle only what actually needs throttling: a SUSTAINED edit (clip/OTF/light drag, sculpt
			// stroke), where the rebuild branch is hit every frame and the stale surf is merely the PREVIOUS
			// DRAG FRAME's — a small, gain-composited delta. There the 6-cone pass (a rebuild-class spike, on
			// top of the per-frame re-voxelize) must stay throttled to keep the drag interactive.
			// On stability: gathering more often is not a divergence risk, but not because the pass is
			// feedback-free — it isn't. SurfaceGather writes grid_surf (u0) and reads the radiance grid (t8),
			// which a previous Propagate already composited grid_surf INTO: the loop closes indirectly, through
			// the checkpoints. It stays bounded because Propagate is a contraction (scatter gain clamped < 1)
			// and the surface term enters at surface_gi_gain (~0.15). So the honest claim is not "cannot
			// flicker" but: this trades the single large pop at bounce T/2 for smaller continuous change.
			// ~0ull = "no rebuild yet" (same sentinel convention as _uint64_VxgiStamp above). NOT 0: core's
			// renderer_excute_count starts at 0 and is forwarded BEFORE its post-render increment, so frame 0
			// is a real frame index — a 0 default would alias "rebuilt on frame 0" with "never rebuilt" and
			// mis-read the second frame of a drag that began on frame 0 as a fresh edit (a harmless extra
			// gather, but the sentinel should not be ambiguous).
			const uint64_t vxgi_prev_rebuild_frame = iobj->GetObjParam<uint64_t>("_uint64_VxgiRebuildFrame", (uint64_t)~0ull);
			const bool vxgi_edit_sustained = (vxgi_prev_rebuild_frame != (uint64_t)~0ull)
				&& (temporal_render_count >= vxgi_prev_rebuild_frame)
				&& (temporal_render_count - vxgi_prev_rebuild_frame) <= 1;
			if (vxgi_rebuild && !vxgi_mat_changed)
				cbVxgi.vxgi_flag |= 0x40u; // bit6: LIGHT-ONLY inject — re-emit the baked DIRECT alpha (t11)
			// upload the CB (deferred to here so the preserve-AO bit above is part of it)
			D3D11_MAPPED_SUBRESOURCE mappedResVxgi;
			dx11DeviceImmContext->Map(cbuf_vxgi, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResVxgi);
			memcpy(mappedResVxgi.pData, &cbVxgi, sizeof(CB_VXGI));
			dx11DeviceImmContext->Unmap(cbuf_vxgi, 0);
			SET_CBUFFERS(13, 1, &cbuf_vxgi);

			if (vxgi_rebuild)
			{
				// -- content changed: (re)bake what actually changed, then advance the radiance grid --
				ID3D11UnorderedAccessView* vxgi_uav_mat = (ID3D11UnorderedAccessView*)gres_vxgi_mat.alloc_res_ptrs[DTYPE_UAV];
				ID3D11UnorderedAccessView* vxgi_uav_direct = (ID3D11UnorderedAccessView*)gres_vxgi_direct.alloc_res_ptrs[DTYPE_UAV];

				// 1) Voxelize -> MAT (u0) — MAT-side changes only. Reads t0=volume, t3=OTF (already bound).
				//    MAT gets a mip chain so the inject's light march can LOD through it.
				if (vxgi_mat_changed)
				{
					dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, &vxgi_uav_mat, (UINT*)(&vxgi_uav_mat));
					SET_SHADER(GETCS(VXGI_VoxelizeVolume_cs_5_0), NULL, 0);
					dx11DeviceImmContext->Dispatch(vxgi_groups, vxgi_groups, vxgi_groups);
					dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));

					// 1.5) MAT blur (3^3 gaussian, BEFORE GenerateMips): kills the stored surface-vs-
					// mip-lattice phase banding at its single source — box mips of the blurred field
					// get OVERLAPPING effective kernels, so the band energy the surface cones were
					// integrating (the AO contour bands that survived a FIXED normal, cubic taps at
					// every lod, large origin offsets and the slab clip) never gets baked. PING is
					// free here (its obscurance-blur / prev-DIRECT uses come later). See BlurMat.hlsl.
					if (vxgi_mat_blur)
					{
						ID3D11UnorderedAccessView* vxgi_uav_ping_m = (ID3D11UnorderedAccessView*)gres_vxgi_ping.alloc_res_ptrs[DTYPE_UAV];
						SET_SHADER_RES(9, 1, &vxgi_mat_srv);
						dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, &vxgi_uav_ping_m, (UINT*)(&vxgi_uav_ping_m));
						SET_SHADER(GETCS(VXGI_BlurMat_cs_5_0), NULL, 0);
						dx11DeviceImmContext->Dispatch(vxgi_groups, vxgi_groups, vxgi_groups);
						dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
						SET_SHADER_RES(9, 1, dx11SRVs_NULL);
						dx11DeviceImmContext->CopySubresourceRegion(
							(ID3D11Resource*)gres_vxgi_mat.alloc_res_ptrs[DTYPE_RES], 0, 0, 0, 0,
							(ID3D11Resource*)gres_vxgi_ping.alloc_res_ptrs[DTYPE_RES], 0, NULL);
					}
					dx11DeviceImmContext->GenerateMips(vxgi_mat_srv);
				}

				// 2) Inject: light-transmittance march through MAT (t9, mips) -> DIRECT field (u0).
				// LIGHT-ONLY rebuild: the shader re-emits the previous baked alpha (cubic obscurance)
				// instead of recomputing it — DIRECT is copied to PING first and bound at t11 as the
				// alpha source (a UAV cannot read its own previous texels without typed-UAV loads).
				ID3D11ShaderResourceView* vxgi_ping_srv_a = (ID3D11ShaderResourceView*)gres_vxgi_ping.alloc_res_ptrs[DTYPE_SRV];
				if (!vxgi_mat_changed)
				{
					dx11DeviceImmContext->CopySubresourceRegion(
						(ID3D11Resource*)gres_vxgi_ping.alloc_res_ptrs[DTYPE_RES], 0, 0, 0, 0,
						(ID3D11Resource*)gres_vxgi_direct.alloc_res_ptrs[DTYPE_RES], 0, NULL);
					SET_SHADER_RES(11, 1, &vxgi_ping_srv_a);
				}
				SET_SHADER_RES(9, 1, &vxgi_mat_srv);
				dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, &vxgi_uav_direct, (UINT*)(&vxgi_uav_direct));
				SET_SHADER(GETCS(VXGI_InjectLight_cs_5_0), NULL, 0);
				dx11DeviceImmContext->Dispatch(vxgi_groups, vxgi_groups, vxgi_groups);
				dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
				SET_SHADER_RES(9, 1, dx11SRVs_NULL);
				SET_SHADER_RES(11, 1, dx11SRVs_NULL);

				// (The former "2.5) post-bake obscurance blur" — BlurObscuranceX/Y/Z on the baked DIRECT
				// alpha, _bool_VxgiAoBlur — was REMOVED: the MAT blur in step 1.5 smooths the same band
				// energy at its source, before the cubic density taps, and A/B showed no remaining
				// visible contribution from the post-pass.)

				// 3) Advance the RADIANCE grid this frame — the rebuild branch must never leave it stale.
				// First build (or a runtime resolution change): the grid texture is UNINITIALIZED — seed it
				// from DIRECT (never crossfade from garbage). OTHERWISE (P0 fix): run ONE diffusion step
				// against the new DIRECT right here. Continuous edits (light/OTF/clip/transform drags) hit
				// this branch EVERY frame, so the stamp-unchanged propagate below never runs during a drag —
				// without this step the visible field stayed FROZEN at the pre-drag state until release.
				// One source-term step per frame (r' = direct_new + gain*gather(prev field)) IS the temporal
				// crossfade (Wicked-style grid blend adapted to our content-gated pipeline): the field follows
				// the drag smoothly and keeps converging from wherever it was — no hard reset, no freeze.
				const int vxgi_prev_res = iobj->GetObjParam<int>("_int_VxgiGridRes", (int)0);
				// HARD RESTART (debug "Restart GI bounces" button): the source-term diffusion converges to
				// a FIXED POINT, so once converged, extra propagate iterations change nothing visibly —
				// re-running from bounce 0 without resetting the field shows nothing (that is exactly the
				// crossfade design for light/OTF edits). The restart button's purpose is to OBSERVE the
				// progressive spread, so it must hard-seed the grid back to DIRECT.
				const uint64_t vxgi_prev_restart = iobj->GetObjParam<uint64_t>("_uint64_VxgiRestartApplied", (uint64_t)0);
				const bool vxgi_first_build = (vxgi_prev_stamp == (uint64_t)~0ull) || (vxgi_prev_res != (int)vxgi_R)
					|| (vxgi_restart != vxgi_prev_restart);
				if (vxgi_first_build)
				{
					// D3D11 gives no zero-fill guarantee on a fresh texture: clear SURF once so the
					// checkpoints==0 path (SurfaceGather never dispatched) cannot leak garbage/NaN into
					// the Propagate composite (0 * NaN = NaN would still poison the field).
					const FLOAT vxgi_surf_zero[4] = { 0.f, 0.f, 0.f, 0.f };
					dx11DeviceImmContext->ClearUnorderedAccessViewFloat(
						(ID3D11UnorderedAccessView*)gres_vxgi_surf.alloc_res_ptrs[DTYPE_UAV], vxgi_surf_zero);
					dx11DeviceImmContext->CopySubresourceRegion(
						(ID3D11Resource*)gres_vxgi.alloc_res_ptrs[DTYPE_RES], 0, 0, 0, 0,
						(ID3D11Resource*)gres_vxgi_direct.alloc_res_ptrs[DTYPE_RES], 0, NULL);
					dx11DeviceImmContext->GenerateMips(vxgi_grid_srv);
					vxgi_bounce = 0; // seeded to bounce 0 exactly
					// Part C checkpoint 0: cones read the DIRECT-seeded field (mips just generated above).
					if (vxgi_surface_checkpoints > 0)
						vxgi_surface_gather();
				}
				else
				{
					vxgi_propagate_once();
					vxgi_bounce = 1; // one diffusion iteration already applied against the new content
					// Part C checkpoint 0 (crossfade path): AFTER the transition step above, so the cones
					// read the previous field one step into its blend toward the new DIRECT. That first
					// propagate consumed the PREVIOUS lighting's grid_surf — allowed BY DESIGN (plan §4.3):
					// crossfade means carrying the whole previous field (its surface term included) while
					// blending; zeroing the term for one step would pop it out and back in. DRAG THROTTLE:
					// continuous edits (light/clip drags) hit this branch every frame — a full 6-cone
					// surface pass per drag frame is a rebuild-class spike, so gate it to every 8th frame
					// (same pattern as _bool_VxgiSlowMotion); the in-between frames keep the slightly stale
					// surf (gain-composited, so no jump) and the convergence-loop checkpoints refine after
					// release.
					// The throttle applies to SUSTAINED edits ONLY (see vxgi_edit_sustained): on the first
					// frame of an edit the stale surf is the OLD content's, and it would otherwise ride the
					// field for every bounce up to the T/2 checkpoint.
					if (vxgi_surface_checkpoints > 0 && (!vxgi_edit_sustained || (temporal_render_count % 8) == 0))
						vxgi_surface_gather();
				}
				iobj->SetObjParam("_uint64_VxgiStamp", vxgi_content_stamp);
				iobj->SetObjParam("_uint64_VxgiMatStamp", vxgi_mat_stamp);
				iobj->SetObjParam("_uint64_VxgiRestartApplied", vxgi_restart);
				iobj->SetObjParam("_int_VxgiGridRes", (int)vxgi_R);
				// Frame of THIS rebuild — the next one compares against it to tell a sustained drag (rebuilt
				// on the previous frame too) from the first frame of an edit, which must gather immediately.
				iobj->SetObjParam("_uint64_VxgiRebuildFrame", temporal_render_count);
			}
			else if (vxgi_bounce < VXGI_BOUNCE_TARGET
				// Slow-motion is an explicit app toggle (SetRenderTestParam "_bool_VxgiSlowMotion"), no longer
				// tied to the debug view mode: advance one iteration every 8th rendered frame while on, full
				// speed otherwise. Frame index = the GLOBAL render count forwarded by core (monotonic).
				&& (!vxgi_slowmo || (temporal_render_count % 8) == 0))
			{
				// Part C refinement checkpoints (plan §4.2/§4.5): derived from N and the target T —
				// N=2 adds {T/2}, N=3 adds {T/2, T-1} (checkpoint 0 ran in the rebuild branch). Bounce-value
				// based, so the slow-motion gate above is automatically compatible (bounce only advances on
				// frames that propagate). SurfaceGather runs BEFORE this bounce's propagate: the rewritten
				// surface term is composited into the field the same frame (§4.3 order).
				const bool vxgi_ckpt = (vxgi_surface_checkpoints >= 2 && vxgi_bounce == VXGI_BOUNCE_TARGET / 2)
					|| (vxgi_surface_checkpoints >= 3 && vxgi_bounce == VXGI_BOUNCE_TARGET - 1);
				if (vxgi_ckpt)
					vxgi_surface_gather();
				// -- content static: ONE volumetric diffusion iteration (progressive refinement) --
				vxgi_propagate_once();
				vxgi_bounce++;
			}

			// Convergence readback: core's skip-gate / CheckRenderConvergence keep the re-render loop alive
			// until both TAA samples AND VXGI bounces are done (renderer owns the algorithm, core only reads).
			iobj->SetObjParam("_int_VxgiBounce", vxgi_bounce);
			iobj->SetObjParam("_int_VxgiBounceTarget", (int)VXGI_BOUNCE_TARGET);

			// Bind the radiance grid (t8) + MAT grid (t9) for the RayCasting march below (nulled after the
			// dispatch). The grids' alpha is PREMULTIPLIED (obscurance * coverage); the DVR un-premultiplies
			// with the MAT coverage at the same lod — see the AO fetch in DvrCS.
			SET_SHADER_RES(8, 1, &vxgi_grid_srv);
			SET_SHADER_RES(9, 1, &vxgi_mat_srv);
		}
		else
		{
			// DISABLED-STATE CONTRACT. b13 and t8/t9 are immediate-context state that OUTLIVES this draw:
			// bindings persist across volumes and across frames. Leave them alone and a VXGI-off (or
			// non-owner, or x-ray) march reads whatever the last VXGI-on draw left there — in particular
			// cbuf_vxgi still holds vxgi_flag bit0 = 1, so VXGI_IS_ENABLED is STALE TRUE and the shader
			// takes the in-scatter path against grids it does not own.
			//
			// This has been harmless only by luck: the SRVs happen to be null by then, and a null-SRV
			// sample returns 0, which sends the AO term through 0/max(0,1e-3) -> sqrt(0) -> factor 1 (an
			// identity) and the in-scatter to +0. Any change to that arithmetic breaks it silently. So
			// state it as a contract instead: when VXGI is not active for this draw, b13 carries a
			// DISABLED CB and t8/t9 are explicitly null.
			CB_VXGI cbVxgiOff; // ZERO_SET ctor: vxgi_flag = 0 => VXGI_IS_ENABLED reads false
			ID3D11Buffer* cbuf_vxgi_off = psoManager->get_cbuf("CB_VXGI");
			D3D11_MAPPED_SUBRESOURCE mappedResVxgiOff;
			dx11DeviceImmContext->Map(cbuf_vxgi_off, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResVxgiOff);
			memcpy(mappedResVxgiOff.pData, &cbVxgiOff, sizeof(CB_VXGI));
			dx11DeviceImmContext->Unmap(cbuf_vxgi_off, 0);
			SET_CBUFFERS(13, 1, &cbuf_vxgi_off);
			SET_SHADER_RES(8, 1, dx11SRVs_NULL);
			SET_SHADER_RES(9, 1, dx11SRVs_NULL);
		}
#endif

#pragma region GPU resource updates
#pragma endregion

#pragma region Renderer
		// LATER... MFR_MODE::DYNAMIC_KB ==> DEPRECATED in VR
		// MFR_MODE::DYNAMIC_KB also uses static K buffer in VR
#ifdef DX10_0
		ID3D11PixelShader* pshader = NULL;
		switch (ray_cast_type)
		{
		case __RM_RAYMIN: pshader = GETPS(VR_RAYMIN_ps_4_0); break;
		case __RM_RAYMAX: pshader = GETPS(VR_RAYMAX_ps_4_0); break;
		case __RM_RAYSUM: pshader = GETPS(VR_RAYSUM_ps_4_0); break;
		case __RM_CLIPOPAQUE:
		case __RM_OPAQUE: pshader = GETPS(VR_OPAQUE_ps_4_0); break;
		case __RM_MODULATION: pshader = GETPS(VR_CONTEXT_ps_4_0); break;
		case __RM_MULTIOTF: pshader = GETPS(VR_MULTIOTF_ps_4_0); break;
		case __RM_MULTIOTF_MODULATION: pshader = GETPS(VR_MULTIOTF_CONTEXT_ps_4_0); break;
		case __RM_VISVOLMASK: pshader = GETPS(VR_MASKVIS_ps_4_0); break;
		case __RM_SCULPTMASK: pshader = 
			sculptBitPackedObj? GETPS(VR_DEFAULT_SCULPTBITS_ps_4_0) : GETPS(VR_SCULPTMASK_ps_4_0);
			break;
		case __RM_SCULPTMASK_MODULATION: pshader = 
			sculptBitPackedObj? GETPS(VR_CONTEXT_SCULPTBITS_ps_4_0) : GETPS(VR_SCULPTMASK_CONTEXT_ps_4_0);
			break;
		case __RM_DEFAULT:
		default: pshader = GETPS(VR_DEFAULT_ps_4_0); break;
		}
#else
		ID3D11ComputeShader* cshader = NULL;
		switch (ray_cast_type)
		{
		case __RM_RAYMIN: cshader = is_sculpt_mode? 
			(sculptBitPackedObj ? GETCS(VR_RAYMIN_SCULPTBITS_cs_5_0) : GETCS(VR_RAYMIN_SCULPTMASK_cs_5_0)) : GETCS(VR_RAYMIN_cs_5_0); break;
		case __RM_RAYMAX: cshader = is_sculpt_mode? 
			(sculptBitPackedObj ? GETCS(VR_RAYMAX_SCULPTBITS_cs_5_0) : GETCS(VR_RAYMAX_SCULPTMASK_cs_5_0)) : GETCS(VR_RAYMAX_cs_5_0); break;
		case __RM_RAYSUM: cshader = is_sculpt_mode? 
			(sculptBitPackedObj ? GETCS(VR_RAYSUM_SCULPTBITS_cs_5_0) : GETCS(VR_RAYSUM_SCULPTMASK_cs_5_0)) : GETCS(VR_RAYSUM_cs_5_0); break;
		case __RM_CLIPOPAQUE:
		case __RM_OPAQUE: 
			switch (mode_OIT)
			{
				// VR_OPAQUE_DFB_cs_5_0 is same as VR_OPAQUE_DKB_cs_5_0
			case MFR_MODE::STATIC_KB:
			case MFR_MODE::DYNAMIC_FB:
				if (is_last_dvr)
					cshader = apply_fragmerge ? GETCS(VR_OPAQUE_FM_cs_5_0) : GETCS(VR_OPAQUE_cs_5_0); 
				else
					cshader = GETCS(VR_SINGLE_OPAQUE_FM_cs_5_0);
				break;
			case MFR_MODE::DYNAMIC_KB:
				cshader = apply_fragmerge? GETCS(VR_OPAQUE_DKBZ_cs_5_0) : GETCS(VR_OPAQUE_DFB_cs_5_0); break;
			default:
				VMERRORMESSAGE("DOES NOT SUPPORT!!");
			}
			break;
		case __RM_OPAQUE_MULTIOTF:
			switch (mode_OIT)
			{
			case MFR_MODE::STATIC_KB:
			case MFR_MODE::DYNAMIC_FB:
				if (is_last_dvr)
					cshader = GETCS(VR_OPAQUE_MULTIOTF_FM_cs_5_0);
				else
					cshader = GETCS(VR_SINGLE_OPAQUE_MULTIOTF_FM_cs_5_0);
				break;
			case MFR_MODE::DYNAMIC_KB:
				assert(0); break;
			default:
				VMERRORMESSAGE("DOES NOT SUPPORT!!");
			}
			break;
		case __RM_MODULATION:
			switch (mode_OIT)
			{
			case MFR_MODE::STATIC_KB:
			case MFR_MODE::DYNAMIC_FB:
				if (is_last_dvr)
					cshader = apply_fragmerge ? GETCS(VR_CONTEXT_FM_cs_5_0) : GETCS(VR_CONTEXT_cs_5_0); 
				else
					cshader = GETCS(VR_SINGLE_CONTEXT_FM_cs_5_0);
				break;
			case MFR_MODE::DYNAMIC_KB:
				cshader = apply_fragmerge ? GETCS(VR_CONTEXT_DKBZ_cs_5_0) : GETCS(VR_CONTEXT_DFB_cs_5_0); break;
			default:
				VMERRORMESSAGE("DOES NOT SUPPORT!!");
			}
			break;
		case __RM_MULTIOTF:
			switch (mode_OIT)
			{
			case MFR_MODE::STATIC_KB:
			case MFR_MODE::DYNAMIC_FB:
				if (is_last_dvr)
					cshader = apply_fragmerge ? GETCS(VR_MULTIOTF_FM_cs_5_0) : GETCS(VR_MULTIOTF_cs_5_0);
				else
					cshader = GETCS(VR_SINGLE_MULTIOTF_FM_cs_5_0);
				break;
			case MFR_MODE::DYNAMIC_KB:
				assert(0); break;
			default:
				VMERRORMESSAGE("DOES NOT SUPPORT!!");
			}
			break;
		case __RM_MULTIOTF_MODULATION:
			switch (mode_OIT)
			{
			case MFR_MODE::STATIC_KB:
			case MFR_MODE::DYNAMIC_FB:
				assert(apply_fragmerge);
				if (is_last_dvr)
					cshader = GETCS(VR_MULTIOTF_CONTEXT_FM_cs_5_0);
				else
					cshader = GETCS(VR_SINGLE_MULTIOTF_CONTEXT_FM_cs_5_0);
				break;
			case MFR_MODE::DYNAMIC_KB:
				assert(0); break;
			default:
				VMERRORMESSAGE("DOES NOT SUPPORT!!");
			}
			break;
		case __RM_VISVOLMASK:
			if (is_last_dvr)
				cshader = apply_fragmerge ? GETCS(VR_MASKVIS_FM_cs_5_0) : GETCS(VR_MASKVIS_cs_5_0);
			else
				cshader = GETCS(VR_SINGLE_MASKVIS_FM_cs_5_0);
			break;
		case __RM_SCULPTMASK:
			if (is_last_dvr)
				cshader = sculptBitPackedObj? GETCS(VR_DEFAULT_SCULPTBITS_FM_cs_5_0) : GETCS(VR_SCULPTMASK_FM_cs_5_0);
			else
				cshader = sculptBitPackedObj? GETCS(VR_SINGLE_DEFAULT_SCULPTBITS_FM_cs_5_0) : GETCS(VR_SINGLE_SCULPTMASK_FM_cs_5_0);
			break;
		case __RM_SCULPTMASK_MODULATION:
			if (is_last_dvr)
				cshader = sculptBitPackedObj? GETCS(VR_CONTEXT_SCULPTBITS_FM_cs_5_0) : GETCS(VR_SCULPTMASK_CONTEXT_FM_cs_5_0);
			else
				cshader = sculptBitPackedObj? GETCS(VR_SINGLE_CONTEXT_SCULPTBITS_FM_cs_5_0) : GETCS(VR_SINGLE_SCULPTMASK_CONTEXT_FM_cs_5_0);
			break;
		case __RM_SAMPLETEST:
			cshader = GETCS(SampleTest_cs_5_0); break;
		case __RM_DEFAULT:
		default:
			switch (mode_OIT)
			{
			case MFR_MODE::STATIC_KB:
			case MFR_MODE::DYNAMIC_FB:
				if (is_last_dvr)
					cshader = apply_fragmerge ? GETCS(VR_DEFAULT_FM_cs_5_0) : GETCS(VR_DEFAULT_cs_5_0);
				else
					cshader = GETCS(VR_SINGLE_DEFAULT_FM_cs_5_0);
				break;
			case MFR_MODE::DYNAMIC_KB:
				cshader = apply_fragmerge ? GETCS(VR_DEFAULT_DKBZ_cs_5_0) : GETCS(VR_DEFAULT_DFB_cs_5_0); break;
			default:
				VMERRORMESSAGE("DOES NOT SUPPORT!!");
			}
			break;
		}
 
		// When the x-ray post-filter is active on the last DVR pass (vobj_flag bit 2 set),
		// DvrCS writes volume-ONLY x-ray color into u2. Redirect u2 to the dedicated
		// volume-only scratch (gres_fb_xray_vol) so gres_fb_rgba stays untouched and becomes
		// the fused XrayFilterComposite pass's final output. u0/u1/u3 are unchanged; the
		// clean volume depth still lands in gres_fb_depthcs (u3).
		const bool xray_redirect_u2 = (is_last_dvr && apply_postprocessing_filter); // redirect only the last volume
		ID3D11UnorderedAccessView* dx11UAVs[4] = {
				  (ID3D11UnorderedAccessView*)gres_fb_counter.alloc_res_ptrs[DTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_fb_k_buffer.alloc_res_ptrs[DTYPE_UAV]
				, (ID3D11UnorderedAccessView*)(xray_redirect_u2 ? gres_fb_xray_vol.alloc_res_ptrs[DTYPE_UAV] : gres_fb_rgba.alloc_res_ptrs[DTYPE_UAV])
				, (ID3D11UnorderedAccessView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_UAV]
		};
		dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 4, dx11UAVs, (UINT*)(&dx11UAVs));

		SET_SHADER_RES(50, 1, (ID3D11ShaderResourceView**)&gres_fb_ref_pidx.alloc_res_ptrs[DTYPE_SRV]); // search why this does not work
#endif

		if(!is_xray_mode) {

#ifdef DX10_0
			ID3D11RenderTargetView* dx11RTVs[2] = {
				(ID3D11RenderTargetView*)gres_fb_vrenc.alloc_res_ptrs[DTYPE_RTV],
				(ID3D11RenderTargetView*)gres_fb_vrdepthcs.alloc_res_ptrs[DTYPE_RTV] };
			dx11DeviceImmContext->OMSetRenderTargets(2, dx11RTVs, NULL);

			SET_SHADER(GETPS(VR_SURFACE_ps_4_0), NULL, 0);

			dx11DeviceImmContext->Draw(4, 0);

			dx11DeviceImmContext->OMSetRenderTargets(2, dx11RTVs_NULL, NULL);
#else
			// 1st hit surface
			dx11DeviceImmContext->CSSetUnorderedAccessViews(4, 1, (ID3D11UnorderedAccessView**)&gres_fb_vrdepthcs.alloc_res_ptrs[DTYPE_UAV], (UINT*)(&dx11UAVs));

			SET_SHADER(GETCS(VR_SURFACE_cs_5_0), NULL, 0);

			dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);

			dx11DeviceImmContext->CSSetUnorderedAccessViews(4, 1, dx11UAVs_NULL, (UINT*)(&dx11UAVs));
#endif
			SET_SHADER_RES(6, 1, (ID3D11ShaderResourceView**)&gres_fb_vrdepthcs.alloc_res_ptrs[DTYPE_SRV]);
		}

#ifdef DX10_0
#else
		if (cbEnvState.r_kernel_ao > 0)
		{
			is_performed_ssao = true;
			//dx11DeviceImmContext->Flush();
			dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 5, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
			ComputeSSAO(dx11DeviceImmContext, psoManager, iobj, num_grid_x, num_grid_y,
				gres_fb_counter, gres_fb_k_buffer, gres_fb_rgba, blur_SSAO,
				gres_fb_depthcs, gres_fb_ao_vr_tex, gres_fb_ao_vr_blf_tex, true, apply_fragmerge);

			dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 4, dx11UAVs, (UINT*)(&dx11UAVs));

			if (blur_SSAO)
			{
				SET_SHADER_RES(10, 1, (ID3D11ShaderResourceView**)&gres_fb_ao_blf_texs[0].alloc_res_ptrs[DTYPE_SRV]);
				SET_SHADER_RES(11, 1, (ID3D11ShaderResourceView**)&gres_fb_ao_blf_texs[1].alloc_res_ptrs[DTYPE_SRV]);
				SET_SHADER_RES(20, 1, (ID3D11ShaderResourceView**)&gres_fb_ao_vr_blf_tex.alloc_res_ptrs[DTYPE_SRV]);
			}
			else
			{
				SET_SHADER_RES(10, 1, (ID3D11ShaderResourceView**)&gres_fb_ao_texs[0].alloc_res_ptrs[DTYPE_SRV]);
				SET_SHADER_RES(11, 1, (ID3D11ShaderResourceView**)&gres_fb_ao_texs[1].alloc_res_ptrs[DTYPE_SRV]);
				SET_SHADER_RES(20, 1, (ID3D11ShaderResourceView**)&gres_fb_ao_vr_tex.alloc_res_ptrs[DTYPE_SRV]);
			}
		}
#endif

#ifdef DX10_0
		SET_SHADER_RES(20, 1, (ID3D11ShaderResourceView**)&gres_fb_rgba_prev.alloc_res_ptrs[DTYPE_SRV]);
		SET_SHADER_RES(21, 1, (ID3D11ShaderResourceView**)&gres_fb_depthcs_prev.alloc_res_ptrs[DTYPE_SRV]);
		SET_SHADER_RES(22, 1, (ID3D11ShaderResourceView**)&gres_fb_vrenc.alloc_res_ptrs[DTYPE_SRV]);
		
		ID3D11RenderTargetView* dx11RTVs[2] = {
			(ID3D11RenderTargetView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_RTV],
			(ID3D11RenderTargetView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_RTV] };
		dx11DeviceImmContext->OMSetRenderTargets(2, dx11RTVs, NULL);

		SET_SHADER(pshader, NULL, 0);

		dx11DeviceImmContext->Draw(4, 0);

		dx11DeviceImmContext->OMSetRenderTargets(2, dx11RTVs_NULL, NULL);

		SET_SHADER_RES(20, 3, dx11SRVs_NULL);
#else
		// When u2 is redirected to the scratch RT, DvrCS reads the MDVR accumulation (vis_prev) from t40
		// instead of u2 — bind the real accumulation RT (gres_fb_rgba) there. It is SRV-only this pass
		// (u2 = scratch), so there is no SRV/UAV alias. For a single volume gres_fb_rgba is the (cleared)
		// RT -> vis_prev.a==0; for multi-volume it holds the accumulated earlier volumes.
		if (xray_redirect_u2)
			SET_SHADER_RES(40, 1, (ID3D11ShaderResourceView**)&gres_fb_rgba.alloc_res_ptrs[DTYPE_SRV]);
		SET_SHADER(cshader, NULL, 0);
		//dx11DeviceImmContext->Flush();
		// u5 = fragment_zthick. When the post-filter is active the bit-2 path stores the volume slab
		// thickness here for the fused pass; reuse the idle gres_fb_vrdepthcs ("RENDER_OUT_DEPTH_1",
		// always allocated) instead of gres_fb_thickcs (only allocated for multi-volume). Non-filter
		// x-ray is unchanged.
		ID3D11UnorderedAccessView* uav_u5 = (ID3D11UnorderedAccessView*)(xray_redirect_u2
			? gres_fb_vrdepthcs.alloc_res_ptrs[DTYPE_UAV] : gres_fb_thickcs.alloc_res_ptrs[DTYPE_UAV]);
		dx11DeviceImmContext->CSSetUnorderedAccessViews(5, 1, &uav_u5, (UINT*)(&uav_u5));
		dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);
		dx11DeviceImmContext->CSSetUnorderedAccessViews(5, 1, dx11UAVs_NULL, (UINT*)(&dx11UAVs));
		if (xray_redirect_u2)
			SET_SHADER_RES(40, 1, dx11SRVs_NULL);
		if (fastRender2x) {
			SET_SHADER(GETCS(FillDither_cs_5_0), NULL, 0);
			dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);
		}

		dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 4, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
		SET_SHADER_RES(6, 1, dx11SRVs_NULL);
		dx11DeviceImmContext->CSSetUnorderedAccessViews(50, 1, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));

		// VXGI v2: the RayCasting dispatch above sampled the voxel grid as SRV t8 (in-scatter) and the MAT
		// grid as t9 (coverage for AO un-premultiply). The grid build + binds now happen BEFORE RayCasting
		// (see the block right after the volume CB setup); release both here.
		// UNCONDITIONAL on purpose (it used to be gated on vxgi_active): these slots are rebound as UAVs by
		// the next rebuild's Voxelize/Propagate dispatches, so leaving a live SRV here is an SRV/UAV hazard —
		// and a conditional release is exactly what lets a binding survive into a draw that does not own it.
		// Nulling slots this draw never bound is free.
		SET_SHADER_RES(8, 1, dx11SRVs_NULL);
		SET_SHADER_RES(9, 1, dx11SRVs_NULL);

		// Slicer x-ray image-level post-processing filter + mesh composite (single fused pass).
		// Runs once, after the LAST DVR volume, when the post-filter is enabled. On that volume DvrCS wrote
		// the volume-ONLY x-ray color into gres_fb_xray_vol (u2 was redirected there) and the clean volume
		// depth into gres_fb_depthcs (vobj_flag bit 2). The mesh K-buffer is intact; gres_fb_rgba holds the
		// accumulated earlier volumes and is the final output target of this pass.
		if (is_last_dvr && apply_postprocessing_filter)
		{
			// --- build the convolution kernel from the X-ray post-filter mode (row-major, length N*N) ---
			// Mode/radius/strength come from CameraParameters::EnableXRayPostFilter (forwarded by RenderScene as
			// _int_XRayPostFilterMode / _int_XRayPostFilterRadius / _float_XRayPostFilterStrength). Modes mirror
			// vzm::CameraParameters::XRayPostFilter: MEAN box blur / GAUSSIAN / SHARPEN (unsharp-box) /
			// SHARPEN_GAUSSIAN / LAPLACIAN high-boost / EDGE high-pass. Brightness-preserving modes sum to 1;
			// EDGE sums to 0. NONE / radius==0 -> passthrough (use_filter = 0).
			//
			// The kernel is CACHED in iobj (XRPF_* obj-params): it is rebuilt on the CPU and re-uploaded to the
			// mask SRV ONLY when mode/radius/strength change. A steady filter setting therefore costs no per-frame
			// kernel rebuild and no GPU mask re-upload (UpdateCustomBuffer reuses the buffer when the timestamp
			// we pass has not advanced).
			const int mode = _fncontainer->fnParams.GetParam("_int_XRayPostFilterMode", (int)__XRPF_NONE);
			const float strength = (float)_fncontainer->fnParams.GetParam("_float_XRayPostFilterStrength", 1.f);
			int radius = _fncontainer->fnParams.GetParam("_int_XRayPostFilterRadius", (int)1);
			if (radius < 0) radius = 0;
			if (radius > 5) radius = 5; // max 11x11 = 121 weights
			const int N = 2 * radius + 1;
			const int kcount = N * N;
			const int center = radius * N + radius;
			const int use_filter = (mode != __XRPF_NONE && radius > 0) ? 1 : 0;

			// Rebuild when the setting changed OR the mask buffer is currently absent. A framebuffer resize
			// (or k_value / buffer_ex change) triggers ReleaseGpuResourcesBySrcID(iobj id) — from THIS renderer
			// or, more commonly, an earlier pass in the same frame: the MESH pass shares _int2_PreviousScreenSize
			// and runs first, so it consumes the resize and frees the iobj-keyed mask before we get here. Probe
			// the resource directly so the freed mask is rebuilt from valid weights instead of regenerated from
			// stale/uninitialized CPU data (which blacked out the volume after a resize).
			gres_xray_filter_mask.vm_src_id = iobj->GetObjectID();
			gres_xray_filter_mask.res_name = "XRAY_FILTER_MASK";
			const bool mask_absent = !gpu_manager->UpdateGpuResource(gres_xray_filter_mask);
			const bool filter_changed =
				   mask_absent
				|| mode     != iobj->GetObjParam<int>("XRPF_MODE", (int)-1)
				|| radius   != iobj->GetObjParam<int>("XRPF_RADIUS", (int)-1)
				|| strength != iobj->GetObjParam<float>("XRPF_STRENGTH", -2.f);
			uint64_t filter_time = iobj->GetObjParam<uint64_t>("XRPF_TIME", (uint64_t)0);

			float mask_weights[121];
			if (filter_changed)
			{
				// F10 dedup: kernel math shared with the curved path (renderer/RendererHeader.cpp).
				BuildXrayPostFilterKernel(mode, strength, radius, N, kcount, center, use_filter, mask_weights);

				filter_time = vmhelpers::GetCurrentTimePack(); // advance so UpdateCustomBuffer re-uploads exactly once
				iobj->SetObjParam("XRPF_MODE", mode);
				iobj->SetObjParam("XRPF_RADIUS", radius);
				iobj->SetObjParam("XRPF_STRENGTH", strength);
				iobj->SetObjParam("XRPF_TIME", filter_time);
			}

			// Upload the mask. filter_time only advances on a setting change, so on steady frames UpdateCustomBuffer
			// finds its stored timestamp already current and reuses the buffer (no Map / no re-upload). When unchanged
			// mask_weights is left untouched above, but it is not read in that case (the size matches and the upload is skipped).
			const int upload_count = (kcount > 0 ? kcount : 1);
			grd_helper::UpdateCustomBuffer(gres_xray_filter_mask, iobj, "XRAY_FILTER_MASK",
				mask_weights, upload_count, DXGI_FORMAT_R32_FLOAT, (int)sizeof(float), NULL, filter_time);

			// upload cbSliceFilter (b12)
			{
				grd_helper::CB_SliceFilter cb_sf = {};
				cb_sf.filter_radius = radius;
				cb_sf.use_filter = use_filter;
				ID3D11Buffer* cbuf_sf = psoManager->get_cbuf("CB_SliceFilter");
				D3D11_MAPPED_SUBRESOURCE mappedResSf;
				dx11DeviceImmContext->Map(cbuf_sf, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResSf);
				memcpy(mappedResSf.pData, &cb_sf, sizeof(grd_helper::CB_SliceFilter));
				dx11DeviceImmContext->Unmap(cbuf_sf, 0);
				SET_CBUFFERS(12, 1, &cbuf_sf);
			}

			// Fused pass : NxN filter of the volume-only color + intermix with the mesh K-buffer.
			//   u0..u3 = counter / k_buffer / rgba(final) / depthcs
			//   t0 = gres_fb_xray_vol (volume-only color), t1 = mask buffer, t50 = offset table
			ID3D11UnorderedAccessView* dx11UAVs_pf[4] = {
				  (ID3D11UnorderedAccessView*)gres_fb_counter.alloc_res_ptrs[DTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_fb_k_buffer.alloc_res_ptrs[DTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_UAV]
			};
			dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 4, dx11UAVs_pf, (UINT*)(&dx11UAVs_pf));
			// u5 = gres_fb_vrdepthcs : the bit-2 DVR pass stored the volume slab thickness (hits_t.y - hits_t.x)
			// into this idle buffer; the fused pass reads it as the INTERMIX thick_sample.
			dx11DeviceImmContext->CSSetUnorderedAccessViews(5, 1, (ID3D11UnorderedAccessView**)&gres_fb_vrdepthcs.alloc_res_ptrs[DTYPE_UAV], (UINT*)(&dx11UAVs_pf));
			SET_SHADER_RES(0, 1, (ID3D11ShaderResourceView**)&gres_fb_xray_vol.alloc_res_ptrs[DTYPE_SRV]);
			SET_SHADER_RES(1, 1, (ID3D11ShaderResourceView**)&gres_xray_filter_mask.alloc_res_ptrs[DTYPE_SRV]);
			SET_SHADER_RES(50, 1, (ID3D11ShaderResourceView**)&gres_fb_ref_pidx.alloc_res_ptrs[DTYPE_SRV]);
			SET_SHADER(GETCS(XrayFilterComposite_cs_5_0), NULL, 0);
			dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);
			dx11DeviceImmContext->CSSetUnorderedAccessViews(5, 1, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
			dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 4, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
			SET_SHADER_RES(0, 1, dx11SRVs_NULL);
			SET_SHADER_RES(1, 1, dx11SRVs_NULL);
			SET_SHADER_RES(50, 1, dx11SRVs_NULL);
		}
#endif

#ifdef __DX_DEBUG_QUERY
		psoManager->debug_info_queue->PushEmptyStorageFilter();
#endif
		count_call_render++;
#pragma endregion 
		vr_render_count++;
	}

#ifdef DX10_0
#else
	if (cbEnvState.r_kernel_ao > 0 && !is_performed_ssao)
	{
		ID3D11UnorderedAccessView* dx11UAVs[4] = {
				  (ID3D11UnorderedAccessView*)gres_fb_counter.alloc_res_ptrs[DTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_fb_k_buffer.alloc_res_ptrs[DTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_UAV]
		};
		is_performed_ssao = true;
		//dx11DeviceImmContext->Flush();
		dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 5, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
		ComputeSSAO(dx11DeviceImmContext, psoManager, iobj, num_grid_x, num_grid_y,
			gres_fb_counter, gres_fb_k_buffer, gres_fb_rgba, blur_SSAO,
			gres_fb_depthcs, gres_fb_ao_vr_tex, gres_fb_ao_vr_blf_tex, true, apply_fragmerge);

		dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 4, dx11UAVs, (UINT*)(&dx11UAVs));

		if (blur_SSAO)
		{
			SET_SHADER_RES(10, 1, (ID3D11ShaderResourceView**)&gres_fb_ao_blf_texs[0].alloc_res_ptrs[DTYPE_SRV]);
			SET_SHADER_RES(11, 1, (ID3D11ShaderResourceView**)&gres_fb_ao_blf_texs[1].alloc_res_ptrs[DTYPE_SRV]);
			SET_SHADER_RES(20, 1, (ID3D11ShaderResourceView**)&gres_fb_ao_vr_blf_tex.alloc_res_ptrs[DTYPE_SRV]);
		}
		else
		{
			SET_SHADER_RES(10, 1, (ID3D11ShaderResourceView**)&gres_fb_ao_texs[0].alloc_res_ptrs[DTYPE_SRV]);
			SET_SHADER_RES(11, 1, (ID3D11ShaderResourceView**)&gres_fb_ao_texs[1].alloc_res_ptrs[DTYPE_SRV]);
			SET_SHADER_RES(20, 1, (ID3D11ShaderResourceView**)&gres_fb_ao_vr_tex.alloc_res_ptrs[DTYPE_SRV]);
		}
	}
#endif

#ifdef DX10_0
#else
	// ----- VXGI debug view (v5): voxel ray-march visualizations ONLY -----
	// The v1..v4 screen-space half (DVR-first-hit probes + legacy hemisphere cone AO/indirect) is
	// REMOVED (plan §3.6) — sampling the grid at arbitrary reconstructed surface points was
	// surface-vs-lattice phase-sensitive (the stripe artifacts that retired v1..v3), and the
	// production GI terms are per-sample inside the DVR march since v4. Modes 1..6 (radiance/
	// obscurance at LOD, SURF indirect/cone-AO/state/normal) all ray-march the grids in Gather.hlsl.
	if (vxgi_on && vxgi_ready && (vxgi_debug & 0xFF) != 0)
	{
		// The trailing SSAO path can leave gres_fb_rgba bound at UAV u2; release the DVR UAV slots first so
		// binding it at u1 below does not alias (a resource may occupy only one UAV slot at a time).
		dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 5, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));

		ID3D11ShaderResourceView* vxgi_grid_srv = (ID3D11ShaderResourceView*)gres_vxgi.alloc_res_ptrs[DTYPE_SRV];
		SET_SHADER_RES(0, 1, &vxgi_grid_srv);
		// t1 (first-hit depth) / t2 (volume) binds removed with the screen-space modes (plan §3.6).
		SET_SHADER_RES(3, 1, &vxgi_mat_srv_dbg); // MAT grid: true coverage — radiance grid alpha is OBSCURANCE
		SET_SHADER_RES(4, 1, &vxgi_surf_srv_dbg); // SURF grid (Part C): DEBUG-ONLY bind — voxel modes 3..6; the normal render path never binds t4

		// Re-bind the CBs the debug shader reads (b0=camera, b13=VXGI); the trailing SSAO pass may have
		// rebound these slots. (b4=volume rebind removed with the screen-space modes — the voxel-march
		// views never touch g_cbVobj.) The buffers still hold the correct last-camera / VXGI data.
		ID3D11Buffer* cbuf_cam_g = psoManager->get_cbuf("CB_CameraState");
		ID3D11Buffer* cbuf_vxgi_g = psoManager->get_cbuf("CB_VXGI");
		SET_CBUFFERS(0, 1, &cbuf_cam_g);
		SET_CBUFFERS(13, 1, &cbuf_vxgi_g);

		ID3D11UnorderedAccessView* vxgi_out_uav = (ID3D11UnorderedAccessView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_UAV];
		dx11DeviceImmContext->CSSetUnorderedAccessViews(1, 1, &vxgi_out_uav, (UINT*)(&vxgi_out_uav));

		const uint32_t vxgi_gx = (uint32_t)ceil(fb_size_cur.x / 8.f);
		const uint32_t vxgi_gy = (uint32_t)ceil(fb_size_cur.y / 8.f);
		SET_SHADER(GETCS(VXGI_Gather_cs_5_0), NULL, 0);
		dx11DeviceImmContext->Dispatch(vxgi_gx, vxgi_gy, 1);

		// Release the RT UAV + SRVs so the later DoModule composite / TaaResolve can rebind RENDER_OUT_RGBA_0.
		dx11DeviceImmContext->CSSetUnorderedAccessViews(1, 1, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
		SET_SHADER_RES(0, 5, dx11SRVs_NULL);
	}
#endif

	iobj->SetObjParam("_int_NumCallRenders", count_call_render);

	//dx11DeviceImmContext->Flush();
	//printf("# Textures : %d, # Drawing : %d, # RTBuffer Change : %d, # Merging : %d\n", iNumTexureLayers, iCountRendering, iCountRTBuffers, iCountMerging);
	psoManager->GpuProfile("VR Begin", true);

	dx11DeviceImmContext->ClearState();

	ID3D11RenderTargetView* pdxRTVOlds[2];
	pdxRTVOlds[0] = pdxRTVOld;
	pdxRTVOlds[1] = NULL;
	dx11DeviceImmContext->OMSetRenderTargets(2, pdxRTVOlds, pdxDSVOld);
	VMSAFE_RELEASE(pdxRTVOld);
	VMSAFE_RELEASE(pdxDSVOld);

	iobj->SetDescriptor("vismtv_inbuilt_renderergpudx module : Volume Renderer");

	//clock_t finish = clock();
	//double duration = (double)(finish - start) / CLOCKS_PER_SEC;
	//printf("###################### %f sec\n", duration);

	return true;
}