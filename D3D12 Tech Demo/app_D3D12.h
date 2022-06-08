// D3D12 Tech Demo
// (c) 2022 by Denis Reischl
// 
// SPDX-License-Identifier: MIT

#include "app.h"

#ifdef _WIN32
#define APP_GfxLib App_D3D12

/// <summary>
/// D3D12 application
/// </summary>
class App_D3D12 : protected App_Windows
{
public:
	explicit App_D3D12(
		std::vector<std::vector<GAME_TASK>>& aavTasks_Init,
		std::vector<std::vector<GAME_TASK>>& aavTasks_Runtime,
		std::vector<std::vector<GAME_TASK>>& aavTasks_Destroy
	) : App_Windows(aavTasks_Init, aavTasks_Runtime, aavTasks_Destroy) {}
	virtual ~App_D3D12() {}

protected:

	/// <summary>
	/// Init D3D12
	/// </summary>
	FUNC_GAME_TASK(GxInit)
	{
		OutputDebugStringA("App_D3D12::GxInit");
		return APP_FORWARD;
	}
	/// <summary></summary>
	FUNC_GAME_TASK(GxRelease)
	{
		OutputDebugStringA("App_D3D12::GxRelease");
		return APP_FORWARD;
	}
};

#else
#error "OS not supported!"
#endif
