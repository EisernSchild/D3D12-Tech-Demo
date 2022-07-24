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
	Donut,
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
		0xFF, 0, 1, 0, sRay, sPay);

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
	in float3 vLight = normalize(float3(-.4f, .2f, -.3f)),
	in float3 cLight = float3(.9f, .8f, .7f))
{
	// get distance, reflection
	float fDist = length(vLitPos - vPos);
	float3 vRef = reflect(vRayDir, vNorm);

	// calculate fresnel, specular factors
	float fFresnel = max(dot(vNorm, -vRayDir), 0.0);
	fFresnel = pow(fFresnel, .3) * 1.1;
	float fSpecular = max(dot(vRef, vLight), 0.0);

	// do lighting.. inverse normal for translucent primitives
	float3 cLit = cMaterial * 0.5f;
	cLit = lerp(cLit, cMaterial * max(dot(vNorm, vLight), fAmbient), min(fFresnel, 1.0));
	if (bTranslucent)
		cLit = lerp(cLit, cMaterial * max(dot(-vNorm, vLight), fAmbient), .2f);
	cLit += cLight * pow(fSpecular, 220.0);
	
	return cLit;
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
			sPay.vColor = float4(SceneLighting(sCamPos.xyz, sPay.vDir, sAttr.vPosition, sAttr.vNormal, float3(1.f, sAttr.vColor), true, .3f), 1.f);
			break;
		}
		case ScenePrimitive::Donut:
		{
			sPay.vColor = float4(1.f, .2f, 1.f, 1.f);
			break;
		}
		case ScenePrimitive::Mallow:
		{
			sPay.vColor = float4(1.f, .2f, 1.f, 1.f);
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

	// ray goes up ?
	if (vDir.y > 0.0f)
	{
		sPay.vColor = lerp(float4(.6f, .6f, 1.f, 1.f), float4(.2f, .2f, 1.f, 1.f), smoothstep(.1f, .3f, fGradient));
	}
	else
	{
		float3 vLitPos = GroundLitPos(sCamPos.xyz, sPay.vDir);
		float fRayDist = length(vLitPos - sCamPos.xyz);
		float3 vNormal = -SimpleFloorNorm(vLitPos.xz * 100.f, .5f);
		float fFadeOff = clamp(fRayDist * .01f, 0.f, 1.f);
		float fFadeOff1 = clamp(fRayDist * .06f, 0.f, 1.f);
		float3 cCandy = lerp(CandySpiral(vLitPos.xz, sTime.x * .18f), float3(1., 1., 1.), fFadeOff);
		vNormal = lerp(vNormal, float3(0.f, 1.f, 0.f), fFadeOff1);
		sPay.vColor = float4(SceneLighting(sCamPos.xyz, sPay.vDir, vLitPos, vNormal, cCandy), 1.f);
	}
}

[shader("intersection")]
void IntersectionShader()
{
	float fThit = 0.1f;
	PosNorm sAttr = (PosNorm)0;

	switch ((ScenePrimitive)PrimitiveIndex())
	{
		case ScenePrimitive::CandyLoop:
		{
			// candy loop - bent cylinder ("endless" means -300 < x < +300)
			if (vrc(ObjectRayOrigin(), normalize(ObjectRayDirection()), Primitive::CylinderBent, fThit, sAttr, sTime.x))
			{
				ReportHit(fThit, 0, sAttr);
			}
			break;
		}
		case ScenePrimitive::CandyDrops:
		{
			//
			float3 vOri = ObjectRayOrigin();
			float3 vDir = normalize(ObjectRayDirection());

			// render a circle of candies
			fThit = sdCircularEllipsoids(vOri, vDir, 5.f, 1.5f, sAttr.vNormal, sAttr.vColor, sTime.x);
			if (fThit > 0.0 && fThit <= RayTCurrent())
			{
				// normal already set
				sAttr.vPosition = vOri + fThit * vDir;
				sAttr.vColor = clamp(sAttr.vColor, float2(.8, .8), float2(1., 1.));
				ReportHit(fThit, 0., sAttr);

				// TODO !! occlusion : occ = 0.5 + 0.5 * nor.y;
			}
			break;
		}
		case ScenePrimitive::Donut:
		{
			//ReportHit(fThit, 0., sAttr);
			break;
		}
		case ScenePrimitive::Mallow:
		{
			//ReportHit(fThit, 0., sAttr);
			break;
		}
		default:break;
	}
}
