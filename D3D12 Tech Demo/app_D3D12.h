// D3D12 Tech Demo
// (c) 2022 by Denis Reischl
// 
// SPDX-License-Identifier: MIT

#pragma once
#include "app.h"

#ifndef _APP_D3D12_GENERIC
#define _APP_D3D12_GENERIC

#ifdef _WIN32
#define APP_GfxLib App_D3D12
#include <windows.h>
#include <wrl.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>
#include "d3dx12.h"
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
using Microsoft::WRL::ComPtr;
using namespace DirectX;

/// <summary>Simple Sample vertex</summary>
struct VertexPosCol
{
	XMFLOAT3 sPos;
	XMFLOAT4 sColor;
};

/// <summary>Simple Sample constants</summary>
struct ConstantsWVP
{
	XMFLOAT4X4 sWVP = XMFLOAT4X4(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);
};

/// <summary>round up to nearest multiple of 256</summary>
inline unsigned Align8Bit(unsigned uSize) { return (uSize + 255) & ~255; }

/// <summary>simple d3d12 mesh class</summary>
template <typename T>
class Mesh
{
public:
	Mesh(ID3D12Device* pcDevice,
		ID3D12GraphicsCommandList* pcCmdList,
		std::vector<T> asVtc,
		std::vector<std::uint16_t> auIdc,
		std::string atName = "mesh")
		: m_uSizeV((UINT)asVtc.size() * sizeof(T))
		, m_uSizeI((UINT)auIdc.size() * sizeof(std::uint16_t))
		, m_uStrideV(sizeof(T))
		, m_eFormatI(DXGI_FORMAT_R16_UINT)
		, m_uIdcN((UINT)auIdc.size())
		, _atName(atName)
	{
		// create blobs and copy vertex/index data to them
		ThrowIfFailed(D3DCreateBlob(m_uSizeV, &m_psBlobVB));
		CopyMemory(m_psBlobVB->GetBufferPointer(), asVtc.data(), m_uSizeV);
		ThrowIfFailed(D3DCreateBlob(m_uSizeI, &m_psBlobIB));
		CopyMemory(m_psBlobIB->GetBufferPointer(), auIdc.data(), m_uSizeI);

		// create buffers and upload buffers
		ThrowIfFailed(Create(pcDevice, pcCmdList, asVtc.data(),
			m_uSizeV, m_pcBufferV, m_pcBufferTmpV));
		ThrowIfFailed(Create(pcDevice, pcCmdList, auIdc.data(),
			m_uSizeI, m_pcBufferI, m_pcBufferTmpI));
	}

	/// <summary>system memory data for vertex and index buffer</summary>
	ComPtr<ID3DBlob> m_psBlobVB = nullptr, m_psBlobIB = nullptr;
	/// <summary>actual mesh as vertex and index buffer</summary>
	ComPtr<ID3D12Resource> m_pcBufferV = nullptr, m_pcBufferI = nullptr;
	/// <summary>temporary buffers (vertex, index - D3D12_HEAP_TYPE_UPLOAD)</summary>
	ComPtr<ID3D12Resource> m_pcBufferTmpV = nullptr, m_pcBufferTmpI = nullptr;
	/// <summary>buffer constants</summary>
	const UINT m_uStrideV, m_uSizeV, m_uSizeI, m_uIdcN;
	/// <summary>buffer index format constant</summary>
	const DXGI_FORMAT m_eFormatI;

	/// <summary>provide vertex buffer view</summary>
	D3D12_VERTEX_BUFFER_VIEW ViewV()const { return { m_pcBufferV->GetGPUVirtualAddress(), m_uSizeV, m_uStrideV }; }
	/// <summary>provide index buffer view</summary>
	D3D12_INDEX_BUFFER_VIEW ViewI()const { return { m_pcBufferI->GetGPUVirtualAddress(), m_uSizeI, m_eFormatI }; }
	/// <summary>dispose temporary</summary>
	void DisposeTmp() { m_pcBufferTmpV = nullptr; m_pcBufferTmpI = nullptr; }

private:

	/// <summary>name identifier</summary>
	const std::string _atName;

	/// <summary>create buffer and upload buffer, schedule to copy data</summary>
	signed Create(
		ID3D12Device* pcDevice,
		ID3D12GraphicsCommandList* pcCmdList,
		const void* pvInitData,
		UINT64 uBytesize,
		ComPtr<ID3D12Resource>& pcBuffer,
		ComPtr<ID3D12Resource>& pcBufferTmp)
	{
		const CD3DX12_HEAP_PROPERTIES sPrpsD(D3D12_HEAP_TYPE_DEFAULT), sPrpsU(D3D12_HEAP_TYPE_UPLOAD);
		const CD3DX12_RESOURCE_DESC sDesc = CD3DX12_RESOURCE_DESC::Buffer(uBytesize);

		// create actual
		HRESULT nHr = pcDevice->CreateCommittedResource(
			&sPrpsD,
			D3D12_HEAP_FLAG_NONE,
			&sDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(pcBuffer.GetAddressOf()));

		// create temporary (upload)
		nHr |= pcDevice->CreateCommittedResource(
			&sPrpsU,
			D3D12_HEAP_FLAG_NONE,
			&sDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(pcBufferTmp.GetAddressOf()));

		if (SUCCEEDED(nHr))
		{
			D3D12_SUBRESOURCE_DATA sSubData = { pvInitData, (LONG_PTR)uBytesize, (LONG_PTR)uBytesize };
			const CD3DX12_RESOURCE_BARRIER sResBr0 = CD3DX12_RESOURCE_BARRIER::Transition(pcBuffer.Get(),
				D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
			const CD3DX12_RESOURCE_BARRIER sResBr1 = CD3DX12_RESOURCE_BARRIER::Transition(pcBuffer.Get(),
				D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);

			// schedule update to command list, keep upload buffers until executed
			pcCmdList->ResourceBarrier(1, &sResBr0);
			UpdateSubresources<1>(pcCmdList, pcBuffer.Get(), pcBufferTmp.Get(), 0, 0, 1, &sSubData);
			pcCmdList->ResourceBarrier(1, &sResBr1);
		}

		return nHr;
	}
};

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
	/// <summary>Init D3D12</summary>
	FUNC_GAME_TASK(GxInit);
	/// <summary></summary>
	FUNC_GAME_TASK(GxRelease);

protected:
	/// <summary>Init D3D12 device</summary>
	static signed InitDirect3D();
	/// <summary>Init D3D12 command objects</summary>
	static signed CreateCommandObjects();
	/// <summary>Init DXGI swap chain</summary>
	static signed CreateSwapChain(IDXGIFactory4* pcFactory);
	/// <summary>Flush the command queue</summary>
	static signed FlushCommandQueue();
	/// <summary>Create the descriptor heaps</summary>
	static signed CreateDHeaps();
	/// <summary>Create depth stencil, viewport</summary>
	static signed OnResize();
	/// <summary>Update Constants helper</summary>
	static signed UpdateConstants(const AppData& gt);
	/// <summary>First draw test function</summary>
	static signed Draw(const AppData& gt);

	static signed BuildDescriptorHeaps();
	static signed BuildConstantBuffers();
	static signed BuildRootSignature();
	static signed BuildShaders();
	static signed BuildGeometry();

	static constexpr int nSwapchainBufferN = 2;

	static struct D3D12_Fields
	{
		/// <summary>fence counter or mark</summary>
		UINT64 uFenceCnt = 0;
		/// <summary>current back buffer (index)</summary>
		int nBackbufferI = 0;
		/// <summary>descriptor sizes (for different views)</summary>
		UINT uRtvDcSz = 0, uDsvDcSz = 0, uCbvSrvUavDcSz = 0;
		/// <summary>back buffer and depth stencil format</summary>
		DXGI_FORMAT eBackbufferFmt = DXGI_FORMAT_R8G8B8A8_UNORM, eDepthstencilFmt = DXGI_FORMAT_D24_UNORM_S8_UINT;
		/// <summary>true if 4 x MSAA</summary>
		bool b4xMsaaState = false;
		/// <summary>4 x MSAA Quality</summary>
		UINT u4xMsaaQuality = 0;
		/// <summary>the screen viewport</summary>
		D3D12_VIEWPORT sScreenVp;
		/// <summary>the scissor rectangle</summary>
		D3D12_RECT sScissorRc;
		/// <summary>mesh of the sample box</summary>
		std::unique_ptr<Mesh<VertexPosCol>> pcMeshBox = nullptr;
		/// <summary>shaders root signature</summary>
		ComPtr<ID3D12RootSignature> psRootSign = nullptr;
		/// <summary>constant buffer view descriptor heap</summary>
		ComPtr<ID3D12DescriptorHeap> psCbvHeap = nullptr;
		/// <summary>shader constants upload buffer</summary>
		ComPtr<ID3D12Resource> psBufferUp = nullptr;
		/// <summary>the pipeline state object</summary>
		ComPtr<ID3D12PipelineState> psPSO = nullptr;
		/// <summary>the actual swap chain</summary>
		ComPtr<IDXGISwapChain> psSwapchain;
		/// <summary>D3D12 device</summary>
		ComPtr<ID3D12Device> pcDevice;
		/// <summary>fence synchonization object</summary>
		ComPtr<ID3D12Fence> psFence;
		/// <summary>D3D12 command queue</summary>
		ComPtr<ID3D12CommandQueue> psCmdQueue;
		/// <summary>D3D12 command list allocator</summary>
		ComPtr<ID3D12CommandAllocator> psCmdListAlloc;
		/// <summary>D3D12 command list</summary>
		ComPtr<ID3D12GraphicsCommandList> psCmdList;
		/// <summary>swap chain buffer array</summary>
		ComPtr<ID3D12Resource> apsBufferSC[nSwapchainBufferN];
		/// <summary>depth stencil buffer</summary>
		ComPtr<ID3D12Resource> psBufferDS;
		/// <summary>render target view descriptor heap</summary>
		ComPtr<ID3D12DescriptorHeap> psHeapRtv;
		/// <summary>depth stencil view descriptor heap</summary>
		ComPtr<ID3D12DescriptorHeap> psHeapDsv;
	} m_sFields;

private:

	/// <summary>D3DCompileFromFile wrapper</summary>
	static signed CompileFromFile(
		const std::wstring& atFilename,
		const D3D_SHADER_MACRO& sDefines,
		const std::string& atEntrypoint,
		const std::string& atTarget,
		ComPtr<ID3DBlob>& pcDest);
};

#else
#error "OS not supported!"
#endif

#endif // _APP_D3D12_GENERIC

