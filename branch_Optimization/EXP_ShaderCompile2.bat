fxc /E CommonVS_OnlyPoint /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_P_vs_4_0
fxc /E CommonVS_PN /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_PN_vs_4_0 
fxc /E CommonVS_PT /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_PT_vs_4_0
fxc /E CommonVS_PNT /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_PNT_vs_4_0 
fxc /E CommonVS_PNT /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_PNT_DEV_vs_4_0 /D VS_POINTDEVIATIRON
fxc /E ProxyVS_Point /T vs_4_0 MergeAndSuperimpose.hlsl /Fo SR_PROJ_PROXY_vs_4_0 /D PROXY_PS

fxc /E CommonVS_OnlyPoint /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_P_MVB_vs_4_0 /D VS_MINUSVIEWBOX
fxc /E CommonVS_PN /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_PN_MVB_vs_4_0 /D VS_MINUSVIEWBOX
fxc /E CommonVS_PT /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_PT_MVB_vs_4_0 /D VS_MINUSVIEWBOX
fxc /E CommonVS_PNT /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_PNT_MVB_vs_4_0 /D VS_MINUSVIEWBOX
fxc /E CommonVS_PNT /T vs_4_0 SRShader_Common.hlsl /Fo SR_PROJ_PNT_DEV_MVB_vs_4_0 /D VS_POINTDEVIATIRON /D VS_MINUSVIEWBOX


fxc /E VRCS_GenShadowMap /T cs_4_0 VRShader_Shadow.hlsl /Fo DVR_VOLUME_GEN_SURFACE_SHADOW_cs_4_0 /D RM_SURFACEEFFECT
fxc /E VRCS_Common /T cs_4_0 VRShader_Common.hlsl /Fo DVR_VOLUME_IND_MASK_SURFACEEFFECT_cs_4_0 /D RM_SURFACEEFFECT /D VR_LAYER_OUT /D OTF_MASK
fxc /E VRCS_Common /T cs_4_0 VRShader_Common.hlsl /Fo DVR_VOLUME_IND_MASK_SURFACEEFFECT_SHADOW_cs_4_0 /D RM_SURFACEEFFECT /D RM_SHADOW /D VR_LAYER_OUT /D OTF_MASK
fxc /E VRCS_Common /T cs_4_0 VRShader_Common.hlsl /Fo DVR_VOLUME_IND_MASK_SURFACECURVATURE_cs_4_0 /D RM_SURFACEEFFECT /D SURFACE_MODE_CURVATURE /D VR_LAYER_OUT /D OTF_MASK
fxc /E VRCS_GenShadowMap /T cs_4_0 VRShader_Shadow.hlsl /Fo DVR_VOLUME_GEN_MASK_SURFACE_SHADOW_cs_4_0 /D RM_SURFACEEFFECT /D OTF_MASK