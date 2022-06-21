// D3D12 Tech Demo
// Copyright © 2022 by Denis Reischl
// 
// SPDX-License-Identifier: MIT

#include "const_3D.h"

#ifdef _WIN64

/// <summary>mesh parent class</summary>
template <typename T>
class Mesh
{
#ifdef GfxLib_D3D12
public:
	Mesh(ID3D12Device* psDevice,
		ID3D12GraphicsCommandList* psCmdList,
		std::string atName = "mesh")
		: m_uStrideV(sizeof(T))
		, m_eFormatI(DXGI_FORMAT_R16_UINT)
		, _atName(atName)
		, m_uSizeV()
		, m_uSizeI()
		, m_uIdcN()
	{}

	/// <summary>provide vertex buffer view</summary>
	D3D12_VERTEX_BUFFER_VIEW ViewV()const { return { m_pcBufferV->GetGPUVirtualAddress(), m_uSizeV, m_uStrideV }; }
	/// <summary>provide index buffer view</summary>
	D3D12_INDEX_BUFFER_VIEW ViewI()const { return { m_pcBufferI->GetGPUVirtualAddress(), m_uSizeI, m_eFormatI }; }
	/// <summary>dispose temporary</summary>
	void DisposeTmp() { m_pcBufferTmpV = nullptr; m_pcBufferTmpI = nullptr; }

protected:
	/// <summary>system memory data for vertex and index buffer</summary>
	ComPtr<ID3DBlob> m_psBlobVB = nullptr, m_psBlobIB = nullptr;
	/// <summary>actual mesh as vertex and index buffer</summary>
	ComPtr<ID3D12Resource> m_pcBufferV = nullptr, m_pcBufferI = nullptr;
	/// <summary>temporary buffers (vertex, index - D3D12_HEAP_TYPE_UPLOAD)</summary>
	ComPtr<ID3D12Resource> m_pcBufferTmpV = nullptr, m_pcBufferTmpI = nullptr;

#endif

public:
	/// <summary>buffer constants</summary>
	UINT Indices_N() { return m_uIdcN; }
	UINT Instances_N() { return m_uInstN; }

protected:
	/// <summary>buffer constants</summary>
	const UINT m_uStrideV;
	/// <summary>buffer constants</summary>
	UINT m_uSizeV, m_uSizeI, m_uIdcN, m_uInstN;
	/// <summary>buffer index format constant</summary>
	const DXGI_FORMAT m_eFormatI;

private:
	/// <summary>name identifier</summary>
	const std::string _atName;
};

/// <summary>simple mesh sample class</summary>
class Mesh_PosCol : public Mesh<VertexPosCol>
{
public:
	Mesh_PosCol(ID3D12Device* psDevice,
		ID3D12GraphicsCommandList* psCmdList,
		std::vector<VertexPosCol> asVtc,
		std::vector<std::uint16_t> auIdc,
		UINT uInstancesN = 1,
		std::string atName = "mesh")
		: Mesh<VertexPosCol>(psDevice, psCmdList, atName)
	{
		m_uSizeV = (UINT)asVtc.size() * sizeof(VertexPosCol);
		m_uSizeI = (UINT)auIdc.size() * sizeof(std::uint16_t);
		m_uIdcN = (UINT)auIdc.size();
		m_uInstN = uInstancesN;

		// create blobs and copy vertex/index data to them
		ThrowIfFailed(D3DCreateBlob(m_uSizeV, &m_psBlobVB));
		CopyMemory(m_psBlobVB->GetBufferPointer(), asVtc.data(), m_uSizeV);
		ThrowIfFailed(D3DCreateBlob(m_uSizeI, &m_psBlobIB));
		CopyMemory(m_psBlobIB->GetBufferPointer(), auIdc.data(), m_uSizeI);

		Create(psDevice, psCmdList, asVtc, auIdc);
	}

private:
	signed Create(ID3D12Device* psDevice,
		ID3D12GraphicsCommandList* psCmdList,
		std::vector<VertexPosCol> asVtc,
		std::vector<std::uint16_t> auIdc)
	{
		// create buffers and upload buffers
		ThrowIfFailed(Create(psDevice, psCmdList, asVtc.data(),
			m_uSizeV, m_pcBufferV, m_pcBufferTmpV));
		ThrowIfFailed(Create(psDevice, psCmdList, auIdc.data(),
			m_uSizeI, m_pcBufferI, m_pcBufferTmpI));

		return APP_FORWARD;
	}

	/// <summary>create buffer and upload buffer, schedule to copy data</summary>
	signed Create(
		ID3D12Device* psDevice,
		ID3D12GraphicsCommandList* psCmdList,
		const void* pvInitData,
		UINT64 uBytesize,
		ComPtr<ID3D12Resource>& pcBuffer,
		ComPtr<ID3D12Resource>& pcBufferTmp)
	{
		const CD3DX12_HEAP_PROPERTIES sPrpsD(D3D12_HEAP_TYPE_DEFAULT), sPrpsU(D3D12_HEAP_TYPE_UPLOAD);
		const CD3DX12_RESOURCE_DESC sDesc = CD3DX12_RESOURCE_DESC::Buffer(uBytesize);

		// create actual
		HRESULT nHr = psDevice->CreateCommittedResource(
			&sPrpsD,
			D3D12_HEAP_FLAG_NONE,
			&sDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(pcBuffer.GetAddressOf()));

		// create temporary (upload)
		nHr |= psDevice->CreateCommittedResource(
			&sPrpsU,
			D3D12_HEAP_FLAG_NONE,
			&sDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(pcBufferTmp.GetAddressOf()));

		if (SUCCEEDED(nHr))
		{
			D3D12_SUBRESOURCE_DATA sSubData = { pvInitData, (LONG_PTR)uBytesize, (LONG_PTR)uBytesize };
			const CD3DX12_RB_TRANSITION sResBr0(pcBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
			const CD3DX12_RB_TRANSITION sResBr1(pcBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);

			// schedule update to command list, keep upload buffers until executed
			psCmdList->ResourceBarrier(1, &sResBr0);
			UpdateSubresources<1>(psCmdList, pcBuffer.Get(), pcBufferTmp.Get(), 0, 0, 1, &sSubData);
			psCmdList->ResourceBarrier(1, &sResBr1);
		}

		return nHr;
	}
};

#endif