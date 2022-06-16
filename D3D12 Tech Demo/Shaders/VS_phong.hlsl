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
	float3 sPosL  : POSITION;
	float4 sCol   : COLOR;
};

struct Out
{
	float4 sPosH  : SV_POSITION;
	float4 sCol   : COLOR;
};

Out main(In sIn)
{
	Out sOut;

	// transform to homogeneous clip space, pass color
	sOut.sPosH = mul(float4(sIn.sPosL, 1.0f), sWVP);
	sOut.sCol = sIn.sCol;

	return sOut;
}