#include "RendererHeader_legacy.h"
//#include "FreeImage.h"

#include <iostream>
#define __CS_5_0_SUPPORT


//_bool_IsForcedPerRTarget true
//_int_LevelSR 0
//_double_VZThickness 1
using namespace grd_helper_legacy;

bool RenderSrCommonCS(VmFnContainer* _fncontainer,
	VmGpuManager* pCGpuManager,
	GpuDX11CommonParametersOld* pdx11CommonParams,
	ID3D11InputLayout* pdx11ILs[NUMINPUTLAYOUTS],
	ID3D11VertexShader** ppdx11VS_CommonUsages[NUMSHADERS_SR_VS],
	ID3D11VertexShader** ppdx11VS_BiasZs[NUMSHADERS_BIASZ_SR_VS],
	ID3D11PixelShader** ppdx11PS_SRs[NUMSHADERS_SR_PS],
	ID3D11GeometryShader** ppdx11GS[NUMSHADERS_GS],
	ID3D11ComputeShader** ppdx11CS_Merges[NUMSHADERS_MERGE_CS],
	ID3D11ComputeShader** ppdx11CS_Outline,
	ID3D11Buffer* pdx11BufProxyRect,
	LocalProgress* pstLocalProgress,
	double* pdRunTime)
{
	LARGE_INTEGER lIntFreq;
	LARGE_INTEGER lIntCntStart, lIntCntEnd;

	vector<VmObject*> vtrInputPrimitives;
	_fncontainer->GetVmObjectList(&vtrInputPrimitives, VmObjKey(ObjectTypePRIMITIVE, true));

	vector<VmObject*> vtrInputVolumes;
	_fncontainer->GetVmObjectList(&vtrInputVolumes, VmObjKey(ObjectTypeVOLUME, true));
	vector<VmObject*> vtrInputTObjects;
	_fncontainer->GetVmObjectList(&vtrInputTObjects, VmObjKey(ObjectTypeTMAP, true));

	VmLObject* lobj = (VmLObject*)_fncontainer->GetVmObject(VmObjKey(ObjectTypeBUFFER, true), 0);

	map<int, VmObject*> mapAssociatedObjects;
	for (int i = 0; i < (int)vtrInputVolumes.size(); i++)
		mapAssociatedObjects.insert(pair<int, VmObject*>(vtrInputVolumes[i]->GetObjectID(), vtrInputVolumes[i]));
	for (int i = 0; i < (int)vtrInputTObjects.size(); i++)
		mapAssociatedObjects.insert(pair<int, VmObject*>(vtrInputTObjects[i]->GetObjectID(), vtrInputTObjects[i]));

	VmIObject* pCOutputIObject = (VmIObject*)_fncontainer->GetVmObject(VmObjKey(ObjectTypeIMAGEPLANE, false), 0);
	VmIObject* pCMergeIObject = (VmIObject*)_fncontainer->GetVmObject(VmObjKey(ObjectTypeIMAGEPLANE, false), 1);
	if (pCMergeIObject == NULL || pCOutputIObject == NULL)
	{
		VMERRORMESSAGE("There should be two IObjects!");
		return false;
	}

	//((std::mutex*)HDx11GetMutexGpuCriticalPath())->lock();

	bool bIsAntiAliasingRS = false;
	int iNumTexureLayers = NUM_TEXRT_LAYERS;
	bool bIsFinalRenderer = true;
	double dVThickness = 0;
	int iMaxNumSelfTransparentRenderPass = 2;
	int iLevelSR = 1;
	int iLevelVR = 1;
	vmdouble4 d4GlobalShadingFactors = vmdouble4(0.4, 0.6, 0.2, 30);	// Emission, Diffusion, Specular, Specular Power

	fncont_getparamvalue<int>(_fncontainer, &iMaxNumSelfTransparentRenderPass, ("_int_MaxNumSelfTransparentRenderPass"));
	fncont_getparamvalue<bool>(_fncontainer, &bIsAntiAliasingRS, ("_bool_IsAntiAliasingRS"));
	fncont_getparamvalue<bool>(_fncontainer, &bIsFinalRenderer, ("_bool_IsFinalRenderer"));
	// 1, 2, ..., NUM_TEXRT_LAYERS : 
	fncont_getparamvalue<int>(_fncontainer, &iNumTexureLayers, ("_int_NumTexureLayers"));
	iNumTexureLayers = min(iNumTexureLayers, NUM_TEXRT_LAYERS);
	iNumTexureLayers = max(iNumTexureLayers, 1);
	fncont_getparamvalue<double>(_fncontainer, &dVThickness, ("_double_VZThickness"));
	fncont_getparamvalue<vmdouble4>(_fncontainer, &d4GlobalShadingFactors, ("_double4_ShadingFactorsForGlobalPrimitives"));
	fncont_getparamvalue<int>(_fncontainer, &iLevelSR, ("_int_LevelSR"));
	fncont_getparamvalue<int>(_fncontainer, &iLevelVR, ("_int_LevelVR"));

	double point_thickness = 3.0;
	fncont_getparamvalue<double>(_fncontainer, &point_thickness, ("_double_PointThickness"));
	double line_thickness = 2.0;
	fncont_getparamvalue<double>(_fncontainer, &line_thickness, ("_double_LineThickness"));
	
	vmdouble3 colorForcedCmmObj = vmdouble3(-1, -1, -1);
	fncont_getparamvalue<vmdouble3>(_fncontainer, &colorForcedCmmObj, ("_double3_CmmGlobalColor"));

	bool bIsGlobalForcedPerRTarget = false;
	fncont_getparamvalue<bool>(_fncontainer, &bIsGlobalForcedPerRTarget, ("_bool_IsForcedPerRTarget"));

	bool is_rgba = false; // false means bgra
	fncont_getparamvalue<bool>(_fncontainer, &is_rgba, ("_bool_IsRGBA"));
	//iLevelVR = 2;

	bool bIsSystemOut = false;
	if (iLevelVR == 2 || bIsFinalRenderer) bIsSystemOut = true;

	int iMergeLevel = min(iLevelSR, 1);

#pragma region // Shader Setting
	// Shader Re-Compile Setting //
	bool bReloadHLSLObjFiles = false;
	fncont_getparamvalue<bool>(_fncontainer, &bReloadHLSLObjFiles, ("_bool_ReloadHLSLObjFiles"));
	if (bReloadHLSLObjFiles)
	{
		string strNames_SR[NUMSHADERS_SR_PS] = {
			 "..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\SR_MAPPING_TEX1_ps_4_0"
			,"..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\SR_PHONG_TEX1COLOR_ps_4_0"
			,"..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\SR_PHONG_GLOBALCOLOR_ps_4_0"
			,"..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\SR_PHONG_GLOBALCOLOR_MARKERSONSURFACE_ps_4_0"
			,"..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\SR_PHONG_GLOBALCOLOR_MAXSHADING_ps_4_0"
			,"..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\SR_PHONG_GLOBALCOLOR_MAXSHADING_MARKERSONSURFACE_ps_4_0"
			,"..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\SR_VOLUME_DEVIATION_ps_4_0"
			,"..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\SR_CMM_LINE_ps_4_0"
			,"..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\SR_CMM_TEXT_ps_4_0"
			,"..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\SR_CMM_MULTI_TEXTS_ps_4_0"
			,"..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\SR_VOLUME_SUPERIMPOSE_ps_4_0"
			,"..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\SR_IMAGESTACK_MAP_ps_4_0"
			,"..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\SR_AIRWAYANALYSIS_ps_4_0"
		};
		for (int i = 0; i < NUMSHADERS_SR_PS; i++)
		{
			string strName = strNames_SR[i];

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
					VMSAFE_RELEASE(*ppdx11PS_SRs[i]);
					*ppdx11PS_SRs[i] = pdx11PShader;
				}
				VMSAFE_DELETEARRAY(pyRead);
			}
		}

		string strNames_GS[NUMSHADERS_GS] = {
			 "..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\GS_ThickPoints_gs_4_0"
			,"..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\GS_ThickLines_gs_4_0"
			,"..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\GS_WireLines_gs_4_0"
		};
		for (int i = 0; i < NUMSHADERS_GS; i++)
		{
			string strName = strNames_GS[i];

			FILE* pFile;
			if (fopen_s(&pFile, strName.c_str(), ("rb")) == 0)
			{
				fseek(pFile, 0, SEEK_END);
				ullong ullFileSize = ftell(pFile);
				fseek(pFile, 0, SEEK_SET);
				byte* pyRead = new byte[ullFileSize];
				fread(pyRead, sizeof(byte), ullFileSize, pFile);
				fclose(pFile);

				ID3D11GeometryShader* pdx11GShader = NULL;
				if (pdx11CommonParams->pdx11Device->CreateGeometryShader(pyRead, ullFileSize, NULL, &pdx11GShader) != S_OK)
				{
					VMERRORMESSAGE("SHADER COMPILE FAILURE!");
				}
				else
				{
					VMSAFE_RELEASE(*ppdx11GS[i]);
					*ppdx11GS[i] = pdx11GShader;
				}
				VMSAFE_DELETEARRAY(pyRead);
			}
		}

		//VMSAFE_RELEASE(*ppdx11GS[0]);
		//grd_helper::Compile_Hlsl("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\SRShader_Common.hlsl", "GS_ThickPoints", "gs_4_0", NULL, (void**)ppdx11GS[0]);

		string strNames_MG_CS[NUMSHADERS_MERGE_CS] = {
			 "..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_MERGE_LAYEROUT_TO_RENDEROUT_cs_5_0"
			,"..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_MERGE_SROUT_AND_LAYEROUT_TO_LAYEROUT_cs_5_0"
			,"..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_MERGE_ADV_SROUT_AND_LAYEROUT_TO_LAYEROUT_cs_5_0"
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

		{
			string strName = "..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\CS_OUTLINE_RENDEROUT_cs_5_0";

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
					VMSAFE_RELEASE(*ppdx11CS_Outline);
					*ppdx11CS_Outline = pdx11CShader;
				}
				VMSAFE_DELETEARRAY(pyRead);
			}
		}
	}

	ID3D11InputLayout* pdx11LayoutInputPos = (ID3D11InputLayout*)pdx11ILs[0];
	ID3D11InputLayout* pdx11LayoutInputPosNor = (ID3D11InputLayout*)pdx11ILs[1];
	ID3D11InputLayout* pdx11LayoutInputPosTex = (ID3D11InputLayout*)pdx11ILs[2];
	ID3D11InputLayout* pdx11LayoutInputPosNorTex = (ID3D11InputLayout*)pdx11ILs[3];
	ID3D11InputLayout* pdx11LayoutInputPosTTTex = (ID3D11InputLayout*)pdx11ILs[4];

	ID3D11VertexShader* pdx11VShader_Point = (ID3D11VertexShader*)*ppdx11VS_CommonUsages[0];
	ID3D11VertexShader* pdx11VShader_PointNormal = (ID3D11VertexShader*)*ppdx11VS_CommonUsages[1];
	ID3D11VertexShader* pdx11VShader_PointTexture = (ID3D11VertexShader*)*ppdx11VS_CommonUsages[2];
	ID3D11VertexShader* pdx11VShader_PointNormalTexture = (ID3D11VertexShader*)*ppdx11VS_CommonUsages[3];
	ID3D11VertexShader* pdx11VShader_PointTTTextures = (ID3D11VertexShader*)*ppdx11VS_CommonUsages[4];
	ID3D11VertexShader* pdx11VShader_PointNormalTexture_Deviation = (ID3D11VertexShader*)*ppdx11VS_CommonUsages[5];
	ID3D11VertexShader* pdx11VShader_Proxy = (ID3D11VertexShader*)*ppdx11VS_CommonUsages[6];

	ID3D11VertexShader* pdx11VShader_BiasZ_Point = (ID3D11VertexShader*)*ppdx11VS_BiasZs[0];
	ID3D11VertexShader* pdx11VShader_BiasZ_PointNormal = (ID3D11VertexShader*)*ppdx11VS_BiasZs[1];
	ID3D11VertexShader* pdx11VShader_BiasZ_PointTexture = (ID3D11VertexShader*)*ppdx11VS_BiasZs[2];
	ID3D11VertexShader* pdx11VShader_BiasZ_PointNormalTexture = (ID3D11VertexShader*)*ppdx11VS_BiasZs[3];
	ID3D11VertexShader* pdx11VShader_BiasZ_PointNormalTexture_Deviation = (ID3D11VertexShader*)*ppdx11VS_BiasZs[4];
#pragma endregion // Shader Setting

#pragma region // IOBJECT CPU
	while (pCOutputIObject->GetFrameBuffer(FrameBufferUsageRENDEROUT, 1) != NULL)
		pCOutputIObject->DeleteFrameBuffer(FrameBufferUsageRENDEROUT, 1);
	if (!pCOutputIObject->ReplaceFrameBuffer(FrameBufferUsageRENDEROUT, 0, data_type::dtype<vmbyte4>(),  ("common render out frame buffer : defined in vismtv_inbuilt_renderergpudx module")))
		pCOutputIObject->InsertFrameBuffer(data_type::dtype<vmbyte4>(), FrameBufferUsageRENDEROUT, ("common render out frame buffer : defined in vismtv_inbuilt_renderergpudx module"));

	while (pCOutputIObject->GetFrameBuffer(FrameBufferUsageDEPTH, 1) != NULL)
		pCOutputIObject->DeleteFrameBuffer(FrameBufferUsageDEPTH, 1);
	if (!pCOutputIObject->ReplaceFrameBuffer(FrameBufferUsageDEPTH, 0, data_type::dtype<float>(), ("1st hit screen depth frame buffer : defined in vismtv_inbuilt_renderergpudx module")))
		pCOutputIObject->InsertFrameBuffer(data_type::dtype<float>(), FrameBufferUsageDEPTH, ("1st hit screen depth frame buffer : defined in vismtv_inbuilt_renderergpudx module"));

	if (iLevelVR == 2 && !bIsFinalRenderer)
	{
		while (pCMergeIObject->GetFrameBuffer(FrameBufferUsageRENDEROUT, 1) != NULL)
			pCMergeIObject->DeleteFrameBuffer(FrameBufferUsageRENDEROUT, 1);
		if (!pCMergeIObject->ReplaceFrameBuffer(FrameBufferUsageRENDEROUT, 0, data_type::dtype<vmbyte4>(),  ("mesh render out frame buffer : defined in vismtv_inbuilt_renderergpudx module")))
			pCMergeIObject->InsertFrameBuffer(data_type::dtype<vmbyte4>(), FrameBufferUsageRENDEROUT, ("mesh render out frame buffer : defined in vismtv_inbuilt_renderergpudx module"));

		while (pCMergeIObject->GetFrameBuffer(FrameBufferUsageDEPTH, 1) != NULL)
			pCMergeIObject->DeleteFrameBuffer(FrameBufferUsageDEPTH, 1);
		if (!pCMergeIObject->ReplaceFrameBuffer(FrameBufferUsageDEPTH, 0, data_type::dtype<float>(), ("mesh 1st hit screen depth frame buffer : defined in vismtv_inbuilt_renderergpudx module")))
			pCMergeIObject->InsertFrameBuffer(data_type::dtype<float>(), FrameBufferUsageDEPTH, ("mesh 1st hit screen depth frame buffer : defined in vismtv_inbuilt_renderergpudx module"));
	}
	else
	{
		while (pCMergeIObject->GetFrameBuffer(FrameBufferUsageRENDEROUT, 0) != NULL)
			pCMergeIObject->DeleteFrameBuffer(FrameBufferUsageRENDEROUT, 0);
		while (pCMergeIObject->GetFrameBuffer(FrameBufferUsageDEPTH, 0) != NULL)
			pCMergeIObject->DeleteFrameBuffer(FrameBufferUsageDEPTH, 0);
	}

	while (pCOutputIObject->GetFrameBuffer(FrameBufferUsageVIRTUAL, 1) != NULL)
		pCOutputIObject->DeleteFrameBuffer(FrameBufferUsageVIRTUAL, 1);

	if (!pCMergeIObject->ReplaceFrameBuffer(FrameBufferUsageVIRTUAL, 0, data_type::dtype<byte>(), ("Merge Buffer for Merge Layer : defined in vismtv_inbuilt_renderergpudx module")))
		pCMergeIObject->InsertFrameBuffer(data_type::dtype<byte>(), FrameBufferUsageVIRTUAL, ("Merge Buffer for Merge Layer : defined in vismtv_inbuilt_renderergpudx module"));
#pragma endregion // IOBJECT CPU

	ID3D11Device* pdx11Device = pdx11CommonParams->pdx11Device;
	ID3D11DeviceContext* pdx11DeviceImmContext = pdx11CommonParams->pdx11DeviceImmContext;

#pragma region // IOBJECT GPU
	vmint2 fb_size_cur, i2SizeScreenOld = vmint2(0, 0);
	pCMergeIObject->GetFrameBufferInfo(&fb_size_cur);
	pCMergeIObject->GetCustomParameter("_int2_PreviousScreenSize", data_type::dtype<vmint2>(), &i2SizeScreenOld);
	if (fb_size_cur.x != i2SizeScreenOld.x || fb_size_cur.y != i2SizeScreenOld.y)
	{
		pCGpuManager->ReleaseGpuResourcesBySrcID(pCOutputIObject->GetObjectID());	// System Out 포함 //
		pCGpuManager->ReleaseGpuResourcesBySrcID(pCMergeIObject->GetObjectID());	// System Out 포함 //
		pCMergeIObject->RegisterCustomParameter(("_int2_PreviousScreenSize"), fb_size_cur);
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

	set_fbs(gres_fbs, pCMergeIObject);

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
	// _bool_IsForcedPerRTarget
	// _bool_IsForcedDepthBias
	// _bool_IsForcedCullOff
	// _int_NumTexureLayers
	map<int, GpuRes> mapGpuRes_Vtx;
	map<int, GpuRes> mapGpuRes_Idx;
	map<int, GpuRes> mapGpuRes_Tex;
	map<int, GpuRes> mapGpuRes_VolumeAndTMap;
	uint num_prim_objs = (uint)vtrInputPrimitives.size();
	float leng_max_diag = 0;
	vector<RenderingObject> vtrValidPrimitivesPriorOpaque;
	vector<RenderingObject> vtrValidPrimitivesPerRTartget;
	for (uint i = 0; i < num_prim_objs; i++)
	{
		VmVObjectPrimitive* prim_obj = (VmVObjectPrimitive*)vtrInputPrimitives[i];
		if (prim_obj->IsDefined() == false)
			continue;

		int prim_obj_id = prim_obj->GetObjectID();

		PrimitiveData* prim_data = prim_obj->GetPrimitiveData();
		vmmat44f matOS2WS = prim_obj->GetMatrixOS2WSf();

		vmfloat3 f3PosAaBbMinWS, f3PosAaBbMaxWS;
		fTransformPoint(&f3PosAaBbMinWS, &(vmfloat3)prim_data->aabb_os.pos_min, &matOS2WS);
		fTransformPoint(&f3PosAaBbMaxWS, &(vmfloat3)prim_data->aabb_os.pos_max, &matOS2WS);
		leng_max_diag = max(leng_max_diag, fLengthVector(&(f3PosAaBbMinWS - f3PosAaBbMaxWS)));

		bool bIsVisibleItem = true;
		prim_obj->GetCustomParameter("_bool_visibility", data_type::dtype<bool>(), &bIsVisibleItem);
		lobj->GetDstObjValue(prim_obj_id, "_bool_visibility", &bIsVisibleItem);
		if (!bIsVisibleItem)
			continue;

		int iVolumeID = 0, iTObjectID = 0;
		lobj->GetDstObjValue(prim_obj_id, ("_int_AssociatedVolumeID"), &iVolumeID);
		lobj->GetDstObjValue(prim_obj_id, ("_int_AssociatedTObjectID"), &iTObjectID);

		VmVObjectVolume* vol_obj = (VmVObjectVolume*)find_asscociated_obj(iVolumeID);
		VmTObject* tobj = (VmTObject*)find_asscociated_obj(iTObjectID);
		RegisterVolumeRes(vol_obj, tobj, lobj, pCGpuManager, pdx11DeviceImmContext, mapAssociatedObjects, mapGpuRes_VolumeAndTMap, pstLocalProgress);

		GpuRes gres_vtx, gres_idx, gres_tex;
		grd_helper_legacy::UpdatePrimitiveModel(gres_vtx, gres_idx, gres_tex, prim_obj);
		if (gres_vtx.alloc_res_ptrs.size() > 0)
			mapGpuRes_Vtx.insert(pair<int, GpuRes>(prim_obj_id, gres_vtx));
		if (gres_idx.alloc_res_ptrs.size() > 0)
			mapGpuRes_Idx.insert(pair<int, GpuRes>(prim_obj_id, gres_idx));
		if (gres_tex.alloc_res_ptrs.size() > 0)
			mapGpuRes_Tex.insert(pair<int, GpuRes>(prim_obj_id, gres_tex));

		//////////////////////////////////////////////
		// Register Valid Objects to Rendering List //
		vmdouble4 d4Color(1.), d4ColorWire(1.);
		prim_obj->GetCustomParameter("_double4_color", data_type::dtype<vmdouble4>(), &d4Color);
		bool use_original_obj_color = false;
		prim_obj->GetCustomParameter("_bool_UseOriginalObjColor", data_type::dtype<bool>(), &use_original_obj_color);
		if (!use_original_obj_color) lobj->GetDstObjValue(prim_obj_id, "_double4_color", &d4Color);
		
		bool bIsWireframe = false;
		if (prim_data->ptype == PrimitiveTypeTRIANGLE)
			prim_obj->GetPrimitiveWireframeVisibilityColor(&bIsWireframe, &d4ColorWire);
		vmfloat4 f4Color(d4Color), f4ColorWire(d4ColorWire);

		bool bIsObjectCMM = false;
		prim_obj->GetCustomParameter("_bool_IsAnnotationObj", data_type::dtype<bool>(), &bIsObjectCMM);

		RenderingObject stRenderingObj;
		stRenderingObj.pCMesh = prim_obj;
		if (bIsWireframe && prim_data->ptype == PrimitiveTypeTRIANGLE)
		{
			stRenderingObj.use_vertex_color = false;
			stRenderingObj.is_wireframe = true;
			stRenderingObj.f4Color = f4ColorWire;
			lobj->GetDstObjValue(prim_obj_id, "_bool_UseVertexWireColor", &stRenderingObj.use_vertex_color);
			//stRenderingObj.f4Color.w = 1.f;
			stRenderingObj.iNumSelfTransparentRenderPass = 1;
			if(stRenderingObj.f4Color.w > 0.99f)
				vtrValidPrimitivesPriorOpaque.push_back(stRenderingObj);	// Copy
			else
				vtrValidPrimitivesPerRTartget.push_back(stRenderingObj);
		}

		stRenderingObj.use_vertex_color = true;
		stRenderingObj.is_wireframe = false;
		stRenderingObj.f4Color = f4Color;
		lobj->GetDstObjValue(prim_obj_id, "_bool_UseVertexColor", &stRenderingObj.use_vertex_color);
		lobj->GetDstObjValue(prim_obj_id, "_bool_ShowOutline", &stRenderingObj.show_outline);
		if (f4Color.w > 0.99f && !bIsObjectCMM)
		{
			stRenderingObj.iNumSelfTransparentRenderPass = 1;
			bool bIsForcedPerRTarget = false;
			prim_obj->GetCustomParameter("_bool_IsForcedPerRTarget", data_type::dtype<bool>(), &bIsForcedPerRTarget);
			//if (bIsForcedPerRTarget || bIsGlobalForcedPerRTarget)
				vtrValidPrimitivesPerRTartget.push_back(stRenderingObj);
			//else
			//	vtrValidPrimitivesPriorOpaque.push_back(stRenderingObj);
		}
		else
		{
			if (f4Color.w > 1.f / 255.f)
			{
				int iObjectNumSelfTransparentRenderPass = iMaxNumSelfTransparentRenderPass;
				lobj->GetDstObjValue(prim_obj_id, ("_int_NumSelfTransparentRenderPass"), &iObjectNumSelfTransparentRenderPass);

				//if (prim_data->num_prims < 1000)
				//	iObjectNumSelfTransparentRenderPass = 1;
				stRenderingObj.iNumSelfTransparentRenderPass = iObjectNumSelfTransparentRenderPass;// min(iObjectNumSelfTransparentRenderPass, 2);
				vtrValidPrimitivesPerRTartget.push_back(stRenderingObj);
			}
		}
	}
#pragma endregion // Presetting of VxObject

	if (dVThickness <= 0)
	{
		if (leng_max_diag == 0)
		{
			dVThickness = 0.001;
		}
		else
		{
			dVThickness = leng_max_diag * 0.003;// min(fLengthDiagonalMax * 0.005, 0.01);
		}
	}
	float fVZThickness = (float)dVThickness;

#pragma region // Camera & Light Setting
	uint uiNumGridX = (uint)ceil(fb_size_cur.x / (float)__BLOCKSIZE);
	uint uiNumGridY = (uint)ceil(fb_size_cur.y / (float)__BLOCKSIZE);

	VmCObject* pCCObject = pCMergeIObject->GetCameraObject();
	vmfloat3 f3PosEyeWS, f3VecViewWS, f3PosLightWS, f3VecLightWS;
	pCCObject->GetCameraExtStatef(&f3PosEyeWS, &f3VecViewWS, NULL);

	CB_VrCameraState cbVrCamState;
	grd_helper_legacy::SetCbVrCamera(&cbVrCamState, pCCObject, fb_size_cur, &_fncontainer->vmparams);
	memcpy(&f3PosLightWS, &cbVrCamState.f3PosGlobalLightWS, sizeof(vmfloat3));
	memcpy(&f3VecLightWS, &cbVrCamState.f3VecGlobalLightWS, sizeof(vmfloat3));

	vmmat44 dmatWS2CS, dmatCS2PS, dmatPS2SS;
	vmmat44 dmatSS2PS, dmatPS2CS, dmatCS2WS;
	pCCObject->GetMatrixWStoSS(&dmatWS2CS, &dmatCS2PS, &dmatPS2SS);
	pCCObject->GetMatrixSStoWS(&dmatSS2PS, &dmatPS2CS, &dmatCS2WS);
	vmmat44f matWS2SS = (dmatWS2CS * dmatCS2PS) * dmatPS2SS;
	vmmat44f matSS2WS = (dmatSS2PS * dmatPS2CS) * dmatCS2WS;
	vmmat44f matWS2CS(dmatWS2CS), matCS2PS(dmatCS2PS);

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
	cbSrCamState.uiScreenSizeX = cbVrCamState.uiScreenSizeX;
	cbSrCamState.uiScreenSizeY = cbVrCamState.uiScreenSizeY;
	cbSrCamState.uiGridOffsetX = cbVrCamState.uiGridOffsetX;

	D3D11_MAPPED_SUBRESOURCE mappedSrCamState;
	pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_CAMSTATE], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSrCamState);
	CB_SrProjectionCameraState* pCBCamStateParam = (CB_SrProjectionCameraState*)mappedSrCamState.pData;
	memcpy(pCBCamStateParam, &cbSrCamState, sizeof(CB_SrProjectionCameraState));
	pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_CAMSTATE], 0);
	pdx11DeviceImmContext->VSSetConstantBuffers(1, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_CAMSTATE]);
	pdx11DeviceImmContext->GSSetConstantBuffers(1, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_CAMSTATE]);
	pdx11DeviceImmContext->PSSetConstantBuffers(1, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_CAMSTATE]);
	pdx11DeviceImmContext->CSSetConstantBuffers(1, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_CAMSTATE]);

	// View Port Setting //
	D3D11_VIEWPORT dx11ViewPort;
	dx11ViewPort.Width = (float)fb_size_cur.x;
	dx11ViewPort.Height = (float)fb_size_cur.y;
	dx11ViewPort.MinDepth = 0;
	dx11ViewPort.MaxDepth = 1.0f;
	dx11ViewPort.TopLeftX = 0;
	dx11ViewPort.TopLeftY = 0;
	pdx11DeviceImmContext->RSSetViewports(1, &dx11ViewPort);

	// View List-Up //
	ID3D11ShaderResourceView* pdx11SRV_RenderOuts[NUM_TEXRT_LAYERS] = { NULL };
	ID3D11ShaderResourceView* pdx11SRV_DepthCSs[NUM_TEXRT_LAYERS] = { NULL };
	ID3D11RenderTargetView* pdx11RTV_RenderOuts[NUM_TEXRT_LAYERS] = { NULL };
	ID3D11RenderTargetView* pdx11RTV_DepthCSs[NUM_TEXRT_LAYERS] = { NULL };
	for (int i = 0; i < NUM_TEXRT_LAYERS; i++)
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


	// TEST //
	//ID3D11Buffer* pdx11BufUnordered = NULL;
	//uint uiNumThreads = uiNumGridX * uiNumGridY * __BLOCKSIZE * __BLOCKSIZE; // TO DO // mod-times is not necessary
	//
	//D3D11_BUFFER_DESC descBuf;
	//ZeroMemory(&descBuf, sizeof(D3D11_BUFFER_DESC));
	//descBuf.ByteWidth = sizeof(uint) * uiNumThreads;
	//descBuf.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	//descBuf.StructureByteStride = sizeof(uint);
	//descBuf.BindFlags = D3D11_BIND_UNORDERED_ACCESS; // | D3D11_BIND_SHADER_RESOURCE;
	//descBuf.Usage = D3D11_USAGE_DEFAULT;
	//descBuf.CPUAccessFlags = NULL; // D3D11_CPU_ACCESS_WRITE // test
	//if (pdx11Device->CreateBuffer(&descBuf, NULL, &pdx11BufUnordered) != S_OK)
	//	::MessageBoxA(NULL, "GG1", NULL, MB_OK);
	//
	//D3D11_UNORDERED_ACCESS_VIEW_DESC descUAV;
	//ZeroMemory(&descUAV, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
	//descUAV.Format = DXGI_FORMAT_UNKNOWN;	// Structured RW Buffer
	//descUAV.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	//descUAV.Buffer.FirstElement = 0;
	//descUAV.Buffer.NumElements = uiNumThreads;
	//descUAV.Buffer.Flags = 0; // D3D11_BUFFER_RW_FLAG_RAW (DXGI_FORMAT_R32_TYPELESS), "D3D11_BUFFER_RW_FLAG_COUNTER"
	//ID3D11View* pdx11UAView = NULL; // ID3D11UnorderedAccessView
	//if (pdx11Device->CreateUnorderedAccessView((ID3D11Resource*)pdx11BufUnordered, &descUAV, (ID3D11UnorderedAccessView**)&pdx11UAView) != S_OK)
	//	::MessageBoxA(NULL, "GG2", NULL, MB_OK);
	// D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL
	// D3D11_KEEP_UNORDERED_ACCESS_VIEWS
	// pdx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(1, )
	// TEST //
	//GpuResDescriptor resRWBufDesc, resRWBufUAVDesc;
	//resRWBufDesc.sdk_type = GpuSdkTypeDX11;
	//resRWBufDesc.dtype = data_type::dtype<uint>();
	//resRWBufDesc.res_type = ResourceTypeDX11RESOURCE;
	//resRWBufDesc.usage_specifier = "BUFFER_RW_TEST";
	//resRWBufDesc.misc = 0;
	//resRWBufDesc.src_obj_id = pCMergeIObject->GetObjectID();
	//resRWBufUAVDesc = resRWBufDesc;
	//resRWBufUAVDesc.res_type = ResourceTypeDX11UAV;
	//
	//GpuResource resRWBufUAV, resRWBufData;
	//if (!pCGpuManager->GetGpuResourceArchive(&resRWBufDesc, &resRWBufData))
	//{
	//	if (!pCGpuManager->GenerateGpuResource(&resRWBufDesc, pCMergeIObject, &resRWBufData, NULL))
	//		return false;
	//}
	//if (!pCGpuManager->GetGpuResourceArchive(&resRWBufUAVDesc, &resRWBufUAV))
	//{
	//	if (!pCGpuManager->GenerateGpuResource(&resRWBufUAVDesc, pCMergeIObject, &resRWBufUAV, NULL))
	//		return false;
	//}
	//bool test_mode = true;

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

	// For Each Primitive //
	int iCountMerging = 0;		// Merging 불린 횟 수
	int iCountRendering = 0;	// Draw 불린 횟 수
	int iCountRTBuffers = 0;	// Render Target Index 가 바뀌는 횟 수

	int iRENDERER_LOOP = 0;
RENDERER_LOOP:

	vector<RenderingObject>* pvtrValidPrimitives;
	if (iRENDERER_LOOP == 0)
		pvtrValidPrimitives = &vtrValidPrimitivesPriorOpaque;
	else
		pvtrValidPrimitives = &vtrValidPrimitivesPerRTartget;

	for (uint iIndexMeshObj = 0; iIndexMeshObj < (int)pvtrValidPrimitives->size(); iIndexMeshObj++)
	{
		RenderingObject stRenderingObj = pvtrValidPrimitives->at(iIndexMeshObj);
		VmVObjectPrimitive* pCPrimitiveObject = stRenderingObj.pCMesh;
		PrimitiveData* pstPrimitiveArchive = (PrimitiveData*)pCPrimitiveObject->GetPrimitiveData();
		if (pstPrimitiveArchive->GetVerticeDefinition("POSITION") == NULL)
			continue;

		int iMeshObjID = pCPrimitiveObject->GetObjectID();

		// Object Unit Conditions
		bool bIsForcedCullOff = false;
		bool bIsAnnotationObject = false;
		bool bIsAirwayAnalysis = false;
		bool bIsDashedLine = false;
		bool bIsInvertColorDashedLine = false;
		bool bIsInvertPlaneDirection = false;
		bool bClipFree = false;
		bool bApplyShadingFactors = true;
		int iVolumeID = 0, iTObjectID = 0;
		double dLineDashedInterval = 1.0;
		double dPointThickness = point_thickness;
		double dLineThickness = line_thickness;
		vmdouble4 d4ShadingFactors = d4GlobalShadingFactors;	// Emission, Diffusion, Specular, Specular Power
		vmmat44 dmatClipBoxWS2BS;
		vmdouble3 d3PosOrthoMaxClipBoxWS;
		vmdouble3 d3PosClipPlaneWS;
		vmdouble3 d3VecClipPlaneWS;
		int iClippingMode = 0; // CLIPBOX / CLIPPLANE / BOTH //
		pCPrimitiveObject->GetCustomParameter("_bool_IsForcedCullOff", data_type::dtype<bool>(), &bIsForcedCullOff);
		pCPrimitiveObject->GetCustomParameter("_bool_IsAnnotationObj", data_type::dtype<bool>(), &bIsAnnotationObject);
		pCPrimitiveObject->GetCustomParameter("_bool_IsDashed", data_type::dtype<bool>(), &bIsDashedLine);
		pCPrimitiveObject->GetCustomParameter("_bool_IsInvertColorDashLine", data_type::dtype<bool>(), &bIsInvertColorDashedLine);
		pCPrimitiveObject->GetCustomParameter("_bool_IsInvertPlaneDirection", data_type::dtype<bool>(), &bIsInvertPlaneDirection);

#define __RENDERER_WITHOUT_VOLUME 0
#define __RENDERER_VOLUME_SAMPLE_AND_MAP 1
#define __RENDERER_DIFFERENCE_DIVIATION_FROM_VOLUME 2
#define __RENDERER_IMAGE_STACK_MAP_PER_MESHSPRITE 3

		int iRendererMode = __RENDERER_WITHOUT_VOLUME;
		lobj->GetDstObjValue(iMeshObjID, "_int_RendererMode", &iRendererMode);
		lobj->GetDstObjValue(iMeshObjID, "_int_AssociatedVolumeID", &iVolumeID);
		lobj->GetDstObjValue(iMeshObjID, "_int_AssociatedTObjectID", &iTObjectID);
		lobj->GetDstObjValue(iMeshObjID, "_double_LineDashInterval", &dLineDashedInterval);
		lobj->GetDstObjValue(iMeshObjID, "_double4_ShadingFactors", &d4ShadingFactors);
		lobj->GetDstObjValue(iMeshObjID, "_int_ClippingMode", &iClippingMode);	// 0 : No, 1 : CLIPPLANE, 2 : CLIPBOX, 3 : BOTH
		lobj->GetDstObjValue(iMeshObjID, "_double3_PosClipPlaneWS", &d3PosClipPlaneWS);
		lobj->GetDstObjValue(iMeshObjID, "_double3_VecClipPlaneWS", &d3VecClipPlaneWS);
		lobj->GetDstObjValue(iMeshObjID, "_double3_PosClipBoxMaxWS", &d3PosOrthoMaxClipBoxWS);
		lobj->GetDstObjValue(iMeshObjID, "_matrix44_MatrixClipWS2BS", &dmatClipBoxWS2BS);
		lobj->GetDstObjValue(iMeshObjID, "_bool_IsAirwayAnalysis", &bIsAirwayAnalysis);
		lobj->GetDstObjValue(iMeshObjID, "_double_PointThickness", &dPointThickness);
		lobj->GetDstObjValue(iMeshObjID, "_double_LineThickness", &dLineThickness);

		pCPrimitiveObject->GetCustomParameter("_bool_ClipFree", data_type::dtype<bool>(), &bClipFree);
		pCPrimitiveObject->GetCustomParameter("_bool_ApplyShadingFactors", data_type::dtype<bool>(), &bApplyShadingFactors);
		
		if (bClipFree) iClippingMode = 0;
		if (!bApplyShadingFactors) d4ShadingFactors = d4GlobalShadingFactors;
		
		//iObjectNumSelfTransparentRenderPass = 4;//

		auto itrGpuVolume = mapGpuRes_VolumeAndTMap.find(iVolumeID);
		auto itrGpuTObject = mapGpuRes_VolumeAndTMap.find(iTObjectID);
		if (itrGpuVolume == mapGpuRes_VolumeAndTMap.end() || itrGpuTObject == mapGpuRes_VolumeAndTMap.end())
		{
			iRendererMode = __RENDERER_WITHOUT_VOLUME;
		}
		if (iRendererMode != __RENDERER_WITHOUT_VOLUME)
		{
#pragma region // iRendererMode != __RENDERER_DEFAULT
			pdx11DeviceImmContext->VSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&itrGpuTObject->second.alloc_res_ptrs[DTYPE_SRV]);
			pdx11DeviceImmContext->PSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&itrGpuTObject->second.alloc_res_ptrs[DTYPE_SRV]);

			if (iRendererMode == __RENDERER_IMAGE_STACK_MAP_PER_MESHSPRITE)
			{
				pdx11DeviceImmContext->VSSetShaderResources(10, 1, (ID3D11ShaderResourceView**)&itrGpuVolume->second.alloc_res_ptrs[DTYPE_SRV]);
				pdx11DeviceImmContext->PSSetShaderResources(10, 1, (ID3D11ShaderResourceView**)&itrGpuVolume->second.alloc_res_ptrs[DTYPE_SRV]);
			}
			else
			{
				pdx11DeviceImmContext->VSSetShaderResources(1, 1, (ID3D11ShaderResourceView**)&itrGpuVolume->second.alloc_res_ptrs[DTYPE_SRV]);
				pdx11DeviceImmContext->PSSetShaderResources(1, 1, (ID3D11ShaderResourceView**)&itrGpuVolume->second.alloc_res_ptrs[DTYPE_SRV]);
			}

			auto itrVolume = mapAssociatedObjects.find(iVolumeID);
			auto itrTObject = mapAssociatedObjects.find(iTObjectID);
			VmVObjectVolume* vobj = (VmVObjectVolume*)itrVolume->second;
			VmTObject* tobj = (VmTObject*)itrTObject->second;

			bool high_samplerate = itrGpuVolume->second.res_dvalues["SAMPLE_OFFSET_X"] > 1.f ||
				itrGpuVolume->second.res_dvalues["SAMPLE_OFFSET_Y"] > 1.f || itrGpuVolume->second.res_dvalues["SAMPLE_OFFSET_Z"] > 1.f;

			CB_VrVolumeObject cbVrVolumeObj;
			vmint3 vol_sampled_size = vmint3(itrGpuVolume->second.res_dvalues["WIDTH"], itrGpuVolume->second.res_dvalues["HEIGHT"], itrGpuVolume->second.res_dvalues["DEPTH"]);
			grd_helper_legacy::SetCbVrObject(&cbVrVolumeObj, high_samplerate, vobj, pCCObject, vol_sampled_size, lobj, iVolumeID, fVZThickness);
			D3D11_MAPPED_SUBRESOURCE mappedResVolObj;
			pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_VOLOBJ], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResVolObj);
			CB_VrVolumeObject* pCBParamVolumeObj = (CB_VrVolumeObject*)mappedResVolObj.pData;
			memcpy(pCBParamVolumeObj, &cbVrVolumeObj, sizeof(CB_VrVolumeObject));
			pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_VOLOBJ], 0);
			pdx11DeviceImmContext->PSSetConstantBuffers(2, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_VOLOBJ]);

			CB_VrOtf1D cbVrOtf1D;
			grd_helper_legacy::SetCbVrOtf1D(&cbVrOtf1D, tobj, 1, &_fncontainer->vmparams);
			D3D11_MAPPED_SUBRESOURCE mappedResOtf1D;
			pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_OTF1D], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResOtf1D);
			CB_VrOtf1D* pCBParamOtf1D = (CB_VrOtf1D*)mappedResOtf1D.pData;
			memcpy(pCBParamOtf1D, &cbVrOtf1D, sizeof(CB_VrOtf1D));
			pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_OTF1D], 0);
			pdx11DeviceImmContext->PSSetConstantBuffers(7, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_OTF1D]);

			if (iRendererMode == __RENDERER_DIFFERENCE_DIVIATION_FROM_VOLUME)
			{
				VolumeBlocks* pstVolBlock0 = vobj->GetVolumeBlock(0);
				VolumeBlocks* pstVolBlock1 = vobj->GetVolumeBlock(1);
				if (pstVolBlock0 == NULL || pstVolBlock1 == NULL)
					vobj->UpdateVolumeMinMaxBlocks();

				int iTObjectForIsoValueID = 0;
				lobj->GetDstObjValue(iMeshObjID, "_int_IsoValueTObjectID", &iTObjectForIsoValueID);
				
				auto itrIsoValueTObject = mapAssociatedObjects.find(iTObjectForIsoValueID);
				VmTObject* pCTObjectIsoValue = (VmTObject*)itrIsoValueTObject->second;
				TMapData* pstTfArchiveIsoValue = pCTObjectIsoValue->GetTMapData();
				int iIsoValue = pstTfArchiveIsoValue->valid_min_idx.x;

				vmdouble2 d2ActiveValueRangeMinMax(pstTfArchiveIsoValue->valid_min_idx.x, pstTfArchiveIsoValue->valid_max_idx.x);
				if (pstVolBlock0->GetTaggedActivatedBlocks(iTObjectForIsoValueID) == NULL)
					vobj->UpdateTagBlocks(iTObjectForIsoValueID, 0, d2ActiveValueRangeMinMax, false);
				if (pstVolBlock1->GetTaggedActivatedBlocks(iTObjectForIsoValueID) == NULL)
					vobj->UpdateTagBlocks(iTObjectForIsoValueID, 1, d2ActiveValueRangeMinMax, false);

				vmdouble2 d2DeviationValueMinMax(0, 1);
				lobj->GetDstObjValue(iMeshObjID, "_double_DeviationMinValue", &d2DeviationValueMinMax.x);
				lobj->GetDstObjValue(iMeshObjID, "_double_DeviationMaxValue", &d2DeviationValueMinMax.y);
				int iMappingValueRange = 512;
				lobj->GetDstObjValue(iMeshObjID, "_int_MappingValueRange", &iMappingValueRange);

				CB_SrDeviation cbSrDeviation;
				cbSrDeviation.fMinMappingValue = (float)d2DeviationValueMinMax.x;
				cbSrDeviation.fMaxMappingValue = (float)d2DeviationValueMinMax.y;
				cbSrDeviation.iValueRange = iMappingValueRange;
				cbSrDeviation.iChannelID = 0;
				cbSrDeviation.iIsoValueForVolume = iIsoValue;

				D3D11_MAPPED_SUBRESOURCE mappedDeviation;
				pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_DEVIATION], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedDeviation);
				CB_SrDeviation* pCBPolygonDeviation = (CB_SrDeviation*)mappedDeviation.pData;
				memcpy(pCBPolygonDeviation, &cbSrDeviation, sizeof(CB_SrDeviation));
				pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_DEVIATION], 0);
				pdx11DeviceImmContext->VSSetConstantBuffers(5, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_DEVIATION]);
				pdx11DeviceImmContext->PSSetConstantBuffers(5, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_DEVIATION]);

				GpuRes gres_volblk;
				const int blk_level = 1;
				VolumeBlocks* vol_blk = vobj->GetVolumeBlock(blk_level);
				grd_helper_legacy::UpdateOtfBlocks(gres_volblk, vobj, tobj, false, false);
				pdx11DeviceImmContext->PSSetShaderResources(2, 1, (ID3D11ShaderResourceView**)&gres_volblk.alloc_res_ptrs[DTYPE_SRV]);

				CB_VrBlockObject cbVrBlock;
				grd_helper_legacy::SetCbVrBlockObj(&cbVrBlock, vobj, blk_level, gres_volblk.options["FORMAT"] == DXGI_FORMAT_R16_UNORM ? 65535.f : 1.f);
				D3D11_MAPPED_SUBRESOURCE mappedResBlock;
				pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_BLOCKOBJ], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResBlock);
				CB_VrBlockObject* pCBParamBlock = (CB_VrBlockObject*)mappedResBlock.pData;
				memcpy(pCBParamBlock, &cbVrBlock, sizeof(CB_VrBlockObject));
				pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_BLOCKOBJ], 0);
				pdx11DeviceImmContext->PSSetConstantBuffers(3, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_BLOCKOBJ]);
			}
#pragma endregion
		}
		else if (bIsAnnotationObject)
		{
			if (pstPrimitiveArchive->texture_res_info.size() > 0)
			{
				// GPU Resource Check!! : Text Resource //
				GpuRes gres_tex = mapGpuRes_Tex[iMeshObjID];
				pdx11DeviceImmContext->PSSetShaderResources(3, 1, (ID3D11ShaderResourceView**)&gres_tex.alloc_res_ptrs[DTYPE_SRV]);
			}
		}
		else if (bIsAirwayAnalysis)
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

				RegisterVolumeRes(0, pCTObject, lobj, pCGpuManager, pdx11DeviceImmContext, mapAssociatedObjects, mapGpuRes_VolumeAndTMap, pstLocalProgress);

#pragma region AIRWAY RESOURCE (STBUFFER) and VIEW
				GpuRes gres_aw;
				gres_aw.vm_src_id = lobj->GetObjectID();
				gres_aw.res_name = "BUFFER_AIRWAY_CONTROLS";

				if (!pCGpuManager->UpdateGpuResource(gres_aw))
				{
					size_t bytes_tmp;
					vmdouble3* pAirwayControlPoints = NULL;
					lobj->LoadBufferPtr("_vlist_DOUBLE3_AirwayPathPointWS", (void**)&pAirwayControlPoints, bytes_tmp);
					double* pAirwayAreaPoints = NULL;
					lobj->LoadBufferPtr("_vlist_DOUBLE_AirwayCrossSectionalAreaWS", (void**)&pAirwayAreaPoints, bytes_tmp);
					vmdouble3* pAirwayTVectors = NULL;
					lobj->LoadBufferPtr("_vlist_DOUBLE3_AirwayPathTangentVectorWS", (void**)&pAirwayTVectors, bytes_tmp);

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
		vmmat44f matOS2WS = pCPrimitiveObject->GetMatrixOS2WSf();
#pragma region // Constant Buffers for Each Mesh Object //
		cbPolygonObj.dxmatOS2WS = *(XMMATRIX*)&matOS2WS;
		/////D3DXMatrixTranspose(&cbPolygonObj.matOS2WS, &cbPolygonObj.matOS2WS);

		//if (stRenderingObj.show_outline) 
		//	cbPolygonObj.outline_mode = 1;

		cbPolygonObj.iFlag = 0x3 & iClippingMode;
		vmfloat3 f3ClipBoxMaxBS;
		vmmat44f matClipBoxWS2BS(dmatClipBoxWS2BS);
		fTransformPoint(&f3ClipBoxMaxBS, &vmfloat3(d3PosOrthoMaxClipBoxWS), &matClipBoxWS2BS);
		cbPolygonObj.f3ClipBoxMaxBS = *(XMFLOAT3*)&f3ClipBoxMaxBS;
		cbPolygonObj.dxmatClipBoxWS2BS = *(XMMATRIX*)&vmmat44f(matClipBoxWS2BS);
		cbPolygonObj.f3PosClipPlaneWS = *(XMFLOAT3*)&vmfloat3(d3PosClipPlaneWS);
		cbPolygonObj.f3VecClipPlaneWS = *(XMFLOAT3*)&vmfloat3(d3VecClipPlaneWS);
		/////D3DXMatrixTranspose(&cbPolygonObj.matClipBoxWS2BS, &cbPolygonObj.matClipBoxWS2BS);

		if (bIsAnnotationObject && pstPrimitiveArchive->texture_res_info.size() > 0)
		{
			if (colorForcedCmmObj.x >= 0 && colorForcedCmmObj.y >= 0 && colorForcedCmmObj.z >= 0)
				stRenderingObj.f4Color = vmfloat4((float)colorForcedCmmObj.x, (float)colorForcedCmmObj.y, (float)colorForcedCmmObj.z, 1.f);

			vmint3 i3TextureWHN;
			pCPrimitiveObject->GetCustomParameter("_int3_TextureWHN", data_type::dtype<vmint3>(),  &i3TextureWHN);
			cbPolygonObj.num_letters = i3TextureWHN.z;
			if (i3TextureWHN.z == 1)
			{
				vmfloat3* pos_vtx = pstPrimitiveArchive->GetVerticeDefinition("POSITION");
				vmfloat3 f3Pos0SS, f3Pos1SS, f3Pos2SS;
				vmmat44f matOS2SS = matOS2WS * matWS2SS;

				vmfloat3 pos_vtx_0_ss, pos_vtx_1_ss, pos_vtx_2_ss;
				fTransformPoint(&pos_vtx_0_ss, &pos_vtx[0], &matOS2SS);
				fTransformPoint(&pos_vtx_1_ss, &pos_vtx[1], &matOS2SS);
				fTransformPoint(&pos_vtx_2_ss, &pos_vtx[2], &matOS2SS);
				if (pos_vtx_1_ss.x - pos_vtx_0_ss.x < 0)
					cbPolygonObj.iFlag |= (0x1 << 9);
				if (pos_vtx_2_ss.y - pos_vtx_0_ss.y < 0)
					cbPolygonObj.iFlag |= (0x1 << 10);

				//vmfloat3* pf3Positions = pstPrimitiveArchive->GetVerticeDefinition("POSITION");
				//vmfloat3 f3Pos0SS, f3Pos1SS, f3Pos2SS;
				//vmmat44f matOS2SS = matOS2WS * matWS2SS;
				//
				//vmfloat3 f3VecWidth = pf3Positions[1] - pf3Positions[0];
				//vmfloat3 f3VecHeight = pf3Positions[2] - pf3Positions[0];
				//fTransformVector(&f3VecWidth, &f3VecWidth, &matOS2SS);
				//fTransformVector(&f3VecHeight, &f3VecHeight, &matOS2SS);
				//fNormalizeVector(&f3VecWidth, &f3VecWidth);
				//fNormalizeVector(&f3VecHeight, &f3VecHeight);
				//
				////vmfloat3 f3VecWidth0 = pf3Positions[1] - pf3Positions[0];
				////vmfloat3 f3VecHeight0 = -pf3Positions[2] + pf3Positions[0];
				////vmmat44 matOS2PS = matOS2WS * matWS2CS * matCS2PS;
				////fTransformVector(&f3VecWidth0, &f3VecWidth0, &matOS2PS);
				////fTransformVector(&f3VecHeight0, &f3VecHeight0, &matOS2PS);
				////fNormalizeVector(&f3VecWidth0, &f3VecWidth0);
				////fNormalizeVector(&f3VecHeight0, &f3VecHeight0);
				//
				//if (fDotVector(&f3VecWidth, &vmfloat3(1.f, 0, 0)) < 0)
				//	cbPolygonObj.iFlag |= (0x1 << 9);
				//
				//vmfloat3 f3VecNormal;
				//fCrossDotVector(&f3VecNormal, &f3VecHeight, &f3VecWidth);
				//fCrossDotVector(&f3VecHeight, &f3VecWidth, &f3VecNormal);
				//
				//if (fDotVector(&f3VecHeight, &vmfloat3(0, 1.f, 0)) <= 0)
				//	cbPolygonObj.iFlag |= (0x1 << 10);
			}
		}

		if (bIsDashedLine)
			cbPolygonObj.iFlag |= (0x1 << 19);
		if (bIsInvertColorDashedLine)
			cbPolygonObj.iFlag |= (0x1 << 20);
		cbPolygonObj.fDashDistance = (float)dLineDashedInterval;
		cbPolygonObj.fShadingMultiplier = 1.f;
		if (bIsInvertPlaneDirection)
			cbPolygonObj.fShadingMultiplier = -1.f;

		vmmat44f matOS2PS = matOS2WS * matWS2CS * matCS2PS;
		cbPolygonObj.dxmatOS2PS = *(XMMATRIX*)&matOS2PS;
		/////D3DXMatrixTranspose(&cbPolygonObj.matOS2PS, &cbPolygonObj.matOS2PS);

		CB_VrInterestingRegion cbVrInterestingRegion;
		grd_helper_legacy::SetCbVrInterestingRegion(&cbVrInterestingRegion, lobj, iVolumeID);
		if (cbVrInterestingRegion.iNumRegions > 0)
		{
			D3D11_MAPPED_SUBRESOURCE mappedResInterstingRegion;
			pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_INTERESTINGREGION], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResInterstingRegion);
			CB_VrInterestingRegion* pCBParamInterestingRegion = (CB_VrInterestingRegion*)mappedResInterstingRegion.pData;
			memcpy(pCBParamInterestingRegion, &cbVrInterestingRegion, sizeof(CB_VrInterestingRegion));
			pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_INTERESTINGREGION], 0);
			pdx11DeviceImmContext->PSSetConstantBuffers(8, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_INTERESTINGREGION]);
		}

		bool bIsForcedDepthBias = false;
		bIsForcedDepthBias = stRenderingObj.is_wireframe;
		pCPrimitiveObject->GetCustomParameter("_bool_IsForcedDepthBias", data_type::dtype<bool>(), &bIsForcedDepthBias);
		if (bIsForcedDepthBias)
			cbPolygonObj.fDepthForwardBias = fVZThickness * 1.5f;
		else
			cbPolygonObj.fDepthForwardBias = 0;

		cbPolygonObj.f4Color = *(XMFLOAT4*)&vmfloat4(stRenderingObj.f4Color);
		vmfloat4 f4ShadingFactor = vmfloat4(d4ShadingFactors);
		cbPolygonObj.f4ShadingFactor = *(XMFLOAT4*)&vmfloat4(f4ShadingFactor);
		if (pstPrimitiveArchive->ptype == PrimitiveTypePOINT)
			cbPolygonObj.pix_thickness = (float)(dPointThickness / 2.);
		else if (pstPrimitiveArchive->ptype == PrimitiveTypeLINE || pstPrimitiveArchive->ptype == PrimitiveTypeTRIANGLE)
			cbPolygonObj.pix_thickness = (float)(dLineThickness / 2.);

		D3D11_MAPPED_SUBRESOURCE mappedPolygonObjRes;
		pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_POLYGONOBJ], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedPolygonObjRes);
		CB_SrPolygonObject* pCBPolygonObjParam = (CB_SrPolygonObject*)mappedPolygonObjRes.pData;
		memcpy(pCBPolygonObjParam, &cbPolygonObj, sizeof(CB_SrPolygonObject));
		pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_POLYGONOBJ], 0);
		pdx11DeviceImmContext->VSSetConstantBuffers(0, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_POLYGONOBJ]);
		pdx11DeviceImmContext->PSSetConstantBuffers(0, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_POLYGONOBJ]);
		if (pstPrimitiveArchive->ptype == PrimitiveTypePOINT || pstPrimitiveArchive->ptype == PrimitiveTypeLINE)
			pdx11DeviceImmContext->GSSetConstantBuffers(0, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_POLYGONOBJ]);

#pragma endregion // Constant Buffers for Each Mesh Object //

#pragma region // Setting Rasterization Stages
		ID3D11InputLayout* pdx11InputLayer_Target = NULL;
		ID3D11VertexShader* pdx11VS_Target = NULL;
		ID3D11GeometryShader* pdx11GS_Target = NULL;
		ID3D11PixelShader* pdx11PS_Target = NULL;
		ID3D11RasterizerState* pdx11RState_TargetObj = NULL;
		uint uiStrideSizeInput = 0;
		uint uiOffset = 0;
		D3D_PRIMITIVE_TOPOLOGY ePRIMITIVE_TOPOLOGY;

		if (pstPrimitiveArchive->GetVerticeDefinition("NORMAL"))
		{
			if (pstPrimitiveArchive->GetVerticeDefinition("TEXCOORD0") && stRenderingObj.use_vertex_color)
			{
				// PNT
				pdx11InputLayer_Target = pdx11LayoutInputPosNorTex;
				if (iRendererMode == __RENDERER_DIFFERENCE_DIVIATION_FROM_VOLUME)
					pdx11VS_Target = pdx11VShader_PointNormalTexture_Deviation;
				else
				{
					if (bIsForcedDepthBias)
						pdx11VS_Target = pdx11VShader_BiasZ_PointNormalTexture;
					else
						pdx11VS_Target = pdx11VShader_PointNormalTexture;
				}

				pdx11PS_Target = *ppdx11PS_SRs[__PS_PHONG_TEX1COLOR];
			}
			else
			{
				// PN
				pdx11InputLayer_Target = pdx11LayoutInputPosNor;
				if (bIsForcedDepthBias)
					pdx11VS_Target = pdx11VShader_BiasZ_PointNormal;
				else
					pdx11VS_Target = pdx11VShader_PointNormal;

				if (iNumTexureLayers > 1)
				{
					if (cbVrInterestingRegion.iNumRegions == 0)
					{
						if (stRenderingObj.f4Color.w < 1.f)
							pdx11PS_Target = *ppdx11PS_SRs[__PS_PHONG_GLOBALCOLOR];//__PS_PHONG_GLOBALCOLOR
						else
							pdx11PS_Target = *ppdx11PS_SRs[__PS_PHONG_GLOBALCOLOR_MAXSHADING];//__PS_PHONG_GLOBALCOLOR
					}
					else
						pdx11PS_Target = *ppdx11PS_SRs[__PS_PHONG_GLOBALCOLOR_MARKERONSURFACE];
				}
				else
				{
					if (cbVrInterestingRegion.iNumRegions == 0)
					{
						pdx11PS_Target = *ppdx11PS_SRs[__PS_PHONG_GLOBALCOLOR_MAXSHADING];
					}
					else
						pdx11PS_Target = *ppdx11PS_SRs[__PS_PHONG_GLOBALCOLOR_MAXSHADING_MARKERONSURFACE];
				}
			}
		}
		else if (pstPrimitiveArchive->GetVerticeDefinition("TEXCOORD0"))
		{
			cbPolygonObj.f4ShadingFactor = XMFLOAT4(1.f, 1.f, 1.f, 1.f);

			if (pstPrimitiveArchive->GetVerticeDefinition("TEXCOORD2"))
			{
				// PTTT
				pdx11InputLayer_Target = pdx11LayoutInputPosTTTex;
				pdx11VS_Target = pdx11VShader_PointTTTextures;
				pdx11PS_Target = *ppdx11PS_SRs[__PS_CMM_MULTI_TEXTS];	// __PS_MAPPING_TEX1
			}
			else
			{
				// PT
				pdx11InputLayer_Target = pdx11LayoutInputPosTex;
				if (bIsForcedDepthBias)
					pdx11VS_Target = pdx11VShader_BiasZ_PointTexture;
				else
					pdx11VS_Target = pdx11VShader_PointTexture;
				pdx11PS_Target = *ppdx11PS_SRs[__PS_MAPPING_TEX1];
			}
		}
		else
		{
			cbPolygonObj.f4ShadingFactor = XMFLOAT4(1.f, 1.f, 1.f, 1.f);
			// P
			pdx11InputLayer_Target = pdx11LayoutInputPos;
			if (bIsForcedDepthBias)
				pdx11VS_Target = pdx11VShader_BiasZ_Point;
			else
				pdx11VS_Target = pdx11VShader_Point;
			pdx11PS_Target = *ppdx11PS_SRs[__PS_MAPPING_TEX1];
		}

		if (bIsAnnotationObject)
		{
			if (pstPrimitiveArchive->texture_res_info.size() > 0)
			{
				if (!pstPrimitiveArchive->GetVerticeDefinition("TEXCOORD2"))
					pdx11PS_Target = *ppdx11PS_SRs[__PS_CMM_TEXT];
			}
			else
			{
				pdx11PS_Target = *ppdx11PS_SRs[__PS_CMM_LINE];
			}
		}
		else if (bIsAirwayAnalysis)
		{
			pdx11PS_Target = *ppdx11PS_SRs[__PS_AIRWAY_ANALYSIS];
		}
		else if (iRendererMode == __RENDERER_VOLUME_SAMPLE_AND_MAP)
		{
			pdx11PS_Target = *ppdx11PS_SRs[__PS_VOLUME_SUPERIMPOSE];
		}
		else if (iRendererMode == __RENDERER_DIFFERENCE_DIVIATION_FROM_VOLUME)
		{
			pdx11PS_Target = *ppdx11PS_SRs[__PS_VOLUME_DEVIATION];
		}
		else if (iRendererMode == __RENDERER_IMAGE_STACK_MAP_PER_MESHSPRITE)
		{
			pdx11PS_Target = *ppdx11PS_SRs[__PS_IMAGESTACK_MAP];
		}

		switch (pstPrimitiveArchive->ptype)
		{
		case PrimitiveTypeLINE:
			if (pstPrimitiveArchive->is_stripe)
				ePRIMITIVE_TOPOLOGY = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
			else
				ePRIMITIVE_TOPOLOGY = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
			if (dLineThickness > 0)
				pdx11GS_Target = *ppdx11GS[1];
			break;
		case PrimitiveTypeTRIANGLE:
			if (pstPrimitiveArchive->is_stripe)
				ePRIMITIVE_TOPOLOGY = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
			else
				ePRIMITIVE_TOPOLOGY = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

			//if (dLineThickness > 0 && stRenderingObj.is_wireframe)
			//	pdx11GS_Target = *ppdx11GS[2];
			
			break;
		case PrimitiveTypePOINT:
			ePRIMITIVE_TOPOLOGY = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
			if(dPointThickness > 0)
				pdx11GS_Target = *ppdx11GS[0];
			break;
		default:
			continue;
		}
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
			if (eCullingMode == rxSR_CULL_NONE || bIsForcedCullOff || bIsAnnotationObject)
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
		pdx11DeviceImmContext->GSSetShader(pdx11GS_Target, NULL, 0);
		pdx11DeviceImmContext->PSSetShader(pdx11PS_Target, NULL, 0);
		pdx11DeviceImmContext->RSSetState(pdx11RState_TargetObj);
		pdx11DeviceImmContext->IASetPrimitiveTopology(ePRIMITIVE_TOPOLOGY);
#pragma endregion // Setting Rasterization Stages

		// Self-Transparent Surfaces //
		int iNumSelfTransparentRenderPass = stRenderingObj.iNumSelfTransparentRenderPass;	// at least 1

#pragma region // SELF RENDERING PASS
		for (int iIndexSelfSurface = 0; iIndexSelfSurface < iNumSelfTransparentRenderPass; iIndexSelfSurface++)
		{
			iCountRendering++;
			// Rendering State Setting //
			if (iRENDERER_LOOP == 0)
			{
				iCountRTBuffers = 1;
				pdx11DeviceImmContext->OMSetDepthStencilState(pdx11CommonParams->pdx11DSStates[__DSState_DEPTHENABLE_LESSEQUAL], 0);
			}
			else
			{
				iCountRTBuffers++;
				if (iIndexSelfSurface % 2 == 0)
					pdx11DeviceImmContext->OMSetDepthStencilState(pdx11CommonParams->pdx11DSStates[__DSState_DEPTHENABLE_LESSEQUAL], 0);
				else
					pdx11DeviceImmContext->OMSetDepthStencilState(pdx11CommonParams->pdx11DSStates[__DSState_DEPTHENABLE_GREATER], 0);
			}
			//if (test_mode)
			//	pdx11DeviceImmContext->OMSetDepthStencilState(pdx11CommonParams->pdx11DSStates[__DSState_DEPTHENABLE_ALWAYS], 0);

			int iModRTLayer = iCountRTBuffers % iNumTexureLayers;
			int iIndexRTT = (iCountRTBuffers + iNumTexureLayers - 1) % iNumTexureLayers;
			
			ID3D11RenderTargetView* pdx11RTVs[2] = { 
				(ID3D11RenderTargetView*)gres_fbs[__GRES_FB_V1_ROUT_RGBA_0 + iIndexRTT].alloc_res_ptrs[DTYPE_RTV], 
				(ID3D11RenderTargetView*)gres_fbs[__GRES_FB_V1_ROUT_DEPTH_0 + iIndexRTT].alloc_res_ptrs[DTYPE_RTV] };

			pdx11DeviceImmContext->OMSetRenderTargets(2, pdx11RTVs, pdx11DSV);
			//pdx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(2, pdx11RTVs, pdx11DSV, 2, 1, (ID3D11UnorderedAccessView**)&resRWBufUAV.vtrPtrs[0], 0);

			// Render Work //
			if (pstPrimitiveArchive->is_stripe || pstPrimitiveArchive->ptype == PrimitiveTypePOINT)
				pdx11DeviceImmContext->Draw(pstPrimitiveArchive->num_vtx, 0);
			else
				pdx11DeviceImmContext->DrawIndexed(pstPrimitiveArchive->num_vidx, 0, 0);

			if (stRenderingObj.show_outline && iIndexSelfSurface == 0)
			{
				pdx11DeviceImmContext->OMSetRenderTargets(2, pdx11RTV_2NULLs, NULL);
				pdx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, (ID3D11UnorderedAccessView**)&gres_fbs[__GRES_FB_V1_ROUT_RGBA_0 + iIndexRTT].alloc_res_ptrs[DTYPE_UAV], 0);
				pdx11DeviceImmContext->CSSetShaderResources(40, 1, (ID3D11ShaderResourceView**)&gres_fbs[__GRES_FB_V1_ROUT_DEPTH_0 + iIndexRTT].alloc_res_ptrs[DTYPE_SRV]);
				pdx11DeviceImmContext->CSSetShader(*ppdx11CS_Outline, NULL, 0);
				pdx11DeviceImmContext->Dispatch(uiNumGridX, uiNumGridY, 1);

				// Set NULL States //
				pdx11DeviceImmContext->CSSetShaderResources(40, 1, pdx11SRV_4NULLs);
				ID3D11UnorderedAccessView* pdx11UAV_NULL = NULL;
				pdx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, &pdx11UAV_NULL, 0);
			}

			if (iRENDERER_LOOP == 0 || iNumTexureLayers == 1)
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

			// D3D11_BIND_DEPTH_STENCIL 는 D3D11_BIND_SHADER_RESOURCE 과 같이 사용 불가!므로 PING PONG 불가!

			// RT 가 바뀔 때마다 Clear!!!
			if (iIndexSelfSurface == iNumSelfTransparentRenderPass - 1)
				pdx11DeviceImmContext->ClearDepthStencilView(pdx11DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
		}	// for(int k = 0; k < iNumSelfTransparentRenderPass; k++)
#pragma endregion // SELF RENDERING PASS
	}

	if (iRENDERER_LOOP > 0)
		goto RENDERER_LOOP_EXIT;

	iRENDERER_LOOP++;
	goto RENDERER_LOOP;

RENDERER_LOOP_EXIT:
#pragma region // Final Rearrangement
	int iModRTLayer = iCountRTBuffers % iNumTexureLayers;
	bool bCall_RTTandLayersToLayers = false;
	if (iModRTLayer != 0)
	{
		if (bIsSystemOut)
			bCall_RTTandLayersToLayers = iCountRTBuffers > 1;
		else
			bCall_RTTandLayersToLayers = true;
	}
	if (bCall_RTTandLayersToLayers)
	{
		RTTandLayersToLayersCS(pdx11DeviceImmContext, uiNumGridX, uiNumGridY,
			pdx11SRV_RenderOuts, pdx11SRV_DepthCSs, iCountMerging,
			pUAV_Merge_PingpongCS, pSRV_Merge_PingpongCS,
			pdx11CommonParams, ppdx11CS_Merges, pdx11SRV_4NULLs, pdx11SRV_2NULLs, pdx11RTV_2NULLs, iMergeLevel);
		iCountMerging++;
	}
#pragma endregion // Final Rearrangement

	pdx11DeviceImmContext->Flush();
	pCMergeIObject->RegisterCustomParameter(("_int_CountMerging"), iCountMerging);

	//printf("# Textures : %d, # Drawing : %d, # RTBuffer Change : %d, # Merging : %d\n", iNumTexureLayers, iCountRendering, iCountRTBuffers, iCountMerging);


	auto DisplayTimes = [&](const string& _test)
	{
		if (fb_size_cur.x > 200 && fb_size_cur.y > 200)
		{
			QueryPerformanceFrequency(&lIntFreq);
			QueryPerformanceCounter(&lIntCntEnd);
			double dRunTime1 = (lIntCntEnd.QuadPart - lIntCntStart.QuadPart) / (double)(lIntFreq.QuadPart);

			cout << "  ** rendering time (" << _test << ")   : " << dRunTime1 * 1000 << " ms" << endl;
			cout << "  ** rendering fps (" << _test << ")    : " << 1. / dRunTime1 << " fps" << endl;
		}
	};

	if (bIsSystemOut)
	{
		VmIObject* pCSrOutIObject = pCOutputIObject;
		if (!bIsFinalRenderer) pCSrOutIObject = pCMergeIObject;

		FrameBuffer* pstFrameBuffer = (FrameBuffer*)pCSrOutIObject->GetFrameBuffer(FrameBufferUsageRENDEROUT, 0);
		FrameBuffer* pstFrameBufferDepth = (FrameBuffer*)pCSrOutIObject->GetFrameBuffer(FrameBufferUsageDEPTH, 0);

		if (iCountRendering == 0)	// this means that there is no valid rendering pass
		{
			vmbyte4* py4Buffer = (vmbyte4*)pstFrameBuffer->fbuffer;
			float* pfBuffer = (float*)pstFrameBufferDepth->fbuffer;
			memset(py4Buffer, 0, fb_size_cur.x * fb_size_cur.y * sizeof(vmbyte4));
			memset(pfBuffer, 0x77, fb_size_cur.x * fb_size_cur.y * sizeof(float));
		}
		else
		{
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

			vmbyte4* py4Buffer = (vmbyte4*)pstFrameBuffer->fbuffer;
			float* pfBuffer = (float*)pstFrameBufferDepth->fbuffer;

			D3D11_MAPPED_SUBRESOURCE mappedResSysRGBA;
			D3D11_MAPPED_SUBRESOURCE mappedResSysDepth;
			pdx11DeviceImmContext->Map((ID3D11Texture2D*)gres_fbs[__GRES_FB_V1_SYS_ROUT].alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, NULL, &mappedResSysRGBA);
			pdx11DeviceImmContext->Map((ID3D11Texture2D*)gres_fbs[__GRES_FB_V1_SYS_DOUT].alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, NULL, &mappedResSysDepth);
			DisplayTimes("GG");

			vmbyte4* py4RenderOuts = (vmbyte4*)mappedResSysRGBA.pData;
			float* pfDeferredContexts = (float*)mappedResSysDepth.pData;
			int iGpuBufferOffset = mappedResSysRGBA.RowPitch / 4;
#ifdef PPL_USE
			int count = fb_size_cur.y;
			parallel_for(int(0), count, [is_rgba, fb_size_cur, py4RenderOuts, pfDeferredContexts, py4Buffer, pfBuffer, iGpuBufferOffset](int i)
#else
#pragma omp parallel for 
			for (int i = 0; i < fb_size_cur.y; i++)
#endif
			{
				for (int j = 0; j < fb_size_cur.x; j++)
				{
					vmbyte4 y4Renderout = py4RenderOuts[j + i * iGpuBufferOffset];
					// __PS_MERGE_LAYERS_TO_RENDEROUT 에서 INT -> FLOAT4 로 되어 배열된 color 요소가 들어 옴. //

					// BGRA
					if (is_rgba)
						py4Buffer[i*fb_size_cur.x + j] = vmbyte4(y4Renderout.x, y4Renderout.y, y4Renderout.z, y4Renderout.w);
					else
						py4Buffer[i*fb_size_cur.x + j] = vmbyte4(y4Renderout.z, y4Renderout.y, y4Renderout.x, y4Renderout.w);

					int iAddr = i*fb_size_cur.x + j;
					if (y4Renderout.w > 0)
						pfBuffer[iAddr] = pfDeferredContexts[j + i * iGpuBufferOffset];
					else
						pfBuffer[iAddr] = FLT_MAX;
				}
#ifdef PPL_USE
			});
#else
		}
#endif
			pdx11DeviceImmContext->Unmap((ID3D11Texture2D*)gres_fbs[__GRES_FB_V1_SYS_ROUT].alloc_res_ptrs[DTYPE_RES], 0);
			pdx11DeviceImmContext->Unmap((ID3D11Texture2D*)gres_fbs[__GRES_FB_V1_SYS_DOUT].alloc_res_ptrs[DTYPE_RES], 0);
#pragma endregion
		}	// if (iCountDrawing == 0)
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
	pCMergeIObject->SetDescriptor("vismtv_inbuilt_renderergpudx module");

	// Time Check
	double dRunTime = (lIntCntEnd.QuadPart - lIntCntStart.QuadPart) / (double)(lIntFreq.QuadPart);
	if (pdRunTime)
		*pdRunTime = dRunTime;

	//((std::mutex*)HDx11GetMutexGpuCriticalPath())->unlock();

	return true;
}