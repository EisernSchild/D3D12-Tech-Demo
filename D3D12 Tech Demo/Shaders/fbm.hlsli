// D3D12 Tech Demo
// (c) 2022 by Denis Reischl
// 
// SPDX-License-Identifier: MIT

// fbm.hlsi:
// By Morgan McGuire @morgan3d, http://graphicscodex.com
// Reuse permitted under the BSD license.

// All noise functions are designed for values on integer scale.
// They are tuned to avoid visible periodicity for both positive and
// negative coordinates within a few orders of magnitude.

#define PI 3.141592654f
#define FLT_MAX 3.402823466e+38F

// static const float2 sFbmOffset = float2(FLT_MAX * .01f, FLT_MAX * .01f);
static const float2 sFbmOffset = float2(50000.f, 5000.f);

float random(in float2 st) {
	return frac(sin(dot(st.xy,
		float2(12.9898, 78.233))) *
		43758.5453123);
}

float noise(in float2 st) {
	float2 i = floor(st);
	float2 f = frac(st);

	// Four corners in 2D of a tile
	float a = random(i);
	float b = random(i + float2(1.0, 0.0));
	float c = random(i + float2(0.0, 1.0));
	float d = random(i + float2(1.0, 1.0));

	float2 u = f * f * (3.0 - 2.0 * f);

	return lerp(a, b, u.x) +
		(c - a) * u.y * (1.0 - u.x) +
		(d - b) * u.x * u.y;
}

#define OCTAVES 6
float fbm(in float2 st) {
	// Initial values
	float value = 0.0;
	float amplitude = .5;
	float frequency = 0.;
	// set an offset
	st += sFbmOffset;
	//
	// Loop of octaves
	for (int i = 0; i < OCTAVES; i++) {
		value += amplitude * noise(st);
		st *= 2.;
		amplitude *= .5;
	}
	return value;
}