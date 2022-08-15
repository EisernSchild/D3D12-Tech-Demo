// D3D12 Tech Demo
// Copyright © 2022 by Denis Reischl
// 
// SPDX-License-Identifier: MIT

// based on code with following rights :
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

// cartesian to hex triangle index
int2 HexTriangle(float2 vXy)
{
	// get hex uv global + local
	float2 vHUv = HexUV(vXy);
	float2 vHUvL = fmod(vHUv, 1.);
	int nIx = int(floor(vHUv.x)) * 2;
	nIx += (vHUvL.x > vHUvL.y) ? 0 : 1;
	return int2(nIx, int(floor(vHUv.y)));
}

// cartesian to hex triangle index (float)
float2 HexTriangleF(float2 vXy)
{
	// get hex uv global + local
	float2 vHUv = HexUV(vXy);
	float2 vHUvL = fmod(vHUv, 1.);
	float fIx = floor(vHUv.x) * 2.;
	fIx += (vHUvL.x > vHUvL.y) ? 1. : 0.;
	return float2(fIx, floor(vHUv.y));
}

// hex triangle index to cartesian triangle center
float2 HexCenterF(float2 vIx)
{
	float2 vHUv = floor(vIx * float2(.5, 1.));
	vHUv += (fmod(vIx.x, 2.) < 1.) ? float2(.33333, .66666) : float2(.66666, .33333);
	float2 vXy = HexXY(vHUv);
	return vXy;
}

// provides the intersection point of the next triangle in a direction
float2 iHexNextTriangle(float2 vXy, float2 vDir)
{
	// get hex uv global + local
	float2 vHUv = HexUV(vXy);
	float2 vHUvL = frac(vHUv);

	// get direction in hex space, normalized
	float2 vHDir = normalize(HexUV(vDir));

	// get angles
	float fA = atan2(.0 - vHUvL.y, .0 - vHUvL.x);
	float fB = atan2(1. - vHUvL.y, 1. - vHUvL.x);
	float fC = atan2(1. - vHUvL.y, .0 - vHUvL.x);
	float fD = atan2(.0 - vHUvL.y, 1. - vHUvL.x);
	float fE = atan2(vHDir.y, vHDir.x);

	// get next intersection
	if (vHUvL.x < vHUvL.y)
	{
		if ((fA < fE) && (fB >= fE))
		{
			float2 vA = ortho_proj(float2(1.f, 1.f), vHUvL);
			float2 vB = ortho_proj(vA - vHUvL, vHDir);
			vHUv += vHDir * (length(vHUvL - vA) / length(vB));
		}
		else if ((fB < fE) && (fC >= fE))
			vHUv += vHDir * abs((1.f - vHUvL.y) / vHDir.y);
		else
			vHUv += vHDir * abs(vHUvL.x / vHDir.x);
	}
	else
	{
		if ((fA < fE) && (fD >= fE))
			vHUv += vHDir * abs(vHUvL.y / vHDir.y);
		else if ((fD < fE) && (fB >= fE))
			vHUv += vHDir * abs((1.f - vHUvL.x) / vHDir.x);
		else
		{
			float2 vA = ortho_proj(float2(1.f, 1.f), vHUvL);
			float2 vB = ortho_proj(vA - vHUvL, vHDir);
			vHUv += vHDir * (length(vHUvL - vA) / length(vB));
		}
	}

	return HexXY(vHUv);
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

// signed distance functions https://iquilezles.org/articles/distfunctions/

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

// intersectors https://iquilezles.org/articles/intersectors

// ellipsoid centered at the origin with radii vRad
float iEllipsoid(in float3 vOri, in float3 vDir, in float3 vCen, in float3 vRad)
{
	float3 vOc = vOri - vCen;

	float3 vOcn = vOc / vRad;
	float3 vRdn = vDir / vRad;

	float fA = dot(vRdn, vRdn);
	float fB = dot(vOcn, vRdn);
	float fC = dot(vOcn, vOcn);
	float fH = fB * fB - fA * (fC - 1.0);
	if (fH < 0.0) return -1.0;
	return (-fB - sqrt(fH)) / fA;
}

// intersect a ray with a rounded box
float iRoundedBox(in float3 vOri, in float3 vDir, in float3 vCen, in float3 vSize, in float fRad)
{
	float3 vOc = vOri - vCen;

	// bounding box
	float3 m = 1.0 / vDir;
	float3 n = m * vOc;
	float3 k = abs(m) * (vSize + fRad);
	float3 t1 = -n - k;
	float3 t2 = -n + k;
	float tN = max(max(t1.x, t1.y), t1.z);
	float tF = min(min(t2.x, t2.y), t2.z);
	if (tN > tF || tF < 0.0) return -1.0;
	float t = tN;

	// convert to first octant
	float3 pos = vOc + t * vDir;
	float3 s = sign(pos);
	vOc *= s;
	vDir *= s;
	pos *= s;

	// faces
	pos -= vSize;
	pos = max(pos.xyz, pos.yzx);
	if (min(min(pos.x, pos.y), pos.z) < 0.0) return t;

	// some precomputation
	float3 oc = vOc - vSize;
	float3 dd = vDir * vDir;
	float3 oo = oc * oc;
	float3 od = oc * vDir;
	float ra2 = fRad * fRad;

	t = 1e20;

	// corner
	{
		float b = od.x + od.y + od.z;
		float c = oo.x + oo.y + oo.z - ra2;
		float h = b * b - c;
		if (h > 0.0) t = -b - sqrt(h);
	}

	// edge X
	{
		float a = dd.y + dd.z;
		float b = od.y + od.z;
		float c = oo.y + oo.z - ra2;
		float h = b * b - a * c;
		if (h > 0.0)
		{
			h = (-b - sqrt(h)) / a;
			if (h > 0.0 && h < t && abs(vOc.x + vDir.x * h) < vSize.x) t = h;
		}
	}
	// edge Y
	{
		float a = dd.z + dd.x;
		float b = od.z + od.x;
		float c = oo.z + oo.x - ra2;
		float h = b * b - a * c;
		if (h > 0.0)
		{
			h = (-b - sqrt(h)) / a;
			if (h > 0.0 && h < t && abs(vOc.y + vDir.y * h) < vSize.y) t = h;
		}
	}
	// edge Z
	{
		float a = dd.x + dd.y;
		float b = od.x + od.y;
		float c = oo.x + oo.y - ra2;
		float h = b * b - a * c;
		if (h > 0.0)
		{
			h = (-b - sqrt(h)) / a;
			if (h > 0.0 && h < t && abs(vOc.z + vDir.z * h) < vSize.z) t = h;
		}
	}

	if (t > 1e19) t = -1.0;

	return t;
}

// f(x,y,z) = x^4 + y^4 + z^4 - ra^4
float iSphere4(in float3 vOri, in float3 vDir, in float3 vCen, in float fRad)
{
	float3 vOc = vOri - vCen;

	// -----------------------------
	// solve quartic equation
	// -----------------------------

	float r2 = fRad * fRad;

	float3 d2 = vDir * vDir; float3 d3 = d2 * vDir;
	float3 o2 = vOc * vOc; float3 o3 = o2 * vOc;

	float ka = 1.0 / dot(d2, d2);

	float k3 = ka * dot(vOc, d3);
	float k2 = ka * dot(o2, d2);
	float k1 = ka * dot(o3, vDir);
	float k0 = ka * (dot(o2, o2) - r2 * r2);

	// -----------------------------
	// solve cubic
	// -----------------------------

	float c2 = k2 - k3 * k3;
	float c1 = k1 + 2.0 * k3 * k3 * k3 - 3.0 * k3 * k2;
	float c0 = k0 - 3.0 * k3 * k3 * k3 * k3 + 6.0 * k3 * k3 * k2 - 4.0 * k3 * k1;

	float p = c2 * c2 + c0 / 3.0;
	float q = c2 * c2 * c2 - c2 * c0 + c1 * c1;

	float h = q * q - p * p * p;

	// -----------------------------
	// skip the case of three real solutions for the cubic, which involves four
	// complex solutions for the quartic, since we know this objcet is convex
	// -----------------------------
	if (h < 0.0) return -1.0;

	// one real solution, two complex (conjugated)
	float sh = sqrt(h);

	float s = sign(q + sh) * pow(abs(q + sh), 1.0 / 3.0); // cuberoot
	float t = sign(q - sh) * pow(abs(q - sh), 1.0 / 3.0); // cuberoot
	float2  w = float2(s + t, s - t);

	// -----------------------------
	// the quartic will have two real solutions and two complex solutions.
	// we only want the real ones
	// -----------------------------

#if 1
	float2  v = float2(w.x + c2 * 4.0, w.y * sqrt(3.0)) * 0.5;
	float r = length(v);
	return -abs(v.y) / sqrt(r + v.x) - c1 / r - k3;
#else
	float r = sqrt(c2 * c2 + w.x * w.x + 2.0 * w.x * c2 - c0);
	return -sqrt(3.0 * w.y * w.y / (4.0 * r + w.x * 2.0 + c2 * 8.0)) - c1 / r - k3;
#endif    
}

// intersect capsule
float iCapsule(in float3 vOri, in float3 vDir, in float3 vCen, in float3 pa, in float3 pb, in float r)
{
	float3 vOc = vOri - vCen;

	float3  ba = pb - pa;
	float3  oa = vOc - pa;

	float baba = dot(ba, ba);
	float bard = dot(ba, vDir);
	float baoa = dot(ba, oa);
	float rdoa = dot(vDir, oa);
	float oaoa = dot(oa, oa);

	float a = baba - bard * bard;
	float b = baba * rdoa - baoa * bard;
	float c = baba * oaoa - baoa * baoa - r * r * baba;
	float h = b * b - a * c;
	if (h >= 0.0)
	{
		float t = (-b - sqrt(h)) / a;
		float y = baoa + t * bard;
		// body
		if (y > 0.0 && y < baba) return t;
		// caps
		float3 oc = (y <= 0.0) ? oa : vOc - pb;
		b = dot(vDir, oc);
		c = dot(oc, oc) - r * r;
		h = b * b - c;
		if (h > 0.0) return -b - sqrt(h);
	}
	return -1.0;
}

// normal function for intersected ellipsoid
float3 nEllipsoid(in float3 vPos, in float3 vCen, in float3 vRad)
{
	return normalize((vPos - vCen) / (vRad * vRad));
}

// normal of a rounded box
float3 nRoundedBox(in float3 vPos, in float3 vCen, in float3 vSiz)
{
	float3 vPc = vPos - vCen;
	return sign(vPc) * normalize(max(abs(vPc) - vSiz, 0.0));
}

// df/dx,df/dy,df/dx for f(x,y,z) = x^4 + y^4 + z^4 - ra^4
float3 nSphere4(in float3 vPos, in float3 vCen)
{
	float3 vPc = vPos - vCen;
	return normalize(vPc * vPc * vPc);
}

float3 nCapsule(in float3 vPos, in float3 vCen, in float3 a, in float3 b, in float r)
{
	float3 vPc = vPos - vCen;
	float3  ba = b - a;
	float3  pa = vPc - a;
	float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
	return (pa - h * ba) / r;
}

// circular repitition, moved to and shifted within aa bounding box 
float iCircularEllipsoids(in float3 vOri, in float3 vDir, in float fRad, in float fSpc, out float3 vNormal, out float2 vIdH, in float fTime)
{
	float fThit = -1.f;
	int nRad = int(fRad);
	
	// loop through grid int2(2*Rad, 2*Rad) 
	for (int nJ = -nRad; nJ <= nRad; nJ++)
		for (int nI = -nRad; nI <= nRad; nI++)
		{
			// is in radius ?
			float2 vId = float2(nI, nJ);
			if (dot(vId, vId) <= fRad * fRad)
			{
				// shift origin
				float3 vQ = vOri - fSpc * float3(vId.x, 0.f, vId.y);

				// rotate and lift ellipsoid
				float fC = sqrt(dot(vId, vId));
				float3 vRad = (fC < (fRad * .33)) ? float3(1.f, 1.5f, 1.f) : (fC < (fRad * .66)) ? float3(1.5f, 1.f, 1.f) : float3(1.f, 1.f, 1.5f);
				vRad *= fSpc * .08f;
				float3 vPosE = float3(12.f, 1.5f + fC * sin(fTime * fC) * .2f, -32.f);

				// ellipsoid intersection ?
				float fH1 = iEllipsoid(vQ, vDir, vPosE, vRad);
				if (fH1 >= 0.f)
				{
					// set nearest hit
					fThit = (fThit >= 0.f) ? min(fThit, fH1) : fH1;

					// nearest == current ?
					if (fThit == fH1)
					{
						float3 vPosition = vQ + fThit * vDir;
						vNormal = nEllipsoid(vPosition, vPosE, vRad);
						vIdH = vId;
					}
				}
			}
		}
	return fThit;
}

// function to bend the cylinder
float fc(float fX)
{
	return sin(fX * .8) * .9 + 3. +cos(fX * .3);
	// return sin(fX * 2.) * .4;
	// return sin(fX * 3.) * .4  + 2.5 + cos(fX * .6);
}

// blanket function to wrap any heightmap by tangent
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
