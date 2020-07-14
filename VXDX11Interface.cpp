#include "VXDX11Interface.h"
#include "VXDX11Helper.h"
#include "VXDX11VObjectGenerator.h"

#define SDK_REDISTRIBUTE

#if (defined(_DEBUG) || defined(DEBUG)) && !defined(SDK_REDISTRIBUTE)
#include <DXGI.h>
ID3D11Debug *debugDev;
#endif

#include <math.h>
#include <iostream>

//#define GMERRORMESSAGE(a) printf(a)

class valueComp
{
public:
	/// SVXGPUResourceDESC 의 key 식별을 위한 비교 operator
	bool operator()(const GpuResDescriptor& a, const GpuResDescriptor& b) const
	{
		if ((int)a.sdk_type < (int)b.sdk_type)
			return true;
		else if ((int)a.sdk_type > (int)b.sdk_type)
			return false;

		if ((int)a.res_type < (int)b.res_type)
			return true;
		else if ((int)a.res_type > (int)b.res_type)
			return false;

		if ((int)a.dtype.type_name.compare(b.dtype.type_name) > 0)
			return true;
		else if ((int)a.dtype.type_name.compare(b.dtype.type_name) < 0)
			return false;

		if (a.dtype.type_bytes < b.dtype.type_bytes)
			return true;
		else if (a.dtype.type_bytes > b.dtype.type_bytes)
			return false;

		if (a.misc < b.misc)
			return true;
		else if (a.misc > b.misc)
			return false;

		if (a.src_obj_id < b.src_obj_id)
			return true;
		else if (a.src_obj_id > b.src_obj_id)
			return false;

		if (a.usage_specifier.compare(b.usage_specifier) < 0)
			return true;
		else if (a.usage_specifier.compare(b.usage_specifier) > 0)
			return false;

		return false;
	}
};


#ifndef VMSAFE_RELEASE
#define VMSAFE_RELEASE(p)			{ if(p) { (p)->Release(); (p)=NULL; } }
#endif
typedef map<GpuResDescriptor, GpuResource, valueComp> GPUResMap;

struct ___gp_lobj_buffer {
	void* buffer_ptr;
	data_type dtype;
	int elements;
};
map<string, ___gp_lobj_buffer> __g_mapCustomParameters;

static ID3D11Device* g_pdx11Device = NULL;
static ID3D11DeviceContext* g_pdx11DeviceImmContext = NULL;
static D3D_FEATURE_LEVEL g_eFeatureLevel;
static DXGI_ADAPTER_DESC g_adapterDesc;
static GPUResMap g_mapVmResources;

bool VmInitializeDevice()
{
	// ClearState
	// ResizeBuffers
	// Flush
	// return false;
	if(g_pdx11Device || g_pdx11DeviceImmContext)
	{
		printf("VmManager::CreateDevice - Already Created!");
		return true;
	}

	UINT createDeviceFlags = 0;
	D3D_DRIVER_TYPE driverTypes = D3D_DRIVER_TYPE_HARDWARE;
#if (defined(_DEBUG) || defined(DEBUG)) && !defined(SDK_REDISTRIBUTE)
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
// 	driverTypes = D3D_DRIVER_TYPE_REFERENCE;
#endif
	try
	{
		D3D11CreateDevice(NULL, driverTypes, NULL, createDeviceFlags, NULL, 0, D3D11_SDK_VERSION,
			&g_pdx11Device, &g_eFeatureLevel, &g_pdx11DeviceImmContext);
	}
	catch (std::exception&)
	{
		//::MessageBox(NULL, ("VX3D requires DirectX Driver!!"), NULL, MB_OK);
		//string str = e.what();
		//std::string wstr(str.length(),L' ');
		//copy(str.begin(),str.end(),wstr.begin());
		//::MessageBox(NULL, wstr.c_str(), NULL, MB_OK);
		//exit(0);
		return false;
	}

	if(g_pdx11Device == NULL || g_pdx11DeviceImmContext == NULL || g_eFeatureLevel < 0x9300 )
	{
		VMSAFE_RELEASE(g_pdx11DeviceImmContext);
		VMSAFE_RELEASE(g_pdx11Device);
		return false;
	}

	printf("\nRenderer's DirectX Device Creation Success\n\n");

#if (defined(_DEBUG) || defined(DEBUG)) && !defined(SDK_REDISTRIBUTE)
	// Debug //
	HRESULT hr = g_pdx11Device->QueryInterface(__uuidof(ID3D11Debug), (void**)(&debugDev)); 
#endif

	IDXGIDevice * pDXGIDevice;
	g_pdx11Device->QueryInterface(__uuidof(IDXGIDevice), (void **)&pDXGIDevice);
	IDXGIAdapter * pDXGIAdapter;
	pDXGIDevice->GetAdapter(&pDXGIAdapter);
	pDXGIAdapter->GetDesc(&g_adapterDesc);
	VMSAFE_RELEASE(pDXGIDevice);
	VMSAFE_RELEASE(pDXGIAdapter);

	// 0x10DE : NVIDIA
	// 0x1002, 0x1022 : AMD ATI
// 	if(g_adapterDesc.VendorId != 0x10DE && g_adapterDesc.VendorId != 0x1002 && g_adapterDesc.VendorId != 0x1022)
// 	{
// 		VMSAFE_RELEASE(g_pdx11DeviceImmContext);
// 		VMSAFE_RELEASE(g_pdx11Device);
// 		return false;
// 	}

	VmDeviceSetting(g_pdx11Device, g_pdx11DeviceImmContext, g_adapterDesc);

	return true;
}

bool VmDeinitializeDevice()
{
	if(g_pdx11DeviceImmContext == NULL || g_pdx11Device == NULL)
		return false;

	g_pdx11DeviceImmContext->Flush();
	g_pdx11DeviceImmContext->ClearState();
	VmReleaseAllGpuResources();
	VMSAFE_RELEASE(g_pdx11DeviceImmContext);
	VMSAFE_RELEASE(g_pdx11Device);

#if (defined(_DEBUG) || defined(DEBUG)) && !defined(SDK_REDISTRIBUTE)
	debugDev->ReportLiveDeviceObjects(D3D11_RLDO_SUMMARY);
	VMSAFE_RELEASE(debugDev);
#endif

	return true;
}

bool VmGetGpuMemoryBytes(uint* puiDedicatedGpuMemory, uint* puiFreeMemory)
{
	if(g_pdx11DeviceImmContext == NULL)
	{
		if(!VmInitializeDevice())
			return false;
	}

	if(puiDedicatedGpuMemory)
		*puiDedicatedGpuMemory = (uint)g_adapterDesc.DedicatedVideoMemory;
	if(puiFreeMemory)
		*puiFreeMemory = (uint)g_adapterDesc.DedicatedVideoMemory;

	return true;
}

bool VmGetDeviceInformation(void* pvDevInfo, const string& strDevSpecification)
{
	if(strDevSpecification.compare(("DEVICE_POINTER")) == 0)
	{
		void** ppvDev = (void**)pvDevInfo;
		*ppvDev = (void*)g_pdx11Device;
	}
	else if(strDevSpecification.compare(("DEVICE_CONTEXT_POINTER")) == 0)
	{
		void** ppvDev = (void**)pvDevInfo;
		*ppvDev = (void*)g_pdx11DeviceImmContext;
	}
	else if(strDevSpecification.compare(("DEVICE_ADAPTER_DESC")) == 0)
	{
		DXGI_ADAPTER_DESC* pdx11Adapter = (DXGI_ADAPTER_DESC*)pvDevInfo;
		*pdx11Adapter = g_adapterDesc;
	}
	else if(strDevSpecification.compare(("FEATURE_LEVEL")) == 0)
	{
		D3D_FEATURE_LEVEL* pdx11FeatureLevel = (D3D_FEATURE_LEVEL*)pvDevInfo;
		*pdx11FeatureLevel = g_eFeatureLevel;
	}
	else 
		return false;
	return true;
}

ullong VmGetUsedGpuMemorySizeBytes()
{
	ullong ullSizeBytes = 0;
	auto itrResDX11 = g_mapVmResources.begin();
	for(; itrResDX11 != g_mapVmResources.end(); itrResDX11++)
	{
		for(int i = 0; i < (int)itrResDX11->second.vtrPtrs.size(); i++)
		{
			void* pvPtr = itrResDX11->second.vtrPtrs.at(i);
			if(pvPtr && itrResDX11->second.gpu_res_desc.res_type == ResourceTypeDX11RESOURCE)
			{
				ID3D11Resource* pdx11Resource = (ID3D11Resource*)pvPtr;
				D3D11_RESOURCE_DIMENSION eD3D11ResDim;
				pdx11Resource->GetType(&eD3D11ResDim);

				if(itrResDX11->second.resource_row_pitch_bytes == 0)
				{
					ullSizeBytes += (itrResDX11->second.logical_res_bytes.x);
				}
				else if(itrResDX11->second.resource_slice_pitch_bytes == 0)
				{
					ullSizeBytes += (itrResDX11->second.logical_res_bytes.y * itrResDX11->second.resource_row_pitch_bytes);
				}
				else 
				{
					ullSizeBytes += (itrResDX11->second.logical_res_bytes.z * itrResDX11->second.resource_slice_pitch_bytes);
				}
			}
		}
	}
	return (ullong)(ullSizeBytes);
}

bool VmGetGpuResourceArchive(const GpuResDescriptor* pGpuResDesc, GpuResource* pstGpuResource/*out*/)
{
 	auto itrResDX11 = g_mapVmResources.find(*pGpuResDesc);
 	if(itrResDX11 == g_mapVmResources.end())
 		return false;

	itrResDX11->second.recent_used_time = vmhelpers::GetCurrentTimePack();
	if(pstGpuResource)
	{
		*pstGpuResource = itrResDX11->second;
	}
	
	return true;
}

uint VmGetGpuResourceArchivesByObjectID(const int iObjectID, vector<GpuResource*>* pvtrGpuResources/*out*/)
{
	auto itrResDX11 = g_mapVmResources.begin();
	for(; itrResDX11 != g_mapVmResources.end(); itrResDX11++)
	{
		if(itrResDX11->second.gpu_res_desc.src_obj_id == iObjectID)
			pvtrGpuResources->push_back(&itrResDX11->second);
	}

	return (int)pvtrGpuResources->size();
}

bool VmGenerateGpuResource(const GpuResDescriptor* pGpuResDesc, const VmObject* obj_ptr, GpuResource* gpu_archive, LocalProgress* pstLocalProgress)
{
	// Check //
	if(pGpuResDesc->sdk_type != GpuSdkTypeDX11 || 
		pGpuResDesc->src_obj_id != obj_ptr->GetObjectID())
	{
		GMERRORMESSAGE("SourceID should be same as VmObjectID in Dojo's DX11 Interface : CONTACT korfriend@gmail.com");
	//	return false;
	}
	
	if(VmGetGpuResourceArchive(pGpuResDesc, NULL))
	{
		GMERRORMESSAGE("Already Exist! GPU Resource Archive\n");
		return false;
	}

	// Main //
	GpuResource gpu_res_data;
	if(pGpuResDesc->res_type != ResourceTypeDX11RESOURCE)
	{
		GpuResDescriptor gpu_res_descPrev = *pGpuResDesc;
		gpu_res_descPrev.res_type = ResourceTypeDX11RESOURCE;
		GpuResource gpu_bind_res;
		if(!VmGetGpuResourceArchive(&gpu_res_descPrev, &gpu_bind_res))
		{
			//GMERRORMESSAGE("View's Resource must be generated!\n");
			return false;
		}
		if(!VmGenerateGpuResBinderAsView(&gpu_bind_res, pGpuResDesc->res_type, &gpu_res_data))
			return false;

		if (gpu_archive) *gpu_archive = gpu_res_data;
		g_mapVmResources.insert(pair<GpuResDescriptor, GpuResource>(*pGpuResDesc, gpu_res_data));
		return true;
	}
	// Resource Type
	uint uiRequiredStorageSizeMB = 100;//VXHGComputeRequiredStorageSizeMB(pGpuResDesc, pCObject);
	if(pGpuResDesc->usage_specifier.compare(("BUFFER_PRIMITIVE_VERTEX_LIST")) == 0)
	{
		if(VmObject::GetObjectTypeFromID(pGpuResDesc->src_obj_id) != ObjectTypePRIMITIVE)
			return false;
		if(VmRetrenchGpuResourcesForStorage(uiRequiredStorageSizeMB) < 0)
			return false;
		if(!VmGenerateGpuResourcePRIMITIVE_VERTEX(pGpuResDesc, (VmVObjectPrimitive*)obj_ptr, &gpu_res_data, pstLocalProgress))
			return false;
	}
	else if(pGpuResDesc->usage_specifier.compare(("BUFFER_PRIMITIVE_INDEX_LIST")) == 0)
	{
		if(VmObject::GetObjectTypeFromID(pGpuResDesc->src_obj_id) != ObjectTypePRIMITIVE)
			return false;
		if(VmRetrenchGpuResourcesForStorage(uiRequiredStorageSizeMB) < 0)
			return false;
		if(!VmGenerateGpuResourcePRIMITIVE_INDEX(pGpuResDesc, (VmVObjectPrimitive*)obj_ptr, &gpu_res_data, pstLocalProgress))
			return false;
	}
	else if(pGpuResDesc->usage_specifier.compare(("TEXTURE3D_VOLUME_DEFAULT")) == 0
		|| pGpuResDesc->usage_specifier.compare(("TEXTURE3D_VOLUME_DEFAULT_MASK")) == 0
		|| pGpuResDesc->usage_specifier.compare(("TEXTURE3D_VOLUME_DOWNSAMPLE")) == 0
		|| pGpuResDesc->usage_specifier.compare(("TEXTURE2DARRAY_IMAGESTACK")) == 0
		)
	{
		if(VmObject::GetObjectTypeFromID(pGpuResDesc->src_obj_id) != ObjectTypeVOLUME)
			return false;
		if(VmRetrenchGpuResourcesForStorage(uiRequiredStorageSizeMB) < 0)
			return false;

		if(!VmGenerateGpuResourceVOLUME(pGpuResDesc, (VmVObjectVolume*)obj_ptr, &gpu_res_data, pstLocalProgress))
			return false;
	}
	else if(pGpuResDesc->usage_specifier.compare(("TEXTURE3D_VOLUME_BLOCKMAP")) == 0)
	{
		if(VmObject::GetObjectTypeFromID(pGpuResDesc->src_obj_id) != ObjectTypeVOLUME)
			return false;
		if(VmRetrenchGpuResourcesForStorage(uiRequiredStorageSizeMB) < 0)
			return false;
		if(!VmGenerateGpuResourceBLOCKSMAP(pGpuResDesc, (VmVObjectVolume*)obj_ptr, &gpu_res_data, pstLocalProgress))
			return false;
	}
	else if(pGpuResDesc->usage_specifier.compare(("TEXTURE3D_VOLUME_MINMAXBLOCK")) == 0)
	{
		if(VmObject::GetObjectTypeFromID(pGpuResDesc->src_obj_id) != ObjectTypeVOLUME)
			return false;
		if(VmRetrenchGpuResourcesForStorage(uiRequiredStorageSizeMB) < 0)
			return false;
		if(!VmGenerateGpuResourceBlockMinMax(pGpuResDesc, (VmVObjectVolume*)obj_ptr, &gpu_res_data, pstLocalProgress))
			return false;
	}
	else if(pGpuResDesc->usage_specifier.compare(("TEXTURE3D_VOLUME_BRICKCHUNK")) == 0)
	{
		if(VmObject::GetObjectTypeFromID(pGpuResDesc->src_obj_id) != ObjectTypeVOLUME)
			return false;
		if(VmRetrenchGpuResourcesForStorage(uiRequiredStorageSizeMB) < 0)
			return false;
		if(!VmGenerateGpuResourceBRICKS(pGpuResDesc, (VmVObjectVolume*)obj_ptr, &gpu_res_data, pstLocalProgress))
			return false;
	}
	else if(pGpuResDesc->usage_specifier.compare(("TEXTURE2D_TOBJECT_2D")) == 0
		|| pGpuResDesc->usage_specifier.compare(("BUFFER_TOBJECT_OTFSERIES")) == 0)
	{
		if(VmObject::GetObjectTypeFromID(pGpuResDesc->src_obj_id) != ObjectTypeTMAP)
			return false;
		if(!VmGenerateGpuResourceOTF(pGpuResDesc, (VmTObject*)obj_ptr, &gpu_res_data, pstLocalProgress))
			return false;
	}
	else if (pGpuResDesc->usage_specifier.compare(0, 14, "BUFFER_IOBJECT") == 0
		|| pGpuResDesc->usage_specifier.compare(0, 17, "TEXTURE2D_IOBJECT") == 0
		|| pGpuResDesc->usage_specifier == "TEXTURE2D_RW_COUNTER"
		|| pGpuResDesc->usage_specifier == "TEXTURE2D_RW_SPINLOCK"
		|| pGpuResDesc->usage_specifier == "BUFFER_RW_DEEP_VIS"
		|| pGpuResDesc->usage_specifier == "BUFFER_RW_DEEP_ZDEPTH"
		|| pGpuResDesc->usage_specifier == "BUFFER_RW_DEEP_ZTHICK")
	{
		if(VmObject::GetObjectTypeFromID(pGpuResDesc->src_obj_id) != ObjectTypeIMAGEPLANE)
			return false;
		if(!VmGenerateGpuResourceFRAMEBUFFER(pGpuResDesc, (VmIObject*)obj_ptr, &gpu_res_data, pstLocalProgress))
			return false;
	}
	else if (pGpuResDesc->usage_specifier.compare(("TEXTURE2D_CMM_TEXT")) == 0)
	{
		if (VmObject::GetObjectTypeFromID(pGpuResDesc->src_obj_id) != ObjectTypePRIMITIVE)
			return false;
		if (!VmGenerateGpuResourceCMM_TEXT(pGpuResDesc, (VmVObjectPrimitive*)obj_ptr, &gpu_res_data, pstLocalProgress))
			return false;
	}
	else
	{
		printf("NOT SUPPORTED USAGE STRING IN DX11 : %s\n", pGpuResDesc->usage_specifier.c_str());
		return false;
	}
	if (gpu_archive) *gpu_archive = gpu_res_data;
	g_mapVmResources.insert(pair<GpuResDescriptor, GpuResource>(*pGpuResDesc, gpu_res_data));
	return true;
}

bool VmReplaceOrAddGpuResource(const GpuResource* pstGpuResource)
{
	if(pstGpuResource->gpu_res_desc.sdk_type != GpuSdkTypeDX11)
		return false;

	auto itr = g_mapVmResources.find(pstGpuResource->gpu_res_desc);
	if (itr != g_mapVmResources.end())
		VmReleaseGpuResource(&((GpuResource*)pstGpuResource)->gpu_res_desc, false);

	g_mapVmResources[pstGpuResource->gpu_res_desc] = *pstGpuResource;
// 	if(VmGetGpuResourceArchive(&(pstGpuResource->gpu_res_desc), NULL) == true)
// 		g_mapVmResources[pstGpuResource->gpu_res_desc] = *pstGpuResource;
// 	else
// 		g_mapVmResources.insert(pair<GpuResDescriptor, GpuResArchive>(pstGpuResource->gpu_res_desc, *pstGpuResource));

	return true;
}

inline bool __VXReleaseGPUResource(GpuResource* pstGpuResArchive)
{
	if(pstGpuResArchive->gpu_res_desc.sdk_type != GpuSdkTypeDX11)
		return false;

	switch(pstGpuResArchive->gpu_res_desc.res_type)
	{
	case ResourceTypeDX11RESOURCE:
		{
			for(int i = 0; i < (int)pstGpuResArchive->vtrPtrs.size(); i++)
			{
				ID3D11Resource* pdx11Resource = (ID3D11Resource*)pstGpuResArchive->vtrPtrs.at(i);
				VMSAFE_RELEASE(pdx11Resource);
			}
		}
		break;
	case ResourceTypeDX11SRV:
	case ResourceTypeDX11RTV:
	case ResourceTypeDX11UAV:
	case ResourceTypeDX11DSV:
		{
			for(int i = 0; i < (int)pstGpuResArchive->vtrPtrs.size(); i++)
			{
				ID3D11View* pdx11View = (ID3D11View*)pstGpuResArchive->vtrPtrs.at(i);
				VMSAFE_RELEASE(pdx11View);
			}
		}
		break;
	default:
		return false;
	}
	//std::cout << "release!! : " << pstGpuResArchive->gpu_res_desc.src_obj_id << " : " << pstGpuResArchive->gpu_res_desc.res_type << std::endl;
	pstGpuResArchive->vtrPtrs.clear();

	
	//if (VmObject::GetObjectTypeFromID(pstGpuResArchive->gpu_res_desc.src_obj_id) == ObjectTypePRIMITIVE
	//	|| VmObject::GetObjectTypeFromID(pstGpuResArchive->gpu_res_desc.src_obj_id) == ObjectTypeVOLUME)
	//{
	//	if (pstGpuResArchive->gpu_res_desc.res_type == ResourceTypeDX11RESOURCE)
	//	{
	//		wprintf(("Release DX11 Resource, Id:%d, Misc:%d%d%d%d(%s)\n")
	//			, pstGpuResArchive->gpu_res_desc.src_obj_id, pstGpuResArchive->gpu_res_desc.misc, (int)pstGpuResArchive->gpu_res_desc.dtype
	//			, pstGpuResArchive->gpu_res_desc.iSizeStride, (int)pstGpuResArchive->gpu_res_desc.res_type
	//			, pstGpuResArchive->gpu_res_desc.usage_specifier.c_str());
	//	}
	//	else
	//	{
	//		if (pstGpuResArchive->gpu_res_desc.res_type == ResourceTypeDX11SRV)
	//		{
	//			printf("Release DX11 View(SRV), ");
	//		}
	//		else if (pstGpuResArchive->gpu_res_desc.res_type == ResourceTypeDX11RTV)
	//		{
	//			printf("Release DX11 View(RTV), ");
	//		}
	//		else if (pstGpuResArchive->gpu_res_desc.res_type == ResourceTypeDX11DSV)
	//		{
	//			printf("Release DX11 View(DSV), ");
	//		}
	//		else if (pstGpuResArchive->gpu_res_desc.res_type == ResourceTypeDX11UAV)
	//		{
	//			printf("Release DX11 View(UAV), ");
	//		}
	//		wprintf(("Id:%d, Misc:%d%d%d%d(%s)\n")
	//			, pstGpuResArchive->gpu_res_desc.src_obj_id, pstGpuResArchive->gpu_res_desc.misc, (int)pstGpuResArchive->gpu_res_desc.dtype
	//			, pstGpuResArchive->gpu_res_desc.iSizeStride, (int)pstGpuResArchive->gpu_res_desc.res_type
	//			, pstGpuResArchive->gpu_res_desc.usage_specifier.c_str());
	//	}
	//}

	return true;
}

bool VmReleaseGpuResource(const GpuResDescriptor* pGpuResDesc, const bool bCallClearState)
{
	auto itrResDX11 = g_mapVmResources.find(*pGpuResDesc);
	if(itrResDX11 == g_mapVmResources.end())
		return false;

	g_pdx11DeviceImmContext->Flush();

	if(__VXReleaseGPUResource(&itrResDX11->second))
		g_mapVmResources.erase(itrResDX11);

	g_pdx11DeviceImmContext->Flush();
	if (bCallClearState)
		g_pdx11DeviceImmContext->ClearState();
	return true;
}

bool VmReleaseGpuResourcesRelatedObject(const int iObjectID, const bool bCallClearState)
{
	if (iObjectID == 0)
		return false;

	g_pdx11DeviceImmContext->Flush();

	auto itrResDX11 = g_mapVmResources.begin();
	while(itrResDX11 != g_mapVmResources.end())
	{
		if(itrResDX11->first.src_obj_id != itrResDX11->second.gpu_res_desc.src_obj_id)
			GMERRORMESSAGE("ERROR!!!! VmReleaseGpuResourcesRelatedObject");

		if (itrResDX11->first.src_obj_id == iObjectID)
		{
			if(itrResDX11->second.vtrPtrs.size() == 0)
			{
				GMERRORMESSAGE("ERROR!!!! NULL POINT!!!!");
			}

			if(__VXReleaseGPUResource(&itrResDX11->second))
			{
				g_mapVmResources.erase(itrResDX11);
				itrResDX11 = g_mapVmResources.begin();
			}
		}
		else
		{
			itrResDX11++;
		}
	}

	g_pdx11DeviceImmContext->Flush();
	if (bCallClearState)
		g_pdx11DeviceImmContext->ClearState();

	return true;
}

bool VmReleaseGpuResourcesVObjects()
{
	auto itrResDX11 = g_mapVmResources.begin();
	while(itrResDX11 != g_mapVmResources.end())
	{
		if(itrResDX11->first.src_obj_id != itrResDX11->second.gpu_res_desc.src_obj_id)
			GMERRORMESSAGE("ERROR!!!! ___");
		if(VmObject::GetObjectTypeFromID(itrResDX11->first.src_obj_id) == ObjectTypeVOLUME
			|| VmObject::GetObjectTypeFromID(itrResDX11->first.src_obj_id) == ObjectTypePRIMITIVE)
		{
			if(__VXReleaseGPUResource(&itrResDX11->second))
			{
				g_mapVmResources.erase(itrResDX11);
				itrResDX11 = g_mapVmResources.begin();
			}
		}
		else
		{
			itrResDX11++;
		}
	}

	g_pdx11DeviceImmContext->Flush();
	g_pdx11DeviceImmContext->ClearState();

	return true;
}

bool VmReleaseAllGpuResources()
{
	HDx11ReleaseCommonResources();

	auto itrResDX11 = g_mapVmResources.begin();
	for(; itrResDX11 != g_mapVmResources.end(); itrResDX11++)
	{
		if(itrResDX11->first.src_obj_id != itrResDX11->second.gpu_res_desc.src_obj_id)
			GMERRORMESSAGE("ERROR!!!! ___");
		__VXReleaseGPUResource(&itrResDX11->second);
	}
	g_mapVmResources.clear();

	g_pdx11DeviceImmContext->Flush();
	g_pdx11DeviceImmContext->ClearState();

	VMSAFE_RELEASE(g_pdx11DeviceImmContext);
	VMSAFE_RELEASE(g_pdx11Device);

#if (defined(_DEBUG) || defined(DEBUG)) && !defined(SDK_REDISTRIBUTE)
	debugDev->ReportLiveDeviceObjects(D3D11_RLDO_SUMMARY);
	VMSAFE_RELEASE(debugDev);
#endif
	//VmInitializeDevice();

	return true;
}

int VmRetrenchGpuResourcesForStorage(const uint uiRequiredStorageSizeMB, const ullong ullMinimumTimeGap, const bool bIsTargetOnlyVObject) // Return # of Retrenched resources
{
	// Later
	return 0;
	/*
	// Current Gpu Memory //
	uint uiMinimumGpuSafeMemoryMB = 300;
	uint uiGpuMemory = (uint)(g_adapterDesc.DedicatedVideoMemory/1024/1024);
 	if(uiGpuMemory < uiMinimumGpuSafeMemoryMB)
 	{
 		GMERRORMESSAGE("GPU memory is too small...");
 		return -1;
 	}
	
	uint uiValidGpuMemory = uiGpuMemory - uiMinimumGpuSafeMemoryMB;
	if(uiRequiredStorageSizeMB > uiValidGpuMemory)
	{
		GMERRORMESSAGE("Required GPU memory is too Large... Fail to Retrench!");
		return -1;
	}

	uint uiPrevGpuUsedSize = VmGetUsedGpuMemorySizeBytes()/1024;
	if(uiPrevGpuUsedSize + uiRequiredStorageSizeMB < uiValidGpuMemory + 10)
		return 0;

	// LRU // At Least One Resource should be released! or return -1
	int iCountRelease = 0;
	ullong ullCurrentTime = vmhelpers::GetCurrentTimePack();
	while((int)uiValidGpuMemory - (int)uiPrevGpuUsedSize < (int)uiRequiredStorageSizeMB)
	{
		ullong ullLRUTime = ULLONG_MAX;
		map<llong, SVXGpuResourceIndicator>::iterator itrResDX11 = g_mapVmResources.begin();
		map<llong, SVXGpuResourceIndicator>::iterator itrLeastRecentUsed = itrResDX11;
		int iCountChecked = 0;
		for(; itrResDX11 != g_mapVmResources.end(); itrResDX11++)
		{
			if(itrResDX11->second.pdx11Resource != NULL)
			{
				if(bIsTargetOnlyVObject)
				{
					if(!VmObject::IsVObjectType(itrResDX11->second.gpu_res_desc.src_obj_id))
						continue;
				}
				if(itrResDX11->second.recent_used_time < ullLRUTime)
				{
					ullLRUTime = itrResDX11->second.recent_used_time;
					itrLeastRecentUsed = itrResDX11;
					iCountChecked++;
				}
			}
		}

		if(iCountChecked == 0 || ullCurrentTime - ullLRUTime == 0)
		{
			if(bIsTargetOnlyVObject == false)
				break;
			else
				bIsTargetOnlyVObject = false;
		}
		else
		{
			// Release
			VmReleaseGpuResource(&itrLeastRecentUsed->second.gpu_res_desc);
			iCountRelease++;
		}

		uiPrevGpuUsedSize = VmGetUsedGpuMemorySizeBytes()/1024;
		if(uiPrevGpuUsedSize == 0)
			break;
	}

	if(iCountRelease == 0)
	{
		GMERRORMESSAGE("This Scene Cannot be render out by GPU...");
		return -1;
	}
	printf("\n***************\nRetrenchGpuResources :: %d\n***************\n", iCountRelease);
	
	return iCountRelease;
	/**/
}
bool VmSetGpuManagerParameters(const string& param_name, const data_type& dtype, const void* v_ptr, const int num_elements)
{
	auto it = __g_mapCustomParameters.find(param_name);
	if (it != __g_mapCustomParameters.end())
	{
		if (it->second.buffer_ptr == v_ptr)
		{
			printf("WARNNING!! ReplaceOrAddBufferPtr ==> Same buffer pointer is used!");
			return false;
		}
		else
		{
			VMSAFE_DELETEARRAY(it->second.buffer_ptr);
			__g_mapCustomParameters.erase(it);
		}
	}

	___gp_lobj_buffer buf;
	buf.elements = num_elements;
	buf.dtype = dtype;
	buf.buffer_ptr = new byte[buf.dtype.type_bytes * num_elements];
	memcpy(buf.buffer_ptr, v_ptr, buf.dtype.type_bytes * num_elements);
	__g_mapCustomParameters[param_name] = buf;
	return true;
}

bool VmGetGpuManagerParameters(const string& param_name, const data_type& dtype, void** v_ptr, int* num_elements)
{
	*v_ptr = NULL;
	auto itrValue = __g_mapCustomParameters.find(param_name);
	if (itrValue == __g_mapCustomParameters.end())
		return false;
	if (itrValue->second.dtype != dtype) return false;
	*v_ptr = itrValue->second.buffer_ptr;
	if (num_elements) *num_elements = itrValue->second.elements;
	return true;
}