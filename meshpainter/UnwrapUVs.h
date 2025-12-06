// UnwrapUVs.h
#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>

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

	Vec3& multiplyScalar(float s) {
		x *= s; y *= s; z *= s;
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

// Box projection UV unwrapping function
// Parameters:
//   positions: vertex positions array (x,y,z, x,y,z, ...)
//   positionCount: number of floats in positions array
//   indices: index array (can be nullptr if not indexed)
//   indexCount: number of indices (0 if not indexed)
//   uvs: output UV array (will be resized automatically)
void UnwrapUVs(
	const float* positions,
	size_t positionCount,
	const unsigned int* indices,
	size_t indexCount,
	std::vector<float>& uvs
);
