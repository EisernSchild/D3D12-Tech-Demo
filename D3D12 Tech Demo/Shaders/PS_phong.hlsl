// D3D12 Tech Demo
// (c) 2022 by Denis Reischl
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
	/// mouse (x - x position, y - y position, z - buttons (unsigned), w - wheel (unsigned))
	float4 sMouse;
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

float4 main(in In sIn) : SV_Target
{
	//// normalized pixel coordinates
	// float2 sUV = sIn.sPosH.xy / sViewport.zw;

	// pass color
	// return sIn.sCol;

	// draw b/w grid based on uv position
	/*return max(float4(0.02, 0.025, 0.025, 1.) * pow(sIn.sPosH.z * 2.5, 3),
		smoothstep(float4(.9, .9, .9, .9), float4(1., 1., 1., 1.), frac(sIn.sUvPos.x * 5.)) +
		smoothstep(float4(.9, .9, .9, .9), float4(1., 1., 1., 1.), 1. - frac(sIn.sUvPos.x * 5.)) +
		smoothstep(float4(.9, .9, .9, .9), float4(1., 1., 1., 1.), frac(sIn.sUvPos.z * 5.)) +
		smoothstep(float4(.9, .9, .9, .9), float4(1., 1., 1., 1.), 1. - frac(sIn.sUvPos.z * 5.)));*/

	// simple hexagons
	return max(float4(0.02, 0.025, 0.025, 1.) * pow(sIn.sPosH.z * 2., 3),
	float4(
		smoothstep(float3(.9f, .9f, .9f), float3(.99f, .99f, .99f), frac(sIn.sCol.w * 8.)) +
		smoothstep(float3(.9f, .9f, .9f), float3(.99f, .99f, .99f), 1. - frac(sIn.sCol.w * 8.))
	, 1.f));

	// dancing hexagons
	/*float3 sBaseCol = float3(fmod(abs(sin(sTime.x)), sIn.sCol.w), fmod(abs(cos(sTime.x * 1.2f)), sIn.sCol.w), fmod(abs(sin(sTime.x * .5f)), sIn.sCol.w));
	float3 sCol = sBaseCol;
	float fRepeat = .1;
	sCol *= smoothstep(0.015 * 2., .0, abs(fmod(sIn.sCol.w + .5 * fRepeat, fRepeat) - .5 * fRepeat));
	sCol *= graduate(sIn.sCol.w + sTime.x, 0.75);
	return float4(max(sBaseCol * .5 * pow(sIn.sPosH.z, 3), sCol), 1.f);*/
}