#pragma once
#include "../../EngineCores/CommonUnits/VimCommon.h"
//#include "VXDX11Helper.h"
#include "gpures_helper_old.h"

#define PPL_USE

#include <process.h>
#include <ppl.h>
using namespace Concurrency;	// for PPL

struct RenderingObject
{
	VmVObjectPrimitive* pCMesh;
	bool is_wireframe;
	bool use_vertex_color;
	bool show_outline;
	vmfloat4 f4Color;
	int iNumSelfTransparentRenderPass;

	RenderingObject()
	{
		show_outline = false;
	}
};

using namespace std;
using namespace vmmath;
using namespace vmobjects;
using namespace fncontainer;
using namespace vmgpuinterface;

extern void RegisterVolumeRes(VmVObjectVolume* vol_obj, VmTObject* tobj, VmLObject* lobj, VmGpuManager* pCGpuManager, ID3D11DeviceContext* pdx11DeviceImmContext,
	map<int, VmObject*>& mapAssociatedObjects, map<int, GpuRes>& mapGpuRes, LocalProgress* progress);

int RTTandLayersToLayersCS(ID3D11DeviceContext* pdx11DeviceImmContext, uint uiNumGridX, uint uiNumGridY
	, ID3D11ShaderResourceView* pdx11SRV_RenderOuts[NUM_TEXRT_LAYERS], ID3D11ShaderResourceView* pdx11SRV_DepthCSs[NUM_TEXRT_LAYERS], int iCountMerging
	, ID3D11UnorderedAccessView* pUAV_Merge_PingpongCSs[2], ID3D11ShaderResourceView* pSRV_Merge_PingpongCSs[2]
	, grd_helper_legacy::GpuDX11CommonParameters* pdx11CommonParams, ID3D11ComputeShader** ppdx11MergeCSs[NUMSHADERS_MERGE_CS]
	, ID3D11ShaderResourceView* pdx11SRV_4NULLs[NUM_TEXRT_LAYERS]
	, ID3D11ShaderResourceView* pdx11SRV_2NULLs[2], ID3D11RenderTargetView* pdx11RTV_2NULLs[2], int iMergeLevel
	);

bool RenderVrCommonCS(VmFnContainer* _fncontainer,
	VmGpuManager* pCGpuManager,
	grd_helper_legacy::GpuDX11CommonParameters* pdx11CommonParams,
	ID3D11ComputeShader** ppdx11CS_VRs[NUMSHADERS_VR_CS],
	ID3D11ComputeShader** ppdx11CS_Merges[NUMSHADERS_MERGE_CS],
	ID3D11VertexShader* pdx11VS_ProxyRect, ID3D11InputLayout* pdx11IL_ProxyRect,
	ID3D11Buffer* pdx11BufProxyRect,
	bool bIsShadowStage, LocalProgress* pstLocalProgress, double* pdRunTime);

bool RenderSrCommonCS(VmFnContainer* _fncontainer,
	VmGpuManager* pCGpuManager,
	grd_helper_legacy::GpuDX11CommonParameters* pdx11CommonParams,
	ID3D11InputLayout* pdx11ILs[NUMINPUTLAYOUTS],
	ID3D11VertexShader** ppdx11VS_CommonUsages[NUMSHADERS_SR_VS],
	ID3D11VertexShader** ppdx11VS_BiasZs[NUMSHADERS_BIASZ_SR_VS],
	ID3D11PixelShader** ppdx11PS_SRs[NUMSHADERS_SR_PS],
	ID3D11GeometryShader** ppdx11GS[NUMSHADERS_GS],
	ID3D11ComputeShader** ppdx11CS_Merges[NUMSHADERS_MERGE_CS],
	ID3D11ComputeShader** ppdx11CS_Outline,
	ID3D11Buffer* pdx11BufProxyRect,
	LocalProgress* pstLocalProgress,
	double* pdRunTime);

bool RenderSrOnPlane(VmFnContainer* _fncontainer,
	VmGpuManager* pCGpuManager,
	grd_helper_legacy::GpuDX11CommonParameters* pdx11CommonParams,
	ID3D11InputLayout* pdx11ILs[NUMINPUTLAYOUTS],
	ID3D11VertexShader** ppdx11CommonVSs[NUMSHADERS_SR_VS],
	ID3D11VertexShader** ppdx11PlaneVSs[NUMSHADERS_PLANE_SR_VS],
	ID3D11PixelShader** ppdx11PSs[NUMSHADERS_SR_PS],
	ID3D11ComputeShader** ppdx11CS_Merges[NUMSHADERS_MERGE_CS],
	LocalProgress* pLocalProgress,
	double* pdRunTime);

template <typename T> bool fncont_getparamvalue(fncontainer::VmFnContainer* _fncontainer, T* _value, const std::string& _key)
{
	auto it = _fncontainer->vmparams.find(_key);
	if (it == _fncontainer->vmparams.end()) return false;
	*_value = *(T*)it->second;
	return true;
}

#define VMERRORMESSAGE(a) printf(a)