#pragma once
#include "../../EngineCores/GpuManager/GpuManager.h"

#include <math.h>
#include <d3d11.h>
#include <d3dx11.h>

using namespace vmgpuinterface_v1;

void VmDeviceSetting(ID3D11Device* pdx11Device, ID3D11DeviceContext* pdx11DeviceImmContext, DXGI_ADAPTER_DESC adapterDesc);

bool VmGenerateGpuResourcePRIMITIVE_VERTEX(const GpuResDescriptor* pGpuResDesc, const VmVObjectPrimitive* pCObjectPrimitive, GpuResource* pstGpuResource/*out*/, LocalProgress* pstLocalProgress);
bool VmGenerateGpuResourcePRIMITIVE_INDEX(const GpuResDescriptor* pGpuResDesc, const VmVObjectPrimitive* pCObjectPrimitive, GpuResource* pstGpuResource/*out*/, LocalProgress* pstLocalProgress);
bool VmGenerateGpuResourceCMM_TEXT(const GpuResDescriptor* pGpuResDesc, const VmVObjectPrimitive* pCObjectPrimitive, GpuResource* pstGpuResource/*out*/, LocalProgress* pstLocalProgress);

bool VmGenerateGpuResourceVOLUME(const GpuResDescriptor* pGpuResDesc, const VmVObjectVolume* pCObjectVolume, GpuResource* pstGpuResource/*out*/, LocalProgress* pstLocalProgress);
bool VmGenerateGpuResourceBRICKS(const GpuResDescriptor* pGpuResDesc, const VmVObjectVolume* pCObjectVolume, GpuResource* pstGpuResource/*out*/, LocalProgress* pstLocalProgress);
bool VmGenerateGpuResourceBLOCKSMAP(const GpuResDescriptor* pGpuResDesc, const VmVObjectVolume* pCObjectVolume, GpuResource* pstGpuResource/*out*/, LocalProgress* pstLocalProgress);
bool VmGenerateGpuResourceBlockMinMax(const GpuResDescriptor* pGpuResDesc, const VmVObjectVolume* pCObjectVolume, GpuResource* pstGpuResource/*out*/, LocalProgress* pstLocalProgress);

bool VmGenerateGpuResourceFRAMEBUFFER(const GpuResDescriptor* pGpuResDesc, const VmIObject* pCIObject, GpuResource* pstGpuResource/*out*/, LocalProgress* pstLocalProgress);
bool VmGenerateGpuResourceOTF(const GpuResDescriptor* pGpuResDesc, const VmTObject* pCTObject, GpuResource* pstGpuResource/*out*/, LocalProgress* pstLocalProgress);

bool VmGenerateGpuResBinderAsView(const GpuResource* pstGpuResource, EvmGpuResourceType res_type, GpuResource* pstGpuResourceOut/*out*/);