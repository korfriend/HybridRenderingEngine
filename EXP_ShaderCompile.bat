fxc /E CommonVS_P /T vs_5_0 ./hlsl/kbuf/Sr_Common.hlsl /Fo ./shader_compiled_objs/SR_OIT_P_vs_5_0
fxc /E CommonVS_PN /T vs_5_0 ./hlsl/kbuf/Sr_Common.hlsl /Fo ./shader_compiled_objs/SR_OIT_PN_vs_5_0 
fxc /E CommonVS_PT /T vs_5_0 ./hlsl/kbuf/Sr_Common.hlsl /Fo ./shader_compiled_objs/SR_OIT_PT_vs_5_0
fxc /E CommonVS_PNT /T vs_5_0 ./hlsl/kbuf/Sr_Common.hlsl /Fo ./shader_compiled_objs/SR_OIT_PNT_vs_5_0 
fxc /E CommonVS_PTTT /T vs_5_0 ./hlsl/kbuf/Sr_Common.hlsl /Fo ./shader_compiled_objs/SR_OIT_PTTT_vs_5_0 

fxc /E OIT_ARRAGNGE /T cs_5_0 ./hlsl/kbuf/KbufResolve.hlsl /Fo ./shader_compiled_objs/SR_OIT_SORT2RENDER_cs_5_0 /D MAX_LAYERS=8 /D ZF_HANDLING=1
fxc /E KB_SSAO /T cs_5_0 ./hlsl/kbuf/SSAO.hlsl /Fo ./shader_compiled_objs/KB_SSAO_cs_5_0 /D MAX_LAYERS=8 /D HBAO=1
fxc /E KB_SSAO_BLUR /T cs_5_0 ./hlsl/kbuf/SSAO.hlsl /Fo ./shader_compiled_objs/KB_SSAO_BLUR_cs_5_0 /D MAX_LAYERS=8
fxc /E KB_TO_TEXTURE /T cs_5_0 ./hlsl/kbuf/SSAO.hlsl /Fo ./shader_compiled_objs/KBZ_TO_TEXTURE_cs_5_0 /D MAX_LAYERS=8

fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_KDEPTH_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 /D ZF_HANDLING=1 /D TAIL_HANDLING=1 /D MAX_LAYERS=8 /D PIXEL_SYNCH=1 /D USE_ROV=1
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_KDEPTH_DASHEDLINE_ps_5_0 /D __RENDERING_MODE=1 /D ZF_HANDLING=1 /D TAIL_HANDLING=1 /D MAX_LAYERS=8 /D PIXEL_SYNCH=1 /D USE_ROV=1 
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_KDEPTH_MULTITEXTMAPPING_ps_5_0 /D __RENDERING_MODE=2 /D ZF_HANDLING=1 /D TAIL_HANDLING=1 /D MAX_LAYERS=8 /D PIXEL_SYNCH=1 /D USE_ROV=1 
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_KDEPTH_TEXTMAPPING_ps_5_0 /D __RENDERING_MODE=3 /D ZF_HANDLING=1 /D TAIL_HANDLING=1 /D MAX_LAYERS=8 /D PIXEL_SYNCH=1 /D USE_ROV=1
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_KDEPTH_TEXTUREIMGMAP_ps_5_0 /D __RENDERING_MODE=4 /D ZF_HANDLING=1 /D TAIL_HANDLING=1 /D MAX_LAYERS=8 /D PIXEL_SYNCH=1 /D USE_ROV=1 

fxc /E SINGLE_LAYER /T ps_5_0 ./hlsl/kbuf/Sr_Common.hlsl /Fo ./shader_compiled_objs/SR_SINGLE_LAYER_ps_5_0
fxc /E OIT_PRESET /T cs_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_PRESET_cs_5_0 /D ZF_HANDLING=1 /D TAIL_HANDLING=1 /D MAX_LAYERS=8 /D SILHOUETTE_EDGE=1 /D PIXEL_SYNCH=0 /D USE_ROV=1


fxc /E SortAndRenderCS /T cs_5_0 ./hlsl/dx_abuf/DxPerPixelLL_Sort.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_SORT2SENDER_cs_5_0 
fxc /E OIT_A_BUFFER_CNF_FRAGS /T ps_5_0 ./hlsl/dx_abuf/DxPerPixelLL.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_FRAGCOUNTER_ps_5_0 
fxc /E OIT_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxPerPixelLL.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0
fxc /E OIT_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxPerPixelLL.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_DASHEDLINE_ps_5_0 /D __RENDERING_MODE=1
fxc /E OIT_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxPerPixelLL.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_MULTITEXTMAPPING_ps_5_0 /D __RENDERING_MODE=2
fxc /E OIT_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxPerPixelLL.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_TEXTMAPPING_ps_5_0 /D __RENDERING_MODE=3
fxc /E OIT_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxPerPixelLL.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_TEXTUREIMGMAP_ps_5_0 /D __RENDERING_MODE=4
fxc /E CreatePrefixSum_Pass0_CS /T cs_5_0 ./hlsl/dx_abuf/DxPerPixelLL_Sort.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_PREFIX_0_cs_5_0 
fxc /E CreatePrefixSum_Pass1_CS /T cs_5_0 ./hlsl/dx_abuf/DxPerPixelLL_Sort.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_PREFIX_1_cs_5_0  


fxc /E Moment_GeneratePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_GEN_ps_5_0 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D MOMENT_GENERATION=1 /D NUM_MOMENTS=4
fxc /E Moment_ResolvePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_OIT_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D NUM_MOMENTS=4
fxc /E Moment_ResolvePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_OIT_DASHEDLINE_ps_5_0 /D __RENDERING_MODE=1 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D NUM_MOMENTS=4 
fxc /E Moment_ResolvePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_OIT_MULTITEXTMAPPING_ps_5_0 /D __RENDERING_MODE=2 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D NUM_MOMENTS=4 
fxc /E Moment_ResolvePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_OIT_TEXTMAPPING_ps_5_0 /D __RENDERING_MODE=3 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D NUM_MOMENTS=4
fxc /E Moment_ResolvePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_OIT_TEXTUREIMGMAP_ps_5_0 /D __RENDERING_MODE=4 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D NUM_MOMENTS=4 

fxc /E GS_ThickPoints /T gs_5_0 ./hlsl/kbuf/Sr_Common.hlsl /Fo ./shader_compiled_objs/GS_ThickPoints_gs_5_0
fxc /E GS_ThickLines /T gs_5_0 ./hlsl/kbuf/Sr_Common.hlsl /Fo ./shader_compiled_objs/GS_ThickLines_gs_5_0

fxc /E DVR /T cs_5_0 ./hlsl/kbuf/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_RAYMAX_cs_5_0 /D RAYMODE=1 /DOTF_MASK=0 /D MAX_LAYERS=8
fxc /E DVR /T cs_5_0 ./hlsl/kbuf/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_RAYMIN_cs_5_0 /D RAYMODE=2 /DOTF_MASK=0 /D MAX_LAYERS=8
fxc /E DVR /T cs_5_0 ./hlsl/kbuf/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_RAYSUM_cs_5_0 /D RAYMODE=3 /DOTF_MASK=0 /D MAX_LAYERS=8
fxc /E DVR /T cs_5_0 ./hlsl/kbuf/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_DEFAULT_cs_5_0 /D RAYMODE=0 /DOTF_MASK=0 /D MAX_LAYERS=8 /D VR_MODE=0
fxc /E DVR /T cs_5_0 ./hlsl/kbuf/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_OPAQUE_cs_5_0 /D RAYMODE=0 /DOTF_MASK=0 /D MAX_LAYERS=8 /D VR_MODE=1
fxc /E DVR /T cs_5_0 ./hlsl/kbuf/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_CONTEXT_cs_5_0 /D RAYMODE=0 /DOTF_MASK=0 /D MAX_LAYERS=8 /D VR_MODE=2
fxc /E VR_SURFACE /T cs_5_0 ./hlsl/kbuf/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_SURFACE_cs_5_0 /D RAYMODE=0 /DOTF_MASK=0 /D MAX_LAYERS=8


fxc /E DVR /T cs_5_0 ./hlsl/kbuf/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_DEFAULT_cs_5_0 /D RAYMODE=0 /DOTF_MASK=0 /D MAX_LAYERS=8 /D VR_MODE=0 /D GHOST_EFFECT=1
fxc /E DVR /T cs_5_0 ./hlsl/kbuf/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_OPAQUE_cs_5_0 /D RAYMODE=0 /DOTF_MASK=0 /D MAX_LAYERS=8 /D VR_MODE=1 /D GHOST_EFFECT=1
fxc /E DVR /T cs_5_0 ./hlsl/kbuf/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_CONTEXT_cs_5_0 /D RAYMODE=0 /DOTF_MASK=0 /D MAX_LAYERS=8 /D VR_MODE=2 /D GHOST_EFFECT=1


fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_KDEPTH_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 /D ZF_HANDLING=0 /D TAIL_HANDLING=0 /D MAX_LAYERS=8 /D USE_ROV=0
fxc /E OIT_ARRAGNGE /T cs_5_0 ./hlsl/kbuf/KbufResolve.hlsl /Fo ./shader_compiled_objs/SR_OIT_SORT2RENDER_cs_5_0 /D MAX_LAYERS=8 /D ZF_HANDLING=0

fxc /E Moment_GeneratePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_GEN_ps_5_0 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D MOMENT_GENERATION=1 /D NUM_MOMENTS=8
fxc /E Moment_ResolvePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_OIT_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D NUM_MOMENTS=8

//fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_KDEPTH_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 /D ZF_HANDLING=1 /D TAIL_HANDLING=1
//fxc /E OIT_ARRAGNGE /T cs_5_0 ./hlsl/kbuf/KbufResolve.hlsl /Fo ./shader_compiled_objs/SR_OIT_SORT2RENDER_cs_5_0 /D MAX_LAYERS=8 /D ZF_HANDLING=1
//fxc /E SINGLE_LAYER /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_SINGLE_LAYER_ps_5_0 /D ZF_HANDLING=1 /D TAIL_HANDLING=1
//fxc /E OIT_PRESET /T cs_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_PRESET_cs_5_0 /D __INTERLOCK=0 /D ZF_HANDLING=1 /D TAIL_HANDLING=1
//
//
//fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_KDEPTH_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 /D ZF_HANDLING=0 /D TAIL_HANDLING=0
//fxc /E OIT_ARRAGNGE /T cs_5_0 ./hlsl/kbuf/KbufResolve.hlsl /Fo ./shader_compiled_objs/SR_OIT_SORT2RENDER_cs_5_0 /D MAX_LAYERS=8 /D ZF_HANDLING=0
//fxc /E SINGLE_LAYER /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_SINGLE_LAYER_ps_5_0 /D ZF_HANDLING=0 /D TAIL_HANDLING=0
//fxc /E OIT_PRESET /T cs_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_PRESET_cs_5_0 /D __INTERLOCK=0 /D ZF_HANDLING=0 /D TAIL_HANDLING=0

//fxc /E Moment_GeneratePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_GEN_ps_5_0 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D MOMENT_GENERATION=1 /D NUM_MOMENTS=4
//fxc /E Moment_GeneratePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_GEN_ps_5_0 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D MOMENT_GENERATION=1 /D NUM_MOMENTS=4 /D TRIGONOMETRIC=1
//fxc /E Moment_ResolvePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_OIT_ps_5_0 /D SINGLE_PRECISION=1 /D NUM_MOMENTS=4 /D TRIGONOMETRIC=1
//fxc /E Moment_ResolvePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_OIT_ps_5_0 /D SINGLE_PRECISION=1 /D NUM_MOMENTS=4 


fxc /E Moment_GeneratePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_GEN_ps_5_0 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D MOMENT_GENERATION=1 /D NUM_MOMENTS=8 /D TRIGONOMETRIC=1
fxc /E Moment_ResolvePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_OIT_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 /D SINGLE_PRECISION=1 /D NUM_MOMENTS=8 /D TRIGONOMETRIC=1

fxc /E Moment_GeneratePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_GEN_ps_5_0 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D MOMENT_GENERATION=1 /D NUM_MOMENTS=8
fxc /E Moment_ResolvePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_OIT_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D NUM_MOMENTS=8 