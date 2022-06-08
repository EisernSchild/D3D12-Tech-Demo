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

#ifdef _WIN32
#define GameTimer game_timer

/// <summary>
/// Game Time simple helper.
/// </summary>
class game_timer
{
public:
	/// <summary>get the frequency here</summary>
	game_timer()
	{
		// get timer frequency
		__int64 n64Frequency;
		QueryPerformanceFrequency((LARGE_INTEGER*)&n64Frequency);
		m_dTicksInSeconds = 1.0 / (double)n64Frequency;

		// init timer
		__int64 nCurrTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&nCurrTime);

		m_nBaseTime = nCurrTime;
		m_nPrevTime = nCurrTime;
		m_nStopTime = 0;
		m_bStopped = false;

	}

	/// <summary>Restart</summary>
	void restart()
	{
		__int64 nStartTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&nStartTime);

		if (m_bStopped)
		{
			m_nPausedTime += (nStartTime - m_nStopTime);

			m_nPrevTime = nStartTime;
			m_nStopTime = 0;
			m_bStopped = false;
		}
	}
	/// <summary>Pause</summary>
	void pause()
	{
		if (!m_bStopped)
		{
			__int64 nCurrTime;
			QueryPerformanceCounter((LARGE_INTEGER*)&nCurrTime);

			m_nStopTime = nCurrTime;
			m_bStopped = true;
		}
	}
	/// <summary>New frame</summary>
	void tick()
	{
		static int s_nFramesThisSecond = 0;
		static int s_nFramesTotal = 0;
		static float s_fSecondsCounter = 0.0f;

		// increment frame counter for this second
		s_nFramesThisSecond++;

		// stopped ?
		if (m_bStopped)
		{
			m_dDelta = 0.0;
			m_fTotal = (float)(((m_nStopTime - m_nPausedTime) - m_nBaseTime) * m_dTicksInSeconds);
			return;
		}

		// get counter
		__int64 nCurrTime;
		QueryPerformanceCounter((LARGE_INTEGER*)&nCurrTime);
		m_nCurrTime = nCurrTime;

		// set total
		m_fTotal = (float)(((m_nCurrTime - m_nPausedTime) - m_nBaseTime) * m_dTicksInSeconds);

		// set delta
		m_dDelta = (m_nCurrTime - m_nPrevTime) * m_dTicksInSeconds;
		m_fDelta = (float)m_dDelta;

		// set previous frame time
		m_nPrevTime = m_nCurrTime;

		// clamp positive
		if (m_dDelta < 0.0)	m_dDelta = 0.0;

		// set fps if new second
		if ((m_fTotal - s_fSecondsCounter) >= 1.0f)
		{
			// set total frames
			s_nFramesTotal += s_nFramesThisSecond;

			// calc fps
			m_fFPSTotal = (float)s_nFramesThisSecond;
			m_fFPS = s_nFramesThisSecond / (m_fTotal - s_fSecondsCounter);

			// reset
			s_nFramesThisSecond = 0;
			s_fSecondsCounter += 1.0f;
		}
	}

	/// <summary>Total time</summary>
	float total() { return m_fTotal; }
	/// <summary>Timer delta in seconds</summary>
	float delta() { return m_fDelta; }
	/// <summary>Frames per second, total</summary>
	float fps_total() { return m_fFPSTotal; }
	/// <summary>Frames per second</summary>
	float fps() { return m_fFPS; }

private:
	/// <summary>Total time</summary>
	float m_fTotal;
	/// <summary>Timer delta in seconds</summary>
	float m_fDelta;
	/// <summary>Frames per second, total</summary>
	float m_fFPSTotal;
	/// <summary>Frames per second</summary>
	float m_fFPS;
	/// <summary>Ticks in seconds game timer helper</summary>
	double m_dTicksInSeconds;
	/// <summary>Timer delta (double)</summary>
	double m_dDelta;
	/// <summary>Timer base.</summary>
	__int64 m_nBaseTime;
	/// <summary>Timer pause time helper.</summary>
	__int64 m_nPausedTime;
	/// <summary>Timer stop time helper.</summary>
	__int64 m_nStopTime;
	/// <summary>Timer previous time helper.</summary>
	__int64 m_nPrevTime;
	/// <summary>Timer current time helper.</summary>
	__int64 m_nCurrTime;
	/// <summary>True if the timer is stopped.</summary>
	bool m_bStopped;
};

#else 
#error "OS not supported!"
#endif

/// <summary>
/// Global application data.
/// </summary>
struct AppData
{
	/// <summary>Total time</summary>
	float fTotal;
	/// <summary>Timer delta in seconds</summary>
	float fDelta;
	/// <summary>Frames per second, total</summary>
	float fFPSTotal;
	/// <summary>Frames per second</summary>
	float fFPS;
};

#define FUNC_GAME_TASK(task) static signed task(AppData& sInfo)
typedef signed(*GAME_TASK)(AppData& sInfo);

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
		std::vector<std::vector<std::packaged_task<signed(AppData&)>>>* paac = nullptr;

		// create lists of task blocks
		for (unsigned uIx : {INIT, RUNTIME, DESTROY})
		{
			// set helper pointers
			switch (uIx)
			{
			case INIT:
				paav = &aavTasks_Init;
				paac = &m_aacTasks_Init;
				break;
			case RUNTIME:
				paav = &aavTasks_Runtime;
				paac = &m_aacTasks_Runtime;
				break;
			case DESTROY:
				paav = &aavTasks_Destroy;
				paac = &m_aacTasks_Destroy;
				break;
			default: continue; break;
			}

			// loop through task functions
			for (std::vector<GAME_TASK>& av : *paav)
			{

				// create task block and add to list
				std::vector<std::packaged_task<signed(AppData&)>> acTaskblock;
				for (GAME_TASK& v : av)
					acTaskblock.push_back(std::packaged_task<signed(AppData&)>(v));

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
		AppData sAppData = {};
		std::vector<std::vector<std::packaged_task<signed(AppData&)>>>* paac = nullptr;
		signed nFuture = APP_FORWARD;

		// restart the game timer
		m_cTimer.restart();

		// execute lists of task blocks
		for (unsigned uIx : {INIT, RUNTIME, DESTROY})
		{
			// any error present ?
			if (nFuture != APP_ERROR) nFuture = APP_FORWARD;

			// set helper pointer
			switch (uIx)
			{
			case INIT: paac = &m_aacTasks_Init; break;
			case RUNTIME: paac = &m_aacTasks_Runtime; break;
			case DESTROY: paac = &m_aacTasks_Destroy; break;
			default: continue; break;
			}

			do
			{
				// update game timer and app data
				m_cTimer.tick();
				sAppData.fDelta = m_cTimer.delta();
				sAppData.fFPS = m_cTimer.fps();
				sAppData.fFPSTotal = m_cTimer.fps_total();
				sAppData.fTotal = m_cTimer.total();

				// loop through task blocks
				for (std::vector<std::packaged_task<signed(AppData&)>>& aac : *paac)
				{
					// pause this task block ?
					if (nFuture == APP_PAUSE) continue;

					// error or quit ?
					if (((nFuture == APP_ERROR) || (nFuture == APP_QUIT)) && (uIx != DESTROY)) continue;

					// loop through tasks
					std::vector<std::future<signed>> acFutures;
					for (std::packaged_task<signed(AppData&)>& ac : aac)
					{
						// reset task, add future
						ac.reset();
						acFutures.push_back(ac.get_future());

						// execute
						ac(sAppData);
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
	std::vector<std::vector<std::packaged_task<signed(AppData&)>>>
		m_aacTasks_Init, m_aacTasks_Runtime, m_aacTasks_Destroy;
	/// <summary>
	/// Game Timer (platform dependent)
	/// </summary>
	GameTimer m_cTimer;
};

#ifdef _WIN32
#define APP_Os App_Windows
#define APP_GfxLib App_D3D12

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
	) : App_Taskhandler(aavTasks_Init, aavTasks_Runtime, aavTasks_Destroy) {}
	virtual ~App_Windows() {}

protected:

	/// <summary>
	/// Init Desktop Window
	/// </summary>
	FUNC_GAME_TASK(OsInit)
	{
		OutputDebugStringA("App_Windows::OsInit");
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
	/// <summary></summary>
	FUNC_GAME_TASK(OsUpdate)
	{
		return APP_FORWARD;
	}
	/// <summary>Handle Window Messages</summary>
	FUNC_GAME_TASK(OsFrame)
	{
		static MSG s_sMsg = { 0 };
		if (PeekMessage(&s_sMsg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&s_sMsg);
			DispatchMessage(&s_sMsg);
			if (s_sMsg.message == WM_QUIT) return APP_QUIT; else return APP_PAUSE;
		}

		return APP_FORWARD;
	}
	/// <summary></summary>
	FUNC_GAME_TASK(OsPreRelease)
	{
		OutputDebugStringA("App_Windows::OsPreRelease");
		return APP_FORWARD;
	}
	/// <summary></summary>
	FUNC_GAME_TASK(OsRelease)
	{
		OutputDebugStringA("App_Windows::OsRelease");
		return APP_FORWARD;
	}

	/// <summary>Windows main procedure</summary>
	static LRESULT CALLBACK WindowProc(HWND pHwnd, UINT uMessage, WPARAM uWparam, LPARAM uLparam)
	{
		AppData* psData = reinterpret_cast<AppData*>(GetWindowLongPtr(pHwnd, GWLP_USERDATA));

		switch (uMessage)
		{
		case WM_CREATE:
		{
			// s ave the app data pointer send via CreateWindow
			LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(uLparam);
			SetWindowLongPtr(pHwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
		}
		return 0;

		case WM_KEYDOWN:
			if (psData)
			{
			}
			return APP_FORWARD;

		case WM_KEYUP:
			if (psData)
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

/// <summary>
/// Main Application
/// </summary>
class App : protected APP_GfxLib
{
public:
	App() : APP_GfxLib(m_aavTasks_Init, m_aavTasks_Runtime, m_aavTasks_Destroy) 
	{
		// .. and execute the app
		Run();
	}
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
		OutputDebugStringA("App::OnRelease");
		return APP_FORWARD;
	}

	static std::vector<std::vector<GAME_TASK>> m_aavTasks_Init;
	static std::vector<std::vector<GAME_TASK>> m_aavTasks_Runtime;
	static std::vector<std::vector<GAME_TASK>> m_aavTasks_Destroy;
};

/// The concurrent scheme of the App 
std::vector<std::vector<GAME_TASK>> App::m_aavTasks_Init = { { APP_Os::OsInit }, { APP_GfxLib::GxInit },  { App::OnInit } };
std::vector<std::vector<GAME_TASK>> App::m_aavTasks_Runtime = {
	{ App::OnUpdate, APP_Os::OsUpdate }, { APP_Os::OsFrame },
	{ App::OnRenderPipeline0, App::OnRenderPipeline1, App::OnRenderPipeline2, App::OnRenderAudio },
	{ App::OnPostRender }
};
std::vector<std::vector<GAME_TASK>> App::m_aavTasks_Destroy = { 
	{ APP_Os::OsPreRelease }, { APP_GfxLib::GxRelease }, { App::OnRelease }, { APP_Os::OsRelease }
};