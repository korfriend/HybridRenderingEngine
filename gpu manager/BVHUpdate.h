#pragma once

#include "../gpu_common_res.h"
#include "gpures_helper.h"

namespace bvh
{
	bool UpdateGeometryGPUBVH(VmGpuManager* gpuManager, VmVObjectPrimitive* pobj);
}