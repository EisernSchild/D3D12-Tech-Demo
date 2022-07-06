// D3D12 Tech Demo
// Copyright © 2022 by Denis Reischl
// 
// SPDX-License-Identifier: MIT

// fbm.hlsi:
// based on https://www.shadertoy.com/view/XdXBRH
//          https://www.shadertoy.com/view/Msf3WH
// Copyright © 2017 Inigo Quilez
// 
// SPDX-License-Identifier: MIT

#define PI 3.141592654f
#define OCTAVES 6

// random value
float2 hash(in float2 vX)
{
	return -1.0 + 2.0 * frac(sin(dot(vX, float2(12.9898, 78.233))) * 43758.5453);
}

// Simplex Noise
float noise_simplex(in float2 p)
{
	const float K1 = 0.366025404; // (sqrt(3)-1)/2;
	const float K2 = 0.211324865; // (3-sqrt(3))/6;

	float2  i = floor(p + (p.x + p.y) * K1);
	float2  a = p - i + (i.x + i.y) * K2;
	float m = step(a.y, a.x);
	float2  o = float2(m, 1.0 - m);
	float2  b = a - o + K2;
	float2  c = a - 1.0 + 2.0 * K2;
	float3  h = max(0.5 - float3(dot(a, a), dot(b, b), dot(c, c)), 0.0);
	float3  n = h * h * h * h * float3(dot(a, hash(i + 0.0)), dot(b, hash(i + o)), dot(c, hash(i + 1.0)));
	return dot(n, float3(70.f, 70.f, 70.f));
}

// Fractal Simplex Noise
float frac_noise_simplex(in float2 uv)
{
	uv *= 5.0;
	float2x2 m = float2x2(1.6, 1.2, -1.2, 1.6);
	float f = 0.5000 * noise_simplex(uv); uv = mul(uv, m);
	f += 0.2500 * noise_simplex(uv); uv = mul(uv, m);
	f += 0.1250 * noise_simplex(uv); uv = mul(uv, m);
	f += 0.0625 * noise_simplex(uv); uv = mul(uv, m);

	f = 0.5 + 0.5 * f;

	return f;
}

// return gradient noise (in x)
float _noised(in float2 afP)
{
	float2 vI = abs(floor(afP));
	float2 vF = abs(frac(afP));

	// we always use quintic interpolation ! (no terrain flaws !)
#if 1
	// quintic interpolation
	float2 vU = vF * vF * vF * (vF * (vF * 6.0 - 15.0) + 10.0);
#else
	// cubic interpolation
	float2 vU = vF * vF * (3.0 - 2.0 * vF);
#endif 

	float2 vGa = hash(vI + float2(0.0, 0.0));
	float2 vGb = hash(vI + float2(1.0, 0.0));
	float2 vGc = hash(vI + float2(0.0, 1.0));
	float2 vGd = hash(vI + float2(1.0, 1.0));

	float vVa = dot(vGa, vF - float2(0.0, 0.0));
	float vVb = dot(vGb, vF - float2(1.0, 0.0));
	float vVc = dot(vGc, vF - float2(0.0, 1.0));
	float vVd = dot(vGd, vF - float2(1.0, 1.0));

	return vVa + vU.x * (vVb - vVa) + vU.y * (vVc - vVa) + vU.x * vU.y * (vVa - vVb - vVc + vVd);
}

// return gradient noise (in x)
float noised(in float2 afP)
{
	float2 vI = floor(afP);
	float2 vF = frac(afP);

	// we always use quintic interpolation ! (no terrain flaws !)
#if 1
	// quintic interpolation
	float2 vU = vF * vF * vF * (vF * (vF * 6.0 - 15.0) + 10.0);
#else
	// cubic interpolation
	float2 vU = vF * vF * (3.0 - 2.0 * vF);
#endif    

	// get random values (3 derivate values build a quad)
	float2 avQuad[4] = { float2(0.0, 0.0), float2(1.0, 0.0), float2(0.0, 1.0), float2(1.0, 1.0) };
	float2 avG[4];
	float afV[4];
	for (int nI = 0; nI < 4; nI++)
	{
		avG[nI] = hash(vI + avQuad[nI]);
		afV[nI] = dot(avG[nI], vF - avQuad[nI]);
	}

	return afV[0] + vU.x * (afV[1] - afV[0]) + vU.y * (afV[2] - afV[0]) + vU.x * vU.y * (afV[0] - afV[1] - afV[2] + afV[3]);   // value
}

// return gradient noise (in x) and its derivatives (in yz)
float3 noised_der(in float2 afP)
{
	float2 vI = floor(afP);
	float2 vF = frac(afP);

	// we always use quintic interpolation ! (no terrain flaws !)
#if 1
	// quintic interpolation
	float2 vU = vF * vF * vF * (vF * (vF * 6.0 - 15.0) + 10.0);
	float2 vDu = 30.0 * vF * vF * (vF * (vF - 2.0) + 1.0);
#else
	// cubic interpolation
	float2 vU = vF * vF * (3.0 - 2.0 * vF);
	float2 vDu = 6.0 * vF * (1.0 - vF);
#endif    

	// get random values (3 derivate values build a quad)
	float2 avQuad[4] = { float2(0.0, 0.0), float2(1.0, 0.0), float2(0.0, 1.0), float2(1.0, 1.0) };
	float2 avG[4];
	float afV[4];
	for (int nI = 0; nI < 4; nI++)
	{
		avG[nI] = hash(vI + avQuad[nI]);
		afV[nI] = dot(avG[nI], vF - avQuad[nI]);
	}

	return float3(afV[0] + vU.x * (afV[1] - afV[0]) + vU.y * (afV[2] - afV[0]) + vU.x * vU.y * (afV[0] - afV[1] - afV[2] + afV[3]),   // value
		avG[0] + vU.x * (avG[1] - avG[0]) + vU.y * (avG[2] - avG[0]) + vU.x * vU.y * (avG[0] - avG[1] - avG[2] + avG[3]) +  // derivatives
		vDu * (vU.yx * (afV[0] - afV[1] - afV[2] + afV[3]) + float2(afV[1], afV[2]) - afV[0]));
}

// Fractional Brownian Motion
//
// vX - coordinates
// fH - the Hurst Exponent (H)
//
float3 fbm_der(in float2 vX, in float fH)
{
	// gain factor (G)
	float fG = exp2(-fH);
	// function, accumulation multiplier
	float fF = 1.0, fA = 1.0;
	// value
	float fT = 0.0;
	// derivatives value
	float2 vD = float2(0., 0.);
	// rotation matrix for derivatives
	float2x2 afRot = float2x2(cos(0.5), sin(0.5), -sin(0.5), cos(0.5));
	for (int nI = 0; nI < OCTAVES; nI++)
	{
		float3 vN = noised(fF * vX);
		fT += fA * vN.x; // accumulate values
		vD += fA * mul(vN.yz, afRot);  // accumulate derivatives
		fF *= 2.0;
		fA *= fG;
	}
	return float3(fT, vD);
}

// Fractional Brownian Motion
//
// vX - coordinates
// fH - the Hurst Exponent (H)
//
float fbm(in float2 vX, in float fH)
{
	// gain factor (G)
	float fG = exp2(-fH);
	// function, accumulation multiplier
	float fF = 1.0, fA = 1.0;
	// value
	float fT = 0.0;

	for (int nI = 0; nI < OCTAVES; nI++)
	{
		float fN = noised(fF * vX);
		fT += fA * fN; // accumulate values
		fF *= 2.0;
		fA *= fG;
	}

	return fT;
}

// heightmap normal calculation helper
//
void fbm_normal(in float2 vX, in float fH, out float fTerrain, out float3 vNormal, in float fSquareHalf = .05f)
{
	// get terrain square
	float fL = fbm(vX - float2(-fSquareHalf, .0), fH);
	float fR = fbm(vX - float2(fSquareHalf, .0), fH);
	float fU = fbm(vX - float2(.0, -fSquareHalf), fH);
	float fD = fbm(vX - float2(.0, fSquareHalf), fH);
	fTerrain = (fL + fR + fU + fD) * .25f;

	// calculate normal
	float3 vTangent = float3(2.0, fR - fL, 0.0);
	float3 vBitangent = float3(0.0, fD - fU, 2.0);
	vNormal = normalize(cross(vTangent, vBitangent));
}
