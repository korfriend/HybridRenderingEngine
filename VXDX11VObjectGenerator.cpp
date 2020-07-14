#include "VXDX11VObjectGenerator.h"
#include "VXDX11Helper.h"

#define VMSAFE_RELEASE(p)			if(p) { (p)->Release(); (p)=NULL; } 
//#define GMERRORMESSAGE(a) printf(a)

ID3D11Device* g_pdx11Device = NULL;
ID3D11DeviceContext* g_pdx11DeviceImmContext = NULL;
DXGI_ADAPTER_DESC g_adapterDescGV;

bool g_bFlatShaderOFF = true;

using namespace std;
using namespace vmmath;
using namespace vmobjects;

void VmDeviceSetting(ID3D11Device* pdx11Device, ID3D11DeviceContext* pdx11DeviceImmContext, DXGI_ADAPTER_DESC adapterDesc)
{
	g_pdx11Device = pdx11Device;
	g_pdx11DeviceImmContext = pdx11DeviceImmContext;
	g_adapterDescGV = adapterDesc;
}

bool VmGenerateGpuResourcePRIMITIVE_VERTEX(const GpuResDescriptor* pGpuResDesc, const VmVObjectPrimitive* pCObjectPrimitive, GpuResource* pGpuResource/*out*/, LocalProgress* pLocalProgress)
{	
	if(pGpuResDesc->usage_specifier.compare("BUFFER_PRIMITIVE_VERTEX_LIST") != 0
		|| pGpuResDesc->res_type != ResourceTypeDX11RESOURCE)
		return false;

	bool bFlatShaderOFF = g_bFlatShaderOFF;
	pCObjectPrimitive->GetCustomParameter("_bool_FlatShaderOFF", data_type::dtype<bool>(), &bFlatShaderOFF);

	PrimitiveData* pstPrimitiveArchive = ((VmVObjectPrimitive*)pCObjectPrimitive)->GetPrimitiveData();
	ID3D11Buffer* pdx11BufferVertex = NULL;

	//int iNumVertexDefinitions = (int)psvxPrimitiveArchive->mapVerticeDefinitions.size();
	int iNumVertexDefinitions = pstPrimitiveArchive->GetNumVertexDefinitions();
	uint uiStructureByteStride = iNumVertexDefinitions*sizeof(vmfloat3);

	// Mapping Vertex List Re-alignment
	vector<vmfloat3*> vtrVertexDefinitions;
	vmfloat3* pf3PosVertexPostion = pstPrimitiveArchive->GetVerticeDefinition("POSITION");
	if (pf3PosVertexPostion)
		vtrVertexDefinitions.push_back(pf3PosVertexPostion);
	vmfloat3* pf3VecVertexNormal = pstPrimitiveArchive->GetVerticeDefinition("NORMAL");
	if (pf3VecVertexNormal)
		vtrVertexDefinitions.push_back(pf3VecVertexNormal);
	for (int i = 0; i < iNumVertexDefinitions; i++)
	{
		vmfloat3* pf3TexCoord = pstPrimitiveArchive->GetVerticeDefinition(string("TEXCOORD") + to_string(i));
		if (pf3TexCoord)
			vtrVertexDefinitions.push_back(pf3TexCoord);
	}

	bool bUseOriginalMeshBuffer = false;
	if (pstPrimitiveArchive->check_redundancy && !bUseOriginalMeshBuffer 
		&& (pf3PosVertexPostion && pf3VecVertexNormal)
		&& pstPrimitiveArchive->ptype == PrimitiveTypeTRIANGLE
		&& !pstPrimitiveArchive->is_stripe
		&& !bFlatShaderOFF)
	{
		int iNumVtx = pstPrimitiveArchive->num_prims * 3;
		int iNumTriIndex = pstPrimitiveArchive->num_prims * 3;


		D3D11_BUFFER_DESC descBufVertex;
		ZeroMemory(&descBufVertex, sizeof(D3D11_BUFFER_DESC));
		descBufVertex.ByteWidth = uiStructureByteStride * iNumVtx;
		descBufVertex.Usage = D3D11_USAGE_DYNAMIC;
		descBufVertex.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		descBufVertex.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		descBufVertex.MiscFlags = NULL;
		descBufVertex.StructureByteStride = uiStructureByteStride;
		if (g_pdx11Device->CreateBuffer(&descBufVertex, NULL, &pdx11BufferVertex) != S_OK)
			return false;

		D3D11_MAPPED_SUBRESOURCE mappedRes;
		g_pdx11DeviceImmContext->Map(pdx11BufferVertex, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes);
		vmfloat3* pf3PosGroup = (vmfloat3*)mappedRes.pData;
		for (uint i = 0; i < pstPrimitiveArchive->num_prims; i++)
		{
			uint uiIdxVtx0 = pstPrimitiveArchive->vidx_buffer[3 * i + 0];
			uint uiIdxVtx1 = pstPrimitiveArchive->vidx_buffer[3 * i + 1];
			uint uiIdxVtx2 = pstPrimitiveArchive->vidx_buffer[3 * i + 2];

			vmfloat3 f3PosVtx0 = pf3PosVertexPostion[uiIdxVtx0];
			vmfloat3 f3PosVtx1 = pf3PosVertexPostion[uiIdxVtx1];
			vmfloat3 f3PosVtx2 = pf3PosVertexPostion[uiIdxVtx2];

			vmfloat3 f3Vec01 = f3PosVtx1 - f3PosVtx0;
			vmfloat3 f3Vec02 = f3PosVtx2 - f3PosVtx0;
			vmfloat3 f3VecNormal;
			fCrossDotVector(&f3VecNormal, &f3Vec01, &f3Vec02);
			fNormalizeVector(&f3VecNormal, &f3VecNormal);

			pf3PosGroup[(3 * i + 0) * iNumVertexDefinitions + 0] = f3PosVtx0;
			pf3PosGroup[(3 * i + 1) * iNumVertexDefinitions + 0] = f3PosVtx1;
			pf3PosGroup[(3 * i + 2) * iNumVertexDefinitions + 0] = f3PosVtx2;

			pf3PosGroup[(3 * i + 0) * iNumVertexDefinitions + 1] = f3VecNormal;
			pf3PosGroup[(3 * i + 1) * iNumVertexDefinitions + 1] = f3VecNormal;
			pf3PosGroup[(3 * i + 2) * iNumVertexDefinitions + 1] = f3VecNormal;

			for (int j = 2; j < iNumVertexDefinitions; j++)
			{
				vmfloat3* pf3TexCoord = vtrVertexDefinitions[j];

				vmfloat3 f3TexVtx0 = pf3TexCoord[uiIdxVtx0];
				vmfloat3 f3TexVtx1 = pf3TexCoord[uiIdxVtx1];
				vmfloat3 f3TexVtx2 = pf3TexCoord[uiIdxVtx2];

				pf3PosGroup[(3 * i + 0) * iNumVertexDefinitions + j] = f3TexVtx0;
				pf3PosGroup[(3 * i + 1) * iNumVertexDefinitions + j] = f3TexVtx1;
				pf3PosGroup[(3 * i + 2) * iNumVertexDefinitions + j] = f3TexVtx2;
			}
		}
		g_pdx11DeviceImmContext->Unmap(pdx11BufferVertex, NULL);

		// Register
		pGpuResource->Init();
		pGpuResource->vtrPtrs.push_back(pdx11BufferVertex);
		pGpuResource->logical_res_bytes.x = iNumVtx * sizeof(vmfloat3);
		pGpuResource->gpu_res_desc = *pGpuResDesc;
		pGpuResource->recent_used_time = vmhelpers::GetCurrentTimePack();
	}
	else
	{
		D3D11_BUFFER_DESC descBufVertex;
		ZeroMemory(&descBufVertex, sizeof(D3D11_BUFFER_DESC));
		descBufVertex.ByteWidth = uiStructureByteStride * pstPrimitiveArchive->num_vtx;
		descBufVertex.Usage = D3D11_USAGE_DYNAMIC;
		descBufVertex.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		descBufVertex.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		descBufVertex.MiscFlags = NULL;
		descBufVertex.StructureByteStride = uiStructureByteStride;
		if (g_pdx11Device->CreateBuffer(&descBufVertex, NULL, &pdx11BufferVertex) != S_OK)
			return false;

		D3D11_MAPPED_SUBRESOURCE mappedRes;
		g_pdx11DeviceImmContext->Map(pdx11BufferVertex, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes);
		vmfloat3* pf3PosGroup = (vmfloat3*)mappedRes.pData;
		for (uint i = 0; i < pstPrimitiveArchive->num_vtx; i++)
		{
			for (int j = 0; j < iNumVertexDefinitions; j++)
			{
				pf3PosGroup[i*iNumVertexDefinitions + j] = vtrVertexDefinitions[j][i];
			}
		}
		g_pdx11DeviceImmContext->Unmap(pdx11BufferVertex, NULL);

		// Register
		pGpuResource->Init();
		pGpuResource->vtrPtrs.push_back(pdx11BufferVertex);
		pGpuResource->logical_res_bytes.x = pstPrimitiveArchive->num_vtx * sizeof(vmfloat3);
		pGpuResource->gpu_res_desc = *pGpuResDesc;
		pGpuResource->recent_used_time = vmhelpers::GetCurrentTimePack();
	}
	
	return true;
}

bool VmGenerateGpuResourcePRIMITIVE_INDEX(const GpuResDescriptor* pGpuResDesc, const VmVObjectPrimitive* pCObjectPrimitive, GpuResource* pGpuResource/*out*/, LocalProgress* pLocalProgress)
{	
	if(pGpuResDesc->usage_specifier.compare(("BUFFER_PRIMITIVE_INDEX_LIST")) != 0
		|| pGpuResDesc->res_type != ResourceTypeDX11RESOURCE)
		return false;

	bool bFlatShaderOFF = g_bFlatShaderOFF;
	pCObjectPrimitive->GetCustomParameter("_bool_FlatShaderOFF", data_type::dtype<bool>(), &bFlatShaderOFF);

	PrimitiveData* pstPrimitiveArchive = ((VmVObjectPrimitive*)pCObjectPrimitive)->GetPrimitiveData();
	vmfloat3* pf3PosVertexPostion = pstPrimitiveArchive->GetVerticeDefinition("POSITION");
	vmfloat3* pf3VecVertexNormal = pstPrimitiveArchive->GetVerticeDefinition("NORMAL");

	ID3D11Buffer* pdx11BufferIndex = NULL;

	D3D11_BUFFER_DESC descBufIndex;
	ZeroMemory(&descBufIndex, sizeof(D3D11_BUFFER_DESC));
	descBufIndex.Usage = D3D11_USAGE_DYNAMIC;
	descBufIndex.BindFlags = D3D11_BIND_INDEX_BUFFER;
	descBufIndex.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	descBufIndex.MiscFlags = NULL;

	bool bUseOriginalMeshBuffer = false;
	if (pstPrimitiveArchive->check_redundancy && !bUseOriginalMeshBuffer
		&& (pf3PosVertexPostion && pf3VecVertexNormal)
		&& pstPrimitiveArchive->ptype == PrimitiveTypeTRIANGLE
		&& !pstPrimitiveArchive->is_stripe
		&& !bFlatShaderOFF)
	{
		descBufIndex.StructureByteStride = sizeof(uint) * 3;
		descBufIndex.ByteWidth = sizeof(uint) * 3 * pstPrimitiveArchive->num_prims;

		if (g_pdx11Device->CreateBuffer(&descBufIndex, NULL, &pdx11BufferIndex) != S_OK)
			return false;

		D3D11_MAPPED_SUBRESOURCE mappedRes;
		g_pdx11DeviceImmContext->Map(pdx11BufferIndex, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes);
		uint* puiIndexPrimitive = (uint*)mappedRes.pData;
		for (uint i = 0; i < pstPrimitiveArchive->num_prims; i++)
		{
			puiIndexPrimitive[3 * i + 0] = 3 * i + 0;
			puiIndexPrimitive[3 * i + 1] = 3 * i + 1;
			puiIndexPrimitive[3 * i + 2] = 3 * i + 2;
		}
		g_pdx11DeviceImmContext->Unmap(pdx11BufferIndex, NULL);
	}
	else
	{
		switch (pstPrimitiveArchive->ptype)	// Actually, this is used in Compute shader's structured buffer
		{
		case PrimitiveTypeLINE:
			descBufIndex.StructureByteStride = sizeof(uint) * 2;
			break;
		case PrimitiveTypeTRIANGLE:
			descBufIndex.StructureByteStride = sizeof(uint) * 3;
			break;
		default:
			return false;
		}
		if (pstPrimitiveArchive->is_stripe)
			descBufIndex.ByteWidth = sizeof(uint) * (pstPrimitiveArchive->num_prims + (pstPrimitiveArchive->idx_stride - 1));
		else
			descBufIndex.ByteWidth = sizeof(uint) * pstPrimitiveArchive->idx_stride * pstPrimitiveArchive->num_prims;

		if (g_pdx11Device->CreateBuffer(&descBufIndex, NULL, &pdx11BufferIndex) != S_OK)
			return false;

		D3D11_MAPPED_SUBRESOURCE mappedRes;
		g_pdx11DeviceImmContext->Map(pdx11BufferIndex, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes);
		uint* puiIndexPrimitive = (uint*)mappedRes.pData;
		memcpy(puiIndexPrimitive, pstPrimitiveArchive->vidx_buffer, descBufIndex.ByteWidth);
		g_pdx11DeviceImmContext->Unmap(pdx11BufferIndex, NULL);
	}

	// Register
	pGpuResource->Init();
	pGpuResource->vtrPtrs.push_back(pdx11BufferIndex);
	pGpuResource->logical_res_bytes.x = descBufIndex.ByteWidth;
	pGpuResource->gpu_res_desc = *pGpuResDesc;
	pGpuResource->recent_used_time = vmhelpers::GetCurrentTimePack();

	return true;
}

bool VmGenerateGpuResourceCMM_TEXT(const GpuResDescriptor* pGpuResDesc, const VmVObjectPrimitive* pCObjectPrimitive, GpuResource* pGpuResource/*out*/, LocalProgress* pLocalProgress)
{
	if(pGpuResDesc->usage_specifier.compare(("TEXTURE2D_CMM_TEXT")) != 0
		|| pGpuResDesc->res_type != ResourceTypeDX11RESOURCE)
		return false;

	PrimitiveData* psvxPrimitiveArchive = ((VmVObjectPrimitive*)pCObjectPrimitive)->GetPrimitiveData();
	if(psvxPrimitiveArchive->texture_res == NULL)
		return false;

	vmint3 i3TextureWHN;
	((VmVObjectPrimitive*)pCObjectPrimitive)->GetCustomParameter("_int3_TextureWHN", data_type::dtype<vmint3>(), &i3TextureWHN);

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
	subDataRes.pSysMem = (void*)psvxPrimitiveArchive->texture_res;
	subDataRes.SysMemPitch = sizeof(vmbyte4)*descTX2D.Width;
	subDataRes.SysMemSlicePitch = sizeof(vmbyte4)*descTX2D.Width*descTX2D.Height;

	if(g_pdx11Device->CreateTexture2D(&descTX2D, &subDataRes, &pdx11TX2D) != S_OK)
		return false;

	// Register
	pGpuResource->Init();
	pGpuResource->resource_row_pitch_bytes = descTX2D.Width * sizeof(vmbyte4);
	pGpuResource->logical_res_bytes = vmint3(descTX2D.Width * sizeof(vmbyte4), descTX2D.Height, 0);
	pGpuResource->vtrPtrs.push_back(pdx11TX2D);
	pGpuResource->gpu_res_desc = *pGpuResDesc;
	pGpuResource->recent_used_time = vmhelpers::GetCurrentTimePack();

	return true;
}

// 2011.12.13 Chunks --> Single Volume
template <typename ST, typename TT> bool _GenerateGpuResourceImageStack(const GpuResDescriptor* pGpuResDesc, const VmVObjectVolume* pCObjectVolume,
	DXGI_FORMAT eVoxelFormat, int iSampleValueOffset, bool bIsSameType, GpuResource* pGpuResource/*out*/, LocalProgress* pLocalProgress)
{
	if (pGpuResDesc->usage_specifier.compare(("TEXTURE2DARRAY_IMAGESTACK")) != 0
		|| pGpuResDesc->res_type != ResourceTypeDX11RESOURCE)
		return false;

	const VolumeData* pVolData = ((VmVObjectVolume*)pCObjectVolume)->GetVolumeData();

	uint uiSizeDataType = pGpuResDesc->dtype.type_bytes; //VXHGetDataTypeSizeByte(pVolData->eVolumeDataType);

	vmint3 bnd_size = pVolData->bnd_size;
	ST** ppstVolume = (ST**)pVolData->vol_slices;
	int iSizeSampleAddrX = pVolData->vol_size.x + 2 * bnd_size.x;
	int iSizeSampleAddrY = pVolData->vol_size.y + 2 * bnd_size.y;
	int iSizeSampleAddrZ = pVolData->vol_size.z + 2 * bnd_size.z;

	if (pLocalProgress)
	{
		pLocalProgress->Init();
	}

	D3D11_TEXTURE2D_DESC descTex2DArray;
	ZeroMemory(&descTex2DArray, sizeof(D3D11_TEXTURE2D_DESC));
	descTex2DArray.Width = (uint)pVolData->vol_size.x;
	descTex2DArray.Height = (uint)pVolData->vol_size.y;
	descTex2DArray.ArraySize = (uint)pVolData->vol_size.z;

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
		if (pLocalProgress)
		{
			pLocalProgress->SetProgress(i, descTex2DArray.ArraySize);
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
		VMSAFE_DELETEARRAY(pttSlice);

	ID3D11Texture2D* pdx11TX2DImageStack = NULL;
	HRESULT hr = g_pdx11Device->CreateTexture2D(&descTex2DArray, psubDataRes, &pdx11TX2DImageStack);
	VMSAFE_DELETEARRAY(psubDataRes);
	if (hr != S_OK)
	{
		GMERRORMESSAGE("_GenerateGpuVObjectResourceVolume - Texture Creation Failure!");
		return false;
	}

	// Register
	pGpuResource->Init();
	pGpuResource->resource_row_pitch_bytes = descTex2DArray.Width * uiSizeDataType;
	pGpuResource->resource_slice_pitch_bytes = descTex2DArray.Width * uiSizeDataType * descTex2DArray.Height;
	pGpuResource->logical_res_bytes = vmint3(descTex2DArray.Width * uiSizeDataType, descTex2DArray.Height, descTex2DArray.ArraySize);
	pGpuResource->sample_interval = vmfloat3(1.f, 1.f, 1.f);
	pGpuResource->vtrPtrs.push_back(pdx11TX2DImageStack);
	pGpuResource->gpu_res_desc = *pGpuResDesc;
	pGpuResource->recent_used_time = vmhelpers::GetCurrentTimePack();

	if (pLocalProgress)
	{
		pLocalProgress->Deinit();
	}

	return true;
}

template <typename ST, typename TT> bool _GenerateGpuResourceVolume(const GpuResDescriptor* pGpuResDesc, const VmVObjectVolume* pCObjectVolume,
	DXGI_FORMAT eVoxelFormat, int iSampleValueOffset, GpuResource* pGpuResource/*out*/, LocalProgress* pLocalProgress)
{
	if ((pGpuResDesc->usage_specifier.compare("TEXTURE3D_VOLUME_DEFAULT") != 0
		&& pGpuResDesc->usage_specifier.compare("TEXTURE3D_VOLUME_DEFAULT_MASK") != 0
		&& pGpuResDesc->usage_specifier.compare("TEXTURE3D_VOLUME_LOA_MASK") != 0
		&& pGpuResDesc->usage_specifier.compare("TEXTURE3D_VOLUME_DOWNSAMPLE") != 0)
		|| pGpuResDesc->res_type != ResourceTypeDX11RESOURCE)
		return false;

	const VolumeData* pVolumeData = ((VmVObjectVolume*)pCObjectVolume)->GetVolumeData();

	uint uiSizeDataType = pGpuResDesc->dtype.type_bytes; //VXHGetDataTypeSizeByte(pVolData->eVolumeDataType);

	vmint3 bnd_size = pVolumeData->bnd_size;
	ST** pptVolume = (ST**)pVolumeData->vol_slices;
	int iSizeSampleAddrX = pVolumeData->vol_size.x + 2 * bnd_size.x;
	int iSizeSampleAddrY = pVolumeData->vol_size.y + 2 * bnd_size.y;
	int iSizeSampleAddrZ = pVolumeData->vol_size.z + 2 * bnd_size.z;


	if (pLocalProgress)
	{
		pLocalProgress->Init();
	}

	bool bIsPoreVolume = false;
	((VmVObjectVolume*)pCObjectVolume)->GetCustomParameter("_bool_UseNearestMax", data_type::dtype<bool>(), &bIsPoreVolume);

	vmfloat3 f3OffsetSize = vmfloat3(1.f, 1.f, 1.f);

	//////////////////////////////
	// GPU Volume Sample Policy //
	vmdouble3 d3VolumeSize;
	d3VolumeSize.x = pVolumeData->vol_size.x;
	d3VolumeSize.y = pVolumeData->vol_size.y;
	d3VolumeSize.z = pVolumeData->vol_size.z;

	double dHalfCriterionKB = 512.0 * 1024.0;
	pCObjectVolume->GetCustomParameter(("_double_ForcedHalfCriterionKB"), data_type::dtype<double>(), &dHalfCriterionKB);
	dHalfCriterionKB = min(max(16.0 * 1024.0, dHalfCriterionKB), 256.0 * 1024.0);	//// Forced To Do //
	if(pGpuResDesc->usage_specifier.compare("TEXTURE3D_VOLUME_DOWNSAMPLE") == 0)
		dHalfCriterionKB = dHalfCriterionKB / 4.0;
	bool bIsMaxUnitSample = pGpuResDesc->usage_specifier.compare("TEXTURE3D_VOLUME_DEFAULT_MASK") == 0;
// 	std::stringstream strTemp;
// 	strTemp.precision(STRINGPRECISION);
// 	strTemp << dHalfCriterionKB;
	//::MessageBox(NULL, strTemp.str().c_str(), NULL, MB_OK);

	// Compute f3OffsetSize //
	//double dSizeVolumeKB = d3VolumeSize.x * d3VolumeSize.y * d3VolumeSize.z * VXHGetDataTypeSizeByte(pVolData->eVolumeDataType) / 1024.0;
	double dSizeVolumeKB = d3VolumeSize.x * d3VolumeSize.y * d3VolumeSize.z * 2.0 / 1024.0;
	if(dSizeVolumeKB > dHalfCriterionKB)
	{
		double dRescaleSize = pow(dSizeVolumeKB / dHalfCriterionKB, 1.0/3.0);
		f3OffsetSize.x = f3OffsetSize.y = f3OffsetSize.z = (float)dRescaleSize;
	}

	////////////////////////////////////
	// Texture for Volume Description //
RETRY:
	int iVolumeSizeX = max((int)((float)pVolumeData->vol_size.x/f3OffsetSize.x), 1);
	int iVolumeSizeY = max((int)((float)pVolumeData->vol_size.y/f3OffsetSize.y), 1);
	int iVolumeSizeZ = max((int)((float)pVolumeData->vol_size.z/f3OffsetSize.z), 1);

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

	f3OffsetSize.x = (float)pVolumeData->vol_size.x / (float)iVolumeSizeX;
	f3OffsetSize.y = (float)pVolumeData->vol_size.y / (float)iVolumeSizeY;
	f3OffsetSize.z = (float)pVolumeData->vol_size.z / (float)iVolumeSizeZ;

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

	if (pGpuResDesc->usage_specifier == "TEXTURE3D_VOLUME_LOA_MASK")
	{
		descTex3D.Usage = D3D11_USAGE_DYNAMIC; // For Free-form Clipping 일단 성능 측정
		descTex3D.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;	// NULL
		descTex3D.Format = DXGI_FORMAT_R16_UNORM; // force to 16 bit encoding
	}
	else
	{
		descTex3D.Usage = D3D11_USAGE_IMMUTABLE;
		descTex3D.CPUAccessFlags = NULL;
	}
	bool bIsSampleMax = false;
	////////////////////
	// Create Texture //
	ID3D11Texture3D* pdx11TX3DChunk = NULL;

	bool bIsDownSample = false;
	if(f3OffsetSize.x > 1.f ||
		f3OffsetSize.y > 1.f ||
		f3OffsetSize.z > 1.f)
		bIsDownSample = true;

	if (pGpuResDesc->usage_specifier == "TEXTURE3D_VOLUME_LOA_MASK")
	{
		if (g_pdx11Device->CreateTexture3D(&descTex3D, NULL, &pdx11TX3DChunk) != S_OK)
		{
			GMERRORMESSAGE("_GenerateGpuVObjectResourceVolume - Texture Creation Failure!");
			f3OffsetSize.x += 0.5f;
			f3OffsetSize.y += 0.5f;
			f3OffsetSize.z += 0.5f;
			goto RETRY;
			//return false;
		}
	}
	else
	{
		printf("GPU Uploaded Volume Size : %d KB (%dx%dx%d) %d bytes\n", descTex3D.Width * descTex3D.Height * descTex3D.Depth / 1024 * uiSizeDataType
			, descTex3D.Width, descTex3D.Height, descTex3D.Depth, uiSizeDataType);

		TT* ptChunk = new TT[descTex3D.Width * descTex3D.Height * descTex3D.Depth];
		for (int i = 0; i < (int)descTex3D.Depth; i++)
		{
			// Main Chunk
			if (pLocalProgress)
			{
				pLocalProgress->SetProgress(uiSliceCount, iVolumeSizeZ);
				uiSliceCount++;
			}

			float fPosZ = 0.5f;
			if (descTex3D.Depth > 1)
				fPosZ = (float)i / (float)(descTex3D.Depth - 1) * (float)(pVolumeData->vol_size.z - 1);

			for (int j = 0; j < (int)descTex3D.Height; j++)
			{
				float fPosY = 0.5f;
				if (descTex3D.Height > 1)
					fPosY = (float)j / (float)(descTex3D.Height - 1) * (float)(pVolumeData->vol_size.y - 1);

				for (int k = 0; k < (int)descTex3D.Width; k++)
				{
					TT tVoxelValue = 0;
					if (bIsDownSample)
					{
						float fPosX = 0.5f;
						if (descTex3D.Width > 1)
							fPosX = (float)k / (float)(descTex3D.Width - 1) * (float)(pVolumeData->vol_size.x - 1);

						vmint3 i3PosSampleVS = vmint3((int)fPosX, (int)fPosY, (int)fPosZ);

						int iMinMaxAddrX = min(max(i3PosSampleVS.x, 0), pVolumeData->vol_size.x - 1) + bnd_size.x;
						int iMinMaxAddrNextX = min(max(i3PosSampleVS.x + 1, 0), pVolumeData->vol_size.x - 1) + bnd_size.x;
						int iMinMaxAddrY = (min(max(i3PosSampleVS.y, 0), pVolumeData->vol_size.y - 1) + bnd_size.y)*iSizeSampleAddrX;
						int iMinMaxAddrNextY = (min(max(i3PosSampleVS.y + 1, 0), pVolumeData->vol_size.y - 1) + bnd_size.y)*iSizeSampleAddrX;

						int iSampleAddr0 = iMinMaxAddrX + iMinMaxAddrY;
						int iSampleAddr1 = iMinMaxAddrNextX + iMinMaxAddrY;
						int iSampleAddr2 = iMinMaxAddrX + iMinMaxAddrNextY;
						int iSampleAddr3 = iMinMaxAddrNextX + iMinMaxAddrNextY;
						int iSampleAddrZ0 = i3PosSampleVS.z + bnd_size.z;
						int iSampleAddrZ1 = i3PosSampleVS.z + bnd_size.z + 1;

						if (i3PosSampleVS.z < 0)
							iSampleAddrZ0 = iSampleAddrZ1;
						else if (i3PosSampleVS.z >= pVolumeData->vol_size.z - 1)
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

						if (!bIsMaxUnitSample)
						{
							vmfloat3 f3InterpolateRatio;
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

							for (int m = 0; m < 8; m++)
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
						int iAddrZ = i + bnd_size.z;
						int iAddrXY = bnd_size.x + k + (j + bnd_size.y)*iSizeSampleAddrX;
						tVoxelValue = (TT)((int)pptVolume[iAddrZ][iAddrXY] + iSampleValueOffset);
					}
					if (bIsPoreVolume && tVoxelValue > 254) tVoxelValue = 0;
					ptChunk[k + j * descTex3D.Width + i * descTex3D.Width*descTex3D.Height] = tVoxelValue;
				}
			}
		}

		D3D11_SUBRESOURCE_DATA subDataRes;
		subDataRes.pSysMem = (void*)ptChunk;
		subDataRes.SysMemPitch = sizeof(TT)*descTex3D.Width;
		subDataRes.SysMemSlicePitch = sizeof(TT)*descTex3D.Width*descTex3D.Height;

		if (g_pdx11Device->CreateTexture3D(&descTex3D, &subDataRes, &pdx11TX3DChunk) != S_OK)
		{
			VMSAFE_DELETEARRAY(ptChunk);
			GMERRORMESSAGE("_GenerateGpuVObjectResourceVolume - Texture Creation Failure!");
			f3OffsetSize.x += 0.5f;
			f3OffsetSize.y += 0.5f;
			f3OffsetSize.z += 0.5f;
			goto RETRY;
			//return false;
		}

		// 	D3D11_MAPPED_SUBRESOURCE mappedRes;
		// 	g_pdx11DeviceImmContext->Map(pdx11TX3DChunk, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes);
			// 	printf("\n\n\n%u\n\n\n\n", sizeof(TT));
			// 	::MessageBox(NULL, "GG3", NULL, MB_OK);
		// 	g_pdx11DeviceImmContext->Unmap(pdx11TX3DChunk, NULL);

		VMSAFE_DELETEARRAY(ptChunk);
	}

	// Register
	pGpuResource->Init();
	pGpuResource->resource_row_pitch_bytes = descTex3D.Width * uiSizeDataType;
	pGpuResource->resource_slice_pitch_bytes = descTex3D.Width * uiSizeDataType * descTex3D.Height;
	pGpuResource->logical_res_bytes = vmint3(descTex3D.Width * uiSizeDataType, descTex3D.Height, descTex3D.Depth);
	pGpuResource->sample_interval = f3OffsetSize;
	pGpuResource->vtrPtrs.push_back(pdx11TX3DChunk);
	pGpuResource->gpu_res_desc = *pGpuResDesc;
	pGpuResource->recent_used_time = vmhelpers::GetCurrentTimePack();

	if(pLocalProgress)
	{
		pLocalProgress->Deinit();
	}

	return true;
}

bool VmGenerateGpuResourceVOLUME(const GpuResDescriptor* pGpuResDesc, const VmVObjectVolume* pCObjectVolume, GpuResource* pGpuResource/*out*/, LocalProgress* pLocalProgress)
{
	// Resource //
	VolumeData* pVolumeData = ((VmVObjectVolume*)pCObjectVolume)->GetVolumeData();
	int iValueMin = (int)(pVolumeData->store_Mm_values.x);
	if (pGpuResDesc->usage_specifier.compare("TEXTURE2DARRAY_IMAGESTACK") == 0)
	{
		if (pVolumeData->store_dtype.type_name == data_type::dtype<byte>().type_name)
		{
			if (!_GenerateGpuResourceImageStack<byte, byte>(pGpuResDesc, pCObjectVolume, DXGI_FORMAT_R8_UNORM, -iValueMin, true, pGpuResource, pLocalProgress))
				return false;
		}
		else if (pVolumeData->store_dtype.type_name == data_type::dtype<char>().type_name)
		{
			if (!_GenerateGpuResourceImageStack<char, byte>(pGpuResDesc, pCObjectVolume, DXGI_FORMAT_R8_UNORM, -iValueMin, false, pGpuResource, pLocalProgress))
				return false;
		}
		else if (pVolumeData->store_dtype.type_name == data_type::dtype<ushort>().type_name)
		{
			if (!_GenerateGpuResourceImageStack<ushort, ushort>(pGpuResDesc, pCObjectVolume, DXGI_FORMAT_R16_UNORM, -iValueMin, true, pGpuResource, pLocalProgress))
				return false;
		}
		else if (pVolumeData->store_dtype.type_name == data_type::dtype<short>().type_name)
		{
			if (!_GenerateGpuResourceImageStack<short, ushort>(pGpuResDesc, pCObjectVolume, DXGI_FORMAT_R16_UNORM, -iValueMin, false, pGpuResource, pLocalProgress))
				return false;
		}
		else
		{
			GMERRORMESSAGE("DX11GenerateGpuResourceVOLUME - Not supported Data Type");
			return false;
		}
	}
	else
	{
		if (pVolumeData->store_dtype.type_name == data_type::dtype<byte>().type_name)
		{
			if (!_GenerateGpuResourceVolume<byte, byte>(pGpuResDesc, pCObjectVolume, DXGI_FORMAT_R8_UNORM, -iValueMin, pGpuResource, pLocalProgress))
				return false;
		}
		else if (pVolumeData->store_dtype.type_name == data_type::dtype<char>().type_name)
		{
			if (!_GenerateGpuResourceVolume<char, byte>(pGpuResDesc, pCObjectVolume, DXGI_FORMAT_R8_UNORM, -iValueMin, pGpuResource, pLocalProgress))
				return false;
		}
		else if (pVolumeData->store_dtype.type_name == data_type::dtype<ushort>().type_name)
		{
			if (!_GenerateGpuResourceVolume<ushort, ushort>(pGpuResDesc, pCObjectVolume, DXGI_FORMAT_R16_UNORM, -iValueMin, pGpuResource, pLocalProgress))
				return false;
		}
		else if (pVolumeData->store_dtype.type_name == data_type::dtype<short>().type_name)
		{
			if (!_GenerateGpuResourceVolume<short, ushort>(pGpuResDesc, pCObjectVolume, DXGI_FORMAT_R16_UNORM, -iValueMin, pGpuResource, pLocalProgress))
				return false;
		}
		else
		{
			GMERRORMESSAGE("DX11GenerateGpuResourceVOLUME - Not supported Data Type");
			return false;
		}
	}
	return true;
}

template <typename ST, typename TT> bool _GenerateGpuResourceBRICKS(const GpuResDescriptor* pGpuResDesc, const VmVObjectVolume* pCObjectVolume, 
	DXGI_FORMAT eVoxelFormat, int iSampleValueOffset, GpuResource* pGpuResource/*out*/, LocalProgress* pLocalProgress)
{
	::MessageBoxA(NULL, "deprecated", NULL, MB_OK);
	/*
	if(pGpuResDesc->usage_specifier.compare(("TEXTURE3D_VOLUME_BRICKCHUNK")) != 0)
		return false;

	const VolumeData* pVolData = ((VmVObjectVolume*)pCObjectVolume)->GetVolumeData();
	int iBlkLevel = pGpuResDesc->misc;	// 0 : High Resolution, 1 : Low Resolution
	if(iBlkLevel != 0 && iBlkLevel != 1)
		return false;

	int iSizeBrickBound = 2;
	VolumeBlocks* pstVolumeResBlock = ((VmVObjectVolume*)pCObjectVolume)->GetVolumeBlock(iBlkLevel); 
	pstVolumeResBlock->iBrickExtraSize = iSizeBrickBound;	// DEFAULT //

	double dRangeProgessOld = 0;
	if(pLocalProgress)
	{
		dRangeProgessOld = pLocalProgress->range;
		pLocalProgress->range = dRangeProgessOld*0.95;
	}

	uint uiSizeDataType = VXHGetDataTypeSizeByte(pVolData->eVolumeDataType);
	uint uiVolumeSizeKiloBytes = (uint)((ullong)pVolData->vol_size.x*(ullong)pVolData->vol_size.y*(ullong)pVolData->vol_size.z*uiSizeDataType/1024);

	byte* pyTaggedActivatedBlocks = pstVolumeResBlock->GetTaggedActivatedBlocks(pGpuResDesc->src_obj_id);
	////////////////////////////////////
	// Texture for Volume Description //		
	ST** pptVolumeSlices = (ST**)pVolData->vol_slices;
	int iVolumeAddrOffsetX = pVolData->vol_size.x + pVolData->bnd_size.x*2;
	vmint3 blk_bnd_size = pstVolumeResBlock->blk_bnd_size;
	vmint3 i3BlockNumSampleSize = pstVolumeResBlock->blk_vol_size + blk_bnd_size * 2;
	int iBlockNumSampleSizeXY = i3BlockNumSampleSize.x * i3BlockNumSampleSize.y;

	vmint3 unitblk_size = pstVolumeResBlock->unitblk_size;
	int iBrickExtraSize = pstVolumeResBlock->iBrickExtraSize;
	vmint3 i3BlockAddrSize;
	i3BlockAddrSize.x = unitblk_size.x + iBrickExtraSize*2;
	i3BlockAddrSize.y = unitblk_size.y + iBrickExtraSize*2;
	i3BlockAddrSize.z = unitblk_size.z + iBrickExtraSize*2;

	vector<int> vtrBrickIDs;
	int iCountID = 0;
	for (int iZ = 0; iZ < pstVolumeResBlock->blk_vol_size.z; iZ++)
	for (int iY = 0; iY < pstVolumeResBlock->blk_vol_size.y; iY++)
	for (int iX = 0; iX < pstVolumeResBlock->blk_vol_size.x; iX++)
	{
		int iSampleAddr = iX + blk_bnd_size.x
			+ (iY + blk_bnd_size.y) * i3BlockNumSampleSize.x + (iZ + blk_bnd_size.z) * iBlockNumSampleSizeXY;
		if (pyTaggedActivatedBlocks[iSampleAddr] != 0)
			vtrBrickIDs.push_back(iCountID);
		iCountID++;
	}
	
	// Given Parameters //
	size_t stSizeSingleBrick = i3BlockAddrSize.x * i3BlockAddrSize.y * i3BlockAddrSize.z * pVolData->store_dtype.type_bytes;
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
		GMERRORMESSAGE("IMPOSSIBLE CASE : NUMBER OF BRICK CHUNKS IS OVER!");
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
	
	if(pLocalProgress)
		pLocalProgress->Init();

	vector<void*> vtrTextures;
	int iNumBricksInChunk = (int)stNEW_NUM_BRICKS_IN_SINGLE_CHUNK;
	int iNumChunks = (int)stNEW_NUM_CHUNKS;
	int iNumTotalActualBricks = (int)vtrBrickIDs.size();
	vmint3 i3NumBricksInTex3D((int)stNUM_BRICK_X_IN_TEX2D, (int)stNUM_BRICK_Y_IN_TEX2D, (int)stNUM_BRICK_Z_IN_TEX2D);
	for(int iIndexChunk = 0; iIndexChunk < iNumChunks; iIndexChunk++)
	{
		memset(ptBrickChunk, 0, sizeof(TT) * descTex3D.Width * descTex3D.Height * descTex3D.Depth);
		for(int i = 0; i < iNumBricksInChunk; i++)
		{
			if(pLocalProgress)
				pLocalProgress->SetProgress(i + iNumBricksInChunk * iIndexChunk, iNumTotalActualBricks);

			int iBrickIndex = i + iNumBricksInChunk * iIndexChunk;
			if(iBrickIndex >= iNumTotalActualBricks)
				break;
			int iBrickID = vtrBrickIDs[iBrickIndex];

			int iBrickIdZ = iBrickID/(pstVolumeResBlock->blk_vol_size.x*pstVolumeResBlock->blk_vol_size.y);
			int iBrickIdXY = iBrickID%(pstVolumeResBlock->blk_vol_size.x*pstVolumeResBlock->blk_vol_size.y);
			int iBrickIdY = iBrickIdXY/pstVolumeResBlock->blk_vol_size.x;
			int iBrickIdX = iBrickIdXY%pstVolumeResBlock->blk_vol_size.x;

			for(int iAddrVoxZ = 0; iAddrVoxZ < i3BlockAddrSize.z; iAddrVoxZ++)	// Down Sampling 시도!
			{
				for(int iAddrVoxY = 0; iAddrVoxY < i3BlockAddrSize.y; iAddrVoxY++)
				{
					for(int iAddrVoxX = 0; iAddrVoxX < i3BlockAddrSize.x; iAddrVoxX++)
					{
						int iAddrWholeZ = max(min((iBrickIdZ*unitblk_size.z - iBrickExtraSize + pVolData->bnd_size.z) + iAddrVoxZ, 
							pVolData->vol_size.z + pVolData->bnd_size.z*2 - 1), 0);
						int iAddrWholeY = max(min((iBrickIdY*unitblk_size.y - iBrickExtraSize + pVolData->bnd_size.y) + iAddrVoxY,
							pVolData->vol_size.y + pVolData->bnd_size.y*2 - 1), 0);
						int iAddrWholeX = max(min((iBrickIdX*unitblk_size.x - iBrickExtraSize + pVolData->bnd_size.x) + iAddrVoxX,
							pVolData->vol_size.x + pVolData->bnd_size.x*2 - 1), 0);
						int iAddrWholeXY = iAddrWholeY*iVolumeAddrOffsetX + iAddrWholeX;

						vmint3 i3ChunkBrickOffset;
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
			VMSAFE_DELETEARRAY(ptBrickChunk);
			GMERRORMESSAGE("_GenerateGpuResourceBRICKS - Texture Creation Failure!");
			return false;
		}

		vtrTextures.push_back(pdx11TX3DChunk);
	}


	VMSAFE_DELETEARRAY(ptBrickChunk);
	
	pGpuResource->Init();
	pGpuResource->resource_row_pitch_bytes = descTex3D.Width * uiSizeDataType;
	pGpuResource->resource_slice_pitch_bytes = descTex3D.Width * uiSizeDataType * descTex3D.Height;
	pGpuResource->logical_res_bytes = vmint3(descTex3D.Width * uiSizeDataType, descTex3D.Height, descTex3D.Depth);
	pGpuResource->sample_interval = vmfloat3(1.f, 1.f, 1.f);
	pGpuResource->vtrPtrs = vtrTextures;
	pGpuResource->gpu_res_desc = *pGpuResDesc;
	pGpuResource->recent_used_time = vmhelpers::GetCurrentTimePack();
	
	if(pLocalProgress)
	{
		pLocalProgress->Deinit();
		pLocalProgress->range = dRangeProgessOld*0.05;
	}

	/**/
	return true;
}

bool _GenerateGpuResourceBLOCKSMAP(const GpuResDescriptor* pGpuResDesc, const VmVObjectVolume* pCObjectVolume, 
	GpuResource* pstGpuResource/*out*/, LocalProgress* pLocalProgress)
{
	if(pGpuResDesc->usage_specifier.compare(("TEXTURE3D_VOLUME_BLOCKMAP")) != 0
		|| pGpuResDesc->res_type != ResourceTypeDX11RESOURCE)
		return false;

	VolumeData* pVolumeData = ((VmVObjectVolume*)pCObjectVolume)->GetVolumeData();
	int iBlkLevel = (int)(pGpuResDesc->misc & 0x1);	// 0 : High Resolution, 1 : Low Resolution
	VolumeBlocks* pstVolumeResBlock = ((VmVObjectVolume*)pCObjectVolume)->GetVolumeBlock(iBlkLevel); 

	////////////////////////////////////
	// Texture for Volume Description //
	D3D11_TEXTURE3D_DESC descTex3DMap;
	ZeroMemory(&descTex3DMap, sizeof(D3D11_TEXTURE3D_DESC));
	descTex3DMap.Width = pstVolumeResBlock->blk_vol_size.x;
	descTex3DMap.Height = pstVolumeResBlock->blk_vol_size.y;
	descTex3DMap.Depth = pstVolumeResBlock->blk_vol_size.z;
	descTex3DMap.MipLevels = 1;
	descTex3DMap.MiscFlags = NULL;

	if (pGpuResDesc->dtype.type_name == data_type::dtype<ushort>().type_name)
		descTex3DMap.Format = DXGI_FORMAT_R16_UNORM; // to UINT...
	else if (pGpuResDesc->dtype.type_name == data_type::dtype<uint>().type_name)
		descTex3DMap.Format = DXGI_FORMAT_R32_UINT;
	else return false;
	
#define BLOCKMAPIMMUTABLE
#ifdef BLOCKMAPIMMUTABLE
	descTex3DMap.Usage = D3D11_USAGE_IMMUTABLE;
	descTex3DMap.CPUAccessFlags = NULL;
#else
	descTex3DMap.Usage = D3D11_USAGE_DYNAMIC;
	descTex3DMap.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
#endif
	descTex3DMap.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	if(pLocalProgress)
		*pLocalProgress->progress_ptr = (pLocalProgress->start);

	uint* piBlocksMap = NULL;
	ushort* pusBlocksMap = NULL;
	int related_tobj_id = (int)((pGpuResDesc->misc >> 16) % 65536) | (((int)ObjectTypeTMAP) << 24);
	if (!HDx11AllocAndUpdateBlockMap(&piBlocksMap, &pusBlocksMap, related_tobj_id, pstVolumeResBlock, descTex3DMap.Format, pLocalProgress))
		return false;

	// Create Texture //
	ID3D11Texture3D* pdx11TX3DChunk = NULL;
#ifdef BLOCKMAPIMMUTABLE
	D3D11_SUBRESOURCE_DATA subDataRes;
	if(descTex3DMap.Format == DXGI_FORMAT_R32_UINT)
	{
		subDataRes.pSysMem = piBlocksMap;
		subDataRes.SysMemPitch = sizeof(uint)*descTex3DMap.Width;
		subDataRes.SysMemSlicePitch = sizeof(uint)*descTex3DMap.Width*descTex3DMap.Height;
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

	if(descTex3DMap.Format == DXGI_FORMAT_R32_UINT)
	{
		uint* piBlkMap = (float*)d11MappedRes.pData;
		for(int i = 0; i < (int)descTex3DMap.Depth; i++)
			for(int j = 0; j < (int)descTex3DMap.Height; j++)
			{
				memcpy(&piBlkMap[j*d11MappedRes.RowPitch/4 + i*d11MappedRes.DepthPitch/4], &piBlocksMap[j*descTex3DMap.Width + i*descTex3DMap.Width*descTex3DMap.Height]
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

	if(descTex3DMap.Format == DXGI_FORMAT_R32_UINT)
	{
		VMSAFE_DELETEARRAY(piBlocksMap);
	}
	else
	{
		VMSAFE_DELETEARRAY(pusBlocksMap);
	}

	if(pLocalProgress)
	{
		*pLocalProgress->progress_ptr = (pLocalProgress->start + pLocalProgress->range);
	}

	if(hr != S_OK)
	{
		GMERRORMESSAGE("_GenerateGpuVObjectResourceBRICKSMAP - Texture Creation Failure!");
		return false;
	}

	// Register
	pstGpuResource->Init();
	pstGpuResource->resource_row_pitch_bytes = sizeof(ushort)*descTex3DMap.Width;
	pstGpuResource->resource_slice_pitch_bytes = sizeof(ushort)*descTex3DMap.Width*descTex3DMap.Height;
	pstGpuResource->logical_res_bytes = vmint3(pstGpuResource->resource_row_pitch_bytes, descTex3DMap.Height, descTex3DMap.Depth);
	pstGpuResource->vtrPtrs.push_back(pdx11TX3DChunk);
	pstGpuResource->gpu_res_desc = *pGpuResDesc;
	pstGpuResource->recent_used_time = vmhelpers::GetCurrentTimePack();

	return true;
}

bool _GenerateGpuResourceMinMaxBlock(const GpuResDescriptor* pGpuResDesc, const VmVObjectVolume* pCObjectVolume, 
	GpuResource* pGpuResource/*out*/, LocalProgress* pLocalProgress)
{
	if(pGpuResDesc->usage_specifier.compare(("TEXTURE3D_VOLUME_MINMAXBLOCK")) != 0
		|| pGpuResDesc->res_type != ResourceTypeDX11RESOURCE)
		return false;

	const VolumeData* pVolData = ((VmVObjectVolume*)pCObjectVolume)->GetVolumeData();
	int iBlkLevel = (int)(pGpuResDesc->misc & 0x1);	// 0 : High Resolution, 1 : Low Resolution
	const VolumeBlocks* psvxVolumeResBlock = ((VmVObjectVolume*)pCObjectVolume)->GetVolumeBlock(iBlkLevel); 

	////////////////////////////////////
	// Texture for Volume Description //
	D3D11_TEXTURE3D_DESC descTex3DMap;
	ZeroMemory(&descTex3DMap, sizeof(D3D11_TEXTURE3D_DESC));
	descTex3DMap.Width = psvxVolumeResBlock->blk_vol_size.x;
	descTex3DMap.Height = psvxVolumeResBlock->blk_vol_size.y;
	descTex3DMap.Depth = psvxVolumeResBlock->blk_vol_size.z;
	descTex3DMap.MipLevels = 1;
	descTex3DMap.MiscFlags = NULL;

	if(pGpuResDesc->dtype.type_name != data_type::dtype<ushort>().type_name)
		return false;

	descTex3DMap.Format = DXGI_FORMAT_R16_UNORM;
	descTex3DMap.Usage = D3D11_USAGE_IMMUTABLE;
	descTex3DMap.CPUAccessFlags = NULL;
	descTex3DMap.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	uint uiNumBlocks = (uint)psvxVolumeResBlock->blk_vol_size.x*(uint)psvxVolumeResBlock->blk_vol_size.y*(uint)psvxVolumeResBlock->blk_vol_size.z;

	if(pLocalProgress)
		*pLocalProgress->progress_ptr = (pLocalProgress->start);

	vmushort2* pus2BlockValues = (vmushort2*)psvxVolumeResBlock->mM_blks;
	vmint3 blk_bnd_size = psvxVolumeResBlock->blk_bnd_size;
	vmint3 i3BlockNumSampleSize = psvxVolumeResBlock->blk_vol_size + blk_bnd_size * 2;
	int iBlockNumSampleSizeXY = i3BlockNumSampleSize.x * i3BlockNumSampleSize.y;
	int iBlockNumXY = psvxVolumeResBlock->blk_vol_size.x * psvxVolumeResBlock->blk_vol_size.y;

	ushort* pusBlockMinValue = new ushort[uiNumBlocks];
	ushort* pusBlockMaxValue = new ushort[uiNumBlocks];
	uint uiCount = 0;
	for (int iZ = 0; iZ < psvxVolumeResBlock->blk_vol_size.z; iZ++)
	{
		if (pLocalProgress)
			*pLocalProgress->progress_ptr = (pLocalProgress->start + pLocalProgress->range*iZ / psvxVolumeResBlock->blk_vol_size.z);
		for (int iY = 0; iY < psvxVolumeResBlock->blk_vol_size.y; iY++)
		for (int iX = 0; iX < psvxVolumeResBlock->blk_vol_size.x; iX++)
		{
			vmushort2 us2BlockValue = pus2BlockValues[(iX + blk_bnd_size.x) 
				+ (iY + blk_bnd_size.y) * i3BlockNumSampleSize.x + (iZ + blk_bnd_size.z) * iBlockNumSampleSizeXY];

			int iAddrBlockInGpu = iX + iY * psvxVolumeResBlock->blk_vol_size.x + iZ * iBlockNumXY;
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
	VMSAFE_DELETEARRAY(pusBlockMinValue);

	ID3D11Texture3D* pdx11TX3DBlockMax = NULL;
	D3D11_SUBRESOURCE_DATA subDataResMax;
	subDataResMax.pSysMem = pusBlockMaxValue;
	subDataResMax.SysMemPitch = sizeof(ushort)*descTex3DMap.Width;
	subDataResMax.SysMemSlicePitch = sizeof(ushort)*descTex3DMap.Width*descTex3DMap.Height;
	hr = g_pdx11Device->CreateTexture3D(&descTex3DMap, &subDataResMax, &pdx11TX3DBlockMax);
	VMSAFE_DELETEARRAY(pusBlockMaxValue);

	if(pLocalProgress)
		*pLocalProgress->progress_ptr = (pLocalProgress->start + pLocalProgress->range);

	if(hr != S_OK)
	{
		GMERRORMESSAGE("_GenerateGpuVObjectResourceBRICKSMAP - Texture Creation Failure!");
		return false;
	}

	// Register
	pGpuResource->Init();
	pGpuResource->resource_row_pitch_bytes = sizeof(ushort)*descTex3DMap.Width;
	pGpuResource->resource_slice_pitch_bytes = sizeof(ushort)*descTex3DMap.Width*descTex3DMap.Height;
	pGpuResource->logical_res_bytes = vmint3(pGpuResource->resource_row_pitch_bytes, descTex3DMap.Height, descTex3DMap.Depth);
	pGpuResource->vtrPtrs.push_back(pdx11TX3DBlockMin);
	pGpuResource->vtrPtrs.push_back(pdx11TX3DBlockMax);
	pGpuResource->gpu_res_desc = *pGpuResDesc;
	pGpuResource->recent_used_time = vmhelpers::GetCurrentTimePack();

	return true;
}

bool VmGenerateGpuResourceBRICKS(const GpuResDescriptor* pGpuResDesc, const VmVObjectVolume* pCObjectVolume, GpuResource* pGpuResource/*out*/, LocalProgress* pLocalProgress)
{
	// Resource //
	VolumeData* pVolumeData = ((VmVObjectVolume*)pCObjectVolume)->GetVolumeData();
	int iValueMin = (int)(pVolumeData->store_Mm_values.x);

	if (pVolumeData->store_dtype.type_name == data_type::dtype<byte>().type_name)
	{
		if (!_GenerateGpuResourceBRICKS<byte, byte>(pGpuResDesc, pCObjectVolume, DXGI_FORMAT_R8_UNORM, -iValueMin, pGpuResource, pLocalProgress))
			return false;
	}
	else if (pVolumeData->store_dtype.type_name == data_type::dtype<char>().type_name)
	{
		if (!_GenerateGpuResourceBRICKS<char, byte>(pGpuResDesc, pCObjectVolume, DXGI_FORMAT_R8_UNORM, -iValueMin, pGpuResource, pLocalProgress))
			return false;
	}
	else if (pVolumeData->store_dtype.type_name == data_type::dtype<ushort>().type_name)
	{
		if (!_GenerateGpuResourceBRICKS<ushort, ushort>(pGpuResDesc, pCObjectVolume, DXGI_FORMAT_R16_UNORM, -iValueMin, pGpuResource, pLocalProgress))
			return false;
	}
	else if (pVolumeData->store_dtype.type_name == data_type::dtype<short>().type_name)
	{
		if (!_GenerateGpuResourceBRICKS<short, ushort>(pGpuResDesc, pCObjectVolume, DXGI_FORMAT_R16_UNORM, -iValueMin, pGpuResource, pLocalProgress))
			return false;
	}
	else
	{
		GMERRORMESSAGE("DX11GenerateGpuResourceBRICKS - Not supported Data Type");
		return false;
	}
	return true;
}

bool VmGenerateGpuResourceBLOCKSMAP(const GpuResDescriptor* pGpuResDesc, const VmVObjectVolume* pCObjectVolume, GpuResource* pGpuResource/*out*/, LocalProgress* pLocalProgress)
{
	if(!_GenerateGpuResourceBLOCKSMAP(pGpuResDesc, pCObjectVolume, pGpuResource, pLocalProgress))
		return false;
	return true;
}

bool VmGenerateGpuResourceBlockMinMax(const GpuResDescriptor* pGpuResDesc, const VmVObjectVolume* pCObjectVolume, GpuResource* pGpuResource/*out*/, LocalProgress* pLocalProgress)
{
	if(!_GenerateGpuResourceMinMaxBlock(pGpuResDesc, pCObjectVolume, pGpuResource, pLocalProgress))
		return false;
	return true;
}

bool VmGenerateGpuResourceFRAMEBUFFER(const GpuResDescriptor* pGpuResDesc, const VmIObject* pCIObject, GpuResource* pGpuResource/*out*/, LocalProgress* pLocalProgress)
{
	if(pGpuResDesc->res_type != ResourceTypeDX11RESOURCE)
		return false;
	
	vmint2 i2SizeScreen;
	vmint3 i3SizeScreenCurrent;
	((VmIObject*)pCIObject)->GetFrameBufferInfo(&i2SizeScreen);
	i3SizeScreenCurrent.x = i2SizeScreen.x;
	i3SizeScreenCurrent.y = i2SizeScreen.y;
	i3SizeScreenCurrent.z = 0;

	if (pGpuResDesc->usage_specifier.compare("TEXTURE2D_IOBJECT_DEPTHSTENCIL") == 0
		|| pGpuResDesc->usage_specifier.compare("TEXTURE2D_IOBJECT_RENDEROUT") == 0
		|| pGpuResDesc->usage_specifier.compare("TEXTURE2D_RW_SPINLOCK") == 0
		|| pGpuResDesc->usage_specifier.compare("TEXTURE2D_RW_COUNTER") == 0
		|| pGpuResDesc->usage_specifier.compare("TEXTURE2D_IOBJECT_ONLYRESOURCE") == 0
		|| pGpuResDesc->usage_specifier.compare("TEXTURE2D_IOBJECT_SYSTEM") == 0)
	{
		DXGI_FORMAT eDXGI_Format = DXGI_FORMAT_UNKNOWN;
		if (pGpuResDesc->dtype.type_name == data_type::dtype<byte>().type_name) eDXGI_Format = DXGI_FORMAT_R8_UNORM;
		else if (pGpuResDesc->dtype.type_name == data_type::dtype<vmbyte4>().type_name) eDXGI_Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		else if (pGpuResDesc->dtype.type_name == data_type::dtype<ushort>().type_name) eDXGI_Format = DXGI_FORMAT_R16_UNORM;
		else if (pGpuResDesc->dtype.type_name == data_type::dtype<uint>().type_name) eDXGI_Format = DXGI_FORMAT_R32_UINT;
		else if (pGpuResDesc->dtype.type_name == data_type::dtype<int>().type_name) eDXGI_Format = DXGI_FORMAT_R32_SINT;
		else if (pGpuResDesc->dtype.type_name == data_type::dtype<vmint4>().type_name) eDXGI_Format = DXGI_FORMAT_R32G32B32A32_SINT;
		else if (pGpuResDesc->dtype.type_name == data_type::dtype<short>().type_name) eDXGI_Format = DXGI_FORMAT_R16_SNORM;
		else if (pGpuResDesc->dtype.type_name == data_type::dtype<float>().type_name) eDXGI_Format = DXGI_FORMAT_R32_FLOAT;
		else if (pGpuResDesc->dtype.type_name == data_type::dtype<vmfloat2>().type_name) eDXGI_Format = DXGI_FORMAT_R32G32_FLOAT;
		else if (pGpuResDesc->dtype.type_name == data_type::dtype<vmfloat4>().type_name) eDXGI_Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		else GMERRORMESSAGE("NOT SUPPORTED DATAFORMAT in Dojo's DX11 Interface : CONTACT korfriend@gmail.com");

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

		if(pGpuResDesc->usage_specifier.compare("TEXTURE2D_IOBJECT_DEPTHSTENCIL") == 0)
		{
			if(pGpuResDesc->dtype.type_name != data_type::dtype<float>().type_name)
				GMERRORMESSAGE("FORCED to SET DXGI_FORMAT_D32_FLOAT!");
			descTX2D.Format = DXGI_FORMAT_D32_FLOAT;
			descTX2D.Usage = D3D11_USAGE_DEFAULT;
			descTX2D.BindFlags = D3D11_BIND_DEPTH_STENCIL;
			descTX2D.CPUAccessFlags = NULL;

			if(g_pdx11Device->CreateTexture2D(&descTX2D, NULL, &pdx11TX2D) != S_OK)
				return false;

			// Register
			pGpuResource->Init();
			pGpuResource->resource_row_pitch_bytes = descTX2D.Width * pGpuResDesc->dtype.type_bytes;
			pGpuResource->logical_res_bytes = vmint3(descTX2D.Width * pGpuResDesc->dtype.type_bytes, descTX2D.Height, 0);
			pGpuResource->vtrPtrs.push_back(pdx11TX2D);
			pGpuResource->gpu_res_desc = *pGpuResDesc;
			pGpuResource->recent_used_time = vmhelpers::GetCurrentTimePack();
		}
		else if(pGpuResDesc->usage_specifier.compare("TEXTURE2D_IOBJECT_RENDEROUT") == 0
			|| pGpuResDesc->usage_specifier.compare("TEXTURE2D_IOBJECT_ONLYRESOURCE") == 0
			|| pGpuResDesc->usage_specifier.compare("TEXTURE2D_IOBJECT_SYSTEM") == 0
			|| pGpuResDesc->usage_specifier.compare("TEXTURE2D_RW_COUNTER") == 0
			|| pGpuResDesc->usage_specifier.compare("TEXTURE2D_RW_SPINLOCK") == 0)
		{
			descTX2D.Format = eDXGI_Format;
			int iNumTextures = max(pGpuResDesc->misc & 0xFF, 1);
			int iFlagComputeShaderBit = (pGpuResDesc->misc >> 16) & 0xF;
			if (pGpuResDesc->usage_specifier.compare("TEXTURE2D_IOBJECT_RENDEROUT") == 0
				|| pGpuResDesc->usage_specifier.compare("TEXTURE2D_RW_COUNTER") == 0
				|| pGpuResDesc->usage_specifier.compare("TEXTURE2D_RW_SPINLOCK") == 0)
			{
				
				descTX2D.Usage = D3D11_USAGE_DEFAULT;
				if (iFlagComputeShaderBit == 2)
					descTX2D.BindFlags = pGpuResDesc->bind_flag;
				else if (iFlagComputeShaderBit == 1)
					descTX2D.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
				else
					descTX2D.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
				descTX2D.CPUAccessFlags = NULL;
			}
			else if(pGpuResDesc->usage_specifier.compare("TEXTURE2D_IOBJECT_SYSTEM") == 0)
			{
				descTX2D.Usage = D3D11_USAGE_STAGING;
				descTX2D.BindFlags = NULL;
				descTX2D.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;	// D3D11_CPU_ACCESS_WRITE 140818 에 추가 by Dojo
			}
			else if(pGpuResDesc->usage_specifier.compare("TEXTURE2D_IOBJECT_ONLYRESOURCE") == 0)
			{
				descTX2D.Usage = D3D11_USAGE_DEFAULT;
				descTX2D.BindFlags = D3D11_BIND_SHADER_RESOURCE;
				descTX2D.CPUAccessFlags = NULL;
			}

			// Register
			pGpuResource->Init();

			for(int i = 0; i < iNumTextures; i++)
			{
				if(g_pdx11Device->CreateTexture2D(&descTX2D, NULL, &pdx11TX2D) != S_OK)
					return false;
				pGpuResource->vtrPtrs.push_back(pdx11TX2D);
			}

			pGpuResource->resource_row_pitch_bytes = descTX2D.Width * pGpuResDesc->dtype.type_bytes;
			pGpuResource->logical_res_bytes = vmint3(descTX2D.Width * pGpuResDesc->dtype.type_bytes, descTX2D.Height, 0);
			pGpuResource->gpu_res_desc = *pGpuResDesc;
			pGpuResource->recent_used_time = vmhelpers::GetCurrentTimePack();
		}
	}
	else if(pGpuResDesc->usage_specifier.compare("BUFFER_IOBJECT_RESULTOUT") == 0
		|| pGpuResDesc->usage_specifier.compare("BUFFER_IOBJECT_SHADOWMAP") == 0
		|| pGpuResDesc->usage_specifier.compare("BUFFER_IOBJECT_SYSTEM") == 0
		)
	{
		if(pGpuResDesc->dtype.type_name != data_type::dtype<RWB_Output_Layers>().type_name)
			return false;

		ID3D11Buffer* pdx11BufUnordered = NULL;
		uint uiGridSizeX = 8;
		uint uiGridSizeY = 8;
		int iGridSizeFromMisc = pGpuResDesc->misc & 0xFFFF;
		int iNumBuffers = max((pGpuResDesc->misc & 0xFFFF0000) >> 16, 1);
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
		descBuf.ByteWidth =  pGpuResDesc->dtype.type_bytes * uiNumThreads;
		descBuf.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		descBuf.StructureByteStride = pGpuResDesc->dtype.type_bytes;

		if(pGpuResDesc->usage_specifier.compare("BUFFER_IOBJECT_SYSTEM") == 0)
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
		pGpuResource->Init();
		for(int i = 0; i < iNumBuffers; i++)
		{
			// Create Buffer and Register
			if(g_pdx11Device->CreateBuffer(&descBuf, NULL, &pdx11BufUnordered) != S_OK)
				return false;
			pGpuResource->vtrPtrs.push_back(pdx11BufUnordered);
		}
		pGpuResource->resource_row_pitch_bytes = descBuf.ByteWidth;
		pGpuResource->logical_res_bytes = vmint3(descBuf.ByteWidth, 0, 0);
		pGpuResource->gpu_res_desc = *pGpuResDesc;
		pGpuResource->recent_used_time = vmhelpers::GetCurrentTimePack();
	}
	else if (pGpuResDesc->usage_specifier == "BUFFER_RW_DEEP_VIS"
			|| pGpuResDesc->usage_specifier == "BUFFER_RW_DEEP_ZDEPTH"
			|| pGpuResDesc->usage_specifier == "BUFFER_RW_DEEP_ZTHICK")
	{
		ID3D11Buffer* pdx11BufUnordered = NULL;
		int iNumBuffers = pGpuResDesc->misc;
		uint uiNumElements = i2SizeScreen.x * i2SizeScreen.y * iNumBuffers;

		D3D11_BUFFER_DESC descBuf;
		ZeroMemory(&descBuf, sizeof(D3D11_BUFFER_DESC));
		descBuf.ByteWidth =  pGpuResDesc->dtype.type_bytes * uiNumElements;
		descBuf.MiscFlags = NULL; // D3D11_RESOURCE_MISC_BUFFER_STRUCTURED
		descBuf.StructureByteStride = pGpuResDesc->dtype.type_bytes;
		descBuf.BindFlags = pGpuResDesc->bind_flag;
		descBuf.Usage = D3D11_USAGE_DEFAULT;
		descBuf.CPUAccessFlags = NULL;

		// Register
		pGpuResource->Init();
		for(int i = 0; i < 1; i++)
		{
			// Create Buffer and Register
			if(g_pdx11Device->CreateBuffer(&descBuf, NULL, &pdx11BufUnordered) != S_OK)
				return false;
			pGpuResource->vtrPtrs.push_back(pdx11BufUnordered);
		}
		pGpuResource->resource_row_pitch_bytes = descBuf.ByteWidth;
		pGpuResource->logical_res_bytes = vmint3(descBuf.ByteWidth, 0, 0);
		pGpuResource->gpu_res_desc = *pGpuResDesc;
		pGpuResource->recent_used_time = vmhelpers::GetCurrentTimePack();
	}

	return true;
}

bool VmGenerateGpuResourceOTF(const GpuResDescriptor* pGpuResDesc, const VmTObject* pCTObject, GpuResource* pGpuResource/*out*/, LocalProgress* pLocalProgress)
{
	if((pGpuResDesc->usage_specifier.compare(("TEXTURE2D_TOBJECT_2D")) != 0
		&& pGpuResDesc->usage_specifier.compare(("BUFFER_TOBJECT_OTFSERIES")) != 0)
		|| pGpuResDesc->res_type != ResourceTypeDX11RESOURCE)
		return false;

	TMapData* pstTfArchive = ((VmTObject*)pCTObject)->GetTMapData();

	if (pGpuResDesc->usage_specifier.compare(("BUFFER_TOBJECT_OTFSERIES")) == 0)
	{
		if (pstTfArchive->num_dim < 1)
			return false;

		int iNumOTFSeries = 0;
		pCTObject->GetCustomParameter("_int_NumOTFSeries", data_type::dtype<int>(), &iNumOTFSeries);
		iNumOTFSeries = max(iNumOTFSeries, 1);

		D3D11_BUFFER_DESC descBuf;
		ZeroMemory(&descBuf, sizeof(D3D11_BUFFER_DESC));
		descBuf.ByteWidth = pstTfArchive->array_lengths.x * iNumOTFSeries * pGpuResDesc->dtype.type_bytes;
		descBuf.MiscFlags = NULL;// D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		descBuf.StructureByteStride = pGpuResDesc->dtype.type_bytes;
		descBuf.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		descBuf.Usage = D3D11_USAGE_DYNAMIC;// D3D11_USAGE_DEFAULT;
		descBuf.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;// NULL;

		if (pGpuResDesc->misc == 1)	// Index Series //
			descBuf.ByteWidth = 128 * pGpuResDesc->dtype.type_bytes;	// 

		ID3D11Buffer* pdx11BufOTF = NULL;
		if (g_pdx11Device->CreateBuffer(&descBuf, NULL, &pdx11BufOTF) != S_OK)
		{
			GMERRORMESSAGE("Called SimpleVR Module's VmGenerateGpuResourceOTF - BUFFER_TOBJECT_OTFSERIES FAILURE!");
			VMSAFE_RELEASE(pdx11BufOTF);
			return false;
		}

		// Register
		pGpuResource->Init();
		pGpuResource->resource_row_pitch_bytes = descBuf.ByteWidth;
		pGpuResource->logical_res_bytes = vmint3(descBuf.ByteWidth, 0, 0);
		pGpuResource->vtrPtrs.push_back(pdx11BufOTF);
		pGpuResource->gpu_res_desc = *pGpuResDesc;
		pGpuResource->recent_used_time = vmhelpers::GetCurrentTimePack();
	}
	else if(pGpuResDesc->usage_specifier.compare(("TEXTURE2D_TOBJECT_2D")) == 0)
	{
		if(pstTfArchive->num_dim < 2 || (pstTfArchive->array_lengths.y <= 0))
			return false;

		if(pstTfArchive->array_lengths.x > 2048 || pstTfArchive->array_lengths.y > 2048)
		{
			GMERRORMESSAGE("Too Large 2D OTF more binning needed!");
			return false;
		}
		vmint2 i2OtfTextureSize;
		i2OtfTextureSize.x = min(pstTfArchive->array_lengths.x, 2048);
		i2OtfTextureSize.y = min(pstTfArchive->array_lengths.y, 2048);

		D3D11_TEXTURE2D_DESC descTex2DOTF;
		ZeroMemory(&descTex2DOTF, sizeof(D3D11_TEXTURE2D_DESC));
		descTex2DOTF.Width = i2OtfTextureSize.x;
		descTex2DOTF.Height = i2OtfTextureSize.y;
		descTex2DOTF.MipLevels = 1;
		descTex2DOTF.ArraySize = 1;
		descTex2DOTF.Format = DXGI_FORMAT_R8G8B8A8_UINT;
		descTex2DOTF.SampleDesc.Count = 1;			
		descTex2DOTF.SampleDesc.Quality = 0;		
		descTex2DOTF.Usage = D3D11_USAGE_DYNAMIC;
		descTex2DOTF.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		descTex2DOTF.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		descTex2DOTF.MiscFlags = NULL;

		ID3D11Texture2D* pdx11TX2DOtf = NULL;
		g_pdx11Device->CreateTexture2D(&descTex2DOTF, NULL, &pdx11TX2DOtf);
				
		// Register
		pGpuResource->Init();
		pGpuResource->resource_row_pitch_bytes = descTex2DOTF.Width * sizeof(vmfloat4);
		pGpuResource->logical_res_bytes = vmint3(descTex2DOTF.Width * sizeof(vmfloat4), descTex2DOTF.Height, 0);
		pGpuResource->vtrPtrs.push_back(pdx11TX2DOtf);
		pGpuResource->gpu_res_desc = *pGpuResDesc;
		pGpuResource->recent_used_time = vmhelpers::GetCurrentTimePack();
	}
	else
	{
		return false;
	}

	return true;
}

bool VmGenerateGpuResBinderAsView(const GpuResource* pGpuResource, EvmGpuResourceType res_type, GpuResource* pGpuResourceOut/*out*/)
{
	if(pGpuResource->gpu_res_desc.res_type != ResourceTypeDX11RESOURCE
		|| res_type == ResourceTypeDX11RESOURCE)
		return false;

	vector<void*> vtrViews;

	if (pGpuResource->gpu_res_desc.usage_specifier.compare("TEXTURE3D_VOLUME_DEFAULT") == 0
		|| pGpuResource->gpu_res_desc.usage_specifier.compare("TEXTURE3D_VOLUME_DEFAULT_MASK") == 0
		|| pGpuResource->gpu_res_desc.usage_specifier.compare("TEXTURE3D_VOLUME_LOA_MASK") == 0
		|| pGpuResource->gpu_res_desc.usage_specifier.compare("TEXTURE3D_VOLUME_DOWNSAMPLE") == 0
		|| pGpuResource->gpu_res_desc.usage_specifier.compare("TEXTURE3D_VOLUME_BLOCKMAP") == 0
		|| pGpuResource->gpu_res_desc.usage_specifier.compare("TEXTURE3D_VOLUME_MINMAXBLOCK") == 0)
	{
		if(res_type != ResourceTypeDX11SRV)
		{
			GMERRORMESSAGE("3D TEXTURE VOLUME RESOURCE SUPPORTS ONLY SRV! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
			return false;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC descSRVVolData;
		if(pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<byte>().type_name)
			descSRVVolData.Format = DXGI_FORMAT_R8_UNORM;
		else if(pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<ushort>().type_name)
			descSRVVolData.Format = DXGI_FORMAT_R16_UNORM;
		else if(pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<float>().type_name)
			descSRVVolData.Format = DXGI_FORMAT_R32_FLOAT;
		else
		{
			GMERRORMESSAGE("NOT SUPPORTED DATAFORMAT! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
			return false;
		}
		descSRVVolData.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
		descSRVVolData.Texture3D.MipLevels = 1;
		descSRVVolData.Texture3D.MostDetailedMip = 0;

		for(int i = 0; i < (int)pGpuResource->vtrPtrs.size(); i++)
		{
			ID3D11ShaderResourceView* pdx11View;
			if(g_pdx11Device->CreateShaderResourceView((ID3D11Resource*)pGpuResource->vtrPtrs.at(i), 
				&descSRVVolData, (ID3D11ShaderResourceView**)&pdx11View) != S_OK)
			{
				GMERRORMESSAGE("Volume Resource View Creation Failure! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
				return false;
			}
			vtrViews.push_back(pdx11View);
		}
	}
	else if(pGpuResource->gpu_res_desc.usage_specifier.compare(("TEXTURE3D_VOLUME_BRICKCHUNK")) == 0)
	{
		if(res_type != ResourceTypeDX11SRV)
		{
			GMERRORMESSAGE("3D TEXTURE VOLUME RESOURCE SUPPORTS ONLY SRV! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
			return false;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC descSRVVolData;
		if(pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<byte>().type_name)
			descSRVVolData.Format = DXGI_FORMAT_R8_UNORM;
		else if(pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<ushort>().type_name)
			descSRVVolData.Format = DXGI_FORMAT_R16_UNORM;
		else
		{
			GMERRORMESSAGE("NOT SUPPORTED DATAFORMAT! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
			return false;
		}
		descSRVVolData.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
		descSRVVolData.Texture3D.MipLevels = 1;
		descSRVVolData.Texture3D.MostDetailedMip = 0;
		for(int i = 0; i < pGpuResource->vtrPtrs.size(); i++)
		{
			ID3D11Resource* pdx11Res = (ID3D11Resource*)pGpuResource->vtrPtrs.at(i);
			ID3D11View* pdx11View = NULL;
			if(g_pdx11Device->CreateShaderResourceView(pdx11Res, &descSRVVolData, (ID3D11ShaderResourceView**)&pdx11View) != S_OK)
			{
				GMERRORMESSAGE("Volume Resource View Creation Failure! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
				return false;
			}
			vtrViews.push_back(pdx11View);
		}
	}
	else if (pGpuResource->gpu_res_desc.usage_specifier.compare(("TEXTURE2DARRAY_IMAGESTACK")) == 0)
	{
		if (res_type != ResourceTypeDX11SRV)
		{
			GMERRORMESSAGE("2D TEXTUREARRAY VOLUME RESOURCE SUPPORTS ONLY SRV! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
			return false;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC descSRVVolData;
		if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<byte>().type_name)
			descSRVVolData.Format = DXGI_FORMAT_R8_UNORM;
		else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<ushort>().type_name)
			descSRVVolData.Format = DXGI_FORMAT_R16_UNORM;
		else
		{
			GMERRORMESSAGE("NOT SUPPORTED DATAFORMAT! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
			return false;
		}
		descSRVVolData.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		descSRVVolData.Texture2DArray.ArraySize = pGpuResource->logical_res_bytes.z;
		descSRVVolData.Texture2DArray.FirstArraySlice = 0;
		descSRVVolData.Texture2DArray.MipLevels = 1;
		descSRVVolData.Texture2DArray.MostDetailedMip = 0;
		for (int i = 0; i < pGpuResource->vtrPtrs.size(); i++)
		{
			ID3D11Resource* pdx11Res = (ID3D11Resource*)pGpuResource->vtrPtrs.at(i);
			ID3D11View* pdx11View = NULL;
			if (g_pdx11Device->CreateShaderResourceView(pdx11Res, &descSRVVolData, (ID3D11ShaderResourceView**)&pdx11View) != S_OK)
			{
				GMERRORMESSAGE("Image Stack Resource View Creation Failure! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
				return false;
			}
			vtrViews.push_back(pdx11View);
		}
	}
	else if(pGpuResource->gpu_res_desc.usage_specifier.compare(("TEXTURE2D_IOBJECT_DEPTHSTENCIL")) == 0)
	{
		if(res_type != ResourceTypeDX11DSV)
		{
			GMERRORMESSAGE("DEPTHSTENCIL 2D TEXTURE SUPPORTS ONLY DSV! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
			return false;
		}

		if(pGpuResource->gpu_res_desc.dtype.type_name != data_type::dtype<float>().type_name)
		{
			GMERRORMESSAGE("NOT SUPPORTED DATAFORMAT! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
			return false;
		}

		D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
		ZeroMemory(&descDSV, sizeof(D3D11_DEPTH_STENCIL_VIEW_DESC));
		descDSV.Format = DXGI_FORMAT_D32_FLOAT;
		descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		descDSV.Flags = 0;
		descDSV.Texture2D.MipSlice = 0;

		for(int i = 0; i < (int)pGpuResource->vtrPtrs.size(); i++)
		{
			ID3D11View* pdx11View = NULL;
			if(g_pdx11Device->CreateDepthStencilView((ID3D11Resource*)pGpuResource->vtrPtrs.at(i), 
				&descDSV, (ID3D11DepthStencilView**)&pdx11View) != S_OK)
			{
				GMERRORMESSAGE("Depth Stencil View Creation Failure! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
				return false;
			}
			vtrViews.push_back(pdx11View);
		}
	}
	else if (pGpuResource->gpu_res_desc.usage_specifier.compare("TEXTURE2D_IOBJECT_DEPTHSTENCIL") == 0
		|| pGpuResource->gpu_res_desc.usage_specifier.compare("TEXTURE2D_IOBJECT_RENDEROUT") == 0
		|| pGpuResource->gpu_res_desc.usage_specifier.compare("TEXTURE2D_RW_SPINLOCK") == 0
		|| pGpuResource->gpu_res_desc.usage_specifier.compare("TEXTURE2D_RW_COUNTER") == 0
		|| pGpuResource->gpu_res_desc.usage_specifier.compare("TEXTURE2D_IOBJECT_SYSTEM") == 0
		|| pGpuResource->gpu_res_desc.usage_specifier.compare("TEXTURE2D_TOBJECT_2D") == 0)
	{
		if(res_type == ResourceTypeDX11DSV)
		{
			GMERRORMESSAGE("NORMAL 2D TEXTURE DO NOT SUPPORT DSV! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
			return false;
		}
		if((pGpuResource->gpu_res_desc.usage_specifier.compare("TEXTURE2D_TOBJECT_2D") == 0)
			&& res_type == ResourceTypeDX11RTV)
		{
			GMERRORMESSAGE("OTF 2D TEXTURE DO NOT SUPPORT RTV! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
			return false;
		}
		if(pGpuResource->gpu_res_desc.usage_specifier.compare("TEXTURE2D_IOBJECT_SYSTEM") == 0)
		{
			GMERRORMESSAGE("OTF 2D TEXTURE SYSTEM DO NOT SUPPORT VIEW! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
			return false;
		}

		DXGI_FORMAT eDXGI_Format = DXGI_FORMAT_UNKNOWN;
		if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<byte>().type_name) eDXGI_Format = DXGI_FORMAT_R8_UNORM;
		else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<vmbyte4>().type_name) eDXGI_Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<ushort>().type_name) eDXGI_Format = DXGI_FORMAT_R16_UNORM;
		else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<uint>().type_name) eDXGI_Format = DXGI_FORMAT_R32_UINT;
		else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<int>().type_name) eDXGI_Format = DXGI_FORMAT_R32_SINT;
		else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<vmint4>().type_name) eDXGI_Format = DXGI_FORMAT_R32G32B32A32_SINT;
		else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<short>().type_name) eDXGI_Format = DXGI_FORMAT_R16_SNORM;
		else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<float>().type_name) eDXGI_Format = DXGI_FORMAT_R32_FLOAT;
		else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<vmfloat2>().type_name) eDXGI_Format = DXGI_FORMAT_R32G32_FLOAT;
		else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<vmfloat4>().type_name) eDXGI_Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		else {
			GMERRORMESSAGE("NOT SUPPORTED DATAFORMAT in Dojo's DX11 Interface : CONTACT korfriend@gmail.com");
			return false;
		}

		if(res_type == ResourceTypeDX11SRV)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
			ZeroMemory(&descSRV, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
			descSRV.Format = eDXGI_Format;
			descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			descSRV.Texture2D.MostDetailedMip = 0;
			descSRV.Texture2D.MipLevels = 1;

			for(int i = 0; i < (int)pGpuResource->vtrPtrs.size(); i++)
			{
				ID3D11View* pdx11View = NULL;
				if(g_pdx11Device->CreateShaderResourceView((ID3D11Resource*)pGpuResource->vtrPtrs.at(i), 
					&descSRV, (ID3D11ShaderResourceView**)&pdx11View) != S_OK)
				{
					GMERRORMESSAGE("Shader Resource View Creation Failure! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
					return false;
				}
				vtrViews.push_back(pdx11View);
			}
		}
		else if (res_type == ResourceTypeDX11RTV)
		{
			D3D11_RENDER_TARGET_VIEW_DESC descRTV;
			ZeroMemory(&descRTV, sizeof(D3D11_RENDER_TARGET_VIEW_DESC));
			descRTV.Format = eDXGI_Format;
			descRTV.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			descRTV.Texture2D.MipSlice = 0;

			for (int i = 0; i < (int)pGpuResource->vtrPtrs.size(); i++)
			{
				ID3D11View* pdx11View = NULL;
				if (g_pdx11Device->CreateRenderTargetView((ID3D11Resource*)pGpuResource->vtrPtrs.at(i),
					&descRTV, (ID3D11RenderTargetView**)&pdx11View) != S_OK)
				{
					GMERRORMESSAGE("Render Target View Creation Failure! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
					return false;
				}
				vtrViews.push_back(pdx11View);
			}
		}
		else if (res_type == ResourceTypeDX11UAV)
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC descUAV;
			ZeroMemory(&descUAV, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
			descUAV.Format = eDXGI_Format;
			descUAV.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
			descUAV.Texture2D.MipSlice = 0;

			for (int i = 0; i < (int)pGpuResource->vtrPtrs.size(); i++)
			{
				ID3D11View* pdx11View = NULL;
				if (g_pdx11Device->CreateUnorderedAccessView((ID3D11Resource*)pGpuResource->vtrPtrs.at(i),
					&descUAV, (ID3D11UnorderedAccessView**)&pdx11View) != S_OK)
				{
					GMERRORMESSAGE("Unordered Access View Creation Failure! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
					return false;
				}
				vtrViews.push_back(pdx11View);
			}
		}
		else
		{
			GMERRORMESSAGE("View Creation Failure! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
			return false;
		}
	}
	else if(pGpuResource->gpu_res_desc.usage_specifier.compare(0, 14, ("BUFFER_IOBJECT")) == 0
		|| pGpuResource->gpu_res_desc.usage_specifier.compare(0, 24, ("BUFFER_TOBJECT_OTFSERIES")) == 0)
	{
		if(res_type == ResourceTypeDX11DSV
			|| res_type == ResourceTypeDX11RTV)
		{
			GMERRORMESSAGE("UNORDERED ACCESS BUFFER DO NOT SUPPORT DSV and RTV! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
			return false;
		}
		if(pGpuResource->gpu_res_desc.usage_specifier.compare(("BUFFER_IOBJECT_SYSTEM")) == 0)
		{
			GMERRORMESSAGE("UNORDERED ACCESS BUFFER SYSTEM DO NOT SUPPORT VIEW! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
			return false;
		}

		if(res_type == ResourceTypeDX11UAV)
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC descUAV;
			ZeroMemory(&descUAV, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
			descUAV.Format = DXGI_FORMAT_UNKNOWN;	// Structured RW Buffer
			descUAV.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			descUAV.Buffer.FirstElement = 0;
			descUAV.Buffer.NumElements = pGpuResource->logical_res_bytes.x / pGpuResource->gpu_res_desc.dtype.type_bytes;
			descUAV.Buffer.Flags = 0;

			for(int i = 0; i < (int)pGpuResource->vtrPtrs.size(); i++)
			{
				ID3D11View* pdx11View = NULL;
				if(g_pdx11Device->CreateUnorderedAccessView((ID3D11Resource*)pGpuResource->vtrPtrs.at(i), 
					&descUAV, (ID3D11UnorderedAccessView**)&pdx11View) != S_OK)
				{
					GMERRORMESSAGE("Unordered Access View Creation Failure! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
					return false;
				}
				vtrViews.push_back(pdx11View);
			}
		}
		else if(res_type == ResourceTypeDX11SRV)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
			ZeroMemory(&descSRV, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));

			DXGI_FORMAT eDXGI_Format = DXGI_FORMAT_UNKNOWN;
			if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<byte>().type_name) eDXGI_Format = DXGI_FORMAT_R8_UNORM;
			else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<vmbyte4>().type_name) eDXGI_Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<ushort>().type_name) eDXGI_Format = DXGI_FORMAT_R16_UNORM;
			else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<vmint4>().type_name) eDXGI_Format = DXGI_FORMAT_R32G32B32A32_SINT;
			else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<short>().type_name) eDXGI_Format = DXGI_FORMAT_R16_SNORM;
			else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<uint>().type_name) eDXGI_Format = DXGI_FORMAT_R32_UINT;
			else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<int>().type_name) eDXGI_Format = DXGI_FORMAT_R32_SINT;
			else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<float>().type_name) eDXGI_Format = DXGI_FORMAT_R32_FLOAT;
			else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<vmfloat2>().type_name) eDXGI_Format = DXGI_FORMAT_R32G32_FLOAT;
			else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<vmfloat4>().type_name) eDXGI_Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<RWB_Output_Layers>().type_name) eDXGI_Format = DXGI_FORMAT_UNKNOWN;
			else {
				GMERRORMESSAGE("NOT SUPPORTED DATAFORMAT in Dojo's DX11 Interface : CONTACT korfriend@gmail.com");
				return false;
			}

			descSRV.Format = eDXGI_Format;
			descSRV.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
			descSRV.BufferEx.FirstElement = 0;
			descSRV.BufferEx.NumElements = pGpuResource->logical_res_bytes.x / pGpuResource->gpu_res_desc.dtype.type_bytes;

			for(int i = 0; i < (int)pGpuResource->vtrPtrs.size(); i++)
			{
				ID3D11View* pdx11View = NULL;
				if(g_pdx11Device->CreateShaderResourceView((ID3D11Resource*)pGpuResource->vtrPtrs.at(i), 
					&descSRV, (ID3D11ShaderResourceView**)&pdx11View) != S_OK)
				{
					GMERRORMESSAGE("Shader Reousrce View Creation Failure! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
					return false;
				}
				vtrViews.push_back(pdx11View);
			}
		}
	}
	else if (pGpuResource->gpu_res_desc.usage_specifier == "BUFFER_RW_DEEP_VIS"
		|| pGpuResource->gpu_res_desc.usage_specifier == "BUFFER_RW_DEEP_ZDEPTH"
		|| pGpuResource->gpu_res_desc.usage_specifier == "BUFFER_RW_DEEP_ZTHICK")
	{
		DXGI_FORMAT eDXGI_Format = DXGI_FORMAT_UNKNOWN;
		if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<byte>().type_name) eDXGI_Format = DXGI_FORMAT_R8_UNORM;
		else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<vmbyte4>().type_name) eDXGI_Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<ushort>().type_name) eDXGI_Format = DXGI_FORMAT_R16_UNORM;
		else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<vmint4>().type_name) eDXGI_Format = DXGI_FORMAT_R32G32B32A32_SINT;
		else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<short>().type_name) eDXGI_Format = DXGI_FORMAT_R16_SNORM;
		else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<uint>().type_name) eDXGI_Format = DXGI_FORMAT_R32_UINT;
		else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<int>().type_name) eDXGI_Format = DXGI_FORMAT_R32_SINT;
		else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<float>().type_name) eDXGI_Format = DXGI_FORMAT_R32_FLOAT;
		else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<vmfloat2>().type_name) eDXGI_Format = DXGI_FORMAT_R32G32_FLOAT;
		else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<vmfloat4>().type_name) eDXGI_Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		else {
			GMERRORMESSAGE("NOT SUPPORTED DATAFORMAT in Dojo's DX11 Interface : CONTACT korfriend@gmail.com");
			return false;
		}

		if (res_type == ResourceTypeDX11SRV)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
			ZeroMemory(&descSRV, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
			descSRV.Format = eDXGI_Format;
			descSRV.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
			descSRV.BufferEx.FirstElement = 0;
			descSRV.BufferEx.NumElements = pGpuResource->logical_res_bytes.x / pGpuResource->gpu_res_desc.dtype.type_bytes;// uiNumThreads;
			
			for (int i = 0; i < (int)pGpuResource->vtrPtrs.size(); i++)
			{
				ID3D11View* pdx11View = NULL; // ID3D11UnorderedAccessView
				if (g_pdx11Device->CreateShaderResourceView((ID3D11Resource*)pGpuResource->vtrPtrs[i], &descSRV, (ID3D11ShaderResourceView**)&pdx11View) != S_OK)
				{
					GMERRORMESSAGE("Shader Resource View Creation Failure! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
					return false;
				}
				vtrViews.push_back(pdx11View);
			}
		}
		else if (res_type == ResourceTypeDX11UAV)
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC descUAV;
			ZeroMemory(&descUAV, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
			descUAV.Format = eDXGI_Format;
			descUAV.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			descUAV.Buffer.FirstElement = 0;
			descUAV.Buffer.NumElements = pGpuResource->logical_res_bytes.x / pGpuResource->gpu_res_desc.dtype.type_bytes;// uiNumThreads;
			descUAV.Buffer.Flags = 0; // D3D11_BUFFER_RW_FLAG_RAW (DXGI_FORMAT_R32_TYPELESS), "D3D11_BUFFER_RW_FLAG_COUNTER"

			for (int i = 0; i < (int)pGpuResource->vtrPtrs.size(); i++)
			{
				ID3D11View* pdx11View = NULL;
				if (g_pdx11Device->CreateUnorderedAccessView((ID3D11Resource*)pGpuResource->vtrPtrs[i], &descUAV, (ID3D11UnorderedAccessView**)&pdx11View) != S_OK)
				{
					GMERRORMESSAGE("Unordered Access View Creation Failure! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
					return false;
				}
				vtrViews.push_back(pdx11View);
			}
		}
		else
		{
			GMERRORMESSAGE("View Creation Failure! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
			return false;
		}
	}
	else if(pGpuResource->gpu_res_desc.usage_specifier.compare("TEXTURE2D_CMM_TEXT") == 0)
	{
		if(res_type == ResourceTypeDX11SRV)
		{
			DXGI_FORMAT eDXGI_Format = DXGI_FORMAT_UNKNOWN;
			if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<byte>().type_name) eDXGI_Format = DXGI_FORMAT_R8_UNORM;
			else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<vmbyte4>().type_name) eDXGI_Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<ushort>().type_name) eDXGI_Format = DXGI_FORMAT_R16_UNORM;
			else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<short>().type_name) eDXGI_Format = DXGI_FORMAT_R16_SNORM;
			else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<uint>().type_name) eDXGI_Format = DXGI_FORMAT_R32_UINT;
			else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<int>().type_name) eDXGI_Format = DXGI_FORMAT_R32_SINT;
			else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<float>().type_name) eDXGI_Format = DXGI_FORMAT_R32_FLOAT;
			else if (pGpuResource->gpu_res_desc.dtype.type_name == data_type::dtype<vmfloat4>().type_name) eDXGI_Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			else {
				GMERRORMESSAGE("NOT SUPPORTED DATAFORMAT in Dojo's DX11 Interface : CONTACT korfriend@gmail.com");
				return false;
			}

			D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
			ZeroMemory(&descSRV, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
			descSRV.Format = eDXGI_Format;
			descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			descSRV.Texture2D.MostDetailedMip = 0;
			descSRV.Texture2D.MipLevels = 1;

			for(int i = 0; i < (int)pGpuResource->vtrPtrs.size(); i++)
			{
				ID3D11View* pdx11View = NULL;
				if(g_pdx11Device->CreateShaderResourceView((ID3D11Resource*)pGpuResource->vtrPtrs.at(i), 
					&descSRV, (ID3D11ShaderResourceView**)&pdx11View) != S_OK)
				{
					GMERRORMESSAGE("Shader Resource View Creation Failure! in Dojo's DX11 Manager : CONTACT korfriend@gmail.com");
					return false;
				}
				vtrViews.push_back(pdx11View);
			}
		}
	}

	if(vtrViews.size() == 0)
		return false;

	// Register
	pGpuResourceOut->Init();
	pGpuResourceOut->resource_row_pitch_bytes = pGpuResource->resource_row_pitch_bytes;
	pGpuResourceOut->resource_slice_pitch_bytes = pGpuResource->resource_slice_pitch_bytes;
	pGpuResourceOut->logical_res_bytes = pGpuResource->logical_res_bytes;
	pGpuResourceOut->sample_interval = pGpuResource->sample_interval;
	pGpuResourceOut->gpu_res_desc = pGpuResource->gpu_res_desc;
	pGpuResourceOut->gpu_res_desc.res_type = res_type;
	pGpuResourceOut->recent_used_time = vmhelpers::GetCurrentTimePack();
	pGpuResourceOut->vtrPtrs = vtrViews;

	return true;
}

