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
	/// hex data (x - number of vertices per hex tile)
	uint4 sHexData;
};

struct Vertex
{
	float3 sPosL;
	float4 sCol;
};

Buffer<float4> avTilePos : register(t0);
RWStructuredBuffer<Vertex> sVBOut : register(u0);

[numthreads(1, 1, 1)]
void main(int3 sGroupTID : SV_GroupThreadID, int3 sDispatchTID : SV_DispatchThreadID)
{
	// get tile index (stored in z as float) and base vertex index (= tile 0)
	uint uBufferTileI = uint(sDispatchTID.x) / uint(sHexData.x);
	uint uTileI = uint(avTilePos[uBufferTileI].z);
	uint uBaseI = uint(sDispatchTID.x) % uint(sHexData.x);
	
	// scale fractal
	const float2 afFbmScale = float2(.05f, 10.f);

	// the first tile is our base tile, set new position based on this tile
	float2 sUV = sVBOut[uBaseI].sPosL.xz + avTilePos[uBufferTileI].xy;

	// this normal method is a mess... do that mathematically correct !!
	float3 afHeight = fbm_normal(sUV * afFbmScale.x);
	
	// set terrain.. UV is our XY position
	sVBOut[uTileI * sHexData.x + sHexData.x + uBaseI].sPosL.y = afHeight.y * afFbmScale.y;
	sVBOut[uTileI * sHexData.x + sHexData.x + uBaseI].sPosL.xz = sUV;

	// set normal as color
	sVBOut[uTileI * sHexData.x + sHexData.x + uBaseI].sCol = float4(normalize(float3(afHeight.x * afFbmScale.y, .1f, afHeight.z * afFbmScale.y)), 1.f);
}