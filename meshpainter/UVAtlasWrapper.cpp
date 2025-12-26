// UVAtlasWrapper.cpp
// Implementation of UVAtlas wrapper for UV unwrapping

#include "UVAtlasWrapper.h"
#include "uvatlas/UVAtlas.h"
#include "vzm2/Backlog.h"
#include <DirectXMath.h>
#include <algorithm>
#include <unordered_map>
#include <chrono>

using namespace DirectX;

void UnwrapUVsUVAtlas(
	const float* positions,
	size_t positionCount,
	const unsigned int* indices,
	size_t indexCount,
	std::vector<float>& uvs,
	int textureSize,
	float gutter,
	float maxStretch,
	bool fastMode
) {
	const bool isIndexed = (indices != nullptr && indexCount > 0);
	const size_t vertexCount = positionCount / 3;
	const size_t triCount = isIndexed ? (indexCount / 3) : (vertexCount / 3);

	vzlog("Starting UVAtlas UV Unwrap... %d triangles", (int)triCount);
	auto startTotal = std::chrono::high_resolution_clock::now();

	if (triCount == 0) {
		uvs.clear();
		return;
	}

	// Convert positions to XMFLOAT3
	auto startConvert = std::chrono::high_resolution_clock::now();
	std::vector<XMFLOAT3> xmPositions(vertexCount);
	for (size_t i = 0; i < vertexCount; i++) {
		xmPositions[i].x = positions[i * 3 + 0];
		xmPositions[i].y = positions[i * 3 + 1];
		xmPositions[i].z = positions[i * 3 + 2];
	}
	auto endConvert = std::chrono::high_resolution_clock::now();
	vzlog("  [TIMING] Position conversion: %.2f ms",
		std::chrono::duration<double, std::milli>(endConvert - startConvert).count());

	// Prepare indices for UVAtlas
	std::vector<uint32_t> uvatlasIndices;
	if (isIndexed) {
		uvatlasIndices.assign(indices, indices + indexCount);
	}
	else {
		// Create sequential indices for non-indexed mesh
		uvatlasIndices.resize(vertexCount);
		for (size_t i = 0; i < vertexCount; i++) {
			uvatlasIndices[i] = (uint32_t)i;
		}
	}

	// Build adjacency (required by UVAtlas) - POSITION-BASED for non-indexed meshes
	// O(N) algorithm using unordered_map for fast edge lookup by position
	auto startAdj = std::chrono::high_resolution_clock::now();
	std::vector<uint32_t> adjacency(triCount * 3, uint32_t(-1));

	vzlog("Building position-based adjacency for %d triangles...", (int)triCount);

	// Edge hash map: key = edge (sorted position pairs), value = (triangle index, edge index)
	struct EdgeKey {
		int64_t x0, y0, z0, x1, y1, z1;

		static int64_t quantize(float v) {
			const float epsilon = 1e-5f;
			return (int64_t)std::round(v / epsilon);
		}

		EdgeKey(const XMFLOAT3& p0, const XMFLOAT3& p1) {
			int64_t ax = quantize(p0.x), ay = quantize(p0.y), az = quantize(p0.z);
			int64_t bx = quantize(p1.x), by = quantize(p1.y), bz = quantize(p1.z);

			// Sort positions to make edge order-independent
			if (ax < bx || (ax == bx && ay < by) || (ax == bx && ay == by && az < bz)) {
				x0 = ax; y0 = ay; z0 = az;
				x1 = bx; y1 = by; z1 = bz;
			} else {
				x0 = bx; y0 = by; z0 = bz;
				x1 = ax; y1 = ay; z1 = az;
			}
		}

		bool operator==(const EdgeKey& other) const {
			return x0 == other.x0 && y0 == other.y0 && z0 == other.z0 &&
			       x1 == other.x1 && y1 == other.y1 && z1 == other.z1;
		}
	};

	struct EdgeKeyHash {
		size_t operator()(const EdgeKey& k) const {
			size_t h0 = std::hash<int64_t>{}(k.x0) ^ (std::hash<int64_t>{}(k.y0) << 1);
			size_t h1 = std::hash<int64_t>{}(k.z0) ^ (std::hash<int64_t>{}(k.x1) << 2);
			size_t h2 = std::hash<int64_t>{}(k.y1) ^ (std::hash<int64_t>{}(k.z1) << 3);
			return h0 ^ h1 ^ h2;
		}
	};

	struct EdgeInfo {
		uint32_t triIndex;
		uint32_t edgeIndex;
	};

	std::unordered_map<EdgeKey, EdgeInfo, EdgeKeyHash> edgeMap;
	edgeMap.reserve(triCount * 3);

	// Build adjacency in single pass using vertex positions
	for (size_t i = 0; i < triCount; i++) {
		for (int e = 0; e < 3; e++) {
			uint32_t v0 = uvatlasIndices[i * 3 + e];
			uint32_t v1 = uvatlasIndices[i * 3 + ((e + 1) % 3)];

			const XMFLOAT3& p0 = xmPositions[v0];
			const XMFLOAT3& p1 = xmPositions[v1];

			EdgeKey key(p0, p1);
			auto it = edgeMap.find(key);

			if (it != edgeMap.end()) {
				// Found matching edge - set adjacency for both
				adjacency[i * 3 + e] = it->second.triIndex;
				adjacency[it->second.triIndex * 3 + it->second.edgeIndex] = (uint32_t)i;
				// Remove from map (edge already matched)
				edgeMap.erase(it);
			}
			else {
				// First occurrence - add to map
				edgeMap[key] = { (uint32_t)i, (uint32_t)e };
			}
		}
	}

	auto endAdj = std::chrono::high_resolution_clock::now();
	vzlog("Adjacency built (%d boundary edges, %d matched edges)",
		(int)edgeMap.size(), (int)(triCount * 3 - edgeMap.size()));
	vzlog("  [TIMING] Adjacency calculation: %.2f ms",
		std::chrono::duration<double, std::milli>(endAdj - startAdj).count());

	// Output buffers
	std::vector<UVAtlasVertex> vertexBuffer;
	std::vector<uint8_t> indexBuffer;
	std::vector<uint32_t> facePartitioning;
	std::vector<uint32_t> vertexRemapArray;
	float maxStretchOut = 0.0f;
	size_t numChartsOut = 0;

	// Status callback (optional, can be nullptr)
	auto statusCallback = [](float percentComplete) -> HRESULT {
		// Silent callback - can add logging here if needed
		return S_OK;
	};

	// Chart count: Keep it sufficient to avoid failures
	// Testing: 54 charts=success(52s), 220 charts=success(113s), 10 charts=FAIL
	// Use ~2000 tris per chart (proven to work)
	size_t maxCharts = (triCount / 2000 > 1) ? (triCount / 2000) : 1;
	if (maxCharts > 100) maxCharts = 100;  // Cap at 100

	vzlog("Calling UVAtlas (triCount: %d, maxCharts: %d, gutter: %.1f)...",
		(int)triCount, (int)maxCharts, gutter);

	// Partition adjacency output (required for packing)
	std::vector<uint32_t> partitionAdj;

	// Use UVAtlasPartition (known to work)
	auto startPartition = std::chrono::high_resolution_clock::now();
	HRESULT hr = UVAtlasPartition(
		xmPositions.data(),
		vertexCount,
		uvatlasIndices.data(),
		DXGI_FORMAT_R32_UINT,
		triCount,
		maxCharts,  // Use chart count for speed
		0.0f,  // maxStretch (ignored when using chart count)
		adjacency.data(),  // Provide adjacency
		nullptr,  // falseEdgeAdjacency
		nullptr,  // pIMTArray
		statusCallback,
		UVATLAS_DEFAULT_CALLBACK_FREQUENCY,
		UVATLAS_GEODESIC_FAST,  // Fast mode only
		vertexBuffer,
		indexBuffer,
		&facePartitioning,
		&vertexRemapArray,
		partitionAdj,  // Output partition adjacency
		&maxStretchOut,
		&numChartsOut
	);

	auto endPartition = std::chrono::high_resolution_clock::now();

	if (SUCCEEDED(hr)) {
		vzlog("UVAtlas partition succeeded (%d charts), packing...", (int)numChartsOut);
		vzlog("  [TIMING] UVAtlasPartition: %.2f ms",
			std::chrono::duration<double, std::milli>(endPartition - startPartition).count());

		// Pack the charts
		auto startPack = std::chrono::high_resolution_clock::now();
		hr = UVAtlasPack(
			vertexBuffer,
			indexBuffer,
			DXGI_FORMAT_R32_UINT,
			(size_t)textureSize,
			(size_t)textureSize,
			gutter,
			partitionAdj,
			statusCallback,
			UVATLAS_DEFAULT_CALLBACK_FREQUENCY
		);
		auto endPack = std::chrono::high_resolution_clock::now();

		if (SUCCEEDED(hr)) {
			vzlog("UVAtlas pack succeeded");
			vzlog("  [TIMING] UVAtlasPack: %.2f ms",
				std::chrono::duration<double, std::milli>(endPack - startPack).count());
		}
	}
	else {
		vzlog("  [TIMING] UVAtlasPartition FAILED: %.2f ms",
			std::chrono::duration<double, std::milli>(endPartition - startPartition).count());
	}

	if (FAILED(hr)) {
		vzlog("ERROR: UVAtlas failed with HRESULT 0x%08X", hr);
		uvs.clear();
		return;
	}

	vzlog("UVAtlas generated %d charts, max stretch: %.3f",
		(int)numChartsOut, maxStretchOut);

	// Extract UVs from result
	// UVAtlas outputs indexed vertices, so we need to reconstruct triangle-indexed UVs
	auto startExtract = std::chrono::high_resolution_clock::now();
	uvs.resize(triCount * 3 * 2);

	// Determine index format (uint16 or uint32)
	const bool is16Bit = (indexBuffer.size() == triCount * 3 * sizeof(uint16_t));

	for (size_t i = 0; i < triCount * 3; i++) {
		uint32_t vertexIndex;
		if (is16Bit) {
			const uint16_t* pIndices = reinterpret_cast<const uint16_t*>(indexBuffer.data());
			vertexIndex = pIndices[i];
		}
		else {
			const uint32_t* pIndices = reinterpret_cast<const uint32_t*>(indexBuffer.data());
			vertexIndex = pIndices[i];
		}

		if (vertexIndex >= vertexBuffer.size()) {
			vzlog("ERROR: UVAtlas vertex index out of range: %d >= %d",
				vertexIndex, (int)vertexBuffer.size());
			continue;
		}

		const UVAtlasVertex& vertex = vertexBuffer[vertexIndex];

		// UVAtlas outputs UVs in [0,1] range
		uvs[i * 2 + 0] = vertex.uv.x;
		uvs[i * 2 + 1] = vertex.uv.y;
	}

	auto endExtract = std::chrono::high_resolution_clock::now();
	vzlog("  [TIMING] UV extraction: %.2f ms",
		std::chrono::duration<double, std::milli>(endExtract - startExtract).count());

	auto endTotal = std::chrono::high_resolution_clock::now();
	vzlog("UVAtlas UV Unwrap complete. Output: %d floats (%d vertices)",
		(int)uvs.size(), (int)vertexBuffer.size());
	vzlog("  [TIMING] TOTAL TIME: %.2f ms",
		std::chrono::duration<double, std::milli>(endTotal - startTotal).count());
}
