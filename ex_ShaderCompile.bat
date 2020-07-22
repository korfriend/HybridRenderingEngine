fxc /E DVR /T cs_5_0 ./hlsl/kbuf/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_DEFAULT_cs_5_0 /D RAYMODE=0 /DOTF_MASK=0 /D MAX_LAYERS=8 /D VR_MODE=0 /D GHOST_EFFECT=1
fxc /E DVR /T cs_5_0 ./hlsl/kbuf/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_OPAQUE_cs_5_0 /D RAYMODE=0 /DOTF_MASK=0 /D MAX_LAYERS=8 /D VR_MODE=1 /D GHOST_EFFECT=1
fxc /E DVR /T cs_5_0 ./hlsl/kbuf/DvrCS.hlsl /Fo ./shader_compiled_objs/VR_CONTEXT_cs_5_0 /D RAYMODE=0 /DOTF_MASK=0 /D MAX_LAYERS=8 /D VR_MODE=2 /D GHOST_EFFECT=1


fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_KDEPTH_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 /D ZF_HANDLING=0 /D TAIL_HANDLING=0 /D MAX_LAYERS=8 /D USE_ROV=0
fxc /E OIT_RESOLVE /T cs_5_0 ./hlsl/kbuf/KbufResolve.hlsl /Fo ./shader_compiled_objs/SR_OIT_SORT2RENDER_cs_5_0 /D MAX_LAYERS=8 /D ZF_HANDLING=0

fxc /E Moment_GeneratePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_GEN_ps_5_0 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D MOMENT_GENERATION=1 /D NUM_MOMENTS=8
fxc /E Moment_ResolvePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_OIT_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D NUM_MOMENTS=8

//fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_KDEPTH_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 /D ZF_HANDLING=1 /D TAIL_HANDLING=1
//fxc /E OIT_RESOLVE /T cs_5_0 ./hlsl/kbuf/KbufResolve.hlsl /Fo ./shader_compiled_objs/SR_OIT_SORT2RENDER_cs_5_0 /D MAX_LAYERS=8 /D ZF_HANDLING=1
//fxc /E SINGLE_LAYER /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_SINGLE_LAYER_ps_5_0 /D ZF_HANDLING=1 /D TAIL_HANDLING=1
//fxc /E OIT_PRESET /T cs_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_PRESET_cs_5_0 /D __INTERLOCK=0 /D ZF_HANDLING=1 /D TAIL_HANDLING=1
//
//
//fxc /E OIT_KDEPTH /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_KDEPTH_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 /D ZF_HANDLING=0 /D TAIL_HANDLING=0
//fxc /E OIT_RESOLVE /T cs_5_0 ./hlsl/kbuf/KbufResolve.hlsl /Fo ./shader_compiled_objs/SR_OIT_SORT2RENDER_cs_5_0 /D MAX_LAYERS=8 /D ZF_HANDLING=0
//fxc /E SINGLE_LAYER /T ps_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_SINGLE_LAYER_ps_5_0 /D ZF_HANDLING=0 /D TAIL_HANDLING=0
//fxc /E OIT_PRESET /T cs_5_0 ./hlsl/kbuf/Sr_Kbuf.hlsl /Fo ./shader_compiled_objs/SR_OIT_PRESET_cs_5_0 /D __INTERLOCK=0 /D ZF_HANDLING=0 /D TAIL_HANDLING=0

//fxc /E Moment_GeneratePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_GEN_ps_5_0 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D MOMENT_GENERATION=1 /D NUM_MOMENTS=4
//fxc /E Moment_GeneratePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_GEN_ps_5_0 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D MOMENT_GENERATION=1 /D NUM_MOMENTS=4 /D TRIGONOMETRIC=1
//fxc /E Moment_ResolvePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_OIT_ps_5_0 /D SINGLE_PRECISION=1 /D NUM_MOMENTS=4 /D TRIGONOMETRIC=1
//fxc /E Moment_ResolvePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_OIT_ps_5_0 /D SINGLE_PRECISION=1 /D NUM_MOMENTS=4 


fxc /E Moment_GeneratePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_GEN_ps_5_0 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D MOMENT_GENERATION=1 /D NUM_MOMENTS=8 /D TRIGONOMETRIC=1
fxc /E Moment_ResolvePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_OIT_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 /D SINGLE_PRECISION=1 /D NUM_MOMENTS=8 /D TRIGONOMETRIC=1

fxc /E Moment_GeneratePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_GEN_ps_5_0 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D MOMENT_GENERATION=1 /D NUM_MOMENTS=8
fxc /E Moment_ResolvePass /T ps_5_0 ./hlsl/moment/MomentOIT.hlsli /Fo ./shader_compiled_objs/SR_MOMENT_OIT_PHONGBLINN_ps_5_0 /D __RENDERING_MODE=0 /D SINGLE_PRECISION=1 /D PIXEL_SYNCH=1 /D NUM_MOMENTS=8 