// UnwrapUVs.cpp
// Angle-based Chart Generation UV Unwrapping
// Ported from JavaScript implementation

#include "UnwrapUVs.h"
#include "vzm2/Backlog.h"
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

	vzlog("Starting Smart UV Unwrap... %d triangles", (int)triCount);

	// Debug: Track unmapped triangles
	int totalMappedTriangles = 0;

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
	int degenerateTriangles = 0;

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
		float nLength = n.length();

		// Check for degenerate triangle (zero area)
		if (nLength < 1e-6f) {
			degenerateTriangles++;
			// Use fallback normal (Z-up)
			n.set(0, 0, 1);
		} else {
			n.normalize();
		}

		normals[i] = n;
	}

	if (degenerateTriangles > 0) {
		vzlog("WARNING: Found %d degenerate triangles (zero area), using fallback normals", degenerateTriangles);
	}

	// ============================================================
	// Step 2: Build Adjacency Graph
	// ============================================================
	std::vector<std::vector<size_t>> adjacency(triCount);

	// Build adjacency for both indexed and non-indexed meshes
	std::unordered_map<std::string, std::vector<size_t>> edgeMap;

	for (size_t i = 0; i < triCount; i++) {
		unsigned int i0, i1, i2;
		getTriIndices(i, i0, i1, i2);

		// For non-indexed meshes, vertices are unique per triangle
		// but we still need to find adjacency by position matching
		Vec3 v0 = getVertex(i0);
		Vec3 v1 = getVertex(i1);
		Vec3 v2 = getVertex(i2);

		// Create edge keys based on vertex positions (for non-indexed)
		// or vertex indices (for indexed)
		auto makePositionKey = [](const Vec3& a, const Vec3& b) -> std::string {
			// Use position-based key for non-indexed meshes
			// Quantize to epsilon grid to handle floating point precision
			const float epsilon = 1e-5f;
			auto quantize = [epsilon](float v) -> int64_t {
				return (int64_t)std::round(v / epsilon);
			};

			int64_t ax = quantize(a.x), ay = quantize(a.y), az = quantize(a.z);
			int64_t bx = quantize(b.x), by = quantize(b.y), bz = quantize(b.z);

			// Ensure consistent ordering
			if (ax > bx || (ax == bx && ay > by) || (ax == bx && ay == by && az > bz)) {
				std::swap(ax, bx);
				std::swap(ay, by);
				std::swap(az, bz);
			}

			char buf[128];
			snprintf(buf, sizeof(buf), "%lld_%lld_%lld_%lld_%lld_%lld",
				(long long)ax, (long long)ay, (long long)az,
				(long long)bx, (long long)by, (long long)bz);
			return std::string(buf);
		};

		Vec3 verts[3] = { v0, v1, v2 };
		for (int j = 0; j < 3; j++) {
			std::string key;
			if (isIndexed) {
				// Use vertex indices for indexed meshes
				unsigned int triIndices[3] = { i0, i1, i2 };
				unsigned int a = triIndices[j];
				unsigned int b = triIndices[(j + 1) % 3];
				key = makeEdgeKey(a, b);
			} else {
				// Use vertex positions for non-indexed meshes
				key = makePositionKey(verts[j], verts[(j + 1) % 3]);
			}

			auto& list = edgeMap[key];
			list.push_back(i);

			if (list.size() > 1) {
				size_t neighbor = list[0];
				adjacency[i].push_back(neighbor);
				adjacency[neighbor].push_back(i);
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

		// Store the seed normal (first triangle's normal) as the chart reference
		Vec3 seedNormal = normals[i];
		seedNormal.normalize();

		// Flood Fill
		while (!stack.empty()) {
			size_t tIdx = stack.top();
			stack.pop();

			chart.triangles.push_back(tIdx);
			chart.avgNormal.add(normals[tIdx]);

			const auto& neighbors = adjacency[tIdx];

			for (size_t nIdx : neighbors) {
				if (visited[nIdx]) continue;

				// Compare neighbor's normal with the chart's SEED normal (not just adjacent triangle)
				// This prevents gradual drift that allows opposite-facing triangles in the same chart
				const Vec3& nB = normals[nIdx];
				if (seedNormal.dot(nB) > thresholdDot) {
					visited[nIdx] = 1;
					stack.push(nIdx);
				}
			}
		}

		chart.avgNormal.normalize();
		charts.push_back(std::move(chart));
	}

	// Count total triangles in all charts
	int chartedTriangles = 0;
	for (const auto& chart : charts) {
		chartedTriangles += (int)chart.triangles.size();
	}

	vzlog("Generated... %d UV charts.", (int)charts.size());
	vzlog("Chart coverage: %d/%d triangles mapped", chartedTriangles, (int)triCount);

	if (chartedTriangles != triCount) {
		vzlog("WARNING: %d triangles NOT MAPPED!", (int)triCount - chartedTriangles);
	}


	// ============================================================
	// Step 4: Project Charts to 2D
	// ============================================================
	std::vector<ChartData> chartData;
	const float totalPadding = 0.02f;

	int degenerateCharts = 0;
	for (const auto& chart : charts) {
		// Calculate orthonormal basis from average normal
		Vec3 N = chart.avgNormal;
		N.normalize();  // IMPORTANT: Normalize average normal!

		// Check for degenerate chart (zero or near-zero normal)
		float normalLength = chart.avgNormal.length();
		if (normalLength < 1e-6f) {
			vzlog("WARNING: Degenerate chart with %d triangles (zero normal), using fallback XY projection",
				(int)chart.triangles.size());
			degenerateCharts++;
			// Use fallback: project to XY plane
			N.set(0, 0, 1);
		}

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

		// Check for degenerate projection (all vertices projected to a line or point)
		float chartWidth = uMax - uMin;
		float chartHeight = vMax - vMin;

		if (chartWidth < 1e-5f || chartHeight < 1e-5f) {
			vzlog("WARNING: Chart %d has degenerate projection (w=%.6f, h=%.6f), %d triangles",
				(int)chartData.size(), chartWidth, chartHeight, (int)chart.triangles.size());
			// Expand to minimum size
			if (chartWidth < 1e-5f) {
				uMax = uMin + 0.01f;
			}
			if (chartHeight < 1e-5f) {
				vMax = vMin + 0.01f;
			}
		}

		// Normalize local UVs to start from 0
		for (size_t k = 0; k < cData.localUVs.size(); k += 2) {
			cData.localUVs[k] -= uMin;
			cData.localUVs[k + 1] -= vMin;
		}

		cData.width = (uMax - uMin) + totalPadding;
		cData.height = (vMax - vMin) + totalPadding;

		// Handle degenerate charts (should not happen after above fix)
		if (cData.width < 0.001f) cData.width = 0.001f;
		if (cData.height < 0.001f) cData.height = 0.001f;

		chartData.push_back(std::move(cData));
	}

	if (degenerateCharts > 0) {
		vzlog("Processed %d degenerate charts with fallback projection", degenerateCharts);
	}

	// ============================================================
	// Step 5: Pack Charts (Simple Row-based Packing)
	// ============================================================
	// Sort by height (descending) for better packing
	std::sort(chartData.begin(), chartData.end(), [](const ChartData& a, const ChartData& b) {
		return a.height > b.height;
	});

	// Chart spacing in local UV space (will be scaled to final UV space)
	// This ensures 2-3 texel gap between charts in final texture
	const float chartGap = 1.0f;  // Gap between charts (local space)

	// Calculate total area including gaps for target width estimation
	float totalArea = 0;
	for (const auto& c : chartData) {
		totalArea += (c.width + chartGap) * (c.height + chartGap);
	}
	float targetWidth = std::sqrt(totalArea) * 1.5f;

	float currentX = 0;
	float currentY = 0;
	float rowHeight = 0;
	float maxMapWidth = 0;

	for (auto& c : chartData) {
		// Start new row if current chart doesn't fit (including gap)
		if (currentX + c.width + chartGap > targetWidth) {
			currentX = 0;
			currentY += rowHeight + chartGap;  // Add gap between rows
			rowHeight = 0;
		}

		c.x = currentX;
		c.y = currentY;

		currentX += c.width + chartGap;  // Add gap after each chart
		rowHeight = std::max(rowHeight, c.height);
		maxMapWidth = std::max(maxMapWidth, currentX);
	}

	// Calculate final dimensions (maxMapWidth already includes gaps from last chart in row)
	// But we need to remove the trailing gap from the last chart
	float finalWidth = maxMapWidth - chartGap;  // Remove trailing gap
	float finalHeight = currentY + rowHeight;   // Height doesn't have trailing gap issue

	// Add safety margin (5%) to ensure all UVs fit within [0,1]
	float safetyMargin = 1.05f;
	float finalScale = 1.0f / (std::max(finalWidth, finalHeight) * safetyMargin);

	vzlog("UV Packing: finalWidth=%.3f, finalHeight=%.3f, finalScale=%.3f (with %.0f%% margin)",
		finalWidth, finalHeight, finalScale, (safetyMargin - 1.0f) * 100.0f);

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

				// Debug: Check if clamping is needed (indicates packing overflow)
				static bool logged = false;
				if (!logged && (u < 0.0f || u > 1.0f || vCoord < 0.0f || vCoord > 1.0f)) {
					vzlog("WARNING: UV out of [0,1] before clamp: u=%.3f, v=%.3f (Chart at x=%.3f, y=%.3f)",
						u, vCoord, c.x, c.y);
					logged = true;  // Only log once to avoid spam
				}

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

	vzlog("Smart UV Unwrap complete. Output: %d floats", (int)uvs.size());
}
