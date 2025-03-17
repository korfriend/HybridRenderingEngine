#pragma once
#include "CommonInclude.h"
#include <cmath>
#include <algorithm>
#include <limits>

#define SCU8(A) static_cast<uint8_t>(A)
#define SCU32(A) static_cast<uint32_t>(A)

#if __has_include("DirectXMath.h")
// In this case, DirectXMath is coming from Windows SDK.
//	It is better to use this on Windows as some Windows libraries could depend on the same
//	DirectXMath headers
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXCollision.h>
#else
// In this case, DirectXMath is coming from supplied source code
//	On platforms that don't have Windows SDK, the source code for DirectXMath is provided
//	as part of the engine utilities
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuninitialized"
#include "Utility/DirectXMath.h"
#include "Utility/DirectXPackedVector.h"
#include "Utility/DirectXCollision.h"
#pragma GCC diagnostic pop
#endif

using namespace DirectX;
using namespace DirectX::PackedVector;

// DOJO adds for supporting RHS
// DOJO TO DO...
#define RENDERING_RHS
#ifdef RENDERING_LHS
#define VZMatrixLookTo XMMatrixLookToLH
#define VZMatrixLookAt XMMatrixLookAtLH
#define VZMatrixOrthographic XMMatrixOrthographicLH
#define VZMatrixOrthographicOffCenter XMMatrixOrthographicOffCenterLH
#define VZMatrixPerspectiveFov XMMatrixPerspectiveFovLH
#else
#define VZMatrixLookTo XMMatrixLookToRH
#define VZMatrixLookAt XMMatrixLookAtRH
#define VZMatrixOrthographic XMMatrixOrthographicRH
#define VZMatrixOrthographicOffCenter XMMatrixOrthographicOffCenterRH
#define VZMatrixPerspectiveFov XMMatrixPerspectiveFovRH
#endif

namespace vz::math
{
	constexpr XMFLOAT4X4 IDENTITY_MATRIX = XMFLOAT4X4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
	constexpr XMFLOAT3X3 IDENTITY_MATRIX33 = XMFLOAT3X3(1, 0, 0, 0, 1, 0, 0, 0, 1);
	constexpr float PI = XM_PI;

	inline bool float_equal(float f1, float f2) {
		return (std::abs(f1 - f2) <= std::numeric_limits<float>::epsilon() * std::max(std::abs(f1), std::abs(f2)));
	}

	constexpr float saturate(float x)
	{
		return ::saturate(x);
	}

	inline float LengthSquared(const XMFLOAT2& v)
	{
		return v.x * v.x + v.y * v.y;
	}
	inline float LengthSquared(const XMFLOAT3& v)
	{
		return v.x * v.x + v.y * v.y + v.z * v.z;
	}
	inline float Length(const XMFLOAT2& v)
	{
		return std::sqrt(LengthSquared(v));
	}
	inline float Length(const XMFLOAT3& v)
	{
		return std::sqrt(LengthSquared(v));
	}
	inline float Distance(XMVECTOR v1, XMVECTOR v2)
	{
		return XMVectorGetX(XMVector3Length(XMVectorSubtract(v1, v2)));
	}
	inline float DistanceSquared(XMVECTOR v1, XMVECTOR v2)
	{
		return XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(v1, v2)));
	}
	inline float DistanceEstimated(const XMVECTOR& v1, const XMVECTOR& v2)
	{
		XMVECTOR vectorSub = XMVectorSubtract(v1, v2);
		XMVECTOR length = XMVector3LengthEst(vectorSub);

		float Distance = 0.0f;
		XMStoreFloat(&Distance, length);
		return Distance;
	}
	inline float Dot(const XMFLOAT2& v1, const XMFLOAT2& v2)
	{
		XMVECTOR vector1 = XMLoadFloat2(&v1);
		XMVECTOR vector2 = XMLoadFloat2(&v2);
		return XMVectorGetX(XMVector2Dot(vector1, vector2));
	}
	inline float Dot(const XMFLOAT3& v1, const XMFLOAT3& v2)
	{
		XMVECTOR vector1 = XMLoadFloat3(&v1);
		XMVECTOR vector2 = XMLoadFloat3(&v2);
		return XMVectorGetX(XMVector3Dot(vector1, vector2));
	}
	inline float Distance(const XMFLOAT2& v1, const XMFLOAT2& v2)
	{
		XMVECTOR vector1 = XMLoadFloat2(&v1);
		XMVECTOR vector2 = XMLoadFloat2(&v2);
		return XMVectorGetX(XMVector2Length(vector2 - vector1));
	}
	inline float Distance(const XMFLOAT3& v1, const XMFLOAT3& v2)
	{
		XMVECTOR vector1 = XMLoadFloat3(&v1);
		XMVECTOR vector2 = XMLoadFloat3(&v2);
		return Distance(vector1, vector2);
	}
	inline float DistanceSquared(const XMFLOAT2& v1, const XMFLOAT2& v2)
	{
		XMVECTOR vector1 = XMLoadFloat2(&v1);
		XMVECTOR vector2 = XMLoadFloat2(&v2);
		return XMVectorGetX(XMVector2LengthSq(vector2 - vector1));
	}
	inline float DistanceSquared(const XMFLOAT3& v1, const XMFLOAT3& v2)
	{
		XMVECTOR vector1 = XMLoadFloat3(&v1);
		XMVECTOR vector2 = XMLoadFloat3(&v2);
		return DistanceSquared(vector1, vector2);
	}
	inline float DistanceEstimated(const XMFLOAT2& v1, const XMFLOAT2& v2)
	{
		XMVECTOR vector1 = XMLoadFloat2(&v1);
		XMVECTOR vector2 = XMLoadFloat2(&v2);
		return XMVectorGetX(XMVector2LengthEst(vector2 - vector1));
	}
	inline float DistanceEstimated(const XMFLOAT3& v1, const XMFLOAT3& v2)
	{
		XMVECTOR vector1 = XMLoadFloat3(&v1);
		XMVECTOR vector2 = XMLoadFloat3(&v2);
		return DistanceEstimated(vector1, vector2);
	}
	inline XMVECTOR ClosestPointOnLine(const XMVECTOR& A, const XMVECTOR& B, const XMVECTOR& Point)
	{
		XMVECTOR AB = B - A;
		XMVECTOR T = XMVector3Dot(Point - A, AB) / XMVector3Dot(AB, AB);
		return A + T * AB;
	}
	inline XMVECTOR ClosestPointOnLineSegment(const XMVECTOR& A, const XMVECTOR& B, const XMVECTOR& Point)
	{
		XMVECTOR AB = B - A;
		XMVECTOR T = XMVector3Dot(Point - A, AB) / XMVector3Dot(AB, AB);
		return A + XMVectorSaturate(T) * AB;
	}
	constexpr XMFLOAT3 getVectorHalfWayPoint(const XMFLOAT3& a, const XMFLOAT3& b)
	{
		return XMFLOAT3((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, (a.z + b.z) * 0.5f);
	}
	inline XMVECTOR InverseLerp(XMVECTOR value1, XMVECTOR value2, XMVECTOR pos)
	{
		return (pos - value1) / (value2 - value1);
	}
	constexpr float InverseLerp(float value1, float value2, float pos)
	{
		return ::inverse_lerp(value1, value2, pos);
	}
	constexpr XMFLOAT2 InverseLerp(const XMFLOAT2& value1, const XMFLOAT2& value2, const XMFLOAT2& pos)
	{
		return XMFLOAT2(InverseLerp(value1.x, value2.x, pos.x), InverseLerp(value1.y, value2.y, pos.y));
	}
	constexpr XMFLOAT3 InverseLerp(const XMFLOAT3& value1, const XMFLOAT3& value2, const XMFLOAT3& pos)
	{
		return XMFLOAT3(InverseLerp(value1.x, value2.x, pos.x), InverseLerp(value1.y, value2.y, pos.y), InverseLerp(value1.z, value2.z, pos.z));
	}
	constexpr XMFLOAT4 InverseLerp(const XMFLOAT4& value1, const XMFLOAT4& value2, const XMFLOAT4& pos)
	{
		return XMFLOAT4(InverseLerp(value1.x, value2.x, pos.x), InverseLerp(value1.y, value2.y, pos.y), InverseLerp(value1.z, value2.z, pos.z), InverseLerp(value1.w, value2.w, pos.w));
	}
	inline XMVECTOR Lerp(XMVECTOR value1, XMVECTOR value2, XMVECTOR amount)
	{
		return value1 + (value2 - value1) * amount;
	}
	constexpr float Lerp(float value1, float value2, float amount)
	{
		return ::lerp(value1, value2, amount);
	}
	constexpr XMFLOAT2 Lerp(const XMFLOAT2& a, const XMFLOAT2& b, float i)
	{
		return XMFLOAT2(Lerp(a.x, b.x, i), Lerp(a.y, b.y, i));
	}
	constexpr XMFLOAT3 Lerp(const XMFLOAT3& a, const XMFLOAT3& b, float i)
	{
		return XMFLOAT3(Lerp(a.x, b.x, i), Lerp(a.y, b.y, i), Lerp(a.z, b.z, i));
	}
	constexpr XMFLOAT4 Lerp(const XMFLOAT4& a, const XMFLOAT4& b, float i)
	{
		return XMFLOAT4(Lerp(a.x, b.x, i), Lerp(a.y, b.y, i), Lerp(a.z, b.z, i), Lerp(a.w, b.w, i));
	}
	constexpr XMFLOAT2 Lerp(const XMFLOAT2& a, const XMFLOAT2& b, const XMFLOAT2& i)
	{
		return XMFLOAT2(Lerp(a.x, b.x, i.x), Lerp(a.y, b.y, i.y));
	}
	constexpr XMFLOAT3 Lerp(const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT3& i)
	{
		return XMFLOAT3(Lerp(a.x, b.x, i.x), Lerp(a.y, b.y, i.y), Lerp(a.z, b.z, i.z));
	}
	constexpr XMFLOAT4 Lerp(const XMFLOAT4& a, const XMFLOAT4& b, const XMFLOAT4& i)
	{
		return XMFLOAT4(Lerp(a.x, b.x, i.x), Lerp(a.y, b.y, i.y), Lerp(a.z, b.z, i.z), Lerp(a.w, b.w, i.w));
	}
	inline XMFLOAT4 Slerp(const XMFLOAT4& a, const XMFLOAT4& b, float i)
	{
		XMVECTOR _a = XMLoadFloat4(&a);
		XMVECTOR _b = XMLoadFloat4(&b);
		XMVECTOR result = XMQuaternionSlerp(_a, _b, i);
		XMFLOAT4 retVal;
		XMStoreFloat4(&retVal, result);
		return retVal;
	}
	constexpr XMFLOAT2 Max(const XMFLOAT2& a, const XMFLOAT2& b) {
		return XMFLOAT2(std::max(a.x, b.x), std::max(a.y, b.y));
	}
	constexpr XMFLOAT3 Max(const XMFLOAT3& a, const XMFLOAT3& b) {
		return XMFLOAT3(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z));
	}
	constexpr XMFLOAT4 Max(const XMFLOAT4& a, const XMFLOAT4& b) {
		return XMFLOAT4(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z), std::max(a.w, b.w));
	}
	constexpr XMFLOAT2 Min(const XMFLOAT2& a, const XMFLOAT2& b) {
		return XMFLOAT2(std::min(a.x, b.x), std::min(a.y, b.y));
	}
	constexpr XMFLOAT3 Min(const XMFLOAT3& a, const XMFLOAT3& b) {
		return XMFLOAT3(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z));
	}
	constexpr XMFLOAT4 Min(const XMFLOAT4& a, const XMFLOAT4& b) {
		return XMFLOAT4(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z), std::min(a.w, b.w));
	}
	constexpr XMFLOAT2 Abs(const XMFLOAT2& a) {
		return XMFLOAT2(std::abs(a.x), std::abs(a.y));
	}
	constexpr XMFLOAT3 Abs(const XMFLOAT3& a) {
		return XMFLOAT3(std::abs(a.x), std::abs(a.y), std::abs(a.z));
	}
	constexpr XMFLOAT4 Abs(const XMFLOAT4& a) {
		return XMFLOAT4(std::abs(a.x), std::abs(a.y), std::abs(a.z), std::abs(a.w));
	}
	constexpr float Clamp(float val, float min, float max)
	{
		return std::min(max, std::max(min, val));
	}
	constexpr XMFLOAT2 Clamp(XMFLOAT2 val, XMFLOAT2 min, XMFLOAT2 max)
	{
		XMFLOAT2 retval = val;
		retval.x = Clamp(retval.x, min.x, max.x);
		retval.y = Clamp(retval.y, min.y, max.y);
		return retval;
	}
	constexpr XMFLOAT3 Clamp(XMFLOAT3 val, XMFLOAT3 min, XMFLOAT3 max)
	{
		XMFLOAT3 retval = val;
		retval.x = Clamp(retval.x, min.x, max.x);
		retval.y = Clamp(retval.y, min.y, max.y);
		retval.z = Clamp(retval.z, min.z, max.z);
		return retval;
	}
	constexpr XMFLOAT4 Clamp(XMFLOAT4 val, XMFLOAT4 min, XMFLOAT4 max)
	{
		XMFLOAT4 retval = val;
		retval.x = Clamp(retval.x, min.x, max.x);
		retval.y = Clamp(retval.y, min.y, max.y);
		retval.z = Clamp(retval.z, min.z, max.z);
		retval.w = Clamp(retval.w, min.w, max.w);
		return retval;
	}
	constexpr float SmoothStep(float value1, float value2, float amount)
	{
		amount = Clamp((amount - value1) / (value2 - value1), 0.0f, 1.0f);
		return amount * amount * amount * (amount * (amount * 6 - 15) + 10);
	}
	constexpr bool Collision2D(const XMFLOAT2& hitBox1Pos, const XMFLOAT2& hitBox1Siz, const XMFLOAT2& hitBox2Pos, const XMFLOAT2& hitBox2Siz)
	{
		if (hitBox1Siz.x <= 0 || hitBox1Siz.y <= 0 || hitBox2Siz.x <= 0 || hitBox2Siz.y <= 0)
			return false;

		if (hitBox1Pos.x + hitBox1Siz.x < hitBox2Pos.x)
			return false;
		else if (hitBox1Pos.x > hitBox2Pos.x + hitBox2Siz.x)
			return false;
		else if (hitBox1Pos.y + hitBox1Siz.y < hitBox2Pos.y)
			return false;
		else if (hitBox1Pos.y > hitBox2Pos.y + hitBox2Siz.y)
			return false;

		return true;
	}
	constexpr uint32_t GetNextPowerOfTwo(uint32_t x)
	{
		--x;
		x |= x >> 1;
		x |= x >> 2;
		x |= x >> 4;
		x |= x >> 8;
		x |= x >> 16;
		return ++x;
	}
	constexpr uint64_t GetNextPowerOfTwo(uint64_t x)
	{
		--x;
		x |= x >> 1;
		x |= x >> 2;
		x |= x >> 4;
		x |= x >> 8;
		x |= x >> 16;
		x |= x >> 32u;
		return ++x;
	}

	// A uniform 2D random generator for hemisphere sampling: http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
	//	idx	: iteration index
	//	num	: number of iterations in total
	constexpr XMFLOAT2 Hammersley2D(uint32_t idx, uint32_t num) {
		uint32_t bits = idx;
		bits = (bits << 16u) | (bits >> 16u);
		bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
		bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
		bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
		bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
		const float radicalInverse_VdC = float(bits) * 2.3283064365386963e-10f; // / 0x100000000

		return XMFLOAT2(float(idx) / float(num), radicalInverse_VdC);
	}
	inline XMMATRIX GetTangentSpace(const XMFLOAT3& N)
	{
		// Choose a helper vector for the cross product
		XMVECTOR helper = std::abs(N.x) > 0.99 ? XMVectorSet(0, 0, 1, 0) : XMVectorSet(1, 0, 0, 0);

		// Generate vectors
		XMVECTOR normal = XMLoadFloat3(&N);
		XMVECTOR tangent = XMVector3Normalize(XMVector3Cross(normal, helper));
		XMVECTOR binormal = XMVector3Normalize(XMVector3Cross(normal, tangent));
		return XMMATRIX(tangent, binormal, normal, XMVectorSet(0, 0, 0, 1));
	}
	inline XMFLOAT3 HemispherePoint_Uniform(float u, float v)
	{
		float phi = v * 2 * PI;
		float cosTheta = 1 - u;
		float sinTheta = std::sqrt(1 - cosTheta * cosTheta);
		return XMFLOAT3(std::cos(phi) * sinTheta, std::sin(phi) * sinTheta, cosTheta);
	}
	inline XMFLOAT3 HemispherePoint_Cos(float u, float v)
	{
		float phi = v * 2 * PI;
		float cosTheta = std::sqrt(1 - u);
		float sinTheta = std::sqrt(1 - cosTheta * cosTheta);
		return XMFLOAT3(std::cos(phi) * sinTheta, std::sin(phi) * sinTheta, cosTheta);
	}

	// A, B, C: trangle vertices
	inline float TriangleArea(const XMVECTOR& A, const XMVECTOR& B, const XMVECTOR& C)
	{
		// Heron's formula:
		XMVECTOR a = XMVector3Length(B - A);
		XMVECTOR b = XMVector3Length(C - A);
		XMVECTOR c = XMVector3Length(C - B);
		XMVECTOR p = (a + b + c) * 0.5f;
		XMVECTOR areaSq = p * (p - a) * (p - b) * (p - c);
		float area;
		XMStoreFloat(&area, areaSq);
		area = std::sqrt(area);
		return area;
	}
	// a, b, c: trangle side lengths
	inline float TriangleArea(float a, float b, float c)
	{
		// Heron's formula:
		float p = (a + b + c) * 0.5f;
		return std::sqrt(p * (p - a) * (p - b) * (p - c));
	}

	constexpr float SphereSurfaceArea(float radius)
	{
		return 4 * PI * radius * radius;
	}
	constexpr float SphereVolume(float radius)
	{
		return 4.0f / 3.0f * PI * radius * radius * radius;
	}

	inline XMFLOAT3 GetCubicHermiteSplinePos(
		const XMFLOAT3& startPos,
		const XMFLOAT3& endPos,
		const XMFLOAT3& startTangent,
		const XMFLOAT3& endTangent,
		float atInterval
	)
	{
		float x, y, z, t;
		t = atInterval;
		x = (2 * t * t * t - 3 * t * t + 1) * startPos.x + (-2 * t * t * t + 3 * t * t) * endPos.x + (t * t * t - 2 * t * t + t) * startTangent.x + (t * t * t - t * t) * endTangent.x;
		y = (2 * t * t * t - 3 * t * t + 1) * startPos.y + (-2 * t * t * t + 3 * t * t) * endPos.y + (t * t * t - 2 * t * t + 1) * startTangent.y + (t * t * t - t * t) * endTangent.y;
		z = (2 * t * t * t - 3 * t * t + 1) * startPos.z + (-2 * t * t * t + 3 * t * t) * endPos.z + (t * t * t - 2 * t * t + 1) * startTangent.z + (t * t * t - t * t) * endTangent.z;
		return XMFLOAT3(x, y, z);
	}
	inline XMFLOAT3 GetQuadraticBezierPos(const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT3& c, float t) {
		float param0, param1, param2;
		param0 = sqr(1 - t);
		param1 = 2 * (1 - t) * t;
		param2 = sqr(t);
		float x = param0 * a.x + param1 * b.x + param2 * c.x;
		float y = param0 * a.y + param1 * b.y + param2 * c.y;
		float z = param0 * a.z + param1 * b.z + param2 * c.z;
		return XMFLOAT3(x, y, z);
	}
	inline XMFLOAT3 GetQuadraticBezierPos(const XMFLOAT4& a, const XMFLOAT4& b, const XMFLOAT4& c, float t) {
		return GetQuadraticBezierPos(XMFLOAT3(a.x, a.y, a.z), XMFLOAT3(b.x, b.y, b.z), XMFLOAT3(c.x, c.y, c.z), t);
	}
	inline XMVECTOR GetQuadraticBezierPos(const XMVECTOR& a, const XMVECTOR& b, const XMVECTOR& c, float t)
	{
		// XMVECTOR optimized version
		const float param0 = sqr(1 - t);
		const float param1 = 2 * (1 - t) * t;
		const float param2 = sqr(t);
		const XMVECTOR param = XMVectorSet(param0, param1, param2, 1);
		const XMMATRIX M = XMMATRIX(a, b, c, XMVectorSet(0, 0, 0, 1));
		return XMVector3TransformNormal(param, M);
	}

	// Centripetal Catmull-Rom avoids self intersections that can appear with XMVectorCatmullRom
	//	But it doesn't support the case when p0 == p1 or p2 == p3!
	//	This also supports tension to control curve smoothness
	//	Note: Catmull-Rom interpolates between p1 and p2 by value of t
	inline XMVECTOR XM_CALLCONV CatmullRomCentripetal(XMVECTOR p0, XMVECTOR p1, XMVECTOR p2, XMVECTOR p3, float t, float tension = 0.5f)
	{
		float alpha = 1.0f - tension;
		float t0 = 0.0f;
		float t1 = t0 + std::pow(DistanceEstimated(p0, p1), alpha);
		float t2 = t1 + std::pow(DistanceEstimated(p1, p2), alpha);
		float t3 = t2 + std::pow(DistanceEstimated(p2, p3), alpha);
		t = Lerp(t1, t2, t);
		float t1t0 = 1.0f / std::max(0.001f, t1 - t0);
		float t2t1 = 1.0f / std::max(0.001f, t2 - t1);
		float t3t2 = 1.0f / std::max(0.001f, t3 - t2);
		float t2t0 = 1.0f / std::max(0.001f, t2 - t0);
		float t3t1 = 1.0f / std::max(0.001f, t3 - t1);
		XMVECTOR A1 = (t1 - t) * t1t0 * p0 + (t - t0) * t1t0 * p1;
		XMVECTOR A2 = (t2 - t) * t2t1 * p1 + (t - t1) * t2t1 * p2;
		XMVECTOR A3 = (t3 - t) * t3t2 * p2 + (t - t2) * t3t2 * p3;
		XMVECTOR B1 = (t2 - t) * t2t0 * A1 + (t - t0) * t2t0 * A2;
		XMVECTOR B2 = (t3 - t) * t3t1 * A2 + (t - t1) * t3t1 * A3;
		XMVECTOR C = (t2 - t) * t2t1 * B1 + (t - t1) * t2t1 * B2;
		return C;
	}

	inline XMFLOAT3 QuaternionToRollPitchYaw(const XMFLOAT4& quaternion)
	{
		float roll = atan2f(2 * quaternion.x * quaternion.w - 2 * quaternion.y * quaternion.z, 1 - 2 * quaternion.x * quaternion.x - 2 * quaternion.z * quaternion.z);
		float pitch = atan2f(2 * quaternion.y * quaternion.w - 2 * quaternion.x * quaternion.z, 1 - 2 * quaternion.y * quaternion.y - 2 * quaternion.z * quaternion.z);
		float yaw = asinf(2 * quaternion.x * quaternion.y + 2 * quaternion.z * quaternion.w);

		return XMFLOAT3(roll, pitch, yaw);
	}

	inline XMVECTOR GetClosestPointToLine(const XMVECTOR& A, const XMVECTOR& B, const XMVECTOR& P, bool segmentClamp)
	{
		XMVECTOR AP_ = P - A;
		XMVECTOR AB_ = B - A;
		XMFLOAT3 AB, AP;
		XMStoreFloat3(&AB, AB_);
		XMStoreFloat3(&AP, AP_);
		float ab2 = AB.x * AB.x + AB.y * AB.y + AB.z * AB.z;
		float ap_ab = AP.x * AB.x + AP.y * AB.y + AP.z * AB.z;
		float t = ap_ab / ab2;
		if (segmentClamp)
		{
			if (t < 0.0f) t = 0.0f;
			else if (t > 1.0f) t = 1.0f;
		}
		XMVECTOR Closest = A + AB_ * t;
		return Closest;
	}
	inline float GetPointSegmentDistance(const XMVECTOR& point, const XMVECTOR& segmentA, const XMVECTOR& segmentB)
	{
		// Return minimum distance between line segment vw and point p
		const float l2 = XMVectorGetX(XMVector3LengthSq(segmentB - segmentA));  // i.e. |w-v|^2 -  avoid a sqrt
		if (l2 == 0.0) return Distance(point, segmentA);   // v == w case
		// Consider the line extending the segment, parameterized as v + t (w - v).
		// We find projection of point p onto the line. 
		// It falls where t = [(p-v) . (w-v)] / |w-v|^2
		// We clamp t from [0,1] to handle points outside the segment vw.
		const float t = std::max(0.0f, std::min(1.0f, XMVectorGetX(XMVector3Dot(point - segmentA, segmentB - segmentA)) / l2));
		const XMVECTOR projection = segmentA + t * (segmentB - segmentA);  // Projection falls on the segment
		return Distance(point, projection);
	}

	inline float GetPlanePointDistance(const XMVECTOR& planeOrigin, const XMVECTOR& planeNormal, const XMVECTOR& point)
	{
		return XMVectorGetX(XMVector3Dot(planeNormal, point - planeOrigin));
	}

	constexpr float RadiansToDegrees(float radians) { return radians / XM_PI * 180.0f; }
	constexpr float DegreesToRadians(float degrees) { return degrees / 180.0f * XM_PI; }

	inline float GetAngle(const XMFLOAT2& a, const XMFLOAT2& b)
	{
		float dot = a.x * b.x + a.y * b.y;      // dot product
		float det = a.x * b.y - a.y * b.x;		// determinant
		float angle = atan2f(det, dot);		// atan2(y, x) or atan2(sin, cos)
		if (angle < 0)
		{
			angle += XM_2PI;
		}
		return angle;
	}
	inline float GetAngle(XMVECTOR A, XMVECTOR B, XMVECTOR AXIS, float max)
	{
		float angle = XMVectorGetX(XMVector3AngleBetweenVectors(A, B));
		angle = std::min(angle, max);
		if (XMVectorGetX(XMVector3Dot(XMVector3Cross(A, B), AXIS)) < 0)
		{
			angle = XM_2PI - angle;
		}
		return angle;
	}
	inline float GetAngle(const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT3& axis, float max)
	{
		XMVECTOR A = XMLoadFloat3(&a);
		XMVECTOR B = XMLoadFloat3(&b);
		XMVECTOR AXIS = XMLoadFloat3(&axis);
		return GetAngle(A, B, AXIS, max);
	}
	inline void ConstructTriangleEquilateral(float radius, XMFLOAT4& A, XMFLOAT4& B, XMFLOAT4& C)
	{
		float deg = 0;
		float t = deg * XM_PI / 180.0f;
		A = XMFLOAT4(radius * cosf(t), radius * -sinf(t), 0, 1);
		deg += 120;
		t = deg * XM_PI / 180.0f;
		B = XMFLOAT4(radius * cosf(t), radius * -sinf(t), 0, 1);
		deg += 120;
		t = deg * XM_PI / 180.0f;
		C = XMFLOAT4(radius * cosf(t), radius * -sinf(t), 0, 1);
	}
	inline void GetBarycentric(const XMVECTOR& p, const XMVECTOR& a, const XMVECTOR& b, const XMVECTOR& c, float& u, float& v, float& w, bool clamp)
	{
		XMVECTOR v0 = b - a, v1 = c - a, v2 = p - a;
		float d00 = XMVectorGetX(XMVector3Dot(v0, v0));
		float d01 = XMVectorGetX(XMVector3Dot(v0, v1));
		float d11 = XMVectorGetX(XMVector3Dot(v1, v1));
		float d20 = XMVectorGetX(XMVector3Dot(v2, v0));
		float d21 = XMVectorGetX(XMVector3Dot(v2, v1));
		float denom = d00 * d11 - d01 * d01;
		v = (d11 * d20 - d01 * d21) / denom;
		w = (d00 * d21 - d01 * d20) / denom;
		u = 1.0f - v - w;

		if (clamp)
		{
			if (u < 0)
			{
				float t = XMVectorGetX(XMVector3Dot(p - b, c - b) / XMVector3Dot(c - b, c - b));
				t = saturate(t);
				u = 0.0f;
				v = 1.0f - t;
				w = t;
			}
			else if (v < 0)
			{
				float t = XMVectorGetX(XMVector3Dot(p - c, a - c) / XMVector3Dot(a - c, a - c));
				t = saturate(t);
				u = t;
				v = 0.0f;
				w = 1.0f - t;
			}
			else if (w < 0)
			{
				float t = XMVectorGetX(XMVector3Dot(p - a, b - a) / XMVector3Dot(b - a, b - a));
				t = saturate(t);
				u = 1.0f - t;
				v = t;
				w = 0.0f;
			}
		}
	}

	inline XMFLOAT3 GetForward(const XMFLOAT4X4& _m)
	{
		return XMFLOAT3(_m.m[2][0], _m.m[2][1], _m.m[2][2]);
	}
	inline XMFLOAT3 GetUp(const XMFLOAT4X4& _m)
	{
		return XMFLOAT3(_m.m[1][0], _m.m[1][1], _m.m[1][2]);
	}
	inline XMFLOAT3 GetRight(const XMFLOAT4X4& _m)
	{
		return XMFLOAT3(-_m.m[0][0], -_m.m[0][1], -_m.m[0][2]);
	}

	// Returns an element of a precomputed halton sequence. Specify which iteration to get with idx >= 0
	inline const XMFLOAT4& GetHaltonSequence(int idx)
	{
		static const XMFLOAT4 HALTON[] = {
			XMFLOAT4(0.5000000000f, 0.3333333333f, 0.2000000000f, 0.1428571429f),
			XMFLOAT4(0.2500000000f, 0.6666666667f, 0.4000000000f, 0.2857142857f),
			XMFLOAT4(0.7500000000f, 0.1111111111f, 0.6000000000f, 0.4285714286f),
			XMFLOAT4(0.1250000000f, 0.4444444444f, 0.8000000000f, 0.5714285714f),
			XMFLOAT4(0.6250000000f, 0.7777777778f, 0.0400000000f, 0.7142857143f),
			XMFLOAT4(0.3750000000f, 0.2222222222f, 0.2400000000f, 0.8571428571f),
			XMFLOAT4(0.8750000000f, 0.5555555556f, 0.4400000000f, 0.0204081633f),
			XMFLOAT4(0.0625000000f, 0.8888888889f, 0.6400000000f, 0.1632653061f),
			XMFLOAT4(0.5625000000f, 0.0370370370f, 0.8400000000f, 0.3061224490f),
			XMFLOAT4(0.3125000000f, 0.3703703704f, 0.0800000000f, 0.4489795918f),
			XMFLOAT4(0.8125000000f, 0.7037037037f, 0.2800000000f, 0.5918367347f),
			XMFLOAT4(0.1875000000f, 0.1481481481f, 0.4800000000f, 0.7346938776f),
			XMFLOAT4(0.6875000000f, 0.4814814815f, 0.6800000000f, 0.8775510204f),
			XMFLOAT4(0.4375000000f, 0.8148148148f, 0.8800000000f, 0.0408163265f),
			XMFLOAT4(0.9375000000f, 0.2592592593f, 0.1200000000f, 0.1836734694f),
			XMFLOAT4(0.0312500000f, 0.5925925926f, 0.3200000000f, 0.3265306122f),
			XMFLOAT4(0.5312500000f, 0.9259259259f, 0.5200000000f, 0.4693877551f),
			XMFLOAT4(0.2812500000f, 0.0740740741f, 0.7200000000f, 0.6122448980f),
			XMFLOAT4(0.7812500000f, 0.4074074074f, 0.9200000000f, 0.7551020408f),
			XMFLOAT4(0.1562500000f, 0.7407407407f, 0.1600000000f, 0.8979591837f),
			XMFLOAT4(0.6562500000f, 0.1851851852f, 0.3600000000f, 0.0612244898f),
			XMFLOAT4(0.4062500000f, 0.5185185185f, 0.5600000000f, 0.2040816327f),
			XMFLOAT4(0.9062500000f, 0.8518518519f, 0.7600000000f, 0.3469387755f),
			XMFLOAT4(0.0937500000f, 0.2962962963f, 0.9600000000f, 0.4897959184f),
			XMFLOAT4(0.5937500000f, 0.6296296296f, 0.0080000000f, 0.6326530612f),
			XMFLOAT4(0.3437500000f, 0.9629629630f, 0.2080000000f, 0.7755102041f),
			XMFLOAT4(0.8437500000f, 0.0123456790f, 0.4080000000f, 0.9183673469f),
			XMFLOAT4(0.2187500000f, 0.3456790123f, 0.6080000000f, 0.0816326531f),
			XMFLOAT4(0.7187500000f, 0.6790123457f, 0.8080000000f, 0.2244897959f),
			XMFLOAT4(0.4687500000f, 0.1234567901f, 0.0480000000f, 0.3673469388f),
			XMFLOAT4(0.9687500000f, 0.4567901235f, 0.2480000000f, 0.5102040816f),
			XMFLOAT4(0.0156250000f, 0.7901234568f, 0.4480000000f, 0.6530612245f),
			XMFLOAT4(0.5156250000f, 0.2345679012f, 0.6480000000f, 0.7959183673f),
			XMFLOAT4(0.2656250000f, 0.5679012346f, 0.8480000000f, 0.9387755102f),
			XMFLOAT4(0.7656250000f, 0.9012345679f, 0.0880000000f, 0.1020408163f),
			XMFLOAT4(0.1406250000f, 0.0493827160f, 0.2880000000f, 0.2448979592f),
			XMFLOAT4(0.6406250000f, 0.3827160494f, 0.4880000000f, 0.3877551020f),
			XMFLOAT4(0.3906250000f, 0.7160493827f, 0.6880000000f, 0.5306122449f),
			XMFLOAT4(0.8906250000f, 0.1604938272f, 0.8880000000f, 0.6734693878f),
			XMFLOAT4(0.0781250000f, 0.4938271605f, 0.1280000000f, 0.8163265306f),
			XMFLOAT4(0.5781250000f, 0.8271604938f, 0.3280000000f, 0.9591836735f),
			XMFLOAT4(0.3281250000f, 0.2716049383f, 0.5280000000f, 0.1224489796f),
			XMFLOAT4(0.8281250000f, 0.6049382716f, 0.7280000000f, 0.2653061224f),
			XMFLOAT4(0.2031250000f, 0.9382716049f, 0.9280000000f, 0.4081632653f),
			XMFLOAT4(0.7031250000f, 0.0864197531f, 0.1680000000f, 0.5510204082f),
			XMFLOAT4(0.4531250000f, 0.4197530864f, 0.3680000000f, 0.6938775510f),
			XMFLOAT4(0.9531250000f, 0.7530864198f, 0.5680000000f, 0.8367346939f),
			XMFLOAT4(0.0468750000f, 0.1975308642f, 0.7680000000f, 0.9795918367f),
			XMFLOAT4(0.5468750000f, 0.5308641975f, 0.9680000000f, 0.0029154519f),
			XMFLOAT4(0.2968750000f, 0.8641975309f, 0.0160000000f, 0.1457725948f),
			XMFLOAT4(0.7968750000f, 0.3086419753f, 0.2160000000f, 0.2886297376f),
			XMFLOAT4(0.1718750000f, 0.6419753086f, 0.4160000000f, 0.4314868805f),
			XMFLOAT4(0.6718750000f, 0.9753086420f, 0.6160000000f, 0.5743440233f),
			XMFLOAT4(0.4218750000f, 0.0246913580f, 0.8160000000f, 0.7172011662f),
			XMFLOAT4(0.9218750000f, 0.3580246914f, 0.0560000000f, 0.8600583090f),
			XMFLOAT4(0.1093750000f, 0.6913580247f, 0.2560000000f, 0.0233236152f),
			XMFLOAT4(0.6093750000f, 0.1358024691f, 0.4560000000f, 0.1661807580f),
			XMFLOAT4(0.3593750000f, 0.4691358025f, 0.6560000000f, 0.3090379009f),
			XMFLOAT4(0.8593750000f, 0.8024691358f, 0.8560000000f, 0.4518950437f),
			XMFLOAT4(0.2343750000f, 0.2469135802f, 0.0960000000f, 0.5947521866f),
			XMFLOAT4(0.7343750000f, 0.5802469136f, 0.2960000000f, 0.7376093294f),
			XMFLOAT4(0.4843750000f, 0.9135802469f, 0.4960000000f, 0.8804664723f),
			XMFLOAT4(0.9843750000f, 0.0617283951f, 0.6960000000f, 0.0437317784f),
			XMFLOAT4(0.0078125000f, 0.3950617284f, 0.8960000000f, 0.1865889213f),
			XMFLOAT4(0.5078125000f, 0.7283950617f, 0.1360000000f, 0.3294460641f),
			XMFLOAT4(0.2578125000f, 0.1728395062f, 0.3360000000f, 0.4723032070f),
			XMFLOAT4(0.7578125000f, 0.5061728395f, 0.5360000000f, 0.6151603499f),
			XMFLOAT4(0.1328125000f, 0.8395061728f, 0.7360000000f, 0.7580174927f),
			XMFLOAT4(0.6328125000f, 0.2839506173f, 0.9360000000f, 0.9008746356f),
			XMFLOAT4(0.3828125000f, 0.6172839506f, 0.1760000000f, 0.0641399417f),
			XMFLOAT4(0.8828125000f, 0.9506172840f, 0.3760000000f, 0.2069970845f),
			XMFLOAT4(0.0703125000f, 0.0987654321f, 0.5760000000f, 0.3498542274f),
			XMFLOAT4(0.5703125000f, 0.4320987654f, 0.7760000000f, 0.4927113703f),
			XMFLOAT4(0.3203125000f, 0.7654320988f, 0.9760000000f, 0.6355685131f),
			XMFLOAT4(0.8203125000f, 0.2098765432f, 0.0240000000f, 0.7784256560f),
			XMFLOAT4(0.1953125000f, 0.5432098765f, 0.2240000000f, 0.9212827988f),
			XMFLOAT4(0.6953125000f, 0.8765432099f, 0.4240000000f, 0.0845481050f),
			XMFLOAT4(0.4453125000f, 0.3209876543f, 0.6240000000f, 0.2274052478f),
			XMFLOAT4(0.9453125000f, 0.6543209877f, 0.8240000000f, 0.3702623907f),
			XMFLOAT4(0.0390625000f, 0.9876543210f, 0.0640000000f, 0.5131195335f),
			XMFLOAT4(0.5390625000f, 0.0041152263f, 0.2640000000f, 0.6559766764f),
			XMFLOAT4(0.2890625000f, 0.3374485597f, 0.4640000000f, 0.7988338192f),
			XMFLOAT4(0.7890625000f, 0.6707818930f, 0.6640000000f, 0.9416909621f),
			XMFLOAT4(0.1640625000f, 0.1152263374f, 0.8640000000f, 0.1049562682f),
			XMFLOAT4(0.6640625000f, 0.4485596708f, 0.1040000000f, 0.2478134111f),
			XMFLOAT4(0.4140625000f, 0.7818930041f, 0.3040000000f, 0.3906705539f),
			XMFLOAT4(0.9140625000f, 0.2263374486f, 0.5040000000f, 0.5335276968f),
			XMFLOAT4(0.1015625000f, 0.5596707819f, 0.7040000000f, 0.6763848397f),
			XMFLOAT4(0.6015625000f, 0.8930041152f, 0.9040000000f, 0.8192419825f),
			XMFLOAT4(0.3515625000f, 0.0411522634f, 0.1440000000f, 0.9620991254f),
			XMFLOAT4(0.8515625000f, 0.3744855967f, 0.3440000000f, 0.1253644315f),
			XMFLOAT4(0.2265625000f, 0.7078189300f, 0.5440000000f, 0.2682215743f),
			XMFLOAT4(0.7265625000f, 0.1522633745f, 0.7440000000f, 0.4110787172f),
			XMFLOAT4(0.4765625000f, 0.4855967078f, 0.9440000000f, 0.5539358601f),
			XMFLOAT4(0.9765625000f, 0.8189300412f, 0.1840000000f, 0.6967930029f),
			XMFLOAT4(0.0234375000f, 0.2633744856f, 0.3840000000f, 0.8396501458f),
			XMFLOAT4(0.5234375000f, 0.5967078189f, 0.5840000000f, 0.9825072886f),
			XMFLOAT4(0.2734375000f, 0.9300411523f, 0.7840000000f, 0.0058309038f),
			XMFLOAT4(0.7734375000f, 0.0781893004f, 0.9840000000f, 0.1486880466f),
			XMFLOAT4(0.1484375000f, 0.4115226337f, 0.0320000000f, 0.2915451895f),
			XMFLOAT4(0.6484375000f, 0.7448559671f, 0.2320000000f, 0.4344023324f),
			XMFLOAT4(0.3984375000f, 0.1893004115f, 0.4320000000f, 0.5772594752f),
			XMFLOAT4(0.8984375000f, 0.5226337449f, 0.6320000000f, 0.7201166181f),
			XMFLOAT4(0.0859375000f, 0.8559670782f, 0.8320000000f, 0.8629737609f),
			XMFLOAT4(0.5859375000f, 0.3004115226f, 0.0720000000f, 0.0262390671f),
			XMFLOAT4(0.3359375000f, 0.6337448560f, 0.2720000000f, 0.1690962099f),
			XMFLOAT4(0.8359375000f, 0.9670781893f, 0.4720000000f, 0.3119533528f),
			XMFLOAT4(0.2109375000f, 0.0164609053f, 0.6720000000f, 0.4548104956f),
			XMFLOAT4(0.7109375000f, 0.3497942387f, 0.8720000000f, 0.5976676385f),
			XMFLOAT4(0.4609375000f, 0.6831275720f, 0.1120000000f, 0.7405247813f),
			XMFLOAT4(0.9609375000f, 0.1275720165f, 0.3120000000f, 0.8833819242f),
			XMFLOAT4(0.0546875000f, 0.4609053498f, 0.5120000000f, 0.0466472303f),
			XMFLOAT4(0.5546875000f, 0.7942386831f, 0.7120000000f, 0.1895043732f),
			XMFLOAT4(0.3046875000f, 0.2386831276f, 0.9120000000f, 0.3323615160f),
			XMFLOAT4(0.8046875000f, 0.5720164609f, 0.1520000000f, 0.4752186589f),
			XMFLOAT4(0.1796875000f, 0.9053497942f, 0.3520000000f, 0.6180758017f),
			XMFLOAT4(0.6796875000f, 0.0534979424f, 0.5520000000f, 0.7609329446f),
			XMFLOAT4(0.4296875000f, 0.3868312757f, 0.7520000000f, 0.9037900875f),
			XMFLOAT4(0.9296875000f, 0.7201646091f, 0.9520000000f, 0.0670553936f),
			XMFLOAT4(0.1171875000f, 0.1646090535f, 0.1920000000f, 0.2099125364f),
			XMFLOAT4(0.6171875000f, 0.4979423868f, 0.3920000000f, 0.3527696793f),
			XMFLOAT4(0.3671875000f, 0.8312757202f, 0.5920000000f, 0.4956268222f),
			XMFLOAT4(0.8671875000f, 0.2757201646f, 0.7920000000f, 0.6384839650f),
			XMFLOAT4(0.2421875000f, 0.6090534979f, 0.9920000000f, 0.7813411079f),
			XMFLOAT4(0.7421875000f, 0.9423868313f, 0.0016000000f, 0.9241982507f),
			XMFLOAT4(0.4921875000f, 0.0905349794f, 0.2016000000f, 0.0874635569f),
			XMFLOAT4(0.9921875000f, 0.4238683128f, 0.4016000000f, 0.2303206997f),
			XMFLOAT4(0.0039062500f, 0.7572016461f, 0.6016000000f, 0.3731778426f),
			XMFLOAT4(0.5039062500f, 0.2016460905f, 0.8016000000f, 0.5160349854f),
			XMFLOAT4(0.2539062500f, 0.5349794239f, 0.0416000000f, 0.6588921283f),
			XMFLOAT4(0.7539062500f, 0.8683127572f, 0.2416000000f, 0.8017492711f),
			XMFLOAT4(0.1289062500f, 0.3127572016f, 0.4416000000f, 0.9446064140f),
			XMFLOAT4(0.6289062500f, 0.6460905350f, 0.6416000000f, 0.1078717201f),
			XMFLOAT4(0.3789062500f, 0.9794238683f, 0.8416000000f, 0.2507288630f),
			XMFLOAT4(0.8789062500f, 0.0288065844f, 0.0816000000f, 0.3935860058f),
			XMFLOAT4(0.0664062500f, 0.3621399177f, 0.2816000000f, 0.5364431487f),
			XMFLOAT4(0.5664062500f, 0.6954732510f, 0.4816000000f, 0.6793002915f),
			XMFLOAT4(0.3164062500f, 0.1399176955f, 0.6816000000f, 0.8221574344f),
			XMFLOAT4(0.8164062500f, 0.4732510288f, 0.8816000000f, 0.9650145773f),
			XMFLOAT4(0.1914062500f, 0.8065843621f, 0.1216000000f, 0.1282798834f),
			XMFLOAT4(0.6914062500f, 0.2510288066f, 0.3216000000f, 0.2711370262f),
			XMFLOAT4(0.4414062500f, 0.5843621399f, 0.5216000000f, 0.4139941691f),
			XMFLOAT4(0.9414062500f, 0.9176954733f, 0.7216000000f, 0.5568513120f),
			XMFLOAT4(0.0351562500f, 0.0658436214f, 0.9216000000f, 0.6997084548f),
			XMFLOAT4(0.5351562500f, 0.3991769547f, 0.1616000000f, 0.8425655977f),
			XMFLOAT4(0.2851562500f, 0.7325102881f, 0.3616000000f, 0.9854227405f),
			XMFLOAT4(0.7851562500f, 0.1769547325f, 0.5616000000f, 0.0087463557f),
			XMFLOAT4(0.1601562500f, 0.5102880658f, 0.7616000000f, 0.1516034985f),
			XMFLOAT4(0.6601562500f, 0.8436213992f, 0.9616000000f, 0.2944606414f),
			XMFLOAT4(0.4101562500f, 0.2880658436f, 0.0096000000f, 0.4373177843f),
			XMFLOAT4(0.9101562500f, 0.6213991770f, 0.2096000000f, 0.5801749271f),
			XMFLOAT4(0.0976562500f, 0.9547325103f, 0.4096000000f, 0.7230320700f),
			XMFLOAT4(0.5976562500f, 0.1028806584f, 0.6096000000f, 0.8658892128f),
			XMFLOAT4(0.3476562500f, 0.4362139918f, 0.8096000000f, 0.0291545190f),
			XMFLOAT4(0.8476562500f, 0.7695473251f, 0.0496000000f, 0.1720116618f),
			XMFLOAT4(0.2226562500f, 0.2139917695f, 0.2496000000f, 0.3148688047f),
			XMFLOAT4(0.7226562500f, 0.5473251029f, 0.4496000000f, 0.4577259475f),
			XMFLOAT4(0.4726562500f, 0.8806584362f, 0.6496000000f, 0.6005830904f),
			XMFLOAT4(0.9726562500f, 0.3251028807f, 0.8496000000f, 0.7434402332f),
			XMFLOAT4(0.0195312500f, 0.6584362140f, 0.0896000000f, 0.8862973761f),
			XMFLOAT4(0.5195312500f, 0.9917695473f, 0.2896000000f, 0.0495626822f),
			XMFLOAT4(0.2695312500f, 0.0082304527f, 0.4896000000f, 0.1924198251f),
			XMFLOAT4(0.7695312500f, 0.3415637860f, 0.6896000000f, 0.3352769679f),
			XMFLOAT4(0.1445312500f, 0.6748971193f, 0.8896000000f, 0.4781341108f),
			XMFLOAT4(0.6445312500f, 0.1193415638f, 0.1296000000f, 0.6209912536f),
			XMFLOAT4(0.3945312500f, 0.4526748971f, 0.3296000000f, 0.7638483965f),
			XMFLOAT4(0.8945312500f, 0.7860082305f, 0.5296000000f, 0.9067055394f),
			XMFLOAT4(0.0820312500f, 0.2304526749f, 0.7296000000f, 0.0699708455f),
			XMFLOAT4(0.5820312500f, 0.5637860082f, 0.9296000000f, 0.2128279883f),
			XMFLOAT4(0.3320312500f, 0.8971193416f, 0.1696000000f, 0.3556851312f),
			XMFLOAT4(0.8320312500f, 0.0452674897f, 0.3696000000f, 0.4985422741f),
			XMFLOAT4(0.2070312500f, 0.3786008230f, 0.5696000000f, 0.6413994169f),
			XMFLOAT4(0.7070312500f, 0.7119341564f, 0.7696000000f, 0.7842565598f),
			XMFLOAT4(0.4570312500f, 0.1563786008f, 0.9696000000f, 0.9271137026f),
			XMFLOAT4(0.9570312500f, 0.4897119342f, 0.0176000000f, 0.0903790087f),
			XMFLOAT4(0.0507812500f, 0.8230452675f, 0.2176000000f, 0.2332361516f),
			XMFLOAT4(0.5507812500f, 0.2674897119f, 0.4176000000f, 0.3760932945f),
			XMFLOAT4(0.3007812500f, 0.6008230453f, 0.6176000000f, 0.5189504373f),
			XMFLOAT4(0.8007812500f, 0.9341563786f, 0.8176000000f, 0.6618075802f),
			XMFLOAT4(0.1757812500f, 0.0823045267f, 0.0576000000f, 0.8046647230f),
			XMFLOAT4(0.6757812500f, 0.4156378601f, 0.2576000000f, 0.9475218659f),
			XMFLOAT4(0.4257812500f, 0.7489711934f, 0.4576000000f, 0.1107871720f),
			XMFLOAT4(0.9257812500f, 0.1934156379f, 0.6576000000f, 0.2536443149f),
			XMFLOAT4(0.1132812500f, 0.5267489712f, 0.8576000000f, 0.3965014577f),
			XMFLOAT4(0.6132812500f, 0.8600823045f, 0.0976000000f, 0.5393586006f),
			XMFLOAT4(0.3632812500f, 0.3045267490f, 0.2976000000f, 0.6822157434f),
			XMFLOAT4(0.8632812500f, 0.6378600823f, 0.4976000000f, 0.8250728863f),
			XMFLOAT4(0.2382812500f, 0.9711934156f, 0.6976000000f, 0.9679300292f),
			XMFLOAT4(0.7382812500f, 0.0205761317f, 0.8976000000f, 0.1311953353f),
			XMFLOAT4(0.4882812500f, 0.3539094650f, 0.1376000000f, 0.2740524781f),
			XMFLOAT4(0.9882812500f, 0.6872427984f, 0.3376000000f, 0.4169096210f),
			XMFLOAT4(0.0117187500f, 0.1316872428f, 0.5376000000f, 0.5597667638f),
			XMFLOAT4(0.5117187500f, 0.4650205761f, 0.7376000000f, 0.7026239067f),
			XMFLOAT4(0.2617187500f, 0.7983539095f, 0.9376000000f, 0.8454810496f),
			XMFLOAT4(0.7617187500f, 0.2427983539f, 0.1776000000f, 0.9883381924f),
			XMFLOAT4(0.1367187500f, 0.5761316872f, 0.3776000000f, 0.0116618076f),
			XMFLOAT4(0.6367187500f, 0.9094650206f, 0.5776000000f, 0.1545189504f),
			XMFLOAT4(0.3867187500f, 0.0576131687f, 0.7776000000f, 0.2973760933f),
			XMFLOAT4(0.8867187500f, 0.3909465021f, 0.9776000000f, 0.4402332362f),
			XMFLOAT4(0.0742187500f, 0.7242798354f, 0.0256000000f, 0.5830903790f),
			XMFLOAT4(0.5742187500f, 0.1687242798f, 0.2256000000f, 0.7259475219f),
			XMFLOAT4(0.3242187500f, 0.5020576132f, 0.4256000000f, 0.8688046647f),
			XMFLOAT4(0.8242187500f, 0.8353909465f, 0.6256000000f, 0.0320699708f),
			XMFLOAT4(0.1992187500f, 0.2798353909f, 0.8256000000f, 0.1749271137f),
			XMFLOAT4(0.6992187500f, 0.6131687243f, 0.0656000000f, 0.3177842566f),
			XMFLOAT4(0.4492187500f, 0.9465020576f, 0.2656000000f, 0.4606413994f),
			XMFLOAT4(0.9492187500f, 0.0946502058f, 0.4656000000f, 0.6034985423f),
			XMFLOAT4(0.0429687500f, 0.4279835391f, 0.6656000000f, 0.7463556851f),
			XMFLOAT4(0.5429687500f, 0.7613168724f, 0.8656000000f, 0.8892128280f),
			XMFLOAT4(0.2929687500f, 0.2057613169f, 0.1056000000f, 0.0524781341f),
			XMFLOAT4(0.7929687500f, 0.5390946502f, 0.3056000000f, 0.1953352770f),
			XMFLOAT4(0.1679687500f, 0.8724279835f, 0.5056000000f, 0.3381924198f),
			XMFLOAT4(0.6679687500f, 0.3168724280f, 0.7056000000f, 0.4810495627f),
			XMFLOAT4(0.4179687500f, 0.6502057613f, 0.9056000000f, 0.6239067055f),
			XMFLOAT4(0.9179687500f, 0.9835390947f, 0.1456000000f, 0.7667638484f),
			XMFLOAT4(0.1054687500f, 0.0329218107f, 0.3456000000f, 0.9096209913f),
			XMFLOAT4(0.6054687500f, 0.3662551440f, 0.5456000000f, 0.0728862974f),
			XMFLOAT4(0.3554687500f, 0.6995884774f, 0.7456000000f, 0.2157434402f),
			XMFLOAT4(0.8554687500f, 0.1440329218f, 0.9456000000f, 0.3586005831f),
			XMFLOAT4(0.2304687500f, 0.4773662551f, 0.1856000000f, 0.5014577259f),
			XMFLOAT4(0.7304687500f, 0.8106995885f, 0.3856000000f, 0.6443148688f),
			XMFLOAT4(0.4804687500f, 0.2551440329f, 0.5856000000f, 0.7871720117f),
			XMFLOAT4(0.9804687500f, 0.5884773663f, 0.7856000000f, 0.9300291545f),
			XMFLOAT4(0.0273437500f, 0.9218106996f, 0.9856000000f, 0.0932944606f),
			XMFLOAT4(0.5273437500f, 0.0699588477f, 0.0336000000f, 0.2361516035f),
			XMFLOAT4(0.2773437500f, 0.4032921811f, 0.2336000000f, 0.3790087464f),
			XMFLOAT4(0.7773437500f, 0.7366255144f, 0.4336000000f, 0.5218658892f),
			XMFLOAT4(0.1523437500f, 0.1810699588f, 0.6336000000f, 0.6647230321f),
			XMFLOAT4(0.6523437500f, 0.5144032922f, 0.8336000000f, 0.8075801749f),
			XMFLOAT4(0.4023437500f, 0.8477366255f, 0.0736000000f, 0.9504373178f),
			XMFLOAT4(0.9023437500f, 0.2921810700f, 0.2736000000f, 0.1137026239f),
			XMFLOAT4(0.0898437500f, 0.6255144033f, 0.4736000000f, 0.2565597668f),
			XMFLOAT4(0.5898437500f, 0.9588477366f, 0.6736000000f, 0.3994169096f),
			XMFLOAT4(0.3398437500f, 0.1069958848f, 0.8736000000f, 0.5422740525f),
			XMFLOAT4(0.8398437500f, 0.4403292181f, 0.1136000000f, 0.6851311953f),
			XMFLOAT4(0.2148437500f, 0.7736625514f, 0.3136000000f, 0.8279883382f),
			XMFLOAT4(0.7148437500f, 0.2181069959f, 0.5136000000f, 0.9708454810f),
			XMFLOAT4(0.4648437500f, 0.5514403292f, 0.7136000000f, 0.1341107872f),
			XMFLOAT4(0.9648437500f, 0.8847736626f, 0.9136000000f, 0.2769679300f),
			XMFLOAT4(0.0585937500f, 0.3292181070f, 0.1536000000f, 0.4198250729f),
			XMFLOAT4(0.5585937500f, 0.6625514403f, 0.3536000000f, 0.5626822157f),
			XMFLOAT4(0.3085937500f, 0.9958847737f, 0.5536000000f, 0.7055393586f),
			XMFLOAT4(0.8085937500f, 0.0013717421f, 0.7536000000f, 0.8483965015f),
			XMFLOAT4(0.1835937500f, 0.3347050754f, 0.9536000000f, 0.9912536443f),
			XMFLOAT4(0.6835937500f, 0.6680384088f, 0.1936000000f, 0.0145772595f),
			XMFLOAT4(0.4335937500f, 0.1124828532f, 0.3936000000f, 0.1574344023f),
			XMFLOAT4(0.9335937500f, 0.4458161866f, 0.5936000000f, 0.3002915452f),
			XMFLOAT4(0.1210937500f, 0.7791495199f, 0.7936000000f, 0.4431486880f),
			XMFLOAT4(0.6210937500f, 0.2235939643f, 0.9936000000f, 0.5860058309f),
			XMFLOAT4(0.3710937500f, 0.5569272977f, 0.0032000000f, 0.7288629738f),
			XMFLOAT4(0.8710937500f, 0.8902606310f, 0.2032000000f, 0.8717201166f),
			XMFLOAT4(0.2460937500f, 0.0384087791f, 0.4032000000f, 0.0349854227f),
			XMFLOAT4(0.7460937500f, 0.3717421125f, 0.6032000000f, 0.1778425656f),
			XMFLOAT4(0.4960937500f, 0.7050754458f, 0.8032000000f, 0.3206997085f),
			XMFLOAT4(0.9960937500f, 0.1495198903f, 0.0432000000f, 0.4635568513f),
			XMFLOAT4(0.0019531250f, 0.4828532236f, 0.2432000000f, 0.6064139942f),
		};
		return HALTON[idx % arraysize(HALTON)];
	}

	inline uint32_t CompressNormal(const XMFLOAT3& normal)
	{
		uint32_t retval = 0;

		retval |= (uint32_t)((uint8_t)(normal.x * 127.5f + 127.5f) << 0);
		retval |= (uint32_t)((uint8_t)(normal.y * 127.5f + 127.5f) << 8);
		retval |= (uint32_t)((uint8_t)(normal.z * 127.5f + 127.5f) << 16);

		return retval;
	}
	inline uint32_t CompressColor(const XMFLOAT3& color)
	{
		uint32_t retval = 0;

		retval |= (uint32_t)((uint8_t)(saturate(color.x) * 255.0f) << 0);
		retval |= (uint32_t)((uint8_t)(saturate(color.y) * 255.0f) << 8);
		retval |= (uint32_t)((uint8_t)(saturate(color.z) * 255.0f) << 16);

		return retval;
	}
	inline uint32_t CompressColor(const XMFLOAT4& color)
	{
		uint32_t retval = 0;

		retval |= (uint32_t)((uint8_t)(saturate(color.x) * 255.0f) << 0);
		retval |= (uint32_t)((uint8_t)(saturate(color.y) * 255.0f) << 8);
		retval |= (uint32_t)((uint8_t)(saturate(color.z) * 255.0f) << 16);
		retval |= (uint32_t)((uint8_t)(saturate(color.w) * 255.0f) << 24);

		return retval;
	}
	inline XMFLOAT3 Unpack_R11G11B10_FLOAT(uint32_t value)
	{
		XMFLOAT3PK pk;
		pk.v = value;
		XMVECTOR V = XMLoadFloat3PK(&pk);
		XMFLOAT3 result;
		XMStoreFloat3(&result, V);
		return result;
	}
	inline uint32_t Pack_R11G11B10_FLOAT(const XMFLOAT3& color)
	{
		XMFLOAT3PK pk;
		XMStoreFloat3PK(&pk, XMLoadFloat3(&color));
		return pk.v;
	}
	inline XMFLOAT3 Unpack_R9G9B9E5_SHAREDEXP(uint32_t value)
	{
		XMFLOAT3SE se;
		se.v = value;
		XMVECTOR V = XMLoadFloat3SE(&se);
		XMFLOAT3 result;
		XMStoreFloat3(&result, V);
		return result;
	}
	inline uint32_t Pack_R9G9B9E5_SHAREDEXP(const XMFLOAT3& value)
	{
		XMFLOAT3SE se;
		XMStoreFloat3SE(&se, XMLoadFloat3(&value));
		return se;
	}

	inline uint32_t pack_half2(float x, float y)
	{
		return (uint32_t)XMConvertFloatToHalf(x) | ((uint32_t)XMConvertFloatToHalf(y) << 16u);
	}
	inline uint32_t pack_half2(const XMFLOAT2& value)
	{
		return pack_half2(value.x, value.y);
	}
	inline XMUINT2 pack_half3(float x, float y, float z)
	{
		return XMUINT2(
			(uint32_t)XMConvertFloatToHalf(x) | ((uint32_t)XMConvertFloatToHalf(y) << 16u),
			(uint32_t)XMConvertFloatToHalf(z)
		);
	}
	inline XMUINT2 pack_half3(const XMFLOAT3& value)
	{
		return pack_half3(value.x, value.y, value.z);
	}
	inline XMUINT2 pack_half4(float x, float y, float z, float w)
	{
		return XMUINT2(
			(uint32_t)XMConvertFloatToHalf(x) | ((uint32_t)XMConvertFloatToHalf(y) << 16u),
			(uint32_t)XMConvertFloatToHalf(z) | ((uint32_t)XMConvertFloatToHalf(w) << 16u)
		);
	}
	inline XMUINT2 pack_half4(const XMFLOAT4& value)
	{
		return pack_half4(value.x, value.y, value.z, value.w);
	}



	//-----------------------------------------------------------------------------
	// Compute the intersection of a ray (Origin, Direction) with a triangle
	// (V0, V1, V2).  Return true if there is an intersection and also set *pDist
	// to the distance along the ray to the intersection.
	//
	// The algorithm is based on Moller, Tomas and Trumbore, "Fast, Minimum Storage
	// Ray-Triangle Intersection", Journal of Graphics Tools, vol. 2, no. 1,
	// pp 21-28, 1997.
	//
	//	Modified for WickedEngine to return barycentrics and support TMin, TMax
	//-----------------------------------------------------------------------------
	_Use_decl_annotations_
		inline bool XM_CALLCONV RayTriangleIntersects(
			FXMVECTOR Origin,
			FXMVECTOR Direction,
			FXMVECTOR V0,
			GXMVECTOR V1,
			HXMVECTOR V2,
			float& Dist,
			XMFLOAT2& bary,
			float TMin = 0,
			float TMax = std::numeric_limits<float>::max()
		)
	{
		const XMVECTOR rayEpsilon = XMVectorSet(1e-20f, 1e-20f, 1e-20f, 1e-20f);
		const XMVECTOR rayNegEpsilon = XMVectorSet(-1e-20f, -1e-20f, -1e-20f, -1e-20f);

		XMVECTOR Zero = XMVectorZero();

		XMVECTOR e1 = XMVectorSubtract(V1, V0);
		XMVECTOR e2 = XMVectorSubtract(V2, V0);

		// p = Direction ^ e2;
		XMVECTOR p = XMVector3Cross(Direction, e2);

		// det = e1 * p;
		XMVECTOR det = XMVector3Dot(e1, p);

		XMVECTOR u, v, t;

		if (XMVector3GreaterOrEqual(det, rayEpsilon))
		{
			// Determinate is positive (front side of the triangle).
			XMVECTOR s = XMVectorSubtract(Origin, V0);

			// u = s * p;
			u = XMVector3Dot(s, p);

			XMVECTOR NoIntersection = XMVectorLess(u, Zero);
			NoIntersection = XMVectorOrInt(NoIntersection, XMVectorGreater(u, det));

			// q = s ^ e1;
			XMVECTOR q = XMVector3Cross(s, e1);

			// v = Direction * q;
			v = XMVector3Dot(Direction, q);

			NoIntersection = XMVectorOrInt(NoIntersection, XMVectorLess(v, Zero));
			NoIntersection = XMVectorOrInt(NoIntersection, XMVectorGreater(XMVectorAdd(u, v), det));

			// t = e2 * q;
			t = XMVector3Dot(e2, q);

			NoIntersection = XMVectorOrInt(NoIntersection, XMVectorLess(t, Zero));

			if (XMVector4EqualInt(NoIntersection, XMVectorTrueInt()))
			{
				Dist = 0.f;
				return false;
			}
		}
		else if (XMVector3LessOrEqual(det, rayNegEpsilon))
		{
			// Determinate is negative (back side of the triangle).
			XMVECTOR s = XMVectorSubtract(Origin, V0);

			// u = s * p;
			u = XMVector3Dot(s, p);

			XMVECTOR NoIntersection = XMVectorGreater(u, Zero);
			NoIntersection = XMVectorOrInt(NoIntersection, XMVectorLess(u, det));

			// q = s ^ e1;
			XMVECTOR q = XMVector3Cross(s, e1);

			// v = Direction * q;
			v = XMVector3Dot(Direction, q);

			NoIntersection = XMVectorOrInt(NoIntersection, XMVectorGreater(v, Zero));
			NoIntersection = XMVectorOrInt(NoIntersection, XMVectorLess(XMVectorAdd(u, v), det));

			// t = e2 * q;
			t = XMVector3Dot(e2, q);

			NoIntersection = XMVectorOrInt(NoIntersection, XMVectorGreater(t, Zero));

			if (XMVector4EqualInt(NoIntersection, XMVectorTrueInt()))
			{
				Dist = 0.f;
				return false;
			}
		}
		else
		{
			// Parallel ray.
			Dist = 0.f;
			return false;
		}

		t = XMVectorDivide(t, det);

		const XMVECTOR invdet = XMVectorReciprocal(det);
		XMStoreFloat(&bary.x, u * invdet);
		XMStoreFloat(&bary.y, v * invdet);

		// Store the x-component to *pDist
		XMStoreFloat(&Dist, t);

		if (Dist > TMax || Dist < TMin)
			return false;

		return true;
	}
};

