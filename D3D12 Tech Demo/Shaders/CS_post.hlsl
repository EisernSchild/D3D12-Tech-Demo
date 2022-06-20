// D3D12 Tech Demo
// Copyright © 2022 by Denis Reischl
// 
// SPDX-License-Identifier: MIT

/// basic scene constant buffer
cbuffer sScene : register(b0)
{
	/// world-view-projection
	float4x4 sWVP;
	/// time (x - total, y - delta, z - fps total, w - fps)
	float4 sTime;
	/// viewport (x - topLeftX, y - topLeftY, z - width, w - height)
	float4 sViewport;
	/// mouse (x - x position, y - y position, z - buttons (unsigned), w - wheel (unsigned))
	float4 sMouse;
};

Texture2D sTexIn            : register(t0);
RWTexture2D<float4> sTexOut : register(u0);

/// number of threads X const
#define N 256
/// grayscale
#define to_grayscale(rgb) (rgb.r * 0.3 + rgb.g * 0.6 + rgb.b * 0.1)

/// simple grayscale bevel
float3 bevel(Texture2D sTex, float2 sUv, float2 sD, float fP)
{
	float3 sA1 = sTex[sUv - sD].xyz;
	float3 sB2 = sTex[sUv + sD].xyz;
	return float3(.5, .5, .5) + to_grayscale(float3(sA1 * fP - sB2 * fP));
}

/// smoothing
float3 smooth(Texture2D sTex, float2 sUv, float2 sKernel, float fStrength)
{
	float3 sAvarage = float3(0., 0., 0.);
	float2 sHalfK = sKernel * 0.5;
	for (float fX = 0.0 - sHalfK.x; fX < sHalfK.x; fX += 1.0)
	{
		for (float fY = 0.0 - sHalfK.y; fY < sHalfK.y; fY += 1.0)
			sAvarage += sTex[sUv + float2(fX, fY) * fStrength].xyz;
	}
	return max(sTex[sUv].xyz, sAvarage / (sKernel.x * sKernel.y));
}

// radial (to center) blur
float3 blur_radial(Texture2D sTex, float2 sUv, int nSc, float fStrength, float fP, float2 sC)
{
	int nHalfSc = nSc / 2;
	float3 sAvarage = float3(0., 0., 0.);
	int nScT = nSc;

	// calculate avarage color from radial distant pixels
	for (int x = 0; x < nSc; x++)
	{
		float fV = float(x - nHalfSc) * fP;
		float2 sUvR = sUv + sC * fV;

		// within bounds ?
		if ((sUvR.x > 0.) && (sUvR.y > 0.) &&
			(sUvR.x < sViewport.z) && (sUvR.y < sViewport.w))
			sAvarage += sTex[sUvR].xyz;
		else
			nScT--;
	}
	sAvarage *= 1.0 / float(nScT + 1);

	float fDist = distance(sC, sUv);
	return lerp(sTex[sUv].xyz, sAvarage, clamp(fDist * fStrength, 0.0, 1.0));
}

[numthreads(N, 1, 1)]
void main(int3 sGroupTID : SV_GroupThreadID, int3 sDispatchTID : SV_DispatchThreadID)
{
	// float4 sPostCol = sTexIn[sDispatchTID.xy];

	// bevel
	// float4 sPostCol = float4(bevel(sTexIn, sDispatchTID.xy, float2(cos(1.3), sin(2.0)), 4.), 1.);

	// smoothing
	float4 sPostCol = float4(smooth(sTexIn, sDispatchTID.xy, float2(2., 2.), 2.), 1.);

	// radial blur
	// float4 sPostCol = float4(blur_radial(sTexIn, sDispatchTID.xy, 10., 2., .00075, (sViewport.zw * .5) - sDispatchTID.xy), 1.);

	// vignette
	float2 sUV = ((float2(float(sDispatchTID.x), float(sDispatchTID.y)) / sViewport.zw) - .5) * 2.;
	float fVgn1 = pow(smoothstep(0.0, .3, (sUV.x + 1.) * (sUV.y + 1.) * (sUV.x - 1.) * (sUV.y - 1.)), .5);
	float fVgn2 = 1. - pow(dot(float2(sUV.x * .3, sUV.y), sUV), 3.);
	sPostCol *= lerp(fVgn1, fVgn2, .4) * .5 + 0.5;

	sTexOut[sDispatchTID.xy] = sPostCol;
}