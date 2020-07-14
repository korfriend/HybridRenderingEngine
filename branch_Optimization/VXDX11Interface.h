#pragma once
#define VXGPUINTERFACE

#include "VXGPUManager.h"

__vxstatic bool VxgInitializeDevice();
__vxstatic bool VxgDeinitializeDevice();
__vxstatic bool VxgGetGpuMemoryBytes(uint* puiDedicatedGpuMemory, uint* puiFreeMemory);
__vxstatic bool VxgGetDeviceInformation(void* pvDevInfo, wstring strDevSpecification);
__vxstatic uint VxgGetUsedGpuMemorySizeKiloBytes();
__vxstatic bool VxgGetGpuResourceArchive(const SVXGPUResourceDESC* psvxGpuResourceDesc, SVXGPUResourceArchive* psvxGpuResource/*out*/);
__vxstatic uint VxgGetGpuResourceArchivesByObjectID(int iObjectID, vector<SVXGPUResourceArchive*>* pvtrGpuResources/*out*/);

__vxstatic bool VxgGenerateGpuResource(const SVXGPUResourceDESC* psvxGpuResourceDesc, CVXObject* pCObject, SVXLocalProgress* psvxLocalProgress = NULL);
__vxstatic bool VxgReplaceOrAddGpuResource(const SVXGPUResourceArchive* psvxGpuResource);

__vxstatic bool VxgReleaseGpuResource(const SVXGPUResourceDESC* psvxGpuResourceDesc, bool bCallClearState);
__vxstatic bool VxgReleaseGpuResourcesRelatedObject(int iObjectID, bool bCallClearState);
__vxstatic bool VxgReleaseGpuResourcesVObjects();
__vxstatic bool VxgReleaseAllGpuResources();

__vxstatic int VxgRetrenchGpuResourcesForStorage(uint uiRequiredStorageSizeMB, ullong ullMinimumTimeGap = 0, bool bIsTargetOnlyVObject = true); // Return # of Retrenched resources
__vxstatic bool VxgSetGpuManagerCustomParameters(wstring strParamName, void* pvParameter);
__vxstatic bool VxgGetGpuManagerCustomParameters(wstring strParamName, void** ppvParameter);
