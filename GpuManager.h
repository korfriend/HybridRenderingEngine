#pragma once

#include "VimCommon.h"

/**
 * @file GpuManager.h
 * @brief Header collecting the interfaces for the shared management of GPU resources used by modules
 * @section Include & Link Information
 *		- Include : GpuManager.h, VimCommon.h
 *		- Library : GpuManager.lib, VimCommon.lib
 *		- Linking Binary : GpuManager.dll, the resource-manager dll files for dynamic dll integration (one per SDK), VimCommon.dll
 */

 /**
  * @package vmgpuinterface
  * @brief Namespace collecting the enumerations, helpers, and manager class for GPU resource management
  */

/*! Kinds of GPU SDKs registered with the framework */
enum EvmGpuSdkType {
	GpuSdkTypeUNDEFINED = 0,/*!< Undefined */
	GpuSdkTypeDX11,/*!< DirectX 11 */
	GpuSdkTypeDX12,/*!< DirectX 12 */
	GpuSdkTypeCUDA,/*!< CUDA */
	GpuSdkTypeOPENGL,/*!< OPENGL */
	GpuSdkTypeOPENCL/*!< OPENCL */
};

namespace vmgpuinterface
{
	using namespace std;
	using namespace vmobjects;
	//============================
	// GPU COM Functions 
	//============================

	enum GpuResType {
		RTYPE_UNDEFINED = 0,/*!< Undefined */
		RTYPE_BUFFER,
		RTYPE_TEXTURE1D,
		RTYPE_TEXTURE2D,
		RTYPE_TEXTURE3D
	};

	enum DesType {
		DTYPE_UNDEFINED = 0,/*!< Undefined */
		DTYPE_RES,
		DTYPE_RTV,
		DTYPE_DSV,
		DTYPE_SRV,
		DTYPE_UAV,
		DTYPE_RES_AUX // for DX12
	};

	struct GpuRes
	{
		int vm_src_id;
		std::string res_name;
		GpuResType rtype;
		std::map<std::string, uint32_t> options;
		vmobjects::VmParamMap<std::string, std::any> res_values;
		std::map<DesType, void*> alloc_res_ptrs;
	};

	__vmstaticclass VmGpuManager
	{
	public:
		VmGpuManager(const EvmGpuSdkType sdk_type, const string& module_file);
		~VmGpuManager();

		/*!
		 * @brief Returns the GPU SDK of the VmGpuManager
		 */
		EvmGpuSdkType GetGpuManagerSDK();
		/*!
		 * @brief Retrieves the device registered with the VmGpuManager's SDK and its related information
		 * @param dev_info_ptr [out] \n void \n void pointer to receive the device information
		 * @param dev_specifier [in] \n string \n string identifying which device information to retrieve
		 * @return bool \n Returns true if a pointer to valid device information is obtained, false otherwise
		 * @remarks
		 * The caller must know in advance the dev_specifier and the structure pointed to by the void pointer, as defined by the developer who implemented GpuInterfaces
		 */
		bool GetDeviceInformation(void* dev_info_ptr, const string& dev_specifier);
		/*!
		 * @brief Retrieves the GPU memory state
		 * @param dedicated_gpu_mem_bytes [out] \n uint64_t \n pointer to a unit that receives the dedicated GPU memory size (bytes)
		 * @param free_gpu_mem [out] \n uint64_t \n pointer to a unit that receives the currently available GPU memory size (bytes)
		 * @return bool \n Returns true if the memory query succeeds, false otherwise
		 */
		bool GetGpuCurrentMemoryBytes(uint64_t* dedicated_gpu_mem_bytes/*out*/, uint64_t* free_gpu_mem/*out*/);
		/*!
		 * @brief Returns the total size of the framework's in-use resources that are registered as GPU resources
		 * @return uint32_t \n Returns the size (bytes) of the framework's in-use resources that are registered as GPU resources
		 * @remarks Works for all GPU SDKs
		 */
		uint64_t GetUsedGpuMemorySizeBytes();
		// NOTE (API v75): the reported bytes now include texture array slices and full mip chains
		// (previously only mip 0 of slice 0 was counted). Totals therefore grow versus older builds --
		// that is the accounting becoming accurate, not an allocation regression. Do not hard-code
		// thresholds against pre-v75 figures.
		/*!
		 * @brief Profiling breakdown of every LIVE GPU resource the renderer holds; returns the grand total in bytes.
		 * @param bytes_by_type [in-out](optional) accumulated bytes keyed by D3D dimension ("BUFFER"/"TEXTURE1D"/"TEXTURE2D"/"TEXTURE3D")
		 * @param bytes_by_category [in-out](optional) accumulated bytes keyed by engine-semantic category
		 *        ("VOLUME","OTF","MESH_GEOMETRY","MATERIAL_TEXTURE","RENDER_TARGET","STAGING_READBACK","BVH","VXGI","PARTICLE","ETC")
		 * @param count_by_type / count_by_category [in-out](optional) resource counts per key
		 * @return uint64_t total bytes of live GPU resources; 0 when the renderer DLL does not expose the profiler
		 * @remarks The maps are ACCUMULATED into (never cleared here) so several VmGpuManager instances can be
		 *          merged into one report -- the caller owns the clearing. The export is optional: an older
		 *          renderer DLL without __ProfileGpuResources still loads, and this method then returns 0.
		 */
		uint64_t ProfileGpuResources(std::map<std::string, uint64_t>* bytes_by_type = NULL,
			std::map<std::string, uint64_t>* bytes_by_category = NULL,
			std::map<std::string, int>* count_by_type = NULL,
			std::map<std::string, int>* count_by_category = NULL);
		/*!
		 * @brief Retrieves the structure registered as a GPU resource
		 * @param GpuRes [in/out] \n gres \n stores the resource identified by its res_name and vm_src_id
		 * @return uint32_t \n Returns true if the GPU-resource structure is retrieved successfully, false otherwise
		 * @remarks Works for all GPU SDKs
		 */
		bool UpdateGpuResource(GpuRes& gres/*in-out*/);
		/*!
		 * @brief Retrieves all GPU resources associated with the given VmObject
		 * @param src_id [in] \n int \n ID of the VmObject
		 * @param gres_list [out] \n vector<GPUResourceArchive*> \n receives all GPU resources associated with the given VObject as a vector list
		 * @return uint32_t \n Number of structures registered as GPU resources
		 * @remarks Works for all GPU SDKs
		 */
		int UpdateGpuResourcesBySrcID(const int src_id, vector<GpuRes>& gres_list/*out*/);
		
		/*!
		 * @brief Sets up DXGI resources (swapchain) from a window handle. Also called on window resize.
		 * @param ppBackBuffer [out] \n void \n pointer to the backbuffer (Texture2D) connected to the swapchain
		 * @param ppRTView [out] \n void \n render-target view pointer created from *ppBackBuffer
		 * @return bool \n Returns true if the GPU-resource structure is obtained successfully, false otherwise
		 * @remarks Works for the DX11 GPU SDK
		 */
		bool UpdateDXGI(void** ppBackBuffer, void** ppRTView, const HWND hwnd, const int w, const int h);
		bool PresentBackBuffer(const HWND hwnd);
		bool ReleaseDXGI(const HWND hwnd);
		bool ReleaseAllDXGIs();

		/*!
		 * @brief Creates and registers a GPU resource
		 * @param gres [in/out] \n GpuRes \n stores the resource identified by its res_name and vm_src_id
		 * @param progress [out](optional) \n LocalProgress \n
		 * Pointer to a LocalProgress carrying the function's progress information \n
		 * The default is NULL; if NULL, it is not used.
		 * @return bool \n Returns true once the GPU resource is created and registered, false otherwise
		 * @remarks Works for all GPU SDKs \n
		 * The GPU resource is created according to the resource-management module policy of the given GPU SDK.
		 */
		bool GenerateGpuResource(GpuRes& gres/*in-out*/, LocalProgress* progress = NULL);

		/*!
		 * @brief Releases the GPU resource pointed to by the given GPU-resource description
		 * @param gres [in] \n GpuRes \n Descriptor of the GPU resource
		 * @param call_clearstate [in] \n bool \n Whether to call ClearState after releasing the resource
		 * @return bool \n Returns true if the GPU resource is released successfully, false otherwise
		 * @remarks Works for all GPU SDKs
		 */
		bool ReleaseGpuResource(GpuRes& gres, const bool call_clearstate = true);
		/*!
		 * @brief Releases all GPU resources associated with the given VmObject
		 * @param src_id [in] \n int \n ID of the VmObject
		 * @param call_clearstate [in] \n bool \n Whether to call ClearState after releasing the resource
		 * @return bool \n Returns true if the GPU resource is released successfully, false otherwise
		 * @remarks Works for all GPU SDKs
		 */
		bool ReleaseGpuResourcesBySrcID(const int src_id, const bool call_clearstate = true);
		/*!
		 * @brief Releases all currently registered GPU resources
		 * @return bool \n Returns true if the GPU resource is released successfully, false otherwise
		 * @remarks Works for all GPU SDKs
		 */
		bool ReleaseAllGpuResources();

		/*!
		* @brief General function for passing a specific parameter (function) to the GPU Manager
		* @param param_name [in] \n string \n Parameter name
		* @param dtype [in] \n data_type \n data type
		* @param v_ptr [in] \n void \n void pointer holding the parameter value
		 * @return bool \n Returns true if passed with a parameter name defined in the GPU Manager, false otherwise
		 * @remarks Works for all GPU SDKs
		 */
		bool SetGpuManagerParameters(const string& param_name, const data_type& dtype, const void* v_ptr, const int num_elements);

		/*!
		* @brief General function for passing a specific parameter (function) to the GPU Manager
		* @param v_ptr [out] \n void \n void pointer that will hold the parameter value
		* @param param_name [in] \n string \n Parameter name
		* @param dtype [in] \n data_type \n Parameter data type
		* @return bool \n Returns true if passed with a parameter name defined in the GPU Manager, false otherwise
		* @remarks Works for all GPU SDKs
		*/
		bool GetGpuManagerParameters(const string& param_name, const data_type& dtype, void* v_pt, int* num_elements = NULL);
	};
}