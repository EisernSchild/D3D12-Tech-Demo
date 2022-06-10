// D3D12 Tech Demo
// (c) 2022 by Denis Reischl
// 
// SPDX-License-Identifier: MIT

#include "app_D3D12.h"

HWND App_Windows::m_pHwnd = nullptr;
App_Windows::Client App_Windows::m_sClientSize;
App_D3D12::D3D12_Fields App_D3D12::m_sFields;

signed App_D3D12::GxInit(AppData& sInfo)
{
	OutputDebugStringA("App_D3D12::GxInit");
	return InitDirect3D();
}

signed App_D3D12::GxRelease(AppData& sInfo)
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
	ThrowIfFailed(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_sFields.pcDevice)));
	ThrowIfFailed(m_sFields.pcDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_sFields.psFence)));

	// get descriptor sizes
	m_sFields.uRtvDcSz = m_sFields.pcDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_sFields.uDsvDcSz = m_sFields.pcDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	m_sFields.uCbvSrvUavDcSz = m_sFields.pcDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// set MSAA quality
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS sLevels = {
		m_sFields.eBackbufferFmt, 4, D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE, 0 };
	ThrowIfFailed(m_sFields.pcDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &sLevels, sizeof(sLevels)));
	m_sFields.u4xMsaaQuality = sLevels.NumQualityLevels;
	assert(m_sFields.u4xMsaaQuality > 0 && "Unexpected MSAA quality level.");

	// create
	CreateCommandObjects();
	CreateSwapChain(psDxgiFactory.Get());
	CreateDHeaps();

	// resize and reset to prepare for initialization
	OnResize();
	ThrowIfFailed(m_sFields.psCmdList->Reset(m_sFields.psCmdListAlloc.Get(), nullptr));

	// build d3d tools and resources
	BuildDescriptorHeaps();
	BuildConstantBuffers();
	BuildRootSignature();
	BuildShaders();
	BuildGeometry();

	// execute initialization
	ThrowIfFailed(m_sFields.psCmdList->Close());
	ID3D12CommandList* cmdsLists[] = { m_sFields.psCmdList.Get() };
	m_sFields.psCmdQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// ...and sync
	FlushCommandQueue();

	return APP_FORWARD;
}

signed App_D3D12::CreateCommandObjects()
{
	// queue
	D3D12_COMMAND_QUEUE_DESC sQueueDc = { D3D12_COMMAND_LIST_TYPE_DIRECT, 0, D3D12_COMMAND_QUEUE_FLAG_NONE, 0 };
	ThrowIfFailed(m_sFields.pcDevice->CreateCommandQueue(&sQueueDc, IID_PPV_ARGS(&m_sFields.psCmdQueue)));

	// allocator
	ThrowIfFailed(m_sFields.pcDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(m_sFields.psCmdListAlloc.GetAddressOf())));

	// list 
	ThrowIfFailed(m_sFields.pcDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_sFields.psCmdListAlloc.Get(),
		nullptr,
		IID_PPV_ARGS(m_sFields.psCmdList.GetAddressOf())));

	// start closed
	m_sFields.psCmdList->Close();

	return APP_FORWARD;
}

signed App_D3D12::CreateSwapChain(IDXGIFactory4* pcFactory)
{
	// release previous
	m_sFields.psSwapchain.Reset();

	DXGI_SWAP_CHAIN_DESC sSCDc = {};
	sSCDc.BufferDesc.Width = (UINT)m_sClientSize.nW;
	sSCDc.BufferDesc.Height = (UINT)m_sClientSize.nH;
	sSCDc.BufferDesc.RefreshRate.Numerator = 60;
	sSCDc.BufferDesc.RefreshRate.Denominator = 1;
	sSCDc.BufferDesc.Format = m_sFields.eBackbufferFmt;
	sSCDc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sSCDc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sSCDc.SampleDesc.Count = m_sFields.b4xMsaaState ? 4 : 1;
	sSCDc.SampleDesc.Quality = m_sFields.b4xMsaaState ? (m_sFields.u4xMsaaQuality - 1) : 0;
	sSCDc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sSCDc.BufferCount = nSwapchainBufferN;
	sSCDc.OutputWindow = m_pHwnd;
	sSCDc.Windowed = true;
	sSCDc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sSCDc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	ThrowIfFailed(pcFactory->CreateSwapChain(
		m_sFields.psCmdQueue.Get(),
		&sSCDc,
		m_sFields.psSwapchain.GetAddressOf()));

	return APP_FORWARD;
}

signed App_D3D12::FlushCommandQueue()
{
	// inc fence value
	m_sFields.uFenceCnt++;

	// send signal
	ThrowIfFailed(m_sFields.psCmdQueue->Signal(m_sFields.psFence.Get(), m_sFields.uFenceCnt));

	if (m_sFields.psFence->GetCompletedValue() < m_sFields.uFenceCnt)
	{
		HANDLE eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (eventHandle == nullptr) { ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError())); }

		// fire event and wait for GPU
		ThrowIfFailed(m_sFields.psFence->SetEventOnCompletion(m_sFields.uFenceCnt, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	return APP_FORWARD;
}

signed App_D3D12::CreateDHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC sHeapRtvDc = { D3D12_DESCRIPTOR_HEAP_TYPE_RTV, nSwapchainBufferN, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0 };
	ThrowIfFailed(m_sFields.pcDevice->CreateDescriptorHeap(&sHeapRtvDc, IID_PPV_ARGS(m_sFields.psHeapRtv.GetAddressOf())));

	D3D12_DESCRIPTOR_HEAP_DESC sHeapDsvDc = { D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0 };
	ThrowIfFailed(m_sFields.pcDevice->CreateDescriptorHeap( &sHeapDsvDc, IID_PPV_ARGS(m_sFields.psHeapDsv.GetAddressOf())));
	
	return APP_FORWARD;
}

signed App_D3D12::OnResize()
{
	assert(m_sFields.pcDevice);
	assert(m_sFields.psSwapchain);
	assert(m_sFields.psCmdListAlloc);

	// flush and reset
	FlushCommandQueue();
	ThrowIfFailed(m_sFields.psCmdList->Reset(m_sFields.psCmdListAlloc.Get(), nullptr));

	// release previous
	for (int i = 0; i < nSwapchainBufferN; ++i)
		m_sFields.apsBufferSC[i].Reset();
	m_sFields.psBufferDS.Reset();

	// resize swapchain, set index to zero
	ThrowIfFailed(m_sFields.psSwapchain->ResizeBuffers(
		nSwapchainBufferN,
		(UINT)m_sClientSize.nW, (UINT)m_sClientSize.nH,
		m_sFields.eBackbufferFmt,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));
	m_sFields.nBackbufferI = 0;


	CD3DX12_CPU_DESCRIPTOR_HANDLE sRtvHeapHandle(m_sFields.psHeapRtv->GetCPUDescriptorHandleForHeapStart());
	for (UINT uI = 0; uI < (UINT)nSwapchainBufferN; uI++)
	{
		ThrowIfFailed(m_sFields.psSwapchain->GetBuffer(uI, IID_PPV_ARGS(&m_sFields.apsBufferSC[uI])));
		m_sFields.pcDevice->CreateRenderTargetView(m_sFields.apsBufferSC[uI].Get(), nullptr, sRtvHeapHandle);
		sRtvHeapHandle.Offset(1, m_sFields.uRtvDcSz);
	}

	// create the depthstencil buffer and view
	D3D12_RESOURCE_DESC sDepthstencilDc;
	sDepthstencilDc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	sDepthstencilDc.Alignment = 0;
	sDepthstencilDc.Width = (UINT)m_sClientSize.nW;
	sDepthstencilDc.Height = (UINT)m_sClientSize.nH;
	sDepthstencilDc.DepthOrArraySize = 1;
	sDepthstencilDc.MipLevels = 1;
	sDepthstencilDc.Format = m_sFields.eDepthstencilFmt;
	sDepthstencilDc.SampleDesc.Count = m_sFields.b4xMsaaState ? 4 : 1;
	sDepthstencilDc.SampleDesc.Quality = m_sFields.b4xMsaaState ? (m_sFields.u4xMsaaQuality - 1) : 0;
	sDepthstencilDc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	sDepthstencilDc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE sClear = { m_sFields.eDepthstencilFmt, { 1.0f, 0 } };
	const CD3DX12_HEAP_PROPERTIES sPrps(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(m_sFields.pcDevice->CreateCommittedResource(
		&sPrps,
		D3D12_HEAP_FLAG_NONE,
		&sDepthstencilDc,
		D3D12_RESOURCE_STATE_COMMON,
		&sClear,
		IID_PPV_ARGS(m_sFields.psBufferDS.GetAddressOf())));

	m_sFields.pcDevice->CreateDepthStencilView(m_sFields.psBufferDS.Get(), nullptr, m_sFields.psHeapDsv->GetCPUDescriptorHandleForHeapStart());

	// transit the resource to be used as a depth buffer
	const CD3DX12_RESOURCE_BARRIER sResBr = CD3DX12_RESOURCE_BARRIER::Transition(m_sFields.psBufferDS.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	m_sFields.psCmdList->ResourceBarrier(1, &sResBr);

	// ... and execute
	ThrowIfFailed(m_sFields.psCmdList->Close());
	ID3D12CommandList* cmdsLists[] = { m_sFields.psCmdList.Get() };
	m_sFields.psCmdQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// ..and sync
	FlushCommandQueue();

	// update viewport and scissor rectangle
	m_sFields.sScreenVp.TopLeftX = 0;
	m_sFields.sScreenVp.TopLeftY = 0;
	m_sFields.sScreenVp.Width = static_cast<float>(m_sClientSize.nW);
	m_sFields.sScreenVp.Height = static_cast<float>(m_sClientSize.nH);
	m_sFields.sScreenVp.MinDepth = 0.0f;
	m_sFields.sScreenVp.MaxDepth = 1.0f;
	m_sFields.sScissorRc = { 0, 0, (long)m_sClientSize.nW, (long)m_sClientSize.nH };

	return APP_FORWARD;
}

signed App_D3D12::UpdateConstants(const AppData& gt)
{
	// meanwhile const
	const float fRadius = 8.f;
	const float fTheta = 1.5f * XM_PI;
	const float fPhi = XM_PIDIV4;

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

	// update constant buffer
	ConstantsWVP sConstants = {};
	XMStoreFloat4x4(&sConstants.sWVP, XMMatrixTranspose(worldViewProj));

	BYTE* ptData = nullptr;
	ThrowIfFailed(m_sFields.psBufferUp->Map(0, nullptr, reinterpret_cast<void**>(&ptData)));
	memcpy(&ptData[0], &sConstants, sizeof(ConstantsWVP));
	if (m_sFields.psBufferUp != nullptr) m_sFields.psBufferUp->Unmap(0, nullptr);

	return APP_FORWARD;
}

signed App_D3D12::Draw(const AppData& gt)
{
	// reset
	ThrowIfFailed(m_sFields.psCmdListAlloc->Reset());
	ThrowIfFailed(m_sFields.psCmdList->Reset(m_sFields.psCmdListAlloc.Get(), m_sFields.psPSO.Get()));

	// trans
	const CD3DX12_RESOURCE_BARRIER sResBr0 = CD3DX12_RESOURCE_BARRIER::Transition(m_sFields.apsBufferSC[m_sFields.nBackbufferI].Get(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_sFields.psCmdList->ResourceBarrier(1, &sResBr0);

	// viewport, scissor rect
	m_sFields.psCmdList->RSSetViewports(1, &m_sFields.sScreenVp);
	m_sFields.psCmdList->RSSetScissorRects(1, &m_sFields.sScissorRc);

	// clear
	const float afColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	m_sFields.psCmdList->ClearRenderTargetView(CD3DX12_CPU_DESCRIPTOR_HANDLE(
		m_sFields.psHeapRtv->GetCPUDescriptorHandleForHeapStart(),
		m_sFields.nBackbufferI,
		m_sFields.uRtvDcSz), afColor, 0, nullptr);
	m_sFields.psCmdList->ClearDepthStencilView(m_sFields.psHeapDsv->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// set render targets
	CD3DX12_CPU_DESCRIPTOR_HANDLE sRtvHandle(m_sFields.psHeapRtv->GetCPUDescriptorHandleForHeapStart(),
		m_sFields.nBackbufferI,
		m_sFields.uRtvDcSz);
	D3D12_CPU_DESCRIPTOR_HANDLE sDsvHandle = m_sFields.psHeapDsv->GetCPUDescriptorHandleForHeapStart();
	m_sFields.psCmdList->OMSetRenderTargets(1, &sRtvHandle, true, &sDsvHandle);

	// descriptor heaps, root signature
	ID3D12DescriptorHeap* apsDHeaps[] = { m_sFields.psCbvHeap.Get() };
	m_sFields.psCmdList->SetDescriptorHeaps(_countof(apsDHeaps), apsDHeaps);
	m_sFields.psCmdList->SetGraphicsRootSignature(m_sFields.psRootSign.Get());

	// vertex, index buffer - topology,... and draw
	D3D12_VERTEX_BUFFER_VIEW sVBV = m_sFields.pcMeshBox->ViewV();
	D3D12_INDEX_BUFFER_VIEW sIBV = m_sFields.pcMeshBox->ViewI();
	m_sFields.psCmdList->IASetVertexBuffers(0, 1, &sVBV);
	m_sFields.psCmdList->IASetIndexBuffer(&sIBV);
	m_sFields.psCmdList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_sFields.psCmdList->SetGraphicsRootDescriptorTable(0, m_sFields.psCbvHeap->GetGPUDescriptorHandleForHeapStart());
	m_sFields.psCmdList->DrawIndexedInstanced(m_sFields.pcMeshBox->m_uIdcN, 1, 0, 0, 0);

	// transit
	const CD3DX12_RESOURCE_BARRIER sResBr1 = CD3DX12_RESOURCE_BARRIER::Transition(m_sFields.apsBufferSC[m_sFields.nBackbufferI].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	m_sFields.psCmdList->ResourceBarrier(1, &sResBr1);

	// close list and execute
	ThrowIfFailed(m_sFields.psCmdList->Close());
	ID3D12CommandList* cmdsLists[] = { m_sFields.psCmdList.Get() };
	m_sFields.psCmdQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// present and swap
	ThrowIfFailed(m_sFields.psSwapchain->Present(0, 0));
	m_sFields.nBackbufferI = (m_sFields.nBackbufferI + 1) % nSwapchainBufferN;

	// wait
	FlushCommandQueue();

	return APP_FORWARD;
}

signed App_D3D12::BuildDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC sCbvHeapDesc = {};
	sCbvHeapDesc.NumDescriptors = 1;
	sCbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	sCbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	sCbvHeapDesc.NodeMask = 0;
	ThrowIfFailed(m_sFields.pcDevice->CreateDescriptorHeap(&sCbvHeapDesc,
		IID_PPV_ARGS(&m_sFields.psCbvHeap)));

	return APP_FORWARD;
}

signed App_D3D12::BuildConstantBuffers()
{
	const CD3DX12_HEAP_PROPERTIES sPrpsU(D3D12_HEAP_TYPE_UPLOAD);
	const CD3DX12_RESOURCE_DESC sDesc = CD3DX12_RESOURCE_DESC::Buffer(Align8Bit(sizeof(ConstantsWVP)) * 1);
	
	// create buffer
	ThrowIfFailed(m_sFields.pcDevice->CreateCommittedResource(
		&sPrpsU,
		D3D12_HEAP_FLAG_NONE,
		&sDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_sFields.psBufferUp)));

	// get address and align
	D3D12_GPU_VIRTUAL_ADDRESS uAddress = m_sFields.psBufferUp->GetGPUVirtualAddress();
	int uCBIx = 0;
	uAddress += uCBIx * Align8Bit(sizeof(ConstantsWVP));

	// create view
	D3D12_CONSTANT_BUFFER_VIEW_DESC sCbvDesc;
	sCbvDesc.BufferLocation = uAddress;
	sCbvDesc.SizeInBytes = Align8Bit(sizeof(ConstantsWVP));
	m_sFields.pcDevice->CreateConstantBufferView(&sCbvDesc, m_sFields.psCbvHeap->GetCPUDescriptorHandleForHeapStart());

	return APP_FORWARD;
}

signed App_D3D12::BuildRootSignature()
{
	CD3DX12_ROOT_PARAMETER sSlotRootPrm[1];
	CD3DX12_DESCRIPTOR_RANGE cbvTable;
	cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	sSlotRootPrm[0].InitAsDescriptorTable(1, &cbvTable);
	CD3DX12_ROOT_SIGNATURE_DESC sRootSigDc(1, sSlotRootPrm, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// serialize
	ComPtr<ID3DBlob> sRootSig = nullptr;
	ComPtr<ID3DBlob> sErr = nullptr;
	HRESULT nHr = D3D12SerializeRootSignature(&sRootSigDc, D3D_ROOT_SIGNATURE_VERSION_1, sRootSig.GetAddressOf(), sErr.GetAddressOf());

	if (sErr != nullptr) { ::OutputDebugStringA((char*)sErr->GetBufferPointer()); }
	ThrowIfFailed(nHr);

	// and create
	ThrowIfFailed(m_sFields.pcDevice->CreateRootSignature(
		0,
		sRootSig->GetBufferPointer(),
		sRootSig->GetBufferSize(),
		IID_PPV_ARGS(&m_sFields.psRootSign)));

	return APP_FORWARD;
}

signed App_D3D12::BuildShaders()
{
	HRESULT hr = S_OK;
	ComPtr<ID3DBlob> sMvsByteCode = nullptr, sMpsByteCode = nullptr;
	std::vector<D3D12_INPUT_ELEMENT_DESC> asLayout;

	D3D_SHADER_MACRO sMacro = {};
	ThrowIfFailed(CompileFromFile(L"Shaders\\color.hlsl", sMacro, "VS", "vs_5_0", sMvsByteCode));
	ThrowIfFailed(CompileFromFile(L"Shaders\\color.hlsl", sMacro, "PS", "ps_5_0", sMpsByteCode));

	asLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC sPsoDc = {};
	sPsoDc.InputLayout = { asLayout.data(), (UINT)asLayout.size() };
	sPsoDc.pRootSignature = m_sFields.psRootSign.Get();
	sPsoDc.VS =
	{
		reinterpret_cast<BYTE*>(sMvsByteCode->GetBufferPointer()),
		sMvsByteCode->GetBufferSize()
	};
	sPsoDc.PS =
	{
		reinterpret_cast<BYTE*>(sMpsByteCode->GetBufferPointer()),
		sMpsByteCode->GetBufferSize()
	};
	sPsoDc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	sPsoDc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	sPsoDc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	sPsoDc.SampleMask = UINT_MAX;
	sPsoDc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	sPsoDc.NumRenderTargets = 1;
	sPsoDc.RTVFormats[0] = m_sFields.eBackbufferFmt;
	sPsoDc.SampleDesc.Count = m_sFields.b4xMsaaState ? 4 : 1;
	sPsoDc.SampleDesc.Quality = m_sFields.b4xMsaaState ? (m_sFields.u4xMsaaQuality - 1) : 0;
	sPsoDc.DSVFormat = m_sFields.eDepthstencilFmt;
	ThrowIfFailed(m_sFields.pcDevice->CreateGraphicsPipelineState(&sPsoDc, IID_PPV_ARGS(&m_sFields.psPSO)));

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

	m_sFields.pcMeshBox = std::make_unique<Mesh<VertexPosCol>>(m_sFields.pcDevice.Get(), m_sFields.psCmdList.Get(), asVertices, auIndices, "box");

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
