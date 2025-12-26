// UVAtlasWrapper.h
// Wrapper for Microsoft UVAtlas library with interface similar to UnwrapUVs
#pragma once
#include <vector>

// UVAtlas-based UV unwrapping (Microsoft DirectX library)
//
// This wrapper uses Microsoft's UVAtlas library for mesh parameterization.
// UVAtlas provides:
// - Fast chart generation optimized for DirectX meshes
// - Geodesic-based partitioning (FAST or QUALITY modes)
// - Excellent packing efficiency
// - Good performance with large meshes
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
//   textureSize: target texture resolution (default: 1024). Used for gutter calculation.
//   gutter: gutter in texels between charts (default: 2.0). Prevents bleeding artifacts.
//   maxStretch: maximum stretch allowed (default: 0.16667 = 1/6). 0 = no stretch, 1 = unlimited.
//   fastMode: use GEODESIC_FAST (true) or GEODESIC_QUALITY (false) mode
void UnwrapUVsUVAtlas(
	const float* positions,
	size_t positionCount,
	const unsigned int* indices,
	size_t indexCount,
	std::vector<float>& uvs,
	int textureSize = 1024,
	float gutter = 2.0f,
	float maxStretch = 0.16667f,
	bool fastMode = true
);
