#pragma once

#include "resource.h"
//#include <d3dx9math.h>	// For Math and Structure
#include <DirectXMath.h>
#include <d3d11.h>
#include <d3d11_3.h>
#include <d3d12.h>
//#include <d3dx11.h>

using namespace std;
using namespace vmmath;
using namespace vmobjects;
using namespace vmgpuinterface;
using namespace DirectX;

#define VMSAFE_RELEASE(p)			if(p) { (p)->Release(); (p)=NULL; }
#define GMERRORMESSAGE(a) ::MessageBoxA(NULL, a, NULL, MB_OK)
#define VMERRORMESSAGE(a) printf(a)

//#define USE_DX11_3