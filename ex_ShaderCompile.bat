fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_CONTEXT_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=2 /D FRAG_MERGING=0 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_CONTEXT_FM_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=2 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_MULTIOTF_CONTEXT_FM_cs_5_0 /D RAYMODE=0 /D OTF_MASK=1 /D VR_MODE=2 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E RayCasting /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_SCULPTMASK_CONTEXT_FM_cs_5_0 /D RAYMODE=0 /D SCULPT_MASK=1 /D VR_MODE=2 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E CurvedSlicer /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/PanoVR_MODULATE_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=2 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E CurvedSlicer /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/PanoVR_MULTIOTF_MODULATE_cs_5_0 /D RAYMODE=0 /D OTF_MASK=1 /D VR_MODE=2 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
