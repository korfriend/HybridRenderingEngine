#include "VXDX11Helper.h"


bool HDx11AllocAndUpdateBlockMap(float** ppfBlocksMap, ushort** ppusBlocksMap, const SVXVolumeBlock* psvxVolumeResBlock, DXGI_FORMAT eDgiFormat, SVXLocalProgress* psvxLocalProgress)
{
	bool* pbTaggedActivatedBlocks = psvxVolumeResBlock->pbTaggedActivatedBlocks;
	uint uiNumBlocks = (uint)psvxVolumeResBlock->i3BlocksNumber.x*(uint)psvxVolumeResBlock->i3BlocksNumber.y*(uint)psvxVolumeResBlock->i3BlocksNumber.z;
	uint uiCount = 0;
	vxint3 i3BlockExBoundaryNum = psvxVolumeResBlock->i3BlockExBoundaryNum;
	vxint3 i3BlockNumSampleSize = psvxVolumeResBlock->i3BlocksNumber + i3BlockExBoundaryNum * 2;
	int iBlockNumSampleSizeXY = i3BlockNumSampleSize.x * i3BlockNumSampleSize.y;
	int iBlockNumXY = psvxVolumeResBlock->i3BlocksNumber.x * psvxVolumeResBlock->i3BlocksNumber.y;

	if (eDgiFormat == DXGI_FORMAT_R32_FLOAT)
	{
		*ppfBlocksMap = new float[uiNumBlocks];	// NEW
		float* pfBlocksMap = *ppfBlocksMap;
		memset(pfBlocksMap, 0, sizeof(float)*uiNumBlocks);

		int iCountID = 0;
		for (int iZ = 0; iZ < psvxVolumeResBlock->i3BlocksNumber.z; iZ++)
		{
			if (psvxLocalProgress)
				*psvxLocalProgress->pdProgressOfCurWork = (psvxLocalProgress->dStartProgress + psvxLocalProgress->dRangeProgress*iZ / psvxVolumeResBlock->i3BlocksNumber.z);
			for (int iY = 0; iY < psvxVolumeResBlock->i3BlocksNumber.y; iY++)
			for (int iX = 0; iX < psvxVolumeResBlock->i3BlocksNumber.x; iX++)
			{
				int iAddrBlockCpu = (iX + i3BlockExBoundaryNum.x)
					+ (iY + i3BlockExBoundaryNum.y) * i3BlockNumSampleSize.x + (iZ + i3BlockExBoundaryNum.z) * iBlockNumSampleSizeXY;
				if (pbTaggedActivatedBlocks[iAddrBlockCpu])
				{
					uiCount++;
					if (uiCount >= 8300000)
					{
						VXERRORMESSAGE("Bricks Overflow!");
						VXSAFE_DELETEARRAY(pfBlocksMap);
						return false;
					}
					pfBlocksMap[iCountID] = (float)uiCount;
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
		for (int iZ = 0; iZ < psvxVolumeResBlock->i3BlocksNumber.z; iZ++)
		{
			if (psvxLocalProgress)
				*psvxLocalProgress->pdProgressOfCurWork = (psvxLocalProgress->dStartProgress + psvxLocalProgress->dRangeProgress*iZ / psvxVolumeResBlock->i3BlocksNumber.z);
			for (int iY = 0; iY < psvxVolumeResBlock->i3BlocksNumber.y; iY++)
			for (int iX = 0; iX < psvxVolumeResBlock->i3BlocksNumber.x; iX++)
			{
				int iAddrBlockCpu = (iX + i3BlockExBoundaryNum.x)
					+ (iY + i3BlockExBoundaryNum.y) * i3BlockNumSampleSize.x + (iZ + i3BlockExBoundaryNum.z) * iBlockNumSampleSizeXY;
				if (pbTaggedActivatedBlocks[iAddrBlockCpu])
				{
					uiCount++;
					if (uiCount >= 65535)
					{
						VXERRORMESSAGE("Bricks Overflow!");
						VXSAFE_DELETEARRAY(pusBlocksMap);
					}
					pusBlocksMap[iCountID] = uiCount;
				}
				iCountID++;
			}
		}
	}
	return true;
}

static GpuDX11CommonParameters g_VxgCommonParams;
static CVXGPUManager* g_pCGpuManager = NULL;

void HDx11ReleaseCommonResources()
{
	GpuDX11CommonParameters* pParam = &g_VxgCommonParams;
	// Initialized once //
	for(int i = 0; i < __DSState_COUNT; i++)
		VXSAFE_RELEASE(pParam->pdx11DSStates[i]);

	for(int i = 0; i < __RState_COUNT; i++)
		VXSAFE_RELEASE(pParam->pdx11RStates[i]);

	for(int i = 0; i < __SState_COUNT; i++)
		VXSAFE_RELEASE(pParam->pdx11SStates[i]);

	for(int i = 0; i < __CB_COUNT; i++)
		VXSAFE_RELEASE(pParam->pdx11BufConstParameters[i]);

	// Deallocate All Shared Resources //
	for (int i = 0; i < NUMSHADERS_VR; i++)
		VXSAFE_RELEASE(pParam->pdx11CS_VR_Shaders[i]);
	for (int i = 0; i < NUMSHADERS_PLANE_SR_VS; i++)
		VXSAFE_RELEASE(pParam->pdx11VS_PLANE_SR_Shaders[i]);
	for (int i = 0; i < NUMSHADERS_SR_PS; i++)
		VXSAFE_RELEASE(pParam->pdx11PS_SR_Shaders[i]);
	for (int i = 0; i < NUMSHADERS_MERGE; i++)
		VXSAFE_RELEASE(pParam->pdx11CS_MergeTextures[i]);
	for (int i = 0; i < NUMINPUTLAYOUTS; i++)
		VXSAFE_RELEASE(pParam->pdx11IinputLayouts[i]);
	for (int i = 0; i < NUMSHADERS_SR_VS; i++)
		VXSAFE_RELEASE(pParam->pdx11VS_SR_Shaders[i]);

	VXSAFE_RELEASE(pParam->pdx11BufImposeVertice);
	VXSAFE_RELEASE(pParam->pdx11BufImposeIndice);
	VXSAFE_RELEASE(pParam->pdx11BufProxyVertice);

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
		VXSAFE_RELEASE(g_VxgCommonParams.pdx11BufConstParameters[i]);
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
		hr |= pdx11Device->CreateBuffer(&descCB, NULL, &g_VxgCommonParams.pdx11BufConstParameters[i]);
	}

	return hr;
}


LPCWSTR g_lpcwstrResourcesVR[NUMSHADERS_VR] = {	// The order should be same as the order of compile resource ones.
	MAKEINTRESOURCE(IDR_RCDATA0010), // "DVR_VOLUME_MIX_DEFAULT_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0011), // "DVR_VOLUME_MIX_DEFAULT_HOMOGENEOUS_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0012), // "DVR_VOLUME_MIX_MASK_DEFAULT_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0020), // "DVR_VOLUME_MIX_CLIPSEMIOPAQUE_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0021), // "DVR_VOLUME_MIX_MASK_CLIPSEMIOPAQUE_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0030), // "DVR_VOLUME_MIX_MODULATION_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0031), // "DVR_VOLUME_MIX_MASK_MODULATION_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0040), // "DVR_VOLUME_MIX_SHADOW_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0041), // "DVR_VOLUME_MIX_MASK_SHADOW_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0050), // "DVR_VOLUME_MIX_DEFAULT_SCULPTMASK_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0060), // "DVR_VOLUME_MIX_SIMULTANEOUS_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0061), // "DVR_VOLUME_MIX_SIMULTANEOUS_THREE_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0500), // "DVR_BLOCK_IND_SHOW_cs_4_0"	
	MAKEINTRESOURCE(IDR_RCDATA0510), // "DVR_VOLUME_IND_DEFAULT_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0511), // "DVR_VOLUME_IND_DEFAULT_HOMOGENEOUS_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0512), // "DVR_VOLUME_IND_MASK_DEFAULT_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0520), // "DVR_VOLUME_IND_CLIPSEMIOPAQUE_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0521), // "DVR_VOLUME_IND_MASK_CLIPSEMIOPAQUE_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0530), // "DVR_VOLUME_IND_MODULATION_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0531), // "DVR_VOLUME_IND_MASK_MODULATION_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0550), // "DVR_VOLUME_IND_SHADOW_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0551), // "DVR_VOLUME_IND_MASK_SHADOW_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0560), // "DVR_VOLUME_IND_DEFAULT_SCULPTMASK_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0570), // "DVR_VOLUME_IND_SURFACEEFFECT_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0571), // "DVR_VOLUME_IND_MASK_SURFACEEFFECT_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0572), // "DVR_VOLUME_IND_SURFACECURVATURE_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0573), // "DVR_VOLUME_IND_MASK_SURFACECURVATURE_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0574), // "DVR_VOLUME_IND_SURFACEEFFECT_MARKERSONSURFACE_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0575), // "DVR_VOLUME_IND_MASK_SURFACEEFFECT_MARKERSONSURFACE_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0580), // "DVR_VOLUME_IND_SURFACEEFFECT_SHADOW_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0581), // "DVR_VOLUME_IND_MASK_SURFACEEFFECT_SHADOW_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0582), // "DVR_VOLUME_IND_SURFACEEFFECT_SHADOW_MARKERSONSURFACE_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0583), // "DVR_VOLUME_IND_MASK_SURFACEEFFECT_MARKERSONSURFACE_SHADOW_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0590), // "DVR_VOLUME_IND_CCF_SURFACEEFFECT_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0600), // "DVR_VOLUME_IND_RAYMAX_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0601), // "DVR_VOLUME_IND_MASK_RAYMAX_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0610), // "DVR_VOLUME_IND_RAYMIN_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0611), // "DVR_VOLUME_IND_MASK_RAYMIN_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0620), // "DVR_VOLUME_IND_RAYSUM_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA0621), // "DVR_VOLUME_IND_MASK_RAYSUM_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA2010), // "DVR_BRICK_MIX_DEFAULT_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA2020), // "DVR_BRICK_MIX_SHADOW_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA2510), // "DVR_BRICK_IND_DEFAULT_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA2520), // "DVR_BRICK_IND_SHADOW_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA2530), // "DVR_BRICK_IND_SURFACEEFFECT_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA2540), // "DVR_BRICK_IND_SURFACEEFFECT_SHADOW_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA2550), // "DVR_BRICK_IND_CCF_SURFACEEFFECT_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA8000), // "DVR_VOLUME_GEN_SHADOW_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA8001), // "DVR_VOLUME_GEN_MASK_SHADOW_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA8010), // "DVR_VOLUME_GEN_SURFACE_SHADOW_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA8011), // "DVR_VOLUME_GEN_MASK_SURFACE_SHADOW_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA8020), // "DVR_BRICK_GEN_SHADOW_cs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA8030), // "DVR_BRICK_GEN_SURFACE_SHADOW_cs_4_0"
};

LPCWSTR g_lpcwstrResourcesMERGE[NUMSHADERS_MERGE] = {
	MAKEINTRESOURCE(IDR_RCDATA20000),
	MAKEINTRESOURCE(IDR_RCDATA20020),
};

LPCWSTR g_lpcwstrResourcesSR[NUMSHADERS_SR_PS] = {
	MAKEINTRESOURCE(IDR_RCDATA10010),
	MAKEINTRESOURCE(IDR_RCDATA10020),
	MAKEINTRESOURCE(IDR_RCDATA10030),
	MAKEINTRESOURCE(IDR_RCDATA10031),
	MAKEINTRESOURCE(IDR_RCDATA10035),
	MAKEINTRESOURCE(IDR_RCDATA10036),
	MAKEINTRESOURCE(IDR_RCDATA10040),
	MAKEINTRESOURCE(IDR_RCDATA10050),
	MAKEINTRESOURCE(IDR_RCDATA10060),
	MAKEINTRESOURCE(IDR_RCDATA10065),
	MAKEINTRESOURCE(IDR_RCDATA10070),
	MAKEINTRESOURCE(IDR_RCDATA10080),
	MAKEINTRESOURCE(IDR_RCDATA10090),
	MAKEINTRESOURCE(IDR_RCDATA11000),
	MAKEINTRESOURCE(IDR_RCDATA11010),
	MAKEINTRESOURCE(IDR_RCDATA11020),
};

LPCWSTR g_lpcwstrResourcesPLANESR[NUMSHADERS_PLANE_SR_VS] = {
	MAKEINTRESOURCE(IDR_RCDATA15010),
	MAKEINTRESOURCE(IDR_RCDATA15020),
	MAKEINTRESOURCE(IDR_RCDATA15030),
	MAKEINTRESOURCE(IDR_RCDATA15040),
	MAKEINTRESOURCE(IDR_RCDATA15050),
};
int HDx11InitializeGpuStatesAndBasicResources(CVXGPUManager* pCGpuManager, GpuDX11CommonParameters** ppParam /*out*/)
{
#define __SUPPORTED_GPU 0
#define __ALREADY_CHECKED 1
#define __LOW_SPEC_NOT_SUPPORT_CS_GPU 2
#define __INVALID_GPU -1
	if (pCGpuManager == NULL)
		return __INVALID_GPU;

	g_pCGpuManager = pCGpuManager;
	GpuDX11CommonParameters* pParam = &g_VxgCommonParams;
	if (ppParam != NULL)
		*ppParam = pParam;

	if (pParam->bIsInitialized)
		return __ALREADY_CHECKED;

	g_pCGpuManager->GetDeviceInformation((void*)(&pParam->pdx11Device), _T("DEVICE_POINTER"));
	g_pCGpuManager->GetDeviceInformation((void*)(&pParam->pdx11DeviceImmContext), _T("DEVICE_CONTEXT_POINTER"));
	g_pCGpuManager->GetDeviceInformation(&pParam->stDx11Adapter, _T("DEVICE_ADAPTER_DESC"));
	g_pCGpuManager->GetDeviceInformation(&pParam->eDx11FeatureLevel, _T("FEATURE_LEVEL"));

	if (pParam->pdx11Device == NULL || pParam->pdx11DeviceImmContext == NULL)
		return __INVALID_GPU;

	for (int i = 0; i < __DSState_COUNT; i++)
		VXSAFE_RELEASE(pParam->pdx11DSStates[i]);

	for (int i = 0; i < __RState_COUNT; i++)
		VXSAFE_RELEASE(pParam->pdx11RStates[i]);

	for (int i = 0; i < __SState_COUNT; i++)
		VXSAFE_RELEASE(pParam->pdx11SStates[i]);

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
		sizeof(CB_VrVolumeObject)* 3, // 16
		sizeof(CB_VrModulation)* 3, // 17
	};
	if (HDx11ConstantBufferSetting(pParam->pdx11Device, uiSizeParams, sizeof(uiSizeParams) / sizeof(uint)) != S_OK)
	{
		VXERRORMESSAGE("Constant Buffer Setting Error!");
		goto ERROR_PRESETTING;
	}

	// Vertex Buffer //
	VXSAFE_RELEASE(pParam->pdx11BufProxyVertice);
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

#pragma region SR
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

	HMODULE hModule = GetModuleHandle(_T("VS-GPUDX11VxRenderer.dll"));

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

	for (int i = 0; i < NUMSHADERS_SR_PS; i++)
	{
		if (HDx11CompiledShaderSetting(pParam->pdx11Device, hModule, g_lpcwstrResourcesSR[i], "ps_4_0", (ID3D11DeviceChild**)&pParam->pdx11PS_SR_Shaders[i]) != S_OK)
		{
			printf("\n*****************\n Invalid DX10 or higher NUMSHADERS_SR_PS\n*****************\n");
			goto ERROR_PRESETTING;
		}
	}
	for (int i = 0; i < NUMSHADERS_PLANE_SR_VS; i++)
	{
		if (HDx11CompiledShaderSetting(pParam->pdx11Device, hModule, g_lpcwstrResourcesPLANESR[i], "vs_4_0", (ID3D11DeviceChild**)&pParam->pdx11VS_PLANE_SR_Shaders[i]) != S_OK)
		{
			printf("\n*****************\n Invalid DX10 or higher NUMSHADERS_PLANE_SR_VS\n*****************\n");
			goto ERROR_PRESETTING;
		}
	}
	// Vertex Buffer //
	VXSAFE_RELEASE(pParam->pdx11BufImposeVertice);
	D3D11_BUFFER_DESC descBufVertex;
	uint uiStructureByteStride = 2 * sizeof(vxfloat3);	// Position & Normal
	ZeroMemory(&descBufVertex, sizeof(D3D11_BUFFER_DESC));
	descBufVertex.ByteWidth = uiStructureByteStride * 6;
	descBufVertex.Usage = D3D11_USAGE_DYNAMIC;
	descBufVertex.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	descBufVertex.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	descBufVertex.MiscFlags = NULL;
	descBufVertex.StructureByteStride = uiStructureByteStride;
	if (pParam->pdx11Device->CreateBuffer(&descBufVertex, NULL, &pParam->pdx11BufImposeVertice) != S_OK)
		return __INVALID_GPU;

	// Index Buffer //
	VXSAFE_RELEASE(pParam->pdx11BufImposeIndice);
	D3D11_BUFFER_DESC descBufIndex;
	ZeroMemory(&descBufIndex, sizeof(D3D11_BUFFER_DESC));
	descBufIndex.Usage = D3D11_USAGE_DYNAMIC;
	descBufIndex.BindFlags = D3D11_BIND_INDEX_BUFFER;
	descBufIndex.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	descBufIndex.MiscFlags = NULL;
	descBufIndex.StructureByteStride = sizeof(uint)* 3;
	descBufIndex.ByteWidth = sizeof(uint)*(3 * 4 + 2 * 6);
	if (pParam->pdx11Device->CreateBuffer(&descBufIndex, NULL, &pParam->pdx11BufImposeIndice) != S_OK)
		return __INVALID_GPU;

	D3D11_MAPPED_SUBRESOURCE mappedRes;
	pParam->pdx11DeviceImmContext->Map(pParam->pdx11BufImposeIndice, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes);
	uint* puiIndexPrimitive = (uint*)mappedRes.pData;
	for (int i = 0; i < 4; i++)
	{
		puiIndexPrimitive[3 * i] = 0;
		puiIndexPrimitive[3 * i + 1] = 1 + i;
		puiIndexPrimitive[3 * i + 2] = 2 + i;
	}
	pParam->pdx11DeviceImmContext->Unmap(pParam->pdx11BufImposeIndice, NULL);
#pragma endregion

	bool bReSafeTry = false;
	bool bSkipCS = false;
	if (pParam->eDx11FeatureLevel == 0x9300 || pParam->eDx11FeatureLevel == 0xA000)
		bSkipCS = true;

ERROR_PRESETTING_CS:
	if (!bSkipCS && !bReSafeTry)
	{
#pragma region // VR
		for (int i = 0; i < NUMSHADERS_VR; i++)
		{
			if (HDx11CompiledShaderSetting(pParam->pdx11Device, hModule, g_lpcwstrResourcesVR[i], "cs_4_0", (ID3D11DeviceChild**)&pParam->pdx11CS_VR_Shaders[i]) != S_OK)
			{
				//VXERRORMESSAGE("Shader Setting Error! - VR");
				printf("\n*****************\n Invalid Compute Shader\n*****************\n");

				bSkipCS = true;
				bReSafeTry = true;
				goto ERROR_PRESETTING_CS;
			}
		}
#pragma endregion 

#pragma region // Merge
		for (int i = 0; i < NUMSHADERS_MERGE; i++)
		{
			if (HDx11CompiledShaderSetting(pParam->pdx11Device, hModule, g_lpcwstrResourcesMERGE[i], "cs_4_0", (ID3D11DeviceChild**)&pParam->pdx11CS_MergeTextures[i]) != S_OK)
			{
				VXERRORMESSAGE("Shader Setting Error! - MERGE");
				//::MessageBox(NULL, _T("MERGE Shader Setting Error!"), NULL, MB_OK);
				goto ERROR_PRESETTING;
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
	else if(_strShaderProfile.compare(0, 2, "ps") == 0)
	{
		if(pdx11Device->CreatePixelShader(pdata, ullFileSize, NULL, (ID3D11PixelShader**)ppdx11Shader) != S_OK)
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

HRESULT HDx11ShaderSetting(ID3D11Device* pdx11Device, wstring* pstrShaderFilePath, HMODULE hModule, LPCWSTR pSrcResource, LPCSTR strShaderProfile, LPCSTR strShaderFuncName, ID3D11DeviceChild** ppdx11Shader/*out*/
	, D3D11_INPUT_ELEMENT_DESC* pInputLayoutDesc, uint uiNumOfInputElements, ID3D11InputLayout** ppdx11LayoutInputVS)
{
	ID3DBlob* pBlobBuf = NULL;
	string _strShaderProfile = strShaderProfile;

	//CShaderHeader includeHlsl;

	if(pstrShaderFilePath)
	{
		if(D3DX11CompileFromFile(pstrShaderFilePath->c_str(), NULL, NULL/*&includeHlsl*/, strShaderFuncName, strShaderProfile, D3D10_SHADER_ENABLE_STRICTNESS|D3D10_SHADER_OPTIMIZATION_LEVEL3
			, 0, NULL, &pBlobBuf, NULL, NULL) != S_OK)
			goto ERROR_SHADER;
	}
	else
	{
		if(D3DX11CompileFromResource(hModule, pSrcResource, NULL, NULL, NULL/*&includeHlsl*/, strShaderFuncName, strShaderProfile, D3D10_SHADER_ENABLE_STRICTNESS|D3D10_SHADER_OPTIMIZATION_LEVEL3
			, 0, NULL, &pBlobBuf, NULL, NULL) != S_OK)
			goto ERROR_SHADER;
	}

	VXSAFE_RELEASE(*ppdx11Shader);
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

	VXSAFE_RELEASE(pBlobBuf);
	return S_OK;

ERROR_SHADER:
	VXSAFE_RELEASE(pBlobBuf);
	return E_FAIL;
}


bool HDx11ModifyBlockMap(CVXVObjectVolume* pCObjectVolume, SVXGPUResourceArchive* psvxGpuResourceBlockTexture, SVXGPUResourceArchive* psvxGpuResourceBlockSRView, bool bIsImmutable)
{
	if (!g_VxgCommonParams.bIsInitialized)
		return false;

	const SVXVolumeDataArchive* psvxVolumeArchive = ((CVXVObjectVolume*)pCObjectVolume)->GetVolumeArchive();
	int iBrickResolution = psvxGpuResourceBlockTexture->svxGpuResourceDesc.iCustomMisc;	// 0 : High Resolution, 1 : Low Resolution
	const SVXVolumeBlock* psvxVolumeResBlock = ((CVXVObjectVolume*)pCObjectVolume)->GetVolumeBlock(iBrickResolution); // 0 : High Resolution, 1 : Low Resolution

	D3D11_TEXTURE3D_DESC descTex3DMap;
	ZeroMemory(&descTex3DMap, sizeof(D3D11_TEXTURE3D_DESC));
	descTex3DMap.Width = psvxVolumeResBlock->i3BlocksNumber.x;
	descTex3DMap.Height = psvxVolumeResBlock->i3BlocksNumber.y;
	descTex3DMap.Depth = psvxVolumeResBlock->i3BlocksNumber.z;
	descTex3DMap.MipLevels = 1;
	descTex3DMap.MiscFlags = NULL;

	switch(psvxGpuResourceBlockTexture->svxGpuResourceDesc.eDataType)
	{
	case vxrDataTypeUNSIGNEDSHORT:
		descTex3DMap.Format = DXGI_FORMAT_R16_UNORM;
		break;
	case vxrDataTypeFLOAT:
		descTex3DMap.Format = DXGI_FORMAT_R32_FLOAT;
		break;
	default:
		return false;
	}
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

	float* pfBlocksMap = NULL;
	ushort* pusBlocksMap = NULL;
	HDx11AllocAndUpdateBlockMap(&pfBlocksMap, &pusBlocksMap, psvxVolumeResBlock, descTex3DMap.Format, NULL);

	// Create Texture //
	ID3D11Texture3D* pdx11TX3DBlkMap = NULL;
	HRESULT hr;
	if(bIsImmutable)
	{
		D3D11_SUBRESOURCE_DATA subDataRes;
		if(descTex3DMap.Format == DXGI_FORMAT_R32_FLOAT)
		{
			subDataRes.pSysMem = pfBlocksMap;
			subDataRes.SysMemPitch = sizeof(float)*descTex3DMap.Width;
			subDataRes.SysMemSlicePitch = sizeof(float)*descTex3DMap.Width*descTex3DMap.Height;
		}
		else
		{
			subDataRes.pSysMem = pusBlocksMap;
			subDataRes.SysMemPitch = sizeof(ushort)*descTex3DMap.Width;
			subDataRes.SysMemSlicePitch = sizeof(ushort)*descTex3DMap.Width*descTex3DMap.Height;
		}
		hr = g_VxgCommonParams.pdx11Device->CreateTexture3D(&descTex3DMap, &subDataRes, &pdx11TX3DBlkMap);
	}
	else
	{
		hr = g_VxgCommonParams.pdx11Device->CreateTexture3D(&descTex3DMap, NULL, &pdx11TX3DBlkMap);
		D3D11_MAPPED_SUBRESOURCE d11MappedRes;
		g_VxgCommonParams.pdx11DeviceImmContext->Map(pdx11TX3DBlkMap, 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes);
		if(descTex3DMap.Format == DXGI_FORMAT_R32_FLOAT)
		{
			float* pfBlkMap = (float*)d11MappedRes.pData;
			for(int i = 0; i < (int)descTex3DMap.Depth; i++)
				for(int j = 0; j < (int)descTex3DMap.Height; j++)
				{
					memcpy(&pfBlkMap[j*d11MappedRes.RowPitch/4 + i*d11MappedRes.DepthPitch/4], &pfBlocksMap[j*descTex3DMap.Width + i*descTex3DMap.Width*descTex3DMap.Height]
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
		g_VxgCommonParams.pdx11DeviceImmContext->Unmap(pdx11TX3DBlkMap, 0);
	}
	if(descTex3DMap.Format == DXGI_FORMAT_R32_FLOAT)
	{
		VXSAFE_DELETEARRAY(pfBlocksMap);
	}
	else
	{
		VXSAFE_DELETEARRAY(pusBlocksMap);
	}
	if(hr == S_OK)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC descSRVVolData;
		descSRVVolData.Format = descTex3DMap.Format;
		descSRVVolData.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
		descSRVVolData.Texture3D.MipLevels = 1;
		descSRVVolData.Texture3D.MostDetailedMip = 0;
		ID3D11ShaderResourceView* pdx11View;
		if (g_VxgCommonParams.pdx11Device->CreateShaderResourceView(pdx11TX3DBlkMap, &descSRVVolData, (ID3D11ShaderResourceView**)&pdx11View) != S_OK)
		{
			VXSAFE_RELEASE(pdx11TX3DBlkMap);
			VXSAFE_RELEASE(pdx11View);
			return false;
		}

		ID3D11Texture3D* pdx11TexBlkMapOld = (ID3D11Texture3D*)psvxGpuResourceBlockTexture->vtrPtrs.at(0);
		VXSAFE_RELEASE(pdx11TexBlkMapOld);
		psvxGpuResourceBlockTexture->vtrPtrs[0] = pdx11TX3DBlkMap;

		ID3D11ShaderResourceView* pdx11SRViewBlkMapOld = (ID3D11ShaderResourceView*)psvxGpuResourceBlockSRView->vtrPtrs.at(0);
		VXSAFE_RELEASE(pdx11SRViewBlkMapOld);
		psvxGpuResourceBlockSRView->vtrPtrs[0] = pdx11View;
	}
	else
	{
		VXSAFE_RELEASE(pdx11TX3DBlkMap);
		return false;
	}

	return true;
}

//#define OLD
bool HDx11ComputeConstantBufferVrObject(CB_VrVolumeObject* pCBVrVolumeObj, bool bIsDownSample, 
	CVXVObjectVolume* pCVObjectVolume, CVXCObject* pCCObject, vxint3 i3SizeVolumeTextureElements, map<wstring, vector<double>>* pmapDValueVolume)
{
	SVXVolumeDataArchive* psvxVolArchive = pCVObjectVolume->GetVolumeArchive();
	pCBVrVolumeObj->iFlags = 0;

	D3DXMATRIX matVS2TS;
#ifdef OLD
	float fInverseX = 1;
	if(psvxVolArchive->i3VolumeSize.x > 1)
		fInverseX = 1.f/((float)psvxVolArchive->i3VolumeSize.x - 0.0f);
	float fInverseY = 1;
	if(psvxVolArchive->i3VolumeSize.y > 1)
		fInverseY = 1.f/((float)psvxVolArchive->i3VolumeSize.y - 0.0f);
	float fInverseZ = 1;
	if(psvxVolArchive->i3VolumeSize.z > 1)
		fInverseZ = 1.f/((float)psvxVolArchive->i3VolumeSize.z - 0.0f);
 	D3DXMatrixScaling(&matVS2TS, fInverseX, fInverseY, fInverseZ);
#else //OLD2
	D3DXMATRIX matShift, matScale;
	D3DXMatrixTranslation(&matShift, 0.5f, 0.5f, 0.5f);
	D3DXMatrixScaling(&matScale, 1.f/(float)(psvxVolArchive->i3VolumeSize.x + psvxVolArchive->f3VoxelPitch.x * 0.00f), 
		1.f/(float)(psvxVolArchive->i3VolumeSize.y + psvxVolArchive->f3VoxelPitch.z * 0.00f), 1.f/(float)(psvxVolArchive->i3VolumeSize.z + psvxVolArchive->f3VoxelPitch.z * 0.00f));
	matVS2TS = matShift * matScale;
// #else
// 	vxfloat3 f3VecScale;
// 	f3VecScale.x = psvxVolArchive->i3VolumeSize.x == 1? 1.f : 1.f / (float)psvxVolArchive->i3VolumeSize.x;
// 	f3VecScale.y = psvxVolArchive->i3VolumeSize.y == 1? 1.f : 1.f / (float)psvxVolArchive->i3VolumeSize.y;
// 	f3VecScale.z = psvxVolArchive->i3VolumeSize.z == 1? 1.f : 1.f / (float)psvxVolArchive->i3VolumeSize.z;
// 	D3DXMatrixScaling(&matVS2TS, f3VecScale.x, f3VecScale.y, f3VecScale.z);
#endif

	pCBVrVolumeObj->matWS2TS = (*(D3DXMATRIX*)pCVObjectVolume->GetMatrixWS2OS()) * matVS2TS;

	D3DXMATRIX matVS2WS = *(D3DXMATRIX*)pCVObjectVolume->GetMatrixOS2WS();

	SVXAaBbMinMax stAaBbVS;
	pCVObjectVolume->GetOrthoBoundingBox(&stAaBbVS);
	D3DXVec3TransformCoord((D3DXVECTOR3*)&pCBVrVolumeObj->f3PosVObjectMaxWS, (D3DXVECTOR3*)&stAaBbVS.f3PosMaxBox, &matVS2WS);

	D3DXVECTOR3 v3PosVObjectMinWS, v3VecVObjectZWS, v3VecVObjectYWS, v3VecVObjectXWS;
	D3DXVec3TransformCoord(&v3PosVObjectMinWS, (D3DXVECTOR3*)&stAaBbVS.f3PosMinBox, &matVS2WS);
	D3DXVec3TransformNormal(&v3VecVObjectZWS, &D3DXVECTOR3(0, 0, -1.f), &matVS2WS);
	D3DXVec3TransformNormal(&v3VecVObjectYWS, &D3DXVECTOR3(0, 1.f, 0), &matVS2WS);
	D3DXVec3TransformNormal(&v3VecVObjectXWS, &D3DXVECTOR3(1.f, 0, 0), &matVS2WS);
	D3DXVECTOR3 v3VecCrossZY;
	D3DXVec3Cross(&v3VecCrossZY, &v3VecVObjectZWS, &v3VecVObjectYWS);
	if(D3DXVec3Dot(&v3VecCrossZY, &v3VecVObjectXWS) < 0)
		D3DXVec3TransformNormal(&v3VecVObjectYWS, &D3DXVECTOR3(1.f, 0, 0), &matVS2WS);
	VXHMMatrixWS2CS((vxmatrix*)&pCBVrVolumeObj->matAlginedWS, (vxfloat3*)&v3PosVObjectMinWS, (vxfloat3*)&v3VecVObjectYWS, (vxfloat3*)&v3VecVObjectZWS);

	vxdouble4 d4ShadingFactor = vxdouble4(0.4, 0.6, 0.8, 70); 
	VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_double4_ShadingFactors"), &d4ShadingFactor);
	pCBVrVolumeObj->f4ShadingFactor = D3DXVECTOR4((float)d4ShadingFactor.x, (float)d4ShadingFactor.y, (float)d4ShadingFactor.z, (float)d4ShadingFactor.w);
	
	vxmatrix svxMatRot;
	vxdouble3 d3PosOrthoMaxBox;
	D3DXMatrixIdentity(&pCBVrVolumeObj->matClipBoxTransform);
	int iClippingMode = 0;
	VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_int_ClippingMode"), &iClippingMode);
	if(VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_matrix44_MatrixClipWS2BS"), &svxMatRot)
		&& VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_double3_PosClipBoxMaxWS"), &d3PosOrthoMaxBox) && iClippingMode & 0x1)
	{
		pCBVrVolumeObj->iFlags |= 0x2;
		pCBVrVolumeObj->matClipBoxTransform = *(D3DXMATRIX*)&svxMatRot;
		pCBVrVolumeObj->f3ClipBoxOrthoMaxWS = *(D3DXVECTOR3*)&vxfloat3(d3PosOrthoMaxBox);
	}

	double dClipPlaneIntensity = 1.0;
	VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_double_ClipPlaneIntensity"), &dClipPlaneIntensity);
	pCBVrVolumeObj->fClipPlaneIntensity = (float)dClipPlaneIntensity;

	vxdouble3 d3PosOnPlane, d3VecPlane;
	if(VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_double3_PosClipPlaneWS"), &d3PosOnPlane)
		&& VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_double3_VecClipPlaneWS"), &d3VecPlane))
	{
		pCBVrVolumeObj->f3PosClipPlaneWS = *(D3DXVECTOR3*)&vxfloat3(d3PosOnPlane);
		pCBVrVolumeObj->f3VecClipPlaneWS = *(D3DXVECTOR3*)&vxfloat3(d3VecPlane);
	}
	bool bIsClippingPlane = false;
	VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_bool_IsClippingPlane"), &bIsClippingPlane);
	if(bIsClippingPlane)
		pCBVrVolumeObj->iFlags |= 0x1;

	switch(psvxVolArchive->eVolumeDataType)
	{
	case vxrDataTypeBYTE:
	case vxrDataTypeCHAR:
		pCBVrVolumeObj->fSampleValueRange = 255.f;
		break;
	case vxrDataTypeUNSIGNEDSHORT:
	case vxrDataTypeSHORT:
		pCBVrVolumeObj->fSampleValueRange = 65535.f;
		break;
	default:
		return false;
	}
	
	D3DXVECTOR3 f3VecSampleDirWS;
	pCCObject->GetCameraState(NULL, (vxfloat3*)&f3VecSampleDirWS, NULL);
	//
	D3DXVECTOR3 f3VecSampleDirVS;
	D3DXVec3TransformNormal(&f3VecSampleDirVS, &f3VecSampleDirWS, (D3DXMATRIX*)pCVObjectVolume->GetMatrixWS2OS());
	D3DXVec3Normalize(&f3VecSampleDirVS, &f3VecSampleDirVS);
	D3DXVec3TransformNormal(&f3VecSampleDirWS, &f3VecSampleDirVS, (D3DXMATRIX*)pCVObjectVolume->GetMatrixOS2WS());
	pCBVrVolumeObj->fSampleDistWS = D3DXVec3Length(&f3VecSampleDirWS);// *0.5f;
	D3DXVec3TransformNormal(&pCBVrVolumeObj->f3VecGradientSampleX, &D3DXVECTOR3(pCBVrVolumeObj->fSampleDistWS, 0, 0), &pCBVrVolumeObj->matWS2TS);
	D3DXVec3TransformNormal(&pCBVrVolumeObj->f3VecGradientSampleY, &D3DXVECTOR3(0, pCBVrVolumeObj->fSampleDistWS, 0), &pCBVrVolumeObj->matWS2TS);
	D3DXVec3TransformNormal(&pCBVrVolumeObj->f3VecGradientSampleZ, &D3DXVECTOR3(0, 0, pCBVrVolumeObj->fSampleDistWS), &pCBVrVolumeObj->matWS2TS);
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
	D3DXMatrixTranspose(&pCBVrVolumeObj->matWS2TS, &pCBVrVolumeObj->matWS2TS);
	D3DXMatrixTranspose(&pCBVrVolumeObj->matAlginedWS, &pCBVrVolumeObj->matAlginedWS);
	D3DXMatrixTranspose(&pCBVrVolumeObj->matClipBoxTransform, &pCBVrVolumeObj->matClipBoxTransform);
	pCBVrVolumeObj->f3SizeVolumeCVS = D3DXVECTOR3((float)i3SizeVolumeTextureElements.x, (float)i3SizeVolumeTextureElements.y, (float)i3SizeVolumeTextureElements.z);

	// from pmapDValueVolume //
	double dSamplePrecisionLevel = 1.0;
	bool bIsForcedVzThickness = false;
	double dThicknessVirtualzSlab = (double)pCBVrVolumeObj->fSampleDistWS;
	double dVoxelSharpnessForAttributeVolume = 0.25;

	if(pmapDValueVolume != NULL)
	{
		VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_double_SamplePrecisionLevel"), &dSamplePrecisionLevel);
		VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_double_VZThickness"), &dThicknessVirtualzSlab);
		VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_double_VoxelSharpnessForAttributeVolume"), &dVoxelSharpnessForAttributeVolume);
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

bool HDx11ComputeConstantBufferVrBlockObj(CB_VrBlockObject* pCBVrBlockObj, CVXVObjectVolume* pCVolume, int iLevelBlock, float fScaleValue)
{
	pCBVrBlockObj->fSampleValueRange = fScaleValue;

	SVXVolumeDataArchive* psvxVolArchive = pCVolume->GetVolumeArchive();
	SVXVolumeBlock* psvxBlock = pCVolume->GetVolumeBlock(iLevelBlock);
	if(psvxBlock == NULL)
		return false;

	pCBVrBlockObj->f3SizeBlockTS = D3DXVECTOR3((float)psvxBlock->i3BlockSize.x / (float)psvxVolArchive->i3VolumeSize.x
		, (float)psvxBlock->i3BlockSize.y / (float)psvxVolArchive->i3VolumeSize.y, (float)psvxBlock->i3BlockSize.z / (float)psvxVolArchive->i3VolumeSize.z);
	return true;
}

bool HDx11ComputeConstantBufferVrBrickChunk(CB_VrBrickChunk* pCBVrBrickChunk, CVXVObjectVolume* pCVolume, int iLevelBlock, SVXGPUResourceArchive* psvxResBrickChunk)
{
	SVXVolumeBlock* psvxBlock = pCVolume->GetVolumeBlock(iLevelBlock);
	if(psvxBlock == NULL)
		return false;

	vxint3 i3NumElement = psvxResBrickChunk->GetSizeLogicalElement();
	pCBVrBrickChunk->f3SizeBrickFullTS.x = (float)(psvxBlock->i3BlockSize.x + psvxBlock->iBrickExtraSize * 2)/(float)i3NumElement.x;
	pCBVrBrickChunk->f3SizeBrickFullTS.y = (float)(psvxBlock->i3BlockSize.y + psvxBlock->iBrickExtraSize * 2)/(float)i3NumElement.y;
	pCBVrBrickChunk->f3SizeBrickFullTS.z = (float)(psvxBlock->i3BlockSize.z + psvxBlock->iBrickExtraSize * 2)/(float)i3NumElement.z;

	int iNumBricksX = i3NumElement.x / (psvxBlock->i3BlockSize.x + psvxBlock->iBrickExtraSize * 2);
	int iNumBricksY = i3NumElement.y / (psvxBlock->i3BlockSize.y + psvxBlock->iBrickExtraSize * 2);
	pCBVrBrickChunk->iNumBricksInChunkXY = iNumBricksX * iNumBricksY;
	pCBVrBrickChunk->iNumBricksInChunkX = iNumBricksX;

	pCBVrBrickChunk->f3SizeBrickExTS.x = (float)(psvxBlock->iBrickExtraSize)/(float)i3NumElement.x;
	pCBVrBrickChunk->f3SizeBrickExTS.y = (float)(psvxBlock->iBrickExtraSize)/(float)i3NumElement.y;
	pCBVrBrickChunk->f3SizeBrickExTS.z = (float)(psvxBlock->iBrickExtraSize)/(float)i3NumElement.z;

	SVXVolumeDataArchive* psvxVolArchive = pCVolume->GetVolumeArchive();
	pCBVrBrickChunk->f3ConvertRatioVTS2CTS = D3DXVECTOR3(
		(float)psvxVolArchive->i3VolumeSize.x / (float)i3NumElement.x, 
		(float)psvxVolArchive->i3VolumeSize.y / (float)i3NumElement.y, 
		(float)psvxVolArchive->i3VolumeSize.z / (float)i3NumElement.z
		);

	return true;
}

bool HDx11ComputeConstantBufferVrCamera(CB_VrCameraState* pCBVrCamera, int iGridSize, CVXCObject* pCCObject, vxint2 i2ScreenSizeFB, map<wstring, wstring>* pmapCustomParameter)
{
	D3DXMATRIX matSS2PS, matPS2CS, matCS2WS;
	pCCObject->GetMatricesSStoWS((vxmatrix*)&matSS2PS, (vxmatrix*)&matPS2CS, (vxmatrix*)&matCS2WS);
	pCBVrCamera->matSS2WS = matSS2PS*(matPS2CS*matCS2WS);

	pCCObject->GetCameraState((vxfloat3*)&pCBVrCamera->f3PosEyeWS, (vxfloat3*)&pCBVrCamera->f3VecViewWS, NULL);
	
	vxint2 i2SizeFB = i2ScreenSizeFB;
	pCBVrCamera->uiScreenSizeX = i2SizeFB.x;
	pCBVrCamera->uiScreenSizeY = i2SizeFB.y;
	uint uiNumGridX = (uint)ceil(i2SizeFB.x/(float)iGridSize);
	pCBVrCamera->uiGridOffsetX = uiNumGridX*iGridSize;

	bool bIsLightOnCamera = true;
	VXHStringGetParameterFromCustomStringMap(&bIsLightOnCamera, pmapCustomParameter, _T("_bool_IsLightOnCamera"));
	vxdouble3 d3VecLightWS(pCBVrCamera->f3VecViewWS.x, pCBVrCamera->f3VecViewWS.y, pCBVrCamera->f3VecViewWS.z);
	vxdouble3 d3PosLightWS(pCBVrCamera->f3PosEyeWS.x, pCBVrCamera->f3PosEyeWS.y, pCBVrCamera->f3PosEyeWS.z);
	if(!bIsLightOnCamera)
	{
		VXHStringGetParameterFromCustomStringMap(&d3VecLightWS, pmapCustomParameter, _T("_double3_VecLightWS"));
		VXHStringGetParameterFromCustomStringMap(&d3PosLightWS, pmapCustomParameter, _T("_double3_PosLightWS"));
	}
	pCBVrCamera->f3PosGlobalLightWS = *(D3DXVECTOR3*)&vxfloat3(d3PosLightWS);
	pCBVrCamera->f3VecGlobalLightWS = *(D3DXVECTOR3*)&vxfloat3(d3VecLightWS);
	D3DXVec3Normalize(&pCBVrCamera->f3VecGlobalLightWS, &pCBVrCamera->f3VecGlobalLightWS);
	
	pCBVrCamera->iFlags = 0;
	if(pCCObject->IsPerspective())
		pCBVrCamera->iFlags |= 0x1;
	bool bIsSpotLight = false;
	VXHStringGetParameterFromCustomStringMap(&bIsSpotLight, pmapCustomParameter, _T("_bool_IsPointSpotLight"));
	if(bIsSpotLight)
		pCBVrCamera->iFlags |= 0x2;
	
	// Matrix Transpose //
	D3DXMatrixTranspose(&pCBVrCamera->matSS2WS, &pCBVrCamera->matSS2WS);

	return true;
}

bool HDx11ComputeConstantBufferVrOtf1D(CB_VrOtf1D* pCBVrOtf1D, CVXTObject* pCTObject, int iOtfIndex, map<wstring, wstring>* pmapCustomParameter)
{
	SVXTransferFunctionArchive* psvxTfArchive = pCTObject->GetTransferFunctionArchive();
	pCBVrOtf1D->iOtf1stValue = psvxTfArchive->i3ValidMinIndex.x;
	pCBVrOtf1D->iOtfLastValue = psvxTfArchive->i3ValidMaxIndex.x;
	pCBVrOtf1D->iOtfSize = psvxTfArchive->i3DimSize.x;
	pCBVrOtf1D->f4OtfColorEnd = *(D3DXVECTOR4*)&((vxfloat4**)psvxTfArchive->ppvArchiveTF)[0][psvxTfArchive->i3DimSize.x - 1];
	if(iOtfIndex == 1)
		pCBVrOtf1D->f4OtfColorEnd += *(D3DXVECTOR4*)&((vxfloat4**)psvxTfArchive->ppvArchiveTF)[1][psvxTfArchive->i3DimSize.x - 1];

	return true;
}

bool HDx11ComputeConstantBufferVrSurfaceEffect(CB_VrSurfaceEffect* pCBVrSurfaceEffect, map<wstring, vector<double>>* pmapDValueVolume)
{
	bool bApplySurfaceAO = false;
	VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_bool_ApplySurfaceAO"), &bApplySurfaceAO);
	bool bApplyAnisotropicBRDF = false;
	VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_bool_ApplyAnisotropicBRDF"), &bApplyAnisotropicBRDF);
// 	bool bApplyShadingFactor = true;
// 	VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_bool_ApplyShadingFactor"), &bApplyShadingFactor);
	int iOcclusionNumSamplesPerKernelRay = 10;
	VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_int_OcclusionNumSamplesPerKernelRay"), &iOcclusionNumSamplesPerKernelRay);
	double dOcclusionSampleStepDistOfKernelRay = 1;
	VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_double_OcclusionSampleStepDistOfKernelRay"), &dOcclusionSampleStepDistOfKernelRay);
	double dPhongBRDF_DiffuseRatio = 0.5;
	VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_double_PhongBRDFDiffuseRatio"), &dPhongBRDF_DiffuseRatio);
	double dPhongBRDF_ReflectanceRatio = 0.5;
	VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_double_PhongBRDFReflectanceRatio"), &dPhongBRDF_ReflectanceRatio);
	double dPhongExpWeightU = 50;
	VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_double_PhongExpWeightU"), &dPhongExpWeightU);
	double dPhongExpWeightV = 50;
	VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_double_PhongExpWeightV"), &dPhongExpWeightV);

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
	VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_int_SizeKernelInVoxel"), &iSizeKernel);
	VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_int_ModeCurvatureDescription"), &iModeCurvatureDescription);
	if (iModeCurvatureDescription == 1)
	{
		double dThetaCurvatureTable = 0;
		VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_double_ThetaCurvatureTable"), &dThetaCurvatureTable);
		fThetaCurvatureTable = (float)dThetaCurvatureTable;
		double dRadiusCurvatureTable = 0;
		VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_double_RadiusCurvatureTable"), &dRadiusCurvatureTable);
		fRadiusCurvatureTable = (float)dRadiusCurvatureTable;

		VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_bool_IsConcavenessDirection"), &bIsConcavenessDirection);
	}
	pCBVrSurfaceEffect->iIsoSurfaceRenderingMode |= (iModeCurvatureDescription << 4) | (bIsConcavenessDirection << 5);
	pCBVrSurfaceEffect->iSizeCurvatureKernel = iSizeKernel;
	pCBVrSurfaceEffect->fThetaCurvatureTable = fThetaCurvatureTable;
	pCBVrSurfaceEffect->fRadiusCurvatureTable = fRadiusCurvatureTable;

	return true;
}

bool HDx11ComputeConstantBufferVrInterestingRegion(CB_VrInterestingRegion* pCBVrInterestingRegion, map<wstring, vector<double>>* pmapDValueVolume)
{
	pCBVrInterestingRegion->iNumRegions = 0;
	VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_int_NumRoiPoints"), &pCBVrInterestingRegion->iNumRegions);
	if (pCBVrInterestingRegion->iNumRegions == 0)
		return true;

	vxdouble3 d3RoiPoint0WS, d3RoiPoint1WS, d3RoiPoint2WS;
	VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_double3_RoiPoint0"), &d3RoiPoint0WS);
	VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_double3_RoiPoint1"), &d3RoiPoint1WS);
	VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_double3_RoiPoint2"), &d3RoiPoint2WS);
	pCBVrInterestingRegion->f3PosPtn0WS = *(D3DXVECTOR3*)&vxfloat3(d3RoiPoint0WS);
	pCBVrInterestingRegion->f3PosPtn1WS = *(D3DXVECTOR3*)&vxfloat3(d3RoiPoint1WS);
	pCBVrInterestingRegion->f3PosPtn2WS = *(D3DXVECTOR3*)&vxfloat3(d3RoiPoint2WS);

	double dRadius0WS, dRadius1WS, dRadius2WS;
	VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_double_RoiRadius0"), &dRadius0WS);
	VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_double_RoiRadius1"), &dRadius1WS);
	VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_double_RoiRadius2"), &dRadius2WS);
	pCBVrInterestingRegion->fRadius0 = (float)dRadius0WS;
	pCBVrInterestingRegion->fRadius1 = (float)dRadius1WS;
	pCBVrInterestingRegion->fRadius2 = (float)dRadius2WS;

	return true;
}

bool GradientMagnitudeAnalysis(vxdouble2* pd2GradientMagMinMax, CVXVObjectVolume* pCVolume, SVXLocalProgress* psvxProgress)
{
	if(pCVolume->GetCustomParameter(_T("_double2_GraidentMagMinMax"), pd2GradientMagMinMax))
		return true;

	const SVXVolumeDataArchive* psvxVolumeArchive = pCVolume->GetVolumeArchive();
	if(psvxVolumeArchive->eVolumeDataType != vxrDataTypeUNSIGNEDSHORT)
		return false;

	if(psvxProgress)
	{
		psvxProgress->dRangeProgress = 70;
	}

	ushort** ppusVolume = (ushort**)psvxVolumeArchive->ppvVolumeSlices;
	int iSizeAddrX = psvxVolumeArchive->i3VolumeSize.x + psvxVolumeArchive->i3SizeExtraBoundary.x*2;
	vxdouble2 d2GradMagMinMax;
	d2GradMagMinMax.x = DBL_MAX;
	d2GradMagMinMax.y = -DBL_MAX;
	for(int iZ = 1; iZ < psvxVolumeArchive->i3VolumeSize.z - 1; iZ+=2)
	{
		if(psvxProgress)
		{
			*psvxProgress->pdProgressOfCurWork = 
				(int)(psvxProgress->dStartProgress + psvxProgress->dRangeProgress*iZ/psvxVolumeArchive->i3VolumeSize.z);
		}

		for(int iY = 1; iY < psvxVolumeArchive->i3VolumeSize.y - 1; iY+=2)
		{
			for(int iX = 1; iX < psvxVolumeArchive->i3VolumeSize.x - 1; iX+=2)
			{
				vxdouble3 d3Difference;
				int iAddrZ = iZ + psvxVolumeArchive->i3SizeExtraBoundary.x;
				int iAddrY = iY + psvxVolumeArchive->i3SizeExtraBoundary.y;
				int iAddrX = iX + psvxVolumeArchive->i3SizeExtraBoundary.z;
				int iAddrZL = iZ - 1 + psvxVolumeArchive->i3SizeExtraBoundary.z;
				int iAddrYL = iY - 1 + psvxVolumeArchive->i3SizeExtraBoundary.y;
				int iAddrXL = iX - 1 + psvxVolumeArchive->i3SizeExtraBoundary.x;
				int iAddrZR = iZ + 1 + psvxVolumeArchive->i3SizeExtraBoundary.z;
				int iAddrYR = iY + 1 + psvxVolumeArchive->i3SizeExtraBoundary.y;
				int iAddrXR = iX + 1 + psvxVolumeArchive->i3SizeExtraBoundary.x;
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

	pCVolume->RegisterCustomParameter(_T("_double2_GraidentMagMinMax"), d2GradMagMinMax);

	if(psvxProgress)
	{
		psvxProgress->dStartProgress = psvxProgress->dStartProgress + psvxProgress->dRangeProgress;
		*psvxProgress->pdProgressOfCurWork = (psvxProgress->dStartProgress);
		psvxProgress->dRangeProgress = 30;
	}

	return true;
}

bool HDx11ComputeConstantBufferVrModulation(CB_VrModulation* pCBVrModulation, CVXVObjectVolume* pCVObjectVolume, vxfloat3 f3PosEyeWS, map<wstring, vector<double>>* pmapDValueVolume)
{
	double dGradMagAmplifier = 1.0;
	VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_double_GradMagAmplifier"), &dGradMagAmplifier);
	double dRevealingFactor = 90.0;
	VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_double_RevealingFactor"), &dRevealingFactor);
	double dSharpnessFactor = 0.40;
	VXHGetCustomValueFromValueContinerMap(pmapDValueVolume, _T("_double_SharpnessFactor"), &dSharpnessFactor);

	vxdouble2 d2GradientMagMinMax;
	if(!GradientMagnitudeAnalysis(&d2GradientMagMinMax, pCVObjectVolume, NULL))
		return false;
	float fMaxGradientSize = (float)(d2GradientMagMinMax.y/65535.0);

	pCBVrModulation->fGradMagAmplifier = (float)dGradMagAmplifier;
	pCBVrModulation->fRevealingFactor = (float)dRevealingFactor;
	pCBVrModulation->fSharpnessFactor = max((float)dSharpnessFactor, 0.00000001f);
	pCBVrModulation->fMaxGradientSize = fMaxGradientSize;

	SVXAaBbMinMax stAaBbVS;
	pCVObjectVolume->GetOrthoBoundingBox(&stAaBbVS);
	vxfloat3 f3PosCenterWS = (stAaBbVS.f3PosMinBox + stAaBbVS.f3PosMaxBox)*0.5f;
	vxfloat3 f3VecMax2MinWS = stAaBbVS.f3PosMinBox - stAaBbVS.f3PosMaxBox;
	float fDistEye2Center = VXHMLengthVector(&(f3PosEyeWS - f3PosCenterWS));
	pCBVrModulation->fDistEyeMaximum = fDistEye2Center + VXHMLengthVector(&f3VecMax2MinWS)*0.5f;

	return true;
}

bool HDx11ComputeConstantBufferVrShadowMap(CB_VrShadowMap* pCBVrShadowMap, CB_VrCameraState* pCBVrCamStateForShadowMap, 
	vxfloat3 f3PosOverviewBoxMinWS, vxfloat3 f3PosOverviewBoxMaxWS, map<wstring, wstring>* pmapCustomParameter)
{
	double dShadowOcclusionWeight = 0.5;
	VXHStringGetParameterFromCustomStringMap(&dShadowOcclusionWeight, pmapCustomParameter, _T("_double_ShadowOcclusionWeight"));
	pCBVrShadowMap->fShadowOcclusionWeight = (float)dShadowOcclusionWeight;					// SM

	int iDepthBiasSampleCount = 5;
	VXHStringGetParameterFromCustomStringMap(&iDepthBiasSampleCount, pmapCustomParameter, _T("_int_DepthBiasSampleCount"));
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

	D3DXMATRIX matSS2PS, matPS2CS, matCS2WS;
	D3DXMATRIX matWS2CS, matCS2PS, matPS2SS;

	D3DXVECTOR3 f3VecUp(0, 0, 1.f);
	D3DXVECTOR3 f3VecRightTmp;
	while(D3DXVec3Length(D3DXVec3Cross(&f3VecRightTmp, &pCBVrCamStateForShadowMap->f3VecViewWS, &f3VecUp)) == 0)
		f3VecUp = f3VecUp + D3DXVECTOR3(0, 1.f, 0);
	D3DXMatrixLookAtRH(&pCBVrShadowMap->matWS2WLS, &pCBVrCamStateForShadowMap->f3PosEyeWS, 
		&(pCBVrCamStateForShadowMap->f3PosEyeWS + pCBVrCamStateForShadowMap->f3VecViewWS), &f3VecUp);	// SM

	matWS2CS = pCBVrShadowMap->matWS2WLS;
	if(pCBVrCamStateForShadowMap->iFlags & 0x2)
	{
		D3DXMatrixPerspectiveFovRH(&matCS2PS, D3DX_PI/4.f, 1.f, 0.1f, fDistCenter2Light + fDiagonalLength/2);
	}
	else
	{
		D3DXMatrixOrthoRH(&matCS2PS, fDiagonalLength, fDiagonalLength, 0, fDistCenter2Light + fDiagonalLength/2);
	}
	VXHMMatrixPS2SS((vxmatrix*)&matPS2SS, (float)pCBVrCamStateForShadowMap->uiScreenSizeX, (float)pCBVrCamStateForShadowMap->uiScreenSizeY);

	D3DXMatrixInverse(&matCS2WS, NULL, &matWS2CS);
	D3DXMatrixInverse(&matPS2CS, NULL, &matCS2PS);
	D3DXMatrixInverse(&matSS2PS, NULL, &matPS2SS);

	pCBVrCamStateForShadowMap->matSS2WS = matSS2PS*(matPS2CS*matCS2WS);	// CS
	pCBVrShadowMap->matWS2LS = matWS2CS*matCS2PS*matPS2SS; // SM

	D3DXMatrixTranspose(&pCBVrShadowMap->matWS2WLS, &pCBVrShadowMap->matWS2WLS);
	D3DXMatrixTranspose(&pCBVrShadowMap->matWS2LS, &pCBVrShadowMap->matWS2LS);
	D3DXMatrixTranspose(&pCBVrCamStateForShadowMap->matSS2WS, &pCBVrCamStateForShadowMap->matSS2WS);

	// 1st bit - 0 : Orthogonal or 1 : Perspective
	// 2nd bit - 0 : Parallel Light or 1 : Point Spot Light
	if(pCBVrCamStateForShadowMap->iFlags & 0x2)
		pCBVrCamStateForShadowMap->iFlags |= 0x1;	// SM
	else
		pCBVrCamStateForShadowMap->iFlags &= 0x0;

// 	D3DXVECTOR3 f3VecViewVS, f3VecViewWS;
// 	D3DXVec3TransformNormal(&f3VecViewVS, &pCBVrCamStateForShadowMap->f3VecViewWS, (D3DXMATRIX*)pCVObjectVolume->GetMatrixWS2OS());
// 	D3DXVec3Normalize(&f3VecViewVS, &f3VecViewVS);
// 	D3DXVec3TransformNormal(&f3VecViewWS, &f3VecViewVS, (D3DXMATRIX*)pCVObjectVolume->GetMatrixOS2WS());
// 	pCBVrShadowMap->fSampleDistWLS = D3DXVec3Length(&f3VecViewWS) * 0.5f;	// SM
	pCBVrShadowMap->fOpaqueCorrectionSMap = 1.f;							// CS

// 	double dLightSamplePrecisionLevel = 2;
// 	VXHStringGetParameterFromCustomStringMap(&dLightSamplePrecisionLevel, pmapCustomParameter, _T("_double_LightSamplePrecisionLevel"));
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

bool HDx11MixOut(CVXIObject* pCIObjectMerger, CVXIObject* pCIObjectSystemOut, int iLOD)
{
	g_mutex_GpuCriticalPath.lock();
	if (g_pCGpuManager == NULL || pCIObjectMerger == NULL || !g_VxgCommonParams.bIsInitialized)
	{
		VXERRORMESSAGE("CVXGPUManager Fault!!");
		return false;
	}
	//ID3D11Device* pdx11Device;
	ID3D11DeviceContext* pdx11DeviceImmContext = g_VxgCommonParams.pdx11DeviceImmContext;
	ID3D11RenderTargetView* pdxRTVOld = NULL;
	ID3D11DepthStencilView* pdxDSVOld = NULL;
	pdx11DeviceImmContext->OMGetRenderTargets(1, &pdxRTVOld, &pdxDSVOld);

#pragma region FB Setting
	bool bInitGen = false;
	SVXGPUResourceDESC gpuResourcesFB_DESCs[__FB_COUNT];
	SVXGPUResourceArchive gpuResourcesFB_ARCHs[__FB_COUNT];
	for (int i = 0; i < __FB_COUNT; i++)
	{
		gpuResourcesFB_DESCs[i].eGpuSdkType = vxgGpuSdkTypeDX11;
		gpuResourcesFB_DESCs[i].iCustomMisc = 0;
		gpuResourcesFB_DESCs[i].iSourceObjectID = pCIObjectMerger->GetObjectID();
	}
	gpuResourcesFB_DESCs[__FB_TX2D_RENDEROUT].eResourceType = vxgResourceTypeDX11RESOURCE;
	gpuResourcesFB_DESCs[__FB_TX2D_RENDEROUT].strUsageSpecifier = _T("TEXTURE2D_IOBJECT_RENDEROUT");
	gpuResourcesFB_DESCs[__FB_TX2D_RENDEROUT].iCustomMisc = NUM_TEXRT_LAYERS;
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
	for (int i = 0; i < __FB_COUNT; i++)
	{
		if (!g_pCGpuManager->GetGpuResourceArchive(&gpuResourcesFB_DESCs[i], &gpuResourcesFB_ARCHs[i]))
		{
			g_pCGpuManager->GenerateGpuResource(&gpuResourcesFB_DESCs[i], pCIObjectMerger, &gpuResourcesFB_ARCHs[i], NULL);
			bInitGen = true;
		}
	}

	SVXGPUResourceDESC gpuResourceMERGE_PS_DESCs[__EXFB_COUNT];
	SVXGPUResourceArchive gpuResourceMERGE_PS_ARCHs[__EXFB_COUNT];
	for (int i = 0; i < __EXFB_COUNT; i++)
	{
		gpuResourceMERGE_PS_DESCs[i].eGpuSdkType = vxgGpuSdkTypeDX11;
		gpuResourceMERGE_PS_DESCs[i].strUsageSpecifier = _T("TEXTURE2D_IOBJECT_RENDEROUT");
		gpuResourceMERGE_PS_DESCs[i].iCustomMisc = NUM_MERGE_LAYERS * 2;	// two of NUM_MERGE_LAYERS resource buffers
		gpuResourceMERGE_PS_DESCs[i].iSourceObjectID = pCIObjectMerger->GetObjectID();
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
	for (int i = 0; i < __EXFB_COUNT; i++)
	{
		if (!g_pCGpuManager->GetGpuResourceArchive(&gpuResourceMERGE_PS_DESCs[i], &gpuResourceMERGE_PS_ARCHs[i]))
		{
			g_pCGpuManager->GenerateGpuResource(&gpuResourceMERGE_PS_DESCs[i], pCIObjectMerger, &gpuResourceMERGE_PS_ARCHs[i], NULL);
			bInitGen = true;
		}
	}

	vxint2 i2SizeScreenCurrent, i2SizeScreenOld = vxint2(0, 0);
	pCIObjectMerger->GetFrameBufferInfo(&i2SizeScreenCurrent);
	if (bInitGen)
		pCIObjectMerger->RegisterCustomParameter(_T("_int2_PreviousScreenSize"), i2SizeScreenCurrent);
	pCIObjectMerger->GetCustomParameter(_T("_int2_PreviousScreenSize"), &i2SizeScreenOld);
	if (i2SizeScreenCurrent.x != i2SizeScreenOld.x || i2SizeScreenCurrent.y != i2SizeScreenOld.y)
	{
		g_pCGpuManager->ReleaseGpuResourcesRelatedObject(pCIObjectMerger->GetObjectID());

		for (int i = 0; i < __EXFB_COUNT; i++)
		{
			g_pCGpuManager->GenerateGpuResource(&gpuResourceMERGE_PS_DESCs[i], pCIObjectMerger, &gpuResourceMERGE_PS_ARCHs[i], NULL);
		}

		for (int i = 0; i < __FB_COUNT; i++)
		{
			g_pCGpuManager->GenerateGpuResource(&gpuResourcesFB_DESCs[i], pCIObjectMerger, &gpuResourcesFB_ARCHs[i], NULL);
		}

		pCIObjectMerger->RegisterCustomParameter(_T("_int2_PreviousScreenSize"), i2SizeScreenCurrent);
	}
	if (pCIObjectSystemOut)
		pCIObjectSystemOut->RegisterCustomParameter(_T("_int2_PreviousScreenSize"), i2SizeScreenCurrent);

	SVXGPUResourceDESC gpuResourceFB_RENDEROUT_SYSTEM_DESC;
	SVXGPUResourceDESC gpuResourceFB_DEPTH_SYSTEM_DESC;
	SVXGPUResourceArchive gpuResourceFB_RENDEROUT_SYSTEM_ARCH;
	SVXGPUResourceArchive gpuResourceFB_DEPTH_SYSTEM_ARCH;
	gpuResourceFB_RENDEROUT_SYSTEM_DESC.eResourceType = vxgResourceTypeDX11RESOURCE;
	gpuResourceFB_RENDEROUT_SYSTEM_DESC.eDataType = vxrDataTypeBYTE4CHANNEL;
	gpuResourceFB_RENDEROUT_SYSTEM_DESC.eGpuSdkType = vxgGpuSdkTypeDX11;
	gpuResourceFB_RENDEROUT_SYSTEM_DESC.strUsageSpecifier = _T("TEXTURE2D_IOBJECT_SYSTEM");
	gpuResourceFB_RENDEROUT_SYSTEM_DESC.iSizeStride = VXHGetDataTypeSizeByte(gpuResourceFB_RENDEROUT_SYSTEM_DESC.eDataType);
	gpuResourceFB_RENDEROUT_SYSTEM_DESC.iCustomMisc = 0;
	gpuResourceFB_RENDEROUT_SYSTEM_DESC.iSourceObjectID = pCIObjectMerger->GetObjectID();
	gpuResourceFB_DEPTH_SYSTEM_DESC.eResourceType = vxgResourceTypeDX11RESOURCE;
	gpuResourceFB_DEPTH_SYSTEM_DESC.eDataType = vxrDataTypeFLOAT;
	gpuResourceFB_DEPTH_SYSTEM_DESC.eGpuSdkType = vxgGpuSdkTypeDX11;
	gpuResourceFB_DEPTH_SYSTEM_DESC.strUsageSpecifier = _T("TEXTURE2D_IOBJECT_SYSTEM");
	gpuResourceFB_DEPTH_SYSTEM_DESC.iSizeStride = VXHGetDataTypeSizeByte(gpuResourceFB_DEPTH_SYSTEM_DESC.eDataType);
	gpuResourceFB_DEPTH_SYSTEM_DESC.iCustomMisc = 0;
	gpuResourceFB_DEPTH_SYSTEM_DESC.iSourceObjectID = pCIObjectMerger->GetObjectID();
	// When Resize, the Framebuffer is released, so we do not have to check the resize case.
	if (!g_pCGpuManager->GetGpuResourceArchive(&gpuResourceFB_RENDEROUT_SYSTEM_DESC, &gpuResourceFB_RENDEROUT_SYSTEM_ARCH))
		g_pCGpuManager->GenerateGpuResource(&gpuResourceFB_RENDEROUT_SYSTEM_DESC, pCIObjectMerger, &gpuResourceFB_RENDEROUT_SYSTEM_ARCH, NULL);
	if (!g_pCGpuManager->GetGpuResourceArchive(&gpuResourceFB_DEPTH_SYSTEM_DESC, &gpuResourceFB_DEPTH_SYSTEM_ARCH))
		g_pCGpuManager->GenerateGpuResource(&gpuResourceFB_DEPTH_SYSTEM_DESC, pCIObjectMerger, &gpuResourceFB_DEPTH_SYSTEM_ARCH, NULL);
#pragma endregion

#pragma region Other Presetting For Shaders
	uint uiClearVlaues[4] = { 0 };
	float fClearColor[4];
	fClearColor[0] = 0; fClearColor[1] = 0; fClearColor[2] = 0; fClearColor[3] = 0;
	if (bInitGen)
	{
		// Clear Merge Buffer //
		for (int i = 0; i < NUM_MERGE_LAYERS * 2; i++)
		{
			pdx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gpuResourceMERGE_PS_ARCHs[__EXFB_RTV_MERGE_RAYSEGMENT].vtrPtrs.at(i), fClearColor);
		}
	}

	ID3D11RenderTargetView* pdx11RTVs_NULL[2] = { NULL };
	ID3D11RenderTargetView* pdx11RTVs[2] = { (ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_RENDEROUT].vtrPtrs.at(0)
		, (ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_DEPTH_ZTHICKCS].vtrPtrs.at(0) };
	// Clear //
	for (int i = 0; i < NUM_TEXRT_LAYERS; i++)
	{
		pdx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_RENDEROUT].vtrPtrs.at(i), fClearColor);
		pdx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_DEPTH_ZTHICKCS].vtrPtrs.at(i), fClearColor);
	}
	pdx11DeviceImmContext->ClearDepthStencilView((ID3D11DepthStencilView*)gpuResourcesFB_ARCHs[__FB_DSV_DEPTH].vtrPtrs.at(0), D3D11_CLEAR_DEPTH, 1.0f, 0);
	pdx11DeviceImmContext->OMSetDepthStencilState(g_VxgCommonParams.pdx11DSStates[__DSState_DEPTHENABLE_LESSEQUAL], 0);

	pdx11DeviceImmContext->VSSetSamplers(0, 1, &g_VxgCommonParams.pdx11SStates[__SState_LINEAR_ZEROBORDER]);
	pdx11DeviceImmContext->VSSetSamplers(1, 1, &g_VxgCommonParams.pdx11SStates[__SState_POINT_ZEROBORDER]);
	pdx11DeviceImmContext->PSSetSamplers(0, 1, &g_VxgCommonParams.pdx11SStates[__SState_LINEAR_ZEROBORDER]);
	pdx11DeviceImmContext->PSSetSamplers(1, 1, &g_VxgCommonParams.pdx11SStates[__SState_POINT_ZEROBORDER]);

	// Proxy Setting //
	D3DXMATRIX dxmatWS2CS, dxmatWS2PS;
	D3DXMatrixLookAtRH(&dxmatWS2CS, &D3DXVECTOR3(0, 0, 1.f), &D3DXVECTOR3(0, 0, 0), &D3DXVECTOR3(0, 1.f, 0));
	D3DXMatrixOrthoRH(&dxmatWS2PS, 1.f, 1.f, 0.f, 2.f);
	D3DXMATRIX dxmatOS2PS = dxmatWS2CS * dxmatWS2PS;
	D3DXMatrixTranspose(&dxmatOS2PS, &dxmatOS2PS);
	CB_SrProxy cbProjCamState;
	cbProjCamState.matOS2PS = dxmatOS2PS;
	D3D11_MAPPED_SUBRESOURCE mappedProxyState;
	pdx11DeviceImmContext->Map(g_VxgCommonParams.pdx11BufConstParameters[__CB_SR_PROXY], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedProxyState);
	CB_SrProxy* pCBProxyParam = (CB_SrProxy*)mappedProxyState.pData;
	memcpy(pCBProxyParam, &cbProjCamState, sizeof(CB_SrProxy));
	pdx11DeviceImmContext->Unmap(g_VxgCommonParams.pdx11BufConstParameters[__CB_SR_PROXY], 0);
	pdx11DeviceImmContext->VSSetConstantBuffers(10, 1, &g_VxgCommonParams.pdx11BufConstParameters[__CB_SR_PROXY]);

	CB_SrProjectionCameraState cbSrCamState;
	double dVThickness = 1.0;
	pCIObjectMerger->GetCustomParameter(_T("_double_VZThickness"), &dVThickness);
	cbSrCamState.fVThicknessGlobal = (float)dVThickness;	// Only for this //
	cbSrCamState.iFlag = iLOD << 16;

	// CB Set MERGE //
	D3D11_MAPPED_SUBRESOURCE mappedSrCamState;
	pdx11DeviceImmContext->Map(g_VxgCommonParams.pdx11BufConstParameters[__CB_SR_CAMSTATE], 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSrCamState);
	CB_SrProjectionCameraState* pCBCamStateParam = (CB_SrProjectionCameraState*)mappedSrCamState.pData;
	memcpy(pCBCamStateParam, &cbSrCamState, sizeof(CB_SrProjectionCameraState));
	pdx11DeviceImmContext->Unmap(g_VxgCommonParams.pdx11BufConstParameters[__CB_SR_CAMSTATE], 0);
	pdx11DeviceImmContext->VSSetConstantBuffers(1, 1, &g_VxgCommonParams.pdx11BufConstParameters[__CB_SR_CAMSTATE]);
	pdx11DeviceImmContext->PSSetConstantBuffers(1, 1, &g_VxgCommonParams.pdx11BufConstParameters[__CB_SR_CAMSTATE]);

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
#pragma endregion

	uint uiStrideSizeInput = 0;
	uint uiOffset = 0;
	
#pragma region Copy to RenderTextures
	SVXFrameBuffer* psvxFrameBuffer_RGBA = (SVXFrameBuffer*)pCIObjectMerger->GetFrameBuffer(vxrFrameBufferUsageCUSTOM, 1);
	SVXFrameBuffer* psvxFrameBuffer_DepthCSB = (SVXFrameBuffer*)pCIObjectMerger->GetFrameBuffer(vxrFrameBufferUsageCUSTOM, 2);
	SVXFrameBuffer* psvxFrameBuffer_Thickness = (SVXFrameBuffer*)pCIObjectMerger->GetFrameBuffer(vxrFrameBufferUsageCUSTOM, 3);

	vxbyte4* py4BufferOut_RGBA = (vxbyte4*)psvxFrameBuffer_RGBA->pvBuffer;
	float* pfBufferOut_DepthCSB = (float*)psvxFrameBuffer_DepthCSB->pvBuffer;
	float* pfBufferOut_Thickness = (float*)psvxFrameBuffer_Thickness->pvBuffer;

	vxbyte4* py4Color = NULL;
	float* pfDepth = NULL;
	D3D11_MAPPED_SUBRESOURCE mappedResSysInit;

	// Copy Color 0
	pdx11DeviceImmContext->Map((ID3D11Texture2D*)gpuResourceFB_RENDEROUT_SYSTEM_ARCH.vtrPtrs.at(0), 0, D3D11_MAP_WRITE, NULL, &mappedResSysInit);
	py4Color = (vxbyte4*)mappedResSysInit.pData;
	for (int i = 0; i < i2SizeScreenCurrent.y; i++)
	{
		memcpy(&py4Color[i * mappedResSysInit.RowPitch / 4], &py4BufferOut_RGBA[i * i2SizeScreenCurrent.x], sizeof(vxbyte4)* i2SizeScreenCurrent.x);
	}
	pdx11DeviceImmContext->Unmap((ID3D11Texture2D*)gpuResourceFB_RENDEROUT_SYSTEM_ARCH.vtrPtrs.at(0), 0);
	pdx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gpuResourcesFB_ARCHs[__FB_TX2D_RENDEROUT].vtrPtrs.at(0), (ID3D11Texture2D*)gpuResourceFB_RENDEROUT_SYSTEM_ARCH.vtrPtrs.at(0));

	// Copy Depth 0
	pdx11DeviceImmContext->Map((ID3D11Texture2D*)gpuResourceFB_DEPTH_SYSTEM_ARCH.vtrPtrs.at(0), 0, D3D11_MAP_WRITE, NULL, &mappedResSysInit);
	pfDepth = (float*)mappedResSysInit.pData;
	for (int i = 0; i < i2SizeScreenCurrent.y; i++)
	{
		memcpy(&pfDepth[i * mappedResSysInit.RowPitch / 4], &pfBufferOut_DepthCSB[i * i2SizeScreenCurrent.x], sizeof(float)* i2SizeScreenCurrent.x);
	}
	pdx11DeviceImmContext->Unmap((ID3D11Texture2D*)gpuResourceFB_DEPTH_SYSTEM_ARCH.vtrPtrs.at(0), 0);
	pdx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gpuResourcesFB_ARCHs[__FB_TX2D_DEPTH_ZTHICKCS].vtrPtrs.at(0), (ID3D11Texture2D*)gpuResourceFB_DEPTH_SYSTEM_ARCH.vtrPtrs.at(0));

	// Copy Thickness 0
	pdx11DeviceImmContext->Map((ID3D11Texture2D*)gpuResourceFB_DEPTH_SYSTEM_ARCH.vtrPtrs.at(0), 0, D3D11_MAP_WRITE, NULL, &mappedResSysInit);
	pfDepth = (float*)mappedResSysInit.pData;
	for (int i = 0; i < i2SizeScreenCurrent.y; i++)
	{
		memcpy(&pfDepth[i * mappedResSysInit.RowPitch / 4], &pfBufferOut_Thickness[i * i2SizeScreenCurrent.x], sizeof(float)* i2SizeScreenCurrent.x);
	}
	pdx11DeviceImmContext->Unmap((ID3D11Texture2D*)gpuResourceFB_DEPTH_SYSTEM_ARCH.vtrPtrs.at(0), 0);
	pdx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gpuResourcesFB_ARCHs[__FB_TX2D_DEPTH_ZTHICKCS].vtrPtrs.at(2), (ID3D11Texture2D*)gpuResourceFB_DEPTH_SYSTEM_ARCH.vtrPtrs.at(0));
#pragma endregion

	// Shader 이름을 다르게 해서 진행해 보자 //!!

#pragma region Mix Merge Out
	pdx11DeviceImmContext->OMSetRenderTargets(2, pdx11RTVs_NULL, (ID3D11DepthStencilView*)gpuResourcesFB_ARCHs[__FB_DSV_DEPTH].vtrPtrs.at(0));
	pdx11DeviceImmContext->PSSetShaderResources(30, NUM_TEXRT_LAYERS, pdx11SRV_RenderOuts);
	pdx11DeviceImmContext->PSSetShaderResources(60, NUM_TEXRT_LAYERS, pdx11SRV_DepthCSs);
	pdx11DeviceImmContext->PSSetShaderResources(80, NUM_TEXRT_LAYERS / 2, &pdx11SRV_DepthCSs[2]);	// 재활용

	vector<void*>* pvtrRTV = &gpuResourceMERGE_PS_ARCHs[__EXFB_RTV_MERGE_RAYSEGMENT].vtrPtrs;
	vector<void*>* pvtrSRV = &gpuResourceMERGE_PS_ARCHs[__EXFB_SRV_MERGE_RAYSEGMENT].vtrPtrs;
	vector<ID3D11RenderTargetView*> vtrRTV_MergePingpongPS;
	vector<ID3D11ShaderResourceView*> vtrSRV_MergePingpongPS;
	for (int i = 0; i < NUM_MERGE_LAYERS * 2; i++)
	{
		vtrRTV_MergePingpongPS.push_back((ID3D11RenderTargetView*)pvtrRTV->at(i));
		vtrSRV_MergePingpongPS.push_back((ID3D11ShaderResourceView*)pvtrSRV->at(i));
	}
	int iCountMerging = 0;
	pCIObjectMerger->GetCustomParameter(_T("_int_CountMerging"), &iCountMerging);


	int iRSA_Index_Offset_Prev = 0;
	int iRSA_Index_Offset_Out = NUM_MERGE_LAYERS;
	if (iCountMerging % 2 == 0)
	{
		iRSA_Index_Offset_Prev = NUM_MERGE_LAYERS;
		iRSA_Index_Offset_Out = 0;
	}
	for (int iIndexRS = 0; iIndexRS < NUM_MERGE_LAYERS; iIndexRS++)
	{
		pdx11SRV_RSA[iIndexRS] = vtrSRV_MergePingpongPS.at(iIndexRS + iRSA_Index_Offset_Prev);	// may from SR
		pdx11RTV_RSA[iIndexRS] = vtrRTV_MergePingpongPS.at(iIndexRS + iRSA_Index_Offset_Out);
	}
	
	pdx11DeviceImmContext->PSSetShaderResources(90, NUM_MERGE_LAYERS, pdx11SRV_RSA);
	pdx11DeviceImmContext->OMSetRenderTargets(min(NUM_MERGE_LAYERS, 8), pdx11RTV_RSA, NULL);

	uiStrideSizeInput = sizeof(vxfloat3);
	uiOffset = 0;
	pdx11DeviceImmContext->IASetVertexBuffers(0, 1, (ID3D11Buffer**)&g_VxgCommonParams.pdx11BufProxyVertice, &uiStrideSizeInput, &uiOffset);
	pdx11DeviceImmContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pdx11DeviceImmContext->IASetInputLayout(g_VxgCommonParams.pdx11IinputLayouts[0]);
	pdx11DeviceImmContext->RSSetState(g_VxgCommonParams.pdx11RStates[__RState_SOLID_NONE]);
	pdx11DeviceImmContext->VSSetShader(g_VxgCommonParams.pdx11VS_SR_Shaders[__VS_PROXY], NULL, 0);
//	if (iLOD == 0)
//		pdx11DeviceImmContext->PSSetShader(g_VxgCommonParams.pdx11PS_SR_Shaders[__PS_MERGE_RSOUT_LOD2X_TO_LAYERS], NULL, 0);
//	else
		pdx11DeviceImmContext->PSSetShader(g_VxgCommonParams.pdx11PS_SR_Shaders[__PS_MERGE_RSOUT_TO_LAYERS], NULL, 0);
	pdx11DeviceImmContext->OMSetDepthStencilState(g_VxgCommonParams.pdx11DSStates[__DSState_DEPTHENABLE_ALWAYS], 0);

	pdx11DeviceImmContext->Draw(4, 0);

	pdx11DeviceImmContext->PSSetShaderResources(30, NUM_TEXRT_LAYERS, pdx11SRV_RenderOuts_NULL);
	pdx11DeviceImmContext->PSSetShaderResources(60, NUM_TEXRT_LAYERS, pdx11SRV_DepthCSs_NULL);
	pdx11DeviceImmContext->PSSetShaderResources(90, NUM_MERGE_LAYERS, pdx11SRV_RSA_NULL);
	pdx11DeviceImmContext->OMSetRenderTargets(min(NUM_MERGE_LAYERS, 8), pdx11RTV_RSA_NULL, NULL);
#pragma endregion

#pragma region // System Out
	// PS into PS
	for (int iIndexRS = 0; iIndexRS < NUM_MERGE_LAYERS; iIndexRS++)
		pdx11SRV_RSA[iIndexRS] = vtrSRV_MergePingpongPS.at(iIndexRS + iRSA_Index_Offset_Out);

	pdx11DeviceImmContext->PSSetShaderResources(90, NUM_MERGE_LAYERS, pdx11SRV_RSA);

	pdx11RTVs[0] = (ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_RENDEROUT].vtrPtrs.at(0);
	pdx11RTVs[1] = (ID3D11RenderTargetView*)gpuResourcesFB_ARCHs[__FB_RTV_DEPTH_ZTHICKCS].vtrPtrs.at(0);
	pdx11DeviceImmContext->OMSetRenderTargets(2, pdx11RTVs, NULL);

	//__PS_MERGE_LAYERS_TO_RENDEROUT
	uiStrideSizeInput = sizeof(vxfloat3);
	uiOffset = 0;
	pdx11DeviceImmContext->IASetVertexBuffers(0, 1, (ID3D11Buffer**)&g_VxgCommonParams.pdx11BufProxyVertice, &uiStrideSizeInput, &uiOffset);
	pdx11DeviceImmContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pdx11DeviceImmContext->IASetInputLayout(g_VxgCommonParams.pdx11IinputLayouts[0]);
	pdx11DeviceImmContext->RSSetState(g_VxgCommonParams.pdx11RStates[__RState_SOLID_NONE]);
	pdx11DeviceImmContext->OMSetDepthStencilState(g_VxgCommonParams.pdx11DSStates[__DSState_DEPTHENABLE_ALWAYS], 0);
	pdx11DeviceImmContext->VSSetShader(g_VxgCommonParams.pdx11VS_SR_Shaders[__VS_PROXY], NULL, 0);
	pdx11DeviceImmContext->PSSetShader(g_VxgCommonParams.pdx11PS_SR_Shaders[__PS_MERGE_LAYERS_TO_RENDEROUT], NULL, 0);

	pdx11DeviceImmContext->Draw(4, 0);

#pragma region // Copy GPU to CPU
	SVXFrameBuffer* psvxFrameBuffer = (SVXFrameBuffer*)pCIObjectSystemOut->GetFrameBuffer(vxrFrameBufferUsageRENDEROUT, 0);
	vxbyte4* py4Buffer = (vxbyte4*)psvxFrameBuffer->pvBuffer;

	D3D11_MAPPED_SUBRESOURCE mappedResSysFinal;
	pdx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gpuResourceFB_RENDEROUT_SYSTEM_ARCH.vtrPtrs.at(0), (ID3D11Texture2D*)gpuResourcesFB_ARCHs[__FB_TX2D_RENDEROUT].vtrPtrs.at(0));
	pdx11DeviceImmContext->Map((ID3D11Texture2D*)gpuResourceFB_RENDEROUT_SYSTEM_ARCH.vtrPtrs.at(0), 0, D3D11_MAP_READ, NULL, &mappedResSysFinal);

	vxbyte4* py4RenderOuts = (vxbyte4*)mappedResSysFinal.pData;
	for (int i = 0; i < i2SizeScreenCurrent.y; i++)
	{
		for (int j = 0; j < i2SizeScreenCurrent.x; j++)
		{
			// __PS_MERGE_LAYERS_TO_RENDEROUT 에서 INT -> FLOAT4 로 되어 배열된 color 요소가 들어 옴. //
			py4Buffer[i*i2SizeScreenCurrent.x + j].x = (byte)(py4RenderOuts[j].z);	// B
			py4Buffer[i*i2SizeScreenCurrent.x + j].y = (byte)(py4RenderOuts[j].y);	// G
			py4Buffer[i*i2SizeScreenCurrent.x + j].z = (byte)(py4RenderOuts[j].x);	// R
			py4Buffer[i*i2SizeScreenCurrent.x + j].w = (byte)(py4RenderOuts[j].w);	// A
		}
		py4RenderOuts += mappedResSysFinal.RowPitch / 4;	// mappedResSysFinal.RowPitch (byte unit)
	}
	pdx11DeviceImmContext->Unmap((ID3D11Texture2D*)gpuResourceFB_RENDEROUT_SYSTEM_ARCH.vtrPtrs.at(0), 0);

	SVXFrameBuffer* psvxFrameBufferDepth = (SVXFrameBuffer*)pCIObjectSystemOut->GetFrameBuffer(vxrFrameBufferUsageDEPTH, 0);
	float* pfBuffer = (float*)psvxFrameBufferDepth->pvBuffer;
	pdx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gpuResourceFB_DEPTH_SYSTEM_ARCH.vtrPtrs.at(0), (ID3D11Texture2D*)gpuResourcesFB_ARCHs[__FB_TX2D_DEPTH_ZTHICKCS].vtrPtrs.at(0));
	pdx11DeviceImmContext->Map((ID3D11Texture2D*)gpuResourceFB_DEPTH_SYSTEM_ARCH.vtrPtrs.at(0), 0, D3D11_MAP_READ, NULL, &mappedResSysFinal);
	float* pfDeferredContexts = (float*)mappedResSysFinal.pData;
	for (int i = 0; i < i2SizeScreenCurrent.y; i++)
	{
		for (int j = 0; j < i2SizeScreenCurrent.x; j++)
		{
			if (py4Buffer[i*i2SizeScreenCurrent.x + j].w > 0)
				pfBuffer[i*i2SizeScreenCurrent.x + j] = pfDeferredContexts[j];
			else
				pfBuffer[i*i2SizeScreenCurrent.x + j] = FLT_MAX;
		}
		pfDeferredContexts += mappedResSysFinal.RowPitch / 4;	// mappedResSysFinal.RowPitch (byte unit)
	}
	pdx11DeviceImmContext->Unmap((ID3D11Texture2D*)gpuResourceFB_DEPTH_SYSTEM_ARCH.vtrPtrs.at(0), 0);
#pragma endregion
#pragma endregion

	pdx11DeviceImmContext->ClearState();

	ID3D11RenderTargetView* pdxRTVOlds[NUM_MERGE_LAYERS] = { NULL };
	pdxRTVOlds[0] = pdxRTVOld;
	pdx11DeviceImmContext->OMSetRenderTargets(NUM_MERGE_LAYERS, pdxRTVOlds, pdxDSVOld);

	VXSAFE_RELEASE(pdxRTVOld);
	VXSAFE_RELEASE(pdxDSVOld);
	g_mutex_GpuCriticalPath.unlock();
	return true;
}

bool HDx11UploadDefaultVolume(SVXGPUResourceArchive* pstGpuResourceVolumeTexture, 
	SVXGPUResourceArchive* pstGpuResourceVolumeSRV,
	CVXVObjectVolume* pCVolume, wstring strUsageSpecifier, SVXLocalProgress* pstProgress)
{
	if (g_pCGpuManager == NULL)
		return false;
	SVXVolumeDataArchive* pstVolArchive = pCVolume->GetVolumeArchive();
	SVXGPUResourceDESC stGpuVolumeResourceDesc;
	stGpuVolumeResourceDesc.eGpuSdkType = vxgGpuSdkTypeDX11;
	stGpuVolumeResourceDesc.eDataType = pstVolArchive->eVolumeDataType; // vxrDataTypeUNSIGNEDSHORT
	stGpuVolumeResourceDesc.iSizeStride = VXHGetDataTypeSizeByte(stGpuVolumeResourceDesc.eDataType);
	stGpuVolumeResourceDesc.iSourceObjectID = pCVolume->GetObjectID();
	stGpuVolumeResourceDesc.strUsageSpecifier = strUsageSpecifier;
	stGpuVolumeResourceDesc.eResourceType = vxgResourceTypeDX11RESOURCE;
	if (!g_pCGpuManager->GetGpuResourceArchive(&stGpuVolumeResourceDesc, pstGpuResourceVolumeTexture))
	{
		g_pCGpuManager->GenerateGpuResource(&stGpuVolumeResourceDesc, pCVolume, pstGpuResourceVolumeTexture, pstProgress);
	}
	stGpuVolumeResourceDesc.eResourceType = vxgResourceTypeDX11SRV;
	if (!g_pCGpuManager->GetGpuResourceArchive(&stGpuVolumeResourceDesc, pstGpuResourceVolumeSRV))
	{
		g_pCGpuManager->GenerateGpuResource(&stGpuVolumeResourceDesc, pCVolume, pstGpuResourceVolumeSRV, NULL);
	}
	return true;
}