#include "RendererHeader.h"

void ComputeSSAO(__ID3D11DeviceContext* dx11DeviceImmContext,
	grd_helper::GpuDX11CommonParameters* dx11CommonParams, VmIObject* iobj,
	int num_grid_x, int num_grid_y,
	GpuRes& gres_fb_counter, GpuRes& gres_fb_deep_k_buffer, GpuRes& gres_fb_rgba, bool blur_SSAO,
	GpuRes& gres_fb_vr_depth, GpuRes& gres_fb_vr_ao, GpuRes& gres_fb_vr_ao_blf, bool involve_vr, bool apply_fragmerge)
{

#define MAX_LAYERS_SSAO 8
	dx11CommonParams->GpuProfile("SSAO Sampling");
	GpuRes gres_fb_ao_texs, gres_fb_ao_blf_texs;
	grd_helper::UpdateFrameBuffer(gres_fb_ao_texs, iobj, "RW_TEXS_AO", RTYPE_TEXTURE2D,
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8_UNORM, UPFB_NFPP_TEXTURESTACK, MAX_LAYERS_SSAO);
	grd_helper::UpdateFrameBuffer(gres_fb_ao_blf_texs, iobj, "RW_TEXS_AO_BLF", RTYPE_TEXTURE2D,
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, UPFB_NFPP_TEXTURESTACK, MAX_LAYERS_SSAO);

	ID3D11ShaderResourceView* dx11SRVs_SSAO[5] = {};
	dx11SRVs_SSAO[0] = (ID3D11ShaderResourceView*)gres_fb_counter.alloc_res_ptrs[DTYPE_SRV];
	dx11SRVs_SSAO[1] = (ID3D11ShaderResourceView*)gres_fb_deep_k_buffer.alloc_res_ptrs[DTYPE_SRV];
	dx11SRVs_SSAO[2] = dx11SRVs_SSAO[3] = dx11SRVs_SSAO[4] = NULL;
	dx11DeviceImmContext->CSSetShaderResources(10, 2, dx11SRVs_SSAO);

	ID3D11UnorderedAccessView* dx11UAVs_SSAO[5] = {};
	dx11UAVs_SSAO[2] = dx11UAVs_SSAO[3] = dx11UAVs_SSAO[4] = NULL;
	// KBZ_TO_TEXTURE_cs_5_0
	//dx11UAVs_SSAO[0] = (ID3D11UnorderedAccessView*)gres_fb_mip_z_halftexs[0].alloc_res_ptrs[DTYPE_UAV];
	//dx11UAVs_SSAO[1] = (ID3D11UnorderedAccessView*)gres_fb_mip_z_halftexs[1].alloc_res_ptrs[DTYPE_UAV];
	//dx11DeviceImmContext->CSSetUnorderedAccessViews(15, 2, dx11UAVs_SSAO, 0);
	//dx11UAVs_SSAO[0] = (ID3D11UnorderedAccessView*)gres_fb_mip_a_halftexs[0].alloc_res_ptrs[DTYPE_UAV];
	//dx11UAVs_SSAO[1] = (ID3D11UnorderedAccessView*)gres_fb_mip_a_halftexs[1].alloc_res_ptrs[DTYPE_UAV];
	//dx11DeviceImmContext->CSSetUnorderedAccessViews(20, 2, dx11UAVs_SSAO, 0);
	//dx11UAVs_SSAO[2] = dx11UAVs_SSAO[3] = dx11UAVs_SSAO[4] = NULL;
	//
	//dx11DeviceImmContext->CSSetShader(GETCS(KBZ_TO_TEXTURE_cs_5_0), NULL, 0);
	//dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);
	////dx11DeviceImmContext->Flush();
	//dx11DeviceImmContext->CSSetUnorderedAccessViews(15, 2, &dx11UAVs_SSAO[2], 0);
	//dx11DeviceImmContext->CSSetUnorderedAccessViews(20, 2, &dx11UAVs_SSAO[2], 0);
	//dx11DeviceImmContext->GenerateMips((ID3D11ShaderResourceView*)gres_fb_mip_z_halftexs[0].alloc_res_ptrs[DTYPE_SRV]);
	//dx11DeviceImmContext->GenerateMips((ID3D11ShaderResourceView*)gres_fb_mip_z_halftexs[1].alloc_res_ptrs[DTYPE_SRV]);
	//dx11DeviceImmContext->GenerateMips((ID3D11ShaderResourceView*)gres_fb_mip_a_halftexs[0].alloc_res_ptrs[DTYPE_SRV]);
	//dx11DeviceImmContext->GenerateMips((ID3D11ShaderResourceView*)gres_fb_mip_a_halftexs[1].alloc_res_ptrs[DTYPE_SRV]);

	// KB_SSAO_cs_5_0, KB_SSAO_FM_cs_5_0
	dx11UAVs_SSAO[0] = (ID3D11UnorderedAccessView*)gres_fb_ao_texs.alloc_res_ptrs[DTYPE_UAV];
	dx11DeviceImmContext->CSSetUnorderedAccessViews(25, 1, dx11UAVs_SSAO, 0);

	dx11DeviceImmContext->CSSetUnorderedAccessViews(10, 1, (ID3D11UnorderedAccessView**)&gres_fb_rgba.alloc_res_ptrs[DTYPE_UAV], 0);

	//dx11SRVs_SSAO[0] = (ID3D11ShaderResourceView*)gres_fb_mip_z_halftexs[0].alloc_res_ptrs[DTYPE_SRV];
	//dx11SRVs_SSAO[1] = (ID3D11ShaderResourceView*)gres_fb_mip_z_halftexs[1].alloc_res_ptrs[DTYPE_SRV];
	//dx11DeviceImmContext->CSSetShaderResources(15, 2, dx11SRVs_SSAO);
	//dx11SRVs_SSAO[0] = (ID3D11ShaderResourceView*)gres_fb_mip_a_halftexs[0].alloc_res_ptrs[DTYPE_SRV];
	//dx11SRVs_SSAO[1] = (ID3D11ShaderResourceView*)gres_fb_mip_a_halftexs[1].alloc_res_ptrs[DTYPE_SRV];
	//dx11DeviceImmContext->CSSetShaderResources(20, 2, dx11SRVs_SSAO);

	if (involve_vr)
	{
		dx11DeviceImmContext->CSSetShaderResources(31, 1, (ID3D11ShaderResourceView**)&gres_fb_vr_depth.alloc_res_ptrs[DTYPE_SRV]);
		dx11DeviceImmContext->CSSetUnorderedAccessViews(30, 1, (ID3D11UnorderedAccessView**)&gres_fb_vr_ao.alloc_res_ptrs[DTYPE_UAV], 0);
	}

	dx11DeviceImmContext->CSSetShader(apply_fragmerge ? GETCS(KB_SSAO_FM_cs_5_0) : GETCS(KB_SSAO_cs_5_0), NULL, 0);
	dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);

	dx11CommonParams->GpuProfile("SSAO Sampling", true);
	
	if (blur_SSAO)
	{
		dx11CommonParams->GpuProfile("SSAO Blurring");
		// BLUR process
		//dx11DeviceImmContext->Flush();
		dx11DeviceImmContext->CSSetUnorderedAccessViews(25, 2, &dx11UAVs_SSAO[2], 0);
		if (involve_vr)
		{
			dx11DeviceImmContext->CSSetUnorderedAccessViews(30, 1, &dx11UAVs_SSAO[2], 0);
			dx11DeviceImmContext->CSSetShaderResources(30, 1, (ID3D11ShaderResourceView**)&gres_fb_vr_ao.alloc_res_ptrs[DTYPE_SRV]);
			dx11DeviceImmContext->CSSetUnorderedAccessViews(30, 1, (ID3D11UnorderedAccessView**)&gres_fb_vr_ao_blf.alloc_res_ptrs[DTYPE_UAV], 0);
		}

		dx11SRVs_SSAO[0] = (ID3D11ShaderResourceView*)gres_fb_ao_texs.alloc_res_ptrs[DTYPE_SRV];
		dx11DeviceImmContext->CSSetShaderResources(25, 1, dx11SRVs_SSAO);

		dx11UAVs_SSAO[0] = (ID3D11UnorderedAccessView*)gres_fb_ao_blf_texs.alloc_res_ptrs[DTYPE_UAV];
		dx11DeviceImmContext->CSSetUnorderedAccessViews(25, 1, dx11UAVs_SSAO, 0);

		dx11DeviceImmContext->CSSetShader(apply_fragmerge ? GETCS(KB_SSAO_BLUR_FM_cs_5_0) : GETCS(KB_SSAO_BLUR_cs_5_0), NULL, 0);
		dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);

		dx11CommonParams->GpuProfile("SSAO Blurring", true);
	}

	dx11DeviceImmContext->CSSetShaderResources(10, 2, &dx11SRVs_SSAO[2]);
	dx11DeviceImmContext->CSSetUnorderedAccessViews(10, 1, &dx11UAVs_SSAO[2], 0);

	dx11DeviceImmContext->CSSetShaderResources(15, 2, &dx11SRVs_SSAO[2]);
	dx11DeviceImmContext->CSSetShaderResources(20, 2, &dx11SRVs_SSAO[2]);
	dx11DeviceImmContext->CSSetShaderResources(25, 2, &dx11SRVs_SSAO[2]);
	dx11DeviceImmContext->CSSetShaderResources(30, 2, &dx11SRVs_SSAO[2]);
	dx11DeviceImmContext->CSSetUnorderedAccessViews(15, 2, &dx11UAVs_SSAO[2], 0);
	dx11DeviceImmContext->CSSetUnorderedAccessViews(20, 2, &dx11UAVs_SSAO[2], 0);
	dx11DeviceImmContext->CSSetUnorderedAccessViews(25, 2, &dx11UAVs_SSAO[2], 0);
	dx11DeviceImmContext->CSSetUnorderedAccessViews(30, 1, &dx11UAVs_SSAO[2], 0);
}

void ComputeDOF(__ID3D11DeviceContext* dx11DeviceImmContext,
	grd_helper::GpuDX11CommonParameters* dx11CommonParams, VmIObject* iobj,
	int num_grid_x, int num_grid_y,
	GpuRes& gres_fb_counter, GpuRes& gres_fb_deep_k_buffer, GpuRes& gres_fb_rgba,
	bool apply_SSAO, bool is_blurred_SSAO, bool apply_fragmerge,
	GpuRes& gres_fb_vr_depth, GpuRes& gres_fb_vr_ao, GpuRes& gres_fb_vr_ao_blf,
	CB_CameraState& cbCamState, ID3D11Buffer* cbuf_cam_state, int __BLOCKSIZE,
	bool involve_vr)
{
	VmCObject* cam_obj = iobj->GetCameraObject();
	vmmat44 dmatWS2CS, dmatCS2PS, dmatPS2SS;
	vmmat44 dmatSS2PS, dmatPS2CS, dmatCS2WS;
	cam_obj->GetMatrixWStoSS(&dmatWS2CS, &dmatCS2PS, &dmatPS2SS);
	cam_obj->GetMatrixSStoWS(&dmatSS2PS, &dmatPS2CS, &dmatCS2WS);
	vmmat44 dmatCS2SS = dmatCS2PS * dmatPS2SS;
	vmmat44 dmatSS2CS = dmatSS2PS * dmatPS2CS;

	vmint2 fb_size_cur;
	iobj->GetFrameBufferInfo(&fb_size_cur);
	vmfloat3 p_test = vmfloat3(fb_size_cur.x - 1, 0, 0);
	fTransformPoint(&p_test, &p_test, &((vmmat44f)dmatSS2CS));
	cout << "================== " << p_test.x << ", " << p_test.y << ", " << p_test.z << endl;

	D3D11_MAPPED_SUBRESOURCE mappedResCamState;
	dx11DeviceImmContext->Map(cbuf_cam_state, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResCamState);
	CB_CameraState* cbCamStateData = (CB_CameraState*)mappedResCamState.pData;
	memcpy(cbCamStateData, &cbCamState, sizeof(CB_CameraState));
	cbCamStateData->mat_ws2ss = TRANSPOSE(dmatCS2SS);
	cbCamStateData->mat_ss2ws = TRANSPOSE(dmatSS2CS);
	dx11DeviceImmContext->Unmap(cbuf_cam_state, 0);
	dx11DeviceImmContext->CSSetConstantBuffers(0, 1, &cbuf_cam_state);

#define MAX_LAYERS_DOF 8
	GpuRes gres_fb_globalminmax, gres_fb_z_minmax_mipmap_nbtex;
	grd_helper::UpdateFrameBuffer(gres_fb_globalminmax, iobj, "BUFFER_RW_GLOBAL_MINMAX", RTYPE_BUFFER,
		D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_UINT, UPFB_NFPP_BUFFERSIZE, 2 * MAX_LAYERS_DOF);
	uint clr_unit4[4] = { 0, 0, 0, 0 };
	dx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gres_fb_globalminmax.alloc_res_ptrs[DTYPE_UAV], clr_unit4);

	grd_helper::UpdateFrameBuffer(gres_fb_z_minmax_mipmap_nbtex, iobj, "TEX_ARRAY_Z_MINMAX_MipMap", RTYPE_TEXTURE2D,
		D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32G32_FLOAT,
		UPFB_NFPP_TEXTURESTACK, MAX_LAYERS_DOF); // UPFB_HALF_H | 
	float clr_float_zero_4[4] = { 0, 0, 0, 0 };
	dx11DeviceImmContext->ClearUnorderedAccessViewFloat((ID3D11UnorderedAccessView*)gres_fb_z_minmax_mipmap_nbtex.alloc_res_ptrs[DTYPE_UAV], clr_float_zero_4);

	ID3D11ShaderResourceView* dx11SRVs_DOF[3] = {};
	ID3D11ShaderResourceView* dx11SRVs_NULL[3] = {};
	dx11SRVs_DOF[0] = (ID3D11ShaderResourceView*)gres_fb_counter.alloc_res_ptrs[DTYPE_SRV];
	dx11SRVs_DOF[1] = (ID3D11ShaderResourceView*)gres_fb_deep_k_buffer.alloc_res_ptrs[DTYPE_SRV];
	dx11DeviceImmContext->CSSetShaderResources(10, 2, dx11SRVs_DOF);

	dx11DeviceImmContext->CSSetUnorderedAccessViews(10, 1, (ID3D11UnorderedAccessView**)&gres_fb_rgba.alloc_res_ptrs[DTYPE_UAV], 0);

	ID3D11UnorderedAccessView* dx11UAVs_DOF[2] = {};
	ID3D11UnorderedAccessView* dx11UAVs_NULL[2] = {};

	// KB_SSAO_cs_5_0
	dx11UAVs_DOF[0] = (ID3D11UnorderedAccessView*)gres_fb_globalminmax.alloc_res_ptrs[DTYPE_UAV];
	dx11UAVs_DOF[1] = (ID3D11UnorderedAccessView*)gres_fb_z_minmax_mipmap_nbtex.alloc_res_ptrs[DTYPE_UAV];
	dx11DeviceImmContext->CSSetUnorderedAccessViews(15, 2, dx11UAVs_DOF, 0);

	if (involve_vr)
	{
		dx11DeviceImmContext->CSSetShaderResources(31, 1, (ID3D11ShaderResourceView**)&gres_fb_vr_depth.alloc_res_ptrs[DTYPE_SRV]);
		dx11DeviceImmContext->CSSetUnorderedAccessViews(30, 1, (ID3D11UnorderedAccessView**)&gres_fb_vr_ao.alloc_res_ptrs[DTYPE_UAV], 0);
	}

	int half_w = fb_size_cur.x / 4;
	int half_h = fb_size_cur.y / 4;
	uint texMm_num_grid_x = __BLOCKSIZE == 1 ? half_w : (uint)ceil(half_w / (float)__BLOCKSIZE);
	uint texMm_num_grid_y = __BLOCKSIZE == 1 ? half_h : (uint)ceil(half_h / (float)__BLOCKSIZE);
	dx11DeviceImmContext->CSSetShader(apply_fragmerge ? GETCS(KB_MINMAXTEXTURE_FM_cs_5_0) : GETCS(KB_MINMAXTEXTURE_cs_5_0), NULL, 0);
	dx11DeviceImmContext->Dispatch(texMm_num_grid_x, texMm_num_grid_y, 1);
	
	dx11CommonParams->GpuProfile("SSAO: MinMax Z (half)");
	dx11DeviceImmContext->CSSetShader(apply_fragmerge ? GETCS(KB_MINMAX_NBUF_FM_cs_5_0) : GETCS(KB_MINMAX_NBUF_cs_5_0), NULL, 0);
	int max_wh = max(half_w, half_w);
	int nbuf_step = 1;
	while (max_wh > 1 && nbuf_step <= 10)
	{
		dx11DeviceImmContext->Map(cbuf_cam_state, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResCamState);
		CB_CameraState* cbCamStateData = (CB_CameraState*)mappedResCamState.pData;
		memcpy(cbCamStateData, &cbCamState, sizeof(CB_CameraState));
		cbCamStateData->mat_ws2ss = TRANSPOSE(dmatCS2SS);
		cbCamStateData->mat_ss2ws = TRANSPOSE(dmatSS2CS);
		cbCamStateData->iSrCamDummy__1 = nbuf_step++;
		dx11DeviceImmContext->Unmap(cbuf_cam_state, 0);
		dx11DeviceImmContext->CSSetConstantBuffers(0, 1, &cbuf_cam_state);

		dx11DeviceImmContext->Dispatch(texMm_num_grid_x, texMm_num_grid_y, 1);
		dx11DeviceImmContext->Flush();
		max_wh >>= 1;
	}
	dx11CommonParams->GpuProfile("SSAO: MinMax Z (half)", true);

	dx11DeviceImmContext->CSSetUnorderedAccessViews(15, 2, dx11UAVs_NULL, 0);
	dx11SRVs_DOF[0] = (ID3D11ShaderResourceView*)gres_fb_globalminmax.alloc_res_ptrs[DTYPE_SRV];
	dx11SRVs_DOF[1] = (ID3D11ShaderResourceView*)gres_fb_z_minmax_mipmap_nbtex.alloc_res_ptrs[DTYPE_SRV];

	GpuRes gres_fb_ao_texs, gres_fb_ao_blf_texs;
	if (apply_SSAO)
	{
		grd_helper::UpdateFrameBuffer(gres_fb_ao_texs, iobj, "RW_TEXS_AO", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8_UNORM, UPFB_NFPP_TEXTURESTACK, MAX_LAYERS_SSAO);
		grd_helper::UpdateFrameBuffer(gres_fb_ao_blf_texs, iobj, "RW_TEXS_AO_BLF", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, UPFB_NFPP_TEXTURESTACK, MAX_LAYERS_SSAO);
		dx11SRVs_DOF[2] = is_blurred_SSAO ? (ID3D11ShaderResourceView*)gres_fb_ao_blf_texs.alloc_res_ptrs[DTYPE_SRV] : (ID3D11ShaderResourceView*)gres_fb_ao_texs.alloc_res_ptrs[DTYPE_SRV];
	}
	dx11DeviceImmContext->CSSetShaderResources(15, 3, dx11SRVs_DOF);

	dx11DeviceImmContext->CSSetShader(apply_fragmerge ? GETCS(KB_SSDOF_RT_FM_cs_5_0) : GETCS(KB_SSDOF_RT_cs_5_0), NULL, 0);
	dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);

	dx11DeviceImmContext->CSSetShaderResources(10, 3, dx11SRVs_NULL);
	dx11DeviceImmContext->CSSetUnorderedAccessViews(10, 1, dx11UAVs_NULL, 0);

	dx11DeviceImmContext->CSSetShaderResources(15, 3, dx11SRVs_NULL);
	dx11DeviceImmContext->CSSetUnorderedAccessViews(15, 2, dx11UAVs_NULL, 0);
}