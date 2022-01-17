#pragma once

#include "VimCommon.h"

//#include <unordered_map>
/**
 * @file GpuManager.h
 * @brief module에서 사용될 GPU resource를 공통으로 관리하기 위한 interface를 모은 헤더 파일
 * @section Include & Link 정보
 *		- Include : GpuManager.h, VimCommon.h
 *		- Library : GpuManager.lib, VimCommon.lib
 *		- Linking Binary : GpuManager.dll, 그 외 동적 dll 연동을 위한 resource manager dll files (각각의 SDK 용), VimCommon.dll
 */

 /**
  * @package vmgpuinterface
  * @brief GPU resource 관리를 위한 enumeration, helpers 그리고 manager class를 모은 namespace
  */

/*! Framework에 등록된 GPU SDK 종류*/
enum EvmGpuSdkType {
	GpuSdkTypeUNDEFINED = 0,/*!< 정의되지 않음 */
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
		RTYPE_UNDEFINED = 0,/*!< 정의되지 않음 */
		RTYPE_BUFFER,
		RTYPE_TEXTURE1D,
		RTYPE_TEXTURE2D,
		RTYPE_TEXTURE3D
	};

	enum DesType {
		DTYPE_UNDEFINED = 0,/*!< 정의되지 않음 */
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
		std::map<std::string, uint> options;
		std::map<std::string, double> res_dvalues;
		std::map<DesType, void*> alloc_res_ptrs;
	};

	__vmstaticclass VmGpuManager
	{
	public:
		VmGpuManager(const EvmGpuSdkType sdk_type, const string& module_file);
		~VmGpuManager();

		/*!
		 * @brief VmGpuManager 의 GPU SDK 확인
		 */
		EvmGpuSdkType GetGpuManagerSDK();
		/*!
		 * @brief VmGpuManager에 등록된 SDK의 device 및 이와 관련된 정보를 얻는 함수
		 * @param dev_info_ptr [out] \n void \n device information이 저장될 void 포인터
		 * @param dev_specifier [in] \n string \n 얻어올 device information의 식별을 위한 string
		 * @return bool \n 유효한 device 정보에 대한 pointer를 얻으면 true, 그렇지 않으면 false 반환
		 * @remarks
		 * GpuInterfaces 를 구현한 개발자가 정의한 dev_specifier 와 해당 void pointer가 가리키는 자료구조를 사전에 알고 있어야 함
		 */
		bool GetDeviceInformation(void* dev_info_ptr, const string& dev_specifier);
		/*!
		 * @brief GPU memory state를 얻는 함수
		 * @param dedicated_gpu_mem_bytes [out] \n ullong \n GPU 전용 memory 크기(bytes)를 저장할 unit의 포인터
		 * @param free_gpu_mem [out] \n ullong \n 현재 사용 가능한 GPU memory 크기(bytes)를 저장할 unit의 포인터
		 * @return bool \n 메모리를 얻는 것이 성공하면 true, 그렇지 않으면 false 반환
		 */
		bool GetGpuCurrentMemoryBytes(ullong* dedicated_gpu_mem_bytes/*out*/, ullong* free_gpu_mem/*out*/);
		/*!
		 * @brief 현재 Framework에서 사용하고 있는 resource 중 GPU resource로 등록된 크기를 얻음
		 * @return uint \n Framework에서 사용하고 있는 resource 중 GPU resource로 등록된 크기(bytes) 반환
		 * @remarks 모든 GPU SDK에 대하여 동작
		 */
		ullong GetUsedGpuMemorySizeBytes();
		/*!
		 * @brief GPU resource로 등록된 자료구조를 얻음
		 * @param GpuRes [in/out] \n gres \n 내부의 res_name 및 vm_src_id 으로 정의되어 있는 resouece 를 저장
		 * @return uint \n GPU resource로 등록된 자료구조를 성공적으로 얻으면 true, 그렇지 않으면 false 반환
		 * @remarks 모든 GPU SDK에 대하여 동작
		 */
		bool UpdateGpuResource(GpuRes& gres/*in-out*/);
		/*!
		 * @brief 해당 VmObject 와 관련된 모든 GPU resource를 얻음
		 * @param src_id [in] \n int \n VmObject의 ID
		 * @param gres_list [out] \n vector<SVXGPUResourceArchive*> \n 해당 VXObject 와 관련된 모든 GPU resource가 vector list로 저장
		 * @return uint \n GPU resource로 등록된 자료구조의 개수
		 * @remarks 모든 GPU SDK에 대하여 동작
		 */
		int UpdateGpuResourcesBySrcID(const int src_id, vector<GpuRes>& gres_list/*out*/);
		
		/*!
		 * @brief 윈도우 핸들로부터 DXGI 리소스 설정 (Swapchain). 윈도우 리사이즈 시에도 호출.
		 * @param ppBackBuffer [out] \n void \n Swapchain 에 연결된 Backbuffer (Texture2D) 를 가리키는 Pointer
		 * @param ppRTView [out] \n void \n *ppBackBuffer 로부터 만들어진 Rendertarget View Pointer
		 * @return bool \n GPU resource로 등록된 자료구조를 성공적으로 얻으면 true, 그렇지 않으면 false 반환
		 * @remarks DX11 GPU SDK에 대하여 동작
		 */
		bool UpdateDXGI(void** ppBackBuffer, void** ppRTView, const HWND hwnd, const int w, const int h);
		bool PresentBackBuffer(const HWND hwnd);
		bool ReleaseDXGI(const HWND hwnd);
		bool ReleaseAllDXGIs();

		/*!
		 * @brief GPU resource를 생성 및 등록하는 함수
		 * @param gres [in/out] \n GpuRes \n 내부의 res_name 및 vm_src_id 으로 정의되어 있는 resouece 를 저장
		 * @param progress [out](optional) \n LocalProgress \n
		 * 함수가 진행되는 progress 정보를 포함하는 LocalProgress 의 포인터 \n
		 * 기본값은 NULL이며, NULL이면 사용 안 함.
		 * @return bool \n GPU resource 가 생성 및 등록 완료되면 true, 그렇지 않으면 false 반환
		 * @remarks 모든 GPU SDK에 대하여 동작 \n
		 * 해당 GPU SDK의 resource 관리 module 정책에 따라 GPU resource가 생성됨.
		 */
		bool GenerateGpuResource(GpuRes& gres/*in-out*/, LocalProgress* progress = NULL);

		/*!
		 * @brief GPU resource에 대한 description이 가리키는 GPU resource를 해제하는 함수
		 * @param gres [in] \n GpuRes \n GPU resource 의 descriptor
		 * @param call_clearstate [in] \n bool \n resource 해제 후 ClearState 호출 여부
		 * @return bool \n GPU resource 의 해제가 성공하면 true, 그렇지 않으면 false 반환
		 * @remarks 모든 GPU SDK에 대하여 동작
		 */
		bool ReleaseGpuResource(GpuRes& gres, const bool call_clearstate = true);
		/*!
		 * @brief 해당 VmObject와 관련된 모든 GPU resource를 해제하는 함수
		 * @param src_id [in] \n int \n VmObject의 ID
		 * @param call_clearstate [in] \n bool \n resource 해제 후 ClearState 호출 여부
		 * @return bool \n GPU resource 의 해제가 성공하면 true, 그렇지 않으면 false 반환
		 * @remarks 모든 GPU SDK에 대하여 동작
		 */
		bool ReleaseGpuResourcesBySrcID(const int src_id, const bool call_clearstate = true);
		/*!
		 * @brief 현재 등록된 모든 GPU resource를 해제하는 함수
		 * @return bool \n GPU resource 의 해제가 성공하면 true, 그렇지 않으면 false 반환
		 * @remarks 모든 GPU SDK에 대하여 동작
		 */
		bool ReleaseAllGpuResources();

		/*!
		* @brief GPU Manager 에 특정 파라미터(함수)를 넘기는 일반 함수
		* @param param_name [in] \n string \n 파라미터 이름
		* @param dtype [in] \n data_type \n data type
		* @param v_ptr [in] \n void \n 파라미터 값을 담고 있는 void 의 포인터
		 * @return bool \n GPU Manager 에서 정의된 파라미터 이름으로 넘기면 true, 그렇지 않으면 false
		 * @remarks 모든 GPU SDK에 대하여 동작
		 */
		bool SetGpuManagerParameters(const string& param_name, const data_type& dtype, const void* v_ptr, const int num_elements);

		/*!
		* @brief GPU Manager 에 특정 파라미터(함수)를 넘기는 일반 함수
		* @param v_ptr [out] \n void \n 파라미터 값을 담고 있을 void 의 포인터
		* @param param_name [in] \n string \n 파라미터 이름
		* @param dtype [in] \n data_type \n 파라미터data type
		* @return bool \n GPU Manager 에서 정의된 파라미터 이름으로 넘기면 true, 그렇지 않으면 false
		* @remarks 모든 GPU SDK에 대하여 동작
		*/
		bool GetGpuManagerParameters(const string& param_name, const data_type& dtype, void* v_pt, int* num_elements = NULL);
	};
}