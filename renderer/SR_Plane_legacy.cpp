#include "RendererHeader_legacy.h"

using namespace grd_helper_legacy;

bool RenderSrOnPlane(VmFnContainer* _fncontainer,
	VmGpuManager* pCGpuManager,
	GpuDX11CommonParametersOld* pdx11CommonParams,
	ID3D11InputLayout* pdx11ILs[NUMINPUTLAYOUTS],
	ID3D11VertexShader** ppdx11CommonVSs[NUMSHADERS_SR_VS],
	ID3D11VertexShader** ppdx11PlaneVSs[NUMSHADERS_PLANE_SR_VS],
	ID3D11PixelShader** ppdx11PSs[NUMSHADERS_SR_PS],
	ID3D11ComputeShader** ppdx11CS_Merges[NUMSHADERS_MERGE_CS],
	LocalProgress* pLocalProgress,
	double* pdRunTime)
{
	LARGE_INTEGER lIntFreq;
	LARGE_INTEGER lIntCntStart, lIntCntEnd;

	vector<VmObject*> vtrInputPrimitives;
	

	_fncontainer->GetVmObjectList(&vtrInputPrimitives, VmObjKey(ObjectTypePRIMITIVE, true));
	VmLObject* lobj = (VmLObject*)_fncontainer->GetVmObject(VmObjKey(ObjectTypeBUFFER, true), 0);
	VmIObject* pCOutputIObject = (VmIObject*)_fncontainer->GetVmObject(VmObjKey(ObjectTypeIMAGEPLANE, false), 0);
	
	vector<VmObject*> vtrInputVolumes;
	_fncontainer->GetVmObjectList(&vtrInputVolumes, VmObjKey(ObjectTypeVOLUME, true));
	vector<VmObject*> vtrInputTObjects;
	_fncontainer->GetVmObjectList(&vtrInputTObjects, VmObjKey(ObjectTypeTMAP, true));

	map<int, VmObject*> mapAssociatedObjects;
	for (int i = 0; i < vtrInputVolumes.size(); i++)
		mapAssociatedObjects.insert(pair<int, VmObject*>(vtrInputVolumes[i]->GetObjectID(), vtrInputVolumes[i]));
	for (int i = 0; i < vtrInputTObjects.size(); i++)
		mapAssociatedObjects.insert(pair<int, VmObject*>(vtrInputTObjects[i]->GetObjectID(), vtrInputTObjects[i]));

	bool bIsAntiAliasingRS = false;
	int iNumTexureLayers = NUM_TEXRT_LAYERS;
	double dVThickness = 0;
	int iLevelSR = 1;
	vmdouble4 d4GlobalShadingFactors = vmdouble4(0.4, 0.6, 0.2, 30);	// Emission, Diffusion, Specular, Specular Power

	// 1, 2, ..., NUM_TEXRT_LAYERS : 
	fncont_getparamvalue<int>(_fncontainer, &iNumTexureLayers, ("_int_NumTexureLayers"));
	iNumTexureLayers = min(iNumTexureLayers, NUM_TEXRT_LAYERS);
	iNumTexureLayers = max(iNumTexureLayers, 1);

	fncont_getparamvalue<bool>(_fncontainer, &bIsAntiAliasingRS, ("_bool_IsAntiAliasingRS"));
	fncont_getparamvalue<vmdouble4>(_fncontainer, &d4GlobalShadingFactors, ("_double4_ShadingFactorsForGlobalPrimitives"));
	fncont_getparamvalue<double>(_fncontainer, &dVThickness, ("_double_VZThickness"));
	fncont_getparamvalue<int>(_fncontainer, &iLevelSR, ("_int_LevelSR"));

	bool bIsFirstRenderer = false;
	fncont_getparamvalue<bool>(_fncontainer, &bIsFirstRenderer, "_bool_IsFirstRenderer");
	bool bIsFinalRenderer = false;
	fncont_getparamvalue<bool>(_fncontainer, &bIsFinalRenderer, "_bool_IsFinalRenderer");

	int iMergeLevel = min(iLevelSR, 1);

#pragma region // Shader Setting
	// Shader Re-Compile Setting //
	bool bReloadHLSLObjFiles = false;
	fncont_getparamvalue<bool>(_fncontainer, &bReloadHLSLObjFiles, ("_bool_ReloadHLSLObjFiles"));
	if (bReloadHLSLObjFiles)
	{
		string strNames_PS[NUMSHADERS_SR_PS] = {
			("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\SR_MAPPING_TEX1_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\SR_PHONG_TEX1COLOR_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\SR_PHONG_GLOBALCOLOR_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\SR_PHONG_GLOBALCOLOR_MARKERSONSURFACE_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\SR_PHONG_GLOBALCOLOR_MAXSHADING_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\SR_PHONG_GLOBALCOLOR_MAXSHADING_MARKERSONSURFACE_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\SR_VOLUME_DEVIATION_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\SR_CMM_LINE_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\SR_CMM_TEXT_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\SR_CMM_MULTI_TEXTS_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\SR_VOLUME_SUPERIMPOSE_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\SR_IMAGESTACK_MAP_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\SR_AIRWAYANALYSIS_ps_4_0")
		};
		for (int i = 0; i < NUMSHADERS_SR_PS; i++)
		{
			string strName = strNames_PS[i];

			FILE* pFile;
			if (fopen_s(&pFile, strName.c_str(), ("rb")) == 0)
			{
				fseek(pFile, 0, SEEK_END);
				ullong ullFileSize = ftell(pFile);
				fseek(pFile, 0, SEEK_SET);
				byte* pyRead = new byte[ullFileSize];
				fread(pyRead, sizeof(byte), ullFileSize, pFile);
				fclose(pFile);

				ID3D11PixelShader* pdx11PShader = NULL;
				if (pdx11CommonParams->pdx11Device->CreatePixelShader(pyRead, ullFileSize, NULL, &pdx11PShader) != S_OK)
				{
					VMERRORMESSAGE("SHADER COMPILE FAILURE!");
				}
				else
				{
					VMSAFE_RELEASE(*ppdx11PSs[i]);
					*ppdx11PSs[i] = pdx11PShader;
				}
				VMSAFE_DELETEARRAY(pyRead);
			}
		}

		string strNames_MG_CS[NUMSHADERS_MERGE_CS] = {
			("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_MERGE_LAYEROUT_TO_RENDEROUT_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_MERGE_SROUT_AND_LAYEROUT_TO_LAYEROUT_cs_5_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_MERGE_ADV_SROUT_AND_LAYEROUT_TO_LAYEROUT_cs_5_0")
		};
		for (int i = 0; i < NUMSHADERS_MERGE_CS; i++)
		{
			string strName = strNames_MG_CS[i];

			FILE* pFile;
			if (fopen_s(&pFile, strName.c_str(), ("rb")) == 0)
			{
				fseek(pFile, 0, SEEK_END);
				ullong ullFileSize = ftell(pFile);
				fseek(pFile, 0, SEEK_SET);
				byte* pyRead = new byte[ullFileSize];
				fread(pyRead, sizeof(byte), ullFileSize, pFile);
				fclose(pFile);

				ID3D11ComputeShader* pdx11CShader = NULL;
				if (pdx11CommonParams->pdx11Device->CreateComputeShader(pyRead, ullFileSize, NULL, &pdx11CShader) != S_OK)
				{
					VMERRORMESSAGE("SHADER COMPILE FAILURE!");
				}
				else
				{
					VMSAFE_RELEASE(*ppdx11CS_Merges[i]);
					*ppdx11CS_Merges[i] = pdx11CShader;
				}
				VMSAFE_DELETEARRAY(pyRead);
			}
		}
		/**/
	}

	ID3D11InputLayout* pdx11LayoutInputPos = (ID3D11InputLayout*)pdx11ILs[0];
	ID3D11InputLayout* pdx11LayoutInputPosNor = (ID3D11InputLayout*)pdx11ILs[1];
	ID3D11InputLayout* pdx11LayoutInputPosTex = (ID3D11InputLayout*)pdx11ILs[2];
	ID3D11InputLayout* pdx11LayoutInputPosNorTex = (ID3D11InputLayout*)pdx11ILs[3];
	ID3D11InputLayout* pdx11LayoutInputPosTTTex = (ID3D11InputLayout*)pdx11ILs[4];

	ID3D11VertexShader* pdx11VShader_Point = (ID3D11VertexShader*)*ppdx11CommonVSs[0];
	ID3D11VertexShader* pdx11VShader_PointNormal = (ID3D11VertexShader*)*ppdx11CommonVSs[1];
	ID3D11VertexShader* pdx11VShader_PointTexture = (ID3D11VertexShader*)*ppdx11CommonVSs[2];
	ID3D11VertexShader* pdx11VShader_PointNormalTexture = (ID3D11VertexShader*)*ppdx11CommonVSs[3];
	ID3D11VertexShader* pdx11VShader_PointTTTextures = (ID3D11VertexShader*)*ppdx11CommonVSs[4];
	ID3D11VertexShader* pdx11VShader_PointNormalTexture_Deviation = (ID3D11VertexShader*)*ppdx11CommonVSs[5];
	ID3D11VertexShader* pdx11VShader_Proxy = (ID3D11VertexShader*)*ppdx11CommonVSs[6];

	ID3D11VertexShader* pdx11VShader_Plane_Point = (ID3D11VertexShader*)*ppdx11PlaneVSs[0];
	ID3D11VertexShader* pdx11VShader_Plane_PointNormal = (ID3D11VertexShader*)*ppdx11PlaneVSs[1];
	ID3D11VertexShader* pdx11VShader_Plane_PointTexture = (ID3D11VertexShader*)*ppdx11PlaneVSs[2];
	ID3D11VertexShader* pdx11VShader_Plane_PointNormalTexture = (ID3D11VertexShader*)*ppdx11PlaneVSs[3];
	ID3D11VertexShader* pdx11VShader_Plane_PointNormalTexture_Deviation = (ID3D11VertexShader*)*ppdx11PlaneVSs[4];


#pragma endregion // Shader Setting
	
#pragma region // IOBJECT CPU
	if (pCOutputIObject->GetFrameBuffer(FrameBufferUsageRENDEROUT, 0) == NULL)
		pCOutputIObject->InsertFrameBuffer(data_type::dtype<vmbyte4>(), FrameBufferUsageRENDEROUT, ("Frame buffer OUT : defined in vismtv_inbuilt_renderergpudx module"));

	if (bIsFinalRenderer)
	{
		pCOutputIObject->DeleteFrameBuffer(FrameBufferUsageRENDEROUT, 1);
	}
	else
	{
		if (pCOutputIObject->GetFrameBuffer(FrameBufferUsageRENDEROUT, 1) == NULL)
			pCOutputIObject->InsertFrameBuffer(data_type::dtype<vmbyte4>(), FrameBufferUsageRENDEROUT, ("Frame buffer back : defined in vismtv_inbuilt_renderergpudx module"));
	}

	while (pCOutputIObject->GetFrameBuffer(FrameBufferUsageDEPTH, 1) != NULL)
		pCOutputIObject->DeleteFrameBuffer(FrameBufferUsageDEPTH, 1);
	if (!pCOutputIObject->ReplaceFrameBuffer(FrameBufferUsageDEPTH, 0, data_type::dtype<float>(), ("1st hit screen depth frame buffer : defined in vismtv_inbuilt_renderergpudx module")))
		pCOutputIObject->InsertFrameBuffer(data_type::dtype<float>(), FrameBufferUsageDEPTH, ("1st hit screen depth frame buffer : defined in vismtv_inbuilt_renderergpudx module"));


	FrameBuffer* pstFrameBuffer = (FrameBuffer*)pCOutputIObject->GetFrameBuffer(FrameBufferUsageRENDEROUT, 0);
	FrameBuffer* pstFrameBufferBack = (FrameBuffer*)pCOutputIObject->GetFrameBuffer(FrameBufferUsageRENDEROUT, 1);
	FrameBuffer* pstFrameBufferDepth = (FrameBuffer*)pCOutputIObject->GetFrameBuffer(FrameBufferUsageDEPTH, 0);

	vmbyte4* py4Buffer = NULL;
	if (bIsFinalRenderer)
		py4Buffer = (vmbyte4*)pstFrameBuffer->fbuffer;
	else 
		py4Buffer = (vmbyte4*)pstFrameBufferBack->fbuffer;
	float* pfBuffer = (float*)pstFrameBufferDepth->fbuffer;

	if (vtrInputPrimitives.size() == 0)
	{
		vmint2 i2SizeScreenCurrent;
		pCOutputIObject->GetFrameBufferInfo(&i2SizeScreenCurrent);

		if (bIsFinalRenderer)
			ZeroMemory(py4Buffer, i2SizeScreenCurrent.x * i2SizeScreenCurrent.y * sizeof(vmbyte4));

		pCOutputIObject->RegisterCustomParameter("_bool_SkipSrOnPlaneRenderer", true);
		return true;
	}
#pragma endregion // IOBJECT CPU

	ID3D11Device* pdx11Device = pdx11CommonParams->pdx11Device;
	ID3D11DeviceContext* pdx11DeviceImmContext = pdx11CommonParams->pdx11DeviceImmContext;

#pragma region // IOBJECT GPU
	vmint2 i2SizeScreenCurrent, i2SizeScreenOld = vmint2(0, 0);
	pCOutputIObject->GetFrameBufferInfo(&i2SizeScreenCurrent);
	pCOutputIObject->GetCustomParameter("_int2_PreviousScreenSize", data_type::dtype<vmint2>(), &i2SizeScreenOld);
	if (i2SizeScreenCurrent.x != i2SizeScreenOld.x || i2SizeScreenCurrent.y != i2SizeScreenOld.y)
	{
		pCGpuManager->ReleaseGpuResourcesBySrcID(pCOutputIObject->GetObjectID());	// System Out 포함 //
		pCOutputIObject->RegisterCustomParameter(("_int2_PreviousScreenSize"), i2SizeScreenCurrent);
	}

	GpuRes gres_fbs[__GRES_FB_V1_COUNT];
	auto set_fbs = [](GpuRes gres_fbs[__GRES_FB_V1_COUNT], const VmIObject* iobj)
	{
		for (int i = 0; i < NUM_TEXRT_LAYERS; i++)
		{
			grd_helper_legacy::UpdateFrameBuffer(gres_fbs[__GRES_FB_V1_ROUT_RGBA_0 + i], iobj, "RENDER_OUT_RGBA_" + std::to_string(i),
				RTYPE_TEXTURE2D, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, false);
			grd_helper_legacy::UpdateFrameBuffer(gres_fbs[__GRES_FB_V1_ROUT_DEPTH_0 + i], iobj, "RENDER_OUT_DEPTH_" + std::to_string(i),
				RTYPE_TEXTURE2D, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_FLOAT, false);
		}
		grd_helper_legacy::UpdateFrameBuffer(gres_fbs[__GRES_FB_V1_DOUT_DS], iobj, "DEPTH_STENCIL",
			RTYPE_TEXTURE2D, D3D11_BIND_DEPTH_STENCIL, DXGI_FORMAT_D32_FLOAT, false);
		grd_helper_legacy::UpdateFrameBuffer(gres_fbs[__GRES_FB_V1_DEEP_0], iobj, "DEEP_LAYER_0",
			RTYPE_BUFFER, D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE, DXGI_FORMAT_UNKNOWN, false, 1, sizeof(RWB_Output_Layers));
		grd_helper_legacy::UpdateFrameBuffer(gres_fbs[__GRES_FB_V1_DEEP_1], iobj, "DEEP_LAYER_1",
			RTYPE_BUFFER, D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE, DXGI_FORMAT_UNKNOWN, false, 1, sizeof(RWB_Output_Layers));
		grd_helper_legacy::UpdateFrameBuffer(gres_fbs[__GRES_FB_V1_SYS_ROUT], iobj, "SYSTEM_OUT_RGBA",
			RTYPE_TEXTURE2D, NULL, DXGI_FORMAT_R8G8B8A8_UNORM, true);
		grd_helper_legacy::UpdateFrameBuffer(gres_fbs[__GRES_FB_V1_SYS_DOUT], iobj, "SYSTEM_OUT_DEPTH",
			RTYPE_TEXTURE2D, NULL, DXGI_FORMAT_R32_FLOAT, true);
	};

	set_fbs(gres_fbs, pCOutputIObject);

	ID3D11UnorderedAccessView* pUAV_Merge_PingpongCS[2] = { 
		(ID3D11UnorderedAccessView*)gres_fbs[__GRES_FB_V1_DEEP_0].alloc_res_ptrs[DTYPE_UAV], 
		(ID3D11UnorderedAccessView*)gres_fbs[__GRES_FB_V1_DEEP_1].alloc_res_ptrs[DTYPE_UAV] };
	ID3D11ShaderResourceView* pSRV_Merge_PingpongCS[2] = { 
		(ID3D11ShaderResourceView*)gres_fbs[__GRES_FB_V1_DEEP_0].alloc_res_ptrs[DTYPE_SRV],
		(ID3D11ShaderResourceView*)gres_fbs[__GRES_FB_V1_DEEP_1].alloc_res_ptrs[DTYPE_SRV] };
#pragma endregion // IOBJECT GPU

#pragma region // Presetting of VxObject
	auto find_asscociated_obj = [&mapAssociatedObjects](int obj_id) -> VmObject*
	{
		auto vol_obj = mapAssociatedObjects.find(obj_id);
		if (vol_obj == mapAssociatedObjects.end()) return NULL;
		return vol_obj->second;
	};

	map<int, GpuRes> mapGpuRes_VolumeAndTMap;
	map<int, GpuRes> mapGpuRes_Vtx;
	map<int, GpuRes> mapGpuRes_Idx;
	vector<RenderingObject> vtrValidPrimitives_FilledPlane;
	vector<RenderingObject> vtrValidPrimitives_3DonPlane_PriorOpaque;
	vector<RenderingObject> vtrValidPrimitives_3DonPlane_PerRTartget;
	uint num_prim_objs = (uint)vtrInputPrimitives.size();
	float leng_max_diag = 0;
	for (uint i = 0; i < num_prim_objs; i++)
	{
		VmVObjectPrimitive* prim_obj = (VmVObjectPrimitive*)vtrInputPrimitives[i];
		if (!prim_obj->IsDefined())
			continue;

		PrimitiveData* prim_data = prim_obj->GetPrimitiveData();

		const vmmat44f matOS2WS = prim_obj->GetMatrixOS2WSf();
		vmfloat3 f3PosAaBbMinWS, f3PosAaBbMaxWS;
		fTransformPoint(&f3PosAaBbMinWS, &(vmfloat3)prim_data->aabb_os.pos_min, &matOS2WS);
		fTransformPoint(&f3PosAaBbMaxWS, &(vmfloat3)prim_data->aabb_os.pos_max, &matOS2WS);
		leng_max_diag = max(leng_max_diag, fLengthVector(&(f3PosAaBbMinWS - f3PosAaBbMaxWS)));

		bool bIsVisibleItem = true;
		prim_obj->GetCustomParameter("_bool_visibility", data_type::dtype<bool>(), &bIsVisibleItem);
		if (!bIsVisibleItem)
			continue;

		int iVolumeID = 0, iTObjectID = 0;
		int iMeshObjID = prim_obj->GetObjectID();
		
		lobj->GetDstObjValue(iMeshObjID, ("_int_AssociatedVolumeID"), &iVolumeID);
		lobj->GetDstObjValue(iMeshObjID, ("_int_AssociatedTObjectID"), &iTObjectID);

		VmVObjectVolume* vol_obj = (VmVObjectVolume*)find_asscociated_obj(iVolumeID);
		VmTObject* tobj = (VmTObject*)find_asscociated_obj(iTObjectID);
		RegisterVolumeRes(vol_obj, tobj, lobj, pCGpuManager, pdx11DeviceImmContext, mapAssociatedObjects, mapGpuRes_VolumeAndTMap, pLocalProgress);

		GpuRes gres_vtx, gres_idx, gres_tex;
		grd_helper_legacy::UpdatePrimitiveModel(gres_vtx, gres_idx, gres_tex, prim_obj);
		if( gres_vtx.alloc_res_ptrs.size() > 0)
			mapGpuRes_Vtx.insert(pair<int, GpuRes>(iMeshObjID, gres_vtx));
		if (gres_idx.alloc_res_ptrs.size() > 0)
			mapGpuRes_Idx.insert(pair<int, GpuRes>(iMeshObjID, gres_idx));

		bool bIsFilledPlane = false;
		lobj->GetDstObjValue(iMeshObjID, ("_bool_IsFilledPlane"), &bIsFilledPlane);

		vmdouble4 d4Color(1.);
		prim_obj->GetCustomParameter("_double4_color", data_type::dtype<vmdouble4>(), &d4Color);
		vmfloat4 f4Color(d4Color), f4ColorWire;
		//bool bIsWireframe = false;
		//pCVMeshObj->GetPrimitiveWireframeVisibilityColor(&bIsWireframe, &f4ColorWire);

		RenderingObject stRenderingObj;
		stRenderingObj.pCMesh = prim_obj;
		stRenderingObj.f4Color = f4Color;
		stRenderingObj.is_wireframe = false;
		stRenderingObj.iNumSelfTransparentRenderPass = 1;

		if (bIsFilledPlane)
			vtrValidPrimitives_FilledPlane.push_back(stRenderingObj);
		else
		{
			if (f4Color.w > 0.99f)
			{
				bool bIsForcedPerRTarget = false;
				prim_obj->GetCustomParameter("_bool_IsForcedPerRTarget", data_type::dtype<bool>(), &bIsForcedPerRTarget);
				if (bIsForcedPerRTarget)
					vtrValidPrimitives_3DonPlane_PerRTartget.push_back(stRenderingObj);
				else
					vtrValidPrimitives_3DonPlane_PriorOpaque.push_back(stRenderingObj);
			}
			else
			{
				if (f4Color.w > 1.f / 255.f)
					vtrValidPrimitives_3DonPlane_PerRTartget.push_back(stRenderingObj);
			}
		}
	}
#pragma endregion // Presetting of VxObject

	if (vtrValidPrimitives_FilledPlane.size()
		+ vtrValidPrimitives_3DonPlane_PriorOpaque.size() +
		+vtrValidPrimitives_3DonPlane_PerRTartget.size() == 0)
	{
		if (bIsFinalRenderer)
			ZeroMemory(py4Buffer, i2SizeScreenCurrent.x * i2SizeScreenCurrent.y * sizeof(vmbyte4));

		pCOutputIObject->RegisterCustomParameter("_bool_SkipSrOnPlaneRenderer", true);
		return true;
	}
	pCOutputIObject->RegisterCustomParameter("_bool_SkipSrOnPlaneRenderer", false);

	if (dVThickness < 0)
	{
		if (leng_max_diag == 0)
		{
			dVThickness = 0.001;
		}
		else
		{
			dVThickness = leng_max_diag * 0.003;
		}
	}
	float fVZThickness = (float)dVThickness;

#pragma region // Camera & Light Setting
	uint uiNumGridX = (uint)ceil(i2SizeScreenCurrent.x / (float)__BLOCKSIZE);
	uint uiNumGridY = (uint)ceil(i2SizeScreenCurrent.y / (float)__BLOCKSIZE);

	// CONSIDER THAT THIS RENDERER HANDLES ONLY <ORTHOGONAL> PROJECTION!!
	VmCObject* pCCObject = pCOutputIObject->GetCameraObject();
	vmfloat3 f3PosEyeWS, f3VecViewWS, f3PosLightWS, f3VecLightWS;
	pCCObject->GetCameraExtStatef(&f3PosEyeWS, &f3VecViewWS, NULL);

	CB_VrCameraState cbVrCamState;
	grd_helper_legacy::SetCbVrCamera(&cbVrCamState, pCCObject, i2SizeScreenCurrent, &_fncontainer->vmparams);
	memcpy(&f3PosLightWS, &cbVrCamState.f3PosGlobalLightWS, sizeof(vmfloat3));
	memcpy(&f3VecLightWS, &cbVrCamState.f3VecGlobalLightWS, sizeof(vmfloat3));

	vmmat44 dmatWS2CS, dmatCS2PS, dmatPS2SS;
	vmmat44 dmatSS2PS, dmatPS2CS, dmatCS2WS;
	pCCObject->GetMatrixWStoSS(&dmatWS2CS, &dmatCS2PS, &dmatPS2SS);
	pCCObject->GetMatrixSStoWS(&dmatSS2PS, &dmatPS2CS, &dmatCS2WS);
	vmmat44f matWS2SS = (dmatWS2CS * dmatCS2PS) * dmatPS2SS;
	vmmat44f matSS2WS = (dmatSS2PS * dmatPS2CS) * dmatCS2WS;
	vmmat44f matWS2CS = dmatWS2CS;
	vmmat44f matCS2PS = dmatCS2PS;

	CB_SrProjectionCameraState cbSrCamState;
	cbSrCamState.dxmatSS2WS = *(XMMATRIX*)&matSS2WS;
	/////D3DXMatrixTranspose(&cbSrCamState.matSS2WS, &cbSrCamState.matSS2WS);
	cbSrCamState.dxmatWS2PS = *(XMMATRIX*)&(matWS2CS * matCS2PS);
	/////D3DXMatrixTranspose(&cbSrCamState.matWS2PS, &cbSrCamState.matWS2PS);

	cbSrCamState.f3PosEyeWS = *(XMFLOAT3*)&f3PosEyeWS;
	cbSrCamState.f3VecViewWS = *(XMFLOAT3*)&f3VecViewWS;
	cbSrCamState.f3PosLightWS = *(XMFLOAT3*)&f3PosLightWS;
	cbSrCamState.f3VecLightWS = *(XMFLOAT3*)&f3VecLightWS;
	cbSrCamState.iFlag = cbVrCamState.iFlags;

	D3D11_MAPPED_SUBRESOURCE mappedSrCamState;
	pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_CAMSTATE], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSrCamState);
	CB_SrProjectionCameraState* pCBCamStateParam = (CB_SrProjectionCameraState*)mappedSrCamState.pData;
	memcpy(pCBCamStateParam, &cbSrCamState, sizeof(CB_SrProjectionCameraState));
	pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_CAMSTATE], 0);
	pdx11DeviceImmContext->VSSetConstantBuffers(1, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_CAMSTATE]);
	pdx11DeviceImmContext->PSSetConstantBuffers(1, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_CAMSTATE]);
	pdx11DeviceImmContext->CSSetConstantBuffers(1, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_CAMSTATE]);

	// View Port Setting //
	D3D11_VIEWPORT dx11ViewPort;
	dx11ViewPort.Width = (float)i2SizeScreenCurrent.x;
	dx11ViewPort.Height = (float)i2SizeScreenCurrent.y;
	dx11ViewPort.MinDepth = 0;	// Dojo Check //
	dx11ViewPort.MaxDepth = 1.0f;
	dx11ViewPort.TopLeftX = 0;
	dx11ViewPort.TopLeftY = 0;
	pdx11DeviceImmContext->RSSetViewports(1, &dx11ViewPort);

	// View List-Up //
	ID3D11ShaderResourceView* pdx11SRV_RenderOuts[NUM_TEXRT_LAYERS] = { NULL };
	ID3D11ShaderResourceView* pdx11SRV_DepthCSs[NUM_TEXRT_LAYERS] = { NULL };
	ID3D11RenderTargetView* pdx11RTV_RenderOuts[NUM_TEXRT_LAYERS] = { NULL };
	ID3D11RenderTargetView* pdx11RTV_DepthCSs[NUM_TEXRT_LAYERS] = { NULL };
	for(int i = 0; i < NUM_TEXRT_LAYERS; i++)
	{
		pdx11SRV_RenderOuts[i] = (ID3D11ShaderResourceView*)gres_fbs[__GRES_FB_V1_ROUT_RGBA_0 + i].alloc_res_ptrs[DTYPE_SRV];
		pdx11SRV_DepthCSs[i] = (ID3D11ShaderResourceView*)gres_fbs[__GRES_FB_V1_ROUT_DEPTH_0 + i].alloc_res_ptrs[DTYPE_SRV];

		pdx11RTV_RenderOuts[i] = (ID3D11RenderTargetView*)gres_fbs[__GRES_FB_V1_ROUT_RGBA_0 + i].alloc_res_ptrs[DTYPE_RTV];
		pdx11RTV_DepthCSs[i] = (ID3D11RenderTargetView*)gres_fbs[__GRES_FB_V1_ROUT_DEPTH_0 + i].alloc_res_ptrs[DTYPE_RTV];
	}

	ID3D11ShaderResourceView* pdx11SRV_4NULLs[NUM_TEXRT_LAYERS] = { NULL };
	ID3D11ShaderResourceView* pdx11SRV_2NULLs[2] = { NULL };
	ID3D11RenderTargetView* pdx11RTV_2NULLs[2] = { NULL };
#pragma endregion // Camera & Light Setting
	
#pragma region // FrameBuffer Setting
	// Backup Previous Render Target //
	ID3D11RenderTargetView* pdxRTVOld = NULL;
	ID3D11DepthStencilView* pdxDSVOld = NULL;
	pdx11DeviceImmContext->OMGetRenderTargets(1, &pdxRTVOld, &pdxDSVOld);
	
	uint uiClearVlaues[4] = { 0, 0, 0, 0 };
	float fClearColor[4] = { 0, 0, 0, 0 };
	float fClearDepth[4] = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };
	// Clear RTT //
	for (int i = 0; i < NUM_TEXRT_LAYERS; i++)
	{
		pdx11DeviceImmContext->ClearRenderTargetView(pdx11RTV_RenderOuts[i], fClearColor);
		pdx11DeviceImmContext->ClearRenderTargetView(pdx11RTV_DepthCSs[i], fClearDepth);
	}
	// Clear Depth Stencil //
	ID3D11DepthStencilView* pdx11DSV = (ID3D11DepthStencilView*)gres_fbs[__GRES_FB_V1_DOUT_DS].alloc_res_ptrs[DTYPE_DSV];
	pdx11DeviceImmContext->ClearDepthStencilView(pdx11DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

	// Clear Merge Buffers //
	for (int i = 0; i < 2; i++)
		pdx11DeviceImmContext->ClearUnorderedAccessViewUint(pUAV_Merge_PingpongCS[i], uiClearVlaues);
#pragma endregion // FrameBuffer Setting

#pragma region // Other Presetting For Shaders

	// N/A
	pdx11DeviceImmContext->VSSetSamplers(0, 1, &pdx11CommonParams->pdx11SStates[__SState_LINEAR_ZEROBORDER]);
	pdx11DeviceImmContext->VSSetSamplers(1, 1, &pdx11CommonParams->pdx11SStates[__SState_POINT_ZEROBORDER]);
	pdx11DeviceImmContext->PSSetSamplers(0, 1, &pdx11CommonParams->pdx11SStates[__SState_LINEAR_ZEROBORDER]);
	pdx11DeviceImmContext->PSSetSamplers(1, 1, &pdx11CommonParams->pdx11SStates[__SState_POINT_ZEROBORDER]);

	// Proxy Setting //
	D3D11_MAPPED_SUBRESOURCE mappedProxyState;
	pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_PROXY], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedProxyState);
	CB_SrProxy* pCBProxyParam = (CB_SrProxy*)mappedProxyState.pData;
	pCBProxyParam->fVZThickness = fVZThickness;
	pCBProxyParam->uiGridOffsetX = cbVrCamState.uiGridOffsetX;
	pCBProxyParam->uiScreenSizeX = cbVrCamState.uiScreenSizeX;
	pCBProxyParam->uiScreenSizeY = cbVrCamState.uiScreenSizeY;
	pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_PROXY], 0);
	pdx11DeviceImmContext->CSSetConstantBuffers(10, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_PROXY]);
#pragma endregion // Other Presetting For Shaders

	QueryPerformanceCounter(&lIntCntStart);

#define __PLANE_RENDER_MODE_FILLED 0
#define __PLANE_RENDER_MODE_3D 1
	int iLOOPMODE = __PLANE_RENDER_MODE_FILLED;
	int iRENDERER_LOOP = 0;
	int iCountRendering[2] = { 0 };	// Draw 불린 횟 수
	int iCountRTBuffers = 0;	// Render Target Index 가 바뀌는 횟 수
	int iCountMerging = 0;		// Merging 불린 횟 수

MODE_LOOP:
RENDERER_LOOP :

	vector<RenderingObject>* pvtrValidPrimitives = NULL;
	if (iLOOPMODE == __PLANE_RENDER_MODE_FILLED)
		pvtrValidPrimitives = &vtrValidPrimitives_FilledPlane;
	else
	{
		if (iRENDERER_LOOP == 0)
			pvtrValidPrimitives = &vtrValidPrimitives_3DonPlane_PriorOpaque;
		else
			pvtrValidPrimitives = &vtrValidPrimitives_3DonPlane_PerRTartget;
	}

	// For Each Primitive //
#pragma region // Main Render Pass
	for (uint iIndexMeshObj = 0; iIndexMeshObj < (int)pvtrValidPrimitives->size(); iIndexMeshObj++)
	{
		RenderingObject stRenderingObj = pvtrValidPrimitives->at(iIndexMeshObj);
		VmVObjectPrimitive* pCPrimitiveObject = stRenderingObj.pCMesh;
		PrimitiveData* pstPrimitiveArchive = (PrimitiveData*)pCPrimitiveObject->GetPrimitiveData();
		if (pstPrimitiveArchive->GetVerticeDefinition("POSITION") == NULL)
			continue;

		int iMeshObjID = pCPrimitiveObject->GetObjectID();

		// Object Unit Conditions
		int iVolumeID = 0, iTObjectID = 0;
		bool bIsForcedCullOff = false;
		bool bIsAirwayAnalysis = false;
		bool bIsInvertPlaneDirection = false;
		vmdouble4 d4ShadingFactors = d4GlobalShadingFactors;	// Emission, Diffusion, Specular, Specular Power
		vmmat44 dmatClipBoxWS2BS;
		vmdouble3 d3PosOrthoMaxClipBoxWS;
		vmdouble3 d3PosClipPlaneWS;
		vmdouble3 d3VecClipPlaneWS;
		int iClippingMode = 0; // CLIPBOX / CLIPPLANE / BOTH //
		pCPrimitiveObject->GetCustomParameter(("_bool_IsForcedCullOff"), data_type::dtype<bool>(), &bIsForcedCullOff);
		pCPrimitiveObject->GetCustomParameter(("_bool_IsInvertPlaneDirection"), data_type::dtype<bool>(), &bIsInvertPlaneDirection);

#define __RENDERER_WITHOUT_VOLUME 0
#define __RENDERER_VOLUME_SAMPLE_AND_MAP 1
#define __RENDERER_DIFFERENCE_DIVIATION_FROM_VOLUME 2
#define __RENDERER_IMAGE_STACK_MAP_PER_MESHSPRITE 3

		lobj->GetDstObjValue(iMeshObjID, ("_int_AssociatedVolumeID"), &iVolumeID);
		lobj->GetDstObjValue(iMeshObjID, ("_int_AssociatedTObjectID"), &iTObjectID);
		lobj->GetDstObjValue(iMeshObjID, ("_double4_ShadingFactors"), &d4ShadingFactors);
		lobj->GetDstObjValue(iMeshObjID, ("_int_ClippingMode"), &iClippingMode);	// 0 : No, 1 : CLIPPLANE, 2 : CLIPBOX, 3 : BOTH
		lobj->GetDstObjValue(iMeshObjID, ("_double3_PosClipPlaneWS"), &d3PosClipPlaneWS);
		lobj->GetDstObjValue(iMeshObjID, ("_double3_VecClipPlaneWS"), &d3VecClipPlaneWS);
		lobj->GetDstObjValue(iMeshObjID, ("_double3_PosClipBoxMaxWS"), &d3PosOrthoMaxClipBoxWS);
		lobj->GetDstObjValue(iMeshObjID, ("_matrix44_MatrixClipWS2BS"), &dmatClipBoxWS2BS);
		lobj->GetDstObjValue(iMeshObjID, ("_bool_IsAirwayAnalysis"), &bIsAirwayAnalysis);

		if (iLOOPMODE == __PLANE_RENDER_MODE_FILLED)
			d4ShadingFactors = vmdouble4(1, 0, 0, 1);

		auto itrGpuVolume = mapGpuRes_VolumeAndTMap.find(iVolumeID);
		auto itrGpuTObject = mapGpuRes_VolumeAndTMap.find(iTObjectID);
		
		if (bIsAirwayAnalysis)
		{
			auto itrTObject = mapAssociatedObjects.find(iTObjectID);
			if (itrTObject != mapAssociatedObjects.end())
			{
				VmTObject* pCTObject = (VmTObject*)itrTObject->second;
				CB_VrOtf1D cbVrOtf1D;
				grd_helper_legacy::SetCbVrOtf1D(&cbVrOtf1D, pCTObject, 1, &_fncontainer->vmparams);
				D3D11_MAPPED_SUBRESOURCE mappedResOtf1D;
				pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_OTF1D], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResOtf1D);
				CB_VrOtf1D* pCBParamOtf1D = (CB_VrOtf1D*)mappedResOtf1D.pData;
				memcpy(pCBParamOtf1D, &cbVrOtf1D, sizeof(CB_VrOtf1D));
				pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_OTF1D], 0);
				pdx11DeviceImmContext->PSSetConstantBuffers(7, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_OTF1D]);

				vmdouble2 d2DeviationValueMinMax(0, 1);
				lobj->GetDstObjValue(iMeshObjID, ("_double_DeviationMinValue"), &d2DeviationValueMinMax.x);
				lobj->GetDstObjValue(iMeshObjID, ("_double_DeviationMaxValue"), &d2DeviationValueMinMax.y);
				int iMappingValueRange = 512;
				lobj->GetDstObjValue(iMeshObjID, ("_int_MappingValueRange"), &iMappingValueRange);
				int iNumControlPoints = 1;
				lobj->GetDstObjValue(iMeshObjID, ("_int_NumAirwayControls"), &iNumControlPoints);

				CB_SrDeviation cbSrDeviation;
				cbSrDeviation.fMinMappingValue = (float)d2DeviationValueMinMax.x;
				cbSrDeviation.fMaxMappingValue = (float)d2DeviationValueMinMax.y;
				cbSrDeviation.iValueRange = iMappingValueRange;
				cbSrDeviation.iChannelID = 0;
				cbSrDeviation.iIsoValueForVolume = 0;
				cbSrDeviation.iSrDevDummy__0 = iNumControlPoints;
				D3D11_MAPPED_SUBRESOURCE mappedDeviation;
				pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_DEVIATION], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedDeviation);
				CB_SrDeviation* pCBPolygonDeviation = (CB_SrDeviation*)mappedDeviation.pData;
				memcpy(pCBPolygonDeviation, &cbSrDeviation, sizeof(CB_SrDeviation));
				pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_DEVIATION], 0);
				pdx11DeviceImmContext->VSSetConstantBuffers(5, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_DEVIATION]);
				pdx11DeviceImmContext->PSSetConstantBuffers(5, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_DEVIATION]);

				RegisterVolumeRes(0, pCTObject, lobj, pCGpuManager, pdx11DeviceImmContext, mapAssociatedObjects, mapGpuRes_VolumeAndTMap, pLocalProgress);

#pragma region AIRWAY RESOURCE (STBUFFER) and VIEW
				GpuRes gres_aw;
				gres_aw.vm_src_id = lobj->GetObjectID();
				gres_aw.res_name = "BUFFER_AIRWAY_CONTROLS";

				if (!pCGpuManager->UpdateGpuResource(gres_aw))
				{
					size_t bytes_tmp;
					vmdouble3* pAirwayControlPoints = NULL;
					lobj->LoadBufferPtr(("_vlist_DOUBLE3_AirwayPathPointWS"), (void**)&pAirwayControlPoints, bytes_tmp);
					double* pAirwayAreaPoints = NULL;
					lobj->LoadBufferPtr(("_vlist_DOUBLE_AirwayCrossSectionalAreaWS"), (void**)&pAirwayAreaPoints, bytes_tmp);
					vmdouble3* pAirwayTVectors = NULL;
					lobj->LoadBufferPtr(("_vlist_DOUBLE3_AirwayPathTangentVectorWS"), (void**)&pAirwayTVectors, bytes_tmp);

					gres_aw.rtype = RTYPE_BUFFER;
					gres_aw.options["USAGE"] = D3D11_USAGE_DYNAMIC;
					gres_aw.options["CPU_ACCESS_FLAG"] = D3D11_CPU_ACCESS_WRITE;
					gres_aw.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE;
					gres_aw.options["FORMAT"] = DXGI_FORMAT_R32G32B32A32_FLOAT;
					gres_aw.res_dvalues["NUM_ELEMENTS"] = iNumControlPoints;
					gres_aw.res_dvalues["STRIDE_BYTES"] = sizeof(vmfloat4);

					pCGpuManager->GenerateGpuResource(gres_aw);

					D3D11_MAPPED_SUBRESOURCE d11MappedRes;
					pdx11DeviceImmContext->Map((ID3D11Resource*)gres_aw.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes);
					vmfloat4* pf4ControlPointAndArea = (vmfloat4*)d11MappedRes.pData;
					for (int iIndexControl = 0; iIndexControl < iNumControlPoints; iIndexControl++)
					{
						vmfloat3 f3PosPoint = vmfloat3(pAirwayControlPoints[iIndexControl]);
						float fArea = float(pAirwayAreaPoints[iIndexControl]);

						pf4ControlPointAndArea[iIndexControl] = vmfloat4(f3PosPoint.x, f3PosPoint.y, f3PosPoint.z, fArea);
					}
					pdx11DeviceImmContext->Unmap((ID3D11Resource*)gres_aw.alloc_res_ptrs[DTYPE_RES], 0);
				}
#pragma endregion
				// PSSetShaderResources
				pdx11DeviceImmContext->PSSetShaderResources(20, 1, (ID3D11ShaderResourceView**)&gres_aw.alloc_res_ptrs[DTYPE_SRV]);
				pdx11DeviceImmContext->PSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&itrGpuTObject->second.alloc_res_ptrs[DTYPE_SRV]);
			}
		}

		CB_SrPolygonObject cbPolygonObj;
		cbPolygonObj.fDepthForwardBias = 0;
		vmmat44f matOS2WS = pCPrimitiveObject->GetMatrixOS2WS();
#pragma region // Constant Buffers for Each Mesh Object //
		cbPolygonObj.dxmatOS2WS = *(XMMATRIX*)&matOS2WS;
		/////D3DXMatrixTranspose(&cbPolygonObj.matOS2WS, &cbPolygonObj.matOS2WS);

		cbPolygonObj.iFlag = 0x3 & iClippingMode;
		vmfloat3 f3ClipBoxMaxBS;
		vmmat44f matClipBoxWS2BS = dmatClipBoxWS2BS;
		fTransformPoint(&f3ClipBoxMaxBS, &vmfloat3(d3PosOrthoMaxClipBoxWS), &matClipBoxWS2BS);
		cbPolygonObj.f3ClipBoxMaxBS = *(XMFLOAT3*)&f3ClipBoxMaxBS;
		cbPolygonObj.dxmatClipBoxWS2BS = *(XMMATRIX*)&matClipBoxWS2BS;
		cbPolygonObj.f3PosClipPlaneWS = *(XMFLOAT3*)&vmfloat3(d3PosClipPlaneWS);
		cbPolygonObj.f3VecClipPlaneWS = *(XMFLOAT3*)&vmfloat3(d3VecClipPlaneWS);
		/////D3DXMatrixTranspose(&cbPolygonObj.matClipBoxWS2BS, &cbPolygonObj.matClipBoxWS2BS);

		cbPolygonObj.fDashDistance = 0;
		cbPolygonObj.f4Color = *(XMFLOAT4*)&vmfloat4(stRenderingObj.f4Color);
		cbPolygonObj.f4ShadingFactor = *(XMFLOAT4*)&vmfloat4(d4ShadingFactors);
		cbPolygonObj.fShadingMultiplier = 1.f;
		if (bIsInvertPlaneDirection)
			cbPolygonObj.fShadingMultiplier = -1.f;

		vmmat44f matOS2PS = matOS2WS * matWS2CS * matCS2PS;
		cbPolygonObj.dxmatOS2PS = *(XMMATRIX*)&matOS2PS;
		/////D3DXMatrixTranspose(&cbPolygonObj.matOS2PS, &cbPolygonObj.matOS2PS);

		D3D11_MAPPED_SUBRESOURCE mappedPolygonObjRes;
		pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_POLYGONOBJ], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedPolygonObjRes);
		CB_SrPolygonObject* pCBPolygonObjParam = (CB_SrPolygonObject*)mappedPolygonObjRes.pData;
		memcpy(pCBPolygonObjParam, &cbPolygonObj, sizeof(CB_SrPolygonObject));
		pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_POLYGONOBJ], 0);
		pdx11DeviceImmContext->VSSetConstantBuffers(0, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_POLYGONOBJ]);
		pdx11DeviceImmContext->PSSetConstantBuffers(0, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_POLYGONOBJ]);

#pragma endregion // Constant Buffers for Each Mesh Object //

#pragma region // Setting Rasterization Stages
		ID3D11InputLayout* pdx11InputLayer_Target = NULL;
		ID3D11VertexShader* pdx11VS_Target = NULL;
		ID3D11PixelShader* pdx11PS_Target = NULL;
		ID3D11RasterizerState* pdx11RState_TargetObj = NULL;
		uint uiStrideSizeInput = 0;
		uint uiOffset = 0;
		D3D_PRIMITIVE_TOPOLOGY ePRIMITIVE_TOPOLOGY;

		if (pstPrimitiveArchive->GetVerticeDefinition("NORMAL"))
		{
			if (pstPrimitiveArchive->GetVerticeDefinition("TEXCOORD0"))
			{
				// PNT
				pdx11InputLayer_Target = pdx11LayoutInputPosNorTex;
				pdx11VS_Target = pdx11VShader_PointNormalTexture;
				if (iLOOPMODE == __PLANE_RENDER_MODE_3D)
					pdx11VS_Target = pdx11VShader_Plane_PointNormalTexture;
				pdx11PS_Target = *ppdx11PSs[__PS_PHONG_TEX1COLOR];
			}
			else
			{
				// PN
				pdx11InputLayer_Target = pdx11LayoutInputPosNor;
				pdx11VS_Target = pdx11VShader_PointNormal;
				if (iLOOPMODE == __PLANE_RENDER_MODE_3D)
					pdx11VS_Target = pdx11VShader_Plane_PointNormal;
				pdx11PS_Target = *ppdx11PSs[__PS_PHONG_GLOBALCOLOR];
			}
		}
		else if (pstPrimitiveArchive->GetVerticeDefinition("TEXCOORD0"))
		{
			cbPolygonObj.f4ShadingFactor = XMFLOAT4(1.f, 1.f, 1.f, 1.f);
			// PT
			pdx11InputLayer_Target = pdx11LayoutInputPosTex;
			pdx11VS_Target = pdx11VShader_PointTexture;
			if (iLOOPMODE == __PLANE_RENDER_MODE_3D)
				pdx11VS_Target = pdx11VShader_Plane_PointTexture;
			pdx11PS_Target = *ppdx11PSs[__PS_MAPPING_TEX1];
		}
		else
		{
			cbPolygonObj.f4ShadingFactor = XMFLOAT4(1.f, 1.f, 1.f, 1.f);
			// P
			pdx11InputLayer_Target = pdx11LayoutInputPos;
			pdx11VS_Target = pdx11VShader_Point;
			if (iLOOPMODE == __PLANE_RENDER_MODE_3D)
				pdx11VS_Target = pdx11VShader_Plane_Point;
			pdx11PS_Target = *ppdx11PSs[__PS_MAPPING_TEX1];
		}

		if (iLOOPMODE == __PLANE_RENDER_MODE_3D)
		{
			if (bIsAirwayAnalysis)
				pdx11PS_Target = *ppdx11PSs[__PS_AIRWAY_ANALYSIS];
			else
				pdx11PS_Target = *ppdx11PSs[__PS_PHONG_GLOBALCOLOR];
		}

		switch (pstPrimitiveArchive->ptype)
		{
		case PrimitiveTypeLINE:
			if (pstPrimitiveArchive->is_stripe)
				ePRIMITIVE_TOPOLOGY = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
			else
				ePRIMITIVE_TOPOLOGY = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
			break;
		case PrimitiveTypeTRIANGLE:
			if (pstPrimitiveArchive->is_stripe)
				ePRIMITIVE_TOPOLOGY = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
			else
				ePRIMITIVE_TOPOLOGY = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			break;
		case PrimitiveTypePOINT:
			ePRIMITIVE_TOPOLOGY = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
			break;
		default:
			continue;
		}

		if (iLOOPMODE == __PLANE_RENDER_MODE_3D)
		{
			int iAntiAliasingIndex = 0;
			if (bIsAntiAliasingRS)
				iAntiAliasingIndex = __RState_ANTIALIASING_SOLID_CW;
			iAntiAliasingIndex = 0;	// Current Version does not support Anti-Aliasing!!

			EnumCullingMode eCullingMode = rxSR_CULL_NONE; // TO DO //
			if (stRenderingObj.is_wireframe)
			{
				pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_WIRE_NONE];
			}
			else
			{
				if (eCullingMode == rxSR_CULL_NONE || bIsForcedCullOff)
					pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_SOLID_NONE + iAntiAliasingIndex];
				else
				{
					if (pstPrimitiveArchive->is_ccw)
					{
						if (eCullingMode == rxSR_CULL_DEFAULT || eCullingMode == rxSR_CULL_FORCED_CW)
							pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_SOLID_CCW + iAntiAliasingIndex];
						else // CCW
							pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_SOLID_CW + iAntiAliasingIndex];
					}
					else
					{
						if (eCullingMode == rxSR_CULL_DEFAULT || eCullingMode == rxSR_CULL_FORCED_CCW)
							pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_SOLID_CW + iAntiAliasingIndex];
						else // CW
							pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_SOLID_CCW + iAntiAliasingIndex];
					}
				}
			}
		}

		auto itrMapBufferVtx = mapGpuRes_Vtx.find(iMeshObjID);
		ID3D11Buffer* pdx11BufferTargetMesh = (ID3D11Buffer*)itrMapBufferVtx->second.alloc_res_ptrs[DTYPE_RES];
		ID3D11Buffer* pdx11IndiceTargetMesh = NULL;
		uiStrideSizeInput = sizeof(vmfloat3)*(uint)pstPrimitiveArchive->GetNumVertexDefinitions();
		pdx11DeviceImmContext->IASetVertexBuffers(0, 1, (ID3D11Buffer**)&pdx11BufferTargetMesh, &uiStrideSizeInput, &uiOffset);
		if (pstPrimitiveArchive->vidx_buffer != NULL)
		{
			auto itrMapBufferIdx = mapGpuRes_Idx.find(iMeshObjID);
			pdx11IndiceTargetMesh = (ID3D11Buffer*)itrMapBufferIdx->second.alloc_res_ptrs[DTYPE_RES];
			pdx11DeviceImmContext->IASetIndexBuffer(pdx11IndiceTargetMesh, DXGI_FORMAT_R32_UINT, 0);
		}

		pdx11DeviceImmContext->IASetInputLayout(pdx11InputLayer_Target);
		pdx11DeviceImmContext->VSSetShader(pdx11VS_Target, NULL, 0);
		pdx11DeviceImmContext->PSSetShader(pdx11PS_Target, NULL, 0);
		pdx11DeviceImmContext->RSSetState(pdx11RState_TargetObj);
		pdx11DeviceImmContext->IASetPrimitiveTopology(ePRIMITIVE_TOPOLOGY);
#pragma endregion // Setting Rasterization Stages

		// Self-Transparent Surfaces //
		//int iNumSelfTransparentRenderPass = Always 1

		iCountRendering[iLOOPMODE]++;
		if (iLOOPMODE == __PLANE_RENDER_MODE_3D)
		{
			// Render Work //
#pragma region // RENDERING PASS
			if (iRENDERER_LOOP == 0)
			{
				iCountRTBuffers = 1;
				pdx11DeviceImmContext->OMSetDepthStencilState(pdx11CommonParams->pdx11DSStates[__DSState_DEPTHENABLE_LESS], 0);
			}
			else
			{
				iCountRTBuffers++;
			}

			int iModRTLayer = iCountRTBuffers % NUM_TEXRT_LAYERS;
			int iIndexRTT = (iCountRTBuffers + NUM_TEXRT_LAYERS - 1) % NUM_TEXRT_LAYERS;

			ID3D11RenderTargetView* pdx11RTVs[2] = {
				(ID3D11RenderTargetView*)gres_fbs[__GRES_FB_V1_ROUT_RGBA_0 + iIndexRTT].alloc_res_ptrs[DTYPE_RTV],
				(ID3D11RenderTargetView*)gres_fbs[__GRES_FB_V1_ROUT_DEPTH_0 + iIndexRTT].alloc_res_ptrs[DTYPE_RTV] };

			pdx11DeviceImmContext->OMSetRenderTargets(2, pdx11RTVs, pdx11DSV);

			if (pstPrimitiveArchive->is_stripe || pstPrimitiveArchive->ptype == PrimitiveTypePOINT)
				pdx11DeviceImmContext->Draw(pstPrimitiveArchive->num_vtx, 0);
			else
				pdx11DeviceImmContext->DrawIndexed(pstPrimitiveArchive->num_vidx, 0, 0);

			if (iRENDERER_LOOP == 0)
				continue;

			if (iModRTLayer == 0)
			{
				int iRSA_Index_Offset_Prev = RTTandLayersToLayersCS(pdx11DeviceImmContext, uiNumGridX, uiNumGridY,
					pdx11SRV_RenderOuts, pdx11SRV_DepthCSs, iCountMerging,
					pUAV_Merge_PingpongCS, pSRV_Merge_PingpongCS,
					pdx11CommonParams, ppdx11CS_Merges, pdx11SRV_4NULLs, pdx11SRV_2NULLs, pdx11RTV_2NULLs, iMergeLevel);

				// Clear //
				pdx11DeviceImmContext->ClearUnorderedAccessViewUint(pUAV_Merge_PingpongCS[iRSA_Index_Offset_Prev], uiClearVlaues);
				
				for (int m = 0; m < iNumTexureLayers; m++)
				{
					pdx11DeviceImmContext->ClearRenderTargetView(pdx11RTV_RenderOuts[m], fClearColor);
					pdx11DeviceImmContext->ClearRenderTargetView(pdx11RTV_DepthCSs[m], fClearDepth);
				}

				// NULL SETTING //
				pdx11DeviceImmContext->PSSetShaderResources(30, NUM_TEXRT_LAYERS, pdx11SRV_4NULLs);
				pdx11DeviceImmContext->PSSetShaderResources(40, NUM_TEXRT_LAYERS, pdx11SRV_4NULLs);

				iCountMerging++;
			}	// iModRTLayer == 0

			pdx11DeviceImmContext->ClearDepthStencilView(pdx11DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
#pragma endregion // SELF RENDERING PASS
		}
		else
		{
			ID3D11RenderTargetView* pdx11RTVs[2] = {
				(ID3D11RenderTargetView*)gres_fbs[__GRES_FB_V1_ROUT_RGBA_0].alloc_res_ptrs[DTYPE_RTV],
				(ID3D11RenderTargetView*)gres_fbs[__GRES_FB_V1_ROUT_DEPTH_0].alloc_res_ptrs[DTYPE_RTV] };

			pdx11DeviceImmContext->OMSetRenderTargets(2, pdx11RTVs, pdx11DSV);
			pdx11DeviceImmContext->OMSetDepthStencilState(pdx11CommonParams->pdx11DSStates[__DSState_DEPTHENABLE_LESS], 0);

			// 1st Step : Draw BackFace //
			pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_SOLID_CCW];
			if (pstPrimitiveArchive->is_ccw)
				pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_SOLID_CW];
			pdx11DeviceImmContext->RSSetState(pdx11RState_TargetObj);

			if (pstPrimitiveArchive->is_stripe || pstPrimitiveArchive->ptype == PrimitiveTypePOINT)
				pdx11DeviceImmContext->Draw(pstPrimitiveArchive->num_vtx, 0);
			else
				pdx11DeviceImmContext->DrawIndexed(pstPrimitiveArchive->num_vidx, 0, 0);
			pdx11DeviceImmContext->Flush();

			// 2nd Step : Draw FrontFace //
			pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_POLYGONOBJ], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedPolygonObjRes);
			CB_SrPolygonObject* pCBPolygonObjParam = (CB_SrPolygonObject*)mappedPolygonObjRes.pData;
			cbPolygonObj.f4Color = XMFLOAT4(0, 0, 0, 0);
			memcpy(pCBPolygonObjParam, &cbPolygonObj, sizeof(CB_SrPolygonObject));
			pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_POLYGONOBJ], 0);
			pdx11DeviceImmContext->VSSetConstantBuffers(0, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_POLYGONOBJ]);
			pdx11DeviceImmContext->PSSetConstantBuffers(0, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_POLYGONOBJ]);

			pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_SOLID_CW];
			if (pstPrimitiveArchive->is_ccw)
				pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_SOLID_CCW];
			pdx11DeviceImmContext->RSSetState(pdx11RState_TargetObj);

			if (pstPrimitiveArchive->is_stripe || pstPrimitiveArchive->ptype == PrimitiveTypePOINT)
				pdx11DeviceImmContext->Draw(pstPrimitiveArchive->num_vtx, 0);
			else
				pdx11DeviceImmContext->DrawIndexed(pstPrimitiveArchive->num_vidx, 0, 0);
			pdx11DeviceImmContext->Flush();
		}
	}
#pragma endregion // Main Render Pass
	
	if (iLOOPMODE == __PLANE_RENDER_MODE_3D)
	{
		if (iRENDERER_LOOP > 0)
			goto RENDERER_LOOP_EXIT;

		iRENDERER_LOOP++;
		goto RENDERER_LOOP;

RENDERER_LOOP_EXIT:

#pragma region // Final Rearrangement
		int iModRTLayer = iCountRTBuffers % NUM_TEXRT_LAYERS;
		bool bCall_RTTandLayersToLayers = iCountRTBuffers > 1 && iModRTLayer != 0;
		if (bCall_RTTandLayersToLayers)	// consider that this renderer is always bIsSystemOut == true
		{
			RTTandLayersToLayersCS(pdx11DeviceImmContext, uiNumGridX, uiNumGridY,
				pdx11SRV_RenderOuts, pdx11SRV_DepthCSs, iCountMerging,
				pUAV_Merge_PingpongCS, pSRV_Merge_PingpongCS,
				pdx11CommonParams, ppdx11CS_Merges, pdx11SRV_4NULLs, pdx11SRV_2NULLs, pdx11RTV_2NULLs, iMergeLevel);

			iCountMerging++;
		}

		//printf("MPR :: # Drawing : %d, # RTBuffer Change : %d, # Merging : %d\n", iCountRendering[0] + iCountRendering[1], iCountRTBuffers, iCountMerging);
#pragma endregion // Final Rearrangement
	}

	// Always, this renderer converts the gpu RT to cpu sys mem. 
#pragma region // Copy GPU to CPU	
	if (iLOOPMODE == 0) // __PLANE_RENDER_MODE_FILLED
	{
		if (iCountRendering[0] > 0)
		{
#pragma region // Plane3D(FILLED) to CPU
			pdx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gres_fbs[__GRES_FB_V1_SYS_ROUT].alloc_res_ptrs[DTYPE_RES], 
				(ID3D11Texture2D*)gres_fbs[__GRES_FB_V1_ROUT_RGBA_0].alloc_res_ptrs[DTYPE_RES]);

			D3D11_MAPPED_SUBRESOURCE mappedResSys;
			pdx11DeviceImmContext->Map((ID3D11Texture2D*)gres_fbs[__GRES_FB_V1_SYS_ROUT].alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, NULL, &mappedResSys);

			vmbyte4* py4RenderOuts = (vmbyte4*)mappedResSys.pData;
			int iGpuBufferOffset = mappedResSys.RowPitch / 4;

#ifdef PPL_USE
			int count = i2SizeScreenCurrent.y;
			parallel_for(int(0), count, [i2SizeScreenCurrent, py4RenderOuts, py4Buffer, iGpuBufferOffset](int i)
#else
#pragma omp parallel for 
			for (int i = 0; i < i2SizeScreenCurrent.y; i++)
#endif
			{
				for (int j = 0; j < i2SizeScreenCurrent.x; j++)
				{
					// __PS_MERGE_LAYERS_TO_RENDEROUT 에서 INT -> FLOAT4 로 되어 배열된 color 요소가 들어 옴. //
					vmbyte4 y4Renderout = py4RenderOuts[j + i * iGpuBufferOffset];
					vmbyte4 y4RenderoutCorrection;
					y4RenderoutCorrection.x = y4Renderout.z;	// B
					y4RenderoutCorrection.y = y4Renderout.y;	// G
					y4RenderoutCorrection.z = y4Renderout.x;	// R
					y4RenderoutCorrection.w = y4Renderout.w;	// A

					py4Buffer[i*i2SizeScreenCurrent.x + j] = y4RenderoutCorrection;
				}
#ifdef PPL_USE
			});
#else
			}
#endif
			pdx11DeviceImmContext->Unmap((ID3D11Texture2D*)gres_fbs[__GRES_FB_V1_SYS_ROUT].alloc_res_ptrs[DTYPE_RES], 0);
#pragma endregion // Plane3D(FILLED) to CPU
		}
	}
	else if (iLOOPMODE == 1) // __PLANE_RENDER_MODE_3D
	{
		if (iCountRendering[1] > 0)
		{
			pdx11DeviceImmContext->Flush();

#pragma region // Plane3D(3D) to CPU
			if (iCountMerging > 0)	// Layers to Render out
			{
				int iRSA_Index_Offset_Prev = 0;
				if (iCountMerging % 2 == 0)
				{
					iRSA_Index_Offset_Prev = 1;
				}

				ID3D11ShaderResourceView* pdx11SRV_Layers = pSRV_Merge_PingpongCS[iRSA_Index_Offset_Prev];
				pdx11DeviceImmContext->CSSetShaderResources(50, 1, &pdx11SRV_Layers);

				ID3D11UnorderedAccessView* pdx11UAV_Renderout = (ID3D11UnorderedAccessView*)gres_fbs[__GRES_FB_V1_ROUT_RGBA_0].alloc_res_ptrs[DTYPE_UAV];
				ID3D11UnorderedAccessView* pdx11UAV_DepthCS = (ID3D11UnorderedAccessView*)gres_fbs[__GRES_FB_V1_ROUT_DEPTH_0].alloc_res_ptrs[DTYPE_UAV];

				// ???? //
				UINT UAVInitialCounts = 0;
				pdx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, &pdx11UAV_Renderout, &UAVInitialCounts);
				pdx11DeviceImmContext->CSSetUnorderedAccessViews(2, 1, &pdx11UAV_DepthCS, &UAVInitialCounts);
				pdx11DeviceImmContext->CSSetShader(*ppdx11CS_Merges[__CS_MERGE_LAYERS_TO_RENDEROUT], NULL, 0);

				pdx11DeviceImmContext->Dispatch(uiNumGridX, uiNumGridY, 1);

				pdx11DeviceImmContext->Flush();
			}

#pragma region // Copy GPU to CPU
			pdx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gres_fbs[__GRES_FB_V1_SYS_ROUT].alloc_res_ptrs[DTYPE_RES],
				(ID3D11Texture2D*)gres_fbs[__GRES_FB_V1_ROUT_RGBA_0].alloc_res_ptrs[DTYPE_RES]);
			pdx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gres_fbs[__GRES_FB_V1_SYS_DOUT].alloc_res_ptrs[DTYPE_RES],
				(ID3D11Texture2D*)gres_fbs[__GRES_FB_V1_ROUT_DEPTH_0].alloc_res_ptrs[DTYPE_RES]);

			D3D11_MAPPED_SUBRESOURCE mappedResSys;
			pdx11DeviceImmContext->Map((ID3D11Texture2D*)gres_fbs[__GRES_FB_V1_SYS_DOUT].alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, NULL, &mappedResSys);
			float* pfDeferredContexts = (float*)mappedResSys.pData;
			int iGpuBufferOffset = mappedResSys.RowPitch / 4;
#ifdef PPL_USE
			int count = i2SizeScreenCurrent.y;
			parallel_for(int(0), count, [i2SizeScreenCurrent, pfDeferredContexts, pfBuffer, py4Buffer, iGpuBufferOffset](int i)
#else
#pragma omp parallel for 
			for (int i = 0; i < i2SizeScreenCurrent.y; i++)
#endif
			{
				for (int j = 0; j < i2SizeScreenCurrent.x; j++)
				{
					int iAddr = i * i2SizeScreenCurrent.x + j;
					pfBuffer[iAddr] = pfDeferredContexts[j + i * iGpuBufferOffset];
				}
#ifdef PPL_USE
			});
#else
			}
#endif
			pdx11DeviceImmContext->Unmap((ID3D11Texture2D*)gres_fbs[__GRES_FB_V1_SYS_DOUT].alloc_res_ptrs[DTYPE_RES], 0);


			pdx11DeviceImmContext->Map((ID3D11Texture2D*)gres_fbs[__GRES_FB_V1_SYS_ROUT].alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, NULL, &mappedResSys);
			vmbyte4* py4RenderGpuOut = (vmbyte4*)mappedResSys.pData;
			iGpuBufferOffset = mappedResSys.RowPitch / 4;

			if (iCountRendering[0] > 0)
			{
#ifdef PPL_USE
				int count = i2SizeScreenCurrent.y;
				parallel_for(int(0), count, [i2SizeScreenCurrent, py4RenderGpuOut, pfBuffer, py4Buffer, iGpuBufferOffset](int i)
#else
#pragma omp parallel for 
				for (int i = 0; i < i2SizeScreenCurrent.y; i++)
#endif
				{
					for (int j = 0; j < i2SizeScreenCurrent.x; j++)
					{
						int iAddr = i * i2SizeScreenCurrent.x + j;
						// MIXING //
						vmbyte4 y4Render3D = py4RenderGpuOut[j + i * iGpuBufferOffset];
						y4Render3D = vmbyte4(y4Render3D.z, y4Render3D.y, y4Render3D.x, y4Render3D.w);
						vmbyte4 y4RenderFilled = py4Buffer[iAddr];

						float fDepth3D = pfBuffer[iAddr];

						if (fDepth3D <= 0)
						{
							py4Buffer[iAddr] = y4Render3D;
						}
						else
						{
							py4Buffer[iAddr] = y4RenderFilled;
							pfBuffer[iAddr] = 0;
						}
					}
#ifdef PPL_USE
				});
#else
				}
#endif
			}
			else // iCountRendering[0] == 0
			{
#ifdef PPL_USE
				int count = i2SizeScreenCurrent.y;
				parallel_for(int(0), count, [i2SizeScreenCurrent, py4RenderGpuOut, py4Buffer, iGpuBufferOffset](int i)
#else
#pragma omp parallel for 
				for (int i = 0; i < i2SizeScreenCurrent.y; i++)
#endif
				{
					for (int j = 0; j < i2SizeScreenCurrent.x; j++)
					{
						int iAddr = i*i2SizeScreenCurrent.x + j;
						// MIXING //
						vmbyte4 y4Render3D = py4RenderGpuOut[j + i * iGpuBufferOffset];
						y4Render3D = vmbyte4(y4Render3D.z, y4Render3D.y, y4Render3D.x, y4Render3D.w);
						py4Buffer[iAddr] = y4Render3D;
					}
#ifdef PPL_USE
				});
#else
				}
#endif
			}

			pdx11DeviceImmContext->Unmap((ID3D11Texture2D*)gres_fbs[__GRES_FB_V1_SYS_ROUT].alloc_res_ptrs[DTYPE_RES], 0);
			
#pragma endregion
#pragma endregion // Plane3D(3D) to CPU
		}
		else // iCountRendering[1] == 0
		{
			if (iCountRendering[0] > 0)
			{
#ifdef PPL_USE
				int count = i2SizeScreenCurrent.y;
				parallel_for(int(0), count, [i2SizeScreenCurrent, pfBuffer, py4Buffer](int i)
#else
#pragma omp parallel for 
				for (int i = 0; i < i2SizeScreenCurrent.y; i++)
#endif
				{
					for (int j = 0; j < i2SizeScreenCurrent.x; j++)
					{
						int iAddr = i*i2SizeScreenCurrent.x + j;
						// MIXING //
						vmbyte4 y4RenderFilled = py4Buffer[iAddr];
						if (y4RenderFilled.w > 0)
							pfBuffer[iAddr] = 0;
						else
							pfBuffer[iAddr] = FLT_MAX;
					}
#ifdef PPL_USE
				});
#else
				}
#endif
			}
			else // iCountRendering[0] == 0
			{
				memset(py4Buffer, 0, i2SizeScreenCurrent.x * i2SizeScreenCurrent.y * sizeof(vmbyte4));
				memset(pfBuffer, 0x77, i2SizeScreenCurrent.x * i2SizeScreenCurrent.y * sizeof(float));
			}
		}
	}

#pragma endregion

	if (iLOOPMODE == 0) // __PLANE_RENDER_MODE_FILLED
	{
		iLOOPMODE = 1; //__PLANE_RENDER_MODE_3D;
		goto MODE_LOOP;
	}

	pdx11DeviceImmContext->ClearState();

	ID3D11RenderTargetView* pdxRTVOlds[2];
	pdxRTVOlds[0] = pdxRTVOld;
	pdxRTVOlds[1] = NULL;
	pdx11DeviceImmContext->OMSetRenderTargets(2, pdxRTVOlds, pdxDSVOld);

	VMSAFE_RELEASE(pdxRTVOld);
	VMSAFE_RELEASE(pdxDSVOld);

	QueryPerformanceFrequency(&lIntFreq);
	QueryPerformanceCounter(&lIntCntEnd);

	pCOutputIObject->SetDescriptor("vismtv_inbuilt_renderergpudx module");

	// Time Check
	double dRunTime = (lIntCntEnd.QuadPart - lIntCntStart.QuadPart) / (double)(lIntFreq.QuadPart);
	if (pdRunTime)
		*pdRunTime = dRunTime;

	return true;
}