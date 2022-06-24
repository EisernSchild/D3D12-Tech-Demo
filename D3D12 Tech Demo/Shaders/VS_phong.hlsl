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
};

Buffer<float2> avTilePos : register(t0);

struct In
{
	float3 sPosL  : POSITION;
	float4 sCol   : COLOR;
};

struct Out
{
	float4 sPosH  : SV_POSITION;
	float4 sCol   : COLOR;
	float4 sUvPos : TEXCOORD0;
};

Out main(in In sIn, in uint uVxIx : SV_VertexID, in uint uInstIx : SV_InstanceID)
{
	Out sOut;

	// get hex tile position
	float2 sInstOffset = avTilePos[uInstIx];

	// get world position with zero height (y)
	float3 sPosL = sIn.sPosL + float3(sInstOffset.x, 0., sInstOffset.y);

	// compute terrain.. we later move that to the compute shader
	float2 sUV = sPosL.xz;
	// float2 sUV = float2(sPosL.x + (sTime.x * 20.), sPosL.z);
	const float2 afFbmScale = float2(.05f, 10.f);
	float3 afHeight = fbm(sUV * afFbmScale.x, 1.)* afFbmScale.y;
	sPosL.y = afHeight.x;

	// transform to homogeneous clip space, pass color
	sOut.sPosH = mul(float4(sPosL, 1.0f), sWVP);
	sOut.sCol = sIn.sCol;

	// pass coordinate as uv (preliminary), set vertex id as w
	sOut.sUvPos = float4(sPosL, (float)uVxIx);

	return sOut;
}