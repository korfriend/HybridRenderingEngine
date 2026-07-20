#include "RendererHeader.h"

// (v76) ComputeSSAO REMOVED -- SSAO feature retired (user directive): the KB_SSAO(_BLUR)(_FM) dispatch
// pipeline, its AO frame buffers (RW_TEXS_AO*) and every consumer bind are gone with it.

void ComputeDOF(__ID3D11DeviceContext* dx11DeviceImmContext,
	grd_helper::PSOManager* psoManager, VmIObject* iobj,
	int num_grid_x, int num_grid_y,
	GpuRes& gres_fb_counter, GpuRes& gres_fb_deep_k_buffer, GpuRes& gres_fb_rgba,
	bool apply_SSAO, bool is_blurred_SSAO, bool apply_fragmerge,
	GpuRes& gres_fb_vr_depth, GpuRes& gres_fb_vr_ao, GpuRes& gres_fb_vr_ao_blf,
	CB_CameraState& cbCamState, ID3D11Buffer* cbuf_cam_state, int __BLOCKSIZE,
	bool involve_vr)
{
	fncontainer::VmCamera* cam_obj = iobj->GetCameraObject();
	vmmat44 dmatWS2CS, dmatCS2PS, dmatPS2SS;
	vmmat44 dmatSS2PS, dmatPS2CS, dmatCS2WS;
	dmatWS2CS = cam_obj->mat_ws2cs; dmatCS2PS = cam_obj->mat_cs2ps; dmatPS2SS = cam_obj->mat_ps2ss;
	dmatSS2PS = cam_obj->mat_ss2ps; dmatPS2CS = cam_obj->mat_ps2cs; dmatCS2WS = cam_obj->mat_cs2ws;
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
	uint32_t clr_unit4[4] = { 0, 0, 0, 0 };
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
	uint32_t texMm_num_grid_x = __BLOCKSIZE == 1 ? half_w : (uint32_t)ceil(half_w / (float)__BLOCKSIZE);
	uint32_t texMm_num_grid_y = __BLOCKSIZE == 1 ? half_h : (uint32_t)ceil(half_h / (float)__BLOCKSIZE);
	dx11DeviceImmContext->CSSetShader(apply_fragmerge ? GETCS(KB_MINMAXTEXTURE_FM_cs_5_0) : GETCS(KB_MINMAXTEXTURE_cs_5_0), NULL, 0);
	dx11DeviceImmContext->Dispatch(texMm_num_grid_x, texMm_num_grid_y, 1);
	
	psoManager->GpuProfile("SSAO: MinMax Z (half)");
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
	psoManager->GpuProfile("SSAO: MinMax Z (half)", true);

	dx11DeviceImmContext->CSSetUnorderedAccessViews(15, 2, dx11UAVs_NULL, 0);
	dx11SRVs_DOF[0] = (ID3D11ShaderResourceView*)gres_fb_globalminmax.alloc_res_ptrs[DTYPE_SRV];
	dx11SRVs_DOF[1] = (ID3D11ShaderResourceView*)gres_fb_z_minmax_mipmap_nbtex.alloc_res_ptrs[DTYPE_SRV];

	// (v76) the apply_SSAO branch (AO texture-stack bind into dx11SRVs_DOF[2] / t17) lived here -- SSAO
	// retired (user directive); the only call site now passes apply_SSAO=false, and SSDOF.hlsl no longer
	// declares t17. dx11SRVs_DOF[2] stays NULL.
	dx11DeviceImmContext->CSSetShaderResources(15, 3, dx11SRVs_DOF);

	dx11DeviceImmContext->CSSetShader(apply_fragmerge ? GETCS(KB_SSDOF_RT_FM_cs_5_0) : GETCS(KB_SSDOF_RT_cs_5_0), NULL, 0);
	dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);

	dx11DeviceImmContext->CSSetShaderResources(10, 3, dx11SRVs_NULL);
	dx11DeviceImmContext->CSSetUnorderedAccessViews(10, 1, dx11UAVs_NULL, 0);

	dx11DeviceImmContext->CSSetShaderResources(15, 3, dx11SRVs_NULL);
	dx11DeviceImmContext->CSSetUnorderedAccessViews(15, 2, dx11UAVs_NULL, 0);
}