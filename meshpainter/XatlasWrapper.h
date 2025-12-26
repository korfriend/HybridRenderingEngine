// XatlasWrapper.h
// Wrapper for xatlas library with interface similar to UnwrapUVs
#pragma once
#include <vector>

// xatlas-based UV unwrapping
//
// This wrapper uses the xatlas library (https://github.com/jpcy/xatlas) for mesh parameterization.
// xatlas provides robust UV unwrapping with:
// - Automatic chart generation with various methods (Planar, Ortho, LSCM, Piecewise)
// - Efficient chart packing
// - Proper handling of UV seams and discontinuities
// - Padding control to prevent texture bleeding
//
// Note: Output UVs are per-triangle-vertex (non-indexed style)
// The output uvs array will have size: (triCount * 3 * 2)
// Each triangle has 3 vertices, each vertex has 2 UV components (u, v)
//
// Parameters:
//   positions: vertex positions array (x,y,z, x,y,z, ...)
//   positionCount: number of floats in positions array (vertexCount * 3)
//   indices: index array (can be nullptr if not indexed)
//   indexCount: number of indices (0 if not indexed)
//   uvs: output UV array (will be resized to triCount * 3 * 2)
//   textureSize: target texture resolution (default: 1024). Used to calculate padding.
//   padding: padding in texels between charts (default: 4). Prevents bleeding artifacts.
void UnwrapUVsXatlas(
	const float* positions,
	size_t positionCount,
	const unsigned int* indices,
	size_t indexCount,
	std::vector<float>& uvs,
	int textureSize = 1024,
	int padding = 4
);
