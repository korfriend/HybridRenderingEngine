#include "VXDX11Interface.h"
#include "VXDX11VObjectGenerator.h"

#define SDK_REDISTRIBUTE

#if (defined(_DEBUG) || defined(DEBUG)) && !defined(SDK_REDISTRIBUTE)
#include <DXGI.h>
ID3D11Debug *debugDev;
#endif

#include <math.h>

#define VXSAFE_RELEASE(p)			{ if(p) { (p)->Release(); (p)=NULL; } }
typedef map<SVXGPUResourceDESC, SVXGPUResourceArchive, valueComp> GPUResMap;
map<wstring, void*> g_mapCustomParameters;

static ID3D11Device* g_pdx11Device = NULL;
static ID3D11DeviceContext* g_pdx11DeviceImmContext = NULL;
static D3D_FEATURE_LEVEL g_eFeatureLevel;
static DXGI_ADAPTER_DESC g_adapterDesc;
static GPUResMap g_mapVxgResources;

bool VxgInitializeDevice()
{
	// ClearState
	// ResizeBuffers
	// Flush
	// return false;
	if(g_pdx11Device || g_pdx11DeviceImmContext)
	{
		VXERRORMESSAGE("VxgManager::CreateDevice - Already Created!");
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
		D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, NULL, 0, D3D11_SDK_VERSION, 
			&g_pdx11Device, &g_eFeatureLevel, &g_pdx11DeviceImmContext);
	}
	catch (std::exception&)
	{
		//::MessageBox(NULL, _T("VX3D requires DirectX Driver!!"), NULL, MB_OK);
		//string str = e.what();
		//std::wstring wstr(str.length(),L' ');
		//copy(str.begin(),str.end(),wstr.begin());
		//::MessageBox(NULL, wstr.c_str(), NULL, MB_OK);
		//exit(0);
		return false;
	}

	if(g_pdx11Device == NULL || g_pdx11DeviceImmContext == NULL || g_eFeatureLevel < 0x9300 )
	{
		VXSAFE_RELEASE(g_pdx11DeviceImmContext);
		VXSAFE_RELEASE(g_pdx11Device);
		return false;
	}

	printf("\nDirectX Device Creation Success\n\n");

#if (defined(_DEBUG) || defined(DEBUG)) && !defined(SDK_REDISTRIBUTE)
	// Debug //
	HRESULT hr = g_pdx11Device->QueryInterface(__uuidof(ID3D11Debug), (void**)(&debugDev)); 
#endif

	IDXGIDevice * pDXGIDevice;
	g_pdx11Device->QueryInterface(__uuidof(IDXGIDevice), (void **)&pDXGIDevice);
	IDXGIAdapter * pDXGIAdapter;
	pDXGIDevice->GetAdapter(&pDXGIAdapter);
	pDXGIAdapter->GetDesc(&g_adapterDesc);
	VXSAFE_RELEASE(pDXGIDevice);
	VXSAFE_RELEASE(pDXGIAdapter);

	// 0x10DE : NVIDIA
	// 0x1002, 0x1022 : AMD ATI
// 	if(g_adapterDesc.VendorId != 0x10DE && g_adapterDesc.VendorId != 0x1002 && g_adapterDesc.VendorId != 0x1022)
// 	{
// 		VXSAFE_RELEASE(g_pdx11DeviceImmContext);
// 		VXSAFE_RELEASE(g_pdx11Device);
// 		return false;
// 	}

	VxgDeviceSetting(g_pdx11Device, g_pdx11DeviceImmContext, g_adapterDesc);

	return true;
}

bool VxgDeinitializeDevice()
{
	if(g_pdx11DeviceImmContext == NULL || g_pdx11Device == NULL)
		return false;

	g_pdx11DeviceImmContext->Flush();
	g_pdx11DeviceImmContext->ClearState();
	VxgReleaseAllGpuResources();
	VXSAFE_RELEASE(g_pdx11DeviceImmContext);
	VXSAFE_RELEASE(g_pdx11Device);

#if (defined(_DEBUG) || defined(DEBUG)) && !defined(SDK_REDISTRIBUTE)
	debugDev->ReportLiveDeviceObjects(D3D11_RLDO_SUMMARY);
	VXSAFE_RELEASE(debugDev);
#endif

	return true;
}

bool VxgGetGpuMemoryBytes(uint* puiDedicatedGpuMemory, uint* puiFreeMemory)
{
	if(g_pdx11DeviceImmContext == NULL)
	{
		if(!VxgInitializeDevice())
			return false;
	}

	if(puiDedicatedGpuMemory)
		*puiDedicatedGpuMemory = (uint)g_adapterDesc.DedicatedVideoMemory;
	if(puiFreeMemory)
		*puiFreeMemory = (uint)g_adapterDesc.DedicatedVideoMemory;

	return true;
}

bool VxgGetDeviceInformation(void* pvDevInfo, wstring strDevSpecification)
{
	if(strDevSpecification.compare(_T("DEVICE_POINTER")) == 0)
	{
		void** ppvDev = (void**)pvDevInfo;
		*ppvDev = (void*)g_pdx11Device;
	}
	else if(strDevSpecification.compare(_T("DEVICE_CONTEXT_POINTER")) == 0)
	{
		void** ppvDev = (void**)pvDevInfo;
		*ppvDev = (void*)g_pdx11DeviceImmContext;
	}
	else if(strDevSpecification.compare(_T("DEVICE_ADAPTER_DESC")) == 0)
	{
		DXGI_ADAPTER_DESC* pdx11Adapter = (DXGI_ADAPTER_DESC*)pvDevInfo;
		*pdx11Adapter = g_adapterDesc;
	}
	else if(strDevSpecification.compare(_T("FEATURE_LEVEL")) == 0)
	{
		D3D_FEATURE_LEVEL* pdx11FeatureLevel = (D3D_FEATURE_LEVEL*)pvDevInfo;
		*pdx11FeatureLevel = g_eFeatureLevel;
	}
	else 
		return false;
	return true;
}

uint VxgGetUsedGpuMemorySizeKiloBytes()
{
	ullong ullSizeKiloBytes = 0;
	auto itrResDX11 = g_mapVxgResources.begin();
	for(; itrResDX11 != g_mapVxgResources.end(); itrResDX11++)
	{
		for(int i = 0; i < (int)itrResDX11->second.vtrPtrs.size(); i++)
		{
			void* pvPtr = itrResDX11->second.vtrPtrs.at(i);
			if(pvPtr && itrResDX11->second.svxGpuResourceDesc.eResourceType == vxgResourceTypeDX11RESOURCE)
			{
				ID3D11Resource* pdx11Resource = (ID3D11Resource*)pvPtr;
				D3D11_RESOURCE_DIMENSION eD3D11ResDim;
				pdx11Resource->GetType(&eD3D11ResDim);

				if(itrResDX11->second.uiResourceRowPitchInBytes == 0)
				{
					ullSizeKiloBytes += (itrResDX11->second.i3SizeLogicalResourceInBytes.x / 1024);
				}
				else if(itrResDX11->second.uiResourceSlicePitchInBytes == 0)
				{
					ullSizeKiloBytes += (itrResDX11->second.i3SizeLogicalResourceInBytes.y * itrResDX11->second.uiResourceRowPitchInBytes / 1024);
				}
				else 
				{
					ullSizeKiloBytes += (itrResDX11->second.i3SizeLogicalResourceInBytes.z * itrResDX11->second.uiResourceSlicePitchInBytes / 1024);
				}
			}
		}
	}
	return (uint)(ullSizeKiloBytes);
}

bool VxgGetGpuResourceArchive(const SVXGPUResourceDESC* pstGpuResourceDesc, SVXGPUResourceArchive* pstGpuResource/*out*/)
{
 	auto itrResDX11 = g_mapVxgResources.find(*pstGpuResourceDesc);
 	if(itrResDX11 == g_mapVxgResources.end())
 		return false;

	itrResDX11->second.ullRecentUsedTime = VXHGetCurrentTimePack();
	if(pstGpuResource)
	{
		*pstGpuResource = itrResDX11->second;
	}
	
	return true;
}

uint VxgGetGpuResourceArchivesByObjectID(int iObjectID, vector<SVXGPUResourceArchive*>* pvtrGpuResources/*out*/)
{
	auto itrResDX11 = g_mapVxgResources.begin();
	for(; itrResDX11 != g_mapVxgResources.end(); itrResDX11++)
	{
		if(itrResDX11->second.svxGpuResourceDesc.iSourceObjectID == iObjectID)
			pvtrGpuResources->push_back(&itrResDX11->second);
	}

	return (int)pvtrGpuResources->size();
}

bool VxgGenerateGpuResource(const SVXGPUResourceDESC* pstGpuResourceDesc, CVXObject* pCObject, SVXLocalProgress* pstLocalProgress)
{
	// Check //
	if(pstGpuResourceDesc->eGpuSdkType != vxgGpuSdkTypeDX11 || 
		pstGpuResourceDesc->iSourceObjectID != pCObject->GetObjectID())
	{
		VXERRORMESSAGE("SourceID should be same as CVXObjectID in Dojo's DX11 Interface : CONTACT korfriend@gmail.com");
		return false;
	}
	
	if(VxgGetGpuResourceArchive(pstGpuResourceDesc, NULL))
	{
		VXERRORMESSAGE("Already Exist! GPU Resource Archive\n");
		return false;
	}

	// Main //
	SVXGPUResourceArchive svxGpuResourceOut;
	if(pstGpuResourceDesc->eResourceType != vxgResourceTypeDX11RESOURCE)
	{
		SVXGPUResourceDESC svxGpuResourceDescPrev = *pstGpuResourceDesc;
		svxGpuResourceDescPrev.eResourceType = vxgResourceTypeDX11RESOURCE;
		SVXGPUResourceArchive svxGpuResource;
		if(!VxgGetGpuResourceArchive(&svxGpuResourceDescPrev, &svxGpuResource))
		{
			//VXERRORMESSAGE("View's Resource must be generated!\n");
			return false;
		}
		if(!VxgGenerateGpuResBinderAsView(&svxGpuResource, pstGpuResourceDesc->eResourceType, &svxGpuResourceOut))
			return false;

		g_mapVxgResources.insert(pair<SVXGPUResourceDESC, SVXGPUResourceArchive>(*pstGpuResourceDesc, svxGpuResourceOut));
		return true;
	}
	// Resource Type
	uint uiRequiredStorageSizeMB = 100;//VXHGComputeRequiredStorageSizeMB(pstGpuResourceDesc, pCObject);
	if(pstGpuResourceDesc->strUsageSpecifier.compare(_T("BUFFER_PRIMITIVE_VERTEX_LIST")) == 0)
	{
		if(CVXObject::GetObjectTypeFromID(pstGpuResourceDesc->iSourceObjectID) != vxrObjectTypePRIMITIVE)
			return false;
		if(VxgRetrenchGpuResourcesForStorage(uiRequiredStorageSizeMB) < 0)
			return false;
		if(!VxgGenerateGpuResourcePRIMITIVE_VERTEX(pstGpuResourceDesc, (CVXVObjectPrimitive*)pCObject, &svxGpuResourceOut, pstLocalProgress))
			return false;
	}
	else if(pstGpuResourceDesc->strUsageSpecifier.compare(_T("BUFFER_PRIMITIVE_INDEX_LIST")) == 0)
	{
		if(CVXObject::GetObjectTypeFromID(pstGpuResourceDesc->iSourceObjectID) != vxrObjectTypePRIMITIVE)
			return false;
		if(VxgRetrenchGpuResourcesForStorage(uiRequiredStorageSizeMB) < 0)
			return false;
		if(!VxgGenerateGpuResourcePRIMITIVE_INDEX(pstGpuResourceDesc, (CVXVObjectPrimitive*)pCObject, &svxGpuResourceOut, pstLocalProgress))
			return false;
	}
	else if(pstGpuResourceDesc->strUsageSpecifier.compare(_T("TEXTURE3D_VOLUME_DEFAULT")) == 0
		|| pstGpuResourceDesc->strUsageSpecifier.compare(_T("TEXTURE3D_VOLUME_DEFAULT_MASK")) == 0
		|| pstGpuResourceDesc->strUsageSpecifier.compare(_T("TEXTURE3D_VOLUME_DOWNSAMPLE")) == 0
		|| pstGpuResourceDesc->strUsageSpecifier.compare(_T("TEXTURE2DARRAY_IMAGESTACK")) == 0
		)
	{
		if(CVXObject::GetObjectTypeFromID(pstGpuResourceDesc->iSourceObjectID) != vxrObjectTypeVOLUME)
			return false;
		if(VxgRetrenchGpuResourcesForStorage(uiRequiredStorageSizeMB) < 0)
			return false;

		if(!VxgGenerateGpuResourceVOLUME(pstGpuResourceDesc, (CVXVObjectVolume*)pCObject, &svxGpuResourceOut, pstLocalProgress))
			return false;
	}
	else if(pstGpuResourceDesc->strUsageSpecifier.compare(_T("TEXTURE3D_VOLUME_BLOCKMAP")) == 0)
	{
		if(CVXObject::GetObjectTypeFromID(pstGpuResourceDesc->iSourceObjectID) != vxrObjectTypeVOLUME)
			return false;
		if(VxgRetrenchGpuResourcesForStorage(uiRequiredStorageSizeMB) < 0)
			return false;
		if(!VxgGenerateGpuResourceBRICKSMAP(pstGpuResourceDesc, (CVXVObjectVolume*)pCObject, &svxGpuResourceOut, pstLocalProgress))
			return false;
	}
	else if(pstGpuResourceDesc->strUsageSpecifier.compare(_T("TEXTURE3D_VOLUME_MINMAXBLOCK")) == 0)
	{
		if(CVXObject::GetObjectTypeFromID(pstGpuResourceDesc->iSourceObjectID) != vxrObjectTypeVOLUME)
			return false;
		if(VxgRetrenchGpuResourcesForStorage(uiRequiredStorageSizeMB) < 0)
			return false;
		if(!VxgGenerateGpuResourceBlockMinMax(pstGpuResourceDesc, (CVXVObjectVolume*)pCObject, &svxGpuResourceOut, pstLocalProgress))
			return false;
	}
	else if(pstGpuResourceDesc->strUsageSpecifier.compare(_T("TEXTURE3D_VOLUME_BRICKCHUNK")) == 0)
	{
		if(CVXObject::GetObjectTypeFromID(pstGpuResourceDesc->iSourceObjectID) != vxrObjectTypeVOLUME)
			return false;
		if(VxgRetrenchGpuResourcesForStorage(uiRequiredStorageSizeMB) < 0)
			return false;
		if(!VxgGenerateGpuResourceBRICKS(pstGpuResourceDesc, (CVXVObjectVolume*)pCObject, &svxGpuResourceOut, pstLocalProgress))
			return false;
	}
	else if(pstGpuResourceDesc->strUsageSpecifier.compare(_T("TEXTURE2D_TOBJECT_2D")) == 0
		|| pstGpuResourceDesc->strUsageSpecifier.compare(_T("BUFFER_TOBJECT_OTFSERIES")) == 0)
	{
		if(CVXObject::GetObjectTypeFromID(pstGpuResourceDesc->iSourceObjectID) != vxrObjectTypeTRANSFERFUNCTION)
			return false;
		if(!VxgGenerateGpuResourceOTF(pstGpuResourceDesc, (CVXTObject*)pCObject, &svxGpuResourceOut, pstLocalProgress))
			return false;
	}
	else if(pstGpuResourceDesc->strUsageSpecifier.compare(0, 14, _T("BUFFER_IOBJECT")) == 0
		|| pstGpuResourceDesc->strUsageSpecifier.compare(0, 17, _T("TEXTURE2D_IOBJECT")) == 0)
	{
		if(CVXObject::GetObjectTypeFromID(pstGpuResourceDesc->iSourceObjectID) != vxrObjectTypeIMAGEPLANE)
			return false;
		if(!VxgGenerateGpuResourceFRAMEBUFFER(pstGpuResourceDesc, (CVXIObject*)pCObject, &svxGpuResourceOut, pstLocalProgress))
			return false;
	}
	else if (pstGpuResourceDesc->strUsageSpecifier.compare(_T("TEXTURE2D_CMM_TEXT")) == 0)
	{
		if (CVXObject::GetObjectTypeFromID(pstGpuResourceDesc->iSourceObjectID) != vxrObjectTypePRIMITIVE)
			return false;
		if (!VxgGenerateGpuResourceCMM_TEXT(pstGpuResourceDesc, (CVXVObjectPrimitive*)pCObject, &svxGpuResourceOut, pstLocalProgress))
			return false;
	}
	else
	{
		printf("NOT SUPPORTED USAGE STRING IN DX11 : \n", pstGpuResourceDesc->strUsageSpecifier.c_str());
		return false;
	}

	g_mapVxgResources.insert(pair<SVXGPUResourceDESC, SVXGPUResourceArchive>(*pstGpuResourceDesc, svxGpuResourceOut));
	return true;
}

bool VxgReplaceOrAddGpuResource(const SVXGPUResourceArchive* pstGpuResource)
{
	if(pstGpuResource->svxGpuResourceDesc.eGpuSdkType != vxgGpuSdkTypeDX11)
		return false;

// 	if(VxgGetGpuResourceArchive(&(pstGpuResource->svxGpuResourceDesc), NULL) == true)
// 		g_mapVxgResources[pstGpuResource->svxGpuResourceDesc] = *pstGpuResource;
// 	else
// 		g_mapVxgResources.insert(pair<SVXGPUResourceDESC, SVXGPUResourceArchive>(pstGpuResource->svxGpuResourceDesc, *pstGpuResource));
	g_mapVxgResources[pstGpuResource->svxGpuResourceDesc] = *pstGpuResource;

	return true;
}

inline bool __VXReleaseGPUResource(SVXGPUResourceArchive* pstGpuResArchive)
{
	if(pstGpuResArchive->svxGpuResourceDesc.eGpuSdkType != vxgGpuSdkTypeDX11)
		return false;

	switch(pstGpuResArchive->svxGpuResourceDesc.eResourceType)
	{
	case vxgResourceTypeDX11RESOURCE:
		{
			for(int i = 0; i < (int)pstGpuResArchive->vtrPtrs.size(); i++)
			{
				ID3D11Resource* pdx11Resource = (ID3D11Resource*)pstGpuResArchive->vtrPtrs.at(i);
				VXSAFE_RELEASE(pdx11Resource);
			}
		}
		break;
	case vxgResourceTypeDX11SRV:
	case vxgResourceTypeDX11RTV:
	case vxgResourceTypeDX11UAV:
	case vxgResourceTypeDX11DSV:
		{
			for(int i = 0; i < (int)pstGpuResArchive->vtrPtrs.size(); i++)
			{
				ID3D11View* pdx11View = (ID3D11View*)pstGpuResArchive->vtrPtrs.at(i);
				VXSAFE_RELEASE(pdx11View);
			}
		}
		break;
	default:
		return false;
	}
	pstGpuResArchive->vtrPtrs.clear();

	//if(pstGpuResArchive->svxGpuResourceDesc.eResourceType == vxgResourceTypeDX11RESOURCE)
	//{
	//	wprintf(_T("Release DX11 Resource, Id:%d, Misc:%d%d%d%d(%s)\n")
	//		, pstGpuResArchive->svxGpuResourceDesc.iSourceObjectID, pstGpuResArchive->svxGpuResourceDesc.iCustomMisc, (int)pstGpuResArchive->svxGpuResourceDesc.eDataType
	//		, pstGpuResArchive->svxGpuResourceDesc.iSizeStride, (int)pstGpuResArchive->svxGpuResourceDesc.eResourceType
	//		, pstGpuResArchive->svxGpuResourceDesc.strUsageSpecifier.c_str());
	//}
	//else
	//{
	//	if(pstGpuResArchive->svxGpuResourceDesc.eResourceType == vxgResourceTypeDX11SRV)
	//	{
	//		printf("Release DX11 View(SRV), ");
	//	}
	//	else if(pstGpuResArchive->svxGpuResourceDesc.eResourceType == vxgResourceTypeDX11RTV)
	//	{
	//		printf("Release DX11 View(RTV), ");
	//	}
	//	else if(pstGpuResArchive->svxGpuResourceDesc.eResourceType == vxgResourceTypeDX11DSV)
	//	{
	//		printf("Release DX11 View(DSV), ");
	//	}
	//	else if(pstGpuResArchive->svxGpuResourceDesc.eResourceType == vxgResourceTypeDX11UAV)
	//	{
	//		printf("Release DX11 View(UAV), ");
	//	}
	//	wprintf(_T("Id:%d, Misc:%d%d%d%d(%s)\n")
	//		, pstGpuResArchive->svxGpuResourceDesc.iSourceObjectID, pstGpuResArchive->svxGpuResourceDesc.iCustomMisc, (int)pstGpuResArchive->svxGpuResourceDesc.eDataType
	//		, pstGpuResArchive->svxGpuResourceDesc.iSizeStride, (int)pstGpuResArchive->svxGpuResourceDesc.eResourceType
	//		, pstGpuResArchive->svxGpuResourceDesc.strUsageSpecifier.c_str());
	//}

	return true;
}

bool VxgReleaseGpuResource(const SVXGPUResourceDESC* pstGpuResourceDesc, bool bCallClearState)
{
	auto itrResDX11 = g_mapVxgResources.find(*pstGpuResourceDesc);
	if(itrResDX11 == g_mapVxgResources.end())
		return false;

	if(__VXReleaseGPUResource(&itrResDX11->second))
		g_mapVxgResources.erase(itrResDX11);

	g_pdx11DeviceImmContext->Flush();
	if (bCallClearState)
		g_pdx11DeviceImmContext->ClearState();
	return true;
}

bool VxgReleaseGpuResourcesRelatedObject(int iObjectID, bool bCallClearState)
{
	if (iObjectID == 0)
		return false;
	auto itrResDX11 = g_mapVxgResources.begin();
	while(itrResDX11 != g_mapVxgResources.end())
	{
		if(itrResDX11->first.iSourceObjectID != itrResDX11->second.svxGpuResourceDesc.iSourceObjectID)
			VXERRORMESSAGE("ERROR!!!! VxgReleaseGpuResourcesRelatedObject");

		if (itrResDX11->first.iSourceObjectID == iObjectID
			|| itrResDX11->first.iRelatedObjectID == iObjectID)
		{
			if(itrResDX11->second.vtrPtrs.size() == 0)
			{
				VXERRORMESSAGE("ERROR!!!! NULL POINT!!!!");
			}

			if(__VXReleaseGPUResource(&itrResDX11->second))
			{
				g_mapVxgResources.erase(itrResDX11);
				itrResDX11 = g_mapVxgResources.begin();
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

bool VxgReleaseGpuResourcesVObjects()
{
	auto itrResDX11 = g_mapVxgResources.begin();
	while(itrResDX11 != g_mapVxgResources.end())
	{
		if(itrResDX11->first.iSourceObjectID != itrResDX11->second.svxGpuResourceDesc.iSourceObjectID)
			VXERRORMESSAGE("ERROR!!!! ___");
		if(CVXObject::GetObjectTypeFromID(itrResDX11->first.iSourceObjectID) == vxrObjectTypeVOLUME
			|| CVXObject::GetObjectTypeFromID(itrResDX11->first.iSourceObjectID) == vxrObjectTypePRIMITIVE)
		{
			if(__VXReleaseGPUResource(&itrResDX11->second))
			{
				g_mapVxgResources.erase(itrResDX11);
				itrResDX11 = g_mapVxgResources.begin();
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

bool VxgReleaseAllGpuResources()
{
	auto itrResDX11 = g_mapVxgResources.begin();
	for(; itrResDX11 != g_mapVxgResources.end(); itrResDX11++)
	{
		if(itrResDX11->first.iSourceObjectID != itrResDX11->second.svxGpuResourceDesc.iSourceObjectID)
			VXERRORMESSAGE("ERROR!!!! ___");
		__VXReleaseGPUResource(&itrResDX11->second);
	}
	g_mapVxgResources.clear();

	g_pdx11DeviceImmContext->Flush();
	g_pdx11DeviceImmContext->ClearState();

	return true;
}

int VxgRetrenchGpuResourcesForStorage(uint uiRequiredStorageSizeMB, ullong ullMinimumTimeGap, bool bIsTargetOnlyVObject) // Return # of Retrenched resources
{
	// Later
	return 0;
	/*
	// Current Gpu Memory //
	uint uiMinimumGpuSafeMemoryMB = 300;
	uint uiGpuMemory = (uint)(g_adapterDesc.DedicatedVideoMemory/1024/1024);
 	if(uiGpuMemory < uiMinimumGpuSafeMemoryMB)
 	{
 		VXERRORMESSAGE("GPU memory is too small...");
 		return -1;
 	}
	
	uint uiValidGpuMemory = uiGpuMemory - uiMinimumGpuSafeMemoryMB;
	if(uiRequiredStorageSizeMB > uiValidGpuMemory)
	{
		VXERRORMESSAGE("Required GPU memory is too Large... Fail to Retrench!");
		return -1;
	}

	uint uiPrevGpuUsedSize = VxgGetUsedGpuMemorySizeKiloBytes()/1024;
	if(uiPrevGpuUsedSize + uiRequiredStorageSizeMB < uiValidGpuMemory + 10)
		return 0;

	// LRU // At Least One Resource should be released! or return -1
	int iCountRelease = 0;
	ullong ullCurrentTime = VXHGetCurrentTimePack();
	while((int)uiValidGpuMemory - (int)uiPrevGpuUsedSize < (int)uiRequiredStorageSizeMB)
	{
		ullong ullLRUTime = ULLONG_MAX;
		map<llong, SVXGpuResourceIndicator>::iterator itrResDX11 = g_mapVxgResources.begin();
		map<llong, SVXGpuResourceIndicator>::iterator itrLeastRecentUsed = itrResDX11;
		int iCountChecked = 0;
		for(; itrResDX11 != g_mapVxgResources.end(); itrResDX11++)
		{
			if(itrResDX11->second.pdx11Resource != NULL)
			{
				if(bIsTargetOnlyVObject)
				{
					if(!CVXObject::IsVObjectType(itrResDX11->second.svxGpuResourceDesc.iSourceObjectID))
						continue;
				}
				if(itrResDX11->second.ullRecentUsedTime < ullLRUTime)
				{
					ullLRUTime = itrResDX11->second.ullRecentUsedTime;
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
			VxgReleaseGpuResource(&itrLeastRecentUsed->second.svxGpuResourceDesc);
			iCountRelease++;
		}

		uiPrevGpuUsedSize = VxgGetUsedGpuMemorySizeKiloBytes()/1024;
		if(uiPrevGpuUsedSize == 0)
			break;
	}

	if(iCountRelease == 0)
	{
		VXERRORMESSAGE("This Scene Cannot be render out by GPU...");
		return -1;
	}
	printf("\n***************\nRetrenchGpuResources :: %d\n***************\n", iCountRelease);
	
	return iCountRelease;
	/**/
}

bool VxgSetGpuManagerCustomParameters(wstring strParamName, void* pvParameter)
{
	vector<wstring> strNameScript;
	wstring strNameDelimiter = _T("_");
	VXHStringSplit(&strNameScript, &strParamName, &strNameDelimiter);

	if (strNameScript[0].compare(_T("function")) == 0)
	{
		g_mapCustomParameters[strParamName] = pvParameter;
	}
	else
	{
		VXERRORMESSAGE("NOT YET : VxgSetGpuManagerCustomParameters");
		return false;
	}
	return true;
}

bool VxgGetGpuManagerCustomParameters(wstring strParamName, void** ppvParameter)
{
	*ppvParameter = NULL;
	auto itrValue = g_mapCustomParameters.find(strParamName);
	if (itrValue == g_mapCustomParameters.end())
		return false;
	*ppvParameter = itrValue->second;
	return true;
}