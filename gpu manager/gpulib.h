#pragma once
#include "gpures_helper.h"

namespace gpulib {
	namespace sort {
		// Perform bitonic sort on a GPU dataset
		//	maxCount				-	Maximum size of the dataset. GPU count can be smaller (see: counterBuffer_read param)
		//	comparisonBuffer_read	-	Buffer containing values to compare by (Read Only)
		//	counterBuffer_read		-	Buffer containing count of values to sort (Read Only)
		//	counterReadOffset		-	Byte offset into the counter buffer to read the count value (Read Only)
		//	indexBuffer_write		-	The index list which to sort. Contains index values which can index the sortBase_read buffer. This will be modified (Read + Write)
		void Sort(VmGpuManager* gpu_manager,
			grd_helper::PSOManager* psoManager,
			uint32_t maxCount,
			GpuRes& comparisonBuffer_read,
			GpuRes& counterBuffer_read,
			uint32_t counterReadOffset,
			GpuRes& indexBuffer_write
			);
	}
}
