#include "RendererHeader.h"

#include <fstream>
#include <iostream>
#include <cmath>
//#include <opencv2/imgproc.hpp>
//#include <opencv2/highgui.hpp>

using namespace grd_helper;

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
		return max(min(1.f, v), 0);
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
		float Discriminant = dot(vmfloat2(4.0f*Delta.x, -Delta.y), vmfloat2(Delta.z, Delta.y));
		// Compute coefficients of the depressed cubic 
		// (third is zero, fourth is one)
		vmfloat2 Depressed = vmfloat2(
			mad(-2.0f*Coefficient.z, Delta.x, Delta.y),
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
			dot(vmfloat2(-0.5f, -0.5f*sqrt(3.0f)), CubicRoot),
			dot(vmfloat2(-0.5f, 0.5f*sqrt(3.0f)), CubicRoot)
		);
		Root = mad(2.0f*sqrt(-Depressed.y), Root, -Coefficient.z);
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
		float discriminant = 4.0 * delta.x * delta.z - delta.y * delta.y;

		vmfloat2 depressed = vmfloat2(delta.z, -coeffs.x * delta.y + 2.0 * coeffs.y * delta.z);
		float theta = abs(atan2(coeffs.x * sqrt(discriminant), -depressed.y)) / 3.0;
		vmfloat2 sin_cos;
		sin_cos.x = sin(theta);
		sin_cos.y = cos(theta);
		float tmp = 2.0 * sqrt(-depressed.x);
		vmfloat2 x = vmfloat2(tmp * sin_cos.y, tmp * (-0.5 * sin_cos.y - 0.5 * sqrt(3.0) * sin_cos.x));
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
		float P = -2.0*C;
		float Q = C * C + B * D - 4.0*E;
		float R = D * D + B * B*E - B * C*D;

		// Obtain the smallest cubic root
		float y = solveCubicBlinnSmallest(vmfloat4(R, Q, P, 1.0));

		float BB = B * B;
		float fy = 4.0 * y;
		float BB_fy = BB - fy;

		float Z = C - y;
		float ZZ = Z * Z;
		float fE = 4.0 * E;
		float ZZ_fE = ZZ - fE;

		float G, g, H, h;
		// Compute the coefficients of the quadratics adaptively using the two 
		// proposed factorizations by Neumark. Choose the appropriate 
		// factorizations using the heuristic proposed by Herbison-Evans.
		if (y < 0 || (ZZ + fE) * BB_fy > ZZ_fE * (BB + fy)) {
			float tmp = sqrt(BB_fy);
			G = (B + tmp) * 0.5;
			g = (B - tmp) * 0.5;

			tmp = (B*Z - 2.0*D) / (2.0*tmp);
			H = mad(Z, 0.5, tmp);
			h = mad(Z, 0.5, -tmp);
		}
		else {
			float tmp = sqrt(ZZ_fE);
			H = (Z + tmp) * 0.5;
			h = (Z - tmp) * 0.5;

			tmp = (B*Z - 2.0*D) / (2.0*tmp);
			G = mad(B, 0.5, tmp);
			g = mad(B, 0.5, -tmp);
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
		float InvD22 = 1.0 / D22;
		float L32D22 = mad(-b[1], b[0], b[2]);
		float L32 = L32D22 * InvD22;
		float L42D22 = mad(-b[2], b[0], b[3]);
		float L42 = L42D22 * InvD22;
		float L52D22 = mad(-b[3], b[0], b[4]);
		float L52 = L52D22 * InvD22;

		float D33 = mad(-L32, L32D22, mad(-b[1], b[1], b[3]));
		float InvD33 = 1.0 / D33;
		float L43D33 = mad(-L42, L32D22, mad(-b[2], b[1], b[4]));
		float L43 = L43D33 * InvD33;
		float L53D33 = mad(-L52, L32D22, mad(-b[3], b[1], b[5]));
		float L53 = L53D33 * InvD33;

		float D44 = mad(-b[2], b[2], b[5]) - dot(vmfloat2(L42, L43), vmfloat2(L42D22, L43D33));
		float InvD44 = 1.0 / D44;
		float L54D44 = mad(-b[3], b[2], b[6]) - dot(vmfloat2(L52, L53), vmfloat2(L42D22, L43D33));
		float L54 = L54D44 * InvD44;

		float D55 = mad(-b[3], b[3], b[7]) - dot(vmfloat3(L52, L53, L54), vmfloat3(L52D22, L53D33, L54D44));
		float InvD55 = 1.0 / D55;

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
		float D = (p*p*0.25f) - q;
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
		else if(num_moment == 6)
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

			const float bias_vector[6] = { 0, 0.48, 0, 0.451, 0, 0.45 };
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
			const float bias_vector[8] = { 0, 0.75, 0, 0.67666666666666664, 0, 0.63, 0, 0.60030303030303034 };
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

void ComputeSSAO(__ID3D11DeviceContext* dx11DeviceImmContext,
	grd_helper::GpuDX11CommonParameters* dx11CommonParams, int num_grid_x, int num_grid_y,
	GpuRes& gres_fb_counter, GpuRes& gres_fb_deep_k_buffer, GpuRes& gres_fb_rgba, bool blur_SSAO,
	GpuRes(& gres_fb_mip_z_halftexs)[2], GpuRes(& gres_fb_mip_a_halftexs)[2],
	GpuRes(&gres_fb_ao_texs)[2], GpuRes(&gres_fb_ao_blf_texs)[2],
	GpuRes& gres_fb_vr_depth, GpuRes& gres_fb_vr_ao, GpuRes& gres_fb_vr_ao_blf, bool involve_vr)
{
	ID3D11ShaderResourceView* dx11SRVs_SSAO[5] = {};
	dx11SRVs_SSAO[0] = (ID3D11ShaderResourceView*)gres_fb_counter.alloc_res_ptrs[DTYPE_SRV];
	dx11SRVs_SSAO[1] = (ID3D11ShaderResourceView*)gres_fb_deep_k_buffer.alloc_res_ptrs[DTYPE_SRV];
	dx11SRVs_SSAO[2] = dx11SRVs_SSAO[3] = dx11SRVs_SSAO[4] = NULL;
	dx11DeviceImmContext->CSSetShaderResources(10, 2, dx11SRVs_SSAO);

	// KBZ_TO_TEXTURE_cs_5_0
	ID3D11UnorderedAccessView* dx11UAVs_SSAO[5] = {};
	dx11UAVs_SSAO[0] = (ID3D11UnorderedAccessView*)gres_fb_mip_z_halftexs[0].alloc_res_ptrs[DTYPE_UAV];
	dx11UAVs_SSAO[1] = (ID3D11UnorderedAccessView*)gres_fb_mip_z_halftexs[1].alloc_res_ptrs[DTYPE_UAV];
	dx11DeviceImmContext->CSSetUnorderedAccessViews(15, 2, dx11UAVs_SSAO, 0);
	dx11UAVs_SSAO[0] = (ID3D11UnorderedAccessView*)gres_fb_mip_a_halftexs[0].alloc_res_ptrs[DTYPE_UAV];
	dx11UAVs_SSAO[1] = (ID3D11UnorderedAccessView*)gres_fb_mip_a_halftexs[1].alloc_res_ptrs[DTYPE_UAV];
	dx11DeviceImmContext->CSSetUnorderedAccessViews(20, 2, dx11UAVs_SSAO, 0);
	dx11UAVs_SSAO[2] = dx11UAVs_SSAO[3] = dx11UAVs_SSAO[4] = NULL;

	dx11DeviceImmContext->CSSetShader(GETCS(KBZ_TO_TEXTURE_cs_5_0), NULL, 0);
	dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);
	dx11DeviceImmContext->Flush();
	dx11DeviceImmContext->CSSetUnorderedAccessViews(15, 2, &dx11UAVs_SSAO[2], 0);
	dx11DeviceImmContext->CSSetUnorderedAccessViews(20, 2, &dx11UAVs_SSAO[2], 0);
	dx11DeviceImmContext->GenerateMips((ID3D11ShaderResourceView*)gres_fb_mip_z_halftexs[0].alloc_res_ptrs[DTYPE_SRV]);
	dx11DeviceImmContext->GenerateMips((ID3D11ShaderResourceView*)gres_fb_mip_z_halftexs[1].alloc_res_ptrs[DTYPE_SRV]);
	dx11DeviceImmContext->GenerateMips((ID3D11ShaderResourceView*)gres_fb_mip_a_halftexs[0].alloc_res_ptrs[DTYPE_SRV]);
	dx11DeviceImmContext->GenerateMips((ID3D11ShaderResourceView*)gres_fb_mip_a_halftexs[1].alloc_res_ptrs[DTYPE_SRV]);

	// KB_SSAO_cs_5_0
	dx11UAVs_SSAO[0] = (ID3D11UnorderedAccessView*)gres_fb_ao_texs[0].alloc_res_ptrs[DTYPE_UAV];
	dx11UAVs_SSAO[1] = (ID3D11UnorderedAccessView*)gres_fb_ao_texs[1].alloc_res_ptrs[DTYPE_UAV];
	dx11DeviceImmContext->CSSetUnorderedAccessViews(25, 2, dx11UAVs_SSAO, 0);

	dx11DeviceImmContext->CSSetUnorderedAccessViews(10, 1, (ID3D11UnorderedAccessView**)&gres_fb_rgba.alloc_res_ptrs[DTYPE_UAV], 0);

	dx11SRVs_SSAO[0] = (ID3D11ShaderResourceView*)gres_fb_mip_z_halftexs[0].alloc_res_ptrs[DTYPE_SRV];
	dx11SRVs_SSAO[1] = (ID3D11ShaderResourceView*)gres_fb_mip_z_halftexs[1].alloc_res_ptrs[DTYPE_SRV];
	dx11DeviceImmContext->CSSetShaderResources(15, 2, dx11SRVs_SSAO);
	dx11SRVs_SSAO[0] = (ID3D11ShaderResourceView*)gres_fb_mip_a_halftexs[0].alloc_res_ptrs[DTYPE_SRV];
	dx11SRVs_SSAO[1] = (ID3D11ShaderResourceView*)gres_fb_mip_a_halftexs[1].alloc_res_ptrs[DTYPE_SRV];
	dx11DeviceImmContext->CSSetShaderResources(20, 2, dx11SRVs_SSAO);

	if (involve_vr)
	{
		dx11DeviceImmContext->CSSetShaderResources(31, 1, (ID3D11ShaderResourceView**)&gres_fb_vr_depth.alloc_res_ptrs[DTYPE_SRV]);
		dx11DeviceImmContext->CSSetUnorderedAccessViews(30, 1, (ID3D11UnorderedAccessView**)&gres_fb_vr_ao.alloc_res_ptrs[DTYPE_UAV], 0);
	}

	dx11DeviceImmContext->CSSetShader(GETCS(KB_SSAO_cs_5_0), NULL, 0);
	dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);

	if (blur_SSAO)
	{
		// BLUR process
		dx11DeviceImmContext->Flush();
		dx11DeviceImmContext->CSSetUnorderedAccessViews(25, 2, &dx11UAVs_SSAO[2], 0);
		if (involve_vr)
		{
			dx11DeviceImmContext->CSSetUnorderedAccessViews(30, 1, &dx11UAVs_SSAO[2], 0);
			dx11DeviceImmContext->CSSetShaderResources(30, 1, (ID3D11ShaderResourceView**)&gres_fb_vr_ao.alloc_res_ptrs[DTYPE_SRV]);
			dx11DeviceImmContext->CSSetUnorderedAccessViews(30, 1, (ID3D11UnorderedAccessView**)&gres_fb_vr_ao_blf.alloc_res_ptrs[DTYPE_UAV], 0);
		}

		dx11SRVs_SSAO[0] = (ID3D11ShaderResourceView*)gres_fb_ao_texs[0].alloc_res_ptrs[DTYPE_SRV];
		dx11SRVs_SSAO[1] = (ID3D11ShaderResourceView*)gres_fb_ao_texs[1].alloc_res_ptrs[DTYPE_SRV];
		dx11DeviceImmContext->CSSetShaderResources(25, 2, dx11SRVs_SSAO);

		dx11UAVs_SSAO[0] = (ID3D11UnorderedAccessView*)gres_fb_ao_blf_texs[0].alloc_res_ptrs[DTYPE_UAV];
		dx11UAVs_SSAO[1] = (ID3D11UnorderedAccessView*)gres_fb_ao_blf_texs[1].alloc_res_ptrs[DTYPE_UAV];
		dx11DeviceImmContext->CSSetUnorderedAccessViews(25, 2, dx11UAVs_SSAO, 0);

		dx11DeviceImmContext->CSSetShader(GETCS(KB_SSAO_BLUR_cs_5_0), NULL, 0);
		dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);
	}

	dx11DeviceImmContext->CSSetShaderResources(10, 2, &dx11SRVs_SSAO[2]);
	dx11DeviceImmContext->CSSetUnorderedAccessViews(10, 1, &dx11UAVs_SSAO[2], 0);

	dx11DeviceImmContext->CSSetShaderResources(15, 2, &dx11SRVs_SSAO[2]);
	dx11DeviceImmContext->CSSetShaderResources(20, 2, &dx11SRVs_SSAO[2]);
	dx11DeviceImmContext->CSSetShaderResources(25, 2, &dx11SRVs_SSAO[2]);
	dx11DeviceImmContext->CSSetShaderResources(30, 2, &dx11SRVs_SSAO[2]);
	dx11DeviceImmContext->CSSetUnorderedAccessViews(15, 2, &dx11UAVs_SSAO[2], 0);
	dx11DeviceImmContext->CSSetUnorderedAccessViews(20, 2, &dx11UAVs_SSAO[2], 0);
	dx11DeviceImmContext->CSSetUnorderedAccessViews(25, 2, &dx11UAVs_SSAO[2], 0);
	dx11DeviceImmContext->CSSetUnorderedAccessViews(30, 1, &dx11UAVs_SSAO[2], 0);
}

bool RenderSrOIT(VmFnContainer* _fncontainer,
	VmGpuManager* gpu_manager,
	grd_helper::GpuDX11CommonParameters* dx11CommonParams,
	LocalProgress* progress,
	double* run_time_ptr)
{
	LARGE_INTEGER lIntFreq;
	LARGE_INTEGER lIntCntStart, lIntCntEnd;

	vector<VmObject*> input_pobjs;
	_fncontainer->GetVmObjectList(&input_pobjs, VmObjKey(ObjectTypePRIMITIVE, true));

	vector<VmObject*> input_vobjs;
	_fncontainer->GetVmObjectList(&input_vobjs, VmObjKey(ObjectTypeVOLUME, true));
	vector<VmObject*> input_tobjs;
	_fncontainer->GetVmObjectList(&input_tobjs, VmObjKey(ObjectTypeTMAP, true));

	VmLObject* lobj = (VmLObject*)_fncontainer->GetVmObject(VmObjKey(ObjectTypeBUFFER, true), 0);

	map<int, VmObject*> associated_objs;
	for (int i = 0; i < (int)input_vobjs.size(); i++)
		associated_objs.insert(pair<int, VmObject*>(input_vobjs[i]->GetObjectID(), input_vobjs[i]));
	for (int i = 0; i < (int)input_tobjs.size(); i++)
		associated_objs.insert(pair<int, VmObject*>(input_tobjs[i]->GetObjectID(), input_tobjs[i]));

	VmIObject* iobj = (VmIObject*)_fncontainer->GetVmObject(VmObjKey(ObjectTypeIMAGEPLANE, false), 0);

	//((std::mutex*)HDx11GetMutexGpuCriticalPath())->lock();
	int num_deep_layers_old = 8;
	lobj->GetCustomParameter("_int_NumDeepLayers", data_type::dtype<int>(), &num_deep_layers_old);
	int num_deep_layers = _fncontainer->GetParamValue("_int_NumDeepLayers", num_deep_layers_old);
	lobj->RegisterCustomParameter("_int_NumDeepLayers", num_deep_layers);

	int num_moments_old = 4;
	lobj->GetCustomParameter("_int_NumQueueLayers", data_type::dtype<int>(), &num_moments_old);
	int num_moments = _fncontainer->GetParamValue("_int_NumQueueLayers", num_moments_old);
	int num_safe_loopexit = _fncontainer->GetParamValue("_int_SpinLockSafeLoops", (int)100);
	bool apply_antialiasing = _fncontainer->GetParamValue("_bool_IsAntiAliasingRS", false);
	bool is_final_renderer = _fncontainer->GetParamValue("_bool_IsFinalRenderer", true);
	double v_thickness = _fncontainer->GetParamValue("_double_VZThickness", -1.0);
	double v_copthickness = _fncontainer->GetParamValue("_double_CopVZThickness", -1.0);
	double v_discont_depth = _fncontainer->GetParamValue("_double_DiscontDepth", -1.0);
	float merging_beta = (float)_fncontainer->GetParamValue("_double_MergingBeta", 0.5);
	bool blur_SSAO = _fncontainer->GetParamValue("_bool_BlurSSAO", true);
	bool is_a_buffer = _fncontainer->GetParamValue("_bool_IsAbuffer", false);
	int buf_ex_scale = _fncontainer->GetParamValue("_int_ABufEx", (int)3);
	bool show_maxlayers_a_buffer_ = _fncontainer->GetParamValue("_bool_ShowMaxLayersAbuffer", false);
	bool is_moment_oit = is_a_buffer? false : _fncontainer->GetParamValue("_bool_MomentOIT", false);
	bool use_blending_option_MomentOIT = _fncontainer->GetParamValue("_bool_UseBlendingOptionMomentOIT", false);
	bool pixel_transmittance = _fncontainer->GetParamValue("_bool_PixelTransmittance", false);
	vmint2 pixel_pos = _fncontainer->GetParamValue("_int2_PixelPos", vmint2(0));
	double tr_interval = _fncontainer->GetParamValue("_double_TrInvterval", (double)0.01);
	double tr_startoffset = _fncontainer->GetParamValue("_double_TrStartOffset", (double)1.00);
	vmdouble2 mot_nf = _fncontainer->GetParamValue("_double2_MotNearFar", vmdouble2(0.1, 100000.));
	//is_a_buffer = show_maxlayers_a_buffer_ = true;
	//vr_level = 2;
	vmdouble4 global_light_factors = _fncontainer->GetParamValue("_double4_ShadingFactorsForGlobalPrimitives", vmdouble4(0.2, 1.0, 0.5, 5)); // Emission, Diffusion, Specular, Specular Power
	vmdouble4 ui_light_factors = _fncontainer->GetParamValue("_double4_ShadingFactorsForUiPrimitives", vmdouble4(0.4, 0.8, 0.2, 30)); // Emission, Diffusion, Specular, Specular Power

	double default_point_thickness = _fncontainer->GetParamValue("_double_PointThickness", 3.0);
	double default_line_thickness = _fncontainer->GetParamValue("_double_LineThickness", 2.0);
	vmdouble3 default_color_cmmobj = _fncontainer->GetParamValue("_double3_CmmGlobalColor", vmdouble3(-1, -1, -1));

	bool is_ghost_mode = _fncontainer->GetParamValue("_bool_GhostEffect", false);

	bool is_rgba = _fncontainer->GetParamValue("_bool_IsRGBA", false); // false means bgra

	bool is_system_out = false;
	if (is_final_renderer) is_system_out = true;

	bool only_surface_test = _fncontainer->GetParamValue("_bool_OnlySurfaceTest", false);
	bool test_consoleout = _fncontainer->GetParamValue("_bool_TestConsoleOut", false);
	auto test_out = [&test_consoleout](const string& _message)
	{
		if (test_consoleout)
			cout << _message << endl;
	};

#pragma region // Shader Setting
	// Shader Re-Compile Setting //
	bool reload_hlsl_objs = _fncontainer->GetParamValue("_bool_ReloadHLSLObjFiles", false);
	if (reload_hlsl_objs)
	{
		//string prefix_path = "..\\..\\VisNativeModules\\vismtv_inbuilt_renderergpudx\\OIT\\";
		//string prefix_path = "..\\..\\VmProjects\\hybrid_rendering_engine\\shader_compiled_objs\\";
		string prefix_path = "E:\\project_srcs\\VisMotive\\VmProjects\\hybrid_rendering_engine\\shader_compiled_objs\\";
		cout << "RECOMPILE HLSL _ OIT renderer!!" << endl;

		dx11CommonParams->dx11DeviceImmContext->VSSetShader(NULL, NULL, 0);
		dx11CommonParams->dx11DeviceImmContext->GSSetShader(NULL, NULL, 0);
		dx11CommonParams->dx11DeviceImmContext->PSSetShader(NULL, NULL, 0);
		dx11CommonParams->dx11DeviceImmContext->CSSetShader(NULL, NULL, 0);
#define VS_NUM 5
#define PS_NUM 18
#define CS_NUM 8
#define SET_VS(NAME, __S) dx11CommonParams->safe_set_res(grd_helper::COMRES_INDICATOR(VERTEX_SHADER, NAME), __S, true)
#define SET_PS(NAME, __S) dx11CommonParams->safe_set_res(grd_helper::COMRES_INDICATOR(PIXEL_SHADER, NAME), __S, true)
#define SET_CS(NAME, __S) dx11CommonParams->safe_set_res(grd_helper::COMRES_INDICATOR(COMPUTE_SHADER, NAME), __S, true)
		
		string strNames_VS[VS_NUM] = {
			   "SR_OIT_P_vs_5_0"
			  ,"SR_OIT_PN_vs_5_0"
			  ,"SR_OIT_PT_vs_5_0"
			  ,"SR_OIT_PNT_vs_5_0"
			  ,"SR_OIT_PTTT_vs_5_0"
		};

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
		
		string strNames_PS[PS_NUM] = {
			   "SR_OIT_KDEPTH_PHONGBLINN_ps_5_0"
			  ,"SR_OIT_KDEPTH_DASHEDLINE_ps_5_0"
			  ,"SR_OIT_KDEPTH_MULTITEXTMAPPING_ps_5_0"
			  ,"SR_OIT_KDEPTH_TEXTMAPPING_ps_5_0"
			  ,"SR_OIT_KDEPTH_TEXTUREIMGMAP_ps_5_0"
			  ,"SR_SINGLE_LAYER_ps_5_0"
			  //,"SR_OIT_KDEPTH_NPRGHOST_ps_5_0"
			  
			  ,"SR_OIT_ABUFFER_FRAGCOUNTER_ps_5_0"
			  ,"SR_OIT_ABUFFER_PHONGBLINN_ps_5_0"
			  ,"SR_OIT_ABUFFER_DASHEDLINE_ps_5_0"
			  ,"SR_OIT_ABUFFER_MULTITEXTMAPPING_ps_5_0"
			  ,"SR_OIT_ABUFFER_TEXTMAPPING_ps_5_0"
			  ,"SR_OIT_ABUFFER_TEXTUREIMGMAP_ps_5_0"
			  
			  ,"SR_MOMENT_GEN_ps_5_0"
			  ,"SR_MOMENT_OIT_PHONGBLINN_ps_5_0"
			  ,"SR_MOMENT_OIT_DASHEDLINE_ps_5_0"
			  ,"SR_MOMENT_OIT_MULTITEXTMAPPING_ps_5_0"
			  ,"SR_MOMENT_OIT_TEXTMAPPING_ps_5_0"
			  ,"SR_MOMENT_OIT_TEXTUREIMGMAP_ps_5_0"
		};

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
		
		string strNames_CS[CS_NUM] = {
			   "SR_OIT_SORT2RENDER_cs_5_0"
			  ,"SR_OIT_PRESET_cs_5_0"
			  ,"SR_OIT_ABUFFER_PREFIX_0_cs_5_0"
			  ,"SR_OIT_ABUFFER_PREFIX_1_cs_5_0"
			  ,"SR_OIT_ABUFFER_SORT2SENDER_cs_5_0"
			  ,"KB_SSAO_cs_5_0"
			  ,"KB_SSAO_BLUR_cs_5_0"
			  ,"KBZ_TO_TEXTURE_cs_5_0"
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
		/**/
	}

	ID3D11InputLayout* dx11LI_P = (ID3D11InputLayout*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(INPUT_LAYOUT, "P"));
	ID3D11InputLayout* dx11LI_PN = (ID3D11InputLayout*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(INPUT_LAYOUT, "PN"));
	ID3D11InputLayout* dx11LI_PT = (ID3D11InputLayout*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(INPUT_LAYOUT, "PT"));
	ID3D11InputLayout* dx11LI_PNT = (ID3D11InputLayout*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(INPUT_LAYOUT, "PNT"));
	ID3D11InputLayout* dx11LI_PTTT = (ID3D11InputLayout*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(INPUT_LAYOUT, "PTTT"));

	ID3D11VertexShader* dx11VShader_P = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(VERTEX_SHADER, "SR_OIT_P_vs_5_0"));
	ID3D11VertexShader* dx11VShader_PN = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(VERTEX_SHADER, "SR_OIT_PN_vs_5_0"));
	ID3D11VertexShader* dx11VShader_PT = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(VERTEX_SHADER, "SR_OIT_PT_vs_5_0"));
	ID3D11VertexShader* dx11VShader_PNT = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(VERTEX_SHADER, "SR_OIT_PNT_vs_5_0"));
	ID3D11VertexShader* dx11VShader_PTTT = (ID3D11VertexShader*)dx11CommonParams->safe_get_res(COMRES_INDICATOR(VERTEX_SHADER, "SR_OIT_PTTT_vs_5_0"));

	ID3D11Buffer* cbuf_cam_state = dx11CommonParams->get_cbuf("CB_CameraState");
	ID3D11Buffer* cbuf_env_state = dx11CommonParams->get_cbuf("CB_EnvState");
	ID3D11Buffer* cbuf_clip = dx11CommonParams->get_cbuf("CB_ClipInfo");
	ID3D11Buffer* cbuf_pobj = dx11CommonParams->get_cbuf("CB_PolygonObject");
	ID3D11Buffer* cbuf_vobj = dx11CommonParams->get_cbuf("CB_VolumeObject");
	ID3D11Buffer* cbuf_reffect = dx11CommonParams->get_cbuf("CB_RenderingEffect");
	ID3D11Buffer* cbuf_tmap = dx11CommonParams->get_cbuf("CB_TMAP");
	ID3D11Buffer* cbuf_hsmask = dx11CommonParams->get_cbuf("CB_HotspotMask");
#pragma endregion // Shader Setting

#pragma region // IOBJECT CPU
	while (iobj->GetFrameBuffer(FrameBufferUsageRENDEROUT, 1) != NULL)
		iobj->DeleteFrameBuffer(FrameBufferUsageRENDEROUT, 1);
	if (!iobj->ReplaceFrameBuffer(FrameBufferUsageRENDEROUT, 0, data_type::dtype<vmbyte4>(), ("common render out frame buffer : defined in vismtv_inbuilt_renderergpudx module")))
		iobj->InsertFrameBuffer(data_type::dtype<vmbyte4>(), FrameBufferUsageRENDEROUT, ("common render out frame buffer : defined in vismtv_inbuilt_renderergpudx module"));

	while (iobj->GetFrameBuffer(FrameBufferUsageDEPTH, 1) != NULL)
		iobj->DeleteFrameBuffer(FrameBufferUsageDEPTH, 1);
	if (!iobj->ReplaceFrameBuffer(FrameBufferUsageDEPTH, 0, data_type::dtype<float>(), ("1st hit screen depth frame buffer : defined in vismtv_inbuilt_renderergpudx module")))
		iobj->InsertFrameBuffer(data_type::dtype<float>(), FrameBufferUsageDEPTH, ("1st hit screen depth frame buffer : defined in vismtv_inbuilt_renderergpudx module"));
#pragma endregion // IOBJECT CPU

	__ID3D11Device* dx11Device = dx11CommonParams->dx11Device;
	__ID3D11DeviceContext* dx11DeviceImmContext = dx11CommonParams->dx11DeviceImmContext;

#pragma region // IOBJECT GPU
	int buffer_ex = is_a_buffer? buf_ex_scale : 1, buffer_ex_old = 0; // optimal for K is 1
	vmint2 fb_size_cur, fb_size_old = vmint2(0, 0);
	iobj->GetFrameBufferInfo(&fb_size_cur);
	iobj->GetCustomParameter("_int2_PreviousScreenSize", data_type::dtype<vmint2>(), &fb_size_old);
	iobj->GetCustomParameter("_int_PreviousBufferEx", data_type::dtype<int>(), &buffer_ex_old);
	if (fb_size_cur.x != fb_size_old.x || fb_size_cur.y != fb_size_old.y
		|| num_deep_layers != num_deep_layers_old || num_moments != num_moments_old
		|| buffer_ex != buffer_ex_old)
	{
		gpu_manager->ReleaseGpuResourcesBySrcID(iobj->GetObjectID());	// System Out 포함 //
		iobj->RegisterCustomParameter("_int2_PreviousScreenSize", fb_size_cur);
		iobj->RegisterCustomParameter("_int_PreviousBufferEx", buffer_ex);
	}

	ullong lastest_render_time = 0;
	iobj->GetCustomParameter("_double_LatestSrTime", data_type::dtype<double>(), &lastest_render_time);

	GpuRes gres_fb_rgba, gres_fb_depthcs, gres_fb_depthstencil, gres_fb_counter, gres_fb_spinlock;
	GpuRes gres_fb_deep_k_buffer;
	GpuRes gres_fb_ao_texs[2], gres_fb_ao_blf_texs[2], gres_fb_mip_a_halftexs[2], gres_fb_mip_z_halftexs[2]; // max_layers
	GpuRes gres_fb_ao_vr_tex, gres_fb_ao_vr_blf_tex;
	GpuRes gres_fb_prefix_sum_dxLL;
	// A buffers... for test
	GpuRes gres_fb_sys_rgba, gres_fb_sys_depthcs, gres_fb_sys_deep_k, gres_fb_sys_prefix_sum_dxLL;
	// Ghost effect mode
	//GpuRes gres_fb_mask_hotspot;
	grd_helper::UpdateFrameBuffer(gres_fb_rgba, iobj, "RENDER_OUT_RGBA_0", RTYPE_TEXTURE2D,
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
	grd_helper::UpdateFrameBuffer(gres_fb_depthcs, iobj, "RENDER_OUT_DEPTH_0", RTYPE_TEXTURE2D,
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_FLOAT, 0);
	grd_helper::UpdateFrameBuffer(gres_fb_depthstencil, iobj, "DEPTH_STENCIL", RTYPE_TEXTURE2D,
		D3D11_BIND_DEPTH_STENCIL, DXGI_FORMAT_D32_FLOAT, false);
	grd_helper::UpdateFrameBuffer(gres_fb_counter, iobj, "RW_COUNTER", RTYPE_TEXTURE2D,
		D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_UINT, 0);
	grd_helper::UpdateFrameBuffer(gres_fb_spinlock, iobj, "RW_SPINLOCK", RTYPE_TEXTURE2D,
		D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_UINT, 0);

	grd_helper::UpdateFrameBuffer(gres_fb_deep_k_buffer, iobj, "BUFFER_RW_DEEP_K_BUF", RTYPE_BUFFER,
		D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_TYPELESS, UPFB_RAWBYTE, num_deep_layers * 4 * buffer_ex);

	// AO
	{
		grd_helper::UpdateFrameBuffer(gres_fb_ao_texs[0], iobj, "RW_TEXS_AO_0", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, UPFB_MIPMAP);
		grd_helper::UpdateFrameBuffer(gres_fb_ao_texs[1], iobj, "RW_TEXS_AO_1", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, UPFB_MIPMAP);
		grd_helper::UpdateFrameBuffer(gres_fb_ao_blf_texs[0], iobj, "RW_TEXS_AO_BLF_0", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, UPFB_MIPMAP);
		grd_helper::UpdateFrameBuffer(gres_fb_ao_blf_texs[1], iobj, "RW_TEXS_AO_BLF_1", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, UPFB_MIPMAP);
		grd_helper::UpdateFrameBuffer(gres_fb_mip_a_halftexs[0], iobj, "RW_TEXS_OPACITY_0", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, UPFB_MIPMAP | UPFB_HALF);
		grd_helper::UpdateFrameBuffer(gres_fb_mip_a_halftexs[1], iobj, "RW_TEXS_OPACITY_1", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R8G8B8A8_UNORM, UPFB_MIPMAP | UPFB_HALF);
		grd_helper::UpdateFrameBuffer(gres_fb_mip_z_halftexs[0], iobj, "RW_TEXS_Z_0", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32G32B32A32_FLOAT, UPFB_MIPMAP | UPFB_HALF);
		grd_helper::UpdateFrameBuffer(gres_fb_mip_z_halftexs[1], iobj, "RW_TEXS_Z_1", RTYPE_TEXTURE2D,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32G32B32A32_FLOAT, UPFB_MIPMAP | UPFB_HALF);
	}

	// RS 에서 스페셜로 빼내기
	// ...

	grd_helper::UpdateFrameBuffer(gres_fb_sys_rgba, iobj, "SYSTEM_OUT_RGBA", RTYPE_TEXTURE2D, NULL, DXGI_FORMAT_R8G8B8A8_UNORM, UPFB_SYSOUT);
	grd_helper::UpdateFrameBuffer(gres_fb_sys_depthcs, iobj, "SYSTEM_OUT_DEPTH", RTYPE_TEXTURE2D, NULL, DXGI_FORMAT_R32_FLOAT, UPFB_SYSOUT);

	if(is_a_buffer)
		grd_helper::UpdateFrameBuffer(gres_fb_prefix_sum_dxLL, iobj, "BUFFER_RW_LL_PREFIX_BUF", RTYPE_BUFFER,
			D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS, DXGI_FORMAT_R32_UINT, 0);
	if (pixel_transmittance)
	{
		grd_helper::UpdateFrameBuffer(gres_fb_sys_deep_k, iobj, "SYSTEM_OUT_DEEP_K_BUF", RTYPE_BUFFER,
			NULL, DXGI_FORMAT_R32_UINT, UPFB_SYSOUT, num_deep_layers * 3 * buffer_ex);
		if (is_a_buffer)
			grd_helper::UpdateFrameBuffer(gres_fb_sys_prefix_sum_dxLL, iobj, "SYSTEM_OUT_LL_PREFIX_BUF", RTYPE_BUFFER,
				NULL, DXGI_FORMAT_R32_UINT, UPFB_SYSOUT);
	}
	if (is_ghost_mode)
	{
		// do 'dynamic'
		auto UpdateMaskFB = [&_fncontainer, &gpu_manager, &dx11CommonParams](GpuRes& gres, const VmIObject* iobj, const string& res_name, bool updatemask)
		{
			gres.vm_src_id = iobj->GetObjectID();
			gres.res_name = res_name;
			vmint2 fb_size;
			((VmIObject*)iobj)->GetFrameBufferInfo(&fb_size);
			if (!gpu_manager->UpdateGpuResource(gres))
			{
				gres.rtype = RTYPE_BUFFER;
				gres.options["USAGE"] = D3D11_USAGE_DYNAMIC;
				gres.options["CPU_ACCESS_FLAG"] = D3D11_CPU_ACCESS_WRITE;
				gres.options["BIND_FLAG"] = D3D11_BIND_SHADER_RESOURCE;
				gres.options["FORMAT"] = DXGI_FORMAT_R32_FLOAT;
				gres.res_dvalues["NUM_ELEMENTS"] = fb_size.x * fb_size.y * 4;
				gres.res_dvalues["STRIDE_BYTES"] = sizeof(float);
				gpu_manager->GenerateGpuResource(gres);
				updatemask = true;
			}
			if (updatemask)
			{
				vmint4 mask_center_rs_0 = _fncontainer->GetParamValue("_int4_MaskCenterRadius0", vmint4(fb_size / 2, fb_size.x / 5, 5));
				vmint4 mask_center_rs_1 = _fncontainer->GetParamValue("_int4_MaskCenterRadius1", vmint4(fb_size / 2, 0, 5));
				vmdouble4 hotspot_params_0 = _fncontainer->GetParamValue("_double4_HotspotParamsTKtKsV0", vmdouble4(1., 1., 1., 1.));
				vmdouble4 hotspot_params_1 = _fncontainer->GetParamValue("_double4_HotspotParamsTKtKsV1", vmdouble4(1., 1., 1., 1.));
				float mask_bnd = (float)_fncontainer->GetParamValue("_double_MaskBndDisplay", (double)1.0);

				D3D11_MAPPED_SUBRESOURCE d11MappedRes;
				HRESULT hr = dx11CommonParams->dx11DeviceImmContext->Map((ID3D11Buffer*)gres.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_WRITE_DISCARD, 0, &d11MappedRes);
				if (hr != S_OK) cout << "ERROR HOTSPOT" << endl;
				float sm_v_max_0 = atan((float)mask_center_rs_0.w);
				vmfloat4* mask_buf = (vmfloat4*)d11MappedRes.pData;
				for (int y = 0; y < fb_size.y; y++ )
					for (int x = 0; x < fb_size.x; x++)
					{
						vmfloat4 mask_v = vmfloat4(0);
						vmfloat2 vdiff = vmfloat2(x, y) - vmfloat2(mask_center_rs_0.x, mask_center_rs_0.y);

						float length = glm::length(vdiff);
						float arc_x = max((mask_center_rs_0.z - length), 0) / mask_center_rs_0.z * 2.f - 1.f; // [-1 (outside), 1 (inside)]
						float value = (atan(arc_x * mask_center_rs_0.w) + sm_v_max_0) / (2.f * sm_v_max_0); // [0, 1]
						mask_v.w = value;
						mask_v.x = max(hotspot_params_0.x * value, 0.00001f); // thickness
						mask_v.y = max(hotspot_params_0.y - 1.f, 0) * value + 1.f;
						mask_v.z = hotspot_params_0.z * value;

						if(abs(length - mask_center_rs_0.z) < mask_bnd)
							mask_v.w = -1.f;

						//float value = (atan((length / mask_center_rs_0.z) * mask_center_rs_0.w) + sm_v_max_0) / (2.f * sm_v_max_0);
						//if (length <= mask_center_rs_0.z)
						//{
						//	mask_v = (vmfloat4)hotspot_params_0 * value;
						//}
						mask_buf[x + y * fb_size.x] = mask_v;
					}
				dx11CommonParams->dx11DeviceImmContext->Unmap((ID3D11Buffer*)gres.alloc_res_ptrs[DTYPE_RES], 0);
			}
		};
		//UpdateMaskFB(gres_fb_mask_hotspot, iobj, "BUFFER_MASK_HOTSPOT", true);

		
		D3D11_MAPPED_SUBRESOURCE mappedResHSMask;
		dx11DeviceImmContext->Map(cbuf_hsmask, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResHSMask);
		CB_HotspotMask* cbHSMaskData = (CB_HotspotMask*)mappedResHSMask.pData;
		grd_helper::SetCb_HotspotMask(*cbHSMaskData, _fncontainer, fb_size_old);
		dx11DeviceImmContext->Unmap(cbuf_hsmask, 0);
		dx11DeviceImmContext->PSSetConstantBuffers(9, 1, &cbuf_hsmask);
		dx11DeviceImmContext->CSSetConstantBuffers(9, 1, &cbuf_hsmask);
	}

#pragma endregion // IOBJECT GPU

#pragma region // Camera & Light Setting
	const int __BLOCKSIZE = 8;
	uint num_grid_x = (uint)ceil(fb_size_cur.x / (float)__BLOCKSIZE);
	uint num_grid_y = (uint)ceil(fb_size_cur.y / (float)__BLOCKSIZE);

	uint num_pobjs = (uint)input_pobjs.size();
	float len_diagonal_max = 0;

	vmfloat3 pos_aabb_min_ws(FLT_MAX), pos_aabb_max_ws(-FLT_MAX);
	for (uint i = 0; i < num_pobjs; i++)
	{
		VmVObjectPrimitive* pobj = (VmVObjectPrimitive*)input_pobjs[i];
		if (pobj->IsDefined() == false)
			continue;
		bool is_visible = true;
		pobj->GetCustomParameter("_bool_visibility", data_type::dtype<bool>(), &is_visible);
		if (!is_visible)
			continue;

		PrimitiveData* prim_data = pobj->GetPrimitiveData();
		vmmat44f matOS2WS = pobj->GetMatrixOS2WSf();

		vmfloat3 min_aabb, max_aabb;
		fTransformPoint(&min_aabb, &(vmfloat3)prim_data->aabb_os.pos_min, &matOS2WS);
		fTransformPoint(&max_aabb, &(vmfloat3)prim_data->aabb_os.pos_max, &matOS2WS);
		pos_aabb_min_ws.x = min(pos_aabb_min_ws.x, min_aabb.x);
		pos_aabb_min_ws.y = min(pos_aabb_min_ws.y, min_aabb.y);
		pos_aabb_min_ws.z = min(pos_aabb_min_ws.z, min_aabb.z);
		pos_aabb_max_ws.x = max(pos_aabb_max_ws.x, max_aabb.x);
		pos_aabb_max_ws.y = max(pos_aabb_max_ws.y, max_aabb.y);
		pos_aabb_max_ws.z = max(pos_aabb_max_ws.z, max_aabb.z);
	}
	len_diagonal_max = fLengthVector(&(pos_aabb_min_ws - pos_aabb_max_ws));
	if(test_consoleout)
		cout << "len_diagonal_max : " << len_diagonal_max << endl;
	if (v_thickness < 0)
	{
		if (len_diagonal_max == 0)
		{
			v_thickness = 0.001;
		}
		else
		{
			v_thickness = len_diagonal_max * 0.005; // min(len_diagonal_max * 0.005, 0.01);
		}
	}
	if (v_discont_depth < 0)
		v_discont_depth = len_diagonal_max * 0.1;
	iobj->RegisterCustomParameter("_double_ploygonobjs_diagonallenth", (double)len_diagonal_max);

	float fv_thickness = (float)v_thickness;
	VmCObject* cam_obj = iobj->GetCameraObject();
	vmmat44f matWS2SS, matWS2PS, matSS2WS;
	CB_CameraState cbCamState;
	grd_helper::SetCb_Camera(cbCamState, matWS2PS, matWS2SS, matSS2WS, cam_obj, fb_size_cur, num_deep_layers, fv_thickness);
	if(!is_a_buffer) cbCamState.iSrCamDummy__0 = *(uint*)&merging_beta;
	D3D11_MAPPED_SUBRESOURCE mappedResCamState;
	dx11DeviceImmContext->Map(cbuf_cam_state, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResCamState);
	CB_CameraState* cbCamStateData = (CB_CameraState*)mappedResCamState.pData;
	memcpy(cbCamStateData, &cbCamState, sizeof(CB_CameraState));
	dx11DeviceImmContext->Unmap(cbuf_cam_state, 0);
	dx11DeviceImmContext->VSSetConstantBuffers(0, 1, &cbuf_cam_state);
	dx11DeviceImmContext->GSSetConstantBuffers(0, 1, &cbuf_cam_state);
	dx11DeviceImmContext->PSSetConstantBuffers(0, 1, &cbuf_cam_state);
	dx11DeviceImmContext->CSSetConstantBuffers(0, 1, &cbuf_cam_state);

	//
	MomentOIT cb_moment;
	if (is_moment_oit)
	{
		ID3D11Buffer* cbuf_moment_state = dx11CommonParams->get_cbuf("MomentOIT");
		computeWrappingZoneParameters((float*)&cb_moment.wrapping_zone_parameters);
		cb_moment.overestimation = 0.25f;
		cb_moment.moment_bias = 0.0025f;
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

	CB_EnvState cbEnvState;
	grd_helper::SetCb_Env(cbEnvState, cam_obj, _fncontainer, (vmfloat3)global_light_factors);
	D3D11_MAPPED_SUBRESOURCE mappedResEnvState;
	dx11DeviceImmContext->Map(cbuf_env_state, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResEnvState);
	CB_EnvState* cbEnvStateData = (CB_EnvState*)mappedResEnvState.pData;
	memcpy(cbEnvStateData, &cbEnvState, sizeof(CB_EnvState));
	dx11DeviceImmContext->Unmap(cbuf_env_state, 0);
	dx11DeviceImmContext->PSSetConstantBuffers(7, 1, &cbuf_env_state);
	dx11DeviceImmContext->CSSetConstantBuffers(7, 1, &cbuf_env_state);

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
#pragma endregion // Camera & Light Setting

	auto GetParam = [&](const std::string& param_name, const GpuRes& gres) -> double
	{
		auto it = gres.res_dvalues.find(param_name);
		if (it == gres.res_dvalues.end())
		{
			::MessageBoxA(NULL, "Error GPU Parameters!!", NULL, MB_OK);
			return 0;
		}
		return it->second;
	};
#pragma region // Presetting of VxObject
	auto find_asscociated_obj = [&associated_objs](int obj_id) -> VmObject*
	{
		auto vol_obj = associated_objs.find(obj_id);
		if (vol_obj == associated_objs.end()) return NULL;
		return vol_obj->second;
	};
	map<int, GpuRes> mapGpuRes_Idx;
	map<int, GpuRes> mapGpuRes_Vtx;
	map<int, map<string, GpuRes>> mapGpuRes_Tex;
	map<int, GpuRes> mapGpuRes_VolumeAndTMap;

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

	vector<RenderObjInfo> general_oit_routine_objs;
	vector<RenderObjInfo> single_layer_routine_objs; // e.g. silhouette
	vector<RenderObjInfo> foremost_surfaces_routine_objs; // for performance //
	int minimum_oit_area = _fncontainer->GetParamValue("_int_MinimumOitArea", (int)1000); // 0 means turn-off the wildcard case
	int _w_max = 0;
	int _h_max = 0;
	for (uint i = 0; i < num_pobjs; i++)
	{
		VmVObjectPrimitive* pobj = (VmVObjectPrimitive*)input_pobjs[i];
		if (pobj->IsDefined() == false)
			continue;

		PrimitiveData* prim_data = pobj->GetPrimitiveData();
		if (prim_data->GetVerticeDefinition("POSITION") == NULL)
			continue;

		bool is_visible = true;
		pobj->GetCustomParameter("_bool_visibility", data_type::dtype<bool>(), &is_visible);
		if (!is_visible)
			continue;

		vmmat44f matOS2WS = pobj->GetMatrixOS2WSf(); // matWS2SS
		vmmat44f matOS2SS = matOS2WS * matWS2SS;
		int w, h;
		__compute_computespace_screen(w, h, matOS2SS, prim_data->aabb_os);
		bool is_wildcard_Candidate = w * h < minimum_oit_area;
		_w_max = max(_w_max, w);
		_h_max = max(_h_max, h);

		int vobj_id = 0, tobj_id = 0;
		int pobj_id = pobj->GetObjectID();
		lobj->GetDstObjValue(pobj_id, "_int_AssociatedVolumeID", &vobj_id);
		lobj->GetDstObjValue(pobj_id, "_int_AssociatedTObjectID", &tobj_id);

		VmVObjectVolume* vol_obj = (VmVObjectVolume*)find_asscociated_obj(vobj_id);
		VmTObject* tobj = (VmTObject*)find_asscociated_obj(tobj_id);
		RegisterVolumeRes(vol_obj, tobj, lobj, gpu_manager, dx11DeviceImmContext, associated_objs, mapGpuRes_VolumeAndTMap, progress);

		GpuRes gres_vtx, gres_idx;
		map<string, GpuRes> map_gres_texs;
		grd_helper::UpdatePrimitiveModel(gres_vtx, gres_idx, map_gres_texs, pobj);
		if (gres_vtx.alloc_res_ptrs.size() > 0)
			mapGpuRes_Vtx.insert(pair<int, GpuRes>(pobj_id, gres_vtx));
		if (gres_idx.alloc_res_ptrs.size() > 0)
			mapGpuRes_Idx.insert(pair<int, GpuRes>(pobj_id, gres_idx));
		if (map_gres_texs.size() > 0)
			mapGpuRes_Tex.insert(pair<int, map<string, GpuRes>>(pobj_id, map_gres_texs));

		//////////////////////////////////////////////
		// Register Valid Objects to Rendering List //
		vmdouble4 dColor(1.), dColorWire(1.);
		pobj->GetCustomParameter("_double4_color", data_type::dtype<vmdouble4>(), &dColor);
		lobj->GetDstObjValue(pobj_id, "_double4_color", &dColor);
		//if (dColor.a == 0)
		//	continue;
		bool is_wire = false;
		if (prim_data->ptype == PrimitiveTypeTRIANGLE)
		{
			pobj->GetPrimitiveWireframeVisibilityColor(&is_wire, &dColorWire);
			lobj->GetDstObjValue(pobj_id, "_bool_IsWireframe", &is_wire);
		}
		vmfloat4 fColor(dColor), fColorWire(dColorWire);

		bool is_foremost_surfaces = false;
		lobj->GetDstObjValue(pobj_id, "_bool_OnlyForemostSurfaces", &is_foremost_surfaces);
		//double pobj_vzthickness = -1.0;
		//lobj->GetDstObjValue(pobj_id, "_double_VzThickness", &pobj_vzthickness);
		//if (pobj_vzthickness <= 0)
		//	pobj_vzthickness = v_thickness;

		RenderObjInfo render_obj_info;
		render_obj_info.pobj = pobj;
		render_obj_info.vzthickness = v_copthickness < 0 ? len_diagonal_max * 0.0001f : (float)v_copthickness; //(float)pobj_vzthickness;
		//render_obj_info.vzthickness = v_copthickness < 0 ? len_diagonal_max * 0.0001f : (float)pobj_vzthickness;

		render_obj_info.num_safe_loopexit = num_safe_loopexit;
		lobj->GetDstObjValue(pobj_id, "_int_SpinLockSafeLoops", &render_obj_info.num_safe_loopexit);
		if (is_wildcard_Candidate) render_obj_info.num_safe_loopexit = 10;

		//if (render_obj_info.fColor.a > 0.99f && render_obj_info.vzthickness <= fv_thickness)
		lobj->GetDstObjValue(pobj_id, "_bool_AbsDiffuse", &render_obj_info.abs_diffuse);

		if (is_wire && prim_data->ptype == PrimitiveTypeTRIANGLE)
		{
			render_obj_info.is_wireframe = true;
			render_obj_info.fColor = fColorWire;
			//render_obj_info.fColor.w = 1.f;
			lobj->GetDstObjValue(pobj_id, "_bool_UseVertexWireColor", &render_obj_info.use_vertex_color);

			//if ((render_obj_info.fColor.a < 0.99f || force_to_oitpass || pobj_vzthickness > v_thickness) && !is_wildcard_Candidate)
			//	general_oit_routine_objs.push_back(render_obj_info);
			//else
			//	opaque_wildcard_objs.push_back(render_obj_info);
			if(is_foremost_surfaces) foremost_surfaces_routine_objs.push_back(render_obj_info);
			else general_oit_routine_objs.push_back(render_obj_info);
			//single_layer_routine_objs.push_back(render_obj_info);
		}

		{
			vmdouble4 shading_factors; // temp
			lobj->GetDstObjValue(pobj->GetObjectID(), "_double4_ShadingFactors", &shading_factors);
			vmdouble3 illum_model;
			if (!pobj->GetCustomParameter("_double3_IllumWavefront", data_type::dtype<vmdouble3>(), &illum_model))
				pobj->RegisterCustomParameter("_double_Ns", shading_factors.w);
		}

		pobj->GetCustomParameter("_bool_IsAnnotationObj", data_type::dtype<bool>(), &render_obj_info.is_annotation_obj);
		pobj->GetCustomParameter("_bool_HasTextureMap", data_type::dtype<bool>(), &render_obj_info.has_texture_img);
		if (render_obj_info.is_annotation_obj) render_obj_info.is_annotation_obj = prim_data->texture_res_info.size() > 0;
		render_obj_info.is_wireframe = false;
		render_obj_info.fColor = fColor;
		render_obj_info.use_vertex_color = true;
		lobj->GetDstObjValue(pobj_id, "_bool_UseVertexColor", &render_obj_info.use_vertex_color);
		if (prim_data->GetVerticeDefinition("TEXCOORD0") == NULL)
			render_obj_info.use_vertex_color = false;
		lobj->GetDstObjValue(pobj_id, "_bool_ShowOutline", &render_obj_info.show_outline);
		if (render_obj_info.show_outline)
			single_layer_routine_objs.push_back(render_obj_info);
		//render_obj_info.show_outline = false;
		//general_oit_routine_objs.push_back(render_obj_info);
		if (is_foremost_surfaces) foremost_surfaces_routine_objs.push_back(render_obj_info);
		else general_oit_routine_objs.push_back(render_obj_info);
	}

	bool print_out_routine_objs = _fncontainer->GetParamValue("_bool_PrintOutRoutineObjs", false) && fb_size_cur.x > 200 && fb_size_cur.y > 200;
	if (print_out_routine_objs)
	{
		cout << "  ** general_oit_routine_objs    : " << general_oit_routine_objs.size() << endl;
		cout << "  ** special_effect_routine_objs : " << single_layer_routine_objs.size() << endl;
		cout << "  ** foremost_sr_routine_objs : " << foremost_surfaces_routine_objs.size() << endl;
	}
	//print_out_routine_objs = true;
#pragma endregion // Presetting of VxObject

#pragma region // FrameBuffer Setting
	// Backup Previous Render Target //
	ID3D11RenderTargetView* pdxRTVOld = NULL;
	ID3D11DepthStencilView* pdxDSVOld = NULL;
	dx11DeviceImmContext->OMGetRenderTargets(1, &pdxRTVOld, &pdxDSVOld);

	// Clear Depth Stencil //
	ID3D11DepthStencilView* dx11DSV = (ID3D11DepthStencilView*)gres_fb_depthstencil.alloc_res_ptrs[DTYPE_DSV];
	dx11DeviceImmContext->ClearDepthStencilView(dx11DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

	float flt_max_ = FLT_MAX;
	uint flt_max_u = *(uint*)&flt_max_;
	uint clr_unit4[4] = { 0, 0, 0, 0 };
	uint clr_max_ufloat_4[4] = { flt_max_u, flt_max_u, flt_max_u, flt_max_u };
	float clr_float_zero_4[4] = { 0, 0, 0, 0 };
	float clr_float_fltmax_4[4] = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };
	//float clr_float_minus_4[4] = { -1.f, -1.f, -1.f, -1.f };
	dx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gres_fb_counter.alloc_res_ptrs[DTYPE_UAV], clr_unit4);
	dx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gres_fb_spinlock.alloc_res_ptrs[DTYPE_UAV], clr_unit4);
	//dx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gres_fb_deep_k_buffer.alloc_res_ptrs[DTYPE_UAV], clr_unit4);
	//if(is_rov_mode)
	//	dx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gres_fb_deep_k_buffer_rov.alloc_res_ptrs[DTYPE_UAV], clr_unit4);
	if(is_a_buffer)
		dx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gres_fb_prefix_sum_dxLL.alloc_res_ptrs[DTYPE_UAV], clr_unit4); 

	dx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_RTV], clr_float_zero_4);
	dx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_RTV], clr_float_fltmax_4);
#pragma endregion // FrameBuffer Setting

#pragma region // Other Presetting For Shaders


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

#pragma endregion // Other Presetting For Shaders

	QueryPerformanceCounter(&lIntCntStart);

	ID3D11DepthStencilView* dx11DSVNULL = NULL;
	ID3D11RenderTargetView* dx11RTVsNULL[2] = { NULL, NULL };
	ID3D11UnorderedAccessView* dx11UAVs_NULL[7] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };
	ID3D11ShaderResourceView* dx11SRVs_NULL[7] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };
	// For Each Primitive //
	int count_call_render = 0;
	int RENDERER_LOOP = 0;
	bool is_LL_counter_buffer = is_a_buffer;
	bool is_MOMENT_gen_buffer = is_moment_oit;
RENDERER_LOOP:
	vector<RenderObjInfo>* pvtrValidPrimitives;
	switch (RENDERER_LOOP)
	{
	case 0:
		pvtrValidPrimitives = &general_oit_routine_objs;
		break;
	case 1:
		pvtrValidPrimitives = &single_layer_routine_objs;
		{
			dx11DeviceImmContext->Map(cbuf_cam_state, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResCamState);
			CB_CameraState* cbCamStateData = (CB_CameraState*)mappedResCamState.pData;
			memcpy(cbCamStateData, &cbCamState, sizeof(CB_CameraState));
			cbCamStateData->cam_flag |= (0x1 << 1);
			dx11DeviceImmContext->Unmap(cbuf_cam_state, 0);
			dx11DeviceImmContext->CSSetConstantBuffers(0, 1, &cbuf_cam_state);
		}
		break;
	case 2:
		pvtrValidPrimitives = &foremost_surfaces_routine_objs;
		{
			dx11DeviceImmContext->Map(cbuf_cam_state, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResCamState);
			CB_CameraState* cbCamStateData = (CB_CameraState*)mappedResCamState.pData;
			memcpy(cbCamStateData, &cbCamState, sizeof(CB_CameraState));
			dx11DeviceImmContext->Unmap(cbuf_cam_state, 0);
			dx11DeviceImmContext->CSSetConstantBuffers(0, 1, &cbuf_cam_state);
		}
		break;
	default:
		goto RENDERER_LOOP_EXIT;
		break;
	}

	for (uint pobj_idx = 0; pobj_idx < (int)pvtrValidPrimitives->size(); pobj_idx++)
	{
		RenderObjInfo& render_obj_info = pvtrValidPrimitives->at(pobj_idx);
		VmVObjectPrimitive* pobj = render_obj_info.pobj;
		PrimitiveData* prim_data = (PrimitiveData*)pobj->GetPrimitiveData();

		int pobj_id = pobj->GetObjectID();

		// Object Unit Conditions
		bool cull_off = false;
		bool is_clip_free = false;
		bool apply_shadingfactors = true;
		int vobj_id = 0, tobj_id = 0;
		pobj->GetCustomParameter("_bool_IsForcedCullOff", data_type::dtype<bool>(), &cull_off);
		lobj->GetDstObjValue(pobj_id, "_int_AssociatedVolumeID", &vobj_id);
		lobj->GetDstObjValue(pobj_id, "_int_AssociatedTObjectID", &tobj_id);
		bool with_vobj = false;
		auto itrGpuVolume = mapGpuRes_VolumeAndTMap.find(vobj_id);
		auto itrGpuTObject = mapGpuRes_VolumeAndTMap.find(tobj_id);
		with_vobj = itrGpuVolume != mapGpuRes_VolumeAndTMap.end() && itrGpuTObject != mapGpuRes_VolumeAndTMap.end();
		
		if (with_vobj)
		{
			GpuRes& gres_vobj = itrGpuVolume->second;
			GpuRes& gres_tobj = itrGpuTObject->second;
			dx11DeviceImmContext->VSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&gres_tobj.alloc_res_ptrs[DTYPE_SRV]);
			dx11DeviceImmContext->PSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&gres_tobj.alloc_res_ptrs[DTYPE_SRV]);
			dx11DeviceImmContext->VSSetShaderResources(1, 1, (ID3D11ShaderResourceView**)&gres_vobj.alloc_res_ptrs[DTYPE_SRV]);
			dx11DeviceImmContext->PSSetShaderResources(1, 1, (ID3D11ShaderResourceView**)&gres_vobj.alloc_res_ptrs[DTYPE_SRV]);

			auto itrVolume = associated_objs.find(vobj_id);
			auto itrTObject = associated_objs.find(tobj_id);
			VmVObjectVolume* vobj = (VmVObjectVolume*)itrVolume->second;
			VmTObject* tobj = (VmTObject*)itrTObject->second;

			bool high_samplerate = gres_vobj.res_dvalues["SAMPLE_OFFSET_X"] > 1.f ||
				gres_vobj.res_dvalues["SAMPLE_OFFSET_Y"] > 1.f || gres_vobj.res_dvalues["SAMPLE_OFFSET_Z"] > 1.f;

			CB_VolumeObject cbVolumeObj;
			vmint3 vol_sampled_size = vmint3(gres_vobj.res_dvalues["WIDTH"], gres_vobj.res_dvalues["HEIGHT"], gres_vobj.res_dvalues["DEPTH"]);
			grd_helper::SetCb_VolumeObj(cbVolumeObj, vobj, lobj, _fncontainer, high_samplerate, vol_sampled_size, 0, 0);
			cbVolumeObj.pb_shading_factor = global_light_factors;
			D3D11_MAPPED_SUBRESOURCE mappedResVolObj;
			dx11DeviceImmContext->Map(cbuf_vobj, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResVolObj);
			CB_VolumeObject* cbVolumeObjData = (CB_VolumeObject*)mappedResVolObj.pData;
			memcpy(cbVolumeObjData, &cbVolumeObj, sizeof(CB_VolumeObject));
			dx11DeviceImmContext->Unmap(cbuf_vobj, 0);

			CB_TMAP cbTmap;
			grd_helper::SetCb_TMap(cbTmap, tobj);
			D3D11_MAPPED_SUBRESOURCE mappedResTmap;
			dx11DeviceImmContext->Map(cbuf_tmap, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResTmap);
			CB_TMAP* cbTmapData = (CB_TMAP*)mappedResTmap.pData;
			memcpy(cbTmapData, &cbTmap, sizeof(CB_TMAP));
			dx11DeviceImmContext->Unmap(cbuf_tmap, 0);
		}

		int tex_map_enum = 0;
		if (render_obj_info.is_annotation_obj)
		{
			map<string, GpuRes>& map_gres_texs = mapGpuRes_Tex[pobj_id];
			auto it_tex = map_gres_texs.find("MAP_COLOR4");
			if (it_tex != map_gres_texs.end())
			{
				tex_map_enum = 1;
				GpuRes& gres_tex = it_tex->second;
				dx11DeviceImmContext->PSSetShaderResources(3, 1, (ID3D11ShaderResourceView**)&gres_tex.alloc_res_ptrs[DTYPE_SRV]);
				if (default_color_cmmobj.x >= 0 && default_color_cmmobj.y >= 0 && default_color_cmmobj.z >= 0)
					render_obj_info.fColor = vmfloat4(default_color_cmmobj, render_obj_info.fColor.a);
			}
		}
		else if (render_obj_info.has_texture_img)
		{
			map<string, GpuRes>& map_gres_texs = mapGpuRes_Tex[pobj_id];
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

		CB_RenderingEffect cbRenderEffect;
		grd_helper::SetCb_RenderingEffect(cbRenderEffect, pobj, lobj, render_obj_info);
		D3D11_MAPPED_SUBRESOURCE mappedResRenderEffect;
		dx11DeviceImmContext->Map(cbuf_reffect, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResRenderEffect);
		CB_RenderingEffect* cbRenderEffectData = (CB_RenderingEffect*)mappedResRenderEffect.pData;
		memcpy(cbRenderEffectData, &cbRenderEffect, sizeof(CB_RenderingEffect));
		dx11DeviceImmContext->Unmap(cbuf_reffect, 0);

		CB_PolygonObject cbPolygonObj;
		cbPolygonObj.tex_map_enum = tex_map_enum;
		vmmat44f matOS2WS = pobj->GetMatrixOS2WSf();
		grd_helper::SetCb_PolygonObj(cbPolygonObj, pobj, lobj, matOS2WS, matWS2SS, matWS2PS, render_obj_info, default_point_thickness, default_line_thickness);
		if (RENDERER_LOOP == 1 && render_obj_info.show_outline)
		{
			cbPolygonObj.depth_forward_bias = (float)v_discont_depth;
			cbPolygonObj.pobj_flag |= 0x1;
		}

		if (is_ghost_mode && !IS_SAFE_OBJ(pobj_id))
			cbPolygonObj.pobj_flag |= 0x1 << 22;
		D3D11_MAPPED_SUBRESOURCE mappedResPobjData;
		dx11DeviceImmContext->Map(cbuf_pobj, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResPobjData);
		CB_PolygonObject* cbPolygonObjData = (CB_PolygonObject*)mappedResPobjData.pData;
		memcpy(cbPolygonObjData, &cbPolygonObj, sizeof(CB_PolygonObject));
		dx11DeviceImmContext->Unmap(cbuf_pobj, 0);

		CB_ClipInfo cbClipInfo;
		grd_helper::SetCb_ClipInfo(cbClipInfo, pobj, lobj);
		D3D11_MAPPED_SUBRESOURCE mappedResClipInfo;
		dx11DeviceImmContext->Map(cbuf_clip, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResClipInfo);
		CB_ClipInfo* cbClipInfoData = (CB_ClipInfo*)mappedResClipInfo.pData;
		memcpy(cbClipInfoData, &cbClipInfo, sizeof(CB_ClipInfo));
		dx11DeviceImmContext->Unmap(cbuf_clip, 0);

#pragma region // Setting Rasterization Stages
		ID3D11InputLayout* dx11InputLayer_Target = NULL;
		ID3D11VertexShader* dx11VS_Target = NULL;
		ID3D11GeometryShader* dx11GS_Target = NULL;
		ID3D11PixelShader* dx11PS_Target = NULL;
		ID3D11RasterizerState2* dx11RState_TargetObj = NULL;
		uint offset = 0;
		D3D_PRIMITIVE_TOPOLOGY pobj_topology_type;

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

			if (render_obj_info.is_annotation_obj && dx11InputLayer_Target == dx11LI_PNT)
				dx11PS_Target = is_a_buffer? GETPS(SR_OIT_ABUFFER_TEXTMAPPING_ps_5_0) : is_moment_oit? GETPS(SR_MOMENT_OIT_TEXTMAPPING_ps_5_0) : GETPS(SR_OIT_KDEPTH_TEXTMAPPING_ps_5_0);
			else if(render_obj_info.has_texture_img && dx11InputLayer_Target == dx11LI_PNT)
				dx11PS_Target = is_a_buffer ? GETPS(SR_OIT_ABUFFER_TEXTUREIMGMAP_ps_5_0) : is_moment_oit ? GETPS(SR_MOMENT_OIT_TEXTUREIMGMAP_ps_5_0) : GETPS(SR_OIT_KDEPTH_TEXTUREIMGMAP_ps_5_0);
			else
				dx11PS_Target = is_a_buffer ? GETPS(SR_OIT_ABUFFER_PHONGBLINN_ps_5_0) : is_moment_oit ? GETPS(SR_MOMENT_OIT_PHONGBLINN_ps_5_0) : GETPS(SR_OIT_KDEPTH_PHONGBLINN_ps_5_0);
		}
		else if (prim_data->GetVerticeDefinition("TEXCOORD0"))
		{
			if (prim_data->GetVerticeDefinition("TEXCOORD2"))
			{
				assert(pvtrValidPrimitives != &single_layer_routine_objs);
				// only text case //
				// PTTT
				dx11InputLayer_Target = dx11LI_PTTT;
				dx11VS_Target = dx11VShader_PTTT;
				dx11PS_Target = is_a_buffer ? GETPS(SR_OIT_ABUFFER_MULTITEXTMAPPING_ps_5_0) : is_moment_oit ? GETPS(SR_MOMENT_OIT_MULTITEXTMAPPING_ps_5_0) : GETPS(SR_OIT_KDEPTH_MULTITEXTMAPPING_ps_5_0);
			}
			else
			{
				// if (render_obj_info.use_vertex_color)
				// PT
				dx11InputLayer_Target = dx11LI_PT;
				dx11VS_Target = dx11VShader_PT;

				if (render_obj_info.is_annotation_obj)
					dx11PS_Target = is_a_buffer ? GETPS(SR_OIT_ABUFFER_TEXTMAPPING_ps_5_0) : is_moment_oit ? GETPS(SR_MOMENT_OIT_TEXTMAPPING_ps_5_0) : GETPS(SR_OIT_KDEPTH_TEXTMAPPING_ps_5_0);
				else if ((cbPolygonObj.pobj_flag & (0x1 << 19)) && prim_data->ptype == PrimitiveTypeLINE)
					dx11PS_Target = is_a_buffer ? GETPS(SR_OIT_ABUFFER_DASHEDLINE_ps_5_0) : is_moment_oit ? GETPS(SR_MOMENT_OIT_DASHEDLINE_ps_5_0) : GETPS(SR_OIT_KDEPTH_DASHEDLINE_ps_5_0);
				else
					dx11PS_Target = is_a_buffer ? GETPS(SR_OIT_ABUFFER_PHONGBLINN_ps_5_0) : is_moment_oit ? GETPS(SR_MOMENT_OIT_PHONGBLINN_ps_5_0) : GETPS(SR_OIT_KDEPTH_PHONGBLINN_ps_5_0);
			}
		}
		else
		{
			// P
			dx11InputLayer_Target = dx11LI_P;
			dx11VS_Target = dx11VShader_P;
			dx11PS_Target = dx11PS_Target = is_a_buffer ? GETPS(SR_OIT_ABUFFER_PHONGBLINN_ps_5_0) : is_moment_oit ? GETPS(SR_MOMENT_OIT_PHONGBLINN_ps_5_0) : GETPS(SR_OIT_KDEPTH_PHONGBLINN_ps_5_0);
		}

		if (pvtrValidPrimitives == &single_layer_routine_objs || pvtrValidPrimitives == &foremost_surfaces_routine_objs)
			dx11PS_Target = GETPS(SR_SINGLE_LAYER_ps_5_0); // ONLY K-BUFFER


		if (pvtrValidPrimitives == &general_oit_routine_objs)
		{
			if (is_LL_counter_buffer)
			{
				// Create a count of the number of fragments at each pixel location
				//CreateFragmentCount(pD3DContext, pScene, mWorldViewProjection, pRTV, pDSV);
				dx11PS_Target = GETPS(SR_OIT_ABUFFER_FRAGCOUNTER_ps_5_0);
			}
			else if (is_MOMENT_gen_buffer)
			{
				dx11PS_Target = GETPS(SR_MOMENT_GEN_ps_5_0);
			}
		}

		switch (prim_data->ptype)
		{
		case PrimitiveTypeLINE:
			if (prim_data->is_stripe)
				pobj_topology_type = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
			else
				pobj_topology_type = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
			if (cbPolygonObj.pix_thickness > 0)
				dx11GS_Target = GETGS(GS_ThickLines_gs_5_0);
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
				dx11GS_Target = GETGS(GS_ThickPoints_gs_5_0);
			break;
		default:
			continue;
		}
		
#define GETRASTER(NAME) dx11CommonParams->get_rasterizer(#NAME)

		bool force_to_pointsetrender = false;
		lobj->GetDstObjValue(pobj_id, "_bool_ForceToPointsetRender", &force_to_pointsetrender);
		if (force_to_pointsetrender)
		{
			dx11RState_TargetObj = GETRASTER(SOLID_NONE);
			pobj_topology_type = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
			if (cbPolygonObj.pix_thickness > 0)
				dx11GS_Target = GETGS(GS_ThickPoints_gs_5_0);
		}

		if (render_obj_info.is_wireframe)
		{
			dx11RState_TargetObj = GETRASTER(WIRE_NONE);
			dx11GS_Target = NULL;
		}
		else
			dx11RState_TargetObj = GETRASTER(SOLID_NONE);
		//dx11RState_TargetObj = GETRASTER(SOLID_CCW);

		auto itrMapBufferVtx = mapGpuRes_Vtx.find(pobj_id);

		//{
		//	if (dx11InputLayer_Target == dx11LI_PNT && prim_data->num_vtx > 1000000)
		//	{
		//		cout << "# of vtx : " << prim_data->num_vtx << endl;
		//
		//		ID3D11Buffer* pdx11bufvtx = (ID3D11Buffer*)itrMapBufferVtx->second.alloc_res_ptrs[DTYPE_RES];
		//		D3D11_MAPPED_SUBRESOURCE mappedRes;
		//		dx11CommonParams->dx11DeviceImmContext->Map(pdx11bufvtx, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes);
		//		vmfloat3* vtxbuf = (vmfloat3*)mappedRes.pData;
		//		static int gg = 0;
		//		gg++;
		//		for (int i = 0; i < prim_data->num_vtx; i++)
		//		{
		//			vtxbuf[3 * i + 2] = gg % 3 == 0 ? vmfloat3(1, 0, 0) : gg % 3 == 1 ? vmfloat3(0, 1, 0) : vmfloat3(0, 0, 1);
		//		}
		//		dx11CommonParams->dx11DeviceImmContext->Unmap(pdx11bufvtx, NULL);
		//	}
		//}

		ID3D11Buffer* dx11BufferTargetPrim = (ID3D11Buffer*)itrMapBufferVtx->second.alloc_res_ptrs[DTYPE_RES];
		ID3D11Buffer* dx11IndiceTargetPrim = NULL;
		uint stride_inputlayer = sizeof(vmfloat3)*(uint)prim_data->GetNumVertexDefinitions();
		dx11DeviceImmContext->IASetVertexBuffers(0, 1, (ID3D11Buffer**)&dx11BufferTargetPrim, &stride_inputlayer, &offset);
		if (prim_data->vidx_buffer != NULL)
		{
			auto itrMapBufferIdx = mapGpuRes_Idx.find(pobj_id);
			dx11IndiceTargetPrim = (ID3D11Buffer*)itrMapBufferIdx->second.alloc_res_ptrs[DTYPE_RES];
			dx11DeviceImmContext->IASetIndexBuffer(dx11IndiceTargetPrim, DXGI_FORMAT_R32_UINT, 0);
		}

		dx11DeviceImmContext->IASetInputLayout(dx11InputLayer_Target);
		dx11DeviceImmContext->VSSetShader(dx11VS_Target, NULL, 0);
		dx11DeviceImmContext->GSSetShader(dx11GS_Target, NULL, 0);
		dx11DeviceImmContext->PSSetShader(dx11PS_Target, NULL, 0);
		dx11DeviceImmContext->RSSetState(dx11RState_TargetObj);
		dx11DeviceImmContext->IASetPrimitiveTopology(pobj_topology_type);
#pragma endregion // Setting Rasterization Stages

#pragma region // Set Constant Buffers
		dx11DeviceImmContext->VSSetConstantBuffers(1, 1, &cbuf_pobj);
		dx11DeviceImmContext->PSSetConstantBuffers(1, 1, &cbuf_pobj);
		dx11DeviceImmContext->CSSetConstantBuffers(1, 1, &cbuf_pobj); // for silhouette or A-buffer test
		if (pobj_topology_type == D3D11_PRIMITIVE_TOPOLOGY_POINTLIST || pobj_topology_type == D3D11_PRIMITIVE_TOPOLOGY_LINELIST)
			dx11DeviceImmContext->GSSetConstantBuffers(1, 1, &cbuf_pobj);

		dx11DeviceImmContext->PSSetConstantBuffers(2, 1, &cbuf_clip);
		dx11DeviceImmContext->PSSetConstantBuffers(3, 1, &cbuf_reffect);
		dx11DeviceImmContext->CSSetConstantBuffers(3, 1, &cbuf_reffect);

		if (with_vobj)
		{
			dx11DeviceImmContext->PSSetConstantBuffers(4, 1, &cbuf_vobj);
			dx11DeviceImmContext->PSSetConstantBuffers(5, 1, &cbuf_tmap);
		}
#pragma endregion //
#pragma region // 1st RENDERING PASS

#define GETDEPTHSTENTIL(NAME) dx11CommonParams->get_depthstencil(#NAME)
#define NUM_UAVs_1ST 4
		ID3D11UnorderedAccessView* dx11UAVs_1st_pass[NUM_UAVs_1ST] = {
				(ID3D11UnorderedAccessView*)gres_fb_counter.alloc_res_ptrs[DTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_fb_spinlock.alloc_res_ptrs[DTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_fb_deep_k_buffer.alloc_res_ptrs[DTYPE_UAV]
				, is_a_buffer ? (ID3D11UnorderedAccessView*)gres_fb_prefix_sum_dxLL.alloc_res_ptrs[DTYPE_UAV] : NULL
		};

		if (is_moment_oit && !is_MOMENT_gen_buffer)
		{
			dx11DeviceImmContext->PSSetShaderResources(20, 1, (ID3D11ShaderResourceView**)&gres_fb_deep_k_buffer.alloc_res_ptrs[DTYPE_SRV]);
			

			switch (RENDERER_LOOP)
			{
			case 0:
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
					dx11UAVs_1st_pass[2] = (ID3D11UnorderedAccessView*)gres_fb_spinlock.alloc_res_ptrs[DTYPE_UAV];
					dx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, NULL, 2, 3, dx11UAVs_1st_pass, 0);
				}
				break;
			case 1:
				// clear //
				// to do // 별도의 RT 를 만들어 두어야 한다.
				//dx11DeviceImmContext->ClearDepthStencilView(dx11DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
				//dx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(2, dx11RTVs, dx11DSV, 2, NUM_UAVs_1ST, dx11UAVs_NULL, 0);
				//dx11DeviceImmContext->OMSetDepthStencilState(GETDEPTHSTENTIL(LESSEQUAL), 0);
				break;
			}
		}
		else //if (is_MOMENT_gen_buffer || !is_moment_buffer)
		{
			ID3D11RenderTargetView* dx11RTVs[2] = {
				(ID3D11RenderTargetView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_RTV],
				(ID3D11RenderTargetView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_RTV] };

			switch (RENDERER_LOOP)
			{
			case 0:
				dx11DeviceImmContext->OMSetDepthStencilState(GETDEPTHSTENTIL(ALWAYS), 0);
				dx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(0, NULL, NULL, 2, NUM_UAVs_1ST, dx11UAVs_1st_pass, NULL);
				break;
			case 1:
				// clear //
				dx11DeviceImmContext->ClearDepthStencilView(dx11DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

				//dx11DeviceImmContext->OMSetRenderTargets(2, dx11RTVs, dx11DSV);
				dx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(2, dx11RTVs, dx11DSV, 2, NUM_UAVs_1ST, dx11UAVs_NULL, 0);
				dx11DeviceImmContext->OMSetDepthStencilState(GETDEPTHSTENTIL(LESSEQUAL), 0);
				break;
			case 2:
				dx11DeviceImmContext->OMSetRenderTargets(2, dx11RTVs, dx11DSV);
				//dx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(2, dx11RTVs, dx11DSV, 2, NUM_UAVs_1ST, dx11UAVs_NULL, 0);
				dx11DeviceImmContext->OMSetDepthStencilState(GETDEPTHSTENTIL(LESSEQUAL), 0);
				break;
			}
		}

		if (render_obj_info.fColor.a == 0) continue;
		//dx11DeviceImmContext->Flush();
		if (prim_data->is_stripe || pobj_topology_type == D3D11_PRIMITIVE_TOPOLOGY_POINTLIST)
			dx11DeviceImmContext->Draw(prim_data->num_vtx, 0);
		else
			dx11DeviceImmContext->DrawIndexed(prim_data->num_vidx, 0, 0);
		count_call_render++;

		dx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(2, dx11RTVsNULL, dx11DSVNULL, 2, 5, dx11UAVs_NULL, 0);

		if (RENDERER_LOOP == 1 && !is_a_buffer && !is_moment_oit)
		{
			// RT to K-Buffer
			ID3D11ShaderResourceView* dx11SRVs_1st_pass[2] = {
				  (ID3D11ShaderResourceView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_SRV]
				, (ID3D11ShaderResourceView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_SRV]
			};
			dx11DeviceImmContext->CSSetUnorderedAccessViews(2, NUM_UAVs_1ST, dx11UAVs_1st_pass, (UINT*)(&dx11UAVs_1st_pass));
			dx11DeviceImmContext->CSSetShaderResources(10, 2, dx11SRVs_1st_pass);

			UINT UAVInitialCounts = 0;
			dx11DeviceImmContext->CSSetShader(GETCS(SR_OIT_PRESET_cs_5_0), NULL, 0);

			dx11DeviceImmContext->Flush();
			dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);

			dx11DeviceImmContext->CSSetUnorderedAccessViews(2, 5, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
			dx11DeviceImmContext->CSSetShaderResources(10, 2, dx11SRVs_NULL);

			dx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_RTV], clr_float_fltmax_4);
			dx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_RTV], clr_float_zero_4);
		}

		// window x 크기가 500 이상일 때, buffer out 해서 thick 만.. 찍어 보기.
		//if (fb_size_cur.x > 500)
		//{
		//	dx11DeviceImmContext->Flush();
		//	ID3D11Buffer* pDeepBufferThick_SYS;
		//	D3D11_BUFFER_DESC descBuf;
		//	ZeroMemory(&descBuf, sizeof(D3D11_BUFFER_DESC));
		//	descBuf.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		//	descBuf.ByteWidth = fb_size_cur.x * fb_size_cur.y * 8 * sizeof(float);
		//	descBuf.StructureByteStride = sizeof(uint);
		//	descBuf.Usage = D3D11_USAGE_STAGING;
		//	descBuf.BindFlags = NULL;
		//	descBuf.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
		//	dx11Device->CreateBuffer(&descBuf, NULL, &pDeepBufferThick_SYS);
		//
		//	ID3D11Texture2D* pTexCnt_SYS;
		//	D3D11_TEXTURE2D_DESC desc2D;
		//	ZeroMemory(&desc2D, sizeof(D3D11_TEXTURE2D_DESC));
		//	desc2D.ArraySize = 1;
		//	desc2D.Format = DXGI_FORMAT_R32_UINT;
		//	desc2D.Width = fb_size_cur.x;
		//	desc2D.Height = fb_size_cur.y;
		//	desc2D.MipLevels = 1;
		//	desc2D.SampleDesc.Count = 1;
		//	desc2D.SampleDesc.Quality = 0;
		//	desc2D.Usage = D3D11_USAGE_STAGING;
		//	desc2D.BindFlags = NULL;
		//	desc2D.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
		//	dx11Device->CreateTexture2D(&desc2D, NULL, &pTexCnt_SYS);
		//
		//	dx11DeviceImmContext->CopyResource(pDeepBufferThick_SYS, (ID3D11Buffer*)res_deep_zthick_buf.vtrPtrs[0]);
		//	dx11DeviceImmContext->CopyResource(pTexCnt_SYS, (ID3D11Buffer*)res_counter_tex.vtrPtrs[0]);
		//
		//	cv::Mat img_test(fb_size_cur.y, fb_size_cur.x, CV_8UC3);
		//	unsigned char* cv_buffer = (unsigned char*)img_test.data;
		//	D3D11_MAPPED_SUBRESOURCE mappedResSysDeepThick, mappedResSysTexCnt;
		//	HRESULT hr = dx11DeviceImmContext->Map(pDeepBufferThick_SYS, 0, D3D11_MAP_READ, NULL, &mappedResSysDeepThick);
		//	hr = dx11DeviceImmContext->Map(pTexCnt_SYS, 0, D3D11_MAP_READ, NULL, &mappedResSysTexCnt);
		//	unsigned int* rgba_gpu_buf_deep_thick = (unsigned int*)mappedResSysDeepThick.pData;
		//	unsigned int* rgba_gpu_buf_cnt = (unsigned int*)mappedResSysTexCnt.pData;
		//	int buf_row_pitch_cnt = mappedResSysTexCnt.RowPitch / 4;
		//	for (int i = 0; i < fb_size_cur.y; i++)
		//	{
		//		for (int j = 0; j < fb_size_cur.x; j++)
		//		{
		//			int addr_cv = (j + i * fb_size_cur.x) * 3;
		//			cv_buffer[addr_cv + 0] = 0;
		//			cv_buffer[addr_cv + 1] = 0;
		//			cv_buffer[addr_cv + 2] = 0;
		//			unsigned int _r = rgba_gpu_buf_cnt[(j + i * buf_row_pitch_cnt)];
		//			//unsigned int _r = rgba_gpu_buf_deep_color[(j + i * Width) * 4 * 8 + 0];
		//			//unsigned int _g = rgba_gpu_buf_deep_color[(j + i * Width) * 4 * 8 + 1];
		//			//unsigned int _b = rgba_gpu_buf_deep_color[(j + i * Width) * 4 * 8 + 2];
		//			for (int k = 0; k < 8; k++)
		//			{
		//				unsigned int _t = rgba_gpu_buf_deep_thick[(j + i * fb_size_cur.x) * 8 + k];
		//				if (_t > 0)
		//				{
		//					if (_t == 10000)
		//						cv_buffer[addr_cv + 0] = 255;
		//					else if (_t == 30000)
		//						cv_buffer[addr_cv + 1] = 255;
		//					//addr_cv -= 160 * 3;
		//					//cv_buffer[addr_cv + 0] = 255;
		//					//cv_buffer[addr_cv + 1] = 0;
		//					//cv_buffer[addr_cv + 2] = 0;
		//				}
		//				if (_r > 0)
		//				{
		//					//addr_cv -= 160 * 3;
		//					cv_buffer[addr_cv + 2] = 255;
		//				}
		//			}
		//		}
		//	}
		//	dx11DeviceImmContext->Unmap(pDeepBufferThick_SYS, 0);
		//	dx11DeviceImmContext->Unmap(pTexCnt_SYS, 0);
		//	cv::cvtColor(img_test, img_test, CV_RGB2BGR);
		//	cv::imwrite("d:/oit_test_1st.bmp", img_test);
		//	VMSAFE_RELEASE(pDeepBufferThick_SYS);
		//}
#pragma endregion // 1st RENDERING PASS
	}

#define NUM_UAVs_2ND 4
	if (is_a_buffer || is_moment_oit)
		if (RENDERER_LOOP == 1) goto RENDERER_LOOP_EXIT;

	if (is_LL_counter_buffer)
	{
		is_LL_counter_buffer = false;

		dx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(2, dx11RTVsNULL, dx11DSVNULL, 2, 5, dx11UAVs_NULL, 0);
		// Create a prefix sum of the fragment counts.  Each pixel location will hold
		// a count of the total number of fragments of every preceding pixel location.
		//CreatePrefixSum(pD3DContext);
		ID3D11UnorderedAccessView* dx11UAVs_2nd_pass[NUM_UAVs_2ND] = {
				  (ID3D11UnorderedAccessView*)gres_fb_deep_k_buffer.alloc_res_ptrs[DTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_fb_prefix_sum_dxLL.alloc_res_ptrs[DTYPE_UAV]
		};
		UINT UAVInitialCounts = 0;

		dx11DeviceImmContext->CSSetShader(GETCS(SR_OIT_ABUFFER_PREFIX_0_cs_5_0), NULL, 0);
		dx11DeviceImmContext->CSSetUnorderedAccessViews(1, NUM_UAVs_2ND, dx11UAVs_2nd_pass, (UINT*)(&dx11UAVs_2nd_pass));
		dx11DeviceImmContext->CSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&gres_fb_counter.alloc_res_ptrs[DTYPE_SRV]);
		dx11DeviceImmContext->Flush();

		//dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);
		dx11DeviceImmContext->Dispatch(fb_size_cur.x, fb_size_cur.y, 1);

		dx11DeviceImmContext->CSSetUnorderedAccessViews(1, 5, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
		dx11DeviceImmContext->CSSetShaderResources(0, 2, dx11SRVs_NULL);

		for (int i = 4; i < (fb_size_cur.x*fb_size_cur.y * 2); i *= 2)
		{
			D3D11_MAPPED_SUBRESOURCE mappedResCamState;
			dx11DeviceImmContext->Map(cbuf_cam_state, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResCamState);
			CB_CameraState* cbCamStateData = (CB_CameraState*)mappedResCamState.pData;
			memcpy(cbCamStateData, &cbCamState, sizeof(CB_CameraState));
			cbCamStateData->iSrCamDummy__0 = i;
			dx11DeviceImmContext->Unmap(cbuf_cam_state, 0);
			dx11DeviceImmContext->CSSetConstantBuffers(0, 1, &cbuf_cam_state);

			dx11DeviceImmContext->CSSetShader(GETCS(SR_OIT_ABUFFER_PREFIX_1_cs_5_0), NULL, 0);

			// flush effect for m_pPrefixSumUAV
			dx11DeviceImmContext->CSSetUnorderedAccessViews(1, NUM_UAVs_2ND, dx11UAVs_2nd_pass, (UINT*)(&dx11UAVs_2nd_pass));

			dx11DeviceImmContext->CSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&gres_fb_counter.alloc_res_ptrs[DTYPE_SRV]);

			// the "ceil((float) m_nFrameWidth*m_nFrameHeight/i)" calculation ensures that 
			//    we dispatch enough threads to cover the entire range.
			//dx11DeviceImmContext->Dispatch((int)(ceil((float)fb_size_cur.x*fb_size_cur.y / i)), 1, 1);
			dx11DeviceImmContext->Dispatch((int)(ceil((float)fb_size_cur.x)), (int)(ceil((float)fb_size_cur.y / i)), 1);
		}
		dx11DeviceImmContext->CSSetUnorderedAccessViews(1, 5, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
		dx11DeviceImmContext->CSSetShaderResources(0, 2, dx11SRVs_NULL);

		dx11DeviceImmContext->ClearUnorderedAccessViewUint((ID3D11UnorderedAccessView*)gres_fb_counter.alloc_res_ptrs[DTYPE_UAV], clr_unit4);
	}
	else if (is_MOMENT_gen_buffer)
	{
		is_MOMENT_gen_buffer = false;
		dx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(2, dx11RTVsNULL, dx11DSVNULL, 2, 5, dx11UAVs_NULL, 0);
	}
	else if (RENDERER_LOOP == 2 && foremost_surfaces_routine_objs.size() > 0)
	{
		ID3D11UnorderedAccessView* dx11UAVs_1st_pass[NUM_UAVs_1ST] = {
				(ID3D11UnorderedAccessView*)gres_fb_counter.alloc_res_ptrs[DTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_fb_spinlock.alloc_res_ptrs[DTYPE_UAV]
				, (ID3D11UnorderedAccessView*)gres_fb_deep_k_buffer.alloc_res_ptrs[DTYPE_UAV]
				, NULL
		};
		// RT to K-Buffer
		ID3D11ShaderResourceView* dx11SRVs_1st_pass[2] = {
				(ID3D11ShaderResourceView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_SRV]
			, (ID3D11ShaderResourceView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_SRV]
		};
		dx11DeviceImmContext->CSSetUnorderedAccessViews(2, NUM_UAVs_1ST, dx11UAVs_1st_pass, (UINT*)(&dx11UAVs_1st_pass));
		dx11DeviceImmContext->CSSetShaderResources(10, 2, dx11SRVs_1st_pass);

		UINT UAVInitialCounts = 0;
		dx11DeviceImmContext->CSSetShader(GETCS(SR_OIT_PRESET_cs_5_0), NULL, 0);

		dx11DeviceImmContext->Flush();
		dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);

		dx11DeviceImmContext->CSSetUnorderedAccessViews(2, 5, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
		dx11DeviceImmContext->CSSetShaderResources(10, 2, dx11SRVs_NULL);

		dx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_RTV], clr_float_fltmax_4);
		dx11DeviceImmContext->ClearRenderTargetView((ID3D11RenderTargetView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_RTV], clr_float_zero_4);
	}
	
	if (RENDERER_LOOP < 2)
	{
		RENDERER_LOOP++;
		goto RENDERER_LOOP;
	}
	//else
	//{
	//
	//}

RENDERER_LOOP_EXIT:
#pragma region // 2nd RENDERING PASS
	// sorting and merging
	auto DisplayTimes = [&](const string& _test)
	{
		QueryPerformanceFrequency(&lIntFreq);
		QueryPerformanceCounter(&lIntCntEnd);
		double dRunTime1 = (lIntCntEnd.QuadPart - lIntCntStart.QuadPart) / (double)(lIntFreq.QuadPart);

		if (print_out_routine_objs)
		{
			cout << "  ** screen size : " << fb_size_cur.x << " x " << fb_size_cur.y << endl;
			cout << "  ** rendering time (" << _test << ")   : " << dRunTime1 * 1000 << " ms" << endl;
			cout << "  ** rendering fps (" << _test << ")    : " << 1. / dRunTime1 << " fps" << endl;
		}
	};
	//DisplayTimes("1");

	dx11DeviceImmContext->OMSetRenderTargetsAndUnorderedAccessViews(2, dx11RTVsNULL, dx11DSVNULL, 2, 5, dx11UAVs_NULL, 0);

	UINT UAVInitialCounts = 0;

	ID3D11UnorderedAccessView* dx11UAVs_2nd_pass[NUM_UAVs_2ND] = {
			(ID3D11UnorderedAccessView*)gres_fb_deep_k_buffer.alloc_res_ptrs[DTYPE_UAV]
			, (ID3D11UnorderedAccessView*)gres_fb_rgba.alloc_res_ptrs[DTYPE_UAV]
			, (ID3D11UnorderedAccessView*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_UAV]
			, is_a_buffer ? (ID3D11UnorderedAccessView*)gres_fb_prefix_sum_dxLL.alloc_res_ptrs[DTYPE_UAV] : NULL
	};
	if (!is_moment_oit)
	{
		if (is_a_buffer)
		{
			// Sort and render the fragments.  Use the prefix sum to determine where the 
			// fragments for each pixel reside.
			//SortAndRenderFragments(pD3DContext, pDevice, pRTV);
			dx11DeviceImmContext->CSSetShader(GETCS(SR_OIT_ABUFFER_SORT2SENDER_cs_5_0), NULL, 0);
			dx11DeviceImmContext->CSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&gres_fb_counter.alloc_res_ptrs[DTYPE_SRV]);
		}
		else
		{
			dx11DeviceImmContext->CSSetShader(GETCS(SR_OIT_SORT2RENDER_cs_5_0), NULL, 0);
			//dx11DeviceImmContext->CSSetShaderResources(0, 1, (ID3D11ShaderResourceView**)&gres_fb_counter.alloc_res_ptrs[DTYPE_SRV]);
			dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 1, (ID3D11UnorderedAccessView**)&gres_fb_counter.alloc_res_ptrs[DTYPE_UAV], 0);
		}

		dx11DeviceImmContext->CSSetUnorderedAccessViews(1, NUM_UAVs_2ND, dx11UAVs_2nd_pass, (UINT*)(&dx11UAVs_2nd_pass));
		// 나중에 1D 로 변경... 
		dx11DeviceImmContext->Flush();
		//DisplayTimes("2");

		if (is_a_buffer)
			dx11DeviceImmContext->Dispatch(fb_size_cur.x, fb_size_cur.y, 1);
		else
			dx11DeviceImmContext->Dispatch(num_grid_x, num_grid_y, 1);

		// SSAO
		if(cbEnvState.r_kernel_ao > 0 && is_final_renderer)
		{
			//gres_fb_deep_k_SSAO
			dx11DeviceImmContext->Flush();
			dx11DeviceImmContext->CSSetUnorderedAccessViews(0, 5, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
			ComputeSSAO(dx11DeviceImmContext, dx11CommonParams, num_grid_x, num_grid_y,
				gres_fb_counter, gres_fb_deep_k_buffer, gres_fb_rgba, blur_SSAO,
				gres_fb_mip_z_halftexs, gres_fb_mip_a_halftexs, gres_fb_ao_texs, gres_fb_ao_blf_texs,
				gres_fb_depthcs, gres_fb_ao_vr_tex, gres_fb_ao_vr_blf_tex, false);
		}
	}
	//dx11DeviceImmContext->Flush();
	//DisplayTimes("3");

	// Set NULL States //
	dx11DeviceImmContext->CSSetUnorderedAccessViews(1, 5, dx11UAVs_NULL, (UINT*)(&dx11UAVs_NULL));
	dx11DeviceImmContext->CSSetShaderResources(0, 2, dx11SRVs_NULL);

#pragma endregion // 2nd RENDERING PASS
	//printf("# Textures : %d, # Drawing : %d, # RTBuffer Change : %d, # Merging : %d\n", iNumTexureLayers, iCountRendering, iCountRTBuffers, iCountMerging);

	//cout << "TEST VZ : " << v_thickness << endl;
	iobj->RegisterCustomParameter("_int_NumCallRenders", count_call_render);
	if (is_system_out)
	{
		FrameBuffer* fb_rout = (FrameBuffer*)iobj->GetFrameBuffer(FrameBufferUsageRENDEROUT, 0);
		FrameBuffer* fb_dout = (FrameBuffer*)iobj->GetFrameBuffer(FrameBufferUsageDEPTH, 0);

		if (count_call_render == 0)	// this means that there is no valid rendering pass
		{
			vmbyte4* rgba_buf = (vmbyte4*)fb_rout->fbuffer;
			float* depth_buf = (float*)fb_dout->fbuffer;
			
			memset(rgba_buf, 0, fb_size_cur.x * fb_size_cur.y * sizeof(vmbyte4));
			memset(depth_buf, 0x77, fb_size_cur.x * fb_size_cur.y * sizeof(float));
		}
		else
		{
#pragma region // Copy GPU to CPU
			dx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gres_fb_sys_rgba.alloc_res_ptrs[DTYPE_RES], 
				(ID3D11Texture2D*)gres_fb_rgba.alloc_res_ptrs[DTYPE_RES]);
			dx11DeviceImmContext->CopyResource((ID3D11Texture2D*)gres_fb_sys_depthcs.alloc_res_ptrs[DTYPE_RES], 
				(ID3D11Texture2D*)gres_fb_depthcs.alloc_res_ptrs[DTYPE_RES]);
			//dx11DeviceImmContext->Flush();
			//DisplayTimes("3-A");

			vmbyte4* rgba_sys_buf = (vmbyte4*)fb_rout->fbuffer;
			float* depth_sys_buf = (float*)fb_dout->fbuffer;

			D3D11_MAPPED_SUBRESOURCE mappedResSysRGBA;
			D3D11_MAPPED_SUBRESOURCE mappedResSysDepth;
			HRESULT hr = dx11DeviceImmContext->Map((ID3D11Texture2D*)gres_fb_sys_rgba.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, NULL, &mappedResSysRGBA);
			hr |= dx11DeviceImmContext->Map((ID3D11Texture2D*)gres_fb_sys_depthcs.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, NULL, &mappedResSysDepth);
			DisplayTimes("3-B");

			vmbyte4* rgba_gpu_buf = (vmbyte4*)mappedResSysRGBA.pData;
			if (rgba_gpu_buf == NULL || depth_sys_buf == NULL)
			{
				//test_out("SR ERROR -- OUT");
				//test_out("screen : " + to_string(fb_size_cur.x) + " x " + to_string(fb_size_cur.y));
				//test_out("v_thickness : " + to_string(fv_thickness));
				//test_out("num_deep_layers : " + to_string(num_deep_layers));
				//test_out("default_line_thickness : " + to_string(default_line_thickness));
				//test_out("width and height max : " + to_string(_w_max) + " x " + to_string(_h_max));
				//float* f_v = (float*)&matWS2SS;
				//for (int i = 0; i < 16; i++)
				//{
				//	test_out("matWS2SS " + to_string(i) + " : " + to_string(f_v[i]));
				//	if (f_v[i] != f_v[i]) test_out("matWS2SS " + to_string(i) + " - element error");
				//}
				//f_v = (float*)&matWS2PS;
				//for (int i = 0; i < 16; i++)
				//{
				//	test_out("matWS2PS " + to_string(i) + " : " + to_string(f_v[i]));
				//	if (f_v[i] != f_v[i]) test_out("matWS2PS " + to_string(i) + " - element error");
				//}
				//f_v = (float*)&matSS2WS;
				//for (int i = 0; i < 16; i++)
				//{
				//	test_out("matSS2WS " + to_string(i) + " : " + to_string(f_v[i]));
				//	if (f_v[i] != f_v[i]) test_out("matSS2WS " + to_string(i) + " - element error");
				//}
			}
			float* depth_gpu_buf = (float*)mappedResSysDepth.pData;
			int buf_row_pitch = mappedResSysRGBA.RowPitch / 4;
#ifdef PPL_USE
			int count = fb_size_cur.y;
			parallel_for(int(0), count, [&](int i) // is_rgba, fb_size_cur, rgba_sys_buf, depth_sys_buf, rgba_gpu_buf, depth_gpu_buf, buf_row_pitch
#else
#pragma omp parallel for 
			for (int i = 0; i < fb_size_cur.y; i++)
#endif
			{
				for (int j = 0; j < fb_size_cur.x; j++)
				{
					vmbyte4 rgba = rgba_gpu_buf[j + i * buf_row_pitch];
					// __PS_MERGE_LAYERS_TO_RENDEROUT 에서 INT -> FLOAT4 로 되어 배열된 color 요소가 들어 옴. //

					// BGRA
					if (is_rgba)
						rgba_sys_buf[i*fb_size_cur.x + j] = vmbyte4(rgba.x, rgba.y, rgba.z, rgba.w);
					else
						rgba_sys_buf[i*fb_size_cur.x + j] = vmbyte4(rgba.z, rgba.y, rgba.x, rgba.w);

					int iAddr = i * fb_size_cur.x + j;
					if (rgba.w > 0)
						depth_sys_buf[iAddr] = depth_gpu_buf[j + i * buf_row_pitch];
					else
						depth_sys_buf[iAddr] = FLT_MAX;
				}
#ifdef PPL_USE
			});
#else
			}
#endif
			// TEST //
			if (print_out_routine_objs && is_a_buffer)
			{
				uint max_layers = 0;
				double avr_layers_sum = 0;
				uint pix_cnt = 0;
				if (show_maxlayers_a_buffer_)
				{
					for (int i = 0; i < fb_size_cur.y; i++)
						for (int j = 0; j < fb_size_cur.x; j++)
						{
							vmbyte4 rgba = rgba_gpu_buf[j + i * buf_row_pitch];
							max_layers = max(max_layers, (uint)depth_gpu_buf[j + i * buf_row_pitch]);
							if (rgba.a > 0)
							{
								pix_cnt++;
								avr_layers_sum += (double)max_layers;
							}
						}
					cout << "******************" << endl;
					cout << "MAX LAYERS : " << max_layers << endl;
					cout << "AVR LAYERS : " << avr_layers_sum / pix_cnt << ", pixels : " << pix_cnt << endl;
					cout << "******************" << endl;
				}
			}
			// TEST //
			if (print_out_routine_objs && pixel_transmittance)
			{
				cout << "******************" << endl;
				cout << "pixel pos : " << pixel_pos.x << ", " << pixel_pos.y << endl;
				cout << "interval : " << tr_interval << endl;
				cout << "******************" << endl;

				dx11DeviceImmContext->CopyResource((ID3D11Buffer*)gres_fb_sys_deep_k.alloc_res_ptrs[DTYPE_RES],
					(ID3D11Buffer*)gres_fb_deep_k_buffer.alloc_res_ptrs[DTYPE_RES]);

				D3D11_MAPPED_SUBRESOURCE mappedResSysDeepK, mappedResSysPrefixLL;
				hr |= dx11DeviceImmContext->Map((ID3D11Buffer*)gres_fb_sys_deep_k.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, NULL, &mappedResSysDeepK);
				if (is_a_buffer)
				{
					dx11DeviceImmContext->CopyResource((ID3D11Buffer*)gres_fb_sys_prefix_sum_dxLL.alloc_res_ptrs[DTYPE_RES],
						(ID3D11Buffer*)gres_fb_prefix_sum_dxLL.alloc_res_ptrs[DTYPE_RES]);
					hr |= dx11DeviceImmContext->Map((ID3D11Buffer*)gres_fb_sys_prefix_sum_dxLL.alloc_res_ptrs[DTYPE_RES], 0, D3D11_MAP_READ, NULL, &mappedResSysPrefixLL);

					// to do //
					uint pixel_prefix = ((uint*)mappedResSysPrefixLL.pData)[max(pixel_pos.y * fb_size_cur.x + pixel_pos.x - 1, 0)];
					uint* deep_buffer = (uint*)mappedResSysDeepK.pData;
					uint num_layers = (uint)depth_gpu_buf[pixel_pos.x + pixel_pos.y * buf_row_pitch];
					cout << "num_layers : " << num_layers << endl;

					float* depth_array = new float[num_layers * 2];
					float* alpha_array = new float[num_layers];
					int* nIndex = new int[num_layers * 2];

					for (int i = 0; i < num_layers; i++)
					{
						nIndex[i] = i;
						depth_array[i] = *(float*)&deep_buffer[2 * (pixel_prefix + i) + 1];
						alpha_array[i] = (deep_buffer[2 * (pixel_prefix + i) + 0] >> 24) / 255.f;
					}
					for (int i = num_layers; i < num_layers * 2; i++)
					{
						nIndex[i] = i;
						depth_array[i] = FLT_MAX;
					}
					// bitonic sort
					uint N2 = 1 << (int)(ceil(log2(num_layers)));
					for (uint k = 2; k <= N2; k = 2 * k)
					{
						for (uint j = k >> 1; j > 0; j = j >> 1)
						{
							for (uint i = 0; i < N2; i++)
							{
								float di = depth_array[nIndex[i]];
								uint ixj = i ^ j;
								if ((ixj) > i)
								{
									float dixj = depth_array[nIndex[ixj]];
									if ((i & k) == 0 && di > dixj)
									{
										int temp = nIndex[i];
										nIndex[i] = nIndex[ixj];
										nIndex[ixj] = temp;
									}
									if ((i & k) != 0 && di < dixj)
									{
										int temp = nIndex[i];
										nIndex[i] = nIndex[ixj];
										nIndex[ixj] = temp;
									}
								}
							}
						}
					}

					string refdepthPath = "d:\\depth_ref.txt";
					ofstream refwriteFile_depth(refdepthPath.data());
					if (refwriteFile_depth.is_open()) {
						for (int i = 0; i < num_layers; i++)
							refwriteFile_depth << to_string(depth_array[nIndex[i]]) + "\n";
						float d_range = depth_array[nIndex[num_layers - 1]] - depth_array[nIndex[0]];
						depth_array[nIndex[num_layers]] = depth_array[nIndex[num_layers - 1]] + d_range * 0.05f;
						refwriteFile_depth << to_string(depth_array[nIndex[num_layers]]) + "\n";
						refwriteFile_depth.close();
					}

					string depthPath = "d:\\depth_LL.txt";
					ofstream writeFile_depth(depthPath.data());
					string filePath = "d:\\absorbance_LL.txt";
					ofstream writeFile_absorb(filePath.data());
					if (writeFile_depth.is_open() && writeFile_absorb.is_open()) {
						int hit_count = 0;
						float d_s = depth_array[nIndex[0]] - tr_startoffset;
						float depth_range = depth_array[nIndex[num_layers]] - d_s;
						int num_intervals = (int)(depth_range / tr_interval) + 2;

						float absorb = 0;
						for (int i = 0; i < num_intervals; i++)
						{
							float d_cur = d_s + (float)i * tr_interval;

							if (hit_count < num_layers)
							{
								while (d_cur > depth_array[nIndex[hit_count]])
								{
									absorb += (1 - absorb) * alpha_array[nIndex[hit_count]];
									hit_count++;
								}
							}
							writeFile_absorb << to_string(absorb) + "\n";
							writeFile_depth << to_string(d_cur) + "\n";
						}
						writeFile_depth.close();
						writeFile_absorb.close();
					}

					dx11DeviceImmContext->Unmap((ID3D11Buffer*)gres_fb_sys_prefix_sum_dxLL.alloc_res_ptrs[DTYPE_RES], 0);
					VMSAFE_DELETEARRAY(depth_array);
					VMSAFE_DELETEARRAY(alpha_array);
					VMSAFE_DELETEARRAY(nIndex);
				}
				else
				{
					string refdepthPath = "d:\\depth_ref.txt";
					ifstream openFile(refdepthPath.data());
					if (openFile.is_open())
					{
						vector<float> depth_array;
						string line;
						while (getline(openFile, line)) {
							depth_array.push_back(stof(line));
						}
						openFile.close();
						uint num_layers = (uint)depth_array.size();

						if (is_moment_oit)
						{
							// see Section 3.3 Warping Depth in Mement-based OIT
							auto __warp_depth = [&mot_nf](float z_depth_cs) -> float
							{
								const float z_min = (float)mot_nf.x;
								const float z_max = (float)mot_nf.y;
								return (log(max(min(z_depth_cs, z_max), z_min)) - log(z_min)) / (log(z_max) - log(z_min)) * 2.f - 1.f;
							};

							cout << "MOMENT OIT!!" << endl;
							cout << "warp min/max : " << mot_nf.x << ", " << mot_nf.y << endl;

							uint* mc_buffer = (uint*)mappedResSysDeepK.pData;

							string depthPath = "d:\\depth_MOT.txt";
							ofstream writeFile_depth(depthPath.data());
							string filePath = "d:\\absorbance_MOT.txt";
							ofstream writeFile_absorb(filePath.data());
							if (writeFile_depth.is_open() && writeFile_absorb.is_open()) {
								int hit_count = 0;
								float d_s = depth_array[0] - tr_startoffset;
								float depth_range = depth_array[num_layers - 1] - d_s;
								int num_intervals = (int)(depth_range / tr_interval) + 2;

								for (int i = 0; i < num_intervals; i++)
								{
									float d_cur = d_s + (float)i * tr_interval;
									float warp_d = __warp_depth(d_cur);
									float transmittance_at_depth, total_transmittance;

									moment_lib::resolveMoments(transmittance_at_depth, total_transmittance, warp_d, pixel_pos, fb_size_cur.x, 8 * 3, mc_buffer, num_moments, false, cb_moment);
									writeFile_absorb << to_string(1.f - transmittance_at_depth) + "\n";
									writeFile_depth << to_string(d_cur) + "\n";
								}
								writeFile_depth.close();
								writeFile_absorb.close();
							}
							//void resolveMoments(float& transmittance_at_depth, float& total_transmittance, const float depth, const vmint2 sv_pos,
							//	const int rt_width, const int buf_stride, const uint* moment_container_buf,
							//	const int num_moment, const bool is_trigonometric, const MomentOIT& mo)
						}
						else
						{
							cout << "KB OIT!!" << endl;

							uint* deep_buffer = (uint*)mappedResSysDeepK.pData;
							const int num_kb_layers = 8;
							float fr_alpha[8];
							float fr_depth[8];
							float fr_thick[8];
							int addr_base = (pixel_pos.x + pixel_pos.y * fb_size_cur.x) * num_kb_layers * 3;
							for (int i = 0; i < 8; i++)
							{
								fr_alpha[i] = (deep_buffer[addr_base + 3 * i + 0] >> 24) / 255.f;
								fr_depth[i] = *(float*)&deep_buffer[addr_base + 3 * i + 1];
								fr_thick[i] = *(float*)&deep_buffer[addr_base + 3 * i + 2];

								cout << to_string(i) << "==> " << fr_alpha[i] << ", " << fr_depth[i] << ", " << fr_thick[i] << endl;
							}

							string depthPath = "d:\\depth_KB.txt";
							ofstream writeFile_depth(depthPath.data());
							string filePath = "d:\\absorbance_KB.txt";
							ofstream writeFile_absorb(filePath.data());
							if (writeFile_depth.is_open() && writeFile_absorb.is_open()) {
								int hit_count = 0;
								float d_s = depth_array[0] - tr_startoffset;
								float depth_range = depth_array[num_layers - 1] - d_s;
								int num_intervals = (int)(depth_range / tr_interval) + 2;

								float absorb = 0;
								for (int i = 0; i < num_intervals; i++)
								{
									float d_cur = d_s + (float)i * tr_interval;
									if (hit_count < num_kb_layers)
									{
										while (d_cur > fr_depth[hit_count] - fr_thick[hit_count])
										{
											if (d_cur >= fr_depth[hit_count])
											{
												absorb += (1 - absorb) * fr_alpha[hit_count];
												hit_count++;
											}
											else
											{
												float sub_thick_f = d_cur - (fr_depth[hit_count] - fr_thick[hit_count]);
												float sub_alpha_f = fr_alpha[hit_count] * sub_thick_f / fr_thick[hit_count];
												absorb += (1 - absorb) * sub_alpha_f;

												fr_thick[hit_count] = fr_thick[hit_count] - sub_thick_f;
												fr_alpha[hit_count] = (fr_alpha[hit_count] - sub_alpha_f) / (1.f - sub_alpha_f);
											}
										}
									}
									writeFile_absorb << to_string(absorb) + "\n";
									writeFile_depth << to_string(d_cur) + "\n";
								}
								writeFile_depth.close();
								writeFile_absorb.close();
							}
						}
					}
				}
				dx11DeviceImmContext->Unmap((ID3D11Buffer*)gres_fb_sys_deep_k.alloc_res_ptrs[DTYPE_RES], 0);
			}


			//DisplayTimes("3-C");
			dx11DeviceImmContext->Unmap((ID3D11Texture2D*)gres_fb_sys_rgba.alloc_res_ptrs[DTYPE_RES], 0);
			dx11DeviceImmContext->Unmap((ID3D11Texture2D*)gres_fb_sys_depthcs.alloc_res_ptrs[DTYPE_RES], 0);
#pragma endregion
	}	// if (iCountDrawing == 0)
}

	dx11DeviceImmContext->ClearState();

	dx11DeviceImmContext->OMSetRenderTargets(1, &pdxRTVOld, pdxDSVOld);
	VMSAFE_RELEASE(pdxRTVOld);
	VMSAFE_RELEASE(pdxDSVOld);

	//DisplayTimes("4");

	iobj->SetDescriptor("vismtv_inbuilt_renderergpudx module");
	
	auto AsDouble = [](unsigned long long _v) -> double
	{
		double d_v;
		memcpy(&d_v, &_v, sizeof(double));
		return d_v;
	};
	iobj->RegisterCustomParameter("_double_LatestSrTime", AsDouble(vmhelpers::GetCurrentTimePack()));
	// Time Check
	double dRunTime = (lIntCntEnd.QuadPart - lIntCntStart.QuadPart) / (double)(lIntFreq.QuadPart);
	if (run_time_ptr)
		*run_time_ptr = dRunTime;

	//if (print_out_routine_objs)
	//{
	//	cout << "  ** rendering time (final)   : " << dRunTime * 1000 << " ms" << endl;
	//	cout << "  ** rendering fps (final)    : " << 1. / dRunTime << " fps" << endl;
	//}

	//((std::mutex*)HDx11GetMutexGpuCriticalPath())->unlock();

	return true;
}


