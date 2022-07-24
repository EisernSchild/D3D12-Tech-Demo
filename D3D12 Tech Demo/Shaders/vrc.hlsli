// D3D12 Tech Demo
// Copyright © 2022 by Denis Reischl
// 
// SPDX-License-Identifier: MIT

// based on code of following sources :
// Copyright (c) Microsoft
// Copyright (c) 2013 Inigo Quilez
// 
// SPDX-License-Identifier: MIT

#include"fbm.hlsli"

#ifdef _DXR
// all primitive types enumeration
enum struct Primitive
{
	Cylinder,
	CylinderBent,
};
#endif

struct PosNorm
{
	float3 vPosition;
	float3 vNormal;
	float2 vColor;
};

// Math Library

// Orthographic projection : ba = (b.an)an
float2 ortho_proj(float2 vA, float2 vB)
{
	float2 vAn = normalize(vA);
	return dot(vB, vAn) * vAn;
}

// rotate 2D
float2 rotate(float2 vV, float fA)
{
	float fS = sin(fA);
	float fC = cos(fA);
	float2x2 mR = float2x2(fC, -fS, fS, fC);
	return mul(vV, mR);
}

// Hex Grid Library

/// Cartesian to hex coordinates
float2 HexUV(float2 vXy)
{
	// hex coords       (u, v) = (          .5 * x + .5 * y,        y ) 
	// hex coord scaled (u, v) = ((sqrt(3.f) * x + y) / 3.f, y / 1.5f )
	return float2((sqrt(3.f) * vXy.x + vXy.y) / 3.f, vXy.y / 1.5f);
}

/// Hex to cartesian coordinates
float2 HexXY(float2 vUv)
{
	// get cartesian coords
	return float2((vUv.x * 3.f - vUv.y * 1.5f) / sqrt(3.f), vUv.y * 1.5f);
}

// provide hex grid
float HexGrid(float2 vPt)
{
	const float2 vNext = float2(1.5f / sqrt(3.f), 1.5f);
	const float fTD = length(vNext);

	// get approx. hexagonal center coords
	float2 vUvC = round(HexUV(vPt));

	// get approx. cartesian hex center
	float2 vPtC = HexXY(vUvC);

	// get local coords absolut, adjust x
	float2 vPtLc = abs(vPt - vPtC);
	if (vPtLc.x > (fTD * .5)) vPtLc.x = fTD - vPtLc.x;

	// project point on constant tile vector
	float2 vPtN = ortho_proj(vNext, vPtLc);

	// get distance, adjust again
	float fD = max(vPtLc.x, length(vPtN));
	if (fD > (fTD * .5)) fD = fTD - fD;

	return fD;
}

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
	in const uint uMax = 40,
	in const float2 afFbmScale = float2(.05f, 10.f),
	in const float fH = 1.f,
	in const float fStepAdjust = .7f,
	in const float fTMin = 0.f,
	in const float fTMax = 1000.f)
{
	const float fThreshold = 0.00001;
	float fT = fTMin;
	float fStep = vOri.y - (fbm(vOri.xz * afFbmScale.x, fH) * afFbmScale.y);
	float3 vPos = vOri;
	
	// march through the AABB
	uint uI = 0;
	while (uI++ < uMax && fT <= fTMax)
	{
		vPos += fStep * vDir;
		float fDist = vPos.y - (fbm(vPos.xz * afFbmScale.x, fH) * afFbmScale.y);

		// intersection ?
		if (fDist <= fThreshold * fT)
		{
			// is valid ?
			if (fT < fTMax)
			{
				// adjust hit position (last step)
				fStep = fStepAdjust * fDist;
				vPos += fStep * vDir;
				fT += fStep;

				fThit = fT;

				// calculate normal
				float3 vNormal;
				fbm_normal(vPos.xz * afFbmScale.x, fH, vPos.y, sAttr.vNormal);
				vPos.y *= afFbmScale.y;

				// set position
				sAttr.vPosition = vPos;
				sAttr.vColor = (float2)0;
				return true;
			}
		}

		// raymarch step
		fStep = fStepAdjust * fDist;
		fT += fStep;
	}
	return false;
}

#ifdef _DXR

// Infinite Cylinder - exact
float sdCylinder(float3 vPos, float3 vC)
{
	return length(vPos.xy - vC.xy) - vC.z;
}

// 2D ellipse
float sdEllipse(float2 vPos, float2 vAB)
{
	float2 pAbs = abs(vPos);
	float2 vABi = 1.0 / vAB;
	float2 vAB2 = vAB * vAB;
	float2 vVe = vABi * float2(vAB2.x - vAB2.y, vAB2.y - vAB2.x);

	float2 vT = float2(0.70710678118654752, 0.70710678118654752);
	for (int nI = 0; nI < 3; nI++) {
		float2 vV = vVe * vT * vT * vT;
		float2 vU = normalize(pAbs - vV) * length(vT * vAB - vV);
		float2 vW = vABi * (vV + vU);
		vT = normalize(clamp(vW, 0.0, 1.0));
	}

	float2 vNextAbs = vT * vAB;
	float fDis = length(pAbs - vNextAbs);
	return dot(pAbs, pAbs) < dot(vNextAbs, vNextAbs) ? -fDis : fDis;
}

// bend a cylinder based on tangent distance - bound
float sdCylinderBent(float3 vPos, float2 vC, float fR, float fD)
{
	// https://mathworld.wolfram.com/CylindricalSegment.html
	//
	float fA = 2. * fR + sqrt(pow(2. * fR, 2.) + pow(fD, 2.));
	float fB = 2. * fR;

	return sdEllipse(vPos.xy - vC.xy, float2(fB, fA));
}

// ellipsoid centered at the origin with radii fRad - intersection
float isEllipsoid(in float3 vOri, in float3 vDir, in float3 vCen, in float3 fRad)
{
	float3 vOc = vOri - vCen;

	float3 vOcn = vOc / fRad;
	float3 vRdn = vDir / fRad;

	float fA = dot(vRdn, vRdn);
	float fB = dot(vOcn, vRdn);
	float fC = dot(vOcn, vOcn);
	float fH = fB * fB - fA * (fC - 1.0);
	if (fH < 0.0) return -1.0;
	return (-fB - sqrt(fH)) / fA;
}

// normal function for intersected ellipsoid
float3 nrEllipsoid(in float3 vPos, in float3 vCen, in float3 fRad)
{
	return normalize((vPos - vCen) / (fRad * fRad));
}

// circular repitition
float sdCircularEllipsoids(in float3 vOri, in float3 vDir, in float fRad, in float fSpc, out float3 vNormal, out float2 vIdH, in float fTime)
{
	// make grid
	float2 fId0 = round(vOri.xz / fSpc);

	// snap to circle
	if (dot(fId0, fId0) > fRad * fRad) fId0 = round(normalize(fId0) * fRad);

	// TODO !! this is a signed distance method, for intersected primitives
	// loop through the radius accordingly

	// scan neighbors
	float fThit = -1.f;
	for (int nJ = -16; nJ <= 16; nJ++)
		for (int nI = -16; nI <= 16; nI++)
		{
			// is in radius ?
			float2 vId = fId0 + float2(nI, nJ);
			if (dot(vId, vId) <= fRad * fRad)
			{
				// shift origin
				float3 vQ = vOri - fSpc * float3(vId.x, 0.f, vId.y);

				// rotate and lift ellipsoid
				float fC = sqrt(dot(vId, vId));
				float3 vRad = (fC < (fRad * .33)) ? float3(1.f, 1.5f, 1.f) : (fC < (fRad * .66)) ? float3(1.5f, 1.f, 1.f) : float3(1.f, 1.f, 1.5f);
				vRad *= fSpc * .15f;
				float3 vPosE = float3(12.f, 1.5f + fC * sin(fTime * fC) * .2f, -32.f);

				// ellipsoid intersection ?
				float fH1 = isEllipsoid(vQ, vDir, vPosE, vRad);
				if (fH1 >= 0.f)
				{
					// set nearest hit
					fThit = (fThit >= 0.f) ? min(fThit, fH1) : fH1;

					// nearest == current ?
					if (fThit == fH1)
					{
						float3 vPosition = vQ + fThit * vDir;
						vNormal = nrEllipsoid(vPosition, vPosE, vRad);
						vIdH = vId;
					}
				}
			}
		}
	return fThit;
}

// test function
float fc(float fX)
{
	return sin(fX * .8) * .9 + 3. +cos(fX * .3);
	// return sin(fX * 2.) * .4;
	// return sin(fX * 3.) * .4  + 2.5 + cos(fX * .6);
}

// blanket function to wrap a any heightmap by tangent
float4 modBlanket(float fX, float fR)
{
	// get height by function
	float fH = fc(fX);

	// get function point by x axis
	float2 vA = float2(fX, fH);

	// get tangent normalized
	float2 vTng = normalize(float2(fX + .1, fc(fX + .1)) - float2(fX - .1, fc(fX - .1)));

	// get normal normalized (rotate tangent 90 deg counter clockwise)
	float2 vNrm = float2(-vTng.y, vTng.x);

	// add radius to function point
	float2 vB = vA + vNrm * fR;

	// calculate length of tangent (-tan(asin(normal x))
	float fTngL = fR * -tan(asin(vNrm.x));

	return float4(fH, abs(fTngL), vNrm);
}

// imports all signed distance methods
float sdf(in float3 vPos, in Primitive ePrimitive, in float fTime)
{
	switch (ePrimitive)
	{
	case Primitive::Cylinder:
	{
		// align on x axist (xyz -> zyx)
		float fD = vPos.x + fTime;
		return sdCylinder(vPos.zyx, float3(0.f, 1.f, 0.2f));
	}
	case Primitive::CylinderBent:
	{
		// align on x axist (xyz -> zyx)
		float fD = vPos.x;// +fTime;
		float fR = 0.2;
		float4 vB = modBlanket(fD, fR);

		return sdCylinderBent(vPos.zyx, float2(0., vB.x), fR, vB.y);
	}
	default:break;
	}
	return 0.f;
}

// get normal for hit
float3 sdCalculateNormal(in float3 vPos, in Primitive ePrimitive, in float fTime)
{
	if (0)
	{
		switch (ePrimitive)
		{
		case Primitive::Cylinder:
			return normalize(vPos - float3(vPos.x, 1.f, 0.f));
		case Primitive::CylinderBent:
		{
			float fD = vPos.x + fTime;
			float fR = 0.2;
			float4 vB = modBlanket(fD, fR);

			float3 vR = normalize(vPos - float3(vPos.x, vB.x, 0.f));
			vR.x = 1.2f * vB.z * vR.y;
			return normalize(vR);

		}
		default:break;
		}
	}
	else
	{
		float2 vE = float2(1.0, -1.0) * 0.5773 * 0.0001;
		return normalize(
			vE.xyy * sdf(vPos + vE.xyy, ePrimitive, fTime) +
			vE.yyx * sdf(vPos + vE.yyx, ePrimitive, fTime) +
			vE.yxy * sdf(vPos + vE.yxy, ePrimitive, fTime) +
			vE.xxx * sdf(vPos + vE.xxx, ePrimitive, fTime));
	}
}

// Volume Ray Casting
bool vrc(in float3 vOri, in float3 vDir, in Primitive ePrimitive, out float fThit, out PosNorm sAttr,
	in float fTime = 0.f, in const uint uMax = 128, in const float fStepAdjust = 1.f)
{
	const float fThreshold = 0.001;
	float fT = RayTMin();
	float fStep = sdf(vOri, ePrimitive, fTime);
	float3 vPos = vOri;

	// march through the AABB
	uint uI = 0;
	while (uI++ < uMax && fT <= RayTCurrent())
	{
		vPos += fStep * vDir;
		float fDist = sdf(vPos, ePrimitive, fTime);

		// intersection ?
		if (fDist <= fThreshold * fT)
		{
			float3 vNormal = sdCalculateNormal(vPos, ePrimitive, fTime);
			if (true) // ValidHit(vPos, vDir, fT, vNormal))
			{
				fThit = fT;
				sAttr.vPosition = vPos;
				sAttr.vNormal = vNormal;
				return true;
			}
		}

		// raymarch step
		fStep = fStepAdjust * fDist;
		fT += fStep;
	}
	return false;
}


#endif
