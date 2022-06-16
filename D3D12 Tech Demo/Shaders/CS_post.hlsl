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

Texture2D sTexIn            : register(t0);
RWTexture2D<float4> sTexOut : register(u0);

#define N 256

[numthreads(N, 1, 1)]
void main(int3 sGroupTID : SV_GroupThreadID, int3 sDispatchTID : SV_DispatchThreadID)
{
	float4 sPostCol = sTexIn[sDispatchTID.xy];

	// vignette
	float2 sUV = ((float2(float(sDispatchTID.x), float(sDispatchTID.y)) / sViewport.zw) - .5) * 2.;
	float fVgn1 = pow(smoothstep(0.0, .3, (sUV.x + 1.) * (sUV.y + 1.) * (sUV.x - 1.) * (sUV.y - 1.)), .5);
	float fVgn2 = 1. - pow(dot(float2(sUV.x * .3, sUV.y), sUV), 3.);
	sPostCol *= lerp(fVgn1, fVgn2, .4) * .5 + 0.5;

	sTexOut[sDispatchTID.xy] = sPostCol;
}