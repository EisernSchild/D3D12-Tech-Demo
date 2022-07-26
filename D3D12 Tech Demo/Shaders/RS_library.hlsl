// D3D12 Tech Demo
// Copyright © 2022 by Denis Reischl
// 
// SPDX-License-Identifier: MIT

#define _DXR
#include"vrc.hlsli"

// scene primitives
enum struct ScenePrimitive
{
	CandyLoop,
	CandyDrops,
	TaffyCandy,
	Mallow
};

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

RaytracingAccelerationStructure Scene : register(t0);
RWTexture2D<float4> RenderTarget : register(u0);

struct RayPayload
{
	float4 vColor;
	float3 vDir;
};

[shader("raygeneration")]
void RayGenerationShader()
{
	float3 vDirect;
	float3 vOrigin;

	// generate a ray by index
	transform_ray(DispatchRaysIndex().xy, DispatchRaysDimensions().xy, sCamPos, sWVPrInv, vOrigin, vDirect);
	RayDesc sRay = {
		vOrigin,
		0.001,
		vDirect,
		10000.0
	};

	RayPayload sPay = { float4(0, 0, 0, 0), vDirect };
	TraceRay(Scene,
		RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
		0xFF, 0, 2, 0, sRay, sPay);

	// vignette
	float2 sUV = ((float2(float(DispatchRaysIndex().x), float(DispatchRaysIndex().y)) / sViewport.zw) - .5) * 2.;
	float fVgn1 = pow(smoothstep(0.0, .3, (sUV.x + 1.) * (sUV.y + 1.) * (sUV.x - 1.) * (sUV.y - 1.)), .5);
	float fVgn2 = 1. - pow(dot(float2(sUV.x * .3, sUV.y), sUV), 3.);
	sPay.vColor *= lerp(fVgn1, fVgn2, .4) * .5 + 0.5;

	// set color
	RenderTarget[DispatchRaysIndex().xy] = sPay.vColor;
}

float SimpleFloor(float2 vPos)
{
	float fH = 1.0;
	fH -= (max(sin(vPos.x * .5 + PI * .5) + cos(vPos.y * .5 - PI * 2.), 1.25) - 2.0) * 0.25;
	return fH;
}

// https://www.shadertoy.com/view/XtByRz
float3 CandySpiral(float2 vPos, float fTime, float fSpirals = .5f, float fSize = .05f)
{
	// distance fom center, angle from center
	float d = length(vPos) * fSize;
	float a = atan2(vPos.x, vPos.y) / 3.141592 * fSpirals;

	// spirals !!
	float v = frac(d + a - fTime);

	return
		v < .25 ? float3(.67f, .85f, .8f) :
		v>.5 && v < .75 ? float3(.94f, .71f, .71f)  :
		float3(1., 1., 1.);
}

float3 SimpleFloorNorm(float2 vPos, float fStep)
{
	// get floor square
	float2 vPosOff = float2(vPos.x - fStep, vPos.y);
	float fL = SimpleFloor(vPosOff);

	vPosOff = float2(vPos.x + fStep, vPos.y);
	float fR = SimpleFloor(vPosOff);

	vPosOff = float2(vPos.x, vPos.y - fStep);
	float fU = SimpleFloor(vPosOff);

	vPosOff = float2(vPos.x, vPos.y + fStep);
	float fD = SimpleFloor(vPosOff);

	// calculate normal
	float3 vTangent = float3(2.0, fR - fL, 0.0);
	float3 vBitangent = float3(0.0, fD - fU, 2.0);
	return normalize(cross(vTangent, vBitangent));
}

float3 GroundLitPos(in float3 vPos, in float3 vRayDir)
{
	// get lit position
	float fD = -vPos.y / vRayDir.y;
	return float3(vPos.x + vRayDir.x * fD, 0.0, vPos.z + vRayDir.z * fD);
}

float3 SceneLighting(in float3 vPos, in float3 vRayDir, in float3 vLitPos,
	in float3 vNorm = float3(.0f, 1.f, 0.f),
	in float3 cMaterial = float3(.3, .4, .45),
	in bool bTranslucent = false,
	in float fAmbient = 0.2f,
	in float fSpecularPow = 220.f,
	in float fSpecularAdj = 1.f,
	in float3 cLight = float3(.9f, .8f, .7f),
	in float3 vLight = normalize(float3(-.4f, .2f, -.3f)))
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

// checkers, hexagonal
float checkers_hex(in float2 vPt, in float2 vDpdx, in float2 vDpdy)
{
	// filter kernel
	float2 vW = abs(vDpdx) + abs(vDpdy) + 0.001;
	// integral
	float fIn = (HexGrid(vPt + .5 * vW) + HexGrid(vPt - .5 * vW)) * .5f;

	return lerp(smoothstep(.6, .65, fIn), fIn, length(vW));
}

[shader("closesthit")]
void ClosestHitShader(inout RayPayload sPay, in PosNorm sAttr)
{
	switch ((ScenePrimitive)PrimitiveIndex())
	{
		case ScenePrimitive::CandyLoop:
		{
			float fX = sAttr.vPosition.x;
			float fZ = sAttr.vPosition.z;
			float3 cCandy = float3(
				max(.7f, sin(fX * .3f) * .5f + .5f),
				max(.6f, cos(fX * .2f) * .5f + .5f),
				max(.5f, sin(fX * .55f) * .5f + .5f)
				);
			cCandy = lerp(cCandy.zxy, cCandy, step(.5, frac(fX + fZ)));
			sPay.vColor = float4(SceneLighting(sCamPos.xyz, sPay.vDir, sAttr.vPosition, sAttr.vNormal, cCandy), 1.f);
			break;
		}
		case ScenePrimitive::CandyDrops:		
		{
			// candy drops
			sPay.vColor = float4(SceneLighting(sCamPos.xyz, sPay.vDir, sAttr.vPosition, sAttr.vNormal, float3(1.f, sAttr.vColor), true, .3f), 1.f);
			break;
		}
		case ScenePrimitive::TaffyCandy:
		{
			// taffy candy bar
			float3 vCol = (fmod(sAttr.vColor.y + sin(sAttr.vColor.x) * .2f, .8f) > .4f) ? float3(1., .9, .8) : float3(.8, .5, .2);
			float3 vNormal = normalize(sAttr.vNormal + float3(sin(sAttr.vColor.x * 20.f) * .05f, 0.f, cos(sAttr.vColor.y * 20.f) * .05f));

			// fade out normals
			float fRayDist = length(sAttr.vPosition - sCamPos.xyz);
			float fFadeOff = clamp(fRayDist * .06f, 0.f, 1.f);
			vNormal = normalize(lerp(vNormal, sAttr.vNormal, fFadeOff));

			sPay.vColor = float4(SceneLighting(sCamPos.xyz, sPay.vDir, sAttr.vPosition, vNormal, vCol, false, .3f, 10.f), 1.f);
			break;
		}
		case ScenePrimitive::Mallow:
		{
			// marsh mallow cube
			float fStep = sAttr.vPosition.y + sin(sAttr.vPosition.x * 10.f) * .1f + sin(sAttr.vPosition.z * 10.f) * .1f;
			float3 vCol = (fStep < 1.) ? float3(1., 1., 1.) :
				(fStep < 2.) ? lerp(float3(1., .5, .5), float3(1., .8, .8), smoothstep(0.0f, 0.2f, frac(fStep))) :
				(fStep < 3.) ? lerp(float3(.8, 1., 1.), float3(.5, 1., 1.), smoothstep(0.8f, 1.0f, frac(fStep))) : float3(1., 1., 1.);

			sPay.vColor = float4(SceneLighting(sCamPos.xyz, sPay.vDir, sAttr.vPosition, sAttr.vNormal, vCol, false, .4f, 10.f, .2f), 1.f);
			break;
		}
		default:break;
	}
}

[shader("miss")]
void MissShader(inout RayPayload sPay)
{
	float3 vDir = normalize(sPay.vDir);
	float fGradient = abs(vDir.y);
	float3 cLight = float3(.9f, .8f, .7f);
	float3 vLight = normalize(float3(-.4f, .2f, -.3f));
	float3 cSky = float3(1.0f, .4f, 0.f);

	// ray goes up ?
	if (vDir.y > 0.0f)
	{
		// render horizon
		sPay.vColor = lerp(float4(.8, .6, .5, 1.), float4(cSky, 1.f), smoothstep(.01f, .1f, fGradient));

		// render a sun
		float fSun = max(dot(vDir, vLight), 0.0);
		sPay.vColor += float4(cLight, 0.f) * pow(fSun, 100.0) * 2.f;
		sPay.vColor = clamp(sPay.vColor, 0.f, 1.f);
	}
	else
	{
		// get ground uv, render checkers, no ddx, ddy in dxr !! 
		float3 vLitPos = GroundLitPos(sCamPos.xyz, sPay.vDir);
		float fRayDist = length(vLitPos - sCamPos.xyz);
		float fCh = checkers_hex(vLitPos.xz, fRayDist * .002f, fRayDist * .002f);
		float3 cCandy = lerp(CandySpiral(vLitPos.xz, sTime.x * .18f), float3(.9, .8, .7), fCh);

		// get little dents
		float3 vNormal = -SimpleFloorNorm(vLitPos.xz * 100.f, .5f);

		// get shadow
		float fThisSh = 0.f;
		bool bShadow = false;
		RayDesc sRaySh = { vLitPos, 0.001, normalize(float3(-.4f, .2f, -.3f)), 20.0 };
		RayPayload sPaySh = { float4(-1.0f, -1.0f, -1.0f, -1.0f), normalize(float3(-.4f, .2f, -.3f)) };				
		TraceRay(Scene, RAY_FLAG_NONE, 0xFF, 1, 2, 1, sRaySh, sPaySh);
		if (sPaySh.vColor.a >= 0.f)
		{
			bShadow = true;
			cCandy *= .8f + clamp(sPay.vColor.a * .04, .0, .15);
		}
		
		// fade out normals
		float fFadeOff = clamp(fRayDist * .06f, 0.f, 1.f);
		vNormal = normalize(lerp(vNormal, float3(0.f, 1.f, 0.f), fFadeOff));

		// get reflection ray
		float3 vRef = normalize(reflect(sPay.vDir, vNormal));
		float fThisRf = 0.f;
		RayDesc sRayRf = { vLitPos, 0.001, vRef, 1000.0 };
		RayPayload sPayRf = { float4(-1.0f, -1.0f, -1.0f, -1.0f), vRef };
		TraceRay(Scene, RAY_FLAG_NONE, 0xFF, 1, 2, 1, sRayRf, sPayRf);

		// reflect by sky color or dark gray
		float3 cRef = cSky;
		if (sPayRf.vColor.a >= 0.f) cRef = float3(.3f, .3f, .3f);
		cCandy = lerp(cCandy, cRef, bShadow ? 0.4f : 0.2f);

		// do lighting
		sPay.vColor = float4(SceneLighting(sCamPos.xyz, sPay.vDir, vLitPos, vNormal, cCandy, false, 0.8f, 220.f, 1.f, bShadow ? float3(.0f, .0f, .0f) : float3(.9f, .8f, .9f)), 1.f);

		// blend with horizon to avoid flaws   
		sPay.vColor = lerp(sPay.vColor, float4(.8, .6, .5, 1.), clamp(fRayDist * .008f, 0., 1.));
	}
}

// shadow  closest hit...
[shader("closesthit")]
void ClosestHitShaderSh(inout RayPayload sPay, in PosNorm sAttr)
{
	// set length to alpha
	float fRayDistSh = length(sAttr.vPosition - ObjectRayOrigin());
	sPay.vColor = float4(0.f, 0.f, 0.f, fRayDistSh);
}

// miss shader for shadows... returns alpha -1.
[shader("miss")]
void MissShaderSh(inout RayPayload sPay)
{
	sPay.vColor = float4(0.f, 0.f, 0.f, -1.f);
}

[shader("intersection")]
void IntersectionShader()
{
	float fThit = 0.1f;
	PosNorm sAttr = (PosNorm)0;
	float3 vOri = ObjectRayOrigin();
	float3 vDir = normalize(ObjectRayDirection());
	
	switch ((ScenePrimitive)PrimitiveIndex())
	{
		case ScenePrimitive::CandyLoop:
		{
			// candy loop - bent cylinder ("endless" means -300 < x < +300)
			if (vrc(vOri, vDir, Primitive::CylinderBent, fThit, sAttr, sTime.x))
			{
				ReportHit(fThit, 0, sAttr);
			}
			break;
		}
		case ScenePrimitive::CandyDrops:
		{
			// render a circle of candies
			fThit = iCircularEllipsoids(vOri, vDir, 3.f, 2.5f, sAttr.vNormal, sAttr.vColor, sTime.x);
			if (fThit > 0.0 && fThit <= RayTCurrent())
			{
				// normal already set
				sAttr.vPosition = vOri + fThit * vDir;
				sAttr.vColor = clamp(sAttr.vColor, float2(.8, .8), float2(1., 1.));
				ReportHit(fThit, 0., sAttr);
			}
			break;
		}
		case ScenePrimitive::TaffyCandy:
		{
			// render a rounded box
			float3 vCen = float3(6.f, .32f, -16.f);
			float3 vBoxSz = float3(8.f, .1f, .75f);
			fThit = iRoundedBox(vOri, vDir, vCen, vBoxSz, 0.2f);
			if (fThit > 0.0 && fThit <= RayTCurrent())
			{
				// set pos, normal, set uv as color
				sAttr.vPosition = vOri + fThit * vDir;
				sAttr.vColor = abs(sAttr.vPosition.xz + vCen.xz * .5f);
				sAttr.vNormal = nRoundedBox(sAttr.vPosition, vCen, vBoxSz);
				ReportHit(fThit, 0., sAttr);
			}
			break;
		}
		case ScenePrimitive::Mallow:
		{
			// render a capsule
			const float3 vCen = float3(22.f, 2.f, -18.f);
			const float3 vBoxSz = float3(8.f, .1f, .75f);
			const float3 vA = float3(-1., 0., 0.);
			const float3 vB = float3(1., 0., 0.);
			const float fR = 2.;
			fThit = iCapsule(vOri, vDir, vCen, vA, vB, fR);
			// fThit = iSphere4(vOri, vDir, vCen, 2.f);
			if (fThit > 0.0 && fThit <= RayTCurrent())
			{
				// set pos, normal, set uv as color
				sAttr.vPosition = vOri + fThit * vDir;
				sAttr.vColor = abs(sAttr.vPosition.xz + vCen.xz * .5f);
				sAttr.vNormal = nCapsule(sAttr.vPosition, vCen, vA, vB, fR);
				// sAttr.vNormal = nSphere4(sAttr.vPosition, vCen);
				ReportHit(fThit, 0., sAttr);
			}
			break;
		}
		default:break;
	}
}

/*
/// not used now... second intersection shader
/// do intersection here without normal calculation for shadows
[shader("intersection")]
void IntersectionShaderSh()
{
	float fThit = 0.1f;
	PosNorm sAttr = (PosNorm)0;
	//ReportHit(fThit, 0, sAttr);
}
*/