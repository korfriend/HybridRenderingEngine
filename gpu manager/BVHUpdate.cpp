#include "BVHUpdate.h"
#include "vzm2/Backlog.h"
#include "hlsl/ShaderInterop_BVH.h"

namespace bvh {

	bool UpdateGeometryGPUBVH(VmGpuManager* gpuManager, VmVObjectPrimitive* pobj)
	{
		PrimitiveData* primitive = pobj->GetPrimitiveData();
		assert(primitive);

		if (primitive->num_prims == 0)
		{
			vzlog_error("target primitive is not valid (no triangles)!!");
			return false;
		}

		GpuRes gres_vtx, gres_idx;

		gres_vtx.vm_src_id = pobj->GetObjectID();
		gres_vtx.res_name = string("PRIMITIVE_MODEL_VTX");

		gres_idx.vm_src_id = pobj->GetObjectID();
		gres_idx.res_name = string("PRIMITIVE_MODEL_IDX");

		if (!gpuManager->UpdateGpuResource(gres_vtx) || !gpuManager->UpdateGpuResource(gres_idx))
		{
			vzlog_error("target primitive MUST be registered in GPU memory!!");
			return false;
		}

		uint totalTriangles = primitive->num_prims;
		/*
		BVHBuffers& bvhBuffers = part_buffers->bvhBuffers;
		if (totalTriangles > 0 && !bvhBuffers.primitiveCounterBuffer.IsValid())
		{
			GPUBufferDesc desc;
			desc.bind_flags = BindFlag::SHADER_RESOURCE;
			desc.stride = sizeof(uint);
			desc.size = desc.stride;
			desc.misc_flags = ResourceMiscFlag::BUFFER_RAW;
			desc.usage = Usage::DEFAULT;
			device->CreateBuffer(&desc, nullptr, &bvhBuffers.primitiveCounterBuffer);
			device->SetName(&bvhBuffers.primitiveCounterBuffer, "GPUBVH::primitiveCounterBuffer");
		}
		else
		{
			bvhBuffers.primitiveCounterBuffer = {};
		}

		if (totalTriangles > bvhBuffers.primitiveCapacity)
		{
			bvhBuffers.primitiveCapacity = std::max(2u, totalTriangles);

			GPUBufferDesc desc;

			desc.bind_flags = BindFlag::SHADER_RESOURCE | BindFlag::UNORDERED_ACCESS;
			desc.stride = sizeof(BVHNode);
			desc.size = desc.stride * bvhBuffers.primitiveCapacity * 2;
			desc.misc_flags = ResourceMiscFlag::BUFFER_RAW;
			desc.usage = Usage::DEFAULT;
			device->CreateBuffer(&desc, nullptr, &bvhBuffers.bvhNodeBuffer);
			device->SetName(&bvhBuffers.bvhNodeBuffer, "GPUBVH::BVHNodeBuffer");

			desc.bind_flags = BindFlag::SHADER_RESOURCE | BindFlag::UNORDERED_ACCESS;
			desc.stride = sizeof(uint);
			desc.size = desc.stride * bvhBuffers.primitiveCapacity * 2;
			desc.misc_flags = ResourceMiscFlag::BUFFER_STRUCTURED;
			desc.usage = Usage::DEFAULT;
			device->CreateBuffer(&desc, nullptr, &bvhBuffers.bvhParentBuffer);
			device->SetName(&bvhBuffers.bvhParentBuffer, "GPUBVH::BVHParentBuffer");

			desc.bind_flags = BindFlag::SHADER_RESOURCE | BindFlag::UNORDERED_ACCESS;
			desc.stride = sizeof(uint);
			desc.size = desc.stride * (((bvhBuffers.primitiveCapacity - 1) + 31) / 32); // bitfield for internal nodes
			desc.misc_flags = ResourceMiscFlag::BUFFER_STRUCTURED;
			desc.usage = Usage::DEFAULT;
			device->CreateBuffer(&desc, nullptr, &bvhBuffers.bvhFlagBuffer);
			device->SetName(&bvhBuffers.bvhFlagBuffer, "GPUBVH::BVHFlagBuffer");

			desc.bind_flags = BindFlag::SHADER_RESOURCE | BindFlag::UNORDERED_ACCESS;
			desc.stride = sizeof(uint);
			desc.size = desc.stride * bvhBuffers.primitiveCapacity;
			desc.misc_flags = ResourceMiscFlag::BUFFER_STRUCTURED;
			desc.usage = Usage::DEFAULT;
			device->CreateBuffer(&desc, nullptr, &bvhBuffers.primitiveIDBuffer);
			device->SetName(&bvhBuffers.primitiveIDBuffer, "GPUBVH::primitiveIDBuffer");

			desc.bind_flags = BindFlag::SHADER_RESOURCE | BindFlag::UNORDERED_ACCESS;
			desc.stride = sizeof(uint);
			desc.size = desc.stride * bvhBuffers.primitiveCapacity;
			desc.misc_flags = ResourceMiscFlag::BUFFER_STRUCTURED;

			desc.bind_flags = BindFlag::SHADER_RESOURCE | BindFlag::UNORDERED_ACCESS;
			desc.stride = sizeof(BVHPrimitive);
			desc.size = desc.stride * bvhBuffers.primitiveCapacity;
			desc.misc_flags = ResourceMiscFlag::BUFFER_RAW;
			desc.usage = Usage::DEFAULT;
			device->CreateBuffer(&desc, nullptr, &bvhBuffers.primitiveBuffer);
			device->SetName(&bvhBuffers.primitiveBuffer, "GPUBVH::primitiveBuffer");

			desc.bind_flags = BindFlag::SHADER_RESOURCE | BindFlag::UNORDERED_ACCESS;
			desc.size = desc.stride * bvhBuffers.primitiveCapacity;
			desc.misc_flags = ResourceMiscFlag::BUFFER_STRUCTURED;
			desc.usage = Usage::DEFAULT;
			desc.stride = sizeof(float); // morton buffer is float because sorting must be done and gpu sort operates on floats for now!
			device->CreateBuffer(&desc, nullptr, &bvhBuffers.primitiveMortonBuffer);
			device->SetName(&bvhBuffers.primitiveMortonBuffer, "GPUBVH::primitiveMortonBuffer");
		}


		// ----- build ----- 
		auto range = profiler::BeginRangeGPU("BVH Rebuild", &cmd);

		uint32_t primitiveCount = 0;

		device->EventBegin("BVH - Primitive (GEOMETRY-ONLY) Builder", cmd);
		{
			device->BindComputeShader(&rcommon::shaders[CSTYPE_BVH_PRIMITIVES_GEOMETRYONLY], cmd);
			const GPUResource* uavs[] = {
				&bvhBuffers.primitiveIDBuffer,
				&bvhBuffers.primitiveBuffer,
				&bvhBuffers.primitiveMortonBuffer,
			};
			device->BindUAVs(uavs, 0, arraysize(uavs), cmd);

			BVHPushConstants push;
			push.geometryIndex = geometry->geometryOffset;
			push.subsetIndex = i;
			push.primitiveCount = totalTriangles;
			push.instanceIndex = 0; // geometry-binding BVH does NOT require instance!

			const geometrics::AABB& aabb = prim.GetAABB();
			push.aabb_min = aabb.getMin();
			push.aabb_extents_rcp = aabb.getWidth();
			push.aabb_extents_rcp.x = 1.f / push.aabb_extents_rcp.x;
			push.aabb_extents_rcp.y = 1.f / push.aabb_extents_rcp.y;
			push.aabb_extents_rcp.z = 1.f / push.aabb_extents_rcp.z;

			device->PushConstants(&push, sizeof(push), cmd);

			primitiveCount += push.primitiveCount;

			device->Dispatch(
				(push.primitiveCount + BVH_BUILDER_GROUPSIZE - 1) / BVH_BUILDER_GROUPSIZE,
				1,
				1,
				cmd
			);

			GPUBarrier barriers[] = {
				GPUBarrier::Memory()
			};
			device->Barrier(barriers, arraysize(barriers), cmd);
		}

		{
			GPUBarrier barriers[] = {
				GPUBarrier::Buffer(&bvhBuffers.primitiveCounterBuffer, ResourceState::SHADER_RESOURCE, ResourceState::COPY_DST),
			};
			device->Barrier(barriers, arraysize(barriers), cmd);
		}
		device->UpdateBuffer(&bvhBuffers.primitiveCounterBuffer, &primitiveCount, cmd);
		{
			GPUBarrier barriers[] = {
				GPUBarrier::Buffer(&bvhBuffers.primitiveCounterBuffer, ResourceState::COPY_DST, ResourceState::SHADER_RESOURCE),
			};
			device->Barrier(barriers, arraysize(barriers), cmd);
		}

		device->EventEnd(cmd);

		device->EventBegin("BVH - Sort Primitive Mortons", cmd);
		gpusortlib::Sort(primitiveCount, bvhBuffers.primitiveMortonBuffer, bvhBuffers.primitiveCounterBuffer, 0, bvhBuffers.primitiveIDBuffer, cmd);
		device->EventEnd(cmd);

		device->EventBegin("BVH - Build Hierarchy", cmd);
		{
			device->BindComputeShader(&rcommon::shaders[CSTYPE_BVH_HIERARCHY], cmd);
			const GPUResource* uavs[] = {
				&bvhBuffers.bvhNodeBuffer,
				&bvhBuffers.bvhParentBuffer,
				&bvhBuffers.bvhFlagBuffer
			};
			device->BindUAVs(uavs, 0, arraysize(uavs), cmd);

			const GPUResource* res[] = {
				&bvhBuffers.primitiveCounterBuffer,
				&bvhBuffers.primitiveIDBuffer,
				&bvhBuffers.primitiveMortonBuffer,
			};
			device->BindResources(res, 0, arraysize(res), cmd);

			device->Dispatch((primitiveCount + BVH_BUILDER_GROUPSIZE - 1) / BVH_BUILDER_GROUPSIZE, 1, 1, cmd);

			GPUBarrier barriers[] = {
				GPUBarrier::Memory()
			};
			device->Barrier(barriers, arraysize(barriers), cmd);
		}
		device->EventEnd(cmd);

		device->EventBegin("BVH - Propagate AABB", cmd);
		{
			GPUBarrier barriers[] = {
				GPUBarrier::Memory()
			};
			device->Barrier(barriers, arraysize(barriers), cmd);

			device->BindComputeShader(&rcommon::shaders[CSTYPE_BVH_PROPAGATEAABB], cmd);
			const GPUResource* uavs[] = {
				&bvhBuffers.bvhNodeBuffer,
				&bvhBuffers.bvhFlagBuffer,
			};
			device->BindUAVs(uavs, 0, arraysize(uavs), cmd);

			const GPUResource* res[] = {
				&bvhBuffers.primitiveCounterBuffer,
				&bvhBuffers.primitiveIDBuffer,
				&bvhBuffers.primitiveBuffer,
				&bvhBuffers.bvhParentBuffer,
			};
			device->BindResources(res, 0, arraysize(res), cmd);

			device->Dispatch((primitiveCount + BVH_BUILDER_GROUPSIZE - 1) / BVH_BUILDER_GROUPSIZE, 1, 1, cmd);

			device->Barrier(barriers, arraysize(barriers), cmd);
		}
		device->EventEnd(cmd);

		profiler::EndRange(range); // BVH rebuild
		/**/
		return true;
	}
}