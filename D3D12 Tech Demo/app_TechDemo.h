// D3D12 Tech Demo
// (c) 2022 by Denis Reischl
// 
// SPDX-License-Identifier: MIT

#include "app_D3D12.h"

/// <summary>
/// Main Application
/// </summary>
class App_TechDemo : protected APP_GfxLib
{
public:
	App_TechDemo() : APP_GfxLib(m_aavTasks_Init, m_aavTasks_Runtime, m_aavTasks_Destroy)
	{
		// .. and execute the app
		Run();
	}
	~App_TechDemo() {}

	FUNC_GAME_TASK(OnInit)
	{
		OutputDebugStringA("App_TechDemo::OnInit");
		return APP_FORWARD;
	}
	FUNC_GAME_TASK(OnUpdate)
	{
		return APP_FORWARD;
	}
	FUNC_GAME_TASK(OnRenderPipeline0)
	{
		return APP_FORWARD;
	}
	FUNC_GAME_TASK(OnRenderPipeline1)
	{
		return APP_FORWARD;
	}
	FUNC_GAME_TASK(OnRenderPipeline2)
	{
		return APP_FORWARD;
	}
	FUNC_GAME_TASK(OnRenderAudio)
	{
		return APP_FORWARD;
	}
	FUNC_GAME_TASK(OnPostRender)
	{
		return APP_FORWARD;
	}
	FUNC_GAME_TASK(OnRelease)
	{
		OutputDebugStringA("App_TechDemo::OnRelease");
		return APP_FORWARD;
	}

	static std::vector<std::vector<GAME_TASK>> m_aavTasks_Init;
	static std::vector<std::vector<GAME_TASK>> m_aavTasks_Runtime;
	static std::vector<std::vector<GAME_TASK>> m_aavTasks_Destroy;
};

/// The concurrent scheme of the App 
std::vector<std::vector<GAME_TASK>> App_TechDemo::m_aavTasks_Init = { { APP_Os::OsInit }, { APP_GfxLib::GxInit },  { App_TechDemo::OnInit } };
std::vector<std::vector<GAME_TASK>> App_TechDemo::m_aavTasks_Runtime = {
	{ App_TechDemo::OnUpdate, APP_Os::OsUpdate }, { APP_Os::OsFrame },
	{ App_TechDemo::OnRenderPipeline0, App_TechDemo::OnRenderPipeline1, App_TechDemo::OnRenderPipeline2, App_TechDemo::OnRenderAudio },
	{ App_TechDemo::OnPostRender }
};
std::vector<std::vector<GAME_TASK>> App_TechDemo::m_aavTasks_Destroy = {
	{ APP_Os::OsPreRelease }, { APP_GfxLib::GxRelease }, { App_TechDemo::OnRelease }, { APP_Os::OsRelease }
};
