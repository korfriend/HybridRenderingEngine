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
		GpuRes gres_primitiveCounterBuffer_write;
		gres_primitiveCounterBuffer.vm_src_id = pobj->GetObjectID();
		gres_primitiveCounterBuffer.res_name = string("GPUBVH::primitiveCounterBuffer");
		gres_primitiveCounterBuffer_write.vm_src_id = pobj->GetObjectID();
		gres_primitiveCounterBuffer_write.res_name = string("GPUBVH::primitiveCounterBuffer_WRITE");
		if (totalTriangles > 0)
		{
			if (!gpuManager->UpdateGpuResource(gres_primitiveCounterBuffer))
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
				gres_primitiveCounterBuffer_write.res_name = string("GPUBVH::primitiveCounterBuffer_WRITE");
				gres_primitiveCounterBuffer_write.options["USAGE"] = D3D11_USAGE_STAGING;// D3D11_USAGE_DYNAMIC;
				gres_primitiveCounterBuffer_write.options["CPU_ACCESS_FLAG"] = D3D11_CPU_ACCESS_WRITE;
				gres_primitiveCounterBuffer_write.options["BIND_FLAG"] = 0u;
				gres_primitiveCounterBuffer_write.options["RAW_ACCESS"] = 0;

				gpuManager->GenerateGpuResource(gres_primitiveCounterBuffer_write);
			}
			else
			{
				assert(gpuManager->UpdateGpuResource(gres_primitiveCounterBuffer_write));
			}
		}
		else
		{
			gpuManager->ReleaseGpuResource(gres_primitiveCounterBuffer, false);
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

//#define BVH_GEN_DEBUG
#ifdef BVH_GEN_DEBUG
			auto generateDubugBuffer = [&gpuManager](GpuRes& gres)
				{
					GpuRes gres_debug = gres;
					gres_debug.res_name += "_DEBUG";
					gres_debug.options["USAGE"] = D3D11_USAGE_STAGING;
					gres_debug.options["CPU_ACCESS_FLAG"] = D3D11_CPU_ACCESS_READ;
					gres_debug.options["BIND_FLAG"] = 0u;
					gpuManager->GenerateGpuResource(gres_debug);
				};
			generateDubugBuffer(gres_bvhNodeBuffer);
			generateDubugBuffer(gres_bvhParentBuffer);
			generateDubugBuffer(gres_bvhFlagBuffer);
			generateDubugBuffer(gres_primitiveIDBuffer);
			generateDubugBuffer(gres_primitiveBuffer);
			generateDubugBuffer(gres_primitiveMortonBuffer);
#endif
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
		//// BVH 버퍼 초기화
		//{
		//	const UINT zero[4] = { 0, 0, 0, 0 };
		//	const UINT invalid[4] = { ~0u, ~0u, ~0u, ~0u };
		//
		//	// gres_bvhNodeBuffer는 float 값들이라 다르게 초기화 필요
		//	dx11DeviceImmContext->ClearUnorderedAccessViewUint(
		//		(ID3D11UnorderedAccessView*)gres_bvhFlagBuffer.alloc_res_ptrs[DTYPE_UAV], zero);
		//	dx11DeviceImmContext->ClearUnorderedAccessViewUint(
		//		(ID3D11UnorderedAccessView*)gres_bvhParentBuffer.alloc_res_ptrs[DTYPE_UAV], invalid);
		//	dx11DeviceImmContext->ClearUnorderedAccessViewUint(
		//		(ID3D11UnorderedAccessView*)gres_primitiveIDBuffer.alloc_res_ptrs[DTYPE_UAV], invalid);
		//	dx11DeviceImmContext->ClearUnorderedAccessViewUint(
		//		(ID3D11UnorderedAccessView*)gres_primitiveMortonBuffer.alloc_res_ptrs[DTYPE_UAV], zero);
		//
		//	// primitive buffer와 node buffer는 구조체라서 compute shader로 초기화하는 게 좋을 수 있음
		//	// 또는 Map으로 CPU에서 초기화
		//}

		// ----- build ----- 
		dx11CommonParams->GpuProfile("BVH Rebuild");

		uint32_t primitiveCount = 0;

		//ID3D11Buffer* cbuf_BVHPushConstants = dx11CommonParams->get_cbuf("BVHPushConstants");
		//D3D11_MAPPED_SUBRESOURCE mapped_pushConstants;

		ID3D11UnorderedAccessView* dx11UAVs_NULL[10] = { };
		dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 10, dx11UAVs_NULL, NULL);
		ID3D11ShaderResourceView* dx11SRVs_NULL[10] = { };
		dx11DeviceImmContext->CSSetShaderResources(0, 10, dx11SRVs_NULL);
		grd_helper::Fence();

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

			geometrics::AABB aabb(XMFLOAT3(primitive->aabb_os.pos_min.x, primitive->aabb_os.pos_min.y, primitive->aabb_os.pos_min.z), 
				XMFLOAT3(primitive->aabb_os.pos_max.x, primitive->aabb_os.pos_max.y, primitive->aabb_os.pos_max.z));
			push.aabb_min = aabb.getMin();
			push.aabb_extents_rcp = aabb.getWidth();
			push.aabb_extents_rcp.x = 1.f / push.aabb_extents_rcp.x;
			push.aabb_extents_rcp.y = 1.f / push.aabb_extents_rcp.y;
			push.aabb_extents_rcp.z = 1.f / push.aabb_extents_rcp.z;

			grd_helper::PushConstants(&push, sizeof(BVHPushConstants), 0);
			grd_helper::Fence();

			//dx11DeviceImmContext->Map(cbuf_BVHPushConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_pushConstants);
			//BVHPushConstants* cbData = (BVHPushConstants*)mapped_pushConstants.pData;
			//memcpy(cbData, &push, sizeof(BVHPushConstants));
			//dx11DeviceImmContext->Unmap(cbuf_BVHPushConstants, 0);
			const ID3D11ShaderResourceView* srv_push = grd_helper::GetPushContantSRV();
			dx11DeviceImmContext->CSSetShaderResources(9, 1, (ID3D11ShaderResourceView* const*)&srv_push);
			grd_helper::Fence();

			primitiveCount = totalTriangles;

			dx11DeviceImmContext->CSSetShader(GETCS(BVH_Primitives_cs_5_0), NULL, 0);
			grd_helper::Fence();
			dx11DeviceImmContext->Dispatch((primitiveCount + BVH_BUILDER_GROUPSIZE - 1) / BVH_BUILDER_GROUPSIZE,
				1,
				1);
			grd_helper::Fence();

			dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 10, dx11UAVs_NULL, NULL);
			dx11DeviceImmContext->CSSetShaderResources(0, 10, dx11SRVs_NULL);
			dx11DeviceImmContext->Flush();
			grd_helper::Fence();
		}


		// update primitiveCounterBuffer
		{
			D3D11_MAPPED_SUBRESOURCE d11MappedRes;
			dx11DeviceImmContext->Map((ID3D11Resource*)gres_primitiveCounterBuffer_write.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_WRITE, 0, &d11MappedRes);
			*(uint*)d11MappedRes.pData = primitiveCount;
			dx11DeviceImmContext->Unmap((ID3D11Resource*)gres_primitiveCounterBuffer_write.alloc_res_ptrs[DTYPE_RES], 0);
			dx11DeviceImmContext->Flush();
			grd_helper::Fence();

			dx11DeviceImmContext->CopyResource((ID3D11Buffer*)gres_primitiveCounterBuffer.alloc_res_ptrs[DTYPE_RES], 
				(ID3D11Buffer*)gres_primitiveCounterBuffer_write.alloc_res_ptrs[DTYPE_RES]);
			dx11DeviceImmContext->Flush();
			grd_helper::Fence();
		}

		// BVH - Sort Primitive Mortons
		{
			grd_helper::Fence();
			gpulib::sort::Sort(gpuManager, dx11CommonParams,
				primitiveCount, gres_primitiveMortonBuffer, gres_primitiveCounterBuffer, 0, gres_primitiveIDBuffer);
			grd_helper::Fence(); 

			//dx11DeviceImmContext->Begin(query);
			dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 10, dx11UAVs_NULL, NULL);
			dx11DeviceImmContext->CSSetShaderResources(0, 10, dx11SRVs_NULL);
			dx11DeviceImmContext->Flush();
			grd_helper::Fence();
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
			grd_helper::Fence();


			dx11DeviceImmContext->CSSetShader(GETCS(BVH_Hierarchy_cs_5_0), NULL, 0);

			dx11DeviceImmContext->Dispatch((primitiveCount + BVH_BUILDER_GROUPSIZE - 1) / BVH_BUILDER_GROUPSIZE,
				1,
				1);
			grd_helper::Fence();

			dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 10, dx11UAVs_NULL, NULL);
			dx11DeviceImmContext->CSSetShaderResources(0, 10, dx11SRVs_NULL);
			dx11DeviceImmContext->Flush();
			grd_helper::Fence();
		}

		// BVH - Propagate AABB
		{
			uint treeDepth = (uint)ceil(log2(primitiveCount));
			vzlog("**** treeDepth : %d", treeDepth);

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
			grd_helper::Fence();

			dx11DeviceImmContext->CSSetShader(GETCS(BVH_Propagateaabb_cs_5_0), NULL, 0);
			for (int i = 0; i < treeDepth; i++) {
				dx11DeviceImmContext->Dispatch((primitiveCount + BVH_BUILDER_GROUPSIZE - 1) / BVH_BUILDER_GROUPSIZE,
					1,
					1);
			}
			grd_helper::Fence();

			dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 10, dx11UAVs_NULL, NULL);
			dx11DeviceImmContext->CSSetShaderResources(0, 10, dx11SRVs_NULL);
			dx11DeviceImmContext->Flush();
			grd_helper::Fence();
		}

		dx11DeviceImmContext->CSSetShaderResources(100, 1, dx11SRVs_NULL);
		grd_helper::Fence();

		dx11CommonParams->GpuProfile("BVH Rebuild", true);

#ifdef BVH_GEN_DEBUG
		if (primitive->num_vtx < 1000)
			return true;

		auto readDubugBuffer = [&gpuManager, &dx11DeviceImmContext](GpuRes& gres)
			{
				GpuRes gres_debug = gres;
				gres_debug.res_name += "_DEBUG";
				vzlog_assert(gpuManager->UpdateGpuResource(gres_debug), "gres_debug");

				dx11DeviceImmContext->CopyResource((ID3D11Buffer*)gres_debug.alloc_res_ptrs[DTYPE_RES],
					(ID3D11Buffer*)gres.alloc_res_ptrs[DTYPE_RES]);

				return gres_debug;

			};

		XMFLOAT3* pp = (XMFLOAT3*)primitive->GetVerticeDefinition("POSITION");

		D3D11_MAPPED_SUBRESOURCE d11MappedRes;
		GpuRes gres_bvhNodeBuffer_DEBUG = readDubugBuffer(gres_bvhNodeBuffer);
		dx11DeviceImmContext->Map((ID3D11Resource*)gres_bvhNodeBuffer_DEBUG.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, 0, &d11MappedRes);
		BVHNode bvh_nodes[10];
		memcpy(bvh_nodes, d11MappedRes.pData, sizeof(BVHNode) * 10);
		dx11DeviceImmContext->Unmap((ID3D11Resource*)gres_bvhNodeBuffer_DEBUG.alloc_res_ptrs[DTYPE_RES], 0);

		GpuRes gres_bvhParentBuffer_DEBUG = readDubugBuffer(gres_bvhParentBuffer);
		dx11DeviceImmContext->Map((ID3D11Resource*)gres_bvhParentBuffer_DEBUG.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, 0, &d11MappedRes);
		uint bvh_parents[10];
		memcpy(bvh_parents, d11MappedRes.pData, sizeof(uint) * 10);
		dx11DeviceImmContext->Unmap((ID3D11Resource*)gres_bvhParentBuffer_DEBUG.alloc_res_ptrs[DTYPE_RES], 0);

		GpuRes gres_bvhFlagBuffer_DEBUG = readDubugBuffer(gres_bvhFlagBuffer);
		dx11DeviceImmContext->Map((ID3D11Resource*)gres_bvhFlagBuffer_DEBUG.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, 0, &d11MappedRes);
		uint bvh_flags[10];
		memcpy(bvh_flags, d11MappedRes.pData, sizeof(uint) * 10);
		dx11DeviceImmContext->Unmap((ID3D11Resource*)gres_bvhFlagBuffer_DEBUG.alloc_res_ptrs[DTYPE_RES], 0);

		GpuRes gres_primitiveIDBuffer_DEBUG = readDubugBuffer(gres_primitiveIDBuffer);
		dx11DeviceImmContext->Map((ID3D11Resource*)gres_primitiveIDBuffer_DEBUG.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, 0, &d11MappedRes);
		uint bvh_prim_IDs[10];
		memcpy(bvh_prim_IDs, d11MappedRes.pData, sizeof(uint) * 10);
		dx11DeviceImmContext->Unmap((ID3D11Resource*)gres_primitiveIDBuffer_DEBUG.alloc_res_ptrs[DTYPE_RES], 0);

		GpuRes gres_primitiveBuffer_DEBUG = readDubugBuffer(gres_primitiveBuffer);
		dx11DeviceImmContext->Map((ID3D11Resource*)gres_primitiveBuffer_DEBUG.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, 0, &d11MappedRes);
		BVHPrimitive bvh_prims[1000];
		memcpy(bvh_prims, d11MappedRes.pData, sizeof(BVHPrimitive) * 1000);
		dx11DeviceImmContext->Unmap((ID3D11Resource*)gres_primitiveBuffer_DEBUG.alloc_res_ptrs[DTYPE_RES], 0);

		GpuRes gres_primitiveMortonBuffer_DEBUG = readDubugBuffer(gres_primitiveMortonBuffer);
		dx11DeviceImmContext->Map((ID3D11Resource*)gres_primitiveMortonBuffer_DEBUG.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, 0, &d11MappedRes);
		float bvh_mortons[10];
		memcpy(bvh_mortons, d11MappedRes.pData, sizeof(float) * 10);
		dx11DeviceImmContext->Unmap((ID3D11Resource*)gres_primitiveMortonBuffer_DEBUG.alloc_res_ptrs[DTYPE_RES], 0);
		
		
		
		struct RayDesc
		{
			float3 Origin;
			float TMin;
			float3 Direction;
			float TMax;
		};

		RayDesc ray;
		ray.Origin = float3(0, 0, -1);
		ray.Direction = float3(0.01, 0.01, -0.9);
		ray.TMin = 0.00001;
		ray.TMax = FLT_MAX;

		const float3 rcpDirection = float3(1. / ray.Direction.x, 1. / ray.Direction.y, 1. / ray.Direction.z);

		const float t0 = (bvh_nodes[0].min.x - ray.Origin.x) * rcpDirection.x;
		const float t1 = (bvh_nodes[0].max.x - ray.Origin.x) * rcpDirection.x;
		const float t2 = (bvh_nodes[0].min.y - ray.Origin.y) * rcpDirection.y;
		const float t3 = (bvh_nodes[0].max.y - ray.Origin.y) * rcpDirection.y;
		const float t4 = (bvh_nodes[0].min.z - ray.Origin.z) * rcpDirection.z;
		const float t5 = (bvh_nodes[0].max.z - ray.Origin.z) * rcpDirection.z;
		const float tmin = max(max(min(t0, t1), min(t2, t3)), min(t4, t5)); // close intersection point's distance on ray
		const float tmax = min(min(max(t0, t1), max(t2, t3)), max(t4, t5)); // far intersection point's distance on ray

		if (tmax < 0 || tmin > tmax || tmin > FLT_MAX) // this also checks if a better primitive was already hit or not and skips if yes
		{
			return false;
		}
		else
		{
			return true;
		}
#endif
		dx11DeviceImmContext->Flush();
		grd_helper::Fence();

		return true;
	}
}