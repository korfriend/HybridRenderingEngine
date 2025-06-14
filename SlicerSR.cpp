#include "RendererHeader.h"
#include "vzm2/Backlog.h"

#include "hlsl/ShaderInterop_BVH.h"

bool RenderSrSlicer(VmFnContainer* _fncontainer,
	VmGpuManager* gpu_manager,
	grd_helper::GpuDX11CommonParameters* dx11CommonParams,
	LocalProgress* progress,
	double* run_time_ptr)
{
	// https://atyuwen.github.io/posts/antialiased-line/


	using namespace std::chrono;
	//((std::mutex*)HDx11GetMutexGpuCriticalPath())->lock();

#pragma region // Parameter Setting //
	VmIObject* iobj = _fncontainer->fnParams.GetParam("_VmIObject*_RenderOut", (VmIObject*)NULL);

	int k_value_old = iobj->GetObjParam("_int_NumK", (int)K_NUM_SLICER);
	int k_value = _fncontainer->fnParams.GetParam("_int_NumK", k_value_old);
	iobj->SetObjParam("_int_NumK", k_value);

	int num_moments_old = iobj->GetObjParam("_int_NumQueueLayers", (int)8);
	int num_moments = _fncontainer->fnParams.GetParam("_int_NumQueueLayers", num_moments_old);
	int num_safe_loopexit = _fncontainer->fnParams.GetParam("_int_SpinLockSafeLoops", (int)10000000);
	bool is_final_renderer = _fncontainer->fnParams.GetParam("_bool_IsFinalRenderer", true);
	//double v_discont_depth = _fncontainer->fnParams.GetParam("_float_DiscontDepth", -1.0);
	float merging_beta = _fncontainer->fnParams.GetParam("_float_MergingBeta", 0.5f);
	bool blur_SSAO = _fncontainer->fnParams.GetParam("_bool_BlurSSAO", true);


	int camClipMode = _fncontainer->fnParams.GetParam("_int_ClippingMode", (int)0);
	vmfloat3 camClipPlanePos = _fncontainer->fnParams.GetParam("_float3_PosClipPlaneWS", vmfloat3(0));
	vmfloat3 camClipPlaneDir = _fncontainer->fnParams.GetParam("_float3_VecClipPlaneWS", vmfloat3(0));
	vmmat44f camClipMatWS2BS = _fncontainer->fnParams.GetParam("_matrix44f_MatrixClipWS2BS", vmmat44f(1));
	std::set<int> camClipperFreeActors = _fncontainer->fnParams.GetParam("_set_int_CamClipperFreeActors", std::set<int>());

	bool is_picking_routine = _fncontainer->fnParams.GetParam("_bool_IsPickingRoutine", false);
//#ifdef DX10_0
//	is_picking_routine = false;
//#endif
	vmint2 picking_pos_ss = _fncontainer->fnParams.GetParam("_int2_PickingPosSS", vmint2(-1, -1));

	int buf_ex_scale = _fncontainer->fnParams.GetParam("_int_BufExScale", (int)8); // scaling the capacity of the k-buffer for _bool_PixelTransmittance
	bool use_blending_option_MomentOIT = _fncontainer->fnParams.GetParam("_bool_UseBlendingOptionMomentOIT", false);
	bool check_pixel_transmittance = _fncontainer->fnParams.GetParam("_bool_PixelTransmittance", false);
	//vr_level = 2;
	vmfloat4 default_phong_lighting_coeff = vmfloat4(0.2, 1.0, 0.5, 5); // Emission, Diffusion, Specular, Specular Power

	float default_point_thickness = _fncontainer->fnParams.GetParam("_float_PointThickness", 3.0f);
	float default_surfel_size = _fncontainer->fnParams.GetParam("_float_SurfelSize", 0.0f);
	float default_line_thickness = _fncontainer->fnParams.GetParam("_float_LineThickness", 2.0f);
	vmfloat3 default_color_cmmobj = _fncontainer->fnParams.GetParam("_float3_CmmGlobalColor", vmfloat3(-1, -1, -1));
	bool use_spinlock_pixsynch = _fncontainer->fnParams.GetParam("_bool_UseSpinLock", false);
	bool is_ghost_mode = _fncontainer->fnParams.GetParam("_bool_GhostEffect", false);
	bool is_rgba = _fncontainer->fnParams.GetParam("_bool_IsRGBA", false); // false means bgra
	bool isDrawingOnlyContours = _fncontainer->fnParams.GetParam("_bool_DrawingOnlyContours", false);

	// note planeThickness is defined in WS
	float planeThickness = _fncontainer->fnParams.GetParam("_float_PlaneThickness", 0.f);

	bool is_render_out = false;
	// note : planeThickness == 0 calls CPU renderer which uses render-out buffer
	if (is_final_renderer || (planeThickness <= 0.f && !is_final_renderer)) is_render_out = true;
	is_render_out = false;

	bool only_surface_test = _fncontainer->fnParams.GetParam("_bool_OnlySurfaceTest", false);
	bool test_consoleout = _fncontainer->fnParams.GetParam("_bool_TestConsoleOut", false);
	bool test_fps_profiling = _fncontainer->fnParams.GetParam("_bool_TestFpsProfile", false);
	auto test_out = [&test_consoleout](const string& _message)
	{
		if (test_consoleout)
			cout << _message << endl;
	};

	float sample_rate = _fncontainer->fnParams.GetParam("_float_UserSampleRate", 0.0f);
	if (sample_rate <= 0) sample_rate = 1.0f;
	bool apply_samplerate2gradient = _fncontainer->fnParams.GetParam("_bool_ApplySampleRateToGradient", false);
	bool reload_hlsl_objs = _fncontainer->fnParams.GetParam("_bool_ReloadHLSLObjFiles", false);

	int __BLOCKSIZE = _fncontainer->fnParams.GetParam("_int_GpuThreadBlockSize", (int)4);
	float v_thickness = _fncontainer->fnParams.GetParam("_float_VZThickness", 0.0f);
	float gi_v_thickness = _fncontainer->fnParams.GetParam("_float_GIVZThickness", v_thickness);
	float scale_z_res = _fncontainer->fnParams.GetParam("_float_zResScale", 1.0f);

	int i_test_shader = (int)_fncontainer->fnParams.GetParam("_int_ShaderTest", (int)0);

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

#pragma region // Shader Setting
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
		//hlslobj_path += "..\\..\\VmModuleProjects\\hybrid_rendering_engine\\";
		hlslobj_path += "..\\..\\VmProjects\\hybrid_rendering_engine\\";

		string enginePath;
		if (grd_helper::GetEnginePath(enginePath)) {
			hlslobj_path = enginePath;
		}
		string hlslobj_path_4_0 = hlslobj_path + "shader_compiled_objs_4_0\\";
		//cout << hlslobj_path << endl;

#ifdef DX10_0
		hlslobj_path += "shader_compiled_objs_4_0\\";
#else
		hlslobj_path += "shader_compiled_objs\\";
#endif

		string prefix_path = hlslobj_path;
		vmlog::LogInfo("RELOAD HLSL _ SR slicer renderer");

		dx11CommonParams->dx11DeviceImmContext->VSSetShader(NULL, NULL, 0);
		dx11CommonParams->dx11DeviceImmContext->GSSetShader(NULL, NULL, 0);
		dx11CommonParams->dx11DeviceImmContext->PSSetShader(NULL, NULL, 0);
		dx11CommonParams->dx11DeviceImmContext->CSSetShader(NULL, NULL, 0);

#define VS_NUM 5
#define GS_NUM 1
#ifdef DX10_0
#define PS_NUM 5
#define SET_PS(NAME, __S) dx11CommonParams->safe_set_res(grd_helper::COMRES_INDICATOR(GpuhelperResType::PIXEL_SHADER, NAME), __S, true)
#else
#define CS_NUM 10
#define SET_CS(NAME, __S) dx11CommonParams->safe_set_res(grd_helper::COMRES_INDICATOR(GpuhelperResType::COMPUTE_SHADER, NAME), __S, true)
#endif
#define SET_VS(NAME, __S) dx11CommonParams->safe_set_res(grd_helper::COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, NAME), __S, true)
#define SET_GS(NAME, __S) dx11CommonParams->safe_set_res(grd_helper::COMRES_INDICATOR(GpuhelperResType::GEOMETRY_SHADER, NAME), __S, true)
#define GETRASTER(NAME) dx11CommonParams->get_rasterizer(#NAME)
#define GETDEPTHSTENTIL(NAME) dx11CommonParams->get_depthstencil(#NAME)

#ifdef DX10_0
		string strNames_VS[VS_NUM] = {
			   "SR_OIT_P_vs_4_0"
			  ,"SR_OIT_PN_vs_4_0"
			  ,"SR_OIT_PT_vs_4_0"
			  ,"SR_OIT_PNT_vs_4_0"
			  ,"SR_OIT_PTTT_vs_4_0"
		};
#else
		string strNames_VS[VS_NUM] = {
			   "SR_OIT_P_vs_5_0"
			  ,"SR_OIT_PN_vs_5_0"
			  ,"SR_OIT_PT_vs_5_0"
			  ,"SR_OIT_PNT_vs_5_0"
			  ,"SR_OIT_PTTT_vs_5_0"
		};
#endif

		for (int i = 0; i < VS_NUM; i++)
		{
			string strName = strNames_VS[i];

			FILE* pFile;
			if (fopen_s(&pFile, (prefix_path + strName).c_str(), "rb") == 0)
			{
				fseek(pFile, 0, SEEK_END);
				ullong ullFileSize = ftell(pFile);
				fseek(pFile, 0, SEEK_SET);
				byte* pyRead = new byte[ullFileSize];
				fread(pyRead, sizeof(byte), ullFileSize, pFile);
				fclose(pFile);

				ID3D11VertexShader* dx11VShader = NULL;
				if (dx11CommonParams->dx11Device->CreateVertexShader(pyRead, ullFileSize, NULL, &dx11VShader) != S_OK)
				{
					VMERRORMESSAGE("SHADER COMPILE FAILURE!");
				}
				else
				{
					SET_VS(strName, dx11VShader);
				}
				VMSAFE_DELETEARRAY(pyRead);
			}
		}
		/**/

		string strNames_GS[GS_NUM] = {
			   "GS_MeshCutLines_gs_4_0"
		};

		for (int i = 0; i < GS_NUM; i++)
		{
			string strName = strNames_GS[i];

			FILE* pFile;
			if (fopen_s(&pFile, (prefix_path + strName).c_str(), "rb") == 0)
			{
				fseek(pFile, 0, SEEK_END);
				ullong ullFileSize = ftell(pFile);
				fseek(pFile, 0, SEEK_SET);
				byte* pyRead = new byte[ullFileSize];
				fread(pyRead, sizeof(byte), ullFileSize, pFile);
				fclose(pFile);

				ID3D11GeometryShader* dx11GShader = NULL;
				if (dx11CommonParams->dx11Device->CreateGeometryShader(pyRead, ullFileSize, NULL, &dx11GShader) != S_OK)
				{
					VMERRORMESSAGE("SHADER COMPILE FAILURE!");
				}
				else
				{
					SET_GS(strName, dx11GShader);
				}
				VMSAFE_DELETEARRAY(pyRead);
			}
		}

#ifdef DX10_0
		string strNames_PS[PS_NUM] = {
			   "ThickSlicePathTracer_ps_4_0"
			  ,"CurvedThickSlicePathTracer_ps_4_0"
			  ,"PickingThickSlice_ps_4_0"
			  ,"PickingCurvedThickSlice_ps_4_0"
			  ,"SliceOutline_ps_4_0"
		};
		for (int i = 0; i < PS_NUM; i++)
		{
			string strName = strNames_PS[i];
#else
		string strNames_CS[CS_NUM] = {
			   "ThickSlicePathTracer_cs_5_0"
			  ,"CurvedThickSlicePathTracer_cs_5_0"
			  ,"PickingThickSlice_cs_5_0"
			  ,"PickingCurvedThickSlice_cs_5_0"
			  ,"ThickSlicePathTracer_GPUBVH_cs_5_0"
			  ,"CurvedThickSlicePathTracer_GPUBVH_cs_5_0"
			  ,"PickingThickSlice_GPUBVH_cs_5_0"
			  ,"PickingCurvedThickSlice_GPUBVH_cs_5_0"

			  ,"SliceOutline_cs_5_0"
			  ,"OIT_SKBZ_RESOLVE_cs_5_0"
		};
		for (int i = 0; i < CS_NUM; i++)
		{
			string strName = strNames_CS[i];
#endif

			FILE* pFile;
			if (fopen_s(&pFile, (prefix_path + strName).c_str(), "rb") == 0)
			{
				fseek(pFile, 0, SEEK_END);
				ullong ullFileSize = ftell(pFile);
				fseek(pFile, 0, SEEK_SET);
				byte* pyRead = new byte[ullFileSize];
				fread(pyRead, sizeof(byte), ullFileSize, pFile);
				fclose(pFile);

#ifdef DX10_0
				ID3D11PixelShader* dx11PShader = NULL;
				if (dx11CommonParams->dx11Device->CreatePixelShader(pyRead, ullFileSize, NULL, &dx11PShader) != S_OK)
				{
					VMERRORMESSAGE("SHADER COMPILE FAILURE!");
				}
				else
				{
					SET_PS(strName, dx11PShader);
				}
#else
				ID3D11ComputeShader* dx11CShader = NULL;
				if (dx11CommonParams->dx11Device->CreateComputeShader(pyRead, ullFileSize, NULL, &dx11CShader) != S_OK)
				{
					VMERRORMESSAGE("SHADER COMPILE FAILURE!");
				}
				else
				{
					SET_CS(strName, dx11CShader);
				}
#endif
				VMSAFE_DELETEARRAY(pyRead);
			}
		}

		{
			string strName = "GS_MeshCutLines_gs_4_0";
			FILE* pFile;
			if (fopen_s(&pFile, (hlslobj_path_4_0 + strName).c_str(), "rb") == 0)
			{
				fseek(pFile, 0, SEEK_END);
				ullong ullFileSize = ftell(pFile);
				fseek(pFile, 0, SEEK_SET);
				byte* pyRead = new byte[ullFileSize];
				fread(pyRead, sizeof(byte), ullFileSize, pFile);
				fclose(pFile);

				// https://learn.microsoft.com/en-us/windows/win32/direct3d11/d3d10-graphics-programming-guide-output-stream-stage-getting-started
				// https://strange-cpp.tistory.com/101
				D3D11_SO_DECLARATION_ENTRY pDecl[] =
				{
					// semantic name, semantic index, start component, component count, output slot
					{ 0, "TEXCOORD", 0, 0, 3, 0 },   // output 
				};
				int numEntries = sizeof(pDecl) / sizeof(D3D11_SO_DECLARATION_ENTRY);
				uint bufferStrides[] = { sizeof(vmfloat3) };
				int numStrides = sizeof(bufferStrides) / sizeof(uint);
				ID3D11GeometryShader* dx11GShader = NULL;
				if (dx11CommonParams->dx11Device->CreateGeometryShaderWithStreamOutput(
					pyRead, ullFileSize, pDecl, numEntries, bufferStrides, numStrides, D3D11_SO_NO_RASTERIZED_STREAM, NULL,
					(ID3D11GeometryShader**)&dx11GShader) != S_OK)
				{
					VMERRORMESSAGE("SHADER COMPILE FAILURE!");
				}
				else
				{
					SET_GS(strName, dx11GShader);
				}
				VMSAFE_DELETEARRAY(pyRead);
			}
		}
		/**/
		dx11CommonParams->dx11DeviceImmContext->Flush();
	}

	ID3D11InputLayout* dx11LI_P = (ID3D11InputLayout*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "P"));
	ID3D11InputLayout* dx11LI_PN = (ID3D11InputLayout*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "PN"));
	ID3D11InputLayout* dx11LI_PT = (ID3D11InputLayout*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "PT"));
	ID3D11InputLayout* dx11LI_PNT = (ID3D11InputLayout*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "PNT"));
	ID3D11InputLayout* dx11LI_PTTT = (ID3D11InputLayout*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "PTTT"));

#ifdef DX10_0
	ID3D11VertexShader* dx11VShader_P = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_P_vs_4_0"));
	ID3D11VertexShader* dx11VShader_PN = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PN_vs_4_0"));
	ID3D11VertexShader* dx11VShader_PT = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PT_vs_4_0"));
	ID3D11VertexShader* dx11VShader_PNT = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PNT_vs_4_0"));
	ID3D11VertexShader* dx11VShader_PTTT = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PTTT_vs_4_0"));
#else
	ID3D11VertexShader* dx11VShader_P = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_P_vs_5_0"));
	ID3D11VertexShader* dx11VShader_PN = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PN_vs_5_0"));
	ID3D11VertexShader* dx11VShader_PT = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PT_vs_5_0"));
	ID3D11VertexShader* dx11VShader_PNT = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PNT_vs_5_0"));
	ID3D11VertexShader* dx11VShader_PTTT = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PTTT_vs_5_0"));
#endif

	ID3D11Buffer* cbuf_cam_state = dx11CommonParams->get_cbuf("CB_CameraState");
	ID3D11Buffer* cbuf_env_state = dx11CommonParams->get_cbuf("CB_EnvState");
	ID3D11Buffer* cbuf_clip = dx11CommonParams->get_cbuf("CB_ClipInfo");
	ID3D11Buffer* cbuf_pobj = dx11CommonParams->get_cbuf("CB_PolygonObject");
	ID3D11Buffer* cbuf_vobj = dx11CommonParams->get_cbuf("CB_VolumeObject");
	ID3D11Buffer* cbuf_reffect = dx11CommonParams->get_cbuf("CB_Material");
	ID3D11Buffer* cbuf_tmap = dx11CommonParams->get_cbuf("CB_TMAP");
	ID3D11Buffer* cbuf_hsmask = dx11CommonParams->get_cbuf("CB_HotspotMask");
	ID3D11Buffer* cbuf_curvedslicer = dx11CommonParams->get_cbuf("CB_CurvedSlicer");
#pragma endregion 

#pragma region // IOBJECT CPU
	//while (iobj->GetFrameBuffer(FrameBufferUsageRENDEROUT, 2) != NULL)
	//	iobj->DeleteFrameBuffer(FrameBufferUsageRENDEROUT, 2);
	if (!iobj->ReplaceFrameBuffer(FrameBufferUsageRENDEROUT, 0, data_type::dtype<vmbyte4>(), ("common render out frame buffer : defined in vismtv_inbuilt_renderergpudx module")))
		iobj->InsertFrameBuffer(data_type::dtype<vmbyte4>(), FrameBufferUsageRENDEROUT, ("common render out frame buffer : defined in vismtv_inbuilt_renderergpudx module"));
	if (iobj->GetFrameBuffer(FrameBufferUsageRENDEROUT, 1) == NULL)
		iobj->InsertFrameBuffer(data_type::dtype<vmbyte4>(), FrameBufferUsageRENDEROUT, ("temp render out frame buffer Backup : defined in vismtv_inbuilt_renderergpudx module"));


	//while (iobj->GetFrameBuffer(FrameBufferUsageDEPTH, 1) != NULL)
	//	iobj->DeleteFrameBuffer(FrameBufferUsageDEPTH, 1);
	if (!iobj->ReplaceFrameBuffer(FrameBufferUsageDEPTH, 0, data_type::dtype<float>(), ("1st hit screen depth frame buffer : defined in vismtv_inbuilt_renderergpudx module")))
		iobj->InsertFrameBuffer(data_type::dtype<float>(), FrameBufferUsageDEPTH, ("1st hit screen depth frame buffer : defined in vismtv_inbuilt_renderergpudx module"));
#pragma endregion 

	__ID3D11Device* dx11Device = dx11CommonParams->dx11Device;
	__ID3D11DeviceContext* dx11DeviceImmContext = dx11CommonParams->dx11DeviceImmContext;

#pragma region // IOBJECT GPU
	vmint2 fb_size_cur;
	iobj->GetFrameBufferInfo(&fb_size_cur);
	vmint2 fb_size_old = iobj->GetObjParam("_int2_PreviousScreenSize", vmint2(0, 0));
	if (fb_size_cur.x != fb_size_old.x || fb_size_cur.y != fb_size_old.y
		|| k_value != k_value_old)
	{
		gpu_manager->ReleaseGpuResourcesBySrcID(iobj->GetObjectID());	// System Out 포함 //
		iobj->SetObjParam("_int2_PreviousScreenSize", fb_size_cur);
		iobj->SetObjParam("_int_PreviousBufferEx", (int)1);
	}
	ullong lastest_render_time = iobj->GetObjParam("_ullong_LatestSrTime", (ullong)0);

	GpuRes gres_fb_rgba, gres_fb_depthcs;
	GpuRes gres_fb_sys_rgba, gres_fb_sys_depthcs;

#ifdef DX10_0
	const uint rtbind = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
#define SET_SAMPLERS dx11DeviceImmContext->PSSetSamplers
#define SET_CBUFFERS dx11DeviceImmContext->PSSetConstantBuffers
#define SET_SHADER_RES dx11DeviceImmContext->PSSetShaderResources
#define SET_SHADER dx11DeviceImmContext->PSSetShader
#else
	const uint rtbind = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
#define SET_SAMPLERS dx11DeviceImmContext->CSSetSamplers
#define SET_CBUFFERS dx11DeviceImmContext->CSSetConstantBuffers
#define SET_SHADER_RES dx11DeviceImmContext->CSSetShaderResources
#define SET_SHADER dx11DeviceImmContext->CSSetShader
#endif
	
	// Ghost effect mode
	//GpuRes gres_fb_mask_hotspot;
	grd_helper::UpdateFrameBuffer(gres_fb_rgba, iobj, "RENDER_OUT_RGBA_0", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
	grd_helper::UpdateFrameBuffer(gres_fb_depthcs, iobj, "RENDER_OUT_DEPTH_0", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R32_FLOAT, 0);

#ifdef DX10_0
	GpuRes gres_fb_rgba2, gres_fb_depthcs2;
	grd_helper::UpdateFrameBuffer(gres_fb_rgba2, iobj, "RENDER_OUT_RGBA_1", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
	grd_helper::UpdateFrameBuffer(gres_fb_depthcs2, iobj, "RENDER_OUT_DEPTH_1", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R32_FLOAT, 0);

	GpuRes gres_picking_buffer2; // for pingpong
#else
	GpuRes gres_fb_counter, gres_fb_k_buffer;
	grd_helper::UpdateFrameBuffer(gres_fb_counter, iobj, "RW_COUNTER", RTYPE_TEXTURE2D, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_UINT, 0);
	const int num_frags_perpixel = k_value * 3;
	grd_helper::UpdateFrameBuffer(gres_fb_k_buffer, iobj, "BUFFER_RW_K_BUF", RTYPE_BUFFER, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_TYPELESS, UPFB_RAWBYTE, num_frags_perpixel);
#endif
	const int max_picking_layers = 100;
	GpuRes gres_picking_buffer, gres_picking_system_buffer;
	if (is_picking_routine) {
#ifdef DX10_0
		// those picking layers contain depth (4bytes) and id (4bytes) information
		grd_helper::UpdateFrameBuffer(gres_picking_buffer, iobj, "BUFFER_RW_PICKING_BUF", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R32G32B32A32_FLOAT, UPFB_PICK_TEXTURE);
		grd_helper::UpdateFrameBuffer(gres_picking_buffer2, iobj, "BUFFER_RW_PICKING_BUF_2", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R32G32B32A32_FLOAT, UPFB_PICK_TEXTURE);
		grd_helper::UpdateFrameBuffer(gres_picking_system_buffer, iobj, "SYSTEM_OUT_RW_PICKING_BUF", RTYPE_TEXTURE2D, NULL, DXGI_FORMAT_R32G32B32A32_FLOAT, UPFB_SYSOUT | UPFB_PICK_TEXTURE);
#else
		// those picking layers contain depth (4bytes) and id (4bytes) information
		grd_helper::UpdateFrameBuffer(gres_picking_buffer, iobj, "BUFFER_RW_PICKING_BUF", RTYPE_BUFFER,
			D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_UINT, UPFB_NFPP_BUFFERSIZE, max_picking_layers * 2);
		grd_helper::UpdateFrameBuffer(gres_picking_system_buffer, iobj, "SYSTEM_OUT_RW_PICKING_BUF", RTYPE_BUFFER,
			NULL, DXGI_FORMAT_R32_UINT, UPFB_SYSOUT | UPFB_NFPP_BUFFERSIZE, max_picking_layers * 2);

		static std::vector<uint> clearDataUnit(max_picking_layers * 3, 0);//make sure that this thing is aligned
		dx11CommonParams->dx11DeviceImmContext->UpdateSubresource((ID3D11Buffer*)gres_picking_buffer.alloc_res_ptrs[DTYPE_RES]
			, 0, NULL, &clearDataUnit[0], sizeof(uint) * clearDataUnit.size(), sizeof(uint) * clearDataUnit.size());
#endif
	}

	grd_helper::UpdateFrameBuffer(gres_fb_sys_rgba, iobj, "SYSTEM_OUT_RGBA", RTYPE_TEXTURE2D, NULL, DXGI_FORMAT_R8G8B8A8_UNORM, UPFB_SYSOUT);
	grd_helper::UpdateFrameBuffer(gres_fb_sys_depthcs, iobj, "SYSTEM_OUT_DEPTH", RTYPE_TEXTURE2D, NULL, DXGI_FORMAT_R32_FLOAT, UPFB_SYSOUT);

#pragma endregion 

	uint num_grid_x = __BLOCKSIZE == 1 ? fb_size_cur.x : (uint)ceil(fb_size_cur.x / (float)__BLOCKSIZE);
	uint num_grid_y = __BLOCKSIZE == 1 ? fb_size_cur.y : (uint)ceil(fb_size_cur.y / (float)__BLOCKSIZE);

	float detaultOutlinePixelThickness = 1.5f;
	bool curved_slicer = _fncontainer->fnParams.GetParam("_bool_IsNonlinear", false);
	if (curved_slicer) {
		vector<vmfloat3>& vtrCurveInterpolations = *_fncontainer->fnParams.GetParamPtr<vector<vmfloat3>>("_vlist_FLOAT3_CurveInterpolations");
		vector<vmfloat3>& vtrCurveUpVectors = *_fncontainer->fnParams.GetParamPtr<vector<vmfloat3>>("_vlist_FLOAT3_CurveUpVectors");
		vector<vmfloat3>& vtrCurveTangentVectors = *_fncontainer->fnParams.GetParamPtr<vector<vmfloat3>>("_vlist_FLOAT3_CurveTangentVectors");
		GpuRes gres_cpPoints, gres_cpTangents;
		int num_curvedPoints = (int)vtrCurveInterpolations.size();
		if (num_curvedPoints < 1)
			return false;
		grd_helper::UpdateCustomBuffer(gres_cpPoints, iobj, "CurvedPlanePoints", &vtrCurveInterpolations[0], num_curvedPoints, DXGI_FORMAT_R32G32B32_FLOAT, 12);
		grd_helper::UpdateCustomBuffer(gres_cpTangents, iobj, "CurvedPlaneTangents", &vtrCurveTangentVectors[0], num_curvedPoints, DXGI_FORMAT_R32G32B32_FLOAT, 12);
		SET_SHADER_RES(30, 1, (ID3D11ShaderResourceView**)&gres_cpPoints.alloc_res_ptrs[DTYPE_SRV]);
		SET_SHADER_RES(31, 1, (ID3D11ShaderResourceView**)&gres_cpTangents.alloc_res_ptrs[DTYPE_SRV]);
	
		float sample_dist = 1.f;
		CB_CurvedSlicer cbCurvedSlicer;
		grd_helper::SetCb_CurvedSlicer(cbCurvedSlicer, _fncontainer, iobj, sample_dist);
		D3D11_MAPPED_SUBRESOURCE mappedResCurvedSlicerState;
		dx11DeviceImmContext->Map(cbuf_curvedslicer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResCurvedSlicerState);
		CB_CurvedSlicer* cbCurvedSlicerData = (CB_CurvedSlicer*)mappedResCurvedSlicerState.pData;
		memcpy(cbCurvedSlicerData, &cbCurvedSlicer, sizeof(CB_CurvedSlicer));
		dx11DeviceImmContext->Unmap(cbuf_curvedslicer, 0);
		SET_CBUFFERS(10, 1, &cbuf_curvedslicer);

		planeThickness = cbCurvedSlicer.thicknessPlane;
		detaultOutlinePixelThickness = 1.2f;
	}

	float camForcedOutlinePixelThickness = _fncontainer->fnParams.GetParam("_float_OutlinePixThickness", -1.f);
	
#pragma region // Camera & Light Setting
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

	vmfloat3 picking_ray_origin, picking_ray_dir;
	if (is_picking_routine) {
		if (curved_slicer) {
			float curved_plane_w, curved_plane_h;
			int num_curve_width_pts;
			
			vector<vmfloat3>& curve_pos_pts = *_fncontainer->fnParams.GetParamPtr<vector<vmfloat3>>("_vlist_FLOAT3_CurveInterpolations");
			vector<vmfloat3>& curve_up_pts = *_fncontainer->fnParams.GetParamPtr<vector<vmfloat3>>("_vlist_FLOAT3_CurveUpVectors");
			vector<vmfloat3>& curve_tan_pts = *_fncontainer->fnParams.GetParamPtr<vector<vmfloat3>>("_vlist_FLOAT3_CurveTangentVectors");

			curved_plane_w = _fncontainer->fnParams.GetParam("_float_ExPlaneWidth", 1.f);
			curved_plane_h = _fncontainer->fnParams.GetParam("_float_ExPlaneHeight", 1.f);
			num_curve_width_pts = (int)curve_pos_pts.size();

			vmfloat3 cam_pos_cws, cam_dir_cws, cam_up_cws;
			cam_obj->GetCameraExtStatef(&cam_pos_cws, &cam_dir_cws, &cam_up_cws);
			vmdouble2 ipSize;
			cam_obj->GetCameraIntState(&ipSize, NULL, NULL, NULL);
			vmfloat2 fIpSize = ipSize;

			vmfloat3 cam_right_cws = normalize(cross(cam_dir_cws, cam_up_cws));
			// if view is +z and up is +y, then x dir is left, which means that curve index increases along right to left
			vmfloat3 pos_tl_cws = cam_pos_cws - cam_right_cws * fIpSize.x * 0.5f + cam_up_cws * fIpSize.y * 0.5f;
			vmfloat3 pos_tr_cws = cam_pos_cws + cam_right_cws * fIpSize.x * 0.5f + cam_up_cws * fIpSize.y * 0.5f;
			vmfloat3 pos_bl_cws = cam_pos_cws - cam_right_cws * fIpSize.x * 0.5f - cam_up_cws * fIpSize.y * 0.5f;
			vmfloat3 pos_br_cws = cam_pos_cws + cam_right_cws * fIpSize.x * 0.5f - cam_up_cws * fIpSize.y * 0.5f;
			float curve_plane_pitch = curved_plane_w / (float)num_curve_width_pts;
			float num_curve_height_pts = curved_plane_h / curve_plane_pitch;
			float num_curve_midheight_pts = num_curve_height_pts * 0.5f;
			
			vmmat44f mat_scale;// = scale(vmfloat3(curve_plane_pitch));
			fMatrixScaling(&mat_scale, &vmfloat3(curve_plane_pitch, curve_plane_pitch, curve_plane_pitch));
			vmmat44f mat_translate;// = translate(vmfloat3(-curved_plane_w * 0.5f, -curved_plane_h * 0.5f, -curve_plane_pitch * 0.5f));
			fMatrixTranslation(&mat_translate, &vmfloat3(-curved_plane_w * 0.5f, -curved_plane_h * 0.5f, -curve_plane_pitch * 0.5f));
			vmmat44f mat_cos2cws = mat_scale * mat_translate; //mat_translate * mat_scale;
			vmmat44f mat_cws2cos = inverse(mat_cos2cws);

			vmfloat3 pos_tl_cos;//transformPos(pos_tl_cws, mat_cws2cos);
			fTransformPoint(&pos_tl_cos, &pos_tl_cws, &mat_cws2cos);
			vmfloat3 pos_tr_cos;//transformPos(pos_tr_cws, mat_cws2cos);
			fTransformPoint(&pos_tr_cos, &pos_tr_cws, &mat_cws2cos);
			vmfloat3 pos_bl_cos;//transformPos(pos_bl_cws, mat_cws2cos);
			fTransformPoint(&pos_bl_cos, &pos_bl_cws, &mat_cws2cos);
			vmfloat3 pos_br_cos;//transformPos(pos_br_cws, mat_cws2cos);
			fTransformPoint(&pos_br_cos, &pos_br_cws, &mat_cws2cos);

			vmfloat2 pos_inter_top_cos, pos_inter_bottom_cos, pos_sample_cos;
			float fRatio0 = (float)(((fb_size_cur.x - 1) - picking_pos_ss.x) / (float)(fb_size_cur.x - 1));
			float fRatio1 = (float)(picking_pos_ss.x) / (float)(fb_size_cur.x - 1);
			pos_inter_top_cos.x = fRatio0 * pos_tl_cos.x + fRatio1 * pos_tr_cos.x;
			pos_inter_top_cos.y = fRatio0 * pos_tl_cos.y + fRatio1 * pos_tr_cos.y;
			if (pos_inter_top_cos.x >= 0 && pos_inter_top_cos.x < (float)(num_curve_width_pts - 1)) {
				int x_sample_cos = (int)floor(pos_inter_top_cos.x);
				float x_ratio = pos_inter_top_cos.x - (float)x_sample_cos;
				int addr_minmix_x = min(max(x_sample_cos, 0), num_curve_width_pts - 1);
				int addr_minmax_nextx = min(max(x_sample_cos + 1, 0), num_curve_width_pts - 1);
				vmfloat3 pos_sample_ws_c0 = curve_pos_pts[addr_minmix_x];
				vmfloat3 pos_sample_ws_c1 = curve_pos_pts[addr_minmax_nextx];
				vmfloat3 pos_sample_ws_c = pos_sample_ws_c0 * (1.f - x_ratio) + pos_sample_ws_c1 * x_ratio;

				vmfloat3 up_sample_ws_c0 = curve_up_pts[addr_minmix_x];
				vmfloat3 up_sample_ws_c1 = curve_up_pts[addr_minmax_nextx];
				vmfloat3 up_sample_ws_c = up_sample_ws_c0 * (1.f - x_ratio) + up_sample_ws_c1 * x_ratio;

				vmfloat3 tan_sample_ws_c0 = curve_tan_pts[addr_minmix_x];
				vmfloat3 tan_sample_ws_c1 = curve_tan_pts[addr_minmax_nextx];
				vmfloat3 tan_sample_ws_c = tan_sample_ws_c0 * (1.f - x_ratio) + tan_sample_ws_c1 * x_ratio;

				//picking_ray_dir;// = normalize(cross(up_sample_ws_c, tan_sample_ws_c));
				fCrossDotVector(&picking_ray_dir, &up_sample_ws_c, &tan_sample_ws_c);
				fNormalizeVector(&picking_ray_dir, &picking_ray_dir);

				up_sample_ws_c = normalize(up_sample_ws_c);
				up_sample_ws_c *= curve_plane_pitch;
				pos_inter_bottom_cos.x = fRatio0 * pos_bl_cos.x + fRatio1 * pos_br_cos.x;
				pos_inter_bottom_cos.y = fRatio0 * pos_bl_cos.y + fRatio1 * pos_br_cos.y;

				//================== y
				float y_ratio0 = (float)((fb_size_cur.y - 1) - picking_pos_ss.y) / (float)(fb_size_cur.y - 1);
				float y_ratio1 = (float)(picking_pos_ss.y) / (float)(fb_size_cur.y - 1);
				pos_sample_cos.x = y_ratio0 * pos_inter_top_cos.x + y_ratio1 * pos_inter_bottom_cos.x;
				pos_sample_cos.y = y_ratio0 * pos_inter_top_cos.y + y_ratio1 * pos_inter_bottom_cos.y;
				if (pos_sample_cos.y < 0 || pos_sample_cos.y > num_curve_height_pts)
					return false;
				picking_ray_origin = pos_sample_ws_c + up_sample_ws_c * (pos_sample_cos.y - num_curve_midheight_pts);
				bool bIsRightSide = _fncontainer->fnParams.GetParam("_bool_IsRightSide", false);
				if (bIsRightSide)
					picking_ray_dir *= -1.f;
				float fPlaneThickness = _fncontainer->fnParams.GetParam("_float_PlaneThickness", 0.f);
				picking_ray_origin -= picking_ray_dir * fPlaneThickness * 0.5f;
			}
			else 
				return true;
		}
		else {
			cam_obj->GetCameraExtStatef(&picking_ray_origin, &picking_ray_dir, NULL);
			vmfloat3 pos_picking_ws, pos_picking_ss(picking_pos_ss.x, picking_pos_ss.y, 0);
			vmmath::fTransformPoint(&pos_picking_ws, &pos_picking_ss, &matSS2WS);

			if (cam_obj->IsPerspective()) {
				picking_ray_dir = pos_picking_ws - picking_ray_origin;
				vmmath::fNormalizeVector(&picking_ray_dir, &picking_ray_dir);
			}
			else {
				picking_ray_origin = pos_picking_ws;
			}
		}
	}

	CB_EnvState cbEnvState;
	grd_helper::SetCb_Env(cbEnvState, cam_obj, light_src, global_lighting, lens_effect);
	cbEnvState.num_safe_loopexit = num_safe_loopexit;
	cbEnvState.env_dummy_2 = i_test_shader;
	D3D11_MAPPED_SUBRESOURCE mappedResEnvState;
	dx11DeviceImmContext->Map(cbuf_env_state, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResEnvState);
	CB_EnvState* cbEnvStateData = (CB_EnvState*)mappedResEnvState.pData;
	memcpy(cbEnvStateData, &cbEnvState, sizeof(CB_EnvState));
	dx11DeviceImmContext->Unmap(cbuf_env_state, 0);
	SET_CBUFFERS(7, 1, &cbuf_env_state);

#ifdef DX10_0
	GpuRes gres_quad;
	gres_quad.vm_src_id = 1;
	gres_quad.res_name = string("PROXY_QUAD");
	assert(gpu_manager->UpdateGpuResource(gres_quad));

	vmmat44f matQaudWS2CS;
	vmmath::fMatrixWS2CS(&matQaudWS2CS, &vmfloat3(0, 0, 1), &vmfloat3(0, 1, 0), &vmfloat3(0, 0, -1));
	vmmat44f matQaudCS2PS;
	vmmath::fMatrixOrthogonalCS2PS(&matQaudCS2PS, 2.f, 2.f, 0, 2.f);
	vmmat44f matQaudWS2PS = matQaudWS2CS * matQaudCS2PS;
	vmmat44f matQaudWS2PS_T = TRANSPOSE(matQaudWS2PS);
#else
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
#endif
	//if (is_ghost_mode)
	//{
	//	SET_SHADER_RES(50, 1, (ID3D11ShaderResourceView**)&gres_fb_mask_hotspot.alloc_res_ptrs[DTYPE_SRV]);
	//}

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
#pragma endregion 

#pragma region // Presetting of VmObject
	auto __compute_computespace_screen = [](int& w, int& h, const vmmat44f& matOS2SS, const AaBbMinMax& stAaBbOS)
	{
		vmint2 aaBbMinSS(INT_MAX, INT_MAX), aaBbMaxSS(0, 0);
		vmfloat3 f3PosOrthoBoxesOS[8];
		f3PosOrthoBoxesOS[0] = vmfloat3(stAaBbOS.pos_min.x, stAaBbOS.pos_min.y, stAaBbOS.pos_max.z);
		f3PosOrthoBoxesOS[1] = vmfloat3(stAaBbOS.pos_max.x, stAaBbOS.pos_min.y, stAaBbOS.pos_max.z);
		f3PosOrthoBoxesOS[2] = vmfloat3(stAaBbOS.pos_min.x, stAaBbOS.pos_min.y, stAaBbOS.pos_min.z);
		f3PosOrthoBoxesOS[3] = vmfloat3(stAaBbOS.pos_max.x, stAaBbOS.pos_min.y, stAaBbOS.pos_min.z);
		f3PosOrthoBoxesOS[4] = vmfloat3(stAaBbOS.pos_min.x, stAaBbOS.pos_max.y, stAaBbOS.pos_max.z);
		f3PosOrthoBoxesOS[5] = vmfloat3(stAaBbOS.pos_max.x, stAaBbOS.pos_max.y, stAaBbOS.pos_max.z);
		f3PosOrthoBoxesOS[6] = vmfloat3(stAaBbOS.pos_min.x, stAaBbOS.pos_max.y, stAaBbOS.pos_min.z);
		f3PosOrthoBoxesOS[7] = vmfloat3(stAaBbOS.pos_max.x, stAaBbOS.pos_max.y, stAaBbOS.pos_min.z);
		for (int i = 0; i < 8; i++)
		{
			vmfloat3 f3PosOrthoBoxSS;
			fTransformPoint(&f3PosOrthoBoxSS, &f3PosOrthoBoxesOS[i], &matOS2SS);
			vmint2 pos_xy = vmint2((int)f3PosOrthoBoxSS.x, (int)f3PosOrthoBoxSS.y);
			aaBbMinSS.x = min(aaBbMinSS.x, pos_xy.x);
			aaBbMinSS.y = min(aaBbMinSS.y, pos_xy.y);
			aaBbMaxSS.x = max(aaBbMaxSS.x, pos_xy.x);
			aaBbMaxSS.y = max(aaBbMaxSS.y, pos_xy.y);
		}
		w = aaBbMaxSS.x - aaBbMinSS.x;
		h = aaBbMaxSS.y - aaBbMinSS.y;
	};

	vector<VmActor*> slicer_post_actors;
	vector<VmActor*> slicer_actors;
	int _w_max = 0;
	int _h_max = 0;
	// For Each Primitive //
	for (auto& actorPair : _fncontainer->sceneActors)
	{
		VmActor* actor = get<1>(actorPair);
		VmVObject* geo_obj = actor->GetGeometryRes();
		if (geo_obj == NULL ||
			geo_obj->GetObjectType() != ObjectTypePRIMITIVE ||
			!geo_obj->IsDefined() ||
			!actor->visible || actor->color.a == 0)
			continue;

		VmVObjectPrimitive* pobj = (VmVObjectPrimitive*)geo_obj;
		PrimitiveData* prim_data = pobj->GetPrimitiveData();
		if (prim_data->GetVerticeDefinition("POSITION") == NULL || 
#ifdef DX10_0
			pobj->GetBVHTree() == NULL ||
#else
			!pobj->GetBVH().IsValid() ||
#endif
			prim_data->ptype != PrimitiveTypeTRIANGLE)
			continue;

		//if (is_picking_routine) {
		//	if (prim_data->ptype == vmenums::PrimitiveTypeLINE //|| 
		//		//grd_helper::CollisionCheck(actor->matWS2OS, prim_data->aabb_os, picking_ray_origin, picking_ray_dir) ||
		//		//curved_slicer
		//		)
		//		slicer_actors.push_back(actor);
		//	//std::cout << "###### obb ray intersection : " << actor->actorId << std::endl;
		//	// NOTE THAT is_picking_routine allows only general_oit_routine_objs!!
		//	continue;
		//}

		//vmmat44f matOS2SS = actor->matOS2WS * matWS2SS;
		//int w, h;
		//__compute_computespace_screen(w, h, matOS2SS, prim_data->aabb_os);
		vector<VmActor*>* targetSlicerActors = &slicer_actors;
		if (planeThickness > 0) {
			bool noSlicerFill = actor->GetParam("_bool_DisableSolidFillOnSlicer", false);
			if (noSlicerFill) targetSlicerActors = &slicer_post_actors;
		}

		targetSlicerActors->push_back(actor);
	}

	for (int i = 0; i < (int)slicer_post_actors.size(); i++) {
		slicer_actors.push_back(slicer_post_actors[i]);
	}

	if (dx11CommonParams->gpu_profile)
		cout << "  ** # of slicer actors    : " << slicer_actors.size() << endl;
#pragma endregion 

#pragma region // FrameBuffer Setting
	// Backup Previous Render Target //
	ID3D11RenderTargetView* pdxRTVOld = NULL;
	ID3D11DepthStencilView* pdxDSVOld = NULL;
	dx11DeviceImmContext->OMGetRenderTargets(1, &pdxRTVOld, &pdxDSVOld);

	dx11CommonParams->GpuProfile("Clear for Slicer Render - SR");

	float flt_max_ = FLT_MAX;
	uint flt_max_u = *(uint*)&flt_max_;
	uint clr_unit4[4] = { 0, 0, 0, 0 };
	uint clr_max_ufloat_4[4] = { flt_max_u, flt_max_u, flt_max_u, flt_max_u };
	float clr_float_zero_4[4] = { 0, 0, 0, 0 };
	float clr_float_fltmax_4[4] = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };
	//float clr_float_minus_4[4] = { -1.f, -1.f, -1.f, -1.f };
	if (is_picking_routine) {
#ifdef DX10_0
		dx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gres_picking_buffer.alloc_res_ptrs[DTYPE_RTV], clr_float_zero_4);
		dx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gres_picking_buffer2.alloc_res_ptrs[DTYPE_RTV], clr_float_zero_4);
#else
		dx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gres_picking_buffer.alloc_res_ptrs[DTYPE_UAV], clr_unit4);
#endif
	}
	else
	{
#ifdef DX10_0
		dx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gres_fb_rgba2.alloc_res_ptrs[DTYPE_RTV], clr_float_zero_4);
		dx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gres_fb_depthcs2.alloc_res_ptrs[DTYPE_RTV], planeThickness > 0 ? clr_float_fltmax_4 : clr_float_zero_4);
#else
		dx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gres_fb_counter.alloc_res_ptrs[DTYPE_UAV], clr_unit4);
#endif
		dx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_RTV], clr_float_zero_4);
		dx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_RTV], planeThickness > 0 ? clr_float_fltmax_4 : clr_float_zero_4);

	}
	dx11CommonParams->GpuProfile("Clear for Slicer Render - SR", true);
#pragma endregion 


#pragma region // HLSL Sampler Setting
	ID3D11SamplerState* sampler_PZ = dx11CommonParams->get_sampler("POINT_ZEROBORDER");
	ID3D11SamplerState* sampler_LZ = dx11CommonParams->get_sampler("LINEAR_ZEROBORDER");
	ID3D11SamplerState* sampler_PC = dx11CommonParams->get_sampler("POINT_CLAMP");
	ID3D11SamplerState* sampler_LC = dx11CommonParams->get_sampler("LINEAR_CLAMP");
	ID3D11SamplerState* sampler_PW = dx11CommonParams->get_sampler("POINT_WRAP");
	ID3D11SamplerState* sampler_LW = dx11CommonParams->get_sampler("LINEAR_WRAP");

	SET_SAMPLERS(0, 1, &sampler_LZ);
	SET_SAMPLERS(1, 1, &sampler_PZ);
	SET_SAMPLERS(2, 1, &sampler_LC);
	SET_SAMPLERS(3, 1, &sampler_PC);
	SET_SAMPLERS(4, 1, &sampler_LW);
	SET_SAMPLERS(5, 1, &sampler_PW);
#pragma endregion 

	ID3D11RenderTargetView* dx11RTVsNULL[2] = { NULL, NULL };
	ID3D11UnorderedAccessView* dx11UAVs_NULL[7] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };
	ID3D11ShaderResourceView* dx11SRVs_NULL[7] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };
	// For Each Primitive //
	int count_call_render = 0; 

#define NUM_UAVs 5
	auto PathTracer = [&dx11CommonParams, &dx11DeviceImmContext, &gpu_manager, &_fncontainer, &dx11LI_P, &dx11LI_PN, &dx11LI_PT, &dx11LI_PNT, &dx11LI_PTTT, &dx11VShader_P,
		&dx11VShader_PN, &dx11VShader_PT, &dx11VShader_PNT, &dx11VShader_PTTT, 
#ifdef DX10_0
		&gres_fb_rgba2, &gres_fb_depthcs2, &gres_quad,
#else
		&gres_fb_counter, &gres_fb_k_buffer, 
#endif	
		&gres_picking_buffer, &gres_fb_rgba, &gres_fb_depthcs, 
		&cbuf_cam_state, &cbuf_env_state, &cbuf_clip, &cbuf_pobj, &cbuf_vobj, &cbuf_reffect, &cbuf_tmap, &cbuf_hsmask,
		&num_grid_x, &num_grid_y, &matWS2PS, &matWS2SS, &matSS2WS, 
		&light_src, &default_phong_lighting_coeff, &default_point_thickness, &default_surfel_size, &default_line_thickness, &default_color_cmmobj, &use_spinlock_pixsynch, &use_blending_option_MomentOIT,
		&count_call_render, &progress, &cam_obj, &planeThickness, &detaultOutlinePixelThickness, &camForcedOutlinePixelThickness,
		&camClipMode, &camClipPlanePos, &camClipPlaneDir, &camClipMatWS2BS, &camClipperFreeActors, &isDrawingOnlyContours,
#ifdef DX10_0
		&matQaudWS2PS_T, & gres_picking_buffer2,
#endif
		&clr_float_zero_4, &clr_float_fltmax_4, &dx11RTVsNULL, &dx11UAVs_NULL, &dx11SRVs_NULL
	](vector<VmActor*>& actor_list, const bool curved_slicer, const bool is_ghost_mode, const bool is_picking_routine)
	{

		// main geometry rendering process
		for (VmActor* actor : actor_list)
		{
			VmVObjectPrimitive* pobj = (VmVObjectPrimitive*)actor->GetGeometryRes();
			//assert(pobj->GetBVHTree() != NULL);
			PrimitiveData* prim_data = pobj->GetPrimitiveData();

			// note that the actor is visible (already checked)
#pragma region Actor Parameters
			bool has_texture_img = actor->GetParam("_bool_HasTextureMap", false); 

			vmfloat4 material_phongCoeffs = actor->GetParam("_float4_PhongCoeffs", default_phong_lighting_coeff);
			bool use_vertex_color = actor->GetParam("_bool_UseVertexColor", false) && prim_data->GetVerticeDefinition("TEXCOORD0") != NULL;

#pragma endregion

#pragma region GPU resource updates
			//VmObject* tobj_otf = (VmObject*)actor->GetAssociateRes("OTF");
			//VmObject* tobj_maptable = (VmObject*)actor->GetAssociateRes("MAPTABLE");
			//VmVObjectVolume* vobj = (VmVObjectVolume*)actor->GetAssociateRes("VOLUME");

			CB_Material cbRenderEffect;
			grd_helper::SetCb_RenderingEffect(cbRenderEffect, actor);
			D3D11_MAPPED_SUBRESOURCE mappedResRenderEffect;
			dx11DeviceImmContext->Map(cbuf_reffect, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResRenderEffect);
			CB_Material* cbRenderEffectData = (CB_Material*)mappedResRenderEffect.pData;
			memcpy(cbRenderEffectData, &cbRenderEffect, sizeof(CB_Material));
			dx11DeviceImmContext->Unmap(cbuf_reffect, 0);

			GpuRes gres_vtx, gres_idx;
			map<string, GpuRes> map_gres_texs;
			grd_helper::UpdatePrimitiveModel(gres_vtx, gres_idx, map_gres_texs, pobj);
			int tex_map_enum = 0;
			//bool is_annotation_obj = pobj->GetObjParam("_bool_IsAnnotationObj", false) && prim_data->texture_res_info.size() > 0;
			//if (is_annotation_obj)
			//{
			//	auto it_tex = map_gres_texs.find("MAP_COLOR4");
			//	if (it_tex != map_gres_texs.end())
			//	{
			//		tex_map_enum = 1;
			//		GpuRes& gres_tex = it_tex->second;
			//		SET_SHADER_RES(10, 1, (ID3D11ShaderResourceView**)&gres_tex.alloc_res_ptrs[DTYPE_SRV]);
			//	}
			//}
			//else 
			if (has_texture_img)
			{
				auto it_tex = map_gres_texs.find("MAP_COLOR4");
				if (it_tex != map_gres_texs.end())
				{
					tex_map_enum |= 0x1;
					GpuRes& gres_tex = it_tex->second;
					SET_SHADER_RES(10, 1, (ID3D11ShaderResourceView**)&gres_tex.alloc_res_ptrs[DTYPE_SRV]);
				}

				for (int i = 0; i < NUM_MATERIALS; i++)
				{
					it_tex = map_gres_texs.find(g_materials[i]);
					if (it_tex != map_gres_texs.end())
					{
						tex_map_enum |= (0x1 << (i + 1));
						GpuRes& gres_tex = it_tex->second;
						SET_SHADER_RES(11 + i, 1, (ID3D11ShaderResourceView**)&gres_tex.alloc_res_ptrs[DTYPE_SRV]);
					}
				}
			}

			CB_PolygonObject cbPolygonObj;				
			cbPolygonObj.tex_map_enum = tex_map_enum;
			cbPolygonObj.pobj_dummy_0 = actor->actorId;// pobj->GetObjectID(); // used for picking
			grd_helper::SetCb_PolygonObj(cbPolygonObj, pobj, actor, matWS2SS, matWS2PS, false, use_vertex_color);
#ifdef DX10_0
			vmmat44f pobj_mat_os2ps_T = cbPolygonObj.mat_os2ps;
			cbPolygonObj.mat_os2ps = matQaudWS2PS_T;
#endif
			//cbPolygonObj.Ka *= material_phongCoeffs.x;
			//cbPolygonObj.Kd *= material_phongCoeffs.y;
			//cbPolygonObj.Ks *= material_phongCoeffs.z;
			//cbPolygonObj.Ns *= material_phongCoeffs.w;
			if (default_color_cmmobj.x >= 0 && default_color_cmmobj.y >= 0 && default_color_cmmobj.z >= 0)
				cbPolygonObj.Ka = cbPolygonObj.Kd = cbPolygonObj.Ks = default_color_cmmobj;

			if (actor->GetParam("_bool_ApplyColorOnSlicer", false))
			{
				vmfloat4 color_on_slicer = actor->GetParam("_float4_ColorOnSlicer", vmfloat4(1));
				cbPolygonObj.Ka = cbPolygonObj.Kd = cbPolygonObj.Ks = color_on_slicer;
				cbPolygonObj.alpha = color_on_slicer.a;
			}


			if (is_ghost_mode)
			{
				bool is_ghost_surface = actor->GetParam("_bool_IsGhostSurface", false);
				bool is_only_hotspot_visible = actor->GetParam("_bool_IsOnlyHotSpotVisible", false);
				cbPolygonObj.pobj_flag |= (int)is_ghost_surface << 22;
				cbPolygonObj.pobj_flag |= (int)is_only_hotspot_visible << 23;
				//cout << "TEST : " << is_ghost_surface << ", " << is_only_hotspot_visible << endl;
			}

			bool noSlicerFill = actor->GetParam("_bool_DisableSolidFillOnSlicer", false);
			if (isDrawingOnlyContours) noSlicerFill = true;
			cbPolygonObj.pobj_flag |= (int)noSlicerFill << 6;
			
			//if (planeThickness == 0) {
				cbPolygonObj.pix_thickness = actor->GetParam("_float_OutlinePixThickness", detaultOutlinePixelThickness);
				if (camForcedOutlinePixelThickness > 0) {
					cbPolygonObj.pix_thickness = camForcedOutlinePixelThickness;
				}

				cbPolygonObj.num_letters = prim_data->GetNumVertexDefinitions() * 3;
			//}
			
			D3D11_MAPPED_SUBRESOURCE mappedResPobjData;
			dx11DeviceImmContext->Map(cbuf_pobj, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResPobjData);
			CB_PolygonObject* cbPolygonObjData = (CB_PolygonObject*)mappedResPobjData.pData;
			memcpy(cbPolygonObjData, &cbPolygonObj, sizeof(CB_PolygonObject));
			dx11DeviceImmContext->Unmap(cbuf_pobj, 0);

			CB_ClipInfo cbClipInfo;
			grd_helper::SetCb_ClipInfo(cbClipInfo, pobj, actor, camClipMode, camClipperFreeActors, camClipMatWS2BS, camClipPlanePos, camClipPlaneDir);
			D3D11_MAPPED_SUBRESOURCE mappedResClipInfo;
			dx11DeviceImmContext->Map(cbuf_clip, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResClipInfo);
			CB_ClipInfo* cbClipInfoData = (CB_ClipInfo*)mappedResClipInfo.pData;
			memcpy(cbClipInfoData, &cbClipInfo, sizeof(CB_ClipInfo));
			dx11DeviceImmContext->Unmap(cbuf_clip, 0);

			//SET_CBUFFERS(0, 1, &cbuf_cam_state);
			dx11DeviceImmContext->VSSetConstantBuffers(1, 1, &cbuf_pobj);
			SET_CBUFFERS(1, 1, &cbuf_pobj);
			SET_CBUFFERS(2, 1, &cbuf_clip);
			SET_CBUFFERS(3, 1, &cbuf_reffect);
#pragma endregion
			// to do ray-tracer
			const bool fillInside = true;
			const bool cutPlane = true && !curved_slicer;

			const bool isLegacyBVH = pobj->GetBVHTree() != NULL;

			if (fillInside)
			{
				if (isLegacyBVH)
				{
					vmint4* nodePtr;
					int nodeSize;
					vmint4* triWoopPtr;
					int triWoopSize;
					vmint4* triDebugPtr;
					int triDebugSize;
					int* cpuTriIndicesPtr;
					int triIndicesSize;
					pobj->GetBVHTreeBuffers(&nodePtr, &nodeSize, &triWoopPtr, &triWoopSize,
						&triDebugPtr, &triDebugSize, &cpuTriIndicesPtr, &triIndicesSize);

					GpuRes bvhNode, bvhTriWoop, bvhTriDebug, bvhIndice;
					grd_helper::UpdateCustomBuffer(bvhNode, pobj, "BvhNode", nodePtr, nodeSize, DXGI_FORMAT_R32G32B32A32_FLOAT, sizeof(vmint4));
					grd_helper::UpdateCustomBuffer(bvhTriWoop, pobj, "BvhTriWoop", triWoopPtr, triWoopSize, DXGI_FORMAT_R32G32B32A32_FLOAT, sizeof(vmint4));
					grd_helper::UpdateCustomBuffer(bvhTriDebug, pobj, "BvhTriDebug", triDebugPtr, triDebugSize, DXGI_FORMAT_R32G32B32A32_FLOAT, sizeof(vmint4));
					grd_helper::UpdateCustomBuffer(bvhIndice, pobj, "BvhIndice", cpuTriIndicesPtr, triIndicesSize, DXGI_FORMAT_R32_SINT, sizeof(int));

					SET_SHADER_RES(0, 1, (ID3D11ShaderResourceView**)&bvhNode.alloc_res_ptrs[DTYPE_SRV]);
					SET_SHADER_RES(1, 1, (ID3D11ShaderResourceView**)&bvhTriWoop.alloc_res_ptrs[DTYPE_SRV]);
					SET_SHADER_RES(2, 1, (ID3D11ShaderResourceView**)&bvhTriDebug.alloc_res_ptrs[DTYPE_SRV]);
					SET_SHADER_RES(3, 1, (ID3D11ShaderResourceView**)&bvhIndice.alloc_res_ptrs[DTYPE_SRV]);
				}
				else
				{
					GpuRes gres_primitiveCounterBuffer;
					gres_primitiveCounterBuffer.vm_src_id = pobj->GetObjectID();
					gres_primitiveCounterBuffer.res_name = string("GPUBVH::primitiveCounterBuffer");
					GpuRes gres_bvhNodeBuffer;
					gres_bvhNodeBuffer.vm_src_id = pobj->GetObjectID();
					gres_bvhNodeBuffer.res_name = string("GPUBVH::BVHNodeBuffer");
					GpuRes gres_bvhParentBuffer;
					gres_bvhParentBuffer.vm_src_id = pobj->GetObjectID();
					gres_bvhParentBuffer.res_name = string("GPUBVH::BVHParentBuffer");
					GpuRes gres_bvhFlagBuffer;
					gres_bvhFlagBuffer.vm_src_id = pobj->GetObjectID();
					gres_bvhFlagBuffer.res_name = string("GPUBVH::BVHFlagBuffer");
					GpuRes gres_primitiveIDBuffer;
					gres_primitiveIDBuffer.vm_src_id = pobj->GetObjectID();
					gres_primitiveIDBuffer.res_name = string("GPUBVH::primitiveIDBuffer");
					GpuRes gres_primitiveBuffer;
					gres_primitiveBuffer.vm_src_id = pobj->GetObjectID();
					gres_primitiveBuffer.res_name = string("GPUBVH::primitiveBuffer");
					GpuRes gres_primitiveMortonBuffer;
					gres_primitiveMortonBuffer.vm_src_id = pobj->GetObjectID();
					gres_primitiveMortonBuffer.res_name = string("GPUBVH::primitiveMortonBuffer");

					vzlog_assert(gpu_manager->UpdateGpuResource(gres_primitiveCounterBuffer), "gres_primitiveCounterBuffer");
					vzlog_assert(gpu_manager->UpdateGpuResource(gres_bvhNodeBuffer), "gres_bvhNodeBuffer");
					vzlog_assert(gpu_manager->UpdateGpuResource(gres_bvhParentBuffer), "gres_bvhParentBuffer");
					vzlog_assert(gpu_manager->UpdateGpuResource(gres_bvhFlagBuffer), "gres_bvhFlagBuffer");
					vzlog_assert(gpu_manager->UpdateGpuResource(gres_primitiveIDBuffer), "gres_primitiveIDBuffer");
					vzlog_assert(gpu_manager->UpdateGpuResource(gres_primitiveBuffer), "gres_primitiveBuffer");
					vzlog_assert(gpu_manager->UpdateGpuResource(gres_primitiveMortonBuffer), "gres_primitiveMortonBuffer");

					SET_SHADER_RES(0, 1, (ID3D11ShaderResourceView**)&gres_primitiveCounterBuffer.alloc_res_ptrs[DTYPE_SRV]);
					SET_SHADER_RES(1, 1, (ID3D11ShaderResourceView**)&gres_primitiveIDBuffer.alloc_res_ptrs[DTYPE_SRV]);
					SET_SHADER_RES(2, 1, (ID3D11ShaderResourceView**)&gres_primitiveMortonBuffer.alloc_res_ptrs[DTYPE_SRV]);
					SET_SHADER_RES(3, 1, (ID3D11ShaderResourceView**)&gres_bvhNodeBuffer.alloc_res_ptrs[DTYPE_SRV]);
					SET_SHADER_RES(4, 1, (ID3D11ShaderResourceView**)&gres_primitiveBuffer.alloc_res_ptrs[DTYPE_SRV]);
					SET_SHADER_RES(5, 1, (ID3D11ShaderResourceView**)&gres_bvhParentBuffer.alloc_res_ptrs[DTYPE_SRV]);
					SET_SHADER_RES(6, 1, (ID3D11ShaderResourceView**)&gres_bvhFlagBuffer.alloc_res_ptrs[DTYPE_SRV]);

					// IASetVertexBuffers, SET_SHADER_RES 둘 다 동시에 가능?
					SET_SHADER_RES(10, 1, (ID3D11ShaderResourceView**)&gres_vtx.alloc_res_ptrs[DTYPE_SRV]);
					SET_SHADER_RES(11, 1, (ID3D11ShaderResourceView**)&gres_idx.alloc_res_ptrs[DTYPE_SRV]);

					BVHPushConstants push;
					push.primitiveCount = prim_data->num_prims;
					push.vertexStride = prim_data->GetNumVertexDefinitions() * 3;

					geometrics::AABB aabb(XMFLOAT3(prim_data->aabb_os.pos_min.x, prim_data->aabb_os.pos_min.y, prim_data->aabb_os.pos_min.z),
						XMFLOAT3(prim_data->aabb_os.pos_max.x, prim_data->aabb_os.pos_max.y, prim_data->aabb_os.pos_max.z));
					push.aabb_min = aabb.getMin();
					push.aabb_extents_rcp = aabb.getWidth();
					push.aabb_extents_rcp.x = 1.f / push.aabb_extents_rcp.x;
					push.aabb_extents_rcp.y = 1.f / push.aabb_extents_rcp.y;
					push.aabb_extents_rcp.z = 1.f / push.aabb_extents_rcp.z;

					grd_helper::PushConstants(&push, sizeof(BVHPushConstants), 0);
					//dx11DeviceImmContext->Map(cbuf_BVHPushConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_pushConstants);
					//BVHPushConstants* cbData = (BVHPushConstants*)mapped_pushConstants.pData;
					//memcpy(cbData, &push, sizeof(BVHPushConstants));
					//dx11DeviceImmContext->Unmap(cbuf_BVHPushConstants, 0);
					const ID3D11ShaderResourceView* srv_push = grd_helper::GetPushContantSRV();
					dx11DeviceImmContext->CSSetShaderResources(100, 1, (ID3D11ShaderResourceView* const*)&srv_push);
				}

#ifdef DX10_0
#else
				ID3D11UnorderedAccessView* dx11UAVs[NUM_UAVs] = {
					(ID3D11UnorderedAccessView*)gres_fb_counter.alloc_res_ptrs[DTYPE_UAV], 
					(ID3D11UnorderedAccessView*)gres_fb_k_buffer.alloc_res_ptrs[DTYPE_UAV], 
					is_picking_routine ? (ID3D11UnorderedAccessView*)gres_picking_buffer.alloc_res_ptrs[DTYPE_UAV] : NULL,
					(ID3D11UnorderedAccessView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_UAV],
					(ID3D11UnorderedAccessView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_UAV] };
#endif
				if (is_picking_routine) {
#ifdef DX10_0
					ID3D11Buffer* dx11BufferTargetPrim = (ID3D11Buffer*)gres_quad.alloc_res_ptrs[DTYPE_RES];
					ID3D11Buffer* dx11IndiceTargetPrim = NULL;
					uint stride_inputlayer = sizeof(vmfloat3);
					uint offset = 0;
					dx11DeviceImmContext->IASetVertexBuffers(0, 1, (ID3D11Buffer**)&dx11BufferTargetPrim, &stride_inputlayer, &offset);
					dx11DeviceImmContext->IASetInputLayout(dx11LI_P);
					dx11DeviceImmContext->VSSetShader(dx11VShader_P, NULL, 0);
					dx11DeviceImmContext->GSSetShader(NULL, NULL, 0);
					dx11DeviceImmContext->RSSetState(GETRASTER(SOLID_NONE));
					dx11DeviceImmContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
					dx11DeviceImmContext->OMSetDepthStencilState(GETDEPTHSTENTIL(ALWAYS), 0);

					SET_SHADER(
						curved_slicer ? GETPS(PickingCurvedThickSlice_ps_4_0) : GETPS(PickingThickSlice_ps_4_0),
						NULL, 0);

					SET_SHADER_RES(20, 1, (ID3D11ShaderResourceView**)&gres_picking_buffer.alloc_res_ptrs[DTYPE_SRV]);
					ID3D11RenderTargetView* dx11RTVs[2] = { (ID3D11RenderTargetView*)gres_picking_buffer2.alloc_res_ptrs[DTYPE_RTV], NULL};
					dx11DeviceImmContext->OMSetRenderTargets(1, dx11RTVs, NULL);
					dx11DeviceImmContext->Draw(4, 0);
					dx11DeviceImmContext->OMSetRenderTargets(1, dx11RTVsNULL, NULL);
					SET_SHADER_RES(20, 1, dx11SRVs_NULL);

					dx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gres_picking_buffer.alloc_res_ptrs[DTYPE_RES], (ID3D11Texture2D*)gres_picking_buffer2.alloc_res_ptrs[DTYPE_RES]);
#else
					dx11DeviceImmContext->CSSetUnorderedAccessViews(0, NUM_UAVs, dx11UAVs, NULL);
					if (isLegacyBVH)
					{
						SET_SHADER(curved_slicer ? GETCS(PickingCurvedThickSlice_cs_5_0) : GETCS(PickingThickSlice_cs_5_0), NULL, 0);
					}
					else
					{
						SET_SHADER(curved_slicer ? GETCS(PickingCurvedThickSlice_GPUBVH_cs_5_0) : GETCS(PickingThickSlice_GPUBVH_cs_5_0), NULL, 0);
					}
					dx11DeviceImmContext->Dispatch(1, 1, 1);
					dx11DeviceImmContext->CSSetUnorderedAccessViews(0, NUM_UAVs, dx11UAVs_NULL, NULL);
#endif
				}
				else {
#ifdef DX10_0
					ID3D11Buffer* dx11BufferTargetPrim = (ID3D11Buffer*)gres_quad.alloc_res_ptrs[DTYPE_RES];
					ID3D11Buffer* dx11IndiceTargetPrim = NULL;
					uint stride_inputlayer = sizeof(vmfloat3);
					uint offset = 0;
					dx11DeviceImmContext->IASetVertexBuffers(0, 1, (ID3D11Buffer**)&dx11BufferTargetPrim, &stride_inputlayer, &offset);
					dx11DeviceImmContext->IASetInputLayout(dx11LI_P);
					dx11DeviceImmContext->VSSetShader(dx11VShader_P, NULL, 0);
					dx11DeviceImmContext->GSSetShader(NULL, NULL, 0);
					dx11DeviceImmContext->RSSetState(GETRASTER(SOLID_NONE));
					dx11DeviceImmContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
					dx11DeviceImmContext->OMSetDepthStencilState(GETDEPTHSTENTIL(ALWAYS), 0);

					SET_SHADER(
						curved_slicer ? GETPS(CurvedThickSlicePathTracer_ps_4_0) : GETPS(ThickSlicePathTracer_ps_4_0),
						NULL, 0);

					SET_SHADER_RES(20, 1, (ID3D11ShaderResourceView**)&gres_fb_rgba.alloc_res_ptrs[DTYPE_SRV]);
					SET_SHADER_RES(21, 1, (ID3D11ShaderResourceView**)&gres_fb_depthcs.alloc_res_ptrs[DTYPE_SRV]);
					ID3D11RenderTargetView* dx11RTVs[2] = {
					(ID3D11RenderTargetView*)gres_fb_rgba2.alloc_res_ptrs[DTYPE_RTV],
						(ID3D11RenderTargetView*)gres_fb_depthcs2.alloc_res_ptrs[DTYPE_RTV] };
					dx11DeviceImmContext->OMSetRenderTargets(2, dx11RTVs, NULL);
					dx11DeviceImmContext->Draw(4, 0);
					dx11DeviceImmContext->OMSetRenderTargets(2, dx11RTVsNULL, NULL);
					SET_SHADER_RES(20, 2, dx11SRVs_NULL);

					// DX10 forces to run the slicer with zero thickness
					if (1)// planeThickness == 0.f)
					{
						SET_SHADER(GETPS(SliceOutline_ps_4_0), NULL, 0);
						SET_SHADER_RES(20, 1, (ID3D11ShaderResourceView**)&gres_fb_rgba2.alloc_res_ptrs[DTYPE_SRV]);
						SET_SHADER_RES(21, 1, (ID3D11ShaderResourceView**)&gres_fb_depthcs2.alloc_res_ptrs[DTYPE_SRV]);
						dx11RTVs[0] = (ID3D11RenderTargetView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_RTV];
						dx11RTVs[1] = (ID3D11RenderTargetView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_RTV];
						dx11DeviceImmContext->OMSetRenderTargets(2, dx11RTVs, NULL);
						dx11DeviceImmContext->Draw(4, 0);
						dx11DeviceImmContext->OMSetRenderTargets(2, dx11RTVsNULL, NULL);
						SET_SHADER_RES(20, 2, dx11SRVs_NULL);
					}
					else {
						dx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gres_fb_rgba.alloc_res_ptrs[DTYPE_RES],
							(ID3D11Texture2D*)gres_fb_rgba2.alloc_res_ptrs[DTYPE_RES]);
						dx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_RES],
							(ID3D11Texture2D*)gres_fb_depthcs2.alloc_res_ptrs[DTYPE_RES]);
					}
#else
					if (isLegacyBVH)
					{
						SET_SHADER(
							curved_slicer ? GETCS(CurvedThickSlicePathTracer_cs_5_0) : GETCS(ThickSlicePathTracer_cs_5_0),
							NULL, 0);
					}
					else
					{
						SET_SHADER(
							curved_slicer ? GETCS(CurvedThickSlicePathTracer_GPUBVH_cs_5_0) : GETCS(ThickSlicePathTracer_GPUBVH_cs_5_0),
							NULL, 0);
					}
					dx11DeviceImmContext->CSSetUnorderedAccessViews(0, NUM_UAVs, dx11UAVs, NULL);
					dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);

					if (planeThickness == 0.f || noSlicerFill) {
						dx11DeviceImmContext->CSSetUnorderedAccessViews(4, 1, dx11UAVs_NULL, NULL);
						SET_SHADER_RES(21, 1, (ID3D11ShaderResourceView**)&gres_fb_depthcs.alloc_res_ptrs[DTYPE_SRV]);
						SET_SHADER(GETCS(SliceOutline_cs_5_0), NULL, 0);
						dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);
						SET_SHADER_RES(20, 2, dx11SRVs_NULL);
					}
					//else if (planeThickness > 0.f) 
					//{
					//	// call k-buffer resolve pass
					//}
					//else assert(0);
					
					dx11DeviceImmContext->CSSetUnorderedAccessViews(0, NUM_UAVs, dx11UAVs_NULL, NULL);
#endif
				}
				//dx11DeviceImmContext->CSSetUnorderedAccessViews(0, NUM_UAVs, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
			}

			// cutPlane
//			if (0) {
//				// Lines Cuttins
//#pragma region // Setting Rasterization Stages
//				ID3D11InputLayout* dx11InputLayer_Target = NULL;
//				ID3D11VertexShader* dx11VS_Target = NULL;
//				ID3D11GeometryShader* dx11GS_Target = NULL;
//				//ID3D11PixelShader* dx11PS_Target = NULL;
//				ID3D11RasterizerState2* dx11RState_TargetObj = NULL;
//				uint offset = 0;
//				D3D_PRIMITIVE_TOPOLOGY pobj_topology_type;
//
//				if (prim_data->GetVerticeDefinition("NORMAL"))
//				{
//					if (prim_data->GetVerticeDefinition("TEXCOORD0"))
//					{
//						// PNT (here, T is used as color)
//						dx11InputLayer_Target = dx11LI_PNT;
//						dx11VS_Target = dx11VShader_PNT;
//					}
//					else
//					{
//						// PN
//						dx11InputLayer_Target = dx11LI_PN;
//						dx11VS_Target = dx11VShader_PN;
//					}
//				}
//				else if (prim_data->GetVerticeDefinition("TEXCOORD0"))
//				{
//					if (prim_data->GetVerticeDefinition("TEXCOORD2"))
//					{
//						dx11InputLayer_Target = dx11LI_PTTT;
//						dx11VS_Target = dx11VShader_PTTT;
//					}
//					else
//					{
//						dx11InputLayer_Target = dx11LI_PT;
//						dx11VS_Target = dx11VShader_PT;
//					}
//				}
//				else
//				{
//					// P
//					dx11InputLayer_Target = dx11LI_P;
//					dx11VS_Target = dx11VShader_P;
//				}
//
//				assert(prim_data->ptype == PrimitiveTypeTRIANGLE);
//
//				if (prim_data->is_stripe)
//					pobj_topology_type = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
//				else
//					pobj_topology_type = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
//
//				dx11RState_TargetObj = GETRASTER(SOLID_NONE);
//
//				ID3D11Buffer* dx11BufferTargetPrim = (ID3D11Buffer*)gres_vtx.alloc_res_ptrs[DTYPE_RES];
//				ID3D11Buffer* dx11IndiceTargetPrim = NULL;
//				uint stride_inputlayer = sizeof(vmfloat3) * (uint)prim_data->GetNumVertexDefinitions();
//				dx11DeviceImmContext->IASetVertexBuffers(0, 1, (ID3D11Buffer**)&dx11BufferTargetPrim, &stride_inputlayer, &offset);
//				if (prim_data->vidx_buffer != NULL)
//				{
//					dx11IndiceTargetPrim = (ID3D11Buffer*)gres_idx.alloc_res_ptrs[DTYPE_RES];
//					dx11DeviceImmContext->IASetIndexBuffer(dx11IndiceTargetPrim, DXGI_FORMAT_R32_UINT, 0);
//				}
//
//				dx11DeviceImmContext->IASetInputLayout(dx11InputLayer_Target);
//				dx11DeviceImmContext->VSSetShader(dx11VS_Target, NULL, 0);
//				dx11DeviceImmContext->GSSetShader(dx11GS_Target, NULL, 0);
//				dx11DeviceImmContext->RSSetState(dx11RState_TargetObj);
//				dx11DeviceImmContext->IASetPrimitiveTopology(pobj_topology_type);
//				dx11DeviceImmContext->PSSetShader(NULL, NULL, 0);
//#pragma endregion 
//
//#pragma region // GEO RENDERING PASS
//
//				// https://stackoverflow.com/questions/12606556/how-do-you-use-geometry-shader-with-output-stream
//				dx11DeviceImmContext->OMSetDepthStencilState(GETDEPTHSTENTIL(ALWAYS), 0);
//
//				dx11DeviceImmContext->SOSetTargets(1, (ID3D11Buffer**)&gres_cutlines_buffer.alloc_res_ptrs[DTYPE_RES], &offset);
//
//				if (prim_data->is_stripe || prim_data->vidx_buffer == NULL)
//					dx11DeviceImmContext->Draw(prim_data->num_vtx, 0);
//				else
//					dx11DeviceImmContext->DrawIndexed(prim_data->num_vidx, 0, 0);
//
//				dx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(2, dx11RTVsNULL, NULL, 2, NUM_UAVs, dx11UAVs_NULL, 0);
//#pragma endregion // GEO RENDERING PASS
//			}
			count_call_render++;
		}
	};

	dx11DeviceImmContext->VSSetConstantBuffers(0, 1, &cbuf_cam_state);
	dx11DeviceImmContext->GSSetConstantBuffers(0, 1, &cbuf_cam_state);
	dx11DeviceImmContext->PSSetConstantBuffers(0, 1, &cbuf_cam_state);
	dx11DeviceImmContext->CSSetConstantBuffers(0, 1, &cbuf_cam_state);
	CB_CameraState cbCamState;
	grd_helper::SetCb_Camera(cbCamState, matWS2SS, matSS2WS, matWS2CS, cam_obj, fb_size_cur, k_value, gi_v_thickness);
//#ifdef DX10_0
//	cbCamState.far_plane = planeThickness_original;
//#else
//	cbCamState.far_plane = planeThickness;
//#endif
	cbCamState.far_plane = planeThickness;
	if (!is_render_out) {
		// which means the k-buffer will be used for the following renderer
		// stores the fragments into the k-buffer and do not store the rendering result into RT
		cbCamState.cam_flag |= (0x1 << 3);
	}
	auto SetCamConstBuf = [&dx11DeviceImmContext, &cbuf_cam_state](const CB_CameraState& cbCamState) {
		D3D11_MAPPED_SUBRESOURCE mappedResCamState;
		dx11DeviceImmContext->Map(cbuf_cam_state, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResCamState);
		CB_CameraState* cbCamStateData = (CB_CameraState*)mappedResCamState.pData;
		memcpy(cbCamStateData, &cbCamState, sizeof(CB_CameraState));
		dx11DeviceImmContext->Unmap(cbuf_cam_state, 0);
	};

	// RENDER BEGIN
	dx11CommonParams->GpuProfile("Render Slicer");

	if (is_picking_routine) {
		cbCamState.iSrCamDummy__1 = (picking_pos_ss.x & 0xFFFF | picking_pos_ss.y << 16);
		cbCamState.cam_flag |= (0x1 << 5);
		
		SetCamConstBuf(cbCamState);

		dx11CommonParams->GpuProfile("Picking");

		PathTracer(slicer_actors, curved_slicer, is_ghost_mode, true); // is_picking_routine = true

#pragma region copyback to sysmem
		dx11DeviceImmContext->CopyResource((ID3D11Buffer*)gres_picking_system_buffer.alloc_res_ptrs[DTYPE_RES],
			(ID3D11Buffer*)gres_picking_buffer.alloc_res_ptrs[DTYPE_RES]);

		map<int, float> picking_layers_id_depth;
		map<float, int> picking_layers_depth_id;
		D3D11_MAPPED_SUBRESOURCE mappedResSysPicking;
		HRESULT hr = dx11DeviceImmContext->Map((ID3D11Buffer*)gres_picking_system_buffer.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, NULL, &mappedResSysPicking);

		int num_layers = 0;
#ifdef DX10_0
		vmfloat4 data = *(vmfloat4*)mappedResSysPicking.pData;
		uint obj_id = *(uint*)(&data.x);
		num_layers = obj_id > 0 ? 1 : 0;
		if (num_layers > 0)
			picking_layers_id_depth[obj_id] = data.y;
#else
		uint* picking_buf = (uint*)mappedResSysPicking.pData;
		// note each layer has 5 integer-stored data 
		// id, depth, planePos.xyz

		const int num_picking_objs = (int)picking_buf[0];
		//vzlog("##num picking %d", num_picking_objs);

		for (int i = 1; i <= num_picking_objs; i += 2)
		{ 
			uint obj_id = picking_buf[i];
			if (obj_id == 0) break;

			float pick_depth = *(float*)&picking_buf[i + 1];

			auto it = picking_layers_id_depth.find(obj_id);
			if (it == picking_layers_id_depth.end()) {
				picking_layers_id_depth.insert(pair<int, float>(obj_id, pick_depth));
			}
			else {
				if (pick_depth < it->second) it->second = pick_depth;
			}
			num_layers++;
		}
#endif
		dx11DeviceImmContext->Unmap((ID3D11Buffer*)gres_picking_system_buffer.alloc_res_ptrs[DTYPE_RES], 0);
#pragma endregion copyback to sysmem
		dx11CommonParams->GpuProfile("Picking", true);

		//if (gpu_profile) {
		//	cout << "### NUM PICKING LAYERS : " << num_layers << endl;
		//	cout << "### NUM PICKING UNIQUE ID LAYERS : " << picking_layers_id_depth.size() << endl;
		//}
		for (auto& it : picking_layers_id_depth) {
			picking_layers_depth_id[it.second] = it.first;
		}

		vector<vmfloat3> picking_pos_out;
		vector<int> picking_id_out;
		for (auto& it : picking_layers_depth_id) {
			vmfloat3 pos_pick = picking_ray_origin + picking_ray_dir * it.first;
			picking_pos_out.push_back(pos_pick);
			picking_id_out.push_back(it.second);
		}

		_fncontainer->fnParams.SetParam("_vlist_float3_PickPos", picking_pos_out);
		_fncontainer->fnParams.SetParam("_vlist_int_PickId", picking_id_out);
		// END of Render Process for picking
	}
	else {
		cbCamState.iSrCamDummy__0 = *(uint*)&merging_beta;
		cbCamState.iSrCamDummy__2 = *(uint*)&scale_z_res;
		SetCamConstBuf(cbCamState);

		dx11CommonParams->GpuProfile("PathTracer");
		// buffer filling
		PathTracer(slicer_actors, curved_slicer, is_ghost_mode, false); // is_picking_routine = false
		dx11CommonParams->GpuProfile("PathTracer", true);

		// Set NULL States //
		//dx11DeviceImmContext->CSSetUnorderedAccessViews(1, NUM_UAVs_GEO, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
		SET_SHADER_RES(0, 2, dx11SRVs_NULL);

#ifdef DX10_0
#else
		if (planeThickness > 0 
			//|| (planeThickness == 0 && is_final_renderer)
			) {
			// to do ...
			UINT UAVInitialCounts = 0;
			ID3D11UnorderedAccessView* dx11UAVs_2nd_pass[3] = {
					(ID3D11UnorderedAccessView*)gres_fb_k_buffer.alloc_res_ptrs[DTYPE_UAV]
					, (ID3D11UnorderedAccessView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_UAV]
					, (ID3D11UnorderedAccessView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_UAV]
			};
			dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, (ID3D11UnorderedAccessView**)&gres_fb_counter.alloc_res_ptrs[DTYPE_UAV], 0); // trimming may occur 
			//dx11DeviceImmContext->CSSetShaderResources(1, 1, (ID3D11ShaderResourceView**)&gres_fb_singlelayer_rgba.alloc_res_ptrs[DTYPE_SRV]);
			//dx11DeviceImmContext->CSSetShaderResources(2, 1, (ID3D11ShaderResourceView**)&gres_fb_singlelayer_depthcs.alloc_res_ptrs[DTYPE_SRV]);
			dx11DeviceImmContext->CSSetShader(GETCS(OIT_SKBZ_RESOLVE_cs_5_0), NULL, 0);
			dx11DeviceImmContext->CSSetUnorderedAccessViews(1, 3, dx11UAVs_2nd_pass, (UINT*)(&dx11UAVs_2nd_pass));

			dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);
			// Set NULL States //
			dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL)); // counter
			dx11DeviceImmContext->CSSetUnorderedAccessViews(1, 3, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
			dx11DeviceImmContext->CSSetShaderResources(0, 2, dx11SRVs_NULL);
		}
#endif
	}

	iobj->SetObjParam("_int_NumCallRenders", count_call_render);
	dx11CommonParams->GpuProfile("Render Slicer", true);

	dx11DeviceImmContext->ClearState();

	dx11DeviceImmContext->OMSetRenderTargets(1, &pdxRTVOld, pdxDSVOld);
	VMSAFE_RELEASE(pdxRTVOld);
	VMSAFE_RELEASE(pdxDSVOld);

	iobj->SetDescriptor("vismtv_inbuilt_renderergpudx module");

	iobj->SetObjParam("_ullong_LatestSrTime", vmhelpers::GetCurrentTimePack());
	//((std::mutex*)HDx11GetMutexGpuCriticalPath())->unlock();

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
				if (__count[j + i * buf_row_pitch] > 0)
					int gg = 0;
			}
		};
		dx11DeviceImmContext->Unmap((ID3D11Texture2D*)gres_fb_counter_sys.alloc_res_ptrs[DTYPE_RES], 0);
	}
#endif
	return true;
}