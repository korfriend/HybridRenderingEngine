#include "RendererHeader.h"
//#include "FreeImage.h"

//#define __CS_5_0_SUPPORT

//_bool_IsForcedPerRTarget true
//_int_LevelSR 0
//_double_VZThickness 1

bool RenderSrCommon(VmFnContainer* _fncontainer,
	VmGpuManager* pCGpuManager,
	GpuDX11CommonParameters* pdx11CommonParams,
	ID3D11InputLayout* pdx11ILs[NUMINPUTLAYOUTS],
	ID3D11VertexShader** ppdx11VS_CommonUsages[NUMSHADERS_SR_VS],
	ID3D11VertexShader** ppdx11VS_BiasZs[NUMSHADERS_BIASZ_SR_VS],
	ID3D11PixelShader** ppdx11PS_SRs[NUMSHADERS_SR_PS],
	ID3D11PixelShader** ppdx11PS_Merges[NUMSHADERS_MERGE_PS],
	ID3D11GeometryShader** ppdx11GS[NUMSHADERS_GS],
	ID3D11ComputeShader** ppdx11CS_Merges[NUMSHADERS_MERGE_CS],
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

	VmLObject* pCLObjectForParameters = (VmLObject*)_fncontainer->GetVmObject(VmObjKey(ObjectTypeBUFFER, true), 0);

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
	double dVThickness = -1.0;
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

		string strNames_MG_PS[NUMSHADERS_MERGE_PS] = {
			("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_MERGE_LAYEROUT_TO_RENDEROUT_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_MERGE_SROUT_AND_LAYEROUT_TO_LAYEROUT_ps_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\PS_MERGE_ADV_SROUT_AND_LAYEROUT_TO_LAYEROUT_ps_4_0")
		};
		for (int i = 0; i < NUMSHADERS_MERGE_PS; i++)
		{
			string strName = strNames_MG_PS[i];

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
					VMSAFE_RELEASE(*ppdx11PS_Merges[i]);
					*ppdx11PS_Merges[i] = pdx11PShader;
				}
				VMSAFE_DELETEARRAY(pyRead);
			}
		}

		string strNames_GS[NUMSHADERS_GS] = {
			("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\GS_ThickPoints_gs_4_0")
			, ("..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\GS_ThickLines_gs_4_0")
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

#ifdef __CS_5_0_SUPPORT
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
#endif
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
	if (!pCOutputIObject->ReplaceFrameBuffer(FrameBufferUsageRENDEROUT, 0, data_type::dtype<vmbyte4>(), ("common render out frame buffer : defined in vismtv_inbuilt_renderergpudx module")))
		pCOutputIObject->InsertFrameBuffer(data_type::dtype<vmbyte4>(), FrameBufferUsageRENDEROUT, ("common render out frame buffer : defined in vismtv_inbuilt_renderergpudx module"));

	while (pCOutputIObject->GetFrameBuffer(FrameBufferUsageDEPTH, 1) != NULL)
		pCOutputIObject->DeleteFrameBuffer(FrameBufferUsageDEPTH, 1);
	if (!pCOutputIObject->ReplaceFrameBuffer(FrameBufferUsageDEPTH, 0, data_type::dtype<float>(), ("1st hit screen depth frame buffer : defined in vismtv_inbuilt_renderergpudx module")))
		pCOutputIObject->InsertFrameBuffer(data_type::dtype<float>(), FrameBufferUsageDEPTH, ("1st hit screen depth frame buffer : defined in vismtv_inbuilt_renderergpudx module"));

	if (iLevelVR == 2 && !bIsFinalRenderer)
	{
		while (pCMergeIObject->GetFrameBuffer(FrameBufferUsageRENDEROUT, 1) != NULL)
			pCMergeIObject->DeleteFrameBuffer(FrameBufferUsageRENDEROUT, 1);
		if (!pCMergeIObject->ReplaceFrameBuffer(FrameBufferUsageRENDEROUT, 0, data_type::dtype<vmbyte4>(), ("mesh render out frame buffer : defined in vismtv_inbuilt_renderergpudx module")))
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
	GpuResDescriptor gpuResourcesFB_DESCs[__FB_COUNT_CS];
	GpuResource gpuResourcesFB_ARCHs[__FB_COUNT_CS];
	GpuResDescriptor gpuResourcesDEPTHSTENCIL_DESCs[__DS_COUNT];
	GpuResource gpuResourcesDEPTHSTENCIL_ARCHs[__DS_COUNT];
	GpuResDescriptor gpuResourceMERGE_CS_DESCs[__EXRWB_COUNT];
	GpuResource gpuResourceMERGE_CS_ARCHs[__EXRWB_COUNT];
	GpuResDescriptor gpuResourceMERGE_PS_DESCs[__EXFB_COUNT];
	GpuResource gpuResourceMERGE_PS_ARCHs[__EXFB_COUNT];

#ifdef __CS_5_0_SUPPORT
	bool bIsAvailableCS = true;
	int iNumCommonFBs = __FB_COUNT_CS;
	int iNumShadowFBs = __SM_COUNT_CS;
#else 
	bool bIsAvailableCS = false;
	int iNumCommonFBs = __FB_COUNT_PS;
	int iNumShadowFBs = __SM_COUNT_PS;
#endif
	MakeFBDescriptions(bIsAvailableCS,
		gpuResourcesFB_DESCs,
		gpuResourcesDEPTHSTENCIL_DESCs,
		gpuResourceMERGE_PS_DESCs,
		gpuResourceMERGE_CS_DESCs, NULL,
		pCMergeIObject->GetObjectID());

	bool bInitGen = false;
	for (int i = 0; i < iNumCommonFBs; i++)
	{
		if (!pCGpuManager->GetGpuResourceArchive(&gpuResourcesFB_DESCs[i], &gpuResourcesFB_ARCHs[i]))
		{
			pCGpuManager->GenerateGpuResource(&gpuResourcesFB_DESCs[i], pCMergeIObject, &gpuResourcesFB_ARCHs[i], NULL);
			bInitGen = true;
		}
	}
	for (int i = 0; i < __DS_COUNT; i++)
	{
		if (!pCGpuManager->GetGpuResourceArchive(&gpuResourcesDEPTHSTENCIL_DESCs[i], &gpuResourcesDEPTHSTENCIL_ARCHs[i]))
		{
			pCGpuManager->GenerateGpuResource(&gpuResourcesDEPTHSTENCIL_DESCs[i], pCMergeIObject, &gpuResourcesDEPTHSTENCIL_ARCHs[i], NULL);
			bInitGen = true;
		}
	}

#ifdef __CS_5_0_SUPPORT
	for (int i = 0; i < __EXRWB_COUNT; i++)
	{
		if (!pCGpuManager->GetGpuResourceArchive(&gpuResourceMERGE_CS_DESCs[i], &gpuResourceMERGE_CS_ARCHs[i]))
		{
			pCGpuManager->GenerateGpuResource(&gpuResourceMERGE_CS_DESCs[i], pCMergeIObject, &gpuResourceMERGE_CS_ARCHs[i], NULL);
			bInitGen = true;
		}
	}
#else
	for (int i = 0; i < __EXFB_COUNT; i++)
	{
		if (!pCGpuManager->GetGpuResourceArchive(&gpuResourceMERGE_PS_DESCs[i], &gpuResourceMERGE_PS_ARCHs[i]))
		{
			pCGpuManager->GenerateGpuResource(&gpuResourceMERGE_PS_DESCs[i], pCMergeIObject, &gpuResourceMERGE_PS_ARCHs[i], NULL);
			bInitGen = true;
		}
	}
#endif

	vmint2 i2SizeScreenCurrent, i2SizeScreenOld = vmint2(0, 0);
	pCMergeIObject->GetFrameBufferInfo(&i2SizeScreenCurrent);
	if (bInitGen)
		pCMergeIObject->RegisterCustomParameter(("_int2_PreviousScreenSize"), i2SizeScreenCurrent);
	pCMergeIObject->GetCustomParameter(("_int2_PreviousScreenSize"), data_type::dtype<vmint2>(), &i2SizeScreenOld);
	if (i2SizeScreenCurrent.x != i2SizeScreenOld.x || i2SizeScreenCurrent.y != i2SizeScreenOld.y)
	{
		pCGpuManager->ReleaseGpuResourcesRelatedObject(pCMergeIObject->GetObjectID());	// System Out 포함 //

		for (int i = 0; i < iNumCommonFBs; i++)
			pCGpuManager->GenerateGpuResource(&gpuResourcesFB_DESCs[i], pCMergeIObject, &gpuResourcesFB_ARCHs[i], NULL);

		for (int i = 0; i < __DS_COUNT; i++)
			pCGpuManager->GenerateGpuResource(&gpuResourcesDEPTHSTENCIL_DESCs[i], pCMergeIObject, &gpuResourcesDEPTHSTENCIL_ARCHs[i], NULL);

#ifdef __CS_5_0_SUPPORT
		for (int i = 0; i < __EXRWB_COUNT; i++)
			pCGpuManager->GenerateGpuResource(&gpuResourceMERGE_CS_DESCs[i], pCMergeIObject, &gpuResourceMERGE_CS_ARCHs[i], NULL);
#else
		for (int i = 0; i < __EXFB_COUNT; i++)
			pCGpuManager->GenerateGpuResource(&gpuResourceMERGE_PS_DESCs[i], pCMergeIObject, &gpuResourceMERGE_PS_ARCHs[i], NULL);
#endif

		pCMergeIObject->RegisterCustomParameter("_int2_PreviousScreenSize", i2SizeScreenCurrent);
	}
	pCMergeIObject->RegisterCustomParameter("_int2_PreviousScreenSize", i2SizeScreenCurrent);
#pragma endregion // IOBJECT GPU

#ifdef __CS_5_0_SUPPORT
	vector<void*>* pvtr_Layers_UAV = &gpuResourceMERGE_CS_ARCHs[__EXRWB_UAV_LAYERS].vtrPtrs;
	vector<void*>* pvtr_Layers_SRV = &gpuResourceMERGE_CS_ARCHs[__EXRWB_SRV_LAYERS].vtrPtrs;
	ID3D11UnorderedAccessView* pUAV_Merge_PingpongCS[2] = { (ID3D11UnorderedAccessView*)pvtr_Layers_UAV->at(0), (ID3D11UnorderedAccessView*)pvtr_Layers_UAV->at(1) };
	ID3D11ShaderResourceView* pSRV_Merge_PingpongCS[2] = { (ID3D11ShaderResourceView*)pvtr_Layers_SRV->at(0), (ID3D11ShaderResourceView*)pvtr_Layers_SRV->at(1) };
#else
	vector<void*>* pvtr_RGBA_RTV = &gpuResourceMERGE_PS_ARCHs[__EXFB_RTV_RGBA_LAYERS].vtrPtrs;
	vector<void*>* pvtr_RGBA_SRV = &gpuResourceMERGE_PS_ARCHs[__EXFB_SRV_RGBA_LAYERS].vtrPtrs;
	vector<void*>* pvtr_DepthCS_RTV = &gpuResourceMERGE_PS_ARCHs[__EXFB_RTV_DEPTHCS_LAYERS].vtrPtrs;
	vector<void*>* pvtr_DepthCS_SRV = &gpuResourceMERGE_PS_ARCHs[__EXFB_SRV_DEPTHCS_LAYERS].vtrPtrs;
	ID3D11RenderTargetView* pRTV_Merge_RGBA_PingpongPS[2] = { (ID3D11RenderTargetView*)pvtr_RGBA_RTV->at(0), (ID3D11RenderTargetView*)pvtr_RGBA_RTV->at(1) };
	ID3D11ShaderResourceView* pSRV_Merge_RGBA_PingpongPS[2] = { (ID3D11ShaderResourceView*)pvtr_RGBA_SRV->at(0), (ID3D11ShaderResourceView*)pvtr_RGBA_SRV->at(1) };
	ID3D11RenderTargetView* pRTV_Merge_DepthCS_PingpongPS[2] = { (ID3D11RenderTargetView*)pvtr_DepthCS_RTV->at(0), (ID3D11RenderTargetView*)pvtr_DepthCS_RTV->at(1) };
	ID3D11ShaderResourceView* pSRV_Merge_DepthCS_PingpongPS[2] = { (ID3D11ShaderResourceView*)pvtr_DepthCS_SRV->at(0), (ID3D11ShaderResourceView*)pvtr_DepthCS_SRV->at(1) };
#endif

#pragma region // Presetting of VxObject
	// _bool_IsForcedPerRTarget
	// _bool_IsForcedDepthBias
	// _bool_IsForcedCullOff
	// _int_NumTexureLayers
	map<int, GpuResource> mapGpuResourceArchiveBufferVertex;
	map<int, GpuResource> mapGpuResourceArchiveBufferIndex;
	map<int, GpuResource> mapGPUResSRVs;
	map<int, GpuResource> mapVolumeBlockResSRVs;
	uint uiNumPrimitiveObjects = (uint)vtrInputPrimitives.size();
	float fLengthDiagonalMax = 0;

	vector<RenderingObject> vtrValidPrimitivesPriorOpaque;
	vector<RenderingObject> vtrValidPrimitivesPerRTartget;
	for (uint i = 0; i < uiNumPrimitiveObjects; i++)
	{
		VmVObjectPrimitive* pCVMeshObj = (VmVObjectPrimitive*)vtrInputPrimitives[i];
		if (pCVMeshObj->IsDefined() == false)
			continue;

		int iMeshObjID = pCVMeshObj->GetObjectID();

		PrimitiveData* pstMeshArchive = pCVMeshObj->GetPrimitiveData();
		vmmat44f matOS2WS = pCVMeshObj->GetMatrixOS2WSf();

		vmfloat3 f3PosAaBbMinWS, f3PosAaBbMaxWS;
		fTransformPoint(&f3PosAaBbMinWS, &(vmfloat3)pstMeshArchive->aabb_os.pos_min, &matOS2WS);
		fTransformPoint(&f3PosAaBbMaxWS, &(vmfloat3)pstMeshArchive->aabb_os.pos_max, &matOS2WS);
		fLengthDiagonalMax = max(fLengthDiagonalMax, fLengthVector(&(f3PosAaBbMinWS - f3PosAaBbMaxWS)));

		bool bIsVisibleItem = true;
		pCVMeshObj->GetCustomParameter("_bool_visibility", data_type::dtype<bool>(), &bIsVisibleItem);
		pCLObjectForParameters->GetDstObjValue(iMeshObjID, "_bool_visibility", &bIsVisibleItem);
		if (!bIsVisibleItem)
			continue;

		int iVolumeID = 0, iTObjectID = 0;
		pCLObjectForParameters->GetDstObjValue(iMeshObjID, ("_int_AssociatedVolumeID"), &iVolumeID);
		pCLObjectForParameters->GetDstObjValue(iMeshObjID, ("_int_AssociatedTObjectID"), &iTObjectID);

		RegisterVolumeRes(iVolumeID, iTObjectID, pCLObjectForParameters, pCGpuManager, pdx11DeviceImmContext, bInitGen,
			mapAssociatedObjects, mapGPUResSRVs, pstLocalProgress);

		GpuResDescriptor stGpuResourcePrimitiveVertexDESC, stGpuResourcePrimitiveIndexDESC;
		stGpuResourcePrimitiveVertexDESC.sdk_type = GpuSdkTypeDX11;
		stGpuResourcePrimitiveVertexDESC.dtype = data_type::dtype<vmfloat3>();
		stGpuResourcePrimitiveVertexDESC.res_type = ResourceTypeDX11RESOURCE;
		stGpuResourcePrimitiveVertexDESC.usage_specifier = ("BUFFER_PRIMITIVE_VERTEX_LIST");
		stGpuResourcePrimitiveVertexDESC.misc = 0;
		stGpuResourcePrimitiveVertexDESC.src_obj_id = pCVMeshObj->GetObjectID();
		stGpuResourcePrimitiveIndexDESC = stGpuResourcePrimitiveVertexDESC;
		stGpuResourcePrimitiveIndexDESC.dtype = data_type::dtype<uint>();
		stGpuResourcePrimitiveIndexDESC.usage_specifier = ("BUFFER_PRIMITIVE_INDEX_LIST");

		GpuResource stGpuResourceBufferVertex, stGpuResourceBufferIndex;
		if (!pCGpuManager->GetGpuResourceArchive(&stGpuResourcePrimitiveVertexDESC, &stGpuResourceBufferVertex))
		{
			if (!pCGpuManager->GenerateGpuResource(&stGpuResourcePrimitiveVertexDESC, pCVMeshObj, &stGpuResourceBufferVertex, NULL))
				return false;
		}
		mapGpuResourceArchiveBufferVertex.insert(pair<int, GpuResource>(iMeshObjID, stGpuResourceBufferVertex));

		if (pstMeshArchive->vidx_buffer != NULL)
		{
			if (stGpuResourceBufferVertex.vtrPtrs.at(0) == NULL)
				return false;

			if (!pCGpuManager->GetGpuResourceArchive(&stGpuResourcePrimitiveIndexDESC, &stGpuResourceBufferIndex))
			{
				if (!pCGpuManager->GenerateGpuResource(&stGpuResourcePrimitiveIndexDESC, pCVMeshObj, &stGpuResourceBufferIndex, NULL))
					return false;
			}
		}
		mapGpuResourceArchiveBufferIndex.insert(pair<int, GpuResource>(iMeshObjID, stGpuResourceBufferIndex));

		//////////////////////////////////////////////
		// Register Valid Objects to Rendering List //
		vmdouble4 d4Color(1.), d4ColorWire(1.);
		pCVMeshObj->GetCustomParameter("_double4_color", data_type::dtype<vmdouble4>(), &d4Color);
		pCLObjectForParameters->GetDstObjValue(iMeshObjID, "_double4_color", &d4Color);
		bool bIsWireframe = false;
		if (pstMeshArchive->ptype == PrimitiveTypeTRIANGLE)
			pCVMeshObj->GetPrimitiveWireframeVisibilityColor(&bIsWireframe, &d4ColorWire);
		vmfloat4 f4Color(d4Color), f4ColorWire(d4ColorWire);

		bool bIsObjectCMM = false;
		pCVMeshObj->GetCustomParameter(("_bool_IsObjectCMM"), data_type::dtype<bool>(), &bIsObjectCMM);

		RenderingObject stRenderingObj;
		stRenderingObj.pCMesh = pCVMeshObj;
		if (bIsWireframe)
		{
			stRenderingObj.bIsWireframe = true;
			stRenderingObj.f4Color = f4ColorWire;
			stRenderingObj.f4Color.w = 1.f;
			stRenderingObj.iNumSelfTransparentRenderPass = 1;
			vtrValidPrimitivesPriorOpaque.push_back(stRenderingObj);	// Copy
			if (pstMeshArchive->ptype != PrimitiveTypeTRIANGLE)
				continue;
		}

		stRenderingObj.bIsWireframe = false;
		stRenderingObj.f4Color = f4Color;
		if (f4Color.w > 0.99f && !bIsObjectCMM)
		{
			stRenderingObj.iNumSelfTransparentRenderPass = 1;
			bool bIsForcedPerRTarget = false;
			pCVMeshObj->GetCustomParameter("_bool_IsForcedPerRTarget", data_type::dtype<bool>(), &bIsForcedPerRTarget);
			if (bIsForcedPerRTarget || bIsGlobalForcedPerRTarget)
				vtrValidPrimitivesPerRTartget.push_back(stRenderingObj);
			else
				vtrValidPrimitivesPriorOpaque.push_back(stRenderingObj);
		}
		else
		{
			if (f4Color.w > 1.f / 255.f)
			{
				int iObjectNumSelfTransparentRenderPass = iMaxNumSelfTransparentRenderPass;
				pCLObjectForParameters->GetDstObjValue(iMeshObjID, ("_int_NumSelfTransparentRenderPass"), &iObjectNumSelfTransparentRenderPass);

				if (pstMeshArchive->num_prims < 1000)
					iObjectNumSelfTransparentRenderPass = 1;
				stRenderingObj.iNumSelfTransparentRenderPass = min(iObjectNumSelfTransparentRenderPass, 2);
				vtrValidPrimitivesPerRTartget.push_back(stRenderingObj);
			}
		}
	}
#pragma endregion // Presetting of VxObject

	if (dVThickness < 0)
	{
		if (fLengthDiagonalMax == 0)
		{
			dVThickness = 0.001;
		}
		else
		{
			dVThickness = fLengthDiagonalMax * 0.005;// min(fLengthDiagonalMax * 0.005, 0.01);
		}
	}
	float fVZThickness = (float)dVThickness;

#pragma region // Camera & Light Setting
	uint uiNumGridX = (uint)ceil(i2SizeScreenCurrent.x / (float)__BLOCKSIZE);
	uint uiNumGridY = (uint)ceil(i2SizeScreenCurrent.y / (float)__BLOCKSIZE);

	VmCObject* pCCObject = pCMergeIObject->GetCameraObject();
	vmfloat3 f3PosEyeWS, f3VecViewWS, f3PosLightWS, f3VecLightWS;
	pCCObject->GetCameraExtStatef(&f3PosEyeWS, &f3VecViewWS, NULL);

	CB_VrCameraState cbVrCamState;
	HDx11ComputeConstantBufferVrCamera(&cbVrCamState, pCCObject, i2SizeScreenCurrent, &_fncontainer->vmparams);
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
	cbSrCamState.dxmatSS2WS = *(D3DXMATRIX*)&matSS2WS;
	/////D3DXMatrixTranspose(&cbSrCamState.matSS2WS, &cbSrCamState.matSS2WS);
	cbSrCamState.dxmatWS2PS = *(D3DXMATRIX*)&(matWS2CS * matCS2PS);
	/////D3DXMatrixTranspose(&cbSrCamState.matWS2PS, &cbSrCamState.matWS2PS);

	cbSrCamState.f3PosEyeWS = *(D3DXVECTOR3*)&f3PosEyeWS;
	cbSrCamState.f3VecViewWS = *(D3DXVECTOR3*)&f3VecViewWS;
	cbSrCamState.f3PosLightWS = *(D3DXVECTOR3*)&f3PosLightWS;
	cbSrCamState.f3VecLightWS = *(D3DXVECTOR3*)&f3VecLightWS;
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

#ifdef __CS_5_0_SUPPORT
	pdx11DeviceImmContext->CSSetConstantBuffers(1, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_CAMSTATE]);
#endif

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
	ID3D11ShaderResourceView* pdx11SRV_RenderOuts[NUM_TEXRT_LAYERS] = { NULL };
	ID3D11ShaderResourceView* pdx11SRV_DepthCSs[NUM_TEXRT_LAYERS] = { NULL };
	ID3D11RenderTargetView* pdx11RTV_RenderOuts[NUM_TEXRT_LAYERS] = { NULL };
	ID3D11RenderTargetView* pdx11RTV_DepthCSs[NUM_TEXRT_LAYERS] = { NULL };
	for (int i = 0; i < NUM_TEXRT_LAYERS; i++)
	{
		pdx11SRV_RenderOuts[i] = (ID3D11ShaderResourceView*)gpuResourcesFB_ARCHs[__FB_SRV_RENDEROUT].vtrPtrs.at(i);
		pdx11SRV_DepthCSs[i] = (ID3D11ShaderResourceView*)gpuResourcesFB_ARCHs[__FB_SRV_DEPTHCS].vtrPtrs.at(i);

		pdx11RTV_RenderOuts[i] = (ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_RENDEROUT].vtrPtrs.at(i);
		pdx11RTV_DepthCSs[i] = (ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_DEPTHCS].vtrPtrs.at(i);
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

	float fClearColor[4] = { 0 };
	float fClearDepth[4] = { FLT_MAX };
	// Clear RTT //
	for (int i = 0; i < NUM_TEXRT_LAYERS; i++)
	{
		pdx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_RENDEROUT].vtrPtrs.at(i), fClearColor);
		pdx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_DEPTHCS].vtrPtrs.at(i), fClearDepth);
	}
	// Clear Depth Stencil //
	ID3D11DepthStencilView* pdx11DSV = (ID3D11DepthStencilView*)gpuResourcesDEPTHSTENCIL_ARCHs[__DS_DSV_DEPTH].vtrPtrs.at(0);
	pdx11DeviceImmContext->ClearDepthStencilView(pdx11DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

#ifdef __CS_5_0_SUPPORT
	uint uiClearVlaues[4] = { 0 };

	// Clear Merge Buffers //
	for (int i = 0; i < 2; i++)
		pdx11DeviceImmContext->ClearUnorderedAccessViewUint(pUAV_Merge_PingpongCS[i], uiClearVlaues);
#else
	// Clear Merge Buffers //
	for (int i = 0; i < 2; i++)
	{
		pdx11DeviceImmContext->ClearRenderTargetView(pRTV_Merge_RGBA_PingpongPS[i], fClearColor);
		pdx11DeviceImmContext->ClearRenderTargetView(pRTV_Merge_DepthCS_PingpongPS[i], fClearDepth);
	}
#endif
#pragma endregion // FrameBuffer Setting

#pragma region // Other Presetting For Shaders

	pdx11DeviceImmContext->VSSetSamplers(0, 1, &pdx11CommonParams->pdx11SStates[__SState_LINEAR_ZEROBORDER]);
	pdx11DeviceImmContext->VSSetSamplers(1, 1, &pdx11CommonParams->pdx11SStates[__SState_POINT_ZEROBORDER]);
	pdx11DeviceImmContext->PSSetSamplers(0, 1, &pdx11CommonParams->pdx11SStates[__SState_LINEAR_ZEROBORDER]);
	pdx11DeviceImmContext->PSSetSamplers(1, 1, &pdx11CommonParams->pdx11SStates[__SState_POINT_ZEROBORDER]);

	// Proxy Setting //
#ifdef __CS_5_0_SUPPORT
	D3D11_MAPPED_SUBRESOURCE mappedProxyState;
	pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_PROXY], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedProxyState);
	CB_SrProxy* pCBProxyParam = (CB_SrProxy*)mappedProxyState.pData;
	pCBProxyParam->fVZThickness = fVZThickness;
	pCBProxyParam->uiGridOffsetX = cbVrCamState.uiGridOffsetX;
	pCBProxyParam->uiScreenSizeX = cbVrCamState.uiScreenSizeX;
	pCBProxyParam->uiScreenSizeY = cbVrCamState.uiScreenSizeY;
	pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_PROXY], 0);
	pdx11DeviceImmContext->CSSetConstantBuffers(10, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_PROXY]);
#else
	D3DXMATRIX dxmatWS2CS, dxmatWS2PS;
	D3DXMatrixLookAtRH(&dxmatWS2CS, &D3DXVECTOR3(0, 0, 1.f), &D3DXVECTOR3(0, 0, 0), &D3DXVECTOR3(0, 1.f, 0));
	D3DXMatrixOrthoRH(&dxmatWS2PS, 1.f, 1.f, 0.f, 2.f);
	D3DXMATRIX dxmatOS2PS = dxmatWS2CS * dxmatWS2PS;
	/////D3DXMatrixTranspose(&dxmatOS2PS, &dxmatOS2PS);

	D3D11_MAPPED_SUBRESOURCE mappedProxyState;
	pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_PROXY], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedProxyState);
	CB_SrProxy* pCBProxyParam = (CB_SrProxy*)mappedProxyState.pData;
	pCBProxyParam->fVZThickness = fVZThickness;
	pCBProxyParam->uiGridOffsetX = cbVrCamState.uiGridOffsetX;
	pCBProxyParam->uiScreenSizeX = cbVrCamState.uiScreenSizeX;
	pCBProxyParam->uiScreenSizeY = cbVrCamState.uiScreenSizeY;
	pCBProxyParam->dxmatOS2PS = dxmatOS2PS;

	pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_SR_PROXY], 0);
	pdx11DeviceImmContext->VSSetConstantBuffers(10, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_PROXY]);
	pdx11DeviceImmContext->PSSetConstantBuffers(10, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_SR_PROXY]);
#endif

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
		bool bIsObjectCMM = false;
		bool bIsAirwayAnalysis = false;
		bool bIsDashedLine = false;
		bool bIsInvertColorDashedLine = false;
		bool bIsInvertPlaneDirection = false;
		bool bClipFree = false;
		bool bApplyShadingFactors = false;
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
		pCPrimitiveObject->GetCustomParameter("_bool_IsObjectCMM", data_type::dtype<bool>(), &bIsObjectCMM);
		pCPrimitiveObject->GetCustomParameter("_bool_IsDashed", data_type::dtype<bool>(), &bIsDashedLine);
		pCPrimitiveObject->GetCustomParameter("_bool_IsInvertColorDashLine", data_type::dtype<bool>(), &bIsInvertColorDashedLine);
		pCPrimitiveObject->GetCustomParameter("_bool_IsInvertPlaneDirection", data_type::dtype<bool>(), &bIsInvertPlaneDirection);

#define __RENDERER_WITHOUT_VOLUME 0
#define __RENDERER_VOLUME_SAMPLE_AND_MAP 1
#define __RENDERER_DIFFERENCE_DIVIATION_FROM_VOLUME 2
#define __RENDERER_IMAGE_STACK_MAP_PER_MESHSPRITE 3

		int iRendererMode = __RENDERER_WITHOUT_VOLUME;
		pCLObjectForParameters->GetDstObjValue(iMeshObjID, "_int_RendererMode", &iRendererMode);
		pCLObjectForParameters->GetDstObjValue(iMeshObjID, "_int_AssociatedVolumeID", &iVolumeID);
		pCLObjectForParameters->GetDstObjValue(iMeshObjID, "_int_AssociatedTObjectID", &iTObjectID);
		pCLObjectForParameters->GetDstObjValue(iMeshObjID, "_double_LineDashInterval", &dLineDashedInterval);
		pCLObjectForParameters->GetDstObjValue(iMeshObjID, "_double4_ShadingFactors", &d4ShadingFactors);
		pCLObjectForParameters->GetDstObjValue(iMeshObjID, "_int_ClippingMode", &iClippingMode);	// 0 : No, 1 : CLIPPLANE, 2 : CLIPBOX, 3 : BOTH
		pCLObjectForParameters->GetDstObjValue(iMeshObjID, "_double3_PosClipPlaneWS", &d3PosClipPlaneWS);
		pCLObjectForParameters->GetDstObjValue(iMeshObjID, "_double3_VecClipPlaneWS", &d3VecClipPlaneWS);
		pCLObjectForParameters->GetDstObjValue(iMeshObjID, "_double3_PosClipBoxMaxWS", &d3PosOrthoMaxClipBoxWS);
		pCLObjectForParameters->GetDstObjValue(iMeshObjID, "_matrix44_MatrixClipWS2BS", &dmatClipBoxWS2BS);
		pCLObjectForParameters->GetDstObjValue(iMeshObjID, "_bool_IsAirwayAnalysis", &bIsAirwayAnalysis);
		pCLObjectForParameters->GetDstObjValue(iMeshObjID, "_double_PointThickness", &dPointThickness);
		pCLObjectForParameters->GetDstObjValue(iMeshObjID, "_double_LineThickness", &dLineThickness);

		pCPrimitiveObject->GetCustomParameter(("_bool_ClipFree"), data_type::dtype<bool>(), &bClipFree);
		pCPrimitiveObject->GetCustomParameter(("_bool_ApplyShadingFactors"), data_type::dtype<bool>(), &bApplyShadingFactors);

		if (bClipFree) iClippingMode = 0;
		if (!bApplyShadingFactors) d4ShadingFactors = d4GlobalShadingFactors;

		//iObjectNumSelfTransparentRenderPass = 4;//

		auto itrGpuVolume = mapGPUResSRVs.find(iVolumeID);
		auto itrGpuTObject = mapGPUResSRVs.find(iTObjectID);
		if (itrGpuVolume == mapGPUResSRVs.end() || itrGpuTObject == mapGPUResSRVs.end())
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

			auto itrVolume = mapAssociatedObjects.find(iVolumeID);
			auto itrTObject = mapAssociatedObjects.find(iTObjectID);
			VmVObjectVolume* pCVolume = (VmVObjectVolume*)itrVolume->second;
			VmTObject* pCTObject = (VmTObject*)itrTObject->second;

			bool bIsDownSample = false;
			if (itrGpuVolume->second.sample_interval.x > 1.f ||
				itrGpuVolume->second.sample_interval.y > 1.f ||
				itrGpuVolume->second.sample_interval.z > 1.f)
				bIsDownSample = true;

			CB_VrVolumeObject cbVrVolumeObj;
			HDx11ComputeConstantBufferVrObject(&cbVrVolumeObj, bIsDownSample, pCVolume, pCCObject, itrGpuVolume->second.logical_res_bytes, pCLObjectForParameters, iVolumeID);
			D3D11_MAPPED_SUBRESOURCE mappedResVolObj;
			pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_VOLOBJ], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResVolObj);
			CB_VrVolumeObject* pCBParamVolumeObj = (CB_VrVolumeObject*)mappedResVolObj.pData;
			memcpy(pCBParamVolumeObj, &cbVrVolumeObj, sizeof(CB_VrVolumeObject));
			pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_VOLOBJ], 0);
			pdx11DeviceImmContext->PSSetConstantBuffers(2, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_VOLOBJ]);

			CB_VrOtf1D cbVrOtf1D;
			HDx11ComputeConstantBufferVrOtf1D(&cbVrOtf1D, pCTObject, 1, &_fncontainer->vmparams);
			D3D11_MAPPED_SUBRESOURCE mappedResOtf1D;
			pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_OTF1D], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResOtf1D);
			CB_VrOtf1D* pCBParamOtf1D = (CB_VrOtf1D*)mappedResOtf1D.pData;
			memcpy(pCBParamOtf1D, &cbVrOtf1D, sizeof(CB_VrOtf1D));
			pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_OTF1D], 0);
			pdx11DeviceImmContext->PSSetConstantBuffers(7, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_OTF1D]);

			if (iRendererMode == __RENDERER_DIFFERENCE_DIVIATION_FROM_VOLUME)
			{
				VolumeBlocks* pstVolBlock0 = pCVolume->GetVolumeBlock(0);
				VolumeBlocks* pstVolBlock1 = pCVolume->GetVolumeBlock(1);
				if (pstVolBlock0 == NULL || pstVolBlock1 == NULL)
					pCVolume->UpdateVolumeMinMaxBlocks();

				int iTObjectForIsoValueID = 0;
				pCLObjectForParameters->GetDstObjValue(iMeshObjID, ("_int_IsoValueTObjectID"), &iTObjectForIsoValueID);

				auto itrIsoValueTObject = mapAssociatedObjects.find(iTObjectForIsoValueID);
				VmTObject* pCTObjectIsoValue = (VmTObject*)itrIsoValueTObject->second;
				TMapData* pstTfArchiveIsoValue = pCTObjectIsoValue->GetTMapData();
				int iIsoValue = pstTfArchiveIsoValue->valid_min_idx.x;

				vmdouble2 d2ActiveValueRangeMinMax(pstTfArchiveIsoValue->valid_min_idx.x, pstTfArchiveIsoValue->valid_max_idx.x);
				if (pstVolBlock0->GetTaggedActivatedBlocks(iTObjectForIsoValueID) == NULL)
					pCVolume->UpdateTagBlocks(iTObjectForIsoValueID, 0, d2ActiveValueRangeMinMax, false);
				if (pstVolBlock1->GetTaggedActivatedBlocks(iTObjectForIsoValueID) == NULL)
					pCVolume->UpdateTagBlocks(iTObjectForIsoValueID, 1, d2ActiveValueRangeMinMax, false);

				vmdouble2 d2DeviationValueMinMax(0, 1);
				pCLObjectForParameters->GetDstObjValue(iMeshObjID, ("_double_DeviationMinValue"), &d2DeviationValueMinMax.x);
				pCLObjectForParameters->GetDstObjValue(iMeshObjID, ("_double_DeviationMaxValue"), &d2DeviationValueMinMax.y);
				int iMappingValueRange = 512;
				pCLObjectForParameters->GetDstObjValue(iMeshObjID, ("_int_MappingValueRange"), &iMappingValueRange);

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

				GpuResource svxGpuResourceBlockTexture, svxGpuResourceBlockSRV;
				int iLevelBlock = 1;
				VolumeBlocks* psvxVolBlock = pCVolume->GetVolumeBlock(iLevelBlock);
				GpuResDescriptor svxGpuResBlockDesc;
				svxGpuResBlockDesc.sdk_type = GpuSdkTypeDX11;
				svxGpuResBlockDesc.res_type = ResourceTypeDX11RESOURCE;
				svxGpuResBlockDesc.usage_specifier = ("TEXTURE3D_VOLUME_BLOCKMAP");
				if (psvxVolBlock->blk_vol_size.x * psvxVolBlock->blk_vol_size.y * psvxVolBlock->blk_vol_size.z < 65536)
					svxGpuResBlockDesc.dtype = data_type::dtype<ushort>();
				else
					svxGpuResBlockDesc.dtype = data_type::dtype<float>();
				svxGpuResBlockDesc.src_obj_id = pCVolume->GetObjectID();
				//svxGpuResBlockDesc.src_obj_id = iTObjectForIsoValueID;
				svxGpuResBlockDesc.misc = ((iTObjectForIsoValueID % 65536) << 16) | iLevelBlock;
				if (!pCGpuManager->GetGpuResourceArchive(&svxGpuResBlockDesc, &svxGpuResourceBlockTexture))
					pCGpuManager->GenerateGpuResource(&svxGpuResBlockDesc, pCVolume, &svxGpuResourceBlockTexture, NULL);

				svxGpuResBlockDesc.res_type = ResourceTypeDX11SRV;
				if (!pCGpuManager->GetGpuResourceArchive(&svxGpuResBlockDesc, &svxGpuResourceBlockSRV))
					pCGpuManager->GenerateGpuResource(&svxGpuResBlockDesc, pCVolume, &svxGpuResourceBlockSRV, NULL);

				pdx11DeviceImmContext->PSSetShaderResources(2, 1, (ID3D11ShaderResourceView**)&svxGpuResourceBlockSRV.vtrPtrs.at(0));

				CB_VrBlockObject cbVrBlock;
				if (svxGpuResBlockDesc.dtype == data_type::dtype<float>())
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
		else if (bIsObjectCMM)
		{
			if (pstPrimitiveArchive->texture_res)
			{
				// GPU Resource Check!! : Text Resource //
				GpuResource stGpuResourceTextTexture, stGpuResourceTextSRV;
				GpuResDescriptor stGpuTextResourceDesc;
				stGpuTextResourceDesc.sdk_type = GpuSdkTypeDX11;
				stGpuTextResourceDesc.dtype = data_type::dtype<vmbyte4>();
				stGpuTextResourceDesc.src_obj_id = pCPrimitiveObject->GetObjectID();
				stGpuTextResourceDesc.usage_specifier = ("TEXTURE2D_CMM_TEXT");
				stGpuTextResourceDesc.res_type = ResourceTypeDX11RESOURCE;
				if (!pCGpuManager->GetGpuResourceArchive(&stGpuTextResourceDesc, &stGpuResourceTextTexture))
					pCGpuManager->GenerateGpuResource(&stGpuTextResourceDesc, pCPrimitiveObject, &stGpuResourceTextTexture, NULL);
				stGpuTextResourceDesc.res_type = ResourceTypeDX11SRV;
				if (!pCGpuManager->GetGpuResourceArchive(&stGpuTextResourceDesc, &stGpuResourceTextSRV))
					pCGpuManager->GenerateGpuResource(&stGpuTextResourceDesc, pCPrimitiveObject, &stGpuResourceTextSRV, NULL);
				pdx11DeviceImmContext->PSSetShaderResources(3, 1, (ID3D11ShaderResourceView**)&stGpuResourceTextSRV.vtrPtrs.at(0));
			}
		}
		else if (bIsAirwayAnalysis)
		{
			auto itrTObject = mapAssociatedObjects.find(iTObjectID);
			if (itrTObject != mapAssociatedObjects.end())
			{
				VmTObject* pCTObject = (VmTObject*)itrTObject->second;
				CB_VrOtf1D cbVrOtf1D;
				HDx11ComputeConstantBufferVrOtf1D(&cbVrOtf1D, pCTObject, 1, &_fncontainer->vmparams);
				D3D11_MAPPED_SUBRESOURCE mappedResOtf1D;
				pdx11DeviceImmContext->Map(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_OTF1D], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResOtf1D);
				CB_VrOtf1D* pCBParamOtf1D = (CB_VrOtf1D*)mappedResOtf1D.pData;
				memcpy(pCBParamOtf1D, &cbVrOtf1D, sizeof(CB_VrOtf1D));
				pdx11DeviceImmContext->Unmap(pdx11CommonParams->pdx11BufConstParameters[__CB_VR_OTF1D], 0);
				pdx11DeviceImmContext->PSSetConstantBuffers(7, 1, &pdx11CommonParams->pdx11BufConstParameters[__CB_VR_OTF1D]);

				vmdouble2 d2DeviationValueMinMax(0, 1);
				pCLObjectForParameters->GetDstObjValue(iMeshObjID, ("_double_DeviationMinValue"), &d2DeviationValueMinMax.x);
				pCLObjectForParameters->GetDstObjValue(iMeshObjID, ("_double_DeviationMaxValue"), &d2DeviationValueMinMax.y);
				int iMappingValueRange = 512;
				pCLObjectForParameters->GetDstObjValue(iMeshObjID, ("_int_MappingValueRange"), &iMappingValueRange);
				int iNumControlPoints = 1;
				pCLObjectForParameters->GetDstObjValue(iMeshObjID, ("_int_NumAirwayControls"), &iNumControlPoints);

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

				RegisterVolumeRes(0, pCTObject->GetObjectID(), pCLObjectForParameters, pCGpuManager, pdx11DeviceImmContext, bInitGen,
					mapAssociatedObjects, mapGPUResSRVs, pstLocalProgress);

#pragma region AIRWAY RESOURCE (STBUFFER) and VIEW
				GpuResDescriptor stGpuResourceAirwayControlsDESC;
				stGpuResourceAirwayControlsDESC.sdk_type = GpuSdkTypeDX11;
				stGpuResourceAirwayControlsDESC.dtype = data_type::dtype<vmfloat4>();
				stGpuResourceAirwayControlsDESC.res_type = ResourceTypeDX11RESOURCE;
				stGpuResourceAirwayControlsDESC.usage_specifier = ("BUFFER_AIRWAY_CONTROLS");
				stGpuResourceAirwayControlsDESC.src_obj_id = pCLObjectForParameters->GetObjectID();
				stGpuResourceAirwayControlsDESC.misc = 0;
				GpuResource stGpuResourceAirwayControlsSTB, stGpuResourceAirwayControlsSRV;

				if (!pCGpuManager->GetGpuResourceArchive(&stGpuResourceAirwayControlsDESC, &stGpuResourceAirwayControlsSTB))
				{
					size_t bytes_tmp;
					vmdouble3* pAirwayControlPoints = NULL;
					pCLObjectForParameters->LoadBufferPtr(("_vlist_DOUBLE3_AirwayPathPointWS"), (void**)&pAirwayControlPoints, bytes_tmp);
					double* pAirwayAreaPoints = NULL;
					pCLObjectForParameters->LoadBufferPtr(("_vlist_DOUBLE_AirwayCrossSectionalAreaWS"), (void**)&pAirwayAreaPoints, bytes_tmp);
					vmdouble3* pAirwayTVectors = NULL;
					pCLObjectForParameters->LoadBufferPtr(("_vlist_DOUBLE3_AirwayPathTangentVectorWS"), (void**)&pAirwayTVectors, bytes_tmp);

					D3D11_BUFFER_DESC descBuf;
					ZeroMemory(&descBuf, sizeof(D3D11_BUFFER_DESC));
					descBuf.ByteWidth = iNumControlPoints * sizeof(vmfloat4);
					descBuf.MiscFlags = NULL;// D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
					descBuf.StructureByteStride = stGpuResourceAirwayControlsDESC.dtype.type_bytes;
					descBuf.BindFlags = D3D11_BIND_SHADER_RESOURCE;
					descBuf.Usage = D3D11_USAGE_DYNAMIC;// D3D11_USAGE_DEFAULT;
					descBuf.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;// NULL;

					ID3D11Buffer* pdx11Buf = NULL;
					if (pdx11Device->CreateBuffer(&descBuf, NULL, &pdx11Buf) != S_OK)
					{
						VMERRORMESSAGE("Called SimpleVR Module's CUSTOM GpuResource - BUFFER_AIRWAY_CONTROLS FAILURE!");
						VMSAFE_RELEASE(pdx11Buf);
						return false;
					}

					D3D11_MAPPED_SUBRESOURCE d11MappedRes;
					pdx11DeviceImmContext->Map((ID3D11Resource*)pdx11Buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes);
					vmfloat4* pf4ControlPointAndArea = (vmfloat4*)d11MappedRes.pData;
					for (int iIndexControl = 0; iIndexControl < iNumControlPoints; iIndexControl++)
					{
						vmfloat3 f3PosPoint = vmfloat3(pAirwayControlPoints[iIndexControl]);
						float fArea = float(pAirwayAreaPoints[iIndexControl]);

						pf4ControlPointAndArea[iIndexControl] = vmfloat4(f3PosPoint.x, f3PosPoint.y, f3PosPoint.z, fArea);
					}
					pdx11DeviceImmContext->Unmap((ID3D11Resource*)pdx11Buf, 0);

					// Register
					stGpuResourceAirwayControlsSTB.Init();
					stGpuResourceAirwayControlsSTB.resource_row_pitch_bytes = descBuf.ByteWidth;
					stGpuResourceAirwayControlsSTB.logical_res_bytes = vmint3(descBuf.ByteWidth, 0, 0);
					stGpuResourceAirwayControlsSTB.vtrPtrs.push_back(pdx11Buf);
					stGpuResourceAirwayControlsSTB.gpu_res_desc = stGpuResourceAirwayControlsDESC;
					stGpuResourceAirwayControlsSTB.recent_used_time = vmhelpers::GetCurrentTimePack();

					pCGpuManager->ReplaceOrAddGpuResource(&stGpuResourceAirwayControlsSTB);
				}

				stGpuResourceAirwayControlsDESC.res_type = ResourceTypeDX11SRV;

				if (!pCGpuManager->GetGpuResourceArchive(&stGpuResourceAirwayControlsDESC, &stGpuResourceAirwayControlsSRV))
				{
					D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
					ZeroMemory(&descSRV, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
					descSRV.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
					descSRV.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
					descSRV.BufferEx.FirstElement = 0;
					descSRV.BufferEx.NumElements = stGpuResourceAirwayControlsSTB.logical_res_bytes.x / stGpuResourceAirwayControlsSTB.gpu_res_desc.dtype.type_bytes;

					vector<void*> vtrViews;
					for (int i = 0; i < (int)(stGpuResourceAirwayControlsSTB.vtrPtrs.size()); i++)
					{
						ID3D11View* pdx11View = NULL;
						if (pdx11Device->CreateShaderResourceView((ID3D11Resource*)stGpuResourceAirwayControlsSTB.vtrPtrs.at(i), &descSRV, (ID3D11ShaderResourceView**)&pdx11View) != S_OK)
						{
							VMERRORMESSAGE("Shader Reousrce <CUSTOM> View Creation Failure! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
							return false;
						}
						vtrViews.push_back(pdx11View);
						stGpuResourceAirwayControlsSRV.Init();
						stGpuResourceAirwayControlsSRV.resource_row_pitch_bytes = stGpuResourceAirwayControlsSTB.resource_row_pitch_bytes;
						stGpuResourceAirwayControlsSRV.resource_slice_pitch_bytes = stGpuResourceAirwayControlsSTB.resource_slice_pitch_bytes;
						stGpuResourceAirwayControlsSRV.logical_res_bytes = stGpuResourceAirwayControlsSTB.logical_res_bytes;
						stGpuResourceAirwayControlsSRV.sample_interval = stGpuResourceAirwayControlsSTB.sample_interval;
						stGpuResourceAirwayControlsSRV.gpu_res_desc = stGpuResourceAirwayControlsSTB.gpu_res_desc;
						stGpuResourceAirwayControlsSRV.gpu_res_desc.res_type = ResourceTypeDX11SRV;
						stGpuResourceAirwayControlsSRV.recent_used_time = vmhelpers::GetCurrentTimePack();
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
		vmmat44f matOS2WS = pCPrimitiveObject->GetMatrixOS2WSf();
#pragma region // Constant Buffers for Each Mesh Object //
		cbPolygonObj.dxmatOS2WS = *(D3DXMATRIX*)&matOS2WS;
		/////D3DXMatrixTranspose(&cbPolygonObj.matOS2WS, &cbPolygonObj.matOS2WS);

		cbPolygonObj.iFlag = 0x3 & iClippingMode;
		vmfloat3 f3ClipBoxMaxBS;
		vmmat44f matClipBoxWS2BS(dmatClipBoxWS2BS);
		fTransformPoint(&f3ClipBoxMaxBS, &vmfloat3(d3PosOrthoMaxClipBoxWS), &matClipBoxWS2BS);
		cbPolygonObj.f3ClipBoxMaxBS = *(D3DXVECTOR3*)&f3ClipBoxMaxBS;
		cbPolygonObj.dxmatClipBoxWS2BS = *(D3DXMATRIX*)&vmmat44f(matClipBoxWS2BS);
		cbPolygonObj.f3PosClipPlaneWS = *(D3DXVECTOR3*)&vmfloat3(d3PosClipPlaneWS);
		cbPolygonObj.f3VecClipPlaneWS = *(D3DXVECTOR3*)&vmfloat3(d3VecClipPlaneWS);
		/////D3DXMatrixTranspose(&cbPolygonObj.matClipBoxWS2BS, &cbPolygonObj.matClipBoxWS2BS);

		if (bIsObjectCMM && pstPrimitiveArchive->texture_res)
		{
			if (colorForcedCmmObj.x >= 0 && colorForcedCmmObj.y >= 0 && colorForcedCmmObj.z >= 0)
				stRenderingObj.f4Color = vmfloat4((float)colorForcedCmmObj.x, (float)colorForcedCmmObj.y, (float)colorForcedCmmObj.z, 1.f);

			vmint3 i3TextureWHN;
			pCPrimitiveObject->GetCustomParameter(("_int3_TextureWHN"), data_type::dtype<vmint3>(), &i3TextureWHN);
			cbPolygonObj.iDummy__0 = i3TextureWHN.z;
			if (i3TextureWHN.z == 1)
			{
				vmfloat3* pf3Positions = pstPrimitiveArchive->GetVerticeDefinition("POSITION");
				vmfloat3 f3Pos0SS, f3Pos1SS, f3Pos2SS;
				vmmat44f matOS2SS = matOS2WS * matWS2SS;

				vmfloat3 f3VecWidth = pf3Positions[1] - pf3Positions[0];
				vmfloat3 f3VecHeight = pf3Positions[2] - pf3Positions[0];
				fTransformVector(&f3VecWidth, &f3VecWidth, &matOS2SS);
				fTransformVector(&f3VecHeight, &f3VecHeight, &matOS2SS);
				fNormalizeVector(&f3VecWidth, &f3VecWidth);
				fNormalizeVector(&f3VecHeight, &f3VecHeight);

				//vmfloat3 f3VecWidth0 = pf3Positions[1] - pf3Positions[0];
				//vmfloat3 f3VecHeight0 = -pf3Positions[2] + pf3Positions[0];
				//vmmat44 matOS2PS = matOS2WS * matWS2CS * matCS2PS;
				//fTransformVector(&f3VecWidth0, &f3VecWidth0, &matOS2PS);
				//fTransformVector(&f3VecHeight0, &f3VecHeight0, &matOS2PS);
				//fNormalizeVector(&f3VecWidth0, &f3VecWidth0);
				//fNormalizeVector(&f3VecHeight0, &f3VecHeight0);

				if (fDotVector(&f3VecWidth, &vmfloat3(1.f, 0, 0)) < 0)
					cbPolygonObj.iFlag |= (0x1 << 9);

				vmfloat3 f3VecNormal;
				fCrossDotVector(&f3VecNormal, &f3VecHeight, &f3VecWidth);
				fCrossDotVector(&f3VecHeight, &f3VecWidth, &f3VecNormal);

				if (fDotVector(&f3VecHeight, &vmfloat3(0, 1.f, 0)) <= 0)
					cbPolygonObj.iFlag |= (0x1 << 10);
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
		cbPolygonObj.dxmatOS2PS = *(D3DXMATRIX*)&matOS2PS;
		/////D3DXMatrixTranspose(&cbPolygonObj.matOS2PS, &cbPolygonObj.matOS2PS);

		CB_VrInterestingRegion cbVrInterestingRegion;
		HDx11ComputeConstantBufferVrInterestingRegion(&cbVrInterestingRegion, pCLObjectForParameters, iVolumeID);
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
		bIsForcedDepthBias = stRenderingObj.bIsWireframe;
		pCPrimitiveObject->GetCustomParameter("_bool_IsForcedDepthBias", data_type::dtype<bool>(), &bIsForcedDepthBias);
		if (bIsForcedDepthBias)
			cbPolygonObj.fDepthForwardBias = fVZThickness;
		else
			cbPolygonObj.fDepthForwardBias = 0;

		cbPolygonObj.f4Color = *(D3DXVECTOR4*)&vmfloat4(stRenderingObj.f4Color);
		vmfloat4 f4ShadingFactor = vmfloat4(d4ShadingFactors);
		cbPolygonObj.f4ShadingFactor = *(D3DXVECTOR4*)&vmfloat4(f4ShadingFactor);
		if (pstPrimitiveArchive->ptype == PrimitiveTypePOINT)
			cbPolygonObj.fDummy__1 = (float)(dPointThickness / 2.);
		else if (pstPrimitiveArchive->ptype == PrimitiveTypeLINE)
			cbPolygonObj.fDummy__1 = (float)(dLineThickness / 2.);

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
			if (pstPrimitiveArchive->GetVerticeDefinition("TEXCOORD0"))
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
						pdx11PS_Target = *ppdx11PS_SRs[__PS_PHONG_GLOBALCOLOR_MAXSHADING];//__PS_PHONG_GLOBALCOLOR
					else
						pdx11PS_Target = *ppdx11PS_SRs[__PS_PHONG_GLOBALCOLOR_MARKERONSURFACE];
				}
				else
				{
					if (cbVrInterestingRegion.iNumRegions == 0)
						pdx11PS_Target = *ppdx11PS_SRs[__PS_PHONG_GLOBALCOLOR_MAXSHADING];
					else
						pdx11PS_Target = *ppdx11PS_SRs[__PS_PHONG_GLOBALCOLOR_MAXSHADING_MARKERONSURFACE];
				}
			}
		}
		else if (pstPrimitiveArchive->GetVerticeDefinition("TEXCOORD0"))
		{
			cbPolygonObj.f4ShadingFactor = D3DXVECTOR4(1.f, 1.f, 1.f, 1.f);

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
			cbPolygonObj.f4ShadingFactor = D3DXVECTOR4(1.f, 1.f, 1.f, 1.f);
			// P
			pdx11InputLayer_Target = pdx11LayoutInputPos;
			if (bIsForcedDepthBias)
				pdx11VS_Target = pdx11VShader_BiasZ_Point;
			else
				pdx11VS_Target = pdx11VShader_Point;
			pdx11PS_Target = *ppdx11PS_SRs[__PS_MAPPING_TEX1];
		}

		if (bIsObjectCMM)
		{
			if (pstPrimitiveArchive->texture_res)
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
			break;
		case PrimitiveTypePOINT:
			ePRIMITIVE_TOPOLOGY = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
			if (dPointThickness > 0)
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
		if (stRenderingObj.bIsWireframe)
		{
			pdx11RState_TargetObj = pdx11CommonParams->pdx11RStates[__RState_WIRE_NONE];
		}
		else
		{
			if (eCullingMode == rxSR_CULL_NONE || bIsForcedCullOff || bIsObjectCMM)
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

		auto itrMapBufferVtx = mapGpuResourceArchiveBufferVertex.find(iMeshObjID);
		ID3D11Buffer* pdx11BufferTargetMesh = (ID3D11Buffer*)itrMapBufferVtx->second.vtrPtrs.at(0);
		ID3D11Buffer* pdx11IndiceTargetMesh = NULL;
		uiStrideSizeInput = sizeof(vmfloat3)*(uint)pstPrimitiveArchive->GetNumVertexDefinitions();
		pdx11DeviceImmContext->IASetVertexBuffers(0, 1, (ID3D11Buffer**)&pdx11BufferTargetMesh, &uiStrideSizeInput, &uiOffset);
		if (pstPrimitiveArchive->vidx_buffer != NULL)
		{
			auto itrMapBufferIdx = mapGpuResourceArchiveBufferIndex.find(iMeshObjID);
			pdx11IndiceTargetMesh = (ID3D11Buffer*)itrMapBufferIdx->second.vtrPtrs.at(0);
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

			ID3D11RenderTargetView* pdx11RTVs[2] = { (ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_RENDEROUT].vtrPtrs.at(iIndexRTT)
				, (ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_DEPTHCS].vtrPtrs.at(iIndexRTT) };

			pdx11DeviceImmContext->OMSetRenderTargets(2, pdx11RTVs, pdx11DSV);
			//pdx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(2, pdx11RTVs, pdx11DSV, 2, 1, (ID3D11UnorderedAccessView**)&resRWBufUAV.vtrPtrs[0], 0);

			// Render Work //
			if (pstPrimitiveArchive->is_stripe || pstPrimitiveArchive->ptype == PrimitiveTypePOINT)
				pdx11DeviceImmContext->Draw(pstPrimitiveArchive->num_vtx, 0);
			else
				pdx11DeviceImmContext->DrawIndexed(pstPrimitiveArchive->num_vidx, 0, 0);

			if (iRENDERER_LOOP == 0 || iNumTexureLayers == 1)
				continue;

			if (iModRTLayer == 0)
			{
#ifdef __CS_5_0_SUPPORT
				int iRSA_Index_Offset_Prev = RTTandLayersToLayersCS(pdx11DeviceImmContext, uiNumGridX, uiNumGridY,
					pdx11SRV_RenderOuts, pdx11SRV_DepthCSs, iCountMerging,
					pUAV_Merge_PingpongCS, pSRV_Merge_PingpongCS,
					pdx11CommonParams, ppdx11CS_Merges, pdx11SRV_4NULLs, pdx11SRV_2NULLs, pdx11RTV_2NULLs, iMergeLevel);

				// Clear //
				pdx11DeviceImmContext->ClearUnorderedAccessViewUint(pUAV_Merge_PingpongCS[iRSA_Index_Offset_Prev], uiClearVlaues);
#else
				int iRSA_Index_Offset_Prev = RTTandLayersToLayers(pdx11DeviceImmContext,
					pdx11SRV_RenderOuts, pdx11SRV_DepthCSs, iCountMerging,
					pRTV_Merge_RGBA_PingpongPS, pSRV_Merge_RGBA_PingpongPS,
					pRTV_Merge_DepthCS_PingpongPS, pSRV_Merge_DepthCS_PingpongPS,
					pdx11LayoutInputPos, pdx11CommonParams, pdx11VShader_Proxy,
					pdx11BufProxyRect, ppdx11PS_Merges, pdx11SRV_4NULLs, pdx11SRV_2NULLs, pdx11RTV_2NULLs, iMergeLevel);

				// Restore //
				uiStrideSizeInput = sizeof(vmfloat3)*(uint)pstPrimitiveArchive->GetNumVertexDefinitions();
				uiOffset = 0;
				pdx11DeviceImmContext->IASetVertexBuffers(0, 1, (ID3D11Buffer**)&pdx11BufferTargetMesh, &uiStrideSizeInput, &uiOffset);
				pdx11DeviceImmContext->IASetPrimitiveTopology(ePRIMITIVE_TOPOLOGY);
				pdx11DeviceImmContext->IASetInputLayout(pdx11InputLayer_Target);
				pdx11DeviceImmContext->IASetIndexBuffer(pdx11IndiceTargetMesh, DXGI_FORMAT_R32_UINT, 0);
				pdx11DeviceImmContext->RSSetState(pdx11RState_TargetObj);
				pdx11DeviceImmContext->VSSetShader(pdx11VS_Target, NULL, 0);
				pdx11DeviceImmContext->GSSetShader(pdx11GS_Target, NULL, 0);
				pdx11DeviceImmContext->PSSetShader(pdx11PS_Target, NULL, 0);

				// Clear //
				pdx11DeviceImmContext->ClearRenderTargetView(pRTV_Merge_RGBA_PingpongPS[iRSA_Index_Offset_Prev], fClearColor);
				pdx11DeviceImmContext->ClearRenderTargetView(pRTV_Merge_DepthCS_PingpongPS[iRSA_Index_Offset_Prev], fClearDepth);
#endif
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
#ifdef __CS_5_0_SUPPORT
		RTTandLayersToLayersCS(pdx11DeviceImmContext, uiNumGridX, uiNumGridY,
			pdx11SRV_RenderOuts, pdx11SRV_DepthCSs, iCountMerging,
			pUAV_Merge_PingpongCS, pSRV_Merge_PingpongCS,
			pdx11CommonParams, ppdx11CS_Merges, pdx11SRV_4NULLs, pdx11SRV_2NULLs, pdx11RTV_2NULLs, iMergeLevel);
#else
		RTTandLayersToLayers(pdx11DeviceImmContext,
			pdx11SRV_RenderOuts, pdx11SRV_DepthCSs, iCountMerging,
			pRTV_Merge_RGBA_PingpongPS, pSRV_Merge_RGBA_PingpongPS,
			pRTV_Merge_DepthCS_PingpongPS, pSRV_Merge_DepthCS_PingpongPS,
			pdx11LayoutInputPos, pdx11CommonParams, pdx11VShader_Proxy,
			pdx11BufProxyRect, ppdx11PS_Merges, pdx11SRV_4NULLs, pdx11SRV_2NULLs, pdx11RTV_2NULLs, iMergeLevel);
#endif
		iCountMerging++;
	}
#pragma endregion // Final Rearrangement

	pdx11DeviceImmContext->Flush();
	pCMergeIObject->RegisterCustomParameter(("_int_CountMerging"), iCountMerging);

	//printf("# Textures : %d, # Drawing : %d, # RTBuffer Change : %d, # Merging : %d\n", iNumTexureLayers, iCountRendering, iCountRTBuffers, iCountMerging);

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
			memset(py4Buffer, 0, i2SizeScreenCurrent.x * i2SizeScreenCurrent.y * sizeof(vmbyte4));
			memset(pfBuffer, 0x77, i2SizeScreenCurrent.x * i2SizeScreenCurrent.y * sizeof(float));
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

#ifdef __CS_5_0_SUPPORT
				ID3D11ShaderResourceView* pdx11SRV_Layers = pSRV_Merge_PingpongCS[iRSA_Index_Offset_Prev];
				pdx11DeviceImmContext->CSSetShaderResources(50, 1, &pdx11SRV_Layers);

				ID3D11UnorderedAccessView* pdx11UAV_Renderout = (ID3D11UnorderedAccessView*)gpuResourcesFB_ARCHs[__FB_UAV_RENDEROUT].vtrPtrs.at(0);
				ID3D11UnorderedAccessView* pdx11UAV_DepthCS = (ID3D11UnorderedAccessView*)gpuResourcesFB_ARCHs[__FB_UAV_DEPTHCS].vtrPtrs.at(0);

				// ???? //
				UINT UAVInitialCounts = 0;
				pdx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, &pdx11UAV_Renderout, &UAVInitialCounts);
				pdx11DeviceImmContext->CSSetUnorderedAccessViews(2, 1, &pdx11UAV_DepthCS, &UAVInitialCounts);
				pdx11DeviceImmContext->CSSetShader(*ppdx11CS_Merges[__CS_MERGE_LAYERS_TO_RENDEROUT], NULL, 0);

				pdx11DeviceImmContext->Dispatch(uiNumGridX, uiNumGridY, 1);
#else
				ID3D11ShaderResourceView* pdx11SRV_RGBA_RSA = pSRV_Merge_RGBA_PingpongPS[iRSA_Index_Offset_Prev];
				ID3D11ShaderResourceView* pdx11SRV_DepthCS_RSA = pSRV_Merge_DepthCS_PingpongPS[iRSA_Index_Offset_Prev];

				pdx11DeviceImmContext->PSSetShaderResources(50, 1, &pdx11SRV_RGBA_RSA);
				pdx11DeviceImmContext->PSSetShaderResources(51, 1, &pdx11SRV_DepthCS_RSA);

				ID3D11RenderTargetView* pdx11RTVs[2] = { (ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_RENDEROUT].vtrPtrs.at(0)
					, (ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_DEPTHCS].vtrPtrs.at(0) };
				pdx11DeviceImmContext->OMSetRenderTargets(2, pdx11RTVs, NULL);

				//__PS_MERGE_LAYERS_TO_RENDEROUT
				uint uiStrideSizeInput = sizeof(vmfloat3);
				uint uiOffset = 0;
				pdx11DeviceImmContext->IASetVertexBuffers(0, 1, (ID3D11Buffer**)&pdx11BufProxyRect, &uiStrideSizeInput, &uiOffset);
				pdx11DeviceImmContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				pdx11DeviceImmContext->IASetInputLayout(pdx11LayoutInputPos);
				pdx11DeviceImmContext->RSSetState(pdx11CommonParams->pdx11RStates[__RState_SOLID_NONE]);
				pdx11DeviceImmContext->OMSetDepthStencilState(pdx11CommonParams->pdx11DSStates[__DSState_DEPTHENABLE_ALWAYS], 0);
				pdx11DeviceImmContext->VSSetShader(pdx11VShader_Proxy, NULL, 0);
				pdx11DeviceImmContext->GSSetShader(NULL, NULL, 0);
				pdx11DeviceImmContext->PSSetShader(*ppdx11PS_Merges[__PS_MERGE_LAYERS_TO_RENDEROUT], NULL, 0);

				pdx11DeviceImmContext->Draw(4, 0);
#endif
				pdx11DeviceImmContext->Flush();
			}

#pragma region // Copy GPU to CPU
			GpuResDescriptor gpuResourceFB_RENDEROUT_SYSTEM_DESC;
			GpuResDescriptor gpuResourceFB_DEPTH_SYSTEM_DESC;
			GpuResource gpuResourceFB_RENDEROUT_SYSTEM_ARCH;
			GpuResource gpuResourceFB_DEPTH_SYSTEM_ARCH;
			gpuResourceFB_RENDEROUT_SYSTEM_DESC.res_type = ResourceTypeDX11RESOURCE;
			gpuResourceFB_RENDEROUT_SYSTEM_DESC.dtype = gpuResourcesFB_DESCs[__FB_TX2D_RENDEROUT].dtype;
			gpuResourceFB_RENDEROUT_SYSTEM_DESC.sdk_type = GpuSdkTypeDX11;
			gpuResourceFB_RENDEROUT_SYSTEM_DESC.usage_specifier = ("TEXTURE2D_IOBJECT_SYSTEM");
			gpuResourceFB_RENDEROUT_SYSTEM_DESC.misc = 0;
			gpuResourceFB_RENDEROUT_SYSTEM_DESC.src_obj_id = pCMergeIObject->GetObjectID();
			gpuResourceFB_DEPTH_SYSTEM_DESC.res_type = ResourceTypeDX11RESOURCE;
			gpuResourceFB_DEPTH_SYSTEM_DESC.dtype = data_type::dtype<float>();
			gpuResourceFB_DEPTH_SYSTEM_DESC.sdk_type = GpuSdkTypeDX11;
			gpuResourceFB_DEPTH_SYSTEM_DESC.usage_specifier = ("TEXTURE2D_IOBJECT_SYSTEM");
			gpuResourceFB_DEPTH_SYSTEM_DESC.misc = 0;
			gpuResourceFB_DEPTH_SYSTEM_DESC.src_obj_id = pCMergeIObject->GetObjectID();
			// When Resize, the Framebuffer is released, so we do not have to check the resize case.
			if (!pCGpuManager->GetGpuResourceArchive(&gpuResourceFB_RENDEROUT_SYSTEM_DESC, &gpuResourceFB_RENDEROUT_SYSTEM_ARCH))
				pCGpuManager->GenerateGpuResource(&gpuResourceFB_RENDEROUT_SYSTEM_DESC, pCMergeIObject, &gpuResourceFB_RENDEROUT_SYSTEM_ARCH, NULL);
			if (!pCGpuManager->GetGpuResourceArchive(&gpuResourceFB_DEPTH_SYSTEM_DESC, &gpuResourceFB_DEPTH_SYSTEM_ARCH))
				pCGpuManager->GenerateGpuResource(&gpuResourceFB_DEPTH_SYSTEM_DESC, pCMergeIObject, &gpuResourceFB_DEPTH_SYSTEM_ARCH, NULL);

			pdx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gpuResourceFB_RENDEROUT_SYSTEM_ARCH.vtrPtrs.at(0), (ID3D11Texture2D*)gpuResourcesFB_ARCHs[__FB_TX2D_RENDEROUT].vtrPtrs.at(0));
			pdx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gpuResourceFB_DEPTH_SYSTEM_ARCH.vtrPtrs.at(0), (ID3D11Texture2D*)gpuResourcesFB_ARCHs[__FB_TX2D_DEPTHCS].vtrPtrs.at(0));

			vmbyte4* py4Buffer = (vmbyte4*)pstFrameBuffer->fbuffer;
			float* pfBuffer = (float*)pstFrameBufferDepth->fbuffer;

			D3D11_MAPPED_SUBRESOURCE mappedResSysRGBA;
			D3D11_MAPPED_SUBRESOURCE mappedResSysDepth;
			pdx11DeviceImmContext->Map((ID3D11Texture2D*)gpuResourceFB_RENDEROUT_SYSTEM_ARCH.vtrPtrs.at(0), 0, D3D11_MAP_READ, NULL, &mappedResSysRGBA);
			pdx11DeviceImmContext->Map((ID3D11Texture2D*)gpuResourceFB_DEPTH_SYSTEM_ARCH.vtrPtrs.at(0), 0, D3D11_MAP_READ, NULL, &mappedResSysDepth);

			vmbyte4* py4RenderOuts = (vmbyte4*)mappedResSysRGBA.pData;
			float* pfDeferredContexts = (float*)mappedResSysDepth.pData;
			int iGpuBufferOffset = mappedResSysRGBA.RowPitch / 4;
#ifdef PPL_USE
			int count = i2SizeScreenCurrent.y;
			parallel_for(int(0), count, [is_rgba, i2SizeScreenCurrent, py4RenderOuts, pfDeferredContexts, py4Buffer, pfBuffer, iGpuBufferOffset](int i)
#else
#pragma omp parallel for 
			for (int i = 0; i < i2SizeScreenCurrent.y; i++)
#endif
			{
				for (int j = 0; j < i2SizeScreenCurrent.x; j++)
				{
					vmbyte4 y4Renderout = py4RenderOuts[j + i * iGpuBufferOffset];
					// __PS_MERGE_LAYERS_TO_RENDEROUT 에서 INT -> FLOAT4 로 되어 배열된 color 요소가 들어 옴. //

					// BGRA
					if (is_rgba)
						py4Buffer[i*i2SizeScreenCurrent.x + j] = vmbyte4(y4Renderout.x, y4Renderout.y, y4Renderout.z, y4Renderout.w);
					else
						py4Buffer[i*i2SizeScreenCurrent.x + j] = vmbyte4(y4Renderout.z, y4Renderout.y, y4Renderout.x, y4Renderout.w);

					int iAddr = i * i2SizeScreenCurrent.x + j;
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
			pdx11DeviceImmContext->Unmap((ID3D11Texture2D*)gpuResourceFB_RENDEROUT_SYSTEM_ARCH.vtrPtrs.at(0), 0);
			pdx11DeviceImmContext->Unmap((ID3D11Texture2D*)gpuResourceFB_DEPTH_SYSTEM_ARCH.vtrPtrs.at(0), 0);
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