// D3D12 Tech Demo
// (c) 2022 by Denis Reischl
// 
// SPDX-License-Identifier: MIT

#include <stdio.h>
#include <thread>
#include <future>

#ifdef _WIN32
#define TRACE_CODE { wchar_t buf[128]; wsprintfW(buf, L"%s(%u) : %s", __FILEW__, __LINE__, __FUNCTIONW__); OutputDebugStringW(buf); }
#define DECLARE_TASK_FUTURE(t) std::packaged_task<signed(GameTime&)> cPTask##t(##t); std::future<signed> cFuture##t = cPTask##t.get_future();
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
/// App Basics Parent Classs.
/// </summary>
class App_Skeleton
{
public:
	explicit App_Skeleton(
		GAME_TASK OsInit,
		GAME_TASK OsUpdate,
		GAME_TASK OsFrame,
		GAME_TASK OsRelease,
		GAME_TASK OnInit,
		GAME_TASK OnUpdate,
		GAME_TASK OnFrame,
		GAME_TASK OnRelease)
		: _OsInit(std::move(OsInit))
		, _OsUpdate(std::move(OsUpdate))
		, _OsFrame(std::move(OsFrame))
		, _OsRelease(std::move(OsRelease))
		, _OnInit(std::move(OnInit))
		, _OnUpdate(std::move(OnUpdate))
		, _OnFrame(std::move(OnFrame))
		, _OnRelease(std::move(OnRelease))
	{
		Run();
	}
	virtual ~App_Skeleton() {}

protected:

	void Run()
	{
		GameTime sTime = {};

		// set packaged tasks and futures
		DECLARE_TASK_FUTURE(_OsInit);
		DECLARE_TASK_FUTURE(_OnInit);
		DECLARE_TASK_FUTURE(_OsUpdate);
		DECLARE_TASK_FUTURE(_OnUpdate);
		DECLARE_TASK_FUTURE(_OsFrame);
		DECLARE_TASK_FUTURE(_OnFrame);
		DECLARE_TASK_FUTURE(_OsRelease);
		DECLARE_TASK_FUTURE(_OnRelease);

		// spawn Operating System Init
		cPTask_OsInit(sTime);
		if (cFuture_OsInit.get() == 0)
		{
			// spawn App Init
			cPTask_OnInit(sTime);

			if (cFuture_OnInit.get() == 0)
			{
				signed nFuture = 0;
				while ((nFuture != APP_QUIT) && (nFuture != APP_ERROR))
				{
					// spawn Os Update
					cPTask_OsUpdate.reset();
					cFuture_OsUpdate = cPTask_OsUpdate.get_future();
					cPTask_OsUpdate(sTime);
					nFuture = cFuture_OsUpdate.get();
					if (nFuture == APP_PAUSE) continue; else if (nFuture != APP_FORWARD) break;

					// spawn App Update
					cPTask_OnUpdate.reset();
					cFuture_OnUpdate = cPTask_OnUpdate.get_future();
					cPTask_OnUpdate(sTime);
					nFuture = cFuture_OnUpdate.get();
					if (nFuture == APP_PAUSE) continue; else if (nFuture != APP_FORWARD) break;

					// spawn Os Frame
					cPTask_OsFrame.reset();
					cFuture_OsFrame = cPTask_OsFrame.get_future();
					cPTask_OsFrame(sTime);
					nFuture = cFuture_OsFrame.get();
					if (nFuture == APP_PAUSE) continue; else if (nFuture != APP_FORWARD) break;

					// spawn App Frame
					cPTask_OnFrame.reset();
					cFuture_OnFrame = cPTask_OnFrame.get_future();
					cPTask_OnFrame(sTime);
					nFuture = cFuture_OnFrame.get();
					if (nFuture == APP_PAUSE) continue; else if (nFuture != APP_FORWARD) break;
				}
			}
		}
	}

	/// <summary>True if video rendering is enabled</summary>
	static bool m_bRender;

private:
	/// <summary>
	/// Tasks for Operating System and App itself.
	/// </summary>
	GAME_TASK
		_OsInit, _OsUpdate, _OsFrame, _OsRelease,
		_OnInit, _OnUpdate, _OnFrame, _OnRelease;
};

bool App_Skeleton::m_bRender;

#ifdef _WIN32
#define APP App_Windows

/// <summary>
/// Windows app skeleton
/// </summary>
class App_Windows : protected App_Skeleton
{
public:
	explicit App_Windows(
		GAME_TASK OnInit,
		GAME_TASK OnUpdate,
		GAME_TASK OnFrame,
		GAME_TASK OnRelease) : App_Skeleton(
			OsInit, OsUpdate, OsFrame, OsRelease,
			std::move(OnInit), std::move(OnUpdate), std::move(OnFrame), std::move(OnRelease))
		{ m_bRender = false; }
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
	App() : APP(OnInit, OnUpdate, OnFrame, OnRelease) {}
	~App() {}

	FUNC_GAME_TASK(OnInit)
	{
		OutputDebugStringA("App::OnInit");
		return APP_FORWARD;
	}
	FUNC_GAME_TASK(OnUpdate)
	{
		OutputDebugStringA("App::OnUpdate");
		return APP_FORWARD;
	}
	FUNC_GAME_TASK(OnFrame)
	{
		OutputDebugStringA("App::OnFrame");
		return APP_FORWARD;
	}
	FUNC_GAME_TASK(OnRelease)
	{
		OutputDebugStringA("App::OnRelease");
		return APP_FORWARD;
	}
};

