#include "RendererHeader.h"
#include <time.h>

using namespace grd_helper;

bool RenderVrDLS(VmFnContainer* _fncontainer,
	VmGpuManager* gpu_manager,
	grd_helper::GpuDX11CommonParameters* dx11CommonParams,
	LocalProgress* progress,
	double* run_time_ptr)
{
#ifdef __DX_DEBUG_QUERY
	if (dx11CommonParams->debug_info_queue == NULL)
		dx11CommonParams->dx11Device->QueryInterface(__uuidof(ID3D11InfoQueue), (void **)&dx11CommonParams->debug_info_queue);
	dx11CommonParams->debug_info_queue->PushEmptyStorageFilter();
#endif


	//clock_t start = clock();
	
#pragma region // Parameter Setting //
	VmIObject* iobj = _fncontainer->fnParams.GetParam("_VmIObject*_RenderOut", (VmIObject*)NULL);
	bool isSlicer = _fncontainer->fnParams.GetParam("_bool_IsSlicer", false);
	int k_value_old = iobj->GetObjParam("_int_NumK", isSlicer? (int)K_NUM_SLICER : (int)K_NUM_3D);
	int k_value = _fncontainer->fnParams.GetParam("_int_NumK", k_value_old);
	iobj->SetObjParam("_int_NumK", k_value);

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

	int ray_cast_type = _fncontainer->fnParams.GetParam("_int_VolumeRayCastType", (int)0);

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
	// Shader Re-Compile Setting //
	if (reload_hlsl_objs)
	{
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
		//hlslobj_path += "..\\..\\VmModuleProjects\\renderer_gpudx11\\shader_compiled_objs\\";
		//hlslobj_path += "..\\..\\VmModuleProjects\\plugin_gpudx11_renderer\\shader_compiled_objs\\";
		hlslobj_path += "..\\..\\VmProjects\\hybrid_rendering_engine\\";
		//cout << hlslobj_path << endl;
		string enginePath;
		if (grd_helper::GetEnginePath(enginePath)) {
			hlslobj_path = enginePath;
		}
		string hlslobj_path_4_0 = hlslobj_path + "shader_compiled_objs_4_0\\";

#ifdef DX10_0
		hlslobj_path += "shader_compiled_objs_4_0\\";
#else
		hlslobj_path += "shader_compiled_objs\\";
#endif
		string prefix_path = hlslobj_path;
		vmlog::LogInfo("RELOAD HLSL _ VR renderer");

#ifdef DX10_0
#define PS_NUM 12
#define SET_PS(NAME) dx11CommonParams->safe_set_res(grd_helper::COMRES_INDICATOR(GpuhelperResType::PIXEL_SHADER, NAME), dx11PShader, true)

		string strNames_PS[PS_NUM] = {
			   "VR_RAYMAX_ps_4_0"
			  ,"VR_RAYMIN_ps_4_0"
			  ,"VR_RAYSUM_ps_4_0"
			   "VR_DEFAULT_ps_4_0"
			  ,"VR_OPAQUE_ps_4_0"
			  ,"VR_CONTEXT_ps_4_0"
			  ,"VR_MULTIOTF_ps_4_0"
			  ,"VR_MULTIOTF_CONTEXT_ps_4_0"
			  ,"VR_MASKVIS_ps_4_0"
			  ,"VR_SCULPTMASK_ps_4_0"
			  ,"VR_SCULPTMASK_CONTEXT_ps_4_0"
			  ,"VR_SURFACE_ps_4_0"
		};

		for (int i = 0; i < PS_NUM; i++)
		{
			string strName = strNames_PS[i];

			FILE* pFile;
			if (fopen_s(&pFile, (prefix_path + strName).c_str(), "rb") == 0)
			{
				fseek(pFile, 0, SEEK_END);
				ullong ullFileSize = ftell(pFile);
				fseek(pFile, 0, SEEK_SET);
				byte* pyRead = new byte[ullFileSize];
				fread(pyRead, sizeof(byte), ullFileSize, pFile);
				fclose(pFile);

				ID3D11PixelShader* dx11PShader = NULL;
				if (dx11CommonParams->dx11Device->CreatePixelShader(pyRead, ullFileSize, NULL, &dx11PShader) != S_OK)
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
#define CS_NUM 27
#define SET_CS(NAME) dx11CommonParams->safe_set_res(grd_helper::COMRES_INDICATOR(GpuhelperResType::COMPUTE_SHADER, NAME), dx11CShader, true)

		string strNames_CS[CS_NUM] = {
			   "VR_RAYMAX_cs_5_0"
			  ,"VR_RAYMIN_cs_5_0"
			  ,"VR_RAYSUM_cs_5_0"
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
			  ,"VR_DEFAULT_DKBZ_cs_5_0"
			  ,"VR_OPAQUE_DKBZ_cs_5_0"
			  ,"VR_CONTEXT_DKBZ_cs_5_0"
			  ,"VR_DEFAULT_DFB_cs_5_0"
			  ,"VR_OPAQUE_DFB_cs_5_0"
			  ,"VR_CONTEXT_DFB_cs_5_0"
			  ,"VR_SURFACE_cs_5_0"
			  ,"FillDither_cs_5_0"
			  ,"SampleTest_cs_5_0"
		};

		for (int i = 0; i < CS_NUM; i++)
		{
			string strName = strNames_CS[i];

			FILE* pFile;
			if (fopen_s(&pFile, (prefix_path + strName).c_str(), "rb") == 0)
			{
				fseek(pFile, 0, SEEK_END);
				ullong ullFileSize = ftell(pFile);
				fseek(pFile, 0, SEEK_SET);
				byte* pyRead = new byte[ullFileSize];
				fread(pyRead, sizeof(byte), ullFileSize, pFile);
				fclose(pFile);

				ID3D11ComputeShader* dx11CShader = NULL;
				if (dx11CommonParams->dx11Device->CreateComputeShader(pyRead, ullFileSize, NULL, &dx11CShader) != S_OK)
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

	__ID3D11Device* pdx11Device = dx11CommonParams->dx11Device;
	__ID3D11DeviceContext* dx11DeviceImmContext = dx11CommonParams->dx11DeviceImmContext;

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
		gpu_manager->ReleaseGpuResourcesBySrcID(iobj->GetObjectID());	// System Out 포함 //
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
		uint* __count = (uint*)mappedResSysTest.pData;
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
	const uint rtbind = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	
	GpuRes gres_fb_rgba, gres_fb_depthcs;
	grd_helper::UpdateFrameBuffer(gres_fb_rgba, iobj, "RENDER_OUT_RGBA_1", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
	grd_helper::UpdateFrameBuffer(gres_fb_depthcs, iobj, "RENDER_OUT_DEPTH_1", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R32_FLOAT, 0);
	
	GpuRes gres_fb_vrdepthcs, gres_fb_vrenc;
	grd_helper::UpdateFrameBuffer(gres_fb_vrdepthcs, iobj, "RENDER_OUT_DEPTH_2", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R32_FLOAT, 0);
	grd_helper::UpdateFrameBuffer(gres_fb_vrenc, iobj, "RENDER_OUT_VRENC", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R8_UINT, 0);

	GpuRes gres_fb_rgba_prev, gres_fb_depthcs_prev;
	grd_helper::UpdateFrameBuffer(gres_fb_rgba_prev, iobj, "RENDER_OUT_RGBA_0", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
	grd_helper::UpdateFrameBuffer(gres_fb_depthcs_prev, iobj, "RENDER_OUT_DEPTH_0", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R32_FLOAT, 0);
#else
	const uint rtbind = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

	GpuRes gres_fb_rgba, gres_fb_depthcs, gres_fb_vrdepthcs;
	grd_helper::UpdateFrameBuffer(gres_fb_rgba, iobj, "RENDER_OUT_RGBA_0", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
	grd_helper::UpdateFrameBuffer(gres_fb_depthcs, iobj, "RENDER_OUT_DEPTH_0", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R32_FLOAT, 0);
	grd_helper::UpdateFrameBuffer(gres_fb_vrdepthcs, iobj, "RENDER_OUT_DEPTH_1", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R32_FLOAT, 0);

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
	uint flt_max_u = *(uint*)&flt_max_;
	uint clr_unit4[4] = { 0, 0, 0, 0 };
	uint clr_max_ufloat_4[4] = { flt_max_u, flt_max_u, flt_max_u, flt_max_u };
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
		// note that gres_fb_vrdepthcs is supposed to be initialized in VR_SURFACE
#endif
		count_call_render = 0;
	}

	ID3D11Buffer* cbuf_cam_state = dx11CommonParams->get_cbuf("CB_CameraState");
	ID3D11Buffer* cbuf_env_state = dx11CommonParams->get_cbuf("CB_EnvState");
	ID3D11Buffer* cbuf_clip = dx11CommonParams->get_cbuf("CB_ClipInfo");
	ID3D11Buffer* cbuf_pobj = dx11CommonParams->get_cbuf("CB_PolygonObject");
	ID3D11Buffer* cbuf_vobj = dx11CommonParams->get_cbuf("CB_VolumeObject");
	ID3D11Buffer* cbuf_reffect = dx11CommonParams->get_cbuf("CB_Material");
	ID3D11Buffer* cbuf_vreffect = dx11CommonParams->get_cbuf("CB_VolumeMaterial");
	ID3D11Buffer* cbuf_tmap = dx11CommonParams->get_cbuf("CB_TMAP");
	ID3D11Buffer* cbuf_hsmask = dx11CommonParams->get_cbuf("CB_HotspotMask");
	ID3D11Buffer* cbuf_testBuffer = dx11CommonParams->get_cbuf("CB_TestBuffer");

#pragma region // HLSL Sampler Setting
	ID3D11SamplerState* sampler_PZ = dx11CommonParams->get_sampler("POINT_ZEROBORDER");
	ID3D11SamplerState* sampler_LZ = dx11CommonParams->get_sampler("LINEAR_ZEROBORDER");
	ID3D11SamplerState* sampler_PC = dx11CommonParams->get_sampler("POINT_CLAMP");
	ID3D11SamplerState* sampler_LC = dx11CommonParams->get_sampler("LINEAR_CLAMP");

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


		ID3D11InputLayout* dx11LI_P = (ID3D11InputLayout*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "P"));
		ID3D11VertexShader* dx11VShader_P = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_P_vs_4_0"));

		ID3D11Buffer* dx11BufferTargetPrim = (ID3D11Buffer*)gres_quad.alloc_res_ptrs[DTYPE_RES];
		//ID3D11Buffer* dx11IndiceTargetPrim = NULL;
		uint stride_inputlayer = sizeof(vmfloat3);
		uint offset = 0;
		dx11DeviceImmContext->IASetVertexBuffers(0, 1, (ID3D11Buffer**)&dx11BufferTargetPrim, &stride_inputlayer, &offset);
		dx11DeviceImmContext->IASetInputLayout(dx11LI_P);
		dx11DeviceImmContext->VSSetShader(dx11VShader_P, NULL, 0);
		dx11DeviceImmContext->GSSetShader(NULL, NULL, 0);
		dx11DeviceImmContext->PSSetShader(NULL, NULL, 0);
		dx11DeviceImmContext->RSSetState(dx11CommonParams->get_rasterizer("SOLID_NONE"));
		dx11DeviceImmContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		dx11DeviceImmContext->OMSetDepthStencilState(dx11CommonParams->get_depthstencil("ALWAYS"), 0);
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
	dx11ViewPort.MaxDepth = 1.0f;
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
// 	uint num_grid_x = (uint)ceil(fb_size_cur.x / (float)__BLOCKSIZE);
// 	uint num_grid_y = (uint)ceil(fb_size_cur.y / (float)__BLOCKSIZE);
	const int __BLOCKSIZE = _fncontainer->fnParams.GetParam("_int_GpuThreadBlockSize", (int)8);
	uint num_grid_x = __BLOCKSIZE == 1 ? fb_size_cur.x : (uint)ceil(fb_size_cur.x / (float)__BLOCKSIZE);
	uint num_grid_y = __BLOCKSIZE == 1 ? fb_size_cur.y : (uint)ceil(fb_size_cur.y / (float)__BLOCKSIZE);

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

	CB_CameraState cbCamState;
	grd_helper::SetCb_Camera(cbCamState, matWS2SS, matSS2WS, matWS2CS, cam_obj, fb_size_cur, k_value, v_thickness <= 0? min_pitch : (float)v_thickness);
	cbCamState.iSrCamDummy__0 = *(uint*)&merging_beta;
	if (fastRender2x) cbCamState.cam_flag |= 0x1 << 8; // 9th bit set
	int oulineiRGB = (int)(outline_color.r * 255.f) | (int)(outline_color.g * 255.f) << 8 | (int)(outline_color.b * 255.f) << 16;
	outline_thickness = min(32, outline_thickness);
	cbCamState.iSrCamDummy__1 = oulineiRGB | outline_thickness << 24;
	//cbCamState.iSrCamDummy__2 = *(uint*)&scale_z_res;
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

	dx11CommonParams->GpuProfile("VR Begin");

	// Initial Setting of Frame Buffers //
	bool is_performed_ssao = false;

	int vr_render_count = 0;
	for (VmActor* actor : dvr_volumes)
	{
		VmVObjectVolume* vobj = (VmVObjectVolume*)actor->GetGeometryRes();
		VolumeData* vol_data = vobj->GetVolumeData();

		// note that the actor is visible (already checked)
#pragma region Actor Parameters
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
#define __SRV_PTR ID3D11ShaderResourceView* 

		// will change integer to string type 
		// and its param name 'RaySamplerMode'
		// also its corresponding cpu renderer
		bool is_xray_mode = false;
		bool is_modulation_mode = false;
		vmfloat2 grad_minmax(FLT_MAX, -FLT_MAX);
		switch (ray_cast_type) {
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
			break;
		case __RM_DEFAULT:
		case __RM_MULTIOTF:
		case __RM_VISVOLMASK:
		case __RM_SCULPTMASK:
		case __RM_OPAQUE:
		case __RM_OPAQUE_MULTIOTF:
		default: break;
		}

		bool skip_volblk_update = actor->GetParam("_bool_ForceToSkipBlockUpdate", false);
		int blk_level = actor->GetParam("_int_BlockLevel", (int)1);
		blk_level = max(min(1, blk_level), 0);

		bool use_mask_volume = actor->GetParam("_bool_ValidateMaskVolume", false);
		int sculpt_index = actor->GetParam("_int_SculptingIndex", (int)-1);
		if (ray_cast_type != __RM_SCULPTMASK && ray_cast_type != __RM_SCULPTMASK_MODULATION)
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
				
				//printf("######111 %f초\n", (double)(clock() - __start) / CLOCKS_PER_SEC);
			}
		}
		else if (ray_cast_type == __RM_MULTIOTF || ray_cast_type == __RM_VISVOLMASK) {
			ray_cast_type = __RM_DEFAULT;
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
		//clock_t __start2 = clock();
		grd_helper::UpdateTMapBuffer(gres_tmap_otf, tobj_otf, false);
		grd_helper::UpdateTMapBuffer(gres_tmap_preintotf, tobj_otf, true);
		//printf("######222 %f초\n", (double)(clock() - __start2) / CLOCKS_PER_SEC);
		SET_SHADER_RES(3, 1, (__SRV_PTR*)&gres_tmap_otf.alloc_res_ptrs[DTYPE_SRV]);
		SET_SHADER_RES(13, 1, (__SRV_PTR*)&gres_tmap_preintotf.alloc_res_ptrs[DTYPE_SRV]);

		VolumeBlocks* vol_blk = vobj->GetVolumeBlock(blk_level);
		if (vol_blk == NULL)
		{
			vobj->UpdateVolumeMinMaxBlocks();
			//vol_blk = vobj->GetVolumeBlock(blk_level);
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
			//clock_t __start3 = clock();
			grd_helper::UpdateOtfBlocks(gres_volblk_otf, vobj, mask_vol_obj, tobj_otf, sculpt_index); // this tagged mask volume is always used even when MIP mode
			volblk_srv = (__SRV_PTR)gres_volblk_otf.alloc_res_ptrs[DTYPE_SRV];
			//printf("######333 %f초\n", (double)(clock() - __start3) / CLOCKS_PER_SEC);
		}
		SET_SHADER_RES(1, 1, (__SRV_PTR*)&volblk_srv);

		CB_VolumeObject cbVolumeObj;
		//vmint3 vol_sampled_size = vmint3(gres_vol.res_values.GetParam("WIDTH", (uint)0),
		//	gres_vol.res_values.GetParam("HEIGHT", (uint)0),
		//	gres_vol.res_values.GetParam("DEPTH", (uint)0));
		//if ( && samplePrecisionLevel > 0)
		//high_samplerate ? 2.f : 1.f
		grd_helper::SetCb_VolumeObj(cbVolumeObj, vobj, actor->matWS2OS, gres_vol, tmap_data->valid_min_idx.x, gres_volblk.options["FORMAT"] == DXGI_FORMAT_R16_UNORM ? 65535.f : 255.f, samplePrecisionLevel, is_xray_mode, sculpt_index);
		if (is_modulation_mode && ((uint)vol_data->vol_size.x * (uint)vol_data->vol_size.y * (uint)vol_data->vol_size.z > 1000000)) {
			//cbVolumeObj.opacity_correction *= 2.f;
			//cbVolumeObj.sample_dist *= 2.f;
			//cbVolumeObj.vec_grad_x *= 2.f;
			//cbVolumeObj.vec_grad_y *= 2.f;
			//cbVolumeObj.vec_grad_z *= 2.f;
		}
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
		if (mask_vol_obj) {
			cbVolumeObj.mask_vol_size = vmfloat3(gres_mask_vol.res_values.GetParam("WIDTH", (uint)1),
				gres_mask_vol.res_values.GetParam("HEIGHT", (uint)1),
				gres_mask_vol.res_values.GetParam("DEPTH", (uint)1));
			VolumeData* mask_vol_data = mask_vol_obj->GetVolumeData();
			if (mask_vol_data->store_dtype.type_bytes == data_type::dtype<byte>().type_bytes) // char
				cbVolumeObj.mask_value_range = 255.f;
			else if (mask_vol_data->store_dtype.type_bytes == data_type::dtype<ushort>().type_bytes) // short
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
		case __RM_SCULPTMASK: pshader = GETPS(VR_SCULPTMASK_ps_4_0); break;
		case __RM_SCULPTMASK_MODULATION: pshader = GETPS(VR_SCULPTMASK_CONTEXT_ps_4_0); break;
		case __RM_DEFAULT:
		default: pshader = GETPS(VR_DEFAULT_ps_4_0); break;
		}
#else
		ID3D11ComputeShader* cshader = NULL;
		int final_ray_cast_type = ray_cast_type;
		if (dvr_volumes.size() > 1 && vr_render_count < dvr_volumes.size() - 1)
		{
			final_ray_cast_type = __RM_OPAQUE;
		}
		switch (final_ray_cast_type)
		{
		case __RM_RAYMIN: cshader = GETCS(VR_RAYMIN_cs_5_0); break;
		case __RM_RAYMAX: cshader = GETCS(VR_RAYMAX_cs_5_0); break;
		case __RM_RAYSUM: cshader = GETCS(VR_RAYSUM_cs_5_0); break;
		case __RM_CLIPOPAQUE:
		case __RM_OPAQUE: 
			switch (mode_OIT)
			{
				// VR_OPAQUE_DFB_cs_5_0 is same as VR_OPAQUE_DKB_cs_5_0
			case MFR_MODE::STATIC_KB:
			case MFR_MODE::DYNAMIC_FB:
				cshader = apply_fragmerge ? GETCS(VR_OPAQUE_FM_cs_5_0) : GETCS(VR_OPAQUE_cs_5_0); break;
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
				cshader = GETCS(VR_OPAQUE_MULTIOTF_FM_cs_5_0); break;
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
				cshader = apply_fragmerge ? GETCS(VR_CONTEXT_FM_cs_5_0) : GETCS(VR_CONTEXT_cs_5_0); break;
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
				cshader = apply_fragmerge ? GETCS(VR_MULTIOTF_FM_cs_5_0) : GETCS(VR_MULTIOTF_cs_5_0); break;
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
				cshader = GETCS(VR_MULTIOTF_CONTEXT_FM_cs_5_0); break;
			case MFR_MODE::DYNAMIC_KB:
				assert(0); break;
			default:
				VMERRORMESSAGE("DOES NOT SUPPORT!!");
			}
			break;
		case __RM_VISVOLMASK:
			cshader = apply_fragmerge ? GETCS(VR_MASKVIS_FM_cs_5_0) : GETCS(VR_MASKVIS_cs_5_0); break;
			break;
		case __RM_SCULPTMASK:
			cshader = GETCS(VR_SCULPTMASK_FM_cs_5_0); break;
			break;
		case __RM_SCULPTMASK_MODULATION:
			cshader = GETCS(VR_SCULPTMASK_CONTEXT_FM_cs_5_0); break;
			break;
		case __RM_SAMPLETEST:
			cshader = GETCS(SampleTest_cs_5_0); break;
		case __RM_DEFAULT:
		default:
			switch (mode_OIT)
			{
			case MFR_MODE::STATIC_KB:
			case MFR_MODE::DYNAMIC_FB:
				cshader = apply_fragmerge ? GETCS(VR_DEFAULT_FM_cs_5_0) : GETCS(VR_DEFAULT_cs_5_0); break;
			case MFR_MODE::DYNAMIC_KB:
				cshader = apply_fragmerge ? GETCS(VR_DEFAULT_DKBZ_cs_5_0) : GETCS(VR_DEFAULT_DFB_cs_5_0); break;
			default:
				VMERRORMESSAGE("DOES NOT SUPPORT!!");
			}
			break;
		}
 
		ID3D11UnorderedAccessView* dx11UAVs[4] = {
				  (ID3D11UnorderedAccessView*)gres_fb_counter.alloc_res_ptrs[DTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_fb_k_buffer.alloc_res_ptrs[DTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_UAV]
		};
		dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 4, dx11UAVs, (UINT*)(&dx11UAVs));

		SET_SHADER_RES(50, 1, (ID3D11ShaderResourceView**)&gres_fb_ref_pidx.alloc_res_ptrs[DTYPE_SRV]); // search why this does not work
#endif

		if(ray_cast_type != __RM_RAYMIN
			&& ray_cast_type != __RM_RAYMAX
			&& ray_cast_type != __RM_RAYSUM) {

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
			ComputeSSAO(dx11DeviceImmContext, dx11CommonParams, iobj, num_grid_x, num_grid_y,
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
		SET_SHADER(cshader, NULL, 0);
		//dx11DeviceImmContext->Flush();
		dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);
		if (fastRender2x) {
			SET_SHADER(GETCS(FillDither_cs_5_0), NULL, 0);
			dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);
		}

		dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 4, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
		SET_SHADER_RES(6, 1, dx11SRVs_NULL);
		dx11DeviceImmContext->CSSetUnorderedAccessViews(50, 1, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
#endif

#ifdef __DX_DEBUG_QUERY
		dx11CommonParams->debug_info_queue->PushEmptyStorageFilter();
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
		ComputeSSAO(dx11DeviceImmContext, dx11CommonParams, iobj, num_grid_x, num_grid_y,
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

	iobj->SetObjParam("_int_NumCallRenders", count_call_render);

	//dx11DeviceImmContext->Flush();
	//printf("# Textures : %d, # Drawing : %d, # RTBuffer Change : %d, # Merging : %d\n", iNumTexureLayers, iCountRendering, iCountRTBuffers, iCountMerging);
	dx11CommonParams->GpuProfile("VR Begin", true);

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
	//printf("###################### %f초\n", duration);

	return true;
}