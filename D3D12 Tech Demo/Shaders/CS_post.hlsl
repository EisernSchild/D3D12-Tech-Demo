// D3D12 Tech Demo
// Copyright © 2022 by Denis Reischl
// 
// SPDX-License-Identifier: MIT

#pragma warning( disable : 4714 )

#include"vrc.hlsli"

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
};

Texture2D sTexIn            : register(t0);
RWTexture2D<float4> sTexOut : register(u0);

/// number of threads X const
#define N 256
/// grayscale
#define to_grayscale(rgb) (rgb.r * 0.3 + rgb.g * 0.6 + rgb.b * 0.1)

/*

/// simple grayscale bevel
float3 bevel(Texture2D sTex, float2 sUv, float2 sD, float fP)
{
	float3 sA1 = sTex[sUv - sD].xyz;
	float3 sB2 = sTex[sUv + sD].xyz;
	return float3(.5, .5, .5) + to_grayscale(float3(sA1 * fP - sB2 * fP));
}

/// smoothing
float3 smooth(Texture2D sTex, float2 sUv, float2 sKernel, float fStrength)
{
	float3 sAvarage = float3(0., 0., 0.);
	float2 sHalfK = sKernel * 0.5;
	for (float fX = 0.0 - sHalfK.x; fX < sHalfK.x; fX += 1.0)
	{
		for (float fY = 0.0 - sHalfK.y; fY < sHalfK.y; fY += 1.0)
			sAvarage += sTex[sUv + float2(fX, fY) * fStrength].xyz;
	}
	return max(sTex[sUv].xyz, sAvarage / (sKernel.x * sKernel.y));
}

// radial (to center) blur
float3 blur_radial(Texture2D sTex, float2 sUv, int nSc, float fStrength, float fP, float2 sC)
{
	int nHalfSc = nSc / 2;
	float3 sAvarage = float3(0., 0., 0.);
	int nScT = nSc;

	// calculate avarage color from radial distant pixels
	for (int x = 0; x < nSc; x++)
	{
		float fV = float(x - nHalfSc) * fP;
		float2 sUvR = sUv + sC * fV;

		// within bounds ?
		if ((sUvR.x > 0.) && (sUvR.y > 0.) &&
			(sUvR.x < sViewport.z) && (sUvR.y < sViewport.w))
			sAvarage += sTex[sUvR].xyz;
		else
			nScT--;
	}
	sAvarage *= 1.0 / float(nScT + 1);

	float fDist = distance(sC, sUv);
	return lerp(sTex[sUv].xyz, sAvarage, clamp(fDist * fStrength, 0.0, 1.0));
}
*/

float vignette(float2 sUv, float2 sScreen)
{
	float2 sUvn = ((float2(float(sUv.x), float(sUv.y)) / sScreen.xy) - .5) * 2.;
	float fVgn1 = pow(smoothstep(0.0, .3, (sUvn.x + 1.) * (sUvn.y + 1.) * (sUvn.x - 1.) * (sUvn.y - 1.)), .5);
	float fVgn2 = 1. - pow(dot(float2(sUvn.x * .3, sUvn.y), sUvn), 3.);
	return lerp(fVgn1, fVgn2, .4) * .5 + 0.5;
}


static const uint4 aauChars[97] =
{
	uint4(0x00000000, 0x00000000, 0x00000000, 0x00000000), //  0x1e 30
	uint4(0x00000000, 0x00000000, 0x00000000, 0x00000000), //  0x1f 31
	uint4(0x00000000, 0x00000000, 0x00000000, 0x00000000), //   0x20 32
	uint4(0x00000000, 0x00001818, 0x00181818, 0x3c3c3c18), // ! 0x21 33
	uint4(0x00000000, 0x00000000, 0x00000000, 0x00444466), // " 0x22 34
	uint4(0x00000000, 0x00003636, 0x7f36367f, 0x36360000), // # 0x23 35
	uint4(0x00000000, 0x08083e6b, 0x6b381c0e, 0x6b6b3e08), // $ 0x24 36
	uint4(0x00000000, 0x00003049, 0x4b360c18, 0x36694906), // % 0x25 37
	uint4(0x00000000, 0x00006e33, 0x333b6e0c, 0x1c36361c), // & 0x26 38
	uint4(0x00000000, 0x00000000, 0x00000000, 0x00081018), // ' 0x27 39

	uint4(0x00000000, 0x00003018, 0x0c0c0c0c, 0x0c0c1830), // ( 0x28 40
	uint4(0x00000000, 0x00000c18, 0x30303030, 0x3030180c), // ) 0x29 41
	uint4(0x00000000, 0x00000000, 0x663cff3c, 0x66000000), // * 0x2a 42
	uint4(0x00000000, 0x00000000, 0x18187e18, 0x18000000), // + 0x2b 43
	uint4(0x00000000, 0x04080c0c, 0x00000000, 0x00000000), // , 0x2c 44
	uint4(0x00000000, 0x00000000, 0x00007f00, 0x00000000), // - 0x2d 45
	uint4(0x00000000, 0x00000c0c, 0x00000000, 0x00000000), // . 0x2e 46
	uint4(0x00000000, 0x00000001, 0x03060c18, 0x30604000), // / 0x2f 47
	uint4(0x00000000, 0x00003e63, 0x63676f7b, 0x7363633e), // 0 0x30 48
	uint4(0x00000000, 0x00007e18, 0x18181818, 0x181e1c18), // 1 0x31 49

	uint4(0x00000000, 0x00007f63, 0x03060c18, 0x3060633e), // 2 0x32 50
	uint4(0x00000000, 0x00003e63, 0x6060603c, 0x6060633e), // 3 0x33 51
	uint4(0x00000000, 0x00007830, 0x307f3333, 0x363c3830), // 4 0x34 52
	uint4(0x00000000, 0x00003e63, 0x6060603f, 0x0303037f), // 5 0x35 53
	uint4(0x00000000, 0x00003e63, 0x6363633f, 0x0303633e), // 6 0x36 54
	uint4(0x00000000, 0x00000c0c, 0x0c0c1830, 0x6060637f), // 7 0x37 55
	uint4(0x00000000, 0x00003e63, 0x6363633e, 0x6363633e), // 8 0x38 56
	uint4(0x00000000, 0x00003e63, 0x60607e63, 0x6363633e), // 9 0x39 57
	uint4(0x00000000, 0x00000018, 0x18000000, 0x18180000), // : 0x3a 58
	uint4(0x00000000, 0x00081018, 0x18000000, 0x18180000), // ; 0x3b 59

	uint4(0x00000000, 0x00006030, 0x180c060c, 0x18306000), // < 0x3c 60
	uint4(0x00000000, 0x00000000, 0x007e0000, 0x7e000000), // = 0x3d 61
	uint4(0x00000000, 0x0000060c, 0x18306030, 0x180c0600), // > 0x3e 62
	uint4(0x00000000, 0x00001818, 0x00181830, 0x6063633e), // ? 0x3f 63
	uint4(0x00000000, 0x00003c02, 0x6db5a5a5, 0xb9423c00), // @ 0x40 64
	uint4(0x00000000, 0x00006363, 0x63637f63, 0x6363361c), // A 0x41 65
	uint4(0x00000000, 0x00003f66, 0x6666663e, 0x6666663f), // B 0x42 66
	uint4(0x00000000, 0x00003e63, 0x63030303, 0x0363633e), // C 0x43 67
	uint4(0x00000000, 0x00003f66, 0x66666666, 0x6666663f), // D 0x44 68
	uint4(0x00000000, 0x00007f66, 0x46161e1e, 0x1646667f), // E 0x45 69

	uint4(0x00000000, 0x00000f06, 0x06161e1e, 0x1646667f), // F 0x46 70
	uint4(0x00000000, 0x00007e63, 0x63637303, 0x0363633e), // G 0x47 71
	uint4(0x00000000, 0x00006363, 0x6363637f, 0x63636363), // H 0x48 72
	uint4(0x00000000, 0x00003c18, 0x18181818, 0x1818183c), // I 0x49 73
	uint4(0x00000000, 0x00001e33, 0x33303030, 0x30303078), // J 0x4a 74
	uint4(0x00000000, 0x00006766, 0x66361e1e, 0x36666667), // K 0x4b 75
	uint4(0x00000000, 0x00007f66, 0x46060606, 0x0606060f), // L 0x4c 76
	uint4(0x00000000, 0x00006363, 0x63636b7f, 0x7f776341), // M 0x4d 77
	uint4(0x00000000, 0x00006363, 0x63737b7f, 0x6f676361), // N 0x4e 78
	uint4(0x00000000, 0x00003e63, 0x63636363, 0x6363633e), // O 0x4f 79

	uint4(0x00000000, 0x00000f06, 0x06063e66, 0x6666663f), // P 0x50 80
	uint4(0x00000000, 0x00603e7b, 0x6b636363, 0x6363633e), // Q 0x51 81
	uint4(0x00000000, 0x00006766, 0x66363e66, 0x6666663f), // R 0x52 82
	uint4(0x00000000, 0x00003e63, 0x6360301c, 0x0663633e), // S 0x53 83
	uint4(0x00000000, 0x00003c18, 0x18181818, 0x185a7e7e), // T 0x54 84
	uint4(0x00000000, 0x00003e63, 0x63636363, 0x63636363), // U 0x55 85
	uint4(0x00000000, 0x0000081c, 0x36636363, 0x63636363), // V 0x56 86
	uint4(0x00000000, 0x00004163, 0x777f6b63, 0x63636363), // W 0x57 87
	uint4(0x00000000, 0x00006363, 0x363e1c1c, 0x3e366363), // X 0x58 88
	uint4(0x00000000, 0x00003c18, 0x1818183c, 0x66666666), // Y 0x59 89

	uint4(0x00000000, 0x00007f63, 0x43060c18, 0x3061637f), // Z 0x5a 90
	uint4(0x00000000, 0x00003c0c, 0x0c0c0c0c, 0x0c0c0c3c), // [ 0x5b 91
	uint4(0x00000000, 0x00000040, 0x6030180c, 0x06030100), // \ 0x5c 92
	uint4(0x00000000, 0x00003c30, 0x30303030, 0x3030303c), // ] 0x5d 93
	uint4(0x00000000, 0x00000000, 0x00000000, 0x63361c08), // ^ 0x5e 94
	uint4(0x00000000, 0x00ff0000, 0x00000000, 0x00000000), // _ 0x5f 95
	uint4(0x00000000, 0x00000000, 0x00000000, 0x00100818), // ` 0x60 96
	uint4(0x00000000, 0x00006e33, 0x33333e30, 0x1e000000), // a 0x61 97
	uint4(0x00000000, 0x00003e66, 0x66666666, 0x3e060607), // b 0x62 98
	uint4(0x00000000, 0x00003e63, 0x03030363, 0x3e000000), // c 0x63 99

	uint4(0x00000000, 0x00006e33, 0x33333333, 0x3e303038), // d 0x64 100
	uint4(0x00000000, 0x00003e63, 0x037f6363, 0x3e000000), // e 0x65 101
	uint4(0x00000000, 0x00001e0c, 0x0c0c0c0c, 0x3e0c6c38), // f 0x66 102
	uint4(0x0000001e, 0x33303e33, 0x33333333, 0x6e000000), // g 0x67 103
	uint4(0x00000000, 0x00006766, 0x6666666e, 0x36060607), // h 0x68 104
	uint4(0x00000000, 0x00003c18, 0x18181818, 0x1c001818), // i 0x69 105
	uint4(0x0000001e, 0x33333030, 0x30303030, 0x38003030), // j 0x6a 106
	uint4(0x00000000, 0x00006766, 0x361e1e36, 0x66060607), // k 0x6b 107
	uint4(0x00000000, 0x00003c18, 0x18181818, 0x1818181c), // l 0x6c 108
	uint4(0x00000000, 0x0000636b, 0x6b6b6b7f, 0x37000000), // m 0x6d 109

	uint4(0x00000000, 0x00006666, 0x66666666, 0x3b000000), // n 0x6e 110
	uint4(0x00000000, 0x00003e63, 0x63636363, 0x3e000000), // o 0x6f 111
	uint4(0x0000000f, 0x06063e66, 0x66666666, 0x3b000000), // p 0x70 112
	uint4(0x00000078, 0x30303e33, 0x33333333, 0x3e000000), // q 0x71 113
	uint4(0x00000000, 0x00000f06, 0x0606066e, 0x7b000000), // r 0x72 114
	uint4(0x00000000, 0x00003e63, 0x301c0663, 0x3e000000), // s 0x73 115
	uint4(0x00000000, 0x0000182c, 0x0c0c0c0c, 0x3f0c0c08), // t 0x74 116
	uint4(0x00000000, 0x00006e33, 0x33333333, 0x33000000), // u 0x75 117
	uint4(0x00000000, 0x0000081c, 0x36636363, 0x63000000), // v 0x76 118
	uint4(0x00000000, 0x0000367f, 0x6b6b6b6b, 0x63000000), // w 0x77 119

	uint4(0x00000000, 0x00006363, 0x361c3663, 0x63000000), // x 0x78 120
	uint4(0x0000001f, 0x30607e63, 0x63636363, 0x63000000), // y 0x79 121
	uint4(0x00000000, 0x00007f43, 0x060c1831, 0x7f000000), // z 0x7a 122
	uint4(0x00000000, 0x00007018, 0x1818180e, 0x18181870), // { 0x7b 123
	uint4(0x00000000, 0x00001818, 0x18180000, 0x18181818), // | 0x7c 124
	uint4(0x00000000, 0x00000e18, 0x18181870, 0x1818180e), // } 0x7d 125
	uint4(0x00000000, 0x00000000, 0x00000000, 0x00003b6e), // ~ 0x7e 126
};

float font(uint2 sXY, uint uAscii, uint uScale = 1)
{
	// clamp char ascii and scale
	uint uIx = clamp(uAscii, 30, 126) - 30;
	sXY >>= clamp(uScale, 0, 7);

	// set local
	uint2 sXYl = uint2(sXY.x % 8, sXY.y % 16);

	// get indices
	uint uBitIx = sXYl.x + sXYl.y * 8;
	uint uLongIx = 3 - (uBitIx / 32);
	uint uOffset = uBitIx % 32;

	// get bit
	uint uBits = aauChars[uIx][uLongIx];
	uBits = uBits >> (uOffset);

	return (uBits & 1) ? 1.f : 0.f;
}

[numthreads(N, 1, 1)]
void main(int3 sGroupTID : SV_GroupThreadID, int3 sDispatchTID : SV_DispatchThreadID)
{
	// float4 sPostCol = sTexIn[sDispatchTID.xy];

	// bevel
	// float4 sPostCol = float4(bevel(sTexIn, sDispatchTID.xy, float2(cos(1.3), sin(2.0)), 4.), 1.);

	// smoothing
	// float4 sPostCol = float4(smooth(sTexIn, sDispatchTID.xy, float2(2., 2.), 2.), 1.);

	// radial blur
	// float4 sPostCol = float4(blur_radial(sTexIn, sDispatchTID.xy, 10., 2., .00075, (sViewport.zw * .5) - sDispatchTID.xy), 1.);

	// vignette
	// sPostCol *= vignette(sDispatchTID.xy, sViewport.zw);

	// get a ray by screen position
	float4 sPostCol = sTexIn[sDispatchTID.xy];
	float3 vDirect;
	float3 vOrigin;
	transform_ray(sDispatchTID.xy, sViewport.zw, sCamPos, sWVPrInv, vOrigin, vDirect);

	// no texel set ? (= negative alpha)... draw background panorama
	if (sPostCol.a == 0.f)
	{
		// vDirect already normalized by transform method
		float fGradient = abs(vDirect.y - .05f);

		// ray goes up ?
		if (vDirect.y > 0.05f)
		{
			sPostCol = lerp(float4(.4f, .4f, 1.f, 1.f), float4(.2f, .2f, 1.f, 1.f), smoothstep(.0f, .3f, fGradient));
		}
		else
		{
			// ray goes down.. do volume ray cast - fractal brownian motion
			float fThit = 0.1f;
			PosNorm sAttr = (PosNorm)0;
			float3 sToEyeW = float3(1.f, 1.f, 0.f);

			// add hex grid rim distance to ray... adjust according to mountain 
			// width to avoid vOrigin within terrain
			const float fMountMaxWidth = 30.f;
			const float fRimDist = 110.8512516844081f - fMountMaxWidth;

			// if camera y position is heigher than rim set this as rim
			vOrigin += vDirect * max(fRimDist, vOrigin.y);

			if (vrc_fbm(
				vOrigin,
				normalize(vDirect),
				fThit,
				sAttr,
				64))
			{
				float fFbmScale = .05f, fFbmScaleSimplex = .5f;
				const float2 afFbmScale = float2(.05f, 10.f);
				float2 sUV = sAttr.vPosition.xz;

				// back scale
				float fTerrain = sAttr.vPosition.y * .1f;

				// get base height color
				float fHeight = max((fTerrain + 1.) * .5f, 0.f);
				float3 sDiffuse = lerp(float3(.65, .6, .4), sDiffuseAlbedo.xyz, smoothstep(0.5f, 0.7f, fHeight));

				// draw grassland
				float fGrass = frac_noise_simplex(sUV * fFbmScaleSimplex * 2.f);
				sDiffuse = lerp(lerp(float3(.5f, .3, .9), float3(.3f, .8, .4), max(1.0f - fHeight * 1.2f, fGrass)), sDiffuse, max(.7f, min(fHeight * 1.7f, 1.f)));

				// to camera vector, ambient light
				sToEyeW = sCamPos.xyz - sAttr.vPosition;
				float3 sToEyeWN = normalize(sToEyeW);
				float4 sAmbient = sAmbientLight * float4(sDiffuse, 1.f);
				float fNdotL = max(dot(sLightVec, sAttr.vNormal), 0.1f);
				float3 sStr = sStrength * fNdotL;

				// do phong
				sPostCol = sAmbient + float4(BlinnPhong(sDiffuse, sStr, sLightVec, sAttr.vNormal, sToEyeWN, smoothstep(.5, .55, abs(fHeight))), 1.f);
			}
			else // terrain simple gradient
				sPostCol = lerp(float4(.4f, .4f, 1.f, 1.f), float4(.8f, .9f, 1.f, 1.f), smoothstep(.0, .05f, fGradient));

			// add mist
			float fDepth = length(sToEyeW);
			float fFog = fDepth * .004f;
			float4 fFogColor = float4(.8f, .9f, 1.f, 1.f) * min(fFog, 1.f);
			sPostCol = max(sPostCol, fFogColor);
		}
	}

	// display text
	uint2 sPos = uint2(26, 31);
	uint uScale = 1;
	float fText = 0.f;
	const uint uTextL = 59;
	uint uFPS = uint(sTime.w);
	uint auText[uTextL] = {
		 84, 104, 105, 115,  32, 105, 115,  32,
		115,  97, 109, 112, 108, 101,  32, 102, 
		111, 111, 116,  97, 103, 101,  32,  97,
		110, 100,  32, 105, 110,  32, 110, 111, 
		 32, 119,  97, 121,  32, 111, 112, 116,
		105, 109, 105, 122, 101, 100,  32,  33, 
		 32,  70,  80,  83,  32,  58, 0,  
		 ((uFPS / 1000) % 10) + 48,
		 ((uFPS / 100) % 10) + 48,
		 ((uFPS / 10) % 10) + 48,
		 ((uFPS) % 10) + 48
	};

	// make a method out of this eventually
	{
		// get size
		uint uW = 8 << uScale;
		uint uH = 16 << uScale;

		// get char
		uint uIx = ((uint(sDispatchTID.x) / uW) - sPos.x) % uTextL;
		uint uChar = auText[uIx];

		// get pixel position
		sPos.x *= uW;
		sPos.y *= uH;

		if ((uint(sDispatchTID.x) > sPos.x) &&
			(uint(sDispatchTID.x) < (sPos.x + uTextL * uW)) &&
			(uint(sDispatchTID.y) > sPos.y) &&
			(uint(sDispatchTID.y) < sPos.y + uH))
			fText = font(sDispatchTID.xy, uChar, uScale);
	}
	sPostCol = lerp(sPostCol, float4(1.f, 0.5f, 0.5f, 1.f), fText);

	// add vignette
	sPostCol *= vignette(sDispatchTID.xy, sViewport.zw);

	// sPostCol = lerp(sPostCol, float4(1.f, 0.5f, 0.5f, 1.f), font(sDispatchTID.xy, auText[(uint(sDispatchTID.x) / 16) % 14], 1));
	// sPostCol = lerp(sPostCol, float4(1.f, 0.5f, 0.5f, 1.f), font(sDispatchTID.xy, 84, 1));

	sTexOut[sDispatchTID.xy] = sPostCol;
}