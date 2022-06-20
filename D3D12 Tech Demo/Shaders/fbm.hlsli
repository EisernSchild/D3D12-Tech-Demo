// D3D12 Tech Demo
// Copyright © 2022 by Denis Reischl
// 
// SPDX-License-Identifier: MIT

// fbm.hlsi:
// based on https://www.shadertoy.com/view/XdXBRH
// Copyright © 2017 Inigo Quilez
// 
// SPDX-License-Identifier: MIT

#define PI 3.141592654f
#define OCTAVES 6

// random value
float2 hash(in float2 vX)
{
    const float2 vK = float2(0.3183099, 0.3678794);
    vX = vX * vK + vK.yx;
    return -1.0 + 2.0 * frac(16.0 * vK * frac(vX.x * vX.y * (vX.x + vX.y)));
}

// return gradient noise (in x) and its derivatives (in yz)
float3 noised(in float2 afP)
{
    float2 vI = floor(afP);
    float2 vF = frac(afP);

    // we always use quintic interpolation ! (no terrain flaws !)
#if 1
    // quintic interpolation
    float2 vU = vF * vF * vF * (vF * (vF * 6.0 - 15.0) + 10.0);
    float2 vDu = 30.0 * vF * vF * (vF * (vF - 2.0) + 1.0);
#else
    // cubic interpolation
    float2 vU = vF * vF * (3.0 - 2.0 * vF);
    float2 vDu = 6.0 * vF * (1.0 - vF);
#endif    

    // get random values (3 derivate values build a quad)
    float2 avQuad[4] = {float2(0.0, 0.0), float2(1.0, 0.0), float2(0.0, 1.0), float2(1.0, 1.0)};
    float2 avG[4];
    float afV[4];
    for (int nI = 0; nI < 4; nI++)
    {
        avG[nI] = hash(vI + avQuad[nI]);
        afV[nI] = dot(avG[nI], vF - avQuad[nI]);
    }

    return float3(afV[0] + vU.x * (afV[1] - afV[0]) + vU.y * (afV[2] - afV[0]) + vU.x * vU.y * (afV[0] - afV[1] - afV[2] + afV[3]),   // value
        avG[0] + vU.x * (avG[1] - avG[0]) + vU.y * (avG[2] - avG[0]) + vU.x * vU.y * (avG[0] - avG[1] - avG[2] + avG[3]) +  // derivatives
        vDu * (vU.yx * (afV[0] - afV[1] - afV[2] + afV[3]) + float2(afV[1], afV[2]) - afV[0]));
}

// Fractional Brownian Motion
//
// vX - coordinates
// fH - the Hurst Exponent (H)
//
float3 fbm(in float2 vX, in float fH)
{
    // gain factor (G)
    float fG = exp2(-fH);
    // function, accumulation multiplier
    float fF = 1.0, fA = 1.0;
    // value
    float fT = 0.0;
    // derivatives value
    float2 vD = float2(0., 0.);
    // rotation matrix for derivatives
    float2x2 afRot = float2x2(cos(0.5), sin(0.5), -sin(0.5), cos(0.5));
    for (int nI = 0; nI < OCTAVES; nI++)
    {
        float3 vN = noised(fF * vX);
        fT += fA * vN.x; // accumulate values
        vD += fA * mul(vN.yz, afRot);  // accumulate derivatives
        fF *= 2.0;
        fA *= fG;
    }
    return float3(fT, vD);
}