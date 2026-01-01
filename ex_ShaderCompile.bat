fxc /E BasicShader4 /T ps_5_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs/SR_BASIC_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 
fxc /E BasicShader4 /T ps_5_0 ./hlsl/shader4/BasicShader.hlsl /Fo ./shader_compiled_objs/SR_BASIC_PHONGBLINN_PAINTER_ps_5_0 /D __RENDERING_MODE=0 /D __PAINTER_UV=1
fxc /E OIT_A_BUFFER_FILL /T ps_5_0 ./hlsl/dx_abuf/DxABuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_ABUFFER_PHONGBLINN_PAINTER_ps_5_0 /D __RENDERING_MODE=0 /D DX_11_STYLE=0 /D __PAINTER_UV=1

fxc /E VS_Brush /T vs_5_0 ./hlsl/meshpainter/MeshPaintShader.hlsl /Fo ./shader_compiled_objs/MP_Brush_vs_5_0 
fxc /E VS_FullscreenQuad /T vs_5_0 ./hlsl/meshpainter/MeshPaintShader.hlsl /Fo ./shader_compiled_objs/MP_FullscreenQuad_vs_5_0
fxc /E PS_BrushStroke /T ps_5_0 ./hlsl/meshpainter/MeshPaintShader.hlsl /Fo ./shader_compiled_objs/MP_BrushStroke_ps_5_0
fxc /E PS_Dilate /T ps_5_0 ./hlsl/meshpainter/MeshPaintShader.hlsl /Fo ./shader_compiled_objs/MP_Dilate_ps_5_0

