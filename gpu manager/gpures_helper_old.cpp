#include "gpures_helper_old.h"
#include "D3DCompiler.h"
#include <iostream>

using namespace grd_helper_legacy;

namespace grd_helper_legacy
{
	static GpuDX11CommonParametersOld* g_pvmCommonParams_old = NULL;
	static VmGpuManager* g_pCGpuManager = NULL;
}

LPCWSTR g_lpcwstrResourcesVR_CS[NUMSHADERS_VR_CS] = {	// The order should be same as the order of compile resource ones.
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

LPCWSTR g_lpcwstrResourcesMERGE_CS[NUMSHADERS_MERGE_CS] = {
	MAKEINTRESOURCE(IDR_RCDATA20100), // "CS_MERGE_LAYEROUT_TO_RENDEROUT_cs_5_0"
	MAKEINTRESOURCE(IDR_RCDATA20110), // "CS_MERGE_SROUT_AND_LAYEROUT_TO_LAYEROUT_cs_5_0" 
	MAKEINTRESOURCE(IDR_RCDATA20120), // "CS_MERGE_ADV_SROUT_AND_LAYEROUT_TO_LAYEROUT_cs_5_0"
};

LPCWSTR g_lpcwstrResourcesGS[NUMSHADERS_GS] = {
	MAKEINTRESOURCE(IDR_RCDATA30010), // "GS_ThickPoints_gs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA30020), // "GS_ThickLines_gs_4_0" 
	MAKEINTRESOURCE(IDR_RCDATA30030), // "GS_WireLines_gs_4_0" 
};

LPCWSTR g_lpcwstrResourcesSR[NUMSHADERS_SR_PS] = {
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

LPCWSTR g_lpcwstrResourcesPLANESR[NUMSHADERS_PLANE_SR_VS] = {
	MAKEINTRESOURCE(IDR_RCDATA15010), // "SR_PROJ_P_MVB_vs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA15020), // "SR_PROJ_PN_MVB_vs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA15030), // "SR_PROJ_PT_MVB_vs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA15040), // "SR_PROJ_PNT_MVB_vs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA15050), // "SR_PROJ_PNT_DEV_MVB_vs_4_0"
};

LPCWSTR g_lpcwstrResourcesBIASZSR[NUMSHADERS_BIASZ_SR_VS] = {
	MAKEINTRESOURCE(IDR_RCDATA15110), // "SR_PROJ_P_FBIASZ_vs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA15120), // "SR_PROJ_PN_FBIASZ_vs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA15130), // "SR_PROJ_PT_FBIASZ_vs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA15140), // "SR_PROJ_PNT_FBIASZ_vs_4_0"
	MAKEINTRESOURCE(IDR_RCDATA15150), // "SR_PROJ_PNT_DEV_FBIASZ_vs_4_0"
};


HRESULT PresetCompiledShader(ID3D11Device* pdx11Device, HMODULE hModule, LPCWSTR pSrcResource, LPCSTR strShaderProfile, ID3D11DeviceChild** ppdx11Shader/*out*/
	, D3D11_INPUT_ELEMENT_DESC* pInputLayoutDesc = NULL, uint uiNumOfInputElements = 0, ID3D11InputLayout** ppdx11LayoutInputVS = NULL, int gg = 0)
{
	HRSRC hrSrc = FindResource(hModule, pSrcResource, RT_RCDATA);
	HGLOBAL hGlobalByte = LoadResource(hModule, hrSrc);
	LPVOID pdata = LockResource(hGlobalByte);
	ullong ullFileSize = SizeofResource(hModule, hrSrc);

	string _strShaderProfile = strShaderProfile;
	if (_strShaderProfile.compare(0, 2, "cs") == 0)
	{
		if (pdx11Device->CreateComputeShader(pdata, ullFileSize, NULL, (ID3D11ComputeShader**)ppdx11Shader) != S_OK)
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
	else if (_strShaderProfile.compare(0, 2, "vs") == 0)
	{
		if (pdx11Device->CreateVertexShader(pdata, ullFileSize, NULL, (ID3D11VertexShader**)ppdx11Shader) != S_OK)
			goto ERROR_SHADER;

		if (pInputLayoutDesc != NULL && uiNumOfInputElements > 0 && ppdx11LayoutInputVS != NULL)
		{
			if (pdx11Device->CreateInputLayout(pInputLayoutDesc, uiNumOfInputElements, pdata, ullFileSize, ppdx11LayoutInputVS) != S_OK)
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

//HRESULT PresetCompiledShader(ID3D11Device* pdx11Device, HMODULE hModule, LPCWSTR pSrcResource, LPCSTR strShaderProfile, ID3D11DeviceChild** ppdx11Shader/*out*/
//	, D3D11_INPUT_ELEMENT_DESC* pInputLayoutDesc, uint uiNumOfInputElements, ID3D11InputLayout** ppdx11LayoutInputVS)
//{
//	HRSRC hrSrc = FindResource(hModule, pSrcResource, RT_RCDATA);
//	HGLOBAL hGlobalByte = LoadResource(hModule, hrSrc);
//	LPVOID pdata = LockResource(hGlobalByte);
//	ullong ullFileSize = SizeofResource(hModule, hrSrc);
//
//	string _strShaderProfile = strShaderProfile;
//	if (_strShaderProfile.compare(0, 2, "cs") == 0)
//	{
//		if (pdx11Device->CreateComputeShader(pdata, ullFileSize, NULL, (ID3D11ComputeShader**)ppdx11Shader) != S_OK)
//			goto ERROR_SHADER;
//	}
//	else if (_strShaderProfile.compare(0, 2, "ps") == 0)
//	{
//		if (pdx11Device->CreatePixelShader(pdata, ullFileSize, NULL, (ID3D11PixelShader**)ppdx11Shader) != S_OK)
//			goto ERROR_SHADER;
//	}
//	else if (_strShaderProfile.compare(0, 2, "gs") == 0)
//	{
//		if (pdx11Device->CreateGeometryShader(pdata, ullFileSize, NULL, (ID3D11GeometryShader**)ppdx11Shader) != S_OK)
//			goto ERROR_SHADER;
//	}
//	else if (_strShaderProfile.compare(0, 2, "vs") == 0)
//	{
//		if (pdx11Device->CreateVertexShader(pdata, ullFileSize, NULL, (ID3D11VertexShader**)ppdx11Shader) != S_OK)
//			goto ERROR_SHADER;
//
//		if (pInputLayoutDesc != NULL && uiNumOfInputElements > 0 && ppdx11LayoutInputVS != NULL)
//		{
//			if (pdx11Device->CreateInputLayout(pInputLayoutDesc, uiNumOfInputElements, pdata, ullFileSize, ppdx11LayoutInputVS) != S_OK)
//				goto ERROR_SHADER;
//		}
//	}
//	else
//	{
//		goto ERROR_SHADER;
//	}
//
//	return S_OK;
//
//ERROR_SHADER:
//	return E_FAIL;
//}

int grd_helper_legacy::InitializePresettings(VmGpuManager* pCGpuManager, GpuDX11CommonParametersOld* gpu_params)
{
#define __SUPPORTED_GPU 0
#define __ALREADY_CHECKED 1
#define __LOW_SPEC_NOT_SUPPORT_CS_GPU 2
#define __INVALID_GPU -1

	g_pCGpuManager = pCGpuManager;
	g_pvmCommonParams_old = gpu_params;
	if (g_pvmCommonParams_old->bIsInitialized)
	{
		return __ALREADY_CHECKED;
	}
	
	g_pCGpuManager->GetDeviceInformation((void*)(&g_pvmCommonParams_old->pdx11Device), ("DEVICE_POINTER"));
	g_pCGpuManager->GetDeviceInformation((void*)(&g_pvmCommonParams_old->pdx11DeviceImmContext), ("DEVICE_CONTEXT_POINTER"));
	g_pCGpuManager->GetDeviceInformation(&g_pvmCommonParams_old->stDx11Adapter, ("DEVICE_ADAPTER_DESC"));
	g_pCGpuManager->GetDeviceInformation(&g_pvmCommonParams_old->eDx11FeatureLevel, ("FEATURE_LEVEL"));
	//return __SUPPORTED_GPU;

	if (g_pvmCommonParams_old->pdx11Device == NULL || g_pvmCommonParams_old->pdx11DeviceImmContext == NULL)
	{
		gpu_params = g_pvmCommonParams_old;
		return __INVALID_GPU;
	}

	for (int i = 0; i < __DSState_COUNT; i++)
		VMSAFE_RELEASE(g_pvmCommonParams_old->pdx11DSStates[i]);

	for (int i = 0; i < __RState_COUNT; i++)
		VMSAFE_RELEASE(g_pvmCommonParams_old->pdx11RStates[i]);

	for (int i = 0; i < __SState_COUNT; i++)
		VMSAFE_RELEASE(g_pvmCommonParams_old->pdx11SStates[i]);

	HRESULT hr = S_OK;
	// HLSL 에서 대체하는 방법 찾아 보기.
	{
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
		hr |= g_pvmCommonParams_old->pdx11Device->CreateRasterizerState(&descRaster, &g_pvmCommonParams_old->pdx11RStates[__RState_SOLID_CW]);
		descRaster.AntialiasedLineEnable = true;
		hr |= g_pvmCommonParams_old->pdx11Device->CreateRasterizerState(&descRaster, &g_pvmCommonParams_old->pdx11RStates[__RState_ANTIALIASING_SOLID_CW]);
		//descRaster.CullMode = D3D11_CULL_FRONT;
		descRaster.CullMode = D3D11_CULL_BACK;
		descRaster.FrontCounterClockwise = TRUE;
		descRaster.AntialiasedLineEnable = false;
		hr |= g_pvmCommonParams_old->pdx11Device->CreateRasterizerState(&descRaster, &g_pvmCommonParams_old->pdx11RStates[__RState_SOLID_CCW]);
		descRaster.AntialiasedLineEnable = true;
		hr |= g_pvmCommonParams_old->pdx11Device->CreateRasterizerState(&descRaster, &g_pvmCommonParams_old->pdx11RStates[__RState_ANTIALIASING_SOLID_CCW]);
		descRaster.CullMode = D3D11_CULL_NONE;
		descRaster.AntialiasedLineEnable = false;
		hr |= g_pvmCommonParams_old->pdx11Device->CreateRasterizerState(&descRaster, &g_pvmCommonParams_old->pdx11RStates[__RState_SOLID_NONE]);
		descRaster.AntialiasedLineEnable = true;
		hr |= g_pvmCommonParams_old->pdx11Device->CreateRasterizerState(&descRaster, &g_pvmCommonParams_old->pdx11RStates[__RState_ANTIALIASING_SOLID_NONE]);

		descRaster.FillMode = D3D11_FILL_WIREFRAME;
		descRaster.CullMode = D3D11_CULL_BACK;
		descRaster.AntialiasedLineEnable = false;
		hr |= g_pvmCommonParams_old->pdx11Device->CreateRasterizerState(&descRaster, &g_pvmCommonParams_old->pdx11RStates[__RState_WIRE_CW]);
		descRaster.AntialiasedLineEnable = true;
		hr |= g_pvmCommonParams_old->pdx11Device->CreateRasterizerState(&descRaster, &g_pvmCommonParams_old->pdx11RStates[__RState_ANTIALIASING_WIRE_CW]);
		descRaster.CullMode = D3D11_CULL_FRONT;
		descRaster.AntialiasedLineEnable = false;
		hr |= g_pvmCommonParams_old->pdx11Device->CreateRasterizerState(&descRaster, &g_pvmCommonParams_old->pdx11RStates[__RState_WIRE_CCW]);
		descRaster.AntialiasedLineEnable = true;
		hr |= g_pvmCommonParams_old->pdx11Device->CreateRasterizerState(&descRaster, &g_pvmCommonParams_old->pdx11RStates[__RState_ANTIALIASING_WIRE_CCW]);
		descRaster.CullMode = D3D11_CULL_NONE;
		descRaster.AntialiasedLineEnable = false;
		hr |= g_pvmCommonParams_old->pdx11Device->CreateRasterizerState(&descRaster, &g_pvmCommonParams_old->pdx11RStates[__RState_WIRE_NONE]);
		descRaster.AntialiasedLineEnable = true;
		hr |= g_pvmCommonParams_old->pdx11Device->CreateRasterizerState(&descRaster, &g_pvmCommonParams_old->pdx11RStates[__RState_ANTIALIASING_WIRE_NONE]);
	}
	{
		D3D11_DEPTH_STENCIL_DESC descDepthStencil;
		ZeroMemory(&descDepthStencil, sizeof(D3D11_DEPTH_STENCIL_DESC));
		descDepthStencil.DepthEnable = TRUE;
		descDepthStencil.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		descDepthStencil.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		descDepthStencil.StencilEnable = FALSE;
		hr |= g_pvmCommonParams_old->pdx11Device->CreateDepthStencilState(&descDepthStencil, &g_pvmCommonParams_old->pdx11DSStates[__DSState_DEPTHENABLE_LESSEQUAL]);
		descDepthStencil.DepthFunc = D3D11_COMPARISON_ALWAYS;
		hr |= g_pvmCommonParams_old->pdx11Device->CreateDepthStencilState(&descDepthStencil, &g_pvmCommonParams_old->pdx11DSStates[__DSState_DEPTHENABLE_ALWAYS]);
		descDepthStencil.DepthFunc = D3D11_COMPARISON_GREATER;
		hr |= g_pvmCommonParams_old->pdx11Device->CreateDepthStencilState(&descDepthStencil, &g_pvmCommonParams_old->pdx11DSStates[__DSState_DEPTHENABLE_GREATER]);
		descDepthStencil.DepthFunc = D3D11_COMPARISON_LESS;
		hr |= g_pvmCommonParams_old->pdx11Device->CreateDepthStencilState(&descDepthStencil, &g_pvmCommonParams_old->pdx11DSStates[__DSState_DEPTHENABLE_LESS]);
	}
	{
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
		hr |= g_pvmCommonParams_old->pdx11Device->CreateSamplerState(&descSampler, &g_pvmCommonParams_old->pdx11SStates[__SState_POINT_CLAMP]);
		descSampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;	// NEAREST by ROUND
		hr |= g_pvmCommonParams_old->pdx11Device->CreateSamplerState(&descSampler, &g_pvmCommonParams_old->pdx11SStates[__SState_LINEAR_CLAMP]);
		descSampler.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
		descSampler.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
		descSampler.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
		hr |= g_pvmCommonParams_old->pdx11Device->CreateSamplerState(&descSampler, &g_pvmCommonParams_old->pdx11SStates[__SState_LINEAR_ZEROBORDER]);
		descSampler.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;	// NEAREST by ROUND
		hr |= g_pvmCommonParams_old->pdx11Device->CreateSamplerState(&descSampler, &g_pvmCommonParams_old->pdx11SStates[__SState_POINT_ZEROBORDER]);
	}
	{
		uint size_params[__CB_COUNT] = {
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
			sizeof(XMFLOAT4), // 12
			sizeof(CB_VrOtf1D), // 13
			sizeof(CB_SrProxy), // 14

			sizeof(CB_VrOtf1D) * 3, // 15
			sizeof(CB_VrVolumeObject) * 3, // 16
			sizeof(CB_VrModulation) * 3, // 17
		};

		int num_cbs = sizeof(size_params) / sizeof(uint);
		for (int i = 0; i < num_cbs; i++)
		{
			VMSAFE_RELEASE(g_pvmCommonParams_old->pdx11BufConstParameters[i]);
		}

		D3D11_BUFFER_DESC descCB;
		for (int i = 0; i < num_cbs; i++)
		{
			descCB.ByteWidth = size_params[i];
			descCB.Usage = D3D11_USAGE_DYNAMIC;
			descCB.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			descCB.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			descCB.MiscFlags = NULL;
			descCB.StructureByteStride = size_params[i];
			hr |= g_pvmCommonParams_old->pdx11Device->CreateBuffer(&descCB, NULL, &g_pvmCommonParams_old->pdx11BufConstParameters[i]);
		}
	}
	{
		// Vertex Buffer //
		VMSAFE_RELEASE(g_pvmCommonParams_old->pdx11BufProxyVertice);
		D3D11_BUFFER_DESC descBufProxyVertex;
		ZeroMemory(&descBufProxyVertex, sizeof(D3D11_BUFFER_DESC));
		descBufProxyVertex.ByteWidth = sizeof(XMFLOAT3) * 4;
		descBufProxyVertex.Usage = D3D11_USAGE_DYNAMIC;
		descBufProxyVertex.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		descBufProxyVertex.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		descBufProxyVertex.MiscFlags = NULL;
		descBufProxyVertex.StructureByteStride = sizeof(XMFLOAT3);
		if (g_pvmCommonParams_old->pdx11Device->CreateBuffer(&descBufProxyVertex, NULL, &g_pvmCommonParams_old->pdx11BufProxyVertice) != S_OK)
		{
			gpu_params = g_pvmCommonParams_old;
			return __INVALID_GPU;
		}

		D3D11_MAPPED_SUBRESOURCE mappedProxyRes;
		g_pvmCommonParams_old->pdx11DeviceImmContext->Map(g_pvmCommonParams_old->pdx11BufProxyVertice, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedProxyRes);
		XMFLOAT3* pv3PosProxy = (XMFLOAT3*)mappedProxyRes.pData;
		pv3PosProxy[0] = XMFLOAT3(-0.5f, 0.5f, 0);
		pv3PosProxy[1] = XMFLOAT3(0.5f, 0.5f, 0);
		pv3PosProxy[2] = XMFLOAT3(-0.5f, -0.5f, 0);
		pv3PosProxy[3] = XMFLOAT3(0.5f, -0.5f, 0);
		g_pvmCommonParams_old->pdx11DeviceImmContext->Unmap(g_pvmCommonParams_old->pdx11BufProxyVertice, NULL);
	}
	{
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

		if (PresetCompiledShader(g_pvmCommonParams_old->pdx11Device, hModule, MAKEINTRESOURCE(IDR_RCDATA10001), "vs_4_0", (ID3D11DeviceChild**)&g_pvmCommonParams_old->pdx11VS_SR_Shaders[__VS_P]
			, lotypeInputPos, 1, &g_pvmCommonParams_old->pdx11IinputLayouts[0]) != S_OK)
		{
			goto ERROR_PRESETTING;
		}
		if (PresetCompiledShader(g_pvmCommonParams_old->pdx11Device, hModule, MAKEINTRESOURCE(IDR_RCDATA10002), "vs_4_0", (ID3D11DeviceChild**)&g_pvmCommonParams_old->pdx11VS_SR_Shaders[__VS_PN]
			, lotypeInputPosNor, 2, &g_pvmCommonParams_old->pdx11IinputLayouts[1]) != S_OK)
		{
			goto ERROR_PRESETTING;
		}
		if (PresetCompiledShader(g_pvmCommonParams_old->pdx11Device, hModule, MAKEINTRESOURCE(IDR_RCDATA10003), "vs_4_0", (ID3D11DeviceChild**)&g_pvmCommonParams_old->pdx11VS_SR_Shaders[__VS_PT]
			, lotypeInputPosTex, 2, &g_pvmCommonParams_old->pdx11IinputLayouts[2]) != S_OK)
		{
			goto ERROR_PRESETTING;
		}
		if (PresetCompiledShader(g_pvmCommonParams_old->pdx11Device, hModule, MAKEINTRESOURCE(IDR_RCDATA10004), "vs_4_0", (ID3D11DeviceChild**)&g_pvmCommonParams_old->pdx11VS_SR_Shaders[__VS_PNT]
			, lotypeInputPosNorTex, 3, &g_pvmCommonParams_old->pdx11IinputLayouts[3]) != S_OK)
		{
			goto ERROR_PRESETTING;
		}
		if (PresetCompiledShader(g_pvmCommonParams_old->pdx11Device, hModule, MAKEINTRESOURCE(IDR_RCDATA10005), "vs_4_0", (ID3D11DeviceChild**)&g_pvmCommonParams_old->pdx11VS_SR_Shaders[__VS_PTTT]
			, lotypeInputPosTTTex, 4, &g_pvmCommonParams_old->pdx11IinputLayouts[4]) != S_OK)
		{
			goto ERROR_PRESETTING;
		}
		if (PresetCompiledShader(g_pvmCommonParams_old->pdx11Device, hModule, MAKEINTRESOURCE(IDR_RCDATA10006), "vs_4_0", (ID3D11DeviceChild**)&g_pvmCommonParams_old->pdx11VS_SR_Shaders[__VS_PNT_DEV]
			, lotypeInputPosNorTex, 3, NULL) != S_OK)
		{
			goto ERROR_PRESETTING;
		}
		if (PresetCompiledShader(g_pvmCommonParams_old->pdx11Device, hModule, MAKEINTRESOURCE(IDR_RCDATA10007), "vs_4_0", (ID3D11DeviceChild**)&g_pvmCommonParams_old->pdx11VS_SR_Shaders[__VS_PROXY]
			, NULL, 1, NULL) != S_OK)
		{
			goto ERROR_PRESETTING;
		}

		for (int i = 0; i < NUMSHADERS_GS; i++)
		{
			if (PresetCompiledShader(g_pvmCommonParams_old->pdx11Device, hModule, g_lpcwstrResourcesGS[i], "gs_4_0", (ID3D11DeviceChild**)&g_pvmCommonParams_old->pdx11GS_Shaders[i]) != S_OK)
			{
				printf("\n*****************\n Invalid DX10 or higher NUMSHADERS_GS\n*****************\n");
				goto ERROR_PRESETTING;
			}
		}

		for (int i = 0; i < NUMSHADERS_SR_PS; i++)
		{
			if (PresetCompiledShader(g_pvmCommonParams_old->pdx11Device, hModule, g_lpcwstrResourcesSR[i], "ps_4_0", (ID3D11DeviceChild**)&g_pvmCommonParams_old->pdx11PS_SR_Shaders[i]) != S_OK)
			{
				printf("\n*****************\n Invalid DX10 or higher NUMSHADERS_SR_PS\n*****************\n");
				goto ERROR_PRESETTING;
			}
		}
		for (int i = 0; i < NUMSHADERS_PLANE_SR_VS; i++)
		{
			if (PresetCompiledShader(g_pvmCommonParams_old->pdx11Device, hModule, g_lpcwstrResourcesPLANESR[i], "vs_4_0", (ID3D11DeviceChild**)&g_pvmCommonParams_old->pdx11VS_PLANE_SR_Shaders[i]) != S_OK)
			{
				printf("\n*****************\n Invalid DX10 or higher NUMSHADERS_PLANE_SR_VS\n*****************\n");
				goto ERROR_PRESETTING;
			}
		}
		for (int i = 0; i < NUMSHADERS_BIASZ_SR_VS; i++)
		{
			if (PresetCompiledShader(g_pvmCommonParams_old->pdx11Device, hModule, g_lpcwstrResourcesBIASZSR[i], "vs_4_0", (ID3D11DeviceChild**)&g_pvmCommonParams_old->pdx11VS_FBIAS_SR_Shaders[i]) != S_OK)
			{
				printf("\n*****************\n Invalid DX10 or higher NUMSHADERS_PLANE_SR_VS\n*****************\n");
				goto ERROR_PRESETTING;
			}
		}
#pragma endregion

#pragma region // VR
		for (int i = 0; i < NUMSHADERS_VR_CS; i++)
		{
			if (PresetCompiledShader(g_pvmCommonParams_old->pdx11Device, hModule, g_lpcwstrResourcesVR_CS[i], "cs_5_0", (ID3D11DeviceChild**)&g_pvmCommonParams_old->pdx11CS_VR_Shaders[i]) != S_OK)
			{
				//GMERRORMESSAGE("Shader Setting Error! - VR");
				printf("\n*****************\n Invalid Pixel Shader\n*****************\n");
				goto ERROR_PRESETTING;
			}
		}
#pragma endregion 

#pragma region // Merge
		for (int i = 0; i < NUMSHADERS_MERGE_CS; i++)
		{
			if (PresetCompiledShader(g_pvmCommonParams_old->pdx11Device, hModule, g_lpcwstrResourcesMERGE_CS[i], "cs_5_0", (ID3D11DeviceChild**)&g_pvmCommonParams_old->pdx11CS_MergeTextures[i]) != S_OK)
			{
				GMERRORMESSAGE("Shader Setting Error! - MERGE");
				//::MessageBox(NULL, ("MERGE Shader Setting Error!"), NULL, MB_OK);
				goto ERROR_PRESETTING;
			}
		}
#pragma endregion

		if (PresetCompiledShader(g_pvmCommonParams_old->pdx11Device, hModule, MAKEINTRESOURCE(IDR_RCDATA20500), "cs_5_0", (ID3D11DeviceChild**)&g_pvmCommonParams_old->pdx11CS_Outline) != S_OK)
		{
			GMERRORMESSAGE("Shader Setting Error! - Outline");
			//::MessageBox(NULL, ("MERGE Shader Setting Error!"), NULL, MB_OK);
			goto ERROR_PRESETTING;
		}

	}

	g_pvmCommonParams_old->bIsInitialized = true;
	gpu_params = g_pvmCommonParams_old;

	if (hr != S_OK)
		return __INVALID_GPU;

	if (g_pvmCommonParams_old->eDx11FeatureLevel == 0x9300 || g_pvmCommonParams_old->eDx11FeatureLevel == 0xA000)
		return __LOW_SPEC_NOT_SUPPORT_CS_GPU;

	return __SUPPORTED_GPU;

ERROR_PRESETTING:
	DeinitializePresettings();
	gpu_params = g_pvmCommonParams_old;
	return __INVALID_GPU;
}

void grd_helper_legacy::DeinitializePresettings()
{
	if (g_pvmCommonParams_old == NULL) return;
	GpuDX11CommonParametersOld* pParam = g_pvmCommonParams_old;
	// Initialized once //

	// Deallocate All Shared Resources //
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
	for (int i = 0; i < NUMSHADERS_MERGE_CS; i++)
		VMSAFE_RELEASE(pParam->pdx11CS_MergeTextures[i]);
	for (int i = 0; i < NUMINPUTLAYOUTS; i++)
		VMSAFE_RELEASE(pParam->pdx11IinputLayouts[i]);
	for (int i = 0; i < NUMSHADERS_SR_VS; i++)
		VMSAFE_RELEASE(pParam->pdx11VS_SR_Shaders[i]);

	VMSAFE_RELEASE(pParam->pdx11CS_Outline);

	VMSAFE_RELEASE(pParam->pdx11BufProxyVertice);

	for (int i = 0; i < __DSState_COUNT; i++)
		VMSAFE_RELEASE(pParam->pdx11DSStates[i]);

	for (int i = 0; i < __RState_COUNT; i++)
		VMSAFE_RELEASE(pParam->pdx11RStates[i]);

	for (int i = 0; i < __SState_COUNT; i++)
		VMSAFE_RELEASE(pParam->pdx11SStates[i]);

	for (int i = 0; i < __CB_COUNT; i++)
		VMSAFE_RELEASE(pParam->pdx11BufConstParameters[i]);

	if (pParam->pdx11DeviceImmContext)
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

extern int __UpdateBlocks(GpuRes& gres, const VmVObjectVolume* vobj, const string& vmode, LocalProgress* progress);
//{
//	gres.vm_src_id = vobj->GetObjectID();
//	gres.res_name = string("VBLOCKS_MODE_") + vmode;
//
//	if (g_pCGpuManager->UpdateGpuResource(gres))
//		return 0;
//
//	gres.rtype = RTYPE_TEXTURE3D;
//	gres.options["USAGE"] = D3D11_USAGE_DYNAMIC;
//	gres.options["CPU_ACCESS_FLAG"] = D3D11_CPU_ACCESS_WRITE;
//	gres.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE;
//
//	VolumeData* vol_data = ((VmVObjectVolume*)vobj)->GetVolumeData();
//	const int blk_level = 1;	// 0 : High Resolution, 1 : Low Resolution
//	VolumeBlocks* volblk = ((VmVObjectVolume*)vobj)->GetVolumeBlock(blk_level);
//
//	gres.res_dvalues["WIDTH"] = (double)volblk->blk_vol_size.x;
//	gres.res_dvalues["HEIGHT"] = (double)volblk->blk_vol_size.y;
//	gres.res_dvalues["DEPTH"] = (double)volblk->blk_vol_size.z;
//
//	uint num_blk_units = volblk->blk_vol_size.x * volblk->blk_vol_size.y * volblk->blk_vol_size.z;
//	gres.options["FORMAT"] = (vmode == "OTFTAG") && num_blk_units >= 65535 ? DXGI_FORMAT_R32_FLOAT : DXGI_FORMAT_R16_UNORM;
//
//	g_pCGpuManager->GenerateGpuResource(gres);
//
//	return 1;
//}

bool grd_helper_legacy::UpdateOtfBlocks(GpuRes& gres, const VmVObjectVolume* vobj, const VmTObject	* tobj,
	const bool update_blks, const int sculpt_value, LocalProgress* progress)
{
	map<int, VmTObject*> mapTObjects;
	mapTObjects.insert(pair<int, VmTObject*>(tobj->GetObjectID(), (VmTObject*)tobj));
	return UpdateOtfBlocks(gres, vobj, NULL, mapTObjects, tobj->GetObjectID(), NULL, 0, update_blks, false, sculpt_value, progress);
}

bool grd_helper_legacy::UpdateOtfBlocks(GpuRes& gres, const VmVObjectVolume* main_vobj, const VmVObjectVolume* mask_vobj,
	const map<int, VmTObject*>& mapTObjects, const int main_tmap_id, 
	const double* mask_tmap_ids, const int num_mask_tmap_ids,
	const bool update_blks, const bool use_mask_otf, const int sculpt_value, LocalProgress* progress)
{
	int is_new = __UpdateBlocks(gres, main_vobj, "OTFTAG", progress);
	if (!update_blks && is_new == 0) return true;

	auto it = mapTObjects.find(main_tmap_id);
	if (it == mapTObjects.end()) return false;
	VmTObject* main_tmap = (VmTObject*)it->second;
	vmdouble2 otf_Mm_range = vmdouble2(DBL_MAX, -DBL_MAX);
	TMapData* tmap_data = (main_tmap)->GetTMapData();
	otf_Mm_range.x = tmap_data->valid_min_idx.x;
	otf_Mm_range.y = tmap_data->valid_max_idx.x;

	const int blk_level = 1;	// 0 : High Resolution, 1 : Low Resolution
	VolumeBlocks* volblk = ((VmVObjectVolume*)main_vobj)->GetVolumeBlock(blk_level);
	// MAIN UPDATE!
	((VmVObjectVolume*)main_vobj)->UpdateTagBlocks(main_tmap->GetObjectID(), 0, otf_Mm_range, NULL);
	((VmVObjectVolume*)main_vobj)->UpdateTagBlocks(main_tmap->GetObjectID(), 1, otf_Mm_range, NULL);
	byte* tag_blks = volblk->GetTaggedActivatedBlocks(main_tmap->GetObjectID()); // set by OTF values

	if (use_mask_otf && mask_tmap_ids && num_mask_tmap_ids > 0 && mask_vobj) // Only Cares for Main Block of iLevelBlock
	{
		VolumeBlocks* mask_volblk = ((VmVObjectVolume*)mask_vobj)->GetVolumeBlock(blk_level);

		vmint3 blk_vol_size = mask_volblk->blk_vol_size;
		vmint3 blk_bnd_size = mask_volblk->blk_bnd_size;
		int num_blk_units_x = blk_vol_size.x + blk_bnd_size.x * 2;
		int num_blk_units_y = blk_vol_size.y + blk_bnd_size.y * 2;
		int num_blk_units_z = blk_vol_size.z + blk_bnd_size.z * 2;
		int num_blks = num_blk_units_x * num_blk_units_y * num_blk_units_z;

		vmbyte2* mask_mM_blk_values = (vmbyte2*)mask_volblk->mM_blks;
		int num_mask_otfs = (int)mapTObjects.size();
		for (int j = 1; j < num_mask_otfs; j++)
		{
			int mask_tmap_id = (int)mask_tmap_ids[j];
			auto it_mask_tmap = mapTObjects.find(mask_tmap_id);
			if (it_mask_tmap == mapTObjects.end())
			{
				VMERRORMESSAGE("GPU DVR! - INVALID TOBJECT ID - MASK TF CHECK");
				return false;
			}

			VmTObject* pCTObjectMask = it_mask_tmap->second;

			TMapData* pstTfArchiveMask = pCTObjectMask->GetTMapData();
			ushort usMinOtf = (ushort)min(max(pstTfArchiveMask->valid_min_idx.x, 0), 65535);
			ushort usMaxOtf = (ushort)max(min(pstTfArchiveMask->valid_max_idx.x, 65535), 0);

			vmushort2* pus2MinMaxMainVolBlocks = (vmushort2*)volblk->mM_blks;

			for (int iBlkIndex = 0; iBlkIndex < num_blks; iBlkIndex++)
			{
				//if (iSculptValue == 0 || iSculptValue > iSculptIndex)
				vmbyte2 y2MinMaxBlockValue = mask_mM_blk_values[iBlkIndex];
				if (y2MinMaxBlockValue.y >= j)
				{
					vmushort2 us2MinMaxVolBlockValue = pus2MinMaxMainVolBlocks[iBlkIndex];
					if (us2MinMaxVolBlockValue.y < usMinOtf || us2MinMaxVolBlockValue.x > usMaxOtf)
						tag_blks[iBlkIndex] = 0;
					else
						tag_blks[iBlkIndex] = 1;
				}
			}
		}
	}

	if (sculpt_value > 0 && mask_vobj)
	{
		VolumeBlocks* sculpt_volblk = ((VmVObjectVolume*)mask_vobj)->GetVolumeBlock(blk_level);
		if (sculpt_volblk == NULL)
			VMERRORMESSAGE("IMPOSSIBLE SCULPT MASK BLOCK!!");

		vmbyte2* sculpt_mM_blk_values = (vmbyte2*)sculpt_volblk->mM_blks;

		vmint3 blk_vol_size = sculpt_volblk->blk_vol_size;
		vmint3 blk_bnd_size = sculpt_volblk->blk_bnd_size;
		int num_blk_units_x = blk_vol_size.x + blk_bnd_size.x * 2;
		int num_blk_units_y = blk_vol_size.y + blk_bnd_size.y * 2;
		int num_blk_units_z = blk_vol_size.z + blk_bnd_size.z * 2;
		int num_blks = num_blk_units_x * num_blk_units_y * num_blk_units_z;

		for (int iBlkIndex = 0; iBlkIndex < num_blks; iBlkIndex++)
		{
			vmbyte2 mM_sculpt_value = sculpt_mM_blk_values[iBlkIndex];
			if (mM_sculpt_value.x != 0 && mM_sculpt_value.y <= sculpt_value)
				tag_blks[iBlkIndex] = 0;
		}
	}

	ID3D11Texture3D* pdx11tx3d_blkmap = (ID3D11Texture3D*)gres.alloc_res_ptrs[DTYPE_RES];
	D3D11_MAPPED_SUBRESOURCE d11MappedRes;
	HRESULT hr = g_pvmCommonParams_old->pdx11DeviceImmContext->Map(pdx11tx3d_blkmap, 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes);

	vmint3 blk_bnd_size = volblk->blk_bnd_size;
	vmint3 blk_sample_width = volblk->blk_vol_size + blk_bnd_size * 2;
	int blk_sample_slice = blk_sample_width.x * blk_sample_width.y;

	uint _format = gres.options["FORMAT"];
	uint count = 0;
	uint count_id = 0;
	if (progress)
		*progress->progress_ptr = (progress->start);

	memset(d11MappedRes.pData, 0, d11MappedRes.DepthPitch * volblk->blk_vol_size.z);

	for (int z = 0; z < (int)volblk->blk_vol_size.z; z++)
	{
		if (progress)
			*progress->progress_ptr = (progress->start + progress->range * z / volblk->blk_vol_size.z);
		for (int y = 0; y < (int)volblk->blk_vol_size.y; y++)
			for (int x = 0; x < (int)volblk->blk_vol_size.x; x++)
			{
				int addr_sample_cpu = (x + blk_bnd_size.x)
					+ (y + blk_bnd_size.y) * blk_sample_width.x + (z + blk_bnd_size.z) * blk_sample_slice;
				if (tag_blks[addr_sample_cpu] != 0)
				{
					if (_format == (int)DXGI_FORMAT_R32_FLOAT)
					{
						((float*)d11MappedRes.pData)[y*d11MappedRes.RowPitch / 4 + z * d11MappedRes.DepthPitch / 4 + x] = (float)(++count);
						//((uint*)d11MappedRes.pData)[count_id] = ++count;
					}
					else // DXGI_FORMAT_R16_UNORM
					{
						((ushort*)d11MappedRes.pData)[y*d11MappedRes.RowPitch / 2 + z * d11MappedRes.DepthPitch / 2 + x] = (ushort)(++count);
						//((ushort*)d11MappedRes.pData)[count_id] = (ushort)(++count);
					}
				}
				count_id++;
			}
	}/**/

	//{
	//	int iCountID = 0;
	//	uint uiCount = 0;
	//	uint uiNumBlocks = (uint)volblk->blk_vol_size.x*(uint)volblk->blk_vol_size.y*(uint)volblk->blk_vol_size.z;
	//
	//	vmint3 i3BlockExBoundaryNum = volblk->blk_bnd_size;
	//	vmint3 i3BlockNumSampleSize = volblk->blk_vol_size + i3BlockExBoundaryNum * 2;
	//	int iBlockNumSampleSizeXY = i3BlockNumSampleSize.x * i3BlockNumSampleSize.y;
	//	int iBlockNumXY = volblk->blk_vol_size.x * volblk->blk_vol_size.y;
	//
	//	ushort *pusBlocksMap = new ushort[uiNumBlocks];	// NEW
	//	memset(pusBlocksMap, 0, sizeof(ushort)*uiNumBlocks);
	//	for (int iZ = 0; iZ < volblk->blk_vol_size.z; iZ++)
	//	{
	//		for (int iY = 0; iY < volblk->blk_vol_size.y; iY++)
	//			for (int iX = 0; iX < volblk->blk_vol_size.x; iX++)
	//			{
	//				int iAddrBlockCpu = (iX + i3BlockExBoundaryNum.x)
	//					+ (iY + i3BlockExBoundaryNum.y) * i3BlockNumSampleSize.x + (iZ + i3BlockExBoundaryNum.z) * iBlockNumSampleSizeXY;
	//				//if (pyTaggedActivatedBlocks[iAddrBlockCpu] != 0)
	//				{
	//					uiCount++;
	//					pusBlocksMap[iCountID] = uiCount;
	//				}
	//				iCountID++;
	//			}
	//	}
	//
	//	ushort* pusBlkMap = (ushort*)d11MappedRes.pData;
	//	for (int i = 0; i < (int)volblk->blk_vol_size.z; i++)
	//		for (int j = 0; j < (int)volblk->blk_vol_size.y; j++)
	//		{
	//			memcpy(&pusBlkMap[j*d11MappedRes.RowPitch / 2 + i * d11MappedRes.DepthPitch / 2], &pusBlocksMap[j*volblk->blk_vol_size.x + i * volblk->blk_vol_size.x*volblk->blk_vol_size.y]
	//				, sizeof(ushort) * volblk->blk_vol_size.x);
	//		}
	//	VMSAFE_DELETEARRAY(pusBlocksMap);
	//}

	g_pvmCommonParams_old->pdx11DeviceImmContext->Unmap(pdx11tx3d_blkmap, 0);
	if (progress)
		*progress->progress_ptr = (progress->start + progress->range);
	return true;
}

bool grd_helper_legacy::UpdateMinMaxBlocks(GpuRes& gres_min, GpuRes& gres_max, const VmVObjectVolume* vobj, LocalProgress* progress)
{
	int is_new1 = __UpdateBlocks(gres_min, vobj, "VMIN", progress);
	int is_new2 = __UpdateBlocks(gres_max, vobj, "VMAX", progress);
	if (is_new1 == 0 && is_new2 == 0) return true;

	const VolumeData* vol_data = ((VmVObjectVolume*)vobj)->GetVolumeData();
	const int blk_level = 1;	// 0 : High Resolution, 1 : Low Resolution
	VolumeBlocks* volblk = ((VmVObjectVolume*)vobj)->GetVolumeBlock(blk_level);

	if (progress)
		*progress->progress_ptr = (progress->start);

	vmushort2* mM_blk_values = (vmushort2*)volblk->mM_blks;
	vmint3 blk_bnd_size = volblk->blk_bnd_size;
	vmint3 blk_sample_width = volblk->blk_vol_size + blk_bnd_size * 2;
	int blk_sample_slice = blk_sample_width.x * blk_sample_width.y;

	ID3D11Texture3D* pdx11tx3d_min_blk = (ID3D11Texture3D*)gres_min.alloc_res_ptrs[DTYPE_RES];
	ID3D11Texture3D* pdx11tx3d_max_blk = (ID3D11Texture3D*)gres_max.alloc_res_ptrs[DTYPE_RES];
	D3D11_MAPPED_SUBRESOURCE d11MappedRes_Min, d11MappedRes_Max;
	HRESULT hr = g_pvmCommonParams_old->pdx11DeviceImmContext->Map(pdx11tx3d_min_blk, 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes_Min);
	hr |= g_pvmCommonParams_old->pdx11DeviceImmContext->Map(pdx11tx3d_max_blk, 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes_Max);

	ushort* min_data = (ushort*)d11MappedRes_Min.pData;
	ushort* max_data = (ushort*)d11MappedRes_Max.pData;
	uint count = 0;
	for (int z = 0; z < volblk->blk_vol_size.z; z++)
	{
		if (progress)
			*progress->progress_ptr = (progress->start + progress->range*z / volblk->blk_vol_size.z);
		for (int y = 0; y < volblk->blk_vol_size.y; y++)
			for (int x = 0; x < volblk->blk_vol_size.x; x++)
			{
				vmushort2 mM_v = mM_blk_values[(x + blk_bnd_size.x)
					+ (y + blk_bnd_size.y) * blk_sample_width.x + (z + blk_bnd_size.z) * blk_sample_slice];

				int iAddrBlockInGpu = x + y * d11MappedRes_Max.RowPitch / 2 + z * d11MappedRes_Max.DepthPitch / 2;
				min_data[iAddrBlockInGpu] = mM_v.x;
				max_data[iAddrBlockInGpu] = mM_v.y;
			}
	}
	g_pvmCommonParams_old->pdx11DeviceImmContext->Unmap(pdx11tx3d_min_blk, 0);
	g_pvmCommonParams_old->pdx11DeviceImmContext->Unmap(pdx11tx3d_max_blk, 0);
	if (progress)
		*progress->progress_ptr = (progress->start + progress->range);
	return true;
}

template <typename GPUTYPE, typename CPUTYPE> bool __FillVolumeValues(CPUTYPE* gpu_data, const GPUTYPE** cpu_data, 
	const bool is_downscaled, const bool use_nearest_max, const int valueoffset,
	const vmint3& cpu_size, const vmint3& cpu_resize, const vmint3& cpu_bnd_size, const vmint2& gpu_row_depth_pitch, LocalProgress* progress)
{
	int cpu_sample_width = cpu_size.x + cpu_bnd_size.x * 2;

	if (progress)
		progress->Init();
	for (int z = 0; z < (int)cpu_resize.z; z++)
	{
		// Main Chunk
		if (progress)
			progress->SetProgress(z, cpu_resize.z);

		float fPosZ = 0.5f;
		if (cpu_resize.z > 1)
			fPosZ = (float)z / (float)(cpu_resize.z - 1) * (float)(cpu_size.z - 1);

		for (int y = 0; y < (int)cpu_resize.y; y++)
		{
			float fPosY = 0.5f;
			if (cpu_resize.y > 1)
				fPosY = (float)y / (float)(cpu_resize.y - 1) * (float)(cpu_size.y - 1);

			for (int x = 0; x < (int)cpu_resize.x; x++)
			{
				CPUTYPE tVoxelValue = 0;
				if (is_downscaled)
				{
					float fPosX = 0.5f;
					if (cpu_resize.x > 1)
						fPosX = (float)x / (float)(cpu_resize.x - 1) * (float)(cpu_size.x - 1);

					vmint3 i3PosSampleVS = vmint3((int)fPosX, (int)fPosY, (int)fPosZ);

					int iMinMaxAddrX = min(max(i3PosSampleVS.x, 0), cpu_size.x - 1) + cpu_bnd_size.x;
					int iMinMaxAddrNextX = min(max(i3PosSampleVS.x + 1, 0), cpu_size.x - 1) + cpu_bnd_size.x;
					int iMinMaxAddrY = (min(max(i3PosSampleVS.y, 0), cpu_size.y - 1) + cpu_bnd_size.y) * cpu_sample_width;
					int iMinMaxAddrNextY = (min(max(i3PosSampleVS.y + 1, 0), cpu_size.y - 1) + cpu_bnd_size.y) * cpu_sample_width;

					int iSampleAddr0 = iMinMaxAddrX + iMinMaxAddrY;
					int iSampleAddr1 = iMinMaxAddrNextX + iMinMaxAddrY;
					int iSampleAddr2 = iMinMaxAddrX + iMinMaxAddrNextY;
					int iSampleAddr3 = iMinMaxAddrNextX + iMinMaxAddrNextY;
					int iSampleAddrZ0 = i3PosSampleVS.z + cpu_bnd_size.z;
					int iSampleAddrZ1 = i3PosSampleVS.z + cpu_bnd_size.z + 1;

					if (i3PosSampleVS.z < 0)
						iSampleAddrZ0 = iSampleAddrZ1;
					else if (i3PosSampleVS.z >= cpu_size.z - 1)
						iSampleAddrZ1 = iSampleAddrZ0;

					CPUTYPE tSampleValues[8];
					tSampleValues[0] = cpu_data[iSampleAddrZ0][iSampleAddr0];
					tSampleValues[1] = cpu_data[iSampleAddrZ0][iSampleAddr1];
					tSampleValues[2] = cpu_data[iSampleAddrZ0][iSampleAddr2];
					tSampleValues[3] = cpu_data[iSampleAddrZ0][iSampleAddr3];
					tSampleValues[4] = cpu_data[iSampleAddrZ1][iSampleAddr0];
					tSampleValues[5] = cpu_data[iSampleAddrZ1][iSampleAddr1];
					tSampleValues[6] = cpu_data[iSampleAddrZ1][iSampleAddr2];
					tSampleValues[7] = cpu_data[iSampleAddrZ1][iSampleAddr3];
					float fSampleTrilinear = 0;

					if (!use_nearest_max)
					{
						vmfloat3 f3InterpolateRatio;
						f3InterpolateRatio.x = fPosX - i3PosSampleVS.x;
						f3InterpolateRatio.y = fPosY - i3PosSampleVS.y;
						f3InterpolateRatio.z = fPosZ - i3PosSampleVS.z;

						float fInterpolateWeights[8];
						fInterpolateWeights[0] = (1.f - f3InterpolateRatio.z)*(1.f - f3InterpolateRatio.y)*(1.f - f3InterpolateRatio.x);
						fInterpolateWeights[1] = (1.f - f3InterpolateRatio.z)*(1.f - f3InterpolateRatio.y)*f3InterpolateRatio.x;
						fInterpolateWeights[2] = (1.f - f3InterpolateRatio.z)*f3InterpolateRatio.y*(1.f - f3InterpolateRatio.x);
						fInterpolateWeights[3] = (1.f - f3InterpolateRatio.z)*f3InterpolateRatio.y*f3InterpolateRatio.x;
						fInterpolateWeights[4] = f3InterpolateRatio.z*(1.f - f3InterpolateRatio.y)*(1.f - f3InterpolateRatio.x);
						fInterpolateWeights[5] = f3InterpolateRatio.z*(1.f - f3InterpolateRatio.y)*f3InterpolateRatio.x;
						fInterpolateWeights[6] = f3InterpolateRatio.z*f3InterpolateRatio.y*(1.f - f3InterpolateRatio.x);
						fInterpolateWeights[7] = f3InterpolateRatio.z*f3InterpolateRatio.y*f3InterpolateRatio.x;

						for (int m = 0; m < 8; m++)
						{
							fSampleTrilinear += tSampleValues[m] * fInterpolateWeights[m];
						}
					}
					else
					{
						for (int m = 0; m < 8; m++)
						{
							fSampleTrilinear = tSampleValues[m];
							if (fSampleTrilinear > 0)
								break;
						}
					}
					tVoxelValue = (CPUTYPE)((int)fSampleTrilinear + valueoffset);
				}
				else
				{
					int iAddrZ = z + cpu_bnd_size.z;
					int iAddrXY = cpu_bnd_size.x + x + (y + cpu_bnd_size.y) * cpu_sample_width;
					tVoxelValue = (CPUTYPE)((int)cpu_data[iAddrZ][iAddrXY] + valueoffset);
				}
				//if (is_pore?! && tVoxelValue > 254) tVoxelValue = 0;

				gpu_data[x + y * gpu_row_depth_pitch.x + z * gpu_row_depth_pitch.y] = tVoxelValue;
			}
		}
	}
	if (progress)
		progress->Deinit();
	return true;
}


bool grd_helper_legacy::UpdateVolumeModel(GpuRes& gres, const VmVObjectVolume* vobj, const bool use_nearest_max, LocalProgress* progress)
{
	bool is_nearest_max_vol = use_nearest_max;
	((VmVObjectVolume*)vobj)->GetCustomParameter("_bool_UseNearestMax", data_type::dtype<bool>(), &is_nearest_max_vol);

	gres.vm_src_id = vobj->GetObjectID();
	gres.res_name = string("VOLUME_MODEL_") + (is_nearest_max_vol ? string("NEAREST_MAX") : string("DEFAULT"));

	if (g_pCGpuManager->UpdateGpuResource(gres))
		return true;

	gres.rtype = RTYPE_TEXTURE3D;
	gres.options["USAGE"] = D3D11_USAGE_DYNAMIC;
	gres.options["CPU_ACCESS_FLAG"] = D3D11_CPU_ACCESS_WRITE;
	gres.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE;

	const VolumeData* vol_data = ((VmVObjectVolume*)vobj)->GetVolumeData();
	if (vol_data->store_dtype.type_name == data_type::dtype<byte>().type_name)
		gres.options["FORMAT"] = DXGI_FORMAT_R8_UNORM;
	else if (vol_data->store_dtype.type_name == data_type::dtype<ushort>().type_name)
		gres.options["FORMAT"] = DXGI_FORMAT_R16_UNORM;
	else
	{
		::MessageBoxA(NULL, "Not supported Data Type", NULL, MB_OK);
		return false;
	}

	double half_criteria_KB = 512.0 * 1024.0;
	vobj->GetCustomParameter(("_double_ForcedHalfCriterionKB"), data_type::dtype<double>(), &half_criteria_KB);
	half_criteria_KB = min(max(16.0 * 1024.0, half_criteria_KB), 256.0 * 1024.0);

	//////////////////////////////
	// GPU Volume Sample Policy //
	vmfloat3 sample_offset = vmfloat3(1.f, 1.f, 1.f);
	vmdouble3 dvol_size;
	dvol_size.x = vol_data->vol_size.x;
	dvol_size.y = vol_data->vol_size.y;
	dvol_size.z = vol_data->vol_size.z;

	double dsize_vol_KB = dvol_size.x * dvol_size.y * dvol_size.z / 1024.0 * (double)vol_data->store_dtype.type_bytes;
	if (dsize_vol_KB > half_criteria_KB)
	{
		double dRescaleSize = pow(dsize_vol_KB / half_criteria_KB, 1.0 / 3.0);
		sample_offset.x = sample_offset.y = sample_offset.z = (float)dRescaleSize;
	}

	////////////////////////////////////
	// Texture for Volume Description //
RETRY:
	int vol_size_x = max((int)((float)vol_data->vol_size.x / sample_offset.x), 1);
	int vol_size_y = max((int)((float)vol_data->vol_size.y / sample_offset.y), 1);
	int vol_size_z = max((int)((float)vol_data->vol_size.z / sample_offset.z), 1);
	gres.res_dvalues["WIDTH"] = vol_size_x;
	gres.res_dvalues["HEIGHT"] = vol_size_y;
	gres.res_dvalues["DEPTH"] = vol_size_z;
	gres.res_dvalues["SAMPLE_OFFSET_X"] = sample_offset.x;
	gres.res_dvalues["SAMPLE_OFFSET_Y"] = sample_offset.y;
	gres.res_dvalues["SAMPLE_OFFSET_Z"] = sample_offset.z;

	if (vol_size_x > 2048)
	{
		sample_offset.x += 0.5f;
		goto RETRY;
	}
	if (vol_size_y > 2048)
	{
		sample_offset.y += 0.5f;
		goto RETRY;
	}
	if (vol_size_z > 2048)
	{
		sample_offset.z += 0.5f;
		goto RETRY;
	}

	sample_offset.x = (float)vol_data->vol_size.x / (float)vol_size_x;
	sample_offset.y = (float)vol_data->vol_size.y / (float)vol_size_y;
	sample_offset.z = (float)vol_data->vol_size.z / (float)vol_size_z;

	printf("GPU Uploaded Volume Size : %d KB (%dx%dx%d) %d bytes\n", 
		vol_size_x * vol_size_y * vol_size_z / 1024 * vol_data->store_dtype.type_bytes,
		vol_size_x, vol_size_y, vol_size_z, vol_data->store_dtype.type_bytes);

	g_pCGpuManager->GenerateGpuResource(gres);

	ID3D11Texture3D* pdx11tx3d_volume = (ID3D11Texture3D*)gres.alloc_res_ptrs[DTYPE_RES];
	D3D11_MAPPED_SUBRESOURCE d11MappedRes;
	HRESULT hr = g_pvmCommonParams_old->pdx11DeviceImmContext->Map(pdx11tx3d_volume, 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes);
	bool is_downscaled = sample_offset.x > 1.f || sample_offset.y > 1.f || sample_offset.z > 1.f;
	vmint2 gpu_row_depth_pitch = vmint2(d11MappedRes.RowPitch, d11MappedRes.DepthPitch);
	switch (gres.options["FORMAT"])
	{
	case DXGI_FORMAT_R8_UNORM:
		__FillVolumeValues((byte*)d11MappedRes.pData, (const byte**)vol_data->vol_slices, is_downscaled, is_nearest_max_vol, 0,
			vol_data->vol_size, vmint3(vol_size_x, vol_size_y, vol_size_z), vol_data->bnd_size, gpu_row_depth_pitch, progress);
		break;
	case DXGI_FORMAT_R16_UNORM:
		gpu_row_depth_pitch /= 2;
		__FillVolumeValues((ushort*)d11MappedRes.pData, (const ushort**)vol_data->vol_slices, is_downscaled, is_nearest_max_vol, 0,
			vol_data->vol_size, vmint3(vol_size_x, vol_size_y, vol_size_z), vol_data->bnd_size, gpu_row_depth_pitch, progress);
		break;
	default:
		break;
	}
	g_pvmCommonParams_old->pdx11DeviceImmContext->Unmap(pdx11tx3d_volume, 0);

	return true;
}

bool grd_helper_legacy::UpdateTMapBuffer(GpuRes& gres, const VmTObject* main_tobj,
	const map<int, VmTObject*>& tobj_map, const double* series_ids, const double* visible_mask, const int otf_series, const bool update_tf_content, LocalProgress* progress)
{
	int num_otf_series = 0;
	main_tobj->GetCustomParameter("_int_NumOTFSeries", data_type::dtype<int>(), &num_otf_series);
	num_otf_series = max(num_otf_series, 1);

	gres.vm_src_id = main_tobj->GetObjectID();
	gres.res_name = string("OTF_BUFFER");

	if (g_pCGpuManager->UpdateGpuResource(gres) && !update_tf_content)
		return true;

	gres.rtype = RTYPE_BUFFER;
	gres.options["USAGE"] = D3D11_USAGE_DYNAMIC;
	gres.options["CPU_ACCESS_FLAG"] = D3D11_CPU_ACCESS_WRITE;
	gres.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE;
	gres.options["FORMAT"] = DXGI_FORMAT_R8G8B8A8_UNORM;

	TMapData* tmap_data = ((VmTObject*)main_tobj)->GetTMapData();
	gres.res_dvalues["NUM_ELEMENTS"] = tmap_data->array_lengths.x * num_otf_series;
	gres.res_dvalues["STRIDE_BYTES"] = tmap_data->dtype.type_bytes;

	if (!g_pCGpuManager->UpdateGpuResource(gres))
		g_pCGpuManager->GenerateGpuResource(gres);

	D3D11_MAPPED_SUBRESOURCE d11MappedRes;
	g_pvmCommonParams_old->pdx11DeviceImmContext->Map((ID3D11Resource*)gres.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes);
	vmbyte4* py4ColorTF = (vmbyte4*)d11MappedRes.pData;
	if (tobj_map.size() <= 1 || otf_series <= 1 || !series_ids || !visible_mask)
	{
		memcpy(py4ColorTF, tmap_data->tmap_buffers[0], tmap_data->array_lengths.x * sizeof(vmbyte4));
	}
	else
	{
		for (int i = 0; i < otf_series; i++)
		{
			int tobj_id = (int)series_ids[i];
			bool bVisibleMaskValue = visible_mask[i] != 0;
			if (bVisibleMaskValue)
			{
				auto itrTObjectMask = tobj_map.find(tobj_id);
				VmTObject* pCTObjectMask = itrTObjectMask->second;
				TMapData* tmap_mask = pCTObjectMask->GetTMapData();
				memcpy(&py4ColorTF[i * tmap_data->array_lengths.x], tmap_mask->tmap_buffers[0], tmap_data->array_lengths.x * sizeof(vmbyte4));
			}
			else
			{
				memset(&py4ColorTF[i * tmap_data->array_lengths.x], 0, tmap_data->array_lengths.x * sizeof(vmbyte4));
			}
		}
	}
	g_pvmCommonParams_old->pdx11DeviceImmContext->Unmap((ID3D11Resource*)gres.alloc_res_ptrs[DTYPE_RES], 0);
	
	return true;
}

bool grd_helper_legacy::UpdatePrimitiveModel(GpuRes& gres_vtx, GpuRes& gres_idx, GpuRes& gres_tex, VmVObjectPrimitive* pobj, LocalProgress* progress)
{
	PrimitiveData* prim_data = ((VmVObjectPrimitive*)pobj)->GetPrimitiveData();

	// always
	{
		gres_vtx.vm_src_id = pobj->GetObjectID();
		gres_vtx.res_name = string("PRIMITIVE_MODEL_VTX");

		bool update_data = false;
		pobj->GetCustomParameter("_bool_UpdateData", data_type::dtype<bool>(), &update_data);

		if (!g_pCGpuManager->UpdateGpuResource(gres_vtx))
		{
			int num_vtx_defs = prim_data->GetNumVertexDefinitions();
			uint stride_bytes = num_vtx_defs * sizeof(vmfloat3);
			gres_vtx.rtype = RTYPE_BUFFER;
			gres_vtx.options["USAGE"] = D3D11_USAGE_DYNAMIC;
			gres_vtx.options["CPU_ACCESS_FLAG"] = D3D11_CPU_ACCESS_WRITE;// | D3D11_CPU_ACCESS_READ;
			gres_vtx.options["BIND_FLAG"] = D3D11_BIND_VERTEX_BUFFER;
			gres_vtx.options["FORMAT"] = DXGI_FORMAT_R32G32B32_FLOAT;
			gres_vtx.res_dvalues["NUM_ELEMENTS"] = prim_data->num_vtx;
			gres_vtx.res_dvalues["STRIDE_BYTES"] = stride_bytes;

			g_pCGpuManager->GenerateGpuResource(gres_vtx);
			update_data = true;
		}

		if (update_data)
		{
			int num_vtx_defs = prim_data->GetNumVertexDefinitions();
			ID3D11Buffer* pdx11bufvtx = (ID3D11Buffer*)gres_vtx.alloc_res_ptrs[DTYPE_RES];

			vector<vmfloat3*> vtx_def_ptrs;
			vmfloat3* vtx_pos = prim_data->GetVerticeDefinition("POSITION");
			if (vtx_pos)
				vtx_def_ptrs.push_back(vtx_pos);
			vmfloat3* vtx_nrl = prim_data->GetVerticeDefinition("NORMAL");
			if (vtx_nrl)
				vtx_def_ptrs.push_back(vtx_nrl);
			for (int i = 0; i < num_vtx_defs; i++)
			{
				vmfloat3* vtx_tex = prim_data->GetVerticeDefinition(string("TEXCOORD") + to_string(i));
				if (vtx_tex)
					vtx_def_ptrs.push_back(vtx_tex);
			}

			D3D11_MAPPED_SUBRESOURCE mappedRes;
			g_pvmCommonParams_old->pdx11DeviceImmContext->Map(pdx11bufvtx, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes);
			vmfloat3* vtx_group = (vmfloat3*)mappedRes.pData;
			for (uint i = 0; i < prim_data->num_vtx; i++)
			{
				for (int j = 0; j < num_vtx_defs; j++)
				{
					vtx_group[i*num_vtx_defs + j] = vtx_def_ptrs[j][i];
				}
			}
			g_pvmCommonParams_old->pdx11DeviceImmContext->Unmap(pdx11bufvtx, NULL);

			pobj->RegisterCustomParameter("_bool_UpdateData", false);
		}
	}

	if (prim_data->vidx_buffer && prim_data->num_vidx > 0)
	{
		gres_idx.vm_src_id = pobj->GetObjectID();
		gres_idx.res_name = string("PRIMITIVE_MODEL_IDX");

		if (!g_pCGpuManager->UpdateGpuResource(gres_idx))
		{
			gres_idx.rtype = RTYPE_BUFFER;
			gres_idx.options["USAGE"] = D3D11_USAGE_DYNAMIC;
			gres_idx.options["CPU_ACCESS_FLAG"] = D3D11_CPU_ACCESS_WRITE;
			gres_idx.options["BIND_FLAG"] = D3D11_BIND_INDEX_BUFFER;
			gres_idx.options["FORMAT"] = DXGI_FORMAT_R32_UINT;
			gres_idx.res_dvalues["NUM_ELEMENTS"] = prim_data->num_vidx;
			gres_idx.res_dvalues["STRIDE_BYTES"] = sizeof(uint);

			g_pCGpuManager->GenerateGpuResource(gres_idx);

			ID3D11Buffer* pdx11bufidx = (ID3D11Buffer*)gres_idx.alloc_res_ptrs[DTYPE_RES];

			D3D11_MAPPED_SUBRESOURCE mappedRes;
			g_pvmCommonParams_old->pdx11DeviceImmContext->Map(pdx11bufidx, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes);
			uint* vidx_buf = (uint*)mappedRes.pData;
			memcpy(vidx_buf, prim_data->vidx_buffer, prim_data->num_vidx * sizeof(uint));
			g_pvmCommonParams_old->pdx11DeviceImmContext->Unmap(pdx11bufidx, NULL);
		}
	}

	//vmint3 tex_res_size;
	//((VmVObjectPrimitive*)pobj)->GetCustomParameter("_int3_TextureWHN", data_type::dtype<vmint3>(), &tex_res_size);
	vmint2 tex_res_size;
	int byte_stride;
	byte* texture_res;
	if (prim_data->GetTexureInfo("MAP_COLOR4", tex_res_size.x, tex_res_size.y, byte_stride, &texture_res))
	{
		// cmm case
		gres_tex.vm_src_id = pobj->GetObjectID();
		gres_tex.res_name = string("PRIMITIVE_MODEL_TEX_COLOR4");

		if (!g_pCGpuManager->UpdateGpuResource(gres_tex))
		{
			gres_tex.rtype = RTYPE_TEXTURE2D;
			gres_tex.options["USAGE"] = D3D11_USAGE_DYNAMIC;
			gres_tex.options["CPU_ACCESS_FLAG"] = D3D11_CPU_ACCESS_WRITE;
			gres_tex.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE;
			gres_tex.options["FORMAT"] = DXGI_FORMAT_R8G8B8A8_UNORM;
			gres_tex.res_dvalues["WIDTH"] = tex_res_size.x;
			gres_tex.res_dvalues["HEIGHT"] = tex_res_size.y;
			gres_tex.res_dvalues["DEPTH"] = (double)prim_data->texture_res_info.size();

			auto upload_teximg = [&prim_data](GpuRes& gres_tex, D3D11_MAP maptype)
			{
				ID3D11Texture2D* pdx11tx2dres = (ID3D11Texture2D*)gres_tex.alloc_res_ptrs[DTYPE_RES];
				D3D11_MAPPED_SUBRESOURCE mappedRes;
				g_pvmCommonParams_old->pdx11DeviceImmContext->Map(pdx11tx2dres, 0, maptype, 0, &mappedRes);
				vmbyte4* tx_res = (vmbyte4*)mappedRes.pData;
				int index = 0;
				for (auto it = prim_data->texture_res_info.begin(); it != prim_data->texture_res_info.end(); it++, index++)
				{
					byte* texture_res = get<3>(it->second);
					vmint2 tex_res_size = vmint2(get<0>(it->second), get<1>(it->second));
					int byte_stride = get<2>(it->second);

					if (byte_stride == 4)
					{
						vmbyte4* tx_res_cpu = (vmbyte4*)texture_res;
						for (int h = 0; h < tex_res_size.y; h++)
							memcpy(&tx_res[h * (mappedRes.RowPitch / 4) + index * (mappedRes.DepthPitch / 4)], &tx_res_cpu[h * tex_res_size.x], tex_res_size.x * sizeof(vmbyte4));
					}
					else
					{
						assert(byte_stride == 3);
						for (int h = 0; h < tex_res_size.y; h++)
							for (int x = 0; x < tex_res_size.x; x++)
							{
								vmbyte4 rgba;
								rgba.r = texture_res[(x + h * tex_res_size.x) * byte_stride + 0];
								rgba.g = texture_res[(x + h * tex_res_size.x) * byte_stride + 1];
								rgba.b = texture_res[(x + h * tex_res_size.x) * byte_stride + 2];
								rgba.a = 255;
								tx_res[x + h * (mappedRes.RowPitch / 4) + index * (mappedRes.DepthPitch / 4)] = rgba;
							}
					}
				}
				g_pvmCommonParams_old->pdx11DeviceImmContext->Unmap(pdx11tx2dres, NULL);
			};

			if (prim_data->texture_res_info.size() == 1)
			{
				g_pCGpuManager->GenerateGpuResource(gres_tex);
				upload_teximg(gres_tex, D3D11_MAP_WRITE_DISCARD);
			}
			else
			{
				gres_tex.options["USAGE"] = D3D11_USAGE_DEFAULT;
				gres_tex.options["CPU_ACCESS_FLAG"] = NULL;
				g_pCGpuManager->GenerateGpuResource(gres_tex);

				// if the texture is array, direct mapping is impossible, so use this tricky copyresource way.
				GpuRes tmp_gres;
				tmp_gres.vm_src_id = pobj->GetObjectID();
				tmp_gres.res_name = string("PRIMITIVE_MODEL_TEX_TEMP");
				tmp_gres.rtype = RTYPE_TEXTURE2D;
				tmp_gres.options["USAGE"] = D3D11_USAGE_STAGING;
				tmp_gres.options["CPU_ACCESS_FLAG"] = D3D11_CPU_ACCESS_WRITE;
				tmp_gres.options["BIND_FLAG"] = NULL;
				tmp_gres.options["FORMAT"] = DXGI_FORMAT_R8G8B8A8_UNORM;
				tmp_gres.res_dvalues["WIDTH"] = tex_res_size.x;
				tmp_gres.res_dvalues["HEIGHT"] = tex_res_size.y;
				tmp_gres.res_dvalues["DEPTH"] = (double)prim_data->texture_res_info.size();
				g_pCGpuManager->GenerateGpuResource(tmp_gres);
				upload_teximg(tmp_gres, D3D11_MAP_WRITE);
				g_pvmCommonParams_old->pdx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gres_tex.alloc_res_ptrs[DTYPE_RES], (ID3D11Texture2D*)tmp_gres.alloc_res_ptrs[DTYPE_RES]);
				g_pCGpuManager->ReleaseGpuResource(tmp_gres, false);
			}
		}
	}

	//if (prim_data->texture_res_info.size() > 0)
	//{
	//	vmint3 tex_res_size;
	//	void* texture_res;
	//	prim_data->GetTexureInfo(0, tex_res_size.x, tex_res_size.y, tex_res_size.z, &texture_res);
	//	gres_tex.vm_src_id = pobj->GetObjectID();
	//	gres_tex.res_name = string("PRIMITIVE_MODEL_TEX_LEGACY");
	//
	//	if (!g_pCGpuManager->UpdateGpuResource(gres_tex))
	//	{
	//		gres_tex.rtype = RTYPE_TEXTURE2D;
	//		gres_tex.options["USAGE"] = D3D11_USAGE_DYNAMIC;
	//		gres_tex.options["CPU_ACCESS_FLAG"] = D3D11_CPU_ACCESS_WRITE;
	//		gres_tex.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE;
	//		gres_tex.options["FORMAT"] = DXGI_FORMAT_R8G8B8A8_UNORM;
	//		gres_tex.res_dvalues["WIDTH"] = tex_res_size.x;
	//		gres_tex.res_dvalues["HEIGHT"] = tex_res_size.y;
	//		//gres_tex.res_dvalues["DEPTH"] = (double)prim_data->texture_res_info.size();
	//
	//		g_pCGpuManager->GenerateGpuResource(gres_tex);
	//
	//		ID3D11Texture2D* pdx11tx2dres = (ID3D11Texture2D*)gres_tex.alloc_res_ptrs[DTYPE_RES];
	//
	//		D3D11_MAPPED_SUBRESOURCE mappedRes;
	//		g_VmCommonParams.pdx11DeviceImmContext->Map(pdx11tx2dres, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes);
	//		vmbyte4* tx_res = (vmbyte4*)mappedRes.pData;
	//		vmbyte4* tx_res_cpu = (vmbyte4*)texture_res;
	//		for (int h = 0; h < tex_res_size.y; h++)
	//			memcpy(&tx_res[h * (mappedRes.RowPitch / 4)], &tx_res_cpu[h * tex_res_size.x], tex_res_size.x * sizeof(vmbyte4));
	//		g_VmCommonParams.pdx11DeviceImmContext->Unmap(pdx11tx2dres, NULL);
	//	}
	//}

	return true;
}

bool grd_helper_legacy::UpdateFrameBuffer(GpuRes& gres, 
	const VmIObject* iobj, 
	const string& res_name, 
	const GpuResType gres_type, 
	const uint bind_flag, 
	const uint dx_format, 
	const bool is_system_out, 
	const int num_deeplayers,
	const int structured_stride)
{
	gres.vm_src_id = iobj->GetObjectID();
	gres.res_name = res_name;
	
	if (g_pCGpuManager->UpdateGpuResource(gres))
		return true;

	gres.rtype = gres_type;
	vmint2 fb_size;
	((VmIObject*)iobj)->GetFrameBufferInfo(&fb_size);

	gres.options["USAGE"] = is_system_out ? D3D11_USAGE_STAGING : D3D11_USAGE_DEFAULT;
	gres.options["CPU_ACCESS_FLAG"] = is_system_out ? D3D11_CPU_ACCESS_READ : NULL;
	gres.options["BIND_FLAG"] = bind_flag;
	gres.options["FORMAT"] = dx_format;
	uint stride_bytes = 0;
	switch (gres_type)
	{
	case RTYPE_BUFFER:
		gres.res_dvalues["NUM_ELEMENTS"] = num_deeplayers > 0 ? fb_size.x * fb_size.y * num_deeplayers : fb_size.x * fb_size.y;
		switch (dx_format)
		{
		case DXGI_FORMAT_R32_UINT: stride_bytes = sizeof(uint); break;
		case DXGI_FORMAT_R32_FLOAT: stride_bytes = sizeof(float); break;
		case DXGI_FORMAT_D32_FLOAT:	stride_bytes = sizeof(float); break;
		case DXGI_FORMAT_R32G32B32A32_FLOAT: stride_bytes = sizeof(vmfloat4); break;
		case DXGI_FORMAT_R8G8B8A8_UNORM: stride_bytes = sizeof(vmbyte4); break;
		case DXGI_FORMAT_R8_UNORM: stride_bytes = sizeof(byte); break;
		case DXGI_FORMAT_R8_UINT: stride_bytes = sizeof(byte); break;
		case DXGI_FORMAT_R16_UNORM: stride_bytes = sizeof(ushort); break;
		case DXGI_FORMAT_R16_UINT: stride_bytes = sizeof(ushort); break;
		case DXGI_FORMAT_UNKNOWN: stride_bytes = structured_stride; break;
		default:
			return false;
		}
		if (stride_bytes == 0) return false;
		gres.res_dvalues["STRIDE_BYTES"] = (double)(stride_bytes);
		break;
	case RTYPE_TEXTURE2D:
		gres.res_dvalues["WIDTH"] = fb_size.x;
		gres.res_dvalues["HEIGHT"] = fb_size.y;
		break;
	default:
		::MessageBoxA(NULL, "Not supported Data Type", NULL, MB_OK);
		return false;
	}

	g_pCGpuManager->GenerateGpuResource(gres);

	return true;
}

template <typename T>
bool __GetParameterFromCustomStringMap(T* pvParameter/*out*/, map<string, void*>* pmapCustomParameter, string strParameterName)
{
	map<string, void*>::iterator itrParam = pmapCustomParameter->find(strParameterName.c_str());
	if (itrParam == pmapCustomParameter->end())
		return false;

	*(T*)pvParameter = *(T*)itrParam->second;

	return true;
}

// Compute Constant Buffers //
bool grd_helper_legacy::SetCbVrBlockObj(CB_VrBlockObject* pCBVrBlockObj, VmVObjectVolume* pCVolume, int iLevelBlock, float fScaleValue)
{
	pCBVrBlockObj->fSampleValueRange = fScaleValue; // BLK 16UNORM ==> 16UINT... and forced to set 1

	VolumeData* pVolumeData = pCVolume->GetVolumeData();
	VolumeBlocks* psvxBlock = pCVolume->GetVolumeBlock(iLevelBlock);
	if (psvxBlock == NULL)
		return false;

	pCBVrBlockObj->f3SizeBlockTS = XMFLOAT3((float)psvxBlock->unitblk_size.x / (float)pVolumeData->vol_size.x
		, (float)psvxBlock->unitblk_size.y / (float)pVolumeData->vol_size.y, (float)psvxBlock->unitblk_size.z / (float)pVolumeData->vol_size.z);
	return true;
}

bool grd_helper_legacy::SetCbVrCamera(CB_VrCameraState* pCBVrCamera, VmCObject* pCCObject, vmint2 i2ScreenSizeFB, map<string, void*>* pmapCustomParameter)
{

	vmmat44 dmatSS2PS, dmatPS2CS, dmatCS2WS;
	pCCObject->GetMatrixSStoWS((vmmat44*)&dmatSS2PS, (vmmat44*)&dmatPS2CS, (vmmat44*)&dmatCS2WS);
	pCBVrCamera->dxmatSS2WS = *(XMMATRIX*)&(vmmat44f)(dmatSS2PS*(dmatPS2CS*dmatCS2WS));

	pCCObject->GetCameraExtStatef((vmfloat3*)&pCBVrCamera->f3PosEyeWS, (vmfloat3*)&pCBVrCamera->f3VecViewWS, NULL);

	vmint2 i2SizeFB = i2ScreenSizeFB;
	pCBVrCamera->uiScreenSizeX = i2SizeFB.x;
	pCBVrCamera->uiScreenSizeY = i2SizeFB.y;
	uint uiNumGridX = (uint)ceil(i2SizeFB.x / (float)__BLOCKSIZE);
	pCBVrCamera->uiGridOffsetX = i2SizeFB.x;// uiNumGridX * __BLOCKSIZE;

	bool bIsLightOnCamera = true;
	__GetParameterFromCustomStringMap<bool>(&bIsLightOnCamera, pmapCustomParameter, ("_bool_IsLightOnCamera"));
	vmdouble3 d3VecLightWS(pCBVrCamera->f3VecViewWS.x, pCBVrCamera->f3VecViewWS.y, pCBVrCamera->f3VecViewWS.z);
	vmdouble3 d3PosLightWS(pCBVrCamera->f3PosEyeWS.x, pCBVrCamera->f3PosEyeWS.y, pCBVrCamera->f3PosEyeWS.z);
	if (!bIsLightOnCamera)
	{
		__GetParameterFromCustomStringMap<vmdouble3>(&d3VecLightWS, pmapCustomParameter, ("_double3_VecLightWS"));
		__GetParameterFromCustomStringMap<vmdouble3>(&d3PosLightWS, pmapCustomParameter, ("_double3_PosLightWS"));
	}
	pCBVrCamera->f3PosGlobalLightWS = *(XMFLOAT3*)&vmfloat3(d3PosLightWS);
	pCBVrCamera->f3VecGlobalLightWS = *(XMFLOAT3*)&vmfloat3(d3VecLightWS);
	XMStoreFloat3(&pCBVrCamera->f3VecGlobalLightWS, XMVector3Normalize(XMLoadFloat3(&pCBVrCamera->f3VecGlobalLightWS)));
	
	pCBVrCamera->iFlags = 0;
	if (pCCObject->IsPerspective())
		pCBVrCamera->iFlags |= 0x1;
	bool bIsSpotLight = false;
	__GetParameterFromCustomStringMap<bool>(&bIsSpotLight, pmapCustomParameter, ("_bool_IsPointSpotLight"));
	if (bIsSpotLight)
		pCBVrCamera->iFlags |= 0x2;

	// Matrix Transpose //
	/////D3DXMatrixTranspose(&pCBVrCamera->matSS2WS, &pCBVrCamera->matSS2WS);

	return true;
}

bool grd_helper_legacy::SetCbVrOtf1D(CB_VrOtf1D* pCBVrOtf1D, VmTObject* pCTObject, int iOtfIndex, map<string, void*>* pmapCustomParameter)
{
	TMapData* psvxTfArchive = pCTObject->GetTMapData();
	pCBVrOtf1D->iOtf1stValue = psvxTfArchive->valid_min_idx.x;
	pCBVrOtf1D->iOtfLastValue = psvxTfArchive->valid_max_idx.x;
	pCBVrOtf1D->iOtfSize = psvxTfArchive->array_lengths.x;
	vmbyte4 y4ColorEnd = ((vmbyte4**)psvxTfArchive->tmap_buffers)[0][psvxTfArchive->array_lengths.x - 1];
	pCBVrOtf1D->f4OtfColorEnd = XMFLOAT4(y4ColorEnd.x / 255.f, y4ColorEnd.y / 255.f, y4ColorEnd.z / 255.f, y4ColorEnd.w / 255.f);
	if (iOtfIndex == 1)
	{
		// Deprecated //
		//pCBVrOtf1D->f4OtfColorEnd += *(XMFLOAT4*)&((vmfloat4**)psvxTfArchive->tmap_buffers)[1][psvxTfArchive->array_lengths.x - 1];
	}

	return true;
}

bool grd_helper_legacy::SetCbVrShadowMap(CB_VrShadowMap* pCBVrShadowMap, CB_VrCameraState* pCBVrCamStateForShadowMap, vmfloat3 f3PosOverviewBoxMinWS, vmfloat3 f3PosOverviewBoxMaxWS, map<string, void*>* pmapCustomParameter)
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
	XMFLOAT3 f3PosOrthoMinWS(f3PosOverviewBoxMinWS.x, f3PosOverviewBoxMinWS.y, f3PosOverviewBoxMinWS.z);
	XMFLOAT3 f3PosOrthoMaxWS(f3PosOverviewBoxMaxWS.x, f3PosOverviewBoxMaxWS.y, f3PosOverviewBoxMaxWS.z);
	XMFLOAT3 f3PosCenterWS;
	XMStoreFloat3(&f3PosCenterWS, (XMLoadFloat3(&f3PosOrthoMaxWS) + XMLoadFloat3(&f3PosOrthoMinWS)) * 0.5f);
	float fDistCenter2Light = XMVectorGetByIndex(XMVector3Length(XMLoadFloat3(&f3PosCenterWS) - XMLoadFloat3(&pCBVrCamStateForShadowMap->f3PosEyeWS)), 0);
	float fDiagonalLength = XMVectorGetByIndex(XMVector3Length(XMLoadFloat3(&f3PosOrthoMaxWS) - XMLoadFloat3(&f3PosOrthoMinWS)), 0);
	if (!(pCBVrCamStateForShadowMap->iFlags & 0x2))
		XMStoreFloat3(&pCBVrCamStateForShadowMap->f3PosEyeWS,
			XMLoadFloat3(&f3PosCenterWS) - XMLoadFloat3(&pCBVrCamStateForShadowMap->f3VecViewWS) * fDiagonalLength);

	XMMATRIX dxmatSS2PS, dxmatPS2CS, dxmatCS2WS;
	XMMATRIX dxmatWS2CS, dxmatCS2PS, dxmatPS2SS;

	XMVECTOR f3VecUp = XMVectorSet(0, 0, 1.f, 1.f);
	while (XMVectorGetByIndex(XMVector3Length(XMVector3Cross(XMLoadFloat3(&pCBVrCamStateForShadowMap->f3VecViewWS), f3VecUp)), 0) == 0)
		f3VecUp = f3VecUp + XMVectorSet(0, 1.f, 0, 1.f);
	pCBVrShadowMap->dxmatWS2WLS = XMMatrixLookAtRH(XMLoadFloat3(&pCBVrCamStateForShadowMap->f3PosEyeWS),
		XMLoadFloat3(&pCBVrCamStateForShadowMap->f3PosEyeWS) + XMLoadFloat3(&pCBVrCamStateForShadowMap->f3VecViewWS), f3VecUp);

	dxmatWS2CS = pCBVrShadowMap->dxmatWS2WLS;
	if (pCBVrCamStateForShadowMap->iFlags & 0x2)
	{
		dxmatCS2PS = XMMatrixPerspectiveFovRH(XM_PI / 4.f, 1.f, 0.1f, fDistCenter2Light + fDiagonalLength / 2);
	}
	else
	{
		dxmatCS2PS = XMMatrixOrthographicRH(fDiagonalLength, fDiagonalLength, 0, fDistCenter2Light + fDiagonalLength / 2);
	}
	fMatrixPS2SS((vmmat44f*)&dxmatPS2SS, (float)pCBVrCamStateForShadowMap->uiRenderingSizeX, (float)pCBVrCamStateForShadowMap->uiRenderingSizeY);
	dxmatPS2SS = XMMatrixTranspose(dxmatPS2SS);

	dxmatCS2WS = XMMatrixInverse(NULL, dxmatWS2CS);
	dxmatPS2CS = XMMatrixInverse(NULL, dxmatCS2PS);
	dxmatSS2PS = XMMatrixInverse(NULL, dxmatPS2SS);

	XMFLOAT4X4 __matRS2SS;
	XMStoreFloat4x4(&__matRS2SS, pCBVrCamStateForShadowMap->dxmatRS2SS);
	__matRS2SS._12 = __matRS2SS._13 = __matRS2SS._14 = 0;
	__matRS2SS._21 = __matRS2SS._23 = __matRS2SS._24 = 0;
	__matRS2SS._33 = __matRS2SS._44 = 1.f;
	__matRS2SS._31 = __matRS2SS._32 = __matRS2SS._34 = 0; 
	__matRS2SS._41 = __matRS2SS._42 = __matRS2SS._43 = 0;
	
	//dxmatRS2SS._11 = pCBVrCamStateForShadowMap->dxmatRS2SS._11;
	//dxmatRS2SS._22 = pCBVrCamStateForShadowMap->dxmatRS2SS._22;

	pCBVrCamStateForShadowMap->dxmatSS2WS = dxmatSS2PS * (dxmatPS2CS*dxmatCS2WS);	// CS
	pCBVrCamStateForShadowMap->dxmatRS2SS = XMLoadFloat4x4(&__matRS2SS);

	XMFLOAT4X4 __matSS2RS = __matRS2SS;
	__matSS2RS._11 = 1.f / __matRS2SS._11;
	__matSS2RS._22 = 1.f / __matRS2SS._22;
	//matRS2SS[3][0] = -pCBVrCamStateForShadowMap->matRS2SS._14;
	//matRS2SS[3][1] = -pCBVrCamStateForShadowMap->matRS2SS._24;

	//XMMATRIX _matRS2SS;
	///////D3DXMatrixTranspose(&_matRS2SS, &pCBVrCamStateForShadowMap->matRS2SS);
	//XMMATRIX _matSS2RS;
	//D3DXMatrixInverse(&_matSS2RS, NULL, &_matRS2SS);

	pCBVrShadowMap->dxmatWS2LS = dxmatWS2CS * dxmatCS2PS*dxmatPS2SS*XMLoadFloat4x4(&__matSS2RS); // SM

	pCBVrShadowMap->dxmatWS2WLS = XMMatrixTranspose(pCBVrShadowMap->dxmatWS2WLS);
	pCBVrShadowMap->dxmatWS2LS = XMMatrixTranspose(pCBVrShadowMap->dxmatWS2LS);
	pCBVrCamStateForShadowMap->dxmatSS2WS = XMMatrixTranspose(pCBVrCamStateForShadowMap->dxmatSS2WS);
	//pCBVrCamStateForShadowMap->matRS2SS._14 = pCBVrCamStateForShadowMap->matRS2SS._24 = 0;

	// 1st bit - 0 : Orthogonal or 1 : Perspective
	// 2nd bit - 0 : Parallel Light or 1 : Point Spot Light
	if (pCBVrCamStateForShadowMap->iFlags & 0x2)
		pCBVrCamStateForShadowMap->iFlags |= 0x1;	// SM
	else
		pCBVrCamStateForShadowMap->iFlags &= 0x0;

	// 	XMFLOAT3 f3VecViewVS, f3VecViewWS;
	// 	D3DXVec3TransformNormal(&f3VecViewVS, &pCBVrCamStateForShadowMap->f3VecViewWS, (XMMATRIX*)&pCVObjectVolume->GetMatrixWS2OSf());
	// 	D3DXVec3Normalize(&f3VecViewVS, &f3VecViewVS);
	// 	D3DXVec3TransformNormal(&f3VecViewWS, &f3VecViewVS, (XMMATRIX*)&pCVObjectVolume->GetMatrixOS2WSf());
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

// Compute Constant Buffers Using VmLObject pCLObject* pCLObjectForParameters//
bool grd_helper_legacy::SetCbVrObject(CB_VrVolumeObject* pCBVrVolumeObj, bool high_samplerate, VmVObjectVolume* pCVObjectVolume, VmCObject* pCCObject, vmint3 i3SizeVolumeTextureElements, VmLObject* pCLObjectForParameters, int obj_param_src_id, float vz_thickness)
{
	VolumeData* pVolumeData = pCVObjectVolume->GetVolumeData();
	pCBVrVolumeObj->iFlags = 0;

	vmmat44f matVS2TS;
#ifdef OLD
	float fInverseX = 1;
	if (pVolumeData->vol_size.x > 1)
		fInverseX = 1.f / ((float)pVolumeData->vol_size.x - 0.0f);
	float fInverseY = 1;
	if (pVolumeData->vol_size.y > 1)
		fInverseY = 1.f / ((float)pVolumeData->vol_size.y - 0.0f);
	float fInverseZ = 1;
	if (pVolumeData->vol_size.z > 1)
		fInverseZ = 1.f / ((float)pVolumeData->vol_size.z - 0.0f);
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

	pCBVrVolumeObj->dxmatWS2TS = *(XMMATRIX*)&(pCVObjectVolume->GetMatrixWS2OSf() * matVS2TS); // set

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
	if (fDotVector(&f3VecCrossZY, &f3VecVObjectXWS) < 0)
		fTransformVector(&f3VecVObjectYWS, &vmfloat3(1.f, 0, 0), &matVS2WS);

	fMatrixWS2CS((vmmat44f*)&pCBVrVolumeObj->dxmatAlginedWS, (vmfloat3*)&f3PosVObjectMinWS, (vmfloat3*)&f3VecVObjectYWS, (vmfloat3*)&f3VecVObjectZWS); // set

	vmdouble4 d4ShadingFactor = vmdouble4(0.4, 0.6, 0.8, 70);
	pCLObjectForParameters->GetDstObjValue(obj_param_src_id, "_double4_ShadingFactors", &d4ShadingFactor);
	pCBVrVolumeObj->f4ShadingFactor = XMFLOAT4((float)d4ShadingFactor.x, (float)d4ShadingFactor.y, (float)d4ShadingFactor.z, (float)d4ShadingFactor.w);

	vmmat44 dmatClipWS2BS;
	vmdouble3 d3PosOrthoMaxBox;
	pCBVrVolumeObj->dxmatClipBoxTransform = XMMatrixIdentity();
	int iClippingMode = 0;
	pCLObjectForParameters->GetDstObjValue(obj_param_src_id, "_int_ClippingMode", &iClippingMode);
	if (pCLObjectForParameters->GetDstObjValue(obj_param_src_id, "_matrix44_MatrixClipWS2BS", &dmatClipWS2BS)
		&& pCLObjectForParameters->GetDstObjValue(obj_param_src_id, "_double3_PosClipBoxMaxWS", &d3PosOrthoMaxBox))
	{
		pCBVrVolumeObj->dxmatClipBoxTransform = *(XMMATRIX*)&(vmmat44f)dmatClipWS2BS;
		pCBVrVolumeObj->f3ClipBoxOrthoMaxWS = *(XMFLOAT3*)&vmfloat3(d3PosOrthoMaxBox);
	}

	pCBVrVolumeObj->iFlags = 0x3 & iClippingMode;

	double dClipPlaneIntensity = 1.0;
	pCLObjectForParameters->GetDstObjValue(obj_param_src_id, "_double_ClipPlaneIntensity", &dClipPlaneIntensity);
	pCBVrVolumeObj->fClipPlaneIntensity = (float)dClipPlaneIntensity;

	vmdouble3 d3PosOnPlane, d3VecPlane;
	if (pCLObjectForParameters->GetDstObjValue(obj_param_src_id, "_double3_PosClipPlaneWS", &d3PosOnPlane)
		&& pCLObjectForParameters->GetDstObjValue(obj_param_src_id, "_double3_VecClipPlaneWS", &d3VecPlane))
	{
		pCBVrVolumeObj->f3PosClipPlaneWS = *(XMFLOAT3*)&vmfloat3(d3PosOnPlane);
		pCBVrVolumeObj->f3VecClipPlaneWS = *(XMFLOAT3*)&vmfloat3(d3VecPlane);
	}

	if (pVolumeData->store_dtype.type_bytes == data_type::dtype<byte>().type_bytes) // char
		pCBVrVolumeObj->fSampleValueRange = 255.f;
	else if (pVolumeData->store_dtype.type_bytes == data_type::dtype<ushort>().type_bytes) // short
		pCBVrVolumeObj->fSampleValueRange = 65535.f;
	else  return false;

	//XMFLOAT3 f3VecSampleDirWS;
	//pCCObject->GetCameraExtState(NULL, (vmfloat3*)&f3VecSampleDirWS, NULL);
	//XMFLOAT3 f3VecSampleDirVS;
	//D3DXVec3TransformNormal(&f3VecSampleDirVS, &f3VecSampleDirWS, (XMMATRIX*)&pCVObjectVolume->GetMatrixWS2OSf());
	//D3DXVec3Normalize(&f3VecSampleDirVS, &f3VecSampleDirVS);
	//pCBVrVolumeObj->fSampleDistWS = D3DXVec3Length(&f3VecSampleDirWS);// *0.5f;
	//D3DXVec3TransformNormal(&f3VecSampleDirWS, &f3VecSampleDirVS, (XMMATRIX*)&pCVObjectVolume->GetMatrixOS2WSf());
	float minDistSample = (float)min(min(pVolumeData->vox_pitch.x, pVolumeData->vox_pitch.y), pVolumeData->vox_pitch.z);
	pCBVrVolumeObj->fSampleDistWS = minDistSample;
	fTransformVector((vmfloat3*)&pCBVrVolumeObj->f3VecGradientSampleX, &vmfloat3(pCBVrVolumeObj->fSampleDistWS, 0, 0), (vmmat44f*)&pCBVrVolumeObj->dxmatWS2TS);
	fTransformVector((vmfloat3*)&pCBVrVolumeObj->f3VecGradientSampleY, &vmfloat3(0, pCBVrVolumeObj->fSampleDistWS, 0), (vmmat44f*)&pCBVrVolumeObj->dxmatWS2TS);
	fTransformVector((vmfloat3*)&pCBVrVolumeObj->f3VecGradientSampleZ, &vmfloat3(0, 0, pCBVrVolumeObj->fSampleDistWS), (vmmat44f*)&pCBVrVolumeObj->dxmatWS2TS);
	pCBVrVolumeObj->fOpaqueCorrection = 1.f;
	if (high_samplerate)
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
	pCBVrVolumeObj->f3SizeVolumeCVS = XMFLOAT3((float)i3SizeVolumeTextureElements.x, (float)i3SizeVolumeTextureElements.y, (float)i3SizeVolumeTextureElements.z);

	// from pmapDValueVolume //
	double dSamplePrecisionLevel = 1.0;
	bool bIsForcedVzThickness = false;
	double dThicknessVirtualzSlab = vz_thickness > 0 ? (double)vz_thickness : pCBVrVolumeObj->fSampleDistWS;;
	double dVoxelSharpnessForAttributeVolume = 0.25;

	if (pCLObjectForParameters != NULL)
	{
		pCLObjectForParameters->GetDstObjValue(obj_param_src_id, "_double_SamplePrecisionLevel", &dSamplePrecisionLevel);
		pCLObjectForParameters->GetDstObjValue(obj_param_src_id, "_double_VZThickness", &dThicknessVirtualzSlab);
		pCLObjectForParameters->GetDstObjValue(obj_param_src_id, "_double_VoxelSharpnessForAttributeVolume", &dVoxelSharpnessForAttributeVolume);
	}

	float fSamplePrecisionLevel = (float)dSamplePrecisionLevel;
	if (fSamplePrecisionLevel > 0)
	{
		pCBVrVolumeObj->fSampleDistWS /= fSamplePrecisionLevel;
		pCBVrVolumeObj->fOpaqueCorrection /= fSamplePrecisionLevel;
	}
	pCBVrVolumeObj->fThicknessVirtualzSlab = (float)dThicknessVirtualzSlab;	// 현재 HLSL 은 Sample Distance 를 강제로 사용 중...
	pCBVrVolumeObj->fVoxelSharpnessForAttributeVolume = (float)dVoxelSharpnessForAttributeVolume;

	return true;
}

bool grd_helper_legacy::SetCbVrSurfaceEffect(CB_VrSurfaceEffect* pCBVrSurfaceEffect, float fDistSample, VmLObject* pCLObjectForParameters, int obj_param_src_id)
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

bool __GradientMagnitudeAnalysis(vmdouble2* pd2GradientMagMinMax, VmVObjectVolume* pCVolume, LocalProgress* _progress)
{
	if (pCVolume->GetCustomParameter(("_double2_GraidentMagMinMax"), data_type::dtype<vmdouble2>(), pd2GradientMagMinMax))
		return true;

	const VolumeData* pVolData = pCVolume->GetVolumeData();
	if (pVolData->store_dtype.type_name != data_type::dtype<ushort>().type_name)
		return false;

	if (_progress)
	{
		_progress->range = 70;
	}

	ushort** ppusVolume = (ushort**)pVolData->vol_slices;
	int iSizeAddrX = pVolData->vol_size.x + pVolData->bnd_size.x * 2;
	vmdouble2 d2GradMagMinMax;
	d2GradMagMinMax.x = DBL_MAX;
	d2GradMagMinMax.y = -DBL_MAX;
	for (int iZ = 1; iZ < pVolData->vol_size.z - 1; iZ += 2)
	{
		if (_progress)
		{
			*_progress->progress_ptr =
				(int)(_progress->start + _progress->range*iZ / pVolData->vol_size.z);
		}

		for (int iY = 1; iY < pVolData->vol_size.y - 1; iY += 2)
		{
			for (int iX = 1; iX < pVolData->vol_size.x - 1; iX += 2)
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

	if (_progress)
	{
		_progress->start = _progress->start + _progress->range;
		*_progress->progress_ptr = (_progress->start);
		_progress->range = 30;
	}

	return true;
}

bool grd_helper_legacy::SetCbVrModulation(CB_VrModulation* pCBVrModulation, VmVObjectVolume* pCVObjectVolume, vmfloat3 f3PosEyeWS, VmLObject* pCLObjectForParameters, int obj_param_src_id)
{
	double dGradMagAmplifier = 1.0;
	pCLObjectForParameters->GetDstObjValue(obj_param_src_id, ("_double_GradMagAmplifier"), &dGradMagAmplifier);

	vmdouble2 d2GradientMagMinMax;
	if (!__GradientMagnitudeAnalysis(&d2GradientMagMinMax, pCVObjectVolume, NULL))
		return false;
	float fMaxGradientSize = (float)(d2GradientMagMinMax.y / 65535.0);

	pCBVrModulation->fGradMagAmplifier = (float)dGradMagAmplifier;
	pCBVrModulation->fMaxGradientSize = fMaxGradientSize;

	return true;
}

bool grd_helper_legacy::SetCbVrInterestingRegion(CB_VrInterestingRegion* pCBVrInterestingRegion, VmLObject* pCLObjectForParameters, int obj_param_src_id)
{
	pCBVrInterestingRegion->iNumRegions = 0;
	pCLObjectForParameters->GetDstObjValue(obj_param_src_id, ("_int_NumRoiPoints"), &pCBVrInterestingRegion->iNumRegions);
	if (pCBVrInterestingRegion->iNumRegions == 0)
		return true;

	vmdouble3 d3RoiPoint0WS, d3RoiPoint1WS, d3RoiPoint2WS;
	pCLObjectForParameters->GetDstObjValue(obj_param_src_id, ("_double3_RoiPoint0"), &d3RoiPoint0WS);
	pCLObjectForParameters->GetDstObjValue(obj_param_src_id, ("_double3_RoiPoint1"), &d3RoiPoint1WS);
	pCLObjectForParameters->GetDstObjValue(obj_param_src_id, ("_double3_RoiPoint2"), &d3RoiPoint2WS);
	pCBVrInterestingRegion->f3PosPtn0WS = *(XMFLOAT3*)&vmfloat3(d3RoiPoint0WS);
	pCBVrInterestingRegion->f3PosPtn1WS = *(XMFLOAT3*)&vmfloat3(d3RoiPoint1WS);
	pCBVrInterestingRegion->f3PosPtn2WS = *(XMFLOAT3*)&vmfloat3(d3RoiPoint2WS);

	double dRadius0WS, dRadius1WS, dRadius2WS;
	pCLObjectForParameters->GetDstObjValue(obj_param_src_id, ("_double_RoiRadius0"), &dRadius0WS);
	pCLObjectForParameters->GetDstObjValue(obj_param_src_id, ("_double_RoiRadius1"), &dRadius1WS);
	pCLObjectForParameters->GetDstObjValue(obj_param_src_id, ("_double_RoiRadius2"), &dRadius2WS);
	pCBVrInterestingRegion->fRadius0 = (float)dRadius0WS;
	pCBVrInterestingRegion->fRadius1 = (float)dRadius1WS;
	pCBVrInterestingRegion->fRadius2 = (float)dRadius2WS;

	return true;
}
