
#define LOAD_BLOCK_INFO(BLK, P, V, N, I) \
    BlockSkip BLK = ComputeBlockSkip(P, V, g_cbVobj.volblk_size_ts, g_cbVobj.volblk_value_range, tex3D_volblk);\
    BLK.num_skip_steps = min(max(1, BLK.num_skip_steps), N - I);

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

// visex 안 쓰는 버전
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