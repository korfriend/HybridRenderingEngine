// UnwrapUVs.h
#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <string>
#include <cstdint>

struct Vec3 {
	float x, y, z;

	Vec3() : x(0), y(0), z(0) {}
	Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

	void set(float x_, float y_, float z_) {
		x = x_; y = y_; z = z_;
	}

	Vec3& sub(const Vec3& v) {
		x -= v.x; y -= v.y; z -= v.z;
		return *this;
	}

	Vec3& add(const Vec3& v) {
		x += v.x; y += v.y; z += v.z;
		return *this;
	}

	Vec3& multiplyScalar(float s) {
		x *= s; y *= s; z *= s;
		return *this;
	}

	Vec3& divideScalar(float s) {
		if (s != 0) { x /= s; y /= s; z /= s; }
		return *this;
	}

	float dot(const Vec3& v) const {
		return x * v.x + y * v.y + z * v.z;
	}

	Vec3 cross(const Vec3& v) const {
		return Vec3(
			y * v.z - z * v.y,
			z * v.x - x * v.z,
			x * v.y - y * v.x
		);
	}

	float length() const {
		return std::sqrt(x * x + y * y + z * z);
	}

	Vec3& normalize() {
		float len = length();
		if (len > 0) divideScalar(len);
		return *this;
	}

	Vec3 clone() const {
		return Vec3(x, y, z);
	}
};

struct Box3 {
	Vec3 min, max;

	Box3() {
		makeEmpty();
	}

	void makeEmpty() {
		min.x = min.y = min.z = std::numeric_limits<float>::max();
		max.x = max.y = max.z = std::numeric_limits<float>::lowest();
	}

	void expandByPoint(const Vec3& point) {
		min.x = std::min(min.x, point.x);
		min.y = std::min(min.y, point.y);
		min.z = std::min(min.z, point.z);
		max.x = std::max(max.x, point.x);
		max.y = std::max(max.y, point.y);
		max.z = std::max(max.z, point.z);
	}

	Vec3 getSize() const {
		return Vec3(max.x - min.x, max.y - min.y, max.z - min.z);
	}
};

// Angle-based Chart Generation UV Unwrapping
//
// Algorithm:
// 1. Segmentation: Groups triangles into "Charts" based on surface angle (45 degree threshold)
// 2. Flattening: Each chart is projected flat based on its average direction
// 3. Packing: Charts are efficiently packed into UV space
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
//   angleThresholdDegrees: angle threshold for chart segmentation (default: 45)
void UnwrapUVs(
	const float* positions,
	size_t positionCount,
	const unsigned int* indices,
	size_t indexCount,
	std::vector<float>& uvs,
	float angleThresholdDegrees = 45.0f
);
