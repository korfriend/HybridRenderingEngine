fxc /E CommonVS_P /T vs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_P_vs_4_0
fxc /E CommonVS_PN /T vs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_PN_vs_4_0
fxc /E CommonVS_PT /T vs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_PT_vs_4_0
fxc /E CommonVS_PNT /T vs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_PNT_vs_4_0 
fxc /E CommonVS_PTTT /T vs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_PTTT_vs_4_0 

fxc /E SINGLE_LAYER /T ps_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_SINGLE_LAYER_ps_4_0

fxc /E BasicShader4 /T ps_4_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs_4_0/SR_BASIC_PHONGBLINN_ps_4_0 /D __RENDERING_MODE=0 
fxc /E BasicShader4 /T ps_4_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs_4_0/SR_BASIC_DASHEDLINE_ps_4_0 /D __RENDERING_MODE=1 
fxc /E BasicShader4 /T ps_4_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs_4_0/SR_BASIC_MULTITEXTMAPPING_ps_4_0 /D __RENDERING_MODE=2 
fxc /E BasicShader4 /T ps_4_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs_4_0/SR_BASIC_TEXTMAPPING_ps_4_0 /D __RENDERING_MODE=3 
fxc /E BasicShader4 /T ps_4_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs_4_0/SR_BASIC_TEXTUREIMGMAP_ps_4_0 /D __RENDERING_MODE=4 
fxc /E BasicShader4 /T ps_4_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs_4_0/SR_BASIC_VOLUMEMAP_ps_4_0 /D __RENDERING_MODE=5 
fxc /E BasicShader4 /T ps_4_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs_4_0/SR_BASIC_OUTLINE_ps_4_0 /D __RENDERING_MODE=6 

fxc /E GS_PickingPoint /T gs_4_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs_4_0/GS_PickingBasic_gs_4_0 
fxc /E GS_MeshCutLines /T gs_4_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs_4_0/GS_MeshCutLines_gs_4_0 

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
