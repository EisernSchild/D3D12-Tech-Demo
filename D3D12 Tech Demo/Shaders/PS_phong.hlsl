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
};

struct In
{
	float4 sPosH  : SV_POSITION;
	float4 sCol   : COLOR;
	float4 sUvPos : TEXCOORD0;
};

float graduate(float fV, float fR)
{
	return clamp((0.5 * fR - abs(0.5 - fmod(fV + 0.5, 1.0))) * 2.0 / fR, 0.0, 1.0);
}

float2 graduate(float2 fV, float fR)
{
	return clamp((0.5 * fR - abs(0.5 - fmod(fV + 0.5, 1.0))) * 2.0 / fR, 0.0, 1.0);
}

float3 graduate(float3 fV, float fR)
{
	return clamp((0.5 * fR - abs(0.5 - fmod(fV + 0.5, 1.0))) * 2.0 / fR, 0.0, 1.0);
}

float4 main(in In sIn) : SV_Target
{
	/*
	// compute terrain texture.. we later move that to the compute shader
	float2 sUV = sIn.sUvPos.xz;
	// float2 sUV = float2(sIn.sUvPos.x + (sTime.x * 20.), sIn.sUvPos.z);
	float fFbmScale = .05f;
	float3 afHeight = fbm(sUV * fFbmScale, 1.);

	return float4(.2, afHeight.yz, 1.);
	*/

	// draw b/w grid based on uv position
	/*return max(lerp(float4(0.2, 0.3, 0.4, 1.), float4(0.1, 1., 0.2, 1.), fY) + pow(sIn.sPosH.z, 2) * .001,
	smoothstep(float4(.9, .9, .9, .9), float4(1., 1., 1., 1.), frac(sIn.sUvPos.x * 5.)) +
	smoothstep(float4(.9, .9, .9, .9), float4(1., 1., 1., 1.), 1. - frac(sIn.sUvPos.x * 5.)) +
	smoothstep(float4(.9, .9, .9, .9), float4(1., 1., 1., 1.), frac(sIn.sUvPos.z * 5.)) +
	smoothstep(float4(.9, .9, .9, .9), float4(1., 1., 1., 1.), 1. - frac(sIn.sUvPos.z * 5.)));*/

	/*
	// simple hexagons with uv grid
	float4 sCol = float4(0.02, 0.025, 0.025, 1.) * pow(sIn.sPosH.z * 2., 3);
	sCol = float4(smoothstep(float3(.9f, .9f, .9f), float3(.99f, .99f, .99f), 
		frac( (sIn.sUvPos.x * sqrt(.75) * 2. + sIn.sUvPos.z) / 3.))
		* .3, 1.);
	sCol += float4(smoothstep(float3(.9f, .9f, .9f), float3(.99f, .99f, .99f), frac((sIn.sUvPos.z * 2.f)/3.f)) * .3, 1.);

	return lerp(sCol,
		float4(
			smoothstep(float3(.9f, .9f, .9f), float3(.99f, .99f, .99f), frac(sIn.sCol.w * 1.)) * .3 +
			smoothstep(float3(.9f, .9f, .9f), float3(.99f, .99f, .99f), 1. - frac(sIn.sCol.w * 1.)) * .3
			, 1.f), .5);
	*/

	// simple hexagon, draw current hex center
	float4 sCol = float4(0.02, 0.025, 0.025, 1.) * pow(sIn.sPosH.z * 2., 3);
	float fDist = distance(sHexUV.zw, sIn.sUvPos.xz);
	sCol += float4(smoothstep(float3(.8f, .7f, .7f), float3(.89f, .79f, .79f), 1. - fDist), 1.);
	fDist = distance(sHexUV.xy, sIn.sUvPos.xz);
	sCol += float4(smoothstep(float3(.7f, .8f, .8f), float3(.79f, .89f, .89f), 1. - fDist), 1.);
	return max(sCol,
	float4(
		smoothstep(float3(.9f, .9f, .9f), float3(.99f, .99f, .99f), frac(sIn.sCol.w * 1.)) * .3 +
		smoothstep(float3(.9f, .9f, .9f), float3(.99f, .99f, .99f), 1. - frac(sIn.sCol.w * 1.)) * .3
	, 1.f));
}