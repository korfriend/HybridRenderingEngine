fxc /E CommonVS_P /T vs_5_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs/SR_OIT_P_vs_5_0
fxc /E CommonVS_PN /T vs_5_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs/SR_OIT_PN_vs_5_0 
fxc /E CommonVS_PT /T vs_5_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs/SR_OIT_PT_vs_5_0
fxc /E CommonVS_PNT /T vs_5_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs/SR_OIT_PNT_vs_5_0 
fxc /E CommonVS_PTTT /T vs_5_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs/SR_OIT_PTTT_vs_5_0 

fxc /E SINGLE_LAYER /T ps_5_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs/SR_SINGLE_LAYER_ps_5_0

fxc /E OIT_RESOLVE /T cs_5_0 ./hlsl/kbuf/KbufResolve.hlsl /Fo ./shader_compiled_objs/OIT_SKBZ_RESOLVE_cs_5_0 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_SKBTZ_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_SKBTZ_DASHEDLINE_ps_5_0 /D __RENDERING_MODE=1 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_SKBTZ_MULTITEXTMAPPING_ps_5_0 /D __RENDERING_MODE=2 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_SKBTZ_TEXTMAPPING_ps_5_0 /D __RENDERING_MODE=3 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_SKBTZ_TEXTUREIMGMAP_ps_5_0 /D __RENDERING_MODE=4 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0
fxc /E OIT_PRESET /T cs_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_SINGLE_LAYER_TO_SKBTZ_cs_5_0 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D SILHOUETTE_EDGE=1 /D PIXEL_SYNCH=0 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DO_NOT_USE_DISCARD=1 /D DYNAMIC_K_MODE=0

fxc /E OIT_RESOLVE /T cs_5_0 ./hlsl/kbuf/KbufResolve.hlsl /Fo ./shader_compiled_objs/OIT_DKBZ_RESOLVE_cs_5_0 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=1 
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_DKBTZ_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=1
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_DKBTZ_DASHEDLINE_ps_5_0 /D __RENDERING_MODE=1 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=1
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_DKBTZ_MULTITEXTMAPPING_ps_5_0 /D __RENDERING_MODE=2 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=1
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_DKBTZ_TEXTMAPPING_ps_5_0 /D __RENDERING_MODE=3 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=1
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_DKBTZ_TEXTUREIMGMAP_ps_5_0 /D __RENDERING_MODE=4 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=1
fxc /E OIT_PRESET /T cs_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_SINGLE_LAYER_TO_DKBTZ_cs_5_0 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D SILHOUETTE_EDGE=1 /D PIXEL_SYNCH=0 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DO_NOT_USE_DISCARD=1 /D DYNAMIC_K_MODE=1

fxc /E OIT_RESOLVE /T cs_5_0 ./hlsl/kbuf/KbufResolve.hlsl /Fo ./shader_compiled_objs/OIT_DKB_RESOLVE_cs_5_0 /D FRAG_MERGING=0 /D DYNAMIC_K_MODE=1 
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_DKBT_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=1
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_DKBT_DASHEDLINE_ps_5_0 /D __RENDERING_MODE=1 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=1
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_DKBT_MULTITEXTMAPPING_ps_5_0 /D __RENDERING_MODE=2 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=1
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_DKBT_TEXTMAPPING_ps_5_0 /D __RENDERING_MODE=3 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=1
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_DKBT_TEXTUREIMGMAP_ps_5_0 /D __RENDERING_MODE=4 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=1
fxc /E OIT_PRESET /T cs_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_SINGLE_LAYER_TO_DKBT_cs_5_0 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D SILHOUETTE_EDGE=1 /D PIXEL_SYNCH=0 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DO_NOT_USE_DISCARD=1 /D DYNAMIC_K_MODE=1

fxc /E KB_SSAO /T cs_5_0 ./hlsl/ssao/SSAO.hlsl /Fo ./shader_compiled_objs/KB_SSAO_cs_5_0 /D HBAO=1
fxc /E KB_SSAO_BLUR /T cs_5_0 ./hlsl/ssao/SSAO.hlsl /Fo ./shader_compiled_objs/KB_SSAO_BLUR_cs_5_0 
fxc /E KB_TO_TEXTURE /T cs_5_0 ./hlsl/ssao/SSAO.hlsl /Fo ./shader_compiled_objs/KBZ_TO_TEXTURE_cs_5_0 
//fxc /E KB_SSAO /T cs_5_0 ./hlsl/ssao/SSAO.hlsl /Fo ./shader_compiled_objs/KB_SSAO_cs_5_0 /D MAX_LAYERS=8 /D HBAO=1
//fxc /E KB_SSAO_BLUR /T cs_5_0 ./hlsl/ssao/SSAO.hlsl /Fo ./shader_compiled_objs/KB_SSAO_BLUR_cs_5_0 /D MAX_LAYERS=8
//fxc /E KB_TO_TEXTURE /T cs_5_0 ./hlsl/ssao/SSAO.hlsl /Fo ./shader_compiled_objs/KBZ_TO_TEXTURE_cs_5_0 /D MAX_LAYERS=8

fxc /E FillHistogram /T cs_5_0 ./hlsl/kbuf/KplusB.hlsl /Fo ./shader_compiled_objs/SR_FillHistogram_cs_5_0
fxc /E CreateOffsetTableKpB /T cs_5_0 ./hlsl/kbuf/KplusB.hlsl /Fo ./shader_compiled_objs/SR_CreateOffsetTableKpB_cs_5_0

fxc /E SortAndRenderCS /T cs_5_0 ./hlsl/dx_abuf/DxABuf_Sort.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_SORT2SENDER_cs_5_0 /D DX_11_STYLE=0 /D DX_11_OIT=0
fxc /E OIT_A_BUFFER_CNF_FRAGS /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_FRAGCOUNTER_ps_5_0 /D DX_11_STYLE=0 
fxc /E OIT_A_BUFFER_CNF_FRAGS /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_FRAGCOUNTER_MTT_ps_5_0 /D DX_11_STYLE=0 /D __RENDERING_MODE=2
fxc /E OIT_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 /D DX_11_STYLE=0
fxc /E OIT_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_DASHEDLINE_ps_5_0 /D __RENDERING_MODE=1 /D DX_11_STYLE=0
fxc /E OIT_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_MULTITEXTMAPPING_ps_5_0 /D __RENDERING_MODE=2 /D DX_11_STYLE=0
fxc /E OIT_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_TEXTMAPPING_ps_5_0 /D __RENDERING_MODE=3 /D DX_11_STYLE=0
fxc /E OIT_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_TEXTUREIMGMAP_ps_5_0 /D __RENDERING_MODE=4 /D DX_11_STYLE=0
fxc /E CreatePrefixSum_Pass0_CS /T cs_5_0 ./hlsl/dx_abuf/DxABuf_Sort.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_PREFIX_0_cs_5_0 /D DX_11_STYLE=1
fxc /E CreatePrefixSum_Pass1_CS /T cs_5_0 ./hlsl/dx_abuf/DxABuf_Sort.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_PREFIX_1_cs_5_0 /D DX_11_STYLE=1
fxc /E CreateOffsetTable_CS /T cs_5_0 ./hlsl/dx_abuf/DxABuf_Sort.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_OffsetTable_cs_5_0

fxc /E Moment_GeneratePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_GEN_ps_5_0 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D MOMENT_GENERATION=1 /D NUM_MOMENTS=8 /D USE_ROV=1
fxc /E Moment_GeneratePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_GEN_MTT_ps_5_0 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D MOMENT_GENERATION=1 /D NUM_MOMENTS=8 /D __RENDERING_MODE=2 /D USE_ROV=1
fxc /E Moment_GeneratePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_GEN_TEXT_ps_5_0 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D MOMENT_GENERATION=1 /D NUM_MOMENTS=8 /D __RENDERING_MODE=3 /D USE_ROV=1
fxc /E Moment_ResolvePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_OIT_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D NUM_MOMENTS=8 /D USE_ROV=1
fxc /E Moment_ResolvePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_OIT_DASHEDLINE_ps_5_0 /D __RENDERING_MODE=1 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D NUM_MOMENTS=8 /D USE_ROV=1
fxc /E Moment_ResolvePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_OIT_MULTITEXTMAPPING_ps_5_0 /D __RENDERING_MODE=2 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D NUM_MOMENTS=8 /D USE_ROV=1
fxc /E Moment_ResolvePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_OIT_TEXTMAPPING_ps_5_0 /D __RENDERING_MODE=3 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D NUM_MOMENTS=8 /D USE_ROV=1
fxc /E Moment_ResolvePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_OIT_TEXTUREIMGMAP_ps_5_0 /D __RENDERING_MODE=4 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D NUM_MOMENTS=8 /D USE_ROV=1

fxc /E GS_ThickPoints /T gs_5_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs/GS_ThickPoints_gs_5_0
fxc /E GS_ThickLines /T gs_5_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs/GS_ThickLines_gs_5_0

//fxc /E DVR /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_RAYMAX_cs_5_0 /D RAYMODE=1 /DOTF_MASK=0 /D MAX_LAYERS=8
//fxc /E DVR /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_RAYMIN_cs_5_0 /D RAYMODE=2 /DOTF_MASK=0 /D MAX_LAYERS=8
//fxc /E DVR /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_RAYSUM_cs_5_0 /D RAYMODE=3 /DOTF_MASK=0 /D MAX_LAYERS=8
//fxc /E DVR /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_DEFAULT_cs_5_0 /D RAYMODE=0 /DOTF_MASK=0 /D MAX_LAYERS=8 /D VR_MODE=0
//fxc /E DVR /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_OPAQUE_cs_5_0 /D RAYMODE=0 /DOTF_MASK=0 /D MAX_LAYERS=8 /D VR_MODE=1
//fxc /E DVR /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_CONTEXT_cs_5_0 /D RAYMODE=0 /DOTF_MASK=0 /D MAX_LAYERS=8 /D VR_MODE=2
//fxc /E VR_SURFACE /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_SURFACE_cs_5_0 /D RAYMODE=0 /DOTF_MASK=0 /D MAX_LAYERS=8
fxc /E DVR /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_RAYMAX_cs_5_0 /D RAYMODE=1 /DOTF_MASK=0 
fxc /E DVR /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_RAYMIN_cs_5_0 /D RAYMODE=2 /DOTF_MASK=0 
fxc /E DVR /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_RAYSUM_cs_5_0 /D RAYMODE=3 /DOTF_MASK=0 
fxc /E DVR /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_DEFAULT_cs_5_0 /D RAYMODE=0 /DOTF_MASK=0 /D VR_MODE=0
fxc /E DVR /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_OPAQUE_cs_5_0 /D RAYMODE=0 /DOTF_MASK=0 /D VR_MODE=1
fxc /E DVR /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_CONTEXT_cs_5_0 /D RAYMODE=0 /DOTF_MASK=0 /D VR_MODE=2
fxc /E VR_SURFACE /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_SURFACE_cs_5_0 /D RAYMODE=0 /DOTF_MASK=0 