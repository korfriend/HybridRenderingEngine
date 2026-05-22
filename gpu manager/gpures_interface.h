#pragma once

#include "GpuManager.h"

#include <functional>

using namespace std;
using namespace vmmath;
using namespace vmobjects;
using namespace vmgpuinterface;

//#include <string>
//#include <vector>
//#include <map>

__vmstatic bool __InitializeDevice();
__vmstatic bool __DeinitializeDevice();
__vmstatic bool __GetGpuMemoryBytes(uint32_t* dedicatedGpuMemory, uint32_t* freeMemory);
__vmstatic bool __GetDeviceInformation(void* devInfo, const string& devSpecification);
__vmstatic uint64_t __GetUsedGpuMemorySizeBytes();
__vmstatic bool __UpdateGpuResource(GpuRes& gres);
__vmstatic int __UpdateGpuResourcesBySrcID(const int src_id, vector<GpuRes>& gres_list);

__vmstatic bool __UpdateDXGI(void** ppBackBuffer, void** ppRTView, const HWND hwnd, const int w, const int h);
__vmstatic bool __PresentBackBuffer(const HWND hwnd);
__vmstatic bool __ReleaseDXGI(const HWND hwnd);
__vmstatic bool __ReleaseAllDXGIs();

__vmstatic bool __GenerateGpuResource(GpuRes& gres, LocalProgress* progress = NULL);

__vmstatic bool __ReleaseGpuResource(GpuRes& gres, const bool call_clearstate);
__vmstatic bool __ReleaseGpuResourcesBySrcID(const int src_id, const bool call_clearstate = true);
__vmstatic bool __ReleaseAllGpuResources();

// Hooks invoked by __ReleaseGpuResourcesBySrcID(src_id) so that modules holding
// per-iobj caches outside g_mapVmResources (e.g. D2D wrappers keyed by iobj id)
// can release them in lockstep. Intended for intra-DLL use only.
void RegisterPerSrcIdReleaseHook(std::function<void(int)> hook);
void ClearPerSrcIdReleaseHooks();

__vmstatic bool __SetGpuManagerParameters(const string& param_name, const data_type& dtype, const void* v_ptr, const int num_elements);
__vmstatic bool __GetGpuManagerParameters(const string& param_name, const data_type& dtype, void** v_ptr, int* num_elements = NULL);