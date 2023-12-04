#pragma once

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
typedef glm::dvec3* d3p;
typedef const glm::dvec3* const_d3p;
typedef glm::dmat4x4* d44p;
typedef const glm::dmat4x4* const_d44p;
typedef glm::fvec3* f3p;
typedef const glm::fvec3* const_f3p;
typedef glm::fmat4x4* f44p;
typedef const glm::fmat4x4* const_f44p;
//typedef void* d3p;
//typedef void* d44p;
//typedef void* f3p;
//typedef void* f44p;
//typedef const void* const_d3p;
//typedef const void* const_d44p;
//typedef const void* const_f3p;
//typedef const void* const_f44p;

// ==> ���߿� void ��...

#define __staticinline extern "C" __declspec(dllexport) inline

namespace vmmath {
	//==========================================
	// Simple Math Defined in Projection Space
	//==========================================
	/*! \fn void vmmath::TransformPoint(d3p pos_out, cd3p pos_in, cd44p mat)
	 *  \brief 3D position�� 4x4 matrix�� ��ȯ�ϴ� function. Homogeneous factor w = 1 �� �ΰ� projecting ó��.
	 *  \param pos_out [out] \n double 3 \n operation�� ��� ������.
	 *  \param pos_in [in] \n double 3 \n operation�� source �� ���� 3D position�� ���� ������.
	 *  \param mat [in] \n double 4x4 \n operation�� ��ȯ�� �����ϴ� 4x4 matrix�� ���� ������
	 *  \remarks row major operation
	 */
	inline void TransformPoint(d3p pos_out, const_d3p pos_in, const_d44p mat);

	/*! \fn void vmmath::TransformVector(d3p vec_out, cd3p vec_in, cd44p mat)
	 *  \brief 3D vector�� 4x4 matrix�� ��ȯ�ϴ� function. Homogeneous factor w = 1 �� �ΰ� projecting ó��.
	 *  \param vec_out [out] \n double 3 \n operation�� ��� ������.
	 *  \param vec_in [in] \n double 3 \n operation�� source �� ���� 3D vector�� ���� ������.
	 *  \param mat [in] \n double 44 \n operation�� ��ȯ�� �����ϴ� 4x4 matrix�� ���� ������
	 *  \remarks row major operation
	 */
	inline void TransformVector(d3p vec_out, const_d3p vec_in, const_d44p mat);

	/*!
	 * @fn void vmmath::MatrixMultiply(d44p mat, cd44p matl, cd44p matr)
	 * @brief �� ���� 4x4 matrix�� ����.
	 * @param mat [out] \n double 44 \n 4x4 matrix�� ������ ��� ������
	 * @param matl [in] \n double 44 \n operation�� source�� 4x4 matrix�� ���� ������ (����)
	 * @param matr [in] \n double 44 \n operation�� source�� 4x4 matrix�� ���� ������ (������)
	 * @remarks n row major ������ operation�̸� ���� ������ ���ʿ��� ���������� �����.
	 */
	inline void MatrixMultiply(d44p mat, const_d44p matl, const_d44p matr);

	/*!
	 * @fn void vmmath::AddVector(d3p vec, cd3p vec1, cd3p vec2)
	 * @brief �� ���� 3D vector�� ����.
	 * @param vec [out] \n double 3 \n 3D vector�� ������ ��� ������
	 * @param vec1 [in] \n double 3 \n operation�� source�� 3D vector�� ���� ������
	 * @param vec2 [in] \n double 3 \n operation�� source�� 3D vector�� ���� ������
	 * @remarks
	*/
	inline void AddVector(d3p vec, const_d3p vec1, const_d3p vec2);

	/*!
	 * @fn void vmmath::SubstractVector(d3p vec, cd3p vec1, cd3p vec2)
	 * @brief �� ���� 3D vector�� ��.
	 * @param vec [out] \n double 3 \n 3D vector�� ���� ��� ������
	 * @param vec1 [in] \n double 3 \n operation�� source�� 3D vector�� ���� ������
	 * @param vec2 [in] \n double 3 \n operation�� source�� 3D vector�� ���� ������
	 * @remarks
	*/
	inline void SubstractVector(d3p vec, const_d3p vec1, const_d3p vec2);

	inline double LengthVector(const_d3p vec);
	inline double LengthVectorSq(const_d3p vec);
	inline void NormalizeVector(d3p vec_out, const_d3p vec_in);

	/*!
	 * @fn float vmmath::DotVector(cd3p vec1, cd3p vec2)
	 * @brief �� ���� 3D vector�� ���� dot product�� ����.
	 * @param vec1 [in] \n double 3 \n operation�� source�� 3D vector�� ���� ������
	 * @param vec2 [in] \n double 3 \n operation�� source�� 3D vector�� ���� ������
	 * @return double \n dot product�� scalar ���
	 * @remarks
	*/
	inline double DotVector(const_d3p vec1, const_d3p vec2);

	/*!
	 * @fn void vmmath::CrossDotVector(d3p vec, cd3p vec1, cd3p vec2)
	 * @brief �� ���� 3D vector�� ���� cross dot product�� ����.
	 * @param vec [out] \n double 3 \n 3D vector�� cross dot product�� ��� ������
	 * @param vec1 [in] \n double 3 \n operation�� source�� 3D vector�� ���� ������ (����)
	 * @param vec2 [in] \n double 3 \n operation�� source�� 3D vector�� ���� ������ (������)
	 * @remarks vec1 ���� vec2 �������� cross dot�� ����, (���ʿ��� ������)
	*/
	inline void CrossDotVector(d3p vec, const_d3p vec1, const_d3p vec2);

	/*!
	 * @fn void vmmath::MatrixWS2CS(d44p mat, cd3p pos_eye, cd3p vec_up, cd3p vec_view)
	 * @brief ������ �������� ���ǵ� Space�� �������� look-at Space�� ��ȯ�ϴ� matrix�� ����.
	 * @param mat [out] \n double 44 \n look-at Space(�Ǵ� Camera Space or Viewing Space)�� ��ȯ�ϴ� matrix�� ���� ������
	 * @param pos_eye [in] \n double 3 \n Camera(�Ǵ� Eye)�� position�� �����ϴ� ������
	 * @param vec_up [in] \n double 3 \n ���� Camera(�Ǵ� Eye)�� ���ǵ� Space������ ���� ������ �����ϴ� vector�� ���� ������
	 * @param vec_view [in] \n double 3 \n ���� Camera(�Ǵ� Eye)�� ���ǵ� Space������ �ü��� ������  �����ϴ� vector�� ���� ������
	 * @remarks
	 * RHS �����̸� row major �������� matrix�� ���� \n
	 * pos_eye, vec_up, vec_view�� Space�� ��� �����ؾ� �ϸ� �ϳ��� World Space�� ���� \n
	 * Camera Space������ View ������ -z�� ����, Up ������ y�� ���� \n
	*/
	inline void MatrixWS2CS(d44p mat, const_d3p pos_eye, const_d3p vec_up, const_d3p vec_view);

	/*!
	 * @fn void vmmath::MatrixOrthogonalCS2PS(d44p mat, const double w, const double h, const double near, const double far)
	 * @brief ������ �������� ���ǵ� Space�� �������� Orhogonal Projecting�� �����ϴ� matrix�� ����.
	 * @param mat [out] \n double 44 \n Projection Space(�Ǵ� Camera Space or Viewing Space)�� ��ȯ�ϴ� matrix�� ���� ������
	 * @param w [in] \n double \n View Frustum �� width�� Camera Space���� ���ǵ� ���� ���
	 * @param h [in] \n double \n View Frustum �� height�� Camera Space���� ���ǵ� ���� ���
	 * @param near [in] \n double \n View Frustum �� minimum z ������ Camera Space���� ���ǵ� ���� ���
	 * @param far [in] \n double \n View Frustum �� maximum z ������ Camera Space���� ���ǵ� ���� ���
	 * @remarks
	 * RHS �����̸� row major �������� matrix�� ���� \n
	 * look-at Space(�Ǵ� Camera Space or Viewing Space)���� Projection Space�� ��ȯ \n
	 * View Frustum ���� ������ ��� Cripping out �Ǿ� Projection ���� ���� \n
	 * ��ǥ���� ������ Camera Space�� ����
	*/
	inline void MatrixOrthogonalCS2PS(d44p mat, const double w, const double h, const double near, const double far);

	/*!
	 * @fn void vmmath::MatrixPerspectiveCS2PS(d44p mat, float fovy, float aspect_ratio, float near, float far)
	 * @brief ������ �������� ���ǵ� Space�� �������� Perspective Projecting�� �����ϴ� matrix�� ����.
	 * @param mat [out] \n double 44 \n Projection Space(�Ǵ� Camera Space or Viewing Space)�� ��ȯ�ϴ� vxmatrix�� ���� ������
	 * @param fovy [in] \n double \n Perspective View Frustum �� y ���⿡ ���� field of view�� radian���� ������ ��
	 * @param aspect_ratio [in] \n double \n Perspective View Frustum �� Aspect Ratio�� Viewing Space���� ���ǵǴ� View Plane�� (width / height)
	 * @param near [in] \n double \n View Frustum �� minimum z ������ Camera Space���� ���ǵ� ���� ���
	 * @param far [in] \n double \n View Frustum �� maximum z ������ Camera Space���� ���ǵ� ���� ���
	 * @remarks
	 * RHS �����̸� row major �������� matrix�� ���� \n
	 * look-at Space(�Ǵ� Camera Space or Viewing Space)���� Projection Space�� ��ȯ \n
	 * View Frustum ���� ������ ��� Cripping out �Ǿ� Projection ���� ���� \n
	 * ��ǥ���� ������ Camera Space�� ����
	*/
	inline void MatrixPerspectiveCS2PS(d44p mat, const double fovy, const double aspect_ratio, const double near, const double far);

	/*!
	 * @fn void vmmath::MatrixPS2SS(d44p mat, const double w, const double h)
	 * @brief ������ �������� ���ǵ� Space�� �������� Screen�� Pixel Plane���� ���ǵǴ� Screen Space�� ��ȯ�ϴ� matrix ����.
	 * @param psvxMatrix [out] \n double 44 \n Screen Space�� ��ȯ�ϴ� vxmatrix�� ���� ������
	 * @param w [in] \n double \n Screen�� width�� pixel ������ ����
	 * @param h [in] \n double \n Screen�� height�� pixel ������ ����
	 * @remarks
	 * RHS �����̸� row major �������� matrix�� ����
	 * Projection Space���� Screen Space�� ��ȯ \n
	 * Projection Space���� ���ǵǴ� View Frustum�� Near Plane�� Screen Plane�� Mapping��. \n
	 * Screen Space���� Screen�� �������� x��, �Ʒ����� y��, Viewing Depth ������ z������ ����
	*/
	inline void MatrixPS2SS(d44p mat, const double w, const double h);

	/*!
	 * @fn void vmmath::MatrixRotationAxis(d44p mat, cd3p vec_axis, const double angle_rad)
	 * @brief ������ �������� �־��� ���� �߽����� ȸ���ϴ� matrix ����
	 * @param mat [out] \n double 44 \n ȸ�� matrix�� ��� ������
	 * @param vec_axis [in] \n double 3 \n ȸ������ �����ϴ� 3D vector�� �����ϴ� ������
	 * @param angle_rad [in] \n double \n ȸ������ ����. Radian ����.
	 * @remarks ��ǥ���� ���⿡ ���� ȸ�� ������ ������ (ex. RHS�� ��� �������� ����, LHS�� ��� �޳��� ����)
	*/
	inline void MatrixRotationAxis(d44p mat, const_d3p vec_axis, const double angle_rad);

	/*!
	* @fn void vmmath::MatrixScaling(d44p mat, cd3p scale_factors)
	* @brief ���� ��ǥ�谡 �����ϴ� ���� ������ ���� scale �ϴ� matrix ����
	* @param mat [out] \n double 44 \n scaling matrix�� ��� ������
	* @param scale_factors [in] \n double 3 \n (x,y,z) �� �� ���⿡ ���� scaling factor�� �����ϴ� ������
	* @remarks row major operation
	*/
	inline void MatrixScaling(d44p mat, const_d3p scale_factors);

	/*!
	* @fn void vmmath::MatrixTranslation(d44p mat, cd3p vec_trl)
	* @brief vector ������ ���� translation �ϴ� matrix ����
	* @param mat [out] \n double 44 \n translation matrix�� ��� ������
	* @param vec_trl [in] \n double 3 \n (x,y,z) �� �� ���⿡ ���� scaling factor�� �����ϴ� ������
	* @remarks row major operation
	*/
	inline void MatrixTranslation(d44p mat, const_d3p vec_trl);

	/*!
	* @fn void vmmath::MatrixInverse(d44p mat, cd44p mat_in)
	* @brief Inverse matrix�� ����
	* @param psvxMatrixInv [out] \n double 44 \n inverse matrix�� ��� ������
	* @param psvxMatrix [in] \n double 44 \n operation�� source�� matrix�� ���� ������
	* @remarks Determinant 0�� ��� mat_in �� ���� ��ȯ���� �ʰ� ��ȯ
	*/
	inline void MatrixInverse(d44p mat, const_d44p mat_in);

	// float version
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
	/*!
	 * @fn bool vxhelpers::AllocateVoidPointer2D(void*** ptr_dst, const int array_length_2d, const int array_sizebytes_1d)
	 * @brief ������ Data Type�� ���� 2D�� �޸𸮸� �Ҵ�
	 * @param ptr_dst [out] \n void** @n �Ҵ�� �޸𸮿� ���� 2D ������
	 * @param array_length_2d [in] \n int \n [v][] , y dimension
	 * @param array_sizebytes_1d [in] \n int \n [][v] (bytes), x dimension
	 * @param clear_zero [in] \n bool \n set zeros to the allocated memory
	 * @remarks
	 * �Ҵ�� �޸� �����Ϳ� ���� (x, y) �� indexing �� (*pppVoidTarget)[y][x]���� ����� ��
	*/
	__staticinline void AllocateVoidPointer2D(void*** ptr_dst, const int array_length_2d, const int array_sizebytes_1d, const bool clear_zero = false);

	/*!
	 * @fn void vxhelpers::GetSystemMemoryInfo(double* free_bytes, double* valid_sysmem_bytes)
	 * @brief ���� �����ǰ� �ִ� OS�� ���� System Memory ���¸� ����
	 * @param free_bytes [out] \n double \n ���� ��� ������ �޸� ũ�� (bytes)
	 * @param valid_sysmem_bytes [out] \n double \n ���� System �� �νĵǴ� ���� �޸� ũ�� (bytes)
	 * @remarks x86 �Ǵ� x64, ���� ���� OS�� ���¿� ���� ���� �޸𸮿� �ٸ��� ���� �� ����.
	*/
	__staticinline void GetSystemMemoryInfo(double* free_bytes, double* valid_sysmem_bytes);

	/*!
	* @fn void vxhelpers::GetCPUInstructionInfo(int* cpu_info)
	* @brief ���� CPU�� �����ϴ� instruction ������ ����
	* @param piCPUInstructionInfo [out] \n int \n instruction ����
	* 00 : MMX;
	* 01 : x64;
	* 02 : ABM;      // Advanced Bit Manipulation
	* 03 : RDRAND;
	* 04 : BMI1;
	* 05 : BMI2;
	* 06 : ADX;
	* 07 : PREFETCHWT1;
	* 08 : SSE;
	* 09 : SSE2;
	* 10 : SSE3;
	* 11 : SSSE3;
	* 12 : SSE41;
	* 13 : SSE42;
	* 14 : SSE4a;
	* 15 : AES;
	* 16 : SHA;
	* 17 : AVX;
	* 18 : XOP;
	* 19 : FMA3;
	* 20 : FMA4;
	* 21 : AVX2;
	* 22 : AVX512F;    //  AVX512 Foundation
	* 23 : AVX512CD;   //  AVX512 Conflict Detection
	* 24 : AVX512PF;   //  AVX512 Prefetch
	* 25 : AVX512ER;   //  AVX512 Exponential + Reciprocal
	* 26 : AVX512VL;   //  AVX512 Vector Length Extensions
	* 27 : AVX512BW;   //  AVX512 Byte + Word
	* 28 : AVX512DQ;   //  AVX512 Doubleword + Quadword
	* 29 : AVX512IFMA; //  AVX512 Integer 52-bit Fused Multiply-Add
	* 30 : AVX512VBMI; //  AVX512 Vector Byte Manipulation Instructions
	* @remarks
	*/
	__staticinline void GetCPUInstructionInfo(int* cpu_info);

	/*!
	 * @fn ullong vxhelpers::GetCurrentTimePack()
	 * @brief ���� �ð��� ���� ������ 64bit���� ���ڵ��Ͽ� ����
	 * @return double \n  0~9 bit : milli-seconds , 10~15 bit : second, 16~21 bit : minute, 22~26 bit : hour,  27~31 bit : day, 32~35 bit : month, 36~65 bit : year
	 * @remarks
	*/
	__staticinline unsigned long long GetCurrentTimePack();
}


//#include "VimHelpers.h"

#define __WINDOWS
#ifdef __WINDOWS
#define NOMINMAX
#include <windows.h>
#endif

namespace vmmath {
#define c_out(out, in, T) *(T*)out = T(in.x / in.w, in.y / in.w, in.z / in.w);
	// row major math...

	// glm::mat2x2 [col][row]
	// ==> (0~3)
	// [0][0] (0), [1][0] (2)
	// [0][1] (1). [1][1] (3)
	// legacy_mat2x2 _m
	// ==> (0~3)
	// _m11 (0), _m21 (2)
	// _m12 (1), _m22 (3)

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
		//double d33[9] = { d[0], d[1], d[2], d[4], d[5], d[6], d[8], d[9], d[10] };
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
		//dmat4x4 _mat = glm::orthoRH(-w / 2., w / 2., -h / 2., h / 2., _near, _far);
		//*(dmat4x4*)mat = glm::transpose(_mat);
		// GL �� �ٸ���. 
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
		//const double h = 1.0;
		//const double w = aspect_ratio * h;
		//dmat4x4 _mat = glm::perspectiveFovRH(fovy, w, h, _near, _far);
		//*(dmat4x4*)mat = glm::transpose(_mat);
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
		//matScale = glm::transpose(matScale);
		matTranslateSampleModel = glm::transpose(matTranslateSampleModel);

		*mat = (matTranslate * matScale) * matTranslateSampleModel;
		//[row][column] (legacy)
		//[col][row] (vismtv...glm...)
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
		//double f33[9] = { _f[0], _f[1], _f[2], _f[4], _f[5], _f[6], _f[8], _f[9], _f[10] };
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
		//const fvec3& _pos_eye = *(fvec3*)pos_eye;
		//const fvec3& _vec_up = *(fvec3*)vec_up;
		//const fvec3& _vec_view = *(fvec3*)vec_view;
		//fmat4x4 _mat = glm::lookAtRH(_pos_eye, _pos_eye + _vec_view, _vec_up);
		//*(fmat4x4*)mat = glm::transpose(_mat);

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
		//fmat4x4 _mat = glm::orthoRH(-w / 2.f, w / 2.f, -h / 2.f, h / 2.f, _near, _far);
		//*(fmat4x4*)mat = glm::transpose(_mat);
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
		//const float h = 1.0f;
		//const float w = aspect_ratio * h;
		//fmat4x4 _mat = glm::perspectiveFovRH(fovy, w, h, _near, _far);
		//*(fmat4x4*)mat = glm::transpose(_mat);

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
		//matScale = glm::transpose(matScale);
		matTranslateSampleModel = glm::transpose(matTranslateSampleModel);

		*mat = (matTranslate * matScale) * matTranslateSampleModel;
		//[row][column] (legacy)
		//[col][row] (vismtv)
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
