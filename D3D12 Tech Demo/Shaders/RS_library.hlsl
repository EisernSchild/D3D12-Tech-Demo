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

[shader("closesthit")]
void ClosestHitShader(inout RayPayload sPay, in ProceduralPrimitiveAttributes attr)
{
	/*float3 barycentrics = float3(1 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
	payload.vColor = float4(barycentrics, 1);*/
	
	sPay.vColor = float4(attr.vNormal, 1.f);// float4(0.8f, 0.8f, 0.6f, 1.f);
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
	ProceduralPrimitiveAttributes attr = (ProceduralPrimitiveAttributes)0;

	// get local origin in cube (-1, -1, -1) -> (1, 1, 1)
	float3 vOri = ObjectRayOrigin();
	float3 vOriA = abs(vOri);
	float3 vOriL = vOri / max(max(vOriA.x, vOriA.y), vOriA.z);

	// max intersection distance
	const float fMaxDist = distance(float3(1., 1., 1.), float3(-1., -1., -1.));

	// direction normalized
	float3 vDir = normalize(ObjectRayDirection());

	// raymarch through cube (-1, -1, -1) -> (1, 1, 1)
	const int nRaySteps = 100;
	const float fRayStepDist = fMaxDist / float(nRaySteps);
	const float3 vStep = vDir * fRayStepDist;
	for (int n = 0; n < nRaySteps; n++)
	{
		// raymarch step
		vOriL += vStep;

		// out of bounds ?
		if ((abs(vOriL.x) > 1.f) || (abs(vOriL.y) > 1.f) || (abs(vOriL.z) > 1.f))
			return;

		// get terrain height, this should be precomputed in heighmap
		float fTerrainY =
			fbm(vOriL.xz * 0.1f, 1.f);

		if (vOriL.y < fTerrainY)
		{
			attr.vNormal = float3(fTerrainY, fTerrainY, fTerrainY);
			ReportHit(fThit, 0, attr);
			return;
		}
	}
}
