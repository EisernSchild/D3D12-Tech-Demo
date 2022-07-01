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
	// get tile index and base vertex index (= tile 0)
	uint uTileI = uint(sDispatchTID.x) / uint(sHexData.x);
	uint uBaseI = uint(sDispatchTID.x) % uint(sHexData.x);

	// the first tile is our base tile, set new position based on this tile
	sVBOut[sDispatchTID.x + sHexData.x].sPosL.y = sVBOut[uBaseI].sPosL.y;
	sVBOut[sDispatchTID.x + sHexData.x].sPosL.xz = sVBOut[uBaseI].sPosL.xz + avTilePos[uTileI].xy;
}