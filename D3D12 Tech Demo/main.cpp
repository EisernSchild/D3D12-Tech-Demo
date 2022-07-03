// D3D12 Tech Demo
// Copyright © 2022 by Denis Reischl
// 
// SPDX-License-Identifier: MIT

// ## notation
//
// ### prior
// a - array
// b - bool
// c - class
// d - double
// e - enumeration
// f - float
// n - signed
// p - pointer
// s - structure
// t - byte / char(token)
// u - unsigned
// v - vector(hlsl only)
// 
// ### superiour
// m - member
// s - static
// 
// ### post
// N - number
// Nme - name
// I / Ix - index
// Idc - indices
// V / Vx - vertex
// Vp - viewport
// Vtc - vertices
// T - type(enumeration)
// Tmp - temporary
// Dc - description
// Dcs - descriptions
// Sz - size
// Cnt - counter
// Fmt - format
// Rc - rectangle
// SC - swapchain
// DS - depthstencil
// Rtv - render target view
// Dsv - depth stencil view
// Rng - range
// H - handle

#include "app_TechDemo.h"

#ifdef _WIN64
/// <summary>
/// Windows main entry point.
/// </summary>
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
#else 
#error "OS not supported!"
#endif
{
	// start Application
	App_TechDemo cApp;
	return 0;
}

