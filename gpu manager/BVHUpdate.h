#pragma once

#include "gpures_helper.h"

namespace bvh
{
	bool UpdateGeometryGPUBVH(VmGpuManager* gpuManager, grd_helper::PSOManager* psoManager, VmVObjectPrimitive* pobj);
}