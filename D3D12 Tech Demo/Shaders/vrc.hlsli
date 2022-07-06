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

// Volume Ray Casting - Fractal Brownian Motion
bool vrc_fbm(
	in float3 vOri,
	in float3 vDir,
	out float fThit,
	out PosNorm sAttr,
	in const float2 afFbmScale = float2(.05f, 10.f),
	in const float fH = 1.f,
	in const int nMaxRaySteps = 40,
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
