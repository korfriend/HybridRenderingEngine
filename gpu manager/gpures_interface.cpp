#include "../gpu_common_res.h"
#include "gpures_interface.h"

//#include <d3dx9math.h>	// For Math and Structure
#include <d3d11_3.h>
//#include <d3dx11.h>
//#define SDK_REDISTRIBUTE

//#define _DEBUG
#if (defined(_DEBUG) || defined(DEBUG)) && !defined(SDK_REDISTRIBUTE)
#include <DXGI.h>
ID3D11Debug *debugDev;
#endif

#include <math.h>
#include <iostream>

#ifndef VMSAFE_RELEASE
#define VMSAFE_RELEASE(p)			{ if(p) { (p)->Release(); (p)=NULL; } }
#endif

struct RES_INDICATOR
{
	int src_id;
	std::string res_name;
	RES_INDICATOR(const GpuRes& gres)
	{
		src_id = gres.vm_src_id;
		res_name = gres.res_name;
	}
};

class valueComp
{
public:
	bool operator()(const RES_INDICATOR& a, const RES_INDICATOR& b) const
	{
		if ((int)a.src_id < (int)b.src_id)
			return true;
		else if ((int)a.src_id > (int)b.src_id)
			return false;
		if (a.res_name.compare(b.res_name) < 0)
			return true;
		else if (a.res_name.compare(b.res_name) > 0)
			return false;
		return false;
	}
};

struct _gp_lobj_buffer {
	void* buffer_ptr;
	data_type dtype;
	int elements;
};

struct _gp_dxgiPresenter {
	DXGI_SWAP_CHAIN_DESC1 sd;
	IDXGISwapChain* dxgiSwapChain;
	IDXGISwapChain1* dxgiSwapChain1;
	IDXGIFactory1* pdxgiFactory = NULL;
	IDXGIFactory2* pdxgiFactory2 = NULL;
	ID3D11Texture2D* pBackBuffer = NULL; // used for CopyResource
	ID3D11RenderTargetView* pRTView = NULL;
};

typedef map<RES_INDICATOR, GpuRes, valueComp> GPUResMap;
map<string, _gp_lobj_buffer> g_mapCustomParameters;

map<HWND, _gp_dxgiPresenter> g_mapDxgiPrensentor;

static ID3D11Device* g_pdx11Device = NULL;
static ID3D11DeviceContext* g_pdx11DeviceImmContext = NULL;
#ifdef USE_DX11_3
static ID3D11Device3* g_pdx11Device3= NULL;
static ID3D11DeviceContext3* g_pdx11DeviceImmContext3 = NULL;
#endif
static D3D_FEATURE_LEVEL g_eFeatureLevel = (D3D_FEATURE_LEVEL)0xb300;
static DXGI_ADAPTER_DESC g_adapterDesc;
static GPUResMap g_mapVmResources;

IDXGIDevice* g_pdxgiDevice = NULL;
IDXGIAdapter* g_pdxgiAdapter = NULL;

bool __InitializeDevice()
{
	if (g_pdx11Device || g_pdx11DeviceImmContext)
	{
		printf("VmManager::CreateDevice - Already Created!");
		return true;
	}

	UINT createDeviceFlags = 0;
	D3D_DRIVER_TYPE driverTypes = D3D_DRIVER_TYPE_HARDWARE;
#if (defined(_DEBUG) || defined(DEBUG)) && !defined(SDK_REDISTRIBUTE)
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
	// 	driverTypes = D3D_DRIVER_TYPE_REFERENCE;
#endif
	try
	{
		D3D_FEATURE_LEVEL featureLevels[] =
		{
			D3D_FEATURE_LEVEL_12_1,
			D3D_FEATURE_LEVEL_12_0,
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
			//D3D_FEATURE_LEVEL_10_1,
			//D3D_FEATURE_LEVEL_10_0,
			//D3D_FEATURE_LEVEL_9_3,
			//D3D_FEATURE_LEVEL_9_1
		};

		D3D11CreateDevice(NULL, driverTypes, NULL, createDeviceFlags, featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION, 
			&g_pdx11Device, &g_eFeatureLevel, &g_pdx11DeviceImmContext);

#ifdef USE_DX11_3
		D3D11_FEATURE_DATA_D3D11_OPTIONS3 FeatureData = {};
		HRESULT hh = g_pdx11Device->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS3, &FeatureData, sizeof(D3D11_FEATURE_DATA_D3D11_OPTIONS3));
		HRESULT hr = g_pdx11Device->QueryInterface(IID_PPV_ARGS(&g_pdx11Device3));
		if (SUCCEEDED(hr) && g_pdx11Device3)
		{
			//GetDXUTState().SetD3D11Device3(pd3d11Device3);

			hr = g_pdx11DeviceImmContext->QueryInterface(IID_PPV_ARGS(&g_pdx11DeviceImmContext3));
			if (SUCCEEDED(hr) && g_pdx11DeviceImmContext3)
			{
				//GetDXUTState().SetD3D11DeviceContext3(pd3dImmediateContext3);
			}
		}
		if (g_pdx11Device3 == NULL || g_pdx11DeviceImmContext3 == NULL)
		{
			cout << "dx11 feature 3 failed!" << endl;
		}
		else
			cout << "dx11 feature 3 OK!!" << endl;
#endif
	}

	catch (std::exception&)
	{
		//::MessageBox(NULL, ("VX3D requires DirectX Driver!!"), NULL, MB_OK);
		//string str = e.what();
		//std::string wstr(str.length(),L' ');
		//copy(str.begin(),str.end(),wstr.begin());
		//::MessageBox(NULL, wstr.c_str(), NULL, MB_OK);
		//exit(0);
		return false;
	}

	if (g_pdx11Device == NULL || g_pdx11DeviceImmContext == NULL || g_eFeatureLevel < 0x9300)
	{
#ifdef USE_DX11_3
		VMSAFE_RELEASE(g_pdx11DeviceImmContext3);
		VMSAFE_RELEASE(g_pdx11Device3);
#endif
		VMSAFE_RELEASE(g_pdx11DeviceImmContext);
		VMSAFE_RELEASE(g_pdx11Device);
		return false;
	}

	printf("\nRenderer's DirectX Device Creation Success\n\n");

#if (defined(_DEBUG) || defined(DEBUG)) && !defined(SDK_REDISTRIBUTE)
	// Debug //
	HRESULT hr = g_pdx11Device->QueryInterface(__uuidof(ID3D11Debug), (void**)(&debugDev));
	debugDev->ReportLiveDeviceObjects(D3D11_RLDO_IGNORE_INTERNAL );
#endif

	g_pdx11Device->QueryInterface(IID_PPV_ARGS(&g_pdxgiDevice));
	g_pdxgiDevice->GetAdapter(&g_pdxgiAdapter);
	g_pdxgiAdapter->GetDesc(&g_adapterDesc);

	// 0x10DE : NVIDIA
	// 0x1002, 0x1022 : AMD ATI
// 	if(g_adapterDesc.VendorId != 0x10DE && g_adapterDesc.VendorId != 0x1002 && g_adapterDesc.VendorId != 0x1022)
// 	{
// 		VMSAFE_RELEASE(g_pdx11DeviceImmContext);
// 		VMSAFE_RELEASE(g_pdx11Device);
// 		return false;
// 	}

//	VmDeviceSetting(g_pdx11Device, g_pdx11DeviceImmContext, g_adapterDesc);

	return true;
}

bool __DeinitializeDevice()
{
	if (g_pdx11DeviceImmContext == NULL || g_pdx11Device == NULL)
		return false;

	g_pdx11DeviceImmContext->Flush();
	g_pdx11DeviceImmContext->ClearState();
	__ReleaseAllGpuResources();
	g_pdx11DeviceImmContext->Flush();
	g_pdx11DeviceImmContext->ClearState();

	VMSAFE_RELEASE(g_pdxgiDevice);
	VMSAFE_RELEASE(g_pdxgiAdapter);

#ifdef USE_DX11_3
	VMSAFE_RELEASE(g_pdx11DeviceImmContext3);
	VMSAFE_RELEASE(g_pdx11Device3);
#endif
	VMSAFE_RELEASE(g_pdx11DeviceImmContext);
	VMSAFE_RELEASE(g_pdx11Device);

#if (defined(_DEBUG) || defined(DEBUG)) && !defined(SDK_REDISTRIBUTE)
	debugDev->ReportLiveDeviceObjects(D3D11_RLDO_IGNORE_INTERNAL );
	VMSAFE_RELEASE(debugDev);
#endif

	return true;
}

bool __GetGpuMemoryBytes(uint* dedicatedGpuMemory, uint* freeMemory)
{
	if (dedicatedGpuMemory)
		*dedicatedGpuMemory = (uint)g_adapterDesc.DedicatedVideoMemory;
	if (freeMemory)
		*freeMemory = (uint)g_adapterDesc.DedicatedVideoMemory;

	return true;
}

bool __GetDeviceInformation(void* devInfo, const string& devSpecification)
{
	if (devSpecification.compare("DEVICE_POINTER") == 0)
	{
		void** ppvDev = (void**)devInfo;
		*ppvDev = (void*)g_pdx11Device;
	}
	else if (devSpecification.compare("DEVICE_CONTEXT_POINTER") == 0)
	{
		void** ppvDev = (void**)devInfo;
		*ppvDev = (void*)g_pdx11DeviceImmContext;
	}
#ifdef USE_DX11_3
	else if (devSpecification.compare("DEVICE_POINTER_3") == 0)
	{
		void** ppvDev = (void**)devInfo;
		*ppvDev = (void*)g_pdx11Device3;
	}
	else if (devSpecification.compare("DEVICE_CONTEXT_POINTER_3") == 0)
	{
		void** ppvDev = (void**)devInfo;
		*ppvDev = (void*)g_pdx11DeviceImmContext3;
	}
#endif
	else if (devSpecification.compare("DEVICE_ADAPTER_DESC") == 0)
	{
		DXGI_ADAPTER_DESC* pdx11Adapter = (DXGI_ADAPTER_DESC*)devInfo;
		*pdx11Adapter = g_adapterDesc;
	}
	else if (devSpecification.compare("DEVICE_DXGI2") == 0)
	{
		//void** ppvDev = (void**)devInfo;
		//*ppvDev = g_pdxgiFactory2;
		//*ppvDev = g_pdxgiFactory2 ? (void*)g_pdxgiFactory2 : (void*)g_pdxgiFactory;
	}
	else if (devSpecification.compare("FEATURE_LEVEL") == 0)
	{
		D3D_FEATURE_LEVEL* pdx11FeatureLevel = (D3D_FEATURE_LEVEL*)devInfo;
		*pdx11FeatureLevel = g_eFeatureLevel;
	}
	else
		return false;
	return true;
}

ullong __GetUsedGpuMemorySizeBytes()
{
	ullong ullSizeBytes = 0;
	auto itrResDX11 = g_mapVmResources.begin();
	for (; itrResDX11 != g_mapVmResources.end(); itrResDX11++)
	{
		if (itrResDX11->second.alloc_res_ptrs[DTYPE_RES] != NULL)
		{
			ullSizeBytes += itrResDX11->second.res_values.GetParam("RES_SIZE_BYTES", (ullong)0);
		}
	}
	return (ullong)(ullSizeBytes);
}

bool __UpdateDXGI(void** ppBackBuffer, void** ppRTView, const HWND hwnd, const int w, const int h)
{
	// g_mapDxgiPrensentor
	// g_pdxgiFactory
	auto it = g_mapDxgiPrensentor.find(hwnd);
	int w_prev = w, h_prev = h;
	if (it == g_mapDxgiPrensentor.end())
	{
		_gp_dxgiPresenter _dxgi = {};
		g_pdxgiAdapter->GetParent(IID_PPV_ARGS(&_dxgi.pdxgiFactory));
		_dxgi.pdxgiFactory->QueryInterface(IID_PPV_ARGS(&_dxgi.pdxgiFactory2));

		// create and register
		_dxgi.sd = {};
		_dxgi.sd.Width = w;
		_dxgi.sd.Height = h;
		_dxgi.sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		_dxgi.sd.SampleDesc.Count = 1;
		_dxgi.sd.SampleDesc.Quality = 0;
		_dxgi.sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		_dxgi.sd.BufferCount = 2;

		HRESULT hr = _dxgi.pdxgiFactory2->CreateSwapChainForHwnd(g_pdx11Device, hwnd, &_dxgi.sd, nullptr, nullptr, &_dxgi.dxgiSwapChain1);
		if (SUCCEEDED(hr))
			hr = _dxgi.dxgiSwapChain1->QueryInterface(IID_PPV_ARGS(&_dxgi.dxgiSwapChain));

		hr = _dxgi.dxgiSwapChain->GetBuffer(0, IID_PPV_ARGS(&_dxgi.pBackBuffer));
		if (FAILED(hr))
			return false;
		
		hr = g_pdx11Device->CreateRenderTargetView(_dxgi.pBackBuffer, nullptr, &_dxgi.pRTView);
		if (FAILED(hr))
			return false;

		g_mapDxgiPrensentor.insert(pair<HWND, _gp_dxgiPresenter>(hwnd, _dxgi));
	}
	else {
		w_prev = (int)it->second.sd.Width;
		h_prev = (int)it->second.sd.Height;

		if (w_prev == w && h_prev == h) {
			// no changes
			*ppBackBuffer = it->second.pBackBuffer;
			*ppRTView = it->second.pRTView;
			return true;
		}
	}

	_gp_dxgiPresenter& dxgiPresenter = g_mapDxgiPrensentor[hwnd];

	if (w != w_prev || h != h_prev) {
		VMSAFE_RELEASE(dxgiPresenter.pRTView);
		VMSAFE_RELEASE(dxgiPresenter.pBackBuffer);
		// resize case ...
		// the output resources must be released priorly
		dxgiPresenter.sd.Width = w;
		dxgiPresenter.sd.Height = h;

		HRESULT hr = dxgiPresenter.dxgiSwapChain->ResizeBuffers(dxgiPresenter.sd.BufferCount, 
			w, h, DXGI_FORMAT_R8G8B8A8_UNORM, 0);

		if (FAILED(hr))
			return false;

		hr = dxgiPresenter.dxgiSwapChain->GetBuffer(0, IID_PPV_ARGS(&dxgiPresenter.pBackBuffer));
		if (FAILED(hr))
			return false;

		hr = g_pdx11Device->CreateRenderTargetView(dxgiPresenter.pBackBuffer, nullptr, &dxgiPresenter.pRTView);
		if (FAILED(hr))
			return false;
	}

	*ppBackBuffer = dxgiPresenter.pBackBuffer;
	*ppRTView = dxgiPresenter.pRTView;

	return true;
}

bool __PresentBackBuffer(const HWND hwnd)
{
	auto it = g_mapDxgiPrensentor.find(hwnd);
	if (it == g_mapDxgiPrensentor.end())
		return false;

	it->second.dxgiSwapChain->Present(0, 0);
	return true;
}

bool __ReleaseDXGI(const HWND hwnd)
{
	auto it = g_mapDxgiPrensentor.find(hwnd);
	if (it == g_mapDxgiPrensentor.end())
		return false;

	VMSAFE_RELEASE(it->second.pBackBuffer);
	VMSAFE_RELEASE(it->second.pRTView);
	VMSAFE_RELEASE(it->second.dxgiSwapChain1);
	VMSAFE_RELEASE(it->second.dxgiSwapChain);
	VMSAFE_RELEASE(it->second.pdxgiFactory2);
	VMSAFE_RELEASE(it->second.pdxgiFactory);

	return true;
}

bool __ReleaseAllDXGIs()
{
	for (auto it = g_mapDxgiPrensentor.begin(); it != g_mapDxgiPrensentor.end(); it++) {
		VMSAFE_RELEASE(it->second.pBackBuffer);
		VMSAFE_RELEASE(it->second.pRTView);
		VMSAFE_RELEASE(it->second.dxgiSwapChain1);
		VMSAFE_RELEASE(it->second.dxgiSwapChain);
		VMSAFE_RELEASE(it->second.pdxgiFactory2);
		VMSAFE_RELEASE(it->second.pdxgiFactory);
	}
	g_mapDxgiPrensentor.clear();

	return true;
}

bool __UpdateGpuResource(GpuRes& gres)
{
	auto itrResDX11 = g_mapVmResources.find(RES_INDICATOR(gres));
	if (itrResDX11 == g_mapVmResources.end())
		return false;
	
	//gres.alloc_res_ptrs = itrResDX11->second.alloc_res_ptrs;
	gres = itrResDX11->second;

	return true;
}

int __UpdateGpuResourcesBySrcID(const int src_id, vector<GpuRes>& gres_list)
{
	auto itrResDX11 = g_mapVmResources.begin();
	for (; itrResDX11 != g_mapVmResources.end(); itrResDX11++)
	{
		if (itrResDX11->second.vm_src_id == src_id)
			gres_list.push_back(itrResDX11->second);
	}

	return (int)gres_list.size();
}

bool __GenerateGpuResource(GpuRes& gres, LocalProgress* progress)
{
	auto GetOption = [&](const std::string& flag_name) -> uint
	{
		auto it = gres.options.find(flag_name);
		if (it == gres.options.end()) return 0;
		return it->second;
	};


	//if (gres.vm_src_id == 33554445)
	//	cout << "------33554445-----> " << gres.res_name << (uint)GetParam("NUM_ELEMENTS") << endl;

	__ReleaseGpuResource(gres, false);

	auto GetSizeFormat = [&](DXGI_FORMAT format) -> uint
	{
		uint stride_bytes = 0;
		switch ((DXGI_FORMAT)GetOption("FORMAT"))
		{
		case DXGI_FORMAT_R32_SINT: stride_bytes = sizeof(int); break;
		case DXGI_FORMAT_R32_UINT: stride_bytes = sizeof(uint); break;
		case DXGI_FORMAT_R32_FLOAT: stride_bytes = sizeof(float); break;
		case DXGI_FORMAT_D32_FLOAT:	stride_bytes = sizeof(float); break;
		case DXGI_FORMAT_R32G32B32A32_FLOAT: stride_bytes = sizeof(vmfloat4); break;
		case DXGI_FORMAT_R32G32_FLOAT: stride_bytes = sizeof(vmfloat2); break;
		case DXGI_FORMAT_R8G8B8A8_UNORM: stride_bytes = sizeof(vmbyte4); break;
		case DXGI_FORMAT_R8G8B8A8_UINT: stride_bytes = sizeof(vmbyte4); break;
		case DXGI_FORMAT_R8_UNORM: stride_bytes = sizeof(byte); break;
		case DXGI_FORMAT_R8_UINT: stride_bytes = sizeof(byte); break;
		case DXGI_FORMAT_R8_SINT: stride_bytes = sizeof(char); break;
		case DXGI_FORMAT_R16_UNORM: stride_bytes = sizeof(ushort); break;
		case DXGI_FORMAT_R16_UINT: stride_bytes = sizeof(ushort); break;
		case DXGI_FORMAT_R16_SINT: stride_bytes = sizeof(short); break;
		case DXGI_FORMAT_UNKNOWN: stride_bytes = gres.res_values.GetParam("STRIDE_BYTES", (uint)0); break;
		default:
			::MessageBoxA(NULL, "NOT SUPPORTED FORMAT!!", NULL, MB_OK);
			return 0;
		}
		return stride_bytes;
	};

	switch (gres.rtype)
	{
	case RTYPE_BUFFER:
	{
		// GetOption("FORMAT") ==> needed for VIEW creation
		// vertex, index, ...
		uint num_elements = gres.res_values.GetParam("NUM_ELEMENTS", (uint)0);
		uint stride_bytes = gres.res_values.GetParam("STRIDE_BYTES", (uint)0);
		D3D11_BUFFER_DESC desc_buf;
		ZeroMemory(&desc_buf, sizeof(D3D11_BUFFER_DESC));
		desc_buf.ByteWidth = stride_bytes * num_elements;
		desc_buf.Usage = (D3D11_USAGE)GetOption("USAGE");
		desc_buf.BindFlags = GetOption("BIND_FLAG");
		desc_buf.CPUAccessFlags = GetOption("CPU_ACCESS_FLAG");
		desc_buf.MiscFlags = NULL;
		desc_buf.StructureByteStride = stride_bytes;
		desc_buf.MiscFlags = (DXGI_FORMAT)GetOption("FORMAT") == DXGI_FORMAT_UNKNOWN ? D3D11_RESOURCE_MISC_BUFFER_STRUCTURED : NULL;
		ID3D11Buffer* pdx11Buffer = NULL;
		if (GetOption("RAW_ACCESS") & 0x1)
		{
			// D3D11_BUFFEREX_SRV_FLAG_RAW not D3D11_BUFFER_SRV_FLAG_RAW
			// https://docs.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-resources-intro
			desc_buf.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
		}
		//if (num_elements == 18 && stride_bytes == 36)
		//	printf("LLLL\n");
		if (g_pdx11Device->CreateBuffer(&desc_buf, NULL, &pdx11Buffer) != S_OK)
		{
			printf("GG %d, %d, \n", num_elements, stride_bytes);
			return false;
		}

		gres.alloc_res_ptrs[DTYPE_RES] = (void*)pdx11Buffer;
		gres.res_values.SetParam("RES_SIZE_BYTES", (ullong)(num_elements * stride_bytes));
		break;
	}
	case RTYPE_TEXTURE1D:
	{
		// not yet.
		return false;
	}
	case RTYPE_TEXTURE2D:
	{
		// OTF, RenderRes, ..
		D3D11_TEXTURE2D_DESC descTex2D;
		ZeroMemory(&descTex2D, sizeof(D3D11_TEXTURE2D_DESC));
		descTex2D.Width = gres.res_values.GetParam("WIDTH", (uint)0);
		descTex2D.Height = gres.res_values.GetParam("HEIGHT", (uint)0); 
		descTex2D.MipLevels = gres.options["MIP_GEN"] == 1? 0 : 1;
		descTex2D.ArraySize = max(gres.res_values.GetParam("DEPTH", (uint)0), (uint)1);
		descTex2D.Format = (DXGI_FORMAT)GetOption("FORMAT");
		descTex2D.SampleDesc.Count = 1;
		descTex2D.SampleDesc.Quality = 0;
		descTex2D.Usage = (D3D11_USAGE)GetOption("USAGE");
		descTex2D.BindFlags = GetOption("BIND_FLAG");
		descTex2D.CPUAccessFlags = GetOption("CPU_ACCESS_FLAG");
		descTex2D.MiscFlags = gres.options["MIP_GEN"] == 1 ? D3D11_RESOURCE_MISC_GENERATE_MIPS : NULL;

		//if (gres.options["MIP_GEN"] == 1)
		//	int gg = 0;
		ID3D11Texture2D* pdx11TX2D = NULL;
		g_pdx11Device->CreateTexture2D(&descTex2D, NULL, &pdx11TX2D);
		if (pdx11TX2D == NULL)
			::MessageBoxA(NULL, "CreateTexture2D ==> ERROR!!", NULL, S_OK);

		gres.alloc_res_ptrs[DTYPE_RES] = (void*)pdx11TX2D;
		gres.res_values.SetParam("RES_SIZE_BYTES", (ullong)(descTex2D.Width* descTex2D.Height* GetSizeFormat(descTex2D.Format)));
		break;
	}
	case RTYPE_TEXTURE3D:
	{
		D3D11_TEXTURE3D_DESC descTex3D;
		ZeroMemory(&descTex3D, sizeof(D3D11_TEXTURE3D_DESC));
		descTex3D.Width = gres.res_values.GetParam("WIDTH", (uint)0);
		descTex3D.Height = gres.res_values.GetParam("HEIGHT", (uint)0);
		descTex3D.Depth = gres.res_values.GetParam("DEPTH", (uint)0);
		descTex3D.MipLevels = 1;
		descTex3D.MiscFlags = NULL;
		descTex3D.Format = (DXGI_FORMAT)GetOption("FORMAT");
		descTex3D.Usage = (D3D11_USAGE)GetOption("USAGE");
		descTex3D.BindFlags = GetOption("BIND_FLAG");
		descTex3D.CPUAccessFlags = GetOption("CPU_ACCESS_FLAG");

		ID3D11Texture3D* pdx11TX3D = NULL;
		g_pdx11Device->CreateTexture3D(&descTex3D, NULL, &pdx11TX3D);

		gres.alloc_res_ptrs[DTYPE_RES] = (void*)pdx11TX3D;
		gres.res_values.SetParam("RES_SIZE_BYTES", (ullong)(descTex3D.Width* descTex3D.Height* descTex3D.Depth* GetSizeFormat(descTex3D.Format)));
		break;
	}
	default: break;
	}

	// CREATE VIEWS
	uint bind_flag = GetOption("BIND_FLAG");
	if (bind_flag & D3D11_BIND_RENDER_TARGET)
	{
		if (gres.rtype != RTYPE_TEXTURE2D)
		{
			::MessageBoxA(NULL, "Error Res Type", NULL, MB_OK);
			return false;
		}
		D3D11_RENDER_TARGET_VIEW_DESC descRTV;
		ZeroMemory(&descRTV, sizeof(D3D11_RENDER_TARGET_VIEW_DESC));
		descRTV.Format = (DXGI_FORMAT)GetOption("FORMAT");
		descRTV.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		descRTV.Texture2D.MipSlice = 0;
		ID3D11View* pdx11View = NULL;
		g_pdx11Device->CreateRenderTargetView((ID3D11Resource*)gres.alloc_res_ptrs[DTYPE_RES], &descRTV, (ID3D11RenderTargetView**)&pdx11View);
		gres.alloc_res_ptrs[DTYPE_RTV] = (void*)pdx11View;
	}
	if (bind_flag & D3D11_BIND_DEPTH_STENCIL)
	{
		if (gres.rtype != RTYPE_TEXTURE2D)
		{
			::MessageBoxA(NULL, "Error Res Type", NULL, MB_OK);
			return false;
		}
		D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
		ZeroMemory(&descDSV, sizeof(D3D11_DEPTH_STENCIL_VIEW_DESC));
		descDSV.Format = (DXGI_FORMAT)GetOption("FORMAT");
		descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		descDSV.Flags = 0;
		descDSV.Texture2D.MipSlice = 0;
		ID3D11View* pdx11View = NULL;
		g_pdx11Device->CreateDepthStencilView((ID3D11Resource*)gres.alloc_res_ptrs[DTYPE_RES], &descDSV, (ID3D11DepthStencilView**)&pdx11View);
		gres.alloc_res_ptrs[DTYPE_DSV] = (void*)pdx11View;
	}
	if (bind_flag & D3D11_BIND_SHADER_RESOURCE)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
		ZeroMemory(&descSRV, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
		descSRV.Format = (DXGI_FORMAT)GetOption("FORMAT");

		switch (gres.rtype)
		{
		case RTYPE_BUFFER:
			descSRV.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
			descSRV.BufferEx.FirstElement = 0;
			descSRV.BufferEx.NumElements = gres.res_values.GetParam("NUM_ELEMENTS", (uint)0);
			if (GetOption("RAW_ACCESS") & 0x1) descSRV.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
			break;
		case RTYPE_TEXTURE2D:
		{
			uint depth = gres.res_values.GetParam("DEPTH", (uint)0);
			if (depth > 1)
			{
				descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
				descSRV.Texture2DArray.MipLevels = gres.options["MIP_GEN"] == 1 ? -1 : 1;
				descSRV.Texture2DArray.ArraySize = depth;
			}
			else
			{
				if (gres.options["MIP_GEN"] == 1)
				{
					descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
					descSRV.Texture2D.MipLevels = -1;
					descSRV.Texture2D.MostDetailedMip = 0;
				}
				else
				{
					descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
					descSRV.Texture2D.MipLevels = 1;
					descSRV.Texture2D.MostDetailedMip = 0;
				}
			}
		} break;
		case RTYPE_TEXTURE3D:
			descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
			descSRV.Texture3D.MipLevels = 1;
			descSRV.Texture3D.MostDetailedMip = 0;
			break;
		default:
			::MessageBoxA(NULL, "Error Res Type", NULL, MB_OK);
			return false;

		}
		ID3D11View* pdx11View = NULL;
		g_pdx11Device->CreateShaderResourceView((ID3D11Resource*)gres.alloc_res_ptrs[DTYPE_RES], &descSRV, (ID3D11ShaderResourceView**)&pdx11View);
		if (pdx11View == NULL)
			::MessageBoxA(NULL, "CreateShaderResourceView ==> ERROR!!", NULL, S_OK);
		//gres.alloc_res_ptrs.insert(pair<BindType, void*>(DTYPE_SRV, pdx11View));
		gres.alloc_res_ptrs[DTYPE_SRV] = (void*)pdx11View;
	}
	if (bind_flag & D3D11_BIND_UNORDERED_ACCESS)
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC descUAV;
		ZeroMemory(&descUAV, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
		descUAV.Format = (DXGI_FORMAT)GetOption("FORMAT");
		switch (gres.rtype)
		{
		case RTYPE_BUFFER:
			descUAV.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			descUAV.Buffer.FirstElement = 0;
			descUAV.Buffer.NumElements = gres.res_values.GetParam("NUM_ELEMENTS", (uint)0);
			if (GetOption("RAW_ACCESS") & 0x1) descUAV.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
			break;
		case RTYPE_TEXTURE2D:
			//descUAV.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
			//descUAV.Texture2D.MipSlice = 0;
			{
				uint depth = gres.res_values.GetParam("DEPTH", (uint)0); 
				if (depth > 1)
				{
					descUAV.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
					descUAV.Texture2DArray.MipSlice = 0;
					descUAV.Texture2DArray.ArraySize = depth;
					descUAV.Texture2DArray.FirstArraySlice = 0;
				}
				else
				{
					descUAV.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
					descUAV.Texture2D.MipSlice = 0;
				}
				break;
			}
		default:
			::MessageBoxA(NULL, "Error Res Type", NULL, MB_OK);
			return false;
		}
		ID3D11View* pdx11View = NULL;
		g_pdx11Device->CreateUnorderedAccessView((ID3D11Resource*)gres.alloc_res_ptrs[DTYPE_RES], &descUAV, (ID3D11UnorderedAccessView**)&pdx11View);
		gres.alloc_res_ptrs[DTYPE_UAV] = (void*)pdx11View;
	}

	gres.res_values.SetParam("LAST_UPDATE_TIME", vmhelpers::GetCurrentTimePack());
	g_mapVmResources[RES_INDICATOR(gres)] = gres;

	return true;
}

bool __ReleaseGpuResource(GpuRes& gres, const bool call_clearstate)
{
	auto itrResDX11 = g_mapVmResources.find(RES_INDICATOR(gres));
	if (itrResDX11 == g_mapVmResources.end())
		return false;

	g_pdx11DeviceImmContext->Flush();

	// add swapchain case...

	std::map<DesType, void*>& alloc_res_ptrs = itrResDX11->second.alloc_res_ptrs;
	for (auto it = alloc_res_ptrs.begin(); it != alloc_res_ptrs.end(); it++)
	{
		switch (it->first)
		{
		case DTYPE_RES:
		{
			ID3D11Resource* pdx11Resource = (ID3D11Resource*)it->second;
			VMSAFE_RELEASE(pdx11Resource);
			break;
		}
		case DTYPE_RTV:
		case DTYPE_DSV:
		case DTYPE_SRV:
		case DTYPE_UAV:
		{
			ID3D11View* pdx11View = (ID3D11View*)it->second;
			VMSAFE_RELEASE(pdx11View);
			break;
		}
		default:
			break;
		}
	}
	//order!
	gres.alloc_res_ptrs.clear();
	g_mapVmResources.erase(itrResDX11);

	g_pdx11DeviceImmContext->Flush();
	if (call_clearstate)
		g_pdx11DeviceImmContext->ClearState();
	return true;
}

bool __ReleaseGpuResourcesBySrcID(const int src_id, const bool call_clearstate)
{
	if (src_id == 0)
		return false;

	g_pdx11DeviceImmContext->Flush();

	auto itrResDX11 = g_mapVmResources.begin();
	while (itrResDX11 != g_mapVmResources.end())
	{
		if (itrResDX11->first.src_id == src_id)
		{
			__ReleaseGpuResource(itrResDX11->second, false);
			itrResDX11 = g_mapVmResources.begin();
		}
		else
		{
			itrResDX11++;
		}
	}

	g_pdx11DeviceImmContext->Flush();
	if (call_clearstate)
		g_pdx11DeviceImmContext->ClearState();

	return true;
}

bool __ReleaseAllGpuResources()
{
	g_pdx11DeviceImmContext->Flush();

	auto itrResDX11 = g_mapVmResources.begin();
	while (itrResDX11 != g_mapVmResources.end())
	{
		__ReleaseGpuResource(itrResDX11->second, false);
		itrResDX11 = g_mapVmResources.begin();
	}

	__ReleaseAllDXGIs();

	g_pdx11DeviceImmContext->Flush();
	g_pdx11DeviceImmContext->ClearState();

	//VMSAFE_RELEASE(g_pdx11DeviceImmContext);
	//VMSAFE_RELEASE(g_pdx11Device);

#if (defined(_DEBUG) || defined(DEBUG)) && !defined(SDK_REDISTRIBUTE)
	debugDev->ReportLiveDeviceObjects(D3D11_RLDO_IGNORE_INTERNAL );
	//VMSAFE_RELEASE(debugDev);
#endif
	//VmInitializeDevice();

	return true;
}

bool __SetGpuManagerParameters(const string& param_name, const data_type& dtype, const void* v_ptr, const int num_elements)
{
	auto it = g_mapCustomParameters.find(param_name);
	if (it != g_mapCustomParameters.end())
	{
		if (it->second.buffer_ptr == v_ptr)
		{
			printf("WARNNING!! ReplaceOrAddBufferPtr ==> Same buffer pointer is used!");
			return false;
		}
		else
		{
			VMSAFE_DELETEARRAY(it->second.buffer_ptr);
			g_mapCustomParameters.erase(it);
		}
	}

	_gp_lobj_buffer buf;
	buf.elements = num_elements;
	buf.dtype = dtype;
	buf.buffer_ptr = new byte[buf.dtype.type_bytes * num_elements];
	memcpy(buf.buffer_ptr, v_ptr, buf.dtype.type_bytes * num_elements);
	g_mapCustomParameters[param_name] = buf;
	return true;
}

bool __GetGpuManagerParameters(const string& param_name, const data_type& dtype, void** v_ptr, int* num_elements)
{
	*v_ptr = NULL;
	auto itrValue = g_mapCustomParameters.find(param_name);
	if (itrValue == g_mapCustomParameters.end())
		return false;
	if (itrValue->second.dtype != dtype) return false;
	*v_ptr = itrValue->second.buffer_ptr;
	if (num_elements) *num_elements = itrValue->second.elements;
	return true;
}