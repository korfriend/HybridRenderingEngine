fxc /E PICKING_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/PICKING_ABUFFER_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 /D DX_11_STYLE=0
fxc /E PICKING_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/PICKING_ABUFFER_DASHEDLINE_ps_5_0 /D __RENDERING_MODE=1 /D DX_11_STYLE=0
fxc /E PICKING_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/PICKING_ABUFFER_MULTITEXTMAPPING_ps_5_0 /D __RENDERING_MODE=2 /D DX_11_STYLE=0
fxc /E PICKING_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/PICKING_ABUFFER_TEXTMAPPING_ps_5_0 /D __RENDERING_MODE=3 /D DX_11_STYLE=0
fxc /E PICKING_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/PICKING_ABUFFER_TEXTUREIMGMAP_ps_5_0 /D __RENDERING_MODE=4 /D DX_11_STYLE=0

fxc /E OIT_PRESET /T cs_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_SINGLE_LAYER_TO_SKBTZ_cs_5_0 /D FRAG_MERGING=1 /D TAIL_HANDLING=1 /D SILHOUETTE_EDGE=1 /D DO_NOT_USE_DISCARD=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0
fxc /E OIT_PRESET /T cs_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_SINGLE_LAYER_TO_SKBT_cs_5_0 /D FRAG_MERGING=0 /D TAIL_HANDLING=1 /D SILHOUETTE_EDGE=1 /D DO_NOT_USE_DISCARD=1 /D DYNAMIC_K_MODE=0 /D FURTHER_OVERLAPS=0


fxc /E SortAndRenderCS /T cs_5_0 ./hlsl/dx_abuf/DxABuf_Sort.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_SORT2SENDER_cs_5_0 /D DX_11_STYLE=0 /D FRAG_MERGING=0
fxc /E SortAndRenderCS /T cs_5_0 ./hlsl/dx_abuf/DxABuf_Sort.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_SORT2SENDER_SFM_cs_5_0 /D DX_11_STYLE=0 /D FRAG_MERGING=1