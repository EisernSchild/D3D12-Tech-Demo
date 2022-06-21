// D3D12 Tech Demo
// Copyright © 2022 by Denis Reischl
// 
// SPDX-License-Identifier: MIT

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

