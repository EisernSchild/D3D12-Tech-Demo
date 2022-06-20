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
	/// mouse (x - x position, y - y position, z - buttons (unsigned), w - wheel (unsigned))
	float4 sMouse;
};

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

// provide one of 6 hexagonal corner points
float2 hex_corner(float fSize, uint uI)
{
	float fAngle_deg = 60. * (float)(uI % 6) - 30.;
	float fAngle_rad = PI / 180. * fAngle_deg;
	return float2(fSize * cos(fAngle_rad), fSize * sin(fAngle_rad));
}

// provide vector to neighbour field (index 0..5)
float2 to_next_field(float fSize, uint uI)
{
	return (hex_corner(fSize, uI) - hex_corner(fSize, (uI + 2) % 6));
}

Out main(in In sIn, in uint uVxIx : SV_VertexID, in uint uInstIx : SV_InstanceID)
{
	Out sOut;

	// get hex tile position
	// we will later precompute that to 
	// an array for efficiency
	float fS = 1.f;
	float2 sInstOffset = float2(0.f, 0.f);
	if (uInstIx > 0)
	{
		uint uMul = (uInstIx - 1) / 6;
		uint uOffset = (uInstIx - 1) % 6;

		// get the ambit (TODO find a more elegant way here...)
		uint uAmTN = 6;
		uint uAmbit = 1;
		uint uIx = (uInstIx - 1);
		while (uAmTN <= uIx)
		{
			uIx -= uAmTN;
			uAmTN += 6;
			uAmbit++;
		}

		sInstOffset = to_next_field(fS, uOffset) * uAmbit;
		if (uIx > uOffset)
			sInstOffset += to_next_field(fS, uOffset + 2) * (float)(uIx / 6);
	}

	// get world position with zero height (y)
	float3 sPosL = sIn.sPosL + float3(sInstOffset.x, 0., sInstOffset.y);

	// compute terrain.. we later move that to the compute shader
	// float2 sUV = sPosL.xz;
	float2 sUV = float2(sPosL.x + (sTime.x * 20.), sPosL.z);
	const float2 afFbmScale = float2(.05f, 10.f);
	float3 afHeight = fbm(sUV * afFbmScale.x, 1.) * afFbmScale.y;
	sPosL.y = afHeight.x;

	// transform to homogeneous clip space, pass color
	sOut.sPosH = mul(float4(sPosL, 1.0f), sWVP);
	sOut.sCol = sIn.sCol;

	// pass coordinate as uv (preliminary), set vertex id as w
	sOut.sUvPos = float4(sPosL, (float)uVxIx);

	return sOut;
}