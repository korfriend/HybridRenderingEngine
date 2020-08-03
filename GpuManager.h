#pragma once

#include "VimCommon.h"

//#include <unordered_map>
/**
 * @file GpuManager.h
 * @brief module���� ���� GPU resource�� �������� �����ϱ� ���� interface�� ���� ��� ����
 * @section Include & Link ����
 *		- Include : GpuManager.h, VimCommon.h
 *		- Library : GpuManager.lib, VimCommon.lib
 *		- Linking Binary : GpuManager.dll, �� �� ���� dll ������ ���� resource manager dll files (������ SDK ��), VimCommon.dll
 */

 /**
  * @package vmgpuinterface
  * @brief GPU resource ������ ���� enumeration, helpers �׸��� manager class�� ���� namespace
  */

/*! Framework�� ��ϵ� GPU SDK ����*/
enum EvmGpuSdkType {
	GpuSdkTypeUNDEFINED = 0,/*!< ���ǵ��� ���� */
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
		RTYPE_UNDEFINED = 0,/*!< ���ǵ��� ���� */
		RTYPE_BUFFER,
		RTYPE_TEXTURE1D,
		RTYPE_TEXTURE2D,
		RTYPE_TEXTURE3D
	};

	enum DesType {
		DTYPE_UNDEFINED = 0,/*!< ���ǵ��� ���� */
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
		 * @brief VmGpuManager �� GPU SDK Ȯ��
		 */
		EvmGpuSdkType GetGpuManagerSDK();
		/*!
		 * @brief VmGpuManager�� ��ϵ� SDK�� device �� �̿� ���õ� ������ ��� �Լ�
		 * @param dev_info_ptr [out] \n void \n device information�� ����� void ������
		 * @param dev_specifier [in] \n string \n ���� device information�� �ĺ��� ���� string
		 * @return bool \n ��ȿ�� device ������ ���� pointer�� ������ true, �׷��� ������ false ��ȯ
		 * @remarks
		 * GpuInterfaces �� ������ �����ڰ� ������ dev_specifier �� �ش� void pointer�� ����Ű�� �ڷᱸ���� ������ �˰� �־�� ��
		 */
		bool GetDeviceInformation(void* dev_info_ptr, const string& dev_specifier);
		/*!
		 * @brief GPU memory state�� ��� �Լ�
		 * @param dedicated_gpu_mem_bytes [out] \n ullong \n GPU ���� memory ũ��(bytes)�� ������ unit�� ������
		 * @param free_gpu_mem [out] \n ullong \n ���� ��� ������ GPU memory ũ��(bytes)�� ������ unit�� ������
		 * @return bool \n �޸𸮸� ��� ���� �����ϸ� true, �׷��� ������ false ��ȯ
		 */
		bool GetGpuCurrentMemoryBytes(ullong* dedicated_gpu_mem_bytes/*out*/, ullong* free_gpu_mem/*out*/);
		/*!
		 * @brief ���� Framework���� ����ϰ� �ִ� resource �� GPU resource�� ��ϵ� ũ�⸦ ����
		 * @return uint \n Framework���� ����ϰ� �ִ� resource �� GPU resource�� ��ϵ� ũ��(bytes) ��ȯ
		 * @remarks ��� GPU SDK�� ���Ͽ� ����
		 */
		ullong GetUsedGpuMemorySizeBytes();
		/*!
		 * @brief GPU resource�� ��ϵ� �ڷᱸ���� ����
		 * @param GpuRes [in/out] \n gres \n ������ res_name �� vm_src_id ���� ���ǵǾ� �ִ� resouece �� ����
		 * @return uint \n GPU resource�� ��ϵ� �ڷᱸ���� ���������� ������ true, �׷��� ������ false ��ȯ
		 * @remarks ��� GPU SDK�� ���Ͽ� ����
		 */
		bool UpdateGpuResource(GpuRes& gres/*in-out*/);
		/*!
		 * @brief �ش� VmObject �� ���õ� ��� GPU resource�� ����
		 * @param src_id [in] \n int \n VmObject�� ID
		 * @param gres_list [out] \n vector<SVXGPUResourceArchive*> \n �ش� VXObject �� ���õ� ��� GPU resource�� vector list�� ����
		 * @return uint \n GPU resource�� ��ϵ� �ڷᱸ���� ���������� ������ true, �׷��� ������ false ��ȯ
		 * @remarks ��� GPU SDK�� ���Ͽ� ����
		 */
		int UpdateGpuResourcesBySrcID(const int src_id, vector<GpuRes>& gres_list/*out*/);
		/*!
		 * @brief GPU resource�� ���� �� ����ϴ� �Լ�
		 * @param gres [in/out] \n GpuRes \n ������ res_name �� vm_src_id ���� ���ǵǾ� �ִ� resouece �� ����
		 * @param progress [out](optional) \n LocalProgress \n
		 * �Լ��� ����Ǵ� progress ������ �����ϴ� LocalProgress �� ������ \n
		 * �⺻���� NULL�̸�, NULL�̸� ��� �� ��.
		 * @return bool \n GPU resource �� ���� �� ��� �Ϸ�Ǹ� true, �׷��� ������ false ��ȯ
		 * @remarks ��� GPU SDK�� ���Ͽ� ���� \n
		 * �ش� GPU SDK�� resource ���� module ��å�� ���� GPU resource�� ������.
		 */
		bool GenerateGpuResource(GpuRes& gres/*in-out*/, LocalProgress* progress = NULL);

		/*!
		 * @brief GPU resource�� ���� description�� ����Ű�� GPU resource�� �����ϴ� �Լ�
		 * @param gres [in] \n GpuRes \n GPU resource �� descriptor
		 * @param call_clearstate [in] \n bool \n resource ���� �� ClearState ȣ�� ����
		 * @return bool \n GPU resource �� ������ �����ϸ� true, �׷��� ������ false ��ȯ
		 * @remarks ��� GPU SDK�� ���Ͽ� ����
		 */
		bool ReleaseGpuResource(GpuRes& gres, const bool call_clearstate = true);
		/*!
		 * @brief �ش� VmObject�� ���õ� ��� GPU resource�� �����ϴ� �Լ�
		 * @param src_id [in] \n int \n VmObject�� ID
		 * @param call_clearstate [in] \n bool \n resource ���� �� ClearState ȣ�� ����
		 * @return bool \n GPU resource �� ������ �����ϸ� true, �׷��� ������ false ��ȯ
		 * @remarks ��� GPU SDK�� ���Ͽ� ����
		 */
		bool ReleaseGpuResourcesBySrcID(const int src_id, const bool call_clearstate = true);
		/*!
		 * @brief ���� ��ϵ� ��� GPU resource�� �����ϴ� �Լ�
		 * @return bool \n GPU resource �� ������ �����ϸ� true, �׷��� ������ false ��ȯ
		 * @remarks ��� GPU SDK�� ���Ͽ� ����
		 */
		bool ReleaseAllGpuResources();

		/*!
		* @brief GPU Manager �� Ư�� �Ķ����(�Լ�)�� �ѱ�� �Ϲ� �Լ�
		* @param param_name [in] \n string \n �Ķ���� �̸�
		* @param dtype [in] \n data_type \n data type
		* @param v_ptr [in] \n void \n �Ķ���� ���� ��� �ִ� void �� ������
		 * @return bool \n GPU Manager ���� ���ǵ� �Ķ���� �̸����� �ѱ�� true, �׷��� ������ false
		 * @remarks ��� GPU SDK�� ���Ͽ� ����
		 */
		bool SetGpuManagerParameters(const string& param_name, const data_type& dtype, const void* v_ptr, const int num_elements);

		/*!
		* @brief GPU Manager �� Ư�� �Ķ����(�Լ�)�� �ѱ�� �Ϲ� �Լ�
		* @param v_ptr [out] \n void \n �Ķ���� ���� ��� ���� void �� ������
		* @param param_name [in] \n string \n �Ķ���� �̸�
		* @param dtype [in] \n data_type \n �Ķ����data type
		* @return bool \n GPU Manager ���� ���ǵ� �Ķ���� �̸����� �ѱ�� true, �׷��� ������ false
		* @remarks ��� GPU SDK�� ���Ͽ� ����
		*/
		bool GetGpuManagerParameters(const string& param_name, const data_type& dtype, void* v_pt, int* num_elements = NULL);
	};
}