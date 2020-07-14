#pragma once
#include "VXGPUManager.h"

#include <math.h>
#include <d3d11.h>
#include <d3dx11.h>

void VxgDeviceSetting(ID3D11Device* pdx11Device, ID3D11DeviceContext* pdx11DeviceImmContext, DXGI_ADAPTER_DESC adapterDesc);
int VxgGetProgress();

bool VxgGenerateGpuResourcePRIMITIVE_VERTEX(const SVXGPUResourceDESC* pstGpuResourceDesc, const CVXVObjectPrimitive* pCObjectPrimitive, SVXGPUResourceArchive* pstGpuResource/*out*/, SVXLocalProgress* pstLocalProgress);
bool VxgGenerateGpuResourcePRIMITIVE_INDEX(const SVXGPUResourceDESC* pstGpuResourceDesc, const CVXVObjectPrimitive* pCObjectPrimitive, SVXGPUResourceArchive* pstGpuResource/*out*/, SVXLocalProgress* pstLocalProgress);
bool VxgGenerateGpuResourceCMM_TEXT(const SVXGPUResourceDESC* pstGpuResourceDesc, const CVXVObjectPrimitive* pCObjectPrimitive, SVXGPUResourceArchive* pstGpuResource/*out*/, SVXLocalProgress* pstLocalProgress);

bool VxgGenerateGpuResourceVOLUME(const SVXGPUResourceDESC* pstGpuResourceDesc, const CVXVObjectVolume* pCObjectVolume, SVXGPUResourceArchive* pstGpuResource/*out*/, SVXLocalProgress* pstLocalProgress);
bool VxgGenerateGpuResourceBRICKS(const SVXGPUResourceDESC* pstGpuResourceDesc, const CVXVObjectVolume* pCObjectVolume, SVXGPUResourceArchive* pstGpuResource/*out*/, SVXLocalProgress* pstLocalProgress);
bool VxgGenerateGpuResourceBRICKSMAP(const SVXGPUResourceDESC* pstGpuResourceDesc, const CVXVObjectVolume* pCObjectVolume, SVXGPUResourceArchive* pstGpuResource/*out*/, SVXLocalProgress* pstLocalProgress);
bool VxgGenerateGpuResourceBlockMinMax(const SVXGPUResourceDESC* pstGpuResourceDesc, const CVXVObjectVolume* pCObjectVolume, SVXGPUResourceArchive* pstGpuResource/*out*/, SVXLocalProgress* pstLocalProgress);

bool VxgGenerateGpuResourceFRAMEBUFFER(const SVXGPUResourceDESC* pstGpuResourceDesc, const CVXIObject* pCIObject, SVXGPUResourceArchive* pstGpuResource/*out*/, SVXLocalProgress* pstLocalProgress);
bool VxgGenerateGpuResourceOTF(const SVXGPUResourceDESC* pstGpuResourceDesc, const CVXTObject* pCTObject, SVXGPUResourceArchive* pstGpuResource/*out*/, SVXLocalProgress* pstLocalProgress);

bool VxgGenerateGpuResBinderAsView(const SVXGPUResourceArchive* pstGpuResource, EnumVXGResourceType eResourceType, SVXGPUResourceArchive* pstGpuResourceOut/*out*/);