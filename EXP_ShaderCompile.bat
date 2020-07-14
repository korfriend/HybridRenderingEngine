fxc /E CommonVS_OnlyPoint /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_P_vs_4_0
fxc /E CommonVS_PN /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_PN_vs_4_0 
fxc /E CommonVS_PT /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_PT_vs_4_0
fxc /E CommonVS_PNT /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_PNT_vs_4_0 
fxc /E CommonVS_PTTT /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_PTTT_vs_4_0 
fxc /E CommonVS_PNT /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_PNT_DEV_vs_4_0 /D VS_POINTDEVIATIRON=1
fxc /E ProxyVS_Point /T vs_4_0 MergeMultipleLayers.hlsl /Fo SR_PROJ_PROXY_vs_4_0 /D PROXY_PS=1

fxc /E CommonVS_OnlyPoint /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_P_MVB_vs_4_0 /D VS_MINUSVIEWBOX=1
fxc /E CommonVS_PN /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_PN_MVB_vs_4_0 /D VS_MINUSVIEWBOX=1
fxc /E CommonVS_PT /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_PT_MVB_vs_4_0 /D VS_MINUSVIEWBOX=1
fxc /E CommonVS_PNT /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_PNT_MVB_vs_4_0 /D VS_MINUSVIEWBOX=1
fxc /E CommonVS_PNT /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_PNT_DEV_MVB_vs_4_0 /D VS_POINTDEVIATIRON=1 /D VS_MINUSVIEWBOX=1

fxc /E CommonPS_TEXCOORD1 /T ps_4_0 SRShader_Common.hlsl /Fo SR_MAPPING_TEX1_ps_4_0
fxc /E CommonPS_PhongShading /T ps_4_0 SRShader_Common.hlsl /Fo SR_PHONG_TEX1COLOR_ps_4_0 
fxc /E CommonPS_PhongShading /T ps_4_0 SRShader_Common.hlsl /Fo SR_PHONG_GLOBALCOLOR_ps_4_0 /D PS_COLOR_GLOBAL=1
fxc /E CommonPS_PhongShading /T ps_4_0 SRShader_Common.hlsl /Fo SR_PHONG_GLOBALCOLOR_MAXSHADING_ps_4_0 /D PS_COLOR_GLOBAL=1
fxc /E CommonPS_PhongShading /T ps_4_0 SRShader_Common.hlsl /Fo SR_PHONG_GLOBALCOLOR_MARKERSONSURFACE_ps_4_0 /D PS_COLOR_GLOBAL=1 /D MARKERS_ON_SURFACE=1
fxc /E CommonPS_PhongShading /T ps_4_0 SRShader_Common.hlsl /Fo SR_PHONG_GLOBALCOLOR_MAXSHADING_MARKERSONSURFACE_ps_4_0 /D PS_COLOR_GLOBAL=1 /D MARKERS_ON_SURFACE=1
fxc /E CommonPS_Superimpose /T ps_4_0 SRShader_Common.hlsl /Fo SR_VOLUME_SUPERIMPOSE_ps_4_0
fxc /E CommonPS_Deviation /T ps_4_0 SRShader_Common.hlsl /Fo SR_VOLUME_DEVIATION_ps_4_0
fxc /E CommonPS_CMM_LINE /T ps_4_0 SRShader_Common.hlsl /Fo SR_CMM_LINE_ps_4_0
fxc /E CommonPS_CMM_TEXT /T ps_4_0 SRShader_Common.hlsl /Fo SR_CMM_TEXT_ps_4_0
fxc /E CommonPS_CMM_MultiTEXTs /T ps_4_0 SRShader_Common.hlsl /Fo SR_CMM_MULTI_TEXTS_ps_4_0
fxc /E CommonPS_ImageStackMapping /T ps_4_0 SRShader_Common.hlsl /Fo SR_IMAGESTACK_MAP_ps_4_0
fxc /E CommonPS_AirwayColorMapping /T ps_4_0 SRShader_Common.hlsl /Fo SR_AIRWAYANALYSIS_ps_4_0