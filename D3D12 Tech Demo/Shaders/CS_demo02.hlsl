// D3D12 Tech Demo
// Copyright © 2022 by Denis Reischl
// 
// SPDX-License-Identifier: MIT

#pragma warning( disable : 4714 )

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

// ray hit attribute
struct PosNormIx
{
	float3 vPosition;
	float3 vNormal;
	float2 vIndex;
};

// hash 2 to 1
float hash12(float2 vUv)
{
	float3 vP3 = frac(float3(vUv.xyx) * .1031);
	vP3 += dot(vP3, vP3.yzx + 33.33);
	return frac((vP3.x + vP3.y) * vP3.z);
}

// simple heightmap function
float heightmap(float2 vUv)
{
	return hash12(vUv) * 6.f + hash12(vUv * .2) * sin(sTime.x);
}

// hexagonal volume ray cast
bool vrc_hex(in float3 vOri, 
	in float3 vDir, 
	out float fThit, 
	out PosNormIx sAttr,
	in uint uMaxSteps = 64,
	in float fTMin = 0.f,
	in float fTMax = 50.f,
	in float fStepAdjust = .005f)
{
	float3 vPos = vOri;
	fThit = 0.f;
	float2 vPrev = HexTriangleF(vPos.xz);
	float fPrev = heightmap(vPrev);
	float fAlpha = 0.f;

	// march through the space
	uint uI = uint(0);
	while ((uI++ < uMaxSteps) && (fThit < fTMax))
	{
		// go through empty space
		//if (fDist > fStepAdjust)
		//{
		//    fThit += fDist - fStepAdjust;
		//    vPos = vOri + fThit * vDir;
		//}

		// perform step
		float2 vStep = iHexNextTriangle(vPos.xz, vDir.xz);
		float fStep = (length(vStep - vPos.xz) / length(vDir.xz)) + fStepAdjust;
		fThit += fStep;
		vPos = vOri + fThit * vDir;
		float2 vNext = HexTriangleF(vPos.xz);
		float fNext = heightmap(vNext);

		// top intersection ?
		if (vPos.y - fPrev <= 0.f)
		{
			// get back to barycentric coords
			fThit -= (abs(vPos.y - fPrev) / abs(vDir.y));
			vPos = vOri + fThit * vDir;

			// set attributes
			sAttr.vIndex = vPrev;
			sAttr.vPosition = vPos;
			sAttr.vNormal = float3(0., 1., 0.);

			return true;
		}
		else
			// lateral intersection
			if (vPos.y - fNext <= 0.f)
			{
				// set hit attributes
				sAttr.vIndex = vNext;
				sAttr.vPosition = vPos;

				float2 vCnt = HexCenterF(vNext);
				float2 vLc = float2(vPos.x - vCnt.x, vPos.z - vCnt.y);
				float fA = atan2(vLc.y, vLc.x);

				// set normal by center->intersection angle
				sAttr.vNormal = (fmod(vNext.x, 2.f) >= 1.) ?
					((fA >= radians(-150.)) && (fA <= radians(-30.))) ?
					float3(0., 0., -1) :
					normalize(float3(sign(vLc.x), 0., 2. / 3.)) :
					((fA <= radians(150.)) && (fA >= radians(30.))) ?
					float3(0., 0., 1.) :
					normalize(float3(sign(vLc.x), 0., -2. / 3.));

				return true;
			}

		fPrev = fNext;
		vPrev = vNext;

	}
	return false;
}

// lighting 
float3 SceneLighting(in float3 vPos, in float3 vRayDir, in float3 vLitPos,
	in float3 vNorm,
	in float3 cMaterial,
	in bool bTranslucent,
	in float fAmbient,
	in float fSpecularPow,
	in float fSpecularAdj,
	in float3 cLight,
	in float3 vLight)
{
	// get distance, reflection
	float fDist = length(vLitPos - vPos);
	float3 vRef = normalize(reflect(vRayDir, vNorm));

	// calculate fresnel, specular factors
	float fFresnel = max(dot(vNorm, -vRayDir), 0.0);
	fFresnel = pow(fFresnel, .3) * 1.1;
	float fSpecular = max(dot(vRef, vLight), 0.0);

	// do lighting.. inverse normal for translucent primitives
	float3 cLit = cMaterial * 0.5f;
	cLit = lerp(cLit, cMaterial * max(dot(vNorm, vLight), fAmbient), min(fFresnel, 1.0));
	if (bTranslucent)
		cLit = lerp(cLit, cMaterial * max(dot(-vNorm, vLight), fAmbient), .2f);
	cLit += cLight * pow(fSpecular, fSpecularPow) * fSpecularAdj;
	cLit = clamp(cLit, 0.f, 1.f);

	return cLit;
}

// get position on ground plane
float3 GroundLitPos(in float3 vPos, in float3 vRayDir)
{
	// get lit position
	float fD = -vPos.y / vRayDir.y;
	return float3(vPos.x + vRayDir.x * fD, 0.0, vPos.z + vRayDir.z * fD);
}

// get horizon by ray direction
float4 Horizon(in float3 vDir)
{
	if (vDir.y > 0.f)
		return min(float4(lerp(float3(.8, .8, 1.), float3(0., 0., 0.), exp2(-1. / vDir.y) * float3(3.f, 1.f, 0.3f)), 1.), float4(1., 1., 1., 1.));
	else
		return min(float4(lerp(float3(.8, .8, 1.), float3(0., 0., 0.), -vDir.y * 2.), 1.), float4(1., 1., 1., 1.));
}

// trace the ray
bool Trace_(in float3 vOri, in float3 vDir, out float fThit, out PosNormIx sAttr, out float4 cOut)
{	
	// trace the ray
	bool bRet = false;
	if (vrc_hex(vOri, vDir, fThit, sAttr))
	{
		float2 vCnt = HexCenterF(sAttr.vIndex);
		float2 vLc = vCnt - sAttr.vPosition.xz;
		cOut = lerp(float4(.6, .7, .9, 1.), float4(.9, .8, .7, 1.), hash12(sAttr.vIndex * .1));

		// fake occlusion
		float fH = heightmap(sAttr.vIndex);
		cOut *= 1. - length(vLc) * (1. - (sAttr.vPosition.y / fH));

		bRet = true;
	}
	else
		cOut = Horizon(vDir);
	return bRet;
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
	// get a ray by screen position
	float4 cOut = float4(0., 0., 0., 0.);
	float3 vDir;
	float3 vOri;
	transform_ray(sDispatchTID.xy, sViewport.zw, sCamPos, sWVPrInv, vOri, vDir);

	// do raytracing
	float fThit = 0.f;
	PosNormIx sAttr;
	bool bHit = Trace_(vOri, vDir, fThit, sAttr, cOut);

	if (bHit)
	{
		bool bShadow = false;

		// get shadow
		float fThisSh = 0.f;
		PosNormIx sAttrSh;
		float4 cRef;
		float3 vDirSh = normalize(float3(-.4f, .2f, -.3f));

		if (Trace_(sAttr.vPosition + sAttr.vNormal * .01f, vDirSh, fThisSh, sAttrSh, cRef))
		{
			bShadow = true;
			cOut *= .9f;
		}

		// get reflection ray
		cRef = float4(1., 1., 1., 1.);
		float3 vRef = normalize(reflect(vDir, sAttr.vNormal));
		float fThisRf = 0.f;
		PosNormIx sAttrRf;
		Trace_(sAttr.vPosition + sAttr.vNormal * .01f, vRef, fThisRf, sAttrRf, cRef);
		cOut = lerp(cOut, cRef * .2f, bShadow ? 0.5f : 0.3f);

		// do actual lighting
		cOut = float4(SceneLighting(
			sCamPos.xyz,
			vDir,
			sAttr.vPosition,
			sAttr.vNormal,
			cOut.xyz,
			false,
			.8f,
			220.f,
			1.f,
			bShadow ? float3(.0f, .0f, .0f) : float3(.9f, .8f, .9f),
			normalize(float3(-.4f, .2f, -.3f))
		), 1.f);

		// fade out...
		float fTMax = 50.f;
		if (fThit > fTMax * .8)
		{
			cOut = lerp(cOut, Horizon(vDir), clamp((fThit - fTMax * .8) * .3, 0., 1.));

		}
	}

	/*
	// vDir already normalized by transform method
	float fGradient = abs(vDir.y - .05f);

	// ray goes up ?
	if (vDir.y > 0.05f)
	{
		cOut = lerp(float4(.4f, .4f, 1.f, 1.f), float4(.2f, .2f, 1.f, 1.f), smoothstep(.0f, .3f, fGradient));
	}
	else
	{
		// terrain simple gradient
		cOut = lerp(float4(.4f, .4f, 1.f, 1.f), float4(.8f, .9f, 1.f, 1.f), smoothstep(.0, .05f, fGradient));
	}
	*/

	// add vignette
	cOut *= vignette(sDispatchTID.xy, sViewport.zw);

	sTexOut[sDispatchTID.xy] = cOut;
}