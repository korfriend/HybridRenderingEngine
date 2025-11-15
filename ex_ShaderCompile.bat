fxc /E CommonVS_PNT /T vs_5_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs/SR_OIT_PNT_vs_5_0 
fxc /E BasicShader4 /T ps_5_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs/SR_BASIC_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 
fxc /E OIT_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 /D DX_11_STYLE=0

fxc /E CommonVS_P /T vs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_P_vs_4_0
fxc /E CommonVS_PN /T vs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_PN_vs_4_0
fxc /E CommonVS_PT /T vs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_PT_vs_4_0
fxc /E CommonVS_PNT /T vs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_PNT_vs_4_0 
fxc /E CommonVS_PTTT /T vs_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_OIT_PTTT_vs_4_0 

fxc /E WRITE_DEPTHZ /T ps_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/WRITE_DEPTH_ps_4_0

fxc /E SINGLE_LAYER /T ps_4_0 ./hlsl/Sr_Common.hlsl /Fo ./shader_compiled_objs_4_0/SR_SINGLE_LAYER_ps_4_0

fxc /E BasicShader4 /T ps_4_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs_4_0/SR_BASIC_PHONGBLINN_ps_4_0 /D __RENDERING_MODE=0 /D DX10_0=1
fxc /E BasicShader4 /T ps_4_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs_4_0/SR_BASIC_DASHEDLINE_ps_4_0 /D __RENDERING_MODE=1 /D DX10_0=1
fxc /E BasicShader4 /T ps_4_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs_4_0/SR_BASIC_MULTITEXTMAPPING_ps_4_0 /D __RENDERING_MODE=2 /D DX10_0=1
fxc /E BasicShader4 /T ps_4_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs_4_0/SR_BASIC_TEXTMAPPING_ps_4_0 /D __RENDERING_MODE=3 /D DX10_0=1
fxc /E BasicShader4 /T ps_4_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs_4_0/SR_BASIC_TEXTUREIMGMAP_ps_4_0 /D __RENDERING_MODE=4 /D DX10_0=1
fxc /E BasicShader4 /T ps_4_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs_4_0/SR_BASIC_VOLUMEMAP_ps_4_0 /D __RENDERING_MODE=5 /D DX10_0=1
fxc /E BasicShader4 /T ps_4_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs_4_0/SR_BASIC_VOLUME_DIST_MAP_ps_4_0 /D __RENDERING_MODE=6 /D DX10_0=1
