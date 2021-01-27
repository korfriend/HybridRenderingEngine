#include "../CommonShader.hlsl"

#define LOCAL_SIZE 8

Texture2D<uint> fragment_counter : register(t10);
ByteAddressBuffer deep_k_buf : register(t11);

RWTexture2D<unorm float4> rw_fragment_blendout : register(u10);

Texture2D<float4> mip_z_textures[LOCAL_SIZE / 4] : register(t15);
RWTexture2D<float4> rw_mip_z_textures[LOCAL_SIZE / 4] : register(u15);
Texture2D<unorm float4> mip_opacity_textures[LOCAL_SIZE / 4] : register(t20);
RWTexture2D<unorm float4> rw_mip_opacity_textures[LOCAL_SIZE / 4] : register(u20);
Texture2D<unorm float4> ao_textures[LOCAL_SIZE / 4] : register(t25);
RWTexture2D<unorm float4> rw_ao_textures[LOCAL_SIZE / 4] : register(u25);

Texture2D<unorm float> ao_vr_texture : register(t30);
Texture2D<float> fragment_vr_zdepth : register(t31);
RWTexture2D<unorm float> rw_ao_vr_texture : register(u30);

float3 ComputePos_SSZ2WS(float x, float y, float z)
{
	// note that column major
	//float4 pos_src_h = mul(g_cbCamState.mat_ss2ws, float4(x, y, 0, 1.f));
	//float3 pos_ip_ws = pos_src_h.xyz / pos_src_h.w;
	float3 pos_ip_ws = TransformPoint(float3(x, y, 0), g_cbCamState.mat_ss2ws);
	float3 view_dir_ws = g_cbCamState.dir_view_ws;
	if (g_cbCamState.cam_flag & 0x1)
		view_dir_ws = pos_ip_ws - g_cbCamState.pos_cam_ws;
	view_dir_ws = normalize(view_dir_ws);
	return pos_ip_ws + view_dir_ws * z;
}

void DisplayRect(int x, int y, float4 color)
{
	for (int xx = 0; xx < 3; xx++)
		for (int yy = 0; yy < 3; yy++)
			rw_fragment_blendout[int2(x, y) + int2(xx, yy)] = color;
}

#if !defined(FRAG_MERGING) || FRAG_MERGING == 1
#define LOAD1_KBUF_VIS(KBUF, F_ADDR, K) KBUF.Load(F_ADDR + (K) * 4 * 4)
#define STORE1_KBUF_VIS(V, KBUF, F_ADDR, K) KBUF.Store(F_ADDR + (K) * 4 * 4, V)
#define LOAD1_KBUF_Z(KBUF, F_ADDR, K) asfloat(KBUF.Load(F_ADDR + ((K) * 4 + 1) * 4))
#define LOAD1_KBUF_ALPHA(KBUF, F_ADDR, K) (LOAD1_KBUF_VIS(KBUF, F_ADDR, K) >> 24)
#define LOAD1_KBUF_ALPHAF(KBUF, F_ADDR, K) (LOAD1_KBUF_ALPHA(KBUF, F_ADDR, K) / 255.f)
#endif

#define XOR_HASH(x, y) (((30*x)&y) + 10*x*y)
#define PACK_1TOFF(x) (((int)(x * 255.f)) & 0xFF)
#define PACK_1TOFF_CH8b(x, c) (PACK_1TOFF(x) << (c*8))

[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void KB_SSAO_OLD(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	if (DTid.x >= g_cbCamState.rt_width || DTid.y >= g_cbCamState.rt_height)
		return;

	// at this moment, only static k buffer is supported.
	const uint k_value = g_cbCamState.k_value;
	uint bytes_per_frag = 4 * NUM_ELES_PER_FRAG;
	uint bytes_frags_per_pixel = k_value * bytes_per_frag; // to do : consider the dynamic scheme. (4 bytes unit)
	uint pixel_id = DTid.y * g_cbCamState.rt_width + DTid.x;
	uint addr_base = pixel_id * bytes_frags_per_pixel;

	int frag_cnt = (int)fragment_counter[DTid.xy];
	uint vr_hit_enc = frag_cnt >> 24;
	frag_cnt = frag_cnt & 0xFFF;

	if (frag_cnt == 0 && vr_hit_enc == 0)
	{
		rw_ao_textures[0][DTid.xy] = (float4)0;
		rw_ao_textures[1][DTid.xy] = (float4)0;
		rw_ao_vr_texture[DTid.xy] = 0;
		return;
	}
	//rw_fragment_blendout[DTid.xy] = float4((float3)frag_cnt / 8.f, 1);
	//return;

	const float r_kernel_ao = g_cbEnv.r_kernel_ao;
	const int num_dirs = g_cbEnv.num_dirs;

	//const float interval_angle = 2.f * F_PI / num_dirs * (float)XOR_HASH(DTid.x, DTid.y) * 7.f;
	const float interval_angle = F_PI / 180.0 * (float)XOR_HASH(DTid.x, DTid.y) * 7.f;
	const int num_steps = g_cbEnv.num_steps;
	const float tangent_bias = g_cbEnv.tangent_bias;
	const float ao_intensity = 1.5f;

	float3 pos_ip_ss = float3(DTid.xy, 0.0f);
	float3 pos_ip_ws = TransformPoint(pos_ip_ss, g_cbCamState.mat_ss2ws);
	float3 view_dir_ws = g_cbCamState.dir_view_ws;
	if (g_cbCamState.cam_flag & 0x1)
		view_dir_ws = pos_ip_ws - g_cbCamState.pos_cam_ws;
	view_dir_ws = normalize(view_dir_ws);

	float3 w_ws = TransformVector(float3(1.f, 0, 0), g_cbCamState.mat_ss2ws);
	float3 h_ws = TransformVector(float3(0, 1.f, 0), g_cbCamState.mat_ss2ws);
	w_ws /= length(w_ws);
	h_ws /= length(h_ws);

	int x_offset = 2; // must be determined by considering zoom-in status
	int y_offset = 2;
	uint addr_base_dxR = (DTid.y * g_cbCamState.rt_width + DTid.x + x_offset) * bytes_frags_per_pixel;
	uint addr_base_dyR = ((DTid.y + y_offset) * g_cbCamState.rt_width + DTid.x) * bytes_frags_per_pixel;

	float v_AOs[LOCAL_SIZE];

	[loop]
	for (int i = 0; i < (int)frag_cnt; i++)
	{
		v_AOs[i] = 0;
		float z = LOAD1_KBUF_Z(deep_k_buf, addr_base, i);
		float3 p = pos_ip_ws + view_dir_ws * z;

		// compute face normal // 
		// http://c0de517e.blogspot.com/2008/10/normals-without-normals.html
		float z_dxR = LOAD1_KBUF_Z(deep_k_buf, addr_base_dyR, i);
		float z_dyR = LOAD1_KBUF_Z(deep_k_buf, addr_base_dyR, i);
		
		float zRx_diff = z - z_dxR;
		float zRy_diff = z - z_dyR;
		float z_dx = z_dxR, z_dy = z_dyR;

		//if ((z_dx - z_dy) * (z_dx - z_dy) > r_kernel_ao * r_kernel_ao) continue;

		float3 p_dx = ComputePos_SSZ2WS((int)(DTid.x + x_offset), (int)DTid.y, z_dx);
		float3 p_dy = ComputePos_SSZ2WS((int)(DTid.x), (int)(DTid.y + y_offset), z_dy);
		
		float3 p_ddx = p_dx - p;
		float3 p_ddy = p_dy - p;
		float3 face_normal = normalize(cross(p_ddx, p_ddy));
		float dot_view_face = dot(view_dir_ws, face_normal);
		if (dot_view_face > 0)
			face_normal *= -1.f;

		float3 w_a, h_b, w, h;
		if (g_cbCamState.cam_flag & 0x1)
		{
			//ComputeSSS_PerspMask(w_a, h_b, g_cbCamState.pos_cam_ws, g_cbCamState.dir_view_ws, r_kernel_ao, pos_ip_ws, p, w_ws);
			float r_i;
			ComputeSSS_PerspMask2(r_i, g_cbCamState.pos_cam_ws, r_kernel_ao, pos_ip_ws, p);
			w_a = w_ws * r_i;
			h_b = h_ws * r_i;
		}
		else
		{
			w_a = w_ws * r_kernel_ao;
			h_b = h_ws * r_kernel_ao;
		}
		float ellipse_a = length(w_a);
		float ellipse_b = length(h_b);
		w = w_a / ellipse_a;
		h = h_b / ellipse_b;
		float inter_r = max(ellipse_a, ellipse_b);

		float AO = 0;
		for (int k = 0; k < num_dirs; k++) 
		{
#if HBAO == 1
			//float azimuthal_angle = interval_angle * k;
			float azimuthal_angle = interval_angle * Random(float2(DTid.x, DTid.y * k) * k) * 360.f;
			float cos_zimuthal = cos(azimuthal_angle);
			float sin_zimuthal = sin(azimuthal_angle);
			//float2 azimuthal_uv = float2(1.f, 0) * cos_zimuthal + float2(0, 1.f) * sin_zimuthal;
			//azimuthal_uv = normalize(azimuthal_uv);
			//float inter_r =  1.f / sqrt(azimuthal_uv.x * azimuthal_uv.x / (ellipse_a * ellipse_a) + azimuthal_uv.y * azimuthal_uv.y / (ellipse_b * ellipse_b));
			// ellipse_a;// 
			//w = w_ws, h = h_ws;
			//float inter_r = ellipse_a;

			float3 azimuthal_v = w * cos_zimuthal + h * sin_zimuthal;
			float3 azimuthal_vcross = cross(view_dir_ws, azimuthal_v);
			azimuthal_v = cross(azimuthal_vcross, view_dir_ws);
			azimuthal_v = normalize(azimuthal_v);

			// compute the angles on the 2D plane defined by -view_dir_ws and azimuthal_v
			float3 face_normal_proj = -view_dir_ws * dot(face_normal, -view_dir_ws) + azimuthal_v * dot(face_normal, azimuthal_v);//
			face_normal_proj = normalize(face_normal_proj);
			float _dot_t = dot(-view_dir_ws, face_normal_proj);
			float t_theta = acos(_dot_t);// min(max(_dot_t, -0.99f), 0.99f));

			///// weird..
			//inter_r = 1.f / sqrt(azimuthal_v.x * azimuthal_v.x / (ellipse_a * ellipse_a) + azimuthal_v.y * azimuthal_v.y / (ellipse_b * ellipse_b));
			float t_theta_signed = dot(azimuthal_v, face_normal_proj) > 0 ? -t_theta : t_theta;
			t_theta_signed += tangent_bias;
			float max_h_theta = t_theta_signed;
			float AO_theta = 0;
			int2 ss_xy_prev = (int2)(pos_ip_ss.xy + (float2)0.5f);
			for (int l = 0; l < num_steps; l++)
			{
				float r_dist = (inter_r / (float)(num_steps + 1)) * (float)(l + 1);
				float3 pos_ip_ellipse_ws = pos_ip_ws + azimuthal_v * r_dist;
				float3 pos_ip_ellipse_ss = TransformPoint(pos_ip_ellipse_ws, g_cbCamState.mat_ws2ss);
				int2 ss_xy = int2(pos_ip_ellipse_ss.xy + (float2)0.5f);
				int2 ss_diff = ss_xy - ss_xy_prev;
				if (dot(ss_diff, ss_diff) == 0) continue;
				ss_xy_prev = ss_xy;
				int frag_nb_cnt = (int)fragment_counter[ss_xy.xy] & 0xFFF;
				for (int j = 0; j < frag_nb_cnt; j++) // k_value, frag_nb_cnt
				{
					uint addr_base_cand = (ss_xy.y * g_cbCamState.rt_width + ss_xy.x) * bytes_frags_per_pixel;
					float z_cand = LOAD1_KBUF_Z(deep_k_buf, addr_base_cand, j);
					if (z_cand < 1000000 && z_cand > 0)
					{
						float3 p_cand = ComputePos_SSZ2WS(pos_ip_ellipse_ss.x, pos_ip_ellipse_ss.y, z_cand);
						float3 D = p_cand - p;
						D = -view_dir_ws * dot(D, -view_dir_ws) + azimuthal_v * dot(D, azimuthal_v);
						float dist = length(D);
						if (dist <= r_kernel_ao) //g_cbCamState.cam_vz_thickness)//
						{
							float _dvd = dot(-view_dir_ws, D / dist);// max(min(dot(-view_dir_ws, D / dist), 0.99f), -0.99f);
							float alpha = F_PI / 2.f - acos(_dvd);// (_dvd);// (max(min(_dvd, 1.f), -1.f));
			
							// HBAO
							//if (1)//i == 0)
							{
								float h_theta = max(alpha, t_theta_signed);
								if (max_h_theta < h_theta)
								{
									float opacity = (LOAD1_KBUF_VIS(deep_k_buf, addr_base_cand, j) >> 24) / 255.f;
									max_h_theta = h_theta;
									float local_AO = (sin(h_theta) - sin(t_theta_signed)) * opacity;
									float r_ratio = r_dist / r_kernel_ao;
									AO += (1.f - r_ratio * r_ratio) * max(local_AO - AO_theta, 0);
									AO_theta += local_AO;
								}
							}
							//else
							//{
							//	if (alpha > t_theta_signed)// && dist > g_cbCamState.cam_vz_thickness)
							//	{
							//		//float opacity = (deep_k_buf[addr_base_cand + 3 * j + 0] >> 24) / 255.f;
							//		float opacity = (LOAD1_KBUF_VIS(deep_k_buf, addr_base_cand, j) >> 24) / 255.f;
							//		float r_ratio = r_dist / r_kernel_ao;
							//		AO += (1.f - r_ratio * r_ratio) * opacity / num_steps;
							//	}
							//}

						}
					}
				} // for (int j = 0; j < frag_nb_cnt; j++) // k_value, frag_nb_cnt
				if (g_cbEnv.env_flag & 0x2)
				{
					// g_cbVobj.ao_intensity;
					float2 uv = pos_ip_ellipse_ss.xy / float2(g_cbCamState.rt_width, g_cbCamState.rt_height);
					//float z_cand = fragment_vr_zdepth[ss_xy_prev];// fragment_vr_zdepth.SampleLevel(g_samplerLinear, uv, 0);
					float z_cand = fragment_vr_zdepth.SampleLevel(g_samplerLinear, uv, 0);
					if (z_cand < 1000000 && z_cand > 0)
					{
						float3 p_cand;
						float4 pos_cand_ip_h = mul(g_cbCamState.mat_ss2ws, float4(ss_xy.x, ss_xy.y, 0, 1.f));
						float3 pos_cand_ip_ws = pos_cand_ip_h.xyz / pos_cand_ip_h.w;
						float3 view_cand_dir_ws = g_cbCamState.dir_view_ws;
						if (g_cbCamState.cam_flag & 0x1)
							view_cand_dir_ws = pos_cand_ip_ws - g_cbCamState.pos_cam_ws;
						view_cand_dir_ws = normalize(view_cand_dir_ws);
						p_cand = pos_cand_ip_ws + view_cand_dir_ws * z_cand;

						float3 D = p_cand - p;
						D = -view_dir_ws * dot(D, -view_dir_ws) + azimuthal_v * dot(D, azimuthal_v);
						float dist = length(D);
						if (dist <= r_kernel_ao)
						{
							float _dvd = dot(-view_dir_ws, D / dist);// max(min(dot(-view_dir_ws, D / dist), 0.99f), -0.99f);
							float alpha = F_PI / 2.f - acos(_dvd);// (_dvd);// (max(min(_dvd, 1.f), -1.f));

							// HBAO
							float h_theta = max(alpha, t_theta_signed);
							if (max_h_theta < h_theta)
							{
								max_h_theta = h_theta;
								float local_AO = (sin(h_theta) - sin(t_theta_signed)) * g_cbVobj.ao_intensity;
								float r_ratio = r_dist / r_kernel_ao;
								AO += (1.f - r_ratio * r_ratio) * max(local_AO - AO_theta, 0);
								AO_theta += local_AO;
							}
						}
					}
				}
			}
#else
#endif
		}
		//rw_fragment_blendout[DTid.xy] = float4((float3)dot(-view_dir_ws, face_normal), 1);
		v_AOs[i] = min(1.f / (2.f * F_PI) * AO * ao_intensity, 1.f);
	}
	rw_fragment_blendout[DTid.xy] = float4((float3)1 - v_AOs[0], 1);
	return;

	for (i = (int)frag_cnt; i < k_value; i++) v_AOs[i] = 0;

	rw_ao_textures[0][DTid.xy] = float4(v_AOs[0], v_AOs[1], v_AOs[2], v_AOs[3]);
	rw_ao_textures[1][DTid.xy] = float4(v_AOs[4], v_AOs[5], v_AOs[6], v_AOs[7]);

	float ao_vr = 0;
	if (g_cbEnv.env_flag & 0x2 && vr_hit_enc == 1)
	{
		float z = fragment_vr_zdepth[DTid.xy];
		float3 p = pos_ip_ws + view_dir_ws * z;

		// compute face normal // 
		// https://www.youtube.com/watch?v=eqzh8lWWyrk
		// http://c0de517e.blogspot.com/2008/10/normals-without-normals.html
		int x_offset = 1;
		int y_offset = 1;
		uint addr_base_dxR = (DTid.y * g_cbCamState.rt_width + DTid.x + x_offset) * k_value * 4;
		uint addr_base_dxL = (DTid.y * g_cbCamState.rt_width + DTid.x - x_offset) * k_value * 4;
		uint addr_base_dyR = ((DTid.y + y_offset) * g_cbCamState.rt_width + DTid.x) * k_value * 4;
		uint addr_base_dyL = ((DTid.y - y_offset) * g_cbCamState.rt_width + DTid.x) * k_value * 4;
		float z_dxR = fragment_vr_zdepth[uint2(DTid.x + x_offset, DTid.y)];
		float z_dxL = fragment_vr_zdepth[uint2(DTid.x - x_offset, DTid.y)];
		float z_dyR = fragment_vr_zdepth[uint2(DTid.x, DTid.y + y_offset)];
		float z_dyL = fragment_vr_zdepth[uint2(DTid.x, DTid.y - y_offset)];
		float zRx_diff = z - z_dxR;
		float zLx_diff = z - z_dxL;
		float zRy_diff = z - z_dyR;
		float zLy_diff = z - z_dyL;
		float z_dx = z_dxR, z_dy = z_dyR;
		if (zRx_diff*zRx_diff > zLx_diff*zLx_diff)
		{
			z_dx = z_dxL;
			x_offset = -1;
		}
		if (zRy_diff*zRy_diff > zLy_diff*zLy_diff)
		{
			z_dy = z_dyL;
			y_offset = -1;
		}
		ao_vr = 1;
		//if ((z_dx - z_dy) * (z_dx - z_dy) > r_kernel_ao * r_kernel_ao) continue;

		float3 p_dx = ComputePos_SSZ2WS(DTid.x + x_offset, DTid.y, z_dx);
		float3 p_dy = ComputePos_SSZ2WS(DTid.x, DTid.y + y_offset, z_dy);
		float3 p_ddx = p_dx - p;
		float3 p_ddy = p_dy - p;
		float3 face_normal = normalize(cross(p_ddx, p_ddy));
		float dot_view_face = dot(view_dir_ws, face_normal);
		if (dot_view_face > 0)
			face_normal *= -1.f;

		float3 w_a, h_b, w, h;
		if (g_cbCamState.cam_flag & 0x1)
		{
			//ComputeSSS_PerspMask(w_a, h_b, g_cbCamState.pos_cam_ws, g_cbCamState.dir_view_ws, r_kernel_ao, pos_ip_ws, p, w_ws);
			float r_i;
			ComputeSSS_PerspMask2(r_i, g_cbCamState.pos_cam_ws, r_kernel_ao, pos_ip_ws, p);
			w_a = w_ws * r_i;
			h_b = h_ws * r_i;
		}
		else
		{
			w_a = w_ws * r_kernel_ao;
			h_b = h_ws * r_kernel_ao;
		}
		float ellipse_a = length(w_a);
		float ellipse_b = length(h_b);
		w = w_a / ellipse_a;
		h = h_b / ellipse_b;

		float AO = 0;
		for (int k = 0; k < num_dirs; k++) // num_dirs
		{
#if HBAO == 1
			float azimuthal_angle = interval_angle * k;
			//float azimuthal_angle = interval_angle * Random(float2(DTid.x, DTid.y * k) * k) * 360.f;
			float cos_zimuthal = cos(azimuthal_angle);
			float sin_zimuthal = sin(azimuthal_angle);
			float2 azimuthal_uv = float2(1.f, 0) * cos_zimuthal + float2(0, 1.f) * sin_zimuthal;
			azimuthal_uv = normalize(azimuthal_uv);
			// ellipse_a;// 
			w = w_ws, h = h_ws;
			float inter_r = ellipse_a;// 1.f / sqrt(azimuthal_uv.x * azimuthal_uv.x / (ellipse_a * ellipse_a) + azimuthal_uv.y * azimuthal_uv.y / (ellipse_b * ellipse_b));

			float3 azimuthal_v = w * cos_zimuthal + h * sin_zimuthal;
			float3 azimuthal_vcross = cross(view_dir_ws, azimuthal_v);
			azimuthal_v = cross(azimuthal_vcross, view_dir_ws);
			azimuthal_v = normalize(azimuthal_v);

			// compute the angles on the 2D plane defined by -view_dir_ws and azimuthal_v
			float3 face_normal_proj = -view_dir_ws * dot(face_normal, -view_dir_ws) + azimuthal_v * dot(face_normal, azimuthal_v);//
			face_normal_proj = normalize(face_normal_proj);
			float _dot_t = dot(-view_dir_ws, face_normal_proj);
			float t_theta = acos(_dot_t);// min(max(_dot_t, -0.99f), 0.99f));

			float t_theta_signed = dot(azimuthal_v, face_normal_proj) > 0 ? -t_theta : t_theta;
			t_theta_signed += tangent_bias;
			float max_h_theta = t_theta_signed;
			float AO_theta = 0;
			int2 ss_xy_prev = (int2)(pos_ip_ss.xy + (float2)0.5f);
			for (int l = 0; l < num_steps; l++)
			{
				float r_dist = (inter_r / (float)(num_steps + 1)) * (float)(l + 1);
				float3 pos_ip_ellipse_ws = pos_ip_ws + azimuthal_v * r_dist;
				float3 pos_ip_ellipse_ss = TransformPoint(pos_ip_ellipse_ws, g_cbCamState.mat_ws2ss);
				//float3 pos_ip_ellipse_ss;
				//{
				//	float4 pos_ip_ellipse_ssh = mul(float4(pos_ip_ellipse_ws, 1.f), g_cbCamState.mat_ws2ss);
				//	pos_ip_ellipse_ss = pos_ip_ellipse_ssh.xyz / pos_ip_ellipse_ssh.w;
				//}
				int2 ss_xy = int2(pos_ip_ellipse_ss.xy + (float2)0.5f);
				int2 ss_diff = ss_xy - ss_xy_prev;
				if (dot(ss_diff, ss_diff) == 0) continue;
				ss_xy_prev = ss_xy;
				//[allow_uav_condition][loop]
				int frag_nb_cnt = (int)fragment_counter[ss_xy.xy] & 0xFFF;
				for (int j = 0; j < frag_nb_cnt; j++) // k_value, frag_nb_cnt
				{
					uint addr_base_cand = (ss_xy.y * g_cbCamState.rt_width + ss_xy.x) * k_value * 4;
					float z_cand = LOAD1_KBUF_Z(deep_k_buf, addr_base_cand, j); // asfloat(deep_k_buf[addr_base_cand + 3 * j + 1]);
					if (z_cand < 1000000 && z_cand > 0)
					{
						float3 p_cand = ComputePos_SSZ2WS(pos_ip_ellipse_ss.x, pos_ip_ellipse_ss.y, z_cand);
						//{
						// float4 pos_cand_ip_h = mul(float4(ss_xy.x, ss_xy.y, 0, 1.f), g_cbCamState.mat_ss2ws);
						// float3 pos_cand_ip_ws = pos_cand_ip_h.xyz / pos_cand_ip_h.w;
						// float3 view_cand_dir_ws = g_cbCamState.dir_view_ws;
						// if (g_cbCamState.cam_flag & 0x1)
						// 	view_cand_dir_ws = pos_cand_ip_ws - g_cbCamState.pos_cam_ws;
						// view_cand_dir_ws = normalize(view_cand_dir_ws);
						// p_cand = pos_cand_ip_ws + view_cand_dir_ws * z_cand;
						//}
						float3 D = p_cand - p;
						D = -view_dir_ws * dot(D, -view_dir_ws) + azimuthal_v * dot(D, azimuthal_v);
						float dist = length(D);
						if (dist <= r_kernel_ao) //g_cbCamState.cam_vz_thickness)//
						{
							float _dvd = dot(-view_dir_ws, D / dist);// max(min(dot(-view_dir_ws, D / dist), 0.99f), -0.99f);
							float alpha = F_PI / 2.f - acos(_dvd);// (_dvd);// (max(min(_dvd, 1.f), -1.f));

							// HBAO
							//if (1)//i == 0)
							{
								float h_theta = max(alpha, t_theta_signed);
								if (max_h_theta < h_theta)
								{
									//float opacity = (deep_k_buf[addr_base_cand + 3 * j + 0] >> 24) / 255.f;
									float opacity = (LOAD1_KBUF_VIS(deep_k_buf, addr_base_cand, j) >> 24) / 255.f;
									max_h_theta = h_theta;
									float local_AO = (sin(h_theta) - sin(t_theta_signed)) * opacity;
									float r_ratio = r_dist / r_kernel_ao;
									AO += (1.f - r_ratio * r_ratio) * max(local_AO - AO_theta, 0);
									AO_theta += local_AO;
								}
							}
							//else
							//{
							//	if (alpha > t_theta_signed)// && dist > g_cbCamState.cam_vz_thickness)
							//	{
							//		//float opacity = (deep_k_buf[addr_base_cand + 3 * j + 0] >> 24) / 255.f;
							//		float opacity = (LOAD1_KBUF_VIS(deep_k_buf, addr_base_cand, j) >> 24) / 255.f;
							//		float r_ratio = r_dist / r_kernel_ao;
							//		AO += (1.f - r_ratio * r_ratio) * opacity / num_steps;
							//	}
							//}

						}
					}
				} // for (int j = 0; j < frag_nb_cnt; j++) // k_value, frag_nb_cnt
				//if (g_cbEnv.env_flag & 0x2) // always
				{
					// g_cbVobj.ao_intensity;
					float2 uv = pos_ip_ellipse_ss.xy / float2(g_cbCamState.rt_width, g_cbCamState.rt_height);
					float z_cand = fragment_vr_zdepth.SampleLevel(g_samplerLinear, uv, 0);
					if (z_cand < 1000000 && z_cand > 0)
					{
						float3 p_cand;
						float4 pos_cand_ip_h = mul(float4(ss_xy.x, ss_xy.y, 0, 1.f), g_cbCamState.mat_ss2ws);
						float3 pos_cand_ip_ws = pos_cand_ip_h.xyz / pos_cand_ip_h.w;
						float3 view_cand_dir_ws = g_cbCamState.dir_view_ws;
						if (g_cbCamState.cam_flag & 0x1)
							view_cand_dir_ws = pos_cand_ip_ws - g_cbCamState.pos_cam_ws;
						view_cand_dir_ws = normalize(view_cand_dir_ws);
						p_cand = pos_cand_ip_ws + view_cand_dir_ws * z_cand;
						
						float3 D = p_cand - p;
						D = -view_dir_ws * dot(D, -view_dir_ws) + azimuthal_v * dot(D, azimuthal_v);
						float dist = length(D);

						if (dist <= r_kernel_ao)
						{
							float _dvd = dot(-view_dir_ws, D / dist);// max(min(dot(-view_dir_ws, D / dist), 0.99f), -0.99f);
							float alpha = F_PI / 2.f - acos(_dvd);// (_dvd);// (max(min(_dvd, 1.f), -1.f));

							// HBAO
							float h_theta = max(alpha, t_theta_signed);
							if (max_h_theta < h_theta)
							{
								max_h_theta = h_theta;
								float local_AO = (sin(h_theta) - sin(t_theta_signed)) * g_cbVobj.ao_intensity;
								float r_ratio = r_dist / r_kernel_ao;
								AO += (1.f - r_ratio * r_ratio) * max(local_AO - AO_theta, 0);
								AO_theta += local_AO;
							}
						}
					}
				}
			}
#else
#endif
		}
		ao_vr = min(1.f / (2.f * F_PI) * AO * ao_intensity, 1.f);
	}
	//rw_ao_vr_texture[DTid.xy] = ao_vr;
	//vis_out = float4(TransformPoint(pos_ray_start_ws, g_cbVobj.mat_ws2ts), 1);
	//rw_fragment_blendout[DTid.xy] = float4((float3)ao_vr, 1);
	////if(DTid.x % 200 != 0 || DTid.y % 200 != 0)
	return;
	/**/
}

#define MaxRadiusPixels 20
void ComputeSteps(out float stepSizePix, out float numSteps, float kernel_r_pix, float rand)
{
	// Avoid oversampling if numSteps is greater than the kernel radius in pixels
	numSteps = min(g_cbEnv.num_steps, kernel_r_pix);

	// Divide by Ns+1 so that the farthest samples are not fully attenuated
	stepSizePix = kernel_r_pix / (numSteps + 1);

	// Clamp numSteps if it is greater than the max kernel footprint
	float maxNumSteps = MaxRadiusPixels / stepSizePix;
	if (maxNumSteps < numSteps)
	{
		// Use dithering to avoid AO discontinuities
		numSteps = floor(maxNumSteps + rand);
		numSteps = max(numSteps, 1);
		stepSizePix = MaxRadiusPixels / numSteps;
	}
}

float2 RotateDirections(float2 Dir, float2 CosSin)
{
	return float2(Dir.x*CosSin.x - Dir.y*CosSin.y,
		Dir.x*CosSin.y + Dir.y*CosSin.x);
}

float TanToSin(float x)
{
	return x * rsqrt(x*x + 1.0);
}

float Length2(float3 V)
{
	return dot(V, V);
}

float HorizonOcclusion(int2 xy_coord, float2 delta_dir,
	float3 P,
	float3 face_normal,
	float randstep,
	int numSamples,
	uint bytes_frags_per_pixel,
	bool apply_vr_geometry)
{
	float ao = 0;

	// Offset the first coord with some noise
	float2 xy_pix = (float2)xy_coord + round(randstep * delta_dir);
	//delta_dir = float2(uint2(delta_dir));

	// Get the angle of the tangent vector from the viewspace axis
	float tanH = tan(g_cbEnv.tangent_bias);
	float sinH = TanToSin(tanH);

	float tanS;
	float d2;
	float3 S;

	float R2 = g_cbEnv.r_kernel_ao * g_cbEnv.r_kernel_ao;
	float NegInvR2 = -1.0 / R2;

	// Sample to find the maximum angle
	[loop]
	for (int s = 1; s <= numSamples; ++s)
	{
		xy_pix += delta_dir;

		int2 ixy_pix = int2(xy_pix);
		int2 diff_pix = ixy_pix - xy_coord;
		if (dot(diff_pix, diff_pix) == 0) continue;

		int frag_cnt = (int)fragment_counter[ixy_pix] & 0xFFF;
		if (frag_cnt == 0) continue;

		uint addr_base = (ixy_pix.y * g_cbCamState.rt_width + ixy_pix.x) * bytes_frags_per_pixel;
#if ZT_MODEL == 1
		float3 pos_ip_ws = TransformPoint(float3(ixy_pix.x, ixy_pix.y, 0), g_cbCamState.mat_ss2ws);
		float3 view_dir_ws = g_cbCamState.dir_view_ws;
		if (g_cbCamState.cam_flag & 0x1)
			view_dir_ws = pos_ip_ws - g_cbCamState.pos_cam_ws;
		view_dir_ws = normalize(view_dir_ws);
		for (int layer = 0; layer < frag_cnt; ++layer)
		{
			Fragment f;
			GET_FRAG(f, addr_base, layer);
			float z = f.z;

			S = pos_ip_ws + view_dir_ws * z;

			float a = (f.i_vis >> 24) / 255.0;
			float zthick_merge = f.zthick - g_cbCamState.cam_vz_thickness;
			float check_merge = (a - f.opacity_sum) * (a - f.opacity_sum) > 1.0 / 255 && zthick_merge > 0;
			float w = 1.f;
			for (int i = 1; i <= 3; i++)
			{
				tanS = tan(F_PI * 0.5 - acos(dot(normalize(S - P), face_normal)));
				d2 = Length2(S - P);

				// Is the sample within the radius and the angle greater?
				if (d2 < R2 && tanS > tanH)
				{
					float sinS = TanToSin(tanS);
					// Apply falloff based on the distance : (d2 * NegInvR2 + 1.0f)
					float falloff = (d2 * NegInvR2 + 1.0f);
					float occl = (sinS - sinH);
#if APPLY_TRANSPARENCY == 1
					ao += falloff * occl * a * w;
#else
					ao += falloff * occl;
#endif
					tanH = tanS;
					sinH = sinS;
					break;
				}
				else
				{
					if (!check_merge) break;
					w = pow(2, -i);
					float thick = zthick_merge * w;
					S -= view_dir_ws * thick;
				}
			}
		}
#else
		for (int layer = 0; layer < frag_cnt; ++layer)
		{
			float z = LOAD1_KBUF_Z(deep_k_buf, addr_base, layer);

			S = ComputePos_SSZ2WS(ixy_pix.x, ixy_pix.y, z);

			tanS = tan(F_PI * 0.5 - acos(dot(normalize(S - P), face_normal)));
			d2 = Length2(S - P);

			// Is the sample within the radius and the angle greater?
			if (d2 < R2 && tanS > tanH)
			{
				float sinS = TanToSin(tanS);
				// Apply falloff based on the distance : (d2 * NegInvR2 + 1.0f)
				float falloff = (d2 * NegInvR2 + 1.0f);
				float occl = (sinS - sinH);
#if APPLY_TRANSPARENCY == 1
				float a = LOAD1_KBUF_ALPHAF(deep_k_buf, addr_base, layer);
				ao += falloff * occl * a;
#else
				ao += falloff * occl;
#endif

				tanH = tanS;
				sinH = sinS;
			}
		}
#endif

		// to do for VR geometry
		if (apply_vr_geometry)
		{
			float z = fragment_vr_zdepth[ixy_pix];

			S = ComputePos_SSZ2WS(ixy_pix.x, ixy_pix.y, z);

			tanS = tan(F_PI * 0.5 - acos(dot(normalize(S - P), face_normal)));
			d2 = Length2(S - P);

			// Is the sample within the radius and the angle greater?
			if (d2 < R2 && tanS > tanH)
			{
				float sinS = TanToSin(tanS);
				// Apply falloff based on the distance
				ao += (d2 * NegInvR2 + 1.0f) * (sinS - sinH);

				tanH = tanS;
				sinH = sinS;
			}
		}
	}

	return ao;
}

float3 MinDiff(float3 P, float3 Pr, float3 Pl)
{
	float3 V1 = Pr - P;
	float3 V2 = P - Pl;
	return (Length2(V1) < Length2(V2)) ? V1 : V2;
}

[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void KB_SSAO(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	if (DTid.x >= g_cbCamState.rt_width || DTid.y >= g_cbCamState.rt_height)
		return;

	// at this moment, only static k buffer is supported.
	const uint k_value = g_cbCamState.k_value;
	uint bytes_per_frag = 4 * NUM_ELES_PER_FRAG;
	uint bytes_frags_per_pixel = k_value * bytes_per_frag; // to do : consider the dynamic scheme. (4 bytes unit)
	uint pixel_id = DTid.y * g_cbCamState.rt_width + DTid.x;
	uint addr_base = pixel_id * bytes_frags_per_pixel;

	int frag_cnt = (int)fragment_counter[DTid.xy];
	uint vr_hit_enc = frag_cnt >> 24;
	frag_cnt = frag_cnt & 0xFFF;
	//rw_fragment_blendout[DTid.xy] = float4((float3)frag_cnt / 8.0, 1);
	//return;

	if (frag_cnt == 0 && vr_hit_enc == 0)
	{
		rw_ao_textures[0][DTid.xy] = (float4)0;
		rw_ao_textures[1][DTid.xy] = (float4)0;
		rw_ao_vr_texture[DTid.xy] = 0;
		return;
	}

	const float r_kernel_ao = g_cbEnv.r_kernel_ao;
	const float ao_intensity = 1.5f;

	float3 pos_ip_ss = float3(DTid.xy, 0.0f);
	float3 pos_ip_ws = TransformPoint(pos_ip_ss, g_cbCamState.mat_ss2ws);
	float3 view_dir_ws = g_cbCamState.dir_view_ws;
	if (g_cbCamState.cam_flag & 0x1)
		view_dir_ws = pos_ip_ws - g_cbCamState.pos_cam_ws;
	view_dir_ws = normalize(view_dir_ws);

	float3 ipO_ws = TransformPoint(float3(g_cbCamState.rt_width / 2.f, g_cbCamState.rt_height / 2.f, 0), g_cbCamState.mat_ss2ws);
	float3 ipx_ws = TransformPoint(float3(g_cbCamState.rt_width / 2.f + 1.f, g_cbCamState.rt_height / 2.f, 0), g_cbCamState.mat_ss2ws);
	float3 pix_ws = ipx_ws - ipO_ws;

	float pix_r = length(pix_ws);
	float3 random_samples = float3(_random(DTid.x + g_cbCamState.rt_width * DTid.y), _random(DTid.x + g_cbCamState.rt_width * DTid.y + g_cbCamState.rt_width * g_cbCamState.rt_height), _random(DTid.xy));

	const int num_dirs = g_cbEnv.num_dirs;
	float alpha = 2.0 * F_PI / num_dirs;
	
	int x_offset = 2; // must be determined by considering zoom-in status
	int y_offset = 2;
	uint addr_base_dxR = (DTid.y * g_cbCamState.rt_width + DTid.x + x_offset) * bytes_frags_per_pixel;
	uint addr_base_dyR = ((DTid.y + y_offset) * g_cbCamState.rt_width + DTid.x) * bytes_frags_per_pixel;
	uint addr_base_dxL = (DTid.y * g_cbCamState.rt_width + DTid.x - x_offset) * bytes_frags_per_pixel;
	uint addr_base_dyL = ((DTid.y - y_offset) * g_cbCamState.rt_width + DTid.x) * bytes_frags_per_pixel;

	float v_AOs[LOCAL_SIZE];
	[loop]
	for (int i = 0; i < (int)frag_cnt; i++)
	{
		v_AOs[i] = 0;
		float z = LOAD1_KBUF_Z(deep_k_buf, addr_base, i);
		float3 p = pos_ip_ws + view_dir_ws * z;
		float z_p = dot(p - g_cbCamState.pos_cam_ws, view_dir_ws);

		float kernel_r_ip = (g_cbCamState.cam_flag & 0x1) ? r_kernel_ao * (g_cbCamState.near_plane / z_p) : r_kernel_ao;
		float kernel_r_ss = kernel_r_ip / pix_r;

		if (kernel_r_ss <= 1.0)
		{
			v_AOs[i] = 1.f;
			continue;
		}

		float numSteps, stepSizePix;
		ComputeSteps(stepSizePix, numSteps, kernel_r_ss, random_samples.z);

		// compute face normal // 
		// http://c0de517e.blogspot.com/2008/10/normals-without-normals.html
		float z_dxR = LOAD1_KBUF_Z(deep_k_buf, addr_base_dxR, i);
		float z_dyR = LOAD1_KBUF_Z(deep_k_buf, addr_base_dyR, i);
		float z_dxL = LOAD1_KBUF_Z(deep_k_buf, addr_base_dxL, i);
		float z_dyL = LOAD1_KBUF_Z(deep_k_buf, addr_base_dyL, i);

		float3 p_dxR = ComputePos_SSZ2WS((int)(DTid.x + x_offset), (int)DTid.y, z_dxR);
		float3 p_dyR = ComputePos_SSZ2WS((int)(DTid.x), (int)(DTid.y + y_offset), z_dyR);
		float3 p_dxL = ComputePos_SSZ2WS((int)(DTid.x - x_offset), (int)DTid.y, z_dxL);
		float3 p_dyL = ComputePos_SSZ2WS((int)(DTid.x), (int)(DTid.y - y_offset), z_dyL);
		
		float3 dPdx = MinDiff(p, p_dxR, p_dxL);
		float3 dPdy = MinDiff(p, p_dyR, p_dyL);
		float3 face_normal = normalize(cross(dPdx, dPdy));
		if (dot(view_dir_ws, face_normal) > 0)
			face_normal *= -1.f;
		//rw_fragment_blendout[DTid.xy] = float4((float3)z / 100, 1);
		//if(i == frag_cnt - 1) return;
		float ao = 0.0;
		// Calculate the horizon occlusion of each direction
		for (float d = 0; d < num_dirs; ++d)
		{
			float theta = alpha * d;

			// Apply noise to the direction
			float2 dir = RotateDirections(float2(cos(theta), sin(theta)), random_samples.xy);
			float2 delta_dir = dir * stepSizePix;

			// Sample the pixels along the direction
			ao += HorizonOcclusion(DTid.xy, delta_dir,
				p, face_normal,
				random_samples.z,
				(int)numSteps, 
				bytes_frags_per_pixel, false);
		}

		v_AOs[i] = ao / num_dirs * ao_intensity;
	}
	rw_fragment_blendout[DTid.xy] = float4((float3)1 - v_AOs[0], 1);
	//return;
	for (i = (int)frag_cnt; i < k_value; i++) v_AOs[i] = 0;
	rw_ao_textures[0][DTid.xy] = float4(v_AOs[0], v_AOs[1], v_AOs[2], v_AOs[3]);
	rw_ao_textures[1][DTid.xy] = float4(v_AOs[4], v_AOs[5], v_AOs[6], v_AOs[7]);

	float ao_vr = 0;
	if (g_cbEnv.env_flag & 0x2 && vr_hit_enc == 1)
	{
		float z = fragment_vr_zdepth[DTid.xy];
		float3 p = pos_ip_ws + view_dir_ws * z;

		float z_p = dot(p - g_cbCamState.pos_cam_ws, view_dir_ws);

		float kernel_r_ip = (g_cbCamState.cam_flag & 0x1) ? r_kernel_ao * (g_cbCamState.near_plane / z_p) : r_kernel_ao;
		float kernel_r_ss = kernel_r_ip / pix_r;

		if (kernel_r_ss <= 1.0)
		{
			ao_vr = 1.f;
		}
		else
		{
			float numSteps, stepSizePix;
			ComputeSteps(stepSizePix, numSteps, kernel_r_ss, random_samples.z);

			// compute face normal // 
			// http://c0de517e.blogspot.com/2008/10/normals-without-normals.html
			float z_dxR = fragment_vr_zdepth[DTid.xy + uint2(1, 0)];
			float z_dyR = fragment_vr_zdepth[DTid.xy + uint2(0, 1)];
			float z_dxL = fragment_vr_zdepth[DTid.xy - uint2(1, 0)];
			float z_dyL = fragment_vr_zdepth[DTid.xy - uint2(0, 1)];

			float3 p_dxR = ComputePos_SSZ2WS((int)(DTid.x + x_offset), (int)DTid.y, z_dxR);
			float3 p_dyR = ComputePos_SSZ2WS((int)(DTid.x), (int)(DTid.y + y_offset), z_dyR);
			float3 p_dxL = ComputePos_SSZ2WS((int)(DTid.x - x_offset), (int)DTid.y, z_dxL);
			float3 p_dyL = ComputePos_SSZ2WS((int)(DTid.x), (int)(DTid.y - y_offset), z_dyL);

			float3 dPdx = MinDiff(p, p_dxR, p_dxL);
			float3 dPdy = MinDiff(p, p_dyR, p_dyL);
			float3 face_normal = normalize(cross(dPdx, dPdy));
			if (dot(view_dir_ws, face_normal) > 0)
				face_normal *= -1.f;

			float ao = 0;
			// Calculate the horizon occlusion of each direction
			for (float d = 0; d < num_dirs; ++d)
			{
				float theta = alpha * d;

				// Apply noise to the direction
				float2 dir = RotateDirections(float2(cos(theta), sin(theta)), random_samples.xy);
				float2 delta_dir = dir * stepSizePix;

				// Sample the pixels along the direction
				ao += HorizonOcclusion(DTid.xy, delta_dir,
					p, face_normal,
					random_samples.z,
					(int)numSteps,
					bytes_frags_per_pixel, true);
			}

			ao_vr = ao / num_dirs * ao_intensity;
		}
	}
	rw_ao_vr_texture[DTid.xy] = ao_vr;
	//vis_out = float4(TransformPoint(pos_ray_start_ws, g_cbVobj.mat_ws2ts), 1);
	//rw_fragment_blendout[DTid.xy] = float4((float3)ao_vr, 1);
	////if(DTid.x % 200 != 0 || DTid.y % 200 != 0)
	return;
	/**/
}

float normpdf(in float x, in float sigma)
{
	return 0.39894*exp(-0.5*x*x / (sigma*sigma)) / sigma;
}

[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void KB_TO_TEXTURE(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	if (DTid.x >= g_cbCamState.rt_width / 2 || DTid.y >= g_cbCamState.rt_height / 2)
		return;

	const uint k_value = g_cbCamState.k_value;
	uint addr_base_f0 = ((DTid.y * 2 + 0) * g_cbCamState.rt_width + (DTid.x * 2 + 0)) * k_value * 4;
	uint addr_base_f1 = ((DTid.y * 2 + 0) * g_cbCamState.rt_width + (DTid.x * 2 + 1)) * k_value * 4;
	uint addr_base_f2 = ((DTid.y * 2 + 1) * g_cbCamState.rt_width + (DTid.x * 2 + 0)) * k_value * 4;
	uint addr_base_f3 = ((DTid.y * 2 + 1) * g_cbCamState.rt_width + (DTid.x * 2 + 1)) * k_value * 4;

	int frag_cnt_max = max(max(fragment_counter[float2(DTid.x * 2 + 0, DTid.y * 2 + 0)] & 0xFFF, fragment_counter[float2(DTid.x * 2 + 0, DTid.y * 2 + 1)] & 0xFFF),
		max(fragment_counter[float2(DTid.x * 2 + 1, DTid.y * 2 + 0)] & 0xFFF, fragment_counter[float2(DTid.x * 2 + 1, DTid.y * 2 + 1)] & 0xFFF));


	float _zs_avr[LOCAL_SIZE];
	float _opacity_avr[LOCAL_SIZE];
	for (int i = 0; i < LOCAL_SIZE; i++)
	{
		if (i < frag_cnt_max)
		{
			_zs_avr[i] = (LOAD1_KBUF_Z(deep_k_buf, addr_base_f0, i) + LOAD1_KBUF_Z(deep_k_buf, addr_base_f1, i) +
				LOAD1_KBUF_Z(deep_k_buf, addr_base_f2, i) + LOAD1_KBUF_Z(deep_k_buf, addr_base_f3, i)) * 0.25f;
			_opacity_avr[i] = (LOAD1_KBUF_ALPHAF(deep_k_buf, addr_base_f0, i) + LOAD1_KBUF_ALPHAF(deep_k_buf, addr_base_f1, i) +
				LOAD1_KBUF_ALPHAF(deep_k_buf, addr_base_f2, i) + LOAD1_KBUF_ALPHAF(deep_k_buf, addr_base_f3, i)) * 0.25f;
		}
		else
		{
			_zs_avr[i] = FLT_MAX;
			_opacity_avr[i] = 0;
		}
	}

	rw_mip_z_textures[0][DTid.xy] = float4(_zs_avr[0], _zs_avr[1], _zs_avr[2], _zs_avr[3]);
	rw_mip_z_textures[1][DTid.xy] = float4(_zs_avr[4], _zs_avr[5], _zs_avr[6], _zs_avr[7]);
	rw_mip_opacity_textures[0][DTid.xy] = float4(_opacity_avr[0], _opacity_avr[1], _opacity_avr[2], _opacity_avr[3]);
	rw_mip_opacity_textures[1][DTid.xy] = float4(_opacity_avr[4], _opacity_avr[5], _opacity_avr[6], _opacity_avr[7]);
}

[numthreads(GRIDSIZE, GRIDSIZE, 1)]
void KB_SSAO_BLUR(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	if (DTid.x >= g_cbCamState.rt_width || DTid.y >= g_cbCamState.rt_height)
		return;

	// at this moment, only static k buffer is supported.
	const uint k_value = g_cbCamState.k_value;
	uint bytes_per_frag = 4 * NUM_ELES_PER_FRAG;
	uint bytes_frags_per_pixel = k_value * bytes_per_frag; // to do : consider the dynamic scheme. (4 bytes unit)
	uint pixel_id = DTid.y * g_cbCamState.rt_width + DTid.x;
	uint addr_base = pixel_id * bytes_frags_per_pixel;

	int frag_cnt = (int)fragment_counter[DTid.xy];
	uint vr_hit_enc = frag_cnt >> 24;
	frag_cnt = frag_cnt & 0xFFF;

	if (frag_cnt == 0 && vr_hit_enc == 0)
	{
		rw_ao_textures[0][DTid.xy] = (float4)0;
		rw_ao_textures[1][DTid.xy] = (float4)0;
		if (g_cbEnv.env_flag & 0x2)
			rw_ao_vr_texture[DTid.xy] = 0;
		return;
	}

#define SIGMA 20.0
#define BSIGMA 0.5
//#define BSIGMA 0.2
#define MSIZE 15

	//declare stuff
	const int kSize = (MSIZE - 1) / 2;
	float kernel[MSIZE];

	//create the 1-D kernel
	[loop]
	for (int j = 0; j <= kSize; ++j) {
		kernel[kSize + j] = kernel[kSize - j] = normpdf(float(j), SIGMA);
	}

	float v_AOs[LOCAL_SIZE];
	float v_AOs_nb[LOCAL_SIZE];
	float4 aos_tex0 = ao_textures[0][DTid.xy];
	float4 aos_tex1 = ao_textures[1][DTid.xy];
	v_AOs[0] = aos_tex0.x; v_AOs[1] = aos_tex0.y; v_AOs[2] = aos_tex0.z; v_AOs[3] = aos_tex0.w;
	v_AOs[4] = aos_tex1.x; v_AOs[5] = aos_tex1.y; v_AOs[6] = aos_tex1.z; v_AOs[7] = aos_tex1.w;
	float v_AO_vr = ao_vr_texture[DTid.xy];

	float v_AOs_out[LOCAL_SIZE];
	float bZs[LOCAL_SIZE];
	[loop]
	for (j = 0; j < k_value; j++)
	{
		v_AOs_out[j] = 0;
		bZs[j] = 0;
	}
	float v_AO_vr_out = 0;
	float bZ_vr = 0;

	float bZnorm = 1.0 / normpdf(0.0, BSIGMA);
	[loop]
	//for (int k = 0; k < frag_cnt; k++)
	for (int i = -kSize; i <= kSize; ++i)
	{
		for (int j = -kSize; j <= kSize; ++j)
		{
			float4 aos_tex0_nb = ao_textures[0][DTid.xy + uint2(i, j)];
			float4 aos_tex1_nb = ao_textures[1][DTid.xy + uint2(i, j)];
			v_AOs_nb[0] = aos_tex0_nb.x; v_AOs_nb[1] = aos_tex0_nb.y; v_AOs_nb[2] = aos_tex0_nb.z; v_AOs_nb[3] = aos_tex0_nb.w;
			v_AOs_nb[4] = aos_tex1_nb.x; v_AOs_nb[5] = aos_tex1_nb.y; v_AOs_nb[6] = aos_tex1_nb.z; v_AOs_nb[7] = aos_tex1_nb.w;

			float gfactor = kernel[kSize + j] * kernel[kSize + i];
			for (int k = 0; k < frag_cnt; k++)
			{
				float ao_nb = v_AOs_nb[k];
				float bfactor = normpdf(ao_nb - v_AOs[k], BSIGMA) * bZnorm * gfactor;
				bZs[k] += bfactor;
				v_AOs_out[k] += bfactor * ao_nb;
			}

			if (g_cbEnv.env_flag & 0x2)
			{
				float ao_nb = ao_vr_texture[DTid.xy + uint2(i, j)];
				float bfactor = normpdf(ao_nb - v_AO_vr, BSIGMA) * bZnorm * gfactor;
				bZ_vr += bfactor;
				v_AO_vr_out += bfactor * ao_nb;
			}
		}
	}

	float4 vis_ssao = (float4)0;
	for (j = 0; j < frag_cnt; j++)
	{
		float4 fvis = ConvertUIntToFloat4(LOAD1_KBUF_VIS(deep_k_buf, addr_base, j));
		if (bZs[j] > 0)
		{
			v_AOs_out[j] /= bZs[j];
			fvis.rgb *= 1.f - v_AOs_out[j];
		}
		vis_ssao += fvis * (1.f - vis_ssao.a);
	}
	if (g_cbEnv.env_flag & 0x2)
	{
		v_AO_vr_out /= bZ_vr;
		rw_ao_vr_texture[DTid.xy] = v_AO_vr_out;
	}
	rw_fragment_blendout[DTid.xy] = vis_ssao;
	//rw_fragment_blendout[DTid.xy] = float4((float3)(1 - v_AOs_out[1]), 1);
	//rw_fragment_blendout[DTid.xy] = float4((float3)(frag_cnt / 8.f), 1);

	rw_ao_textures[0][DTid.xy] = float4(v_AOs_out[0], v_AOs_out[1], v_AOs_out[2], v_AOs_out[3]);
	rw_ao_textures[1][DTid.xy] = float4(v_AOs_out[4], v_AOs_out[5], v_AOs_out[6], v_AOs_out[7]);
}
