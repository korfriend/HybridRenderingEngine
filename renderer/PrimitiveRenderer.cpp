#include "RendererHeader.h"

//#include <opencv2/imgproc.hpp>
//#include <opencv2/highgui.hpp>

#define __SORT(num, fragments, FRAG) {	   				\
	for (uint j = 1; j < num; ++j)						\
	{													\
		FRAG key = fragments[j];						\
		uint i = j - 1;									\
														\
		while (i >= 0 && fragments[i].z > key.z)		\
		{												\
			fragments[i + 1] = fragments[i];			\
			--i;										\
		}												\
		fragments[i + 1] = key;	}}		


namespace moment_lib
{
	vmfloat4 lerp(vmfloat4 a, vmfloat4 b, float t) {
		return a + t * (b - a);
	}
	float lerp(float a, float b, float t) {
		return a + t * (b - a);
	}
	vmfloat3 mad(float m, vmfloat3 a, float b) {
		return m * a + b;
	}
	float mad(float m, float a, float b) {
		return m * a + b;
	}
	float saturate(float v)
	{
		return max(min(1.f, v), 0.f);
	}

	/*! Given coefficients of a quadratic polynomial A*x^2+B*x+C, this function
	outputs its two real roots.*/
	vmfloat2 solveQuadratic(vmfloat3 coeffs)
	{
		coeffs[1] *= 0.5;

		float x1, x2, tmp;

		tmp = (coeffs[1] * coeffs[1] - coeffs[0] * coeffs[2]);
		if (coeffs[1] >= 0) {
			tmp = sqrt(tmp);
			x1 = (-coeffs[2]) / (coeffs[1] + tmp);
			x2 = (-coeffs[1] - tmp) / coeffs[0];
		}
		else {
			tmp = sqrt(tmp);
			x1 = (-coeffs[1] + tmp) / coeffs[0];
			x2 = coeffs[2] / (-coeffs[1] + tmp);
		}
		return vmfloat2(x1, x2);
	}

	/*! Code taken from the blog "Moments in Graphics" by Christoph Peters.
	http://momentsingraphics.de/?p=105
	This function computes the three real roots of a cubic polynomial
	Coefficient[0]+Coefficient[1]*x+Coefficient[2]*x^2+Coefficient[3]*x^3.*/
	vmfloat3 SolveCubic(vmfloat4 Coefficient) {
		// Normalize the polynomial
		Coefficient.x /= Coefficient.w;
		Coefficient.y /= Coefficient.w;
		Coefficient.z /= Coefficient.w;
		// Divide middle coefficients by three
		Coefficient.y /= 3.0f;
		Coefficient.z /= 3.0f;
		// Compute the Hessian and the discrimant
		vmfloat3 Delta = vmfloat3(
			mad(-Coefficient.z, Coefficient.z, Coefficient.y),
			mad(-Coefficient.y, Coefficient.z, Coefficient.x),
			dot(vmfloat2(Coefficient.z, -Coefficient.y), vmfloat2(Coefficient.x, Coefficient.y))
		);
		float Discriminant = dot(vmfloat2(4.0f * Delta.x, -Delta.y), vmfloat2(Delta.z, Delta.y));
		// Compute coefficients of the depressed cubic 
		// (third is zero, fourth is one)
		vmfloat2 Depressed = vmfloat2(
			mad(-2.0f * Coefficient.z, Delta.x, Delta.y),
			Delta.x
		);
		// Take the cubic root of a normalized complex number
		float Theta = atan2(sqrt(Discriminant), -Depressed.x) / 3.0f;
		vmfloat2 CubicRoot;
		CubicRoot.y = sin(Theta);
		CubicRoot.x = cos(Theta);
		// Compute the three roots, scale appropriately and 
		// revert the depression transform
		vmfloat3 Root = vmfloat3(
			CubicRoot.x,
			dot(vmfloat2(-0.5f, -0.5f * sqrt(3.0f)), CubicRoot),
			dot(vmfloat2(-0.5f, 0.5f * sqrt(3.0f)), CubicRoot)
		);
		Root = mad(2.0f * sqrt(-Depressed.y), Root, -Coefficient.z);
		return Root;
	}

	/*! Given coefficients of a cubic polynomial
	coeffs[0]+coeffs[1]*x+coeffs[2]*x^2+coeffs[3]*x^3 with three real roots,
	this function returns the root of least magnitude.*/
	float solveCubicBlinnSmallest(vmfloat4 coeffs)
	{
		coeffs.x /= coeffs.w;
		coeffs.y /= coeffs.w;
		coeffs.z /= coeffs.w;
		coeffs.y /= 3.0;
		coeffs.z /= 3.0;

		vmfloat3 delta = vmfloat3(mad(-coeffs.z, coeffs.z, coeffs.y), mad(-coeffs.z, coeffs.y, coeffs.x), coeffs.z * coeffs.x - coeffs.y * coeffs.y);
		float discriminant = 4.f * delta.x * delta.z - delta.y * delta.y;

		vmfloat2 depressed = vmfloat2(delta.z, -coeffs.x * delta.y + 2.0 * coeffs.y * delta.z);
		float theta = abs(atan2(coeffs.x * sqrt(discriminant), -depressed.y)) / 3.f;
		vmfloat2 sin_cos;
		sin_cos.x = sin(theta);
		sin_cos.y = cos(theta);
		float tmp = 2.f * sqrt(-depressed.x);
		vmfloat2 x = vmfloat2(tmp * sin_cos.y, tmp * (-0.5f * sin_cos.y - 0.5f * sqrt(3.f) * sin_cos.x));
		vmfloat2 s = (x.x + x.y < 2.0 * coeffs.y) ? vmfloat2(-coeffs.x, x.x + coeffs.y) : vmfloat2(-coeffs.x, x.y + coeffs.y);

		return  s.x / s.y;
	}

	/*! Given coefficients of a quartic polynomial
		coeffs[0]+coeffs[1]*x+coeffs[2]*x^2+coeffs[3]*x^3+coeffs[4]*x^4 with four
		real roots, this function returns all roots.*/
	vmfloat4 solveQuarticNeumark(float coeffs[5])
	{
		// Normalization
		float B = coeffs[3] / coeffs[4];
		float C = coeffs[2] / coeffs[4];
		float D = coeffs[1] / coeffs[4];
		float E = coeffs[0] / coeffs[4];

		// Compute coefficients of the cubic resolvent
		float P = -2.0f * C;
		float Q = C * C + B * D - 4.0f * E;
		float R = D * D + B * B * E - B * C * D;

		// Obtain the smallest cubic root
		float y = solveCubicBlinnSmallest(vmfloat4(R, Q, P, 1.0));

		float BB = B * B;
		float fy = 4.0f * y;
		float BB_fy = BB - fy;

		float Z = C - y;
		float ZZ = Z * Z;
		float fE = 4.f * E;
		float ZZ_fE = ZZ - fE;

		float G, g, H, h;
		// Compute the coefficients of the quadratics adaptively using the two 
		// proposed factorizations by Neumark. Choose the appropriate 
		// factorizations using the heuristic proposed by Herbison-Evans.
		if (y < 0 || (ZZ + fE) * BB_fy > ZZ_fE * (BB + fy)) {
			float tmp = sqrt(BB_fy);
			G = (B + tmp) * 0.5f;
			g = (B - tmp) * 0.5f;

			tmp = (B * Z - 2.0f * D) / (2.0f * tmp);
			H = mad(Z, 0.5f, tmp);
			h = mad(Z, 0.5f, -tmp);
		}
		else {
			float tmp = sqrt(ZZ_fE);
			H = (Z + tmp) * 0.5f;
			h = (Z - tmp) * 0.5f;

			tmp = (B * Z - 2.0f * D) / (2.0f * tmp);
			G = mad(B, 0.5f, tmp);
			g = mad(B, 0.5f, -tmp);
		}
		// Solve the quadratics
		return vmfloat4(solveQuadratic(vmfloat3(1.0, G, H)), solveQuadratic(vmfloat3(1.0, g, h)));
	}

	/*! This function reconstructs the transmittance at the given depth from eight
		normalized power moments and the given zeroth moment.*/
	float computeTransmittanceAtDepthFrom8PowerMoments(float b_0, vmfloat4 b_even, vmfloat4 b_odd, float depth, float bias, float overestimation, const float bias_vector[8])
	{
		float b[8] = { b_odd.x, b_even.x, b_odd.y, b_even.y, b_odd.z, b_even.z, b_odd.w, b_even.w };
		// Bias input data to avoid artifacts
		for (int i = 0; i != 8; ++i) {
			b[i] = lerp(b[i], bias_vector[i], bias);
		}

		float z[5];
		z[0] = depth;

		// Compute a Cholesky factorization of the Hankel matrix B storing only non-trivial entries or related products
		float D22 = mad(-b[0], b[0], b[1]);
		float InvD22 = 1.0f / D22;
		float L32D22 = mad(-b[1], b[0], b[2]);
		float L32 = L32D22 * InvD22;
		float L42D22 = mad(-b[2], b[0], b[3]);
		float L42 = L42D22 * InvD22;
		float L52D22 = mad(-b[3], b[0], b[4]);
		float L52 = L52D22 * InvD22;

		float D33 = mad(-L32, L32D22, mad(-b[1], b[1], b[3]));
		float InvD33 = 1.0f / D33;
		float L43D33 = mad(-L42, L32D22, mad(-b[2], b[1], b[4]));
		float L43 = L43D33 * InvD33;
		float L53D33 = mad(-L52, L32D22, mad(-b[3], b[1], b[5]));
		float L53 = L53D33 * InvD33;

		float D44 = mad(-b[2], b[2], b[5]) - dot(vmfloat2(L42, L43), vmfloat2(L42D22, L43D33));
		float InvD44 = 1.0f / D44;
		float L54D44 = mad(-b[3], b[2], b[6]) - dot(vmfloat2(L52, L53), vmfloat2(L42D22, L43D33));
		float L54 = L54D44 * InvD44;

		float D55 = mad(-b[3], b[3], b[7]) - dot(vmfloat3(L52, L53, L54), vmfloat3(L52D22, L53D33, L54D44));
		float InvD55 = 1.0f / D55;

		// Construct the polynomial whose roots have to be points of support of the
		// Canonical distribution:
		// bz = (1,z[0],z[0]^2,z[0]^3,z[0]^4)^T
		float c[5];
		c[0] = 1.0;
		c[1] = z[0];
		c[2] = c[1] * z[0];
		c[3] = c[2] * z[0];
		c[4] = c[3] * z[0];

		// Forward substitution to solve L*c1 = bz
		c[1] -= b[0];
		c[2] -= mad(L32, c[1], b[1]);
		c[3] -= b[2] + dot(vmfloat2(L42, L43), vmfloat2(c[1], c[2]));
		c[4] -= b[3] + dot(vmfloat3(L52, L53, L54), vmfloat3(c[1], c[2], c[3]));

		// Scaling to solve D*c2 = c1
		//c = c .*[1, InvD22, InvD33, InvD44, InvD55];
		c[1] *= InvD22;
		c[2] *= InvD33;
		c[3] *= InvD44;
		c[4] *= InvD55;

		// Backward substitution to solve L^T*c3 = c2
		c[3] -= L54 * c[4];
		c[2] -= dot(vmfloat2(L53, L43), vmfloat2(c[4], c[3]));
		c[1] -= dot(vmfloat3(L52, L42, L32), vmfloat3(c[4], c[3], c[2]));
		c[0] -= dot(vmfloat4(b[3], b[2], b[1], b[0]), vmfloat4(c[4], c[3], c[2], c[1]));

		// Solve the quartic equation
		vmfloat4 zz = solveQuarticNeumark(c);
		z[1] = zz[0];
		z[2] = zz[1];
		z[3] = zz[2];
		z[4] = zz[3];

		// Compute the absorbance by summing the appropriate weights
		vmfloat4 weigth_factor;
		weigth_factor.x = z[1] <= z[0];
		weigth_factor.y = z[2] <= z[0];
		weigth_factor.z = z[3] <= z[0];
		weigth_factor.w = z[4] <= z[0];
		// Construct an interpolation polynomial
		float f0 = overestimation;
		float f1 = weigth_factor[0];
		float f2 = weigth_factor[1];
		float f3 = weigth_factor[2];
		float f4 = weigth_factor[3];
		float f01 = (f1 - f0) / (z[1] - z[0]);
		float f12 = (f2 - f1) / (z[2] - z[1]);
		float f23 = (f3 - f2) / (z[3] - z[2]);
		float f34 = (f4 - f3) / (z[4] - z[3]);
		float f012 = (f12 - f01) / (z[2] - z[0]);
		float f123 = (f23 - f12) / (z[3] - z[1]);
		float f234 = (f34 - f23) / (z[4] - z[2]);
		float f0123 = (f123 - f012) / (z[3] - z[0]);
		float f1234 = (f234 - f123) / (z[4] - z[1]);
		float f01234 = (f1234 - f0123) / (z[4] - z[0]);

		float Polynomial_0;
		vmfloat4 Polynomial;
		// f0123 + f01234 * (z - z3)
		Polynomial_0 = mad(-f01234, z[3], f0123);
		Polynomial[0] = f01234;
		// * (z - z2) + f012
		Polynomial[1] = Polynomial[0];
		Polynomial[0] = mad(-Polynomial[0], z[2], Polynomial_0);
		Polynomial_0 = mad(-Polynomial_0, z[2], f012);
		// * (z - z1) + f01
		Polynomial[2] = Polynomial[1];
		Polynomial[1] = mad(-Polynomial[1], z[1], Polynomial[0]);
		Polynomial[0] = mad(-Polynomial[0], z[1], Polynomial_0);
		Polynomial_0 = mad(-Polynomial_0, z[1], f01);
		// * (z - z0) + f1
		Polynomial[3] = Polynomial[2];
		Polynomial[2] = mad(-Polynomial[2], z[0], Polynomial[1]);
		Polynomial[1] = mad(-Polynomial[1], z[0], Polynomial[0]);
		Polynomial[0] = mad(-Polynomial[0], z[0], Polynomial_0);
		Polynomial_0 = mad(-Polynomial_0, z[0], f0);
		float absorbance = Polynomial_0 + dot(Polynomial, vmfloat4(b[0], b[1], b[2], b[3]));
		// Turn the normalized absorbance into transmittance
		return saturate(exp(-b_0 * absorbance));
	}


	/*! This function reconstructs the transmittance at the given depth from six
		normalized power moments and the given zeroth moment.*/
	float computeTransmittanceAtDepthFrom6PowerMoments(float b_0, vmfloat3 b_even, vmfloat3 b_odd, float depth, float bias, float overestimation, const float bias_vector[6])
	{
		float b[6] = { b_odd.x, b_even.x, b_odd.y, b_even.y, b_odd.z, b_even.z };
		// Bias input data to avoid artifacts
		for (int i = 0; i != 6; ++i) {
			b[i] = lerp(b[i], bias_vector[i], bias);
		}

		vmfloat4 z;
		z[0] = depth;

		// Compute a Cholesky factorization of the Hankel matrix B storing only non-
		// trivial entries or related products
		float InvD11 = 1.0f / mad(-b[0], b[0], b[1]);
		float L21D11 = mad(-b[0], b[1], b[2]);
		float L21 = L21D11 * InvD11;
		float D22 = mad(-L21D11, L21, mad(-b[1], b[1], b[3]));
		float L31D11 = mad(-b[0], b[2], b[3]);
		float L31 = L31D11 * InvD11;
		float InvD22 = 1.0f / D22;
		float L32D22 = mad(-L21D11, L31, mad(-b[1], b[2], b[4]));
		float L32 = L32D22 * InvD22;
		float D33 = mad(-b[2], b[2], b[5]) - dot(vmfloat2(L31D11, L32D22), vmfloat2(L31, L32));
		float InvD33 = 1.0f / D33;

		// Construct the polynomial whose roots have to be points of support of the 
		// canonical distribution: bz=(1,z[0],z[0]*z[0],z[0]*z[0]*z[0])^T
		vmfloat4 c;
		c[0] = 1.0f;
		c[1] = z[0];
		c[2] = c[1] * z[0];
		c[3] = c[2] * z[0];
		// Forward substitution to solve L*c1=bz
		c[1] -= b[0];
		c[2] -= mad(L21, c[1], b[1]);
		c[3] -= b[2] + dot(vmfloat2(L31, L32), vmfloat2(c.y, c.z));
		// Scaling to solve D*c2=c1
		c.y *= InvD11;
		c.z *= InvD22;
		c.w *= InvD33;
		// Backward substitution to solve L^T*c3=c2
		c[2] -= L32 * c[3];
		c[1] -= dot(vmfloat2(L21, L31), vmfloat2(c.z, c.w));
		c[0] -= dot(vmfloat3(b[0], b[1], b[2]), vmfloat3(c.y, c.z, c.w));

		// Solve the cubic equation
		memcpy(&z[1], &SolveCubic(c), sizeof(vmfloat3));

		// Compute the absorbance by summing the appropriate weights
		vmfloat4 weigth_factor;
		weigth_factor[0] = overestimation;
		*(vmfloat3*)&weigth_factor[1] = (z.y > z.x && z.z > z.x && z.w > z.x) ? vmfloat3(0.0f, 0.0f, 0.0f) : vmfloat3(1.0f, 1.0f, 1.0f);
		// Construct an interpolation polynomial
		float f0 = weigth_factor[0];
		float f1 = weigth_factor[1];
		float f2 = weigth_factor[2];
		float f3 = weigth_factor[3];
		float f01 = (f1 - f0) / (z[1] - z[0]);
		float f12 = (f2 - f1) / (z[2] - z[1]);
		float f23 = (f3 - f2) / (z[3] - z[2]);
		float f012 = (f12 - f01) / (z[2] - z[0]);
		float f123 = (f23 - f12) / (z[3] - z[1]);
		float f0123 = (f123 - f012) / (z[3] - z[0]);
		vmfloat4 polynomial;
		// f012+f0123 *(z-z2)
		polynomial[0] = mad(-f0123, z[2], f012);
		polynomial[1] = f0123;
		// *(z-z1) +f01
		polynomial[2] = polynomial[1];
		polynomial[1] = mad(polynomial[1], -z[1], polynomial[0]);
		polynomial[0] = mad(polynomial[0], -z[1], f01);
		// *(z-z0) +f0
		polynomial[3] = polynomial[2];
		polynomial[2] = mad(polynomial[2], -z[0], polynomial[1]);
		polynomial[1] = mad(polynomial[1], -z[0], polynomial[0]);
		polynomial[0] = mad(polynomial[0], -z[0], f0);
		float absorbance = dot(polynomial, vmfloat4(1.0, b[0], b[1], b[2]));
		// Turn the normalized absorbance into transmittance
		return saturate(exp(-b_0 * absorbance));
	}

	/*! This function reconstructs the transmittance at the given depth from four
		normalized power moments and the given zeroth moment.*/
	float computeTransmittanceAtDepthFrom4PowerMoments(float b_0, vmfloat2 b_even, vmfloat2 b_odd, float depth, float bias, float overestimation, vmfloat4 bias_vector)
	{
		vmfloat4 b = vmfloat4(b_odd.x, b_even.x, b_odd.y, b_even.y);
		// Bias input data to avoid artifacts
		b = lerp(b, bias_vector, bias);
		vmfloat3 z;
		z[0] = depth;

		// Compute a Cholesky factorization of the Hankel matrix B storing only non-
		// trivial entries or related products
		float L21D11 = mad(-b[0], b[1], b[2]);
		float D11 = mad(-b[0], b[0], b[1]);
		float InvD11 = 1.0f / D11;
		float L21 = L21D11 * InvD11;
		float SquaredDepthVariance = mad(-b[1], b[1], b[3]);
		float D22 = mad(-L21D11, L21, SquaredDepthVariance);

		// Obtain a scaled inverse image of bz=(1,z[0],z[0]*z[0])^T
		vmfloat3 c = vmfloat3(1.0f, z[0], z[0] * z[0]);
		// Forward substitution to solve L*c1=bz
		c[1] -= b.x;
		c[2] -= b.y + L21 * c[1];
		// Scaling to solve D*c2=c1
		c[1] *= InvD11;
		c[2] /= D22;
		// Backward substitution to solve L^T*c3=c2
		c[1] -= L21 * c[2];
		c[0] -= dot(vmfloat2(c.y, c.z), vmfloat2(b.x, b.y));
		// Solve the quadratic equation c[0]+c[1]*z+c[2]*z^2 to obtain solutions 
		// z[1] and z[2]
		float InvC2 = 1.0f / c[2];
		float p = c[1] * InvC2;
		float q = c[0] * InvC2;
		float D = (p * p * 0.25f) - q;
		float r = sqrt(D);
		z[1] = -p * 0.5f - r;
		z[2] = -p * 0.5f + r;
		// Compute the absorbance by summing the appropriate weights
		vmfloat3 polynomial;
		vmfloat3 weight_factor = vmfloat3(overestimation, (z[1] < z[0]) ? 1.0f : 0.0f, (z[2] < z[0]) ? 1.0f : 0.0f);
		float f0 = weight_factor[0];
		float f1 = weight_factor[1];
		float f2 = weight_factor[2];
		float f01 = (f1 - f0) / (z[1] - z[0]);
		float f12 = (f2 - f1) / (z[2] - z[1]);
		float f012 = (f12 - f01) / (z[2] - z[0]);
		polynomial[0] = f012;
		polynomial[1] = polynomial[0];
		polynomial[0] = f01 - polynomial[0] * z[1];
		polynomial[2] = polynomial[1];
		polynomial[1] = polynomial[0] - polynomial[1] * z[0];
		polynomial[0] = f0 - polynomial[0] * z[0];
		float absorbance = polynomial[0] + dot(vmfloat2(b.x, b.y), vmfloat2(polynomial.y, polynomial.z));
		// Turn the normalized absorbance into transmittance
		return saturate(exp(-b_0 * absorbance));
	}

#define asfloat *(float*)&

	void resolveMoments(float& transmittance_at_depth, float& total_transmittance, const float depth, const vmint2 sv_pos,
		const int rt_width, const int buf_stride, const uint* moment_container_buf,
		const int num_moment, const bool is_trigonometric, const MomentOIT& mo)
	{
		const float moment_bias = mo.moment_bias;
		const float overestimation = mo.overestimation;

		//int4 idx0 = int4(sv_pos, 0, 0);
		//int4 idx1 = idx0;
		//idx1[2] = 1;
		int addr_base = (sv_pos.y * rt_width + sv_pos.x) * buf_stride;

		transmittance_at_depth = 1;
		total_transmittance = 1;

		//float b_0 = zeroth_moment.Load(idx0).x;
		float b_0 = asfloat(moment_container_buf[addr_base + 0]);
		if (b_0 - 0.00100050033f < 0) return;
		total_transmittance = exp(-b_0);

		if (num_moment == 4)
		{
			//vmfloat4 b_1234 = moments.Load(idx0).xyzw;
			vmfloat4 b_1234;
			b_1234.x = asfloat(moment_container_buf[addr_base + 1]);
			b_1234.y = asfloat(moment_container_buf[addr_base + 2]);
			b_1234.z = asfloat(moment_container_buf[addr_base + 3]);
			b_1234.w = asfloat(moment_container_buf[addr_base + 4]);

			vmfloat2 b_even = vmfloat2(b_1234.y, b_1234.w);
			vmfloat2 b_odd = vmfloat2(b_1234.x, b_1234.z);

			b_even /= b_0;
			b_odd /= b_0;

			const vmfloat4 bias_vector = vmfloat4(0, 0.375, 0, 0.375);
			transmittance_at_depth = computeTransmittanceAtDepthFrom4PowerMoments(b_0, b_even, b_odd, depth, moment_bias, overestimation, bias_vector);
		}
		else if (num_moment == 6)
		{
			//int4 idx2 = idx0;
			//idx2[2] = 2;

			vmfloat2 b_12;// = moments.Load(idx0).xy;
			b_12.x = asfloat(moment_container_buf[addr_base + 1]);
			b_12.y = asfloat(moment_container_buf[addr_base + 2]);

			vmfloat2 b_34;// = moments.Load(idx1).xy;
			vmfloat2 b_56;// = moments.Load(idx2).xy;
			b_34.x = asfloat(moment_container_buf[addr_base + 3]);
			b_34.y = asfloat(moment_container_buf[addr_base + 4]);
			b_56.x = asfloat(moment_container_buf[addr_base + 5]);
			b_56.y = asfloat(moment_container_buf[addr_base + 6]);

			vmfloat3 b_even = vmfloat3(b_12.y, b_34.y, b_56.y);
			vmfloat3 b_odd = vmfloat3(b_12.x, b_34.x, b_56.x);

			b_even /= b_0;
			b_odd /= b_0;

			const float bias_vector[6] = { 0.f, 0.48f, 0.f, 0.451f, 0.f, 0.45f };
			transmittance_at_depth = computeTransmittanceAtDepthFrom6PowerMoments(b_0, b_even, b_odd, depth, moment_bias, overestimation, bias_vector);
		}
		else if (num_moment == 8)
		{
			vmfloat4 b_even;// = moments.Load(idx0);
			b_even.x = asfloat(moment_container_buf[addr_base + 1]);
			b_even.y = asfloat(moment_container_buf[addr_base + 2]);
			b_even.z = asfloat(moment_container_buf[addr_base + 3]);
			b_even.w = asfloat(moment_container_buf[addr_base + 4]);
			vmfloat4 b_odd;// = moments.Load(idx1);
			b_odd.x = asfloat(moment_container_buf[addr_base + 5]);
			b_odd.y = asfloat(moment_container_buf[addr_base + 6]);
			b_odd.z = asfloat(moment_container_buf[addr_base + 7]);
			b_odd.w = asfloat(moment_container_buf[addr_base + 8]);

			b_even /= b_0;
			b_odd /= b_0;
			const float bias_vector[8] = { 0.f, 0.75f, 0.f, 0.67666666666666664f, 0.f, 0.63f, 0.f, 0.60030303030303034f };
			transmittance_at_depth = computeTransmittanceAtDepthFrom8PowerMoments(b_0, b_even, b_odd, depth, moment_bias, overestimation, bias_vector);
		}

	}




}

/*! This utility function turns an angle from 0 to 2*pi into a parameter that
	grows monotonically as function of the input. It is designed to be
	efficiently computable from a point on the unit circle and must match the
	function in TrigonometricMomentMath.hlsli.
   \param pOutMaxParameter Set to the maximal possible output value.*/
float circleToParameter(float angle, float* pOutMaxParameter = nullptr) {
#ifndef M_PI
#define M_PI 3.14159265358979323f
#endif
	float x = std::cos(angle);
	float y = std::sin(angle);
	float result = std::abs(y) - std::abs(x);
	result = (x < 0.0f) ? (2.0f - result) : result;
	result = (y < 0.0f) ? (6.0f - result) : result;
	result += (angle >= 2.0f * M_PI) ? 8.0f : 0.0f;
	if (pOutMaxParameter) {
		(*pOutMaxParameter) = 7.0f;
	}
	return result;
}


/*! Given an angle in radians providing the size of the wrapping zone, this
	function computes all constants required by the shader.*/
void computeWrappingZoneParameters(float p_out_wrapping_zone_parameters[4], float new_wrapping_zone_angle = 0.1f * M_PI) {
	p_out_wrapping_zone_parameters[0] = new_wrapping_zone_angle;
	p_out_wrapping_zone_parameters[1] = M_PI - 0.5f * new_wrapping_zone_angle;
	if (new_wrapping_zone_angle <= 0.0f) {
		p_out_wrapping_zone_parameters[2] = 0.0f;
		p_out_wrapping_zone_parameters[3] = 0.0f;
	}
	else {
		float zone_end_parameter;
		float zone_begin_parameter = circleToParameter(2.0f * M_PI - new_wrapping_zone_angle, &zone_end_parameter);
		p_out_wrapping_zone_parameters[2] = 1.0f / (zone_end_parameter - zone_begin_parameter);
		p_out_wrapping_zone_parameters[3] = 1.0f - zone_end_parameter * p_out_wrapping_zone_parameters[2];
	}
}

bool RenderSrOIT(VmFnContainer* _fncontainer,
	VmGpuManager* gpu_manager,
	grd_helper::GpuDX11CommonParameters* dx11CommonParams,
	LocalProgress* progress,
	double* run_time_ptr)
{
	using namespace std::chrono;
	//((std::mutex*)HDx11GetMutexGpuCriticalPath())->lock();

#pragma region // Parameter Setting //
	VmIObject* iobj = _fncontainer->fnParams.GetParam("_VmIObject*_RenderOut", (VmIObject*)NULL);
	iobj->SetObjParam("NUM_SECOND_LAYER_OBJECTS", (int)0);

	int k_value_old = iobj->GetObjParam("_int_NumK", (int)8);
	int k_value = _fncontainer->fnParams.GetParam("_int_NumK", k_value_old);
	iobj->SetObjParam("_int_NumK", k_value);

	int num_moments_old = iobj->GetObjParam("_int_NumQueueLayers", (int)8);
	int num_moments = _fncontainer->fnParams.GetParam("_int_NumQueueLayers", num_moments_old);
	int num_safe_loopexit = _fncontainer->fnParams.GetParam("_int_SpinLockSafeLoops", (int)10000000);
	bool is_final_renderer = _fncontainer->fnParams.GetParam("_bool_IsFinalRenderer", true);
	//double v_discont_depth = _fncontainer->fnParams.GetParam("_float_DiscontDepth", -1.0);
	float merging_beta = _fncontainer->fnParams.GetParam("_float_MergingBeta", 0.5f);
	bool blur_SSAO = _fncontainer->fnParams.GetParam("_bool_BlurSSAO", true);

	bool apply_fragmerge = _fncontainer->fnParams.GetParam("_bool_ApplyFragMerge", true);
	MFR_MODE mode_OIT = (MFR_MODE)_fncontainer->fnParams.GetParam("_int_OitMode", (int)MFR_MODE::DYNAMIC_FB);
	mode_OIT = (MFR_MODE)min((int)mode_OIT, (int)MFR_MODE::MOMENT);
#ifdef DX10_0
	mode_OIT = MFR_MODE::NONE;
#endif

	//if (mode_OIT == MFR_MODE::STATIC_KB_FM) apply_fragmerge = true;

	int buf_ex_scale = _fncontainer->fnParams.GetParam("_int_BufExScale", (int)8); // scaling the capacity of the k-buffer for _bool_PixelTransmittance
	bool use_blending_option_MomentOIT = _fncontainer->fnParams.GetParam("_bool_UseBlendingOptionMomentOIT", false);
	bool check_pixel_transmittance = _fncontainer->fnParams.GetParam("_bool_PixelTransmittance", false);
	//vr_level = 2;
	vmfloat4 default_phong_lighting_coeff = vmfloat4(0.2, 1.0, 0.5, 5); // Emission, Diffusion, Specular, Specular Power

	float default_point_thickness = _fncontainer->fnParams.GetParam("_float_PointThickness", 3.0f);
	float default_surfel_size = _fncontainer->fnParams.GetParam("_float_SurfelSize", 0.0f);
	float default_line_thickness = _fncontainer->fnParams.GetParam("_float_LineThickness", 2.0f);
	vmfloat3 default_color_cmmobj = _fncontainer->fnParams.GetParam("_float3_CmmGlobalColor", vmfloat3(-1, -1, -1));
	bool use_spinlock_pixsynch = _fncontainer->fnParams.GetParam("_bool_UseSpinLock", false);
	bool is_ghost_mode = _fncontainer->fnParams.GetParam("_bool_GhostEffect", false);
	bool is_rgba = _fncontainer->fnParams.GetParam("_bool_IsRGBA", false); // false means bgra

	int camClipMode = _fncontainer->fnParams.GetParam("_int_ClippingMode", (int)0);
	vmfloat3 camClipPlanePos = _fncontainer->fnParams.GetParam("_float3_PosClipPlaneWS", vmfloat3(0));
	vmfloat3 camClipPlaneDir = _fncontainer->fnParams.GetParam("_float3_VecClipPlaneWS", vmfloat3(0));
	vmmat44f camClipMatWS2BS = _fncontainer->fnParams.GetParam("_matrix44f_MatrixClipWS2BS", vmmat44f(1));

	bool is_system_out = false;
	if (is_final_renderer) is_system_out = true;

	bool only_surface_test = _fncontainer->fnParams.GetParam("_bool_OnlySurfaceTest", false);
	bool test_consoleout = _fncontainer->fnParams.GetParam("_bool_TestConsoleOut", false);
	bool test_fps_profiling = _fncontainer->fnParams.GetParam("_bool_TestFpsProfile", false);
	auto test_out = [&test_consoleout](const string& _message)
	{
		if (test_consoleout)
			cout << _message << endl;
	};

	float sample_rate = _fncontainer->fnParams.GetParam("_float_UserSampleRate", 0.0f);
	if (sample_rate <= 0) sample_rate = 1.0f;
	bool apply_samplerate2gradient = _fncontainer->fnParams.GetParam("_bool_ApplySampleRateToGradient", false);
	bool reload_hlsl_objs = _fncontainer->fnParams.GetParam("_bool_ReloadHLSLObjFiles", false);

	int __BLOCKSIZE = _fncontainer->fnParams.GetParam("_int_GpuThreadBlockSize", (int)4);
	float v_thickness =  _fncontainer->fnParams.GetParam("_float_VZThickness", 0.0f);
	//if (v_thickness > 0) {
	//	vmlog::LogInfo("vzthickness : " + std::to_string(v_thickness));
	//}
	float gi_v_thickness = _fncontainer->fnParams.GetParam("_float_GIVZThickness", v_thickness);
	float scale_z_res = _fncontainer->fnParams.GetParam("_float_zResScale", 1.0f);

	bool is_picking_routine = _fncontainer->fnParams.GetParam("_bool_IsPickingRoutine", false);
	bool pickPrimitive = false; 
	if (is_picking_routine) {
#ifdef DX10_0
		mode_OIT = MFR_MODE::NONE;
		pickPrimitive = true;
#else
		mode_OIT = DYNAMIC_FB;
		pickPrimitive = _fncontainer->fnParams.GetParam("_bool_PickPrimitive", false);
#endif
		//gi_v_thickness = v_thickness = 0.0000001f;
	}
	vmint2 picking_pos_ss = _fncontainer->fnParams.GetParam("_int2_PickingPosSS", vmint2(-1, -1));

	int outline_thickness = _fncontainer->fnParams.GetParam("_int_SilhouetteThickness", (int)0);
	float outline_depthThres = _fncontainer->fnParams.GetParam("_float_SilhouetteDepthThres", 10000.f);
	vmfloat3 outline_color = _fncontainer->fnParams.GetParam("_float3_SilhouetteColor", vmfloat3(1));
	bool outline_fadeEffect = _fncontainer->fnParams.GetParam("_bool_SilhouetteFadeEffect", true); 

	int i_test_shader = (int)_fncontainer->fnParams.GetParam("_int_ShaderTest", (int)0);

	int forcedMaterialColorMode = (int)_fncontainer->fnParams.GetParam("_int_ForcedMaterialColorMode", (int)0);

	VmLight* light = _fncontainer->fnParams.GetParam("_VmLight*_LightSource", (VmLight*)NULL);
	VmLens* lens = _fncontainer->fnParams.GetParam("_VmLens*_CamLens", (VmLens*)NULL);
	LightSource light_src;
	GlobalLighting global_lighting;
	LensEffect lens_effect;
	if (light) {
		light_src.is_on_camera = light->is_on_camera;
		light_src.is_pointlight = light->is_pointlight;
		light_src.light_pos = light->pos;
		light_src.light_dir = light->dir;
		light_src.light_ambient_color = vmfloat3(1.f);
		light_src.light_diffuse_color = vmfloat3(1.f);
		light_src.light_specular_color = vmfloat3(1.f);

		global_lighting.apply_ssao = light->effect_ssao.is_on_ssao;
		global_lighting.ssao_r_kernel = light->effect_ssao.kernel_r;
		global_lighting.ssao_num_steps = light->effect_ssao.num_steps;
		global_lighting.ssao_num_dirs = light->effect_ssao.num_dirs;
		global_lighting.ssao_tangent_bias = light->effect_ssao.tangent_bias;
		global_lighting.ssao_blur = light->effect_ssao.smooth_filter;
		global_lighting.ssao_intensity = light->effect_ssao.ao_power;
		global_lighting.ssao_debug = _fncontainer->fnParams.GetParam("_int_SSAOOutput", (int)0);
	}
	if (lens) {
		lens_effect.apply_ssdof = lens->apply_ssdof;
		lens_effect.dof_focus_z = lens->dof_focus_z;
		lens_effect.dof_lens_F = lens->dof_lens_F;
		lens_effect.dof_lens_r = lens->dof_lens_r;
		lens_effect.dof_ray_num_samples = lens->dof_ray_num_samples;
	}
#pragma endregion

#pragma region // Shader Setting
	// Shader Re-Compile Setting //
	if (reload_hlsl_objs)
	{
		char ownPth[2048];
		GetModuleFileNameA(NULL, ownPth, (sizeof(ownPth)));
		string exe_path = ownPth;
		size_t pos = 0;
		std::string token;
		string delimiter = "\\";
		string hlslobj_path = "";
		while ((pos = exe_path.find(delimiter)) != std::string::npos) {
			token = exe_path.substr(0, pos);
			if (token.find(".exe") != std::string::npos) break;
			hlslobj_path += token + "\\";
			exe_path.erase(0, pos + delimiter.length());
		}


		//hlslobj_path += "..\\..\\VmModuleProjects\\plugin_gpudx11_renderer\\";
		//hlslobj_path += "..\\..\\VmModuleProjects\\hybrid_rendering_engine\\";
		hlslobj_path += "..\\..\\VmProjects\\hybrid_rendering_engine\\";
		//cout << hlslobj_path << endl;

		string enginePath;
		if (grd_helper::GetEnginePath(enginePath)) {
			hlslobj_path = enginePath;
		}
		string hlslobj_path_4_0 = hlslobj_path + "shader_compiled_objs_4_0\\";

#ifdef DX10_0
		hlslobj_path += "shader_compiled_objs_4_0\\";
#else
		hlslobj_path += "shader_compiled_objs\\";
#endif

		string prefix_path = hlslobj_path;
		vmlog::LogInfo("RELOAD HLSL _ SR renderer");

		dx11CommonParams->dx11DeviceImmContext->VSSetShader(NULL, NULL, 0);
		dx11CommonParams->dx11DeviceImmContext->GSSetShader(NULL, NULL, 0);
		dx11CommonParams->dx11DeviceImmContext->PSSetShader(NULL, NULL, 0);
		dx11CommonParams->dx11DeviceImmContext->CSSetShader(NULL, NULL, 0);

#define SET_VS(NAME, __S) dx11CommonParams->safe_set_res(grd_helper::COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, NAME), __S, true)
#define SET_PS(NAME, __S) dx11CommonParams->safe_set_res(grd_helper::COMRES_INDICATOR(GpuhelperResType::PIXEL_SHADER, NAME), __S, true)
#define SET_CS(NAME, __S) dx11CommonParams->safe_set_res(grd_helper::COMRES_INDICATOR(GpuhelperResType::COMPUTE_SHADER, NAME), __S, true)
#define SET_GS(NAME, __S) dx11CommonParams->safe_set_res(grd_helper::COMRES_INDICATOR(GpuhelperResType::GEOMETRY_SHADER, NAME), __S, true)
#define GETRASTER(NAME) dx11CommonParams->get_rasterizer(#NAME)

#ifdef DX10_0
#define VS_NUM 5
#define GS_NUM 4
#define PS_NUM 10
#else
#define VS_NUM 6
#define GS_NUM 4
#define PS_NUM 83
#define CS_NUM 32
#endif

#ifdef DX10_0
		string strNames_VS[VS_NUM] = {
			   "SR_OIT_P_vs_4_0"
			  ,"SR_OIT_PN_vs_4_0"
			  ,"SR_OIT_PT_vs_4_0"
			  ,"SR_OIT_PNT_vs_4_0"
			  ,"SR_OIT_PTTT_vs_4_0"
		};
#else
		string strNames_VS[VS_NUM] = {
			   "SR_OIT_P_vs_5_0"
			  ,"SR_OIT_PN_vs_5_0"
			  ,"SR_OIT_PT_vs_5_0"
			  ,"SR_OIT_PNT_vs_5_0"
			  ,"SR_OIT_PTTT_vs_5_0"
			  ,"SR_OIT_IDX_vs_5_0"
		};
#endif

		for (int i = 0; i < VS_NUM; i++)
		{
			string strName = strNames_VS[i];

			FILE* pFile;
			if (fopen_s(&pFile, (prefix_path + strName).c_str(), "rb") == 0)
			{
				fseek(pFile, 0, SEEK_END);
				ullong ullFileSize = ftell(pFile);
				fseek(pFile, 0, SEEK_SET);
				byte* pyRead = new byte[ullFileSize];
				fread(pyRead, sizeof(byte), ullFileSize, pFile);
				fclose(pFile);

				ID3D11VertexShader* dx11VShader = NULL;
				if (dx11CommonParams->dx11Device->CreateVertexShader(pyRead, ullFileSize, NULL, &dx11VShader) != S_OK)
				{
					VMERRORMESSAGE("SHADER COMPILE FAILURE!");
				}
				else
				{
					SET_VS(strName, dx11VShader);
				}
				VMSAFE_DELETEARRAY(pyRead);
			}
		}
		/**/
#ifdef DX10_0
		string strNames_GS[GS_NUM] = {
			   "GS_ThickPoints_gs_4_0"
			  ,"GS_SurfelPoints_gs_4_0"
			  ,"GS_ThickLines_gs_4_0"
			  ,"GS_PickingBasic_gs_4_0"
		};
#else
		string strNames_GS[GS_NUM] = {
			   "GS_ThickPoints_gs_5_0"
			  ,"GS_SurfelPoints_gs_5_0"
			  ,"GS_ThickLines_gs_5_0"
			  ,"GS_TriNormal_gs_5_0"
		};
#endif
		int testNum = sizeof(strNames_GS) / sizeof(string);

		for (int i = 0; i < GS_NUM; i++)
		{
			string strName = strNames_GS[i];

			FILE* pFile;
			if (fopen_s(&pFile, (prefix_path + strName).c_str(), "rb") == 0)
			{
				fseek(pFile, 0, SEEK_END);
				ullong ullFileSize = ftell(pFile);
				fseek(pFile, 0, SEEK_SET);
				byte* pyRead = new byte[ullFileSize];
				fread(pyRead, sizeof(byte), ullFileSize, pFile);
				fclose(pFile);

				ID3D11GeometryShader* dx11GShader = NULL;
				if (dx11CommonParams->dx11Device->CreateGeometryShader(pyRead, ullFileSize, NULL, &dx11GShader) != S_OK)
				{
					VMERRORMESSAGE("SHADER COMPILE FAILURE!");
				}
				else
				{
					SET_GS(strName, dx11GShader);
				}
				VMSAFE_DELETEARRAY(pyRead);
			}
		}

		{
			string strName = "GS_PickingBasic_gs_4_0";
			FILE* pFile;
			if (fopen_s(&pFile, (hlslobj_path_4_0 + strName).c_str(), "rb") == 0)
			{
				fseek(pFile, 0, SEEK_END);
				ullong ullFileSize = ftell(pFile);
				fseek(pFile, 0, SEEK_SET);
				byte* pyRead = new byte[ullFileSize];
				fread(pyRead, sizeof(byte), ullFileSize, pFile);
				fclose(pFile);

				// https://learn.microsoft.com/en-us/windows/win32/direct3d11/d3d10-graphics-programming-guide-output-stream-stage-getting-started
				// https://strange-cpp.tistory.com/101
				D3D11_SO_DECLARATION_ENTRY pDecl[] =
				{
					// semantic name, semantic index, start component, component count, output slot
					{ 0, "TEXCOORD", 0, 0, 3, 0 },   // output 
				};
				int numEntries = sizeof(pDecl) / sizeof(D3D11_SO_DECLARATION_ENTRY);
				uint bufferStrides[] = { sizeof(vmfloat3) };
				int numStrides = sizeof(bufferStrides) / sizeof(uint);
				ID3D11GeometryShader* dx11GShader = NULL;
				if (dx11CommonParams->dx11Device->CreateGeometryShaderWithStreamOutput(
					pyRead, ullFileSize, pDecl, numEntries, bufferStrides, numStrides, D3D11_SO_NO_RASTERIZED_STREAM, NULL,
					(ID3D11GeometryShader**)&dx11GShader) != S_OK)
				{
					VMERRORMESSAGE("SHADER COMPILE FAILURE!");
				}
				else
				{
					SET_GS(strName, dx11GShader);
				}
				VMSAFE_DELETEARRAY(pyRead);
			}
		}

#ifdef DX10_0
		string strNames_PS[PS_NUM] = {
			 "SR_SINGLE_LAYER_ps_4_0"
			,"WRITE_DEPTH_ps_4_0"

			,"SR_BASIC_PHONGBLINN_ps_4_0"
			,"SR_BASIC_DASHEDLINE_ps_4_0"
			,"SR_BASIC_MULTITEXTMAPPING_ps_4_0"
			,"SR_BASIC_TEXTMAPPING_ps_4_0"
			,"SR_BASIC_TEXTUREIMGMAP_ps_4_0"
			,"SR_BASIC_VOLUMEMAP_ps_4_0"
			,"SR_BASIC_VOLUME_DIST_MAP_ps_4_0"
			,"SR_QUAD_OUTLINE_ps_4_0"
		};
#else
		string strNames_PS[PS_NUM] = {
			   "SR_OIT_FILL_SKBTZ_PHONGBLINN_ps_5_0"
			  ,"SR_OIT_FILL_SKBTZ_DASHEDLINE_ps_5_0"
			  ,"SR_OIT_FILL_SKBTZ_MULTITEXTMAPPING_ps_5_0"
			  ,"SR_OIT_FILL_SKBTZ_TEXTMAPPING_ps_5_0"
			  ,"SR_OIT_FILL_SKBTZ_TEXTUREIMGMAP_ps_5_0"

			  ,"SR_OIT_FILL_SKBTZ_PHONGBLINN_ROV_ps_5_0"
			  ,"SR_OIT_FILL_SKBTZ_DASHEDLINE_ROV_ps_5_0"
			  ,"SR_OIT_FILL_SKBTZ_MULTITEXTMAPPING_ROV_ps_5_0"
			  ,"SR_OIT_FILL_SKBTZ_TEXTMAPPING_ROV_ps_5_0"
			  ,"SR_OIT_FILL_SKBTZ_TEXTUREIMGMAP_ROV_ps_5_0"

			   "SR_OIT_FILL_SKBT_PHONGBLINN_ps_5_0"
			  ,"SR_OIT_FILL_SKBT_DASHEDLINE_ps_5_0"
			  ,"SR_OIT_FILL_SKBT_MULTITEXTMAPPING_ps_5_0"
			  ,"SR_OIT_FILL_SKBT_TEXTMAPPING_ps_5_0"
			  ,"SR_OIT_FILL_SKBT_TEXTUREIMGMAP_ps_5_0"

			  ,"SR_OIT_FILL_SKBT_PHONGBLINN_ROV_ps_5_0"
			  ,"SR_OIT_FILL_SKBT_DASHEDLINE_ROV_ps_5_0"
			  ,"SR_OIT_FILL_SKBT_MULTITEXTMAPPING_ROV_ps_5_0"
			  ,"SR_OIT_FILL_SKBT_TEXTMAPPING_ROV_ps_5_0"
			  ,"SR_OIT_FILL_SKBT_TEXTUREIMGMAP_ROV_ps_5_0"

			  ,"SR_OIT_FILL_DKBTZ_PHONGBLINN_ps_5_0"
			  ,"SR_OIT_FILL_DKBTZ_DASHEDLINE_ps_5_0"
			  ,"SR_OIT_FILL_DKBTZ_MULTITEXTMAPPING_ps_5_0"
			  ,"SR_OIT_FILL_DKBTZ_TEXTMAPPING_ps_5_0"
			  ,"SR_OIT_FILL_DKBTZ_TEXTUREIMGMAP_ps_5_0"

			  ,"SR_OIT_FILL_DKBTZ_PHONGBLINN_ROV_ps_5_0"
			  ,"SR_OIT_FILL_DKBTZ_DASHEDLINE_ROV_ps_5_0"
			  ,"SR_OIT_FILL_DKBTZ_MULTITEXTMAPPING_ROV_ps_5_0"
			  ,"SR_OIT_FILL_DKBTZ_TEXTMAPPING_ROV_ps_5_0"
			  ,"SR_OIT_FILL_DKBTZ_TEXTUREIMGMAP_ROV_ps_5_0"

			  ,"SR_OIT_FILL_DKBT_PHONGBLINN_ps_5_0"
			  ,"SR_OIT_FILL_DKBT_DASHEDLINE_ps_5_0"
			  ,"SR_OIT_FILL_DKBT_MULTITEXTMAPPING_ps_5_0"
			  ,"SR_OIT_FILL_DKBT_TEXTMAPPING_ps_5_0"
			  ,"SR_OIT_FILL_DKBT_TEXTUREIMGMAP_ps_5_0"

			  ,"SR_OIT_FILL_DKBT_PHONGBLINN_ROV_ps_5_0"
			  ,"SR_OIT_FILL_DKBT_DASHEDLINE_ROV_ps_5_0"
			  ,"SR_OIT_FILL_DKBT_MULTITEXTMAPPING_ROV_ps_5_0"
			  ,"SR_OIT_FILL_DKBT_TEXTMAPPING_ROV_ps_5_0"
			  ,"SR_OIT_FILL_DKBT_TEXTUREIMGMAP_ROV_ps_5_0"

			  ,"SR_SINGLE_LAYER_ps_5_0"
			  ,"SR_SINGLE_LAYER_ps_5_0"

			,"SR_BASIC_PHONGBLINN_ps_5_0"
			,"SR_BASIC_DASHEDLINE_ps_5_0"
			,"SR_BASIC_MULTITEXTMAPPING_ps_5_0"
			,"SR_BASIC_TEXTMAPPING_ps_5_0"
			,"SR_BASIC_TEXTUREIMGMAP_ps_5_0"
			,"SR_BASIC_VOLUMEMAP_ps_5_0"
			,"SR_BASIC_VOLUME_DIST_MAP_ps_5_0"
			,"SR_UNDERCUT_ps_5_0"
			,"PCE_ParticleRenderBasic_ps_5_0"

			,"SR_QUAD_OUTLINE_ps_5_0"
			//,"SR_OIT_KDEPTH_NPRGHOST_ps_5_0"

			,"SR_OIT_ABUFFER_FRAGCOUNTER_ps_5_0"
			,"SR_OIT_ABUFFER_FRAGCOUNTER_MTT_ps_5_0"
			,"SR_OIT_ABUFFER_PHONGBLINN_ps_5_0"
			,"SR_OIT_ABUFFER_DASHEDLINE_ps_5_0"
			,"SR_OIT_ABUFFER_MULTITEXTMAPPING_ps_5_0"
			,"SR_OIT_ABUFFER_TEXTMAPPING_ps_5_0"
			,"SR_OIT_ABUFFER_TEXTUREIMGMAP_ps_5_0"
			,"SR_OIT_ABUFFER_VOLUMEMAP_ps_5_0"
			,"SR_OIT_ABUFFER_VOLUME_DIST_MAP_ps_5_0"
			,"SR_OIT_ABUFFER_UNDERCUT_ps_5_0"

			,"SR_MOMENT_GEN_ps_5_0"
			,"SR_MOMENT_GEN_TEXT_ps_5_0"
			,"SR_MOMENT_GEN_MTT_ps_5_0"
			,"SR_MOMENT_OIT_PHONGBLINN_ps_5_0"
			,"SR_MOMENT_OIT_DASHEDLINE_ps_5_0"
			,"SR_MOMENT_OIT_MULTITEXTMAPPING_ps_5_0"
			,"SR_MOMENT_OIT_TEXTMAPPING_ps_5_0"
			,"SR_MOMENT_OIT_TEXTUREIMGMAP_ps_5_0"

			,"SR_MOMENT_GEN_ROV_ps_5_0"
			,"SR_MOMENT_GEN_TEXT_ROV_ps_5_0"
			,"SR_MOMENT_GEN_MTT_ROV_ps_5_0"
			,"SR_MOMENT_OIT_PHONGBLINN_ROV_ps_5_0"
			,"SR_MOMENT_OIT_DASHEDLINE_ROV_ps_5_0"
			,"SR_MOMENT_OIT_MULTITEXTMAPPING_ROV_ps_5_0"
			,"SR_MOMENT_OIT_TEXTMAPPING_ROV_ps_5_0"
			,"SR_MOMENT_OIT_TEXTUREIMGMAP_ROV_ps_5_0"

			,"PICKING_ABUFFER_PHONGBLINN_ps_5_0"
			,"PICKING_ABUFFER_DASHEDLINE_ps_5_0"
			,"PICKING_ABUFFER_MULTITEXTMAPPING_ps_5_0"
			,"PICKING_ABUFFER_TEXTMAPPING_ps_5_0"
			,"PICKING_ABUFFER_TEXTUREIMGMAP_ps_5_0"
		};
#endif
		for (int i = 0; i < PS_NUM; i++)
		{
			string strName = strNames_PS[i];

			FILE* pFile;
			if (fopen_s(&pFile, (prefix_path + strName).c_str(), "rb") == 0)
			{
				fseek(pFile, 0, SEEK_END);
				ullong ullFileSize = ftell(pFile);
				fseek(pFile, 0, SEEK_SET);
				byte* pyRead = new byte[ullFileSize];
				fread(pyRead, sizeof(byte), ullFileSize, pFile);
				fclose(pFile);

				ID3D11PixelShader* dx11PShader = NULL;
				if (dx11CommonParams->dx11Device->CreatePixelShader(pyRead, ullFileSize, NULL, &dx11PShader) != S_OK)
				{
					VMERRORMESSAGE("SHADER COMPILE FAILURE!");
				}
				else
				{
					SET_PS(strName, dx11PShader);
				}
				VMSAFE_DELETEARRAY(pyRead);
			}
		}

#ifdef DX10_0
#else
		string strNames_CS[CS_NUM] = {
			   "OIT_SKBZ_RESOLVE_cs_5_0"
			  ,"SR_SINGLE_LAYER_TO_SKBTZ_cs_5_0"
			  ,"OIT_SKB_RESOLVE_cs_5_0"
			  ,"SR_SINGLE_LAYER_TO_SKBT_cs_5_0"
			  ,"OIT_DKBZ_RESOLVE_cs_5_0"
			  ,"OIT_DKB_RESOLVE_cs_5_0"
			  ,"SR_SINGLE_LAYER_TO_DKBTZ_cs_5_0"
			  ,"SR_FillHistogram_cs_5_0"
			  ,"SR_CreateOffsetTableKpB_cs_5_0"
			  ,"SR_SINGLE_LAYER_TO_DFB_cs_5_0"
			  ,"SR_OIT_ABUFFER_PREFIX_0_cs_5_0"
			  ,"SR_OIT_ABUFFER_PREFIX_1_cs_5_0"
			  ,"SR_OIT_ABUFFER_OffsetTable_cs_5_0"
			  ,"SR_OIT_ABUFFER_SORT2SENDER_cs_5_0"
			  ,"SR_OIT_ABUFFER_SORT2SENDER_SFM_cs_5_0"
			  ,"PCE_BlobRayMarching_cs_5_0"
			  ,"PCE_KickoffEmitterSystem_cs_5_0"
			  ,"PCE_ParticleEmitter_cs_5_0"
			  ,"PCE_ParticleSimulation_cs_5_0"
			  ,"CS_Blend2ndLayer_cs_5_0"
			  ,"KB_SSAO_cs_5_0"
			  ,"KB_SSAO_BLUR_cs_5_0"
			  ,"KB_TO_TEXTURE_cs_5_0"
			  ,"KB_SSAO_FM_cs_5_0"
			  ,"KB_SSAO_BLUR_FM_cs_5_0"
			  ,"KB_TO_TEXTURE_FM_cs_5_0"
			  ,"KB_MINMAXTEXTURE_cs_5_0"
			  ,"KB_MINMAX_NBUF_cs_5_0"
			  ,"KB_SSDOF_RT_cs_5_0"
			  ,"KB_MINMAXTEXTURE_FM_cs_5_0"
			  ,"KB_MINMAX_NBUF_FM_cs_5_0"
			  ,"KB_SSDOF_RT_FM_cs_5_0"
		};

		for (int i = 0; i < CS_NUM; i++)
		{
			string strName = strNames_CS[i];

			FILE* pFile;
			if (fopen_s(&pFile, (prefix_path + strName).c_str(), "rb") == 0)
			{
				fseek(pFile, 0, SEEK_END);
				ullong ullFileSize = ftell(pFile);
				fseek(pFile, 0, SEEK_SET);
				byte* pyRead = new byte[ullFileSize];
				fread(pyRead, sizeof(byte), ullFileSize, pFile);
				fclose(pFile);

				ID3D11ComputeShader* dx11CShader = NULL;
				if (dx11CommonParams->dx11Device->CreateComputeShader(pyRead, ullFileSize, NULL, &dx11CShader) != S_OK)
				{
					VMERRORMESSAGE("SHADER COMPILE FAILURE!");
				}
				else
				{
					SET_CS(strName, dx11CShader);
				}
				VMSAFE_DELETEARRAY(pyRead);
			}
		}
#endif
		/**/
		dx11CommonParams->dx11DeviceImmContext->Flush();
	}

	ID3D11InputLayout* dx11LI_P = (ID3D11InputLayout*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "P"));
	ID3D11InputLayout* dx11LI_PN = (ID3D11InputLayout*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "PN"));
	ID3D11InputLayout* dx11LI_PT = (ID3D11InputLayout*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "PT"));
	ID3D11InputLayout* dx11LI_PNT = (ID3D11InputLayout*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "PNT"));
	//ID3D11InputLayout* dx11LI_PNTC = (ID3D11InputLayout*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "PNTC"));
	ID3D11InputLayout* dx11LI_PTTT = (ID3D11InputLayout*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::INPUT_LAYOUT, "PTTT"));

#ifdef DX10_0
	ID3D11VertexShader* dx11VShader_P = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_P_vs_4_0"));
	ID3D11VertexShader* dx11VShader_PN = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PN_vs_4_0"));
	ID3D11VertexShader* dx11VShader_PT = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PT_vs_4_0"));
	ID3D11VertexShader* dx11VShader_PNT = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PNT_vs_4_0"));
	ID3D11VertexShader* dx11VShader_PTTT = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PTTT_vs_4_0"));
#else
	ID3D11VertexShader* dx11VShader_P = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_P_vs_5_0"));
	ID3D11VertexShader* dx11VShader_PN = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PN_vs_5_0"));
	ID3D11VertexShader* dx11VShader_PT = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PT_vs_5_0"));
	ID3D11VertexShader* dx11VShader_PNT = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PNT_vs_5_0"));
	ID3D11VertexShader* dx11VShader_PTTT = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_PTTT_vs_5_0"));
	ID3D11VertexShader* dx11VShader_IDX = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(GpuhelperResType::VERTEX_SHADER, "SR_OIT_IDX_vs_5_0"));
#endif
	ID3D11Buffer* cbuf_cam_state = dx11CommonParams->get_cbuf("CB_CameraState");
	ID3D11Buffer* cbuf_env_state = dx11CommonParams->get_cbuf("CB_EnvState");
	ID3D11Buffer* cbuf_clip = dx11CommonParams->get_cbuf("CB_ClipInfo");
	ID3D11Buffer* cbuf_pobj = dx11CommonParams->get_cbuf("CB_PolygonObject");
	ID3D11Buffer* cbuf_vobj = dx11CommonParams->get_cbuf("CB_VolumeObject");
	ID3D11Buffer* cbuf_reffect = dx11CommonParams->get_cbuf("CB_Material");
	ID3D11Buffer* cbuf_tmap = dx11CommonParams->get_cbuf("CB_TMAP");
	ID3D11Buffer* cbuf_hsmask = dx11CommonParams->get_cbuf("CB_HotspotMask");
#pragma endregion 

#pragma region // IOBJECT CPU
	//while (iobj->GetFrameBuffer(FrameBufferUsageRENDEROUT, 1) != NULL)
	//	iobj->DeleteFrameBuffer(FrameBufferUsageRENDEROUT, 1);
	if (!iobj->ReplaceFrameBuffer(FrameBufferUsageRENDEROUT, 0, data_type::dtype<vmbyte4>(), ("common render out frame buffer : defined in vismtv_inbuilt_renderergpudx module")))
		iobj->InsertFrameBuffer(data_type::dtype<vmbyte4>(), FrameBufferUsageRENDEROUT, ("common render out frame buffer : defined in vismtv_inbuilt_renderergpudx module"));

	//while (iobj->GetFrameBuffer(FrameBufferUsageDEPTH, 1) != NULL)
	//	iobj->DeleteFrameBuffer(FrameBufferUsageDEPTH, 1);
	if (!iobj->ReplaceFrameBuffer(FrameBufferUsageDEPTH, 0, data_type::dtype<float>(), ("1st hit screen depth frame buffer : defined in vismtv_inbuilt_renderergpudx module")))
		iobj->InsertFrameBuffer(data_type::dtype<float>(), FrameBufferUsageDEPTH, ("1st hit screen depth frame buffer : defined in vismtv_inbuilt_renderergpudx module"));
#pragma endregion 

	__ID3D11Device* dx11Device = dx11CommonParams->dx11Device;
	__ID3D11DeviceContext* dx11DeviceImmContext = dx11CommonParams->dx11DeviceImmContext;

#pragma region // IOBJECT GPU
	int buffer_ex = (check_pixel_transmittance && mode_OIT == MFR_MODE::DYNAMIC_FB) ? buf_ex_scale : 1, buffer_ex_old = 0; // optimal for K is 1
	// 'cause we now do not support the dynamic version of k+ buffer
	// it always uses static number of k!!
	// note that DFB uses a simple fragment model (vis and depth) and the stored simple fragments are extended into the z-thickness model fragments in the resolve pass

	vmint2 fb_size_cur;
	iobj->GetFrameBufferInfo(&fb_size_cur);
	vmint2 fb_size_old = iobj->GetObjParam("_int2_PreviousScreenSize", vmint2(0, 0));
	buffer_ex_old = iobj->GetObjParam("_int_PreviousBufferEx", -1);
	if (fb_size_cur.x != fb_size_old.x || fb_size_cur.y != fb_size_old.y
		|| k_value != k_value_old || num_moments != num_moments_old
		|| buffer_ex != buffer_ex_old)
	{
		gpu_manager->ReleaseGpuResourcesBySrcID(iobj->GetObjectID());	// System Out 포함 //
		iobj->SetObjParam("_int2_PreviousScreenSize", fb_size_cur);
		iobj->SetObjParam("_int_PreviousBufferEx", buffer_ex);
	}
	ullong lastest_render_time = iobj->GetObjParam("_ullong_LatestSrTime", (ullong)0);

	GpuRes gres_fb_rgba, gres_fb_depthcs, gres_fb_depthstencil;
	GpuRes gres_fb_singlelayer_rgba, gres_fb_singlelayer_depthcs, gres_fb_singlelayer_tempDepth;
	GpuRes gres_fb_sys_rgba, gres_fb_sys_depthcs;
#ifdef DX10_0
	const uint rtbind = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	GpuRes gres_fb_pickingId, gres_fb_pickingDepthcs, gres_fb_pickingDepthstencil;
#else
	const uint rtbind = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	GpuRes gres_fb_counter, gres_fb_spinlock;
	GpuRes gres_fb_k_buffer, gres_fb_ubk_buffer;
	//GpuRes gres_fb_mip_a_halftexs[2], gres_fb_mip_z_halftexs[2]; // deprecated
	GpuRes gres_fb_ao_vr_tex, gres_fb_ao_vr_blf_tex;
	GpuRes gres_fb_ref_pidx;
	GpuRes gres_fb_moment_rgba;

	// check_pixel_transmittance
	GpuRes gres_fb_sys_deep_k, gres_fb_sys_ref_pidx, gres_fb_sys_counter;
#endif
	// Ghost effect mode
	//GpuRes gres_fb_mask_hotspot;
	grd_helper::UpdateFrameBuffer(gres_fb_rgba, iobj, "RENDER_OUT_RGBA_0", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
	grd_helper::UpdateFrameBuffer(gres_fb_depthcs, iobj, "RENDER_OUT_DEPTH_0", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R32_FLOAT, 0);
	grd_helper::UpdateFrameBuffer(gres_fb_singlelayer_rgba, iobj, "RENDER_OUT_RGBA_1", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
	grd_helper::UpdateFrameBuffer(gres_fb_singlelayer_depthcs, iobj, "RENDER_OUT_DEPTH_1", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R32_FLOAT, 0);
	grd_helper::UpdateFrameBuffer(gres_fb_singlelayer_tempDepth, iobj, "RENDER_OUT_DEPTH_2", RTYPE_TEXTURE2D, rtbind, DXGI_FORMAT_R32_FLOAT, 0);
	grd_helper::UpdateFrameBuffer(gres_fb_depthstencil, iobj, "DEPTH_STENCIL", RTYPE_TEXTURE2D, D3D11_BIND_DEPTH_STENCIL, DXGI_FORMAT_D32_FLOAT, false);
	grd_helper::UpdateFrameBuffer(gres_fb_sys_rgba, iobj, "SYSTEM_OUT_RGBA", RTYPE_TEXTURE2D, NULL, DXGI_FORMAT_R8G8B8A8_UNORM, UPFB_SYSOUT);
	grd_helper::UpdateFrameBuffer(gres_fb_sys_depthcs, iobj, "SYSTEM_OUT_DEPTH", RTYPE_TEXTURE2D, NULL, DXGI_FORMAT_R32_FLOAT, UPFB_SYSOUT);

	const int max_picking_layers = 100;
	GpuRes gres_picking_buffer, gres_picking_system_buffer, gres_picking_GSO_buffer, gres_picking_system_GSO_buffer;
	if (is_picking_routine) {
		// those picking layers contain depth (4bytes) and id (4bytes) information
		grd_helper::UpdateFrameBuffer(gres_picking_buffer, iobj, "BUFFER_RW_PICKING_BUF", RTYPE_BUFFER,
			D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_UINT, UPFB_NFPP_BUFFERSIZE, max_picking_layers * 2);
		grd_helper::UpdateFrameBuffer(gres_picking_system_buffer, iobj, "SYSTEM_OUT_RW_PICKING_BUF", RTYPE_BUFFER,
			NULL, DXGI_FORMAT_R32_UINT, UPFB_SYSOUT | UPFB_NFPP_BUFFERSIZE, max_picking_layers * 2);

		grd_helper::UpdateFrameBuffer(gres_picking_GSO_buffer, iobj, "BUFFER_RW_PICKING_GSO_BUF", RTYPE_BUFFER,
			D3D11_BIND_STREAM_OUTPUT, DXGI_FORMAT_R32G32B32A32_FLOAT, UPFB_NFPP_BUFFERSIZE, max_picking_layers);
		grd_helper::UpdateFrameBuffer(gres_picking_system_GSO_buffer, iobj, "SYSTEM_OUT_RW_PICKING_GSO_BUF", RTYPE_BUFFER,
			NULL, DXGI_FORMAT_R32G32B32A32_FLOAT, UPFB_SYSOUT | UPFB_NFPP_BUFFERSIZE, max_picking_layers);

		std::vector<uint> clearDataUnit(max_picking_layers * 4, 0);//make sure that this thing is aligned
		dx11CommonParams->dx11DeviceImmContext->UpdateSubresource(
			pickPrimitive? (ID3D11Buffer*)gres_picking_GSO_buffer.alloc_res_ptrs[DTYPE_RES] : (ID3D11Buffer*)gres_picking_buffer.alloc_res_ptrs[DTYPE_RES]
			, 0, NULL, &clearDataUnit[0], sizeof(uint) * clearDataUnit.size(), sizeof(uint) * clearDataUnit.size());
	}

#ifdef DX10_0
#else
	// also using singlelayer rt
	grd_helper::UpdateFrameBuffer(gres_fb_counter, iobj, "RW_COUNTER", RTYPE_TEXTURE2D,
		D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_UINT, 0);
	if (use_spinlock_pixsynch)// || mode_OIT == MFR_MODE::MOMENT) // when MFR_MODE::DXAB mode, this buffer used for another purpose?!
		grd_helper::UpdateFrameBuffer(gres_fb_spinlock, iobj, "RW_SPINLOCK", RTYPE_TEXTURE2D,
			D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_UINT, 0);

	if (mode_OIT == MFR_MODE::MOMENT)
		grd_helper::UpdateFrameBuffer(gres_fb_moment_rgba, iobj, "RENDER_MOMENT_RGBA", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32G32B32A32_FLOAT, 0);

	const int num_frags_perpixel = k_value * 4 * buffer_ex;
	grd_helper::UpdateFrameBuffer(gres_fb_k_buffer, iobj, "BUFFER_RW_K_BUF", RTYPE_BUFFER,
		D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_TYPELESS, UPFB_RAWBYTE, num_frags_perpixel);
	grd_helper::UpdateFrameBuffer(gres_fb_ubk_buffer, iobj, "BUFFER_RW_UBK_BUF", RTYPE_BUFFER, // here, ubk refers to UnBounded-K
		D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_TYPELESS, UPFB_RAWBYTE, k_value * 2 * 8);

	// SSAO
	{
		//GpuRes gres_fb_ao_texs, gres_fb_ao_blf_texs;
		//grd_helper::UpdateFrameBuffer(gres_fb_ao_texs, iobj, "RW_TEXS_AO", RTYPE_TEXTURE2D,
		//	D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8_UNORM, UPFB_MIPMAP);
		//grd_helper::UpdateFrameBuffer(gres_fb_ao_blf_texs, iobj, "RW_TEXS_AO_BLF", RTYPE_TEXTURE2D,
		//	D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, UPFB_MIPMAP);
		//
		//grd_helper::UpdateFrameBuffer(gres_fb_mip_a_halftexs[0], iobj, "RW_TEXS_OPACITY_0", RTYPE_TEXTURE2D,
		//	D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, UPFB_MIPMAP | UPFB_HALF);
		//grd_helper::UpdateFrameBuffer(gres_fb_mip_a_halftexs[1], iobj, "RW_TEXS_OPACITY_1", RTYPE_TEXTURE2D,
		//	D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, UPFB_MIPMAP | UPFB_HALF);
		//grd_helper::UpdateFrameBuffer(gres_fb_mip_z_halftexs[0], iobj, "RW_TEXS_Z_0", RTYPE_TEXTURE2D,
		//	D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32G32B32A32_FLOAT, UPFB_MIPMAP | UPFB_HALF);
		//grd_helper::UpdateFrameBuffer(gres_fb_mip_z_halftexs[1], iobj, "RW_TEXS_Z_1", RTYPE_TEXTURE2D,
		//	D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32G32B32A32_FLOAT, UPFB_MIPMAP | UPFB_HALF);
	}

	// RS 에서 스페셜로 빼내기
	// ...


	if (mode_OIT == MFR_MODE::DYNAMIC_FB || mode_OIT == MFR_MODE::DYNAMIC_KB)
		grd_helper::UpdateFrameBuffer(gres_fb_ref_pidx, iobj, "BUFFER_RW_REF_PIDX_BUF", RTYPE_BUFFER, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_UINT, 0);

	if (check_pixel_transmittance)
	{
		cout << "*********************************************************" << endl;
		cout << "check_pixel_transmittance : ON!! performance downgrade!" << endl;

		grd_helper::UpdateFrameBuffer(gres_fb_sys_deep_k, iobj, "SYSTEM_OUT_K_BUF", RTYPE_BUFFER,
			NULL, DXGI_FORMAT_R32_UINT, UPFB_SYSOUT, num_frags_perpixel);
		grd_helper::UpdateFrameBuffer(gres_fb_sys_ref_pidx, iobj, "SYSTEM_OUT_REF_PIDX_BUF", RTYPE_BUFFER,
			NULL, DXGI_FORMAT_R32_UINT, UPFB_SYSOUT);
		grd_helper::UpdateFrameBuffer(gres_fb_sys_counter, iobj, "SYSTEM_OUT_COUNTER", RTYPE_TEXTURE2D,
			NULL, DXGI_FORMAT_R32_UINT, UPFB_SYSOUT);
	}
	//if (is_ghost_mode)
	//{
	//	// do 'dynamic'
	//	// current version does not use buffer-based mask
	//	auto UpdateMaskFB = [&_fncontainer, &gpu_manager, &dx11CommonParams](GpuRes& gres, const VmIObject* iobj, const string& res_name, bool updatemask)
	//	{
	//		gres.vm_src_id = iobj->GetObjectID();
	//		gres.res_name = res_name;
	//		vmint2 fb_size;
	//		((VmIObject*)iobj)->GetFrameBufferInfo(&fb_size);
	//		if (!gpu_manager->UpdateGpuResource(gres))
	//		{
	//			gres.rtype = RTYPE_BUFFER;
	//			gres.options["USAGE"] = D3D11_USAGE_DYNAMIC;
	//			gres.options["CPU_ACCESS_FLAG"] = D3D11_CPU_ACCESS_WRITE;
	//			gres.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE;
	//			gres.options["FORMAT"] = DXGI_FORMAT_R32_FLOAT;
	//			gres.res_dvalues["NUM_ELEMENTS"] = fb_size.x * fb_size.y * 4;
	//			gres.res_dvalues["STRIDE_BYTES"] = sizeof(float);
	//			gpu_manager->GenerateGpuResource(gres);
	//			updatemask = true;
	//		}
	//		if (updatemask)
	//		{
	//			vmdouble4 mask_center_rs_0 = _fncontainer->fnParams.GetParam("_float4_MaskCenterRadius0", vmdouble4(fb_size / 2, fb_size.x / 5, 5));
	//			//vmdouble4 mask_center_rs_1 = _fncontainer->fnParams.GetParam("_float4_MaskCenterRadius1", vmdouble4(fb_size / 2, 0, 5));
	//			vmdouble4 hotspot_params_0 = _fncontainer->fnParams.GetParam("_float4_HotspotParamsTKtKsV0", vmdouble4(1., 1., 1., 1.));
	//			//vmdouble4 hotspot_params_1 = _fncontainer->fnParams.GetParam("_float4_HotspotParamsTKtKsV1", vmdouble4(1., 1., 1., 1.));
	//			float mask_bnd = (float)_fncontainer->fnParams.GetParam("_float_MaskBndDisplay", (double)1.0);
	//
	//			D3D11_MAPPED_SUBRESOURCE d11MappedRes;
	//			HRESULT hr = dx11CommonParams->dx11DeviceImmContext->Map((ID3D11Buffer*)gres.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes);
	//			if (hr != S_OK) cout << "ERROR HOTSPOT" << endl;
	//			float sm_v_max_0 = atan((float)mask_center_rs_0.w);
	//			vmfloat4* mask_buf = (vmfloat4*)d11MappedRes.pData;
	//			for (int y = 0; y < fb_size.y; y++ )
	//				for (int x = 0; x < fb_size.x; x++)
	//				{
	//					vmfloat4 mask_v = vmfloat4(0);
	//					vmfloat2 vdiff = vmfloat2(x, y) - vmfloat2(mask_center_rs_0.x, mask_center_rs_0.y);
	//
	//					float length = glm::length(vdiff);
	//					float arc_x = max((mask_center_rs_0.z - length), 0) / mask_center_rs_0.z * 2.f - 1.f; // [-1 (outside), 1 (inside)]
	//					float value = (atan(arc_x * mask_center_rs_0.w) + sm_v_max_0) / (2.f * sm_v_max_0); // [0, 1]
	//					mask_v.w = value;
	//					mask_v.x = max(hotspot_params_0.x * value, 0.00001f); // thickness
	//					mask_v.y = max(hotspot_params_0.y - 1.f, 0) * value + 1.f;
	//					mask_v.z = hotspot_params_0.z * value;
	//
	//					if(abs(length - mask_center_rs_0.z) < mask_bnd)
	//						mask_v.w = -1.f;
	//
	//					//float value = (atan((length / mask_center_rs_0.z) * mask_center_rs_0.w) + sm_v_max_0) / (2.f * sm_v_max_0);
	//					//if (length <= mask_center_rs_0.z)
	//					//{
	//					//	mask_v = (vmfloat4)hotspot_params_0 * value;
	//					//}
	//					mask_buf[x + y * fb_size.x] = mask_v;
	//				}
	//			dx11CommonParams->dx11DeviceImmContext->Unmap((ID3D11Buffer*)gres.alloc_res_ptrs[DTYPE_RES], 0);
	//		}
	//	};
	//	//UpdateMaskFB(gres_fb_mask_hotspot, iobj, "BUFFER_MASK_HOTSPOT", true);
	//
	//	
	//	D3D11_MAPPED_SUBRESOURCE mappedResHSMask;
	//	dx11DeviceImmContext->Map(cbuf_hsmask, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResHSMask);
	//	CB_HotspotMask* cbHSMaskData = (CB_HotspotMask*)mappedResHSMask.pData;
	//	grd_helper::SetCb_HotspotMask(*cbHSMaskData, _fncontainer, fb_size_old);
	//	dx11DeviceImmContext->Unmap(cbuf_hsmask, 0);
	//	dx11DeviceImmContext->PSSetConstantBuffers(9, 1, &cbuf_hsmask);
	//	dx11DeviceImmContext->CSSetConstantBuffers(9, 1, &cbuf_hsmask);
	//}
#endif
#pragma endregion 

	uint num_grid_x = __BLOCKSIZE == 1 ? fb_size_cur.x : (uint)ceil(fb_size_cur.x / (float)__BLOCKSIZE);
	uint num_grid_y = __BLOCKSIZE == 1 ? fb_size_cur.y : (uint)ceil(fb_size_cur.y / (float)__BLOCKSIZE);

#pragma region // Camera & Light Setting
	//cout << v_copthickness_abs << endl;
	//float fv_copthickness = v_copthickness_abs <= 0 ? (float)(len_diagonal_max * v_copthickness) : (float)v_copthickness_abs;
	//float fv_thickness = v_thickness_abs <= 0 ? v_copthickness_abs <= 0 ? (float)(len_diagonal_max * v_thickness) : fv_copthickness * v_thickness / v_copthickness : (float)v_thickness_abs;

	VmCObject* cam_obj = iobj->GetCameraObject();

	vmmat44 dmatWS2CS, dmatCS2PS, dmatPS2SS;
	vmmat44 dmatSS2PS, dmatPS2CS, dmatCS2WS;
	cam_obj->GetMatrixWStoSS(&dmatWS2CS, &dmatCS2PS, &dmatPS2SS);
	cam_obj->GetMatrixSStoWS(&dmatSS2PS, &dmatPS2CS, &dmatCS2WS);
	vmmat44 dmatWS2PS = dmatWS2CS * dmatCS2PS;
	vmmat44f matWS2PS = dmatWS2PS;
	vmmat44f matWS2SS = dmatWS2PS * dmatPS2SS;
	vmmat44f matSS2WS = (dmatSS2PS * dmatPS2CS) * dmatCS2WS;

	vmfloat3 picking_ray_origin, picking_ray_dir;
	if (is_picking_routine) {
		cam_obj->GetCameraExtStatef(&picking_ray_origin, &picking_ray_dir, NULL);
		vmfloat3 pos_picking_ws, pos_picking_ss(picking_pos_ss.x, picking_pos_ss.y, 0);
		vmmath::fTransformPoint(&pos_picking_ws, &pos_picking_ss, &matSS2WS);

		if (cam_obj->IsPerspective()) {
			picking_ray_dir = pos_picking_ws - picking_ray_origin;
			vmmath::fNormalizeVector(&picking_ray_dir, &picking_ray_dir);
		}
		else {
			picking_ray_origin = pos_picking_ws;
		}
	}


	CB_EnvState cbEnvState;
	grd_helper::SetCb_Env(cbEnvState, cam_obj, light_src, global_lighting, lens_effect);
	cbEnvState.num_safe_loopexit = num_safe_loopexit;
	cbEnvState.env_dummy_2 = i_test_shader;
	D3D11_MAPPED_SUBRESOURCE mappedResEnvState;
	dx11DeviceImmContext->Map(cbuf_env_state, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResEnvState);
	CB_EnvState* cbEnvStateData = (CB_EnvState*)mappedResEnvState.pData;
	memcpy(cbEnvStateData, &cbEnvState, sizeof(CB_EnvState));
	dx11DeviceImmContext->Unmap(cbuf_env_state, 0);
	dx11DeviceImmContext->PSSetConstantBuffers(7, 1, &cbuf_env_state);
	dx11DeviceImmContext->CSSetConstantBuffers(7, 1, &cbuf_env_state);

	if (is_ghost_mode)
	{
		// do 'dynamic'
		D3D11_MAPPED_SUBRESOURCE mappedResHSMask;
		dx11DeviceImmContext->Map(cbuf_hsmask, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResHSMask);
		CB_HotspotMask* cbHSMaskData = (CB_HotspotMask*)mappedResHSMask.pData;
		grd_helper::SetCb_HotspotMask(*cbHSMaskData, _fncontainer, matWS2SS);
		dx11DeviceImmContext->Unmap(cbuf_hsmask, 0);
		dx11DeviceImmContext->PSSetConstantBuffers(9, 1, &cbuf_hsmask);
		dx11DeviceImmContext->CSSetConstantBuffers(9, 1, &cbuf_hsmask);
	}
	//
	MomentOIT cb_moment;
	vmdouble2 mot_nf;
	if (mode_OIT == MFR_MODE::MOMENT)
	{
		ID3D11Buffer* cbuf_moment_state = dx11CommonParams->get_cbuf("MomentOIT");
		computeWrappingZoneParameters((float*)&cb_moment.wrapping_zone_parameters);
		cb_moment.overestimation = 0.25f;
		cb_moment.moment_bias = 0.0025f;
		double np, fp;
		if (cam_obj->IsArIntrinsics())
			cam_obj->GetCameraIntStateAR(NULL, NULL, NULL, NULL, NULL, &np, &fp);
		else
			cam_obj->GetCameraIntState(NULL, &np, &fp, NULL);
		mot_nf = _fncontainer->fnParams.GetParam("_float2_MotNearFar", vmdouble2(np, fp));
		cb_moment.warp_nf = mot_nf;
		switch (num_moments)
		{
		case 4: cb_moment.moment_bias = 0.00006f; break;
		case 6: cb_moment.moment_bias = 0.0006f; break;
		case 8:	cb_moment.moment_bias = 0.0025f; break;
		case 14: cb_moment.moment_bias = 0.0004f; break;
		case 16: cb_moment.moment_bias = 0.00065f; break;
		case 18: cb_moment.moment_bias = 0.00085f; break;
		}
		cb_moment.moment_bias = _fncontainer->fnParams.GetParam("_float_MomentBias", cb_moment.moment_bias);
		D3D11_MAPPED_SUBRESOURCE mappedMomentState;
		dx11DeviceImmContext->Map(cbuf_moment_state, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedMomentState);
		MomentOIT* cbMomentData = (MomentOIT*)mappedMomentState.pData;
		memcpy(cbMomentData, &cb_moment, sizeof(MomentOIT));
		dx11DeviceImmContext->Unmap(cbuf_moment_state, 0);
		dx11DeviceImmContext->PSSetConstantBuffers(8, 1, &cbuf_moment_state);
	}
	//

	//if (is_ghost_mode)
	//{
	//	dx11DeviceImmContext->PSSetShaderResources(50, 1, (ID3D11ShaderResourceView**)&gres_fb_mask_hotspot.alloc_res_ptrs[DTYPE_SRV]);
	//	dx11DeviceImmContext->CSSetShaderResources(50, 1, (ID3D11ShaderResourceView**)&gres_fb_mask_hotspot.alloc_res_ptrs[DTYPE_SRV]);
	//}

	D3D11_RECT rects[1];
	rects[0].left = 0;
	rects[0].right = fb_size_cur.x;
	rects[0].top = 0;
	rects[0].bottom = fb_size_cur.y;
	dx11DeviceImmContext->RSSetScissorRects(1, rects);

	// View Port Setting //
	D3D11_VIEWPORT dx11ViewPort;
	dx11ViewPort.Width = (float)fb_size_cur.x;
	dx11ViewPort.Height = (float)fb_size_cur.y;
	dx11ViewPort.MinDepth = 0;
	dx11ViewPort.MaxDepth = 1.0f;
	dx11ViewPort.TopLeftX = 0;
	dx11ViewPort.TopLeftY = 0;
	dx11DeviceImmContext->RSSetViewports(1, &dx11ViewPort);
#pragma endregion 

#pragma region // Presetting of VmObject
	auto __compute_computespace_screen = [](int& w, int& h, const vmmat44f& matOS2SS, const AaBbMinMax& stAaBbOS)
	{
		vmint2 aaBbMinSS(INT_MAX, INT_MAX), aaBbMaxSS(0, 0);
		vmfloat3 f3PosOrthoBoxesOS[8];
		f3PosOrthoBoxesOS[0] = vmfloat3(stAaBbOS.pos_min.x, stAaBbOS.pos_min.y, stAaBbOS.pos_max.z);
		f3PosOrthoBoxesOS[1] = vmfloat3(stAaBbOS.pos_max.x, stAaBbOS.pos_min.y, stAaBbOS.pos_max.z);
		f3PosOrthoBoxesOS[2] = vmfloat3(stAaBbOS.pos_min.x, stAaBbOS.pos_min.y, stAaBbOS.pos_min.z);
		f3PosOrthoBoxesOS[3] = vmfloat3(stAaBbOS.pos_max.x, stAaBbOS.pos_min.y, stAaBbOS.pos_min.z);
		f3PosOrthoBoxesOS[4] = vmfloat3(stAaBbOS.pos_min.x, stAaBbOS.pos_max.y, stAaBbOS.pos_max.z);
		f3PosOrthoBoxesOS[5] = vmfloat3(stAaBbOS.pos_max.x, stAaBbOS.pos_max.y, stAaBbOS.pos_max.z);
		f3PosOrthoBoxesOS[6] = vmfloat3(stAaBbOS.pos_min.x, stAaBbOS.pos_max.y, stAaBbOS.pos_min.z);
		f3PosOrthoBoxesOS[7] = vmfloat3(stAaBbOS.pos_max.x, stAaBbOS.pos_max.y, stAaBbOS.pos_min.z);
		for (int i = 0; i < 8; i++)
		{
			vmfloat3 f3PosOrthoBoxSS;
			fTransformPoint(&f3PosOrthoBoxSS, &f3PosOrthoBoxesOS[i], &matOS2SS);
			vmint2 pos_xy = vmint2((int)f3PosOrthoBoxSS.x, (int)f3PosOrthoBoxSS.y);
			aaBbMinSS.x = min(aaBbMinSS.x, pos_xy.x);
			aaBbMinSS.y = min(aaBbMinSS.y, pos_xy.y);
			aaBbMaxSS.x = max(aaBbMaxSS.x, pos_xy.x);
			aaBbMaxSS.y = max(aaBbMaxSS.y, pos_xy.y);
		}
		w = aaBbMaxSS.x - aaBbMinSS.x;
		h = aaBbMaxSS.y - aaBbMinSS.y;
	};

	vector<VmActor> temperal_actors;
	vector<VmActor*> general_oit_routine_objs;
	vector<VmActor*> single_layer_routine_group_objs; // e.g., individual silhouette 
	vector<VmActor*> single_layer_routine_objs; // e.g. group silhouette
#ifndef DX10_0
	vector<VmActor*> particle_layer_objs; // e.g., particle effect
	vector<VmActor*> second_layer_objs;
#endif
	vector<VmActor*> foremost_opaque_surfaces_routine_objs; // for performance //
	int minimum_oit_area = _fncontainer->fnParams.GetParam("_int_MinimumOitArea", (int)1000); // 0 means turn-off the wildcard case
	int _w_max = 0;
	int _h_max = 0;
	// For Each Primitive //
	for (auto& actorPair : _fncontainer->sceneActors)
	{
		VmActor* actor = get<1>(actorPair);

#ifndef DX10_0
		bool isParticleActor = actor->GetParam("_bool_IsParticleActor", false);
		if (isParticleActor) {
			particle_layer_objs.push_back(actor);
			continue;
		}
#endif

		VmVObject* geo_obj = actor->GetGeometryRes();
		if (geo_obj == NULL ||
			geo_obj->GetObjectType() != ObjectTypePRIMITIVE ||
			!geo_obj->IsDefined() ||
			!actor->visible || actor->color.a == 0)
			continue;

		VmVObjectPrimitive* pobj = (VmVObjectPrimitive*)geo_obj;
		PrimitiveData* prim_data = pobj->GetPrimitiveData();
		if (prim_data->GetVerticeDefinition("POSITION") == NULL)
			continue;

		if (is_picking_routine) {
			if (prim_data->ptype == vmenums::PrimitiveTypeLINE || grd_helper::CollisionCheck(actor->matWS2OS, prim_data->aabb_os, picking_ray_origin, picking_ray_dir))
				general_oit_routine_objs.push_back(actor);
			//std::cout << "###### obb ray intersection : " << actor->actorId << std::endl;
			// NOTE THAT is_picking_routine allows only general_oit_routine_objs!!
			continue;
		}

		vmmat44f matPivot = (actor->GetParam("_matrix44f_Pivot", vmmat44f(1)));
		vmmat44f matOS2SS = matPivot * actor->matOS2WS * matWS2SS;
		int w, h;
		__compute_computespace_screen(w, h, matOS2SS, prim_data->aabb_os);
		bool is_wildcard_Candidate = w * h < minimum_oit_area;
		actor->SetParam("_int_SpinLockCount", is_wildcard_Candidate ? (int)10 : num_safe_loopexit);
		_w_max = max(_w_max, w);
		_h_max = max(_h_max, h);

		bool has_wire = actor->GetParam("_bool_HasWireframe", false);

		bool is_foremost_surfaces = true;

		bool is_annotation_obj = pobj->GetObjParam("_bool_IsAnnotationObj", false);
		if (is_annotation_obj || actor->color.a < 1.f) {
			is_foremost_surfaces = false;
		}
		if (has_wire && prim_data->ptype == PrimitiveTypeTRIANGLE)
		{
			temperal_actors.push_back(*actor);
			VmActor* temp_actor = &temperal_actors[temperal_actors.size() - 1];
			temp_actor->color = actor->GetParam("_float4_WireColor", vmfloat4(0));
			temp_actor->SetParam("_bool_IsWireframe", true);
			//foremost_surfaces_routine_objs.push_back(temp_actor);
			// trick for using 'z-thickness blending' between wires and surfaces
			is_foremost_surfaces = false;
		}
		actor->SetParam("_bool_IsWireframe", false);

		bool showOutline = actor->GetParam("_bool_ShowOutline", false); 
		if (showOutline) {
			single_layer_routine_objs.push_back(actor);
		}
		bool showGroupOutline = actor->GetParam("_bool_ShowGroupOutline", false);
		if (showGroupOutline) {
			single_layer_routine_group_objs.push_back(actor);
		}

#ifndef DX10_0
		bool isSecondLayerActor = actor->GetParam("_bool_IsSecondLayerActor", false);
		if (isSecondLayerActor) {
			second_layer_objs.push_back(actor);
			continue;
		}
#endif

		VmObject* imgObj = actor->GetAssociateRes("TEXTURE2DIMAGE");
		if (imgObj && is_foremost_surfaces) {

			if (imgObj->GetObjParam("IS_SEMIPARENT", false))
				is_foremost_surfaces = false;
		}
		if (is_foremost_surfaces)
		{
			foremost_opaque_surfaces_routine_objs.push_back(actor);
		}
		else {
#ifdef DX10_0
			general_oit_routine_objs.push_back(actor);
#else
			//if(actor->GetParam("_bool_OnlyForemostSurfaces", false))
			//	general_oit_routine_objs.push_back(actor);
			//else 
			//	single_foremost_routine_objs.push_back(actor);
			general_oit_routine_objs.push_back(actor);
#endif
		}
	}

	for (VmActor& __actor : temperal_actors)
		foremost_opaque_surfaces_routine_objs.push_back(&__actor);

#ifdef DX10_0
	for (int i = 0; i < (int)general_oit_routine_objs.size(); i++) {
		foremost_opaque_surfaces_routine_objs.push_back(general_oit_routine_objs[i]);
	}
	general_oit_routine_objs.clear();
	if (is_picking_routine) {
		general_oit_routine_objs = foremost_opaque_surfaces_routine_objs;
		foremost_opaque_surfaces_routine_objs.clear();
	}
#endif

	if (dx11CommonParams->gpu_profile)
	{
		cout << "  ** general_oit_routine_objs    : " << general_oit_routine_objs.size() << endl;
		cout << "  ** special_effect_routine_objs : " << single_layer_routine_objs.size() << endl;
		cout << "  ** single_layer_routine_group_objs : " << single_layer_routine_group_objs.size() << endl;
#ifndef DX10_0
		cout << "  ** particle_layer_objs : " << particle_layer_objs.size() << endl;
#endif
		cout << "  ** foremost_sr_routine_objs : " << foremost_opaque_surfaces_routine_objs.size() << endl;
	}
#pragma endregion 

	//if (is_picking_routine) {
	//	cout << "PICKING SCREEN POS : " << picking_pos_ss.x << ", " << picking_pos_ss.y << endl;
	//	for (auto a : general_oit_routine_objs) {
	//		cout << "PICKING TARGET ACTORS : " << a->actorId << endl;
	//	}
	//}

#pragma region // FrameBuffer Setting
	// Backup Previous Render Target //
	ID3D11RenderTargetView* pdxRTVOld = NULL;
	ID3D11DepthStencilView* pdxDSVOld = NULL;
	dx11DeviceImmContext->OMGetRenderTargets(1, &pdxRTVOld, &pdxDSVOld);

	dx11CommonParams->GpuProfile("Clear for Render1 - SR");

	// Clear Depth Stencil //
	ID3D11DepthStencilView* dx11DSV = (ID3D11DepthStencilView*)gres_fb_depthstencil.alloc_res_ptrs[DTYPE_DSV];
	// dx11DeviceImmContext->ClearDepthStencilView(dx11DSV, D3D11_CLEAR_DEPTH, 1.0f, 0); // this code is located to the right ahead of rendering calls

	float flt_max_ = FLT_MAX;
	uint flt_max_u = *(uint*)&flt_max_;
	uint clr_unit4[4] = { 0, 0, 0, 0 };
	uint clr_max_ufloat_4[4] = { flt_max_u, flt_max_u, flt_max_u, flt_max_u };
	float clr_float_zero_4[4] = { 0, 0, 0, 0 };
	float clr_float_fltmax_4[4] = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };

#ifdef DX10_0
#else 
	//float clr_float_minus_4[4] = { -1.f, -1.f, -1.f, -1.f };
	dx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gres_fb_counter.alloc_res_ptrs[DTYPE_UAV], clr_unit4);
	if (use_spinlock_pixsynch)
		dx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gres_fb_spinlock.alloc_res_ptrs[DTYPE_UAV], clr_unit4);
	if (mode_OIT == MFR_MODE::MOMENT)
		dx11DeviceImmContext->ClearUnorderedAccessViewFloat((ID3D11UnorderedAccessView*)gres_fb_moment_rgba.alloc_res_ptrs[DTYPE_UAV], clr_float_zero_4);
	//dx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gres_fb_deep_k_buffer.alloc_res_ptrs[DTYPE_UAV], clr_unit4);
	//if(is_rov_mode)
	//	dx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gres_fb_deep_k_buffer_rov.alloc_res_ptrs[DTYPE_UAV], clr_unit4);
	if (mode_OIT == MFR_MODE::DYNAMIC_FB || mode_OIT == MFR_MODE::DYNAMIC_KB)
		dx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gres_fb_ref_pidx.alloc_res_ptrs[DTYPE_UAV], clr_unit4);
#endif

	if (!is_picking_routine)
	{
		dx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_RTV], clr_float_zero_4);
		dx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_RTV], clr_float_fltmax_4);
		dx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gres_fb_singlelayer_rgba.alloc_res_ptrs[DTYPE_RTV], clr_float_zero_4);
		dx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gres_fb_singlelayer_tempDepth.alloc_res_ptrs[DTYPE_RTV], clr_float_fltmax_4);
	}
	dx11CommonParams->GpuProfile("Clear for Render1 - SR", true);
#pragma endregion 

#pragma region // HLSL Sampler Setting
	ID3D11SamplerState* sampler_PZ = dx11CommonParams->get_sampler("POINT_ZEROBORDER");
	ID3D11SamplerState* sampler_LZ = dx11CommonParams->get_sampler("LINEAR_ZEROBORDER");
	ID3D11SamplerState* sampler_PC = dx11CommonParams->get_sampler("POINT_CLAMP");
	ID3D11SamplerState* sampler_LC = dx11CommonParams->get_sampler("LINEAR_CLAMP");
	ID3D11SamplerState* sampler_PW = dx11CommonParams->get_sampler("POINT_WRAP");
	ID3D11SamplerState* sampler_LW = dx11CommonParams->get_sampler("LINEAR_WRAP");

	dx11DeviceImmContext->VSSetSamplers(0, 1, &sampler_LZ);
	dx11DeviceImmContext->VSSetSamplers(1, 1, &sampler_PZ);
	dx11DeviceImmContext->VSSetSamplers(2, 1, &sampler_LC);
	dx11DeviceImmContext->VSSetSamplers(3, 1, &sampler_PC);
	dx11DeviceImmContext->VSSetSamplers(4, 1, &sampler_LW);
	dx11DeviceImmContext->VSSetSamplers(5, 1, &sampler_PW);

	dx11DeviceImmContext->PSSetSamplers(0, 1, &sampler_LZ);
	dx11DeviceImmContext->PSSetSamplers(1, 1, &sampler_PZ);
	dx11DeviceImmContext->PSSetSamplers(2, 1, &sampler_LC);
	dx11DeviceImmContext->PSSetSamplers(3, 1, &sampler_PC);
	dx11DeviceImmContext->PSSetSamplers(4, 1, &sampler_LW);
	dx11DeviceImmContext->PSSetSamplers(5, 1, &sampler_PW);

	dx11DeviceImmContext->CSSetSamplers(0, 1, &sampler_LZ);
	dx11DeviceImmContext->CSSetSamplers(1, 1, &sampler_PZ);
	dx11DeviceImmContext->CSSetSamplers(2, 1, &sampler_LC);
	dx11DeviceImmContext->CSSetSamplers(3, 1, &sampler_PC);
	dx11DeviceImmContext->CSSetSamplers(4, 1, &sampler_LW);
	dx11DeviceImmContext->CSSetSamplers(5, 1, &sampler_PW);
#pragma endregion 

	ID3D11DepthStencilView* dx11DSVNULL = NULL;
	ID3D11RenderTargetView* dx11RTVsNULL[20] = { };
	ID3D11UnorderedAccessView* dx11UAVs_NULL[20] = { };
	ID3D11ShaderResourceView* dx11SRVs_NULL[20] = { };
	// For Each Primitive //
	int count_call_render = 0;

#define NUM_UAVs_1ST 4
#define NUM_UAVs_2ND 5

	enum class PICKING_TYPE {
		NONE,
		PICK_ON_SCREEN,
		PICK_ON_GS
	};

	auto RenderStage1 = [&](
		vector<VmActor*>& actor_list,
		const MFR_MODE mode_OIT, const RENDER_GEOPASS render_pass, const bool is_frag_counter_buffer,
		const bool is_ghost_mode, const PICKING_TYPE pickType, const bool apply_fragmerge,
		const bool is_MOMENT_gen_buffer, const bool is_group_silhouette, const bool is_2ndlayer)
	{

		// main geometry rendering process
		for (VmActor* actor : actor_list)
		{
			VmVObjectPrimitive* pobj = (VmVObjectPrimitive*)actor->GetGeometryRes();
			PrimitiveData* prim_data = pobj->GetPrimitiveData();
			// note that the actor is visible (already checked)
#pragma region Actor Parameters
			//bool has_texture_img = actor->GetParam("_bool_HasTextureMap", false); // ????


			vmfloat4 material_phongCoeffs = actor->GetParam("_float4_PhongCoeffs", default_phong_lighting_coeff);
			bool use_vertex_color = actor->GetParam("_bool_UseVertexColor", false) && prim_data->GetVerticeDefinition("TEXCOORD0") != NULL;

			//int outline_thickness = actor->GetParam("_int_SilhouetteThickness", (int)0);
			//float outline_depthThres = actor->GetParam("_float_SilhouetteDepthThres", 10000.f);
			//vmfloat3 outline_color = actor->GetParam("_float3_SilhouetteColor", vmfloat3(1));

			// Object Unit Conditions
			bool bf_cull_on = actor->GetParam("_bool_ForcedBackfaceCulling", false);
			bool is_surfel = actor->GetParam("_bool_PointToSurfel", true);

			bool force_to_pointsetrender = actor->GetParam("_bool_ForceToPointsetRender", false);
			bool is_wireframe = actor->GetParam("_bool_IsWireframe", false);

			float cmap_vmin = actor->GetParam("_float_ColorMapVMin", 0.f);
			float cmap_vmax = actor->GetParam("_float_ColorMapVMax", 1.f);
			bool cmap_clip = actor->GetParam("_bool_ColorMapClip", false);
			bool useTriNormal = actor->GetParam("_bool_UseTriNormal", false);
			bool applyUndercut = actor->GetParam("_bool_ApplyUnderCut", false);
			vmfloat3 undercutColor(1.f);
			vmfloat3 undercutDir(0, 0, 1.f);
			if (applyUndercut) {
				undercutColor = actor->GetParam("_float3_UndercutColor", vmfloat3(1.f, 1.f, 0));
				undercutDir = actor->GetParam("_float3_UndercutDir", vmfloat3(0, 0, 1.f));
				vmmath::fNormalizeVector(&undercutDir, &undercutDir);
			}

			bool cmap_windowing = actor->GetParam("_bool_ColorMapAsWindowing", false);
			//float isoValueVolumeActor = actor->GetParam("_float_ColorMapVolumeIsoValue", -1.f);
			VmActor* actorDistTo = actor->GetParam("_VmActor*_DistToActor", (VmActor*)NULL);
			VmObject* actorDistGeoRes = actorDistTo? actorDistTo->GetGeometryRes() : NULL;
			int distanceMapMode = 0; // 0 : none, 1 : to volume, 2 : to mesh
#pragma endregion

#pragma region GPU resource updates
			VmObject* tobj_otf = (VmObject*)actor->GetAssociateRes("OTF");
			VmObject* tobj_maptable = (VmObject*)actor->GetAssociateRes("MAPTABLE"); // single one
			VmVObjectVolume* vobj = (VmVObjectVolume*)actor->GetAssociateRes("VOLUME");
			if (actorDistGeoRes) {
				if (vobj == actorDistGeoRes) distanceMapMode = 1;

				// todo : actorDistGeoRes is mesh
			}

			if (vobj) {
				GpuRes gres_vol;
				grd_helper::UpdateVolumeModel(gres_vol, vobj, false, progress);
				//dx11DeviceImmContext->VSSetShaderResources(1, 1, (ID3D11ShaderResourceView**)&gres_vol.alloc_res_ptrs[DTYPE_SRV]);
				dx11DeviceImmContext->PSSetShaderResources(1, 1, (ID3D11ShaderResourceView**)&gres_vol.alloc_res_ptrs[DTYPE_SRV]);

				if (tobj_otf) {
					GpuRes gres_volblk_otf;
					grd_helper::UpdateOtfBlocks(gres_volblk_otf, vobj, NULL, tobj_otf, -1); // this tagged mask volume is always used even when MIP mode
					dx11DeviceImmContext->PSSetShaderResources(2, 1, (ID3D11ShaderResourceView**)&gres_volblk_otf.alloc_res_ptrs[DTYPE_SRV]);
				}

				vmmat44f matGeoOS2VolOS = actor->GetParam("_matrix44f_GeoOS2VolOS", vmmat44f(1));
				CB_VolumeObject cbVolumeObj;
				grd_helper::SetCb_VolumeObj(cbVolumeObj, vobj, matGeoOS2VolOS, gres_vol, 0, 65535.f, 1.f, false);
				cbVolumeObj.pb_shading_factor = material_phongCoeffs;
				D3D11_MAPPED_SUBRESOURCE mappedResVolObj;
				dx11DeviceImmContext->Map(cbuf_vobj, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResVolObj);
				CB_VolumeObject* cbVolumeObjData = (CB_VolumeObject*)mappedResVolObj.pData;
				memcpy(cbVolumeObjData, &cbVolumeObj, sizeof(CB_VolumeObject));
				dx11DeviceImmContext->Unmap(cbuf_vobj, 0);

				dx11DeviceImmContext->PSSetConstantBuffers(4, 1, &cbuf_vobj);
			}
			if (tobj_maptable) {
				GpuRes gres_tmap_maptble;
				grd_helper::UpdateTMapBuffer(gres_tmap_maptble, tobj_maptable);
				dx11DeviceImmContext->PSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&gres_tmap_maptble.alloc_res_ptrs[DTYPE_SRV]);

				CB_TMAP cbTmap;
				grd_helper::SetCb_TMap(cbTmap, tobj_maptable);
				cbTmap.mapping_v_min = cmap_vmin;
				cbTmap.mapping_v_max = cmap_vmax;
				cbTmap.flag |= (int)cmap_clip;
				D3D11_MAPPED_SUBRESOURCE mappedResTmap;
				dx11DeviceImmContext->Map(cbuf_tmap, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResTmap);
				CB_TMAP* cbTmapData = (CB_TMAP*)mappedResTmap.pData;
				memcpy(cbTmapData, &cbTmap, sizeof(CB_TMAP));
				dx11DeviceImmContext->Unmap(cbuf_tmap, 0);

				dx11DeviceImmContext->PSSetConstantBuffers(5, 1, &cbuf_tmap);
			}

			CB_Material cbRenderEffect;
			grd_helper::SetCb_RenderingEffect(cbRenderEffect, actor);
			D3D11_MAPPED_SUBRESOURCE mappedResRenderEffect;
			dx11DeviceImmContext->Map(cbuf_reffect, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResRenderEffect);
			CB_Material* cbRenderEffectData = (CB_Material*)mappedResRenderEffect.pData;
			memcpy(cbRenderEffectData, &cbRenderEffect, sizeof(CB_Material));
			dx11DeviceImmContext->Unmap(cbuf_reffect, 0);

			GpuRes gres_vtx, gres_idx;
			map<string, GpuRes> map_gres_texs;
			VmObject* imgObj = actor->GetAssociateRes("TEXTURE2DIMAGE");
			bool has_texture_img = false;
			grd_helper::UpdatePrimitiveModel(gres_vtx, gres_idx, map_gres_texs, pobj, imgObj, &has_texture_img);

			if (forcedMaterialColorMode > 0) {
				has_texture_img = false;
				if (forcedMaterialColorMode == 1)
					use_vertex_color = false;
			}

			use_vertex_color &= !has_texture_img;

			int tex_map_enum = 0;
			bool is_annotation_obj = pobj->GetObjParam("_bool_IsAnnotationObj", false) && prim_data->texture_res_info.size() > 0;
			if (is_annotation_obj)
			{
				auto it_tex = map_gres_texs.find("MAP_COLOR4");
				if (it_tex != map_gres_texs.end())
				{
					tex_map_enum = 1;
					GpuRes& gres_tex = it_tex->second;
					dx11DeviceImmContext->PSSetShaderResources(3, 1, (ID3D11ShaderResourceView**)&gres_tex.alloc_res_ptrs[DTYPE_SRV]);
				}
			}
			else if (has_texture_img)
			{
				auto it_tex = map_gres_texs.find("MAP_COLOR4");
				if (it_tex != map_gres_texs.end())
				{
					tex_map_enum |= 0x1;
					GpuRes& gres_tex = it_tex->second;
					dx11DeviceImmContext->PSSetShaderResources(3, 1, (ID3D11ShaderResourceView**)&gres_tex.alloc_res_ptrs[DTYPE_SRV]);
				}

				for (int i = 0; i < NUM_MATERIALS; i++)
				{
					it_tex = map_gres_texs.find(g_materials[i]);
					if (it_tex != map_gres_texs.end())
					{
						tex_map_enum |= (0x1 << (i + 1));
						GpuRes& gres_tex = it_tex->second;
						dx11DeviceImmContext->PSSetShaderResources(10 + i, 1, (ID3D11ShaderResourceView**)&gres_tex.alloc_res_ptrs[DTYPE_SRV]);
					}
				}
			}

			CB_PolygonObject cbPolygonObj;
			cbPolygonObj.tex_map_enum = tex_map_enum;
			cbPolygonObj.pobj_dummy_0 = actor->actorId;// pobj->GetObjectID(); // used for picking
			grd_helper::SetCb_PolygonObj(cbPolygonObj, pobj, actor, matWS2SS, matWS2PS, is_annotation_obj, use_vertex_color);
			cbPolygonObj.pobj_flag |= (int)cmap_windowing << 7;
			
			if (distanceMapMode == 1 && vobj && !is_picking_routine) {
				VolumeData* volData = vobj->GetVolumeData();
				float vtypemax = 1.f;
				if (volData->store_dtype == data_type::dtype<byte>()) vtypemax = 255.f;
				else if (volData->store_dtype == data_type::dtype<ushort>()) vtypemax = 65535.f;
				else assert(0);
				float fisoValue = 0;// isoValueVolumeActor / vtypemax;
				if (tobj_otf) { // && isoValueVolumeActor < 0) {
					MapTable* tobj_data = tobj_otf->GetObjParamPtr<MapTable>("_TableMap_OTF");
					fisoValue = (float)tobj_data->valid_min_idx.x / (float)tobj_data->array_lengths.x;
				}
				cbPolygonObj.pobj_dummy_0 = *(uint*)&fisoValue;
			}
			if (use_vertex_color) {
				cbPolygonObj.Ka = vmfloat3(material_phongCoeffs);
				cbPolygonObj.Ns *= material_phongCoeffs.w;
			}
			else {
				cbPolygonObj.Ka *= material_phongCoeffs.x;
				cbPolygonObj.Kd *= material_phongCoeffs.y;
				cbPolygonObj.Ks *= material_phongCoeffs.z;
				cbPolygonObj.Ns *= material_phongCoeffs.w;
			}
			if (default_color_cmmobj.x >= 0 && default_color_cmmobj.y >= 0 && default_color_cmmobj.z >= 0)
				cbPolygonObj.Ka = cbPolygonObj.Kd = cbPolygonObj.Ks = default_color_cmmobj;
			//if (render_pass == RENDER_GEOPASS::PASS_SILHOUETTE)
			//{
			//	//cbPolygonObj.depth_forward_bias = (float)v_discont_depth;
			//	//cbPolygonObj.pobj_flag |= 0x1;
			//	//cbPolygonObj.pix_thickness = (float)outline_thickness;
			//	//cbPolygonObj.depth_thres = outline_depthThres;
			//	//cbPolygonObj.pobj_dummy_0 = (int)(outline_color.r * 255.f) | (int)(outline_color.g * 255.f) << 8 | (int)(outline_color.b * 255.f) << 16;
			//}
			if (is_ghost_mode)// && !IS_SAFE_OBJ(pobj->GetObjectID()))
			{
				bool is_ghost_surface = actor->GetParam("_bool_IsGhostSurface", false);
				bool is_only_hotspot_visible = actor->GetParam("_bool_IsOnlyHotSpotVisible", false);
				cbPolygonObj.pobj_flag |= (int)is_ghost_surface << 22;
				cbPolygonObj.pobj_flag |= (int)is_only_hotspot_visible << 23;
				//cout << "TEST : " << is_ghost_surface << ", " << is_only_hotspot_visible << endl;
			}
			if (applyUndercut)
			{
				cbPolygonObj.dash_interval = undercutDir.x;
				cbPolygonObj.depth_thres = undercutDir.y;
				cbPolygonObj.pix_thickness = undercutDir.z;
				cbPolygonObj.pobj_dummy_1 = int(undercutColor.b * 255) | (int(undercutColor.g * 255) << 8) | (int(undercutColor.r * 255)) << 16;
			}
			D3D11_MAPPED_SUBRESOURCE mappedResPobjData;
			dx11DeviceImmContext->Map(cbuf_pobj, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResPobjData);
			CB_PolygonObject* cbPolygonObjData = (CB_PolygonObject*)mappedResPobjData.pData;
			memcpy(cbPolygonObjData, &cbPolygonObj, sizeof(CB_PolygonObject));
			dx11DeviceImmContext->Unmap(cbuf_pobj, 0);

			CB_ClipInfo cbClipInfo;
			grd_helper::SetCb_ClipInfo(cbClipInfo, pobj, actor, camClipMode, camClipMatWS2BS, camClipPlanePos, camClipPlaneDir);
			D3D11_MAPPED_SUBRESOURCE mappedResClipInfo;
			dx11DeviceImmContext->Map(cbuf_clip, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResClipInfo);
			CB_ClipInfo* cbClipInfoData = (CB_ClipInfo*)mappedResClipInfo.pData;
			memcpy(cbClipInfoData, &cbClipInfo, sizeof(CB_ClipInfo));
			dx11DeviceImmContext->Unmap(cbuf_clip, 0);
#pragma endregion

#pragma region // Setting Rasterization Stages
			ID3D11InputLayout* dx11InputLayer_Target = NULL;
			ID3D11VertexShader* dx11VS_Target = NULL;
			ID3D11GeometryShader* dx11GS_Target = NULL;
			ID3D11PixelShader* dx11PS_Target = NULL;
			ID3D11RasterizerState2* dx11RState_TargetObj = NULL;
			uint offset = 0;
			D3D_PRIMITIVE_TOPOLOGY pobj_topology_type;

#ifdef DX10_0
			// ASSERT 
			if (!is_picking_routine) {
				assert(render_pass == RENDER_GEOPASS::PASS_OPAQUESURFACES || render_pass == RENDER_GEOPASS::PASS_SILHOUETTE);
				assert(mode_OIT == MFR_MODE::NONE);
			}
#endif

			bool is_picking_routine = pickType != PICKING_TYPE::NONE;
			switch (pickType) {
			case PICKING_TYPE::NONE: 
			case PICKING_TYPE::PICK_ON_SCREEN:
			{
				if (prim_data->GetVerticeDefinition("NORMAL"))
				{
					if (prim_data->GetVerticeDefinition("TEXCOORD0"))
					{
						// PNT (here, T is used as color)
						dx11InputLayer_Target = dx11LI_PNT;
						dx11VS_Target = dx11VShader_PNT;
					}
					else
					{
						// PN
						dx11InputLayer_Target = dx11LI_PN;
						dx11VS_Target = dx11VShader_PN;
					}

					if (is_annotation_obj && dx11InputLayer_Target == dx11LI_PNT) {
#ifdef DX10_0
						dx11PS_Target = GETPS(SR_BASIC_TEXTMAPPING_ps_4_0); // if (render_pass == RENDER_GEOPASS::PASS_OPAQUESURFACES)
#else
						if (render_pass == RENDER_GEOPASS::PASS_OPAQUESURFACES) {
							dx11PS_Target = GETPS(SR_BASIC_TEXTMAPPING_ps_5_0);
						}
						else {
							switch (mode_OIT)
							{
							case MFR_MODE::DYNAMIC_FB: dx11PS_Target = is_picking_routine ? GETPS(PICKING_ABUFFER_TEXTMAPPING_ps_5_0) : GETPS(SR_OIT_ABUFFER_TEXTMAPPING_ps_5_0); break;
							case MFR_MODE::MOMENT: dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_MOMENT_OIT_TEXTMAPPING_ps_5_0) : GETPS(SR_MOMENT_OIT_TEXTMAPPING_ROV_ps_5_0); break;
							case MFR_MODE::DYNAMIC_KB:
								if (apply_fragmerge)
									dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_OIT_FILL_DKBTZ_TEXTMAPPING_ps_5_0) : GETPS(SR_OIT_FILL_DKBTZ_TEXTMAPPING_ROV_ps_5_0);
								else
									dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_OIT_FILL_DKBT_TEXTMAPPING_ps_5_0) : GETPS(SR_OIT_FILL_DKBT_TEXTMAPPING_ROV_ps_5_0);
								break;
							case MFR_MODE::STATIC_KB:
							default:
								if (apply_fragmerge)
									dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_OIT_FILL_SKBTZ_TEXTMAPPING_ps_5_0) : GETPS(SR_OIT_FILL_SKBTZ_TEXTMAPPING_ROV_ps_5_0);
								else
									dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_OIT_FILL_SKBT_TEXTMAPPING_ps_5_0) : GETPS(SR_OIT_FILL_SKBT_TEXTMAPPING_ROV_ps_5_0);
								break;
							}
						}
#endif
					}
					else if (has_texture_img && dx11InputLayer_Target == dx11LI_PNT) {
#ifdef DX10_0
						dx11PS_Target = GETPS(SR_BASIC_TEXTUREIMGMAP_ps_4_0); // render_pass == RENDER_GEOPASS::PASS_OPAQUESURFACES
#else
						if (render_pass == RENDER_GEOPASS::PASS_OPAQUESURFACES) {
							dx11PS_Target = GETPS(SR_BASIC_TEXTUREIMGMAP_ps_5_0);
						}
						else {
							switch (mode_OIT)
							{
							case MFR_MODE::DYNAMIC_FB: dx11PS_Target = is_picking_routine ? GETPS(PICKING_ABUFFER_TEXTUREIMGMAP_ps_5_0) : GETPS(SR_OIT_ABUFFER_TEXTUREIMGMAP_ps_5_0); break;
							case MFR_MODE::MOMENT: dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_MOMENT_OIT_TEXTUREIMGMAP_ps_5_0) : GETPS(SR_MOMENT_OIT_TEXTUREIMGMAP_ROV_ps_5_0); break;
							case MFR_MODE::DYNAMIC_KB:
								if (apply_fragmerge)
									dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_OIT_FILL_DKBTZ_TEXTUREIMGMAP_ps_5_0) : GETPS(SR_OIT_FILL_DKBTZ_TEXTUREIMGMAP_ROV_ps_5_0);
								else
									dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_OIT_FILL_DKBT_TEXTUREIMGMAP_ps_5_0) : GETPS(SR_OIT_FILL_DKBT_TEXTUREIMGMAP_ROV_ps_5_0);
								break;
							case MFR_MODE::STATIC_KB:
							default:
								if (apply_fragmerge)
									dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_OIT_FILL_SKBTZ_TEXTUREIMGMAP_ps_5_0) : GETPS(SR_OIT_FILL_SKBTZ_TEXTUREIMGMAP_ROV_ps_5_0);
								else
									dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_OIT_FILL_SKBT_TEXTUREIMGMAP_ps_5_0) : GETPS(SR_OIT_FILL_SKBT_TEXTUREIMGMAP_ROV_ps_5_0);
								break;
							}
						}
#endif
					}
					else if (vobj && tobj_maptable && render_pass != RENDER_GEOPASS::PASS_SILHOUETTE) {
#ifdef DX10_0
						dx11PS_Target = distanceMapMode == 1 ? GETPS(SR_BASIC_VOLUME_DIST_MAP_ps_4_0) : GETPS(SR_BASIC_VOLUMEMAP_ps_4_0); // render_pass == RENDER_GEOPASS::PASS_OPAQUESURFACES
#else
						if (render_pass == RENDER_GEOPASS::PASS_OPAQUESURFACES) {
							dx11PS_Target = distanceMapMode == 1 ? GETPS(SR_BASIC_VOLUME_DIST_MAP_ps_5_0) : GETPS(SR_BASIC_VOLUMEMAP_ps_5_0);
						}
						else {
							assert(mode_OIT == MFR_MODE::DYNAMIC_FB);
							if (is_picking_routine) {
								dx11PS_Target = GETPS(PICKING_ABUFFER_PHONGBLINN_ps_5_0);
							} 
							else {
								dx11PS_Target = distanceMapMode == 1 ? GETPS(SR_OIT_ABUFFER_VOLUME_DIST_MAP_ps_5_0) : GETPS(SR_OIT_ABUFFER_VOLUMEMAP_ps_5_0);
							}
						}
#endif
					}
					else {
#ifdef DX10_0
						dx11PS_Target = GETPS(SR_BASIC_PHONGBLINN_ps_4_0); // render_pass == RENDER_GEOPASS::PASS_OPAQUESURFACES
#else
						if (render_pass == RENDER_GEOPASS::PASS_OPAQUESURFACES) {
							dx11PS_Target = GETPS(SR_BASIC_PHONGBLINN_ps_5_0);
						}
						else {
							switch (mode_OIT)
							{
							case MFR_MODE::DYNAMIC_FB: dx11PS_Target = is_picking_routine ? GETPS(PICKING_ABUFFER_PHONGBLINN_ps_5_0) : GETPS(SR_OIT_ABUFFER_PHONGBLINN_ps_5_0); break;
							case MFR_MODE::MOMENT: dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_MOMENT_OIT_PHONGBLINN_ps_5_0) : GETPS(SR_MOMENT_OIT_PHONGBLINN_ROV_ps_5_0); break;
							case MFR_MODE::DYNAMIC_KB:
								if (apply_fragmerge)
									dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_OIT_FILL_DKBTZ_PHONGBLINN_ps_5_0) : GETPS(SR_OIT_FILL_DKBTZ_PHONGBLINN_ROV_ps_5_0);
								else
									dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_OIT_FILL_DKBT_PHONGBLINN_ps_5_0) : GETPS(SR_OIT_FILL_DKBT_PHONGBLINN_ROV_ps_5_0);
								break;
							case MFR_MODE::STATIC_KB:
							default:
								if (apply_fragmerge)
									dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_OIT_FILL_SKBTZ_PHONGBLINN_ps_5_0) : GETPS(SR_OIT_FILL_SKBTZ_PHONGBLINN_ROV_ps_5_0);
								else
									dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_OIT_FILL_SKBT_PHONGBLINN_ps_5_0) : GETPS(SR_OIT_FILL_SKBT_PHONGBLINN_ROV_ps_5_0);
								break;
							}
						}
#endif
					}
				}
				else if (prim_data->GetVerticeDefinition("TEXCOORD0"))
				{
					if (prim_data->GetVerticeDefinition("TEXCOORD2"))
					{
						assert(render_pass != RENDER_GEOPASS::PASS_SILHOUETTE);
						// only text case //
						// PTTT
						dx11InputLayer_Target = dx11LI_PTTT;
						dx11VS_Target = dx11VShader_PTTT;

#ifdef DX10_0
						dx11PS_Target = GETPS(SR_BASIC_MULTITEXTMAPPING_ps_4_0); // render_pass == RENDER_GEOPASS::PASS_OPAQUESURFACES
#else
						if (render_pass == RENDER_GEOPASS::PASS_OPAQUESURFACES) {
							dx11PS_Target = GETPS(SR_BASIC_MULTITEXTMAPPING_ps_5_0);
						}
						else {
							switch (mode_OIT)
							{
							case MFR_MODE::DYNAMIC_FB: dx11PS_Target = is_picking_routine ? GETPS(PICKING_ABUFFER_MULTITEXTMAPPING_ps_5_0) : GETPS(SR_OIT_ABUFFER_MULTITEXTMAPPING_ps_5_0); break;
							case MFR_MODE::MOMENT: dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_MOMENT_OIT_MULTITEXTMAPPING_ps_5_0) : GETPS(SR_MOMENT_OIT_MULTITEXTMAPPING_ROV_ps_5_0); break;
							case MFR_MODE::DYNAMIC_KB:
								if (apply_fragmerge)
									dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_OIT_FILL_DKBTZ_MULTITEXTMAPPING_ps_5_0) : GETPS(SR_OIT_FILL_DKBTZ_MULTITEXTMAPPING_ROV_ps_5_0);
								else
									dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_OIT_FILL_DKBT_MULTITEXTMAPPING_ps_5_0) : GETPS(SR_OIT_FILL_DKBT_MULTITEXTMAPPING_ROV_ps_5_0);
								break;
							case MFR_MODE::STATIC_KB:
							default:
								if (apply_fragmerge)
									dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_OIT_FILL_SKBTZ_MULTITEXTMAPPING_ps_5_0) : GETPS(SR_OIT_FILL_SKBTZ_MULTITEXTMAPPING_ROV_ps_5_0);
								else
									dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_OIT_FILL_SKBT_MULTITEXTMAPPING_ps_5_0) : GETPS(SR_OIT_FILL_SKBT_MULTITEXTMAPPING_ROV_ps_5_0);
								break;
							}
						}
#endif
					}
					else // prim_data->GetVerticeDefinition("TEXCOORD2") is NULL
					{
						// if (render_obj_info.use_vertex_color)
						// PT
						dx11InputLayer_Target = dx11LI_PT;
						dx11VS_Target = dx11VShader_PT;

#ifdef DX10_0
						// render_pass == RENDER_GEOPASS::PASS_OPAQUESURFACES
						if (is_annotation_obj)
							dx11PS_Target = GETPS(SR_BASIC_TEXTMAPPING_ps_4_0);
						else if ((cbPolygonObj.pobj_flag & (0x1 << 19)) && prim_data->ptype == PrimitiveTypeLINE)
							dx11PS_Target = GETPS(SR_BASIC_DASHEDLINE_ps_4_0);
						else
							dx11PS_Target = GETPS(SR_BASIC_PHONGBLINN_ps_4_0);
#else
						if (render_pass == RENDER_GEOPASS::PASS_OPAQUESURFACES) {
							if (is_annotation_obj)
								dx11PS_Target = GETPS(SR_BASIC_TEXTMAPPING_ps_5_0);
							else if ((cbPolygonObj.pobj_flag & (0x1 << 19)) && prim_data->ptype == PrimitiveTypeLINE)
								dx11PS_Target = GETPS(SR_BASIC_DASHEDLINE_ps_5_0);
							else
								dx11PS_Target = GETPS(SR_BASIC_PHONGBLINN_ps_5_0);
						}
						else {

							if (is_annotation_obj)
								switch (mode_OIT)
								{
								case MFR_MODE::DYNAMIC_FB: dx11PS_Target = is_picking_routine ? GETPS(PICKING_ABUFFER_TEXTMAPPING_ps_5_0) : GETPS(SR_OIT_ABUFFER_TEXTMAPPING_ps_5_0); break;
								case MFR_MODE::MOMENT: dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_MOMENT_OIT_TEXTMAPPING_ps_5_0) : GETPS(SR_MOMENT_OIT_TEXTMAPPING_ROV_ps_5_0); break;
								case MFR_MODE::DYNAMIC_KB:
									if (apply_fragmerge)
										dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_OIT_FILL_DKBTZ_TEXTMAPPING_ps_5_0) : GETPS(SR_OIT_FILL_DKBTZ_TEXTMAPPING_ROV_ps_5_0);
									else
										dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_OIT_FILL_DKBT_TEXTMAPPING_ps_5_0) : GETPS(SR_OIT_FILL_DKBT_TEXTMAPPING_ROV_ps_5_0);
									break;
								case MFR_MODE::STATIC_KB:
								default:
									if (apply_fragmerge)
										dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_OIT_FILL_SKBTZ_TEXTMAPPING_ps_5_0) : GETPS(SR_OIT_FILL_SKBTZ_TEXTMAPPING_ROV_ps_5_0);
									else
										dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_OIT_FILL_SKBT_TEXTMAPPING_ps_5_0) : GETPS(SR_OIT_FILL_SKBT_TEXTMAPPING_ROV_ps_5_0);
									break;
								}
							else if ((cbPolygonObj.pobj_flag & (0x1 << 19)) && prim_data->ptype == PrimitiveTypeLINE)
								switch (mode_OIT)
								{
								case MFR_MODE::DYNAMIC_FB: dx11PS_Target = is_picking_routine ? GETPS(PICKING_ABUFFER_DASHEDLINE_ps_5_0) : GETPS(SR_OIT_ABUFFER_DASHEDLINE_ps_5_0); break;
								case MFR_MODE::MOMENT: dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_MOMENT_OIT_DASHEDLINE_ps_5_0) : GETPS(SR_MOMENT_OIT_DASHEDLINE_ROV_ps_5_0); break;
								case MFR_MODE::DYNAMIC_KB:
									if (apply_fragmerge)
										dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_OIT_FILL_DKBTZ_DASHEDLINE_ps_5_0) : GETPS(SR_OIT_FILL_DKBTZ_DASHEDLINE_ROV_ps_5_0);
									else
										dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_OIT_FILL_DKBT_DASHEDLINE_ps_5_0) : GETPS(SR_OIT_FILL_DKBT_DASHEDLINE_ROV_ps_5_0);
									break;
								case MFR_MODE::STATIC_KB:
								default:
									if (apply_fragmerge)
										dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_OIT_FILL_SKBTZ_DASHEDLINE_ps_5_0) : GETPS(SR_OIT_FILL_SKBTZ_DASHEDLINE_ROV_ps_5_0);
									else
										dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_OIT_FILL_SKBT_DASHEDLINE_ps_5_0) : GETPS(SR_OIT_FILL_SKBT_DASHEDLINE_ROV_ps_5_0);
									break;
								}
							else
								switch (mode_OIT)
								{
								case MFR_MODE::DYNAMIC_FB: dx11PS_Target = is_picking_routine ? GETPS(PICKING_ABUFFER_PHONGBLINN_ps_5_0) : GETPS(SR_OIT_ABUFFER_PHONGBLINN_ps_5_0); break;
								case MFR_MODE::MOMENT: dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_MOMENT_OIT_PHONGBLINN_ps_5_0) : GETPS(SR_MOMENT_OIT_PHONGBLINN_ROV_ps_5_0); break;
								case MFR_MODE::DYNAMIC_KB:
									if (apply_fragmerge)
										dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_OIT_FILL_DKBTZ_PHONGBLINN_ps_5_0) : GETPS(SR_OIT_FILL_DKBTZ_PHONGBLINN_ROV_ps_5_0);
									else
										dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_OIT_FILL_DKBT_PHONGBLINN_ps_5_0) : GETPS(SR_OIT_FILL_DKBT_PHONGBLINN_ROV_ps_5_0);
									break;
								case MFR_MODE::STATIC_KB:
								default:
									if (apply_fragmerge)
										dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_OIT_FILL_SKBTZ_PHONGBLINN_ps_5_0) : GETPS(SR_OIT_FILL_SKBTZ_PHONGBLINN_ROV_ps_5_0);
									else
										dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_OIT_FILL_SKBT_PHONGBLINN_ps_5_0) : GETPS(SR_OIT_FILL_SKBT_PHONGBLINN_ROV_ps_5_0);
									break;
								}
						}
#endif
					}
				}
				else
				{
					// P
					dx11InputLayer_Target = dx11LI_P;
					dx11VS_Target = dx11VShader_P;

#ifdef DX10_0
					dx11PS_Target = GETPS(SR_BASIC_PHONGBLINN_ps_4_0); // render_pass == RENDER_GEOPASS::PASS_OPAQUESURFACES
#else
					if (render_pass == RENDER_GEOPASS::PASS_OPAQUESURFACES) {
						dx11PS_Target = GETPS(SR_BASIC_PHONGBLINN_ps_5_0);
					}
					else {
						switch (mode_OIT)
						{
						case MFR_MODE::DYNAMIC_FB: dx11PS_Target = is_picking_routine ? GETPS(PICKING_ABUFFER_PHONGBLINN_ps_5_0) : GETPS(SR_OIT_ABUFFER_PHONGBLINN_ps_5_0); break;
						case MFR_MODE::MOMENT: dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_MOMENT_OIT_PHONGBLINN_ps_5_0) : GETPS(SR_MOMENT_OIT_PHONGBLINN_ROV_ps_5_0); break;
						case MFR_MODE::DYNAMIC_KB:
							if (apply_fragmerge)
								dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_OIT_FILL_DKBTZ_PHONGBLINN_ps_5_0) : GETPS(SR_OIT_FILL_DKBTZ_PHONGBLINN_ROV_ps_5_0);
							else
								dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_OIT_FILL_DKBT_PHONGBLINN_ps_5_0) : GETPS(SR_OIT_FILL_DKBT_PHONGBLINN_ROV_ps_5_0);
							break;
						case MFR_MODE::STATIC_KB:
						default:
							if (apply_fragmerge)
								dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_OIT_FILL_SKBTZ_PHONGBLINN_ps_5_0) : GETPS(SR_OIT_FILL_SKBTZ_PHONGBLINN_ROV_ps_5_0);
							else
								dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_OIT_FILL_SKBT_PHONGBLINN_ps_5_0) : GETPS(SR_OIT_FILL_SKBT_PHONGBLINN_ROV_ps_5_0);
							break;
						}
					}
#endif
				}

				switch (render_pass) {
				case RENDER_GEOPASS::PASS_SILHOUETTE:
#ifdef DX10_0
					dx11PS_Target = GETPS(SR_SINGLE_LAYER_ps_4_0); // ONLY K-BUFFER
#else
					dx11PS_Target = GETPS(SR_SINGLE_LAYER_ps_5_0); // ONLY K-BUFFER
#endif
					break;
				case RENDER_GEOPASS::PASS_OIT:
#ifdef DX10_0
					assert(0);
#endif
					if (is_frag_counter_buffer)
					{
						// Create a count of the number of fragments at each pixel location
						if (dx11InputLayer_Target == dx11LI_PTTT)
							dx11PS_Target = GETPS(SR_OIT_ABUFFER_FRAGCOUNTER_MTT_ps_5_0);
						else
							dx11PS_Target = GETPS(SR_OIT_ABUFFER_FRAGCOUNTER_ps_5_0);
					}
					else if (is_MOMENT_gen_buffer)
					{
						if (dx11InputLayer_Target == dx11LI_PTTT)
							dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_MOMENT_GEN_MTT_ps_5_0) : GETPS(SR_MOMENT_GEN_MTT_ROV_ps_5_0);
						else if ((dx11InputLayer_Target == dx11LI_PT || dx11InputLayer_Target == dx11LI_PT) && is_annotation_obj)
							dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_MOMENT_GEN_TEXT_ps_5_0) : GETPS(SR_MOMENT_GEN_TEXT_ROV_ps_5_0);
						else
							dx11PS_Target = use_spinlock_pixsynch ? GETPS(SR_MOMENT_GEN_ps_5_0) : GETPS(SR_MOMENT_GEN_ROV_ps_5_0);
					}
					break;
				default:
					break;
				}

				switch (prim_data->ptype)
				{
				case PrimitiveTypeLINE:
					if (prim_data->is_stripe)
						pobj_topology_type = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
					else
						pobj_topology_type = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
					if (cbPolygonObj.pix_thickness > 0)
#ifdef DX10_0
						dx11GS_Target = GETGS(GS_ThickLines_gs_4_0);
#else
						dx11GS_Target = GETGS(GS_ThickLines_gs_5_0);
#endif
					break;
				case PrimitiveTypeTRIANGLE:
					if (prim_data->is_stripe)
						pobj_topology_type = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
					else
						pobj_topology_type = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
					break;
				case PrimitiveTypePOINT:
					pobj_topology_type = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
					if (cbPolygonObj.pix_thickness > 0)
#ifdef DX10_0
						dx11GS_Target = is_surfel ? GETGS(GS_SurfelPoints_gs_4_0) : GETGS(GS_ThickPoints_gs_4_0);
#else
						dx11GS_Target = is_surfel ? GETGS(GS_SurfelPoints_gs_5_0) : GETGS(GS_ThickPoints_gs_5_0);
#endif

					break;
				default:
					continue;
				}

				if (force_to_pointsetrender)
				{
					dx11RState_TargetObj = GETRASTER(SOLID_NONE);
					pobj_topology_type = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
					if (cbPolygonObj.pix_thickness > 0)
#ifdef DX10_0
						dx11GS_Target = is_surfel ? GETGS(GS_SurfelPoints_gs_4_0) : GETGS(GS_ThickPoints_gs_4_0);
#else
						dx11GS_Target = is_surfel ? GETGS(GS_SurfelPoints_gs_5_0) : GETGS(GS_ThickPoints_gs_5_0);
#endif
				}

				if (pobj_topology_type == D3D11_PRIMITIVE_TOPOLOGY_POINTLIST || pobj_topology_type == D3D11_PRIMITIVE_TOPOLOGY_LINELIST)
					dx11DeviceImmContext->GSSetConstantBuffers(1, 1, &cbuf_pobj);

				if (is_wireframe)
				{
					dx11RState_TargetObj = GETRASTER(WIRE_NONE);
					dx11GS_Target = NULL;
				}
				else
				{

					if (render_pass == RENDER_GEOPASS::PASS_SILHOUETTE || bf_cull_on) {
						dx11RState_TargetObj = GETRASTER(SOLID_CW);
					}
					else {
						dx11RState_TargetObj = GETRASTER(SOLID_NONE);
					}
				}
			}break;
			case PICKING_TYPE::PICK_ON_GS: 
			{
				assert(render_pass == RENDER_GEOPASS::PASS_OIT);
				if (is_annotation_obj || force_to_pointsetrender || is_wireframe || prim_data->ptype != PrimitiveTypeTRIANGLE) continue;

				if (prim_data->GetVerticeDefinition("NORMAL"))
				{
					if (prim_data->GetVerticeDefinition("TEXCOORD0"))
					{
						// PNT (here, T is used as color)
						dx11InputLayer_Target = dx11LI_PNT;
						dx11VS_Target = dx11VShader_PNT;
					}
					else
					{
						// PN
						dx11InputLayer_Target = dx11LI_PN;
						dx11VS_Target = dx11VShader_PN;
					}
				}
				else if (prim_data->GetVerticeDefinition("TEXCOORD0"))
				{
					if (prim_data->GetVerticeDefinition("TEXCOORD2"))
					{
						dx11InputLayer_Target = dx11LI_PTTT;
						dx11VS_Target = dx11VShader_PTTT;
					}
					else // prim_data->GetVerticeDefinition("TEXCOORD2") is NULL
					{
						// if (render_obj_info.use_vertex_color)
						// PT
						dx11InputLayer_Target = dx11LI_PT;
						dx11VS_Target = dx11VShader_PT;
					}
				}
				else
				{
					// P
					dx11InputLayer_Target = dx11LI_P;
					dx11VS_Target = dx11VShader_P;
				}

				// prim_data->ptype == PrimitiveTypeTRIANGLE
				if (prim_data->is_stripe)
					pobj_topology_type = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
				else
					pobj_topology_type = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

				dx11GS_Target = GETGS(GS_PickingBasic_gs_4_0);
				//dx11PS_Target = NULL;
				dx11RState_TargetObj = GETRASTER(SOLID_NONE);

				dx11DeviceImmContext->GSSetConstantBuffers(1, 1, &cbuf_pobj);
				dx11DeviceImmContext->GSSetConstantBuffers(2, 1, &cbuf_clip);

			}break;
			}

			ID3D11Buffer* dx11BufferTargetPrim = (ID3D11Buffer*)gres_vtx.alloc_res_ptrs[DTYPE_RES];
			ID3D11Buffer* dx11IndiceTargetPrim = NULL;
			uint stride_inputlayer = sizeof(vmfloat3) * (uint)prim_data->GetNumVertexDefinitions();
			dx11DeviceImmContext->IASetVertexBuffers(0, 1, (ID3D11Buffer**)&dx11BufferTargetPrim, &stride_inputlayer, &offset);
			if (prim_data->vidx_buffer != NULL && prim_data->ptype != EvmPrimitiveType::PrimitiveTypePOINT)
			{
				dx11IndiceTargetPrim = (ID3D11Buffer*)gres_idx.alloc_res_ptrs[DTYPE_RES];
				dx11DeviceImmContext->IASetIndexBuffer(dx11IndiceTargetPrim, DXGI_FORMAT_R32_UINT, 0);
			}

			dx11DeviceImmContext->IASetInputLayout(dx11InputLayer_Target);
			dx11DeviceImmContext->VSSetShader(dx11VS_Target, NULL, 0);
			if (!is_picking_routine) {
				if (dx11GS_Target == NULL && useTriNormal) {
					dx11GS_Target = GETGS(GS_TriNormal_gs_5_0);
				}
				if (dx11InputLayer_Target != dx11LI_PTTT && applyUndercut)// && render_pass == RENDER_GEOPASS::PASS_OPAQUESURFACES) 
				{

					vmint4* nodePtr;
					int nodeSize;
					vmint4* triWoopPtr;
					int triWoopSize;
					vmint4* triDebugPtr;
					int triDebugSize;
					int* cpuTriIndicesPtr;
					int triIndicesSize;
					if (pobj->GetBVHTreeBuffers(&nodePtr, &nodeSize, &triWoopPtr, &triWoopSize,
						&triDebugPtr, &triDebugSize, &cpuTriIndicesPtr, &triIndicesSize)) {

						GpuRes bvhNode, bvhTriWoop, bvhTriDebug, bvhIndice;
						grd_helper::UpdateCustomBuffer(bvhNode, pobj, "BvhNode", nodePtr, nodeSize, DXGI_FORMAT_R32G32B32A32_FLOAT, sizeof(vmint4));
						grd_helper::UpdateCustomBuffer(bvhTriWoop, pobj, "BvhTriWoop", triWoopPtr, triWoopSize, DXGI_FORMAT_R32G32B32A32_FLOAT, sizeof(vmint4));
						grd_helper::UpdateCustomBuffer(bvhTriDebug, pobj, "BvhTriDebug", triDebugPtr, triDebugSize, DXGI_FORMAT_R32G32B32A32_FLOAT, sizeof(vmint4));
						grd_helper::UpdateCustomBuffer(bvhIndice, pobj, "BvhIndice", cpuTriIndicesPtr, triIndicesSize, DXGI_FORMAT_R32_SINT, sizeof(int));

						dx11DeviceImmContext->PSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&bvhNode.alloc_res_ptrs[DTYPE_SRV]);
						dx11DeviceImmContext->PSSetShaderResources(1, 1, (ID3D11ShaderResourceView**)&bvhTriWoop.alloc_res_ptrs[DTYPE_SRV]);
						dx11DeviceImmContext->PSSetShaderResources(2, 1, (ID3D11ShaderResourceView**)&bvhTriDebug.alloc_res_ptrs[DTYPE_SRV]);
						dx11DeviceImmContext->PSSetShaderResources(3, 1, (ID3D11ShaderResourceView**)&bvhIndice.alloc_res_ptrs[DTYPE_SRV]);

						dx11PS_Target = render_pass == RENDER_GEOPASS::PASS_OPAQUESURFACES ? GETPS(SR_UNDERCUT_ps_5_0) : GETPS(SR_OIT_ABUFFER_UNDERCUT_ps_5_0);
					}
				}
			}
			dx11DeviceImmContext->GSSetShader(dx11GS_Target, NULL, 0);
			dx11DeviceImmContext->PSSetShader(dx11PS_Target, NULL, 0);
			dx11DeviceImmContext->RSSetState(dx11RState_TargetObj);
			dx11DeviceImmContext->IASetPrimitiveTopology(pobj_topology_type);
#pragma endregion 

#pragma region // GEO RENDERING PASS

#define GETDEPTHSTENTIL(NAME) dx11CommonParams->get_depthstencil(#NAME)

			ID3D11RenderTargetView* dx11RTVs[2] = {
				(ID3D11RenderTargetView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_RTV],
				(ID3D11RenderTargetView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_RTV] };
			if (is_2ndlayer) {
				dx11RTVs[0] = (ID3D11RenderTargetView*)gres_fb_singlelayer_rgba.alloc_res_ptrs[DTYPE_RTV];
				dx11RTVs[1] = (ID3D11RenderTargetView*)gres_fb_singlelayer_tempDepth.alloc_res_ptrs[DTYPE_RTV];
			}

#ifdef DX10_0
			if (render_pass == RENDER_GEOPASS::PASS_SILHOUETTE) {
				dx11DeviceImmContext->OMSetRenderTargets(1, (ID3D11RenderTargetView**)&gres_fb_singlelayer_tempDepth.alloc_res_ptrs[DTYPE_RTV], dx11DSV);
			}
			else {
				dx11DeviceImmContext->OMSetRenderTargets(2, dx11RTVs, dx11DSV);
			}
			dx11DeviceImmContext->OMSetDepthStencilState(GETDEPTHSTENTIL(LESSEQUAL), 0);
#else
			ID3D11UnorderedAccessView* dx11UAVs_1st_pass[NUM_UAVs_1ST] = {
					(ID3D11UnorderedAccessView*)gres_fb_counter.alloc_res_ptrs[DTYPE_UAV]
					, use_spinlock_pixsynch ? (ID3D11UnorderedAccessView*)gres_fb_spinlock.alloc_res_ptrs[DTYPE_UAV] : NULL
					, mode_OIT == MFR_MODE::DYNAMIC_FB ? (ID3D11UnorderedAccessView*)gres_fb_ubk_buffer.alloc_res_ptrs[DTYPE_UAV] : (ID3D11UnorderedAccessView*)gres_fb_k_buffer.alloc_res_ptrs[DTYPE_UAV]
					, is_picking_routine ? (ID3D11UnorderedAccessView*)gres_picking_buffer.alloc_res_ptrs[DTYPE_UAV] : NULL
			};

			if (mode_OIT == MFR_MODE::MOMENT)
			{
				if (is_MOMENT_gen_buffer)
				{
					dx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, NULL, 2, 3, dx11UAVs_1st_pass, 0);
				}
				else // if (!is_MOMENT_gen_buffer), which means the main geometry pass
				{
					dx11DeviceImmContext->PSSetShaderResources(20, 1, (ID3D11ShaderResourceView**)&gres_fb_k_buffer.alloc_res_ptrs[DTYPE_SRV]);

					// to do //
					switch (render_pass)
					{
					case RENDER_GEOPASS::PASS_OIT:
						dx11DeviceImmContext->OMSetDepthStencilState(GETDEPTHSTENTIL(ALWAYS), 0);
						if (use_blending_option_MomentOIT)
						{
							ID3D11RenderTargetView* dx11RTVs[2] = {
								(ID3D11RenderTargetView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_RTV],
								(ID3D11RenderTargetView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_RTV] };
							dx11DeviceImmContext->OMSetBlendState(dx11CommonParams->get_blender("ADD"), NULL, 0xffffffff);
							dx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(2, dx11RTVs, dx11DSV, 2, NUM_UAVs_1ST, dx11UAVs_NULL, 0);
						}
						else
						{
							dx11UAVs_1st_pass[0] = (ID3D11UnorderedAccessView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_UAV];
							dx11UAVs_1st_pass[1] = (ID3D11UnorderedAccessView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_UAV];
							dx11UAVs_1st_pass[2] = use_spinlock_pixsynch ? (ID3D11UnorderedAccessView*)gres_fb_spinlock.alloc_res_ptrs[DTYPE_UAV]
								: (ID3D11UnorderedAccessView*)gres_fb_counter.alloc_res_ptrs[DTYPE_UAV];
							dx11UAVs_1st_pass[3] = (ID3D11UnorderedAccessView*)gres_fb_moment_rgba.alloc_res_ptrs[DTYPE_UAV];
							dx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, NULL, 2, NUM_UAVs_1ST, dx11UAVs_1st_pass, 0);
						}
						break;
					case RENDER_GEOPASS::PASS_SILHOUETTE:
						// clear //
						// to do // 별도의 RT 를 만들어 두어야 한다.
						//dx11DeviceImmContext->ClearDepthStencilView(dx11DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
						//dx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(2, dx11RTVs, dx11DSV, 2, NUM_UAVs_1ST, dx11UAVs_NULL, 0);
						//dx11DeviceImmContext->OMSetDepthStencilState(GETDEPTHSTENTIL(LESSEQUAL), 0);
						break;
					}
				}
			}
			else //if (mode_OIT != MFR_MODE::MOMENT)
			{
				switch (render_pass)
				{
				case RENDER_GEOPASS::PASS_OIT:
					if ((mode_OIT == MFR_MODE::DYNAMIC_FB || mode_OIT == MFR_MODE::DYNAMIC_KB) && !is_frag_counter_buffer) // storing fragments
					{
						// weird //mode_OIT == MFR_MODE::DXAB ? (ID3D11UnorderedAccessView*)gres_fb_ref_pidx.alloc_res_ptrs[DTYPE_UAV] : NULL
						//dx11UAVs_1st_pass[3] = (ID3D11UnorderedAccessView*)gres_fb_ref_pidx.alloc_res_ptrs[DTYPE_UAV];
						dx11DeviceImmContext->PSSetShaderResources(50, 1, (ID3D11ShaderResourceView**)&gres_fb_ref_pidx.alloc_res_ptrs[DTYPE_SRV]); // search why this does not work
					}
					
					// ... //
					if (actor->GetParam("_bool_OnlyForemostSurfaces", false)) {
						dx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, dx11DSV, 2, NUM_UAVs_1ST, dx11UAVs_NULL, 0);
						dx11DeviceImmContext->ClearDepthStencilView(dx11DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
						dx11DeviceImmContext->OMSetDepthStencilState(GETDEPTHSTENTIL(LESSEQUAL), 0);

						ID3D11PixelShader* dx11PS_DepthWrite = GETPS(WRITE_DEPTH_ps_5_0);
						dx11DeviceImmContext->PSSetShader(dx11PS_DepthWrite, NULL, 0);
						if (prim_data->is_stripe || pobj_topology_type == D3D11_PRIMITIVE_TOPOLOGY_POINTLIST)
							dx11DeviceImmContext->Draw(prim_data->num_vtx, 0);
						else
							dx11DeviceImmContext->DrawIndexed(prim_data->num_vidx, 0, 0);

						dx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, dx11DSV, 2, NUM_UAVs_1ST, dx11UAVs_1st_pass, NULL);
						dx11DeviceImmContext->PSSetShader(dx11PS_Target, NULL, 0);
						dx11DeviceImmContext->OMSetDepthStencilState(GETDEPTHSTENTIL(LESSEQUAL), 0);
					}
					else {
						dx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, NULL, 2, NUM_UAVs_1ST, dx11UAVs_1st_pass, NULL);
						//dx11DeviceImmContext->OMSetDepthStencilState(GETDEPTHSTENTIL(ALWAYS), 0);
						dx11DeviceImmContext->OMSetDepthStencilState(GETDEPTHSTENTIL(DISABLED), 0);
					}
					break;
				case RENDER_GEOPASS::PASS_SILHOUETTE:
					// clear //
					//dx11DeviceImmContext->ClearDepthStencilView(dx11DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
					//dx11DeviceImmContext->OMSetRenderTargets(1, (ID3D11RenderTargetView**)&gres_fb_singlelayer_tempDepth, dx11DSV);
					dx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(1, (ID3D11RenderTargetView**)&gres_fb_singlelayer_tempDepth.alloc_res_ptrs[DTYPE_RTV], dx11DSV, 2, NUM_UAVs_1ST, dx11UAVs_NULL, 0);
					dx11DeviceImmContext->OMSetDepthStencilState(GETDEPTHSTENTIL(LESSEQUAL), 0);
					break;
				case RENDER_GEOPASS::PASS_OPAQUESURFACES:
					//dx11DeviceImmContext->OMSetRenderTargets(2, dx11RTVs, dx11DSV);
					dx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(2, dx11RTVs, dx11DSV, 2, NUM_UAVs_1ST, dx11UAVs_NULL, 0);
					dx11DeviceImmContext->OMSetDepthStencilState(GETDEPTHSTENTIL(LESSEQUAL), 0);
					break;
				}
			}
#endif
			//dx11DeviceImmContext->Flush();
			if (prim_data->is_stripe || pobj_topology_type == D3D11_PRIMITIVE_TOPOLOGY_POINTLIST)
				dx11DeviceImmContext->Draw(prim_data->num_vtx, 0);
			else
				dx11DeviceImmContext->DrawIndexed(prim_data->num_vidx, 0, 0);
			count_call_render++;

			dx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(2, dx11RTVsNULL, dx11DSVNULL, 2, NUM_UAVs_1ST, dx11UAVs_NULL, 0);

			dx11DeviceImmContext->PSSetShaderResources(50, 1, dx11SRVs_NULL); // note that t50 is assigned for offsettable used in DKB and DFB 
			dx11DeviceImmContext->PSSetShaderResources(0, 4, dx11SRVs_NULL); // note that t50 is assigned for offsettable used in DKB and DFB 
#pragma endregion // GEO RENDERING PASS

			if (render_pass == RENDER_GEOPASS::PASS_SILHOUETTE && !is_group_silhouette) {

				GpuRes gres_quad;
				gres_quad.vm_src_id = 1;
				gres_quad.res_name = string("PROXY_QUAD");

				if (gpu_manager->UpdateGpuResource(gres_quad))
				{
					D3D11_MAPPED_SUBRESOURCE mappedResPobjData;
					dx11DeviceImmContext->Map(cbuf_pobj, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResPobjData);
					CB_PolygonObject* cbPolygonObjData = (CB_PolygonObject*)mappedResPobjData.pData;
					ZeroMemory(cbPolygonObjData, sizeof(CB_PolygonObject));

					int outline_thickness = min(actor->GetParam("_int_SilhouetteThickness", (int)0), 32);
					float outline_depthThres = actor->GetParam("_float_SilhouetteDepthThres", 10000.f);
					vmfloat3 outline_color = actor->GetParam("_float3_SilhouetteColor", vmfloat3(1));
					bool outline_fadeEffect = actor->GetParam("_bool_SilhouetteFadeEffect", true);

					cbPolygonObjData->Kd = outline_color;
					cbPolygonObjData->Ns = scale_z_res;
					cbPolygonObjData->pobj_dummy_0 = (int)outline_fadeEffect;
					cbPolygonObjData->pobj_dummy_1 = outline_thickness;

					vmmat44f matQaudWS2CS;
					vmmath::fMatrixWS2CS(&matQaudWS2CS, &vmfloat3(0, 0, 1), &vmfloat3(0, 1, 0), &vmfloat3(0, 0, -1));
					vmmat44f matQaudCS2PS;
					vmmath::fMatrixOrthogonalCS2PS(&matQaudCS2PS, 2.f, 2.f, 0, 2.f);
					vmmat44f matQaudWS2PS = matQaudWS2CS * matQaudCS2PS;
					cbPolygonObjData->mat_os2ps = TRANSPOSE(matQaudWS2PS);
					dx11DeviceImmContext->Unmap(cbuf_pobj, 0);

					ID3D11Buffer* dx11BufferTargetPrim = (ID3D11Buffer*)gres_quad.alloc_res_ptrs[DTYPE_RES];
					ID3D11Buffer* dx11IndiceTargetPrim = NULL;
					uint stride_inputlayer = sizeof(vmfloat3);
					uint offset = 0;
					dx11DeviceImmContext->IASetVertexBuffers(0, 1, (ID3D11Buffer**)&dx11BufferTargetPrim, &stride_inputlayer, &offset);
					dx11DeviceImmContext->IASetInputLayout(dx11LI_P);
					dx11DeviceImmContext->VSSetShader(dx11VShader_P, NULL, 0);
					dx11DeviceImmContext->GSSetShader(NULL, NULL, 0);
#ifdef DX10_0
					dx11DeviceImmContext->PSSetShader(GETPS(SR_QUAD_OUTLINE_ps_4_0), NULL, 0);
#else
					dx11DeviceImmContext->PSSetShader(GETPS(SR_QUAD_OUTLINE_ps_5_0), NULL, 0);
#endif
					dx11DeviceImmContext->RSSetState(GETRASTER(SOLID_NONE));
					dx11DeviceImmContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
					dx11DeviceImmContext->OMSetDepthStencilState(GETDEPTHSTENTIL(ALWAYS), 0);

#ifdef DX10_0
					dx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gres_fb_singlelayer_rgba.alloc_res_ptrs[DTYPE_RES], (ID3D11Texture2D*)gres_fb_rgba.alloc_res_ptrs[DTYPE_RES]);
					dx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gres_fb_singlelayer_depthcs.alloc_res_ptrs[DTYPE_RES], (ID3D11Texture2D*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_RES]);
					dx11DeviceImmContext->PSSetShaderResources(10, 1, (ID3D11ShaderResourceView**)&gres_fb_singlelayer_rgba.alloc_res_ptrs[DTYPE_SRV]);
					dx11DeviceImmContext->PSSetShaderResources(11, 1, (ID3D11ShaderResourceView**)&gres_fb_singlelayer_tempDepth.alloc_res_ptrs[DTYPE_SRV]);
					dx11DeviceImmContext->PSSetShaderResources(12, 1, (ID3D11ShaderResourceView**)&gres_fb_singlelayer_depthcs.alloc_res_ptrs[DTYPE_SRV]);
					ID3D11RenderTargetView* dx11RTVs[2] = {
						(ID3D11RenderTargetView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_RTV],
						(ID3D11RenderTargetView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_RTV] };
					dx11DeviceImmContext->OMSetRenderTargets(2, dx11RTVs, dx11DSV);
#else
					//dx11DeviceImmContext->PSSetShaderResources(1, 1, (ID3D11ShaderResourceView**)&gres_fb_rgba.alloc_res_ptrs[DTYPE_SRV]);
					dx11DeviceImmContext->PSSetShaderResources(11, 1, (ID3D11ShaderResourceView**)&gres_fb_singlelayer_tempDepth.alloc_res_ptrs[DTYPE_SRV]);
					ID3D11RenderTargetView* dx11RTVs[2] = {
						(ID3D11RenderTargetView*)gres_fb_singlelayer_rgba.alloc_res_ptrs[DTYPE_RTV],
						(ID3D11RenderTargetView*)gres_fb_singlelayer_depthcs.alloc_res_ptrs[DTYPE_RTV] };
					dx11DeviceImmContext->OMSetRenderTargets(2, dx11RTVs, dx11DSV);
#endif

					dx11DeviceImmContext->Draw(4, 0);

					dx11DeviceImmContext->PSSetShaderResources(11, 1, dx11SRVs_NULL);
				}

				dx11DeviceImmContext->ClearDepthStencilView(dx11DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
				dx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gres_fb_singlelayer_tempDepth.alloc_res_ptrs[DTYPE_RTV], clr_float_fltmax_4);

			}
		}
	};

	dx11DeviceImmContext->VSSetConstantBuffers(0, 1, &cbuf_cam_state);
	dx11DeviceImmContext->GSSetConstantBuffers(0, 1, &cbuf_cam_state);
	dx11DeviceImmContext->PSSetConstantBuffers(0, 1, &cbuf_cam_state);
	dx11DeviceImmContext->CSSetConstantBuffers(0, 1, &cbuf_cam_state);

	dx11DeviceImmContext->VSSetConstantBuffers(1, 1, &cbuf_pobj);
	dx11DeviceImmContext->PSSetConstantBuffers(1, 1, &cbuf_pobj);
	dx11DeviceImmContext->CSSetConstantBuffers(1, 1, &cbuf_pobj); // for silhouette or A-buffer test
	dx11DeviceImmContext->PSSetConstantBuffers(2, 1, &cbuf_clip);
	dx11DeviceImmContext->PSSetConstantBuffers(3, 1, &cbuf_reffect);
	dx11DeviceImmContext->CSSetConstantBuffers(3, 1, &cbuf_reffect);

	CB_CameraState cbCamState;
	grd_helper::SetCb_Camera(cbCamState, matWS2SS, matSS2WS, cam_obj, fb_size_cur, k_value, gi_v_thickness);

	auto SetCamConstBuf = [&dx11DeviceImmContext, &cbuf_cam_state](const CB_CameraState& cbCamState) {
		D3D11_MAPPED_SUBRESOURCE mappedResCamState;
		dx11DeviceImmContext->Map(cbuf_cam_state, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResCamState);
		CB_CameraState* cbCamStateData = (CB_CameraState*)mappedResCamState.pData;
		memcpy(cbCamStateData, &cbCamState, sizeof(CB_CameraState));
		dx11DeviceImmContext->Unmap(cbuf_cam_state, 0);
	};

	HWND hWnd = (HWND)_fncontainer->fnParams.GetParam("_hwnd_WindowHandle", (HWND)NULL);

	// RENDER BEGIN
	dx11CommonParams->GpuProfile("SR Render");

	if (is_picking_routine) {
#ifdef DX10_0
#else
		assert(single_layer_routine_objs.size() + foremost_opaque_surfaces_routine_objs.size() 
			+ single_layer_routine_group_objs.size() == 0);
		assert(mode_OIT == MFR_MODE::DYNAMIC_FB);
#endif

		map<int, tuple<float, int>> picking_layers_id_depthPrimId;
		map<float, vmint2> picking_layers_depth_actorid_primid;
		dx11CommonParams->GpuProfile("Picking");
		//pickPrimitive = true;
		ID3D11Buffer* pickSysBuf = NULL;
		if (pickPrimitive) {
			cbCamState.pos_cam_ws = picking_ray_origin;
			cbCamState.dir_view_ws = picking_ray_dir;
			SetCamConstBuf(cbCamState);

			UINT offset[1] = {0};
			dx11DeviceImmContext->SOSetTargets(1, (ID3D11Buffer**)&gres_picking_GSO_buffer.alloc_res_ptrs[DTYPE_RES], offset);
			// draw //
			RenderStage1(general_oit_routine_objs, MFR_MODE::DYNAMIC_FB, RENDER_GEOPASS::PASS_OIT
				, false // is_frag_counter_buffer
				, false
				, PICKING_TYPE::PICK_ON_GS
				, true, false, false, false);

			ID3D11Buffer* bufNull = NULL;
			dx11DeviceImmContext->SOSetTargets(1, (ID3D11Buffer**)&bufNull, offset);

			dx11DeviceImmContext->CopyResource((ID3D11Buffer*)gres_picking_system_GSO_buffer.alloc_res_ptrs[DTYPE_RES],
				(ID3D11Buffer*)gres_picking_GSO_buffer.alloc_res_ptrs[DTYPE_RES]);

			pickSysBuf = (ID3D11Buffer*)gres_picking_system_GSO_buffer.alloc_res_ptrs[DTYPE_RES];
		}
		else {
			cbCamState.iSrCamDummy__1 = (picking_pos_ss.x & 0xFFFF | picking_pos_ss.y << 16);
			cbCamState.cam_flag |= (0x1 << 5);
			SetCamConstBuf(cbCamState);

			RenderStage1(general_oit_routine_objs, MFR_MODE::DYNAMIC_FB, RENDER_GEOPASS::PASS_OIT
				, false // is_frag_counter_buffer
				, false
				, PICKING_TYPE::PICK_ON_SCREEN
				, false, false, false, false);

			dx11DeviceImmContext->CopyResource((ID3D11Buffer*)gres_picking_system_buffer.alloc_res_ptrs[DTYPE_RES],
				(ID3D11Buffer*)gres_picking_buffer.alloc_res_ptrs[DTYPE_RES]);

			pickSysBuf = (ID3D11Buffer*)gres_picking_system_buffer.alloc_res_ptrs[DTYPE_RES];
		}

#pragma region copyback to sysmem
		D3D11_MAPPED_SUBRESOURCE mappedResSysPicking;
		HRESULT hr = dx11DeviceImmContext->Map(pickSysBuf, 0, D3D11_MAP_READ, NULL, &mappedResSysPicking);

		float* picking_buf = (float*)mappedResSysPicking.pData;
		int pick_stride = pickPrimitive ? 3 : 2;
		int num_layers = 0;
		for (int i = 0; i < max_picking_layers; i += pick_stride) {
			uint actorid = 0;
			float pick_depth = -1.f;
			int pid = -1;

			if (pickPrimitive) {
				actorid = *(uint*)&picking_buf[i + 2];
				if (actorid == 0) break;
				pick_depth = picking_buf[i + 0];
				pid = (int)*(uint*)&picking_buf[i + 1];
			}
			else {
				actorid = *(uint*)&picking_buf[i + 0];
				if (actorid == 0) break;
				pick_depth = picking_buf[i + 1];
			}

			auto it = picking_layers_id_depthPrimId.find(actorid);
			if (it == picking_layers_id_depthPrimId.end()) {
				picking_layers_id_depthPrimId.insert(pair<int, tuple<float, int>>(actorid, tuple<float, int>(pick_depth, pid)));
			}
			else {
				float depthOnSampleActor = get<0>(it->second);
				if (pick_depth < depthOnSampleActor) it->second = tuple<float, int>(pick_depth, pid);
			}
			num_layers++;
		}
		//cout << "num_layers : " << num_layers << endl;
		dx11DeviceImmContext->Unmap(pickSysBuf, 0);
#pragma endregion copyback to sysmem
		dx11CommonParams->GpuProfile("Picking", true);

		//if (gpu_profile) {
		//	cout << "### NUM PICKING LAYERS : " << num_layers << endl;
		//	cout << "### NUM PICKING UNIQUE ID LAYERS : " << picking_layers_id_depth.size() << endl;
		//}
		for (auto& it : picking_layers_id_depthPrimId) {
			picking_layers_depth_actorid_primid[get<0>(it.second)] = vmint2(it.first, get<1>(it.second));
		}

		vector<vmfloat3> picking_pos_out;
		vector<int> picking_id_out;
		vector<int> picking_prim_id_out;
		for (auto& it : picking_layers_depth_actorid_primid) {
			vmfloat3 pos_pick = picking_ray_origin + picking_ray_dir * it.first;
			picking_pos_out.push_back(pos_pick);
			picking_id_out.push_back(it.second.x);
			picking_prim_id_out.push_back(it.second.y);
			//std::cout << "PICKING ACTORS : " << it.second << std::endl;
		}

		_fncontainer->fnParams.SetParam("_vlist_float3_PickPos", picking_pos_out);
		_fncontainer->fnParams.SetParam("_vlist_int_PickId", picking_id_out);
		if (pickPrimitive)
			_fncontainer->fnParams.SetParam("_vlist_int_PickPrimitiveId", picking_prim_id_out);
		// END of Render Process for picking
	}
	else if (mode_OIT == MFR_MODE::NONE || mode_OIT == MFR_MODE::DYNAMIC_FB || mode_OIT == MFR_MODE::DYNAMIC_KB || mode_OIT == MFR_MODE::STATIC_KB) {
		
		cbCamState.iSrCamDummy__0 = *(uint*)&merging_beta;
		int oulineiRGB = (int)(outline_color.r * 255.f) | (int)(outline_color.g * 255.f) << 8 | (int)(outline_color.b * 255.f) << 16;
		outline_thickness = min(32, outline_thickness);
		cbCamState.iSrCamDummy__1 = oulineiRGB | outline_thickness << 24;
		cbCamState.iSrCamDummy__2 = *(uint*)&scale_z_res;
		cbCamState.cam_flag |= ((int)outline_fadeEffect << 9); //
		
		SetCamConstBuf(cbCamState);

		if (foremost_opaque_surfaces_routine_objs.size() > 0) 
		{
			dx11DeviceImmContext->ClearDepthStencilView(dx11DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
			dx11CommonParams->GpuProfile("Opaque Surface");
			// Opaque Foremost Rendering ==> to Render Target Textures (gres_fb_rgba and gres_fb_depthcs)
			RenderStage1(foremost_opaque_surfaces_routine_objs, MFR_MODE::NONE, RENDER_GEOPASS::PASS_OPAQUESURFACES
				, false /*is_frag_counter_buffer*/, is_ghost_mode, PICKING_TYPE::NONE, apply_fragmerge, false, false, false);
			dx11CommonParams->GpuProfile("Opaque Surface", true);
		}

		// single.. k buffers... 
		if (single_layer_routine_objs.size() > 0)
		{
			dx11DeviceImmContext->ClearDepthStencilView(dx11DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
			dx11CommonParams->GpuProfile("Single Layer Pass");
			// Single Layer Effect (outline) Rendering 
			// ==> to a Temporary Render Target Texture (gres_fb_singlelayer_depthcs)
			RenderStage1(single_layer_routine_objs, MFR_MODE::NONE, RENDER_GEOPASS::PASS_SILHOUETTE
				, false /*is_frag_counter_buffer*/, is_ghost_mode, PICKING_TYPE::NONE, apply_fragmerge, false, false, false);
			dx11CommonParams->GpuProfile("Single Layer Pass", true);
		}

		if (single_layer_routine_group_objs.size() > 0) 
		{
			dx11DeviceImmContext->ClearDepthStencilView(dx11DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
			dx11CommonParams->GpuProfile("Group Single Layer Pass");
			// Single Layer Effect (outline) Rendering 
			// ==> to a Temporary Render Target Texture (gres_fb_singlelayer_depthcs)
			RenderStage1(single_layer_routine_group_objs, MFR_MODE::NONE, RENDER_GEOPASS::PASS_SILHOUETTE
				, false /*is_frag_counter_buffer*/, is_ghost_mode, PICKING_TYPE::NONE, apply_fragmerge, false, true, false);
			dx11CommonParams->GpuProfile("Group Single Layer Pass", true);

			GpuRes gres_quad;
			gres_quad.vm_src_id = 1;
			gres_quad.res_name = string("PROXY_QUAD");

			if (gpu_manager->UpdateGpuResource(gres_quad)) 
			{
				D3D11_MAPPED_SUBRESOURCE mappedResPobjData;
				dx11DeviceImmContext->Map(cbuf_pobj, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResPobjData);
				CB_PolygonObject* cbPolygonObjData = (CB_PolygonObject*)mappedResPobjData.pData;
				ZeroMemory(cbPolygonObjData, sizeof(CB_PolygonObject));

				vmmat44f matQaudWS2CS;
				vmmath::fMatrixWS2CS(&matQaudWS2CS, &vmfloat3(0, 0, 1), &vmfloat3(0, 1, 0), &vmfloat3(0, 0, -1));
				vmmat44f matQaudCS2PS;
				vmmath::fMatrixOrthogonalCS2PS(&matQaudCS2PS, 2.f, 2.f, 0, 2.f);
				vmmat44f matQaudWS2PS = matQaudWS2CS * matQaudCS2PS;
				cbPolygonObjData->mat_os2ps = TRANSPOSE(matQaudWS2PS);
				dx11DeviceImmContext->Unmap(cbuf_pobj, 0);

				ID3D11Buffer* dx11BufferTargetPrim = (ID3D11Buffer*)gres_quad.alloc_res_ptrs[DTYPE_RES];
				ID3D11Buffer* dx11IndiceTargetPrim = NULL;
				uint stride_inputlayer = sizeof(vmfloat3);
				uint offset = 0;
				dx11DeviceImmContext->IASetVertexBuffers(0, 1, (ID3D11Buffer**)&dx11BufferTargetPrim, &stride_inputlayer, &offset);
				dx11DeviceImmContext->IASetInputLayout(dx11LI_P);
				dx11DeviceImmContext->VSSetShader(dx11VShader_P, NULL, 0);
				dx11DeviceImmContext->GSSetShader(NULL, NULL, 0);
#ifdef DX10_0
				dx11DeviceImmContext->PSSetShader(GETPS(SR_QUAD_OUTLINE_ps_4_0), NULL, 0);
#else
				dx11DeviceImmContext->PSSetShader(GETPS(SR_QUAD_OUTLINE_ps_5_0), NULL, 0);
#endif
				dx11DeviceImmContext->RSSetState(GETRASTER(SOLID_NONE));
				dx11DeviceImmContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				dx11DeviceImmContext->OMSetDepthStencilState(GETDEPTHSTENTIL(ALWAYS), 0);

#ifdef DX10_0
				dx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gres_fb_singlelayer_rgba.alloc_res_ptrs[DTYPE_RES], (ID3D11Texture2D*)gres_fb_rgba.alloc_res_ptrs[DTYPE_RES]);
				dx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gres_fb_singlelayer_depthcs.alloc_res_ptrs[DTYPE_RES], (ID3D11Texture2D*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_RES]);
				dx11DeviceImmContext->PSSetShaderResources(10, 1, (ID3D11ShaderResourceView**)&gres_fb_singlelayer_rgba.alloc_res_ptrs[DTYPE_SRV]);
				dx11DeviceImmContext->PSSetShaderResources(11, 1, (ID3D11ShaderResourceView**)&gres_fb_singlelayer_tempDepth.alloc_res_ptrs[DTYPE_SRV]);
				dx11DeviceImmContext->PSSetShaderResources(12, 1, (ID3D11ShaderResourceView**)&gres_fb_singlelayer_depthcs.alloc_res_ptrs[DTYPE_SRV]);
				ID3D11RenderTargetView* dx11RTVs[2] = {
					(ID3D11RenderTargetView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_RTV],
					(ID3D11RenderTargetView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_RTV] };
				dx11DeviceImmContext->OMSetRenderTargets(2, dx11RTVs, dx11DSV);
#else
				//dx11DeviceImmContext->PSSetShaderResources(1, 1, (ID3D11ShaderResourceView**)&gres_fb_rgba.alloc_res_ptrs[DTYPE_SRV]);
				dx11DeviceImmContext->PSSetShaderResources(11, 1, (ID3D11ShaderResourceView**)&gres_fb_singlelayer_tempDepth.alloc_res_ptrs[DTYPE_SRV]);
				ID3D11RenderTargetView* dx11RTVs[2] = {
					(ID3D11RenderTargetView*)gres_fb_singlelayer_rgba.alloc_res_ptrs[DTYPE_RTV],
					(ID3D11RenderTargetView*)gres_fb_singlelayer_depthcs.alloc_res_ptrs[DTYPE_RTV] };
				dx11DeviceImmContext->OMSetRenderTargets(2, dx11RTVs, dx11DSV);
#endif

				dx11DeviceImmContext->Draw(4, 0);

				dx11DeviceImmContext->PSSetShaderResources(11, 1, dx11SRVs_NULL);
			}
		}

#ifndef DX10_0
		// test version
		if (particle_layer_objs.size() > 0) {
			dx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(2, dx11RTVsNULL, dx11DSVNULL, 2, NUM_UAVs_1ST, dx11UAVs_NULL, 0);

			for (int i = 0; i < (int)particle_layer_objs.size(); i++)
			{
				VmActor* actor = particle_layer_objs[i];

				vmfloat4 material_phongCoeffs = actor->GetParam("_float4_PhongCoeffs", default_phong_lighting_coeff);
				CB_PolygonObject cbPolygonObj;
				{
					vmmat44f matPivot = (actor->GetParam("_matrix44f_Pivot", vmmat44f(1)));
					vmmat44f matPivotInv = (actor->GetParam("_matrix44f_PivotInv", vmmat44f(1)));
					vmmat44f matRS2WS = matPivot * actor->matOS2WS;
					vmmat44f matWS2RS = actor->matWS2OS * matPivotInv;
					cbPolygonObj.mat_os2ws = TRANSPOSE(matRS2WS); // not used
					cbPolygonObj.mat_ws2os = TRANSPOSE(matWS2RS); // not used
					cbPolygonObj.pobj_dummy_0 = actor->actorId; // used for picking

					vmfloat3 illum_model = actor->GetParam("_float3_IllumWavefront", vmfloat3(1));
					// material setting
					cbPolygonObj.alpha = actor->color.a;
					cbPolygonObj.Ka = actor->GetParam("_float3_Ka", vmfloat3(actor->color)) * illum_model.x;
					cbPolygonObj.Kd = actor->GetParam("_float3_Kd", vmfloat3(actor->color)) * illum_model.y;
					cbPolygonObj.Ks = actor->GetParam("_float3_Ks", vmfloat3(actor->color)) * illum_model.z;
					cbPolygonObj.Ns = actor->GetParam("_float_Ns", (float)1.f);;
				}

				cbPolygonObj.Ka *= material_phongCoeffs.x;
				cbPolygonObj.Kd *= material_phongCoeffs.y;
				cbPolygonObj.Ks *= material_phongCoeffs.z;
				cbPolygonObj.Ns *= material_phongCoeffs.w;

				if (default_color_cmmobj.x >= 0 && default_color_cmmobj.y >= 0 && default_color_cmmobj.z >= 0)
					cbPolygonObj.Ka = cbPolygonObj.Kd = cbPolygonObj.Ks = default_color_cmmobj;

				D3D11_MAPPED_SUBRESOURCE mappedResPobjData;
				dx11DeviceImmContext->Map(cbuf_pobj, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResPobjData);
				CB_PolygonObject* cbPolygonObjData = (CB_PolygonObject*)mappedResPobjData.pData;
				memcpy(cbPolygonObjData, &cbPolygonObj, sizeof(CB_PolygonObject));
				dx11DeviceImmContext->Unmap(cbuf_pobj, 0);

				dx11DeviceImmContext->CSSetConstantBuffers(1, 1, &cbuf_pobj);

				//std::string PARTICLE_TYPE = actor->GetParam("_string_ParticleSysType", "METABALL");
				std::string PARTICLE_TYPE = actor->GetParam("_string_ParticleSysType", "NORMAL");
				if (PARTICLE_TYPE == "METABALL") {

					ID3D11Buffer* cbuf_particleblob = dx11CommonParams->get_cbuf("CB_Particle_Blob");
					dx11DeviceImmContext->CSSetConstantBuffers(11, 1, &cbuf_particleblob);

					D3D11_MAPPED_SUBRESOURCE mappedResPclBlob;
					dx11DeviceImmContext->Map(cbuf_particleblob, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResPclBlob);
					CB_Particle_Blob* cbPclBlobData = (CB_Particle_Blob*)mappedResPclBlob.pData;

					float smoothCoeff = actor->GetParam("_float_Smoothness", 10.f);
					glm::fvec3 minRoiCube = actor->GetParam("_fvec3_MinRoiCube", glm::fvec3(-50.f));
					glm::fvec3 maxRoiCube = actor->GetParam("_fvec3_MaxRoiCube", glm::fvec3(50.f));
					//vzm::SetActorParams(aidBlobParticle, apParticleActor);
					std::vector<glm::fvec3>* centerSpheres = actor->GetParamPtr<std::vector<glm::fvec3>>("_vector<fvec3>_SphereCenter");
					std::vector<glm::fvec4>* colorSpheres = actor->GetParamPtr<std::vector<glm::fvec4>>("_vector<fvec4>_SphereColor");
					std::vector<float>* radiusSpheres = actor->GetParamPtr<std::vector<float>>("_vector<float>_SphereRadius");
					if (centerSpheres == NULL || colorSpheres == NULL || radiusSpheres == NULL) {
						vmlog::LogErr(actor->name + " has no sphere data!!");
					}
					else {
						for (int i = 0; i < (int)centerSpheres->size(); i++) {
							vmfloat3 xyz = centerSpheres->at(i);
							cbPclBlobData->xyzr_spheres[i] = vmfloat4(xyz, radiusSpheres->at(i));
							vmfloat4 color = colorSpheres->at(i);
							int iColor = (int)(color.r * 255.f) | (int)(color.g * 255.f) << 8 | (int)(color.b * 255.f) << 16 | (int)(color.a * 255.f) << 24;
							//cbPclBlobData->color_spheres = vmint4(iColor);
							memcpy(&((int*)&cbPclBlobData->color_spheres)[i], &iColor, sizeof(int));
						}
						cbPclBlobData->smoothCoeff = smoothCoeff;
						cbPclBlobData->minRoiCube = minRoiCube;
						cbPclBlobData->maxRoiCube = maxRoiCube;
					}
					dx11DeviceImmContext->Unmap(cbuf_particleblob, 0);


					ID3D11UnorderedAccessView* dx11UAVs[3] = {
							(ID3D11UnorderedAccessView*)gres_fb_singlelayer_rgba.alloc_res_ptrs[DTYPE_UAV]
							, (ID3D11UnorderedAccessView*)gres_fb_singlelayer_depthcs.alloc_res_ptrs[DTYPE_UAV]
							, (ID3D11UnorderedAccessView*)gres_fb_singlelayer_tempDepth.alloc_res_ptrs[DTYPE_UAV]
					};

					dx11DeviceImmContext->CSSetShader(GETCS(PCE_BlobRayMarching_cs_5_0), NULL, 0);
					dx11DeviceImmContext->CSSetUnorderedAccessViews(1, 3, dx11UAVs, (UINT*)(&dx11UAVs));

					dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);
				}
				else if (PARTICLE_TYPE == "NORMAL") {
					const int MAX_PARTICLES = 1000;
					VmVObjectPrimitive* pobj = (VmVObjectPrimitive*)actor->GetGeometryRes();
					int pobjId = pobj ? pobj->GetObjectID() : 1234567;

					// rendering resources //
					GpuRes gres_vb_pos, gres_vb_nor, gres_vb_tex, gres_vb_color, gres_vb_idx;
					{
						gres_vb_pos.vm_src_id = pobjId;
						gres_vb_pos.res_name = string("gres_particle_vb_pos");
						if (!gpu_manager->UpdateGpuResource(gres_vb_pos)) {
							gres_vb_pos.rtype = RTYPE_BUFFER;
							gres_vb_pos.options["USAGE"] = D3D11_USAGE_DEFAULT;
							gres_vb_pos.options["CPU_ACCESS_FLAG"] = NULL;
							gres_vb_pos.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
							gres_vb_pos.options["FORMAT"] = DXGI_FORMAT_R32_TYPELESS; // note RGB32F is not allowed for Views
							gres_vb_pos.options["RAW_ACCESS"] = 1;
							gres_vb_pos.res_values.SetParam("NUM_ELEMENTS", (uint)(MAX_PARTICLES * 4 * 3) );
							gres_vb_pos.res_values.SetParam("STRIDE_BYTES", 4u);

							// D3D11_BIND_VERTEX_BUFFER 는 shader resource view 가 안 되는 것일까? 되네 :)
							gpu_manager->GenerateGpuResource(gres_vb_pos);
						}
						gres_vb_nor = gres_vb_pos;
						gres_vb_nor.res_name = string("gres_particle_vb_nor");
						if (!gpu_manager->UpdateGpuResource(gres_vb_pos)) {

							// D3D11_BIND_VERTEX_BUFFER 는 shader resource view 가 안 되는 것일까? 되네 :)
							gpu_manager->GenerateGpuResource(gres_vb_pos);
						}

						gres_vb_tex = gres_vb_pos;
						gres_vb_tex.res_name = string("gres_particle_vb_tex");
						if (!gpu_manager->UpdateGpuResource(gres_vb_tex)) {
							gres_vb_tex.rtype = RTYPE_BUFFER;
							gres_vb_tex.options["USAGE"] = D3D11_USAGE_DEFAULT;
							gres_vb_tex.options["CPU_ACCESS_FLAG"] = NULL;
							gres_vb_tex.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
							gres_vb_tex.options["FORMAT"] = DXGI_FORMAT_R16G16_UNORM; 
							gres_vb_tex.options["RAW_ACCESS"] = 0;
							gres_vb_tex.res_values.SetParam("NUM_ELEMENTS", (uint)(MAX_PARTICLES * 4));
							gres_vb_tex.res_values.SetParam("STRIDE_BYTES", 4u);

							// D3D11_BIND_VERTEX_BUFFER 는 shader resource view 가 안 되는 것일까? 되네 :)
							gpu_manager->GenerateGpuResource(gres_vb_tex);
						}

						gres_vb_color= gres_vb_pos;
						gres_vb_color.res_name = string("gres_particle_vb_color");
						if (!gpu_manager->UpdateGpuResource(gres_vb_color)) {
							gres_vb_color.rtype = RTYPE_BUFFER;
							gres_vb_color.options["USAGE"] = D3D11_USAGE_DEFAULT;
							gres_vb_color.options["CPU_ACCESS_FLAG"] = NULL;
							gres_vb_color.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
							gres_vb_color.options["FORMAT"] = DXGI_FORMAT_R8G8B8A8_UNORM;
							gres_vb_color.options["RAW_ACCESS"] = 0;
							gres_vb_color.res_values.SetParam("NUM_ELEMENTS", (uint)(MAX_PARTICLES * 4));
							gres_vb_color.res_values.SetParam("STRIDE_BYTES", 4u);

							// D3D11_BIND_VERTEX_BUFFER 는 shader resource view 가 안 되는 것일까? 되네 :)
							gpu_manager->GenerateGpuResource(gres_vb_color);
						}

						gres_vb_idx.vm_src_id = pobjId;
						gres_vb_idx.res_name = string("PARTICLE_VB_IDX");
						if (!gpu_manager->UpdateGpuResource(gres_vb_idx))
						{
							gres_vb_idx.rtype = RTYPE_BUFFER;
							gres_vb_idx.options["USAGE"] = D3D11_USAGE_DEFAULT;
							gres_vb_idx.options["CPU_ACCESS_FLAG"] = NULL;
							gres_vb_idx.options["BIND_FLAG"] = D3D11_BIND_INDEX_BUFFER;
							gres_vb_idx.options["FORMAT"] = DXGI_FORMAT_R32_UINT;
							gres_vb_idx.res_values.SetParam("NUM_ELEMENTS", (uint)(MAX_PARTICLES * 6));
							gres_vb_idx.res_values.SetParam("STRIDE_BYTES", 4u);

							gpu_manager->GenerateGpuResource(gres_vb_idx);

							std::vector<uint> indexBuffer(MAX_PARTICLES * 6);
							for (int i = 0; i < MAX_PARTICLES; i++)
							{
								uint v0 = i * 4;
								uint i0 = i * 6;
								indexBuffer[i0 + 0] = v0 + 0;
								indexBuffer[i0 + 1] = v0 + 1;
								indexBuffer[i0 + 2] = v0 + 2;
								indexBuffer[i0 + 3] = v0 + 2;
								indexBuffer[i0 + 4] = v0 + 1;
								indexBuffer[i0 + 5] = v0 + 3;
							}

							D3D11_SUBRESOURCE_DATA subres;
							subres.pSysMem = &indexBuffer[0];
							subres.SysMemPitch = sizeof(uint) * MAX_PARTICLES;
							subres.SysMemSlicePitch = 0; // only for 3D resource
							dx11DeviceImmContext->UpdateSubresource((ID3D11Resource*)gres_vb_idx.alloc_res_ptrs[DesType::DTYPE_RES], 0, NULL, subres.pSysMem, subres.SysMemPitch, 0);
						}

					}

					GpuRes gres_particle;
					gres_particle.vm_src_id = pobjId;
					gres_particle.res_name = string("PARTICLE_EMITTER");
					if (!gpu_manager->UpdateGpuResource(gres_particle)) {
						gres_particle.rtype = RTYPE_BUFFER;
						gres_particle.options["USAGE"] = D3D11_USAGE_DEFAULT;
						gres_particle.options["CPU_ACCESS_FLAG"] = 0;
						gres_particle.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
						gres_particle.options["FORMAT"] = DXGI_FORMAT_UNKNOWN;
						gres_particle.options["MISC"] = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
						gres_particle.res_values.SetParam("NUM_ELEMENTS", (const uint)MAX_PARTICLES);
						gres_particle.res_values.SetParam("STRIDE_BYTES", (const uint)sizeof(Particle));
						gpu_manager->GenerateGpuResource(gres_particle);
					}
					GpuRes gres_aliveList0 = gres_particle;
					gres_aliveList0.res_name = string("PARTICLE_ALIVELIST0");
					if (!gpu_manager->UpdateGpuResource(gres_aliveList0)) {
						gres_aliveList0.res_values.SetParam("NUM_ELEMENTS", (const uint)MAX_PARTICLES);
						gres_aliveList0.res_values.SetParam("STRIDE_BYTES", (const uint)sizeof(uint));
						gpu_manager->GenerateGpuResource(gres_aliveList0);
					}
					GpuRes gres_aliveList1 = gres_aliveList0;
					gres_aliveList1.res_name = string("PARTICLE_ALIVELIST1");
					if (!gpu_manager->UpdateGpuResource(gres_aliveList1)) {
						gpu_manager->GenerateGpuResource(gres_aliveList1);
					}
					GpuRes gres_culledIndirect0 = gres_aliveList0;
					gres_culledIndirect0.res_name = string("PARTICLE_CULLEDINDIRECT0");
					if (!gpu_manager->UpdateGpuResource(gres_culledIndirect0)) {
						gpu_manager->GenerateGpuResource(gres_culledIndirect0);
					}
					GpuRes gres_culledIndirect1 = gres_aliveList0;
					gres_culledIndirect1.res_name = string("PARTICLE_CULLEDINDIRECT1");
					if (!gpu_manager->UpdateGpuResource(gres_culledIndirect1)) {
						gpu_manager->GenerateGpuResource(gres_culledIndirect1);
					}

					GpuRes gres_deadList = gres_aliveList0;
					gres_deadList.res_name = string("PARTICLE_DEADLIST");
					if (!gpu_manager->UpdateGpuResource(gres_deadList)) {
						gpu_manager->GenerateGpuResource(gres_deadList);

						// Dead index list:
						std::vector<uint> deadList(MAX_PARTICLES);
						for (int i = 0; i < MAX_PARTICLES; i++) deadList[i] = i;

						D3D11_SUBRESOURCE_DATA subres;
						subres.pSysMem = &deadList[0];
						subres.SysMemPitch = sizeof(uint) * MAX_PARTICLES;
						subres.SysMemSlicePitch = 0; // only for 3D resource
						dx11DeviceImmContext->UpdateSubresource((ID3D11Resource*)gres_deadList.alloc_res_ptrs[DesType::DTYPE_RES], 0, NULL, subres.pSysMem, subres.SysMemPitch, 0);
					}

					GpuRes gres_counter;
					gres_counter.vm_src_id = pobjId;
					gres_counter.res_name = string("PARTICLE_COUNTER");
					if (!gpu_manager->UpdateGpuResource(gres_counter)) {
						gres_counter.rtype = RTYPE_BUFFER;
						gres_counter.options["USAGE"] = D3D11_USAGE_DEFAULT;
						gres_counter.options["CPU_ACCESS_FLAG"] = 0;
						gres_counter.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
						gres_counter.options["FORMAT"] = DXGI_FORMAT_R32_TYPELESS;
						gres_counter.options["RAW_ACCESS"] = 1;
						gres_counter.res_values.SetParam("NUM_ELEMENTS", (uint)(sizeof(ParticleCounters) / 4));
						gres_counter.res_values.SetParam("STRIDE_BYTES", 4u);
						gpu_manager->GenerateGpuResource(gres_counter);

						ParticleCounters counters;
						counters.aliveCount = 0;
						counters.deadCount = MAX_PARTICLES;
						counters.realEmitCount = 0;
						counters.aliveCount_afterSimulation = 0;
						counters.cellAllocator = 0;
						
						D3D11_SUBRESOURCE_DATA subres;
						subres.pSysMem = &counters;
						subres.SysMemPitch = sizeof(ParticleCounters);
						subres.SysMemSlicePitch = 0; // only for 3D resource
						dx11DeviceImmContext->UpdateSubresource((ID3D11Resource*)gres_counter.alloc_res_ptrs[DesType::DTYPE_RES], 0, NULL, subres.pSysMem, subres.SysMemPitch, 0);
					}

					GpuRes gres_indirect = gres_aliveList0;
					gres_indirect.res_name = string("PARTICLE_INDIRECT");
					if (!gpu_manager->UpdateGpuResource(gres_indirect)) {
						
						gres_indirect.rtype = RTYPE_BUFFER;
						gres_indirect.options["USAGE"] = D3D11_USAGE_DEFAULT;
						gres_indirect.options["CPU_ACCESS_FLAG"] = 0;
						gres_indirect.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
						gres_indirect.options["FORMAT"] = DXGI_FORMAT_R32_TYPELESS;
						gres_indirect.options["RAW_ACCESS"] = 1;
						gres_indirect.options["MISC"] = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
						uint stride = AlignTo(sizeof(IndirectDispatchArgs), (uint64_t)4) +
							AlignTo(sizeof(IndirectDispatchArgs), (uint64_t)4) +
							AlignTo(sizeof(IndirectDrawArgsInstanced), (uint64_t)4);
						gres_indirect.res_values.SetParam("NUM_ELEMENTS", stride / 4u);
						gres_indirect.res_values.SetParam("STRIDE_BYTES", 4u);
						gpu_manager->GenerateGpuResource(gres_indirect);
					}

					GpuRes gres_distance = gres_aliveList0;
					gres_distance.res_name = string("PARTICLE_DISTANCE");
					if (!gpu_manager->UpdateGpuResource(gres_distance)) {
						gpu_manager->GenerateGpuResource(gres_distance);

						// Dead index list:
						std::vector<float> distList(MAX_PARTICLES, 0.f);
						//for (int i = 0; i < MAX_PARTICLES; i++) distList[i] = 0.f;

						D3D11_SUBRESOURCE_DATA subres;
						subres.pSysMem = &distList[0];
						subres.SysMemPitch = sizeof(float) * MAX_PARTICLES;
						subres.SysMemSlicePitch = 0; // only for 3D resource
						dx11DeviceImmContext->UpdateSubresource((ID3D11Resource*)gres_distance.alloc_res_ptrs[DesType::DTYPE_RES], 0, NULL, subres.pSysMem, subres.SysMemPitch, 0);
					}

					// particle update //
					{
						dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 20, dx11UAVs_NULL, NULL);
						dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, (ID3D11UnorderedAccessView**)&gres_particle.alloc_res_ptrs[DesType::DTYPE_UAV], NULL);
						dx11DeviceImmContext->CSSetUnorderedAccessViews(1, 1, (ID3D11UnorderedAccessView**)&gres_aliveList0.alloc_res_ptrs[DesType::DTYPE_UAV], NULL);
						dx11DeviceImmContext->CSSetUnorderedAccessViews(2, 1, (ID3D11UnorderedAccessView**)&gres_aliveList1.alloc_res_ptrs[DesType::DTYPE_UAV], NULL);
						dx11DeviceImmContext->CSSetUnorderedAccessViews(3, 1, (ID3D11UnorderedAccessView**)&gres_deadList.alloc_res_ptrs[DesType::DTYPE_UAV], NULL);
						dx11DeviceImmContext->CSSetUnorderedAccessViews(4, 1, (ID3D11UnorderedAccessView**)&gres_counter.alloc_res_ptrs[DesType::DTYPE_UAV], NULL);
						dx11DeviceImmContext->CSSetUnorderedAccessViews(5, 1, (ID3D11UnorderedAccessView**)&gres_indirect.alloc_res_ptrs[DesType::DTYPE_UAV], NULL);
						dx11DeviceImmContext->CSSetUnorderedAccessViews(6, 1, (ID3D11UnorderedAccessView**)&gres_distance.alloc_res_ptrs[DesType::DTYPE_UAV], NULL);

						dx11DeviceImmContext->CSSetUnorderedAccessViews(7, 1, (ID3D11UnorderedAccessView**)&gres_vb_pos.alloc_res_ptrs[DesType::DTYPE_UAV], NULL);
						dx11DeviceImmContext->CSSetUnorderedAccessViews(8, 1, (ID3D11UnorderedAccessView**)&gres_vb_nor.alloc_res_ptrs[DesType::DTYPE_UAV], NULL);
						dx11DeviceImmContext->CSSetUnorderedAccessViews(9, 1, (ID3D11UnorderedAccessView**)&gres_vb_tex.alloc_res_ptrs[DesType::DTYPE_UAV], NULL);
						dx11DeviceImmContext->CSSetUnorderedAccessViews(10, 1, (ID3D11UnorderedAccessView**)&gres_vb_color.alloc_res_ptrs[DesType::DTYPE_UAV], NULL);

						dx11DeviceImmContext->CSSetUnorderedAccessViews(11, 1, (ID3D11UnorderedAccessView**)&gres_culledIndirect0.alloc_res_ptrs[DesType::DTYPE_UAV], NULL);
						dx11DeviceImmContext->CSSetUnorderedAccessViews(12, 1, (ID3D11UnorderedAccessView**)&gres_culledIndirect1.alloc_res_ptrs[DesType::DTYPE_UAV], NULL);

						ID3D11Buffer* cbuf_Emitter = dx11CommonParams->get_cbuf("CB_Emitter");
						dx11DeviceImmContext->CSSetConstantBuffers(11, 1, &cbuf_Emitter);
						ID3D11Buffer* cbuf_Frame = dx11CommonParams->get_cbuf("CB_Frame");
						dx11DeviceImmContext->CSSetConstantBuffers(12, 1, &cbuf_Frame);

						CB_Frame cbFrame = {};
						{
							static uint frameCount = 0;
							frameCount++;

							static float time = 0, time_previous = 0;
							static std::chrono::high_resolution_clock::time_point timestamp = std::chrono::high_resolution_clock::now();
							auto timestamp2 = std::chrono::high_resolution_clock::now();
							std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(timestamp2 - timestamp);
							auto elapsed = time_span.count();
							timestamp = timestamp2;
							time_previous = time;
							time += cbFrame.delta_time;

							cbFrame.frame_count = frameCount;
							cbFrame.delta_time = (float)elapsed;
							cbFrame.time = time;
							cbFrame.time_previous = time_previous;
						}
						D3D11_MAPPED_SUBRESOURCE mappedResFrame;
						dx11DeviceImmContext->Map(cbuf_Frame, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResFrame);
						CB_Frame* cbGpuFrame = (CB_Frame*)mappedResFrame.pData;
						memcpy(cbGpuFrame, &cbFrame, sizeof(CB_Frame));
						dx11DeviceImmContext->Unmap(cbuf_Frame, 0);

						CB_Emitter cbEmitter = {};
						{
							cbEmitter.xEmitterTransform = TRANSPOSE(actor->matOS2WS); // cbPolygonObj.mat_os2ws
							//if (mesh == nullptr || !IsFormatUnorm(mesh->position_format) || mesh->so_pos.IsValid())
							//{
							//	cb.xEmitterBaseMeshUnormRemap.init();
							//}
							//else
							//{
							//	XMFLOAT4X4 unormRemap;
							//	XMStoreFloat4x4(&unormRemap, mesh->aabb.getUnormRemapMatrix());
							//	cb.xEmitterBaseMeshUnormRemap.Create(unormRemap);
							//}
							cbEmitter.xEmitCount = (uint32_t)actor->GetParam("_int_EmitCount", (int)0);
							//if (mesh == nullptr)
							{
								cbEmitter.xEmitterMeshGeometryOffset = 0;
								cbEmitter.xEmitterMeshGeometryCount = 0;
							}
							//else
							//{
							//	uint32_t first_subset = 0;
							//	uint32_t last_subset = 0;
							//	mesh->GetLODSubsetRange(0, first_subset, last_subset);
							//	cb.xEmitterMeshGeometryOffset = mesh->geometryOffset + first_subset;
							//	cb.xEmitterMeshGeometryCount = last_subset - first_subset;
							//}
							srand(time(NULL));
							cbEmitter.xEmitterRandomness = (rand() % 100) * 0.01f;
							cbEmitter.xParticleLifeSpan = actor->GetParam("_float_LifeSpan", (float)1.f);
							cbEmitter.xParticleLifeSpanRandomness = actor->GetParam("_float_LifeSpanRandomness", (float)1.f);
							cbEmitter.xParticleNormalFactor = actor->GetParam("_float_NormFactor", (float)1.f);
							cbEmitter.xParticleRandomFactor = actor->GetParam("_float_RandomFactor", (float)1.f);
							cbEmitter.xParticleScaling = actor->GetParam("_float_ScaleFactor", (float)1.f);
							cbEmitter.xParticleSize = actor->GetParam("_float_ScaleSize", (float)1.f);
							cbEmitter.xParticleMotionBlurAmount = actor->GetParam("_float_MotionBlurAmount", (float)0.f);
							cbEmitter.xParticleRotation = actor->GetParam("_float_RotationDeg", (float)0.f) * VM_PI * 60.f;
							cbEmitter.xParticleMass = actor->GetParam("_float_Mass", (float)1.f);
							cbEmitter.xEmitterMaxParticleCount = MAX_PARTICLES;
							cbEmitter.xEmitterFixedTimestep = actor->GetParam("_float_FixedTimeStep", (float)-1.f);
							cbEmitter.xEmitterRestitution = actor->GetParam("_float_Restitution", (float)0.98f);
							cbEmitter.xEmitterFramesXY = vmuint2(std::max(1u, (uint)actor->GetParam("_int_FramesX", (int)1)), 
								std::max(1u, (uint)actor->GetParam("_int_FramesY", (int)1)));
							cbEmitter.xEmitterFrameCount = std::max(1u, (uint)actor->GetParam("_int_FrameCount", (int)1));
							cbEmitter.xEmitterFrameStart = (uint)actor->GetParam("_int_FrameStart", (int)0);
							cbEmitter.xEmitterTexMul = vmfloat2(1.0f / (float)cbEmitter.xEmitterFramesXY.x, 1.0f / (float)cbEmitter.xEmitterFramesXY.y);
							cbEmitter.xEmitterFrameRate = actor->GetParam("_float_FrameRate", (float)0.f);
							cbEmitter.xParticleGravity = vmfloat3(0, 0, 0);
							cbEmitter.xParticleDrag = actor->GetParam("_float_ParticleDrag", (float)1.f);
							vmfloat3 velosity = actor->GetParam("_float3_Velocity", (vmfloat3)0.f);
							vmmath::fTransformVector(&cbEmitter.xParticleVelocity, &velosity, &actor->matOS2WS);
							cbEmitter.xParticleRandomColorFactor = (rand() % 100) * 0.01f;
							cbEmitter.xEmitterLayerMask = ~0u;
							cbEmitter.xEmitterInstanceIndex = 0;

							cbEmitter.xEmitterOptions = 0;
							//if (IsSPHEnabled())
							//{
							//	cb.xEmitterOptions |= EMITTER_OPTION_BIT_SPH_ENABLED;
							//}
							//if (IsFrameBlendingEnabled())
							//{
							//	cb.xEmitterOptions |= EMITTER_OPTION_BIT_FRAME_BLENDING_ENABLED;
							//}
							//if (ALLOW_MESH_SHADER && device->CheckCapability(GraphicsDeviceCapability::MESH_SHADER))
							//{
							//	cb.xEmitterOptions |= EMITTER_OPTION_BIT_MESH_SHADER_ENABLED;
							//}
							//if (IsCollidersDisabled())
							//{
							//	cb.xEmitterOptions |= EMITTER_OPTION_BIT_COLLIDERS_DISABLED;
							//}
							//if (_flags & FLAG_USE_RAIN_BLOCKER)
							//{
							//	cb.xEmitterOptions |= EMITTER_OPTION_BIT_USE_RAIN_BLOCKER;
							//}
							//if (IsTakeColorFromMesh())
							//{
							//	cb.xEmitterOptions |= EMITTER_OPTION_BIT_TAKE_COLOR_FROM_MESH;
							//}

							// SPH:
							//cb.xSPH_h = SPH_h;
							//cb.xSPH_h_rcp = 1.0f / SPH_h;
							//cb.xSPH_h2 = SPH_h * SPH_h;
							//cb.xSPH_h3 = cb.xSPH_h2 * SPH_h;
							//const float h6 = cb.xSPH_h2 * cb.xSPH_h2 * cb.xSPH_h2;
							//const float h9 = h6 * cb.xSPH_h3;
							//cb.xSPH_poly6_constant = (315.0f / (64.0f * XM_PI * h9));
							//cb.xSPH_spiky_constant = (-45.0f / (XM_PI * h6));
							//cb.xSPH_visc_constant = (45.0f / (XM_PI * h6));
							//cb.xSPH_K = SPH_K;
							//cb.xSPH_p0 = SPH_p0;
							//cb.xSPH_e = SPH_e;

							//device->UpdateBuffer(&constantBuffer, &cb, cmd);
							//{
							//	GPUBarrier barriers[] = {
							//		GPUBarrier::Buffer(&constantBuffer, ResourceState::COPY_DST, ResourceState::CONSTANT_BUFFER),
							//	};
							//	device->Barrier(barriers, arraysize(barriers), cmd);
							//}
						}
						D3D11_MAPPED_SUBRESOURCE mappedResEmitter;
						dx11DeviceImmContext->Map(cbuf_Emitter, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResEmitter);
						CB_Emitter* cbGpuEmitter = (CB_Emitter*)mappedResEmitter.pData;
						memcpy(cbGpuEmitter, &cbEmitter, sizeof(CB_Emitter));
						dx11DeviceImmContext->Unmap(cbuf_Emitter, 0);

						dx11DeviceImmContext->CSSetShader(GETCS(PCE_KickoffEmitterSystem_cs_5_0), NULL, 0);
						dx11DeviceImmContext->Dispatch(1, 1, 1);

						constexpr uint ARGUMENTBUFFER_OFFSET_DISPATCHEMIT = 0u;
						dx11DeviceImmContext->CSSetShader(GETCS(PCE_ParticleEmitter_cs_5_0), NULL, 0);
						dx11DeviceImmContext->DispatchIndirect((ID3D11Buffer*)gres_indirect.alloc_res_ptrs[DTYPE_RES], ARGUMENTBUFFER_OFFSET_DISPATCHEMIT);

						constexpr uint ARGUMENTBUFFER_OFFSET_DISPATCHSIMULATION = 12u;
						dx11DeviceImmContext->CSSetShader(GETCS(PCE_ParticleSimulation_cs_5_0), NULL, 0);
						dx11DeviceImmContext->DispatchIndirect((ID3D11Buffer*)gres_indirect.alloc_res_ptrs[DTYPE_RES], ARGUMENTBUFFER_OFFSET_DISPATCHSIMULATION);
					}

					// particle rendering //
					{
						dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 12, dx11UAVs_NULL, NULL);
					
						//ID3D11InputLayout* dx11InputLayer_Target = dx11LI_PNTC;
						ID3D11VertexShader* dx11VS_Target = dx11VShader_IDX;
						ID3D11GeometryShader* dx11GS_Target = NULL;
						ID3D11PixelShader* dx11PS_Target = GETPS(PCE_ParticleRenderBasic_ps_5_0);
						ID3D11RasterizerState2* dx11RState_TargetObj = GETRASTER(SOLID_NONE); // blender state...
						//ID3D11BlendState 
						uint stride_inputlayer = 0u, offset = 0u;
						D3D_PRIMITIVE_TOPOLOGY pobj_topology_type = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;// D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
						
						dx11DeviceImmContext->IASetVertexBuffers(0, 1, NULL, &stride_inputlayer, &offset);
						dx11DeviceImmContext->IASetIndexBuffer(
							(ID3D11Buffer*)gres_vb_idx.alloc_res_ptrs[DTYPE_RES], 
							DXGI_FORMAT_R32_UINT, 0);
						dx11DeviceImmContext->IASetInputLayout(NULL); // using idx buffer and access SRV of VB

						dx11DeviceImmContext->VSSetShader(dx11VS_Target, NULL, 0);
						dx11DeviceImmContext->GSSetShader(dx11GS_Target, NULL, 0);
						dx11DeviceImmContext->PSSetShader(dx11PS_Target, NULL, 0);
						dx11DeviceImmContext->RSSetState(dx11RState_TargetObj);
						//dx11DeviceImmContext->
						dx11DeviceImmContext->IASetPrimitiveTopology(pobj_topology_type);


						ID3D11RenderTargetView* dx11RTVs[2] = {
							(ID3D11RenderTargetView*)gres_fb_singlelayer_rgba.alloc_res_ptrs[DTYPE_RTV],
							(ID3D11RenderTargetView*)gres_fb_singlelayer_depthcs.alloc_res_ptrs[DTYPE_RTV] };

						dx11DeviceImmContext->PSSetShaderResources(20, 1, (ID3D11ShaderResourceView**)&gres_fb_k_buffer.alloc_res_ptrs[DTYPE_SRV]);
						dx11DeviceImmContext->OMSetDepthStencilState(GETDEPTHSTENTIL(ALWAYS), 0);
						dx11DeviceImmContext->OMSetBlendState(dx11CommonParams->get_blender("ADD"), NULL, 0xffffffff);
						dx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(2, dx11RTVs, dx11DSV, 2, NUM_UAVs_1ST, dx11UAVs_NULL, 0);


						//dx11DeviceImmContext->DrawInstancedIndirect();//
						//if (pobj_topology_type == D3D11_PRIMITIVE_TOPOLOGY_POINTLIST)
						//	dx11DeviceImmContext->Draw(MAX_PARTICLES * 4, 0);
						//else
						//	dx11DeviceImmContext->DrawIndexed(MAX_PARTICLES * 6, 0, 0);

						dx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(2, dx11RTVsNULL, dx11DSVNULL, 2, NUM_UAVs_1ST, dx11UAVs_NULL, 0);
						dx11DeviceImmContext->PSSetShaderResources(20, 1, dx11SRVs_NULL);
					}
				}
			}

			// Set NULL States //
			dx11DeviceImmContext->CSSetUnorderedAccessViews(1, 3, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
		}
#endif

		if (general_oit_routine_objs.size() > 0) {
#ifdef DX10_0
			assert(0);
#else
			// pre-pass : Counting fragments
			if (mode_OIT == MFR_MODE::DYNAMIC_FB || mode_OIT == MFR_MODE::DYNAMIC_KB) {

				dx11CommonParams->GpuProfile("Fragment Count");
				RenderStage1(general_oit_routine_objs, mode_OIT, RENDER_GEOPASS::PASS_OIT
					, true /*is_frag_counter_buffer*/, is_ghost_mode, PICKING_TYPE::NONE, apply_fragmerge, false, false, false);
				dx11CommonParams->GpuProfile("Fragment Count", true);

				dx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(2, dx11RTVsNULL, dx11DSVNULL, 2, NUM_UAVs_1ST, dx11UAVs_NULL, 0);

				if (mode_OIT == MFR_MODE::DYNAMIC_FB) {
					dx11CommonParams->GpuProfile("Offset Table Generation");
#pragma region table offset
					ID3D11UnorderedAccessView* dx11UAVs_2nd_pass[NUM_UAVs_2ND] = {
							  (ID3D11UnorderedAccessView*)gres_fb_k_buffer.alloc_res_ptrs[DTYPE_UAV]
							, (ID3D11UnorderedAccessView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_UAV]
							, (ID3D11UnorderedAccessView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_UAV]
							, (ID3D11UnorderedAccessView*)gres_fb_ref_pidx.alloc_res_ptrs[DTYPE_UAV]
							, NULL // (ID3D11UnorderedAccessView*)gres_fb_ubk_buffer.alloc_res_ptrs[DTYPE_UAV]
					};
					UINT UAVInitialCounts = 0;
					dx11DeviceImmContext->CSSetShader(GETCS(SR_OIT_ABUFFER_OffsetTable_cs_5_0), NULL, 0);

					// for detailed description, refer to 
					// "Memory-Efficient Order-Independent Transparency with Dynamic Fragment Buffer"
					// here, we use more efficient implementation for constructing the index table by using 'atomic add' operation.
					dx11DeviceImmContext->CSSetUnorderedAccessViews(1, NUM_UAVs_2ND, dx11UAVs_2nd_pass, (UINT*)(&dx11UAVs_2nd_pass));
					dx11DeviceImmContext->CSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&gres_fb_counter.alloc_res_ptrs[DTYPE_SRV]);
					//dx11DeviceImmContext->Flush();

					dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);
					//dx11DeviceImmContext->Dispatch(fb_size_cur.x, fb_size_cur.y, 1);

					dx11DeviceImmContext->CSSetUnorderedAccessViews(1, NUM_UAVs_2ND, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
					dx11DeviceImmContext->CSSetShaderResources(0, 2, dx11SRVs_NULL);
#pragma endregion table offset
					dx11CommonParams->GpuProfile("Offset Table Generation", true);
				}
				else if (mode_OIT == MFR_MODE::DYNAMIC_KB) {
					dx11CommonParams->GpuProfile("Offset Table Generation & K-Determination (Histogram-based)");
#pragma region table offset & K-determination
					// histogram //
					const int bins_frag_histo = 1024;
					GpuRes histo_buf, histo_buf_sys;
					histo_buf.res_name = "HISTO_FRAGS";
					histo_buf.rtype = RTYPE_BUFFER;
					histo_buf.options["USAGE"] = D3D11_USAGE_DEFAULT;
					histo_buf.options["CPU_ACCESS_FLAG"] = NULL;
					histo_buf.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
					histo_buf.options["FORMAT"] = DXGI_FORMAT_R32_UINT;
					histo_buf.res_values.SetParam("NUM_ELEMENTS", (uint)bins_frag_histo);
					histo_buf.res_values.SetParam("STRIDE_BYTES", (uint)sizeof(uint));
					if (!gpu_manager->UpdateGpuResource(histo_buf))
						gpu_manager->GenerateGpuResource(histo_buf);

					dx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)histo_buf.alloc_res_ptrs[DTYPE_UAV], clr_unit4);

					dx11DeviceImmContext->CSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&gres_fb_counter.alloc_res_ptrs[DTYPE_SRV]);
					dx11DeviceImmContext->CSSetUnorderedAccessViews(10, 1, (ID3D11UnorderedAccessView**)&histo_buf.alloc_res_ptrs[DTYPE_UAV], 0);
					dx11DeviceImmContext->CSSetUnorderedAccessViews(11, 1, (ID3D11UnorderedAccessView**)&gres_fb_ref_pidx.alloc_res_ptrs[DTYPE_UAV], 0);

					dx11DeviceImmContext->CSSetShader(GETCS(SR_FillHistogram_cs_5_0), NULL, 0);
					dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);
					//dx11DeviceImmContext->Dispatch(fb_size_cur.x, fb_size_cur.y, 1);

					// read-back //
					histo_buf_sys = histo_buf;
					histo_buf_sys.res_name = "SYS_OUT_HISTO_FRAGS";
					histo_buf_sys.options["USAGE"] = D3D11_USAGE_STAGING;
					histo_buf_sys.options["BIND_FLAG"] = NULL;
					histo_buf_sys.options["CPU_ACCESS_FLAG"] = D3D11_CPU_ACCESS_READ;
					if (!gpu_manager->UpdateGpuResource(histo_buf_sys))
						gpu_manager->GenerateGpuResource(histo_buf_sys);

					dx11DeviceImmContext->CopyResource((ID3D11Buffer*)histo_buf_sys.alloc_res_ptrs[DTYPE_RES], (ID3D11Buffer*)histo_buf.alloc_res_ptrs[DTYPE_RES]);

					D3D11_MAPPED_SUBRESOURCE mappedResSysHisto;
					HRESULT hr = dx11DeviceImmContext->Map((ID3D11Buffer*)histo_buf_sys.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, NULL, &mappedResSysHisto);
					uint* histogram_frags = (uint*)mappedResSysHisto.pData;
					uint dyn_k_value = bins_frag_histo;
					uint totol_num_frags = 0;
					uint max_layers = 0;
					for (uint i = 0; i < bins_frag_histo; i++)
					{
						uint h_v = histogram_frags[i];
						if (h_v > 0) max_layers = i;
						totol_num_frags += h_v * (i + 1);
					}

					const double Rh = _fncontainer->fnParams.GetParam("_float_RobustRatio", 0.75);
					const bool is_ztmodel = _fncontainer->fnParams.GetParam("_bool_ApplyZTModel", false);
					const uint bytes_per_f = is_ztmodel ? 4 * 4 : 4 * 2; // here, we assign this w.r.t. our z-thickness model (rgba, depth, thickness, opacity_sum)
					// here, we assign this w.r.t. our z-thickness model (rgba, depth, thickness, opacity_sum), i.e., 16 bytes per fragment
					const double size_k_buf_mb = (double)(16 * k_value * buffer_ex) * ((double)fb_size_cur.x * (double)fb_size_cur.y / (1024.0 * 1024.0));

					double min_diff = 1.0;
					uint miss_frags = 0;
					for (; dyn_k_value >= 8; dyn_k_value--)
					{
						miss_frags = 0;
						for (uint i = bins_frag_histo - 1; i >= dyn_k_value; i--)
						{
							miss_frags += (i + 1 - dyn_k_value) * histogram_frags[i];
						}

						double diff = (1. - (double)miss_frags / (double)totol_num_frags) - Rh;
						if ((double)bytes_per_f * (double)(totol_num_frags - miss_frags) / (1024.0 * 1024.0) < size_k_buf_mb)
						{
							if (min_diff * min_diff >= diff * diff)
							{
								min_diff = diff;
								k_value = dyn_k_value;
							}
							else
								break;
						}
					}

					dx11DeviceImmContext->Unmap((ID3D11Buffer*)histo_buf_sys.alloc_res_ptrs[DTYPE_RES], 0);

					if (dx11CommonParams->gpu_profile)
					{
						cout << "----> total frag    : " << totol_num_frags << endl;
						cout << "----> miss frag     : " << miss_frags << endl;
						cout << "----> max layers    : " << max_layers << endl;
						cout << "----> Processing Rh : " << Rh - min_diff << " (" << Rh << ")" << endl;
						cout << "----> dynamic k     : " << k_value << endl;
					}

					if (test_fps_profiling)
					{
						//...totol_num_frags
						// k_value
						ofstream file_totalfrags;
						file_totalfrags.open(".\\data\\frames_profile_totalfrags.txt", std::ios_base::app);
						file_totalfrags << totol_num_frags << endl;
						file_totalfrags.close();
						ofstream file_missfrags;
						file_missfrags.open(".\\data\\frames_profile_missfrags.txt", std::ios_base::app);
						file_missfrags << miss_frags << endl;
						file_missfrags.close();
						ofstream file_k;
						file_k.open(".\\data\\frames_profile_k.txt", std::ios_base::app);
						file_k << k_value << endl;
						file_k.close();
					}

					const int test_K = _fncontainer->fnParams.GetParam("_int_TestKvalue", -1);
					if (test_K >= 8) k_value = test_K;
					cbCamState.k_value = k_value;

					D3D11_MAPPED_SUBRESOURCE mappedResCamState;
					dx11DeviceImmContext->Map(cbuf_cam_state, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResCamState);
					CB_CameraState* cbCamStateData = (CB_CameraState*)mappedResCamState.pData;
					memcpy(cbCamStateData, &cbCamState, sizeof(CB_CameraState));
					dx11DeviceImmContext->Unmap(cbuf_cam_state, 0);
					dx11DeviceImmContext->PSSetConstantBuffers(0, 1, &cbuf_cam_state);
					dx11DeviceImmContext->CSSetConstantBuffers(0, 1, &cbuf_cam_state);

					dx11DeviceImmContext->CSSetShader(GETCS(SR_CreateOffsetTableKpB_cs_5_0), NULL, 0);
					dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);
					//dx11DeviceImmContext->Dispatch(fb_size_cur.x, fb_size_cur.y, 1);

					dx11DeviceImmContext->CSSetUnorderedAccessViews(10, 5, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
					dx11DeviceImmContext->CSSetShaderResources(0, 2, dx11SRVs_NULL);
#pragma endregion table offset & K-determination
					dx11CommonParams->GpuProfile("Offset Table Generation & K-Determination (Histogram-based)", true);
				}
				// used for the increment of the fragments (index)
				dx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gres_fb_counter.alloc_res_ptrs[DTYPE_UAV], clr_unit4);
			}

			dx11CommonParams->GpuProfile("Geometry for OIT");
			// buffer filling
			RenderStage1(general_oit_routine_objs, mode_OIT, RENDER_GEOPASS::PASS_OIT
				, false /*is_frag_counter_buffer*/, is_ghost_mode, PICKING_TYPE::NONE, apply_fragmerge, false, false, false);
			dx11CommonParams->GpuProfile("Geometry for OIT", true);
#endif
		}


#ifndef DX10_0
		auto ResolvePass = [&dx11CommonParams, &dx11DeviceImmContext, &dx11RTVsNULL, &dx11DSVNULL, &dx11UAVs_NULL, &dx11SRVs_NULL,
			&gres_fb_k_buffer, &gres_fb_counter, &gres_fb_rgba, &gres_fb_depthcs, &gres_fb_ubk_buffer, &gres_fb_ref_pidx,
			&gres_fb_singlelayer_rgba, &gres_fb_singlelayer_depthcs,
			&num_grid_x, &num_grid_y]
			(const MFR_MODE mode_OIT, const bool apply_fragmerge) {
			assert(mode_OIT != MFR_MODE::MOMENT);

			dx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(2, dx11RTVsNULL, dx11DSVNULL, 2, NUM_UAVs_1ST, dx11UAVs_NULL, 0);

			UINT UAVInitialCounts = 0;

			ID3D11UnorderedAccessView* dx11UAVs_2nd_pass[NUM_UAVs_2ND] = {
					(ID3D11UnorderedAccessView*)gres_fb_k_buffer.alloc_res_ptrs[DTYPE_UAV]
					, (ID3D11UnorderedAccessView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_UAV]
					, (ID3D11UnorderedAccessView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_UAV]
					, NULL//mode_OIT == MFR_MODE::DXAB ? (ID3D11UnorderedAccessView*)gres_fb_ref_pidx.alloc_res_ptrs[DTYPE_UAV] : NULL
					, mode_OIT == MFR_MODE::DYNAMIC_FB ? (ID3D11UnorderedAccessView*)gres_fb_ubk_buffer.alloc_res_ptrs[DTYPE_UAV] : NULL
			};

			dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, (ID3D11UnorderedAccessView**)&gres_fb_counter.alloc_res_ptrs[DTYPE_UAV], 0); // trimming may occur 

			dx11DeviceImmContext->CSSetShaderResources(1, 1, (ID3D11ShaderResourceView**)&gres_fb_singlelayer_rgba.alloc_res_ptrs[DTYPE_SRV]);
			dx11DeviceImmContext->CSSetShaderResources(2, 1, (ID3D11ShaderResourceView**)&gres_fb_singlelayer_depthcs.alloc_res_ptrs[DTYPE_SRV]);

			switch (mode_OIT)
			{
			case MFR_MODE::STATIC_KB:
				dx11DeviceImmContext->CSSetShader(apply_fragmerge ? GETCS(OIT_SKBZ_RESOLVE_cs_5_0) : GETCS(OIT_SKB_RESOLVE_cs_5_0), NULL, 0);
				break;
			case MFR_MODE::DYNAMIC_FB:
				// sort and render the fragments.  Use the prefix sum to determine where the 
				// fragments for each pixel reside.
				dx11DeviceImmContext->CSSetShader(apply_fragmerge ? GETCS(SR_OIT_ABUFFER_SORT2SENDER_SFM_cs_5_0) : GETCS(SR_OIT_ABUFFER_SORT2SENDER_cs_5_0), NULL, 0);
				//dx11DeviceImmContext->CSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&gres_fb_counter.alloc_res_ptrs[DTYPE_SRV]);
				dx11DeviceImmContext->CSSetShaderResources(50, 1, (ID3D11ShaderResourceView**)&gres_fb_ref_pidx.alloc_res_ptrs[DTYPE_SRV]);
				break;
			case MFR_MODE::DYNAMIC_KB:
				dx11DeviceImmContext->CSSetShader(apply_fragmerge ? GETCS(OIT_DKBZ_RESOLVE_cs_5_0) : GETCS(OIT_DKB_RESOLVE_cs_5_0), NULL, 0);
				dx11DeviceImmContext->CSSetShaderResources(50, 1, (ID3D11ShaderResourceView**)&gres_fb_ref_pidx.alloc_res_ptrs[DTYPE_SRV]);
				break;
			default:
				assert(0);
			}

			dx11DeviceImmContext->CSSetUnorderedAccessViews(1, NUM_UAVs_2ND, dx11UAVs_2nd_pass, (UINT*)(&dx11UAVs_2nd_pass));

			dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);

			// Set NULL States //
			dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL)); // counter
			dx11DeviceImmContext->CSSetUnorderedAccessViews(1, NUM_UAVs_2ND, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
			dx11DeviceImmContext->CSSetShaderResources(0, 2, dx11SRVs_NULL);
		};

		bool additionalKLayerForMFB = foremost_opaque_surfaces_routine_objs.size() > 0 
			|| single_layer_routine_objs.size() > 0 
			|| particle_layer_objs.size() > 0;

		cbCamState.cam_flag |= ((int)additionalKLayerForMFB << 8);
		int storeKBuf = (int)(!is_final_renderer || check_pixel_transmittance
			|| cbEnvState.r_kernel_ao > 0
			|| (cbEnvState.dof_lens_r > 0 && cam_obj->IsPerspective()));
		// which means the k-buffer will be used for the following renderer
		// stores the fragments into the k-buffer and do not store the rendering result into RT
		cbCamState.cam_flag |= (storeKBuf << 3);
		SetCamConstBuf(cbCamState);

		if (general_oit_routine_objs.size() + single_layer_routine_objs.size() > 0 || particle_layer_objs.size() > 0
			|| storeKBuf) 
		{
			dx11CommonParams->GpuProfile("Resolve Pass");
			// ResolvePass calls 
			// 1. SR_OIT_ABUFFER_SORT2SENDER_SFM_cs_5_0 (to FB to SKB)
			// 2. OIT_SKBZ_RESOLVE_cs_5_0 (unordered SKB to ordered SKB)
			// 3. OIT_DKBZ_RESOLVE_cs_5_0 // deprecated..
			// remember RENDER_GEOPASS::PASS_OPAQUESURFACES and RENDER_GEOPASS::PASS_SILHOUETTE stores RTs
			// RenderTargetsToSKB the stored RTs to SKB (unordered)
			// Note that if K-buffer is preserved, then no RT-out is available
			// Dynamic FB layers into K-buffer if there is post-processing (SS algorithms) or single layer pass
			ResolvePass(mode_OIT, apply_fragmerge); // ==> SK-buffer ????
			dx11CommonParams->GpuProfile("Resolve Pass", true);
		}

		if (is_final_renderer) {
			// Note that these post-processing will be performed in volume rendering stage when there is volume rendering requirement
			if (cbEnvState.r_kernel_ao > 0) {
				dx11CommonParams->GpuProfile("SSAO");
				dx11DeviceImmContext->CSSetUnorderedAccessViews(0, NUM_UAVs_2ND, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
				ComputeSSAO(dx11DeviceImmContext, dx11CommonParams, iobj, num_grid_x, num_grid_y,
					gres_fb_counter, gres_fb_k_buffer, gres_fb_rgba, blur_SSAO,
					gres_fb_depthcs, gres_fb_ao_vr_tex, gres_fb_ao_vr_blf_tex, false, apply_fragmerge);
				dx11CommonParams->GpuProfile("SSAO", true);
			}
			if ((cbEnvState.dof_lens_r > 0 && cam_obj->IsPerspective())) {
				dx11CommonParams->GpuProfile("SSDOF");
				dx11DeviceImmContext->CSSetUnorderedAccessViews(0, NUM_UAVs_2ND, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
				ComputeDOF(dx11DeviceImmContext, dx11CommonParams, iobj, num_grid_x, num_grid_y,
					gres_fb_counter, gres_fb_k_buffer, gres_fb_rgba, cbEnvState.r_kernel_ao > 0, blur_SSAO, apply_fragmerge,
					gres_fb_depthcs, gres_fb_ao_vr_tex, gres_fb_ao_vr_blf_tex,
					cbCamState, cbuf_cam_state, __BLOCKSIZE, false);
				dx11CommonParams->GpuProfile("SSDOF", true);
			}
		}

		if (second_layer_objs.size() > 0) {
			dx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gres_fb_singlelayer_rgba.alloc_res_ptrs[DTYPE_RTV], clr_float_zero_4);
			dx11DeviceImmContext->ClearDepthStencilView(dx11DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
			dx11CommonParams->GpuProfile("Second Layer Surface");
			RenderStage1(second_layer_objs, MFR_MODE::NONE, RENDER_GEOPASS::PASS_OPAQUESURFACES
				, false /*is_frag_counter_buffer*/, false/*is_ghost_mode*/, PICKING_TYPE::NONE, false/*apply_fragmerge*/, false, false, true);
			dx11CommonParams->GpuProfile("Second Layer Surface", true);
			iobj->SetObjParam("NUM_SECOND_LAYER_OBJECTS", (int)second_layer_objs.size());
		}
#endif

		// Set NULL States //
		dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL)); // counter
		dx11DeviceImmContext->CSSetUnorderedAccessViews(1, NUM_UAVs_2ND, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
		dx11DeviceImmContext->CSSetShaderResources(0, 2, dx11SRVs_NULL);
	}
	else if (mode_OIT == MFR_MODE::MOMENT) {
#ifdef DX10_0
		assert(0);
#else
		SetCamConstBuf(cbCamState);

		RenderStage1(general_oit_routine_objs, MFR_MODE::MOMENT, RENDER_GEOPASS::PASS_OIT // moment generation
			, false /*is_frag_counter_buffer*/, is_ghost_mode, PICKING_TYPE::NONE, apply_fragmerge, true /*is_MOMENT_gen_buffer*/, false, false);

		dx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(2, dx11RTVsNULL, dx11DSVNULL, 2, NUM_UAVs_1ST, dx11UAVs_NULL, 0);
		dx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gres_fb_counter.alloc_res_ptrs[DTYPE_UAV], clr_unit4);

		RenderStage1(general_oit_routine_objs, MFR_MODE::MOMENT, RENDER_GEOPASS::PASS_OIT // moment generation
			, false /*is_frag_counter_buffer*/, is_ghost_mode, PICKING_TYPE::NONE, apply_fragmerge, false /*is_MOMENT_gen_buffer*/, false, false);
#endif
	}

	iobj->SetObjParam("_int_NumCallRenders", count_call_render);
	dx11CommonParams->GpuProfile("SR Render", true);

	// DF.. K 버퍼 저장 
	// * RENDER 1/2 갈 때, 항상.. 
	// * K 버퍼로 저장, 
	// * color 저장 옵션...
	// * general 만 있을 때, 그냥 종료				\

#ifndef DX10_0
	if (check_pixel_transmittance)
	{
		dx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gres_fb_sys_counter.alloc_res_ptrs[DTYPE_RES],
			(ID3D11Texture2D*)gres_fb_counter.alloc_res_ptrs[DTYPE_RES]);

		D3D11_MAPPED_SUBRESOURCE mappedResSysCounter;
		HRESULT hr = dx11DeviceImmContext->Map((ID3D11Texture2D*)gres_fb_sys_counter.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, NULL, &mappedResSysCounter);
		int buf_row_pitch = mappedResSysCounter.RowPitch / 4;
		uint* cnt_buf = (uint*)mappedResSysCounter.pData;
		uint max_layers = 0;
		double avr_layers_sum = 0;
		uint pix_cnt = 0;
		for (int i = 0; i < fb_size_cur.y; i++)
			for (int j = 0; j < fb_size_cur.x; j++)
			{
				uint cnt = cnt_buf[j + i * buf_row_pitch];
				max_layers = max(max_layers, cnt);
				avr_layers_sum += cnt;
				pix_cnt += cnt > 0 ? 1 : 0;
			}
		cout << "--------------" << endl;
		cout << "TOTAL LAYERS : " << avr_layers_sum << ", MAX LAYERS : " << max_layers << endl;
		cout << "AVR LAYERS : " << avr_layers_sum / pix_cnt/*(fb_size_cur.x*fb_size_cur.y)*/ << endl;
		cout << "--------------" << endl;

		dx11DeviceImmContext->Unmap((ID3D11Texture2D*)gres_fb_sys_counter.alloc_res_ptrs[DTYPE_RES], 0);


		vmint2 pixel_pos = _fncontainer->fnParams.GetParam("_int2_PixelPos", vmint2(0));
		float tr_interval = _fncontainer->fnParams.GetParam("_float_TrInvterval", 0.01f);
		float tr_startoffset = _fncontainer->fnParams.GetParam("_float_TrStartOffset", 1.00f);

		cout << "-------------" << endl;
		cout << "interval : " << tr_interval << endl;

		dx11DeviceImmContext->CopyResource((ID3D11Buffer*)gres_fb_sys_deep_k.alloc_res_ptrs[DTYPE_RES],
			(ID3D11Buffer*)gres_fb_k_buffer.alloc_res_ptrs[DTYPE_RES]);

		string __METHODS[3] = { "SKB" , "DFB" , "MOMENT" };// , "DKBTZ", "DKBT"};
		if (mode_OIT == MFR_MODE::MOMENT)
		{
			string refdepthrangePath = ".\\data\\absorbance_depth_ref_range.txt";
			ifstream openFile(refdepthrangePath.data());
			if (openFile.is_open())
			{
				// see Section 3.3 Warping Depth in Mement-based OIT
				auto __warp_depth = [&mot_nf](float z_depth_cs) -> float
				{
					const float z_min = (float)mot_nf.x;
					const float z_max = (float)mot_nf.y;
					return (log(max(min(z_depth_cs, z_max), z_min)) - log(z_min)) / (log(z_max) - log(z_min)) * 2.f - 1.f;
				};

				cout << "warp min/max : " << mot_nf.x << ", " << mot_nf.y << endl;

				vector<float> depth_range_info;
				string line;
				while (getline(openFile, line)) {
					depth_range_info.push_back(stof(line));
				}
				openFile.close();

				string depthPath = ".\\data\\absorbance_depth_" + __METHODS[(int)mode_OIT] + ".txt";
				string alphaPath = ".\\data\\absorbance_alpha_" + __METHODS[(int)mode_OIT] + ".txt";

				ofstream writeFile_depth(depthPath.data());
				ofstream writeFile_absorb(alphaPath.data());

				float d_s = depth_range_info[0];
				float depth_range = depth_range_info[1];
				int num_intervals = (int)(depth_range / tr_interval) + 2;

				D3D11_MAPPED_SUBRESOURCE mappedResSysDeepK;
				hr |= dx11DeviceImmContext->Map((ID3D11Buffer*)gres_fb_sys_deep_k.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, NULL, &mappedResSysDeepK);
				uint* mc_buffer = (uint*)mappedResSysDeepK.pData;
				for (int i = 0; i < num_intervals; i++)
				{
					float d_cur = d_s + (float)i * tr_interval;
					float warp_d = __warp_depth(d_cur);
					float transmittance_at_depth, total_transmittance;

					moment_lib::resolveMoments(transmittance_at_depth, total_transmittance, warp_d, pixel_pos, fb_size_cur.x, 8 * 4, mc_buffer, num_moments, false, cb_moment);
					writeFile_absorb << to_string(1.f - transmittance_at_depth) + "\n";
					writeFile_depth << to_string(d_cur) + "\n";
				}
				dx11DeviceImmContext->Unmap((ID3D11Buffer*)gres_fb_sys_deep_k.alloc_res_ptrs[DTYPE_RES], 0);

				writeFile_depth.close();
				writeFile_absorb.close();
			}
			else
			{
				cout << "NO REF DEPTH!" << endl;
			}
		}
		else
		{
			uint pixel_offset = (pixel_pos.x + pixel_pos.y * fb_size_cur.x) * 8;
			if (mode_OIT != MFR_MODE::STATIC_KB)
			{
				dx11DeviceImmContext->CopyResource((ID3D11Buffer*)gres_fb_sys_ref_pidx.alloc_res_ptrs[DTYPE_RES],
					(ID3D11Buffer*)gres_fb_ref_pidx.alloc_res_ptrs[DTYPE_RES]);

				D3D11_MAPPED_SUBRESOURCE mappedResSysRefPIdx;
				hr |= dx11DeviceImmContext->Map((ID3D11Buffer*)gres_fb_sys_ref_pidx.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, NULL, &mappedResSysRefPIdx);
				pixel_offset = ((uint*)mappedResSysRefPIdx.pData)[pixel_pos.x + pixel_pos.y * fb_size_cur.x];
				dx11DeviceImmContext->Unmap((ID3D11Buffer*)gres_fb_sys_ref_pidx.alloc_res_ptrs[DTYPE_RES], 0);
			}

			D3D11_MAPPED_SUBRESOURCE mappedResSysDeepK, mappedResSysCounter;
			hr |= dx11DeviceImmContext->Map((ID3D11Buffer*)gres_fb_sys_deep_k.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, NULL, &mappedResSysDeepK);
			hr |= dx11DeviceImmContext->Map((ID3D11Texture2D*)gres_fb_sys_counter.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, NULL, &mappedResSysCounter);

			uint* deep_kbuffer = (uint*)mappedResSysDeepK.pData;
			uint num_layers = ((uint*)mappedResSysCounter.pData)[pixel_pos.x + pixel_pos.y * buf_row_pitch];
			cout << "num_layers : " << num_layers << " at (" << pixel_pos.x << ", " << pixel_pos.y << ")" << endl;

			if (num_layers > 0)
			{
				struct Fragment
				{
					float alpha;
					float z;
					float thick;
				};

				uint element_size = apply_fragmerge ? 4 : 2;
				Fragment* fs = new Fragment[num_layers];

				for (uint i = 0; i < num_layers; i++)
				{
					fs[i].z = *(float*)&deep_kbuffer[element_size * (pixel_offset + i) + 1];
					fs[i].alpha = (deep_kbuffer[element_size * (pixel_offset + i) + 0] >> 24) / 255.f;
					if (mode_OIT == MFR_MODE::DYNAMIC_KB || mode_OIT == MFR_MODE::STATIC_KB)
						fs[i].thick = *(float*)&deep_kbuffer[element_size * (pixel_offset + i) + 2];
				}
				dx11DeviceImmContext->Unmap((ID3D11Buffer*)gres_fb_sys_deep_k.alloc_res_ptrs[DTYPE_RES], 0);
				dx11DeviceImmContext->Unmap((ID3D11Texture2D*)gres_fb_sys_counter.alloc_res_ptrs[DTYPE_RES], 0);

				__SORT(num_layers, fs, Fragment);
				/**/

				string refdepthrangePath = ".\\data\\absorbance_depth_ref_range.txt";
				if (mode_OIT == MFR_MODE::DYNAMIC_FB)
				{
					ofstream refwriteFile_depth_range(refdepthrangePath.data());
					if (refwriteFile_depth_range.is_open())
					{
						refwriteFile_depth_range << to_string(fs[0].z - tr_startoffset) + "\n";
						float d_range = fs[num_layers - 1].z - fs[0].z;
						refwriteFile_depth_range << to_string(d_range + tr_startoffset * 2) + "\n";
						refwriteFile_depth_range.close();
					}
				}

				ifstream openFile(refdepthrangePath.data());
				if (openFile.is_open())
				{
					vector<float> depth_range_info;
					string line;
					while (getline(openFile, line)) {
						depth_range_info.push_back(stof(line));
					}
					openFile.close();

					string depthPath = ".\\data\\absorbance_depth_" + __METHODS[(int)mode_OIT] + ".txt";
					string alphaPath = ".\\data\\absorbance_alpha_" + __METHODS[(int)mode_OIT] + ".txt";

					ofstream writeFile_depth(depthPath.data());
					ofstream writeFile_absorb(alphaPath.data());

					assert(writeFile_depth.is_open() && writeFile_absorb.is_open());

					float d_s = depth_range_info[0];
					float depth_range = depth_range_info[1];
					int num_intervals = (int)(depth_range / tr_interval) + 2;

					uint hit_count = 0;
					float absorb = 0;
					if (mode_OIT == MFR_MODE::DYNAMIC_FB && !apply_fragmerge)
					{
						for (int i = 0; i < num_intervals; i++)
						{
							float d_cur = d_s + (float)i * tr_interval;

							if (hit_count < num_layers)
							{
								while (d_cur > fs[hit_count].z)
								{
									absorb += (1 - absorb) * fs[hit_count].alpha;
									hit_count++;
									if (hit_count >= num_layers) break;
								}
							}
							writeFile_absorb << to_string(absorb) + "\n";
							writeFile_depth << to_string(d_cur) + "\n";
						}
					}
					else if ((mode_OIT == MFR_MODE::DYNAMIC_KB && apply_fragmerge) || mode_OIT == MFR_MODE::STATIC_KB)
					{
						for (int i = 0; i < num_intervals; i++)
						{
							float d_cur = d_s + (float)i * tr_interval;
							if (hit_count < num_layers)
							{
								while (d_cur > fs[hit_count].z - fs[hit_count].thick)
								{
									if (d_cur >= fs[hit_count].z)
									{
										absorb += (1 - absorb) * fs[hit_count].alpha;
										hit_count++;
										if (hit_count >= num_layers) break;
									}
									else
									{
										float sub_thick_f = d_cur - (fs[hit_count].z - fs[hit_count].thick);
										float sub_alpha_f = fs[hit_count].alpha * sub_thick_f / fs[hit_count].thick;
										absorb += (1 - absorb) * sub_alpha_f;

										fs[hit_count].thick = fs[hit_count].thick - sub_thick_f;
										fs[hit_count].alpha = (fs[hit_count].alpha - sub_alpha_f) / (1.f - sub_alpha_f);
									}
								}
							}
							writeFile_absorb << to_string(absorb) + "\n";
							writeFile_depth << to_string(d_cur) + "\n";
						}
					}

					writeFile_depth.close();
					writeFile_absorb.close();
				}
				else
				{
					cout << "NO REF DEPTH!" << endl;
				}
				delete[] fs;
			}
		}
	}
#endif
	dx11DeviceImmContext->ClearState();

	dx11DeviceImmContext->OMSetRenderTargets(1, &pdxRTVOld, pdxDSVOld);
	VMSAFE_RELEASE(pdxRTVOld);
	VMSAFE_RELEASE(pdxDSVOld);

	iobj->SetDescriptor("vismtv_inbuilt_renderergpudx module");

	iobj->SetObjParam("_ullong_LatestSrTime", vmhelpers::GetCurrentTimePack());
	//((std::mutex*)HDx11GetMutexGpuCriticalPath())->unlock();

	return true;
}