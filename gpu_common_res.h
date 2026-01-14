#pragma once
#include "GpuManager.h"
#include "resource.h"
#include <DirectXMath.h>
#include <d3d11.h>
#include <d3d11_3.h>
#include <d3d12.h>

using namespace std;
using namespace vmmath;
using namespace vmobjects;
using namespace vmgpuinterface;
using namespace DirectX;

#define VMSAFE_RELEASE(p) if(p) { (p)->Release(); (p)=NULL; }
#define VMERRORMESSAGE(a) {vmlog::LogErr(a);}

#define K_NUM_SLICER 2
#define K_NUM_3D 8

enum class GpuhelperResType {
	VERTEX_SHADER = 0,
	PIXEL_SHADER,
	GEOMETRY_SHADER,
	COMPUTE_SHADER,
	DEPTHSTENCIL_STATE,
	RASTERIZER_STATE,
	SAMPLER_STATE,
	INPUT_LAYOUT,
	BLEND_STATE,

	CONST_BUFFER,
	BUFFER1D,
	BUFFER2D,
	BUFFER3D,
	TEXTURE1D,
	TEXTURE2D,
	TEXTURE3D,
};


inline void __check_and_release(GpuhelperResType res_type, ID3D11DeviceChild* res)
{
	// I think 'res->Release()' is enough for releasing GPU memory because
	// 	   COM system manages its memory inside the data structure and data/type identification
	//res->Release();
	switch (res_type)
	{
	case GpuhelperResType::VERTEX_SHADER:			((ID3D11VertexShader*)res)->Release(); break;
	case GpuhelperResType::PIXEL_SHADER:			((ID3D11PixelShader*)res)->Release(); break;
	case GpuhelperResType::GEOMETRY_SHADER:		((ID3D11GeometryShader*)res)->Release(); break;
	case GpuhelperResType::COMPUTE_SHADER:		((ID3D11ComputeShader*)res)->Release(); break;
	case GpuhelperResType::DEPTHSTENCIL_STATE:	((ID3D11DepthStencilState*)res)->Release(); break;
	case GpuhelperResType::RASTERIZER_STATE:		((ID3D11RasterizerState2*)res)->Release(); break;
	case GpuhelperResType::SAMPLER_STATE:			((ID3D11SamplerState*)res)->Release(); break;
	case GpuhelperResType::INPUT_LAYOUT:			((ID3D11InputLayout*)res)->Release(); break;
	case GpuhelperResType::BLEND_STATE:			((ID3D11BlendState*)res)->Release(); break;
		
	case GpuhelperResType::CONST_BUFFER:
	case GpuhelperResType::BUFFER1D:
	case GpuhelperResType::BUFFER2D:
	case GpuhelperResType::BUFFER3D:
	case GpuhelperResType::TEXTURE1D:
	case GpuhelperResType::TEXTURE2D:
	case GpuhelperResType::TEXTURE3D:
	default:
		VMERRORMESSAGE("UNEXPECTED RESTYPE : ~GpuDX11CommonParameters");
	}
};

//#define DX11_3
//#define TEXTITEM_VERSION 230813
//#define TEXTITEM_VERSION 250904
#define TEXTITEM_VERSION 260114
struct TextItem
{
private:
	int version = 260114;
public:
	std::string textStr = "";
	std::string font = "";
	std::string alignment = ""; // CENTER, LEFT, RIGHT to the position
	float fontSize = 10.f; // Logical size of the font in DIP units. A DIP ("device-independent pixel") equals 1/96 inch.
	uint32_t color = 0xFFFFFFFF; //AABBGGRR
	float alpha = 1.f;
	bool isItalic = false;
	int fontWeight = 4; // 1 : thinest, 4 : regular, 7 : bold, 9 : maximum heavy
	int posScreenX = 0, posScreenY = 0; // 
	std::vector<int> userData;
};

#ifdef DX11_3
#define __ID3D11Device ID3D11Device3
#define __ID3D11DeviceContext ID3D11DeviceContext3
#define __DLLNAME "vismtv_inbuilt_renderergpudx.dll"
#elif defined(DX11_0)
#define __ID3D11Device ID3D11Device
#define __ID3D11DeviceContext ID3D11DeviceContext
#ifdef _DEBUG
#define __DLLNAME "vismtv_inbuilt_renderergpudx.dll"
#else
#define __DLLNAME "vismtv_inbuilt_renderergpudx11_0.dll"
#endif
#elif defined(DX10_0)
#define __ID3D11Device ID3D11Device
#define __ID3D11DeviceContext ID3D11DeviceContext
#ifdef _DEBUG
#define __DLLNAME "vismtv_inbuilt_renderergpudx.dll"
#else
#define __DLLNAME "vismtv_inbuilt_renderergpudx10_0.dll"
#endif
#endif