#include "gpulib.h"

using namespace gpulib;
using namespace grd_helper;


void sort::Sort(VmGpuManager* gpu_manager,
	grd_helper::GpuDX11CommonParameters* dx11CommonParams,
	uint32_t maxCount,
	GpuRes& comparisonBuffer_read,
	GpuRes& counterBuffer_read,
	uint32_t counterReadOffset,
	GpuRes& indexBuffer_write)
{
	GpuRes indirectBuffer;
	indirectBuffer.vm_src_id = 1; 
	indirectBuffer.res_name = string("GPU_SORT_INDRIECT_BUFFER");
	if (!gpu_manager->UpdateGpuResource(indirectBuffer)) {

		indirectBuffer.rtype = RTYPE_BUFFER;
		indirectBuffer.options["USAGE"] = D3D11_USAGE_DEFAULT;
		indirectBuffer.options["CPU_ACCESS_FLAG"] = 0;
		indirectBuffer.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		indirectBuffer.options["FORMAT"] = DXGI_FORMAT_R32_TYPELESS;
		indirectBuffer.options["RAW_ACCESS"] = 1;
		indirectBuffer.options["MISC"] = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
		indirectBuffer.res_values.SetParam("NUM_ELEMENTS", (uint)sizeof(IndirectDispatchArgs) / 4u);
		indirectBuffer.res_values.SetParam("STRIDE_BYTES", 4u);
		gpu_manager->GenerateGpuResource(indirectBuffer);
	}

	__ID3D11DeviceContext* dx11DeviceImmContext = dx11CommonParams->dx11DeviceImmContext;

	ID3D11Buffer* cbuf_sort = dx11CommonParams->get_cbuf("CB_SortConstants");
	auto SetSortContants = [&dx11DeviceImmContext, &cbuf_sort](const uint counterReadOffset, const vmint3& job_params) {
		D3D11_MAPPED_SUBRESOURCE mappedResSortConstants;
		dx11DeviceImmContext->Map(cbuf_sort, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResSortConstants);
		CB_SortConstants* cbData = (CB_SortConstants*)mappedResSortConstants.pData;
		cbData->counterReadOffset = counterReadOffset;
		cbData->job_params = job_params;
		dx11DeviceImmContext->Unmap(cbuf_sort, 0);
		dx11DeviceImmContext->CSSetConstantBuffers(13, 1, &cbuf_sort);
	};
	// initialize sorting arguments:
	{
		dx11DeviceImmContext->CSSetShader(GETCS(SORT_Kickoff_cs_5_0), NULL, 0);

		//ByteAddressBuffer counterBuffer : register(t0);
		//StructuredBuffer<float> comparisonBuffer : register(t1);
		//
		//RWByteAddressBuffer indirectBuffers : register(u0);
		//RWStructuredBuffer<uint> indexBuffer : register(u1);

		dx11DeviceImmContext->CSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&comparisonBuffer_read.alloc_res_ptrs[DTYPE_SRV]);
		dx11DeviceImmContext->CSSetUnorderedAccessViews(1, 1, (ID3D11UnorderedAccessView**)&indirectBuffer.alloc_res_ptrs[DTYPE_UAV], NULL);

		SetSortContants(counterReadOffset, vmint3());

		dx11DeviceImmContext->Dispatch(1, 1, 1);
		dx11DeviceImmContext->Flush();
	}


	dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, (ID3D11UnorderedAccessView**)&indexBuffer_write.alloc_res_ptrs[DTYPE_UAV], NULL);

	ID3D11ShaderResourceView* srvs[] = {
		(ID3D11ShaderResourceView*)counterBuffer_read.alloc_res_ptrs[DTYPE_SRV],
		(ID3D11ShaderResourceView*)comparisonBuffer_read.alloc_res_ptrs[DTYPE_SRV],
	};
	dx11DeviceImmContext->CSSetShaderResources(0, 2, srvs);

	// initial sorting:
	bool bDone = true;
	{
		// calculate how many threads we'll require:
		//   we'll sort 512 elements per CU (threadgroupsize 256)
		//     maybe need to optimize this or make it changeable during init
		//     TGS=256 is a good intermediate value

		unsigned int numThreadGroups = ((maxCount - 1) >> 9) + 1;

		//assert(numThreadGroups <= 1024);

		if (numThreadGroups > 1)
		{
			bDone = false;
		}

		// sort all buffers of size 512 (and presort bigger ones)
		dx11DeviceImmContext->CSSetShader(GETCS(SORT_cs_5_0), NULL, 0);

		SetSortContants(counterReadOffset, vmint3());

		dx11DeviceImmContext->DispatchIndirect((ID3D11Buffer*)indirectBuffer.alloc_res_ptrs[DTYPE_RES], 0);
		dx11DeviceImmContext->Flush();
	}

	int presorted = 512;
	while (!bDone)
	{
		// Incremental sorting:

		bDone = true;
		dx11DeviceImmContext->CSSetShader(GETCS(SORT_Step_cs_5_0), NULL, 0);

		// prepare thread group description data
		uint32_t numThreadGroups = 0;

		if (maxCount > (uint32_t)presorted)
		{
			if (maxCount > (uint32_t)presorted * 2)
				bDone = false;

			uint32_t pow2 = presorted;
			while (pow2 < maxCount)
				pow2 *= 2;
			numThreadGroups = pow2 >> 9;
		}

		uint32_t nMergeSize = presorted * 2;
		for (uint32_t nMergeSubSize = nMergeSize >> 1; nMergeSubSize > 256; nMergeSubSize = nMergeSubSize >> 1)
		{
			vmint3 job_params;
			job_params.x = nMergeSubSize;
			if (nMergeSubSize == nMergeSize >> 1)
			{
				job_params.y = (2 * nMergeSubSize - 1);
				job_params.z = -1;
			}
			else
			{
				job_params.y = nMergeSubSize;
				job_params.z = 1;
			}

			SetSortContants(counterReadOffset, job_params);
			dx11DeviceImmContext->Dispatch(numThreadGroups, 1, 1);
			dx11DeviceImmContext->Flush();
		}

		dx11DeviceImmContext->CSSetShader(GETCS(SORT_Inner_cs_5_0), NULL, 0);
		SetSortContants(counterReadOffset, vmint3());
		dx11DeviceImmContext->Dispatch(numThreadGroups, 1, 1);
		dx11DeviceImmContext->Flush();

		presorted *= 2;
	}
}