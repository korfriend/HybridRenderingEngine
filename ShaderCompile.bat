fxc /E CommonVS_P /T vs_5_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs/SR_OIT_P_vs_5_0
fxc /E CommonVS_PN /T vs_5_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs/SR_OIT_PN_vs_5_0 
fxc /E CommonVS_PT /T vs_5_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs/SR_OIT_PT_vs_5_0
fxc /E CommonVS_PNT /T vs_5_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs/SR_OIT_PNT_vs_5_0 
fxc /E CommonVS_PTTT /T vs_5_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs/SR_OIT_PTTT_vs_5_0 

fxc /E CastDepthMap /T ps_5_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs/SR_CAST_DEPTHMAP_ps_5_0

fxc /E BasicShader4 /T ps_5_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs/SR_BASIC_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 
fxc /E BasicShader4 /T ps_5_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs/SR_BASIC_DASHEDLINE_ps_5_0 /D __RENDERING_MODE=1 
fxc /E BasicShader4 /T ps_5_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs/SR_BASIC_MULTITEXTMAPPING_ps_5_0 /D __RENDERING_MODE=2 
fxc /E BasicShader4 /T ps_5_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs/SR_BASIC_TEXTMAPPING_ps_5_0 /D __RENDERING_MODE=3 
fxc /E BasicShader4 /T ps_5_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs/SR_BASIC_TEXTUREIMGMAP_ps_5_0 /D __RENDERING_MODE=4 
fxc /E BasicShader4 /T ps_5_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs/SR_BASIC_VOLUMEMAP_ps_5_0 /D __RENDERING_MODE=5 
fxc /E BasicShader4 /T ps_5_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs/SR_BASIC_VOLUME_DIST_MAP_ps_5_0 /D __RENDERING_MODE=6 
fxc /E UndercutShader /T ps_5_0 ./hlsl/RayProcessing.hlsl /Fo ./shader_compiled_objs/SR_UNDERCUT_ps_5_0 /D __RENDERING_MODE=0 /D PATHTR_USE_KBUF=1

fxc /E BasicShader4 /T ps_5_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs/SR_QUAD_OUTLINE_ps_5_0 /D __RENDERING_MODE=100 

fxc /E WRITE_DEPTHZ /T ps_5_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs/WRITE_DEPTH_ps_5_0

fxc /E SINGLE_LAYER /T ps_5_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs/SR_SINGLE_LAYER_ps_5_0

fxc /E OIT_RESOLVE /T cs_5_0 ./hlsl/kbuf/KbufResolve.hlsl /Fo ./shader_compiled_objs/OIT_SKBZ_RESOLVE_cs_5_0 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 

fxc /E SortAndRenderCS /T cs_5_0 ./hlsl/dx_abuf/DxABuf_Sort.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_SORT2SENDER_cs_5_0 /D DX_11_STYLE=0 /D FRAG_MERGING=0
fxc /E SortAndRenderCS /T cs_5_0 ./hlsl/dx_abuf/DxABuf_Sort.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_SORT2SENDER_SFM_cs_5_0 /D DX_11_STYLE=0 /D FRAG_MERGING=1 /D LINEAR_MODE=1
fxc /E OIT_A_BUFFER_CNF_FRAGS /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_FRAGCOUNTER_ps_5_0 /D DX_11_STYLE=0 
fxc /E OIT_A_BUFFER_CNF_FRAGS /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_FRAGCOUNTER_MTT_ps_5_0 /D DX_11_STYLE=0 /D __RENDERING_MODE=2
fxc /E OIT_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 /D DX_11_STYLE=0
fxc /E OIT_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_DASHEDLINE_ps_5_0 /D __RENDERING_MODE=1 /D DX_11_STYLE=0
fxc /E OIT_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_MULTITEXTMAPPING_ps_5_0 /D __RENDERING_MODE=2 /D DX_11_STYLE=0
fxc /E OIT_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_TEXTMAPPING_ps_5_0 /D __RENDERING_MODE=3 /D DX_11_STYLE=0
fxc /E OIT_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_TEXTUREIMGMAP_ps_5_0 /D __RENDERING_MODE=4 /D DX_11_STYLE=0
fxc /E OIT_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_VOLUMEMAP_ps_5_0 /D __RENDERING_MODE=5 /D DX_11_STYLE=0
fxc /E OIT_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_VOLUME_DIST_MAP_ps_5_0 /D __RENDERING_MODE=6 /D DX_11_STYLE=0
fxc /E OIT_A_BUFFER_FILL_UNDERCUT /T ps_5_0 ./hlsl/RayProcessing.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_UNDERCUT_ps_5_0 /D __RENDERING_MODE=0 /D DX_11_STYLE=0

fxc /E CreatePrefixSum_Pass0_CS /T cs_5_0 ./hlsl/dx_abuf/DxABuf_Sort.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_PREFIX_0_cs_5_0 /D DX_11_STYLE=1
fxc /E CreatePrefixSum_Pass1_CS /T cs_5_0 ./hlsl/dx_abuf/DxABuf_Sort.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_PREFIX_1_cs_5_0 /D DX_11_STYLE=1
fxc /E CreateOffsetTable_CS /T cs_5_0 ./hlsl/dx_abuf/DxABuf_Sort.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_OffsetTable_cs_5_0 /D DX_11_STYLE=0

fxc /E PICKING_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/PICKING_ABUFFER_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 /D DX_11_STYLE=0
fxc /E PICKING_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/PICKING_ABUFFER_DASHEDLINE_ps_5_0 /D __RENDERING_MODE=1 /D DX_11_STYLE=0
fxc /E PICKING_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/PICKING_ABUFFER_MULTITEXTMAPPING_ps_5_0 /D __RENDERING_MODE=2 /D DX_11_STYLE=0
fxc /E PICKING_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/PICKING_ABUFFER_TEXTMAPPING_ps_5_0 /D __RENDERING_MODE=3 /D DX_11_STYLE=0
fxc /E PICKING_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/PICKING_ABUFFER_TEXTUREIMGMAP_ps_5_0 /D __RENDERING_MODE=4 /D DX_11_STYLE=0

fxc /E GS_Surfels /T gs_5_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs/GS_SurfelPoints_gs_5_0
fxc /E GS_ThickPoints_Pixels /T gs_5_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs/GS_ThickPoints_gs_5_0
fxc /E GS_ThickLines /T gs_5_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs/GS_ThickLines_gs_5_0
fxc /E GS_TriNormal /T gs_5_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs/GS_TriNormal_gs_5_0

fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_RAYMAX_cs_5_0 /D RAYMODE=1 /D OTF_MASK=0 /D MAX_LAYERS=8 /D FRAG_MERGING=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_RAYMIN_cs_5_0 /D RAYMODE=2 /D OTF_MASK=0 /D MAX_LAYERS=8 /D FRAG_MERGING=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_RAYSUM_cs_5_0 /D RAYMODE=3 /D OTF_MASK=0 /D MAX_LAYERS=8 /D FRAG_MERGING=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_RAYMAX_SCULPTMASK_cs_5_0 /D RAYMODE=1 /D OTF_MASK=0 /D SCULPT_MASK=1 /D MAX_LAYERS=8 /D FRAG_MERGING=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_RAYMIN_SCULPTMASK_cs_5_0 /D RAYMODE=2 /D OTF_MASK=0 /D SCULPT_MASK=1 /D MAX_LAYERS=8 /D FRAG_MERGING=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_RAYSUM_SCULPTMASK_cs_5_0 /D RAYMODE=3 /D OTF_MASK=0 /D SCULPT_MASK=1 /D MAX_LAYERS=8 /D FRAG_MERGING=1

fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_DEFAULT_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=0 /D FRAG_MERGING=0 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_OPAQUE_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=1 /D FRAG_MERGING=0 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_CONTEXT_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=2 /D FRAG_MERGING=0 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_MULTIOTF_cs_5_0 /D RAYMODE=0 /D OTF_MASK=1 /D VR_MODE=0 /D FRAG_MERGING=0 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_MASKVIS_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=3 /D FRAG_MERGING=0 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1

fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_DEFAULT_FM_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=0 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_OPAQUE_FM_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=1 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_CONTEXT_FM_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=2 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_MULTIOTF_FM_cs_5_0 /D RAYMODE=0 /D OTF_MASK=1 /D VR_MODE=0 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_MULTIOTF_CONTEXT_FM_cs_5_0 /D RAYMODE=0 /D OTF_MASK=1 /D VR_MODE=2 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_MASKVIS_FM_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=3 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_SCULPTMASK_FM_cs_5_0 /D RAYMODE=0 /D SCULPT_MASK=1 /D VR_MODE=0 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_SCULPTMASK_CONTEXT_FM_cs_5_0 /D RAYMODE=0 /D SCULPT_MASK=1 /D VR_MODE=2 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_CINEMATIC_FM_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=1 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_OPAQUE_MULTIOTF_FM_cs_5_0 /D RAYMODE=0 /D OTF_MASK=1 /D VR_MODE=1 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1

fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_DEFAULT_SCULPTBITS_FM_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D SCULPT_BITS=1 /D VR_MODE=0 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_CONTEXT_SCULPTBITS_FM_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D SCULPT_BITS=1 /D VR_MODE=2 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1

fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_SINGLE_DEFAULT_FM_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=0 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1 /D ONLY_SINGLE_LAYER=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_SINGLE_OPAQUE_FM_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=1 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1 /D ONLY_SINGLE_LAYER=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_SINGLE_CONTEXT_FM_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=2 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1 /D ONLY_SINGLE_LAYER=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_SINGLE_MULTIOTF_FM_cs_5_0 /D RAYMODE=0 /D OTF_MASK=1 /D VR_MODE=0 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1 /D ONLY_SINGLE_LAYER=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_SINGLE_MULTIOTF_CONTEXT_FM_cs_5_0 /D RAYMODE=0 /D OTF_MASK=1 /D VR_MODE=2 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1 /D ONLY_SINGLE_LAYER=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_SINGLE_MASKVIS_FM_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=3 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1 /D ONLY_SINGLE_LAYER=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_SINGLE_SCULPTMASK_FM_cs_5_0 /D RAYMODE=0 /D SCULPT_MASK=1 /D VR_MODE=0 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1 /D ONLY_SINGLE_LAYER=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_SINGLE_SCULPTMASK_CONTEXT_FM_cs_5_0 /D RAYMODE=0 /D SCULPT_MASK=1 /D VR_MODE=2 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1 /D ONLY_SINGLE_LAYER=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_SINGLE_CINEMATIC_FM_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=1 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1 /D ONLY_SINGLE_LAYER=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_SINGLE_OPAQUE_MULTIOTF_FM_cs_5_0 /D RAYMODE=0 /D OTF_MASK=1 /D VR_MODE=1 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1 /D ONLY_SINGLE_LAYER=1

fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_SINGLE_DEFAULT_SCULPTBITS_FM_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D SCULPT_BITS=1 /D VR_MODE=0 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1 /D ONLY_SINGLE_LAYER=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_SINGLE_CONTEXT_SCULPTBITS_FM_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D SCULPT_BITS=1 /D VR_MODE=2 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1 /D ONLY_SINGLE_LAYER=1


fxc /E VR_SURFACE /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_SURFACE_cs_5_0 /D OTF_MASK=0
fxc /E FillDither /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/FillDither_cs_5_0 

fxc /E CurvedSlicer /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/PanoVR_RAYMAX_cs_5_0 /D RAYMODE=1 /D OTF_MASK=0 /D MAX_LAYERS=8 /D FRAG_MERGING=1
fxc /E CurvedSlicer /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/PanoVR_RAYMIN_cs_5_0 /D RAYMODE=2 /D OTF_MASK=0 /D MAX_LAYERS=8 /D FRAG_MERGING=1
fxc /E CurvedSlicer /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/PanoVR_RAYSUM_cs_5_0 /D RAYMODE=3 /D OTF_MASK=0 /D MAX_LAYERS=8 /D FRAG_MERGING=1
fxc /E CurvedSlicer /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/PanoVR_DEFAULT_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=0 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E CurvedSlicer /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/PanoVR_MODULATE_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=2 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E CurvedSlicer /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/PanoVR_MULTIOTF_DEFAULT_cs_5_0 /D RAYMODE=0 /D OTF_MASK=1 /D VR_MODE=0 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E CurvedSlicer /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/PanoVR_MULTIOTF_MODULATE_cs_5_0 /D RAYMODE=0 /D OTF_MASK=1 /D VR_MODE=2 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1


fxc /E ThickSlicePathTracer /T cs_5_0 ./hlsl/RayProcessing.hlsl /Fo ./shader_compiled_objs/ThickSlicePathTracer_cs_5_0 /D FRAG_MERGING=1 /D CURVEDPLANE=0 /D PATHTR_USE_KBUF=1 /D USE_ROV=0 /D TAIL_HANDLING=1 /D DO_NOT_USE_DISCARD=1 /D BVH_LEGACY=1
fxc /E ThickSlicePathTracer /T cs_5_0 ./hlsl/RayProcessing.hlsl /Fo ./shader_compiled_objs/CurvedThickSlicePathTracer_cs_5_0 /D FRAG_MERGING=1 /D CURVEDPLANE=1 /D PATHTR_USE_KBUF=1 /D USE_ROV=0 /D TAIL_HANDLING=1 /D DO_NOT_USE_DISCARD=1 /D BVH_LEGACY=1
fxc /E ThickSlicePathTracer /T cs_5_0 ./hlsl/RayProcessing.hlsl /Fo ./shader_compiled_objs/PickingThickSlice_cs_5_0 /D FRAG_MERGING=1 /D CURVEDPLANE=0 /D PICKING=1 /D PATHTR_USE_KBUF=1 /D BVH_LEGACY=1
fxc /E ThickSlicePathTracer /T cs_5_0 ./hlsl/RayProcessing.hlsl /Fo ./shader_compiled_objs/PickingCurvedThickSlice_cs_5_0 /D FRAG_MERGING=1 /D CURVEDPLANE=1 /D PICKING=1 /D PATHTR_USE_KBUF=1 /D BVH_LEGACY=1

fxc /E ThickSlicePathTracer /T cs_5_0 ./hlsl/RayProcessing.hlsl /Fo ./shader_compiled_objs/ThickSlicePathTracer_GPUBVH_cs_5_0 /D FRAG_MERGING=1 /D CURVEDPLANE=0 /D PATHTR_USE_KBUF=1 /D USE_ROV=0 /D TAIL_HANDLING=1 /D DO_NOT_USE_DISCARD=1
fxc /E ThickSlicePathTracer /T cs_5_0 ./hlsl/RayProcessing.hlsl /Fo ./shader_compiled_objs/CurvedThickSlicePathTracer_GPUBVH_cs_5_0 /D FRAG_MERGING=1 /D CURVEDPLANE=1 /D PATHTR_USE_KBUF=1 /D USE_ROV=0 /D TAIL_HANDLING=1 /D DO_NOT_USE_DISCARD=1
fxc /E ThickSlicePathTracer /T cs_5_0 ./hlsl/RayProcessing.hlsl /Fo ./shader_compiled_objs/PickingThickSlice_GPUBVH_cs_5_0 /D FRAG_MERGING=1 /D CURVEDPLANE=0 /D PICKING=1 /D PATHTR_USE_KBUF=1 
fxc /E ThickSlicePathTracer /T cs_5_0 ./hlsl/RayProcessing.hlsl /Fo ./shader_compiled_objs/PickingCurvedThickSlice_GPUBVH_cs_5_0 /D FRAG_MERGING=1 /D CURVEDPLANE=1 /D PICKING=1 /D PATHTR_USE_KBUF=1 

fxc /E Outline2D /T cs_5_0 ./hlsl/RayProcessing.hlsl /Fo ./shader_compiled_objs/SliceOutline_cs_5_0 /D PATHTR_USE_KBUF=1 /D FRAG_MERGING=1 /D DO_NOT_USE_DISCARD=1 /D USE_ROV=0 /D TAIL_HANDLING=1


fxc /E GS_PickingPoint /T gs_4_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs_4_0/GS_PickingBasic_gs_4_0 
fxc /E GS_MeshCutLines /T gs_4_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs_4_0/GS_MeshCutLines_gs_4_0 


fxc /E Blend2ndLayer /T cs_5_0 ./hlsl/SecondLayerBlend.hlsl /Fo ./shader_compiled_objs/CS_Blend2ndLayer_cs_5_0

fxc /E RayMarchingDistanceMap /T cs_5_0 ./hlsl/particle/BlobParticle.hlsl /Fo ./shader_compiled_objs/PCE_BlobRayMarching_cs_5_0
fxc /E KickoffEmitterSystem /T cs_5_0 ./hlsl/particle/Emitter.hlsl /Fo ./shader_compiled_objs/PCE_KickoffEmitterSystem_cs_5_0
fxc /E ParticleEmitter /T cs_5_0 ./hlsl/particle/Emitter.hlsl /Fo ./shader_compiled_objs/PCE_ParticleEmitter_cs_5_0
fxc /E ParticleSimulation /T cs_5_0 ./hlsl/particle/Emitter.hlsl /Fo ./shader_compiled_objs/PCE_ParticleSimulation_cs_5_0
fxc /E ParticleSimulation /T cs_5_0 ./hlsl/particle/Emitter.hlsl /Fo ./shader_compiled_objs/PCE_ParticleSimulationSort_cs_5_0 /D SORTING=1
fxc /E ParticleUpdateFinish /T cs_5_0 ./hlsl/particle/Emitter.hlsl /Fo ./shader_compiled_objs/PCE_ParticleUpdateFinish_cs_5_0

fxc /E CommonVS_IDX /T vs_5_0 ./hlsl/particle/ParticleRenderer.hlsl /Fo ./shader_compiled_objs/SR_OIT_IDX_vs_5_0 
fxc /E ParticleRender /T ps_5_0 ./hlsl/particle/ParticleRenderer.hlsl /Fo ./shader_compiled_objs/PCE_ParticleRenderBasic_ps_5_0

fxc /E SortKickoff /T cs_5_0 ./hlsl/sort/Sort.hlsl /Fo ./shader_compiled_objs/SORT_Kickoff_cs_5_0 
fxc /E Sort /T cs_5_0 ./hlsl/sort/Sort.hlsl /Fo ./shader_compiled_objs/SORT_cs_5_0 /D SORT_INNER=0
fxc /E SortStep /T cs_5_0 ./hlsl/sort/Sort.hlsl /Fo ./shader_compiled_objs/SORT_Step_cs_5_0 
fxc /E SortInner /T cs_5_0 ./hlsl/sort/Sort.hlsl /Fo ./shader_compiled_objs/SORT_Inner_cs_5_0 /D SORT_INNER=1


fxc /E main /T cs_5_0 ./hlsl/BVH/bvh_hierarchyCS.hlsl /Fo ./shader_compiled_objs/BVH_Hierarchy_cs_5_0 
fxc /E main /T cs_5_0 ./hlsl/BVH/bvh_primitivesCS.hlsl /Fo ./shader_compiled_objs/BVH_Primitives_cs_5_0 
fxc /E main /T cs_5_0 ./hlsl/BVH/bvh_propagateaabbCS.hlsl /Fo ./shader_compiled_objs/BVH_Propagateaabb_cs_5_0 

