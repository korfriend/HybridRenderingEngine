#include "RendererHeader.h"
//#include "FreeImage.h"

void RegisterVTResource(int iVolumeID, int iTObjectID, CVXLObject* pCLObjectForParamters, CVXGPUManager* pCGpuManager, ID3D11DeviceContext* pdx11DeviceImmContext, bool bInitGen,
	map<int, CVXObject*>& mapAssociatedObjects, map<int, SVXGPUResourceArchive>& mapGPUResSRVs, SVXLocalProgress* psvxLocalProgress)
{
	map<wstring, vector<double>> mapDValueClear;
	auto itrVolume = mapAssociatedObjects.find(iVolumeID);
	if (iVolumeID != 0 && itrVolume != mapAssociatedObjects.end())
	{
		map<wstring, vector<double>>* pmapDValueVolume = NULL;
		pCLObjectForParamters->GetCustomDValue(iVolumeID, (void**)&pmapDValueVolume);
		if (pmapDValueVolume == NULL)
			pmapDValueVolume = &mapDValueClear;

		// GPU Resource Check!! : Volume //
		CVXVObjectVolume* pCVolume = (CVXVObjectVolume*)itrVolume->second;
		SVXGPUResourceArchive svxGpuResourceVolumeTexture, svxGpuResourceVolumeSRV;
		bool bIsImageStack = false;
		pCVolume->GetCustomParameter(_T("_bool_IsImageStack"), &bIsImageStack);
		wstring strVolumeUsageSpecifier;
		if (bIsImageStack)
			strVolumeUsageSpecifier = _T("TEXTURE2DARRAY_IMAGESTACK");
		else
			strVolumeUsageSpecifier = _T("TEXTURE3D_VOLUME_DEFAULT");
		HDx11UploadDefaultVolume(&svxGpuResourceVolumeTexture, &svxGpuResourceVolumeSRV, pCVolume, strVolumeUsageSpecifier, psvxLocalProgress);
		mapGPUResSRVs.insert(pair<int, SVXGPUResourceArchive>(iVolumeID, svxGpuResourceVolumeSRV));
	}
	auto itrTObject = mapAssociatedObjects.find(iTObjectID);
	if (iTObjectID != 0 && itrTObject != mapAssociatedObjects.end())
	{
		map<wstring, vector<double>>* pmapDValueTObject = NULL;
		pCLObjectForParamters->GetCustomDValue(iTObjectID, (void**)&pmapDValueTObject);
		if (pmapDValueTObject == NULL)
			pmapDValueTObject = &mapDValueClear;

		bool bIsTfChanged = false;
		VXHGetCustomValueFromValueContinerMap(pmapDValueTObject, _T("_bool_IsTfChanged"), &bIsTfChanged);

		// TObject which is not 'preintegrated' one
		CVXTObject* pCTObject = (CVXTObject*)itrTObject->second;
		SVXTransferFunctionArchive* psvxTfArchive = pCTObject->GetTransferFunctionArchive();
		SVXGPUResourceDESC svxGpuOtfResourceDesc;
		SVXGPUResourceArchive svxGpuResourceOtfBuffer, svxGpuResourceOtfSRV;
		svxGpuOtfResourceDesc.eGpuSdkType = vxgGpuSdkTypeDX11;
		svxGpuOtfResourceDesc.eResourceType = vxgResourceTypeDX11RESOURCE;
		svxGpuOtfResourceDesc.strUsageSpecifier = _T("BUFFER_TOBJECT_OTFSERIES");
		svxGpuOtfResourceDesc.eDataType = psvxTfArchive->eValueDataType;
		svxGpuOtfResourceDesc.iSizeStride = VXHGetDataTypeSizeByte(svxGpuOtfResourceDesc.eDataType);
		svxGpuOtfResourceDesc.iSourceObjectID = pCTObject->GetObjectID();
		svxGpuOtfResourceDesc.iCustomMisc = 0;
		if (!pCGpuManager->GetGpuResourceArchive(&svxGpuOtfResourceDesc, &svxGpuResourceOtfBuffer))
		{
			pCGpuManager->GenerateGpuResource(&svxGpuOtfResourceDesc, pCTObject, &svxGpuResourceOtfBuffer, NULL);
			bIsTfChanged = true;
		}
		svxGpuOtfResourceDesc.eResourceType = vxgResourceTypeDX11SRV;
		if (!pCGpuManager->GetGpuResourceArchive(&svxGpuOtfResourceDesc, &svxGpuResourceOtfSRV))
		{
			pCGpuManager->GenerateGpuResource(&svxGpuOtfResourceDesc, pCTObject, &svxGpuResourceOtfSRV, NULL);
		}

		if (bIsTfChanged || bInitGen)
		{
			D3D11_MAPPED_SUBRESOURCE d11MappedRes;
			pdx11DeviceImmContext->Map((ID3D11Resource*)svxGpuResourceOtfBuffer.vtrPtrs.at(0), 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes);
			vxfloat4* pf4ColorTF = (vxfloat4*)d11MappedRes.pData;
			memcpy(pf4ColorTF, psvxTfArchive->ppvArchiveTF[0], psvxTfArchive->i3DimSize.x * sizeof(vxfloat4));
			pdx11DeviceImmContext->Unmap((ID3D11Resource*)svxGpuResourceOtfBuffer.vtrPtrs.at(0), 0);
		}
		mapGPUResSRVs.insert(pair<int, SVXGPUResourceArchive>(iTObjectID, svxGpuResourceOtfSRV));
	}
}

bool RenderSrCommon(SVXModuleParameters* pstModuleParameter,
	CVXGPUManager* pCGpuManager,
	GpuDX11CommonParameters* pdx11CommonParams,
	ID3D11InputLayout* pdx11ILs[NUMINPUTLAYOUTS],
	ID3D11VertexShader** ppdx11VSs[NUMSHADERS_SR_VS],
	ID3D11PixelShader** ppdx11PSs[NUMSHADERS_SR_PS],
	ID3D11ComputeShader** ppdx11CS_Merge[NUMSHADERS_MERGE],
	ID3D11Buffer* pdx11Bufs[3], // 0: Proxy, 1:SI_VTX, 2:SI_IDX
	SVXLocalProgress* psvxLocalProgress,
	double* pdRunTime)
{
	LARGE_INTEGER lIntFreq;
	LARGE_INTEGER lIntCntStart, lIntCntEnd;
	
	vector<CVXObject*> vtrInputPrimitives;
	VXHStringGetVXObjectListFromObjectStringMap(&vtrInputPrimitives, &pstModuleParameter->mapVXObjects, vxrObjectTypePRIMITIVE, true);

	vector<CVXObject*> vtrInputVolumes;
	VXHStringGetVXObjectListFromObjectStringMap(&vtrInputVolumes, &pstModuleParameter->mapVXObjects, vxrObjectTypeVOLUME, true);
	vector<CVXObject*> vtrInputTObjects;
	VXHStringGetVXObjectListFromObjectStringMap(&vtrInputTObjects, &pstModuleParameter->mapVXObjects, vxrObjectTypeTRANSFERFUNCTION, true);

	CVXLObject* pCLObjectForParamters = (CVXLObject*)pstModuleParameter->GetVXObject(vxrObjectTypeCUSTOMLIST, true, 0);

	map<int, CVXObject*> mapAssociatedObjects;
	for (int i = 0; i < (int)vtrInputVolumes.size(); i++)
		mapAssociatedObjects.insert(pair<int, CVXObject*>(vtrInputVolumes[i]->GetObjectID(), vtrInputVolumes[i]));
	for (int i = 0; i < (int)vtrInputTObjects.size(); i++)
		mapAssociatedObjects.insert(pair<int, CVXObject*>(vtrInputTObjects[i]->GetObjectID(), vtrInputTObjects[i]));
	
	CVXIObject* pCOutputIObject = (CVXIObject*)pstModuleParameter->GetVXObject(vxrObjectTypeIMAGEPLANE, false, 0);
	CVXIObject* pCMergeIObject = (CVXIObject*)pstModuleParameter->GetVXObject(vxrObjectTypeIMAGEPLANE, false, 1);
	if(pCMergeIObject == NULL || pCOutputIObject == NULL)
	{
		VXERRORMESSAGE("There should be two IObjects!");
		return false;
	}

	((std::mutex*)HDx11GetMutexGpuCriticalPath())->lock();

	bool bIsAntiAliasingRS = false;
	int iNumTexureLayers = NUM_TEXRT_LAYERS;
	bool bIsSystemOut = true;
	bool bIsSupportedCS = true;	// test 나중에 자체 테스트로 돌리게 하는 것도 강구!
	double dVThickness = -1.0;
	int iMaxNumSelfTransparentRenderPass = 2;
	vxdouble4 d4GlobalShadingFactors = vxdouble4(0.4, 0.6, 0.2, 30);	// Emission, Diffusion, Specular, Specular Power

	pstModuleParameter->GetCustomParameter(&iMaxNumSelfTransparentRenderPass, _T("_int_MaxNumSelfTransparentRenderPass"));
	pstModuleParameter->GetCustomParameter(&bIsAntiAliasingRS, _T("_bool_IsAntiAliasingRS"));
	pstModuleParameter->GetCustomParameter(&bIsSystemOut, _T("_bool_IsSystemOut"));
	// 1, 2, ..., NUM_TEXRT_LAYERS : 
	pstModuleParameter->GetCustomParameter(&iNumTexureLayers, _T("_int_NumTexureLayers"));
	iNumTexureLayers = min(iNumTexureLayers, NUM_TEXRT_LAYERS);
	iNumTexureLayers = max(iNumTexureLayers, 1);
	pstModuleParameter->GetCustomParameter(&bIsSupportedCS, _T("_bool_IsSupportedCS"));
	pstModuleParameter->GetCustomParameter(&dVThickness, _T("_double_VZThickness"));
	pstModuleParameter->GetCustomParameter(&d4GlobalShadingFactors, _T("_double4_ShadingFactorsForGlobalPrimitives"));

	// To Do // 나중에 테스트 : 어떤 거가 더 효율적인가? VR CPU -> GPU BUFFER -> GPU MIX -> CPU BUFFER
	if(bIsSystemOut)
		bIsSupportedCS = false;	// GPU VR 은 항상 CS

#pragma region // Shader Setting
	// Shader Re-Compile Setting //
	bool bReloadHLSLObjFiles = false;
	pstModuleParameter->GetCustomParameter(&bReloadHLSLObjFiles, _T("_bool_ReloadHLSLObjFiles"));
	if(bReloadHLSLObjFiles)
	{
		wstring strNames_SR[NUMSHADERS_SR_PS] = {
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
		wstring strNames_MG[NUMSHADERS_MERGE] = {
			_T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\MERGE_SROUT_AND_LAYEROUT_TO_LAYEROUT_cs_4_0")
			, _T("..\\..\\Module Manager\\VS-GPUDX11VxRenderer\\MERGE_LAYEROUT_TO_RENDEROUT_cs_4_0")
		};
		for(int i = 0; i < NUMSHADERS_SR_PS; i++)
		{
			wstring strName = strNames_SR[i];

			FILE* pFile;
			if(_wfopen_s(&pFile, strName.c_str(), _T("rb")) == 0)
			{
				fseek(pFile, 0, SEEK_END);
				ullong ullFileSize = ftell(pFile);
				fseek(pFile, 0, SEEK_SET);
				byte* pyRead = new byte[ullFileSize];
				fread(pyRead, sizeof(byte), ullFileSize, pFile);
				fclose(pFile);

				ID3D11PixelShader* pdx11PShader = NULL;
				if(pdx11CommonParams->pdx11Device->CreatePixelShader(pyRead, ullFileSize, NULL, &pdx11PShader) != S_OK)
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
		for(int i = 0; i < NUMSHADERS_MERGE; i++)
		{
			wstring strName = strNames_MG[i];

			FILE* pFile;
			if(_wfopen_s(&pFile, strName.c_str(), _T("rb")) == 0)
			{
				fseek(pFile, 0, SEEK_END);
				ullong ullFileSize = ftell(pFile);
				fseek(pFile, 0, SEEK_SET);
				byte* pyRead = new byte[ullFileSize];
				fread(pyRead, sizeof(byte), ullFileSize, pFile);
				fclose(pFile);

				ID3D11ComputeShader* pdx11CShader = NULL;
				if(pdx11CommonParams->pdx11Device->CreateComputeShader(pyRead, ullFileSize, NULL, &pdx11CShader) != S_OK)
				{
					VXERRORMESSAGE("SHADER COMPILE FAILURE!")
				}
				else
				{
					VXSAFE_RELEASE(*ppdx11CS_Merge[i]);
					*ppdx11CS_Merge[i] = pdx11CShader;
				}
				VXSAFE_DELETEARRAY(pyRead);
			}
		}
	}

	ID3D11InputLayout* pdx11LayoutInputPos = (ID3D11InputLayout*)pdx11ILs[0];
	ID3D11InputLayout* pdx11LayoutInputPosNor = (ID3D11InputLayout*)pdx11ILs[1];	
	ID3D11InputLayout* pdx11LayoutInputPosTex = (ID3D11InputLayout*)pdx11ILs[2];
	ID3D11InputLayout* pdx11LayoutInputPosNorTex = (ID3D11InputLayout*)pdx11ILs[3];
	ID3D11InputLayout* pdx11LayoutInputPosTTTex = (ID3D11InputLayout*)pdx11ILs[4];

	ID3D11VertexShader* pdx11VShader_Point = (ID3D11VertexShader*)*ppdx11VSs[0];
	ID3D11VertexShader* pdx11VShader_PointNormal = (ID3D11VertexShader*)*ppdx11VSs[1];
	ID3D11VertexShader* pdx11VShader_PointTexture = (ID3D11VertexShader*)*ppdx11VSs[2];
	ID3D11VertexShader* pdx11VShader_PointNormalTexture = (ID3D11VertexShader*)*ppdx11VSs[3];
	ID3D11VertexShader* pdx11VShader_PointTTTextures = (ID3D11VertexShader*)*ppdx11VSs[4];
	ID3D11VertexShader* pdx11VShader_PointNormalTexture_Deviation = (ID3D11VertexShader*)*ppdx11VSs[5];
	ID3D11VertexShader* pdx11VShader_Proxy = (ID3D11VertexShader*)*ppdx11VSs[6];
#pragma endregion // Shader Setting

	ID3D11Device* pdx11Device = pdx11CommonParams->pdx11Device;
	ID3D11DeviceContext* pdx11DeviceImmContext = pdx11CommonParams->pdx11DeviceImmContext;
	SVXGPUResourceDESC gpuResourcesFB_DESCs[__FB_COUNT];
	SVXGPUResourceArchive gpuResourcesFB_ARCHs[__FB_COUNT];
	SVXGPUResourceDESC gpuResourceMERGE_CS_DESCs[__UFB_MERGE_FB_COUNT];
	SVXGPUResourceArchive gpuResourceMERGE_CS_ARCHs[__UFB_MERGE_FB_COUNT];

#pragma region // IOBJECT CPU
	while(pCOutputIObject->GetFrameBuffer(vxrFrameBufferUsageRENDEROUT, 1) != NULL)
		pCOutputIObject->DeleteFrameBuffer(vxrFrameBufferUsageRENDEROUT, 1);
	if(!pCOutputIObject->ReplaceFrameBuffer(vxrDataTypeBYTE4CHANNEL, vxrFrameBufferUsageRENDEROUT, 0, _T("common render out frame buffer : defined in VS-GPUDX11VxRenderer module")))
		pCOutputIObject->InsertFrameBuffer(vxrDataTypeBYTE4CHANNEL, vxrFrameBufferUsageRENDEROUT, _T("common render out frame buffer : defined in VS-GPUDX11VxRenderer module"));

	while(pCOutputIObject->GetFrameBuffer(vxrFrameBufferUsageDEPTH, 1) != NULL)
		pCOutputIObject->DeleteFrameBuffer(vxrFrameBufferUsageDEPTH, 1);
	if(!pCOutputIObject->ReplaceFrameBuffer(vxrDataTypeFLOAT, vxrFrameBufferUsageDEPTH, 0, _T("1st hit screen depth frame buffer : defined in VS-GPUDX11VxRenderer module")))
		pCOutputIObject->InsertFrameBuffer(vxrDataTypeFLOAT, vxrFrameBufferUsageDEPTH, _T("1st hit screen depth frame buffer : defined in VS-GPUDX11VxRenderer module"));

//#define CICERO_TEST
#ifdef CICERO_TEST
	while (pCOutputIObject->GetFrameBuffer(vxrFrameBufferUsageCUSTOM, 1) != NULL)
		pCOutputIObject->DeleteFrameBuffer(vxrFrameBufferUsageCUSTOM, 1);
	if (!pCOutputIObject->ReplaceFrameBuffer(vxrDataTypeFLOAT3CHANNEL, vxrFrameBufferUsageCUSTOM, 0, _T("CICERO FLOAT RENDER OUT : defined in VS-GPUDX11VxRenderer module")))
		pCOutputIObject->InsertFrameBuffer(vxrDataTypeFLOAT3CHANNEL, vxrFrameBufferUsageCUSTOM, _T("CICERO FLOAT RENDER OUT : defined in VS-GPUDX11VxRenderer module"));
#endif

	int iNumCustomBuffers = 7;	// ffor CPU VR...

	while (pCMergeIObject->GetFrameBuffer(vxrFrameBufferUsageCUSTOM, iNumCustomBuffers) != NULL)
		pCMergeIObject->DeleteFrameBuffer(vxrFrameBufferUsageCUSTOM, iNumCustomBuffers);

	if (!pCMergeIObject->ReplaceFrameBuffer(vxrDataTypeSTRUCTURED, vxrFrameBufferUsageCUSTOM, 0, _T("Merge Buffer for Merge Layer : defined in VS-GPUDX11VxRenderer module"), 1))
		pCMergeIObject->InsertFrameBuffer(vxrDataTypeSTRUCTURED, vxrFrameBufferUsageCUSTOM, _T("Merge Buffer for Merge Layer : defined in VS-GPUDX11VxRenderer module"), 1);
#pragma endregion // IOBJECT CPU

#pragma region // IOBJECT GPU
	// GPU Resource Check!! : Frame Buffer //
	for(int i = 0; i < __FB_COUNT; i++)
	{
		gpuResourcesFB_DESCs[i].eGpuSdkType = vxgGpuSdkTypeDX11;
		gpuResourcesFB_DESCs[i].iCustomMisc = 0;
		gpuResourcesFB_DESCs[i].iSourceObjectID = pCMergeIObject->GetObjectID();
	}

	gpuResourcesFB_DESCs[__FB_TX2D_RENDEROUT].eResourceType = vxgResourceTypeDX11RESOURCE;
	gpuResourcesFB_DESCs[__FB_TX2D_RENDEROUT].strUsageSpecifier = _T("TEXTURE2D_IOBJECT_RENDEROUT");
	gpuResourcesFB_DESCs[__FB_TX2D_RENDEROUT].iCustomMisc = NUM_TEXRT_LAYERS;
#ifdef CICERO_TEST
	gpuResourcesFB_DESCs[__FB_TX2D_RENDEROUT].eDataType = vxrDataTypeFLOAT4CHANNEL;
#else
	gpuResourcesFB_DESCs[__FB_TX2D_RENDEROUT].eDataType = vxrDataTypeBYTE4CHANNEL;
#endif
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
	gpuResourcesFB_DESCs[__FB_TX2D_DEPTH_ZTHICKCS].iCustomMisc = NUM_TEXRT_LAYERS;
	gpuResourcesFB_DESCs[__FB_TX2D_DEPTH_ZTHICKCS].eDataType = vxrDataTypeFLOAT;
	gpuResourcesFB_DESCs[__FB_TX2D_DEPTH_ZTHICKCS].iSizeStride = VXHGetDataTypeSizeByte(gpuResourcesFB_DESCs[__FB_TX2D_DEPTH_ZTHICKCS].eDataType);
	gpuResourcesFB_DESCs[__FB_RTV_DEPTH_ZTHICKCS] = gpuResourcesFB_DESCs[__FB_SRV_DEPTH_ZTHICKCS] = gpuResourcesFB_DESCs[__FB_TX2D_DEPTH_ZTHICKCS];
	gpuResourcesFB_DESCs[__FB_RTV_DEPTH_ZTHICKCS].eResourceType = vxgResourceTypeDX11RTV;
	gpuResourcesFB_DESCs[__FB_SRV_DEPTH_ZTHICKCS].eResourceType = vxgResourceTypeDX11SRV;
	gpuResourcesFB_DESCs[__FB_TX2D_DEPTH_DUAL] = gpuResourcesFB_DESCs[__FB_TX2D_DEPTH];
	gpuResourcesFB_DESCs[__FB_TX2D_DEPTH_DUAL].strUsageSpecifier = _T("TEXTURE2D_IOBJECT_RENDEROUT");
	gpuResourcesFB_DESCs[__FB_TX2D_DEPTH_DUAL].eResourceType = vxgResourceTypeDX11RESOURCE;
	gpuResourcesFB_DESCs[__FB_SRV_DEPTH_DUAL] = gpuResourcesFB_DESCs[__FB_RTV_DEPTH_DUAL] = gpuResourcesFB_DESCs[__FB_TX2D_DEPTH_DUAL];
	gpuResourcesFB_DESCs[__FB_SRV_DEPTH_DUAL].eResourceType = vxgResourceTypeDX11SRV;
	gpuResourcesFB_DESCs[__FB_RTV_DEPTH_DUAL].eResourceType = vxgResourceTypeDX11RTV;

	bool bInitGen = false;
	for(int i = 0; i < __FB_COUNT; i++)
	{
		if(!pCGpuManager->GetGpuResourceArchive(&gpuResourcesFB_DESCs[i], &gpuResourcesFB_ARCHs[i]))
		{
			pCGpuManager->GenerateGpuResource(&gpuResourcesFB_DESCs[i], pCMergeIObject, &gpuResourcesFB_ARCHs[i], NULL);
			bInitGen = true;
		}
	}

	// To Store and Merge Output //
	for(int i = 0; i < __UFB_MERGE_FB_COUNT; i++)
	{
		gpuResourceMERGE_CS_DESCs[i].eDataType = vxrDataTypeSTRUCTURED;
		gpuResourceMERGE_CS_DESCs[i].eGpuSdkType = vxgGpuSdkTypeDX11;
		gpuResourceMERGE_CS_DESCs[i].strUsageSpecifier = _T("BUFFER_IOBJECT_RESULTOUT");
		gpuResourceMERGE_CS_DESCs[i].iCustomMisc = __BLOCKSIZE | (2 << 16);	// '2' means two resource buffers
		gpuResourceMERGE_CS_DESCs[i].iSizeStride = sizeof(RWB_Output_MultiLayers);
		gpuResourceMERGE_CS_DESCs[i].iSourceObjectID = pCMergeIObject->GetObjectID();
	}
	gpuResourceMERGE_CS_DESCs[__FB_UBUF_MERGEOUT].eResourceType = vxgResourceTypeDX11RESOURCE;
	gpuResourceMERGE_CS_DESCs[__FB_UAV_MERGEOUT].eResourceType = vxgResourceTypeDX11UAV;
	gpuResourceMERGE_CS_DESCs[__FB_SRV_MERGEOUT].eResourceType = vxgResourceTypeDX11SRV;

	SVXGPUResourceDESC gpuResourceMERGE_PS_DESCs[__EXFB_COUNT];
	SVXGPUResourceArchive gpuResourceMERGE_PS_ARCHs[__EXFB_COUNT];
	for(int i = 0; i < __EXFB_COUNT; i++)
	{
		gpuResourceMERGE_PS_DESCs[i].eGpuSdkType = vxgGpuSdkTypeDX11;
		gpuResourceMERGE_PS_DESCs[i].strUsageSpecifier = _T("TEXTURE2D_IOBJECT_RENDEROUT");
		gpuResourceMERGE_PS_DESCs[i].iCustomMisc = NUM_MERGE_LAYERS * 2;	// two of NUM_MERGE_LAYERS resource buffers
		gpuResourceMERGE_PS_DESCs[i].iSourceObjectID = pCMergeIObject->GetObjectID();
		gpuResourceMERGE_PS_DESCs[i].eDataType = vxrDataTypeFLOAT4CHANNEL;	// RG : Int RGBA // B : DepthBack // A : Thickness
		gpuResourceMERGE_PS_DESCs[i].iSizeStride = VXHGetDataTypeSizeByte(gpuResourceMERGE_PS_DESCs[i].eDataType);
		switch(i)
		{
		case 0:
			gpuResourceMERGE_PS_DESCs[i].eResourceType = vxgResourceTypeDX11RESOURCE;break;
		case 1:
			gpuResourceMERGE_PS_DESCs[i].eResourceType = vxgResourceTypeDX11SRV;break;
		case 2:
			gpuResourceMERGE_PS_DESCs[i].eResourceType = vxgResourceTypeDX11RTV;break;
		}
	}

	if(bIsSupportedCS)
	{
		for(int i = 0; i < __EXFB_COUNT; i++)
			pCGpuManager->ReleaseGpuResource(&gpuResourceMERGE_PS_DESCs[i]);

		for(int i = 0; i < __UFB_MERGE_FB_COUNT; i++)
		{
			if(!pCGpuManager->GetGpuResourceArchive(&gpuResourceMERGE_CS_DESCs[i], &gpuResourceMERGE_CS_ARCHs[i]))
			{
				pCGpuManager->GenerateGpuResource(&gpuResourceMERGE_CS_DESCs[i], pCMergeIObject, &gpuResourceMERGE_CS_ARCHs[i], NULL);
				bInitGen = true;
			}
		}
	}
	else
	{
		for(int i = 0; i < __UFB_MERGE_FB_COUNT; i++)
			pCGpuManager->ReleaseGpuResource(&gpuResourceMERGE_CS_DESCs[i]);

		for(int i = 0; i < __EXFB_COUNT; i++)
		{
			if(!pCGpuManager->GetGpuResourceArchive(&gpuResourceMERGE_PS_DESCs[i], &gpuResourceMERGE_PS_ARCHs[i]))
			{
				pCGpuManager->GenerateGpuResource(&gpuResourceMERGE_PS_DESCs[i], pCMergeIObject, &gpuResourceMERGE_PS_ARCHs[i], NULL);
				bInitGen = true;
			}
		}
	}

	vxint2 i2SizeScreenCurrent, i2SizeScreenOld = vxint2(0, 0);
	pCMergeIObject->GetFrameBufferInfo(&i2SizeScreenCurrent);
	if(bInitGen)
		pCMergeIObject->RegisterCustomParameter(_T("_int2_PreviousScreenSize"), i2SizeScreenCurrent);
	pCMergeIObject->GetCustomParameter(_T("_int2_PreviousScreenSize"), &i2SizeScreenOld);
	bool bIsResized = false;
	if(i2SizeScreenCurrent.x != i2SizeScreenOld.x || i2SizeScreenCurrent.y != i2SizeScreenOld.y)
	{
		bIsResized = true;
		pCGpuManager->ReleaseGpuResourcesRelatedObject(pCMergeIObject->GetObjectID());	// System Out 포함 //

		if(bIsSupportedCS)
		{
			for(int i = 0; i < __UFB_MERGE_FB_COUNT; i++)
				pCGpuManager->GenerateGpuResource(&gpuResourceMERGE_CS_DESCs[i], pCMergeIObject, &gpuResourceMERGE_CS_ARCHs[i], NULL);
		}
		else
		{
			for(int i = 0; i < __EXFB_COUNT; i++)
				pCGpuManager->GenerateGpuResource(&gpuResourceMERGE_PS_DESCs[i], pCMergeIObject, &gpuResourceMERGE_PS_ARCHs[i], NULL);
		}

		for(int i = 0; i < __FB_COUNT; i++)
		{
			pCGpuManager->GenerateGpuResource(&gpuResourcesFB_DESCs[i], pCMergeIObject, &gpuResourcesFB_ARCHs[i], NULL);
		}

		pCMergeIObject->RegisterCustomParameter(_T("_int2_PreviousScreenSize"), i2SizeScreenCurrent);
	}
	pCOutputIObject->RegisterCustomParameter(_T("_int2_PreviousScreenSize"), i2SizeScreenCurrent);
#pragma endregion // IOBJECT GPU

	vector<ID3D11RenderTargetView*> vtrRTV_MergePingpongPS;
	vector<ID3D11ShaderResourceView*> vtrSRV_MergePingpongPS;
	if (!bIsSupportedCS)
	{
		vector<void*>* pvtrRTV = &gpuResourceMERGE_PS_ARCHs[__EXFB_RTV_MERGE_RAYSEGMENT].vtrPtrs;
		vector<void*>* pvtrSRV = &gpuResourceMERGE_PS_ARCHs[__EXFB_SRV_MERGE_RAYSEGMENT].vtrPtrs;
		for (int i = 0; i < NUM_MERGE_LAYERS * 2; i++)
		{
			vtrRTV_MergePingpongPS.push_back((ID3D11RenderTargetView*)pvtrRTV->at(i));
			vtrSRV_MergePingpongPS.push_back((ID3D11ShaderResourceView*)pvtrSRV->at(i));
		}
	}

#pragma region // Presetting of VxObject
	map<wstring, vector<double>> mapDValueClear;
	vector<SVXGPUResourceArchive> vtrGpuResourceArchiveBufferVertex;
	vector<SVXGPUResourceArchive> vtrGpuResourceArchiveBufferIndex;
	vector<CVXVObjectPrimitive*> vtrValidPrimitives;
	map<int, SVXGPUResourceArchive> mapGPUResSRVs;
	map<int, SVXGPUResourceArchive> mapVolumeBlockResSRVs;
	uint uiNumPrimitiveObjects = (uint)vtrInputPrimitives.size();
	float fLengthDiagonalMax = 0;
	for(uint i = 0; i < uiNumPrimitiveObjects; i++)
	{
		CVXVObjectPrimitive* pCVMeshObj = (CVXVObjectPrimitive*)vtrInputPrimitives[i];
		if(pCVMeshObj->IsDefinedResource() == false)
			continue;

		SVXPrimitiveDataArchive* pstMeshArchive = pCVMeshObj->GetPrimitiveArchive();
		vxmatrix matOS2WS = *pCVMeshObj->GetMatrixOS2WS();
			
		vxfloat3 f3PosAaBbMinWS, f3PosAaBbMaxWS;
		VXHMTransformPoint(&f3PosAaBbMinWS, &pstMeshArchive->stAaBbMinMaxOS.f3PosMinBox, &matOS2WS);
		VXHMTransformPoint(&f3PosAaBbMaxWS, &pstMeshArchive->stAaBbMinMaxOS.f3PosMaxBox, &matOS2WS);
		fLengthDiagonalMax = max(fLengthDiagonalMax, VXHMLengthVector(&(f3PosAaBbMinWS - f3PosAaBbMaxWS)));

		bool bIsVisibleItem = pCVMeshObj->GetVisibility();
		if(!bIsVisibleItem)
			continue;

		int iVolumeID = 0, iTObjectID = 0;
		int iMeshObjID = pCVMeshObj->GetObjectID();
		map<wstring, vector<double>>* pmapDValueMesh = NULL;
		pCLObjectForParamters->GetCustomDValue(iMeshObjID, (void**)&pmapDValueMesh);
		if(pmapDValueMesh == NULL)
			pmapDValueMesh = &mapDValueClear;
		VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_int_AssociatedVolumeID"), &iVolumeID);
		VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_int_AssociatedTObjectID"), &iTObjectID);

		RegisterVTResource(iVolumeID, iTObjectID, pCLObjectForParamters, pCGpuManager, pdx11DeviceImmContext, bInitGen,
			mapAssociatedObjects, mapGPUResSRVs, psvxLocalProgress);

		SVXGPUResourceDESC stGpuResourcePrimitiveVertexDESC, stGpuResourcePrimitiveIndexDESC;
		stGpuResourcePrimitiveVertexDESC.eGpuSdkType = vxgGpuSdkTypeDX11;
		stGpuResourcePrimitiveVertexDESC.eDataType = vxrDataTypeFLOAT3CHANNEL;
		stGpuResourcePrimitiveVertexDESC.eResourceType = vxgResourceTypeDX11RESOURCE;
		stGpuResourcePrimitiveVertexDESC.strUsageSpecifier = _T("BUFFER_PRIMITIVE_VERTEX_LIST");
		stGpuResourcePrimitiveVertexDESC.iCustomMisc = 0;
		stGpuResourcePrimitiveVertexDESC.iSourceObjectID = pCVMeshObj->GetObjectID();
		stGpuResourcePrimitiveVertexDESC.iSizeStride = VXHGetDataTypeSizeByte(stGpuResourcePrimitiveVertexDESC.eDataType);
		stGpuResourcePrimitiveIndexDESC = stGpuResourcePrimitiveVertexDESC;
		stGpuResourcePrimitiveIndexDESC.eDataType = vxrDataTypeUNSIGNEDINT;
		stGpuResourcePrimitiveIndexDESC.iSizeStride = VXHGetDataTypeSizeByte(stGpuResourcePrimitiveIndexDESC.eDataType);
		stGpuResourcePrimitiveIndexDESC.strUsageSpecifier = _T("BUFFER_PRIMITIVE_INDEX_LIST");

		SVXGPUResourceArchive stGpuResourceBufferVertex, stGpuResourceBufferIndex;
		if(!pCGpuManager->GetGpuResourceArchive(&stGpuResourcePrimitiveVertexDESC, &stGpuResourceBufferVertex))
		{
			if(!pCGpuManager->GenerateGpuResource(&stGpuResourcePrimitiveVertexDESC, pCVMeshObj, &stGpuResourceBufferVertex, NULL))
				return false;
		}
		vtrGpuResourceArchiveBufferVertex.push_back(stGpuResourceBufferVertex);

		if (pstMeshArchive->puiIndexList != NULL)
		{
			if(stGpuResourceBufferVertex.vtrPtrs.at(0) == NULL)
				return false;

			if(!pCGpuManager->GetGpuResourceArchive(&stGpuResourcePrimitiveIndexDESC, &stGpuResourceBufferIndex))
			{
				if(!pCGpuManager->GenerateGpuResource(&stGpuResourcePrimitiveIndexDESC, pCVMeshObj, &stGpuResourceBufferIndex, NULL))
					return false;
			}
		}
		vtrGpuResourceArchiveBufferIndex.push_back(stGpuResourceBufferIndex);
		vtrValidPrimitives.push_back(pCVMeshObj);
	}
#pragma endregion // Presetting of VxObject
	
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
	
#pragma region // Camera & Light Setting
	CVXCObject* pCCObject = pCMergeIObject->GetCameraObject();
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
	cbSrCamState.fVThicknessGlobal = (float)dVThickness;

	// CB Set MERGE //
	if (bIsSupportedCS)
	{
		D3D11_MAPPED_SUBRESOURCE mappedVrCamState;
		pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_CAMSTATE], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedVrCamState);
		CB_VrCameraState* pCBParamCamState = (CB_VrCameraState*)mappedVrCamState.pData;
		memcpy(pCBParamCamState, &cbVrCamState, sizeof(CB_VrCameraState));
		pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_CAMSTATE], 0);
		pdx11DeviceImmContext->CSSetConstantBuffers(11, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_CAMSTATE]);
	}

	D3D11_MAPPED_SUBRESOURCE mappedSrCamState;
	pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_CAMSTATE], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSrCamState);
	CB_SrProjectionCameraState* pCBCamStateParam = (CB_SrProjectionCameraState*)mappedSrCamState.pData;
	memcpy(pCBCamStateParam, &cbSrCamState, sizeof(CB_SrProjectionCameraState));
	pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_CAMSTATE], 0);
	pdx11DeviceImmContext->VSSetConstantBuffers(1, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_CAMSTATE]);
	pdx11DeviceImmContext->PSSetConstantBuffers(1, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_CAMSTATE]);
	if(bIsSupportedCS)
		pdx11DeviceImmContext->CSSetConstantBuffers(1, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_CAMSTATE]);

	// View Port Setting //
	D3D11_VIEWPORT dx11ViewPort;
	dx11ViewPort.Width = (float)i2SizeScreenCurrent.x;
	dx11ViewPort.Height = (float)i2SizeScreenCurrent.y;
	dx11ViewPort.MinDepth = 0;
	dx11ViewPort.MaxDepth = 1.0f;
	dx11ViewPort.TopLeftX = 0;
	dx11ViewPort.TopLeftY = 0;
	pdx11DeviceImmContext->RSSetViewports(1, &dx11ViewPort);

	// View List-Up //
	ID3D11ShaderResourceView* pdx11SRV_RenderOuts_NULL[NUM_TEXRT_LAYERS] = { NULL };
	ID3D11ShaderResourceView* pdx11SRV_DepthCSs_NULL[NUM_TEXRT_LAYERS] = { NULL };
	ID3D11ShaderResourceView* pdx11SRV_RenderOuts[NUM_TEXRT_LAYERS] = { NULL };
	ID3D11ShaderResourceView* pdx11SRV_DepthCSs[NUM_TEXRT_LAYERS] = { NULL };
	for(int i = 0; i < NUM_TEXRT_LAYERS; i++)
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
#pragma endregion // Camera & Light Setting

#pragma region // FrameBuffer Setting
	uint uiClearVlaues[4] = {0};
	float fClearColor[4];
	fClearColor[0] = 0; fClearColor[1] = 0; fClearColor[2] = 0; fClearColor[3] = 0;
	if(bIsSupportedCS)
	{
		pdx11DeviceImmContext->CSSetConstantBuffers(11, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_CAMSTATE]);
		pdx11DeviceImmContext->CSSetShader( *ppdx11CS_Merge[__CS_MERGE_SROUT_AND_LAYEROUT_TO_LAYEROUT], NULL, 0 );
		// Clear Merge Buffer //
		pdx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gpuResourceMERGE_CS_ARCHs[__FB_UAV_MERGEOUT].vtrPtrs.at(0), uiClearVlaues);
		pdx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gpuResourceMERGE_CS_ARCHs[__FB_UAV_MERGEOUT].vtrPtrs.at(1), uiClearVlaues);
	}
	else
	{
		// Clear Merge Buffer //
		for(int i = 0; i < NUM_MERGE_LAYERS * 2; i++)
			pdx11DeviceImmContext->ClearRenderTargetView(vtrRTV_MergePingpongPS.at(i), fClearColor);
	}

	// Grid Size Computation //
	uint uiNumGridX = (uint)ceil(i2SizeScreenCurrent.x/(float)__BLOCKSIZE);
	uint uiNumGridY = (uint)ceil(i2SizeScreenCurrent.y/(float)__BLOCKSIZE);

	// Backup Previous Render Target //
	ID3D11RenderTargetView* pdxRTVOld = NULL;
	ID3D11DepthStencilView* pdxDSVOld = NULL;
	pdx11DeviceImmContext->OMGetRenderTargets(1, &pdxRTVOld, &pdxDSVOld);

	ID3D11RenderTargetView* pdx11RTVs_NULL[2] = {NULL};
	ID3D11RenderTargetView* pdx11RTVs[2] = {(ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_RENDEROUT].vtrPtrs.at(0)
		, (ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_DEPTH_ZTHICKCS].vtrPtrs.at(0)};
	pdx11DeviceImmContext->OMSetRenderTargets(2, pdx11RTVs, (ID3D11DepthStencilView*)gpuResourcesFB_ARCHs[__FB_DSV_DEPTH].vtrPtrs.at(0));

	// Clear //
	for(int i = 0; i < NUM_TEXRT_LAYERS; i++)
	{
		pdx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_RENDEROUT].vtrPtrs.at(i), fClearColor);
		pdx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_DEPTH_ZTHICKCS].vtrPtrs.at(i), fClearColor);
	}
	// Dual Depth //
	pdx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_DEPTH_DUAL].vtrPtrs.at(0), fClearColor);
	pdx11DeviceImmContext->PSSetShaderResources(4, 1, (ID3D11ShaderResourceView**)&gpuResourcesFB_ARCHs[__FB_SRV_DEPTH_DUAL].vtrPtrs.at(0));

	pdx11DeviceImmContext->ClearDepthStencilView((ID3D11DepthStencilView*)gpuResourcesFB_ARCHs[__FB_DSV_DEPTH].vtrPtrs.at(0), D3D11_CLEAR_DEPTH, 1.0f, 0);

	pdx11DeviceImmContext->OMSetDepthStencilState( pdx11CommonParams->pdx11DSStates[__DSState_DEPTHENABLE_LESSEQUAL], 0 );

#pragma endregion // FrameBuffer Setting

#pragma region // Other Presetting For Shaders

	pdx11DeviceImmContext->VSSetSamplers(0, 1, &pdx11CommonParams->pdx11SStates[__SState_LINEAR_ZEROBORDER]);
	pdx11DeviceImmContext->VSSetSamplers(1, 1, &pdx11CommonParams->pdx11SStates[__SState_POINT_ZEROBORDER]);
	pdx11DeviceImmContext->PSSetSamplers(0, 1, &pdx11CommonParams->pdx11SStates[__SState_LINEAR_ZEROBORDER]);
	pdx11DeviceImmContext->PSSetSamplers(1, 1, &pdx11CommonParams->pdx11SStates[__SState_POINT_ZEROBORDER]);

	if(bIsSupportedCS)
	{
		pdx11DeviceImmContext->CSSetSamplers(0, 1, &pdx11CommonParams->pdx11SStates[__SState_LINEAR_ZEROBORDER]);
		pdx11DeviceImmContext->CSSetSamplers(1, 1, &pdx11CommonParams->pdx11SStates[__SState_POINT_ZEROBORDER]);
	}

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

#pragma endregion // Other Presetting For Shaders

	QueryPerformanceCounter(&lIntCntStart);

	// For Each Primitive //
	int iCountMerging = 0;	// Merging 불린 횟 수
	int iCountDrawing = 0;	// Draw 불린 횟 수
	int iCountRTBuffers = 0;	// Render Target Index 가 바뀌는 횟 수

	for(uint iIndexMeshObj = 0; iIndexMeshObj < (int)vtrValidPrimitives.size(); iIndexMeshObj++)
	{
		CVXVObjectPrimitive* pCPrimitiveObject = vtrValidPrimitives.at(iIndexMeshObj);
		SVXPrimitiveDataArchive* pstPrimitiveArchive = (SVXPrimitiveDataArchive*)pCPrimitiveObject->GetPrimitiveArchive();
		if(pstPrimitiveArchive->GetVerticeDefinition(vxrVertexTypePOSITION) == NULL)
			continue;

		int iMeshObjID = pCPrimitiveObject->GetObjectID();
		map<wstring, vector<double>>* pmapDValueMesh = NULL;
		pCLObjectForParamters->GetCustomDValue(iMeshObjID, (void**)&pmapDValueMesh);
		if(pmapDValueMesh == NULL)
			pmapDValueMesh = &mapDValueClear;

		// Object Unit Conditions
		bool bIsForcedCullOff = false;
		bool bIsObjectCMM = false;
		bool bIsAirwayAnalysis = false;
		bool bIsDashedLine = false;
		bool bIsInvertColorDashedLine = false;
		bool bIsInvertPlaneDirection = false;
		int iVolumeID = 0, iTObjectID = 0;
		double dLineDashedInterval = 1.0;
		vxdouble4 d4ShadingFactors = d4GlobalShadingFactors;	// Emission, Diffusion, Specular, Specular Power
		vxmatrix matClipBoxWS2BS;
		vxdouble3 d3PosOrthoMaxClipBoxWS;
		vxdouble3 d3PosClipPlaneWS;
		vxdouble3 d3VecClipPlaneWS;
		int iObjectNumSelfTransparentRenderPass = iMaxNumSelfTransparentRenderPass;
		int iClippingMode = 0; // CLIPBOX / CLIPPLANE / BOTH //
		pCPrimitiveObject->GetCustomParameter(_T("_bool_IsForcedCullOff"), &bIsForcedCullOff);
		pCPrimitiveObject->GetCustomParameter(_T("_bool_IsObjectCMM"), &bIsObjectCMM);
		pCPrimitiveObject->GetCustomParameter(_T("_bool_IsDashed"), &bIsDashedLine);
		pCPrimitiveObject->GetCustomParameter(_T("_bool_IsInvertColorDashLine"), &bIsInvertColorDashedLine);
		pCPrimitiveObject->GetCustomParameter(_T("_bool_IsInvertPlaneDirection"), &bIsInvertPlaneDirection);

#define __RENDERER_WITHOUT_VOLUME 0
#define __RENDERER_VOLUME_SAMPLE_AND_MAP 1
#define __RENDERER_DIFFERENCE_DIVIATION_FROM_VOLUME 2
#define __RENDERER_IMAGE_STACK_MAP_PER_MESHSPRITE 3

		int iRendererMode = __RENDERER_WITHOUT_VOLUME;
		VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_int_RendererMode"), &iRendererMode);
		VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_int_AssociatedVolumeID"), &iVolumeID);
		VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_int_AssociatedTObjectID"), &iTObjectID);
		VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_double_LineDashInterval"), &dLineDashedInterval);
		VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_double4_ShadingFactors"), &d4ShadingFactors);
		VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_int_ClippingMode"), &iClippingMode);	// 0 : No, 1 : CLIPBOX, 2 : CLIPPLANE, 3 : BOTH
		VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_double3_PosClipPlaneWS"), &d3PosClipPlaneWS);
		VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_double3_VecClipPlaneWS"), &d3VecClipPlaneWS);
		VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_double3_PosClipBoxMaxWS"), &d3PosOrthoMaxClipBoxWS);
		VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_matrix44_MatrixClipWS2BS"), &matClipBoxWS2BS);
		VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_int_NumSelfTransparentRenderPass"), &iObjectNumSelfTransparentRenderPass);
		VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_bool_IsAirwayAnalysis"), &bIsAirwayAnalysis);
		//iObjectNumSelfTransparentRenderPass = 4;//

		map<wstring, vector<double>>* pmapDValueVolume = NULL;
		pCLObjectForParamters->GetCustomDValue(iVolumeID, (void**)&pmapDValueVolume);
		if(pmapDValueVolume == NULL)
			pmapDValueVolume = &mapDValueClear;

		auto itrGpuVolume = mapGPUResSRVs.find(iVolumeID);
		auto itrGpuTObject = mapGPUResSRVs.find(iTObjectID);
		if(itrGpuVolume == mapGPUResSRVs.end() || itrGpuTObject == mapGPUResSRVs.end())
		{
			iRendererMode = __RENDERER_WITHOUT_VOLUME;
		}
		if (iRendererMode != __RENDERER_WITHOUT_VOLUME)
		{
#pragma region // iRendererMode != __RENDERER_DEFAULT
			pdx11DeviceImmContext->VSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&itrGpuTObject->second.vtrPtrs.at(0));
			pdx11DeviceImmContext->PSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&itrGpuTObject->second.vtrPtrs.at(0));

			if (iRendererMode == __RENDERER_IMAGE_STACK_MAP_PER_MESHSPRITE)
			{
				pdx11DeviceImmContext->VSSetShaderResources(10, 1, (ID3D11ShaderResourceView**)&itrGpuVolume->second.vtrPtrs.at(0));
				pdx11DeviceImmContext->PSSetShaderResources(10, 1, (ID3D11ShaderResourceView**)&itrGpuVolume->second.vtrPtrs.at(0));
			}
			else
			{
				pdx11DeviceImmContext->VSSetShaderResources(1, 1, (ID3D11ShaderResourceView**)&itrGpuVolume->second.vtrPtrs.at(0));
				pdx11DeviceImmContext->PSSetShaderResources(1, 1, (ID3D11ShaderResourceView**)&itrGpuVolume->second.vtrPtrs.at(0));
			}

			if(bIsSupportedCS)
			{
				pdx11DeviceImmContext->CSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&itrGpuTObject->second.vtrPtrs.at(0));
				if (iRendererMode == __RENDERER_IMAGE_STACK_MAP_PER_MESHSPRITE)
					pdx11DeviceImmContext->CSSetShaderResources(10, 1, (ID3D11ShaderResourceView**)&itrGpuVolume->second.vtrPtrs.at(0));
				else
					pdx11DeviceImmContext->CSSetShaderResources(1, 1, (ID3D11ShaderResourceView**)&itrGpuVolume->second.vtrPtrs.at(0));
			}

			auto itrVolume = mapAssociatedObjects.find(iVolumeID);
			auto itrTObject = mapAssociatedObjects.find(iTObjectID);
			CVXVObjectVolume* pCVolume = (CVXVObjectVolume*)itrVolume->second;
			CVXTObject* pCTObject = (CVXTObject*)itrTObject->second;

			bool bIsDownSample = false;
			if(itrGpuVolume->second.f3SampleIntervalElements.x > 1.f ||
				itrGpuVolume->second.f3SampleIntervalElements.y > 1.f ||
				itrGpuVolume->second.f3SampleIntervalElements.z > 1.f)
				bIsDownSample = true;
			CB_VrVolumeObject cbVrVolumeObj;
			HDx11ComputeConstantBufferVrObject(&cbVrVolumeObj, bIsDownSample, pCVolume, pCCObject, itrGpuVolume->second.i3SizeLogicalResourceInBytes, pmapDValueVolume);
			D3D11_MAPPED_SUBRESOURCE mappedResVolObj;
			pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_VOLOBJ], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResVolObj);
			CB_VrVolumeObject* pCBParamVolumeObj = (CB_VrVolumeObject*)mappedResVolObj.pData;
			memcpy(pCBParamVolumeObj, &cbVrVolumeObj, sizeof(CB_VrVolumeObject));
			pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_VOLOBJ], 0);
			pdx11DeviceImmContext->PSSetConstantBuffers(2, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_VOLOBJ]);

			CB_VrOtf1D cbVrOtf1D;
			HDx11ComputeConstantBufferVrOtf1D(&cbVrOtf1D, pCTObject, 1, &pstModuleParameter->mapCustomParamters);
			D3D11_MAPPED_SUBRESOURCE mappedResOtf1D;
			pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_OTF1D], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResOtf1D);
			CB_VrOtf1D* pCBParamOtf1D = (CB_VrOtf1D*)mappedResOtf1D.pData;
			memcpy(pCBParamOtf1D, &cbVrOtf1D, sizeof(CB_VrOtf1D));
			pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_OTF1D], 0);
			pdx11DeviceImmContext->PSSetConstantBuffers(7, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_OTF1D]);

			if (iRendererMode == __RENDERER_DIFFERENCE_DIVIATION_FROM_VOLUME)
			{
				int iTObjectForIsoValueID = 0;
				VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_int_IsoValueTObjectID"), &iTObjectForIsoValueID);

				auto itrIsoValueTObject = mapAssociatedObjects.find(iTObjectForIsoValueID);
				CVXTObject* pCTObjectIsoValue = (CVXTObject*)itrIsoValueTObject->second;
				int iIsoValue = pCTObjectIsoValue->GetTransferFunctionArchive()->i3ValidMinIndex.x;

				vxdouble2 d2DeviationValueMinMax(0, 1);
				VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_double_DeviationMinValue"), &d2DeviationValueMinMax.x);
				VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_double_DeviationMaxValue"), &d2DeviationValueMinMax.y);
				int iMappingValueRange = 512;
				VXHGetCustomValueFromValueContinerMap(pmapDValueMesh, _T("_int_MappingValueRange"), &iMappingValueRange);

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

				SVXGPUResourceArchive svxGpuResourceBlockTexture, svxGpuResourceBlockSRV;
				int iLevelBlock = 1;
				SVXVolumeBlock* psvxVolBlock = pCVolume->GetVolumeBlock(iLevelBlock);
				SVXGPUResourceDESC svxGpuResBlockDesc;
				svxGpuResBlockDesc.eGpuSdkType = vxgGpuSdkTypeDX11;
				svxGpuResBlockDesc.eResourceType = vxgResourceTypeDX11RESOURCE;
				svxGpuResBlockDesc.strUsageSpecifier = _T("TEXTURE3D_VOLUME_BLOCKMAP");
				if(psvxVolBlock->i3BlocksNumber.x * psvxVolBlock->i3BlocksNumber.y * psvxVolBlock->i3BlocksNumber.z < 65536)
					svxGpuResBlockDesc.eDataType = vxrDataTypeUNSIGNEDSHORT;
				else
					svxGpuResBlockDesc.eDataType = vxrDataTypeFLOAT;
				svxGpuResBlockDesc.iSizeStride = VXHGetDataTypeSizeByte(svxGpuResBlockDesc.eDataType);
				svxGpuResBlockDesc.iSourceObjectID = pCVolume->GetObjectID();
				svxGpuResBlockDesc.iRelatedObjectID = pCTObject->GetObjectID();
				svxGpuResBlockDesc.iCustomMisc = iLevelBlock;
				if(!pCGpuManager->GetGpuResourceArchive(&svxGpuResBlockDesc, &svxGpuResourceBlockTexture))
					pCGpuManager->GenerateGpuResource(&svxGpuResBlockDesc, pCVolume, &svxGpuResourceBlockTexture, NULL);

				svxGpuResBlockDesc.eResourceType = vxgResourceTypeDX11SRV;
				if(!pCGpuManager->GetGpuResourceArchive(&svxGpuResBlockDesc, &svxGpuResourceBlockSRV))
					pCGpuManager->GenerateGpuResource(&svxGpuResBlockDesc, pCVolume, &svxGpuResourceBlockSRV, NULL);

				pdx11DeviceImmContext->PSSetShaderResources(2, 1, (ID3D11ShaderResourceView**)&svxGpuResourceBlockSRV.vtrPtrs.at(0));

				CB_VrBlockObject cbVrBlock;
				if(svxGpuResBlockDesc.eDataType == vxrDataTypeFLOAT)
					HDx11ComputeConstantBufferVrBlockObj(&cbVrBlock, pCVolume, iLevelBlock, 1.f);
				else
					HDx11ComputeConstantBufferVrBlockObj(&cbVrBlock, pCVolume, iLevelBlock, 65535.f);
				D3D11_MAPPED_SUBRESOURCE mappedResBlock;
				pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_BLOCKOBJ], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResBlock);
				CB_VrBlockObject* pCBParamBlock = (CB_VrBlockObject*)mappedResBlock.pData;
				memcpy(pCBParamBlock, &cbVrBlock, sizeof(CB_VrBlockObject));
				pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_BLOCKOBJ], 0);
				pdx11DeviceImmContext->PSSetConstantBuffers(3, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_BLOCKOBJ]);
			}
#pragma endregion
		}
		else if(bIsObjectCMM)
		{
			if(pstPrimitiveArchive->pvTextureResource)
			{
				// GPU Resource Check!! : Text Resource //
				SVXGPUResourceArchive stGpuResourceTextTexture, stGpuResourceTextSRV;
				SVXGPUResourceDESC stGpuTextResourceDesc;
				stGpuTextResourceDesc.eGpuSdkType = vxgGpuSdkTypeDX11;
				stGpuTextResourceDesc.eDataType = vxrDataTypeBYTE4CHANNEL;
				stGpuTextResourceDesc.iSizeStride = VXHGetDataTypeSizeByte(stGpuTextResourceDesc.eDataType);
				stGpuTextResourceDesc.iSourceObjectID = pCPrimitiveObject->GetObjectID();
				stGpuTextResourceDesc.strUsageSpecifier = _T("TEXTURE2D_CMM_TEXT");
				stGpuTextResourceDesc.eResourceType = vxgResourceTypeDX11RESOURCE;
				if(!pCGpuManager->GetGpuResourceArchive(&stGpuTextResourceDesc, &stGpuResourceTextTexture))
					pCGpuManager->GenerateGpuResource(&stGpuTextResourceDesc, pCPrimitiveObject, &stGpuResourceTextTexture, NULL);
				stGpuTextResourceDesc.eResourceType = vxgResourceTypeDX11SRV;
				if(!pCGpuManager->GetGpuResourceArchive(&stGpuTextResourceDesc, &stGpuResourceTextSRV))
					pCGpuManager->GenerateGpuResource(&stGpuTextResourceDesc, pCPrimitiveObject, &stGpuResourceTextSRV, NULL);
				pdx11DeviceImmContext->PSSetShaderResources(3, 1, (ID3D11ShaderResourceView**)&stGpuResourceTextSRV.vtrPtrs.at(0));
			}
		}
		else if (bIsAirwayAnalysis)
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

		CB_SrPolygonObject cbPolygonObj;
		vxmatrix matOS2WS = *pCPrimitiveObject->GetMatrixOS2WS();
#pragma region // Constant Buffers for Each Mesh Object //
		cbPolygonObj.matOS2WS = *(D3DXMATRIX*)&matOS2WS;
		D3DXMatrixTranspose(&cbPolygonObj.matOS2WS, &cbPolygonObj.matOS2WS);

		cbPolygonObj.iFlag = iClippingMode;
		vxfloat3 f3ClipBoxMaxBS;
		VXHMTransformPoint(&f3ClipBoxMaxBS, &vxfloat3(d3PosOrthoMaxClipBoxWS), &matClipBoxWS2BS);
		cbPolygonObj.f3ClipBoxMaxBS = *(D3DXVECTOR3*)&f3ClipBoxMaxBS;
		cbPolygonObj.matClipBoxWS2BS = *(D3DXMATRIX*)&vxmatrix(matClipBoxWS2BS);
		D3DXMatrixTranspose(&cbPolygonObj.matClipBoxWS2BS, &cbPolygonObj.matClipBoxWS2BS);

		if (bIsObjectCMM && pstPrimitiveArchive->pvTextureResource)
		{
			vxint3 i3TextureWHN;
			pCPrimitiveObject->GetCustomParameter(_T("_int3_TextureWHN"), &i3TextureWHN);
			cbPolygonObj.iDummy__0 = i3TextureWHN.z;
			if (i3TextureWHN.z == 1)
			{
				vxfloat3* pf3Positions = pstPrimitiveArchive->GetVerticeDefinition(vxrVertexTypePOSITION);
				vxfloat3 f3Pos0SS, f3Pos1SS, f3Pos2SS;
				vxmatrix matOS2SS = matOS2WS * matWS2SS;

				vxfloat3 f3VecWidth = pf3Positions[1] - pf3Positions[0];
				vxfloat3 f3VecHeight = pf3Positions[2] - pf3Positions[0];
				VXHMTransformVector(&f3VecWidth, &f3VecWidth, &matOS2SS);
				VXHMTransformVector(&f3VecHeight, &f3VecHeight, &matOS2SS);
				VXHMNormalizeVector(&f3VecWidth, &f3VecWidth);
				VXHMNormalizeVector(&f3VecHeight, &f3VecHeight);

				//vxfloat3 f3VecWidth0 = pf3Positions[1] - pf3Positions[0];
				//vxfloat3 f3VecHeight0 = -pf3Positions[2] + pf3Positions[0];
				//vxmatrix matOS2PS = matOS2WS * matWS2CS * matCS2PS;
				//VXHMTransformVector(&f3VecWidth0, &f3VecWidth0, &matOS2PS);
				//VXHMTransformVector(&f3VecHeight0, &f3VecHeight0, &matOS2PS);
				//VXHMNormalizeVector(&f3VecWidth0, &f3VecWidth0);
				//VXHMNormalizeVector(&f3VecHeight0, &f3VecHeight0);

				if (VXHMDotVector(&f3VecWidth, &vxfloat3(1.f, 0, 0)) < 0)
					cbPolygonObj.iFlag |= (0x1 << 9);

				vxfloat3 f3VecNormal;
				VXHMCrossDotVector(&f3VecNormal, &f3VecHeight, &f3VecWidth);
				VXHMCrossDotVector(&f3VecHeight, &f3VecWidth, &f3VecNormal);

				if (VXHMDotVector(&f3VecHeight, &vxfloat3(0, 1.f, 0)) <= 0)
					cbPolygonObj.iFlag |= (0x1 << 10);
			}
		}

		if (bIsDashedLine)
			cbPolygonObj.iFlag |= (0x1 << 19);
		if (bIsInvertColorDashedLine)
			cbPolygonObj.iFlag |= (0x1 << 20);
		cbPolygonObj.fDashDistance = (float)dLineDashedInterval;
		cbPolygonObj.fVThickness = (float)dVThickness;
		cbPolygonObj.fShadingMultiplier = 1.f;
		if (bIsInvertPlaneDirection)
			cbPolygonObj.fShadingMultiplier = -1.f;

		vxmatrix matOS2PS = matOS2WS * matWS2CS * matCS2PS;
		cbPolygonObj.matOS2PS = *(D3DXMATRIX*)&matOS2PS;
		D3DXMatrixTranspose(&cbPolygonObj.matOS2PS, &cbPolygonObj.matOS2PS);

		CB_VrInterestingRegion cbVrInterestingRegion;
		HDx11ComputeConstantBufferVrInterestingRegion(&cbVrInterestingRegion, pmapDValueMesh);
		if (cbVrInterestingRegion.iNumRegions > 0)
		{
			D3D11_MAPPED_SUBRESOURCE mappedResInterstingRegion;
			pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_INTERESTINGREGION], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResInterstingRegion);
			CB_VrInterestingRegion* pCBParamInterestingRegion = (CB_VrInterestingRegion*)mappedResInterstingRegion.pData;
			memcpy(pCBParamInterestingRegion, &cbVrInterestingRegion, sizeof(CB_VrInterestingRegion));
			pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_INTERESTINGREGION], 0);
			pdx11DeviceImmContext->PSSetConstantBuffers(8, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_INTERESTINGREGION]);
		}
#pragma endregion // Constant Buffers for Each Mesh Object //

		bool bIsVisible = pCPrimitiveObject->GetVisibility();
		bool bIsMainWireframe = false;
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
		vxfloat4 f4ShadingFactor = vxfloat4(d4ShadingFactors);
		for(uint uiIndexObjUnit = 0; uiIndexObjUnit < uiNumPrimitiveArrangements; uiIndexObjUnit++)
		{
			if (uiIndexObjUnit == 1)
			{
				f4Color = f4WireframeColor;
				f4ShadingFactor = vxfloat4(1.f, 0.f, 0.f, 1.f);
				bIsMainWireframe = bIsWireframe;
			}
			
			if (f4Color.w < 1.f / 255.f)
				continue;

			cbPolygonObj.f4Color = *(D3DXVECTOR4*)&vxfloat4(f4Color);
			cbPolygonObj.f4ShadingFactor = *(D3DXVECTOR4*)&vxfloat4(f4ShadingFactor);

#pragma region // Setting Rasterization Stages
			ID3D11InputLayout* pdx11InputLayer_Target = NULL;
			ID3D11VertexShader* pdx11VS_Target = NULL;
			ID3D11PixelShader* pdx11PS_Target = NULL;
			ID3D11RasterizerState* pdx11RState_TargetObj= NULL;
			uint uiStrideSizeInput = 0;
			uint uiOffset = 0;
			D3D_PRIMITIVE_TOPOLOGY ePRIMITIVE_TOPOLOGY;

			if(pstPrimitiveArchive->GetVerticeDefinition(vxrVertexTypeNORMAL))
			{
				if(pstPrimitiveArchive->GetVerticeDefinition(vxrVertexTypeTEXCOORD0))
				{
					// PNT
					pdx11InputLayer_Target = pdx11LayoutInputPosNorTex;
					if (iRendererMode == __RENDERER_DIFFERENCE_DIVIATION_FROM_VOLUME)
						pdx11VS_Target = pdx11VShader_PointNormalTexture_Deviation;
					else
						pdx11VS_Target = pdx11VShader_PointNormalTexture;

					pdx11PS_Target = *ppdx11PSs[__PS_PHONG_TEX1COLOR];
				}
				else
				{
					// PN
					pdx11InputLayer_Target = pdx11LayoutInputPosNor;
					pdx11VS_Target = pdx11VShader_PointNormal;

					if (iNumTexureLayers > 1)
					{
						if (cbVrInterestingRegion.iNumRegions == 0)
							pdx11PS_Target = *ppdx11PSs[__PS_PHONG_GLOBALCOLOR];
						else
							pdx11PS_Target = *ppdx11PSs[__PS_PHONG_GLOBALCOLOR_MARKERONSURFACE];
					}
					else
					{
						if (cbVrInterestingRegion.iNumRegions == 0)
							pdx11PS_Target = *ppdx11PSs[__PS_PHONG_GLOBALCOLOR_MAXSHADING];
						else
							pdx11PS_Target = *ppdx11PSs[__PS_PHONG_GLOBALCOLOR_MAXSHADING_MARKERONSURFACE];
					}
				}
			}
			else if(pstPrimitiveArchive->GetVerticeDefinition(vxrVertexTypeTEXCOORD0))
			{
				cbPolygonObj.f4ShadingFactor = D3DXVECTOR4(1.f, 1.f, 1.f, 1.f);

				if (pstPrimitiveArchive->GetVerticeDefinition(vxrVertexTypeTEXCOORD2))
				{
					// PTTT
					pdx11InputLayer_Target = pdx11LayoutInputPosTTTex;
					pdx11VS_Target = pdx11VShader_PointTTTextures;
					pdx11PS_Target = *ppdx11PSs[__PS_CMM_MULTI_TEXTS];	// __PS_MAPPING_TEX1
				}
				else
				{
					// PT
					pdx11InputLayer_Target = pdx11LayoutInputPosTex;
					pdx11VS_Target = pdx11VShader_PointTexture;
					pdx11PS_Target = *ppdx11PSs[__PS_MAPPING_TEX1];
				}
			}
			else
			{
				cbPolygonObj.f4ShadingFactor = D3DXVECTOR4(1.f, 1.f, 1.f, 1.f);
				// P
				pdx11InputLayer_Target = pdx11LayoutInputPos;
				pdx11VS_Target = pdx11VShader_Point;
				pdx11PS_Target = *ppdx11PSs[__PS_MAPPING_TEX1];
			}

			if(bIsObjectCMM)
			{
				if (pstPrimitiveArchive->pvTextureResource)
				{
					if (!pstPrimitiveArchive->GetVerticeDefinition(vxrVertexTypeTEXCOORD2))
						pdx11PS_Target = *ppdx11PSs[__PS_CMM_TEXT];
				}
				else
				{
					pdx11PS_Target = *ppdx11PSs[__PS_CMM_LINE];
				}
			}
			else if (bIsAirwayAnalysis)
			{
				pdx11PS_Target = *ppdx11PSs[__PS_AIRWAY_ANALYSIS];
			}
			else if (iRendererMode == __RENDERER_VOLUME_SAMPLE_AND_MAP)
			{
				pdx11PS_Target = *ppdx11PSs[__PS_VOLUME_SUPERIMPOSE];
			}
			else if (iRendererMode == __RENDERER_DIFFERENCE_DIVIATION_FROM_VOLUME)
			{
				pdx11PS_Target = *ppdx11PSs[__PS_VOLUME_DEVIATION];
			}
			else if (iRendererMode == __RENDERER_IMAGE_STACK_MAP_PER_MESHSPRITE)
			{
				pdx11PS_Target = *ppdx11PSs[__PS_IMAGESTACK_MAP];
			}

			switch(pstPrimitiveArchive->ePrimitiveType)
			{
			case vxrPrimitiveTypeLINE:
				if(pstPrimitiveArchive->bIsPrimitiveStripe)
					ePRIMITIVE_TOPOLOGY = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
				else
					ePRIMITIVE_TOPOLOGY = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
				bIsMainWireframe = true;
				break;
			case vxrPrimitiveTypeTRIANGLE:
				if(pstPrimitiveArchive->bIsPrimitiveStripe)
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
			if(bIsAntiAliasingRS)
				iAntiAliasingIndex = __RState_ANTIALIASING_SOLID_CW;
			iAntiAliasingIndex = 0;	// Current Version does not support Anti-Aliasing!!

			EnumCullingMode eCullingMode = rxSR_CULL_NONE;
			if(pstPrimitiveArchive->bIsPrimitiveFrontCCW)
			{
				if(!bIsMainWireframe && eCullingMode != rxSR_CULL_NONE)
					if(eCullingMode == rxSR_CULL_DEFAULT || eCullingMode == rxSR_CULL_FORCED_CW)
						pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_SOLID_CCW + iAntiAliasingIndex];
					else // CCW
						pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_SOLID_CW + iAntiAliasingIndex];
				else
				{
					if(!bIsMainWireframe)
						pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_SOLID_NONE + iAntiAliasingIndex];
					else
						pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_WIRE_NONE + iAntiAliasingIndex];
				}
			}
			else 
			{
				if(!bIsMainWireframe && eCullingMode != rxSR_CULL_NONE)
					if(eCullingMode == rxSR_CULL_DEFAULT || eCullingMode == rxSR_CULL_FORCED_CCW)
						pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_SOLID_CW + iAntiAliasingIndex];
					else // CW
						pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_SOLID_CCW + iAntiAliasingIndex];
				else
				{
					if(!bIsMainWireframe)
						pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_SOLID_NONE + iAntiAliasingIndex];
					else
						pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_WIRE_NONE + iAntiAliasingIndex];
				}
			}
			if(bIsMainWireframe)
				pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_WIRE_NONE];
			else if((bIsForcedCullOff && !bIsMainWireframe) || bIsObjectCMM)
				pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_SOLID_NONE + iAntiAliasingIndex];


			ID3D11Buffer* pdx11BufferTargetMesh = (ID3D11Buffer*)vtrGpuResourceArchiveBufferVertex.at(iIndexMeshObj).vtrPtrs.at(0);
			ID3D11Buffer* pdx11IndiceTargetMesh = NULL;
			uiStrideSizeInput = sizeof(vxfloat3)*(uint)pstPrimitiveArchive->GetNumVertexDefinitions();
			pdx11DeviceImmContext->IASetVertexBuffers(0, 1, (ID3D11Buffer**)&pdx11BufferTargetMesh, &uiStrideSizeInput, &uiOffset);
			if(pstPrimitiveArchive->puiIndexList != NULL)
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

#pragma region // SELF RENDERING PASS
			// Self-Transparent Surfaces //
			int iNumSelfTransparentRenderPass = iObjectNumSelfTransparentRenderPass;	// at least 1
			if(cbPolygonObj.f4Color.w > 0.99f)
				iNumSelfTransparentRenderPass = 1;

			for(int iIndexSelfSurface = 0; iIndexSelfSurface < iNumSelfTransparentRenderPass; iIndexSelfSurface++)
			{
				// Render Work //
				if(pstPrimitiveArchive->bIsPrimitiveStripe || pstPrimitiveArchive->ePrimitiveType == vxrPrimitiveTypePOINT)
				{
					pdx11DeviceImmContext->Draw( pstPrimitiveArchive->uiNumVertice, 0 );
				}
				else
				{
					pdx11DeviceImmContext->DrawIndexed( pstPrimitiveArchive->uiNumIndice, 0, 0 ); 
				}
				iCountDrawing++;

				if(iNumTexureLayers == 1)
					continue;

				iCountRTBuffers++;
				int iModRTLayer = iCountRTBuffers % iNumTexureLayers;
				if(iModRTLayer == 0)
				{
					pdx11DeviceImmContext->OMSetRenderTargets(2, pdx11RTVs_NULL, (ID3D11DepthStencilView*)gpuResourcesFB_ARCHs[__FB_DSV_DEPTH].vtrPtrs.at(0));

					if(bIsSupportedCS)
					{
						pdx11DeviceImmContext->CSSetShaderResources(30, NUM_TEXRT_LAYERS, pdx11SRV_RenderOuts);
						pdx11DeviceImmContext->CSSetShaderResources(60, NUM_TEXRT_LAYERS, pdx11SRV_DepthCSs);

						UINT UAVInitialCounts = 0;
						if(iCountMerging % 2 == 0)
						{
							pdx11DeviceImmContext->CSSetShaderResources(5, 1, (ID3D11ShaderResourceView**)&gpuResourceMERGE_CS_ARCHs[__FB_SRV_MERGEOUT].vtrPtrs.at(1));
							pdx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, (ID3D11UnorderedAccessView**)&gpuResourceMERGE_CS_ARCHs[__FB_UAV_MERGEOUT].vtrPtrs.at(0), &UAVInitialCounts);
						}
						else
						{
							pdx11DeviceImmContext->CSSetShaderResources(5, 1, (ID3D11ShaderResourceView**)&gpuResourceMERGE_CS_ARCHs[__FB_SRV_MERGEOUT].vtrPtrs.at(0));
							pdx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, (ID3D11UnorderedAccessView**)&gpuResourceMERGE_CS_ARCHs[__FB_UAV_MERGEOUT].vtrPtrs.at(1), &UAVInitialCounts);
						}

						pdx11DeviceImmContext->Dispatch(uiNumGridX, uiNumGridY, 1);

						// Set NULL States //
						pdx11DeviceImmContext->CSSetShaderResources(5, 1, &pdx11SRV_NULL);
						pdx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, &pdx11UAV_NULL, &UAVInitialCounts);
						pdx11DeviceImmContext->CSSetShaderResources(30, NUM_TEXRT_LAYERS, pdx11SRV_RenderOuts_NULL);
						pdx11DeviceImmContext->CSSetShaderResources(60, NUM_TEXRT_LAYERS, pdx11SRV_DepthCSs_NULL);

						// Clear //
						if(iCountMerging % 2 == 0)
							pdx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gpuResourceMERGE_CS_ARCHs[__FB_UAV_MERGEOUT].vtrPtrs.at(1), uiClearVlaues);
						else
							pdx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gpuResourceMERGE_CS_ARCHs[__FB_UAV_MERGEOUT].vtrPtrs.at(0), uiClearVlaues);
					}
					else
					{
						pdx11DeviceImmContext->PSSetShaderResources(30, NUM_TEXRT_LAYERS, pdx11SRV_RenderOuts);
						pdx11DeviceImmContext->PSSetShaderResources(60, NUM_TEXRT_LAYERS, pdx11SRV_DepthCSs);

						int iRSA_Index_Offset_Prev = 0;
						int iRSA_Index_Offset_Out = NUM_MERGE_LAYERS;
						if(iCountMerging % 2 == 0)
						{
							iRSA_Index_Offset_Prev = NUM_MERGE_LAYERS;
							iRSA_Index_Offset_Out = 0;
						}

						for(int iIndexRS = 0; iIndexRS < NUM_MERGE_LAYERS; iIndexRS++)
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
						pdx11DeviceImmContext->OMSetDepthStencilState( pdx11CommonParams->pdx11DSStates[__DSState_DEPTHENABLE_ALWAYS], 0 );

						pdx11DeviceImmContext->Draw(4, 0);

						// Set NULL States //
						pdx11DeviceImmContext->PSSetShaderResources(30, NUM_TEXRT_LAYERS, pdx11SRV_RenderOuts_NULL);
						pdx11DeviceImmContext->PSSetShaderResources(60, NUM_TEXRT_LAYERS, pdx11SRV_DepthCSs_NULL);
						pdx11DeviceImmContext->PSSetShaderResources(90, NUM_MERGE_LAYERS, pdx11SRV_RSA_NULL);
						pdx11DeviceImmContext->OMSetRenderTargets(min(NUM_MERGE_LAYERS, 8)	, pdx11RTV_RSA_NULL, NULL);	// MAX 8 in Feature Level 10_0

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
						pdx11DeviceImmContext->OMSetDepthStencilState( pdx11CommonParams->pdx11DSStates[__DSState_DEPTHENABLE_LESSEQUAL], 0 );

						// Clear //
						for(int iIndexRS = 0; iIndexRS < NUM_MERGE_LAYERS; iIndexRS++)
							pdx11DeviceImmContext->ClearRenderTargetView(vtrRTV_MergePingpongPS.at(iIndexRS + iRSA_Index_Offset_Prev), fClearColor);
					}

					for(int iIndexRTT = 0; iIndexRTT < iNumTexureLayers; iIndexRTT++)
					{
						pdx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_RENDEROUT].vtrPtrs.at(iIndexRTT), fClearColor);
						pdx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_DEPTH_ZTHICKCS].vtrPtrs.at(iIndexRTT), fClearColor);
					}

					iCountMerging++;
				}	// iModRTLayer == 0

				if(bIsSupportedCS)
				{
					pdx11DeviceImmContext->CSSetShaderResources(30, NUM_TEXRT_LAYERS, pdx11SRV_RenderOuts_NULL);
					pdx11DeviceImmContext->CSSetShaderResources(60, NUM_TEXRT_LAYERS, pdx11SRV_DepthCSs_NULL);
				}
				else
				{
					pdx11DeviceImmContext->PSSetShaderResources(30, NUM_TEXRT_LAYERS, pdx11SRV_RenderOuts_NULL);
					pdx11DeviceImmContext->PSSetShaderResources(60, NUM_TEXRT_LAYERS, pdx11SRV_DepthCSs_NULL);
				}

				pdx11RTVs[0] = (ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_RENDEROUT].vtrPtrs.at(iModRTLayer);
				pdx11RTVs[1] = (ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_DEPTH_ZTHICKCS].vtrPtrs.at(iModRTLayer);
				pdx11DeviceImmContext->OMSetRenderTargets(2, pdx11RTVs, (ID3D11DepthStencilView*)gpuResourcesFB_ARCHs[__FB_DSV_DEPTH].vtrPtrs.at(0));

				if(iNumSelfTransparentRenderPass > 1)
				{
					pdx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gpuResourcesFB_ARCHs[__FB_TX2D_DEPTH_DUAL].vtrPtrs.at(0), (ID3D11Texture2D*)gpuResourcesFB_ARCHs[__FB_TX2D_DEPTH].vtrPtrs.at(0));
					pdx11DeviceImmContext->ClearDepthStencilView((ID3D11DepthStencilView*)gpuResourcesFB_ARCHs[__FB_DSV_DEPTH].vtrPtrs.at(0), D3D11_CLEAR_DEPTH, 1.0f, 0);
				}
			}	// for(int k = 0; k < iNumSelfTransparentRenderPass; k++)

			if (iNumTexureLayers > 1)
			{
				pdx11DeviceImmContext->ClearDepthStencilView((ID3D11DepthStencilView*)gpuResourcesFB_ARCHs[__FB_DSV_DEPTH].vtrPtrs.at(0), D3D11_CLEAR_DEPTH, 1.0f, 0);
				pdx11DeviceImmContext->OMSetDepthStencilState(pdx11CommonParams->pdx11DSStates[__DSState_DEPTHENABLE_LESSEQUAL], 0);

				if (iNumSelfTransparentRenderPass > 1)
					pdx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_DEPTH_DUAL].vtrPtrs.at(0), fClearColor);
			}
#pragma endregion // SELF RENDERING PASS
		}
	}

#pragma region // Final Rearrangement
	if (iCountRTBuffers % iNumTexureLayers != 0 || (iCountRTBuffers == 0 && iCountDrawing > 0))
	{
		pdx11DeviceImmContext->OMSetRenderTargets(2, pdx11RTVs_NULL, (ID3D11DepthStencilView*)gpuResourcesFB_ARCHs[__FB_DSV_DEPTH].vtrPtrs.at(0));

		if(bIsSupportedCS)
		{
			pdx11DeviceImmContext->CSSetShaderResources(30, NUM_TEXRT_LAYERS, pdx11SRV_RenderOuts);
			pdx11DeviceImmContext->CSSetShaderResources(60, NUM_TEXRT_LAYERS, pdx11SRV_DepthCSs);

			UINT UAVInitialCounts = 0;
			if(iCountMerging % 2 == 0)
			{
				pdx11DeviceImmContext->CSSetShaderResources(5, 1, (ID3D11ShaderResourceView**)&gpuResourceMERGE_CS_ARCHs[__FB_SRV_MERGEOUT].vtrPtrs.at(1));
				pdx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, (ID3D11UnorderedAccessView**)&gpuResourceMERGE_CS_ARCHs[__FB_UAV_MERGEOUT].vtrPtrs.at(0), &UAVInitialCounts);
			}
			else
			{
				pdx11DeviceImmContext->CSSetShaderResources(5, 1, (ID3D11ShaderResourceView**)&gpuResourceMERGE_CS_ARCHs[__FB_SRV_MERGEOUT].vtrPtrs.at(0));
				pdx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, (ID3D11UnorderedAccessView**)&gpuResourceMERGE_CS_ARCHs[__FB_UAV_MERGEOUT].vtrPtrs.at(1), &UAVInitialCounts);
			}

			pdx11DeviceImmContext->Dispatch(uiNumGridX, uiNumGridY, 1);

			pdx11DeviceImmContext->CSSetShaderResources(5, 1, &pdx11SRV_NULL);
			pdx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, &pdx11UAV_NULL, &UAVInitialCounts);
			pdx11DeviceImmContext->CSSetShaderResources(30, NUM_TEXRT_LAYERS, pdx11SRV_RenderOuts_NULL);
			pdx11DeviceImmContext->CSSetShaderResources(60, NUM_TEXRT_LAYERS, pdx11SRV_DepthCSs_NULL);
		}
		else
		{
			pdx11DeviceImmContext->PSSetShaderResources(30, NUM_TEXRT_LAYERS, pdx11SRV_RenderOuts);
			pdx11DeviceImmContext->PSSetShaderResources(60, NUM_TEXRT_LAYERS, pdx11SRV_DepthCSs);

			int iRSA_Index_Offset_Prev = 0;
			int iRSA_Index_Offset_Out = NUM_MERGE_LAYERS;
			if(iCountMerging % 2 == 0)
			{
				iRSA_Index_Offset_Prev = NUM_MERGE_LAYERS;
				iRSA_Index_Offset_Out = 0;
			}
			for(int iIndexRS = 0; iIndexRS < NUM_MERGE_LAYERS; iIndexRS++)
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
			pdx11DeviceImmContext->OMSetDepthStencilState( pdx11CommonParams->pdx11DSStates[__DSState_DEPTHENABLE_ALWAYS], 0 );

			pdx11DeviceImmContext->Draw(4, 0);

			pdx11DeviceImmContext->PSSetShaderResources(90, NUM_MERGE_LAYERS, pdx11SRV_RSA_NULL);
			pdx11DeviceImmContext->OMSetRenderTargets(min(NUM_MERGE_LAYERS, 8), pdx11RTV_RSA_NULL, NULL);
			pdx11DeviceImmContext->PSSetShaderResources(30, NUM_TEXRT_LAYERS, pdx11SRV_RenderOuts_NULL);
			pdx11DeviceImmContext->PSSetShaderResources(60, NUM_TEXRT_LAYERS, pdx11SRV_DepthCSs_NULL);
		}

		iCountMerging++;
	}
#pragma endregion // Final Rearrangement

	pCMergeIObject->RegisterCustomParameter(_T("_int_CountMerging"), iCountMerging);

	if(iCountMerging == 0)	// this means that there is no valid rendering pass
	{
		if(bIsSystemOut)
		{
			if (bIsSupportedCS)
			{
				SVXFrameBuffer* psvxFrameBuffer = (SVXFrameBuffer*)pCMergeIObject->GetFrameBuffer(vxrFrameBufferUsageCUSTOM, 0);
				RWB_Output_MultiLayers* pstRWBuffer = (RWB_Output_MultiLayers*)psvxFrameBuffer->pvBuffer;
				for (int iY = 0; iY < i2SizeScreenCurrent.y; iY++)
				{
					for (int iX = 0; iX < i2SizeScreenCurrent.x; iX++)
					{
						for (int i = 0; i < NUM_MERGE_LAYERS; i++)
						{
							int iAddrCpuMem = iY*i2SizeScreenCurrent.x + iX;
							pstRWBuffer[iAddrCpuMem].arrayIntVisibilityRSA[i] = 0;
							pstRWBuffer[iAddrCpuMem].arrayDepthBackRSA[i] = FLT_MAX;
							pstRWBuffer[iAddrCpuMem].arrayThicknessRSA[i] = 0;
						}
					}
				}
			}
			else
			{
				SVXFrameBuffer* psvxFrameBuffer = (SVXFrameBuffer*)pCOutputIObject->GetFrameBuffer(vxrFrameBufferUsageRENDEROUT, 0);
				vxbyte4* py4Buffer = (vxbyte4*)psvxFrameBuffer->pvBuffer;
				SVXFrameBuffer* psvxFrameBufferDepth = (SVXFrameBuffer*)pCOutputIObject->GetFrameBuffer(vxrFrameBufferUsageDEPTH, 0);
				float* pfBuffer = (float*)psvxFrameBufferDepth->pvBuffer;
				memset(py4Buffer, 0, i2SizeScreenCurrent.x * i2SizeScreenCurrent.y * sizeof(vxbyte4));
				memset(pfBuffer, 0x77, i2SizeScreenCurrent.x * i2SizeScreenCurrent.y * sizeof(float));
			}
		}
		goto ENDRENDER;
	}

	if(bIsSystemOut) // This implies !bIsSupportedCS // See Line 92
	{
		// PS into PS
		int iRSA_Index_Offset_Prev = 0;
		if(iCountMerging % 2 == 0)
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
		gpuResourceFB_RENDEROUT_SYSTEM_DESC.iSourceObjectID = pCMergeIObject->GetObjectID();
		gpuResourceFB_DEPTH_SYSTEM_DESC.eResourceType = vxgResourceTypeDX11RESOURCE;
		gpuResourceFB_DEPTH_SYSTEM_DESC.eDataType = vxrDataTypeFLOAT;
		gpuResourceFB_DEPTH_SYSTEM_DESC.eGpuSdkType = vxgGpuSdkTypeDX11;
		gpuResourceFB_DEPTH_SYSTEM_DESC.strUsageSpecifier = _T("TEXTURE2D_IOBJECT_SYSTEM");
		gpuResourceFB_DEPTH_SYSTEM_DESC.iSizeStride = VXHGetDataTypeSizeByte(gpuResourceFB_DEPTH_SYSTEM_DESC.eDataType);
		gpuResourceFB_DEPTH_SYSTEM_DESC.iCustomMisc = 0;
		gpuResourceFB_DEPTH_SYSTEM_DESC.iSourceObjectID = pCMergeIObject->GetObjectID();
		// When Resize, the Framebuffer is released, so we do not have to check the resize case.
		if(!pCGpuManager->GetGpuResourceArchive(&gpuResourceFB_RENDEROUT_SYSTEM_DESC, &gpuResourceFB_RENDEROUT_SYSTEM_ARCH))
			pCGpuManager->GenerateGpuResource(&gpuResourceFB_RENDEROUT_SYSTEM_DESC, pCMergeIObject, &gpuResourceFB_RENDEROUT_SYSTEM_ARCH, NULL);
		if(!pCGpuManager->GetGpuResourceArchive(&gpuResourceFB_DEPTH_SYSTEM_DESC, &gpuResourceFB_DEPTH_SYSTEM_ARCH))
			pCGpuManager->GenerateGpuResource(&gpuResourceFB_DEPTH_SYSTEM_DESC, pCMergeIObject, &gpuResourceFB_DEPTH_SYSTEM_ARCH, NULL);

		SVXFrameBuffer* psvxFrameBuffer = (SVXFrameBuffer*)pCOutputIObject->GetFrameBuffer(vxrFrameBufferUsageRENDEROUT, 0);
		vxbyte4* py4Buffer = (vxbyte4*)psvxFrameBuffer->pvBuffer;

		D3D11_MAPPED_SUBRESOURCE mappedResSys;
		pdx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gpuResourceFB_RENDEROUT_SYSTEM_ARCH.vtrPtrs.at(0), (ID3D11Texture2D*)gpuResourcesFB_ARCHs[__FB_TX2D_RENDEROUT].vtrPtrs.at(0));
		pdx11DeviceImmContext->Map((ID3D11Texture2D*)gpuResourceFB_RENDEROUT_SYSTEM_ARCH.vtrPtrs.at(0), 0, D3D11_MAP_READ, NULL, &mappedResSys);
		
#ifdef CICERO_TEST
		vxfloat4* pf4RenderOutFile = NULL;
		if (bReloadHLSLObjFiles)
		{
			pf4RenderOutFile = new vxfloat4[i2SizeScreenCurrent.x * i2SizeScreenCurrent.y];
		}

		vxfloat4* pf4RenderOuts = (vxfloat4*)mappedResSys.pData;
		for(int i = 0; i < i2SizeScreenCurrent.y; i++)
		{
			for(int j = 0; j < i2SizeScreenCurrent.x; j++)
			{
				vxfloat4 f3Test = pf4RenderOuts[j] * 255.f;
				if (f3Test.w > 0)
					int gg = 0; 
				if (bReloadHLSLObjFiles)
					pf4RenderOutFile[i*i2SizeScreenCurrent.x + j] = pf4RenderOuts[j];
				// __PS_MERGE_LAYERS_TO_RENDEROUT 에서 INT -> FLOAT4 로 되어 배열된 color 요소가 들어 옴. //
				py4Buffer[i*i2SizeScreenCurrent.x + j].x = (byte)(f3Test.z);	// B
				py4Buffer[i*i2SizeScreenCurrent.x + j].y = (byte)(f3Test.y);	// G
				py4Buffer[i*i2SizeScreenCurrent.x + j].z = (byte)(f3Test.x);	// R
				py4Buffer[i*i2SizeScreenCurrent.x + j].w = (byte)(f3Test.w);	// A
			}
			pf4RenderOuts += mappedResSys.RowPitch / (4 * 4);	// mappedResSys.RowPitch (byte unit)
		}

		// To Do //
		if (bReloadHLSLObjFiles)
		{
			FIBITMAP *dib = NULL;
			FreeImage_Initialise();
			FREE_IMAGE_TYPE fif = FIT_RGBAF;
			dib = FreeImage_AllocateT(fif, i2SizeScreenCurrent.x, i2SizeScreenCurrent.y, sizeof(vxfloat4)* 8);
			int iTifWidthPitch = FreeImage_GetPitch(dib) / sizeof(vxfloat4);

			TCHAR tcTemp[512];
			std::wstringstream strFileNameTemp;
			wsprintf(tcTemp, _T("_%dx%d_%04d"), i2SizeScreenCurrent.x, i2SizeScreenCurrent.y, 0);
			wstring wsFileName = L"d:\\TEST\\CICERO_ORTHO_4K.tif";
			vxfloat4* pf4Bmp = (vxfloat4*)FreeImage_GetBits(dib);
			for (int iY = 0; iY < i2SizeScreenCurrent.y; iY++)
			{
				memcpy(pf4Bmp, &pf4RenderOutFile[iY * i2SizeScreenCurrent.x], sizeof(vxfloat4)* i2SizeScreenCurrent.x);
				pf4Bmp += iTifWidthPitch;
			}
			FreeImage_SaveU(FIF_TIFF, dib, wsFileName.c_str(), 0);
			FreeImage_Unload(dib);
			FreeImage_DeInitialise();

// 			FILE* pFile;
// 			if (_wfopen_s(&pFile, L"d:\\TEST\\CICERO_ORTHO_4K.raw", _T("wb")))
// 				return false;
// 
// 			for (int i = 0; i < i2SizeScreenCurrent.y; i++)
// 				fwrite(&pf4RenderOutFile[i2SizeScreenCurrent.x * i], sizeof(vxfloat4), i2SizeScreenCurrent.x, pFile);
// 
// 			fclose(pFile);
 			VXSAFE_DELETEARRAY(pf4RenderOutFile);
		}


#else
		vxbyte4* py4RenderOuts = (vxbyte4*)mappedResSys.pData;
		for (int i = 0; i < i2SizeScreenCurrent.y; i++)
		{
			for (int j = 0; j < i2SizeScreenCurrent.x; j++)
			{
				vxbyte4 y4Renderout = py4RenderOuts[j];
				// __PS_MERGE_LAYERS_TO_RENDEROUT 에서 INT -> FLOAT4 로 되어 배열된 color 요소가 들어 옴. //

				// BGRA
				py4Buffer[i*i2SizeScreenCurrent.x + j] = vxbyte4(y4Renderout.z, y4Renderout.y, y4Renderout.x, y4Renderout.w);

			}
			py4RenderOuts += mappedResSys.RowPitch / 4;	// mappedResSys.RowPitch (byte unit)
		}
#endif
		pdx11DeviceImmContext->Unmap((ID3D11Texture2D*)gpuResourceFB_RENDEROUT_SYSTEM_ARCH.vtrPtrs.at(0), 0);

		SVXFrameBuffer* psvxFrameBufferDepth = (SVXFrameBuffer*)pCOutputIObject->GetFrameBuffer(vxrFrameBufferUsageDEPTH, 0);
		float* pfBuffer = (float*)psvxFrameBufferDepth->pvBuffer;
		pdx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gpuResourceFB_DEPTH_SYSTEM_ARCH.vtrPtrs.at(0), (ID3D11Texture2D*)gpuResourcesFB_ARCHs[__FB_TX2D_DEPTH_ZTHICKCS].vtrPtrs.at(0));
		pdx11DeviceImmContext->Map((ID3D11Texture2D*)gpuResourceFB_DEPTH_SYSTEM_ARCH.vtrPtrs.at(0), 0, D3D11_MAP_READ, NULL, &mappedResSys);
		float* pfDeferredContexts = (float*)mappedResSys.pData;
		for(int i = 0; i < i2SizeScreenCurrent.y; i++)
		{
			for(int j = 0; j < i2SizeScreenCurrent.x; j++)
			{
				if(py4Buffer[i*i2SizeScreenCurrent.x + j].w > 0)
					pfBuffer[i*i2SizeScreenCurrent.x + j] = pfDeferredContexts[j];
				else
					pfBuffer[i*i2SizeScreenCurrent.x + j] = FLT_MAX;
			}
			pfDeferredContexts += mappedResSys.RowPitch/4;	// mappedResSys.RowPitch (byte unit)
		}
		pdx11DeviceImmContext->Unmap((ID3D11Texture2D*)gpuResourceFB_DEPTH_SYSTEM_ARCH.vtrPtrs.at(0), 0);
#pragma endregion
	}
#ifdef ____MY_PERFORMANCE_TEST
	else
	{
		if(bIsSupportedCS)
		{
			SVXGPUResourceDESC gpuResourceMERGE_SYSTEM_DESC;
			SVXGPUResourceArchive gpuResourceMERGE_SYSTEM_ARCH;
			gpuResourceMERGE_SYSTEM_DESC.eDataType = vxrDataTypeSTRUCTURED;
			gpuResourceMERGE_SYSTEM_DESC.eResourceType = vxgResourceTypeDX11RESOURCE;
			gpuResourceMERGE_SYSTEM_DESC.eGpuSdkType = vxgGpuSdkTypeDX11;
			gpuResourceMERGE_SYSTEM_DESC.strUsageSpecifier = _T("BUFFER_IOBJECT_SYSTEM");
			gpuResourceMERGE_SYSTEM_DESC.iCustomMisc = __BLOCKSIZE | (2 << 16);	// '2' means two resource buffers
			gpuResourceMERGE_SYSTEM_DESC.iSizeStride = sizeof(RWB_Output_MultiLayers);
			gpuResourceMERGE_SYSTEM_DESC.iSourceObjectID = pCMergeIObject->GetObjectID();
			if(!pCGpuManager->GetGpuResourceArchive(&gpuResourceMERGE_SYSTEM_DESC, &gpuResourceMERGE_SYSTEM_ARCH))
				pCGpuManager->GenerateGpuResource(&gpuResourceMERGE_SYSTEM_DESC, pCMergeIObject, &gpuResourceMERGE_SYSTEM_ARCH, NULL);

#pragma region // Copy GPU to CPU
			SVXFrameBuffer* psvxFrameBuffer = (SVXFrameBuffer*)pCMergeIObject->GetFrameBuffer(vxrFrameBufferUsageCUSTOM, 0);
			RWB_Output_MultiLayers* pstRWBuffer = (RWB_Output_MultiLayers*)psvxFrameBuffer->pvBuffer;

			ID3D11Buffer* pdx11TargetBuffer = NULL;
			if(iCountMerging % 2 == 0)
				pdx11TargetBuffer = (ID3D11Buffer*)gpuResourceMERGE_CS_ARCHs[__FB_UBUF_MERGEOUT].vtrPtrs.at(1);
			else
				pdx11TargetBuffer = (ID3D11Buffer*)gpuResourceMERGE_CS_ARCHs[__FB_UBUF_MERGEOUT].vtrPtrs.at(0);

			D3D11_MAPPED_SUBRESOURCE mappedResSys;
			pdx11DeviceImmContext->CopyResource((ID3D11Buffer*)gpuResourceMERGE_SYSTEM_ARCH.vtrPtrs.at(0), pdx11TargetBuffer);
			pdx11DeviceImmContext->Map((ID3D11Buffer*)gpuResourceMERGE_SYSTEM_ARCH.vtrPtrs.at(0), 0, D3D11_MAP_READ, NULL, &mappedResSys);
			RWB_Output_MultiLayers* pstRWBufferOut = (RWB_Output_MultiLayers*)mappedResSys.pData;

			int iAddrBufferWidthOffset = uiNumGridX*__BLOCKSIZE;

			for(int i = 0; i < i2SizeScreenCurrent.y; i++)
			{
				memcpy(&pstRWBuffer[i * i2SizeScreenCurrent.x], 
					&pstRWBufferOut[i * iAddrBufferWidthOffset], 
					sizeof(RWB_Output_MultiLayers) * i2SizeScreenCurrent.x);
			}
			pdx11DeviceImmContext->Unmap((ID3D11Buffer*)gpuResourceMERGE_SYSTEM_ARCH.vtrPtrs.at(0), 0);

			QueryPerformanceFrequency(&lIntFreq);
			QueryPerformanceCounter(&lIntCntEnd);
			double dRunTime = (lIntCntEnd.QuadPart - lIntCntStart.QuadPart)/(double)(lIntFreq.QuadPart) * 1000;
			printf("Window (%dx%d), SR : %f ms, # meshes : %d;;;&d\n", i2SizeScreenCurrent.x, i2SizeScreenCurrent.y, (float)dRunTime, vtrInputPrimitives.size(), (int)pstRWBuffer[i2SizeScreenCurrent.y / 2 * i2SizeScreenCurrent.x + i2SizeScreenCurrent.x / 2].arrayIntVisibilityRSA[1] % 10);
#pragma endregion
		}
		else
		{
			SVXGPUResourceDESC gpuResourceMERGE_PS_RAYSEGMENT_SYSTEM_DESC;
			SVXGPUResourceArchive gpuResourceMERGE_PS_RAYSEGMENT_SYSTEM_ARCH;

			gpuResourceMERGE_PS_RAYSEGMENT_SYSTEM_DESC.eDataType = vxrDataTypeFLOAT4CHANNEL;
			gpuResourceMERGE_PS_RAYSEGMENT_SYSTEM_DESC.eResourceType = vxgResourceTypeDX11RESOURCE;
			gpuResourceMERGE_PS_RAYSEGMENT_SYSTEM_DESC.eGpuSdkType = vxgGpuSdkTypeDX11;
			gpuResourceMERGE_PS_RAYSEGMENT_SYSTEM_DESC.strUsageSpecifier = _T("TEXTURE2D_IOBJECT_SYSTEM");
			gpuResourceMERGE_PS_RAYSEGMENT_SYSTEM_DESC.iCustomMisc = 0;
			gpuResourceMERGE_PS_RAYSEGMENT_SYSTEM_DESC.iSizeStride = VXHGetDataTypeSizeByte(gpuResourceMERGE_PS_RAYSEGMENT_SYSTEM_DESC.eDataType);
			gpuResourceMERGE_PS_RAYSEGMENT_SYSTEM_DESC.iSourceObjectID = pCMergeIObject->GetObjectID();
			if(!pCGpuManager->GetGpuResourceArchive(&gpuResourceMERGE_PS_RAYSEGMENT_SYSTEM_DESC, &gpuResourceMERGE_PS_RAYSEGMENT_SYSTEM_ARCH))
				pCGpuManager->GenerateGpuResource(&gpuResourceMERGE_PS_RAYSEGMENT_SYSTEM_DESC, pCMergeIObject, &gpuResourceMERGE_PS_RAYSEGMENT_SYSTEM_ARCH, NULL);

			int iRSA_Index_Offset_Out = 0;
			if(iCountMerging % 2 == 0)
			{
				iRSA_Index_Offset_Out = NUM_MERGE_LAYERS;
			}

			SVXFrameBuffer* psvxFrameBuffer = (SVXFrameBuffer*)pCMergeIObject->GetFrameBuffer(vxrFrameBufferUsageCUSTOM, 0);
			RWB_Output_MultiLayers* pstRWBuffer = (RWB_Output_MultiLayers*)psvxFrameBuffer->pvBuffer;

#pragma region // Copy GPU to CPU
			for(int iIndexRSABuffer = 0; iIndexRSABuffer < NUM_MERGE_LAYERS; iIndexRSABuffer++)
			{
				ID3D11Texture2D* pdx11TargetTextureRGBA = 
					(ID3D11Texture2D*)gpuResourceMERGE_PS_ARCHs[__EXFB_TX2D_MERGER_RAYSEGMENT].vtrPtrs.at(iIndexRSABuffer + iRSA_Index_Offset_Out);

				pdx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gpuResourceMERGE_PS_RAYSEGMENT_SYSTEM_ARCH.vtrPtrs.at(0), pdx11TargetTextureRGBA);

				D3D11_MAPPED_SUBRESOURCE mappedResSys;
				pdx11DeviceImmContext->Map((ID3D11Buffer*)gpuResourceMERGE_PS_RAYSEGMENT_SYSTEM_ARCH.vtrPtrs.at(0), 0, D3D11_MAP_READ, NULL, &mappedResSys);
				vxfloat4* pf4Outs = (vxfloat4*)mappedResSys.pData;

				for(int i = 0; i < i2SizeScreenCurrent.y; i++)
				{
					for(int j = 0; j < i2SizeScreenCurrent.x; j++)
					{
						vxfloat4 f4Out = pf4Outs[j];

						vxbyte4 y4Color = vxbyte4((byte)(f4Out.x / 1000.f), (byte)((int)f4Out.x % 1000), (byte)(f4Out.y / 1000.f), (byte)((int)f4Out.y % 1000));
						byte iR = y4Color.x;
						y4Color.x = y4Color.z;
						y4Color.z = iR;
						memcpy(&pstRWBuffer[i*i2SizeScreenCurrent.x + j].arrayIntVisibilityRSA[iIndexRSABuffer], &y4Color, sizeof(vxbyte4));

						pstRWBuffer[i*i2SizeScreenCurrent.x + j].arrayDepthBackRSA[iIndexRSABuffer] = f4Out.z;
						pstRWBuffer[i*i2SizeScreenCurrent.x + j].arrayThicknessRSA[iIndexRSABuffer] = f4Out.w;
					}
					pf4Outs += mappedResSys.RowPitch / 16;	// mappedResSys.RowPitch (byte unit)
				}
				pdx11DeviceImmContext->Unmap((ID3D11Buffer*)gpuResourceMERGE_PS_RAYSEGMENT_SYSTEM_ARCH.vtrPtrs.at(0), 0);
			}
#pragma endregion
		}
	}
#endif

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
	pCMergeIObject->SetDefineModuleSpecifier(_T("VS-GPUDX11VxRenderer module"));

	// Time Check
	double dRunTime = (lIntCntEnd.QuadPart - lIntCntStart.QuadPart)/(double)(lIntFreq.QuadPart);
	if(pdRunTime)
		*pdRunTime = dRunTime;

	((std::mutex*)HDx11GetMutexGpuCriticalPath())->unlock();
	
	return true;
}