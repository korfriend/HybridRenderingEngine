fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_DEFAULT_FM_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=0 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_OPAQUE_FM_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=1 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_CONTEXT_FM_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=2 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_MULTIOTF_FM_cs_5_0 /D RAYMODE=0 /D OTF_MASK=1 /D VR_MODE=0 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_MULTIOTF_CONTEXT_FM_cs_5_0 /D RAYMODE=0 /D OTF_MASK=1 /D VR_MODE=2 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_MASKVIS_FM_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=3 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1

fxc /E VR_SURFACE /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_SURFACE_cs_5_0 /D OTF_MASK=0
fxc /E FillDither /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/FillDither_cs_5_0 
