fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_DEFAULT_SINGLE_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=0 /D FRAG_MERGING=1 /D LINEAR_MODE=1 /D ONLY_SINGLE_LAYER=1
fxc /E OIT_RESOLVE /T cs_5_0 ./hlsl/kbuf/KbufResolve.hlsl /Fo ./shader_compiled_objs/OIT_DKBZ_RESOLVE_cs_5_0 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=1 
