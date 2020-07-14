#include "VXDX11Helper.h"
#include "Sampler.h"

//http://seanmiddleditch.com/direct3d-11-debug-api-tricks/

LPCWSTR __g_lpcwstrResourcesVR_PS[NUMSHADERS_VR_PS] = {	// The order should be same as the order of compile resource ones.
	MAKEINTRESOURCE(IDR_RCDATA0010), // "PS_DVR_BASIC_DEFAULT_ps_4_0" //
	MAKEINTRESOURCE(IDR_RCDATA0011), // "PS_DVR_BASIC_DEFAULT_SHADOW_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0012), // "PS_DVR_BASIC_DEFAULT_OTFMASK_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0013), // "PS_DVR_BASIC_DEFAULT_OTFMASK_SHADOW_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0014), // "PS_DVR_BASIC_CLIPSEMIOPAQUE_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0015), // "PS_DVR_BASIC_SCULPTMASK_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0016), // "PS_DVR_BASIC_MODULATION_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0017), // "PS_DVR_BASIC_MODULATION_OTFMASK_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0110), // "PS_DVR_LAYERS_DEFAULT_ps_4_0"	//
	MAKEINTRESOURCE(IDR_RCDATA0111), // "PS_DVR_LAYERS_DEFAULT_SHADOW_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0112), // "PS_DVR_LAYERS_DEFAULT_OTFMASK_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0113), // "PS_DVR_LAYERS_DEFAULT_OTFMASK_SHADOW_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0114), // "PS_DVR_LAYERS_CLIPSEMIOPAQUE_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0115), // "PS_DVR_LAYERS_SCULPTMASK_ps_4_0"	
	MAKEINTRESOURCE(IDR_RCDATA0116), // "PS_DVR_LAYERS_MODULATION_ps_4_0"	
	MAKEINTRESOURCE(IDR_RCDATA0117), // "PS_DVR_LAYERS_MODULATION_OTFMASK_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0210), // "PS_DVR_LAYERS_ADV_DEFAULT_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0211), // "PS_DVR_LAYERS_ADV_DEFAULT_SHADOW_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0212), // "PS_DVR_LAYERS_ADV_DEFAULT_OTFMASK_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0213), // "PS_DVR_LAYERS_ADV_DEFAULT_OTFMASK_SHADOW_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0214), // "PS_DVR_LAYERS_ADV_CLIPSEMIOPAQUE_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0215), // "PS_DVR_LAYERS_ADV_SCULPTMASK_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0216), // "PS_DVR_LAYERS_ADV_MODULATION_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0217), // "PS_DVR_LAYERS_ADV_MODULATION_OTFMASK_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0310), // "PS_DVR_SURFACE_DEFAULT_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0311), // "PS_DVR_SURFACE_DEFAULT_SHADOW_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0312), // "PS_DVR_SURFACE_DEFAULT_OTFMASK_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0313), // "PS_DVR_SURFACE_DEFAULT_OTFMASK_SHADOW_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0314), // "PS_DVR_SURFACE_CURVATURE_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0315), // "PS_DVR_SURFACE_CCF_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0316), // "PS_DVR_SURFACE_MARKER_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0500), // "PS_DVR_TEST_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0610), // "PS_XRAY_MAX_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0611), // "PS_XRAY_MAX_OTFMASK_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0612), // "PS_XRAY_MIN_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0613), // "PS_XRAY_MIN_OTFMASK_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0614), // "PS_XRAY_SUM_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0615), // "PS_XRAY_SUM_OTFMASK_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0810), // "PS_DVR_BASIC_DEFAULT_SHADOWMAP_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0811), // "PS_DVR_BASIC_OTFMASK_SHADOWMAP_ps_4_0"
};

LPCWSTR __g_lpcwstrResourcesVR_CS[NUMSHADERS_VR_CS] = {	// The order should be same as the order of compile resource ones.
	MAKEINTRESOURCE(IDR_RCDATA1010), // "CS_DVR_BASIC_DEFAULT_cs_5_0" //
	MAKEINTRESOURCE(IDR_RCDATA1011), // "CS_DVR_BASIC_DEFAULT_SHADOW_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1012), // "CS_DVR_BASIC_DEFAULT_OTFMASK_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1013), // "CS_DVR_BASIC_DEFAULT_OTFMASK_SHADOW_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1014), // "CS_DVR_BASIC_CLIPSEMIOPAQUE_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1015), // "CS_DVR_BASIC_SCULPTMASK_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1016), // "CS_DVR_BASIC_MODULATION_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1017), // "CS_DVR_BASIC_MODULATION_OTFMASK_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1110), // "CS_DVR_LAYERS_DEFAULT_cs_5_0"	//
	MAKEINTRESOURCE(IDR_RCDATA1111), // "CS_DVR_LAYERS_DEFAULT_SHADOW_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1112), // "CS_DVR_LAYERS_DEFAULT_OTFMASK_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1113), // "CS_DVR_LAYERS_DEFAULT_OTFMASK_SHADOW_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1114), // "CS_DVR_LAYERS_CLIPSEMIOPAQUE_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1115), // "CS_DVR_LAYERS_SCULPTMASK_cs_5_0"	
	MAKEINTRESOURCE(IDR_RCDATA1116), // "CS_DVR_LAYERS_MODULATION_cs_5_0"	
	MAKEINTRESOURCE(IDR_RCDATA1117), // "CS_DVR_LAYERS_MODULATION_OTFMASK_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1210), // "CS_DVR_LAYERS_ADV_DEFAULT_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1211), // "CS_DVR_LAYERS_ADV_DEFAULT_SHADOW_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1212), // "CS_DVR_LAYERS_ADV_DEFAULT_OTFMASK_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1213), // "CS_DVR_LAYERS_ADV_DEFAULT_OTFMASK_SHADOW_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1214), // "CS_DVR_LAYERS_ADV_CLIPSEMIOPAQUE_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1215), // "CS_DVR_LAYERS_ADV_SCULPTMASK_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1216), // "CS_DVR_LAYERS_ADV_MODULATION_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1217), // "CS_DVR_LAYERS_ADV_MODULATION_OTFMASK_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1310), // "CS_DVR_SURFACE_DEFAULT_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1311), // "CS_DVR_SURFACE_DEFAULT_SHADOW_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1312), // "CS_DVR_SURFACE_DEFAULT_OTFMASK_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1313), // "CS_DVR_SURFACE_DEFAULT_OTFMASK_SHADOW_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1314), // "CS_DVR_SURFACE_CURVATURE_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1315), // "CS_DVR_SURFACE_CCF_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1316), // "CS_DVR_SURFACE_MARKER_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1500), // "CS_DVR_TEST_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1610), // "CS_XRAY_MAX_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1611), // "CS_XRAY_MAX_OTFMASK_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1612), // "CS_XRAY_MIN_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1613), // "CS_XRAY_MIN_OTFMASK_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1614), // "CS_XRAY_SUM_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1615), // "CS_XRAY_SUM_OTFMASK_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1810), // "CS_DVR_BASIC_DEFAULT_SHADOWMAP_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA1811), // "CS_DVR_BASIC_OTFMASK_SHADOWMAP_cs_5_0"
};

LPCWSTR __g_lpcwstrResourcesMERGE_PS[NUMSHADERS_MERGE_PS] = {
	MAKEINTRESOURCE(IDR_RCDATA20000), // "PS_MERGE_LAYEROUT_TO_RENDEROUT_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA20010), // "PS_MERGE_SROUT_AND_LAYEROUT_TO_LAYEROUT_ps_4_0" 
	MAKEINTRESOURCE(IDR_RCDATA20020), // "PS_MERGE_ADV_SROUT_AND_LAYEROUT_TO_LAYEROUT_ps_4_0"
};

LPCWSTR __g_lpcwstrResourcesMERGE_CS[NUMSHADERS_MERGE_PS] = {
	MAKEINTRESOURCE(IDR_RCDATA20100), // "CS_MERGE_LAYEROUT_TO_RENDEROUT_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA20110), // "CS_MERGE_SROUT_AND_LAYEROUT_TO_LAYEROUT_cs_5_0" 
	MAKEINTRESOURCE(IDR_RCDATA20120), // "CS_MERGE_ADV_SROUT_AND_LAYEROUT_TO_LAYEROUT_cs_5_0"
};

LPCWSTR __g_lpcwstrResourcesGS[NUMSHADERS_GS] = {
	MAKEINTRESOURCE(IDR_RCDATA30010), // "GS_ThickPoints_gs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA30020), // "GS_ThickLines_gs_4_0" 
};

LPCWSTR __g_lpcwstrResourcesSR[NUMSHADERS_SR_PS] = {
	MAKEINTRESOURCE(IDR_RCDATA10010), // "SR_MAPPING_TEX1_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA10020), // "SR_PHONG_TEX1COLOR_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA10030), // "SR_PHONG_GLOBALCOLOR_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA10031), // "SR_PHONG_GLOBALCOLOR_MARKERSONSURFACE_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA10035), // "SR_PHONG_GLOBALCOLOR_MAXSHADING_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA10036), // "SR_PHONG_GLOBALCOLOR_MAXSHADING_MARKERSONSURFACE_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA10040), // "SR_VOLUME_DEVIATION_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA10050), // "SR_CMM_LINE_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA10060), // "SR_CMM_TEXT_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA10065), // "SR_CMM_MULTI_TEXTS_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA10070), // "SR_VOLUME_SUPERIMPOSE_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA10080), // "SR_IMAGESTACK_MAP_ps_4_0"
	MAKEINTRESOURCE(IDR_RCDATA10090), // "SR_AIRWAYANALYSIS_ps_4_0"
};

LPCWSTR __g_lpcwstrResourcesPLANESR[NUMSHADERS_PLANE_SR_VS] = {
	MAKEINTRESOURCE(IDR_RCDATA15010), // "SR_PROJ_P_MVB_vs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA15020), // "SR_PROJ_PN_MVB_vs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA15030), // "SR_PROJ_PT_MVB_vs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA15040), // "SR_PROJ_PNT_MVB_vs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA15050), // "SR_PROJ_PNT_DEV_MVB_vs_4_0"
};

LPCWSTR __g_lpcwstrResourcesBIASZSR[NUMSHADERS_BIASZ_SR_VS] = {
	MAKEINTRESOURCE(IDR_RCDATA15110), // "SR_PROJ_P_FBIASZ_vs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA15120), // "SR_PROJ_PN_FBIASZ_vs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA15130), // "SR_PROJ_PT_FBIASZ_vs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA15140), // "SR_PROJ_PNT_FBIASZ_vs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA15150), // "SR_PROJ_PNT_DEV_FBIASZ_vs_4_0"
};


LPCWSTR __g_lpcwstrRes_OIT_VS[5] = {
	MAKEINTRESOURCE(IDR_RCDATA11001), // "./OIT/SR_OIT_P_vs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA11002), // "./OIT/SR_OIT_PN_vs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA11003), // "./OIT/SR_OIT_PT_vs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA11004), // "./OIT/SR_OIT_PNT_vs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA11005), // "./OIT/SR_OIT_PTTT_vs_5_0"
};

LPCWSTR __g_lpcwstrRes_OIT_PS[1] = {
	MAKEINTRESOURCE(IDR_RCDATA11010), // "./OIT/SR_OIT_TEXMAP_ps_5_0"
};

LPCWSTR __g_lpcwstrRes_OIT_CS[1] = {
	MAKEINTRESOURCE(IDR_RCDATA21000), // "./OIT/SR_OIT_SORT2RENDER_cs_5_0"
};

template <typename T>
bool __GetParameterFromCustomStringMap(T* pvParameter/*out*/, map<string, void*>* pmapCustomParameter, string strParameterName)
{
	map<string, void*>::iterator itrParam = pmapCustomParameter->find(strParameterName.c_str());
	if (itrParam == pmapCustomParameter->end())
		return false;

	*(T*)pvParameter = *(T*)itrParam->second;

	return true;
}

bool HDx11AllocAndUpdateBlockMap(uint** ppiblocksMap, ushort** ppusBlocksMap, int related_tobj_id, VolumeBlocks* pstVolumeResBlock, DXGI_FORMAT eDgiFormat, LocalProgress* pLocalProgress)
{
	byte* pyTaggedActivatedBlocks = pstVolumeResBlock->GetTaggedActivatedBlocks(related_tobj_id);
	if (pyTaggedActivatedBlocks == NULL)
		GMERRORMESSAGE("UNBOUND TOBJECT VOLUME BLOCK!");
	uint uiNumBlocks = (uint)pstVolumeResBlock->blk_vol_size.x*(uint)pstVolumeResBlock->blk_vol_size.y*(uint)pstVolumeResBlock->blk_vol_size.z;
	uint uiCount = 0;
	vmint3 blk_bnd_size = pstVolumeResBlock->blk_bnd_size;
	vmint3 i3BlockNumSampleSize = pstVolumeResBlock->blk_vol_size + blk_bnd_size * 2;
	int iBlockNumSampleSizeXY = i3BlockNumSampleSize.x * i3BlockNumSampleSize.y;
	int iBlockNumXY = pstVolumeResBlock->blk_vol_size.x * pstVolumeResBlock->blk_vol_size.y;

	if (eDgiFormat == DXGI_FORMAT_R32_UINT)
	{
		*ppiblocksMap = new uint[uiNumBlocks];	// NEW
		uint* piBlocksMap = *ppiblocksMap;
		memset(piBlocksMap, 0, sizeof(uint)*uiNumBlocks);

		int iCountID = 0;
		for (int iZ = 0; iZ < pstVolumeResBlock->blk_vol_size.z; iZ++)
		{
			if (pLocalProgress)
				*pLocalProgress->progress_ptr = (pLocalProgress->start + pLocalProgress->range*iZ / pstVolumeResBlock->blk_vol_size.z);
			for (int iY = 0; iY < pstVolumeResBlock->blk_vol_size.y; iY++)
			for (int iX = 0; iX < pstVolumeResBlock->blk_vol_size.x; iX++)
			{
				int iAddrBlockCpu = (iX + blk_bnd_size.x)
					+ (iY + blk_bnd_size.y) * i3BlockNumSampleSize.x + (iZ + blk_bnd_size.z) * iBlockNumSampleSizeXY;
				if (pyTaggedActivatedBlocks[iAddrBlockCpu] != 0)
				{
					uiCount++;
					if (uiCount >= 8300000)
					{
						GMERRORMESSAGE("Bricks Overflow!");
						VMSAFE_DELETEARRAY(piBlocksMap);
						return false;
					}
					piBlocksMap[iCountID] = uiCount;
				}
				iCountID++;
			}
		}
	}
	else
	{
		*ppusBlocksMap = new ushort[uiNumBlocks];	// NEW
		ushort* pusBlocksMap = *ppusBlocksMap;
		memset(pusBlocksMap, 0, sizeof(ushort)*uiNumBlocks);
		int iCountID = 0;
		for (int iZ = 0; iZ < pstVolumeResBlock->blk_vol_size.z; iZ++)
		{
			if (pLocalProgress)
				*pLocalProgress->progress_ptr = (pLocalProgress->start + pLocalProgress->range*iZ / pstVolumeResBlock->blk_vol_size.z);
			for (int iY = 0; iY < pstVolumeResBlock->blk_vol_size.y; iY++)
			for (int iX = 0; iX < pstVolumeResBlock->blk_vol_size.x; iX++)
			{
				int iAddrBlockCpu = (iX + blk_bnd_size.x)
					+ (iY + blk_bnd_size.y) * i3BlockNumSampleSize.x + (iZ + blk_bnd_size.z) * iBlockNumSampleSizeXY;
				if (pyTaggedActivatedBlocks[iAddrBlockCpu] != 0)
				{
					uiCount++;
					if (uiCount >= 65535)
					{
						GMERRORMESSAGE("Bricks Overflow!");
						VMSAFE_DELETEARRAY(pusBlocksMap);
					}
					pusBlocksMap[iCountID] = uiCount;
				}
				iCountID++;
			}
		}
	}
	return true;
}

static GpuDX11CommonParameters g_VmCommonParams;
static VmGpuManager_v1* g_pCGpuManager = NULL;

void HDx11ReleaseCommonResources()
{
	GpuDX11CommonParameters* pParam = &g_VmCommonParams;
	// Initialized once //
	for(int i = 0; i < __DSState_COUNT; i++)
		VMSAFE_RELEASE(pParam->pdx11DSStates[i]);

	for(int i = 0; i < __RState_COUNT; i++)
		VMSAFE_RELEASE(pParam->pdx11RStates[i]);

	for(int i = 0; i < __SState_COUNT; i++)
		VMSAFE_RELEASE(pParam->pdx11SStates[i]);

	for(int i = 0; i < __CB_COUNT; i++)
		VMSAFE_RELEASE(pParam->pdx11BufConstParameters[i]);

	// Deallocate All Shared Resources //
	for (int i = 0; i < NUMSHADERS_VR_PS; i++)
		VMSAFE_RELEASE(pParam->pdx11PS_VR_Shaders[i]);
	for (int i = 0; i < NUMSHADERS_VR_CS; i++)
		VMSAFE_RELEASE(pParam->pdx11CS_VR_Shaders[i]);
	for (int i = 0; i < NUMSHADERS_PLANE_SR_VS; i++)
		VMSAFE_RELEASE(pParam->pdx11VS_PLANE_SR_Shaders[i]);
	for (int i = 0; i < NUMSHADERS_BIASZ_SR_VS; i++)
		VMSAFE_RELEASE(pParam->pdx11VS_FBIAS_SR_Shaders[i]);
	for (int i = 0; i < NUMSHADERS_SR_PS; i++)
		VMSAFE_RELEASE(pParam->pdx11PS_SR_Shaders[i]);
	for (int i = 0; i < NUMSHADERS_GS; i++)
		VMSAFE_RELEASE(pParam->pdx11GS_Shaders[i]);
	for (int i = 0; i < NUMSHADERS_MERGE_PS; i++)
		VMSAFE_RELEASE(pParam->pdx11PS_MergeTextures[i]);
	for (int i = 0; i < NUMSHADERS_MERGE_CS; i++)
		VMSAFE_RELEASE(pParam->pdx11CS_MergeTextures[i]);
	for (int i = 0; i < NUMINPUTLAYOUTS; i++)
		VMSAFE_RELEASE(pParam->pdx11IinputLayouts[i]);
	for (int i = 0; i < NUMSHADERS_SR_VS; i++)
		VMSAFE_RELEASE(pParam->pdx11VS_SR_Shaders[i]);

	for (int i = 0; i < NUMSHADERS_SR_OIT_VS; i++)
		VMSAFE_RELEASE(pParam->pdx11VS_SR_OIT[i]);
	for (int i = 0; i < NUMSHADERS_SR_OIT_PS; i++)
		VMSAFE_RELEASE(pParam->pdx11PS_SR_OIT[i]);
	for (int i = 0; i < NUMSHADERS_OIT_CS; i++)
		VMSAFE_RELEASE(pParam->pdx11CS_SR_OIT[i]);
	
	VMSAFE_RELEASE(pParam->pdx11BufProxyVertice);

	if(pParam->pdx11DeviceImmContext)
	{
		pParam->pdx11DeviceImmContext->Flush();
		pParam->pdx11DeviceImmContext->ClearState();
	}

	pParam->pdx11Device = NULL;
	pParam->pdx11DeviceImmContext = NULL;

	memset(&pParam->stDx11Adapter, NULL, sizeof(DXGI_ADAPTER_DESC));
	pParam->eDx11FeatureLevel = D3D_FEATURE_LEVEL_9_1;

	pParam->bIsInitialized = false;
}

HRESULT HDx11ConstantBufferSetting(ID3D11Device* pdx11Device, uint* puiSizeVRParameters, uint uiNumParams)
{
	if (uiNumParams > __CB_COUNT)
		return E_FAIL;

	for (int i = 0; i < __CB_COUNT; i++)
	{
		VMSAFE_RELEASE(g_VmCommonParams.pdx11BufConstParameters[i]);
	}

	HRESULT hr = S_OK;
	// Constant Buffer
	D3D11_BUFFER_DESC descCB;

	for (uint i = 0; i < uiNumParams; i++)
	{
		descCB.ByteWidth = puiSizeVRParameters[i];
		descCB.Usage = D3D11_USAGE_DYNAMIC;
		descCB.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		descCB.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		descCB.MiscFlags = NULL;
		descCB.StructureByteStride = puiSizeVRParameters[i];
		hr |= pdx11Device->CreateBuffer(&descCB, NULL, &g_VmCommonParams.pdx11BufConstParameters[i]);
	}

	return hr;
}

int HDx11InitializeGpuStatesAndBasicResources(VmGpuManager_v1* pCGpuManager, GpuDX11CommonParameters** ppParam /*out*/, bool* pbIsAvailableCS)
{
#define __SUPPORTED_GPU 0
#define __ALREADY_CHECKED 1
#define __LOW_SPEC_NOT_SUPPORT_CS_GPU 2
#define __INVALID_GPU -1
	if (pCGpuManager == NULL)
		return __INVALID_GPU;

	g_pCGpuManager = pCGpuManager;
	GpuDX11CommonParameters* pParam = &g_VmCommonParams;
	if (ppParam != NULL)
		*ppParam = pParam;

	if (pParam->bIsInitialized)
		return __ALREADY_CHECKED;

	g_pCGpuManager->GetDeviceInformation((void*)(&pParam->pdx11Device), ("DEVICE_POINTER"));
	g_pCGpuManager->GetDeviceInformation((void*)(&pParam->pdx11DeviceImmContext), ("DEVICE_CONTEXT_POINTER"));
	g_pCGpuManager->GetDeviceInformation(&pParam->stDx11Adapter, ("DEVICE_ADAPTER_DESC"));
	g_pCGpuManager->GetDeviceInformation(&pParam->eDx11FeatureLevel, ("FEATURE_LEVEL"));

	if (pParam->pdx11Device == NULL || pParam->pdx11DeviceImmContext == NULL)
		return __INVALID_GPU;

	for (int i = 0; i < __DSState_COUNT; i++)
		VMSAFE_RELEASE(pParam->pdx11DSStates[i]);

	for (int i = 0; i < __RState_COUNT; i++)
		VMSAFE_RELEASE(pParam->pdx11RStates[i]);

	for (int i = 0; i < __SState_COUNT; i++)
		VMSAFE_RELEASE(pParam->pdx11SStates[i]);

	HRESULT hr = S_OK;
	// HLSL 에서 대체하는 방법 찾아 보기.
	D3D11_RASTERIZER_DESC descRaster;
	ZeroMemory(&descRaster, sizeof(D3D11_RASTERIZER_DESC));
	descRaster.FillMode = D3D11_FILL_SOLID;
	descRaster.CullMode = D3D11_CULL_BACK;
	descRaster.FrontCounterClockwise = FALSE;
	descRaster.DepthBias = 0;
	descRaster.DepthBiasClamp = 0;
	descRaster.SlopeScaledDepthBias = 0;
	descRaster.DepthClipEnable = TRUE;
	descRaster.ScissorEnable = FALSE;
	descRaster.MultisampleEnable = FALSE;
	descRaster.AntialiasedLineEnable = false;
	hr |= pParam->pdx11Device->CreateRasterizerState(&descRaster, &pParam->pdx11RStates[__RState_SOLID_CW]);
	descRaster.AntialiasedLineEnable = true;
	hr |= pParam->pdx11Device->CreateRasterizerState(&descRaster, &pParam->pdx11RStates[__RState_ANTIALIASING_SOLID_CW]);
	//descRaster.CullMode = D3D11_CULL_FRONT;
	descRaster.CullMode = D3D11_CULL_BACK;
	descRaster.FrontCounterClockwise = TRUE;
	descRaster.AntialiasedLineEnable = false;
	hr |= pParam->pdx11Device->CreateRasterizerState(&descRaster, &pParam->pdx11RStates[__RState_SOLID_CCW]);
	descRaster.AntialiasedLineEnable = true;
	hr |= pParam->pdx11Device->CreateRasterizerState(&descRaster, &pParam->pdx11RStates[__RState_ANTIALIASING_SOLID_CCW]);
	descRaster.CullMode = D3D11_CULL_NONE;
	descRaster.AntialiasedLineEnable = false;
	hr |= pParam->pdx11Device->CreateRasterizerState(&descRaster, &pParam->pdx11RStates[__RState_SOLID_NONE]);
	descRaster.AntialiasedLineEnable = true;
	hr |= pParam->pdx11Device->CreateRasterizerState(&descRaster, &pParam->pdx11RStates[__RState_ANTIALIASING_SOLID_NONE]);

	descRaster.FillMode = D3D11_FILL_WIREFRAME;
	descRaster.CullMode = D3D11_CULL_BACK;
	descRaster.AntialiasedLineEnable = false;
	hr |= pParam->pdx11Device->CreateRasterizerState(&descRaster, &pParam->pdx11RStates[__RState_WIRE_CW]);
	descRaster.AntialiasedLineEnable = true;
	hr |= pParam->pdx11Device->CreateRasterizerState(&descRaster, &pParam->pdx11RStates[__RState_ANTIALIASING_WIRE_CW]);
	descRaster.CullMode = D3D11_CULL_FRONT;
	descRaster.AntialiasedLineEnable = false;
	hr |= pParam->pdx11Device->CreateRasterizerState(&descRaster, &pParam->pdx11RStates[__RState_WIRE_CCW]);
	descRaster.AntialiasedLineEnable = true;
	hr |= pParam->pdx11Device->CreateRasterizerState(&descRaster, &pParam->pdx11RStates[__RState_ANTIALIASING_WIRE_CCW]);
	descRaster.CullMode = D3D11_CULL_NONE;
	descRaster.AntialiasedLineEnable = false;
	hr |= pParam->pdx11Device->CreateRasterizerState(&descRaster, &pParam->pdx11RStates[__RState_WIRE_NONE]);
	descRaster.AntialiasedLineEnable = true;
	hr |= pParam->pdx11Device->CreateRasterizerState(&descRaster, &pParam->pdx11RStates[__RState_ANTIALIASING_WIRE_NONE]);

	D3D11_DEPTH_STENCIL_DESC descDepthStencil;
	ZeroMemory(&descDepthStencil, sizeof(D3D11_DEPTH_STENCIL_DESC));
	descDepthStencil.DepthEnable = TRUE;
	descDepthStencil.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	descDepthStencil.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	descDepthStencil.StencilEnable = FALSE;
	hr |= pParam->pdx11Device->CreateDepthStencilState(&descDepthStencil, &pParam->pdx11DSStates[__DSState_DEPTHENABLE_LESSEQUAL]);
	descDepthStencil.DepthFunc = D3D11_COMPARISON_ALWAYS;
	hr |= pParam->pdx11Device->CreateDepthStencilState(&descDepthStencil, &pParam->pdx11DSStates[__DSState_DEPTHENABLE_ALWAYS]);
	descDepthStencil.DepthFunc = D3D11_COMPARISON_GREATER;
	hr |= pParam->pdx11Device->CreateDepthStencilState(&descDepthStencil, &pParam->pdx11DSStates[__DSState_DEPTHENABLE_GREATER]);
	descDepthStencil.DepthFunc = D3D11_COMPARISON_LESS;
	hr |= pParam->pdx11Device->CreateDepthStencilState(&descDepthStencil, &pParam->pdx11DSStates[__DSState_DEPTHENABLE_LESS]);

	D3D11_SAMPLER_DESC descSampler;
	descSampler.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	descSampler.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	descSampler.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	descSampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	descSampler.MipLODBias = 0;
	descSampler.MaxAnisotropy = 16;
	descSampler.ComparisonFunc = D3D11_COMPARISON_NEVER;	// D3D11_COMPARISON_ALWAYS??
	descSampler.BorderColor[0] = 0.f;
	descSampler.BorderColor[1] = 0.f;
	descSampler.BorderColor[2] = 0.f;
	descSampler.BorderColor[3] = 0.f;
	descSampler.MaxLOD = FLT_MAX;
	descSampler.MinLOD = FLT_MIN;
	hr |= pParam->pdx11Device->CreateSamplerState(&descSampler, &pParam->pdx11SStates[__SState_POINT_CLAMP]);
	descSampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;	// NEAREST by ROUND
	hr |= pParam->pdx11Device->CreateSamplerState(&descSampler, &pParam->pdx11SStates[__SState_LINEAR_CLAMP]);
	descSampler.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	descSampler.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	descSampler.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	hr |= pParam->pdx11Device->CreateSamplerState(&descSampler, &pParam->pdx11SStates[__SState_LINEAR_ZEROBORDER]);
	descSampler.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;	// NEAREST by ROUND
	hr |= pParam->pdx11Device->CreateSamplerState(&descSampler, &pParam->pdx11SStates[__SState_POINT_ZEROBORDER]);


	uint uiSizeParams[__CB_COUNT] = {
		sizeof(CB_VrVolumeObject), // 0
		sizeof(CB_VrCameraState), // 1
		sizeof(CB_VrOtf1D), // 2
		sizeof(CB_VrBlockObject), // 3
		sizeof(CB_VrBrickChunk),  // 4
		sizeof(CB_VrSurfaceEffect), // 5
		sizeof(CB_VrModulation), // 6
		sizeof(CB_VrShadowMap), // 7
		sizeof(CB_SrPolygonObject), // 8
		sizeof(CB_SrProjectionCameraState), // 9
		sizeof(CB_SrDeviation), // 10
		sizeof(CB_VrInterestingRegion), // 11
		sizeof(D3DXVECTOR4), // 12
		sizeof(CB_VrOtf1D), // 13
		sizeof(CB_SrProxy), // 14

		sizeof(CB_VrOtf1D)* 3, // 15
		sizeof(CB_VrVolumeObject) * 3, // 16
		sizeof(CB_VrModulation) * 3, // 17

		sizeof(CB_FloatEncode), // 18
	};
	if (HDx11ConstantBufferSetting(pParam->pdx11Device, uiSizeParams, sizeof(uiSizeParams) / sizeof(uint)) != S_OK)
	{
		GMERRORMESSAGE("Constant Buffer Setting Error!");
		goto ERROR_PRESETTING;
	}

	// Vertex Buffer //
	VMSAFE_RELEASE(pParam->pdx11BufProxyVertice);
	D3D11_BUFFER_DESC descBufProxyVertex;
	ZeroMemory(&descBufProxyVertex, sizeof(D3D11_BUFFER_DESC));
	descBufProxyVertex.ByteWidth = sizeof(D3DXVECTOR3)* 4;
	descBufProxyVertex.Usage = D3D11_USAGE_DYNAMIC;
	descBufProxyVertex.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	descBufProxyVertex.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	descBufProxyVertex.MiscFlags = NULL;
	descBufProxyVertex.StructureByteStride = sizeof(D3DXVECTOR3);
	if (pParam->pdx11Device->CreateBuffer(&descBufProxyVertex, NULL, &pParam->pdx11BufProxyVertice) != S_OK)
		return __INVALID_GPU;

	D3D11_MAPPED_SUBRESOURCE mappedProxyRes;
	pParam->pdx11DeviceImmContext->Map(pParam->pdx11BufProxyVertice, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedProxyRes);
	D3DXVECTOR3* pv3PosProxy = (D3DXVECTOR3*)mappedProxyRes.pData;
	pv3PosProxy[0] = D3DXVECTOR3(-0.5f, 0.5f, 0);
	pv3PosProxy[1] = D3DXVECTOR3(0.5f, 0.5f, 0);
	pv3PosProxy[2] = D3DXVECTOR3(-0.5f, -0.5f, 0);
	pv3PosProxy[3] = D3DXVECTOR3(0.5f, -0.5f, 0);
	pParam->pdx11DeviceImmContext->Unmap(pParam->pdx11BufProxyVertice, NULL);

#pragma region // SR
	D3D11_INPUT_ELEMENT_DESC lotypeInputPosNor[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	D3D11_INPUT_ELEMENT_DESC lotypeInputPos[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	D3D11_INPUT_ELEMENT_DESC lotypeInputPosNorTex[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	D3D11_INPUT_ELEMENT_DESC lotypeInputPosTex[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	D3D11_INPUT_ELEMENT_DESC lotypeInputPosTTTex[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 2, DXGI_FORMAT_R32G32B32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	HMODULE hModule = GetModuleHandleA("vismtv_inbuilt_renderergpudx.dll");

	if (HDx11CompiledShaderSetting(pParam->pdx11Device, hModule, MAKEINTRESOURCE(IDR_RCDATA10001), "vs_4_0", (ID3D11DeviceChild**)&pParam->pdx11VS_SR_Shaders[__VS_P]
		, lotypeInputPos, 1, &pParam->pdx11IinputLayouts[0]) != S_OK)
	{
		goto ERROR_PRESETTING;
	}
	if (HDx11CompiledShaderSetting(pParam->pdx11Device, hModule, MAKEINTRESOURCE(IDR_RCDATA10002), "vs_4_0", (ID3D11DeviceChild**)&pParam->pdx11VS_SR_Shaders[__VS_PN]
		, lotypeInputPosNor, 2, &pParam->pdx11IinputLayouts[1]) != S_OK)
	{
		goto ERROR_PRESETTING;
	}
	if (HDx11CompiledShaderSetting(pParam->pdx11Device, hModule, MAKEINTRESOURCE(IDR_RCDATA10003), "vs_4_0", (ID3D11DeviceChild**)&pParam->pdx11VS_SR_Shaders[__VS_PT]
		, lotypeInputPosTex, 2, &pParam->pdx11IinputLayouts[2]) != S_OK)
	{
		goto ERROR_PRESETTING;
	}
	if (HDx11CompiledShaderSetting(pParam->pdx11Device, hModule, MAKEINTRESOURCE(IDR_RCDATA10004), "vs_4_0", (ID3D11DeviceChild**)&pParam->pdx11VS_SR_Shaders[__VS_PNT]
		, lotypeInputPosNorTex, 3, &pParam->pdx11IinputLayouts[3]) != S_OK)
	{
		goto ERROR_PRESETTING;
	}
	if (HDx11CompiledShaderSetting(pParam->pdx11Device, hModule, MAKEINTRESOURCE(IDR_RCDATA10005), "vs_4_0", (ID3D11DeviceChild**)&pParam->pdx11VS_SR_Shaders[__VS_PTTT]
		, lotypeInputPosTTTex, 4, &pParam->pdx11IinputLayouts[4]) != S_OK)
	{
		goto ERROR_PRESETTING;
	}
	if (HDx11CompiledShaderSetting(pParam->pdx11Device, hModule, MAKEINTRESOURCE(IDR_RCDATA10006), "vs_4_0", (ID3D11DeviceChild**)&pParam->pdx11VS_SR_Shaders[__VS_PNT_DEV]
		, lotypeInputPosNorTex, 3, NULL) != S_OK)
	{
		goto ERROR_PRESETTING;
	}
	if (HDx11CompiledShaderSetting(pParam->pdx11Device, hModule, MAKEINTRESOURCE(IDR_RCDATA10007), "vs_4_0", (ID3D11DeviceChild**)&pParam->pdx11VS_SR_Shaders[__VS_PROXY]
		, NULL, 1, NULL) != S_OK)
	{
		goto ERROR_PRESETTING;
	}

	for (int i = 0; i < NUMSHADERS_GS; i++)
	{
		if (HDx11CompiledShaderSetting(pParam->pdx11Device, hModule, __g_lpcwstrResourcesGS[i], "gs_4_0", (ID3D11DeviceChild**)&pParam->pdx11GS_Shaders[i]) != S_OK)
		{
			printf("\n*****************\n Invalid DX10 or higher NUMSHADERS_GS\n*****************\n");
			goto ERROR_PRESETTING;
		}
	}

	for (int i = 0; i < NUMSHADERS_SR_PS; i++)
	{
		if (HDx11CompiledShaderSetting(pParam->pdx11Device, hModule, __g_lpcwstrResourcesSR[i], "ps_4_0", (ID3D11DeviceChild**)&pParam->pdx11PS_SR_Shaders[i]) != S_OK)
		{
			printf("\n*****************\n Invalid DX10 or higher NUMSHADERS_SR_PS\n*****************\n");
			goto ERROR_PRESETTING;
		}
	}
	for (int i = 0; i < NUMSHADERS_PLANE_SR_VS; i++)
	{
		if (HDx11CompiledShaderSetting(pParam->pdx11Device, hModule, __g_lpcwstrResourcesPLANESR[i], "vs_4_0", (ID3D11DeviceChild**)&pParam->pdx11VS_PLANE_SR_Shaders[i]) != S_OK)
		{
			printf("\n*****************\n Invalid DX10 or higher NUMSHADERS_PLANE_SR_VS\n*****************\n");
			goto ERROR_PRESETTING;
		}
	}
	for (int i = 0; i < NUMSHADERS_BIASZ_SR_VS; i++)
	{
		if (HDx11CompiledShaderSetting(pParam->pdx11Device, hModule, __g_lpcwstrResourcesBIASZSR[i], "vs_4_0", (ID3D11DeviceChild**)&pParam->pdx11VS_FBIAS_SR_Shaders[i]) != S_OK)
		{
			printf("\n*****************\n Invalid DX10 or higher NUMSHADERS_PLANE_SR_VS\n*****************\n");
			goto ERROR_PRESETTING;
		}
	}
#pragma endregion

#pragma region // VR
	for (int i = 0; i < NUMSHADERS_VR_PS; i++)
	{
		if (HDx11CompiledShaderSetting(pParam->pdx11Device, hModule, __g_lpcwstrResourcesVR_PS[i], "ps_4_0", (ID3D11DeviceChild**)&pParam->pdx11PS_VR_Shaders[i]) != S_OK)
		{
			//GMERRORMESSAGE("Shader Setting Error! - VR");
			printf("\n*****************\n Invalid Pixel Shader\n*****************\n");
			return __INVALID_GPU;
		}
	}
#pragma endregion 

#pragma region // Merge
	for (int i = 0; i < NUMSHADERS_MERGE_PS; i++)
	{
		if (HDx11CompiledShaderSetting(pParam->pdx11Device, hModule, __g_lpcwstrResourcesMERGE_PS[i], "ps_4_0", (ID3D11DeviceChild**)&pParam->pdx11PS_MergeTextures[i]) != S_OK)
		{
			GMERRORMESSAGE("Shader Setting Error! - MERGE");
			printf("\n*****************\n Invalid Pixel Shader\n*****************\n");
			return __INVALID_GPU;
		}
	}
#pragma endregion

#pragma region // OIT
	for (int i = 0; i < NUMSHADERS_SR_OIT_VS; i++)
	{
		if (HDx11CompiledShaderSetting(pParam->pdx11Device, hModule, __g_lpcwstrRes_OIT_VS[i], "vs_5_0", (ID3D11DeviceChild**)&pParam->pdx11VS_SR_OIT[i]) != S_OK)
		{
			printf("\n*****************\n Invalid DX10 or higher NUMSHADERS_SR_OIT_VS\n*****************\n");
			goto ERROR_PRESETTING;
		}
	}
	for (int i = 0; i < NUMSHADERS_SR_OIT_PS; i++)
	{
		if (HDx11CompiledShaderSetting(pParam->pdx11Device, hModule, __g_lpcwstrRes_OIT_PS[i], "ps_5_0", (ID3D11DeviceChild**)&pParam->pdx11PS_SR_OIT[i]) != S_OK)
		{
			printf("\n*****************\n Invalid DX10 or higher NUMSHADERS_SR_OIT_PS\n*****************\n");
			goto ERROR_PRESETTING;
		}
	}
	for (int i = 0; i < NUMSHADERS_OIT_CS; i++)
	{
		if (HDx11CompiledShaderSetting(pParam->pdx11Device, hModule, __g_lpcwstrRes_OIT_CS[i], "cs_5_0", (ID3D11DeviceChild**)&pParam->pdx11CS_SR_OIT[i]) != S_OK)
		{
			printf("\n*****************\n Invalid DX10 or higher NUMSHADERS_OIT_CS\n*****************\n");
			goto ERROR_PRESETTING;
		}
	}
#pragma endregion



	bool bReSafeTry = false;
	bool bSkipCS = false;
	if (pParam->eDx11FeatureLevel == 0x9300 || pParam->eDx11FeatureLevel == 0xA000)
		bSkipCS = true;

ERROR_PRESETTING_CS:
	if(pbIsAvailableCS) 
		*pbIsAvailableCS = !bSkipCS;
	if (!bSkipCS && !bReSafeTry)
	{
#pragma region // VR
		for (int i = 0; i < NUMSHADERS_VR_CS; i++)
		{
			if (HDx11CompiledShaderSetting(pParam->pdx11Device, hModule, __g_lpcwstrResourcesVR_CS[i], "cs_5_0", (ID3D11DeviceChild**)&pParam->pdx11CS_VR_Shaders[i]) != S_OK)
			{
				//GMERRORMESSAGE("Shader Setting Error! - VR");
				printf("\n*****************\n Invalid Pixel Shader\n*****************\n");
		
				bSkipCS = true;
				bReSafeTry = true;
				goto ERROR_PRESETTING_CS;
				return __INVALID_GPU;
			}
		}
#pragma endregion 

#pragma region // Merge
		for (int i = 0; i < NUMSHADERS_MERGE_CS; i++)
		{
			if (HDx11CompiledShaderSetting(pParam->pdx11Device, hModule, __g_lpcwstrResourcesMERGE_CS[i], "cs_5_0", (ID3D11DeviceChild**)&pParam->pdx11CS_MergeTextures[i]) != S_OK)
			{
				GMERRORMESSAGE("Shader Setting Error! - MERGE");
				//::MessageBox(NULL, ("MERGE Shader Setting Error!"), NULL, MB_OK);
				bSkipCS = true;
				bReSafeTry = true;
				goto ERROR_PRESETTING_CS;
			}
		}
#pragma endregion
	}

	pParam->bIsInitialized = true;

	if (hr != S_OK)
		return __INVALID_GPU;

	if (bSkipCS)
		return __LOW_SPEC_NOT_SUPPORT_CS_GPU;

	return __SUPPORTED_GPU;

ERROR_PRESETTING:
	HDx11ReleaseCommonResources();
	return __INVALID_GPU;
}

HRESULT HDx11CompiledShaderSetting(ID3D11Device* pdx11Device, HMODULE hModule, LPCWSTR pSrcResource, LPCSTR strShaderProfile, ID3D11DeviceChild** ppdx11Shader/*out*/
	, D3D11_INPUT_ELEMENT_DESC* pInputLayoutDesc, uint uiNumOfInputElements, ID3D11InputLayout** ppdx11LayoutInputVS)
{
	HRSRC hrSrc = FindResource(hModule, pSrcResource, RT_RCDATA);
	HGLOBAL hGlobalByte = LoadResource(hModule, hrSrc);
	LPVOID pdata = LockResource(hGlobalByte);
	ullong ullFileSize = SizeofResource(hModule, hrSrc);

	string _strShaderProfile = strShaderProfile;
	if(_strShaderProfile.compare(0, 2, "cs") == 0)
	{
		if(pdx11Device->CreateComputeShader(pdata, ullFileSize, NULL, (ID3D11ComputeShader**)ppdx11Shader) != S_OK)
			goto ERROR_SHADER;
	}
	else if (_strShaderProfile.compare(0, 2, "ps") == 0)
	{
		if (pdx11Device->CreatePixelShader(pdata, ullFileSize, NULL, (ID3D11PixelShader**)ppdx11Shader) != S_OK)
			goto ERROR_SHADER;
	}
	else if (_strShaderProfile.compare(0, 2, "gs") == 0)
	{
		if (pdx11Device->CreateGeometryShader(pdata, ullFileSize, NULL, (ID3D11GeometryShader**)ppdx11Shader) != S_OK)
			goto ERROR_SHADER;
	}
	else if(_strShaderProfile.compare(0, 2, "vs") == 0)
	{
		if(pdx11Device->CreateVertexShader(pdata, ullFileSize, NULL, (ID3D11VertexShader**)ppdx11Shader) != S_OK)
			goto ERROR_SHADER;

		if(pInputLayoutDesc != NULL && uiNumOfInputElements > 0 && ppdx11LayoutInputVS != NULL)
		{
			if(pdx11Device->CreateInputLayout( pInputLayoutDesc, uiNumOfInputElements, pdata, ullFileSize, ppdx11LayoutInputVS ) != S_OK)
				goto ERROR_SHADER;
		}
	}
	else 
	{
		goto ERROR_SHADER;
	}

	return S_OK;

ERROR_SHADER:
	return E_FAIL;
}

HRESULT HDx11ShaderSetting(ID3D11Device* pdx11Device, string* pstrShaderFilePath, HMODULE hModule, LPCWSTR pSrcResource, LPCSTR strShaderProfile, LPCSTR strShaderFuncName, ID3D11DeviceChild** ppdx11Shader/*out*/
	, D3D11_INPUT_ELEMENT_DESC* pInputLayoutDesc, uint uiNumOfInputElements, ID3D11InputLayout** ppdx11LayoutInputVS)
{
	ID3DBlob* pBlobBuf = NULL;
	string _strShaderProfile = strShaderProfile;

	//CShaderHeader includeHlsl;

	if(pstrShaderFilePath)
	{
		if(D3DX11CompileFromFileA(pstrShaderFilePath->c_str(), NULL, NULL/*&includeHlsl*/, strShaderFuncName, strShaderProfile, D3D10_SHADER_ENABLE_STRICTNESS|D3D10_SHADER_OPTIMIZATION_LEVEL3
			, 0, NULL, &pBlobBuf, NULL, NULL) != S_OK)
			goto ERROR_SHADER;
	}
	else
	{
		if(D3DX11CompileFromResource(hModule, pSrcResource, NULL, NULL, NULL/*&includeHlsl*/, strShaderFuncName, strShaderProfile, D3D10_SHADER_ENABLE_STRICTNESS|D3D10_SHADER_OPTIMIZATION_LEVEL3
			, 0, NULL, &pBlobBuf, NULL, NULL) != S_OK)
			goto ERROR_SHADER;
	}

	VMSAFE_RELEASE(*ppdx11Shader);
	if(_strShaderProfile.compare(0, 2, "cs") == 0)
	{
		if(pdx11Device->CreateComputeShader(pBlobBuf->GetBufferPointer(), pBlobBuf->GetBufferSize(), 
			NULL, (ID3D11ComputeShader**)ppdx11Shader) != S_OK)
			goto ERROR_SHADER;
	}
	else if(_strShaderProfile.compare(0, 2, "ps") == 0)
	{
		if(pdx11Device->CreatePixelShader(pBlobBuf->GetBufferPointer(), pBlobBuf->GetBufferSize(), 
			NULL, (ID3D11PixelShader**)ppdx11Shader) != S_OK)
			goto ERROR_SHADER;
	}
	else if(_strShaderProfile.compare(0, 2, "vs") == 0)
	{
		if(pdx11Device->CreateVertexShader(pBlobBuf->GetBufferPointer(), pBlobBuf->GetBufferSize(), 
			NULL, (ID3D11VertexShader**)ppdx11Shader) != S_OK)
			goto ERROR_SHADER;

		if(pInputLayoutDesc != NULL && uiNumOfInputElements > 0 && ppdx11LayoutInputVS != NULL)
		{
			if(pdx11Device->CreateInputLayout( pInputLayoutDesc, uiNumOfInputElements, pBlobBuf->GetBufferPointer(), pBlobBuf->GetBufferSize(), ppdx11LayoutInputVS ) != S_OK)
				goto ERROR_SHADER;
		}
	}
	else 
	{
		goto ERROR_SHADER;
	}

	VMSAFE_RELEASE(pBlobBuf);
	return S_OK;

ERROR_SHADER:
	VMSAFE_RELEASE(pBlobBuf);
	return E_FAIL;
}


bool HDx11ModifyBlockMap(VmVObjectVolume* pCObjectVolume, GpuResource* pGpuResBlks, GpuResource* pstGpuResBlkSRView, bool bIsImmutable)
{
	if (!g_VmCommonParams.bIsInitialized)
		return false;

	VolumeData* pVolumeData = ((VmVObjectVolume*)pCObjectVolume)->GetVolumeData();
	int iBlkLevel = (int)(pGpuResBlks->gpu_res_desc.misc & 0x1);	// 0 : High Resolution, 1 : Low Resolution
	VolumeBlocks* pstVolumeResBlock = ((VmVObjectVolume*)pCObjectVolume)->GetVolumeBlock(iBlkLevel); 

	D3D11_TEXTURE3D_DESC descTex3DMap;
	ZeroMemory(&descTex3DMap, sizeof(D3D11_TEXTURE3D_DESC));
	descTex3DMap.Width = pstVolumeResBlock->blk_vol_size.x;
	descTex3DMap.Height = pstVolumeResBlock->blk_vol_size.y;
	descTex3DMap.Depth = pstVolumeResBlock->blk_vol_size.z;
	descTex3DMap.MipLevels = 1;
	descTex3DMap.MiscFlags = NULL;

	if(pGpuResBlks->gpu_res_desc.dtype.type_name == data_type::dtype<ushort>().type_name) descTex3DMap.Format = DXGI_FORMAT_R16_UNORM;
	else if(pGpuResBlks->gpu_res_desc.dtype.type_name == data_type::dtype<uint>().type_name) descTex3DMap.Format = DXGI_FORMAT_R32_UINT;
	else return false;
	
	//bIsImmutable = false;
	descTex3DMap.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	if(bIsImmutable)
	{
		descTex3DMap.Usage = D3D11_USAGE_IMMUTABLE;
		descTex3DMap.CPUAccessFlags = NULL;
	}
	else
	{
		descTex3DMap.Usage = D3D11_USAGE_DYNAMIC;
		descTex3DMap.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	}

	uint* piBlocksMap = NULL;
	ushort* pusBlocksMap = NULL;

	int related_tobj_id = (int)((pGpuResBlks->gpu_res_desc.misc >> 16) % 65536) | (((int)ObjectTypeTMAP) << 24);
	HDx11AllocAndUpdateBlockMap(&piBlocksMap, &pusBlocksMap, related_tobj_id, pstVolumeResBlock, descTex3DMap.Format, NULL);

	// Create Texture //
	ID3D11Texture3D* pdx11TX3DBlkMap = NULL;
	HRESULT hr;
	if (bIsImmutable)
	{
		D3D11_SUBRESOURCE_DATA subDataRes;
		if(descTex3DMap.Format == DXGI_FORMAT_R32_UINT)
		{
			subDataRes.pSysMem = piBlocksMap;
			subDataRes.SysMemPitch = sizeof(uint)*descTex3DMap.Width;
			subDataRes.SysMemSlicePitch = sizeof(uint)*descTex3DMap.Width*descTex3DMap.Height;
		}
		else
		{
			subDataRes.pSysMem = pusBlocksMap;
			subDataRes.SysMemPitch = sizeof(ushort)*descTex3DMap.Width;
			subDataRes.SysMemSlicePitch = sizeof(ushort)*descTex3DMap.Width*descTex3DMap.Height;
		}
		hr = g_VmCommonParams.pdx11Device->CreateTexture3D(&descTex3DMap, &subDataRes, &pdx11TX3DBlkMap);
	}
	else
	{
		//hr = g_VmCommonParams.pdx11Device->CreateTexture3D(&descTex3DMap, NULL, &pdx11TX3DBlkMap);
		pdx11TX3DBlkMap = (ID3D11Texture3D*)pGpuResBlks->vtrPtrs[0];
		D3D11_MAPPED_SUBRESOURCE d11MappedRes;
		hr = g_VmCommonParams.pdx11DeviceImmContext->Map(pdx11TX3DBlkMap, 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes);
		if(descTex3DMap.Format == DXGI_FORMAT_R32_UINT)
		{
			uint* piBlkMap = (uint*)d11MappedRes.pData;
			for(int i = 0; i < (int)descTex3DMap.Depth; i++)
				for(int j = 0; j < (int)descTex3DMap.Height; j++)
				{
					memcpy(&piBlkMap[j*d11MappedRes.RowPitch/4 + i*d11MappedRes.DepthPitch/4], &piBlocksMap[j*descTex3DMap.Width + i*descTex3DMap.Width*descTex3DMap.Height]
					, sizeof(float) * descTex3DMap.Width);
				}
		}
		else
		{
			ushort* pusBlkMap = (ushort*)d11MappedRes.pData;
			for(int i = 0; i < (int)descTex3DMap.Depth; i++)
				for(int j = 0; j < (int)descTex3DMap.Height; j++)
				{
					memcpy(&pusBlkMap[j*d11MappedRes.RowPitch/2 + i*d11MappedRes.DepthPitch/2], &pusBlocksMap[j*descTex3DMap.Width + i*descTex3DMap.Width*descTex3DMap.Height]
					, sizeof(ushort) * descTex3DMap.Width);
				}
		}
		g_VmCommonParams.pdx11DeviceImmContext->Unmap(pdx11TX3DBlkMap, 0);
	}
	if(descTex3DMap.Format == DXGI_FORMAT_R32_UINT)
	{
		VMSAFE_DELETEARRAY(piBlocksMap);
	}
	else
	{
		VMSAFE_DELETEARRAY(pusBlocksMap);
	}
	if(hr == S_OK)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC descSRVVolData;
		descSRVVolData.Format = descTex3DMap.Format;
		descSRVVolData.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
		descSRVVolData.Texture3D.MipLevels = 1;
		descSRVVolData.Texture3D.MostDetailedMip = 0;
		ID3D11ShaderResourceView* pdx11View;
		if (g_VmCommonParams.pdx11Device->CreateShaderResourceView(pdx11TX3DBlkMap, &descSRVVolData, (ID3D11ShaderResourceView**)&pdx11View) != S_OK)
		{
			VMSAFE_RELEASE(pdx11TX3DBlkMap);
			VMSAFE_RELEASE(pdx11View);
			return false;
		}

		if (bIsImmutable)
		{
			//ID3D11Texture3D* pdx11TexBlkMapOld = (ID3D11Texture3D*)pGpuResBlks->vtrPtrs.at(0);
			//VMSAFE_RELEASE(pdx11TexBlkMapOld);
			// release 가 ReplaceOrAddGpuResource 에서 발생
			pGpuResBlks->vtrPtrs[0] = pdx11TX3DBlkMap;

			//ID3D11ShaderResourceView* pdx11SRViewBlkMapOld = (ID3D11ShaderResourceView*)pstGpuResourceBlockSRView->vtrPtrs.at(0);
			//VMSAFE_RELEASE(pdx11SRViewBlkMapOld);
			// release 가 ReplaceOrAddGpuResource 에서 발생
			pstGpuResBlkSRView->vtrPtrs[0] = pdx11View;

			g_pCGpuManager->ReplaceOrAddGpuResource(pGpuResBlks);
			g_pCGpuManager->ReplaceOrAddGpuResource(pstGpuResBlkSRView);
		}
		else
		{
			//
		}
	}
	else
	{
		VMSAFE_RELEASE(pdx11TX3DBlkMap);
		return false;
	}

	return true;
}

bool HDx11ModifyAOMap(VmVObjectVolume* pCObjectVolume, GpuResource* pGpuResAOM, VmTObject* pCTObj)
{
	if (!g_VmCommonParams.bIsInitialized)
		return false;

	VolumeData* vol_data = pCObjectVolume->GetVolumeData();
	TMapData* tmap_data = pCTObj->GetTMapData();
	vmbyte4* otf_map = (vmbyte4*)tmap_data->tmap_buffers[0];

	ID3D11Texture3D* pdx11TX3D_AOM = (ID3D11Texture3D*)pGpuResAOM->vtrPtrs[0];
	D3D11_MAPPED_SUBRESOURCE d11MappedRes;
	HRESULT hr = g_VmCommonParams.pdx11DeviceImmContext->Map(pdx11TX3D_AOM, 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes);

	vmfloat3 vecVoxelGradDirs[3];// = { vmfloat3(1, 0, 0), vmfloat3(0, 1, 0), vmfloat3(0, 0, 1) };
	float min_dist_sample = (float)min(min(vol_data->vox_pitch.x, vol_data->vox_pitch.y), vol_data->vox_pitch.z);
	fTransformVector(&vecVoxelGradDirs[0], &vmfloat3(min_dist_sample, 0, 0), &pCObjectVolume->GetMatrixWS2OSf());
	fTransformVector(&vecVoxelGradDirs[1], &vmfloat3(0, min_dist_sample, 0), &pCObjectVolume->GetMatrixWS2OSf());
	fTransformVector(&vecVoxelGradDirs[2], &vmfloat3(0, 0, min_dist_sample), &pCObjectVolume->GetMatrixWS2OSf());

	const float sample_dist_scale = 1.f;
	const float sample_dist = min_dist_sample;
	const float surface_like = (float)(vol_data->store_Mm_values.y - vol_data->store_Mm_values.x / 1000.);
	const int num_ray_steps = 10;

	vmfloat3 vec_grad_unit_vs[3] = {
		vecVoxelGradDirs[0] * (float)sample_dist_scale,
		vecVoxelGradDirs[1] * (float)sample_dist_scale,
		vecVoxelGradDirs[2] * (float)sample_dist_scale };

	vmint3 volSize = vol_data->vol_size;
	vmint3 volSizeEx = vol_data->bnd_size;
	int width_sample = volSize.x + volSizeEx.x * 2;

	// DXGI_FORMAT_R16_UNORM
	ushort* aom_array = (ushort*)d11MappedRes.pData;

#define NUM_RAYS 5
	float w_dir[NUM_RAYS];
	w_dir[0] = 0.3f;
	w_dir[1] = 0.175f;
	w_dir[2] = 0.175f;
	w_dir[3] = 0.175f;
	w_dir[4] = 0.175f;

	for (int z = 2; z < vol_data->vol_size.z - 2; z++)
		for (int y = 2; y < vol_data->vol_size.y - 2; y++)
			for (int x = 2; x < vol_data->vol_size.x - 2; x++)
			{
				vmfloat3 g = __Safe_Gradient_by_Samples(vmfloat3(x, y, z), volSize, vec_grad_unit_vs[0], vec_grad_unit_vs[1], vec_grad_unit_vs[2], width_sample, 2, (ushort**)vol_data->vol_slices);
				float gm = fLengthVector(&g);
				g /= gm;

				vmfloat3 g_up = vmfloat3(0, 0, 1), g_right;
				vmfloat3 v_tmp = cross(g, g_up);
				if (fLengthVector(&v_tmp) < 0.000001f)
					g_up = vmfloat3(0, 1, 0);
				fCrossDotVector(&g_right, &g, &g_up);
				fCrossDotVector(&g_up, &g_right, &g);

				vmfloat3 dir_sample[NUM_RAYS];
				dir_sample[0] = g;
				fNormalizeVector(&dir_sample[1], &(g + g_right));
				fNormalizeVector(&dir_sample[2], &(g + g_up));
				fNormalizeVector(&dir_sample[3], &(g - g_right));
				fNormalizeVector(&dir_sample[4], &(g - g_up));

				vmfloat3 pos_sample_cen;
				fTransformPoint(&pos_sample_cen, &vmfloat3(x, y, z), &pCObjectVolume->GetMatrixOS2WSf());

				float ambient = 0;

				for (int i = 0; i < NUM_RAYS; i++)
				{
					vmfloat3 vec_sample = dir_sample[i] * sample_dist;
					float occ_a_k = 0;
					float ambient_ray = 0;
					for (int j = 2; j < num_ray_steps; j++)
					{
						vmfloat3 pos_sample_ws = pos_sample_cen + vec_sample * (float)j, pos_sample_vs;
						fTransformPoint(&pos_sample_vs, &pos_sample_ws, &pCObjectVolume->GetMatrixWS2OSf());
						float v = __Safe_TrilinearSample(pos_sample_vs, volSize, width_sample, 2, (ushort**)vol_data->vol_slices);
						//vmbyte4 emmv = otf_map[(int)v];
						
						occ_a_k += (1 - occ_a_k) * otf_map[(int)v].a / 255.f;
						if (occ_a_k > 0.95f) break;
						ambient_ray += (1 - occ_a_k); // the light contribution 은 shader 의 rendering pass 에서 처리, 즉 여기서는 1 로 처리.
					}
					ambient += ambient_ray * w_dir[i] / (float)(num_ray_steps - 2);
				}

				aom_array[y*d11MappedRes.RowPitch / 2 + z * d11MappedRes.DepthPitch / 2 + x] = (ushort)(ambient * 65535.f);
			}

	g_VmCommonParams.pdx11DeviceImmContext->Unmap(pdx11TX3D_AOM, 0);

	return true;
}

//#define OLD
bool HDx11ComputeConstantBufferVrObject(CB_VrVolumeObject* pCBVrVolumeObj, bool bIsDownSample, 
	VmVObjectVolume* pCVObjectVolume, VmCObject* pCCObject, vmint3 i3SizeVolumeTextureElements, VmLObject* pCLObjectForParameters, int obj_param_src_id)
{
	VolumeData* pVolumeData = pCVObjectVolume->GetVolumeData();
	pCBVrVolumeObj->iFlags = 0;

	vmmat44f matVS2TS;
#ifdef OLD
	float fInverseX = 1;
	if(pVolumeData->vol_size.x > 1)
		fInverseX = 1.f/((float)pVolumeData->vol_size.x - 0.0f);
	float fInverseY = 1;
	if(pVolumeData->vol_size.y > 1)
		fInverseY = 1.f/((float)pVolumeData->vol_size.y - 0.0f);
	float fInverseZ = 1;
	if(pVolumeData->vol_size.z > 1)
		fInverseZ = 1.f/((float)pVolumeData->vol_size.z - 0.0f);
	fMatrixScaling(&matVS2TS, &vmfloat3(fInverseX, fInverseY, fInverseZ));
#else //OLD2
	vmmat44f matShift, matScale;
	fMatrixTranslation(&matShift, &vmfloat3(0.5f));
	fMatrixScaling(&matScale, &vmfloat3(1.f / (float)(pVolumeData->vol_size.x + pVolumeData->vox_pitch.x * 0.00f),
		1.f / (float)(pVolumeData->vol_size.y + pVolumeData->vox_pitch.z * 0.00f), 1.f / (float)(pVolumeData->vol_size.z + pVolumeData->vox_pitch.z * 0.00f)));
	matVS2TS = matShift * matScale;
// #else
// 	vmfloat3 f3VecScale;
// 	f3VecScale.x = pVolumeData->vol_size.x == 1? 1.f : 1.f / (float)pVolumeData->vol_size.x;
// 	f3VecScale.y = pVolumeData->vol_size.y == 1? 1.f : 1.f / (float)pVolumeData->vol_size.y;
// 	f3VecScale.z = pVolumeData->vol_size.z == 1? 1.f : 1.f / (float)pVolumeData->vol_size.z;
// 	D3DXMatrixScaling(&matVS2TS, f3VecScale.x, f3VecScale.y, f3VecScale.z);
#endif

	pCBVrVolumeObj->dxmatWS2TS = *(D3DXMATRIX*)&(pCVObjectVolume->GetMatrixWS2OSf() * matVS2TS); // set

	const vmmat44f& matVS2WS = pCVObjectVolume->GetMatrixOS2WSf();

	AaBbMinMax stAaBbVS;
	pCVObjectVolume->GetOrthoBoundingBox(stAaBbVS);
	fTransformPoint((vmfloat3*)&pCBVrVolumeObj->f3PosVObjectMaxWS, &(vmfloat3)stAaBbVS.pos_max, &matVS2WS); // set
	
	vmfloat3 f3PosVObjectMinWS, f3VecVObjectZWS, f3VecVObjectYWS, f3VecVObjectXWS;
	fTransformPoint(&f3PosVObjectMinWS, &(vmfloat3)stAaBbVS.pos_min, &matVS2WS);
	fTransformVector(&f3VecVObjectZWS, &vmfloat3(0, 0, -1.f), &matVS2WS);
	fTransformVector(&f3VecVObjectYWS, &vmfloat3(0, 1.f, 0), &matVS2WS);
	fTransformVector(&f3VecVObjectXWS, &vmfloat3(1.f, 0, 0), &matVS2WS);
	vmfloat3 f3VecCrossZY;
	fCrossDotVector(&f3VecCrossZY, &f3VecVObjectZWS, &f3VecVObjectYWS);
	if(fDotVector(&f3VecCrossZY, &f3VecVObjectXWS) < 0)
		fTransformVector(&f3VecVObjectYWS, &vmfloat3(1.f, 0, 0), &matVS2WS);

	fMatrixWS2CS((vmmat44f*)&pCBVrVolumeObj->dxmatAlginedWS, (vmfloat3*)&f3PosVObjectMinWS, (vmfloat3*)&f3VecVObjectYWS, (vmfloat3*)&f3VecVObjectZWS); // set

	vmdouble4 d4ShadingFactor = vmdouble4(0.4, 0.6, 0.8, 70); 
	pCLObjectForParameters->GetDstObjValue(obj_param_src_id, "_double4_ShadingFactors", &d4ShadingFactor);
	pCBVrVolumeObj->f4ShadingFactor = D3DXVECTOR4((float)d4ShadingFactor.x, (float)d4ShadingFactor.y, (float)d4ShadingFactor.z, (float)d4ShadingFactor.w);
	
	vmmat44 dmatClipWS2BS;
	vmdouble3 d3PosOrthoMaxBox;
	D3DXMatrixIdentity(&pCBVrVolumeObj->dxmatClipBoxTransform);
	int iClippingMode = 0;
	pCLObjectForParameters->GetDstObjValue(obj_param_src_id, "_int_ClippingMode", &iClippingMode);
	if(pCLObjectForParameters->GetDstObjValue(obj_param_src_id, "_matrix44_MatrixClipWS2BS", &dmatClipWS2BS)
		&& pCLObjectForParameters->GetDstObjValue(obj_param_src_id, "_double3_PosClipBoxMaxWS", &d3PosOrthoMaxBox))
	{
		pCBVrVolumeObj->dxmatClipBoxTransform = *(D3DXMATRIX*)&(vmmat44f)dmatClipWS2BS;
		pCBVrVolumeObj->f3ClipBoxOrthoMaxWS = *(D3DXVECTOR3*)&vmfloat3(d3PosOrthoMaxBox);
	}

	pCBVrVolumeObj->iFlags = 0x3 & iClippingMode;

	double dClipPlaneIntensity = 1.0;
	pCLObjectForParameters->GetDstObjValue(obj_param_src_id, "_double_ClipPlaneIntensity", &dClipPlaneIntensity);
	pCBVrVolumeObj->fClipPlaneIntensity = (float)dClipPlaneIntensity;

	vmdouble3 d3PosOnPlane, d3VecPlane;
	if(pCLObjectForParameters->GetDstObjValue(obj_param_src_id, "_double3_PosClipPlaneWS", &d3PosOnPlane)
		&& pCLObjectForParameters->GetDstObjValue(obj_param_src_id, "_double3_VecClipPlaneWS", &d3VecPlane))
	{
		pCBVrVolumeObj->f3PosClipPlaneWS = *(D3DXVECTOR3*)&vmfloat3(d3PosOnPlane);
		pCBVrVolumeObj->f3VecClipPlaneWS = *(D3DXVECTOR3*)&vmfloat3(d3VecPlane);
	}

	if (pVolumeData->store_dtype.type_bytes == data_type::dtype<byte>().type_bytes) // char
		pCBVrVolumeObj->fSampleValueRange = 255.f;
	else if (pVolumeData->store_dtype.type_bytes == data_type::dtype<ushort>().type_bytes) // short
		pCBVrVolumeObj->fSampleValueRange = 65535.f;
	else  return false;
	
	//D3DXVECTOR3 f3VecSampleDirWS;
	//pCCObject->GetCameraExtState(NULL, (vmfloat3*)&f3VecSampleDirWS, NULL);
	//D3DXVECTOR3 f3VecSampleDirVS;
	//D3DXVec3TransformNormal(&f3VecSampleDirVS, &f3VecSampleDirWS, (D3DXMATRIX*)&pCVObjectVolume->GetMatrixWS2OSf());
	//D3DXVec3Normalize(&f3VecSampleDirVS, &f3VecSampleDirVS);
	//pCBVrVolumeObj->fSampleDistWS = D3DXVec3Length(&f3VecSampleDirWS);// *0.5f;
	//D3DXVec3TransformNormal(&f3VecSampleDirWS, &f3VecSampleDirVS, (D3DXMATRIX*)&pCVObjectVolume->GetMatrixOS2WSf());
	float minDistSample = (float)min(min(pVolumeData->vox_pitch.x, pVolumeData->vox_pitch.y), pVolumeData->vox_pitch.z);
	pCBVrVolumeObj->fSampleDistWS = minDistSample;
	fTransformVector((vmfloat3*)&pCBVrVolumeObj->f3VecGradientSampleX, &vmfloat3(pCBVrVolumeObj->fSampleDistWS, 0, 0), (vmmat44f*)&pCBVrVolumeObj->dxmatWS2TS);
	fTransformVector((vmfloat3*)&pCBVrVolumeObj->f3VecGradientSampleY, &vmfloat3(0, pCBVrVolumeObj->fSampleDistWS, 0), (vmmat44f*)&pCBVrVolumeObj->dxmatWS2TS);
	fTransformVector((vmfloat3*)&pCBVrVolumeObj->f3VecGradientSampleZ, &vmfloat3(0, 0, pCBVrVolumeObj->fSampleDistWS), (vmmat44f*)&pCBVrVolumeObj->dxmatWS2TS);
	pCBVrVolumeObj->fOpaqueCorrection = 1.f;
	if(bIsDownSample)
	{
// 		pCBVrVolumeObj->fSampleDistWS *= 0.5f;
// 		pCBVrVolumeObj->fOpaqueCorrection *= 0.5f;
// 
// 		pCBVrVolumeObj->f3VecGradientSampleX *= 2.f;
// 		pCBVrVolumeObj->f3VecGradientSampleY *= 2.f;
// 		pCBVrVolumeObj->f3VecGradientSampleZ *= 2.f;
	}
	/*pCBVrVolumeObj->f3VecGradientSampleX /= 2.f;
	pCBVrVolumeObj->f3VecGradientSampleY /= 2.f;
	pCBVrVolumeObj->f3VecGradientSampleZ /= 2.f;*/

	// Matrix Transpose //
	/////D3DXMatrixTranspose(&pCBVrVolumeObj->dxmatWS2TS, &pCBVrVolumeObj->matWS2TS);
	/////D3DXMatrixTranspose(&pCBVrVolumeObj->dxmatAlginedWS, &pCBVrVolumeObj->matAlginedWS);
	/////D3DXMatrixTranspose(&pCBVrVolumeObj->dxmatClipBoxTransform, &pCBVrVolumeObj->matClipBoxTransform);
	pCBVrVolumeObj->f3SizeVolumeCVS = D3DXVECTOR3((float)i3SizeVolumeTextureElements.x, (float)i3SizeVolumeTextureElements.y, (float)i3SizeVolumeTextureElements.z);

	// from pmapDValueVolume //
	double dSamplePrecisionLevel = 1.0;
	bool bIsForcedVzThickness = false;
	double dThicknessVirtualzSlab = (double)pCBVrVolumeObj->fSampleDistWS;
	double dVoxelSharpnessForAttributeVolume = 0.25;

	if(pCLObjectForParameters != NULL)
	{
		pCLObjectForParameters->GetDstObjValue(obj_param_src_id, "_double_SamplePrecisionLevel", &dSamplePrecisionLevel);
		pCLObjectForParameters->GetDstObjValue(obj_param_src_id, "_double_VZThickness", &dThicknessVirtualzSlab);
		pCLObjectForParameters->GetDstObjValue(obj_param_src_id, "_double_VoxelSharpnessForAttributeVolume", &dVoxelSharpnessForAttributeVolume);
	}

	float fSamplePrecisionLevel = (float)dSamplePrecisionLevel;
	if(fSamplePrecisionLevel > 0)
	{
		pCBVrVolumeObj->fSampleDistWS /= fSamplePrecisionLevel;
		pCBVrVolumeObj->fOpaqueCorrection /= fSamplePrecisionLevel;
	}
	pCBVrVolumeObj->fThicknessVirtualzSlab = (float)dThicknessVirtualzSlab;	// 현재 HLSL 은 Sample Distance 를 강제로 사용 중...
	pCBVrVolumeObj->fVoxelSharpnessForAttributeVolume = (float)dVoxelSharpnessForAttributeVolume;

	return true;
}

// _double_VZThickness 처리 함수...

bool HDx11ComputeConstantBufferVrBlockObj(CB_VrBlockObject* pCBVrBlockObj, VmVObjectVolume* pCVolume, int iLevelBlock, float fScaleValue)
{
	pCBVrBlockObj->fSampleValueRange = fScaleValue;

	VolumeData* pVolumeData = pCVolume->GetVolumeData();
	VolumeBlocks* psvxBlock = pCVolume->GetVolumeBlock(iLevelBlock);
	if(psvxBlock == NULL)
		return false;

	pCBVrBlockObj->f3SizeBlockTS = D3DXVECTOR3((float)psvxBlock->unitblk_size.x / (float)pVolumeData->vol_size.x
		, (float)psvxBlock->unitblk_size.y / (float)pVolumeData->vol_size.y, (float)psvxBlock->unitblk_size.z / (float)pVolumeData->vol_size.z);
	return true;
}

bool HDx11ComputeConstantBufferVrBrickChunk(CB_VrBrickChunk* pCBVrBrickChunk, VmVObjectVolume* pCVolume, int iLevelBlock, GpuResource* psvxResBrickChunk)
{
	::MessageBoxA(NULL, "deprecated", NULL, MB_OK);
	/*
	VolumeBlocks* psvxBlock = pCVolume->GetVolumeBlock(iLevelBlock);
	if (psvxBlock == NULL)
		return false;

	vmint3 i3NumElement = psvxResBrickChunk->GetLogicalElements();
	pCBVrBrickChunk->f3SizeBrickFullTS.x = (float)(psvxBlock->unitblk_size.x + psvxBlock->iBrickExtraSize * 2) / (float)i3NumElement.x;
	pCBVrBrickChunk->f3SizeBrickFullTS.y = (float)(psvxBlock->unitblk_size.y + psvxBlock->iBrickExtraSize * 2) / (float)i3NumElement.y;
	pCBVrBrickChunk->f3SizeBrickFullTS.z = (float)(psvxBlock->unitblk_size.z + psvxBlock->iBrickExtraSize * 2) / (float)i3NumElement.z;

	int iNumBricksX = i3NumElement.x / (psvxBlock->unitblk_size.x + psvxBlock->iBrickExtraSize * 2);
	int iNumBricksY = i3NumElement.y / (psvxBlock->unitblk_size.y + psvxBlock->iBrickExtraSize * 2);
	pCBVrBrickChunk->iNumBricksInChunkXY = iNumBricksX * iNumBricksY;
	pCBVrBrickChunk->iNumBricksInChunkX = iNumBricksX;

	pCBVrBrickChunk->f3SizeBrickExTS.x = (float)(psvxBlock->iBrickExtraSize) / (float)i3NumElement.x;
	pCBVrBrickChunk->f3SizeBrickExTS.y = (float)(psvxBlock->iBrickExtraSize) / (float)i3NumElement.y;
	pCBVrBrickChunk->f3SizeBrickExTS.z = (float)(psvxBlock->iBrickExtraSize) / (float)i3NumElement.z;

	VolumeData* pVolumeData = pCVolume->GetVolumeData();
	pCBVrBrickChunk->f3ConvertRatioVTS2CTS = D3DXVECTOR3(
		(float)pVolumeData->vol_size.x / (float)i3NumElement.x,
		(float)pVolumeData->vol_size.y / (float)i3NumElement.y,
		(float)pVolumeData->vol_size.z / (float)i3NumElement.z
	);
	/**/
	return true;
}

bool HDx11ComputeConstantBufferVrCamera(CB_VrCameraState* pCBVrCamera, VmCObject* pCCObject, vmint2 i2ScreenSizeFB, map<string, void*>* pmapCustomParameter)
{
	vmmat44 dmatSS2PS, dmatPS2CS, dmatCS2WS;
	pCCObject->GetMatrixSStoWS((vmmat44*)&dmatSS2PS, (vmmat44*)&dmatPS2CS, (vmmat44*)&dmatCS2WS);
	pCBVrCamera->dxmatSS2WS = *(D3DXMATRIX*)&(vmmat44f)(dmatSS2PS*(dmatPS2CS*dmatCS2WS));

	pCCObject->GetCameraExtStatef((vmfloat3*)&pCBVrCamera->f3PosEyeWS, (vmfloat3*)&pCBVrCamera->f3VecViewWS, NULL);
	
	vmint2 i2SizeFB = i2ScreenSizeFB;
	pCBVrCamera->uiScreenSizeX = i2SizeFB.x;
	pCBVrCamera->uiScreenSizeY = i2SizeFB.y;
	uint uiNumGridX = (uint)ceil(i2SizeFB.x / (float)__BLOCKSIZE);
	pCBVrCamera->uiGridOffsetX = uiNumGridX * __BLOCKSIZE;

	bool bIsLightOnCamera = true;
	__GetParameterFromCustomStringMap<bool>(&bIsLightOnCamera, pmapCustomParameter, ("_bool_IsLightOnCamera"));
	vmdouble3 d3VecLightWS(pCBVrCamera->f3VecViewWS.x, pCBVrCamera->f3VecViewWS.y, pCBVrCamera->f3VecViewWS.z);
	vmdouble3 d3PosLightWS(pCBVrCamera->f3PosEyeWS.x, pCBVrCamera->f3PosEyeWS.y, pCBVrCamera->f3PosEyeWS.z);
	if(!bIsLightOnCamera)
	{
		__GetParameterFromCustomStringMap<vmdouble3>(&d3VecLightWS, pmapCustomParameter, ("_double3_VecLightWS"));
		__GetParameterFromCustomStringMap<vmdouble3>(&d3PosLightWS, pmapCustomParameter, ("_double3_PosLightWS"));
	}
	pCBVrCamera->f3PosGlobalLightWS = *(D3DXVECTOR3*)&vmfloat3(d3PosLightWS);
	pCBVrCamera->f3VecGlobalLightWS = *(D3DXVECTOR3*)&vmfloat3(d3VecLightWS);
	D3DXVec3Normalize(&pCBVrCamera->f3VecGlobalLightWS, &pCBVrCamera->f3VecGlobalLightWS);
	
	pCBVrCamera->iFlags = 0;
	if(pCCObject->IsPerspective())
		pCBVrCamera->iFlags |= 0x1;
	bool bIsSpotLight = false;
	__GetParameterFromCustomStringMap<bool>(&bIsSpotLight, pmapCustomParameter, ("_bool_IsPointSpotLight"));
	if(bIsSpotLight)
		pCBVrCamera->iFlags |= 0x2;
	
	// Matrix Transpose //
	/////D3DXMatrixTranspose(&pCBVrCamera->matSS2WS, &pCBVrCamera->matSS2WS);

	return true;
}

bool HDx11ComputeConstantBufferVrOtf1D(CB_VrOtf1D* pCBVrOtf1D, VmTObject* pCTObject, int iOtfIndex, map<string, void*>* pmapCustomParameter)
{
	TMapData* psvxTfArchive = pCTObject->GetTMapData();
	pCBVrOtf1D->iOtf1stValue = psvxTfArchive->valid_min_idx.x;
	pCBVrOtf1D->iOtfLastValue = psvxTfArchive->valid_max_idx.x;
	pCBVrOtf1D->iOtfSize = psvxTfArchive->array_lengths.x;
	vmbyte4 y4ColorEnd = ((vmbyte4**)psvxTfArchive->tmap_buffers)[0][psvxTfArchive->array_lengths.x - 1];
	pCBVrOtf1D->f4OtfColorEnd = D3DXVECTOR4(y4ColorEnd.x / 255.f, y4ColorEnd.y / 255.f, y4ColorEnd.z / 255.f, y4ColorEnd.w / 255.f);
	if (iOtfIndex == 1)
	{
		// Deprecated //
		//pCBVrOtf1D->f4OtfColorEnd += *(D3DXVECTOR4*)&((vmfloat4**)psvxTfArchive->tmap_buffers)[1][psvxTfArchive->array_lengths.x - 1];
	}

	return true;
}

bool HDx11ComputeConstantBufferVrSurfaceEffect(CB_VrSurfaceEffect* pCBVrSurfaceEffect, float fDistSample, VmLObject* pCLObjectForParameters, int obj_param_src_id)
{
	bool bApplySurfaceAO = false;
	pCLObjectForParameters->GetDstObjValue(obj_param_src_id, ("_bool_ApplySurfaceAO"), &bApplySurfaceAO);
	bool bApplyAnisotropicBRDF = false;
	pCLObjectForParameters->GetDstObjValue(obj_param_src_id, ("_bool_ApplyAnisotropicBRDF"), &bApplyAnisotropicBRDF);
// 	bool bApplyShadingFactor = true;
// 	pCLObjectForParameters->GetDstObjValue(obj_param_src_id, ("_bool_ApplyShadingFactor"), &bApplyShadingFactor);
	int iOcclusionNumSamplesPerKernelRay = 5;
	pCLObjectForParameters->GetDstObjValue(obj_param_src_id, ("_int_OcclusionNumSamplesPerKernelRay"), &iOcclusionNumSamplesPerKernelRay);
	double dOcclusionSampleStepDistOfKernelRay = (double)(fDistSample * 3.f);
	pCLObjectForParameters->GetDstObjValue(obj_param_src_id, ("_double_OcclusionSampleStepDistOfKernelRay"), &dOcclusionSampleStepDistOfKernelRay);
	double dPhongBRDF_DiffuseRatio = 0.5;
	pCLObjectForParameters->GetDstObjValue(obj_param_src_id, ("_double_PhongBRDFDiffuseRatio"), &dPhongBRDF_DiffuseRatio);
	double dPhongBRDF_ReflectanceRatio = 0.5;
	pCLObjectForParameters->GetDstObjValue(obj_param_src_id, ("_double_PhongBRDFReflectanceRatio"), &dPhongBRDF_ReflectanceRatio);
	double dPhongExpWeightU = 50;
	pCLObjectForParameters->GetDstObjValue(obj_param_src_id, ("_double_PhongExpWeightU"), &dPhongExpWeightU);
	double dPhongExpWeightV = 50;
	pCLObjectForParameters->GetDstObjValue(obj_param_src_id, ("_double_PhongExpWeightV"), &dPhongExpWeightV);

	pCBVrSurfaceEffect->iIsoSurfaceRenderingMode = (int)bApplySurfaceAO | ((int)bApplyAnisotropicBRDF << 1);// | ((int)bApplyShadingFactor << 2);
	pCBVrSurfaceEffect->iOcclusionNumSamplesPerKernelRay = iOcclusionNumSamplesPerKernelRay;
	pCBVrSurfaceEffect->fOcclusionSampleStepDistOfKernelRay = (float)dOcclusionSampleStepDistOfKernelRay;
	pCBVrSurfaceEffect->fPhongBRDF_DiffuseRatio = (float)dPhongBRDF_DiffuseRatio;
	pCBVrSurfaceEffect->fPhongBRDF_ReflectanceRatio = (float)dPhongBRDF_ReflectanceRatio;
	pCBVrSurfaceEffect->fPhongExpWeightU = (float)dPhongExpWeightU;
	pCBVrSurfaceEffect->fPhongExpWeightV = (float)dPhongExpWeightV;

	// Curvature
	int iSizeKernel = 3;
	float fThetaCurvatureTable = 0;
	float fRadiusCurvatureTable = 0;
	bool bIsConcavenessDirection = true;
	int iModeCurvatureDescription = 0;	// 0 : Normal Curvature Map (2D), 1 : Apply Concaveness
	if (iModeCurvatureDescription == 1)
	{
		pCLObjectForParameters->GetDstObjValue(obj_param_src_id, ("_int_SizeKernelInVoxel"), &iSizeKernel);
		pCLObjectForParameters->GetDstObjValue(obj_param_src_id, ("_int_ModeCurvatureDescription"), &iModeCurvatureDescription);
		double dThetaCurvatureTable = 0;
		pCLObjectForParameters->GetDstObjValue(obj_param_src_id, ("_double_ThetaCurvatureTable"), &dThetaCurvatureTable);
		fThetaCurvatureTable = (float)dThetaCurvatureTable;
		double dRadiusCurvatureTable = 0;
		pCLObjectForParameters->GetDstObjValue(obj_param_src_id, ("_double_RadiusCurvatureTable"), &dRadiusCurvatureTable);
		fRadiusCurvatureTable = (float)dRadiusCurvatureTable;

		pCLObjectForParameters->GetDstObjValue(obj_param_src_id, ("_bool_IsConcavenessDirection"), &bIsConcavenessDirection);
	}
	pCBVrSurfaceEffect->iIsoSurfaceRenderingMode |= (iModeCurvatureDescription << 4) | (bIsConcavenessDirection << 5);
	pCBVrSurfaceEffect->iSizeCurvatureKernel = iSizeKernel;
	pCBVrSurfaceEffect->fThetaCurvatureTable = fThetaCurvatureTable;
	pCBVrSurfaceEffect->fRadiusCurvatureTable = fRadiusCurvatureTable;

	return true;
}

bool HDx11ComputeConstantBufferVrInterestingRegion(CB_VrInterestingRegion* pCBVrInterestingRegion, VmLObject* pCLObjectForParameters, int obj_param_src_id)
{
	pCBVrInterestingRegion->iNumRegions = 0;
	pCLObjectForParameters->GetDstObjValue(obj_param_src_id, ("_int_NumRoiPoints"), &pCBVrInterestingRegion->iNumRegions);
	if (pCBVrInterestingRegion->iNumRegions == 0)
		return true;

	vmdouble3 d3RoiPoint0WS, d3RoiPoint1WS, d3RoiPoint2WS;
	pCLObjectForParameters->GetDstObjValue(obj_param_src_id, ("_double3_RoiPoint0"), &d3RoiPoint0WS);
	pCLObjectForParameters->GetDstObjValue(obj_param_src_id, ("_double3_RoiPoint1"), &d3RoiPoint1WS);
	pCLObjectForParameters->GetDstObjValue(obj_param_src_id, ("_double3_RoiPoint2"), &d3RoiPoint2WS);
	pCBVrInterestingRegion->f3PosPtn0WS = *(D3DXVECTOR3*)&vmfloat3(d3RoiPoint0WS);
	pCBVrInterestingRegion->f3PosPtn1WS = *(D3DXVECTOR3*)&vmfloat3(d3RoiPoint1WS);
	pCBVrInterestingRegion->f3PosPtn2WS = *(D3DXVECTOR3*)&vmfloat3(d3RoiPoint2WS);

	double dRadius0WS, dRadius1WS, dRadius2WS;
	pCLObjectForParameters->GetDstObjValue(obj_param_src_id, ("_double_RoiRadius0"), &dRadius0WS);
	pCLObjectForParameters->GetDstObjValue(obj_param_src_id, ("_double_RoiRadius1"), &dRadius1WS);
	pCLObjectForParameters->GetDstObjValue(obj_param_src_id, ("_double_RoiRadius2"), &dRadius2WS);
	pCBVrInterestingRegion->fRadius0 = (float)dRadius0WS;
	pCBVrInterestingRegion->fRadius1 = (float)dRadius1WS;
	pCBVrInterestingRegion->fRadius2 = (float)dRadius2WS;

	return true;
}

bool GradientMagnitudeAnalysis(vmdouble2* pd2GradientMagMinMax, VmVObjectVolume* pCVolume, LocalProgress* _progress)
{
	if (pCVolume->GetCustomParameter(("_double2_GraidentMagMinMax"), data_type::dtype<vmdouble2>(), pd2GradientMagMinMax))
		return true;

	const VolumeData* pVolData = pCVolume->GetVolumeData();
	if(pVolData->store_dtype.type_name != data_type::dtype<ushort>().type_name)
		return false;

	if(_progress)
	{
		_progress->range = 70;
	}

	ushort** ppusVolume = (ushort**)pVolData->vol_slices;
	int iSizeAddrX = pVolData->vol_size.x + pVolData->bnd_size.x*2;
	vmdouble2 d2GradMagMinMax;
	d2GradMagMinMax.x = DBL_MAX;
	d2GradMagMinMax.y = -DBL_MAX;
	for(int iZ = 1; iZ < pVolData->vol_size.z - 1; iZ+=2)
	{
		if(_progress)
		{
			*_progress->progress_ptr = 
				(int)(_progress->start + _progress->range*iZ/pVolData->vol_size.z);
		}

		for(int iY = 1; iY < pVolData->vol_size.y - 1; iY+=2)
		{
			for(int iX = 1; iX < pVolData->vol_size.x - 1; iX+=2)
			{
				vmdouble3 d3Difference;
				int iAddrZ = iZ + pVolData->bnd_size.x;
				int iAddrY = iY + pVolData->bnd_size.y;
				int iAddrX = iX + pVolData->bnd_size.z;
				int iAddrZL = iZ - 1 + pVolData->bnd_size.z;
				int iAddrYL = iY - 1 + pVolData->bnd_size.y;
				int iAddrXL = iX - 1 + pVolData->bnd_size.x;
				int iAddrZR = iZ + 1 + pVolData->bnd_size.z;
				int iAddrYR = iY + 1 + pVolData->bnd_size.y;
				int iAddrXR = iX + 1 + pVolData->bnd_size.x;
				d3Difference.x = (double)((int)ppusVolume[iAddrZ][iAddrY*iSizeAddrX + iAddrXR] - (int)ppusVolume[iAddrZ][iAddrY*iSizeAddrX + iAddrXL]);
				d3Difference.y = (double)((int)ppusVolume[iAddrZ][iAddrYR*iSizeAddrX + iAddrX] - (int)ppusVolume[iAddrZ][iAddrYL*iSizeAddrX + iAddrX]);
				d3Difference.z = (double)((int)ppusVolume[iAddrZR][iAddrY*iSizeAddrX + iAddrX] - (int)ppusVolume[iAddrZL][iAddrY*iSizeAddrX + iAddrX]);
				double dGradientMag = sqrt(d3Difference.x*d3Difference.x + d3Difference.y*d3Difference.y + d3Difference.z*d3Difference.z);
				d2GradMagMinMax.x = min(d2GradMagMinMax.x, dGradientMag);
				d2GradMagMinMax.y = max(d2GradMagMinMax.y, dGradientMag);
			}
		}
	}
	*pd2GradientMagMinMax = d2GradMagMinMax;

	pCVolume->RegisterCustomParameter(("_double2_GraidentMagMinMax"), d2GradMagMinMax);

	if(_progress)
	{
		_progress->start = _progress->start + _progress->range;
		*_progress->progress_ptr = (_progress->start);
		_progress->range = 30;
	}

	return true;
}

bool HDx11ComputeConstantBufferVrModulation(CB_VrModulation* pCBVrModulation, VmVObjectVolume* pCVObjectVolume, vmfloat3 f3PosEyeWS, VmLObject* pCLObjectForParameters, int obj_param_src_id)
{
	double dGradMagAmplifier = 1.0;
	pCLObjectForParameters->GetDstObjValue(obj_param_src_id, ("_double_GradMagAmplifier"), &dGradMagAmplifier);

	vmdouble2 d2GradientMagMinMax;
	if(!GradientMagnitudeAnalysis(&d2GradientMagMinMax, pCVObjectVolume, NULL))
		return false;
	float fMaxGradientSize = (float)(d2GradientMagMinMax.y/65535.0);

	pCBVrModulation->fGradMagAmplifier = (float)dGradMagAmplifier;
	pCBVrModulation->fMaxGradientSize = fMaxGradientSize;

	return true;
}

bool HDx11ComputeConstantBufferVrShadowMap(CB_VrShadowMap* pCBVrShadowMap, CB_VrCameraState* pCBVrCamStateForShadowMap, 
	vmfloat3 f3PosOverviewBoxMinWS, vmfloat3 f3PosOverviewBoxMaxWS, map<string, void*>* pmapCustomParameter)
{
	double dShadowOcclusionWeight = 0.5;
	__GetParameterFromCustomStringMap<double>(&dShadowOcclusionWeight, pmapCustomParameter, ("_double_ShadowOcclusionWeight"));
	pCBVrShadowMap->fShadowOcclusionWeight = (float)dShadowOcclusionWeight;					// SM

	int iDepthBiasSampleCount = 5;
	__GetParameterFromCustomStringMap<int>(&iDepthBiasSampleCount, pmapCustomParameter, ("_int_DepthBiasSampleCount"));
	pCBVrShadowMap->fDepthBiasSampleCount = (float)iDepthBiasSampleCount;					// SM

	pCBVrCamStateForShadowMap->f3PosEyeWS = pCBVrCamStateForShadowMap->f3PosGlobalLightWS;	//	CS
	pCBVrCamStateForShadowMap->f3VecViewWS = pCBVrCamStateForShadowMap->f3VecGlobalLightWS;	//	CS
	
	// Forced Image Plane Width and Height
	D3DXVECTOR3 f3PosOrthoMinWS(f3PosOverviewBoxMinWS.x, f3PosOverviewBoxMinWS.y, f3PosOverviewBoxMinWS.z);
	D3DXVECTOR3 f3PosOrthoMaxWS(f3PosOverviewBoxMaxWS.x, f3PosOverviewBoxMaxWS.y, f3PosOverviewBoxMaxWS.z);
	D3DXVECTOR3 f3PosCenterWS = (f3PosOrthoMaxWS + f3PosOrthoMinWS) * 0.5f;
	float fDistCenter2Light = D3DXVec3Length(&(f3PosCenterWS - pCBVrCamStateForShadowMap->f3PosEyeWS));
	float fDiagonalLength = D3DXVec3Length(&(f3PosOrthoMaxWS - f3PosOrthoMinWS));
	if(!(pCBVrCamStateForShadowMap->iFlags & 0x2))
		pCBVrCamStateForShadowMap->f3PosEyeWS = f3PosCenterWS - pCBVrCamStateForShadowMap->f3VecViewWS * fDiagonalLength;

	D3DXMATRIX dxmatSS2PS, dxmatPS2CS, dxmatCS2WS;
	D3DXMATRIX dxmatWS2CS, dxmatCS2PS, dxmatPS2SS;

	D3DXVECTOR3 f3VecUp(0, 0, 1.f);
	D3DXVECTOR3 f3VecRightTmp;
	while(D3DXVec3Length(D3DXVec3Cross(&f3VecRightTmp, &pCBVrCamStateForShadowMap->f3VecViewWS, &f3VecUp)) == 0)
		f3VecUp = f3VecUp + D3DXVECTOR3(0, 1.f, 0);
	D3DXMatrixLookAtRH(&pCBVrShadowMap->dxmatWS2WLS, &pCBVrCamStateForShadowMap->f3PosEyeWS, 
		&(pCBVrCamStateForShadowMap->f3PosEyeWS + pCBVrCamStateForShadowMap->f3VecViewWS), &f3VecUp);	// SM

	dxmatWS2CS = pCBVrShadowMap->dxmatWS2WLS;
	if(pCBVrCamStateForShadowMap->iFlags & 0x2)
	{
		D3DXMatrixPerspectiveFovRH(&dxmatCS2PS, D3DX_PI/4.f, 1.f, 0.1f, fDistCenter2Light + fDiagonalLength/2);
	}
	else
	{
		D3DXMatrixOrthoRH(&dxmatCS2PS, fDiagonalLength, fDiagonalLength, 0, fDistCenter2Light + fDiagonalLength/2);
	}
	fMatrixPS2SS((vmmat44f*)&dxmatPS2SS, (float)pCBVrCamStateForShadowMap->uiRenderingSizeX, (float)pCBVrCamStateForShadowMap->uiRenderingSizeY);
	D3DXMatrixTranspose(&dxmatPS2SS, &dxmatPS2SS);

	D3DXMatrixInverse(&dxmatCS2WS, NULL, &dxmatWS2CS);
	D3DXMatrixInverse(&dxmatPS2CS, NULL, &dxmatCS2PS);
	D3DXMatrixInverse(&dxmatSS2PS, NULL, &dxmatPS2SS);

	D3DXMATRIX dxmatRS2SS;
	D3DXMatrixIdentity(&dxmatRS2SS);
	dxmatRS2SS._11 = pCBVrCamStateForShadowMap->dxmatRS2SS._11;
	dxmatRS2SS._22 = pCBVrCamStateForShadowMap->dxmatRS2SS._22;
	//matRS2SS[3][0] = pCBVrCamStateForShadowMap->matRS2SS._14;
	//matRS2SS[3][1] = pCBVrCamStateForShadowMap->matRS2SS._24;

	pCBVrCamStateForShadowMap->dxmatSS2WS = dxmatSS2PS*(dxmatPS2CS*dxmatCS2WS);	// CS
	pCBVrCamStateForShadowMap->dxmatRS2SS = dxmatRS2SS;

	D3DXMATRIX dxmatSS2RS;
	D3DXMatrixIdentity(&dxmatSS2RS);
	dxmatSS2RS._11 = 1.f / pCBVrCamStateForShadowMap->dxmatRS2SS._11;
	dxmatSS2RS._22 = 1.f / pCBVrCamStateForShadowMap->dxmatRS2SS._22;
	//matRS2SS[3][0] = -pCBVrCamStateForShadowMap->matRS2SS._14;
	//matRS2SS[3][1] = -pCBVrCamStateForShadowMap->matRS2SS._24;

	//D3DXMATRIX _matRS2SS;
	///////D3DXMatrixTranspose(&_matRS2SS, &pCBVrCamStateForShadowMap->matRS2SS);
	//D3DXMATRIX _matSS2RS;
	//D3DXMatrixInverse(&_matSS2RS, NULL, &_matRS2SS);

	pCBVrShadowMap->dxmatWS2LS = dxmatWS2CS*dxmatCS2PS*dxmatPS2SS*dxmatSS2RS; // SM

	D3DXMatrixTranspose(&pCBVrShadowMap->dxmatWS2WLS, &pCBVrShadowMap->dxmatWS2WLS);
	D3DXMatrixTranspose(&pCBVrShadowMap->dxmatWS2LS, &pCBVrShadowMap->dxmatWS2LS);
	D3DXMatrixTranspose(&pCBVrCamStateForShadowMap->dxmatSS2WS, &pCBVrCamStateForShadowMap->dxmatSS2WS);
	//pCBVrCamStateForShadowMap->matRS2SS._14 = pCBVrCamStateForShadowMap->matRS2SS._24 = 0;

	// 1st bit - 0 : Orthogonal or 1 : Perspective
	// 2nd bit - 0 : Parallel Light or 1 : Point Spot Light
	if(pCBVrCamStateForShadowMap->iFlags & 0x2)
		pCBVrCamStateForShadowMap->iFlags |= 0x1;	// SM
	else
		pCBVrCamStateForShadowMap->iFlags &= 0x0;

// 	D3DXVECTOR3 f3VecViewVS, f3VecViewWS;
// 	D3DXVec3TransformNormal(&f3VecViewVS, &pCBVrCamStateForShadowMap->f3VecViewWS, (D3DXMATRIX*)&pCVObjectVolume->GetMatrixWS2OSf());
// 	D3DXVec3Normalize(&f3VecViewVS, &f3VecViewVS);
// 	D3DXVec3TransformNormal(&f3VecViewWS, &f3VecViewVS, (D3DXMATRIX*)&pCVObjectVolume->GetMatrixOS2WSf());
// 	pCBVrShadowMap->fSampleDistWLS = D3DXVec3Length(&f3VecViewWS) * 0.5f;	// SM
	pCBVrShadowMap->fOpaqueCorrectionSMap = 1.f;							// CS

// 	double dLightSamplePrecisionLevel = 2;
// 	__GetParameterFromCustomStringMap(&dLightSamplePrecisionLevel, pmapCustomParameter, ("_double_LightSamplePrecisionLevel"));
// 	if(dLightSamplePrecisionLevel > 0)
// 	{
// 		pCBVrShadowMap->fOpaqueCorrectionSMap /= (float)dLightSamplePrecisionLevel;	// SM
// 		pCBVrShadowMap->fSampleDistWLS /= (float)dLightSamplePrecisionLevel;
// 	}

	return true;
}


// Check Asynchronous Call //

//static std::atomic<bool> g_ready(true);
static std::mutex g_mutex_GpuCriticalPath;	// CPU Rendering 에서 mutex 없애기, GPU operations 는 하나의 thread 로만 처리되야 한다.
void* HDx11GetMutexGpuCriticalPath()
{
	return &g_mutex_GpuCriticalPath;
}

// bool HDx11MixOut(VmIObject* pCIObjectMerger, VmIObject* pCIObjectSystemOut, int iLOD)
// {
// 	g_mutex_GpuCriticalPath.lock();
// 	if (g_pCGpuManager == NULL || pCIObjectMerger == NULL || !g_VmCommonParams.bIsInitialized)
// 	{
// 		GMERRORMESSAGE("VmGpuManager_v1 Fault!!");
// 		return false;
// 	}
// 	//ID3D11Device* pdx11Device;
// 	ID3D11DeviceContext* pdx11DeviceImmContext = g_VmCommonParams.pdx11DeviceImmContext;
// 	ID3D11RenderTargetView* pdxRTVOld = NULL;
// 	ID3D11DepthStencilView* pdxDSVOld = NULL;
// 	pdx11DeviceImmContext->OMGetRenderTargets(1, &pdxRTVOld, &pdxDSVOld);
// 
// #pragma region FB Setting
// 	bool bInitGen = false;
// 	GpuResDescriptor gpuResourcesFB_DESCs[__FB_COUNT];
// 	GpuResArchive gpuResourcesFB_ARCHs[__FB_COUNT];
// 	for (int i = 0; i < __FB_COUNT; i++)
// 	{
// 		gpuResourcesFB_DESCs[i].sdk_type = GpuSdkTypeDX11;
// 		gpuResourcesFB_DESCs[i].misc = 0;
// 		gpuResourcesFB_DESCs[i].src_obj_id = pCIObjectMerger->GetObjectID();
// 	}
// 	gpuResourcesFB_DESCs[__FB_TX2D_RENDEROUT].res_type = ResourceTypeDX11RESOURCE;
// 	gpuResourcesFB_DESCs[__FB_TX2D_RENDEROUT].usage_specifier = ("TEXTURE2D_IOBJECT_RENDEROUT");
// 	gpuResourcesFB_DESCs[__FB_TX2D_RENDEROUT].misc = NUM_TEXRT_LAYERS;
// 	gpuResourcesFB_DESCs[__FB_TX2D_RENDEROUT].dtype = data_type::dtype<vmbyte4>();
// 	gpuResourcesFB_DESCs[__FB_TX2D_RENDEROUT].iSizeStride = VXHGetDataTypeSizeByte(gpuResourcesFB_DESCs[__FB_TX2D_RENDEROUT].eDataType);
// 	gpuResourcesFB_DESCs[__FB_RTV_RENDEROUT] = gpuResourcesFB_DESCs[__FB_SRV_RENDEROUT] = gpuResourcesFB_DESCs[__FB_TX2D_RENDEROUT];
// 	gpuResourcesFB_DESCs[__FB_RTV_RENDEROUT].res_type = ResourceTypeDX11RTV;
// 	gpuResourcesFB_DESCs[__FB_SRV_RENDEROUT].res_type = ResourceTypeDX11SRV;
// 	gpuResourcesFB_DESCs[__FB_TX2D_DEPTH].res_type = ResourceTypeDX11RESOURCE;
// 	gpuResourcesFB_DESCs[__FB_TX2D_DEPTH].usage_specifier = ("TEXTURE2D_IOBJECT_DEPTHSTENCIL");
// 	gpuResourcesFB_DESCs[__FB_TX2D_DEPTH].dtype = data_type::dtype<float>();
// 	gpuResourcesFB_DESCs[__FB_TX2D_DEPTH].iSizeStride = VXHGetDataTypeSizeByte(gpuResourcesFB_DESCs[__FB_TX2D_DEPTH].eDataType);
// 	gpuResourcesFB_DESCs[__FB_DSV_DEPTH] = gpuResourcesFB_DESCs[__FB_TX2D_DEPTH];
// 	gpuResourcesFB_DESCs[__FB_DSV_DEPTH].res_type = ResourceTypeDX11DSV;
// 	gpuResourcesFB_DESCs[__FB_TX2D_DEPTH_ZTHICKCS].res_type = ResourceTypeDX11RESOURCE;
// 	gpuResourcesFB_DESCs[__FB_TX2D_DEPTH_ZTHICKCS].usage_specifier = ("TEXTURE2D_IOBJECT_RENDEROUT");
// 	gpuResourcesFB_DESCs[__FB_TX2D_DEPTH_ZTHICKCS].misc = NUM_TEXRT_LAYERS;
// 	gpuResourcesFB_DESCs[__FB_TX2D_DEPTH_ZTHICKCS].dtype = data_type::dtype<float>();
// 	gpuResourcesFB_DESCs[__FB_TX2D_DEPTH_ZTHICKCS].iSizeStride = VXHGetDataTypeSizeByte(gpuResourcesFB_DESCs[__FB_TX2D_DEPTH_ZTHICKCS].eDataType);
// 	gpuResourcesFB_DESCs[__FB_RTV_DEPTH_ZTHICKCS] = gpuResourcesFB_DESCs[__FB_SRV_DEPTH_ZTHICKCS] = gpuResourcesFB_DESCs[__FB_TX2D_DEPTH_ZTHICKCS];
// 	gpuResourcesFB_DESCs[__FB_RTV_DEPTH_ZTHICKCS].res_type = ResourceTypeDX11RTV;
// 	gpuResourcesFB_DESCs[__FB_SRV_DEPTH_ZTHICKCS].res_type = ResourceTypeDX11SRV;
// 	gpuResourcesFB_DESCs[__FB_TX2D_DEPTH_DUAL] = gpuResourcesFB_DESCs[__FB_TX2D_DEPTH];
// 	gpuResourcesFB_DESCs[__FB_TX2D_DEPTH_DUAL].usage_specifier = ("TEXTURE2D_IOBJECT_RENDEROUT");
// 	gpuResourcesFB_DESCs[__FB_TX2D_DEPTH_DUAL].res_type = ResourceTypeDX11RESOURCE;
// 	gpuResourcesFB_DESCs[__FB_SRV_DEPTH_DUAL] = gpuResourcesFB_DESCs[__FB_RTV_DEPTH_DUAL] = gpuResourcesFB_DESCs[__FB_TX2D_DEPTH_DUAL];
// 	gpuResourcesFB_DESCs[__FB_SRV_DEPTH_DUAL].res_type = ResourceTypeDX11SRV;
// 	gpuResourcesFB_DESCs[__FB_RTV_DEPTH_DUAL].res_type = ResourceTypeDX11RTV;
// 	for (int i = 0; i < __FB_COUNT; i++)
// 	{
// 		if (!g_pCGpuManager->GetGpuResourceArchive(&gpuResourcesFB_DESCs[i], &gpuResourcesFB_ARCHs[i]))
// 		{
// 			g_pCGpuManager->GenerateGpuResource(&gpuResourcesFB_DESCs[i], pCIObjectMerger, &gpuResourcesFB_ARCHs[i], NULL);
// 			bInitGen = true;
// 		}
// 	}
// 
// 	GpuResDescriptor gpuResourceMERGE_PS_DESCs[__EXFB_COUNT];
// 	GpuResArchive gpuResourceMERGE_PS_ARCHs[__EXFB_COUNT];
// 	for (int i = 0; i < __EXFB_COUNT; i++)
// 	{
// 		gpuResourceMERGE_PS_DESCs[i].sdk_type = GpuSdkTypeDX11;
// 		gpuResourceMERGE_PS_DESCs[i].usage_specifier = ("TEXTURE2D_IOBJECT_RENDEROUT");
// 		gpuResourceMERGE_PS_DESCs[i].misc = NUM_MERGE_LAYERS * 2;	// two of NUM_MERGE_LAYERS resource buffers
// 		gpuResourceMERGE_PS_DESCs[i].src_obj_id = pCIObjectMerger->GetObjectID();
// 		gpuResourceMERGE_PS_DESCs[i].dtype = data_type::dtype<vmfloat4>();	// RG : Int RGBA // B : DepthBack // A : Thickness
// 		gpuResourceMERGE_PS_DESCs[i].iSizeStride = VXHGetDataTypeSizeByte(gpuResourceMERGE_PS_DESCs[i].eDataType);
// 		switch (i)
// 		{
// 		case 0:
// 			gpuResourceMERGE_PS_DESCs[i].res_type = ResourceTypeDX11RESOURCE; break;
// 		case 1:
// 			gpuResourceMERGE_PS_DESCs[i].res_type = ResourceTypeDX11SRV; break;
// 		case 2:
// 			gpuResourceMERGE_PS_DESCs[i].res_type = ResourceTypeDX11RTV; break;
// 		}
// 	}
// 	for (int i = 0; i < __EXFB_COUNT; i++)
// 	{
// 		if (!g_pCGpuManager->GetGpuResourceArchive(&gpuResourceMERGE_PS_DESCs[i], &gpuResourceMERGE_PS_ARCHs[i]))
// 		{
// 			g_pCGpuManager->GenerateGpuResource(&gpuResourceMERGE_PS_DESCs[i], pCIObjectMerger, &gpuResourceMERGE_PS_ARCHs[i], NULL);
// 			bInitGen = true;
// 		}
// 	}
// 
// 	vmint2 i2SizeScreenCurrent, i2SizeScreenOld = vmint2(0, 0);
// 	pCIObjectMerger->GetFrameBufferInfo(&i2SizeScreenCurrent);
// 	if (bInitGen)
// 		pCIObjectMerger->RegisterCustomParameter(("_int2_PreviousScreenSize"), i2SizeScreenCurrent);
// 	pCIObjectMerger->GetCustomParameter(("_int2_PreviousScreenSize"), &i2SizeScreenOld);
// 	if (i2SizeScreenCurrent.x != i2SizeScreenOld.x || i2SizeScreenCurrent.y != i2SizeScreenOld.y)
// 	{
// 		g_pCGpuManager->ReleaseGpuResourcesRelatedObject(pCIObjectMerger->GetObjectID());
// 
// 		for (int i = 0; i < __EXFB_COUNT; i++)
// 		{
// 			g_pCGpuManager->GenerateGpuResource(&gpuResourceMERGE_PS_DESCs[i], pCIObjectMerger, &gpuResourceMERGE_PS_ARCHs[i], NULL);
// 		}
// 
// 		for (int i = 0; i < __FB_COUNT; i++)
// 		{
// 			g_pCGpuManager->GenerateGpuResource(&gpuResourcesFB_DESCs[i], pCIObjectMerger, &gpuResourcesFB_ARCHs[i], NULL);
// 		}
// 
// 		pCIObjectMerger->RegisterCustomParameter(("_int2_PreviousScreenSize"), i2SizeScreenCurrent);
// 	}
// 	if (pCIObjectSystemOut)
// 		pCIObjectSystemOut->RegisterCustomParameter(("_int2_PreviousScreenSize"), i2SizeScreenCurrent);
// 
// 	GpuResDescriptor gpuResourceFB_RENDEROUT_SYSTEM_DESC;
// 	GpuResDescriptor gpuResourceFB_DEPTH_SYSTEM_DESC;
// 	GpuResArchive gpuResourceFB_RENDEROUT_SYSTEM_ARCH;
// 	GpuResArchive gpuResourceFB_DEPTH_SYSTEM_ARCH;
// 	gpuResourceFB_RENDEROUT_SYSTEM_DESC.res_type = ResourceTypeDX11RESOURCE;
// 	gpuResourceFB_RENDEROUT_SYSTEM_DESC.dtype = data_type::dtype<vmbyte4>();
// 	gpuResourceFB_RENDEROUT_SYSTEM_DESC.sdk_type = GpuSdkTypeDX11;
// 	gpuResourceFB_RENDEROUT_SYSTEM_DESC.usage_specifier = ("TEXTURE2D_IOBJECT_SYSTEM");
// 	gpuResourceFB_RENDEROUT_SYSTEM_DESC.iSizeStride = VXHGetDataTypeSizeByte(gpuResourceFB_RENDEROUT_SYSTEM_DESC.eDataType);
// 	gpuResourceFB_RENDEROUT_SYSTEM_DESC.misc = 0;
// 	gpuResourceFB_RENDEROUT_SYSTEM_DESC.src_obj_id = pCIObjectMerger->GetObjectID();
// 	gpuResourceFB_DEPTH_SYSTEM_DESC.res_type = ResourceTypeDX11RESOURCE;
// 	gpuResourceFB_DEPTH_SYSTEM_DESC.dtype = data_type::dtype<float>();
// 	gpuResourceFB_DEPTH_SYSTEM_DESC.sdk_type = GpuSdkTypeDX11;
// 	gpuResourceFB_DEPTH_SYSTEM_DESC.usage_specifier = ("TEXTURE2D_IOBJECT_SYSTEM");
// 	gpuResourceFB_DEPTH_SYSTEM_DESC.iSizeStride = VXHGetDataTypeSizeByte(gpuResourceFB_DEPTH_SYSTEM_DESC.eDataType);
// 	gpuResourceFB_DEPTH_SYSTEM_DESC.misc = 0;
// 	gpuResourceFB_DEPTH_SYSTEM_DESC.src_obj_id = pCIObjectMerger->GetObjectID();
// 	// When Resize, the Framebuffer is released, so we do not have to check the resize case.
// 	if (!g_pCGpuManager->GetGpuResourceArchive(&gpuResourceFB_RENDEROUT_SYSTEM_DESC, &gpuResourceFB_RENDEROUT_SYSTEM_ARCH))
// 		g_pCGpuManager->GenerateGpuResource(&gpuResourceFB_RENDEROUT_SYSTEM_DESC, pCIObjectMerger, &gpuResourceFB_RENDEROUT_SYSTEM_ARCH, NULL);
// 	if (!g_pCGpuManager->GetGpuResourceArchive(&gpuResourceFB_DEPTH_SYSTEM_DESC, &gpuResourceFB_DEPTH_SYSTEM_ARCH))
// 		g_pCGpuManager->GenerateGpuResource(&gpuResourceFB_DEPTH_SYSTEM_DESC, pCIObjectMerger, &gpuResourceFB_DEPTH_SYSTEM_ARCH, NULL);
// #pragma endregion
// 
// #pragma region Other Presetting For Shaders
// 	uint uiClearVlaues[4] = { 0 };
// 	float fClearColor[4];
// 	fClearColor[0] = 0; fClearColor[1] = 0; fClearColor[2] = 0; fClearColor[3] = 0;
// 	if (bInitGen)
// 	{
// 		// Clear Merge Buffer //
// 		for (int i = 0; i < NUM_MERGE_LAYERS * 2; i++)
// 		{
// 			pdx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gpuResourceMERGE_PS_ARCHs[__EXFB_RTV_MERGE_RAYSEGMENT].vtrPtrs.at(i), fClearColor);
// 		}
// 	}
// 
// 	ID3D11RenderTargetView* pdx11RTVs_NULL[2] = { NULL };
// 	ID3D11RenderTargetView* pdx11RTVs[2] = { (ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_RENDEROUT].vtrPtrs.at(0)
// 		, (ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_DEPTH_ZTHICKCS].vtrPtrs.at(0) };
// 	// Clear //
// 	for (int i = 0; i < NUM_TEXRT_LAYERS; i++)
// 	{
// 		pdx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_RENDEROUT].vtrPtrs.at(i), fClearColor);
// 		pdx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_DEPTH_ZTHICKCS].vtrPtrs.at(i), fClearColor);
// 	}
// 	pdx11DeviceImmContext->ClearDepthStencilView((ID3D11DepthStencilView*)gpuResourcesFB_ARCHs[__FB_DSV_DEPTH].vtrPtrs.at(0), D3D11_CLEAR_DEPTH, 1.0f, 0);
// 	pdx11DeviceImmContext->OMSetDepthStencilState(g_VmCommonParams.pdx11DSStates[__DSState_DEPTHENABLE_LESSEQUAL], 0);
// 
// 	pdx11DeviceImmContext->VSSetSamplers(0, 1, &g_VmCommonParams.pdx11SStates[__SState_LINEAR_ZEROBORDER]);
// 	pdx11DeviceImmContext->VSSetSamplers(1, 1, &g_VmCommonParams.pdx11SStates[__SState_POINT_ZEROBORDER]);
// 	pdx11DeviceImmContext->PSSetSamplers(0, 1, &g_VmCommonParams.pdx11SStates[__SState_LINEAR_ZEROBORDER]);
// 	pdx11DeviceImmContext->PSSetSamplers(1, 1, &g_VmCommonParams.pdx11SStates[__SState_POINT_ZEROBORDER]);
// 
// 	// Proxy Setting //
// 	D3DXMATRIX dxmatWS2CS, dxmatWS2PS;
// 	D3DXMatrixLookAtRH(&dxmatWS2CS, &D3DXVECTOR3(0, 0, 1.f), &D3DXVECTOR3(0, 0, 0), &D3DXVECTOR3(0, 1.f, 0));
// 	D3DXMatrixOrthoRH(&dxmatWS2PS, 1.f, 1.f, 0.f, 2.f);
// 	D3DXMATRIX dxmatOS2PS = dxmatWS2CS * dxmatWS2PS;
// 	/////D3DXMatrixTranspose(&dxmatOS2PS, &dxmatOS2PS);
// 	CB_SrProxy cbProjCamState;
// 	cbProjCamState.matOS2PS = dxmatOS2PS;
// 	D3D11_MAPPED_SUBRESOURCE mappedProxyState;
// 	pdx11DeviceImmContext->Map(g_VmCommonParams.pdx11BufConstParameters[__CB_SR_PROXY], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedProxyState);
// 	CB_SrProxy* pCBProxyParam = (CB_SrProxy*)mappedProxyState.pData;
// 	memcpy(pCBProxyParam, &cbProjCamState, sizeof(CB_SrProxy));
// 	pdx11DeviceImmContext->Unmap(g_VmCommonParams.pdx11BufConstParameters[__CB_SR_PROXY], 0);
// 	pdx11DeviceImmContext->VSSetConstantBuffers(10, 1, &g_VmCommonParams.pdx11BufConstParameters[__CB_SR_PROXY]);
// 
// 	CB_SrProjectionCameraState cbSrCamState;
// 	double dVThickness = 1.0;
// 	pCIObjectMerger->GetCustomParameter(("_double_VZThickness"), &dVThickness);
// 	cbSrCamState.fVThicknessGlobal = (float)dVThickness;	// Only for this //
// 	cbSrCamState.iFlag = iLOD << 16;
// 
// 	// CB Set MERGE //
// 	D3D11_MAPPED_SUBRESOURCE mappedSrCamState;
// 	pdx11DeviceImmContext->Map(g_VmCommonParams.pdx11BufConstParameters[__CB_SR_CAMSTATE], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSrCamState);
// 	CB_SrProjectionCameraState* pCBCamStateParam = (CB_SrProjectionCameraState*)mappedSrCamState.pData;
// 	memcpy(pCBCamStateParam, &cbSrCamState, sizeof(CB_SrProjectionCameraState));
// 	pdx11DeviceImmContext->Unmap(g_VmCommonParams.pdx11BufConstParameters[__CB_SR_CAMSTATE], 0);
// 	pdx11DeviceImmContext->VSSetConstantBuffers(1, 1, &g_VmCommonParams.pdx11BufConstParameters[__CB_SR_CAMSTATE]);
// 	pdx11DeviceImmContext->PSSetConstantBuffers(1, 1, &g_VmCommonParams.pdx11BufConstParameters[__CB_SR_CAMSTATE]);
// 
// 	// View Port Setting //
// 	D3D11_VIEWPORT dx11ViewPort;
// 	dx11ViewPort.Width = (float)i2SizeScreenCurrent.x;
// 	dx11ViewPort.Height = (float)i2SizeScreenCurrent.y;
// 	dx11ViewPort.MinDepth = 0;
// 	dx11ViewPort.MaxDepth = 1.0f;
// 	dx11ViewPort.TopLeftX = 0;
// 	dx11ViewPort.TopLeftY = 0;
// 	pdx11DeviceImmContext->RSSetViewports(1, &dx11ViewPort);
// 
// 	// View List-Up //
// 	ID3D11ShaderResourceView* pdx11SRV_RenderOuts_NULL[NUM_TEXRT_LAYERS] = { NULL };
// 	ID3D11ShaderResourceView* pdx11SRV_DepthCSs_NULL[NUM_TEXRT_LAYERS] = { NULL };
// 	ID3D11ShaderResourceView* pdx11SRV_RenderOuts[NUM_TEXRT_LAYERS] = { NULL };
// 	ID3D11ShaderResourceView* pdx11SRV_DepthCSs[NUM_TEXRT_LAYERS] = { NULL };
// 	for (int i = 0; i < NUM_TEXRT_LAYERS; i++)
// 	{
// 		pdx11SRV_RenderOuts[i] = (ID3D11ShaderResourceView*)gpuResourcesFB_ARCHs[__FB_SRV_RENDEROUT].vtrPtrs.at(i);
// 		pdx11SRV_DepthCSs[i] = (ID3D11ShaderResourceView*)gpuResourcesFB_ARCHs[__FB_SRV_DEPTH_ZTHICKCS].vtrPtrs.at(i);
// 	}
// 
// 	ID3D11ShaderResourceView* pdx11SRV_RSA_NULL[NUM_MERGE_LAYERS] = { NULL };
// 	ID3D11RenderTargetView* pdx11RTV_RSA_NULL[NUM_MERGE_LAYERS] = { NULL };
// 	ID3D11ShaderResourceView* pdx11SRV_RSA[NUM_MERGE_LAYERS] = { NULL };
// 	ID3D11RenderTargetView* pdx11RTV_RSA[NUM_MERGE_LAYERS] = { NULL };
// 
// 	ID3D11ShaderResourceView* pdx11SRV_NULL = NULL;
// 	ID3D11UnorderedAccessView* pdx11UAV_NULL = NULL;
// #pragma endregion
// 
// 	uint uiStrideSizeInput = 0;
// 	uint uiOffset = 0;
// 	
// #pragma region Copy to RenderTextures
// 	FrameBuffer* psvxFrameBuffer_RGBA = (FrameBuffer*)pCIObjectMerger->GetFrameBuffer(FrameBufferUsageCUSTOM, 1);
// 	FrameBuffer* psvxFrameBuffer_DepthCSB = (FrameBuffer*)pCIObjectMerger->GetFrameBuffer(FrameBufferUsageCUSTOM, 2);
// 	FrameBuffer* psvxFrameBuffer_Thickness = (FrameBuffer*)pCIObjectMerger->GetFrameBuffer(FrameBufferUsageCUSTOM, 3);
// 
// 	vmbyte4* py4BufferOut_RGBA = (vmbyte4*)psvxFrameBuffer_RGBA->fbuffer;
// 	float* pfBufferOut_DepthCSB = (float*)psvxFrameBuffer_DepthCSB->fbuffer;
// 	float* pfBufferOut_Thickness = (float*)psvxFrameBuffer_Thickness->fbuffer;
// 
// 	vmbyte4* py4Color = NULL;
// 	float* pfDepth = NULL;
// 	D3D11_MAPPED_SUBRESOURCE mappedResSysInit;
// 
// 	// Copy Color 0
// 	pdx11DeviceImmContext->Map((ID3D11Texture2D*)gpuResourceFB_RENDEROUT_SYSTEM_ARCH.vtrPtrs.at(0), 0, D3D11_MAP_WRITE, NULL, &mappedResSysInit);
// 	py4Color = (vmbyte4*)mappedResSysInit.pData;
// 	for (int i = 0; i < i2SizeScreenCurrent.y; i++)
// 	{
// 		memcpy(&py4Color[i * mappedResSysInit.RowPitch / 4], &py4BufferOut_RGBA[i * i2SizeScreenCurrent.x], sizeof(vmbyte4)* i2SizeScreenCurrent.x);
// 	}
// 	pdx11DeviceImmContext->Unmap((ID3D11Texture2D*)gpuResourceFB_RENDEROUT_SYSTEM_ARCH.vtrPtrs.at(0), 0);
// 	pdx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gpuResourcesFB_ARCHs[__FB_TX2D_RENDEROUT].vtrPtrs.at(0), (ID3D11Texture2D*)gpuResourceFB_RENDEROUT_SYSTEM_ARCH.vtrPtrs.at(0));
// 
// 	// Copy Depth 0
// 	pdx11DeviceImmContext->Map((ID3D11Texture2D*)gpuResourceFB_DEPTH_SYSTEM_ARCH.vtrPtrs.at(0), 0, D3D11_MAP_WRITE, NULL, &mappedResSysInit);
// 	pfDepth = (float*)mappedResSysInit.pData;
// 	for (int i = 0; i < i2SizeScreenCurrent.y; i++)
// 	{
// 		memcpy(&pfDepth[i * mappedResSysInit.RowPitch / 4], &pfBufferOut_DepthCSB[i * i2SizeScreenCurrent.x], sizeof(float)* i2SizeScreenCurrent.x);
// 	}
// 	pdx11DeviceImmContext->Unmap((ID3D11Texture2D*)gpuResourceFB_DEPTH_SYSTEM_ARCH.vtrPtrs.at(0), 0);
// 	pdx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gpuResourcesFB_ARCHs[__FB_TX2D_DEPTH_ZTHICKCS].vtrPtrs.at(0), (ID3D11Texture2D*)gpuResourceFB_DEPTH_SYSTEM_ARCH.vtrPtrs.at(0));
// 
// 	// Copy Thickness 0
// 	pdx11DeviceImmContext->Map((ID3D11Texture2D*)gpuResourceFB_DEPTH_SYSTEM_ARCH.vtrPtrs.at(0), 0, D3D11_MAP_WRITE, NULL, &mappedResSysInit);
// 	pfDepth = (float*)mappedResSysInit.pData;
// 	for (int i = 0; i < i2SizeScreenCurrent.y; i++)
// 	{
// 		memcpy(&pfDepth[i * mappedResSysInit.RowPitch / 4], &pfBufferOut_Thickness[i * i2SizeScreenCurrent.x], sizeof(float)* i2SizeScreenCurrent.x);
// 	}
// 	pdx11DeviceImmContext->Unmap((ID3D11Texture2D*)gpuResourceFB_DEPTH_SYSTEM_ARCH.vtrPtrs.at(0), 0);
// 	pdx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gpuResourcesFB_ARCHs[__FB_TX2D_DEPTH_ZTHICKCS].vtrPtrs.at(2), (ID3D11Texture2D*)gpuResourceFB_DEPTH_SYSTEM_ARCH.vtrPtrs.at(0));
// #pragma endregion
// 
// 	// Shader 이름을 다르게 해서 진행해 보자 //!!
// 
// #pragma region Mix Merge Out
// 	pdx11DeviceImmContext->OMSetRenderTargets(2, pdx11RTVs_NULL, (ID3D11DepthStencilView*)gpuResourcesFB_ARCHs[__FB_DSV_DEPTH].vtrPtrs.at(0));
// 	pdx11DeviceImmContext->PSSetShaderResources(30, NUM_TEXRT_LAYERS, pdx11SRV_RenderOuts);
// 	pdx11DeviceImmContext->PSSetShaderResources(60, NUM_TEXRT_LAYERS, pdx11SRV_DepthCSs);
// 	pdx11DeviceImmContext->PSSetShaderResources(80, NUM_TEXRT_LAYERS / 2, &pdx11SRV_DepthCSs[2]);	// 재활용
// 
// 	vector<void*>* pvtrRTV = &gpuResourceMERGE_PS_ARCHs[__EXFB_RTV_MERGE_RAYSEGMENT].vtrPtrs;
// 	vector<void*>* pvtrSRV = &gpuResourceMERGE_PS_ARCHs[__EXFB_SRV_MERGE_RAYSEGMENT].vtrPtrs;
// 	vector<ID3D11RenderTargetView*> vtrRTV_MergePingpongPS;
// 	vector<ID3D11ShaderResourceView*> vtrSRV_MergePingpongPS;
// 	for (int i = 0; i < NUM_MERGE_LAYERS * 2; i++)
// 	{
// 		vtrRTV_MergePingpongPS.push_back((ID3D11RenderTargetView*)pvtrRTV->at(i));
// 		vtrSRV_MergePingpongPS.push_back((ID3D11ShaderResourceView*)pvtrSRV->at(i));
// 	}
// 	int iCountMerging = 0;
// 	pCIObjectMerger->GetCustomParameter(("_int_CountMerging"), &iCountMerging);
// 
// 
// 	int iRSA_Index_Offset_Prev = 0;
// 	int iRSA_Index_Offset_Out = NUM_MERGE_LAYERS;
// 	if (iCountMerging % 2 == 0)
// 	{
// 		iRSA_Index_Offset_Prev = NUM_MERGE_LAYERS;
// 		iRSA_Index_Offset_Out = 0;
// 	}
// 	for (int iIndexRS = 0; iIndexRS < NUM_MERGE_LAYERS; iIndexRS++)
// 	{
// 		pdx11SRV_RSA[iIndexRS] = vtrSRV_MergePingpongPS.at(iIndexRS + iRSA_Index_Offset_Prev);	// may from SR
// 		pdx11RTV_RSA[iIndexRS] = vtrRTV_MergePingpongPS.at(iIndexRS + iRSA_Index_Offset_Out);
// 	}
// 	
// 	pdx11DeviceImmContext->PSSetShaderResources(90, NUM_MERGE_LAYERS, pdx11SRV_RSA);
// 	pdx11DeviceImmContext->OMSetRenderTargets(min(NUM_MERGE_LAYERS, 8), pdx11RTV_RSA, NULL);
// 
// 	uiStrideSizeInput = sizeof(vmfloat3);
// 	uiOffset = 0;
// 	pdx11DeviceImmContext->IASetVertexBuffers(0, 1, (ID3D11Buffer**)&g_VmCommonParams.pdx11BufProxyVertice, &uiStrideSizeInput, &uiOffset);
// 	pdx11DeviceImmContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
// 	pdx11DeviceImmContext->IASetInputLayout(g_VmCommonParams.pdx11IinputLayouts[0]);
// 	pdx11DeviceImmContext->RSSetState(g_VmCommonParams.pdx11RStates[__RState_SOLID_NONE]);
// 	pdx11DeviceImmContext->VSSetShader(g_VmCommonParams.pdx11VS_SR_Shaders[__VS_PROXY], NULL, 0);
// //	if (iLOD == 0)
// //		pdx11DeviceImmContext->PSSetShader(g_VmCommonParams.pdx11PS_SR_Shaders[__PS_MERGE_RSOUT_LOD2X_TO_LAYERS], NULL, 0);
// //	else
// 		pdx11DeviceImmContext->PSSetShader(g_VmCommonParams.pdx11PS_SR_Shaders[__PS_MERGE_RSOUT_TO_LAYERS], NULL, 0);
// 	pdx11DeviceImmContext->OMSetDepthStencilState(g_VmCommonParams.pdx11DSStates[__DSState_DEPTHENABLE_ALWAYS], 0);
// 
// 	pdx11DeviceImmContext->Draw(4, 0);
// 
// 	pdx11DeviceImmContext->PSSetShaderResources(30, NUM_TEXRT_LAYERS, pdx11SRV_RenderOuts_NULL);
// 	pdx11DeviceImmContext->PSSetShaderResources(60, NUM_TEXRT_LAYERS, pdx11SRV_DepthCSs_NULL);
// 	pdx11DeviceImmContext->PSSetShaderResources(90, NUM_MERGE_LAYERS, pdx11SRV_RSA_NULL);
// 	pdx11DeviceImmContext->OMSetRenderTargets(min(NUM_MERGE_LAYERS, 8), pdx11RTV_RSA_NULL, NULL);
// #pragma endregion
// 
// #pragma region // System Out
// 	// PS into PS
// 	for (int iIndexRS = 0; iIndexRS < NUM_MERGE_LAYERS; iIndexRS++)
// 		pdx11SRV_RSA[iIndexRS] = vtrSRV_MergePingpongPS.at(iIndexRS + iRSA_Index_Offset_Out);
// 
// 	pdx11DeviceImmContext->PSSetShaderResources(90, NUM_MERGE_LAYERS, pdx11SRV_RSA);
// 
// 	pdx11RTVs[0] = (ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_RENDEROUT].vtrPtrs.at(0);
// 	pdx11RTVs[1] = (ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_DEPTH_ZTHICKCS].vtrPtrs.at(0);
// 	pdx11DeviceImmContext->OMSetRenderTargets(2, pdx11RTVs, NULL);
// 
// 	//__PS_MERGE_LAYERS_TO_RENDEROUT
// 	uiStrideSizeInput = sizeof(vmfloat3);
// 	uiOffset = 0;
// 	pdx11DeviceImmContext->IASetVertexBuffers(0, 1, (ID3D11Buffer**)&g_VmCommonParams.pdx11BufProxyVertice, &uiStrideSizeInput, &uiOffset);
// 	pdx11DeviceImmContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
// 	pdx11DeviceImmContext->IASetInputLayout(g_VmCommonParams.pdx11IinputLayouts[0]);
// 	pdx11DeviceImmContext->RSSetState(g_VmCommonParams.pdx11RStates[__RState_SOLID_NONE]);
// 	pdx11DeviceImmContext->OMSetDepthStencilState(g_VmCommonParams.pdx11DSStates[__DSState_DEPTHENABLE_ALWAYS], 0);
// 	pdx11DeviceImmContext->VSSetShader(g_VmCommonParams.pdx11VS_SR_Shaders[__VS_PROXY], NULL, 0);
// 	pdx11DeviceImmContext->PSSetShader(g_VmCommonParams.pdx11PS_SR_Shaders[__PS_MERGE_LAYERS_TO_RENDEROUT], NULL, 0);
// 
// 	pdx11DeviceImmContext->Draw(4, 0);
// 
// #pragma region // Copy GPU to CPU
// 	FrameBuffer* psvxFrameBuffer = (FrameBuffer*)pCIObjectSystemOut->GetFrameBuffer(FrameBufferUsageRENDEROUT, 0);
// 	vmbyte4* py4Buffer = (vmbyte4*)psvxFrameBuffer->fbuffer;
// 
// 	D3D11_MAPPED_SUBRESOURCE mappedResSysFinal;
// 	pdx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gpuResourceFB_RENDEROUT_SYSTEM_ARCH.vtrPtrs.at(0), (ID3D11Texture2D*)gpuResourcesFB_ARCHs[__FB_TX2D_RENDEROUT].vtrPtrs.at(0));
// 	pdx11DeviceImmContext->Map((ID3D11Texture2D*)gpuResourceFB_RENDEROUT_SYSTEM_ARCH.vtrPtrs.at(0), 0, D3D11_MAP_READ, NULL, &mappedResSysFinal);
// 
// 	vmbyte4* py4RenderOuts = (vmbyte4*)mappedResSysFinal.pData;
// 	for (int i = 0; i < i2SizeScreenCurrent.y; i++)
// 	{
// 		for (int j = 0; j < i2SizeScreenCurrent.x; j++)
// 		{
// 			// __PS_MERGE_LAYERS_TO_RENDEROUT 에서 INT -> FLOAT4 로 되어 배열된 color 요소가 들어 옴. //
// 			py4Buffer[i*i2SizeScreenCurrent.x + j].x = (byte)(py4RenderOuts[j].z);	// B
// 			py4Buffer[i*i2SizeScreenCurrent.x + j].y = (byte)(py4RenderOuts[j].y);	// G
// 			py4Buffer[i*i2SizeScreenCurrent.x + j].z = (byte)(py4RenderOuts[j].x);	// R
// 			py4Buffer[i*i2SizeScreenCurrent.x + j].w = (byte)(py4RenderOuts[j].w);	// A
// 		}
// 		py4RenderOuts += mappedResSysFinal.RowPitch / 4;	// mappedResSysFinal.RowPitch (byte unit)
// 	}
// 	pdx11DeviceImmContext->Unmap((ID3D11Texture2D*)gpuResourceFB_RENDEROUT_SYSTEM_ARCH.vtrPtrs.at(0), 0);
// 
// 	FrameBuffer* psvxFrameBufferDepth = (FrameBuffer*)pCIObjectSystemOut->GetFrameBuffer(FrameBufferUsageDEPTH, 0);
// 	float* pfBuffer = (float*)psvxFrameBufferDepth->fbuffer;
// 	pdx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gpuResourceFB_DEPTH_SYSTEM_ARCH.vtrPtrs.at(0), (ID3D11Texture2D*)gpuResourcesFB_ARCHs[__FB_TX2D_DEPTH_ZTHICKCS].vtrPtrs.at(0));
// 	pdx11DeviceImmContext->Map((ID3D11Texture2D*)gpuResourceFB_DEPTH_SYSTEM_ARCH.vtrPtrs.at(0), 0, D3D11_MAP_READ, NULL, &mappedResSysFinal);
// 	float* pfDeferredContexts = (float*)mappedResSysFinal.pData;
// 	for (int i = 0; i < i2SizeScreenCurrent.y; i++)
// 	{
// 		for (int j = 0; j < i2SizeScreenCurrent.x; j++)
// 		{
// 			if (py4Buffer[i*i2SizeScreenCurrent.x + j].w > 0)
// 				pfBuffer[i*i2SizeScreenCurrent.x + j] = pfDeferredContexts[j];
// 			else
// 				pfBuffer[i*i2SizeScreenCurrent.x + j] = FLT_MAX;
// 		}
// 		pfDeferredContexts += mappedResSysFinal.RowPitch / 4;	// mappedResSysFinal.RowPitch (byte unit)
// 	}
// 	pdx11DeviceImmContext->Unmap((ID3D11Texture2D*)gpuResourceFB_DEPTH_SYSTEM_ARCH.vtrPtrs.at(0), 0);
// #pragma endregion
// #pragma endregion
// 
// 	pdx11DeviceImmContext->ClearState();
// 
// 	ID3D11RenderTargetView* pdxRTVOlds[NUM_MERGE_LAYERS] = { NULL };
// 	pdxRTVOlds[0] = pdxRTVOld;
// 	pdx11DeviceImmContext->OMSetRenderTargets(NUM_MERGE_LAYERS, pdxRTVOlds, pdxDSVOld);
// 
// 	VMSAFE_RELEASE(pdxRTVOld);
// 	VMSAFE_RELEASE(pdxDSVOld);
// 	g_mutex_GpuCriticalPath.unlock();
// 	return true;
// }

bool HDx11UploadDefaultVolume(GpuResource* pstGpuResourceVolumeTexture, 
	GpuResource* pstGpuResourceVolumeSRV,
	VmVObjectVolume* pCVolume, string usage_specifier, LocalProgress* _progress)
{
	if (g_pCGpuManager == NULL)
		return false;
	VolumeData* pVolumeData = pCVolume->GetVolumeData();
	GpuResDescriptor stGpuVolumeResourceDesc;
	stGpuVolumeResourceDesc.sdk_type = GpuSdkTypeDX11;
	stGpuVolumeResourceDesc.dtype = pVolumeData->store_dtype; // vxrDataTypeUNSIGNEDSHORT
	stGpuVolumeResourceDesc.src_obj_id = pCVolume->GetObjectID();
	stGpuVolumeResourceDesc.usage_specifier = usage_specifier;
	stGpuVolumeResourceDesc.res_type = ResourceTypeDX11RESOURCE;
	if (!g_pCGpuManager->GetGpuResourceArchive(&stGpuVolumeResourceDesc, pstGpuResourceVolumeTexture))
	{
		g_pCGpuManager->GenerateGpuResource(&stGpuVolumeResourceDesc, pCVolume, pstGpuResourceVolumeTexture, _progress);
	}
	stGpuVolumeResourceDesc.res_type = ResourceTypeDX11SRV;
	if (!g_pCGpuManager->GetGpuResourceArchive(&stGpuVolumeResourceDesc, pstGpuResourceVolumeSRV))
	{
		g_pCGpuManager->GenerateGpuResource(&stGpuVolumeResourceDesc, pCVolume, pstGpuResourceVolumeSRV, NULL);
	}
	return true;
}

bool HDx11UploadMesh(GpuResource* pstGpuResourceMeshBufferVtx,
	GpuResource* pstGpuResourceMeshBufferIdx,
	VmVObjectPrimitive* pMesh, LocalProgress* _progress)
{
	if (g_pCGpuManager == NULL)
		return false;
	GpuResDescriptor stGpuMeshDesc;
	stGpuMeshDesc.sdk_type = GpuSdkTypeDX11;
	stGpuMeshDesc.dtype = data_type::dtype<vmfloat3>();
	stGpuMeshDesc.src_obj_id = pMesh->GetObjectID();
	stGpuMeshDesc.usage_specifier = ("BUFFER_PRIMITIVE_VERTEX_LIST");
	stGpuMeshDesc.res_type = ResourceTypeDX11RESOURCE;
	if (!g_pCGpuManager->GetGpuResourceArchive(&stGpuMeshDesc, pstGpuResourceMeshBufferVtx))
	{
		g_pCGpuManager->GenerateGpuResource(&stGpuMeshDesc, pMesh, pstGpuResourceMeshBufferVtx, _progress);
	}
	if (pMesh->GetPrimitiveData()->vidx_buffer)
	{
		stGpuMeshDesc.usage_specifier = ("BUFFER_PRIMITIVE_INDEX_LIST");
		if (!g_pCGpuManager->GetGpuResourceArchive(&stGpuMeshDesc, pstGpuResourceMeshBufferIdx))
		{
			g_pCGpuManager->GenerateGpuResource(&stGpuMeshDesc, pMesh, pstGpuResourceMeshBufferIdx, _progress);
		}
	}
	return true;
}