fxc /E CommonVS_P /T vs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_P_vs_4_0
fxc /E CommonVS_PN /T vs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_PN_vs_4_0 
fxc /E CommonVS_PT /T vs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_PT_vs_4_0
fxc /E CommonVS_PNT /T vs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_PNT_vs_4_0 
fxc /E CommonVS_PTTT /T vs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_PTTT_vs_4_0 

fxc /E SINGLE_LAYER /T ps_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_SINGLE_LAYER_ps_4_0

fxc /E OIT_RESOLVE /T cs_4_0 ./hlsl/kbuf/KbufResolve.hlsl /Fo ./shader_compiled_objs_4_0/OIT_SKBZ_RESOLVE_cs_4_0 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 
fxc /E OIT_PRESET /T cs_4_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs_4_0/SR_SINGLE_LAYER_TO_SKBTZ_cs_4_0 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D SILHOUETTE_EDGE=1 /D DO_NOT_USE_DISCARD=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0 /D USE_ROV=1

fxc /E OIT_KDEPTH /T ps_4_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_FILL_SKBTZ_PHONGBLINN_ps_4_0 /D __RENDERING_MODE=0 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=0 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_4_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_FILL_SKBTZ_DASHEDLINE_ps_4_0 /D __RENDERING_MODE=1 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=0 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_4_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_FILL_SKBTZ_MULTITEXTMAPPING_ps_4_0 /D __RENDERING_MODE=2 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=0 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_4_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_FILL_SKBTZ_TEXTMAPPING_ps_4_0 /D __RENDERING_MODE=3 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=0 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_4_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_FILL_SKBTZ_TEXTUREIMGMAP_ps_4_0 /D __RENDERING_MODE=4 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=0 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0

fxc /E OIT_RESOLVE /T cs_4_0 ./hlsl/kbuf/KbufResolve.hlsl /Fo ./shader_compiled_objs_4_0/OIT_SKB_RESOLVE_cs_4_0 /D FRAG_MERGING=0 /D DYNAMIC_K_MODE=0
fxc /E OIT_PRESET /T cs_4_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs_4_0/SR_SINGLE_LAYER_TO_SKBT_cs_4_0 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D SILHOUETTE_EDGE=1 /D DO_NOT_USE_DISCARD=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0

fxc /E OIT_KDEPTH /T ps_4_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_FILL_SKBT_PHONGBLINN_ps_4_0 /D __RENDERING_MODE=0 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=0 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_4_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_FILL_SKBT_DASHEDLINE_ps_4_0 /D __RENDERING_MODE=1 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=0 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_4_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_FILL_SKBT_MULTITEXTMAPPING_ps_4_0 /D __RENDERING_MODE=2 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=0 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_4_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_FILL_SKBT_TEXTMAPPING_ps_4_0 /D __RENDERING_MODE=3 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=0 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_4_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_FILL_SKBT_TEXTUREIMGMAP_ps_4_0 /D __RENDERING_MODE=4 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=0 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0

fxc /E PICKING_A_BUFFER_FILL /T ps_4_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs_4_0/PICKING_ABUFFER_PHONGBLINN_ps_4_0 /D __RENDERING_MODE=0 /D DX_11_STYLE=0
fxc /E PICKING_A_BUFFER_FILL /T ps_4_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs_4_0/PICKING_ABUFFER_DASHEDLINE_ps_4_0 /D __RENDERING_MODE=1 /D DX_11_STYLE=0
fxc /E PICKING_A_BUFFER_FILL /T ps_4_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs_4_0/PICKING_ABUFFER_MULTITEXTMAPPING_ps_4_0 /D __RENDERING_MODE=2 /D DX_11_STYLE=0
fxc /E PICKING_A_BUFFER_FILL /T ps_4_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs_4_0/PICKING_ABUFFER_TEXTMAPPING_ps_4_0 /D __RENDERING_MODE=3 /D DX_11_STYLE=0
fxc /E PICKING_A_BUFFER_FILL /T ps_4_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs_4_0/PICKING_ABUFFER_TEXTUREIMGMAP_ps_4_0 /D __RENDERING_MODE=4 /D DX_11_STYLE=0

fxc /E GS_Surfels /T gs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/GS_SurfelPoints_gs_4_0
fxc /E GS_ThickPoints_Pixels /T gs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/GS_ThickPoints_gs_4_0
fxc /E GS_ThickLines /T gs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/GS_ThickLines_gs_4_0

fxc /E RayCasting /T cs_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/VR_RAYMAX_cs_4_0 /D RAYMODE=1 /D OTF_MASK=0 /D MAX_LAYERS=8 /D FRAG_MERGING=1
fxc /E RayCasting /T cs_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/VR_RAYMIN_cs_4_0 /D RAYMODE=2 /D OTF_MASK=0 /D MAX_LAYERS=8 /D FRAG_MERGING=1
fxc /E RayCasting /T cs_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/VR_RAYSUM_cs_4_0 /D RAYMODE=3 /D OTF_MASK=0 /D MAX_LAYERS=8 /D FRAG_MERGING=1

fxc /E RayCasting /T cs_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/VR_DEFAULT_cs_4_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=0 /D FRAG_MERGING=0 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/VR_OPAQUE_cs_4_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=1 /D FRAG_MERGING=0 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/VR_CONTEXT_cs_4_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=2 /D FRAG_MERGING=0 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/VR_MULTIOTF_cs_4_0 /D RAYMODE=0 /D OTF_MASK=1 /D VR_MODE=0 /D FRAG_MERGING=0 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/VR_MASKVIS_cs_4_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=3 /D FRAG_MERGING=0 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1

fxc /E RayCasting /T cs_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/VR_DEFAULT_FM_cs_4_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=0 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/VR_OPAQUE_FM_cs_4_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=1 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/VR_CONTEXT_FM_cs_4_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=2 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/VR_MULTIOTF_FM_cs_4_0 /D RAYMODE=0 /D OTF_MASK=1 /D VR_MODE=0 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/VR_MULTIOTF_CONTEXT_FM_cs_4_0 /D RAYMODE=0 /D OTF_MASK=1 /D VR_MODE=2 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/VR_MASKVIS_FM_cs_4_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=3 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/VR_SCULPTMASK_FM_cs_4_0 /D RAYMODE=0 /D SCULPT_MASK=1 /D VR_MODE=0 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/VR_SCULPTMASK_CONTEXT_FM_cs_4_0 /D RAYMODE=0 /D SCULPT_MASK=1 /D VR_MODE=2 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1

fxc /E VR_SURFACE /T cs_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/VR_SURFACE_cs_4_0 /D OTF_MASK=0
fxc /E FillDither /T cs_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/FillDither_cs_4_0 

fxc /E CurvedSlicer /T cs_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/PanoVR_RAYMAX_cs_4_0 /D RAYMODE=1 /D OTF_MASK=0 /D MAX_LAYERS=8 /D FRAG_MERGING=1
fxc /E CurvedSlicer /T cs_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/PanoVR_RAYMIN_cs_4_0 /D RAYMODE=2 /D OTF_MASK=0 /D MAX_LAYERS=8 /D FRAG_MERGING=1
fxc /E CurvedSlicer /T cs_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/PanoVR_RAYSUM_cs_4_0 /D RAYMODE=3 /D OTF_MASK=0 /D MAX_LAYERS=8 /D FRAG_MERGING=1
fxc /E CurvedSlicer /T cs_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/PanoVR_DEFAULT_cs_4_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=0 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E CurvedSlicer /T cs_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/PanoVR_MODULATE_cs_4_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=2 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E CurvedSlicer /T cs_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/PanoVR_MULTIOTF_DEFAULT_cs_4_0 /D RAYMODE=0 /D OTF_MASK=1 /D VR_MODE=0 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E CurvedSlicer /T cs_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/PanoVR_MULTIOTF_MODULATE_cs_4_0 /D RAYMODE=0 /D OTF_MASK=1 /D VR_MODE=2 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1

fxc /E ThickSlicePathTracer /T cs_4_0 ./hlsl/PathTracer.hlsl /Fo ./shader_compiled_objs_4_0/ThickSlicePathTracer_cs_4_0 /D FRAG_MERGING=1 /D CURVEDPLANE=0
fxc /E ThickSlicePathTracer /T cs_4_0 ./hlsl/PathTracer.hlsl /Fo ./shader_compiled_objs_4_0/CurvedThickSlicePathTracer_cs_4_0 /D FRAG_MERGING=1 /D CURVEDPLANE=1

fxc /E ThickSlicePathTracer /T cs_4_0 ./hlsl/PathTracer.hlsl /Fo ./shader_compiled_objs_4_0/PickingThickSlice_cs_4_0 /D FRAG_MERGING=1 /D CURVEDPLANE=0 /D PICKING=1
fxc /E ThickSlicePathTracer /T cs_4_0 ./hlsl/PathTracer.hlsl /Fo ./shader_compiled_objs_4_0/PickingCurvedThickSlice_cs_4_0 /D FRAG_MERGING=1 /D CURVEDPLANE=1 /D PICKING=1

fxc /E Outline2D /T cs_4_0 ./hlsl/PathTracer.hlsl /Fo ./shader_compiled_objs_4_0/SliceOutline_cs_4_0 
