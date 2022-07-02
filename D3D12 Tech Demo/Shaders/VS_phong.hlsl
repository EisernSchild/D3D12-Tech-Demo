// D3D12 Tech Demo
// Copyright � 2022 by Denis Reischl
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
};

Buffer<float4> avTilePos : register(t0);

struct In
{
	float3 sPosL  : POSITION;
	float4 sCol   : COLOR;
};

struct Out
{
	float4 sPosH   : SV_POSITION;
	float3 sPosW   : POSITION;
	float4 sCol    : COLOR;
	float3 sNormal : NORMAL;
};

Out main(in In sIn, in uint uVxIx : SV_VertexID, in uint uInstIx : SV_InstanceID)
{
	Out sOut;

	// compute terrain here... in compute shader we get flaws
	// for some reason.... !!
	//

	// compute terrain.. we later move that to the compute shader
	const float2 afFbmScale = float2(.05f, 10.f);
	float2 sUV = sIn.sPosL.xz;

	// this normal method is a mess... do that mathematically correct
	float3 afHeight = fbm_normal(sUV * afFbmScale.x);
	sIn.sPosL.y = afHeight.y * afFbmScale.y;

	// set normal
	sOut.sNormal = normalize(float3(afHeight.x * afFbmScale.y, .1f, afHeight.z * afFbmScale.y));

	// transform to homogeneous clip space, pass color
	sOut.sPosH = mul(float4(sIn.sPosL, 1.0f), sWVP);
	sOut.sCol = sIn.sCol;

	// set world position
	sOut.sPosW = sIn.sPosL;
	
	return sOut;
}