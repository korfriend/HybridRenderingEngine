/// Shared native resource, actor and module-dispatch types.
 
/// Include VimCommon.h and link CommonUnits.lib; all DLL consumers must use matching headers.
 
#pragma once
 // __VERSION identifies the shared layout and behaviour contract; deploy matching core/module builds.
#define __VERSION "1.80" // Shared core/module contract.

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

/// VizMotive Framework

// Build with UNICODE. Math helpers use GLM and the row-vector convention.

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
	/// Start of the progress range, between 0.0 and 100.0
	double start;
	/// Extent of the progress range, between 0.0 and 100.0
	double range;
	/// Pointer to a static parameter inside a module/function where the current progress is recorded
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

	/// Defines the orientation in which the Resource-Space axes x(1,0,0), y(0,1,0), z(0,0,1) are initially placed into Object Space (RHS). \n Pitch is not considered; only direction is defined (i.e. valid for vectors only)
	struct AxisInfoRS2OS {
		/// Defines the placed object's x-axis in Object Space corresponding to the Resource-Space x-axis (1,0,0); unit vector
		vmdouble3 vec_axisx_os;
		/// Defines the placed object's y-axis in Object Space corresponding to the Resource-Space y-axis (0,1,0); unit vector
		vmdouble3 vec_axisy_os;
		/// Whether the XY right-handed cross-product direction is reversed when defining the placed object's z-axis in World Space corresponding to the Object-Space z-axis (0,0,1) If true, it is placed right-handed and the transform holds in affine space; if false, the z-axis is placed left-handed
		bool is_rhs;
		/// Initial RS2OS transform matrix derived from vec_axisx_ws, vec_axisy_ws, and is_rhs
		vmmat44 mat_rs2os;
		/// constructor; performs initialization
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

	/// Data structure holding the detailed information of a volume as defined by the framework
	struct VolumeData {
		/// Data type of the volume array, <typeinfo>
		data_type store_dtype;
		/// Original volume data type before it was stored in memory
		data_type origin_dtype;
		/// Padded voxel slices. Allocated dimensions are vol_size + 2*bnd_size; do not assume tightly packed storage.
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
		/// Minimum (store_Mm_values.x) and maximum (store_Mm_values.y) of the stored volume (ppvVolumeSlices)
		vmdouble2 store_Mm_values;
		/// Minimum (actual_Mm_values.x) and maximum (actual_Mm_values.y) defined before the volume was stored
		vmdouble2 actual_Mm_values;
		/// Array defining the histogram of the volume
		uint64_t* histo_values;
		/// Transform matrix mapping the volume space stored in memory (sample coordinates) to its initial placement in world space
		AxisInfoRS2OS axis_info;
		/// constructor; performs initialization
		VolumeData() {
			vol_size = vox_pitch = bnd_size = vmdouble3(0);
			store_dtype = data_type(typeid(void), 0);
			origin_dtype = data_type(typeid(void), 0);

			store_Mm_values = actual_Mm_values = vmdouble2(DBL_MAX, -DBL_MAX);
			vol_slices = NULL;
			histo_values = NULL;
		}

		/// Returns the histogram array size, uint32_t(store_Mm_values.y - store_Mm_values.x + 1.5)
		uint32_t GetHistogramSize() const { return (uint32_t)((double)__max(store_Mm_values.y - store_Mm_values.x + 1.5, 1.0)); }
		/// Returns the ppvVolumeSlices array size, including the extra boundary
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

	/// Data structure holding the detailed information of a primitive-based object as defined by the framework
	struct PrimitiveData {
	private:
		/// Container map storing the vertex arrays string ==> POSITION, NORMAL, TEXCOORD[n], ... Holds the allocated pointers as values, which are freed in PrimitiveData::Delete.
		std::map<std::string, uint8_t*> defined_vtxbuffers;
		std::map<std::string, uint8_t*> defined_custombuffers;
	public:
		/// Vertex winding order of the object's polygons relative to their normal vectors
		bool is_ccw;	// will be deprecated
		/// Primitive Type
		EvmPrimitiveType ptype;
		/// How the primitive's vertices are arranged; true: strip, false: list
		bool is_stripe;
		/// Whether redundancy among the primitive's vertices and edges has been removed
		bool check_redundancy;
		/// Number of polygons in the primitive-based object
		uint32_t num_prims;
		/// Number of indices that define a single primitive (polygon)
		uint32_t idx_stride;
		/// Size of the index buffer (puiIndexList) used to define polygons by vertex index
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
		/// Number of vertices in the primitive-based object
		uint32_t num_vtx;
		/// Bounding box defined in OS at the PrimitiveData level
		AaBbMinMax aabb_os;
		/// Information about the texture resource <w, h, bytes_stride, res_ptr>
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
		/// Method returning the pointer stored as a value in defined_buffers,
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

		/// Replace a vertex buffer, freeing the previous buffer and adopting the supplied allocation.
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
		/// Returns the number of registered vertex definitions
		int GetNumVertexDefinitions() const
		{
			return (int)defined_vtxbuffers.size();
		}
		/// Returns the number of registered custom-buffer definitions (e.g. FACECOLOR).
		int GetNumCustomDefinitions() const
		{
			return (int)defined_custombuffers.size();
		}
		/// Clear vertex-buffer entries without freeing their allocations.
		void ClearVertexDefinitionContainer()
		{
			defined_vtxbuffers.clear();
		}
		void ClearCustomDefinitionContainer()
		{
			defined_custombuffers.clear();
		}
		/// Computes the AABB min/max from the positions in the POSITION vtx_buffer registered in defined_buffers.
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
		/// Pointer to the OTF array
		void** tmap_buffers;
		/**
		 * @brief Pointer dimensionality of the OTF array
		 * @details num_dim = 1 or 2 or 3
		 */
		int num_dim;
		/// Minimum valid OTF array index for each allocated dimension
		vmint3 valid_min_idx;
		/// Maximum valid OTF array index for each allocated dimension
		vmint3 valid_max_idx;
		/// Size of the OTF array along each dimension
		vmint3 array_lengths;
		/// Bin size over the range of volume values that the OTF metric is based on
		vmdouble3 bin_size;
		/// Data type of the OTF array values
		data_type dtype;
		/// constructor; initializes everything to 0 (NULL or false)
		MapTable() {
			tmap_buffers = NULL;
			num_dim = 0;
			valid_min_idx = valid_max_idx = bin_size = vmdouble3(0);
			array_lengths = vmint3(0);
		}

		// Static Helper Functions //
		/// Static helper function that allocates the OTF array stored in VolumeData
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

	/// Data structure for a block-based volume
	struct VolumeBlocks {
		/// Size of a single block
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
		/// 1D array storing the per-block min/max values
		void* mM_blks;
		/// 1D array of per-block binary tags, defined per object
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

	/// Data structure holding the detailed information of a frame buffer as defined by the framework
	struct FrameBuffer {
		/// Width of the frame buffer
		int w;
		/// Height of the frame buffer
		int h;
		/// Frame buffer defined as an array
		void* fbuffer;
		/// Data type of the frame buffer
		data_type dtype;
		/**
		 * @brief Intended usage of the frame buffer
		 * @details When buffer_usage == FrameBufferUsageRENDEROUT, it must be set to vmbyte4.
		 */
		EvmFrameBufferUsage buffer_usage;
		/// Descriptor of the frame buffer
		std::string descriptor;

#ifdef __WINDOWS
		/// Handle for buffer interoperation through file memory on win32
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

		/// Checks whether the VmObject's contents are defined
		bool IsDefined();
		/// Sets the VmObject's object ID
		void SetObjectID(const int obj_id);
		/*!
		 * @brief Returns the VmObject's object ID
		 * @return int \n Returns the object ID
		 */
		int GetObjectID() const;
		/// Sets the ID of the most closely related VmObject used to define this VmObject
		void SetReferenceObjectID(const int ref_obj_id);
		/// Returns the ID of the most closely related VmObject used to define this VmObject
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
		
		/// Returns the type of the defined VmObject
		EvmObjectType GetObjectType();

		uint64_t GetContentUpdateTime();

		void SetContentUpdateTime();

		// Resource incarnation: birth/mutation/poison identify content lifetime. Compare tokens for equality only; direct payload writes need separate content-change tracking.
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
		/// Static helper function that returns the object type from a VmObject ID
		static EvmObjectType GetObjectTypeFromID(const int obj_id);
		/// Static helper function that checks, from a VmObject ID, whether the object type is a VObject
		static bool IsVObject(const int obj_id);
	};

	struct VObjectArchive;
	/// Base class inheriting from VmObject that holds the spatial information shared by VmVObjectVolume and VmVObjectPrimitive
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
		/// Sets the matrix defining the transform between the VmVObject's Resource Space (RS) and Model Space (MS)
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

	/// Class holding volume information whose OS-to-WS placement is established through VmVObject
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
		/// Registers the vmobjects::VolumeData structure holding the volume information into the VmVObjectVolume Copies volume data by default; ref_obj_id selects pointer sharing. Null blk_size2 omits block generation.
		virtual bool RegisterVolumeData(const VolumeData& vol_data, vmint3 blk_size2[2]/* 0 : Large, 1: Small */, const int ref_obj_id = 0, LocalProgress* progress = NULL) = 0;
		/// Returns the volume information defined in the VmVObjectVolume. Borrowed const view; destructive edits go through the owning object.
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

		// Transfer all volume content from src in O(1), preserving this resource identity and invalidating content tokens.
		bool MoveVolumeContentFrom(VmVObjectVolume* src);

		// Optional //
		/// Updates the VolumeBlock structure holding the per-block min/max values after the volume's internal values change
		virtual bool UpdateVolumeMinMaxBlocks(LocalProgress* progress = NULL, const vmint3 blk_size2[2] = NULL) = 0;
		
		/// Returns the volume's block structure
		virtual VolumeBlocks* GetVolumeBlock(const int level) = 0;	// 0 or 1

		/// Updates the tags of the blocks whose values fall within the min/max range configured in the volume's block structure
		virtual void UpdateTagBlocks(const int tobj_id, const int level, const vmdouble2& targetMm, LocalProgress* progress = NULL) = 0;

		/// Fills the extra-boundary volume region defined in VolumeData with the volume's minimum value
		static bool FillBoundaryWithValue(VolumeData& vol_data, const double v, const bool clamp_z, LocalProgress* progress = NULL);

		/// Builds the histogram for the volume defined in VolumeData
		static bool FillHistogram(VolumeData& vol_data, LocalProgress* progress = NULL);
		static bool FillMinMaxStoreValues(VolumeData& vol_data, LocalProgress* progress = NULL);
		static bool ComputeIntialAlignmentMatrixRS2OS(vmmat44& mat_rs2os, AxisInfoRS2OS& axis_info, const vmdouble3& vox_pitch, const AaBbMinMax& aabbMm_rs);
	};
	__vmstatic VmVObjectVolume* NewVObjectVolume(); // (1.70) factory; VmVObjectVolume_Detail hidden in VimCommon.cpp

	/// Class, included as a single instance in VmIObject, that handles camera-related information
	// (1.70) VmLens was REMOVED: its optics/projection/pose + WS<->SS matrices are now plain fields on
	// fncontainer::VmCamera, and the matrix math lives in the CommonApi layer (UpdateCameraTransforms, run during
	// the scene-tree update). VmIObject holds a borrowed VmCamera* as its "camera object".

	struct IObjectArchive;
	/// VmObject-derived render-target: holds image-plane buffers and references (non-owning) one camera (fncontainer::VmCamera).
	__vmstaticclass VmIObject : public VmObject
	{
	private:
	protected:
		IObjectArchive* ioa_res;

	public:
		/// constructor; requires the resolution that defines the frame buffer mapped to the image plane
		VmIObject(const int w = 0, const int h = 0);
		~VmIObject();

		/// Resizes the defined frame buffers
		void ResizeFrameBuffer(const int w, const int h);
		/// Returns information about the defined frame buffers
		void GetFrameBufferInfo(vmint2* buffer_size/*out*/, int* num_buffers = NULL/*out*/, int* bytes_per_pixel = NULL/*out*/);
		/// Returns the vmobjects::FrameBuffer (including its array) holding the defined frame buffer's information
		FrameBuffer* GetFrameBuffer(const EvmFrameBufferUsage fb_usage, const int buffer_idx);

		/// Adds a single frame buffer
		void InsertFrameBuffer(const data_type& dtype, const EvmFrameBufferUsage fb_usage, const std::string& descriptor);

		/// Replaces a frame buffer
		bool ReplaceFrameBuffer(const EvmFrameBufferUsage fb_usage, const int buffer_idx, const data_type& dtype, const std::string& descriptor);

		/// Deletes a frame buffer
		bool DeleteFrameBuffer(const EvmFrameBufferUsage fb_usage, const int buffer_idx);

		/// Deprecated no-op; camera state belongs to the VmCamera actor.
		void AttachCamera(const AaBbMinMax& aabbMm, const EvmStageViewType stage_vtype);
		/*!
		 * @brief returns this iobj's borrowed (non-owning) camera pointer (fncontainer::VmCamera*).
		 * @return the borrowed (non-owning) fncontainer::VmCamera* connected to this iobj (NULL if none).
		 */
		fncontainer::VmCamera* GetCameraObject();
		// (1.70) non-owning setter: camera state is plain fields on VmCamera (no separate lens object); the iobj
		// holds a borrowed VmCamera* so ResizeFrameBuffer can keep updating the camera's SS/projection on resize.
		void SetCameraObject(fncontainer::VmCamera* camera);

		/// Returns the vector container holding the frame buffers
		std::vector<FrameBuffer>* GetBufferPointerList(const EvmFrameBufferUsage fb_usage);
	};

	/// Class holding the information of a primitive object whose OS-to-WS placement is established through VmVObject.
	__vmstaticclass VmVObjectPrimitive : public VmVObject
	{
	public:
		// (1.70) leaf type -> pure-virtual interface; impl (holding the primitive data as direct members) is
		// VmVObjectPrimitive_Detail, hidden in VimCommon.cpp. Construct via NewVObjectPrimitive().
		virtual ~VmVObjectPrimitive() {}

		/// Registers the vmobjects::PrimitiveData structure holding the primitive-defined object information into this primitive object
		virtual bool RegisterPrimitiveData(const PrimitiveData& prim_data, LocalProgress* progress = NULL) = 0;

		// Transfer primitive content from src in O(1); destination identity stays, content tokens change.
		bool MovePrimitiveContentFrom(VmVObjectPrimitive* src);
		virtual bool RemovePrimitiveData() = 0;
		/// Returns the primitive-defined object information stored in the VmVObjectPrimitive. Borrowed const view; destructive edits go through the owning object.
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
		// Last-change timestamp for all actor kinds; core updates it on successful effective edits.
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

	// Lights are scene actors: they share actor identity, transform, visibility and lifetime.
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

		// Linear emission RGB and intensity for direct shading; separate from display/gizmo color.
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

	// Cross-DLL size fingerprint; __VERSION additionally covers size-preserving contract changes.
	inline unsigned int VimCommonLayoutSig() {
		return (unsigned int)( sizeof(VmCamera)
			+ sizeof(VmLight)  * 131u
			+ sizeof(VmActor)  * 131u * 131u
			+ sizeof(VmFnContainer) * 131u * 131u * 131u );
	}
}

// Declare VM_DEFINE_MODULE_HANDSHAKE(module_abi) once per plugin and require it at InitModule entry.
// The core checks shared version/layout before use; arming lasts for the DLL load lifetime.

// Module-specific ABI is checked separately against the core table. Missing version export is refused.
// __GetModuleAbiVersion has a frozen signature: no arguments, unsigned int result.
#define VM_DEFINE_MODULE_HANDSHAKE(module_abi)                                                                \
	static bool g_vimHandshakeArmed = false;                                                        \
	__vmstatic bool __ArmVimCommonHandshake(const char* core_version, unsigned int core_sig)         \
	{                                                                                               \
		g_vimHandshakeArmed = (core_version != NULL                                                 \
			&& std::string(core_version) == std::string(__VERSION)                                  \
			&& core_sig == fncontainer::VimCommonLayoutSig());                                      \
		return g_vimHandshakeArmed;                                                                 \
	}                                                                               \
	__vmstatic unsigned int __GetModuleAbiVersion() { return (unsigned int)(module_abi); }

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
