// D3D12 Tech Demo
// Copyright © 2022 by Denis Reischl
// 
// SPDX-License-Identifier: MIT

#include "zone_3D.h"
#include <array>

#ifdef _WIN64

/// <summary>
/// Wrapped pipeline state object
/// </summary>
class D3D12_PSO
{
public:
	explicit D3D12_PSO(
		ComPtr<ID3D12Device> psDevice,
		HRESULT& nHr,
		ComPtr<ID3D12RootSignature>& psRootSignature,
		ComPtr<ID3DBlob>& psVS,
		ComPtr<ID3DBlob>& psPS,
		ComPtr<ID3DBlob>& psDS,
		ComPtr<ID3DBlob>& psHS,
		ComPtr<ID3DBlob>& psGS,
		std::vector<D3D12_INPUT_ELEMENT_DESC>& asInputLayout,
		std::array<DXGI_FORMAT, 8> aeRTVFormat = { DXGI_FORMAT_R8G8B8A8_UNORM },
		UINT uNumRenderTargets = 1,
		DXGI_FORMAT eDSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT,
		D3D12_PRIMITIVE_TOPOLOGY_TYPE eTopology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		D3D12_RASTERIZER_DESC sRasterizerDc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
		D3D12_BLEND_DESC sBlendDc = CD3DX12_BLEND_DESC(D3D12_DEFAULT),
		D3D12_DEPTH_STENCIL_DESC sDepthstencilDc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
		UINT uSampleMask = UINT_MAX,
		bool b4xMsaaState = false,
		UINT u4xMsaaQuality = 0
	)
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC sPsoDc = {};
		sPsoDc.InputLayout = { asInputLayout.data(), (UINT)asInputLayout.size() };
		sPsoDc.pRootSignature = psRootSignature.Get();
		if (psVS) sPsoDc.VS = { reinterpret_cast<BYTE*>(psVS->GetBufferPointer()), psVS->GetBufferSize() };
		if (psPS) sPsoDc.PS = { reinterpret_cast<BYTE*>(psPS->GetBufferPointer()), psPS->GetBufferSize() };
		if (psDS) sPsoDc.DS = { reinterpret_cast<BYTE*>(psDS->GetBufferPointer()), psDS->GetBufferSize() };
		if (psHS) sPsoDc.HS = { reinterpret_cast<BYTE*>(psHS->GetBufferPointer()), psHS->GetBufferSize() };
		if (psGS) sPsoDc.GS = { reinterpret_cast<BYTE*>(psGS->GetBufferPointer()), psGS->GetBufferSize() };
		sPsoDc.RasterizerState = sRasterizerDc;
		sPsoDc.BlendState = sBlendDc;
		sPsoDc.DepthStencilState = sDepthstencilDc;
		sPsoDc.SampleMask = uSampleMask;
		sPsoDc.PrimitiveTopologyType = eTopology;
		sPsoDc.NumRenderTargets = uNumRenderTargets;
		unsigned uF(0);
		for (DXGI_FORMAT e : aeRTVFormat)
			sPsoDc.RTVFormats[uF++] = e;
		sPsoDc.SampleDesc.Count = b4xMsaaState ? 4 : 1;
		sPsoDc.SampleDesc.Quality = b4xMsaaState ? (u4xMsaaQuality - 1) : 0;
		sPsoDc.DSVFormat = eDSVFormat;
		nHr = psDevice.Get()->CreateGraphicsPipelineState(&sPsoDc, IID_PPV_ARGS(&m_psPSO));
	}

	/// <summary>provide wrapped pointer</summary>
	ID3D12PipelineState* Get() const
	{
		return m_psPSO.Get();
	}

private:
	/// <summary>the pipeline state object</summary>
	ComPtr<ID3D12PipelineState> m_psPSO = nullptr;
};

#endif // _WIN64