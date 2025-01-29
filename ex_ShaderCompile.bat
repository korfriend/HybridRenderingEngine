fxc /E ThickSlicePathTracer /T cs_5_0 ./hlsl/RayProcessing.hlsl /Fo ./shader_compiled_objs/ThickSlicePathTracer_GPUBVH_cs_5_0 /D FRAG_MERGING=1 /D CURVEDPLANE=0 /D PATHTR_USE_KBUF=1 /D USE_ROV=0 /D TAIL_HANDLING=1 /D DO_NOT_USE_DISCARD=1
fxc /E ThickSlicePathTracer /T cs_5_0 ./hlsl/RayProcessing.hlsl /Fo ./shader_compiled_objs/CurvedThickSlicePathTracer_GPUBVH_cs_5_0 /D FRAG_MERGING=1 /D CURVEDPLANE=1 /D PATHTR_USE_KBUF=1 /D USE_ROV=0 /D TAIL_HANDLING=1 /D DO_NOT_USE_DISCARD=1
fxc /E ThickSlicePathTracer /T cs_5_0 ./hlsl/RayProcessing.hlsl /Fo ./shader_compiled_objs/PickingThickSlice_GPUBVH_cs_5_0 /D FRAG_MERGING=1 /D CURVEDPLANE=0 /D PICKING=1 /D PATHTR_USE_KBUF=1 
fxc /E ThickSlicePathTracer /T cs_5_0 ./hlsl/RayProcessing.hlsl /Fo ./shader_compiled_objs/PickingCurvedThickSlice_GPUBVH_cs_5_0 /D FRAG_MERGING=1 /D CURVEDPLANE=1 /D PICKING=1 /D PATHTR_USE_KBUF=1 

fxc /E main /T cs_5_0 ./hlsl/BVH/bvh_hierarchyCS.hlsl /Fo ./shader_compiled_objs/BVH_Hierarchy_cs_5_0 
fxc /E main /T cs_5_0 ./hlsl/BVH/bvh_primitivesCS.hlsl /Fo ./shader_compiled_objs/BVH_Primitives_cs_5_0 
fxc /E main /T cs_5_0 ./hlsl/BVH/bvh_propagateaabbCS.hlsl /Fo ./shader_compiled_objs/BVH_Propagateaabb_cs_5_0 

