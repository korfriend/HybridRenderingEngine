#include "gpures_helper.h"
#include "D3DCompiler.h"
#include <iostream>

using namespace grd_helper;

namespace grd_helper
{
	static GpuDX11CommonParameters g_VmCommonParams;
	static VmGpuManager* g_pCGpuManager = NULL;
}

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

int grd_helper::InitializePresettings(VmGpuManager* pCGpuManager, GpuDX11CommonParameters& gpu_params)
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

	if (g_VmCommonParams.is_initialized)
	{
		gpu_params = g_VmCommonParams;
		return __ALREADY_CHECKED;
	}

#ifdef USE_DX11_3
	g_pCGpuManager->GetDeviceInformation((void*)(&g_VmCommonParams.dx11Device), "DEVICE_POINTER_3");
	g_pCGpuManager->GetDeviceInformation((void*)(&g_VmCommonParams.dx11DeviceImmContext), "DEVICE_CONTEXT_POINTER_3");
#else
	g_pCGpuManager->GetDeviceInformation((void*)(&g_VmCommonParams.dx11Device), "DEVICE_POINTER");
	g_pCGpuManager->GetDeviceInformation((void*)(&g_VmCommonParams.dx11DeviceImmContext), "DEVICE_CONTEXT_POINTER");
#endif
	g_pCGpuManager->GetDeviceInformation(&g_VmCommonParams.dx11_adapter, "DEVICE_ADAPTER_DESC");
	g_pCGpuManager->GetDeviceInformation(&g_VmCommonParams.dx11_featureLevel, "FEATURE_LEVEL");
	//return __SUPPORTED_GPU;

	if (g_VmCommonParams.dx11Device == NULL || g_VmCommonParams.dx11DeviceImmContext == NULL)
	{
		gpu_params = g_VmCommonParams;
		return (int)__INVALID_GPU;
	}

	g_VmCommonParams.Delete();
	
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
		hr |= g_VmCommonParams.dx11Device->CreateBlendState(&descBlend, &blender_state);
		g_VmCommonParams.safe_set_res(COMRES_INDICATOR(BLEND_STATE, "ADD"), blender_state);

		D3D11_RASTERIZER_DESC2 descRaster;
		ZeroMemory(&descRaster, sizeof(D3D11_RASTERIZER_DESC));
		descRaster.FillMode = D3D11_FILL_SOLID;
		descRaster.CullMode = D3D11_CULL_BACK;
		descRaster.FrontCounterClockwise = FALSE;
		descRaster.DepthBias = 0;
		descRaster.DepthBiasClamp = 0;
		descRaster.SlopeScaledDepthBias = 0;
		descRaster.DepthClipEnable = TRUE;
		descRaster.ScissorEnable = TRUE;
		descRaster.MultisampleEnable = FALSE;
		descRaster.AntialiasedLineEnable = FALSE;
		descRaster.ForcedSampleCount = 0;
		descRaster.ConservativeRaster = D3D11_CONSERVATIVE_RASTERIZATION_MODE_OFF;
		ID3D11RasterizerState2* raster_state;
		hr |= g_VmCommonParams.dx11Device->CreateRasterizerState2(&descRaster, &raster_state);
		g_VmCommonParams.safe_set_res(COMRES_INDICATOR(RASTERIZER_STATE, "SOLID_CW"), raster_state);
		descRaster.AntialiasedLineEnable = true;
		hr |= g_VmCommonParams.dx11Device->CreateRasterizerState2(&descRaster, &raster_state);
		g_VmCommonParams.safe_set_res(COMRES_INDICATOR(RASTERIZER_STATE, "AA_SOLID_CW"), raster_state);
		//descRaster.CullMode = D3D11_CULL_FRONT;
		descRaster.CullMode = D3D11_CULL_BACK;
		descRaster.FrontCounterClockwise = TRUE;
		descRaster.AntialiasedLineEnable = false;
		hr |= g_VmCommonParams.dx11Device->CreateRasterizerState2(&descRaster, &raster_state);
		g_VmCommonParams.safe_set_res(COMRES_INDICATOR(RASTERIZER_STATE, "SOLID_CCW"), raster_state);
		descRaster.AntialiasedLineEnable = true;
		hr |= g_VmCommonParams.dx11Device->CreateRasterizerState2(&descRaster, &raster_state);
		g_VmCommonParams.safe_set_res(COMRES_INDICATOR(RASTERIZER_STATE, "AA_SOLID_CCW"), raster_state);
		descRaster.CullMode = D3D11_CULL_NONE;
		descRaster.AntialiasedLineEnable = false;
		hr |= g_VmCommonParams.dx11Device->CreateRasterizerState2(&descRaster, &raster_state);
		g_VmCommonParams.safe_set_res(COMRES_INDICATOR(RASTERIZER_STATE, "SOLID_NONE"), raster_state);
		descRaster.AntialiasedLineEnable = true;
		hr |= g_VmCommonParams.dx11Device->CreateRasterizerState2(&descRaster, &raster_state);
		g_VmCommonParams.safe_set_res(COMRES_INDICATOR(RASTERIZER_STATE, "AA_SOLID_NONE"), raster_state);

		descRaster.FillMode = D3D11_FILL_WIREFRAME;
		descRaster.CullMode = D3D11_CULL_BACK;
		descRaster.ConservativeRaster = D3D11_CONSERVATIVE_RASTERIZATION_MODE_OFF;
		descRaster.AntialiasedLineEnable = false;
		hr |= g_VmCommonParams.dx11Device->CreateRasterizerState2(&descRaster, &raster_state);
		g_VmCommonParams.safe_set_res(COMRES_INDICATOR(RASTERIZER_STATE, "WIRE_CW"), raster_state);
		descRaster.AntialiasedLineEnable = true;
		hr |= g_VmCommonParams.dx11Device->CreateRasterizerState2(&descRaster, &raster_state);
		g_VmCommonParams.safe_set_res(COMRES_INDICATOR(RASTERIZER_STATE, "AA_WIRE_CW"), raster_state);
		descRaster.CullMode = D3D11_CULL_FRONT;
		descRaster.AntialiasedLineEnable = false;
		hr |= g_VmCommonParams.dx11Device->CreateRasterizerState2(&descRaster, &raster_state);
		g_VmCommonParams.safe_set_res(COMRES_INDICATOR(RASTERIZER_STATE, "WIRE_CCW"), raster_state);
		descRaster.AntialiasedLineEnable = true;
		hr |= g_VmCommonParams.dx11Device->CreateRasterizerState2(&descRaster, &raster_state);
		g_VmCommonParams.safe_set_res(COMRES_INDICATOR(RASTERIZER_STATE, "AA_WIRE_CCW"), raster_state);
		descRaster.CullMode = D3D11_CULL_NONE;
		descRaster.AntialiasedLineEnable = false;
		hr |= g_VmCommonParams.dx11Device->CreateRasterizerState2(&descRaster, &raster_state);
		g_VmCommonParams.safe_set_res(COMRES_INDICATOR(RASTERIZER_STATE, "WIRE_NONE"), raster_state);
		descRaster.AntialiasedLineEnable = true;
		hr |= g_VmCommonParams.dx11Device->CreateRasterizerState2(&descRaster, &raster_state);
		g_VmCommonParams.safe_set_res(COMRES_INDICATOR(RASTERIZER_STATE, "AA_WIRE_NONE"), raster_state);
	}
	{
		D3D11_DEPTH_STENCIL_DESC descDepthStencil;
		ZeroMemory(&descDepthStencil, sizeof(D3D11_DEPTH_STENCIL_DESC));
		descDepthStencil.DepthEnable = TRUE;
		descDepthStencil.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		descDepthStencil.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		descDepthStencil.StencilEnable = FALSE;
		ID3D11DepthStencilState* ds_state;
		hr |= g_VmCommonParams.dx11Device->CreateDepthStencilState(&descDepthStencil, &ds_state);
		g_VmCommonParams.safe_set_res(COMRES_INDICATOR(DEPTHSTENCIL_STATE, "LESSEQUAL"), ds_state);
		descDepthStencil.DepthFunc = D3D11_COMPARISON_ALWAYS;
		hr |= g_VmCommonParams.dx11Device->CreateDepthStencilState(&descDepthStencil, &ds_state);
		g_VmCommonParams.safe_set_res(COMRES_INDICATOR(DEPTHSTENCIL_STATE, "ALWAYS"), ds_state);
		descDepthStencil.DepthFunc = D3D11_COMPARISON_GREATER;
		hr |= g_VmCommonParams.dx11Device->CreateDepthStencilState(&descDepthStencil, &ds_state);
		g_VmCommonParams.safe_set_res(COMRES_INDICATOR(DEPTHSTENCIL_STATE, "GREATER"), ds_state);
		descDepthStencil.DepthFunc = D3D11_COMPARISON_LESS;
		hr |= g_VmCommonParams.dx11Device->CreateDepthStencilState(&descDepthStencil, &ds_state);
		g_VmCommonParams.safe_set_res(COMRES_INDICATOR(DEPTHSTENCIL_STATE, "LESS"), ds_state);
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
		hr |= g_VmCommonParams.dx11Device->CreateSamplerState(&descSampler, &sampler_state);
		g_VmCommonParams.safe_set_res(COMRES_INDICATOR(SAMPLER_STATE, "POINT_CLAMP"), sampler_state);
		descSampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;	// NEAREST by ROUND
		hr |= g_VmCommonParams.dx11Device->CreateSamplerState(&descSampler, &sampler_state);
		g_VmCommonParams.safe_set_res(COMRES_INDICATOR(SAMPLER_STATE, "LINEAR_CLAMP"), sampler_state);

		descSampler.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
		descSampler.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
		descSampler.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
		descSampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;	// NEAREST by ROUND
		hr |= g_VmCommonParams.dx11Device->CreateSamplerState(&descSampler, &sampler_state);
		g_VmCommonParams.safe_set_res(COMRES_INDICATOR(SAMPLER_STATE, "LINEAR_ZEROBORDER"), sampler_state);
		descSampler.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;	// NEAREST by ROUND
		hr |= g_VmCommonParams.dx11Device->CreateSamplerState(&descSampler, &sampler_state);
		g_VmCommonParams.safe_set_res(COMRES_INDICATOR(SAMPLER_STATE, "POINT_ZEROBORDER"), sampler_state);

		descSampler.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		descSampler.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		descSampler.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		descSampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;	// NEAREST by ROUND
		hr |= g_VmCommonParams.dx11Device->CreateSamplerState(&descSampler, &sampler_state);
		g_VmCommonParams.safe_set_res(COMRES_INDICATOR(SAMPLER_STATE, "LINEAR_WRAP"), sampler_state);
		descSampler.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;	// NEAREST by ROUND
		hr |= g_VmCommonParams.dx11Device->CreateSamplerState(&descSampler, &sampler_state);
		g_VmCommonParams.safe_set_res(COMRES_INDICATOR(SAMPLER_STATE, "POINT_WRAP"), sampler_state);
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
		hr |= g_VmCommonParams.dx11Device->CreateBuffer(&descCB, NULL, &cbuffer);\
		g_VmCommonParams.safe_set_res(COMRES_INDICATOR(BUFFER, string(#STRUCT)), cbuffer);}\
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
	}
	if(hr != S_OK)
		goto ERROR_PRESETTING;

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
			if (PresetCompiledShader(g_VmCommonParams.dx11Device, hModule, pSrcResource, "vs_5_0", (ID3D11DeviceChild**)&vshader
				, in_layout_desc, num_elements, &in_layout) != S_OK)
			{
				VMSAFE_RELEASE(in_layout);
				VMSAFE_RELEASE(vshader);
				return E_FAIL;
			}
			if(in_layout)
				g_VmCommonParams.safe_set_res(COMRES_INDICATOR(INPUT_LAYOUT, name_layer), in_layout);
			g_VmCommonParams.safe_set_res(COMRES_INDICATOR(VERTEX_SHADER, name_shader), vshader);
			return S_OK;
		};
		auto register_shader = [&](const LPCWSTR pSrcResource, const string& name_shader, const string& profile)
		{
			ID3D11DeviceChild* shader = NULL;
			if (PresetCompiledShader(g_VmCommonParams.dx11Device, hModule, pSrcResource, profile.c_str(), (ID3D11DeviceChild**)&shader, NULL, 0, NULL) != S_OK)
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

			g_VmCommonParams.safe_set_res(COMRES_INDICATOR(_type, name_shader), shader);
			return S_OK;
		};

#define VRETURN(v) if(v != S_OK) goto ERROR_PRESETTING;

		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA11001), "SR_OIT_P_vs_5_0", "P", lotypeInputPos, 1));
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA11002), "SR_OIT_PN_vs_5_0", "PN", lotypeInputPosNor, 2));
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA11003), "SR_OIT_PT_vs_5_0", "PT", lotypeInputPosTex, 2));
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA11004), "SR_OIT_PNT_vs_5_0", "PNT", lotypeInputPosNorTex, 3));
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA11005), "SR_OIT_PTTT_vs_5_0", "PTTT", lotypeInputPosTTTex, 4));

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11010), "SR_OIT_KDEPTH_PHONGBLINN_ps_5_0", "ps_5_1"));
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11011), "SR_OIT_KDEPTH_DASHEDLINE_ps_5_0", "ps_5_0"));
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11012), "SR_OIT_KDEPTH_MULTITEXTMAPPING_ps_5_0", "ps_5_0"));
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11013), "SR_OIT_KDEPTH_TEXTMAPPING_ps_5_0", "ps_5_0"));
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11014), "SR_OIT_KDEPTH_TEXTUREIMGMAP_ps_5_0", "ps_5_0"));
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11015), "SR_SINGLE_LAYER_ps_5_0", "ps_5_0"));
		//VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11016), "SR_OIT_KDEPTH_NPRGHOST_ps_5_0", "ps_5_0"));

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11020), "SR_OIT_ABUFFER_FRAGCOUNTER_ps_5_0", "ps_5_0"));
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11021), "SR_OIT_ABUFFER_PHONGBLINN_ps_5_0", "ps_5_0"));
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11022), "SR_OIT_ABUFFER_DASHEDLINE_ps_5_0", "ps_5_0"));
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11023), "SR_OIT_ABUFFER_MULTITEXTMAPPING_ps_5_0", "ps_5_0"));
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11024), "SR_OIT_ABUFFER_TEXTMAPPING_ps_5_0", "ps_5_0"));
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11025), "SR_OIT_ABUFFER_TEXTUREIMGMAP_ps_5_0", "ps_5_0"));

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11030), "SR_MOMENT_GEN_ps_5_0", "ps_5_0"));
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11031), "SR_MOMENT_OIT_PHONGBLINN_ps_5_0", "ps_5_0"));
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11032), "SR_MOMENT_OIT_DASHEDLINE_ps_5_0", "ps_5_0"));
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11033), "SR_MOMENT_OIT_MULTITEXTMAPPING_ps_5_0", "ps_5_0"));
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11034), "SR_MOMENT_OIT_TEXTMAPPING_ps_5_0", "ps_5_0"));
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11035), "SR_MOMENT_OIT_TEXTUREIMGMAP_ps_5_0", "ps_5_0"));

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA21000), "SR_OIT_SORT2RENDER_cs_5_0", "cs_5_0"));
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA21001), "SR_OIT_PRESET_cs_5_0", "cs_5_0"));
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA21002), "SR_OIT_ABUFFER_PREFIX_0_cs_5_0", "cs_5_0"));
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA21003), "SR_OIT_ABUFFER_PREFIX_1_cs_5_0", "cs_5_0"));
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA21004), "SR_OIT_ABUFFER_SORT2SENDER_cs_5_0", "cs_5_0"));
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA21100), "KB_SSAO_cs_5_0", "cs_5_0"));
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA21101), "KB_SSAO_BLUR_cs_5_0", "cs_5_0"));
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA21102), "KBZ_TO_TEXTURE_cs_5_0", "cs_5_0"));
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA31010), "GS_ThickPoints_gs_5_0", "gs_5_0"));
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA31020), "GS_ThickLines_gs_5_0", "gs_5_0"));

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50000), "VR_RAYMAX_cs_5_0", "cs_5_0"));
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50001), "VR_RAYMIN_cs_5_0", "cs_5_0"));
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50002), "VR_RAYSUM_cs_5_0", "cs_5_0"));
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50003), "VR_DEFAULT_cs_5_0", "cs_5_0"));
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50004), "VR_OPAQUE_cs_5_0", "cs_5_0"));
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50005), "VR_CONTEXT_cs_5_0", "cs_5_0"));
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50006), "VR_SURFACE_cs_5_0", "cs_5_0"));
	}

	g_VmCommonParams.is_initialized = true;
	gpu_params = g_VmCommonParams;

	if (hr != S_OK)
		return __INVALID_GPU;

	if (g_VmCommonParams.dx11_featureLevel == 0x9300 || g_VmCommonParams.dx11_featureLevel == 0xA000)
		return __LOW_SPEC_NOT_SUPPORT_CS_GPU;

	return __SUPPORTED_GPU;

ERROR_PRESETTING:
	DeinitializePresettings();
	gpu_params = g_VmCommonParams;
	return __INVALID_GPU;
}

void grd_helper::DeinitializePresettings()
{
	if (!g_VmCommonParams.is_initialized) return;

	if (g_VmCommonParams.dx11DeviceImmContext)
	{
		g_VmCommonParams.dx11DeviceImmContext->Flush();
		g_VmCommonParams.dx11DeviceImmContext->ClearState();
	}

	g_VmCommonParams.Delete();

	if (g_VmCommonParams.dx11DeviceImmContext)
	{
		g_VmCommonParams.dx11DeviceImmContext->Flush();
		g_VmCommonParams.dx11DeviceImmContext->ClearState();
	}

	g_VmCommonParams.dx11Device = NULL;
	g_VmCommonParams.dx11DeviceImmContext = NULL;
	memset(&g_VmCommonParams.dx11_adapter, NULL, sizeof(DXGI_ADAPTER_DESC));
	g_VmCommonParams.dx11_featureLevel = D3D_FEATURE_LEVEL_9_1;
	g_VmCommonParams.is_initialized = false;
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
	const int blk_level = 1;	// 0 : High Resolution, 1 : Low Resolution
	VolumeBlocks* volblk = ((VmVObjectVolume*)vobj)->GetVolumeBlock(blk_level);

	gres.res_dvalues["WIDTH"] = (double)volblk->blk_vol_size.x;
	gres.res_dvalues["HEIGHT"] = (double)volblk->blk_vol_size.y;
	gres.res_dvalues["DEPTH"] = (double)volblk->blk_vol_size.z;

	uint num_blk_units = volblk->blk_vol_size.x * volblk->blk_vol_size.y * volblk->blk_vol_size.z;
	gres.options["FORMAT"] = (vmode == "OTFTAG") && num_blk_units >= 65535 ? DXGI_FORMAT_R32_FLOAT : DXGI_FORMAT_R16_UNORM;

	g_pCGpuManager->GenerateGpuResource(gres);

	return 1;
}

bool grd_helper::UpdateOtfBlocks(GpuRes& gres, const VmVObjectVolume* vobj, const VmTObject	* tobj,
	const bool update_blks, const int sculpt_value, LocalProgress* progress)
{
	map<int, VmTObject*> mapTObjects;
	mapTObjects.insert(pair<int, VmTObject*>(tobj->GetObjectID(), (VmTObject*)tobj));
	return UpdateOtfBlocks(gres, vobj, NULL, mapTObjects, tobj->GetObjectID(), NULL, 0, update_blks, false, sculpt_value, progress);
}

bool grd_helper::UpdateOtfBlocks(GpuRes& gres, const VmVObjectVolume* main_vobj, const VmVObjectVolume* mask_vobj,
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

	ullong _tp = vmhelpers::GetCurrentTimePack();
	((VmVObjectVolume*)main_vobj)->RegisterCustomParameter("_int2_LatestVolBlkUpdateTime", *(vmint2*)&_tp);

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
	HRESULT hr = g_VmCommonParams.dx11DeviceImmContext->Map(pdx11tx3d_blkmap, 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes);

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

	g_VmCommonParams.dx11DeviceImmContext->Unmap(pdx11tx3d_blkmap, 0);
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
	HRESULT hr = g_VmCommonParams.dx11DeviceImmContext->Map(pdx11tx3d_min_blk, 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes_Min);
	hr |= g_VmCommonParams.dx11DeviceImmContext->Map(pdx11tx3d_max_blk, 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes_Max);

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
	g_VmCommonParams.dx11DeviceImmContext->Unmap(pdx11tx3d_min_blk, 0);
	g_VmCommonParams.dx11DeviceImmContext->Unmap(pdx11tx3d_max_blk, 0);
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

auto GetParam = [](const GpuRes& gres, const std::string& param_name) -> double
{
	auto it = gres.res_dvalues.find(param_name);
	if (it == gres.res_dvalues.end())
	{
		::MessageBoxA(NULL, "Error GPU Parameters!!", NULL, MB_OK);
		return 0;
	}
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


bool grd_helper::UpdateVolumeModel(GpuRes& gres, const VmVObjectVolume* vobj, const bool use_nearest_max, LocalProgress* progress)
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
	HRESULT hr = g_VmCommonParams.dx11DeviceImmContext->Map(pdx11tx3d_volume, 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes);
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
	g_VmCommonParams.dx11DeviceImmContext->Unmap(pdx11tx3d_volume, 0);

	return true;
}

bool grd_helper::UpdateTMapBuffer(GpuRes& gres, const VmTObject* main_tobj,
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
	g_VmCommonParams.dx11DeviceImmContext->Map((ID3D11Resource*)gres.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes);
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
	g_VmCommonParams.dx11DeviceImmContext->Unmap((ID3D11Resource*)gres.alloc_res_ptrs[DTYPE_RES], 0);

	return true;
}

bool grd_helper::UpdatePrimitiveModel(GpuRes& gres_vtx, GpuRes& gres_idx, map<string, GpuRes>& map_gres_texs, VmVObjectPrimitive* pobj, LocalProgress* progress)
{
	PrimitiveData* prim_data = ((VmVObjectPrimitive*)pobj)->GetPrimitiveData();

	auto CheckReusability = [&pobj](GpuRes& gres, bool& update_data)
	{
		auto it = gres.res_dvalues.find("LASTEST_GENCALL_TIME");
		unsigned long long _gpu_gen_timg = 0;
		if (it != gres.res_dvalues.end())
			_gpu_gen_timg = *(unsigned long long*)&it->second;
		unsigned long long _cpu_gen_timg = pobj->GetContentsUpdateTime();
		if (_gpu_gen_timg < _cpu_gen_timg)
		{
			// now, at least update
			update_data = true;
			bool is_reuse_memory = false;
			pobj->GetCustomParameter("_bool_ReuseGpuMemory", data_type::dtype<bool>(), &is_reuse_memory);
			gres.options["REUSE_MEMORY"] = (int)is_reuse_memory;
		}
	};

	bool update_data = false;
	pobj->GetCustomParameter("_bool_UpdateData", data_type::dtype<bool>(), &update_data);
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
			gres_vtx.res_dvalues["NUM_ELEMENTS"] = prim_data->num_vtx;
			gres_vtx.res_dvalues["STRIDE_BYTES"] = stride_bytes;

			update_data = true;
			g_pCGpuManager->GenerateGpuResource(gres_vtx);
		}
		else
		{
			CheckReusability(gres_vtx, update_data);
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
				g_VmCommonParams.dx11DeviceImmContext->UpdateSubresource(pdx11bufvtx, 0, NULL, subres.pSysMem, subres.SysMemPitch, 0);
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
			gres_idx.options["USAGE"] = D3D11_USAGE_DEFAULT;
			gres_idx.options["CPU_ACCESS_FLAG"] = NULL;
			gres_idx.options["BIND_FLAG"] = D3D11_BIND_INDEX_BUFFER;
			gres_idx.options["FORMAT"] = DXGI_FORMAT_R32_UINT;
			gres_idx.res_dvalues["NUM_ELEMENTS"] = prim_data->num_vidx;
			gres_idx.res_dvalues["STRIDE_BYTES"] = sizeof(uint);

			g_pCGpuManager->GenerateGpuResource(gres_idx);


			//D3D11_MAPPED_SUBRESOURCE mappedRes;
			//g_VmCommonParams.dx11DeviceImmContext->Map(pdx11bufidx, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes);
			//uint* vidx_buf = (uint*)mappedRes.pData;
			//memcpy(vidx_buf, prim_data->vidx_buffer, prim_data->num_vidx * sizeof(uint));
			//g_VmCommonParams.dx11DeviceImmContext->Unmap(pdx11bufidx, NULL);
		}
		else
		{
			CheckReusability(gres_idx, update_data);
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
				g_VmCommonParams.dx11DeviceImmContext->UpdateSubresource(pdx11bufidx, 0, NULL, subres.pSysMem, subres.SysMemPitch, 0);
				VMSAFE_DELETEARRAY(subres.pSysMem);
			}
		}
	}

	//vmint3 tex_res_size;
	//((VmVObjectPrimitive*)pobj)->GetCustomParameter("_int3_TextureWHN", data_type::dtype<vmint3>(), &tex_res_size);
	bool has_texture_img = false;
	pobj->GetCustomParameter("_bool_HasTextureMap", data_type::dtype<bool>(), &has_texture_img);
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
			g_VmCommonParams.dx11DeviceImmContext->UpdateSubresource(pdx11tx2dres, 0, NULL, subres.pSysMem, subres.SysMemPitch, subres.SysMemSlicePitch);
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
				gres_tex.res_dvalues["WIDTH"] = tex_res_size.x;
				gres_tex.res_dvalues["HEIGHT"] = tex_res_size.y;
				gres_tex.res_dvalues["DEPTH"] = (double)prim_data->texture_res_info.size();
				
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
				CheckReusability(gres_tex, update_data);
				g_pCGpuManager->GenerateGpuResource(gres_tex);
			}

			if (update_data)
				upload_teximg(gres_tex, tex_res_size, byte_stride);

			map_gres_texs["MAP_COLOR4"] = gres_tex;
		}
		else
		{
			double Ns;
			pobj->GetCustomParameter("_double_Ns", data_type::dtype<double>(), &Ns);

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
					g_VmCommonParams.dx11DeviceImmContext->UpdateSubresource(pdx11tx2dres, 0, NULL, subres.pSysMem, subres.SysMemPitch, subres.SysMemSlicePitch);
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
					gres_tex.res_dvalues["WIDTH"] = tex_res_size.x;
					gres_tex.res_dvalues["HEIGHT"] = tex_res_size.y;

					g_pCGpuManager->GenerateGpuResource(gres_tex);
				}
				else
				{
					CheckReusability(gres_tex, update_data);
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

	gres.options["USAGE"] = (fb_flag & UPFB_SYSOUT) ? D3D11_USAGE_STAGING : D3D11_USAGE_DEFAULT;
	gres.options["CPU_ACCESS_FLAG"] = (fb_flag & UPFB_SYSOUT) ? D3D11_CPU_ACCESS_READ : NULL;
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
		case DXGI_FORMAT_R32G32B32A32_UINT: stride_bytes = sizeof(vmuint4); break;
		case DXGI_FORMAT_UNKNOWN: stride_bytes = structured_stride; break;
		case DXGI_FORMAT_R32_TYPELESS: stride_bytes = sizeof(uint); break;
		default:
			return false;
		}
		if (fb_flag & UPFB_RAWBYTE) gres.options["RAW_ACCESS"] = 1;
		if (stride_bytes == 0) return false;
		gres.res_dvalues["STRIDE_BYTES"] = (double)(stride_bytes);
		break;
	case RTYPE_TEXTURE2D:
		gres.res_dvalues["WIDTH"] = (fb_flag & UPFB_HALF) ? fb_size.x / 2 : fb_size.x;
		gres.res_dvalues["HEIGHT"] = (fb_flag & UPFB_HALF) ? fb_size.y / 2 : fb_size.y;
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

bool grd_helper::CheckOtfAndVolBlobkUpdate(VmVObjectVolume* vobj, VmTObject* tobj)
{
	ullong latest_otf_update_time = 17;
	tobj->GetCustomParameter("_int2_LatestTmapUpdateTime", data_type::dtype<vmint2>(), &latest_otf_update_time);

	ullong latest_blk_update_time = 17;
	vobj->GetCustomParameter("_int2_LatestVolBlkUpdateTime", data_type::dtype<vmint2>(), &latest_blk_update_time);
	//if (latest_blk_update_time <= latest_otf_update_time)
	//{
	//	int v_ms = latest_blk_update_time & 0x3FF;
	//	int o_ms = latest_otf_update_time & 0x3FF;
	//	int v_s = (latest_blk_update_time >> 10) & 0x3F;
	//	int o_s = (latest_otf_update_time >> 10) & 0x3F;
	//	int v_m = (latest_blk_update_time >> 16) & 0x3F;
	//	int o_m = (latest_otf_update_time >> 16) & 0x3F;
	//	printf("** TMAP OBJ ID : %d, Volume %d:%d:%d vs OTF %d:%d:%d\n", tobj->GetObjectID(), v_m, v_s, v_ms, o_m, o_s, o_ms);
	//}
	return latest_blk_update_time <= latest_otf_update_time;
}

//#define *(vmfloat3*)&
//#define *(vmfloat4*)&
//#define *(XMMATRIX*)&

void grd_helper::SetCb_Camera(CB_CameraState& cb_cam, vmmat44f& matWS2PS, vmmat44f& matWS2SS, vmmat44f& matSS2WS, VmCObject* ccobj, const vmint2& fb_size, const int num_deep_layers, const float vz_thickness)
{
	vmmat44 dmatWS2CS, dmatCS2PS, dmatPS2SS;
	vmmat44 dmatSS2PS, dmatPS2CS, dmatCS2WS;
	ccobj->GetMatrixWStoSS(&dmatWS2CS, &dmatCS2PS, &dmatPS2SS);
	ccobj->GetMatrixSStoWS(&dmatSS2PS, &dmatPS2CS, &dmatCS2WS);
	vmmat44 dmatWS2PS = dmatWS2CS * dmatCS2PS;
	matWS2PS = dmatWS2PS;
	matWS2SS = dmatWS2PS * dmatPS2SS;
	matSS2WS = (dmatSS2PS * dmatPS2CS) * dmatCS2WS;

	cb_cam.mat_ss2ws = matSS2WS;
	cb_cam.mat_ws2ss = matWS2SS;

	vmfloat3 pos_cam, dir_cam;
	ccobj->GetCameraExtStatef(&pos_cam, &dir_cam, NULL);

	cb_cam.cam_flag = 0;
	if (ccobj->IsPerspective())
		cb_cam.cam_flag |= 0x1;

	cb_cam.pos_cam_ws = pos_cam;
	cb_cam.dir_view_ws = dir_cam;

	cb_cam.rt_width = (uint)fb_size.x;
	cb_cam.rt_height = (uint)fb_size.y;

	cb_cam.num_deep_layers = num_deep_layers;
	cb_cam.cam_vz_thickness = vz_thickness;
}

void grd_helper::SetCb_Env(CB_EnvState& cb_env, VmCObject* ccobj, VmFnContainer* _fncontainer, vmfloat3 simple_light_intensities)
{
	vmfloat3 pos_cam, dir_cam;
	ccobj->GetCameraExtStatef(&pos_cam, &dir_cam, NULL);

	bool light_on_cam = _fncontainer->GetParamValue("_bool_IsLightOnCamera", true);
	vmdouble3 dVecLightWS(dir_cam);
	vmdouble3 dPosLightWS(pos_cam);
	if (!light_on_cam)
	{
		dVecLightWS = _fncontainer->GetParamValue("_double3_VecLightWS", vmdouble3(dir_cam));
		dPosLightWS = _fncontainer->GetParamValue("_double3_PosLightWS", vmdouble3(pos_cam));
	}
	cb_env.dir_light_ws = (vmfloat3)dVecLightWS;
	cb_env.pos_light_ws = (vmfloat3)dPosLightWS;
	fNormalizeVector(&cb_env.dir_light_ws, &cb_env.dir_light_ws);

	bool is_spot_light = _fncontainer->GetParamValue("_bool_IsPointSpotLight", false);
	if (is_spot_light)
		cb_env.env_flag |= 0x1;

	bool apply_SSAO = _fncontainer->GetParamValue("_bool_ApplySSAO", false);
	cb_env.r_kernel_ao = 0;
	if (apply_SSAO)
	{
		cb_env.r_kernel_ao = (float)_fncontainer->GetParamValue("_double_SSAOKernalR", (double)5.0);
		cb_env.num_dirs = _fncontainer->GetParamValue("_int_SSAONumDirs", (int)8);
		cb_env.num_steps = _fncontainer->GetParamValue("_int_SSAONumSteps", (int)8);
		cb_env.tangent_bias = (float)_fncontainer->GetParamValue("_double_SSAOTangentBias", (double)VM_PI / 6.0);
	}

	cb_env.ltint_ambient = vmfloat4(simple_light_intensities.x);
	cb_env.ltint_diffuse = vmfloat4(simple_light_intensities.y);
	cb_env.ltint_spec = vmfloat4(simple_light_intensities.z);

	cb_env.num_lights = 1;

	//{
	//	vmdouble4 shading_factors = default_shading_factor;
	//	bool apply_shadingfactors = true;
	//	vobj->GetCustomParameter("_bool_ApplyShadingFactors", data_type::dtype<bool>(), &apply_shadingfactors);
	//	if (apply_shadingfactors)
	//		lobj->GetDstObjValue(vobj->GetObjectID(), "_double4_ShadingFactors", &shading_factors);
	//	cb_volume.pb_shading_factor = *(vmfloat4*)&vmfloat4(shading_factors);
	//}
}

void grd_helper::SetCb_VolumeObj(CB_VolumeObject& cb_volume, VmVObjectVolume* vobj, VmLObject* lobj, VmFnContainer* _fncontainer, const bool high_samplerate, const vmint3& vol_size, const int iso_value, const float volblk_valuerange, const float sample_rate, const int sculpt_index)
{
	VolumeData* vol_data = vobj->GetVolumeData();

	//bool invert_normal = false;
	//vobj->GetCustomParameter("_bool_IsInvertPlaneDirection", data_type::dtype<bool>(), &invert_normal);
	//if (invert_normal)
	//	cb_volume.vobj_flag |= (0x1);
	if (sculpt_index >= 0)
		cb_volume.vobj_flag |= sculpt_index << 24;

	vmmat44f matVS2TS;
	vmmat44f matShift, matScale;
	fMatrixTranslation(&matShift, &vmfloat3(0.5f));
	fMatrixScaling(&matScale, &vmfloat3(1.f / (float)(vol_data->vol_size.x + vol_data->vox_pitch.x * 0.00f),
		1.f / (float)(vol_data->vol_size.y + vol_data->vox_pitch.z * 0.00f), 1.f / (float)(vol_data->vol_size.z + vol_data->vox_pitch.z * 0.00f)));
	matVS2TS = matShift * matScale;
	cb_volume.mat_ws2ts = vobj->GetMatrixWS2OSf() * matVS2TS;

	const vmmat44f& matVS2WS = vobj->GetMatrixOS2WSf();

	AaBbMinMax aabb;
	vobj->GetOrthoBoundingBox(aabb);

	vmfloat3 fPosVObjectMinWS, fVecVObjectZWS, fVecVObjectYWS, fVecVObjectXWS;
	fTransformPoint(&fPosVObjectMinWS, &(vmfloat3)aabb.pos_min, &matVS2WS);
	fTransformVector(&fVecVObjectZWS, &vmfloat3(0, 0, -1.f), &matVS2WS);
	fTransformVector(&fVecVObjectYWS, &vmfloat3(0, 1.f, 0), &matVS2WS);
	fTransformVector(&fVecVObjectXWS, &vmfloat3(1.f, 0, 0), &matVS2WS);
	vmfloat3 fVecCrossZY;
	fCrossDotVector(&fVecCrossZY, &fVecVObjectZWS, &fVecVObjectYWS);
	if (fDotVector(&fVecCrossZY, &fVecVObjectXWS) < 0)
		fTransformVector(&fVecVObjectYWS, &vmfloat3(1.f, 0, 0), &matVS2WS);

	vmmat44f matWS2BS;
	fMatrixWS2CS(&matWS2BS, (vmfloat3*)&fPosVObjectMinWS, (vmfloat3*)&fVecVObjectYWS, (vmfloat3*)&fVecVObjectZWS); // set
	fTransformPoint((vmfloat3*)&cb_volume.pos_alignedvbox_max_bs, &(vmfloat3)aabb.pos_max, &(matVS2WS * matWS2BS)); // set
	cb_volume.mat_alignedvbox_tr_ws2bs = matWS2BS;

	if (vol_data->store_dtype.type_bytes == data_type::dtype<byte>().type_bytes) // char
		cb_volume.value_range = 255.f;
	else if (vol_data->store_dtype.type_bytes == data_type::dtype<ushort>().type_bytes) // short
		cb_volume.value_range = 65535.f;
	else GMERRORMESSAGE("UNSUPPORTED FORMAT : grd_helper::SetCb_VolumeObj");

	float minDistSample = (float)min(min(vol_data->vox_pitch.x, vol_data->vox_pitch.y), vol_data->vox_pitch.z);
	cb_volume.sample_dist = minDistSample;
	fTransformVector((vmfloat3*)&cb_volume.vec_grad_x, &vmfloat3(cb_volume.sample_dist, 0, 0), (vmmat44f*)&cb_volume.mat_ws2ts);
	fTransformVector((vmfloat3*)&cb_volume.vec_grad_y, &vmfloat3(0, cb_volume.sample_dist, 0), (vmmat44f*)&cb_volume.mat_ws2ts);
	fTransformVector((vmfloat3*)&cb_volume.vec_grad_z, &vmfloat3(0, 0, cb_volume.sample_dist), (vmmat44f*)&cb_volume.mat_ws2ts);
	cb_volume.opacity_correction = 1.f;
	if (high_samplerate)
	{
		//cb_volume.sample_dist *= 0.5f;
		//cb_volume.opacity_correction *= 0.5f;
		//
		//cb_volume.f3VecGradientSampleX *= 2.f;
		//cb_volume.f3VecGradientSampleY *= 2.f;
		//cb_volume.f3VecGradientSampleZ *= 2.f;
	}

	if (sample_rate > 0) // test
		cb_volume.sample_dist /= (float)sample_rate;

	cb_volume.vol_size = vmfloat3((float)vol_size.x, (float)vol_size.y, (float)vol_size.z);

	// from pmapDValueVolume //
	double dSamplePrecisionLevel = 1.0;
	double dThicknessVirtualzSlab = cb_volume.sample_dist;
	double ssao_intensity = _fncontainer->GetParamValue("_double_SSAOVrIntensity", 0.5);;

	int vobj_id = vobj->GetObjectID();
	if (lobj != NULL)
	{
		lobj->GetDstObjValue(vobj_id, "_double_SamplePrecisionLevel", &dSamplePrecisionLevel);
		lobj->GetDstObjValue(vobj_id, "_double_VZThickness", &dThicknessVirtualzSlab);
		lobj->GetDstObjValue(vobj_id, "_double_SSAOIntensity", &ssao_intensity);
	}

	float fSamplePrecisionLevel = (float)dSamplePrecisionLevel;
	if (fSamplePrecisionLevel > 0)
	{
		cb_volume.sample_dist /= fSamplePrecisionLevel;
		cb_volume.opacity_correction /= fSamplePrecisionLevel;
	}
	cb_volume.vz_thickness = (float)dThicknessVirtualzSlab;	// 현재 HLSL 은 Sample Distance 를 강제로 사용 중...
	cb_volume.iso_value = iso_value;
	cb_volume.ao_intensity = (float)ssao_intensity;

	//printf("sample_rate : %f\n", sample_rate);
	//printf("minDistSample : %f\n", minDistSample);
	//printf("fSamplePrecisionLevel : %f\n", fSamplePrecisionLevel);
	//printf("sample dist : %f\n", cb_volume.sample_dist);
	//printf("vz dist : %f\n", dThicknessVirtualzSlab);

	// volume blocks
	cb_volume.volblk_value_range = volblk_valuerange; // DOJO TO DO :  BLK 16UNORM ==> 16UINT... and forced to set 1
	const int blk_level = 1;	// 0 : High Resolution, 1 : Low Resolution
	VolumeBlocks* volblk = vobj->GetVolumeBlock(blk_level);
	if(volblk != NULL)
		cb_volume.volblk_size_ts = vmfloat3((float)volblk->unitblk_size.x / (float)vol_data->vol_size.x
		, (float)volblk->unitblk_size.y / (float)vol_data->vol_size.y, (float)volblk->unitblk_size.z / (float)vol_data->vol_size.z);
}

void grd_helper::SetCb_PolygonObj(CB_PolygonObject& cb_polygon, VmVObjectPrimitive* pobj, VmLObject* lobj, 
	const vmmat44f& matOS2WS, const vmmat44f& matWS2SS, const vmmat44f& matWS2PS, 
	const RenderObjInfo& rendering_obj_info, const double default_point_thickness, const double default_line_thickness)
{
	PrimitiveData* pobj_data = pobj->GetPrimitiveData();

	cb_polygon.mat_os2ws = matOS2WS;

	if (rendering_obj_info.is_annotation_obj)// && prim_data->texture_res)
	{
		vmint3 i3TextureWHN;
		pobj->GetCustomParameter("_int3_TextureWHN", data_type::dtype<vmint3>(), &i3TextureWHN);
		cb_polygon.num_letters = i3TextureWHN.z;
		if (i3TextureWHN.z == 1)
		{
			vmfloat3* pos_vtx = pobj_data->GetVerticeDefinition("POSITION");
			vmfloat3 f3Pos0SS, f3Pos1SS, f3Pos2SS;
			vmmat44f matOS2SS = matOS2WS * matWS2SS;

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
		if (!rendering_obj_info.use_vertex_color)
			cb_polygon.pobj_flag |= (0x1 << 3);
	}

	{
		// material setting
		cb_polygon.alpha = rendering_obj_info.fColor.a;
		vmdouble3 default_clr = rendering_obj_info.fColor;
		vmdouble3 _clr_Ka(default_clr), _clr_Kd(default_clr), _clr_Ks(default_clr);
		double Ns = 1.0;
		pobj->GetCustomParameter("_double3_Ka", data_type::dtype<vmdouble3>(), &_clr_Ka);
		pobj->GetCustomParameter("_double3_Kd", data_type::dtype<vmdouble3>(), &_clr_Kd);
		pobj->GetCustomParameter("_double3_Ks", data_type::dtype<vmdouble3>(), &_clr_Ks);
		pobj->GetCustomParameter("_double_Ns", data_type::dtype<double>(), &Ns);
		
		vmdouble3 illum_model;
		if (pobj->GetCustomParameter("_double3_IllumWavefront", data_type::dtype<vmdouble3>(), &illum_model))
		{
			_clr_Ka *= illum_model.x;
			_clr_Kd *= illum_model.y;
			_clr_Ks *= illum_model.z;
		}

		cb_polygon.Ka = (vmfloat3)_clr_Ka;
		cb_polygon.Kd = (vmfloat3)_clr_Kd;
		cb_polygon.Ks = (vmfloat3)_clr_Ks;
		cb_polygon.Ns = (float)Ns;
	}

	if (!rendering_obj_info.abs_diffuse)
		cb_polygon.pobj_flag |= (0x1 << 5);

	bool is_dashed_line = false;
	pobj->GetCustomParameter("_bool_IsDashed", data_type::dtype<bool>(), &is_dashed_line);
	if (is_dashed_line)
	{
		cb_polygon.pobj_flag |= (0x1 << 19);

		bool dashed_line_option_invclrs = false;
		pobj->GetCustomParameter("_bool_IsInvertColorDashLine", data_type::dtype<bool>(), &dashed_line_option_invclrs);
		if (dashed_line_option_invclrs)
			cb_polygon.pobj_flag |= (0x1 << 20);
	}

	int pobj_id = pobj->GetObjectID();
	double dashed_line_interval = 1.0;
	lobj->GetDstObjValue(pobj_id, "_double_LineDashInterval", &dashed_line_interval);

	cb_polygon.dash_interval = (float)dashed_line_interval;
	cb_polygon.vz_thickness = rendering_obj_info.vzthickness;
	cb_polygon.num_safe_loopexit = (uint)rendering_obj_info.num_safe_loopexit;

	vmmat44f matOS2PS = matOS2WS * matWS2PS;
	cb_polygon.mat_os2ps = matOS2PS;

	bool is_forced_depth_front_bias = rendering_obj_info.is_wireframe;
	pobj->GetCustomParameter("_bool_IsForcedDepthBias", data_type::dtype<bool>(), &is_forced_depth_front_bias);
	cb_polygon.depth_forward_bias = rendering_obj_info.is_wireframe ? rendering_obj_info.vzthickness : 0;

	bool force_to_pointsetrender = false;
	lobj->GetDstObjValue(pobj_id, "_bool_ForceToPointsetRender", &force_to_pointsetrender);
	double dPointThickness = default_point_thickness;
	double dLineThickness = default_line_thickness;
	lobj->GetDstObjValue(pobj_id, "_double_PointThickness", &dPointThickness);
	lobj->GetDstObjValue(pobj_id, "_double_LineThickness", &dLineThickness);
	if (pobj_data->ptype == PrimitiveTypePOINT || force_to_pointsetrender)
		cb_polygon.pix_thickness = (float)(dPointThickness / 2.);
	else if (pobj_data->ptype == PrimitiveTypeLINE)
		cb_polygon.pix_thickness = (float)(dLineThickness / 2.);
}

void grd_helper::SetCb_TMap(CB_TMAP& cb_tmap, VmTObject* tobj)
{
	TMapData* tobj_data = tobj->GetTMapData();
	cb_tmap.first_nonzeroalpha_index = tobj_data->valid_min_idx.x;
	cb_tmap.last_nonzeroalpha_index = tobj_data->valid_max_idx.x;
	cb_tmap.tmap_size = tobj_data->array_lengths.x;
	vmbyte4 y4ColorEnd = ((vmbyte4**)tobj_data->tmap_buffers)[0][tobj_data->array_lengths.x - 1];
	cb_tmap.last_color = vmfloat4(y4ColorEnd.x / 255.f, y4ColorEnd.y / 255.f, y4ColorEnd.z / 255.f, y4ColorEnd.w / 255.f);

	double d_min_v = 0, d_max_v = 0;
	tobj->GetCustomParameter("double_MappingMinValue", data_type::dtype<double>(), &d_min_v);
	tobj->GetCustomParameter("double_MappingMaxValue", data_type::dtype<double>(), &d_max_v);
	cb_tmap.mapping_v_min = (float)d_min_v;
	cb_tmap.mapping_v_max = (float)d_max_v;
}

void grd_helper::SetCb_RenderingEffect(CB_RenderingEffect& cb_reffect, VmVObject* obj, VmLObject* lobj, const RenderObjInfo& rendering_obj_info)
{
	const int obj_id = obj->GetObjectID();

	cb_reffect.outline_mode = rendering_obj_info.show_outline? 1 : 0; // DOJO TO DO , encoding

	bool apply_occ = false;
	lobj->GetDstObjValue(obj_id, "_bool_ApplyAO", &apply_occ);
	bool apply_brdf = false;
	lobj->GetDstObjValue(obj_id, "_bool_ApplyBRDF", &apply_brdf);
	cb_reffect.rf_flag = (int)apply_occ | ((int)apply_brdf << 1);

	if (apply_occ)
	{
		int occ_num_rays = 5;
		lobj->GetDstObjValue(obj_id, "_int_OccNumRays", &occ_num_rays);
		cb_reffect.occ_num_rays = (uint)occ_num_rays;
		double occ_radius = 0;
		lobj->GetDstObjValue(obj_id, "_double_OccRadius", &occ_radius);
		cb_reffect.occ_radius = (float)occ_radius;
	}
	if (apply_brdf)
	{
		double dPhongBRDF_DiffuseRatio = 0.5;
		lobj->GetDstObjValue(obj_id, ("_double_PhongBRDFDiffuseRatio"), &dPhongBRDF_DiffuseRatio);
		cb_reffect.brdf_diffuse_ratio = (float)dPhongBRDF_DiffuseRatio;
		double dPhongBRDF_ReflectanceRatio = 0.5;
		lobj->GetDstObjValue(obj_id, ("_double_PhongBRDFReflectanceRatio"), &dPhongBRDF_ReflectanceRatio);
		cb_reffect.brdf_reft_ratio = (float)dPhongBRDF_ReflectanceRatio;
		double dPhongExpWeightU = 50;
		lobj->GetDstObjValue(obj_id, ("_double_PhongExpWeightU"), &dPhongExpWeightU);
		cb_reffect.brdf_expw_u = (float)dPhongExpWeightU;
		double dPhongExpWeightV = 50;
		lobj->GetDstObjValue(obj_id, ("_double_PhongExpWeightV"), &dPhongExpWeightV);
		cb_reffect.brdf_expw_v = (float)dPhongExpWeightV;
	}

	// Curvature
	// to do
}

void grd_helper::SetCb_VolumeRenderingEffect(CB_VolumeRenderingEffect& cb_vreffect, VmVObjectVolume* vobj, VmLObject* lobj)
{
	const int vobj_id = vobj->GetObjectID();
	double dVoxelSharpnessForAttributeVolume = 0.25;
	lobj->GetDstObjValue(vobj_id, "_double_VoxelSharpnessForAttributeVolume", &dVoxelSharpnessForAttributeVolume);
	cb_vreffect.attribute_voxel_sharpness = (float)dVoxelSharpnessForAttributeVolume;

	double dClipPlaneIntensity = 1.0;
	lobj->GetDstObjValue(vobj_id, "_double_ClipPlaneIntensity", &dClipPlaneIntensity);
	cb_vreffect.clip_plane_intensity = (float)dClipPlaneIntensity;

	double dGradMagAmplifier = 1.0;
	lobj->GetDstObjValue(vobj_id, "_double_GradMagAmplifier", &dGradMagAmplifier);

	auto compute_grad_info = [](vmdouble2* d2GradientMagMinMax, VmVObjectVolume* vobj, LocalProgress* _progress)
	{
		if (vobj->GetCustomParameter("_double2_GraidentMagMinMax", data_type::dtype<vmdouble2>(), d2GradientMagMinMax))
			return;

		if (d2GradientMagMinMax->y - d2GradientMagMinMax->x > 0)
			return;

		const VolumeData* vol_data = vobj->GetVolumeData();

		//assert(vol_data->store_dtype.type_name == data_type::dtype<ushort>().type_name);
		if (vol_data->store_dtype.type_name != data_type::dtype<ushort>().type_name)
		{
			d2GradientMagMinMax->x = 0;
			d2GradientMagMinMax->y = 255;
			return;
		}

		if (_progress)
		{
			_progress->range = 70;
		}

		ushort** ppusVolume = (ushort**)vol_data->vol_slices;
		int iSizeAddrX = vol_data->vol_size.x + vol_data->bnd_size.x * 2;
		vmdouble2 d2GradMagMinMax;
		d2GradMagMinMax.x = DBL_MAX;
		d2GradMagMinMax.y = -DBL_MAX;
		for (int iZ = 1; iZ < vol_data->vol_size.z - 1; iZ += 2)
		{
			if (_progress)
			{
				*_progress->progress_ptr =
					(int)(_progress->start + _progress->range*iZ / vol_data->vol_size.z);
			}

			for (int iY = 1; iY < vol_data->vol_size.y - 1; iY += 2)
			{
				for (int iX = 1; iX < vol_data->vol_size.x - 1; iX += 2)
				{
					vmdouble3 d3Difference;
					int iAddrZ = iZ + vol_data->bnd_size.x;
					int iAddrY = iY + vol_data->bnd_size.y;
					int iAddrX = iX + vol_data->bnd_size.z;
					int iAddrZL = iZ - 1 + vol_data->bnd_size.z;
					int iAddrYL = iY - 1 + vol_data->bnd_size.y;
					int iAddrXL = iX - 1 + vol_data->bnd_size.x;
					int iAddrZR = iZ + 1 + vol_data->bnd_size.z;
					int iAddrYR = iY + 1 + vol_data->bnd_size.y;
					int iAddrXR = iX + 1 + vol_data->bnd_size.x;
					d3Difference.x = (double)((int)ppusVolume[iAddrZ][iAddrY*iSizeAddrX + iAddrXR] - (int)ppusVolume[iAddrZ][iAddrY*iSizeAddrX + iAddrXL]);
					d3Difference.y = (double)((int)ppusVolume[iAddrZ][iAddrYR*iSizeAddrX + iAddrX] - (int)ppusVolume[iAddrZ][iAddrYL*iSizeAddrX + iAddrX]);
					d3Difference.z = (double)((int)ppusVolume[iAddrZR][iAddrY*iSizeAddrX + iAddrX] - (int)ppusVolume[iAddrZL][iAddrY*iSizeAddrX + iAddrX]);
					double dGradientMag = sqrt(d3Difference.x*d3Difference.x + d3Difference.y*d3Difference.y + d3Difference.z*d3Difference.z);
					d2GradMagMinMax.x = min(d2GradMagMinMax.x, dGradientMag);
					d2GradMagMinMax.y = max(d2GradMagMinMax.y, dGradientMag);
				}
			}
		}

		vobj->RegisterCustomParameter("_double2_GraidentMagMinMax", d2GradMagMinMax);
		*d2GradientMagMinMax = d2GradMagMinMax;

		if (_progress)
		{
			_progress->start = _progress->start + _progress->range;
			*_progress->progress_ptr = (_progress->start);
			_progress->range = 30;
		}
	};

	vmdouble2 dGradientMagMinMax = vmdouble2(1, -1);
	compute_grad_info(&dGradientMagMinMax, vobj, NULL);

	cb_vreffect.mod_max_grad_size = (float)(dGradientMagMinMax.y / 65535.0);
	cb_vreffect.mod_grad_mag_scale = (float)dGradMagAmplifier;


	double occ_sample_dist_scale = 1.0;
	lobj->GetDstObjValue(vobj_id, "_double_OccSampleDistScale", &occ_sample_dist_scale);
	cb_vreffect.occ_sample_dist_scale = (float)occ_sample_dist_scale;

	double sdm_sample_dist_scale = 1.0;
	lobj->GetDstObjValue(vobj_id, "_double_SdmSampleDistScale", &sdm_sample_dist_scale);
	cb_vreffect.sdm_sample_dist_scale = (float)sdm_sample_dist_scale;
}

void grd_helper::SetCb_ClipInfo(CB_ClipInfo& cb_clip, VmVObject* obj, VmLObject* lobj)
{
	const int obj_id = obj->GetObjectID();
	bool is_clip_free = false;
	obj->GetCustomParameter("_bool_ClipFree", data_type::dtype<bool>(), &is_clip_free);

	if (!is_clip_free)
	{
		int clip_mode = 0; // CLIPBOX / CLIPPLANE / BOTH //
		lobj->GetDstObjValue(obj_id, "_int_ClippingMode", &clip_mode);	// 0 : No, 1 : CLIPPLANE, 2 : CLIPBOX, 3 : BOTH
		cb_clip.clip_flag = clip_mode & 0x3;
	}

	if (cb_clip.clip_flag & 0x1)
	{
		vmdouble3 d3PosOnPlane, d3VecPlane;
		if (lobj->GetDstObjValue(obj_id, "_double3_PosClipPlaneWS", &d3PosOnPlane)
			&& lobj->GetDstObjValue(obj_id, "_double3_VecClipPlaneWS", &d3VecPlane))
		{
			cb_clip.pos_clipplane = vmfloat3(d3PosOnPlane);
			cb_clip.vec_clipplane = vmfloat3(d3VecPlane);
		}
	}
	if (cb_clip.clip_flag & 0x2)
	{
		vmmat44 dmatClipWS2BS;
		vmdouble3 dPosOrthoMaxBox;
		if (lobj->GetDstObjValue(obj_id, "_matrix44_MatrixClipWS2BS", &dmatClipWS2BS)
			&& lobj->GetDstObjValue(obj_id, "_double3_PosClipBoxMaxWS", &dPosOrthoMaxBox))
		{
			TransformPoint(&dPosOrthoMaxBox, &dPosOrthoMaxBox, &dmatClipWS2BS);
			cb_clip.mat_clipbox_ws2bs = (vmmat44f)dmatClipWS2BS;
			cb_clip.pos_clipbox_max_bs = vmfloat3(dPosOrthoMaxBox);
		}
	}
}

void grd_helper::SetCb_HotspotMask(CB_HotspotMask& cb_hsmask, VmFnContainer* _fncontainer, vmint2 fb_size)
{
	vmint4 mask_center_rs_0 = _fncontainer->GetParamValue("_int4_MaskCenterRadius0", vmint4(fb_size / 2, fb_size.x / 5, 5));
	vmint4 mask_center_rs_1 = _fncontainer->GetParamValue("_int4_MaskCenterRadius1", vmint4(fb_size / 4, fb_size.x / 5, 3));
	vmdouble3 hotspot_params_0 = _fncontainer->GetParamValue("_double3_HotspotParamsTKtKs0", vmdouble3(1.));
	vmdouble3 hotspot_params_1 = _fncontainer->GetParamValue("_double3_HotspotParamsTKtKs1", vmdouble3(1.));
	bool show_silhuette_edge_0 = _fncontainer->GetParamValue("_bool_HotspotSilhuette0", false);
	bool show_silhuette_edge_1 = _fncontainer->GetParamValue("_bool_HotspotSilhuette1", false);
	float mask_bnd = (float)_fncontainer->GetParamValue("_double_MaskBndDisplay", (double)1.0);
	auto __set_params = [&cb_hsmask](int idx, vmint4 mask_center_rs, vmdouble3 hotspot_params, bool show_silhuette_edge, float mask_bnd)
	{
		//cbHSMaskData->pos_centerx_[idx] = mask_center_rs.x;
		//cbHSMaskData->pos_centery_[idx] = mask_center_rs.y;
		//cbHSMaskData->radius_[idx] = mask_center_rs.z;
		//cbHSMaskData->smoothness_[idx] = mask_center_rs.w | ((int)show_silhuette_edge << 16);
		//cbHSMaskData->thick_[idx] = (float)hotspot_params.x;
		//cbHSMaskData->kappa_s_[idx] = (float)hotspot_params.y;
		//cbHSMaskData->kappa_t_[idx] = (float)hotspot_params.z;
		//cbHSMaskData->bnd_thick_[idx] = mask_bnd;
		cb_hsmask.mask_info_[idx].pos_center = vmint2(mask_center_rs.x, mask_center_rs.y);
		cb_hsmask.mask_info_[idx].radius = mask_center_rs.z;
		cb_hsmask.mask_info_[idx].smoothness = mask_center_rs.w | ((int)show_silhuette_edge << 16);
		cb_hsmask.mask_info_[idx].thick = (float)hotspot_params.x;
		cb_hsmask.mask_info_[idx].kappa_t = (float)hotspot_params.y;
		cb_hsmask.mask_info_[idx].kappa_s = (float)hotspot_params.z;
		cb_hsmask.mask_info_[idx].bnd_thick = mask_bnd;
	};
	__set_params(0, mask_center_rs_0, hotspot_params_0, show_silhuette_edge_0, mask_bnd);
	__set_params(1, mask_center_rs_1, hotspot_params_1, show_silhuette_edge_1, mask_bnd);
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
		if (g_VmCommonParams.dx11Device->CreatePixelShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, (ID3D11PixelShader**)sm) != S_OK)
		{
			std::cout << "*** COMPILE ERROR : " << str << ", " << entry_point << ", " << shader_model << endl;
			return false;
		}
	}
	else if (shader_model.find("vs") != string::npos)
	{
		if (g_VmCommonParams.dx11Device->CreateVertexShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, (ID3D11VertexShader**)sm) != S_OK)
		{
			std::cout << "*** COMPILE ERROR : " << str << ", " << entry_point << ", " << shader_model << endl;
			return false;
		}
	}
	else if (shader_model.find("cs") != string::npos)
	{
		if (g_VmCommonParams.dx11Device->CreateComputeShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, (ID3D11ComputeShader**)sm) != S_OK)
		{
			std::cout << "*** COMPILE ERROR : " << str << ", " << entry_point << ", " << shader_model << endl;
			return false;
		}
	}
	else if (shader_model.find("gs") != string::npos)
	{
		if (g_VmCommonParams.dx11Device->CreateGeometryShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, (ID3D11GeometryShader**)sm) != S_OK)
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