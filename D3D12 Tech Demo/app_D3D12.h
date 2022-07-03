// D3D12 Tech Demo
// Copyright © 2022 by Denis Reischl
// 
// SPDX-License-Identifier: MIT

#pragma once
#include "app.h"
#include "mesh.h"
#include "pso.h"

#ifndef _APP_D3D12_GENERIC
#define _APP_D3D12_GENERIC

#ifdef _WIN64

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
	static signed InitDirect3D(AppData& sData);
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
	/// <summary>Update constants</summary>
	static signed UpdateConstants(const AppData& sData);
	/// <summary>clear render target</summary>
	static void Clear();
	/// <summary>First draw test function</summary>
	static signed Draw(const AppData& sData);
	/// <summary>Compute shader post processing</summary>
	static void ExecutePost(ID3D12GraphicsCommandList* psCmdList,
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
	/// <summary>Create a pipeline state object for DXR</summary>
	static signed CreateDXRStateObject();
	/// <summary>Create the acceleration structures for DXR</summary>
	static signed CreateDXRAcceleration();
	/// <summary>Create the shader tables for DXR</summary>
	static void BuildDXRShaderTables();
	/// <summary>execute raytracing</summary>
	static void DoRaytracing();
	/// <summary>DXR render method</summary>
	static void DrawDXR();
	/// <summary>copy RenderMap0 to render target (back buffer)</summary>
	static void RenderMap2Backbuffer();
	/// <summary>translate hex tiles by compute shader</summary>
	static void OffsetTiles(ID3D12GraphicsCommandList* psCmdList,
		ID3D12RootSignature* psRootSign,
		ID3D12PipelineState* psPSO);
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
		/// <summary>true if DXR is supported by device</summary>
		bool bDXRSupport = false;
		/// <summary>true if DXR is chosen by user</summary>
		bool bDXRMode = false;
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
		/// <summary>shaders root signature</summary>
		ComPtr<ID3D12RootSignature> psDXRRootSign = nullptr;
		/// <summary>constant buffer view descriptor heap</summary>
		ComPtr<ID3D12DescriptorHeap> psHeapSRV = nullptr;
		/// <summary>sampler state descriptor heap</summary>
		ComPtr<ID3D12DescriptorHeap> psSampler = nullptr;
		/// <summary>shader constants upload buffer</summary>
		ComPtr<ID3D12Resource> psBufferUp = nullptr;
		/// <summary>the pipeline state object</summary>
		std::shared_ptr<D3D12_PSO> psPSO;
		/// <summary>the actual swap chain</summary>
		ComPtr<IDXGISwapChain3> psSwapchain;
		/// <summary>D3D12 device</summary>
		ComPtr<ID3D12Device5> psDevice;
		/// <summary>fence synchonization object</summary>
		ComPtr<ID3D12Fence> psFence;
		/// <summary>D3D12 command queue</summary>
		ComPtr<ID3D12CommandQueue> psCmdQueue;
		/// <summary>D3D12 command list allocator</summary>
		ComPtr<ID3D12CommandAllocator> psCmdListAlloc;
		/// <summary>D3D12 command list</summary>
		ComPtr<ID3D12GraphicsCommandList4> psCmdList;
		/// <summary>swap chain buffer array</summary>
		ComPtr<ID3D12Resource> apsBufferSC[nSwapchainBufferN];
		/// <summary>depth stencil buffer</summary>
		ComPtr<ID3D12Resource> psBufferDS;
		/// <summary>render target view descriptor heap</summary>
		ComPtr<ID3D12DescriptorHeap> psHeapRTV;
		/// <summary>depth stencil view descriptor heap</summary>
		ComPtr<ID3D12DescriptorHeap> psHeapDSV;
		/// <summary>the pipeline state object (compute shader post)</summary>
		ComPtr<ID3D12PipelineState> psPsoCsPost = nullptr;
		/// <summary>the pipeline state object (compute shader hex translate)</summary>
		ComPtr<ID3D12PipelineState> psPsoCsHexTrans = nullptr;
		/// <summary>shader (compute) root signature</summary>
		ComPtr<ID3D12RootSignature> psRootSignCS = nullptr;
		/// <summary>Main render targets</summary>
		ComPtr<ID3D12Resource> psRenderMap0 = nullptr, psRenderMap1 = nullptr;
		/// <summary>Buffer containing the terrain (hex) tiles xy position + upload buffer</summary>
		ComPtr<ID3D12Resource> psTileLayout = nullptr, psTileLayoutUp = nullptr;
		/// <summary>all resource view handles (GPU), enumerated in CbvSrvUav_Heap_Idc</summary>
		std::vector<CD3DX12_GPU_DESCRIPTOR_HANDLE> asCbvSrvUavGpuH = std::vector<CD3DX12_GPU_DESCRIPTOR_HANDLE>(uSrvN);
		/// <summary>all resource view handles (CPU), enumerated in CbvSrvUav_Heap_Idc</summary>
		std::vector<CD3DX12_CPU_DESCRIPTOR_HANDLE> asCbvSrvUavCpuH = std::vector<CD3DX12_CPU_DESCRIPTOR_HANDLE>(uSrvN);
		/// <summary>state object for raytracing</summary>
		ComPtr<ID3D12StateObject> psDXRStateObject;
		/// <summary>bottom level acceleration structure</summary>
		ComPtr<ID3D12Resource> psBotAccelStruct;
		/// <summary>top level acceleration structure</summary>
		ComPtr<ID3D12Resource> psTopAccelStruct;
		/// <summary>miss shader table</summary>
		ComPtr<ID3D12Resource> psMissTable;
		/// <summary>hit group shader table</summary>
		ComPtr<ID3D12Resource> psHitGroupTable;
		/// <summary>ray gen shader table</summary>
		ComPtr<ID3D12Resource> psRayGenTable;
		/// <summary>intersection shader table</summary>
		ComPtr<ID3D12Resource> psIntersectionTable;
		/// <summary>axis-aligned bounding box</summary>
		ComPtr<ID3D12Resource> psAABB;
	} m_sD3D;

	static struct SceneData 
	{
		/// <summary>camera position</summary>
		XMFLOAT3 sCamPos = XMFLOAT3(0.f, 10.f, 0.f);
		/// <summary>camera velocity</summary>
		XMFLOAT3 sCamVelo = {};
		/// <summary>hexagonal coordinates (camera)</summary>
		XMFLOAT2 sHexUV = {};
		/// <summary>hexagonal coordinates next tile center (camera)</summary>
		XMFLOAT2 sHexUVc = {};
		/// <summary>hexagonal coordinates next tile center cardesian (camera)</summary>
		XMFLOAT2 sHexXYc = {};
		/// <summary>constant up vector</summary>
		const XMVECTOR sUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		/// <summary>acceleration for translation and rotation</summary>
		const float fAccelTran = .1f, fAccelRot = 4.f;
		/// <summary>resistance to velocity</summary>
		const float fDrag = .995f;
		/// <summary>camera pitch, yaw</summary>
		float fYaw = 0.f, fPitch = 0.f;
		/// <summary>number of hex ambits (or "circles") around the main hexagon</summary>
		const unsigned uAmbitN = 48;
		/// <summary>number of hex tiles (or instances), to be computed</summary>
		unsigned uInstN = 1;
		/// <summary>number of vertices of one hex tile, to be computed</summary>
		unsigned uBaseVtxN = 0;
		/// <summary>hex tiles positions (current)</summary>
		std::vector<XMFLOAT4> aafTilePos;
		/// <summary>hex tiles positions (to be updated)</summary>
		std::vector<XMFLOAT4> aafTilePosUpdate;
		/// <summary>constant hex tile size</summary>
		const float fTileSz = 1.f;
		/// <summary>constant hex tile minimum width</summary>
		const float fMinW = sqrt(1.f - (.5f * .5f)) * 2.f;
		/// <summary>Size of Vector4</summary>
		const unsigned uVec4Sz = sizeof(float) * 4;
		/// <summary>constants upload struture</summary>
		ConstantsScene sConstants = {};
	} m_sScene;

private:

	/// <summary>Shader resource view heap continous enumeration</summary>
	enum struct CbvSrvUav_Heap_Idc : unsigned
	{
		SceneConstants = 0,
		PostMap0Srv = 1,
		PostMap0Uav = 2,
		PostMap1Srv = 3,
		PostMap1Uav = 4,
		TileOffsetSrv = 5,
		MeshVtcUav = 6
	};
	static constexpr unsigned uSrvN = 7;

	/// <summary>upload hex tiles xy vector offsets to constant buffer</summary>
	static void UpdateHexOffsets(D3D12_RESOURCE_STATES eState)
	{
		// update the constant buffer for the tiles offsets
		D3D12_SUBRESOURCE_DATA sSubData = { m_sScene.aafTilePosUpdate.data(), (LONG_PTR)(m_sScene.aafTilePosUpdate.size() * m_sScene.uVec4Sz), (LONG_PTR)(m_sScene.aafTilePosUpdate.size() * m_sScene.uVec4Sz) };
		const CD3DX12_RB_TRANSITION sResBr0(m_sD3D.psTileLayout.Get(), eState, D3D12_RESOURCE_STATE_COPY_DEST);
		const CD3DX12_RB_TRANSITION sResBr1(m_sD3D.psTileLayout.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);

		// schedule update to command list
		if (eState != D3D12_RESOURCE_STATE_COPY_DEST)
			m_sD3D.psCmdList->ResourceBarrier(1, &sResBr0);
		UpdateSubresources<1>(m_sD3D.psCmdList.Get(), m_sD3D.psTileLayout.Get(), m_sD3D.psTileLayoutUp.Get(), 0, 0, 1, &sSubData);
		m_sD3D.psCmdList->ResourceBarrier(1, &sResBr1);
	}
};

#else
#error "OS not supported!"
#endif

#endif // _APP_D3D12_GENERIC

