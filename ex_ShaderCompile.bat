fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_MASKVIS_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=3 /D FRAG_MERGING=0 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_MASKVIS_FM_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=3 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
