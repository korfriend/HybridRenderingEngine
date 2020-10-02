
#define LOAD_BLOCK_INFO(BLK, P, V, N, I) \
    BlockSkip BLK = ComputeBlockSkip(P, V, g_cbVobj.volblk_size_ts, g_cbVobj.volblk_value_range, tex3D_volblk);\
    BLK.num_skip_steps = min(max(1, BLK.num_skip_steps), N - I);


#define sort_insert(num, fragments, FRAG) {					\
	[loop]												\
	for (int j = 1; j < num; ++j)						\
	{													\
		FRAG key = fragments[j];					\
		int i = j - 1;									\
														\
		[loop]											\
		while (i >= 0 && fragments[i].z > key.z)\
		{												\
			fragments[i + 1] = fragments[i];			\
			--i;										\
		}												\
		fragments[i + 1] = key;							\
	}													\
}

#define sort_shell(num, fragments, FRAG) {								\
	int inc = num >> 1;												\
	[loop]															\
	while (inc > 0)													\
	{																\
		[loop]														\
		for (int i = inc; i < num; ++i)								\
		{															\
			FRAG tmp = fragments[i];							\
																	\
			int j = i;												\
			[loop]													\
			while (j >= inc && fragments[j - inc].z > tmp.z)\
			{														\
				fragments[j] = fragments[j - inc];					\
				j -= inc;											\
			}														\
			fragments[j] = tmp;										\
		}															\
		inc = int(inc / 2.2f + 0.5f);								\
	}																\
}

#define merge(steps, a, b, c) {														 \
	int i;																			 \
	[loop]																			 \
	for (i = 0; i < steps; ++i)														 \
		leftArray[i] = fragments[a + i];											 \
																					 \
	i = 0;																			 \
	int j = 0;																		 \
	[loop]																			 \
	for (int k = a; k < c; ++k)														 \
	{																				 \
		if (b + j >= c || (i < steps && leftArray[i].z < fragments[b + j].z))\
			fragments[k] = leftArray[i++];											 \
		else																		 \
			fragments[k] = fragments[b + j++];										 \
	}																				 \
}

#define sort_merge(num, fragments, FRAG, SIZE_2D){								  \
	FRAG leftArray[SIZE_2D];					  \
	int n = num;												  \
	int steps = 1;												  \
																  \
	[loop]														  \
	while (steps <= n)											  \
	{															  \
		int i = 0;												  \
		[loop]													  \
		while (i < n - steps)									  \
		{														  \
			merge(steps, i, i + steps, min(i + steps + steps, n));\
			i += (steps << 1); /*i += 2 * steps;*/				  \
		}														  \
		steps = (steps << 1); /*steps *= 2;	*/					  \
	}															  \
}

#define sort(num, fragments, FRAG) {	   \
	if (num <= 16)				   \
		sort_insert(num, fragments, FRAG)\
	else						   \
		sort_shell(num, fragments, FRAG)\
}

#define INTERMIX(vis_out, idx_dlayer, num_frags, vis_sample, depth_sample, thick_sample, fs, merging_beta) {\
    if (idx_dlayer >= num_frags)\
    {\
        vis_out += vis_sample * (1.f - vis_out.a);\
    }\
    else\
    {\
        Fragment f_cur;\
        f_cur.i_vis = ConvertFloat4ToUInt(vis_sample);\
        f_cur.z = depth_sample;\
        f_cur.zthick = thick_sample;\
		f_cur.opacity_sum = vis_sample.a;\
        [loop]\
        while (idx_dlayer < num_frags && (f_cur.i_vis >> 24) > 0)\
        {\
            Fragment f_dly = fs[idx_dlayer];\
            Fragment f_prior, f_posterior;\
            if (f_cur.z > f_dly.z)\
            {\
                f_prior = f_dly;\
                f_posterior = f_cur;\
            }\
            else\
            {\
                f_prior = f_cur;\
                f_posterior = f_dly;\
            }\
			Fragment_OUT fs_out = MergeFrags(f_prior, f_posterior, merging_beta);\
            vis_out += ConvertUIntToFloat4(fs_out.f_prior.i_vis) * (1.f - vis_out.a);\
            if (vis_out.a > ERT_ALPHA)\
            {\
                idx_dlayer = num_frags;\
                break;\
            }\
            else\
            {\
                if (f_cur.z > f_dly.z)\
                {\
                    f_cur = fs_out.f_posterior;\
                    idx_dlayer++;\
                }\
                else\
                {\
                    fs[idx_dlayer] = fs_out.f_posterior;\
                    /*f_cur.zthick = 0;*/\
					f_cur.i_vis = 0;\
                    if (fs_out.f_posterior.zthick == 0)\
                        idx_dlayer++;\
                }\
            }\
        } \
        if (f_cur.zthick > 0)\
        {\
            vis_out += ConvertUIntToFloat4(f_cur.i_vis) * (1.f - vis_out.a);\
        }\
    }\
}

// no ZTE
#define INTERMIX_V1(vis_out, idx_dlayer, num_frags, vis_sample, depth_sample, fs) {\
    if (idx_dlayer >= num_frags)\
    {\
        vis_out += vis_sample * (1.f - vis_out.a);\
    }\
    else\
    {\
		[loop]\
        for (int k = idx_dlayer; k < num_frags; k++)\
        {\
            Fragment f_dly = fs[k];\
            float depth_diff = depth_sample - f_dly.z;\
            float4 vix_mix = vis_sample;\
            if (depth_diff >= 0) \
            {\
                float4 vis_dly = ConvertUIntToFloat4(f_dly.i_vis); \
                if (depth_diff < g_cbVobj.sample_dist)\
                    /*vix_mix = BlendFloat4AndFloat4(vis_sample, vis_dly);*/\
					vix_mix = MixOpt(vis_sample, vis_sample.a, vis_dly, f_dly.opacity_sum);\
                else\
                    vix_mix = vis_dly;\
                vis_out += vix_mix * (1.f - vis_out.a);\
                idx_dlayer++;\
                /*이것은 없애도 될 듯.*/if (vis_out.a > ERT_ALPHA)\
                {\
                    idx_dlayer = num_frags; \
                    k = num_frags;\
                    break;\
                }\
            }\
            else\
            {\
                vis_out += vix_mix * (1.f - vis_out.a);\
                k = num_frags;\
                break;\
            }\
        }\
    }\
}

#define REMAINING_MIX(vis_out, idx_dlayer, num_frags, fs) {\
    for (; idx_dlayer < num_frags; idx_dlayer++)\
    {\
        float4 vis_dly = ConvertUIntToFloat4(fs[idx_dlayer].i_vis); \
        vis_out += vis_dly * (1.f - vis_out.a);\
    }\
}

#define BitCheck(BITS, IDX) (BITS & (0x1 << IDX))

#define LOAD4_KBUF(V, F_ADDR, K) V = deep_k_buf.Load4(F_ADDR + (K) * 4 * 4)
#define STORE4_KBUF(V, F_ADDR, K) deep_k_buf.Store4(F_ADDR + (K) * 4 * 4, V)

#define LOAD2_KBUF(V, F_ADDR, K) V = deep_k_buf.Load2(F_ADDR + (K) * 2 * 4)
#define STORE2_KBUF(V, F_ADDR, K) deep_k_buf.Store2(F_ADDR + (K) * 2 * 4, V)

#if !defined(FRAG_MERGING) || FRAG_MERGING == 1
#define NUM_ELES_PER_FRAG 4
#define GET_FRAG(F, F_ADDR, K) {uint4 rb; LOAD4_KBUF(rb, F_ADDR, K); F.i_vis = rb.x; F.z = asfloat(rb.y); F.zthick = asfloat(rb.z); F.opacity_sum = asfloat(rb.w);}
#define SET_FRAG(F_ADDR, K, F) {uint4 rb = uint4(F.i_vis, asuint(F.z), asuint(F.zthick), asuint(F.opacity_sum)); STORE4_KBUF(rb, F_ADDR, K);}
#define SET_ZEROFRAG(F_ADDR, K) {STORE4_KBUF(0, F_ADDR, K);}
#else
#define NUM_ELES_PER_FRAG 2 // not optimal code
#define GET_FRAG(F, F_ADDR, K) {uint2 rb; LOAD2_KBUF(rb, F_ADDR, K); F.i_vis = rb.x; F.z = asfloat(rb.y);}
#define SET_FRAG(F_ADDR, K, F) {uint2 rb = uint2(F.i_vis, asuint(F.z)); STORE2_KBUF(rb, F_ADDR, K);}
#define SET_ZEROFRAG(F_ADDR, K) {STORE2_KBUF(0, F_ADDR, K);}
#endif


#if DO_NOT_USE_DISCARD == 1
#define EXIT return
#else
#define EXIT clip(-1)
#endif

//#define DEBUG__