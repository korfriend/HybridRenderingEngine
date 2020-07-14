#pragma once
#include "CommonUnits/VimCommon.h"

inline vmint3 __MultInt3(const vmint3* pi3_0, const vmint3* pi3_1)
{
	return vmint3(pi3_0->x * pi3_1->x, pi3_0->y * pi3_1->y, pi3_0->z * pi3_1->z);
}

template <typename T>
inline T __ReadVoxel(const vmint3& idx, const int width_slice, const int exb, T** vol_slices)
{
	return vol_slices[idx.z + exb][idx.x + exb + (idx.y + exb) * width_slice];
};

inline bool __SafeCheck(const vmint3& idx, const vmint3& vol_size)
{
	vmint3 max_dir = idx - (vol_size - vmint3(1, 1, 1));
	vmint3 mult_dot = __MultInt3(&max_dir, &idx);
	return !(mult_dot.x > 0 || mult_dot.y > 0 || mult_dot.z > 0);
}

template <typename T>
inline T __Safe__ReadVoxel(const vmint3& idx, const vmint3& vol_size, const int width_slice, const int exb, T** vol_slices, const T bnd_v = 0)
{
	return __SafeCheck(idx, vol_size) ? __ReadVoxel<T>(idx, width_slice, exb, vol_slices) : (T)bnd_v;
};

inline float __TrilinearInterpolation(float v_0, float v_1, float v_2, float v_3, float v_4, float v_5, float v_6, float v_7,
	const vmfloat3& ratio)
{
	float v01 = v_0 * (1.f - ratio.x) + v_1 * ratio.x;
	float v23 = v_2 * (1.f - ratio.x) + v_3 * ratio.x;
	float v0123 = v01 * (1.f - ratio.y) + v23 * ratio.y;
	float v45 = v_4 * (1.f - ratio.x) + v_5 * ratio.x;
	float v67 = v_6 * (1.f - ratio.x) + v_7 * ratio.x;
	float v4567 = v45 * (1.f - ratio.y) + v67 * ratio.y;
	return v0123 * (1.f - ratio.z) + v4567 * ratio.z;
}

template <typename T>
inline float __Safe_TrilinearSample(const vmfloat3& pos_sample, const vmint3& vol_size, const int width_slice, const int exb, T** vol_slices, const T bnd_v = 0)
{
	vmfloat3 __pos_sample = pos_sample + vmfloat3(1000.f, 1000.f, 1000.f); // SAFE //
	vmint3 idx_sample = vmint3((int)__pos_sample.x, (int)__pos_sample.y, (int)__pos_sample.z);
	vmfloat3 ratio = vmfloat3((__pos_sample.x) - (float)(idx_sample.x), (__pos_sample.y) - (float)(idx_sample.y), (__pos_sample.z) - (float)(idx_sample.z));
	idx_sample -= vmint3(1000, 1000, 1000);

	float v0, v1, v2, v3, v4, v5, v6, v7;
	v0 = (float)__Safe__ReadVoxel<T>(idx_sample + vmint3(0, 0, 0), vol_size, width_slice, exb, vol_slices, bnd_v);
	v1 = (float)__Safe__ReadVoxel<T>(idx_sample + vmint3(1, 0, 0), vol_size, width_slice, exb, vol_slices, bnd_v);
	v2 = (float)__Safe__ReadVoxel<T>(idx_sample + vmint3(0, 1, 0), vol_size, width_slice, exb, vol_slices, bnd_v);
	v3 = (float)__Safe__ReadVoxel<T>(idx_sample + vmint3(1, 1, 0), vol_size, width_slice, exb, vol_slices, bnd_v);
	v4 = (float)__Safe__ReadVoxel<T>(idx_sample + vmint3(0, 0, 1), vol_size, width_slice, exb, vol_slices, bnd_v);
	v5 = (float)__Safe__ReadVoxel<T>(idx_sample + vmint3(1, 0, 1), vol_size, width_slice, exb, vol_slices, bnd_v);
	v6 = (float)__Safe__ReadVoxel<T>(idx_sample + vmint3(0, 1, 1), vol_size, width_slice, exb, vol_slices, bnd_v);
	v7 = (float)__Safe__ReadVoxel<T>(idx_sample + vmint3(1, 1, 1), vol_size, width_slice, exb, vol_slices, bnd_v);
	return __TrilinearInterpolation(v0, v1, v2, v3, v4, v5, v6, v7, ratio);
};

template <typename T>
inline vmfloat3 __Safe_Gradient_by_Samples(const vmfloat3& pos_sample, const vmint3& vol_size, const vmfloat3& dir_x, const vmfloat3& dir_y, const vmfloat3& dir_z, const int width_slice, const int exb, T** vol_slices)
{
	float v_XL = __Safe_TrilinearSample<T>(pos_sample - dir_x, vol_size, width_slice, exb, vol_slices);
	float v_XR = __Safe_TrilinearSample<T>(pos_sample + dir_x, vol_size, width_slice, exb, vol_slices);
	float v_YL = __Safe_TrilinearSample<T>(pos_sample - dir_y, vol_size, width_slice, exb, vol_slices);
	float v_YR = __Safe_TrilinearSample<T>(pos_sample + dir_y, vol_size, width_slice, exb, vol_slices);
	float v_ZL = __Safe_TrilinearSample<T>(pos_sample - dir_z, vol_size, width_slice, exb, vol_slices);
	float v_ZR = __Safe_TrilinearSample<T>(pos_sample + dir_z, vol_size, width_slice, exb, vol_slices);
	float g_x = v_XR - v_XL;
	float g_y = v_YR - v_YL;
	float g_z = v_ZR - v_ZL;
	return vmfloat3(g_x, g_y, g_z) * 0.5f;
}