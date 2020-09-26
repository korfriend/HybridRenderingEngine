#include "RendererHeader.h"

#include <iostream>

using namespace grd_helper;

extern void ComputeSSAO(__ID3D11DeviceContext* dx11DeviceImmContext,
	grd_helper::GpuDX11CommonParameters* dx11CommonParams, int num_grid_x, int num_grid_y,
	GpuRes& gres_fb_counter, GpuRes& gres_fb_deep_k_buffer, GpuRes& gres_fb_rgba, bool blur_SSAO,
	GpuRes(&gres_fb_mip_z_halftexs)[2], GpuRes(&gres_fb_mip_a_halftexs)[2],
	GpuRes(&gres_fb_ao_texs)[2], GpuRes(&gres_fb_ao_blf_texs)[2],
	GpuRes& gres_fb_vr_depth, GpuRes& gres_fb_vr_ao, GpuRes& gres_fb_vr_ao_blf, bool involve_vr);

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

	// Get VXObjects //
	vector<VmObject*> input_vobjs;
	_fncontainer->GetVmObjectList(&input_vobjs, VmObjKey(ObjectTypeVOLUME, true));
	vector<VmObject*> input_tobjs;
	_fncontainer->GetVmObjectList(&input_tobjs, VmObjKey(ObjectTypeTMAP, true));

	VmLObject* lobj = (VmLObject*)_fncontainer->GetVmObject(VmObjKey(ObjectTypeBUFFER, true), 0);
	VmIObject* iobj = (VmIObject*)_fncontainer->GetVmObject(VmObjKey(ObjectTypeIMAGEPLANE, false), 0);

	// Check Parameters //
	vmdouble4 global_light_factors = _fncontainer->GetParamValue("_double4_ShadingFactorsForGlobalPrimitives", vmdouble4(0.4, 0.6, 0.2, 30)); // Emission, Diffusion, Specular, Specular Power
	bool force_to_update_otf = _fncontainer->GetParamValue("_bool_ForceToUpdateOtf", false);
	bool show_block_test = _fncontainer->GetParamValue("_bool_IsShowBlock", false);
	double v_thickness = _fncontainer->GetParamValue("_double_VZThickness", 0.0);
	float merging_beta = (float)_fncontainer->GetParamValue("_double_MergingBeta", 0.5);
	bool is_rgba = _fncontainer->GetParamValue("_bool_IsRGBA", false); // false means bgra
	bool is_ghost_mode = _fncontainer->GetParamValue("_bool_GhostEffect", false);
	bool blur_SSAO = _fncontainer->GetParamValue("_bool_BlurSSAO", true);
	bool without_sr = _fncontainer->GetParamValue("_bool_IsFirstRenderer", false);
	bool test_consoleout = _fncontainer->GetParamValue("_bool_TestConsoleOut", false);
	auto test_out = [&test_consoleout](const string& _message)
	{
		if(test_consoleout)
			cout << _message << endl;
	};

	bool print_out_routine_objs = _fncontainer->GetParamValue("_bool_PrintOutRoutineObjs", false);
	bool gpu_profile = false;
	if (print_out_routine_objs)
	{
		gpu_profile = _fncontainer->GetParamValue("_bool_GpuProfile", false);
	}

#define __DTYPE(type) data_type::dtype<type>()
	auto Get_LCParam = [&lobj](const string& name, data_type dtype, auto default_value)
	{
		auto vret = default_value;
		lobj->GetCustomParameter(name, dtype, &vret);
		return vret;
	};

	int k_value_old = Get_LCParam("_int_NumK", __DTYPE(int), (int)8);
	int k_value = _fncontainer->GetParamValue("_int_NumK", k_value_old);
	lobj->RegisterCustomParameter("_int_NumK", k_value);
	MFR_MODE mode_OIT = (MFR_MODE)_fncontainer->GetParamValue("_int_OitMode", (int)0);
	int buf_ex_scale = _fncontainer->GetParamValue("_int_BufExScale", (int)4); // 32 layers
	int num_moments_old = 4;
	lobj->GetCustomParameter("_int_NumQueueLayers", data_type::dtype<int>(), &num_moments_old);
	int num_moments = _fncontainer->GetParamValue("_int_NumQueueLayers", num_moments_old);

	// TEST
	int test_value = _fncontainer->GetParamValue("_int_TestValue", (int)0);
	int test_mode = _fncontainer->GetParamValue("_int_TestMode", (int)0);

#pragma region // SHADER SETTING
	// Shader Re-Compile Setting //
	bool recompile_hlsl = _fncontainer->GetParamValue("_bool_ReloadHLSLObjFiles", false);
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
		hlslobj_path += "..\\..\\VmProjects\\hybrid_rendering_engine\\shader_compiled_objs\\";
		//cout << hlslobj_path << endl;

		string prefix_path = hlslobj_path;

#define CS_NUM 7
#define SET_CS(NAME) dx11CommonParams->safe_set_res(grd_helper::COMRES_INDICATOR(COMPUTE_SHADER, NAME), dx11CShader, true)

		string strNames_CS[CS_NUM] = {
			   "VR_RAYMAX_cs_5_0"
			  ,"VR_RAYMIN_cs_5_0"
			  ,"VR_RAYSUM_cs_5_0"
			  ,"VR_DEFAULT_cs_5_0"
			  ,"VR_OPAQUE_cs_5_0"
			  ,"VR_CONTEXT_cs_5_0"
			  ,"VR_SURFACE_cs_5_0"
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
#pragma endregion // SHADER SETTING

#pragma region // IOBJECT OUT
	while (iobj->GetFrameBuffer(FrameBufferUsageRENDEROUT, 1) != NULL)
		iobj->DeleteFrameBuffer(FrameBufferUsageRENDEROUT, 1);
	if (!iobj->ReplaceFrameBuffer(FrameBufferUsageRENDEROUT, 0, data_type::dtype<vmbyte4>(), ("common render out frame buffer : defined in vismtv_inbuilt_renderergpudx module")))
		iobj->InsertFrameBuffer(data_type::dtype<vmbyte4>(), FrameBufferUsageRENDEROUT, ("common render out frame buffer : defined in vismtv_inbuilt_renderergpudx module"));

	while (iobj->GetFrameBuffer(FrameBufferUsageDEPTH, 1) != NULL)
		iobj->DeleteFrameBuffer(FrameBufferUsageDEPTH, 1);
	if (!iobj->ReplaceFrameBuffer(FrameBufferUsageDEPTH, 0, data_type::dtype<float>(), ("1st hit screen depth frame buffer : defined in vismtv_inbuilt_renderergpudx module")))
		iobj->InsertFrameBuffer(data_type::dtype<float>(), FrameBufferUsageDEPTH, ("1st hit screen depth frame buffer : defined in vismtv_inbuilt_renderergpudx module"));
#pragma endregion // IOBJECT OUT

	__ID3D11Device* pdx11Device = dx11CommonParams->dx11Device;
	__ID3D11DeviceContext* dx11DeviceImmContext = dx11CommonParams->dx11DeviceImmContext;

#pragma region // IOBJECT GPU
	int buffer_ex = mode_OIT != MFR_MODE::SKBZT ? buf_ex_scale : 1, buffer_ex_old = 0; // optimal for K is 1
	vmint2 fb_size_cur, fb_size_old = vmint2(0, 0);
	iobj->GetFrameBufferInfo(&fb_size_cur);
	iobj->GetCustomParameter("_int2_PreviousScreenSize", data_type::dtype<vmint2>(), &fb_size_old);
	iobj->GetCustomParameter("_int_PreviousBufferEx", data_type::dtype<int>(), &buffer_ex_old);
	if (fb_size_cur.x != fb_size_old.x || fb_size_cur.y != fb_size_old.y || k_value != k_value_old
		|| k_value != k_value_old || num_moments != num_moments_old
		|| buffer_ex != buffer_ex_old)
	{
		gpu_manager->ReleaseGpuResourcesBySrcID(iobj->GetObjectID());	// System Out 포함 //
		iobj->RegisterCustomParameter("_int2_PreviousScreenSize", fb_size_cur);
	}

	GpuRes gres_fb_rgba, gres_fb_depthcs;
	GpuRes gres_fb_deep_k_buffer, gres_fb_counter;
	GpuRes gres_fb_ao_texs[2], gres_fb_ao_blf_texs[2], gres_fb_mip_a_halftexs[2], gres_fb_mip_z_halftexs[2]; // max_layers
	GpuRes gres_fb_ao_vr_tex, gres_fb_ao_vr_blf_tex;
	GpuRes gres_fb_sys_rgba, gres_fb_sys_depthcs;
	GpuRes gres_fb_ref_pidx;

	grd_helper::UpdateFrameBuffer(gres_fb_rgba, iobj, "RENDER_OUT_RGBA_0", RTYPE_TEXTURE2D,
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
	grd_helper::UpdateFrameBuffer(gres_fb_depthcs, iobj, "RENDER_OUT_DEPTH_0", RTYPE_TEXTURE2D,
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_FLOAT, 0);
	grd_helper::UpdateFrameBuffer(gres_fb_counter, iobj, "RW_COUNTER", RTYPE_TEXTURE2D,
		D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_UINT, 0);

	grd_helper::UpdateFrameBuffer(gres_fb_deep_k_buffer, iobj, "BUFFER_RW_K_BUF", RTYPE_BUFFER,
		D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_TYPELESS, UPFB_RAWBYTE, k_value * 4 * buffer_ex);

	grd_helper::UpdateFrameBuffer(gres_fb_sys_rgba, iobj, "SYSTEM_OUT_RGBA", RTYPE_TEXTURE2D, NULL, DXGI_FORMAT_R8G8B8A8_UNORM, UPFB_SYSOUT);
	grd_helper::UpdateFrameBuffer(gres_fb_sys_depthcs, iobj, "SYSTEM_OUT_DEPTH", RTYPE_TEXTURE2D, NULL, DXGI_FORMAT_R32_FLOAT, UPFB_SYSOUT);

	// AO
	{
		grd_helper::UpdateFrameBuffer(gres_fb_ao_texs[0], iobj, "RW_TEXS_AO_0", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, UPFB_MIPMAP);
		grd_helper::UpdateFrameBuffer(gres_fb_ao_texs[1], iobj, "RW_TEXS_AO_1", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, UPFB_MIPMAP);
		grd_helper::UpdateFrameBuffer(gres_fb_ao_blf_texs[0], iobj, "RW_TEXS_AO_BLF_0", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, UPFB_MIPMAP);
		grd_helper::UpdateFrameBuffer(gres_fb_ao_blf_texs[1], iobj, "RW_TEXS_AO_BLF_1", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, UPFB_MIPMAP);
		grd_helper::UpdateFrameBuffer(gres_fb_mip_a_halftexs[0], iobj, "RW_TEXS_OPACITY_0", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, UPFB_MIPMAP | UPFB_HALF);
		grd_helper::UpdateFrameBuffer(gres_fb_mip_a_halftexs[1], iobj, "RW_TEXS_OPACITY_1", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, UPFB_MIPMAP | UPFB_HALF);
		grd_helper::UpdateFrameBuffer(gres_fb_mip_z_halftexs[0], iobj, "RW_TEXS_Z_0", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32G32B32A32_FLOAT, UPFB_MIPMAP | UPFB_HALF);
		grd_helper::UpdateFrameBuffer(gres_fb_mip_z_halftexs[1], iobj, "RW_TEXS_Z_1", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32G32B32A32_FLOAT, UPFB_MIPMAP | UPFB_HALF);

		grd_helper::UpdateFrameBuffer(gres_fb_ao_vr_tex, iobj, "RW_TEX_AO_VR", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8_UNORM, UPFB_MIPMAP);
		grd_helper::UpdateFrameBuffer(gres_fb_ao_vr_blf_tex, iobj, "RW_TEX_AO_VR_BLF", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8_UNORM, UPFB_MIPMAP);
	}

	if (mode_OIT == MFR_MODE::DXAB || mode_OIT == MFR_MODE::DKBZT)
		grd_helper::UpdateFrameBuffer(gres_fb_ref_pidx, iobj, "BUFFER_RW_REF_PIDX_BUF", RTYPE_BUFFER,
			D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_UINT, 0);
#pragma endregion // IOBJECT GPU

#pragma region // Presetting of VxObject
	int last_render_vol_id = _fncontainer->GetParamValue("_int_LastRenderVolumeID", input_vobjs[input_vobjs.size() - 1]->GetObjectID());

	map<int, VmVObjectVolume*> mapVolumes;
	for (int i = 0; i < (int)input_vobjs.size(); i++)
	{
		VmVObjectVolume* pCVolume = (VmVObjectVolume*)input_vobjs[i];
		mapVolumes.insert(pair<int, VmVObjectVolume*>(pCVolume->GetObjectID(), pCVolume));
	}

	map<int, VmTObject*> mapTObjects;
	for (int i = 0; i < (int)input_tobjs.size(); i++)
	{
		VmTObject* pCTObject = (VmTObject*)input_tobjs[i];
		mapTObjects.insert(pair<int, VmTObject*>(pCTObject->GetObjectID(), pCTObject));
	}

	auto Get_Lbuffer = [&lobj](const string& name, auto** pp, int& num_elements)
	{
		size_t bytes_temp;
		lobj->LoadBufferPtr("_vlist_INT_MainVolumeIDs", (void**)pp, bytes_temp);
		num_elements = voo::get_num_from_bytes<int>(bytes_temp);
	};

	int* main_volume_ids = NULL; int num_main_volumes = 0;
	Get_Lbuffer("_vlist_INT_MainVolumeIDs", &main_volume_ids, num_main_volumes);
	if (num_main_volumes == 0)
	{
		VMERRORMESSAGE("GPU DVR! - ERROR 00");
		return false;
	}

	vector<int> ordered_main_volume_ids;
	bool is_valid_list = false;
	vmfloat3 pos_aabb_min_ws(FLT_MAX), pos_aabb_max_ws(-FLT_MAX);
	float min_pitch = FLT_MAX;
	for (int i = 0; i < num_main_volumes; i++)
	{
		int vobj_id = main_volume_ids[i];
		auto it = mapVolumes.find(vobj_id);
		if (it == mapVolumes.end())
		{
			VMERRORMESSAGE("GPU DVR! - INVALID VOLUME ID");
			return false;
		}
		if (vobj_id == last_render_vol_id)
		{
			is_valid_list = true;
		}
		else
		{
			ordered_main_volume_ids.push_back(vobj_id);
		}
		AaBbMinMax aabb_os;
		it->second->GetOrthoBoundingBox(aabb_os);
		vmmat44f matOS2WS = it->second->GetMatrixOS2WSf();
		vmfloat3 min_aabb, max_aabb;
		fTransformPoint(&min_aabb, &(vmfloat3)aabb_os.pos_min, &matOS2WS);
		fTransformPoint(&max_aabb, &(vmfloat3)aabb_os.pos_max, &matOS2WS);
		pos_aabb_min_ws.x = min(pos_aabb_min_ws.x, min_aabb.x);
		pos_aabb_min_ws.y = min(pos_aabb_min_ws.y, min_aabb.y);
		pos_aabb_min_ws.z = min(pos_aabb_min_ws.z, min_aabb.z);
		pos_aabb_max_ws.x = max(pos_aabb_max_ws.x, max_aabb.x);
		pos_aabb_max_ws.y = max(pos_aabb_max_ws.y, max_aabb.y);
		pos_aabb_max_ws.z = max(pos_aabb_max_ws.z, max_aabb.z);

		min_pitch = min(min(min(it->second->GetVolumeData()->vox_pitch.x, it->second->GetVolumeData()->vox_pitch.y), it->second->GetVolumeData()->vox_pitch.z), min_pitch);
	}
	if (!is_valid_list)
	{
		VMERRORMESSAGE("GPU DVR! - ERROR 01");
		return false;
	}
	ordered_main_volume_ids.push_back(last_render_vol_id);

#pragma endregion // Presetting of VxObject

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
		//dx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gres_fb_deep_k_buffer.alloc_res_ptrs[DTYPE_UAV], clr_unit4);
		dx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_UAV], clr_unit4);
		dx11DeviceImmContext->ClearUnorderedAccessViewFloat((ID3D11UnorderedAccessView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_UAV], clr_float_fltmax_4);
	}

	ID3D11Buffer* cbuf_cam_state = dx11CommonParams->get_cbuf("CB_CameraState");
	ID3D11Buffer* cbuf_env_state = dx11CommonParams->get_cbuf("CB_EnvState");
	ID3D11Buffer* cbuf_clip = dx11CommonParams->get_cbuf("CB_ClipInfo");
	ID3D11Buffer* cbuf_pobj = dx11CommonParams->get_cbuf("CB_PolygonObject");
	ID3D11Buffer* cbuf_vobj = dx11CommonParams->get_cbuf("CB_VolumeObject");
	ID3D11Buffer* cbuf_reffect = dx11CommonParams->get_cbuf("CB_RenderingEffect");
	ID3D11Buffer* cbuf_vreffect = dx11CommonParams->get_cbuf("CB_VolumeRenderingEffect");
	ID3D11Buffer* cbuf_tmap = dx11CommonParams->get_cbuf("CB_TMAP");
	ID3D11Buffer* cbuf_hsmask = dx11CommonParams->get_cbuf("CB_HotspotMask");

	ID3D11SamplerState* sampler_PZ = dx11CommonParams->get_sampler("POINT_ZEROBORDER");
	ID3D11SamplerState* sampler_LZ = dx11CommonParams->get_sampler("LINEAR_ZEROBORDER");
	ID3D11SamplerState* sampler_PC = dx11CommonParams->get_sampler("POINT_CLAMP");
	ID3D11SamplerState* sampler_LC = dx11CommonParams->get_sampler("LINEAR_CLAMP");
	dx11DeviceImmContext->CSSetSamplers(0, 1, &sampler_LZ);
	dx11DeviceImmContext->CSSetSamplers(1, 1, &sampler_PZ);
	dx11DeviceImmContext->CSSetSamplers(2, 1, &sampler_LC);
	dx11DeviceImmContext->CSSetSamplers(3, 1, &sampler_PC);

	ID3D11UnorderedAccessView* dx11UAVs_NULL[6] = { NULL, NULL, NULL, NULL, NULL, NULL };
	ID3D11ShaderResourceView* dx11SRVs_NULL[6] = { NULL, NULL, NULL, NULL, NULL, NULL };
#pragma region // Camera & Environment 
// 	const int __BLOCKSIZE = 8;
// 	uint num_grid_x = (uint)ceil(fb_size_cur.x / (float)__BLOCKSIZE);
// 	uint num_grid_y = (uint)ceil(fb_size_cur.y / (float)__BLOCKSIZE);
	const int __BLOCKSIZE = _fncontainer->GetParamValue("_int_GpuThreadBlockSize", (int)1);
	uint num_grid_x = __BLOCKSIZE == 1 ? fb_size_cur.x : (uint)ceil(fb_size_cur.x / (float)__BLOCKSIZE);
	uint num_grid_y = __BLOCKSIZE == 1 ? fb_size_cur.y : (uint)ceil(fb_size_cur.y / (float)__BLOCKSIZE);

	VmCObject* cam_obj = iobj->GetCameraObject();
	vmmat44f matWS2SS, matWS2PS, matSS2WS;
	CB_CameraState cbCamState;
	grd_helper::SetCb_Camera(cbCamState, matWS2PS, matWS2SS, matSS2WS, cam_obj, fb_size_cur, k_value, v_thickness <= 0? min_pitch : (float)v_thickness);
	cbCamState.iSrCamDummy__0 = *(uint*)&merging_beta;
	if (mode_OIT == MFR_MODE::DXAB || mode_OIT == MFR_MODE::DKBZT)
		cbCamState.cam_flag |= (0x2 << 1);
	
	D3D11_MAPPED_SUBRESOURCE mappedResCamState;
	dx11DeviceImmContext->Map(cbuf_cam_state, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResCamState);
	CB_CameraState* cbCamStateData = (CB_CameraState*)mappedResCamState.pData;
	memcpy(cbCamStateData, &cbCamState, sizeof(CB_CameraState));
	dx11DeviceImmContext->Unmap(cbuf_cam_state, 0);
	dx11DeviceImmContext->CSSetConstantBuffers(0, 1, &cbuf_cam_state);

	CB_EnvState cbEnvState;
	grd_helper::SetCb_Env(cbEnvState, cam_obj, _fncontainer, (vmfloat3)global_light_factors);
	cbEnvState.env_flag |= 0x2;
	D3D11_MAPPED_SUBRESOURCE mappedResEnvState;
	dx11DeviceImmContext->Map(cbuf_env_state, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResEnvState);
	CB_EnvState* cbEnvStateData = (CB_EnvState*)mappedResEnvState.pData;
	memcpy(cbEnvStateData, &cbEnvState, sizeof(CB_EnvState));
	dx11DeviceImmContext->Unmap(cbuf_env_state, 0);
	dx11DeviceImmContext->CSSetConstantBuffers(7, 1, &cbuf_env_state);

	if (is_ghost_mode)
	{
		D3D11_MAPPED_SUBRESOURCE mappedResHSMask;
		dx11DeviceImmContext->Map(cbuf_hsmask, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResHSMask);
		CB_HotspotMask* cbHSMaskData = (CB_HotspotMask*)mappedResHSMask.pData;
		grd_helper::SetCb_HotspotMask(*cbHSMaskData, _fncontainer, fb_size_old);
		dx11DeviceImmContext->Unmap(cbuf_hsmask, 0);
		dx11DeviceImmContext->CSSetConstantBuffers(9, 1, &cbuf_hsmask);
	}
#pragma endregion // Light & Shadow Setting

	int gpu_profilecount = 0;
	map<string, int> profile_map;
	if (gpu_profile)
	{
		dx11DeviceImmContext->Begin(dx11CommonParams->dx11qr_disjoint);
		dx11DeviceImmContext->End(dx11CommonParams->dx11qr_timestamps[gpu_profilecount]);
		profile_map["begin"] = gpu_profilecount;
		gpu_profilecount++;
	}

	// Initial Setting of Frame Buffers //
	int count_call_render = 0;
	iobj->GetCustomParameter("_int_NumCallRenders", __DTYPE(int), &count_call_render);
	bool is_performed_ssao = false;

	int num_main_vrs = (int)ordered_main_volume_ids.size();
	for (int i = 0; i < num_main_vrs; i++)
	{
		int vobj_id = ordered_main_volume_ids[i];

#pragma region // Volume General Parameters
		VmVObjectVolume* main_vol_obj = NULL;

		// Volumes //
		auto itrVolume = mapVolumes.find(vobj_id);
		if (itrVolume == mapVolumes.end())
		{
			VMERRORMESSAGE("NO Volume ID");
			continue;
		}
		main_vol_obj = itrVolume->second;

		bool is_visible = true;
		lobj->GetDstObjValue(vobj_id, "_bool_IsVisible", &is_visible); // VxFramework
		lobj->GetDstObjValue(vobj_id, "_bool_TestVisible", &is_visible);
		lobj->GetDstObjValue(vobj_id, "_bool_visibility", &is_visible); // VisMotive

		if (!is_visible)
			continue;

#define __RM_DEFAULT 0
#define __RM_CLIPOPAQUE 1
#define __RM_OPAQUE 2
#define __RM_MODULATION 3
#define __RM_SCULPTMASK 4
#define __RM_MAXMASK 5
#define __RM_TEST 6
#define __RM_RAYMAX 10
#define __RM_RAYMIN 11
#define __RM_RAYSUM 12

#define __SRV_PTR ID3D11ShaderResourceView* 

		auto GET_LDVALUE = [&lobj, &vobj_id](const string& name, auto default_v, int id = 0)
		{
			if (id == 0) id = vobj_id;
			auto v = default_v;
			lobj->GetDstObjValue(id, name, &v);
			return v;
		};

		int render_type = GET_LDVALUE("_int_RendererType", (int)0);
		bool is_xray_mode = false;
		if (render_type >= __RM_RAYMAX)
			is_xray_mode = true;

		bool skip_volblk_update = GET_LDVALUE("_bool_ForceToSkipBlockUpdate", false);

		int blk_level = GET_LDVALUE("_int_BlockLevel", (int)1);
		blk_level = max(min(1, blk_level), 0);

		bool use_mask_volume = GET_LDVALUE("_bool_ValidateMaskVolume", false);
		bool update_volblk_sculptmask = GET_LDVALUE("_bool_BlockUpdateForSculptMask", false);
		int main_tobj_id = GET_LDVALUE("_int_MainTObjectID", (int)0);
		int mask_vol_id = GET_LDVALUE("_int_MaskVolumeObjectID", (int)0);
		// TEST 
		int windowing_tobj_id = GET_LDVALUE("_int_WindowingTObjectID", (int)0);
#pragma endregion // Volume General Parameters

#pragma region // Mask Volume Custom Parameters
		VmVObjectVolume* mask_vol_obj = NULL;
		//pCLObjectForParameters->GetCustomDValue(iMaskVolumeID, (void**)&pmapDValueMaskVolume);
		//if (pmapDValueMaskVolume == NULL)
		//	pmapDValueMaskVolume = &mapDValueClear;

		int sculpt_index = -1;
		double* mask_otf_ids = NULL;
		double* mask_otf_ids_vis = NULL;
		double* mask_otf_idmap = NULL;
		int num_mask_otfs = 0;
		if (mask_vol_id != 0 && (use_mask_volume || render_type == __RM_SCULPTMASK))
		{
			auto itrMaskVolume = mapVolumes.find(mask_vol_id);
			if (itrMaskVolume == mapVolumes.end())
			{
				VMERRORMESSAGE("NO Mask Volume ID");
				continue;
			}

			mask_vol_obj = itrMaskVolume->second;
			if (mask_vol_obj == NULL)
			{
				VMERRORMESSAGE("INVALID Mask Volume ID");
				return false;
			}

			sculpt_index = GET_LDVALUE("_int_SculptingIndex", (int)-1);
			Get_Lbuffer("_vlist_DOUBLE_MaskOTFIDs", &mask_otf_ids, num_mask_otfs);
			Get_Lbuffer("_vlist_DOUBLE_MaskOTFIDs_VisibilitySeries", &mask_otf_ids_vis, num_mask_otfs);
			Get_Lbuffer("_vlist_DOUBLE_MaskOTFIndexMap", &mask_otf_idmap, num_mask_otfs);
		}
		bool use_mask_otfs = false;
		if (num_mask_otfs > 0)
		{
			use_mask_otfs = true;
			// Check if Mask Volume Renderer is used or not.
			if (num_mask_otfs == 1 && (int)mask_otf_ids[0] == main_tobj_id)
				use_mask_otfs = false;
		}
#pragma endregion // Mask Volume Custom Parameters

#pragma region // Main TObject Custom Parameters
		////// 여기서 부터 토요일!
		VmTObject* main_tobj = NULL;
		
		auto itrTObject = mapTObjects.find(main_tobj_id);
		if (itrTObject == mapTObjects.end())
		{
			VMERRORMESSAGE("NO TOBJECT ID");
			continue;
		}
		main_tobj = itrTObject->second;
		if (main_tobj->GetTMapData() == NULL)
		{
			VMERRORMESSAGE("NOT ASSIGNED TOBJECT");
			continue;
		}

		bool is_otf_changed = GET_LDVALUE("_bool_IsTfChanged", false, main_tobj_id);
		is_otf_changed |= grd_helper::CheckOtfAndVolBlobkUpdate(main_vol_obj, main_tobj);
		if (force_to_update_otf)
			is_otf_changed = true;
		bool force_to_skip_volblk_update = GET_LDVALUE("_bool_ForceToSkipBlockUpdate", false, main_tobj_id) || skip_volblk_update;

		if (use_mask_otfs)
		{
			for (int j = 0; j < num_mask_otfs; j++)
			{
				bool is_mask_otf_changed = GET_LDVALUE("_bool_IsTfChanged", false, (int)mask_otf_ids[j]);
				is_otf_changed |= is_mask_otf_changed;
			}
		}
#pragma endregion // Main TObject Custom Parameters

#pragma region // Volume Resource Setting 
		GpuRes gres_main_vol;
		grd_helper::UpdateVolumeModel(gres_main_vol, main_vol_obj, render_type == __RM_MAXMASK);
#pragma endregion // Volume Resource Setting 

#pragma region // Mask Volume Resource Setting
		if (mask_vol_obj != NULL)
		{
			GpuRes gres_mask_vol;
			grd_helper::UpdateVolumeModel(gres_mask_vol, mask_vol_obj, true);

			dx11DeviceImmContext->CSSetShaderResources(2, 1, (__SRV_PTR*)&gres_mask_vol.alloc_res_ptrs[DTYPE_SRV]);
		}
#pragma endregion // Mask Volume Resource Setting

#pragma region // TObject Resource Setting 
		GpuRes gres_otf;
		int num_prev_mask_otfs = 0;
		main_tobj->GetCustomParameter("_int_NumOTFSeries", data_type::dtype<int>(), &num_prev_mask_otfs);
		bool bMaskUpdate = false;
		if (num_prev_mask_otfs != num_mask_otfs && use_mask_otfs)
		{
			bMaskUpdate = true;
			gres_otf.vm_src_id = main_tobj->GetObjectID();
			gres_otf.res_name = "OTF_BUFFER";
			
			gpu_manager->ReleaseGpuResource(gres_otf);
			main_tobj->RegisterCustomParameter("_int_NumOTFSeries", num_mask_otfs);
		}
		grd_helper::UpdateTMapBuffer(gres_otf, main_tobj, mapTObjects,
			mask_otf_ids, mask_otf_ids_vis, num_mask_otfs, is_otf_changed);

		// TEST 
		if (test_value == 1 && windowing_tobj_id > 0)
		{
			auto itrTObject = mapTObjects.find(windowing_tobj_id);
			if (itrTObject == mapTObjects.end())
			{
				VMERRORMESSAGE("NO TOBJECT ID");
				continue;
			}

			VmTObject* windowing_tobj = itrTObject->second;

			bool is_windowing_changed = GET_LDVALUE("_bool_IsTfChanged", false, windowing_tobj_id);
			is_windowing_changed |= grd_helper::CheckOtfAndVolBlobkUpdate(main_vol_obj, windowing_tobj);
			is_otf_changed |= is_windowing_changed;

			GpuRes gres_windowing;
			grd_helper::UpdateTMapBuffer(gres_windowing, windowing_tobj, mapTObjects, NULL, NULL, 1, is_windowing_changed);
			dx11DeviceImmContext->CSSetShaderResources(4, 1, (__SRV_PTR*)&gres_windowing.alloc_res_ptrs[DTYPE_SRV]);
		}

		GpuRes gres_otf_series_ids;
		if (use_mask_otfs)
		{
			gres_otf_series_ids.vm_src_id = main_tobj->GetObjectID();
			gres_otf_series_ids.res_name = string("OTF_SERIES_IDS");
			gres_otf_series_ids.rtype = RTYPE_BUFFER;
			gres_otf_series_ids.options["USAGE"] = D3D11_USAGE_DYNAMIC;
			gres_otf_series_ids.options["CPU_ACCESS_FLAG"] = D3D11_CPU_ACCESS_WRITE;
			gres_otf_series_ids.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE;
			gres_otf_series_ids.options["FORMAT"] = DXGI_FORMAT_R32_SINT;
			gres_otf_series_ids.res_dvalues["NUM_ELEMENTS"] = 128;
			gres_otf_series_ids.res_dvalues["STRIDE_BYTES"] = sizeof(int);

			if (bMaskUpdate)
				gpu_manager->ReleaseGpuResource(gres_otf_series_ids);

			if (!gpu_manager->UpdateGpuResource(gres_otf_series_ids))
				gpu_manager->GenerateGpuResource(gres_otf_series_ids);

			D3D11_MAPPED_SUBRESOURCE d11MappedRes;
			dx11DeviceImmContext->Map((ID3D11Resource*)gres_otf_series_ids.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes);
			int* otf_index_data = (int*)d11MappedRes.pData;
			memset(otf_index_data, 0, sizeof(int) * 128);
			for (int j = 0; j < num_mask_otfs; j++)
			{
				int otf_index = static_cast<int>(mask_otf_idmap[j]);
				otf_index_data[otf_index] = j;
			}
			dx11DeviceImmContext->Unmap((ID3D11Resource*)gres_otf_series_ids.alloc_res_ptrs[DTYPE_RES], 0);
		}
#pragma endregion // TObject Resource Setting 

#pragma region // Main Volume Block Mask Setting From OTFs
		VolumeBlocks* vol_blk = main_vol_obj->GetVolumeBlock(blk_level);
		if (vol_blk == NULL)
		{
			main_vol_obj->UpdateVolumeMinMaxBlocks();
			vol_blk = main_vol_obj->GetVolumeBlock(blk_level);
		}

		GpuRes gres_volblk_otf, gres_volblk_min, gres_volblk_max;
		if (is_xray_mode)
			grd_helper::UpdateMinMaxBlocks(gres_volblk_min, gres_volblk_max, main_vol_obj);

		// this is always used even when MIP mode
		grd_helper::UpdateOtfBlocks(gres_volblk_otf, main_vol_obj, mask_vol_obj, mapTObjects, main_tobj->GetObjectID(), mask_otf_idmap, num_mask_otfs,
			(is_otf_changed | update_volblk_sculptmask) && !force_to_skip_volblk_update, use_mask_otfs, sculpt_index);
#pragma endregion // Main Block Mask Setting From OTFs

#pragma region // Constant Buffers
		bool high_samplerate = gres_main_vol.res_dvalues["SAMPLE_OFFSET_X"] > 1.f ||
			gres_main_vol.res_dvalues["SAMPLE_OFFSET_Y"] > 1.f ||
			gres_main_vol.res_dvalues["SAMPLE_OFFSET_Z"] > 1.f;

		if (render_type == __RM_MODULATION && high_samplerate)
			high_samplerate = false;

		CB_VolumeObject cbVolumeObj;
		vmint3 vol_sampled_size = vmint3(gres_main_vol.res_dvalues["WIDTH"], gres_main_vol.res_dvalues["HEIGHT"], gres_main_vol.res_dvalues["DEPTH"]);
		
		int iso_value = GET_LDVALUE("_int_Isovalue", main_tobj->GetTMapData()->valid_min_idx.x);
		grd_helper::SetCb_VolumeObj(cbVolumeObj, main_vol_obj, lobj, _fncontainer, high_samplerate, vol_sampled_size, iso_value, gres_volblk_otf.options["FORMAT"] == DXGI_FORMAT_R16_UNORM ? 65535.f : 1.f, sculpt_index);
		cbVolumeObj.pb_shading_factor = global_light_factors;

		D3D11_MAPPED_SUBRESOURCE mappedResVolObj;
		dx11DeviceImmContext->Map(cbuf_vobj, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResVolObj);
		CB_VolumeObject* cbVolumeObjData = (CB_VolumeObject*)mappedResVolObj.pData;
		memcpy(cbVolumeObjData, &cbVolumeObj, sizeof(CB_VolumeObject));
		dx11DeviceImmContext->Unmap(cbuf_vobj, 0);

		CB_ClipInfo cbClipInfo;
		grd_helper::SetCb_ClipInfo(cbClipInfo, main_vol_obj, lobj);
		D3D11_MAPPED_SUBRESOURCE mappedResClipInfo;
		dx11DeviceImmContext->Map(cbuf_clip, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResClipInfo);
		CB_ClipInfo* cbClipInfoData = (CB_ClipInfo*)mappedResClipInfo.pData;
		memcpy(cbClipInfoData, &cbClipInfo, sizeof(CB_ClipInfo));
		dx11DeviceImmContext->Unmap(cbuf_clip, 0);

		CB_TMAP cbTmap;
		grd_helper::SetCb_TMap(cbTmap, main_tobj);
		D3D11_MAPPED_SUBRESOURCE mappedResOtf;
		dx11DeviceImmContext->Map(cbuf_tmap, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResOtf);
		CB_TMAP* cbTmapData = (CB_TMAP*)mappedResOtf.pData;
		memcpy(cbTmapData, &cbTmap, sizeof(CB_TMAP));
		dx11DeviceImmContext->Unmap(cbuf_tmap, 0);

		RenderObjInfo tmp_render_info;
		CB_RenderingEffect cbRndEffect;
		grd_helper::SetCb_RenderingEffect(cbRndEffect, main_vol_obj, lobj, tmp_render_info);
		D3D11_MAPPED_SUBRESOURCE mappedResRdnEffect;
		dx11DeviceImmContext->Map(cbuf_reffect, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResRdnEffect);
		CB_RenderingEffect* cbRndEffectData = (CB_RenderingEffect*)mappedResRdnEffect.pData;
		memcpy(cbRndEffectData, &cbRndEffect, sizeof(CB_RenderingEffect));
		dx11DeviceImmContext->Unmap(cbuf_reffect, 0);

		CB_VolumeRenderingEffect cbVrEffect;
		grd_helper::SetCb_VolumeRenderingEffect(cbVrEffect, main_vol_obj, lobj);
		D3D11_MAPPED_SUBRESOURCE mappedVrEffect;
		dx11DeviceImmContext->Map(cbuf_vreffect, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedVrEffect);
		CB_VolumeRenderingEffect* cbVrEffectData = (CB_VolumeRenderingEffect*)mappedVrEffect.pData;
		memcpy(cbVrEffectData, &cbVrEffect, sizeof(CB_VolumeRenderingEffect));
		dx11DeviceImmContext->Unmap(cbuf_vreffect, 0);


		dx11DeviceImmContext->CSSetConstantBuffers(2, 1, &cbuf_clip);
		dx11DeviceImmContext->CSSetConstantBuffers(3, 1, &cbuf_reffect);
		dx11DeviceImmContext->CSSetConstantBuffers(4, 1, &cbuf_vobj);
		dx11DeviceImmContext->CSSetConstantBuffers(5, 1, &cbuf_tmap);
		dx11DeviceImmContext->CSSetConstantBuffers(6, 1, &cbuf_vreffect);
#pragma endregion // Constant Buffers

#pragma region // Resource Shader Setting
		dx11DeviceImmContext->CSSetShaderResources(3, 1, (__SRV_PTR*)&gres_otf.alloc_res_ptrs[DTYPE_SRV]);
		if (use_mask_otfs)
			dx11DeviceImmContext->CSSetShaderResources(5, 1, (__SRV_PTR*)&gres_otf_series_ids.alloc_res_ptrs[DTYPE_SRV]);

		__SRV_PTR volblk_srv = NULL;
		if (is_xray_mode)
		{
			if (render_type == __RM_RAYMAX)	// Min
				volblk_srv = (__SRV_PTR)gres_volblk_max.alloc_res_ptrs[DTYPE_SRV];
			else if (render_type == __RM_RAYMIN)
				volblk_srv = (__SRV_PTR)gres_volblk_min.alloc_res_ptrs[DTYPE_SRV];
		}
		else
			volblk_srv = (__SRV_PTR)gres_volblk_otf.alloc_res_ptrs[DTYPE_SRV];
		dx11DeviceImmContext->CSSetShaderResources(1, 1, (__SRV_PTR*)&volblk_srv);
		dx11DeviceImmContext->CSSetShaderResources(0, 1, (__SRV_PTR*)&gres_main_vol.alloc_res_ptrs[DTYPE_SRV]);
#pragma endregion // Resource Shader Setting

#pragma region // Renderer
//#define __RM_DEFAULT 0
//#define __RM_OPAQUE 1
//#define __RM_MODULATION 2
//#define __RM_SCULPTMASK 3
//#define __RM_MAXMASK 4
//#define __RM_TEST 6
//#define __RM_RAYMAX 10
//#define __RM_RAYMIN 11
//#define __RM_RAYSUM 12

		ID3D11ComputeShader* cshader = NULL;
		switch (render_type)
		{
		case __RM_RAYMIN: cshader = GETCS(VR_RAYMIN_cs_5_0); break;
		case __RM_RAYMAX: cshader = GETCS(VR_RAYMAX_cs_5_0); break;
		case __RM_RAYSUM: cshader = GETCS(VR_RAYSUM_cs_5_0); break;
		case __RM_CLIPOPAQUE:
		case __RM_OPAQUE: cshader = GETCS(VR_OPAQUE_cs_5_0); break;
		case __RM_MODULATION: cshader = GETCS(VR_CONTEXT_cs_5_0); break;
		case __RM_DEFAULT:
		case __RM_SCULPTMASK: 
		default: cshader = GETCS(VR_DEFAULT_cs_5_0); break;
		}

		ID3D11UnorderedAccessView* dx11UAVs[4] = {
				  (ID3D11UnorderedAccessView*)gres_fb_counter.alloc_res_ptrs[DTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_fb_deep_k_buffer.alloc_res_ptrs[DTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_UAV]
		};
		dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 4, dx11UAVs, (UINT*)(&dx11UAVs));

		if (mode_OIT == MFR_MODE::DXAB || mode_OIT == MFR_MODE::DKBZT) // filling
		{
			dx11DeviceImmContext->CSSetShaderResources(50, 1, (ID3D11ShaderResourceView**)&gres_fb_ref_pidx.alloc_res_ptrs[DTYPE_SRV]); // search why this does not work
		}
		
		if(render_type != __RM_RAYMIN
			&& render_type != __RM_RAYMAX
			&& render_type != __RM_RAYSUM)
		{
			dx11DeviceImmContext->CSSetShader(GETCS(VR_SURFACE_cs_5_0), NULL, 0);
			dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);
		}

		if (i == num_main_vrs - 1 && cbEnvState.r_kernel_ao > 0)
		{
			is_performed_ssao = true;
			dx11DeviceImmContext->Flush();
			dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 5, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
			ComputeSSAO(dx11DeviceImmContext, dx11CommonParams, num_grid_x, num_grid_y,
				gres_fb_counter, gres_fb_deep_k_buffer, gres_fb_rgba, blur_SSAO,
				gres_fb_mip_z_halftexs, gres_fb_mip_a_halftexs, gres_fb_ao_texs, gres_fb_ao_blf_texs,
				gres_fb_depthcs, gres_fb_ao_vr_tex, gres_fb_ao_vr_blf_tex, true);

			dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 4, dx11UAVs, (UINT*)(&dx11UAVs));

			if (blur_SSAO)
			{
				dx11DeviceImmContext->CSSetShaderResources(10, 1, (ID3D11ShaderResourceView**)&gres_fb_ao_blf_texs[0].alloc_res_ptrs[DTYPE_SRV]);
				dx11DeviceImmContext->CSSetShaderResources(11, 1, (ID3D11ShaderResourceView**)&gres_fb_ao_blf_texs[1].alloc_res_ptrs[DTYPE_SRV]);
				dx11DeviceImmContext->CSSetShaderResources(20, 1, (ID3D11ShaderResourceView**)&gres_fb_ao_vr_blf_tex.alloc_res_ptrs[DTYPE_SRV]);
			}
			else
			{
				dx11DeviceImmContext->CSSetShaderResources(10, 1, (ID3D11ShaderResourceView**)&gres_fb_ao_texs[0].alloc_res_ptrs[DTYPE_SRV]);
				dx11DeviceImmContext->CSSetShaderResources(11, 1, (ID3D11ShaderResourceView**)&gres_fb_ao_texs[1].alloc_res_ptrs[DTYPE_SRV]);
				dx11DeviceImmContext->CSSetShaderResources(20, 1, (ID3D11ShaderResourceView**)&gres_fb_ao_vr_tex.alloc_res_ptrs[DTYPE_SRV]);
			}
		}

		dx11DeviceImmContext->CSSetShader(cshader, NULL, 0);
		dx11DeviceImmContext->Flush();
		dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);
#ifdef __DX_DEBUG_QUERY
		dx11CommonParams->debug_info_queue->PushEmptyStorageFilter();
#endif
		count_call_render++;

		dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 4, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
		dx11DeviceImmContext->CSSetUnorderedAccessViews(50, 1, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
#pragma endregion // Renderer
	}

	if (cbEnvState.r_kernel_ao > 0 && !is_performed_ssao)
	{
		ID3D11UnorderedAccessView* dx11UAVs[4] = {
				  (ID3D11UnorderedAccessView*)gres_fb_counter.alloc_res_ptrs[DTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_fb_deep_k_buffer.alloc_res_ptrs[DTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_UAV]
		};
		is_performed_ssao = true;
		dx11DeviceImmContext->Flush();
		dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 5, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
		ComputeSSAO(dx11DeviceImmContext, dx11CommonParams, num_grid_x, num_grid_y,
			gres_fb_counter, gres_fb_deep_k_buffer, gres_fb_rgba, blur_SSAO,
			gres_fb_mip_z_halftexs, gres_fb_mip_a_halftexs, gres_fb_ao_texs, gres_fb_ao_blf_texs,
			gres_fb_depthcs, gres_fb_ao_vr_tex, gres_fb_ao_vr_blf_tex, true);

		dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 4, dx11UAVs, (UINT*)(&dx11UAVs));

		if (blur_SSAO)
		{
			dx11DeviceImmContext->CSSetShaderResources(10, 1, (ID3D11ShaderResourceView**)&gres_fb_ao_blf_texs[0].alloc_res_ptrs[DTYPE_SRV]);
			dx11DeviceImmContext->CSSetShaderResources(11, 1, (ID3D11ShaderResourceView**)&gres_fb_ao_blf_texs[1].alloc_res_ptrs[DTYPE_SRV]);
			dx11DeviceImmContext->CSSetShaderResources(20, 1, (ID3D11ShaderResourceView**)&gres_fb_ao_vr_blf_tex.alloc_res_ptrs[DTYPE_SRV]);
		}
		else
		{
			dx11DeviceImmContext->CSSetShaderResources(10, 1, (ID3D11ShaderResourceView**)&gres_fb_ao_texs[0].alloc_res_ptrs[DTYPE_SRV]);
			dx11DeviceImmContext->CSSetShaderResources(11, 1, (ID3D11ShaderResourceView**)&gres_fb_ao_texs[1].alloc_res_ptrs[DTYPE_SRV]);
			dx11DeviceImmContext->CSSetShaderResources(20, 1, (ID3D11ShaderResourceView**)&gres_fb_ao_vr_tex.alloc_res_ptrs[DTYPE_SRV]);
		}
	}

	dx11DeviceImmContext->Flush();
	//printf("# Textures : %d, # Drawing : %d, # RTBuffer Change : %d, # Merging : %d\n", iNumTexureLayers, iCountRendering, iCountRTBuffers, iCountMerging);
	
	if (gpu_profile)
	{
		dx11DeviceImmContext->End(dx11CommonParams->dx11qr_timestamps[gpu_profilecount]);
		profile_map["end dvr"] = gpu_profilecount;
		gpu_profilecount++;
	}
	const bool is_system_out = true;
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
				//float* f_v = (float*)&matWS2SS;
				//for (int i = 0; i < 16; i++)
				//{
				//	test_out("matWS2SS " + to_string(i) + " : " + to_string(f_v[i]));
				//	if (f_v[i] != f_v[i]) test_out("matWS2SS " + to_string(i) + " - element error");
				//}
				//f_v = (float*)&matWS2PS;
				//for (int i = 0; i < 16; i++)
				//{
				//	test_out("matWS2PS " + to_string(i) + " : " + to_string(f_v[i]));
				//	if (f_v[i] != f_v[i]) test_out("matWS2PS " + to_string(i) + " - element error");
				//}
				//f_v = (float*)&matSS2WS;
				//for (int i = 0; i < 16; i++)
				//{
				//	test_out("matSS2WS " + to_string(i) + " : " + to_string(f_v[i]));
				//	if (f_v[i] != f_v[i]) test_out("matSS2WS " + to_string(i) + " - element error");
				//}
				//return false;
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
						rgba_sys_buf[i*fb_size_cur.x + j] = vmbyte4(rgba.x, rgba.y, rgba.z, rgba.w);
					else
						rgba_sys_buf[i*fb_size_cur.x + j] = vmbyte4(rgba.z, rgba.y, rgba.x, rgba.w);

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

	if(gpu_profile)
	{
		dx11DeviceImmContext->End(dx11CommonParams->dx11qr_timestamps[gpu_profilecount]);
		profile_map["end"] = gpu_profilecount;
		gpu_profilecount++;

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
			UINT64 tsBeginFrame = 0, tsEndDVR = 0, tsEndFrame = 0;

			auto GetTimeGpuProfile = [&profile_map, &dx11DeviceImmContext, &dx11CommonParams](const string& name, UINT64& ts) -> bool
			{
				auto it = profile_map.find(name);
				if (it == profile_map.end())
				{
					ts = 0;
					return false;
				}

				dx11DeviceImmContext->GetData(dx11CommonParams->dx11qr_timestamps[it->second], &ts, sizeof(UINT64), 0);
			};

			GetTimeGpuProfile("begin", tsBeginFrame);
			GetTimeGpuProfile("end dvr", tsEndDVR);
			GetTimeGpuProfile("end", tsEndFrame);
			UINT64 __dummny;
			for (int i = 0; i < MAXSTAMPS; i++)
				dx11DeviceImmContext->GetData(dx11CommonParams->dx11qr_timestamps[i], &__dummny, sizeof(UINT64), 0);

			auto DisplayDuration = [&tsDisjoint](UINT64 tsS, UINT64 tsE, const string& _test)
			{
				if (tsS == 0 || tsE == 0) return;
				cout << _test << " : " << float(tsE - tsS) / float(tsDisjoint.Frequency) * 1000.0f << " ms" << endl;
			};
			DisplayDuration(tsBeginFrame, tsEndFrame, "#GPU# Total (including copyback) Time");
			DisplayDuration(tsBeginFrame, tsEndDVR, "#GPU# Direct Volume Render Time");
			DisplayDuration(tsEndDVR, tsEndFrame, "#GPU# CopyBack Time");
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

	return true;
}