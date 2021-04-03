fxc /E KB_TO_MINMAXDEPTHTEXTURE /T cs_5_0 "./hlsl/layered dof/SSDOF.hlsl" /Fo ./shader_compiled_objs/KB_MINMAXTEXTURE_cs_5_0 /D FRAG_MERGING=1 /D ZT_MODEL=0
fxc /E KB_TO_MINMAXDEPTH_NBUF /T cs_5_0 "./hlsl/layered dof/SSDOF.hlsl" /Fo ./shader_compiled_objs/KB_MINMAX_NBUF_cs_5_0 
fxc /E KB_SSDOF_RT /T cs_5_0 "./hlsl/layered dof/SSDOF.hlsl" /Fo ./shader_compiled_objs/KB_SSDOF_RT_cs_5_0 /D FRAG_MERGING=1 /D ZT_MODEL=0 /D EARLY_ENTIRE_LAYERS_CULLING=1 /D RAY_LAYER_CULLING=1
