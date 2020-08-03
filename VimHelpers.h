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

// ==> 나중에 void 로...

#define __staticinline extern "C" __declspec(dllexport) inline

namespace vmmath {
	//==========================================
	// Simple Math Defined in Projection Space
	//==========================================
	/*! \fn void vmmath::TransformPoint(d3p pos_out, cd3p pos_in, cd44p mat)
	 *  \brief 3D position을 4x4 matrix로 변환하는 function. Homogeneous factor w = 1 로 두고 projecting 처리.
	 *  \param pos_out [out] \n double 3 \n operation의 결과 포인터.
	 *  \param pos_in [in] \n double 3 \n operation의 source 로 사용될 3D position에 대한 포인터.
	 *  \param mat [in] \n double 4x4 \n operation의 변환을 정의하는 4x4 matrix에 대한 포인터
	 *  \remarks row major operation
	 */
	__staticinline void TransformPoint(d3p pos_out, const_d3p pos_in, const_d44p mat);

	/*! \fn void vmmath::TransformVector(d3p vec_out, cd3p vec_in, cd44p mat)
	 *  \brief 3D vector를 4x4 matrix로 변환하는 function. Homogeneous factor w = 1 로 두고 projecting 처리.
	 *  \param vec_out [out] \n double 3 \n operation의 결과 포인터.
	 *  \param vec_in [in] \n double 3 \n operation의 source 로 사용될 3D vector에 대한 포인터.
	 *  \param mat [in] \n double 44 \n operation의 변환을 정의하는 4x4 matrix에 대한 포인터
	 *  \remarks row major operation
	 */
	__staticinline void TransformVector(d3p vec_out, const_d3p vec_in, const_d44p mat);

	/*!
	 * @fn void vmmath::MatrixMultiply(d44p mat, cd44p matl, cd44p matr)
	 * @brief 두 개의 4x4 matrix를 곱함.
	 * @param mat [out] \n double 44 \n 4x4 matrix가 곱해진 결과 포인터
	 * @param matl [in] \n double 44 \n operation의 source인 4x4 matrix에 대한 포인터 (왼쪽)
	 * @param matr [in] \n double 44 \n operation의 source인 4x4 matrix에 대한 포인터 (오른쪽)
	 * @remarks n row major 에서는 operation이며 곱의 순서가 왼쪽에서 오른쪽으로 진행됨.
	 */
	__staticinline void MatrixMultiply(d44p mat, const_d44p matl, const_d44p matr);

	/*!
	 * @fn void vmmath::AddVector(d3p vec, cd3p vec1, cd3p vec2)
	 * @brief 두 개의 3D vector를 더함.
	 * @param vec [out] \n double 3 \n 3D vector가 더해진 결과 포인터
	 * @param vec1 [in] \n double 3 \n operation의 source인 3D vector에 대한 포인터
	 * @param vec2 [in] \n double 3 \n operation의 source인 3D vector에 대한 포인터
	 * @remarks
	*/
	__staticinline void AddVector(d3p vec, const_d3p vec1, const_d3p vec2);

	/*!
	 * @fn void vmmath::SubstractVector(d3p vec, cd3p vec1, cd3p vec2)
	 * @brief 두 개의 3D vector를 뺌.
	 * @param vec [out] \n double 3 \n 3D vector가 빼진 결과 포인터
	 * @param vec1 [in] \n double 3 \n operation의 source인 3D vector에 대한 포인터
	 * @param vec2 [in] \n double 3 \n operation의 source인 3D vector에 대한 포인터
	 * @remarks
	*/
	__staticinline void SubstractVector(d3p vec, const_d3p vec1, const_d3p vec2);

	__staticinline double LengthVector(const_d3p vec);
	__staticinline double LengthVectorSq(const_d3p vec);
	__staticinline void NormalizeVector(d3p vec_out, const_d3p vec_in);

	/*!
	 * @fn float vmmath::DotVector(cd3p vec1, cd3p vec2)
	 * @brief 두 개의 3D vector에 대한 dot product를 수행.
	 * @param vec1 [in] \n double 3 \n operation의 source인 3D vector에 대한 포인터
	 * @param vec2 [in] \n double 3 \n operation의 source인 3D vector에 대한 포인터
	 * @return double \n dot product의 scalar 결과
	 * @remarks
	*/
	__staticinline double DotVector(const_d3p vec1, const_d3p vec2);

	/*!
	 * @fn void vmmath::CrossDotVector(d3p vec, cd3p vec1, cd3p vec2)
	 * @brief 두 개의 3D vector에 대한 cross dot product를 수행.
	 * @param vec [out] \n double 3 \n 3D vector가 cross dot product된 결과 포인터
	 * @param vec1 [in] \n double 3 \n operation의 source인 3D vector에 대한 포인터 (왼쪽)
	 * @param vec2 [in] \n double 3 \n operation의 source인 3D vector에 대한 포인터 (오른쪽)
	 * @remarks vec1 에서 vec2 방향으로 cross dot을 수행, (왼쪽에서 오른쪽)
	*/
	__staticinline void CrossDotVector(d3p vec, const_d3p vec1, const_d3p vec2);

	/*!
	 * @fn void vmmath::MatrixWS2CS(d44p mat, cd3p pos_eye, cd3p vec_up, cd3p vec_view)
	 * @brief 오른손 방향으로 정의된 Space를 기준으로 look-at Space로 변환하는 matrix를 생성.
	 * @param mat [out] \n double 44 \n look-at Space(또는 Camera Space or Viewing Space)로 변환하는 matrix에 대한 포인터
	 * @param pos_eye [in] \n double 3 \n Camera(또는 Eye)의 position을 정의하는 포인터
	 * @param vec_up [in] \n double 3 \n 현재 Camera(또는 Eye)가 정의된 Space에서의 위쪽 방향을 정의하는 vector에 대한 포인터
	 * @param vec_view [in] \n double 3 \n 현재 Camera(또는 Eye)가 정의된 Space에서의 시선의 방향을  정의하는 vector에 대한 포인터
	 * @remarks
	 * RHS 기준이며 row major 기준으로 matrix가 생성 \n
	 * pos_eye, vec_up, vec_view의 Space는 모두 동일해야 하며 하나의 World Space로 통일 \n
	 * Camera Space에서의 View 방향은 -z축 방향, Up 방향은 y축 방향 \n
	*/
	__staticinline void MatrixWS2CS(d44p mat, const_d3p pos_eye, const_d3p vec_up, const_d3p vec_view);

	/*!
	 * @fn void vmmath::MatrixOrthogonalCS2PS(d44p mat, const double w, const double h, const double near, const double far)
	 * @brief 오른손 방향으로 정의된 Space를 기준으로 Orhogonal Projecting을 수행하는 matrix를 생성.
	 * @param mat [out] \n double 44 \n Projection Space(또는 Camera Space or Viewing Space)로 변환하는 matrix에 대한 포인터
	 * @param w [in] \n double \n View Frustum 의 width로 Camera Space에서 정의된 단위 사용
	 * @param h [in] \n double \n View Frustum 의 height로 Camera Space에서 정의된 단위 사용
	 * @param near [in] \n double \n View Frustum 의 minimum z 값으로 Camera Space에서 정의된 단위 사용
	 * @param far [in] \n double \n View Frustum 의 maximum z 값으로 Camera Space에서 정의된 단위 사용
	 * @remarks
	 * RHS 기준이며 row major 기준으로 matrix가 생성 \n
	 * look-at Space(또는 Camera Space or Viewing Space)에서 Projection Space로 변환 \n
	 * View Frustum 외의 영역은 모두 Cripping out 되어 Projection 되지 않음 \n
	 * 좌표계의 방향은 Camera Space와 동일
	*/
	__staticinline void MatrixOrthogonalCS2PS(d44p mat, const double w, const double h, const double near, const double far);

	/*!
	 * @fn void vmmath::MatrixPerspectiveCS2PS(d44p mat, float fovy, float aspect_ratio, float near, float far)
	 * @brief 오른손 방향으로 정의된 Space를 기준으로 Perspective Projecting을 수행하는 matrix를 생성.
	 * @param mat [out] \n double 44 \n Projection Space(또는 Camera Space or Viewing Space)로 변환하는 vxmatrix에 대한 포인터
	 * @param fovy [in] \n double \n Perspective View Frustum 의 y 방향에 대한 field of view를 radian으로 정의한 값
	 * @param aspect_ratio [in] \n double \n Perspective View Frustum 의 Aspect Ratio로 Viewing Space에거 정의되는 View Plane의 (width / height)
	 * @param near [in] \n double \n View Frustum 의 minimum z 값으로 Camera Space에서 정의된 단위 사용
	 * @param far [in] \n double \n View Frustum 의 maximum z 값으로 Camera Space에서 정의된 단위 사용
	 * @remarks
	 * RHS 기준이며 row major 기준으로 matrix가 생성 \n
	 * look-at Space(또는 Camera Space or Viewing Space)에서 Projection Space로 변환 \n
	 * View Frustum 외의 영역은 모두 Cripping out 되어 Projection 되지 않음 \n
	 * 좌표계의 방향은 Camera Space와 동일
	*/
	__staticinline void MatrixPerspectiveCS2PS(d44p mat, const double fovy, const double aspect_ratio, const double near, const double far);

	/*!
	 * @fn void vmmath::MatrixPS2SS(d44p mat, const double w, const double h)
	 * @brief 오른손 방향으로 정의된 Space를 기준으로 Screen의 Pixel Plane으로 정의되는 Screen Space로 변환하는 matrix 생성.
	 * @param psvxMatrix [out] \n double 44 \n Screen Space로 변환하는 vxmatrix에 대한 포인터
	 * @param w [in] \n double \n Screen의 width로 pixel 단위로 정의
	 * @param h [in] \n double \n Screen의 height로 pixel 단위로 정의
	 * @remarks
	 * RHS 기준이며 row major 기준으로 matrix가 생성
	 * Projection Space에서 Screen Space로 변환 \n
	 * Projection Space에서 정의되는 View Frustum의 Near Plane에 Screen Plane이 Mapping됨. \n
	 * Screen Space에서 Screen의 오른쪽이 x축, 아래쪽이 y축, Viewing Depth 방향이 z축으로 정의
	*/
	__staticinline void MatrixPS2SS(d44p mat, const double w, const double h);

	/*!
	 * @fn void vmmath::MatrixRotationAxis(d44p mat, cd3p vec_axis, const double angle_rad)
	 * @brief 원점을 기준으로 주어진 축을 중심으로 회전하는 matrix 생성
	 * @param mat [out] \n double 44 \n 회전 matrix의 결과 포인터
	 * @param vec_axis [in] \n double 3 \n 회전축을 정의하는 3D vector를 정의하는 포인터
	 * @param angle_rad [in] \n double \n 회전각을 정의. Radian 단위.
	 * @remarks 좌표계의 방향에 따라 회전 방향이 정해짐 (ex. RHS의 경우 오른나사 방향, LHS의 경우 왼나사 방향)
	*/
	__staticinline void MatrixRotationAxis(d44p mat, const_d3p vec_axis, const double angle_rad);

	/*!
	* @fn void vmmath::MatrixScaling(d44p mat, cd3p scale_factors)
	* @brief 현재 좌표계가 정의하는 축의 방향을 따라 scale 하는 matrix 생성
	* @param mat [out] \n double 44 \n scaling matrix의 결과 포인터
	* @param scale_factors [in] \n double 3 \n (x,y,z) 각 축 방향에 따른 scaling factor를 정의하는 포인터
	* @remarks row major operation
	*/
	__staticinline void MatrixScaling(d44p mat, const_d3p scale_factors);

	/*!
	* @fn void vmmath::MatrixTranslation(d44p mat, cd3p vec_trl)
	* @brief vector 방향을 따라 translation 하는 matrix 생성
	* @param mat [out] \n double 44 \n translation matrix의 결과 포인터
	* @param vec_trl [in] \n double 3 \n (x,y,z) 각 축 방향에 따른 scaling factor를 정의하는 포인터
	* @remarks row major operation
	*/
	__staticinline void MatrixTranslation(d44p mat, const_d3p vec_trl);

	/*!
	* @fn void vmmath::MatrixInverse(d44p mat, cd44p mat_in)
	* @brief Inverse matrix를 생성
	* @param psvxMatrixInv [out] \n double 44 \n inverse matrix의 결과 포인터
	* @param psvxMatrix [in] \n double 44 \n operation의 source인 matrix에 대한 포인터
	* @remarks Determinant 0인 경우 mat_in 의 값을 변환하지 않고 반환
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
	 * @brief 임의의 Data Type에 대해 2D로 메모리를 할당
	 * @param ptr_dst [out] \n void** @n 할당된 메모리에 대한 2D 포인터
	 * @param array_length_2d [in] \n int \n [v][] , y dimension
	 * @param array_sizebytes_1d [in] \n int \n [][v] (bytes), x dimension
	 * @remarks
	 * 할당된 메모리 포인터에 대해 (x, y) 의 indexing 은 (*pppVoidTarget)[y][x]으로 으루어 짐
	*/
	__staticinline void AllocateVoidPointer2D(void*** ptr_dst, const int array_length_2d, const int array_sizebytes_1d);

	/*!
	 * @fn void vxhelpers::GetSystemMemoryInfo(double* free_bytes, double* valid_sysmem_bytes)
	 * @brief 현재 구동되고 있는 OS를 통한 System Memory 상태를 제공
	 * @param free_bytes [out] \n double \n 현재 사용 가능한 메모리 크기 (bytes)
	 * @param valid_sysmem_bytes [out] \n double \n 현재 System 에 인식되는 물리 메모리 크기 (bytes)
	 * @remarks x86 또는 x64, 현재 구동 OS의 상태에 따라 실제 메모리와 다르게 잡힐 수 있음.
	*/
	__staticinline void GetSystemMemoryInfo(double* free_bytes, double* valid_sysmem_bytes);

	/*!
	* @fn void vxhelpers::GetCPUInstructionInfo(int* cpu_info)
	* @brief 현재 CPU가 지원하는 instruction 정보를 제공
	* @param piCPUInstructionInfo [out] \n int \n instruction 정보
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
	 * @brief 현재 시간에 대한 정보를 64bit으로 인코딩하여 제공
	 * @return double \n  0~9 bit : milli-seconds , 10~15 bit : second, 16~21 bit : minute, 22~26 bit : hour,  27~31 bit : day, 32~35 bit : month, 36~65 bit : year
	 * @remarks
	*/
	__staticinline unsigned long long GetCurrentTimePack();
}