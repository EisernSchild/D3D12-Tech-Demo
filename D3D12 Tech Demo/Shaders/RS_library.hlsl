// D3D12 Tech Demo
// Copyright © 2022 by Denis Reischl
// 
// SPDX-License-Identifier: MIT

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

typedef BuiltInTriangleIntersectionAttributes MyAttributes;
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
void ClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
	float3 barycentrics = float3(1 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
	payload.vColor = float4(barycentrics, 1);
}

[shader("miss")]
void MissShader(inout RayPayload payload)
{
	payload.vColor = float4(0.0f, 0.2f, 0.4f, 1.f);
}
