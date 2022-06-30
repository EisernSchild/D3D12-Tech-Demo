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
// Texture2D sTexIn            : register(t0);
RWStructuredBuffer<Vertex> sVBOut : register(u0);

[numthreads(1, 1, 1)]
void main(int3 sGroupTID : SV_GroupThreadID, int3 sDispatchTID : SV_DispatchThreadID)
{
	uint uIx = uint(sDispatchTID.x) / uint(19);
	uint uIxB = uint(sDispatchTID.x) % uint(19);

	sVBOut[sDispatchTID.x + 19].sPosL.y = sVBOut[uIxB].sPosL.y;
	sVBOut[sDispatchTID.x + 19].sPosL.xz = sVBOut[uIxB].sPosL.xz + avTilePos[uIx].xy;

	/*uint uIx = uint(sDispatchTID.x) / uint(sHexData.x);
	uint uIxB = uint(sDispatchTID.x) % uint(sHexData.x);

	sVBOut[sDispatchTID.x + sHexData.x].sPosL.y = sVBOut[uIxB].sPosL.y;
	sVBOut[sDispatchTID.x + sHexData.x].sPosL.xz = sVBOut[uIxB].sPosL.xz + avTilePos[uIx].xy;*/
}