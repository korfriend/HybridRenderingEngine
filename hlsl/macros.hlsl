
#define LOAD_BLOCK_INFO(BLK, P, V, N, I) \
    BlockSkip BLK = ComputeBlockSkip(P, V, g_cbVobj.volblk_size_ts, g_cbVobj.volblk_value_range, tex3D_volblk);\
    BLK.num_skip_steps = min(BLK.num_skip_steps, N - I - 1);

// NOTE THAT current low-res GPUs shows unexpected behavior when using sort_insert. 
// Instead, Do USE sort_insertOpt and sort_shellOpt
#define sort_insert(num, fragments, FRAG) {					\
	[loop]												\
	for (uint j = 1; j < num; ++j)						\
	{													\
		FRAG key = fragments[j];					\
		uint i = j - 1;									\
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


#define sort_insertOpt(num, fragments, FRAG) {			\
	[loop]												\
	for (uint j = 1; j < num; ++j)						\
	{													\
		FRAG key = fragments[j];						\
		uint i = j - 1;									\
		FRAG df = fragments[i];							\
		[loop]											\
		while (i >= 0 && df.z > key.z)					\
		{												\
			fragments[i + 1] = df;						\
			--i;										\
			df = fragments[i];							\
		}												\
		fragments[i + 1] = key;							\
	}													\
}


#define sort_shell(num, fragments, FRAG) {								\
	uint inc = num >> 1;												\
	[loop]															\
	while (inc > 0)													\
	{																\
		[loop]														\
		for (uint i = inc; i < num; ++i)								\
		{															\
			FRAG tmp = fragments[i];							\
																	\
			uint j = i;												\
			[loop]													\
			while (j >= inc && fragments[j - inc].z > tmp.z)		\
			{														\
				fragments[j] = fragments[j - inc];					\
				j -= inc;											\
			}														\
			fragments[j] = tmp;										\
		}															\
		inc = uint(inc / 2.2f + 0.5f);								\
	}																\
}


#define sort_shellOpt(num, fragments, FRAG) {								\
	uint inc = num >> 1;												\
	[loop]															\
	while (inc > 0)													\
	{																\
		[loop]														\
		for (uint i = inc; i < num; ++i)								\
		{															\
			FRAG tmp = fragments[i];							\
																	\
			uint j = i;												\
			FRAG dfrag = fragments[j - inc];						\
			[loop]													\
			while (j >= inc && dfrag.z > tmp.z)						\
			{														\
				fragments[j] = fragments[j - inc];					\
				j -= inc;											\
				dfrag = fragments[j - inc];							\
			}														\
			fragments[j] = tmp;										\
		}															\
		inc = uint(inc / 2.2f + 0.5f);								\
	}																\
}

#define merge(steps, a, b, c) {														 \
	uint i;																			 \
	[loop]																			 \
	for (i = 0; i < steps; ++i)														 \
		leftArray[i] = fragments[a + i];											 \
																					 \
	i = 0;																			 \
	uint j = 0;																		 \
	[loop]																			 \
	for (uint k = a; k < c; ++k)														 \
	{																				 \
		if (b + j >= c || (i < steps && leftArray[i].z < fragments[b + j].z))\
			fragments[k] = leftArray[i++];											 \
		else																		 \
			fragments[k] = fragments[b + j++];										 \
	}																				 \
}

#define sort_merge(num, fragments, FRAG, SIZE_2D){								  \
	FRAG leftArray[SIZE_2D];					  \
	uint n = num;												  \
	uint steps = 1;												  \
																  \
	[loop]														  \
	while (steps <= n)											  \
	{															  \
		uint i = 0;												  \
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
		sort_insertOpt(num, fragments, FRAG)\
	else						   \
		sort_shellOpt(num, fragments, FRAG)\
}

// sampling range Àû¿ë
#define MODULATE(NORM_D2, GRAD_LENGTH) {\
			float __s = GRAD_LENGTH > 0.001f ? abs(dot(view_dir, nrl)) : 0;\
			float kappa_t = g_cbVobj.kappa_i; /*5*/\
			float kappa_s = g_cbVobj.kappa_s; /*0.5*/\
			float modulator = min(GRAD_LENGTH * 2.f * g_cbVobj.value_range * g_cbVobj.grad_scale / g_cbVobj.grad_max, 1.f);\
			modulator *= pow(min(max(NORM_D2, 0.1), 1.f), kappa_t) * pow(max(1.f - __s, 0.1), kappa_s);\
			vis_sample *= modulator;\
}

#define INTERMIX_OLD(vis_out, idx_dlayer, num_frags, vis_sample, depth_sample, thick_sample, fs, merging_beta) {\
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

#define INTERMIX_OLD2(vis_out, idx_dlayer, num_frags, vis_sample, depth_sample, thick_sample, fs, merging_beta) {\
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
        while (idx_dlayer <= num_frags && (f_cur.i_vis >> 24) != 0)\
        {\
            if (vis_out.a > ERT_ALPHA)\
            {\
                idx_dlayer = num_frags;\
				i = num_ray_samples;\
                break;\
            }\
			if (f_cur.z < f_dly.z - f_dly.zthick)\
            {\
				vis_out += ConvertUIntToFloat4(f_cur.i_vis) * (1.f - vis_out.a);\
				f_cur.i_vis = 0;\
                break;\
            }\
			else if (f_dly.z < f_cur.z - f_cur.zthick)\
            {\
				vis_out += ConvertUIntToFloat4(f_dly.i_vis) * (1.f - vis_out.a);\
				f_dly = fs[++idx_dlayer];\
            }\
			else\
			{\
				Fragment f_prior, f_posterior; \
				if (f_cur.z > f_dly.z)\
				{\
					f_prior = f_dly; \
					f_posterior = f_cur; \
				}\
				else\
				{\
					f_prior = f_cur; \
					f_posterior = f_dly; \
				}\
				Fragment_OUT fs_out = MergeFrags(f_prior, f_posterior, merging_beta);\
				vis_out += ConvertUIntToFloat4(fs_out.f_prior.i_vis) * (1.f - vis_out.a);\
				if (f_cur.z > f_dly.z)\
				{\
					f_cur = fs_out.f_posterior;\
					f_dly = fs[++idx_dlayer];\
				}\
				else\
				{\
					f_cur.i_vis = 0; \
					if ((fs_out.f_posterior.i_vis >> 24) == 0)\
					{\
						f_dly = fs[++idx_dlayer]; \
					}\
					else\
						f_dly = fs_out.f_posterior;\
					break;\
				}\
			}\
        } \
		if (f_cur.i_vis != 0)\
		{\
			vis_out += ConvertUIntToFloat4(f_cur.i_vis) * (1.f - vis_out.a); \
		}\
    }\
}

// optimized
#define INTERMIX(vis_out, idx_dlayer, num_frags, vis_sample, depth_sample, thick_sample, fs, merging_beta) {\
    if (idx_dlayer >= num_frags)\
    {\
        vis_out += vis_sample * (1.f - vis_out.a);\
    }\
    else\
    {\
        Fragment f_cur;\
		/*conversion to integer causes some color difference.. but negligible*/\
        f_cur.i_vis = ConvertFloat4ToUInt(vis_sample);\
        f_cur.z = depth_sample;\
        f_cur.zthick = thick_sample;\
		f_cur.opacity_sum = vis_sample.a;\
        [loop]\
        while (idx_dlayer <= num_frags && (f_cur.i_vis >> 24) != 0)\
        {\
            if (vis_out.a > ERT_ALPHA)\
            {\
                idx_dlayer = num_frags;\
				i = num_ray_samples;\
                break;\
            }\
			if (f_cur.z < f_dly.z - f_dly.zthick)\
            {\
				vis_out += ConvertUIntToFloat4(f_cur.i_vis) * (1.f - vis_out.a);\
				f_cur.i_vis = 0;\
                break;\
            }\
			else if (f_dly.z < f_cur.z - f_cur.zthick)\
            {\
				vis_out += ConvertUIntToFloat4(f_dly.i_vis) * (1.f - vis_out.a);\
				f_dly = fs[++idx_dlayer];\
            }\
			else\
			{\
				Fragment f_prior, f_posterior; \
				if (f_cur.z > f_dly.z)\
				{\
					f_prior = f_dly; \
					f_posterior = f_cur; \
				}\
				else\
				{\
					f_prior = f_cur; \
					f_posterior = f_dly; \
				}\
				Fragment_OUT fs_out = MergeFrags(f_prior, f_posterior, merging_beta);\
				vis_out += ConvertUIntToFloat4(fs_out.f_prior.i_vis) * (1.f - vis_out.a);\
				if (f_cur.z > f_dly.z)\
				{\
					f_cur = fs_out.f_posterior;\
					f_dly = fs[++idx_dlayer];\
				}\
				else\
				{\
					f_cur.i_vis = 0; \
					if ((fs_out.f_posterior.i_vis >> 24) == 0)\
					{\
						f_dly = fs[++idx_dlayer]; \
					}\
					else\
						f_dly = fs_out.f_posterior;\
					break;\
				}\
			}\
        } \
		if (f_cur.i_vis != 0)\
		{\
			vis_out += ConvertUIntToFloat4(f_cur.i_vis) * (1.f - vis_out.a); \
		}\
    }\
}

// without zthickness blending
#define INTERMIX_V1(vis_out, idx_dlayer, num_frags, vis_sample, depth_sample, fs) {\
    if (idx_dlayer >= num_frags)\
    {\
        vis_out += vis_sample * (1.f - vis_out.a);\
    }\
    else\
    {\
		[loop]\
		/*bool is_sample_used = false;*/\
		/*herein, k <= num_frags's '=' is for safe blending of the ray sample */\
        for (uint k = idx_dlayer; k <= num_frags; k++)\
        {\
            Fragment f_dly = fs[k];\
            float depth_diff = depth_sample - f_dly.z;\
            float4 vix_mix = vis_sample;\
            if (depth_diff >= 0) \
            {\
                float4 vis_dly = ConvertUIntToFloat4(f_dly.i_vis); \
                if (depth_diff < g_cbVobj.sample_dist) {\
                    /*vix_mix = BlendFloat4AndFloat4(vis_sample, vis_dly);*/\
					vix_mix = MixOpt(vis_sample, vis_sample.a, vis_dly, vis_dly.a);\
					/*is_sample_used = true;*/ \
				}\
                else\
                    vix_mix = vis_dly;\
                vis_out += vix_mix * (1.f - vis_out.a);\
                idx_dlayer++;\
                /*maybe removable*/if (vis_out.a > ERT_ALPHA)\
                {\
                    idx_dlayer = num_frags; \
					i = num_ray_samples;\
                    k = num_frags + 1;\
                    break;\
                }\
            }\
            else\
            {\
                vis_out += vix_mix * (1.f - vis_out.a);\
				/*is_sample_used = true;*/\
                k = num_frags + 1;\
                break;\
            }\
        }\
		/*if(!is_sample_used) vis_out += vis_sample * (1.f - vis_out.a);*/\
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

#define LOAD4_KBUF(V, F_ADDR, K) V = deep_dynK_buf.Load4(F_ADDR + (K) * 4 * 4)
#define STORE4_KBUF(V, F_ADDR, K) deep_dynK_buf.Store4(F_ADDR + (K) * 4 * 4, V)

#define LOAD3_KBUF(V, F_ADDR, K) V = deep_dynK_buf.Load3(F_ADDR + (K) * 3 * 4)
#define STORE3_KBUF(V, F_ADDR, K) deep_dynK_buf.Store3(F_ADDR + (K) * 3 * 4, V)

#define LOAD2_KBUF(V, F_ADDR, K) V = deep_dynK_buf.Load2(F_ADDR + (K) * 2 * 4)
#define STORE2_KBUF(V, F_ADDR, K) deep_dynK_buf.Store2(F_ADDR + (K) * 2 * 4, V)

#if !defined(FRAG_MERGING) || FRAG_MERGING == 1
#define NUM_ELES_PER_FRAG 3
#define GET_FRAG(F, F_ADDR, K) {uint3 rb; LOAD3_KBUF(rb, F_ADDR, K); F.i_vis = rb.x; F.z = asfloat(rb.y); F.zthick = f16tof32(rb.z & 0xFFFF); F.opacity_sum = f16tof32(rb.z >> 16);}
#define SET_FRAG(F_ADDR, K, F) {uint3 rb = uint3(F.i_vis, asuint(F.z), f32tof16(F.zthick)); rb.z |= (f32tof16(F.opacity_sum) << 16); STORE3_KBUF(rb, F_ADDR, K);}
#define SET_ZEROFRAG(F_ADDR, K) {STORE3_KBUF(0, F_ADDR, K);}
#else
#define NUM_ELES_PER_FRAG 2
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