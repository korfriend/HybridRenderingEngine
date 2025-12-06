// UnwrapUVs.cpp
#include "UnwrapUVs.h"
#include <iostream>

void UnwrapUVs(
	const float* positions,
	size_t positionCount,
	const unsigned int* indices,
	size_t indexCount,
	std::vector<float>& uvs
) {
	size_t triCount = indices ? (indexCount / 3) : (positionCount / 9);

	std::cout << "Autogenerating UV map... " << triCount << " triangles" << std::endl;

	Box3 box;
	Vec3 v0, v1, v2;

	// Lambda to get triangle vertices
	auto getTriangle = [&](size_t i) {
		if (indices) {
			size_t vi = i * 3;
			size_t ia = indices[vi] * 3;
			size_t ib = indices[vi + 1] * 3;
			size_t ic = indices[vi + 2] * 3;
			v0.set(positions[ia], positions[ia + 1], positions[ia + 2]);
			v1.set(positions[ib], positions[ib + 1], positions[ib + 2]);
			v2.set(positions[ic], positions[ic + 1], positions[ic + 2]);
		}
		else {
			size_t vi = i * 9;
			v0.set(positions[vi], positions[vi + 1], positions[vi + 2]);
			v1.set(positions[vi + 3], positions[vi + 4], positions[vi + 5]);
			v2.set(positions[vi + 6], positions[vi + 7], positions[vi + 8]);
		}
		};

	// Allocate UV array (2 floats per vertex)
	size_t vertexCount = positionCount / 3;
	uvs.resize(vertexCount * 2);

	// Compute bounding box of all vertices
	box.makeEmpty();
	for (size_t i = 0; i < positionCount; i += 3) {
		v0.set(positions[i], positions[i + 1], positions[i + 2]);
		box.expandByPoint(v0);
	}

	Vec3 sz = box.getSize();
	Vec3 bmin = box.min.clone();
	float maxAxis = std::max(sz.x, std::max(sz.y, sz.z));
	float imaxax = 1.0f / maxAxis;

	// Generate UVs for each triangle
	for (size_t i = 0, w = 0; i < triCount; i++, w += 6) {
		getTriangle(i);

		// Normalize vertices to 0-1 range based on bounding box
		v0.sub(bmin).multiplyScalar(imaxax);
		v1.sub(bmin).multiplyScalar(imaxax);
		v2.sub(bmin).multiplyScalar(imaxax);

		// Use X and Y coordinates as UV (box projection)
		if (indices) {
			size_t vi = i * 3;
			size_t ia = indices[vi];
			size_t ib = indices[vi + 1];
			size_t ic = indices[vi + 2];

			uvs[ia * 2] = v0.x;
			uvs[ia * 2 + 1] = v0.y;
			uvs[ib * 2] = v1.x;
			uvs[ib * 2 + 1] = v1.y;
			uvs[ic * 2] = v2.x;
			uvs[ic * 2 + 1] = v2.y;
		}
		else {
			uvs[w] = v0.x;
			uvs[w + 1] = v0.y;
			uvs[w + 2] = v1.x;
			uvs[w + 3] = v1.y;
			uvs[w + 4] = v2.x;
			uvs[w + 5] = v2.y;
		}
	}
}
