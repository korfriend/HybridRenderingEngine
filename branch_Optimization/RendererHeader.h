#pragma once
#include "VXEngineGlobalUnit.h"
#include "VXDX11Helper.h"

#define __BLOCKSIZE 8

// MERGE
#define __FB_UBUF_MERGEOUT 0
#define __FB_UAV_MERGEOUT 1
#define __FB_SRV_MERGEOUT 2
#define __UFB_MERGE_FB_COUNT 3


struct RWB_Output_MultiLayers	// CS //
{
	int arrayIntVisibilityRSA[NUM_MERGE_LAYERS]; // ARGB : high to low
	float arrayDepthBackRSA[NUM_MERGE_LAYERS];
	float arrayThicknessRSA[NUM_MERGE_LAYERS];
};

// VR
struct RWB_Output_RenderOut
{
	int iColor;
	float fDepth1stHit;
};

// VR
struct RWB_Output_ShadowMap
{
	float fDepthHits[MAXLEVEL]; 
	float fOpacity; 
};

bool RenderVrCommon(SVXModuleParameters* psvxModuleParameter,
	CVXGPUManager* pCGpuManager,
	GpuDX11CommonParameters* pdx11CommonParams,
	ID3D11ComputeShader** ppdx11CS_VRs[NUMSHADERS_VR],
	ID3D11ComputeShader** ppdx11CS_Merge[NUMSHADERS_MERGE],
	bool bIsShadowStage,  bool bSkipProcess,
	SVXLocalProgress* psvxLocalProgress,
	double* pdRunTime);

bool MergeSrVrCommon(SVXModuleParameters* psvxModuleParameter,
	CVXGPUManager* pCGpuManager,
	GpuDX11CommonParameters* pdx11CommonParams,
	ID3D11ComputeShader** ppdx11CS_Merge[NUMSHADERS_MERGE],
	SVXLocalProgress* psvxLocalProgress,
	double* pdRunTime);

bool RenderSrCommon(SVXModuleParameters* psvxModuleParameter,
	CVXGPUManager* pCGpuManager,
	GpuDX11CommonParameters* pdx11CommonParams,
	ID3D11InputLayout* pdx11ILs[NUMINPUTLAYOUTS],
	ID3D11VertexShader** ppdx11VSs[NUMSHADERS_SR_VS],
	ID3D11PixelShader** ppdx11PSs[NUMSHADERS_SR_PS],
	ID3D11ComputeShader** ppdx11CS_Merge[NUMSHADERS_MERGE],
	ID3D11Buffer* pdx11Bufs[3], //bool bIsShadowStage, 
	SVXLocalProgress* psvxLocalProgress,
	double* pdRunTime);

bool RenderSrOnPlane(SVXModuleParameters* pstModuleParameter,
	CVXGPUManager* pCGpuManager,
	GpuDX11CommonParameters* pdx11CommonParams,
	ID3D11InputLayout* pdx11ILs[NUMINPUTLAYOUTS],
	ID3D11VertexShader** ppdx11CommonVSs[NUMSHADERS_SR_VS],
	ID3D11VertexShader** ppdx11PlaneVSs[NUMSHADERS_PLANE_SR_VS],
	ID3D11PixelShader** ppdx11PSs[NUMSHADERS_SR_PS],
	ID3D11Buffer* pdx11Bufs[3], // 0: Proxy, 1:SI_VTX, 2:SI_IDX
	SVXLocalProgress* psvxLocalProgress,
	double* pdRunTime);