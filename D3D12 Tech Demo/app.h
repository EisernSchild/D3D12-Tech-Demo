// D3D12 Tech Demo
// (c) 2022 by Denis Reischl
// 
// SPDX-License-Identifier: MIT

#include <stdio.h>
#include <thread>
#include <future>

#ifdef _WIN32
#define TRACE_UINT(a) { wchar_t buf[128]; wsprintfW(buf, L"%s:%u:0x%x", L#a, a, a); OutputDebugStringW(buf); }
#define TRACE_HEX(a) { wchar_t buf[128]; wsprintfW(buf,  L"%s:%x", L#a, a); OutputDebugStringW(buf); }
#define TRACE_FLOAT(a) { wchar_t buf[128]; swprintf(buf, 128, L"%s:%f", L#a, a); OutputDebugStringW(buf); }
#define TRACE_CODE { wchar_t buf[128]; wsprintfW(buf, L"(%u) : %s", __LINE__, __FUNCTIONW__); OutputDebugStringW(buf); }
#define APP_FORWARD S_OK
#define APP_PAUSE S_FALSE
#define APP_QUIT E_ABORT
#define APP_ERROR E_FAIL
#else 
#error "OS not supported!"
#endif

/// <summary>
/// Game Time simple helper.
/// </summary>
struct GameTime
{
	double dTime;
};

#define FUNC_GAME_TASK(task) static signed task(GameTime& sInfo)
typedef signed(*GAME_TASK)(GameTime& sInfo);

/// <summary>
/// App Basics Parent Class.
/// </summary>
class App_Taskhandler
{
public:
	static constexpr unsigned INIT = 0;
	static constexpr unsigned RUNTIME = 1;
	static constexpr unsigned DESTROY = 2;

	explicit App_Taskhandler(
		std::vector<std::vector<GAME_TASK>>& aavTasks_Init,
		std::vector<std::vector<GAME_TASK>>& aavTasks_Runtime,
		std::vector<std::vector<GAME_TASK>>& aavTasks_Destroy
	)
	{
		// add tasks
		Add(aavTasks_Init, aavTasks_Runtime, aavTasks_Destroy);
	}
	virtual ~App_Taskhandler() {}

protected:

	/// <summary>
	/// Add tasks to the list
	/// </summary>
	/// <param name="bInsertFirst">true if taskblocks should be inserted at begin</param>
	void Add(
		std::vector<std::vector<GAME_TASK>>& aavTasks_Init,
		std::vector<std::vector<GAME_TASK>>& aavTasks_Runtime,
		std::vector<std::vector<GAME_TASK>>& aavTasks_Destroy,
		bool bInsertFirst = false
	)
	{
		// pointer helpers for following operation
		std::vector<std::vector<GAME_TASK>>* paav = nullptr;
		std::vector<std::vector<std::packaged_task<signed(GameTime&)>>>* paac = nullptr;

		// create lists of task blocks
		for (unsigned uIx : {INIT, RUNTIME, DESTROY})
		{
			// set helper pointers
			switch (uIx)
			{
			case INIT:
				paav = &aavTasks_Init;
				paac = &aacTasks_Init;
				break;
			case RUNTIME:
				paav = &aavTasks_Runtime;
				paac = &aacTasks_Runtime;
				break;
			case DESTROY:
				paav = &aavTasks_Destroy;
				paac = &aacTasks_Destroy;
				break;
			default: continue; break;
			}

			// loop through task functions
			for (std::vector<GAME_TASK>& av : *paav)
			{

				// create task block and add to list
				std::vector<std::packaged_task<signed(GameTime&)>> acTaskblock;
				for (GAME_TASK& v : av)
					acTaskblock.push_back(std::packaged_task<signed(GameTime&)>(v));

				// add the block at begin (?)
				if (bInsertFirst)
					paac->insert(paac->begin(), std::move(acTaskblock));
				else
					paac->push_back(std::move(acTaskblock));
			}
		}
		aavTasks_Init.clear();
		aavTasks_Runtime.clear();
		aavTasks_Destroy.clear();
	}

	/// <summary>
	/// Task Handler... Init->Runtime->Destroy
	/// </summary>
	void Run()
	{
		GameTime sTime = {};
		std::vector<std::vector<std::packaged_task<signed(GameTime&)>>>* paac = nullptr;
		signed nFuture = APP_FORWARD;

		// execute lists of task blocks
		for (unsigned uIx : {INIT, RUNTIME, DESTROY})
		{
			// any error present ?
			if (nFuture != APP_ERROR) nFuture = APP_FORWARD;

			// set helper pointer
			switch (uIx)
			{
			case INIT: paac = &aacTasks_Init; break;
			case RUNTIME: paac = &aacTasks_Runtime; break;
			case DESTROY: paac = &aacTasks_Destroy; break;
			default: continue; break;
			}

			do
			{
				// loop through task blocks
				for (std::vector<std::packaged_task<signed(GameTime&)>>& aac : *paac)
				{
					// pause this task block ?
					if (nFuture == APP_PAUSE) continue;

					// error or quit ?
					if (((nFuture == APP_ERROR) || (nFuture == APP_QUIT)) && (uIx != DESTROY)) continue;

					// loop through tasks
					std::vector<std::future<signed>> acFutures;
					for (std::packaged_task<signed(GameTime&)>& ac : aac)
					{
						// reset task, add future
						ac.reset();
						acFutures.push_back(ac.get_future());

						// execute
						ac(sTime);
					}

					// loop through futures to synchronize
					for (std::future<signed>& cFuture : acFutures)
					{
						signed nFutureCurrent = cFuture.get();
						switch (nFutureCurrent)
						{
						case APP_FORWARD: break;
						case APP_PAUSE:
							if ((nFuture != APP_ERROR) && (nFuture != APP_QUIT))
								nFuture = nFutureCurrent;
							break;
						case APP_QUIT:
							if (nFuture != APP_ERROR)
								nFuture = nFutureCurrent;
							break;
						case APP_ERROR:
							nFuture = nFutureCurrent;
							break;
						default: break;
						}
					}
				}

				// end pause
				if (nFuture == APP_PAUSE) nFuture = APP_FORWARD;

			} while ((uIx == RUNTIME) && (nFuture != APP_QUIT) && (nFuture != APP_ERROR));
		}
	}

private:
	/// <summary>
	/// Task blocks for Initialization, Runtime and Application End
	/// Each vector of tasks (=taskblock) will be executed simultanely.
	/// Each vector of vectors will be executed sequentually.
	/// </summary>
	std::vector<std::vector<std::packaged_task<signed(GameTime&)>>>
		aacTasks_Init, aacTasks_Runtime, aacTasks_Destroy;
};

#ifdef _WIN32
#define APP App_Windows

/// <summary>
/// Windows app skeleton
/// </summary>
class App_Windows : protected App_Taskhandler
{
public:
	explicit App_Windows(
		std::vector<std::vector<GAME_TASK>>& aavTasks_Init,
		std::vector<std::vector<GAME_TASK>>& aavTasks_Runtime,
		std::vector<std::vector<GAME_TASK>>& aavTasks_Destroy
	) : App_Taskhandler(aavTasks_Init, aavTasks_Runtime, aavTasks_Destroy)
	{
		// create and add the task function blocks for any Windows action
		std::vector<std::vector<GAME_TASK>> aavTasks_Init_this = { { OsInit } };
		std::vector<std::vector<GAME_TASK>> aavTasks_Runtime_this = { { OsFrame }, { OsUpdate } };
		std::vector<std::vector<GAME_TASK>> aavTasks_Destroy_this = { { OsPreRelease } };
		Add(aavTasks_Init_this, aavTasks_Runtime_this, aavTasks_Destroy_this, true);
		aavTasks_Destroy_this = { { OsRelease } };
		Add(aavTasks_Init_this, aavTasks_Runtime_this, aavTasks_Destroy_this, false);

		// .. and execute the app
		Run();
	}
	virtual ~App_Windows() {}

protected:

	FUNC_GAME_TASK(OsInit)
	{
		HINSTANCE pHInstance = GetModuleHandle(NULL);

		// register window class
		WNDCLASSEX sWindowClass = { 0 };
		sWindowClass.cbSize = sizeof(WNDCLASSEX);
		sWindowClass.style = CS_HREDRAW | CS_VREDRAW;
		sWindowClass.lpfnWndProc = WindowProc;
		sWindowClass.hInstance = pHInstance;
		sWindowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
		sWindowClass.lpszClassName = L"App_Windows";
		RegisterClassEx(&sWindowClass);

		RECT sRcDeskt = {};
		GetClientRect(GetDesktopWindow(), &sRcDeskt);
		RECT sRect = { 0, 0, sRcDeskt.right >> 1, (sRcDeskt.right >> 5) * 9 };
		AdjustWindowRect(&sRect, WS_OVERLAPPEDWINDOW, FALSE);

		// create the window
		m_pHwnd = CreateWindow(
			sWindowClass.lpszClassName,
			L"scrith sample",
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			sRect.right - sRect.left,
			sRect.bottom - sRect.top,
			nullptr,
			nullptr,
			pHInstance,
			&sInfo);

		// show the window
		ShowWindow(m_pHwnd, SW_SHOW);
		ShowCursor(FALSE);

		return APP_FORWARD;
	}
	FUNC_GAME_TASK(OsUpdate)
	{
		return APP_FORWARD;
	}
	FUNC_GAME_TASK(OsFrame)
	{
		static MSG s_sMsg = { 0 };
		if (PeekMessage(&s_sMsg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&s_sMsg);
			DispatchMessage(&s_sMsg);
			return APP_PAUSE;
		}

		if (s_sMsg.message == WM_QUIT) return APP_QUIT; else return APP_FORWARD;
		return APP_FORWARD;
	}
	FUNC_GAME_TASK(OsPreRelease)
	{
		OutputDebugStringA("App_Windows::OsPreRelease");
		return APP_FORWARD;
	}
	FUNC_GAME_TASK(OsRelease)
	{
		OutputDebugStringA("App_Windows::OsRelease");
		return APP_FORWARD;
	}

	/// <summary>Windows main procedure</summary>
	static LRESULT CALLBACK WindowProc(HWND pHwnd, UINT uMessage, WPARAM uWparam, LPARAM uLparam)
	{
		GameTime* psTime = reinterpret_cast<GameTime*>(GetWindowLongPtr(pHwnd, GWLP_USERDATA));

		switch (uMessage)
		{
		case WM_CREATE:
		{
			// Save the DXSample* passed in to CreateWindow.
			LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(uLparam);
			SetWindowLongPtr(pHwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
		}
		return 0;

		case WM_KEYDOWN:
			if (psTime)
			{
			}
			return APP_FORWARD;

		case WM_KEYUP:
			if (psTime)
			{
			}
			return APP_FORWARD;

		case WM_DESTROY:
			PostQuitMessage(0);
			return APP_FORWARD;
		}

		return DefWindowProc(pHwnd, uMessage, uWparam, uLparam);
	}

	static HWND m_pHwnd;

};

HWND App_Windows::m_pHwnd = nullptr;

#else
#error "OS not supported!"
#endif

/// <summary>
/// Main Application
/// </summary>
class App : protected APP
{
public:
	App() : APP(m_aavTasks_Init, m_aavTasks_Runtime, m_aavTasks_Destroy) {}
	~App() {}

	FUNC_GAME_TASK(OnInit)
	{
		OutputDebugStringA("App::OnInit");
		return APP_FORWARD;
	}
	FUNC_GAME_TASK(OnUpdate)
	{
		return APP_FORWARD;
	}
	FUNC_GAME_TASK(OnFrame)
	{
		return APP_FORWARD;
	}
	FUNC_GAME_TASK(OnRelease)
	{
		OutputDebugStringA("App::OnRelease");
		return APP_FORWARD;
	}

	static std::vector<std::vector<GAME_TASK>> m_aavTasks_Init;
	static std::vector<std::vector<GAME_TASK>> m_aavTasks_Runtime;
	static std::vector<std::vector<GAME_TASK>> m_aavTasks_Destroy;
};

std::vector<std::vector<GAME_TASK>> App::m_aavTasks_Init = { { App::OnInit } };
std::vector<std::vector<GAME_TASK>> App::m_aavTasks_Runtime = { { App::OnUpdate }, { App::OnFrame } };
std::vector<std::vector<GAME_TASK>> App::m_aavTasks_Destroy = { { App::OnRelease } };