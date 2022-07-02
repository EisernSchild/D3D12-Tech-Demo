// D3D12 Tech Demo
// Copyright © 2022 by Denis Reischl
// 
// SPDX-License-Identifier: MIT

#include"fbm.hlsli"

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

struct ProceduralPrimitiveAttributes
{
	float3 vNormal;
};

struct RayPayload
{
	float4 vColor;
};

inline void TransformRay(uint2 index, out float3 origin, out float3 direction)
{
	float2 xy = index + 0.5f; // center in the middle of the pixel.
	float2 screenPos = xy / DispatchRaysDimensions().xy * 2.0 - 1.0;

	// Invert Y for DirectX-style coordinates.
	screenPos.y = -screenPos.y;

	// unproject by inverse wvp
	float4 world = mul(float4(screenPos, 0, 1), sWVPrInv);

	world.xyz /= world.w;
	origin = sCamPos.xyz;
	direction = normalize(world.xyz - origin);
}

[shader("raygeneration")]
void RayGenerationShader()
{
	float3 vDirect;
	float3 vOrigin;

	// generate a ray by dispatched grid
	TransformRay(DispatchRaysIndex().xy, vOrigin, vDirect);

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

// phong constants
static const float4 sDiffuseAlbedo = { .9f, .9f, 1.f, 1.0f };
static const float3 sFresnelR0 = { 0.01f, 0.01f, 0.01f };
static const float4 sAmbientLight = { 0.3f, 0.4f, 0.5f, 1.0f };
static const float fRoughness = 0.15f;
static const float3 sStrength = { .9f, .9f, .9f };
static const float3 sLightVec = { 1.f, -.6f, .5f };

float CalcAttenuation(float d, float falloffStart, float falloffEnd)
{
	// Linear falloff.
	return saturate((falloffEnd - d) / (falloffEnd - falloffStart));
}

// Schlick gives an approximation to Fresnel reflectance (see pg. 233 "Real-Time Rendering 3rd Ed.").
// R0 = ( (n-1)/(n+1) )^2, where n is the index of refraction.
float3 SchlickFresnel(float3 R0, float3 normal, float3 lightVec)
{
	float cosIncidentAngle = saturate(dot(normal, lightVec));

	float f0 = 1.0f - cosIncidentAngle;
	float3 reflectPercent = R0 + (1.0f - R0) * (f0 * f0 * f0 * f0 * f0);

	return reflectPercent;
}

// BlinnPhong method by Frank Luna (C) 2015 All Rights Reserved.
float3 BlinnPhong(float3 sDiffuse, float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, float fSpec)
{
	const float m = (1.f - fRoughness) * 256.0f;
	float3 halfVec = normalize(toEye + lightVec);

	float roughnessFactor = (m + 8.0f) * pow(max(dot(halfVec, normal), 0.0f), m) / 8.0f;
	float3 fresnelFactor = SchlickFresnel(sFresnelR0, halfVec, lightVec);

	float3 specAlbedo = fresnelFactor * roughnessFactor;

	// Our spec formula goes outside [0,1] range, but we are 
	// doing LDR rendering.  So scale it down a bit.
	specAlbedo = (specAlbedo / (specAlbedo + 1.0f)) * fSpec;

	return (sDiffuse + specAlbedo) * lightStrength;
}

[shader("closesthit")]
void ClosestHitShader(inout RayPayload sPay, in ProceduralPrimitiveAttributes attr)
{
	float fFbmScale = .05f, fFbmScaleSimplex = .5f;
	const float2 afFbmScale = float2(.05f, 10.f);
	float2 sUV = attr.vNormal.xz;

	// this normal method is a mess... do that mathematically correct
	float3 afHeight = fbm_normal(sUV * fFbmScale, .5f);

	// get base height color
	float fHeight = max((afHeight.y + 1.) * .5f, 0.f);
	float3 sDiffuse = lerp(float3(1., 1., 1.) - sDiffuseAlbedo.xyz, sDiffuseAlbedo.xyz, fHeight);

	// draw rocks
	float fRocks = frac_noise_simplex(sUV * fFbmScaleSimplex * .2);
	sDiffuse = lerp(float3(.1, .1, .1), sDiffuse, max(fHeight, fRocks));

	// draw grassland
	float fGrass = frac_noise_simplex(sUV * fFbmScaleSimplex);
	sDiffuse = lerp(lerp(float3(.5f, .3, .2), float3(.3f, .8, .4), max(1.0f - fHeight * 1.2f, fGrass)), sDiffuse, max(.7f, min(fHeight * 1.7f, 1.f)));
		
	// set normal
	float3 sNormal = normalize(float3(afHeight.x * afFbmScale.y, .5f, afHeight.z * afFbmScale.y));
	
	// to camera vector, ambient light
	float3 sToEyeW = normalize(sCamPos.xyz - attr.vNormal);
	float4 sAmbient = sAmbientLight * float4(sDiffuse, 1.f);
	float fNdotL = max(dot(sLightVec, sNormal), 0.1f);
	float3 sStr = sStrength * fNdotL;

	// do phong
	sPay.vColor = sAmbient + float4(BlinnPhong(sDiffuse, sStr, sLightVec, sNormal, sToEyeW, smoothstep(.5, .55, abs(fHeight))), 1.f);
}

[shader("miss")]
void MissShader(inout RayPayload sPay)
{
	sPay.vColor = float4(0.0f, 0.2f, 0.4f, 1.f);
}

[shader("intersection")]
void IntersectionShader()
{
	const float2 afFbmScale = float2(.05f, 10.f);
	float fThit = 0.1f;
	ProceduralPrimitiveAttributes attr = (ProceduralPrimitiveAttributes)0;

	// get origin
	float3 vOri = ObjectRayOrigin();
	float3 vOriA = abs(vOri);
	float3 vOriL = vOri;

	// direction normalized
	float3 vDir = normalize(ObjectRayDirection());

	// raymarch 
	const int nMaxRaySteps = 40;
	float fStepAdjust = .6f;
	float3 vStep = vDir;
	float fThitPrev = fThit;
	for (int n = 0; n < nMaxRaySteps; n++)
	{
		// get terrain height, this should be precomputed in heighmap
		float fTerrainY =
			fbm(vOriL.xz * afFbmScale.x, 1.f) * afFbmScale.y;
		
		fThitPrev = fThit;
		fThit = vOriL.y - fTerrainY;
		if (fThit < 0.1f)
		{
			if (fThit < 0.f)
			{
				fThit = abs(fThit);
				vOriL -= vStep * (fThit / (fThit + fThitPrev));
				fThit = 0.01f;
			}
			attr.vNormal = float3(vOriL.x, fTerrainY, vOriL.z);
			ReportHit(fThit, 0, attr);
			return;
		}

		// raymarch step
		vStep = vDir * abs(fThit) * fStepAdjust;
		vOriL += vStep;
	}
}
