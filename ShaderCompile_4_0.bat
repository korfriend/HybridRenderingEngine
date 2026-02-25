fxc /E QuadVS /T vs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_QUAD_P_vs_4_0 /D VSIN_N=0 /D VSIN_T=0 /D VSIN_C=0 /D VSIN_G=0

fxc /E CommonVS /T vs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_P_vs_4_0 /D VSIN_N=0 /D VSIN_T=0 /D VSIN_C=0 /D VSIN_G=0
fxc /E CommonVS /T vs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_PN_vs_4_0 /D VSIN_N=1 /D VSIN_T=0 /D VSIN_C=0 /D VSIN_G=0
fxc /E CommonVS /T vs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_PC_vs_4_0 /D VSIN_N=0 /D VSIN_T=0 /D VSIN_C=1 /D VSIN_G=0
fxc /E CommonVS /T vs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_PT_vs_4_0 /D VSIN_N=0 /D VSIN_T=1 /D VSIN_C=0 /D VSIN_G=0
fxc /E CommonVS /T vs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_PNC_vs_4_0 /D VSIN_N=1 /D VSIN_T=0 /D VSIN_C=1 /D VSIN_G=0
fxc /E CommonVS /T vs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_PNT_vs_4_0 /D VSIN_N=1 /D VSIN_T=1 /D VSIN_C=0 /D VSIN_G=0
fxc /E CommonVS /T vs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_PTC_vs_4_0 /D VSIN_N=0 /D VSIN_T=1 /D VSIN_C=1 /D VSIN_G=0
fxc /E CommonVS /T vs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_PNTC_vs_4_0 /D VSIN_N=1 /D VSIN_T=1 /D VSIN_C=1 /D VSIN_G=0
fxc /E CommonVS_PTTT /T vs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_PTTT_vs_4_0 
fxc /E CommonVS /T vs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_PG_vs_4_0 /D VSIN_N=0 /D VSIN_T=0 /D VSIN_C=0 /D VSIN_G=1
fxc /E CommonVS /T vs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_PNG_vs_4_0 /D VSIN_N=1 /D VSIN_T=0 /D VSIN_C=0 /D VSIN_G=1
fxc /E CommonVS /T vs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_PCG_vs_4_0 /D VSIN_N=0 /D VSIN_T=0 /D VSIN_C=1 /D VSIN_G=1
fxc /E CommonVS /T vs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_PTG_vs_4_0 /D VSIN_N=0 /D VSIN_T=1 /D VSIN_C=0 /D VSIN_G=1
fxc /E CommonVS /T vs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_PNCG_vs_4_0 /D VSIN_N=1 /D VSIN_T=0 /D VSIN_C=1 /D VSIN_G=1
fxc /E CommonVS /T vs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_PNTG_vs_4_0 /D VSIN_N=1 /D VSIN_T=1 /D VSIN_C=0 /D VSIN_G=1
fxc /E CommonVS /T vs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_PTCG_vs_4_0 /D VSIN_N=0 /D VSIN_T=1 /D VSIN_C=1 /D VSIN_G=1
fxc /E CommonVS /T vs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_PNTCG_vs_4_0 /D VSIN_N=1 /D VSIN_T=1 /D VSIN_C=1 /D VSIN_G=1

fxc /E WRITE_DEPTHZ /T ps_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/WRITE_DEPTH_ps_4_0

fxc /E SINGLE_LAYER /T ps_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_SINGLE_LAYER_ps_4_0

fxc /E BasicShader4 /T ps_4_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs_4_0/SR_BASIC_PHONGBLINN_ps_4_0 /D __RENDERING_MODE=0 /D DX10_0=1
fxc /E BasicShader4 /T ps_4_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs_4_0/SR_BASIC_DASHEDLINE_ps_4_0 /D __RENDERING_MODE=1 /D DX10_0=1
fxc /E BasicShader4 /T ps_4_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs_4_0/SR_BASIC_MULTITEXTMAPPING_ps_4_0 /D __RENDERING_MODE=2 /D DX10_0=1
fxc /E BasicShader4 /T ps_4_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs_4_0/SR_BASIC_TEXTMAPPING_ps_4_0 /D __RENDERING_MODE=3 /D DX10_0=1
fxc /E BasicShader4 /T ps_4_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs_4_0/SR_BASIC_TEXTUREIMGMAP_ps_4_0 /D __RENDERING_MODE=4 /D DX10_0=1
fxc /E BasicShader4 /T ps_4_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs_4_0/SR_BASIC_VOLUMEMAP_ps_4_0 /D __RENDERING_MODE=5 /D DX10_0=1
fxc /E BasicShader4 /T ps_4_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs_4_0/SR_BASIC_VOLUME_DIST_MAP_ps_4_0 /D __RENDERING_MODE=6 /D DX10_0=1

fxc /E BasicShader4 /T ps_4_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs_4_0/SR_QUAD_OUTLINE_ps_4_0 /D __RENDERING_MODE=100 /D DX10_0=1

fxc /E GS_TriNormal /T gs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs/GS_TriNormal_gs_4_0
fxc /E GS_PickingPoint /T gs_4_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs_4_0/GS_PickingBasic_gs_4_0 
fxc /E GS_MeshCutLines /T gs_4_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs_4_0/GS_MeshCutLines_gs_4_0 

fxc /E GS_Surfels /T gs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/GS_SurfelPoints_gs_4_0
fxc /E GS_ThickPoints_Pixels /T gs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/GS_ThickPoints_gs_4_0
fxc /E GS_ThickLines /T gs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/GS_ThickLines_gs_4_0

fxc /E ThickSlicePathTracer /T ps_4_0 ./hlsl/RayProcessing.hlsl /Fo ./shader_compiled_objs_4_0/PickingThickSlice_ps_4_0 /D CURVEDPLANE=0 /D PICKING=1 /D PATHTR_USE_KBUF=1 /D DX10_0=1 
fxc /E ThickSlicePathTracer /T ps_4_0 ./hlsl/RayProcessing.hlsl /Fo ./shader_compiled_objs_4_0/PickingCurvedThickSlice_ps_4_0 /D CURVEDPLANE=1 /D PICKING=1 /D PATHTR_USE_KBUF=1 /D DX10_0=1 

fxc /E ThickSlicePathTracer /T ps_4_0 ./hlsl/RayProcessing.hlsl /Fo ./shader_compiled_objs_4_0/ThickSlicePathTracer_ps_4_0 /D FRAG_MERGING=0 /D CURVEDPLANE=0 /D DX10_0=1
fxc /E ThickSlicePathTracer /T ps_4_0 ./hlsl/RayProcessing.hlsl /Fo ./shader_compiled_objs_4_0/CurvedThickSlicePathTracer_ps_4_0 /D FRAG_MERGING=0 /D CURVEDPLANE=1 /D DX10_0=1

fxc /E Outline2D /T ps_4_0 ./hlsl/RayProcessing.hlsl /Fo ./shader_compiled_objs_4_0/SliceOutline_ps_4_0 /D DX10_0=1

fxc /E RayCasting /T ps_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/VR_RAYMAX_ps_4_0 /D RAYMODE=1 /D OTF_MASK=0 /D MAX_LAYERS=8 /D FRAG_MERGING=0 /D DX10_0=1
fxc /E RayCasting /T ps_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/VR_RAYMIN_ps_4_0 /D RAYMODE=2 /D OTF_MASK=0 /D MAX_LAYERS=8 /D FRAG_MERGING=0 /D DX10_0=1
fxc /E RayCasting /T ps_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/VR_RAYSUM_ps_4_0 /D RAYMODE=3 /D OTF_MASK=0 /D MAX_LAYERS=8 /D FRAG_MERGING=0 /D DX10_0=1

fxc /E RayCasting /T ps_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/VR_DEFAULT_ps_4_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=0 /D FRAG_MERGING=0 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1 /D DX10_0=1
fxc /E RayCasting /T ps_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/VR_OPAQUE_ps_4_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=1 /D FRAG_MERGING=0 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1 /D DX10_0=1
fxc /E RayCasting /T ps_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/VR_CONTEXT_ps_4_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=2 /D FRAG_MERGING=0 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1 /D DX10_0=1
fxc /E RayCasting /T ps_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/VR_MULTIOTF_ps_4_0 /D RAYMODE=0 /D OTF_MASK=1 /D VR_MODE=0 /D FRAG_MERGING=0 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1 /D DX10_0=1
fxc /E RayCasting /T ps_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/VR_MULTIOTF_CONTEXT_ps_4_0 /D RAYMODE=0 /D OTF_MASK=1 /D VR_MODE=2 /D FRAG_MERGING=0 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1 /D DX10_0=1
fxc /E RayCasting /T ps_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/VR_MASKVIS_ps_4_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=3 /D FRAG_MERGING=0 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1 /D DX10_0=1
fxc /E RayCasting /T ps_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/VR_SCULPTMASK_ps_4_0 /D RAYMODE=0 /D SCULPT_MASK=1 /D VR_MODE=0 /D FRAG_MERGING=0 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1 /D DX10_0=1
fxc /E RayCasting /T ps_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/VR_SCULPTMASK_CONTEXT_ps_4_0 /D RAYMODE=0 /D SCULPT_MASK=1 /D VR_MODE=2 /D FRAG_MERGING=0 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1 /D DX10_0=1
fxc /E RayCasting /T ps_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/VR_DEFAULT_SCULPTBITS_ps_4_0 /D RAYMODE=0 /D SCULPT_BITS=1  /D VR_MODE=0 /D FRAG_MERGING=0 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1 /D DX10_0=1
fxc /E RayCasting /T ps_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/VR_CONTEXT_SCULPTBITS_ps_4_0 /D RAYMODE=0 /D SCULPT_BITS=1  /D VR_MODE=2 /D FRAG_MERGING=0 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1 /D DX10_0=1

fxc /E VR_SURFACE /T ps_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/VR_SURFACE_ps_4_0 /D OTF_MASK=0 /D DX10_0=1

fxc /E CurvedSlicer /T ps_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/PanoVR_RAYMAX_ps_4_0 /D RAYMODE=1 /D OTF_MASK=0 /D MAX_LAYERS=8 /D FRAG_MERGING=1 /D DX10_0=1
fxc /E CurvedSlicer /T ps_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/PanoVR_RAYMIN_ps_4_0 /D RAYMODE=2 /D OTF_MASK=0 /D MAX_LAYERS=8 /D FRAG_MERGING=1 /D DX10_0=1
fxc /E CurvedSlicer /T ps_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/PanoVR_RAYSUM_ps_4_0 /D RAYMODE=3 /D OTF_MASK=0 /D MAX_LAYERS=8 /D FRAG_MERGING=1 /D DX10_0=1
fxc /E CurvedSlicer /T ps_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/PanoVR_DEFAULT_ps_4_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=0 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1 /D DX10_0=1
fxc /E CurvedSlicer /T ps_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/PanoVR_MODULATE_ps_4_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=2 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1 /D DX10_0=1
fxc /E CurvedSlicer /T ps_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/PanoVR_MULTIOTF_DEFAULT_ps_4_0 /D RAYMODE=0 /D OTF_MASK=1 /D VR_MODE=0 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1 /D DX10_0=1
fxc /E CurvedSlicer /T ps_4_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs_4_0/PanoVR_MULTIOTF_MODULATE_ps_4_0 /D RAYMODE=0 /D OTF_MASK=1 /D VR_MODE=2 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1 /D DX10_0=1

