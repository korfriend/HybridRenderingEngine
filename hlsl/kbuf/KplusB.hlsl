#include "../Sr_Common.hlsl"

///////////// CS /////////////////
Texture2D<uint> sr_fragment_counter : register(t0);
RWBuffer<uint> histo_buf : register(u10); // 1024 elements
RWBuffer<uint> offsettable_buf : register(u11); // gres_fb_ref_pidx

// set larger thread group?
#define MAX_FRAGS 1024
[numthreads(GRIDSIZE, GRIDSIZE, 1)]
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

[numthreads(GRIDSIZE, GRIDSIZE, 1)] // later, tuning the gird size
void CreateOffsetTableKpB(uint3 nGid : SV_GroupID, uint3 nDTid : SV_DispatchThreadID, uint3 nGTid : SV_GroupThreadID)
{
	uint nThreadId = nDTid.y * g_cbCamState.rt_width + nDTid.x;
	uint num_frags = sr_fragment_counter[nDTid.xy];
	if (num_frags == 0
		|| nThreadId == 0 // test
		)
		return;

	uint valid_frags = min(num_frags, g_cbCamState.k_value);

	uint offset = 0;
	InterlockedAdd(offsettable_buf[0], valid_frags, offset);
	//InterlockedAdd(__offset, valid_frags, offset);

	offsettable_buf[nThreadId] = offset;
}