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
	__staticinline void TransformPoint(d3p pos_out, const_d3p pos_in, const_d44p mat);

	/*! \fn void vmmath::TransformVector(d3p vec_out, cd3p vec_in, cd44p mat)
	 *  \brief 3D vector�� 4x4 matrix�� ��ȯ�ϴ� function. Homogeneous factor w = 1 �� �ΰ� projecting ó��.
	 *  \param vec_out [out] \n double 3 \n operation�� ��� ������.
	 *  \param vec_in [in] \n double 3 \n operation�� source �� ���� 3D vector�� ���� ������.
	 *  \param mat [in] \n double 44 \n operation�� ��ȯ�� �����ϴ� 4x4 matrix�� ���� ������
	 *  \remarks row major operation
	 */
	__staticinline void TransformVector(d3p vec_out, const_d3p vec_in, const_d44p mat);

	/*!
	 * @fn void vmmath::MatrixMultiply(d44p mat, cd44p matl, cd44p matr)
	 * @brief �� ���� 4x4 matrix�� ����.
	 * @param mat [out] \n double 44 \n 4x4 matrix�� ������ ��� ������
	 * @param matl [in] \n double 44 \n operation�� source�� 4x4 matrix�� ���� ������ (����)
	 * @param matr [in] \n double 44 \n operation�� source�� 4x4 matrix�� ���� ������ (������)
	 * @remarks n row major ������ operation�̸� ���� ������ ���ʿ��� ���������� �����.
	 */
	__staticinline void MatrixMultiply(d44p mat, const_d44p matl, const_d44p matr);

	/*!
	 * @fn void vmmath::AddVector(d3p vec, cd3p vec1, cd3p vec2)
	 * @brief �� ���� 3D vector�� ����.
	 * @param vec [out] \n double 3 \n 3D vector�� ������ ��� ������
	 * @param vec1 [in] \n double 3 \n operation�� source�� 3D vector�� ���� ������
	 * @param vec2 [in] \n double 3 \n operation�� source�� 3D vector�� ���� ������
	 * @remarks
	*/
	__staticinline void AddVector(d3p vec, const_d3p vec1, const_d3p vec2);

	/*!
	 * @fn void vmmath::SubstractVector(d3p vec, cd3p vec1, cd3p vec2)
	 * @brief �� ���� 3D vector�� ��.
	 * @param vec [out] \n double 3 \n 3D vector�� ���� ��� ������
	 * @param vec1 [in] \n double 3 \n operation�� source�� 3D vector�� ���� ������
	 * @param vec2 [in] \n double 3 \n operation�� source�� 3D vector�� ���� ������
	 * @remarks
	*/
	__staticinline void SubstractVector(d3p vec, const_d3p vec1, const_d3p vec2);

	__staticinline double LengthVector(const_d3p vec);
	__staticinline double LengthVectorSq(const_d3p vec);
	__staticinline void NormalizeVector(d3p vec_out, const_d3p vec_in);

	/*!
	 * @fn float vmmath::DotVector(cd3p vec1, cd3p vec2)
	 * @brief �� ���� 3D vector�� ���� dot product�� ����.
	 * @param vec1 [in] \n double 3 \n operation�� source�� 3D vector�� ���� ������
	 * @param vec2 [in] \n double 3 \n operation�� source�� 3D vector�� ���� ������
	 * @return double \n dot product�� scalar ���
	 * @remarks
	*/
	__staticinline double DotVector(const_d3p vec1, const_d3p vec2);

	/*!
	 * @fn void vmmath::CrossDotVector(d3p vec, cd3p vec1, cd3p vec2)
	 * @brief �� ���� 3D vector�� ���� cross dot product�� ����.
	 * @param vec [out] \n double 3 \n 3D vector�� cross dot product�� ��� ������
	 * @param vec1 [in] \n double 3 \n operation�� source�� 3D vector�� ���� ������ (����)
	 * @param vec2 [in] \n double 3 \n operation�� source�� 3D vector�� ���� ������ (������)
	 * @remarks vec1 ���� vec2 �������� cross dot�� ����, (���ʿ��� ������)
	*/
	__staticinline void CrossDotVector(d3p vec, const_d3p vec1, const_d3p vec2);

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
	__staticinline void MatrixWS2CS(d44p mat, const_d3p pos_eye, const_d3p vec_up, const_d3p vec_view);

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
	__staticinline void MatrixOrthogonalCS2PS(d44p mat, const double w, const double h, const double near, const double far);

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
	__staticinline void MatrixPerspectiveCS2PS(d44p mat, const double fovy, const double aspect_ratio, const double near, const double far);

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
	__staticinline void MatrixPS2SS(d44p mat, const double w, const double h);

	/*!
	 * @fn void vmmath::MatrixRotationAxis(d44p mat, cd3p vec_axis, const double angle_rad)
	 * @brief ������ �������� �־��� ���� �߽����� ȸ���ϴ� matrix ����
	 * @param mat [out] \n double 44 \n ȸ�� matrix�� ��� ������
	 * @param vec_axis [in] \n double 3 \n ȸ������ �����ϴ� 3D vector�� �����ϴ� ������
	 * @param angle_rad [in] \n double \n ȸ������ ����. Radian ����.
	 * @remarks ��ǥ���� ���⿡ ���� ȸ�� ������ ������ (ex. RHS�� ��� �������� ����, LHS�� ��� �޳��� ����)
	*/
	__staticinline void MatrixRotationAxis(d44p mat, const_d3p vec_axis, const double angle_rad);

	/*!
	* @fn void vmmath::MatrixScaling(d44p mat, cd3p scale_factors)
	* @brief ���� ��ǥ�谡 �����ϴ� ���� ������ ���� scale �ϴ� matrix ����
	* @param mat [out] \n double 44 \n scaling matrix�� ��� ������
	* @param scale_factors [in] \n double 3 \n (x,y,z) �� �� ���⿡ ���� scaling factor�� �����ϴ� ������
	* @remarks row major operation
	*/
	__staticinline void MatrixScaling(d44p mat, const_d3p scale_factors);

	/*!
	* @fn void vmmath::MatrixTranslation(d44p mat, cd3p vec_trl)
	* @brief vector ������ ���� translation �ϴ� matrix ����
	* @param mat [out] \n double 44 \n translation matrix�� ��� ������
	* @param vec_trl [in] \n double 3 \n (x,y,z) �� �� ���⿡ ���� scaling factor�� �����ϴ� ������
	* @remarks row major operation
	*/
	__staticinline void MatrixTranslation(d44p mat, const_d3p vec_trl);

	/*!
	* @fn void vmmath::MatrixInverse(d44p mat, cd44p mat_in)
	* @brief Inverse matrix�� ����
	* @param psvxMatrixInv [out] \n double 44 \n inverse matrix�� ��� ������
	* @param psvxMatrix [in] \n double 44 \n operation�� source�� matrix�� ���� ������
	* @remarks Determinant 0�� ��� mat_in �� ���� ��ȯ���� �ʰ� ��ȯ
	*/
	__staticinline void MatrixInverse(d44p mat, const_d44p mat_in);

	// float version
	__staticinline void fTransformPoint(f3p pos_out, const_f3p pos_in, const_f44p mat);
	__staticinline void fTransformVector(f3p vec_out, const_f3p vec_in, const_f44p mat);
	__staticinline float fLengthVector(const_f3p vec);
	__staticinline float fLengthVectorSq(const_f3p vec);
	__staticinline void fNormalizeVector(f3p vec_out, const_f3p vec_in);
	__staticinline float fDotVector(const_f3p vec1, const_f3p vec2);
	__staticinline void fCrossDotVector(f3p vec, const_f3p vec1, const_f3p vec2);

	__staticinline void fMatrixWS2CS(f44p mat, const_f3p pos_eye, const_f3p vec_up, const_f3p vec_view);
	__staticinline void fMatrixOrthogonalCS2PS(f44p mat, const float w, const float h, const float near, const float far);
	__staticinline void fMatrixPerspectiveCS2PS(f44p mat, const float fovy, const float aspect_ratio, const float near, const float far);
	__staticinline void fMatrixPS2SS(f44p mat, const float w, const float h);
	__staticinline void fMatrixRotationAxis(f44p mat, const_f3p vec_axis, const float angle_rad);
	__staticinline void fMatrixScaling(f44p mat, const_f3p scale_factors);
	__staticinline void fMatrixTranslation(f44p mat, const_f3p vec_trl);
	__staticinline void fMatrixInverse(f44p mat, const_f44p mat_in);
}

namespace vmhelpers {
	/*!
	 * @fn bool vxhelpers::AllocateVoidPointer2D(void*** ptr_dst, const int array_length_2d, const int array_sizebytes_1d)
	 * @brief ������ Data Type�� ���� 2D�� �޸𸮸� �Ҵ�
	 * @param ptr_dst [out] \n void** @n �Ҵ�� �޸𸮿� ���� 2D ������
	 * @param array_length_2d [in] \n int \n [v][] , y dimension
	 * @param array_sizebytes_1d [in] \n int \n [][v] (bytes), x dimension
	 * @remarks
	 * �Ҵ�� �޸� �����Ϳ� ���� (x, y) �� indexing �� (*pppVoidTarget)[y][x]���� ����� ��
	*/
	__staticinline void AllocateVoidPointer2D(void*** ptr_dst, const int array_length_2d, const int array_sizebytes_1d);

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