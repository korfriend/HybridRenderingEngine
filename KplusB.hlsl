#include "../kbuf/Sr_Common.hlsl"

///////////// CS /////////////////
RWByteAddressBuffer deep_k_buf : register(u1);
RWTexture2D<float4> fragment_blendout : register(u2);
RWTexture2D<float> fragment_zdepth : register(u3);
RWBuffer<uint> offsettable_buf : register(u4); // gres_fb_ref_pidx

RWBuffer<uint> histo_buf : register(u10); // 1024 elements

Texture2D<uint> sr_fragment_counter : register(t0);

// set larger thread group?
#define MAX_FRAGS 1024
[numthreads(1, 1, 1)]
void FillHistogram(uint3 nGid : SV_GroupID, uint3 nDTid : SV_DispatchThreadID, uint3 nGTid : SV_GroupThreadID)
{
	if (nDTid.x >= g_cbCamState.rt_width || nDTid.y >= g_cbCamState.rt_height)
	    return;

	int frag_num = (int)sr_fragment_counter[nDTid.xy];
	if (frag_num == 0) return;
	if (frag_num >= MAX_FRAGS)
		frag_num = MAX_FRAGS - 1;

	InterlockedAdd(histo_buf[frag_num], 1);
}