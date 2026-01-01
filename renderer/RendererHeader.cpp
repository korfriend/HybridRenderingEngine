#include "RendererHeader.h"

void GradientMagnitudeAnalysis(vmfloat2& grad_minmax, VmVObjectVolume* vobj)
{
	grad_minmax = vobj->GetObjParam("_float2_GraidentMagMinMax", vmfloat2(FLT_MAX, -FLT_MAX));
	if (grad_minmax.x < grad_minmax.y) return;

	VolumeData* vol_data = vobj->GetVolumeData();

	int max_length = max(max(vol_data->vol_size.x, vol_data->vol_size.y), vol_data->vol_size.z);
	int offset = max(max_length / 200, 2);

	uint16_t** ppusVolume = (uint16_t**)vol_data->vol_slices;
	int iSizeAddrX = vol_data->vol_size.x + vol_data->bnd_size.x * 2;
	for (int iZ = 1; iZ < vol_data->vol_size.z - 1; iZ += offset)
	{
		for (int iY = 1; iY < vol_data->vol_size.y - 1; iY += offset)
		{
			for (int iX = 1; iX < vol_data->vol_size.x - 1; iX += offset)
			{
				vmfloat3 f3Difference;
				int iAddrZ = iZ + vol_data->bnd_size.x;
				int iAddrY = iY + vol_data->bnd_size.y;
				int iAddrX = iX + vol_data->bnd_size.z;
				int iAddrZL = iZ - 1 + vol_data->bnd_size.z;
				int iAddrYL = iY - 1 + vol_data->bnd_size.y;
				int iAddrXL = iX - 1 + vol_data->bnd_size.x;
				int iAddrZR = iZ + 1 + vol_data->bnd_size.z;
				int iAddrYR = iY + 1 + vol_data->bnd_size.y;
				int iAddrXR = iX + 1 + vol_data->bnd_size.x;
				f3Difference.x = (float)((int)ppusVolume[iAddrZ][iAddrY * iSizeAddrX + iAddrXR] - (int)ppusVolume[iAddrZ][iAddrY * iSizeAddrX + iAddrXL]);
				f3Difference.y = (float)((int)ppusVolume[iAddrZ][iAddrYR * iSizeAddrX + iAddrX] - (int)ppusVolume[iAddrZ][iAddrYL * iSizeAddrX + iAddrX]);
				f3Difference.z = (float)((int)ppusVolume[iAddrZR][iAddrY * iSizeAddrX + iAddrX] - (int)ppusVolume[iAddrZL][iAddrY * iSizeAddrX + iAddrX]);
				float fGradientMag = sqrt(f3Difference.x * f3Difference.x + f3Difference.y * f3Difference.y + f3Difference.z * f3Difference.z);
				grad_minmax.x = min(grad_minmax.x, fGradientMag);
				grad_minmax.y = max(grad_minmax.y, fGradientMag);
			}
		}
	}
	vobj->SetObjParam("_float2_GraidentMagMinMax", grad_minmax);
}