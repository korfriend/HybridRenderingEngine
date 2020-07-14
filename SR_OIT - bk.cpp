#include "RendererHeader.h"

#include <iostream>
//#include <opencv2/imgproc.hpp>
//#include <opencv2/highgui.hpp>
#include "../vismtv_modeling_vera/nanoflann.hpp"
template <typename T, typename TT>
struct PointCloud
{
	//public:
	const uint num_vtx;
	const TT* pts;
	PointCloud(const TT* _pts, const uint _num_vtx) : pts(_pts), num_vtx(_num_vtx){ }

	// Must return the number of data points
	inline size_t kdtree_get_point_count() const { return num_vtx; }

	// Returns the distance between the vector "p1[0:size-1]" and the data point with index "idx_p2" stored in the class:
	inline T kdtree_distance(const T *p1, const size_t idx_p2, size_t) const
	{
		const T d0 = p1[0] - pts[idx_p2].x;
		const T d1 = p1[1] - pts[idx_p2].y;
		const T d2 = p1[2] - pts[idx_p2].z;
		return d0 * d0 + d1 * d1 + d2 * d2;
	}

	// Returns the dim'th component of the idx'th point in the class:
	// Since this is inlined and the "dim" argument is typically an immediate value, the
	//  "if/else's" are actually solved at compile time.
	inline T kdtree_get_pt(const size_t idx, int dim) const
	{
		if (dim == 0) return pts[idx].x;
		else if (dim == 1) return pts[idx].y;
		else return pts[idx].z;
	}

	// Optional bounding-box computation: return false to default to a standard bbox computation loop.
	//   Return true if the BBOX was already computed by the class and returned in "bb" so it can be avoided to redo it again.
	//   Look at bb.size() to find out the expected dimensionality (e.g. 2 or 3 for point clouds)
	template <class BBOX>
	bool kdtree_get_bbox(BBOX&) const { return false; }
};

typedef nanoflann::KDTreeSingleIndexAdaptor<
	nanoflann::L2_Simple_Adaptor<float, PointCloud<float, vmfloat3> >,
	PointCloud<float, vmfloat3>,
	3 // dim 
> kd_tree_t;

using namespace grd_helper;

bool RenderSrOIT(VmFnContainer* _fncontainer,
	VmGpuManager* gpu_manager,
	grd_helper::GpuDX11CommonParameters* dx11CommonParams,
	LocalProgress* progress,
	double* run_time_ptr)
{
	LARGE_INTEGER lIntFreq;
	LARGE_INTEGER lIntCntStart, lIntCntEnd;

	vector<VmObject*> input_pobjs;
	_fncontainer->GetVmObjectList(&input_pobjs, VmObjKey(ObjectTypePRIMITIVE, true));

	vector<VmObject*> input_vobjs;
	_fncontainer->GetVmObjectList(&input_vobjs, VmObjKey(ObjectTypeVOLUME, true));
	vector<VmObject*> input_tobjs;
	_fncontainer->GetVmObjectList(&input_tobjs, VmObjKey(ObjectTypeTMAP, true));

	VmLObject* lobj = (VmLObject*)_fncontainer->GetVmObject(VmObjKey(ObjectTypeBUFFER, true), 0);

	map<int, VmObject*> associated_objs;
	for (int i = 0; i < (int)input_vobjs.size(); i++)
		associated_objs.insert(pair<int, VmObject*>(input_vobjs[i]->GetObjectID(), input_vobjs[i]));
	for (int i = 0; i < (int)input_tobjs.size(); i++)
		associated_objs.insert(pair<int, VmObject*>(input_tobjs[i]->GetObjectID(), input_tobjs[i]));

	VmIObject* iobj = (VmIObject*)_fncontainer->GetVmObject(VmObjKey(ObjectTypeIMAGEPLANE, false), 0);

	//((std::mutex*)HDx11GetMutexGpuCriticalPath())->lock();
	bool apply_avrmerge_old = true;
	lobj->GetCustomParameter("_bool_ExperimentAvrMerge", data_type::dtype<bool>(), &apply_avrmerge_old);
	int num_deep_layers_old = 8;
	lobj->GetCustomParameter("_int_NumDeepLayers", data_type::dtype<int>(), &num_deep_layers_old);
	bool apply_avrmerge = _fncontainer->GetParamValue("_bool_ExperimentAvrMerge", apply_avrmerge_old);
	int num_deep_layers = _fncontainer->GetParamValue("_int_NumDeepLayers", num_deep_layers_old);
	lobj->RegisterCustomParameter("_bool_ExperimentAvrMerge", apply_avrmerge);
	lobj->RegisterCustomParameter("_int_NumDeepLayers", num_deep_layers);

	bool apply_antialiasing = _fncontainer->GetParamValue("_bool_IsAntiAliasingRS", false);
	bool is_final_renderer = _fncontainer->GetParamValue("_bool_IsFinalRenderer", true);
	double v_thickness = _fncontainer->GetParamValue("_double_VZThickness", -1.0);

	//vr_level = 2;
	vmdouble4 global_phongblinn_factors = _fncontainer->GetParamValue("_double4_ShadingFactorsForGlobalPrimitives", vmdouble4(0.4, 0.6, 0.2, 30)); // Emission, Diffusion, Specular, Specular Power

	double default_point_thickness = _fncontainer->GetParamValue("_double_PointThickness", 3.0);
	double default_line_thickness = _fncontainer->GetParamValue("_double_LineThickness", 2.0);
	vmdouble3 default_color_cmmobj = _fncontainer->GetParamValue("_double3_CmmGlobalColor", vmdouble3(-1, -1, -1));

	bool is_rgba = _fncontainer->GetParamValue("_bool_IsRGBA", false);; // false means bgra

	bool is_system_out = false;
	if (is_final_renderer) is_system_out = true;
	
	bool test_consoleout = _fncontainer->GetParamValue("_bool_TestConsoleOut", false);
	auto test_out = [&test_consoleout](const string& _message)
	{
		if (test_consoleout)
			cout << _message << endl;
	};

#pragma region // Shader Setting
#define GETVS(NAME) dx11CommonParams->get_vshader(#NAME)
#define GETPS(NAME) dx11CommonParams->get_pshader(#NAME)
#define GETGS(NAME) dx11CommonParams->get_gshader(#NAME)
#define GETCS(NAME) dx11CommonParams->get_cshader(#NAME)

	// Shader Re-Compile Setting //
	bool reload_hlsl_objs = _fncontainer->GetParamValue("_bool_ReloadHLSLObjFiles", false);
	if (reload_hlsl_objs)
	{
		string prefix_path = "..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\OIT\\";

#define PS_NUM 7
#define SET_PS(NAME) dx11CommonParams->safe_set_res(grd_helper::COMRES_INDICATOR(PIXEL_SHADER, NAME), dx11PShader, true)

		string strNames_PS[PS_NUM] = {
			   "SR_OIT_KDEPTH_PHONGBLINN_ps_5_0"
			  ,"SR_OIT_KDEPTH_DASHEDLINE_ps_5_0"
			  ,"SR_OIT_KDEPTH_MULTITEXTMAPPING_ps_5_0"
			  ,"SR_OIT_KDEPTH_TEXTMAPPING_ps_5_0"
			  ,"SR_SINGLE_LAYER_ps_5_0"
			  ,"SR_OIT_KDEPTH_PHONGBLINN_SEQ_ps_5_0"
			  ,"SR_OIT_KDEPTH_PHONGBLINN_CTMODELER_ps_5_0"
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
	}

	ID3D11InputLayout* dx11LI_P = (ID3D11InputLayout*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(INPUT_LAYOUT, "P"));
	ID3D11InputLayout* dx11LI_PN = (ID3D11InputLayout*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(INPUT_LAYOUT, "PN"));
	ID3D11InputLayout* dx11LI_PT = (ID3D11InputLayout*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(INPUT_LAYOUT, "PT"));
	ID3D11InputLayout* dx11LI_PNT = (ID3D11InputLayout*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(INPUT_LAYOUT, "PNT"));
	ID3D11InputLayout* dx11LI_PTTT = (ID3D11InputLayout*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(INPUT_LAYOUT, "PTTT"));

	ID3D11VertexShader* dx11VShader_P = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(VERTEX_SHADER, "SR_OIT_P_vs_5_0"));
	ID3D11VertexShader* dx11VShader_PN = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(VERTEX_SHADER, "SR_OIT_PN_vs_5_0"));
	ID3D11VertexShader* dx11VShader_PT = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(VERTEX_SHADER, "SR_OIT_PT_vs_5_0"));
	ID3D11VertexShader* dx11VShader_PNT = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(VERTEX_SHADER, "SR_OIT_PNT_vs_5_0"));
	ID3D11VertexShader* dx11VShader_PTTT = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(VERTEX_SHADER, "SR_OIT_PTTT_vs_5_0"));

	ID3D11Buffer* cbuf_cam_state = dx11CommonParams->get_cbuf("CB_CameraState");
	ID3D11Buffer* cbuf_clip = dx11CommonParams->get_cbuf("CB_ClipInfo");
	ID3D11Buffer* cbuf_pobj = dx11CommonParams->get_cbuf("CB_PolygonObject");
	ID3D11Buffer* cbuf_vobj = dx11CommonParams->get_cbuf("CB_VolumeObject");
	ID3D11Buffer* cbuf_reffect = dx11CommonParams->get_cbuf("CB_RenderingEffect");
	ID3D11Buffer* cbuf_tmap = dx11CommonParams->get_cbuf("CB_TMAP");
#pragma endregion // Shader Setting

#pragma region // IOBJECT CPU
	while (iobj->GetFrameBuffer(FrameBufferUsageRENDEROUT, 1) != NULL)
		iobj->DeleteFrameBuffer(FrameBufferUsageRENDEROUT, 1);
	if (!iobj->ReplaceFrameBuffer(FrameBufferUsageRENDEROUT, 0, data_type::dtype<vmbyte4>(), ("common render out frame buffer : defined in vismtv_inbuilt_renderergpudx module")))
		iobj->InsertFrameBuffer(data_type::dtype<vmbyte4>(), FrameBufferUsageRENDEROUT, ("common render out frame buffer : defined in vismtv_inbuilt_renderergpudx module"));

	while (iobj->GetFrameBuffer(FrameBufferUsageDEPTH, 1) != NULL)
		iobj->DeleteFrameBuffer(FrameBufferUsageDEPTH, 1);
	if (!iobj->ReplaceFrameBuffer(FrameBufferUsageDEPTH, 0, data_type::dtype<float>(), ("1st hit screen depth frame buffer : defined in vismtv_inbuilt_renderergpudx module")))
		iobj->InsertFrameBuffer(data_type::dtype<float>(), FrameBufferUsageDEPTH, ("1st hit screen depth frame buffer : defined in vismtv_inbuilt_renderergpudx module"));
#pragma endregion // IOBJECT CPU

	ID3D11Device* dx11Device = dx11CommonParams->dx11Device;
	ID3D11DeviceContext* dx11DeviceImmContext = dx11CommonParams->dx11DeviceImmContext;

#pragma region // IOBJECT GPU
	vmint2 fb_size_cur, fb_size_old = vmint2(0, 0);
	iobj->GetFrameBufferInfo(&fb_size_cur);
	iobj->GetCustomParameter("_int2_PreviousScreenSize", data_type::dtype<vmint2>(), &fb_size_old);
	if (fb_size_cur.x != fb_size_old.x || fb_size_cur.y != fb_size_old.y
		|| apply_avrmerge != apply_avrmerge_old || num_deep_layers != num_deep_layers_old)
	{
		gpu_manager->ReleaseGpuResourcesBySrcID(iobj->GetObjectID());	// System Out 포함 //
		iobj->RegisterCustomParameter("_int2_PreviousScreenSize", fb_size_cur);
	}

	ullong lastest_render_time = 0;
	iobj->GetCustomParameter("_double_LatestSrTime", data_type::dtype<double>(), &lastest_render_time);

	GpuRes gres_fb_rgba, gres_fb_depthcs, gres_fb_depthstencil, gres_fb_counter, gres_fb_spinlock;
	GpuRes gres_fb_deep_vis, gres_fb_deep_depth, gres_fb_deep_thick;
	GpuRes gres_fb_sys_rgba, gres_fb_sys_depthcs;
	grd_helper::UpdateFrameBuffer(gres_fb_rgba, iobj, "RENDER_OUT_RGBA_0", RTYPE_TEXTURE2D,
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, false);
	grd_helper::UpdateFrameBuffer(gres_fb_depthcs, iobj, "RENDER_OUT_DEPTH_0", RTYPE_TEXTURE2D,
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_FLOAT, false);
	grd_helper::UpdateFrameBuffer(gres_fb_depthstencil, iobj, "DEPTH_STENCIL", RTYPE_TEXTURE2D,
		D3D11_BIND_DEPTH_STENCIL, DXGI_FORMAT_D32_FLOAT, false);
	grd_helper::UpdateFrameBuffer(gres_fb_counter, iobj, "RW_COUNTER", RTYPE_TEXTURE2D,
		D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_UINT, false);
	grd_helper::UpdateFrameBuffer(gres_fb_spinlock, iobj, "RW_SPINLOCK", RTYPE_TEXTURE2D,
		D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_UINT, false);

	grd_helper::UpdateFrameBuffer(gres_fb_deep_vis, iobj, "BUFFER_RW_DEEP_VIS", RTYPE_BUFFER,
		D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_UINT, false, 
		apply_avrmerge? num_deep_layers + 4 : num_deep_layers);
	
	grd_helper::UpdateFrameBuffer(gres_fb_deep_depth, iobj, "BUFFER_RW_DEEP_DEPTH", RTYPE_BUFFER,
		D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_UINT, false, num_deep_layers);
	grd_helper::UpdateFrameBuffer(gres_fb_deep_thick, iobj, "BUFFER_RW_DEEP_THICK", RTYPE_BUFFER,
		D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_UINT, false, num_deep_layers);
	grd_helper::UpdateFrameBuffer(gres_fb_sys_rgba, iobj, "SYSTEM_OUT_RGBA", RTYPE_TEXTURE2D, NULL, DXGI_FORMAT_R8G8B8A8_UNORM, true);
	grd_helper::UpdateFrameBuffer(gres_fb_sys_depthcs, iobj, "SYSTEM_OUT_DEPTH", RTYPE_TEXTURE2D, NULL, DXGI_FORMAT_R32_FLOAT, true);
#pragma endregion // IOBJECT GPU

#pragma region // Camera & Light Setting
	const int __BLOCKSIZE = 8;
	uint num_grid_x = (uint)ceil(fb_size_cur.x / (float)__BLOCKSIZE);
	uint num_grid_y = (uint)ceil(fb_size_cur.y / (float)__BLOCKSIZE);

	uint num_pobjs = (uint)input_pobjs.size();
	float len_diagonal_max = 0;

	for (uint i = 0; i < num_pobjs; i++)
	{
		VmVObjectPrimitive* pobj = (VmVObjectPrimitive*)input_pobjs[i];
		if (pobj->IsDefined() == false)
			continue;

		PrimitiveData* prim_data = pobj->GetPrimitiveData();
		vmmat44f matOS2WS = pobj->GetMatrixOS2WSf();

		vmfloat3 pos_aabb_min_ws, pos_aabb_max_ws;
		fTransformPoint(&pos_aabb_min_ws, &(vmfloat3)prim_data->aabb_os.pos_min, &matOS2WS);
		fTransformPoint(&pos_aabb_max_ws, &(vmfloat3)prim_data->aabb_os.pos_max, &matOS2WS);
		len_diagonal_max = max(len_diagonal_max, fLengthVector(&(pos_aabb_min_ws - pos_aabb_max_ws)));
	}
	if (v_thickness < 0)
	{
		if (len_diagonal_max == 0)
		{
			v_thickness = 0.001;
		}
		else
		{
			v_thickness = len_diagonal_max * 0.005; // min(len_diagonal_max * 0.005, 0.01);
		}
	}

	float fv_thickness = (float)v_thickness;
	VmCObject* cam_obj = iobj->GetCameraObject();
	vmmat44f matWS2SS, matWS2PS, matSS2WS;
	CB_CameraState cbCamState;
	grd_helper::SetCb_Camera(cbCamState, matWS2PS, matWS2SS, matSS2WS, cam_obj, _fncontainer, fb_size_cur, num_deep_layers, fv_thickness);

	D3D11_MAPPED_SUBRESOURCE mappedResCamState;
	dx11DeviceImmContext->Map(cbuf_cam_state, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResCamState);
	CB_CameraState* cbCamStateData = (CB_CameraState*)mappedResCamState.pData;
	memcpy(cbCamStateData, &cbCamState, sizeof(CB_CameraState));
	dx11DeviceImmContext->Unmap(cbuf_cam_state, 0);
	dx11DeviceImmContext->VSSetConstantBuffers(0, 1, &cbuf_cam_state);
	dx11DeviceImmContext->GSSetConstantBuffers(0, 1, &cbuf_cam_state);
	dx11DeviceImmContext->PSSetConstantBuffers(0, 1, &cbuf_cam_state);
	dx11DeviceImmContext->CSSetConstantBuffers(0, 1, &cbuf_cam_state);

	// View Port Setting //
	D3D11_VIEWPORT dx11ViewPort;
	dx11ViewPort.Width = (float)fb_size_cur.x;
	dx11ViewPort.Height = (float)fb_size_cur.y;
	dx11ViewPort.MinDepth = 0;
	dx11ViewPort.MaxDepth = 1.0f;
	dx11ViewPort.TopLeftX = 0;
	dx11ViewPort.TopLeftY = 0;
	dx11DeviceImmContext->RSSetViewports(1, &dx11ViewPort);
#pragma endregion // Camera & Light Setting

#pragma region // Presetting of VxObject
	auto find_asscociated_obj = [&associated_objs](int obj_id) -> VmObject*
	{
		auto vol_obj = associated_objs.find(obj_id);
		if (vol_obj == associated_objs.end()) return NULL;
		return vol_obj->second;
	};
	map<int, GpuRes> mapGpuRes_Idx;
	map<int, GpuRes> mapGpuRes_Vtx;
	map<int, GpuRes> mapGpuRes_Tex;
	map<int, GpuRes> mapGpuRes_VolumeAndTMap;

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

	vector<RenderObjInfo> general_oit_routine_objs;
	vector<RenderObjInfo> single_layer_routine_objs;
	int minimum_oit_area = _fncontainer->GetParamValue("_int_MinimumOitArea", (int)1000); // 0 means turn-off the wildcard case
	int _w_max = 0;
	int _h_max = 0;
	for (uint i = 0; i < num_pobjs; i++)
	{
		VmVObjectPrimitive* pobj = (VmVObjectPrimitive*)input_pobjs[i];
		if (pobj->IsDefined() == false)
			continue;

		PrimitiveData* prim_data = pobj->GetPrimitiveData();
		if (prim_data->GetVerticeDefinition("POSITION") == NULL)
			continue;

		bool is_visible = true;
		pobj->GetCustomParameter("_bool_visibility", data_type::dtype<bool>(), &is_visible);
		if (!is_visible)
			continue;

		vmmat44f matOS2WS = pobj->GetMatrixOS2WSf(); // matWS2SS
		vmmat44f matOS2SS = matOS2WS * matWS2SS;
		int w, h;
		__compute_computespace_screen(w, h, matOS2SS, prim_data->aabb_os);
		bool is_wildcard_Candidate = w * h < minimum_oit_area;
		_w_max = max(_w_max, w);
		_h_max = max(_h_max, h);

		int vobj_id = 0, tobj_id = 0;
		int pobj_id = pobj->GetObjectID();
		lobj->GetDstObjValue(pobj_id, "_int_AssociatedVolumeID", &vobj_id);
		lobj->GetDstObjValue(pobj_id, "_int_AssociatedTObjectID", &tobj_id);

		VmVObjectVolume* vol_obj = (VmVObjectVolume*)find_asscociated_obj(vobj_id);
		VmTObject* tobj = (VmTObject*)find_asscociated_obj(tobj_id);
		RegisterVolumeRes(vol_obj, tobj, lobj, gpu_manager, dx11DeviceImmContext, associated_objs, mapGpuRes_VolumeAndTMap, progress);

		GpuRes gres_vtx, gres_idx, gres_tex;
		grd_helper::UpdatePrimitiveModel(gres_vtx, gres_idx, gres_tex, pobj);
		if (gres_vtx.alloc_res_ptrs.size() > 0)
			mapGpuRes_Vtx.insert(pair<int, GpuRes>(pobj_id, gres_vtx));
		if (gres_idx.alloc_res_ptrs.size() > 0)
			mapGpuRes_Idx.insert(pair<int, GpuRes>(pobj_id, gres_idx));
		if (gres_tex.alloc_res_ptrs.size() > 0)
			mapGpuRes_Tex.insert(pair<int, GpuRes>(pobj_id, gres_tex));

		//////////////////////////////////////////////
		// Register Valid Objects to Rendering List //
		vmdouble4 dColor(1.), dColorWire(1.);
		pobj->GetCustomParameter("_double4_color", data_type::dtype<vmdouble4>(), &dColor);
		lobj->GetDstObjValue(pobj_id, "_double4_color", &dColor);
		bool is_wire = false;
		if (prim_data->ptype == PrimitiveTypeTRIANGLE)
			pobj->GetPrimitiveWireframeVisibilityColor(&is_wire, &dColorWire);
		vmfloat4 fColor(dColor), fColorWire(dColorWire);

		bool force_to_oitpass = false;
		lobj->GetDstObjValue(pobj_id, "_bool_ForceToOitPass", &force_to_oitpass);
		double pobj_vzthickness = v_thickness;
		lobj->GetDstObjValue(pobj_id, "_double_VzThickness", &pobj_vzthickness);

		RenderObjInfo render_obj_info;
		render_obj_info.pobj = pobj;
		render_obj_info.vzthickness = (float)pobj_vzthickness;
		if (is_wildcard_Candidate) render_obj_info.num_safe_loopexit = 10;

		//if (render_obj_info.fColor.a > 0.99f && render_obj_info.vzthickness <= fv_thickness)
		lobj->GetDstObjValue(pobj_id, "_bool_AbsDiffuse", &render_obj_info.abs_diffuse);

		if (is_wire && prim_data->ptype == PrimitiveTypeTRIANGLE)
		{
			render_obj_info.is_wireframe = true;
			render_obj_info.fColor = fColorWire;
			//render_obj_info.fColor.w = 1.f;
			lobj->GetDstObjValue(pobj_id, "_bool_UseVertexWireColor", &render_obj_info.use_vertex_color);

			//if ((render_obj_info.fColor.a < 0.99f || force_to_oitpass || pobj_vzthickness > v_thickness) && !is_wildcard_Candidate)
			//	general_oit_routine_objs.push_back(render_obj_info);
			//else
			//	opaque_wildcard_objs.push_back(render_obj_info);
			general_oit_routine_objs.push_back(render_obj_info);
		}

		pobj->GetCustomParameter("_bool_IsAnnotationObj", data_type::dtype<bool>(), &render_obj_info.is_annotation_obj);
		if (render_obj_info.is_annotation_obj) render_obj_info.is_annotation_obj = prim_data->texture_res != NULL;
		render_obj_info.is_wireframe = false;
		render_obj_info.fColor = fColor;
		render_obj_info.use_vertex_color = true;
		lobj->GetDstObjValue(pobj_id, "_bool_UseVertexColor", &render_obj_info.use_vertex_color);
		lobj->GetDstObjValue(pobj_id, "_bool_ShowOutline", &render_obj_info.show_outline);
		if (render_obj_info.show_outline)
			single_layer_routine_objs.push_back(render_obj_info);
		render_obj_info.show_outline = false;
		general_oit_routine_objs.push_back(render_obj_info);
	}

	bool print_out_routine_objs = _fncontainer->GetParamValue("_bool_PrintOutRoutineObjs", false);
	if (print_out_routine_objs)
	{
		cout << "  ** general_oit_routine_objs    : " << general_oit_routine_objs.size() << endl;
		cout << "  ** special_effect_routine_objs : " << single_layer_routine_objs.size() << endl;
	}
#pragma endregion // Presetting of VxObject

#pragma region // FrameBuffer Setting
	// Backup Previous Render Target //
	ID3D11RenderTargetView* pdxRTVOld = NULL;
	ID3D11DepthStencilView* pdxDSVOld = NULL;
	dx11DeviceImmContext->OMGetRenderTargets(1, &pdxRTVOld, &pdxDSVOld);

	// Clear Depth Stencil //
	ID3D11DepthStencilView* dx11DSV = (ID3D11DepthStencilView*)gres_fb_depthstencil.alloc_res_ptrs[BTYPE_DSV];
	dx11DeviceImmContext->ClearDepthStencilView(dx11DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

	float flt_max_ = FLT_MAX;
	uint flt_max_u = *(uint*)&flt_max_;
	uint clr_unit4[4] = { 0, 0, 0, 0 };
	uint clr_max_ufloat_4[4] = { flt_max_u, flt_max_u, flt_max_u, flt_max_u };
	float clr_float_zero_4[4] = { 0, 0, 0, 0 };
	float clr_float_fltmax_4[4] = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };
	float clr_float_minus_4[4] = { -1.f, -1.f, -1.f, -1.f };
	dx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gres_fb_counter.alloc_res_ptrs[BTYPE_UAV], clr_unit4);
	dx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gres_fb_spinlock.alloc_res_ptrs[BTYPE_UAV], clr_unit4);
	dx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gres_fb_deep_vis.alloc_res_ptrs[BTYPE_UAV], clr_unit4);
	dx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gres_fb_deep_depth.alloc_res_ptrs[BTYPE_UAV], clr_max_ufloat_4);
	dx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gres_fb_deep_thick.alloc_res_ptrs[BTYPE_UAV], clr_unit4);

	dx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gres_fb_rgba.alloc_res_ptrs[BTYPE_RTV], clr_float_zero_4);
	dx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gres_fb_depthcs.alloc_res_ptrs[BTYPE_RTV], clr_float_fltmax_4);
#pragma endregion // FrameBuffer Setting

#pragma region // Other Presetting For Shaders


	ID3D11SamplerState* sampler_PZ = dx11CommonParams->get_sampler("POINT_ZEROBORDER");
	ID3D11SamplerState* sampler_LZ = dx11CommonParams->get_sampler("LINEAR_ZEROBORDER");
	ID3D11SamplerState* sampler_PC = dx11CommonParams->get_sampler("POINT_CLAMP");
	ID3D11SamplerState* sampler_LC = dx11CommonParams->get_sampler("LINEAR_CLAMP");

	dx11DeviceImmContext->VSSetSamplers(0, 1, &sampler_LZ);
	dx11DeviceImmContext->VSSetSamplers(1, 1, &sampler_PZ);
	dx11DeviceImmContext->PSSetSamplers(0, 1, &sampler_LZ);
	dx11DeviceImmContext->PSSetSamplers(1, 1, &sampler_PZ);
	dx11DeviceImmContext->PSSetSamplers(2, 1, &sampler_LC);
	dx11DeviceImmContext->PSSetSamplers(3, 1, &sampler_PC);
	dx11DeviceImmContext->CSSetSamplers(0, 1, &sampler_LZ);
	dx11DeviceImmContext->CSSetSamplers(1, 1, &sampler_PZ);
	dx11DeviceImmContext->CSSetSamplers(2, 1, &sampler_LC);
	dx11DeviceImmContext->CSSetSamplers(3, 1, &sampler_PC);

#pragma endregion // Other Presetting For Shaders

	QueryPerformanceCounter(&lIntCntStart);

	ID3D11DepthStencilView* dx11DSVNULL = NULL;
	ID3D11RenderTargetView* dx11RTVsNULL[2] = { NULL, NULL };
	ID3D11UnorderedAccessView* dx11UAVs_NULL[6] = { NULL, NULL, NULL, NULL, NULL, NULL };
	ID3D11ShaderResourceView* dx11SRVs_NULL[6] = { NULL, NULL, NULL, NULL, NULL, NULL };
	// For Each Primitive //
	int count_call_render = 0;
	int RENDERER_LOOP = 0;
RENDERER_LOOP:
	vector<RenderObjInfo>* pvtrValidPrimitives;
	switch (RENDERER_LOOP)
	{
	case 0:
		pvtrValidPrimitives = &general_oit_routine_objs;
		break;
	case 1:
		pvtrValidPrimitives = &single_layer_routine_objs;
		break;
	default:
		goto RENDERER_LOOP_EXIT;
		break;
	}

	for (uint pobj_idx = 0; pobj_idx < (int)pvtrValidPrimitives->size(); pobj_idx++)
	{
		RenderObjInfo& render_obj_info = pvtrValidPrimitives->at(pobj_idx);
		VmVObjectPrimitive* pobj = render_obj_info.pobj;
		PrimitiveData* prim_data = (PrimitiveData*)pobj->GetPrimitiveData();

		int pobj_id = pobj->GetObjectID();

		// Object Unit Conditions
		bool cull_off = false;
		bool is_clip_free = false;
		bool apply_shadingfactors = true;
		int vobj_id = 0, tobj_id = 0;
		pobj->GetCustomParameter("_bool_IsForcedCullOff", data_type::dtype<bool>(), &cull_off);
		lobj->GetDstObjValue(pobj_id, "_int_AssociatedVolumeID", &vobj_id);
		lobj->GetDstObjValue(pobj_id, "_int_AssociatedTObjectID", &tobj_id);
		bool with_vobj = false;
		auto itrGpuVolume = mapGpuRes_VolumeAndTMap.find(vobj_id);
		auto itrGpuTObject = mapGpuRes_VolumeAndTMap.find(tobj_id);
		with_vobj = itrGpuVolume != mapGpuRes_VolumeAndTMap.end() && itrGpuTObject != mapGpuRes_VolumeAndTMap.end();
		
		if (with_vobj)
		{
			GpuRes& gres_vobj = itrGpuVolume->second;
			GpuRes& gres_tobj = itrGpuTObject->second;
			dx11DeviceImmContext->VSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&gres_tobj.alloc_res_ptrs[BTYPE_SRV]);
			dx11DeviceImmContext->PSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&gres_tobj.alloc_res_ptrs[BTYPE_SRV]);
			dx11DeviceImmContext->VSSetShaderResources(1, 1, (ID3D11ShaderResourceView**)&gres_vobj.alloc_res_ptrs[BTYPE_SRV]);
			dx11DeviceImmContext->PSSetShaderResources(1, 1, (ID3D11ShaderResourceView**)&gres_vobj.alloc_res_ptrs[BTYPE_SRV]);

			auto itrVolume = associated_objs.find(vobj_id);
			auto itrTObject = associated_objs.find(tobj_id);
			VmVObjectVolume* vobj = (VmVObjectVolume*)itrVolume->second;
			VmTObject* tobj = (VmTObject*)itrTObject->second;

			bool high_samplerate = gres_vobj.res_dvalues["SAMPLE_OFFSET_X"] > 1.f ||
				gres_vobj.res_dvalues["SAMPLE_OFFSET_Y"] > 1.f || gres_vobj.res_dvalues["SAMPLE_OFFSET_Z"] > 1.f;

			CB_VolumeObject cbVolumeObj;
			vmint3 vol_sampled_size = vmint3(gres_vobj.res_dvalues["WIDTH"], gres_vobj.res_dvalues["HEIGHT"], gres_vobj.res_dvalues["DEPTH"]);
			grd_helper::SetCb_VolumeObj(cbVolumeObj, vobj, lobj, high_samplerate, vol_sampled_size, 0, 0);
			D3D11_MAPPED_SUBRESOURCE mappedResVolObj;
			dx11DeviceImmContext->Map(cbuf_vobj, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResVolObj);
			CB_VolumeObject* cbVolumeObjData = (CB_VolumeObject*)mappedResVolObj.pData;
			memcpy(cbVolumeObjData, &cbVolumeObj, sizeof(CB_VolumeObject));
			dx11DeviceImmContext->Unmap(cbuf_vobj, 0);

			CB_TMAP cbTmap;
			grd_helper::SetCb_TMap(cbTmap, tobj);
			D3D11_MAPPED_SUBRESOURCE mappedResTmap;
			dx11DeviceImmContext->Map(cbuf_tmap, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResTmap);
			CB_TMAP* cbTmapData = (CB_TMAP*)mappedResTmap.pData;
			memcpy(cbTmapData, &cbTmap, sizeof(CB_TMAP));
			dx11DeviceImmContext->Unmap(cbuf_tmap, 0);
		}

		if (render_obj_info.is_annotation_obj)
		{
			GpuRes gres_tex = mapGpuRes_Tex[pobj_id];
			dx11DeviceImmContext->PSSetShaderResources(3, 1, (ID3D11ShaderResourceView**)&gres_tex.alloc_res_ptrs[BTYPE_SRV]);
			if (default_color_cmmobj.x >= 0 && default_color_cmmobj.y >= 0 && default_color_cmmobj.z >= 0)
				render_obj_info.fColor = vmfloat4(default_color_cmmobj, render_obj_info.fColor.a);
		}

		int special_renderer = -1;
		pobj->GetCustomParameter("_int_CtModelerStep", data_type::dtype<int>(), &special_renderer);
		if (special_renderer > 0)
		{
			render_obj_info.use_vertex_color = true;
		}

		CB_PolygonObject cbPolygonObj;
		vmmat44f matOS2WS = pobj->GetMatrixOS2WSf();
		grd_helper::SetCb_PolygonObj(cbPolygonObj, pobj, lobj, matOS2WS, matWS2SS, matWS2PS, render_obj_info, default_point_thickness, default_line_thickness);
		D3D11_MAPPED_SUBRESOURCE mappedResPobjData;
		dx11DeviceImmContext->Map(cbuf_pobj, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResPobjData);
		CB_PolygonObject* cbPolygonObjData = (CB_PolygonObject*)mappedResPobjData.pData;
		if (special_renderer > 0)
		{
			int metric_idx;
			lobj->GetDstObjValue(pobj_id, "_int_CtModelerMetricIndex", &metric_idx);
			double metric_min_v, metric_max_v;
			lobj->GetDstObjValue(pobj_id, "_double_CtModelerMinMapV", &metric_min_v);
			lobj->GetDstObjValue(pobj_id, "_double_CtModelerMaxMapV", &metric_max_v);
			cbPolygonObj.pobj_ct_modeler_idx = (uint)metric_idx;
			cbPolygonObj.pobj_ct_modeler_min = *(uint*)&metric_min_v;
			cbPolygonObj.pobj_ct_modeler_max = *(uint*)&metric_max_v;
		}
		memcpy(cbPolygonObjData, &cbPolygonObj, sizeof(CB_PolygonObject));
		dx11DeviceImmContext->Unmap(cbuf_pobj, 0);

		CB_ClipInfo cbClipInfo;
		grd_helper::SetCb_ClipInfo(cbClipInfo, pobj, lobj);
		D3D11_MAPPED_SUBRESOURCE mappedResClipInfo;
		dx11DeviceImmContext->Map(cbuf_clip, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResClipInfo);
		CB_ClipInfo* cbClipInfoData = (CB_ClipInfo*)mappedResClipInfo.pData;
		memcpy(cbClipInfoData, &cbClipInfo, sizeof(CB_ClipInfo));
		dx11DeviceImmContext->Unmap(cbuf_clip, 0);

		CB_RenderingEffect cbRenderEffect;
		grd_helper::SetCb_RenderingEffect(cbRenderEffect, pobj, lobj, render_obj_info, global_phongblinn_factors);
		D3D11_MAPPED_SUBRESOURCE mappedResRenderEffect;
		dx11DeviceImmContext->Map(cbuf_reffect, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResRenderEffect);
		CB_RenderingEffect* cbRenderEffectData = (CB_RenderingEffect*)mappedResRenderEffect.pData;
		memcpy(cbRenderEffectData, &cbRenderEffect, sizeof(CB_RenderingEffect));
		dx11DeviceImmContext->Unmap(cbuf_reffect, 0);

#pragma region // Setting Rasterization Stages
		ID3D11InputLayout* dx11InputLayer_Target = NULL;
		ID3D11VertexShader* dx11VS_Target = NULL;
		ID3D11GeometryShader* dx11GS_Target = NULL;
		ID3D11PixelShader* dx11PS_Target = NULL;
		ID3D11RasterizerState* dx11RState_TargetObj = NULL;
		uint offset = 0;
		D3D_PRIMITIVE_TOPOLOGY pobj_topology_type;

		if (prim_data->GetVerticeDefinition("NORMAL"))
		{
			if (prim_data->GetVerticeDefinition("TEXCOORD0"))
			{
				// PNT (here, T is used as color)
				dx11InputLayer_Target = dx11LI_PNT;
				dx11VS_Target = dx11VShader_PNT;
			}
			else
			{
				// PN
				dx11InputLayer_Target = dx11LI_PN;
				dx11VS_Target = dx11VShader_PN;
			}

			if (render_obj_info.is_annotation_obj && dx11InputLayer_Target == dx11LI_PNT)
				dx11PS_Target = GETPS(SR_OIT_KDEPTH_TEXTMAPPING_ps_5_0);
			else
				dx11PS_Target = apply_avrmerge ? GETPS(SR_OIT_KDEPTH_PHONGBLINN_ps_5_0) : GETPS(SR_OIT_KDEPTH_PHONGBLINN_SEQ_ps_5_0);
		}
		else if (prim_data->GetVerticeDefinition("TEXCOORD0"))
		{
			cbRenderEffect.pb_shading_factor = D3DXVECTOR4(1.f, 1.f, 1.f, 1.f);
			if (prim_data->GetVerticeDefinition("TEXCOORD2"))
			{
				assert(pvtrValidPrimitives != &single_layer_routine_objs);
				// only text case //
				// PTTT
				dx11InputLayer_Target = dx11LI_PTTT;
				dx11VS_Target = dx11VShader_PTTT;
				dx11PS_Target = GETPS(SR_OIT_KDEPTH_MULTITEXTMAPPING_ps_5_0);
			}
			else
			{
				// if (render_obj_info.use_vertex_color)
				// PT
				dx11InputLayer_Target = dx11LI_PT;
				dx11VS_Target = dx11VShader_PT;

				if (render_obj_info.is_annotation_obj)
					dx11PS_Target = GETPS(SR_OIT_KDEPTH_TEXTMAPPING_ps_5_0);
				else if ((cbPolygonObj.pobj_flag & (0x1 << 19)) && prim_data->ptype == PrimitiveTypeLINE)
					dx11PS_Target = GETPS(SR_OIT_KDEPTH_DASHEDLINE_ps_5_0);
				else
					dx11PS_Target = apply_avrmerge ? GETPS(SR_OIT_KDEPTH_PHONGBLINN_ps_5_0) : GETPS(SR_OIT_KDEPTH_PHONGBLINN_SEQ_ps_5_0);
			}
		}
		else
		{
			cbRenderEffect.pb_shading_factor = D3DXVECTOR4(1.f, 1.f, 1.f, 1.f);
			// P
			dx11InputLayer_Target = dx11LI_P;
			dx11VS_Target = dx11VShader_P;
			dx11PS_Target = apply_avrmerge ? GETPS(SR_OIT_KDEPTH_PHONGBLINN_ps_5_0) : GETPS(SR_OIT_KDEPTH_PHONGBLINN_SEQ_ps_5_0);
		}

		if (pvtrValidPrimitives == &single_layer_routine_objs)
			dx11PS_Target = GETPS(SR_SINGLE_LAYER_ps_5_0);
		
		if (special_renderer > 0)
		{
			dx11PS_Target = GETPS(SR_OIT_KDEPTH_PHONGBLINN_CTMODELER_ps_5_0);

			struct VtxVisMap : VmVObjectBaseCustomData
			{
				bool* buffer_map;
				PointCloud<float, vmfloat3>* pc;
				kd_tree_t* kdt;

				void Delete() override
				{
					delete kdt;
					delete pc;
					delete[] buffer_map;
					delete this;
				}
			};

			bool vis_updated = false;
			lobj->GetDstObjValue(pobj_id, "_bool_CtModelerMapUpdate", &vis_updated);
			double eps_u = 0.0; // const
			lobj->GetDstObjValue(pobj_id, "_double_CtModelerEpsu", &eps_u);
			double mu_u = 0.3;
			lobj->GetDstObjValue(pobj_id, "_double_CtModelerMuu", &mu_u);
			double g_l = 0.3;
			lobj->GetDstObjValue(pobj_id, "_double_CtModelerGl", &g_l);

			VtxVisMap* vtx_vis_map = (VtxVisMap*)pobj->GetCustumDataStructure("_VtxVisMap");
			if (vtx_vis_map == NULL)
			{
				vtx_vis_map = new VtxVisMap();
				vtx_vis_map->buffer_map = new bool[prim_data->num_vtx];
				memset(vtx_vis_map->buffer_map, 1, sizeof(bool) * prim_data->num_vtx);
				pobj->ReplaceOrAddCustumDataStructure("_VtxVisMap", vtx_vis_map);

				vtx_vis_map->pc = new PointCloud<float, vmfloat3>(prim_data->GetVerticeDefinition("POSITION"), prim_data->num_vtx);
				vtx_vis_map->kdt = new kd_tree_t(3, *vtx_vis_map->pc, nanoflann::KDTreeSingleIndexAdaptorParams(10));
				vtx_vis_map->kdt->buildIndex();
				vis_updated = true;
			}

			// update 시에만 호출..
			GpuRes gres_vis_map;
			gres_vis_map.vm_src_id = pobj_id;
			gres_vis_map.res_name = "VTX_VIS_MAP";
			gres_vis_map.rtype = RTYPE_BUFFER;
			gres_vis_map.options["USAGE"] = D3D11_USAGE_DYNAMIC;
			gres_vis_map.options["CPU_ACCESS_FLAG"] = D3D11_CPU_ACCESS_WRITE;
			gres_vis_map.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE;
			gres_vis_map.options["FORMAT"] = DXGI_FORMAT_R8_UINT;
			gres_vis_map.res_dvalues["NUM_ELEMENTS"] = prim_data->num_vtx;
			gres_vis_map.res_dvalues["STRIDE_BYTES"] = 1;

			if (!gpu_manager->UpdateGpuResource(gres_vis_map))
			{
				gpu_manager->GenerateGpuResource(gres_vis_map);
				vis_updated = true;
			}

			if (vis_updated)
			{
				static nanoflann::SearchParams params;
				params.sorted = false;
				memset(vtx_vis_map->buffer_map, 1, sizeof(bool) * prim_data->num_vtx);
				float r_sq = (float)(eps_u * eps_u);

				vmfloat3* vtx_info_buf = prim_data->GetVerticeDefinition("TEXCOORD0");
				for (uint i = 0; i < prim_data->num_vtx; i++)
				{
					float* _info = (float*)&vtx_info_buf[i];

					if (_info[1] >= (float)mu_u)
					{
						vmfloat3 pos_src = vtx_vis_map->pc->pts[i];
						std::vector<std::pair<size_t, float>> ret_matches;
						const int nMatches = (int)vtx_vis_map->kdt->radiusSearch((float*)&pos_src, r_sq, ret_matches, params);
						for (int j = 0; j < nMatches; j++)
						{
							int idx_neighbor = (int)ret_matches[j].first;
							vtx_vis_map->buffer_map[idx_neighbor] = false;
						}
					}
					if (_info[0] <= (float)g_l)
						vtx_vis_map->buffer_map[i] = false;
				}

				ID3D11Buffer* pdx11bufvtx_vis_map = (ID3D11Buffer*)gres_vis_map.alloc_res_ptrs[BTYPE_RES];
				D3D11_MAPPED_SUBRESOURCE mappedRes;
				dx11CommonParams->dx11DeviceImmContext->Map(pdx11bufvtx_vis_map, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes);
				memcpy(mappedRes.pData, vtx_vis_map->buffer_map, sizeof(bool) * prim_data->num_vtx);
				dx11CommonParams->dx11DeviceImmContext->Unmap(pdx11bufvtx_vis_map, NULL);
			}
			dx11CommonParams->dx11DeviceImmContext->CSSetShaderResources(10, 1, (ID3D11ShaderResourceView**)&gres_vis_map.alloc_res_ptrs[BTYPE_SRV]);
		}

		switch (prim_data->ptype)
		{
		case PrimitiveTypeLINE:
			if (prim_data->is_stripe)
				pobj_topology_type = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
			else
				pobj_topology_type = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
			if (default_line_thickness > 0)
				dx11GS_Target = GETGS(GS_ThickLines_gs_5_0);
			break;
		case PrimitiveTypeTRIANGLE:
			if (prim_data->is_stripe)
				pobj_topology_type = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
			else
				pobj_topology_type = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			break;
		case PrimitiveTypePOINT:
			pobj_topology_type = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
			if (default_point_thickness > 0)
				dx11GS_Target = GETGS(GS_ThickPoints_gs_5_0);
			break;
		default:
			continue;
		}
		
#define GETRASTER(NAME) dx11CommonParams->get_rasterizer(#NAME)

		if (render_obj_info.is_wireframe)
			dx11RState_TargetObj = GETRASTER(WIRE_NONE);
		else
			dx11RState_TargetObj = GETRASTER(SOLID_NONE);

		auto itrMapBufferVtx = mapGpuRes_Vtx.find(pobj_id);

		{
			if (dx11InputLayer_Target == dx11LI_PNT && prim_data->num_vtx > 1000000)
			{
				cout << "# of vtx : " << prim_data->num_vtx << endl;

				ID3D11Buffer* pdx11bufvtx = (ID3D11Buffer*)itrMapBufferVtx->second.alloc_res_ptrs[BTYPE_RES];
				D3D11_MAPPED_SUBRESOURCE mappedRes;
				dx11CommonParams->dx11DeviceImmContext->Map(pdx11bufvtx, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes);
				vmfloat3* vtxbuf = (vmfloat3*)mappedRes.pData;
				static int gg = 0;
				gg++;
				for (int i = 0; i < prim_data->num_vtx; i++)
				{
					vtxbuf[3 * i + 2] = gg % 3 == 0 ? vmfloat3(1, 0, 0) : gg % 3 == 1 ? vmfloat3(0, 1, 0) : vmfloat3(0, 0, 1);
				}
				dx11CommonParams->dx11DeviceImmContext->Unmap(pdx11bufvtx, NULL);
			}
		}

		ID3D11Buffer* dx11BufferTargetPrim = (ID3D11Buffer*)itrMapBufferVtx->second.alloc_res_ptrs[BTYPE_RES];
		ID3D11Buffer* dx11IndiceTargetPrim = NULL;
		uint stride_inputlayer = sizeof(vmfloat3)*(uint)prim_data->GetNumVertexDefinitions();
		dx11DeviceImmContext->IASetVertexBuffers(0, 1, (ID3D11Buffer**)&dx11BufferTargetPrim, &stride_inputlayer, &offset);
		if (prim_data->vidx_buffer != NULL)
		{
			auto itrMapBufferIdx = mapGpuRes_Idx.find(pobj_id);
			dx11IndiceTargetPrim = (ID3D11Buffer*)itrMapBufferIdx->second.alloc_res_ptrs[BTYPE_RES];
			dx11DeviceImmContext->IASetIndexBuffer(dx11IndiceTargetPrim, DXGI_FORMAT_R32_UINT, 0);
		}

		dx11DeviceImmContext->IASetInputLayout(dx11InputLayer_Target);
		dx11DeviceImmContext->VSSetShader(dx11VS_Target, NULL, 0);
		dx11DeviceImmContext->GSSetShader(dx11GS_Target, NULL, 0);
		dx11DeviceImmContext->PSSetShader(dx11PS_Target, NULL, 0);
		dx11DeviceImmContext->RSSetState(dx11RState_TargetObj);
		dx11DeviceImmContext->IASetPrimitiveTopology(pobj_topology_type);
#pragma endregion // Setting Rasterization Stages

#pragma region // Set Contant Buffers
		dx11DeviceImmContext->VSSetConstantBuffers(1, 1, &cbuf_pobj);
		dx11DeviceImmContext->PSSetConstantBuffers(1, 1, &cbuf_pobj);
		dx11DeviceImmContext->CSSetConstantBuffers(1, 1, &cbuf_pobj);
		if (prim_data->ptype == PrimitiveTypePOINT || prim_data->ptype == PrimitiveTypeLINE)
			dx11DeviceImmContext->GSSetConstantBuffers(1, 1, &cbuf_pobj);

		dx11DeviceImmContext->PSSetConstantBuffers(2, 1, &cbuf_clip);
		dx11DeviceImmContext->PSSetConstantBuffers(3, 1, &cbuf_reffect);

		if (with_vobj)
		{
			dx11DeviceImmContext->PSSetConstantBuffers(4, 1, &cbuf_vobj);
			dx11DeviceImmContext->PSSetConstantBuffers(5, 1, &cbuf_tmap);
		}
#pragma endregion //
#pragma region // 1st RENDERING PASS
		ID3D11UnorderedAccessView* dx11UAVs_1st_pass[5] = {
				  (ID3D11UnorderedAccessView*)gres_fb_counter.alloc_res_ptrs[BTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_fb_spinlock.alloc_res_ptrs[BTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_fb_deep_vis.alloc_res_ptrs[BTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_fb_deep_depth.alloc_res_ptrs[BTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_fb_deep_thick.alloc_res_ptrs[BTYPE_UAV]
		};

#define GETDEPTHSTENTIL(NAME) dx11CommonParams->get_depthstencil(#NAME)

		switch (RENDERER_LOOP)
		{
		case 0:
			dx11DeviceImmContext->OMSetDepthStencilState(GETDEPTHSTENTIL(ALWAYS), 0);
			dx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, NULL, 2, 5, dx11UAVs_1st_pass, NULL);
			break;
		case 1:
			// clear //
			dx11DeviceImmContext->ClearDepthStencilView(dx11DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
			ID3D11RenderTargetView* dx11RTVs[2] = {
				(ID3D11RenderTargetView*)gres_fb_rgba.alloc_res_ptrs[BTYPE_RTV],
				(ID3D11RenderTargetView*)gres_fb_depthcs.alloc_res_ptrs[BTYPE_RTV] };
			//dx11DeviceImmContext->OMSetRenderTargets(2, dx11RTVs, dx11DSV);
			dx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(2, dx11RTVs, dx11DSV, 2, 5, dx11UAVs_NULL, 0);
			dx11DeviceImmContext->OMSetDepthStencilState(GETDEPTHSTENTIL(LESSEQUAL), 0);
			break;
		}

		dx11DeviceImmContext->Flush();
		if (prim_data->is_stripe || prim_data->ptype == PrimitiveTypePOINT)
			dx11DeviceImmContext->Draw(prim_data->num_vtx, 0);
		else
			dx11DeviceImmContext->DrawIndexed(prim_data->num_vidx, 0, 0);
		count_call_render++;

		if (RENDERER_LOOP == 1)
		{
			dx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(2, dx11RTVsNULL, dx11DSVNULL, 2, 5, dx11UAVs_NULL, 0);

			ID3D11ShaderResourceView* dx11SRVs_1st_pass[2] = {
				  (ID3D11ShaderResourceView*)gres_fb_rgba.alloc_res_ptrs[BTYPE_SRV]
				, (ID3D11ShaderResourceView*)gres_fb_depthcs.alloc_res_ptrs[BTYPE_SRV]
			};
			dx11DeviceImmContext->CSSetUnorderedAccessViews(2, 5, dx11UAVs_1st_pass, (UINT*)(&dx11UAVs_1st_pass));
			dx11DeviceImmContext->CSSetShaderResources(10, 2, dx11SRVs_1st_pass);

			UINT UAVInitialCounts = 0;
			dx11DeviceImmContext->CSSetShader(apply_avrmerge ? GETCS(SR_OIT_PRESET_cs_5_0) : GETCS(SR_OIT_PRESET_SEQ_cs_5_0), NULL, 0);

			dx11DeviceImmContext->Flush();
			dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);

			dx11DeviceImmContext->CSSetUnorderedAccessViews(2, 5, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
			dx11DeviceImmContext->CSSetShaderResources(10, 2, dx11SRVs_NULL);

			dx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gres_fb_depthcs.alloc_res_ptrs[BTYPE_RTV], clr_float_fltmax_4);
			dx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gres_fb_rgba.alloc_res_ptrs[BTYPE_RTV], clr_float_zero_4);
		}

		// window x 크기가 500 이상일 때, buffer out 해서 thick 만.. 찍어 보기.
		//if (fb_size_cur.x > 500)
		//{
		//	dx11DeviceImmContext->Flush();
		//	ID3D11Buffer* pDeepBufferThick_SYS;
		//	D3D11_BUFFER_DESC descBuf;
		//	ZeroMemory(&descBuf, sizeof(D3D11_BUFFER_DESC));
		//	descBuf.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		//	descBuf.ByteWidth = fb_size_cur.x * fb_size_cur.y * 8 * sizeof(float);
		//	descBuf.StructureByteStride = sizeof(uint);
		//	descBuf.Usage = D3D11_USAGE_STAGING;
		//	descBuf.BindFlags = NULL;
		//	descBuf.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
		//	dx11Device->CreateBuffer(&descBuf, NULL, &pDeepBufferThick_SYS);
		//
		//	ID3D11Texture2D* pTexCnt_SYS;
		//	D3D11_TEXTURE2D_DESC desc2D;
		//	ZeroMemory(&desc2D, sizeof(D3D11_TEXTURE2D_DESC));
		//	desc2D.ArraySize = 1;
		//	desc2D.Format = DXGI_FORMAT_R32_UINT;
		//	desc2D.Width = fb_size_cur.x;
		//	desc2D.Height = fb_size_cur.y;
		//	desc2D.MipLevels = 1;
		//	desc2D.SampleDesc.Count = 1;
		//	desc2D.SampleDesc.Quality = 0;
		//	desc2D.Usage = D3D11_USAGE_STAGING;
		//	desc2D.BindFlags = NULL;
		//	desc2D.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
		//	dx11Device->CreateTexture2D(&desc2D, NULL, &pTexCnt_SYS);
		//
		//	dx11DeviceImmContext->CopyResource(pDeepBufferThick_SYS, (ID3D11Buffer*)res_deep_zthick_buf.vtrPtrs[0]);
		//	dx11DeviceImmContext->CopyResource(pTexCnt_SYS, (ID3D11Buffer*)res_counter_tex.vtrPtrs[0]);
		//
		//	cv::Mat img_test(fb_size_cur.y, fb_size_cur.x, CV_8UC3);
		//	unsigned char* cv_buffer = (unsigned char*)img_test.data;
		//	D3D11_MAPPED_SUBRESOURCE mappedResSysDeepThick, mappedResSysTexCnt;
		//	HRESULT hr = dx11DeviceImmContext->Map(pDeepBufferThick_SYS, 0, D3D11_MAP_READ, NULL, &mappedResSysDeepThick);
		//	hr = dx11DeviceImmContext->Map(pTexCnt_SYS, 0, D3D11_MAP_READ, NULL, &mappedResSysTexCnt);
		//	unsigned int* rgba_gpu_buf_deep_thick = (unsigned int*)mappedResSysDeepThick.pData;
		//	unsigned int* rgba_gpu_buf_cnt = (unsigned int*)mappedResSysTexCnt.pData;
		//	int buf_row_pitch_cnt = mappedResSysTexCnt.RowPitch / 4;
		//	for (int i = 0; i < fb_size_cur.y; i++)
		//	{
		//		for (int j = 0; j < fb_size_cur.x; j++)
		//		{
		//			int addr_cv = (j + i * fb_size_cur.x) * 3;
		//			cv_buffer[addr_cv + 0] = 0;
		//			cv_buffer[addr_cv + 1] = 0;
		//			cv_buffer[addr_cv + 2] = 0;
		//			unsigned int _r = rgba_gpu_buf_cnt[(j + i * buf_row_pitch_cnt)];
		//			//unsigned int _r = rgba_gpu_buf_deep_color[(j + i * Width) * 4 * 8 + 0];
		//			//unsigned int _g = rgba_gpu_buf_deep_color[(j + i * Width) * 4 * 8 + 1];
		//			//unsigned int _b = rgba_gpu_buf_deep_color[(j + i * Width) * 4 * 8 + 2];
		//			for (int k = 0; k < 8; k++)
		//			{
		//				unsigned int _t = rgba_gpu_buf_deep_thick[(j + i * fb_size_cur.x) * 8 + k];
		//				if (_t > 0)
		//				{
		//					if (_t == 10000)
		//						cv_buffer[addr_cv + 0] = 255;
		//					else if (_t == 30000)
		//						cv_buffer[addr_cv + 1] = 255;
		//					//addr_cv -= 160 * 3;
		//					//cv_buffer[addr_cv + 0] = 255;
		//					//cv_buffer[addr_cv + 1] = 0;
		//					//cv_buffer[addr_cv + 2] = 0;
		//				}
		//				if (_r > 0)
		//				{
		//					//addr_cv -= 160 * 3;
		//					cv_buffer[addr_cv + 2] = 255;
		//				}
		//			}
		//		}
		//	}
		//	dx11DeviceImmContext->Unmap(pDeepBufferThick_SYS, 0);
		//	dx11DeviceImmContext->Unmap(pTexCnt_SYS, 0);
		//	cv::cvtColor(img_test, img_test, CV_RGB2BGR);
		//	cv::imwrite("d:/oit_test_1st.bmp", img_test);
		//	VMSAFE_RELEASE(pDeepBufferThick_SYS);
		//}
#pragma endregion // 1st RENDERING PASS
	}

	if (RENDERER_LOOP == 1)
		goto RENDERER_LOOP_EXIT;
	RENDERER_LOOP++;
	goto RENDERER_LOOP;

RENDERER_LOOP_EXIT:
#pragma region // 2nd RENDERING PASS
	// sorting and merging

	dx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(2, dx11RTVsNULL, dx11DSVNULL, 2, 5, dx11UAVs_NULL, 0);

	ID3D11UnorderedAccessView* dx11UAVs_2nd_pass[5] = {
			  (ID3D11UnorderedAccessView*)gres_fb_deep_vis.alloc_res_ptrs[BTYPE_UAV]
			, (ID3D11UnorderedAccessView*)gres_fb_deep_depth.alloc_res_ptrs[BTYPE_UAV]
			, (ID3D11UnorderedAccessView*)gres_fb_deep_thick.alloc_res_ptrs[BTYPE_UAV]
			, (ID3D11UnorderedAccessView*)gres_fb_rgba.alloc_res_ptrs[BTYPE_UAV]
			, (ID3D11UnorderedAccessView*)gres_fb_depthcs.alloc_res_ptrs[BTYPE_UAV]
			//, (ID3D11UnorderedAccessView*)gres_fb_counter.alloc_res_ptrs[BTYPE_UAV] // 나주에 SRV 로 빼기..
	};
	UINT UAVInitialCounts = 0;
	
	dx11DeviceImmContext->CSSetShader(apply_avrmerge ? GETCS(SR_OIT_SORT2RENDER_cs_5_0) : GETCS(SR_OIT_SORT2RENDER_SEQ_cs_5_0), NULL, 0);
	dx11DeviceImmContext->CSSetUnorderedAccessViews(1, 5, dx11UAVs_2nd_pass, (UINT*)(&dx11UAVs_2nd_pass));
	dx11DeviceImmContext->CSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&gres_fb_counter.alloc_res_ptrs[BTYPE_SRV]);
	// 나중에 1D 로 변경... 
	dx11DeviceImmContext->Flush();
	dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);
	
	// Set NULL States //
	ID3D11ShaderResourceView* dx11SRV_NULL = NULL;
	ID3D11UnorderedAccessView* dx11UAV_NULL = NULL;
	dx11DeviceImmContext->CSSetUnorderedAccessViews(1, 5, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
	dx11DeviceImmContext->CSSetShaderResources(0, 1, &dx11SRV_NULL);
#pragma endregion // 2nd RENDERING PASS
	//printf("# Textures : %d, # Drawing : %d, # RTBuffer Change : %d, # Merging : %d\n", iNumTexureLayers, iCountRendering, iCountRTBuffers, iCountMerging);

	//cout << "TEST VZ : " << v_thickness << endl;
	iobj->RegisterCustomParameter("_int_NumCallRenders", count_call_render);
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
			dx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gres_fb_sys_rgba.alloc_res_ptrs[BTYPE_RES], 
				(ID3D11Texture2D*)gres_fb_rgba.alloc_res_ptrs[BTYPE_RES]);
			dx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gres_fb_sys_depthcs.alloc_res_ptrs[BTYPE_RES], 
				(ID3D11Texture2D*)gres_fb_depthcs.alloc_res_ptrs[BTYPE_RES]);

			vmbyte4* rgba_sys_buf = (vmbyte4*)fb_rout->fbuffer;
			float* depth_sys_buf = (float*)fb_dout->fbuffer;

			D3D11_MAPPED_SUBRESOURCE mappedResSysRGBA;
			D3D11_MAPPED_SUBRESOURCE mappedResSysDepth;
			HRESULT hr = dx11DeviceImmContext->Map((ID3D11Texture2D*)gres_fb_sys_rgba.alloc_res_ptrs[BTYPE_RES], 0, D3D11_MAP_READ, NULL, &mappedResSysRGBA);
			hr = dx11DeviceImmContext->Map((ID3D11Texture2D*)gres_fb_sys_depthcs.alloc_res_ptrs[BTYPE_RES], 0, D3D11_MAP_READ, NULL, &mappedResSysDepth);

			vmbyte4* rgba_gpu_buf = (vmbyte4*)mappedResSysRGBA.pData;
			if (rgba_gpu_buf == NULL || depth_sys_buf == NULL)
			{
				test_out("SR ERROR -- OUT");
				test_out("screen : " + to_string(fb_size_cur.x) + " x " + to_string(fb_size_cur.y));
				test_out("v_thickness : " + to_string(fv_thickness));
				test_out("num_deep_layers : " + to_string(num_deep_layers));
				test_out("default_line_thickness : " + to_string(default_line_thickness));
				test_out("width and height max : " + to_string(_w_max) + " x " + to_string(_h_max));
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
			}
			float* depth_gpu_buf = (float*)mappedResSysDepth.pData;
			int buf_row_pitch = mappedResSysRGBA.RowPitch / 4;
#ifdef PPL_USE
			int count = fb_size_cur.y;
			parallel_for(int(0), count, [is_rgba, fb_size_cur, rgba_sys_buf, depth_sys_buf, rgba_gpu_buf, depth_gpu_buf, buf_row_pitch](int i)
#else
#pragma omp parallel for 
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
			dx11DeviceImmContext->Unmap((ID3D11Texture2D*)gres_fb_sys_rgba.alloc_res_ptrs[BTYPE_RES], 0);
			dx11DeviceImmContext->Unmap((ID3D11Texture2D*)gres_fb_sys_depthcs.alloc_res_ptrs[BTYPE_RES], 0);
#pragma endregion
	}	// if (iCountDrawing == 0)
}

	dx11DeviceImmContext->ClearState();

	dx11DeviceImmContext->OMSetRenderTargets(1, &pdxRTVOld, pdxDSVOld);
	VMSAFE_RELEASE(pdxRTVOld);
	VMSAFE_RELEASE(pdxDSVOld);

	QueryPerformanceFrequency(&lIntFreq);
	QueryPerformanceCounter(&lIntCntEnd);

	iobj->SetDescriptor("vismtv_inbuilt_renderergpudx module");
	
	auto AsDouble = [](unsigned long long _v) -> double
	{
		double d_v;
		memcpy(&d_v, &_v, sizeof(double));
		return d_v;
	};
	iobj->RegisterCustomParameter("_double_LatestSrTime", AsDouble(vmhelpers::GetCurrentTimePack()));
	// Time Check
	double dRunTime = (lIntCntEnd.QuadPart - lIntCntStart.QuadPart) / (double)(lIntFreq.QuadPart);
	if (run_time_ptr)
		*run_time_ptr = dRunTime;

	//((std::mutex*)HDx11GetMutexGpuCriticalPath())->unlock();

	return true;
}


