#include "RendererHeader.h"

extern void RegisterVTResource(int iVolumeID, int iTObjectID, CVXLObject* pCLObjectForParamters, CVXGPUManager* pCGpuManager, ID3D11DeviceContext* pdx11DeviceImmContext, bool bInitGen,
	map<int, CVXObject*>& mapAssociatedObjects, map<int, SVXGPUResourceArchive>& mapGPUResSRVs, SVXLocalProgress* psvxLocalProgress);

#define MULTILAYERMODE

bool RenderSrOnPlane(SVXModuleParameters* pstModuleParameter,
	CVXGPUManager* pCGpuManager,
	GpuDX11CommonParameters* pdx11CommonParams,
	ID3D11InputLayout* pdx11ILs[NUMINPUTLAYOUTS],
	ID3D11VertexShader** ppdx11CommonVSs[NUMSHADERS_SR_VS],
	ID3D11VertexShader** ppdx11PlaneVSs[NUMSHADERS_PLANE_SR_VS],
	ID3D11PixelShader** ppdx11PSs[NUMSHADERS_SR_PS],
	ID3D11Buffer* pdx11Bufs[3], // 0: Proxy, 1:SI_VTX, 2:SI_IDX
	SVXLocalProgress* psvxLocalProgress,
	double* pdRunTime)
{
	LARGE_INTEGER lIntFreq;
	LARGE_INTEGER lIntCntStart, lIntCntEnd;

	vector<CVXObject*> vtrInputPrimitives;
	VXHStringGetVXObjectListFromObjectStringMap(&vtrInputPrimitives, &pstModuleParameter->mapVXObjects, vxrObjectTypePRIMITIVE, true);
	CVXLObject* pCLObjectForParamters = (CVXLObject*)pstModuleParameter->GetVXObject(vxrObjectTypeCUSTOMLIST, true, 0);
	CVXIObject* pCOutputIObject = (CVXIObject*)pstModuleParameter->GetVXObject(vxrObjectTypeIMAGEPLANE, false, 0);
	if (vtrInputPrimitives.size() == 0)
	{
		pCOutputIObject->RegisterCustomParameter(L"_bool_SkipSrOnPlaneRenderer", true);
		return true;
	}
	pCOutputIObject->RegisterCustomParameter(L"_bool_SkipSrOnPlaneRenderer", false);

	vector<CVXObject*> vtrInputVolumes;
	VXHStringGetVXObjectListFromObjectStringMap(&vtrInputVolumes, &pstModuleParameter->mapVXObjects, vxrObjectTypeVOLUME, true);
	vector<CVXObject*> vtrInputTObjects;
	VXHStringGetVXObjectListFromObjectStringMap(&vtrInputTObjects, &pstModuleParameter->mapVXObjects, vxrObjectTypeTRANSFERFUNCTION, true);

	map<int, CVXObject*> mapAssociatedObjects;
	for (int i = 0; i < vtrInputVolumes.size(); i++)
		mapAssociatedObjects.insert(pair<int, CVXObject*>(vtrInputVolumes[i]->GetObjectID(), vtrInputVolumes[i]));
	for (int i = 0; i < vtrInputTObjects.size(); i++)
		mapAssociatedObjects.insert(pair<int, CVXObject*>(vtrInputTObjects[i]->GetObjectID(), vtrInputTObjects[i]));

	((std::mutex*)HDx11GetMutexGpuCriticalPath())->lock();

	bool bIsAntiAliasingRS = false;
#ifdef MULTILAYERMODE
	int iNumTexureLayers = NUM_TEXRT_LAYERS;
	double dVThickness = -1.0;
	pstModuleParameter->GetCustomParameter(&iNumTexureLayers, _T("_int_NumTexureLayers"));
	iNumTexureLayers = min(iNumTexureLayers, NUM_TEXRT_LAYERS);
	iNumTexureLayers = max(iNumTexureLayers, 1);
	pstModuleParameter->GetCustomParameter(&dVThickness, _T("_double_VZThickness"));
#endif
	vxdouble4 d4GlobalShadingFactors = vxdouble4(0.4, 0.6, 0.2, 30);	// Emission, Diffusion, Specular, Specular Power
	pstModuleParameter->GetCustomParameter(&bIsAntiAliasingRS, _T("_bool_IsAntiAliasingRS"));
	pstModuleParameter->GetCustomParameter(&d4GlobalShadingFactors, _T("_double4_ShadingFactorsForGlobalPrimitives"));

#pragma region // Shader Setting
	// Shader Re-Compile Setting //
	bool bReloadHLSLObjFiles = false;
	pstModuleParameter->GetCustomParameter(&bReloadHLSLObjFiles, _T("_bool_ReloadHLSLObjFiles"));
	if (bReloadHLSLObjFiles)
	{
		wstring strNames_PS[NUMSHADERS_SR_PS] = {
			_T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\SR_MAPPING_TEX1_ps_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\SR_PHONG_TEX1COLOR_ps_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\SR_PHONG_GLOBALCOLOR_ps_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\SR_PHONG_GLOBALCOLOR_MARKERSONSURFACE_ps_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\SR_PHONG_GLOBALCOLOR_MAXSHADING_ps_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\SR_PHONG_GLOBALCOLOR_MAXSHADING_MARKERSONSURFACE_ps_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\SR_VOLUME_DEVIATION_ps_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\SR_CMM_LINE_ps_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\SR_CMM_TEXT_ps_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\SR_CMM_MULTI_TEXTS_ps_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\SR_VOLUME_SUPERIMPOSE_ps_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\SR_IMAGESTACK_MAP_ps_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\SR_AIRWAYANALYSIS_ps_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\PS_MERGE_LAYEROUT_TO_RENDEROUT_ps_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\PS_MERGE_SROUT_AND_LAYEROUT_TO_LAYEROUT_ps_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\PS_MERGE_SROUT_LOD2X_AND_LAYEROUT_TO_LAYEROUT_ps_4_0")
		};
		for (int i = 0; i < NUMSHADERS_SR_PS; i++)
		{
			wstring strName = strNames_PS[i];

			FILE* pFile;
			if (_wfopen_s(&pFile, strName.c_str(), _T("rb")) == 0)
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
					VXERRORMESSAGE("SHADER COMPILE FAILURE!")
				}
				else
				{
					VXSAFE_RELEASE(*ppdx11PSs[i]);
					*ppdx11PSs[i] = pdx11PShader;
				}
				VXSAFE_DELETEARRAY(pyRead);
			}
		}
		/*
		wstring strNames_VS[NUMSHADERS_SRPLANE_VS] = {
			_T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\SRPLANE_PHONG_GLOBALCOLOR_vs_4_0"),
			_T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\SRPLANE_VOXELIZATION_GLOBALCOLOR_ps_4_0")
		};
		for (int i = 0; i < NUMSHADERS_SRPLANE_VS; i++)
		{
			wstring strName = strNames_VS[i];

			FILE* pFile;
			if (_wfopen_s(&pFile, strName.c_str(), _T("rb")) == 0)
			{
				fseek(pFile, 0, SEEK_END);
				ullong ullFileSize = ftell(pFile);
				fseek(pFile, 0, SEEK_SET);
				byte* pyRead = new byte[ullFileSize];
				fread(pyRead, sizeof(byte), ullFileSize, pFile);
				fclose(pFile);

				ID3D11VertexShader* pdx11VShader = NULL;
				if (pdx11CommonParams->pdx11Device->CreateVertexShader(pyRead, ullFileSize, NULL, &pdx11VShader) != S_OK)
				{
					VXERRORMESSAGE("SHADER COMPILE FAILURE!")
				}
				else
				{
					VXSAFE_RELEASE(*ppdx11VSs[i]);
					*ppdx11VSs[i] = pdx11VShader;
				}
				VXSAFE_DELETEARRAY(pyRead);
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

	ID3D11Device* pdx11Device = pdx11CommonParams->pdx11Device;
	ID3D11DeviceContext* pdx11DeviceImmContext = pdx11CommonParams->pdx11DeviceImmContext;
#ifdef MULTILAYERMODE 
#define __FB_PLANE_COUNT __FB_COUNT
#else
#define __FB_PLANE_COUNT __FB_PLANE_COUNT___
#endif
	SVXGPUResourceDESC gpuResourcesFB_DESCs[__FB_PLANE_COUNT];
	SVXGPUResourceArchive gpuResourcesFB_ARCHs[__FB_PLANE_COUNT];

#pragma region // IOBJECT CPU
	// IObject Adaptation //
	while (pCOutputIObject->GetFrameBuffer(vxrFrameBufferUsageRENDEROUT, 3) != NULL)
		pCOutputIObject->DeleteFrameBuffer(vxrFrameBufferUsageRENDEROUT, 3);
	if (pCOutputIObject->GetFrameBuffer(vxrFrameBufferUsageRENDEROUT, 0) == NULL)
		pCOutputIObject->InsertFrameBuffer(vxrDataTypeBYTE4CHANNEL, vxrFrameBufferUsageRENDEROUT, _T("mesh plane 3D rendering pipeline frame buffer OUT : defined in VS-GPUVxRenderer module"));
	if (pCOutputIObject->GetFrameBuffer(vxrFrameBufferUsageRENDEROUT, 1) == NULL)
		pCOutputIObject->InsertFrameBuffer(vxrDataTypeBYTE4CHANNEL, vxrFrameBufferUsageRENDEROUT, _T("mesh plane 3D rendering pipeline frame buffer Backup 0 : defined in VS-GPUVxRenderer module"));
	if (pCOutputIObject->GetFrameBuffer(vxrFrameBufferUsageRENDEROUT, 2) == NULL)
		pCOutputIObject->InsertFrameBuffer(vxrDataTypeBYTE4CHANNEL, vxrFrameBufferUsageRENDEROUT, _T("mesh plane 3D rendering pipeline frame buffer Backup 1 : defined in VS-GPUVxRenderer module"));

	while (pCOutputIObject->GetFrameBuffer(vxrFrameBufferUsageDEPTH, 1) != NULL)
		pCOutputIObject->DeleteFrameBuffer(vxrFrameBufferUsageDEPTH, 1);
	if (!pCOutputIObject->ReplaceFrameBuffer(vxrDataTypeFLOAT, vxrFrameBufferUsageDEPTH, 0, _T("1st hit screen depth frame buffer OUT and Backup 0 : defined in VS-GPUDX11VxRenderer module")))
		pCOutputIObject->InsertFrameBuffer(vxrDataTypeFLOAT, vxrFrameBufferUsageDEPTH, _T("1st hit screen depth frame buffer OUT and Backup 0 : defined in VS-GPUDX11VxRenderer module"));
#pragma endregion // IOBJECT CPU

#pragma region // IOBJECT GPU
	// GPU Resource Check!! : Frame Buffer //
	for (int i = 0; i < __FB_PLANE_COUNT; i++)
	{
		gpuResourcesFB_DESCs[i].eGpuSdkType = vxgGpuSdkTypeDX11;
		gpuResourcesFB_DESCs[i].iCustomMisc = 0;
		gpuResourcesFB_DESCs[i].iSourceObjectID = pCOutputIObject->GetObjectID();
	}

	gpuResourcesFB_DESCs[__FB_TX2D_RENDEROUT].eResourceType = vxgResourceTypeDX11RESOURCE;
	gpuResourcesFB_DESCs[__FB_TX2D_RENDEROUT].strUsageSpecifier = _T("TEXTURE2D_IOBJECT_RENDEROUT");
#ifdef MULTILAYERMODE
	gpuResourcesFB_DESCs[__FB_TX2D_RENDEROUT].iCustomMisc = NUM_TEXRT_LAYERS;	// Number of Textures
#else
	gpuResourcesFB_DESCs[__FB_TX2D_RENDEROUT].iCustomMisc = 1;	// Number of Textures
#endif
	gpuResourcesFB_DESCs[__FB_TX2D_RENDEROUT].eDataType = vxrDataTypeBYTE4CHANNEL;
	gpuResourcesFB_DESCs[__FB_TX2D_RENDEROUT].iSizeStride = VXHGetDataTypeSizeByte(gpuResourcesFB_DESCs[__FB_TX2D_RENDEROUT].eDataType);
	gpuResourcesFB_DESCs[__FB_RTV_RENDEROUT] = gpuResourcesFB_DESCs[__FB_SRV_RENDEROUT] = gpuResourcesFB_DESCs[__FB_TX2D_RENDEROUT];
	gpuResourcesFB_DESCs[__FB_RTV_RENDEROUT].eResourceType = vxgResourceTypeDX11RTV;
	gpuResourcesFB_DESCs[__FB_SRV_RENDEROUT].eResourceType = vxgResourceTypeDX11SRV;
	gpuResourcesFB_DESCs[__FB_TX2D_DEPTH].eResourceType = vxgResourceTypeDX11RESOURCE;
	gpuResourcesFB_DESCs[__FB_TX2D_DEPTH].strUsageSpecifier = _T("TEXTURE2D_IOBJECT_DEPTHSTENCIL");
	gpuResourcesFB_DESCs[__FB_TX2D_DEPTH].eDataType = vxrDataTypeFLOAT;
	gpuResourcesFB_DESCs[__FB_TX2D_DEPTH].iSizeStride = VXHGetDataTypeSizeByte(gpuResourcesFB_DESCs[__FB_TX2D_DEPTH].eDataType);
	gpuResourcesFB_DESCs[__FB_DSV_DEPTH] = gpuResourcesFB_DESCs[__FB_TX2D_DEPTH];
	gpuResourcesFB_DESCs[__FB_DSV_DEPTH].eResourceType = vxgResourceTypeDX11DSV;
	gpuResourcesFB_DESCs[__FB_TX2D_DEPTH_ZTHICKCS].eResourceType = vxgResourceTypeDX11RESOURCE;
	gpuResourcesFB_DESCs[__FB_TX2D_DEPTH_ZTHICKCS].strUsageSpecifier = _T("TEXTURE2D_IOBJECT_RENDEROUT");
#ifdef MULTILAYERMODE
	gpuResourcesFB_DESCs[__FB_TX2D_DEPTH_ZTHICKCS].iCustomMisc = NUM_TEXRT_LAYERS;	// Number of Textures
#else
	gpuResourcesFB_DESCs[__FB_TX2D_DEPTH_ZTHICKCS].iCustomMisc = 1;	// Number of Textures
#endif
	gpuResourcesFB_DESCs[__FB_TX2D_DEPTH_ZTHICKCS].eDataType = vxrDataTypeFLOAT;
	gpuResourcesFB_DESCs[__FB_TX2D_DEPTH_ZTHICKCS].iSizeStride = VXHGetDataTypeSizeByte(gpuResourcesFB_DESCs[__FB_TX2D_DEPTH_ZTHICKCS].eDataType);
	gpuResourcesFB_DESCs[__FB_RTV_DEPTH_ZTHICKCS] = gpuResourcesFB_DESCs[__FB_SRV_DEPTH_ZTHICKCS] = gpuResourcesFB_DESCs[__FB_TX2D_DEPTH_ZTHICKCS];
	gpuResourcesFB_DESCs[__FB_RTV_DEPTH_ZTHICKCS].eResourceType = vxgResourceTypeDX11RTV;
	gpuResourcesFB_DESCs[__FB_SRV_DEPTH_ZTHICKCS].eResourceType = vxgResourceTypeDX11SRV;
#ifdef MULTILAYERMODE
	gpuResourcesFB_DESCs[__FB_TX2D_DEPTH_DUAL] = gpuResourcesFB_DESCs[__FB_TX2D_DEPTH];
	gpuResourcesFB_DESCs[__FB_TX2D_DEPTH_DUAL].strUsageSpecifier = _T("TEXTURE2D_IOBJECT_RENDEROUT");
	gpuResourcesFB_DESCs[__FB_TX2D_DEPTH_DUAL].eResourceType = vxgResourceTypeDX11RESOURCE;
	gpuResourcesFB_DESCs[__FB_SRV_DEPTH_DUAL] = gpuResourcesFB_DESCs[__FB_RTV_DEPTH_DUAL] = gpuResourcesFB_DESCs[__FB_TX2D_DEPTH_DUAL];
	gpuResourcesFB_DESCs[__FB_SRV_DEPTH_DUAL].eResourceType = vxgResourceTypeDX11SRV;
	gpuResourcesFB_DESCs[__FB_RTV_DEPTH_DUAL].eResourceType = vxgResourceTypeDX11RTV;

	SVXGPUResourceDESC gpuResourceMERGE_PS_DESCs[__EXFB_COUNT];
	SVXGPUResourceArchive gpuResourceMERGE_PS_ARCHs[__EXFB_COUNT];
	for (int i = 0; i < __EXFB_COUNT; i++)
	{
		gpuResourceMERGE_PS_DESCs[i].eGpuSdkType = vxgGpuSdkTypeDX11;
		gpuResourceMERGE_PS_DESCs[i].strUsageSpecifier = _T("TEXTURE2D_IOBJECT_RENDEROUT");
		gpuResourceMERGE_PS_DESCs[i].iCustomMisc = NUM_MERGE_LAYERS * 2;	// two of NUM_MERGE_LAYERS resource buffers
		gpuResourceMERGE_PS_DESCs[i].iSourceObjectID = pCOutputIObject->GetObjectID();
		gpuResourceMERGE_PS_DESCs[i].eDataType = vxrDataTypeFLOAT4CHANNEL;	// RG : Int RGBA // B : DepthBack // A : Thickness
		gpuResourceMERGE_PS_DESCs[i].iSizeStride = VXHGetDataTypeSizeByte(gpuResourceMERGE_PS_DESCs[i].eDataType);
		switch (i)
		{
		case 0:
			gpuResourceMERGE_PS_DESCs[i].eResourceType = vxgResourceTypeDX11RESOURCE; break;
		case 1:
			gpuResourceMERGE_PS_DESCs[i].eResourceType = vxgResourceTypeDX11SRV; break;
		case 2:
			gpuResourceMERGE_PS_DESCs[i].eResourceType = vxgResourceTypeDX11RTV; break;
		}
	}
#endif

	bool bInitGen = false;
	for (int i = 0; i < __FB_PLANE_COUNT; i++)
	{
		if (!pCGpuManager->GetGpuResourceArchive(&gpuResourcesFB_DESCs[i], &gpuResourcesFB_ARCHs[i]))
		{
			pCGpuManager->GenerateGpuResource(&gpuResourcesFB_DESCs[i], pCOutputIObject, &gpuResourcesFB_ARCHs[i], NULL);
			bInitGen = true;
		}
	}
#ifdef MULTILAYERMODE
	for (int i = 0; i < __EXFB_COUNT; i++)
	{
		if (!pCGpuManager->GetGpuResourceArchive(&gpuResourceMERGE_PS_DESCs[i], &gpuResourceMERGE_PS_ARCHs[i]))
		{
			pCGpuManager->GenerateGpuResource(&gpuResourceMERGE_PS_DESCs[i], pCOutputIObject, &gpuResourceMERGE_PS_ARCHs[i], NULL);
			bInitGen = true;
		}
	}
#endif

	vxint2 i2SizeScreenCurrent, i2SizeScreenOld = vxint2(0, 0);
	pCOutputIObject->GetFrameBufferInfo(&i2SizeScreenCurrent);
	if (bInitGen)
		pCOutputIObject->RegisterCustomParameter(_T("_int2_PreviousScreenSize"), i2SizeScreenCurrent);
	pCOutputIObject->GetCustomParameter(_T("_int2_PreviousScreenSize"), &i2SizeScreenOld);
	bool bIsResized = false;
	if (i2SizeScreenCurrent.x != i2SizeScreenOld.x || i2SizeScreenCurrent.y != i2SizeScreenOld.y)
	{
		bIsResized = true;
		pCGpuManager->ReleaseGpuResourcesRelatedObject(pCOutputIObject->GetObjectID());	// System Out 포함 //

		for (int i = 0; i < __FB_PLANE_COUNT; i++)
			pCGpuManager->GenerateGpuResource(&gpuResourcesFB_DESCs[i], pCOutputIObject, &gpuResourcesFB_ARCHs[i], NULL);

#ifdef MULTILAYERMODE
		for (int i = 0; i < __EXFB_COUNT; i++)
			pCGpuManager->GenerateGpuResource(&gpuResourceMERGE_PS_DESCs[i], pCOutputIObject, &gpuResourceMERGE_PS_ARCHs[i], NULL);
#endif
	}
	pCOutputIObject->RegisterCustomParameter(_T("_int2_PreviousScreenSize"), i2SizeScreenCurrent);
#pragma endregion // IOBJECT GPU

#ifdef MULTILAYERMODE
	vector<ID3D11RenderTargetView*> vtrRTV_MergePingpongPS;
	vector<ID3D11ShaderResourceView*> vtrSRV_MergePingpongPS;
	vector<void*>* pvtrRTV = &gpuResourceMERGE_PS_ARCHs[__EXFB_RTV_MERGE_RAYSEGMENT].vtrPtrs;
	vector<void*>* pvtrSRV = &gpuResourceMERGE_PS_ARCHs[__EXFB_SRV_MERGE_RAYSEGMENT].vtrPtrs;
	for (int i = 0; i < NUM_MERGE_LAYERS * 2; i++)
	{
		vtrRTV_MergePingpongPS.push_back((ID3D11RenderTargetView*)pvtrRTV->at(i));
		vtrSRV_MergePingpongPS.push_back((ID3D11ShaderResourceView*)pvtrSRV->at(i));
	}

	float fLengthDiagonalMax = 0;
#endif

#pragma region // Presetting of VxObject
	map<wstring, vector<double>> mapDValueClear;
	map<int, SVXGPUResourceArchive> mapGPUResSRVs;
	map<int, SVXGPUResourceArchive> mapVolumeBlockResSRVs;
	vector<SVXGPUResourceArchive> vtrGpuResourceArchiveBufferVertex;
	vector<SVXGPUResourceArchive> vtrGpuResourceArchiveBufferIndex;
	vector<CVXVObjectPrimitive*> vtrValidPrimitives_FilledPlane;
	vector<CVXVObjectPrimitive*> vtrValidPrimitives_3DonPlane;
	uint uiNumPrimitiveObjects = (uint)vtrInputPrimitives.size();
	for (uint i = 0; i < uiNumPrimitiveObjects; i++)
	{
		CVXVObjectPrimitive* pCVMeshObj = (CVXVObjectPrimitive*)vtrInputPrimitives[i];
		if (pCVMeshObj->IsDefinedResource() == false)
			continue;

		SVXPrimitiveDataArchive* pstMeshArchive = pCVMeshObj->GetPrimitiveArchive();
#ifdef MULTILAYERMODE
		vxmatrix matOS2WS = *pCVMeshObj->GetMatrixOS2WS();
#endif

#ifdef MULTILAYERMODE
		vxfloat3 f3PosAaBbMinWS, f3PosAaBbMaxWS;
		VXHMTransformPoint(&f3PosAaBbMinWS, &pstMeshArchive->stAaBbMinMaxOS.f3PosMinBox, &matOS2WS);
		VXHMTransformPoint(&f3PosAaBbMaxWS, &pstMeshArchive->stAaBbMinMaxOS.f3PosMaxBox, &matOS2WS);
		fLengthDiagonalMax = max(fLengthDiagonalMax, VXHMLengthVector(&(f3PosAaBbMinWS - f3PosAaBbMaxWS)));
#endif
		bool bIsVisibleItem = pCVMeshObj->GetVisibility();
		if (!bIsVisibleItem)
			continue;

		int iVolumeID = 0, iTObjectID = 0;
		int iMeshObjID = pCVMeshObj->GetObjectID();
		map<wstring, vector<double>>* pmapDValueMesh = NULL;
		pCLObjectForParamters->GetCustomDValue(iMeshObjID, (void**)&pmapDValueMesh);
		if (pmapDValueMesh == NULL)
			pmapDValueMesh = &mapDValueClear;
		VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_int_AssociatedVolumeID"), &iVolumeID);
		VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_int_AssociatedTObjectID"), &iTObjectID);

		RegisterVTResource(iVolumeID, iTObjectID, pCLObjectForParamters, pCGpuManager, pdx11DeviceImmContext, bInitGen,
			mapAssociatedObjects, mapGPUResSRVs, psvxLocalProgress);

		SVXGPUResourceDESC svxGpuResourcePrimitiveVertexDESC, svxGpuResourcePrimitiveIndexDESC;
		svxGpuResourcePrimitiveVertexDESC.eGpuSdkType = vxgGpuSdkTypeDX11;
		svxGpuResourcePrimitiveVertexDESC.eDataType = vxrDataTypeFLOAT3CHANNEL;
		svxGpuResourcePrimitiveVertexDESC.eResourceType = vxgResourceTypeDX11RESOURCE;
		svxGpuResourcePrimitiveVertexDESC.strUsageSpecifier = _T("BUFFER_PRIMITIVE_VERTEX_LIST");
		svxGpuResourcePrimitiveVertexDESC.iCustomMisc = 0;
		svxGpuResourcePrimitiveVertexDESC.iSourceObjectID = pCVMeshObj->GetObjectID();
		svxGpuResourcePrimitiveVertexDESC.iSizeStride = VXHGetDataTypeSizeByte(svxGpuResourcePrimitiveVertexDESC.eDataType);
		svxGpuResourcePrimitiveIndexDESC = svxGpuResourcePrimitiveVertexDESC;
		svxGpuResourcePrimitiveIndexDESC.eDataType = vxrDataTypeUNSIGNEDINT;
		svxGpuResourcePrimitiveIndexDESC.iSizeStride = VXHGetDataTypeSizeByte(svxGpuResourcePrimitiveIndexDESC.eDataType);
		svxGpuResourcePrimitiveIndexDESC.strUsageSpecifier = _T("BUFFER_PRIMITIVE_INDEX_LIST");

		SVXGPUResourceArchive svxGpuResourceBufferVertex, svxGpuResourceBufferIndex;
		if (!pCGpuManager->GetGpuResourceArchive(&svxGpuResourcePrimitiveVertexDESC, &svxGpuResourceBufferVertex))
		{
			if (!pCGpuManager->GenerateGpuResource(&svxGpuResourcePrimitiveVertexDESC, pCVMeshObj, &svxGpuResourceBufferVertex, NULL))
				return false;
		}
		vtrGpuResourceArchiveBufferVertex.push_back(svxGpuResourceBufferVertex);

		if (pstMeshArchive->puiIndexList != NULL)
		{
			if (svxGpuResourceBufferVertex.vtrPtrs.at(0) == NULL)
				return false;

			if (!pCGpuManager->GetGpuResourceArchive(&svxGpuResourcePrimitiveIndexDESC, &svxGpuResourceBufferIndex))
			{
				if (!pCGpuManager->GenerateGpuResource(&svxGpuResourcePrimitiveIndexDESC, pCVMeshObj, &svxGpuResourceBufferIndex, NULL))
					return false;
			}
		}
		vtrGpuResourceArchiveBufferIndex.push_back(svxGpuResourceBufferIndex);

		bool bIsFilledPlane = false;
		VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_bool_IsFilledPlane"), &bIsFilledPlane);

		if (bIsFilledPlane)
			vtrValidPrimitives_FilledPlane.push_back(pCVMeshObj);
		else 
			vtrValidPrimitives_3DonPlane.push_back(pCVMeshObj);
	}
#pragma endregion // Presetting of VxObject

#ifdef MULTILAYERMODE
	if (dVThickness < 0)
	{
		if (fLengthDiagonalMax == 0)
		{
			dVThickness = 1;
		}
		else
		{
			dVThickness = fLengthDiagonalMax * 0.01;
		}
	}
#endif

#pragma region // Camera & Light Setting
	// CONSIDER THAT THIS RENDERER HANDLES ONLY <ORTHOGONAL> PROJECTION!!
	CVXCObject* pCCObject = pCOutputIObject->GetCameraObject();
	vxfloat3 f3PosEyeWS, f3VecViewWS, f3PosLightWS, f3VecLightWS;
	pCCObject->GetCameraState(&f3PosEyeWS, &f3VecViewWS, NULL);

	CB_VrCameraState cbVrCamState;
	HDx11ComputeConstantBufferVrCamera(&cbVrCamState, __BLOCKSIZE, pCCObject, i2SizeScreenCurrent, &pstModuleParameter->mapCustomParamters);
	memcpy(&f3PosLightWS, &cbVrCamState.f3PosGlobalLightWS, sizeof(vxfloat3));
	memcpy(&f3VecLightWS, &cbVrCamState.f3VecGlobalLightWS, sizeof(vxfloat3));

	vxmatrix matWS2CS, matCS2PS, matPS2SS;
	vxmatrix matSS2PS, matPS2CS, matCS2WS;
	pCCObject->GetMatricesWStoSS(&matWS2CS, &matCS2PS, &matPS2SS);
	pCCObject->GetMatricesSStoWS(&matSS2PS, &matPS2CS, &matCS2WS);
	vxmatrix matWS2SS = (matWS2CS * matCS2PS) * matPS2SS;
	vxmatrix matSS2WS = (matSS2PS * matPS2CS) * matCS2WS;

	CB_SrProjectionCameraState cbSrCamState;
	cbSrCamState.matSS2WS = *(D3DXMATRIX*)&matSS2WS;
	D3DXMatrixTranspose(&cbSrCamState.matSS2WS, &cbSrCamState.matSS2WS);
	cbSrCamState.matWS2PS = *(D3DXMATRIX*)&(matWS2CS * matCS2PS);
	D3DXMatrixTranspose(&cbSrCamState.matWS2PS, &cbSrCamState.matWS2PS);

	cbSrCamState.f3PosEyeWS = *(D3DXVECTOR3*)&f3PosEyeWS;
	cbSrCamState.f3VecViewWS = *(D3DXVECTOR3*)&f3VecViewWS;
	cbSrCamState.f3PosLightWS = *(D3DXVECTOR3*)&f3PosLightWS;
	cbSrCamState.f3VecLightWS = *(D3DXVECTOR3*)&f3VecLightWS;
	cbSrCamState.iFlag = cbVrCamState.iFlags;
#ifdef MULTILAYERMODE
	cbSrCamState.fVThicknessGlobal = (float)dVThickness;
#else
	cbSrCamState.fVThicknessGlobal = 0;
#endif

	D3D11_MAPPED_SUBRESOURCE mappedSrCamState;
	pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_CAMSTATE], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSrCamState);
	CB_SrProjectionCameraState* pCBCamStateParam = (CB_SrProjectionCameraState*)mappedSrCamState.pData;
	memcpy(pCBCamStateParam, &cbSrCamState, sizeof(CB_SrProjectionCameraState));
	pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_CAMSTATE], 0);
	pdx11DeviceImmContext->VSSetConstantBuffers(1, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_CAMSTATE]);
	pdx11DeviceImmContext->PSSetConstantBuffers(1, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_CAMSTATE]);

	// View Port Setting //
	D3D11_VIEWPORT dx11ViewPort;
	dx11ViewPort.Width = (float)i2SizeScreenCurrent.x;
	dx11ViewPort.Height = (float)i2SizeScreenCurrent.y;
	dx11ViewPort.MinDepth = 0;	// Dojo Check //
	dx11ViewPort.MaxDepth = 1.0f;
	dx11ViewPort.TopLeftX = 0;
	dx11ViewPort.TopLeftY = 0;
	pdx11DeviceImmContext->RSSetViewports(1, &dx11ViewPort);

#ifdef MULTILAYERMODE
	// View List-Up //
	ID3D11ShaderResourceView* pdx11SRV_RenderOuts_NULL[NUM_TEXRT_LAYERS] = { NULL };
	ID3D11ShaderResourceView* pdx11SRV_DepthCSs_NULL[NUM_TEXRT_LAYERS] = { NULL };
	ID3D11ShaderResourceView* pdx11SRV_RenderOuts[NUM_TEXRT_LAYERS] = { NULL };
	ID3D11ShaderResourceView* pdx11SRV_DepthCSs[NUM_TEXRT_LAYERS] = { NULL };
	for (int i = 0; i < NUM_TEXRT_LAYERS; i++)
	{
		pdx11SRV_RenderOuts[i] = (ID3D11ShaderResourceView*)gpuResourcesFB_ARCHs[__FB_SRV_RENDEROUT].vtrPtrs.at(i);
		pdx11SRV_DepthCSs[i] = (ID3D11ShaderResourceView*)gpuResourcesFB_ARCHs[__FB_SRV_DEPTH_ZTHICKCS].vtrPtrs.at(i);
	}

	ID3D11ShaderResourceView* pdx11SRV_RSA_NULL[NUM_MERGE_LAYERS] = { NULL };
	ID3D11RenderTargetView* pdx11RTV_RSA_NULL[NUM_MERGE_LAYERS] = { NULL };
	ID3D11ShaderResourceView* pdx11SRV_RSA[NUM_MERGE_LAYERS] = { NULL };
	ID3D11RenderTargetView* pdx11RTV_RSA[NUM_MERGE_LAYERS] = { NULL };

	ID3D11ShaderResourceView* pdx11SRV_NULL = NULL;
	ID3D11UnorderedAccessView* pdx11UAV_NULL = NULL;
#endif
#pragma endregion // Camera & Light Setting

#define __PLANE_RENDER_MODE_FILLED 0
#define __PLANE_RENDER_MODE_3D 1
	int iLoopMode = __PLANE_RENDER_MODE_FILLED; 

	vector<CVXVObjectPrimitive*>* pvtrValidPrimitives = NULL;

	int iCountDrawings[2] = { 0 };	// Draw 불린 횟 수
RENDER_LOOP:

	if (iLoopMode == __PLANE_RENDER_MODE_FILLED)
		pvtrValidPrimitives = &vtrValidPrimitives_FilledPlane;
	else 
		pvtrValidPrimitives = &vtrValidPrimitives_3DonPlane;

#pragma region // FrameBuffer Setting
	uint uiClearVlaues[4] = { 0 };
	float fClearColor[4];
	fClearColor[0] = 0; fClearColor[1] = 0; fClearColor[2] = 0; fClearColor[3] = 0;

#ifdef MULTILAYERMODE

	int iCountRTBuffers = 0;
	int iCountMerging = 0;

	// Clear Merge Buffer //
	for (int i = 0; i < NUM_MERGE_LAYERS * 2; i++)
		pdx11DeviceImmContext->ClearRenderTargetView(vtrRTV_MergePingpongPS.at(i), fClearColor);
#endif

	// Backup Previous Render Target //
	ID3D11RenderTargetView* pdxRTVOld = NULL;
	ID3D11DepthStencilView* pdxDSVOld = NULL;
	pdx11DeviceImmContext->OMGetRenderTargets(1, &pdxRTVOld, &pdxDSVOld);

	ID3D11RenderTargetView* pdx11RTVs[2] = { (ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_RENDEROUT].vtrPtrs.at(0)
		, (ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_DEPTH_ZTHICKCS].vtrPtrs.at(0) };
	pdx11DeviceImmContext->OMSetRenderTargets(2, pdx11RTVs, (ID3D11DepthStencilView*)gpuResourcesFB_ARCHs[__FB_DSV_DEPTH].vtrPtrs.at(0));

	// Clear //
#ifdef MULTILAYERMODE
	ID3D11RenderTargetView* pdx11RTVs_NULL[2] = { NULL };
	for(int i = 0; i < NUM_TEXRT_LAYERS; i++)
	{
		pdx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_RENDEROUT].vtrPtrs.at(i), fClearColor);
		pdx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_DEPTH_ZTHICKCS].vtrPtrs.at(i), fClearColor);
	}
	// Dual Depth //
	pdx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_DEPTH_DUAL].vtrPtrs.at(0), fClearColor);
	pdx11DeviceImmContext->PSSetShaderResources(4, 1, (ID3D11ShaderResourceView**)&gpuResourcesFB_ARCHs[__FB_SRV_DEPTH_DUAL].vtrPtrs.at(0));
#else
	pdx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_RENDEROUT].vtrPtrs.at(0), fClearColor);
	pdx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_DEPTH_ZTHICKCS].vtrPtrs.at(0), fClearColor);
#endif

	pdx11DeviceImmContext->ClearDepthStencilView((ID3D11DepthStencilView*)gpuResourcesFB_ARCHs[__FB_DSV_DEPTH].vtrPtrs.at(0), D3D11_CLEAR_DEPTH, 1.0f, 0);
	
	pdx11DeviceImmContext->OMSetDepthStencilState(pdx11CommonParams->pdx11DSStates[__DSState_DEPTHENABLE_LESSEQUAL], 0);
#pragma endregion // FrameBuffer Setting

#pragma region // Other Presetting For Shaders

	// N/A
	pdx11DeviceImmContext->VSSetSamplers(0, 1, &pdx11CommonParams->pdx11SStates[__SState_LINEAR_ZEROBORDER]);
	pdx11DeviceImmContext->VSSetSamplers(1, 1, &pdx11CommonParams->pdx11SStates[__SState_POINT_ZEROBORDER]);
	pdx11DeviceImmContext->PSSetSamplers(0, 1, &pdx11CommonParams->pdx11SStates[__SState_LINEAR_ZEROBORDER]);
	pdx11DeviceImmContext->PSSetSamplers(1, 1, &pdx11CommonParams->pdx11SStates[__SState_POINT_ZEROBORDER]);

#ifdef MULTILAYERMODE
	// Proxy Setting //
	D3DXMATRIX dxmatWS2CS, dxmatWS2PS;
	D3DXMatrixLookAtRH(&dxmatWS2CS, &D3DXVECTOR3(0, 0, 1.f), &D3DXVECTOR3(0, 0, 0), &D3DXVECTOR3(0, 1.f, 0));
	D3DXMatrixOrthoRH(&dxmatWS2PS, 1.f, 1.f, 0.f, 2.f);
	D3DXMATRIX dxmatOS2PS = dxmatWS2CS * dxmatWS2PS;
	D3DXMatrixTranspose(&dxmatOS2PS, &dxmatOS2PS);
	CB_SrProxy cbProjCamState;
	cbProjCamState.matOS2PS = dxmatOS2PS;
	D3D11_MAPPED_SUBRESOURCE mappedProxyState;
	pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_PROXY], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedProxyState);
	CB_SrProxy* pCBProxyParam = (CB_SrProxy*)mappedProxyState.pData;
	memcpy(pCBProxyParam, &cbProjCamState, sizeof(CB_SrProxy));
	pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_PROXY], 0);
	pdx11DeviceImmContext->VSSetConstantBuffers(10, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_PROXY]);
#endif

#pragma endregion // Other Presetting For Shaders

	QueryPerformanceCounter(&lIntCntStart);

	// For Each Primitive //
#pragma region // Main Render Pass
	for (uint iIndexMeshObj = 0; iIndexMeshObj < (int)pvtrValidPrimitives->size(); iIndexMeshObj++)
	{
		CVXVObjectPrimitive* pCPrimitiveObject = pvtrValidPrimitives->at(iIndexMeshObj);
		SVXPrimitiveDataArchive* pstPrimitiveArchive = (SVXPrimitiveDataArchive*)pCPrimitiveObject->GetPrimitiveArchive();
		if (pstPrimitiveArchive->GetVerticeDefinition(vxrVertexTypePOSITION) == NULL)
			continue;

		int iMeshObjID = pCPrimitiveObject->GetObjectID();
		map<wstring, vector<double>>* pmapDValueMesh = NULL;
		pCLObjectForParamters->GetCustomDValue(iMeshObjID, (void**)&pmapDValueMesh);
		if (pmapDValueMesh == NULL)
			pmapDValueMesh = &mapDValueClear;

		// Object Unit Conditions
		int iVolumeID = 0, iTObjectID = 0;
		bool bIsForcedCullOff = false;
		bool bIsAirwayAnalysis = false;
		bool bIsFilledPlane = false;
		bool bIsInvertPlaneDirection = false;
		vxdouble4 d4ShadingFactors = d4GlobalShadingFactors;	// Emission, Diffusion, Specular, Specular Power
		vxmatrix matClipBoxWS2BS;
		vxdouble3 d3PosOrthoMaxClipBoxWS;
		vxdouble3 d3PosClipPlaneWS;
		vxdouble3 d3VecClipPlaneWS;
		int iClippingMode = 0; // CLIPBOX / CLIPPLANE / BOTH //
		pCPrimitiveObject->GetCustomParameter(_T("_bool_IsForcedCullOff"), &bIsForcedCullOff);
		pCPrimitiveObject->GetCustomParameter(_T("_bool_IsInvertPlaneDirection"), &bIsInvertPlaneDirection);

#define __RENDERER_WITHOUT_VOLUME 0
#define __RENDERER_VOLUME_SAMPLE_AND_MAP 1
#define __RENDERER_DIFFERENCE_DIVIATION_FROM_VOLUME 2
#define __RENDERER_IMAGE_STACK_MAP_PER_MESHSPRITE 3

		VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_int_AssociatedVolumeID"), &iVolumeID);
		VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_int_AssociatedTObjectID"), &iTObjectID);
		VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_double4_ShadingFactors"), &d4ShadingFactors);
		VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_int_ClippingMode"), &iClippingMode);	// 0 : No, 1 : CLIPBOX, 2 : CLIPPLANE, 3 : BOTH
		VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_double3_PosClipPlaneWS"), &d3PosClipPlaneWS);
		VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_double3_VecClipPlaneWS"), &d3VecClipPlaneWS);
		VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_double3_PosClipBoxMaxWS"), &d3PosOrthoMaxClipBoxWS);
		VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_matrix44_MatrixClipWS2BS"), &matClipBoxWS2BS);
		VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_bool_IsAirwayAnalysis"), &bIsAirwayAnalysis);
		VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_bool_IsFilledPlane"), &bIsFilledPlane);

		if (bIsFilledPlane)
			d4ShadingFactors = vxdouble4(1, 0, 0, 1);

		map<wstring, vector<double>>* pmapDValueVolume = NULL;
		pCLObjectForParamters->GetCustomDValue(iVolumeID, (void**)&pmapDValueVolume);
		if (pmapDValueVolume == NULL)
			pmapDValueVolume = &mapDValueClear;

		auto itrGpuVolume = mapGPUResSRVs.find(iVolumeID);
		auto itrGpuTObject = mapGPUResSRVs.find(iTObjectID);
		
		if (bIsAirwayAnalysis)
		{
			auto itrTObject = mapAssociatedObjects.find(iTObjectID);
			if (itrTObject != mapAssociatedObjects.end())
			{
				CVXTObject* pCTObject = (CVXTObject*)itrTObject->second;
				CB_VrOtf1D cbVrOtf1D;
				HDx11ComputeConstantBufferVrOtf1D(&cbVrOtf1D, pCTObject, 1, &pstModuleParameter->mapCustomParamters);
				D3D11_MAPPED_SUBRESOURCE mappedResOtf1D;
				pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_OTF1D], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResOtf1D);
				CB_VrOtf1D* pCBParamOtf1D = (CB_VrOtf1D*)mappedResOtf1D.pData;
				memcpy(pCBParamOtf1D, &cbVrOtf1D, sizeof(CB_VrOtf1D));
				pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_OTF1D], 0);
				pdx11DeviceImmContext->PSSetConstantBuffers(7, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_OTF1D]);

				vxdouble2 d2DeviationValueMinMax(0, 1);
				VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_double_DeviationMinValue"), &d2DeviationValueMinMax.x);
				VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_double_DeviationMaxValue"), &d2DeviationValueMinMax.y);
				int iMappingValueRange = 512;
				VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_int_MappingValueRange"), &iMappingValueRange);
				int iNumControlPoints = 1;
				VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_int_NumAirwayControls"), &iNumControlPoints);

				CB_SrDeviation cbSrDeviation;
				cbSrDeviation.fMinMappingValue = (float)d2DeviationValueMinMax.x;
				cbSrDeviation.fMaxMappingValue = (float)d2DeviationValueMinMax.y;
				cbSrDeviation.iValueRange = iMappingValueRange;
				cbSrDeviation.iChannelID = 0;
				cbSrDeviation.iIsoValueForVolume = 0;
				cbSrDeviation.iDummy__0 = iNumControlPoints;
				D3D11_MAPPED_SUBRESOURCE mappedDeviation;
				pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_DEVIATION], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedDeviation);
				CB_SrDeviation* pCBPolygonDeviation = (CB_SrDeviation*)mappedDeviation.pData;
				memcpy(pCBPolygonDeviation, &cbSrDeviation, sizeof(CB_SrDeviation));
				pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_DEVIATION], 0);
				pdx11DeviceImmContext->VSSetConstantBuffers(5, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_DEVIATION]);
				pdx11DeviceImmContext->PSSetConstantBuffers(5, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_DEVIATION]);

				RegisterVTResource(0, pCTObject->GetObjectID(), pCLObjectForParamters, pCGpuManager, pdx11DeviceImmContext, bInitGen,
					mapAssociatedObjects, mapGPUResSRVs, psvxLocalProgress);

#pragma region AIRWAY RESOURCE (STBUFFER) and VIEW
				SVXGPUResourceDESC stGpuResourceAirwayControlsDESC;
				stGpuResourceAirwayControlsDESC.eGpuSdkType = vxgGpuSdkTypeDX11;
				stGpuResourceAirwayControlsDESC.eDataType = vxrDataTypeFLOAT4CHANNEL;
				stGpuResourceAirwayControlsDESC.eResourceType = vxgResourceTypeDX11RESOURCE;
				stGpuResourceAirwayControlsDESC.strUsageSpecifier = _T("BUFFER_AIRWAY_CONTROLS");
				stGpuResourceAirwayControlsDESC.iSourceObjectID = pCLObjectForParamters->GetObjectID();
				stGpuResourceAirwayControlsDESC.iCustomMisc = 0;
				stGpuResourceAirwayControlsDESC.iSizeStride = sizeof(vxfloat4);
				SVXGPUResourceArchive stGpuResourceAirwayControlsSTB, stGpuResourceAirwayControlsSRV;

				if (!pCGpuManager->GetGpuResourceArchive(&stGpuResourceAirwayControlsDESC, &stGpuResourceAirwayControlsSTB))
				{
					vector<vxdouble3>* pvtrAirwayControlPoints = NULL;
					pCLObjectForParamters->GetList(_T("_vlist_DOUBLE3_AirwayPathPointWS"), (void**)&pvtrAirwayControlPoints);
					vector<double>* pvtrAirwayAreaPoints = NULL;
					pCLObjectForParamters->GetList(_T("_vlist_DOUBLE_AirwayCrossSectionalAreaWS"), (void**)&pvtrAirwayAreaPoints);
					vector<vxdouble3>* pvtrAirwayTVectors = NULL;
					pCLObjectForParamters->GetList(_T("_vlist_DOUBLE3_AirwayPathTangentVectorWS"), (void**)&pvtrAirwayTVectors);

					D3D11_BUFFER_DESC descBuf;
					ZeroMemory(&descBuf, sizeof(D3D11_BUFFER_DESC));
					descBuf.ByteWidth = iNumControlPoints * sizeof(vxfloat4);
					descBuf.MiscFlags = NULL;// D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
					descBuf.StructureByteStride = stGpuResourceAirwayControlsDESC.iSizeStride;
					descBuf.BindFlags = D3D11_BIND_SHADER_RESOURCE;
					descBuf.Usage = D3D11_USAGE_DYNAMIC;// D3D11_USAGE_DEFAULT;
					descBuf.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;// NULL;

					ID3D11Buffer* pdx11Buf = NULL;
					if (pdx11Device->CreateBuffer(&descBuf, NULL, &pdx11Buf) != S_OK)
					{
						VXERRORMESSAGE("Called SimpleVR Module's CUSTOM GpuResource - BUFFER_AIRWAY_CONTROLS FAILURE!");
						VXSAFE_RELEASE(pdx11Buf);
						return false;
					}

					D3D11_MAPPED_SUBRESOURCE d11MappedRes;
					pdx11DeviceImmContext->Map((ID3D11Resource*)pdx11Buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes);
					vxfloat4* pf4ControlPointAndArea = (vxfloat4*)d11MappedRes.pData;
					for (int iIndexControl = 0; iIndexControl < iNumControlPoints; iIndexControl++)
					{
						vxfloat3 f3PosPoint = vxfloat3(pvtrAirwayControlPoints->at(iIndexControl));
						float fArea = float(pvtrAirwayAreaPoints->at(iIndexControl));

						pf4ControlPointAndArea[iIndexControl] = vxfloat4(f3PosPoint.x, f3PosPoint.y, f3PosPoint.z, fArea);
					}
					pdx11DeviceImmContext->Unmap((ID3D11Resource*)pdx11Buf, 0);

					// Register
					stGpuResourceAirwayControlsSTB.Init();
					stGpuResourceAirwayControlsSTB.uiResourceRowPitchInBytes = descBuf.ByteWidth;
					stGpuResourceAirwayControlsSTB.i3SizeLogicalResourceInBytes = vxint3(descBuf.ByteWidth, 0, 0);
					stGpuResourceAirwayControlsSTB.vtrPtrs.push_back(pdx11Buf);
					stGpuResourceAirwayControlsSTB.svxGpuResourceDesc = stGpuResourceAirwayControlsDESC;
					stGpuResourceAirwayControlsSTB.ullRecentUsedTime = VXHGetCurrentTimePack();

					pCGpuManager->ReplaceOrAddGpuResource(&stGpuResourceAirwayControlsSTB);
				}

				stGpuResourceAirwayControlsDESC.eResourceType = vxgResourceTypeDX11SRV;

				if (!pCGpuManager->GetGpuResourceArchive(&stGpuResourceAirwayControlsDESC, &stGpuResourceAirwayControlsSRV))
				{
					D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
					ZeroMemory(&descSRV, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
					descSRV.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
					descSRV.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
					descSRV.BufferEx.FirstElement = 0;
					descSRV.BufferEx.NumElements = stGpuResourceAirwayControlsSTB.i3SizeLogicalResourceInBytes.x / stGpuResourceAirwayControlsSTB.svxGpuResourceDesc.iSizeStride;

					vector<void*> vtrViews;
					for (int i = 0; i < (int)(stGpuResourceAirwayControlsSTB.vtrPtrs.size()); i++)
					{
						ID3D11View* pdx11View = NULL;
						if (pdx11Device->CreateShaderResourceView((ID3D11Resource*)stGpuResourceAirwayControlsSTB.vtrPtrs.at(i), &descSRV, (ID3D11ShaderResourceView**)&pdx11View) != S_OK)
						{
							VXERRORMESSAGE("Shader Reousrce <CUSTOM> View Creation Failure! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
							return false;
						}
						vtrViews.push_back(pdx11View);
						stGpuResourceAirwayControlsSRV.Init();
						stGpuResourceAirwayControlsSRV.uiResourceRowPitchInBytes = stGpuResourceAirwayControlsSTB.uiResourceRowPitchInBytes;
						stGpuResourceAirwayControlsSRV.uiResourceSlicePitchInBytes = stGpuResourceAirwayControlsSTB.uiResourceSlicePitchInBytes;
						stGpuResourceAirwayControlsSRV.i3SizeLogicalResourceInBytes = stGpuResourceAirwayControlsSTB.i3SizeLogicalResourceInBytes;
						stGpuResourceAirwayControlsSRV.f3SampleIntervalElements = stGpuResourceAirwayControlsSTB.f3SampleIntervalElements;
						stGpuResourceAirwayControlsSRV.svxGpuResourceDesc = stGpuResourceAirwayControlsSTB.svxGpuResourceDesc;
						stGpuResourceAirwayControlsSRV.svxGpuResourceDesc.eResourceType = vxgResourceTypeDX11SRV;
						stGpuResourceAirwayControlsSRV.ullRecentUsedTime = VXHGetCurrentTimePack();
						stGpuResourceAirwayControlsSRV.vtrPtrs = vtrViews;

						pCGpuManager->ReplaceOrAddGpuResource(&stGpuResourceAirwayControlsSRV);
					}
				}
#pragma endregion
				// PSSetShaderResources
				pdx11DeviceImmContext->PSSetShaderResources(20, 1, (ID3D11ShaderResourceView**)&stGpuResourceAirwayControlsSRV.vtrPtrs.at(0));
				pdx11DeviceImmContext->PSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&itrGpuTObject->second.vtrPtrs.at(0));
			}
		}

		bool bIsVisible = pCPrimitiveObject->GetVisibility();
		vxmatrix matOS2WS = *pCPrimitiveObject->GetMatrixOS2WS();

		bool bIsWireframe = false;
		vxfloat4 f4WireframeColor;
		pCPrimitiveObject->GetPrimitiveWireframeVisibilityColor(&bIsWireframe, &f4WireframeColor);

		uint uiNumPrimitiveArrangements = 1;
		if (!bIsVisible)
			uiNumPrimitiveArrangements = 0;
		else if (bIsWireframe &&
			pstPrimitiveArchive->ePrimitiveType != vxrPrimitiveTypeLINE &&
			pstPrimitiveArchive->ePrimitiveType != vxrPrimitiveTypePOINT)
			uiNumPrimitiveArrangements = 2;

		vxfloat4 f4Color = pCPrimitiveObject->GetVObjectColor();

		for (uint uiIndexObjUnit = 0; uiIndexObjUnit < uiNumPrimitiveArrangements; uiIndexObjUnit++)
		{
			if (uiIndexObjUnit == 1)
				f4Color = f4WireframeColor;

			if (f4Color.w < 1.f / 255.f)
				continue;

			CB_SrPolygonObject cbPolygonObj;
#pragma region // Constant Buffers for Each Mesh Object //
			cbPolygonObj.matOS2WS = *(D3DXMATRIX*)&matOS2WS;
			D3DXMatrixTranspose(&cbPolygonObj.matOS2WS, &cbPolygonObj.matOS2WS);

			cbPolygonObj.iFlag = iClippingMode;
			vxfloat3 f3ClipBoxMaxBS;
			VXHMTransformPoint(&f3ClipBoxMaxBS, &vxfloat3(d3PosOrthoMaxClipBoxWS), &matClipBoxWS2BS);
			cbPolygonObj.f3ClipBoxMaxBS = *(D3DXVECTOR3*)&f3ClipBoxMaxBS;
			cbPolygonObj.matClipBoxWS2BS = *(D3DXMATRIX*)&vxmatrix(matClipBoxWS2BS);
			D3DXMatrixTranspose(&cbPolygonObj.matClipBoxWS2BS, &cbPolygonObj.matClipBoxWS2BS);

			cbPolygonObj.fDashDistance = 0;
			cbPolygonObj.f4Color = *(D3DXVECTOR4*)&vxfloat4(f4Color);
			cbPolygonObj.f4ShadingFactor = *(D3DXVECTOR4*)&vxfloat4(d4ShadingFactors);
			cbPolygonObj.fVThickness = 0;
			cbPolygonObj.fShadingMultiplier = 1.f;
			if (bIsInvertPlaneDirection)
				cbPolygonObj.fShadingMultiplier = -1.f;

			vxmatrix matOS2PS = matOS2WS * matWS2CS * matCS2PS;
			cbPolygonObj.matOS2PS = *(D3DXMATRIX*)&matOS2PS;
			D3DXMatrixTranspose(&cbPolygonObj.matOS2PS, &cbPolygonObj.matOS2PS);
#pragma endregion // Constant Buffers for Each Mesh Object //

#pragma region // Setting Rasterization Stages
			ID3D11InputLayout* pdx11InputLayer_Target = NULL;
			ID3D11VertexShader* pdx11VS_Target = NULL;
			ID3D11PixelShader* pdx11PS_Target = NULL;
			ID3D11RasterizerState* pdx11RState_TargetObj = NULL;
			uint uiStrideSizeInput = 0;
			uint uiOffset = 0;
			D3D_PRIMITIVE_TOPOLOGY ePRIMITIVE_TOPOLOGY;

			if (pstPrimitiveArchive->GetVerticeDefinition(vxrVertexTypeNORMAL))
			{
				if (pstPrimitiveArchive->GetVerticeDefinition(vxrVertexTypeTEXCOORD0))
				{
					// PNT
					pdx11InputLayer_Target = pdx11LayoutInputPosNorTex;
					pdx11VS_Target = pdx11VShader_PointNormalTexture;
					if (!bIsFilledPlane)
						pdx11VS_Target = pdx11VShader_Plane_PointNormalTexture;
					pdx11PS_Target = *ppdx11PSs[__PS_PHONG_TEX1COLOR];
				}
				else
				{
					// PN
					pdx11InputLayer_Target = pdx11LayoutInputPosNor;
					pdx11VS_Target = pdx11VShader_PointNormal;
					if (!bIsFilledPlane)
						pdx11VS_Target = pdx11VShader_Plane_PointNormal;
					pdx11PS_Target = *ppdx11PSs[__PS_PHONG_GLOBALCOLOR];
				}
			}
			else if (pstPrimitiveArchive->GetVerticeDefinition(vxrVertexTypeTEXCOORD0))
			{
				cbPolygonObj.f4ShadingFactor = D3DXVECTOR4(1.f, 1.f, 1.f, 1.f);
				// PT
				pdx11InputLayer_Target = pdx11LayoutInputPosTex;
				pdx11VS_Target = pdx11VShader_PointTexture;
				if (!bIsFilledPlane)
					pdx11VS_Target = pdx11VShader_Plane_PointTexture;
				pdx11PS_Target = *ppdx11PSs[__PS_MAPPING_TEX1];
			}
			else
			{
				cbPolygonObj.f4ShadingFactor = D3DXVECTOR4(1.f, 1.f, 1.f, 1.f);
				// P
				pdx11InputLayer_Target = pdx11LayoutInputPos;
				pdx11VS_Target = pdx11VShader_Point;
				if (!bIsFilledPlane)
					pdx11VS_Target = pdx11VShader_Plane_Point;
				pdx11PS_Target = *ppdx11PSs[__PS_MAPPING_TEX1];
			}

			if (bIsAirwayAnalysis)
				pdx11PS_Target = *ppdx11PSs[__PS_AIRWAY_ANALYSIS];

			switch (pstPrimitiveArchive->ePrimitiveType)
			{
			case vxrPrimitiveTypeLINE:
				if (pstPrimitiveArchive->bIsPrimitiveStripe)
					ePRIMITIVE_TOPOLOGY = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
				else
					ePRIMITIVE_TOPOLOGY = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
				bIsWireframe = true;
				break;
			case vxrPrimitiveTypeTRIANGLE:
				if (pstPrimitiveArchive->bIsPrimitiveStripe)
					ePRIMITIVE_TOPOLOGY = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
				else
					ePRIMITIVE_TOPOLOGY = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
				break;
			case vxrPrimitiveTypePOINT:
				ePRIMITIVE_TOPOLOGY = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
				break;
			default:
				continue;
			}
			int iAntiAliasingIndex = 0;
			if (bIsAntiAliasingRS)
				iAntiAliasingIndex = __RState_ANTIALIASING_SOLID_CW;
			iAntiAliasingIndex = 0;	// Current Version does not support Anti-Aliasing!!

			EnumCullingMode eCullingMode = rxSR_CULL_NONE;	// To Do //
			if (pstPrimitiveArchive->bIsPrimitiveFrontCCW)
			{
				if (!bIsWireframe && eCullingMode != rxSR_CULL_NONE)
				if (eCullingMode == rxSR_CULL_DEFAULT || eCullingMode == rxSR_CULL_FORCED_CW)
					pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_SOLID_CCW + iAntiAliasingIndex];
				else // CCW
					pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_SOLID_CW + iAntiAliasingIndex];
				else
				{
					if (!bIsWireframe)
						pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_SOLID_NONE + iAntiAliasingIndex];
					else
						pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_WIRE_NONE + iAntiAliasingIndex];
				}
			}
			else
			{
				if (!bIsWireframe && eCullingMode != rxSR_CULL_NONE)
				if (eCullingMode == rxSR_CULL_DEFAULT || eCullingMode == rxSR_CULL_FORCED_CCW)
					pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_SOLID_CW + iAntiAliasingIndex];
				else // CW
					pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_SOLID_CCW + iAntiAliasingIndex];
				else
				{
					if (!bIsWireframe)
						pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_SOLID_NONE + iAntiAliasingIndex];
					else
						pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_WIRE_NONE + iAntiAliasingIndex];
				}
			}
			if (bIsWireframe)
				pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_WIRE_NONE];
			else if (bIsForcedCullOff && !bIsWireframe)
				pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_SOLID_NONE + iAntiAliasingIndex];

			ID3D11Buffer* pdx11BufferTargetMesh = (ID3D11Buffer*)vtrGpuResourceArchiveBufferVertex.at(iIndexMeshObj).vtrPtrs.at(0);
			ID3D11Buffer* pdx11IndiceTargetMesh = NULL;
			uiStrideSizeInput = sizeof(vxfloat3)*(uint)pstPrimitiveArchive->GetNumVertexDefinitions();
			pdx11DeviceImmContext->IASetVertexBuffers(0, 1, (ID3D11Buffer**)&pdx11BufferTargetMesh, &uiStrideSizeInput, &uiOffset);
			if (pstPrimitiveArchive->puiIndexList != NULL)
			{
				pdx11IndiceTargetMesh = (ID3D11Buffer*)vtrGpuResourceArchiveBufferIndex.at(iIndexMeshObj).vtrPtrs.at(0);
				pdx11DeviceImmContext->IASetIndexBuffer(pdx11IndiceTargetMesh, DXGI_FORMAT_R32_UINT, 0);
			}

			pdx11DeviceImmContext->IASetInputLayout(pdx11InputLayer_Target);
			pdx11DeviceImmContext->VSSetShader(pdx11VS_Target, NULL, 0);
			pdx11DeviceImmContext->PSSetShader(pdx11PS_Target, NULL, 0);
			pdx11DeviceImmContext->RSSetState(pdx11RState_TargetObj);
			pdx11DeviceImmContext->IASetPrimitiveTopology(ePRIMITIVE_TOPOLOGY);
#pragma endregion // Setting Rasterization Stages

			D3D11_MAPPED_SUBRESOURCE mappedPolygonObjRes;
			pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_POLYGONOBJ], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedPolygonObjRes);
			CB_SrPolygonObject* pCBPolygonObjParam = (CB_SrPolygonObject*)mappedPolygonObjRes.pData;
			memcpy(pCBPolygonObjParam, &cbPolygonObj, sizeof(CB_SrPolygonObject));
			pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_POLYGONOBJ], 0);
			pdx11DeviceImmContext->VSSetConstantBuffers(0, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_POLYGONOBJ]);
			pdx11DeviceImmContext->PSSetConstantBuffers(0, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_POLYGONOBJ]);

			iCountDrawings[iLoopMode]++;

			if (!bIsFilledPlane)
			{
				// Render Work //
#ifdef MULTILAYERMODE
#pragma region // RENDERING PASS
				if (pstPrimitiveArchive->bIsPrimitiveStripe || pstPrimitiveArchive->ePrimitiveType == vxrPrimitiveTypePOINT)
				{
					pdx11DeviceImmContext->Draw(pstPrimitiveArchive->uiNumVertice, 0);
				}
				else
				{
					pdx11DeviceImmContext->DrawIndexed(pstPrimitiveArchive->uiNumIndice, 0, 0);
				}

				if (iNumTexureLayers == 1)
					continue;

				iCountRTBuffers++;
				int iModRTLayer = iCountRTBuffers % iNumTexureLayers;
				if (iModRTLayer == 0)
				{
					pdx11DeviceImmContext->OMSetRenderTargets(2, pdx11RTVs_NULL, (ID3D11DepthStencilView*)gpuResourcesFB_ARCHs[__FB_DSV_DEPTH].vtrPtrs.at(0));

					pdx11DeviceImmContext->PSSetShaderResources(30, NUM_TEXRT_LAYERS, pdx11SRV_RenderOuts);
					pdx11DeviceImmContext->PSSetShaderResources(60, NUM_TEXRT_LAYERS, pdx11SRV_DepthCSs);

					int iRSA_Index_Offset_Prev = 0;
					int iRSA_Index_Offset_Out = NUM_MERGE_LAYERS;
					if (iCountMerging % 2 == 0)
					{
						iRSA_Index_Offset_Prev = NUM_MERGE_LAYERS;
						iRSA_Index_Offset_Out = 0;
					}

					for (int iIndexRS = 0; iIndexRS < NUM_MERGE_LAYERS; iIndexRS++)
					{
						pdx11SRV_RSA[iIndexRS] = vtrSRV_MergePingpongPS.at(iRSA_Index_Offset_Prev + iIndexRS);
						pdx11RTV_RSA[iIndexRS] = vtrRTV_MergePingpongPS.at(iRSA_Index_Offset_Out + iIndexRS);
					}

					pdx11DeviceImmContext->PSSetShaderResources(90, NUM_MERGE_LAYERS, pdx11SRV_RSA);
					pdx11DeviceImmContext->OMSetRenderTargets(min(NUM_MERGE_LAYERS, 8), pdx11RTV_RSA, NULL);

					uiStrideSizeInput = sizeof(vxfloat3);
					uiOffset = 0;
					pdx11DeviceImmContext->IASetVertexBuffers(0, 1, (ID3D11Buffer**)&pdx11Bufs[0], &uiStrideSizeInput, &uiOffset);
					pdx11DeviceImmContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
					pdx11DeviceImmContext->IASetInputLayout(pdx11LayoutInputPos);
					pdx11DeviceImmContext->RSSetState(pdx11CommonParams->pdx11RStates[__RState_SOLID_NONE]);
					pdx11DeviceImmContext->VSSetShader(pdx11VShader_Proxy, NULL, 0);
					pdx11DeviceImmContext->PSSetShader(*ppdx11PSs[__PS_MERGE_RSOUT_TO_LAYERS], NULL, 0);
					pdx11DeviceImmContext->OMSetDepthStencilState(pdx11CommonParams->pdx11DSStates[__DSState_DEPTHENABLE_ALWAYS], 0);

					pdx11DeviceImmContext->Draw(4, 0);

					// Set NULL States //
					pdx11DeviceImmContext->PSSetShaderResources(30, NUM_TEXRT_LAYERS, pdx11SRV_RenderOuts_NULL);
					pdx11DeviceImmContext->PSSetShaderResources(60, NUM_TEXRT_LAYERS, pdx11SRV_DepthCSs_NULL);
					pdx11DeviceImmContext->PSSetShaderResources(90, NUM_MERGE_LAYERS, pdx11SRV_RSA_NULL);
					pdx11DeviceImmContext->OMSetRenderTargets(min(NUM_MERGE_LAYERS, 8), pdx11RTV_RSA_NULL, NULL);	// MAX 8 in Feature Level 10_0

					// Restore //
					uiStrideSizeInput = sizeof(vxfloat3)*(uint)pstPrimitiveArchive->GetNumVertexDefinitions();
					uiOffset = 0;
					pdx11DeviceImmContext->IASetVertexBuffers(0, 1, (ID3D11Buffer**)&pdx11BufferTargetMesh, &uiStrideSizeInput, &uiOffset);
					pdx11DeviceImmContext->IASetPrimitiveTopology(ePRIMITIVE_TOPOLOGY);
					pdx11DeviceImmContext->IASetInputLayout(pdx11InputLayer_Target);
					pdx11DeviceImmContext->IASetIndexBuffer(pdx11IndiceTargetMesh, DXGI_FORMAT_R32_UINT, 0);
					pdx11DeviceImmContext->RSSetState(pdx11RState_TargetObj);
					pdx11DeviceImmContext->VSSetShader(pdx11VS_Target, NULL, 0);
					pdx11DeviceImmContext->PSSetShader(pdx11PS_Target, NULL, 0);
					pdx11DeviceImmContext->OMSetDepthStencilState(pdx11CommonParams->pdx11DSStates[__DSState_DEPTHENABLE_LESSEQUAL], 0);

					// Clear //
					for (int iIndexRS = 0; iIndexRS < NUM_MERGE_LAYERS; iIndexRS++)
						pdx11DeviceImmContext->ClearRenderTargetView(vtrRTV_MergePingpongPS.at(iIndexRS + iRSA_Index_Offset_Prev), fClearColor);

					for (int iIndexRTT = 0; iIndexRTT < iNumTexureLayers; iIndexRTT++)
					{
						pdx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_RENDEROUT].vtrPtrs.at(iIndexRTT), fClearColor);
						pdx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_DEPTH_ZTHICKCS].vtrPtrs.at(iIndexRTT), fClearColor);
					}

					iCountMerging++;
				}	// iModRTLayer == 0

				pdx11DeviceImmContext->PSSetShaderResources(30, NUM_TEXRT_LAYERS, pdx11SRV_RenderOuts_NULL);
				pdx11DeviceImmContext->PSSetShaderResources(60, NUM_TEXRT_LAYERS, pdx11SRV_DepthCSs_NULL);

				pdx11RTVs[0] = (ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_RENDEROUT].vtrPtrs.at(iModRTLayer);
				pdx11RTVs[1] = (ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_DEPTH_ZTHICKCS].vtrPtrs.at(iModRTLayer);
				pdx11DeviceImmContext->OMSetRenderTargets(2, pdx11RTVs, (ID3D11DepthStencilView*)gpuResourcesFB_ARCHs[__FB_DSV_DEPTH].vtrPtrs.at(0));

				if (iNumTexureLayers > 1)
				{
					pdx11DeviceImmContext->ClearDepthStencilView((ID3D11DepthStencilView*)gpuResourcesFB_ARCHs[__FB_DSV_DEPTH].vtrPtrs.at(0), D3D11_CLEAR_DEPTH, 1.0f, 0);
					pdx11DeviceImmContext->OMSetDepthStencilState(pdx11CommonParams->pdx11DSStates[__DSState_DEPTHENABLE_LESSEQUAL], 0);
				}
#pragma endregion // SELF RENDERING PASS
#else
				if (pstPrimitiveArchive->bIsPrimitiveStripe || pstPrimitiveArchive->ePrimitiveType == vxrPrimitiveTypePOINT)
					pdx11DeviceImmContext->Draw(pstPrimitiveArchive->uiNumVertice, 0);
				else
					pdx11DeviceImmContext->DrawIndexed(pstPrimitiveArchive->uiNumIndice, 0, 0);
#endif
			}
			else
			{
				pdx11PS_Target = *ppdx11PSs[__PS_PHONG_GLOBALCOLOR];

				// 1st Step : Draw BackFace //
				pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_SOLID_CCW];
				if (pstPrimitiveArchive->bIsPrimitiveFrontCCW)
					pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_SOLID_CW];
				pdx11DeviceImmContext->RSSetState(pdx11RState_TargetObj);

				if (pstPrimitiveArchive->bIsPrimitiveStripe || pstPrimitiveArchive->ePrimitiveType == vxrPrimitiveTypePOINT)
					pdx11DeviceImmContext->Draw(pstPrimitiveArchive->uiNumVertice, 0);
				else
					pdx11DeviceImmContext->DrawIndexed(pstPrimitiveArchive->uiNumIndice, 0, 0);

				// 2nd Step : Draw FrontFace //
				pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_POLYGONOBJ], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedPolygonObjRes);
				CB_SrPolygonObject* pCBPolygonObjParam = (CB_SrPolygonObject*)mappedPolygonObjRes.pData;
				cbPolygonObj.f4Color = D3DXVECTOR4(0, 0, 0, 0);
				memcpy(pCBPolygonObjParam, &cbPolygonObj, sizeof(CB_SrPolygonObject));
				pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_POLYGONOBJ], 0);
				pdx11DeviceImmContext->VSSetConstantBuffers(0, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_POLYGONOBJ]);
				pdx11DeviceImmContext->PSSetConstantBuffers(0, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_POLYGONOBJ]);

				pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_SOLID_CW];
				if (pstPrimitiveArchive->bIsPrimitiveFrontCCW)
					pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_SOLID_CCW];
				pdx11DeviceImmContext->RSSetState(pdx11RState_TargetObj);

				if (pstPrimitiveArchive->bIsPrimitiveStripe || pstPrimitiveArchive->ePrimitiveType == vxrPrimitiveTypePOINT)
					pdx11DeviceImmContext->Draw(pstPrimitiveArchive->uiNumVertice, 0);
				else
					pdx11DeviceImmContext->DrawIndexed(pstPrimitiveArchive->uiNumIndice, 0, 0);
			}
		}
#pragma endregion // Main Render Pass
	}


	if (iLoopMode == __PLANE_RENDER_MODE_3D)
	{
#pragma region // Final Rearrangement
		if (iCountRTBuffers % iNumTexureLayers != 0 || (iCountRTBuffers == 0 && iCountDrawings[iLoopMode] > 0))
		{
			pdx11DeviceImmContext->OMSetRenderTargets(2, pdx11RTVs_NULL, (ID3D11DepthStencilView*)gpuResourcesFB_ARCHs[__FB_DSV_DEPTH].vtrPtrs.at(0));

			pdx11DeviceImmContext->PSSetShaderResources(30, NUM_TEXRT_LAYERS, pdx11SRV_RenderOuts);
			pdx11DeviceImmContext->PSSetShaderResources(60, NUM_TEXRT_LAYERS, pdx11SRV_DepthCSs);

			int iRSA_Index_Offset_Prev = 0;
			int iRSA_Index_Offset_Out = NUM_MERGE_LAYERS;
			if (iCountMerging % 2 == 0)
			{
				iRSA_Index_Offset_Prev = NUM_MERGE_LAYERS;
				iRSA_Index_Offset_Out = 0;
			}
			for (int iIndexRS = 0; iIndexRS < NUM_MERGE_LAYERS; iIndexRS++)
			{
				pdx11SRV_RSA[iIndexRS] = vtrSRV_MergePingpongPS.at(iRSA_Index_Offset_Prev + iIndexRS);
				pdx11RTV_RSA[iIndexRS] = vtrRTV_MergePingpongPS.at(iRSA_Index_Offset_Out + iIndexRS);
			}

			pdx11DeviceImmContext->PSSetShaderResources(90, NUM_MERGE_LAYERS, pdx11SRV_RSA);
			pdx11DeviceImmContext->OMSetRenderTargets(min(NUM_MERGE_LAYERS, 8), pdx11RTV_RSA, NULL);

			uint uiStrideSizeInput = sizeof(vxfloat3);
			uint uiOffset = 0;
			pdx11DeviceImmContext->IASetVertexBuffers(0, 1, (ID3D11Buffer**)&pdx11Bufs[0], &uiStrideSizeInput, &uiOffset);
			pdx11DeviceImmContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			pdx11DeviceImmContext->IASetInputLayout(pdx11LayoutInputPos);
			pdx11DeviceImmContext->RSSetState(pdx11CommonParams->pdx11RStates[__RState_SOLID_NONE]);
			pdx11DeviceImmContext->VSSetShader(pdx11VShader_Proxy, NULL, 0);
			pdx11DeviceImmContext->PSSetShader(*ppdx11PSs[__PS_MERGE_RSOUT_TO_LAYERS], NULL, 0);
			pdx11DeviceImmContext->OMSetDepthStencilState(pdx11CommonParams->pdx11DSStates[__DSState_DEPTHENABLE_ALWAYS], 0);

			pdx11DeviceImmContext->Draw(4, 0);

			pdx11DeviceImmContext->PSSetShaderResources(90, NUM_MERGE_LAYERS, pdx11SRV_RSA_NULL);
			pdx11DeviceImmContext->OMSetRenderTargets(min(NUM_MERGE_LAYERS, 8), pdx11RTV_RSA_NULL, NULL);
			pdx11DeviceImmContext->PSSetShaderResources(30, NUM_TEXRT_LAYERS, pdx11SRV_RenderOuts_NULL);
			pdx11DeviceImmContext->PSSetShaderResources(60, NUM_TEXRT_LAYERS, pdx11SRV_DepthCSs_NULL);

			iCountMerging++;
		}
#pragma endregion // Final Rearrangement
	}

	// Always, this renderer converts the gpu RT to cpu sys mem. 
#pragma region // Copy GPU to CPU
	SVXGPUResourceDESC gpuResourceFB_RENDEROUT_SYSTEM_DESC;
	SVXGPUResourceDESC gpuResourceFB_DEPTH_SYSTEM_DESC;
	SVXGPUResourceArchive gpuResourceFB_RENDEROUT_SYSTEM_ARCH;
	SVXGPUResourceArchive gpuResourceFB_DEPTH_SYSTEM_ARCH;

	if (iLoopMode == __PLANE_RENDER_MODE_3D && (iCountDrawings[0] + iCountDrawings[1]) == 0)
	{
		SVXFrameBuffer* pstFrameBuffer = (SVXFrameBuffer*)pCOutputIObject->GetFrameBuffer(vxrFrameBufferUsageRENDEROUT, 0);
		vxbyte4* py4Buffer = (vxbyte4*)pstFrameBuffer->pvBuffer;
		SVXFrameBuffer* pstFrameBufferDepth = (SVXFrameBuffer*)pCOutputIObject->GetFrameBuffer(vxrFrameBufferUsageDEPTH, 0);
		float* pfBuffer = (float*)pstFrameBufferDepth->pvBuffer;
		memset(py4Buffer, 0, i2SizeScreenCurrent.x * i2SizeScreenCurrent.y * sizeof(vxbyte4));
		memset(pfBuffer, 0x77, i2SizeScreenCurrent.x * i2SizeScreenCurrent.y * sizeof(float));

		goto ENDRENDER;
	}
	if (iCountDrawings[iLoopMode] == 0)
	{
		SVXFrameBuffer* pstFrameBuffer = (SVXFrameBuffer*)pCOutputIObject->GetFrameBuffer(vxrFrameBufferUsageRENDEROUT, iLoopMode + 1);
		vxbyte4* py4Buffer = (vxbyte4*)pstFrameBuffer->pvBuffer;
		memset(py4Buffer, 0, i2SizeScreenCurrent.x * i2SizeScreenCurrent.y * sizeof(vxbyte4));

		if (iLoopMode == __PLANE_RENDER_MODE_3D)
		{
			SVXFrameBuffer* psvxFrameBufferDepth = (SVXFrameBuffer*)pCOutputIObject->GetFrameBuffer(vxrFrameBufferUsageDEPTH, 0);
			float* pfBuffer = (float*)psvxFrameBufferDepth->pvBuffer;
			memset(pfBuffer, 0x77, i2SizeScreenCurrent.x * i2SizeScreenCurrent.y * sizeof(float));
		}
	}

#ifdef MULTILAYERMODE
	if (iLoopMode == __PLANE_RENDER_MODE_FILLED && iCountDrawings[iLoopMode] > 0)
	{
		gpuResourceFB_RENDEROUT_SYSTEM_DESC.eResourceType = vxgResourceTypeDX11RESOURCE;
		gpuResourceFB_RENDEROUT_SYSTEM_DESC.eDataType = gpuResourcesFB_DESCs[__FB_TX2D_RENDEROUT].eDataType;
		gpuResourceFB_RENDEROUT_SYSTEM_DESC.eGpuSdkType = vxgGpuSdkTypeDX11;
		gpuResourceFB_RENDEROUT_SYSTEM_DESC.strUsageSpecifier = _T("TEXTURE2D_IOBJECT_SYSTEM");
		gpuResourceFB_RENDEROUT_SYSTEM_DESC.iSizeStride = VXHGetDataTypeSizeByte(gpuResourceFB_RENDEROUT_SYSTEM_DESC.eDataType);
		gpuResourceFB_RENDEROUT_SYSTEM_DESC.iCustomMisc = 0;
		gpuResourceFB_RENDEROUT_SYSTEM_DESC.iSourceObjectID = pCOutputIObject->GetObjectID();
		gpuResourceFB_DEPTH_SYSTEM_DESC.eResourceType = vxgResourceTypeDX11RESOURCE;
		gpuResourceFB_DEPTH_SYSTEM_DESC.eDataType = vxrDataTypeFLOAT;
		gpuResourceFB_DEPTH_SYSTEM_DESC.eGpuSdkType = vxgGpuSdkTypeDX11;
		gpuResourceFB_DEPTH_SYSTEM_DESC.strUsageSpecifier = _T("TEXTURE2D_IOBJECT_SYSTEM");
		gpuResourceFB_DEPTH_SYSTEM_DESC.iSizeStride = VXHGetDataTypeSizeByte(gpuResourceFB_DEPTH_SYSTEM_DESC.eDataType);
		gpuResourceFB_DEPTH_SYSTEM_DESC.iCustomMisc = 0;
		gpuResourceFB_DEPTH_SYSTEM_DESC.iSourceObjectID = pCOutputIObject->GetObjectID();
		// When Resize, the Framebuffer is released, so we do not have to check the resize case.
		if (!pCGpuManager->GetGpuResourceArchive(&gpuResourceFB_RENDEROUT_SYSTEM_DESC, &gpuResourceFB_RENDEROUT_SYSTEM_ARCH))
			pCGpuManager->GenerateGpuResource(&gpuResourceFB_RENDEROUT_SYSTEM_DESC, pCOutputIObject, &gpuResourceFB_RENDEROUT_SYSTEM_ARCH, NULL);
		if (!pCGpuManager->GetGpuResourceArchive(&gpuResourceFB_DEPTH_SYSTEM_DESC, &gpuResourceFB_DEPTH_SYSTEM_ARCH))
			pCGpuManager->GenerateGpuResource(&gpuResourceFB_DEPTH_SYSTEM_DESC, pCOutputIObject, &gpuResourceFB_DEPTH_SYSTEM_ARCH, NULL);

		SVXFrameBuffer* pstFrameBuffer = (SVXFrameBuffer*)pCOutputIObject->GetFrameBuffer(vxrFrameBufferUsageRENDEROUT, iLoopMode + 1);
		vxbyte4* py4Buffer = (vxbyte4*)pstFrameBuffer->pvBuffer;

		D3D11_MAPPED_SUBRESOURCE mappedResSys;
		pdx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gpuResourceFB_RENDEROUT_SYSTEM_ARCH.vtrPtrs.at(0), (ID3D11Texture2D*)gpuResourcesFB_ARCHs[__FB_TX2D_RENDEROUT].vtrPtrs.at(0));
		pdx11DeviceImmContext->Map((ID3D11Texture2D*)gpuResourceFB_RENDEROUT_SYSTEM_ARCH.vtrPtrs.at(0), 0, D3D11_MAP_READ, NULL, &mappedResSys);

		vxbyte4* py4RenderOuts = (vxbyte4*)mappedResSys.pData;
		for (int i = 0; i < i2SizeScreenCurrent.y; i++)
		{
			for (int j = 0; j < i2SizeScreenCurrent.x; j++)
			{
				// __PS_MERGE_LAYERS_TO_RENDEROUT 에서 INT -> FLOAT4 로 되어 배열된 color 요소가 들어 옴. //
				vxbyte4 y4Renderout = py4RenderOuts[j];
				vxbyte4 y4RenderoutCorrection;
				y4RenderoutCorrection.x = y4Renderout.z;	// B
				y4RenderoutCorrection.y = y4Renderout.y;	// G
				y4RenderoutCorrection.z = y4Renderout.x;	// R
				y4RenderoutCorrection.w = y4Renderout.w;	// A

				py4Buffer[i*i2SizeScreenCurrent.x + j] = y4RenderoutCorrection;
			}
			py4RenderOuts += mappedResSys.RowPitch / 4;	// mappedResSys.RowPitch (byte unit)
		}

		pdx11DeviceImmContext->Unmap((ID3D11Texture2D*)gpuResourceFB_RENDEROUT_SYSTEM_ARCH.vtrPtrs.at(0), 0);
	}
	else
	{
		// PS into PS
		int iRSA_Index_Offset_Prev = 0;
		if (iCountMerging % 2 == 0)
		{
			iRSA_Index_Offset_Prev = NUM_MERGE_LAYERS;
		}
		for(int iIndexRS = 0; iIndexRS < NUM_MERGE_LAYERS; iIndexRS++)
		{
			pdx11SRV_RSA[iIndexRS] = vtrSRV_MergePingpongPS.at(iRSA_Index_Offset_Prev + iIndexRS);
		}
		pdx11DeviceImmContext->PSSetShaderResources(90, NUM_MERGE_LAYERS, pdx11SRV_RSA);

		pdx11RTVs[0] = (ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_RENDEROUT].vtrPtrs.at(0);
		pdx11RTVs[1] = (ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_DEPTH_ZTHICKCS].vtrPtrs.at(0);
		pdx11DeviceImmContext->OMSetRenderTargets(2, pdx11RTVs, NULL);

		//__PS_MERGE_LAYERS_TO_RENDEROUT
		uint uiStrideSizeInput = sizeof(vxfloat3);
		uint uiOffset = 0;
		pdx11DeviceImmContext->IASetVertexBuffers(0, 1, (ID3D11Buffer**)&pdx11Bufs[0], &uiStrideSizeInput, &uiOffset);
		pdx11DeviceImmContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		pdx11DeviceImmContext->IASetInputLayout(pdx11LayoutInputPos);
		pdx11DeviceImmContext->RSSetState(pdx11CommonParams->pdx11RStates[__RState_SOLID_NONE]);
		pdx11DeviceImmContext->OMSetDepthStencilState( pdx11CommonParams->pdx11DSStates[__DSState_DEPTHENABLE_ALWAYS], 0 );
		pdx11DeviceImmContext->VSSetShader(pdx11VShader_Proxy, NULL, 0);
		pdx11DeviceImmContext->PSSetShader(*ppdx11PSs[__PS_MERGE_LAYERS_TO_RENDEROUT], NULL, 0);

		pdx11DeviceImmContext->Draw( 4, 0 );

#pragma region // Copy GPU to CPU
		SVXGPUResourceDESC gpuResourceFB_RENDEROUT_SYSTEM_DESC;
		SVXGPUResourceDESC gpuResourceFB_DEPTH_SYSTEM_DESC;
		SVXGPUResourceArchive gpuResourceFB_RENDEROUT_SYSTEM_ARCH;
		SVXGPUResourceArchive gpuResourceFB_DEPTH_SYSTEM_ARCH;
		gpuResourceFB_RENDEROUT_SYSTEM_DESC.eResourceType = vxgResourceTypeDX11RESOURCE;
		gpuResourceFB_RENDEROUT_SYSTEM_DESC.eDataType = gpuResourcesFB_DESCs[__FB_TX2D_RENDEROUT].eDataType;
		gpuResourceFB_RENDEROUT_SYSTEM_DESC.eGpuSdkType = vxgGpuSdkTypeDX11;
		gpuResourceFB_RENDEROUT_SYSTEM_DESC.strUsageSpecifier = _T("TEXTURE2D_IOBJECT_SYSTEM");
		gpuResourceFB_RENDEROUT_SYSTEM_DESC.iSizeStride = VXHGetDataTypeSizeByte(gpuResourceFB_RENDEROUT_SYSTEM_DESC.eDataType);
		gpuResourceFB_RENDEROUT_SYSTEM_DESC.iCustomMisc = 0;
		gpuResourceFB_RENDEROUT_SYSTEM_DESC.iSourceObjectID = pCOutputIObject->GetObjectID();
		gpuResourceFB_DEPTH_SYSTEM_DESC.eResourceType = vxgResourceTypeDX11RESOURCE;
		gpuResourceFB_DEPTH_SYSTEM_DESC.eDataType = vxrDataTypeFLOAT;
		gpuResourceFB_DEPTH_SYSTEM_DESC.eGpuSdkType = vxgGpuSdkTypeDX11;
		gpuResourceFB_DEPTH_SYSTEM_DESC.strUsageSpecifier = _T("TEXTURE2D_IOBJECT_SYSTEM");
		gpuResourceFB_DEPTH_SYSTEM_DESC.iSizeStride = VXHGetDataTypeSizeByte(gpuResourceFB_DEPTH_SYSTEM_DESC.eDataType);
		gpuResourceFB_DEPTH_SYSTEM_DESC.iCustomMisc = 0;
		gpuResourceFB_DEPTH_SYSTEM_DESC.iSourceObjectID = pCOutputIObject->GetObjectID();
		// When Resize, the Framebuffer is released, so we do not have to check the resize case.
		if(!pCGpuManager->GetGpuResourceArchive(&gpuResourceFB_RENDEROUT_SYSTEM_DESC, &gpuResourceFB_RENDEROUT_SYSTEM_ARCH))
			pCGpuManager->GenerateGpuResource(&gpuResourceFB_RENDEROUT_SYSTEM_DESC, pCOutputIObject, &gpuResourceFB_RENDEROUT_SYSTEM_ARCH, NULL);
		if(!pCGpuManager->GetGpuResourceArchive(&gpuResourceFB_DEPTH_SYSTEM_DESC, &gpuResourceFB_DEPTH_SYSTEM_ARCH))
			pCGpuManager->GenerateGpuResource(&gpuResourceFB_DEPTH_SYSTEM_DESC, pCOutputIObject, &gpuResourceFB_DEPTH_SYSTEM_ARCH, NULL);

		SVXFrameBuffer* psvxFrameBuffer = (SVXFrameBuffer*)pCOutputIObject->GetFrameBuffer(vxrFrameBufferUsageRENDEROUT, iLoopMode + 1);
		vxbyte4* py4Buffer = (vxbyte4*)psvxFrameBuffer->pvBuffer;

		D3D11_MAPPED_SUBRESOURCE mappedResSys;
		pdx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gpuResourceFB_RENDEROUT_SYSTEM_ARCH.vtrPtrs.at(0), (ID3D11Texture2D*)gpuResourcesFB_ARCHs[__FB_TX2D_RENDEROUT].vtrPtrs.at(0));
		pdx11DeviceImmContext->Map((ID3D11Texture2D*)gpuResourceFB_RENDEROUT_SYSTEM_ARCH.vtrPtrs.at(0), 0, D3D11_MAP_READ, NULL, &mappedResSys);

		vxbyte4* py4RenderOuts = (vxbyte4*)mappedResSys.pData;
		for (int i = 0; i < i2SizeScreenCurrent.y; i++)
		{
			for (int j = 0; j < i2SizeScreenCurrent.x; j++)
			{
				// __PS_MERGE_LAYERS_TO_RENDEROUT 에서 INT -> FLOAT4 로 되어 배열된 color 요소가 들어 옴. //
				vxbyte4 y4Renderout = py4RenderOuts[j];

				// BGRA
				py4Buffer[i*i2SizeScreenCurrent.x + j] = vxbyte4(y4Renderout.z, y4Renderout.y, y4Renderout.x, y4Renderout.w);
			}
			py4RenderOuts += mappedResSys.RowPitch / 4;	// mappedResSys.RowPitch (byte unit)
		}

		pdx11DeviceImmContext->Unmap((ID3D11Texture2D*)gpuResourceFB_RENDEROUT_SYSTEM_ARCH.vtrPtrs.at(0), 0);

		SVXFrameBuffer* psvxFrameBufferDepth = (SVXFrameBuffer*)pCOutputIObject->GetFrameBuffer(vxrFrameBufferUsageDEPTH, 0);
		float* pfBuffer = (float*)psvxFrameBufferDepth->pvBuffer;
		pdx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gpuResourceFB_DEPTH_SYSTEM_ARCH.vtrPtrs.at(0), (ID3D11Texture2D*)gpuResourcesFB_ARCHs[__FB_TX2D_DEPTH_ZTHICKCS].vtrPtrs.at(0));
		pdx11DeviceImmContext->Map((ID3D11Texture2D*)gpuResourceFB_DEPTH_SYSTEM_ARCH.vtrPtrs.at(0), 0, D3D11_MAP_READ, NULL, &mappedResSys);
		float* pfDeferredContexts = (float*)mappedResSys.pData;
		for (int i = 0; i < i2SizeScreenCurrent.y; i++)
		{
			for (int j = 0; j < i2SizeScreenCurrent.x; j++)
			{
				if (py4Buffer[i*i2SizeScreenCurrent.x + j].w > 0)
					pfBuffer[i*i2SizeScreenCurrent.x + j] = pfDeferredContexts[j];
				else
					pfBuffer[i*i2SizeScreenCurrent.x + j] = FLT_MAX;
			}
			pfDeferredContexts += mappedResSys.RowPitch / 4;	// mappedResSys.RowPitch (byte unit)
		}
		pdx11DeviceImmContext->Unmap((ID3D11Texture2D*)gpuResourceFB_DEPTH_SYSTEM_ARCH.vtrPtrs.at(0), 0);
#pragma endregion
	}

#else
	gpuResourceFB_RENDEROUT_SYSTEM_DESC.eResourceType = vxgResourceTypeDX11RESOURCE;
	gpuResourceFB_RENDEROUT_SYSTEM_DESC.eDataType = gpuResourcesFB_DESCs[__FB_TX2D_RENDEROUT].eDataType;
	gpuResourceFB_RENDEROUT_SYSTEM_DESC.eGpuSdkType = vxgGpuSdkTypeDX11;
	gpuResourceFB_RENDEROUT_SYSTEM_DESC.strUsageSpecifier = _T("TEXTURE2D_IOBJECT_SYSTEM");
	gpuResourceFB_RENDEROUT_SYSTEM_DESC.iSizeStride = VXHGetDataTypeSizeByte(gpuResourceFB_RENDEROUT_SYSTEM_DESC.eDataType);
	gpuResourceFB_RENDEROUT_SYSTEM_DESC.iCustomMisc = 0;
	gpuResourceFB_RENDEROUT_SYSTEM_DESC.iSourceObjectID = pCOutputIObject->GetObjectID();
	gpuResourceFB_DEPTH_SYSTEM_DESC.eResourceType = vxgResourceTypeDX11RESOURCE;
	gpuResourceFB_DEPTH_SYSTEM_DESC.eDataType = vxrDataTypeFLOAT;
	gpuResourceFB_DEPTH_SYSTEM_DESC.eGpuSdkType = vxgGpuSdkTypeDX11;
	gpuResourceFB_DEPTH_SYSTEM_DESC.strUsageSpecifier = _T("TEXTURE2D_IOBJECT_SYSTEM");
	gpuResourceFB_DEPTH_SYSTEM_DESC.iSizeStride = VXHGetDataTypeSizeByte(gpuResourceFB_DEPTH_SYSTEM_DESC.eDataType);
	gpuResourceFB_DEPTH_SYSTEM_DESC.iCustomMisc = 0;
	gpuResourceFB_DEPTH_SYSTEM_DESC.iSourceObjectID = pCOutputIObject->GetObjectID();
	// When Resize, the Framebuffer is released, so we do not have to check the resize case.
	if (!pCGpuManager->GetGpuResourceArchive(&gpuResourceFB_RENDEROUT_SYSTEM_DESC, &gpuResourceFB_RENDEROUT_SYSTEM_ARCH))
		pCGpuManager->GenerateGpuResource(&gpuResourceFB_RENDEROUT_SYSTEM_DESC, pCOutputIObject, &gpuResourceFB_RENDEROUT_SYSTEM_ARCH, NULL);
	if (!pCGpuManager->GetGpuResourceArchive(&gpuResourceFB_DEPTH_SYSTEM_DESC, &gpuResourceFB_DEPTH_SYSTEM_ARCH))
		pCGpuManager->GenerateGpuResource(&gpuResourceFB_DEPTH_SYSTEM_DESC, pCOutputIObject, &gpuResourceFB_DEPTH_SYSTEM_ARCH, NULL);

	SVXFrameBuffer* pstFrameBuffer = (SVXFrameBuffer*)pCOutputIObject->GetFrameBuffer(vxrFrameBufferUsageRENDEROUT, iLoopMode + 1);
	vxbyte4* py4Buffer = (vxbyte4*)pstFrameBuffer->pvBuffer;

	D3D11_MAPPED_SUBRESOURCE mappedResSys;
	pdx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gpuResourceFB_RENDEROUT_SYSTEM_ARCH.vtrPtrs.at(0), (ID3D11Texture2D*)gpuResourcesFB_ARCHs[__FB_TX2D_RENDEROUT].vtrPtrs.at(0));
	pdx11DeviceImmContext->Map((ID3D11Texture2D*)gpuResourceFB_RENDEROUT_SYSTEM_ARCH.vtrPtrs.at(0), 0, D3D11_MAP_READ, NULL, &mappedResSys);

	vxbyte4* py4RenderOuts = (vxbyte4*)mappedResSys.pData;
	for (int i = 0; i < i2SizeScreenCurrent.y; i++)
	{
		for (int j = 0; j < i2SizeScreenCurrent.x; j++)
		{
			// __PS_MERGE_LAYERS_TO_RENDEROUT 에서 INT -> FLOAT4 로 되어 배열된 color 요소가 들어 옴. //
			vxbyte4 y4Renderout = py4RenderOuts[j];
			vxbyte4 y4RenderoutCorrection;
			y4RenderoutCorrection.x = y4Renderout.z;	// B
			y4RenderoutCorrection.y = y4Renderout.y;	// G
			y4RenderoutCorrection.z = y4Renderout.x;	// R
			y4RenderoutCorrection.w = y4Renderout.w;	// A
			py4Buffer[i*i2SizeScreenCurrent.x + j] = y4RenderoutCorrection;
		}
		py4RenderOuts += mappedResSys.RowPitch / 4;	// mappedResSys.RowPitch (byte unit)
	}

	pdx11DeviceImmContext->Unmap((ID3D11Texture2D*)gpuResourceFB_RENDEROUT_SYSTEM_ARCH.vtrPtrs.at(0), 0);

	if (iLoopMode == __PLANE_RENDER_MODE_3D)
	{
		SVXFrameBuffer* psvxFrameBufferDepth = (SVXFrameBuffer*)pCOutputIObject->GetFrameBuffer(vxrFrameBufferUsageDEPTH, 0);
		float* pfBuffer = (float*)psvxFrameBufferDepth->pvBuffer;
		pdx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gpuResourceFB_DEPTH_SYSTEM_ARCH.vtrPtrs.at(0), (ID3D11Texture2D*)gpuResourcesFB_ARCHs[__FB_TX2D_DEPTH_ZTHICKCS].vtrPtrs.at(0));
		pdx11DeviceImmContext->Map((ID3D11Texture2D*)gpuResourceFB_DEPTH_SYSTEM_ARCH.vtrPtrs.at(0), 0, D3D11_MAP_READ, NULL, &mappedResSys);
		float* pfDeferredContexts = (float*)mappedResSys.pData;
		for (int i = 0; i < i2SizeScreenCurrent.y; i++)
		{
			for (int j = 0; j < i2SizeScreenCurrent.x; j++)
			{
				if (py4Buffer[i*i2SizeScreenCurrent.x + j].w > 0)
					pfBuffer[i*i2SizeScreenCurrent.x + j] = pfDeferredContexts[j];
				else
					pfBuffer[i*i2SizeScreenCurrent.x + j] = FLT_MAX;	// CS Depth 를 사용하는 쪽으로...
			}
			pfDeferredContexts += mappedResSys.RowPitch / 4;	// mappedResSys.RowPitch (byte unit)
		}
		pdx11DeviceImmContext->Unmap((ID3D11Texture2D*)gpuResourceFB_DEPTH_SYSTEM_ARCH.vtrPtrs.at(0), 0);
	}
#endif
#pragma endregion

	if (iLoopMode == __PLANE_RENDER_MODE_FILLED)
	{
		iLoopMode = __PLANE_RENDER_MODE_3D;
		goto RENDER_LOOP;
	}

	// Mix //
	SVXFrameBuffer* pstFrameBufferOut = (SVXFrameBuffer*)pCOutputIObject->GetFrameBuffer(vxrFrameBufferUsageRENDEROUT, 0);
	vxbyte4* py4BufferOut = (vxbyte4*)pstFrameBufferOut->pvBuffer;
	SVXFrameBuffer* pstFrameBuffer_FilledPlane = (SVXFrameBuffer*)pCOutputIObject->GetFrameBuffer(vxrFrameBufferUsageRENDEROUT, 1);
	vxbyte4* py4Buffer_FilledPlane = (vxbyte4*)pstFrameBuffer_FilledPlane->pvBuffer;
	SVXFrameBuffer* pstFrameBuffer_3DonPlane = (SVXFrameBuffer*)pCOutputIObject->GetFrameBuffer(vxrFrameBufferUsageRENDEROUT, 2);
	vxbyte4* py4Buffer_3DonPlane = (vxbyte4*)pstFrameBuffer_3DonPlane->pvBuffer;

	SVXFrameBuffer* pstFrameBufferDepthOut = (SVXFrameBuffer*)pCOutputIObject->GetFrameBuffer(vxrFrameBufferUsageDEPTH, 0);
	float* pfBufferDepthOut = (float*)pstFrameBufferDepthOut->pvBuffer;

	for (int i = 0; i < i2SizeScreenCurrent.y; i++)
	{
		for (int j = 0; j < i2SizeScreenCurrent.x; j++)
		{
			int iAddr = i*i2SizeScreenCurrent.x + j;
			vxbyte4 y4Color_FilledPlane = py4Buffer_FilledPlane[iAddr];	// Filled Plane
			vxbyte4 y4Color_3DonPlane = py4Buffer_3DonPlane[iAddr];

			float fDepth_FilledPlane = FLT_MAX;
			if (y4Color_FilledPlane.w > 0)
				fDepth_FilledPlane = 0;
			float fDepth_3DonPlane = pfBufferDepthOut[iAddr];
			vxbyte4 y4ColorMix;
			float fDepthOut = FLT_MAX;
			// To Do //
			if (fDepth_FilledPlane > fDepth_3DonPlane)
			{
				y4ColorMix = y4Color_3DonPlane;
				fDepthOut = fDepth_3DonPlane;
			}
			else
			{
				y4ColorMix = y4Color_FilledPlane;
				fDepthOut = fDepth_FilledPlane;
			}

			py4BufferOut[iAddr] = y4ColorMix;
			pfBufferDepthOut[iAddr] = fDepthOut;
		}
	}
ENDRENDER:
	pdx11DeviceImmContext->ClearState();

	ID3D11RenderTargetView* pdxRTVOlds[2];
	pdxRTVOlds[0] = pdxRTVOld;
	pdxRTVOlds[1] = NULL;
	pdx11DeviceImmContext->OMSetRenderTargets(2, pdxRTVOlds, pdxDSVOld);

	VXSAFE_RELEASE(pdxRTVOld);
	VXSAFE_RELEASE(pdxDSVOld);

	QueryPerformanceFrequency(&lIntFreq);
	QueryPerformanceCounter(&lIntCntEnd);

	pCOutputIObject->SetDefineModuleSpecifier(_T("VS-GPUDX11VxRenderer module"));

	// Time Check
	double dRunTime = (lIntCntEnd.QuadPart - lIntCntStart.QuadPart) / (double)(lIntFreq.QuadPart);
	if (pdRunTime)
		*pdRunTime = dRunTime;

	((std::mutex*)HDx11GetMutexGpuCriticalPath())->unlock();
	/**/
	return true;
}