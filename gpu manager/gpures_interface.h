#pragma once

#include "GpuManager/GpuManager.h"

using namespace std;
using namespace vmmath;
using namespace vmobjects;
using namespace vmgpuinterface;

//#include <string>
//#include <vector>
//#include <map>

__vmstatic bool __InitializeDevice();
__vmstatic bool __DeinitializeDevice();
__vmstatic bool __GetGpuMemoryBytes(uint* dedicatedGpuMemory, uint* freeMemory);
__vmstatic bool __GetDeviceInformation(void* devInfo, const string& devSpecification);
__vmstatic ullong __GetUsedGpuMemorySizeBytes();
__vmstatic bool __UpdateGpuResource(GpuRes& gres);
__vmstatic int __UpdateGpuResourcesBySrcID(const int src_id, vector<GpuRes>& gres_list);

__vmstatic bool __GenerateGpuResource(GpuRes& gres, LocalProgress* progress = NULL);

__vmstatic bool __ReleaseGpuResource(GpuRes& gres, const bool call_clearstate);
__vmstatic bool __ReleaseGpuResourcesBySrcID(const int src_id, const bool call_clearstate = true);
__vmstatic bool __ReleaseAllGpuResources();

__vmstatic bool __SetGpuManagerParameters(const string& param_name, const data_type& dtype, const void* v_ptr, const int num_elements);
__vmstatic bool __GetGpuManagerParameters(const string& param_name, const data_type& dtype, void** v_ptr, int* num_elements = NULL);