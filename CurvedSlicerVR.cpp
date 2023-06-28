#include "RendererHeader.h"

using namespace grd_helper;
#define __SRV_PTR ID3D11ShaderResourceView* 

bool RenderVrCurvedSlicer(VmFnContainer* _fncontainer,
	VmGpuManager* gpu_manager,
	grd_helper::GpuDX11CommonParameters* dx11CommonParams,
	LocalProgress* progress,
	double* run_time_ptr)
{
#ifdef __DX_DEBUG_QUERY
	if (dx11CommonParams->debug_info_queue == NULL)
		dx11CommonParams->dx11Device->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)&dx11CommonParams->debug_info_queue);
	dx11CommonParams->debug_info_queue->PushEmptyStorageFilter();
#endif
	
#pragma region // Parameter Setting //
	VmIObject* iobj = _fncontainer->fnParams.GetParam("_VmIObject*_RenderOut", (VmIObject*)NULL);
	int k_value_old = iobj->GetObjParam("_int_NumK", (int)8);
	int k_value = _fncontainer->fnParams.GetParam("_int_NumK", k_value_old);
	iobj->SetObjParam("_int_NumK", k_value);

	vmfloat4 default_phong_lighting_coeff = vmfloat4(0.2, 1.0, 0.5, 5); // Emission, Diffusion, Specular, Specular Power
	bool force_to_update_otf = _fncontainer->fnParams.GetParam("_bool_ForceToUpdateOtf", false);
	bool show_block_test = _fncontainer->fnParams.GetParam("_bool_IsShowBlock", false);
	float v_thickness = (float)_fncontainer->fnParams.GetParam("_float_VZThickness", 0.0f);

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

	int ray_cast_type = _fncontainer->fnParams.GetParam("_int_VolumeRayCastType", (int)0);
	float samplePrecisionLevel = _fncontainer->fnParams.GetParam("_float_SamplePrecisionLevel", 1.0f);

	bool fastRender2x = _fncontainer->fnParams.GetParam("_bool_FastRender2X", false);

	// TEST
	int test_value = _fncontainer->fnParams.GetParam("_int_TestValue", (int)0);
	int test_mode = _fncontainer->fnParams.GetParam("_int_TestMode", (int)0);

	bool recompile_hlsl = _fncontainer->fnParams.GetParam("_bool_ReloadHLSLObjFiles", false);

	float planeThickness = _fncontainer->fnParams.GetParam("_float_PlaneThickness", -1.f);

	VmLight* light = _fncontainer->fnParams.GetParam("_VmLight*_LightSource", (VmLight*)NULL);
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
	if (recompile_hlsl)
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
		hlslobj_path += "..\\..\\VmProjects\\hybrid_rendering_engine\\shader_compiled_objs\\";
		//cout << hlslobj_path << endl;

		string prefix_path = hlslobj_path;

#define CS_NUM 8
#define SET_CS(NAME) dx11CommonParams->safe_set_res(grd_helper::COMRES_INDICATOR(GpuhelperResType::COMPUTE_SHADER, NAME), dx11CShader, true)

		string strNames_CS[CS_NUM] = {
			   "PanoVR_RAYMAX_cs_5_0"
			  ,"PanoVR_RAYMIN_cs_5_0"
			  ,"PanoVR_RAYSUM_cs_5_0"
			  ,"PanoVR_DEFAULT_cs_5_0"
			  ,"PanoVR_MODULATE_cs_5_0"
			  ,"PanoVR_MULTIOTF_DEFAULT_cs_5_0"
			  ,"PanoVR_MULTIOTF_MODULATE_cs_5_0"
			  ,"FillDither_cs_5_0"
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
	}
#pragma endregion 

#pragma region // IOBJECT OUT
	while (iobj->GetFrameBuffer(FrameBufferUsageRENDEROUT, 1) != NULL)
		iobj->DeleteFrameBuffer(FrameBufferUsageRENDEROUT, 1);
	if (!iobj->ReplaceFrameBuffer(FrameBufferUsageRENDEROUT, 0, data_type::dtype<vmbyte4>(), ("common render out frame buffer : defined in vismtv_inbuilt_renderergpudx module")))
		iobj->InsertFrameBuffer(data_type::dtype<vmbyte4>(), FrameBufferUsageRENDEROUT, ("common render out frame buffer : defined in vismtv_inbuilt_renderergpudx module"));

	while (iobj->GetFrameBuffer(FrameBufferUsageDEPTH, 1) != NULL)
		iobj->DeleteFrameBuffer(FrameBufferUsageDEPTH, 1);
	if (!iobj->ReplaceFrameBuffer(FrameBufferUsageDEPTH, 0, data_type::dtype<float>(), ("1st hit screen depth frame buffer : defined in vismtv_inbuilt_renderergpudx module")))
		iobj->InsertFrameBuffer(data_type::dtype<float>(), FrameBufferUsageDEPTH, ("1st hit screen depth frame buffer : defined in vismtv_inbuilt_renderergpudx module"));
#pragma endregion 

	__ID3D11Device* pdx11Device = dx11CommonParams->dx11Device;
	__ID3D11DeviceContext* dx11DeviceImmContext = dx11CommonParams->dx11DeviceImmContext;

#pragma region // IOBJECT GPU
	vmint2 fb_size_cur;
	iobj->GetFrameBufferInfo(&fb_size_cur);
	vmint2 fb_size_old = iobj->GetObjParam("_int2_PreviousScreenSize", vmint2(0, 0));
	if (fb_size_cur.x != fb_size_old.x || fb_size_cur.y != fb_size_old.y || k_value != k_value_old
		|| k_value != k_value_old)
	{
		gpu_manager->ReleaseGpuResourcesBySrcID(iobj->GetObjectID());	// System Out 포함 //
		iobj->SetObjParam("_int2_PreviousScreenSize", fb_size_cur);
	}

	bool gpu_profile = false;
	if (fb_size_cur.x > 200 && fb_size_cur.y > 200)
	{
		gpu_profile = _fncontainer->fnParams.GetParam("_bool_GpuProfile", false);
	}
	//gpu_profile = true;

	GpuRes gres_fb_rgba, gres_fb_depthcs, gres_fb_vrdepthcs;
	GpuRes gres_fb_k_buffer, gres_fb_counter;
	GpuRes gres_fb_sys_rgba, gres_fb_sys_depthcs;

	grd_helper::UpdateFrameBuffer(gres_fb_rgba, iobj, "RENDER_OUT_RGBA_0", RTYPE_TEXTURE2D,
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
	grd_helper::UpdateFrameBuffer(gres_fb_depthcs, iobj, "RENDER_OUT_DEPTH_0", RTYPE_TEXTURE2D,
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_FLOAT, 0);
	grd_helper::UpdateFrameBuffer(gres_fb_vrdepthcs, iobj, "RENDER_OUT_DEPTH_1", RTYPE_TEXTURE2D,
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_FLOAT, 0);
	grd_helper::UpdateFrameBuffer(gres_fb_counter, iobj, "RW_COUNTER", RTYPE_TEXTURE2D,
		D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_UINT, 0);

	const int num_frags_perpixel = k_value * 4;
	grd_helper::UpdateFrameBuffer(gres_fb_k_buffer, iobj, "BUFFER_RW_K_BUF", RTYPE_BUFFER,
		D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_TYPELESS, UPFB_RAWBYTE, num_frags_perpixel);

	grd_helper::UpdateFrameBuffer(gres_fb_sys_rgba, iobj, "SYSTEM_OUT_RGBA", RTYPE_TEXTURE2D, NULL, DXGI_FORMAT_R8G8B8A8_UNORM, UPFB_SYSOUT);
	grd_helper::UpdateFrameBuffer(gres_fb_sys_depthcs, iobj, "SYSTEM_OUT_DEPTH", RTYPE_TEXTURE2D, NULL, DXGI_FORMAT_R32_FLOAT, UPFB_SYSOUT);
#pragma endregion 

	vector<vmfloat3>& vtrCurveInterpolations = *_fncontainer->fnParams.GetParamPtr<vector<vmfloat3>>("_vlist_FLOAT3_CurveInterpolations");
	vector<vmfloat3>& vtrCurveUpVectors = *_fncontainer->fnParams.GetParamPtr<vector<vmfloat3>>("_vlist_FLOAT3_CurveUpVectors");
	vector<vmfloat3>& vtrCurveTangentVectors = *_fncontainer->fnParams.GetParamPtr<vector<vmfloat3>>("_vlist_FLOAT3_CurveTangentVectors");
	GpuRes gres_cpPoints, gres_cpTangents;
	int num_curvedPoints = (int)vtrCurveInterpolations.size();
	if (num_curvedPoints < 1) 
		return false;
	grd_helper::UpdateCustomBuffer(gres_cpPoints, iobj, "CurvedPlanePoints", &vtrCurveInterpolations[0], num_curvedPoints, DXGI_FORMAT_R32G32B32_FLOAT, 12);
	grd_helper::UpdateCustomBuffer(gres_cpTangents, iobj, "CurvedPlaneTangents", &vtrCurveTangentVectors[0], num_curvedPoints, DXGI_FORMAT_R32G32B32_FLOAT, 12);
	dx11DeviceImmContext->CSSetShaderResources(30, 1, (__SRV_PTR*)&gres_cpPoints.alloc_res_ptrs[DTYPE_SRV]);
	dx11DeviceImmContext->CSSetShaderResources(31, 1, (__SRV_PTR*)&gres_cpTangents.alloc_res_ptrs[DTYPE_SRV]);

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

		if (dvr_volumes.size() > 1) {
			test_out("WARNNING!! two rendering target volumes are not allowed!");
			break;
		}
		dvr_volumes.push_back(actor);
		VmVObjectVolume* volobj = (VmVObjectVolume*)geo_obj;
		VolumeData* vol_data = volobj->GetVolumeData();

		min_pitch = min(min(
			min(vol_data->vox_pitch.x, vol_data->vox_pitch.y),
			vol_data->vox_pitch.z),
			min_pitch);
	}
#pragma endregion 

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
		dx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gres_fb_counter.alloc_res_ptrs[DTYPE_UAV], clr_unit4);
		//dx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gres_fb_k_buffer.alloc_res_ptrs[DTYPE_UAV], clr_unit4);
		dx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_UAV], clr_unit4);
		dx11DeviceImmContext->ClearUnorderedAccessViewFloat((ID3D11UnorderedAccessView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_UAV], clr_float_fltmax_4);
		// note that gres_fb_vrdepthcs is supposed to be initialized in VR_SURFACE
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
	ID3D11Buffer* cbuf_curvedslicer = dx11CommonParams->get_cbuf("CB_CurvedSlicer");

#pragma region // HLSL Sampler Setting
	ID3D11SamplerState* sampler_PZ = dx11CommonParams->get_sampler("POINT_ZEROBORDER");
	ID3D11SamplerState* sampler_LZ = dx11CommonParams->get_sampler("LINEAR_ZEROBORDER");
	ID3D11SamplerState* sampler_PC = dx11CommonParams->get_sampler("POINT_CLAMP");
	ID3D11SamplerState* sampler_LC = dx11CommonParams->get_sampler("LINEAR_CLAMP");
	dx11DeviceImmContext->CSSetSamplers(0, 1, &sampler_LZ);
	dx11DeviceImmContext->CSSetSamplers(1, 1, &sampler_PZ);
	dx11DeviceImmContext->CSSetSamplers(2, 1, &sampler_LC);
	dx11DeviceImmContext->CSSetSamplers(3, 1, &sampler_PC);
#pragma endregion

	ID3D11UnorderedAccessView* dx11UAVs_NULL[10] = { NULL };
	ID3D11ShaderResourceView* dx11SRVs_NULL[10] = { NULL };

#pragma region // Camera & Environment 
	// 	const int __BLOCKSIZE = 8;
	// 	uint num_grid_x = (uint)ceil(fb_size_cur.x / (float)__BLOCKSIZE);
	// 	uint num_grid_y = (uint)ceil(fb_size_cur.y / (float)__BLOCKSIZE);
	const int __BLOCKSIZE = _fncontainer->fnParams.GetParam("_int_GpuThreadBlockSize", (int)2);
	uint num_grid_x = __BLOCKSIZE == 1 ? fb_size_cur.x : (uint)ceil(fb_size_cur.x / (float)__BLOCKSIZE);
	uint num_grid_y = __BLOCKSIZE == 1 ? fb_size_cur.y : (uint)ceil(fb_size_cur.y / (float)__BLOCKSIZE);

	VmCObject* cam_obj = iobj->GetCameraObject();
	vmmat44 dmatWS2CS, dmatCS2PS, dmatPS2SS;
	vmmat44 dmatSS2PS, dmatPS2CS, dmatCS2WS;
	cam_obj->GetMatrixWStoSS(&dmatWS2CS, &dmatCS2PS, &dmatPS2SS);
	cam_obj->GetMatrixSStoWS(&dmatSS2PS, &dmatPS2CS, &dmatCS2WS);
	vmmat44 dmatWS2PS = dmatWS2CS * dmatCS2PS;
	vmmat44f matWS2PS = dmatWS2PS;
	vmmat44f matWS2SS = dmatWS2PS * dmatPS2SS;
	vmmat44f matSS2WS = (dmatSS2PS * dmatPS2CS) * dmatCS2WS;

	CB_CameraState cbCamState;
	grd_helper::SetCb_Camera(cbCamState, matWS2SS, matSS2WS, cam_obj, fb_size_cur, k_value, v_thickness <= 0 ? min_pitch : (float)v_thickness);
	cbCamState.iSrCamDummy__0 = *(uint*)&merging_beta;
	if (fastRender2x) cbCamState.cam_flag |= 0x1 << 8; // 9th bit set

	D3D11_MAPPED_SUBRESOURCE mappedResCamState;
	dx11DeviceImmContext->Map(cbuf_cam_state, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResCamState);
	CB_CameraState* cbCamStateData = (CB_CameraState*)mappedResCamState.pData;
	memcpy(cbCamStateData, &cbCamState, sizeof(CB_CameraState));
	dx11DeviceImmContext->Unmap(cbuf_cam_state, 0);
	dx11DeviceImmContext->CSSetConstantBuffers(0, 1, &cbuf_cam_state);

	CB_EnvState cbEnvState;
	grd_helper::SetCb_Env(cbEnvState, cam_obj, light_src, global_lighting, lens_effect);
	if(light_src.is_on_camera)
		cbEnvState.env_flag |= 0x4;
	D3D11_MAPPED_SUBRESOURCE mappedResEnvState;
	dx11DeviceImmContext->Map(cbuf_env_state, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResEnvState);
	CB_EnvState* cbEnvStateData = (CB_EnvState*)mappedResEnvState.pData;
	memcpy(cbEnvStateData, &cbEnvState, sizeof(CB_EnvState));
	dx11DeviceImmContext->Unmap(cbuf_env_state, 0);
	dx11DeviceImmContext->CSSetConstantBuffers(7, 1, &cbuf_env_state);

	if (is_ghost_mode)
	{
		// do 'dynamic'
		D3D11_MAPPED_SUBRESOURCE mappedResHSMask;
		dx11DeviceImmContext->Map(cbuf_hsmask, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResHSMask);
		CB_HotspotMask* cbHSMaskData = (CB_HotspotMask*)mappedResHSMask.pData;
		grd_helper::SetCb_HotspotMask(*cbHSMaskData, _fncontainer, matWS2SS);
		dx11DeviceImmContext->Unmap(cbuf_hsmask, 0);
		dx11DeviceImmContext->CSSetConstantBuffers(9, 1, &cbuf_hsmask);
	}
#pragma endregion // Light & Shadow Setting

	float sample_dist = min_pitch;

	CB_CurvedSlicer cbCurvedSlicer;
	grd_helper::SetCb_CurvedSlicer(cbCurvedSlicer, _fncontainer, iobj, sample_dist);
	D3D11_MAPPED_SUBRESOURCE mappedResCurvedSlicerState;
	dx11DeviceImmContext->Map(cbuf_curvedslicer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResCurvedSlicerState);
	CB_CurvedSlicer* cbCurvedSlicerData = (CB_CurvedSlicer*)mappedResCurvedSlicerState.pData;
	memcpy(cbCurvedSlicerData, &cbCurvedSlicer, sizeof(CB_CurvedSlicer));
	dx11DeviceImmContext->Unmap(cbuf_curvedslicer, 0);
	dx11DeviceImmContext->CSSetConstantBuffers(10, 1, &cbuf_curvedslicer);

	map<string, vmint2>& profile_map = dx11CommonParams->profile_map;
	if (gpu_profile)
	{
		int gpu_profilecount = (int)profile_map.size();
		dx11DeviceImmContext->Begin(dx11CommonParams->dx11qr_disjoint);
		//gpu_profilecount++;
	}

	auto ___GpuProfile = [&gpu_profile, &dx11DeviceImmContext, &profile_map, &dx11CommonParams](const string& profile_name, const bool is_closed = false) {
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

			dx11DeviceImmContext->End(dx11CommonParams->dx11qr_timestamps[stamp_idx]);
			//gpu_profilecount++;
		}
	};


	// Initial Setting of Frame Buffers //
	int count_call_render = iobj->GetObjParam("_int_NumCallRenders", (int)0);
	bool is_performed_ssao = false;

	___GpuProfile("VR Begin");

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
#define __RM_SCULPTMASK 22
#define __RM_MULTIOTF 23
#define __RM_VISVOLMASK 24
//#define __RM_TEST 6
#define __RM_RAYMAX 10
#define __RM_RAYMIN 11
#define __RM_RAYSUM 12

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
			GradientMagnitudeAnalysis(grad_minmax, vobj);
			is_modulation_mode = true;
			break;
		case __RM_DEFAULT:
		case __RM_MULTIOTF:
		case __RM_VISVOLMASK:
		default: break;
		}

		bool skip_volblk_update = actor->GetParam("_bool_ForceToSkipBlockUpdate", false);
		int blk_level = actor->GetParam("_int_BlockLevel", (int)1);
		blk_level = max(min(1, blk_level), 0);

		bool use_mask_volume = actor->GetParam("_bool_ValidateMaskVolume", false);
		int sculpt_index = actor->GetParam("_int_SculptingIndex", (int)-1);

		vmfloat4 material_phongCoeffs = actor->GetParam("_float4_PhongCoeffs", default_phong_lighting_coeff);
		int outline_thickness = actor->GetParam("_int_SilhouetteThickness", (int)0);
		float outline_depthThres = 10000.f;
		vmfloat3 outline_color = vmfloat3(1.f);
		if (outline_thickness > 0) {
			outline_depthThres = actor->GetParam("_float_SilhouetteDepthThres", outline_depthThres);
			outline_color = actor->GetParam("_float3_SilhouetteColor", outline_color);
		}
#pragma endregion

#pragma region GPU resource updates
		VmObject* tobj_otf = (VmObject*)actor->GetAssociateRes("OTF"); // essential!
		if (is_xray_mode) {
			VmObject* tobj_windowing = (VmObject*)actor->GetAssociateRes("WINDOWING");
			if (tobj_windowing)
				tobj_otf = tobj_windowing;
		}
		MapTable* tmap_data = tobj_otf->GetObjParamPtr<MapTable>("_TableMap_OTF");

		if (tobj_otf == NULL)
		{
			VMERRORMESSAGE("NOT ASSIGNED OTF");
			continue;
		}

		VmVObjectVolume* mask_vol_obj = (VmVObjectVolume*)actor->GetAssociateRes("MASKVOLUME");

		if (mask_vol_obj != NULL)
		{
			if (ray_cast_type == __RM_VISVOLMASK) {
				vobj = mask_vol_obj;
				mask_vol_obj = NULL;
				vol_data = vobj->GetVolumeData();
			}
			else {
				GpuRes gres_mask_vol;
				grd_helper::UpdateVolumeModel(gres_mask_vol, mask_vol_obj, true);
				dx11DeviceImmContext->CSSetShaderResources(2, 1, (__SRV_PTR*)&gres_mask_vol.alloc_res_ptrs[DTYPE_SRV]);
			}
		}
		else if (ray_cast_type == __RM_MULTIOTF || ray_cast_type == __RM_VISVOLMASK) {
			ray_cast_type = __RM_DEFAULT;
		}

		GpuRes gres_vol;
		grd_helper::UpdateVolumeModel(gres_vol, vobj, ray_cast_type == __RM_VISVOLMASK, planeThickness <= 0, progress); // ray_cast_type == __RM_MAXMASK
		dx11DeviceImmContext->CSSetShaderResources(0, 1, (__SRV_PTR*)&gres_vol.alloc_res_ptrs[DTYPE_SRV]);

		GpuRes gres_tmap_otf, gres_tmap_preintotf;
		grd_helper::UpdateTMapBuffer(gres_tmap_otf, tobj_otf, false);
		grd_helper::UpdateTMapBuffer(gres_tmap_preintotf, tobj_otf, true);
		dx11DeviceImmContext->CSSetShaderResources(3, 1, (__SRV_PTR*)&gres_tmap_otf.alloc_res_ptrs[DTYPE_SRV]);
		dx11DeviceImmContext->CSSetShaderResources(13, 1, (__SRV_PTR*)&gres_tmap_preintotf.alloc_res_ptrs[DTYPE_SRV]);

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
			grd_helper::UpdateOtfBlocks(gres_volblk_otf, vobj, mask_vol_obj, tobj_otf, sculpt_index); // this tagged mask volume is always used even when MIP mode
			volblk_srv = (__SRV_PTR)gres_volblk_otf.alloc_res_ptrs[DTYPE_SRV];
		}
		dx11DeviceImmContext->CSSetShaderResources(1, 1, (__SRV_PTR*)&volblk_srv);

		CB_VolumeObject cbVolumeObj;
		grd_helper::SetCb_VolumeObj(cbVolumeObj, vobj, actor->matWS2OS, gres_vol, tmap_data->valid_min_idx.x, gres_volblk.options["FORMAT"] == DXGI_FORMAT_R16_UNORM ? 65535.f : 255.f, samplePrecisionLevel, is_xray_mode);
		if (is_modulation_mode && ((uint)vol_data->vol_size.x * (uint)vol_data->vol_size.y * (uint)vol_data->vol_size.z > 1000000)) {
			//cbVolumeObj.opacity_correction *= 2.f;
			//cbVolumeObj.sample_dist *= 2.f;
			//cbVolumeObj.vec_grad_x *= 2.f;
			//cbVolumeObj.vec_grad_y *= 2.f;
			//cbVolumeObj.vec_grad_z *= 2.f;
		}
		cbVolumeObj.pb_shading_factor = material_phongCoeffs;
		cbVolumeObj.outline_color = (uint)(outline_color.r * 255.f) | ((uint)(outline_color.g * 255.f) << 8) | ((uint)(outline_color.b * 255.f) << 16) | (uint)(outline_thickness << 24);
		if (is_ghost_mode) {
			bool is_ghost_surface = actor->GetParam("_bool_IsGhostSurface", false);
			bool is_only_hotspot_visible = actor->GetParam("_bool_IsOnlyHotSpotVisible", false);
			if (is_ghost_surface) cbVolumeObj.vobj_flag |= 0x1 << 19;
			if (is_only_hotspot_visible) cbVolumeObj.vobj_flag |= 0x1 << 20;
			//cout << "TEST : " << is_ghost_surface << ", " << is_only_hotspot_visible << endl;
		}
		else if (is_modulation_mode) {
			cbVolumeObj.grad_max = grad_minmax.y;
			cbVolumeObj.grad_scale = actor->GetParam("_float_ModulationGradScale", 1.f);
			cbVolumeObj.kappa_i = actor->GetParam("_float_ModulationKappai", 1.f);
			cbVolumeObj.kappa_s = actor->GetParam("_float_ModulationKappas", 1.f);
		}
		if (mask_vol_obj) {
			VolumeData* mask_vol_data = mask_vol_obj->GetVolumeData();
			if (mask_vol_data->store_dtype.type_bytes == data_type::dtype<byte>().type_bytes) // char
				cbVolumeObj.mask_value_range = 255.f;
			else if (mask_vol_data->store_dtype.type_bytes == data_type::dtype<ushort>().type_bytes) // short
				cbVolumeObj.mask_value_range = 65535.f;
			else GMERRORMESSAGE("UNSUPPORTED FORMAT : MASK VOLUME");
		}
		cbVolumeObj.sample_dist = sample_dist;
		D3D11_MAPPED_SUBRESOURCE mappedResVolObj;
		dx11DeviceImmContext->Map(cbuf_vobj, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResVolObj);
		CB_VolumeObject* cbVolumeObjData = (CB_VolumeObject*)mappedResVolObj.pData;
		memcpy(cbVolumeObjData, &cbVolumeObj, sizeof(CB_VolumeObject));
		dx11DeviceImmContext->Unmap(cbuf_vobj, 0);
		dx11DeviceImmContext->CSSetConstantBuffers(4, 1, &cbuf_vobj);

		CB_ClipInfo cbClipInfo;
		grd_helper::SetCb_ClipInfo(cbClipInfo, vobj, actor);
		D3D11_MAPPED_SUBRESOURCE mappedResClipInfo;
		dx11DeviceImmContext->Map(cbuf_clip, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResClipInfo);
		CB_ClipInfo* cbClipInfoData = (CB_ClipInfo*)mappedResClipInfo.pData;
		memcpy(cbClipInfoData, &cbClipInfo, sizeof(CB_ClipInfo));
		dx11DeviceImmContext->Unmap(cbuf_clip, 0);
		dx11DeviceImmContext->CSSetConstantBuffers(2, 1, &cbuf_clip);

		CB_TMAP cbTmap;
		grd_helper::SetCb_TMap(cbTmap, tobj_otf);
		D3D11_MAPPED_SUBRESOURCE mappedResOtf;
		dx11DeviceImmContext->Map(cbuf_tmap, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResOtf);
		CB_TMAP* cbTmapData = (CB_TMAP*)mappedResOtf.pData;
		memcpy(cbTmapData, &cbTmap, sizeof(CB_TMAP));
		dx11DeviceImmContext->Unmap(cbuf_tmap, 0);
		dx11DeviceImmContext->CSSetConstantBuffers(5, 1, &cbuf_tmap);

		CB_Material cbRndEffect;
		grd_helper::SetCb_RenderingEffect(cbRndEffect, actor);
		D3D11_MAPPED_SUBRESOURCE mappedResRdnEffect;
		dx11DeviceImmContext->Map(cbuf_reffect, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResRdnEffect);
		CB_Material* cbRndEffectData = (CB_Material*)mappedResRdnEffect.pData;
		memcpy(cbRndEffectData, &cbRndEffect, sizeof(CB_Material));
		dx11DeviceImmContext->Unmap(cbuf_reffect, 0);
		dx11DeviceImmContext->CSSetConstantBuffers(6, 1, &cbuf_vreffect);

		CB_VolumeMaterial cbVrEffect;
		grd_helper::SetCb_VolumeRenderingEffect(cbVrEffect, vobj, actor);
		D3D11_MAPPED_SUBRESOURCE mappedVrEffect;
		dx11DeviceImmContext->Map(cbuf_vreffect, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedVrEffect);
		CB_VolumeMaterial* cbVrEffectData = (CB_VolumeMaterial*)mappedVrEffect.pData;
		memcpy(cbVrEffectData, &cbVrEffect, sizeof(CB_VolumeMaterial));
		dx11DeviceImmContext->Unmap(cbuf_vreffect, 0);
		dx11DeviceImmContext->CSSetConstantBuffers(3, 1, &cbuf_reffect);
#pragma endregion 

#pragma region GPU resource updates
#pragma endregion 

#pragma region Renderer
		// LATER... MFR_MODE::DYNAMIC_KB ==> DEPRECATED in VR
		// MFR_MODE::DYNAMIC_KB also uses static K buffer in VR
		ID3D11ComputeShader* cshader = NULL;
		switch (ray_cast_type)
		{
		case __RM_RAYMIN: cshader = GETCS(PanoVR_RAYMIN_cs_5_0); break;
		case __RM_RAYMAX: cshader = GETCS(PanoVR_RAYMAX_cs_5_0); break;
		case __RM_RAYSUM: cshader = GETCS(PanoVR_RAYSUM_cs_5_0); break;
		case __RM_DEFAULT: cshader = GETCS(PanoVR_DEFAULT_cs_5_0); break;
		case __RM_MODULATION: cshader = GETCS(PanoVR_MODULATE_cs_5_0); break;
		case __RM_MULTIOTF: cshader = GETCS(PanoVR_MULTIOTF_DEFAULT_cs_5_0); break;
		case __RM_MULTIOTF_MODULATION: cshader = GETCS(PanoVR_MULTIOTF_MODULATE_cs_5_0); break;
		case __RM_CLIPOPAQUE:
		case __RM_OPAQUE:
		case __RM_VISVOLMASK:
		case __RM_SCULPTMASK:
		default:
			VMERRORMESSAGE("NOT SUPPORT RAY CASTER!"); return false;
		}

		ID3D11UnorderedAccessView* dx11UAVs[4] = {
				  (ID3D11UnorderedAccessView*)gres_fb_counter.alloc_res_ptrs[DTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_fb_k_buffer.alloc_res_ptrs[DTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_UAV]
		};
		dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 4, dx11UAVs, (UINT*)(&dx11UAVs));

		dx11DeviceImmContext->CSSetShader(cshader, NULL, 0);
		//dx11DeviceImmContext->Flush();
		dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);
#ifdef __DX_DEBUG_QUERY
		dx11CommonParams->debug_info_queue->PushEmptyStorageFilter();
#endif
		if (fastRender2x) {
			dx11DeviceImmContext->CSSetShader(GETCS(FillDither_cs_5_0), NULL, 0);
			dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);
		}
		count_call_render++;

		dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 4, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
		dx11DeviceImmContext->CSSetShaderResources(6, 1, dx11SRVs_NULL);
		dx11DeviceImmContext->CSSetUnorderedAccessViews(50, 1, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
#pragma endregion 
	}

	//dx11DeviceImmContext->Flush();
	//printf("# Textures : %d, # Drawing : %d, # RTBuffer Change : %d, # Merging : %d\n", iNumTexureLayers, iCountRendering, iCountRTBuffers, iCountMerging);
	
	___GpuProfile("VR Begin", true);
	
	bool is_system_out = true;
	// APPLY HWND MODE
	HWND hWnd = (HWND)_fncontainer->fnParams.GetParam("_hwnd_WindowHandle", (HWND)NULL);
	if (is_system_out && hWnd)
	{
		ID3D11Texture2D* pTex2dHwndRT = NULL;
		ID3D11RenderTargetView* pHwndRTV = NULL;
		gpu_manager->UpdateDXGI((void**)&pTex2dHwndRT, (void**)&pHwndRTV, hWnd, fb_size_cur.x, fb_size_cur.y);

		dx11DeviceImmContext->CopyResource(pTex2dHwndRT, (ID3D11Texture2D*)gres_fb_rgba.alloc_res_ptrs[DTYPE_RES]);

		is_system_out = false;
	}
	if (is_system_out)
	{
		FrameBuffer* fb_rout = (FrameBuffer*)iobj->GetFrameBuffer(FrameBufferUsageRENDEROUT, 0);
		FrameBuffer* fb_dout = (FrameBuffer*)iobj->GetFrameBuffer(FrameBufferUsageDEPTH, 0);

		if (count_call_render == 0)	// this means that there is no valid rendering pass
		{
			vmbyte4* rgba_buf = (vmbyte4*)fb_rout->fbuffer;
			float* depth_buf = (float*)fb_dout->fbuffer;

			memset(rgba_buf, 0, fb_size_cur.x * fb_size_cur.y * sizeof(vmbyte4));
			memset(depth_buf, 0x77, fb_size_cur.x * fb_size_cur.y * sizeof(float));
		}
		else
		{
#pragma region // Copy GPU to CPU
			dx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gres_fb_sys_rgba.alloc_res_ptrs[DTYPE_RES],
				(ID3D11Texture2D*)gres_fb_rgba.alloc_res_ptrs[DTYPE_RES]);
			dx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gres_fb_sys_depthcs.alloc_res_ptrs[DTYPE_RES],
				(ID3D11Texture2D*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_RES]);

			vmbyte4* rgba_sys_buf = (vmbyte4*)fb_rout->fbuffer;
			float* depth_sys_buf = (float*)fb_dout->fbuffer;

			D3D11_MAPPED_SUBRESOURCE mappedResSysRGBA;
			D3D11_MAPPED_SUBRESOURCE mappedResSysDepth;
			HRESULT hr = dx11DeviceImmContext->Map((ID3D11Texture2D*)gres_fb_sys_rgba.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, NULL, &mappedResSysRGBA);
			hr = dx11DeviceImmContext->Map((ID3D11Texture2D*)gres_fb_sys_depthcs.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, NULL, &mappedResSysDepth);

			vmbyte4* rgba_gpu_buf = (vmbyte4*)mappedResSysRGBA.pData;
			float* depth_gpu_buf = (float*)mappedResSysDepth.pData;
			if (rgba_gpu_buf == NULL || depth_gpu_buf == NULL)
			{
#ifdef __DX_DEBUG_QUERY
				dx11CommonParams->debug_info_queue->PushEmptyStorageFilter();

				UINT64 message_count = dx11CommonParams->debug_info_queue->GetNumStoredMessages();

				for (UINT64 i = 0; i < message_count; i++) {
					SIZE_T message_size = 0;
					dx11CommonParams->debug_info_queue->GetMessage(i, nullptr, &message_size); //get the size of the message

					D3D11_MESSAGE* message = (D3D11_MESSAGE*)malloc(message_size); //allocate enough space
					dx11CommonParams->debug_info_queue->GetMessage(i, message, &message_size); //get the actual message

					//do whatever you want to do with it
					printf("Directx11: %.*s", message->DescriptionByteLength, message->pDescription);

					free(message);
				}
				dx11CommonParams->debug_info_queue->ClearStoredMessages();
#endif
				test_out("VR ERROR -- OUT");
				test_out("screen : " + to_string(fb_size_cur.x) + " x " + to_string(fb_size_cur.y));
				test_out("v_thickness : " + to_string(v_thickness));
				test_out("k_value : " + to_string(k_value));
				test_out("grid width and height : " + to_string(num_grid_x) + " x " + to_string(num_grid_y));
			}
			int buf_row_pitch = mappedResSysRGBA.RowPitch / 4;
#ifdef PPL_USE
			int count = fb_size_cur.y;
			parallel_for(int(0), count, [is_rgba, fb_size_cur, rgba_sys_buf, depth_sys_buf, rgba_gpu_buf, depth_gpu_buf, buf_row_pitch](int i)
#else
			//#pragma omp parallel for 
			for (int i = 0; i < fb_size_cur.y; i++)
#endif
			{
				for (int j = 0; j < fb_size_cur.x; j++)
				{
					vmbyte4 rgba = rgba_gpu_buf[j + i * buf_row_pitch];
					// __PS_MERGE_LAYERS_TO_RENDEROUT 에서 INT -> FLOAT4 로 되어 배열된 color 요소가 들어 옴. //

					// BGRA
					if (is_rgba)
						rgba_sys_buf[i * fb_size_cur.x + j] = vmbyte4(rgba.x, rgba.y, rgba.z, rgba.w);
					else
						rgba_sys_buf[i * fb_size_cur.x + j] = vmbyte4(rgba.z, rgba.y, rgba.x, rgba.w);

					int iAddr = i * fb_size_cur.x + j;
					if (rgba.w > 0)
						depth_sys_buf[iAddr] = depth_gpu_buf[j + i * buf_row_pitch];
					else
						depth_sys_buf[iAddr] = FLT_MAX;
				}
#ifdef PPL_USE
			});
#else
		}
#endif
			dx11DeviceImmContext->Unmap((ID3D11Texture2D*)gres_fb_sys_rgba.alloc_res_ptrs[DTYPE_RES], 0);
			dx11DeviceImmContext->Unmap((ID3D11Texture2D*)gres_fb_sys_depthcs.alloc_res_ptrs[DTYPE_RES], 0);
			test_out("COPY END STEP !!");
#pragma endregion
	}	// if (iCountDrawing == 0)
}
	if (gpu_profile)
	{
		dx11DeviceImmContext->End(dx11CommonParams->dx11qr_disjoint);

		// Wait for data to be available
		while (dx11DeviceImmContext->GetData(dx11CommonParams->dx11qr_disjoint, NULL, 0, 0) == S_FALSE)
		{
			Sleep(1);       // Wait a bit, but give other threads a chance to run
		}

		// Check whether timestamps were disjoint during the last frame
		D3D10_QUERY_DATA_TIMESTAMP_DISJOINT tsDisjoint;
		dx11DeviceImmContext->GetData(dx11CommonParams->dx11qr_disjoint, &tsDisjoint, sizeof(tsDisjoint), 0);
		if (!tsDisjoint.Disjoint)
		{
			auto DisplayDuration = [&tsDisjoint](UINT64 tsS, UINT64 tsE, const string& _test)
			{
				if (tsS == 0 || tsE == 0) return;
				cout << _test << " : " << float(tsE - tsS) / float(tsDisjoint.Frequency) * 1000.0f << " ms" << endl;
			};

			for (auto& it : profile_map) {
				UINT64 ts, te;
				dx11DeviceImmContext->GetData(dx11CommonParams->dx11qr_timestamps[it.second.x], &ts, sizeof(UINT64), 0);
				dx11DeviceImmContext->GetData(dx11CommonParams->dx11qr_timestamps[it.second.y], &te, sizeof(UINT64), 0);

				DisplayDuration(ts, te, it.first);
			}

			//if (test_fps_profiling)
			//{
			//	auto it = profile_map.find("SR Render");
			//	UINT64 ts, te;
			//	dx11DeviceImmContext->GetData(dx11CommonParams->dx11qr_timestamps[it->second.x], &ts, sizeof(UINT64), 0);
			//	dx11DeviceImmContext->GetData(dx11CommonParams->dx11qr_timestamps[it->second.y], &te, sizeof(UINT64), 0);
			//	ofstream file_rendertime;
			//	file_rendertime.open(".\\data\\frames_profile_rendertime.txt", std::ios_base::app);
			//	file_rendertime << float(te - ts) / float(tsDisjoint.Frequency) * 1000.0f << endl;
			//	file_rendertime.close();
			//}
		}
	}

	dx11DeviceImmContext->ClearState();

	ID3D11RenderTargetView* pdxRTVOlds[2];
	pdxRTVOlds[0] = pdxRTVOld;
	pdxRTVOlds[1] = NULL;
	dx11DeviceImmContext->OMSetRenderTargets(2, pdxRTVOlds, pdxDSVOld);
	VMSAFE_RELEASE(pdxRTVOld);
	VMSAFE_RELEASE(pdxDSVOld);

	iobj->SetDescriptor("vismtv_inbuilt_renderergpudx module : Volume Renderer");
	/**/
	return true;
}