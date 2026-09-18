#pragma once

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
// Pointer aliases used by every function below: d / f = double / float element type, 3 = vec3, 44 = mat4x4.
typedef glm::dvec3* d3p;
typedef const glm::dvec3* const_d3p;
typedef glm::dmat4x4* d44p;
typedef const glm::dmat4x4* const_d44p;
typedef glm::fvec3* f3p;
typedef const glm::fvec3* const_f3p;
typedef glm::fmat4x4* f44p;
typedef const glm::fmat4x4* const_f44p;

#include <chrono>

// C linkage (undecorated name), defined in CommonUnits' VimHelpers.cpp and exported from CommonUnits.dll only.
// A consumer TU sees a declaration without a body and resolves the call through CommonUnits.lib; the
// dllexport is inert there.
#define __staticinline extern "C" __declspec(dllexport) inline

namespace vmmath {
	// ------------------------------------------------------------------------------------------------
	// Conventions for every function in this namespace
	//   Row-vector convention: p' = p * M. Translation sits in the fourth row (m[0][3], m[1][3], m[2][3]
	//   in glm's m[col][row] storage) and MatrixMultiply(out, A, B) applies A first, then B.
	//   Right-handed spaces. WS = world; CS = camera (-z forward, +y up, +x right); PS = projection
	//   (x, y in [-1, 1], depth in [0, 1]); SS = screen (pixels, x right, y down, integers at pixel centres).
	//   The f-prefixed functions are the same operations on float (f3p / f44p) instead of double.
	//   Nothing is validated. Every function except MatrixWS2CS / fMatrixWS2CS finishes reading its inputs
	//   before its first store, so an [out] pointer may alias an [in] pointer of the same call. Those two
	//   store mat element by element and read pos_eye after the first twelve stores: mat must not overlap
	//   pos_eye (the two differ in type, so only a type-punned pointer can make them overlap); vec_up and
	//   vec_view are consumed before the first store and may overlap mat.
	// ------------------------------------------------------------------------------------------------

	// pos_out [out] = pos_in [in] as a point (w = 1) times mat [in], divided by the resulting w.
	// A resulting w of 0 yields non-finite output.
	inline void TransformPoint(d3p pos_out, const_d3p pos_in, const_d44p mat);

	// vec_out [out] = vec_in [in] times the upper-left 3x3 of mat [in]: rotation and scale only,
	// no translation, no divide.
	inline void TransformVector(d3p vec_out, const_d3p vec_in, const_d44p mat);

	// mat [out] = matl [in] * matr [in]; under the row-vector convention matl is applied first.
	inline void MatrixMultiply(d44p mat, const_d44p matl, const_d44p matr);

	// vec [out] = vec1 [in] + vec2 [in].
	inline void AddVector(d3p vec, const_d3p vec1, const_d3p vec2);

	// vec [out] = vec1 [in] - vec2 [in].
	inline void SubstractVector(d3p vec, const_d3p vec1, const_d3p vec2);

	// Euclidean length of vec [in], and its square.
	inline double LengthVector(const_d3p vec);
	inline double LengthVectorSq(const_d3p vec);
	// vec_out [out] = vec_in [in] scaled to unit length. A length of DBL_EPSILON or less yields the zero vector.
	inline void NormalizeVector(d3p vec_out, const_d3p vec_in);

	// Dot product of vec1 [in] and vec2 [in].
	inline double DotVector(const_d3p vec1, const_d3p vec2);

	// vec [out] = vec1 [in] x vec2 [in] (cross product, in that order).
	inline void CrossDotVector(d3p vec, const_d3p vec1, const_d3p vec2);

	// mat [out] = the WS-to-CS (view) matrix of a camera at pos_eye [in] looking along vec_view [in] with
	// vec_up [in] as its up hint; all three are WS. CS axes: +z = -vec_view, +x = vec_up x (+z), +y = +z x +x.
	// vec_up need not be orthogonal to vec_view, only non-zero and not parallel to it.
	inline void MatrixWS2CS(d44p mat, const_d3p pos_eye, const_d3p vec_up, const_d3p vec_view);

	// mat [out] = the orthographic CS-to-PS matrix. w [in] and h [in] are the frustum width and height in
	// CS units; near [in] and far [in] are positive CS distances along -z. x and y map to [-1, 1]; z from
	// -near to -far maps to depth 0 to 1; w stays 1.
	inline void MatrixOrthogonalCS2PS(d44p mat, const double w, const double h, const double near, const double far);

	// mat [out] = the perspective CS-to-PS matrix. fovy [in] is the vertical field of view in radians,
	// aspect_ratio [in] is width / height, near [in] and far [in] are positive CS distances along -z.
	// The homogeneous w becomes -z; after the divide, z from -near to -far maps to depth 0 to 1.
	// The x and y scale terms are rounded to float precision in both variants.
	inline void MatrixPerspectiveCS2PS(d44p mat, const double fovy, const double aspect_ratio, const double near, const double far);

	// mat [out] = the PS-to-SS matrix of a w [in] x h [in] pixel viewport: x' = (x + 1) * w / 2 - 0.5,
	// y' = (1 - y) * h / 2 - 0.5, z and w unchanged. Integer SS coordinates are pixel centres and
	// pixel (0, 0) is top-left.
	inline void MatrixPS2SS(d44p mat, const double w, const double h);

	// mat [out] = rotation by angle_rad [in] radians about vec_axis [in] through the origin, right-hand
	// rule. vec_axis need not be unit length.
	inline void MatrixRotationAxis(d44p mat, const_d3p vec_axis, const double angle_rad);

	// mat [out] = scaling by scale_factors [in] (x, y, z) along the axes of the current space.
	inline void MatrixScaling(d44p mat, const_d3p scale_factors);

	// mat [out] = translation by vec_trl [in].
	inline void MatrixTranslation(d44p mat, const_d3p vec_trl);

	// mat [out] = the inverse of mat_in [in], computed as adjugate / determinant with no singularity
	// check: a singular mat_in yields non-finite output.
	inline void MatrixInverse(d44p mat, const_d44p mat_in);

	// Float variants of the functions above, same semantics.
	inline void fTransformPoint(f3p pos_out, const_f3p pos_in, const_f44p mat);
	inline void fTransformVector(f3p vec_out, const_f3p vec_in, const_f44p mat);
	inline float fLengthVector(const_f3p vec);
	inline float fLengthVectorSq(const_f3p vec);
	inline void fNormalizeVector(f3p vec_out, const_f3p vec_in);
	inline float fDotVector(const_f3p vec1, const_f3p vec2);
	inline void fCrossDotVector(f3p vec, const_f3p vec1, const_f3p vec2);

	inline void fMatrixWS2CS(f44p mat, const_f3p pos_eye, const_f3p vec_up, const_f3p vec_view);
	inline void fMatrixOrthogonalCS2PS(f44p mat, const float w, const float h, const float near, const float far);
	inline void fMatrixPerspectiveCS2PS(f44p mat, const float fovy, const float aspect_ratio, const float near, const float far);
	inline void fMatrixPS2SS(f44p mat, const float w, const float h);
	inline void fMatrixRotationAxis(f44p mat, const_f3p vec_axis, const float angle_rad);
	inline void fMatrixScaling(f44p mat, const_f3p scale_factors);
	inline void fMatrixTranslation(f44p mat, const_f3p vec_trl);
	inline void fMatrixInverse(f44p mat, const_f44p mat_in);
}

namespace vmhelpers {
	// Allocates a 2D byte array: ptr_dst [out] receives a table of array_length_2d [in] row pointers, each
	// row a separate new[] of array_sizebytes_1d [in] bytes, zero-filled when clear_zero [in] is true.
	// Element (x, y) is (*ptr_dst)[y][x]. Release every row as char* with delete[] and then the table
	// (VMSAFE_DELETE2DARRAY_VOID in VimCommon.h does exactly that).
	__staticinline void AllocateVoidPointer2D(void*** ptr_dst, const int array_length_2d, const int array_sizebytes_1d, const bool clear_zero = false);

	// free_bytes [out] = available physical memory and valid_sysmem_bytes [out] = total physical memory,
	// in bytes, as the OS reports them at the time of the call.
	__staticinline void GetSystemMemoryInfo(double* free_bytes, double* valid_sysmem_bytes);

	// cpu_info [out] = a bit set of the instruction-set extensions the running CPU reports: bit i is set
	// when feature i below is available.
	//   0 MMX        1 x64        2 ABM        3 RDRAND     4 BMI1       5 BMI2       6 ADX        7 PREFETCHWT1
	//   8 SSE        9 SSE2      10 SSE3      11 SSSE3     12 SSE41     13 SSE42     14 SSE4a     15 AES
	//  16 SHA       17 AVX       18 XOP       19 FMA3      20 FMA4      21 AVX2      22 AVX512F   23 AVX512CD
	//  24 AVX512PF  25 AVX512ER  26 AVX512VL  27 AVX512BW  28 AVX512DQ  29 AVX512IFMA 30 AVX512VBMI
	__staticinline void GetCPUInstructionInfo(int* cpu_info);

	// Stopwatch on std::chrono::high_resolution_clock. Construction and record() store a reference instant;
	// the elapsed_* members measure from that instant. elapsed() is milliseconds.
	struct VmTimer
	{
		std::chrono::high_resolution_clock::time_point timestamp = std::chrono::high_resolution_clock::now();

		// Stores now() as the reference instant.
		inline void record()
		{
			timestamp = std::chrono::high_resolution_clock::now();
		}

		// Seconds from the reference instant to timestamp2.
		inline double elapsed_seconds_since(std::chrono::high_resolution_clock::time_point timestamp2)
		{
			std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(timestamp2 - timestamp);
			return time_span.count();
		}

		// Seconds since the reference instant.
		inline double elapsed_seconds()
		{
			return elapsed_seconds_since(std::chrono::high_resolution_clock::now());
		}

		// Milliseconds since the reference instant.
		inline double elapsed_milliseconds()
		{
			return elapsed_seconds() * 1000.0;
		}

		// Same as elapsed_milliseconds().
		inline double elapsed()
		{
			return elapsed_milliseconds();
		}

		// Seconds since the reference instant; that instant then becomes the new reference.
		inline double record_elapsed_seconds()
		{
			auto timestamp2 = std::chrono::high_resolution_clock::now();
			auto elapsed = elapsed_seconds_since(timestamp2);
			timestamp = timestamp2;
			return elapsed;
		}
	};

	// The current UTC wall-clock time packed into one integer: bits 0-9 millisecond, 10-15 second,
	// 16-21 minute, 22-26 hour, 27-31 day, 32-35 month, 36 and up year. Values order chronologically as
	// integers at 1 ms resolution; the source is the system clock, not a monotonic one. Non-Windows
	// builds return 0.
	__staticinline uint64_t GetCurrentTimePack();
}

#define __WINDOWS
#ifdef __WINDOWS
#define NOMINMAX
#include <windows.h>
#endif

namespace vmmath {
	// Stores in.xyz / in.w into *out as a T.
#define c_out(out, in, T) *(T*)out = T(in.x / in.w, in.y / in.w, in.z / in.w);
	// glm stores m[col][row]; the row-vector convention puts translation in m[c][3]. glm's column-vector
	// builders (translate, rotate) are transposed into that convention below; scale is symmetric and is not.

	inline void TransformPoint(d3p pos_out, const_d3p pos_in, const_d44p mat)
	{
		using namespace glm;
		const dvec3& _pos_in = *(const dvec3*)pos_in;
		const dmat4x4& _mat = *(dmat4x4*)mat;
		dvec4 _pos_out = dvec4(_pos_in, 1.) * _mat;
		c_out(pos_out, _pos_out, dvec3);
	}

	inline void TransformVector(d3p vec_out, const_d3p vec_in, const_d44p mat)
	{
		using namespace glm;
		const dvec3& _vec_in = *(const dvec3*)vec_in;
		const dmat4x4& _mat = *(dmat4x4*)mat;

		const double* _d = glm::value_ptr(_mat);
		double d33[9] = { _d[0], _d[4], _d[8], _d[1], _d[5], _d[9], _d[2], _d[6], _d[10] };
		dmat3x3 _mat33 = glm::make_mat3x3(d33);

		*(dvec3*)vec_out = _mat33 * _vec_in;
	}

	inline void MatrixMultiply(d44p mat, const_d44p matl, const_d44p matr)
	{
		using namespace glm;
		const dmat4x4& _matl = *(dmat4x4*)matl;
		const dmat4x4& _matr = *(dmat4x4*)matr;
		*(dmat4x4*)mat = _matl * _matr;
	}

	inline void AddVector(d3p vec, const_d3p vec1, const_d3p vec2)
	{
		using namespace glm;
		const dvec3& _vec1 = *(dvec3*)vec1;
		const dvec3& _vec2 = *(dvec3*)vec2;
		*(dvec3*)vec = _vec1 + _vec2;
	}

	inline void SubstractVector(d3p vec, const_d3p vec1, const_d3p vec2)
	{
		using namespace glm;
		const dvec3& _vec1 = *(dvec3*)vec1;
		const dvec3& _vec2 = *(dvec3*)vec2;
		*(dvec3*)vec = _vec1 - _vec2;
	}

	inline double LengthVector(const_d3p vec)
	{
		using namespace glm;
		const dvec3& _vec = *(dvec3*)vec;
		return glm::length(_vec);
	}
	inline double LengthVectorSq(const_d3p vec)
	{
		using namespace glm;
		const dvec3& _vec = *(dvec3*)vec;
		return _vec.x * _vec.x + _vec.y * _vec.y + _vec.z * _vec.z;
	}

	inline void NormalizeVector(d3p vec_out, const_d3p vec_in)
	{
		using namespace glm;
		double l = LengthVector(vec_in);
		if (l <= DBL_EPSILON) *(dvec3*)vec_out = dvec3(0);
		else *(dvec3*)vec_out = *(dvec3*)vec_in / l;
	}

	inline double DotVector(const_d3p vec1, const_d3p vec2)
	{
		using namespace glm;
		const dvec3& _vec1 = *(dvec3*)vec1;
		const dvec3& _vec2 = *(dvec3*)vec2;
		return glm::dot(_vec1, _vec2);
	}

	inline void CrossDotVector(d3p vec, const_d3p vec1, const_d3p vec2)
	{
		using namespace glm;
		const dvec3& _vec1 = *(dvec3*)vec1;
		const dvec3& _vec2 = *(dvec3*)vec2;
		*(dvec3*)vec = glm::cross(_vec1, _vec2);
	}

	inline void MatrixWS2CS(d44p mat, const_d3p pos_eye, const_d3p vec_up, const_d3p vec_view)
	{
		using namespace glm;
		const dvec3& _pos_eye = *(dvec3*)pos_eye;
		const dvec3& _vec_up = *(dvec3*)vec_up;
		const dvec3& _vec_view = *(dvec3*)vec_view;

		dvec3 d3VecAxisZ = -_vec_view;
		d3VecAxisZ = glm::normalize(d3VecAxisZ);

		dvec3 d3VecAxisX = glm::cross(_vec_up, d3VecAxisZ);
		d3VecAxisX = glm::normalize(d3VecAxisX);

		dvec3 d3VecAxisY = glm::cross(d3VecAxisZ, d3VecAxisX);
		d3VecAxisY = glm::normalize(d3VecAxisY);

		dmat4x4& _mat = *(dmat4x4*)mat;

		_mat[0][0] = d3VecAxisX.x;
		_mat[1][0] = d3VecAxisY.x;
		_mat[2][0] = d3VecAxisZ.x;
		_mat[3][0] = 0;
		_mat[0][1] = d3VecAxisX.y;
		_mat[1][1] = d3VecAxisY.y;
		_mat[2][1] = d3VecAxisZ.y;
		_mat[3][1] = 0;
		_mat[0][2] = d3VecAxisX.z;
		_mat[1][2] = d3VecAxisY.z;
		_mat[2][2] = d3VecAxisZ.z;
		_mat[3][2] = 0;
		_mat[0][3] = -glm::dot(d3VecAxisX, _pos_eye);
		_mat[1][3] = -glm::dot(d3VecAxisY, _pos_eye);
		_mat[2][3] = -glm::dot(d3VecAxisZ, _pos_eye);
		_mat[3][3] = 1;
	}

	inline void MatrixOrthogonalCS2PS(d44p mat, const double w, const double h, const double _near, const double _far)
	{
		using namespace glm;
		// depth maps to [0, 1], unlike glm::orthoRH
		dmat4x4& _mat = *(dmat4x4*)mat;
		_mat[0][0] = 2. / w;
		_mat[1][0] = 0;
		_mat[2][0] = 0;
		_mat[3][0] = 0;
		_mat[0][1] = 0;
		_mat[1][1] = 2. / h;
		_mat[2][1] = 0;
		_mat[3][1] = 0;
		_mat[0][2] = 0;
		_mat[1][2] = 0;
		_mat[2][2] = 1. / (_near - _far);
		_mat[3][2] = 0;
		_mat[0][3] = 0;
		_mat[1][3] = 0;
		_mat[2][3] = _near / (_near - _far);
		_mat[3][3] = 1.;
	}

	inline void MatrixPerspectiveCS2PS(d44p mat, const double fovy, const double aspect_ratio, const double _near, const double _far)
	{
		using namespace glm;
		double yScale = 1.0 / tan(fovy / 2.0);
		double xScale = yScale / aspect_ratio;

		dmat4x4& _mat = *(dmat4x4*)mat;
		_mat[0][0] = (float)xScale;
		_mat[1][0] = 0;
		_mat[2][0] = 0;
		_mat[3][0] = 0;
		_mat[0][1] = 0;
		_mat[1][1] = (float)yScale;
		_mat[2][1] = 0;
		_mat[3][1] = 0;
		_mat[0][2] = 0;
		_mat[1][2] = 0;
		_mat[2][2] = _far / (_near - _far);
		_mat[3][2] = -1;
		_mat[0][3] = 0;
		_mat[1][3] = 0;
		_mat[2][3] = _near * _far / (_near - _far);
		_mat[3][3] = 0;
	}

	inline void MatrixPS2SS(d44p mat, const double w, const double h)
	{
		using namespace glm;
		dmat4x4 matTranslate, matScale, matTranslateSampleModel;
		matTranslate = glm::translate(dvec3(1., -1., 0.));
		matScale = glm::scale(dvec3(w * 0.5, h * 0.5, 1.));
		matTranslateSampleModel = glm::translate(dvec3(-0.5, 0.5, 0.));

		matTranslate = glm::transpose(matTranslate);
		matTranslateSampleModel = glm::transpose(matTranslateSampleModel);

		*mat = (matTranslate * matScale) * matTranslateSampleModel;
		// negate the y output column: SS y grows downward
		(*mat)[1][0] *= -1.;
		(*mat)[1][1] *= -1.;
		(*mat)[1][2] *= -1.;
		(*mat)[1][3] *= -1.;
	}

	inline void MatrixRotationAxis(d44p mat, const_d3p vec_axis, const double angle_rad)
	{
		using namespace glm;
		const dvec3& _vec_axis = *(dvec3*)vec_axis;
		dmat4x4 _mat = glm::rotate(angle_rad, _vec_axis);
		*(dmat4x4*)mat = glm::transpose(_mat);
	}

	inline void MatrixScaling(d44p mat, const_d3p scale_factors)
	{
		using namespace glm;
		*(dmat4x4*)mat = glm::scale(*(const dvec3*)scale_factors);
	}

	inline void MatrixTranslation(d44p mat, const_d3p vec_trl)
	{
		using namespace glm;
		dmat4x4 _mat = glm::translate(*(const dvec3*)vec_trl);
		*(dmat4x4*)mat = glm::transpose(_mat);
	}

	inline void MatrixInverse(d44p mat, const_d44p mat_in)
	{
		using namespace glm;
		const dmat4x4& _mat = *(dmat4x4*)mat_in;
		*(dmat4x4*)mat = glm::inverse(_mat);
	}

	inline void fTransformPoint(f3p pos_out, const_f3p pos_in, const_f44p mat)
	{
		using namespace glm;
		fvec3& p_in = *(fvec3*)pos_in;

		const fmat4x4& _mat = *(fmat4x4*)mat;
		fvec4 _pos_out = fvec4(p_in, 1.) * _mat;
		c_out(pos_out, _pos_out, fvec3);
	}
	inline void fTransformVector(f3p vec_out, const_f3p vec_in, const_f44p mat)
	{
		using namespace glm;
		const fvec3& _vec_in = *(const fvec3*)vec_in;
		const fmat4x4& _mat = *(fmat4x4*)mat;

		const float* _f = glm::value_ptr(_mat);
		double f33[9] = { _f[0], _f[4], _f[8], _f[1], _f[5], _f[9], _f[2], _f[6], _f[10] };
		fmat3x3 _mat33 = glm::make_mat3x3(f33);

		*(fvec3*)vec_out = _mat33 * _vec_in;
	}
	inline float fLengthVector(const_f3p vec)
	{
		using namespace glm;
		const fvec3& _vec = *(fvec3*)vec;
		return glm::length(_vec);
	}
	inline float fLengthVectorSq(const_f3p vec)
	{
		using namespace glm;
		const fvec3& _vec = *(fvec3*)vec;
		return _vec.x * _vec.x + _vec.y * _vec.y + _vec.z * _vec.z;
	}
	inline void fNormalizeVector(f3p vec_out, const_f3p vec_in)
	{
		using namespace glm;
		float l = fLengthVector(vec_in);
		if (l <= DBL_EPSILON) *(fvec3*)vec_out = fvec3(0);
		else *(fvec3*)vec_out = *(fvec3*)vec_in / l;
	}
	inline float fDotVector(const_f3p vec1, const_f3p vec2)
	{
		using namespace glm;
		const fvec3& _vec1 = *(fvec3*)vec1;
		const fvec3& _vec2 = *(fvec3*)vec2;
		return glm::dot(_vec1, _vec2);
	}
	inline void fCrossDotVector(f3p vec, const_f3p vec1, const_f3p vec2)
	{
		using namespace glm;
		const fvec3& _vec1 = *(fvec3*)vec1;
		const fvec3& _vec2 = *(fvec3*)vec2;
		*(fvec3*)vec = glm::cross(_vec1, _vec2);
	}

	inline void fMatrixWS2CS(f44p mat, const_f3p pos_eye, const_f3p vec_up, const_f3p vec_view)
	{
		using namespace glm;
		const fvec3& _pos_eye = *(fvec3*)pos_eye;
		const fvec3& _vec_up = *(fvec3*)vec_up;
		const fvec3& _vec_view = *(fvec3*)vec_view;

		fvec3 f3VecAxisZ = -_vec_view;
		f3VecAxisZ = glm::normalize(f3VecAxisZ);

		fvec3 f3VecAxisX = glm::cross(_vec_up, f3VecAxisZ);
		f3VecAxisX = glm::normalize(f3VecAxisX);

		fvec3 f3VecAxisY = glm::cross(f3VecAxisZ, f3VecAxisX);
		f3VecAxisY = glm::normalize(f3VecAxisY);

		fmat4x4& _mat = *(fmat4x4*)mat;

		_mat[0][0] = f3VecAxisX.x;
		_mat[1][0] = f3VecAxisY.x;
		_mat[2][0] = f3VecAxisZ.x;
		_mat[3][0] = 0;
		_mat[0][1] = f3VecAxisX.y;
		_mat[1][1] = f3VecAxisY.y;
		_mat[2][1] = f3VecAxisZ.y;
		_mat[3][1] = 0;
		_mat[0][2] = f3VecAxisX.z;
		_mat[1][2] = f3VecAxisY.z;
		_mat[2][2] = f3VecAxisZ.z;
		_mat[3][2] = 0;
		_mat[0][3] = -glm::dot(f3VecAxisX, _pos_eye);
		_mat[1][3] = -glm::dot(f3VecAxisY, _pos_eye);
		_mat[2][3] = -glm::dot(f3VecAxisZ, _pos_eye);
		_mat[3][3] = 1;
	}
	inline void fMatrixOrthogonalCS2PS(f44p mat, const float w, const float h, const float _near, const float _far)
	{
		using namespace glm;
		// depth maps to [0, 1], unlike glm::orthoRH
		fmat4x4& _mat = *(fmat4x4*)mat;
		_mat[0][0] = 2.f / w;
		_mat[1][0] = 0;
		_mat[2][0] = 0;
		_mat[3][0] = 0;
		_mat[0][1] = 0;
		_mat[1][1] = 2.f / h;
		_mat[2][1] = 0;
		_mat[3][1] = 0;
		_mat[0][2] = 0;
		_mat[1][2] = 0;
		_mat[2][2] = 1.f / (_near - _far);
		_mat[3][2] = 0;
		_mat[0][3] = 0;
		_mat[1][3] = 0;
		_mat[2][3] = _near / (_near - _far);
		_mat[3][3] = 1.f;
	}
	inline void fMatrixPerspectiveCS2PS(f44p mat, const float fovy, const float aspect_ratio, const float _near, const float _far)
	{
		using namespace glm;
		double yScale = 1.0 / tan(fovy / 2.0);
		double xScale = yScale / aspect_ratio;

		fmat4x4& _mat = *(fmat4x4*)mat;
		_mat[0][0] = (float)xScale;
		_mat[1][0] = 0;
		_mat[2][0] = 0;
		_mat[3][0] = 0;
		_mat[0][1] = 0;
		_mat[1][1] = (float)yScale;
		_mat[2][1] = 0;
		_mat[3][1] = 0;
		_mat[0][2] = 0;
		_mat[1][2] = 0;
		_mat[2][2] = _far / (_near - _far);
		_mat[3][2] = -1.f;
		_mat[0][3] = 0;
		_mat[1][3] = 0;
		_mat[2][3] = _near * _far / (_near - _far);
		_mat[3][3] = 0;
	}
	inline void fMatrixPS2SS(f44p mat, const float w, const float h)
	{
		using namespace glm;
		fmat4x4 matTranslate, matScale, matTranslateSampleModel;
		matTranslate = glm::translate(fvec3(1.f, -1.f, 0.f));
		matScale = glm::scale(fvec3(w * 0.5f, h * 0.5f, 1.f));
		matTranslateSampleModel = glm::translate(fvec3(-0.5f, 0.5f, 0.f));

		matTranslate = glm::transpose(matTranslate);
		matTranslateSampleModel = glm::transpose(matTranslateSampleModel);

		*mat = (matTranslate * matScale) * matTranslateSampleModel;
		// negate the y output column: SS y grows downward
		(*mat)[1][0] *= -1.;
		(*mat)[1][1] *= -1.;
		(*mat)[1][2] *= -1.;
		(*mat)[1][3] *= -1.;
	}
	inline void fMatrixRotationAxis(f44p mat, const_f3p vec_axis, const float angle_rad)
	{
		using namespace glm;
		const fvec3& _vec_axis = *(fvec3*)vec_axis;
		fmat4x4 _mat = glm::rotate(angle_rad, _vec_axis);
		*(fmat4x4*)mat = glm::transpose(_mat);
	}
	inline void fMatrixScaling(f44p mat, const_f3p scale_factors)
	{
		using namespace glm;
		*(fmat4x4*)mat = glm::scale(*(const fvec3*)scale_factors);
	}
	inline void fMatrixTranslation(f44p mat, const_f3p vec_trl)
	{
		using namespace glm;
		fmat4x4 _mat = glm::translate(*(const fvec3*)vec_trl);
		*(fmat4x4*)mat = glm::transpose(_mat);
	}
	inline void fMatrixInverse(f44p mat, const_f44p mat_in)
	{
		using namespace glm;
		const fmat4x4& _mat = *(fmat4x4*)mat_in;
		*(fmat4x4*)mat = glm::inverse(_mat);
	}
}
