fxc /E OIT_RESOLVE /T cs_5_0 ./hlsl/kbuf/KbufResolve.hlsl /Fo ./shader_compiled_objs/OIT_SKBZ_RESOLVE_cs_5_0 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0
fxc /E OIT_RESOLVE /T cs_5_0 ./hlsl/kbuf/KbufResolve.hlsl /Fo ./shader_compiled_objs/OIT_DKBZ_RESOLVE_cs_5_0 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=1 
fxc /E OIT_RESOLVE /T cs_5_0 ./hlsl/kbuf/KbufResolve.hlsl /Fo ./shader_compiled_objs/OIT_DKB_RESOLVE_cs_5_0 /D FRAG_MERGING=0 /D DYNAMIC_K_MODE=1 
