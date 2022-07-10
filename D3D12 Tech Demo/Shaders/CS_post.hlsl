// D3D12 Tech Demo
// Copyright © 2022 by Denis Reischl
// 
// SPDX-License-Identifier: MIT

#include"vrc.hlsli"

/// basic scene constant buffer
cbuffer sScene : register(b0)
{
	/// world-view-projection
	float4x4 sWVP;
	/// time (x - total, y - delta, z - fps total, w - fps)
	float4 sTime;
	/// viewport (x - topLeftX, y - topLeftY, z - width, w - height)
	float4 sViewport;
	/// mouse (x - x position, y - y position, z - buttons (uint), w - wheel (uint))
	float4 sMouse;
	/// hexagonal uv (x - x cartesian center, y - y cartesian center, z - u center, w - v center)
	float4 sHexUV;
	/// camera position (xyz - position)
	float4 sCamPos;
	/// camera velocity 3d vector (xyz - direction)
	float4 sCamVelo;
	/// inverse world-view-projection
	float4x4 sWVPrInv;
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

float vignette(float2 sUv, float2 sScreen)
{
	float2 sUvn = ((float2(float(sUv.x), float(sUv.y)) / sScreen.xy) - .5) * 2.;
	float fVgn1 = pow(smoothstep(0.0, .3, (sUvn.x + 1.) * (sUvn.y + 1.) * (sUvn.x - 1.) * (sUvn.y - 1.)), .5);
	float fVgn2 = 1. - pow(dot(float2(sUvn.x * .3, sUvn.y), sUvn), 3.);
	return lerp(fVgn1, fVgn2, .4) * .5 + 0.5;
}

[numthreads(N, 1, 1)]
void main(int3 sGroupTID : SV_GroupThreadID, int3 sDispatchTID : SV_DispatchThreadID)
{
	// float4 sPostCol = sTexIn[sDispatchTID.xy];

	// bevel
	// float4 sPostCol = float4(bevel(sTexIn, sDispatchTID.xy, float2(cos(1.3), sin(2.0)), 4.), 1.);

	// smoothing
	// float4 sPostCol = float4(smooth(sTexIn, sDispatchTID.xy, float2(2., 2.), 2.), 1.);

	// radial blur
	// float4 sPostCol = float4(blur_radial(sTexIn, sDispatchTID.xy, 10., 2., .00075, (sViewport.zw * .5) - sDispatchTID.xy), 1.);

	// vignette
	// sPostCol *= vignette(sDispatchTID.xy, sViewport.zw);

	// get a ray by screen position
	float4 sPostCol = sTexIn[sDispatchTID.xy];
	float3 vDirect;
	float3 vOrigin;
	transform_ray(sDispatchTID.xy, sViewport.zw, sCamPos, sWVPrInv, vOrigin, vDirect);

	// no texel set ? (= negative alpha)... draw background panorama
	if (sPostCol.a == 0.f)
	{
		// vDirect already normalized by transform method
		float fGradient = abs(vDirect.y - .05f);

		// ray goes up ?
		if (vDirect.y > 0.05f)
		{
			sPostCol = lerp(float4(.4f, .4f, 1.f, 1.f), float4(.2f, .2f, 1.f, 1.f), smoothstep(.0f, .3f, fGradient));
		}
		else
		{
			// ray goes down.. do volume ray cast - fractal brownian motion
			float fThit = 0.1f;
			PosNorm sAttr = (PosNorm)0;
			float3 sToEyeW = float3(1.f, 1.f, 0.f);

			// add hex grid rim distance to ray... adjust according to mountain 
			// width to avoid vOrigin within terrain
			const float fMountMaxWidth = 15.f;
			const float fRimDist = 110.8512516844081f - fMountMaxWidth;

			// if camera y position is heigher than rim set this as rim
			vOrigin += vDirect * max(fRimDist, vOrigin.y);

			if (vrc_fbm(
				vOrigin,
				normalize(vDirect),
				fThit,
				sAttr,
				50))
			{
				float fFbmScale = .05f, fFbmScaleSimplex = .5f;
				const float2 afFbmScale = float2(.05f, 10.f);
				float2 sUV = sAttr.vPosition.xz;

				// back scale
				float fTerrain = sAttr.vPosition.y * .1f;

				// get base height color
				float fHeight = max((fTerrain + 1.) * .5f, 0.f);
				float3 sDiffuse = lerp(float3(.65, .6, .4), sDiffuseAlbedo.xyz, smoothstep(0.5f, 0.7f, fHeight));

				// draw grassland
				float fGrass = frac_noise_simplex(sUV * fFbmScaleSimplex * 2.f);
				sDiffuse = lerp(lerp(float3(.5f, .3, .2), float3(.3f, .8, .4), max(1.0f - fHeight * 1.2f, fGrass)), sDiffuse, max(.7f, min(fHeight * 1.7f, 1.f)));

				// to camera vector, ambient light
				sToEyeW = sCamPos.xyz - sAttr.vPosition;
				float3 sToEyeWN = normalize(sToEyeW);
				float4 sAmbient = sAmbientLight * float4(sDiffuse, 1.f);
				float fNdotL = max(dot(sLightVec, sAttr.vNormal), 0.1f);
				float3 sStr = sStrength * fNdotL;

				// do phong
				sPostCol = sAmbient + float4(BlinnPhong(sDiffuse, sStr, sLightVec, sAttr.vNormal, sToEyeWN, smoothstep(.5, .55, abs(fHeight))), 1.f);
			}
			else // terrain simple gradient
				sPostCol = lerp(float4(.4f, .4f, 1.f, 1.f), float4(.8f, .9f, 1.f, 1.f), smoothstep(.0, .05f, fGradient));

			float fDepth = length(sToEyeW);
			float fFog = fDepth * .0052f;
			float4 fFogColor = float4(.8f, .9f, 1.f, 1.f) * min(fFog, 1.f);
			sPostCol = max(sPostCol, fFogColor);
		}
	}
	
	sPostCol *= vignette(sDispatchTID.xy, sViewport.zw);

	sTexOut[sDispatchTID.xy] = sPostCol;
}