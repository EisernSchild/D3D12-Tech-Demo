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

RaytracingAccelerationStructure Scene : register(t0);
RWTexture2D<float4> RenderTarget                : register(u0);

struct RayPayload
{
	float4 vColor;
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

	RayPayload sPay = { float4(0, 0, 0, 0) };
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

[shader("closesthit")]
void ClosestHitShader(inout RayPayload sPay, in PosNorm sAttr)
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
	float3 sToEyeW = normalize(sCamPos.xyz - sAttr.vPosition);
	float4 sAmbient = sAmbientLight * float4(sDiffuse, 1.f);
	float fNdotL = max(dot(sLightVec, sAttr.vNormal), 0.1f);
	float3 sStr = sStrength * fNdotL;

	// do phong
	sPay.vColor = sAmbient + float4(BlinnPhong(sDiffuse, sStr, sLightVec, sAttr.vNormal, sToEyeW, smoothstep(.5, .55, abs(fHeight))), 1.f);
}

[shader("miss")]
void MissShader(inout RayPayload sPay)
{
	sPay.vColor = float4(0.0f, 0.2f, 0.4f, 1.f);
}

[shader("intersection")]
void IntersectionShader()
{
	float fThit = 0.1f;
	PosNorm sAttr = (PosNorm)0;

	// do volume ray cast - fractal brownian motion
	if (vrc_fbm(
		ObjectRayOrigin(),
		normalize(ObjectRayDirection()),
		fThit,
		sAttr))
	{
		ReportHit(fThit, 0, sAttr);
	}
}
