
fxc /E BasicShader4 /T ps_5_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs/SR_BASIC_VOLUME_DIST_MAP_ps_5_0 /D __RENDERING_MODE=6 
fxc /E BasicShader4 /T ps_4_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs_4_0/SR_BASIC_VOLUME_DIST_MAP_ps_4_0 /D __RENDERING_MODE=6 /D DX10_0=1
fxc /E OIT_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_VOLUME_DIST_MAP_ps_5_0 /D __RENDERING_MODE=6 /D DX_11_STYLE=0
