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
};

float4 main(In sIn) : SV_Target
{
	//// normalized pixel coordinates
	//float2 sUV = sIn.sPosH.xy / sViewport.zw;

	//// time varying pixel color
	//float4 sCol = float4(0.5 + 0.5 * cos(sTime.x + sUV.xyx + float3(0.,2.,4.)), 1.);
	//
	//// mix
	//return lerp(sIn.sCol, sCol, .5);

	// pass color
	// return sIn.sCol;

	float fA = smoothstep(0.99, 0.995, sIn.sCol.x);
	float fB = smoothstep(0.99, 0.995, sIn.sCol.y);
	float fC = smoothstep(0.99, 0.995, sIn.sCol.z);
	float fD = smoothstep(0.49, 0.495, 1. - dot(sIn.sCol.xyz, float3(.5, .5, .5)));
	return float4(max(fA, fD), max(fB, fD), max(fC, fD), 1.);
}