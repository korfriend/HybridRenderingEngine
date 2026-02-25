#include "gpures_helper.h"
#include "meshpainter/PaintResourceManager.h"
#include "D3DCompiler.h"
#include "hlsl/ShaderInterop_BVH.h"
#include "BVHUpdate.h"
#include "vzm2/Backlog.h"
#include <iostream>
#include <fstream>

#if (defined(_DEBUG) || defined(DEBUG))
bool DEBUG_GPU_UPDATE_DATA = true;
#else
bool DEBUG_GPU_UPDATE_DATA = false;
#endif

using namespace grd_helper;

#define __BLKLEVEL 1

namespace grd_helper
{
	static PSOManager* g_psoManager = NULL;
	static VmGpuManager* g_pCGpuManager = NULL;

	static ID3D11Buffer* pushConstant = nullptr;
	static ID3D11ShaderResourceView* srvPushConstant = nullptr;
	static ID3D11Buffer* pushConstant_write = nullptr;

	static MeshPainter* meshPainter = nullptr;
}

#include <DirectXColors.h>
#include <DirectXCollision.h>

HRESULT grd_helper::PresetCompiledShader(__ID3D11Device* pdx11Device, HMODULE hModule, LPCWSTR pSrcResource, LPCSTR strShaderProfile, ID3D11DeviceChild** ppdx11Shader/*out*/
	, D3D11_INPUT_ELEMENT_DESC* pInputLayoutDesc, uint32_t num_elements, ID3D11InputLayout** ppdx11LayoutInputVS)
{
	HRSRC hrSrc = FindResource(hModule, pSrcResource, RT_RCDATA);
	HGLOBAL hGlobalByte = LoadResource(hModule, hrSrc);
	LPVOID pdata = LockResource(hGlobalByte);
	uint64_t ullFileSize = SizeofResource(hModule, hrSrc);

	string _strShaderProfile = strShaderProfile;
	if (_strShaderProfile.compare(0, 2, "cs") == 0)
	{
		HRESULT hr = pdx11Device->CreateComputeShader(pdata, ullFileSize, NULL, (ID3D11ComputeShader**)ppdx11Shader);
		if (hr != S_OK)
			goto ERROR_SHADER;
	}
	else if (_strShaderProfile.compare(0, 2, "ps") == 0)
	{
		if (pdx11Device->CreatePixelShader(pdata, ullFileSize, NULL, (ID3D11PixelShader**)ppdx11Shader) != S_OK)
			goto ERROR_SHADER;
	}
	else if (_strShaderProfile.compare(0, 2, "gs") == 0)
	{
		if (_strShaderProfile.find("SO") != string::npos) {
			// https://learn.microsoft.com/en-us/windows/win32/direct3d11/d3d10-graphics-programming-guide-output-stream-stage-getting-started
			// https://strange-cpp.tistory.com/101
			D3D11_SO_DECLARATION_ENTRY pDecl[] =
			{
				// semantic name, semantic index, start component, component count, output slot
				{ 0, "TEXCOORD", 0, 0, 3, 0 },   // output 
			};
			int numEntries = sizeof(pDecl) / sizeof(D3D11_SO_DECLARATION_ENTRY);
			uint32_t bufferStrides[] = { sizeof(vmfloat3) };
			int numStrides = sizeof(bufferStrides) / sizeof(uint32_t);
			if (pdx11Device->CreateGeometryShaderWithStreamOutput(
				pdata, ullFileSize, pDecl, numEntries, bufferStrides, numStrides, D3D11_SO_NO_RASTERIZED_STREAM, NULL,
				(ID3D11GeometryShader**)ppdx11Shader) != S_OK)
				goto ERROR_SHADER;
		}
		else {
			if (pdx11Device->CreateGeometryShader(pdata, ullFileSize, NULL, (ID3D11GeometryShader**)ppdx11Shader) != S_OK)
				goto ERROR_SHADER;
		}
	}
	else if (_strShaderProfile.compare(0, 2, "vs") == 0)
	{
		HRESULT hr = pdx11Device->CreateVertexShader(pdata, ullFileSize, NULL, (ID3D11VertexShader**)ppdx11Shader);
		if (hr != S_OK)
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

int grd_helper::Initialize(VmGpuManager* pCGpuManager, PSOManager* gpu_params)
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
	g_psoManager = gpu_params;

	if (g_psoManager->is_initialized)
	{
		return __ALREADY_CHECKED;
	}

#ifdef DX11_3
	g_pCGpuManager->GetDeviceInformation((void*)(&g_psoManager->dx11Device), "DEVICE_POINTER_3");
	g_pCGpuManager->GetDeviceInformation((void*)(&g_psoManager->dx11DeviceImmContext), "DEVICE_CONTEXT_POINTER_3");
#else
	g_pCGpuManager->GetDeviceInformation((void*)(&g_psoManager->dx11Device), "DEVICE_POINTER");
	g_pCGpuManager->GetDeviceInformation((void*)(&g_psoManager->dx11DeviceImmContext), "DEVICE_CONTEXT_POINTER");
#endif
	g_pCGpuManager->GetDeviceInformation(&g_psoManager->dx11_adapter, "DEVICE_ADAPTER_DESC");
	g_pCGpuManager->GetDeviceInformation(&g_psoManager->dx11_featureLevel, "FEATURE_LEVEL");

	//return __SUPPORTED_GPU;

	if (g_psoManager->dx11Device == NULL || g_psoManager->dx11DeviceImmContext == NULL)
	{
		vmlog::LogErr("error devices!");
		gpu_params = g_psoManager;
		return (int)__INVALID_GPU;
	}

	g_psoManager->Delete();

	//goto ERROR_PRESETTING;
	
	D3D11_QUERY_DESC qr_desc;
	qr_desc.MiscFlags = 0;
	qr_desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
	g_psoManager->dx11Device->CreateQuery(&qr_desc, &g_psoManager->dx11qr_disjoint);
	qr_desc.Query = D3D11_QUERY_TIMESTAMP;
	for (int i = 0; i < MAXSTAMPS; i++)
		g_psoManager->dx11Device->CreateQuery(&qr_desc, &g_psoManager->dx11qr_timestamps[i]);

	qr_desc.Query = D3D11_QUERY_EVENT;
	g_psoManager->dx11Device->CreateQuery(&qr_desc, &g_psoManager->dx11qr_fenceQuery);

	HRESULT hr = S_OK;

	{
		D3D11_BLEND_DESC descBlend = {};
		descBlend.AlphaToCoverageEnable = false;
		descBlend.IndependentBlendEnable = true;
		descBlend.RenderTarget[0].BlendEnable = true;
		descBlend.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		descBlend.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		descBlend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;

		descBlend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		descBlend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		descBlend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		descBlend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		ID3D11BlendState* blender_state;
		hr |= g_psoManager->dx11Device->CreateBlendState(&descBlend, &blender_state);
		g_psoManager->safe_set_res(COMRES_INDICATOR(GpuhelperResType::BLEND_STATE, "ADD"), blender_state);

		descBlend.AlphaToCoverageEnable = false;
		descBlend.IndependentBlendEnable = true;
		descBlend.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		descBlend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		descBlend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		descBlend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		descBlend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		descBlend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		for (int i = 1; i < 8; i++) {
			descBlend.RenderTarget[i].BlendEnable = false;
			descBlend.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_ONE;
			descBlend.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_ONE;
			descBlend.RenderTarget[i].BlendOp = D3D11_BLEND_OP_ADD;
			descBlend.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_ONE;
			descBlend.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_ONE;
			descBlend.RenderTarget[i].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			descBlend.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		}
		hr |= g_psoManager->dx11Device->CreateBlendState(&descBlend, &blender_state);
		g_psoManager->safe_set_res(COMRES_INDICATOR(GpuhelperResType::BLEND_STATE, "ALPHA_BLEND"), blender_state);


#ifdef DX11_3
		D3D11_RASTERIZER_DESC2 descRaster;
		ZeroMemory(&descRaster, sizeof(D3D11_RASTERIZER_DESC2));
		ID3D11RasterizerState2* raster_state;
#define MyCreateRasterizerState CreateRasterizerState2
#else
		D3D11_RASTERIZER_DESC descRaster;
		ZeroMemory(&descRaster, sizeof(D3D11_RASTERIZER_DESC));
		ID3D11RasterizerState* raster_state;
#define MyCreateRasterizerState CreateRasterizerState
#endif
		descRaster.FillMode = D3D11_FILL_SOLID;
		descRaster.CullMode = D3D11_CULL_BACK;
		descRaster.FrontCounterClockwise = TRUE;
		descRaster.DepthBias = 0;
		descRaster.DepthBiasClamp = 0;
		descRaster.SlopeScaledDepthBias = 0;
		descRaster.DepthClipEnable = TRUE;
		descRaster.ScissorEnable = FALSE;
		descRaster.MultisampleEnable = FALSE;
		descRaster.AntialiasedLineEnable = FALSE;
#ifdef DX11_3
		descRaster.ForcedSampleCount = 0;
		descRaster.ConservativeRaster = D3D11_CONSERVATIVE_RASTERIZATION_MODE_OFF;
#endif
		hr |= g_psoManager->dx11Device->MyCreateRasterizerState(&descRaster, &raster_state);
		g_psoManager->safe_set_res(COMRES_INDICATOR(GpuhelperResType::RASTERIZER_STATE, "SOLID_CULL_BACK"), raster_state);
		descRaster.AntialiasedLineEnable = true;
		hr |= g_psoManager->dx11Device->MyCreateRasterizerState(&descRaster, &raster_state);
		g_psoManager->safe_set_res(COMRES_INDICATOR(GpuhelperResType::RASTERIZER_STATE, "AA_SOLID_CULL_BACK"), raster_state);
		descRaster.CullMode = D3D11_CULL_FRONT;
		descRaster.AntialiasedLineEnable = false;
		hr |= g_psoManager->dx11Device->MyCreateRasterizerState(&descRaster, &raster_state);
		g_psoManager->safe_set_res(COMRES_INDICATOR(GpuhelperResType::RASTERIZER_STATE, "SOLID_CULL_FRONT"), raster_state);
		descRaster.AntialiasedLineEnable = true;
		hr |= g_psoManager->dx11Device->MyCreateRasterizerState(&descRaster, &raster_state);
		g_psoManager->safe_set_res(COMRES_INDICATOR(GpuhelperResType::RASTERIZER_STATE, "AA_SOLID_CULL_FRONT"), raster_state);
		descRaster.CullMode = D3D11_CULL_NONE;
		descRaster.AntialiasedLineEnable = false;
		hr |= g_psoManager->dx11Device->MyCreateRasterizerState(&descRaster, &raster_state);
		g_psoManager->safe_set_res(COMRES_INDICATOR(GpuhelperResType::RASTERIZER_STATE, "SOLID_NONE"), raster_state);
		descRaster.AntialiasedLineEnable = true;
		hr |= g_psoManager->dx11Device->MyCreateRasterizerState(&descRaster, &raster_state);
		g_psoManager->safe_set_res(COMRES_INDICATOR(GpuhelperResType::RASTERIZER_STATE, "AA_SOLID_NONE"), raster_state);

		descRaster.FillMode = D3D11_FILL_WIREFRAME;
		descRaster.CullMode = D3D11_CULL_BACK;
#ifdef DX11_3
		descRaster.ConservativeRaster = D3D11_CONSERVATIVE_RASTERIZATION_MODE_OFF;
#endif
		descRaster.CullMode = D3D11_CULL_BACK;
		descRaster.AntialiasedLineEnable = false;
		hr |= g_psoManager->dx11Device->MyCreateRasterizerState(&descRaster, &raster_state);
		g_psoManager->safe_set_res(COMRES_INDICATOR(GpuhelperResType::RASTERIZER_STATE, "WIRE_CW"), raster_state);
		descRaster.AntialiasedLineEnable = true;
		hr |= g_psoManager->dx11Device->MyCreateRasterizerState(&descRaster, &raster_state);
		g_psoManager->safe_set_res(COMRES_INDICATOR(GpuhelperResType::RASTERIZER_STATE, "AA_WIRE_CW"), raster_state);
		descRaster.CullMode = D3D11_CULL_FRONT;
		descRaster.AntialiasedLineEnable = false;
		hr |= g_psoManager->dx11Device->MyCreateRasterizerState(&descRaster, &raster_state);
		g_psoManager->safe_set_res(COMRES_INDICATOR(GpuhelperResType::RASTERIZER_STATE, "WIRE_CCW"), raster_state);
		descRaster.AntialiasedLineEnable = true;
		hr |= g_psoManager->dx11Device->MyCreateRasterizerState(&descRaster, &raster_state);
		g_psoManager->safe_set_res(COMRES_INDICATOR(GpuhelperResType::RASTERIZER_STATE, "AA_WIRE_CCW"), raster_state);
		descRaster.CullMode = D3D11_CULL_NONE;
		descRaster.AntialiasedLineEnable = false;
		hr |= g_psoManager->dx11Device->MyCreateRasterizerState(&descRaster, &raster_state);
		g_psoManager->safe_set_res(COMRES_INDICATOR(GpuhelperResType::RASTERIZER_STATE, "WIRE_NONE"), raster_state);
		descRaster.AntialiasedLineEnable = true;
		hr |= g_psoManager->dx11Device->MyCreateRasterizerState(&descRaster, &raster_state);
		g_psoManager->safe_set_res(COMRES_INDICATOR(GpuhelperResType::RASTERIZER_STATE, "AA_WIRE_NONE"), raster_state);
	}
	{
		D3D11_DEPTH_STENCIL_DESC descDepthStencil;
		ZeroMemory(&descDepthStencil, sizeof(D3D11_DEPTH_STENCIL_DESC));
		descDepthStencil.DepthEnable = TRUE;
		descDepthStencil.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		descDepthStencil.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL; // Reverse Z: was LESS_EQUAL
		descDepthStencil.StencilEnable = FALSE;
		ID3D11DepthStencilState* ds_state;
		hr |= g_psoManager->dx11Device->CreateDepthStencilState(&descDepthStencil, &ds_state);
		g_psoManager->safe_set_res(COMRES_INDICATOR(GpuhelperResType::DEPTHSTENCIL_STATE, "LESSEQUAL"), ds_state);
		descDepthStencil.DepthFunc = D3D11_COMPARISON_EQUAL;
		hr |= g_psoManager->dx11Device->CreateDepthStencilState(&descDepthStencil, &ds_state);
		g_psoManager->safe_set_res(COMRES_INDICATOR(GpuhelperResType::DEPTHSTENCIL_STATE, "EQUAL"), ds_state);
		descDepthStencil.DepthFunc = D3D11_COMPARISON_ALWAYS;
		hr |= g_psoManager->dx11Device->CreateDepthStencilState(&descDepthStencil, &ds_state);
		g_psoManager->safe_set_res(COMRES_INDICATOR(GpuhelperResType::DEPTHSTENCIL_STATE, "ALWAYS"), ds_state);
		descDepthStencil.DepthFunc = D3D11_COMPARISON_LESS; // Reverse Z: was GREATER
		hr |= g_psoManager->dx11Device->CreateDepthStencilState(&descDepthStencil, &ds_state);
		g_psoManager->safe_set_res(COMRES_INDICATOR(GpuhelperResType::DEPTHSTENCIL_STATE, "GREATER"), ds_state);
		descDepthStencil.DepthFunc = D3D11_COMPARISON_GREATER; // Reverse Z: was LESS
		hr |= g_psoManager->dx11Device->CreateDepthStencilState(&descDepthStencil, &ds_state);
		g_psoManager->safe_set_res(COMRES_INDICATOR(GpuhelperResType::DEPTHSTENCIL_STATE, "LESS"), ds_state);

		descDepthStencil.DepthEnable = FALSE;
		descDepthStencil.StencilEnable = FALSE;
		descDepthStencil.DepthFunc = D3D11_COMPARISON_ALWAYS;
		hr |= g_psoManager->dx11Device->CreateDepthStencilState(&descDepthStencil, &ds_state);
		g_psoManager->safe_set_res(COMRES_INDICATOR(GpuhelperResType::DEPTHSTENCIL_STATE, "DISABLED"), ds_state);
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
		hr |= g_psoManager->dx11Device->CreateSamplerState(&descSampler, &sampler_state);
		g_psoManager->safe_set_res(COMRES_INDICATOR(GpuhelperResType::SAMPLER_STATE, "POINT_CLAMP"), sampler_state);
		descSampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;	// NEAREST by ROUND
		hr |= g_psoManager->dx11Device->CreateSamplerState(&descSampler, &sampler_state);
		g_psoManager->safe_set_res(COMRES_INDICATOR(GpuhelperResType::SAMPLER_STATE, "LINEAR_CLAMP"), sampler_state);

		descSampler.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
		descSampler.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
		descSampler.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
		descSampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;	// NEAREST by ROUND
		hr |= g_psoManager->dx11Device->CreateSamplerState(&descSampler, &sampler_state);
		g_psoManager->safe_set_res(COMRES_INDICATOR(GpuhelperResType::SAMPLER_STATE, "LINEAR_ZEROBORDER"), sampler_state);
		descSampler.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;	// NEAREST by ROUND
		hr |= g_psoManager->dx11Device->CreateSamplerState(&descSampler, &sampler_state);
		g_psoManager->safe_set_res(COMRES_INDICATOR(GpuhelperResType::SAMPLER_STATE, "POINT_ZEROBORDER"), sampler_state);

		descSampler.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		descSampler.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		descSampler.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		descSampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;	// NEAREST by ROUND
		hr |= g_psoManager->dx11Device->CreateSamplerState(&descSampler, &sampler_state);
		g_psoManager->safe_set_res(COMRES_INDICATOR(GpuhelperResType::SAMPLER_STATE, "LINEAR_WRAP"), sampler_state);
		descSampler.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;	// NEAREST by ROUND
		hr |= g_psoManager->dx11Device->CreateSamplerState(&descSampler, &sampler_state);
		g_psoManager->safe_set_res(COMRES_INDICATOR(GpuhelperResType::SAMPLER_STATE, "POINT_WRAP"), sampler_state);
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
		hr |= g_psoManager->dx11Device->CreateBuffer(&descCB, NULL, &cbuffer);\
		g_psoManager->safe_set_cbuf(string(#STRUCT), cbuffer);}\
//CREATE_AND_SET

		CREATE_AND_SET(CB_CameraState);
		CREATE_AND_SET(CB_EnvState);
		CREATE_AND_SET(CB_ClipInfo);
		CREATE_AND_SET(CB_PolygonObject);
		CREATE_AND_SET(CB_VolumeObject);
		CREATE_AND_SET(CB_Material);
		CREATE_AND_SET(CB_VolumeMaterial);
		CREATE_AND_SET(CB_TMAP);
		CREATE_AND_SET(MomentOIT);
		CREATE_AND_SET(CB_HotspotMask);
		CREATE_AND_SET(CB_CurvedSlicer);
		CREATE_AND_SET(CB_TestBuffer);
		CREATE_AND_SET(CB_Particle_Blob);
		CREATE_AND_SET(CB_Frame);
		CREATE_AND_SET(CB_Emitter);
		CREATE_AND_SET(CB_Undercut);
		CREATE_AND_SET(CB_SortConstants);
		//CREATE_AND_SET(BVHPushConstants);
	}

	{
		D3D11_BUFFER_DESC bd;
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		bd.CPUAccessFlags = 0u;
		bd.ByteWidth = 32u;
		bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bd.StructureByteStride = 32u;
		g_psoManager->dx11Device->CreateBuffer(&bd, nullptr, &pushConstant);

		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
		srv_desc.BufferEx.NumElements = 1;
		srv_desc.Format = DXGI_FORMAT::DXGI_FORMAT_UNKNOWN;
		g_psoManager->dx11Device->CreateShaderResourceView(pushConstant, &srv_desc, &srvPushConstant);

		bd.Usage = D3D11_USAGE_STAGING;
		bd.BindFlags = 0u;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		g_psoManager->dx11Device->CreateBuffer(&bd, nullptr, &pushConstant_write);
	}

	HMODULE hModule = GetModuleHandleA(__DLLNAME);

	if (hr != S_OK)
	{
		vmlog::LogErr("error : basic dx11 resources!");
		goto ERROR_PRESETTING;
	}

	{
		D3D11_INPUT_ELEMENT_DESC lotypeInputP[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		D3D11_INPUT_ELEMENT_DESC lotypeInputPN[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		D3D11_INPUT_ELEMENT_DESC lotypeInputPC[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 3, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		D3D11_INPUT_ELEMENT_DESC lotypeInputPT[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R16G16_UNORM, 2, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		D3D11_INPUT_ELEMENT_DESC lotypeInputPNT[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R16G16_UNORM, 2, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		D3D11_INPUT_ELEMENT_DESC lotypeInputPNC[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 3, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		D3D11_INPUT_ELEMENT_DESC lotypeInputPTC[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R16G16_UNORM, 2, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 3, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		D3D11_INPUT_ELEMENT_DESC lotypeInputPNTC[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R16G16_UNORM, 2, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 3, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		D3D11_INPUT_ELEMENT_DESC lotypeInputPTTT_Annotation[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R16G16_UNORM, 2, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 1, DXGI_FORMAT_R32G32B32_FLOAT, 4, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 2, DXGI_FORMAT_R32G32B32_FLOAT, 5, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		// geodesics
		D3D11_INPUT_ELEMENT_DESC lotypeInputPG[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 1, DXGI_FORMAT_R32_FLOAT, 6, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		D3D11_INPUT_ELEMENT_DESC lotypeInputPNG[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 1, DXGI_FORMAT_R32_FLOAT, 6, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		D3D11_INPUT_ELEMENT_DESC lotypeInputPCG[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 3, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 1, DXGI_FORMAT_R32_FLOAT, 6, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		D3D11_INPUT_ELEMENT_DESC lotypeInputPTG[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R16G16_UNORM, 2, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 1, DXGI_FORMAT_R32_FLOAT, 6, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		D3D11_INPUT_ELEMENT_DESC lotypeInputPNTG[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R16G16_UNORM, 2, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 1, DXGI_FORMAT_R32_FLOAT, 6, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		D3D11_INPUT_ELEMENT_DESC lotypeInputPNCG[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 3, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 1, DXGI_FORMAT_R32_FLOAT, 6, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		D3D11_INPUT_ELEMENT_DESC lotypeInputPTCG[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R16G16_UNORM, 2, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 3, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 1, DXGI_FORMAT_R32_FLOAT, 6, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		D3D11_INPUT_ELEMENT_DESC lotypeInputPNTCG[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R16G16_UNORM, 2, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 3, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 1, DXGI_FORMAT_R32_FLOAT, 6, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		auto register_vertex_shader = [&](const LPCWSTR pSrcResource, const string& name_shader, const string& profile, const string& name_layer, D3D11_INPUT_ELEMENT_DESC in_layout_desc[], uint32_t num_elements)
		{
			ID3D11InputLayout* in_layout = NULL;
			ID3D11VertexShader* vshader = NULL;

			if (PresetCompiledShader(g_psoManager->dx11Device, hModule, pSrcResource, profile.c_str(), (ID3D11DeviceChild**)&vshader
				, in_layout_desc, num_elements, &in_layout) != S_OK)
			{
				VMSAFE_RELEASE(in_layout);
				VMSAFE_RELEASE(vshader);
				return E_FAIL;
			}
			if(in_layout && name_layer != "")
				g_psoManager->safe_set_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, name_layer), in_layout);
			g_psoManager->safe_set_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, name_shader), vshader);
			return S_OK;
		};
		auto register_shader = [&](const LPCWSTR pSrcResource, const string& name_shader, const string& profile)
		{
			ID3D11DeviceChild* shader = NULL;
			if (PresetCompiledShader(g_psoManager->dx11Device, hModule, pSrcResource, profile.c_str(), (ID3D11DeviceChild**)&shader, NULL, 0, NULL) != S_OK)
			{
				VMSAFE_RELEASE(shader);
				return E_FAIL;
			}
			GpuhelperResType _type = GpuhelperResType::VERTEX_SHADER;
			if (profile.compare(0, 2, "vs") == 0) _type = GpuhelperResType::VERTEX_SHADER;
			else if (profile.compare(0, 2, "ps") == 0) _type = GpuhelperResType::PIXEL_SHADER;
			else if (profile.compare(0, 2, "gs") == 0) _type = GpuhelperResType::GEOMETRY_SHADER;
			else if (profile.compare(0, 2, "cs") == 0) _type = GpuhelperResType::COMPUTE_SHADER;
			else VMERRORMESSAGE("UNDEFINED SHADER TYPE ! : grd_helper::InitializePresettings");

			g_psoManager->safe_set_res(COMRES_INDICATOR(_type, name_shader), shader);
			return S_OK;
		};

		GpuRes gres_quad;
		{
			gres_quad.vm_src_id = 1;
			gres_quad.res_name = string("PROXY_QUAD");
			gres_quad.rtype = RTYPE_BUFFER;
			gres_quad.options["USAGE"] = D3D11_USAGE_DEFAULT;
			gres_quad.options["CPU_ACCESS_FLAG"] = NULL; // D3D11_CPU_ACCESS_WRITE;// | D3D11_CPU_ACCESS_READ;
			gres_quad.options["BIND_FLAG"] = D3D11_BIND_VERTEX_BUFFER;
			gres_quad.options["FORMAT"] = DXGI_FORMAT_R32G32B32_FLOAT;
			gres_quad.res_values.SetParam("NUM_ELEMENTS", (uint32_t)4);
			gres_quad.res_values.SetParam("STRIDE_BYTES", (uint32_t)sizeof(vmfloat3));
			g_pCGpuManager->GenerateGpuResource(gres_quad);

			ID3D11Buffer* pdx11bufvtx = (ID3D11Buffer*)gres_quad.alloc_res_ptrs[DTYPE_RES];
			D3D11_SUBRESOURCE_DATA subres;
			subres.pSysMem = new vmfloat3[4];
			subres.SysMemPitch = sizeof(vmfloat3) * 4;
			subres.SysMemSlicePitch = 0; // only for 3D resource
			vmfloat3* vtx_group = (vmfloat3*)subres.pSysMem;
			vtx_group[0] = vmfloat3(-1, 1, 0);
			vtx_group[1] = vmfloat3( 1, 1, 0);
			vtx_group[2] = vmfloat3(-1,-1, 0);
			vtx_group[3] = vmfloat3( 1,-1, 0);
			g_psoManager->dx11DeviceImmContext->UpdateSubresource(pdx11bufvtx, 0, NULL, subres.pSysMem, subres.SysMemPitch, 0);
			VMSAFE_DELETEARRAY_VOID(subres.pSysMem);

			if (DEBUG_GPU_UPDATE_DATA) vzlog("DEBUG_GPU_UPDATE_DATA: %s", gres_quad.res_name.c_str());
		}

		// mesh painter
		if (meshPainter == nullptr)
		{
			meshPainter = new MeshPainter(g_psoManager->dx11Device, g_psoManager->dx11DeviceImmContext);
		}


#define VRETURN(v, ERR) if(v != S_OK) { vmlog::LogErr(#ERR); goto ERROR_PRESETTING; }

		// common...
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA31030), "GS_PickingBasic_gs_4_0", "gs_4_0_SO"), GS_PickingBasic_gs_4_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA31031), "GS_MeshCutLines_gs_4_0", "gs_4_0_SO"), GS_MeshCutLines_gs_4_0);
#ifdef DX10_0
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA91000), "SR_QUAD_P_vs_4_0", "vs_4_0", "P", NULL, 0), SR_QUAD_P_vs_4_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA91001), "SR_OIT_P_vs_4_0", "vs_4_0", "P", lotypeInputP, 1), SR_OIT_P_vs_4_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA91002), "SR_OIT_PC_vs_4_0", "vs_4_0", "PC", lotypeInputPC, 2), SR_OIT_PC_vs_4_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA91003), "SR_OIT_PT_vs_4_0", "vs_4_0", "PT", lotypeInputPT, 2), SR_OIT_PT_vs_4_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA91004), "SR_OIT_PN_vs_4_0", "vs_4_0", "PN", lotypeInputPN, 2), SR_OIT_PN_vs_4_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA91005), "SR_OIT_PNC_vs_4_0", "vs_4_0", "PNC", lotypeInputPNC, 3), SR_OIT_PNC_vs_4_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA91006), "SR_OIT_PNT_vs_4_0", "vs_4_0", "PNT", lotypeInputPNT, 3), SR_OIT_PNT_vs_4_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA91007), "SR_OIT_PTC_vs_4_0", "vs_4_0", "PTC", lotypeInputPTC, 3), SR_OIT_PTC_vs_4_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA91008), "SR_OIT_PNTC_vs_4_0", "vs_4_0", "PNTC", lotypeInputPNTC, 4), SR_OIT_PNTC_vs_4_0);

		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA91021), "SR_OIT_PG_vs_4_0", "vs_4_0", "PG", lotypeInputPG, 1), SR_OIT_PG_vs_4_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA91022), "SR_OIT_PCG_vs_4_0", "vs_4_0", "PCG", lotypeInputPCG, 2), SR_OIT_PCG_vs_4_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA91023), "SR_OIT_PTG_vs_4_0", "vs_4_0", "PTG", lotypeInputPTG, 2), SR_OIT_PTG_vs_4_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA91024), "SR_OIT_PNG_vs_4_0", "vs_4_0", "PNG", lotypeInputPNG, 2), SR_OIT_PNG_vs_4_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA91025), "SR_OIT_PNCG_vs_4_0", "vs_4_0", "PNCG", lotypeInputPNCG, 3), SR_OIT_PNCG_vs_4_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA91026), "SR_OIT_PNTG_vs_4_0", "vs_4_0", "PNTG", lotypeInputPNTG, 3), SR_OIT_PNTG_vs_4_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA91027), "SR_OIT_PTCG_vs_4_0", "vs_4_0", "PTCG", lotypeInputPTCG, 3), SR_OIT_PTCG_vs_4_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA91028), "SR_OIT_PNTCG_vs_4_0", "vs_4_0", "PNTCG", lotypeInputPNTCG, 4), SR_OIT_PNTCG_vs_4_0);

		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA91009), "SR_OIT_PTTT_vs_4_0", "vs_4_0", "PTTT", lotypeInputPTTT_Annotation, 4), SR_OIT_PTTT_vs_4_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA91015), "SR_SINGLE_LAYER_ps_4_0", "ps_4_0"), SR_SINGLE_LAYER_ps_4_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA91016), "WRITE_DEPTH_ps_4_0", "ps_4_0"), WRITE_DEPTH_ps_4_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA91010), "GS_ThickPoints_gs_4_0", "gs_4_0"), GS_ThickPoints_gs_4_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA91011), "GS_SurfelPoints_gs_4_0", "gs_4_0"), GS_SurfelPoints_gs_4_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA91020), "GS_ThickLines_gs_4_0", "gs_4_0"), GS_ThickLines_gs_4_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA31025), "GS_TriNormal_gs_4_0", "gs_4_0"), GS_TriNormal_gs_4_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA90101), "SR_BASIC_PHONGBLINN_ps_4_0", "ps_4_0"), SR_BASIC_PHONGBLINN_ps_4_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA90102), "SR_BASIC_DASHEDLINE_ps_4_0", "ps_4_0"), SR_BASIC_DASHEDLINE_ps_4_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA90103), "SR_BASIC_MULTITEXTMAPPING_ps_4_0", "ps_4_0"), SR_BASIC_MULTITEXTMAPPING_ps_4_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA90104), "SR_BASIC_TEXTMAPPING_ps_4_0", "ps_4_0"), SR_BASIC_TEXTMAPPING_ps_4_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA90105), "SR_BASIC_TEXTUREIMGMAP_ps_4_0", "ps_4_0"), SR_BASIC_TEXTUREIMGMAP_ps_4_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA90106), "SR_BASIC_VOLUMEMAP_ps_4_0", "ps_4_0"), SR_BASIC_VOLUMEMAP_ps_4_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA90107), "SR_BASIC_VOLUME_DIST_MAP_ps_4_0", "ps_4_0"), SR_BASIC_VOLUME_DIST_MAP_ps_4_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA9000), "VR_RAYMAX_ps_4_0", "ps_4_0"), VR_RAYMAX_ps_4_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA9001), "VR_RAYMIN_ps_4_0", "ps_4_0"), VR_RAYMIN_ps_4_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA9002), "VR_RAYSUM_ps_4_0", "ps_4_0"), VR_RAYSUM_ps_4_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA90003), "VR_DEFAULT_ps_4_0", "ps_4_0"), VR_DEFAULT_ps_4_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA90004), "VR_OPAQUE_ps_4_0", "ps_4_0"), VR_OPAQUE_ps_4_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA90005), "VR_CONTEXT_ps_4_0", "ps_4_0"), VR_CONTEXT_ps_4_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA90011), "VR_MULTIOTF_ps_4_0", "ps_4_0"), VR_MULTIOTF_ps_4_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA90041), "VR_MULTIOTF_CONTEXT_ps_4_0", "ps_4_0"), VR_MULTIOTF_CONTEXT_ps_4_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA90012), "VR_MASKVIS_ps_4_0", "ps_4_0"), VR_MASKVIS_ps_4_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA90042), "VR_SCULPTMASK_ps_4_0", "ps_4_0"), VR_SCULPTMASK_ps_4_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA90043), "VR_SCULPTMASK_CONTEXT_ps_4_0", "ps_4_0"), VR_SCULPTMASK_CONTEXT_ps_4_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA90044), "VR_DEFAULT_SCULPTBITS_ps_4_0", "ps_4_0"), VR_DEFAULT_SCULPTBITS_ps_4_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA90045), "VR_CONTEXT_SCULPTBITS_ps_4_0", "ps_4_0"), VR_CONTEXT_SCULPTBITS_ps_4_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA90006), "VR_SURFACE_ps_4_0", "ps_4_0"), VR_SURFACE_ps_4_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA90150), "SR_QUAD_OUTLINE_ps_4_0", "ps_4_0"), SR_QUAD_OUTLINE_ps_4_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA9100), "ThickSlicePathTracer_ps_4_0", "ps_4_0"), ThickSlicePathTracer_ps_4_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA9101), "CurvedThickSlicePathTracer_ps_4_0", "ps_4_0"), CurvedThickSlicePathTracer_ps_4_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA9102), "SliceOutline_ps_4_0", "ps_4_0"), SliceOutline_ps_4_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA9110), "PickingThickSlice_ps_4_0", "ps_4_0"), PickingThickSlice_ps_4_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA9111), "PickingCurvedThickSlice_ps_4_0", "ps_4_0"), PickingCurvedThickSlice_ps_4_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA6001), "PanoVR_RAYMAX_ps_4_0", "ps_4_0"), PanoVR_RAYMAX_ps_4_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA6002), "PanoVR_RAYMIN_ps_4_0", "ps_4_0"), PanoVR_RAYMIN_ps_4_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA6003), "PanoVR_RAYSUM_ps_4_0", "ps_4_0"), PanoVR_RAYSUM_ps_4_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA6004), "PanoVR_DEFAULT_ps_4_0", "ps_4_0"), PanoVR_DEFAULT_ps_4_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA6005), "PanoVR_MODULATE_ps_4_0", "ps_4_0"), PanoVR_MODULATE_ps_4_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA6006), "PanoVR_MULTIOTF_DEFAULT_ps_4_0", "ps_4_0"), PanoVR_MULTIOTF_DEFAULT_ps_4_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA6007), "PanoVR_MULTIOTF_MODULATE_ps_4_0", "ps_4_0"), PanoVR_MULTIOTF_MODULATE_ps_4_0);
#else
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA11000), "SR_QUAD_P_vs_5_0", "vs_5_0", "P", NULL, 0), SR_QUAD_P_vs_5_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA11001), "SR_OIT_P_vs_5_0", "vs_5_0", "P", lotypeInputP, 1), SR_OIT_P_vs_5_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA11002), "SR_OIT_PC_vs_5_0", "vs_5_0", "PC", lotypeInputPC, 2), SR_OIT_PC_vs_5_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA11003), "SR_OIT_PT_vs_5_0", "vs_5_0", "PT", lotypeInputPT, 2), SR_OIT_PT_vs_5_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA11004), "SR_OIT_PN_vs_5_0", "vs_5_0", "PN", lotypeInputPN, 2), SR_OIT_PN_vs_5_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA11005), "SR_OIT_PNC_vs_5_0", "vs_5_0", "PNC", lotypeInputPNC, 3), SR_OIT_PNC_vs_5_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA11006), "SR_OIT_PNT_vs_5_0", "vs_5_0", "PNT", lotypeInputPNT, 3), SR_OIT_PNT_vs_5_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA11007), "SR_OIT_PTC_vs_5_0", "vs_5_0", "PTC", lotypeInputPTC, 3), SR_OIT_PTC_vs_5_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA11008), "SR_OIT_PNTC_vs_5_0", "vs_5_0", "PNTC", lotypeInputPNTC, 4), SR_OIT_PNTC_vs_5_0);

		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA11041), "SR_OIT_PG_vs_5_0", "vs_5_0", "PG", lotypeInputPG, 2), SR_OIT_PG_vs_5_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA11042), "SR_OIT_PCG_vs_5_0", "vs_5_0", "PCG", lotypeInputPCG, 3), SR_OIT_PCG_vs_5_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA11043), "SR_OIT_PTG_vs_5_0", "vs_5_0", "PTG", lotypeInputPTG, 3), SR_OIT_PTG_vs_5_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA11044), "SR_OIT_PNG_vs_5_0", "vs_5_0", "PNG", lotypeInputPNG, 3), SR_OIT_PNG_vs_5_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA11045), "SR_OIT_PNCG_vs_5_0", "vs_5_0", "PNCG", lotypeInputPNCG, 4), SR_OIT_PNCG_vs_5_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA11046), "SR_OIT_PNTG_vs_5_0", "vs_5_0", "PNTG", lotypeInputPNTG, 4), SR_OIT_PNTG_vs_5_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA11047), "SR_OIT_PTCG_vs_5_0", "vs_5_0", "PTCG", lotypeInputPTCG, 4), SR_OIT_PTCG_vs_5_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA11048), "SR_OIT_PNTCG_vs_5_0", "vs_5_0", "PNTCG", lotypeInputPNTCG, 5), SR_OIT_PNTCG_vs_5_0);
		
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA11009), "SR_OIT_PTTT_vs_5_0", "vs_5_0", "PTTT", lotypeInputPTTT_Annotation, 4), SR_OIT_PTTT_vs_5_0);
		VRETURN(register_vertex_shader(MAKEINTRESOURCE(IDR_RCDATA11010), "SR_OIT_IDX_vs_5_0", "vs_5_0", "INDEX", NULL, 0), SR_OIT_IDX_vs_5_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA10000), "SR_CAST_DEPTHMAP_ps_5_0", "ps_5_0"), SR_CAST_DEPTHMAP_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA10150), "SR_QUAD_OUTLINE_ps_5_0", "ps_5_0"), SR_QUAD_OUTLINE_ps_5_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA10101), "SR_BASIC_PHONGBLINN_ps_5_0", "ps_5_0"), SR_BASIC_PHONGBLINN_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA10102), "SR_BASIC_DASHEDLINE_ps_5_0", "ps_5_0"), SR_BASIC_DASHEDLINE_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA10103), "SR_BASIC_MULTITEXTMAPPING_ps_5_0", "ps_5_0"), SR_BASIC_MULTITEXTMAPPING_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA10104), "SR_BASIC_TEXTMAPPING_ps_5_0", "ps_5_0"), SR_BASIC_TEXTMAPPING_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA10105), "SR_BASIC_TEXTUREIMGMAP_ps_5_0", "ps_5_0"), SR_BASIC_TEXTUREIMGMAP_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA10106), "SR_BASIC_VOLUMEMAP_ps_5_0", "ps_5_0"), SR_BASIC_VOLUMEMAP_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA10107), "SR_BASIC_VOLUME_DIST_MAP_ps_5_0", "ps_5_0"), SR_BASIC_VOLUME_DIST_MAP_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA10108), "SR_UNDERCUT_ps_5_0", "ps_5_0"), SR_UNDERCUT_ps_5_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA10111), "SR_BASIC_PHONGBLINN_PAINTER_ps_5_0", "ps_5_0"), SR_BASIC_PHONGBLINN_PAINTER_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA10211), "SR_OIT_ABUFFER_PHONGBLINN_PAINTER_ps_5_0", "ps_5_0"), SR_OIT_ABUFFER_PHONGBLINN_PAINTER_ps_5_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA10201), "PCE_ParticleRenderBasic_ps_5_0", "ps_5_0"), PCE_ParticleRenderBasic_ps_5_0);

		{
			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA10800), "SORT_Kickoff_cs_5_0", "cs_5_0"), SORT_Kickoff_cs_5_0);
			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA10801), "SORT_cs_5_0", "cs_5_0"), SORT_cs_5_0);
			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA10802), "SORT_Step_cs_5_0", "cs_5_0"), SORT_Step_cs_5_0);
			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA10803), "SORT_Inner_cs_5_0", "cs_5_0"), SORT_Inner_cs_5_0);
		}

		{
			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA10900), "BVH_Hierarchy_cs_5_0", "cs_5_0"), BVH_Hierarchy_cs_5_0);
			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA10901), "BVH_Primitives_cs_5_0", "cs_5_0"), BVH_Primitives_cs_5_0);
			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA10902), "BVH_Propagateaabb_cs_5_0", "cs_5_0"), BVH_Propagateaabb_cs_5_0);
		}

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11015), "SR_SINGLE_LAYER_ps_5_0", "ps_5_0"), SR_SINGLE_LAYER_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11016), "WRITE_DEPTH_ps_5_0", "ps_5_0"), WRITE_DEPTH_ps_5_0);
		//VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11016), "SR_OIT_KDEPTH_NPRGHOST_ps_5_0", "ps_5_0"));

		if (g_psoManager->dx11_featureLevel > (D3D_FEATURE_LEVEL)0xb100) {
			// for dynamic k-buffer
			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA21050), "SR_FillHistogram_cs_5_0", "cs_5_0"), SR_FillHistogram_cs_5_0); 
			VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA21051), "SR_CreateOffsetTableKpB_cs_5_0", "cs_5_0"), SR_CreateOffsetTableKpB_cs_5_0);
		}


		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11020), "SR_OIT_ABUFFER_FRAGCOUNTER_ps_5_0", "ps_5_0"), SR_OIT_ABUFFER_FRAGCOUNTER_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11026), "SR_OIT_ABUFFER_FRAGCOUNTER_MTT_ps_5_0", "ps_5_0"), SR_OIT_ABUFFER_FRAGCOUNTER_MTT_ps_5_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11021), "SR_OIT_ABUFFER_PHONGBLINN_ps_5_0", "ps_5_0"), SR_OIT_ABUFFER_PHONGBLINN_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11022), "SR_OIT_ABUFFER_DASHEDLINE_ps_5_0", "ps_5_0"), SR_OIT_ABUFFER_DASHEDLINE_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11023), "SR_OIT_ABUFFER_MULTITEXTMAPPING_ps_5_0", "ps_5_0"), SR_OIT_ABUFFER_MULTITEXTMAPPING_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11024), "SR_OIT_ABUFFER_TEXTMAPPING_ps_5_0", "ps_5_0"), SR_OIT_ABUFFER_TEXTMAPPING_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11025), "SR_OIT_ABUFFER_TEXTUREIMGMAP_ps_5_0", "ps_5_0"), SR_OIT_ABUFFER_TEXTUREIMGMAP_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11028), "SR_OIT_ABUFFER_VOLUMEMAP_ps_5_0", "ps_5_0"), SR_OIT_ABUFFER_VOLUMEMAP_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11029), "SR_OIT_ABUFFER_VOLUME_DIST_MAP_ps_5_0", "ps_5_0"), SR_OIT_ABUFFER_VOLUME_DIST_MAP_ps_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11100), "SR_OIT_ABUFFER_UNDERCUT_ps_5_0", "ps_5_0"), SR_OIT_ABUFFER_UNDERCUT_ps_5_0);

		// note : picking ps is same but the input is different according to the rendering mode
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

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA11201), "OIT_SKBZ_RESOLVE_cs_5_0", "cs_5_0"), OIT_SKBZ_RESOLVE_cs_5_0);
		
		{
			if (g_psoManager->dx11_featureLevel > (D3D_FEATURE_LEVEL)0xb100) {
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
			}


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

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA31010), "GS_ThickPoints_gs_5_0", "gs_5_0"), GS_ThickPoints_gs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA31011), "GS_SurfelPoints_gs_5_0", "gs_5_0"), GS_SurfelPoints_gs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA31020), "GS_ThickLines_gs_5_0", "gs_5_0"), GS_ThickLines_gs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA31021), "GS_TriNormal_gs_5_0", "gs_5_0"), GS_TriNormal_gs_5_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50000), "VR_RAYMAX_cs_5_0", "cs_5_0"), VR_RAYMAX_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50001), "VR_RAYMIN_cs_5_0", "cs_5_0"), VR_RAYMIN_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50002), "VR_RAYSUM_cs_5_0", "cs_5_0"), VR_RAYSUM_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA51000), "VR_RAYMAX_SCULPTMASK_cs_5_0", "cs_5_0"), VR_RAYMAX_SCULPTMASK_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA51001), "VR_RAYMIN_SCULPTMASK_cs_5_0", "cs_5_0"), VR_RAYMIN_SCULPTMASK_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA51002), "VR_RAYSUM_SCULPTMASK_cs_5_0", "cs_5_0"), VR_RAYSUM_SCULPTMASK_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA51003), "VR_RAYMAX_SCULPTBITS_cs_5_0", "cs_5_0"), VR_RAYMAX_SCULPTBITS_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA51004), "VR_RAYMIN_SCULPTBITS_cs_5_0", "cs_5_0"), VR_RAYMIN_SCULPTBITS_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA51005), "VR_RAYSUM_SCULPTBITS_cs_5_0", "cs_5_0"), VR_RAYSUM_SCULPTBITS_cs_5_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50006), "VR_SURFACE_cs_5_0", "cs_5_0"), VR_SURFACE_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA90000), "SampleTest_cs_5_0", "cs_5_0"), SampleTest_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50100), "FillDither_cs_5_0", "cs_5_0"), FillDither_cs_5_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50003), "VR_DEFAULT_FM_cs_5_0", "cs_5_0"), VR_DEFAULT_FM_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50004), "VR_OPAQUE_FM_cs_5_0", "cs_5_0"), VR_OPAQUE_FM_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50007), "VR_OPAQUE_MULTIOTF_FM_cs_5_0", "cs_5_0"), VR_OPAQUE_MULTIOTF_FM_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50005), "VR_CONTEXT_FM_cs_5_0", "cs_5_0"), VR_CONTEXT_FM_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50011), "VR_MULTIOTF_FM_cs_5_0", "cs_5_0"), VR_MULTIOTF_FM_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50041), "VR_MULTIOTF_CONTEXT_FM_cs_5_0", "cs_5_0"), VR_MULTIOTF_CONTEXT_FM_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50012), "VR_MASKVIS_FM_cs_5_0", "cs_5_0"), VR_MASKVIS_FM_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50042), "VR_SCULPTMASK_FM_cs_5_0", "cs_5_0"), VR_SCULPTMASK_FM_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50043), "VR_SCULPTMASK_CONTEXT_FM_cs_5_0", "cs_5_0"), VR_SCULPTMASK_CONTEXT_FM_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50044), "VR_DEFAULT_SCULPTBITS_FM_cs_5_0", "cs_5_0"), VR_DEFAULT_SCULPTBITS_FM_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50045), "VR_CONTEXT_SCULPTBITS_FM_cs_5_0", "cs_5_0"), VR_CONTEXT_SCULPTBITS_FM_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50050), "VR_CINEMATIC_FM_cs_5_0", "cs_5_0"), VR_CINEMATIC_FM_cs_5_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50070), "VR_SINGLE_DEFAULT_FM_cs_5_0", "cs_5_0"), VR_SINGLE_DEFAULT_FM_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50071), "VR_SINGLE_OPAQUE_FM_cs_5_0", "cs_5_0"), VR_SINGLE_OPAQUE_FM_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50072), "VR_SINGLE_OPAQUE_MULTIOTF_FM_cs_5_0", "cs_5_0"), VR_SINGLE_OPAQUE_MULTIOTF_FM_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50073), "VR_SINGLE_CONTEXT_FM_cs_5_0", "cs_5_0"), VR_SINGLE_CONTEXT_FM_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50074), "VR_SINGLE_MULTIOTF_FM_cs_5_0", "cs_5_0"), VR_SINGLE_MULTIOTF_FM_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50075), "VR_SINGLE_MULTIOTF_CONTEXT_FM_cs_5_0", "cs_5_0"), VR_SINGLE_MULTIOTF_CONTEXT_FM_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50076), "VR_SINGLE_MASKVIS_FM_cs_5_0", "cs_5_0"), VR_SINGLE_MASKVIS_FM_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50077), "VR_SINGLE_SCULPTMASK_FM_cs_5_0", "cs_5_0"), VR_SINGLE_SCULPTMASK_FM_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50078), "VR_SINGLE_SCULPTMASK_CONTEXT_FM_cs_5_0", "cs_5_0"), VR_SINGLE_SCULPTMASK_CONTEXT_FM_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50080), "VR_SINGLE_DEFAULT_SCULPTBITS_FM_cs_5_0", "cs_5_0"), VR_SINGLE_DEFAULT_SCULPTBITS_FM_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50081), "VR_SINGLE_CONTEXT_SCULPTBITS_FM_cs_5_0", "cs_5_0"), VR_SINGLE_CONTEXT_SCULPTBITS_FM_cs_5_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50013), "VR_DEFAULT_cs_5_0", "cs_5_0"), VR_DEFAULT_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50014), "VR_OPAQUE_cs_5_0", "cs_5_0"), VR_OPAQUE_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50015), "VR_CONTEXT_cs_5_0", "cs_5_0"), VR_CONTEXT_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50016), "VR_MULTIOTF_cs_5_0", "cs_5_0"), VR_MULTIOTF_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA50017), "VR_MASKVIS_cs_5_0", "cs_5_0"), VR_MASKVIS_cs_5_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA60001), "PanoVR_RAYMAX_cs_5_0", "cs_5_0"), PanoVR_RAYMAX_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA60002), "PanoVR_RAYMIN_cs_5_0", "cs_5_0"), PanoVR_RAYMIN_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA60003), "PanoVR_RAYSUM_cs_5_0", "cs_5_0"), PanoVR_RAYSUM_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA60004), "PanoVR_DEFAULT_cs_5_0", "cs_5_0"), PanoVR_DEFAULT_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA60005), "PanoVR_MODULATE_cs_5_0", "cs_5_0"), PanoVR_MODULATE_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA60006), "PanoVR_MULTIOTF_DEFAULT_cs_5_0", "cs_5_0"), PanoVR_MULTIOTF_DEFAULT_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA60007), "PanoVR_MULTIOTF_MODULATE_cs_5_0", "cs_5_0"), PanoVR_MULTIOTF_MODULATE_cs_5_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA60100), "ThickSlicePathTracer_cs_5_0", "cs_5_0"), ThickSlicePathTracer_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA60101), "CurvedThickSlicePathTracer_cs_5_0", "cs_5_0"), CurvedThickSlicePathTracer_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA60102), "PickingThickSlice_cs_5_0", "cs_5_0"), PickingThickSlice_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA60103), "PickingCurvedThickSlice_cs_5_0", "cs_5_0"), PickingCurvedThickSlice_cs_5_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA60110), "ThickSlicePathTracer_GPUBVH_cs_5_0", "cs_5_0"), ThickSlicePathTracer_GPUBVH_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA60111), "CurvedThickSlicePathTracer_GPUBVH_cs_5_0", "cs_5_0"), CurvedThickSlicePathTracer_GPUBVH_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA60112), "PickingThickSlice_GPUBVH_cs_5_0", "cs_5_0"), PickingThickSlice_GPUBVH_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA60113), "PickingCurvedThickSlice_GPUBVH_cs_5_0", "cs_5_0"), PickingCurvedThickSlice_GPUBVH_cs_5_0);

		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA60105), "SliceOutline_cs_5_0", "cs_5_0"), SliceOutline_cs_5_0);

#pragma region Particle
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA71000), "PCE_BlobRayMarching_cs_5_0", "cs_5_0"), PCE_BlobRayMarching_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA71001), "PCE_KickoffEmitterSystem_cs_5_0", "cs_5_0"), PCE_KickoffEmitterSystem_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA71002), "PCE_ParticleEmitter_cs_5_0", "cs_5_0"), PCE_ParticleEmitter_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA71003), "PCE_ParticleSimulation_cs_5_0", "cs_5_0"), PCE_ParticleSimulation_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA71005), "PCE_ParticleSimulationSort_cs_5_0", "cs_5_0"), PCE_ParticleSimulationSort_cs_5_0);
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA71004), "PCE_ParticleUpdateFinish_cs_5_0", "cs_5_0"), PCE_ParticleUpdateFinish_cs_5_0);
#pragma endregion
		VRETURN(register_shader(MAKEINTRESOURCE(IDR_RCDATA72000), "CS_Blend2ndLayer_cs_5_0", "cs_5_0"), CS_Blend2ndLayer_cs_5_0);

#endif
	}

	g_psoManager->is_initialized = true;
	gpu_params = g_psoManager;

	if (hr != S_OK)
		return __INVALID_GPU;

	if (g_psoManager->dx11_featureLevel <= 0x9300)
		return __LOW_SPEC_NOT_SUPPORT_CS_GPU;

	return __SUPPORTED_GPU;

ERROR_PRESETTING:
	Deinitialize();
	gpu_params = g_psoManager;
	return __INVALID_GPU;
}

PSOManager* grd_helper::GetPSOManager()
{
	return g_psoManager;
}

MeshPainter* grd_helper::GetMeshPainter()
{
	return meshPainter;
}

void grd_helper::Deinitialize()
{
	if (g_psoManager == NULL) return;
	if (!g_psoManager->is_initialized) return;

	if (meshPainter)
	{
		delete meshPainter;
		meshPainter = nullptr;
	}

	if (g_psoManager->dx11DeviceImmContext)
	{
		g_psoManager->dx11DeviceImmContext->Flush();
		g_psoManager->dx11DeviceImmContext->ClearState();
	}

	VMSAFE_RELEASE(srvPushConstant);
	VMSAFE_RELEASE(pushConstant);
	VMSAFE_RELEASE(pushConstant_write);

	g_psoManager->Delete();

	if (g_psoManager->dx11DeviceImmContext)
	{
		g_psoManager->dx11DeviceImmContext->Flush();
		g_psoManager->dx11DeviceImmContext->ClearState();
	}

	g_psoManager->dx11Device = NULL;
	g_psoManager->dx11DeviceImmContext = NULL;
	memset(&g_psoManager->dx11_adapter, NULL, sizeof(DXGI_ADAPTER_DESC));
	g_psoManager->dx11_featureLevel = D3D_FEATURE_LEVEL_9_1;
	g_psoManager->is_initialized = false;
}

const Variant* grd_helper::GetPSOVariant(uint32_t mask)
{
	static ID3D11InputLayout* dx11LI_P = (ID3D11InputLayout*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "P"));
	static ID3D11InputLayout* dx11LI_PN = (ID3D11InputLayout*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "PN"));
	static ID3D11InputLayout* dx11LI_PT = (ID3D11InputLayout*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "PT"));
	static ID3D11InputLayout* dx11LI_PC = (ID3D11InputLayout*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "PC"));
	static ID3D11InputLayout* dx11LI_PNT = (ID3D11InputLayout*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "PNT"));
	static ID3D11InputLayout* dx11LI_PNC = (ID3D11InputLayout*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "PNC"));
	static ID3D11InputLayout* dx11LI_PTC = (ID3D11InputLayout*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "PTC"));
	static ID3D11InputLayout* dx11LI_PNTC = (ID3D11InputLayout*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "PNTC"));
	static ID3D11InputLayout* dx11LI_PTTT = (ID3D11InputLayout*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "PTTT"));
	static ID3D11InputLayout* dx11LI_PG = (ID3D11InputLayout*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "PG"));
	static ID3D11InputLayout* dx11LI_PNG = (ID3D11InputLayout*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "PNG"));
	static ID3D11InputLayout* dx11LI_PTG = (ID3D11InputLayout*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "PTG"));
	static ID3D11InputLayout* dx11LI_PCG = (ID3D11InputLayout*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "PCG"));
	static ID3D11InputLayout* dx11LI_PNTG = (ID3D11InputLayout*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "PNTG"));
	static ID3D11InputLayout* dx11LI_PNCG = (ID3D11InputLayout*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "PNCG"));
	static ID3D11InputLayout* dx11LI_PTCG = (ID3D11InputLayout*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "PTCG"));
	static ID3D11InputLayout* dx11LI_PNTCG = (ID3D11InputLayout*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "PNTCG"));

#ifdef DX10_0
	static ID3D11VertexShader* dx11VShader_Quad = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_QUAD_P_vs_4_0"));
	static ID3D11VertexShader* dx11VShader_P = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_P_vs_4_0"));
	static ID3D11VertexShader* dx11VShader_PN = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PN_vs_4_0"));
	static ID3D11VertexShader* dx11VShader_PT = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PT_vs_4_0"));
	static ID3D11VertexShader* dx11VShader_PC = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PC_vs_4_0"));
	static ID3D11VertexShader* dx11VShader_PNT = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PNT_vs_4_0"));
	static ID3D11VertexShader* dx11VShader_PNC = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PNC_vs_4_0"));
	static ID3D11VertexShader* dx11VShader_PTC = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PTC_vs_4_0"));
	static ID3D11VertexShader* dx11VShader_PNTC = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PNTC_vs_4_0"));
	static ID3D11VertexShader* dx11VShader_PTTT = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PTTT_vs_4_0"));
	static ID3D11VertexShader* dx11VShader_PG = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PG_vs_4_0"));
	static ID3D11VertexShader* dx11VShader_PNG = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PNG_vs_4_0"));
	static ID3D11VertexShader* dx11VShader_PTG = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PTG_vs_4_0"));
	static ID3D11VertexShader* dx11VShader_PCG = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PCG_vs_4_0"));
	static ID3D11VertexShader* dx11VShader_PNTG = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PNTG_vs_4_0"));
	static ID3D11VertexShader* dx11VShader_PNCG = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PNCG_vs_4_0"));
	static ID3D11VertexShader* dx11VShader_PTCG = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PTCG_vs_4_0"));
	static ID3D11VertexShader* dx11VShader_PNTCG = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PNTCG_vs_4_0"));
#else
	static ID3D11VertexShader* dx11VShader_Quad = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_QUAD_P_vs_5_0"));
	static ID3D11VertexShader* dx11VShader_P = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_P_vs_5_0"));
	static ID3D11VertexShader* dx11VShader_PN = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PN_vs_5_0"));
	static ID3D11VertexShader* dx11VShader_PT = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PT_vs_5_0"));
	static ID3D11VertexShader* dx11VShader_PC = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PC_vs_5_0"));
	static ID3D11VertexShader* dx11VShader_PNT = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PNT_vs_5_0"));
	static ID3D11VertexShader* dx11VShader_PNC = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PNC_vs_5_0"));
	static ID3D11VertexShader* dx11VShader_PTC = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PTC_vs_5_0"));
	static ID3D11VertexShader* dx11VShader_PNTC = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PNTC_vs_5_0"));
	static ID3D11VertexShader* dx11VShader_PTTT = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PTTT_vs_5_0"));
	static ID3D11VertexShader* dx11VShader_IDX = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_IDX_vs_5_0"));
	static ID3D11VertexShader* dx11VShader_PG = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PG_vs_5_0"));
	static ID3D11VertexShader* dx11VShader_PNG = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PNG_vs_5_0"));
	static ID3D11VertexShader* dx11VShader_PTG = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PTG_vs_5_0"));
	static ID3D11VertexShader* dx11VShader_PCG = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PCG_vs_5_0"));
	static ID3D11VertexShader* dx11VShader_PNTG = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PNTG_vs_5_0"));
	static ID3D11VertexShader* dx11VShader_PNCG = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PNCG_vs_5_0"));
	static ID3D11VertexShader* dx11VShader_PTCG = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PTCG_vs_5_0"));
	static ID3D11VertexShader* dx11VShader_PNTCG = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PNTCG_vs_5_0"));
#endif

	static Variant kVariants[] = {
		{ M_P,    "P",    dx11VShader_P,    dx11LI_P    },
		{ M_PN,   "PN",   dx11VShader_PN,   dx11LI_PN   },
		{ M_PT,   "PT",   dx11VShader_PT,   dx11LI_PT   },
		{ M_PC,   "PC",   dx11VShader_PC,   dx11LI_PC   },
		{ M_PNT,  "PNT",  dx11VShader_PNT,  dx11LI_PNT  },
		{ M_PNC,  "PNC",  dx11VShader_PNC,  dx11LI_PNC  },
		{ M_PTC,  "PTC",  dx11VShader_PTC,  dx11LI_PTC  },
		{ M_PNTC, "PNTC", dx11VShader_PNTC, dx11LI_PNTC },
		{ M_PTTT, "PTTT", dx11VShader_PTTT, dx11LI_PTTT },
		{ M_PG,    "PG",    dx11VShader_PG,    dx11LI_PG    },
		{ M_PNG,   "PNG",   dx11VShader_PNG,   dx11LI_PNG   },
		{ M_PTG,   "PTG",   dx11VShader_PTG,   dx11LI_PTG   },
		{ M_PCG,   "PCG",   dx11VShader_PCG,   dx11LI_PCG   },
		{ M_PNTG,  "PNTG",  dx11VShader_PNTG,  dx11LI_PNTG  },
		{ M_PNCG,  "PNCG",  dx11VShader_PNCG,  dx11LI_PNCG  },
		{ M_PTCG,  "PTCG",  dx11VShader_PTCG,  dx11LI_PTCG  },
		{ M_PNTCG, "PNTCG", dx11VShader_PNTCG, dx11LI_PNTCG },
	};

	if (mask == ~0)
	{
#ifdef DX10_0
		dx11VShader_P = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_P_vs_4_0"));
		dx11VShader_PN = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PN_vs_4_0"));
		dx11VShader_PT = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PT_vs_4_0"));
		dx11VShader_PC = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PC_vs_4_0"));
		dx11VShader_PNT = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PNT_vs_4_0"));
		dx11VShader_PNC = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PNC_vs_4_0"));
		dx11VShader_PTC = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PTC_vs_4_0"));
		dx11VShader_PNTC = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PNTC_vs_4_0"));
		dx11VShader_PTTT = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PTTT_vs_4_0"));
		dx11VShader_PG = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PG_vs_4_0"));
		dx11VShader_PNG = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PNG_vs_4_0"));
		dx11VShader_PTG = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PTG_vs_4_0"));
		dx11VShader_PCG = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PCG_vs_4_0"));
		dx11VShader_PNTG = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PNTG_vs_4_0"));
		dx11VShader_PNCG = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PNCG_vs_4_0"));
		dx11VShader_PTCG = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PTCG_vs_4_0"));
		dx11VShader_PNTCG = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PNTCG_vs_4_0"));
#else
		dx11VShader_P = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_P_vs_5_0"));
		dx11VShader_PN = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PN_vs_5_0"));
		dx11VShader_PT = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PT_vs_5_0"));
		dx11VShader_PC = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PC_vs_5_0"));
		dx11VShader_PNT = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PNT_vs_5_0"));
		dx11VShader_PNC = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PNC_vs_5_0"));
		dx11VShader_PTC = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PTC_vs_5_0"));
		dx11VShader_PNTC = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PNTC_vs_5_0"));
		dx11VShader_PTTT = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PTTT_vs_5_0"));
		dx11VShader_IDX = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_IDX_vs_5_0"));
		dx11VShader_PG = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PG_vs_5_0"));
		dx11VShader_PNG = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PNG_vs_5_0"));
		dx11VShader_PTG = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PTG_vs_5_0"));
		dx11VShader_PCG = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PCG_vs_5_0"));
		dx11VShader_PNTG = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PNTG_vs_5_0"));
		dx11VShader_PNCG = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PNCG_vs_5_0"));
		dx11VShader_PTCG = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PTCG_vs_5_0"));
		dx11VShader_PNTCG = (ID3D11VertexShader*)g_psoManager->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PNTCG_vs_5_0"));
#endif

		Variant tmp[17] = {
		{ M_P,    "P",    dx11VShader_P,    dx11LI_P    },
		{ M_PN,   "PN",   dx11VShader_PN,   dx11LI_PN   },
		{ M_PT,   "PT",   dx11VShader_PT,   dx11LI_PT   },
		{ M_PC,   "PC",   dx11VShader_PC,   dx11LI_PC   },
		{ M_PNT,  "PNT",  dx11VShader_PNT,  dx11LI_PNT  },
		{ M_PNC,  "PNC",  dx11VShader_PNC,  dx11LI_PNC  },
		{ M_PTC,  "PTC",  dx11VShader_PTC,  dx11LI_PTC  },
		{ M_PNTC, "PNTC", dx11VShader_PNTC, dx11LI_PNTC },
		{ M_PTTT, "PTTT", dx11VShader_PTTT, dx11LI_PTTT },
		{ M_PG,    "PG",    dx11VShader_PG,    dx11LI_PG    },
		{ M_PNG,   "PNG",   dx11VShader_PNG,   dx11LI_PNG   },
		{ M_PTG,   "PTG",   dx11VShader_PTG,   dx11LI_PTG   },
		{ M_PCG,   "PCG",   dx11VShader_PCG,   dx11LI_PCG   },
		{ M_PNTG,  "PNTG",  dx11VShader_PNTG,  dx11LI_PNTG  },
		{ M_PNCG,  "PNCG",  dx11VShader_PNCG,  dx11LI_PNCG  },
		{ M_PTCG,  "PTCG",  dx11VShader_PTCG,  dx11LI_PTCG  },
		{ M_PNTCG, "PNTCG", dx11VShader_PNTCG, dx11LI_PNTCG },
		};
		std::memcpy(kVariants, tmp, sizeof(kVariants));

		return nullptr;
	}



	for (auto& v : kVariants) {
		if (v.mask == mask) return &v;
	}

	if ((mask & (A_T1 | A_T2)) == (A_T1 | A_T2) && (mask & A_T0)) {
		for (auto& v : kVariants) if (v.mask == M_PTTT) return &v;
	}

	const Variant* best = nullptr;
	int bestBits = -1;
	for (auto& v : kVariants) {
		if ((mask & v.mask) == v.mask) {
			int bits = __popcnt(v.mask);
			if (bits > bestBits) { bestBits = bits; best = &v; }
		}
	}
	return best; 
}

const ID3D11ShaderResourceView* grd_helper::GetPushContantSRV()
{
	return srvPushConstant;
}

void grd_helper::PushConstants(const void* data, uint32_t size, uint32_t offset)
{
	assert(size % sizeof(uint32_t) == 0);
	assert(offset % sizeof(uint32_t) == 0);

	D3D11_MAPPED_SUBRESOURCE mapped_pushConstants;
	g_psoManager->dx11DeviceImmContext->Map(pushConstant_write, 0, D3D11_MAP_WRITE, 0, &mapped_pushConstants);
	BVHPushConstants* cbData = (BVHPushConstants*)mapped_pushConstants.pData;
	memcpy((uint8_t*)mapped_pushConstants.pData + offset, data, size);
	g_psoManager->dx11DeviceImmContext->Unmap(pushConstant_write, 0);
	g_psoManager->dx11DeviceImmContext->Flush();

	g_psoManager->dx11DeviceImmContext->CopyResource(pushConstant, pushConstant_write);
	g_psoManager->dx11DeviceImmContext->Flush();
	Fence();
}

void grd_helper::CheckReusability(GpuRes& gres, VmObject* resObj, bool& update_data, bool& regen_data,
	const vmobjects::VmParamMap<std::string, std::any>& res_new_values)
{
	uint64_t _gpu_gen_timg = gres.res_values.GetParam("LAST_UPDATE_TIME", (uint64_t)0);
	uint64_t _cpu_gen_timg = resObj->GetContentUpdateTime();
	if (_gpu_gen_timg < _cpu_gen_timg)
	{
		// now, at least update
		update_data = true;
		bool is_reuse_memory = resObj->GetObjParam("_bool_ReuseGpuMemory", false);

		//vmlog::LogInfo(">>> OBJ ID : " + std::to_string(resObj->GetObjectID()) + ", RES TYPE : " + gres.res_name + ", reuseTag : " + (is_reuse_memory ? "true" : "false"));
		if (!is_reuse_memory)
		{
			regen_data = true;
			// add properties
			for (auto it = res_new_values.begin(); it != res_new_values.end(); it++)
				gres.res_values.SetParam(it->first, it->second);
		}
	}
};

void grd_helper::Fence()
{
	//g_psoManager->dx11DeviceImmContext->End(g_psoManager->dx11qr_fenceQuery);
	//BOOL query_finished = FALSE;
	//while (S_OK != g_psoManager->dx11DeviceImmContext->GetData(g_psoManager->dx11qr_fenceQuery, &query_finished, sizeof(query_finished), 0) || !query_finished) {
	//	Sleep(0);
	//}
}

int __UpdateBlocks(GpuRes& gres, const VmVObjectVolume* vobj, const string& vmode, const DXGI_FORMAT dxformat, LocalProgress* progress)
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

	gres.res_values.SetParam("WIDTH", (uint32_t)volblk->blk_vol_size.x);
	gres.res_values.SetParam("HEIGHT", (uint32_t)volblk->blk_vol_size.y);
	gres.res_values.SetParam("DEPTH", (uint32_t)volblk->blk_vol_size.z);

	uint32_t num_blk_units = volblk->blk_vol_size.x * volblk->blk_vol_size.y * volblk->blk_vol_size.z;
	if(vmode.find("OTFTAG") != string::npos)
		assert(dxformat == DXGI_FORMAT_R8_UNORM);
	else if(vmode.find("VMIN") != string::npos || vmode.find("VMAX") != string::npos)
		assert(dxformat == DXGI_FORMAT_R16_UNORM);
	else 
		assert(0);
	//gres.options["FORMAT"] = (vmode.find("OTFTAG") != string::npos) && num_blk_units >= 65535 ? DXGI_FORMAT_R32_FLOAT : DXGI_FORMAT_R16_UNORM;
	gres.options["FORMAT"] = dxformat;

	//if (vmode.find("OTFTAG") != string::npos)
	//	cout << vmode << endl;

	g_pCGpuManager->GenerateGpuResource(gres);

	return 1;
}

bool grd_helper::UpdateOtfBlocks(GpuRes& gres, VmVObjectVolume* main_vobj, VmVObjectVolume* mask_vobj,
	VmObject* tobj, const int sculpt_value, LocalProgress* progress)
{
	int tobj_id = tobj->GetObjectID();

	int is_new = __UpdateBlocks(gres, main_vobj, string("OTFTAG_") + to_string(tobj_id), DXGI_FORMAT_R8_UNORM, progress);
	
	vmdouble2 otf_Mm_range = vmdouble2(DBL_MAX, -DBL_MAX);
	MapTable* tmap_data = tobj->GetObjParamPtr<MapTable>("_TableMap_OTF");


	VolumeData* vol_data = main_vobj->GetVolumeData();
	float value_range = 65536.f;
	if (vol_data->store_dtype.type_bytes == data_type::dtype<uint8_t>().type_bytes) 
		value_range = 256.f;
	else assert(vol_data->store_dtype.type_bytes == data_type::dtype<uint16_t>().type_bytes); 

	float scale_tf2volume = value_range / (float)tmap_data->array_lengths.x;

	otf_Mm_range.x = tmap_data->valid_min_idx.x * scale_tf2volume;
	otf_Mm_range.y = tmap_data->valid_max_idx.x * scale_tf2volume;

	const int blk_level = __BLKLEVEL;	// 0 : High Resolution, 1 : Low Resolution
	VolumeBlocks* volblk = ((VmVObjectVolume*)main_vobj)->GetVolumeBlock(blk_level);

	uint64_t _tob_time_cpu = tobj->GetContentUpdateTime();
	uint64_t _blk_time_cpu = volblk->GetUpdateTime(tobj_id);

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
	HRESULT hr = g_psoManager->dx11DeviceImmContext->Map(pdx11tx3d_blkmap, 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes);

	vmint3 blk_bnd_size = volblk->blk_bnd_size;
	vmint3 blk_sample_width = volblk->blk_vol_size + blk_bnd_size * 2;
	int blk_sample_slice = blk_sample_width.x * blk_sample_width.y;

	uint32_t _format = gres.options["FORMAT"];
	uint32_t count = 0;
	uint32_t count_id = 0;
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
					((byte*)d11MappedRes.pData)[y * d11MappedRes.RowPitch / 1 + z * d11MappedRes.DepthPitch / 1 + x] = 255;
				}
				count_id++;
			}
	}

	g_psoManager->dx11DeviceImmContext->Unmap(pdx11tx3d_blkmap, 0);
	if (progress)
		*progress->progress_ptr = (progress->start + progress->range);
	return true;
}

bool grd_helper::UpdateMinMaxBlocks(GpuRes& gres_min, GpuRes& gres_max, const VmVObjectVolume* vobj, LocalProgress* progress)
{
	int is_new1 = __UpdateBlocks(gres_min, vobj, "VMIN", DXGI_FORMAT_R16_UNORM, progress);
	int is_new2 = __UpdateBlocks(gres_max, vobj, "VMAX", DXGI_FORMAT_R16_UNORM, progress);
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
	HRESULT hr = g_psoManager->dx11DeviceImmContext->Map(pdx11tx3d_min_blk, 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes_Min);
	hr |= g_psoManager->dx11DeviceImmContext->Map(pdx11tx3d_max_blk, 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes_Max);

	uint16_t* min_data = (uint16_t*)d11MappedRes_Min.pData;
	uint16_t* max_data = (uint16_t*)d11MappedRes_Max.pData;
	uint32_t count = 0;
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
	g_psoManager->dx11DeviceImmContext->Unmap(pdx11tx3d_min_blk, 0);
	g_psoManager->dx11DeviceImmContext->Unmap(pdx11tx3d_max_blk, 0);
	if (progress)
		*progress->progress_ptr = (progress->start + progress->range);
	return true;
}

auto GetOption = [](const GpuRes& gres, const std::string& flag_name) -> uint32_t
{
	auto it = gres.options.find(flag_name);
	if (it == gres.options.end()) return 0;
	return it->second;
};

template <typename GPUTYPE, typename CPUTYPE> bool __FillVolumeValues(CPUTYPE* gpu_data, const GPUTYPE** cpu_data,
	const bool is_downscaled, const bool use_nearest, const int valueoffset,
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

					if (use_nearest) {
						vmint3 i3PosSampleVS = vmint3((int)(fPosX + 0.5f), (int)(fPosY + 0.5f), (int)(fPosZ + 0.5f));
						int iMinMaxAddrX = min(max(i3PosSampleVS.x, 0), cpu_size.x - 1) + cpu_bnd_size.x;
						int iMinMaxAddrY = (min(max(i3PosSampleVS.y, 0), cpu_size.y - 1) + cpu_bnd_size.y) * cpu_sample_width;
						int iSampleAddrZ = i3PosSampleVS.z + cpu_bnd_size.z;

						tVoxelValue = cpu_data[iSampleAddrZ][iMinMaxAddrX + iMinMaxAddrY] + valueoffset;
					}
					else {
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
						tVoxelValue = (CPUTYPE)((int)fSampleTrilinear + valueoffset);
					}
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

float g_maxVolumeSizeKB = 1024.f * 1024.f;
float g_maxVolumeExtent = 2048.f;
void grd_helper::SetUserCapacity(const float maxVolumeSizeKB, const float maxVolumeExtent)
{
	g_maxVolumeSizeKB = maxVolumeSizeKB;
	g_maxVolumeExtent = maxVolumeExtent;
}

bool grd_helper::UpdateVolumeModel(GpuRes& gres, VmVObjectVolume* vobj, const bool use_nearest_max, bool heuristicResize, LocalProgress* progress)
{
	bool is_nearest_max_vol = vobj->GetObjParam("_bool_UseNearestMax", use_nearest_max);

	gres.vm_src_id = vobj->GetObjectID();
	gres.res_name = string("VOLUME_MODEL_") + (heuristicResize? "RESIZED_" : "ORIGINAL_") + (is_nearest_max_vol ? string("NEAREST_MAX") : string("DEFAULT"));

	if (g_pCGpuManager->UpdateGpuResource(gres)) {
		uint64_t _gpu_gen_timg = gres.res_values.GetParam("LAST_UPDATE_TIME", (uint64_t)0);
		uint64_t _cpu_gen_timg = vobj->GetContentUpdateTime();
		if (_gpu_gen_timg > _cpu_gen_timg)
			return true;
	}

	gres.rtype = RTYPE_TEXTURE3D;
	gres.options["USAGE"] = D3D11_USAGE_IMMUTABLE; // D3D11_USAGE_DYNAMIC
	gres.options["CPU_ACCESS_FLAG"] = 0;// D3D11_CPU_ACCESS_WRITE;
	//gres.options["USAGE"] = D3D11_USAGE_DYNAMIC;
	//gres.options["CPU_ACCESS_FLAG"] = D3D11_CPU_ACCESS_WRITE;
	gres.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE;

	const VolumeData* vol_data = ((VmVObjectVolume*)vobj)->GetVolumeData();
	if (vol_data->store_dtype.type_name == data_type::dtype<uint8_t>().type_name)
		gres.options["FORMAT"] = DXGI_FORMAT_R8_UNORM;
	else if (vol_data->store_dtype.type_name == data_type::dtype<uint16_t>().type_name)
		gres.options["FORMAT"] = DXGI_FORMAT_R16_UNORM;
	else
	{
		vmlog::LogErr("Not supported Data Type");
		return false;
	}

	double hueristic_size = 256.0;
	int hueristic_res = 256;
	if (!heuristicResize) {
		hueristic_size = 100000.0;
		hueristic_res = 100000;
	}

	double half_criteria_KB = (float)g_maxVolumeSizeKB;

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
		//sample_offset.x = sample_offset.y = sample_offset.z = ceil((float)dRescaleSize);
	}

	auto ResizeVolumeOffset = [](const int volSize, const float inOffset, float& outOffset)
		{
			float resample_size = (float)volSize / inOffset;
			if (resample_size > 2048.f)
			{
				outOffset = (float)volSize / g_maxVolumeExtent;
				assert(outOffset > inOffset);
			}
		};
	ResizeVolumeOffset(vol_data->vol_size.x, sample_offset.x, sample_offset.x);
	ResizeVolumeOffset(vol_data->vol_size.y, sample_offset.y, sample_offset.y);
	ResizeVolumeOffset(vol_data->vol_size.z, sample_offset.z, sample_offset.z);
	
	////////////////////////////////////
	// Texture for Volume Description //
RETRY:
	int vol_size_x = max((int)((float)vol_data->vol_size.x / sample_offset.x), 1);
	int vol_size_y = max((int)((float)vol_data->vol_size.y / sample_offset.y), 1);
	int vol_size_z = max((int)((float)vol_data->vol_size.z / sample_offset.z), 1);
	gres.res_values.SetParam("WIDTH", (uint32_t)vol_size_x);
	gres.res_values.SetParam("HEIGHT", (uint32_t)vol_size_y);
	gres.res_values.SetParam("DEPTH", (uint32_t)vol_size_z);
	gres.res_values.SetParam("SAMPLE_OFFSET_X", (float)sample_offset.x);
	gres.res_values.SetParam("SAMPLE_OFFSET_Y", (float)sample_offset.y);
	gres.res_values.SetParam("SAMPLE_OFFSET_Z", (float)sample_offset.z);

	//cout << "***************>> " << vol_size_x << ", " << vol_size_y << ", " << vol_size_z << endl;

	if (vol_size_x > hueristic_res)
	{
		sample_offset.x += 0.2f;
		goto RETRY;
	}
	if (vol_size_y > hueristic_res)
	{
		sample_offset.y += 0.2f;
		goto RETRY;
	}
	if (vol_size_z > hueristic_res)
	{
		sample_offset.z += 0.2f;
		goto RETRY;
	}

	sample_offset.x = (float)vol_data->vol_size.x / (float)vol_size_x;
	sample_offset.y = (float)vol_data->vol_size.y / (float)vol_size_y;
	sample_offset.z = (float)vol_data->vol_size.z / (float)vol_size_z;

	//vzlog("sample_offset: %f, %f, %f", sample_offset.x, sample_offset.y, sample_offset.z);
	//printf("GPU Uploaded Volume Size : %d KB (%dx%dx%d) %d bytes\n",

	bool is_downscaled = sample_offset.x > 1.f || sample_offset.y > 1.f || sample_offset.z > 1.f;
	//vmint2 gpu_row_depth_pitch = 0;
	D3D11_SUBRESOURCE_DATA subRes;
	subRes.pSysMem = NULL;
	subRes.SysMemPitch = vol_size_x;
	subRes.SysMemSlicePitch = vol_size_x * vol_size_y;
	switch (gres.options["FORMAT"])
	{
	case DXGI_FORMAT_R8_UNORM:
		subRes.pSysMem = new byte[vol_size_x * vol_size_y * vol_size_z];
		__FillVolumeValues((byte*)subRes.pSysMem, (const byte**)vol_data->vol_slices, is_downscaled, is_nearest_max_vol, 0,
			vol_data->vol_size, vmint3(vol_size_x, vol_size_y, vol_size_z), vol_data->bnd_size, vmint2(subRes.SysMemPitch, subRes.SysMemSlicePitch), progress);
		break;
	case DXGI_FORMAT_R16_UNORM:
		subRes.pSysMem = new uint16_t[vol_size_x * vol_size_y * vol_size_z];
		__FillVolumeValues((uint16_t*)subRes.pSysMem, (const uint16_t**)vol_data->vol_slices, is_downscaled, is_nearest_max_vol, 0,
			vol_data->vol_size, vmint3(vol_size_x, vol_size_y, vol_size_z), vol_data->bnd_size, vmint2(subRes.SysMemPitch, subRes.SysMemSlicePitch), progress);
		subRes.SysMemPitch *= 2;
		subRes.SysMemSlicePitch *= 2;
		break;
	default:
		break;
	}
	
	gres.res_values.SetParam("SUB_RESOURCE", subRes);
	g_pCGpuManager->GenerateGpuResource(gres);
	delete[] subRes.pSysMem;// (gres.options["FORMAT"] == DXGI_FORMAT_R8_UNORM ? (byte*)subRes.pSysMem : (uint16_t*)subRes.pSysMem);

	//switch (gres.options["FORMAT"])
	//{
	//case DXGI_FORMAT_R8_UNORM:
	//	delete[] (byte*)subRes.pSysMem;
	//	break;
	//case DXGI_FORMAT_R16_UNORM:
	//	delete[] (uint16_t*)subRes.pSysMem;
	//	break;
	//default:
	//	break;
	//}

	return true;

	//===================
	// the following code is for 
	//		gres.options["USAGE"] = D3D11_USAGE_DYNAMIC;
	//		gres.options["CPU_ACCESS_FLAG"] = D3D11_CPU_ACCESS_WRITE;

	ID3D11Texture3D* pdx11tx3d_volume = (ID3D11Texture3D*)gres.alloc_res_ptrs[DTYPE_RES];
	D3D11_MAPPED_SUBRESOURCE d11MappedRes;
	HRESULT hr = g_psoManager->dx11DeviceImmContext->Map(pdx11tx3d_volume, 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes);
	vmint2 gpu_row_depth_pitch = vmint2(d11MappedRes.RowPitch, d11MappedRes.DepthPitch);
	switch (gres.options["FORMAT"])
	{
	case DXGI_FORMAT_R8_UNORM:
		__FillVolumeValues((byte*)d11MappedRes.pData, (const byte**)vol_data->vol_slices, is_downscaled, is_nearest_max_vol, 0,
			vol_data->vol_size, vmint3(vol_size_x, vol_size_y, vol_size_z), vol_data->bnd_size, gpu_row_depth_pitch, progress);
		break;
	case DXGI_FORMAT_R16_UNORM:
		gpu_row_depth_pitch /= 2;
		__FillVolumeValues((uint16_t*)d11MappedRes.pData, (const uint16_t**)vol_data->vol_slices, is_downscaled, is_nearest_max_vol, 0,
			vol_data->vol_size, vmint3(vol_size_x, vol_size_y, vol_size_z), vol_data->bnd_size, gpu_row_depth_pitch, progress);
		break;
	default:
		break;
	}
	g_psoManager->dx11DeviceImmContext->Unmap(pdx11tx3d_volume, 0);

	return true;
}

bool grd_helper::UpdateTMapBuffer(GpuRes& gres, VmObject* tobj, const bool isPreInt, LocalProgress* progress)
{
	gres.vm_src_id = tobj->GetObjectID();
	gres.res_name = isPreInt? string("PREINT_OTF_BUFFER") : string("OTF_BUFFER");

	MapTable* tmap_data = tobj->GetObjParamPtr<MapTable>("_TableMap_OTF");

	bool needRegen = true;
	if (g_pCGpuManager->UpdateGpuResource(gres)) {

		uint32_t nemElementPrev = gres.res_values.GetParam("NUM_ELEMENTS", (uint32_t)0);
		uint32_t typeBytesPrev = gres.res_values.GetParam("STRIDE_BYTES", (uint32_t)0);

		needRegen = (nemElementPrev * typeBytesPrev)
			!= ((uint32_t)(tmap_data->array_lengths.x * (tmap_data->array_lengths.y)) * (isPreInt ? (uint32_t)16 : (uint32_t)tmap_data->dtype.type_bytes));

		uint64_t _tp_cpu = tobj->GetContentUpdateTime();
		uint64_t _tp_gpu = gres.res_values.GetParam("LAST_UPDATE_TIME", (uint64_t)0);
		if (_tp_gpu >= _tp_cpu) {
			if (needRegen)
			{
				vzlog_warning("INCONSISTENT STATE: GPU timestamp is up-to-date but size changed. _tp_cpu(%llu), _tp_gpu(%llu), forcing regeneration", _tp_cpu, _tp_gpu);
			}
			else
			{
				return true;
			}
		}
	}

	if (needRegen) {
		gres.rtype = RTYPE_BUFFER;
		gres.options["USAGE"] = D3D11_USAGE_DYNAMIC;
		gres.options["CPU_ACCESS_FLAG"] = D3D11_CPU_ACCESS_WRITE;
		gres.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE;
		gres.options["FORMAT"] = isPreInt? DXGI_FORMAT_R32G32B32A32_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;

		gres.res_values.SetParam("NUM_ELEMENTS", (uint32_t)(tmap_data->array_lengths.x * (tmap_data->array_lengths.y)));
		gres.res_values.SetParam("STRIDE_BYTES", isPreInt? (uint32_t)16 : (uint32_t)tmap_data->dtype.type_bytes);

		// including safe-delete to avoid redundant gen
		g_pCGpuManager->GenerateGpuResource(gres);
	}

	D3D11_MAPPED_SUBRESOURCE d11MappedRes;
	g_psoManager->dx11DeviceImmContext->Map((ID3D11Resource*)gres.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes);
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
	g_psoManager->dx11DeviceImmContext->Unmap((ID3D11Resource*)gres.alloc_res_ptrs[DTYPE_RES], 0);

	gres.options["Update LAST_UPDATE_TIME"] = 1u;
	g_pCGpuManager->UpdateGpuResource(gres);

	return true;
}

bool grd_helper::UpdatePrimitiveModel(map<string, GpuRes>& map_gres_vtxs, GpuRes& gres_idx, map<string, GpuRes>& map_gres_texs, VmVObjectPrimitive* pobj, VmObject* imgObj, bool* hasTextureMap, LocalProgress* progress)
{
	PrimitiveData* prim_data = ((VmVObjectPrimitive*)pobj)->GetPrimitiveData();

	bool update_data = pobj->GetObjParam("_bool_UpdateData", false);
	// always

#define VERTEX_DEF_LIST \
    X(POSITION) \
    X(NORMAL) \
    X(TEXCOORD0) \
    X(COLOR) \
    X(TEXCOORD1) \
    X(TEXCOORD2)  \
    X(GEODIST)  \

	enum class VERTEX_DEFINITIONS : int
	{
#define X(name) name,
		VERTEX_DEF_LIST
#undef X
		VTX_DEF_COUNT
	};

	static const char* vtx_def_names[VERTEX_DEFINITIONS::VTX_DEF_COUNT] =
	{
	#define X(name) #name,
		VERTEX_DEF_LIST
	#undef X
	};

	for (int i = 0; i < (int)VERTEX_DEFINITIONS::VTX_DEF_COUNT; ++i)
	{
		bool update_data_attriute = update_data;

		uint8_t* vtx_buf = prim_data->GetVerticeDefinition<uint8_t>(vtx_def_names[i]);
		if (vtx_buf == nullptr)
			continue;

		GpuRes& gres_vtx = map_gres_vtxs[vtx_def_names[i]];
		gres_vtx.vm_src_id = pobj->GetObjectID();
		gres_vtx.res_name = "VTX_" + string(vtx_def_names[i]);
		uint32_t stride_bytes = sizeof(vmfloat3);
		DXGI_FORMAT vtxbuf_format = DXGI_FORMAT_R32_FLOAT;
		switch (i)
		{
		case VERTEX_DEFINITIONS::TEXCOORD0:
			vtxbuf_format = DXGI_FORMAT_R16G16_UNORM;
			stride_bytes = 4;
			break;
		case VERTEX_DEFINITIONS::COLOR:
			vtxbuf_format = DXGI_FORMAT_R8G8B8A8_UNORM;
			stride_bytes = 4;
			break;
		case VERTEX_DEFINITIONS::GEODIST:
			vtxbuf_format = DXGI_FORMAT_R32_FLOAT;
			stride_bytes = 4;
			break;
		default:
			break;
		}

		if (!g_pCGpuManager->UpdateGpuResource(gres_vtx))
		{
			gres_vtx.options["USAGE"] = D3D11_USAGE_DEFAULT;
			gres_vtx.options["FORMAT"] = vtxbuf_format;

			gres_vtx.rtype = RTYPE_BUFFER;
			gres_vtx.options["CPU_ACCESS_FLAG"] = NULL;
			gres_vtx.options["BIND_FLAG"] = D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_SHADER_RESOURCE;
			//gres_pos.options["FORMAT"] = DXGI_FORMAT_R32G32B32_FLOAT; // buffer does not need the specific format except UNKNOWN (used for STRUCTURED buffer)
			gres_vtx.res_values.SetParam("NUM_ELEMENTS", (uint32_t)prim_data->num_vtx);
			gres_vtx.res_values.SetParam("STRIDE_BYTES", (uint32_t)stride_bytes);

			update_data_attriute = true;
			g_pCGpuManager->GenerateGpuResource(gres_vtx);
		}
		else
		{
			vmobjects::VmParamMap<std::string, std::any> res_new_values;
			res_new_values.SetParam("NUM_ELEMENTS", (uint32_t)prim_data->num_vtx);
			res_new_values.SetParam("STRIDE_BYTES", (uint32_t)stride_bytes);
			bool regen_data = false;
			CheckReusability(gres_vtx, pobj, update_data_attriute, regen_data, res_new_values);

			if (i == (int)VERTEX_DEFINITIONS::GEODIST)
			{
				uint64_t cpu_update_time = pobj->GetObjParam("_ullong_Latest_GeodesicField", (uint64_t)0);
				uint64_t gpu_gen_timg = gres_vtx.res_values.GetParam("LAST_UPDATE_TIME", (uint64_t)0);
				update_data_attriute |= cpu_update_time > gpu_gen_timg;
			}

			if(regen_data)
				g_pCGpuManager->GenerateGpuResource(gres_vtx);
		}

		if (pobj->GetObjParam(std::string(vtx_def_names[i]) + "_UPDATED", false))
		{
			update_data_attriute = true;
			pobj->SetObjParam(std::string(vtx_def_names[i]) + "_UPDATED", false);
		}
		if (update_data_attriute)
		{
			uint8_t* buffer_vertices = nullptr;

			D3D11_SUBRESOURCE_DATA subres;
			if (i == (int)VERTEX_DEFINITIONS::TEXCOORD0)
			{
				buffer_vertices = new uint8_t[stride_bytes * prim_data->num_vtx];

				const vmfloat2* vtx_uv = (vmfloat2*)vtx_buf;
				vmushort2* uv_buf = (vmushort2*)buffer_vertices;
				for (int j = 0; j < prim_data->num_vtx; ++j)
				{
					vmfloat2 uv = glm::clamp(vtx_uv[j], 0.f, 1.f);
					uv = glm::round(uv * 65535.f);
					uv_buf[j] = vmushort2(uv.x, uv.y);
				}
				subres.pSysMem = buffer_vertices;
			}
			else
			{
				subres.pSysMem = vtx_buf;
			}

			subres.SysMemPitch = stride_bytes * prim_data->num_vtx;
			subres.SysMemSlicePitch = 0; // only for 3D resource
			
			ID3D11Buffer* pdx11bufvtx = (ID3D11Buffer*)gres_vtx.alloc_res_ptrs[DTYPE_RES];
			g_psoManager->dx11DeviceImmContext->UpdateSubresource(pdx11bufvtx, 0, NULL, subres.pSysMem, subres.SysMemPitch, 0);

			VMSAFE_DELETEARRAY(buffer_vertices);

			gres_vtx.options["Update LAST_UPDATE_TIME"] = 1u;
			g_pCGpuManager->UpdateGpuResource(gres_vtx);

			if (DEBUG_GPU_UPDATE_DATA) vzlog("DEBUG_GPU_UPDATE_DATA: %s", gres_vtx.res_name.c_str());
		}
	}
	if (update_data)
	{
		pobj->SetObjParam("_bool_UpdateData", false);
	}

	if (prim_data->vidx_buffer && prim_data->num_vidx > 0)
	{
		bool update_data_attriute = update_data;

		gres_idx.vm_src_id = pobj->GetObjectID();
		gres_idx.res_name = string("PRIMITIVE_MODEL_IDX");
		if (!g_pCGpuManager->UpdateGpuResource(gres_idx))
		{
			gres_idx.rtype = RTYPE_BUFFER;
			gres_idx.options["USAGE"] = D3D11_USAGE_DEFAULT;
			gres_idx.options["CPU_ACCESS_FLAG"] = NULL;
			gres_idx.options["BIND_FLAG"] = D3D11_BIND_INDEX_BUFFER | D3D11_BIND_SHADER_RESOURCE;
			gres_idx.options["FORMAT"] = DXGI_FORMAT_R32_UINT;
			gres_idx.res_values.SetParam("NUM_ELEMENTS", (uint32_t)prim_data->num_vidx);
			gres_idx.res_values.SetParam("STRIDE_BYTES", (uint32_t)sizeof(uint32_t));
			
			update_data_attriute = true;
			g_pCGpuManager->GenerateGpuResource(gres_idx);
		}
		else
		{
			vmobjects::VmParamMap<std::string, std::any> res_new_values;
			res_new_values.SetParam("NUM_ELEMENTS", (uint32_t)prim_data->num_vidx);
			bool regen_data = false;
			CheckReusability(gres_idx, pobj, update_data_attriute, regen_data, res_new_values);
			if(regen_data)
				g_pCGpuManager->GenerateGpuResource(gres_idx);
		}

		if (update_data_attriute)
		{
			ID3D11Buffer* pdx11bufidx = (ID3D11Buffer*)gres_idx.alloc_res_ptrs[DTYPE_RES];
			{
				D3D11_SUBRESOURCE_DATA subres;
				subres.pSysMem = new uint32_t[prim_data->num_vidx];
				subres.SysMemPitch = prim_data->num_vidx * sizeof(uint32_t);
				subres.SysMemSlicePitch = 0; // only for 3D resource
				memcpy((void*)subres.pSysMem, prim_data->vidx_buffer, prim_data->num_vidx * sizeof(uint32_t));
				g_psoManager->dx11DeviceImmContext->UpdateSubresource(pdx11bufidx, 0, NULL, subres.pSysMem, subres.SysMemPitch, 0);
				VMSAFE_DELETEARRAY_VOID(subres.pSysMem);
			}

			gres_idx.options["Update LAST_UPDATE_TIME"] = 1u;
			g_pCGpuManager->UpdateGpuResource(gres_idx);

			if (DEBUG_GPU_UPDATE_DATA) vzlog("DEBUG_GPU_UPDATE_DATA: %s", gres_idx.res_name.c_str());
		}
	}

	bool has_texture_img = false;
	if (prim_data->GetVerticeDefinition<vmfloat2>("TEXCOORD0")) {
		bool update_data_attriute = update_data;
		if (imgObj) {
			MapTable* imgBuffer = imgObj->GetObjParamPtr<MapTable>("_TableMap_ImageBuffer");
			has_texture_img = true;

			vmfloat2* pp = prim_data->GetVerticeDefinition<vmfloat2>("TEXCOORD0");

			GpuRes gres_tex;
			gres_tex.vm_src_id = imgObj->GetObjectID();
			gres_tex.res_name = string("TEXTURE_COLOR4");

			int imgWidth = imgObj->GetObjParam("IMG_WIDTH", (int)0);
			int imgHeight = imgObj->GetObjParam("IMG_HEIGHT", (int)0);
			int imgStride = imgObj->GetObjParam("IMG_STRIDE", (int)0);

			bool regen = !g_pCGpuManager->UpdateGpuResource(gres_tex);
			if (!regen) {
				uint32_t prevW = gres_tex.res_values.GetParam("WIDTH", (uint32_t)0);
				uint32_t prevH = gres_tex.res_values.GetParam("HEIGHT", (uint32_t)0);
				if (prevW != imgWidth || prevH != imgHeight) {
					g_pCGpuManager->ReleaseGpuResource(gres_tex, false);
					regen = true;
				}
			}

			if (regen)
			{
				gres_tex.rtype = RTYPE_TEXTURE2D;
				gres_tex.options["USAGE"] = D3D11_USAGE_DEFAULT;
				gres_tex.options["CPU_ACCESS_FLAG"] = NULL;// D3D11_CPU_ACCESS_WRITE;
				// D3D11_RESOURCE_MISC_GENERATE_MIPS requires that the D3D11_BIND_RENDER_TARGET & D3D11_BIND_SHADER_RESOURCE flag be set.
				gres_tex.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
				gres_tex.options["FORMAT"] = DXGI_FORMAT_R8G8B8A8_UNORM;
				gres_tex.options["MIP_GEN"] = 1;
				gres_tex.res_values.SetParam("WIDTH", (uint32_t)imgWidth);
				gres_tex.res_values.SetParam("HEIGHT", (uint32_t)imgHeight);
				gres_tex.res_values.SetParam("DEPTH", (uint32_t)1);

				update_data_attriute = true;
				g_pCGpuManager->GenerateGpuResource(gres_tex);
			}
			else
			{
				vmobjects::VmParamMap<std::string, std::any> res_new_values;
				res_new_values.SetParam("WIDTH", (uint32_t)imgWidth);
				res_new_values.SetParam("HEIGHT", (uint32_t)imgHeight);
				res_new_values.SetParam("DEPTH", (uint32_t)1);
				bool regen_data = false;
				CheckReusability(gres_tex, imgObj, update_data_attriute, regen_data, res_new_values);
				if (regen_data)
					g_pCGpuManager->GenerateGpuResource(gres_tex);
			}

			if (update_data_attriute) {
				//upload_teximg(gres_tex, vmint2(imgWidth, imgHeight), imgStride);
				ID3D11Texture2D* pdx11tx2dres = (ID3D11Texture2D*)gres_tex.alloc_res_ptrs[DTYPE_RES];

				D3D11_SUBRESOURCE_DATA subres;
				int byte_stride_gpu = imgStride == 1 ? 1 : 4;
				subres.pSysMem = new byte[imgWidth * imgHeight * byte_stride_gpu];
				subres.SysMemPitch = (imgWidth * byte_stride_gpu); 
				subres.SysMemSlicePitch = imgWidth * imgHeight * byte_stride_gpu; // only for 3D resource

				byte* tx_subres = (byte*)subres.pSysMem;

				byte* texture_res = (byte*)imgBuffer->tmap_buffers[0];
				vmint2 tex_res_size = vmint2(imgWidth, imgHeight);

				for (int h = 0; h < tex_res_size.y; h++)
					for (int x = 0; x < tex_res_size.x; x++)
					{
						for (int i = 0; i < imgStride; i++)
							tx_subres[byte_stride_gpu * (x + h * tex_res_size.x) + i] = texture_res[imgStride * (x + h * tex_res_size.x) + i];
						for (int i = imgStride; i < byte_stride_gpu; i++)
							tx_subres[byte_stride_gpu * (x + h * tex_res_size.x) + i] = 255;
					}
				g_psoManager->dx11DeviceImmContext->UpdateSubresource(pdx11tx2dres, 0, NULL, subres.pSysMem, subres.SysMemPitch, subres.SysMemSlicePitch);
				ID3D11ShaderResourceView* pSRV = (ID3D11ShaderResourceView*)gres_tex.alloc_res_ptrs[DTYPE_SRV];
				g_psoManager->dx11DeviceImmContext->GenerateMips(pSRV);
				VMSAFE_DELETEARRAY_VOID(subres.pSysMem);

				gres_tex.options["Update LAST_UPDATE_TIME"] = 1u;
				g_pCGpuManager->UpdateGpuResource(gres_tex);

				if (DEBUG_GPU_UPDATE_DATA) vzlog("DEBUG_GPU_UPDATE_DATA: %s", gres_tex.res_name.c_str());
			}

			map_gres_texs["MAP_COLOR4"] = gres_tex;
		}
		else if (prim_data->texture_res_info.size() > 0)
		{
			has_texture_img = true;

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
							for (int i = 0; i < byte_stride; i++)
								tx_subres[byte_stride_gpu * (x + h * tex_res_size.x) + index * byte_stride_gpu * tex_res_size.x * tex_res_size.y + i] = texture_res[byte_stride * (x + h * tex_res_size.x) + i];
							for (int i = byte_stride; i < byte_stride_gpu; i++)
								tx_subres[byte_stride_gpu * (x + h * tex_res_size.x) + index * byte_stride_gpu * tex_res_size.x * tex_res_size.y + i] = 255;
						}
				}

				g_psoManager->dx11DeviceImmContext->UpdateSubresource(pdx11tx2dres, 0, NULL, subres.pSysMem, subres.SysMemPitch, subres.SysMemSlicePitch);
				ID3D11ShaderResourceView* pSRV = (ID3D11ShaderResourceView*)gres_tex.alloc_res_ptrs[DTYPE_SRV];
				g_psoManager->dx11DeviceImmContext->GenerateMips(pSRV);
				VMSAFE_DELETEARRAY_VOID(subres.pSysMem);

				gres_tex.options["Update LAST_UPDATE_TIME"] = 1u;
				g_pCGpuManager->UpdateGpuResource(gres_tex);

				if (DEBUG_GPU_UPDATE_DATA) vzlog("DEBUG_GPU_UPDATE_DATA: %s", gres_tex.res_name.c_str());
			};

			if (prim_data->GetTexureInfo("MAP_COLOR4", tex_res_size.x, tex_res_size.y, byte_stride, &texture_res) || prim_data->GetTexureInfo("PLY_TEX_MAP_0", tex_res_size.x, tex_res_size.y, byte_stride, &texture_res))
			{
				// text in a texture array
				// cmm case

				GpuRes gres_tex;
				gres_tex.vm_src_id = pobj->GetObjectID();
				gres_tex.res_name = string("TEXTURE_COLOR4");

				if (!g_pCGpuManager->UpdateGpuResource(gres_tex))
				{
					gres_tex.rtype = RTYPE_TEXTURE2D;
					gres_tex.options["USAGE"] = D3D11_USAGE_DEFAULT;
					gres_tex.options["CPU_ACCESS_FLAG"] = NULL;// D3D11_CPU_ACCESS_WRITE;
					// D3D11_RESOURCE_MISC_GENERATE_MIPS requires that the D3D11_BIND_RENDER_TARGET & D3D11_BIND_SHADER_RESOURCE flag be set.
					gres_tex.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
					gres_tex.options["FORMAT"] = DXGI_FORMAT_R8G8B8A8_UNORM;
					gres_tex.options["MIP_GEN"] = 1;
					gres_tex.res_values.SetParam("WIDTH", (uint32_t)tex_res_size.x);
					gres_tex.res_values.SetParam("HEIGHT", (uint32_t)tex_res_size.y);
					gres_tex.res_values.SetParam("DEPTH", (uint32_t)prim_data->texture_res_info.size());

					update_data_attriute = true;
					g_pCGpuManager->GenerateGpuResource(gres_tex);
				}
				else
				{
					vmobjects::VmParamMap<std::string, std::any> res_new_values;
					res_new_values.SetParam("WIDTH", (uint32_t)tex_res_size.x);
					res_new_values.SetParam("HEIGHT", (uint32_t)tex_res_size.y);
					res_new_values.SetParam("DEPTH", (uint32_t)prim_data->texture_res_info.size());
					bool regen_data = false;
					CheckReusability(gres_tex, pobj, update_data_attriute, regen_data, res_new_values);
					if (regen_data)
						g_pCGpuManager->GenerateGpuResource(gres_tex);
				}

				if (update_data_attriute)
					upload_teximg(gres_tex, tex_res_size, byte_stride);

				map_gres_texs["MAP_COLOR4"] = gres_tex;
			}
			else
			{
				double Ns = pobj->GetObjParam("_float_Ns", (double)0);

				auto update_tex_res = [&Ns, &update_data_attriute, &pobj](GpuRes& gres_tex, const string& mat_name, const vmint2& tex_res_size, const int byte_stride, const byte* texture_res)
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
						g_psoManager->dx11DeviceImmContext->UpdateSubresource(pdx11tx2dres, 0, NULL, subres.pSysMem, subres.SysMemPitch, subres.SysMemSlicePitch);
						ID3D11ShaderResourceView* pSRV = (ID3D11ShaderResourceView*)gres_tex.alloc_res_ptrs[DTYPE_SRV];
						g_psoManager->dx11DeviceImmContext->GenerateMips(pSRV); 
						VMSAFE_DELETEARRAY_VOID(subres.pSysMem);

						gres_tex.options["Update LAST_UPDATE_TIME"] = 1u;
						g_pCGpuManager->UpdateGpuResource(gres_tex);

						if (DEBUG_GPU_UPDATE_DATA) vzlog("DEBUG_GPU_UPDATE_DATA: %s", gres_tex.res_name.c_str());
					};

					gres_tex.res_name = string("PRIMITIVE_MODEL_") + mat_name;
					if (!g_pCGpuManager->UpdateGpuResource(gres_tex))
					{
						gres_tex.rtype = RTYPE_TEXTURE2D;
						gres_tex.options["USAGE"] = D3D11_USAGE_DEFAULT;
						gres_tex.options["CPU_ACCESS_FLAG"] = NULL;
						// D3D11_RESOURCE_MISC_GENERATE_MIPS requires that the D3D11_BIND_RENDER_TARGET & D3D11_BIND_SHADER_RESOURCE flag be set.
						gres_tex.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
						assert(byte_stride != 2);
						gres_tex.options["FORMAT"] = byte_stride == 1 ? DXGI_FORMAT_R8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;
						gres_tex.res_values.SetParam("WIDTH", (uint32_t)tex_res_size.x);
						gres_tex.res_values.SetParam("HEIGHT", (uint32_t)tex_res_size.y);
						gres_tex.options["MIP_GEN"] = 1;

						update_data_attriute = true;
						g_pCGpuManager->GenerateGpuResource(gres_tex);
					}
					else
					{
						vmobjects::VmParamMap<std::string, std::any> res_new_values;
						res_new_values.SetParam("WIDTH", (uint32_t)tex_res_size.x);
						res_new_values.SetParam("HEIGHT", (uint32_t)tex_res_size.y);
						bool regen_data = false;
						CheckReusability(gres_tex, pobj, update_data_attriute, regen_data, res_new_values);
						if (regen_data)
							g_pCGpuManager->GenerateGpuResource(gres_tex);
					}

					if (update_data_attriute)
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
		}
	}

	if (hasTextureMap) *hasTextureMap = has_texture_img;

#ifndef DX10_0
	// BVH
	GpuRes gres_bvhNodeBuffer;
	gres_bvhNodeBuffer.vm_src_id = pobj->GetObjectID();
	gres_bvhNodeBuffer.res_name = string("GPUBVH::BVHNodeBuffer"); 
	g_pCGpuManager->UpdateGpuResource(gres_bvhNodeBuffer);
	uint64_t _gpu_gen_timg = gres_bvhNodeBuffer.res_values.GetParam("LAST_UPDATE_TIME", (uint64_t)0);
	uint64_t _cpu_gen_timg = pobj->GetContentUpdateTime();
	const geometrics::BVH& bvh2 = ((VmVObjectPrimitive*)pobj)->GetBVH();
	if (bvh2.IsValid() && prim_data->ptype == EvmPrimitiveType::PrimitiveTypeTRIANGLE && _gpu_gen_timg < _cpu_gen_timg)
	{
		bvh::UpdateGeometryGPUBVH(g_pCGpuManager, g_psoManager, pobj);
	}
#endif

	return true;
}

bool grd_helper::UpdateFrameBuffer(GpuRes& gres,
	const VmIObject* iobj,
	const string& res_name,
	const GpuResType gres_type,
	const uint32_t bind_flag,
	const uint32_t dx_format,
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
	uint32_t stride_bytes = 0;
	switch (gres_type)
	{
	case RTYPE_BUFFER:
		if (fb_flag & UPFB_NFPP_BUFFERSIZE) gres.res_values.SetParam("NUM_ELEMENTS", (uint32_t)num_frags_perpixel);
		else gres.res_values.SetParam("NUM_ELEMENTS", (uint32_t)(num_frags_perpixel > 0 ? fb_size.x * fb_size.y * num_frags_perpixel : fb_size.x * fb_size.y));
		switch (dx_format)
		{
		case DXGI_FORMAT_R32_UINT: stride_bytes = sizeof(uint32_t); break;
		case DXGI_FORMAT_R32_FLOAT: stride_bytes = sizeof(float); break;
		case DXGI_FORMAT_D32_FLOAT:	stride_bytes = sizeof(float); break;
		case DXGI_FORMAT_R32G32B32A32_FLOAT: stride_bytes = sizeof(vmfloat4); break;
		case DXGI_FORMAT_R8G8B8A8_UNORM: stride_bytes = sizeof(vmbyte4); break;
		case DXGI_FORMAT_R8_UNORM: stride_bytes = sizeof(byte); break;
		case DXGI_FORMAT_R8_UINT: stride_bytes = sizeof(byte); break;
		case DXGI_FORMAT_R16_UNORM: stride_bytes = sizeof(uint16_t); break;
		case DXGI_FORMAT_R16G16_UNORM: stride_bytes = sizeof(vmushort2); break;
		case DXGI_FORMAT_R16_UINT: stride_bytes = sizeof(uint16_t); break;
		case DXGI_FORMAT_R16G16_UINT: stride_bytes = sizeof(vmushort2); break;
		case DXGI_FORMAT_R32G32B32A32_UINT: stride_bytes = sizeof(vmuint4); break;
		case DXGI_FORMAT_UNKNOWN: stride_bytes = structured_stride; break;
		case DXGI_FORMAT_R32_TYPELESS: stride_bytes = sizeof(uint32_t); break;
		default:
			return false;
		}
		if (fb_flag & UPFB_RAWBYTE) gres.options["RAW_ACCESS"] = 1;
		if (stride_bytes == 0) return false;
		gres.res_values.SetParam("STRIDE_BYTES", (uint32_t)stride_bytes);
		break;
	case RTYPE_TEXTURE2D:
		if (fb_flag & UPFB_PICK_TEXTURE)
		{
			gres.res_values.SetParam("WIDTH", 1u);
			gres.res_values.SetParam("HEIGHT", 1u);
		}
		else
		{
			gres.res_values.SetParam("WIDTH", (uint32_t)(((fb_flag & UPFB_HALF) || (fb_flag & UPFB_HALF_W)) ? fb_size.x / 2 : fb_size.x));
			gres.res_values.SetParam("HEIGHT", (uint32_t)(((fb_flag & UPFB_HALF) || (fb_flag & UPFB_HALF_H)) ? fb_size.y / 2 : fb_size.y));
		}
		if (fb_flag & UPFB_NFPP_TEXTURESTACK) gres.res_values.SetParam("DEPTH", (uint32_t)num_frags_perpixel);
		if (fb_flag & UPFB_MIPMAP) gres.options["MIP_GEN"] = 1;
		if (fb_flag & UPFB_HALF) gres.options["HALF_GEN"] = 1;
		break;
	default:
		vmlog::LogErr("Not supported Data Type");
		return false;
	}

	g_pCGpuManager->GenerateGpuResource(gres);

	return true;
}

bool grd_helper::UpdateCustomBuffer(GpuRes& gres, VmObject* srcObj, const string& resName, const void* bufPtr, const int numElements, DXGI_FORMAT dxFormat, const int type_bytes, LocalProgress* progress, uint64_t cpu_update_custom_time)
{
	gres.vm_src_id = srcObj->GetObjectID();
	gres.res_name = resName;

	bool needRegen = true;
	if (g_pCGpuManager->UpdateGpuResource(gres)) {

		uint32_t nemElementPrev = gres.res_values.GetParam("NUM_ELEMENTS", (uint32_t)0);
		uint32_t typeBytesPrev = gres.res_values.GetParam("STRIDE_BYTES", (uint32_t)0);

		needRegen = (nemElementPrev * typeBytesPrev) != ((uint32_t)numElements * (uint32_t)type_bytes);

		uint64_t _tp_cpu = cpu_update_custom_time == 0 ? srcObj->GetContentUpdateTime() : cpu_update_custom_time;
		uint64_t _tp_gpu = gres.res_values.GetParam("LAST_UPDATE_TIME", (uint64_t)0);
		if (_tp_gpu >= _tp_cpu) {
			assert(!needRegen);
			return true;
		}
	}

	if (needRegen) {
		gres.rtype = RTYPE_BUFFER;
		gres.options["USAGE"] = D3D11_USAGE_DYNAMIC;
		gres.options["CPU_ACCESS_FLAG"] = D3D11_CPU_ACCESS_WRITE;
		gres.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE;
		gres.options["FORMAT"] = dxFormat;// DXGI_FORMAT_R8G8B8A8_UNORM;

		gres.res_values.SetParam("NUM_ELEMENTS", (const uint32_t)numElements);
		gres.res_values.SetParam("STRIDE_BYTES", (const uint32_t)type_bytes);

		// including safe-delete to avoid redundant gen
		g_pCGpuManager->GenerateGpuResource(gres);
	}

	D3D11_MAPPED_SUBRESOURCE d11MappedRes;
	g_psoManager->dx11DeviceImmContext->Map((ID3D11Resource*)gres.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes);
	memcpy(d11MappedRes.pData, bufPtr, type_bytes * numElements);
	g_psoManager->dx11DeviceImmContext->Unmap((ID3D11Resource*)gres.alloc_res_ptrs[DTYPE_RES], 0);

	gres.options["Update LAST_UPDATE_TIME"] = 1u;
	g_pCGpuManager->UpdateGpuResource(gres);

	return true;
}

bool grd_helper::UpdatePaintTexture(VmActor* actor, const vmmat44f& matSS2WS, VmCObject* camObj, const vmfloat2& paint_pos2d_ss, const BrushParams& brushParams)
{
	static PaintResourceManager* manager = meshPainter->getPaintResourceManager();

	if (!actor || !meshPainter)
		return false;
	VmVObjectPrimitive* pobj = (VmVObjectPrimitive*)actor->GetGeometryRes();
	PrimitiveData* prim_data = pobj->GetPrimitiveData();
	if (!prim_data) 
		return false;
	if (prim_data->idx_stride != 3) 
		return false;

	std::string painter_mode = actor->GetParam("_string_PainterMode", std::string("NONE"));
	ActorPaintData* paintRes = manager->getPaintResource(actor->actorId);
	if (paintRes && (paint_pos2d_ss.x < 0 || paint_pos2d_ss.y < 0))
	{
		return false;
	}

	if (painter_mode == "NONE" || paint_pos2d_ss.x < 0 || paint_pos2d_ss.y < 0)
	{
		if (paintRes)
		{
			ID3D11ShaderResourceView* paintTex2DSRV = paintRes->paintTexture->getRenderTarget()->srv.Get();
			g_psoManager->dx11DeviceImmContext->PSSetShaderResources(60, 1, &paintTex2DSRV); // t60
			return true;
		}
		return false;
	}

	bool is_paint = false;
	bool clean_paint = false;
	if (painter_mode == "PAINT")
	{
		is_paint = true;
	}
	else if (painter_mode == "HOVER")
	{
		is_paint = false;
	}
	else if (painter_mode == "CLEAN")
	{
		clean_paint = true;
	}
	else
	{
		vzlog_error("Invalid Painter Mode! %s", painter_mode.c_str());
		return false;
	}

	// Check if mesh is indexed
	// Indexed mesh: num_vtx < num_prims * 3 (vertices are shared between triangles)
	bool is_indexed = (prim_data->vidx_buffer != nullptr) && (prim_data->num_vtx != prim_data->num_prims * 3);

	bool is_regen_res = true;
	if (paintRes)
	{
		is_regen_res = pobj->GetContentUpdateTime() > paintRes->timeStamp;
	}

	if (is_regen_res)
	{
		vmfloat2* vb_texcoord = prim_data->GetVerticeDefinition<vmfloat2>("TEXCOORD0");

		if (vb_texcoord == nullptr) {
			vzlog_error("No TEXCOORD0 found, generating UVs with UnwrapUVs");
			return false;
		}

		// Calculate texture size based on triangle count
		// Target: ~16-20 pixels per triangle average for reliable rasterization
		int textureSize = 1024;
		float avgPixelsPerTri = (textureSize * textureSize) / (float)prim_data->num_prims;

		if (avgPixelsPerTri < 12.0f) {
			textureSize = 2048;  // Use higher resolution for dense meshes
			vzlog("Using 2048x2048 paint texture for dense mesh (%d triangles, %.1f pixels/tri avg)",
				prim_data->num_prims, (textureSize * textureSize) / (float)prim_data->num_prims);
		}

		// MeshPainter uses TRIANGLE-VERTEX indexed UVs only (no conversion to vertex-indexed)
		// This avoids UV seam issues where shared vertices need different UVs in different charts
		// The shader will use triangle-vertex indexing: uvIndex = (triangleID * 3 + vertexInTriangle)
		paintRes = manager->createPaintResource(actor->actorId, textureSize, textureSize);
		paintRes->timeStamp = vmhelpers::GetCurrentTimePack();
		pobj->SetObjParam("Painter TimeStamp", paintRes->timeStamp);
	}

	// now paintRes refers to the valid resource pointer

	map<string, GpuRes> gres_vtxs;
	map<string, GpuRes> gres_texs;
	GpuRes gres_ib;
	UpdatePrimitiveModel(gres_vtxs, gres_ib, gres_texs, pobj, nullptr);
	
	vmmat44f matPivot = (actor->GetParam("_matrix44f_Pivot", vmmat44f(1)));
	vmmat44f matRS2WS = matPivot * actor->matOS2WS;
	const geometrics::BVH& bvh = pobj->GetBVH();

	vmfloat3 ray_origin, ray_dir;
	camObj->GetCameraExtStatef(&ray_origin, &ray_dir, NULL);
	vmfloat3 paint_pos_ws, paint_pos_ss(paint_pos2d_ss.x, paint_pos2d_ss.y, 0);
	vmmath::fTransformPoint(&paint_pos_ws, &paint_pos_ss, &matSS2WS);

	if (camObj->IsPerspective()) {
		ray_dir = paint_pos_ws - ray_origin;
		vmmath::fNormalizeVector(&ray_dir, &ray_dir);
	}
	else {
		ray_origin = paint_pos_ws;
	}
	RayHitResult hit = meshPainter->raycastMesh(prim_data, matRS2WS, ray_origin, ray_dir, &bvh, actor->actorId);
	if (!hit.hit)
		return false;

	BrushParams brush = brushParams;
	memcpy(brush.position, hit.worldPos, sizeof(float) * 3);
	//brush.position[0] = hit.worldPos[0];
	//brush.position[1] = hit.worldPos[1];
	//brush.position[2] = hit.worldPos[2];

	if (is_paint)
	{
		brush.blendMode = PaintBlendMode::ADDITIVE;
	}

	MeshParams mesh_params;
	mesh_params.primData = prim_data;
	mesh_params.vbPos = (ID3D11Buffer*)gres_vtxs["POSITION"].alloc_res_ptrs[DTYPE_RES];
	mesh_params.vbUV = (ID3D11Buffer*)gres_vtxs["TEXCOORD0"].alloc_res_ptrs[DTYPE_RES];
	mesh_params.indexBuffer = (is_indexed && gres_ib.alloc_res_ptrs.size() > 0)
		? (ID3D11Buffer*)gres_ib.alloc_res_ptrs[DTYPE_RES]
		: nullptr;

	float clr_float_zero_4[4] = { 0, 0, 0, 0 };
	if (clean_paint)
	{
		g_psoManager->dx11DeviceImmContext->ClearRenderTargetView(paintRes->paintTexture->getRenderTarget()->rtv.Get(), clr_float_zero_4);
		g_psoManager->dx11DeviceImmContext->ClearRenderTargetView(paintRes->paintTexture->getOffRenderTarget()->rtv.Get(), clr_float_zero_4);
	}

	g_psoManager->dx11DeviceImmContext->CopyResource(
		paintRes->hoverTexture->getRenderTarget()->texture.Get(),
		paintRes->paintTexture->getRenderTarget()->texture.Get()
	);
	g_psoManager->dx11DeviceImmContext->CopyResource(
		paintRes->hoverTexture->getOffRenderTarget()->texture.Get(),
		paintRes->paintTexture->getOffRenderTarget()->texture.Get()
	);

	ID3D11ShaderResourceView* hoverTex2DSRV = paintRes->hoverTexture->getRenderTarget()->srv.Get();
	ID3D11ShaderResourceView* paintTex2DSRV = paintRes->paintTexture->getRenderTarget()->srv.Get();

	meshPainter->paintOnActor(actor->actorId, mesh_params, matRS2WS, brush, hit, is_paint);

	vzlog_assert (manager->hasPaintResource(actor->actorId), "No painter resource!");


	if (!hoverTex2DSRV || !paintTex2DSRV)
	{
		vzlog_error("hoverTex2DSRV and paintUvSRV must be valid!");
		return false;
	}
	//g_psoManager->dx11DeviceImmContext->PSSetShaderResources(60, 1, is_paint? &paintTex2DSRV : &hoverTex2DSRV); // t60

	return true;
}

void grd_helper::SetCb_Camera(CB_CameraState& cb_cam, const vmmat44f& matWS2SS, const vmmat44f& matSS2WS, const vmmat44f& matWS2CS, const vmmat44f& matWS2PS, VmCObject* ccobj, const vmint2& fb_size, const int k_value, const float vz_thickness)
{
	cb_cam.mat_ss2ws = TRANSPOSE(matSS2WS);
	cb_cam.mat_ws2ss = TRANSPOSE(matWS2SS);
	cb_cam.mat_ws2cs = TRANSPOSE(matWS2CS);

	// Reverse Z: remap z from [0,1] to [1,0] by transforming column 2
	// P_revZ[2] = P[3] - P[2], so z_ndc = (w - z_standard) / w = 1 - z_standard/w
	vmmat44f matWS2PS_revZ = matWS2PS;
	matWS2PS_revZ[2] = vmfloat4(
		matWS2PS[3].x - matWS2PS[2].x,
		matWS2PS[3].y - matWS2PS[2].y,
		matWS2PS[3].z - matWS2PS[2].z,
		matWS2PS[3].w - matWS2PS[2].w);
	cb_cam.mat_ws2ps_revZ = TRANSPOSE(matWS2PS_revZ);

	vmfloat3 pos_cam, dir_cam;
	ccobj->GetCameraExtStatef(&pos_cam, &dir_cam, NULL);

	cb_cam.cam_flag = 0;
	if (ccobj->IsPerspective())
		cb_cam.cam_flag |= 0x1;

	cb_cam.pos_cam_ws = pos_cam;
	cb_cam.dir_view_ws = dir_cam;

	cb_cam.rt_width = (uint32_t)fb_size.x;
	cb_cam.rt_height = (uint32_t)fb_size.y;

	cb_cam.k_value = k_value;
	cb_cam.cam_vz_thickness = vz_thickness;

	double np, fp;
	if (ccobj->IsArIntrinsics())
		ccobj->GetCameraIntStateAR(NULL, NULL, NULL, NULL, NULL, &np, &fp);
	else
		ccobj->GetCameraIntState(NULL, &np, &fp, NULL);

	cb_cam.near_plane = (float)np;
	cb_cam.far_plane = (float)fp;
}

void grd_helper::SetCb_Env(CB_EnvState& cb_env, VmCObject* ccobj, const LightSource& light_src, const GlobalLighting& global_lighting, const LensEffect& lens_effect)
{
	vmfloat3 pos_cam, dir_cam;
	ccobj->GetCameraExtStatef(&pos_cam, &dir_cam, NULL);

	if (light_src.is_on_camera) 
	{
		cb_env.dir_light_ws = dir_cam;
		cb_env.pos_light_ws = pos_cam;

		//vmlog::LogInfo(std::to_string(dir_cam.x) + ", " + std::to_string(dir_cam.y) + ", " + std::to_string(dir_cam.z));
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

void grd_helper::SetCb_VolumeObj(CB_VolumeObject& cb_volume, VmVObjectVolume* vobj, const vmmat44f& matWS2OS, GpuRes& gresVol, const int iso_value, const float volblk_valuerange, const float sample_precision, const bool is_xraymode, const int sculpt_index)
{
	VolumeData* vol_data = vobj->GetVolumeData();

	//bool invert_normal = false;
	//vobj->GetCustomParameter("_bool_IsInvertPlaneDirection", data_type::dtype<bool>(), &invert_normal);
	//if (invert_normal)
	//	cb_volume.vobj_flag |= (0x1);
	if (sculpt_index >= 0)
		cb_volume.vobj_flag |= sculpt_index << 24;

	vmmat44f matWS2VS = matWS2OS * vobj->GetMatrixOS2RSf();

	vmmat44f matVS2TS, matScale;
	//vmmat44f matShift;
	//fMatrixTranslation(&matShift, &vmfloat3(0.5f));
	//fMatrixScaling(&matScale, &vmfloat3(1.f / (float)(vol_data->vol_size.x + vol_data->vox_pitch.x * 0.00f),
	//	1.f / (float)(vol_data->vol_size.y + vol_data->vox_pitch.z * 0.00f), 1.f / (float)(vol_data->vol_size.z + vol_data->vox_pitch.z * 0.00f)));
	//matVS2TS = matShift * matScale;
	//fMatrixTranslation(&matShift, &vmfloat3(0.5f));

	fMatrixScaling(&matScale, &vmfloat3(1.f / (float)max(vol_data->vol_size.x - 1, 1),
		1.f / (float)max(vol_data->vol_size.y - 1, 1), 
		1.f / (float)max(vol_data->vol_size.z - 1, 1)));
	matVS2TS = matScale;

	vmmat44f mat_ws2ts = matWS2VS * matVS2TS;
	cb_volume.mat_ws2ts = TRANSPOSE(mat_ws2ts);

	AaBbMinMax aabb_os;
	vobj->GetOrthoBoundingBox(aabb_os);
	vmmat44f mat_s;
	vmfloat3 aabb_size = aabb_os.pos_max - aabb_os.pos_min;
	fMatrixScaling(&mat_s, &vmfloat3(1. / aabb_size.x, 1. / aabb_size.y, 1. / aabb_size.z));

	//vmmat44f mat_t;
	//fMatrixScaling(&mat_t, &vmfloat3(-0.5f, -0.5f, -0.5f));

	vmmat44f matWS2BS = matWS2OS * mat_s;
	cb_volume.mat_alignedvbox_tr_ws2bs = TRANSPOSE(matWS2BS);

	if (vol_data->store_dtype.type_bytes == data_type::dtype<uint8_t>().type_bytes) // char
		cb_volume.value_range = 255.f;
	else if (vol_data->store_dtype.type_bytes == data_type::dtype<uint16_t>().type_bytes) // short
		cb_volume.value_range = 65535.f;
	else VMERRORMESSAGE("UNSUPPORTED FORMAT : grd_helper::SetCb_VolumeObj");

	vmfloat3 sampleOffset = vmfloat3(gresVol.res_values.GetParam("SAMPLE_OFFSET_X", 1.f),
		gresVol.res_values.GetParam("SAMPLE_OFFSET_Y", 1.f),
		gresVol.res_values.GetParam("SAMPLE_OFFSET_Z", 1.f));
	float minDistSample = (float)min(min(vol_data->vox_pitch.x * sampleOffset.x, vol_data->vox_pitch.y * sampleOffset.y),
		vol_data->vox_pitch.z * sampleOffset.z);
	//cout << "minDistSample >>>> " << minDistSample << endl;
	float maxDistSample = (float)max(max(vol_data->vox_pitch.x * sampleOffset.x, vol_data->vox_pitch.y * sampleOffset.y),
		vol_data->vox_pitch.z * sampleOffset.z);
	//cout << "maxDistSample >>>> " << maxDistSample << endl;
	float resampleDist = vobj->GetObjParam("RESAMPLEVOLUME_SAMPLEDIST", -1.f);
	if (resampleDist > 0) maxDistSample = resampleDist;
	float grad_offset_dist = maxDistSample;
	fTransformVector((vmfloat3*)&cb_volume.vec_grad_x, &vmfloat3(grad_offset_dist, 0, 0), &mat_ws2ts);
	fTransformVector((vmfloat3*)&cb_volume.vec_grad_y, &vmfloat3(0, grad_offset_dist, 0), &mat_ws2ts);
	fTransformVector((vmfloat3*)&cb_volume.vec_grad_z, &vmfloat3(0, 0, grad_offset_dist), &mat_ws2ts);
	
	float sample_rate = fabs(sample_precision);
	//cb_volume.sample_dist = minDistSample / sample_rate;
	if (is_xraymode)
	{
		cb_volume.opacity_correction = 1.f;
		cb_volume.sample_dist = minDistSample / sample_rate;
		cb_volume.v_dummy0 = *(uint32_t*)&cb_volume.opacity_correction;
	}
	else
	{
		if (sample_precision < 0) // modulation
		{
			cb_volume.opacity_correction = 1.f;
			cb_volume.sample_dist = minDistSample / sample_rate;
			cb_volume.v_dummy0 = *(uint32_t*)&cb_volume.opacity_correction;
			cb_volume.opacity_correction = 1.f / sample_rate;
		}
		else
		{
			cb_volume.opacity_correction = 1.f;
			cb_volume.sample_dist = minDistSample;
			cb_volume.v_dummy0 = *(uint32_t*)&sample_rate;
		}

		//cb_volume.sample_dist = minDistSample / sample_rate;
		//cb_volume.opacity_correction = 1.f / sample_rate;
	}
	
	//cb_volume.vol_size = vmfloat3((float)vol_data->vol_size.x, (float)vol_data->vol_size.y, (float)vol_data->vol_size.z);

	cb_volume.vol_size = vmfloat3(gresVol.res_values.GetParam("WIDTH", (uint32_t)1),
		gresVol.res_values.GetParam("HEIGHT", (uint32_t)1),
		gresVol.res_values.GetParam("DEPTH", (uint32_t)1));

	cb_volume.vol_original_size = vmuint3(
		vol_data->vol_size.x,
		vol_data->vol_size.y,
		vol_data->vol_size.z
	);

	// from pmapDValueVolume //
	//float fSamplePrecisionLevel = actor->GetParam("_float_SamplePrecisionLevel", 1.0f);
	//if (fSamplePrecisionLevel > 0)
	//{
	//	cb_volume.sample_dist /= fSamplePrecisionLevel;
	//	cb_volume.opacity_correction /= fSamplePrecisionLevel;
	//}
	cb_volume.vz_thickness = cb_volume.sample_dist;	// ���� HLSL �� Sample Distance �� ������ ��� ��...
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

	vmmat44f matPivot = (actor->GetParam("_matrix44f_Pivot", vmmat44f(1)));
	vmmat44f matPivotInv = (actor->GetParam("_matrix44f_PivotInv", vmmat44f(1)));

	vmmat44f matRS2WS = matPivot * actor->matOS2WS;
	vmmat44f matWS2RS = actor->matWS2OS * matPivotInv;

	cb_polygon.mat_os2ws = TRANSPOSE(matRS2WS);
	cb_polygon.mat_ws2os = TRANSPOSE(matWS2RS);

	bool use_facecolor = actor->GetParam("_bool_UseFaceColor", false);

	if (is_annotation_obj)// && prim_data->texture_res)
	{
		vmint3 i3TextureWHN = pobj->GetObjParam("_int3_TextureWHN", vmint3(0));
		cb_polygon.num_letters = i3TextureWHN.z;
		if (i3TextureWHN.z == 1)
		{
			vmfloat3* pos_vtx = pobj_data->GetVerticeDefinition<vmfloat3>("POSITION");
			vmmat44f matOS2SS = matRS2WS * matWS2SS;

			vmfloat3 pos_vtx_0_ss, pos_vtx_1_ss, pos_vtx_2_ss;
			fTransformPoint(&pos_vtx_0_ss, &pos_vtx[0], &matOS2SS);
			fTransformPoint(&pos_vtx_1_ss, &pos_vtx[1], &matOS2SS);
			fTransformPoint(&pos_vtx_2_ss, &pos_vtx[2], &matOS2SS);
			if(pos_vtx_1_ss.x - pos_vtx_0_ss.x < 0)
				cb_polygon.pobj_flag |= (0x1 << 9);
			if (pos_vtx_2_ss.y - pos_vtx_0_ss.y < 0)
				cb_polygon.pobj_flag |= (0x1 << 10);
		}
	}
	else
	{
		if (use_facecolor && pobj_data->GetCustomDefinition("FACECOLOR"))
		{
			cb_polygon.pobj_flag |= (0x1 << 2);
		}

		if (use_vertex_color || use_facecolor)
			cb_polygon.pobj_flag |= (0x1 << 3);


		if (actor->GetParam("_bool_UseTriNormal", false))
		{
			cb_polygon.pobj_flag |= (0x1 << 1);
		}
	}

	{
		vmfloat3 illum_model = actor->GetParam("_float3_IllumWavefront", vmfloat3(1));
		// material setting
		cb_polygon.alpha = actor->color.a;
		vmfloat3 material_color = use_facecolor? vmfloat3(1) : vmfloat3(actor->color);
		cb_polygon.Ka = actor->GetParam("_float3_Ka", material_color) * illum_model.x;
		cb_polygon.Kd = actor->GetParam("_float3_Kd", material_color) * illum_model.y;
		cb_polygon.Ks = actor->GetParam("_float3_Ks", material_color) * illum_model.z;
		cb_polygon.Ns = actor->GetParam("_float_Ns", (float)1.f);;

		cb_polygon.pobj_dummy_1 = 
			(uint32_t)(actor->color.r * 255.f) | 
			((uint32_t)(actor->color.g * 255.f) << 8) |
			((uint32_t)(actor->color.b * 255.f) << 16) |
			((uint32_t)(actor->color.a * 255.f) << 24);
	}

	bool abs_diffuse = actor->GetParam("_bool_AbsDiffuse", false); // alpha ���� ����...??
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
			fTransformPoint(&pos_max_ws, &vmfloat3(pobj_data->aabb_os.pos_max), &matRS2WS);
			fTransformPoint(&pos_min_ws, &vmfloat3(pobj_data->aabb_os.pos_min), &matRS2WS);
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

	vmmat44f matOS2PS = matRS2WS * matWS2PS;
	cb_polygon.mat_os2ps = TRANSPOSE(matOS2PS);

	bool force_to_pointsetrender = actor->GetParam("_bool_ForceToPointsetRender", false);
	if (pobj_data->ptype == PrimitiveTypePOINT || force_to_pointsetrender)
	{
		bool is_surfel = actor->GetParam("_bool_PointToSurfel", true);
		float fPointThickness = is_surfel? actor->GetParam("_float_SurfelSize", 0.f) : actor->GetParam("_float_PointThickness", 0.f);
		if (fPointThickness <= 0)
		{
			vmfloat3 pos_max_ws, pos_min_ws;
			fTransformPoint(&pos_max_ws, &((vmfloat3)pobj_data->aabb_os.pos_max), &matRS2WS);
			fTransformPoint(&pos_min_ws, &((vmfloat3)pobj_data->aabb_os.pos_min), &matRS2WS);
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

void grd_helper::SetCb_RenderingEffect(CB_Material& cb_reffect, VmActor* actor)
{
	bool apply_occ = actor->GetParam("_bool_ApplyAO", false);
	bool apply_brdf = actor->GetParam("_bool_ApplyBRDF", false);
	cb_reffect.rf_flag = (int)apply_occ | ((int)apply_brdf << 1);

	if (apply_occ)
	{
		cb_reffect.occ_num_rays = (uint32_t)actor->GetParam("_int_OccNumRays", (int)5);
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

void grd_helper::SetCb_VolumeRenderingEffect(CB_VolumeMaterial& cb_vreffect, VmVObjectVolume* vobj, VmActor* actor)
{
	cb_vreffect.attribute_voxel_sharpness = actor->GetParam("_float_VoxelSharpnessForAttributeVolume", 0.25f);
	cb_vreffect.clip_plane_intensity = actor->GetParam("_float_ClipPlaneIntensity", 1.f);

	vmdouble2 dGradientMagMinMax = vmdouble2(1, -1);
	cb_vreffect.occ_sample_dist_scale = actor->GetParam("_float_OccSampleDistScale", 1.f);
	cb_vreffect.sdm_sample_dist_scale = actor->GetParam("_float_SdmSampleDistScale", 1.f);

	bool jitteringSample = actor->GetParam("_bool_JitteringSample", false);
	cb_vreffect.flag = (int)jitteringSample;
}

void grd_helper::SetCb_ClipInfo(CB_ClipInfo& cb_clip, VmVObject* obj, VmActor* actor, const int camClipMode, const std::set<int> camClipperFreeActor,
	const vmmat44f& matCamClipWS2BS, const vmfloat3& camClipPlanePos, const vmfloat3& camClipPlaneDir)
{
	const int obj_id = obj->GetObjectID();
	bool is_clip_free = obj->GetObjParam("_bool_ClipFree", false);
	is_clip_free |= actor->GetParam("_bool_ClipFree", false);

	cb_clip.clip_flag = 0;
	if (!is_clip_free)
	{
		// CLIPBOX / CLIPPLANE / BOTH //

		if (camClipMode == 0) {
			int clip_mode = actor->GetParam("_int_ClippingMode", (int)0);	// 0 : No, 1 : CLIPPLANE, 2 : CLIPBOX, 3 : BOTH
			cb_clip.clip_flag = clip_mode & 0x3;
		}
		else {
			if (camClipperFreeActor.find(actor->actorId) == camClipperFreeActor.end())
				cb_clip.clip_flag = camClipMode & 0x3;
		}
	}

	if (cb_clip.clip_flag & 0x1)
	{
		if (camClipMode == 0) {
			cb_clip.pos_clipplane = actor->GetParam("_float3_PosClipPlaneWS", (vmfloat3)0);
			cb_clip.vec_clipplane = actor->GetParam("_float3_VecClipPlaneWS", (vmfloat3)0);
		}
		else {
			cb_clip.pos_clipplane = camClipPlanePos;
			cb_clip.vec_clipplane = camClipPlaneDir;
		}
	}
	if (cb_clip.clip_flag & 0x2)
	{
		if (camClipMode == 0) {
			cb_clip.mat_clipbox_ws2bs = TRANSPOSE(actor->GetParam("_matrix44f_MatrixClipWS2BS", vmmat44f(1)));
		}
		else {
			cb_clip.mat_clipbox_ws2bs = TRANSPOSE(matCamClipWS2BS);
		}
	}
}

void grd_helper::SetCb_HotspotMask(CB_HotspotMask& cb_hsmask, VmFnContainer* _fncontainer, const vmmat44f& matWS2SS)
{
	vmfloat3 pos_3dtip_ws = _fncontainer->fnParams.GetParam("_float3_3DTipPos", vmfloat3(0));
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
		cb_hsmask.mask_info_[idx].smoothness = (int)(max(min(mask_center_rs_0.w * max_smoothness, max_smoothness), 0.0)) | ((int)show_silhuette_edge << 16);
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
	float fThicknessPosition = fThicknessRatio * fExCurveThicknessPositionRange * 0.5f;
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

	//cout << "TEST sample dvr : " << iThicknessStep << ", " << sample_dist << ", " << fPlanePitch << ", " << fPlaneThickness << endl;

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
		if (g_psoManager->dx11Device->CreatePixelShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, (ID3D11PixelShader**)sm) != S_OK)
		{
			vmlog::LogErr(string("*** COMPILE ERROR : ") + str + ", " + entry_point + ", " + shader_model);
			return false;
		}
	}
	else if (shader_model.find("vs") != string::npos)
	{
		if (g_psoManager->dx11Device->CreateVertexShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, (ID3D11VertexShader**)sm) != S_OK)
		{
			vmlog::LogErr(string("*** COMPILE ERROR : ") + str + ", " + entry_point + ", " + shader_model);
			return false;
		}
	}
	else if (shader_model.find("cs") != string::npos)
	{
		if (g_psoManager->dx11Device->CreateComputeShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, (ID3D11ComputeShader**)sm) != S_OK)
		{
			vmlog::LogErr(string("*** COMPILE ERROR : ") + str + ", " + entry_point + ", " + shader_model);
			return false;
		}
	}
	else if (shader_model.find("gs") != string::npos)
	{
		if (shader_model.find("SO") != string::npos) {
			// https://learn.microsoft.com/en-us/windows/win32/direct3d11/d3d10-graphics-programming-guide-output-stream-stage-getting-started
			// https://strange-cpp.tistory.com/101
			D3D11_SO_DECLARATION_ENTRY pDecl[] =
			{
				// semantic name, semantic index, start component, component count, output slot
				{ 0, "TEXCOORD", 0, 0, 3, 0 },   // output 
			};
			int numEntries = sizeof(pDecl) / sizeof(D3D11_SO_DECLARATION_ENTRY);
			uint32_t bufferStrides[] = { sizeof(vmfloat3) };
			int numStrides = sizeof(bufferStrides) / sizeof(uint32_t);
			if (g_psoManager->dx11Device->CreateGeometryShaderWithStreamOutput(
				pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), pDecl, numEntries, bufferStrides, numStrides, D3D11_SO_NO_RASTERIZED_STREAM, NULL,
				(ID3D11GeometryShader**)sm) != S_OK)
			{
				vmlog::LogErr(string("*** COMPILE ERROR : ") + str + ", " + entry_point + ", " + shader_model);
				return false;
			}
		}
		else {
			if (g_psoManager->dx11Device->CreateGeometryShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, (ID3D11GeometryShader**)sm) != S_OK)
			{
				vmlog::LogErr(string("*** COMPILE ERROR : ") + str + ", " + entry_point + ", " + shader_model);
				return false;
			}
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
	dxRay.direction = XMVector3Normalize(dxRay.direction);
	float fDist = 0;
	if (dxAabb.aabox.Intersects(dxRay.origin, dxRay.direction, fDist))
		return true;
	return false;
}

bool grd_helper::GetEnginePath(std::string& enginePath)
{
	using namespace std;
	char ownPth[2048];
	GetModuleFileNameA(NULL, ownPth, (sizeof(ownPth)));
	string exe_path = ownPth;
	string exe_path_;
	size_t pos = 0;
	std::string token;
	string delimiter = "\\";
	while ((pos = exe_path.find(delimiter)) != std::string::npos) {
		token = exe_path.substr(0, pos);
		if (token.find(".exe") != std::string::npos) break;
		exe_path += token + "\\";
		exe_path_ += token + "\\";
		exe_path.erase(0, pos + delimiter.length());
	}

	ifstream file(exe_path_ + "..\\engine_module_path.txt");
	if (file.is_open()) {
		getline(file, enginePath);
		file.close();
		return true;
	}
	return false;
}