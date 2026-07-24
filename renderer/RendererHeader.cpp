#include "RendererHeader.h"

void GradientMagnitudeAnalysis(vmfloat2& grad_minmax, VmVObjectVolume* vobj)
{
	grad_minmax = vobj->GetObjParam("_float2_GraidentMagMinMax", vmfloat2(FLT_MAX, -FLT_MAX));
	if (grad_minmax.x < grad_minmax.y) return;

	const VolumeData* vol_data = vobj->GetVolumeData();

	int max_length = max(max(vol_data->vol_size.x, vol_data->vol_size.y), vol_data->vol_size.z);
	int offset = max(max_length / 200, 2);

	uint16_t** ppusVolume = (uint16_t**)vol_data->GetVolSlices();
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

void BuildXrayPostFilterKernel(int mode, float strength, int radius,
	int N, int kcount, int center, int use_filter, float weights[121])
{
	for (int t = 0; t < 121; t++) weights[t] = 0.f;

	// helper: fill weights with a normalized 2D gaussian (sum == 1); sigma = scale*radius.
	auto build_gaussian = [&](float sigma_scale) {
		float sigma = (sigma_scale > 0.f ? sigma_scale : 0.5f) * (float)radius;
		if (sigma < 1e-4f) sigma = 1e-4f;
		const float two_s2 = 2.f * sigma * sigma;
		float sum = 0.f; int t = 0;
		for (int dy = -radius; dy <= radius; dy++)
			for (int dx = -radius; dx <= radius; dx++) {
				float wv = expf(-(float)(dx * dx + dy * dy) / two_s2);
				weights[t++] = wv; sum += wv;
			}
		if (sum > 0.f) for (int q = 0; q < kcount; q++) weights[q] /= sum;
	};
	// helper: write the 4-neighbour laplacian stencil (center, up/down/left/right), rest zero.
	auto build_laplacian = [&](float c, float nb) {
		weights[center] = c;
		if (radius >= 1) {
			weights[(radius - 1) * N + radius] = nb; // up
			weights[(radius + 1) * N + radius] = nb; // down
			weights[radius * N + (radius - 1)] = nb; // left
			weights[radius * N + (radius + 1)] = nb; // right
		}
	};

	switch (use_filter ? mode : __XRPF_NONE)
	{
	case __XRPF_MEAN: {
		// uniform box blur, sum == 1.
		const float w = 1.f / (float)kcount;
		for (int t = 0; t < kcount; t++) weights[t] = w;
		break; }
	case __XRPF_GAUSSIAN:
		// normalized 2D gaussian; strength scales sigma (wider = more blur).
		build_gaussian(strength);
		break;
	case __XRPF_SHARPEN: {
		// unsharp via box : center = 1 + strength*(1 - 1/kcount), off-center = -strength/kcount. Sum == 1.
		const float inv = 1.f / (float)kcount;
		for (int t = 0; t < kcount; t++) weights[t] = -strength * inv;
		weights[center] = 1.f + strength * (1.f - inv);
		break; }
	case __XRPF_SHARPEN_GAUSSIAN:
		// unsharp via gaussian low-pass : (1+strength)*identity - strength*gaussian. Sum == 1.
		build_gaussian(1.f);
		for (int t = 0; t < kcount; t++) weights[t] *= -strength;
		weights[center] += 1.f + strength;
		break;
	case __XRPF_LAPLACIAN:
		// laplacian high-boost : identity + strength*L (L = 4-neighbour, sums to 0). Sum == 1.
		build_laplacian(1.f + 4.f * strength, -strength);
		break;
	case __XRPF_EDGE:
		// pure laplacian high-pass (edge map). Sum == 0 (not brightness-preserving).
		build_laplacian(4.f * strength, -strength);
		break;
	default: // __XRPF_NONE / radius==0 : passthrough (mask stays zero; use_filter == 0)
		break;
	}
}