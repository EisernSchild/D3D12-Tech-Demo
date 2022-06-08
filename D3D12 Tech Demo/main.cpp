// D3D12 Tech Demo
// (c) 2022 by Denis Reischl
// 
// SPDX-License-Identifier: MIT

#ifdef _WIN32
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#else 
#error "OS not supported!"
#endif

#include "app_TechDemo.h"

#ifdef _WIN32
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

