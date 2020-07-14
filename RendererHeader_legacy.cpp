#include "RendererHeader_legacy.h"

using namespace grd_helper_legacy;

//void RegisterVolumeRes(VmVObjectVolume* vol_obj, VmTObject* tobj, VmLObject* lobj, VmGpuManager* pCGpuManager, //ID3D11DeviceContext* pdx11DeviceImmContext,
//	map<int, VmObject*>& mapAssociatedObjects, map<int, GpuRes>& mapGpuRes, LocalProgress* progress)
//{
//	if (vol_obj)
//	{
//		auto itrVolume = mapAssociatedObjects.find(vol_obj->GetObjectID());
//		if (itrVolume != mapAssociatedObjects.end())
//		{
//			// GPU Resource Check!! : Volume //
//			GpuRes gres_vol;
//			grd_helper_legacy::UpdateVolumeModel(gres_vol, vol_obj, false, progress);
//			mapGpuRes[vol_obj->GetObjectID()] = gres_vol;
//			//mapGpuRes.insert(pair<int, GpuRes>(vol_obj->GetObjectID(), gres_vol));
//		}
//	}
//	if (tobj)
//	{
//		auto itrTObject = mapAssociatedObjects.find(tobj->GetObjectID());
//		if (itrTObject != mapAssociatedObjects.end())
//		{
//			bool bIsTfChanged = false;
//			lobj->GetDstObjValue(tobj->GetObjectID(), ("_bool_IsTfChanged"), &bIsTfChanged);
//
//			// TObject which is not 'preintegrated' one
//			map<int, VmTObject*> tobj_map;
//			tobj_map.insert(pair<int, VmTObject*>(tobj->GetObjectID(), tobj));
//
//			GpuRes gres_tmap;
//			grd_helper_legacy::UpdateTMapBuffer(gres_tmap, tobj, tobj_map, NULL, NULL, 0, bIsTfChanged);
//			mapGpuRes[tobj->GetObjectID()] = gres_tmap;
//			//mapGpuRes.insert(pair<int, GpuRes>(tobj->GetObjectID(), gres_tmap));
//		}
//	}
//}

int RTTandLayersToLayersCS(ID3D11DeviceContext* pdx11DeviceImmContext, uint uiNumGridX, uint uiNumGridY
	, ID3D11ShaderResourceView* pdx11SRV_RenderOuts[NUM_TEXRT_LAYERS], ID3D11ShaderResourceView* pdx11SRV_DepthCSs[NUM_TEXRT_LAYERS], int iCountMerging
	, ID3D11UnorderedAccessView* pUAV_Merge_PingpongCSs[2], ID3D11ShaderResourceView* pSRV_Merge_PingpongCSs[2]
	, GpuDX11CommonParameters* pdx11CommonParams, ID3D11ComputeShader** ppdx11MergeCSs[NUMSHADERS_MERGE_CS]
	, ID3D11ShaderResourceView* pdx11SRV_4NULLs[NUM_TEXRT_LAYERS]
	, ID3D11ShaderResourceView* pdx11SRV_2NULLs[2], ID3D11RenderTargetView* pdx11RTV_2NULLs[2], int iMergeLevel
	)
{
	pdx11DeviceImmContext->Flush();
	if (pdx11RTV_2NULLs != NULL)
		pdx11DeviceImmContext->OMSetRenderTargets(2, pdx11RTV_2NULLs, NULL);

	UINT UAVInitialCounts = 0;
	ID3D11UnorderedAccessView* pdx11UAV_NULL = NULL;
	pdx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, &pdx11UAV_NULL, &UAVInitialCounts);
	pdx11DeviceImmContext->CSSetUnorderedAccessViews(2, 1, &pdx11UAV_NULL, &UAVInitialCounts);

	pdx11DeviceImmContext->CSSetShaderResources(30, NUM_TEXRT_LAYERS, pdx11SRV_RenderOuts);
	pdx11DeviceImmContext->CSSetShaderResources(40, NUM_TEXRT_LAYERS, pdx11SRV_DepthCSs);	// 60 to 40

	int iRSA_Index_Offset_Prev = 0;
	int iRSA_Index_Offset_Out = 1;
	if (iCountMerging % 2 == 0)
	{
		iRSA_Index_Offset_Prev = 1;
		iRSA_Index_Offset_Out = 0;
	}

	// ???? //
	ID3D11ShaderResourceView* pdx11SRV_Layers = pSRV_Merge_PingpongCSs[iRSA_Index_Offset_Prev];
	ID3D11UnorderedAccessView* pdx11UAV_Layers = pUAV_Merge_PingpongCSs[iRSA_Index_Offset_Out];

	pdx11DeviceImmContext->CSSetShaderResources(50, 1, &pdx11SRV_Layers);
	pdx11DeviceImmContext->CSSetUnorderedAccessViews(1, 1, &pdx11UAV_Layers, &UAVInitialCounts);

	switch (iMergeLevel)
	{
	case 0:
		pdx11DeviceImmContext->CSSetShader(*ppdx11MergeCSs[__CS_MERGE_ADV_RSOUT_TO_LAYERS], NULL, 0); break;
	case 1:
		pdx11DeviceImmContext->CSSetShader(*ppdx11MergeCSs[__CS_MERGE_RSOUT_TO_LAYERS], NULL, 0); break;
	default:
		VMERRORMESSAGE("NOT SUPPORTED::iMergeLevel");
		break;
	}

	pdx11DeviceImmContext->Dispatch(uiNumGridX, uiNumGridY, 1);

	// Set NULL States //
	pdx11DeviceImmContext->CSSetShaderResources(30, NUM_TEXRT_LAYERS, pdx11SRV_4NULLs);
	pdx11DeviceImmContext->CSSetShaderResources(40, NUM_TEXRT_LAYERS, pdx11SRV_4NULLs);
	pdx11DeviceImmContext->CSSetShaderResources(50, 2, pdx11SRV_2NULLs);
	pdx11DeviceImmContext->CSSetUnorderedAccessViews(1, 1, &pdx11UAV_NULL, &UAVInitialCounts);

	return iRSA_Index_Offset_Prev;
}

//void MakeFBDescriptions(bool bIsAvailableCS,
//	GpuResDescriptor stGpuResFbDescs[__FB_COUNT_CS],
//	GpuResDescriptor stGpuResDepthStencilDescs[__DS_COUNT],
//	GpuResDescriptor stGpuResExFbLayersDescs[__EXFB_COUNT],
//	GpuResDescriptor stGpuResRwbLayersDescs[__EXRWB_COUNT],
//	GpuResDescriptor stGpuResShadowMapDescs[__SM_COUNT_CS],
//	int iIObjectID)
//{
//	// GPU Resource Check!! : Frame Buffer //
//	if (bIsAvailableCS)
//	{
//		for (int i = 0; i < __FB_COUNT_CS; i++)
//		{
//			stGpuResFbDescs[i].sdk_type = GpuSdkTypeDX11;
//			stGpuResFbDescs[i].misc = NUM_TEXRT_LAYERS | 1 << 16;	// 1 << 16 means possible UAV 
//			stGpuResFbDescs[i].src_obj_id = iIObjectID;
//		}
//	}
//	else
//	{
//		for (int i = 0; i < __FB_COUNT_PS; i++)
//		{
//			stGpuResFbDescs[i].sdk_type = GpuSdkTypeDX11;
//			stGpuResFbDescs[i].misc = NUM_TEXRT_LAYERS;
//			stGpuResFbDescs[i].src_obj_id = iIObjectID;
//		}
//	}
//
//	stGpuResFbDescs[__FB_TX2D_RENDEROUT].res_type = ResourceTypeDX11RESOURCE;
//	stGpuResFbDescs[__FB_TX2D_RENDEROUT].usage_specifier = ("TEXTURE2D_IOBJECT_RENDEROUT");
//	stGpuResFbDescs[__FB_TX2D_RENDEROUT].dtype = data_type::dtype<vmbyte4>();
//	stGpuResFbDescs[__FB_RTV_RENDEROUT] = stGpuResFbDescs[__FB_SRV_RENDEROUT] = stGpuResFbDescs[__FB_TX2D_RENDEROUT];
//	stGpuResFbDescs[__FB_RTV_RENDEROUT].res_type = ResourceTypeDX11RTV;
//	stGpuResFbDescs[__FB_SRV_RENDEROUT].res_type = ResourceTypeDX11SRV;
//	stGpuResFbDescs[__FB_TX2D_DEPTHCS].res_type = ResourceTypeDX11RESOURCE;
//	stGpuResFbDescs[__FB_TX2D_DEPTHCS].usage_specifier = ("TEXTURE2D_IOBJECT_RENDEROUT");
//	stGpuResFbDescs[__FB_TX2D_DEPTHCS].dtype = data_type::dtype<float>();
//	stGpuResFbDescs[__FB_RTV_DEPTHCS] = stGpuResFbDescs[__FB_SRV_DEPTHCS] = stGpuResFbDescs[__FB_TX2D_DEPTHCS];
//	stGpuResFbDescs[__FB_RTV_DEPTHCS].res_type = ResourceTypeDX11RTV;
//	stGpuResFbDescs[__FB_SRV_DEPTHCS].res_type = ResourceTypeDX11SRV;
//
//	if (bIsAvailableCS)
//	{
//		stGpuResFbDescs[__FB_UAV_RENDEROUT] = stGpuResFbDescs[__FB_SRV_RENDEROUT];
//		stGpuResFbDescs[__FB_UAV_RENDEROUT].res_type = ResourceTypeDX11UAV;
//		stGpuResFbDescs[__FB_UAV_DEPTHCS] = stGpuResFbDescs[__FB_SRV_DEPTHCS];
//		stGpuResFbDescs[__FB_UAV_DEPTHCS].res_type = ResourceTypeDX11UAV;
//	}
//
//	if (stGpuResDepthStencilDescs != NULL)
//	{
//		stGpuResDepthStencilDescs[__DS_TX2D_DEPTH] = stGpuResFbDescs[__FB_TX2D_DEPTHCS];
//		stGpuResDepthStencilDescs[__DS_TX2D_DEPTH].res_type = ResourceTypeDX11RESOURCE;
//		stGpuResDepthStencilDescs[__DS_TX2D_DEPTH].usage_specifier = ("TEXTURE2D_IOBJECT_DEPTHSTENCIL");
//		stGpuResDepthStencilDescs[__DS_TX2D_DEPTH].dtype = data_type::dtype<float>();
//		stGpuResDepthStencilDescs[__DS_TX2D_DEPTH].misc = 0;
//		stGpuResDepthStencilDescs[__DS_DSV_DEPTH] = stGpuResDepthStencilDescs[__DS_TX2D_DEPTH];
//		stGpuResDepthStencilDescs[__DS_DSV_DEPTH].res_type = ResourceTypeDX11DSV;
//	}
//
//	// To Store and Merge Output //
//	if (bIsAvailableCS)
//	{
//		stGpuResRwbLayersDescs[__EXRWB_UBUF_LAYERS].sdk_type = GpuSdkTypeDX11;
//		stGpuResRwbLayersDescs[__EXRWB_UBUF_LAYERS].usage_specifier = ("BUFFER_IOBJECT_RESULTOUT");
//		stGpuResRwbLayersDescs[__EXRWB_UBUF_LAYERS].misc = __BLOCKSIZE | (2 << 16);
//		stGpuResRwbLayersDescs[__EXRWB_UBUF_LAYERS].src_obj_id = iIObjectID;
//		stGpuResRwbLayersDescs[__EXRWB_UBUF_LAYERS].dtype = data_type::dtype<RWB_Output_Layers>();
//		stGpuResRwbLayersDescs[__EXRWB_UBUF_LAYERS].res_type = ResourceTypeDX11RESOURCE;
//
//		stGpuResRwbLayersDescs[__EXRWB_UAV_LAYERS] = stGpuResRwbLayersDescs[__EXRWB_SRV_LAYERS]
//			= stGpuResRwbLayersDescs[__EXRWB_UBUF_LAYERS];
//		stGpuResRwbLayersDescs[__EXRWB_SRV_LAYERS].res_type = ResourceTypeDX11SRV;
//		stGpuResRwbLayersDescs[__EXRWB_UAV_LAYERS].res_type = ResourceTypeDX11UAV;
//	}
//	else
//	{
//		// To Store and Merge Output //
//		stGpuResExFbLayersDescs[__EXFB_TX2D_RGBA_LAYERS].sdk_type = GpuSdkTypeDX11;
//		stGpuResExFbLayersDescs[__EXFB_TX2D_RGBA_LAYERS].usage_specifier = ("TEXTURE2D_IOBJECT_RENDEROUT");
//		stGpuResExFbLayersDescs[__EXFB_TX2D_RGBA_LAYERS].misc = 2;
//		stGpuResExFbLayersDescs[__EXFB_TX2D_RGBA_LAYERS].src_obj_id = iIObjectID;
//		stGpuResExFbLayersDescs[__EXFB_TX2D_RGBA_LAYERS].dtype = data_type::dtype<vmint4>();
//		stGpuResExFbLayersDescs[__EXFB_TX2D_RGBA_LAYERS].res_type = ResourceTypeDX11RESOURCE;
//
//		stGpuResExFbLayersDescs[__EXFB_RTV_DEPTHCS_LAYERS]
//			= stGpuResExFbLayersDescs[__EXFB_SRV_DEPTHCS_LAYERS]
//			= stGpuResExFbLayersDescs[__EXFB_TX2D_DEPTHCS_LAYERS]
//			= stGpuResExFbLayersDescs[__EXFB_RTV_RGBA_LAYERS]
//			= stGpuResExFbLayersDescs[__EXFB_SRV_RGBA_LAYERS]
//			= stGpuResExFbLayersDescs[__EXFB_TX2D_RGBA_LAYERS];
//
//		stGpuResExFbLayersDescs[__EXFB_SRV_RGBA_LAYERS].res_type = ResourceTypeDX11SRV;
//		stGpuResExFbLayersDescs[__EXFB_RTV_RGBA_LAYERS].res_type = ResourceTypeDX11RTV;
//
//		stGpuResExFbLayersDescs[__EXFB_TX2D_DEPTHCS_LAYERS].dtype
//			= stGpuResExFbLayersDescs[__EXFB_SRV_DEPTHCS_LAYERS].dtype
//			= stGpuResExFbLayersDescs[__EXFB_RTV_DEPTHCS_LAYERS].dtype
//			= data_type::dtype<vmfloat4>();
//		stGpuResExFbLayersDescs[__EXFB_SRV_DEPTHCS_LAYERS].res_type = ResourceTypeDX11SRV;
//		stGpuResExFbLayersDescs[__EXFB_RTV_DEPTHCS_LAYERS].res_type = ResourceTypeDX11RTV;
//	}
//
//	if (stGpuResShadowMapDescs != NULL)
//	{
//		stGpuResShadowMapDescs[__SM_TX2D_DEPTH_SHADOWMAP] = stGpuResFbDescs[__FB_TX2D_DEPTHCS];
//		stGpuResShadowMapDescs[__SM_TX2D_DEPTH_SHADOWMAP].misc = bIsAvailableCS ? 1 << 16 : 0;
//		stGpuResShadowMapDescs[__SM_RTV_DEPTH_SHADOWMAP] =
//			stGpuResShadowMapDescs[__SM_SRV_DEPTH_SHADOWMAP] =
//			stGpuResShadowMapDescs[__SM_TX2D_DEPTH_SHADOWMAP];
//		stGpuResShadowMapDescs[__SM_SRV_DEPTH_SHADOWMAP].res_type = ResourceTypeDX11SRV;
//		stGpuResShadowMapDescs[__SM_RTV_DEPTH_SHADOWMAP].res_type = ResourceTypeDX11RTV;
//
//		if (bIsAvailableCS)
//		{
//			stGpuResShadowMapDescs[__SM_UAV_DEPTH_SHADOWMAP] = stGpuResShadowMapDescs[__SM_TX2D_DEPTH_SHADOWMAP];
//			stGpuResShadowMapDescs[__SM_UAV_DEPTH_SHADOWMAP].res_type = ResourceTypeDX11UAV;
//		}
//	}
//}