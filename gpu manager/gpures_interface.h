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

// (API v76) ABI HANDSHAKE. Returns the VimCommon __VERSION this DLL was COMPILED against -- i.e. the
// layout of VmActor / VmLight / GpuRes and the fnParams contract it expects. GpuManager compares it to
// its own at load and refuses a mismatch (hard gate, user directive O3): before this, a stale renderer
// DLL simply rendered wrong/unlit with no diagnostic, which on a medical viewer is the quiet version
// of a crash. Absence of the export == a DLL older than the handshake == also a mismatch.
// Returns a static string literal (no allocation crosses the DLL boundary).
__vmstatic const char* __GetVimCommonVersion();

// Companion sizeof-based ABI fingerprint (see VimCommon.h VimCommonLayoutSig). Absent on a renderer older
// than this handshake -> GpuManager reads 0 -> mismatch -> disabled, same policy as a missing version.
__vmstatic unsigned int __GetVimCommonLayoutSig();

__vmstatic bool __InitializeDevice();
__vmstatic bool __DeinitializeDevice();
__vmstatic bool __GetGpuMemoryBytes(uint32_t* dedicatedGpuMemory, uint32_t* freeMemory);
__vmstatic bool __GetDeviceInformation(void* devInfo, const string& devSpecification);
__vmstatic uint64_t __GetUsedGpuMemorySizeBytes();
// Profiling breakdown of every LIVE GPU resource this renderer holds (returns the grand total in bytes).
// The maps are ACCUMULATED into (not cleared): the caller may merge several renderer instances into one
// report, so the caller owns the clearing. Any out-pointer may be NULL to skip that aggregation.
//   bytes/count_by_type     : keyed by D3D dimension ("BUFFER" / "TEXTURE1D" / "TEXTURE2D" / "TEXTURE3D")
//   bytes/count_by_category : keyed by engine-semantic category derived from res_name conventions
//                             ("VOLUME", "OTF", "MESH_GEOMETRY", "MATERIAL_TEXTURE", "RENDER_TARGET",
//                              "STAGING_READBACK", "BVH", "VXGI", "PARTICLE", "ETC")
// __GetUsedGpuMemorySizeBytes() is implemented on top of this function (single source of truth).
__vmstatic uint64_t __ProfileGpuResources(
	std::map<std::string, uint64_t>* bytes_by_type,
	std::map<std::string, uint64_t>* bytes_by_category,
	std::map<std::string, int>* count_by_type,
	std::map<std::string, int>* count_by_category);
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