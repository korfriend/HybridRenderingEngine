#include "BVHUpdate.h"
#include "vzm2/Backlog.h"
#include "hlsl/ShaderInterop_BVH.h"
#include "gpulib.h"

namespace bvh {

	bool UpdateGeometryGPUBVH(VmGpuManager* gpuManager, grd_helper::GpuDX11CommonParameters* dx11CommonParams, VmVObjectPrimitive* pobj)
	{
		PrimitiveData* primitive = pobj->GetPrimitiveData();
		assert(primitive);

		if (primitive->num_prims == 0 || primitive->ptype != EvmPrimitiveType::PrimitiveTypeTRIANGLE)
		{
			vzlog_error("target primitive is not valid (no triangles)!!");
			return false;
		}

		GpuRes gres_vtx, gres_idx;
		// gpu resource check
		{

			gres_vtx.vm_src_id = pobj->GetObjectID();
			gres_vtx.res_name = string("PRIMITIVE_MODEL_VTX");

			gres_idx.vm_src_id = pobj->GetObjectID();
			gres_idx.res_name = string("PRIMITIVE_MODEL_IDX");

			if (!gpuManager->UpdateGpuResource(gres_vtx) || !gpuManager->UpdateGpuResource(gres_idx))
			{
				vzlog_error("target primitive MUST be registered in GPU memory!!");
				return false;
			}
		}

		uint totalTriangles = primitive->num_prims;

		GpuRes gres_primitiveCounterBuffer;
		//GpuRes gres_primitiveCounterBuffer_system;
		GpuRes gres_primitiveCounterBuffer_write;
		gres_primitiveCounterBuffer.vm_src_id = pobj->GetObjectID();
		gres_primitiveCounterBuffer.res_name = string("GPUBVH::primitiveCounterBuffer");
		if (totalTriangles > 0 && !gpuManager->UpdateGpuResource(gres_primitiveCounterBuffer))
		{
			uint stride_bytes = sizeof(uint);
			gres_primitiveCounterBuffer.rtype = RTYPE_BUFFER;
			gres_primitiveCounterBuffer.options["USAGE"] = D3D11_USAGE_DEFAULT;
			gres_primitiveCounterBuffer.options["CPU_ACCESS_FLAG"] = 0u;
			gres_primitiveCounterBuffer.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE;
			gres_primitiveCounterBuffer.options["FORMAT"] = DXGI_FORMAT_R32_TYPELESS;
			gres_primitiveCounterBuffer.options["RAW_ACCESS"] = 1;
			gres_primitiveCounterBuffer.res_values.SetParam("NUM_ELEMENTS", (uint)1);
			gres_primitiveCounterBuffer.res_values.SetParam("STRIDE_BYTES", (uint)stride_bytes);

			gpuManager->GenerateGpuResource(gres_primitiveCounterBuffer);


			//gres_primitiveCounterBuffer_system = gres_primitiveCounterBuffer;
			//gres_primitiveCounterBuffer_system.res_name = string("GPUBVH::primitiveCounterBuffer_SYSTEM");
			//gres_primitiveCounterBuffer_system.options["USAGE"] = D3D11_USAGE_STAGING;
			//gres_primitiveCounterBuffer_system.options["CPU_ACCESS_FLAG"] = D3D11_CPU_ACCESS_READ;
			//gres_primitiveCounterBuffer_system.options["BIND_FLAG"] = 0u;
			//gres_primitiveCounterBuffer_system.options["RAW_ACCESS"] = 0;
			//
			//gpuManager->GenerateGpuResource(gres_primitiveCounterBuffer_system);

			gres_primitiveCounterBuffer_write = gres_primitiveCounterBuffer;
			gres_primitiveCounterBuffer_write.res_name = string("GPUBVH::primitiveCounterBuffer_DYNAMIC");
			gres_primitiveCounterBuffer_write.options["USAGE"] = D3D11_USAGE_STAGING;// D3D11_USAGE_DYNAMIC;
			gres_primitiveCounterBuffer_write.options["CPU_ACCESS_FLAG"] = D3D11_CPU_ACCESS_WRITE;
			gres_primitiveCounterBuffer_write.options["BIND_FLAG"] = 0u;
			gres_primitiveCounterBuffer_write.options["RAW_ACCESS"] = 0;

			gpuManager->GenerateGpuResource(gres_primitiveCounterBuffer_write);
		}
		else
		{
			gpuManager->ReleaseGpuResource(gres_primitiveCounterBuffer, false);
			//gpuManager->ReleaseGpuResource(gres_primitiveCounterBuffer_system, false);
			gpuManager->ReleaseGpuResource(gres_primitiveCounterBuffer_write, false);
			return false;
		}

		GpuRes gres_bvhNodeBuffer;
		gres_bvhNodeBuffer.vm_src_id = pobj->GetObjectID();
		gres_bvhNodeBuffer.res_name = string("GPUBVH::BVHNodeBuffer");
		GpuRes gres_bvhParentBuffer;
		gres_bvhParentBuffer.vm_src_id = pobj->GetObjectID();
		gres_bvhParentBuffer.res_name = string("GPUBVH::BVHParentBuffer");
		GpuRes gres_bvhFlagBuffer;
		gres_bvhFlagBuffer.vm_src_id = pobj->GetObjectID();
		gres_bvhFlagBuffer.res_name = string("GPUBVH::BVHFlagBuffer");
		GpuRes gres_primitiveIDBuffer;
		gres_primitiveIDBuffer.vm_src_id = pobj->GetObjectID();
		gres_primitiveIDBuffer.res_name = string("GPUBVH::primitiveIDBuffer");
		GpuRes gres_primitiveBuffer;
		gres_primitiveBuffer.vm_src_id = pobj->GetObjectID();
		gres_primitiveBuffer.res_name = string("GPUBVH::primitiveBuffer");
		GpuRes gres_primitiveMortonBuffer;
		gres_primitiveMortonBuffer.vm_src_id = pobj->GetObjectID();
		gres_primitiveMortonBuffer.res_name = string("GPUBVH::primitiveMortonBuffer");

		uint primitive_capacity = pobj->GetObjParam("primitiveCapacity", 0u);
		if (totalTriangles > primitive_capacity)
		{
			primitive_capacity = std::max(2u, totalTriangles);
			pobj->SetObjParam("primitiveCapacity", primitive_capacity);

			gres_bvhNodeBuffer.rtype = RTYPE_BUFFER;
			gres_bvhNodeBuffer.options["USAGE"] = D3D11_USAGE_DEFAULT;
			gres_bvhNodeBuffer.options["CPU_ACCESS_FLAG"] = 0u;
			gres_bvhNodeBuffer.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			gres_bvhNodeBuffer.options["FORMAT"] = DXGI_FORMAT_UNKNOWN;
			gres_bvhNodeBuffer.options["MISC"] = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
			gres_bvhNodeBuffer.res_values.SetParam("NUM_ELEMENTS", (uint)primitive_capacity * 2);
			gres_bvhNodeBuffer.res_values.SetParam("STRIDE_BYTES", (uint)sizeof(BVHNode));
			gpuManager->GenerateGpuResource(gres_bvhNodeBuffer);

			gres_bvhParentBuffer.rtype = RTYPE_BUFFER;
			gres_bvhParentBuffer.options["USAGE"] = D3D11_USAGE_DEFAULT;
			gres_bvhParentBuffer.options["CPU_ACCESS_FLAG"] = 0u;
			gres_bvhParentBuffer.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			gres_bvhParentBuffer.options["FORMAT"] = DXGI_FORMAT_UNKNOWN;
			gres_bvhParentBuffer.options["MISC"] = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
			gres_bvhParentBuffer.res_values.SetParam("NUM_ELEMENTS", (uint)primitive_capacity * 2);
			gres_bvhParentBuffer.res_values.SetParam("STRIDE_BYTES", (uint)sizeof(uint));
			gpuManager->GenerateGpuResource(gres_bvhParentBuffer);

			gres_bvhFlagBuffer.rtype = RTYPE_BUFFER;
			gres_bvhFlagBuffer.options["USAGE"] = D3D11_USAGE_DEFAULT;
			gres_bvhFlagBuffer.options["CPU_ACCESS_FLAG"] = 0u;
			gres_bvhFlagBuffer.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			gres_bvhFlagBuffer.options["FORMAT"] = DXGI_FORMAT_UNKNOWN;
			gres_bvhFlagBuffer.options["MISC"] = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
			gres_bvhFlagBuffer.res_values.SetParam("NUM_ELEMENTS", (uint)(((primitive_capacity - 1) + 31) / 32)); // bitfield for internal nodes
			gres_bvhFlagBuffer.res_values.SetParam("STRIDE_BYTES", (uint)sizeof(uint));
			gpuManager->GenerateGpuResource(gres_bvhFlagBuffer);

			gres_primitiveIDBuffer.rtype = RTYPE_BUFFER;
			gres_primitiveIDBuffer.options["USAGE"] = D3D11_USAGE_DEFAULT;
			gres_primitiveIDBuffer.options["CPU_ACCESS_FLAG"] = 0u;
			gres_primitiveIDBuffer.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			gres_primitiveIDBuffer.options["FORMAT"] = DXGI_FORMAT_UNKNOWN;
			gres_primitiveIDBuffer.options["MISC"] = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
			gres_primitiveIDBuffer.res_values.SetParam("NUM_ELEMENTS", (uint)primitive_capacity); 
			gres_primitiveIDBuffer.res_values.SetParam("STRIDE_BYTES", (uint)sizeof(uint));
			gpuManager->GenerateGpuResource(gres_primitiveIDBuffer);

			gres_primitiveBuffer.rtype = RTYPE_BUFFER;
			gres_primitiveBuffer.options["USAGE"] = D3D11_USAGE_DEFAULT;
			gres_primitiveBuffer.options["CPU_ACCESS_FLAG"] = 0u;
			gres_primitiveBuffer.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			gres_primitiveBuffer.options["FORMAT"] = DXGI_FORMAT_UNKNOWN;
			gres_primitiveBuffer.options["MISC"] = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
			gres_primitiveBuffer.res_values.SetParam("NUM_ELEMENTS", (uint)primitive_capacity);
			gres_primitiveBuffer.res_values.SetParam("STRIDE_BYTES", (uint)sizeof(BVHPrimitive));
			gpuManager->GenerateGpuResource(gres_primitiveBuffer);

			gres_primitiveMortonBuffer.rtype = RTYPE_BUFFER;
			gres_primitiveMortonBuffer.options["USAGE"] = D3D11_USAGE_DEFAULT;
			gres_primitiveMortonBuffer.options["CPU_ACCESS_FLAG"] = 0u;
			gres_primitiveMortonBuffer.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			gres_primitiveMortonBuffer.options["FORMAT"] = DXGI_FORMAT_UNKNOWN;
			gres_primitiveMortonBuffer.options["MISC"] = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
			gres_primitiveMortonBuffer.res_values.SetParam("NUM_ELEMENTS", (uint)primitive_capacity);
			gres_primitiveMortonBuffer.res_values.SetParam("STRIDE_BYTES", (uint)sizeof(float));
			gpuManager->GenerateGpuResource(gres_primitiveMortonBuffer);
		}
		else
		{
			vzlog_assert(gpuManager->UpdateGpuResource(gres_bvhNodeBuffer), "gres_bvhNodeBuffer");
			vzlog_assert(gpuManager->UpdateGpuResource(gres_bvhParentBuffer), "gres_bvhParentBuffer");
			vzlog_assert(gpuManager->UpdateGpuResource(gres_bvhFlagBuffer), "gres_bvhFlagBuffer");
			vzlog_assert(gpuManager->UpdateGpuResource(gres_primitiveIDBuffer), "gres_primitiveIDBuffer");
			vzlog_assert(gpuManager->UpdateGpuResource(gres_primitiveBuffer), "gres_primitiveBuffer");
			vzlog_assert(gpuManager->UpdateGpuResource(gres_primitiveMortonBuffer), "gres_primitiveMortonBuffer");
		}

		__ID3D11DeviceContext* dx11DeviceImmContext = dx11CommonParams->dx11DeviceImmContext;

		// ----- build ----- 
		dx11CommonParams->GpuProfile("BVH Rebuild");

		uint32_t primitiveCount = 0;

		//ID3D11Buffer* cbuf_BVHPushConstants = dx11CommonParams->get_cbuf("BVHPushConstants");
		//D3D11_MAPPED_SUBRESOURCE mapped_pushConstants;

		ID3D11UnorderedAccessView* dx11UAVs_NULL[10] = { };
		dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 10, dx11UAVs_NULL, NULL);
		ID3D11ShaderResourceView* dx11SRVs_NULL[10] = { };
		dx11DeviceImmContext->CSSetShaderResources(0, 10, dx11SRVs_NULL);

		// BVH - Primitive (GEOMETRY-ONLY) Builder //
		{
			ID3D11UnorderedAccessView* uavs[3] = {
					  (ID3D11UnorderedAccessView*)gres_primitiveIDBuffer.alloc_res_ptrs[DTYPE_UAV]
					, (ID3D11UnorderedAccessView*)gres_primitiveBuffer.alloc_res_ptrs[DTYPE_UAV]
					, (ID3D11UnorderedAccessView*)gres_primitiveMortonBuffer.alloc_res_ptrs[DTYPE_UAV]
			};
			dx11DeviceImmContext->CSSetUnorderedAccessViews(0, arraysize(uavs), uavs, nullptr);

			ID3D11ShaderResourceView* srvs[2] = {
				  (ID3D11ShaderResourceView*)gres_vtx.alloc_res_ptrs[DTYPE_SRV]
				, (ID3D11ShaderResourceView*)gres_idx.alloc_res_ptrs[DTYPE_SRV]
			};
			dx11DeviceImmContext->CSSetShaderResources(0, arraysize(srvs), srvs);

			BVHPushConstants push;
			push.primitiveCount = totalTriangles;
			push.vertexStride = primitive->GetNumVertexDefinitions() * 3;

			geometrics::AABB aabb(XMFLOAT3(primitive->aabb_os.pos_min.x, primitive->aabb_os.pos_min.x, primitive->aabb_os.pos_min.x), 
				XMFLOAT3(primitive->aabb_os.pos_max.x, primitive->aabb_os.pos_max.x, primitive->aabb_os.pos_max.x));
			push.aabb_min = aabb.getMin();
			push.aabb_extents_rcp = aabb.getWidth();
			push.aabb_extents_rcp.x = 1.f / push.aabb_extents_rcp.x;
			push.aabb_extents_rcp.y = 1.f / push.aabb_extents_rcp.y;
			push.aabb_extents_rcp.z = 1.f / push.aabb_extents_rcp.z;

			grd_helper::PushConstants(&push, sizeof(BVHPushConstants), 0);
			//dx11DeviceImmContext->Map(cbuf_BVHPushConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_pushConstants);
			//BVHPushConstants* cbData = (BVHPushConstants*)mapped_pushConstants.pData;
			//memcpy(cbData, &push, sizeof(BVHPushConstants));
			//dx11DeviceImmContext->Unmap(cbuf_BVHPushConstants, 0);
			const ID3D11ShaderResourceView* srv_push = grd_helper::GetPushContantSRV();
			dx11DeviceImmContext->CSSetShaderResources(100, 1, (ID3D11ShaderResourceView* const*)&srv_push);

			primitiveCount += push.primitiveCount;

			dx11DeviceImmContext->CSSetShader(GETCS(BVH_Primitives_cs_5_0), NULL, 0);
			dx11DeviceImmContext->Dispatch((push.primitiveCount + BVH_BUILDER_GROUPSIZE - 1) / BVH_BUILDER_GROUPSIZE,
				1,
				1);
			dx11DeviceImmContext->Flush();

			dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 10, dx11UAVs_NULL, NULL);
			dx11DeviceImmContext->CSSetShaderResources(0, 10, dx11SRVs_NULL);
		}

		// update primitiveCounterBuffer
		{
			D3D11_MAPPED_SUBRESOURCE d11MappedRes;
			dx11DeviceImmContext->Map((ID3D11Resource*)gres_primitiveCounterBuffer_write.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_WRITE, 0, &d11MappedRes);
			*(uint*)d11MappedRes.pData = primitiveCount;
			dx11DeviceImmContext->Unmap((ID3D11Resource*)gres_primitiveCounterBuffer_write.alloc_res_ptrs[DTYPE_RES], 0);

			dx11DeviceImmContext->CopyResource((ID3D11Buffer*)gres_primitiveCounterBuffer.alloc_res_ptrs[DTYPE_RES], 
				(ID3D11Buffer*)gres_primitiveCounterBuffer_write.alloc_res_ptrs[DTYPE_RES]);
		}

		// BVH - Sort Primitive Mortons
		{
			gpulib::sort::Sort(gpuManager, dx11CommonParams,
				primitiveCount, gres_primitiveMortonBuffer, gres_primitiveCounterBuffer, 0, gres_primitiveIDBuffer);

			dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 10, dx11UAVs_NULL, NULL);
			dx11DeviceImmContext->CSSetShaderResources(0, 10, dx11SRVs_NULL);

		}

		// BVH - Build Hierarchy
		{
			ID3D11UnorderedAccessView* uavs[3] = {
				  (ID3D11UnorderedAccessView*)gres_bvhNodeBuffer.alloc_res_ptrs[DTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_bvhParentBuffer.alloc_res_ptrs[DTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_bvhFlagBuffer.alloc_res_ptrs[DTYPE_UAV]
			};
			dx11DeviceImmContext->CSSetUnorderedAccessViews(0, arraysize(uavs), uavs, nullptr);

			ID3D11ShaderResourceView* srvs[3] = {
				  (ID3D11ShaderResourceView*)gres_primitiveCounterBuffer.alloc_res_ptrs[DTYPE_SRV]
				, (ID3D11ShaderResourceView*)gres_primitiveIDBuffer.alloc_res_ptrs[DTYPE_SRV]
				, (ID3D11ShaderResourceView*)gres_primitiveMortonBuffer.alloc_res_ptrs[DTYPE_SRV]
			};
			dx11DeviceImmContext->CSSetShaderResources(0, arraysize(srvs), srvs);


			dx11DeviceImmContext->CSSetShader(GETCS(BVH_Hierarchy_cs_5_0), NULL, 0); 
			dx11DeviceImmContext->Dispatch((primitiveCount + BVH_BUILDER_GROUPSIZE - 1) / BVH_BUILDER_GROUPSIZE,
				1,
				1);
			dx11DeviceImmContext->Flush();

			dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 10, dx11UAVs_NULL, NULL);
			dx11DeviceImmContext->CSSetShaderResources(0, 10, dx11SRVs_NULL);
		}

		// BVH - Propagate AABB
		{
			ID3D11UnorderedAccessView* uavs[2] = {
				  (ID3D11UnorderedAccessView*)gres_bvhNodeBuffer.alloc_res_ptrs[DTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_bvhFlagBuffer.alloc_res_ptrs[DTYPE_UAV]
			};
			dx11DeviceImmContext->CSSetUnorderedAccessViews(0, arraysize(uavs), uavs, nullptr);

			ID3D11ShaderResourceView* srvs[4] = {
				  (ID3D11ShaderResourceView*)gres_primitiveCounterBuffer.alloc_res_ptrs[DTYPE_SRV]
				, (ID3D11ShaderResourceView*)gres_primitiveIDBuffer.alloc_res_ptrs[DTYPE_SRV]
				, (ID3D11ShaderResourceView*)gres_primitiveBuffer.alloc_res_ptrs[DTYPE_SRV]
				, (ID3D11ShaderResourceView*)gres_bvhParentBuffer.alloc_res_ptrs[DTYPE_SRV]
			};
			dx11DeviceImmContext->CSSetShaderResources(0, arraysize(srvs), srvs);

			dx11DeviceImmContext->CSSetShader(GETCS(BVH_Propagateaabb_cs_5_0), NULL, 0);
			dx11DeviceImmContext->Dispatch((primitiveCount + BVH_BUILDER_GROUPSIZE - 1) / BVH_BUILDER_GROUPSIZE,
				1,
				1);
			dx11DeviceImmContext->Flush();

			dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 10, dx11UAVs_NULL, NULL);
			dx11DeviceImmContext->CSSetShaderResources(0, 10, dx11SRVs_NULL);
		}
		dx11DeviceImmContext->CSSetShaderResources(100, 1, dx11SRVs_NULL);

		dx11CommonParams->GpuProfile("BVH Rebuild", true);

		return true;
	}
}