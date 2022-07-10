// D3D12 Tech Demo
// Copyright © 2022 by Denis Reischl
// 
// SPDX-License-Identifier: MIT

#include"fbm.hlsli"

struct PosNorm
{
	float3 vPosition;
	float3 vNormal;
};

// transform a ray based on screen position, camera position and inverse wvp matrix 
inline void transform_ray(in uint2 sIndex, in float2 sScreenSz, in float4 vCamPos, in float4x4 sWVPrInv, 
	out float3 vOrigin, out float3 vDirection)
{
	// center in the middle of the pixel, get screen position
	float2 vXy = sIndex.xy + 0.5f;
	float2 vUv = vXy / sScreenSz.xy * 2.0 - 1.0;

	// invert y 
	vUv.y = -vUv.y;

	// unproject by inverse wvp
	float4 vWorld = mul(float4(vUv, 0, 1), sWVPrInv);

	vWorld.xyz /= vWorld.w;
	vOrigin = vCamPos.xyz;
	vDirection = normalize(vWorld.xyz - vOrigin);
}

// Volume Ray Casting - Fractal Brownian Motion
bool vrc_fbm(
	in float3 vOri,
	in float3 vDir,
	out float fThit,
	out PosNorm sAttr,
	in const int nMaxRaySteps = 40,
	in const float2 afFbmScale = float2(.05f, 10.f),
	in const float fH = 1.f,
	in const float fStepAdjust = .6f)
{
	// raymarch 
	float3 vStep = vDir;
	float fThitPrev = fThit;
	for (int n = 0; n < nMaxRaySteps; n++)
	{
		// get terrain height, this should be precomputed in heighmap
		float2 vX = vOri.xz * afFbmScale.x;
		float fTerrainY = fbm(vX, fH) * afFbmScale.y;

		fThitPrev = fThit;
		fThit = vOri.y - fTerrainY;
		if (fThit < 0.1f)
		{
			// adjust hit position
			if (fThit < 0.f)
			{
				fThit = abs(fThit);
				vOri -= vStep * (fThit / (fThit + fThitPrev));
				fThit = 0.01f;
			}

			// calculate normal
			float3 vNormal;
			fbm_normal(vX, fH, vOri.y, sAttr.vNormal);
			vOri.y *= afFbmScale.y;

			// set position
			sAttr.vPosition = vOri;
			return true;
		}

		// raymarch step
		vStep = vDir * abs(fThit) * fStepAdjust;
		vOri += vStep;
	}

	return false;
}
