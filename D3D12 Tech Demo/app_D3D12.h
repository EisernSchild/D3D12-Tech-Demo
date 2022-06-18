// D3D12 Tech Demo
// (c) 2022 by Denis Reischl
// 
// SPDX-License-Identifier: MIT

#pragma once
#include "app.h"
#include "mesh.h"
#include "pso.h"

#ifndef _APP_D3D12_GENERIC
#define _APP_D3D12_GENERIC

#ifdef _WIN32


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
	/// <summary>Create the main descriptor heaps</summary>
	static signed CreateMainDHeaps();
	/// <summary>Create depth stencil, viewport</summary>
	static signed OnResize();
	/// <summary>Update Constants helper</summary>
	static signed UpdateConstants(const AppData& sData);
	/// <summary>First draw test function</summary>
	static signed Draw(const AppData& sData);
	/// <summary>Compute shader post processing</summary>
	static void ExecutePostProcessing(ID3D12GraphicsCommandList* psCmdList,
		ID3D12RootSignature* psRootSign,
		ID3D12PipelineState* psPSO,
		ID3D12Resource* psInput);
	/// <summary>Create the descriptor heaps for the scene</summary>
	static signed CreateSceneDHeaps();
	/// <summary>Create scene constant buffer</summary>
	static signed CreateConstantBuffers();
	/// <summary>Create root signatures for main render pipe and compute shader</summary>
	static signed CreateRootSignatures();
	/// <summary>Create fragment, pixel and compute shaders</summary>
	static signed CreateShaders();
	/// <summary>Create post processing targets</summary>
	static signed CreateTextures();
	/// <summary>Build the geometry of the scene</summary>
	static signed BuildGeometry();
	/// <summary>Double back buffer ( = 2 )</summary>
	static constexpr int nSwapchainBufferN = 2;

	static struct D3D12_Fields
	{
		/// <summary>fence counter or mark</summary>
		UINT64 uFenceCnt = 0;
		/// <summary>current back buffer (index)</summary>
		int nBackbufferI = 0;
		/// <summary>descriptor sizes (for different views)</summary>
		UINT uRtvDcSz = 0, uDsvDcSz = 0, uCbvSrvUavDcSz = 0, uSamplerDcSz = 0;
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
		/// <summary>base hexagon mesh</summary>
		std::unique_ptr<Mesh_PosCol> pcHexMesh = nullptr;
		/// <summary>shaders root signature</summary>
		ComPtr<ID3D12RootSignature> psRootSign = nullptr;
		/// <summary>constant buffer view descriptor heap</summary>
		ComPtr<ID3D12DescriptorHeap> psConstSRV = nullptr;
		/// <summary>sampler state descriptor heap</summary>
		ComPtr<ID3D12DescriptorHeap> psSampler = nullptr;
		/// <summary>shader constants upload buffer</summary>
		ComPtr<ID3D12Resource> psBufferUp = nullptr;
		/// <summary>the pipeline state object</summary>
		std::shared_ptr<D3D12_PSO> psPSO;
		/// <summary>the actual swap chain</summary>
		ComPtr<IDXGISwapChain> psSwapchain;
		/// <summary>D3D12 device</summary>
		ComPtr<ID3D12Device> psDevice;
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
		ComPtr<ID3D12DescriptorHeap> psHeapRTV;
		/// <summary>depth stencil view descriptor heap</summary>
		ComPtr<ID3D12DescriptorHeap> psHeapDSV;
		/// <summary>the pipeline state object (compute shader)</summary>
		ComPtr<ID3D12PipelineState> psPSOCS = nullptr;
		/// <summary>shader (compute) root signature</summary>
		ComPtr<ID3D12RootSignature> psRootSignCS = nullptr;
		/// <summary>Main render target post processing buffers</summary>
		ComPtr<ID3D12Resource> psPostMap0 = nullptr, psPostMap1 = nullptr;
		/// <summary>GPU descriptor handles for post processing</summary>
		std::array<CD3DX12_GPU_DESCRIPTOR_HANDLE, 4> asPostGpuH;
		/// <summary>CPU descriptor handles for post processing</summary>
		std::array<CD3DX12_CPU_DESCRIPTOR_HANDLE, 4> asPostCpuH;
	} m_sD3D;

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

