// FEATURE LEVEL 10_0 //
fxc /E CommonVS_OnlyPoint /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_P_vs_4_0
fxc /E CommonVS_PN /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_PN_vs_4_0 
fxc /E CommonVS_PT /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_PT_vs_4_0
fxc /E CommonVS_PNT /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_PNT_vs_4_0 
fxc /E CommonVS_PTTT /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_PTTT_vs_4_0 
fxc /E CommonVS_PNT /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_PNT_DEV_vs_4_0 /D VS_POINTDEVIATIRON
fxc /E ProxyVS_Point /T vs_4_0 MergeMultipleLayers.hlsl /Fo SR_PROJ_PROXY_vs_4_0 /D PROXY_PS

fxc /E CommonVS_OnlyPoint /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_P_MVB_vs_4_0 /D VS_MINUSVIEWBOX
fxc /E CommonVS_PN /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_PN_MVB_vs_4_0 /D VS_MINUSVIEWBOX
fxc /E CommonVS_PT /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_PT_MVB_vs_4_0 /D VS_MINUSVIEWBOX
fxc /E CommonVS_PNT /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_PNT_MVB_vs_4_0 /D VS_MINUSVIEWBOX
fxc /E CommonVS_PNT /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_PNT_DEV_MVB_vs_4_0 /D VS_POINTDEVIATIRON /D VS_MINUSVIEWBOX

fxc /E CommonVS_OnlyPoint /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_P_FBIASZ_vs_4_0 /D VS_FORWARDBIAS
fxc /E CommonVS_PN /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_PN_FBIASZ_vs_4_0 /D VS_FORWARDBIAS
fxc /E CommonVS_PT /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_PT_FBIASZ_vs_4_0 /D VS_FORWARDBIAS
fxc /E CommonVS_PNT /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_PNT_FBIASZ_vs_4_0 /D VS_FORWARDBIAS
fxc /E CommonVS_PNT /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_PNT_DEV_FBIASZ_vs_4_0 /D VS_POINTDEVIATIRON /D VS_FORWARDBIAS

fxc /E CommonPS_TEXCOORD1 /T ps_4_0 SRShader_Common.hlsl /Fo SR_MAPPING_TEX1_ps_4_0
fxc /E CommonPS_PhongShading /T ps_4_0 SRShader_Common.hlsl /Fo SR_PHONG_TEX1COLOR_ps_4_0 /D PS_MAX_IILUMINATION
fxc /E CommonPS_PhongShading /T ps_4_0 SRShader_Common.hlsl /Fo SR_PHONG_GLOBALCOLOR_ps_4_0 /D PS_COLOR_GLOBAL
fxc /E CommonPS_PhongShading /T ps_4_0 SRShader_Common.hlsl /Fo SR_PHONG_GLOBALCOLOR_MAXSHADING_ps_4_0 /D PS_MAX_IILUMINATION /D PS_COLOR_GLOBAL
fxc /E CommonPS_PhongShading /T ps_4_0 SRShader_Common.hlsl /Fo SR_PHONG_GLOBALCOLOR_MARKERSONSURFACE_ps_4_0 /D PS_COLOR_GLOBAL /D MARKERS_ON_SURFACE=1
fxc /E CommonPS_PhongShading /T ps_4_0 SRShader_Common.hlsl /Fo SR_PHONG_GLOBALCOLOR_MAXSHADING_MARKERSONSURFACE_ps_4_0 /D PS_MAX_IILUMINATION /D PS_COLOR_GLOBAL /D MARKERS_ON_SURFACE=1
fxc /E CommonPS_Superimpose /T ps_4_0 SRShader_Common.hlsl /Fo SR_VOLUME_SUPERIMPOSE_ps_4_0
fxc /E CommonPS_Deviation /T ps_4_0 SRShader_Common.hlsl /Fo SR_VOLUME_DEVIATION_ps_4_0
fxc /E CommonPS_CMM_LINE /T ps_4_0 SRShader_Common.hlsl /Fo SR_CMM_LINE_ps_4_0
fxc /E CommonPS_CMM_TEXT /T ps_4_0 SRShader_Common.hlsl /Fo SR_CMM_TEXT_ps_4_0
fxc /E CommonPS_CMM_MultiTEXTs /T ps_4_0 SRShader_Common.hlsl /Fo SR_CMM_MULTI_TEXTS_ps_4_0
fxc /E CommonPS_ImageStackMapping /T ps_4_0 SRShader_Common.hlsl /Fo SR_IMAGESTACK_MAP_ps_4_0
fxc /E CommonPS_AirwayColorMapping /T ps_4_0 SRShader_Common.hlsl /Fo SR_AIRWAYANALYSIS_ps_4_0

fxc /E GS_ThickPoints /T gs_4_0 SRShader_Common.hlsl /Fo GS_ThickPoints_gs_4_0
fxc /E GS_ThickLines /T gs_4_0 SRShader_Common.hlsl /Fo GS_ThickLines_gs_4_0
fxc /E GS_WireLines /T gs_4_0 SRShader_Common.hlsl /Fo GS_WireLines_gs_4_0

// CS //
fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_DVR_BASIC_DEFAULT_cs_5_0 /D __CS_MODEL=1
fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_DVR_BASIC_DEFAULT_SHADOW_cs_5_0 /D RM_SHADOW=1 /D __CS_MODEL=1
fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_DVR_BASIC_DEFAULT_OTFMASK_cs_5_0 /D OTF_MASK=1 /D __CS_MODEL=1
fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_DVR_BASIC_DEFAULT_OTFMASK_SHADOW_cs_5_0 /D OTF_MASK=1 /D RM_SHADOW=1 /D __CS_MODEL=1
fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_DVR_BASIC_CLIPSEMIOPAQUE_cs_5_0 /D RM_CLIPSEMIOPAQUE=1 /D __CS_MODEL=1
fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_DVR_BASIC_SCULPTMASK=1_cs_5_0 /D SCULPTMASK=1 /D __CS_MODEL=1
fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_DVR_BASIC_MODULATION_cs_5_0 /D RM_MODULATION=1 /D __CS_MODEL=1
fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_DVR_BASIC_MODULATION_OTFMASK_cs_5_0 /D RM_MODULATION=1 /D OTF_MASK=1 /D __CS_MODEL=1

fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_DVR_SURFACE_DEFAULT_cs_5_0 /D RM_SURFACEEFFECT=1 /D __CS_MODEL=1
fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_DVR_SURFACE_DEFAULT_SHADOW_cs_5_0 /D RM_SURFACEEFFECT=1 /D RM_SHADOW=1 /D __CS_MODEL=1
fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_DVR_SURFACE_DEFAULT_OTFMASK_cs_5_0 /D RM_SURFACEEFFECT=1 /D OTF_MASK=1 /D __CS_MODEL=1
fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_DVR_SURFACE_DEFAULT_OTFMASK_SHADOW_cs_5_0 /D RM_SURFACEEFFECT=1 /D OTF_MASK=1 /D RM_SHADOW=1 /D __CS_MODEL=1
fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_DVR_SURFACE_CURVATURE_cs_5_0 /D RM_SURFACEEFFECT=1 /D SURFACE_MODE_CURVATURE=1 /D __CS_MODEL=1
fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_DVR_SURFACE_CCF_cs_5_0 /D RM_SURFACEEFFECT=1 /D SAMPLER_VOLUME_CCF=1 /D __CS_MODEL=1
fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_DVR_SURFACE_MARKER_cs_5_0 /D RM_SURFACEEFFECT=1 /D MARKERS_ON_SURFACE=1 /D __CS_MODEL=1

fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_DVR_LAYERS_DEFAULT_cs_5_0 /D INPUT_LAYERS=1 /D __CS_MODEL=1
fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_DVR_LAYERS_DEFAULT_SHADOW_cs_5_0 /D INPUT_LAYERS=1 /D RM_SHADOW=1 /D __CS_MODEL=1
fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_DVR_LAYERS_DEFAULT_OTFMASK_cs_5_0 /D INPUT_LAYERS=1 /D OTF_MASK=1 /D RM_SHADOW=1 /D __CS_MODEL=1
fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_DVR_LAYERS_DEFAULT_OTFMASK_SHADOW_cs_5_0 /D INPUT_LAYERS=1 /D OTF_MASK=1 /D RM_SHADOW=1 /D __CS_MODEL=1
fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_DVR_LAYERS_CLIPSEMIOPAQUE_cs_5_0 /D INPUT_LAYERS=1 /D RM_CLIPSEMIOPAQUE=1 /D __CS_MODEL=1
fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_DVR_LAYERS_SCULPTMASK=1_cs_5_0 /D INPUT_LAYERS=1 /D SCULPTMASK=1 /D __CS_MODEL=1
fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_DVR_LAYERS_MODULATION_cs_5_0 /D INPUT_LAYERS=1 /D RM_MODULATION=1 /D __CS_MODEL=1
fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_DVR_LAYERS_MODULATION_OTFMASK_cs_5_0 /D INPUT_LAYERS=1 /D RM_MODULATION=1 /D OTF_MASK=1 /D __CS_MODEL=1

fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_DVR_LAYERS_ADV_DEFAULT_cs_5_0 /D INPUT_LAYERS=1 /D ZSLAB_BLENDING=1 /D __CS_MODEL=1
fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_DVR_LAYERS_ADV_DEFAULT_SHADOW_cs_5_0 /D INPUT_LAYERS=1 /D ZSLAB_BLENDING=1 /D RM_SHADOW=1 /D __CS_MODEL=1
fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_DVR_LAYERS_ADV_DEFAULT_OTFMASK_cs_5_0 /D INPUT_LAYERS=1 /D ZSLAB_BLENDING=1 /D OTF_MASK=1 /D __CS_MODEL=1
fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_DVR_LAYERS_ADV_DEFAULT_OTFMASK_SHADOW_cs_5_0 /D INPUT_LAYERS=1 /D ZSLAB_BLENDING=1 /D OTF_MASK=1 /D RM_SHADOW=1 /D __CS_MODEL=1
fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_DVR_LAYERS_ADV_CLIPSEMIOPAQUE_cs_5_0 /D INPUT_LAYERS=1 /D ZSLAB_BLENDING=1 /D RM_CLIPSEMIOPAQUE=1 /D __CS_MODEL=1
fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_DVR_LAYERS_ADV_SCULPTMASK=1_cs_5_0 /D INPUT_LAYERS=1 /D ZSLAB_BLENDING=1 /D SCULPTMASK=1 /D __CS_MODEL=1
fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_DVR_LAYERS_ADV_MODULATION_cs_5_0 /D INPUT_LAYERS=1 /D ZSLAB_BLENDING=1 /D RM_MODULATION=1 /D __CS_MODEL=1
fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_DVR_LAYERS_ADV_MODULATION_OTFMASK_cs_5_0 /D INPUT_LAYERS=1 /D ZSLAB_BLENDING=1 /D RM_MODULATION=1 /D OTF_MASK=1 /D __CS_MODEL=1

fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_DVR_TEST_cs_5_0 /D BLOCKSHOW /D __CS_MODEL=1

fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_XRAY_MAX_cs_5_0 /D RM_RAYMAX /D __CS_MODEL=1
fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_XRAY_MAX_OTFMASK_cs_5_0 /D RM_RAYMAX /D OTF_MASK=1 /D __CS_MODEL=1
fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_XRAY_MIN_cs_5_0 /D RM_RAYMIN /D __CS_MODEL=1
fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_XRAY_MIN_OTFMASK_cs_5_0 /D RM_RAYMIN /D OTF_MASK=1 /D __CS_MODEL=1
fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_XRAY_SUM_cs_5_0 /D RM_RAYSUM /D __CS_MODEL=1
fxc /E CsDVR_Common /T cs_5_0 VRShader_Common.hlsl /Fo CS_XRAY_SUM_OTFMASK_cs_5_0 /D RM_RAYSUM /D OTF_MASK=1 /D __CS_MODEL=1

fxc /E CsDVR_ShadowMapGeneration /T cs_5_0 VRShader_Shadow.hlsl /Fo CS_DVR_BASIC_DEFAULT_SHADOWMAP_cs_5_0 /D __CS_MODEL=1
fxc /E CsDVR_ShadowMapGeneration /T cs_5_0 VRShader_Shadow.hlsl /Fo CS_DVR_BASIC_OTFMASK_SHADOWMAP_cs_5_0 /D OTF_MASK=1 /D __CS_MODEL=1

fxc /E CsLAYEROUT_TO_RENDEROUT /T cs_5_0 MergeMultipleLayers.hlsl /Fo CS_MERGE_LAYEROUT_TO_RENDEROUT_cs_5_0 /D __CS_MODEL=1
fxc /E CsSROUT_AND_LAYEROUT_TO_LAYEROUT /T cs_5_0 MergeMultipleLayers.hlsl /Fo CS_MERGE_SROUT_AND_LAYEROUT_TO_LAYEROUT_cs_5_0 /D __CS_MODEL=1
fxc /E CsSROUT_AND_LAYEROUT_TO_LAYEROUT /T cs_5_0 MergeMultipleLayers.hlsl /Fo CS_MERGE_ADV_SROUT_AND_LAYEROUT_TO_LAYEROUT_cs_5_0 /D ZSLAB_BLENDING=1 /D __CS_MODEL=1

fxc /E CsOutline /T cs_5_0 MergeMultipleLayers.hlsl /Fo CS_OUTLINE_RENDEROUT_cs_5_0 /D __CS_MODEL=1
