#include "VXDX11VObjectGenerator.h"
#include "VXDX11Helper.h"

#define VXSAFE_RELEASE(p)			if(p) { (p)->Release(); (p)=NULL; } 

ID3D11Device* g_pdx11Device = NULL;
ID3D11DeviceContext* g_pdx11DeviceImmContext = NULL;
DXGI_ADAPTER_DESC g_adapterDescGV;

void VxgDeviceSetting(ID3D11Device* pdx11Device, ID3D11DeviceContext* pdx11DeviceImmContext, DXGI_ADAPTER_DESC adapterDesc)
{
	g_pdx11Device = pdx11Device;
	g_pdx11DeviceImmContext = pdx11DeviceImmContext;
	g_adapterDescGV = adapterDesc;
}

bool VxgGenerateGpuResourcePRIMITIVE_VERTEX(const SVXGPUResourceDESC* psvxGpuResourceDesc, const CVXVObjectPrimitive* pCObjectPrimitive, SVXGPUResourceArchive* psvxGpuResource/*out*/, SVXLocalProgress* psvxLocalProgress)
{	
	if(psvxGpuResourceDesc->strUsageSpecifier.compare(_T("BUFFER_PRIMITIVE_VERTEX_LIST")) != 0
		|| psvxGpuResourceDesc->eResourceType != vxgResourceTypeDX11RESOURCE)
		return false;

	SVXPrimitiveDataArchive* psvxPrimitiveArchive = ((CVXVObjectPrimitive*)pCObjectPrimitive)->GetPrimitiveArchive();
	ID3D11Buffer* pdx11BufferVertex = NULL;

	//int iNumVertexDefinitions = (int)psvxPrimitiveArchive->mapVerticeDefinitions.size();
	int iNumVertexDefinitions = psvxPrimitiveArchive->GetNumVertexDefinitions();
	uint uiStructureByteStride = iNumVertexDefinitions*sizeof(vxfloat3);

	D3D11_BUFFER_DESC descBufVertex;
	ZeroMemory(&descBufVertex, sizeof(D3D11_BUFFER_DESC));
	descBufVertex.ByteWidth = uiStructureByteStride*psvxPrimitiveArchive->uiNumVertice;
	descBufVertex.Usage = D3D11_USAGE_DYNAMIC;
	descBufVertex.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	descBufVertex.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	descBufVertex.MiscFlags = NULL;
	descBufVertex.StructureByteStride = uiStructureByteStride;
	if(g_pdx11Device->CreateBuffer(&descBufVertex, NULL, &pdx11BufferVertex) != S_OK)
		return false;

	// Mapping Vertex List Re-alignment
	vector<vxfloat3*> vtrVertexDefinitions;
	vxfloat3* pf3PosVertexPostion = psvxPrimitiveArchive->GetVerticeDefinition(vxrVertexTypePOSITION);
	if(pf3PosVertexPostion)
		vtrVertexDefinitions.push_back(pf3PosVertexPostion);
	vxfloat3* pf3VecVertexNormal = psvxPrimitiveArchive->GetVerticeDefinition(vxrVertexTypeNORMAL);
	if(pf3VecVertexNormal)
		vtrVertexDefinitions.push_back(pf3VecVertexNormal);
	for(int i = 0; i < iNumVertexDefinitions; i++)
	{
		vxfloat3* pf3TexCoord = psvxPrimitiveArchive->GetVerticeDefinition((EnumVXRVertexType)(vxrVertexTypeTEXCOORD0 + i));
		if(pf3TexCoord)
			vtrVertexDefinitions.push_back(pf3TexCoord);
	}

	D3D11_MAPPED_SUBRESOURCE mappedRes;
	g_pdx11DeviceImmContext->Map(pdx11BufferVertex, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes);
	vxfloat3* pf3PosGroup = (vxfloat3*)mappedRes.pData;
	for(uint i = 0; i < psvxPrimitiveArchive->uiNumVertice; i++)
	{
		for(int j = 0; j < iNumVertexDefinitions; j++)
		{
			pf3PosGroup[i*iNumVertexDefinitions + j] = vtrVertexDefinitions[j][i];
		}
	}

	g_pdx11DeviceImmContext->Unmap(pdx11BufferVertex, NULL);
	
	// Register
	psvxGpuResource->Init();
	psvxGpuResource->vtrPtrs.push_back(pdx11BufferVertex);
	psvxGpuResource->i3SizeLogicalResourceInBytes.x = psvxPrimitiveArchive->uiNumVertice * sizeof(vxfloat3);
	psvxGpuResource->svxGpuResourceDesc = *psvxGpuResourceDesc;
	psvxGpuResource->ullRecentUsedTime = VXHGetCurrentTimePack();
	
	return true;
}

bool VxgGenerateGpuResourcePRIMITIVE_INDEX(const SVXGPUResourceDESC* psvxGpuResourceDesc, const CVXVObjectPrimitive* pCObjectPrimitive, SVXGPUResourceArchive* psvxGpuResource/*out*/, SVXLocalProgress* psvxLocalProgress)
{	
	if(psvxGpuResourceDesc->strUsageSpecifier.compare(_T("BUFFER_PRIMITIVE_INDEX_LIST")) != 0
		|| psvxGpuResourceDesc->eResourceType != vxgResourceTypeDX11RESOURCE)
		return false;

	SVXPrimitiveDataArchive* psvxPrimitiveArchive = ((CVXVObjectPrimitive*)pCObjectPrimitive)->GetPrimitiveArchive();

	ID3D11Buffer* pdx11BufferIndex = NULL;

	D3D11_BUFFER_DESC descBufIndex;
	ZeroMemory(&descBufIndex, sizeof(D3D11_BUFFER_DESC));
	descBufIndex.Usage = D3D11_USAGE_DYNAMIC;
	descBufIndex.BindFlags = D3D11_BIND_INDEX_BUFFER;
	descBufIndex.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	descBufIndex.MiscFlags = NULL;
	switch(psvxPrimitiveArchive->ePrimitiveType)	// Actually, this is used in Compute shader's structured buffer
	{
	case vxrPrimitiveTypeLINE:
		descBufIndex.StructureByteStride = sizeof(uint)*2;
		break;
	case vxrPrimitiveTypeTRIANGLE:
		descBufIndex.StructureByteStride = sizeof(uint)*3;
		break;
	default:
		return false;
	}
	if(psvxPrimitiveArchive->bIsPrimitiveStripe)
		descBufIndex.ByteWidth = sizeof(uint)*(psvxPrimitiveArchive->uiNumPrimitives + (psvxPrimitiveArchive->uiStrideIndexListPerPolygon - 1));
	else
		descBufIndex.ByteWidth = sizeof(uint)*psvxPrimitiveArchive->uiStrideIndexListPerPolygon*psvxPrimitiveArchive->uiNumPrimitives;

	if(g_pdx11Device->CreateBuffer(&descBufIndex, NULL, &pdx11BufferIndex) != S_OK)
		return false;

	D3D11_MAPPED_SUBRESOURCE mappedRes;
	g_pdx11DeviceImmContext->Map(pdx11BufferIndex, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes);
	uint* puiIndexPrimitive = (uint*)mappedRes.pData;
	memcpy(puiIndexPrimitive, psvxPrimitiveArchive->puiIndexList, descBufIndex.ByteWidth);
	g_pdx11DeviceImmContext->Unmap(pdx11BufferIndex, NULL);

	// Register
	psvxGpuResource->Init();
	psvxGpuResource->vtrPtrs.push_back(pdx11BufferIndex);
	psvxGpuResource->i3SizeLogicalResourceInBytes.x = descBufIndex.ByteWidth;
	psvxGpuResource->svxGpuResourceDesc = *psvxGpuResourceDesc;
	psvxGpuResource->ullRecentUsedTime = VXHGetCurrentTimePack();

	return true;
}

bool VxgGenerateGpuResourceCMM_TEXT(const SVXGPUResourceDESC* psvxGpuResourceDesc, const CVXVObjectPrimitive* pCObjectPrimitive, SVXGPUResourceArchive* psvxGpuResource/*out*/, SVXLocalProgress* psvxLocalProgress)
{
	if(psvxGpuResourceDesc->strUsageSpecifier.compare(_T("TEXTURE2D_CMM_TEXT")) != 0
		|| psvxGpuResourceDesc->eResourceType != vxgResourceTypeDX11RESOURCE)
		return false;

	SVXPrimitiveDataArchive* psvxPrimitiveArchive = ((CVXVObjectPrimitive*)pCObjectPrimitive)->GetPrimitiveArchive();
	if(psvxPrimitiveArchive->pvTextureResource == NULL)
		return false;

	vxint3 i3TextureWHN;
	((CVXVObjectPrimitive*)pCObjectPrimitive)->GetCustomParameter(_T("_int3_TextureWHN"), &i3TextureWHN);

	ID3D11Texture2D* pdx11TX2D = NULL;

	D3D11_TEXTURE2D_DESC descTX2D;
	ZeroMemory(&descTX2D, sizeof(D3D11_TEXTURE2D_DESC));
	descTX2D.Width = i3TextureWHN.x;
	descTX2D.Height = i3TextureWHN.y;
	descTX2D.MipLevels = 1;
	descTX2D.ArraySize = 1;
	descTX2D.SampleDesc.Count = 1;
	descTX2D.SampleDesc.Quality = 0;
	descTX2D.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	descTX2D.MiscFlags = NULL;
	descTX2D.Usage = D3D11_USAGE_IMMUTABLE;
	descTX2D.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	descTX2D.CPUAccessFlags = NULL;

	D3D11_SUBRESOURCE_DATA subDataRes;
	subDataRes.pSysMem = (void*)psvxPrimitiveArchive->pvTextureResource;
	subDataRes.SysMemPitch = sizeof(vxbyte4)*descTX2D.Width;
	subDataRes.SysMemSlicePitch = sizeof(vxbyte4)*descTX2D.Width*descTX2D.Height;

	if(g_pdx11Device->CreateTexture2D(&descTX2D, &subDataRes, &pdx11TX2D) != S_OK)
		return false;

	// Register
	psvxGpuResource->Init();
	psvxGpuResource->uiResourceRowPitchInBytes = descTX2D.Width * sizeof(vxbyte4);
	psvxGpuResource->i3SizeLogicalResourceInBytes = vxint3(descTX2D.Width * sizeof(vxbyte4), descTX2D.Height, 0);
	psvxGpuResource->vtrPtrs.push_back(pdx11TX2D);
	psvxGpuResource->svxGpuResourceDesc = *psvxGpuResourceDesc;
	psvxGpuResource->ullRecentUsedTime = VXHGetCurrentTimePack();

	return true;
}

// 2011.12.13 Chunks --> Single Volume

template <typename ST, typename TT> bool _GenerateGpuResourceImageStack(const SVXGPUResourceDESC* psvxGpuResourceDesc, const CVXVObjectVolume* pCObjectVolume,
	DXGI_FORMAT eVoxelFormat, int iSampleValueOffset, bool bIsSameType, SVXGPUResourceArchive* psvxGpuResource/*out*/, SVXLocalProgress* psvxLocalProgress)
{
	if (psvxGpuResourceDesc->strUsageSpecifier.compare(_T("TEXTURE2DARRAY_IMAGESTACK")) != 0
		|| psvxGpuResourceDesc->eResourceType != vxgResourceTypeDX11RESOURCE)
		return false;

	const SVXVolumeDataArchive* psvxVolumeArchive = ((CVXVObjectVolume*)pCObjectVolume)->GetVolumeArchive();

	uint uiSizeDataType = psvxGpuResourceDesc->iSizeStride; //VXHGetDataTypeSizeByte(psvxVolumeArchive->eVolumeDataType);

	vxint3 i3SizeExtraBoundary = psvxVolumeArchive->i3SizeExtraBoundary;
	ST** ppstVolume = (ST**)psvxVolumeArchive->ppvVolumeSlices;
	int iSizeSampleAddrX = psvxVolumeArchive->i3VolumeSize.x + 2 * i3SizeExtraBoundary.x;
	int iSizeSampleAddrY = psvxVolumeArchive->i3VolumeSize.y + 2 * i3SizeExtraBoundary.y;
	int iSizeSampleAddrZ = psvxVolumeArchive->i3VolumeSize.z + 2 * i3SizeExtraBoundary.z;


	if (psvxLocalProgress)
	{
		psvxLocalProgress->Init();
	}

	D3D11_TEXTURE2D_DESC descTex2DArray;
	ZeroMemory(&descTex2DArray, sizeof(D3D11_TEXTURE2D_DESC));
	descTex2DArray.Width = (uint)psvxVolumeArchive->i3VolumeSize.x;
	descTex2DArray.Height = (uint)psvxVolumeArchive->i3VolumeSize.y;
	descTex2DArray.ArraySize = (uint)psvxVolumeArchive->i3VolumeSize.z;

	descTex2DArray.MipLevels = 1;
	descTex2DArray.MiscFlags = NULL;
	descTex2DArray.Format = eVoxelFormat;
	descTex2DArray.Usage = D3D11_USAGE_IMMUTABLE;
	descTex2DArray.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	descTex2DArray.CPUAccessFlags = NULL;
	descTex2DArray.SampleDesc.Count = 1;
	descTex2DArray.SampleDesc.Quality = 0;

	D3D11_SUBRESOURCE_DATA* psubDataRes = new D3D11_SUBRESOURCE_DATA[descTex2DArray.ArraySize];
	int iPixels = (int)(descTex2DArray.Width * descTex2DArray.Height);
	TT* pttSlice = NULL;
	if (!bIsSameType)
		pttSlice = new TT[iPixels];

	for (uint i = 0; i < descTex2DArray.ArraySize; i++)
	{
		if (psvxLocalProgress)
		{
			psvxLocalProgress->SetProgress(i, descTex2DArray.ArraySize);
		}

		// TT is same as ST
		if (bIsSameType)
		{
			pttSlice = (TT*)ppstVolume[i];
		}
		else
		{
			for (int j = 0; j < iPixels; j++)
				pttSlice[j] = (TT)((float)ppstVolume[i][j] + (float)iSampleValueOffset);
		}

		psubDataRes[i].pSysMem = pttSlice;
		psubDataRes[i].SysMemPitch = sizeof(TT)*descTex2DArray.Width;
		psubDataRes[i].SysMemSlicePitch = sizeof(TT)*descTex2DArray.Width*descTex2DArray.Height;
	}
	if (!bIsSameType)
		VXSAFE_DELETEARRAY(pttSlice);

	ID3D11Texture2D* pdx11TX2DImageStack = NULL;
	HRESULT hr = g_pdx11Device->CreateTexture2D(&descTex2DArray, psubDataRes, &pdx11TX2DImageStack);
	VXSAFE_DELETEARRAY(psubDataRes);
	if (hr != S_OK)
	{
		VXERRORMESSAGE("_GenerateGpuVObjectResourceVolume - Texture Creation Failure!");
		return false;
	}

	// Register
	psvxGpuResource->Init();
	psvxGpuResource->uiResourceRowPitchInBytes = descTex2DArray.Width * uiSizeDataType;
	psvxGpuResource->uiResourceSlicePitchInBytes = descTex2DArray.Width * uiSizeDataType * descTex2DArray.Height;
	psvxGpuResource->i3SizeLogicalResourceInBytes = vxint3(descTex2DArray.Width * uiSizeDataType, descTex2DArray.Height, descTex2DArray.ArraySize);
	psvxGpuResource->f3SampleIntervalElements = vxfloat3(1.f, 1.f, 1.f);
	psvxGpuResource->vtrPtrs.push_back(pdx11TX2DImageStack);
	psvxGpuResource->svxGpuResourceDesc = *psvxGpuResourceDesc;
	psvxGpuResource->ullRecentUsedTime = VXHGetCurrentTimePack();

	if (psvxLocalProgress)
	{
		psvxLocalProgress->Deinit();
	}

	return true;
}

template <typename ST, typename TT> bool _GenerateGpuResourceVolume(const SVXGPUResourceDESC* pstGpuResourceDesc, const CVXVObjectVolume* pCObjectVolume,
	DXGI_FORMAT eVoxelFormat, int iSampleValueOffset, SVXGPUResourceArchive* psvxGpuResource/*out*/, SVXLocalProgress* psvxLocalProgress)
{
	if((pstGpuResourceDesc->strUsageSpecifier.compare(_T("TEXTURE3D_VOLUME_DEFAULT")) != 0
		&& pstGpuResourceDesc->strUsageSpecifier.compare(_T("TEXTURE3D_VOLUME_DEFAULT_MASK")) != 0
		&& pstGpuResourceDesc->strUsageSpecifier.compare(_T("TEXTURE3D_VOLUME_DOWNSAMPLE")) != 0)
		|| pstGpuResourceDesc->eResourceType != vxgResourceTypeDX11RESOURCE)
		return false;

	const SVXVolumeDataArchive* pstVolumeArchive = ((CVXVObjectVolume*)pCObjectVolume)->GetVolumeArchive();

	uint uiSizeDataType = pstGpuResourceDesc->iSizeStride; //VXHGetDataTypeSizeByte(psvxVolumeArchive->eVolumeDataType);

	vxint3 i3SizeExtraBoundary = pstVolumeArchive->i3SizeExtraBoundary;
	ST** pptVolume = (ST**)pstVolumeArchive->ppvVolumeSlices;
	int iSizeSampleAddrX = pstVolumeArchive->i3VolumeSize.x + 2 * i3SizeExtraBoundary.x;
	int iSizeSampleAddrY = pstVolumeArchive->i3VolumeSize.y + 2 * i3SizeExtraBoundary.y;
	int iSizeSampleAddrZ = pstVolumeArchive->i3VolumeSize.z + 2 * i3SizeExtraBoundary.z;


	if (psvxLocalProgress)
	{
		psvxLocalProgress->Init();
	}

	bool bIsPoreVolume = false;
	((CVXVObjectVolume*)pCObjectVolume)->GetCustomParameter(_T("_bool_IsPoreVolume"), &bIsPoreVolume);

	vxfloat3 f3OffsetSize = vxfloat3(1.f, 1.f, 1.f);

	//////////////////////////////
	// GPU Volume Sample Policy //
	vxdouble3 d3VolumeSize;
	d3VolumeSize.x = pstVolumeArchive->i3VolumeSize.x;
	d3VolumeSize.y = pstVolumeArchive->i3VolumeSize.y;
	d3VolumeSize.z = pstVolumeArchive->i3VolumeSize.z;

	double dHalfCriterionKB = 512.0 * 1024.0;
	pCObjectVolume->GetCustomParameter(_T("_double_ForcedHalfCriterionKB"), &dHalfCriterionKB);
	dHalfCriterionKB = min(max(16.0 * 1024.0, dHalfCriterionKB), 256.0 * 1024.0);	//// Forced To Do //
	if(pstGpuResourceDesc->strUsageSpecifier.compare(_T("TEXTURE3D_VOLUME_DOWNSAMPLE")) == 0)
		dHalfCriterionKB = dHalfCriterionKB / 4;
	bool bIsMaxUnitSample = pstGpuResourceDesc->strUsageSpecifier.compare(_T("TEXTURE3D_VOLUME_DEFAULT_MASK")) == 0;
// 	std::wstringstream strTemp;
// 	strTemp.precision(STRINGPRECISION);
// 	strTemp << dHalfCriterionKB;
	//::MessageBox(NULL, strTemp.str().c_str(), NULL, MB_OK);

	// Compute f3OffsetSize //
	//double dSizeVolumeKB = d3VolumeSize.x * d3VolumeSize.y * d3VolumeSize.z * VXHGetDataTypeSizeByte(psvxVolumeArchive->eVolumeDataType) / 1024.0;
	double dSizeVolumeKB = d3VolumeSize.x * d3VolumeSize.y * d3VolumeSize.z * 2 / 1024.0;
	if(dSizeVolumeKB > dHalfCriterionKB)
	{
		double dRescaleSize = pow(dSizeVolumeKB / dHalfCriterionKB, 1.0/3.0);
		f3OffsetSize.x = f3OffsetSize.y = f3OffsetSize.z = (float)dRescaleSize;
	}

	////////////////////////////////////
	// Texture for Volume Description //
RETRY:
	int iVolumeSizeX = max((int)((float)pstVolumeArchive->i3VolumeSize.x/f3OffsetSize.x), 1);
	int iVolumeSizeY = max((int)((float)pstVolumeArchive->i3VolumeSize.y/f3OffsetSize.y), 1);
	int iVolumeSizeZ = max((int)((float)pstVolumeArchive->i3VolumeSize.z/f3OffsetSize.z), 1);

	if (iVolumeSizeX > 2048)
	{
		f3OffsetSize.x += 0.5f;
		goto RETRY;
	}
	if (iVolumeSizeY > 2048)
	{
		f3OffsetSize.y += 0.5f;
		goto RETRY;
	}
	if (iVolumeSizeZ > 2048)
	{
		f3OffsetSize.z += 0.5f;
		goto RETRY;
	}

	f3OffsetSize.x = (float)pstVolumeArchive->i3VolumeSize.x / (float)iVolumeSizeX;
	f3OffsetSize.y = (float)pstVolumeArchive->i3VolumeSize.y / (float)iVolumeSizeY;
	f3OffsetSize.z = (float)pstVolumeArchive->i3VolumeSize.z / (float)iVolumeSizeZ;

	uint uiSliceCount = 0;

	D3D11_TEXTURE3D_DESC descTex3D;
	ZeroMemory(&descTex3D, sizeof(D3D11_TEXTURE3D_DESC));
	descTex3D.Width = iVolumeSizeX;
	descTex3D.Height = iVolumeSizeY;
	descTex3D.Depth = iVolumeSizeZ;
	descTex3D.MipLevels = 1;
	descTex3D.MiscFlags = NULL;
	descTex3D.Format = eVoxelFormat;
	descTex3D.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	descTex3D.Usage = D3D11_USAGE_IMMUTABLE;
	descTex3D.CPUAccessFlags = NULL;
// 	descTex3D.Usage = D3D11_USAGE_DYNAMIC; // For Free-form Clipping 일단 성능 측정
// 	descTex3D.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;	// NULL
	bool bIsSampleMax = false;
	////////////////////
	// Create Texture //
	ID3D11Texture3D* pdx11TX3DChunk = NULL;

	bool bIsDownSample = false;
	if(f3OffsetSize.x > 1.f ||
		f3OffsetSize.y > 1.f ||
		f3OffsetSize.z > 1.f)
		bIsDownSample = true;

	printf("GPU Uploaded Volume Size : %d KB (%dx%dx%d) %dbytes\n", descTex3D.Width * descTex3D.Height * descTex3D.Depth / 1024  * uiSizeDataType
		, descTex3D.Width, descTex3D.Height, descTex3D.Depth, uiSizeDataType);

	TT* ptChunk = new TT[descTex3D.Width * descTex3D.Height * descTex3D.Depth];
	for(int i = 0; i < (int)descTex3D.Depth; i++)
	{
		// Main Chunk
		if(psvxLocalProgress)
		{
			psvxLocalProgress->SetProgress(uiSliceCount, iVolumeSizeZ);
			uiSliceCount++;
		}

		float fPosZ = 0.5f;
		if(descTex3D.Depth > 1)
			fPosZ = (float)i / (float)(descTex3D.Depth - 1) * (float)(pstVolumeArchive->i3VolumeSize.z - 1);

		for(int j = 0; j < (int)descTex3D.Height; j++)
		{
			float fPosY = 0.5f;
			if(descTex3D.Height > 1)
				fPosY = (float)j / (float)(descTex3D.Height - 1) * (float)(pstVolumeArchive->i3VolumeSize.y - 1);

			for(int k = 0; k < (int)descTex3D.Width; k++)
			{
				TT tVoxelValue = 0;
				if(bIsDownSample)
				{
					float fPosX = 0.5f;
					if(descTex3D.Width > 1)
						fPosX = (float)k / (float)(descTex3D.Width - 1) * (float)(pstVolumeArchive->i3VolumeSize.x - 1);

					vxint3 i3PosSampleVS = vxint3((int)fPosX, (int)fPosY, (int)fPosZ);

					int iMinMaxAddrX = min(max(i3PosSampleVS.x, 0), pstVolumeArchive->i3VolumeSize.x - 1) + i3SizeExtraBoundary.x;
					int iMinMaxAddrNextX = min(max(i3PosSampleVS.x + 1, 0), pstVolumeArchive->i3VolumeSize.x - 1) + i3SizeExtraBoundary.x;
					int iMinMaxAddrY = (min(max(i3PosSampleVS.y, 0), pstVolumeArchive->i3VolumeSize.y - 1) + i3SizeExtraBoundary.y)*iSizeSampleAddrX;
					int iMinMaxAddrNextY = (min(max(i3PosSampleVS.y + 1, 0), pstVolumeArchive->i3VolumeSize.y - 1) + i3SizeExtraBoundary.y)*iSizeSampleAddrX;

					int iSampleAddr0 = iMinMaxAddrX + iMinMaxAddrY;
					int iSampleAddr1 = iMinMaxAddrNextX + iMinMaxAddrY;
					int iSampleAddr2 = iMinMaxAddrX + iMinMaxAddrNextY;
					int iSampleAddr3 = iMinMaxAddrNextX + iMinMaxAddrNextY;
					int iSampleAddrZ0 = i3PosSampleVS.z + i3SizeExtraBoundary.z;
					int iSampleAddrZ1 = i3PosSampleVS.z + i3SizeExtraBoundary.z + 1;

					if(i3PosSampleVS.z < 0)
						iSampleAddrZ0 = iSampleAddrZ1;
					else if(i3PosSampleVS.z >= pstVolumeArchive->i3VolumeSize.z - 1)
						iSampleAddrZ1 = iSampleAddrZ0;

					TT tSampleValues[8];
					tSampleValues[0] = pptVolume[iSampleAddrZ0][iSampleAddr0];
					tSampleValues[1] = pptVolume[iSampleAddrZ0][iSampleAddr1];
					tSampleValues[2] = pptVolume[iSampleAddrZ0][iSampleAddr2];
					tSampleValues[3] = pptVolume[iSampleAddrZ0][iSampleAddr3];
					tSampleValues[4] = pptVolume[iSampleAddrZ1][iSampleAddr0];
					tSampleValues[5] = pptVolume[iSampleAddrZ1][iSampleAddr1];
					tSampleValues[6] = pptVolume[iSampleAddrZ1][iSampleAddr2];
					tSampleValues[7] = pptVolume[iSampleAddrZ1][iSampleAddr3];
					float fSampleTrilinear = 0;

					if(!bIsMaxUnitSample)
					{
						vxfloat3 f3InterpolateRatio;
						f3InterpolateRatio.x = fPosX - i3PosSampleVS.x;
						f3InterpolateRatio.y = fPosY - i3PosSampleVS.y;
						f3InterpolateRatio.z = fPosZ - i3PosSampleVS.z;

						float fInterpolateWeights[8];
						fInterpolateWeights[0] = (1.f - f3InterpolateRatio.z)*(1.f - f3InterpolateRatio.y)*(1.f - f3InterpolateRatio.x);
						fInterpolateWeights[1] = (1.f - f3InterpolateRatio.z)*(1.f - f3InterpolateRatio.y)*f3InterpolateRatio.x;
						fInterpolateWeights[2] = (1.f - f3InterpolateRatio.z)*f3InterpolateRatio.y*(1.f - f3InterpolateRatio.x);
						fInterpolateWeights[3] = (1.f - f3InterpolateRatio.z)*f3InterpolateRatio.y*f3InterpolateRatio.x;
						fInterpolateWeights[4] = f3InterpolateRatio.z*(1.f - f3InterpolateRatio.y)*(1.f - f3InterpolateRatio.x);
						fInterpolateWeights[5] = f3InterpolateRatio.z*(1.f - f3InterpolateRatio.y)*f3InterpolateRatio.x;
						fInterpolateWeights[6] = f3InterpolateRatio.z*f3InterpolateRatio.y*(1.f - f3InterpolateRatio.x);
						fInterpolateWeights[7] = f3InterpolateRatio.z*f3InterpolateRatio.y*f3InterpolateRatio.x;

						for(int m = 0; m < 8; m++)
						{
							fSampleTrilinear += tSampleValues[m] * fInterpolateWeights[m];
						}
					}
					else
					{
						for (int m = 0; m < 8; m++)
						{
							fSampleTrilinear = tSampleValues[m];
							if (fSampleTrilinear > 0)
								break;
						}
					}
					tVoxelValue = (TT)((int)fSampleTrilinear + iSampleValueOffset);
				}
				else
				{
					int iAddrZ = i + i3SizeExtraBoundary.z;
					int iAddrXY = i3SizeExtraBoundary.x + k + (j + i3SizeExtraBoundary.y)*iSizeSampleAddrX;
					tVoxelValue = (TT)((int)pptVolume[iAddrZ][iAddrXY] + iSampleValueOffset);
				}
				if (bIsPoreVolume && tVoxelValue > 254) tVoxelValue = 0;
				ptChunk[k + j*descTex3D.Width + i*descTex3D.Width*descTex3D.Height] = tVoxelValue;
			}
		}
	}

	D3D11_SUBRESOURCE_DATA subDataRes;
	subDataRes.pSysMem = (void*)ptChunk;
	subDataRes.SysMemPitch = sizeof(TT)*descTex3D.Width;
	subDataRes.SysMemSlicePitch = sizeof(TT)*descTex3D.Width*descTex3D.Height;

	if (g_pdx11Device->CreateTexture3D(&descTex3D, &subDataRes, &pdx11TX3DChunk) != S_OK)
	{
		VXSAFE_DELETEARRAY(ptChunk);
		VXERRORMESSAGE("_GenerateGpuVObjectResourceVolume - Texture Creation Failure!");
		f3OffsetSize.x += 0.5f;
		f3OffsetSize.y += 0.5f;
		f3OffsetSize.z += 0.5f;
		goto RETRY;
		//return false;
	}

// 	D3D11_MAPPED_SUBRESOURCE mappedRes;
// 	g_pdx11DeviceImmContext->Map(pdx11TX3DChunk, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes);
	// 	printf("\n\n\n%u\n\n\n\n", sizeof(TT));
	// 	::MessageBox(NULL, L"GG3", NULL, MB_OK);
// 	g_pdx11DeviceImmContext->Unmap(pdx11TX3DChunk, NULL);


	VXSAFE_DELETEARRAY(ptChunk);

	// Register
	psvxGpuResource->Init();
	psvxGpuResource->uiResourceRowPitchInBytes = descTex3D.Width * uiSizeDataType;
	psvxGpuResource->uiResourceSlicePitchInBytes = descTex3D.Width * uiSizeDataType * descTex3D.Height;
	psvxGpuResource->i3SizeLogicalResourceInBytes = vxint3(descTex3D.Width * uiSizeDataType, descTex3D.Height, descTex3D.Depth);
	psvxGpuResource->f3SampleIntervalElements = f3OffsetSize;
	psvxGpuResource->vtrPtrs.push_back(pdx11TX3DChunk);
	psvxGpuResource->svxGpuResourceDesc = *pstGpuResourceDesc;
	psvxGpuResource->ullRecentUsedTime = VXHGetCurrentTimePack();

	if(psvxLocalProgress)
	{
		psvxLocalProgress->Deinit();
	}

	return true;
}

bool VxgGenerateGpuResourceVOLUME(const SVXGPUResourceDESC* pstGpuResourceDesc, const CVXVObjectVolume* pCObjectVolume, SVXGPUResourceArchive* psvxGpuResource/*out*/, SVXLocalProgress* psvxLocalProgress)
{
	// Resource //
	SVXVolumeDataArchive* pstVolArchive = ((CVXVObjectVolume*)pCObjectVolume)->GetVolumeArchive();
	int iValueMin = (int)(pstVolArchive->d2MinMaxValue.x);
	if (pstGpuResourceDesc->strUsageSpecifier.compare(_T("TEXTURE2DARRAY_IMAGESTACK")) == 0)
	{
		switch (pstVolArchive->eVolumeDataType)
		{
		case vxrDataTypeBYTE:
			if (!_GenerateGpuResourceImageStack<byte, byte>(pstGpuResourceDesc, pCObjectVolume, DXGI_FORMAT_R8_UNORM, -iValueMin, true, psvxGpuResource, psvxLocalProgress))
				return false;
			break;
		case vxrDataTypeCHAR:
			if (!_GenerateGpuResourceImageStack<char, byte>(pstGpuResourceDesc, pCObjectVolume, DXGI_FORMAT_R8_UNORM, -iValueMin, false, psvxGpuResource, psvxLocalProgress))
				return false;
			break;
		case vxrDataTypeUNSIGNEDSHORT:
			if (!_GenerateGpuResourceImageStack<ushort, ushort>(pstGpuResourceDesc, pCObjectVolume, DXGI_FORMAT_R16_UNORM, -iValueMin, true, psvxGpuResource, psvxLocalProgress))
				return false;
			break;
		case vxrDataTypeSHORT:
			if (!_GenerateGpuResourceImageStack<short, ushort>(pstGpuResourceDesc, pCObjectVolume, DXGI_FORMAT_R16_UNORM, -iValueMin, false, psvxGpuResource, psvxLocalProgress))
				return false;
			break;
		default:
			VXERRORMESSAGE("DX11GenerateGpuResourceVOLUME - Not supported Data Type");
			return false;
		}
	}
	else
	{
		switch (pstVolArchive->eVolumeDataType)
		{
		case vxrDataTypeBYTE:
			if (!_GenerateGpuResourceVolume<byte, byte>(pstGpuResourceDesc, pCObjectVolume, DXGI_FORMAT_R8_UNORM, -iValueMin, psvxGpuResource, psvxLocalProgress))
				return false;
			break;
		case vxrDataTypeCHAR:
			if (!_GenerateGpuResourceVolume<char, byte>(pstGpuResourceDesc, pCObjectVolume, DXGI_FORMAT_R8_UNORM, -iValueMin, psvxGpuResource, psvxLocalProgress))
				return false;
			break;
		case vxrDataTypeUNSIGNEDSHORT:
			if (!_GenerateGpuResourceVolume<ushort, ushort>(pstGpuResourceDesc, pCObjectVolume, DXGI_FORMAT_R16_UNORM, -iValueMin, psvxGpuResource, psvxLocalProgress))
				return false;
			break;
		case vxrDataTypeSHORT:
			if (!_GenerateGpuResourceVolume<short, ushort>(pstGpuResourceDesc, pCObjectVolume, DXGI_FORMAT_R16_UNORM, -iValueMin, psvxGpuResource, psvxLocalProgress))
				return false;
			break;
		default:
			VXERRORMESSAGE("DX11GenerateGpuResourceVOLUME - Not supported Data Type");
			return false;
		}
	}
	return true;
}

template <typename ST, typename TT> bool _GenerateGpuResourceBRICKS(const SVXGPUResourceDESC* psvxGpuResourceDesc, const CVXVObjectVolume* pCObjectVolume, 
	DXGI_FORMAT eVoxelFormat, int iSampleValueOffset, SVXGPUResourceArchive* psvxGpuResource/*out*/, SVXLocalProgress* psvxLocalProgress)
{
	if(psvxGpuResourceDesc->strUsageSpecifier.compare(_T("TEXTURE3D_VOLUME_BRICKCHUNK")) != 0)
		return false;

	const SVXVolumeDataArchive* psvxVolumeArchive = ((CVXVObjectVolume*)pCObjectVolume)->GetVolumeArchive();
	int iBrickResolution = psvxGpuResourceDesc->iCustomMisc;	// 0 : High Resolution, 1 : Low Resolution
	if(iBrickResolution != 0 && iBrickResolution != 1)
		return false;

	int iSizeBrickBound = 2;
	SVXVolumeBlock* psvxVolumeResBlock = ((CVXVObjectVolume*)pCObjectVolume)->GetVolumeBlock(iBrickResolution); 
	psvxVolumeResBlock->iBrickExtraSize = iSizeBrickBound;	// DEFAULT //

	double dRangeProgessOld = 0;
	if(psvxLocalProgress)
	{
		dRangeProgessOld = psvxLocalProgress->dRangeProgress;
		psvxLocalProgress->dRangeProgress = dRangeProgessOld*0.95;
	}

	uint uiSizeDataType = VXHGetDataTypeSizeByte(psvxVolumeArchive->eVolumeDataType);
	uint uiVolumeSizeKiloBytes = (uint)((ullong)psvxVolumeArchive->i3VolumeSize.x*(ullong)psvxVolumeArchive->i3VolumeSize.y*(ullong)psvxVolumeArchive->i3VolumeSize.z*uiSizeDataType/1024);

	////////////////////////////////////
	// Texture for Volume Description //		
	ST** pptVolumeSlices = (ST**)psvxVolumeArchive->ppvVolumeSlices;
	int iVolumeAddrOffsetX = psvxVolumeArchive->i3VolumeSize.x + psvxVolumeArchive->i3SizeExtraBoundary.x*2;
	bool* pbTaggedActivatedBlocks = psvxVolumeResBlock->pbTaggedActivatedBlocks;
	vxint3 i3BlockExBoundaryNum = psvxVolumeResBlock->i3BlockExBoundaryNum;
	vxint3 i3BlockNumSampleSize = psvxVolumeResBlock->i3BlocksNumber + i3BlockExBoundaryNum * 2;
	int iBlockNumSampleSizeXY = i3BlockNumSampleSize.x * i3BlockNumSampleSize.y;

	vxint3 i3BlockSize = psvxVolumeResBlock->i3BlockSize;
	int iBrickExtraSize = psvxVolumeResBlock->iBrickExtraSize;
	vxint3 i3BlockAddrSize;
	i3BlockAddrSize.x = i3BlockSize.x + iBrickExtraSize*2;
	i3BlockAddrSize.y = i3BlockSize.y + iBrickExtraSize*2;
	i3BlockAddrSize.z = i3BlockSize.z + iBrickExtraSize*2;

	vector<int> vtrBrickIDs;
	int iCountID = 0;
	for (int iZ = 0; iZ < psvxVolumeResBlock->i3BlocksNumber.z; iZ++)
	for (int iY = 0; iY < psvxVolumeResBlock->i3BlocksNumber.y; iY++)
	for (int iX = 0; iX < psvxVolumeResBlock->i3BlocksNumber.x; iX++)
	{
		int iSampleAddr = iX + i3BlockExBoundaryNum.x
			+ (iY + i3BlockExBoundaryNum.y) * i3BlockNumSampleSize.x + (iZ + i3BlockExBoundaryNum.z) * iBlockNumSampleSizeXY;
		if(pbTaggedActivatedBlocks[iSampleAddr])
			vtrBrickIDs.push_back(iCountID);
		iCountID++;
	}
	
	// Given Parameters //
	size_t stSizeSingleBrick = i3BlockAddrSize.x * i3BlockAddrSize.y * i3BlockAddrSize.z * VXHGetDataTypeSizeByte(psvxVolumeArchive->eVolumeDataType);
	size_t stTOTAL_BRICK_SIZE = vtrBrickIDs.size() * stSizeSingleBrick;
	size_t stMAX_SIZE_SINGLE_BRICK_CHUNK = 300 * 1024 * 1024;
	size_t stNUM_TOTAL_BRICKS = vtrBrickIDs.size();
	size_t MAX_NUMX_IN_TEX2D = 2048;
	size_t MAX_NUMY_IN_TEX2D = 2048;

	// Computed Parameters //
	size_t stNUM_CHUNKS = (size_t)ceil((double)stTOTAL_BRICK_SIZE / (double)stMAX_SIZE_SINGLE_BRICK_CHUNK);
	size_t stNUM_BRICKS_IN_SINGLE_BRICK_CHUNK = (size_t)ceil((double)stNUM_TOTAL_BRICKS / (double)stNUM_CHUNKS);
	double dDIM_NUM_SQRT = sqrt((double)stNUM_BRICKS_IN_SINGLE_BRICK_CHUNK);
	size_t stNUM_BRICK_X_IN_TEX2D = min( (size_t)ceil(dDIM_NUM_SQRT), MAX_NUMX_IN_TEX2D );
	size_t stNUM_BRICK_Y_IN_TEX2D = min( (size_t)ceil(dDIM_NUM_SQRT), MAX_NUMY_IN_TEX2D );
	size_t stNUM_BRICK_Z_IN_TEX2D = 1;
	size_t stNEW_NUM_BRICKS_IN_SINGLE_CHUNK = stNUM_BRICK_X_IN_TEX2D * stNUM_BRICK_Y_IN_TEX2D * stNUM_BRICK_Z_IN_TEX2D;
	size_t stNEW_NUM_CHUNKS = (size_t)ceil( (double)stNUM_TOTAL_BRICKS / (double)stNEW_NUM_BRICKS_IN_SINGLE_CHUNK );

	printf("Total Brick Number is %d\n", (int)vtrBrickIDs.size());
	printf("Total Brick Chunk Number is %d\n", (int)stNEW_NUM_CHUNKS);
	printf("Brick Number Dimension is X : %d, Y : %d, Z : 1\n", stNUM_BRICK_X_IN_TEX2D, stNUM_BRICK_Y_IN_TEX2D);

	if(stNEW_NUM_CHUNKS > 3)
	{
		VXERRORMESSAGE("IMPOSSIBLE CASE : NUMBER OF BRICK CHUNKS IS OVER!");
		stNEW_NUM_CHUNKS = 3;//return false;
	}

	D3D11_TEXTURE3D_DESC descTex3D;
	ZeroMemory(&descTex3D, sizeof(D3D11_TEXTURE3D_DESC));
	descTex3D.Width = (uint)stNUM_BRICK_X_IN_TEX2D * i3BlockAddrSize.x;
	descTex3D.Height = (uint)stNUM_BRICK_Y_IN_TEX2D * i3BlockAddrSize.y;
	descTex3D.Depth = (uint)stNUM_BRICK_Z_IN_TEX2D * i3BlockAddrSize.z;
	descTex3D.MipLevels = 1;
	descTex3D.MiscFlags = NULL;
	descTex3D.Format = eVoxelFormat;
	descTex3D.BindFlags = D3D11_BIND_SHADER_RESOURCE;

// 	descTex3D.Usage = D3D11_USAGE_DYNAMIC;
// 	descTex3D.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	descTex3D.Usage = D3D11_USAGE_IMMUTABLE;
	descTex3D.CPUAccessFlags = NULL;

	printf("Required Brick Size : %d KB\n", vtrBrickIDs.size()*i3BlockAddrSize.x*i3BlockAddrSize.y*i3BlockAddrSize.z*sizeof(ST)/1024);
	printf("Required Chunk Size : %d KB\n", descTex3D.Width*descTex3D.Height*descTex3D.Depth*sizeof(ST)/1024 * stNEW_NUM_CHUNKS);

	TT* ptBrickChunk = new TT[descTex3D.Width * descTex3D.Height * descTex3D.Depth];
	
	if(psvxLocalProgress)
		psvxLocalProgress->Init();

	vector<void*> vtrTextures;
	int iNumBricksInChunk = (int)stNEW_NUM_BRICKS_IN_SINGLE_CHUNK;
	int iNumChunks = (int)stNEW_NUM_CHUNKS;
	int iNumTotalActualBricks = (int)vtrBrickIDs.size();
	vxint3 i3NumBricksInTex3D((int)stNUM_BRICK_X_IN_TEX2D, (int)stNUM_BRICK_Y_IN_TEX2D, (int)stNUM_BRICK_Z_IN_TEX2D);
	for(int iIndexChunk = 0; iIndexChunk < iNumChunks; iIndexChunk++)
	{
		memset(ptBrickChunk, 0, sizeof(TT) * descTex3D.Width * descTex3D.Height * descTex3D.Depth);
		for(int i = 0; i < iNumBricksInChunk; i++)
		{
			if(psvxLocalProgress)
				psvxLocalProgress->SetProgress(i + iNumBricksInChunk * iIndexChunk, iNumTotalActualBricks);

			int iBrickIndex = i + iNumBricksInChunk * iIndexChunk;
			if(iBrickIndex >= iNumTotalActualBricks)
				break;
			int iBrickID = vtrBrickIDs[iBrickIndex];

			int iBrickIdZ = iBrickID/(psvxVolumeResBlock->i3BlocksNumber.x*psvxVolumeResBlock->i3BlocksNumber.y);
			int iBrickIdXY = iBrickID%(psvxVolumeResBlock->i3BlocksNumber.x*psvxVolumeResBlock->i3BlocksNumber.y);
			int iBrickIdY = iBrickIdXY/psvxVolumeResBlock->i3BlocksNumber.x;
			int iBrickIdX = iBrickIdXY%psvxVolumeResBlock->i3BlocksNumber.x;

			for(int iAddrVoxZ = 0; iAddrVoxZ < i3BlockAddrSize.z; iAddrVoxZ++)	// Down Sampling 시도!
			{
				for(int iAddrVoxY = 0; iAddrVoxY < i3BlockAddrSize.y; iAddrVoxY++)
				{
					for(int iAddrVoxX = 0; iAddrVoxX < i3BlockAddrSize.x; iAddrVoxX++)
					{
						int iAddrWholeZ = max(min((iBrickIdZ*i3BlockSize.z - iBrickExtraSize + psvxVolumeArchive->i3SizeExtraBoundary.z) + iAddrVoxZ, 
							psvxVolumeArchive->i3VolumeSize.z + psvxVolumeArchive->i3SizeExtraBoundary.z*2 - 1), 0);
						int iAddrWholeY = max(min((iBrickIdY*i3BlockSize.y - iBrickExtraSize + psvxVolumeArchive->i3SizeExtraBoundary.y) + iAddrVoxY,
							psvxVolumeArchive->i3VolumeSize.y + psvxVolumeArchive->i3SizeExtraBoundary.y*2 - 1), 0);
						int iAddrWholeX = max(min((iBrickIdX*i3BlockSize.x - iBrickExtraSize + psvxVolumeArchive->i3SizeExtraBoundary.x) + iAddrVoxX,
							psvxVolumeArchive->i3VolumeSize.x + psvxVolumeArchive->i3SizeExtraBoundary.x*2 - 1), 0);
						int iAddrWholeXY = iAddrWholeY*iVolumeAddrOffsetX + iAddrWholeX;

						vxint3 i3ChunkBrickOffset;
						i3ChunkBrickOffset.z = i / (i3NumBricksInTex3D.x * i3NumBricksInTex3D.y);
						i3ChunkBrickOffset.y = (i - i3ChunkBrickOffset.z * i3NumBricksInTex3D.x * i3NumBricksInTex3D.y) / i3NumBricksInTex3D.x;
						i3ChunkBrickOffset.x = i - i3ChunkBrickOffset.z * i3NumBricksInTex3D.x * i3NumBricksInTex3D.y - i3ChunkBrickOffset.y * i3NumBricksInTex3D.x;

						int iBrickAddrInChunk = (iAddrVoxZ + i3ChunkBrickOffset.z * i3BlockAddrSize.z) * descTex3D.Width*descTex3D.Height
							+ (iAddrVoxY + i3ChunkBrickOffset.y * i3BlockAddrSize.y) * descTex3D.Width + (iAddrVoxX + i3ChunkBrickOffset.x * i3BlockAddrSize.x);
						ptBrickChunk[iBrickAddrInChunk] = pptVolumeSlices[iAddrWholeZ][iAddrWholeXY] + iSampleValueOffset;
					}
				}
			}
		}

		ID3D11Texture3D* pdx11TX3DChunk = NULL;
		D3D11_SUBRESOURCE_DATA subDataRes;
		subDataRes.pSysMem = (void*)ptBrickChunk;
		subDataRes.SysMemPitch = sizeof(TT)*descTex3D.Width;
		subDataRes.SysMemSlicePitch = sizeof(TT)*descTex3D.Width*descTex3D.Height;

		if(g_pdx11Device->CreateTexture3D(&descTex3D, &subDataRes, &pdx11TX3DChunk) != S_OK)
		{
			VXSAFE_DELETEARRAY(ptBrickChunk);
			VXERRORMESSAGE("_GenerateGpuResourceBRICKS - Texture Creation Failure!");
			return false;
		}

		vtrTextures.push_back(pdx11TX3DChunk);
	}


	VXSAFE_DELETEARRAY(ptBrickChunk);
	
	psvxGpuResource->Init();
	psvxGpuResource->uiResourceRowPitchInBytes = descTex3D.Width * uiSizeDataType;
	psvxGpuResource->uiResourceSlicePitchInBytes = descTex3D.Width * uiSizeDataType * descTex3D.Height;
	psvxGpuResource->i3SizeLogicalResourceInBytes = vxint3(descTex3D.Width * uiSizeDataType, descTex3D.Height, descTex3D.Depth);
	psvxGpuResource->f3SampleIntervalElements = vxfloat3(1.f, 1.f, 1.f);
	psvxGpuResource->vtrPtrs = vtrTextures;
	psvxGpuResource->svxGpuResourceDesc = *psvxGpuResourceDesc;
	psvxGpuResource->ullRecentUsedTime = VXHGetCurrentTimePack();
	
	if(psvxLocalProgress)
	{
		psvxLocalProgress->Deinit();
		psvxLocalProgress->dRangeProgress = dRangeProgessOld*0.05;
	}

	return true;
}

bool _GenerateGpuResourceBRICKSMAP(const SVXGPUResourceDESC* psvxGpuResourceDesc, const CVXVObjectVolume* pCObjectVolume, 
	SVXGPUResourceArchive* psvxGpuResource/*out*/, SVXLocalProgress* psvxLocalProgress)
{
	if(psvxGpuResourceDesc->strUsageSpecifier.compare(_T("TEXTURE3D_VOLUME_BLOCKMAP")) != 0
		|| psvxGpuResourceDesc->eResourceType != vxgResourceTypeDX11RESOURCE)
		return false;

	const SVXVolumeDataArchive* psvxVolumeArchive = ((CVXVObjectVolume*)pCObjectVolume)->GetVolumeArchive();
	int iBrickResolution = psvxGpuResourceDesc->iCustomMisc;	// 0 : High Resolution, 1 : Low Resolution
	const SVXVolumeBlock* psvxVolumeResBlock = ((CVXVObjectVolume*)pCObjectVolume)->GetVolumeBlock(iBrickResolution); // 0 : High Resolution, 1 : Low Resolution

	////////////////////////////////////
	// Texture for Volume Description //
	D3D11_TEXTURE3D_DESC descTex3DMap;
	ZeroMemory(&descTex3DMap, sizeof(D3D11_TEXTURE3D_DESC));
	descTex3DMap.Width = psvxVolumeResBlock->i3BlocksNumber.x;
	descTex3DMap.Height = psvxVolumeResBlock->i3BlocksNumber.y;
	descTex3DMap.Depth = psvxVolumeResBlock->i3BlocksNumber.z;
	descTex3DMap.MipLevels = 1;
	descTex3DMap.MiscFlags = NULL;

	switch(psvxGpuResourceDesc->eDataType)
	{
	case vxrDataTypeUNSIGNEDSHORT:
		descTex3DMap.Format = DXGI_FORMAT_R16_UNORM;
		break;
	case vxrDataTypeFLOAT:
		descTex3DMap.Format = DXGI_FORMAT_R32_FLOAT;
		break;
	default:
		return false;
	}
	
#define BLOCKMAPIMMUTABLE
#ifdef BLOCKMAPIMMUTABLE
	descTex3DMap.Usage = D3D11_USAGE_IMMUTABLE;
	descTex3DMap.CPUAccessFlags = NULL;
#else
	descTex3DMap.Usage = D3D11_USAGE_DYNAMIC;
	descTex3DMap.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
#endif
	descTex3DMap.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	if(psvxLocalProgress)
		*psvxLocalProgress->pdProgressOfCurWork = (psvxLocalProgress->dStartProgress);

	float* pfBlocksMap = NULL;
	ushort* pusBlocksMap = NULL;
	if (!HDx11AllocAndUpdateBlockMap(&pfBlocksMap, &pusBlocksMap, psvxVolumeResBlock, descTex3DMap.Format, psvxLocalProgress))
		return false;

	// Create Texture //
	ID3D11Texture3D* pdx11TX3DChunk = NULL;
#ifdef BLOCKMAPIMMUTABLE
	D3D11_SUBRESOURCE_DATA subDataRes;
	if(descTex3DMap.Format == DXGI_FORMAT_R32_FLOAT)
	{
		subDataRes.pSysMem = pfBlocksMap;
		subDataRes.SysMemPitch = sizeof(float)*descTex3DMap.Width;
		subDataRes.SysMemSlicePitch = sizeof(float)*descTex3DMap.Width*descTex3DMap.Height;
	}
	else
	{
		subDataRes.pSysMem = pusBlocksMap;
		subDataRes.SysMemPitch = sizeof(ushort)*descTex3DMap.Width;
		subDataRes.SysMemSlicePitch = sizeof(ushort)*descTex3DMap.Width*descTex3DMap.Height;
	}
	HRESULT hr = g_pdx11Device->CreateTexture3D(&descTex3DMap, &subDataRes, &pdx11TX3DChunk);
#else
	HRESULT hr = g_pdx11Device->CreateTexture3D(&descTex3DMap, NULL, &pdx11TX3DChunk);
	D3D11_MAPPED_SUBRESOURCE d11MappedRes;
	g_pdx11DeviceImmContext->Map(pdx11TX3DChunk, 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes);

	if(descTex3DMap.Format == DXGI_FORMAT_R32_FLOAT)
	{
		float* pfBlkMap = (float*)d11MappedRes.pData;
		for(int i = 0; i < (int)descTex3DMap.Depth; i++)
			for(int j = 0; j < (int)descTex3DMap.Height; j++)
			{
				memcpy(&pfBlkMap[j*d11MappedRes.RowPitch/4 + i*d11MappedRes.DepthPitch/4], &pfBlocksMap[j*descTex3DMap.Width + i*descTex3DMap.Width*descTex3DMap.Height]
				, sizeof(float) * descTex3DMap.Width);
			}
	}
	else
	{
		ushort* pusBlkMap = (ushort*)d11MappedRes.pData;
		for(int i = 0; i < (int)descTex3DMap.Depth; i++)
			for(int j = 0; j < (int)descTex3DMap.Height; j++)
			{
				memcpy(&pusBlkMap[j*d11MappedRes.RowPitch/2 + i*d11MappedRes.DepthPitch/2], &pusBlocksMap[j*descTex3DMap.Width + i*descTex3DMap.Width*descTex3DMap.Height]
				, sizeof(ushort) * descTex3DMap.Width);
			}
	}
	g_pdx11DeviceImmContext->Unmap(pdx11TX3DChunk, 0);
#endif

	if(descTex3DMap.Format == DXGI_FORMAT_R32_FLOAT)
	{
		VXSAFE_DELETEARRAY(pfBlocksMap);
	}
	else
	{
		VXSAFE_DELETEARRAY(pusBlocksMap);
	}

	if(psvxLocalProgress)
	{
		*psvxLocalProgress->pdProgressOfCurWork = (psvxLocalProgress->dStartProgress + psvxLocalProgress->dRangeProgress);
	}

	if(hr != S_OK)
	{
		VXERRORMESSAGE("_GenerateGpuVObjectResourceBRICKSMAP - Texture Creation Failure!");
		return false;
	}

	// Register
	psvxGpuResource->Init();
	psvxGpuResource->uiResourceRowPitchInBytes = sizeof(ushort)*descTex3DMap.Width;
	psvxGpuResource->uiResourceSlicePitchInBytes = sizeof(ushort)*descTex3DMap.Width*descTex3DMap.Height;
	psvxGpuResource->i3SizeLogicalResourceInBytes = vxint3(psvxGpuResource->uiResourceRowPitchInBytes, descTex3DMap.Height, descTex3DMap.Depth);
	psvxGpuResource->vtrPtrs.push_back(pdx11TX3DChunk);
	psvxGpuResource->svxGpuResourceDesc = *psvxGpuResourceDesc;
	psvxGpuResource->ullRecentUsedTime = VXHGetCurrentTimePack();

	return true;
}

bool _GenerateGpuResourceMinMaxBlock(const SVXGPUResourceDESC* psvxGpuResourceDesc, const CVXVObjectVolume* pCObjectVolume, 
	SVXGPUResourceArchive* psvxGpuResource/*out*/, SVXLocalProgress* psvxLocalProgress)
{
	if(psvxGpuResourceDesc->strUsageSpecifier.compare(_T("TEXTURE3D_VOLUME_MINMAXBLOCK")) != 0
		|| psvxGpuResourceDesc->eResourceType != vxgResourceTypeDX11RESOURCE)
		return false;

	const SVXVolumeDataArchive* psvxVolumeArchive = ((CVXVObjectVolume*)pCObjectVolume)->GetVolumeArchive();
	int iBrickResolution = psvxGpuResourceDesc->iCustomMisc;	// 0 : High Resolution, 1 : Low Resolution
	const SVXVolumeBlock* psvxVolumeResBlock = ((CVXVObjectVolume*)pCObjectVolume)->GetVolumeBlock(iBrickResolution); // 0 : High Resolution, 1 : Low Resolution

	////////////////////////////////////
	// Texture for Volume Description //
	D3D11_TEXTURE3D_DESC descTex3DMap;
	ZeroMemory(&descTex3DMap, sizeof(D3D11_TEXTURE3D_DESC));
	descTex3DMap.Width = psvxVolumeResBlock->i3BlocksNumber.x;
	descTex3DMap.Height = psvxVolumeResBlock->i3BlocksNumber.y;
	descTex3DMap.Depth = psvxVolumeResBlock->i3BlocksNumber.z;
	descTex3DMap.MipLevels = 1;
	descTex3DMap.MiscFlags = NULL;

	if(psvxGpuResourceDesc->eDataType != vxrDataTypeUNSIGNEDSHORT)
		return false;

	descTex3DMap.Format = DXGI_FORMAT_R16_UNORM;
	descTex3DMap.Usage = D3D11_USAGE_IMMUTABLE;
	descTex3DMap.CPUAccessFlags = NULL;
	descTex3DMap.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	uint uiNumBlocks = (uint)psvxVolumeResBlock->i3BlocksNumber.x*(uint)psvxVolumeResBlock->i3BlocksNumber.y*(uint)psvxVolumeResBlock->i3BlocksNumber.z;

	if(psvxLocalProgress)
		*psvxLocalProgress->pdProgressOfCurWork = (psvxLocalProgress->dStartProgress);

	vxushort2* pus2BlockValues = (vxushort2*)psvxVolumeResBlock->pvMinMaxBlocks;
	vxint3 i3BlockExBoundaryNum = psvxVolumeResBlock->i3BlockExBoundaryNum;
	vxint3 i3BlockNumSampleSize = psvxVolumeResBlock->i3BlocksNumber + i3BlockExBoundaryNum * 2;
	int iBlockNumSampleSizeXY = i3BlockNumSampleSize.x * i3BlockNumSampleSize.y;
	int iBlockNumXY = psvxVolumeResBlock->i3BlocksNumber.x * psvxVolumeResBlock->i3BlocksNumber.y;

	ushort* pusBlockMinValue = new ushort[uiNumBlocks];
	ushort* pusBlockMaxValue = new ushort[uiNumBlocks];
	uint uiCount = 0;
	for (int iZ = 0; iZ < psvxVolumeResBlock->i3BlocksNumber.z; iZ++)
	{
		if (psvxLocalProgress)
			*psvxLocalProgress->pdProgressOfCurWork = (psvxLocalProgress->dStartProgress + psvxLocalProgress->dRangeProgress*iZ / psvxVolumeResBlock->i3BlocksNumber.z);
		for (int iY = 0; iY < psvxVolumeResBlock->i3BlocksNumber.y; iY++)
		for (int iX = 0; iX < psvxVolumeResBlock->i3BlocksNumber.x; iX++)
		{
			vxushort2 us2BlockValue = pus2BlockValues[(iX + i3BlockExBoundaryNum.x) 
				+ (iY + i3BlockExBoundaryNum.y) * i3BlockNumSampleSize.x + (iZ + i3BlockExBoundaryNum.z) * iBlockNumSampleSizeXY];

			int iAddrBlockInGpu = iX + iY * psvxVolumeResBlock->i3BlocksNumber.x + iZ * iBlockNumXY;
			pusBlockMinValue[iAddrBlockInGpu] = us2BlockValue.x;
			pusBlockMaxValue[iAddrBlockInGpu] = us2BlockValue.y;
		}
	}

	// Create Texture //
	ID3D11Texture3D* pdx11TX3DBlockMin = NULL;
	D3D11_SUBRESOURCE_DATA subDataResMin;
	subDataResMin.pSysMem = pusBlockMinValue;
	subDataResMin.SysMemPitch = sizeof(ushort)*descTex3DMap.Width;
	subDataResMin.SysMemSlicePitch = sizeof(ushort)*descTex3DMap.Width*descTex3DMap.Height;
	HRESULT hr = g_pdx11Device->CreateTexture3D(&descTex3DMap, &subDataResMin, &pdx11TX3DBlockMin);
	VXSAFE_DELETEARRAY(pusBlockMinValue);

	ID3D11Texture3D* pdx11TX3DBlockMax = NULL;
	D3D11_SUBRESOURCE_DATA subDataResMax;
	subDataResMax.pSysMem = pusBlockMaxValue;
	subDataResMax.SysMemPitch = sizeof(ushort)*descTex3DMap.Width;
	subDataResMax.SysMemSlicePitch = sizeof(ushort)*descTex3DMap.Width*descTex3DMap.Height;
	hr = g_pdx11Device->CreateTexture3D(&descTex3DMap, &subDataResMax, &pdx11TX3DBlockMax);
	VXSAFE_DELETEARRAY(pusBlockMaxValue);

	if(psvxLocalProgress)
		*psvxLocalProgress->pdProgressOfCurWork = (psvxLocalProgress->dStartProgress + psvxLocalProgress->dRangeProgress);

	if(hr != S_OK)
	{
		VXERRORMESSAGE("_GenerateGpuVObjectResourceBRICKSMAP - Texture Creation Failure!");
		return false;
	}

	// Register
	psvxGpuResource->Init();
	psvxGpuResource->uiResourceRowPitchInBytes = sizeof(ushort)*descTex3DMap.Width;
	psvxGpuResource->uiResourceSlicePitchInBytes = sizeof(ushort)*descTex3DMap.Width*descTex3DMap.Height;
	psvxGpuResource->i3SizeLogicalResourceInBytes = vxint3(psvxGpuResource->uiResourceRowPitchInBytes, descTex3DMap.Height, descTex3DMap.Depth);
	psvxGpuResource->vtrPtrs.push_back(pdx11TX3DBlockMin);
	psvxGpuResource->vtrPtrs.push_back(pdx11TX3DBlockMax);
	psvxGpuResource->svxGpuResourceDesc = *psvxGpuResourceDesc;
	psvxGpuResource->ullRecentUsedTime = VXHGetCurrentTimePack();

	return true;
}

bool VxgGenerateGpuResourceBRICKS(const SVXGPUResourceDESC* psvxGpuResourceDesc, const CVXVObjectVolume* pCObjectVolume, SVXGPUResourceArchive* psvxGpuResource/*out*/, SVXLocalProgress* psvxLocalProgress)
{
	// Resource //
	SVXVolumeDataArchive* psvxVolArchive = ((CVXVObjectVolume*)pCObjectVolume)->GetVolumeArchive();
	int iValueMin = (int)(psvxVolArchive->d2MinMaxValue.x);
	switch(psvxVolArchive->eVolumeDataType)
	{
	case vxrDataTypeBYTE:
		if(!_GenerateGpuResourceBRICKS<byte, byte>(psvxGpuResourceDesc, pCObjectVolume, DXGI_FORMAT_R8_UNORM, -iValueMin, psvxGpuResource, psvxLocalProgress))
			return false;
		break;
	case vxrDataTypeCHAR:
		if(!_GenerateGpuResourceBRICKS<char, byte>(psvxGpuResourceDesc, pCObjectVolume, DXGI_FORMAT_R8_UNORM, -iValueMin, psvxGpuResource, psvxLocalProgress))
			return false;
		break;
	case vxrDataTypeUNSIGNEDSHORT:
		if(!_GenerateGpuResourceBRICKS<ushort, ushort>(psvxGpuResourceDesc, pCObjectVolume, DXGI_FORMAT_R16_UNORM, -iValueMin, psvxGpuResource, psvxLocalProgress))
			return false;
		break;
	case vxrDataTypeSHORT:
		if(!_GenerateGpuResourceBRICKS<short, ushort>(psvxGpuResourceDesc, pCObjectVolume, DXGI_FORMAT_R16_UNORM, -iValueMin, psvxGpuResource, psvxLocalProgress))
			return false;
		break;
	default:
		VXERRORMESSAGE("DX11GenerateGpuResourceBRICKS - Not supported Data Type");
		return false;
	}
	return true;
}

bool VxgGenerateGpuResourceBRICKSMAP(const SVXGPUResourceDESC* psvxGpuResourceDesc, const CVXVObjectVolume* pCObjectVolume, SVXGPUResourceArchive* psvxGpuResource/*out*/, SVXLocalProgress* psvxLocalProgress)
{
	if(!_GenerateGpuResourceBRICKSMAP(psvxGpuResourceDesc, pCObjectVolume, psvxGpuResource, psvxLocalProgress))
		return false;
	return true;
}

bool VxgGenerateGpuResourceBlockMinMax(const SVXGPUResourceDESC* psvxGpuResourceDesc, const CVXVObjectVolume* pCObjectVolume, SVXGPUResourceArchive* psvxGpuResource/*out*/, SVXLocalProgress* psvxLocalProgress)
{
	if(!_GenerateGpuResourceMinMaxBlock(psvxGpuResourceDesc, pCObjectVolume, psvxGpuResource, psvxLocalProgress))
		return false;
	return true;
}

bool VxgGenerateGpuResourceFRAMEBUFFER(const SVXGPUResourceDESC* psvxGpuResourceDesc, const CVXIObject* pCIObject, SVXGPUResourceArchive* psvxGpuResource/*out*/, SVXLocalProgress* psvxLocalProgress)
{
	if(psvxGpuResourceDesc->eResourceType != vxgResourceTypeDX11RESOURCE)
		return false;
	
	vxint2 i2SizeScreen;
	vxint3 i3SizeScreenCurrent;
	((CVXIObject*)pCIObject)->GetFrameBufferInfo(&i2SizeScreen);
	i3SizeScreenCurrent.x = i2SizeScreen.x;
	i3SizeScreenCurrent.y = i2SizeScreen.y;
	i3SizeScreenCurrent.z = 0;

	if(psvxGpuResourceDesc->strUsageSpecifier.compare(_T("TEXTURE2D_IOBJECT_DEPTHSTENCIL")) == 0
		|| psvxGpuResourceDesc->strUsageSpecifier.compare(_T("TEXTURE2D_IOBJECT_RENDEROUT")) == 0
		|| psvxGpuResourceDesc->strUsageSpecifier.compare(_T("TEXTURE2D_IOBJECT_ONLYRESOURCE")) == 0
		|| psvxGpuResourceDesc->strUsageSpecifier.compare(_T("TEXTURE2D_IOBJECT_SYSTEM")) == 0)
	{
		DXGI_FORMAT eDXGI_Format = DXGI_FORMAT_UNKNOWN;
		switch(psvxGpuResourceDesc->eDataType)
		{
		case vxrDataTypeBYTE: eDXGI_Format = DXGI_FORMAT_R8_UNORM; break;
		case vxrDataTypeBYTE4CHANNEL: eDXGI_Format = DXGI_FORMAT_R8G8B8A8_UNORM; break;
		case vxrDataTypeUNSIGNEDSHORT: eDXGI_Format = DXGI_FORMAT_R16_UNORM; break;
		case vxrDataTypeSHORT: eDXGI_Format = DXGI_FORMAT_R16_SNORM; break;
		case vxrDataTypeFLOAT: eDXGI_Format = DXGI_FORMAT_R32_FLOAT; break;
		case vxrDataTypeFLOAT2CHANNEL: eDXGI_Format = DXGI_FORMAT_R32G32_FLOAT; break;
		case vxrDataTypeFLOAT4CHANNEL: eDXGI_Format = DXGI_FORMAT_R32G32B32A32_FLOAT; break;
		default:
			VXERRORMESSAGE("NOT SUPPORTED DATAFORMAT in Dojo's DX11 Interface : CONTACT korfriend@gmail.com");
			break;
		}

		ID3D11Texture2D* pdx11TX2D = NULL;

		D3D11_TEXTURE2D_DESC descTX2D;
		ZeroMemory(&descTX2D, sizeof(D3D11_TEXTURE2D_DESC));
		descTX2D.Width = i3SizeScreenCurrent.x;
		descTX2D.Height = i3SizeScreenCurrent.y;
		descTX2D.MipLevels = 1;
		descTX2D.ArraySize = 1;
		descTX2D.SampleDesc.Count = 1;
		descTX2D.SampleDesc.Quality = 0;
		descTX2D.MiscFlags = NULL;

		if(psvxGpuResourceDesc->strUsageSpecifier.compare(_T("TEXTURE2D_IOBJECT_DEPTHSTENCIL")) == 0)
		{
			if(psvxGpuResourceDesc->eDataType != vxrDataTypeFLOAT)
				VXERRORMESSAGE("FORCED to SET DXGI_FORMAT_D32_FLOAT!");
			descTX2D.Format = DXGI_FORMAT_D32_FLOAT;
			descTX2D.Usage = D3D11_USAGE_DEFAULT;
			descTX2D.BindFlags = D3D11_BIND_DEPTH_STENCIL;
			descTX2D.CPUAccessFlags = NULL;

			if(g_pdx11Device->CreateTexture2D(&descTX2D, NULL, &pdx11TX2D) != S_OK)
				return false;

			// Register
			psvxGpuResource->Init();
			psvxGpuResource->uiResourceRowPitchInBytes = descTX2D.Width * sizeof(float);
			psvxGpuResource->i3SizeLogicalResourceInBytes = vxint3(descTX2D.Width * sizeof(float), descTX2D.Height, 0);
			psvxGpuResource->vtrPtrs.push_back(pdx11TX2D);
			psvxGpuResource->svxGpuResourceDesc = *psvxGpuResourceDesc;
			psvxGpuResource->ullRecentUsedTime = VXHGetCurrentTimePack();
		}
		else if(psvxGpuResourceDesc->strUsageSpecifier.compare(_T("TEXTURE2D_IOBJECT_RENDEROUT")) == 0
			|| psvxGpuResourceDesc->strUsageSpecifier.compare(_T("TEXTURE2D_IOBJECT_ONLYRESOURCE")) == 0
			|| psvxGpuResourceDesc->strUsageSpecifier.compare(_T("TEXTURE2D_IOBJECT_SYSTEM")) == 0)
		{
			descTX2D.Format = eDXGI_Format;
			int iNumTextures = max(psvxGpuResourceDesc->iCustomMisc & 0xFF, 1);
			if(psvxGpuResourceDesc->strUsageSpecifier.compare(_T("TEXTURE2D_IOBJECT_RENDEROUT")) == 0)
			{
				
				descTX2D.Usage = D3D11_USAGE_DEFAULT;
				descTX2D.BindFlags |= D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
				descTX2D.CPUAccessFlags = NULL;
			}
			else if(psvxGpuResourceDesc->strUsageSpecifier.compare(_T("TEXTURE2D_IOBJECT_SYSTEM")) == 0)
			{
				descTX2D.Usage = D3D11_USAGE_STAGING;
				descTX2D.BindFlags = NULL;
				descTX2D.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;	// D3D11_CPU_ACCESS_WRITE 140818 에 추가 by Dojo
			}
			else if(psvxGpuResourceDesc->strUsageSpecifier.compare(_T("TEXTURE2D_IOBJECT_ONLYRESOURCE")) == 0)
			{
				descTX2D.Usage = D3D11_USAGE_DEFAULT;
				descTX2D.BindFlags = D3D11_BIND_SHADER_RESOURCE;
				descTX2D.CPUAccessFlags = NULL;
			}

			// Register
			psvxGpuResource->Init();

			for(int i = 0; i < iNumTextures; i++)
			{
				if(g_pdx11Device->CreateTexture2D(&descTX2D, NULL, &pdx11TX2D) != S_OK)
					return false;
				psvxGpuResource->vtrPtrs.push_back(pdx11TX2D);
			}

			psvxGpuResource->uiResourceRowPitchInBytes = descTX2D.Width * sizeof(float);
			psvxGpuResource->i3SizeLogicalResourceInBytes = vxint3(descTX2D.Width * sizeof(float), descTX2D.Height, 0);
			psvxGpuResource->svxGpuResourceDesc = *psvxGpuResourceDesc;
			psvxGpuResource->ullRecentUsedTime = VXHGetCurrentTimePack();
		}
	}
	else if(psvxGpuResourceDesc->strUsageSpecifier.compare(_T("BUFFER_IOBJECT_RESULTOUT")) == 0
		|| psvxGpuResourceDesc->strUsageSpecifier.compare(_T("BUFFER_IOBJECT_SHADOWMAP")) == 0
		|| psvxGpuResourceDesc->strUsageSpecifier.compare(_T("BUFFER_IOBJECT_SYSTEM")) == 0)
	{
		if(psvxGpuResourceDesc->eDataType != vxrDataTypeSTRUCTURED)
			return false;

		ID3D11Buffer* pdx11BufUnordered = NULL;
		uint uiGridSizeX = 8;
		uint uiGridSizeY = 8;
		int iGridSizeFromMisc = psvxGpuResourceDesc->iCustomMisc & 0xFFFF;
		int iNumBuffers = max((psvxGpuResourceDesc->iCustomMisc & 0xFFFF0000) >> 16, 1);
		if(iGridSizeFromMisc > 1)
		{
			uiGridSizeX = uiGridSizeY = iGridSizeFromMisc;
		}
		else
		{
			printf("FORCED TO GRID SIZE 8x8 in Dojo's DX11 Interface : CONTACT korfriend@gmail.com\n");
		}

		uint uiNumGridX = (uint)ceil(i2SizeScreen.x/(float)uiGridSizeX);
		uint uiNumGridY = (uint)ceil(i2SizeScreen.y/(float)uiGridSizeY);	// TO DO // mod-times is not necessary
		uint uiNumThreads = uiNumGridX*uiNumGridY*uiGridSizeX*uiGridSizeY; // TO DO // mod-times is not necessary

		D3D11_BUFFER_DESC descBuf;
		ZeroMemory(&descBuf, sizeof(D3D11_BUFFER_DESC));
		descBuf.ByteWidth =  psvxGpuResourceDesc->iSizeStride*uiNumThreads;
		descBuf.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		descBuf.StructureByteStride = psvxGpuResourceDesc->iSizeStride;

		if(psvxGpuResourceDesc->strUsageSpecifier.compare(_T("BUFFER_IOBJECT_SYSTEM")) == 0)
		{
			descBuf.BindFlags = NULL;
			descBuf.Usage = D3D11_USAGE_STAGING;
			descBuf.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		}
		else
		{
			descBuf.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
			descBuf.Usage = D3D11_USAGE_DEFAULT;
			descBuf.CPUAccessFlags = NULL;
		}

		// Register
		psvxGpuResource->Init();
		for(int i = 0; i < iNumBuffers; i++)
		{
			// Create Buffer and Register
			if(g_pdx11Device->CreateBuffer(&descBuf, NULL, &pdx11BufUnordered) != S_OK)
				return false;
			psvxGpuResource->vtrPtrs.push_back(pdx11BufUnordered);
		}
		psvxGpuResource->uiResourceRowPitchInBytes = descBuf.ByteWidth;
		psvxGpuResource->i3SizeLogicalResourceInBytes = vxint3(descBuf.ByteWidth, 0, 0);
		psvxGpuResource->svxGpuResourceDesc = *psvxGpuResourceDesc;
		psvxGpuResource->ullRecentUsedTime = VXHGetCurrentTimePack();
	}

	return true;
}

bool VxgGenerateGpuResourceOTF(const SVXGPUResourceDESC* psvxGpuResourceDesc, const CVXTObject* pCTObject, SVXGPUResourceArchive* psvxGpuResource/*out*/, SVXLocalProgress* psvxLocalProgress)
{
	if((psvxGpuResourceDesc->strUsageSpecifier.compare(_T("TEXTURE2D_TOBJECT_2D")) != 0
		&& psvxGpuResourceDesc->strUsageSpecifier.compare(_T("BUFFER_TOBJECT_OTFSERIES")) != 0)
		|| psvxGpuResourceDesc->eResourceType != vxgResourceTypeDX11RESOURCE)
		return false;

	SVXTransferFunctionArchive* psvxTfArchive = ((CVXTObject*)pCTObject)->GetTransferFunctionArchive();

	if (psvxGpuResourceDesc->strUsageSpecifier.compare(_T("BUFFER_TOBJECT_OTFSERIES")) == 0)
	{
		if (psvxTfArchive->iNumDims < 1)
			return false;

		int iNumOTFSeries = 0;
		pCTObject->GetCustomParameter(_T("_int_NumOTFSeries"), &iNumOTFSeries);
		iNumOTFSeries = max(iNumOTFSeries, 1);

		D3D11_BUFFER_DESC descBuf;
		ZeroMemory(&descBuf, sizeof(D3D11_BUFFER_DESC));
		descBuf.ByteWidth = psvxTfArchive->i3DimSize.x * iNumOTFSeries * psvxGpuResourceDesc->iSizeStride;
		descBuf.MiscFlags = NULL;// D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		descBuf.StructureByteStride = psvxGpuResourceDesc->iSizeStride;
		descBuf.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		descBuf.Usage = D3D11_USAGE_DYNAMIC;// D3D11_USAGE_DEFAULT;
		descBuf.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;// NULL;

		if (psvxGpuResourceDesc->iCustomMisc == 1)
			descBuf.ByteWidth = 256 * psvxGpuResourceDesc->iSizeStride;	// 

		ID3D11Buffer* pdx11BufOTF = NULL;
		if (g_pdx11Device->CreateBuffer(&descBuf, NULL, &pdx11BufOTF) != S_OK)
		{
			VXERRORMESSAGE("Called SimpleVR Module's VxgGenerateGpuResourceOTF - BUFFER_TOBJECT_OTFSERIES FAILURE!");
			VXSAFE_RELEASE(pdx11BufOTF);
			return false;
		}

		// Register
		psvxGpuResource->Init();
		psvxGpuResource->uiResourceRowPitchInBytes = descBuf.ByteWidth;
		psvxGpuResource->i3SizeLogicalResourceInBytes = vxint3(descBuf.ByteWidth, 0, 0);
		psvxGpuResource->vtrPtrs.push_back(pdx11BufOTF);
		psvxGpuResource->svxGpuResourceDesc = *psvxGpuResourceDesc;
		psvxGpuResource->ullRecentUsedTime = VXHGetCurrentTimePack();
	}
	else if(psvxGpuResourceDesc->strUsageSpecifier.compare(_T("TEXTURE2D_TOBJECT_2D")) == 0)
	{
		if(psvxTfArchive->iNumDims < 2 || (psvxTfArchive->i3DimSize.y <= 0))
			return false;

		if(psvxTfArchive->i3DimSize.x > 2048 || psvxTfArchive->i3DimSize.y > 2048)
		{
			VXERRORMESSAGE("Too Large 2D OTF more binning needed!");
			return false;
		}
		vxint2 i2OtfTextureSize;
		i2OtfTextureSize.x = min(psvxTfArchive->i3DimSize.x, 2048);
		i2OtfTextureSize.y = min(psvxTfArchive->i3DimSize.y, 2048);

		D3D11_TEXTURE2D_DESC descTex2DOTF;
		ZeroMemory(&descTex2DOTF, sizeof(D3D11_TEXTURE2D_DESC));
		descTex2DOTF.Width = i2OtfTextureSize.x;
		descTex2DOTF.Height = i2OtfTextureSize.y;
		descTex2DOTF.MipLevels = 1;
		descTex2DOTF.ArraySize = 1;
		descTex2DOTF.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		descTex2DOTF.SampleDesc.Count = 1;			
		descTex2DOTF.SampleDesc.Quality = 0;		
		descTex2DOTF.Usage = D3D11_USAGE_DYNAMIC;
		descTex2DOTF.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		descTex2DOTF.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		descTex2DOTF.MiscFlags = NULL;

		ID3D11Texture2D* pdx11TX2DOtf = NULL;
		g_pdx11Device->CreateTexture2D(&descTex2DOTF, NULL, &pdx11TX2DOtf);
				
		// Register
		psvxGpuResource->Init();
		psvxGpuResource->uiResourceRowPitchInBytes = descTex2DOTF.Width * sizeof(vxfloat4);
		psvxGpuResource->i3SizeLogicalResourceInBytes = vxint3(descTex2DOTF.Width * sizeof(vxfloat4), descTex2DOTF.Height, 0);
		psvxGpuResource->vtrPtrs.push_back(pdx11TX2DOtf);
		psvxGpuResource->svxGpuResourceDesc = *psvxGpuResourceDesc;
		psvxGpuResource->ullRecentUsedTime = VXHGetCurrentTimePack();
	}
	else
	{
		return false;
	}

	return true;
}

bool VxgGenerateGpuResBinderAsView(const SVXGPUResourceArchive* psvxGpuResource, EnumVXGResourceType eResourceType, SVXGPUResourceArchive* psvxGpuResourceOut/*out*/)
{
	if(psvxGpuResource->svxGpuResourceDesc.eResourceType != vxgResourceTypeDX11RESOURCE
		|| eResourceType == vxgResourceTypeDX11RESOURCE)
		return false;

	vector<void*> vtrViews;

	if(psvxGpuResource->svxGpuResourceDesc.strUsageSpecifier.compare(_T("TEXTURE3D_VOLUME_DEFAULT")) == 0
		|| psvxGpuResource->svxGpuResourceDesc.strUsageSpecifier.compare(_T("TEXTURE3D_VOLUME_DEFAULT_MASK")) == 0
		|| psvxGpuResource->svxGpuResourceDesc.strUsageSpecifier.compare(_T("TEXTURE3D_VOLUME_DOWNSAMPLE")) == 0
		|| psvxGpuResource->svxGpuResourceDesc.strUsageSpecifier.compare(_T("TEXTURE3D_VOLUME_BLOCKMAP")) == 0
		|| psvxGpuResource->svxGpuResourceDesc.strUsageSpecifier.compare(_T("TEXTURE3D_VOLUME_MINMAXBLOCK")) == 0)
	{
		if(eResourceType != vxgResourceTypeDX11SRV)
		{
			VXERRORMESSAGE("3D TEXTURE VOLUME RESOURCE SUPPORTS ONLY SRV! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
			return false;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC descSRVVolData;
		if(psvxGpuResource->svxGpuResourceDesc.eDataType == vxrDataTypeBYTE)
			descSRVVolData.Format = DXGI_FORMAT_R8_UNORM;
		else if(psvxGpuResource->svxGpuResourceDesc.eDataType == vxrDataTypeUNSIGNEDSHORT)
			descSRVVolData.Format = DXGI_FORMAT_R16_UNORM;
		else if(psvxGpuResource->svxGpuResourceDesc.eDataType == vxrDataTypeFLOAT)
			descSRVVolData.Format = DXGI_FORMAT_R32_FLOAT;
		else
		{
			VXERRORMESSAGE("NOT SUPPORTED DATAFORMAT! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
			return false;
		}
		descSRVVolData.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
		descSRVVolData.Texture3D.MipLevels = 1;
		descSRVVolData.Texture3D.MostDetailedMip = 0;

		for(int i = 0; i < (int)psvxGpuResource->vtrPtrs.size(); i++)
		{
			ID3D11ShaderResourceView* pdx11View;
			if(g_pdx11Device->CreateShaderResourceView((ID3D11Resource*)psvxGpuResource->vtrPtrs.at(i), 
				&descSRVVolData, (ID3D11ShaderResourceView**)&pdx11View) != S_OK)
			{
				VXERRORMESSAGE("Volume Resource View Creation Failure! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
				return false;
			}
			vtrViews.push_back(pdx11View);
		}
	}
	else if(psvxGpuResource->svxGpuResourceDesc.strUsageSpecifier.compare(_T("TEXTURE3D_VOLUME_BRICKCHUNK")) == 0)
	{
		if(eResourceType != vxgResourceTypeDX11SRV)
		{
			VXERRORMESSAGE("3D TEXTURE VOLUME RESOURCE SUPPORTS ONLY SRV! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
			return false;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC descSRVVolData;
		if(psvxGpuResource->svxGpuResourceDesc.eDataType == vxrDataTypeBYTE)
			descSRVVolData.Format = DXGI_FORMAT_R8_UNORM;
		else if(psvxGpuResource->svxGpuResourceDesc.eDataType == vxrDataTypeUNSIGNEDSHORT)
			descSRVVolData.Format = DXGI_FORMAT_R16_UNORM;
		else
		{
			VXERRORMESSAGE("NOT SUPPORTED DATAFORMAT! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
			return false;
		}
		descSRVVolData.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
		descSRVVolData.Texture3D.MipLevels = 1;
		descSRVVolData.Texture3D.MostDetailedMip = 0;
		for(int i = 0; i < psvxGpuResource->vtrPtrs.size(); i++)
		{
			ID3D11Resource* pdx11Res = (ID3D11Resource*)psvxGpuResource->vtrPtrs.at(i);
			ID3D11View* pdx11View = NULL;
			if(g_pdx11Device->CreateShaderResourceView(pdx11Res, &descSRVVolData, (ID3D11ShaderResourceView**)&pdx11View) != S_OK)
			{
				VXERRORMESSAGE("Volume Resource View Creation Failure! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
				return false;
			}
			vtrViews.push_back(pdx11View);
		}
	}
	else if (psvxGpuResource->svxGpuResourceDesc.strUsageSpecifier.compare(_T("TEXTURE2DARRAY_IMAGESTACK")) == 0)
	{
		if (eResourceType != vxgResourceTypeDX11SRV)
		{
			VXERRORMESSAGE("2D TEXTUREARRAY VOLUME RESOURCE SUPPORTS ONLY SRV! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
			return false;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC descSRVVolData;
		if (psvxGpuResource->svxGpuResourceDesc.eDataType == vxrDataTypeBYTE)
			descSRVVolData.Format = DXGI_FORMAT_R8_UNORM;
		else if (psvxGpuResource->svxGpuResourceDesc.eDataType == vxrDataTypeUNSIGNEDSHORT)
			descSRVVolData.Format = DXGI_FORMAT_R16_UNORM;
		else
		{
			VXERRORMESSAGE("NOT SUPPORTED DATAFORMAT! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
			return false;
		}
		descSRVVolData.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		descSRVVolData.Texture2DArray.ArraySize = psvxGpuResource->i3SizeLogicalResourceInBytes.z;
		descSRVVolData.Texture2DArray.FirstArraySlice = 0;
		descSRVVolData.Texture2DArray.MipLevels = 1;
		descSRVVolData.Texture2DArray.MostDetailedMip = 0;
		for (int i = 0; i < psvxGpuResource->vtrPtrs.size(); i++)
		{
			ID3D11Resource* pdx11Res = (ID3D11Resource*)psvxGpuResource->vtrPtrs.at(i);
			ID3D11View* pdx11View = NULL;
			if (g_pdx11Device->CreateShaderResourceView(pdx11Res, &descSRVVolData, (ID3D11ShaderResourceView**)&pdx11View) != S_OK)
			{
				VXERRORMESSAGE("Image Stack Resource View Creation Failure! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
				return false;
			}
			vtrViews.push_back(pdx11View);
		}
	}
	else if(psvxGpuResource->svxGpuResourceDesc.strUsageSpecifier.compare(_T("TEXTURE2D_IOBJECT_DEPTHSTENCIL")) == 0)
	{
		if(eResourceType != vxgResourceTypeDX11DSV)
		{
			VXERRORMESSAGE("DEPTHSTENCIL 2D TEXTURE SUPPORTS ONLY DSV! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
			return false;
		}

		if(psvxGpuResource->svxGpuResourceDesc.eDataType != vxrDataTypeFLOAT)
		{
			VXERRORMESSAGE("NOT SUPPORTED DATAFORMAT! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
			return false;
		}

		D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
		ZeroMemory(&descDSV, sizeof(D3D11_DEPTH_STENCIL_VIEW_DESC));
		descDSV.Format = DXGI_FORMAT_D32_FLOAT;
		descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		descDSV.Flags = 0;
		descDSV.Texture2D.MipSlice = 0;

		for(int i = 0; i < (int)psvxGpuResource->vtrPtrs.size(); i++)
		{
			ID3D11View* pdx11View = NULL;
			if(g_pdx11Device->CreateDepthStencilView((ID3D11Resource*)psvxGpuResource->vtrPtrs.at(i), 
				&descDSV, (ID3D11DepthStencilView**)&pdx11View) != S_OK)
			{
				VXERRORMESSAGE("Depth Stencil View Creation Failure! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
				return false;
			}
			vtrViews.push_back(pdx11View);
		}
	}
	else if(psvxGpuResource->svxGpuResourceDesc.strUsageSpecifier.compare(_T("TEXTURE2D_IOBJECT_DEPTHSTENCIL")) == 0
		|| psvxGpuResource->svxGpuResourceDesc.strUsageSpecifier.compare(_T("TEXTURE2D_IOBJECT_RENDEROUT")) == 0
		|| psvxGpuResource->svxGpuResourceDesc.strUsageSpecifier.compare(_T("TEXTURE2D_IOBJECT_SYSTEM")) == 0
		|| psvxGpuResource->svxGpuResourceDesc.strUsageSpecifier.compare(_T("TEXTURE2D_TOBJECT_2D")) == 0)
	{
		if(eResourceType == vxgResourceTypeDX11DSV)
		{
			VXERRORMESSAGE("NORMAL 2D TEXTURE DO NOT SUPPORT DSV! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
			return false;
		}
		if((psvxGpuResource->svxGpuResourceDesc.strUsageSpecifier.compare(_T("TEXTURE2D_TOBJECT_2D")) == 0)
			&& eResourceType == vxgResourceTypeDX11RTV)
		{
			VXERRORMESSAGE("OTF 2D TEXTURE DO NOT SUPPORT RTV! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
			return false;
		}
		if(psvxGpuResource->svxGpuResourceDesc.strUsageSpecifier.compare(_T("TEXTURE2D_IOBJECT_SYSTEM")) == 0)
		{
			VXERRORMESSAGE("OTF 2D TEXTURE SYSTEM DO NOT SUPPORT VIEW! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
			return false;
		}

		DXGI_FORMAT eDXGI_Format = DXGI_FORMAT_UNKNOWN;
		switch(psvxGpuResource->svxGpuResourceDesc.eDataType)
		{
		case vxrDataTypeBYTE: eDXGI_Format = DXGI_FORMAT_R8_UNORM; break;
		case vxrDataTypeBYTE4CHANNEL: eDXGI_Format = DXGI_FORMAT_R8G8B8A8_UNORM; break;
		case vxrDataTypeUNSIGNEDSHORT: eDXGI_Format = DXGI_FORMAT_R16_UNORM; break;
		case vxrDataTypeSHORT: eDXGI_Format = DXGI_FORMAT_R16_SNORM; break;
		case vxrDataTypeFLOAT: eDXGI_Format = DXGI_FORMAT_R32_FLOAT; break;
		case vxrDataTypeFLOAT2CHANNEL: eDXGI_Format = DXGI_FORMAT_R32G32_FLOAT; break;
		case vxrDataTypeFLOAT4CHANNEL: eDXGI_Format = DXGI_FORMAT_R32G32B32A32_FLOAT; break;
		default:
			VXERRORMESSAGE("NOT SUPPORTED DATAFORMAT in Dojo's DX11 Interface : CONTACT korfriend@gmail.com");
			return false;
		}

		if(eResourceType == vxgResourceTypeDX11SRV)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
			ZeroMemory(&descSRV, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
			descSRV.Format = eDXGI_Format;
			descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			descSRV.Texture2D.MostDetailedMip = 0;
			descSRV.Texture2D.MipLevels = 1;

			for(int i = 0; i < (int)psvxGpuResource->vtrPtrs.size(); i++)
			{
				ID3D11View* pdx11View = NULL;
				if(g_pdx11Device->CreateShaderResourceView((ID3D11Resource*)psvxGpuResource->vtrPtrs.at(i), 
					&descSRV, (ID3D11ShaderResourceView**)&pdx11View) != S_OK)
				{
					VXERRORMESSAGE("Shader Resource View Creation Failure! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
					return false;
				}
				vtrViews.push_back(pdx11View);
			}
		}
		else if(eResourceType == vxgResourceTypeDX11RTV)
		{
			D3D11_RENDER_TARGET_VIEW_DESC descRTV;
			ZeroMemory(&descRTV, sizeof(D3D11_RENDER_TARGET_VIEW_DESC));
			descRTV.Format = eDXGI_Format;
			descRTV.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			descRTV.Texture2D.MipSlice = 0;

			for(int i = 0; i < (int)psvxGpuResource->vtrPtrs.size(); i++)
			{
				ID3D11View* pdx11View = NULL;
				if(g_pdx11Device->CreateRenderTargetView((ID3D11Resource*)psvxGpuResource->vtrPtrs.at(i),
					&descRTV, (ID3D11RenderTargetView**)&pdx11View) != S_OK)
				{
					VXERRORMESSAGE("Render Target View Creation Failure! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
					return false;
				}
				vtrViews.push_back(pdx11View);
			}
		}
	}
	else if(psvxGpuResource->svxGpuResourceDesc.strUsageSpecifier.compare(0, 14, _T("BUFFER_IOBJECT")) == 0
		|| psvxGpuResource->svxGpuResourceDesc.strUsageSpecifier.compare(0, 24, _T("BUFFER_TOBJECT_OTFSERIES")) == 0)
	{
		if(eResourceType == vxgResourceTypeDX11DSV
			|| eResourceType == vxgResourceTypeDX11RTV)
		{
			VXERRORMESSAGE("UNORDERED ACCESS BUFFER DO NOT SUPPORT DSV and RTV! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
			return false;
		}
		if(psvxGpuResource->svxGpuResourceDesc.strUsageSpecifier.compare(_T("BUFFER_IOBJECT_SYSTEM")) == 0)
		{
			VXERRORMESSAGE("UNORDERED ACCESS BUFFER SYSTEM DO NOT SUPPORT VIEW! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
			return false;
		}

		if(eResourceType == vxgResourceTypeDX11UAV)
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC descUAV;
			ZeroMemory(&descUAV, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
			descUAV.Format = DXGI_FORMAT_UNKNOWN;	// Structured RW Buffer
			descUAV.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			descUAV.Buffer.FirstElement = 0;
			descUAV.Buffer.NumElements = psvxGpuResource->i3SizeLogicalResourceInBytes.x / psvxGpuResource->svxGpuResourceDesc.iSizeStride;
			descUAV.Buffer.Flags = 0;

			for(int i = 0; i < (int)psvxGpuResource->vtrPtrs.size(); i++)
			{
				ID3D11View* pdx11View = NULL;
				if(g_pdx11Device->CreateUnorderedAccessView((ID3D11Resource*)psvxGpuResource->vtrPtrs.at(i), 
					&descUAV, (ID3D11UnorderedAccessView**)&pdx11View) != S_OK)
				{
					VXERRORMESSAGE("Unordered Access View Creation Failure! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
					return false;
				}
				vtrViews.push_back(pdx11View);
			}
		}
		else if(eResourceType == vxgResourceTypeDX11SRV)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
			ZeroMemory(&descSRV, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));


			DXGI_FORMAT eDXGI_Format = DXGI_FORMAT_UNKNOWN;
			switch (psvxGpuResource->svxGpuResourceDesc.eDataType)
			{
			case vxrDataTypeBYTE: eDXGI_Format = DXGI_FORMAT_R8_UNORM; break;
			case vxrDataTypeBYTE4CHANNEL: eDXGI_Format = DXGI_FORMAT_R8G8B8A8_UNORM; break;
			case vxrDataTypeUNSIGNEDSHORT: eDXGI_Format = DXGI_FORMAT_R16_UNORM; break;
			case vxrDataTypeSHORT: eDXGI_Format = DXGI_FORMAT_R16_SNORM; break;
			case vxrDataTypeFLOAT: eDXGI_Format = DXGI_FORMAT_R32_FLOAT; break;
			case vxrDataTypeFLOAT2CHANNEL: eDXGI_Format = DXGI_FORMAT_R32G32_FLOAT; break;
			case vxrDataTypeFLOAT4CHANNEL: eDXGI_Format = DXGI_FORMAT_R32G32B32A32_FLOAT; break;
			case vxrDataTypeSTRUCTURED: eDXGI_Format = DXGI_FORMAT_UNKNOWN; break;
			default:
				VXERRORMESSAGE("NOT SUPPORTED DATAFORMAT in Dojo's DX11 Interface : CONTACT korfriend@gmail.com");
				return false;
			}
			descSRV.Format = eDXGI_Format;
			descSRV.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
			descSRV.BufferEx.FirstElement = 0;
			descSRV.BufferEx.NumElements = psvxGpuResource->i3SizeLogicalResourceInBytes.x / psvxGpuResource->svxGpuResourceDesc.iSizeStride;

			for(int i = 0; i < (int)psvxGpuResource->vtrPtrs.size(); i++)
			{
				ID3D11View* pdx11View = NULL;
				if(g_pdx11Device->CreateShaderResourceView((ID3D11Resource*)psvxGpuResource->vtrPtrs.at(i), 
					&descSRV, (ID3D11ShaderResourceView**)&pdx11View) != S_OK)
				{
					VXERRORMESSAGE("Shader Reousrce View Creation Failure! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
					return false;
				}
				vtrViews.push_back(pdx11View);
			}
		}
	}
	else if(psvxGpuResource->svxGpuResourceDesc.strUsageSpecifier.compare(_T("TEXTURE2D_CMM_TEXT")) == 0)
	{
		if(eResourceType == vxgResourceTypeDX11SRV)
		{
			DXGI_FORMAT eDXGI_Format = DXGI_FORMAT_UNKNOWN;
			switch(psvxGpuResource->svxGpuResourceDesc.eDataType)
			{
			case vxrDataTypeBYTE: eDXGI_Format = DXGI_FORMAT_R8_UNORM; break;
			case vxrDataTypeBYTE4CHANNEL: eDXGI_Format = DXGI_FORMAT_R8G8B8A8_UNORM; break;
			case vxrDataTypeUNSIGNEDSHORT: eDXGI_Format = DXGI_FORMAT_R16_UNORM; break;
			case vxrDataTypeSHORT: eDXGI_Format = DXGI_FORMAT_R16_SNORM; break;
			case vxrDataTypeFLOAT: eDXGI_Format = DXGI_FORMAT_R32_FLOAT; break;
			case vxrDataTypeFLOAT4CHANNEL: eDXGI_Format = DXGI_FORMAT_R32G32B32A32_FLOAT; break;
			default:
				VXERRORMESSAGE("NOT SUPPORTED DATAFORMAT in Dojo's DX11 Interface : CONTACT korfriend@gmail.com");
				return false;
			}

			D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
			ZeroMemory(&descSRV, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
			descSRV.Format = eDXGI_Format;
			descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			descSRV.Texture2D.MostDetailedMip = 0;
			descSRV.Texture2D.MipLevels = 1;

			for(int i = 0; i < (int)psvxGpuResource->vtrPtrs.size(); i++)
			{
				ID3D11View* pdx11View = NULL;
				if(g_pdx11Device->CreateShaderResourceView((ID3D11Resource*)psvxGpuResource->vtrPtrs.at(i), 
					&descSRV, (ID3D11ShaderResourceView**)&pdx11View) != S_OK)
				{
					VXERRORMESSAGE("Shader Resource View Creation Failure! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
					return false;
				}
				vtrViews.push_back(pdx11View);
			}
		}
	}

	if(vtrViews.size() == 0)
		return false;

	// Register
	psvxGpuResourceOut->Init();
	psvxGpuResourceOut->uiResourceRowPitchInBytes = psvxGpuResource->uiResourceRowPitchInBytes;
	psvxGpuResourceOut->uiResourceSlicePitchInBytes = psvxGpuResource->uiResourceSlicePitchInBytes;
	psvxGpuResourceOut->i3SizeLogicalResourceInBytes = psvxGpuResource->i3SizeLogicalResourceInBytes;
	psvxGpuResourceOut->f3SampleIntervalElements = psvxGpuResource->f3SampleIntervalElements;
	psvxGpuResourceOut->svxGpuResourceDesc = psvxGpuResource->svxGpuResourceDesc;
	psvxGpuResourceOut->svxGpuResourceDesc.eResourceType = eResourceType;
	psvxGpuResourceOut->ullRecentUsedTime = VXHGetCurrentTimePack();
	psvxGpuResourceOut->vtrPtrs = vtrViews;

	return true;
}

