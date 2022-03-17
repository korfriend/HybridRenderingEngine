/**
 * @mainpage    VisMotive Framework
 *
 * @section intro 소개
 *      - VisMotive Framework 을 이루는 Global Data Structures, Helper Functions 그리고 Engine APIs를 설명한다.
 *
 * @section CREATEINFO 작성정보
 *      - 작성자      :   김동준
 *      - 작성일      :   2019/7/11
 *      - Contact     :   korfriend@gmail.com
 *
 * @section MODIFYINFO 수정정보
 *      - 2019.7.11    :   이 일짜 소스를 기준으로 Framework Doxygen 0.0.1 최초 작성
 */
 
/**
 * @file VimCommon.h
 * @brief Global Data Structures 및 Classes 그리고 Helper Functions 선언된 파일.
 * @section Include & Link 정보
 *		- Include : VimCommon.h
 *		- Library : CommonUnits.lib
 *		- Linking Binary : CommonUnits.dll
 */
 
#pragma once
 //#define __VERSION "0.x beta" // released at 22.01.10
//#define __VERSION "1.00" // released at 22.02.5
//#define __VERSION "1.01" // released at 22.02.15
#define __VERSION "1.10_under_developing" //

#define _HAS_STD_BYTE 0

#include <map>
#include <vector>
#include <set>
#include <string>
#include <sstream>
#include <math.h>
#include <algorithm>
#include <typeinfo>
#include <typeindex>
#include <any>

#include "VimHelpers.h"

#define __WINDOWS
#define __FILEMAP
#ifdef __WINDOWS
#include <windows.h>
#endif

/**
 * @brief VisMotive Framework
 */

//=====================================================================
// Please, project's character set as UNICODE
// GLM library is used as a common math
// Copyright by DongJoon Kim. All rights reserved.
//=====================================================================


// ONLY FOR WINDOWS VERSION
#define VMENGINEVERSION 0x29AD7‬	// 170711(allocating 20 bits) and  12 bits for modules and engine enhancement version
#define VMSAFE_DELETE(p)	{ if(p) { delete (p); (p)=NULL; } }
#define VMSAFE_DELETEARRAY(p)	{ if(p) { delete[] (p); (p)=NULL; } }
#define VMSAFE_DELETE2DARRAY(pp, numPtrs)	{ if(pp){ for(int i = 0; i < numPtrs; i++){ VMSAFE_DELETEARRAY(pp[i]);} VMSAFE_DELETEARRAY(pp); } }

#ifdef __WINDOWS
	typedef HMODULE VmHMODULE;
#define VMLOADLIBRARY(hModule, filename)     hModule = LoadLibraryA(filename)
#define VMGETPROCADDRESS(pModule, pProcName)    GetProcAddress(pModule, pProcName)
#define VMFREELIBRARY(pModule)    FreeLibrary(pModule)
#endif

#define __vmstatic extern "C" __declspec(dllexport)
#define __vmstaticinline extern "C" __declspec(dllexport) inline
#define __vmstaticclass class __declspec(dllexport)
#define __vmstaticstruct struct __declspec(dllexport)

#define VM_PI 3.14159265358979323846
#define VM_fPI    ((float)  3.141592654f)

#ifndef ushort
	typedef unsigned short ushort;
#endif
#ifndef uint
	typedef unsigned int uint;
#endif
#if !defined(byte)
	//typedef unsigned char byte;
	//#define byte unsigned char
#endif
	//typedef std::byte byte;
#ifndef ullong
	typedef unsigned long long ullong;
#endif
#ifndef llong
	typedef long long llong;
#endif

	// temp typedefs
	// our proj math structures are based on glm::
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
	typedef struct color4 {
		union { struct { byte r, g, b, a; }; struct { byte x, y, z, w; }; };
		color4() { r = g = b = a = (byte)0; };
		color4(byte _r, byte _g, byte _b, byte _a) { r = _r; g = _g; b = _b; a = _a; };
	} vmbyte4;
	typedef struct color3 {
		union { struct { byte r, g, b; }; struct { byte x, y, z; }; };
		color3() { r = g = b = (byte)0; };
		color3(byte _r, byte _g, byte _b) { r = _r; g = _g; b = _b; };
	} vmbyte3;
	typedef struct char2 {
		char x, y; char2() { x = y = 0; }
		char2(char _x, char _y) { x = _x; y = _y; }
	} vmchar2;
	typedef struct byte2 {
		byte x, y; byte2() { x = y = (byte)0; }
		byte2(byte _x, byte _y) { x = _x; y = _y; }
	} vmbyte2;
	typedef struct short2 {
		short x, y; short2() { x = y = 0; }
		short2(short _x, short _y) { x = _x; y = _y; }
	} vmshort2;
	typedef struct ushort2 {
		ushort x, y; ushort2() { x = y = 0; }
		ushort2(ushort _x, ushort _y) { x = _x; y = _y; }
	} vmushort2;
	typedef struct short3 {
		short x, y, z; short3() { x = y = z = 0; }
		short3(short _x, short _y, short _z) {
			x = _x; y = _y; z = _z;
		}
	} vmshort3;
	typedef struct ushort3 {
		ushort x, y, z; ushort3() { x = y = z = 0; }
		ushort3(ushort _x, ushort _y, ushort _z) { x = _x; y = _y; z = _z; }
	} vmushort3;
	typedef glm::ivec2 vmint2;
	typedef glm::ivec3 vmint3;
	typedef glm::ivec4 vmint4;
	typedef glm::uvec2 vmuint2;
	typedef glm::uvec3 vmuint3;
	typedef glm::uvec4 vmuint4;
	typedef glm::dvec2 vmdouble2;
	typedef glm::dvec3 vmdouble3;
	typedef glm::dvec4 vmdouble4;
	typedef glm::fvec2 vmfloat2;
	typedef glm::fvec3 vmfloat3;
	typedef glm::fvec4 vmfloat4;
	typedef glm::dmat4x4 vmmat44;
	typedef glm::fmat4x4 vmmat44f;

#define __VMCVT3__(d, s, t3, tt) d=t3((tt)s.x, (tt)s.y, (tt)s.z)
#define __OPS__(d, s, op) d=op(d, s)

//=====================================
// Global Enumerations
//=====================================

/**
 * @package vmenums
 * @brief Common Data Structure로 사용되는 enumeration 을 모은 namespace
 */
namespace vmenums {
	/*! Framework에서 지원하는 변환 좌표계의 종류, 4x4 matrix가 정의하는 Projecting 변환에서 닫혀 있음 */
	enum EvmCoordSpace {
		CoordSpaceSCREEN = 0,/*!< Pixel로 정의되는 Screen Space */
		CoordSpacePROJECTION,/*!< Normalized Frustrum으로 정의되는 Projection Space */
		CoordSpaceCAMERA,/*!< Viewing Frustrum으로 정의되는 Camera Space */
		CoordSpaceWORLD,/*!< 객체가 실제로 배치되는 World Space */
		CoordSpaceOBJECT/*!< 객체가 정의되는 Object Space */
	};

	/*! Camera의 초기 States(위치, View 및 Up Vector)에 대한 종류 */
	enum EvmStageViewType {
		StageViewORTHOBOXOVERVIEW = 0,/*!< 3D View에서 OS의 Object Bounding Box의 대각선 방향에 대한 Overview를 정의 */
		StageViewCENTERFRONT,/*!< 단면 영상에서 OS의 Object Bounding Box의 기준 Front (or Coronal) View 정의 */
		StageViewCENTERRIGHT,/*!< 단면 영상에서 OS의 Object Bounding Box의 기준 Right (or Sagittal) View 정의 */
		StageViewCENTERHORIZON/*!< 단면 영상에서 OS의 Object Bounding Box의 기준 Top (or Axial) View 정의 */
	};

	/*! Polygonal VObject를 정의하는 Primitive 종류 */
	enum EvmPrimitiveType {
		PrimitiveTypeUNDEFINED = 0,/*!< Undefined */
		PrimitiveTypeLINE,/*!< Line */
		PrimitiveTypeTRIANGLE,/*!< Triangle */
		PrimitiveTypePOINT/*!< Point */
	};

	/*! VObject 의 Element 단위의 Bounding Unit 의 종류 */
	enum EvmBoundingUnitType {
		BoundingUnitTypeOBB = 0,/*!< OBB */
		BoundingUnitTypeAABB,/*!< AABB */
		BoundingUnitTypeSPHERE,/*!< Sphere */
	};

	/*! Module-Platform interoperation에 사용되는 VmObject의 type 종류 */
	enum EvmObjectType {
		ObjectTypeOBJECT = 0,/*!< Just an Object for archiving something */
		ObjectTypeVOLUME,/*!< Volume Object */
		ObjectTypePRIMITIVE,/*!< Polygon Object */
		ObjectTypeIMAGEPLANE/*!< Image Plane을 정의하는 VmObject이며 Camera Object를 갖음 */
	};

	/*! VmIObject에서 사용하는 Frame Buffer 종류 */
	enum EvmFrameBufferUsage {
		FrameBufferUsageNONE = 0,/*!< Undefined, There is no allocated frame buffer */
		// Render //
		FrameBufferUsageRENDEROUT,/*!< Used for rendering out buffer, the buffer should have vmbyte4 as data type */
		// Depth //
		FrameBufferUsageDEPTH,/*!< Used for depth buffer, the buffer should have vmfloat4 as data type */
		// Custom //
		FrameBufferUsageCUSTOM,/*!< Used for customized purpose, the buffer may have any type */
		FrameBufferUsageALIGNEDSTURCTURE,/*!< custom defined bytes, defined as custom structure, aligned by 16 bytes, the buffer may have any type */
		FrameBufferUsageVIRTUAL,/*!< Used for customized purpose, the buffer is NULL */
	};
}

/**
* @class LocalProgress
* @brief Framework에서 정의하는 progress 에 대한 자료구조
*/
struct LocalProgress {
	/**
	 * @brief progress의 시작, 0.0 에서 100.0 사이
	 */
	double start;
	/**
	 * @brief progress의 범위, 0.0 에서 100.0 사이
	 */
	double range;
	/**
	 * @brief progress의 현재 진행 정도가 기록되는 module/function 내의 static parameter 에 대한 포인터
	 */
	double* progress_ptr; /*out*/
	/// constructor, 초기화
	LocalProgress()
	{
		start = 0;
		range = 100;
		progress_ptr = NULL;
	}

	/*!
	 * @fn void vmobjects::LocalProgress::Init()
	 * @brief >> *progress_ptr = start;
	 */
	void Init()
	{
		if (!progress_ptr) return;
		*progress_ptr = start;
	}

	/*!
	 * @fn void vmobjects::LocalProgress::SetProgress(double dProgress, double dTotal)
	 * @brief >> *progress_ptr = start + range * progress / total;
	 */
	void SetProgress(const double progress, const double total)
	{
		if (!progress_ptr) return;
		*progress_ptr =
			start + range * progress / total;
	}

	/*!
	 * @fn void vmobjects::SVXLocalProgress::Deinit()
	 * @brief >> dStartProgress = *pdProgressOfCurWork;
	 */
	void Deinit()
	{
		if (!progress_ptr) return;
		start = *progress_ptr;
	}
};

typedef void(*VmDelegate)(void* pv);

/**
 * @package vmobjects
 * @brief Framework의 Global Data Structures 및 VmObject Classes를 모은 namespace
 */
namespace vmobjects
{
	using namespace vmenums;
	//=========================
	// Object Structures
	//=========================

	template <typename NAME, typename ANY> struct VmParamMap {
	private:
		std::string __PM_VERSION = "LIBI_1.2";
		std::map<NAME, ANY> __params;
	public:
		template <typename SRCV> bool GetParamCheck(const NAME& param_name, SRCV& param) {
			auto it = __params.find(param_name);
			if (it == __params.end()) return false;
			param = std::any_cast<SRCV&>(it->second);
			return true;
		}
		template <typename SRCV> SRCV GetParam(const NAME& param_name, const SRCV& init_v) {
			auto it = __params.find(param_name);
			if (it == __params.end()) return init_v;
			return std::any_cast<SRCV&>(it->second);
		}
		template <typename SRCV> SRCV* GetParamPtr(const NAME& param_name) {
			auto it = __params.find(param_name);
			if (it == __params.end()) return NULL;
			return (SRCV*)&std::any_cast<SRCV&>(it->second);
		}
		template <typename SRCV, typename DSTV> bool GetParamCastingCheck(const NAME& param_name, DSTV& param) {
			auto it = __params.find(param_name);
			if (it == __params.end()) return false;
			param = (DSTV)std::any_cast<SRCV&>(it->second);
			return true;
		}
		template <typename SRCV, typename DSTV> DSTV GetParamCasting(const NAME& param_name, const DSTV& init_v) {
			auto it = __params.find(param_name);
			if (it == __params.end()) return init_v;
			return (DSTV)std::any_cast<SRCV&>(it->second);
		}
		void SetParam(const NAME& param_name, const ANY& param) {
			__params[param_name] = param;
		}
		void RemoveParam(const NAME& param_name) {
			auto it = __params.find(param_name);
			if (it != __params.end()) {
				__params.erase(it);
			}
		}
		void RemoveAll() {
			__params.clean();
		}
		size_t Size() {
			return __params.size();
		}
		std::string GetPMapVersion() {
			return __PM_VERSION;
		}

		typedef std::map<NAME, ANY> MapType;
		typename typedef MapType::iterator iterator;
		typename typedef MapType::const_iterator const_iterator;
		typename typedef MapType::reference reference;
		iterator begin() { return __params.begin(); }
		const_iterator begin() const { return __params.begin(); }
		iterator end() { return __params.end(); }
		const_iterator end() const { return __params.end(); }
	};

	struct data_type {
		std::string type_name; // <typeinfo>
		size_t type_hash;
		size_t type_bytes;
		data_type() {
			type_name = ""; type_hash = 0; type_bytes = 0;
		}
		data_type(const std::type_info& info, size_t type_size) {
			type_name = info.name(); type_hash = info.hash_code(); type_bytes = type_size;
		};
		template<typename T>
		static data_type dtype() {
			data_type d(typeid(T), sizeof(T));
			return d;
		};
		bool operator == (data_type other) const
		{
			return type_hash == other.type_hash;
		}
		bool operator != (data_type other) const
		{
			return type_hash != other.type_hash;
		}
	};
	/**
	 * @class AaBbMinMax
	 * @brief 현재 좌표계의 Axis에 평행한 Box를 정의하는 자료구조
	 */
	struct AaBbMinMax {
		/// 현재 좌표계의 Axis에 평행한 Box에 대한 최소점, 최대점 위치
		vmdouble3 pos_min, pos_max;
		/// constructor, 모두 0 (NULL or false)으로 초기화
		AaBbMinMax() { }
		AaBbMinMax(vmint3 vol_size) {
			pos_min = vmdouble3(-0.5, -0.5, -0.5);
			vmint3 idx_max = vol_size - vmint3(1, 1, 1);
			pos_max = vmdouble3((double)idx_max.x, (double)idx_max.y, (double)idx_max.z) + vmdouble3(0.5, 0.5, 0.5);

			assert(IsAvailableBox());
		}
		/// 현재 좌표계의 AaBbMinMax 가 유효하게 정의되었는가 확인
		bool IsAvailableBox() const {
			if (pos_max.x <= pos_min.x || pos_max.y <= pos_min.y || pos_max.z <= pos_min.z)
				return false;
			return true;
		}
	};

	/**
	 * @class AxisInfoOS2WS
	 * @brief Object Space 정의된 x축(1,0,0), y축(0,1,0), z축(0,0,z)이 처음 World Space 에 배치될 때의 방향을 정의 \n
	 * pitch 는 고려되지 않으며 방향만 정의, (즉 vector 에 대해서만 유효함)
	 * @sa
	 * @ref vmobjects::VolumeData
	 */
	struct AxisInfoOS2WS {
		/**
		 * @brief Object Space 정의된 x축(1,0,0)에 대응되는 World Space 상에 배치된 Object의 x축을 정의, unit vector
		 */
		vmdouble3 vec_axisx_ws;
		/**
		 * @brief Object Space 정의된 y축(0,1,0)에 대응되는 World Space 상에 배치된 Object의 y축을 정의, unit vector
		 */
		vmdouble3 vec_axisy_ws;
		/**
		 * @brief Object Space 정의된 z축(0,0,1)에 대응되는 World Space 상에 배치된 Object의 z축을 정의하기 위한 XY RHS cross vecor 방향의 reverse 여부\n
		 * true 면 RHS 로 배치되며 Affine Space 에서 변환 성립, false 면 LHS 로 z툭이 배치
		 */
		bool is_rhs;
		/**
		 * @brief vec_axisx_ws, vec_axisy_ws, is_rhs 로부터 정의되는 초기 OS2WS 변환 행렬
		 */
		vmmat44 mat_os2ws;
		/**
		 * @brief constructor, 초기화 작업 수행
		 * @details
		 * >> vec_axisx_ws = (1, 0, 0);
		 * >> vec_axisy_ws = (0, 1, 0);
		 * >> RHS;
		 * >> mat_os2ws is identity matrix
		 */
		AxisInfoOS2WS()
		{
			vec_axisx_ws = vmdouble3(1, 0, 0);
			vec_axisy_ws = vmdouble3(0, 1, 0);
			is_rhs = true;
			ComputeInitalMatrix();
		}
		AxisInfoOS2WS(vmdouble3 _vec_axisx_ws, vmdouble3 _vec_axisy_ws, bool _is_rhs)
		{
			vec_axisx_ws = _vec_axisx_ws;
			vec_axisy_ws = _vec_axisy_ws;
			is_rhs = _is_rhs;
			ComputeInitalMatrix();
		}
		/// 정의된 vec_axisx_ws와 vec_axisy_ws로부터 mat_os2ws 계산하여 등록
		void ComputeInitalMatrix() {
			vmdouble3 z_vec_rhs;
			vmmath::CrossDotVector(&z_vec_rhs, &vec_axisy_ws, &vec_axisx_ws);	// this is for -z direction
			vmmat44 matT;
			vmmath::MatrixWS2CS(&matT, &vmdouble3(0, 0, 0), &vec_axisy_ws, &z_vec_rhs);
			vmmath::MatrixInverse(&mat_os2ws, &matT);
			if (!is_rhs)
			{
				vmmat44 matInverseZ;
				vmmath::MatrixScaling(&matInverseZ, &vmdouble3(1., 1., -1.));
				mat_os2ws = mat_os2ws * matInverseZ;
			}
		}
	};

	/**
	 * @class VolumeData
	 * @brief Framework에서 정의하는 Volume의 상세 정보를 위한 자료구조
	 * @sa
	 * @ref vmobjects::VmVObjectVolume \n
	 */
	struct VolumeData {
		/**
		 * @brief Volume array 의 data type, <typeinfo>
		 */
		data_type store_dtype;
		/**
		 * @brief 메모리에 저장되기 전 Volume Original data type 
		 */
		data_type origin_dtype;
		/**
		 * @brief Volume을 저장한 2D array (safe sample version)
		 * @details
		 * 실제 할당된 x 축 방향 크기 = i3VolumeSize.x + i3SizeExtraBoundary.x*2 \n
		 * 실제 할당된 y 축 방향 크기 = i3VolumeSize.y + i3SizeExtraBoundary.y*2 \n
		 * 실제 할당된 z 축 방향 크기 = i3VolumeSize.z + i3SizeExtraBoundary.z*2 \n
		 * @par ex.
		 * ushort 512x512x512 Volume에서 (100, 120, 150) index 값 sample \n
		 * @par
		 * >> int iSamplePosX = 100 + i3SizeExtraBoundary.x; \n
		 * >> int iSamplePosY = 120 + i3SizeExtraBoundary.y; \n
		 * >> int iSamplePosZ = 150 + i3SizeExtraBoundary.z; \n
		 * >> ushort usValue = ((ushort**)ppvVolumeSlices)[iSamplePosZ][iSamplePosX + iSamplePosY*(i3VolumeSize.x + i3SizeExtraBoundary.x*2)];
		 */
		void** vol_slices;
		/**
		 * @brief CPU Memory Access Violation을 피하기 위해 System Memory 상의 Extra Boundary 영역의 한쪽면 크기
		 * @details bnd_size = (한쪽 x축 방향 크기, 한쪽 y축 방향 크기, 한쪽 z축 방향 크기)
		 */
		vmint3 bnd_size;
		/**
		 * @brief Volume 의 크기 vol_size = (width, height, depth or slices)
		 * @details bnd_size 가 포함되지 않음
		 */
		vmint3 vol_size;
		/**
		 * @brief 단위 Voxel에 대해 OS 상 cell 의 WS 상의 크기
		 * @details vox_pitch = (OS 상의 x 방향 voxel 크기, OS 상의 y 방향 voxel 크기, OS 상의 z 방향 voxel 크기)
		 */
		vmdouble3 vox_pitch;
		/**
		 * @brief Volume으로 저장된(ppvVolumeSlices)의 최소값 store_Mm_values.x, 최대값 store_Mm_values.y
		 */
		vmdouble2 store_Mm_values;
		/**
		 * @brief Volume으로 저장되기 전에 정의된 최소값 actual_Mm_values.x, 최대값 actual_Mm_values.y
		 * @par ex.
		 * file format float으로 -1.5 ~ 2.5 저장된 볼륨을 ushort 으로 저장할 경우
		 * @par
		 * >> store_Mm_values = vmdouble2(0, 65535), actual_Mm_values = vmdouble2(-1.5, 2.5);
		 */
		vmdouble2 actual_Mm_values;
		/**
		 * @brief Volume에 대한 Histogram 을 정의하는 array
		 * @details
		 * array 크기는 uint(d2MinMaxValue.y - d2MinMaxValue.x + 1.5) \n
		 * pullHistogram[volume value] = # of voxels
		 */
		ullong* histo_values;
		/**
		 * @brief memory 에 저장된 volume space (샘플 좌표)와 초기 world space 에 배치되는 변환 matrix 정의
		 */
		AxisInfoOS2WS axis_info;
		/**
		 * @brief constructor, 초기화 작업 수행
		 */
		VolumeData() {
			vol_size = vox_pitch = bnd_size = vmdouble3(0);
			store_dtype = data_type(typeid(void), 0);
			origin_dtype = data_type(typeid(void), 0);

			store_Mm_values = actual_Mm_values = vmdouble2(DBL_MAX, -DBL_MAX);
			vol_slices = NULL;
			histo_values = NULL;
		}

		/**
		 * @brief Histogram array 의 크기를 얻음, uint(store_Mm_values.y - store_Mm_values.x + 1.5)
		 */
		uint GetHistogramSize() { return (uint)((double)__max(store_Mm_values.y - store_Mm_values.x + 1.5, 1.0)); }
		/**
		 * @brief ppvVolumeSlices array 크기를 얻음, Extra Boundary가 적용된 크기
		 */
		vmint3 GetSampleSize() { return vmint3(vol_size.x + bnd_size.x * 2, vol_size.y + bnd_size.y * 2, vol_size.z + bnd_size.z * 2); }

		// 포인터 ppvVolumeSlices 및 pullHistogram 에 할당된 메모리 해제
		void Delete() {
			VMSAFE_DELETE2DARRAY(vol_slices, vol_size.z + bnd_size.z * 2);
			VMSAFE_DELETEARRAY(histo_values);
		}
	};

	/**
	 * @class PrimitiveData
	 * @brief Framework 에서 정의하는 Primitive 로 이루어진 객체의 상세 정보를 위한 자료구조
	 * @sa vmobjects::VmVObjectPrimitive
	 */
	struct PrimitiveData {
	private:
		/**
		 * @brief vertex array를 저장하는 container map
		 * string ==> POSITION, NORMAL, TEXCOORD[n], ...
		 * memory 할당된 pointer를 value로 갖고, @ref PrimitiveData::Delete 에서 해제됨.
		 */
		std::map<std::string, vmfloat3*> defined_buffers;
	public:
		/**
		 * @brief Primitive로 구성 된 객체의 Polygon 의 normal vector 기준의 vertex 배열 방향
		 */
		bool is_ccw;	// will be deprecated
		/**
		 * @brief Primitive Type
		 */
		EvmPrimitiveType ptype;
		/**
		 * @brief Primitive를 정의하는 vertex의 배열 기준, true : stripe, false : list
		 */
		bool is_stripe;
		/**
		* @brief Primitive를 정의하는 vertex 및 edge 의 중복성을 없앴는가의 유무
		*/
		bool check_redundancy;
		/**
		 * @brief Primitive로 구성 된 객체의 Polygon 개수
		 */
		uint num_prims;
		/**
		 * @brief 하나의 Primitive(Polygon)을 정의하는 index 개수
		 */
		uint idx_stride;
		/**
		 * @brief Vertex의 Index기반으로 Polygon을 정의하기 위한 Index Buffer (puiIndexList) 의 크기
		 * @details
		 * >> if(is_stripe)\n
		 * >>    num_vidx = num_prims + (idx_stride - 1);\n
		 * >> else\n
		 * >>    num_vidx = num_prims * idx_stride;
		 */
		uint num_vidx;
		/**
		 * @brief Vertex의 Index 기반으로 Polygon를 위한 Index Buffer가 정의된 array
		 */
		uint* vidx_buffer;
		/**
		 * @brief Primitive로 구성 된 객체의 Vertex 개수
		 */
		uint num_vtx;
		/**
		 * @brief PrimitiveData 단위의 OS 상에서 정의되는 bounding box
		 */
		AaBbMinMax aabb_os;
		/**
		 * @brief Texture Resource 에 대한 정보 \n
		 * <w, h, bytes_stride, res_ptr>
		 */
		std::map<std::string, std::tuple<int, int, int, byte*>> texture_res_info;

		bool GetTexureInfo(const std::string& desc, int& w, int& h, int& bytes_stride, byte** res_ptr)
		{
			auto it = texture_res_info.find(desc);
			if (it == texture_res_info.end()) return false;
			auto tx_res = it->second;
			w = std::get<0>(tx_res);
			h = std::get<1>(tx_res);
			bytes_stride = std::get<2>(tx_res);
			*res_ptr = std::get<3>(tx_res);
			return true;
		}

		/// constructor, 모두 0 (NULL or false)으로 초기화
		PrimitiveData() {
			is_ccw = true; num_prims = 0; num_vtx = 0;
			idx_stride = num_vidx = 0;
			ptype = PrimitiveTypeUNDEFINED;
			is_stripe = false;
			check_redundancy = false;
			vidx_buffer = NULL;
		}
		/*!
		 * @fn void vmobjects::PrimitiveData::Delete()
		 * @brief 포인터 puiIndexList 및 defined_buffers 의 value로 정의된 pointer 에 할당된 메모리 해제
		*/
		void Delete() {
			VMSAFE_DELETEARRAY(vidx_buffer);
			for (auto it = texture_res_info.begin(); it != texture_res_info.end(); it++)
			{
				byte* p = std::get<3>(it->second);
				VMSAFE_DELETEARRAY(p);
			}
			texture_res_info.clear();

			for (std::map<std::string, vmfloat3*>::iterator itrVertex3D = defined_buffers.begin(); itrVertex3D != defined_buffers.end(); itrVertex3D++)
			{
				VMSAFE_DELETEARRAY(itrVertex3D->second);
			}
			defined_buffers.clear();
		}
		/*!
		 * @fn vmfloat3* vmobjects::PrimitiveData::GetVerticeDefinition(const string& vtype)
		 * @brief defined_buffers 의 value로 정의된 pointer 반환하는 method,
		 * @param vtype [in] \n string \n vertex buffer의 이름, (defined_buffers의 key) \n
		 * keys : POSITION, NORMAL, TEXCOORD[n], ...
		 * @return vmfloat3 \n vertex buffer 포인터. 없으면 NULL 반환
		 */
		vmfloat3* GetVerticeDefinition(const std::string& vtype) {
			std::map<std::string, vmfloat3*>::iterator itrVtxDef = defined_buffers.find(vtype);
			if (itrVtxDef == defined_buffers.end())
				return NULL;
			return itrVtxDef->second;
		}
		/*!
		 * @fn void vmobjects::PrimitiveData::ReplaceOrAddVerticeDefinition(const string& vtype, vmfloat3* vtx_buffer)
		 * @brief defined_buffers 의 value로 정의된 pointer 반환하는 method,
		 * @param vtype [in] \n string \n vertex buffer의 이름, POSITION, NORMAL, TEXCOORD[n], ...
		 * @param vtx_buffer [in] \n vmfloat3* \n vertex buffer를 정의하는 vmfloat3의 포인터.
		 * @remarks string vtype 을 키로 갖기 때문에 이미 등록된 vertex definition 이 있을 경우, \n
		 * 해당 definition 의 vertex pointer 를 메모리에서 해제하고 새로운 vertex pointer 를 등록
		 */
		void ReplaceOrAddVerticeDefinition(const std::string& vtype, vmfloat3* vtx_buffer) {
			vmfloat3* vtx_buffer_old = GetVerticeDefinition(vtype);
			if (vtx_buffer_old != NULL)
			{
				VMSAFE_DELETEARRAY(vtx_buffer_old);
				defined_buffers.erase(vtype);
			}
			defined_buffers.insert(std::pair<std::string, vmfloat3*>(vtype, vtx_buffer));
		}
		/*!
		 * @fn int vmobjects::PrimitiveData::GetNumVertexDefinitions()
		 * @brief 등록된 vertex definition 의 개수를 얻음
		 * @return int \n 등록된 vertex definition 의 개수
		 */
		int GetNumVertexDefinitions() const
		{
			return (int)defined_buffers.size();
		}
		/*!
		 * @fn void vmobjects::PrimitiveData::ClearVertexDefinitionContainer()
		 * @brief mapVerticeDefinitions 를 clear 함.
		 * @remarks 등록된 vertex pointer 에 대한 메모리 해제는 하지 않고 container 만 clear 함.
		 */
		void ClearVertexDefinitionContainer()
		{
			defined_buffers.clear();
		}
		/*!
		* @fn void vmobjects::PrimitiveData::ComputeOrthoBoundingBoxWithCurrentValues()
		* @brief defined_buffers 에 등록된 POSITION vtx_buffer 의 position 을 기반으로 AABB min/max 를 구함.
		* @remarks 이미 PrimitiveData 의 aabb_os 를 계산했으면 하지 않아도 되나, 그렇지 않으면 반드시 이를 실행하여 AABB 를 정의해야 함.
		*/
		void ComputeOrthoBoundingBoxWithCurrentValues()
		{
			vmfloat3* vtx_buffer = GetVerticeDefinition("POSITION");
			assert(vtx_buffer != NULL && num_vtx > 0);

			aabb_os.pos_min = vmfloat3(DBL_MAX, DBL_MAX, DBL_MAX);
			aabb_os.pos_max = vmfloat3(-DBL_MAX, -DBL_MAX, -DBL_MAX);
			for (int j = 0; j < (int)num_vtx; j++)
			{
				const vmfloat3& p = vtx_buffer[j];
				vmdouble3 _p;
				__VMCVT3__(_p, p, vmdouble3, double);
				__OPS__(aabb_os.pos_min.x, _p.x, __min);
				__OPS__(aabb_os.pos_min.y, _p.y, __min);
				__OPS__(aabb_os.pos_min.z, _p.z, __min);
				__OPS__(aabb_os.pos_max.x, _p.x, __max);
				__OPS__(aabb_os.pos_max.y, _p.y, __max);
				__OPS__(aabb_os.pos_max.z, _p.z, __max);
			}
		}
	};

	/**
	 * @class TMapData
	 * @brief Framework에서 정의하는 OTF에 대한 상세 정보를 위한 자료구조
	 * @sa vmobjects::VmTObject
	 */
	struct MapTable {
		/**
		 * @brief OTF array 의 pointer
		 * @details
		 * 1D : [0][0 to (array_lengths.x - 1)] - default, [1][0 to (array_lengths.x - 1)] - customized
		 * 2D : [0][0 to (array_lengths.x*array_lengths.y - 1)] - default, [...][0 to (array_lengths.x*array_lengths.y - 1)] - customized
		 * 3D : [0 to (array_lengths.z - 1)][0 to (array_lengths.x*array_lengths.y - 1)]
		 */
		void** tmap_buffers;
		/**
		 * @brief OTF array 의 pointer dimension
		 * @details num_dim = 1 or 2 or 3
		 */
		int num_dim;
		/**
		 * @brief OTF array 에 할당되어 있는 각각의 dimension에 대한 OTF array index 중 최소값
		 * @details
		 * valid_min_idx.x : 1st dimension 의 array index 의 최소값 \n
		 * valid_min_idx.y : 2nd dimension 의 array index 의 최소값 \n
		 * valid_min_idx.z : 3rd dimension 의 array index 의 최소값
		 */
		vmint3 valid_min_idx;
		/**
		 * @brief OTF array 에 할당되어 있는 각각의 dimension에 대한 OTF array 값 중 최대값
		 * @details
		 * valid_max_idx.x : 1st dimension 의 array 의 최대값 \n
		 * valid_max_idx.y : 2nd dimension 의 array 의 최대값 \n
		 * valid_max_idx.z : 3rd dimension 의 array 의 최대값
		 */
		vmint3 valid_max_idx;
		/**
		 * @brief OTF array 의 각 dimension 에 대한 크기
		 * @details
		 * array_lengths.x : 1st dimension 의 array 크기 \n
		 * array_lengths.y : 2nd dimension 의 array 크기 \n
		 * array_lengths.z : 3rd dimension 의 array 크기 \n
		 * Valid dimension에 대하여 array_lengths.xyz > 0, Invalid demension에 대하여 array_lengths.xyz <= 0
		 */
		vmint3 array_lengths;
		/**
		 * @brief OTF metric의 기준이 되는 Volume 값의 범위에 대한 bin 크기
		 * @par ex.
		 * 16 bit Volume Data에 대한 2D OTF (density, gradient magnitude), 각각의 범위를 0~65535 라고 가정할 때 \n
		 * 512x1024 크기의 2D OTF 를 정의하면, bin 의 XY 크기는 (65536/512, 65536/1024) 가 됨.
		 */
		vmdouble3 bin_size;
		/**
		 * @brief OTF array 값에 대한 data type
		 */
		data_type dtype;
		/**
		 * @brief constructor, 모두 0 (NULL or false)으로 초기화
		 */
		MapTable() {
			tmap_buffers = NULL;
			num_dim = 0;
			valid_min_idx = valid_max_idx = bin_size = vmdouble3(0);
			array_lengths = vmint3(0);
		}

		// Static Helper Functions //
		/*!
		 * @brief VolumeData 에 저장되는 OTF array를 할당하는 static helper 함수
		 * @param num_dim [in] \n int \n OTF dimension
		 * @param dim_length [in] \n int 3 \n OTF dimension 의 크기
		 * @param dtype [in] \n data_type \n OTF array의 data type
		 * @param res_tmap [in] \n void \n 2D OTF array에 대한 void**의 포인터 (3D 포인터)
		 * @return bool \n 성공하면 true, 실패하면 false 반환
		 * @remarks OTF array 는 항상 2D OTF로 저장됨
		 * @sa vmobjects::TMapData
		 */
		bool CreateTMapBuffer(const int num_dim, const vmint3& dim_length)
		{
			if (num_dim <= 0 || num_dim > 3)
			{
				printf("TMapData::CreateTMapBuffer - UNAVAILABLE INPUT");
				return false;
			}

			switch (dtype.type_bytes)
			{
			case 1:
			case 2:
				if (dim_length.x <= 0 || dim_length.y <= 0)
				{
					printf("TMapData::CreateTMapBuffer - Type Error 2");
					return false;
				}
				vmhelpers::AllocateVoidPointer2D(&tmap_buffers, dim_length.y, dtype.type_bytes * dim_length.x);
				break;
			case 3:
				if (dim_length.x <= 0 || dim_length.y <= 0 || dim_length.z <= 0)
				{
					printf("TMapData::CreateTMapBuffer - Type Error 2");
					return false;
				}
				vmhelpers::AllocateVoidPointer2D(&tmap_buffers, dim_length.z, dtype.type_bytes * dim_length.x * dim_length.y);
				break;
			default:
				printf("TMapData::CreateTMapBuffer - UNAVAILABLE INPUT");
				return false;
			}

			return true;
		}

		/*!
		 * @fn void vmobjects::TMapData::Delete()
		 * @brief OTF array 에 대한 포인터 ppvArchiveTF 에 할당된 memory 해제
		*/
		void Delete() {
			switch (num_dim)
			{
			case 1:
			case 2:
				VMSAFE_DELETE2DARRAY(tmap_buffers, array_lengths.y);
				break;
			case 3:
				VMSAFE_DELETE2DARRAY(tmap_buffers, array_lengths.z);
				break;
			default:
				break;
			}
		}
	};

	/**
	 * @class VolumeBlocks
	 * @brief block 기반의 volume에 대한 자료구조
	 * @sa vmobjects::VmVObjectVolume, vmobjects::VolumeData
	 */
	struct VolumeBlocks {
		/**
		 * @brief 단위 block의 크기
		 * @details extra boundary가 포함되지 않은 크기 \n
		 * unitblk_size = vmint3(size of x, size of y, size of z)
		 */
		vmint3 unitblk_size;
		/**
		* @brief block 정보를 저장하는 mM_blks 및 pbTaggedActivatedBlocks 의 extra boundary 의 크기
		* @details 이를 고려하여 mM_blks 및 pbTaggedActivatedBlocks 의 값을 샘플해야 함 \n
		*/
		vmint3 blk_bnd_size;
		/**
		 * @brief 하나의 VolumeBlocks 에 각 축 방향으로 정의된 block들의 개수
		 * @details 모든 block의 개수 = blk_vol_size.x * blk_vol_size.y * blk_vol_size.z;
		 */
		vmint3 blk_vol_size;
		/**
		 * @brief block 의 최소 최대 값에 대한 단일 channel data type
		 * @details 일반적으로 vmobjects::VolumeData.store_dtype 와 같음
		 */
		data_type dtype;
		/**
		 * @brief block 단위의 최소 최대값을 저장하는 1D array
		 * @details
		 * array의 크기는 block의 전체 개수와 동일, extra boundary을 고려하지 않음 \n
		 * data type은 2 channel을 갖고 volume type과 같음. x : 최소값, y : 최대값
		 * @par ex.
		 * 8x8x8 block을 단위로 정의된 512x512x512 volume (ushort) 에서 OS의 (100, 100, 100) 좌표에 해당하는 block의 최소 최대값 \n
		 * 이 경우 unitblk_size = vmint3(8, 8, 8), blk_vol_size = (ceil(512/8), ceil(512/8), ceil(512/8)) \n
		 *
		 * @par
		 * >> vmint3 blk_id = vmint3(floor(100/8), floor(100/8), floor(100/8)); \n
		 * >> int blk_idx = blk_id.x + blk_bnd_size.x
		 * >>                     + (blk_id.y + blk_bnd_size.y)*(blk_id.x + 2*blk_bnd_size.x)
		 * >>                     + (blk_id.z + blk_bnd_size.z)*(blk_id.x + 2*blk_bnd_size.x)*(blk_id.y + 2*blk_bnd_size.y); \n
		 * >> vmushort2 mM = ((vmushort2*)mM_blks)[iBlockIdIndex];
		 */
		void* mM_blks;
		/**
		 * @brief Object 별로 정의된, block 단위로 binary tag가 지정된 1D array
		 * @details
		 * array의 크기는 block의 전체 개수와 동일, extra boundary을 고려하지 않음 \n
		 * resource manager 에서 TObject 삭제 시, 등록된 Volume Object 의 Block 들에 대해 resouece 정리 수행 \n
		 * VolumeBlocks::GetTaggedActivatedBlocks 와 VolumeBlocks::GetTaggedActivatedBlocks 로 Pointer 작업 가능 \n
		 * @par ex.
		 * 8x8x8 block을 단위로 정의된 512x512x512 volume (ushort) 에서 OS의 (100, 100, 100) 좌표에 해당하는 block 의 tag \n
		 * 이 경우 unitblk_size = vmint3(8, 8, 8), blk_vol_size = (ceil(512/8), ceil(512/8), ceil(512/8)) \n
		 *
		 * @par
		 * >> byte* tflag_blks = itratorMap->second;
		 * >> vmint3 blk_id = vmint3(floor(100/8), floor(100/8), floor(100/8)); \n
		 * >> int blk_idx = blk_id.x + blk_bnd_size.x
		 * >>                     + (blk_id.y + blk_bnd_size.y)*(blk_id.x + 2*blk_bnd_size.x)
		 * >>                     + (blk_id.z + blk_bnd_size.z)*(blk_id.x + 2*blk_bnd_size.x)*(blk_id.y + 2*blk_bnd_size.y); \n
		 * >> byte tflag = tflag_blks[blk_idx];
		 */
		std::map<int, byte*> tflag_blks_map;
		std::map<int, ullong> updatetime_map;

		/// constructor, 모두 0 (NULL or false)으로 초기화
		VolumeBlocks() {
			unitblk_size = blk_vol_size = blk_bnd_size = vmint3(0);
			mM_blks = NULL;
		}

		/*!
		 * @fn void vmobjects::VolumeBlocks::Delete()
		 * @brief 할당된 memory 모두 해제
		*/
		void Delete() {
			VMSAFE_DELETEARRAY(mM_blks);
			for (std::map<int, byte*>::iterator itr = tflag_blks_map.begin(); itr != tflag_blks_map.end(); itr++)
				VMSAFE_DELETEARRAY(itr->second);
			tflag_blks_map.clear();
			updatetime_map.clear();
		}

		/*!
		* @fn void vmobjects::VolumeBlocks::GetTaggedActivatedBlocks(int iTObjectID)
		* @brief 해당 TObjectID 의 Tagged Activated Blocks (byte* tflag_blks) 포인터를 받음
		*/
		byte* GetTaggedActivatedBlocks(int tobj_id)
		{
			std::map<int, byte*>::iterator itr = tflag_blks_map.find(tobj_id);
			if (itr == tflag_blks_map.end())
				return NULL;
			return itr->second;
		}

		ullong GetUpdateTime(int tobj_id)
		{
			std::map<int, ullong>::iterator itr = updatetime_map.find(tobj_id);
			if (itr == updatetime_map.end())
				return 0;
			return itr->second;
		}

		/*!
		* @fn void vmobjects::VolumeBlocks::ReplaceOrAddTaggedActivatedBlocks(int tobj_id, byte* tflag_blks)
		* @brief 해당 TObjectID 에 대한 TaggedActivatedBlocks 을 등록
		*/
		bool ReplaceOrAddTaggedActivatedBlocks(const int tobj_id, byte* tflag_blks)
		{
			std::map<int, byte*>::iterator itr = tflag_blks_map.find(tobj_id);
			if (itr != tflag_blks_map.end())
				VMSAFE_DELETEARRAY(itr->second);

			tflag_blks_map[tobj_id] = tflag_blks;
			return true;
		}

		/*!
		* @fn void vmobjects::VolumeBlocks::DeleteTaggedActivatedBlocks()
		* @brief 해당 TObjectID 의 TaggedActivatedBlocks 를 삭제
		*/
		void DeleteTaggedActivatedBlocks(const int tobj_id)
		{
			std::map<int, byte*>::iterator itr = tflag_blks_map.find(tobj_id);
			if (itr != tflag_blks_map.end())
			{
				VMSAFE_DELETEARRAY(itr->second);
				tflag_blks_map.erase(itr);
			}
		}

		static void ComputeOctreeBlockSize(const vmint3& vol_size, vmint3* ublk0_size, vmint3* ublk1_size)
		{
			int max_size = __max(__max(vol_size.x, vol_size.y), vol_size.z);
			int size_blk_max = __max((int)pow(2.0, floor((log((double)max_size / 16.0) / log(2.0)))), (int)8);
			ublk0_size->x = size_blk_max;
			ublk0_size->y = size_blk_max;
			ublk0_size->z = size_blk_max;
			ublk1_size->x = size_blk_max / 2;
			ublk1_size->y = size_blk_max / 2;
			ublk1_size->z = size_blk_max / 2;
		}
	};

	/**
	 * @class FrameBuffer
	 * @brief Framework 에서 정의하는 frame buffer에 대한 상세 정보를 위한 자료구조
	 * @sa vmobjects::VmIObject
	 */
	struct FrameBuffer {
		/**
		 * @brief frame buffer 의 width
		 */
		int w;
		/**
		 * @brief frame buffer 의 height
		 */
		int h;
		/**
		 * @brief array로 정의된 frame buffer
		 */
		void* fbuffer;
		/**
		 * @brief frame buffer 의 data type
		 */
		data_type dtype;
		/**
		 * @brief frame buffer 의 사용 용도
		 * @details buffer_usage == FrameBufferUsageRENDEROUT 일 경우, 반드시 vmbyte4 로 설정됨.
		 */
		EvmFrameBufferUsage buffer_usage;
		/**
		 * @brief frame buffer 에 대한 descriptor
		 */
		std::string descriptor;

#ifdef __WINDOWS
		/**
		 * @brief win32에서 file memory를 통한 buffer interoperation을 위한 handle
		 */
		HANDLE hFileMap;
#endif

		/// constructor, 모두 0 (NULL or false)으로 초기화
		FrameBuffer() {
			w = h = 0;
			fbuffer = NULL;
			buffer_usage = FrameBufferUsageCUSTOM;
			descriptor = "";
#ifdef __WINDOWS
			hFileMap = NULL;
#endif
		}

		/*!
		 * @fn void vmobjects::FrameBuffer::Delete()
		 * @brief 할당된 memory 모두 해제
		 */
		void Delete() {
			w = h = 0;
			switch (buffer_usage)
			{
			case FrameBufferUsageRENDEROUT:
			{
#ifdef __WINDOWS
#ifdef __FILEMAP
				UnmapViewOfFile(fbuffer);
				CloseHandle(hFileMap);
				hFileMap = NULL;
#else
				delete[] fbuffer;
#endif
#endif
				fbuffer = NULL;
			}
			break;
			case FrameBufferUsageALIGNEDSTURCTURE:
			{
				if (fbuffer != NULL)
				{
					_aligned_free(fbuffer);
					fbuffer = NULL;
				}
			}
			break;
			default: VMSAFE_DELETEARRAY(fbuffer); break;
			}
		}
	};


	//=========================
	// Global Objects
	//=========================
	struct ObjectArchive;
	/**
	 * @class VmObject
	 * @brief VisMotive Framework Object 의 최상위 class 로 VmObject family 의 공통 parameter 를 갖음
	 */ // __vmstaticclass
	__vmstaticclass VmObject
	{
	private:
	protected:
		ObjectArchive* oa_res;
		std::any& GetObjParamA(const std::string& param_name, bool& ret);

	public:
		VmObject();
		~VmObject();

		/*!
		 * @brief VmObject의 contents가 정의되어 있는가를 확인
		 * @remarks contents는 VmObject를 상속 받는 최하위 VmObject에서 정의됨
		 * @li @ref vmobjects::VmIObject
		 * @li @ref vmobjects::VmVObjectVolume
		 * @li @ref vmobjects::VmVObjectPrimitive
		 */
		bool IsDefined();
		/*!
		 * @brief VmObject의 Object ID 설정
		 * @param obj_id [in] \n int \n 32 bit ID
		 * @remarks
		 * 일반적으로 Resource Manager (@ref VmResourceManager) 에서 지정 \n
		 * [8bit : Object Type][8bit : Magic Bits][16 bit : Count-based ID]
		 */
		void SetObjectID(const int obj_id);
		/*!
		 * @brief VmObject의 Object ID를 얻음
		 * @return int \n Object ID를 반환
		 */
		int GetObjectID() const;
		/*!
		 * @brief VmObject를 정의하는데 사용된 가장 연관성 높은 VmObject의 ID를 설정
		 * @param ref_obj_id [in] \n int \n VmObject를 정의하는데 사용된 가장 연관성 높은 VmObject의 ID
		 * @remarks default ID는 0으로 설정되어 있음
		 */
		void SetReferenceObjectID(const int ref_obj_id);
		/*!
		 * @brief VmObject를 정의하는데 사용된 가장 연관성 높은 VmObject의 ID를 얻음
		 * @return int \n VmObject를 정의하는데 사용된 가장 연관성 높은 VmObject의 ID
		 * @remarks 연관된 VmObject의 ID가 설정되어 있지 않은 경우 0을 반환
		 */
		int GetReferenceObjectID() const;
		/*!
		 * @brief VmObject에 대한 사용자 description 을 설정
		 * @param str [in] \n string \n 저장할 VmObject에 대한 사용자 description
		 */
		void SetDescriptor(const std::string& str);
		/*!
		 * @brief VmObject에 대한 사용자 description을 얻음
		 * @return wstring \n VmObject에 대한 사용자 description 반환
		 */
		std::string GetDescriptor() const;
		
		/*!
		 * @brief 정의된 VmObject의 type을 얻음
		 * @return ObjectType:: \n 정의된 VmObject의 type 반환
		 * @remarks VmObject가 정의되어 있는 상태면 이것에 따라 VmObject의 instance를 생성함
		 */
		EvmObjectType GetObjectType();

		unsigned long long GetContentUpdateTime();

		void SetContentUpdateTime();
		
		bool SetObjParam(const std::string& param_name, const std::any& v);

		template <typename T> T* GetObjParamPtr(const std::string& param_name) {
			bool ret = false;
			any& p = GetObjParamA(param_name, ret);
			return ret? (T*)&any_cast<T&>(p) : NULL;
		}
		template <typename T> T GetObjParam(const std::string& param_name, const T& init_v) {
			T* p = GetObjParamPtr<T>(param_name);
			return p == NULL ? init_v : *p;
		}

		bool RemoveObjParameters();
		bool RemoveObjParameter(const std::string& _key);
		// Static Helper Functions //
		/*!
		 * @brief VmObject ID 값으로부터 object type을 반환하는 static helper 함수
		 * @param obj_id [in] \n int \n VmObject 의 ID
		 * @return ObjectType \n VmObject의 ID에 인코딩되어 있는 object type
		 * @remarks
		 *
		 */
		static EvmObjectType GetObjectTypeFromID(const int obj_id);
		/*!
		 * @brief VmObject ID 값으로부터 object type이 VmObject인가를 확인하는 static helper 함수
		 * @param obj_id [in] \n int \n VmObject의 ID
		 * @return bool \n VmVObject이면 true, 그렇지 않으면 false 반환
		 * @remarks
		 * VmObject ID 형식에 대해서 지원.\n
		 */
		static bool IsVObject(const int obj_id);
	};

	struct VObjectArchive;
	/**
	 * @class VmVObject
	 * @brief VmObject를 상속 받으며 VmVObjectVolume과 VmVObjectPrimitive의 공간 정보를 갖는 상위 class
	 * @sa vmobjects::VmVObjectVolume, vmobjects::VmVObjectPrimitive
	 */
	__vmstaticclass VmVObject : public VmObject
	{
	private:
	protected:
		// Defined In Object Space!! //
		VObjectArchive* voa_res;

	public:
		VmVObject();
		~VmVObject();
	
		/*!
		 * @brief content를 포함하는 axis-aligned bounding box in OS 를 얻음
		 * @param aabbMm [out] \n AaBbMinMax \n object space (OS) 에서의 axis-aligned bounding box
		 */
		void GetOrthoBoundingBox(AaBbMinMax& aabbMm_os);
	
		/*!
		 * @brief OS가 WS로 배치되는 것이 정의되었는지를 확인하는 함수
		 * @return bool \n 정의되어 있으면 true, 그렇지 않으면 false
		 */
		bool IsGeometryDefined();
	
		// only to_model_space is available.. from ver. 1.10
		// transforms between OS and WS are moved into actor parameters (stored in LObject)
		// here, RS normally refers to volume space (indexing the memory address), and MS refers to dicom-specified model
		// Transform //
		/*!
		 * @brief VmVObject의 Resource Space (RS) 와 Model Space (MS) 의 변환을 정의하는 matrix를 설정
		 * @param mat_os2ws [in] \n double44 \n RS와 MS의 변환을 정의하는 matrix를 저장
		 * @remarks 내부적으로 RS와 MS 변환과 관련된 좌표계가 재설정되며 관련 matrix가 재설정됨
		 */
		void SetMatrixRS2OS(const vmmat44& mat_rs2os);
		void SetMatrixRS2OSf(const vmmat44f& mat_rs2os);
		/*!
		 * @brief VmVObject에서 정의되어 있는 RS의 MS의 변환을 정의하는 matrix를 얻음
		 * @return double44 \n OS의 WS의 변환을 정의하는 matrix
		 */
		vmmat44 GetMatrixRS2OS();
		vmmat44f GetMatrixRS2OSf();
		/*!
		 * @brief VmVObject에서 정의되어 있는 RS의 MS의 변환을 정의하는 matrix를 얻음
		 * @return double44 \n MS의 RS의 변환을 정의하는 matrix
		 */
		vmmat44 GetMatrixOS2RS();
		vmmat44f GetMatrixOS2RSf();
	};

	struct VObjectVolumeArchive;
	/**
	 * @class VmVObjectVolume
	 * @brief VmVObject를 통해 OS와 WS의 배치 관계가 설정된 Volume의 정보를 갖는 class
	 * @sa vmobjects::VmVObject
	 */
	__vmstaticclass VmVObjectVolume : public VmVObject	// CT Volume or Processing Result Volume or Histogram (2D : Size(x, y, 1))
	{
	private:
	protected:
		VObjectVolumeArchive* voavol_res;
		void Destroy();

	public:
		VmVObjectVolume();
		~VmVObjectVolume();

		// Basic Functions //
		// Block & Brick for Interactive Rendering //
		// Not Hierarchical blocking
		// Octree : level 0, Large Block,  level 1, Small Block
		/*!
		 * @brief volume 정보를 갖고 있는 @ref vmobjects::VolumeData 자료 구조를 VmVObjectVolume 에 등록함
		 * @param vol_data [in] \n VolumeData \n Volume 정보가 정의된 VolumeData
		 * @param blk_size2[2] [in] \n int3 \n
		 * volume을 정의하는 단위 block 자료구조에서 block 크기를 저장하고 있는 크기 2의 static array \n
		 * block 크기가 지정되면 내부적으로 block 단위의 최소, 최대값 자료구조가 생성되나, block 단위로 volume 재구성되지는 않음 \n
		 * blk_size2[0] : 큰 block, blk_size2[1] : 작은 block \n
		 * NULL 이면 block 생성을 안 함
		 * @param ref_obj_id [in] \n int \n
		 * content의 pointer reference를 다른 VmVObjectVolume와 공유할 때 해당 VmVObjectVolume의 ID \n
		 * 일반적으로 copy method가 사용되며, pointer 공유를 안 하며, 이 경우 0을 씀. 기본값은 0
		 * @param progress [out](optional) \n LocalProgress \n
		 * 함수가 진행되는 progress 정보를 포함하는 LocalProgress의 포인터 \n
		 * 기본값은 NULL이며, NULL이면 사용 안 함.
		 * @remarks blk_size2 이 주어지면 내부적으로 @ref VmVObjectVolume::GenerateVolumeMinMaxBlocks 이 호출됨,
		 */
		bool RegisterVolumeData(const VolumeData& vol_data, vmint3 blk_size2[2]/* 0 : Large, 1: Small */, const int ref_obj_id = 0, LocalProgress* progress = NULL);
		/*!
		 * @brief VmVObjectVolume에 정의되어 있는 volume 정보를 얻음.
		 * @return VolumeData \n volume 정보가 저장되어 있는 VolumeData 의 포인터
		 */
		VolumeData* GetVolumeData();

		// Optional //
		/*!
		* @brief volume 의 내부 값이 바뀌었을 경우, 해당 값에 대한 block 단위의 최소 최대값을 저장하고 있는 @ref SVXVolumeBlock 자료구조를 갱신하는 함수
		* @param progress [out](optional) \n LocalProgress \n
		* 함수가 진행되는 progress 정보를 포함하는 SVXLocalProgress 의 포인터 \n
		* 기본값은 NULL이며, NULL이면 사용 안 함.
		* @param i3BlockSizes[2] [in](optional) \n vmint3[] \n
		* 생성할 Block Size. Array Index 는 Level 을 의미.
		* 기본값은 NULL이며, NULL이면 내부 로직에 의거하여 Block 재사용 또는 재생성함.
		* @return bool \n Update 에 성공하면 true, 아니면 false 반환.
		* @remarks
		* 기존에 생성되어 있는 최소, 최대값 block 에 값만 갱신하며, 생성되어 있지 않은 경우 내부적으로 VmVObjectVolume::GenerateVolumeMinMaxBlocks 를 통해 Block 을 생성함 \n
		*/
		bool UpdateVolumeMinMaxBlocks(LocalProgress* progress = NULL, const vmint3 blk_size2[2] = NULL);
		
		/*!
		 * @brief volume 의 block 자료구조를 얻는 함수
		 * @param level [in] \n int \n block 의 level, 0 or 1
		 * @return VolumeBlocks \n
		 * volume 의 block 자료구조가 저장된 VolumeBlocks 의 포인터 \n
		 * volume 및 block 자료구조가 정의되어 있지 않거나 level 값이 잘못 들어가면 NULL 반환.
		 */
		VolumeBlocks* GetVolumeBlock(const int level);	// 0 or 1

		/*!
		 * @brief volume 의 block 자료구조에서 설정하는 최소, 최대값 사이의 값을 포함하고 있는 block에 대한 tag를 update하는 함수
		 * @param tobj_id [in] \n int \n block 에 의미 상 binding 되는 TObject ID
		 * @param level [in] \n int \n block 의 level, 0 or 1
		 * @param targetMm [in] \n double2 \n 사용할 최소(x), 최대값(y) \n
		 * @param progress [out] \n LocalProgress \n
		 * 함수가 진행되는 progress 정보를 포함하는 LocalProgress의 포인터 \n
		 * 기본값은 NULL이며, NULL이면 사용 안 함.
		 * @remarks
		 * 기존에 생성되어 있는 최소, 최대값 block이 class에 등록되어 있어야 함 \n
		 * @sa vmobjects::VolumeBlocks
		 */
		void UpdateTagBlocks(const int tobj_id, const int level, const vmdouble2& targetMm, LocalProgress* progress = NULL);

		/*!
		 * @fn void FillBoundaryWithValue(const double v, const bool clamp_z = false, LocalProgress* progress = NULL)
		 * @brief VolumeData 에서 정의되는 Extra Boundary 볼륨 영역에 Volume의 최소값을 채움
		 * @param v [in] \n double \n 볼륨의 Extra Boundary에 채울 볼륨값
		 * @param clamp_z [in] \n bool \n Volume이 z 축 방향에 대한 Extra Boundary 에 값을 Clamp 형식(경계값으로 통일)으로 채우는 가의 여부
		 * @param progress [in](optional)
		 * LocalProgress \n 현재 진행 정도를 처리하는 자료구조 LocalProgress에 대한 포인터 \n
		 * Default는 NULL 이며, 이 경우 진행 정도를 처리하지 않고 함수 수행
		 * @return true : 성공, false : 실패
		 * @remarks
		 * vol_slices 가 정의되어 있어야 함. \n
		 * clamp_z 가 false 이면 Extra Boundary 영역을 v 값으로 채움
		*/
		static bool FillBoundaryWithValue(VolumeData& vol_data, const double v, const bool clamp_z, LocalProgress* progress = NULL);

		/*!
		 * @fn void FillHistogram(LocalProgress* progress = NULL)
		 * @brief VolumeData 에서 정의되는 볼륨에 대한 Histogram을 생성
		 * @param progress [in](optional) \n
		 * LocalProgress \n 현재 진행 정도를 처리하는 자료구조 LocalProgress에 대한 포인터 \n
		 * Default는 NULL 이며, 이 경우 진행 정도를 처리하지 않고 함수 수행
		 * @return true : 성공, false : 실패
		 * @remarks
		 * 기존에 생성 및 정의되어 있으면 기존 것을 삭제 후 다시 생성 및 정의 \n
		 * Histogram의 Array(histo_values) 크기는 uint(store_Mm_values.y - store_Mm_values.x + 1.5)으로 정함
		*/
		static bool FillHistogram(VolumeData& vol_data, LocalProgress* progress = NULL);
		static bool FillMinMaxStoreValues(VolumeData& vol_data, LocalProgress* progress = NULL);
	};

	struct CObjectArchive;
	/**
	 * @class VmCObject
	 * @brief 카메라 관련 정보를 다루는 VmIObject에 하나의 instance로 포함된 class
	 * @remarks 
	 * 본 class에서 사용되는 space는 다음과 같다.
	 * @li WS (World Space) : 카메라 및 객체가 배치되는 real world.
	 * @li CS (Camera Space or Viewing Space) : 카메라 기준의 space, WS와 unit이 같음 \n
	 * 원점 : 카메라 위치, y축 : up vector, -z축 : viewing direction
	 * @li PS (Projection Space) : CS에서 정의된 view frustum의 내부를 기준으로 normalized cube형태의 frustum으로 정의된 space \n
	 * 원점 : near plane과 viewing direction이 만나는 점.\n
	 * y 및 z 축은 CS의 방향과 같으나, 길이는 view frustum으로 정의되는 길이가 1로 normalized 되게끔 scaling 됨.
	 * @li SS (Screen Space or Window Space) : buffer pixel에 대응하는 space \n
	 * 원점 : PS 상의 normalized view frustum 기준 z = 0인 plane의 좌측상단 \n
	 * x축 : PS 상의 x축 방향과 동일, y축 : PS 상의 -y축 방향, z축 : PS 상의 -z축 방향 \n
	 * xy scaling : screen or window를 정의하는 buffer의 해상도 크기 \n
	 * z scaling : 1 (즉 PS의 z값에 부호만 반대)
	 * @remarks image plane은 near plane 상에 정의됨.
	 * @sa vmobjects::VmIObject
	 */
	__vmstaticclass VmCObject
	{
	private:
	protected:
		// Default Interest Coordinate System is World Space
		// Camera Position : f3PosOriginInGlobalSpace
		// Viewing Direction : -f3VecAxisZInGlobalSpace
		// Up Vector : f3VecAxisYInGlobalSpace
		// Left Vector : f3VecAxisXInGlobalSpace
		CObjectArchive* coa_res;
	
	public:
		/*!
		 * @brief constructor, 카메라를 초기화하기 위한 parameter 필요
		 * @param aabbMm [in] \n AaBbMinMax \n WS에 axis-aligned 된 cube인 scene stage 정의
		 * @param stage_vtype [in] \n EvmStageViewType \n 주어진 scene stage 상의 어느 위치를 기준으로 camera를 설정하는가를 결정
		 * @param w, h [in](optional) \n int \n 
		 * SS를 정의하기 위한 screen 의 해상도 \n
		 * vrtual rendering 경우, width = 1, height = 1로 설정
		 */
		VmCObject(const AaBbMinMax& aabbMm, const EvmStageViewType stage_vtype, const int w, const int h);
		~VmCObject();
		
		/*!
		 * @brief 카메라를 scene stage (or view stage) 기준으로 설정하는 함수
		 * @param aabbMm [in] \n AaBbMinMax \n WS에 axis-aligned 된 cube인 scene stage가 정의된 AaBbMinMax 의 포인터
		 * @param stage_vtype [in] \n EvmStageViewType \n 주어진 scene stage 상의 어느 위치를 기준으로 camera를 설정하는가를 결정
		 * @remarks WS, CS, PS 간 변환 matrix가 다시 계산됨.
		 */
	 	void SetViewStage(const AaBbMinMax* aabbMm, const EvmStageViewType stage_vtype);
	 
		/*!
		 * @brief CS 에서 PS 로 변환 (projection) 되는 상태를 설정
		 * @param is_perspective [in] \n bool \n true 면 perspective projection이 되며, false 면 orthogonal projection이 됨.
		 * @param fov_y [in] \n double \n 
		 * perspective projection 시 up vector 방향 기준 field of view의 angle (radian) 이 정의된 float 포인터 \n
		 * NULL이면 기존에 설정된 field of view 값을 사용.
		 * @remarks field of view 의 초기값은 PI/4 로 설정되어 있음.
		 */
	 	void SetPerspectiveViewingState(const bool is_perspective, const double* fov_y = NULL); 
		/*!
		 * @brief perspective projection 의 상태를 얻는 함수
		 * @param fov_y [out] \n double \n 
		 * perspective projection 시 up vector 방향 기준 field of view의 angle (radian) 이 정의된 float 포인터 \n
		 * null 이면 해당 파라미터에 값을 할당하지 않음
		 * @return perspective 유무
		 * @remarks field of view 의 초기값은 PI/4 로 설정되어 있음.
		 */
	 	bool GetPerspectiveViewingState(double* fov_y);
	 
	 	// Image Plane is defined as Near Plane from Camera
		/*!
		 * @brief viewing state를 통한 CS, PS, SS 의 변환 matrix를 재설정하는 함수
		 * @param ipsize_cs [in] \n double 2 \n WS(or CS) 상의 near plane의 width(x), height(y)가 저장된 포인터
		 * @param near_p [in] \n double \n WS(or CS) 상의 camera에서 near plane 사이의 거리가 정의된 포인터
		 * @param far_p [in] \n double \n WS(or CS) 상의 camera에서 far plane 사이의 거리가 정의된 포인터
		 * @param wh_ss [in] \n int 2 \n SS 상의 screen 크기(width(x), height(y))가 정의된 포인터. 
		 * @param fit_ipsize [in] \n double 2 \n Orthogonal Projection에서 Camera 가로 세로 ratio 를 유지시키기 위한 Fitting 정보가 저장된 포인터
		 * @param fit_fovy [in] \n double \n Perspective Projection에서 Camera 가로 세로 ratio 를 유지시키기 위한 Fitting 정보가 저장된 포인터
		 * @remarks WS to CS, CS to PS, PS to SS 에 대한 변환 matrix 및 각각에 대한 inverse matrix가 다시 계산됨. \n
		 * @ 파라미터가 NULL 이면 해당 파라미터는 기존값 사용
		 */
		void SetCameraIntState(const vmdouble2* ipsize_cs, const double* near_p, const double* far_p, const vmint2* wh_ss, vmdouble2* fit_ipsize = NULL, const double* fit_fovy = NULL);
		void SetCameraIntStateAR(const double* fx, const double* fy, const double* sc, const double* cx, const double* cy, const double* near_p, const double* far_p, const vmint2* wh_ss);
		/*!
		 * @brief 현재 CS, PS, SS 의 변환 matrix를 정의하는 viewing state를 얻는 함수
		 * @param ipsize_cs [in] \n double 2 \n WS(or CS) 상의 near plane의 width(x), height(y)가 저장된 포인터
		 * @param near_p [in] \n double \n WS(or CS) 상의 camera에서 near plane 사이의 거리가 정의된 포인터
		 * @param far_p [in] \n double \n WS(or CS) 상의 camera에서 far plane 사이의 거리가 정의된 포인터
		 * @param wh_ss [in] \n int 2 \n SS 상의 screen 크기(width(x), height(y))가 정의된 포인터.
		 * @param fit_ipsize [in] \n double 2 \n Orthogonal Projection에서 Camera 가로 세로 ratio 가 저장된 포인터
		 * @param fit_fovy [in] \n double \n Perspective Projection에서 Camera 가로 세로 ratio 를 유지시키기 위한 Fitting 정보가 저장된 포인터
		 * @remarks 얻고 싶지 않은 parameter에 대해 NULL을 넣으면 해당 parameter에 값을 저장 안 함.
		 */
		void GetCameraIntState(vmdouble2* ipsize_cs, double* near_p, double* far_p, vmint2* wh_ss, vmdouble2* fit_ipsize = NULL, double* fit_fovy = NULL);
		void GetCameraIntStateAR(double* fx, double* fy, double* sc, double* cx, double* cy, double* near_p, double* far_p);
		/*!
		 * @brief WS 상에서 정의된 카메라 state를 설정하는 함수
		 * @param pos [in] \n double 3 \n WS 상의 카메라 위치가 정의된 포인터
		 * @param view [in] \n double 3 \n WS 상의 viewing direction이 정의된 포인터
		 * @param up [in] \n double 3 \n WS 상의 up vector가 정의된 포인터
		 * @remarks WS 와 CS 의 변환을 정의하는 matrix가 다시 계산됨.\n
		 * @ 파라미터가 NULL 이면 해당 파라미터는 기존값 사용\n
		 * @ view 및 up 는 orthonormal 이며, unit vector 이어야 함.
		 */
		void SetCameraExtState(const vmdouble3* pos, const vmdouble3* view, const vmdouble3* up);
		void SetCameraExtStatef(const vmfloat3* pos, const vmfloat3* view, const vmfloat3* up);
		/*!
		 * @brief WS 상의 카메라 정보를 얻는 함수
		 * @param pos [in] \n double 3 \n WS 상의 카메라 위치가 정의된 포인터
		 * @param view [in] \n double 3 \n WS 상의 viewing direction이 정의된 포인터
		 * @param up [in] \n double 3 \n WS 상의 up vector가 정의된 포인터
		 */
		void GetCameraExtState(vmdouble3* pos, vmdouble3* view, vmdouble3* up);
		void GetCameraExtStatef(vmfloat3* pos, vmfloat3* view, vmfloat3* up);
		/*!
		 * @brief WS 상에서 정의된 카메라 zoom 상태를 1.0 (100% 상태) 으로 정의하는 up vector 방향의 field of view (rad) 를 설정하는 함수
		 * @param fitsize_fov_y [in] \n double \n WS 에서 up vector 방향의 field of view
		 */
		void SetZoomfactorFovY(const double fitting_fov_y);
		/*!
		 * @brief WS 상에서 정의된 카메라 zoom 상태를 1.0 (100% 상태) 으로 정의하는 up vector 방향의 field of view 를 얻는 함수
		 * @return WS 상에서 정의된 카메라 zoom 상태를 1.0 (100% 상태) 으로 정의하는 up vector 방향의 field of view (rad)
		 */
		double GetZoomfactorFovY();
		/*!
		 * @brief perspective projection 의 여부를 얻는 함수
		 * @return bool \n perspective projection 이면 true 반환, orthogonal projection 이면 false 반환
		 */
	 	bool IsPerspective();
	 
		/*!
		 * @brief WS 에서 SS 로의 변환을 정의하는 matrix를 얻음
		 * @param mat_ws2cs [out] \n double 44 \n WS 에서 CS로 변환하는 matrix가 저장될 포인터
		 * @param mat_cs2ps [out] \n double 44 \n CS 에서 PS로 변환하는 matrix가 저장될 포인터
		 * @param mat_ps2ss [out] \n double 44 \n PS 에서 SS로 변환하는 matrix가 저장될 포인터
		 * @remarks 얻고 싶지 않은 parameter에 대해 NULL을 넣으면 해당 parameter에 값을 저장 안 함.
		 */
		void GetMatrixWStoSS(vmmat44* mat_ws2cs, vmmat44* mat_cs2ps, vmmat44* mat_ps2ss);
		void GetMatrixWStoSSf(vmmat44f* mat_ws2cs, vmmat44f* mat_cs2ps, vmmat44f* mat_ps2ss);
		/*!
		 * @brief SS 에서 WS 로의 변환을 정의하는 matrix를 얻음
		 * @param mat_ss2ps [out] \n double 44 \n SS 에서 PS로 변환하는 matrix가 저장될 포인터
		 * @param mat_ps2cs [out] \n double 44 \n PS 에서 CS로 변환하는 matrix가 저장될 포인터
		 * @param mat_cs2ws [out] \n double 44 \n CS 에서 WS로 변환하는 matrix가 저장될 포인터
		 * @remarks 얻고 싶지 않은 parameter에 대해 NULL을 넣으면 해당 parameter에 값을 저장 안 함.
		 */
		void GetMatrixSStoWS(vmmat44* mat_ss2ps, vmmat44* mat_ps2cs, vmmat44* mat_cs2ws);
		void GetMatrixSStoWSf(vmmat44f* mat_ss2ps, vmmat44f* mat_ps2cs, vmmat44f* mat_cs2ws);
		
		/*!
		 * @brief WS or CS or PS or SS 에서 정의되는 image plane의 rectangle을 정의하는 네 점을 얻음
		 * @param coord_space [in] \n EvmCoordSpace \n image plane을 얻고자 하는 space
		 * @param pos_tl [out] \n double 3 \n coord_space 에서 정의된 image plane의 rectangle의 좌측상단 점이 저장될 포인터
		 * @param pos_tr [out] \n double 3 \n coord_space 에서 정의된 image plane의 rectangle의 우측상단 점이 저장될 포인터
		 * @param pos_bl [out] \n double 3 \n coord_space 에서 정의된 image plane의 rectangle의 좌측하단 점이 저장될 포인터
		 * @param pos_br [out] \n double 3 \n coord_space 에서 정의된 image plane의 rectangle의 우측하단 점이 저장될 포인터
		 * @remarks 
		 * 얻고 싶지 않은 parameter에 대해 NULL을 넣으면 해당 parameter에 값을 저장 안 함. \n
		 * OS 에 대해선 동작하지 않음.
		 */
		void GetImagePlaneRectPoints(const EvmCoordSpace coord_space, vmdouble3* pos_tl, vmdouble3* pos_tr, vmdouble3* pos_bl, vmdouble3* pos_br);
		void GetImagePlaneRectPointsf(const EvmCoordSpace coord_space, vmfloat3* pos_tl, vmfloat3* pos_tr, vmfloat3* pos_bl, vmfloat3* pos_br);
	};

	struct IObjectArchive;
	/**
	 * @class VmIObject
	 * @brief image plane의 buffer 를 다루고 하나의 VmCObject를 갖는 VmObject를 상속 받는 class
	 * @remarks
	 * 하나의 해상도 (width, height)에 대해 다양한 용도에 사용될 여러개의 image buffer (frame buffer)가 정의됨. \n
	 * image plane과 연결되는 하나의 카메라 정보를 갖는 VmCObject 의 instance를 갖음.
	 * @sa vmobjects::VmObject, vmobjects::VmCObject
	 */
	__vmstaticclass VmIObject : public VmObject
	{
	private:
	protected:
		IObjectArchive* ioa_res;

	public:
		/*!
		 * @brief constructor, image plane 에 mapping되는 frame buffer를 정의하는 해상도 필요
		 * @param w [in](optional) \n int \n 해상도 width (pixel), 기본값은 0
		 * @param h [in](optional) \n int \n 해상도 height (pixel), 기본값은 0
		 * @remarks width 또는 height 가 0 이하면 frame buffer를 생성하는 함수 (@ref VmIObject::ResizeFrameBuffer, @ref  VmIObject::InsertFrameBuffer)가 실패
		 */
		VmIObject(const int w = 0, const int h = 0);
		~VmIObject();

		/*!
		 * @brief 정의되어 있는 frame buffer의 크기를 재설정하는 함수
		 * @param w [in] \n int \n 해상도 width (pixel), 1 이상
		 * @param h [in] \n int \n 해상도 height (pixel), 1 이상
		 * @remarks
		 * 기존에 정의된 frame buffer를 memory에서 해제  후 주어진 크기로 다시 할당\n
		 * frame buffer에 저장되었던 내용물도 모두 삭제됨 (다시 채우는 module or function이 호출되야 함)\n
		 * image plane의 pixel에 대한 x pitch와 y pitch가 같다고 가정하므로 width와 height에 대한 ratio가 바뀜\n
		 * 따라서, VmCObject 에서 정의하는 WS 상의 image plane의 정보가 재설정되고, 관련된 변환 matrix가 재설정됨.
		 */
		void ResizeFrameBuffer(const int w, const int h);
		/*!
		 * @brief 정의되어 있는 frame buffer의 정보를 얻는 함수
		 * @param buffer_size [out] \n int 2 \n frame buffer의 해상도가 저장될 포인터. width(x), height(y)
		 * @param num_buffers [out](optional) \n int \n 현재 정의된 frame buffer의 개수가 저장될 포인터
		 * @param bytes_per_pixel [out](optional) \n int \n 현재 정의된 모든 frame buffer에 대하여 하나의 pixel을 정의하는 type들의 크기합이 저장될 포인터
		 * @remarks 얻고 싶지 않은 parameter에 대해 NULL을 넣으면 해당 parameter에 값을 저장 안 함.
		 */
		void GetFrameBufferInfo(vmint2* buffer_size/*out*/, int* num_buffers = NULL/*out*/, int* bytes_per_pixel = NULL/*out*/);
		/*!
		 * @brief 정의되어 있는 frame buffer의 정보가 저장된(array 포함) @ref vmobjects::SVXFrameBuffer 를 얻는 함수
		 * @param fb_usage [in] \n EvmFrameBufferUsage \n 정의된 frame buffer들 중 해당 usage의 buffer를 얻음
		 * @param buffer_idx [in] \n int \n 해당 usage의 frame buffer 중 index 번째로 정의되어 있는 buffer를 얻음
		 * @return FrameBuffer \n frame buffer의 정보가 저장된(array 포함) @ref vmobjects::FrameBuffer 의 포인터
		 */
		FrameBuffer* GetFrameBuffer(const EvmFrameBufferUsage fb_usage, const int buffer_idx);

		/*!
		 * @brief frame buffer를 하나 추가하는 함수
		 * @param dtype [in] \n data_type \n 추가할 frame buffer의 data type
		 * @param fb_usage [in] \n EvmFrameBufferUsage \n 추가할 frame buffer의 usage
		 * @param descriptor [in] \n string \n 추가할 frame buffer에 대한 descriptor
		 * @remarks fb_usage == vmenums::EvmFrameBufferUsage::FrameBufferUsageRENDEROUT 일 경우 data type 은 typeid(vmbyte4).name() 이어야 함.
		 */
		void InsertFrameBuffer(const data_type& dtype, const EvmFrameBufferUsage fb_usage, const std::string& descriptor);

		/*!
		 * @brief frame buffer를 변경하는 함수
		 * @param fb_usage [in] \n EvmFrameBufferUsage \n 변경할 frame buffer의 usage
		 * @param buffer_idx [in] \n int \n 변경할 frame buffer의 index
		 * @param dtype [in] \n data_type \n 변경할 frame buffer 를 data type 으로 변경
		 * @param descriptor [in] \n string \n 변경할 frame buffer 를 descriptor 로 변경
		 * @return bool \n 해당 buffer_idx 의 버퍼가 있으면 true, 없으면 false 반환
		 * @remarks 해당 index 의 버퍼가 dtype 로 이미 선언되어 있으면 아무 작업 없이 true 반환
		 */
		bool ReplaceFrameBuffer(const EvmFrameBufferUsage fb_usage, const int buffer_idx, const data_type& dtype, const std::string& descriptor);

		/*!
		 * @brief frame buffer를 삭제하는 함수
		 * @param fb_usage [in] \n EvmFrameBufferUsage \n 삭제할 frame buffer의 usage
		 * @param buffer_idx [in] \n int \n 해당 usage (eFrameBufferUsage)의 frame buffer 중 index 번째의 buffer를 삭제
		 * @return 해당 frame buffer가 존재하며 삭제에 성공하면 true, 그렇지 않으면 false 반환
		 * @remarks 해당 frame buffer(fb_usage && buffer_idx)는 memory에서 해제됨.
		 */
		bool DeleteFrameBuffer(const EvmFrameBufferUsage fb_usage, const int buffer_idx);

		/*!
		 * @brief VmCObject의 instance를 생성.
		 * @param aabbMm [in] \n AaBbMinMax \n WS에 axis-aligned 된 cube인 scene stage
		 * @param stage_vtype [in] \n EvmStageViewType \n 주어진 scene stage 상의 어느 위치를 기준으로 camera를 설정하는가를 결정
		 * @remarks
		 * 이미 생성되어 있으면 동작하지 않음. \n
		 * instance 생성 전 VmIObject 의 content 가 정의되어 있어야 함. (즉 buffer가 생성되어 있어야 함)
		 * @sa vmobjects::VmObject, vmobjects::VmCObject
		 */
		void AttachCamera(const AaBbMinMax& aabbMm, const EvmStageViewType stage_vtype);
		/*!
		 * @brief VmCObject의 instance를 얻는 함수
		 * @return VmCObject \n instance로 정의되어 있는 VmCObject 의 포인터
		 */
		VmCObject* GetCameraObject();

		/*!
		 * @brief SVXFrameBuffer 들이 저장된 vector container를 얻음
		 * @param fb_usage [in] \n EvmFrameBufferUsage \n 얻고자 하는 Frame Usage
		 * @return 정의된 FrameBuffer 들이 저장된 vector container에 대한 vector<FrameBuffer> 의 포인터
		 */
		std::vector<FrameBuffer>* GetBufferPointerList(const EvmFrameBufferUsage fb_usage);
	};

	struct VObjectPrimitiveArchive;
	/**
	 * @class VmVObjectPrimitive
	 * @brief VmVObject를 통해 OS와 WS의 배치 관계가 설정된 Volume의 정보를 갖는 class.
	 * @remarks
	 * 기존의 OS를 OS 와 VOS 의 두개로 나누어 처리. \n
	 * @li OS : PrimitiveData 저장되는 primitive 좌표계
	 * @li VOS : PrimitiveData 로 정의되는 개별 객체가 WS 좌표계 이전에 배치되는 좌표계 \n
	 * VmVObject 의 OS 가 VOS 가 되며, OS/VOS/WS 간 변환은 사용자 정의에 따름 \n
	 * @remarks OS 가 VOS 로의 객체 변환을 통해 PrimitiveData 로 정의되는 개별 객체를 WS 상에 여러 형태(affine transform)로 배치 및 변형 할 수 있음.
	 * @sa vmobjects::VmVObject, vmobjects::VmCObject
	 */
	__vmstaticclass VmVObjectPrimitive : public VmVObject
	{
	private:
	protected:
		VObjectPrimitiveArchive* voaprim_res;

	public:
		VmVObjectPrimitive();
		~VmVObjectPrimitive();

		/*!
		 * @brief primitive로 정의된 객체 정보를 갖고 있는 @ref vmobjects::PrimitiveData 자료 구조를 VObjectPrimitiveArchive에 등록함
		 * @param prim_data [in] \n PrimitiveData \n primitive로 정의된 객체 정보
		 * @param progress [out](optional) \n LocalProgress \n
		 * 함수가 진행되는 progress 정보를 포함하는 포인터 \n
		 * 기본값은 NULL이며, NULL이면 사용 안 함.
		 */
		bool RegisterPrimitiveData(const PrimitiveData& prim_data, LocalProgress* progress = NULL);
		/*!
		 * @brief VmVObjectPrimitive에 정의되어 있는 primitive로 정의된 객체 정보를 얻음.
		 * @return PrimitiveData \n primitive로 정의된 객체 정보가 저장되어 있는 PrimitiveData 의 포인터
		 */
		PrimitiveData* GetPrimitiveData();
		/*!
		* @brief VmVObjectPrimitive 의 가시화 시 wireframe 가시화 상태를 설정.
		* @param bIsVibible [in] \n bool \n true 면 가시화, false 면 가시화되지 않음
		* @param color [in] \n double 4 \n RGBA = XYZW. [min, max] = [0.0f, 1.0f]
		*/
		//void SetPrimitiveWireframeVisibilityColor(const bool is_wireframe, const vmdouble4& color);
		/*!
		* @brief VmVObjectPrimitive 의 가시화 시 wireframe 가시화 상태를 가져옴.
		* @param is_wireframe [out](optional) \n bool \n 가시화 여부를 저장하는 pointer
		* @param color [out](optional) \n double 4 \n 색상 정보를 저장하는 pointer
		*/
		//void GetPrimitiveWireframeVisibilityColor(bool* is_wireframe, vmdouble4* color);

		bool HasKDTree(int* num_updated = NULL);
		void UpdateKDTree(); // just for point cloud
		uint KDTSearchRadius(const vmfloat3& p_src, const float r_sq, const bool is_sorted, std::vector<std::pair<size_t, float>>& ret_matches);
		uint KDTSearchKnn(const vmfloat3& p_src, const int k, size_t* out_ids, float* out_dists);
		void UpdateBVHTree(int min_size = -1, int max_size = -1); // for primitives
		void* GetBVHTree();
	};
}; // namespace vmobjects


namespace vmgeom {
	__vmstatic void GeneratePrimitive_Sphere(vmobjects::PrimitiveData& prim_data/*out*/, const vmdouble3& pos_center, const double radius, const int num_iter);
	__vmstatic void GeneratePrimitive_Cone(vmobjects::PrimitiveData& prim_data/*out*/, const vmdouble3& pos_s, const vmdouble3& pos_e, const double radius, const bool open_cone, const int num_interpolations);
	__vmstatic void GeneratePrimitive_Cylinder(vmobjects::PrimitiveData& prim_data/*out*/, const vmdouble3& pos_s, const vmdouble3& pos_e, const double radius, const bool open_top, const bool open_bootom, const int num_interpolations, int num_circle_interpolations, int num_sideheight_interpolations);
	__vmstatic void GeneratePrimitive_Cube(vmobjects::PrimitiveData& prim_data/*out*/, const vmdouble3& pos_min, const vmdouble3& pos_max, const double edge_nrl_weight/*0.0 to 1.0*/, const bool cube_frame_mode);
	__vmstatic void GeneratePrimitive_Line(vmobjects::PrimitiveData& prim_data/*out*/, const vmdouble3& pos_s, const vmdouble3& pos_e);
	__vmstatic void GeneratePrimitive_Arrow(vmobjects::PrimitiveData& prim_data, const vmdouble3& pos_s, const vmdouble3& pos_e, const double arrow_body_ratio, const vmdouble2& arrow_components_radius, const int num_interpolation);
};


//==========================================
// Function Container : 2022.03.10
//==========================================
namespace fncontainer
{
	struct VmActor {
	private:
		vmobjects::VmVObject* _geometry_res = NULL;
		vmobjects::VmParamMap<std::string, std::any> _associated_res; // VmObject
		vmobjects::VmParamMap<std::string, std::any> _vmparams;
	public:
		bool visible = true;
		vmfloat4 color = vmfloat4(1.f);
		vmmat44f matOS2WS = vmmat44f();
		vmmat44f matWS2OS = vmmat44f();
		std::string name = "No Name";
		int actorId = 0;

		VmActor* parentActor = NULL;

		vmobjects::VmVObject* GetGeometryRes() {
			return _geometry_res;
		}

		void SetGeometryRes(vmobjects::VmVObject* geometry_res) {
			_geometry_res = geometry_res;
		}

		vmobjects::VmObject* GetAssociateRes(const std::string& name) {
			return _associated_res.GetParam(name, (vmobjects::VmObject*)NULL);
		}

		void SetAssociatedRes(const std::string name, const vmobjects::VmObject* geometry_res) {
			_associated_res.SetParam(name, (vmobjects::VmObject*)geometry_res);
		}

		template <typename T>
		T GetParam(const std::string& param_name, const T _init)
		{
			return _vmparams.GetParam(param_name, _init);
		}

		template <typename T>
		bool GetParamCheck(const std::string& param_name, T& _v)
		{
			return _vmparams.GetParamCheck(param_name, _v);
		}

		template <typename T>
		T* GetParamPtr(const std::string& param_name)
		{
			return _vmparams.GetParamPtr<T>(param_name);
		}

		template <typename S, typename T>
		T GetParamCasting(const std::string& param_name, const T _init)
		{
			return _vmparams.GetParamCasting<S>(param_name, _init);
		}

		template <typename S, typename T>
		T GetParamCastingCheck(const std::string& param_name, T& _v)
		{
			return _vmparams.GetParamCastingCheck<S>(param_name, _v);
		}

		template <typename T>
		void SetParam(const std::string& param_name, const T& _v)
		{
			if (_vmparams == NULL) return;
			_vmparams.SetParam(param_name, _v);
		}

		template <typename T>
		void SetParamV(const std::string& param_name, const T _v)
		{
			if (_vmparams == NULL) return;
			_vmparams.SetParam(param_name, _v);
		}
	};

	struct VmFnContainer {
		std::string descriptor;

		vmobjects::VmParamMap<int, VmActor>* sceneActors = NULL;
		vmobjects::VmParamMap<std::string, std::any>* vmparams = NULL;

		template <typename T>
		T GetParam(const std::string& param_name, const T _init)
		{
			if (vmparams == NULL) return _init;
			T v = _init;
			vmparams->GetParam(param_name, v);
			return v;
		}

		template <typename T>
		bool GetParamCheck(const std::string& param_name, T& _v)
		{
			return vmparams->GetParamCheck(param_name, _v);
		}

		template <typename T>
		T* GetParamPtr(const std::string& param_name)
		{
			if (vmparams == NULL) return NULL;
			return vmparams->GetParamPtr<T>(param_name);
		}

		template <typename S, typename T>
		T GetParamCasting(const std::string& param_name, const T _init)
		{
			if (vmparams == NULL) return _init;
			T v = _init;
			vmparams->GetParamCasting<S>(param_name, v);
			return v;
		}

		template <typename S, typename T>
		bool GetParamCastingCheck(const std::string& param_name, T& _v)
		{
			return vmparams->GetParamCastingCheck<S>(param_name, _v);
		}

		template <typename T>
		void SetParam(const std::string& param_name, const T& _v)
		{
			if (vmparams == NULL) return;
			vmparams->SetParam(param_name, _v);
		}

		template <typename T>
		void SetParamV(const std::string& param_name, const T _v)
		{
			if (vmparams == NULL) return;
			vmparams->SetParam(param_name, _v);
		}
	};
}
