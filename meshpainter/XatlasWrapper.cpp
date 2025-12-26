// XatlasWrapper.cpp
// Implementation of xatlas wrapper for UV unwrapping

#include "XatlasWrapper.h"
#include "xatlas.h"
#include "vzm2/Backlog.h"
#include <iostream>

void UnwrapUVsXatlas(
	const float* positions,
	size_t positionCount,
	const unsigned int* indices,
	size_t indexCount,
	std::vector<float>& uvs,
	int textureSize,
	int padding
) {
	const bool isIndexed = (indices != nullptr && indexCount > 0);
	const size_t vertexCount = positionCount / 3;
	const size_t triCount = isIndexed ? (indexCount / 3) : (vertexCount / 3);

	vzlog("Starting xatlas UV Unwrap... %d triangles", (int)triCount);

	if (triCount == 0) {
		uvs.clear();
		return;
	}

	// Create xatlas atlas
	xatlas::Atlas* atlas = xatlas::Create();

	// Prepare mesh declaration
	xatlas::MeshDecl meshDecl;
	meshDecl.vertexPositionData = positions;
	meshDecl.vertexCount = (uint32_t)vertexCount;
	meshDecl.vertexPositionStride = sizeof(float) * 3;

	if (isIndexed) {
		meshDecl.indexData = indices;
		meshDecl.indexCount = (uint32_t)indexCount;
		meshDecl.indexFormat = xatlas::IndexFormat::UInt32;
	}
	else {
		// Non-indexed mesh: xatlas will treat each triangle as separate
		meshDecl.indexData = nullptr;
		meshDecl.indexCount = 0;
	}

	// Add mesh to atlas
	xatlas::AddMeshError error = xatlas::AddMesh(atlas, meshDecl);
	if (error != xatlas::AddMeshError::Success) {
		vzlog("ERROR: xatlas AddMesh failed with error code %d", (int)error);
		xatlas::Destroy(atlas);
		uvs.clear();
		return;
	}

	// Configure chart generation options
	xatlas::ChartOptions chartOptions;
	chartOptions.maxIterations = 1;  // Balance between quality and speed

	// Configure packing options
	xatlas::PackOptions packOptions;
	packOptions.padding = (uint32_t)padding;  // Padding in texels
	packOptions.texelsPerUnit = 0.0f;  // Auto-calculate
	packOptions.resolution = (uint32_t)textureSize;  // Target resolution
	packOptions.bilinear = true;  // Leave space for bilinear filtering
	packOptions.blockAlign = true;  // Align to 4x4 blocks for better packing
	packOptions.bruteForce = false;  // Use faster packing (not brute force)
	packOptions.createImage = false;  // We don't need the debug image
	packOptions.rotateChartsToAxis = true;  // Better packing
	packOptions.rotateCharts = true;  // Allow chart rotation

	// Generate atlas (compute charts + pack)
	vzlog("Computing charts and packing...");
	xatlas::Generate(atlas, chartOptions, packOptions);

	vzlog("xatlas generated %d charts, atlas size: %dx%d",
		atlas->chartCount, atlas->width, atlas->height);

	// Extract UVs from atlas
	// Output format: per-triangle-vertex (non-indexed)
	// Each triangle has 3 vertices, each with 2 UV components
	uvs.resize(triCount * 3 * 2);

	if (atlas->meshCount == 0) {
		vzlog("ERROR: xatlas generated no output meshes");
		xatlas::Destroy(atlas);
		uvs.clear();
		return;
	}

	const xatlas::Mesh& outputMesh = atlas->meshes[0];

	// Normalize UVs to [0, 1] range
	float invWidth = 1.0f / (float)atlas->width;
	float invHeight = 1.0f / (float)atlas->height;

	// For non-indexed meshes, we need to map output vertices back to triangle order
	// xatlas may reorder/duplicate vertices, so we use the index array
	if (outputMesh.indexCount != triCount * 3) {
		vzlog("WARNING: xatlas index count mismatch. Expected %d, got %d",
			(int)(triCount * 3), outputMesh.indexCount);
	}

	// Build output UV array in triangle order
	for (size_t i = 0; i < triCount * 3; i++) {
		uint32_t vertexIndex = outputMesh.indexArray[i];

		if (vertexIndex >= outputMesh.vertexCount) {
			vzlog("ERROR: xatlas vertex index out of range: %d >= %d",
				vertexIndex, outputMesh.vertexCount);
			continue;
		}

		const xatlas::Vertex& vertex = outputMesh.vertexArray[vertexIndex];

		// Normalize UV coordinates to [0, 1]
		float u = vertex.uv[0] * invWidth;
		float v = vertex.uv[1] * invHeight;

		// Clamp to [0, 1] for safety
		u = std::max(0.0f, std::min(1.0f, u));
		v = std::max(0.0f, std::min(1.0f, v));

		uvs[i * 2 + 0] = u;
		uvs[i * 2 + 1] = v;
	}

	// Save utilization before cleanup
	float utilization = (atlas->utilization != nullptr && atlas->atlasCount > 0)
		? atlas->utilization[0] * 100.0f
		: 0.0f;

	// Cleanup
	xatlas::Destroy(atlas);

	vzlog("xatlas UV Unwrap complete. Output: %d floats (%.1f%% utilization)",
		(int)uvs.size(), utilization);
}
