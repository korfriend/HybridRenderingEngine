#pragma once

#include "../../EngineCores/GpuManager/GpuManager.h"

using namespace std;
using namespace vmmath;
using namespace vmobjects;
using namespace vmgpuinterface_v1;

__vmstatic bool VmInitializeDevice();
__vmstatic bool VmDeinitializeDevice();
__vmstatic bool VmGetGpuMemoryBytes(uint* puiDedicatedGpuMemory, uint* puiFreeMemory);
__vmstatic bool VmGetDeviceInformation(void* pvDevInfo, const string& strDevSpecification);
__vmstatic ullong VmGetUsedGpuMemorySizeBytes();
__vmstatic bool VmGetGpuResourceArchive(const GpuResDescriptor* pGpuResDesc, GpuResource* pGpuResource/*out*/);
__vmstatic uint VmGetGpuResourceArchivesByObjectID(const int iObjectID, vector<GpuResource*>* pvtrGpuResources/*out*/);

__vmstatic bool VmGenerateGpuResource(const GpuResDescriptor* gpu_desc, const VmObject* obj_ptr, GpuResource* gpu_archive/*out can be NULL*/, LocalProgress* progress);
__vmstatic bool VmReplaceOrAddGpuResource(const GpuResource* pGpuResource);

__vmstatic bool VmReleaseGpuResource(const GpuResDescriptor* pGpuResDesc, const bool bCallClearState);
__vmstatic bool VmReleaseGpuResourcesRelatedObject(const int iObjectID, const bool bCallClearState);
__vmstatic bool VmReleaseGpuResourcesVObjects();
__vmstatic bool VmReleaseAllGpuResources();

__vmstatic int VmRetrenchGpuResourcesForStorage(const uint uiRequiredStorageSizeMB, const ullong ullMinimumTimeGap = 0, const bool bIsTargetOnlyVObject = true); // Return # of Retrenched resources

__vmstatic bool VmSetGpuManagerParameters(const string& param_name, const data_type& dtype, const void* v_ptr, const int num_elements);
__vmstatic bool VmGetGpuManagerParameters(const string& param_name, const data_type& dtype, void** v_ptr, int* num_elements = NULL);