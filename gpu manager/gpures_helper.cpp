#include "gpures_helper.h"
#include "D3DCompiler.h"
#include <iostream>

using namespace grd_helper;

#define __BLKLEVEL 1

namespace grd_helper
{
	static GpuDX11CommonParameters* g_pvmCommonParams = NULL;
	static VmGpuManager* g_pCGpuManager = NULL;
}

#include <DirectXColors.h>
#include <DirectXCollision.h>

HRESULT PresetCompiledShader(__ID3D11Device* pdx11Device, HMODULE hModule, LPCWSTR pSrcResource, LPCSTR strShaderProfile, ID3D11DeviceChild** ppdx11Shader/*out*/
	, D3D11_INPUT_ELEMENT_DESC* pInputLayoutDesc, uint num_elements, ID3D11InputLayout** ppdx11LayoutInputVS)
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

		if (pInputLayoutDesc != NULL && num_elements > 0 && ppdx11LayoutInputVS != NULL)
		{
			if (pdx11Device->CreateInputLayout(pInputLayoutDesc, num_elements, pdata, ullFileSize, ppdx11LayoutInputVS) != S_OK)
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

int grd_helper::InitializePresettings(VmGpuManager* pCGpuManager, GpuDX11CommonParameters* gpu_params)
{
	enum RET_TYPE
	{
		__SUPPORTED_GPU = 0
		, __ALREADY_CHECKED = 1
		, __LOW_SPEC_NOT_SUPPORT_CS_GPU = 2
		, __INVALID_GPU = -1
	};

	//return 0;//

	g_pCGpuManager = pCGpuManager;
	g_pvmCommonParams = gpu_params;

	if (g_pvmCommonParams->is_initialized)
	{
		return __ALREADY_CHECKED;
	}

#ifdef USE_DX11_3
	g_pCGpuManager->GetDeviceInformation((void*)(&g_pvmCommonParams->dx11Device), "DEVICE_POINTER_3");
	g_pCGpuManager->GetDeviceInformation((void*)(&g_pvmCommonParams->dx11DeviceImmContext), "DEVICE_CONTEXT_POINTER_3");
#else
	g_pCGpuManager->GetDeviceInformation((void*)(&g_pvmCommonParams->dx11Device), "DEVICE_POINTER");
	g_pCGpuManager->GetDeviceInformation((void*)(&g_pvmCommonParams->dx11DeviceImmContext), "DEVICE_CONTEXT_POINTER");
#endif
	g_pCGpuManager->GetDeviceInformation(&g_pvmCommonParams->dx11_adapter, "DEVICE_ADAPTER_DESC");
	g_pCGpuManager->GetDeviceInformation(&g_pvmCommonParams->dx11_featureLevel, "FEATURE_LEVEL");
	//return __SUPPORTED_GPU;

	if (g_pvmCommonParams->dx11Device == NULL || g_pvmCommonParams->dx11DeviceImmContext == NULL)
	{
		cout << "error devices!" << endl;
		gpu_params = g_pvmCommonParams;
		return (int)__INVALID_GPU;
	}

	g_pvmCommonParams->Delete();
	
	D3D11_QUERY_DESC qr_desc;
	qr_desc.MiscFlags = 0;
	qr_desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
	g_pvmCommonParams->dx11Device->CreateQuery(&qr_desc, &g_pvmCommonParams->dx11qr_disjoint);
	qr_desc.Query = D3D11_QUERY_TIMESTAMP;
	for (int i = 0; i < MAXSTAMPS; i++)
		g_pvmCommonParams->dx11Device->CreateQuery(&qr_desc, &g_pvmCommonParams->dx11qr_timestamps[i]);

	HRESULT hr = S_OK;
	// HLSL 에서 대체하는 방법 찾아 보기.
	{
		D3D11_BLEND_DESC descBlend = {};
		descBlend.AlphaToCoverageEnable = false;
		descBlend.IndependentBlendEnable = false;
		descBlend.RenderTarget[0].BlendEnable = true;
		descBlend.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		descBlend.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		descBlend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;

		descBlend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		descBlend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		descBlend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		descBlend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		ID3D11BlendState* blender_state;
		hr |= g_pvmCommonParams->dx11Device->CreateBlendState(&descBlend, &blender_state);
		g_pvmCommonParams->safe_set_res(COMRES_INDICATOR(BLEND_STATE, "ADD"), blender_state);

		D3D11_RASTERIZER_DESC2 descRaster;
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
		descRaster.AntialiasedLineEnable = FALSE;
		descRaster.ForcedSampleCount = 0;
		descRaster.ConservativeRaster = D3D11_CONSERVATIVE_RASTERIZATION_MODE_OFF;
		ID3D11RasterizerState2* raster_state;
		hr |= g_pvmCommonParams->dx11Device->CreateRasterizerState2(&descRaster, &raster_state);
		g_pvmCommonParams->safe_set_res(COMRES_INDICATOR(RASTERIZER_STATE, "SOLID_CW"), raster_state);
		descRaster.AntialiasedLineEnable = true;
		hr |= g_pvmCommonParams->dx11Device->CreateRasterizerState2(&descRaster, &raster_state);
		g_pvmCommonParams->safe_set_res(COMRES_INDICATOR(RASTERIZER_STATE, "AA_SOLID_CW"), raster_state);
		//descRaster.CullMode = D3D11_CULL_FRONT;
		descRaster.CullMode = D3D11_CULL_BACK;
		descRaster.FrontCounterClockwise = TRUE;
		descRaster.AntialiasedLineEnable = false;
		hr |= g_pvmCommonParams->dx11Device->CreateRasterizerState2(&descRaster, &raster_state);
		g_pvmCommonParams->safe_set_res(COMRES_INDICATOR(RASTERIZER_STATE, "SOLID_CCW"), raster_state);
		descRaster.AntialiasedLineEnable = true;
		hr |= g_pvmCommonParams->dx11Device->CreateRasterizerState2(&descRaster, &raster_state);
		g_pvmCommonParams->safe_set_res(COMRES_INDICATOR(RASTERIZER_STATE, "AA_SOLID_CCW"), raster_state);
		descRaster.CullMode = D3D11_CULL_NONE;
		descRaster.AntialiasedLineEnable = false;
		hr |= g_pvmCommonParams->dx11Device->CreateRasterizerState2(&descRaster, &raster_state);
		g_pvmCommonParams->safe_set_res(COMRES_INDICATOR(RASTERIZER_STATE, "SOLID_NONE"), raster_state);
		descRaster.AntialiasedLineEnable = true;
		hr |= g_pvmCommonParams->dx11Device->CreateRasterizerState2(&descRaster, &raster_state);
		g_pvmCommonParams->safe_set_res(COMRES_INDICATOR(RASTERIZER_STATE, "AA_SOLID_NONE"), raster_state);

		descRaster.FillMode = D3D11_FILL_WIREFRAME;
		descRaster.CullMode = D3D11_CULL_BACK;
		descRaster.ConservativeRaster = D3D11_CONSERVATIVE_RASTERIZATION_MODE_OFF;
		descRaster.AntialiasedLineEnable = false;
		hr |= g_pvmCommonParams->dx11Device->CreateRasterizerState2(&descRaster, &raster_state);
		g_pvmCommonParams->safe_set_res(COMRES_INDICATOR(RASTERIZER_STATE, "WIRE_CW"), raster_state);
		descRaster.AntialiasedLineEnable = true;
		hr |= g_pvmCommonParams->dx11Device->CreateRasterizerState2(&descRaster, &raster_state);
		g_pvmCommonParams->safe_set_res(COMRES_INDICATOR(RASTERIZER_STATE, "AA_WIRE_CW"), raster_state);
		descRaster.CullMode = D3D11_CULL_FRONT;
		descRaster.AntialiasedLineEnable = false;
		hr |= g_pvmCommonParams->dx11Device->CreateRasterizerState2(&descRaster, &raster_state);
		g_pvmCommonParams->safe_set_res(COMRES_INDICATOR(RASTERIZER_STATE, "WIRE_CCW"), raster_state);
		descRaster.AntialiasedLineEnable = true;
		hr |= g_pvmCommonParams->dx11Device->CreateRasterizerState2(&descRaster, &raster_state);
		g_pvmCommonParams->safe_set_res(COMRES_INDICATOR(RASTERIZER_STATE, "AA_WIRE_CCW"), raster_state);
		descRaster.CullMode = D3D11_CULL_NONE;
		descRaster.AntialiasedLineEnable = false;
		hr |= g_pvmCommonParams->dx11Device->CreateRasterizerState2(&descRaster, &raster_state);
		g_pvmCommonParams->safe_set_res(COMRES_INDICATOR(RASTERIZER_STATE, "WIRE_NONE"), raster_state);
		descRaster.AntialiasedLineEnable = true;
		hr |= g_pvmCommonParams->dx11Device->CreateRasterizerState2(&descRaster, &raster_state);
		g_pvmCommonParams->safe_set_res(COMRES_INDICATOR(RASTERIZER_STATE, "AA_WIRE_NONE"), raster_state);
	}
	{
		D3D11_DEPTH_STENCIL_DESC descDepthStencil;
		ZeroMemory(&descDepthStencil, sizeof(D3D11_DEPTH_STENCIL_DESC));
		descDepthStencil.DepthEnable = TRUE;
		descDepthStencil.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		descDepthStencil.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		descDepthStencil.StencilEnable = FALSE;
		ID3D11DepthStencilState* ds_state;
		hr |= g_pvmCommonParams->dx11Device->CreateDepthStencilState(&descDepthStencil, &ds_state);
		g_pvmCommonParams->safe_set_res(COMRES_INDICATOR(DEPTHSTENCIL_STATE, "LESSEQUAL"), ds_state);
		descDepthStencil.DepthFunc = D3D11_COMPARISON_ALWAYS;
		hr |= g_pvmCommonParams->dx11Device->CreateDepthStencilState(&descDepthStencil, &ds_state);
		g_pvmCommonParams->safe_set_res(COMRES_INDICATOR(DEPTHSTENCIL_STATE, "ALWAYS"), ds_state);
		descDepthStencil.DepthFunc = D3D11_COMPARISON_GREATER;
		hr |= g_pvmCommonParams->dx11Device->CreateDepthStencilState(&descDepthStencil, &ds_state);
		g_pvmCommonParams->safe_set_res(COMRES_INDICATOR(DEPTHSTENCIL_STATE, "GREATER"), ds_state);
		descDepthStencil.DepthFunc = D3D11_COMPARISON_LESS;
		hr |= g_pvmCommonParams->dx11Device->CreateDepthStencilState(&descDepthStencil, &ds_state);
		g_pvmCommonParams->safe_set_res(COMRES_INDICATOR(DEPTHSTENCIL_STATE, "LESS"), ds_state);
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
		ID3D11SamplerState* sampler_state;
		hr |= g_pvmCommonParams->dx11Device->CreateSamplerState(&descSampler, &sampler_state);
		g_pvmCommonParams->safe_set_res(COMRES_INDICATOR(SAMPLER_STATE, "POINT_CLAMP"), sampler_state);
		descSampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;	// NEAREST by ROUND
		hr |= g_pvmCommonParams->dx11Device->CreateSamplerState(&descSampler, &sampler_state);
		g_pvmCommonParams->safe_set_res(COMRES_INDICATOR(SAMPLER_STATE, "LINEAR_CLAMP"), sampler_state);

		descSampler.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
		descSampler.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
		descSampler.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
		descSampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;	// NEAREST by ROUND
		hr |= g_pvmCommonParams->dx11Device->CreateSamplerState(&descSampler, &sampler_state);
		g_pvmCommonParams->safe_set_res(COMRES_INDICATOR(SAMPLER_STATE, "LINEAR_ZEROBORDER"), sampler_state);
		descSampler.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;	// NEAREST by ROUND
		hr |= g_pvmCommonParams->dx11Device->CreateSamplerState(&descSampler, &sampler_state);
		g_pvmCommonParams->safe_set_res(COMRES_INDICATOR(SAMPLER_STATE, "POINT_ZEROBORDER"), sampler_state);

		descSampler.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		descSampler.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		descSampler.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		descSampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;	// NEAREST by ROUND
		hr |= g_pvmCommonParams->dx11Device->CreateSamplerState(&descSampler, &sampler_state);
		g_pvmCommonParams->safe_set_res(COMRES_INDICATOR(SAMPLER_STATE, "LINEAR_WRAP"), sampler_state);
		descSampler.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;	// NEAREST by ROUND
		hr |= g_pvmCommonParams->dx11Device->CreateSamplerState(&descSampler, &sampler_state);
		g_pvmCommonParams->safe_set_res(COMRES_INDICATOR(SAMPLER_STATE, "POINT_WRAP"), sampler_state);
	}
	{
		D3D11_BUFFER_DESC descCB;
		descCB.Usage = D3D11_USAGE_DYNAMIC;
		descCB.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		descCB.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		descCB.MiscFlags = NULL;
#define CREATE_AND_SET(STRUCT) {\
		ID3D11Buffer* cbuffer;\
		descCB.ByteWidth = sizeof(STRUCT);\
		descCB.StructureByteStride = sizeof(STRUCT);\
		hr |= g_pvmCommonParams->dx11Device->CreateBuffer(&descCB, NULL, &cbuffer);\
		g_pvmCommonParams->safe_set_cbuf(string(#STRUCT), cbuffer);}\
//CREATE_AND_SET

		CREATE_AND_SET(CB_CameraState);
		CREATE_AND_SET(CB_EnvState);
		CREATE_AND_SET(CB_ClipInfo);
		CREATE_AND_SET(CB_PolygonObject);
		CREATE_AND_SET(CB_VolumeObject);
		CREATE_AND_SET(CB_RenderingEffect);
		CREATE_AND_SET(CB_VolumeRenderingEffect);
		CREATE_AND_SET(CB_TMAP);
		CREATE_AND_SET(MomentOIT);
		CREATE_AND_SET(CB_HotspotMask);
		CREATE_AND_SET(CB_CurvedSlicer);
	}
	if (hr != S_OK)
	{
		cout << "error : basic dx11 resources!" << endl;
		goto ERROR_PRESETTING;
	}

	{
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
		auto register_vertex_shader = [&](const LPCWSTR pSrcResource, const string& name_shader, const string& name_layer, D3D11_INPUT_ELEMENT_DESC in_layout_desc[], uint num_elements)
		{
			ID3D11InputLayout* in_layout = NULL;
			ID3D11VertexShader* vshader = NULL;
			if (PresetCompiledShader(g_pvmCommonParams->dx11Device, hModule, pSrcResource, "vs_5_0", (ID3D11DeviceChild**)&vshader
				, in_layout_desc, num_elements, &in_layout) != S_OK)
			{
				VMSAFE_RELEASE(in_layout);
				VMSAFE_RELEASE(vshader);
				return E_FAIL;
			}
			if(in_layout)
				g_pvmCommonParams->safe_set_res(COMRES_INDICATOR(INPUT_LAYOUT, name_layer), in_layout);
			g_pvmCommonParams->safe_set_res(COMRES_INDICATOR(VERTEX_SHADER, name_shader), vshader);
			return S_OK;
		};
		auto register_shader = [&](const LPCWSTR pSrcResource, const string& name_shader, const string& profile)
		{
			ID3D11DeviceChild* shader = NULL;
			if (PresetCompiledShader(g_pvmCommonParams->dx11Device, hModule, pSrcResource, profile.c_str(), (ID3D11DeviceChild**)&shader, NULL, 0, NULL) != S_OK)
			{
				VMSAFE_RELEASE(shader);
				return E_FAIL;
			}
			GpuhelperResType _type = VERTEX_SHADER;
			if (profile.compare(0, 2, "vs") == 0) _type = VERTEX_SHADER;
			else if (profile.compare(0, 2, "ps") == 0) _type = PIXEL_SHADER;
			else if (profile.compare(0, 2, "gs") == 0) _type = GEOMETRY_SHADER;
			else if (profile.compare(0, 2, "cs") == 0) _type = COMPUTE_SHADER;
			else GMERRORMESSAGE("UN DEFINED SHADER TYPE ! : grd_helper::InitializePresettings");

			g_pvmCommonParams->safe_set_res(COMRES_INDICATOR(_type, name_shader), shader);
			return S_OK;
		};

#define VRETURN(v, ERR) if(v != S_OK) { cout << #ERR << endl; goto ERROR_PRESETTING; }

		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA11001), "SR_OIT_P_vs_5_0", "P", lotypeInputPos, 1), SR_OIT_P_vs_5_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA11002), "SR_OIT_PN_vs_5_0", "PN", lotypeInputPosNor, 2), SR_OIT_PN_vs_5_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA11003), "SR_OIT_PT_vs_5_0", "PT", lotypeInputPosTex, 2), SR_OIT_PT_vs_5_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA11004), "SR_OIT_PNT_vs_5_0", "PNT", lotypeInputPosNorTex, 3), SR_OIT_PNT_vs_5_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA11005), "SR_OIT_PTTT_vs_5_0", "PTTT", lotypeInputPosTTTex, 4), SR_OIT_PTTT_vs_5_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11010), "SR_OIT_FILL_SKBTZ_PHONGBLINN_ps_5_0", "ps_5_0"), SR_OIT_FILL_SKBTZ_PHONGBLINN_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11011), "SR_OIT_FILL_SKBTZ_DASHEDLINE_ps_5_0", "ps_5_0"), SR_OIT_FILL_SKBTZ_DASHEDLINE_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11012), "SR_OIT_FILL_SKBTZ_MULTITEXTMAPPING_ps_5_0", "ps_5_0"), SR_OIT_FILL_SKBTZ_MULTITEXTMAPPING_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11013), "SR_OIT_FILL_SKBTZ_TEXTMAPPING_ps_5_0", "ps_5_0"), SR_OIT_FILL_SKBTZ_TEXTMAPPING_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11014), "SR_OIT_FILL_SKBTZ_TEXTUREIMGMAP_ps_5_0", "ps_5_0"), SR_OIT_FILL_SKBTZ_TEXTUREIMGMAP_ps_5_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11050), "SR_OIT_FILL_SKBTZ_PHONGBLINN_ROV_ps_5_0", "ps_5_0"), SR_OIT_FILL_SKBTZ_PHONGBLINN_ROV_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11051), "SR_OIT_FILL_SKBTZ_DASHEDLINE_ROV_ps_5_0", "ps_5_0"), SR_OIT_FILL_SKBTZ_DASHEDLINE_ROV_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11052), "SR_OIT_FILL_SKBTZ_MULTITEXTMAPPING_ROV_ps_5_0", "ps_5_0"), SR_OIT_FILL_SKBTZ_MULTITEXTMAPPING_ROV_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11053), "SR_OIT_FILL_SKBTZ_TEXTMAPPING_ROV_ps_5_0", "ps_5_0"), SR_OIT_FILL_SKBTZ_TEXTMAPPING_ROV_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11054), "SR_OIT_FILL_SKBTZ_TEXTUREIMGMAP_ROV_ps_5_0", "ps_5_0"), SR_OIT_FILL_SKBTZ_TEXTUREIMGMAP_ROV_ps_5_0);
		
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA12010), "SR_OIT_FILL_SKBT_PHONGBLINN_ps_5_0", "ps_5_0"), SR_OIT_FILL_SKBT_PHONGBLINN_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA12011), "SR_OIT_FILL_SKBT_DASHEDLINE_ps_5_0", "ps_5_0"), SR_OIT_FILL_SKBT_DASHEDLINE_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA12012), "SR_OIT_FILL_SKBT_MULTITEXTMAPPING_ps_5_0", "ps_5_0"), SR_OIT_FILL_SKBT_MULTITEXTMAPPING_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA12013), "SR_OIT_FILL_SKBT_TEXTMAPPING_ps_5_0", "ps_5_0"), SR_OIT_FILL_SKBT_TEXTMAPPING_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA12014), "SR_OIT_FILL_SKBT_TEXTUREIMGMAP_ps_5_0", "ps_5_0"), SR_OIT_FILL_SKBT_TEXTUREIMGMAP_ps_5_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA12050), "SR_OIT_FILL_SKBT_PHONGBLINN_ROV_ps_5_0", "ps_5_0"), SR_OIT_FILL_SKBT_PHONGBLINN_ROV_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA12051), "SR_OIT_FILL_SKBT_DASHEDLINE_ROV_ps_5_0", "ps_5_0"), SR_OIT_FILL_SKBT_DASHEDLINE_ROV_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA12052), "SR_OIT_FILL_SKBT_MULTITEXTMAPPING_ROV_ps_5_0", "ps_5_0"), SR_OIT_FILL_SKBT_MULTITEXTMAPPING_ROV_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA12053), "SR_OIT_FILL_SKBT_TEXTMAPPING_ROV_ps_5_0", "ps_5_0"), SR_OIT_FILL_SKBT_TEXTMAPPING_ROV_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA12054), "SR_OIT_FILL_SKBT_TEXTUREIMGMAP_ROV_ps_5_0", "ps_5_0"), SR_OIT_FILL_SKBT_TEXTUREIMGMAP_ROV_ps_5_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11015), "SR_SINGLE_LAYER_ps_5_0", "ps_5_0"), SR_SINGLE_LAYER_ps_5_0);
		//VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11016), "SR_OIT_KDEPTH_NPRGHOST_ps_5_0", "ps_5_0"));

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11110), "SR_OIT_FILL_DKBTZ_PHONGBLINN_ps_5_0", "ps_5_0"), SR_OIT_FILL_DKBTZ_PHONGBLINN_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11111), "SR_OIT_FILL_DKBTZ_DASHEDLINE_ps_5_0", "ps_5_0"), SR_OIT_FILL_DKBTZ_DASHEDLINE_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11112), "SR_OIT_FILL_DKBTZ_MULTITEXTMAPPING_ps_5_0", "ps_5_0"), SR_OIT_FILL_DKBTZ_MULTITEXTMAPPING_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11113), "SR_OIT_FILL_DKBTZ_TEXTMAPPING_ps_5_0", "ps_5_0"), SR_OIT_FILL_DKBTZ_TEXTMAPPING_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11114), "SR_OIT_FILL_DKBTZ_TEXTUREIMGMAP_ps_5_0", "ps_5_0"), SR_OIT_FILL_DKBTZ_TEXTUREIMGMAP_ps_5_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11150), "SR_OIT_FILL_DKBTZ_PHONGBLINN_ROV_ps_5_0", "ps_5_0"), SR_OIT_FILL_DKBTZ_PHONGBLINN_ROV_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11151), "SR_OIT_FILL_DKBTZ_DASHEDLINE_ROV_ps_5_0", "ps_5_0"), SR_OIT_FILL_DKBTZ_DASHEDLINE_ROV_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11152), "SR_OIT_FILL_DKBTZ_MULTITEXTMAPPING_ROV_ps_5_0", "ps_5_0"), SR_OIT_FILL_DKBTZ_MULTITEXTMAPPING_ROV_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11153), "SR_OIT_FILL_DKBTZ_TEXTMAPPING_ROV_ps_5_0", "ps_5_0"), SR_OIT_FILL_DKBTZ_TEXTMAPPING_ROV_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11154), "SR_OIT_FILL_DKBTZ_TEXTUREIMGMAP_ROV_ps_5_0", "ps_5_0"), SR_OIT_FILL_DKBTZ_TEXTUREIMGMAP_ROV_ps_5_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11120), "SR_OIT_FILL_DKBT_PHONGBLINN_ps_5_0", "ps_5_0"), SR_OIT_FILL_DKBT_PHONGBLINN_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11121), "SR_OIT_FILL_DKBT_DASHEDLINE_ps_5_0", "ps_5_0"), SR_OIT_FILL_DKBT_DASHEDLINE_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11122), "SR_OIT_FILL_DKBT_MULTITEXTMAPPING_ps_5_0", "ps_5_0"), SR_OIT_FILL_DKBT_MULTITEXTMAPPING_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11123), "SR_OIT_FILL_DKBT_TEXTMAPPING_ps_5_0", "ps_5_0"), SR_OIT_FILL_DKBT_TEXTMAPPING_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11124), "SR_OIT_FILL_DKBT_TEXTUREIMGMAP_ps_5_0", "ps_5_0"), SR_OIT_FILL_DKBT_TEXTUREIMGMAP_ps_5_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11170), "SR_OIT_FILL_DKBT_PHONGBLINN_ROV_ps_5_0", "ps_5_0"), SR_OIT_FILL_DKBT_PHONGBLINN_ROV_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11171), "SR_OIT_FILL_DKBT_DASHEDLINE_ROV_ps_5_0", "ps_5_0"), SR_OIT_FILL_DKBT_DASHEDLINE_ROV_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11172), "SR_OIT_FILL_DKBT_MULTITEXTMAPPING_ROV_ps_5_0", "ps_5_0"), SR_OIT_FILL_DKBT_MULTITEXTMAPPING_ROV_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11173), "SR_OIT_FILL_DKBT_TEXTMAPPING_ROV_ps_5_0", "ps_5_0"), SR_OIT_FILL_DKBT_TEXTMAPPING_ROV_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11174), "SR_OIT_FILL_DKBT_TEXTUREIMGMAP_ROV_ps_5_0", "ps_5_0"), SR_OIT_FILL_DKBT_TEXTUREIMGMAP_ROV_ps_5_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA21050), "SR_FillHistogram_cs_5_0", "cs_5_0"), SR_FillHistogram_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA21051), "SR_CreateOffsetTableKpB_cs_5_0", "cs_5_0"), SR_CreateOffsetTableKpB_cs_5_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11020), "SR_OIT_ABUFFER_FRAGCOUNTER_ps_5_0", "ps_5_0"), SR_OIT_ABUFFER_FRAGCOUNTER_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11026), "SR_OIT_ABUFFER_FRAGCOUNTER_MTT_ps_5_0", "ps_5_0"), SR_OIT_ABUFFER_FRAGCOUNTER_MTT_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11027), "SR_SINGLE_LAYER_TO_DFB_cs_5_0", "cs_5_0"), SR_SINGLE_LAYER_TO_DFB_cs_5_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11021), "SR_OIT_ABUFFER_PHONGBLINN_ps_5_0", "ps_5_0"), SR_OIT_ABUFFER_PHONGBLINN_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11022), "SR_OIT_ABUFFER_DASHEDLINE_ps_5_0", "ps_5_0"), SR_OIT_ABUFFER_DASHEDLINE_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11023), "SR_OIT_ABUFFER_MULTITEXTMAPPING_ps_5_0", "ps_5_0"), SR_OIT_ABUFFER_MULTITEXTMAPPING_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11024), "SR_OIT_ABUFFER_TEXTMAPPING_ps_5_0", "ps_5_0"), SR_OIT_ABUFFER_TEXTMAPPING_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11025), "SR_OIT_ABUFFER_TEXTUREIMGMAP_ps_5_0", "ps_5_0"), SR_OIT_ABUFFER_TEXTUREIMGMAP_ps_5_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11321), "PICKING_ABUFFER_PHONGBLINN_ps_5_0", "ps_5_0"), PICKING_ABUFFER_PHONGBLINN_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11322), "PICKING_ABUFFER_DASHEDLINE_ps_5_0", "ps_5_0"), PICKING_ABUFFER_DASHEDLINE_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11323), "PICKING_ABUFFER_MULTITEXTMAPPING_ps_5_0", "ps_5_0"), PICKING_ABUFFER_MULTITEXTMAPPING_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11324), "PICKING_ABUFFER_TEXTMAPPING_ps_5_0", "ps_5_0"), PICKING_ABUFFER_TEXTMAPPING_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11325), "PICKING_ABUFFER_TEXTUREIMGMAP_ps_5_0", "ps_5_0"), PICKING_ABUFFER_TEXTUREIMGMAP_ps_5_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11101), "SR_OIT_ABUFFER_PREFIX_0_cs_5_0", "cs_5_0"), SR_OIT_ABUFFER_PREFIX_0_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11102), "SR_OIT_ABUFFER_PREFIX_1_cs_5_0", "cs_5_0"), SR_OIT_ABUFFER_PREFIX_1_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11103), "SR_OIT_ABUFFER_OffsetTable_cs_5_0", "cs_5_0"), SR_OIT_ABUFFER_OffsetTable_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11104), "SR_OIT_ABUFFER_SORT2SENDER_cs_5_0", "cs_5_0"), SR_OIT_ABUFFER_SORT2SENDER_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11105), "SR_OIT_ABUFFER_SORT2SENDER_SFM_cs_5_0", "cs_5_0"), SR_OIT_ABUFFER_SORT2SENDER_SFM_cs_5_0);

		{
			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11030), "SR_MOMENT_GEN_ps_5_0", "ps_5_0"), SR_MOMENT_GEN_ps_5_0);
			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11037), "SR_MOMENT_GEN_TEXT_ps_5_0", "ps_5_0"), SR_MOMENT_GEN_TEXT_ps_5_0);
			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11036), "SR_MOMENT_GEN_MTT_ps_5_0", "ps_5_0"), SR_MOMENT_GEN_MTT_ps_5_0);
			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11031), "SR_MOMENT_OIT_PHONGBLINN_ps_5_0", "ps_5_0"), SR_MOMENT_OIT_PHONGBLINN_ps_5_0);
			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11032), "SR_MOMENT_OIT_DASHEDLINE_ps_5_0", "ps_5_0"), SR_MOMENT_OIT_DASHEDLINE_ps_5_0);
			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11033), "SR_MOMENT_OIT_MULTITEXTMAPPING_ps_5_0", "ps_5_0"), SR_MOMENT_OIT_MULTITEXTMAPPING_ps_5_0);
			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11034), "SR_MOMENT_OIT_TEXTMAPPING_ps_5_0", "ps_5_0"), SR_MOMENT_OIT_TEXTMAPPING_ps_5_0);
			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11035), "SR_MOMENT_OIT_TEXTUREIMGMAP_ps_5_0", "ps_5_0"), SR_MOMENT_OIT_TEXTUREIMGMAP_ps_5_0);

			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11070), "SR_MOMENT_GEN_ROV_ps_5_0", "ps_5_0"), SR_MOMENT_GEN_ROV_ps_5_0);
			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11077), "SR_MOMENT_GEN_TEXT_ROV_ps_5_0", "ps_5_0"), SR_MOMENT_GEN_TEXT_ROV_ps_5_0);
			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11076), "SR_MOMENT_GEN_MTT_ROV_ps_5_0", "ps_5_0"), SR_MOMENT_GEN_MTT_ROV_ps_5_0);
			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11071), "SR_MOMENT_OIT_PHONGBLINN_ROV_ps_5_0", "ps_5_0"), SR_MOMENT_OIT_PHONGBLINN_ROV_ps_5_0);
			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11072), "SR_MOMENT_OIT_DASHEDLINE_ROV_ps_5_0", "ps_5_0"), SR_MOMENT_OIT_DASHEDLINE_ROV_ps_5_0);
			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11073), "SR_MOMENT_OIT_MULTITEXTMAPPING_ROV_ps_5_0", "ps_5_0"), SR_MOMENT_OIT_MULTITEXTMAPPING_ROV_ps_5_0);
			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11074), "SR_MOMENT_OIT_TEXTMAPPING_ROV_ps_5_0", "ps_5_0"), SR_MOMENT_OIT_TEXTMAPPING_ROV_ps_5_0);
			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11075), "SR_MOMENT_OIT_TEXTUREIMGMAP_ROV_ps_5_0", "ps_5_0"), SR_MOMENT_OIT_TEXTUREIMGMAP_ROV_ps_5_0);



			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA21100), "KB_SSAO_FM_cs_5_0", "cs_5_0"), KB_SSAO_FM_cs_5_0);
			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA21101), "KB_SSAO_BLUR_FM_cs_5_0", "cs_5_0"), KB_SSAO_BLUR_FM_cs_5_0);
			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA21102), "KB_TO_TEXTURE_FM_cs_5_0", "cs_5_0"), KB_TO_TEXTURE_FM_cs_5_0);

			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA21110), "KB_SSAO_cs_5_0", "cs_5_0"), KB_SSAO_cs_5_0);
			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA21111), "KB_SSAO_BLUR_cs_5_0", "cs_5_0"), KB_SSAO_BLUR_cs_5_0);
			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA21112), "KB_TO_TEXTURE_cs_5_0", "cs_5_0"), KB_TO_TEXTURE_cs_5_0);

			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA21105), "KB_MINMAXTEXTURE_FM_cs_5_0", "cs_5_0"), KB_MINMAXTEXTURE_cs_5_0);
			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA21106), "KB_SSDOF_RT_FM_cs_5_0", "cs_5_0"), KB_SSDOF_RT_cs_5_0);
			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA21107), "KB_MINMAX_NBUF_FM_cs_5_0", "cs_5_0"), KB_MINMAX_NBUF_cs_5_0);

			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA21115), "KB_MINMAXTEXTURE_cs_5_0", "cs_5_0"), KB_MINMAXTEXTURE_cs_5_0);
			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA21116), "KB_SSDOF_RT_cs_5_0", "cs_5_0"), KB_SSDOF_RT_cs_5_0);
			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA21117), "KB_MINMAX_NBUF_cs_5_0", "cs_5_0"), KB_MINMAX_NBUF_cs_5_0);
		}

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA21000), "OIT_SKBZ_RESOLVE_cs_5_0", "cs_5_0"), OIT_SKBZ_RESOLVE_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA21002), "OIT_SKB_RESOLVE_cs_5_0", "cs_5_0"), OIT_SKB_RESOLVE_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA21001), "OIT_DKBZ_RESOLVE_cs_5_0", "cs_5_0"), OIT_DKBZ_RESOLVE_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA21005), "OIT_DKB_RESOLVE_cs_5_0", "cs_5_0"), OIT_DKB_RESOLVE_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA21003), "SR_SINGLE_LAYER_TO_SKBTZ_cs_5_0", "cs_5_0"), SR_SINGLE_LAYER_TO_SKBTZ_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA21006), "SR_SINGLE_LAYER_TO_SKBT_cs_5_0", "cs_5_0"), SR_SINGLE_LAYER_TO_SKBT_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA21004), "SR_SINGLE_LAYER_TO_DKBTZ_cs_5_0", "cs_5_0"), SR_SINGLE_LAYER_TO_DKBTZ_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA21007), "SR_SINGLE_LAYER_TO_DKBT_cs_5_0", "cs_5_0"), SR_SINGLE_LAYER_TO_DKBT_cs_5_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA31010), "GS_ThickPoints_gs_5_0", "gs_5_0"), GS_ThickPoints_gs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA31011), "GS_SurfelPoints_gs_5_0", "gs_5_0"), GS_SurfelPoints_gs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA31020), "GS_ThickLines_gs_5_0", "gs_5_0"), GS_ThickLines_gs_5_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50000), "VR_RAYMAX_cs_5_0", "cs_5_0"), VR_RAYMAX_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50001), "VR_RAYMIN_cs_5_0", "cs_5_0"), VR_RAYMIN_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50002), "VR_RAYSUM_cs_5_0", "cs_5_0"), VR_RAYSUM_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50006), "VR_SURFACE_cs_5_0", "cs_5_0"), VR_SURFACE_cs_5_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50003), "VR_DEFAULT_FM_cs_5_0", "cs_5_0"), VR_DEFAULT_FM_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50004), "VR_OPAQUE_FM_cs_5_0", "cs_5_0"), VR_OPAQUE_FM_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50005), "VR_CONTEXT_FM_cs_5_0", "cs_5_0"), VR_CONTEXT_FM_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50011), "VR_MULTIOTF_FM_cs_5_0", "cs_5_0"), VR_MULTIOTF_FM_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50041), "VR_MULTIOTF_CONTEXT_FM_cs_5_0", "cs_5_0"), VR_MULTIOTF_CONTEXT_FM_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50012), "VR_MASKVIS_FM_cs_5_0", "cs_5_0"), VR_MASKVIS_FM_cs_5_0);
		
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50013), "VR_DEFAULT_cs_5_0", "cs_5_0"), VR_DEFAULT_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50014), "VR_OPAQUE_cs_5_0", "cs_5_0"), VR_OPAQUE_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50015), "VR_CONTEXT_cs_5_0", "cs_5_0"), VR_CONTEXT_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50016), "VR_MULTIOTF_cs_5_0", "cs_5_0"), VR_MULTIOTF_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50017), "VR_MASKVIS_cs_5_0", "cs_5_0"), VR_MASKVIS_cs_5_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50020), "VR_DEFAULT_DKBZ_cs_5_0", "cs_5_0"), VR_DEFAULT_DKBZ_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50021), "VR_OPAQUE_DKBZ_cs_5_0", "cs_5_0"), VR_OPAQUE_DKBZ_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50022), "VR_CONTEXT_DKBZ_cs_5_0", "cs_5_0"), VR_CONTEXT_DKBZ_cs_5_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50030), "VR_DEFAULT_DFB_cs_5_0", "cs_5_0"), VR_DEFAULT_DFB_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50031), "VR_OPAQUE_DFB_cs_5_0", "cs_5_0"), VR_OPAQUE_DFB_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50032), "VR_CONTEXT_DFB_cs_5_0", "cs_5_0"), VR_CONTEXT_DFB_cs_5_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA60001), "PanoVR_RAYMAX_cs_5_0", "cs_5_0"), PanoVR_RAYMAX_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA60002), "PanoVR_RAYMIN_cs_5_0", "cs_5_0"), PanoVR_RAYMIN_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA60003), "PanoVR_RAYSUM_cs_5_0", "cs_5_0"), PanoVR_RAYSUM_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA60004), "PanoVR_DEFAULT_cs_5_0", "cs_5_0"), PanoVR_DEFAULT_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA60005), "PanoVR_MODULATE_cs_5_0", "cs_5_0"), PanoVR_MODULATE_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA60006), "PanoVR_MULTIOTF_DEFAULT_cs_5_0", "cs_5_0"), PanoVR_MULTIOTF_DEFAULT_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA60007), "PanoVR_MULTIOTF_MODULATE_cs_5_0", "cs_5_0"), PanoVR_MULTIOTF_MODULATE_cs_5_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA60100), "ThickSlicePathTracer_cs_5_0", "cs_5_0"), ThickSlicePathTracer_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA60101), "CurvedThickSlicePathTracer_cs_5_0", "cs_5_0"), CurvedThickSlicePathTracer_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA60102), "SliceOutline_cs_5_0", "cs_5_0"), SliceOutline_cs_5_0);
	}

	g_pvmCommonParams->is_initialized = true;
	gpu_params = g_pvmCommonParams;

	if (hr != S_OK)
		return __INVALID_GPU;

	if (g_pvmCommonParams->dx11_featureLevel == 0x9300 || g_pvmCommonParams->dx11_featureLevel == 0xA000)
		return __LOW_SPEC_NOT_SUPPORT_CS_GPU;

	return __SUPPORTED_GPU;

ERROR_PRESETTING:
	DeinitializePresettings();
	gpu_params = g_pvmCommonParams;
	return __INVALID_GPU;
}

void grd_helper::DeinitializePresettings()
{
	if (g_pvmCommonParams == NULL) return;
	if (!g_pvmCommonParams->is_initialized) return;

	if (g_pvmCommonParams->dx11DeviceImmContext)
	{
		g_pvmCommonParams->dx11DeviceImmContext->Flush();
		g_pvmCommonParams->dx11DeviceImmContext->ClearState();
	}

	g_pvmCommonParams->Delete();

	if (g_pvmCommonParams->dx11DeviceImmContext)
	{
		g_pvmCommonParams->dx11DeviceImmContext->Flush();
		g_pvmCommonParams->dx11DeviceImmContext->ClearState();
	}

	g_pvmCommonParams->dx11Device = NULL;
	g_pvmCommonParams->dx11DeviceImmContext = NULL;
	memset(&g_pvmCommonParams->dx11_adapter, NULL, sizeof(DXGI_ADAPTER_DESC));
	g_pvmCommonParams->dx11_featureLevel = D3D_FEATURE_LEVEL_9_1;
	g_pvmCommonParams->is_initialized = false;
}

int __UpdateBlocks(GpuRes& gres, const VmVObjectVolume* vobj, const string& vmode, LocalProgress* progress)
{
	gres.vm_src_id = vobj->GetObjectID();
	gres.res_name = string("VBLOCKS_MODE_") + vmode;

	if (g_pCGpuManager->UpdateGpuResource(gres))
		return 0;

	gres.rtype = RTYPE_TEXTURE3D;
	gres.options["USAGE"] = D3D11_USAGE_DYNAMIC;
	gres.options["CPU_ACCESS_FLAG"] = D3D11_CPU_ACCESS_WRITE;
	gres.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE;

	VolumeData* vol_data = ((VmVObjectVolume*)vobj)->GetVolumeData();
	const int blk_level = __BLKLEVEL;	// 0 : High Resolution, 1 : Low Resolution
	VolumeBlocks* volblk = ((VmVObjectVolume*)vobj)->GetVolumeBlock(blk_level);

	gres.res_values.SetParam("WIDTH", (uint)volblk->blk_vol_size.x);
	gres.res_values.SetParam("HEIGHT", (uint)volblk->blk_vol_size.y);
	gres.res_values.SetParam("DEPTH", (uint)volblk->blk_vol_size.z);

	uint num_blk_units = volblk->blk_vol_size.x * volblk->blk_vol_size.y * volblk->blk_vol_size.z;
	gres.options["FORMAT"] = (vmode.find("OTFTAG") != string::npos) && num_blk_units >= 65535 ? DXGI_FORMAT_R32_FLOAT : DXGI_FORMAT_R16_UNORM;

	//if (vmode.find("OTFTAG") != string::npos)
	//	cout << vmode << endl;

	g_pCGpuManager->GenerateGpuResource(gres);

	return 1;
}

bool grd_helper::UpdateOtfBlocks(GpuRes& gres, VmVObjectVolume* main_vobj, VmVObjectVolume* mask_vobj,
	VmObject* tobj, const int sculpt_value, LocalProgress* progress)
{
	int tobj_id = tobj->GetObjectID();

	int is_new = __UpdateBlocks(gres, main_vobj, string("OTFTAG_") + to_string(tobj_id), progress);
	
	vmdouble2 otf_Mm_range = vmdouble2(DBL_MAX, -DBL_MAX);
	MapTable* tmap_data = tobj->GetObjParamPtr<MapTable>("_TableMap_OTF");


	VolumeData* vol_data = main_vobj->GetVolumeData();
	float value_range = 65535.f;
	if (vol_data->store_dtype.type_bytes == data_type::dtype<byte>().type_bytes) 
		value_range = 255.f;
	else assert(vol_data->store_dtype.type_bytes == data_type::dtype<ushort>().type_bytes); 

	float scale_tf2volume = value_range / (float)tmap_data->array_lengths.x;

	otf_Mm_range.x = tmap_data->valid_min_idx.x * scale_tf2volume;
	otf_Mm_range.y = tmap_data->valid_max_idx.x * scale_tf2volume;

	const int blk_level = __BLKLEVEL;	// 0 : High Resolution, 1 : Low Resolution
	VolumeBlocks* volblk = ((VmVObjectVolume*)main_vobj)->GetVolumeBlock(blk_level);

	ullong _tob_time_cpu = tobj->GetContentUpdateTime();
	ullong _blk_time_cpu = volblk->GetUpdateTime(tobj_id);

	if (_tob_time_cpu < _blk_time_cpu) {
		if (mask_vobj == NULL) 
			return true;
		else {
			int last_sculpt_index = mask_vobj->GetObjParam("_int_LastSculptIndex", (int)0);
			if (sculpt_value == last_sculpt_index) 
				return true;
		}
	}
	if(mask_vobj)
		mask_vobj->SetObjParam("_int_LastSculptIndex", sculpt_value);
	
	// MAIN UPDATE!
	((VmVObjectVolume*)main_vobj)->UpdateTagBlocks(tobj_id, 0, otf_Mm_range, NULL); // update volblk->GetUpdateTime(tobj_id)
	((VmVObjectVolume*)main_vobj)->UpdateTagBlocks(tobj_id, 1, otf_Mm_range, NULL); // update volblk->GetUpdateTime(tobj_id)
	byte* tag_blks = volblk->GetTaggedActivatedBlocks(tobj_id); // set by OTF values

	//if (use_mask_otf && mask_tmap_ids && num_mask_tmap_ids > 0 && mask_vobj) // Only Cares for Main Block of iLevelBlock
	//{
	//	VolumeBlocks* mask_volblk = ((VmVObjectVolume*)mask_vobj)->GetVolumeBlock(blk_level);
	//
	//	vmint3 blk_vol_size = mask_volblk->blk_vol_size;
	//	vmint3 blk_bnd_size = mask_volblk->blk_bnd_size;
	//	int num_blk_units_x = blk_vol_size.x + blk_bnd_size.x * 2;
	//	int num_blk_units_y = blk_vol_size.y + blk_bnd_size.y * 2;
	//	int num_blk_units_z = blk_vol_size.z + blk_bnd_size.z * 2;
	//	int num_blks = num_blk_units_x * num_blk_units_y * num_blk_units_z;
	//
	//	vmbyte2* mask_mM_blk_values = (vmbyte2*)mask_volblk->mM_blks;
	//	for (int j = 1; j < tmap_data->array_lengths.y; j++)
	//	{
	//		ushort usMinOtf = (ushort)min(max(pstTfArchiveMask->valid_min_idx.x, 0), 65535);
	//		ushort usMaxOtf = (ushort)max(min(pstTfArchiveMask->valid_max_idx.x, 65535), 0);
	//
	//		vmushort2* pus2MinMaxMainVolBlocks = (vmushort2*)volblk->mM_blks;
	//
	//		for (int iBlkIndex = 0; iBlkIndex < num_blks; iBlkIndex++)
	//		{
	//			//if (iSculptValue == 0 || iSculptValue > iSculptIndex)
	//			vmbyte2 y2MinMaxBlockValue = mask_mM_blk_values[iBlkIndex];
	//			if (y2MinMaxBlockValue.y >= j)
	//			{
	//				vmushort2 us2MinMaxVolBlockValue = pus2MinMaxMainVolBlocks[iBlkIndex];
	//				if (us2MinMaxVolBlockValue.y < usMinOtf || us2MinMaxVolBlockValue.x > usMaxOtf)
	//					tag_blks[iBlkIndex] = 0;
	//				else
	//					tag_blks[iBlkIndex] = 1;
	//			}
	//		}
	//	}
	//}

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
	HRESULT hr = g_pvmCommonParams->dx11DeviceImmContext->Map(pdx11tx3d_blkmap, 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes);

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

	g_pvmCommonParams->dx11DeviceImmContext->Unmap(pdx11tx3d_blkmap, 0);
	if (progress)
		*progress->progress_ptr = (progress->start + progress->range);
	return true;
}

bool grd_helper::UpdateMinMaxBlocks(GpuRes& gres_min, GpuRes& gres_max, const VmVObjectVolume* vobj, LocalProgress* progress)
{
	int is_new1 = __UpdateBlocks(gres_min, vobj, "VMIN", progress);
	int is_new2 = __UpdateBlocks(gres_max, vobj, "VMAX", progress);
	if (is_new1 == 0 && is_new2 == 0) return true;

	const VolumeData* vol_data = ((VmVObjectVolume*)vobj)->GetVolumeData();
	const int blk_level = __BLKLEVEL;	// 0 : High Resolution, 1 : Low Resolution
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
	HRESULT hr = g_pvmCommonParams->dx11DeviceImmContext->Map(pdx11tx3d_min_blk, 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes_Min);
	hr |= g_pvmCommonParams->dx11DeviceImmContext->Map(pdx11tx3d_max_blk, 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes_Max);

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
				vmushort2 mM_v  = mM_blk_values[(x + blk_bnd_size.x)
					+ (y + blk_bnd_size.y) * blk_sample_width.x + (z + blk_bnd_size.z) * blk_sample_slice];

				int iAddrBlockInGpu = x + y * d11MappedRes_Max.RowPitch / 2 + z * d11MappedRes_Max.DepthPitch / 2;
				min_data[iAddrBlockInGpu] = mM_v.x;
				max_data[iAddrBlockInGpu] = mM_v.y;
			}
	}
	g_pvmCommonParams->dx11DeviceImmContext->Unmap(pdx11tx3d_min_blk, 0);
	g_pvmCommonParams->dx11DeviceImmContext->Unmap(pdx11tx3d_max_blk, 0);
	if (progress)
		*progress->progress_ptr = (progress->start + progress->range);
	return true;
}

auto GetOption = [](const GpuRes& gres, const std::string& flag_name) -> uint
{
	auto it = gres.options.find(flag_name);
	if (it == gres.options.end()) return 0;
	return it->second;
};

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


bool grd_helper::UpdateVolumeModel(GpuRes& gres, VmVObjectVolume* vobj, const bool use_nearest_max, LocalProgress* progress)
{
	bool is_nearest_max_vol = vobj->GetObjParam("_bool_UseNearestMax", use_nearest_max);

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

	double half_criteria_KB = vobj->GetObjParam("_float_ForcedHalfCriterionKB", (double)(512.0 * 1024.0));
	half_criteria_KB = min(max(16.0 * 1024.0, half_criteria_KB), 256.0 * 1024.0);

	//////////////////////////////
	// GPU Volume Sample Policy //
	vmfloat3 sample_offset = vmfloat3(1.f, 1.f, 1.f);
	vmdouble3 dvol_size;
	dvol_size.x = vol_data->vol_size.x;
	dvol_size.y = vol_data->vol_size.y;
	dvol_size.z = vol_data->vol_size.z;

	double type_size = 2.0; // forced to 2 for using the same volume size of mask and main instead of using vol_data->store_dtype.type_bytes;

	//if (use_nearest_max) half_criteria_KB /= 8;

	double dsize_vol_KB = dvol_size.x * dvol_size.y * dvol_size.z / 1024.0 * type_size;
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
	gres.res_values.SetParam("WIDTH", (uint)vol_size_x);
	gres.res_values.SetParam("HEIGHT", (uint)vol_size_y);
	gres.res_values.SetParam("DEPTH", (uint)vol_size_z);
	gres.res_values.SetParam("SAMPLE_OFFSET_X", (float)sample_offset.x);
	gres.res_values.SetParam("SAMPLE_OFFSET_Y", (float)sample_offset.y);
	gres.res_values.SetParam("SAMPLE_OFFSET_Z", (float)sample_offset.z);

	cout << "***************>> " << vol_size_x << ", " << vol_size_y << ", " << vol_size_z << endl;

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
		vol_size_x * vol_size_y * vol_size_z / 1024 * type_size,
		vol_size_x, vol_size_y, vol_size_z, type_size);

	cout << "************offset*>> " << sample_offset.x << ", " << sample_offset.y << ", " << sample_offset.z << endl;

	g_pCGpuManager->GenerateGpuResource(gres);

	ID3D11Texture3D* pdx11tx3d_volume = (ID3D11Texture3D*)gres.alloc_res_ptrs[DTYPE_RES];
	D3D11_MAPPED_SUBRESOURCE d11MappedRes;
	HRESULT hr = g_pvmCommonParams->dx11DeviceImmContext->Map(pdx11tx3d_volume, 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes);
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
	g_pvmCommonParams->dx11DeviceImmContext->Unmap(pdx11tx3d_volume, 0);

	return true;
}

bool grd_helper::UpdateTMapBuffer(GpuRes& gres, VmObject* tobj, const bool isPreInt, LocalProgress* progress)
{
	gres.vm_src_id = tobj->GetObjectID();
	gres.res_name = isPreInt? string("PREINT_OTF_BUFFER") : string("OTF_BUFFER");

	MapTable* tmap_data = tobj->GetObjParamPtr<MapTable>("_TableMap_OTF");
	string updateTimeName = string("_ullong_Latest") + string(isPreInt? "PreIntOtf" : "Otf") + string("GpuUpdateTime");
	if (g_pCGpuManager->UpdateGpuResource(gres)) {

		ullong _tp_cpu = tobj->GetContentUpdateTime(); 
		ullong _tp_gpu = tobj->GetObjParam(updateTimeName, (ullong)0);
		if(_tp_gpu >= _tp_cpu)
			return true;
	}
	else {
		gres.rtype = RTYPE_BUFFER;
		gres.options["USAGE"] = D3D11_USAGE_DYNAMIC;
		gres.options["CPU_ACCESS_FLAG"] = D3D11_CPU_ACCESS_WRITE;
		gres.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE;
		gres.options["FORMAT"] = isPreInt? DXGI_FORMAT_R32G32B32A32_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;

		gres.res_values.SetParam("NUM_ELEMENTS", (uint)(tmap_data->array_lengths.x * (tmap_data->array_lengths.y)));
		gres.res_values.SetParam("STRIDE_BYTES", isPreInt? (uint)16 : (uint)tmap_data->dtype.type_bytes);

		g_pCGpuManager->GenerateGpuResource(gres);
	}

	D3D11_MAPPED_SUBRESOURCE d11MappedRes;
	g_pvmCommonParams->dx11DeviceImmContext->Map((ID3D11Resource*)gres.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes);
	if (isPreInt) {
		vmfloat4* f4ColorPreIntTF = (vmfloat4*)d11MappedRes.pData;

#pragma omp parallel for 
		for (int i = 0; i < tmap_data->array_lengths.y; i++)
		{
			vmbyte4* py4OTF = (vmbyte4*)tmap_data->tmap_buffers[i];
			vmbyte4 rgba = py4OTF[0];
			vmfloat4 frfba = vmfloat4(rgba.r / 255.f, rgba.g / 255.f, rgba.b / 255.f, rgba.a / 255.f);
			//frfba.r *= frfba.a;
			//frfba.g *= frfba.a;
			//frfba.b *= frfba.a;
			int offset = i * tmap_data->array_lengths.x;
			f4ColorPreIntTF[0 + offset] = frfba;
			for (int j = 1; j < tmap_data->array_lengths.x; j++)
			{
				vmbyte4 rgba = py4OTF[j];
				vmfloat4 frfba = vmfloat4(rgba.r / 255.f, rgba.g / 255.f, rgba.b / 255.f, rgba.a / 255.f);
				//frfba.r *= frfba.a;
				//frfba.g *= frfba.a;
				//frfba.b *= frfba.a;
				f4ColorPreIntTF[j + offset] = f4ColorPreIntTF[j - 1 + offset] + frfba;
			}
		}
	}
	else {
		vmbyte4* py4ColorTF = (vmbyte4*)d11MappedRes.pData;
#pragma omp parallel for 
		for (int i = 0; i < tmap_data->array_lengths.y; i++)
		{
			memcpy(&py4ColorTF[i * tmap_data->array_lengths.x], tmap_data->tmap_buffers[i], tmap_data->array_lengths.x * sizeof(vmbyte4));
		}
	}
	g_pvmCommonParams->dx11DeviceImmContext->Unmap((ID3D11Resource*)gres.alloc_res_ptrs[DTYPE_RES], 0);

	tobj->SetObjParam(updateTimeName, vmhelpers::GetCurrentTimePack());

	return true;
}

bool grd_helper::UpdatePrimitiveModel(GpuRes& gres_vtx, GpuRes& gres_idx, map<string, GpuRes>& map_gres_texs, VmVObjectPrimitive* pobj, LocalProgress* progress)
{
	PrimitiveData* prim_data = ((VmVObjectPrimitive*)pobj)->GetPrimitiveData();

	auto CheckReusability = [&pobj](GpuRes& gres, bool& update_data, bool& regen_data,
		const vmobjects::VmParamMap<std::string, std::any>& res_new_values)
	{
		unsigned long long _gpu_gen_timg = gres.res_values.GetParam("LAST_UPDATE_TIME", (ullong)0);
		unsigned long long _cpu_gen_timg = pobj->GetContentUpdateTime();
		if (_gpu_gen_timg < _cpu_gen_timg)
		{
			// now, at least update
			update_data = true;
			bool is_reuse_memory = pobj->GetObjParam("_bool_ReuseGpuMemory", false);
			if (!is_reuse_memory)
			{
				regen_data = true;
				// add properties
				for (auto it = res_new_values.begin(); it != res_new_values.end(); it++)
					gres.res_values.SetParam(it->first, it->second);
			}
		}
	};

	bool update_data = pobj->GetObjParam("_bool_UpdateData", false);
	// always
	{
		gres_vtx.vm_src_id = pobj->GetObjectID();
		gres_vtx.res_name = string("PRIMITIVE_MODEL_VTX");

		if (!g_pCGpuManager->UpdateGpuResource(gres_vtx))
		{
			int num_vtx_defs = prim_data->GetNumVertexDefinitions();
			uint stride_bytes = num_vtx_defs * sizeof(vmfloat3);
			gres_vtx.rtype = RTYPE_BUFFER;
			gres_vtx.options["USAGE"] = D3D11_USAGE_DEFAULT;
			gres_vtx.options["CPU_ACCESS_FLAG"] = NULL; // D3D11_CPU_ACCESS_WRITE;// | D3D11_CPU_ACCESS_READ;
			gres_vtx.options["BIND_FLAG"] = D3D11_BIND_VERTEX_BUFFER;
			gres_vtx.options["FORMAT"] = DXGI_FORMAT_R32G32B32_FLOAT;
			gres_vtx.res_values.SetParam("NUM_ELEMENTS", (uint)prim_data->num_vtx);
			gres_vtx.res_values.SetParam("STRIDE_BYTES", (uint)stride_bytes);

			update_data = true;
			g_pCGpuManager->GenerateGpuResource(gres_vtx);
		}
		else
		{
			vmobjects::VmParamMap<std::string, std::any> res_new_values;
			res_new_values.SetParam("NUM_ELEMENTS", (uint)prim_data->num_vtx);
			bool regen_data = false;
			CheckReusability(gres_vtx, update_data, regen_data, res_new_values);
			if(regen_data)
				g_pCGpuManager->GenerateGpuResource(gres_vtx);
		}

		if (update_data)
		{
			if(is_test_out)
				cout << "vertex update!! ---> " << gres_vtx.vm_src_id << endl;
			int num_vtx_defs = prim_data->GetNumVertexDefinitions();

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

			//if (pobj->GetObjectID() == 33554445)
			//	cout << "------33554445--------------------> " << prim_data->num_vtx << endl;

			ID3D11Buffer* pdx11bufvtx = (ID3D11Buffer*)gres_vtx.alloc_res_ptrs[DTYPE_RES];
			{
				D3D11_SUBRESOURCE_DATA subres;
				subres.pSysMem = new vmfloat3[num_vtx_defs * prim_data->num_vtx];
				subres.SysMemPitch = num_vtx_defs * sizeof(vmfloat3) * prim_data->num_vtx;
				subres.SysMemSlicePitch = 0; // only for 3D resource
				vmfloat3* vtx_group = (vmfloat3*)subres.pSysMem;
				for (uint i = 0; i < prim_data->num_vtx; i++)
				{
					for (int j = 0; j < num_vtx_defs; j++)
					{
						vtx_group[i*num_vtx_defs + j] = vtx_def_ptrs[j][i];
					}
				}
				g_pvmCommonParams->dx11DeviceImmContext->UpdateSubresource(pdx11bufvtx, 0, NULL, subres.pSysMem, subres.SysMemPitch, 0);
				VMSAFE_DELETEARRAY(subres.pSysMem);
			}

			//ID3D11Buffer* pdx11bufvtx = (ID3D11Buffer*)gres_vtx.alloc_res_ptrs[DTYPE_RES];
			//D3D11_MAPPED_SUBRESOURCE mappedRes;
			//g_VmCommonParams.dx11DeviceImmContext->Map(pdx11bufvtx, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes);
			//vmfloat3* vtx_group = (vmfloat3*)mappedRes.pData;
			//for (uint i = 0; i < prim_data->num_vtx; i++)
			//{
			//	for (int j = 0; j < num_vtx_defs; j++)
			//	{
			//		vtx_group[i*num_vtx_defs + j] = vtx_def_ptrs[j][i];
			//	}
			//}
			//g_VmCommonParams.dx11DeviceImmContext->Unmap(pdx11bufvtx, NULL);

			pobj->SetObjParam("_bool_UpdateData", false);
		}
	}

	if (prim_data->vidx_buffer && prim_data->num_vidx > 0)
	{
		gres_idx.vm_src_id = pobj->GetObjectID();
		gres_idx.res_name = string("PRIMITIVE_MODEL_IDX");
		if (!g_pCGpuManager->UpdateGpuResource(gres_idx))
		{
			gres_idx.rtype = RTYPE_BUFFER;
			gres_idx.options["USAGE"] = D3D11_USAGE_DEFAULT;
			gres_idx.options["CPU_ACCESS_FLAG"] = NULL;
			gres_idx.options["BIND_FLAG"] = D3D11_BIND_INDEX_BUFFER;
			gres_idx.options["FORMAT"] = DXGI_FORMAT_R32_UINT;
			gres_idx.res_values.SetParam("NUM_ELEMENTS", (uint)prim_data->num_vidx);
			gres_idx.res_values.SetParam("STRIDE_BYTES", (uint)sizeof(uint));

			g_pCGpuManager->GenerateGpuResource(gres_idx);


			//D3D11_MAPPED_SUBRESOURCE mappedRes;
			//g_VmCommonParams.dx11DeviceImmContext->Map(pdx11bufidx, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes);
			//uint* vidx_buf = (uint*)mappedRes.pData;
			//memcpy(vidx_buf, prim_data->vidx_buffer, prim_data->num_vidx * sizeof(uint));
			//g_VmCommonParams.dx11DeviceImmContext->Unmap(pdx11bufidx, NULL);
		}
		else
		{
			vmobjects::VmParamMap<std::string, std::any> res_new_values;
			res_new_values.SetParam("NUM_ELEMENTS", (uint)prim_data->num_vidx);
			bool regen_data = false;
			CheckReusability(gres_idx, update_data, regen_data, res_new_values);
			if(regen_data)
				g_pCGpuManager->GenerateGpuResource(gres_idx);
		}

		if (update_data)
		{
			ID3D11Buffer* pdx11bufidx = (ID3D11Buffer*)gres_idx.alloc_res_ptrs[DTYPE_RES];
			{
				D3D11_SUBRESOURCE_DATA subres;
				subres.pSysMem = new uint[prim_data->num_vidx];
				subres.SysMemPitch = prim_data->num_vidx * sizeof(uint);
				subres.SysMemSlicePitch = 0; // only for 3D resource
				memcpy((void*)subres.pSysMem, prim_data->vidx_buffer, prim_data->num_vidx * sizeof(uint));
				g_pvmCommonParams->dx11DeviceImmContext->UpdateSubresource(pdx11bufidx, 0, NULL, subres.pSysMem, subres.SysMemPitch, 0);
				VMSAFE_DELETEARRAY(subres.pSysMem);
			}
		}
	}

	//vmint3 tex_res_size;
	//((VmVObjectPrimitive*)pobj)->GetCustomParameter("_int3_TextureWHN", data_type::dtype<vmint3>(), &tex_res_size);
	bool has_texture_img = pobj->GetObjParam("_bool_HasTextureMap", true);
	if (prim_data->texture_res_info.size() > 0)
	{
		vmint2 tex_res_size;
		int byte_stride;
		byte* texture_res;

		auto upload_teximg = [&prim_data](GpuRes& gres_tex, const vmint2& tex_res_size, int byte_stride)
		{
			ID3D11Texture2D* pdx11tx2dres = (ID3D11Texture2D*)gres_tex.alloc_res_ptrs[DTYPE_RES];

			D3D11_SUBRESOURCE_DATA subres;
			int byte_stride_gpu = byte_stride == 1 ? 1 : 4;
			subres.pSysMem = new byte[tex_res_size.x * tex_res_size.y * byte_stride_gpu * prim_data->texture_res_info.size()];
			subres.SysMemPitch = tex_res_size.x * byte_stride_gpu;
			subres.SysMemSlicePitch = tex_res_size.x * tex_res_size.y * byte_stride_gpu; // only for 3D resource

			byte* tx_subres = (byte*)subres.pSysMem;
			int index = 0;
			for (auto it = prim_data->texture_res_info.begin(); it != prim_data->texture_res_info.end(); it++, index++)
			{
				byte* texture_res = get<3>(it->second);
				vmint2 tex_res_size = vmint2(get<0>(it->second), get<1>(it->second));
				assert(byte_stride == get<2>(it->second));
				assert(byte_stride != 2);

				for (int h = 0; h < tex_res_size.y; h++)
					for (int x = 0; x < tex_res_size.x; x++)
					{
						//for (int i = 0; i < byte_stride; i++)
						//	tx_subres[byte_stride_gpu * x + h * subres.SysMemPitch + i + index * subres.SysMemSlicePitch] = texture_res[byte_stride * (x + h * tex_res_size.x) + i];
						//for (int i = byte_stride; i < byte_stride_gpu; i++)
						//	tx_subres[byte_stride_gpu * x + h * subres.SysMemPitch + i + index * subres.SysMemSlicePitch] = 255;

						for (int i = 0; i < byte_stride; i++)
							tx_subres[byte_stride_gpu * (x + h * tex_res_size.x) + index * byte_stride_gpu * tex_res_size.x * tex_res_size.y + i] = texture_res[byte_stride * (x + h * tex_res_size.x) + i];
						for (int i = byte_stride; i < byte_stride_gpu; i++)
							tx_subres[byte_stride_gpu * (x + h * tex_res_size.x) + index * byte_stride_gpu * tex_res_size.x * tex_res_size.y + i] = 255;
					}
			}

			// ??
			g_pvmCommonParams->dx11DeviceImmContext->UpdateSubresource(pdx11tx2dres, 0, NULL, subres.pSysMem, subres.SysMemPitch, subres.SysMemSlicePitch);
			VMSAFE_DELETEARRAY(subres.pSysMem);
		};

		if (prim_data->GetTexureInfo("MAP_COLOR4", tex_res_size.x, tex_res_size.y, byte_stride, &texture_res) || prim_data->GetTexureInfo("PLY_TEX_MAP_0", tex_res_size.x, tex_res_size.y, byte_stride, &texture_res))
		{
			// cmm case
			GpuRes gres_tex;
			gres_tex.vm_src_id = pobj->GetObjectID();
			gres_tex.res_name = string("PRIMITIVE_MODEL_TEX_COLOR4");

			if (!g_pCGpuManager->UpdateGpuResource(gres_tex))
			{
				gres_tex.rtype = RTYPE_TEXTURE2D;
				gres_tex.options["USAGE"] = D3D11_USAGE_DEFAULT;
				gres_tex.options["CPU_ACCESS_FLAG"] = NULL;// D3D11_CPU_ACCESS_WRITE;
				gres_tex.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE;
				gres_tex.options["FORMAT"] = DXGI_FORMAT_R8G8B8A8_UNORM;
				gres_tex.res_values.SetParam("WIDTH", (uint)tex_res_size.x);
				gres_tex.res_values.SetParam("HEIGHT", (uint)tex_res_size.y);
				gres_tex.res_values.SetParam("DEPTH", (uint)prim_data->texture_res_info.size());
				
				//auto upload_teximg = [&prim_data](GpuRes& gres_tex, D3D11_MAP maptype)
				//{
				//	ID3D11Texture2D* pdx11tx2dres = (ID3D11Texture2D*)gres_tex.alloc_res_ptrs[DTYPE_RES];
				//	D3D11_MAPPED_SUBRESOURCE mappedRes;
				//	g_VmCommonParams.dx11DeviceImmContext->Map(pdx11tx2dres, 0, maptype, 0, &mappedRes);
				//	vmbyte4* tx_res = (vmbyte4*)mappedRes.pData;
				//	int index = 0;
				//	for (auto it = prim_data->texture_res_info.begin(); it != prim_data->texture_res_info.end(); it++, index++)
				//	{
				//		byte* texture_res = get<3>(it->second);
				//		vmint2 tex_res_size = vmint2(get<0>(it->second), get<1>(it->second));
				//		int byte_stride = get<2>(it->second);
				//
				//		if (byte_stride == 4)
				//		{
				//			vmbyte4* tx_res_cpu = (vmbyte4*)texture_res;
				//			for (int h = 0; h < tex_res_size.y; h++)
				//				memcpy(&tx_res[h * (mappedRes.RowPitch / 4) + index * (mappedRes.DepthPitch / 4)], &tx_res_cpu[h * tex_res_size.x], tex_res_size.x * sizeof(vmbyte4));
				//		}
				//		else
				//		{
				//			assert(byte_stride == 3);
				//			for (int h = 0; h < tex_res_size.y; h++)
				//				for (int x = 0; x < tex_res_size.x; x++)
				//				{
				//					vmbyte4 rgba;
				//					rgba.r = texture_res[(x + h * tex_res_size.x) * byte_stride + 0];
				//					rgba.g = texture_res[(x + h * tex_res_size.x) * byte_stride + 1];
				//					rgba.b = texture_res[(x + h * tex_res_size.x) * byte_stride + 2];
				//					rgba.a = 255;
				//					tx_res[x + h * (mappedRes.RowPitch / 4) + index * (mappedRes.DepthPitch / 4)] = rgba;
				//				}
				//		}
				//	}
				//	g_VmCommonParams.dx11DeviceImmContext->Unmap(pdx11tx2dres, NULL);
				//};
				
				g_pCGpuManager->GenerateGpuResource(gres_tex);

				//if (prim_data->texture_res_info.size() == 1)
				//{
				//	g_pCGpuManager->GenerateGpuResource(gres_tex);
				//	upload_teximg(gres_tex, D3D11_MAP_WRITE_DISCARD);
				//}
				//else
				//{
				//	gres_tex.options["USAGE"] = D3D11_USAGE_DEFAULT;
				//	gres_tex.options["CPU_ACCESS_FLAG"] = NULL;
				//	g_pCGpuManager->GenerateGpuResource(gres_tex);
				//
				//	// if the texture is array, direct mapping is impossible, so use this tricky copyresource way.
				//	GpuRes tmp_gres;
				//	tmp_gres.vm_src_id = pobj->GetObjectID();
				//	tmp_gres.res_name = string("PRIMITIVE_MODEL_TEX_TEMP");
				//	tmp_gres.rtype = RTYPE_TEXTURE2D;
				//	tmp_gres.options["USAGE"] = D3D11_USAGE_STAGING;
				//	tmp_gres.options["CPU_ACCESS_FLAG"] = D3D11_CPU_ACCESS_WRITE;
				//	tmp_gres.options["BIND_FLAG"] = NULL;
				//	tmp_gres.options["FORMAT"] = DXGI_FORMAT_R8G8B8A8_UNORM;
				//	tmp_gres.res_dvalues["WIDTH"] = tex_res_size.x;
				//	tmp_gres.res_dvalues["HEIGHT"] = tex_res_size.y;
				//	tmp_gres.res_dvalues["DEPTH"] = (double)prim_data->texture_res_info.size();
				//	g_pCGpuManager->GenerateGpuResource(tmp_gres);
				//	upload_teximg(tmp_gres, D3D11_MAP_WRITE);
				//	g_VmCommonParams.dx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gres_tex.alloc_res_ptrs[DTYPE_RES], (ID3D11Texture2D*)tmp_gres.alloc_res_ptrs[DTYPE_RES]);
				//	g_pCGpuManager->ReleaseGpuResource(tmp_gres, false);
				//}
			}
			else
			{
				vmobjects::VmParamMap<std::string, std::any> res_new_values;
				res_new_values.SetParam("WIDTH", (uint)tex_res_size.x);
				res_new_values.SetParam("HEIGHT", (uint)tex_res_size.y);
				res_new_values.SetParam("DEPTH", (uint)prim_data->texture_res_info.size());
				bool regen_data = false;
				CheckReusability(gres_tex, update_data, regen_data, res_new_values);
				if(regen_data)
					g_pCGpuManager->GenerateGpuResource(gres_tex);
			}

			if (update_data)
				upload_teximg(gres_tex, tex_res_size, byte_stride);

			map_gres_texs["MAP_COLOR4"] = gres_tex;
		}
		else
		{
			double Ns = pobj->GetObjParam("_float_Ns", (double)0);

			auto update_tex_res = [&Ns, &CheckReusability, &update_data](GpuRes& gres_tex, const string& mat_name, const vmint2& tex_res_size, const int byte_stride, const byte* texture_res)
			{
				auto upload_single_teximg = [&tex_res_size, &byte_stride, &texture_res](GpuRes& gres_tex)
				{
					ID3D11Texture2D* pdx11tx2dres = (ID3D11Texture2D*)gres_tex.alloc_res_ptrs[DTYPE_RES];

					D3D11_SUBRESOURCE_DATA subres;
					int byte_stride_gpu = byte_stride == 1 ? 1 : 4;
					subres.pSysMem = new byte[tex_res_size.x * tex_res_size.y * byte_stride_gpu];
					subres.SysMemPitch = tex_res_size.x * byte_stride_gpu;
					subres.SysMemSlicePitch = 0; // only for 3D resource

					byte* tx_res_gpu = (byte*)subres.pSysMem;
					for (int h = 0; h < tex_res_size.y; h++)
						for (int x = 0; x < tex_res_size.x; x++)
						{
							for (int i = 0; i < byte_stride; i++)
								tx_res_gpu[byte_stride_gpu * x + h * subres.SysMemPitch + i] = texture_res[byte_stride * (x + h * tex_res_size.x) + i];
							for (int i = byte_stride; i < byte_stride_gpu; i++)
								tx_res_gpu[byte_stride_gpu * x + h * subres.SysMemPitch + i] = 255;
						}
					g_pvmCommonParams->dx11DeviceImmContext->UpdateSubresource(pdx11tx2dres, 0, NULL, subres.pSysMem, subres.SysMemPitch, subres.SysMemSlicePitch);
					VMSAFE_DELETEARRAY(subres.pSysMem);
					//D3D11_MAPPED_SUBRESOURCE mappedRes;
					//g_VmCommonParams.dx11DeviceImmContext->Map(pdx11tx2dres, 0, maptype, 0, &mappedRes);
					//byte* tx_res_gpu = (byte*)mappedRes.pData;
					//int byte_stride_gpu = byte_stride == 1 ? 1 : 4;
					//for (int h = 0; h < tex_res_size.y; h++)
					//	for (int x = 0; x < tex_res_size.x; x++)
					//	{
					//		for (int i = 0; i < byte_stride; i++)
					//			tx_res_gpu[byte_stride_gpu * x + h * mappedRes.RowPitch + i] = texture_res[byte_stride * (x + h * tex_res_size.x) + i];
					//		for (int i = byte_stride; i < byte_stride_gpu; i++)
					//			tx_res_gpu[byte_stride_gpu * x + h * mappedRes.RowPitch + i] = 255;
					//	}
					//g_VmCommonParams.dx11DeviceImmContext->Unmap(pdx11tx2dres, NULL);
				};

				gres_tex.res_name = string("PRIMITIVE_MODEL_") + mat_name;
				if (!g_pCGpuManager->UpdateGpuResource(gres_tex))
				{
					gres_tex.rtype = RTYPE_TEXTURE2D;
					gres_tex.options["USAGE"] = D3D11_USAGE_DEFAULT;
					gres_tex.options["CPU_ACCESS_FLAG"] = NULL;
					gres_tex.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE;
					assert(byte_stride != 2);
					gres_tex.options["FORMAT"] = byte_stride == 1 ? DXGI_FORMAT_R8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;
					gres_tex.res_values.SetParam("WIDTH", (uint)tex_res_size.x);
					gres_tex.res_values.SetParam("HEIGHT", (uint)tex_res_size.y);

					g_pCGpuManager->GenerateGpuResource(gres_tex);
				}
				else
				{
					vmobjects::VmParamMap<std::string, std::any> res_new_values;
					res_new_values.SetParam("WIDTH", (uint)tex_res_size.x);
					res_new_values.SetParam("HEIGHT", (uint)tex_res_size.y);
					bool regen_data = false;
					CheckReusability(gres_tex, update_data, regen_data, res_new_values);
					if(regen_data)
						g_pCGpuManager->GenerateGpuResource(gres_tex);
				}

				if (update_data)
					upload_single_teximg(gres_tex);
			};


			for (int i = 0; i < NUM_MATERIALS; i++)
			{
				if (prim_data->GetTexureInfo(g_materials[i], tex_res_size.x, tex_res_size.y, byte_stride, &texture_res))
				{
					GpuRes gres_tex;
					gres_tex.vm_src_id = pobj->GetObjectID();
					update_tex_res(gres_tex, g_materials[i], tex_res_size, byte_stride, texture_res);
					map_gres_texs[g_materials[i]] = gres_tex;
				}
			}
		}

		//if (!g_pCGpuManager->UpdateGpuResource(gres_tex))
		//{
		//	gres_tex.rtype = RTYPE_TEXTURE2D;
		//	gres_tex.options["USAGE"] = D3D11_USAGE_DYNAMIC;
		//	gres_tex.options["CPU_ACCESS_FLAG"] = D3D11_CPU_ACCESS_WRITE;
		//	gres_tex.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE;
		//	gres_tex.options["FORMAT"] = DXGI_FORMAT_R8G8B8A8_UNORM;
		//	gres_tex.res_dvalues["WIDTH"] = tex_res_size.x;
		//	gres_tex.res_dvalues["HEIGHT"] = tex_res_size.y;
		//	gres_tex.res_dvalues["DEPTH"] = (double)prim_data->texture_res_info.size();
		//
		//	auto upload_teximg = [&prim_data](GpuRes& gres_tex, D3D11_MAP maptype)
		//	{
		//		ID3D11Texture2D* pdx11tx2dres = (ID3D11Texture2D*)gres_tex.alloc_res_ptrs[DTYPE_RES];
		//		D3D11_MAPPED_SUBRESOURCE mappedRes;
		//		g_VmCommonParams.dx11DeviceImmContext->Map(pdx11tx2dres, 0, maptype, 0, &mappedRes);
		//		vmbyte4* tx_res = (vmbyte4*)mappedRes.pData;
		//		for (int i = 0; i < prim_data->texture_res_info.size(); i++)
		//		{
		//			void* texture_res;
		//			vmint3 tex_res_size;
		//			prim_data->GetTexureInfo(i, tex_res_size.x, tex_res_size.y, tex_res_size.z, &texture_res);
		//			vmbyte4* tx_res_cpu = (vmbyte4*)texture_res;
		//
		//			for (int h = 0; h < tex_res_size.y; h++)
		//				memcpy(&tx_res[h * (mappedRes.RowPitch / 4) + i * (mappedRes.DepthPitch / 4)], &tx_res_cpu[h * tex_res_size.x], tex_res_size.x * sizeof(vmbyte4));
		//		}
		//		g_VmCommonParams.dx11DeviceImmContext->Unmap(pdx11tx2dres, NULL);
		//	};
		//
		//	if (prim_data->texture_res_info.size() == 1)
		//	{
		//		g_pCGpuManager->GenerateGpuResource(gres_tex);
		//		upload_teximg(gres_tex, D3D11_MAP_WRITE_DISCARD);
		//	}
		//	else
		//	{
		//		gres_tex.options["USAGE"] = D3D11_USAGE_DEFAULT;
		//		gres_tex.options["CPU_ACCESS_FLAG"] = NULL;
		//		g_pCGpuManager->GenerateGpuResource(gres_tex);
		//
		//		GpuRes tmp_gres;
		//		tmp_gres.vm_src_id = pobj->GetObjectID();
		//		tmp_gres.res_name = string("PRIMITIVE_MODEL_TEX_TEMP");
		//		tmp_gres.rtype = RTYPE_TEXTURE2D;
		//		tmp_gres.options["USAGE"] = D3D11_USAGE_STAGING;
		//		tmp_gres.options["CPU_ACCESS_FLAG"] = D3D11_CPU_ACCESS_WRITE;
		//		tmp_gres.options["BIND_FLAG"] = NULL;
		//		tmp_gres.options["FORMAT"] = DXGI_FORMAT_R8G8B8A8_UNORM;
		//		tmp_gres.res_dvalues["WIDTH"] = tex_res_size.x;
		//		tmp_gres.res_dvalues["HEIGHT"] = tex_res_size.y;
		//		tmp_gres.res_dvalues["DEPTH"] = (double)prim_data->texture_res_info.size();
		//		g_pCGpuManager->GenerateGpuResource(tmp_gres);
		//		upload_teximg(tmp_gres, D3D11_MAP_WRITE);
		//		g_VmCommonParams.dx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gres_tex.alloc_res_ptrs[DTYPE_RES], (ID3D11Texture2D*)tmp_gres.alloc_res_ptrs[DTYPE_RES]);
		//		g_pCGpuManager->ReleaseGpuResource(tmp_gres, false);
		//	}
		//	//g_VmCommonParams.dx11DeviceImmContext->Cop
		//}
	}

	return true;
}

bool grd_helper::UpdateFrameBuffer(GpuRes& gres,
	const VmIObject* iobj,
	const string& res_name,
	const GpuResType gres_type,
	const uint bind_flag,
	const uint dx_format,
	const int fb_flag,
	const int num_frags_perpixel,
	const int structured_stride)
{
	gres.vm_src_id = iobj->GetObjectID();
	gres.res_name = res_name;

	if (g_pCGpuManager->UpdateGpuResource(gres))
		return true;

	gres.rtype = gres_type;
	vmint2 fb_size;
	((VmIObject*)iobj)->GetFrameBufferInfo(&fb_size);

	gres.options["USAGE"] = (fb_flag & UPFB_SYSOUT) ? D3D11_USAGE_STAGING : D3D11_USAGE_DEFAULT;
	gres.options["CPU_ACCESS_FLAG"] = (fb_flag & UPFB_SYSOUT) ? D3D11_CPU_ACCESS_READ : NULL;
	gres.options["BIND_FLAG"] = bind_flag;
	gres.options["FORMAT"] = dx_format;
	uint stride_bytes = 0;
	switch (gres_type)
	{
	case RTYPE_BUFFER:
		if (fb_flag & UPFB_NFPP_BUFFERSIZE) gres.res_values.SetParam("NUM_ELEMENTS", (uint)num_frags_perpixel);
		else gres.res_values.SetParam("NUM_ELEMENTS", (uint)(num_frags_perpixel > 0 ? fb_size.x * fb_size.y * num_frags_perpixel : fb_size.x * fb_size.y));
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
		case DXGI_FORMAT_R32G32B32A32_UINT: stride_bytes = sizeof(vmuint4); break;
		case DXGI_FORMAT_UNKNOWN: stride_bytes = structured_stride; break;
		case DXGI_FORMAT_R32_TYPELESS: stride_bytes = sizeof(uint); break;
		default:
			return false;
		}
		if (fb_flag & UPFB_RAWBYTE) gres.options["RAW_ACCESS"] = 1;
		if (stride_bytes == 0) return false;
		gres.res_values.SetParam("STRIDE_BYTES", (uint)stride_bytes);
		break;
	case RTYPE_TEXTURE2D:
		gres.res_values.SetParam("WIDTH", (uint)(((fb_flag & UPFB_HALF) || (fb_flag & UPFB_HALF_W)) ? fb_size.x / 2 : fb_size.x));
		gres.res_values.SetParam("HEIGHT", (uint)(((fb_flag & UPFB_HALF) || (fb_flag & UPFB_HALF_H)) ? fb_size.y / 2 : fb_size.y));
		if (fb_flag & UPFB_NFPP_TEXTURESTACK) gres.res_values.SetParam("DEPTH", (uint)num_frags_perpixel);
		if (fb_flag & UPFB_MIPMAP) gres.options["MIP_GEN"] = 1;
		if (fb_flag & UPFB_HALF) gres.options["HALF_GEN"] = 1;
		break;
	default:
		::MessageBoxA(NULL, "Not supported Data Type", NULL, MB_OK);
		return false;
	}

	g_pCGpuManager->GenerateGpuResource(gres);

	return true;
}

bool grd_helper::UpdateCustomBuffer(GpuRes& gres, VmObject* srcObj, const string& resName, const void* bufPtr, const int numElements, DXGI_FORMAT dxFormat, const int type_bytes, LocalProgress* progress)
{
	gres.vm_src_id = srcObj->GetObjectID();
	gres.res_name = resName;

	string updateName = "_ullong_Latest" + resName + "GpuUpdateTime";

	if (g_pCGpuManager->UpdateGpuResource(gres)) {

		ullong _tp_cpu = srcObj->GetContentUpdateTime();
		ullong _tp_gpu = srcObj->GetObjParam(updateName, (ullong)0);
		if (_tp_gpu >= _tp_cpu)
			return true;
	}
	else {
		gres.rtype = RTYPE_BUFFER;
		gres.options["USAGE"] = D3D11_USAGE_DYNAMIC;
		gres.options["CPU_ACCESS_FLAG"] = D3D11_CPU_ACCESS_WRITE;
		gres.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE;
		gres.options["FORMAT"] = dxFormat;// DXGI_FORMAT_R8G8B8A8_UNORM;

		gres.res_values.SetParam("NUM_ELEMENTS", (const uint)numElements);
		gres.res_values.SetParam("STRIDE_BYTES", (const uint)type_bytes);

		g_pCGpuManager->GenerateGpuResource(gres);
	}

	D3D11_MAPPED_SUBRESOURCE d11MappedRes;
	g_pvmCommonParams->dx11DeviceImmContext->Map((ID3D11Resource*)gres.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes);
	memcpy(d11MappedRes.pData, bufPtr, type_bytes * numElements);
	g_pvmCommonParams->dx11DeviceImmContext->Unmap((ID3D11Resource*)gres.alloc_res_ptrs[DTYPE_RES], 0);

	srcObj->SetObjParam(updateName, vmhelpers::GetCurrentTimePack());

	return true;
}

//#define *(vmfloat3*)&
//#define *(vmfloat4*)&
//#define *(XMMATRIX*)&

void grd_helper::SetCb_Camera(CB_CameraState& cb_cam, const vmmat44f& matWS2SS, const vmmat44f& matSS2WS, VmCObject* ccobj, const vmint2& fb_size, const int k_value, const float vz_thickness)
{
	cb_cam.mat_ss2ws = TRANSPOSE(matSS2WS);
	cb_cam.mat_ws2ss = TRANSPOSE(matWS2SS);

	vmfloat3 pos_cam, dir_cam;
	ccobj->GetCameraExtStatef(&pos_cam, &dir_cam, NULL);

	cb_cam.cam_flag = 0;
	if (ccobj->IsPerspective())
		cb_cam.cam_flag |= 0x1;

	cb_cam.pos_cam_ws = pos_cam;
	cb_cam.dir_view_ws = dir_cam;

	cb_cam.rt_width = (uint)fb_size.x;
	cb_cam.rt_height = (uint)fb_size.y;

	cb_cam.k_value = k_value;
	cb_cam.cam_vz_thickness = vz_thickness;

	double np, fp;
	ccobj->GetCameraIntState(NULL, &np, &fp, NULL);
	cb_cam.near_plane = (float)np;
	cb_cam.far_plane = (float)fp;
}

void grd_helper::SetCb_Env(CB_EnvState& cb_env, VmCObject* ccobj, const LightSource& light_src, const GlobalLighting& global_lighting, const LensEffect& lens_effect)
{
	vmfloat3 pos_cam, dir_cam;
	ccobj->GetCameraExtStatef(&pos_cam, &dir_cam, NULL);

	if (light_src.is_on_camera) {
		cb_env.dir_light_ws = dir_cam;
		cb_env.pos_light_ws = pos_cam;
	}
	else {
		cb_env.dir_light_ws = light_src.light_dir;
		cb_env.pos_light_ws = light_src.light_pos;
	}
	fNormalizeVector(&cb_env.dir_light_ws, &cb_env.dir_light_ws);

	if (light_src.is_pointlight)
		cb_env.env_flag |= 0x1;

	cb_env.r_kernel_ao = 0;
	if (global_lighting.apply_ssao)
	{
		cb_env.r_kernel_ao = global_lighting.ssao_r_kernel;
		cb_env.num_dirs = global_lighting.ssao_num_dirs;
		cb_env.num_steps = global_lighting.ssao_num_steps;
		cb_env.tangent_bias = global_lighting.ssao_tangent_bias;
		cb_env.ao_intensity = global_lighting.ssao_intensity;
		cb_env.env_flag |= (global_lighting.ssao_debug & 0xF) << 9;
	}

	cb_env.dof_lens_r = 0;
	if (lens_effect.apply_ssdof)
	{
		cb_env.dof_lens_r = lens_effect.dof_lens_r;
		cb_env.dof_lens_F = lens_effect.dof_lens_F;
		cb_env.dof_lens_ray_num_samples = lens_effect.dof_ray_num_samples;
		cb_env.dof_focus_z = lens_effect.dof_focus_z;
		cb_env.dof_focus_z = max(cb_env.dof_focus_z, cb_env.dof_lens_F * 1.2f);
	}

	cb_env.ltint_ambient = vmfloat4(light_src.light_ambient_color, 1.f);
	cb_env.ltint_diffuse = vmfloat4(light_src.light_diffuse_color, 1.f);
	cb_env.ltint_spec = vmfloat4(light_src.light_specular_color, 1.f);

	cb_env.num_lights = 1;

	//{
	//	vmdouble4 shading_factors = default_shading_factor;
	//	bool apply_shadingfactors = true;
	//	vobj->GetCustomParameter("_bool_ApplyShadingFactors", data_type::dtype<bool>(), &apply_shadingfactors);
	//	if (apply_shadingfactors)
	//		lobj->GetDstObjValue(vobj->GetObjectID(), "_float4_ShadingFactors", &shading_factors);
	//	cb_volume.pb_shading_factor = *(vmfloat4*)&vmfloat4(shading_factors);
	//}
}

void grd_helper::SetCb_VolumeObj(CB_VolumeObject& cb_volume, VmVObjectVolume* vobj, VmActor* actor, GpuRes& gresVol, const int iso_value, const float volblk_valuerange, const int sculpt_index)
{
	VolumeData* vol_data = vobj->GetVolumeData();

	//bool invert_normal = false;
	//vobj->GetCustomParameter("_bool_IsInvertPlaneDirection", data_type::dtype<bool>(), &invert_normal);
	//if (invert_normal)
	//	cb_volume.vobj_flag |= (0x1);
	if (sculpt_index >= 0)
		cb_volume.vobj_flag |= sculpt_index << 24;

	vmmat44f matWS2VS = actor->matWS2OS * vobj->GetMatrixOS2RSf();

	vmmat44f matVS2TS;
	vmmat44f matShift, matScale;
	fMatrixTranslation(&matShift, &vmfloat3(0.5f));
	fMatrixScaling(&matScale, &vmfloat3(1.f / (float)(vol_data->vol_size.x + vol_data->vox_pitch.x * 0.00f),
		1.f / (float)(vol_data->vol_size.y + vol_data->vox_pitch.z * 0.00f), 1.f / (float)(vol_data->vol_size.z + vol_data->vox_pitch.z * 0.00f)));
	matVS2TS = matShift * matScale;
	vmmat44f mat_ws2ts = matWS2VS * matVS2TS;
	cb_volume.mat_ws2ts = TRANSPOSE(mat_ws2ts);

	AaBbMinMax aabb_os;
	vobj->GetOrthoBoundingBox(aabb_os);
	vmmat44f mat_s;
	vmfloat3 aabb_size = aabb_os.pos_max - aabb_os.pos_min;
	fMatrixScaling(&mat_s, &vmfloat3(1. / aabb_size.x, 1. / aabb_size.y, 1. / aabb_size.z));

	vmmat44f matWS2BS = actor->matWS2OS * mat_s;
	cb_volume.mat_alignedvbox_tr_ws2bs = TRANSPOSE(matWS2BS);

	if (vol_data->store_dtype.type_bytes == data_type::dtype<byte>().type_bytes) // char
		cb_volume.value_range = 255.f;
	else if (vol_data->store_dtype.type_bytes == data_type::dtype<ushort>().type_bytes) // short
		cb_volume.value_range = 65535.f;
	else GMERRORMESSAGE("UNSUPPORTED FORMAT : grd_helper::SetCb_VolumeObj");

	vmfloat3 sampleOffset = vmfloat3(gresVol.res_values.GetParam("SAMPLE_OFFSET_X", 1.f),
		gresVol.res_values.GetParam("SAMPLE_OFFSET_Y", 1.f),
		gresVol.res_values.GetParam("SAMPLE_OFFSET_Z", 1.f));
	float minDistSample = (float)min(min(vol_data->vox_pitch.x * sampleOffset.x, vol_data->vox_pitch.y * sampleOffset.y),
		vol_data->vox_pitch.z * sampleOffset.z);
	cout << "minDistSample >>>> " << minDistSample << endl;
	float maxDistSample = (float)max(max(vol_data->vox_pitch.x * sampleOffset.x, vol_data->vox_pitch.y * sampleOffset.y),
		vol_data->vox_pitch.z * sampleOffset.z);
	cout << "maxDistSample >>>> " << maxDistSample << endl;
	float grad_offset_dist = maxDistSample;
	fTransformVector((vmfloat3*)&cb_volume.vec_grad_x, &vmfloat3(grad_offset_dist, 0, 0), &mat_ws2ts);
	fTransformVector((vmfloat3*)&cb_volume.vec_grad_y, &vmfloat3(0, grad_offset_dist, 0), &mat_ws2ts);
	fTransformVector((vmfloat3*)&cb_volume.vec_grad_z, &vmfloat3(0, 0, grad_offset_dist), &mat_ws2ts);
	
	const float sample_rate = 1.f;
	cb_volume.opacity_correction = 1.f;
	cb_volume.sample_dist = minDistSample / sample_rate;
	cb_volume.opacity_correction /= sample_rate;
	
	//cb_volume.vol_size = vmfloat3((float)vol_data->vol_size.x, (float)vol_data->vol_size.y, (float)vol_data->vol_size.z);

	cb_volume.vol_size = vmfloat3(gresVol.res_values.GetParam("WIDTH", (uint)1),
		gresVol.res_values.GetParam("HEIGHT", (uint)1),
		gresVol.res_values.GetParam("DEPTH", (uint)1));

	// from pmapDValueVolume //
	//float fSamplePrecisionLevel = actor->GetParam("_float_SamplePrecisionLevel", 1.0f);
	//if (fSamplePrecisionLevel > 0)
	//{
	//	cb_volume.sample_dist /= fSamplePrecisionLevel;
	//	cb_volume.opacity_correction /= fSamplePrecisionLevel;
	//}
	cb_volume.vz_thickness = cb_volume.sample_dist;	// 현재 HLSL 은 Sample Distance 를 강제로 사용 중...
	cb_volume.iso_value = iso_value;

	//printf("sample_rate : %f\n", sample_rate);
	//printf("minDistSample : %f\n", minDistSample);
	//printf("fSamplePrecisionLevel : %f\n", fSamplePrecisionLevel);
	//printf("sample dist : %f\n", cb_volume.sample_dist);
	//printf("vz dist : %f\n", dThicknessVirtualzSlab);

	// volume blocks
	cb_volume.volblk_value_range = volblk_valuerange; // DOJO TO DO :  BLK 16UNORM ==> 16UINT... and forced to set 1
	const int blk_level = __BLKLEVEL;	// 0 : High Resolution, 1 : Low Resolution
	VolumeBlocks* volblk = vobj->GetVolumeBlock(blk_level);
	if(volblk != NULL)
		cb_volume.volblk_size_ts = vmfloat3(
			(float)volblk->unitblk_size.x / (float)vol_data->vol_size.x,
			(float)volblk->unitblk_size.y / (float)vol_data->vol_size.y, 
			(float)volblk->unitblk_size.z / (float)vol_data->vol_size.z
		);
}

void grd_helper::SetCb_PolygonObj(CB_PolygonObject& cb_polygon, VmVObjectPrimitive* pobj, VmActor* actor, 
	const vmmat44f& matWS2SS, const vmmat44f& matWS2PS, 
	const bool is_annotation_obj, const bool use_vertex_color)
{
	PrimitiveData* pobj_data = pobj->GetPrimitiveData();

	cb_polygon.mat_os2ws = TRANSPOSE(actor->matOS2WS);
	cb_polygon.mat_ws2os = TRANSPOSE(actor->matWS2OS);

	if (is_annotation_obj)// && prim_data->texture_res)
	{
		vmint3 i3TextureWHN = pobj->GetObjParam("_int3_TextureWHN", vmint3(0));
		cb_polygon.num_letters = i3TextureWHN.z;
		if (i3TextureWHN.z == 1)
		{
			vmfloat3* pos_vtx = pobj_data->GetVerticeDefinition("POSITION");
			vmfloat3 f3Pos0SS, f3Pos1SS, f3Pos2SS;
			vmmat44f matOS2SS = actor->matOS2WS * matWS2SS;

			vmfloat3 pos_vtx_0_ss, pos_vtx_1_ss, pos_vtx_2_ss;
			fTransformPoint(&pos_vtx_0_ss, &pos_vtx[0], &matOS2SS);
			fTransformPoint(&pos_vtx_1_ss, &pos_vtx[1], &matOS2SS);
			fTransformPoint(&pos_vtx_2_ss, &pos_vtx[2], &matOS2SS);
			if(pos_vtx_1_ss.x - pos_vtx_0_ss.x < 0)
				cb_polygon.pobj_flag |= (0x1 << 9);
			if (pos_vtx_2_ss.y - pos_vtx_0_ss.y < 0)
				cb_polygon.pobj_flag |= (0x1 << 10);
			//vmfloat3 f3VecWidth = pos_vtx[1] - pos_vtx[0];
			//vmfloat3 f3VecHeight = pos_vtx[2] - pos_vtx[0];
			//fTransformVector(&f3VecWidth, &f3VecWidth, &matOS2SS); // projection term 이 있는 경우 fTransformVector 만 쓰면 안 됨!!!
			//fTransformVector(&f3VecHeight, &f3VecHeight, &matOS2SS);
			//fNormalizeVector(&f3VecWidth, &f3VecWidth);
			//fNormalizeVector(&f3VecHeight, &f3VecHeight);
			//
			//f3VecWidth.z = 0;
			//f3VecHeight.z = 0;
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
			//	cb_polygon.pobj_flag |= (0x1 << 9);
			//
			////vmfloat3 f3VecNormal;
			////fCrossDotVector(&f3VecNormal, &f3VecHeight, &f3VecWidth);
			////fCrossDotVector(&f3VecHeight, &f3VecWidth, &f3VecNormal);
			//
			//if (fDotVector(&f3VecHeight, &vmfloat3(0, 1.f, 0)) < 0)
			//	cb_polygon.pobj_flag |= (0x1 << 10);
		}
	}
	else
	{
		if (!use_vertex_color)
			cb_polygon.pobj_flag |= (0x1 << 3);
	}

	{
		vmfloat3 illum_model = actor->GetParam("_float3_IllumWavefront", vmfloat3(1));
		// material setting
		cb_polygon.alpha = actor->color.a;
		cb_polygon.Ka = actor->GetParam("_float3_Ka", vmfloat3(actor->color)) * illum_model.x;
		cb_polygon.Kd = actor->GetParam("_float3_Kd", vmfloat3(actor->color)) * illum_model.y;
		cb_polygon.Ks = actor->GetParam("_float3_Ks", vmfloat3(actor->color)) * illum_model.z;
		cb_polygon.Ns = actor->GetParam("_float_Ns", (float)1.f);;
	}

	bool abs_diffuse = actor->GetParam("_bool_AbsDiffuse", false); // alpha 값에 따라...??
	if (!abs_diffuse)
		cb_polygon.pobj_flag |= (0x1 << 5);

	if (pobj_data->ptype == vmenums::PrimitiveTypeLINE) {
		bool is_dashed_line = actor->GetParam("_bool_IsDashed", false);
		if (is_dashed_line)
		{
			cb_polygon.pobj_flag |= (0x1 << 19);

			bool dashed_line_option_invclrs = actor->GetParam("_bool_IsInvertColorDashLine", false);
			if (dashed_line_option_invclrs)
				cb_polygon.pobj_flag |= (0x1 << 20);

			vmfloat3 pos_max_ws, pos_min_ws;
			fTransformPoint(&pos_max_ws, &vmfloat3(pobj_data->aabb_os.pos_max), &actor->matOS2WS);
			fTransformPoint(&pos_min_ws, &vmfloat3(pobj_data->aabb_os.pos_min), &actor->matOS2WS);
			vmfloat3 diff = pos_max_ws - pos_min_ws;
			diff.x = fabs(diff.x);
			diff.y = fabs(diff.y);
			diff.z = fabs(diff.z);
			float max_v = max(max(diff.x, diff.y), diff.z);
			int max_c_flag = 0;
			if (max_v == diff.y) max_c_flag = 1;
			else if (max_v == diff.z) max_c_flag = 2;

			cb_polygon.pobj_flag |= (max_c_flag << 30);
			cb_polygon.dash_interval = actor->GetParam("_float_LineDashInterval", 1.f);
		}
	}

	cb_polygon.vz_thickness = actor->GetParam("_float_VZThickness", 0.f);

	vmmat44f matOS2PS = actor->matOS2WS * matWS2PS;
	cb_polygon.mat_os2ps = TRANSPOSE(matOS2PS);

	bool force_to_pointsetrender = actor->GetParam("_bool_ForceToPointsetRender", false);
	if (pobj_data->ptype == PrimitiveTypePOINT || force_to_pointsetrender)
	{
		bool is_surfel = actor->GetParam("_bool_PointToSurfel", true);
		float fPointThickness = is_surfel? actor->GetParam("_float_SurfelSize", 0.f) : actor->GetParam("_float_PointThickness", 0.f);
		if (fPointThickness <= 0)
		{
			vmfloat3 pos_max_ws, pos_min_ws;
			fTransformPoint(&pos_max_ws, &((vmfloat3)pobj_data->aabb_os.pos_max), &actor->matOS2WS);
			fTransformPoint(&pos_min_ws, &((vmfloat3)pobj_data->aabb_os.pos_min), &actor->matOS2WS);
			fPointThickness = fLengthVector(&(pos_max_ws - pos_min_ws)) * 0.002f;
		}
		cb_polygon.pix_thickness = (float)(fPointThickness / 2.);
	}
	else if (pobj_data->ptype == PrimitiveTypeLINE)
	{
		cb_polygon.pix_thickness = actor->GetParam("_float_LineThickness", 0.f);
	}
}

void grd_helper::SetCb_TMap(CB_TMAP& cb_tmap, VmObject* tobj)
{
	MapTable* tobj_data = tobj->GetObjParamPtr<MapTable>("_TableMap_OTF");

	cb_tmap.first_nonzeroalpha_index = tobj_data->valid_min_idx.x;
	cb_tmap.last_nonzeroalpha_index = tobj_data->valid_max_idx.x;
	cb_tmap.tmap_size_x = tobj_data->array_lengths.x;
	vmbyte4 y4ColorEnd = ((vmbyte4**)tobj_data->tmap_buffers)[0][tobj_data->array_lengths.x - 1];
	cb_tmap.last_color = vmfloat4(y4ColorEnd.x / 255.f, y4ColorEnd.y / 255.f, y4ColorEnd.z / 255.f, y4ColorEnd.w / 255.f);
}

void grd_helper::SetCb_RenderingEffect(CB_RenderingEffect& cb_reffect, VmActor* actor)
{
	bool apply_occ = actor->GetParam("_bool_ApplyAO", false);
	bool apply_brdf = actor->GetParam("_bool_ApplyBRDF", false);
	cb_reffect.rf_flag = (int)apply_occ | ((int)apply_brdf << 1);

	if (apply_occ)
	{
		cb_reffect.occ_num_rays = (uint)actor->GetParam("_int_OccNumRays", (int)5);
		cb_reffect.occ_radius = actor->GetParam("_float_OccRadius", 0.f);
	}
	if (apply_brdf)
	{
		cb_reffect.brdf_diffuse_ratio = actor->GetParam("_float_PhongBRDFDiffuseRatio", 0.5f);
		cb_reffect.brdf_reft_ratio = actor->GetParam("_float_PhongBRDFReflectanceRatio", 0.5f);
		cb_reffect.brdf_expw_u = actor->GetParam("_float_PhongExpWeightU", 50.f);
		cb_reffect.brdf_expw_v = actor->GetParam("_float_PhongExpWeightV", 50.f);
	}
	cb_reffect.ao_intensity = actor->GetParam("_float_SSAOIntensity", 0.5f);

	// Curvature
	// to do
}

void grd_helper::SetCb_VolumeRenderingEffect(CB_VolumeRenderingEffect& cb_vreffect, VmVObjectVolume* vobj, VmActor* actor)
{
	cb_vreffect.attribute_voxel_sharpness = actor->GetParam("_float_VoxelSharpnessForAttributeVolume", 0.25f);
	cb_vreffect.clip_plane_intensity = actor->GetParam("_float_ClipPlaneIntensity", 1.f);

	vmdouble2 dGradientMagMinMax = vmdouble2(1, -1);
	cb_vreffect.occ_sample_dist_scale = actor->GetParam("_float_OccSampleDistScale", 1.f);
	cb_vreffect.sdm_sample_dist_scale = actor->GetParam("_float_SdmSampleDistScale", 1.f);
}

void grd_helper::SetCb_ClipInfo(CB_ClipInfo& cb_clip, VmVObject* obj, VmActor* actor)
{
	const int obj_id = obj->GetObjectID();
	bool is_clip_free = obj->GetObjParam("_bool_ClipFree", false);

	if (!is_clip_free)
	{
		// CLIPBOX / CLIPPLANE / BOTH //
		int clip_mode = actor->GetParam("_int_ClippingMode", (int)0);	// 0 : No, 1 : CLIPPLANE, 2 : CLIPBOX, 3 : BOTH
		cb_clip.clip_flag = clip_mode & 0x3;
	}

	if (cb_clip.clip_flag & 0x1)
	{
		cb_clip.pos_clipplane = actor->GetParam("_float3_PosClipPlaneWS", (vmfloat3)0);
		cb_clip.vec_clipplane = actor->GetParam("_float3_VecClipPlaneWS", (vmfloat3)0);
	}
	if (cb_clip.clip_flag & 0x2)
	{
		cb_clip.mat_clipbox_ws2bs = TRANSPOSE(actor->GetParam("_matrix44f_MatrixClipWS2BS", vmmat44f()));
	}
}

void grd_helper::SetCb_HotspotMask(CB_HotspotMask& cb_hsmask, VmFnContainer* _fncontainer, const vmmat44f& matWS2SS)
{
	vmfloat3 pos_3dtip_ws = _fncontainer->fnParams.GetParam("_float3_3DTipPos", vmdouble3());
	bool use_mask_3dtip = _fncontainer->fnParams.GetParam("_bool_UseMask3DTip", false);

	vmdouble4 mask_center_rs_0 = _fncontainer->fnParams.GetParam("_float4_MaskCenterRadius0", vmdouble4(150, 150, 200, 0.5f));
	//vmdouble3 mask_center_rs_1 = _fncontainer->fnParams.GetParam("_float4_MaskCenterRadius1", vmdouble3(fb_size / 4, fb_size.x / 5, 150, 3));
	vmdouble3 hotspot_params_0 = _fncontainer->fnParams.GetParam("_float3_HotspotParamsTKtKs0", vmdouble3(1.));
	//vmdouble3 hotspot_params_1 = _fncontainer->fnParams.GetParam("_float3_HotspotParamsTKtKs1", vmdouble3(1.));
	bool show_silhuette_edge_0 = _fncontainer->fnParams.GetParam("_bool_HotspotSilhuette0", false);
	//bool show_silhuette_edge_1 = _fncontainer->fnParams.GetParam("_bool_HotspotSilhuette1", false);
	float mask_bnd = _fncontainer->fnParams.GetParam("_float_MaskBndDisplay", 1.f);
	float inDepthVis = _fncontainer->fnParams.GetParam("_float_InDepthVis", 0.01f);
	auto __set_params = [&cb_hsmask, &matWS2SS, &pos_3dtip_ws, &use_mask_3dtip, &inDepthVis](int idx, const vmdouble4& mask_center_rs_0, const vmdouble3& hotspot_params, bool show_silhuette_edge, float mask_bnd)
	{
		const double max_smoothness = 30.;
		cb_hsmask.mask_info_[idx].pos_center = vmint2(mask_center_rs_0.x, mask_center_rs_0.y);
		cb_hsmask.mask_info_[idx].radius = (float)mask_center_rs_0.z;
		cb_hsmask.mask_info_[idx].smoothness = (int)(max(min(mask_center_rs_0.w * max_smoothness, max_smoothness), 0)) | ((int)show_silhuette_edge << 16);
		cb_hsmask.mask_info_[idx].thick = (float)hotspot_params.x;
		cb_hsmask.mask_info_[idx].kappa_t = (float)hotspot_params.y;
		cb_hsmask.mask_info_[idx].kappa_s = (float)hotspot_params.z;
		cb_hsmask.mask_info_[idx].bnd_thick = mask_bnd;
		cb_hsmask.mask_info_[idx].flag = 0;
		cb_hsmask.mask_info_[idx].in_depth_vis = inDepthVis;
		
		if (use_mask_3dtip)
		{
			cb_hsmask.mask_info_[idx].flag = 1;
			cb_hsmask.mask_info_[idx].pos_spotcenter = pos_3dtip_ws;
			vmfloat3 pos_3dtip_ss;
			fTransformPoint(&pos_3dtip_ss, &pos_3dtip_ws, &matWS2SS);
			cb_hsmask.mask_info_[idx].pos_center = vmint2(pos_3dtip_ss.x, pos_3dtip_ss.y);
		}
	};
	__set_params(0, mask_center_rs_0, hotspot_params_0, show_silhuette_edge_0, mask_bnd);
	//__set_params(1, mask_center_rs_1, hotspot_params_1, show_silhuette_edge_1, mask_bnd);
}

void grd_helper::SetCb_CurvedSlicer(CB_CurvedSlicer& cb_curvedSlicer, VmFnContainer* _fncontainer, VmIObject* iobj, float& sample_dist) {

	float fExPlaneWidth = _fncontainer->fnParams.GetParam("_float_ExPlaneWidth", 1.f);
	float fExPlaneHeight = _fncontainer->fnParams.GetParam("_float_ExPlaneHeight", 1.f);
	float fExCurveThicknessPositionRange = _fncontainer->fnParams.GetParam("_float_CurveThicknessPositionRange", 1.f);
	float fThicknessRatio = _fncontainer->fnParams.GetParam("_float_ThicknessRatio", 0.f);
	bool bIsRightSide = _fncontainer->fnParams.GetParam("_bool_IsRightSide", false);
	float fThicknessPosition = fThicknessRatio * fExCurveThicknessPositionRange * 0.5;
	float fPlaneThickness = _fncontainer->fnParams.GetParam("_float_PlaneThickness", 0.f);

	vector<vmfloat3>& vtrCurveInterpolations = *_fncontainer->fnParams.GetParamPtr<vector<vmfloat3>>("_vlist_FLOAT3_CurveInterpolations");
	vector<vmfloat3>& vtrCurveUpVectors = *_fncontainer->fnParams.GetParamPtr<vector<vmfloat3>>("_vlist_FLOAT3_CurveUpVectors");
	vector<vmfloat3>& vtrCurveTangentVectors = *_fncontainer->fnParams.GetParamPtr<vector<vmfloat3>>("_vlist_FLOAT3_CurveTangentVectors");
	int num_interpolation = (int)vtrCurveInterpolations.size();

	VmCObject* cobj = iobj->GetCameraObject();
	vmmat44 matSS2PS, matPS2CS, matCS2WS;
	cobj->GetMatrixSStoWS(&matSS2PS, &matPS2CS, &matCS2WS);
	vmmat44f matSS2WS = matSS2PS * matPS2CS * matCS2WS;

	vmint2 i2SizeBuffer;
	iobj->GetFrameBufferInfo(&i2SizeBuffer);

	vmfloat3 f3PosTopLeftCWS, f3PosTopRightCWS, f3PosBottomLeftCWS, f3PosBottomRightCWS;
	cobj->GetImagePlaneRectPointsf(CoordSpaceWORLD,
		&f3PosTopLeftCWS,
		&f3PosTopRightCWS,
		&f3PosBottomLeftCWS,
		&f3PosBottomRightCWS);

	int iPlaneSizeX = num_interpolation;
	float fPlanePitch = fExPlaneWidth / iPlaneSizeX;
	float fPlaneSizeY = fExPlaneHeight / fPlanePitch;
	float fPlaneCenterY = fPlaneSizeY * 0.5f;
	
	vmmat44f matScale, matTranslate;
	float fPlanePixelPitch = (float)fPlanePitch;
	fMatrixScaling(&matScale, &vmfloat3(fPlanePixelPitch, fPlanePixelPitch, fPlanePixelPitch));
	float fHalfPlaneWidth = (float)(fExPlaneWidth * 0.5);
	float fHalfPlaneHeight = (float)(fExPlaneHeight * 0.5);
	fMatrixTranslation(&matTranslate, &vmfloat3(-fHalfPlaneWidth, -fHalfPlaneHeight, -fPlanePixelPitch * 0.5f));
	vmmat44f matCOS2CWS = matScale * matTranslate;
	vmmat44f matCWS2COS;
	fMatrixInverse(&matCWS2COS, &matCOS2CWS);
	
	vmfloat3 f3PosTopLeftCOS, f3PosTopRightCOS, f3PosBottomLeftCOS, f3PosBottomRightCOS; 
	fTransformPoint(&f3PosTopLeftCOS, &f3PosTopLeftCWS, &matCWS2COS);
	fTransformPoint(&f3PosTopRightCOS, &f3PosTopRightCWS, &matCWS2COS); 
	fTransformPoint(&f3PosBottomLeftCOS, &f3PosBottomLeftCWS, &matCWS2COS);
	fTransformPoint(&f3PosBottomRightCOS, &f3PosBottomRightCWS, &matCWS2COS);
	
	int iThicknessStep = (int)ceil(fPlaneThickness / sample_dist); /* Think "fPlaneThickness > fMinPitch"!! */\
	sample_dist = fPlaneThickness / (float)iThicknessStep;

	{
		cb_curvedSlicer.posTopLeftCOS = f3PosTopLeftCOS;
		cb_curvedSlicer.posTopRightCOS = f3PosTopRightCOS;
		cb_curvedSlicer.posBottomLeftCOS = f3PosBottomLeftCOS;
		cb_curvedSlicer.posBottomRightCOS = f3PosBottomRightCOS;
		cb_curvedSlicer.numCurvePoints = num_interpolation;
		cb_curvedSlicer.planeHeight = fPlaneSizeY;
		cb_curvedSlicer.thicknessPlane = fPlaneThickness;
		cb_curvedSlicer.__dummy0 = iThicknessStep;
		vmfloat3 curvedPlaneUp;
		fNormalizeVector(&curvedPlaneUp, &vtrCurveUpVectors[0]);
		cb_curvedSlicer.planeUp = curvedPlaneUp * fPlanePitch;
		cb_curvedSlicer.flag = bIsRightSide;
	}
}

std::wstring s2ws(const std::string& s)
{
	int len;
	int slength = (int)s.length() + 1;
	len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
	wchar_t* buf = new wchar_t[len];
	MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
	std::wstring r(buf);
	delete[] buf;
	return r;
}

bool grd_helper::Compile_Hlsl(const string& str, const string& entry_point, const string& shader_model, D3D10_SHADER_MACRO* defines, void** sm)
{
	ID3DBlob* pShaderBlob = NULL;
	ID3DBlob* pErrorBlob = NULL;
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows 
	// the shaders to be optimized and to run exactly the way they will run in 
	// the release configuration of this program.
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif
	if (D3DCompileFromFile(s2ws(str).c_str(), defines, NULL, entry_point.c_str(), shader_model.c_str(), dwShaderFlags, 0, &pShaderBlob, &pErrorBlob) != S_OK)
	{
		if (pErrorBlob != NULL)
			OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
		VMSAFE_RELEASE(pErrorBlob);
		return false;
	}
	if (shader_model.find("ps") != string::npos)
	{
		if (g_pvmCommonParams->dx11Device->CreatePixelShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, (ID3D11PixelShader**)sm) != S_OK)
		{
			std::cout << "*** COMPILE ERROR : " << str << ", " << entry_point << ", " << shader_model << endl;
			return false;
		}
	}
	else if (shader_model.find("vs") != string::npos)
	{
		if (g_pvmCommonParams->dx11Device->CreateVertexShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, (ID3D11VertexShader**)sm) != S_OK)
		{
			std::cout << "*** COMPILE ERROR : " << str << ", " << entry_point << ", " << shader_model << endl;
			return false;
		}
	}
	else if (shader_model.find("cs") != string::npos)
	{
		if (g_pvmCommonParams->dx11Device->CreateComputeShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, (ID3D11ComputeShader**)sm) != S_OK)
		{
			std::cout << "*** COMPILE ERROR : " << str << ", " << entry_point << ", " << shader_model << endl;
			return false;
		}
	}
	else if (shader_model.find("gs") != string::npos)
	{
		if (g_pvmCommonParams->dx11Device->CreateGeometryShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, (ID3D11GeometryShader**)sm) != S_OK)
		{
			std::cout << "*** COMPILE ERROR : " << str << ", " << entry_point << ", " << shader_model << endl;
			return false;
		}
	}
	VMSAFE_RELEASE(pShaderBlob);
	VMSAFE_RELEASE(pErrorBlob);
	return true;
};

void grd_helper::__TestOutErrors()
{	
//#if (defined(_DEBUG) || defined(DEBUG)) && !defined(SDK_REDISTRIBUTE)
//#endif
}

bool grd_helper::CollisionCheck(const vmmat44f& matWS2OS, const AaBbMinMax& aabb_os, const vmfloat3& ray_origin_ws, const vmfloat3& ray_dir_ws) 
{
	struct CollisionAABox
	{
		BoundingBox aabox;
		ContainmentType collision;
	};

	struct CollisionRay
	{
		XMVECTOR origin;
		XMVECTOR direction;
	};

	vmfloat3 pos_center_os = vmfloat3((aabb_os.pos_min + aabb_os.pos_max) * 0.5);
	vmfloat3 ext_os = vmfloat3(aabb_os.pos_max) - pos_center_os;
	vmfloat3 ray_origin_os, ray_dir_os;
	vmmath::fTransformPoint(&ray_origin_os, &ray_origin_ws, &matWS2OS);
	vmmath::fTransformVector(&ray_dir_os, &ray_dir_ws, &matWS2OS);
	vmmath::fNormalizeVector(&ray_dir_os, &ray_dir_os);

	CollisionAABox dxAabb;
	dxAabb.aabox.Center = XMFLOAT3(pos_center_os.x, pos_center_os.y, pos_center_os.z);
	dxAabb.aabox.Extents = XMFLOAT3(ext_os.x, ext_os.y, ext_os.z);
	dxAabb.collision = DISJOINT;

	CollisionRay dxRay;
	dxRay.origin = XMVectorSet(ray_origin_os.x, ray_origin_os.y, ray_origin_os.z, 1.f);
	dxRay.direction = XMVectorSet(ray_dir_os.x, ray_dir_os.y, ray_dir_os.z, 0);
	float fDist = 0;
	if (dxAabb.aabox.Intersects(dxRay.origin, dxRay.direction, fDist))
		return true;
	return false;
}