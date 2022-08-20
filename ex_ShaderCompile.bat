fxc /E CurvedSlicer /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/PanoVR_RAYMAX_cs_5_0 /D RAYMODE=1 /D OTF_MASK=0 /D MAX_LAYERS=8 /D FRAG_MERGING=1
fxc /E CurvedSlicer /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/PanoVR_RAYMIN_cs_5_0 /D RAYMODE=2 /D OTF_MASK=0 /D MAX_LAYERS=8 /D FRAG_MERGING=1
fxc /E CurvedSlicer /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/PanoVR_RAYSUM_cs_5_0 /D RAYMODE=3 /D OTF_MASK=0 /D MAX_LAYERS=8 /D FRAG_MERGING=1
fxc /E CurvedSlicer /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/PanoVR_DEFAULT_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=0 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E CurvedSlicer /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/PanoVR_MODULATE_cs_5_0 /D RAYMODE=0 /D OTF_MASK=0 /D VR_MODE=2 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E CurvedSlicer /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/PanoVR_MULTIOTF_DEFAULT_cs_5_0 /D RAYMODE=0 /D OTF_MASK=1 /D VR_MODE=0 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1
fxc /E CurvedSlicer /T cs_5_0 ./hlsl/dvr/DvrCS.hlsl /Fo ./shader_compiled_objs/PanoVR_MULTIOTF_MODULATE_cs_5_0 /D RAYMODE=0 /D OTF_MASK=1 /D VR_MODE=2 /D FRAG_MERGING=1 /D DYNAMIC_K_MODE=0 /D LINEAR_MODE=1

fxc /E ThickSlicePathTracer /T cs_5_0 ./hlsl/PathTracer.hlsl /Fo ./shader_compiled_objs/ThickSlicePathTracer_cs_5_0 /D FRAG_MERGING=1 /D CURVEDPLANE=0
fxc /E ThickSlicePathTracer /T cs_5_0 ./hlsl/PathTracer.hlsl /Fo ./shader_compiled_objs/CurvedThickSlicePathTracer_cs_5_0 /D FRAG_MERGING=1 /D CURVEDPLANE=1
fxc /E Outline2D /T cs_5_0 ./hlsl/PathTracer.hlsl /Fo ./shader_compiled_objs/SliceOutline_cs_5_0 

fxc /E ThickSlicePathTracer /T cs_5_0 ./hlsl/PathTracer.hlsl /Fo ./shader_compiled_objs/PickingThickSlice_cs_5_0 /D FRAG_MERGING=1 /D CURVEDPLANE=0 /D PICKING=1
fxc /E ThickSlicePathTracer /T cs_5_0 ./hlsl/PathTracer.hlsl /Fo ./shader_compiled_objs/PickingCurvedThickSlice_cs_5_0 /D FRAG_MERGING=1 /D CURVEDPLANE=1 /D PICKING=1
