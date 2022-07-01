// D3D12 Tech Demo
// Copyright © 2022 by Denis Reischl
// 
// SPDX-License-Identifier: MIT

#include "app_D3D12.h"
#include "CompiledShaders\RS_library.hlsl.h"

const wchar_t* s_atHitGroup = L"HitGroup";
const wchar_t* s_atRaygen = L"RayGenerationShader";
const wchar_t* s_atClosestHit = L"ClosestHitShader";
const wchar_t* s_atMiss = L"MissShader";

HWND App_Windows::m_pHwnd = nullptr;
App_Windows::Client App_Windows::m_sClientSize;
App_D3D12::D3D12_Fields App_D3D12::m_sD3D;
App_D3D12::SceneData App_D3D12::m_sScene;

XMFLOAT2 operator+(const XMFLOAT2& sSummand0, const XMFLOAT2& sSummand1) {
	return XMFLOAT2(sSummand0.x + sSummand1.x, sSummand0.y + sSummand1.y);
}
XMFLOAT2 operator*(const XMFLOAT2& sMultiplier, const XMFLOAT2& sMultiplicant) {
	return XMFLOAT2(sMultiplier.x * sMultiplicant.x, sMultiplier.y * sMultiplicant.y);
}
XMFLOAT2 operator*(const XMFLOAT2& sMultiplier, const float& fMultiplicant) {
	return XMFLOAT2(sMultiplier.x * fMultiplicant, sMultiplier.y * fMultiplicant);
}

XMFLOAT3 operator+(const XMFLOAT3& sSummand0, const XMFLOAT3& sSummand1) {
	return XMFLOAT3(sSummand0.x + sSummand1.x, sSummand0.y + sSummand1.y, sSummand0.z + sSummand1.z);
}
XMFLOAT3 operator*(const XMFLOAT3& sMultiplier, const XMFLOAT3& sMultiplicant) {
	return XMFLOAT3(sMultiplier.x * sMultiplicant.x, sMultiplier.y * sMultiplicant.z, sMultiplier.z * sMultiplicant.z);
}
XMFLOAT3 operator*(const XMFLOAT3& sMultiplier, const float& fMultiplicant) {
	return XMFLOAT3(sMultiplier.x * fMultiplicant, sMultiplier.y * fMultiplicant, sMultiplier.z * fMultiplicant);
}

signed App_D3D12::GxInit(AppData& sData)
{
	OutputDebugStringA("App_D3D12::GxInit");
	return InitDirect3D(sData);
}

signed App_D3D12::GxRelease(AppData& sData)
{
	OutputDebugStringA("App_D3D12::GxRelease");
	return APP_FORWARD;
}

signed App_D3D12::InitDirect3D(AppData& sData)
{
	ComPtr<IDXGIFactory4> psDxgiFactory = nullptr;

#if defined(DEBUG) || defined(_DEBUG) 
	// D3D12 debug layer ?
	{
		ComPtr<ID3D12Debug> psDebug;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&psDebug)));
		psDebug->EnableDebugLayer();
	}
#endif

	// create DXGI factory, hardware device and fence
	ThrowIfFailed(CreateDXGIFactory2(0, IID_PPV_ARGS(&psDxgiFactory)));
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&psDxgiFactory)));
	ThrowIfFailed(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_sD3D.psDevice)));
	ThrowIfFailed(m_sD3D.psDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_sD3D.psFence)));

	// confirm the device supports DXR
	D3D12_FEATURE_DATA_D3D12_OPTIONS5 sOpts = {};
	if (FAILED(m_sD3D.psDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &sOpts, sizeof(sOpts)))
		|| sOpts.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
	{
		OutputDebugStringA("App_D3D12 : No DirectX Raytracing support found.\n");

	}
	else
		m_sD3D.bDXRSupport = true;

	// confirm the device supports Shader Model 6.3 or better
	D3D12_FEATURE_DATA_SHADER_MODEL sShaderModel = { D3D_SHADER_MODEL_6_3 };
	if (FAILED(m_sD3D.psDevice->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &sShaderModel, sizeof(sShaderModel)))
		|| sShaderModel.HighestShaderModel < D3D_SHADER_MODEL_6_3)
	{
		OutputDebugStringA("App_D3D12 : No Shader Model 6.3 support.\n");
		m_sD3D.bDXRSupport = false;
	}

	// uncomment to force BlinPhong
	m_sD3D.bDXRSupport = false;

	// get descriptor sizes
	m_sD3D.uRtvDcSz = m_sD3D.psDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_sD3D.uDsvDcSz = m_sD3D.psDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	m_sD3D.uCbvSrvUavDcSz = m_sD3D.psDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_sD3D.uSamplerDcSz = m_sD3D.psDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

	// set MSAA quality
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS sLevels = {
		m_sD3D.eBackbufferFmt, 4, D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE, 0 };
	ThrowIfFailed(m_sD3D.psDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &sLevels, sizeof(sLevels)));
	m_sD3D.u4xMsaaQuality = sLevels.NumQualityLevels;
	assert(m_sD3D.u4xMsaaQuality > 0 && "Unexpected MSAA quality level.");

	// create
	CreateCommandObjects();
	CreateSwapChain(psDxgiFactory.Get());
	CreateMainDHeaps();

	// resize and reset to prepare for initialization
	OnResize();
	ThrowIfFailed(m_sD3D.psCmdList->Reset(m_sD3D.psCmdListAlloc.Get(), nullptr));

	CreateSceneDHeaps();
	CreateConstantBuffers();
	CreateRootSignatures();
	BuildGeometry();

	if (m_sD3D.bDXRSupport)
	{
		CreateDXRStateObject();
		CreateDXRAcceleration();
		BuildDXRShaderTables();

		ThrowIfFailed(m_sD3D.psCmdList->Reset(m_sD3D.psCmdListAlloc.Get(), nullptr));
	}

	// build d3d tools and resources
	CreateShaders();
	CreateTextures();

	// init  scene constants and set basic hex tile offsets
	UpdateConstants(sData);
	OffsetTiles(m_sD3D.psCmdList.Get(), m_sD3D.psRootSignCS.Get(), m_sD3D.psPsoCsHexTrans.Get());

	// execute initialization
	ThrowIfFailed(m_sD3D.psCmdList->Close());
	ID3D12CommandList* cmdsLists[] = { m_sD3D.psCmdList.Get() };
	m_sD3D.psCmdQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// ...and sync
	return FlushCommandQueue();
}

signed App_D3D12::CreateCommandObjects()
{
	// queue
	D3D12_COMMAND_QUEUE_DESC sQueueDc = { D3D12_COMMAND_LIST_TYPE_DIRECT, 0, D3D12_COMMAND_QUEUE_FLAG_NONE, 0 };
	ThrowIfFailed(m_sD3D.psDevice->CreateCommandQueue(&sQueueDc, IID_PPV_ARGS(&m_sD3D.psCmdQueue)));

	// allocator
	ThrowIfFailed(m_sD3D.psDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(m_sD3D.psCmdListAlloc.GetAddressOf())));

	// list 
	ThrowIfFailed(m_sD3D.psDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_sD3D.psCmdListAlloc.Get(),
		nullptr,
		IID_PPV_ARGS(m_sD3D.psCmdList.GetAddressOf())));

	// start closed
	m_sD3D.psCmdList->Close();

	return APP_FORWARD;
}

signed App_D3D12::CreateSwapChain(IDXGIFactory4* pcFactory)
{
	// release previous
	m_sD3D.psSwapchain.Reset();

	// Create a descriptor for the swap chain.
	DXGI_SWAP_CHAIN_DESC1 sSCDc = {};
	sSCDc.Width = (UINT)m_sClientSize.nW;
	sSCDc.Height = (UINT)m_sClientSize.nH;
	sSCDc.Format = m_sD3D.eBackbufferFmt;
	sSCDc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sSCDc.BufferCount = nSwapchainBufferN;
	sSCDc.SampleDesc.Count = m_sD3D.b4xMsaaState ? 4 : 1;
	sSCDc.SampleDesc.Quality = m_sD3D.b4xMsaaState ? (m_sD3D.u4xMsaaQuality - 1) : 0;
	sSCDc.Scaling = DXGI_SCALING_STRETCH;
	sSCDc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sSCDc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
	sSCDc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	DXGI_SWAP_CHAIN_FULLSCREEN_DESC sSCFDc = {};
	sSCFDc.Windowed = TRUE;

	// Create a swap chain for the window.
	ComPtr<IDXGISwapChain1> psSwapchain;
	ThrowIfFailed(pcFactory->CreateSwapChainForHwnd(
		m_sD3D.psCmdQueue.Get(),
		m_pHwnd,
		&sSCDc,
		&sSCFDc,
		nullptr,
		psSwapchain.GetAddressOf()
	));

	ThrowIfFailed(psSwapchain.As(&m_sD3D.psSwapchain));

	// This class does not support exclusive full-screen mode and prevents DXGI from responding to the ALT+ENTER shortcut
	ThrowIfFailed(pcFactory->MakeWindowAssociation(m_pHwnd, DXGI_MWA_NO_ALT_ENTER));

	return APP_FORWARD;
}

signed App_D3D12::FlushCommandQueue()
{
	// inc fence value
	m_sD3D.uFenceCnt++;

	// send signal
	ThrowIfFailed(m_sD3D.psCmdQueue->Signal(m_sD3D.psFence.Get(), m_sD3D.uFenceCnt));

	if (m_sD3D.psFence->GetCompletedValue() < m_sD3D.uFenceCnt)
	{
		HANDLE pEventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (pEventHandle == nullptr) { ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError())); }

		// fire event and wait for GPU
		ThrowIfFailed(m_sD3D.psFence->SetEventOnCompletion(m_sD3D.uFenceCnt, pEventHandle));
		WaitForSingleObject(pEventHandle, INFINITE);
		CloseHandle(pEventHandle);
	}

	return APP_FORWARD;
}

signed App_D3D12::CreateMainDHeaps()
{
	// swapchain back buffer view
	{
		D3D12_DESCRIPTOR_HEAP_DESC sHeapRtvDc = { D3D12_DESCRIPTOR_HEAP_TYPE_RTV, nSwapchainBufferN, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0 };
		ThrowIfFailed(m_sD3D.psDevice->CreateDescriptorHeap(&sHeapRtvDc, IID_PPV_ARGS(m_sD3D.psHeapRTV.GetAddressOf())));
	}
	// depth stencil view
	{
		D3D12_DESCRIPTOR_HEAP_DESC sHeapDsvDc = { D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0 };
		ThrowIfFailed(m_sD3D.psDevice->CreateDescriptorHeap(&sHeapDsvDc, IID_PPV_ARGS(m_sD3D.psHeapDSV.GetAddressOf())));
	}
	return APP_FORWARD;
}

signed App_D3D12::OnResize()
{
	assert(m_sD3D.psDevice);
	assert(m_sD3D.psSwapchain);
	assert(m_sD3D.psCmdListAlloc);

	// flush and reset
	FlushCommandQueue();
	ThrowIfFailed(m_sD3D.psCmdList->Reset(m_sD3D.psCmdListAlloc.Get(), nullptr));

	// release previous
	for (int i = 0; i < nSwapchainBufferN; ++i)
		m_sD3D.apsBufferSC[i].Reset();
	m_sD3D.psBufferDS.Reset();

	// resize swapchain, set index to zero
	ThrowIfFailed(m_sD3D.psSwapchain->ResizeBuffers(
		nSwapchainBufferN,
		(UINT)m_sClientSize.nW, (UINT)m_sClientSize.nH,
		m_sD3D.eBackbufferFmt,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));
	m_sD3D.nBackbufferI = 0;

	// create render target views
	CD3DX12_CPU_DESCRIPTOR_HANDLE sRtvHeapHandle(m_sD3D.psHeapRTV->GetCPUDescriptorHandleForHeapStart());
	for (UINT uI = 0; uI < (UINT)nSwapchainBufferN; uI++)
	{
		ThrowIfFailed(m_sD3D.psSwapchain->GetBuffer(uI, IID_PPV_ARGS(&m_sD3D.apsBufferSC[uI])));
		m_sD3D.psDevice->CreateRenderTargetView(m_sD3D.apsBufferSC[uI].Get(), nullptr, sRtvHeapHandle);
		sRtvHeapHandle.Offset(1, m_sD3D.uRtvDcSz);
	}

	// create the depthstencil buffer and view
	D3D12_RESOURCE_DESC sDepthstencilDc = { D3D12_RESOURCE_DIMENSION_TEXTURE2D, 0,
		(UINT)m_sClientSize.nW, (UINT)m_sClientSize.nH, 1, 1, m_sD3D.eDepthstencilFmt,
		{
			(m_sD3D.b4xMsaaState) ? (UINT)4 : (UINT)1,
			m_sD3D.b4xMsaaState ? (m_sD3D.u4xMsaaQuality - 1) : (UINT)0
		},
		D3D12_TEXTURE_LAYOUT_UNKNOWN, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
	};

	D3D12_CLEAR_VALUE sClear = { m_sD3D.eDepthstencilFmt, { 1.0f, 0 } };
	const CD3DX12_HEAP_PROPERTIES sPrps(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(m_sD3D.psDevice->CreateCommittedResource(
		&sPrps,
		D3D12_HEAP_FLAG_NONE,
		&sDepthstencilDc,
		D3D12_RESOURCE_STATE_COMMON,
		&sClear,
		IID_PPV_ARGS(m_sD3D.psBufferDS.GetAddressOf())));

	m_sD3D.psDevice->CreateDepthStencilView(m_sD3D.psBufferDS.Get(), nullptr, m_sD3D.psHeapDSV->GetCPUDescriptorHandleForHeapStart());

	// transit to depth write
	CD3DX12_RB_TRANSITION::ResourceBarrier(m_sD3D.psCmdList.Get(), m_sD3D.psBufferDS.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);

	// ... and execute
	ThrowIfFailed(m_sD3D.psCmdList->Close());
	ID3D12CommandList* cmdsLists[] = { m_sD3D.psCmdList.Get() };
	m_sD3D.psCmdQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// ..and sync
	FlushCommandQueue();

	// update viewport and scissor rectangle
	m_sD3D.sScreenVp = { 0, 0, static_cast<float>(m_sClientSize.nW), static_cast<float>(m_sClientSize.nH), 0.0f, 1.0f };
	m_sD3D.sScissorRc = { 0, 0, (long)m_sClientSize.nW, (long)m_sClientSize.nH };

	return APP_FORWARD;
}

signed App_D3D12::UpdateConstants(const AppData& sData)
{
	static float s_fTimeOld = 0.f;
	float fTimeEl = sData.fTotal - s_fTimeOld;
	static float s_fTmp = 0.f;
	s_fTmp += fTimeEl;

	/// world - view - projection
	{
		// get controller 0 state
		XINPUT_STATE sState = {};
		uint32_t uR = XInputGetState(0, &sState);
		if (uR == ERROR_SUCCESS)
		{
			// position xz
			float fThX = ((float)sState.Gamepad.sThumbLX / 32767.f) * fTimeEl * m_sScene.fAccelTran;
			float fThY = ((float)sState.Gamepad.sThumbLY / 32767.f) * fTimeEl * m_sScene.fAccelTran;
			m_sScene.sCamVelo.x += fThX * cosf(m_sScene.fYaw) - fThY * sinf(m_sScene.fYaw);
			m_sScene.sCamVelo.z += fThX * sinf(m_sScene.fYaw) + fThY * cosf(m_sScene.fYaw);

			// height y
			m_sScene.sCamVelo.y += ((float)sState.Gamepad.bRightTrigger / 256.f) * fTimeEl * m_sScene.fAccelTran;
			m_sScene.sCamVelo.y -= ((float)sState.Gamepad.bLeftTrigger / 256.f) * fTimeEl * m_sScene.fAccelTran;

			// yaw, pitch
			m_sScene.fYaw -= ((float)sState.Gamepad.sThumbRX / 32767.f) * fTimeEl * m_sScene.fAccelRot;
			m_sScene.fYaw = fmod(m_sScene.fYaw, XM_2PI);
			m_sScene.fPitch = ((float)sState.Gamepad.sThumbRY / 32767.f) * XM_PIDIV2;
		}

		// word view projection
		XMFLOAT4X4 sWorld, sView, sProj;
		XMStoreFloat4x4(&sWorld, XMMatrixIdentity());
		XMMATRIX sP = XMMatrixPerspectiveFovLH(0.25f * XM_PI, static_cast<float>(m_sClientSize.nW) / static_cast<float>(m_sClientSize.nH), 1.0f, 1000.0f);
		XMStoreFloat4x4(&sProj, sP);

		// add velo, translate, decelerate		
		m_sScene.sCamPos = m_sScene.sCamPos + m_sScene.sCamVelo;
		XMMATRIX sV =
			XMMatrixTranslation(-m_sScene.sCamPos.x, -m_sScene.sCamPos.y, -m_sScene.sCamPos.z) *
			XMMatrixRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), m_sScene.fYaw) *
			XMMatrixRotationAxis(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), m_sScene.fPitch);
		XMStoreFloat4x4(&sView, sV);
		m_sScene.sCamVelo = m_sScene.sCamVelo * m_sScene.fDrag;
		XMVECTOR sCamPos = XMVectorSet(m_sScene.sCamPos.x, m_sScene.sCamPos.y, m_sScene.sCamPos.z, 0.f);
		XMVECTOR sCamVelo = XMVectorSet(m_sScene.sCamVelo.x, m_sScene.sCamVelo.y, m_sScene.sCamVelo.z, 0.f);
		XMStoreFloat4(&m_sScene.sConstants.sCamPos, sCamPos);
		XMStoreFloat4(&m_sScene.sConstants.sCamVelo, sCamVelo);

		XMMATRIX sW = XMLoadFloat4x4(&sWorld);
		XMMATRIX sPr = XMLoadFloat4x4(&sProj);
		XMMATRIX sWVP = sW * sV * sPr;

		// store world view projection and inverse
		XMStoreFloat4x4(&m_sScene.sConstants.sWVP, XMMatrixTranspose(sWVP));
		XMStoreFloat4x4(&m_sScene.sConstants.sWVPrInv, XMMatrixInverse(nullptr, XMMatrixTranspose(sWVP)));
	}

	/// time (x - total, y - delta, z - fps total, w - fps)
	{
		XMVECTOR sTime = XMVectorSet(sData.fTotal, sData.fDelta, sData.fFPSTotal, sData.fFPS);
		XMStoreFloat4(&m_sScene.sConstants.sTime, sTime);
	}

	/// viewport (x - topLeftX, y - topLeftY, z - width, w - height)
	{

		XMVECTOR sViewport = XMVectorSet(m_sD3D.sScreenVp.TopLeftX, m_sD3D.sScreenVp.TopLeftY, m_sD3D.sScreenVp.Width, m_sD3D.sScreenVp.Height);
		XMStoreFloat4(&m_sScene.sConstants.sViewport, sViewport);
	}

	/// mouse (x - x position, y - y position, z - buttons (uint), w - wheel (uint))
	{
		///...	
	}

	/// hex uv
	{
		// get position xz
		XMFLOAT2 sXY = XMFLOAT2{ m_sScene.sCamPos.x, m_sScene.sCamPos.z };

		// get uv
		XMFLOAT2 sUV = m_sScene.sHexUV = HexUV(sXY.x, sXY.y);

		// get rounded uv = hex center (not fully mathematically correct but enough for our purpose)
		float fUc = m_sScene.sHexUVc.x = round(sUV.x);
		float fVc = m_sScene.sHexUVc.y = round(sUV.y);

		// get cartesian hex center
		sXY = HexXY(fUc, fVc);

		// store to constants
		XMVECTOR sUVv = XMVectorSet(sXY.x, sXY.y, sUV.x, sUV.y);
		XMStoreFloat4(&m_sScene.sConstants.sHexUV, sUVv);

		// loop through tiles by index
		for (unsigned uIx(0); uIx < min(m_sScene.uInstN, (unsigned)m_sScene.aafTilePos.size()); uIx++)
		{
			// get tile uv relative to new center
			XMFLOAT2 sTileXY = XMFLOAT2{ m_sScene.aafTilePos[uIx].x - sXY.x, m_sScene.aafTilePos[uIx].y - sXY.y };
			XMFLOAT2 sTileUV = HexUV(sTileXY.x, sTileXY.y);

			// convert axial (uv) to cube coords (qrs)
			float fQ = round(sTileUV.x);
			float fR = -round(sTileUV.y);
			float fS = -fQ - fR;

			// get cube distance
			float fDistCube = (abs(fQ) + abs(fR) + abs(fS)) / 2.f;

			// off rim ?
			if (fDistCube > float(m_sScene.uAmbitN))
			{
				// get offset to old center
				sTileXY = XMFLOAT2{ m_sScene.aafTilePos[uIx].x - m_sScene.sHexXYc.x, m_sScene.aafTilePos[uIx].y - m_sScene.sHexXYc.y };

				// negate tile offset
				sTileXY.x *= -1.f;
				sTileXY.y *= -1.f;

				// set new to rim
				m_sScene.aafTilePos[uIx].x = m_sScene.sHexXYc.x + sTileXY.x;
				m_sScene.aafTilePos[uIx].y = m_sScene.sHexXYc.y + sTileXY.y;
				m_sScene.aafTilePos[uIx].x += sXY.x - m_sScene.sHexXYc.x;
				m_sScene.aafTilePos[uIx].y += sXY.y - m_sScene.sHexXYc.y;
			}
		}

		// set hex center as old for next frame
		m_sScene.sHexXYc = sXY;

		XMVECTOR sHexData = XMVectorSet((float)m_sScene.uBaseVtxN, 0, 0, 0);
		XMStoreUInt4(&m_sScene.sConstants.sHexData, sHexData);
	}

	// and update
	{
		BYTE* ptData = nullptr;
		ThrowIfFailed(m_sD3D.psBufferUp->Map(0, nullptr, reinterpret_cast<void**>(&ptData)));
		memcpy(&ptData[0], &m_sScene.sConstants, sizeof(ConstantsScene));
		if (m_sD3D.psBufferUp != nullptr) m_sD3D.psBufferUp->Unmap(0, nullptr);
	}

	s_fTimeOld = sData.fTotal;

	return APP_FORWARD;
}

void App_D3D12::Clear()
{
	auto psCmdList = m_sD3D.psCmdList.Get();

	// Clear the views.
	CD3DX12_CPU_DESCRIPTOR_HANDLE sRtvHandle(m_sD3D.psHeapRTV->GetCPUDescriptorHandleForHeapStart(),
		m_sD3D.nBackbufferI,
		m_sD3D.uRtvDcSz);
	D3D12_CPU_DESCRIPTOR_HANDLE sDsvHandle = m_sD3D.psHeapDSV->GetCPUDescriptorHandleForHeapStart();
	psCmdList->OMSetRenderTargets(1, &sRtvHandle, FALSE, &sDsvHandle);
	psCmdList->ClearDepthStencilView(sDsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Set the viewport and scissor rect.
	m_sD3D.psCmdList->RSSetViewports(1, &m_sD3D.sScreenVp);
	m_sD3D.psCmdList->RSSetScissorRects(1, &m_sD3D.sScissorRc);
}

signed App_D3D12::Draw(const AppData& sData)
{
	if (m_sD3D.bDXRSupport)
	{
		DrawDXR();
		return APP_FORWARD;
	}

	// reset
	ThrowIfFailed(m_sD3D.psCmdListAlloc->Reset());
	ThrowIfFailed(m_sD3D.psCmdList->Reset(m_sD3D.psCmdListAlloc.Get(), m_sD3D.psPSO->Get()));
	
	// transit to render target
	CD3DX12_RB_TRANSITION::ResourceBarrier(m_sD3D.psCmdList.Get(), m_sD3D.apsBufferSC[m_sD3D.nBackbufferI].Get(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

	// viewport, scissor rect
	m_sD3D.psCmdList->RSSetViewports(1, &m_sD3D.sScreenVp);
	m_sD3D.psCmdList->RSSetScissorRects(1, &m_sD3D.sScissorRc);

	// clear
	const float afColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	m_sD3D.psCmdList->ClearRenderTargetView(CD3DX12_CPU_DESCRIPTOR_HANDLE(
		m_sD3D.psHeapRTV->GetCPUDescriptorHandleForHeapStart(),
		m_sD3D.nBackbufferI,
		m_sD3D.uRtvDcSz), afColor, 0, nullptr);
	Clear();

	// descriptor heaps, root signature
	ID3D12DescriptorHeap* apsDHeaps[] = { m_sD3D.psHeapSRV.Get() };
	m_sD3D.psCmdList->SetDescriptorHeaps(_countof(apsDHeaps), apsDHeaps);
	m_sD3D.psCmdList->SetGraphicsRootSignature(m_sD3D.psRootSign.Get());

	// vertex, index buffer - topology,... and draw
	D3D12_VERTEX_BUFFER_VIEW sVBV = m_sD3D.pcHexMesh->ViewV();
	D3D12_INDEX_BUFFER_VIEW sIBV = m_sD3D.pcHexMesh->ViewI();
	m_sD3D.psCmdList->IASetVertexBuffers(0, 1, &sVBV);
	m_sD3D.psCmdList->IASetIndexBuffer(&sIBV);
	m_sD3D.psCmdList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_sD3D.psCmdList->SetGraphicsRootDescriptorTable(0, m_sD3D.asCbvSrvUavGpuH[(uint)CbvSrvUav_Heap_Idc::SceneConstants]);
	m_sD3D.psCmdList->SetGraphicsRootDescriptorTable(1, m_sD3D.asCbvSrvUavGpuH[(uint)CbvSrvUav_Heap_Idc::TileOffsetSrv]);
	//m_sD3D.psCmdList->DrawIndexedInstanced(m_sD3D.pcHexMesh->Indices_N(), m_sD3D.pcHexMesh->Instances_N(), 0, 0, 0);
	m_sD3D.psCmdList->DrawIndexedInstanced(m_sD3D.pcHexMesh->Indices_N(), 1, 0, 0, 0);

	// update the hex tiles, first the offsets, then the tiles ( move that later... )
	UpdateHexOffsets(D3D12_RESOURCE_STATE_GENERIC_READ);
	OffsetTiles(m_sD3D.psCmdList.Get(), m_sD3D.psRootSignCS.Get(), m_sD3D.psPsoCsHexTrans.Get());

	// execute post processing
	ExecutePost(m_sD3D.psCmdList.Get(), m_sD3D.psRootSignCS.Get(),
		m_sD3D.psPsoCsPost.Get(), m_sD3D.apsBufferSC[m_sD3D.nBackbufferI].Get());

	// transit to copy destination (is in copy source due to post processing execution), copy, transit to present
	CD3DX12_RB_TRANSITION::ResourceBarrier(m_sD3D.psCmdList.Get(), m_sD3D.apsBufferSC[m_sD3D.nBackbufferI].Get(),
		D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
	m_sD3D.psCmdList->CopyResource(m_sD3D.apsBufferSC[m_sD3D.nBackbufferI].Get(), m_sD3D.psRenderMap1.Get());
	CD3DX12_RB_TRANSITION::ResourceBarrier(m_sD3D.psCmdList.Get(), m_sD3D.apsBufferSC[m_sD3D.nBackbufferI].Get(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);

	// close list and execute
	ThrowIfFailed(m_sD3D.psCmdList->Close());
	ID3D12CommandList* asCmdLists[] = { m_sD3D.psCmdList.Get() };
	m_sD3D.psCmdQueue->ExecuteCommandLists(_countof(asCmdLists), asCmdLists);

	// present and swap
	ThrowIfFailed(m_sD3D.psSwapchain->Present(0, 0));
	m_sD3D.nBackbufferI = (m_sD3D.nBackbufferI + 1) % nSwapchainBufferN;

	// sync
	return FlushCommandQueue();
}

void App_D3D12::ExecutePost(ID3D12GraphicsCommandList* psCmdList,
	ID3D12RootSignature* psRootSign,
	ID3D12PipelineState* psPSO,
	ID3D12Resource* psInput)
{
	// copy the back-buffer to first post map, transit to generic read
	CD3DX12_RB_TRANSITION::ResourceBarrier(psCmdList, psInput, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
	CD3DX12_RB_TRANSITION::ResourceBarrier(psCmdList, m_sD3D.psRenderMap0.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST);
	psCmdList->CopyResource(m_sD3D.psRenderMap0.Get(), psInput);
	CD3DX12_RB_TRANSITION::ResourceBarrier(psCmdList, m_sD3D.psRenderMap0.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);

	// transit second map to unordered access
	CD3DX12_RB_TRANSITION::ResourceBarrier(psCmdList, m_sD3D.psRenderMap1.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	// set root sign, shader inputs
	psCmdList->SetComputeRootSignature(psRootSign);
	psCmdList->SetPipelineState(psPSO);
	psCmdList->SetComputeRootDescriptorTable(0, m_sD3D.psHeapSRV->GetGPUDescriptorHandleForHeapStart());
	psCmdList->SetComputeRootDescriptorTable(1, m_sD3D.asCbvSrvUavGpuH[(uint)CbvSrvUav_Heap_Idc::PostMap0Srv]);
	psCmdList->SetComputeRootDescriptorTable(2, m_sD3D.asCbvSrvUavGpuH[(uint)CbvSrvUav_Heap_Idc::PostMap1Uav]);

	// dispatch (x * 256 (N in compute shader))
	UINT uNumGroupsX = (UINT)ceilf(static_cast<float>(m_sClientSize.nW) / 256.0f);
	psCmdList->Dispatch(uNumGroupsX, (uint)m_sClientSize.nH, 1);

	// transit second map to generic read
	CD3DX12_RB_TRANSITION::ResourceBarrier(psCmdList, m_sD3D.psRenderMap1.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
}

void App_D3D12::OffsetTiles(ID3D12GraphicsCommandList* psCmdList,
	ID3D12RootSignature* psRootSign,
	ID3D12PipelineState* psPSO)
{	
	// transit to unordered access
	CD3DX12_RB_TRANSITION::ResourceBarrier(psCmdList, m_sD3D.pcHexMesh->Vertex_Buffer(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	ID3D12DescriptorHeap* apsDHeaps[] = { m_sD3D.psHeapSRV.Get() };
	psCmdList->SetDescriptorHeaps(_countof(apsDHeaps), apsDHeaps);

	// set root sign, shader inputs
	psCmdList->SetComputeRootSignature(psRootSign);
	psCmdList->SetPipelineState(psPSO);
	psCmdList->SetComputeRootDescriptorTable(0, m_sD3D.psHeapSRV->GetGPUDescriptorHandleForHeapStart());
	psCmdList->SetComputeRootDescriptorTable(1, m_sD3D.asCbvSrvUavGpuH[(uint)CbvSrvUav_Heap_Idc::TileOffsetSrv]);
	psCmdList->SetComputeRootDescriptorTable(2, m_sD3D.asCbvSrvUavGpuH[(uint)CbvSrvUav_Heap_Idc::MeshVtcUav]);

	// dispatch
	UINT uNumGroupsX = (UINT)m_sScene.uInstN * m_sScene.uBaseVtxN;
	psCmdList->Dispatch(uNumGroupsX, 1, 1);

	// transit second map to generic read
	CD3DX12_RB_TRANSITION::ResourceBarrier(psCmdList, m_sD3D.pcHexMesh->Vertex_Buffer(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
}

signed App_D3D12::CreateSceneDHeaps()
{
	// constants shader resource view heaps
	{
		const int nConstantsDcN = 1;
		const int nPostDcN = 4;

		D3D12_DESCRIPTOR_HEAP_DESC sCbvHeapDesc = { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, nConstantsDcN + nPostDcN + 1, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 0 };
		ThrowIfFailed(m_sD3D.psDevice->CreateDescriptorHeap(&sCbvHeapDesc, IID_PPV_ARGS(m_sD3D.psHeapSRV.ReleaseAndGetAddressOf())));
		m_sD3D.psHeapSRV->SetName(L"constant SRV heap");

		// set handles for scene constant buffer
		m_sD3D.asCbvSrvUavCpuH[(uint)CbvSrvUav_Heap_Idc::SceneConstants] = m_sD3D.psHeapSRV->GetCPUDescriptorHandleForHeapStart();
		m_sD3D.asCbvSrvUavGpuH[(uint)CbvSrvUav_Heap_Idc::SceneConstants] = m_sD3D.psHeapSRV->GetGPUDescriptorHandleForHeapStart();
	}
	// sampler setup
	{
		D3D12_DESCRIPTOR_HEAP_DESC sSamplerDc = { D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, static_cast<UINT>(1), D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 0 };
		ThrowIfFailed(m_sD3D.psDevice->CreateDescriptorHeap(&sSamplerDc, IID_PPV_ARGS(m_sD3D.psSampler.ReleaseAndGetAddressOf())));
		m_sD3D.psSampler->SetName(L"sampler heap");
	}

	return APP_FORWARD;
}

signed App_D3D12::CreateConstantBuffers()
{
	const CD3DX12_HEAP_PROPERTIES sPrpsU(D3D12_HEAP_TYPE_UPLOAD);
	const CD3DX12_RESOURCE_DESC sDesc = CD3DX12_RESOURCE_DESC::Buffer(Align8Bit(sizeof(ConstantsScene)) * 1);

	// create buffer
	ThrowIfFailed(m_sD3D.psDevice->CreateCommittedResource(
		&sPrpsU,
		D3D12_HEAP_FLAG_NONE,
		&sDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_sD3D.psBufferUp)));

	// get address and align
	D3D12_GPU_VIRTUAL_ADDRESS uAddress = m_sD3D.psBufferUp->GetGPUVirtualAddress();
	int uCBIx = 0;
	uAddress += uCBIx * Align8Bit(sizeof(ConstantsScene));

	// create view
	D3D12_CONSTANT_BUFFER_VIEW_DESC sCbvDesc;
	sCbvDesc.BufferLocation = uAddress;
	sCbvDesc.SizeInBytes = Align8Bit(sizeof(ConstantsScene));
	m_sD3D.psDevice->CreateConstantBufferView(&sCbvDesc, m_sD3D.psHeapSRV->GetCPUDescriptorHandleForHeapStart());

	return APP_FORWARD;
}

signed App_D3D12::CreateRootSignatures()
{
	// render pipeline root signature
	{
		// table
		CD3DX12_DESCRIPTOR_RANGE sCbvTable;
		sCbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

		CD3DX12_DESCRIPTOR_RANGE sSrvTable;
		sSrvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		// param
		CD3DX12_ROOT_PARAMETER asSlotRootPrm[6];
		asSlotRootPrm[0].InitAsDescriptorTable(1, &sCbvTable);
		asSlotRootPrm[1].InitAsDescriptorTable(1, &sSrvTable);

		// description
		CD3DX12_ROOT_SIGNATURE_DESC sRootSigDc(2, asSlotRootPrm, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ThrowIfFailed(CreateRootSignature(m_sD3D.psDevice.Get(), sRootSigDc, &m_sD3D.psRootSign));

		m_sD3D.psRootSign->SetName(L"main pipe RS");
	}

	// compute shader root signature
	{
		// tables
		CD3DX12_DESCRIPTOR_RANGE sCbvTable;
		sCbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

		CD3DX12_DESCRIPTOR_RANGE sSrvTable;
		sSrvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_DESCRIPTOR_RANGE sUavTable;
		sUavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

		// params
		CD3DX12_ROOT_PARAMETER asSlotRootParam[3];
		asSlotRootParam[0].InitAsDescriptorTable(1, &sCbvTable);
		asSlotRootParam[1].InitAsDescriptorTable(1, &sSrvTable);
		asSlotRootParam[2].InitAsDescriptorTable(1, &sUavTable);

		// description
		CD3DX12_ROOT_SIGNATURE_DESC sRootSigDc(3, asSlotRootParam,
			0, nullptr,
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ThrowIfFailed(CreateRootSignature(m_sD3D.psDevice.Get(), sRootSigDc, &m_sD3D.psRootSignCS));

		m_sD3D.psRootSignCS->SetName(L"compute RS");
	}

	// DXR root
	if (m_sD3D.bDXRSupport)
	{
		// tables
		CD3DX12_DESCRIPTOR_RANGE sUavTable;
		sUavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

		CD3DX12_DESCRIPTOR_RANGE sCbvTable;
		sCbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

		CD3DX12_ROOT_PARAMETER asSlotRootParam[3];
		asSlotRootParam[0].InitAsDescriptorTable(1, &sUavTable);
		asSlotRootParam[1].InitAsShaderResourceView(0);
		asSlotRootParam[2].InitAsDescriptorTable(1, &sCbvTable);

		CD3DX12_ROOT_SIGNATURE_DESC sGlobalDc(3, asSlotRootParam);
		ThrowIfFailed(CreateRootSignature(m_sD3D.psDevice.Get(), sGlobalDc, &m_sD3D.psDXRRootSign));
	}

	return APP_FORWARD;
}

signed App_D3D12::CreateShaders()
{
	// main render pipeline shaders
	{
		HRESULT nHr = S_OK;
		ComPtr<ID3DBlob> psVsByteCode = nullptr, psPsByteCode = nullptr, psNullBlob = nullptr;
		std::vector<D3D12_INPUT_ELEMENT_DESC> asLayout;

		ThrowIfFailed(D3DReadFileToBlob(L"VS_phong.cso", &psVsByteCode));
		ThrowIfFailed(D3DReadFileToBlob(L"PS_phong.cso", &psPsByteCode));

		asLayout =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		m_sD3D.psPSO = std::make_shared<D3D12_PSO>(
			m_sD3D.psDevice,
			nHr,
			m_sD3D.psRootSign,
			psVsByteCode,
			psPsByteCode,
			psNullBlob,
			psNullBlob,
			psNullBlob,
			asLayout
			);
		ThrowIfFailed(nHr);
	}

	// compute shader post
	{
		// compile...
		D3D_SHADER_MACRO sMacro = {};
		ComPtr<ID3DBlob> psCsByteCode = nullptr;
		ThrowIfFailed(D3DReadFileToBlob(L"CS_post.cso", &psCsByteCode));

		// Create compute pipeline state
		D3D12_COMPUTE_PIPELINE_STATE_DESC psPsoDc = {};
		psPsoDc.pRootSignature = m_sD3D.psRootSignCS.Get();
		psPsoDc.CS = { reinterpret_cast<BYTE*>(psCsByteCode->GetBufferPointer()), psCsByteCode->GetBufferSize() };

		ThrowIfFailed(
			m_sD3D.psDevice->CreateComputePipelineState(&psPsoDc, IID_PPV_ARGS(m_sD3D.psPsoCsPost.ReleaseAndGetAddressOf())));
		m_sD3D.psPsoCsPost->SetName(L"compute post PSO");
	}

	// compute shader post
	{
		// compile...
		D3D_SHADER_MACRO sMacro = {};
		ComPtr<ID3DBlob> psCsByteCode = nullptr;
		ThrowIfFailed(D3DReadFileToBlob(L"CS_hextrans.cso", &psCsByteCode));

		// Create compute pipeline state
		D3D12_COMPUTE_PIPELINE_STATE_DESC psPsoDc = {};
		psPsoDc.pRootSignature = m_sD3D.psRootSignCS.Get();
		psPsoDc.CS = { reinterpret_cast<BYTE*>(psCsByteCode->GetBufferPointer()), psCsByteCode->GetBufferSize() };

		ThrowIfFailed(
			m_sD3D.psDevice->CreateComputePipelineState(&psPsoDc, IID_PPV_ARGS(m_sD3D.psPsoCsHexTrans.ReleaseAndGetAddressOf())));
		m_sD3D.psPsoCsHexTrans->SetName(L"compute hex trans PSO");
	}

	return APP_FORWARD;
}

signed App_D3D12::CreateTextures()
{
	// post textures
	{
		D3D12_RESOURCE_DESC sTexDc = {
			D3D12_RESOURCE_DIMENSION_TEXTURE2D,
			0,
			(uint)m_sClientSize.nW,
			(uint)m_sClientSize.nH,
			1,
			1,
			m_sD3D.eBackbufferFmt,
			{ 1, 0 },
			D3D12_TEXTURE_LAYOUT_UNKNOWN,
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
		};

		const CD3DX12_HEAP_PROPERTIES sPrps(D3D12_HEAP_TYPE_DEFAULT);
		const D3D12_RESOURCE_STATES eState = m_sD3D.bDXRSupport ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : D3D12_RESOURCE_STATE_GENERIC_READ;
		ThrowIfFailed(m_sD3D.psDevice->CreateCommittedResource(
			&sPrps,
			D3D12_HEAP_FLAG_NONE,
			&sTexDc,
			eState,
			nullptr,
			IID_PPV_ARGS(&m_sD3D.psRenderMap0)));

		ThrowIfFailed(m_sD3D.psDevice->CreateCommittedResource(
			&sPrps,
			D3D12_HEAP_FLAG_NONE,
			&sTexDc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_sD3D.psRenderMap1)));
	}

	// and views
	{
		// set handles
		CD3DX12_CPU_DESCRIPTOR_HANDLE sCpuDc = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_sD3D.psHeapSRV->GetCPUDescriptorHandleForHeapStart(), 1, m_sD3D.uCbvSrvUavDcSz);
		CD3DX12_GPU_DESCRIPTOR_HANDLE sGpuDc = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_sD3D.psHeapSRV->GetGPUDescriptorHandleForHeapStart(), 1, m_sD3D.uCbvSrvUavDcSz);
		m_sD3D.asCbvSrvUavCpuH[(uint)CbvSrvUav_Heap_Idc::PostMap0Srv] = sCpuDc;
		m_sD3D.asCbvSrvUavCpuH[(uint)CbvSrvUav_Heap_Idc::PostMap0Uav] = sCpuDc.Offset(1, m_sD3D.uCbvSrvUavDcSz);
		m_sD3D.asCbvSrvUavCpuH[(uint)CbvSrvUav_Heap_Idc::PostMap1Srv] = sCpuDc.Offset(1, m_sD3D.uCbvSrvUavDcSz);
		m_sD3D.asCbvSrvUavCpuH[(uint)CbvSrvUav_Heap_Idc::PostMap1Uav] = sCpuDc.Offset(1, m_sD3D.uCbvSrvUavDcSz);
		m_sD3D.asCbvSrvUavGpuH[(uint)CbvSrvUav_Heap_Idc::PostMap0Srv] = sGpuDc;
		m_sD3D.asCbvSrvUavGpuH[(uint)CbvSrvUav_Heap_Idc::PostMap0Uav] = sGpuDc.Offset(1, m_sD3D.uCbvSrvUavDcSz);
		m_sD3D.asCbvSrvUavGpuH[(uint)CbvSrvUav_Heap_Idc::PostMap1Srv] = sGpuDc.Offset(1, m_sD3D.uCbvSrvUavDcSz);
		m_sD3D.asCbvSrvUavGpuH[(uint)CbvSrvUav_Heap_Idc::PostMap1Uav] = sGpuDc.Offset(1, m_sD3D.uCbvSrvUavDcSz);

		// create SRVs, UAVs
		D3D12_SHADER_RESOURCE_VIEW_DESC sSrvDc = {
			m_sD3D.eBackbufferFmt,
			D3D12_SRV_DIMENSION_TEXTURE2D,
			D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING, {}
		};
		sSrvDc.Texture2D = { 0, 1, 0, 0.0f };

		D3D12_UNORDERED_ACCESS_VIEW_DESC sUavDc = {
			m_sD3D.eBackbufferFmt,
			D3D12_UAV_DIMENSION_TEXTURE2D, {}
		};
		m_sD3D.psDevice->CreateShaderResourceView(m_sD3D.psRenderMap0.Get(), &sSrvDc, m_sD3D.asCbvSrvUavCpuH[(uint)CbvSrvUav_Heap_Idc::PostMap0Srv]);
		m_sD3D.psDevice->CreateUnorderedAccessView(m_sD3D.psRenderMap0.Get(), nullptr, &sUavDc, m_sD3D.asCbvSrvUavCpuH[(uint)CbvSrvUav_Heap_Idc::PostMap0Uav]);

		m_sD3D.psDevice->CreateShaderResourceView(m_sD3D.psRenderMap1.Get(), &sSrvDc, m_sD3D.asCbvSrvUavCpuH[(uint)CbvSrvUav_Heap_Idc::PostMap1Srv]);
		m_sD3D.psDevice->CreateUnorderedAccessView(m_sD3D.psRenderMap1.Get(), nullptr, &sUavDc, m_sD3D.asCbvSrvUavCpuH[(uint)CbvSrvUav_Heap_Idc::PostMap1Uav]);
	}
	return APP_FORWARD;
}

signed App_D3D12::BuildGeometry()
{
	// base hexagon
	{
		// create basic hexagon with 6 triangles
		// (each triangle one side and center)
		// set normalized distance (= size unit = 1.f) to y
		std::vector<XMFLOAT3> asHexVtc =
		{
			{                  0.0f, 0.0f,  0.0f }, // center
			{                  0.0f, 1.0f, -1.0f }, // top
			{  m_sScene.fMinW * .5f, 1.0f, -0.5f }, // top right
			{  m_sScene.fMinW * .5f, 1.0f,  0.5f }, // bottom right
			{                  0.0f, 1.0f,  1.0f }, // bottom
			{ -m_sScene.fMinW * .5f, 1.0f,  0.5f }, // bottom left
			{ -m_sScene.fMinW * .5f, 1.0f, -0.5f }, // top left
		};
		std::vector<std::uint32_t> auHexIdc =
		{
			1, 0, 2, // top-center-top right
			2, 0, 3, // top right-center-bottom right
			3, 0, 4, // ....
			4, 0, 5,
			5, 0, 6,
			6, 0, 1
		};

		// subdivide each triangle to 3 triangles
		for (unsigned uI(0); uI < 6; uI++)
		{
			// move the last triangle
			std::array<uint32_t, 3> auTriangle;
			for (unsigned uJ : {2, 1, 0})
			{
				auTriangle[uJ] = auHexIdc.back();
				auHexIdc.pop_back();
			}

			// create the new vertices
			std::array<XMFLOAT3, 3> asNewVtc;
			for (unsigned uJ : {0, 1, 2})
			{
				asNewVtc[uJ] = {
					0.5f * (asHexVtc[auTriangle[uJ]].x + asHexVtc[auTriangle[(uJ + 1) % 3]].x),
					0.5f * (asHexVtc[auTriangle[uJ]].y + asHexVtc[auTriangle[(uJ + 1) % 3]].y),
					0.5f * (asHexVtc[auTriangle[uJ]].z + asHexVtc[auTriangle[(uJ + 1) % 3]].z)
				};
			}

			// create new indices, push back non-existing vertices
			std::array<uint32_t, 3> auNewIdc;
			for (unsigned uJ : {0, 1, 2})
			{
				unsigned uK(0);
				bool bExists = false;
				for (XMFLOAT3& sV : asHexVtc)
				{
					if ((sV.x == asNewVtc[uJ].x) &&
						(sV.y == asNewVtc[uJ].y) &&
						(sV.z == asNewVtc[uJ].z))
					{
						// vertex already exists, set index
						auNewIdc[uJ] = uK;
						bExists = true;
						break;
					}
					uK++;
				}

				// not existing ? set index, add to vertices
				if (!bExists)
				{
					auNewIdc[uJ] = (uint32_t)asHexVtc.size();
					asHexVtc.push_back(asNewVtc[uJ]);
				}

			}

			// create the new triangles
			auHexIdc.insert(auHexIdc.begin(), auNewIdc[2]);
			auHexIdc.insert(auHexIdc.begin(), auNewIdc[1]);
			auHexIdc.insert(auHexIdc.begin(), auNewIdc[0]);
			auHexIdc.insert(auHexIdc.begin(), auTriangle[2]);
			auHexIdc.insert(auHexIdc.begin(), auNewIdc[1]);
			auHexIdc.insert(auHexIdc.begin(), auNewIdc[2]);
			auHexIdc.insert(auHexIdc.begin(), auNewIdc[1]);
			auHexIdc.insert(auHexIdc.begin(), auTriangle[1]);
			auHexIdc.insert(auHexIdc.begin(), auNewIdc[0]);
			auHexIdc.insert(auHexIdc.begin(), auNewIdc[2]);
			auHexIdc.insert(auHexIdc.begin(), auNewIdc[0]);
			auHexIdc.insert(auHexIdc.begin(), auTriangle[0]);
		}

		// convert to d3d vertex
		std::vector<VertexPosCol> asHexagonVtc;
		for (XMFLOAT3& sV : asHexVtc)
		{
			// color values : xy - local position; z - distance to mesh center; w - normalized distance to mesh center ( = hexagon )
			XMFLOAT4 sCol = { sV.x, sV.z, sqrt(sV.x * sV.x + sV.z * sV.z), sV.y };

			// sV.y (preliminary normalized dist) to zero
			sV.y = 0.f;

			asHexagonVtc.push_back({ sV, sCol });
		}

		// get number of hexagons
		unsigned uAmbitTileN = 6;
		for (unsigned uI(0); uI < m_sScene.uAmbitN; uI++)
		{
			m_sScene.uInstN += uAmbitTileN;
			uAmbitTileN += 6;
		}


		// add the hexagons to the vertices/indices, first hex stays as base
		m_sScene.uBaseVtxN = (unsigned)asHexagonVtc.size();
		const unsigned uBaseIdxN = (unsigned)auHexIdc.size();
		for (unsigned uI(0); uI < m_sScene.uInstN; uI++)
		{
			// we simply add the vertices with zero xy offset, this will be set by compute shader eventually
			for (unsigned uJ(0); uJ < m_sScene.uBaseVtxN; uJ++)
				asHexagonVtc.push_back(asHexagonVtc[uJ]);

			// we add the base number of vertices to the new index
			for (unsigned uJ(0); uJ < uBaseIdxN; uJ++)
				auHexIdc.push_back(auHexIdc[uJ] + (uI + 1) * m_sScene.uBaseVtxN);
		}

		// store vertex number to constants
		// m_sScene.sConstants.sHexData.x = (UINT32)m_sScene.uBaseVtxN;

		// get uav handles, create mesh
		m_sD3D.asCbvSrvUavCpuH[(uint)CbvSrvUav_Heap_Idc::MeshVtcUav] =
			CD3DX12_CPU_DESCRIPTOR_HANDLE(m_sD3D.psHeapSRV->GetCPUDescriptorHandleForHeapStart(), (uint)CbvSrvUav_Heap_Idc::MeshVtcUav, m_sD3D.uCbvSrvUavDcSz);
		m_sD3D.asCbvSrvUavGpuH[(uint)CbvSrvUav_Heap_Idc::MeshVtcUav] =
			CD3DX12_GPU_DESCRIPTOR_HANDLE(m_sD3D.psHeapSRV->GetGPUDescriptorHandleForHeapStart(), (uint)CbvSrvUav_Heap_Idc::MeshVtcUav, m_sD3D.uCbvSrvUavDcSz);
		m_sD3D.pcHexMesh = std::make_unique<Mesh_PosCol>(m_sD3D.psDevice.Get(), m_sD3D.psCmdList.Get(), asHexagonVtc, auHexIdc,
			m_sD3D.asCbvSrvUavCpuH[(uint)CbvSrvUav_Heap_Idc::MeshVtcUav], m_sScene.uInstN, "hexagon");
	}

	// create a hex tile offset buffer (containing the xy positions of the tiles (float2))
	{
		D3D12_RESOURCE_DESC sBufDc = {
			D3D12_RESOURCE_DIMENSION_BUFFER,
			0,
			Align8Bit(m_sScene.uInstN * m_sScene.uVec4Sz),
			1,
			1,
			1,
			DXGI_FORMAT_UNKNOWN,
			{ 1, 0 },
			D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
			D3D12_RESOURCE_FLAG_NONE
		};

		// create buffer and upload buffer
		CD3DX12_HEAP_PROPERTIES sPrps(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(m_sD3D.psDevice->CreateCommittedResource(
			&sPrps,
			D3D12_HEAP_FLAG_NONE,
			&sBufDc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(m_sD3D.psTileLayout.ReleaseAndGetAddressOf())));
		sPrps.Type = D3D12_HEAP_TYPE_UPLOAD;
		ThrowIfFailed(m_sD3D.psDevice->CreateCommittedResource(
			&sPrps,
			D3D12_HEAP_FLAG_NONE,
			&sBufDc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(m_sD3D.psTileLayoutUp.ReleaseAndGetAddressOf())));

		// create the tile offsets, const tile size 1.f
		for (unsigned uInstIx(0); uInstIx < Align8Bit(m_sScene.uInstN); uInstIx++)
		{
			XMFLOAT2 sInstOffset = XMFLOAT2{ 0.f, 0.f };
			if ((uInstIx > 0) && (uInstIx < m_sScene.uInstN))
			{
				unsigned uMul = (uInstIx - 1) / 6;
				unsigned uOffset = (uInstIx - 1) % 6;

				// get the ambit (TODO find a more elegant way here...)
				unsigned uAmTN = 6;
				unsigned uAmbit = 1;
				unsigned uIx = (uInstIx - 1);
				while (uAmTN <= uIx)
				{
					uIx -= uAmTN;
					uAmTN += 6;
					uAmbit++;
				}

				// move
				sInstOffset = HexNext(m_sScene.fTileSz, uOffset) * (float)uAmbit;
				if (uIx > uOffset)
					sInstOffset = sInstOffset + HexNext(m_sScene.fTileSz, uOffset + 2) * (float)(uIx / 6);
			}
			m_sScene.aafTilePos.push_back(XMFLOAT4(sInstOffset.x, sInstOffset.y, 0.f, 0.f));
		}

		// backup initial positions
		m_sScene.aafTilePosInit.insert(m_sScene.aafTilePosInit.begin(), m_sScene.aafTilePos.begin(), m_sScene.aafTilePos.end());

		// and update the constant buffer
		UpdateHexOffsets(D3D12_RESOURCE_STATE_COPY_DEST);

		// get handle
		m_sD3D.asCbvSrvUavCpuH[(uint)CbvSrvUav_Heap_Idc::TileOffsetSrv] =
			CD3DX12_CPU_DESCRIPTOR_HANDLE(m_sD3D.psHeapSRV->GetCPUDescriptorHandleForHeapStart(), (uint)CbvSrvUav_Heap_Idc::TileOffsetSrv, m_sD3D.uCbvSrvUavDcSz);
		m_sD3D.asCbvSrvUavGpuH[(uint)CbvSrvUav_Heap_Idc::TileOffsetSrv] =
			CD3DX12_GPU_DESCRIPTOR_HANDLE(m_sD3D.psHeapSRV->GetGPUDescriptorHandleForHeapStart(), (uint)CbvSrvUav_Heap_Idc::TileOffsetSrv, m_sD3D.uCbvSrvUavDcSz);

		// create SRV
		D3D12_SHADER_RESOURCE_VIEW_DESC sSrvDc = {
			DXGI_FORMAT_UNKNOWN,
			D3D12_SRV_DIMENSION_BUFFER,
			D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING, {}
		};
		sSrvDc.Buffer = { 0, m_sScene.uInstN, m_sScene.uVec4Sz, D3D12_BUFFER_SRV_FLAG_NONE };
		CD3DX12_CPU_DESCRIPTOR_HANDLE sSrvHeapHandle(m_sD3D.psHeapRTV->GetCPUDescriptorHandleForHeapStart());
		m_sD3D.psDevice->CreateShaderResourceView(m_sD3D.psTileLayout.Get(), &sSrvDc, m_sD3D.asCbvSrvUavCpuH[(uint)CbvSrvUav_Heap_Idc::TileOffsetSrv]);
	}

	return APP_FORWARD;
}

signed App_D3D12::CreateDXRStateObject()
{
	CD3DX12_STATE_OBJECT_DESC sObjectDc{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

	// load shader code, define method exports
	auto psLibrary = sObjectDc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
	D3D12_SHADER_BYTECODE sLibCode = CD3DX12_SHADER_BYTECODE((void*)g_pRS_library, ARRAYSIZE(g_pRS_library));
	psLibrary->SetDXILLibrary(&sLibCode);
	psLibrary->DefineExport(s_atRaygen);
	psLibrary->DefineExport(s_atClosestHit);
	psLibrary->DefineExport(s_atMiss);

	// set hit group sub object
	auto pcHitG = sObjectDc.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
	pcHitG->SetClosestHitShaderImport(s_atClosestHit);
	pcHitG->SetHitGroupExport(s_atHitGroup);
	pcHitG->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

	// set shader config sub object (float4 color float2 barycentrics)
	auto pcShaderConf = sObjectDc.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
	pcShaderConf->Config(4 * sizeof(float), 2 * sizeof(float));

	// set global root signature sub object
	auto pcGlobalRootSign = sObjectDc.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	pcGlobalRootSign->SetRootSignature(m_sD3D.psDXRRootSign.Get());

	// set pipeline config sub object (~ primary rays only = 1)
	auto pcPipelineConf = sObjectDc.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
	pcPipelineConf->Config(1);

	ThrowIfFailed(m_sD3D.psDevice.Get()->CreateStateObject(sObjectDc, IID_PPV_ARGS(m_sD3D.psDXRStateObject.ReleaseAndGetAddressOf())));

	return APP_FORWARD;
}

signed App_D3D12::CreateDXRAcceleration()
{
	auto psDevice = m_sD3D.psDevice.Get();
	auto psCmdList = m_sD3D.psCmdList.Get();
	auto psCmdListAlloc = m_sD3D.psCmdListAlloc.Get();

	// reset command list
	psCmdList->Reset(psCmdListAlloc, nullptr);

	D3D12_VERTEX_BUFFER_VIEW sVBV = m_sD3D.pcHexMesh->ViewV();
	D3D12_INDEX_BUFFER_VIEW sIBV = m_sD3D.pcHexMesh->ViewI();

	D3D12_RAYTRACING_GEOMETRY_DESC sGeoDc = {};
	sGeoDc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	sGeoDc.Triangles.IndexBuffer = sIBV.BufferLocation;
	sGeoDc.Triangles.IndexCount = static_cast<UINT>(sIBV.SizeInBytes) / sizeof(UINT16);
	sGeoDc.Triangles.IndexFormat = sIBV.Format;
	sGeoDc.Triangles.Transform3x4 = 0;
	sGeoDc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
	sGeoDc.Triangles.VertexCount = static_cast<UINT>(sVBV.SizeInBytes) / sizeof(VertexPosCol);
	sGeoDc.Triangles.VertexBuffer.StartAddress = sVBV.BufferLocation;
	sGeoDc.Triangles.VertexBuffer.StrideInBytes = sVBV.StrideInBytes;

	sGeoDc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

	// get required sizes - top
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO sTopInfoDc = {};
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS sTopInputsDc = {};
	sTopInputsDc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	sTopInputsDc.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	sTopInputsDc.NumDescs = 1;
	sTopInputsDc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	sTopInputsDc.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	sTopInputsDc.NumDescs = 1;
	sTopInputsDc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	psDevice->GetRaytracingAccelerationStructurePrebuildInfo(&sTopInputsDc, &sTopInfoDc);
	assert(sTopInfoDc.ResultDataMaxSizeInBytes > 0);

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO sBotInfoDc = {};
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS sBotInputsDc = sTopInputsDc;
	sBotInputsDc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	sBotInputsDc.pGeometryDescs = &sGeoDc;
	psDevice->GetRaytracingAccelerationStructurePrebuildInfo(&sBotInputsDc, &sBotInfoDc);
	assert(sBotInfoDc.ResultDataMaxSizeInBytes > 0);

	// scratch storage on the GPU required during acceleration structure build
	ComPtr<ID3D12Resource> psScratch;
	AllocateUAVBuffer(psDevice, max(sTopInfoDc.ScratchDataSizeInBytes, sBotInfoDc.ScratchDataSizeInBytes), &psScratch, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");

	// create acceleration structure resources
	AllocateUAVBuffer(psDevice, sBotInfoDc.ResultDataMaxSizeInBytes, &m_sD3D.psBotAccelStruct,
		D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, L"BottomLevelAccelerationStructure");
	AllocateUAVBuffer(psDevice, sTopInfoDc.ResultDataMaxSizeInBytes, &m_sD3D.psTopAccelStruct,
		D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, L"TopLevelAccelerationStructure");

	// create an upload buffer
	D3D12_RAYTRACING_INSTANCE_DESC sInstDc = {};
	sInstDc.Transform[0][0] = sInstDc.Transform[1][1] = sInstDc.Transform[2][2] = 1;
	sInstDc.InstanceMask = 1;
	sInstDc.AccelerationStructure = m_sD3D.psBotAccelStruct->GetGPUVirtualAddress();
	ComPtr<ID3D12Resource> psInstanceDsc;
	AllocateUploadBuffer(psDevice, &sInstDc, sizeof(sInstDc), &psInstanceDsc, L"InstanceDescs");

	// Bottom Level Acceleration Structure desc
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC sBotBuildDc = {};
	{
		sBotBuildDc.Inputs = sBotInputsDc;
		sBotBuildDc.ScratchAccelerationStructureData = psScratch->GetGPUVirtualAddress();
		sBotBuildDc.DestAccelerationStructureData = m_sD3D.psBotAccelStruct->GetGPUVirtualAddress();
	}

	// Top Level Acceleration Structure desc
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC sTopBuildDc = sBotBuildDc;
	{
		sTopInputsDc.InstanceDescs = psInstanceDsc->GetGPUVirtualAddress();
		sTopBuildDc.Inputs = sTopInputsDc;
		sTopBuildDc.DestAccelerationStructureData = m_sD3D.psTopAccelStruct->GetGPUVirtualAddress();
		sTopBuildDc.ScratchAccelerationStructureData = psScratch->GetGPUVirtualAddress();
	}

	// Build acceleration structure.
	psCmdList->BuildRaytracingAccelerationStructure(&sBotBuildDc, 0, nullptr);

	CD3DX12_RESOURCE_BARRIER sBarrier = CD3DX12_RESOURCE_BARRIER::UAV(m_sD3D.psBotAccelStruct.Get());
	psCmdList->ResourceBarrier(1, &sBarrier);
	psCmdList->BuildRaytracingAccelerationStructure(&sTopBuildDc, 0, nullptr);

	// execute
	ThrowIfFailed(m_sD3D.psCmdList->Close());
	ID3D12CommandList* cmdsLists[] = { m_sD3D.psCmdList.Get() };
	m_sD3D.psCmdQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// sync
	return FlushCommandQueue();
}

void App_D3D12::BuildDXRShaderTables()
{
	auto psDevice = m_sD3D.psDevice.Get();
	UINT uIdSz = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	void* pvRayGenId, * pvMissId, * pvHitGroupId;

	auto GetShaderIdentifiers = [&](auto* psObjectProp)
	{
		pvRayGenId = psObjectProp->GetShaderIdentifier(s_atRaygen);
		pvMissId = psObjectProp->GetShaderIdentifier(s_atMiss);
		pvHitGroupId = psObjectProp->GetShaderIdentifier(s_atHitGroup);
	};

	// get shader identifiers
	ComPtr<ID3D12StateObjectProperties> psObjectProp;
	ThrowIfFailed(m_sD3D.psDXRStateObject.As(&psObjectProp));
	GetShaderIdentifiers(psObjectProp.Get());

	// Ray gen shader table
	{
		ShaderTable cRayGenShaderTable(psDevice, 1, uIdSz, L"RayGenShaderTable");
		cRayGenShaderTable.Add(ShaderRecord(pvRayGenId, uIdSz));
		m_sD3D.psRayGenTable = cRayGenShaderTable.GetResource();
	}

	// Miss shader table
	{
		ShaderTable cMissShaderTable(psDevice, 1, uIdSz, L"MissShaderTable");
		cMissShaderTable.Add(ShaderRecord(pvMissId, uIdSz));
		m_sD3D.psMissTable = cMissShaderTable.GetResource();
	}

	// Hit group shader table
	{
		ShaderTable cHitGroupShaderTable(psDevice, 1, uIdSz, L"HitGroupShaderTable");
		cHitGroupShaderTable.Add(ShaderRecord(pvHitGroupId, uIdSz));
		m_sD3D.psHitGroupTable = cHitGroupShaderTable.GetResource();
	}
}

void App_D3D12::DoRaytracing()
{
	auto psCmdList = m_sD3D.psCmdList.Get();

	auto DispatchRays = [&](auto* psCmdList, auto* stateObject, auto* dispatchDesc)
	{
		// Since each shader table has only one shader record, the stride is same as the size.
		dispatchDesc->Depth = 1;
		dispatchDesc->Width = (uint)m_sClientSize.nW;
		dispatchDesc->Height = (uint)m_sClientSize.nH;
		dispatchDesc->HitGroupTable.StartAddress = m_sD3D.psHitGroupTable->GetGPUVirtualAddress();
		dispatchDesc->HitGroupTable.SizeInBytes = m_sD3D.psHitGroupTable->GetDesc().Width;
		dispatchDesc->HitGroupTable.StrideInBytes = dispatchDesc->HitGroupTable.SizeInBytes;
		dispatchDesc->MissShaderTable.StartAddress = m_sD3D.psMissTable->GetGPUVirtualAddress();
		dispatchDesc->MissShaderTable.SizeInBytes = m_sD3D.psMissTable->GetDesc().Width;
		dispatchDesc->MissShaderTable.StrideInBytes = dispatchDesc->MissShaderTable.SizeInBytes;
		dispatchDesc->RayGenerationShaderRecord.StartAddress = m_sD3D.psRayGenTable->GetGPUVirtualAddress();
		dispatchDesc->RayGenerationShaderRecord.SizeInBytes = m_sD3D.psRayGenTable->GetDesc().Width;
		psCmdList->SetPipelineState1(stateObject);
		psCmdList->DispatchRays(dispatchDesc);
	};

	psCmdList->SetComputeRootSignature(m_sD3D.psDXRRootSign.Get());

	// Bind the heaps, acceleration structure and dispatch rays.    
	D3D12_DISPATCH_RAYS_DESC sDispDc = {};
	ID3D12DescriptorHeap* apsDHeaps[] = { m_sD3D.psHeapSRV.Get() };
	psCmdList->SetDescriptorHeaps(_countof(apsDHeaps), apsDHeaps);
	psCmdList->SetComputeRootDescriptorTable(0, m_sD3D.asCbvSrvUavGpuH[(uint)CbvSrvUav_Heap_Idc::PostMap0Uav]);
	psCmdList->SetComputeRootShaderResourceView(1, m_sD3D.psTopAccelStruct->GetGPUVirtualAddress());
	psCmdList->SetComputeRootDescriptorTable(2, m_sD3D.psHeapSRV->GetGPUDescriptorHandleForHeapStart());
	DispatchRays(psCmdList, m_sD3D.psDXRStateObject.Get(), &sDispDc);
}

void App_D3D12::DrawDXR()
{
	// reset
	ThrowIfFailed(m_sD3D.psCmdListAlloc->Reset());
	ThrowIfFailed(m_sD3D.psCmdList->Reset(m_sD3D.psCmdListAlloc.Get(), nullptr));

	// transit to render target
	CD3DX12_RB_TRANSITION::ResourceBarrier(m_sD3D.psCmdList.Get(), m_sD3D.apsBufferSC[m_sD3D.nBackbufferI].Get(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

	Clear();
	DoRaytracing();
	RenderMap2Backbuffer();

	// transit to present
	CD3DX12_RB_TRANSITION::ResourceBarrier(m_sD3D.psCmdList.Get(), m_sD3D.apsBufferSC[m_sD3D.nBackbufferI].Get(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);

	// close list and execute
	ThrowIfFailed(m_sD3D.psCmdList->Close());
	ID3D12CommandList* asCmdLists[] = { m_sD3D.psCmdList.Get() };
	m_sD3D.psCmdQueue->ExecuteCommandLists(_countof(asCmdLists), asCmdLists);

	// present and swap
	ThrowIfFailed(m_sD3D.psSwapchain->Present(0, 0));
	m_sD3D.nBackbufferI = (m_sD3D.nBackbufferI + 1) % nSwapchainBufferN;

	// sync
	FlushCommandQueue();
}

void App_D3D12::RenderMap2Backbuffer()
{
	auto psCmdList = m_sD3D.psCmdList.Get();
	auto psTarget = m_sD3D.apsBufferSC[m_sD3D.nBackbufferI].Get();

	D3D12_RESOURCE_BARRIER preCopyBarriers[2];
	preCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(psTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
	preCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(m_sD3D.psRenderMap0.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	psCmdList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);

	psCmdList->CopyResource(psTarget, m_sD3D.psRenderMap0.Get());

	D3D12_RESOURCE_BARRIER postCopyBarriers[2];
	postCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(psTarget, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
	postCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(m_sD3D.psRenderMap0.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	psCmdList->ResourceBarrier(ARRAYSIZE(postCopyBarriers), postCopyBarriers);
}


