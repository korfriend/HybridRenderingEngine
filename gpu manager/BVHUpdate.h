#pragma once

#include "../gpu_common_res.h"
#include "gpures_helper.h"

namespace bvh
{
	bool UpdateGeometryGPUBVH(VmGpuManager* gpuManager, grd_helper::GpuDX11CommonParameters* dx11CommonParams, VmVObjectPrimitive* pobj);
}