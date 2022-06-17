// D3D12 Tech Demo
// (c) 2022 by Denis Reischl
// 
// SPDX-License-Identifier: MIT

#include "app_D3D12.h"

HWND App_Windows::m_pHwnd = nullptr;
App_Windows::Client App_Windows::m_sClientSize;
App_D3D12::D3D12_Fields App_D3D12::m_sD3D;

signed App_D3D12::GxInit(AppData& sData)
{
	OutputDebugStringA("App_D3D12::GxInit");
	return InitDirect3D();
}

signed App_D3D12::GxRelease(AppData& sData)
{
	OutputDebugStringA("App_D3D12::GxRelease");
	return APP_FORWARD;
}

signed App_D3D12::InitDirect3D()
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
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&psDxgiFactory)));
	ThrowIfFailed(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_sD3D.psDevice)));
	ThrowIfFailed(m_sD3D.psDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_sD3D.psFence)));

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

	// build d3d tools and resources
	CreateSceneDHeaps();
	CreateConstantBuffers();
	CreateRootSignatures();
	CreateShaders();
	CreateTextures();
	BuildGeometry();

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

	DXGI_SWAP_CHAIN_DESC sSCDc = {};
	sSCDc.BufferDesc.Width = (UINT)m_sClientSize.nW;
	sSCDc.BufferDesc.Height = (UINT)m_sClientSize.nH;
	sSCDc.BufferDesc.RefreshRate.Numerator = 60;
	sSCDc.BufferDesc.RefreshRate.Denominator = 1;
	sSCDc.BufferDesc.Format = m_sD3D.eBackbufferFmt;
	sSCDc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sSCDc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sSCDc.SampleDesc.Count = m_sD3D.b4xMsaaState ? 4 : 1;
	sSCDc.SampleDesc.Quality = m_sD3D.b4xMsaaState ? (m_sD3D.u4xMsaaQuality - 1) : 0;
	sSCDc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sSCDc.BufferCount = nSwapchainBufferN;
	sSCDc.OutputWindow = m_pHwnd;
	sSCDc.Windowed = true;
	sSCDc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sSCDc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	ThrowIfFailed(pcFactory->CreateSwapChain(
		m_sD3D.psCmdQueue.Get(),
		&sSCDc,
		m_sD3D.psSwapchain.GetAddressOf()));

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
	ConstantsScene sConstants = {};

	/// world - view - projection
	{
		// meanwhile const
		const float fRadius = 2.f * (float)abs(sin((double)sData.fTotal * 2.1f)) + 4.f;
		const float fTheta = (float)sin((double)sData.fTotal * 1.7f);
		const float fPhi = (float)cos((double)sData.fTotal * 1.5f);;

		// meanwhile compute here
		XMFLOAT4X4 sWorld, sView, sProj;
		XMStoreFloat4x4(&sWorld, XMMatrixIdentity());
		XMMATRIX sP = XMMatrixPerspectiveFovLH(0.25f * XM_PI, static_cast<float>(m_sClientSize.nW) / static_cast<float>(m_sClientSize.nH), 1.0f, 1000.0f);
		XMStoreFloat4x4(&sProj, sP);

		// Spherical to Cartesian
		float x = fRadius * sinf(fPhi) * cosf(fTheta);
		float z = fRadius * sinf(fPhi) * sinf(fTheta);
		float y = fRadius * cosf(fPhi);

		// build view
		XMVECTOR sPos = XMVectorSet(x, y, z, 1.0f);
		XMVECTOR sTarget = XMVectorZero();
		XMVECTOR sUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

		XMMATRIX sV = XMMatrixLookAtLH(sPos, sTarget, sUp);
		XMStoreFloat4x4(&sView, sV);

		XMMATRIX sW = XMLoadFloat4x4(&sWorld);
		XMMATRIX sPr = XMLoadFloat4x4(&sProj);
		XMMATRIX worldViewProj = sW * sV * sPr;

		XMStoreFloat4x4(&sConstants.sWVP, XMMatrixTranspose(worldViewProj));
	}

	/// time (x - total, y - delta, z - fps total, w - fps)
	{
		XMVECTOR sTime = XMVectorSet(sData.fTotal, sData.fDelta, sData.fFPSTotal, sData.fFPS);
		XMStoreFloat4(&sConstants.sTime, sTime);
	}

	/// viewport (x - topLeftX, y - topLeftY, z - width, w - height)
	{

		XMVECTOR sViewport = XMVectorSet(m_sD3D.sScreenVp.TopLeftX, m_sD3D.sScreenVp.TopLeftY, m_sD3D.sScreenVp.Width, m_sD3D.sScreenVp.Height);
		XMStoreFloat4(&sConstants.sViewport, sViewport);
	}

	/// mouse (x - x position, y - y position, z - buttons (unsigned), w - wheel (unsigned))
	{
		///...	
	}

	// and update
	{
		BYTE* ptData = nullptr;
		ThrowIfFailed(m_sD3D.psBufferUp->Map(0, nullptr, reinterpret_cast<void**>(&ptData)));
		memcpy(&ptData[0], &sConstants, sizeof(ConstantsScene));
		if (m_sD3D.psBufferUp != nullptr) m_sD3D.psBufferUp->Unmap(0, nullptr);
	}

	return APP_FORWARD;
}

signed App_D3D12::Draw(const AppData& sData)
{
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
	m_sD3D.psCmdList->ClearDepthStencilView(m_sD3D.psHeapDSV->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// set render targets
	CD3DX12_CPU_DESCRIPTOR_HANDLE sRtvHandle(m_sD3D.psHeapRTV->GetCPUDescriptorHandleForHeapStart(),
		m_sD3D.nBackbufferI,
		m_sD3D.uRtvDcSz);
	D3D12_CPU_DESCRIPTOR_HANDLE sDsvHandle = m_sD3D.psHeapDSV->GetCPUDescriptorHandleForHeapStart();
	m_sD3D.psCmdList->OMSetRenderTargets(1, &sRtvHandle, true, &sDsvHandle);

	// descriptor heaps, root signature
	ID3D12DescriptorHeap* apsDHeaps[] = { m_sD3D.psConstSRV.Get() };
	m_sD3D.psCmdList->SetDescriptorHeaps(_countof(apsDHeaps), apsDHeaps);
	m_sD3D.psCmdList->SetGraphicsRootSignature(m_sD3D.psRootSign.Get());

	// vertex, index buffer - topology,... and draw
	D3D12_VERTEX_BUFFER_VIEW sVBV = m_sD3D.pcMeshBox->ViewV();
	D3D12_INDEX_BUFFER_VIEW sIBV = m_sD3D.pcMeshBox->ViewI();
	m_sD3D.psCmdList->IASetVertexBuffers(0, 1, &sVBV);
	m_sD3D.psCmdList->IASetIndexBuffer(&sIBV);
	m_sD3D.psCmdList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_sD3D.psCmdList->SetGraphicsRootDescriptorTable(0, m_sD3D.psConstSRV->GetGPUDescriptorHandleForHeapStart());
	m_sD3D.psCmdList->DrawIndexedInstanced(m_sD3D.pcMeshBox->Indices_N(), 1, 0, 0, 0);

	// execute post processing
	ExecutePostProcessing(m_sD3D.psCmdList.Get(), m_sD3D.psRootSignCS.Get(),
		m_sD3D.psPSOCS.Get(), m_sD3D.apsBufferSC[m_sD3D.nBackbufferI].Get());

	// transit to copy destination (is in copy source due to post processing execution), copy, transit to present
	CD3DX12_RB_TRANSITION::ResourceBarrier(m_sD3D.psCmdList.Get(), m_sD3D.apsBufferSC[m_sD3D.nBackbufferI].Get(),
		D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
	m_sD3D.psCmdList->CopyResource(m_sD3D.apsBufferSC[m_sD3D.nBackbufferI].Get(), m_sD3D.psPostMap1.Get());
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

void App_D3D12::ExecutePostProcessing(ID3D12GraphicsCommandList* psCmdList,
	ID3D12RootSignature* psRootSign,
	ID3D12PipelineState* psPSO,
	ID3D12Resource* psInput)
{
	// copy the back-buffer to first post map, transit to generic read
	CD3DX12_RB_TRANSITION::ResourceBarrier(psCmdList, psInput, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
	CD3DX12_RB_TRANSITION::ResourceBarrier(psCmdList, m_sD3D.psPostMap0.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST);
	psCmdList->CopyResource(m_sD3D.psPostMap0.Get(), psInput);
	CD3DX12_RB_TRANSITION::ResourceBarrier(psCmdList, m_sD3D.psPostMap0.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);

	// transit second map to unordered access
	CD3DX12_RB_TRANSITION::ResourceBarrier(psCmdList, m_sD3D.psPostMap1.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	// set root sign, shader inputs
	psCmdList->SetComputeRootSignature(psRootSign);
	psCmdList->SetPipelineState(psPSO);
	psCmdList->SetComputeRootDescriptorTable(0, m_sD3D.psConstSRV->GetGPUDescriptorHandleForHeapStart());
	psCmdList->SetComputeRootDescriptorTable(1, m_sD3D.asPostGpuH[0]);
	psCmdList->SetComputeRootDescriptorTable(2, m_sD3D.asPostGpuH[3]);

	// dispatch (x * 256 (N in compute shader))
	UINT uNumGroupsX = (UINT)ceilf(static_cast<float>(m_sClientSize.nW) / 256.0f);
	psCmdList->Dispatch(uNumGroupsX, (unsigned)m_sClientSize.nH, 1);

	// transit second map to generic read
	CD3DX12_RB_TRANSITION::ResourceBarrier(psCmdList, m_sD3D.psPostMap1.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
}

signed App_D3D12::CreateSceneDHeaps()
{
	// constants shader resource view heaps
	{
		const int nConstantsDcN = 1;
		const int nPostDcN = 4;

		D3D12_DESCRIPTOR_HEAP_DESC sCbvHeapDesc = { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, nConstantsDcN + nPostDcN, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 0 };
		ThrowIfFailed(m_sD3D.psDevice->CreateDescriptorHeap(&sCbvHeapDesc, IID_PPV_ARGS(m_sD3D.psConstSRV.ReleaseAndGetAddressOf())));
		m_sD3D.psConstSRV->SetName(L"constant SRV heap");
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
	m_sD3D.psDevice->CreateConstantBufferView(&sCbvDesc, m_sD3D.psConstSRV->GetCPUDescriptorHandleForHeapStart());

	return APP_FORWARD;
}

signed App_D3D12::CreateRootSignatures()
{
	// render pipeline root signature
	{
		// table
		CD3DX12_DESCRIPTOR_RANGE sCbvTable;
		sCbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

		// param
		CD3DX12_ROOT_PARAMETER sSlotRootPrm[1];
		sSlotRootPrm[0].InitAsDescriptorTable(1, &sCbvTable);

		// description
		CD3DX12_ROOT_SIGNATURE_DESC sRootSigDc(1, sSlotRootPrm, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		// serialize
		ComPtr<ID3DBlob> psRootSig = nullptr;
		ComPtr<ID3DBlob> psErr = nullptr;
		HRESULT nHr = D3D12SerializeRootSignature(&sRootSigDc, D3D_ROOT_SIGNATURE_VERSION_1, psRootSig.GetAddressOf(), psErr.GetAddressOf());

		if (psErr != nullptr) { ::OutputDebugStringA((char*)psErr->GetBufferPointer()); }
		ThrowIfFailed(nHr);

		// and create
		ThrowIfFailed(m_sD3D.psDevice->CreateRootSignature(
			0,
			psRootSig->GetBufferPointer(),
			psRootSig->GetBufferSize(),
			IID_PPV_ARGS(m_sD3D.psRootSign.ReleaseAndGetAddressOf())));

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

		// serialize
		ComPtr<ID3DBlob> psRootSig = nullptr;
		ComPtr<ID3DBlob> psErr = nullptr;
		HRESULT nHr = D3D12SerializeRootSignature(&sRootSigDc, D3D_ROOT_SIGNATURE_VERSION_1,
			psRootSig.GetAddressOf(), psErr.GetAddressOf());

		if (psErr != nullptr) { ::OutputDebugStringA((char*)psErr->GetBufferPointer()); }
		ThrowIfFailed(nHr);

		// and create
		ThrowIfFailed(m_sD3D.psDevice->CreateRootSignature(
			0,
			psRootSig->GetBufferPointer(),
			psRootSig->GetBufferSize(),
			IID_PPV_ARGS(m_sD3D.psRootSignCS.ReleaseAndGetAddressOf())));

		m_sD3D.psRootSignCS->SetName(L"compute RS");
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

	// compute shader
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
			m_sD3D.psDevice->CreateComputePipelineState(&psPsoDc, IID_PPV_ARGS(m_sD3D.psPSOCS.ReleaseAndGetAddressOf())));
		m_sD3D.psPSOCS->SetName(L"compute PSO");
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
			(unsigned)m_sClientSize.nW,
			(unsigned)m_sClientSize.nH,
			1,
			1,
			m_sD3D.eBackbufferFmt,
			{ 1, 0 },
			D3D12_TEXTURE_LAYOUT_UNKNOWN,
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
		};

		const CD3DX12_HEAP_PROPERTIES sPrps(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(m_sD3D.psDevice->CreateCommittedResource(
			&sPrps,
			D3D12_HEAP_FLAG_NONE,
			&sTexDc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_sD3D.psPostMap0)));

		ThrowIfFailed(m_sD3D.psDevice->CreateCommittedResource(
			&sPrps,
			D3D12_HEAP_FLAG_NONE,
			&sTexDc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_sD3D.psPostMap1)));
	}

	// and views
	{
		// set handles
		CD3DX12_CPU_DESCRIPTOR_HANDLE sCpuDc = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_sD3D.psConstSRV->GetCPUDescriptorHandleForHeapStart(), 1, m_sD3D.uCbvSrvUavDcSz);
		CD3DX12_GPU_DESCRIPTOR_HANDLE sGpuDc = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_sD3D.psConstSRV->GetGPUDescriptorHandleForHeapStart(), 1, m_sD3D.uCbvSrvUavDcSz);
		m_sD3D.asPostCpuH[0] = sCpuDc;
		for (signed nI : { 1, 2, 3}) m_sD3D.asPostCpuH[nI] = sCpuDc.Offset(1, m_sD3D.uCbvSrvUavDcSz);
		m_sD3D.asPostGpuH[0] = sGpuDc;
		for (signed nI : { 1, 2, 3}) m_sD3D.asPostGpuH[nI] = sGpuDc.Offset(1, m_sD3D.uCbvSrvUavDcSz);

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

		m_sD3D.psDevice->CreateShaderResourceView(m_sD3D.psPostMap0.Get(), &sSrvDc, m_sD3D.asPostCpuH[0]);
		m_sD3D.psDevice->CreateUnorderedAccessView(m_sD3D.psPostMap0.Get(), nullptr, &sUavDc, m_sD3D.asPostCpuH[1]);

		m_sD3D.psDevice->CreateShaderResourceView(m_sD3D.psPostMap1.Get(), &sSrvDc, m_sD3D.asPostCpuH[2]);
		m_sD3D.psDevice->CreateUnorderedAccessView(m_sD3D.psPostMap1.Get(), nullptr, &sUavDc, m_sD3D.asPostCpuH[3]);
	}

	return APP_FORWARD;
}

signed App_D3D12::BuildGeometry()
{
	std::vector<VertexPosCol> asVertices =
	{
		VertexPosCol({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White) }),
		VertexPosCol({ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Black) }),
		VertexPosCol({ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Red) }),
		VertexPosCol({ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green) }),
		VertexPosCol({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Blue) }),
		VertexPosCol({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Yellow) }),
		VertexPosCol({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Cyan) }),
		VertexPosCol({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Magenta) })
	};

	std::vector<std::uint16_t> auIndices =
	{
		// front face
		0, 1, 2,
		0, 2, 3,

		// back face
		4, 6, 5,
		4, 7, 6,

		// left face
		4, 5, 1,
		4, 1, 0,

		// right face
		3, 2, 6,
		3, 6, 7,

		// top face
		1, 5, 6,
		1, 6, 2,

		// bottom face
		4, 0, 3,
		4, 3, 7
	};

	m_sD3D.pcMeshBox = std::make_unique<Mesh_PosCol>(m_sD3D.psDevice.Get(), m_sD3D.psCmdList.Get(), asVertices, auIndices, "box");

	return APP_FORWARD;
}

/// <summary>D3DCompileFromFile wrapper</summary>
signed App_D3D12::CompileFromFile(
	const std::wstring& atFilename,
	const D3D_SHADER_MACRO& sDefines,
	const std::string& atEntrypoint,
	const std::string& atTarget,
	ComPtr<ID3DBlob>& pcDest)
{
	if (pcDest != nullptr) { OutputDebugStringA("App : Invalid Source Code Arg !"); return APP_ERROR; }

	UINT uFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)  
	uFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	ComPtr<ID3DBlob> pcErrors;
	HRESULT nHr = D3DCompileFromFile(atFilename.c_str(), &sDefines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		atEntrypoint.c_str(), atTarget.c_str(), uFlags, 0, &pcDest, &pcErrors);

	if (pcErrors != nullptr) OutputDebugStringA((char*)pcErrors->GetBufferPointer());

	return nHr;
}
