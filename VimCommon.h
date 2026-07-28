/**
 * @mainpage    VizMotive Framework
 *
 * @section intro Introduction
 *      - Describes the Global Data Structures, Helper Functions, and Engine APIs that make up the VizMotive Framework.
 *
 * @section CREATEINFO Authoring Information
 *      - Author       :   DongJoon Kim
 *      - Date         :   2019/7/11
 *      - Contact     :   korfriend@gmail.com
 *
 * @section MODIFYINFO Revision Information
 *      - 2019.7.11    :   Initial Framework Doxygen 0.0.1 documentation authored against the source as of this date
 */
 
/**
 * @file VimCommon.h
 * @brief File declaring the Global Data Structures, Classes, and Helper Functions.
 * @section Include & Link Information
 *		- Include : VimCommon.h
 *		- Library : CommonUnits.lib
 *		- Linking Binary : CommonUnits.dll
 */
 
#pragma once
 //#define __VERSION "0.x beta" // released at 22.01.10
//#define __VERSION "1.00" // released at 22.02.5
//#define __VERSION "1.01" // released at 22.02.15
//#define __VERSION "1.10" // released at 22.04.02
//#define __VERSION "1.11" // released at 22.05.17
//#define __VERSION "1.12" // released at 22.08.08
//#define __VERSION "1.13" // released at 22.12.05
//#define __VERSION "1.20" // released at 23.04.03
//#define __VERSION "1.30" // released at 25.02.04
//#define __VERSION "1.31" // released at 25.02.07
//#define __VERSION "1.32" // released at 25.02.08
//#define __VERSION "1.33" // released at 25.08.11
//#define __VERSION "1.40" // released at 25.11.29
//#define __VERSION "1.41" // released at 25.12.01
//#define __VERSION "1.50" // released at 25.12.29
//#define __VERSION "1.51" // released at 26.01.12
//#define __VERSION "1.52" // released at 26.01.17
//#define __VERSION "1.60" // released at 26.07.17
//#define __VERSION "1.61" // released at 26.07.18
//#define __VERSION "1.70" // released at 26.07.19 : VmObject family -> virtual interface + _Detail (cobj->VmLens pilot)
//#define __VERSION "1.71" // released at 26.07.24 : PrimitiveData::GetNumCustomDefinitions() added (GenerateCopiedObject FACECOLOR deep-copy fix)
//#define __VERSION "1.72" // released at 26.07.25 : (§4.2a) resource incarnation token — VmObject birth/mutation/poison + owner-only destructive mutators + const GetPrimitiveData/GetVolumeData + encapsulated vidx_buffer/vol_slices
// "1.73" — released at 26.07.28. THE FIRST BUMP FOR A BEHAVIOUR CONTRACT RATHER THAN A LAYOUT ONE, and the
// meaning of this constant is deliberately widened here: it answers "may this DLL be mixed with this core?",
// and a behaviour contract can make a mismatch just as unsafe as a struct-size one. This time it is worse than
// unsafe-in-theory — it is silently destructive:
//   - vismtv_inbuilt_renderergpudx gains the fnParams key _bool_ForceStoreRenderBuffer, which runs the existing
//     RenderOut() GPU->CPU copy-back with NO render pass and replies through _bool_StoredRenderBuffer. A DLL
//     that predates the key does not ignore the request: it reads that minimal container as an ORDINARY RENDER
//     of an empty actor set and BLANKS the CPU framebuffer (RenderOut zero-fills on _int_NumCallRenders == 0),
//     and only afterwards does the caller notice the missing reply. A blanked frame returned as success.
//   - vismtv_inbuilt_rwfiles gains _string_UsageMode "EXPORT_2DIMAGE_MEMORY". An older build hits its
//     unknown-usage-mode else branch, which RETURNS TRUE and writes no output.
// Neither is caught by the layout signature (no struct changed) and neither is caught by kApiVersionTag /
// kModuleVersionTag, which are reported in GetEngineAPIsVer()'s string and enforced by nothing. __VERSION is
// the only value the loader actually refuses on (GpuManager.cpp), so this is where the refusal has to live.
// CONSEQUENCE, stated plainly: every DLL compiled against VimCommon.h must be rebuilt — all renderergpu
// variants, renderercpu, and every vismtv_* plugin. One that is not rebuilt is DISABLED at load, which is the
// intended outcome, because running it is what corrupts the frame.
#define __VERSION "1.73" // released at 26.07.28 : plugin BEHAVIOUR contract — on-demand GPU->CPU copy-back (renderergpu) + in-memory image encode (rwfiles); an older plugin mishandles both destructively

#define _HAS_STD_BYTE 0

#include "vzm2/Geometrics.h"

#include <map>
#include <unordered_map>
#include <vector>
#include <set>
#include <string>
#include <sstream>
#include <algorithm>
#include <typeinfo>
#include <typeindex>
#include <type_traits> // (rev.14) the LIGHT-actor invariant static_assert in VisMtvApi.cpp
#include <any>

#include "VimHelpers.h"

using namespace vz;

#define __WINDOWS
#define __FILEMAP
#ifdef __WINDOWS

#define NOMINMAX
#include <windows.h>
#endif

/**
 * @brief VizMotive Framework
 */

//=====================================================================
// Please, project's character set as UNICODE
// GLM library is used as a common math
// Copyright by DongJoon Kim. All rights reserved.
//=====================================================================


// ONLY FOR WINDOWS VERSION
#define VMENGINEVERSION 0x29AD7	// 170711(allocating 20 bits) and  12 bits for modules and engine enhancement version
#define VMSAFE_DELETE(p)	{ if(p) { delete (p); (p)=NULL; } }
#define VMSAFE_DELETEARRAY(p)	{ if(p) { delete[] (p); (p)=NULL; } }
#define VMSAFE_DELETE2DARRAY(pp, numPtrs)	{ if(pp){ for(int i = 0; i < numPtrs; i++){ VMSAFE_DELETEARRAY(pp[i]);} VMSAFE_DELETEARRAY(pp); } }
#define VMSAFE_DELETEARRAY_VOID(p) { if(p){ delete[] (char*)(p); (p)=NULL; } }
#define VMSAFE_DELETE2DARRAY_VOID(pp, numPtrs) { if(pp){ for(int i=0;i<numPtrs;i++){ VMSAFE_DELETEARRAY_VOID((pp)[i]); } delete[] (pp); (pp)=NULL; } }

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

// (1.70) VmCamera (namespace fncontainer, defined far below) is referenced by pointer from vmobjects::VmIObject.
// Forward-declare it here so the iobj can hold/return a VmCamera* (the dropped VmLens' role folded into VmCamera).
namespace fncontainer { struct VmCamera; }

#define VM_PI 3.14159265358979323846
#define VM_fPI    ((float)  3.141592654f)

#define NUM_VTX_DEFINITIONS 8

	// temp typedefs
	// our proj math structures are based on glm::
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

typedef glm::u8vec4 vmbyte4;
typedef glm::u8vec3 vmbyte3;
typedef glm::u8vec2 vmbyte2;
typedef glm::i8vec2 vmchar2;
typedef glm::i16vec2 vmshort2;
typedef glm::u16vec2 vmushort2;
typedef glm::i16vec3 vmshort3;
typedef glm::u16vec3 vmushort3;
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

static vmbyte3 Float3ToU8vec3(const vmfloat3& c01)
{
	glm::vec3 x = glm::clamp(c01, 0.0f, 1.0f);
	x = glm::round(x * 255.0f);
	return glm::u8vec3(x);
}

static vmbyte4 Float3ToU8vec4(const vmfloat3& c01)
{
	glm::vec3 x = glm::clamp(c01, 0.0f, 1.0f);
	x = glm::round(x * 255.0f);
	return glm::u8vec4(x.x, x.y, x.z, 255);
}

#define __VMCVT3__(d, s, t3, tt) d=t3((tt)s.x, (tt)s.y, (tt)s.z)
#define __OPS__(d, s, op) d=op(d, s)

namespace vmlog {
	__vmstatic void InitLog(const std::string& coreName, const std::string& logFileName);
	__vmstatic void LogInfo(std::string str);
	__vmstatic void LogWarn(std::string str);
	__vmstatic void LogErr(std::string str);
}
//=====================================
// Global Enumerations
//=====================================

/**
 * @package vmenums
 * @brief Namespace collecting the enumerations used as common data structures.
 */
namespace vmenums {
	/*! Kinds of coordinate spaces supported by the framework; closed under the projective transform defined by a 4x4 matrix */
	enum EvmCoordSpace {
		CoordSpaceSCREEN = 0,/*!< Screen space, defined in pixels */
		CoordSpacePROJECTION,/*!< Projection space, defined by a normalized frustum */
		CoordSpaceCAMERA,/*!< Camera space, defined by the viewing frustum */
		CoordSpaceWORLD,/*!< World space, where objects are actually placed */
		CoordSpaceOBJECT/*!< Object space, where an object is defined */
	};

	/*! Kinds of initial camera states (position, view and up vectors) */
	enum EvmStageViewType {
		StageViewORTHOBOXOVERVIEW = 0,/*!< 3D view: an overview looking along the diagonal of the object bounding box in OS */
		StageViewCENTERFRONT,/*!< Cross-sectional view: the front (coronal) view centered on the object bounding box in OS */
		StageViewCENTERRIGHT,/*!< Cross-sectional view: the right (sagittal) view centered on the object bounding box in OS */
		StageViewCENTERHORIZON/*!< Cross-sectional view: the top (axial) view centered on the object bounding box in OS */
	};

	/*! Kinds of primitives that define a polygonal VObject */
	enum EvmPrimitiveType {
		PrimitiveTypeUNDEFINED = 0,/*!< Undefined */
		PrimitiveTypeLINE,/*!< Line */
		PrimitiveTypeTRIANGLE,/*!< Triangle */
		PrimitiveTypePOINT/*!< Point */
	};

	/*! Kinds of per-element bounding units for a VObject */
	enum EvmBoundingUnitType {
		BoundingUnitTypeOBB = 0,/*!< OBB */
		BoundingUnitTypeAABB,/*!< AABB */
		BoundingUnitTypeSPHERE,/*!< Sphere */
	};

	/*! Kinds of VmObject types used in module-platform interoperation */
	enum EvmObjectType {
		ObjectTypeOBJECT = 1,/*!< Just an Object for archiving something */
		ObjectTypeVOLUME,/*!< Volume Object */
		ObjectTypePRIMITIVE,/*!< Polygon Object */
		ObjectTypeIMAGEPLANE/*!< A VmObject that defines an image plane and owns a camera object */
	};

	/*! Kinds of frame buffers used by VmIObject */
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
* @brief Data structure describing progress as defined by the framework.
*/
struct LocalProgress {
	/**
	 * @brief Start of the progress range, between 0.0 and 100.0
	 */
	double start;
	/**
	 * @brief Extent of the progress range, between 0.0 and 100.0
	 */
	double range;
	/**
	 * @brief Pointer to a static parameter inside a module/function where the current progress is recorded
	 */
	double* progress_ptr; /*out*/
	/// constructor; initialization
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
	 * @fn void vmobjects::LocalProgress::Deinit()
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
 * @brief Namespace collecting the framework's global data structures and VmObject classes.
 */
namespace vmobjects
{
	using namespace vmenums;
	//=========================
	// Object Structures
	//=========================
	template <typename NAME, typename T, class HASH_COMP = std::hash<NAME>> struct VmMap {
	private:
		std::string __PM_VERSION = "LIBI_1.4";
		std::unordered_map<NAME, T, HASH_COMP> __params;
	public:
		bool GetParamCheck(const NAME& param_name, T& param) {
			auto it = __params.find(param_name);
			if (it == __params.end()) return false;
			param = it->second;
			return true;
		}
		T GetParam(const NAME& param_name, const T& init_v) {
			auto it = __params.find(param_name);
			if (it == __params.end()) return init_v;
			return it->second;
		}
		T* GetParamPtr(const NAME& param_name) {
			auto it = __params.find(param_name);
			if (it == __params.end()) return NULL;
			return &it->second;
		}
		template <typename DSTV> bool GetParamCastingCheck(const NAME& param_name, DSTV& param) {
			auto it = __params.find(param_name);
			if (it == __params.end()) return false;
			param = (DSTV)it->second;
			return true;
		}
		template <typename DSTV> DSTV GetParamCasting(const NAME& param_name, const DSTV& init_v) {
			auto it = __params.find(param_name);
			if (it == __params.end()) return init_v;
			return (DSTV)it->second;
		}
		void SetParam(const NAME& param_name, const T& param) {
			__params[param_name] = param;
		}
		void RemoveParam(const NAME& param_name) {
			auto it = __params.find(param_name);
			if (it != __params.end()) {
				__params.erase(it);
			}
		}
		void RemoveAll() {
			__params.clear();
		}
		size_t Size() {
			return __params.size();
		}
		std::string GetPMapVersion() {
			return __PM_VERSION;
		}

		typedef std::unordered_map<NAME, T> MapType;
		typename typedef MapType::iterator iterator;
		typename typedef MapType::const_iterator const_iterator;
		typename typedef MapType::reference reference;
		iterator begin() { return __params.begin(); }
		const_iterator begin() const { return __params.begin(); }
		iterator end() { return __params.end(); }
		const_iterator end() const { return __params.end(); }
	};

	template <typename NAME, typename ANY> struct VmParamMap {
	private:
		std::string __PM_VERSION = "LIBI_1.4";
		std::unordered_map<NAME, ANY> __params;
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
			__params.clear();
		}
		size_t Size() {
			return __params.size();
		}
		std::string GetPMapVersion() {
			return __PM_VERSION;
		}

		typedef std::unordered_map<NAME, ANY> MapType;
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
	 * @brief Data structure defining a box axis-aligned with the current coordinate space
	 */
	struct AaBbMinMax {
		/// Minimum and maximum corner positions of the box axis-aligned with the current coordinate space
		vmdouble3 pos_min, pos_max;
		/// constructor; initializes everything to 0 (NULL or false)
		AaBbMinMax() { }
		/// Checks whether the AaBbMinMax is validly defined in the current coordinate space
		AaBbMinMax(vmint3 volSize) {
			pos_min = vmdouble3(-0.5, -0.5, -0.5);
			vmint3 idx_max = volSize - vmint3(1, 1, 1);
			pos_max = vmdouble3((double)idx_max.x, (double)idx_max.y, (double)idx_max.z) + vmdouble3(0.5, 0.5, 0.5);
		}
		bool IsAvailableBox() const {
			if (pos_max.x <= pos_min.x || pos_max.y <= pos_min.y || pos_max.z <= pos_min.z)
				return false;
			return true;
		}
	};

	/**
	 * @class AxisInfoRS2OS
	 * @brief Defines the orientation in which the Resource-Space axes x(1,0,0), y(0,1,0), z(0,0,1) are initially placed into Object Space (RHS). \n
	 * Pitch is not considered; only direction is defined (i.e. valid for vectors only)
	 * @sa
	 * @ref vmobjects::VolumeData
	 */
	struct AxisInfoRS2OS {
		/**
		 * @brief Defines the placed object's x-axis in Object Space corresponding to the Resource-Space x-axis (1,0,0); unit vector
		 */
		vmdouble3 vec_axisx_os;
		/**
		 * @brief Defines the placed object's y-axis in Object Space corresponding to the Resource-Space y-axis (0,1,0); unit vector
		 */
		vmdouble3 vec_axisy_os;
		/**
		 * @brief Whether the XY right-handed cross-product direction is reversed when defining the placed object's z-axis in World Space corresponding to the Object-Space z-axis (0,0,1)\n
		 * If true, it is placed right-handed and the transform holds in affine space; if false, the z-axis is placed left-handed
		 */
		bool is_rhs;
		/**
		 * @brief Initial RS2OS transform matrix derived from vec_axisx_ws, vec_axisy_ws, and is_rhs
		 */
		vmmat44 mat_rs2os;
		/**
		 * @brief constructor; performs initialization
		 * @details
		 * >> vec_axisx_ws = (1, 0, 0);
		 * >> vec_axisy_ws = (0, 1, 0);
		 * >> RHS;
		 * >> mat_os2ws is identity matrix
		 */
		AxisInfoRS2OS()
		{
			vec_axisx_os = vmdouble3(1, 0, 0);
			vec_axisy_os = vmdouble3(0, 1, 0);
			is_rhs = true;
			ComputeInitalMatrix();
		}
		AxisInfoRS2OS(vmdouble3 _vec_axisx_os, vmdouble3 _vec_axisy_os, bool _is_rhs)
		{
			vec_axisx_os = _vec_axisx_os;
			vec_axisy_os = _vec_axisy_os;
			is_rhs = _is_rhs;
			ComputeInitalMatrix();
		}
		/// Computes and registers mat_os2ws from the defined vec_axisx_ws and vec_axisy_ws
		void ComputeInitalMatrix() {
			vmdouble3 z_vec_rhs;
			vmmath::CrossDotVector(&z_vec_rhs, &vec_axisy_os, &vec_axisx_os); // note the z-dir in lookat
			vmmat44 matT;
			vmmath::MatrixWS2CS(&matT, &vmdouble3(0, 0, 0), &vec_axisy_os, &z_vec_rhs);
			vmmath::MatrixInverse(&mat_rs2os, &matT);
			if (!is_rhs)
			{
				vmmat44 matInverseZ;
				vmmath::MatrixScaling(&matInverseZ, &vmdouble3(1., 1., -1.));
				mat_rs2os = mat_rs2os * matInverseZ;
			}
		}
	};

	/**
	 * @class VolumeData
	 * @brief Data structure holding the detailed information of a volume as defined by the framework
	 * @sa
	 * @ref vmobjects::VmVObjectVolume \n
	 */
	struct VolumeData {
		/**
		 * @brief Data type of the volume array, <typeinfo>
		 */
		data_type store_dtype;
		/**
		 * @brief Original volume data type before it was stored in memory
		 */
		data_type origin_dtype;
		/**
		 * @brief 2D array storing the volume (safe-sample version)
		 * @details
		 * Actual allocated size along x = i3VolumeSize.x + i3SizeExtraBoundary.x*2 \n
		 * Actual allocated size along y = i3VolumeSize.y + i3SizeExtraBoundary.y*2 \n
		 * Actual allocated size along z = i3VolumeSize.z + i3SizeExtraBoundary.z*2 \n
		 * @par ex.
		 * Sampling the value at index (100, 120, 150) in a uint16_t 512x512x512 volume \n
		 * @par
		 * >> int iSamplePosX = 100 + i3SizeExtraBoundary.x; \n
		 * >> int iSamplePosY = 120 + i3SizeExtraBoundary.y; \n
		 * >> int iSamplePosZ = 150 + i3SizeExtraBoundary.z; \n
		 * >> uint16_t usValue = ((uint16_t**)ppvVolumeSlices)[iSamplePosZ][iSamplePosX + iSamplePosY*(i3VolumeSize.x + i3SizeExtraBoundary.x*2)];
		 */
	private:
		// (1.72, §4.2a) encapsulated raw field. Reassigning/freeing the slice array is a buffer-
		// destroying act that must go through the owner (VmVObjectVolume::ReplaceSlices/ReleaseSlices/
		// DeleteData), which bumps the incarnation first. Content is still mutable via GetVolSlices().
		void** vol_slices;
	public:
		// (1.72) Slice-content accessor. Returns the raw 2D slice array. The pointer stays valid until
		// an owner-only mutator replaces it, so this is content-mutable (the token contract is pointer
		// VALIDITY, not content immutability) and callable on an owner-const handle.
		void** GetVolSlices() const { return vol_slices; }
		// (1.72) Builder-only setter: non-const, so it does NOT compile on an owner-const handle taken
		// from GetVolumeData(). Local builder VolumeData (filled then handed to RegisterVolumeData) uses it.
		void SetVolSlices(void** slices) { vol_slices = slices; }
		/**
		 * @brief One-side thickness of the extra boundary region in system memory, used to avoid CPU memory access violations
		 * @details bnd_size = (one-side size along x, one-side size along y, one-side size along z)
		 */
		vmint3 bnd_size;
		/**
		 * @brief Volume size, vol_size = (width, height, depth or slices)
		 * @details bnd_size is not included
		 */
		vmint3 vol_size;
		/**
		 * @brief WS-space size of a single OS-space voxel cell
		 * @details vox_pitch = (OS-space voxel size along x, along y, along z)
		 */
		vmdouble3 vox_pitch;
		/**
		 * @brief Minimum (store_Mm_values.x) and maximum (store_Mm_values.y) of the stored volume (ppvVolumeSlices)
		 */
		vmdouble2 store_Mm_values;
		/**
		 * @brief Minimum (actual_Mm_values.x) and maximum (actual_Mm_values.y) defined before the volume was stored
		 * @par ex.
		 * e.g. when a volume stored as float in the range -1.5 ~ 2.5 is stored as uint16_t
		 * @par
		 * >> store_Mm_values = vmdouble2(0, 65535), actual_Mm_values = vmdouble2(-1.5, 2.5);
		 */
		vmdouble2 actual_Mm_values;
		/**
		 * @brief Array defining the histogram of the volume
		 * @details
		 * The array size is uint32_t(d2MinMaxValue.y - d2MinMaxValue.x + 1.5) \n
		 * pullHistogram[volume value] = # of voxels
		 */
		uint64_t* histo_values;
		/**
		 * @brief Transform matrix mapping the volume space stored in memory (sample coordinates) to its initial placement in world space
		 */
		AxisInfoRS2OS axis_info;
		/**
		 * @brief constructor; performs initialization
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
		 * @brief Returns the histogram array size, uint32_t(store_Mm_values.y - store_Mm_values.x + 1.5)
		 */
		uint32_t GetHistogramSize() const { return (uint32_t)((double)__max(store_Mm_values.y - store_Mm_values.x + 1.5, 1.0)); }
		/**
		 * @brief Returns the ppvVolumeSlices array size, including the extra boundary
		 */
		vmint3 GetSampleSize() const { return vmint3(vol_size.x + bnd_size.x * 2, vol_size.y + bnd_size.y * 2, vol_size.z + bnd_size.z * 2); }

		// Frees the memory allocated for the ppvVolumeSlices and pullHistogram pointers
		void Delete() {
			VMSAFE_DELETE2DARRAY_VOID(vol_slices, vol_size.z + bnd_size.z * 2);
			VMSAFE_DELETEARRAY(histo_values);
		}
		// (1.72) Frees ONLY the slice array (not the histogram) and nulls the field. Used by the
		// owner-only VmVObjectVolume::ReleaseSlices/ReplaceSlices mutators (they have no access to
		// the now-private vol_slices field).
		void DeleteSlices() {
			VMSAFE_DELETE2DARRAY_VOID(vol_slices, vol_size.z + bnd_size.z * 2);
		}
	};

	/**
	 * @class PrimitiveData
	 * @brief Data structure holding the detailed information of a primitive-based object as defined by the framework
	 * @sa vmobjects::VmVObjectPrimitive
	 */
	struct PrimitiveData {
	private:
		/**
		 * @brief Container map storing the vertex arrays
		 * string ==> POSITION, NORMAL, TEXCOORD[n], ...
		 * Holds the allocated pointers as values, which are freed in @ref PrimitiveData::Delete.
		 */
		std::map<std::string, uint8_t*> defined_vtxbuffers;
		std::map<std::string, uint8_t*> defined_custombuffers;
	public:
		/**
		 * @brief Vertex winding order of the object's polygons relative to their normal vectors
		 */
		bool is_ccw;	// will be deprecated
		/**
		 * @brief Primitive Type
		 */
		EvmPrimitiveType ptype;
		/**
		 * @brief How the primitive's vertices are arranged; true: strip, false: list
		 */
		bool is_stripe;
		/**
		* @brief Whether redundancy among the primitive's vertices and edges has been removed
		*/
		bool check_redundancy;
		/**
		 * @brief Number of polygons in the primitive-based object
		 */
		uint32_t num_prims;
		/**
		 * @brief Number of indices that define a single primitive (polygon)
		 */
		uint32_t idx_stride;
		/**
		 * @brief Size of the index buffer (puiIndexList) used to define polygons by vertex index
		 * @details
		 * >> if(is_stripe)\n
		 * >>    num_vidx = num_prims + (idx_stride - 1);\n
		 * >> else\n
		 * >>    num_vidx = num_prims * idx_stride;
		 */
		uint32_t num_vidx;
	private:
		// (1.72, §4.2a) encapsulated raw field. Reassigning/freeing the index buffer is a buffer-
		// destroying act that must go through the owner (VmVObjectPrimitive::ReplaceIndexBuffer/
		// ReleaseIndexBuffer/DeleteData), which bumps the incarnation first. Content stays mutable
		// via GetIndexBuffer().
		uint32_t* vidx_buffer;
	public:
		// (1.72) Index-buffer accessor. Returns the raw index array; the pointer stays valid until an
		// owner-only mutator replaces it (token contract = pointer VALIDITY), so it is content-mutable
		// and callable on an owner-const handle.
		uint32_t* GetIndexBuffer() const { return vidx_buffer; }
		// (1.72) Builder-only setter: non-const, so it does NOT compile on an owner-const handle taken
		// from GetPrimitiveData(). Local builder PrimitiveData (filled then handed to RegisterPrimitiveData)
		// uses it; object-owned data must use VmVObjectPrimitive::ReplaceIndexBuffer instead.
		void SetIndexBuffer(uint32_t* index_buffer) { vidx_buffer = index_buffer; }
		/**
		 * @brief Number of vertices in the primitive-based object
		 */
		uint32_t num_vtx;
		/**
		 * @brief Bounding box defined in OS at the PrimitiveData level
		 */
		AaBbMinMax aabb_os;
		/**
		 * @brief Information about the texture resource \n
		 * <w, h, bytes_stride, res_ptr>
		 */
		std::map<std::string, std::tuple<int, int, int, uint8_t*>> texture_res_info;

		bool GetTexureInfo(const std::string& desc, int& w, int& h, int& bytes_stride, uint8_t** res_ptr) const
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

		/// constructor; initializes everything to 0 (NULL or false)
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
		 * @brief Frees the memory allocated for the puiIndexList pointer and the pointers stored as values in defined_buffers
		*/
		void Delete() {
			VMSAFE_DELETEARRAY(vidx_buffer);
			for (auto it = texture_res_info.begin(); it != texture_res_info.end(); it++)
			{
				uint8_t* p = std::get<3>(it->second);
				VMSAFE_DELETEARRAY(p);
			}
			texture_res_info.clear();

			for (std::map<std::string, uint8_t*>::iterator itrVertex3D = defined_vtxbuffers.begin(); itrVertex3D != defined_vtxbuffers.end(); itrVertex3D++)
			{
				VMSAFE_DELETEARRAY(itrVertex3D->second);
			}
			defined_vtxbuffers.clear();
			for (std::map<std::string, uint8_t*>::iterator itrVertex3D = defined_custombuffers.begin(); itrVertex3D != defined_custombuffers.end(); itrVertex3D++)
			{
				VMSAFE_DELETEARRAY(itrVertex3D->second);
			}
			defined_custombuffers.clear();
		}
		/*!
		 * @fn vmfloat3* vmobjects::PrimitiveData::GetVerticeDefinition(const string& vtype)
		 * @brief Method returning the pointer stored as a value in defined_buffers,
		 * @param vtype [in] \n string \n Name of the vertex buffer (the key in defined_buffers) \n
		 * keys : POSITION, NORMAL, TEXCOORD[n], ...
		 * @return vmfloat3 \n Pointer to the vertex buffer; returns NULL if not found
		 */
		// (1.72) const-qualified so it is callable on an owner-const PrimitiveData taken from
		// GetPrimitiveData(). Returns a content-mutable pointer (token contract = pointer VALIDITY,
		// not content immutability); only reallocation/free is gated (ReplaceOrAdd*/Delete stay non-const).
		template<class T>
		T* GetVerticeDefinition(const std::string& vtype) const {
			std::map<std::string, uint8_t*>::const_iterator itrVtxDef = defined_vtxbuffers.find(vtype);
			if (itrVtxDef == defined_vtxbuffers.end())
				return NULL;
			return (T*)itrVtxDef->second;
		}
		uint8_t* GetCustomDefinition(const std::string& vtype) const {
			std::map<std::string, uint8_t*>::const_iterator itrVtxDef = defined_custombuffers.find(vtype);
			if (itrVtxDef == defined_custombuffers.end())
				return NULL;
			return (uint8_t*)itrVtxDef->second;
		}

		template<class T>
		const T* GetVerticeDefinitionConst(const std::string& vtype) const {
			std::map<std::string, uint8_t*>::const_iterator itrVtxDef = defined_vtxbuffers.find(vtype);
			if (itrVtxDef == defined_vtxbuffers.end())
				return NULL;
			return (T*)itrVtxDef->second;
		}

		const uint8_t* GetCustomDefinitionConst(const std::string& vtype) const {
			std::map<std::string, uint8_t*>::const_iterator itrVtxDef = defined_custombuffers.find(vtype);
			if (itrVtxDef == defined_custombuffers.end())
				return NULL;
			return (uint8_t*)itrVtxDef->second;
		}

		/*!
		 * @fn void vmobjects::PrimitiveData::ReplaceOrAddVerticeDefinition(const string& vtype, vmfloat3* vtx_buffer)
		 * @brief Method returning the pointer stored as a value in defined_buffers,
		 * @param vtype [in] \n string \n Name of the vertex buffer: POSITION, NORMAL, TEXCOORD[n], ...
		 * @param vtx_buffer [in] \n vmfloat3* \n Pointer to the vmfloat3 array defining the vertex buffer.
		 * @remarks Because vtype is used as the key, if a vertex definition is already registered, \n
		 * its existing vertex pointer is freed from memory and the new vertex pointer is registered
		 */
		void ReplaceOrAddVerticeDefinition(const std::string& vtype, void* vtx_buffer) {
			uint8_t* vtx_buffer_old = GetVerticeDefinition<uint8_t>(vtype);
			if (vtx_buffer_old != NULL)
			{
				VMSAFE_DELETEARRAY(vtx_buffer_old);
				defined_vtxbuffers.erase(vtype);
			}
			defined_vtxbuffers.insert(std::pair<std::string, uint8_t*>(vtype, (uint8_t*)vtx_buffer));
		}
		void ReplaceOrAddCustomDefinition(const std::string& vtype, void* buffer) {
			uint8_t* buffer_old = GetCustomDefinition(vtype);
			if (buffer_old != NULL)
			{
				VMSAFE_DELETEARRAY(buffer_old);
				defined_custombuffers.erase(vtype);
			}
			defined_custombuffers.insert(std::pair<std::string, uint8_t*>(vtype, (uint8_t*)buffer));
		}
		/*!
		 * @fn int vmobjects::PrimitiveData::GetNumVertexDefinitions()
		 * @brief Returns the number of registered vertex definitions
		 * @return int \n Number of registered vertex definitions
		 */
		int GetNumVertexDefinitions() const
		{
			return (int)defined_vtxbuffers.size();
		}
		/*!
		 * @fn int vmobjects::PrimitiveData::GetNumCustomDefinitions()
		 * @brief Returns the number of registered custom-buffer definitions (e.g. FACECOLOR).
		 * @remarks Used by deep-copy paths to detect custom channels whose byte length is not
		 * derivable here, so they can be refused rather than aliased (double-free) or silently dropped.
		 */
		int GetNumCustomDefinitions() const
		{
			return (int)defined_custombuffers.size();
		}
		/*!
		 * @fn void vmobjects::PrimitiveData::ClearVertexDefinitionContainer()
		 * @brief Clears mapVerticeDefinitions.
		 * @remarks Only the container is cleared; the memory of the registered vertex pointers is not freed.
		 */
		void ClearVertexDefinitionContainer()
		{
			defined_vtxbuffers.clear();
		}
		void ClearCustomDefinitionContainer()
		{
			defined_custombuffers.clear();
		}
		/*!
		* @fn void vmobjects::PrimitiveData::ComputeOrthoBoundingBoxWithCurrentValues()
		* @brief Computes the AABB min/max from the positions in the POSITION vtx_buffer registered in defined_buffers.
		* @remarks Unnecessary if the PrimitiveData's aabb_os has already been computed; otherwise this must be run to define the AABB.
		*/
		void ComputeOrthoBoundingBoxWithCurrentValues()
		{
			vmfloat3* vtx_buffer = GetVerticeDefinition<vmfloat3>("POSITION");
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
	 * @brief Data structure holding the detailed information of an OTF as defined by the framework
	 */
	struct MapTable {
		/**
		 * @brief Pointer to the OTF array
		 * @details
		 * 1D : [0][0 to (array_lengths.x - 1)] - default, [1][0 to (array_lengths.x - 1)] - customized
		 * 2D : [0][0 to (array_lengths.x*array_lengths.y - 1)] - default, [...][0 to (array_lengths.x*array_lengths.y - 1)] - customized
		 * 3D : [0 to (array_lengths.z - 1)][0 to (array_lengths.x*array_lengths.y - 1)]
		 */
		void** tmap_buffers;
		/**
		 * @brief Pointer dimensionality of the OTF array
		 * @details num_dim = 1 or 2 or 3
		 */
		int num_dim;
		/**
		 * @brief Minimum valid OTF array index for each allocated dimension
		 * @details
		 * valid_min_idx.x : minimum array index of the 1st dimension \n
		 * valid_min_idx.y : minimum array index of the 2nd dimension \n
		 * valid_min_idx.z : minimum array index of the 3rd dimension
		 */
		vmint3 valid_min_idx;
		/**
		 * @brief Maximum valid OTF array index for each allocated dimension
		 * @details
		 * valid_max_idx.x : maximum array index of the 1st dimension \n
		 * valid_max_idx.y : maximum array index of the 2nd dimension \n
		 * valid_max_idx.z : maximum array index of the 3rd dimension
		 */
		vmint3 valid_max_idx;
		/**
		 * @brief Size of the OTF array along each dimension
		 * @details
		 * array_lengths.x : array size of the 1st dimension \n
		 * array_lengths.y : array size of the 2nd dimension \n
		 * array_lengths.z : array size of the 3rd dimension \n
		 * For valid dimensions array_lengths.xyz > 0; for invalid dimensions array_lengths.xyz <= 0
		 */
		vmint3 array_lengths;
		/**
		 * @brief Bin size over the range of volume values that the OTF metric is based on
		 * @par ex.
		 * For a 2D OTF (density, gradient magnitude) over 16-bit volume data, assuming each range is 0~65535 \n
		 * defining a 512x1024 2D OTF makes the bin's XY size (65536/512, 65536/1024).
		 */
		vmdouble3 bin_size;
		/**
		 * @brief Data type of the OTF array values
		 */
		data_type dtype;
		/**
		 * @brief constructor; initializes everything to 0 (NULL or false)
		 */
		MapTable() {
			tmap_buffers = NULL;
			num_dim = 0;
			valid_min_idx = valid_max_idx = bin_size = vmdouble3(0);
			array_lengths = vmint3(0);
		}

		// Static Helper Functions //
		/*!
		 * @brief Static helper function that allocates the OTF array stored in VolumeData
		 * @param num_dim [in] \n int \n OTF dimension
		 * @param dim_length [in] \n int 3 \n Size of each OTF dimension
		 * @param dtype [in] \n data_type \n Data type of the OTF array
		 * @param res_tmap [in] \n void \n Pointer to the void** of the 2D OTF array (a 3D pointer)
		 * @return bool \n Returns true on success, false on failure
		 * @remarks The OTF array is always stored as a 2D OTF
		 * @sa vmobjects::TMapData
		 */
		bool CreateTMapBuffer(const int num_dim, const vmint3& dim_length)
		{
			if (num_dim <= 0 || num_dim > 3)
			{
				printf("TMapData::CreateTMapBuffer - UNAVAILABLE INPUT");
				return false;
			}

			switch (num_dim)
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
		 * @brief Frees the memory allocated for the OTF array pointer ppvArchiveTF
		*/
		void Delete() {
			switch (num_dim)
			{
			case 1:
			case 2:
				VMSAFE_DELETE2DARRAY_VOID(tmap_buffers, array_lengths.y);
				break;
			case 3:
				VMSAFE_DELETE2DARRAY_VOID(tmap_buffers, array_lengths.z);
				break;
			default:
				break;
			}
		}
	};

	/**
	 * @class VolumeBlocks
	 * @brief Data structure for a block-based volume
	 * @sa vmobjects::VmVObjectVolume, vmobjects::VolumeData
	 */
	struct VolumeBlocks {
		/**
		 * @brief Size of a single block
		 * @details Size excluding the extra boundary \n
		 * unitblk_size = vmint3(size of x, size of y, size of z)
		 */
		vmint3 unitblk_size;
		/**
		* @brief Extra-boundary size of mM_blks and pbTaggedActivatedBlocks, which store the block information
		* @details Sampling values from mM_blks and pbTaggedActivatedBlocks must account for this \n
		*/
		vmint3 blk_bnd_size;
		/**
		 * @brief Number of blocks defined along each axis within a single VolumeBlocks
		 * @details Total number of blocks = blk_vol_size.x * blk_vol_size.y * blk_vol_size.z;
		 */
		vmint3 blk_vol_size;
		/**
		 * @brief Single-channel data type of the block's min/max values
		 * @details Normally the same as vmobjects::VolumeData.store_dtype
		 */
		data_type dtype;
		/**
		 * @brief 1D array storing the per-block min/max values
		 * @details
		 * The array size equals the total number of blocks and does not account for the extra boundary \n
		 * The data type has 2 channels and matches the volume type. x : minimum, y : maximum
		 * @par ex.
		 * Min/max of the block at OS coordinate (100, 100, 100) in a 512x512x512 volume (uint16_t) partitioned into 8x8x8 blocks \n
		 * In this case unitblk_size = vmint3(8, 8, 8), blk_vol_size = (ceil(512/8), ceil(512/8), ceil(512/8)) \n
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
		 * @brief 1D array of per-block binary tags, defined per object
		 * @details
		 * The array size equals the total number of blocks and does not account for the extra boundary \n
		 * When the resource manager deletes a TObject, it cleans up the resources of the registered volume object's blocks \n
		 * Pointer operations are available via VolumeBlocks::GetTaggedActivatedBlocks and VolumeBlocks::GetTaggedActivatedBlocks \n
		 * @par ex.
		 * Tag of the block at OS coordinate (100, 100, 100) in a 512x512x512 volume (uint16_t) partitioned into 8x8x8 blocks \n
		 * In this case unitblk_size = vmint3(8, 8, 8), blk_vol_size = (ceil(512/8), ceil(512/8), ceil(512/8)) \n
		 *
		 * @par
		 * >> uint8_t* tflag_blks = itratorMap->second;
		 * >> vmint3 blk_id = vmint3(floor(100/8), floor(100/8), floor(100/8)); \n
		 * >> int blk_idx = blk_id.x + blk_bnd_size.x
		 * >>                     + (blk_id.y + blk_bnd_size.y)*(blk_id.x + 2*blk_bnd_size.x)
		 * >>                     + (blk_id.z + blk_bnd_size.z)*(blk_id.x + 2*blk_bnd_size.x)*(blk_id.y + 2*blk_bnd_size.y); \n
		 * >> byte tflag = tflag_blks[blk_idx];
		 */
		std::map<int, uint8_t*> tflag_blks_map;
		std::map<int, uint64_t> updatetime_map;

		/// constructor; initializes everything to 0 (NULL or false)
		VolumeBlocks() {
			unitblk_size = blk_vol_size = blk_bnd_size = vmint3(0);
			mM_blks = NULL;
		}

		/*!
		 * @fn void vmobjects::VolumeBlocks::Delete()
		 * @brief Frees all allocated memory
		*/
		void Delete() {
			VMSAFE_DELETEARRAY_VOID(mM_blks);
			for (std::map<int, uint8_t*>::iterator itr = tflag_blks_map.begin(); itr != tflag_blks_map.end(); itr++)
				VMSAFE_DELETEARRAY(itr->second);
			tflag_blks_map.clear();
			updatetime_map.clear();
		}

		/*!
		* @fn void vmobjects::VolumeBlocks::GetTaggedActivatedBlocks(int iTObjectID)
		* @brief Returns the tagged-activated-blocks pointer (uint8_t* tflag_blks) for the given TObjectID
		*/
		uint8_t* GetTaggedActivatedBlocks(int tobj_id)
		{
			std::map<int, uint8_t*>::iterator itr = tflag_blks_map.find(tobj_id);
			if (itr == tflag_blks_map.end())
				return NULL;
			return itr->second;
		}

		uint64_t GetUpdateTime(int tobj_id)
		{
			std::map<int, uint64_t>::iterator itr = updatetime_map.find(tobj_id);
			if (itr == updatetime_map.end())
				return 0;
			return itr->second;
		}

		/*!
		* @fn void vmobjects::VolumeBlocks::ReplaceOrAddTaggedActivatedBlocks(int tobj_id, uint8_t* tflag_blks)
		* @brief Registers the tagged-activated-blocks for the given TObjectID
		*/
		bool ReplaceOrAddTaggedActivatedBlocks(const int tobj_id, uint8_t* tflag_blks)
		{
			std::map<int, uint8_t*>::iterator itr = tflag_blks_map.find(tobj_id);
			if (itr != tflag_blks_map.end())
				VMSAFE_DELETEARRAY(itr->second);

			tflag_blks_map[tobj_id] = tflag_blks;
			return true;
		}

		/*!
		* @fn void vmobjects::VolumeBlocks::DeleteTaggedActivatedBlocks()
		* @brief Deletes the tagged-activated-blocks for the given TObjectID
		*/
		void DeleteTaggedActivatedBlocks(const int tobj_id)
		{
			std::map<int, uint8_t*>::iterator itr = tflag_blks_map.find(tobj_id);
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
	 * @brief Data structure holding the detailed information of a frame buffer as defined by the framework
	 * @sa vmobjects::VmIObject
	 */
	struct FrameBuffer {
		/**
		 * @brief Width of the frame buffer
		 */
		int w;
		/**
		 * @brief Height of the frame buffer
		 */
		int h;
		/**
		 * @brief Frame buffer defined as an array
		 */
		void* fbuffer;
		/**
		 * @brief Data type of the frame buffer
		 */
		data_type dtype;
		/**
		 * @brief Intended usage of the frame buffer
		 * @details When buffer_usage == FrameBufferUsageRENDEROUT, it must be set to vmbyte4.
		 */
		EvmFrameBufferUsage buffer_usage;
		/**
		 * @brief Descriptor of the frame buffer
		 */
		std::string descriptor;

#ifdef __WINDOWS
		/**
		 * @brief Handle for buffer interoperation through file memory on win32
		 */
		HANDLE hFileMap;
#endif

		/// constructor; initializes everything to 0 (NULL or false)
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
		 * @brief Frees all allocated memory
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
			default: VMSAFE_DELETEARRAY_VOID(fbuffer); break;
			}
		}
	};


	//=========================
	// Global Objects
	//=========================
	struct ObjectArchive;
	/**
	 * @class VmObject
	 * @brief Topmost class of the VizMotive framework objects, holding the common parameters of the VmObject family
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
		 * @brief Checks whether the VmObject's contents are defined
		 * @remarks Contents are defined by the leaf-most VmObject subclass
		 * @li @ref vmobjects::VmIObject
		 * @li @ref vmobjects::VmVObjectVolume
		 * @li @ref vmobjects::VmVObjectPrimitive
		 */
		bool IsDefined();
		/*!
		 * @brief Sets the VmObject's object ID
		 * @param obj_id [in] \n int \n 32 bit ID
		 * @remarks
		 * Normally assigned by the resource manager (@ref VmResourceManager) \n
		 * [8bit : Object Type][8bit : Magic Bits][16 bit : Count-based ID]
		 */
		void SetObjectID(const int obj_id);
		/*!
		 * @brief Returns the VmObject's object ID
		 * @return int \n Returns the object ID
		 */
		int GetObjectID() const;
		/*!
		 * @brief Sets the ID of the most closely related VmObject used to define this VmObject
		 * @param ref_obj_id [in] \n int \n ID of the most closely related VmObject used to define this VmObject
		 * @remarks The default ID is 0
		 */
		void SetReferenceObjectID(const int ref_obj_id);
		/*!
		 * @brief Returns the ID of the most closely related VmObject used to define this VmObject
		 * @return int \n ID of the most closely related VmObject used to define this VmObject
		 * @remarks Returns 0 if no related VmObject ID has been set
		 */
		int GetReferenceObjectID() const;
		/*!
		 * @brief Sets the user description for the VmObject
		 * @param str [in] \n string \n User description to store for the VmObject
		 */
		void SetDescriptor(const std::string& str);
		/*!
		 * @brief Returns the user description of the VmObject
		 * @return wstring \n Returns the user description of the VmObject
		 */
		std::string GetDescriptor() const;
		
		/*!
		 * @brief Returns the type of the defined VmObject
		 * @return ObjectType:: \n Returns the type of the defined VmObject
		 * @remarks When the VmObject is defined, its instance is created according to this type
		 */
		EvmObjectType GetObjectType();

		uint64_t GetContentUpdateTime();

		void SetContentUpdateTime();

		// ------------------------------------------------------------------
		// (1.72, §4.2a) Resource incarnation / generation token.
		// The ONLY contract the token expresses: "same token => a pointer previously
		// returned by GetPrimitiveData()/GetVolumeData() is still valid".
		//   token = (uint64_t(birth) << 32) | mutation
		// * birth   : a PROCESS-LIFETIME monotonic id issued by the SINGLE CommonApi
		//             ResourceManager at RegisterObject (never reused, never reset).
		//             Stored here so GetResObjGeneration can read it back.
		// * mutation: an owner-local counter bumped exactly once BEFORE every buffer-
		//             destroying operation (Register*Data / owner-only mutators).
		// * poison  : latched when mutation would saturate; a poisoned incarnation
		//             never yields a token again (fail-closed).
		// ------------------------------------------------------------------
		// Set once by the ResourceManager at registration. 0 means "not yet issued".
		void SetResBirth(const uint32_t birth);
		uint32_t GetResBirth() const;
		// Composes token from (birth, mutation). Returns false if birth==0 (unissued)
		// or the incarnation is poisoned.
		bool GetResGenerationToken(uint64_t& token) const;
		// Monotonic bump of the owner-local mutation counter, performed BEFORE the
		// destructive act. On saturation the incarnation is latched to poison.
		void BumpResMutation();
		bool IsResIncarnationPoisoned() const;

		void SetDestoryer(const std::string& name, void(*fn)(VmObject* obj));
		bool ContainsDestroyer(const std::string& name);

		bool RemoveDestoryers();
		bool RemoveDestoryer(const std::string& name);
		
		void SetObjParam(const std::string& param_name, const std::any& v);

		template <typename T> T* GetObjParamPtr(const std::string& param_name) {
			bool ret = false;
			std::any& p = GetObjParamA(param_name, ret);
			return ret? (T*)&std::any_cast<T&>(p) : NULL;
		}
		template <typename T> T GetObjParam(const std::string& param_name, const T& init_v) {
			T* p = GetObjParamPtr<T>(param_name);
			return p == NULL ? init_v : *p;
		}

		bool RemoveObjParameters();
		bool RemoveObjParameter(const std::string& _key);
		// Static Helper Functions //
		/*!
		 * @brief Static helper function that returns the object type from a VmObject ID
		 * @param obj_id [in] \n int \n ID of the VmObject
		 * @return ObjectType \n Object type encoded in the VmObject ID
		 * @remarks
		 *
		 */
		static EvmObjectType GetObjectTypeFromID(const int obj_id);
		/*!
		 * @brief Static helper function that checks, from a VmObject ID, whether the object type is a VObject
		 * @param obj_id [in] \n int \n ID of the VmObject
		 * @return bool \n Returns true if it is a VmVObject, false otherwise
		 * @remarks
		 * Supported for the VmObject ID format.\n
		 */
		static bool IsVObject(const int obj_id);
	};

	struct VObjectArchive;
	/**
	 * @class VmVObject
	 * @brief Base class inheriting from VmObject that holds the spatial information shared by VmVObjectVolume and VmVObjectPrimitive
	 * @sa vmobjects::VmVObjectVolume, vmobjects::VmVObjectPrimitive
	 */
	// (1.70) Live-instance census. Every VmObject ctor/dtor adjusts one CommonUnits-side atomic counter, so this
	// returns how many VmObject instances (volume/primitive/iobj/tobj/...) are still alive. The API layer logs an
	// error at DeinitEngineLib when it is non-zero, i.e. something outlived engine teardown.
	__vmstatic int GetLiveVmObjectCount();

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
		 * @brief Returns the axis-aligned bounding box in OS that contains the content
		 * @param aabbMm [out] \n AaBbMinMax \n Axis-aligned bounding box in object space (OS)
		 */
		void GetOrthoBoundingBox(AaBbMinMax& aabbMm_os);
	
		/*!
		 * @brief Checks whether the OS-to-WS placement has been defined
		 * @return bool \n true if defined, false otherwise
		 */
		bool IsGeometryDefined();
	
		// only to_model_space is available.. from ver. 1.10
		// transforms between OS and WS are moved into actor parameters (stored in LObject)
		// here, RS normally refers to volume space (indexing the memory address), and MS refers to dicom-specified model
		// Transform //
		/*!
		 * @brief Sets the matrix defining the transform between the VmVObject's Resource Space (RS) and Model Space (MS)
		 * @param mat_os2ws [in] \n double44 \n Matrix defining the RS-to-MS transform to store
		 * @remarks Internally the coordinate spaces related to the RS/MS transform are reset, along with the associated matrices
		 */
		void SetMatrixRS2OS(const vmmat44& mat_rs2os);
		void SetMatrixRS2OSf(const vmmat44f& mat_rs2os);
		/*!
		 * @brief Returns the matrix defining the RS-to-MS transform stored in the VmVObject
		 * @return double44 \n Matrix defining the OS-to-WS transform
		 */
		vmmat44 GetMatrixRS2OS();
		vmmat44f GetMatrixRS2OSf();
		/*!
		 * @brief Returns the matrix defining the RS-to-MS transform stored in the VmVObject
		 * @return double44 \n Matrix defining the MS-to-RS transform
		 */
		vmmat44 GetMatrixOS2RS();
		vmmat44f GetMatrixOS2RSf();
	};

	/**
	 * @class VmVObjectVolume
	 * @brief Class holding volume information whose OS-to-WS placement is established through VmVObject
	 * @sa vmobjects::VmVObject
	 */
	__vmstaticclass VmVObjectVolume : public VmVObject	// CT Volume or Processing Result Volume or Histogram (2D : Size(x, y, 1))
	{
	public:
		// (1.70) leaf type -> pure-virtual interface; impl (holding the volume data as direct members) is
		// VmVObjectVolume_Detail, hidden in VimCommon.cpp. Construct via NewVObjectVolume(). Static utilities stay.
		virtual ~VmVObjectVolume() {}

		// Basic Functions //
		// Block & Brick for Interactive Rendering //
		// Not Hierarchical blocking
		// Octree : level 0, Large Block,  level 1, Small Block
		/*!
		 * @brief Registers the @ref vmobjects::VolumeData structure holding the volume information into the VmVObjectVolume
		 * @param vol_data [in] \n VolumeData \n VolumeData with the volume information defined
		 * @param blk_size2[2] [in] \n int3 \n
		 * A static array of size 2 holding the block sizes of the unit-block structure that defines the volume \n
		 * When block sizes are given, the per-block min/max structures are created internally, but the volume itself is not reorganized into blocks \n
		 * blk_size2[0] : large block, blk_size2[1] : small block \n
		 * If NULL, no blocks are created
		 * @param ref_obj_id [in] \n int \n
		 * ID of the VmVObjectVolume to share the content's pointer reference with \n
		 * Normally the copy method is used without pointer sharing, in which case 0 is used. The default is 0
		 * @param progress [out](optional) \n LocalProgress \n
		 * Pointer to a LocalProgress carrying the function's progress information \n
		 * The default is NULL; if NULL, it is not used.
		 * @remarks If blk_size2 is given, @ref VmVObjectVolume::GenerateVolumeMinMaxBlocks is called internally,
		 */
		virtual bool RegisterVolumeData(const VolumeData& vol_data, vmint3 blk_size2[2]/* 0 : Large, 1: Small */, const int ref_obj_id = 0, LocalProgress* progress = NULL) = 0;
		/*!
		 * @brief Returns the volume information defined in the VmVObjectVolume.
		 * @return const VolumeData* \n Pointer to the VolumeData holding the volume information
		 * @remarks (1.72, §4.2a) The general return type is now const: an object-owned VolumeData
		 * handle cannot be reallocated/freed (VolumeData::Delete and SetVolSlices are non-const,
		 * so they do not compile on this handle). Slice CONTENT is still mutable via GetVolSlices().
		 * Buffer-destroying changes must go through the owner-only mutators below (each bumps the
		 * incarnation token first).
		 */
		virtual const VolumeData* GetVolumeData() = 0;

		// (1.72, §4.2a) owner-only destructive mutators. Each bumps the object's incarnation
		// (BumpResMutation) BEFORE the destructive act, so a stale copy=false View is invalidated
		// before the pointer it holds can dangle. Order invariant: change the token FIRST, then free.
		// Frees the current slice array (and metadata) and clears the volume definition.
		virtual void DeleteData() = 0;
		// Frees only the current slice array (histogram/metadata retained by the caller's discipline).
		virtual void ReleaseSlices() = 0;
		// Frees the current slice array and adopts a new one (ownership transferred to the object).
		virtual void ReplaceSlices(void** new_slices) = 0;

		// Optional //
		/*!
		* @brief Updates the @ref VolumeBlock structure holding the per-block min/max values after the volume's internal values change
		* @param progress [out](optional) \n LocalProgress \n
		* Pointer to a LocalProgress carrying the function's progress information \n
		* The default is NULL; if NULL, it is not used.
		* @param i3BlockSizes[2] [in](optional) \n vmint3[] \n
		* Block sizes to create. The array index denotes the level.
		* The default is NULL; if NULL, blocks are reused or regenerated per internal logic.
		* @return bool \n Returns true if the update succeeds, false otherwise.
		* @remarks
		* Only refreshes the values of the existing min/max blocks; if none exist, blocks are created internally via VmVObjectVolume::GenerateVolumeMinMaxBlocks \n
		*/
		virtual bool UpdateVolumeMinMaxBlocks(LocalProgress* progress = NULL, const vmint3 blk_size2[2] = NULL) = 0;
		
		/*!
		 * @brief Returns the volume's block structure
		 * @param level [in] \n int \n Block level, 0 or 1
		 * @return VolumeBlocks \n
		 * Pointer to the VolumeBlocks holding the volume's block structure \n
		 * Returns NULL if the volume or block structure is undefined, or if the level value is invalid.
		 */
		virtual VolumeBlocks* GetVolumeBlock(const int level) = 0;	// 0 or 1

		/*!
		 * @brief Updates the tags of the blocks whose values fall within the min/max range configured in the volume's block structure
		 * @param tobj_id [in] \n int \n TObject ID semantically bound to the block
		 * @param level [in] \n int \n Block level, 0 or 1
		 * @param targetMm [in] \n double2 \n Minimum (x) and maximum (y) to use \n
		 * @param progress [out] \n LocalProgress \n
		 * Pointer to a LocalProgress carrying the function's progress information \n
		 * The default is NULL; if NULL, it is not used.
		 * @remarks
		 * The existing min/max blocks must already be registered in the class \n
		 * @sa vmobjects::VolumeBlocks
		 */
		virtual void UpdateTagBlocks(const int tobj_id, const int level, const vmdouble2& targetMm, LocalProgress* progress = NULL) = 0;

		/*!
		 * @fn void FillBoundaryWithValue(const double v, const bool clamp_z = false, LocalProgress* progress = NULL)
		 * @brief Fills the extra-boundary volume region defined in VolumeData with the volume's minimum value
		 * @param v [in] \n double \n Volume value to fill into the volume's extra boundary
		 * @param clamp_z [in] \n bool \n Whether to fill the z-axis extra boundary by clamping (replicating the border value)
		 * @param progress [in](optional)
		 * LocalProgress \n Pointer to a LocalProgress that tracks the current progress \n
		 * The default is NULL, in which case the function runs without tracking progress
		 * @return true : success, false : failure
		 * @remarks
		 * vol_slices must be defined. \n
		 * If clamp_z is false, the extra-boundary region is filled with the value v
		*/
		static bool FillBoundaryWithValue(VolumeData& vol_data, const double v, const bool clamp_z, LocalProgress* progress = NULL);

		/*!
		 * @fn void FillHistogram(LocalProgress* progress = NULL)
		 * @brief Builds the histogram for the volume defined in VolumeData
		 * @param progress [in](optional) \n
		 * LocalProgress \n Pointer to a LocalProgress that tracks the current progress \n
		 * The default is NULL, in which case the function runs without tracking progress
		 * @return true : success, false : failure
		 * @remarks
		 * If one already exists, it is deleted and then rebuilt and redefined \n
		 * The histogram array (histo_values) size is set to uint32_t(store_Mm_values.y - store_Mm_values.x + 1.5)
		*/
		static bool FillHistogram(VolumeData& vol_data, LocalProgress* progress = NULL);
		static bool FillMinMaxStoreValues(VolumeData& vol_data, LocalProgress* progress = NULL);
		static bool ComputeIntialAlignmentMatrixRS2OS(vmmat44& mat_rs2os, AxisInfoRS2OS& axis_info, const vmdouble3& vox_pitch, const AaBbMinMax& aabbMm_rs);
	};
	__vmstatic VmVObjectVolume* NewVObjectVolume(); // (1.70) factory; VmVObjectVolume_Detail hidden in VimCommon.cpp

	/**
	 * @class VmIObject camera doc (camera state now lives on fncontainer::VmCamera)
	 * @brief Class, included as a single instance in VmIObject, that handles camera-related information
	 * @remarks 
	 * The spaces used by this class are as follows.
	 * @li WS (World Space) : the real world where the camera and objects are placed.
	 * @li CS (Camera Space or Viewing Space) : camera-relative space, same units as WS \n
	 * origin : camera position, y-axis : up vector, -z-axis : viewing direction
	 * @li PS (Projection Space) : space defined as a normalized cube-shaped frustum from the interior of the CS view frustum \n
	 * origin : the point where the near plane meets the viewing direction.\n
	 * The y and z axes match the CS directions, but are scaled so that the length defined by the view frustum is normalized to 1.
	 * @li SS (Screen Space or Window Space) : space corresponding to buffer pixels \n
	 * origin : the top-left of the z = 0 plane of the normalized view frustum in PS \n
	 * x-axis : same as the PS x-axis, y-axis : PS -y-axis, z-axis : PS -z-axis \n
	 * xy scaling : the resolution of the buffer that defines the screen or window \n
	 * z scaling : 1 (i.e. the PS z value with its sign flipped)
	 * @remarks The image plane is defined on the near plane.
	 * @sa vmobjects::VmIObject
	 */
	// (1.70) VmLens was REMOVED: its optics/projection/pose + WS<->SS matrices are now plain fields on
	// fncontainer::VmCamera, and the matrix math lives in the CommonApi layer (UpdateCameraTransforms, run during
	// the scene-tree update). VmIObject holds a borrowed VmCamera* as its "camera object".

	struct IObjectArchive;
	/**
	 * @class VmIObject
	 * @brief VmObject-derived render-target: holds image-plane buffers and references (non-owning) one camera (fncontainer::VmCamera).
	 * @remarks
	 * For a single resolution (width, height), several image buffers (frame buffers) for various uses are defined. \n
	 * The connected camera (fncontainer::VmCamera) is referenced non-owningly; the VmCamera scene actor owns it.
	 * @sa vmobjects::VmObject, fncontainer::VmCamera
	 */
	__vmstaticclass VmIObject : public VmObject
	{
	private:
	protected:
		IObjectArchive* ioa_res;

	public:
		/*!
		 * @brief constructor; requires the resolution that defines the frame buffer mapped to the image plane
		 * @param w [in](optional) \n int \n Resolution width (pixels); default 0
		 * @param h [in](optional) \n int \n Resolution height (pixels); default 0
		 * @remarks If width or height is 0 or less, the frame-buffer creation functions (@ref VmIObject::ResizeFrameBuffer, @ref  VmIObject::InsertFrameBuffer) fail
		 */
		VmIObject(const int w = 0, const int h = 0);
		~VmIObject();

		/*!
		 * @brief Resizes the defined frame buffers
		 * @param w [in] \n int \n Resolution width (pixels), 1 or more
		 * @param h [in] \n int \n Resolution height (pixels), 1 or more
		 * @remarks
		 * Frees the previously defined frame buffers from memory, then reallocates them at the given size\n
		 * The contents stored in the frame buffers are also discarded (a module or function must be called to refill them)\n
		 * The image plane's pixel x-pitch and y-pitch are assumed equal, so the width-to-height ratio changes\n
		 * Accordingly, the WS image-plane information defined by VmCamera is reset, along with the associated transform matrices.
		 */
		void ResizeFrameBuffer(const int w, const int h);
		/*!
		 * @brief Returns information about the defined frame buffers
		 * @param buffer_size [out] \n int 2 \n Pointer to receive the frame buffer resolution: width(x), height(y)
		 * @param num_buffers [out](optional) \n int \n Pointer to receive the number of currently defined frame buffers
		 * @param bytes_per_pixel [out](optional) \n int \n Pointer to receive the summed byte size of the per-pixel types across all currently defined frame buffers
		 * @remarks Passing NULL for a parameter you do not need skips storing that value.
		 */
		void GetFrameBufferInfo(vmint2* buffer_size/*out*/, int* num_buffers = NULL/*out*/, int* bytes_per_pixel = NULL/*out*/);
		/*!
		 * @brief Returns the @ref vmobjects::FrameBuffer (including its array) holding the defined frame buffer's information
		 * @param fb_usage [in] \n EvmFrameBufferUsage \n Retrieves the buffer of this usage among the defined frame buffers
		 * @param buffer_idx [in] \n int \n Retrieves the index-th buffer among the frame buffers of that usage
		 * @return FrameBuffer \n Pointer to the @ref vmobjects::FrameBuffer (including its array) holding the frame buffer's information
		 */
		FrameBuffer* GetFrameBuffer(const EvmFrameBufferUsage fb_usage, const int buffer_idx);

		/*!
		 * @brief Adds a single frame buffer
		 * @param dtype [in] \n data_type \n Data type of the frame buffer to add
		 * @param fb_usage [in] \n EvmFrameBufferUsage \n Usage of the frame buffer to add
		 * @param descriptor [in] \n string \n Descriptor for the frame buffer to add
		 * @remarks When fb_usage == vmenums::EvmFrameBufferUsage::FrameBufferUsageRENDEROUT, the data type must be typeid(vmbyte4).name().
		 */
		void InsertFrameBuffer(const data_type& dtype, const EvmFrameBufferUsage fb_usage, const std::string& descriptor);

		/*!
		 * @brief Replaces a frame buffer
		 * @param fb_usage [in] \n EvmFrameBufferUsage \n Usage of the frame buffer to replace
		 * @param buffer_idx [in] \n int \n Index of the frame buffer to replace
		 * @param dtype [in] \n data_type \n New data type for the frame buffer
		 * @param descriptor [in] \n string \n New descriptor for the frame buffer
		 * @return bool \n Returns true if a buffer exists at buffer_idx, false otherwise
		 * @remarks If the buffer at that index is already declared with dtype, returns true without doing anything
		 */
		bool ReplaceFrameBuffer(const EvmFrameBufferUsage fb_usage, const int buffer_idx, const data_type& dtype, const std::string& descriptor);

		/*!
		 * @brief Deletes a frame buffer
		 * @param fb_usage [in] \n EvmFrameBufferUsage \n Usage of the frame buffer to delete
		 * @param buffer_idx [in] \n int \n Deletes the index-th buffer among the frame buffers of that usage (eFrameBufferUsage)
		 * @return Returns true if the frame buffer exists and is deleted successfully, false otherwise
		 * @remarks The frame buffer (fb_usage && buffer_idx) is freed from memory.
		 */
		bool DeleteFrameBuffer(const EvmFrameBufferUsage fb_usage, const int buffer_idx);

		/*!
		 * @brief (1.70) DEPRECATED no-op. Camera state lives on the VmCamera actor; nothing is created here.
		 * @param aabbMm [ignored] retained for source/ABI compatibility; the no-op discards it.
		 * @param stage_vtype [ignored] retained for source/ABI compatibility; the no-op discards it.
		 * @remarks No-op since 1.70. The camera object IS the VmCamera scene actor, created by CommonApi (NewCamera)
		 * and configured/connected by MakeCameraRes -- NOT by this method. The impl (void)-discards both parameters;
		 * this stub is kept only so pre-1.70 callers keep compiling.
		 * @sa vmobjects::VmObject, fncontainer::VmCamera
		 */
		void AttachCamera(const AaBbMinMax& aabbMm, const EvmStageViewType stage_vtype);
		/*!
		 * @brief returns this iobj's borrowed (non-owning) camera pointer (fncontainer::VmCamera*).
		 * @return the borrowed (non-owning) fncontainer::VmCamera* connected to this iobj (NULL if none).
		 */
		fncontainer::VmCamera* GetCameraObject();
		// (1.70) non-owning setter: camera state is plain fields on VmCamera (no separate lens object); the iobj
		// holds a borrowed VmCamera* so ResizeFrameBuffer can keep updating the camera's SS/projection on resize.
		void SetCameraObject(fncontainer::VmCamera* camera);

		/*!
		 * @brief Returns the vector container holding the frame buffers
		 * @param fb_usage [in] \n EvmFrameBufferUsage \n The frame usage to retrieve
		 * @return Pointer to the vector<FrameBuffer> holding the defined frame buffers
		 */
		std::vector<FrameBuffer>* GetBufferPointerList(const EvmFrameBufferUsage fb_usage);
	};

	/**
	 * @class VmVObjectPrimitive
	 * @brief Class holding the information of a primitive object whose OS-to-WS placement is established through VmVObject.
	 * @remarks
	 * The original OS is split into two: OS and VOS. \n
	 * @li OS : the primitive coordinate space in which PrimitiveData is stored
	 * @li VOS : the coordinate space in which individual objects defined by PrimitiveData are placed before the WS coordinate space \n
	 * The VmVObject's OS becomes the VOS, and the OS/VOS/WS transforms are user-defined \n
	 * @remarks Through the OS-to-VOS object transform, individual objects defined by PrimitiveData can be placed and deformed in WS in various ways (affine transforms).
	 * @sa vmobjects::VmVObject, fncontainer::VmCamera
	 */
	__vmstaticclass VmVObjectPrimitive : public VmVObject
	{
	public:
		// (1.70) leaf type -> pure-virtual interface; impl (holding the primitive data as direct members) is
		// VmVObjectPrimitive_Detail, hidden in VimCommon.cpp. Construct via NewVObjectPrimitive().
		virtual ~VmVObjectPrimitive() {}

		/*!
		 * @brief Registers the @ref vmobjects::PrimitiveData structure holding the primitive-defined object information into this primitive object
		 * @param prim_data [in] \n PrimitiveData \n Primitive-defined object information
		 * @param progress [out](optional) \n LocalProgress \n
		 * Pointer carrying the function's progress information \n
		 * The default is NULL; if NULL, it is not used.
		 */
		virtual bool RegisterPrimitiveData(const PrimitiveData& prim_data, LocalProgress* progress = NULL) = 0;
		virtual bool RemovePrimitiveData() = 0;
		/*!
		 * @brief Returns the primitive-defined object information stored in the VmVObjectPrimitive.
		 * @return const PrimitiveData* \n Pointer to the PrimitiveData holding the primitive-defined object information
		 * @remarks (1.72, §4.2a) The general return type is now const: an object-owned PrimitiveData
		 * handle cannot be reallocated/freed (PrimitiveData::Delete, ReplaceOrAdd*Definition and
		 * SetIndexBuffer are non-const, so they do not compile on this handle). Buffer CONTENT is still
		 * mutable via GetVerticeDefinition/GetCustomDefinition/GetIndexBuffer. Buffer-destroying changes
		 * must go through the owner-only mutators below (each bumps the incarnation token first).
		 */
		virtual const PrimitiveData* GetPrimitiveData() = 0;

		// (1.72, §4.2a) owner-only destructive mutators. Each bumps the object's incarnation
		// (BumpResMutation) BEFORE the destructive act. Order invariant: change the token FIRST, then free.
		// Frees all vertex/custom/index/texture buffers and clears the primitive definition.
		virtual void DeleteData() = 0;
		// Frees the current index buffer and adopts a new one (num_vidx updated; ownership transferred).
		virtual void ReplaceIndexBuffer(uint32_t* new_index_buffer, const uint32_t num_vidx) = 0;
		// Frees the current index buffer and clears num_vidx.
		virtual void ReleaseIndexBuffer() = 0;
		// Frees the current buffer registered under vtype (if any) and adopts vtx_buffer.
		virtual void ReplaceVertexDefinition(const std::string& vtype, void* vtx_buffer) = 0;
		// Frees the current custom buffer registered under vtype (if any) and adopts buffer.
		virtual void ReplaceCustomDefinition(const std::string& vtype, void* buffer) = 0;

		virtual bool HasKDTree(int* num_updated = NULL) = 0;
		virtual void UpdateKDTree() = 0; // just for point cloud
		virtual uint32_t KDTSearchRadius(const vmfloat3& p_src, const float r_sq, const bool is_sorted, std::vector<std::pair<size_t, float>>& ret_matches) = 0;
		virtual uint32_t KDTSearchKnn(const vmfloat3& p_src, const int k, size_t* out_ids, float* out_dists) = 0;
		
		virtual void UpdateBVHTree(int min_size = -1, int max_size = -1) = 0; // for primitives
		virtual void* GetBVHTree() = 0;
		virtual bool GetBVHTreeBuffers(vmint4** nodePtr, int* nodeSize, vmint4** triWoopPtr, int* triWoopSize,
			vmint4** triDebugPtr, int* triDebugSize, int** cpuTriIndicesPtr, int* triIndicesSize) = 0;

		// VZM2 features
		virtual const void UpdateBVH(const bool GPUBVHEnabled) = 0;
		virtual const geometrics::BVH& GetBVH() const = 0;
	};
	__vmstatic VmVObjectPrimitive* NewVObjectPrimitive(); // (1.70) factory; VmVObjectPrimitive_Detail hidden in VimCommon.cpp
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
		vmobjects::VmMap<std::string, vmobjects::VmObject*> _associated_res;
	protected:
		vmobjects::VmParamMap<std::string, std::any> _vmparams;
		std::string _actorType = "ACTOR";
	public:
		bool visible = true;
		vmfloat4 color = vmfloat4(1.f);
		vmmat44f matOS2WS = vmmat44f();
		vmmat44f matWS2OS = vmmat44f();
		std::string name = "No Name";
		int actorId = 0;
		int sceneId = 0;
		// (rev.14) LAST-CHANGE stamp, common to every actor kind (absorbed VmLight::timeStamp).
		// POLICY: core updates it whenever the actor's state meaningfully changes, and EVERY plugin
		// module must update it when it changes this actor -- it is the intended coarse clue for
		// future process gates ("did anyone touch this?"). It does NOT replace the value-compare
		// gates (__LightParamsEqual, the VXGI D11 deadband): those answer the different, finer
        // question "did the value actually change?" -- a stamp moves even on a same-value re-set.
		// Coarse then fine; the two stack, neither substitutes for the other.
		uint64_t timeStamp = 0ull;

		VmActor* parentActor = NULL;
		std::vector<VmActor*> childActors;

		VmActor() {
			_actorType = "ACTOR";
		}

		std::string GetActorType() { return _actorType; }

		void Reset() {
			_geometry_res = NULL;
			_associated_res.RemoveAll();
			_vmparams.RemoveAll();
			visible = true;
			color = vmfloat4(1.f);
			matOS2WS = vmmat44f();
			matWS2OS = vmmat44f();
		}

		int GetAllResourceObjs(std::vector<vmobjects::VmObject*>* res_objs) {
			std::vector<vmobjects::VmObject*> _res_objs;
			if (_geometry_res) {
				_res_objs.push_back(_geometry_res);
			}
			for (auto it = _associated_res.begin(); it != _associated_res.end(); it++) {
				vmobjects::VmObject* _res = it->second;
				if (_res)
					_res_objs.push_back(_res);
			}
			if (res_objs) *res_objs = _res_objs;
			return (int)_res_objs.size();
		}

		vmobjects::VmVObject* GetGeometryRes() {
			return _geometry_res;
		}

		void SetGeometryRes(vmobjects::VmVObject* geometry_res) {
			_geometry_res = geometry_res;
		}

		vmobjects::VmObject* GetAssociateRes(const std::string& name) {
			return _associated_res.GetParam(name, (vmobjects::VmObject*)NULL);
		}

		void SetAssociateRes(const std::string name, const vmobjects::VmObject* geometry_res) {
			_associated_res.SetParam(name, (vmobjects::VmObject*)geometry_res);
		}

		void RemoveAssociateRes(const std::string name) {
			_associated_res.RemoveParam(name);
		}

		void RemoveResFromID(const int res_obj_id) {
			if (_geometry_res && _geometry_res->GetObjectID() == res_obj_id) _geometry_res = NULL;
			std::vector<std::string> res_names;
			for (auto& it : _associated_res) {
				vmobjects::VmObject* res = std::get<1>(it);
				if (res->GetObjectID() == res_obj_id) res_names.push_back(std::get<0>(it));
			}
			for (auto& it : res_names) {
				_associated_res.RemoveParam(it);
			}
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
			_vmparams.SetParam(param_name, _v);
		}

		template <typename T>
		void SetParamV(const std::string& param_name, const T _v)
		{
			_vmparams.SetParam(param_name, _v);
		}
	};

	// (Multi-Light rev.14 -- user directive #6) A light IS an actor: it carries actorId / name /
	// visible / matOS2WS / parent-child / timeStamp like any other, rides VmFnContainer::sceneActors
	// BY POINTER, and is filtered by per-view hidden_actors + scene-level visible exactly like a
	// geometry actor. Give it a geometry resource and the existing mesh path simply draws it (the
	// debug-view use case that makes actor-hood mandatory).
	//
	// NEVER COPY THIS BY VALUE across the module boundary: it is an identity, and a copy would
	// duplicate actorId / parentActor / childActors / _vmparams. That is why the old value channel
	// "_VmLight_LightSource" (and the VmSceneLight { light_id; light; } pair, whose light_id is now
	// simply actorId) were retired -- the renderer receives VmLight* out of sceneActors and the core
	// only adds "_int_DominantLightId".
	//
	// pos/dir CONTRACT (ML-D9): these are the RESOLVED WORLD-SPACE values for a STATIONARY light --
	// core writes them from the final matOS2WS in UpdateActorMatrix (view-independent, idempotent).
	// For a CAMERA_ATTACHED light (type == AUTO_ATTACH_3DCAM) the renderer overrides them with its own
	// camera, but ONLY when this light is the view's dominant (actorId == _int_DominantLightId);
	// otherwise the light is interpreted as DIRECTIONAL/STATIONARY and the renderer warns (W-L4). The
	// stored type is never rewritten -- the demotion is an interpretation, not a mutation of the param.
	// (Multi-Light rev.18) Single light-type field replacing the base `is_pointlight` + `is_on_camera`
	// bool pair (and the rev.17 `is_spotlight` proposal). Kept BYTE-VALUE-IDENTICAL to the public
	// vzm::LightParameters::LightType in VisMtvApi.h (the two headers do not include each other; __LightParamsToVmLight
	// converts field-wise). AUTO_ATTACH_3DCAM is the default = the old headlight (directional, pose
	// follows the active 3D camera) so an unconfigured scene is unchanged.
	enum class LightType : uint32_t {
		DIRECTIONAL       = 0, // parallel light; STATIONARY pose (actor transform, local -z)
		POINT             = 1, // omni positional; STATIONARY pose (matOS2WS origin)
		SPOT              = 2, // positional cone (spot_inner/outer_deg); STATIONARY pose
		AUTO_ATTACH_3DCAM = 3, // headlight: directional, pos/dir follow the active 3D camera.
		                       // DOMINANT-only -- a non-dominant light is interpreted as DIRECTIONAL
		                       // (STATIONARY) + W-L4; the stored type is never mutated (demotion is
		                       // interpretation only) and takes effect the moment it becomes dominant.
	};

	struct VmLight : VmActor {
		vmfloat3 pos = vmfloat3(0), dir = vmfloat3(0, 0, -1), up = vmfloat3(0, 1, 0);
		// (rev.18) type replaces is_pointlight + is_on_camera. spot angles used only when type == SPOT.
		LightType type = LightType::AUTO_ATTACH_3DCAM;
		float spot_inner_deg = 30.f; // SPOT: full-intensity half-angle (deg); <= spot_outer_deg
		float spot_outer_deg = 45.f; // SPOT: zero-intensity half-angle (deg); inner==outer = hard edge

		// (Multi-Light ML-D10, API v76) light EMISSION color & intensity, consumed by the direct-shading
		// tint (ambient/diffuse/specular alike) AND the VXGI light injection. The defaults (white, 1.0)
		// reproduce the legacy fixed-white shading exactly.
		//
		// NAMED light_color, NOT color, ON PURPOSE (api-stability review, rev.14): VmActor::color already
		// exists and means something else entirely -- the actor's RGBA render/cull color (a light drawn as
		// a debug gizmo uses THAT one, and its alpha drives the a==0 cull). Two members called `color` on
		// one type would resolve by the static type of the pointer you happen to hold, silently and with
		// no compiler diagnostic: VmActor* -> RGBA, VmLight* -> emission. Different names, no trap.
		vmfloat3 light_color = vmfloat3(1.f);
		float intensity = 1.f;

		VmLight() {
			_actorType = "LIGHT";
		}
	};

	// (2026-07-19, user directive) First-class camera scene node, symmetric with VmLight (both : VmActor, ride
	// sceneActors). pose = its VmActor transform (matOS2WS). (1.70) The old VmLens object was DROPPED and its
	// optics/projection/pose + WS<->SS matrices are now plain fields below; `iobj` is its render-target VmIObject,
	// and the iobj holds a borrowed VmCamera* (iobj->GetCameraObject()) pointing back here.
	struct VmCamera : VmActor {
		vmobjects::VmIObject* iobj = nullptr; // render-target iobj; iobj->GetCameraObject() borrows this VmCamera*
		// (1.70) camera optics + pose, absorbed from the dropped VmLens. intrinsics + extrinsics + cached
		// WS<->SS matrices. The CommonApi layer writes these and recomputes the matrices via
		// UpdateCameraTransforms during the scene-tree update (timestamp-gated on VmActor::timeStamp).
		vmdouble3 pos_cam = vmdouble3(0);
		vmdouble3 view_cam = vmdouble3(0, 0, -1.);
		vmdouble3 up_cam  = vmdouble3(0, 1., 0);
		vmdouble3 left_cam = vmdouble3(-1., 0, 0);
		bool   is_perspective = false;
		double fov_y = VM_PI / 4.;
		double aspect_ratio = 1.0;
		double near_p = 0, far_p = 1000.;
		vmdouble2 ip_size = vmdouble2(1., 1.);
		vmint2 pix_size = vmint2(1, 1);
		vmdouble2 fitting_ip_size = vmdouble2(1., 1.);
		double fitting_fov_y = VM_PI / 4.;
		bool   is_ar_mode = false;
		double fx = 0, fy = 0, sc = 0, cx = 0, cy = 0;
		vmmat44  mat_ws2cs, mat_cs2ps, mat_ps2ss;
		vmmat44  mat_cs2ws, mat_ps2cs, mat_ss2ps;
		vmmat44f fmat_ws2cs, fmat_cs2ps, fmat_ps2ss;
		vmmat44f fmat_cs2ws, fmat_ps2cs, fmat_ss2ps;
		uint64_t _matrix_stamp = ~0ull; // last VmActor::timeStamp the matrices were computed for (UpdateCameraTransforms gate)
		// (increment: post-processing payload) per-view render config that used to travel as loose
		// fnParams channels; the CommonApi layer writes these in RenderScene from the camera's
		// script_params, and renderers read them off the render VmCamera instead of fnParams.GetParam.
		uint64_t temporal_render_count = 0; // TAA / frame-paced index (per-view; MAY be reset/overridden on accumulation restart)
		uint64_t global_render_count = 0;   // API render count (renderer_excute_count); NEVER reset; lifetime == this VmCamera
		// (1.70) tonemap presentation params moved to VmActor::_vmparams (Get/SetParam "TONEMAP_*") -- kept off the
		// sizeof-fingerprinted VmCamera layout so tone-curve changes don't perturb VimCommonLayoutSig.
		VmCamera() { _actorType = "CAMERA"; } // trivial: camera state is plain data now (no VmLens to own)
	};

	struct VmFnContainer {
	private:
	public:
		std::string descriptor;
		vmobjects::VmMap<int, VmActor*> sceneActors;
		vmobjects::VmParamMap<std::string, std::any> fnParams;
	};

	// (1.61 size handshake) ABI layout fingerprint of the structs that cross the core<->renderer DLL boundary.
	// __VERSION is a hand-maintained string that can lag a layout change (e.g. VmCamera grew a tonemap/temporal
	// tail within 1.61 without a version bump); computed from sizeof, so a stale core/renderer pair with the SAME
	// __VERSION but a drifted SIZE is caught at load (a size-preserving field reorder/type change is NOT -- add
	// field offsets here if that becomes a risk). Core and each renderer compute it from their OWN header copy.
	inline unsigned int VimCommonLayoutSig() {
		return (unsigned int)( sizeof(VmCamera)
			+ sizeof(VmLight)  * 131u
			+ sizeof(VmActor)  * 131u * 131u
			+ sizeof(VmFnContainer) * 131u * 131u * 131u );
	}
}

// =================================================================================================
// PLUGIN-SIDE HALF OF THE LOAD HANDSHAKE  (core side: VmModuleArbiter::RegisterModule)
//
// A plugin DLL declares itself compatible by putting VM_DEFINE_MODULE_HANDSHAKE() at file scope once
// and VM_REQUIRE_MODULE_HANDSHAKE(name) at the top of its InitModule. Nothing else.
//
// WHY THIS IS A MACRO AND NOT SIX MODULES' WORTH OF COPY-PASTE: it was copy-paste first, and that is
// the problem. The attestation is only worth anything if EVERY module computes it the same way from
// the same header; six hand-written copies is six chances for one of them to drift into comparing
// something weaker, and a handshake that silently weakens is worse than none because the loader still
// prints success. Here there is one definition, it lives in the very header whose contract is being
// attested, and a module that forgets it does not compile a subtly wrong version -- it exports
// nothing and is REJECTED at load, which is the safe direction to fail in.
//
// The arm is MUTUAL and takes PRIMITIVES ONLY (const char*, unsigned int). That restriction is the
// point: it must be callable before the two sides are known to agree on any struct layout, so no
// struct may cross in it. The core arms the DLL right after loading it; InitModule then refuses to
// run un-armed, so an OLD core that never calls arm leaves a NEW plugin self-disabled rather than
// half-trusted. Both directions are covered by one exchange.
//
// LIFETIME is DLL-LOAD, not per-Init. The arbiter arms once when it loads the library, and a loaded
// image's VimCommon layout cannot change while it is loaded; the flag is deliberately NOT cleared in
// DeInitModule, because a module can be DeInit/re-Init'd without being unloaded and re-armed.
//
// __VERSION is compared as well as the size fingerprint because since 1.73 it also carries BEHAVIOUR
// contracts -- a plugin can be layout-identical and still mishandle a new dispatch key destructively,
// and no sizeof can see that.
#define VM_DEFINE_MODULE_HANDSHAKE()                                                                \
	static bool g_vimHandshakeArmed = false;                                                        \
	__vmstatic bool __ArmVimCommonHandshake(const char* core_version, unsigned int core_sig)         \
	{                                                                                               \
		g_vimHandshakeArmed = (core_version != NULL                                                 \
			&& std::string(core_version) == std::string(__VERSION)                                  \
			&& core_sig == fncontainer::VimCommonLayoutSig());                                      \
		return g_vimHandshakeArmed;                                                                 \
	}

// Put this as the FIRST statement of InitModule. `module_name` is a string literal used only in the
// diagnostic. Refusing here (rather than trusting the loader to have refused already) is what closes
// the old-core direction: an old arbiter does not know to call arm at all.
#define VM_REQUIRE_MODULE_HANDSHAKE(module_name)                                                     \
	do {                                                                                             \
		if (!g_vimHandshakeArmed) {                                                                  \
			vmlog::LogErr(std::string(module_name) + " InitModule refused: VimCommon handshake not"   \
				" armed (stale or absent core -- rebuild every plugin against the same VimCommon.h)."); \
			return false;                                                                            \
		}                                                                                            \
	} while (0)
