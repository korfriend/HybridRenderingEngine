// UnwrapUVs.cpp
// Angle-based Chart Generation UV Unwrapping
// Ported from JavaScript implementation

#include "UnwrapUVs.h"
#include <iostream>
#include <unordered_map>
#include <stack>

namespace {
	// Helper to create edge key for adjacency map
	std::string makeEdgeKey(unsigned int a, unsigned int b) {
		if (a < b) {
			return std::to_string(a) + "_" + std::to_string(b);
		}
		return std::to_string(b) + "_" + std::to_string(a);
	}

	// Chart data structure
	struct Chart {
		std::vector<size_t> triangles;
		Vec3 avgNormal;
	};

	// Chart projection data
	struct ChartData {
		std::vector<size_t> triangles;
		std::vector<float> localUVs;  // 6 floats per triangle (3 vertices * 2 components)
		float width = 0;
		float height = 0;
		float x = 0;  // Packed position
		float y = 0;
	};
}

void UnwrapUVs(
	const float* positions,
	size_t positionCount,
	const unsigned int* indices,
	size_t indexCount,
	std::vector<float>& uvs,
	float angleThresholdDegrees
) {
	const bool isIndexed = (indices != nullptr && indexCount > 0);
	const size_t vertexCount = positionCount / 3;
	const size_t triCount = isIndexed ? (indexCount / 3) : (vertexCount / 3);

	std::cout << "Starting Smart UV Unwrap... " << triCount << " triangles" << std::endl;

	if (triCount == 0) {
		uvs.clear();
		return;
	}

	// Lambda to get triangle vertex indices
	auto getTriIndices = [&](size_t ti, unsigned int& i0, unsigned int& i1, unsigned int& i2) {
		if (isIndexed) {
			i0 = indices[ti * 3];
			i1 = indices[ti * 3 + 1];
			i2 = indices[ti * 3 + 2];
		}
		else {
			i0 = static_cast<unsigned int>(ti * 3);
			i1 = static_cast<unsigned int>(ti * 3 + 1);
			i2 = static_cast<unsigned int>(ti * 3 + 2);
		}
	};

	// Lambda to get vertex position
	auto getVertex = [&](unsigned int idx) -> Vec3 {
		size_t base = idx * 3;
		return Vec3(positions[base], positions[base + 1], positions[base + 2]);
	};

	// ============================================================
	// Step 1: Precompute Face Normals
	// ============================================================
	std::vector<Vec3> normals(triCount);

	for (size_t i = 0; i < triCount; i++) {
		unsigned int i0, i1, i2;
		getTriIndices(i, i0, i1, i2);

		Vec3 v0 = getVertex(i0);
		Vec3 v1 = getVertex(i1);
		Vec3 v2 = getVertex(i2);

		// Edge vectors
		Vec3 e1 = v1.clone().sub(v0);
		Vec3 e2 = v2.clone().sub(v0);

		// Normal = e1 x e2
		Vec3 n = e1.cross(e2);
		n.normalize();

		normals[i] = n;
	}

	// ============================================================
	// Step 2: Build Adjacency Graph
	// ============================================================
	std::vector<std::vector<size_t>> adjacency(triCount);

	if (isIndexed) {
		std::unordered_map<std::string, std::vector<size_t>> edgeMap;

		for (size_t i = 0; i < triCount; i++) {
			unsigned int i0, i1, i2;
			getTriIndices(i, i0, i1, i2);
			unsigned int triIndices[3] = { i0, i1, i2 };

			for (int j = 0; j < 3; j++) {
				unsigned int a = triIndices[j];
				unsigned int b = triIndices[(j + 1) % 3];
				std::string key = makeEdgeKey(a, b);

				auto& list = edgeMap[key];
				list.push_back(i);

				if (list.size() > 1) {
					size_t neighbor = list[0];
					adjacency[i].push_back(neighbor);
					adjacency[neighbor].push_back(i);
				}
			}
		}
	}

	// ============================================================
	// Step 3: Segment into Charts (Flood Fill)
	// ============================================================
	std::vector<uint8_t> visited(triCount, 0);
	std::vector<Chart> charts;
	const float thresholdDot = std::cos(angleThresholdDegrees * 3.14159265358979f / 180.0f);

	for (size_t i = 0; i < triCount; i++) {
		if (visited[i]) continue;

		Chart chart;
		chart.avgNormal = Vec3(0, 0, 0);

		std::stack<size_t> stack;
		stack.push(i);
		visited[i] = 1;

		// Flood Fill
		while (!stack.empty()) {
			size_t tIdx = stack.top();
			stack.pop();

			chart.triangles.push_back(tIdx);
			chart.avgNormal.add(normals[tIdx]);

			const Vec3& nA = normals[tIdx];
			const auto& neighbors = adjacency[tIdx];

			for (size_t nIdx : neighbors) {
				if (visited[nIdx]) continue;

				const Vec3& nB = normals[nIdx];
				if (nA.dot(nB) > thresholdDot) {
					visited[nIdx] = 1;
					stack.push(nIdx);
				}
			}
		}

		chart.avgNormal.normalize();
		charts.push_back(std::move(chart));
	}

	std::cout << "Generated " << charts.size() << " UV charts." << std::endl;

	// ============================================================
	// Step 4: Project Charts to 2D
	// ============================================================
	std::vector<ChartData> chartData;
	const float totalPadding = 0.02f;

	for (const auto& chart : charts) {
		// Calculate orthonormal basis from average normal
		Vec3 N = chart.avgNormal;
		Vec3 up(0, 1, 0);
		if (std::abs(N.dot(up)) > 0.9f) {
			up.set(1, 0, 0);
		}

		Vec3 tangent = up.cross(N);
		tangent.normalize();
		Vec3 bitangent = N.cross(tangent);
		bitangent.normalize();

		ChartData cData;
		cData.triangles = chart.triangles;

		float uMin = std::numeric_limits<float>::max();
		float uMax = std::numeric_limits<float>::lowest();
		float vMin = std::numeric_limits<float>::max();
		float vMax = std::numeric_limits<float>::lowest();

		// Project all vertices in this chart
		for (size_t tIdx : chart.triangles) {
			unsigned int i0, i1, i2;
			getTriIndices(tIdx, i0, i1, i2);

			Vec3 verts[3] = { getVertex(i0), getVertex(i1), getVertex(i2) };

			for (int k = 0; k < 3; k++) {
				float u = verts[k].dot(tangent);
				float v = verts[k].dot(bitangent);

				cData.localUVs.push_back(u);
				cData.localUVs.push_back(v);

				uMin = std::min(uMin, u);
				uMax = std::max(uMax, u);
				vMin = std::min(vMin, v);
				vMax = std::max(vMax, v);
			}
		}

		// Normalize local UVs to start from 0
		for (size_t k = 0; k < cData.localUVs.size(); k += 2) {
			cData.localUVs[k] -= uMin;
			cData.localUVs[k + 1] -= vMin;
		}

		cData.width = (uMax - uMin) + totalPadding;
		cData.height = (vMax - vMin) + totalPadding;

		// Handle degenerate charts
		if (cData.width < 0.001f) cData.width = 0.001f;
		if (cData.height < 0.001f) cData.height = 0.001f;

		chartData.push_back(std::move(cData));
	}

	// ============================================================
	// Step 5: Pack Charts (Simple Row-based Packing)
	// ============================================================
	// Sort by height (descending) for better packing
	std::sort(chartData.begin(), chartData.end(), [](const ChartData& a, const ChartData& b) {
		return a.height > b.height;
	});

	// Calculate total area for target width estimation
	float totalArea = 0;
	for (const auto& c : chartData) {
		totalArea += c.width * c.height;
	}
	float targetWidth = std::sqrt(totalArea) * 1.5f;

	float currentX = 0;
	float currentY = 0;
	float rowHeight = 0;
	float maxMapWidth = 0;

	for (auto& c : chartData) {
		// Start new row if current chart doesn't fit
		if (currentX + c.width > targetWidth) {
			currentX = 0;
			currentY += rowHeight;
			rowHeight = 0;
		}

		c.x = currentX;
		c.y = currentY;

		currentX += c.width;
		rowHeight = std::max(rowHeight, c.height);
		maxMapWidth = std::max(maxMapWidth, currentX);
	}

	float finalHeight = currentY + rowHeight;
	float finalScale = 1.0f / std::max(std::max(maxMapWidth, targetWidth), finalHeight);

	// ============================================================
	// Step 6: Output Final UVs
	// ============================================================
	// Output is per-triangle-vertex (non-indexed style)
	uvs.resize(triCount * 3 * 2);

	// We need to map from chartData (sorted) back to original triangle order
	// Create a mapping from triangle index to its UV data
	std::vector<std::pair<float, float>> triVertexUVs(triCount * 3);

	for (const auto& c : chartData) {
		for (size_t k = 0; k < c.triangles.size(); k++) {
			size_t tIdx = c.triangles[k];
			size_t luOffset = k * 6;

			for (int v = 0; v < 3; v++) {
				float u = (c.localUVs[luOffset + v * 2] + c.x) * finalScale;
				float vCoord = (c.localUVs[luOffset + v * 2 + 1] + c.y) * finalScale;

				// Clamp to [0, 1]
				u = std::max(0.0f, std::min(1.0f, u));
				vCoord = std::max(0.0f, std::min(1.0f, vCoord));

				triVertexUVs[tIdx * 3 + v] = { u, vCoord };
			}
		}
	}

	// Write to output array
	for (size_t i = 0; i < triCount * 3; i++) {
		uvs[i * 2] = triVertexUVs[i].first;
		uvs[i * 2 + 1] = triVertexUVs[i].second;
	}

	std::cout << "Smart UV Unwrap complete. Output: " << uvs.size() << " floats" << std::endl;
}
