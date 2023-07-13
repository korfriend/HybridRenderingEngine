fxc /E CommonVS_P /T vs_5_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs/SR_OIT_P_vs_5_0
fxc /E CommonVS_PN /T vs_5_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs/SR_OIT_PN_vs_5_0 
fxc /E CommonVS_PT /T vs_5_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs/SR_OIT_PT_vs_5_0
fxc /E CommonVS_PNT /T vs_5_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs/SR_OIT_PNT_vs_5_0 
fxc /E CommonVS_PTTT /T vs_5_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs/SR_OIT_PTTT_vs_5_0 

fxc /E BasicShader4 /T ps_5_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs/SR_BASIC_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 
fxc /E BasicShader4 /T ps_5_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs/SR_BASIC_DASHEDLINE_ps_5_0 /D __RENDERING_MODE=1 
fxc /E BasicShader4 /T ps_5_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs/SR_BASIC_MULTITEXTMAPPING_ps_5_0 /D __RENDERING_MODE=2 
fxc /E BasicShader4 /T ps_5_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs/SR_BASIC_TEXTMAPPING_ps_5_0 /D __RENDERING_MODE=3 
fxc /E BasicShader4 /T ps_5_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs/SR_BASIC_TEXTUREIMGMAP_ps_5_0 /D __RENDERING_MODE=4 
fxc /E BasicShader4 /T ps_5_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs/SR_BASIC_VOLUMEMAP_ps_5_0 /D __RENDERING_MODE=5 

fxc /E BasicShader4 /T ps_5_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs/SR_QUAD_OUTLINE_ps_5_0 /D __RENDERING_MODE=6 

fxc /E SINGLE_LAYER /T ps_5_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs/SR_SINGLE_LAYER_ps_5_0

fxc /E OIT_RESOLVE /T cs_5_0 ./hlsl/kbuf/KbufResolve.hlsl /Fo ./shader_compiled_objs/OIT_SKBZ_RESOLVE_cs_5_0 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 

// deprecated //
fxc /E OIT_PRESET /T cs_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_SINGLE_LAYER_TO_SKBTZ_cs_5_0 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D SILHOUETTE_EDGE=1 /D DO_NOT_USE_DISCARD=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0 /D USE_ROV=1

fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_SKBTZ_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=0 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_SKBTZ_DASHEDLINE_ps_5_0 /D __RENDERING_MODE=1 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=0 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_SKBTZ_MULTITEXTMAPPING_ps_5_0 /D __RENDERING_MODE=2 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=0 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_SKBTZ_TEXTMAPPING_ps_5_0 /D __RENDERING_MODE=3 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=0 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_SKBTZ_TEXTUREIMGMAP_ps_5_0 /D __RENDERING_MODE=4 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=0 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0

fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_SKBTZ_PHONGBLINN_ROV_ps_5_0 /D __RENDERING_MODE=0 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_SKBTZ_DASHEDLINE_ROV_ps_5_0 /D __RENDERING_MODE=1 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_SKBTZ_MULTITEXTMAPPING_ROV_ps_5_0 /D __RENDERING_MODE=2 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_SKBTZ_TEXTMAPPING_ROV_ps_5_0 /D __RENDERING_MODE=3 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_SKBTZ_TEXTUREIMGMAP_ROV_ps_5_0 /D __RENDERING_MODE=4 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0

fxc /E OIT_RESOLVE /T cs_5_0 ./hlsl/kbuf/KbufResolve.hlsl /Fo ./shader_compiled_objs/OIT_SKB_RESOLVE_cs_5_0 /D FRAG_MERGING=0 /D DYNAMIC_K_MODE=0
// deprecated //
fxc /E OIT_PRESET /T cs_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_SINGLE_LAYER_TO_SKBT_cs_5_0 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D SILHOUETTE_EDGE=1 /D DO_NOT_USE_DISCARD=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0

fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_SKBT_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=0 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_SKBT_DASHEDLINE_ps_5_0 /D __RENDERING_MODE=1 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=0 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_SKBT_MULTITEXTMAPPING_ps_5_0 /D __RENDERING_MODE=2 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=0 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_SKBT_TEXTMAPPING_ps_5_0 /D __RENDERING_MODE=3 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=0 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_SKBT_TEXTUREIMGMAP_ps_5_0 /D __RENDERING_MODE=4 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=0 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0

fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_SKBT_PHONGBLINN_ROV_ps_5_0 /D __RENDERING_MODE=0 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_SKBT_DASHEDLINE_ROV_ps_5_0 /D __RENDERING_MODE=1 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_SKBT_MULTITEXTMAPPING_ROV_ps_5_0 /D __RENDERING_MODE=2 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_SKBT_TEXTMAPPING_ROV_ps_5_0 /D __RENDERING_MODE=3 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_SKBT_TEXTUREIMGMAP_ROV_ps_5_0 /D __RENDERING_MODE=4 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0

/////////////////////////////////
// Dynamic KB (deprecated)
fxc /E OIT_RESOLVE /T cs_5_0 ./hlsl/kbuf/KbufResolve.hlsl /Fo ./shader_compiled_objs/OIT_DKBZ_RESOLVE_cs_5_0 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=1 
// deprecated //
fxc /E OIT_PRESET /T cs_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_SINGLE_LAYER_TO_DKBTZ_cs_5_0 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D SILHOUETTE_EDGE=1 /D DO_NOT_USE_DISCARD=1 /D DYNAMIC_K_MODE=1 /D FURTHER_OVERLAPS=0

fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_DKBTZ_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=0 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=1 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_DKBTZ_DASHEDLINE_ps_5_0 /D __RENDERING_MODE=1 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=0 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=1 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_DKBTZ_MULTITEXTMAPPING_ps_5_0 /D __RENDERING_MODE=2 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=0 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=1 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_DKBTZ_TEXTMAPPING_ps_5_0 /D __RENDERING_MODE=3 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=0 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=1 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_DKBTZ_TEXTUREIMGMAP_ps_5_0 /D __RENDERING_MODE=4 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=0 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=1 /D FURTHER_OVERLAPS=0

fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_DKBTZ_PHONGBLINN_ROV_ps_5_0 /D __RENDERING_MODE=0 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=1 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_DKBTZ_DASHEDLINE_ROV_ps_5_0 /D __RENDERING_MODE=1 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=1 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_DKBTZ_MULTITEXTMAPPING_ROV_ps_5_0 /D __RENDERING_MODE=2 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=1 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_DKBTZ_TEXTMAPPING_ROV_ps_5_0 /D __RENDERING_MODE=3 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=1 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_DKBTZ_TEXTUREIMGMAP_ROV_ps_5_0 /D __RENDERING_MODE=4 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=1 /D FURTHER_OVERLAPS=0

fxc /E OIT_RESOLVE /T cs_5_0 ./hlsl/kbuf/KbufResolve.hlsl /Fo ./shader_compiled_objs/OIT_DKB_RESOLVE_cs_5_0 /D FRAG_MERGING=0 /D DYNAMIC_K_MODE=1 
fxc /E OIT_PRESET /T cs_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_SINGLE_LAYER_TO_DKBT_cs_5_0 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D SILHOUETTE_EDGE=1 /D PIXEL_SYNCH=0 /D DO_NOT_USE_DISCARD=1 /D DYNAMIC_K_MODE=1 /D FURTHER_OVERLAPS=0

fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_DKBT_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=0 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=1 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_DKBT_DASHEDLINE_ps_5_0 /D __RENDERING_MODE=1 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=0 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=1 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_DKBT_MULTITEXTMAPPING_ps_5_0 /D __RENDERING_MODE=2 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=0 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=1 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_DKBT_TEXTMAPPING_ps_5_0 /D __RENDERING_MODE=3 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=0 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=1 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_DKBT_TEXTUREIMGMAP_ps_5_0 /D __RENDERING_MODE=4 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=0 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=1 /D FURTHER_OVERLAPS=0

fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_DKBT_PHONGBLINN_ROV_ps_5_0 /D __RENDERING_MODE=0 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=1 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_DKBT_DASHEDLINE_ROV_ps_5_0 /D __RENDERING_MODE=1 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=1 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_DKBT_MULTITEXTMAPPING_ROV_ps_5_0 /D __RENDERING_MODE=2 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=1 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_DKBT_TEXTMAPPING_ROV_ps_5_0 /D __RENDERING_MODE=3 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=1 /D FURTHER_OVERLAPS=0
fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_FILL_DKBT_TEXTUREIMGMAP_ROV_ps_5_0 /D __RENDERING_MODE=4 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D PIXEL_SYNCH=1 /D USE_ROV=1 /D STRICT_LOCKED=1 /D DYNAMIC_K_MODE=1 /D FURTHER_OVERLAPS=0
////////////////////////////////////

fxc /E KB_SSAO /T cs_5_0 ./hlsl/ssao/SSAO.hlsl /Fo ./shader_compiled_objs/KB_SSAO_FM_cs_5_0 /D HBAO=1 /D FRAG_MERGING=1 /D APPLY_TRANSPARENCY=1 /D ZT_MODEL=1
fxc /E KB_SSAO_BLUR /T cs_5_0 ./hlsl/ssao/SSAO.hlsl /Fo ./shader_compiled_objs/KB_SSAO_BLUR_FM_cs_5_0 /D FRAG_MERGING=1
fxc /E KB_TO_TEXTURE /T cs_5_0 ./hlsl/ssao/SSAO.hlsl /Fo ./shader_compiled_objs/KB_TO_TEXTURE_FM_cs_5_0 /D FRAG_MERGING=1

fxc /E KB_SSAO /T cs_5_0 ./hlsl/ssao/SSAO.hlsl /Fo ./shader_compiled_objs/KB_SSAO_cs_5_0 /D HBAO=1 /D FRAG_MERGING=0 /D APPLY_TRANSPARENCY=1 /D ZT_MODEL=0
fxc /E KB_SSAO_BLUR /T cs_5_0 ./hlsl/ssao/SSAO.hlsl /Fo ./shader_compiled_objs/KB_SSAO_BLUR_cs_5_0 /D FRAG_MERGING=0
fxc /E KB_TO_TEXTURE /T cs_5_0 ./hlsl/ssao/SSAO.hlsl /Fo ./shader_compiled_objs/KB_TO_TEXTURE_cs_5_0 /D FRAG_MERGING=0

fxc /E KB_TO_MINMAXDEPTHTEXTURE /T cs_5_0 "./hlsl/layered dof/SSDOF.hlsl" /Fo ./shader_compiled_objs/KB_MINMAXTEXTURE_FM_cs_5_0 /D FRAG_MERGING=1 /D ZT_MODEL=0
fxc /E KB_TO_MINMAXDEPTH_NBUF /T cs_5_0 "./hlsl/layered dof/SSDOF.hlsl" /Fo ./shader_compiled_objs/KB_MINMAX_NBUF_FM_cs_5_0 
fxc /E KB_SSDOF_RT /T cs_5_0 "./hlsl/layered dof/SSDOF.hlsl" /Fo ./shader_compiled_objs/KB_SSDOF_RT_FM_cs_5_0 /D FRAG_MERGING=1 /D ZT_MODEL=0 /D EARLY_ENTIRE_LAYERS_CULLING=1 /D RAY_LAYER_CULLING=1

fxc /E KB_TO_MINMAXDEPTHTEXTURE /T cs_5_0 "./hlsl/layered dof/SSDOF.hlsl" /Fo ./shader_compiled_objs/KB_MINMAXTEXTURE_cs_5_0 /D FRAG_MERGING=0 /D ZT_MODEL=0
fxc /E KB_TO_MINMAXDEPTH_NBUF /T cs_5_0 "./hlsl/layered dof/SSDOF.hlsl" /Fo ./shader_compiled_objs/KB_MINMAX_NBUF_cs_5_0 
fxc /E KB_SSDOF_RT /T cs_5_0 "./hlsl/layered dof/SSDOF.hlsl" /Fo ./shader_compiled_objs/KB_SSDOF_RT_cs_5_0 /D FRAG_MERGING=0 /D ZT_MODEL=0 /D EARLY_ENTIRE_LAYERS_CULLING=1 /D RAY_LAYER_CULLING=1

fxc /E FillHistogram /T cs_5_0 ./hlsl/kbuf/KplusB.hlsl /Fo ./shader_compiled_objs/SR_FillHistogram_cs_5_0
fxc /E CreateOffsetTableKpB /T cs_5_0 ./hlsl/kbuf/KplusB.hlsl /Fo ./shader_compiled_objs/SR_CreateOffsetTableKpB_cs_5_0

//fxc /E DFB_PRESET /T cs_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/SR_SINGLE_LAYER_TO_DFB_cs_5_0 
fxc /E SortAndRenderCS /T cs_5_0 ./hlsl/dx_abuf/DxABuf_Sort.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_SORT2SENDER_cs_5_0 /D DX_11_STYLE=0 /D FRAG_MERGING=0
fxc /E SortAndRenderCS /T cs_5_0 ./hlsl/dx_abuf/DxABuf_Sort.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_SORT2SENDER_SFM_cs_5_0 /D DX_11_STYLE=0 /D FRAG_MERGING=1
fxc /E OIT_A_BUFFER_CNF_FRAGS /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_FRAGCOUNTER_ps_5_0 /D DX_11_STYLE=0 
fxc /E OIT_A_BUFFER_CNF_FRAGS /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_FRAGCOUNTER_MTT_ps_5_0 /D DX_11_STYLE=0 /D __RENDERING_MODE=2
fxc /E OIT_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 /D DX_11_STYLE=0
fxc /E OIT_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_DASHEDLINE_ps_5_0 /D __RENDERING_MODE=1 /D DX_11_STYLE=0
fxc /E OIT_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_MULTITEXTMAPPING_ps_5_0 /D __RENDERING_MODE=2 /D DX_11_STYLE=0
fxc /E OIT_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_TEXTMAPPING_ps_5_0 /D __RENDERING_MODE=3 /D DX_11_STYLE=0
fxc /E OIT_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_TEXTUREIMGMAP_ps_5_0 /D __RENDERING_MODE=4 /D DX_11_STYLE=0
fxc /E OIT_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_VOLUMEMAP_ps_5_0 /D __RENDERING_MODE=5 /D DX_11_STYLE=0
fxc /E CreatePrefixSum_Pass0_CS /T cs_5_0 ./hlsl/dx_abuf/DxABuf_Sort.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_PREFIX_0_cs_5_0 /D DX_11_STYLE=1
fxc /E CreatePrefixSum_Pass1_CS /T cs_5_0 ./hlsl/dx_abuf/DxABuf_Sort.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_PREFIX_1_cs_5_0 /D DX_11_STYLE=1
fxc /E CreateOffsetTable_CS /T cs_5_0 ./hlsl/dx_abuf/DxABuf_Sort.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_OffsetTable_cs_5_0

fxc /E PICKING_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/PICKING_ABUFFER_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 /D DX_11_STYLE=0
fxc /E PICKING_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/PICKING_ABUFFER_DASHEDLINE_ps_5_0 /D __RENDERING_MODE=1 /D DX_11_STYLE=0
fxc /E PICKING_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/PICKING_ABUFFER_MULTITEXTMAPPING_ps_5_0 /D __RENDERING_MODE=2 /D DX_11_STYLE=0
fxc /E PICKING_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/PICKING_ABUFFER_TEXTMAPPING_ps_5_0 /D __RENDERING_MODE=3 /D DX_11_STYLE=0
fxc /E PICKING_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/PICKING_ABUFFER_TEXTUREIMGMAP_ps_5_0 /D __RENDERING_MODE=4 /D DX_11_STYLE=0

fxc /E Moment_GeneratePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_GEN_ps_5_0 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D MOMENT_GENERATION=1 /D NUM_MOMENTS=8 /D USE_ROV=0 /D STRICT_LOCKED=1 
fxc /E Moment_GeneratePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_GEN_MTT_ps_5_0 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D MOMENT_GENERATION=1 /D NUM_MOMENTS=8 /D __RENDERING_MODE=2 /D USE_ROV=0 /D STRICT_LOCKED=1 
fxc /E Moment_GeneratePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_GEN_TEXT_ps_5_0 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D MOMENT_GENERATION=1 /D NUM_MOMENTS=8 /D __RENDERING_MODE=3 /D USE_ROV=0 /D STRICT_LOCKED=1 
fxc /E Moment_ResolvePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_OIT_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D NUM_MOMENTS=8 /D USE_ROV=0 /D STRICT_LOCKED=1 
fxc /E Moment_ResolvePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_OIT_DASHEDLINE_ps_5_0 /D __RENDERING_MODE=1 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D NUM_MOMENTS=8 /D USE_ROV=0 /D STRICT_LOCKED=1 
fxc /E Moment_ResolvePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_OIT_MULTITEXTMAPPING_ps_5_0 /D __RENDERING_MODE=2 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D NUM_MOMENTS=8 /D USE_ROV=0 /D STRICT_LOCKED=1 
fxc /E Moment_ResolvePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_OIT_TEXTMAPPING_ps_5_0 /D __RENDERING_MODE=3 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D NUM_MOMENTS=8 /D USE_ROV=0 /D STRICT_LOCKED=1 
fxc /E Moment_ResolvePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_OIT_TEXTUREIMGMAP_ps_5_0 /D __RENDERING_MODE=4 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D NUM_MOMENTS=8 /D USE_ROV=0 /D STRICT_LOCKED=1 

fxc /E Moment_GeneratePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_GEN_ROV_ps_5_0 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D MOMENT_GENERATION=1 /D NUM_MOMENTS=8 /D USE_ROV=1 /D STRICT_LOCKED=1 
fxc /E Moment_GeneratePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_GEN_MTT_ROV_ps_5_0 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D MOMENT_GENERATION=1 /D NUM_MOMENTS=8 /D __RENDERING_MODE=2 /D USE_ROV=1 /D STRICT_LOCKED=1 
fxc /E Moment_GeneratePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_GEN_TEXT_ROV_ps_5_0 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D MOMENT_GENERATION=1 /D NUM_MOMENTS=8 /D __RENDERING_MODE=3 /D USE_ROV=1 /D STRICT_LOCKED=1 
fxc /E Moment_ResolvePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_OIT_PHONGBLINN_ROV_ps_5_0 /D __RENDERING_MODE=0 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D NUM_MOMENTS=8 /D USE_ROV=1 /D STRICT_LOCKED=1 
fxc /E Moment_ResolvePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_OIT_DASHEDLINE_ROV_ps_5_0 /D __RENDERING_MODE=1 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D NUM_MOMENTS=8 /D USE_ROV=1 /D STRICT_LOCKED=1 
fxc /E Moment_ResolvePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_OIT_MULTITEXTMAPPING_ROV_ps_5_0 /D __RENDERING_MODE=2 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D NUM_MOMENTS=8 /D USE_ROV=1 /D STRICT_LOCKED=1 
fxc /E Moment_ResolvePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_OIT_TEXTMAPPING_ROV_ps_5_0 /D __RENDERING_MODE=3 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D NUM_MOMENTS=8 /D USE_ROV=1 /D STRICT_LOCKED=1 
fxc /E Moment_ResolvePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_OIT_TEXTUREIMGMAP_ROV_ps_5_0 /D __RENDERING_MODE=4 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D NUM_MOMENTS=8 /D USE_ROV=1 /D STRICT_LOCKED=1 

fxc /E GS_Surfels /T gs_5_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs/GS_SurfelPoints_gs_5_0
fxc /E GS_ThickPoints_Pixels /T gs_5_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs/GS_ThickPoints_gs_5_0
fxc /E GS_ThickLines /T gs_5_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs/GS_ThickLines_gs_5_0

fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_RAYMAX_cs_5_0 /D RAYMODE=1 /D OTF_MASK=0 /D MAX_LAYERS=8 /D FRAG_MERGING=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_RAYMIN_cs_5_0 /D RAYMODE=2 /D OTF_MASK=0 /D MAX_LAYERS=8 /D FRAG_MERGING=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_RAYSUM_cs_5_0 /D RAYMODE=3 /D OTF_MASK=0 /D MAX_LAYERS=8 /D FRAG_MERGING=1

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

fxc /E VR_SURFACE /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_SURFACE_cs_5_0 /D OTF_MASK=0
fxc /E FillDither /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/FillDither_cs_5_0 

fxc /E CurvedSlicer /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/PanoVR_RAYMAX_cs_5_0 /D RAYMODE=1 /D OTF_MASK=0 /D MAX_LAYERS=8 /D FRAG_MERGING=1
fxc /E CurvedSlicer /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/PanoVR_RAYMIN_cs_5_0 /D RAYMODE=2 /D OTF_MASK=0 /D MAX_LAYERS=8 /D FRAG_MERGING=1
fxc /E CurvedSlicer /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/PanoVR_RAYSUM_cs_5_0 /D RAYMODE=3 /D OTF_MASK=0 /D MAX_LAYERS=8 /D FRAG_MERGING=1
fxc /E CurvedSlicer /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/PanoVR_DEFAULT_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=0 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E CurvedSlicer /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/PanoVR_MODULATE_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=2 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E CurvedSlicer /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/PanoVR_MULTIOTF_DEFAULT_cs_5_0 /D RAYMODE=0 /D OTF_MASK=1 /D VR_MODE=0 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E CurvedSlicer /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/PanoVR_MULTIOTF_MODULATE_cs_5_0 /D RAYMODE=0 /D OTF_MASK=1 /D VR_MODE=2 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1

fxc /E ThickSlicePathTracer /T cs_5_0 ./hlsl/PathTracer.hlsl /Fo ./shader_compiled_objs/ThickSlicePathTracer_cs_5_0 /D FRAG_MERGING=1 /D CURVEDPLANE=0
fxc /E ThickSlicePathTracer /T cs_5_0 ./hlsl/PathTracer.hlsl /Fo ./shader_compiled_objs/CurvedThickSlicePathTracer_cs_5_0 /D FRAG_MERGING=1 /D CURVEDPLANE=1

fxc /E ThickSlicePathTracer /T cs_5_0 ./hlsl/PathTracer.hlsl /Fo ./shader_compiled_objs/PickingThickSlice_cs_5_0 /D FRAG_MERGING=1 /D CURVEDPLANE=0 /D PICKING=1
fxc /E ThickSlicePathTracer /T cs_5_0 ./hlsl/PathTracer.hlsl /Fo ./shader_compiled_objs/PickingCurvedThickSlice_cs_5_0 /D FRAG_MERGING=1 /D CURVEDPLANE=1 /D PICKING=1

fxc /E Outline2D /T cs_5_0 ./hlsl/PathTracer.hlsl /Fo ./shader_compiled_objs/SliceOutline_cs_5_0 
/////////////////
// deprecated
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_DEFAULT_DKBZ_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=0 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_OPAQUE_DKBZ_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=1 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_CONTEXT_DKBZ_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=2 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_MULTIOTF_DKBZ_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=2 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=1

fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_DEFAULT_DFB_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=0 /D FRAG_MERGING=0 /D DYNAMIC_K_MODE=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_OPAQUE_DFB_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=1 /D FRAG_MERGING=0 /D DYNAMIC_K_MODE=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_CONTEXT_DFB_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=2 /D FRAG_MERGING=0 /D DYNAMIC_K_MODE=1
/////////////////////